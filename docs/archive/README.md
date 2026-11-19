# docs/archive/ — 历史归档（附属 3）

> **定位**: 附属 3 · 历史保留。归档件**只读，不重写**；仅在文件顶部追加 `## 归档说明` 标注。
> **归档判据**: ① **时点快照**（dated report / receipt / checklist / tracker / progress log），结论已固化到别处；② **已闭环**的实施计划与实施笔记；③ **被取代**的设计稿。
> **不归档**: 真源文档（`architecture/` `designs/` `adr/`）、活跃契约载体、仍在飞行的规范。
> **更早的归档层**: `docs-archived/`（仓库根，Phase 1-3 早期决策 / `samples-orphaned/` / `dead-code-*` / `v2-architecture/`）—— 与本目录**不重叠**。

---

## 分类索引

| 子目录 | 内容 | 份数 | 归档理由 |
|--------|------|-----|---------|
| [`architecture/`](./architecture/) | 通用基础架构的历史版本 | 5 | 被现行 `architecture/` 取代（v2.0 混合架构 / 05 packet-semantic 网络 / TGMS 规格 / FastAPI 迁移 / 统一可视化平台） |
| [`designs/dgpu-soc/`](./designs/dgpu-soc/) | dGPU SoC 设计的历史版本 | 1 | `soc-internal-noc-design.md` —— 已被 OpenSpec change 实施真相替换（其指针目标在 gitignored 的 `openspec/changes/archive/`） |
| [`implementation/`](./implementation/) | 已闭环的实施计划与实施笔记 | 6 | ✅ 全部已完成（TGMS v4 / Dashboard / ArchForge 拆仓 / GMMU PTE / LWC hub / VRAM functional / 架构真相清理 / MESIF 快照） |
| [`tracking/`](./tracking/) | 第 4 层实施跟踪的历史产物 | 15 | 对应 OpenSpec change 已 archive（HSK-8 / dGPU Board Shell / v0.5-GA / dgpu-realip-refactor / pin-contract / SM 重构）+ 会话跟踪器 |
| [`validation/`](./validation/) | 早期验证报告 | 2 | 2026-09 拓扑 MVP / NSA scale-up oracle-pass |
| [`process/`](./process/) | 工作流过程产物 | 1 | `rdd-workflow-integration-gap-gap-analysis.md` —— 流程 gap 分析，**非架构文档**，误置于真源 1，已移出 |
| [`roadmap/`](./roadmap/) | 旧路线图快照 | 3 | 2026-10-19 整体归档；现路线图真源 = `.rddf/roadmap.md` |
| [`skills/`](./skills/) | v2.0 期技能清单 | 2 | 2026-04-10 版；技能真源已迁 `.opencode/skills/` + `~/.config/opencode/skills/` |

### 根级归档件（5 份）

| 文档 | 说明 |
|------|------|
| [`10-tgms-implementation-plan.md`](./10-tgms-implementation-plan.md) | TGMS Phase 1-3 实施计划 |
| [`phase3.1-defect-fixes-plan.md`](./phase3.1-defect-fixes-plan.md) | Phase 3.1 缺陷修复计划 |
| [`phase3.2-port-management-plan.md`](./phase3.2-port-management-plan.md) | Phase 3.2 端口管理计划 |
| [`phase3.3-config-enhancement-plan.md`](./phase3.3-config-enhancement-plan.md) | Phase 3.3 配置增强计划 |
| [`phase3.4-validation-toolchain-plan.md`](./phase3.4-validation-toolchain-plan.md) | Phase 3.4 验证工具链计划 |

---

## 维护规则

1. **归档用 `git mv`** —— 保留 git 历史（`git log --follow <path>` 可追溯）。
2. **不重写归档件** —— 只在顶部追加归档说明（何时、为何、从哪来）。
3. **归档前先确认真源已承载结论** —— 归档件里若仍是唯一结论，**那是真源缺失**，不是归档对象。
4. **路径引用同步** —— 归档后全仓引用须指向新路径（`docs_sync_check.sh --strict` 会兜底）。

---

**维护**: builder（归档动作）· architect（归档判据）
