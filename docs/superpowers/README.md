# docs/superpowers/ — 第 4 层实施跟踪

> **定位**: 4 层文档模型的**第 4 层** —— TDD 5 步实施计划 + 实施期规格。
> **与 OpenSpec 的关系**: **互补，不替代**。
> - `openspec/changes/<name>/tasks.md` = **提案驱动**的任务清单（随 change 生命周期）
> - 本目录 `plans/` + `specs/` = **实施驱动**的实施细节与阶段进度（随实施 phase 生命周期，可跨 change）
> **归档规则**: 对应 OpenSpec change archive 后 ⇒ 迁 [`../archive/tracking/`](../archive/tracking/)。

---

## 目录

### [`plans/`](./plans/) — TDD 5 步实施计划 + 进度报告

| 文档 | 主题 | 状态 |
|------|------|------|
| [`2026-09-27-cpptlm-driver-visible-minimal-soc-t1.md`](./plans/2026-09-27-cpptlm-driver-visible-minimal-soc-t1.md) | D-AXI T1（14 Phase × 83 子任务） | 🟡 进行中 |
| [`2026-09-27-cpptlm-driver-visible-minimal-soc-phase0-progress.md`](./plans/2026-09-27-cpptlm-driver-visible-minimal-soc-phase0-progress.md) | D-AXI T1 Phase 0 进度 | 🟡 进行中 |
| [`2026-10-06-dgpu-realip-refactor.md`](./plans/2026-10-06-dgpu-realip-refactor.md) | dGPU Real-IP 重构（7 阶段 PCIe EP + D-AXI 整合） | 🟡 Phase C 启动条件已就绪 |
| [`README.md`](./plans/README.md) | 本目录职责 + 归档规则 + 当前 plan 状态表 | — |

### [`specs/`](./specs/) — 实施期规格与基线

| 文档 | 主题 |
|------|------|
| [`2026-10-06-dgpu-realip-refactor-design.md`](./specs/2026-10-06-dgpu-realip-refactor-design.md) | dGPU Real-IP 重构设计 |
| [`2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md`](./specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md) | HSK-9 `ICOMPUTE_API_VERSION=1` SM 重写（权威 Spec） |
| [`2026-08-29-dgpu-board-shell-injection-design.md`](./specs/2026-08-29-dgpu-board-shell-injection-design.md) | DGpuBoard Shell 注入设计 |
| [`2026-08-31-ptxir-h2d-pcie-sdma-integration-design.md`](./specs/2026-08-31-ptxir-h2d-pcie-sdma-integration-design.md) | PTXIR H2D PCIe SDMA 集成设计 |
| [`HSK-9-baseline-tracker.md`](./specs/HSK-9-baseline-tracker.md) | HSK-9 实测基线（回归比对锚点） |

---

## 4 层模型中的位置

```
┌─ 第 1 层 ─ 长期维护架构 ─ docs/architecture/ + docs/designs/
├─ 第 2 层 ─ 架构决策 ───── docs/adr/ADR-*.md
├─ 第 3 层 ─ OpenSpec ───── openspec/changes/<name>/{proposal,design,spec,tasks}.md
└─ 第 4 层 ─ 实施跟踪 ───── docs/superpowers/   ← 本目录
```

---

**维护**: builder
