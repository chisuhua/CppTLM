// include/tlm/gpu/cuda_core_adapter_mvp.hh
// CudaCoreAdapterMVP: SM 微架构探索器 (timing adapter, per S1 / T-s1-4 Phase I.2)
// 功能: 持有 PtxEmuSubmoduleMVP facade 引用 (非 owning), 经 facade 驱动
//       PTX-EMU SM timing 路径 (sm->exe_once), 注入 4 个 CppTLM timing 模块
//       (MinimalWarpSchedulerTLM / ScoreboardTLM / PipelineTLM / TensorCoreTLM),
//       并向 SM 层提供 CTA 反压 (on_cta_arrival) 与 warp 完成回调。
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-4, Sisyphus)
// 参考:
//   - openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/design.md §1/§2.2
//   - openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/tasks.md T-s1-4
//
// 编译防火墙: 本 header **不** include 任何 PTX-EMU 头 — 仅前向声明 +
//             facade 头 (ptx_emu_submodule_mvp.hh)。PTX-EMU 头仅允许出现在
//             src/tlm/gpu/ptx_emu_submodule_mvp.cc 与
//             src/tlm/gpu/cuda_core_adapter_mvp.cc。
//
// functional/timing 严格分离 (per gpgpu-sim 分层):
//   - 本类是 timing 侧: tick() 驱动 cycle 推进, WarpStateMirror 只含 timing
//     状态 (cycle_count / exec_mask / blocked_cycles / scheduler_state),
//     **不含 PC** (PC 属 functional 侧, per R4)。
//   - 不直接调 PTX-EMU functional 接口 — functional 读写走 facade。
#ifndef TLM_GPU_CUDA_CORE_ADAPTER_MVP_HH
#define TLM_GPU_CUDA_CORE_ADAPTER_MVP_HH

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

// facade 头: 提供 PtxEmuSubmoduleMVP 完整声明 + GPUContext/WarpContext 前向声明
// (facade 头自身只前向声明 PTX-EMU 内部类, 不破坏防火墙)
#include "tlm/gpu/ptx_emu_submodule_mvp.hh"

namespace tlm {

// 前向声明 — 4 个 CppTLM timing 模块 (定义在 include/tlm/gpu/*.hh)
class MinimalWarpSchedulerTLM;
class ScoreboardTLM;
class PipelineTLM;
class TensorCoreTLM;

/// CudaCoreAdapterMVP — SM 微架构探索器 (timing 主入口)
///
/// 生命周期:
///   1. init(facade)            — 绑定 facade + 创建 owned GPUContext
///   2. inject_timing_modules() — 一次性创建 4 个 timing 实例并 set_*() 到 SM
///                                (per FIX-H8/B.2; SM 持有 non-owning raw 指针)
///   3. on_cta_arrival()        — CTA 到达, SM 资源反压 + PTX IR 解码提交
///   4. tick()                  — 每周期驱动 sm->exe_once()
///   5. on_warp_complete()      — warp 完成回调 (计数 + 状态记录)
class CudaCoreAdapterMVP {
public:
    /// CTA 描述符 — 纯 CppTLM 值类型 (无 PTX-EMU 依赖)
    struct CtaDescriptor {
        std::vector<uint8_t> ptxir_bytes;  // PTX IR 镜像 (空 = 仅资源预留)
        size_t shared_mem_size = 0;        // 本 CTA 请求的 shared memory (bytes)
        int warp_count = 1;                // 本 CTA 的 warp 数
        uint64_t task_id = 0;              // 上层任务标识 (日志/回调用)
    };

    /// WarpState 镜像 (timing only, **不**含 PC, per Phase I.2 §3 / R4)
    /// scheduler_state 映射: 0=IDLE, 1=RUN, 2=BAR_SYNC, 3=EXIT
    struct WarpStateMirror {
        uint64_t cycle_count = 0;
        uint32_t exec_mask = 0;
        uint32_t blocked_cycles = 0;
        int scheduler_state = 0;
    };

    CudaCoreAdapterMVP();
    ~CudaCoreAdapterMVP();  // .cc 定义 (unique_ptr 不完整类型 + delete GPUContext)

    CudaCoreAdapterMVP(const CudaCoreAdapterMVP&) = delete;
    CudaCoreAdapterMVP& operator=(const CudaCoreAdapterMVP&) = delete;

    /// 绑定 facade (非 owning) 并初始化: facade 未初始化则以默认 GPUConfig
    /// 初始化, 然后创建 adapter-owned GPUContext (生命周期归本对象)。
    void init(PtxEmuSubmoduleMVP& facade);

    /// 一次性 timing 模块注入 (per FIX-H8/B.2): 创建 4 个实例,
    /// 并把 scoreboard/pipeline/tensor_core 经 SMContext::set_*() 注入 SM 0。
    /// 幂等 — 重复调用不重建实例。
    void inject_timing_modules();

    /// CTA 到达处理 (SM 资源反压入口)
    /// @return false = 资源不足 (shared_mem 或 warp 数超限), CTA 应排队等待
    bool on_cta_arrival(const CtaDescriptor& cta);

    /// warp 完成回调 — 记录计数与最近完成状态
    void on_warp_complete(uint64_t task_id, int32_t status);

    /// ★ timing 主入口 — 驱动 sm->exe_once() (PTX-EMU 内部 3-Step 注入:
    /// scoreboard->allocate / pipeline->get_fractional_cycles / scoreboard->release)
    void tick();

    /// WarpState 镜像查询 (timing only, 经 facade 读 exec_mask/blocked_cycles;
    /// warp == nullptr 时仅返回 SM cycle_count, 其余字段为 0)
    WarpStateMirror get_warp_state(WarpContext* warp) const;

    // =========================================================================
    // 测试观测口 (read-only)
    // =========================================================================
    GPUContext* gpu_context() const { return gpu_; }
    MinimalWarpSchedulerTLM* warp_scheduler() const { return warp_scheduler_.get(); }
    ScoreboardTLM* scoreboard() const { return scoreboard_.get(); }
    PipelineTLM* pipeline() const { return pipeline_.get(); }
    TensorCoreTLM* tensor_core() const { return tensor_core_.get(); }
    bool timing_injected() const { return timing_injected_; }
    uint64_t completed_warp_count() const { return completed_warp_count_; }
    int32_t last_completion_status() const { return last_completion_status_; }

private:
    PtxEmuSubmoduleMVP* facade_ = nullptr;  // 非 owning
    GPUContext* gpu_ = nullptr;             // owning (delete 于 .cc, 完整类型仅 .cc 可见)

    // Owned timing 模块实例 (SM 侧持 non-owning raw 指针, 生命周期归本对象)
    std::unique_ptr<MinimalWarpSchedulerTLM> warp_scheduler_;
    std::unique_ptr<ScoreboardTLM> scoreboard_;
    std::unique_ptr<PipelineTLM> pipeline_;
    std::unique_ptr<TensorCoreTLM> tensor_core_;

    bool timing_injected_ = false;
    uint64_t completed_warp_count_ = 0;
    int32_t last_completion_status_ = 0;
};

}  // namespace tlm

#endif  // TLM_GPU_CUDA_CORE_ADAPTER_MVP_HH
