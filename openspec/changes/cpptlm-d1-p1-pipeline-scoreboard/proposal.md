# Proposal: cpptlm-d1-p1-pipeline-scoreboard — D1-Full Compute 注入 + Async Seam

> **Status**: Proposed（2026-07-15 Oracle + Metis 双审后拆分）
> **Created**: 2026-07-15
> **Parent change**: `cpptlm-f12b-ld-impl`（P0 MemoryBridge, 归档后本 change 启动）
> **Cross-project**: `PTX-EMU/openspec/changes/cpptlm-d1-full/`（姊妹 change）
> **Branch**: `feature/d1-full-impl`（P0 同分支，P0 归档后继续）
> **Worktree**: `CppTLM/.worktrees/feature-d1-full-impl`

## Why

CppTLM P0（MemoryBridge + KernelLaunchTLM 扩展）完成后，CppTLM 已成为时钟真相源。PTX-EMU `SMContext::exe_once()` 在每次 CppTLM tick 时被动调用。

本 change 实施 **D1-Full Compute 注入**（综合计划 §3 P1）：CppTLM 提供 Scoreboard hazard 检测、Pipeline 分数延迟、TensorCore timing，通过 Adapter 模式注入 PTX-EMU `SMContext`。最终使 PTX-EMU 指令执行周期精度由 CppTLM 控制。

**触发事件**:
1. **2026-07-15** — Oracle + Metis 双审决议：P0/P1 拆分为两个独立 change
2. **阻塞**: PTX-EMU 端尚未提交 P1 接口（`scoreboard_interface.h`/`pipeline_interface.h`/`tensor_core_interface.h` + `SMContext` 修改）

**前置**:
- ✅ P0 `cpptlm-f12b-ld-impl` 归档（`MemoryBridge` + `KernelLaunchTLM` 扩展完成）
- ⚠️ PTX-EMU 提交 P1 接口（`include/ptxsim/{scoreboard,pipeline,tensor_core}_interface.h` + `SMContext` 修改）
- ✅ HSK-1/2/3 已就绪（P0 已验证）

## What Changes

### 新增产物

- **新增** `include/tlm/gpu/scoreboard_tlm.hh` + `src/tlm/gpu/scoreboard_tlm.cc`（#C4a）
- **新增** `include/tlm/gpu/pipeline_tlm.hh` + `src/tlm/gpu/pipeline_tlm.cc`（#C4b）
- **新增** `include/tlm/gpu/tensor_core_tlm.hh` + `src/tlm/gpu/tensor_core_tlm.cc`（#C4c）
- **新增** `include/tlm/gpu/scoreboard_internal.hh`（`IScoreboardInternal` 内部接口）
- **新增** `include/tlm/gpu/pipeline_internal.hh`（`IPipelineLatencyInternal` 内部接口）
- **新增** `include/tlm/gpu/tensor_core_internal.hh`（`ITensorCoreTimingInternal` 内部接口）
- **新增** `include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.{hh,cc}`（#C3a）
- **新增** `include/tlm/gpu/adapter/cpptlm_scoreboard_adapter.{hh,cc}`（#C3b）
- **新增** `include/tlm/gpu/adapter/cpptlm_pipeline_adapter.{hh,cc}`（#C3c）
- **新增** `include/tlm/gpu/adapter/cpptlm_tensor_core_adapter.{hh,cc}`（#C3d）
- **新增** `include/tlm/gpu/async_completion_adapter.hh` + `.cc`（#C5 占位）
- **新增** `test/test_scoreboard_tlm.cc`（#C4a 单测）
- **新增** `test/test_pipeline_tlm.cc`（#C4b 单测）
- **新增** `test/test_tensor_core_tlm.cc`（#C4c 单测）
- **新增** `test/test_12_endpoint_static_assert.cc`（12 端点编译期验证）
- **新增** `test/test_d1_adapters.cc`（4 Adapter 单测）
- **新增** `test/python/test_gpgpu_sim_comparison.py`（G-D5 5 类 microbenchmark）

### 修改产物

- **修改** `include/tlm/gpu/kernel_launch_tlm.hh`：激活 4 Adapter setter（P0 已预留接口）
- **修改** `CMakeLists.txt`：注册 3 核心模块 + 4 Adapter 目标
- **修改** `test/CMakeLists.txt`：注册 5 个新测试目标

### 关键依赖

- **必须先完成**: P0 `cpptlm-f12b-ld-impl` 归档
- **必须先完成**: PTX-EMU 提交 P1 接口（scoreboard/pipeline/tensor_core 3 个 interface + SMContext 修改）
- **PTX-EMU 端同步**: PTX-EMU `feature/cpptlm-d1-full` 分支 #6-#9（PTX-EMU 侧 D1-Full Compute 接口 + SMContext 集成）

## Capabilities

### New Capabilities

- `cpptlm-scoreboard`: `ScoreboardTLM : public IScoreboardInternal` — ≥12 entries hazard table, `has_free_entry()`/`allocate()`/`release()`
- `cpptlm-pipeline`: `PipelineTLM : public IPipelineLatencyInternal` — 5+V pipeline 抽象, `get_fractional_cycles_by_type()`
- `cpptlm-tensorcore`: `TensorCoreTLM : public ITensorCoreTimingInternal` — 6 精度, `get_latency(precision)`
- `cpptlm-4-adapters`: WarpScheduler + Scoreboard + Pipeline + TC 4 个 Adapter — `WarpContext* ↔ uint32_t` 转换 + 12 端点 `static_assert`
- `cpptlm-async-completion`: `IAsyncCompletion` 占位 Adapter（Phase 9+ TMA async 预留）

## Impact

| 文件 | 类型 | 工时 | 验证 |
|------|------|:---:|------|
| `include/tlm/gpu/scoreboard_tlm.hh` + `.cc` | **新增** | 0.3d | `cpptlm_tests [gpu][sb]` PASS |
| `include/tlm/gpu/pipeline_tlm.hh` + `.cc` | **新增** | 0.3d | `cpptlm_tests [gpu][pipe]` PASS |
| `include/tlm/gpu/tensor_core_tlm.hh` + `.cc` | **新增** | 0.3d | `cpptlm_tests [gpu][tc]` PASS |
| `include/tlm/gpu/adapter/cpptlm_*_adapter.{hh,cc}` (4 个) | **新增** | 0.5d | `static_assert` 编译通过 + 单测 |
| `include/tlm/gpu/async_completion_adapter.hh` | **新增** | 0.1d | 编译通过 + 占位 |
| `test/test_12_endpoint_static_assert.cc` | **新增** | 0.1d | 编译通过 = 验证通过 |
| `CMakeLists.txt` | **修改** | 0.2d | `cmake --build build` PASS |
| `test/test_{scoreboard,pipeline,tensorcore}_tlm.cc` | **新增** | 0.4d | 单测 PASS |
| `test/test_d1_adapters.cc` | **新增** | 0.2d | 4 Adapter 单测 PASS |
| `test/python/test_gpgpu_sim_comparison.py` | **新增** | 0.3d | G-D5 5 类 microbenchmark |
| **合计** | | **~6.5d** | **12 文件 + 5 测试** |

**影响类别**:
- **新增公共 API 表面**: 3 核心模块 + 4 Adapter + IAsyncCompletion 占位
- **现有行为变更**: `KernelLaunchTLM::inject_into_sm_context()` 从空实现变为真实注入
- **依赖关系**: 依赖 PTX-EMU P1 接口（3 个纯虚基类 + SMContext 3 setter）
- **回退路径**: 4 setter 全 nullptr 时 PTX-EMU 零退化

## Cross-Project Coordination Points

| 同步点 | 时间 | 双端要求 |
|--------|------|---------|
| P1 启动 | PTX-EMU 交付 P1 接口 | 双端 12 端点 enum `static_assert` 一致 |
| P1 D3 | 🟣 G-D1~G-D4 | Scoreboard/Pipeline/TC 注入行为双端验证 |
| P1 D5 EOD | 🟢 G-D5 + G-D8 | 5 类 microbenchmark ±15% + exe_once chaos test + 全量回归 |
| P1 归档 | 🟢 交付 | P0+P1 联合验证通过 + `cpptlm_tests` 全 PASS |