# Tasks: gpu_soc Phase 8.B — 核心仿真

> **Status**: 🟢 Ready to Start (Phase 8.A ✅ Archived, `e8280fe`)
> **Parent**: `proposal.md` + `design.md` (2026-06-24-gpu-soc-phase8b-core)
> **Revision**: 2026-07-03 — 基于 ADR-NV-02 D1-Full 接口对齐修订

## Task 列表

### 阶段 A: 6 模块独立实现 (并行, ~1.5 周)

- [ ] **Task 9**: ScoreboardTLM ≥12 entries `tlm::IScoreboardInternal`
  - 产出: `include/tlm/gpu/scoreboard_interface.hh` (新增) + `scoreboard_tlm.hh/.cc` (实现接口) + `test/test_scoreboard_tlm.cc`
  - Commit: `feat(tlm/gpu): ScoreboardTLM implements IScoreboardInternal (Phase 8.B Task 9)`

- [ ] **Task 10a**: WarpSchedulerTLM — 重命名 MinimalWarpSchedulerTLM + CGGTY 5-warp 阈值 + priority 队列
  - 保留 uint32_t 接口 (F12a 已对齐), 保留旧注册项 + [[deprecated]]
  - 产出: `include/tlm/gpu/warp_scheduler_tlm.hh/.cc` (重命名) + 修改 `test/test_warp_scheduler_tlm.cc`
  - Commit: `feat(tlm/gpu): WarpSchedulerTLM rename + CGGTY threshold (Phase 8.B Task 10a)`

- [ ] **Task 10b**: CppTLMWarpSchedulerAdapter — 桥接 `WarpScheduler` ↔ `WarpSchedulerTLM`
  - `WarpContext*` ↔ `uint32_t` 转换, 额外 3 方法默认实现
  - 产出: `include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.hh/.cc` (新增) + 对应 test
  - Commit: `feat(tlm/gpu): CppTLMWarpSchedulerAdapter for D1 injection (Phase 8.B Task 10b)`

- [ ] **Task 11**: PipelineTLM 5+V 抽象 `tlm::IPipelineLatencyInternal`
  - 产出: `include/tlm/gpu/pipeline_interface.hh` (新增) + `pipeline_tlm.hh/.cc` (实现接口) + `test/test_pipeline_tlm.cc`
  - Commit: `feat(tlm/gpu): PipelineTLM implements IPipelineLatencyInternal (Phase 8.B Task 11)`

- [ ] **Task 12**: TensorCoreTLM 6 精度 `tlm::ITensorCoreTimingInternal`
  - 产出: `include/tlm/gpu/tensor_core_interface.hh` (新增) + `tensor_core_tlm.hh/.cc` (实现接口) + `test/test_tensor_core_tlm.cc`
  - Commit: `feat(tlm/gpu): TensorCoreTLM implements ITensorCoreTimingInternal (Phase 8.B Task 12)`

- [ ] **Task 13**: L2PartitionTLM multi-slice 近/远分区
  - 产出: `include/tlm/gpu/l2_partition_tlm.hh/.cc` + `test/test_l2_partition_tlm.cc`
  - Commit: `feat(tlm/gpu): L2PartitionTLM multi-slice (Phase 8.B Task 13)`

- [ ] **Task 14**: SubCoreTLM black-box pipe 封装
  - 内部组合 Task 10-13 模块, `set_sm_context()` 预留 D1 模式
  - 产出: `include/tlm/gpu/subcore_tlm.hh/.cc` + `test/test_subcore_tlm.cc`
  - 注册: `REGISTER_CHSTREAM(SubCoreTLM)` + ComputeCluster 集成
  - Commit: `feat(tlm/gpu): SubCoreTLM SMContext wrapper + standalone mode (Phase 8.B Task 14)`

### 阶段 B: 集成 + 验证 (串行, ~1 周)

- [ ] **Task 15**: Adapter 层 — Pipeline / TensorCore / Scoreboard Adapter
  - 依赖 PTX-EMU `scoreboard_interface.h` / `pipeline_interface.h` / `tensor_core_interface.h`
  - 产出: `include/tlm/gpu/adapter/` 下 3 个 Adapter + 对应 test
  - Commit: `feat(tlm/gpu): PTX-EMU adapters for Scoreboard/Pipeline/TensorCore (Phase 8.B Task 15)`

- [ ] **Task 16**: Level 1 集成测试 + Level 2/3 gpgpu-sim 对照 + 6 docs + M2 验收
  - 合成 workload (Level 1: `test_gpu_soc_phase8b.cc`) + 真实 CUDA kernel (Level 2: F12b 后)
  - Python gpgpu-sim 对照 (Level 3: `test_gpgpu_sim_comparison.py`)
  - 6 个微架构 doc + docs_sync + M2 性能验收 (1 GB203 × 1M < 60s)
  - Commit: `feat(gpu_soc): Phase 8.B 5 microbenchmarks + gpgpu-sim ±15% bandwidth + M2 docs (Task 16)`

## 验收 Gates

- [ ] **G1** `[gpu][subcore][sched][sb][tc][pipe][l2]` 全 pass
- [ ] **G2** `test_gpu_soc_phase8b.cc` Level 1 合成 workload 5 类 microbenchmark 跑通
- [ ] **G3** `test_gpgpu_sim_comparison.py` 带宽 ±15% (Level 3)
- [ ] **G4** 1 GB203 × 1M < 60s
- [ ] **G5** 6 个微架构 doc + docs_sync 0 missing
- [ ] **G6** apu_soc 兼容性全绿 (不破坏 Phase 8.A)
- [ ] **G7** Adapter 编译通过 (与 PTX-EMU 头文件联编)

## 实施后节点

- [ ] **Oracle 审查** (调 oracle subagent)
- [ ] **OpenSpec 归档** → `openspec/changes/archive/2026-06-24-gpu-soc-phase8b-core/`

## 依赖

- **必须先完成**: `2026-06-24-gpu-soc-phase8a-infra` (M1) ✅
- **Task 10b 依赖**: Task 10a
- **Task 15 依赖**: PTX-EMU 接口就绪 (PTX-1~PTX-4) + Task 9-12 完成
- **Task 16 Level 2/3 依赖**: F12b-LD (MemoryBridge + libcpptlm_cudart.so)
- **后续 change**: `2026-06-24-gpu-soc-phase8c-advanced` 依赖本 change (M2)
