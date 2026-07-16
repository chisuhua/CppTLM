# 2026-07-16 RFCs to PTX-EMU P1 注入接口（CppTLM D1-Full Compute）

> **用途**: CppTLM 端就 Phase 8.B D1-Full Compute 注入（P1）所需的 3 个 PTX-EMU 注入接口，向 PTX-EMU 团队发起正式 RFC 请求
> **与 P0 RFC 的区别**: 现有 [`2026-07-16-rfcs-to-ptxemu.md`](2026-07-16-rfcs-to-ptxemu.md) 涵盖 P0（MemoryBridge / synchronize_stream / kernel_args lifecycle），本文件聚焦 **P1 注入接口 + SMContext 修改 + 12-endpoint 双向断言 + 5 Open Questions**
> **关联**:
> - CppTLM P0 change (已归档): [`openspec/changes/archive/2026-07-16-cpptlm-f12b-ld-impl/`](../../../openspec/changes/archive/2026-07-16-cpptlm-f12b-ld-impl/)
> - CppTLM P1 change (待启动): [`openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/`](../../../openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/)
> - CppTLM P2 AsyncCompletion 占位 (已实现, 本 RFC 不阻塞): [`include/tlm/gpu/async_completion_adapter.hh`](../../../include/tlm/gpu/async_completion_adapter.hh)
> - PTX-EMU P1 姊妹 change (待启动): `PTX-EMU/openspec/changes/cpptlm-phase8b-injection-points/`
> - 综合设计参考: [`2026-07-03-ptxemu-phase8b-d1full-plan.md`](2026-07-03-ptxemu-phase8b-d1full-plan.md) (440 行, PTX-EMU 侧 6 工时设计)
> - 跨端综合任务书: [`2026-07-14-ptxemu-comprehensive-modification-plan.md`](2026-07-14-ptxemu-comprehensive-modification-plan.md) §3
> **发送时间**: 2026-07-16 (P0 归档后立即发送)
> **发送方**: CppTLM Team
> **接收方**: PTX-EMU 团队（review `cpptlm-phase8b-injection-points` change）
> **回复 SLA**: RFC-P1-001~003 ≤ 72h（接口签名锁定）；RFC-P1-004 (Q1-Q5) ≤ 1 周（开放问题）

---

## 0. 状态追踪

| RFC ID | 主题 | 推荐选项 | PTX-EMU 回复 | 状态 |
|:------:|------|----------|--------------|:----:|
| RFC-P1-001 | IScoreboard / IPipelineLatencyProvider / ITensorCoreTiming 接口签名 | 见 §1 完整签名 | ⏳ 待回复 | 🟡 Pending |
| RFC-P1-002 | SMContext 3 个 setter + 私有成员 + exe_once 三段式注入 | 见 §2 完整 diff | ⏳ 待回复 | 🟡 Pending |
| RFC-P1-003 | 12-endpoint 双向 `static_assert`（PipelineId 6 + TcPrecision 6） | 见 §3 完整清单 | ⏳ 待回复 | 🟡 Pending |
| RFC-P1-004 | 5 个 Open Questions（Q1-Q5） | 见 §4 详细问题 | ⏳ 待回复 | 🟡 Pending |

> **更新方式**: PTX-EMU 团队回复后，更新本表"PTX-EMU 回复"列并替换状态 emoji（🟢 Accepted / 🟠 Partial / 🔴 Rejected）。

---

## RFC-P1-001: 3 个注入接口签名

> **阻塞 CppTLM P1 Phase 1 启动**（阻塞任务见 §6 时间线）

### 1.1 `include/ptxsim/scoreboard_interface.h`

```cpp
#ifndef PTXSIM_SCOREBOARD_INTERFACE_H
#define PTXSIM_SCOREBOARD_INTERFACE_H

#include <cstdint>

/// IScoreboard: RAW hazard 检测注入接口
/// 由 PTX-EMU SMContext::exe_once() Step A 调用
class IScoreboard {
public:
    virtual ~IScoreboard() = default;

    /// 检查当前 warp 是否有空闲 entry（无 hazard）
    /// @return true = 有空闲 entry, false = 所有 entry 占用中（需 stall）
    virtual bool has_free_entry() const = 0;

    /// 分配 entry 跟踪 RAW hazard
    /// @param reg_id  目标寄存器 ID
    /// @param warp_id 请求分配的 warp
    /// @return true = 分配成功, false = 无空闲 entry (stall warp)
    virtual bool allocate(uint32_t reg_id, uint32_t warp_id) = 0;

    /// 释放 entry
    /// @param reg_id  寄存器 ID
    /// @param warp_id 释放的 warp
    /// @return true = 释放成功, false = 未找到对应 entry
    virtual bool release(uint32_t reg_id, uint32_t warp_id) = 0;

    /// 每个 tick 推进内部状态（可选，取决于实现策略）
    virtual void tick() = 0;
};

#endif
```

### 1.2 `include/ptxsim/pipeline_interface.h`

```cpp
#ifndef PTXSIM_PIPELINE_INTERFACE_H
#define PTXSIM_PIPELINE_INTERFACE_H

#include <cstdint>
#include <string>

/// PipelineId 枚举（5+V 抽象，6 值）
/// 重要：枚举值与 CppTLM 端 tlm::PipelineId 双向一致（见 RFC-P1-003）
enum class PipelineId : uint32_t {
    P0_INT_FP32 = 0,
    V_SIMD     = 1,
    P1_FP64    = 2,
    P2_SFU     = 3,
    P3_LSU     = 4,
    P4_TC      = 5
};

/// IPipelineLatencyProvider: 管线延迟查询注入接口
/// 由 PTX-EMU SMContext::exe_once() Step B 调用
class IPipelineLatencyProvider {
public:
    virtual ~IPipelineLatencyProvider() = default;

    /// 查询指定指令在指定管线上的分数 cycle 延迟
    /// @param instruction  指令名 (如 "FFMA", "LDG", "HMMA")
    /// @param pipe_id      执行管线
    /// @return 分数 cycle 延迟 (如 4.22)
    virtual double get_fractional_cycles(
        const std::string& instruction,
        PipelineId pipe_id) const = 0;

    /// 按 PTX StatementType 查询（用于 InstructionLatencyTable 集成）
    /// @param type  来自 ptx_ir/ptx_types.h
    /// @param pipe_id 执行管线
    /// @return 分数 cycle 延迟，返回 0.0 表示查表未命中
    virtual double get_fractional_cycles_by_type(
        int statement_type,
        PipelineId pipe_id) const = 0;
};

#endif
```

### 1.3 `include/ptxsim/tensor_core_interface.h`

```cpp
#ifndef PTXSIM_TENSOR_CORE_INTERFACE_H
#define PTXSIM_TENSOR_CORE_INTERFACE_H

#include <cstdint>

/// TcPrecision 枚举（6 精度）
/// 重要：枚举值与 CppTLM 端 tlm::TcPrecision 双向一致（见 RFC-P1-003）
enum class TcPrecision : uint32_t {
    FP4  = 0,
    FP6  = 1,
    FP8  = 2,
    FP16 = 3,
    BF16 = 4,
    TF32 = 5
};

/// ITensorCoreTiming: TC 延迟注入接口
/// 由 PTX-EMU SMContext::exe_once() Step B (TC 指令分支) 调用
class ITensorCoreTiming {
public:
    virtual ~ITensorCoreTiming() = default;

    /// 查询指定精度的 TC 延迟（cycles）
    virtual uint32_t get_latency(TcPrecision prec) const = 0;

    /// 查询指定精度的 TC 吞吐（cycles per instruction）
    virtual uint32_t get_throughput_cycles(TcPrecision prec) const = 0;

    /// 按矩阵维度查询延迟（未来支持 M/N/K 敏感延迟）
    virtual uint32_t get_latency_mnk(
        TcPrecision prec, uint32_t M, uint32_t N, uint32_t K) const {
        return get_latency(prec);  // 默认退化到精度统一延迟
    }
};

#endif
```

---

## RFC-P1-002: SMContext 修改（3 个 setter + exe_once 三段式注入）

### 2.1 修改 `include/ptxsim/sm_context.h`

```cpp
// 新增 include
#include "ptxsim/scoreboard_interface.h"
#include "ptxsim/pipeline_interface.h"
#include "ptxsim/tensor_core_interface.h"

class SMContext {
    // === 新增公开方法 ===

    /// 注入外部 Scoreboard（nullptr=禁用，默认）
    void set_scoreboard(IScoreboard* scoreboard);

    /// 注入外部管线延迟提供者（nullptr=使用内置 InstructionLatencyTable）
    void set_pipeline_latency_provider(IPipelineLatencyProvider* provider);

    /// 注入外部 TC timing（nullptr=使用内置延迟）
    void set_tensor_core_timing(ITensorCoreTiming* tc);

    // === 新增私有成员 ===
private:
    IScoreboard* scoreboard_ = nullptr;
    IPipelineLatencyProvider* pipeline_provider_ = nullptr;
    ITensorCoreTiming* tensor_core_timing_ = nullptr;
};
```

### 2.2 修改 `src/ptxsim/core/sm_context.cpp` (exe_once 方法内)

**当前 exe_once() 流程**:
```
1. warp_scheduler_->schedule_next()  → warp
2. warp->execute_warp_instruction(stmt)
3. update_state()
```

**注入后流程** (+3 个钩子):
```
1. warp_scheduler_->schedule_next()  → warp
1.5 [NEW] if scoreboard_:
        if !scoreboard_->has_free_entry() → stall warp, skip to next
        for each dest reg in stmt:
          if !scoreboard_->allocate(reg_id, warp_id) → stall
2. [MODIFIED] if pipeline_provider_:
       latency = pipeline_provider_->get_fractional_cycles_by_type(stmt.type, pipe_id)
       warp->set_blocked_cycles(static_cast<uint32_t>(latency))
     elif tensor_core_timing_ && is_tc_instruction(stmt):
       latency = tensor_core_timing_->get_latency(precision)
       warp->set_blocked_cycles(latency)
2.5 [NEW] warp->execute_warp_instruction(stmt)   ← 原执行逻辑
3. [NEW] if scoreboard_:
       for each dest reg in stmt: scoreboard_->release(reg_id, warp_id)
4. scoreboard_->tick()  (推进内部计数器)
5. update_state()
```

### 2.3 关键约束

- 步骤 1.5~2 之间的 stall 逻辑**不改变** PTX-EMU 原有执行语义——仅在原有基础上增加 scoreboard 检查
- `pipeline_provider_` 返回值**覆盖** `InstructionLatencyTable` 查表结果（而非叠加）
- 如果 `pipeline_provider_` 和 `tensor_core_timing_` 都为 nullptr，退化为原有 `InstructionLatencyTable` 查表

### 2.4 PTX-EMU 端验证测试（建议新增）

```cpp
// test/test_scoreboard_injection.cpp
TEST_CASE("Scoreboard injection: stall on 13th allocate") {
    SMContext sm(64, 2048, 64*1024, 0);
    MockScoreboardLimited sb(12);  // 12 entries, 13th fails
    sm.set_scoreboard(&sb);
    // 验证: 第 13 条指令 stall, warp blocked_cycles > 0
}

TEST_CASE("Pipeline injection: override FFMA latency") {
    SMContext sm(...);
    MockPipelineLatencyProvider plp;
    plp.set_latency("FFMA", PipelineId::P0_INT_FP32, 4.22);
    sm.set_pipeline_latency_provider(&plp);
    // 验证: 执行 FFMA 后 warp blocked_cycles == 4 (ceiling of 4.22)
}

TEST_CASE("Injection points: nullptr fallback to defaults") {
    SMContext sm(...);
    // 三个 setter 全 nullptr → 行为与注入前完全一致
}
```

**PTX-EMU 端总工时估算**: 2 天（3 接口 0.3d + SMContext 改造 1.1d + 测试 0.5d）

---

## RFC-P1-003: 12-endpoint 双向 `static_assert`

### 3.1 PipelineId 6 端点（双端 enum 值必须逐项 lock）

| Index | CppTLM `tlm::PipelineId` | PTX-EMU `PipelineId` | 一致? |
|:-----:|------------------------|---------------------|:-----:|
| 0 | `P0_INT_FP32` | `P0_INT_FP32` | ⏳ 待 PTX-EMU 锁定 |
| 1 | `V_SIMD` | `V_SIMD` | ⏳ 待 PTX-EMU 锁定 |
| 2 | `P1_FP64` | `P1_FP64` | ⏳ 待 PTX-EMU 锁定 |
| 3 | `P2_SFU` | `P2_SFU` | ⏳ 待 PTX-EMU 锁定 |
| 4 | `P3_LSU` | `P3_LSU` | ⏳ 待 PTX-EMU 锁定 |
| 5 | `P4_TC` | `P4_TC` | ⏳ 待 PTX-EMU 锁定 |

### 3.2 TcPrecision 6 端点（双端 enum 值必须逐项 lock）

| Index | CppTLM `tlm::TcPrecision` | PTX-EMU `TcPrecision` | 一致? |
|:-----:|--------------------------|----------------------|:-----:|
| 0 | `FP4` | `FP4` | ⏳ 待 PTX-EMU 锁定 |
| 1 | `FP6` | `FP6` | ⏳ 待 PTX-EMU 锁定 |
| 2 | `FP8` | `FP8` | ⏳ 待 PTX-EMU 锁定 |
| 3 | `FP16` | `FP16` | ⏳ 待 PTX-EMU 锁定 |
| 4 | `BF16` | `BF16` | ⏳ 待 PTX-EMU 锁定 |
| 5 | `TF32` | `TF32` | ⏳ 待 PTX-EMU 锁定 |

### 3.3 双端 `static_assert` 测试（编译期拦截）

CppTLM 端（[`test/test_12_endpoint_static_assert.cc`](../../../test/test_12_endpoint_static_assert.cc) 已规划）:
```cpp
// PipelineId verification
static_assert(static_cast<int>(PipelineId::P0_INT_FP32) == 0);
static_assert(static_cast<int>(PipelineId::V_SIMD)      == 1);
static_assert(static_cast<int>(PipelineId::P1_FP64)     == 2);
static_assert(static_cast<int>(PipelineId::P2_SFU)      == 3);
static_assert(static_cast<int>(PipelineId::P3_LSU)      == 4);
static_assert(static_cast<int>(PipelineId::P4_TC)       == 5);

// TcPrecision verification
static_assert(static_cast<int>(TcPrecision::FP4)  == 0);
static_assert(static_cast<int>(TcPrecision::FP6)  == 1);
static_assert(static_cast<int>(TcPrecision::FP8)  == 2);
static_assert(static_cast<int>(TcPrecision::FP16) == 3);
static_assert(static_cast<int>(TcPrecision::BF16) == 4);
static_assert(static_cast<int>(TcPrecision::TF32) == 5);
```

**PTX-EMU 端建议**: 同样新增 `test/test_12_endpoint_static_assert.cpp`，编译失败 = 双端不一致。

---

## RFC-P1-004: 5 个 Open Questions

> 来源: CppTLM [`openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/internal-plan.md §5`](../../../openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/internal-plan.md)

| Q# | 问题 | 推荐答案 | 阻塞 |
|:--:|------|----------|:----:|
| **Q1** | PTX-EMU `IScoreboard` 命名是否包含 `set_blocked_cycles_for_active()`？ | ❌ 不在接口中。该方法是 `WarpScheduler` 子类行为，不属于 hazard 检测接口 | Phase 1 启动前 |
| **Q2** | `IPipelineLatencyProvider` 是否需要 thread-safety？ | ❌ Phase 8.B 单线程假设（与 F12b-LD 一致），Phase 9+ 添加 mutex | Phase 2 启动前 |
| **Q3** | `ITensorCoreTiming` 是否包含 `latency_mnk` 三维查询？ | ✅ 默认实现 `return get_latency(prec)` 退化，CppTLM 端按需 override | Phase 3 启动前 |
| **Q4** | `AsyncCompletionAdapter` Phase 9+ 触发时机（PTX-EMU `pending_callbacks_` 集成）？ | CppTLM 端已实现占位（[`async_completion_adapter.hh`](../../../include/tlm/gpu/async_completion_adapter.hh)），Phase 9+ 由 PTX-EMU 触发 fire_completion() 真正调用 | Phase 9+ 规划时 |
| **Q5** | CppTLM `PipelineId` 实际整数值是否需与 PTX-EMU 逐项 lock（当前假设 0..5）？ | ✅ 是。RFC-P1-003 已枚举，**必须逐项 lock** | Phase 1 启动前 |

---

## 5. 同步协议（双端 Sync Points）

| 阶段 | CppTLM P1 任务 | 依赖 PTX-EMU 状态 |
|------|---------------|-------------------|
| **Phase 1 启动** | 实现 `IScoreboardInternal` + `ScoreboardTLM` + `CppTLMScoreboardAdapter` | 需 PTX-EMU `cpptlm-phase8b-injection-points` commit 锁定 `IScoreboard` 签名（RFC-P1-001 §1.1） |
| **Phase 2 启动** | 实现 `IPipelineLatencyInternal` + `PipelineTLM` + `CppTLMPipelineAdapter` | 需 PTX-EMU `IPipelineLatencyProvider` 签名冻结 + `PipelineId` 锁定（RFC-P1-001 §1.2 + RFC-P1-003 §3.1） |
| **Phase 3 启动** | 实现 `ITensorCoreTimingInternal` + `TensorCoreTLM` + `CppTLMTensorCoreAdapter` | 需 PTX-EMU `ITensorCoreTiming` 签名冻结 + `TcPrecision` 锁定（RFC-P1-001 §1.3 + RFC-P1-003 §3.2） |
| **Phase 4 启动** | 12-endpoint `static_assert` 集成测试 | 需 PTX-EMU 端 Adapter 测试基线 + 双端 enum 一致（RFC-P1-003 §3.3） |
| **Phase 5 启动** | 双端 G-D5 microbenchmark 对齐（5 类，±15%） | 需 PTX-EMU `test_gpgpu_sim_comparison.py` 可执行 |

---

## 6. 时间线 + 阻塞关系

```
Day 0 (2026-07-16, 今日):
   - CppTLM 端 P0 归档 ✅ (b94eccc)
   - CppTLM 端 P2 AsyncCompletion 占位实现 ✅ (e69cd1d)
   - 本 RFC 发送 (RFC-P1-001~004)

Day 1-2 (RFC-P1-001/002/003 锁定):
   PTX-EMU: 实施 3 接口 + SMContext 修改 + Mock 测试 (2d)
   CppTLM: 等待接口冻结（无主动开发）

Day 3-4 (Q1-Q5 解决 + Phase 1 启动):
   PTX-EMU: 解决 Q1-Q5 + 12-endpoint 双向 static_assert
   CppTLM: 启动 Phase 1 (ScoreboardTLM + PipelineTLM + TensorCoreTLM)

Day 5-6 (Phase 1 完成 + Phase 2 启动):
   PTX-EMU: 接受 Adapter 层 PR review
   CppTLM: 完成 3 核心模块 + 启动 Phase 2 (4 Adapter)

Day 7-8 (Phase 2/3/4 完成):
   CppTLM: 4 Adapter + 12-endpoint static_assert + AsyncCompletion 集成

Day 9-12 (Phase 5: 集成验证 + G-D5):
   双端: 5 类 microbenchmark vs gpgpu-sim ±15% + G-D8 chaos test

Day 13 (M2 验收 + Oracle 审查 + OpenSpec 归档):
   双端: P1 联合验收通过 → cpptlm-d1-p1-pipeline-scoreboard 归档
```

**关键路径**: PTX-EMU `cpptlm-phase8b-injection-points` 接口冻结时间（Day 1-2）

---

## 7. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: 12 端点 enum 值与 PTX-EMU 端不一致 | 低 | 中 | `static_assert` 编译期拦截 + 双端 CI 双重断言（RFC-P1-003） |
| **R2**: D1-Full fast/slow path 注入遗漏 | 中 | 高 | Adapter nullptr fallback + 强测试覆盖两条路径（G-D6, G-D7） |
| **R3**: Scoreboard stall/release 状态不一致 | 中 | 高 | chaos test: stall→re-schedule→release→re-issue 完整循环（G-D8） |
| **R4**: PTX-EMU P1 接口交付延迟 | 高 | 高 | P0 已独立交付，CppTLM Phase 3 (AsyncCompletion) 不依赖 PTX-EMU P1；Phase 1+2 可并行 |

---

## 8. 验收清单（PTX-EMU 团队回复模板）

请 PTX-EMU 团队对每条 RFC 回复 **Accepted / Partial / Rejected** + 简短说明：

```
[RFC-P1-001] 3 接口签名
   Status: <Accepted/Partial/Rejected>
   Note: <如有修改请提供 diff>

[RFC-P1-002] SMContext 修改 + exe_once 三段式注入
   Status: <Accepted/Partial/Rejected>
   Note: <如有修改请提供 diff>

[RFC-P1-003] 12-endpoint 双向 static_assert
   Status: <Accepted/Partial/Rejected>
   PipelineId 锁定值: <请确认 P0_INT_FP32=0, V_SIMD=1, P1_FP64=2, P2_SFU=3, P3_LSU=4, P4_TC=5>
   TcPrecision 锁定值: <请确认 FP4=0, FP6=1, FP8=2, FP16=3, BF16=4, TF32=5>

[RFC-P1-004] 5 Open Questions
   Q1 (set_blocked_cycles_for_active 归属): <Answer>
   Q2 (thread-safety): <Answer>
   Q3 (latency_mnk 默认行为): <Answer>
   Q4 (AsyncCompletion Phase 9+ 触发): <Answer>
   Q5 (PipelineId 整数 lock): <Answer>

预期交付:
   - 3 接口 + SMContext 修改 commit hash (PTX-EMU)
   - Mock 测试 PR (test/test_scoreboard_injection.cpp 等)
   - 预期工时: 2 天
```

---

## 9. 附录

### 9.1 CppTLM 端 P1 实施产物（已规划，未启动）

- 3 核心模块: `include/tlm/gpu/scoreboard_tlm.{hh,cc}` + `pipeline_tlm.{hh,cc}` + `tensor_core_tlm.{hh,cc}`
- 4 Adapter: `include/tlm/gpu/adapter/cpptlm_{scoreboard,pipeline,tensor_core,warp_scheduler}_adapter.{hh,cc}`
- 12-endpoint `static_assert`: `test/test_12_endpoint_static_assert.cc`
- 5 个单测: `test/test_{scoreboard,pipeline,tensor_core}_tlm.cc` + `test_d1_adapters.cc`
- P2 (已落地): [`include/tlm/gpu/async_completion_adapter.hh`](../../../include/tlm/gpu/async_completion_adapter.hh) + 单测

### 9.2 CppTLM P1 工期估算

总 ~6.5d（与 internal-plan.md Day-by-Day 一致）:
- Phase 1 (3 核心模块): ~1d
- Phase 2 (4 Adapter): ~0.5d
- Phase 3 (AsyncCompletion 占位): ~1h ✅ 已完成
- Phase 4 (集成验证 + KernelLaunchTLM 激活): ~0.5d
- + 子测编写 + 联调 buffer: ~4d

### 9.3 跨端 commit 引用

- CppTLM P0 archive: [`openspec/changes/archive/2026-07-16-cpptlm-f12b-ld-impl/`](../../../openspec/changes/archive/2026-07-16-cpptlm-f12b-ld-impl/)
- CppTLM P2 commit: `e69cd1d feat(tlm/gpu): AsyncCompletionAdapter placeholder`
- PTX-EMU HSK-1 commit: `8dc000eca9f78e8ee017eafcb305eb4ca62ffd6d` (P0 ABI v1)
- PTX-EMU P1 接口 (待提交): `cpptlm-phase8b-injection-points` change

---

*本 RFC 待 PTX-EMU 团队确认后，CppTLM P1 Phase 1 启动*
*发送时间: 2026-07-16 / 回复 SLA: RFC-P1-001~003 ≤ 72h / RFC-P1-004 ≤ 1 周*