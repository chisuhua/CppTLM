// src/tlm/gpu/kernel_launch_tlm.cc
// KernelLaunchTLM 实现 (Phase 8.A Task 4 + D1-Full P1 Phase 4 Wave 1)
// 作者 CppTLM Team / 日期 2026-06-24 (Phase 8.A) + 2026-07-16 (P0 扩展) + 2026-07-18 (Phase 4 P1)
#include "tlm/gpu/kernel_launch_tlm.hh"

// HSK-8 Phase 2 Step 4: ptx_emu_driver.hh + memory_bridge.hh 已物理删除 (HSK-6 deprecate by
// 369cf71). Phase 2a: PipelineTLM / ScoreboardTLM / TensorCoreTLM 注入
#include "tlm/gpu/pipeline_tlm.hh"
#include "tlm/gpu/scoreboard_tlm.hh"
#include "tlm/gpu/tensor_core_tlm.hh"

namespace tlm {

    KernelLaunchTLM::KernelLaunchTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {
    }

    void KernelLaunchTLM::tick() {
        // HSK-8 Phase 2 Step 4: 仅保留 Phase 8.A 独立模式 (bridge_/driver_ P0/P1 路径已废弃).
        cycle_counter_++;
        if (interval_ > 0 && (cycle_counter_ % interval_) == 0) {
            kernels_launched_++;
        }
    }

} // namespace tlm
