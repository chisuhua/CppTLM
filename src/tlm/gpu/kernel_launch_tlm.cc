// src/tlm/gpu/kernel_launch_tlm.cc
// KernelLaunchTLM 实现 (Phase 8.A Task 4 + D1-Full P0 PTX-EMU 驱动)
// 作者 CppTLM Team / 日期 2026-06-24 (Phase 8.A) + 2026-07-16 (P0 扩展)
#include "tlm/gpu/kernel_launch_tlm.hh"

// tick() 中 bridge_ 路径需要 MemoryBridge 完整定义 (synchronize_stream + poll)
#include "tlm/gpu/memory_bridge.hh"

namespace tlm {

KernelLaunchTLM::KernelLaunchTLM(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq) {
}

void KernelLaunchTLM::tick() {
    if (bridge_ != nullptr) {
        // === D1-Full P0 PTX-EMU 驱动路径 ===
        // 1. 同步 stream 0 — 清掉已完成的 kernel (P0 立即完成语义)
        bridge_->synchronize_stream(0);

        // 2. 推进 PTX-EMU 内部 warp 状态 (最多 MAX_PTX_STEPS_PER_TICK 次, 防死循环)
        //    当 pending_ 非空时连续调用 exe_once() 推进仿真
        //    (P0 stub: call_ptx_emu_exe_once_() 仅计数, 不真实驱动 PTX-EMU)
        uint32_t steps = 0;
        while (!pending_.empty() && steps < MAX_PTX_STEPS_PER_TICK) {
            call_ptx_emu_exe_once_();
            ++steps;
        }

        // 3. 检查 kernel 完成状态
        //    P0: poll_kernel 立即返回 0, pending_kernels_ 在首次 synchronize_stream 清空
        //    P1+: 遍历 pending_ FIFO 逐项 poll, 已完成项 pop

    } else {
        // === Phase 8.A 独立模式 (零回归, 不受 P0 扩展影响) ===
        cycle_counter_++;
        if (interval_ > 0 && (cycle_counter_ % interval_) == 0) {
            kernels_launched_++;
        }
    }
}

void KernelLaunchTLM::call_ptx_emu_exe_once_() {
    // P0 stub: 不真实驱动 PTX-EMU, 仅占位。
    // Phase 9+ 实现: 通过 set_ptx_emu_context() 传入的 handle 调用 PTX-EMU exe_once()
    //   auto* ctx = static_cast<GPUContext*>(ptx_emu_context_);
    //   ctx->exe_once(next_warp_id);
    // P0 阶段 pending_ kernel 立即通过 bridge_->synchronize_stream(0) 完成,
    // queue 将在 P1+ 真实 exe_once 驱动后才变空。
}

}  // namespace tlm