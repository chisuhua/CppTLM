# Roadmap

> **最后更新**: 2026-10-19 (v1.0-planner 重写, per rdd-doctor 12 CRITICAL + 路线图严重失真治理)
> **真源原则**: 本文件代表项目长期路线图 (rdd-workflow 主文档), **真源 1 (docs/architecture/)** 与 **真源 2 (docs/designs/)** 是技术真源, 本文件是协调真源
> **OpenSpec KPI 政策**: per AGENTS.md §DOC HYGIENE line 420, 当前 baseline 5/3 (active changes / Proposed 上限), 详见 §OpenSpec 现状

## Phase Skeleton

| Phase | Theme | Status | Started | Done |
|-------|-------|--------|---------|------|
| phase-1 | 通用基础架构 (事务/错误/复位/拓扑/metrics) v1.2 治理完成 | completed | 2026-04 | 2026-10 |
| phase-2 | GPGPU 多层 SimModule + ApuSoC 顶层容器 | completed | 2026-06 | 2026-08 |
| phase-3 | PCIe EP 链路层 + 编码 + PHY (Phase 1-3 微架构) | completed | 2026-03 | 2026-09 |
| phase-4 | PCIe EP SR-IOV VF Pool (Phase 4 微架构) | completed | 2026-08 | 2026-10 |
| phase-5 | PCIe EP AXI Stream Adapter + AXI4Mapper (Phase 5-6 微架构) | completed | 2026-09 | 2026-10 |
| phase-6 | PCIe EP Host Bypass + RC + 整合交付 (Phase 7-8 微架构) | completed | 2026-10 | 2027-02 |
| phase-7 | D-AXI Driver-Visible Minimal SoC v1.4 | completed | 2026-09 | 2026-09 |
| phase-8 | timing-mode SoC v0.5 (per ADR-DGPU-11) | completed | 2027-02 | 2027-02 |
| phase-9 | dGPU Phase 2/3: ControlHub + 06 v0.4 + 06-bis + 5 active changes | active | 2026-10 | — |

<!-- AUTO-INDEX -->

### Phases

- `phase-1` — 通用基础架构 (事务/错误/复位/拓扑/metrics) v1.2 治理完成 · completed
- `phase-2` — GPGPU 多层 SimModule + ApuSoC 顶层容器 · completed
- `phase-3` — PCIe EP 链路层 + 编码 + PHY (Phase 1-3 微架构) · completed
- `phase-4` — PCIe EP SR-IOV VF Pool (Phase 4 微架构) · completed
- `phase-5` — PCIe EP AXI Stream Adapter + AXI4Mapper (Phase 5-6 微架构) · completed
- `phase-6` — PCIe EP Host Bypass + RC + 整合交付 (Phase 7-8 微架构) · completed
- `phase-7` — D-AXI Driver-Visible Minimal SoC v1.4 · completed
- `phase-8` — timing-mode SoC v0.5 (per ADR-DGPU-11) · completed
- `phase-9` — dGPU Phase 2/3: ControlHub + 06 v0.4 + 06-bis + 5 active changes · active

### Features

> **当前无跨 phase feature fragment** (`.rddf/roadmap/features/` 不存在). 跨 phase 改进通过 OpenSpec change + 调研附属 (`docs/research/`) 跟踪. 待 dGPU Phase 2/3 (phase-9) 实施期发现跨 phase 复用点时建立.

### Objectives

> **当前无 objective** (`.rddf/roadmap/objectives/` 不存在). dGPU Phase 2/3 (phase-9) 多个 active changes 是结构聚合, 不承载跨 sprint 叙事, 按 rdd-workflow skill "Objective vs Feature fragment" 决策规则归为 feature fragment 类 (免费、自动、零维护). 暂不建立 objective. 待 2026-Q4 出现跨 sprint 复杂目标时建立.

<!-- AUTO-SPRINT -->