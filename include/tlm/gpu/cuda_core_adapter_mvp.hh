// include/tlm/gpu/cuda_core_adapter_mvp.hh
// CudaCoreAdapterMVP: SM microarchitecture explorer (HSK-8 Phase 2 timing adapter)
//
// 功能: 持有 PtxEmuSubmoduleMVP facade 引用 (非 owning), 经 facade 驱动
//       PTX-EMU SM timing 路径 (device->sm_exe_once + attach_timing),
//       并向 SM 层提供 CTA 反压 (on_cta_arrival) 与 warp 完成回调。
//
// HSK-8 Phase 2 重构要点:
//   - 移除 GPUContext*/SMContext*/WarpContext* 完整类型指针依赖
//   - 通过 IPtxEmuDevice sm_exe_once(sm_id) 替换 sm->exe_once()
//   - 通过 IPtxEmuDevice attach_timing(sb, pl, tc) 3→1 聚合替换 3 个 set_*()
//   - WarpStatus DTO 替换 warp->get_thread_state()/active_mask/blocked_cycles
//   - PTX-EMU 内部类型 (EXE_STATE/GPUContext/WarpContext) 完全不出现
//
// 作者 CppTLM Team / 2026-08-24 (HSK-8 Phase 2, Sisyphus)
// 参考:
//   - openspec/changes/cpptlm-ptxemu-public-device-api/{proposal,design}.md
//   - HSK-8 spec: docs/superpowers/specs/2026-08-21-hsk-8-ptxemu-public-api.md
//   - PTX-EMU device API: include/ptxemu/device_api.h (fcdad151)
#ifndef TLM_GPU_CUDA_CORE_ADAPTER_MVP_HH
#define TLM_GPU_CUDA_CORE_ADAPTER_MVP_HH

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

#include "tlm/gpu/ptx_emu_submodule_mvp.hh"  // 提供 PtxEmuSubmoduleMVP + WarpStatus 等 DTO

namespace tlm {

// 前向声明 — 4 个 CppTLM timing 模块 (定义在 include/tlm/gpu/*.hh)
class MinimalWarpSchedulerTLM;
class ScoreboardTLM;
class PipelineTLM;
class TensorCoreTLM;

/// CudaCoreAdapterMVP — SM 微架构探索器 (timing 主入口, HSK-8 Phase 2)
///
/// 生命周期:
///   1. init(facade)            — 绑定 facade (非 owning)
///   2. inject_timing_modules() — 一次性创建 4 个 timing 实例并
///                                facade->attach_timing() (3→1 聚合)
///   3. on_cta_arrival()        — CTA 到达, 资源反压入口
///   4. tick()                  — 每周期驱动 device->sm_exe_once()
///   5. on_warp_complete()      — warp 完成回调
class CudaCoreAdapterMVP {
public:
    /// CTA 描述符 — 纯 CppTLM 值类型 (无 PTX-EMU 依赖)
    struct CtaDescriptor {
        std::vector<uint8_t> ptxir_bytes;  // PTX IR 镜像 (空 = 仅资源预留)
        size_t shared_mem_size = 0;        // 本 CTA 请求的 shared memory (bytes)
        int warp_count = 1;                // 本 CTA 的 warp 数
        uint64_t task_id = 0;              // 上层任务标识
    };

    /// WarpState 镜像 (timing only, **不**含 PC)
    /// scheduler_state 映射: 0=kIdle, 1=kRun, 2=kBarSync, 3=kExit
    struct WarpStateMirror {
        uint64_t cycle_count = 0;
        uint32_t exec_mask = 0;
        int32_t blocked_cycles = 0;
        int scheduler_state = 0;
    };

    CudaCoreAdapterMVP();
    ~CudaCoreAdapterMVP();

    CudaCoreAdapterMVP(const CudaCoreAdapterMVP&) = delete;
    CudaCoreAdapterMVP& operator=(const CudaCoreAdapterMVP&) = delete;

    /// 绑定 facade (非 owning). 默认 SM 0 调度
    void init(PtxEmuSubmoduleMVP& facade);

    /// 一次性 timing 模块注入 (per FIX-H8/B.2): 创建 4 个实例,
    /// 经 facade->attach_timing() 3→1 聚合注入 device (HSK-4 接口).
    /// 幂等 — 重复调用不重建实例.
    void inject_timing_modules();

    /// CTA 到达处理 (SM 资源反压入口)
    bool on_cta_arrival(const CtaDescriptor& cta);

    /// warp 完成回调 — 记录计数与最近完成状态
    void on_warp_complete(uint64_t task_id, int32_t status);

    /// ★ timing 主入口 — 驱动 facade->sm_exe_once()
    void tick();

    /// WarpState 镜像查询 (timing only, 经 facade->get_warp_status 解析)
    WarpStateMirror get_warp_state(uint32_t sm_id, uint32_t warp_id) const;

    // =========================================================================
    // 测试观测口 (read-only)
    // =========================================================================
    MinimalWarpSchedulerTLM* warp_scheduler() const { return warp_scheduler_.get(); }
    ScoreboardTLM* scoreboard() const { return scoreboard_.get(); }
    PipelineTLM* pipeline() const { return pipeline_.get(); }
    TensorCoreTLM* tensor_core() const { return tensor_core_.get(); }
    bool timing_injected() const { return timing_injected_; }
    uint64_t completed_warp_count() const { return completed_warp_count_; }
    int32_t last_completion_status() const { return last_completion_status_; }

private:
    PtxEmuSubmoduleMVP* facade_ = nullptr;  // 非 owning

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