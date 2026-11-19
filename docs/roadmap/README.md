# docs/roadmap/ — 已归档（重定向存根）

> **本目录已归档 (2026-10-19)**。历史快照文档已迁移至 [`../archive/roadmap/`](../archive/roadmap/)。
>
> **当前权威实施路线图**: [`.rddf/roadmap.md`](../../.rddf/roadmap.md) (per rdd-workflow 主文档)
>
> ⚠️ **本文件仅为重定向存根** —— 保留此目录与本文件，使不可变 ADR（`docs/adr/ADR-NV-01-gpu-soc-architecture-target.md` §关联文档）中的 `docs/roadmap/` 目录链接保持有效。**不再在此目录新增任何内容。**

## 历史快照清单 (2026-10-19 docs/roadmap/ 整体归档)

| 原路径 | 归档路径 |
|--------|---------|
| `README.md` (v1.0) | [`../archive/roadmap/2026-10-19-roadmap-readme-archived.md`](../archive/roadmap/2026-10-19-roadmap-readme-archived.md) |
| `current_status.md` (v2.0-historical) | [`../archive/roadmap/2026-10-19-current-status-archived.md`](../archive/roadmap/2026-10-19-current-status-archived.md) |
| `pcie-ep-cpptlm-collaboration-roadmap.md` (Draft v0.1) | [`../archive/roadmap/2026-10-19-pcie-ep-cpptlm-collaboration-roadmap-archived.md`](../archive/roadmap/2026-10-19-pcie-ep-cpptlm-collaboration-roadmap-archived.md) |

## 当前活跃治理路径

| 路径 | 角色 |
|------|------|
| `.rddf/roadmap.md` + `.rddf/roadmap/phases/*.md` | **协调真源 (planner 维护)** |
| `openspec/changes/<name>/` | **实施期临时变更 (builder 维护)** |
| `docs/superpowers/plans/*.md` | **实施期计划跟踪 (builder 维护)** |
| `docs/architecture/` | **真源 1 通用基础架构 (architect + builder)** |
| `docs/designs/{dgpu-board,dgpu-soc,dgpu-driver}/` | **真源 2 dGPU 应用架构 (architect + builder)** |
| `docs/adr/` | **真源 3 不可变决策 (architect)** |

---

**维护**: CppTLM 开发团队 + planner 角色
