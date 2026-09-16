// src/tlm/pcie/pcie_tlp_codec.cc
// PcieTlpCodec: wire-format TLP 编解码器实现
// 功能描述：PCIe TLP wire-format 编码/解码
//           - CRC-32 (PCIe spec §2.7): poly=0x04C11DB7, reflected as 0xEDB88320
//           - LCRC: cover header+payload+ECRC
//           - ECRC: cover header+payload (TD=1 only)
//           - DLLP CRC-16: poly=0x100B, reflected
//           - TLP 编码: MRd/MWr/CplD (3DW header, TD=0)
//           - TLP 解码: 任意 3DW TLP decode + Malformed 判别
// 作者 CppTLM Team / 日期 2026-09-17
// 参考: PCIe Base Spec §2.2 (TLP header), §2.7 (CRC)
//       test/fixtures/pcie_tlp_golden/generate_golden.py

#include "tlm/pcie/pcie_tlp_codec.hh"

#include <algorithm>
#include <array>
#include <cstring>
#include <tuple>

namespace cpptlm::pcie {

// ===========================================================================
// CRC-32 Lookup Table (reflected polynomial 0xEDB88320)
// Standard CRC-32: poly=0x04C11DB7, reflected on LSB-first serial interface
// ===========================================================================

namespace {

/** Build 256-entry CRC-32 lookup table (reflected) */
static std::array<uint32_t, 256> build_crc32_table() {
    std::array<uint32_t, 256> table{};
    const uint32_t poly = 0xEDB88320;
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ poly;
            } else {
                crc >>= 1;
            }
        }
        table[i] = crc;
    }
    return table;
}

/** Build 256-entry CRC-16 lookup table (reflected, poly=0x100B reflected = 0x8408) */
static std::array<uint16_t, 256> build_crc16_table() {
    std::array<uint16_t, 256> table{};
    const uint16_t poly = 0x8408;  // reflected 0x100B
    for (uint32_t i = 0; i < 256; ++i) {
        uint16_t crc = static_cast<uint16_t>(i);
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = static_cast<uint16_t>((crc >> 1) ^ poly);
            } else {
                crc = static_cast<uint16_t>(crc >> 1);
            }
        }
        table[i] = crc;
    }
    return table;
}

/** Singleton CRC-32 table (lazy-init at first use) */
const std::array<uint32_t, 256>& crc32_table() {
    static const auto table = build_crc32_table();
    return table;
}

/** Singleton CRC-16 table (lazy-init at first use) */
const std::array<uint16_t, 256>& crc16_table() {
    static const auto table = build_crc16_table();
    return table;
}

/** Read a LE uint16 from byte buffer */
inline uint16_t read_le16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

/** Read a LE uint32 from byte buffer */
inline uint32_t read_le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

/** Write a LE uint32 to byte buffer */
inline void write_le32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

} // anonymous namespace

// ===========================================================================
// CRC-32 Implementation (PCIe spec §2.7)
// ===========================================================================

uint32_t PcieTlpCodec::crc32_pcie(const uint8_t* data, std::size_t len) {
    const auto& table = crc32_table();
    uint32_t crc = 0xFFFFFFFF;
    for (std::size_t i = 0; i < len; ++i) {
        uint8_t idx = static_cast<uint8_t>((crc ^ data[i]) & 0xFF);
        crc = table[idx] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

uint32_t PcieTlpCodec::compute_lcrc(const uint8_t* data, std::size_t len) {
    return crc32_pcie(data, len);
}

uint32_t PcieTlpCodec::compute_ecrc(const uint8_t* data, std::size_t len) {
    return crc32_pcie(data, len);
}

// ===========================================================================
// CRC-16 DLLP Implementation
// poly=0x100B, reflected poly=0x8408, init=0xFFFF, reflected
// ===========================================================================

uint16_t PcieTlpCodec::compute_dllp_crc16(const uint8_t* data, std::size_t len) {
    const auto& table = crc16_table();
    uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < len; ++i) {
        uint8_t idx = static_cast<uint8_t>((crc ^ data[i]) & 0xFF);
        crc = static_cast<uint16_t>((table[idx] ^ (crc >> 8)) & 0xFFFF);
    }
    return crc;
}

// ===========================================================================
// TLP Encoder Helpers
// ===========================================================================

/** Build DW0 for Memory Write (3DW, with data) */
static uint32_t mwr_dw0(uint32_t length_dw) {
    // Fmt=010 (3DW with data), Type=00000 (Memory)
    // DW0 = 0x40000000 | (length & 0x3FF)
    uint32_t len10 = (length_dw >= 1024) ? 0 : (length_dw & 0x3FF);
    return 0x40000000u | len10;
}

/** Build DW0 for Memory Read (3DW, no data) */
static uint32_t mrd_dw0(uint32_t length_dw) {
    // Fmt=000 (3DW no data), Type=00000 (Memory)
    uint32_t len10 = (length_dw >= 1024) ? 0 : (length_dw & 0x3FF);
    return len10;
}

/** Build DW0 for Completion with Data (3DW, with data) */
static uint32_t cpld_dw0(uint32_t length_dw) {
    // Fmt=010 (3DW with data), Type=01010 (Completion)
    // DW0 = 0x4A000000 | (length & 0x3FF)
    uint32_t len10 = (length_dw >= 1024) ? 0 : (length_dw & 0x3FF);
    return 0x4A000000u | len10;
}

// ===========================================================================
// TLP Encoders
// ===========================================================================

std::vector<uint8_t> PcieTlpCodec::encode_mwr(uint16_t requester_id, uint8_t tag,
                                                uint32_t address,
                                                const uint8_t* data, std::size_t len) {
    // Payload must be DW-aligned
    std::size_t payload_dw = (len + 3) / 4;
    std::size_t payload_bytes = payload_dw * 4;

    // Header: 3 DW = 12 bytes
    std::vector<uint8_t> tlp;
    tlp.reserve(12 + payload_bytes + 4);

    // DW0: Fmt=010, Type=00000, Length
    uint32_t dw0 = mwr_dw0(static_cast<uint32_t>(payload_dw));
    uint8_t hdr[12];
    write_le32(hdr, dw0);

    // DW1: Requester ID | Tag | Last BE | First BE
    hdr[4] = static_cast<uint8_t>(requester_id & 0xFF);
    hdr[5] = static_cast<uint8_t>((requester_id >> 8) & 0xFF);
    hdr[6] = tag;
    hdr[7] = 0xFF;  // All byte enables

    // DW2: Address (32-bit)
    write_le32(&hdr[8], address);

    tlp.insert(tlp.end(), hdr, hdr + 12);

    // Payload (zero-padded to DW boundary)
    tlp.insert(tlp.end(), data, data + len);
    for (std::size_t i = len; i < payload_bytes; ++i) {
        tlp.push_back(0);
    }

    // LCRC
    uint32_t lcrc = compute_lcrc(tlp.data(), tlp.size());
    uint8_t lcrc_bytes[4];
    write_le32(lcrc_bytes, lcrc);
    tlp.insert(tlp.end(), lcrc_bytes, lcrc_bytes + 4);

    return tlp;
}

std::vector<uint8_t> PcieTlpCodec::encode_mrd(uint16_t requester_id, uint8_t tag,
                                                uint32_t address, std::size_t len_dw) {
    // Header: 3 DW = 12 bytes + LCRC 4 bytes
    std::vector<uint8_t> tlp;
    tlp.reserve(16);

    // DW0: Fmt=000, Type=00000, Length
    uint32_t dw0 = mrd_dw0(static_cast<uint32_t>(len_dw));
    uint8_t hdr[12];
    write_le32(hdr, dw0);

    // DW1: Requester ID | Tag | Last BE | First BE
    hdr[4] = static_cast<uint8_t>(requester_id & 0xFF);
    hdr[5] = static_cast<uint8_t>((requester_id >> 8) & 0xFF);
    hdr[6] = tag;
    hdr[7] = 0xFF;  // All byte enables

    // DW2: Address (32-bit)
    write_le32(&hdr[8], address);

    tlp.insert(tlp.end(), hdr, hdr + 12);

    // LCRC
    uint32_t lcrc = compute_lcrc(tlp.data(), tlp.size());
    uint8_t lcrc_bytes[4];
    write_le32(lcrc_bytes, lcrc);
    tlp.insert(tlp.end(), lcrc_bytes, lcrc_bytes + 4);

    return tlp;
}

std::vector<uint8_t> PcieTlpCodec::encode_cpld(uint16_t completer_id, uint16_t requester_id,
                                                 uint8_t tag, uint16_t byte_count,
                                                 const uint8_t* data, std::size_t len) {
    // Payload must be DW-aligned
    std::size_t payload_dw = (len + 3) / 4;
    std::size_t payload_bytes = payload_dw * 4;

    std::vector<uint8_t> tlp;
    tlp.reserve(12 + payload_bytes + 4);

    // DW0: Fmt=010, Type=01010 (Completion), Length
    uint32_t dw0 = cpld_dw0(static_cast<uint32_t>(payload_dw));
    uint8_t hdr[12];
    write_le32(hdr, dw0);

    // DW1: [Completer ID (16)] [BCM(1)|R(1)|Status(3)|BC_high(3)] [Byte Count low(8)]
    uint16_t bc = byte_count & 0x0FFF;
    uint8_t bc_high = static_cast<uint8_t>((bc >> 8) & 0x07);
    uint8_t bc_low  = static_cast<uint8_t>(bc & 0xFF);
    hdr[4] = static_cast<uint8_t>(completer_id & 0xFF);
    hdr[5] = static_cast<uint8_t>((completer_id >> 8) & 0xFF);
    hdr[6] = (0 << 7) | (0 << 6) | (0 << 3) | bc_high;  // BCM=0,R=0,Status=000
    hdr[7] = bc_low;

    // DW2: [Requester ID (16)] [Tag (8)] [Lower Address (8)]
    hdr[8]  = 0x00;  // Lower Address (aligned)
    hdr[9]  = tag;
    hdr[10] = static_cast<uint8_t>((requester_id >> 8) & 0xFF);  // ReqID high byte
    hdr[11] = static_cast<uint8_t>(requester_id & 0xFF);          // ReqID low byte

    tlp.insert(tlp.end(), hdr, hdr + 12);

    // Payload
    tlp.insert(tlp.end(), data, data + len);
    for (std::size_t i = len; i < payload_bytes; ++i) {
        tlp.push_back(0);
    }

    // LCRC
    uint32_t lcrc = compute_lcrc(tlp.data(), tlp.size());
    uint8_t lcrc_bytes[4];
    write_le32(lcrc_bytes, lcrc);
    tlp.insert(tlp.end(), lcrc_bytes, lcrc_bytes + 4);

    return tlp;
}

// ===========================================================================
// TLP Decoder
// ===========================================================================

std::optional<PcieTlpCodec::DecodedTlp>
PcieTlpCodec::decode(const uint8_t* tlp_bytes, std::size_t len) {
    // Minimum TLP: 3DW header (12 bytes) + LCRC (4 bytes) = 16 bytes
    if (len < 16) {
        return std::nullopt;
    }

    DecodedTlp result;

    // ── Parse Header ──

    // DW0: Fmt, Type, TC, Attr, Length
    uint32_t dw0 = read_le32(tlp_bytes);
    result.fmt    = static_cast<uint8_t>((dw0 >> 29) & 0x7);   // bits 31:29
    result.type   = static_cast<uint8_t>((dw0 >> 24) & 0x1F);  // bits 28:24
    result.tc     = static_cast<uint8_t>((dw0 >> 20) & 0x7);   // bits 22:20
    result.td     = static_cast<uint8_t>((dw0 >> 15) & 0x1);   // bit 15
    result.ep     = static_cast<uint8_t>((dw0 >> 14) & 0x1);   // bit 14
    result.attr   = static_cast<uint8_t>((dw0 >> 12) & 0x3);   // bits 13:12
    result.length = static_cast<uint16_t>(dw0 & 0x3FF);         // bits 9:0

    // Determine header DW count from Fmt
    // Fmt[2:0]: bit 2 = 1 means 4DW header (64-bit address)
    //           bit 1 = 1 means with data
    //           bit 0 = varies
    bool is_4dw = (result.fmt & 0x4) != 0;
    bool has_data = (result.fmt & 0x2) != 0;

    // We only handle 3DW headers (Fmt = 0b000 or 0b010, meaning bit 2 = 0)
    if (is_4dw) {
        // 4DW not yet supported
        return std::nullopt;
    }

    // DW1: varies by TLP type
    uint16_t dw1_lo = read_le16(&tlp_bytes[4]);  // bytes 4-5
    uint8_t byte6 = tlp_bytes[6];
    uint8_t byte7 = tlp_bytes[7];

    // DW2: bytes 8-11
    uint32_t dw2 = read_le32(&tlp_bytes[8]);

    // Classify by Fmt+Type
    // Memory: Type=00000
    // Completion: Type=01010
    // Configuration: Type=00101 (Type0) or Type=00100 (Type1)
    // Message: Type=10010
    uint8_t raw_type_byte = tlp_bytes[3]; // Byte 3 of DW0 = upper byte (MSB)
    // Actually, type is bits 28:24 of DW0, and DW0 is packed LE.
    // tlp_bytes[3] is the MSB (byte 3 in LE).
    // DW0 value (LE): byte[0]=LSB, byte[3]=MSB = bits 31:24
    // Fmt = bits 31:29, Type = bits 28:24
    // So tlp_bytes[3] = (Fmt << 5) | Type
    
    // Memory TLP (Type = 00000): MRd or MWr
    if (result.type == 0x00) {
        // DW1: [Requester ID (16)] [Tag (8)] [Last DW BE (4)|First DW BE (4)]
        result.requester_id = dw1_lo;
        result.tag = byte6;
        // byte7 = Last BE | First BE (ignored for now)

        // DW2: [Address (32)] (for 3DW headers)
        result.address = dw2;

    // Completion (Type = 01010)
    } else if (result.type == 0x0A) {
        // DW1: [Completer ID (16)] [BCM|R|Status|BC_high] [BC_low]
        result.completer_id = dw1_lo;
        // byte6: bit 7=BCM, bit 6=R, bits 5:3=Status, bits 2:0=BC_high
        result.status = static_cast<uint8_t>((byte6 >> 3) & 0x7);
        uint8_t bc_high = byte6 & 0x7;
        result.byte_count = (static_cast<uint16_t>(bc_high) << 8) | byte7;

        // DW2: [Requester ID (16)] [Tag (8)] [Lower Address (8)]
        // bytes 8-9 = Lower Address + Tag (LE)
        result.tag = tlp_bytes[9];   // byte 9 = Tag
        // bytes 10-11 = Requester ID (high byte first in LE of DW2)
        // Actually checking the Python code:
        // byte8=LowerAddr, byte9=Tag, byte10=ReqID[15:8], byte11=ReqID[7:0]
        result.requester_id = (static_cast<uint16_t>(tlp_bytes[10]) << 8) |
                               static_cast<uint16_t>(tlp_bytes[11]);

    // Configuration Type 0 (Type = 00101)
    } else if (result.type == 0x05) {
        // DW1: [Requester ID (16)] [Tag (8)] [reserved]
        result.requester_id = dw1_lo;
        result.tag = byte6;

    // Configuration Type 1 (Type = 00100)
    } else if (result.type == 0x04) {
        result.requester_id = dw1_lo;
        result.tag = byte6;

    // Message (Type = 10010) or Message with Data (Type = 11010)
    } else if (result.type == 0x12 || result.type == 0x1A) {
        result.requester_id = dw1_lo;
        result.tag = byte6;
    }

    // ── Parse Payload ──
    std::size_t header_bytes = 12;
    // Payload exists only for TLPs with data (Fmt bit 1 = 1)
    // For MRd: no payload; length field is read request size, not TLP payload
    std::size_t payload_bytes = 0;
    if (has_data) {
        std::size_t payload_dw = (result.length == 0) ? 1024 : result.length;
        payload_bytes = payload_dw * 4;
    }

    if (len < header_bytes + payload_bytes + 4) {
        // Not enough bytes for TLP header + payload + LCRC
        return std::nullopt;
    }

    // Extract payload (only when has_data)
    if (has_data && payload_bytes > 0) {
        result.payload.assign(tlp_bytes + header_bytes,
                              tlp_bytes + header_bytes + payload_bytes);
    }

    // ── Parse LCRC ──
    // LCRC follows header (for no-data TLPs) or header+payload (for data TLPs)
    std::size_t lcrc_offset = header_bytes + payload_bytes;
    if (len >= lcrc_offset + 4) {
        result.lcrc = read_le32(&tlp_bytes[lcrc_offset]);

        // Verify LCRC (covers header + payload + ECRC; for TD=0, no ECRC)
        uint32_t expected_lcrc = compute_lcrc(tlp_bytes, lcrc_offset);
        result.lcrc_valid = (result.lcrc == expected_lcrc);
    }

    // ECRC validation (only when TD=1 and there's room)
    if (result.td == 1 && len >= lcrc_offset + 8) {
        // ECRC exists between payload and LCRC
        uint32_t ecrc = read_le32(&tlp_bytes[lcrc_offset]);
        uint32_t expected_ecrc = compute_ecrc(tlp_bytes, header_bytes + payload_bytes);
        result.ecrc_valid = (ecrc == expected_ecrc);
    }

    return result;
}

// ===========================================================================
// Malformed Detection
// ===========================================================================

bool PcieTlpCodec::is_malformed(const DecodedTlp& tlp) {
    // 1. Reserved Fmt/Type combinations
    // Fmt[2:0] valid: 000 (3DW no data), 001 (4DW no data),
    //                 010 (3DW with data), 011 (4DW with data)
    if (tlp.fmt > 3) return true;

    // 2. Payload size check: for TLPs with data, payload must match length
    // For no-data TLPs (MRd, CfgRd, Cpl), length field is not payload size
    bool has_data = (tlp.fmt & 0x2) != 0;
    if (has_data) {
        std::size_t declared_dw = (tlp.length == 0) ? 1024 : tlp.length;
        std::size_t payload_dw = tlp.payload.size() / 4;
        if (payload_dw != declared_dw) return true;
    }

    // 3. LCRC mismatch (fatal)
    if (!tlp.lcrc_valid) return true;

    // 4. For TD=1, ECRC must be valid
    if (tlp.td == 1 && !tlp.ecrc_valid) return true;

    // (More checks can be added as needed)
    return false;
}

// ===========================================================================
// TLP Classification
// ===========================================================================

PcieTlpCodec::TlpKind PcieTlpCodec::classify(const DecodedTlp& tlp) {
    // Type=00000: Memory
    if (tlp.type == 0x00) {
        // Fmt bit 1 = has data → MWr; else MRd
        if (tlp.fmt & 0x2) {
            return TlpKind::MWr;
        } else {
            return TlpKind::MRd;
        }
    }

    // Type=01010: Completion
    if (tlp.type == 0x0A) {
        if (tlp.fmt & 0x2) {
            return TlpKind::CplD;
        } else {
            return TlpKind::Cpl;
        }
    }

    // Type=00101: Configuration Type 0
    if (tlp.type == 0x05) {
        if (tlp.fmt & 0x2) {
            return TlpKind::CfgWr0;
        } else {
            return TlpKind::CfgRd0;
        }
    }

    // Type=00100: Configuration Type 1
    if (tlp.type == 0x04) {
        if (tlp.fmt & 0x2) {
            return TlpKind::CfgWr1;
        } else {
            return TlpKind::CfgRd1;
        }
    }

    // Type=10010: Message (no data)
    if (tlp.type == 0x12) {
        return TlpKind::Msg;
    }

    // Type=11010: Message with Data
    if (tlp.type == 0x1A) {
        return TlpKind::MsgD;
    }

    return TlpKind::Unknown;
}

// ===========================================================================
// CRC Self-Test
// ===========================================================================

bool PcieTlpCodec::crc_self_test() {
    // Test vector 1: CRC of empty data = 0
    if (crc32_pcie(nullptr, 0) != 0) return false;

    // Test vector 2: CRC of four zero bytes = 0x2144DF1C (PCIe spec §2.7)
    uint8_t zeros[4] = {0, 0, 0, 0};
    if (crc32_pcie(zeros, 4) != 0x2144DF1C) return false;

    // Round-trip residue check: msg + CRC appended → 0x2144DF1C
    uint8_t msg[4] = {0x01, 0x02, 0x03, 0x04};
    uint32_t crc_val = crc32_pcie(msg, 4);
    uint8_t extended[8];
    std::memcpy(extended, msg, 4);
    write_le32(&extended[4], crc_val);
    if (crc32_pcie(extended, 8) != 0x2144DF1C) return false;

    return true;
}

} // namespace cpptlm::pcie