# Proposal: cpptlm-d1-p1-pipeline-scoreboard — D1-Full Compute 注入 + Async Seam

> **Status**: Proposed（2026-07-15 Oracle + Metis 双审后拆分）
> **Created**: 2026-07-15
> **Parent change**: `cpptlm-f12b-ld-impl`（P0 MemoryBridge, 归档后本 change 启动）
> **Cross-project**: `PTX-EMU/openspec/changes/cpptlm-d1-full/`（姊妹 change）
> **Branch**: `feature/d1-full-impl`（P0 同分支，P0 归档后继续）
> **Worktree**: `CppTLM/.worktrees/feature-d1-full-impl`

## Design Revision (2026-07-17 HSK-4/5 后)

> 修订原因: 原 design 写于 2026-07-15（HSK-4 交付前），使用 `IScoreboardInternal` 等 Internal 接口 + 4 Adapter 翻译层。HSK-4 (`8acfd2d1`/`9e7361b9`/`463038e0`) 交付后，PTX-EMU 接口已是纯虚头文件（零依赖），可直接 vendor + 直接实现，无需 Internal 层和空壳 Adapter。

3 项简化:
1. Vendor 3 接口头文件到 `include/cudart/`（与 `cpptlm_bridge.h` 同策略），不再用 `IScoreboardInternal` 等 Internal 接口
2. 去掉 3 空壳 Adapter（Scoreboard/Pipeline/TC Adapter 是纯转发空壳）-- 3 核心模块直接 `public IScoreboard` / `IPipelineLatencyProvider` / `ITensorCoreTiming`
3. Phase 1 占位 latency = 1.0（G-D5 精确对齐留给 Phase 4）

影响:
- 文件数: 18 -> 15（省 3 个 Internal `.hh` + 3 个 Adapter `.hh/.cc`，加 3 个 vendor 头文件）
- LOC: ~1000 -> ~560（省 44%）
- Adapter 模式: 4 Adapter -> 1 optional WarpScheduler Adapter（待 Phase 4 评估）
- 见 `docs/superpowers/specs/2026-07-17-hsk-4-5-responses.md` HSK-4/5 响应 + `design.md` §1 修订

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

- **新增** `include/cudart/scoreboard_interface.h`（vendor from PTX-EMU `8acfd2d1`，HSK-4）
- **新增** `include/cudart/pipeline_interface.h`（vendor from PTX-EMU `9e7361b9`，HSK-4）
- **新增** `include/cudart/tensor_core_interface.h`（vendor from PTX-EMU `463038e0`，HSK-4）
- **新增** `include/tlm/gpu/scoreboard_tlm.hh` + `src/tlm/gpu/scoreboard_tlm.cc`（#C4a，直接 `public IScoreboard`）
- **新增** `include/tlm/gpu/pipeline_tlm.hh` + `src/tlm/gpu/pipeline_tlm.cc`（#C4b，直接 `public IPipelineLatencyProvider`）
- **新增** `include/tlm/gpu/tensor_core_tlm.hh` + `src/tlm/gpu/tensor_core_tlm.cc`（#C4c，直接 `public ITensorCoreTiming`）
- **新增** `include/tlm/gpu/async_completion_adapter.hh`（#C5 占位，已落地 commit `e69cd1d`）
- **新增** `test/test_scoreboard_tlm.cc`（#C4a 单测）
- **新增** `test/test_pipeline_tlm.cc`（#C4b 单测）
- **新增** `test/test_tensor_core_tlm.cc`（#C4c 单测）
- **新增** `test/test_12_endpoint_static_assert.cc`（12 端点编译期验证，用 vendor enum）
- **新增** `test/python/test_gpgpu_sim_comparison.py`（G-D5 5 类 microbenchmark，Phase 4）

### 修改产物

- **修改** `include/tlm/gpu/kernel_launch_tlm.hh`：激活 3 setter（P0 已预留 4 setter 接口，Phase 4 激活）
- **修改** `include/cudart/AGENTS.md`：记录 3 vendor 头文件 provenance（SHA-256 + commit hash）
- **修改** `CMakeLists.txt`：注册 3 核心模块到 `cpptlm_core` 静态库
- **修改** `test/CMakeLists.txt`：GLOB 自动发现 4 个新测试（`test_*.cc` 模式）

### 关键依赖

- **必须先完成**: P0 `cpptlm-f12b-ld-impl` 归档
- **必须先完成**: PTX-EMU 提交 P1 接口（scoreboard/pipeline/tensor_core 3 个 interface + SMContext 修改）
- **PTX-EMU 端同步**: PTX-EMU `feature/cpptlm-d1-full` 分支 #6-#9（PTX-EMU 侧 D1-Full Compute 接口 + SMContext 集成）

## Capabilities

### New Capabilities

- `cpptlm-scoreboard`: `ScoreboardTLM : public IScoreboard` - 64 entries hazard table, `has_free_entry()`/`allocate()`/`release()`/`tick()`（vendor `cudart/scoreboard_interface.h`）
- `cpptlm-pipeline`: `PipelineTLM : public IPipelineLatencyProvider` - 6 pipeline 抽象 (PipelineId 0-5), `get_fractional_cycles_by_type()`，Phase 1 占位 1.0
- `cpptlm-tensorcore`: `TensorCoreTLM : public ITensorCoreTiming` - 6 精度 (TcPrecision 0-5), `get_latency(precision)`，Phase 1 占位 1
- `cpptlm-12-endpoint-assert`: 12 端点 `static_assert`（PipelineId 6 + TcPrecision 6）编译期双向拦截
- `cpptlm-async-completion`: `IAsyncCompletion` 占位 Adapter（Phase 9+ TMA async 预留，已落地 `e69cd1d`）

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