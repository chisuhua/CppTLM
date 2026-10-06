// test/test_pcie_wire_codec_golden.cc
// P1: PcieTlpWireBundle ↔ PcieTlpCodec bridge golden-vector tests
// Per Oracle bg_ae8d4f14 — REUSE PcieTlpCodec, do not reimplement codec.
#include "bundles/pcie_tlp_wire_bundle.hh"
#include "tlm/pcie/pcie_tlp_codec.hh"

#include <catch_amalgamated.hpp>

#include <cstring>
#include <vector>

using cpptlm::pcie::PcieTlpWireBundle;
using cpptlm::pcie::PcieTlpCodec;

TEST_CASE("tlp_pack_to_bytes: MEM_READ bridge to PcieTlpCodec::encode_mrd",
          "[pcie][wire][p1]") {
    PcieTlpWireBundle wb;
    wb.kind = PcieTlpWireBundle::MEM_READ;
    wb.requester_id = 0x0100;     // BDF 01:00
    wb.tag = 0x42;
    wb.address_lo = 0x00001000;
    wb.length = 1;                // 1 DW payload

    uint8_t out[4096];
    unsigned out_len = 0;
    wb.tlp_pack_to_bytes(out, out_len);

    const auto expected = PcieTlpCodec::encode_mrd(wb.requester_id, wb.tag,
                                                  wb.address_lo, wb.length);
    REQUIRE(out_len == expected.size());
    REQUIRE(std::memcmp(out, expected.data(), out_len) == 0);
}

TEST_CASE("tlp_pack_to_bytes: MEM_WRITE bridge to PcieTlpCodec::encode_mwr",
          "[pcie][wire][p1]") {
    PcieTlpWireBundle wb;
    wb.kind = PcieTlpWireBundle::MEM_WRITE;
    wb.requester_id = 0x0100;
    wb.tag = 0x55;
    wb.address_lo = 0xDEAD0000;
    wb.length = 2;                // 2 DW payload = 8 bytes
    wb.payload[0] = 0xCAFEBABE;
    wb.payload[1] = 0xDEADBEEF;

    uint8_t out[4096];
    unsigned out_len = 0;
    wb.tlp_pack_to_bytes(out, out_len);

    // Pack payload bytes (LE uint32_t → bytes)
    uint8_t payload_bytes[8];
    std::memcpy(&payload_bytes[0], &wb.payload[0], 4);
    std::memcpy(&payload_bytes[4], &wb.payload[1], 4);
    const auto expected = PcieTlpCodec::encode_mwr(wb.requester_id, wb.tag,
                                                   wb.address_lo,
                                                   payload_bytes, 8);
    REQUIRE(out_len == expected.size());
    REQUIRE(std::memcmp(out, expected.data(), out_len) == 0);
}

TEST_CASE("tlp_unpack_from_bytes is inverse of pack for MEM_READ",
          "[pcie][wire][p1]") {
    // Construct wire bytes via encode, then unpack and verify fields match.
    const uint16_t req_id = 0x0200;
    const uint8_t tag = 0x77;
    const uint32_t addr = 0x12345000;
    const uint16_t len_dw = 1;
    const auto wire_bytes = PcieTlpCodec::encode_mrd(req_id, tag, addr, len_dw);

    PcieTlpWireBundle wb;
    wb.tlp_unpack_from_bytes(wire_bytes.data(), wire_bytes.size());

    REQUIRE(wb.requester_id == req_id);
    REQUIRE(wb.tag == tag);
    REQUIRE(wb.address_lo == addr);
    REQUIRE(wb.length == len_dw);
}

TEST_CASE("crc32_compute matches PcieTlpCodec::crc32_pcie on MEM_READ TLP bytes",
          "[pcie][wire][crc][p1]") {
    PcieTlpWireBundle wb;
    wb.kind = PcieTlpWireBundle::MEM_READ;
    wb.requester_id = 0;
    wb.tag = 0;
    wb.address_lo = 0x1000;
    wb.length = 1;

    const uint32_t wire_crc = wb.crc32_compute();
    uint8_t wire_bytes[4096];
    unsigned wire_len = 0;
    wb.tlp_pack_to_bytes(wire_bytes, wire_len);
    const uint32_t codec_crc = PcieTlpCodec::crc32_pcie(wire_bytes, wire_len);

    REQUIRE(wire_crc == codec_crc);
}

TEST_CASE("PcieTlpCodec::crc_self_test still passes (no bridge regression)",
          "[pcie][wire][p1]") {
    REQUIRE(PcieTlpCodec::crc_self_test());
}