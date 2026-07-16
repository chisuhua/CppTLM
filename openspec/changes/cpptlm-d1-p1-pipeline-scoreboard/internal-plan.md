# Internal Plan: cpptlm-d1-p1-pipeline-scoreboard (P1)

> **Purpose**: Implementation sequencing + Adapter mapping + cross-repo commit sync points
> **Author**: CppTLM P1 implementer (autonomous from this plan)
> **Created**: 2026-07-16 (Day 3 / Metis Round 4 follow-up)

---

## 1. Adapter Mapping Table (4 Adapters)

| CppTLM 内部接口 | CppTLM 实现类 | PTX-EMU 上游接口 | Adapter 类 | 文件路径 |
|----------------|---------------|-----------------|-----------|---------|
| `IScoreboardInternal` | `ScoreboardTLM` | `IScoreboard` | `CppTLMScoreboardAdapter` | `include/tlm/gpu/adapter/cpptlm_scoreboard_adapter.{hh,cc}` |
| `IPipelineLatencyInternal` | `PipelineTLM` | `IPipelineLatencyProvider` | `CppTLMPipelineAdapter` | `include/tlm/gpu/adapter/cpptlm_pipeline_adapter.{hh,cc}` |
| `ITensorCoreTimingInternal` | `TensorCoreTLM` | `ITensorCoreTiming` | `CppTLMTensorCoreAdapter` | `include/tlm/gpu/adapter/cpptlm_tensor_core_adapter.{hh,cc}` |
| `IAsyncCompletion` | `AsyncCompletionAdapter` (Phase 8.B placeholder) | N/A | (same class) | `include/tlm/gpu/async_completion_adapter.{hh,cc}` |
| N/A (SMContext scheduling) | (uses CppTLM-side via WarpScheduler hook) | `SMContext::set_blocked_cycles_for_active` | `CppTLMWarpSchedulerAdapter` (optional) | `include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.{hh,cc}` |

**Construction pattern** (all Adapters):
- Adapter constructor takes **raw pointer** to CppTLM module (non-owning)
- PTX-EMU side retains ownership of Adapter instances
- CppTLM modules owned by `ModuleFactory` registry + `JsonIncluder` JSON config

---

## 2. 12-endpoint Enum Value Mapping Table

> **Critical**: Both sides MUST agree on integer values. Mismatch → 12-endpoint `static_assert` fails.

| Index | CppTLM `PipelineId` | PTX-EMU `PipelineId` | CppTLM `TcPrecision` | PTX-EMU `TcPrecision` |
|:-----:|--------------------|-----------------------|---------------------|------------------------|
| 0 | `P0_INT_FP32` | `P0_INT_FP32` | `FP4` | `FP4` |
| 1 | `V_SIMD` | `V_SIMD` | `FP6` | `FP6` |
| 2 | `P1_FP64` | `P1_FP64` | `FP8` | `FP8` |
| 3 | `P2_SFU` | `P2_SFU` | `FP16` | `FP16` |
| 4 | `P3_LSU` | `P3_LSU` | `BF16` | `BF16` |
| 5 | `P4_TC` | `P4_TC` | `TF32` | `TF32` |

**Verification**: `test/test_12_endpoint_static_assert.cc` 编译期 `static_assert` 全 12 项。

**Reference**: PTX-EMU `cpptlm-phase8b-injection-points/design.md:211-239`（待 PTX-EMU 团队确认值后定稿）。

---

## 3. PTX-EMU `cpptlm-phase8b-injection-points` Commit Sync Points

| 阶段 | CppTLM P1 任务 | 依赖 PTX-EMU 状态 |
|------|---------------|-------------------|
| Phase 1 | 实现 `IScoreboardInternal` + `ScoreboardTLM` + `CppTLMScoreboardAdapter` | 需 PTX-EMU `cpptlm-phase8b-injection-points` commit 锁定 `IScoreboard` 签名 |
| Phase 2 | 实现 `IPipelineLatencyInternal` + `PipelineTLM` + `CppTLMPipelineAdapter` | 需 PTX-EMU `IPipelineLatencyProvider` 签名冻结 |
| Phase 3 | 实现 `ITensorCoreTimingInternal` + `TensorCoreTLM` + `CppTLMTensorCoreAdapter` | 需 PTX-EMU `ITensorCoreTiming` 签名冻结 |
| Phase 4 | 实现 `AsyncCompletionAdapter` (placeholder) | 不阻塞，可独立推进 |
| Phase 5 | 实施 12-endpoint `static_assert` 集成测试 | 需 PTX-EMU 端 Adapter 测试基线 |
| Phase 6 | 双端 G-D5 microbenchmark 对齐（5 类，±15%） | 需 PTX-EMU `test_gpgpu_sim_comparison.py` 可执行 |

**Sync protocol**:
1. CppTLM P1 Phase 1 启动条件：PTX-EMU `cpptlm-phase8b-injection-points` 至少包含 `IScoreboard` 接口 PR
2. CppTLM P1 Phase 2-3 启动条件：PTX-EMU `IPipelineLatencyProvider` + `ITensorCoreTiming` 接口锁定
3. CppTLM P1 Phase 5 启动条件：PTX-EMU 端 Adapter 实现 PR 合并
4. CppTLM P1 Phase 6 启动条件：PTX-EMU + CppTLM 双端 G-D5 microbenchmark CI green

---

## 4. Implementation Order (4 sub-phases)

### Phase 1: Core Modules (3 days, independent of PTX-EMU P1)
- Day 1: `IScoreboardInternal` + `ScoreboardTLM` + 单测 `test/test_scoreboard_tlm.cc`
- Day 2: `IPipelineLatencyInternal` + `PipelineTLM` + 单测 `test/test_pipeline_tlm.cc`
- Day 3: `ITensorCoreTimingInternal` + `TensorCoreTLM` + 单测 `test/test_tensorcore_tlm.cc`

### Phase 2: Adapter Bridge (2 days, depends on PTX-EMU P1 interface freeze)
- Day 4: 3 Adapters (Scoreboard/Pipeline/TC) + 单测 `test/test_d1_adapters.cc`
- Day 5: `CppTLMWarpSchedulerAdapter` (optional) + integration test stub

### Phase 3: AsyncCompletion Placeholder (0.5 days, independent)
- Day 5.5: `AsyncCompletionAdapter` + placeholder 单测

### Phase 4: 12-endpoint Integration + Verification (1 day)
- Day 6: `test/test_12_endpoint_static_assert.cc` + 双端对齐 + G-D5 microbenchmark setup

**Total estimated**: ~6.5 working days
**Critical path**: PTX-EMU `cpptlm-phase8b-injection-points` 接口冻结时间

---

## 5. Open Questions (NOT blocking P1 Proposed)

| Q | 描述 | 解决时机 |
|---|------|---------|
| Q1 | PTX-EMU P1 接口 `IScoreboard` 命名是否包含 `set_blocked_cycles_for_active`？ | Phase 1 启动前 |
| Q2 | `IPipelineLatencyProvider` 是否需要 thread-safety？ | Phase 2 启动前 |
| Q3 | `ITensorCoreTiming` 是否包含 `latency_mnk` 三维查询？ | Phase 3 启动前 |
| Q4 | `AsyncCompletionAdapter` Phase 9+ 触发时机（PTX-EMU `pending_callbacks_` 集成）？ | Phase 9+ 规划时 |
| Q5 | CppTLM `PipelineId` 实际整数值是否需与 PTX-EMU 逐项 lock（当前假设 0..5）？ | Phase 1 启动前 |

**Resolution path**: 通过 GitHub issue 在 PTX-EMU `cpptlm-phase8b-injection-points` 下提问，或 CppTLM ↔ PTX-EMU 30-min sync call。
