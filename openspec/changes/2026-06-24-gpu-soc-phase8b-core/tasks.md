# Tasks: gpu_soc Phase 8.B — 核心仿真

> **Status**: 🟢 Ready to Start (Phase 8.A ✅ Archived, `e8280fe`)
> **Parent**: `proposal.md` + `design.md` (2026-06-24-gpu-soc-phase8b-core)
> **Revision**: 2026-07-15 — **重构为 P0/P1/P2/P3 阶段命名（修复 C1 P0：与综合计划实施顺序对齐）**
> **关联**: [`docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md`](../../../docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md) §2-§5

## 阶段总览（与综合计划 P0/P1/P2/P3 对齐）

| 阶段 | 内容 | 时长 | 阻塞依赖 |
|------|------|:---:|----------|
| **🔴 P0** | F12b-LD MemoryBridge (#C1, #C2) | ~5 天 | **阻塞所有后续阶段** |
| **P1** | D1-Full Compute 注入 (Task 9-15) | ~2.5 天 | 依赖 P0 接口 |
| **P2** | Phase 9+ Async Seam (#C5) | ~1 小时 | 依赖 P1 #C3 |
| **P3** | 集成验证 (Task 16 + G-F0 + 5 microbenchmark) | ~1 周 | 依赖 P0 + P1 |

## P0: F12b-LD MemoryBridge（🔴 关键路径，~5 天）

> 与综合计划 §2 完全对齐。新增的 C 端任务（#C1, #C2）必须先于 P1 实施。
> **改动理由（原 C1 P0）**：原 tasks.md 将 6 模块独立实现（Task 9-14）放在"阶段 A"先做，将 F12b-LD 集成放在"阶段 B"后做。这意味着工程师开工后会先实现 6 个独立模块（ScoreboardTLM 等），再尝试 F12b-LD 集成——但 MemoryBridge（#C1）尚未存在，集成测试无法进行。本重构将 MemoryBridge 提升到 P0，确保实施顺序与综合计划一致。

- [ ] **Task #C1**: MemoryBridge 实现
  - 文件: `src/tlm/gpu/memory_bridge.{hh,cc}` (新增)
  - 实现: 5 虚方法（`version` / `submit_kernel` / `poll_kernel` / `synchronize_stream` / `global_access`）+ `kernel_args` deep-copy
  - 依赖: PTX-EMU HSK-1 头文件（已在 PTX-EMU commit `8dc000ec`）
  - Commit: `feat(tlm/gpu): MemoryBridge implements CppTLMBridge (P0)`

- [ ] **Task #C2**: KernelLaunchTLM 实现
  - 文件: `src/tlm/gpu/kernel_launch_tlm.cc` (新增)
  - 实现: EventQueue 集成 + PTX-EMU 驱动 + `KernelLaunchRequest` 数据结构 + FIFO 调度
  - 依赖: #C1
  - Commit: `feat(tlm/gpu): KernelLaunchTLM EventQueue + PTX-EMU driver (P0)`

### P0 验收门

- [ ] **G-F0** `vector_add` 烟雾测试 — 输出与 baseline 逐元素一致，延迟 ≤ 2× baseline
- [ ] **G-F1** `g_cpptlm_bridge == nullptr` 时 PTX-EMU 零退化
- [ ] **G-F2** 有 bridge 时 `cudaLaunchKernel` 立即返回
- [ ] **G-F3** `global_access()` 延迟与 CppTLM NoC 路由延迟一致
- [ ] **G-F4** `cudaDeviceSynchronize` 正确等待所有 kernel 完成
- [ ] **G-F5** F12b-LD 集成测试: `cpptlm_tests [gpu][f12b]` 全 pass

## P1: D1-Full Compute 注入（~2.5 天）

> 与综合计划 §3 对齐。包含 6 模块独立实现 + 4 Adapter 桥接。
> **关联**：[`docs/superpowers/specs/2026-07-03-ptxemu-phase8b-d1full-plan.md`](../../../docs/superpowers/specs/2026-07-03-ptxemu-phase8b-d1full-plan.md) §2.3 设计参考

### P1.1 6 模块独立实现（原 Task 9-14，原"阶段 A"）

- [ ] **Task 9**: ScoreboardTLM ≥12 entries `tlm::IScoreboardInternal`
  - 产出: `include/tlm/gpu/scoreboard_interface.hh` (新增) + `scoreboard_tlm.hh/.cc` (实现接口) + test
  - Commit: `feat(tlm/gpu): ScoreboardTLM implements IScoreboardInternal (P1)`

- [ ] **Task 10a**: WarpSchedulerTLM — 重命名 MinimalWarpSchedulerTLM + CGGTY 5-warp 阈值 + priority 队列
  - 保留 `uint32_t` 接口（F12a 已对齐），保留旧注册项 + `[[deprecated]]`
  - 产出: `include/tlm/gpu/warp_scheduler_tlm.hh/.cc` (重命名) + 修改 test
  - Commit: `feat(tlm/gpu): WarpSchedulerTLM rename + CGGTY threshold (P1)`

- [ ] **Task 10b**: CppTLMWarpSchedulerAdapter — 桥接 `WarpScheduler` ↔ `WarpSchedulerTLM`
  - `WarpContext*` ↔ `uint32_t` 转换，额外 3 方法默认实现
  - 产出: `include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.hh/.cc` (新增) + 对应 test
  - 依赖: Task 10a
  - Commit: `feat(tlm/gpu): CppTLMWarpSchedulerAdapter (P1)`

- [ ] **Task 11**: PipelineTLM 5+V 抽象 `tlm::IPipelineLatencyInternal`
  - 产出: `include/tlm/gpu/pipeline_interface.hh` (新增) + `pipeline_tlm.hh/.cc` (实现接口) + test
  - Commit: `feat(tlm/gpu): PipelineTLM implements IPipelineLatencyInternal (P1)`

- [ ] **Task 12**: TensorCoreTLM 6 精度 `tlm::ITensorCoreTimingInternal`
  - 产出: `include/tlm/gpu/tensor_core_interface.hh` (新增) + `tensor_core_tlm.hh/.cc` (实现接口) + test
  - Commit: `feat(tlm/gpu): TensorCoreTLM implements ITensorCoreTimingInternal (P1)`

- [ ] **Task 13**: L2PartitionTLM multi-slice 近/远分区
  - 产出: `include/tlm/gpu/l2_partition_tlm.hh/.cc` + test
  - Commit: `feat(tlm/gpu): L2PartitionTLM multi-slice (P1)`

- [ ] **Task 14**: SubCoreTLM black-box pipe 封装
  - 内部组合 Task 10-13 模块，`set_sm_context()` 预留 D1 模式
  - 产出: `include/tlm/gpu/subcore_tlm.hh/.cc` + test
  - 注册: `REGISTER_CHSTREAM(SubCoreTLM)` + ComputeCluster 集成
  - Commit: `feat(tlm/gpu): SubCoreTLM SMContext wrapper + standalone mode (P1)`

### P1.2 Adapter 层（原 Task 15，从"阶段 B"上移到 P1）

> 关键路径调整：原"阶段 B"将 Adapter 放在 F12b-LD 之后，但 D1-Full Compute 注入（综合计划 §3）需要 4 个 Adapter 全部就绪。Adapter 与 6 模块并行开发（依赖 PTX-EMU `scoreboard_interface.h` 等 P1 端接口）。

- [ ] **Task 15**: 3 Adapter 层（Scoreboard / Pipeline / TensorCore）
  - 依赖: PTX-EMU `scoreboard_interface.h` / `pipeline_interface.h` / `tensor_core_interface.h`（由 PTX-EMU §3 #6 提供）+ Task 9-12 完成
  - 产出: `include/tlm/gpu/adapter/` 下 3 个 Adapter + 对应 test
  - Commit: `feat(tlm/gpu): PTX-EMU adapters for Scoreboard/Pipeline/TensorCore (P1)`

- [ ] **Task #C3**: 4 Adapter 集中注册
  - 文件: `include/tlm/gpu/adapter/` 全部 4 个 Adapter
  - 包含: WarpScheduler (10b) + Scoreboard + Pipeline + TensorCore
  - 12 端点（PipelineId 6 + TcPrecision 6）`static_assert` 编译期拦截
  - Commit: `feat(tlm/gpu): 4 Adapter centralized with 12-endpoint static_assert (P1)`

- [ ] **Task #C4**: 3 核心模块 + `tlm::I*Internal` 接口
  - 文件: `include/tlm/gpu/{scoreboard,pipeline,tensorcore}_tlm.hh` (改)
  - 与 Task 9-12 协同
  - Commit: `feat(tlm/gpu): 3 core modules + Internal interfaces (P1)`

### P1 验收门

- [ ] **G-D1** 3 纯虚接口编译通过，无 CppTLM 头文件污染 PTX-EMU
- [ ] **G-D2** `set_blocked_cycles_for_active()` 对 warp 内活跃线程正确设置延迟
- [ ] **G-D3** `exe_once()` Step A/B/C 注入后 `blocked_cycles_remaining` 与 CppTLM 独立模型差值 ≤ 1 cycle
- [ ] **G-D4** 4 Adapter `static_assert` 12 端点 0-5 双向一致
- [ ] **G-D6** 4 setter 全 nullptr 时 PTX-EMU 零退化
- [ ] **G-D7** scoreboard/pipeline/TC 任意 nullptr 时回退到 `InstructionLatencyTable`

## P2: Phase 9+ Async Seam（~1 小时）

> 与综合计划 §4 对齐。预埋 TMA async 完成回调 seam，避免 Phase 9+ 架构重构。

- [ ] **Task #C5**: AsyncCompletionAdapter 占位实现
  - 文件: `include/tlm/gpu/async_completion_adapter.hh` (新增)
  - 实现: 空实现，预留 `register_completion_callback` 接口
  - Commit: `feat(tlm/gpu): AsyncCompletionAdapter placeholder (P2)`

### P2 验收门

- [ ] 独立模式 `async_completion_ = nullptr` 无影响
- [ ] Phase 9+ 启用时只需替换 Adapter 实现，无需修改 §3 任何代码

## P3: 集成验证（~1 周）

> 与综合计划 §5 对齐。E2E CUDA kernel 验证 + gpgpu-sim 对照。

- [ ] **Task 16**: Level 1/2/3 集成测试 + 6 docs + M2 验收
  - **Level 1**: 合成 workload（`test_gpu_soc_phase8b.cc`）
  - **Level 2**: 真实 CUDA kernel（F12b 后）+ 5 类 microbenchmark
  - **Level 3**: gpgpu-sim 对照（`test_gpgpu_sim_comparison.py`）

#### 5 类 microbenchmark vs gpgpu-sim ±15%

| 场景 | baseline | 验收 |
|------|:---:|:---:|
| GEMM (FP16, M=N=K=4096) | gpgpu-sim 700 GB/s | ±15% |
| FlashAttn (b=8, h=16, seq=512) | 470 GB/s | ±15% |
| vector_add (n=1024²) | 1176 GB/s | ±15% |
| stencil (3D 7-point, N=512³) | 940 GB/s | ±15% |
| sparse SpMV (10k×10k, 0.01) | 230 GB/s | ±15% |

- 1 GB203 × 1M < 60s
- 6 个微架构 doc + `docs_sync_check.sh --strict` 0 missing
- Commit: `feat(gpu_soc): Phase 8.B 5 microbenchmarks + gpgpu-sim ±15% bandwidth + M2 docs (P3)`

### P3 验收门

- [ ] **G-D5** 5 类 microbenchmark vs gpgpu-sim ±15%
- [ ] **G-D8** `exe_once()` scoreboard stall → re-schedule → release → re-issue 完整循环无状态不一致
- [ ] **G4** 1 GB203 × 1M < 60s
- [ ] **G5** 6 个微架构 doc + docs_sync 0 missing

## 综合验收 Gates

- [ ] **G0** P0/P1/P2/P3 所有阶段完成
- [ ] **G1** `[gpu][subcore][sched][sb][tc][pipe][l2]` 全 pass
- [ ] **G2** `test_gpu_soc_phase8b.cc` Level 1 合成 workload 5 类 microbenchmark 跑通
- [ ] **G3** `test_gpgpu_sim_comparison.py` 带宽 ±15%
- [ ] **G6** apu_soc 兼容性全绿（不破坏 Phase 8.A）
- [ ] **G7** Adapter 编译通过（与 PTX-EMU 头文件联编）
- [ ] **G-F0** `vector_add` 烟雾测试通过（P0 阶段交付门）
- [ ] **`docs_sync_check.sh --strict` 0 missing**（pre-commit 强制）

## 实施后节点

- [ ] **Oracle 审查**（调 oracle subagent）
- [ ] **OpenSpec 归档** → `openspec/changes/archive/2026-06-24-gpu-soc-phase8b-core/`

## 依赖关系图

```
2026-06-24-gpu-soc-phase8a-infra (M1) ✅ 已完成
         ↓
    PTX-EMU HSK-1 (8dc000ec) ✅ 已 commit
         ↓
┌─────────────────────┐
│ 🔴 P0: MemoryBridge │ ← #C1 + #C2（~5 天）
│  G-F0~G-F5 验证     │
└──────────┬──────────┘
           ↓
┌─────────────────────────────────────────────┐
│ P1: D1-Full Compute 注入（~2.5 天）          │
│  P1.1 6 模块 (Task 9-14) — 与 P1.2 Adapter 并行 │
│  P1.2 4 Adapter (Task 15 + #C3 + #C4)        │
│  G-D1~G-D4, G-D6, G-D7 验证                  │
└──────────┬──────────────────────────────────┘
           ↓
┌──────────────────────┐
│ P2: Async Seam (1h)  │ ← #C5 占位
└──────────┬───────────┘
           ↓
┌──────────────────────────────────────────────┐
│ P3: 集成验证（~1 周）                          │
│  G-D5, G-D8, G4, G5 验证                      │
│  5 类 microbenchmark vs gpgpu-sim ±15%        │
└──────────────────────────────────────────────┘
           ↓
   后续 change: 2026-06-24-gpu-soc-phase8c-advanced
```

## 详细依赖

- **必须先完成**：`2026-06-24-gpu-soc-phase8a-infra`（M1）✅ Archived
- **P0 (MemoryBridge)** 依赖：PTX-EMU HSK-1 头文件（已 commit `8dc000ec`，待 push）
- **P1 Task 10b 依赖**：Task 10a
- **P1 Task 15 依赖**：PTX-EMU `scoreboard_interface.h` 等 P1 端接口 + Task 9-12 完成
- **P1 整体依赖**：P0 MemoryBridge 必须先完成（接口就绪）
- **P2 依赖**：P1 #C3 Adapter 层（`IAsyncCompletion` setter）
- **P3 依赖**：P0 + P1 + P2 全部完成
- **后续 change**：`2026-06-24-gpu-soc-phase8c-advanced` 依赖本 change（M2）

## 关键变更记录（2026-07-15 重构）

| 变更 | 原状态 | 新状态 | 修复的问题 |
|------|--------|--------|----------|
| 阶段命名 | "阶段 A: 6 模块" / "阶段 B: 集成验证" | P0/P1/P2/P3 | C1 P0：与综合计划实施顺序对齐 |
| MemoryBridge | 不存在 | 提升至 P0 关键路径 | 实施顺序错误 |
| 4 Adapter | 原 Task 15 阶段 B | 提升至 P1.2（与 6 模块并行） | Adapter 不能在集成阶段才做 |
| 验收门 | G1-G7 | G0-G7 + G-F0~G-F5 + G-D1~G-D8 | 与综合计划 §5 验收标准对齐 |
| 烟雾测试 | 不存在 | G-F0 `vector_add` 烟雾测试 | 故障域隔离 |
