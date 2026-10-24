// test/test_realip_json_nested_config.cc
// T23B pin: dgpu_soc_v2_realip_p2.json nested DGpuSoc + 3 IP children
// instantiate and expose via getInternalInstance.
//
// Per docs/superpowers/plans/2026-10-07-dgpu-realip-refactor-phase-c-handoff.md
// §5 T23B 验收:
//   - 重建 dgpu_soc_v2_realip_p2.json 为 DGpuSoc 嵌套
//   - instantiate 后 3 个直接 child getInternalInstance 非空:
//       pcie_ctrl (PcieControllerIP SimModule)
//       fabric    (GpuInternalFabric SimModule)
//       vram_ctrl (VramControllerTLM ChStreamModuleBase)
//   - PcieAxiBridgeIP 是 PcieEndpointIP::lazy_init 创建的 EP composite,
//     NOT a JSON 顶层 child (per P3 Amendment + MUST NOT 2);
//     见 T22b / test_pin_contract_pcie_controller_ip.cc 验证 B1 dormant gate
//
// TDD 5 步:
//   Step 1 RED:  当前 JSON 是平铺 2 modules, soc 顶层不存在 → factory.getInstance("soc") 返 nullptr
//   Step 2 GREEN: 重建 JSON 为 DGpuSoc 嵌套 + 3 children → instantiateAll 找到 3 internal instances
//   Step 3 REFACTOR: 加 PcieAxiBridgeIP 间接查找断言 + type 断言
//   Step 4 COMMIT
//   Step 5 VERIFY §10 + new gate [realip][p3] 至少 1 个 test case
//
// Tag: `[realip][p3]` (per handoff §9.2 #10: 必须 commit 前打 tag, 避免 vacuous gate)

#include "chstream_register.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/sim_module.hh"
#include "tlm/gpu/dgpu_soc.hh"

#include <fstream>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>

namespace {

    nlohmann::json load_config(const std::string& rel_path) {
        std::ifstream f(rel_path);
        if (!f.is_open()) {
            throw std::runtime_error("cannot open config: " + rel_path);
        }
        nlohmann::json cfg;
        f >> cfg;
        return cfg;
    }

} // namespace

TEST_CASE("dgpu_soc_v2_realip_p2.json: DGpuSoc nested + 3 IP children instantiate",
          "[realip][p3]") {
    auto cfg = load_config("configs/dgpu_soc_v2_realip_p2.json");
    REQUIRE(cfg.contains("modules"));

    EventQueue eq;
    ModuleFactory factory(&eq);

    REQUIRE(factory.instantiateAll(cfg));

    auto* soc = factory.getInstance("soc");
    REQUIRE(soc != nullptr); // 顶层必须是 DGpuSoc, name="soc"
    REQUIRE(soc->get_module_type() == "DGpuSoc");

    auto* sim_soc = dynamic_cast<SimModule*>(soc);
    REQUIRE(sim_soc != nullptr);

    // 3 个直接 children 必须 instantiate 成功
    REQUIRE(sim_soc->getInternalInstance("pcie_ctrl") != nullptr);
    REQUIRE(sim_soc->getInternalInstance("fabric") != nullptr);
    REQUIRE(sim_soc->getInternalInstance("vram_ctrl") != nullptr);

    // 类型断言 — 锁 4-IP 边界: PcieControllerIP (SimModule), GpuInternalFabric (SimModule),
    // VramControllerTLM (ChStream, 非 SimModule)。
    auto* pcie_ctrl = sim_soc->getInternalInstance("pcie_ctrl");
    auto* fabric = sim_soc->getInternalInstance("fabric");
    auto* vram_ctrl = sim_soc->getInternalInstance("vram_ctrl");

    REQUIRE(pcie_ctrl->get_module_type() == "PcieControllerIP");
    REQUIRE(fabric->get_module_type() == "GpuInternalFabric");
    REQUIRE(vram_ctrl->get_module_type() == "VramControllerTLM");
}

TEST_CASE("dgpu_soc_v2_realip_p2.json: 4-IP topology contract — MUST NOT 2 lock", "[realip][p3]") {
    auto cfg = load_config("configs/dgpu_soc_v2_realip_p2.json");

    EventQueue eq;
    ModuleFactory factory(&eq);

    REQUIRE(factory.instantiateAll(cfg));

    auto* soc = factory.getInstance("soc");
    REQUIRE(soc != nullptr);
    auto* sim_soc = dynamic_cast<SimModule*>(soc);
    REQUIRE(sim_soc != nullptr);

    auto* pcie_ctrl = sim_soc->getInternalInstance("pcie_ctrl");
    REQUIRE(pcie_ctrl != nullptr);
    auto* ctrl_sim = dynamic_cast<SimModule*>(pcie_ctrl);
    REQUIRE(ctrl_sim != nullptr);

    // MUST NOT 2 lock: PcieAxiBridgeIP is EP composite (lazy_init inside
    // PcieEndpointIP), NOT a JSON 顶层 or PcieControllerIP internal child.
    // Direct查找应返 nullptr;顶层 JSON 中无 "PcieAxiBridgeIP" 实例 (per
    // grep-gate scripts/test/check_pcie_ip_facade.sh:54).
    auto* axi_bridge_direct = ctrl_sim->getInternalInstance("axi_bridge");
    REQUIRE(axi_bridge_direct == nullptr);

    // PcieControllerIP 的 internal_factory 是空的 (PcieAxiBridgeIP 不在那里)
    // → 锁 contract: ctrl 自己 instantiate 后不直接持有 axi_bridge。
    // (实际 4-IP 链由后续 T23C/T25B 通过 PcieEndpointIP::attach_composition 走)
}