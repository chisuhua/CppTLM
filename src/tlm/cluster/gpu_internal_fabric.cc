// src/tlm/cluster/gpu_internal_fabric.cc
// P3 GpuInternalFabric: minimal SimModule impl (P3 Task 22 scaffold).
//
// T25A.1 (Phase C 2026-10-07) 修订: 实现 simulate_instantiate 覆写, 让
// ModuleFactory JSON 装配路径能自动 attach inner CrossbarTLM child。
// 锁 MUST NOT (per Oracle bg_c60653e0 §3.4): inner crossbar MUST 来自
// internal_factory JSON child instance (Step 7 StreamAdapter 注入不会跳过),
// NEVER raw `new`。
#include "tlm/cluster/gpu_internal_fabric.hh"

namespace tlm::cluster {

GpuInternalFabric::GpuInternalFabric(const std::string& name, EventQueue* eq)
    : SimModule(name, eq) {}

void GpuInternalFabric::simulate_instantiate(const nlohmann::json& cfg) {
    // 基类 instantiateAll + 递归子 SimModule (line 81-109 sim_module.hh)
    SimModule::simulate_instantiate(cfg);

    // 幂等守卫: T22b 直接 C++ attach_crossbar 已调 → crossbar_ 非空, 不覆盖
    if (crossbar_ != nullptr) {
        return;
    }

    // 遍历 cfg.modules 找名为 "crossbar" 的 child, 从 internal_factory 拿实例
    // (per Oracle §3.4 inner crossbar MUST 来自 JSON child instance)
    if (cfg.contains("modules") && cfg["modules"].is_array()) {
        for (const auto& child_cfg : cfg["modules"]) {
            const std::string child_name = child_cfg.value("name", std::string{});
            if (child_name == "crossbar") {
                if (auto* xbar = dynamic_cast<CrossbarTLM*>(
                        internal_factory->getInstance("crossbar"))) {
                    attach_crossbar(xbar);
                }
                break;
            }
        }
    }
}

} // namespace tlm::cluster