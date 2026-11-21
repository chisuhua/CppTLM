# cpptlm-agents-md-slimming

> **状态**: 📋 Proposed（2026-11-19）
> **类型**: **doc-only**（零代码 / 零构建 / 零冻结面改动）
> **KPI**: ⚠️ **不计入 Proposed 存量上限（3 个）** —— 本 change 只改文档（`AGENTS.md` + 2 份新文档 + `docs/README.md` 1 行登记），不产生任何能力变更（per `cpptlm-dgpu-soc-arch-truth-cleanup` 先例的 KPI 豁免声明）
> **delta spec**: 无（doc-only，`openspec/specs/` 零改动）

## Why

`AGENTS.md` 注入**每一个** agent 的上下文 ⇒ 它的体量直接等于每会话 token 成本。当前实测：

| 项 | 实测 | 问题 |
|---|---|---|
| 体量 | **64,448 B / 65,536 B（余量 1,088 B）** | 剩 1.6% 余量 ⇒ 下一次任何增补都会让 `agents_md_lint` 转红；而「便宜解法」是调高阈值 = **判据退化** |
| 过程日志 | 17 处 `Entry #`、20 处 commit SHA、38 行流水 | 违反文件自身的 §DOC HYGIENE 硬性规则：「过程日志（finding 编号 / 评审轮次 / 归档流水）**一律不进 AGENTS.md**」 |
| 硬编码计数 | 「370 个 `test_*.cc`」，**实测 376** | 已漂。同一文件的 `KEY INVARIANTS` 还写着「实时计数用 `--list-tags` 取 —— 不要在此固化精确值」⇒ 自相矛盾 |
| MCP 段 | 仍写 `code-review-graph` 且「**优先于** Grep/Glob/Read」 | 全局 `AGENTS.md` 已改用 `codebase-memory-mcp` 且把「优先」改成**条件句** ⇒ 本项目段会诱导 agent 调用**不存在的工具名** |

字节分布（谁在吃空间）：`STRUCTURE` 14,955 B(23%) · `DOC HYGIENE` 13,950 B(22%) · `PHASE STATE` 7,370 B(11%) · `WHERE TO LOOK` 6,632 B(10%) · `KEY INVARIANTS` 6,597 B(10%) · `COMMANDS` 5,542 B(9%)。

## What Changes

| # | 动作 | 落点 |
|:--:|---|---|
| **W1** | `STRUCTURE` 目录树 **188 行 → ≤ 50 行**（只留 ★ 主线 + 一级目录 + 关键子目录），逐文件清单**原样外移** | `AGENTS.md` → `docs/REPO-MAP.md`（新） |
| **W2** | `PHASE STATE` **78 行 → ≤ 18 行**：只留**终态表**（阶段 / 状态 / 当前主线入口）；交付流水（commit 区间 / Oracle 轮次 / 归档 Entry）/ 已知问题 / 既有交付明细**外移** | `AGENTS.md` → `docs/archive/process/PHASE-LOG.md`（新） |
| **W3** | `WHERE TO LOOK` **60 行 → ≤ 24 行**：只留高频「任务 → 入口」；完整索引指向 `docs/README.md §3` / `docs/adr/README.md` / `docs/designs/README.md` | `AGENTS.md` |
| **W4** | `MCP TOOLS` 段改为**指针 + 条件使用**（工具名以全局 `AGENTS.md` 为准；⛔ 不再无条件「优先于 Grep/Glob/Read」） | `AGENTS.md` |
| **W5** | 硬编码计数改为「实测值 + 取值命令」；修正 `370` → 实测值 | `AGENTS.md` |
| **W6** | 登记 `docs/REPO-MAP.md` | `docs/README.md §3.6` |

**⛔ 不做（本 change 范围外）**：`DOC HYGIENE` 的模型段拆分（真源分级 / 4 层模型改为指针）—— 它需要先确认 `docs/README.md` 完整覆盖，留作后续；`COMMANDS` 与 `scripts/README.md` 合并 —— 同上。`agents_md_lint.py` 的 **64 KiB 阈值不改**（放宽阈值即判据退化）。

## Acceptance

| # | 判据 | 手段 |
|:--:|---|---|
| AC-1 | `AGENTS.md` ≤ **45,000 B**，且 `agents_md_lint.py --strict` rc=0（树路径存在 / 体量 / 单行长度） | 脚本 |
| AC-2 | **信息不丢**：原 `STRUCTURE` 树中被 lint 实际校验的全部路径，在新 `docs/REPO-MAP.md` 中逐条出现 | 脚本比对（抽取两侧 token 集合） |
| AC-3 | **信息不丢**：原 `PHASE STATE` 的全部表格行（42 行）在 `docs/archive/process/PHASE-LOG.md` 中逐条出现 | 脚本比对（行级集合） |
| AC-4 | 硬编码计数与实测一致（`test/test_*.cc` 等），且每处附取值命令 | 实测比对 |
| AC-5 | `AGENTS.md` 不再出现 `code-review-graph` / `semantic_search_nodes` / `get_impact_radius` 等废止工具名；不再有无条件「优先于 Grep/Glob/Read」 | grep 反证 |
| AC-6 | **零回归**：`docs_sync_check --strict` · `agents_md_lint --strict` · `docs_index_consistency_check` · 全部 `cep_*` 文档闸绿 + `ctest` 全绿 + M1 冻结面 89 件 hash 一致 | 闸套件 |
| AC-7 | 新文档被 `docs_sync_check` 覆盖（其中引用的每一条路径真实存在） | 闸 |

## 风险与回滚

| 风险 | 处置 |
|---|---|
| 外移后 agent 找不到文件（信息可达性下降） | `AGENTS.md` 每节保留**一行指针**指向新文档；`docs/README.md §3.6` 登记 `REPO-MAP.md` |
| 与 `agents_md_lint.py` 的树解析不兼容（`## STRUCTURE` 后必须紧跟代码围栏） | 保持围栏形态；提交前 `--strict` 验证 |
| 回滚 | 纯文档改动，`git revert` 单笔即可；⛔ 不涉及冻结面 / 构建 / ABI |
