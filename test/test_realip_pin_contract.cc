// test/test_realip_pin_contract.cc
// A.1 RED → A.5 GREEN: Fabric2VramCtrlBundle field-width static_assert + programmatic bridge
// round-trip 双标签 [realip][pin-contract] per handoff §9.2 #10 防 vacuous pass Author: CppTLM Team
// / Date: 2026-10-08

#include "bundles/dgpu_fabric_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh" // D.4 (#4) NoCFlitBundle — chip-internal fabric↔SDMA/GMMU
#include "core/event_queue.hh"
#include "tlm/cluster/gpu_internal_fabric.hh"
#include "tlm/pcie/pcie_controller_ip.hh" // match_bar 签名锁 (Change B regression guard)
#include "tlm/vram_controller_tlm.hh"
#include "utils/port_contract_asserts.hh"

#include <memory>
#include <type_traits>
#include <catch_amalgamated.hpp>

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
TEST_CASE("GpuInternalFabric exposes set_vram_target / vram_ctrl accessors",
          "[realip][pin-contract]") {
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
TEST_CASE("set_vram_target programmatic bridge round-trip fabric↔vram_ctrl",
          "[realip][pin-contract]") {
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

// ─────────────────────────────────────────────────────────────────────────────
// TEST_CASE #4 (D.4 #4 NoCFlitBundle 编译期契约, 2026-10-08)
// 为 SDMA/GMMU ↔ GpuInternalFabric 端口从 PcieTlpBundle 迁移到 NoCFlitBundle 准备
// 编译期检查机制。SDMA 5 端口混合 wire-format (v1.8 H1 实际) 当前用 PcieTlpBundle,
// 未来 D.4 实施时通过 BUNDLE_FIELD_WIDTH 锁住 NoCFlitBundle 字段宽度 + ch_bool
// 类型签名, 让 wire-format 漂移在编译期被捕获 (per chip-internal↔board-level
// 边界严格分离 ADR-DGPU-06)。
//
// 设计依据 (per SDMA/GMMU 迁移设计):
//   - NoCFlitBundle 18 字段, max 64-bit (避免 ch_uint<W>64 wide-path shim, per P0 fix)
//   - is_write / is_ok 是 ch_bool (1-bit), 通过 is_same_v 验证类型
//   - 其余 16 字段是 ch_uint<W>, 通过 BUNDLE_FIELD_WIDTH 锁宽度
//   - mesh 路由字段 src_node/dst_node 锁 32-bit (匹配 GpuNoC/GpuInternalFabric 节点空间)
//   - VC 字段 vc_id 锁 8-bit (支持 0-3 + 扩展位)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("NoCFlitBundle field widths + types (SDMA/GMMU migration prep)",
          "[realip][pin-contract][noc-flit]") {
    NoCFlitBundle flit{};

    // ch_uint 字段宽度锁 (16 字段, max 64-bit per P0 fix)
    // ── 事务元数据
    STATIC_REQUIRE(decltype(flit.transaction_id)::width == 64);
    STATIC_REQUIRE(decltype(flit.src_node)::width == 32);
    STATIC_REQUIRE(decltype(flit.dst_node)::width == 32);

    // ── 地址与数据
    STATIC_REQUIRE(decltype(flit.address)::width == 64);
    STATIC_REQUIRE(decltype(flit.data)::width == 64);
    STATIC_REQUIRE(decltype(flit.size)::width == 8);

    // ── NoC 路由与流控
    STATIC_REQUIRE(decltype(flit.vc_id)::width == 8);
    STATIC_REQUIRE(decltype(flit.flit_type)::width == 8);
    STATIC_REQUIRE(decltype(flit.flit_index)::width == 8);
    STATIC_REQUIRE(decltype(flit.flit_count)::width == 8);
    STATIC_REQUIRE(decltype(flit.hops)::width == 8);

    // ── 请求/响应区分 + 错误码
    STATIC_REQUIRE(decltype(flit.flit_category)::width == 8);
    STATIC_REQUIRE(decltype(flit.src_port)::width == 8);
    STATIC_REQUIRE(decltype(flit.error_code)::width == 8);

    // BUNDLE_FIELD_WIDTH 宏对 ch_uint 字段生效 (核心契约机制, 复用 A.1 模板)
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, transaction_id, 64);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, src_node, 32);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, dst_node, 32);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, address, 64);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, data, 64);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, vc_id, 8);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, flit_type, 8);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, flit_index, 8);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, flit_count, 8);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, hops, 8);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, flit_category, 8);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, src_port, 8);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, error_code, 8);

    // ch_bool 字段类型签名锁 (is_write / is_ok 是 ch_bool 而非 ch_uint<1>,
    // 原因: chip-internal 仿真用 bool 而非 1-bit 整数, 避免 stream_adapter 序列化歧义)
    // 通过 is_same_v 验证类型不变性 — 若未来有人误改为 ch_uint<1>, 编译失败
    STATIC_REQUIRE(std::is_same_v<decltype(flit.is_write), bundles::ch_bool>);
    STATIC_REQUIRE(std::is_same_v<decltype(flit.is_ok), bundles::ch_bool>);

    // 工厂方法可用性 (per design AD-3 make_head / make_resp_head)
    auto head_flit = NoCFlitBundle::make_head(
        /*tid=*/0x1234, /*src=*/1, /*dst=*/2,
        /*addr=*/0x1000, /*vc=*/0, /*total_flits=*/1,
        /*write=*/true, /*data_word=*/0xDEADBEEF);
    STATIC_REQUIRE(decltype(head_flit.transaction_id)::width == 64);
    REQUIRE(head_flit.flit_type.read() == NoCFlitBundle::FLIT_HEAD_TAIL); // single-flit → HEAD_TAIL
    REQUIRE(head_flit.flit_category.read() == NoCFlitBundle::CATEGORY_REQUEST);
    REQUIRE(head_flit.is_write.read() == true);

    auto resp_flit = NoCFlitBundle::make_resp_head(
        /*tid=*/0x1234, /*src=*/2, /*dst=*/1,
        /*vc=*/0, /*total_flits=*/1,
        /*ok=*/true, /*err=*/0);
    REQUIRE(resp_flit.flit_type.read() == NoCFlitBundle::FLIT_HEAD_TAIL);
    REQUIRE(resp_flit.flit_category.read() == NoCFlitBundle::CATEGORY_RESPONSE);
    REQUIRE(resp_flit.is_ok.read() == true);

    // 谓词方法 (per is_head/is_tail/is_request/is_response API)
    REQUIRE(head_flit.is_head());
    REQUIRE(head_flit.is_tail());
    REQUIRE(head_flit.is_request());
    REQUIRE_FALSE(head_flit.is_response());
    REQUIRE(resp_flit.is_response());
    REQUIRE_FALSE(resp_flit.is_request());
}

// TEST_CASE #5: PcieControllerIP ↔ PcieEndpointIP match_bar 签名锁 (Change B B.2b)
// PcieControllerIP 是 stateless facade, 所有数据经 ep_ 委托。match_bar 是
// public API, 签名变更 = breaking change (per Oracle bg_14b6dd83 AD-B3)。
// 编译期 static_assert 锁签名, 防未来 refactor 误改返回值类型或 noexcept。
TEST_CASE("PcieControllerIP::match_bar signature is locked (Change B regression guard)",
          "[realip][pin-contract][b-regression]") {
    // BarTarget 是 PcieControllerIP 内嵌 enum class, 完整路径须走类作用域
    using BarTarget = tlm::pcie::PcieControllerIP::BarTarget;
    using MatchBarFn =
        bool (tlm::pcie::PcieControllerIP::*)(uint8_t, uint64_t, BarTarget&, uint64_t&) noexcept;
    using ActualFn = decltype(&tlm::pcie::PcieControllerIP::match_bar);
    STATIC_REQUIRE(std::is_same_v<ActualFn, MatchBarFn>);
}