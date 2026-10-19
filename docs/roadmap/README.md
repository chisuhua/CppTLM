# docs/roadmap/ 已归档

> **本目录已归档 (2026-10-19)**. 历史快照文档已迁移至 `docs/archive/roadmap/`.
>
> **当前权威实施路线图**: [`.rddf/roadmap.md`](../../.rddf/roadmap.md) (per rdd-workflow 主文档)
>
> **真源原则 (per AGENTS.md §DOC HYGIENE line 432)**:
> - 真源 1 (通用基础架构): [`docs/architecture/`](../../.rddf/roadmap.md#phase-1)
> - 真源 2 (dGPU 应用架构): [`docs/designs/`](../../docs/designs/)
> - 真源 3 (不可变决策): [`docs/adr/`](../../docs/adr/)
> - **协调真源 (项目级路线图)**: [`.rddf/roadmap.md`](../../.rddf/roadmap.md)
> - 本目录原内容 = 历史快照, 已 **archived**, 不再独立维护

## 归档清单 (2026-10-19 docs/roadmap/ 整体归档)

| 原路径 | 归档路径 |
|--------|---------|
| `README.md` (v1.0) | [`archive/roadmap/2026-10-19-roadmap-readme-archived.md`](../archive/roadmap/2026-10-19-roadmap-readme-archived.md) |
| `current_status.md` (v2.0-historical) | [`archive/roadmap/2026-10-19-current-status-archived.md`](../archive/roadmap/2026-10-19-current-status-archived.md) |
| `pcie-ep-cpptlm-collaboration-roadmap.md` (Draft v0.1) | [`archive/roadmap/2026-10-19-pcie-ep-cpptlm-collaboration-roadmap-archived.md`](../archive/roadmap/2026-10-19-pcie-ep-cpptlm-collaboration-roadmap-archived.md) |

## 归档原因

1. **真源原则 (per AGENTS.md §DOC HYGIENE)**: 路线图治理**协调真源**在 `.rddf/roadmap.md`, `docs/roadmap/` 历史上的 3 文件都是 2026-07-03 旧视角 (顶部已标 FAIL by Oracle 2027-02-09), 7 个月未更新, 与项目实际状态脱节
2. **rdd-doctor 严重失真**: 12 CRITICAL (roadmap-md-integrity.index-sync × 5 + roadmap-phases.index-drift × 4 + main-doc-missing × 1 + table-schema × 1) + 4 WARNING (index-segment × 3 + table-schema × 1) 全部已通过 `.rddf/roadmap.md` 9 phase 重写修复 (commit `84846cbb`)
3. **维护规则变更**: per `docs/archive/roadmap/2026-10-19-roadmap-readme-archived.md` line 27 既有声明: "维护规则变更: docs/roadmap/ 不再独立维护; 新增 OpenSpec change 时由 `openspec/changes/<name>/` 承担项目级协调"

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