# Tasks: gpu_soc Phase 8.C — 高级特性

> **Status**: 🔄 Draft
> **Parent**: `proposal.md` + `design.md` (2026-06-24-gpu-soc-phase8c-advanced)

## Task 列表

- [ ] **Task 17**: TccTLM write coalescing 单元测试 + commit
- [ ] **Task 18**: TmaTLM async copy 单元测试 + commit
- [ ] **Task 19**: DsmTLM inter-SM shmem 单元测试 + commit
- [ ] **Task 20**: PowerModelTLM 80W+1W/SM 经验模型单元测试 + commit
- [ ] **Task 21**: cpptlm.nvidia 拓扑生成 + 4 SKU 预置 (test_cpptlm_nvidia.py) + commit
- [ ] **Task 22**: cpptlm.gpu_workload + 5 pattern 构造器 (test_cpptlm_gpu_workload.py) + commit
- [ ] **Task 23**: cpptlm.gpu_soc 顶层 simulate + report (test_cpptlm_gpu_soc.py) + commit
- [ ] **Task 24**: apu_soc 集成测试 (test_gpu_soc_phase8c.cc) + 完整验证报告 + commit
- [ ] **Task 25**: 4 个微架构 doc + ADR-NV-01 已存在 + roadmap 更新 + AGENTS.md 更新 + commit

## 验收 Gates

- [ ] **G1** `[gpu][tcc][tma][dsm][power]` 全 pass
- [ ] **G2** Python pytest 15+ pass
- [ ] **G3** apu_soc 集成测试 pass
- [ ] **G4** 完整 5 类场景报告生成
- [ ] **G5** 性能 M3
- [ ] **G6** docs_sync 0 missing + format clean

## 实施后节点

- [ ] **Oracle 审查** (调 oracle subagent)
- [ ] **OpenSpec 归档** → `openspec/changes/archive/2026-06-24-gpu-soc-phase8c-advanced/`
- [ ] **更新 roadmap**: 标记 Phase 8 全部完成

## 依赖

- **必须先完成**: `2026-06-24-gpu-soc-phase8b-core` (M2)
- **完成后**: gpu_soc 整体可用；apu_soc 端到端可享 gpu_soc 仿真能力
