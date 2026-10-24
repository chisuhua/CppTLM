// src/bundles/pcie_tlp_wire_bundle.cc
// P1: PcieTlpWireBundle implementations
// Per Oracle bg_ae8d4f14: to_descriptor/from_descriptor retained with
// [[deprecated]] attribute; byte-level methods bridge to PcieTlpCodec.
//
// This file holds method bodies that were inline in pcie_bundles_tlm.hh
// (lines 187-258 pre-migration). Including the old header here gives
// access to the full bundles::PcieTlpBundle definition for construction.
#include "bundles/pcie_tlp_wire_bundle.hh"
#include "bundles/pcie_bundles_tlm.hh" // for full PcieTlpBundle definition

#include "tlm/pcie/pcie_tlp_codec.hh" // P1 Task 7 bridge (stub until Task 7)

#include <cstring>

namespace cpptlm::pcie {

    bundles::PcieTlpBundle PcieTlpWireBundle::to_descriptor() const {
        uint8_t desc_kind = kind;
        if (desc_kind == CPLD)
            desc_kind = bundles::PcieTlpBundle::MMIO_READ;

        uint32_t length_dw = (length == 0) ? 1024u : static_cast<uint32_t>(length);
        uint32_t size_bytes = length_dw * 4;

        uint64_t data_val = (static_cast<uint64_t>(payload[1]) << 32) | payload[0];

        return bundles::PcieTlpBundle(desc_kind,
                                      0, // bar_index (wire bundle doesn't carry)
                                      address_lo, size_bytes, data_val, requester_id,
                                      static_cast<uint32_t>(tag));
    }

    PcieTlpWireBundle PcieTlpWireBundle::from_descriptor(const bundles::PcieTlpBundle& desc) {
        PcieTlpWireBundle wb;
        uint8_t dk = static_cast<uint8_t>(desc.kind.read());
        wb.kind = dk;

        // NOTE: Lossy fmt/type semantics per Oracle review (per pending fix in
        // follow-up change cpptlm-tlp-descriptor-fmt-fix).
        wb.fmt = (dk == bundles::PcieTlpBundle::CFG_READ || dk == bundles::PcieTlpBundle::CFG_WRITE)
                     ? 0b100
                     : 0b010;
        wb.type = 0b00000;
        wb.tc = 0;
        wb.td = 0;
        wb.ep = 0;
        wb.attr = 0;

        uint32_t size_bytes = static_cast<uint32_t>(desc.size.read());
        uint32_t length_dw = (size_bytes + 3) / 4;
        wb.length = (length_dw >= 1024) ? 0 : static_cast<uint16_t>(length_dw);

        wb.requester_id = static_cast<uint16_t>(desc.requester_id.read());
        wb.tag = static_cast<uint8_t>(desc.trans_id.read() & 0xFF);
        wb.last_be = 0;
        wb.first_be = 0xF;
        wb.address_lo = static_cast<uint32_t>(desc.offset.read() & 0xFFFFFFFF);
        wb.address_hi = 0;

        uint64_t data_val = desc.data.read();
        wb.payload[0] = static_cast<uint32_t>(data_val & 0xFFFFFFFF);
        wb.payload[1] = static_cast<uint32_t>((data_val >> 32) & 0xFFFFFFFF);

        return wb;
    }

    // P1 Task 7: PcieTlpWireBundle ↔ PcieTlpCodec bridge.
    // Per Oracle bg_ae8d4f14 — REUSE PcieTlpCodec (no parallel implementation).
    // Supported kinds: MEM_READ, MEM_WRITE, CPLD. CFG_*/MMIO_*/IRQ_DELIVERY
    // unsupported by current PcieTlpCodec; pack returns 0 length, unpack leaves
    // kind unchanged. Follow-up change 'cpptlm-tlp-descriptor-fmt-fix' will
    // extend PcieTlpCodec with CFG/MMIO/IRQ encoders (per Task 6 Step 7 follow-up).

    void PcieTlpWireBundle::tlp_pack_to_bytes(uint8_t out[4096], unsigned& out_len) const noexcept {
        std::vector<uint8_t> encoded;
        switch (kind) {
        case MEM_READ:
            encoded = PcieTlpCodec::encode_mrd(requester_id, tag, address_lo, length);
            break;
        case MEM_WRITE: {
            const size_t payload_dw = (length == 0) ? 1024 : length;
            const size_t payload_bytes =
                std::min(payload_dw * 4, sizeof(uint32_t) * MAX_PAYLOAD_DW);
            std::vector<uint8_t> data(payload_bytes);
            std::memcpy(data.data(), payload.data(), payload_bytes);
            encoded =
                PcieTlpCodec::encode_mwr(requester_id, tag, address_lo, data.data(), data.size());
            break;
        }
        case CPLD: {
            const size_t payload_dw = (length == 0) ? 1024 : length;
            const size_t payload_bytes =
                std::min(payload_dw * 4, sizeof(uint32_t) * MAX_PAYLOAD_DW);
            std::vector<uint8_t> data(payload_bytes);
            std::memcpy(data.data(), payload.data(), payload_bytes);
            encoded = PcieTlpCodec::encode_cpld(completer_id, requester_id, tag, byte_count,
                                                data.data(), data.size());
            break;
        }
        default:
            // CFG_READ/WRITE, MMIO_READ/WRITE, IRQ_DELIVERY: not supported by
            // PcieTlpCodec yet. Caller must use to_descriptor/from_descriptor
            // (deprecated) for these kinds, or extend PcieTlpCodec.
            out_len = 0;
            return;
        }
        out_len = static_cast<unsigned>(std::min(encoded.size(), static_cast<size_t>(4096)));
        std::memcpy(out, encoded.data(), out_len);
    }

    void PcieTlpWireBundle::tlp_unpack_from_bytes(const uint8_t in[4096],
                                                  unsigned in_len) noexcept {
        auto result = PcieTlpCodec::decode(in, in_len);
        if (!result.has_value()) {
            return; // malformed TLP: leave fields unchanged
        }
        const auto& dec = result.value();

        fmt = dec.fmt;
        type = dec.type;
        tc = dec.tc;
        td = dec.td;
        ep = dec.ep;
        attr = dec.attr;
        length = dec.length;
        requester_id = dec.requester_id;
        tag = dec.tag;
        address_lo = dec.address;
        address_hi = 0;
        completer_id = dec.completer_id;
        byte_count = dec.byte_count;
        status = dec.status;
        lcrc = dec.lcrc;

        // Copy payload bytes → uint32_t payload[] (LE, max 1024 DWs)
        const size_t copy_bytes = std::min(dec.payload.size(), sizeof(uint32_t) * MAX_PAYLOAD_DW);
        const size_t copy_dw = copy_bytes / 4;
        for (size_t i = 0; i < copy_dw; ++i) {
            const uint8_t* b = &dec.payload[i * 4];
            payload[i] = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
                         (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
        }

        // Classify TLP kind via PcieTlpCodec, map to wire bundle enum
        using CK = PcieTlpCodec::TlpKind;
        switch (PcieTlpCodec::classify(dec)) {
        case CK::MRd:
            kind = MEM_READ;
            break;
        case CK::MWr:
            kind = MEM_WRITE;
            break;
        case CK::CplD:
            kind = CPLD;
            break;
        case CK::CfgRd0:
        case CK::CfgRd1:
            kind = CFG_READ;
            break;
        case CK::CfgWr0:
        case CK::CfgWr1:
            kind = CFG_WRITE;
            break;
        case CK::Cpl:
        case CK::Msg:
        case CK::MsgD:
        case CK::Unknown:
        default:
            kind = MMIO_READ;
            break;
        }
    }

    uint32_t PcieTlpWireBundle::crc32_compute() const noexcept {
        // Bridge: pack → compute CRC32 on wire bytes (single source of truth)
        uint8_t wire_bytes[4096];
        unsigned wire_len = 0;
        tlp_pack_to_bytes(wire_bytes, wire_len);
        if (wire_len == 0)
            return 0; // unsupported kind: no wire bytes to CRC
        return PcieTlpCodec::crc32_pcie(wire_bytes, wire_len);
    }

} // namespace cpptlm::pcie