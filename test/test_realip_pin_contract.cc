// test/test_realip_pin_contract.cc
// Phase A.1 RED: Fabric2VramCtrlBundle field-width static_assert coverage
// 双标签 [realip][pin-contract] per handoff §9.2 #10 防 vacuous pass
// Author: CppTLM Team / Date: 2026-10-08

#include "bundles/dgpu_fabric_bundles_tlm.hh"
#include "utils/port_contract_asserts.hh"

#include <catch_amalgamated.hpp>
#include <type_traits>

using namespace bundles;

// TEST_CASE #1: 8 字段宽度对齐 bundles::ch_uint<W> width constants (Momus 修订: ch_uint 加 width 静态常量后)
// per design AD-5 + Oracle B.6 wide-path < 64 限制
TEST_CASE("Fabric2VramCtrlBundle field widths match design AD-5", "[realip][pin-contract]") {
    using T = Fabric2VramCtrlBundle;
    T bundle{};

    STATIC_REQUIRE(decltype(bundle.address)::width == 64);
    STATIC_REQUIRE(decltype(bundle.size)::width == 8);
    STATIC_REQUIRE(decltype(bundle.is_write)::width == 1);
    STATIC_REQUIRE(decltype(bundle.data)::width == 64);
    STATIC_REQUIRE(decltype(bundle.transaction_id)::width == 32);
    STATIC_REQUIRE(decltype(bundle.resp_data)::width == 64);
    STATIC_REQUIRE(decltype(bundle.error_code)::width == 8);
    STATIC_REQUIRE(decltype(bundle.chip_select)::width == 8);

    // 同时验证 BUNDLE_FIELD_WIDTH macro 工作
    BUNDLE_FIELD_WIDTH(Fabric2VramCtrlBundle, address, 64);
    BUNDLE_FIELD_WIDTH(Fabric2VramCtrlBundle, chip_select, 8);
}

// TEST_CASE #2 (Momus 修订: 改为检查 setter accessor, 不检查 ChStreamInitiatorPort<Bundle> 模板)
TEST_CASE("GpuInternalFabric exposes set_vram_target / vram_ctrl accessors", "[realip][pin-contract]") {
    // Phase A.3 填写
    FAIL("Phase A.3 pending — set_vram_target accessor not yet declared");
}

// TEST_CASE #3 (Phase A.5 填写)
TEST_CASE("set_vram_target programmatic bridge round-trip fabric→vram_ctrl", "[realip][pin-contract]") {
    FAIL("Phase A.5 pending — set_vram_target not yet called from DGpuBoard");
}