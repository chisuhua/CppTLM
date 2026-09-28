# docs/superpowers/plans/ — 实施计划 + 进度跟踪归档层

> **定位**: 第 4 层文档（实施跟踪）。与 `openspec/changes/<name>/tasks.md` 互补，不替代。
> **维护策略**: 实施期短期文档 → 完成后归档至 `archive/`（**仅在 change archive 时迁移，不主动清理**）
> **新建时间**: 2026-09-26（4 层文档模型重组时建立 README 明示职责）

---

## 目录职责

| 文档类型 | 角色 | 与 OpenSpec tasks.md 的关系 |
|---------|------|---------------------------|
| `plans/<name>.md` | TDD 5 步实施计划（RED → GREEN → REFACTOR + commit 策略 + 出口验证） | **互补**：tasks.md 列任务清单，plan 列实施细节 |
| `plans/<name>-progress.md` | 阶段进度报告（已完成 phase + 实施期发现 + baseline 验证） | **互补**：tasks.md checkbox vs progress.md 进度快照 |
| `plans/archive/*.md` | 已完成的实施计划 + 进度报告 | 实施完成 + change archive 后迁移 |

**为什么不合并到 OpenSpec tasks.md**：
- tasks.md 是 **提案驱动**（随 OpenSpec change 生命周期）
- plans/ 是 **实施驱动**（随实施 phase 生命周期，可跨 change）
- 同一 phase 可能服务多个 OpenSpec change（如 D-AXI T1 服务 `cpptlm-driver-visible-minimal-soc` change）

---

## 归档规则

1. **何时归档**：对应的 OpenSpec change archive 后
2. **如何归档**：`git mv plans/<name>.md plans/archive/<name>.md`（保留 git 历史）
3. **不主动清理**：实施期不主动删除 plan，仅在归档时迁移

---

## 当前 6 个 plan 状态（2026-09-26 snapshot）

| 文件 | 关联 OpenSpec change | 状态 | 归档优先级 |
|------|---------------------|------|-----------|
| `2026-08-29-dgpu-board-shell-injection.md` | DGpuBoard Shell Injection（v0.5 → v3.0 实施） | ✅ 已完成 | 待归档（建议实施完成确认后迁移至 `archive/`） |
| `2026-08-29-v05-ga-roadmap.md` | v0.5.0-MVP → v0.5.0-GA → v3.0.0 dGPU Board | ✅ 已完成 | 待归档 |
| `2026-09-27-cpptlm-driver-visible-minimal-soc-phase0-progress.md` | cpptlm-driver-visible-minimal-soc (D-AXI v1.8) | 🟡 进行中（Phase 0 完成） | 暂不归档（待 Phase 0+ 全部完成） |
| `2026-09-27-cpptlm-driver-visible-minimal-soc-t1.md` | cpptlm-driver-visible-minimal-soc (D-AXI v1.8) | 🟡 进行中（T1 实施计划, 14 Phase × 83 子任务） | 暂不归档 |
| `2027-02-09-sm-microarchitecture-rewrite.md` | dGPU SoC v1.0 SM Microarchitecture 重写 | ✅ 已完成（per `## Status Update` 段） | 待归档 |
| `2027-02-10-sm-task18-impl-and-ptxemu-hsk9.md` | SM Task 18 完整实施 + PTX-EMU HSK-9 | 🟡 进行中 | 暂不归档 |

---

## 阅读路径

1. **快速理解本目录**: 本 README.md（你在这里）
2. **查找特定 phase 的 TDD 5 步**: `plans/<name>.md`
3. **查看 phase 进度**: `plans/<name>-progress.md`
4. **历史归档**: `plans/archive/`

---

## 与 4 层文档模型的关系

本目录是 **第 4 层**（实施跟踪），完整分层：

```
┌─ 第 1 层 ─ 长期维护架构 ─ docs/architecture/ + docs/designs/
├─ 第 2 层 ─ 架构决策 ───── docs/adr/ADR-{X,DGPU}-*.md
├─ 第 3 层 ─ OpenSpec ───── openspec/changes/<name>/{proposal,design,spec,tasks}.md
└─ 第 4 层 ─ 实施跟踪 ───── docs/superpowers/plans/   ← 本目录
```

详细见 [AGENTS.md §DOC HYGIENE](../../AGENTS.md)。

---

## 相关资源

- [AGENTS.md §DOC HYGIENE](../../AGENTS.md) — 4 层文档模型详细说明
- [openspec/](../../openspec/) — 第 3 层 OpenSpec changes
- [.opencode/skills/cpptlm-debug/SKILL.md](../../../.opencode/skills/cpptlm-debug/SKILL.md) — 调试 test fail（auto-loads）

---

## 维护记录

| 日期 | 事件 |
|------|------|
| 2026-09-26 | 新建本 README.md（4 层文档模型重组时建立职责说明） |

---

**维护**: CppTLM 开发团队