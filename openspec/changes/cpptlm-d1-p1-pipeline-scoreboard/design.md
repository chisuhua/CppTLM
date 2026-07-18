# Design: cpptlm-d1-p1-pipeline-scoreboard - D1-Full Compute 注入 + Async Seam

> **Status**: Proposed（2026-07-15 从 `cpptlm-f12b-ld-impl` §2.5/§2.6/§3.3~§3.5 剥离）
> **Design Revision 1**: 2026-07-17（HSK-4/5 后简化 -- vendor 头文件 + 去掉 Internal/空壳 Adapter）
> **Design Revision 2**: 2026-07-18（Metis + Oracle 双审后修复 4 项架构 P0 + 4 项文档 P0）
> **Design Revision 3**: 2026-07-18（Oracle P0 协同仿真架构设计 — IPtxEmuDriver 窄接口 + §8 完整数据流 + 9 步实施计划）
> **Design Revision 4**: 2026-07-18（Oracle P0 审查修复实施 — unordered_map O(1) + CAPACITY=2048 + AdvanceResult + unique_ptr + reset() + VERSION=2）
> **Parent**: `proposal.md` (cpptlm-d1-p1-pipeline-scoreboard)
> **前置 change**: `cpptlm-f12b-ld-impl`（P0 MemoryBridge, 归档后启动）

## 0. Design Revision 记录

### 0.1 Revision 1 (2026-07-17 HSK-4/5 后)

原 design（2026-07-15）使用 `IScoreboardInternal` / `IPipelineLatencyInternal` / `ITensorCoreTimingInternal` 内部接口 + 4 Adapter 翻译层。HSK-4 交付后确认 PTX-EMU 接口为纯虚头文件（零依赖），可直接 vendor + 直接实现。

**3 项简化**:
1. **Vendor 3 接口头文件**到 `include/cudart/`（与 `cpptlm_bridge.h` 同策略）
2. **去掉 Internal 接口层** -- 3 核心模块直接 `public IScoreboard` / `IPipelineLatencyProvider` / `ITensorCoreTiming`
3. **去掉 3 空壳 Adapter**（Scoreboard/Pipeline/TC 纯转发无附加价值）-- WarpScheduler Adapter 待 Phase 4 评估

见 `proposal.md` § Design Revision 段 + `docs/superpowers/specs/2026-07-17-hsk-4-5-responses.md`。

### 0.2 Revision 2 (2026-07-18 Metis + Oracle 双审后)

经 Metis（预规划审查）+ Oracle（架构深度审查）双审，确认并修复 4 项架构 P0（基于实际读取 PTX-EMU 端 `sm_context.h`/`sm_context.cpp`/`warp_context.h`/`scoreboard_interface.h` 验证）:

1. **ScoreboardTLM `MAX_ENTRIES` 64 -> 512**: 经 PTX-EMU 端代码验证，`scoreboard_` 是 `SMContext` 成员（`sm_context.h:188`），**global scoreboard 语义**（所有 warp 共用一个 `IScoreboard` 实例，用 `(reg_id, warp_id)` 二元组区分）。64 entries 对 64 warps × 8 in-flight 严重不足。详见 §2.1。

2. **`duplicate_allocate` 行为决议**: rejects（返回 false）。PTX-EMU `step_a_scoreboard_check` 的 rollback 逻辑（`sm_context.cpp:37-43`）假设 allocate 失败即回滚已分配 entries，rejects 语义最匹配。详见 §2.1。

3. **G-D6/G-D7 验收归属重划**: nullptr 回退行为在 PTX-EMU `step_a/b/c` helper 中实现（`sm_context.cpp:31`/`56`/`280`），CppTLM 端无法独立测试。G-D6/G-D7 移到 PTX-EMU 端验收（PTX-7a `test_nullptr_fallback` 已覆盖）。详见 §6.4。

4. **HSK-4/5 响应文档修订**: L156/L176/L202-207/L31 四处残留旧架构表述（"4 Adapter 注入 3 setter" 等），与 §0.1 Design Revision 矛盾。需立即修订避免 PTX-EMU 团队误解。

**另确认无问题**:
- Pipeline/TC 是 stateless 查询（`sm_context.cpp:283-291`），global 共享正确
- Phase 1 占位值 1.0/1 不会导致 exe_once 行为异常（仅 latency 不真实）
- vendor + 直接实现架构方向正确（去掉 3 空壳 Adapter 减 440 LOC 无功能损失）

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

> **Design Revision 3** (2026-07-18 Oracle P0): 数据结构从 `std::array` 线性扫描改为 `std::unordered_map` O(1) 查找，CAPACITY 从 512 提升至 2048（覆盖 3-dest 最坏场景），新增 `reset()` 跨 kernel 清空方法。

```cpp
// include/tlm/gpu/scoreboard_tlm.hh
#include "cudart/scoreboard_interface.h"  // vendor from PTX-EMU 8acfd2d1
#include <unordered_map>

class ScoreboardTLM : public IScoreboard {
public:
    static constexpr size_t CAPACITY = 2048;  // 64 warps × 8 in-flight × 3 dest × 1.33 margin
    ScoreboardTLM();

    // IScoreboard 4 纯虚方法 — O(1) 实现
    bool has_free_entry() const override;
    bool allocate(uint32_t reg_id, uint32_t warp_id) override;
    bool release(uint32_t reg_id, uint32_t warp_id) override;
    void tick() override;

    /// 跨 kernel 重置（非虚，CppTLM 特有 — IScoreboard vendored 接口不含此方法）
    void reset();

private:
    static uint64_t make_key(uint32_t reg_id, uint32_t warp_id) {
        return (static_cast<uint64_t>(warp_id) << 32) | reg_id;
    }
    std::unordered_map<uint64_t, bool> entries_;  // key → active flag
};
```

**Global scoreboard 语义**（2026-07-18 Oracle 审查确认）:

经 PTX-EMU 端代码验证，`scoreboard_` 是 `SMContext` 成员（`sm_context.h:188`），**所有 warp 共用一个 `IScoreboard` 实例**，用 `(reg_id, warp_id)` 二元组区分不同 warp 的 entries。PTX-EMU `step_a_scoreboard_check`（`sm_context.cpp:29-45`）从 `WarpContext::get_physical_warp_id()` 获取 `warp_id` 后传入 `allocate(reg_id, warp_id)`。

**容量依据**（`CAPACITY = 2048`）:
- 最坏场景: 64 warps/SM × 8 in-flight instructions/warp × 3 dest regs = **1536 entries**
- 2048 = 1536 × 1.33 margin（覆盖异常高并发 + hash table 负载因子 ~75%）
- 原 `MAX_ENTRIES = 512` 修改为 2048（Oracle P0 审查: 512 在 3-dest 指令场景下严重不足）

**行为**:
- `has_free_entry()`: `entries_.size() < CAPACITY`
- `allocate(reg, warp)`: 
  - 构造函数 `reserve(CAPACITY)` 预分配 bucket，防频繁 rehash
  - `emplace(make_key(reg_id, warp_id), true)` — O(1) 平均，插入失败 = **duplicate rejects**
  - 满则返回 false
  - **rejects 决议依据**: `unordered_map::emplace` 的 `.second == false` 语义等同于 key 已存在 → 返回 false，与 PTX-EMU rollback 逻辑完全对齐（`sm_context.cpp:37-43`）
- `release(reg, warp)`: `erase(make_key(reg_id, warp_id))` — O(1) 平均；返回是否成功删除
- `tick()`: P1 阶段 no-op（Phase 4 可加超时释放逻辑）
- `reset()`: `entries_.clear()` + `reserve(CAPACITY)` — 跨 kernel 清空，保留预分配（Phase 4 PTX-EMU 在 kernel launch 前调用）

**死锁缓解说明**（2026-07-18 Oracle 审查 + Design Revision 3 更新）:

**潜在死锁场景**: Step A `allocate` 失败 -> `goto warp_done` -> `warp_executed = false` -> Step C 不执行 -> release 永不发生 -> scoreboard 越来越满。

**缓解机制**（无需 P1 实现 `tick()` 超时释放）:
1. **容量充足**: `CAPACITY = 2048` 覆盖 3-dest 最坏场景（1536），余量 508 entries
2. **Step A rollback 完整**: `sm_context.cpp:37-43` 的 `for (auto prev : allocated) scoreboard->release(prev, warp_id)` 确保本批次已分配的 entries 完整回滚，**不会泄漏**
3. **`decrement_blocked_cycles` 每 tick 执行**: `sm_context.cpp:313-316` 对所有 warp 调用 `WarpContext::decrement_blocked_cycles`，被 Step B `set_blocked_cycles_for_active` 阻塞的 warp 会自动恢复可调度
4. **`reset()` 跨 kernel 隔离**: 每个 kernel launch 前 PTX-EMU 调用 `scoreboard->reset()`，kernel 间 stale entries 不回传播
5. **真死锁场景**（边缘）: warp 异常退出但 entries 未 release。留 Phase 4 `tick()` 超时释放处理（如 active > 1000 cycles 强制 release）

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

    /// Phase 1 占位标记（Phase 4 替换为真实值后返回 false）
    bool is_placeholder() const { return is_placeholder_; }
private:
    bool is_placeholder_ = true;  // PHASE 1 PLACEHOLDER - Phase 4 改为 false
};
```

> **⚠️ PHASE 1 PLACEHOLDER - DO NOT USE AS CANONICAL VALUE**
>
> P1 占位返回 `1.0`，**不用于性能评估**。Phase 4 替换为 gpgpu-sim 精确值（G-D5, ±15%）。

**行为** (Phase 1 占位):
- `get_fractional_cycles(instruction, pipe_id)`: P1 占位返回 `1.0`（所有指令所有 pipeline = 1.0 cycle）
- `get_fractional_cycles_by_type(statement_type, pipe_id)`: P1 占位返回 `1.0`
- **Phase 4 对齐 gpgpu-sim 精确值**（G-D5）
- `is_placeholder()`: P1 返回 `true`，Phase 4 替换真实值后返回 `false`（供测试/监控区分）

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

    /// Phase 1 占位标记（Phase 4 替换为真实值后返回 false）
    bool is_placeholder() const { return is_placeholder_; }
private:
    bool is_placeholder_ = true;  // PHASE 1 PLACEHOLDER - Phase 4 改为 false
};
```

> **⚠️ PHASE 1 PLACEHOLDER - DO NOT USE AS CANONICAL VALUE**
>
> P1 占位返回 `1`，**不用于性能评估**。Phase 4 替换为 gpgpu-sim 精确值（G-D5, ±15%）。

**行为** (Phase 1 占位):
- `get_latency(prec)`: P1 占位返回 `1`（所有精度 = 1 cycle）
- `get_throughput_cycles(prec)`: P1 占位返回 `1`
- `get_latency_mnk(prec, M, N, K)`: 默认实现退化到 `get_latency(prec)`（不 override）
- **Phase 4 对齐 gpgpu-sim 精确值**（G-D5）
- `is_placeholder()`: P1 返回 `true`，Phase 4 替换真实值后返回 `false`

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
#include "cudart/scoreboard_interface.h"  // vendor
#include <type_traits>

// === Enum 值验证 (12 端点) ===

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

// === 方法签名级验证 (2026-07-18 Oracle 建议补充) ===
// 防止 PTX-EMU 修改方法签名时（如 int -> StatementType）仅 enum 值验证通过但
// override 不匹配的 silent ABI drift。

// IScoreboard 签名
static_assert(std::is_same_v<
    decltype(&IScoreboard::has_free_entry), bool(IScoreboard::*)() const>);
static_assert(std::is_same_v<
    decltype(&IScoreboard::allocate), bool(IScoreboard::*)(uint32_t, uint32_t)>);
static_assert(std::is_same_v<
    decltype(&IScoreboard::release), bool(IScoreboard::*)(uint32_t, uint32_t)>);
static_assert(std::is_same_v<
    decltype(&IScoreboard::tick), void(IScoreboard::*)()>);

// IPipelineLatencyProvider 签名
static_assert(std::is_same_v<
    decltype(&IPipelineLatencyProvider::get_fractional_cycles),
    double(IPipelineLatencyProvider::*)(const std::string&, PipelineId) const>);
static_assert(std::is_same_v<
    decltype(&IPipelineLatencyProvider::get_fractional_cycles_by_type),
    double(IPipelineLatencyProvider::*)(int, PipelineId) const>);

// ITensorCoreTiming 签名
static_assert(std::is_same_v<
    decltype(&ITensorCoreTiming::get_latency),
    uint32_t(ITensorCoreTiming::*)(TcPrecision) const>);
static_assert(std::is_same_v<
    decltype(&ITensorCoreTiming::get_throughput_cycles),
    uint32_t(ITensorCoreTiming::*)(TcPrecision) const>);
```

**优势**: 
- Enum 值验证: 直接用 PTX-EMU vendor enum，无翻译层，编译期 `static_assert` 最干净。enum 值来自 PTX-EMU 真值源，HSK-4 已确认字节级一致。
- 方法签名验证: 防止 silent ABI drift（如 PTX-EMU 修改 `int statement_type` -> `StatementType type` 时，仅 enum 验证通过但 override 不匹配，编译器报"cannot override"错误信息不友好；签名级 `static_assert` 失败信息明确）。

## 5. IAsyncCompletion 占位（Phase 9+ 预留，已落地）

> ✅ 已实施于 commit `e69cd1d`（header-only `include/tlm/gpu/async_completion_adapter.hh`，97 行 + 5 单测全 PASS）。
>
> Phase 8.B 语义: `fire_completion()` 仅递增 `fire_completion_count_`，**不调用 callback**。Phase 9+ TMA async 触发时再激活。

**架构定位**（2026-07-18 Oracle 审查）:

`IAsyncCompletion` 是 **CppTLM 自定义接口**（非 PTX-EMU vendor）。HSK-4/5 响应文档中未提及 PTX-EMU 端 `sm_context.h` 是否有 `set_async_completion()` setter（已验证 `sm_context.h` 仅含 `set_scoreboard`/`set_pipeline_latency_provider`/`set_tensor_core_timing` 3 个 setter，无 `set_async_completion`）。

**当前定位**: 投机性脚手架（speculative scaffolding）。Phase 9+ TMA async 是否需要此接口取决于 PTX-EMU 端是否添加对应集成点。若 PTX-EMU 不需要，应 deprecated 或删除（避免维护负担）。

## 6. 关键约束

### 6.1 字节级回退

- Scoreboard/Pipeline/TC 3 setter 全 nullptr -> PTX-EMU 回退到内置延迟（`ptxsim::getLatency(stmt.type).cycles`，即 **InstructionLatencyTable**，此回退逻辑在 PTX-EMU 端 `step_b_set_blocked_cycles` 实现，`sm_context.cpp:280-294`）
- P0 MemoryBridge 独立模式（`g_cpptlm_bridge == nullptr`）不依赖 P1 模块
- **InstructionLatencyTable 是 PTX-EMU 端逻辑**，CppTLM 端无需实现

### 6.2 双端 ABI 严格一致

- 12 端点（PipelineId 6 + TcPrecision 6）编译期 `static_assert`（用 vendor enum）
- **方法签名级 `static_assert`**（用 `decltype` + `std::is_same_v`，见 §4）-- 防止 silent ABI drift
- 3 纯虚基类接口签名通过 vendor 头文件字节级一致
- `nullptr` = 回退到 PTX-EMU 内置延迟

### 6.3 D1-Full 三段式注入位置

PTX-EMU 端 `sm_context.cpp` Step A + B + C 三段式注入（HSK-5 commit `367fd6a5`）:
1. **Step A** (exe_once 入口): Scoreboard hazard check -> stall warp
2. **Step B** (指令执行前): Pipeline/TC latency query -> 设置 blocked_cycles
3. **Step C** (指令执行后): Scoreboard release (gated by warp_executed)

### 6.4 验收门归属（2026-07-18 Oracle 审查后明确）

- **CppTLM 端可独立验证**: G-D1（编译通过）、G-D4（12 端点 static_assert）
- **PTX-EMU 端验收**: G-D6（3 setter nullptr 零退化）、G-D7（nullptr 回退 InstructionLatencyTable）-- 因 nullptr 回退行为在 PTX-EMU `step_a/b/c` helper 中实现（`sm_context.cpp:31`/`56`/`280`），CppTLM 端无法独立测试。PTX-7a `test_nullptr_fallback` 已覆盖。
- **双端联合验收**: G-D2、G-D3、G-D5、G-D8（需 PTX-7a/7b 完成）

## 7. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: 12 端点 enum 值与 PTX-EMU 端不一致 | 低 | 中 | `static_assert` 编译期拦截 + vendor 头文件真值源 + **签名级 `decltype` 验证**（§4） |
| **R2**: D1-Full fast/slow path 注入遗漏 | 中 | 高 | Adapter nullptr fallback + 强测试覆盖两条路径（PTX-7a `test_nullptr_fallback`） |
| **R3**: Scoreboard stall/release 状态不一致 | 低 | 高 | ~~chaos test: stall->re-schedule->release->re-issue 完整循环~~（P1 阶段无需额外测试）-- 2026-07-18 Oracle 审查: Step A rollback 完整（`sm_context.cpp:37-43`）+ `MAX_ENTRIES=512` 容量充足 + `decrement_blocked_cycles` 每 tick 执行，正常 workload 无死锁 |
| **R4**: PTX-EMU P1 接口交付延迟 | 已消除 | - | HSK-4/5 已交付（2026-07-17） |
| **R5**: Phase 1 占位 latency 偏离实际 | 高 | 低 | G-D5 (Phase 4) 精确对齐 gpgpu-sim ±15% + `is_placeholder()` 方法标注（§2.2/§2.3） |
| **R6**: Scoreboard 容量不足导致 Step A 频繁失败 | ~~高~~ 低 | ~~高~~ 中 | ~~原 `MAX_ENTRIES=64` 严重不足~~ -- 2026-07-18 Design Revision 4: `CAPACITY=2048`（`unordered_map` O(1)，覆盖 64 warps × 8 in-flight × 3 dest × 1.33 margin，远超前版 512） |
| **R7**: HSK-4/5 响应文档残留旧架构表述误导 PTX-EMU 团队 | 中 | 中 | 2026-07-18 修订 L156/L176/L202-207/L31 四处过时表述 |
| **R8**: AsyncCompletionAdapter 投机性脚手架维护负担 | 中 | 低 | Phase 9+ 确认 PTX-EMU 端是否需要 `set_async_completion` setter；若不需要则 deprecated |

## 8. IPtxEmuDriver 窄驱动接口 — 跨仓库协同仿真架构（2026-07-18 Oracle P0 审查）

> **Design Revision 3**: 原 Phase 4 设计采用 `static_cast<GPUContext*>(ptx_emu_context_)->exe_once(warp_id)` 直调 PTX-EMU，但 `cpptlm_core` (C++17) 无法 include PTX-EMU (C++20) 头文件。Oracle P0 审查重新设计为纯虚抽象接口 + PTX 侧 shim 模式。

### 8.1 设计动机

**原方案的根本问题**:
1. `cpptlm_core` 静态库独立编译（C++17），不依赖任何 PTX-EMU 头文件
2. PTX-EMU 使用 C++20 + ANTLR4 + 自定义 IR 类型，无法被 CppTLM 消费
3. `void* ptx_emu_context_` 在不引入 PTX-EMU 头文件的前提下无法安全转换
4. `exe_once(warp_id)` API 签名与实际 `GPUContext::exe_once()`（无参数，`EXE_STATE` 返回值）不符

**替代方案**: 定义 C++17 纯虚窄接口 `IPtxEmuDriver`，PTX-EMU 侧实现 shim。

### 8.2 接口定义 (CppTLM 侧)

> **Design Revision 3** (2026-07-18 Oracle P0): `advance()` 返回 `AdvanceResult` enum（替代 `void`），`inject_*()` 改为 `unique_ptr` 转移所有权，新增错误传播语义。

```cpp
// include/tlm/gpu/ptx_emu_driver.hh
// C++17, 零 PTX-EMU 依赖 (仅 include 已 vendor 的 3 个纯虚接口头)
#ifndef TLM_GPU_PTX_EMU_DRIVER_HH
#define TLM_GPU_PTX_EMU_DRIVER_HH

#include "cudart/scoreboard_interface.h"    // IScoreboard
#include "cudart/pipeline_interface.h"      // IPipelineLatencyProvider + PipelineId
#include "cudart/tensor_core_interface.h"   // ITensorCoreTiming + TcPrecision
#include <cstdint>
#include <memory>

namespace tlm {

/// advance() 返回值 — 区分 4 种执行状态
enum class AdvanceResult : uint8_t {
    Executed,       // 推进 ≥1 cycle
    NoOp,           // 无 pending work（kernel 完成或未启动）
    KernelComplete, // kernel 执行完毕（is_kernel_complete 可查询）
    Error           // PTX-EMU 异常（exe_once 抛异常 / SM 越界）
};

/// PTX-EMU 执行引擎的抽象驱动接口
/// CppTLM (C++17) 定义，PTX-EMU (C++20) 实现
class IPtxEmuDriver {
public:
    virtual ~IPtxEmuDriver() = default;

    /// 推进 PTX-EMU 执行最多 max_cycles 个周期
    /// @return AdvanceResult（替代原 uint32_t，支持错误传播）
    virtual AdvanceResult advance(uint32_t max_cycles, uint32_t& actual_cycles) = 0;

    /// 查询 kernel 是否已完成执行
    virtual bool is_kernel_complete(uint64_t kernel_id) = 0;

    /// 注入 per-SM scoreboard（转移所有权）
    /// CppTLM 调用后不再持有该实例；PTX-EMU 负责 SM 销毁时释放
    virtual void inject_scoreboard(uint32_t sm_id, std::unique_ptr<IScoreboard> sb) = 0;

    /// 注入 per-SM pipeline latency provider（转移所有权）
    virtual void inject_pipeline(uint32_t sm_id, std::unique_ptr<IPipelineLatencyProvider> p) = 0;

    /// 注入 per-SM tensor core timing（转移所有权）
    virtual void inject_tensor_core(uint32_t sm_id, std::unique_ptr<ITensorCoreTiming> tc) = 0;

    /// 获取 SM 数量
    virtual uint32_t num_sms() const = 0;
};

}  // namespace tlm
#endif
```

**Design Revision 3 变更要点**:
- `AdvanceResult` enum: `advance()` 原返回 `uint32_t`（歧义: 0 = 成功? 失败?），改为 `AdvanceResult` 显式语义。`actual_cycles` 输出参数报告实际推进数。
- `unique_ptr` 所有权转移: `inject_*()` 原裸指针无所有权语义 → CppTLM 注入后不再持有 → PTX-EMU 负责销毁。避免 `shared_ptr` 循环引用风险（CppTLM ↔ PTX-EMU 交叉持有）。
- 可 mock: 测试时注入 fake driver，`AdvanceResult` 枚举支持 4 种状态的分支覆盖。

**设计原则**:
- 纯虚接口仅使用 `uint32_t`、`uint64_t`、`unique_ptr` 和已 vendor 的纯虚接口（均为 C++17 兼容）
- C++17/20 ABI 安全：纯虚表布局在两个语言版本间一致；`unique_ptr` ABI 跨语言版本兼容
- 错误不可静默：`AdvanceResult::Error` 要求 CppTLM 端 `KernelLaunchTLM::tick()` 记录日志 + 终止 tick

### 8.3 Shim 实现 (PTX-EMU 侧)

> **Design Revision 3**: `advance()` 返回 `AdvanceResult` + 输出 `actual_cycles`，`inject_*()` 接收 `unique_ptr`。

```cpp
// PTX-EMU src/cudart/cpptlm_bridge/PtxEmuDriverShim.h  (C++20)
#include "tlm/gpu/ptx_emu_driver.hh"   // from CppTLM install
#include "ptxsim/gpu_context.h"         // GPUContext
#include <mutex>
#include <unordered_map>

class PtxEmuDriverShim : public tlm::IPtxEmuDriver {
    GPUContext* ctx_;  // 非所有权
    std::unordered_map<uint64_t, bool> completion_;
    mutable std::mutex mu_;

    // 持有 CppTLM 注入的 per-SM 资源（unique_ptr，PTX-EMU 负责销毁）
    std::vector<std::unique_ptr<IScoreboard>> scoreboards_;
    std::vector<std::unique_ptr<IPipelineLatencyProvider>> pipelines_;
    std::vector<std::unique_ptr<ITensorCoreTiming>> tensorcores_;

public:
    explicit PtxEmuDriverShim(GPUContext* ctx) : ctx_(ctx) {}

    AdvanceResult advance(uint32_t max_cycles, uint32_t& actual_cycles) override {
        actual_cycles = 0;
        if (!ctx_) return AdvanceResult::Error;
        try {
            while (actual_cycles < max_cycles && ctx_->get_state() != EXIT) {
                ctx_->exe_once();
                ++actual_cycles;
            }
            if (ctx_->get_state() == EXIT) {
                // 标记所有已注册 kernel 为完成
                std::lock_guard lock(mu_);
                for (auto& [kid, done] : completion_) done = true;
            }
        } catch (...) {
            return AdvanceResult::Error;
        }
        return actual_cycles > 0 ? AdvanceResult::Executed : AdvanceResult::NoOp;
    }

    bool is_kernel_complete(uint64_t kernel_id) override {
        std::lock_guard lock(mu_);
        auto it = completion_.find(kernel_id);
        return it != completion_.end() && it->second;
    }

    void register_kernel(uint64_t kid) {
        std::lock_guard lock(mu_);
        completion_[kid] = false;
    }
    void mark_complete(uint64_t kid) {
        std::lock_guard lock(mu_);
        if (auto it = completion_.find(kid); it != completion_.end())
            it->second = true;
    }

    void inject_scoreboard(uint32_t sm_id, std::unique_ptr<IScoreboard> sb) override {
        if (sm_id < num_sms()) {
            // kernel launch 前调用 reset() 清除上一 kernel 的 stale entries
            if (auto* scoreboard = dynamic_cast<ScoreboardTLM*>(sb.get()))
                scoreboard->reset();
            ctx_->get_sm(sm_id)->set_scoreboard(sb.get());
            scoreboards_.push_back(std::move(sb));  // 转移所有权
        }
    }
    void inject_pipeline(uint32_t sm_id, std::unique_ptr<IPipelineLatencyProvider> p) override {
        if (sm_id < num_sms()) {
            ctx_->get_sm(sm_id)->set_pipeline_latency_provider(p.get());
            pipelines_.push_back(std::move(p));
        }
    }
    void inject_tensor_core(uint32_t sm_id, std::unique_ptr<ITensorCoreTiming> tc) override {
        if (sm_id < num_sms()) {
            ctx_->get_sm(sm_id)->set_tensor_core_timing(tc.get());
            tensorcores_.push_back(std::move(tc));
        }
    }
    uint32_t num_sms() const override {
        return static_cast<uint32_t>(ctx_->get_num_sms());
    }
};
```

**Design Revision 3 关键变更**:
- `advance()` 捕获 `exe_once()` 异常 → `AdvanceResult::Error`（不再静默）
- `inject_*()` 接收 `unique_ptr` → 存入 `vector<unique_ptr>` → PTX-EMU 负责生命周期
- `inject_scoreboard()` 调用 `ScoreboardTLM::reset()` 清空上一 kernel 的 stale entries
- `completion_` 使用 `std::mutex` + `bool`（非 `std::atomic<bool>` — `atomic` 不可复制/移动，无法直接放入 `unordered_map`）
- `SMContext` 的 3 setter 已在 `sm_context.h:67-75` 定义（inline），shim 仅做转发 — 零 PTX-EMU 核心代码修改
- `GPUContext::KernelLaunchRequest::on_complete` 回调已存在（`gpu_context.h:62`），bridge path 构造 request 时设置即可

### 8.4 运行时加载

新增 ABI 入口（与 `cpptlm_attach_bridge` 同层级）:

```cpp
// PTX-EMU: cudart_sim.cpp
extern "C" PTXEMU_BRIDGE_API void cpptlm_set_driver(tlm::IPtxEmuDriver* driver);

// CppTLM: 全局指针
namespace tlm { inline IPtxEmuDriver* g_ptx_emu_driver = nullptr; }
```

加载时机: PTX-EMU `initialize_environment()` 创建 `GPUContext` → 创建 `PtxEmuDriverShim` → `cpptlm_set_driver()` → CppTLM `KernelLaunchTLM::set_ptx_emu_driver()`。

### 8.5 端到端数据流

```
cudaLaunchKernel (PTX-EMU bridge path)
  ├─ kernel_id = generate_kernel_id()
  ├─ shared_ptr deep-copy args (单次，消除双重深拷贝)
  ├─ g_cpptlm_bridge->submit_kernel(kernel_id, ...)
  │    → MemoryBridge → KernelLaunchTLM::pending_.push(req)
  ├─ g_driver_shim->register_kernel(kernel_id)       ─┐
  ├─ g_ptx_interpreter->launchPtxInterpreter(...)      │
  │    → GPUContext::submit_kernel_request({            │
  │        on_complete = [kid]{                         │
  │          g_driver_shim->mark_complete(kid); ────────┤ shared
  │        }                                            │ completion
  │      })                                             │ map
  └─ return cudaSuccess                                │
                                                         │
CppTLM EventQueue::run(1)                               │
  └─ KernelLaunchTLM::tick()                            │
       ├─ result = driver_->advance(MAX_PTX_STEPS, actual) │
       │    switch (result):                             │
       │      Error       → log + abort tick             │
       │      NoOp        → skip completion check        │
       │      Executed / KernelComplete:                 │
       │         → GPUContext::exe_once() × N             │
       │              → SMContext::exe_once()             │
       │                   ├─ Step A: sb->allocate()      │
       │                   ├─ execute_warp_instruction()  │
       │                   ├─ Step B: pipeline->get_frac  │
       │                   └─ Step C: sb->release()       │
       │              → kernel done → on_complete ────────┘
       │
       └─ for each pending req:
            if driver_->is_kernel_complete(req.kernel_id)
              → bridge_->mark_complete()
              → pending_.pop_front()
```

### 8.6 KernelLaunchTLM 与 Phase 1 模块的关系

**Phase 1 创建的 3 模块** (`ScoreboardTLM`/`PipelineTLM`/`TensorCoreTLM`):
- **不通过 KernelLaunchTLM setter 注入**（原 tasks.md 4.1/4.2 设计已废弃）
- 改为: `main.cpp` 通过 `IPtxEmuDriver::inject_*()` 直接注入到 PTX-EMU `SMContext`
- **多 SM**: `num_sms()` 查询数量 → 创建 N 个 `std::unique_ptr<ScoreboardTLM>` → 逐 SM 注入
- **理由**: 注入发生在仿真启动时，不在 tick() 热路径中；职责分离（KernelLaunchTLM 是调度器，非资源管理器）

### 8.7 Cycle 契约

| 维度 | 定义 |
|------|------|
| **1 CppTLM EventQueue tick** | = 1 次 `KernelLaunchTLM::tick()` |
| **1 tick → PTX-EMU** | = `driver->advance(N)` → 最多 N 次 `GPUContext::exe_once()` |
| **1 GPUContext::exe_once()** | = 1 个 PTX-EMU cycle（所有 SM 各推进 1 cycle） |
| **推荐的 N** | `MAX_PTX_STEPS_PER_TICK = 1`（1:1 映射，满足 G-D3 ≤1 cycle） |
| **Stall 语义** | `cycle_counter_++` 在 stall check 之前执行 — stall 消耗 1 cycle（有意设计） |

### 8.8 构建修复

```cmake
# CppTLM src/CMakeLists.txt: 启用 PIC（静态库链接到 .so 必须）
set_target_properties(cpptlm_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# PTX-EMU CMakeLists.txt: pin commit + 修正选项名
set(CPPTLM_COMMIT_HASH "73e5422" CACHE STRING "...")
# ExternalProject_Add: -DBUILD_TESTS=OFF (非 -DBUILD_TESTING=OFF)
```

