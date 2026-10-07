// include/tlm/cluster/gpu_internal_fabric.hh
// P3 GpuInternalFabric: SimModule wrapper around crossbar_tlm/router_tlm
//
// Per Oracle bg_c60653e0 §3.4 + Metis bg_4051aad9:
//   - extends SimModule (composite, NOT ChStreamModuleBase — inner is ChStream)
//   - inner crossbar must be instantiated via ModuleFactory's object path
//     (JSON child modules), NEVER raw `new` — otherwise Step 7 adapter
//     injection silently skips (commit-time hooks dead-port scenario)
//   - routing table configurable via JSON params (no hardcoded routing)
//
// PR-A scope: class definition + facade getters. Wire-up of inner
// crossbar deferred to PR-B (or follow-up) when JSON child-module path
// is exercised end-to-end.
#ifndef CPPTLM_GPU_INTERNAL_FABRIC_H
#define CPPTLM_GPU_INTERNAL_FABRIC_H

#include "core/sim_module.hh"
#include "tlm/crossbar_tlm.hh"

#include <nlohmann/json.hpp>
#include <string>

namespace tlm::cluster {

// T22b Phase B 修订 — forward-decl `class CrossbarTLM;` 在 tlm::cluster 命名
// 空间内创建了一个不存在的新符号 (tlm::cluster::CrossbarTLM),与真实的
// 全局命名空间 CrossbarTLM 类型不一致,导致 attach_crossbar() 永远无法被调用
// (T22 commit 95a68889 scaffold-only 漏修此 bug,被 T22b pin-contract test 暴露).
//
// Fix: 直接 #include "tlm/crossbar_tlm.hh",引用真实全局命名空间 CrossbarTLM。
// 代价: 增加 include cost ~10KB,但对 SimModule composite 头文件可接受。
class GpuInternalFabric : public SimModule {
public:
    GpuInternalFabric(const std::string& name, EventQueue* eq);
    ~GpuInternalFabric() override = default;

    std::string get_module_type() const override {
        return "GpuInternalFabric";
    }

    // Inner crossbar reference (non-owning; ModuleFactory owns lifetime
    // when inner is instantiated via JSON child-modules path per Oracle
    // bg_c60653e0 §3.4).
    CrossbarTLM* crossbar() const noexcept { return crossbar_; }

    // Wire-up helper: attach inner crossbar. PT-PR-C will route this via
    // attach_composition() reading JSON children modules.
    void attach_crossbar(CrossbarTLM* inner) noexcept { crossbar_ = inner; }

    // Routing port count (default 4 matches CrossbarTLM::NUM_PORTS).
    void set_num_ports(unsigned n) noexcept { num_ports_ = n; }
    unsigned num_ports() const noexcept { return num_ports_; }

    // T25A.1 (Phase C 2026-10-07): JSON 装配路径自动 attach inner crossbar.
    // 覆写 SimModule::simulate_instantiate — 调基类 instantiateAll + 递归子 SimModule
    // 后, 遍历 cfg.modules 找名为 "crossbar" 的 CrossbarTLM child instance,
    // 调 attach_crossbar 激活 dormant gate (per handoff §9.2 #8 AI-fail-point).
    //
    // Per Oracle bg_c60653e0 §3.4: inner crossbar MUST instantiate via
    // ModuleFactory JSON child path (NOT raw `new`), 保证 Step 7 StreamAdapter
    // 注入不会跳过。
    //
    // idempotent: 若 crossbar_ 已非空 (T22b 直接 C++ attach_crossbar 已调),
    // 不覆盖 (T22b 测试兼容性)。
    void simulate_instantiate(const nlohmann::json& cfg) override;

private:
    CrossbarTLM* crossbar_ = nullptr;  // non-owning (ModuleFactory owns)
    unsigned num_ports_ = 4;
};

} // namespace tlm::cluster

#endif // CPPTLM_GPU_INTERNAL_FABRIC_H