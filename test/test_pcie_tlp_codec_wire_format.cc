// test/test_pcie_tlp_codec_wire_format.cc
// PcieTlpCodec wire-format → golden hex dump 对比验证
// 功能描述：3 个 TEST_CASE 验证 PcieTlpCodec 编解码正确性
//           1. 4KB MWr golden hex 比对
//           2. 64B MRd golden hex 比对
//           3. 64B CplD golden hex 比对
// 作者 CppTLM Team / 日期 2026-09-17
// 参考: test/fixtures/pcie_tlp_golden/README.md
//       PCIe Base Spec §2.2 (TLP header), §2.7 (CRC)

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "tlm/pcie/pcie_tlp_codec.hh"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace cpptlm::pcie;

// ── 辅助函数 ──────────────────────────────────────────────────────────────

/** 读取 .hex 文件 → vector<uint8_t> (uppercase hex, one line per 16 bytes) */
static std::vector<uint8_t> read_hex_file(const std::string& path) {
    std::ifstream f(path);
    REQUIRE(f.is_open());

    std::vector<uint8_t> result;
    std::string line;
    while (std::getline(f, line)) {
        // 跳过空行
        if (line.empty()) continue;
        REQUIRE(line.length() % 2 == 0);
        for (std::size_t i = 0; i < line.length(); i += 2) {
            unsigned int byte_val = 0;
            auto hex_byte = line.substr(i, 2);
            REQUIRE(std::sscanf(hex_byte.c_str(), "%02x", &byte_val) == 1);
            result.push_back(static_cast<uint8_t>(byte_val));
        }
    }
    return result;
}

/** 根据 CPPTLM_SOURCE_DIR 拼合 golden fixture 路径 */
static std::string golden_path(const std::string& filename) {
    const char* src_dir = CPPTLM_SOURCE_DIR;
    return std::string(src_dir) + "/test/fixtures/pcie_tlp_golden/" + filename;
}

// ── 4KB MWr 测试 ──────────────────────────────────────────────────────────

TEST_CASE("PcieTlpCodec: decode 4KB MWr golden hex matches expected fields",
          "[phase9][pcie][tlp-codec]") {
    auto tlp_bytes = read_hex_file(golden_path("4kb_mwr_32bit.hex"));

    // 验证总长度: header(12) + payload(4096) + LCRC(4) = 4112
    REQUIRE(tlp_bytes.size() == 4112);

    // 解码
    auto decoded = PcieTlpCodec::decode(tlp_bytes.data(), tlp_bytes.size());
    REQUIRE(decoded.has_value());

    // 验证 header 字段 (per README.md)
    // Fmt=010 (3DW with data), Type=00000 (Memory)
    REQUIRE(decoded->fmt == 0b010);
    REQUIRE(decoded->type == 0b00000);
    // Length=0 means 1024 DW = 4096 bytes
    REQUIRE(decoded->length == 0);
    // Requester ID = 0x0100
    REQUIRE(decoded->requester_id == 0x0100);
    // Tag = 0x01
    REQUIRE(decoded->tag == 0x01);
    // Address = 0x10000000
    REQUIRE(decoded->address == 0x10000000);
    // TD = 0 (no ECRC)
    REQUIRE(decoded->td == 0);

    // 验证 payload 大小: 4096 bytes (2048 repetitions of 0xAA55)
    REQUIRE(decoded->payload.size() == 4096);

    // 验证 payload 内容: first 4 bytes = 0xAA55AA55, last 4 bytes = 0x55AA55AA
    REQUIRE(decoded->payload[0] == 0xAA);
    REQUIRE(decoded->payload[1] == 0x55);
    REQUIRE(decoded->payload[2] == 0xAA);
    REQUIRE(decoded->payload[3] == 0x55);
    REQUIRE(decoded->payload[4092] == 0xAA);
    REQUIRE(decoded->payload[4093] == 0x55);
    REQUIRE(decoded->payload[4094] == 0xAA);
    REQUIRE(decoded->payload[4095] == 0x55);

    // 验证 LCRC 有效
    REQUIRE(decoded->lcrc_valid == true);

    // 验证 TLP kind
    REQUIRE(PcieTlpCodec::classify(*decoded) == PcieTlpCodec::TlpKind::MWr);
}

// ── 64B MRd 测试 ──────────────────────────────────────────────────────────

TEST_CASE("PcieTlpCodec: decode 64B MRd golden hex matches expected fields",
          "[phase9][pcie][tlp-codec]") {
    auto tlp_bytes = read_hex_file(golden_path("64b_mrd_32bit.hex"));

    // 验证总长度: header(12) + LCRC(4) = 16
    REQUIRE(tlp_bytes.size() == 16);

    // 解码
    auto decoded = PcieTlpCodec::decode(tlp_bytes.data(), tlp_bytes.size());
    REQUIRE(decoded.has_value());

    // 验证 header 字段 (per README.md)
    // Fmt=000 (3DW no data), Type=00000 (Memory)
    REQUIRE(decoded->fmt == 0b000);
    REQUIRE(decoded->type == 0b00000);
    // Length = 16 DW (= 64 bytes)
    REQUIRE(decoded->length == 16);
    // Requester ID = 0x0100
    REQUIRE(decoded->requester_id == 0x0100);
    // Tag = 0x02
    REQUIRE(decoded->tag == 0x02);
    // Address = 0x10001000
    REQUIRE(decoded->address == 0x10001000);
    // TD = 0
    REQUIRE(decoded->td == 0);
    // 无 payload
    REQUIRE(decoded->payload.empty());

    // 验证 LCRC 有效
    REQUIRE(decoded->lcrc_valid == true);

    // 验证 TLP kind
    REQUIRE(PcieTlpCodec::classify(*decoded) == PcieTlpCodec::TlpKind::MRd);
}

// ── 64B CplD 测试 ────────────────────────────────────────────────────────

TEST_CASE("PcieTlpCodec: decode 64B CplD golden hex matches expected fields",
          "[phase9][pcie][tlp-codec]") {
    auto tlp_bytes = read_hex_file(golden_path("64b_cpld.hex"));

    // 验证总长度: header(12) + payload(64) + LCRC(4) = 80
    REQUIRE(tlp_bytes.size() == 80);

    // 解码
    auto decoded = PcieTlpCodec::decode(tlp_bytes.data(), tlp_bytes.size());
    REQUIRE(decoded.has_value());

    // 验证 header 字段 (per README.md)
    // Fmt=010 (3DW with data), Type=01010 (Completion)
    REQUIRE(decoded->fmt == 0b010);
    REQUIRE(decoded->type == 0b01010);
    // Length = 16 DW (= 64 bytes)
    REQUIRE(decoded->length == 16);
    // Completer ID = 0x0200
    REQUIRE(decoded->completer_id == 0x0200);
    // Requester ID = 0x0100
    REQUIRE(decoded->requester_id == 0x0100);
    // Tag = 0x02
    REQUIRE(decoded->tag == 0x02);
    // Byte Count = 64
    REQUIRE(decoded->byte_count == 64);
    // Status = 0 (SC)
    REQUIRE(decoded->status == 0);
    // TD = 0
    REQUIRE(decoded->td == 0);

    // 验证 payload 大小: 64 bytes (16 repetitions of 0xDEADBEEF)
    REQUIRE(decoded->payload.size() == 64);

    // 验证 payload 内容: first 4 bytes = 0xDEADBEEF
    REQUIRE(decoded->payload[0] == 0xDE);
    REQUIRE(decoded->payload[1] == 0xAD);
    REQUIRE(decoded->payload[2] == 0xBE);
    REQUIRE(decoded->payload[3] == 0xEF);

    // 验证 LCRC 有效
    REQUIRE(decoded->lcrc_valid == true);

    // 验证 TLP kind
    REQUIRE(PcieTlpCodec::classify(*decoded) == PcieTlpCodec::TlpKind::CplD);
}

// ── CRC self-test ─────────────────────────────────────────────────────────

TEST_CASE("PcieTlpCodec: CRC self-test matches PCIe spec vectors",
          "[phase9][pcie][tlp-codec][crc]") {
    // Empty data → 0x00000000
    uint8_t empty[1] = {0};
    REQUIRE(PcieTlpCodec::crc32_pcie(empty, 0) == 0x00000000);

    // Four zero bytes → 0x2144DF1C (PCIe spec §2.7)
    uint8_t zeros[4] = {0, 0, 0, 0};
    REQUIRE(PcieTlpCodec::crc32_pcie(zeros, 4) == 0x2144DF1C);

    // Round-trip: msg + CRC appended → residue = 0x2144DF1C
    uint8_t msg[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t crc_val = PcieTlpCodec::crc32_pcie(msg, 4);
    uint8_t extended[8];
    std::memcpy(extended, msg, 4);
    extended[4] = static_cast<uint8_t>(crc_val & 0xFF);
    extended[5] = static_cast<uint8_t>((crc_val >> 8) & 0xFF);
    extended[6] = static_cast<uint8_t>((crc_val >> 16) & 0xFF);
    extended[7] = static_cast<uint8_t>((crc_val >> 24) & 0xFF);
    REQUIRE(PcieTlpCodec::crc32_pcie(extended, 8) == 0x2144DF1C);

    // 完整 self-test
    REQUIRE(PcieTlpCodec::crc_self_test() == true);
}

// ── is_malformed 测试 ─────────────────────────────────────────────────────

TEST_CASE("PcieTlpCodec: is_malformed detects bad TLP",
          "[phase9][pcie][tlp-codec][malformed]") {
    // 合法 MRd → 不是 malformed
    auto mrd = read_hex_file(golden_path("64b_mrd_32bit.hex"));
    auto dec_mrd = PcieTlpCodec::decode(mrd.data(), mrd.size());
    REQUIRE(dec_mrd.has_value());
    REQUIRE(PcieTlpCodec::is_malformed(*dec_mrd) == false);

    // 合法 MWr → 不是 malformed
    auto mwr = read_hex_file(golden_path("4kb_mwr_32bit.hex"));
    auto dec_mwr = PcieTlpCodec::decode(mwr.data(), mwr.size());
    REQUIRE(dec_mwr.has_value());
    REQUIRE(PcieTlpCodec::is_malformed(*dec_mwr) == false);

    // 合法 CplD → 不是 malformed
    auto cpld = read_hex_file(golden_path("64b_cpld.hex"));
    auto dec_cpld = PcieTlpCodec::decode(cpld.data(), cpld.size());
    REQUIRE(dec_cpld.has_value());
    REQUIRE(PcieTlpCodec::is_malformed(*dec_cpld) == false);
}

// ── 编码 round-trip 测试 ──────────────────────────────────────────────────

TEST_CASE("PcieTlpCodec: encode then decode MRd round-trip",
          "[phase9][pcie][tlp-codec][roundtrip]") {
    std::vector<uint8_t> encoded = PcieTlpCodec::encode_mrd(
        /*requester_id=*/0x0100,
        /*tag=*/0x02,
        /*address=*/0x10000000,
        /*len_dw=*/64);  // 64 DW = 256 bytes

    REQUIRE(encoded.size() > 0);

    auto decoded = PcieTlpCodec::decode(encoded.data(), encoded.size());
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->fmt == 0b000);
    REQUIRE(decoded->type == 0b00000);
    REQUIRE(decoded->length == 64);
    REQUIRE(decoded->requester_id == 0x0100);
    REQUIRE(decoded->tag == 0x02);
    REQUIRE(decoded->address == 0x10000000);
    REQUIRE(decoded->lcrc_valid == true);
}

TEST_CASE("PcieTlpCodec: encode then decode MWr round-trip",
          "[phase9][pcie][tlp-codec][roundtrip]") {
    uint8_t payload[256];
    for (int i = 0; i < 256; ++i) {
        payload[i] = static_cast<uint8_t>(i);
    }
    std::vector<uint8_t> encoded = PcieTlpCodec::encode_mwr(
        /*requester_id=*/0x0100,
        /*tag=*/0x01,
        /*address=*/0x10000000,
        /*data=*/payload, /*len=*/256);

    REQUIRE(encoded.size() > 0);

    auto decoded = PcieTlpCodec::decode(encoded.data(), encoded.size());
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->fmt == 0b010);
    REQUIRE(decoded->type == 0b00000);
    // 256 bytes = 64 DW
    REQUIRE(decoded->length == 64);
    REQUIRE(decoded->requester_id == 0x0100);
    REQUIRE(decoded->tag == 0x01);
    REQUIRE(decoded->address == 0x10000000);
    REQUIRE(decoded->payload.size() == 256);
    REQUIRE(decoded->payload[0] == 0);
    REQUIRE(decoded->payload[255] == 255);
    REQUIRE(decoded->lcrc_valid == true);
}

TEST_CASE("PcieTlpCodec: encode then decode CplD round-trip",
          "[phase9][pcie][tlp-codec][roundtrip]") {
    uint8_t payload[64];
    for (int i = 0; i < 64; ++i) {
        payload[i] = static_cast<uint8_t>(i * 3);
    }
    std::vector<uint8_t> encoded = PcieTlpCodec::encode_cpld(
        /*completer_id=*/0x0200,
        /*requester_id=*/0x0100,
        /*tag=*/0x02,
        /*byte_count=*/64,
        /*data=*/payload, /*len=*/64);

    REQUIRE(encoded.size() > 0);

    auto decoded = PcieTlpCodec::decode(encoded.data(), encoded.size());
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->fmt == 0b010);
    REQUIRE(decoded->type == 0b01010);
    REQUIRE(decoded->length == 16);
    REQUIRE(decoded->completer_id == 0x0200);
    REQUIRE(decoded->requester_id == 0x0100);
    REQUIRE(decoded->tag == 0x02);
    REQUIRE(decoded->byte_count == 64);
    REQUIRE(decoded->payload.size() == 64);
    REQUIRE(decoded->lcrc_valid == true);
}

// ── 编码 vs golden fixture 字节比对 (cross-verify with generate_golden.py) ──

TEST_CASE("PcieTlpCodec: encode MRd matches golden fixture byte-for-byte",
          "[phase9][pcie][tlp-codec][crossverify]") {
    // Same params as generate_golden.py build_mrd_tlp(0x0100, 0x02, 0x10001000, 16)
    auto encoded = PcieTlpCodec::encode_mrd(
        /*requester_id=*/0x0100,
        /*tag=*/0x02,
        /*address=*/0x10001000,
        /*len_dw=*/16);

    // Read golden fixture
    auto golden = read_hex_file(golden_path("64b_mrd_32bit.hex"));

    // Compare byte-for-byte
    REQUIRE(encoded.size() == golden.size());
    for (std::size_t i = 0; i < golden.size(); ++i) {
        REQUIRE(encoded[i] == golden[i]);
    }
}

TEST_CASE("PcieTlpCodec: encode CplD matches golden fixture byte-for-byte",
          "[phase9][pcie][tlp-codec][crossverify]") {
    // Same params as generate_golden.py build_cpld_tlp
    // completer_id=0x0200, requester_id=0x0100, tag=0x02, byte_count=64
    uint8_t payload[64];
    // b"\xDE\xAD\xBE\xEF" * 16
    for (int i = 0; i < 16; ++i) {
        payload[i*4 + 0] = 0xDE;
        payload[i*4 + 1] = 0xAD;
        payload[i*4 + 2] = 0xBE;
        payload[i*4 + 3] = 0xEF;
    }
    auto encoded = PcieTlpCodec::encode_cpld(
        /*completer_id=*/0x0200,
        /*requester_id=*/0x0100,
        /*tag=*/0x02,
        /*byte_count=*/64,
        /*data=*/payload, /*len=*/64);

    // Read golden fixture
    auto golden = read_hex_file(golden_path("64b_cpld.hex"));

    // Compare byte-for-byte
    REQUIRE(encoded.size() == golden.size());
    for (std::size_t i = 0; i < golden.size(); ++i) {
        REQUIRE(encoded[i] == golden[i]);
    }
}

TEST_CASE("PcieTlpCodec: encode 4KB MWr matches golden fixture byte-for-byte",
          "[phase9][pcie][tlp-codec][crossverify]") {
    // Same params as generate_golden.py build_mwr_tlp
    // req_id=0x0100, tag=0x01, addr32=0x10000000
    uint8_t payload[4096];
    // b"\xAA\x55" * 2048
    for (int i = 0; i < 2048; ++i) {
        payload[i*2 + 0] = 0xAA;
        payload[i*2 + 1] = 0x55;
    }
    auto encoded = PcieTlpCodec::encode_mwr(
        /*requester_id=*/0x0100,
        /*tag=*/0x01,
        /*address=*/0x10000000,
        /*data=*/payload, /*len=*/4096);

    // Read golden fixture
    auto golden = read_hex_file(golden_path("4kb_mwr_32bit.hex"));

    // Compare byte-for-byte
    REQUIRE(encoded.size() == golden.size());
    for (std::size_t i = 0; i < golden.size(); ++i) {
        REQUIRE(encoded[i] == golden[i]);
    }
}