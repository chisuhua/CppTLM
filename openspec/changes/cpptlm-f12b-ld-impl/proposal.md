# Proposal: cpptlm-f12b-ld-impl — F12b-LD MemoryBridge D1 实施

> **Status**: Proposed
> **Created**: 2026-07-15
> **Parent change**: `2026-06-24-gpu-soc-phase8b-core`（架构定义 — 不动）
> **Cross-project**: `PTX-EMU/openspec/changes/cpptlm-d1-full/`（姊妹 change）
> **Branch**: `feature/d1-full-impl`
> **Worktree**: `CppTLM/.worktrees/feature-d1-full-impl`

## Why

PTX-EMU 端已完成 ABI 头文件首发（commit `8dc000eca9f78e8ee017eafcb305eb4ca62ffd6d`），定义了 `CppTLMBridge` 抽象接口（5 虚方法 + `CPPTLMBRIDGE_VERSION=1`）。CppTLM 端需要实施对应的 `MemoryBridge` 实现，使 CppTLM 成为 PTX-EMU 协同仿真的**唯一时钟真相源（clock-of-truth）**。

本 change 实施 `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md` §2（P0 F12b-LD MemoryBridge）+ §3（P1 D1-Full Compute 注入）+ §4（P2 Phase 9+ Async Seam）的 **CppTLM 端 5 项任务**（#C1~#C5）。

**触发事件**:
1. **2026-07-14** — PTX-EMU 团队签发综合任务书，§2 列出 CppTLM 端 5 项配套任务（#C1~#C5）
2. **2026-07-14** — ADR-NV-02 D1-Lite → D1-Full 升级 Status Update
3. **2026-07-15** — PTX-EMU HSK-1 锁定（commit `8dc000ec` ABI header + `CPPTLMBRIDGE_VERSION=1`）
4. **2026-07-15** — CppTLM 端 HSK-1/2/3 全部 OK 确认（commit `7ab1ec1` 响应文档）
5. **2026-07-15** — Phase 0.5 baseline worktrees 双端就绪（commit `c89d996` 综合报告）

**前置 ADR**:
- [`docs/adr/ADR-NV-01-gpu-soc-architecture-target.md`](../../../../docs/adr/ADR-NV-01-gpu-soc-architecture-target.md) — D8 Phase-accurate 定位
- [`docs/adr/ADR-NV-02-phase8b-d1-strategy.md`](../../../../docs/adr/ADR-NV-02-phase8b-d1-strategy.md) — D1-Full 4 Adapter 策略
- [`docs/superpowers/specs/2026-07-15-cpptlm-hsk-response.md`](../../../../docs/superpowers/specs/2026-07-15-cpptlm-hsk-response.md) — HSK-1/2 OK + HSK-3 选项 1

## What Changes

### 新增产物

- **新增** `include/cudart/cpptlm_bridge.h`（vendor from PTX-EMU commit 8dc000ec）
- **新增** `include/cudart/AGENTS.md`（vendor provenance 记录）
- **新增** `include/tlm/gpu/memory_bridge.hh` + `src/tlm/gpu/memory_bridge.cc`（#C1）
- **新增** `include/tlm/gpu/kernel_launch_tlm.hh` + `src/tlm/gpu/kernel_launch_tlm.cc`（#C2）
- **新增** `include/tlm/gpu/{scoreboard,pipeline,tensorcore}_tlm.hh`（#C4 3 核心模块）
- **新增** `include/tlm/gpu/{scoreboard,pipeline,tensorcore}_internal.hh`（#C4 `tlm::I*Internal` 接口）
- **新增** `include/tlm/gpu/adapter/cpptlm_{warp_scheduler,scoreboard,pipeline,tensor_core}_adapter.hh`（#C3 4 Adapter）
- **新增** `include/tlm/gpu/async_completion_adapter.hh`（#C5 占位）
- **新增** `tests/unit/cpptlm/test_memory_bridge.cc`（#C1 单测）
- **新增** `tests/unit/cpptlm/test_kernel_launch.cc`（#C2 单测）
- **新增** `tests/integration/cpptlm/test_f12b_integration.cc`（P0 端到端）
- **新增** `tests/unit/cpptlm/test_{scoreboard,pipeline,tensorcore}_tlm.cc`（#C4 单测）
- **新增** `tests/unit/cpptlm/test_{scoreboard,pipeline,tensor_core}_adapter.cc`（#C3 单测）
- **新增** `tests/python/test_f12b_smoke.py`（G-F0 vector_add 烟雾测试）
- **新增** `tests/python/test_gpgpu_sim_comparison.py`（G-D5 5 类 microbenchmark）
- **新增** `configs/vector_add_n1024.json`（G-F0 测试配置）

### 修改产物

- **修改** `CMakeLists.txt`：注册 MemoryBridge + KernelLaunchTLM + 4 Adapter 目标
- **修改** `tests/CMakeLists.txt`：注册 7 个新测试目标
- **修改** `AGENTS.md`：CROSS-PROJECT INTEGRATION 段增加 D1 实施链接

### 关键依赖

- **必须先完成**: Phase 0.5 baseline worktree（`baseline/d1-full-prep` @ 683485f）就绪
- **PTX-EMU 端同步**: PTX-EMU `feature/cpptlm-d1-full` 分支并行实施 #1-#10
- **HSK 依赖**: HSK-1 commit 8dc000ec 已就绪 + HSK-2 ANTLR4 4.13.2 已就绪

## Capabilities

### New Capabilities

- `cpptlm-bridge-memorybridge`: `MemoryBridge : public CppTLMBridge` 实现 5 虚方法 + `kernel_args` deep-copy + NoC 路由查询
- `cpptlm-bridge-kernellaunch`: `KernelLaunchTLM : public ChStreamModuleBase` EventQueue 集成 + PTX-EMU 驱动 + FIFO 调度
- `cpptlm-bridge-3-core-modules`: ScoreboardTLM + PipelineTLM + TensorCoreTLM（`tlm::I*Internal` 接口）
- `cpptlm-bridge-4-adapters`: 4 个 CppTLM Adapter 层（WarpScheduler + Scoreboard + Pipeline + TC）+ 12 端点 `static_assert` 拦截
- `cpptlm-bridge-async-completion`: `IAsyncCompletion` 占位 Adapter（Phase 9+ TMA async 预留）

### Modified Capabilities

（空 — 本 change 不修改现有 openspec/specs/ 下的 capability，仅新增上 5 个）

## Impact

| 文件 | 类型 | 工时 | 验证 |
|------|------|:---:|------|
| `include/cudart/cpptlm_bridge.h` (vendor) | **新增** | 0.1d | SHA-256 字节级与 PTX-EMU 8dc000ec 一致 ✅ |
| `include/tlm/gpu/memory_bridge.hh` + `.cc` | **新增** | 1.0d | `cpptlm_tests [gpu][f12b]` PASS |
| `include/tlm/gpu/kernel_launch_tlm.hh` + `.cc` | **新增** | 1.0d | 单测 + 集成测试 |
| `include/tlm/gpu/{scoreboard,pipeline,tensorcore}_tlm.hh` | **新增** | 1.0d | 12 端点 `static_assert` 编译通过 |
| `include/tlm/gpu/adapter/cpptlm_*_adapter.hh` (4 个) | **新增** | 0.5d | D-PTX-1~6 ABI 验证 |
| `include/tlm/gpu/async_completion_adapter.hh` | **新增** | 0.1d | 编译通过 + 占位 |
| `CMakeLists.txt` | **修改** | 0.2d | `cmake --build build --target cpptlm_core` PASS |
| `tests/unit/cpptlm/test_*.cc` (7 个) | **新增** | 0.5d | `cpptlm_tests [gpu]` 全 PASS |
| `tests/integration/cpptlm/test_f12b_integration.cc` | **新增** | 0.5d | G-F0 vector_add 逐元素 diff |
| `tests/python/test_f12b_smoke.py` | **新增** | 0.3d | G-F0 延迟 ≤ 2× baseline |
| **合计** | | **~5.2d** | **~5 文件 + 7 tests + 5 PTX-EMU-side 协调点** |

**影响类别**:
- **新增公共 API 表面**: `CppTLMBridge` 实现（#C1 MemoryBridge）+ 5 内部模块 + 4 Adapter
- **现有行为变更**: `KernelLaunchTLM::tick()` 每个 EventQueue tick 调用 `MemoryBridge::submit/poll/synchronize`，PTX-EMU `g_cpptlm_bridge` 走新 MemoryBridge 路径
- **依赖关系**: vendor PTX-EMU 头文件（HSK-1 commit 8dc000ec），通过 `ExternalProject_Add` 动态拉取（HSK-3 选项 1，未来）
- **回退路径**: `g_cpptlm_bridge == nullptr` 时所有改动字节级回退到原行为（独立模式零退化）

## References

- **综合计划**: `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md` §2-§5
- **协作同步**: `docs/superpowers/specs/2026-07-01-f12b-ld-ptxemu-collaboration-sync.md`
- **ADR**: `docs/adr/ADR-NV-02-phase8b-d1-strategy.md` (R1-R9 风险 + G-D1~G-D8 验收)
- **PTX-EMU 端响应**: `docs/superpowers/specs/2026-07-15-cpptlm-hsk-response.md`
- **Phase 0.5 baseline**: `docs/superpowers/specs/2026-07-15-phase05-baseline-report.md`
- **PTX-EMU 端姊妹 change**: `https://github.com/chisuhua/PTX-EMU/blob/main/openspec/changes/cpptlm-d1-full/`
- **PTX-EMU 端 ADR**: `PTX-EMU/docs/adr/0021-cpptlm-d1-full-integration.md` (D-PTX-1~6 决策)

## Cross-Project Coordination Points

| 同步点 | 时间 | 双端要求 |
|--------|------|---------|
| D1 | 开工 | 双端 commit hash 同步（双向 HSK 重发） |
| D5 EOD | 🔵 G-F0 | 双端联合验证 vector_add 逐元素 diff |
| D8 | 🟣 G-D1~G-D4 | 双端 12 端点 enum `static_assert` 一致 |
| D14 | 🟢 G-D5 + G-D8 | 5 类 microbenchmark ±15% + 全量回归 |
