# cuda-core-adapter 微架构文档

> **类别**: GPU > Cuda Core Adapter (新概念) · **状态**: 🔵 MVP 切片 (per ADR-X.17)
> **Header**: `include/tlm/gpu/cuda_core_adapter_mvp.hh`
> **位置**: DGpuBoardTLM 内部组件 + 也可作为独立 ChStreamModuleBase 暴露
> **蓝图来源**: PTX-EMU `WarpContext::execute_warp_instruction` + `SMContext::exe_once` + CUDA Core 抽象
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> **关联 ADR**: [`ADR-X.17-cpptlm-v05-mvp.md`](../../adr/ADR-X.17-cpptlm-v05-mvp.md) D2/D5
> **关联模块**: [`ptx-emu-submodule-mvp.md`](./ptx-emu-submodule-mvp.md) · [`tmu-dispatch-processor.md`](./tmu-dispatch-processor.md)
> **首版 commit**: 🔵 W3-4 实施(基础)+ W5-6 升级(per-warp)· **最近更新**: 2026-08-19
> **维护者**: CppTLM Team (Sisyphus)

---

## 1. 设计目标

`CudaCoreAdapter` 是 **新概念模块**,包装 **Cuda Core(SM)** 与 PTX-EMU 之间的 bridge,**调用 PTX-EMU 的 warp 指令实现指令执行**。MVP 通过 PtxEmuSubmoduleMVP adapter 与 PTX-EMU 交互,支持双路径(黑盒 + 白盒)。

**核心特性**:
- **双路径 dispatch**:`dispatch_blackbox`(image_execute)+ `dispatch_whitebox`(stepOneWarpInstruction)
- **黑盒 MVP 路径**(S1 默认):调 `ptxemu_image_execute`,PTX-EMU 内部完整执行
- **白盒精度路径**(S3 可选,需 PTX-EMU 新 API):循环 `stepOneWarpInstruction`,返回 PC + cycle + status
- **per-warp cycle 跟踪**(白盒):per-warp WarpState { pc, cycle_count, register_deps }
- **承接 ComputeUnitTLM Legacy 角色**(per ADR-X.15 §"12 SM 模块命运表"):GpuComputeUnitTLM 推 Legacy,CudaCoreAdapter 作为 MVP 替代

**MVP vs 现有 ComputeUnitTLM 差异**:
- ❌ `GpuComputeUnitTLM`(Legacy,Phase 8.A):黑盒 4 SubCoreSlot + MinimalWarpSchedulerTLM + WavefrontTLM,**不调 PTX-EMU**
- ✅ `CudaCoreAdapter`(MVP):调 PTX-EMU warp 指令,继承 ChStreamModuleBase 或作为内部组件

**MVP vs v0.5 完整版简化**:
- ✅ 保留:双路径 dispatch
- ✅ 保留:per-warp cycle 跟踪(白盒路径)
- ❌ 裁剪:ScoreboardTLM + PipelineTLM 升级(MVP 推迟到 S4)
- � 裁剪:TensorCoreTLM + SharedMemoryTLM + VectorRegfileTLM(MVP 不实施)
- ❌ 裁剪:WarpScheduler 注入策略(由 PTX-EMU 自带)

---

## 2. 架构概览

### 2.1 黑盒 MVP 路径(S1-S2)

```
TmuDispatchProcessor::submit(record)
    │
    ▼
CudaCoreAdapter::issueTask(record)
    │
    ▼ (黑盒默认)
PtxEmuSubmoduleMVP::image_execute(handle, grid, block, shared_mem, args, argc)
    │
    ▼
PTX-EMU (submodule)
    ├─ GPUContext 分配 SM
    ├─ SMContext::exe_once() × N cycles (每 cycle 推进)
    │    └─ (PTX-EMU 内部)WarpContext::execute_warp_instruction × M times
    │       ├─ Step A: Scoreboard hazard check
    │       ├─ Step B: Pipeline/TC latency query (PTX-EMU 内部)
    │       └─ Step C: Scoreboard release
    └─ Kernel 完成 → completion callback
         │
         ▼
CudaCoreAdapter::on_complete(task_id, status)
    │
    ▼
TmuDispatchProcessor::on_complete(task_id, status)
    │
    ▼
CompletionRing::push(task_id, status) → host_notify
```

### 2.2 白盒精度路径(S3 可选)

```
CudaCoreAdapter::dispatch_whitebox(warp_count, max_cycles)
    │
    ▼ 循环每个 warp
PtxEmuSubmoduleMVP::stepOneWarpInstruction(warp_id, &pc, &status, &cycle_count)
    │
    ▼
PTX-EMU SMContext::get_warp(warp_id)
    └─ WarpContext::execute_warp_instruction(stmt, target_pc)
    └─ 返回 PC + cycle_count + status(per-warp 精度)
    │
    ▼
CudaCoreAdapter 更新 WarpState[warp_id] = { pc, cycle_count, register_deps }
```

---

## 3. 接口(Public API)

```cpp
class CudaCoreAdapter {
public:
    /// Warp 状态(per-warp cycle 跟踪)
    struct WarpState {
        uint64_t pc = 0;
        uint64_t cycle_count = 0;
        std::array<uint64_t, 32> register_deps = {};  // 32 lanes × 1 reg id(MVP 简化)
    };

    /// DispatchParams: image_execute 调用参数
    struct DispatchParams {
        uint32_t grid_x = 1, grid_y = 1, grid_z = 1;
        uint32_t block_x = 1, block_y = 1, block_z = 1;
        uint32_t shared_mem_bytes = 0;
        void** kernel_args = nullptr;
        size_t args_count = 0;
    };

    explicit CudaCoreAdapter(PtxEmuSubmoduleMVP& ptx_emu);

    /// TmuDispatchProcessor 调用:接收 task
    /// @return true=成功接受,false=拒绝(资源不足)
    bool issueTask(const TmuDispatchRecord& record);

    /// 黑盒 dispatch(默认 MVP 路径)
    /// @return image_id (>=0=成功, -1=失败)
    int32_t dispatch_blackbox(uint64_t image_handle, const DispatchParams& params);

    /// 白盒 dispatch(可选,需 PTX-EMU 新 API)
    /// @return 完成 warp 数(等于 warp_count 表示全部完成)
    uint32_t dispatch_whitebox(uint32_t warp_count, uint64_t max_cycles);

    /// PTX-EMU 完成 kernel 后回调
    void on_complete(uint64_t task_id, int32_t status);

    // === 测试/监控 ===
    WarpState warp_state(uint32_t warp_id) const;
    uint64_t total_cycle_count() const { return total_cycle_count_; }
    uint64_t blackbox_dispatch_count() const { return blackbox_dispatch_count_; }
    uint64_t whitebox_dispatch_count() const { return whitebox_dispatch_count_; }
    bool is_enabled_whitebox() const { return enable_whitebox_; }

    /// JSON params 注入
    void set_enable_whitebox(bool enabled) { enable_whitebox_ = enabled; }
    void set_max_warps(uint32_t n) { max_warps_ = n; }

    /// Per-tick 推进(由 DGpuBoardTLM::tick() 调用)
    void tick();

private:
    // === 依赖 ===
    PtxEmuSubmoduleMVP& ptx_emu_;

    // === 状态 ===
    std::unordered_map<uint64_t, int32_t> active_tasks_;  // task_id → PTX-EMU image_id
    std::vector<WarpState> warp_states_;  // per-warp 状态(白盒路径)

    // === 配置 ===
    bool enable_whitebox_ = false;  // 默认 false(MVP 黑盒优先)
    uint32_t max_warps_ = 64;       // per-SM warps 上限

    // === 统计 ===
    uint64_t total_cycle_count_ = 0;
    uint64_t blackbox_dispatch_count_ = 0;
    uint64_t whitebox_dispatch_count_ = 0;
};
```

---

## 4. 行为流程

### 4.1 issueTask()(TmuDispatchProcessor 调用)

```cpp
bool CudaCoreAdapter::issueTask(const TmuDispatchRecord& record) {
    // 1. 从 PtxEmuSubmoduleMVP 获取 image handle(per vram_addr)
    //    注:install_kernel_module 已返回 vram_addr + image handle 映射
    uint64_t image_handle = record.image_handle;  // pre-loaded in install_kernel_module

    // 2. 构造 DispatchParams
    DispatchParams params;
    params.grid_x = record.grid_dim[0];
    params.grid_y = record.grid_dim[1];
    params.grid_z = record.grid_dim[2];
    params.block_x = record.block_dim[0];
    params.block_y = record.block_dim[1];
    params.block_z = record.block_dim[2];
    params.shared_mem_bytes = record.shared_mem_bytes;
    params.kernel_args = ...;  // 从 args_vram_addr 映射(MVP 简化)
    params.args_count = record.args_size;

    // 3. 双路径 dispatch
    int32_t image_id;
    if (enable_whitebox_) {
        // 白盒路径(per-warp)
        uint32_t warp_count = params.block_x * params.block_y * params.block_z;
        image_id = dispatch_whitebox(warp_count, /*max_cycles=*/1000);
    } else {
        // 黑盒路径(整 kernel)
        image_id = dispatch_blackbox(image_handle, params);
    }

    if (image_id < 0) return false;

    // 4. 记录 active task
    active_tasks_[record.task_id] = image_id;
    return true;
}
```

### 4.2 黑盒 dispatch_blackbox()

```cpp
int32_t CudaCoreAdapter::dispatch_blackbox(uint64_t image_handle,
                                           const DispatchParams& params) {
    blackbox_dispatch_count_++;

    // 调 PtxEmuSubmoduleMVP::image_execute(8 ABI 透传)
    int32_t status = ptx_emu_.image_execute(
        image_handle,
        params.grid_x, params.grid_y, params.grid_z,
        params.block_x, params.block_y, params.block_z,
        params.shared_mem_bytes,
        params.kernel_args, params.args_count
    );

    // PTX-EMU 自包含执行,完成后回调 on_complete
    if (status == 0) {
        // 模拟执行完成(MVP:同步返回)
        return /*image_id*/ next_image_id_++;
    } else {
        return -1;  // 失败
    }
}
```

### 4.3 白盒 dispatch_whitebox()(S3 启用)

```cpp
uint32_t CudaCoreAdapter::dispatch_whitebox(uint32_t warp_count, uint64_t max_cycles) {
    whitebox_dispatch_count_++;

    uint32_t completed = 0;
    uint64_t cycle_accum = 0;

    for (uint32_t warp_id = 0; warp_id < warp_count; ++warp_id) {
        // 循环 stepOneWarpInstruction 直到 warp 完成或超 max_cycles
        while (cycle_accum < max_cycles) {
            uint64_t pc = 0;
            int32_t status = 0;
            uint64_t cycle_count = 0;

            int32_t rc = ptx_emu_.stepOneWarpInstruction(warp_id, &pc, &status, &cycle_count);
            cycle_accum += cycle_count;
            warp_states_[warp_id].pc = pc;
            warp_states_[warp_id].cycle_count += cycle_count;
            total_cycle_count_ += cycle_count;

            if (rc != 0 || status != 0) {
                // warp 完成或错误
                completed++;
                break;
            }
        }
    }

    return completed;
}
```

---

## 5. 关键设计取舍

### 5.1 双路径共存(per ADR-X.16 D8)

- **黑盒 MVP 路径**(`image_execute`):快速模式,完整 kernel 执行,cycle 数不暴露
- **白盒精度路径**(`stepOneWarpInstruction`):per-warp PC + cycle 精度,**可选启用**

**MVP 选择**:默认黑盒(`enable_whitebox=false`),S3 可选启用白盒。白盒需 PTX-EMU 端新增 `stepOneWarpInstruction` API;若 PTX-EMU 维护者拒收,MVP 仅黑盒。

### 5.2 承接 GpuComputeUnitTLM Legacy 角色

per `ADR-X.15 §"12 SM 模块命运表"`:
- `GpuComputeUnitTLM` v0.4 保留 Legacy(Phase 8.A 单元测试覆盖)
- `CudaCoreAdapter` 作为 MVP 替代,调 PTX-EMU warp 指令
- **不破 apu_soc 兼容**:GpuComputeUnitTLM 单元测试保留(v0.4 §"显式声明 v3.0.0 不删除")

### 5.3 per-warp WarpState(白盒路径)

MVP 白盒路径 per-warp 跟踪:
- `pc`:当前指令 PC
- `cycle_count`:累计 cycle 数
- `register_deps`:32 lanes × 1 reg id(MVP 简化,v0.5 完整版 32 lanes × 8 reg deps)

### 5.4 简化版 args 映射

MVP `kernel_args` 从 VRAM 映射(MVP 简化):
- `args_vram_addr` → 直接当 host pointer(MVP 不仿真 H2D DMA 反向映射)
- v0.5 完整版:实现 `map_vram_to_host()`(per `ADR-X.15 §3.6.4`)

---

## 6. 测试覆盖

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_cuda_core_adapter_mvp.cc` | `[cuda-core][mvp]` | issueTask + dispatch_blackbox + on_complete |
| `test_cuda_core_adapter_mvp_whitebox.cc` | `[cuda-core][mvp][whitebox]` | 白盒路径(S3)+ per-warp WarpState 跟踪 |

**验收标准**(per ADR-X.17 G-MVP-1, G-MVP-3):
- 黑盒路径:issueTask → dispatch_blackbox → PTX-EMU image_execute → on_complete 完整 PASS
- 白盒路径(S3):stepOneWarpInstruction per-warp cycle 跟踪 PASS

---

## 7. 实施路径

### 7.1 S1 MVP-Cut(W1-2)

1. 新建 `include/tlm/gpu/cuda_core_adapter_mvp.hh` + `src/tlm/gpu/cuda_core_adapter_mvp.cc`(~200 LOC)
2. 引用 `ptx_emu_submodule_mvp.hh`(编译防火墙)
3. 黑盒 dispatch_blackbox 完整实现
4. 新建 `test/test_cuda_core_adapter_mvp.cc`(issueTask + 黑盒 dispatch)

### 7.2 S3 Warp-Precision(W5-6)

1. 升级白盒 dispatch_whitebox(需 PTX-EMU 新 API)
2. per-warp WarpState 跟踪
3. 新建 `test/test_cuda_core_adapter_mvp_whitebox.cc`

---

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| R1 | PTX-EMU 维护者拒收 `stepOneWarpInstruction` | 中 | 中 | MVP 仅黑盒;fork 兜底 |
| R2 | GpuComputeUnitTLM Legacy 单元测试与 CudaCoreAdapter 冲突 | 低 | 中 | Legacy 保留(Legacy 标签),CudaCoreAdapter 新建独立 namespace |
| R3 | 黑盒路径无 per-warp 精度,违反 G-D5 | 中 | 低 | 白盒路径可选;MVP 仅"内部一致性"验证 |
| R4 | args 简化映射触发真实工作负载崩溃 | 中 | 中 | MVP 仅验证简单 kernel(vec_add);真实负载推到 v0.5 完整版 |
| R5 | on_complete 回调未触发导致 active_tasks_ 泄漏 | 中 | 中 | 超时 watchdog(per S5+);MVP 简单计数验证 |

---

## 9. 修订历史

- **2026-08-19**: 初版 — per ADR-X.17 D2/D5 切片(MVP 4 阶段 S1+S3)

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-19*
