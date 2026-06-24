// src/tlm/gpu/kernel_launch_tlm.cc
// KernelLaunchTLM 实现 (Phase 8.A Task 4)
// 作者 CppTLM Team / 日期 2026-06-24
#include "tlm/gpu/kernel_launch_tlm.hh"

namespace tlm {

KernelLaunchTLM::KernelLaunchTLM(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq) {}

void KernelLaunchTLM::tick() {
    cycle_counter_++;
    // Phase 8.A 简化模型: 每 interval_ cycles launch 1 个 kernel
    // 真实 AQL dispatch 在 Phase 9+ (per ADR-NV-01 §10)
    if (interval_ > 0 && (cycle_counter_ % interval_) == 0) {
        kernels_launched_++;
        // 简化: 不实际发送 KernelDesc 到端口 (Phase 8.B 完整实现)
    }
}

}  // namespace tlm