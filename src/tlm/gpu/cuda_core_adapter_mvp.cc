// src/tlm/gpu/cuda_core_adapter_mvp.cc
// CudaCoreAdapterMVP: SM 微架构探索器实现 (HSK-8 Phase 2)
//
// 编译防火墙: 与 facade.cc 一致, 仅 include PTX-EMU 公共头
// (`ptxemu/device_api.h`). PTX-EMU 内部头 (`ptxsim/*.h`/`ptx_ir/*.h`/
// `memory/*.h`/`register/*.h`) 完全封装在 PTX-EMU 端 .a 静态库内.
//
// HSK-8 Phase 2 重构:
//   - tick() 改为 facade->sm_exe_once(0) (1:1 替换 sm->exe_once)
//   - inject_timing_modules() 改为 facade->attach_timing(sb, pl, tc) 3→1 聚合
//   - WarpState 镜像改为 facade->get_warp_status(sm_id, warp_id) 解析
//
// 作者 CppTLM Team / 2026-08-24 (HSK-8 Phase 2, Sisyphus)

#include "tlm/gpu/cuda_core_adapter_mvp.hh"

// HSK-8 Phase 2: 仅 include PTX-EMU 公共头 (内部头不可见)
#include "ptxemu/device_api.h"

// CppTLM timing 模块 (具体实现)
#include "tlm/gpu/minimal_warp_scheduler_tlm.hh"
#include "tlm/gpu/pipeline_tlm.hh"
#include "tlm/gpu/scoreboard_tlm.hh"
#include "tlm/gpu/tensor_core_tlm.hh"

namespace tlm {

// =============================================================================
// RAII
// =============================================================================
CudaCoreAdapterMVP::CudaCoreAdapterMVP() = default;

CudaCoreAdapterMVP::~CudaCoreAdapterMVP() = default;

// =============================================================================
// init
// =============================================================================
void CudaCoreAdapterMVP::init(PtxEmuSubmoduleMVP& facade) {
    if (facade_ == &facade && timing_injected_) {
        return;  // 同 facade 已注入, 幂等 no-op (保留已创建模块)
    }
    facade_ = &facade;
    timing_injected_ = false;
    // init-time injection per D1-Full tasks.md §1.5.1 (避免 caller 遗漏)
    inject_timing_modules();
}

// =============================================================================
// inject_timing_modules (HSK-8 Phase 2: 3→1 聚合)
// =============================================================================
void CudaCoreAdapterMVP::inject_timing_modules() {
    if (timing_injected_) return;
    if (!facade_) return;

    // 1. 创建 4 个 owned timing 模块实例
    warp_scheduler_ = std::make_unique<MinimalWarpSchedulerTLM>(
        "warp_scheduler", /*EventQueue=*/nullptr);
    scoreboard_     = std::make_unique<ScoreboardTLM>();
    pipeline_       = std::make_unique<PipelineTLM>();
    tensor_core_    = std::make_unique<TensorCoreTLM>();

    // 2. 3→1 聚合注入 (HSK-8 attach_timing)
    //    HSK-4 跨仓 ABI 锁定 (pt:pc-3), CppTLM 端具体实现接口与
    //    PTX-EMU forward decl 二进制兼容.
    facade_->attach_timing(
        static_cast<void*>(scoreboard_.get()),
        static_cast<void*>(pipeline_.get()),
        static_cast<void*>(tensor_core_.get()));

    timing_injected_ = true;
}

// =============================================================================
// CTA 反压入口
// =============================================================================
bool CudaCoreAdapterMVP::on_cta_arrival(const CtaDescriptor& cta) {
    if (!facade_) return false;
    // MVP 占位: 简化资源反压检查 (shared_mem + warp_count);
    // 真实实现需 IPtxEmuDevice 端补充 reserve_resources(sm, shared_mem, warps).
    if (cta.shared_mem_size > 48ULL * 1024) return false;  // 单 SM shared mem 上限 (HSK-8 默认)
    if (cta.warp_count > 64) return false;  // 单 SM warp 上限 (HSK-8 默认)
    return true;
}

// =============================================================================
// warp 完成回调
// =============================================================================
void CudaCoreAdapterMVP::on_warp_complete(uint64_t task_id, int32_t status) {
    ++completed_warp_count_;
    last_completion_status_ = status;
    (void)task_id;  // MVP 阶段未使用
}

// =============================================================================
// tick (★ timing 主入口)
// =============================================================================
void CudaCoreAdapterMVP::tick() {
    if (!facade_) return;
    // HSK-8 Phase 2: 直接调 device->sm_exe_once(0) (默认 SM 0)
    facade_->sm_exe_once(0);
}

// =============================================================================
// WarpState 镜像 (timing only, 经 facade->get_warp_status 解析)
// =============================================================================
CudaCoreAdapterMVP::WarpStateMirror CudaCoreAdapterMVP::get_warp_state(
    uint32_t sm_id, uint32_t warp_id) const {
    WarpStateMirror mirror{};
    if (!facade_) return mirror;
    WarpStatus ws = facade_->get_warp_status(sm_id, warp_id);
    mirror.exec_mask = facade_->get_active_mask_from_warp_status(sm_id, warp_id);
    // lane[0].state 推断 warp 整体 scheduler_state (任一活跃 lane 即可表征)
    if (!ws.lanes.empty()) {
        ThreadState s = ws.lanes[0].state;
        mirror.scheduler_state = (s == ThreadState::kRun) ? 1 :
                                 (s == ThreadState::kBarSync) ? 2 :
                                 (s == ThreadState::kExit) ? 3 : 0;
        mirror.blocked_cycles = (s == ThreadState::kBarSync) ? 1 : 0;
    }
    mirror.cycle_count = static_cast<uint64_t>(ws.blocked_cycles);  // 近似
    return mirror;
}

}  // namespace tlm