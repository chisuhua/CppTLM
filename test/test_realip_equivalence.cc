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
TEST_CASE("P3 topology: GpuInternalFabric.crossbar() dormant (T25A.1 待 wire JSON children)",
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

    // T22b 直接 C++ attach_crossbar 锁 dormant gate (null until activated)
    // T25A.1 之前: JSON 装配路径未触发 attach → crossbar() == nullptr
    // T25A.1 之后: 期望 crossbar() != nullptr (由 simulate_instantiate 注入)
    // 当前 (T25A): 锁 dormant 行为, 测试名标注 "T25A.1 待 wire"
    REQUIRE(fabric->crossbar() == nullptr);
}