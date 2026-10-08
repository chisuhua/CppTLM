// test/test_realip_pin_contract.cc
// A.1 RED → A.5 GREEN: Fabric2VramCtrlBundle field-width static_assert + programmatic bridge round-trip
// 双标签 [realip][pin-contract] per handoff §9.2 #10 防 vacuous pass
// Author: CppTLM Team / Date: 2026-10-08

#include "bundles/dgpu_fabric_bundles_tlm.hh"
#include "utils/port_contract_asserts.hh"
#include "core/event_queue.hh"
#include "tlm/cluster/gpu_internal_fabric.hh"
#include "tlm/vram_controller_tlm.hh"

#include <catch_amalgamated.hpp>
#include <memory>
#include <type_traits>

using namespace bundles;

// TEST_CASE #1: 8 字段宽度对齐 bundles::ch_uint<W> width constants
// per design AD-5 + Oracle B.6 wide-path < 64 限制
TEST_CASE("Fabric2VramCtrlBundle field widths match design AD-5", "[realip][pin-contract]") {
    Fabric2VramCtrlBundle bundle{};

    STATIC_REQUIRE(decltype(bundle.address)::width == 64);
    STATIC_REQUIRE(decltype(bundle.size)::width == 8);
    STATIC_REQUIRE(decltype(bundle.is_write)::width == 1);
    STATIC_REQUIRE(decltype(bundle.data)::width == 64);
    STATIC_REQUIRE(decltype(bundle.transaction_id)::width == 32);
    STATIC_REQUIRE(decltype(bundle.resp_data)::width == 64);
    STATIC_REQUIRE(decltype(bundle.error_code)::width == 8);
    STATIC_REQUIRE(decltype(bundle.chip_select)::width == 8);

    BUNDLE_FIELD_WIDTH(Fabric2VramCtrlBundle, address, 64);
    BUNDLE_FIELD_WIDTH(Fabric2VramCtrlBundle, chip_select, 8);
}

// TEST_CASE #2 (Momus 修订: set_vram_target / vram_ctrl() accessors + build test)
TEST_CASE("GpuInternalFabric exposes set_vram_target / vram_ctrl accessors", "[realip][pin-contract]") {
    EventQueue eq;
    auto fabric = std::make_unique<tlm::cluster::GpuInternalFabric>("fabric", &eq);

    REQUIRE(fabric != nullptr);

    // 默认 vram_ctrl_ = nullptr
    REQUIRE(fabric->vram_ctrl() == nullptr);

    // 构造 vram_ctrl 指针并 setter
    auto vram = std::make_unique<VramControllerTLM>("vram_ctrl", &eq);
    VramControllerTLM* vram_ptr = vram.get();

    fabric->set_vram_target(vram_ptr);
    REQUIRE(fabric->vram_ctrl() == vram_ptr);
}

// TEST_CASE #3 (Phase A.5 完整 round-trip: 双向桥接)
TEST_CASE("set_vram_target programmatic bridge round-trip fabric↔vram_ctrl", "[realip][pin-contract]") {
    EventQueue eq;

    auto fabric = std::make_unique<tlm::cluster::GpuInternalFabric>("fabric", &eq);
    auto vram = std::make_unique<VramControllerTLM>("vram_ctrl", &eq);

    REQUIRE(fabric != nullptr);
    REQUIRE(vram != nullptr);

    // 起始两端均为 nullptr
    REQUIRE(fabric->vram_ctrl() == nullptr);
    REQUIRE(vram->fabric() == nullptr);

    // 模拟 DGpuBoard::bind_memory_backings() 中的双向桥接
    VramControllerTLM* vram_ptr = vram.get();
    tlm::cluster::GpuInternalFabric* fabric_ptr = fabric.get();
    fabric->set_vram_target(vram_ptr);
    vram->set_fabric_source(fabric_ptr);

    // 验证双向桥接两端均生效
    REQUIRE(fabric->vram_ctrl() == vram_ptr);
    REQUIRE(vram->fabric() == fabric_ptr);

    // 桥接的对称性: fabric->vram_ctrl() 与 vram->fabric() 互指
    REQUIRE(fabric->vram_ctrl()->fabric() == fabric_ptr);
    REQUIRE(vram->fabric()->vram_ctrl() == vram_ptr);
}