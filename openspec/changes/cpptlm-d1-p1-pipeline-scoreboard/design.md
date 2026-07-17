# Design: cpptlm-d1-p1-pipeline-scoreboard - D1-Full Compute 注入 + Async Seam

> **Status**: Proposed（2026-07-15 从 `cpptlm-f12b-ld-impl` §2.5/§2.6/§3.3~§3.5 剥离）
> **Design Revision**: 2026-07-17（HSK-4/5 后简化 -- vendor 头文件 + 去掉 Internal/空壳 Adapter）
> **Parent**: `proposal.md` (cpptlm-d1-p1-pipeline-scoreboard)
> **前置 change**: `cpptlm-f12b-ld-impl`（P0 MemoryBridge, 归档后启动）

## 0. Design Revision 记录 (2026-07-17 HSK-4/5 后)

原 design（2026-07-15）使用 `IScoreboardInternal` / `IPipelineLatencyInternal` / `ITensorCoreTimingInternal` 内部接口 + 4 Adapter 翻译层。HSK-4 交付后确认 PTX-EMU 接口为纯虚头文件（零依赖），可直接 vendor + 直接实现。

**3 项简化**:
1. **Vendor 3 接口头文件**到 `include/cudart/`（与 `cpptlm_bridge.h` 同策略）
2. **去掉 Internal 接口层** -- 3 核心模块直接 `public IScoreboard` / `IPipelineLatencyProvider` / `ITensorCoreTiming`
3. **去掉 3 空壳 Adapter**（Scoreboard/Pipeline/TC 纯转发无附加价值）-- WarpScheduler Adapter 待 Phase 4 评估

见 `proposal.md` § Design Revision 段 + `docs/superpowers/specs/2026-07-17-hsk-4-5-responses.md`。

## 1. 设计概览

P0 完成后，`MemoryBridge` 已实现 `CppTLMBridge` 接口，`KernelLaunchTLM` 已作为 PTX-EMU 驱动器。CppTLM 已成为唯一时钟真相源。

P1 在此基础之上，通过 **直接实现 PTX-EMU 纯虚接口** 将 CppTLM 的 Scoreboard/Pipeline/TensorCore timing 注入 PTX-EMU `SMContext::exe_once()`。

```
CppTLM EventQueue tick
        |
        v
KernelLaunchTLM::tick()
  └---> PTX-EMU exe_once()
        |
        ├─ [NEW] Scoreboard hazard check -> ScoreboardTLM (CppTLM 提供, public IScoreboard)
        ├─ [NEW] Pipeline latency    -> PipelineTLM (CppTLM 提供, public IPipelineLatencyProvider)
        ├─ [NEW] TC timing           -> TensorCoreTLM (CppTLM 提供, public ITensorCoreTiming)
        |
        └---> execute_warp_instruction() (PTX-EMU 原有逻辑)
```

**关键简化**: 3 核心模块直接实现 PTX-EMU 接口（vendor 头文件），无 Internal 接口层 + 无空壳 Adapter。

## 2. 3 核心模块设计

### 2.1 ScoreboardTLM

```cpp
// include/tlm/gpu/scoreboard_tlm.hh
#include "cudart/scoreboard_interface.h"  // vendor from PTX-EMU 8acfd2d1
#include <array>

class ScoreboardTLM : public IScoreboard {
public:
    ScoreboardTLM();

    // IScoreboard 4 纯虚方法
    bool has_free_entry() const override;
    bool allocate(uint32_t reg_id, uint32_t warp_id) override;
    bool release(uint32_t reg_id, uint32_t warp_id) override;
    void tick() override;

private:
    static constexpr size_t MAX_ENTRIES = 64;  // 保守值，覆盖典型 SM 配置
    struct Entry { uint32_t reg_id = 0; uint32_t warp_id = 0; bool active = false; };
    std::array<Entry, MAX_ENTRIES> entries_;
    size_t active_count_ = 0;
};
```

**行为**:
- `has_free_entry()`: `active_count_ < MAX_ENTRIES`
- `allocate(reg, warp)`: 找第一个 `!active` slot，标记 active，`++active_count_`，返回 true；满则返回 false
- `release(reg, warp)`: 找匹配 `(reg_id, warp_id, active)` slot，标记 inactive，`--active_count_`，返回 true；未找到返回 false
- `tick()`: P1 阶段 no-op（Phase 4 可加超时释放逻辑）

### 2.2 PipelineTLM

```cpp
// include/tlm/gpu/pipeline_tlm.hh
#include "cudart/pipeline_interface.h"  // vendor from PTX-EMU 9e7361b9

class PipelineTLM : public IPipelineLatencyProvider {
public:
    PipelineTLM();

    double get_fractional_cycles(const std::string& instruction,
                                 PipelineId pipe_id) const override;
    double get_fractional_cycles_by_type(int statement_type,
                                         PipelineId pipe_id) const override;
};
```

**行为** (Phase 1 占位):
- `get_fractional_cycles(instruction, pipe_id)`: P1 占位返回 `1.0`（所有指令所有 pipeline = 1.0 cycle）
- `get_fractional_cycles_by_type(statement_type, pipe_id)`: P1 占位返回 `1.0`
- **Phase 4 对齐 gpgpu-sim 精确值**（G-D5）

### 2.3 TensorCoreTLM

```cpp
// include/tlm/gpu/tensor_core_tlm.hh
#include "cudart/tensor_core_interface.h"  // vendor from PTX-EMU 463038e0

class TensorCoreTLM : public ITensorCoreTiming {
public:
    TensorCoreTLM();

    uint32_t get_latency(TcPrecision prec) const override;
    uint32_t get_throughput_cycles(TcPrecision prec) const override;
    // get_latency_mnk: 不 override，用 PTX-EMU 头文件的 default impl (退化到 get_latency)
};
```

**行为** (Phase 1 占位):
- `get_latency(prec)`: P1 占位返回 `1`（所有精度 = 1 cycle）
- `get_throughput_cycles(prec)`: P1 占位返回 `1`
- `get_latency_mnk(prec, M, N, K)`: 默认实现退化到 `get_latency(prec)`（不 override）
- **Phase 4 对齐 gpgpu-sim 精确值**（G-D5）

## 3. Adapter 层（已去掉 - Design Revision 2026-07-17）

> **原设计**: 4 Adapter（WarpScheduler + Scoreboard + Pipeline + TC）做 CppTLM 内部接口 <-> PTX-EMU 接口翻译。
>
> **简化后**: 3 核心模块直接实现 PTX-EMU 接口（vendor 头文件），Scoreboard/Pipeline/TC Adapter 变成纯转发空壳，**已去掉**。
>
> **保留**: `CppTLMWarpSchedulerAdapter`（optional，待 Phase 4 评估）-- 做 `WarpContext*` <-> `uint32_t warp_id` 转换，不是纯转发。

## 4. 12 端点 static_assert 编译期拦截

```cpp
// test/test_12_endpoint_static_assert.cc
#include "cudart/pipeline_interface.h"    // vendor
#include "cudart/tensor_core_interface.h" // vendor

// PipelineId 6 端点 (来自 PTX-EMU pipeline_interface.h)
static_assert(static_cast<uint32_t>(PipelineId::P0_INT_FP32) == 0);
static_assert(static_cast<uint32_t>(PipelineId::V_SIMD)     == 1);
static_assert(static_cast<uint32_t>(PipelineId::P1_FP64)    == 2);
static_assert(static_cast<uint32_t>(PipelineId::P2_SFU)     == 3);
static_assert(static_cast<uint32_t>(PipelineId::P3_LSU)     == 4);
static_assert(static_cast<uint32_t>(PipelineId::P4_TC)      == 5);

// TcPrecision 6 端点 (来自 PTX-EMU tensor_core_interface.h)
static_assert(static_cast<uint32_t>(TcPrecision::FP4)  == 0);
static_assert(static_cast<uint32_t>(TcPrecision::FP6)  == 1);
static_assert(static_cast<uint32_t>(TcPrecision::FP8)  == 2);
static_assert(static_cast<uint32_t>(TcPrecision::FP16) == 3);
static_assert(static_cast<uint32_t>(TcPrecision::BF16) == 4);
static_assert(static_cast<uint32_t>(TcPrecision::TF32) == 5);
```

**优势**: 直接用 PTX-EMU vendor enum，无翻译层，编译期 `static_assert` 最干净。enum 值来自 PTX-EMU 真值源，HSK-4 已确认字节级一致。

## 5. IAsyncCompletion 占位（Phase 9+ 预留，已落地）

> ✅ 已实施于 commit `e69cd1d`（header-only `include/tlm/gpu/async_completion_adapter.hh`，97 行 + 5 单测全 PASS）。
>
> Phase 8.B 语义: `fire_completion()` 仅递增 `fire_completion_count_`，**不调用 callback**。Phase 9+ TMA async 触发时再激活。

## 6. 关键约束

### 6.1 字节级回退

- Scoreboard/Pipeline/TC 3 setter 全 nullptr -> PTX-EMU 回退到 `InstructionLatencyTable`
- P0 MemoryBridge 独立模式（`g_cpptlm_bridge == nullptr`）不依赖 P1 模块

### 6.2 双端 ABI 严格一致

- 12 端点（PipelineId 6 + TcPrecision 6）编译期 `static_assert`（用 vendor enum）
- 3 纯虚基类接口签名通过 vendor 头文件字节级一致
- `nullptr` = 回退到 PTX-EMU 内置延迟

### 6.3 D1-Full 三段式注入位置

PTX-EMU 端 `sm_context.cpp` Step A + B + C 三段式注入（HSK-5 commit `367fd6a5`）:
1. **Step A** (exe_once 入口): Scoreboard hazard check -> stall warp
2. **Step B** (指令执行前): Pipeline/TC latency query -> 设置 blocked_cycles
3. **Step C** (指令执行后): Scoreboard release (gated by warp_executed)

## 7. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: 12 端点 enum 值与 PTX-EMU 端不一致 | 低 | 中 | `static_assert` 编译期拦截 + vendor 头文件真值源 |
| **R2**: D1-Full fast/slow path 注入遗漏 | 中 | 高 | Adapter nullptr fallback + 强测试覆盖两条路径 |
| **R3**: Scoreboard stall/release 状态不一致 | 中 | 高 | chaos test: stall->re-schedule->release->re-issue 完整循环 |
| **R4**: PTX-EMU P1 接口交付延迟 | 已消除 | - | HSK-4/5 已交付（2026-07-17） |
| **R5**: Phase 1 占位 latency 偏离实际 | 高 | 低 | G-D5 (Phase 4) 精确对齐 gpgpu-sim ±15% |
