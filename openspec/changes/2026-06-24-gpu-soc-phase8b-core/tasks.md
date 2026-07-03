# Tasks: gpu_soc Phase 8.B — 核心仿真

> **Status**: 🟢 Ready to Start (Phase 8.A ✅ Archived, `e8280fe`)
> **Parent**: `proposal.md` + `design.md` (2026-06-24-gpu-soc-phase8b-core)

## Task 列表

- [ ] **Task 9**: ScoreboardTLM ≥12 entries 单元测试 + commit
- [ ] **Task 10**: WarpSchedulerTLM CGGTY 5-warp 阈值 单元测试 + commit
- [ ] **Task 11**: PipelineTLM 5+V 分数 cycle 单元测试 + commit
- [ ] **Task 12**: TensorCoreTLM 6 精度统一管线 单元测试 + commit
- [ ] **Task 13**: L2PartitionTLM multi-slice 近/远分区 单元测试 + commit
- [ ] **Task 14**: SubCoreTLM black-box pipe 封装 单元测试 + commit
- [ ] **Task 15**: 5 类 microbenchmark + gpgpu-sim 区间对照 (test_gpu_soc_phase8b.cc + test_gpgpu_sim_comparison.py) + commit
- [ ] **Task 16**: 6 个微架构 doc + 性能 M2 验收 + docs_sync + commit

## 验收 Gates

- [ ] **G1** `[gpu][subcore][sched][sb][tc][pipe][l2]` 全 pass
- [ ] **G2** `test_gpu_soc_phase8b.cc` 5 类 microbenchmark 跑通
- [ ] **G3** `test_gpgpu_sim_comparison.py` 带宽 ±15%
- [ ] **G4** 1 GB203 × 1M < 60s
- [ ] **G5** 6 个微架构 doc + docs_sync 0 missing
- [ ] **G6** apu_soc 兼容性全绿

## 实施后节点

- [ ] **Oracle 审查** (调 oracle subagent)
- [ ] **OpenSpec 归档** → `openspec/changes/archive/2026-06-24-gpu-soc-phase8b-core/`

## 依赖

- **必须先完成**: `2026-06-24-gpu-soc-phase8a-infra` (M1)
- **后续 change**: `2026-06-24-gpu-soc-phase8c-advanced` 依赖本 change (M2)
