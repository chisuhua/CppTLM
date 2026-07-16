# Design: cpptlm-d1-p1-pipeline-scoreboard — D1-Full Compute 注入 + Async Seam

> **Status**: Proposed（2026-07-15 从 `cpptlm-f12b-ld-impl` §2.5/§2.6/§3.3~§3.5 剥离）
> **Parent**: `proposal.md` (cpptlm-d1-p1-pipeline-scoreboard)
> **前置 change**: `cpptlm-f12b-ld-impl`（P0 MemoryBridge, 归档后启动）

## 1. 设计概览

P0 完成后，`MemoryBridge` 已实现 `CppTLMBridge` 接口，`KernelLaunchTLM` 已作为 PTX-EMU 驱动器。CppTLM 已成为唯一时钟真相源。

P1 在此基础之上，通过 **Adapter 模式** 将 CppTLM 的 Scoreboard/Pipeline/TensorCore timing 注入 PTX-EMU `SMContext::exe_once()`。

```
CppTLM EventQueue tick
        │
        ▼
KernelLaunchTLM::tick()
  └─→ PTX-EMU exe_once()
        │
        ├─ [NEW] Scoreboard hazard check → ScoreboardTLM (CppTLM 提供)
        ├─ [NEW] Pipeline latency    → PipelineTLM (CppTLM 提供)
        ├─ [NEW] TC timing           → TensorCoreTLM (CppTLM 提供)
        │
        └─→ execute_warp_instruction() (PTX-EMU 原有逻辑)
```

## 2. 3 核心模块设计

### 2.1 ScoreboardTLM

```cpp
// include/tlm/gpu/scoreboard_tlm.hh
class ScoreboardTLM : public IScoreboardInternal {
public:
    bool has_free_entry() const override;
    bool allocate(uint32_t reg_id, uint32_t warp_id) override;
    bool release(uint32_t reg_id, uint32_t warp_id) override;
    void tick() override;

private:
    static constexpr size_t MAX_ENTRIES = 12;
    struct Entry { uint32_t reg_id; uint32_t warp_id; bool active; };
    std::array<Entry, MAX_ENTRIES> entries_;
    size_t active_count_ = 0;
};
```

### 2.2 PipelineTLM

```cpp
// include/tlm/gpu/pipeline_tlm.hh
class PipelineTLM : public IPipelineLatencyInternal {
public:
    double get_fractional_cycles(const std::string& instruction,
                                 PipelineId pipe_id) const override;
    double get_fractional_cycles_by_type(int statement_type,
                                         PipelineId pipe_id) const override;
};
```

### 2.3 TensorCoreTLM

```cpp
// include/tlm/gpu/tensor_core_tlm.hh
class TensorCoreTLM : public ITensorCoreTimingInternal {
public:
    uint32_t get_latency(TcPrecision prec) const override;
    uint32_t get_throughput_cycles(TcPrecision prec) const override;
    uint32_t get_latency_mnk(TcPrecision prec, uint32_t M, uint32_t N, uint32_t K) const override;
};
```

## 3. 4 Adapter 设计

```cpp
// include/tlm/gpu/adapter/cpptlm_scoreboard_adapter.hh
class CppTLMScoreboardAdapter : public IScoreboard {
public:
    explicit CppTLMScoreboardAdapter(ScoreboardTLM* impl);
    bool has_free_entry() const override { return impl_->has_free_entry(); }
    bool allocate(uint32_t reg_id, uint32_t warp_id) override { return impl_->allocate(reg_id, warp_id); }
    bool release(uint32_t reg_id, uint32_t warp_id) override { return impl_->release(reg_id, warp_id); }
    void tick() override { impl_->tick(); }
private:
    ScoreboardTLM* impl_;
};

// 类似 CppTLMPipelineAdapter(CppTLM↔PTX-EMU PipelineId 枚举双向翻译)
// 类似 CppTLMTensorCoreAdapter(CppTLM↔PTX-EMU TcPrecision 枚举双向翻译)
// 类似 CppTLMWarpSchedulerAdapter(WarpContext* ↔ uint32_t warp_id 转换)
```

### 12 端点 `static_assert` 编译期拦截

```cpp
// test/test_12_endpoint_static_assert.cc
static_assert(static_cast<int>(PipelineId::P0_INT_FP32) == 0);
static_assert(static_cast<int>(PipelineId::V_SIMD) == 1);
static_assert(static_cast<int>(PipelineId::P1_FP64) == 2);
static_assert(static_cast<int>(PipelineId::P2_SFU) == 3);
static_assert(static_cast<int>(PipelineId::P3_LSU) == 4);
static_assert(static_cast<int>(PipelineId::P4_TC) == 5);
// + TcPrecision 6 (FP4=0, FP6=1, FP8=2, FP16=3, BF16=4, TF32=5)
```

## 4. IAsyncCompletion 占位（Phase 9+ 预留）

```cpp
// include/tlm/gpu/async_completion_adapter.hh
class AsyncCompletionAdapter : public IAsyncCompletion {
public:
    void register_completion_callback(uint64_t id, std::function<void()> cb) override {
        pending_callbacks_[id] = std::move(cb);
    }
    void fire_completion(uint64_t id) override {
        if (auto it = pending_callbacks_.find(id); it != pending_callbacks_.end()) {
            it->second();
            pending_callbacks_.erase(it);
        }
    }
private:
    std::unordered_map<uint64_t, std::function<void()>> pending_callbacks_;
};
```

## 5. 关键约束

### 5.1 字节级回退

- Scoreboard/Pipeline/TC/TensorCore/AsyncCompletion 5 setter 全 nullptr → PTX-EMU 回退到 `InstructionLatencyTable`
- P0 MemoryBridge 独立模式（`g_cpptlm_bridge == nullptr`）不依赖 P1 模块

### 5.2 双端 ABI 严格一致

- 12 端点（PipelineId 6 + TcPrecision 6）双向 `static_assert`
- 3 纯虚基类接口签名逐字节一致
- `nullptr` = 回退到 PTX-EMU 内置延迟

### 5.3 D1-Full 三段式注入位置

PTX-EMU 端 `sm_context.cpp` Step A + B + C 三段式注入：
1. **Step A** (exe_once 入口): Scoreboard hazard check → stall warp
2. **Step B** (指令执行前): Pipeline/TC latency query → 设置 blocked_cycles
3. **Step C** (指令执行后): Scoreboard release

## 6. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: 12 端点 enum 值与 PTX-EMU 端不一致 | 低 | 中 | `static_assert` 编译期拦截 + 双端 CI 双重断言 |
| **R2**: D1-Full fast/slow path 注入遗漏 | 中 | 高 | Adapter nullptr fallback + 强测试覆盖两条路径 |
| **R3**: Scoreboard stall/release 状态不一致 | 中 | 高 | chaos test: stall→re-schedule→release→re-issue 完整循环 |
| **R4**: PTX-EMU P1 接口交付延迟 | 高 | 高 | P1 拆为独立 change，P0 不受阻