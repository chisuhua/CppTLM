# Design: cpptlm-f12b-ld-impl — CppTLM 端 F12b-LD + D1-Full 实施设计

> **Status**: Proposed
> **Parent**: `proposal.md` (cpptlm-f12b-ld-impl)
> **Reference**: [`2026-07-14-ptxemu-comprehensive-modification-plan.md`](../../../../docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md) §2-§5
> **Companion**: PTX-EMU `openspec/changes/cpptlm-d1-full/design.md` + `internal-plan.md`

## 1. 现状分析

### 1.1 CppTLM 端已有基础设施

```
CppTLM/
├── include/core/chstream_module.hh     # ChStreamModuleBase 基类
├── include/framework/stream_adapter.hh # StreamAdapter 转换层
├── include/tlm/crossbar_tlm.hh          # CrossbarTLM（4 端口）
├── include/tlm/cache_tlm.hh             # CacheTLM
├── include/tlm/memory_tlm.hh            # MemoryTLM
├── include/core/event_queue.hh          # EventQueue（事件循环核心）
├── include/utils/config_utils.hh        # JSON 配置解析
├── src/core/module_factory.cc           # ModuleFactory（含 StreamAdapter 注入）
└── tests/                                # Catch2 v3.7.0, 88 个 test_*.cc
```

### 1.2 PTX-EMU 端已定义 ABI（HSK-1 commit 8dc000ec）

```cpp
class CppTLMBridge {
public:
    virtual int version() const = 0;
    virtual int submit_kernel(uint64_t kernel_id, const char* kernel_name,
                              uint32_t grid_x, grid_y, grid_z,
                              uint32_t block_x, block_y, block_z,
                              const void** kernel_args, size_t args_count,
                              size_t shared_mem, uint64_t stream_id) = 0;
    virtual uint64_t poll_kernel(uint64_t kernel_id) = 0;
    virtual int synchronize_stream(uint64_t stream_id) = 0;
    virtual uint64_t global_access(uint64_t device_addr, uint64_t val,
                                   uint8_t type) = 0;
};
```

### 1.3 问题归纳

| # | 问题 | 影响 |
|---|------|------|
| **P1** | CppTLM 无 `CppTLMBridge` 实现 | PTX-EMU 端 `g_cpptlm_bridge == nullptr` 时只能走独立模式 |
| **P2** | CppTLM 无 PTX-EMU 驱动层 | `KernelLaunchTLM::tick()` 不知道如何调用 PTX-EMU `exe_once()` |
| **P3** | 无 `MemoryBridge` 把 NoC 延迟查询转为 `global_access()` 返回值 | PTX-EMU `LdHandler/StHandler` 调用 `global_access()` 时拿不到 CppTLM NoC 路由延迟 |
| **P4** | 无 D1-Full 4 Adapter + 3 核心模块 | PTX-EMU 端 SMContext 4 setter 拿到 nullptr |
| **P5** | 无 `IAsyncCompletion` 占位 | Phase 9+ TMA 演进无 seam |

## 2. 目标架构

### 2.1 整体控制流（实施后）

```
┌─────────────────────────────────────────────────────────────────┐
│ CppTLM EventQueue (唯一时钟真相源)                               │
└────────┬────────────────────────────────────────────────────────┘
         │ tick (每个周期)
         ▼
┌─────────────────────────────────────────────────────────────────┐
│ KernelLaunchTLM::tick()                                           │
│  ├─ 1) 从 pending_ FIFO pop KernelLaunchRequest                   │
│  ├─ 2) MemoryBridge::poll_kernel()  → 检查 PTX-EMU 内部完成状态     │
│  ├─ 3) 调用 PTX-EMU GPUContext::exe_once()  (每 tick 最多 N 次)     │
│  └─ 4) 检查 CppTLM NoC 异步完成事件 (IAsyncCompletion 触发)         │
└────────┬────────────────────────────────────────────────────────┘
         │ 触发
         ▼
┌─────────────────────────────────────────────────────────────────┐
│ MemoryBridge (implements CppTLMBridge)                            │
│  ├─ submit_kernel() → 1) deep-copy args 2) push to FIFO           │
│  ├─ poll_kernel()   → 查询 PTX-EMU pending 状态                   │
│  ├─ synchronize_stream() → 遍历 stream 等待完成                  │
│  └─ global_access() → CrossbarTLM::query_latency() → NoC 路由      │
└────────┬────────────────────────────────────────────────────────┘
         │ 查询 NoC 延迟
         ▼
┌─────────────────────────────────────────────────────────────────┐
│ CrossbarTLM + MemoryController (已有 CppTLM 基础设施)              │
│  ├─ query_latency(device_addr) → cycle 数                          │
│  └─ MemoryController::access(addr, val, type) → 数据读写         │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 MemoryBridge 详细设计

```cpp
// include/tlm/gpu/memory_bridge.hh
class MemoryBridge : public CppTLMBridge {
public:
    // 构造：注入 3 个 CppTLM 端依赖
    MemoryBridge(KernelLaunchTLM* kernel_launch,
                 CrossbarTLM* gpu_xbar,
                 MemoryController* gpu_memory);
    ~MemoryBridge() override = default;

    // CppTLMBridge 接口实现（5 虚方法）
    int version() const override { return CPPTLMBRIDGE_VERSION; }
    int submit_kernel(uint64_t kernel_id, const char* kernel_name,
                      uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                      uint32_t block_x, uint32_t block_y, uint32_t block_z,
                      const void** kernel_args, size_t args_count,
                      size_t shared_mem, uint64_t stream_id) override;
    uint64_t poll_kernel(uint64_t kernel_id) override;
    int synchronize_stream(uint64_t stream_id) override;
    uint64_t global_access(uint64_t device_addr, uint64_t val,
                           uint8_t type) override;

private:
    // kernel_args deep-copy helper（PTX-EMU host 端 args 内存可能在返回后失效）
    std::vector<std::vector<uint8_t>> deep_copy_args_(const void** args, size_t n);

    // 错误码转发
    cudaError_t translate_error_(int ret) const;

    // 依赖
    KernelLaunchTLM* kernel_launch_;     // FIFO push
    CrossbarTLM* gpu_xbar_;              // NoC 延迟查询
    MemoryController* gpu_memory_;      // 实际数据读写（如果需要）

    // kernel_id -> pending kernel 追踪（用于 poll_kernel/synchronize_stream）
    struct PendingKernel {
        uint64_t kernel_id;
        uint64_t stream_id;
    };
    std::unordered_map<uint64_t, PendingKernel> pending_kernels_;
};
```

### 2.3 KernelLaunchTLM 详细设计

```cpp
// include/tlm/gpu/kernel_launch_tlm.hh
class KernelLaunchTLM : public ChStreamModuleBase {
public:
    explicit KernelLaunchTLM(const std::string& name, EventQueue* eq);
    ~KernelLaunchTLM() override = default;

    std::string get_module_type() const override { return "KernelLaunchTLM"; }

    // EventQueue 每个 tick 调用一次
    void tick() override;

    // MemoryBridge 调用此方法入队 kernel
    void submit(KernelLaunchRequest&& req);

    // 设置 PTX-EMU 驱动（PTX-EMU 端用 extern "C" cpptlm_attach_bridge 调用）
    void set_ptx_emu_context(void* gpu_context);

    // Adapter 4 setter（D1-Full P1 阶段）
    void set_scoreboard(IScoreboardInternal* s);
    void set_pipeline_provider(IPipelineLatencyInternal* p);
    void set_tensor_core_timing(ITensorCoreTimingInternal* t);
    void set_async_completion(IAsyncCompletion* ac);

    // 注入到 SMContext（PTX-EMU 端 exe_once 时调用）
    void inject_into_sm_context(SMContext* sm) const;

private:
    // Pending kernel FIFO
    std::deque<KernelLaunchRequest> pending_;

    // MemoryBridge 实例
    std::unique_ptr<MemoryBridge> bridge_;

    // 4 内部模块（D1-Full）
    IScoreboardInternal* scoreboard_ = nullptr;
    IPipelineLatencyInternal* pipeline_provider_ = nullptr;
    ITensorCoreTimingInternal* tensor_core_timing_ = nullptr;
    IAsyncCompletion* async_completion_ = nullptr;

    // PTX-EMU 内部 handle（gpu_context 模拟）
    void* ptx_emu_context_ = nullptr;

    // 上限防护
    static constexpr uint32_t MAX_PTX_STEPS_PER_TICK = 10000;

    // 内部辅助
    void call_ptx_emu_exe_once_();
    void poll_ptx_emu_completion_(uint64_t kernel_id);
};
```

### 2.4 数据结构：KernelLaunchRequest

```cpp
struct KernelLaunchRequest {
    uint64_t kernel_id;
    uint64_t stream_id;
    const char* kernel_name;        // PTX-EMU 内部长期存储，无需 deep-copy
    uint32_t grid_x, grid_y, grid_z;
    uint32_t block_x, block_y, block_z;
    size_t shared_mem;
    void* func_ptr = nullptr;       // PTX 函数指针（kernel_name 解析后）
};
```

### 2.5 D1-Full 4 Adapter 设计

```cpp
// include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.hh
class CppTLMWarpSchedulerAdapter : public WarpScheduler {
public:
    explicit CppTLMWarpSchedulerAdapter(WarpSchedulerTLM* impl);
    
    // PTX-EMU WarpScheduler 接口
    int schedule_next() override;
    void update_active_mask(WarpContext* w) override;
    bool all_warps_finished() override;
    
    // PTX-EMU 端可能添加的 3 方法（默认实现）
    void set_execution_mode(ExeMode mode) override { /* no-op */ }
    ExeMode get_execution_mode() override { return ExeMode::Default; }
    void schedule_with_migration(uint32_t warp_id, uint32_t target_sm) override { /* no-op */ }
    
private:
    WarpSchedulerTLM* impl_;  // CppTLM 端实现
    // WarpContext* ↔ uint32_t warp_id 转换
    uint32_t to_warp_id(WarpContext* w) const;
    WarpContext* from_warp_id(uint32_t id) const;
};

// 类似 Scoreboard/Pipeline/TC 3 个 Adapter
// 关键: 12 端点（PipelineId 6 + TcPrecision 6）双向 static_assert 编译期拦截
static_assert(PipelineId::SASS_0 == 0);  // 与 PTX-EMU 端 PipelineId 枚举值一致
static_assert(TcPrecision::FP16 == 2);   // 与 PTX-EMU 端 TcPrecision 枚举值一致
// ... 共 12 个 static_assert
```

### 2.6 IAsyncCompletion 占位（Phase 9+ 预留）

```cpp
// include/tlm/gpu/async_completion_adapter.hh
class AsyncCompletionAdapter : public IAsyncCompletion {
public:
    void register_completion_callback(uint64_t id, std::function<void()> cb) override {
        // Phase 8.B: 存回调但不触发（占位）
        pending_callbacks_[id] = std::move(cb);
    }
    void fire_completion(uint64_t id) override {
        // Phase 9+: 真正触发回调
        // Phase 8.B: 空实现（独立模式不触发）
        if (auto it = pending_callbacks_.find(id); it != pending_callbacks_.end()) {
            it->second();
            pending_callbacks_.erase(it);
        }
    }
private:
    std::unordered_map<uint64_t, std::function<void()>> pending_callbacks_;
};
```

## 3. 实施阶段（P0/P1/P2/P3）

### 3.1 阶段总览（与综合计划对齐）

| 阶段 | 内容 | 时长 | 阻塞依赖 |
|------|------|:---:|----------|
| **P0** | F12b-LD MemoryBridge (#C1, #C2) + G-F0 | ~5 天 | HSK-1（已就绪） |
| **P1** | D1-Full Compute (#C3, #C4) | ~2.5 天 | P0 接口就绪 |
| **P2** | Phase 9+ Async Seam (#C5) | ~1 小时 | P1 #C3 |
| **P3** | 集成验证 (5 类 microbenchmark + 文档同步) | ~1 周 | P0+P1+P2 |

### 3.2 P0 阶段详细设计（~5 天）

#### Day 1-2: #C1 MemoryBridge 实施

**实施内容**:
1. `include/tlm/gpu/memory_bridge.hh`：类声明 + 5 虚方法 override
2. `src/tlm/gpu/memory_bridge.cc`：
   - 构造函数：保存 3 个 CppTLM 端依赖指针
   - `version()`：返回 `CPPTLMBRIDGE_VERSION`（编译期断言 = 1）
   - `submit_kernel()`：
     1. 校验 `kernel_name != nullptr` + `kernel_args != nullptr || args_count == 0`
     2. `deep_copy_args_(args, args_count)` → `std::vector<std::vector<uint8_t>>`
     3. 构造 `KernelLaunchRequest` + push to `pending_kernels_` map
     4. 调用 `kernel_launch_->submit(std::move(req))`（FIFO push）
     5. 返回 `0` (success) 或 `cudaError_t`
   - `poll_kernel()`：
     1. 在 `pending_kernels_` 中查找 `kernel_id`
     2. 未找到 → 返回 `UINT64_MAX`（未知）
     3. 找到 → 调用 `kernel_launch_->poll_ptx_emu_completion_(kernel_id)` 获取剩余 cycles
     4. 若返回 0，从 map 中 erase
     5. 返回剩余 cycles（>0 表示还需等待）
   - `synchronize_stream()`：
     1. 遍历 `pending_kernels_` 中所有 `stream_id` 匹配的项
     2. 循环调用 `poll_kernel()` 直到 stream 上无 pending
     3. 返回 `0` (success)
   - `global_access()`：
     1. 调用 `gpu_xbar_->query_latency(device_addr)` 获取 NoC 路由延迟
     2. 若地址未映射 → 返回 `UINT64_MAX`（fallback 到 PTX-EMU 内部）
     3. 若映射 → 返回延迟 cycle 数
     4. 数据读写由 PTX-EMU 端 `LdHandler/StHandler` 在 `SimpleMemory` 完成（Phase 8.B timing-only 语义）

**验证**: `cpptlm_tests [gpu][f12b]` + 7 个 MemoryBridge 单测 PASS

#### Day 3-4: #C2 KernelLaunchTLM 实施

**实施内容**:
1. `include/tlm/gpu/kernel_launch_tlm.hh`：类声明 + 4 Adapter setter 预留
2. `src/tlm/gpu/kernel_launch_tlm.cc`：
   - `tick()`：
     1. 调 `bridge_->synchronize_stream(0)`（默认 stream）
     2. 循环执行 `call_ptx_emu_exe_once_()` 最多 `MAX_PTX_STEPS_PER_TICK` 次
     3. 每次 exe_once 后检查 `bridge_->poll_kernel()` 是否有 kernel 完成
     4. 若有 → 从 `pending_` FIFO erase 已完成 kernel
   - `submit()`：FIFO push
   - `set_*()`：4 Adapter setter（D1-Full P1 阶段用）
3. `set_ptx_emu_context()`：接收 PTX-EMU 端 gpu_context handle
4. `inject_into_sm_context()`：把 4 内部模块 + MemoryBridge 注入 PTX-EMU SMContext

**关键约束**: `tick()` 必须**不**调用 `bridge_->tick()`（`g_cpptlm_bridge == nullptr` 时字节级回退到原行为）

**验证**: 单测 + 集成测试 PASS

#### Day 5: G-F0 vector_add 烟雾测试

**实施内容**:
1. `configs/vector_add_n1024.json`：测试配置（n=1024²）
2. `tests/python/test_f12b_smoke.py`：
   - 启动 `cpptlm_sim` with F12b-LD enabled
   - 运行 vector_add kernel
   - 比对输出与 standalone PTX-EMU baseline（逐元素）
   - 验证延迟 ≤ 2× baseline
3. PTX-EMU 端 Phase 1 实施完成后，**双端联合验证**

**质量门**:
```python
assert output_ptxemu.equals(output_cpptlm)         # 逐元素一致
assert latency_cpptlm <= 2 * latency_standalone  # 延迟 ≤ 2× baseline
```

**通过** → P0 阶段交付 → 进入 P1

### 3.3 P1 阶段详细设计（~2.5 天）

#### Day 6: 3 核心模块（#C4）

**实施内容**:
1. `include/tlm/gpu/scoreboard_tlm.hh` + `scoreboard_tlm.cc`：
   - `class ScoreboardTLM : public IScoreboardInternal`
   - ≥12 entries hazard table
   - `has_free_entry()`, `allocate(reg_id, warp_id)`, `release(reg_id, warp_id)` 3 方法
2. `include/tlm/gpu/pipeline_tlm.hh` + `pipeline_tlm.cc`：
   - `class PipelineTLM : public IPipelineLatencyInternal`
   - 5+V 抽象（仅返回分数 cycle 延迟）
   - `get_fractional_cycles_by_type(instr_type, pipeline_id) → double`
3. `include/tlm/gpu/tensor_core_tlm.hh` + `tensor_core_tlm.cc`：
   - `class TensorCoreTLM : public ITensorCoreTimingInternal`
   - 6 精度（FP16/FP32/FP64/BF16/INT8/INT4）
   - `get_latency(precision) → uint32_t cycles`

**关键约束**: 12 端点（PipelineId 6 + TcPrecision 6）双向 `static_assert` 编译期拦截（与 PTX-EMU 端枚举值一致）

#### Day 7: 4 Adapter（#C3）

**实施内容**:
1. `include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.{hh,cc}`（10b）
2. `include/tlm/gpu/adapter/cpptlm_scoreboard_adapter.{hh,cc}`（15）
3. `include/tlm/gpu/adapter/cpptlm_pipeline_adapter.{hh,cc}`（15）
4. `include/tlm/gpu/adapter/cpptlm_tensor_core_adapter.{hh,cc}`（15）

**关键约束**:
- 每个 Adapter 持有对应 CppTLM 端模块指针 + WarpContext* ↔ uint32_t 转换
- 12 端点 enum 与 PTX-EMU 端**严格一致**（`static_assert` 编译期拦截）
- `nullptr` 行为：完全跳过（fallback 到 `InstructionLatencyTable`）

### 3.4 P2 阶段详细设计（~1 小时）

#### Day 8: IAsyncCompletion 占位（#C5）

**实施内容**:
- `include/tlm/gpu/async_completion_adapter.hh` + `.cc`
- 简单 map<id, callback> 存回调
- `register_completion_callback()` 存
- `fire_completion()` 触发（Phase 8.B 不调用，Phase 9+ 调用）

**质量门**: 编译通过 + 独立模式 `async_completion_ = nullptr` 无影响

### 3.5 P3 阶段详细设计（~1 周）

#### Day 9-14: 集成验证

**实施内容**:
1. `tests/python/test_gpgpu_sim_comparison.py`：
   - 5 类 microbenchmark（GEMM/FlashAttn/vector_add/stencil/sparse SpMV）
   - vs gpgpu-sim baseline ±15% 带宽
   - vs standalone PTX-EMU ±10%
2. `tests/integration/cpptlm/test_full_pipeline.cc`：
   - Level 1 合成 workload 5 类 microbenchmark
   - Level 2 真实 CUDA kernel（`test_cuda_kernel_integration.cc`）
3. `docs/microarchitecture/`：6 个微架构 doc（待办）
4. 1 GB203 × 1M < 60s 性能验收
5. docs_sync_check 0 missing

**质量门**:
- G-D5: 5 类 microbenchmark vs gpgpu-sim ±15%
- G-D8: exe_once chaos test（stall → re-schedule → release → re-issue 完整循环）
- G3/G4/G5: docs + 性能 + 全量回归

## 4. 关键约束

### 4.1 字节级回退

- `g_cpptlm_bridge == nullptr` 时所有改动**字节级**与原行为一致
- 独立模式 600+ PTX-EMU 测试零退化
- 现有 764 CppTLM 测试零退化

### 4.2 双端 ABI 严格一致

- `CPPTLMBRIDGE_VERSION=1`
- 5 虚方法签名（12 参数 `submit_kernel`）逐字节匹配
- 12 端点（PipelineId 6 + TcPrecision 6）双向 `static_assert`
- `cudaStream_t` 宽度 `static_assert`

### 4.3 D1-Full 三段式注入位置

PTX-EMU 端 `sm_context.cpp:222` (Step A) + `253/338` (Step B+C) 三段式注入（已确认）。CppTLM 端 4 内部模块 + 4 Adapter 必须在 PTX-EMU 端 exe_once 之前就绪（D1 EOD），否则注入点 nullptr fallback。

## 5. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: MemoryBridge `kernel_args` deep-copy 漏掉某类型 | 低 | 高 | 单测覆盖 5 类（int/float/ptr/struct/array）+ 与 PTX-EMU 端 ABI 字节级对比 |
| **R2**: KernelLaunchTLM 死循环（PTX-EMU 内部状态卡住） | 中 | 高 | `MAX_PTX_STEPS_PER_TICK=10000` 上限防护 + 死锁检测日志 |
| **R3**: 12 端点 enum 值与 PTX-EMU 端不一致 | 低 | 中 | `static_assert` 编译期拦截 + 双端 CI 双重断言 |
| **R4**: D1-Full fast/slow path 注入遗漏 | 中 | 高 | 12 Adapter nullptr fallback + 强测试覆盖两条路径 |
| **R5**: `synchronize_stream` 死锁 | 低 | 高 | 单测覆盖 + 主机端事件循环 yield 策略 |
