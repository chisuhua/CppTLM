# cuda-core-adapter 微架构文档(per Phase I.2 重构)

> **类别**: GPU > Cuda Core Adapter (SM **Microarchitecture Exploration**) · **状态**: 🔵 MVP 切片 (per ADR-SOC-06)
> **Header**: `include/tlm/gpu/cuda_core_adapter_mvp.hh`
> **位置**: **DGpuBoardTLM 内部组件**(`dgpu-board.md` §3 已作为 `CudaCoreAdapter cuda_core_` 私有成员固定);独立 ChStreamModuleBase 暴露模式推到 v0.5 完整版
> **蓝图来源**: gpgpu-sim `gpgpu-sim/shader_core/`(timing model 层)+ PTX-EMU `SMContext::exe_once` 3-Step 注入机制 + 现有 `ScoreboardTLM` / `PipelineTLM` / `TensorCoreTLM` / `MinimalWarpSchedulerTLM`
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> **关联 ADR**: [`ADR-SOC-06-cpptlm-v05-mvp`](../../soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5
> **关联模块**: [`ptx-emu-submodule-mvp.md`](./ptx-emu-submodule-mvp.md)(PTX functional facade,与本模块严格分离)| [`tmu-dispatch-processor.md`](./tmu-dispatch-processor.md) · [`submit-queue.md`](./submit-queue.md)
> **首版 commit**: 🔵 W3-4 实施(基础)+ W5-6 升级(timing 微架构)· **最近更新**: 2026-08-20(Phase I 重构)
> **维护者**: CppTLM Team (Sisyphus)

> **关联调研**: gpgpu-sim `gpgpu-sim/shader_core/`(timing model 模块) + PTX-EMU `ptxsim/sm_context.h:59 exe_once` 3-Step 注入机制。架构参照:本模块是 timing 微架构模拟器,不关心单条 PTX 指令计算什么;`PtxEmuSubmoduleMVP` 才是 functional 责任人。

---

## 1. 设计目标(per Phase I.2 重构)

`CudaCoreAdapter` 是 **SM 微架构探索器**(timing model),严格遵循 **gpgpu-sim functional/timing 分离原则**:

| 维度 | 本模块(CudaCoreAdapter) | PtxEmuSubmoduleMVP(对偶模块) |
|------|------------------------|-----------------------------|
| **职责** | SM **微架构行为**(timing/pipeline/调度) | PTX 指令**功能正确性** |
| **关心** | cycle 推进、warp 调度、scoreboard hazard、pipeline latency、TC timing | 寄存器值、内存值、PC 推进、SIMT 分支、barrier sync |
| **不关心** | 单条指令具体计算什么 | cycle 数、stall 原因、调度策略、流水线延迟 |
| **API 调用** | `tick()` 驱动 `sm->exe_once()` 推进 cycle | `functional_execute_warp(warp, stmt, pc)` |
| **类比 gpgpu-sim** | `gpgpu-sim/shader_core/`(timing) | `cuda-sim/`(functional) |

### 1.1 微架构组件清单(per Phase I.2)

本模块集成 **5 个 timing 组件**:

| 组件 | 作用 | 来源 |
|------|------|------|
| `MinimalWarpSchedulerTLM` | per-cycle 选哪个 warp issue | `include/tlm/gpu/minimal_warp_scheduler_tlm.hh`(已存在) |
| `ScoreboardTLM` | RAW hazard 跟踪(`IScoreboard` 实现) | `include/tlm/gpu/scoreboard_tlm.hh`(已存在) |
| `PipelineTLM` | pipeline latency 注入(`IPipelineLatencyProvider` 实现) | `include/tlm/gpu/pipeline_tlm.hh`(已存在) |
| `TensorCoreTLM` | TC 延迟注入(`ITensorCoreTiming` 实现) | `include/tlm/gpu/tensor_core_tlm.hh`(已存在) |
| `WarpState[warp_id]` | per-warp 微架构状态镜像(cycle/exec_mask/blocked_cycles/scheduler_state)| 本模块定义 |

### 1.2 职责严格分离(per Phase I.2)

**本模块(CudaCoreAdapter)负责**:
1. ❌ **不**调 PTX-EMU internal 直接接口(都通过 PtxEmuSubmoduleMVP 转发)
2. ✅ 调 `PtxEmuSubmoduleMVP::functional_execute_warp()` 触发 functional 计算(由 PtxEmuSubmoduleMVP 保证正确性)
3. ✅ per-tick `tick()` 推进 `sm->exe_once()`(timing model 主入口)
4. ✅ 持有 `MinimalWarpSchedulerTLM`(warp 调度策略)
5. ✅ 持有 `ScoreboardTLM`(RAW hazard 检查 + 释放)
6. ✅ 持有 `PipelineTLM`(流水线延迟查询)
7. ✅ 持有 `TensorCoreTLM`(TC 指令延迟)
8. ✅ 镜像 `WarpState[warp_id]`(cycle / exec_mask / blocked_cycles / scheduler_state)— **不含 PC**(PC 由 PtxEmuSubmoduleMVP 负责)
9. ✅ 接管 SM 资源反压(reserve_resources / release_resources)
10. ✅ 完成检测 + 完成回调(→ SQ → TMU → CQ)

**本模块不负责**(由 PtxEmuSubmoduleMVP 处理):
1. ❌ PTX 指令功能正确性(`functional_execute_warp` 内核)
2. ❌ 寄存器值/内存值的读写 API(由 PtxEmuSubmoduleMVP::read_register/write_register 处理)
3. ❌ PC 推进(由 PtxEmuSubmoduleMVP::advance_thread_pc 处理)
4. ❌ exec mask 读写(由 PtxEmuSubmoduleMVP::read_active_mask 处理)
5. ❌ PTX IR 解码(由 PtxEmuSubmoduleMVP::decode_ptxir 处理)

### 1.3 与 PTX-EMU 注入点对接

per PTX-EMU `sm_context.h:87-95`,SMContext 接受 3 个注入:
```cpp
void set_scoreboard(IScoreboard* scoreboard);
void set_pipeline_latency_provider(IPipelineLatencyProvider* provider);
void set_tensor_core_timing(ITensorCoreTiming* tc);
void set_warp_scheduler(std::unique_ptr<WarpScheduler> scheduler);
```

本模块在 `init()` 时**通过 PtxEmuSubmoduleMVP 创建这些注入对象**:
- `scoreboard_ = std::make_unique<ScoreboardTLM>()`(IScoreboard 实现,`include/tlm/gpu/scoreboard_tlm.hh`)
- `pipeline_provider_ = std::make_unique<PipelineTLM>()`(IPipelineLatencyProvider 实现,`include/tlm/gpu/pipeline_tlm.hh`)
- `tensor_core_timing_ = std::make_unique<TensorCoreTLM>()`(ITensorCoreTiming 实现,`include/tlm/gpu/tensor_core_tlm.hh`)
- `warp_scheduler = std::make_unique<MinimalWarpSchedulerTLM>(...)`

然后**注入到 PTX-EMU SMContext**(一次性,在 `init()` 完成)。

---

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────────────┐
│ SubmitQueue::on_warp_complete(task_id, status)                     │
│   ├─ 推送完成到 TmuDispatchProcessor::on_complete                 │
│   └─ 释放 CudaCore in_flight 槽位                                  │
└─────────────────────────────────────────────────────────────────────┘
        │
        ▼ 反压/调度
┌─────────────────────────────────────────────────────────────────────┐
│ CudaCoreAdapter::tick()(★ 微架构 timing 主入口)                   │
│                                                                      │
│   ┌─────────────────────────────────────────────────────────┐    │
│   │ 1. WarpScheduler 调度 (per cycle)                         │    │
│   │    MinimalWarpSchedulerTLM::schedule_next() → next_warp  │    │
│   └─────────────────────────────────────────────────────────┘    │
│                                                                      │
│   ┌─────────────────────────────────────────────────────────┐    │
│   │ 2. SM cycle 推进(★ timing 核心)                          │    │
│   │    sm->exe_once()                                          │    │
│   │      ├─ cycle_counter_++                                  │    │
│   │      ├─ WarpScheduler 选 next_warp                         │    │
│   │      ├─ Step A: ScoreboardTLM.allocate() — RAW hazard?    │    │
│   │      │      └─ false → skip (warp stall)                  │    │
│   │      ├─ ★ PtxEmuSubmoduleMVP.functional_execute_warp()    │    │
│   │      │      → PTX-EMU WarpContext::execute_warp_instruction│    │
│   │      │      → 寄存器/内存/PC 按指令语义更新(FUNCTIONAL)   │    │
│   │      ├─ Step B: PipelineTLM.get_fractional_cycles()        │    │
│   │      │      → warp.set_blocked_cycles_for_active(N)        │    │
│   │      ├─ Step C: ScoreboardTLM.release()                   │    │
│   │      └─ TensorCoreTLM.get_latency() (TC 指令时)             │    │
│   └─────────────────────────────────────────────────────────┘    │
│                                                                      │
│   ┌─────────────────────────────────────────────────────────┐    │
│   │ 3. WarpState[warp_id] 镜像(本模块专有)                   │    │
│   │    for warp in 0..warp_count_:                             │    │
│   │      warp_states_[warp] = {                                │    │
│   │        cycle_count = sm->get_cycle_count(),                │    │
│   │        exec_mask = sm->get_warp(w)->get_active_mask(),     │    │
│   │        blocked_cycles = w->get_blocked_cycles_remaining(),│    │
│   │        scheduler_state = (last_scheduled_warp_id == warp)  │    │
│   │      }                                                      │    │
│   │    ⚠ 注意: WarpState 不含 PC (由 PtxEmuSubmoduleMVP 读)    │    │
│   └─────────────────────────────────────────────────────────┘    │
│                                                                      │
│   ┌─────────────────────────────────────────────────────────┐    │
│   │ 4. 完成检测                                                │    │
│   │    if (sm->is_idle() && last_warp_state_dirty_):          │    │
│   │      submit_queue_->on_warp_complete(task_id, 0)         │    │
│   │      sm->release_resources(reservation_id)               │    │
│   │      gpu_ctx_->clear_requests()                           │    │
│   │      last_warp_state_dirty_ = false                       │    │
│   └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
        │
        ▼ 反向流
┌─────────────────────────────────────────────────────────────────────┐
│ SubmitQueue::on_warp_complete → TmuDispatchProcessor::on_complete │
│                                → CompletionRing::push → host_notify│
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. WarpState 微架构状态镜像(本模块专有)

```cpp
struct WarpState {
    // === Timing 状态(本模块管)===
    uint64_t cycle_count = 0;          // SM cycle counter(单调递增)
    uint32_t exec_mask = 0;            // 32-lane 活跃掩码(per WarpContext::get_active_mask)
    uint32_t blocked_cycles = 0;       // 剩余阻塞 cycles(per pipeline latency)
    bool     scheduler_state = false;  // true = 本 cycle 被 scheduler 选中
    
    // === ❌ 本结构不含 === 
    // - pc: 由 PtxEmuSubmoduleMVP::read_thread_pc 读取
    // - 寄存器值: 由 PtxEmuSubmoduleMVP::read_register 读取
    // - 内存值: 由 PtxEmuSubmoduleMVP::read_global_memory 读取
};
```

**为什么 WarpState 不含 PC**:
- PC 是 **functional 状态**(指令语义的一部分),"PC=42 表示这个 warp 正在执行 PC=42 位置的 PTX 指令" — 这是指令逻辑关心的,不是 timing 关心的
- Timing model 只关心 "warp 何时被 issue"、"issue 后 stall 多久"、"什么时候能完成" — 这些都不需要看 PC
- 强行把 PC 放到 WarpState 会模糊 functional/timing 边界,违反 gpgpu-sim 分层原则

---

## 4. 接口(Public API)(per Phase I.2 重构)

```cpp
// include/tlm/gpu/cuda_core_adapter_mvp.hh
// 不 include PTX-EMU 头(只通过 PtxEmuSubmoduleMVP 转发)
#include "ptx_emu_submodule_mvp.hh"
#include "minimal_warp_scheduler_tlm.hh"  // WarpScheduler
#include <vector>

class PtxEmuSubmoduleMVP;
class MinimalWarpSchedulerTLM;

class CudaCoreAdapter {
public:
    // === Warp 微架构状态镜像(timing only,不含 PC)===
    struct WarpState {
        uint64_t cycle_count = 0;
        uint32_t exec_mask = 0;
        uint32_t blocked_cycles = 0;
        bool     scheduler_state = false;
    };

    /// CTA 描述符(从 SubmitQueue 接收,per submit-queue.md §3)
    struct CtaDescriptor {
        uint64_t vram_image_addr;
        size_t image_size;
        uint32_t grid_x, grid_y, grid_z;
        uint32_t block_x, block_y, block_z;
        size_t shared_mem_bytes;
        void** kernel_args;
        size_t args_count;
        uint64_t task_id;
        uint32_t cluster_id;
    };

    /// JSON 配置字段(per configs/dgpu_mvp_*.json)
    struct Config {
        uint32_t num_sms = 1;              // MVP 默认 1 SM
        uint32_t max_warps_per_sm = 64;
        size_t shared_mem_size = 64 * 1024;
        uint32_t registers_per_sm = 65536;
        uint32_t max_blocks_per_sm = 1;
        uint32_t warp_size = 32;
        // ✅ timing 注入开关
        bool enable_scoreboard = true;     // MVP 默认启用
        bool enable_pipeline_latency = true;
        bool enable_tensor_core_timing = true;
        uint32_t target_core_id = 0;
    };

    explicit CudaCoreAdapter(const Config& cfg, EventQueue* eq);
    ~CudaCoreAdapter();

    std::string get_module_type() const override { return "CudaCoreAdapter"; }

    /// 初始化(注入 PTX-EMU functional facade + timing 模块)
    /// @param ptx_emu_facade PTX functional facade(编译防火墙守护)
    void init(PtxEmuSubmoduleMVP& ptx_emu_facade);

    /// SubmitQueue 调用:接收 CTA 描述符
    /// @return true=成功接受,false=拒绝(SM 资源不足,反压停 dispatch)
    bool on_cta_arrival(const CtaDescriptor& cta);

    /// per-tick 推进(由 DGpuBoardTLM::tick() 调用)— ★ 微架构 timing 主入口 ★
    void tick();

    /// warp 完成回调(由 GPUContext::submit_kernel_request on_complete 触发)
    void on_warp_complete(uint64_t task_id, int32_t status);

    // === 测试/监控用(timing only)===
    WarpState warp_state(uint32_t warp_id) const;
    uint64_t total_cycle_count() const { return total_cycle_count_; }
    uint64_t ctas_completed() const { return ctas_completed_; }
    bool is_idle() const;
    uint64_t scoreboard_allocate_count() const;
    uint64_t scoreboard_release_count() const;
    uint64_t pipeline_latency_injected_count() const;

private:
    Config cfg_;
    EventQueue* eq_;
    PtxEmuSubmoduleMVP* ptx_emu_facade_ = nullptr;  // 转发 functional 调用

    // === Timing 组件(集成已有 TLM 模块)===
    std::unique_ptr<ptxsim::GPUContext> gpu_ctx_;       // 持有 SMContext 实例
    std::unique_ptr<MinimalWarpSchedulerTLM> warp_scheduler_;
    std::unique_ptr<ScoreboardTLM> scoreboard_;
    std::unique_ptr<PipelineTLM> pipeline_provider_;
    std::unique_ptr<TensorCoreTLM> tensor_core_timing_;

    // === 状态镜像(本模块管 timing only)===
    std::vector<WarpState> warp_states_;
    uint64_t current_task_id_ = UINT64_MAX;
    uint32_t warp_count_ = 0;
    uint64_t total_cycle_count_ = 0;
    uint64_t ctas_completed_ = 0;
    uint64_t ctas_failed_ = 0;
    bool last_warp_state_dirty_ = false;
    uint32_t last_scheduled_warp_id_ = UINT32_MAX;  // FIX-H8:声明缺失成员(per Phase I.2 §5.2)

    // === 注入到 PTX-EMU SMContext ===
    void inject_timing_modules(ptxsim::SMContext& sm);
};
```

---

## 5. 数据流(微架构 timing 视角)

### 5.1 init()(注入 timing 模块到 PTX-EMU)

```cpp
void CudaCoreAdapter::init(PtxEmuSubmoduleMVP& ptx_emu_facade) {
    ptx_emu_facade_ = &ptx_emu_facade;

    // 1. ★ 通过 facade 创建 GPUContext(facade 管 functional 构造)
    gpu_ctx_ = ptx_emu_facade_->create_gpu_context();

    // 2. 创建 timing 组件(MVP 默认全部启用)
    warp_scheduler_ = std::make_unique<MinimalWarpSchedulerTLM>(
        /* policy = */ MinimalWarpSchedulerTLM::Policy::ROUND_ROBIN);

    if (cfg_.enable_scoreboard) {
        scoreboard_ = std::make_unique<ScoreboardTLM>();
    }
    if (cfg_.enable_pipeline_latency) {
        pipeline_provider_ = std::make_unique<PipelineTLM>();
    }
    if (cfg_.enable_tensor_core_timing) {
        tensor_core_timing_ = std::make_unique<TensorCoreTLM>();
    }

    // 3. ★ 注入到 PTX-EMU SMContext(per sm_context.h:87-98)
    ptxsim::SMContext* sm = ptx_emu_facade_->get_sm_context(*gpu_ctx_, 0);
    inject_timing_modules(*sm);
}

void CudaCoreAdapter::inject_timing_modules(ptxsim::SMContext& sm) {
    // 注入 scoreboard(per sm_context.h:87)
    if (scoreboard_) {
        sm.set_scoreboard(scoreboard_.get());
    }
    // 注入 pipeline latency provider(per sm_context.h:90)
    if (pipeline_provider_) {
        sm.set_pipeline_latency_provider(pipeline_provider_.get());
    }
    // 注入 tensor core timing(per sm_context.h:93)
    if (tensor_core_timing_) {
        sm.set_tensor_core_timing(tensor_core_timing_.get());
    }
    // 注入 warp scheduler(per sm_context.h:83)
    if (warp_scheduler_) {
        sm.set_warp_scheduler(std::move(warp_scheduler_));
    }
}
```

### 5.2 tick()(per-cycle 微架构推进)

```cpp
void CudaCoreAdapter::tick() {
    if (!gpu_ctx_ || !ptx_emu_facade_) return;

    ptxsim::SMContext* sm = ptx_emu_facade_->get_sm_context(*gpu_ctx_, 0);

    // ★ Timing model 主入口:推进一个 SM cycle
    // sm->exe_once() 内部完成:
    //   1. cycle_counter_++
    //   2. WarpScheduler::schedule_next() → 选 next_warp
    //   3. Step A: scoreboard->allocate(reg_id, warp_id) — RAW hazard 检查
    //   4. (★ 通过 PtxEmuSubmoduleMVP 转发) functional_execute_warp()
    //   5. Step B: pipeline_provider->get_fractional_cycles() — 注入延迟
    //   6. Step C: scoreboard->release(reg_id, warp_id)
    sm->exe_once();

    // 镜像 WarpState(timing only,不含 PC)
    uint64_t now_cycle = sm->get_cycle_count();
    for (uint32_t i = 0; i < warp_count_; ++i) {
        ptxsim::WarpContext* w = ptx_emu_facade_->get_warp_context(*sm, i);
        if (!w) continue;
        warp_states_[i].cycle_count = now_cycle;
        warp_states_[i].exec_mask = ptx_emu_facade_->read_active_mask(*w);  // FIX-H8/B.3:经 facade 读
        warp_states_[i].blocked_cycles = ptx_emu_facade_->read_blocked_cycles(*w);  // FIX-H8/B.3:经 facade 读
        warp_states_[i].scheduler_state = (last_scheduled_warp_id_ == i);
        last_warp_state_dirty_ = true;
    }
    total_cycle_count_ = now_cycle;
}
```

### 5.3 on_cta_arrival()(SubmitQueue 调用)

```cpp
bool CudaCoreAdapter::on_cta_arrival(const CtaDescriptor& cta) {
    ptxsim::SMContext* sm = ptx_emu_facade_->get_sm_context(*gpu_ctx_, 0);
    int warp_count = (cta.block_x * cta.block_y * cta.block_z + 31) / 32;

    // ★ Timing model 资源管理
    if (!sm->reserve_resources(cta.shared_mem_bytes, warp_count)) {
        return false;  // 反压停 dispatch,SubmitQueue 应 back off
    }

    // ★ Functional 模型初始化(通过 facade,不直接调 PTX-EMU)
    //    - decode_ptxir(per PtxEmuSubmoduleMVP)
    //    - 构造 KernelLaunchRequest
    //    - submit_kernel_request(per PtxEmuSubmoduleMVP)
    const uint8_t* image_bytes = vram_.read(cta.vram_image_addr, cta.image_size);
    auto stmts = ptx_emu_facade_->decode_ptxir(image_bytes, cta.image_size);

    ptxsim::KernelLaunchRequest req {
        .args = cta.kernel_args,
        .gridDim = {cta.grid_x, cta.grid_y, cta.grid_z},
        .blockDim = {cta.block_x, cta.block_y, cta.block_z},
        .statements = &stmts,
        .name2Sym = ...,
        .label2pc = ...,
        .request_id = static_cast<int>(cta.task_id),
        .on_complete = [this, task_id = cta.task_id](int) {
            on_warp_complete(task_id, 0);
        },
        .shared_mem_size = cta.shared_mem_bytes
    };
    ptx_emu_facade_->submit_kernel_request(*gpu_ctx_, std::move(req));

    warp_count_ = warp_count;
    warp_states_.resize(warp_count);
    return true;
}
```

### 5.4 on_warp_complete()(GPUContext on_complete callback)

```cpp
void CudaCoreAdapter::on_warp_complete(uint64_t task_id, int32_t status) {
    if (status != 0) {
        ctas_failed_++;
        return;
    }
    ptxsim::SMContext* sm = ptx_emu_facade_->get_sm_context(*gpu_ctx_, 0);
    sm->release_resources(0);  // MVP 单 reservation

    // ★ 反向流: 提交到 SQ → TMU → CQ → host_notify
    submit_queue_->on_warp_complete(task_id, status);
    ctas_completed_++;
    last_warp_state_dirty_ = false;

    gpu_ctx_->clear_requests();
}
```

---

## 6. Timing vs Functional 分离矩阵(完整版)

| 操作 | CudaCoreAdapter(timing)负责 | PtxEmuSubmoduleMVP(functional)负责 |
|------|---------------------------|-----------------------------------|
| 解码 PTX IR | ❌ | ✅ `decode_ptxir()` |
| 创建 GPUContext/SMContext/WarpContext | ❌ | ✅ `create_*()` |
| 提交 kernel | ❌(只调 facade.submit_kernel_request)| ✅ `submit_kernel_request()` |
| **推进一个 SM cycle** | ✅ `tick()` → `sm->exe_once()` | ❌ |
| **WarpScheduler 选 warp** | ✅(注入 PTX-EMU) | ❌ |
| **Scoreboard hazard 检查** | ✅ Step A(注入 IScoreboard)| ❌ |
| **Pipeline latency 注入** | ✅ Step B(注入 IPipelineLatencyProvider)| ❌ |
| **TC timing 注入** | ✅ Step C(注入 ITensorCoreTiming)| ❌ |
| **单条 PTX 指令功能执行** | ❌(只转发) | ✅ `functional_execute_warp()` |
| Barrier sync | ❌(只转发)| ✅ `functional_barrier_sync()` |
| Warp exit | ❌(只转发)| ✅ `functional_exit_warp()` |
| 读 lane 寄存器值 | ❌(WarpState 不含) | ✅ `read_register<T>()` |
| 写 lane 寄存器值 | ❌ | ✅ `write_register<T>()` |
| 读全局内存 | ❌ | ✅ `read_global_memory<T>()` |
| 写全局内存 | ❌ | ✅ `write_global_memory<T>()` |
| 读 lane PC | ❌(WarpState 不含) | ✅ `read_thread_pc()` |
| 推进 lane PC | ❌ | ✅ `advance_thread_pc()` |
| 读 active mask | ✅(WarpState.exec_mask = 镜像)| ✅ `read_active_mask()`(本征) |
| Warp 完成判断 | ❌ | ✅ `is_warp_finished()` |
| Lane 退出判断 | ❌ | ✅ `is_thread_exited()` |
| **镜像 cycle_count** | ✅ WarpState.cycle_count | ❌ |
| **镜像 blocked_cycles** | ✅ WarpState.blocked_cycles | ❌ |
| **镜像 scheduler_state** | ✅ WarpState.scheduler_state | ❌ |
| **创建 Scoreboard 实例** | ❌(调 facade)| ✅ `create_scoreboard()` |
| **创建 Pipeline 实例** | ❌(调 facade)| ✅ `create_pipeline_latency_provider()` |
| **创建 TC 实例** | ❌(调 facade)| ✅ `create_tensor_core_timing()` |
| 注入 timing 到 SMContext | ✅ `inject_timing_modules()` | ❌ |
| SM 资源管理(reserve/release)| ✅ | ❌ |
| 完成检测 + 反向流推送 | ✅ `on_warp_complete()` | ❌ |

---

## 7. 单元测试覆盖(微架构 timing 验证)

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_cuda_core_adapter_mvp_tick.cc` | `[cuda-core][mvp][tick]` | per-tick cycle 推进 + WarpScheduler 行为 |
| `test_cuda_core_adapter_mvp_scoreboard.cc` | `[cuda-core][mvp][scoreboard]` | RAW hazard 检查 + allocate/release 计数 |
| `test_cuda_core_adapter_mvp_pipeline.cc` | `[cuda-core][mvp][pipeline]` | Pipeline latency 注入 + blocked_cycles 镜像 |
| `test_cuda_core_adapter_mvp_dispatch.cc` | `[cuda-core][mvp][dispatch]` | on_cta_arrival 反压 + resource 管理 |
| `test_cuda_core_adapter_mvp_warp_state.cc` | `[cuda-core][mvp][warp-state]` | WarpState timing 状态镜像正确性(**不**含 PC) |
| `test_cuda_core_adapter_mvp_injection.cc` | `[cuda-core][mvp][injection]` | IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming 注入路径 |

**验收标准**(per ADR-SOC-06 G-MVP-3):
- [ ] per-tick `tick()` cycle_count 单调递增
- [ ] WarpScheduler 在 cycle 内选不同 warp(Round-Robin policy)
- [ ] RAW hazard 时 Step A 拒绝,blocked_cycles 镜像正确
- [ ] Pipeline latency 注入后 warp.blocked_cycles ≥ 期望值
- [ ] WarpState **不**含 PC 字段(架构契约)
- [ ] Functional 调用(`functional_execute_warp`)通过 facade 转发,本模块不直接调 PTX-EMU

---

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| R1 | PTX-EMU 注入接口变更(`set_scoreboard/set_pipeline_latency_provider/set_tensor_core_timing`) | 中 | 高 | submodule pin `PTX-EMU@87820951`(per ADR-SOC-06 §7.5);`abi_guards.h` 17 条静态断言 |
| R2 | 误用 functional API(直接调 PTX-EMU internal) | 中 | 高 | 编译期隔离:本模块接口**禁止** `WarpContext::execute_warp_instruction` 等 functional API;只能调 PtxEmuSubmoduleMVP facade |
| R3 | WarpState 误加 PC 字段 | 低 | 中 | 文档 + 接口表明确分离;测试 `warp_states_[warp_id]` 与 PtxEmuSubmoduleMVP::read_thread_pc 是不同来源 |
| R4 | Scoreboard/Pipeline/TC timing 注入时序错(在 sm->exe_once() 之前调注入) | 低 | 高 | 注入只在 `init()` 中调用一次;`tick()` 只调 `sm->exe_once()` |
| R5 | MinimalWarpSchedulerTLM 调度策略不适配 MVP | 低 | 中 | MVP 默认 Round-Robin;v0.5 完整版可加 GTO/Two-Level 等策略(per `warp_scheduler.h:42 RoundRobinWarpScheduler`) |
| R6 | WarpScheduler 接口与 PTX-EMU 不匹配(我们用 `MinimalWarpSchedulerTLM`,PTX-EMU 期望 `WarpScheduler`) | 中 | 高 | 适配器层:`MinimalWarpSchedulerTLM` 包装 `WarpScheduler` 纯虚接口(类似 ScoreboardTLM 适配 IScoreboard) |

---

## 9. 修订历史

| 日期 | 修订 |
|------|------|
| 2026-08-19 | 初版 — per ADR-SOC-06 D2 切片(双路径 blackbox/whitebox) |
| 2026-08-20 | Phase F-H.1 重构:改为深度集成 PTX-EMU 内部接口 |
| 2026-08-20 | **Phase I.2 重构(本次)**:严格按 gpgpu-sim functional/timing 分离原则重写。本模块定位为 **SM 微架构探索器**(timing model),集成 4 个已有 TLM 模块(`MinimalWarpSchedulerTLM` + `ScoreboardTLM` + `PipelineTLM` + `TensorCoreTLM`)。Functional 全部通过 `PtxEmuSubmoduleMVP` facade 转发,**不**直接调 PTX-EMU 内部接口。`WarpState` **不**含 PC(由 PtxEmuSubmoduleMVP 负责)。`tick()` 是 timing 主入口,3-Step 注入(scoreboard → functional → pipeline)由 PTX-EMU `sm->exe_once()` 内部完成。`inject_timing_modules()` 一次性注入 3 个 IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming 实例。 |

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-20*