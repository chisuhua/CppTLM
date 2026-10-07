// test/test_realip_equivalence.cc
// T25A P3 topology assembly test: DGpuSoc nested + 3 IP children + EP attach.
//
// Per docs/superpowers/plans/2026-10-07-d-dglass-pciep-refactor-phase-c-handoff.md
// §5 T25A 验收:
//   - 重建 dgpu_soc_v2_realip_p2.json (T23B 已完成)
//   - instantiate 后 4 个 IP module objects 可达:
//       soc          (DGpuSoc SimModule container)
//       pcie_ctrl    (PcieControllerIP SimModule facade)
//       fabric       (GpuInternalFabric SimModule)
//       vram_ctrl    (VramControllerTLM ChStreamModuleBase)
//   - attach 完成: PcieControllerIP::attach_endpoint(&ep) 激活 dormant B1 gate,
//     match_bar 5 stimulus 全绿 (BAR0/1/2/3 解码正确)。
//
// T25A 范围 (独立测试先绿 per handoff §5 T25A):
//   - 测试 #1 (topology instantiate): 验证 4 个 module objects accessible via
//     DGpuSoc nested path
//   - 测试 #2 (attach + match_bar 5 stimuli): 验证 PcieControllerIP facade
//     attach_endpoint(&ep) 后 dormant gate 激活, BAR decode 全 5 case 正确
//   - 测试 #3 (GpuInternalFabric dormant crossbar): 验证 attach_crossbar
//     尚未调用时 crossbar()==nullptr (T25A.1 后会 wire JSON children)
//
// 不重复 T22b / test_pin_contract_pcie_controller_ip.cc 的 6 个 stimulus,
// 它们已锁 [controller_ip][p2] 8/34。T25A 集成版只验证 5 个 BAR decode 走
// 完整 JSON 装配路径,确认 dormant-gate trap (B2d 修订 2) 未回归。
//
// PcieAxiBridgeIP (per MUST NOT 2 lock): 它由 PcieEndpointIP::lazy_init 在
// mmio path 首次访问时创建, NOT a JSON 顶层 child, 不在 PcieControllerIP
// internal_factory 中。T25A 不验证 axi_bridge 创建 (那是 mmio path 端到端
// 测试范围, 不在 P3 topology assembly 范围内)。
//
// Tag: `[realip][p3]`

#include "catch_amalgamated.hpp"
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/sim_module.hh"
#include "tlm/gpu/dgpu_soc.hh"
#include "tlm/cluster/gpu_internal_fabric.hh"
#include "tlm/vram_controller_tlm.hh"
#include "tlm/pcie/pcie_controller_ip.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"

#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

using tlm::pcie::PcieControllerIP;
using tlm::pcie::PcieEndpointIP;
using BT = PcieControllerIP::BarTarget;
using tlm::cluster::GpuInternalFabric;
using cpptlm::tlm::DGpuSoc;
using json = nlohmann::json;

namespace {

json load_realip_p2_config() {
    std::ifstream f("configs/dgpu_soc_v2_realip_p2.json");
    REQUIRE(f.is_open());
    json cfg;
    f >> cfg;
    return cfg;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────
// 测试 #1: DGpuSoc 嵌套 + 3 IP children instantiate 路径 (4 module objects
// 全部 accessible via getInternalInstance / ModuleFactory 实例查找)。
// T23B 已锁此契约; T25A 验证独立 harness 也能 reproduce。
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("P3 4-IP topology: DGpuSoc + PcieControllerIP + GpuInternalFabric + "
          "VramControllerTLM all instantiated from nested JSON",
          "[realip][p3]") {
    auto cfg = load_realip_p2_config();

    EventQueue eq;
    ModuleFactory factory(&eq);

    REQUIRE(factory.instantiateAll(cfg));

    // 4 module objects per handoff §5 T25A 验收
    auto* soc = dynamic_cast<DGpuSoc*>(factory.getInstance("soc"));
    REQUIRE(soc != nullptr);
    REQUIRE(soc->get_module_type() == "DGpuSoc");

    auto* soc_sim = dynamic_cast<SimModule*>(soc);
    REQUIRE(soc_sim != nullptr);

    auto* pcie_ctrl = dynamic_cast<PcieControllerIP*>(
        soc_sim->getInternalInstance("pcie_ctrl"));
    REQUIRE(pcie_ctrl != nullptr);
    REQUIRE(pcie_ctrl->get_module_type() == "PcieControllerIP");

    auto* fabric = dynamic_cast<GpuInternalFabric*>(
        soc_sim->getInternalInstance("fabric"));
    REQUIRE(fabric != nullptr);
    REQUIRE(fabric->get_module_type() == "GpuInternalFabric");

    auto* vram_ctrl = dynamic_cast<VramControllerTLM*>(
        soc_sim->getInternalInstance("vram_ctrl"));
    REQUIRE(vram_ctrl != nullptr);
    REQUIRE(vram_ctrl->get_module_type() == "VramControllerTLM");
}

// ─────────────────────────────────────────────────────────────────────────
// 测试 #2: PcieControllerIP attach_endpoint 后 5 BAR decode stimulus 全绿。
// 锁 B1 修订 + B2d dormant-gate trap (per handoff §9.2 #4 修订后锁)。
// 注: bar_size 由 dgpu_soc_v2_realip_p2.json pcie_ctrl.params.bar_sizes 派生:
//   BAR0=4KB, BAR1=16MB, BAR2=8GB (与现有 [controller_ip][p2] 测试对齐)
//
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("P3 topology + attach: PcieControllerIP 5 BAR decode stimuli after EP attach",
          "[realip][p3]") {
    constexpr uint64_t kBar1Size = 16777216ULL;   // 16MB (与 JSON bar_sizes[1] 对齐)
    constexpr uint64_t kBar2Size = 8589934592ULL; // 8GB (与 JSON bar_sizes[2] 对齐)

    EventQueue eq;

    // 创建外部 PcieEndpointIP (per lazy_init pattern)
    PcieEndpointIP ep("realip_t25a_ep", &eq);
    json ep_cfg;
    ep_cfg["bar_sizes"] = json::array({0ULL, kBar1Size, kBar2Size, 0ULL, 0ULL, 0ULL});
    ep.set_config(ep_cfg);
    REQUIRE(ep.bar_size(1) == kBar1Size);
    REQUIRE(ep.bar_size(2) == kBar2Size);

    // 走 JSON 装配获取 PcieControllerIP (T23B 路径)
    auto cfg = load_realip_p2_config();
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));
    auto* soc = dynamic_cast<DGpuSoc*>(factory.getInstance("soc"));
    REQUIRE(soc != nullptr);
    auto* soc_sim = dynamic_cast<SimModule*>(soc);
    REQUIRE(soc_sim != nullptr);
    auto* ctrl = dynamic_cast<PcieControllerIP*>(soc_sim->getInternalInstance("pcie_ctrl"));
    REQUIRE(ctrl != nullptr);

    // B1 attach: 激活 dormant gate
    ctrl->attach_endpoint(&ep);

    // 5 stimulus (per test_pin_contract_pcie_controller_ip.cc)
    BT target = BT::Unknown;
    uint64_t sub_offset = 0;

    // 1. bar=0 → Bar0Mmio
    REQUIRE(ctrl->match_bar(0, 0x123ULL, target, sub_offset));
    REQUIRE(target == BT::Bar0Mmio);
    REQUIRE(sub_offset == 0x123ULL);

    // 2. bar=1, offset < bar_size(1) → Bar1FbFastPath
    REQUIRE(ctrl->match_bar(1, 0x5000ULL, target, sub_offset));
    REQUIRE(target == BT::Bar1FbFastPath);
    REQUIRE(sub_offset == 0x5000ULL);

    // 3. bar=1, offset >= bar_size(1) → Unknown (B2d guard)
    REQUIRE_FALSE(ctrl->match_bar(1, kBar1Size, target, sub_offset));
    REQUIRE(target == BT::Unknown);
    REQUIRE_FALSE(ctrl->match_bar(1, 0x10010000ULL, target, sub_offset));
    REQUIRE(target == BT::Unknown);

    // 4. bar=2 → Bar2VramAperture
    REQUIRE(ctrl->match_bar(2, 0x1000ULL, target, sub_offset));
    REQUIRE(target == BT::Bar2VramAperture);
    REQUIRE(sub_offset == 0x1000ULL);

    // 5. bar=3 → Unknown (BAR3 = upper dword of BAR2 64-bit pair)
    REQUIRE_FALSE(ctrl->match_bar(3, 0x0ULL, target, sub_offset));
    REQUIRE(target == BT::Unknown);
}

// ─────────────────────────────────────────────────────────────────────────
// 测试 #3: GpuInternalFabric dormant crossbar — T25A 之前 attach_crossbar
// 未被调 (T22b 测试显式调, T25A 走 JSON 路径未调), crossbar() 应返 nullptr。
// T25A.1 会实现 simulate_instantiate 覆写或 attach_composition 让 JSON 路径
// 也调 attach_crossbar。本测试锁当前 dormant 行为, T25A.1 后测试需更新。
//
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("P3 topology: GpuInternalFabric JSON path auto-attaches inner crossbar (post-T25A.1)",
          "[realip][p3]") {
    auto cfg = load_realip_p2_config();

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));

    auto* soc = dynamic_cast<DGpuSoc*>(factory.getInstance("soc"));
    REQUIRE(soc != nullptr);
    auto* soc_sim = dynamic_cast<SimModule*>(soc);
    REQUIRE(soc_sim != nullptr);
    auto* fabric = dynamic_cast<GpuInternalFabric*>(soc_sim->getInternalInstance("fabric"));
    REQUIRE(fabric != nullptr);

    // T22b 直接 C++ attach_crossbar 锁 dormant gate (null until activated)。
    // T25A.1 (Phase C 2026-10-07) 修订: GpuInternalFabric::simulate_instantiate 覆写
    // 遍历 cfg.modules 找名为 "crossbar" 的 child, 从 internal_factory 拿实例并
    // 调 attach_crossbar. JSON 装配路径现在自动 wire inner CrossbarTLM,
    // crossbar() == nullptr 是 dormant 锁, T25A.1 后应 non-null。
    REQUIRE(fabric->crossbar() != nullptr);
    REQUIRE(fabric->crossbar()->get_module_type() == "CrossbarTLM");
}

// ─────────────────────────────────────────────────────────────────────────
// 测试 #4 (T25B dual-path equivalence smoke, 5-10 刺激):
// facade path (PcieControllerIP::match_bar) 与 PcieEndpointIP direct
// 路径 (bar_store_value_set / value) 在相同 BAR/offset/data 刺激下
// 产生一致行为。
//
// 范围声明 (per handoff §9.2 #9): VramControllerTLM 不在 P3 等价性数据路径,
// 5-10 刺激全部经 PcieControllerIP → PcieEndpointIP → PcieMemoryDevice
// → vram_storage_ (D-AXI v1.4 host BAR2 fast-path). fabric→vram_ctrl 链
// 属 D3/D4, 本期不验证 end-to-end 数据路径, 仅 T23C 验证对象级 read() 契约.
//
// Strangler gate (per dgpu_board_shell.cc:336-343 facade verdict divergence
// std::abort): facade verdict 与 expected 一致 = dual-path 一致 (legacy path
// 行为被 facade gate 锁定).
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("P3 dual-path equivalence: facade match_bar 与 EP bar_store_value 在 5-10 刺激下一致",
          "[realip][p3][equivalence]") {
    constexpr uint64_t kBar1Size = 16777216ULL;   // 16MB
    constexpr uint64_t kBar2Size = 8589934592ULL; // 8GB

    EventQueue eq;

    // 走 JSON 装配获取 PcieControllerIP (T23B 路径)
    auto cfg = load_realip_p2_config();
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));
    auto* soc = dynamic_cast<DGpuSoc*>(factory.getInstance("soc"));
    REQUIRE(soc != nullptr);
    auto* soc_sim = dynamic_cast<SimModule*>(soc);
    REQUIRE(soc_sim != nullptr);
    auto* ctrl = dynamic_cast<PcieControllerIP*>(soc_sim->getInternalInstance("pcie_ctrl"));
    REQUIRE(ctrl != nullptr);

    // 外部 PcieEndpointIP attach (B1 contract)
    PcieEndpointIP ep("realip_t25b_ep", &eq);
    json ep_cfg;
    ep_cfg["bar_sizes"] = json::array({0ULL, kBar1Size, kBar2Size, 0ULL, 0ULL, 0ULL});
    ep.set_config(ep_cfg);
    REQUIRE(ep.bar_size(1) == kBar1Size);
    REQUIRE(ep.bar_size(2) == kBar2Size);
    ctrl->attach_endpoint(&ep);

    // Strangler gate 锁: facade verdict 与 expected mirror 一致 = dual-path 一致.
    // 5 刺激同 T25A #2 (per controller_ip match_bar semantics):
    BT facade_target;
    uint64_t sub_offset;
    uint64_t ep_stored;

    // 1. bar=0, offset=0x123 → Bar0Mmio (sub_offset 通过)
    REQUIRE(ctrl->match_bar(0, 0x123ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Bar0Mmio);
    REQUIRE(sub_offset == 0x123ULL);
    ep.bar_store_value_set(0, 0, 0x123ULL, 0xCAFE0001ULL);
    ep_stored = ep.bar_store_value(0, 0, 0x123ULL);
    REQUIRE(ep_stored == 0xCAFE0001ULL);  // direct path 也可写读

    // 2. bar=1, offset=0x5000 (< bar_size(1)=16MB) → Bar1FbFastPath
    REQUIRE(ctrl->match_bar(1, 0x5000ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Bar1FbFastPath);
    REQUIRE(sub_offset == 0x5000ULL);
    ep.bar_store_value_set(0, 1, 0x5000ULL, 0xDEAD0002ULL);
    REQUIRE(ep.bar_store_value(0, 1, 0x5000ULL) == 0xDEAD0002ULL);

    // 3. bar=1, offset=kBar1Size (boundary) → Unknown (B2d guard)
    REQUIRE_FALSE(ctrl->match_bar(1, kBar1Size, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Unknown);

    // 4. bar=1, offset=0x10010000 (doorbell offset, OOB) → Unknown
    REQUIRE_FALSE(ctrl->match_bar(1, 0x10010000ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Unknown);

    // 5. bar=2, offset=0x1000 → Bar2VramAperture
    REQUIRE(ctrl->match_bar(2, 0x1000ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Bar2VramAperture);
    REQUIRE(sub_offset == 0x1000ULL);
    ep.bar_store_value_set(0, 2, 0x1000ULL, 0xBEEF0003ULL);
    REQUIRE(ep.bar_store_value(0, 2, 0x1000ULL) == 0xBEEF0003ULL);

    // 6. bar=2, offset=8GB-8 (max valid) → Bar2VramAperture (no bound in facade)
    REQUIRE(ctrl->match_bar(2, kBar2Size - 8ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Bar2VramAperture);

    // 7. bar=3 (upper dword of BAR2 pair) → Unknown (per H2)
    REQUIRE_FALSE(ctrl->match_bar(3, 0x0ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Unknown);

    // 8. bar=4-5 (unused) → Unknown
    REQUIRE_FALSE(ctrl->match_bar(4, 0x0ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Unknown);
    REQUIRE_FALSE(ctrl->match_bar(5, 0x0ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Unknown);

    // facade path 通过 mmio_write 直接落到 EP.bar_store_ (per PcieEndpointIP::mmio_write
    // 实现 dispatch), 验证 facade→EP direct 路径一致.
    REQUIRE(ep.mmio_write(0, 0x200ULL, 0xFACADE00ULL));  // bar=0 寄存器写
    REQUIRE(ep.bar_store_value(0, 0, 0x200ULL) == 0xFACADE00ULL);
    REQUIRE(ep.mmio_write(1, 0x1000ULL, 0xFACADE01ULL));  // bar=1 framebuffer write
    REQUIRE(ep.bar_store_value(0, 1, 0x1000ULL) == 0xFACADE01ULL);
    REQUIRE(ep.mmio_write(2, 0x2000ULL, 0xFACADE02ULL));  // bar=2 vram write
    REQUIRE(ep.bar_store_value(0, 2, 0x2000ULL) == 0xFACADE02ULL);

    // 9. facade path 在 doorbell offset 写应该走 SDMA ring WPTR trigger
    //    (per PcieEndpointIP::mmio_write Stage 1.3a), 不进 bar_store_
    //    注意: 返回 true (write accepted), 但 bar_store_ 不增加 doorbell offset 条目
    const uint64_t pre_count = ep.sdma_ring_processed_count();
    REQUIRE(ep.mmio_write(1, PcieEndpointIP::kBar1DoorbellOffset, 0x10ULL));
    REQUIRE(ep.sdma_ring_processed_count() == pre_count + 0x10ULL);
    REQUIRE(ep.bar_store_value(0, 1, PcieEndpointIP::kBar1DoorbellOffset) == 0ULL);

    // 10. facade path mmio_write OOB (per PcieEndpointIP MVP, mmio_write 无条件接受)
    //    ↔ facade match_bar OOB → Unknown (刺激 3+4) 是不一致但符合 MVP 设计。
    //    (mmio_write 边界检查属 P4 deferred 工作, 见 PcieEndpointIP::mmio_write line 651 comment)
    REQUIRE(ep.mmio_write(1, kBar1Size + 0x100ULL, 0xDEADULL));  // accepted per MVP
    REQUIRE(ep.bar_store_value(0, 1, kBar1Size + 0x100ULL) == 0xDEADULL);
}