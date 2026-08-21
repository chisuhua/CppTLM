// src/tlm/gpu/cuda_core_adapter_mvp.cc
// CudaCoreAdapterMVP: SM 微架构探索器实现 (per S1 / T-s1-4 Phase I.2)
//
// 编译防火墙豁免: 此 .cc 是除 src/tlm/gpu/ptx_emu_submodule_mvp.cc 外
// **唯一**允许 include PTX-EMU 头 (`ptxsim/*.h`) 的 CppTLM .cc — 因为
// tick() 必须驱动 SMContext::exe_once() (timing API, facade 按职责不暴露),
// inject_timing_modules() 必须调 SMContext::set_scoreboard() 等 setter。
// 其余 CppTLM 代码只见 cuda_core_adapter_mvp.hh 的前向声明。
//
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-4, Sisyphus)

#include "tlm/gpu/cuda_core_adapter_mvp.hh"

// =============================================================================
// PTX-EMU headers — 编译防火墙豁免 (见文件头注释)
// =============================================================================
#include "ptxsim/execution_types.h"  // EXE_STATE { IDLE, RUN, EXIT, BAR_SYNC }
#include "ptxsim/gpu_context.h"      // GPUContext (delete 需完整类型)
#include "ptxsim/sm_context.h"       // SMContext::exe_once / reserve_resources / set_*
#include "ptxsim/thread_context.h"   // ThreadContext::get_state
#include "ptxsim/warp_context.h"     // WarpContext::get_thread

// CppTLM timing 模块 (owned 实例的具体类型)
#include "tlm/gpu/minimal_warp_scheduler_tlm.hh"
#include "tlm/gpu/pipeline_tlm.hh"
#include "tlm/gpu/scoreboard_tlm.hh"
#include "tlm/gpu/tensor_core_tlm.hh"

namespace tlm {

namespace {

/// PTX-EMU EXE_STATE → WarpStateMirror::scheduler_state 映射
/// (镜像约定: 0=IDLE, 1=RUN, 2=BAR_SYNC, 3=EXIT; 与 EXE_STATE 枚举值
///  顺序不同 — EXIT/BAR_SYNC 对调, 故必须显式映射而非 static_cast)
int map_scheduler_state(EXE_STATE s) {
    switch (s) {
        case IDLE:     return 0;
        case RUN:      return 1;
        case BAR_SYNC: return 2;
        case EXIT:     return 3;
        default:       return 0;
    }
}

/// 读取 warp 的 scheduler_state: 扫描 lanes, 首个非 IDLE 的 lane 状态
/// 表征整个 warp (与 facade functional_execute_warp 的状态归约一致)。
int read_scheduler_state(const WarpContext* warp) {
    for (int i = 0; i < WarpContext::WARP_SIZE; ++i) {
        const ThreadContext* t = warp->get_thread(i);
        if (t) {
            EXE_STATE s = t->get_state();
            if (s != IDLE) return map_scheduler_state(s);
        }
    }
    return 0;  // IDLE
}

}  // namespace

// =============================================================================
// ctor / dtor
// =============================================================================
CudaCoreAdapterMVP::CudaCoreAdapterMVP() = default;

CudaCoreAdapterMVP::~CudaCoreAdapterMVP() {
    // GPUContext 完整类型仅在此 .cc 可见 — delete 必须在此处
    delete gpu_;
    gpu_ = nullptr;
}

// =============================================================================
// init — 绑定 facade + 创建 owned GPUContext
// =============================================================================
void CudaCoreAdapterMVP::init(PtxEmuSubmoduleMVP& facade) {
    facade_ = &facade;
    if (!facade_->is_initialized()) {
        // MVP: 默认 GPUConfig (1 SM / 64 warps / 64KB shared), 空 root
        facade_->init("", GPUConfig{});
    }
    if (!gpu_) {
        gpu_ = facade_->create_gpu_context();
    }
}

// =============================================================================
// inject_timing_modules — 一次性注入 (per FIX-H8/B.2), 幂等
// =============================================================================
void CudaCoreAdapterMVP::inject_timing_modules() {
    if (timing_injected_) return;

    // 1. 创建 4 个 owned timing 实例
    //    MinimalWarpSchedulerTLM 是 CppTLM ChStreamModuleBase 派生 (非 PTX-EMU
    //    WarpScheduler), 不注入 SM — 由 adapter 自持做 per-cycle 调度探索。
    //    EventQueue 传 nullptr: 本适配器不驱动其 tick(), 仅用调度查询接口。
    warp_scheduler_ =
        std::make_unique<MinimalWarpSchedulerTLM>("cuda_core_warp_scheduler", nullptr);
    scoreboard_ = std::make_unique<ScoreboardTLM>();
    pipeline_ = std::make_unique<PipelineTLM>();
    tensor_core_ = std::make_unique<TensorCoreTLM>();

    // 2. 经 SMContext::set_*() 注入 SM 0 (SM 持 non-owning raw 指针,
    //    nullptr = byte-identical fallback, per ADR-0020)
    if (facade_ && gpu_) {
        if (SMContext* sm = facade_->get_sm_context(*gpu_, 0)) {
            sm->set_scoreboard(scoreboard_.get());
            sm->set_pipeline_latency_provider(pipeline_.get());
            sm->set_tensor_core_timing(tensor_core_.get());
        }
    }
    timing_injected_ = true;
}

// =============================================================================
// on_cta_arrival — SM 资源反压 + PTX IR 解码提交
// =============================================================================
bool CudaCoreAdapterMVP::on_cta_arrival(const CtaDescriptor& cta) {
    if (!facade_ || !gpu_) return false;
    SMContext* sm = facade_->get_sm_context(*gpu_, 0);
    if (!sm) return false;

    // SM 资源反压: shared memory / warp 槽位不足则拒绝 (上层应排队)
    if (!sm->reserve_resources(cta.shared_mem_size, cta.warp_count)) {
        return false;
    }

    // PTX IR 解码 + kernel 提交 (经 facade 转发; MVP 阶段 submit 为占位,
    // 不以 submit 返回值阻断 CTA 接收 — 资源预留成功即算 accepted)
    if (!cta.ptxir_bytes.empty()) {
        std::vector<StatementContext> statements =
            facade_->decode_ptxir(cta.ptxir_bytes);
        facade_->submit_kernel_request(*gpu_, statements);
    }
    return true;
}

// =============================================================================
// on_warp_complete — 完成回调 (计数 + 状态记录)
// =============================================================================
void CudaCoreAdapterMVP::on_warp_complete(uint64_t /*task_id*/, int32_t status) {
    ++completed_warp_count_;
    last_completion_status_ = status;
}

// =============================================================================
// tick — ★ timing 主入口, 驱动 sm->exe_once() (PTX-EMU 内部 3-Step 注入)
// =============================================================================
void CudaCoreAdapterMVP::tick() {
    if (!facade_ || !gpu_) return;
    SMContext* sm = facade_->get_sm_context(*gpu_, 0);
    if (!sm) return;
    // exe_once: cycle_counter_++ → (RUN 态时) blocked decrement → warp 调度
    // → 指令 dispatch (scoreboard allocate / pipeline latency / release)
    sm->exe_once();
}

// =============================================================================
// get_warp_state — WarpState 镜像 (timing only, **不**含 PC)
// =============================================================================
CudaCoreAdapterMVP::WarpStateMirror
CudaCoreAdapterMVP::get_warp_state(WarpContext* warp) const {
    WarpStateMirror mirror{};

    // cycle_count 来自 SM (权威 timing 计数器), 与 warp 无关
    if (facade_ && gpu_) {
        if (SMContext* sm = facade_->get_sm_context(*gpu_, 0)) {
            mirror.cycle_count = sm->get_cycle_count();
        }
    }

    if (facade_ && warp) {
        // exec_mask / blocked_cycles 经 facade (WarpState 权威源, FIX-H8/B.3)
        mirror.exec_mask = facade_->read_active_mask(warp);
        mirror.blocked_cycles = facade_->read_blocked_cycles(warp, /*lane_id=*/0);
        mirror.scheduler_state = read_scheduler_state(warp);
    }
    return mirror;
}

}  // namespace tlm
