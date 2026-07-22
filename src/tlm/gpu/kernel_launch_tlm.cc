// src/tlm/gpu/kernel_launch_tlm.cc
// KernelLaunchTLM 实现 (Phase 8.A Task 4 + D1-Full P1 Phase 4 Wave 1)
// 作者 CppTLM Team / 日期 2026-06-24 (Phase 8.A) + 2026-07-16 (P0 扩展) + 2026-07-18 (Phase 4 P1)
#include "tlm/gpu/kernel_launch_tlm.hh"

// tick() 中 bridge_ 路径需要 MemoryBridge 完整定义 (synchronize_stream + poll)
#include "tlm/gpu/memory_bridge.hh"
// IPtxEmuDriver 接口 (AdvanceResult enum + advance/is_kernel_complete)
#include "tlm/gpu/ptx_emu_driver.hh"
// Phase 2a: PipelineTLM / ScoreboardTLM / TensorCoreTLM 注入
#include "tlm/gpu/pipeline_tlm.hh"
#include "tlm/gpu/scoreboard_tlm.hh"
#include "tlm/gpu/tensor_core_tlm.hh"

namespace tlm {

KernelLaunchTLM::KernelLaunchTLM(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq) {
}

void KernelLaunchTLM::tick() {
    if (bridge_ != nullptr) {
        // === D1-Full P1 PTX-EMU 驱动路径 ===
        // 1. 同步 stream 0 — 清掉已完成的 kernel
        bridge_->synchronize_stream(0);

        // 2. 通过 IPtxEmuDriver 推进 PTX-EMU 执行 (1:1 映射, 满足 G-D3 ≤1 cycle)
        if (driver_ != nullptr) {
            uint32_t actual_cycles = 0;

            // Phase 2a: 惰性注入 PipelineTLM / ScoreboardTLM / TensorCoreTLM
            // 在各 SM 的 exe_once 第一次执行前注入，确保 Step A/B/C
            // 三段式注入点收到非 nullptr 的 CppTLM 时序模型对象
            if (!tlm_objects_injected_) {
                uint32_t num_sms = driver_->num_sms();
                for (uint32_t sm_id = 0; sm_id < num_sms; ++sm_id) {
                    driver_->inject_scoreboard(sm_id,
                        std::make_unique<ScoreboardTLM>());
                    driver_->inject_pipeline(sm_id,
                        std::make_unique<PipelineTLM>());
                    driver_->inject_tensor_core(sm_id,
                        std::make_unique<TensorCoreTLM>());
                }
                tlm_objects_injected_ = true;
            }

            AdvanceResult result = driver_->advance(MAX_PTX_STEPS_PER_TICK, actual_cycles);

            switch (result) {
            case AdvanceResult::Error:
                // PTX-EMU 异常 — 记录日志并终止 tick
                fprintf(stderr, "[WARN] KernelLaunchTLM: PTX-EMU driver advance() returned Error\n");
                return;  // 不继续处理 pending queue

            case AdvanceResult::Executed:
            case AdvanceResult::KernelComplete:
                // 3. 检查 kernel 完成状态
                // 遍历 pending_ FIFO, 已完成项 pop
                while (!pending_.empty()) {
                    auto& front = pending_.front();
                    if (driver_->is_kernel_complete(front.kernel_id)) {
                        // MemoryBridge 通过 poll_kernel 自行跟踪完成状态,
                        // 无需 CppTLM 端显式调 mark_complete
                        pending_.pop_front();
                    } else {
                        break;  // FIFO 保序 — 后续 kernel 也一定未完成
                    }
                }
                break;

            case AdvanceResult::NoOp:
                // 无 pending work — kernel 未启动或全部完成
                break;
            }
        }
    } else {
        // === Phase 8.A 独立模式 (零回归, bridge_ 为空时退化为独立模拟) ===
        cycle_counter_++;
        if (interval_ > 0 && (cycle_counter_ % interval_) == 0) {
            kernels_launched_++;
        }
    }
}

}  // namespace tlm
