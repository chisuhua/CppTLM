// test/test_pcie_tlp_serializer.cc
// PcieTlpSerializer 单元测试 (per X1 v1.1 spec AC G2)
//
// 测试标签: [pcie][tlp-serializer][unit]
#include "catch_amalgamated.hpp"

#include "bundles/pcie_bundles_tlm.hh"
#include "utils/pcie_tlp_serializer.hh"

#include <cstdint>

using cpptlm::utils::PcieTlpSerializer;
using bundles::PcieTlpBundle;

TEST_CASE("PcieTlpSerializer: encode_config_read fields",
          "[pcie][tlp-serializer][unit][cfg]") {
    PcieTlpSerializer ser;
    auto tlp = ser.encode_config_read(0x100, 0x10, 4);

    CHECK(tlp.kind.read() == PcieTlpBundle::CFG_READ);
    CHECK(tlp.offset.read() == 0x10);
    CHECK(tlp.size.read() == 4);
    CHECK(tlp.requester_id.read() == 0x100);
    CHECK(tlp.data.read() == 0);
    CHECK(tlp.bar_index.read() == 0);  // CFG 不用 BAR
    CHECK(tlp.trans_id.read() == 0);    // 第一个分配
}

TEST_CASE("PcieTlpSerializer: encode_config_write fields",
          "[pcie][tlp-serializer][unit][cfg]") {
    PcieTlpSerializer ser;
    auto tlp = ser.encode_config_write(0x100, 0x20, 4, 0xDEADBEEF);

    CHECK(tlp.kind.read() == PcieTlpBundle::CFG_WRITE);
    CHECK(tlp.offset.read() == 0x20);
    CHECK(tlp.size.read() == 4);
    CHECK(tlp.data.read() == 0xDEADBEEF);
    CHECK(tlp.requester_id.read() == 0x100);
}

TEST_CASE("PcieTlpSerializer: encode_mmio_read fields",
          "[pcie][tlp-serializer][unit][mmio]") {
    PcieTlpSerializer ser;
    auto tlp = ser.encode_mmio_read(2, 0x10000, 8);

    CHECK(tlp.kind.read() == PcieTlpBundle::MMIO_READ);
    CHECK(tlp.bar_index.read() == 2);
    CHECK(tlp.offset.read() == 0x10000);
    CHECK(tlp.size.read() == 8);
}

TEST_CASE("PcieTlpSerializer: encode_mmio_write fields",
          "[pcie][tlp-serializer][unit][mmio]") {
    PcieTlpSerializer ser;
    auto tlp = ser.encode_mmio_write(0, 0x2000, 4, 0xCAFEBABE);

    CHECK(tlp.kind.read() == PcieTlpBundle::MMIO_WRITE);
    CHECK(tlp.bar_index.read() == 0);
    CHECK(tlp.offset.read() == 0x2000);
    CHECK(tlp.size.read() == 4);
    CHECK(tlp.data.read() == 0xCAFEBABE);
}

TEST_CASE("PcieTlpSerializer: encode_completion fields",
          "[pcie][tlp-serializer][unit][cpld]") {
    PcieTlpSerializer ser;
    auto tlp = ser.encode_completion(0x100, 0xFEEDFACE, true);

    CHECK(tlp.kind.read() == PcieTlpBundle::CPLD);
    CHECK(tlp.requester_id.read() == 0x100);
    CHECK(tlp.data.read() == 0xFEEDFACE);
    CHECK(tlp.size.read() == 8);
}

TEST_CASE("PcieTlpSerializer: decode round-trip preserves fields",
          "[pcie][tlp-serializer][unit][roundtrip]") {
    PcieTlpSerializer ser;
    auto tlp = ser.encode_config_write(0x100, 0x10, 4, 0xDEADBEEF);

    auto decoded = ser.decode(tlp);
    CHECK(decoded.type == PcieTlpBundle::CFG_WRITE);
    CHECK(decoded.bdf == 0x100);
    CHECK(decoded.address == 0x10);
    CHECK(decoded.byte_count == 4);
    CHECK(decoded.data == 0xDEADBEEF);
}

TEST_CASE("PcieTlpSerializer: trans_id monotonically increasing",
          "[pcie][tlp-serializer][unit][trans-id]") {
    PcieTlpSerializer ser;
    auto tlp1 = ser.encode_config_read(0, 0, 4);
    auto tlp2 = ser.encode_config_read(0, 0, 4);
    auto tlp3 = ser.encode_config_read(0, 0, 4);

    CHECK(tlp1.trans_id.read() == 0);
    CHECK(tlp2.trans_id.read() == 1);
    CHECK(tlp3.trans_id.read() == 2);
    CHECK(ser.alloc_trans_id_count() == 3);
}
