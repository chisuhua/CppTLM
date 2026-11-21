# design: cpptlm-spec-scenario-level-fix

> **角色**: builder · **上游**: `proposal.md` + improvement · 台账 **Entry #438**(立项) / **#439**(收口)
> **范围**: 主 spec **标题层级**定点修正（3 处）· 零实现层 · 零 ADR

## D-1 · 根因：`## Requirements` 段内的 `###` 一律被当作 Requirement

`openspec validate --json` 的错误路径直接指认：

| spec | 错误路径 | 实际 `### Requirement:` 数 | 真相 |
|---|---|:--:|---|
| `pcie-slice-prerequisites` | `requirements.5.scenarios` | **5** | 索引 5 = **第 6 个 `###` 标题** —— `### 【延至 change C/D】 Backdoor-vs-BAR0 隔离`（**占位小节**，不是 Requirement） |
| `vram-controller-functional-mode` | `requirements.1` / `.2` | 7 | 那 2 个 `###` 是 `### Scenario:`（level 3，**本来就是 Scenario**，正文含 GIVEN/WHEN/THEN） |

⇒ 修复 = **改标题层级** ⇒ **零语义变化、零契约授权**（与 Entry #438 §3 的「推翻」一致）。

## D-2 · 两条载体路径（各有理由，不混用）

| 目标 | 载体 | 理由 |
|---|---|---|
| `vram-controller-functional-mode`（2 处 level-3 Scenario） | **1 份 MODIFIED delta**（`vc-functional-mode-data-path (新)` 整条 Requirement 重发，3 个 Scenario 全在） | 正规规范演进路径；⛔ 但 MODIFIED 会**重发整条 Requirement** ⇒ 合并器**规范化整段** ⇒ 产生 3 行空白噪声（见 D-4） |
| `pcie-slice-prerequisites`（1 处伪 Requirement） | **直接编辑主 spec** | ⛔ **无 delta 途径** —— MODIFIED 块只能重发**具名 Requirement**，该标题**不是** Requirement ⇒ 解析器无从匹配。降为**非标题**（`**…**`）⇒ 内容逐字保留、位置不变 |

⚠️ **为什么不把伪 Requirement 改成 `#### Scenario:`**：它的内容是「**待实现**场景（本 change Out of Scope）」⇒ 写成 Scenario 等于**宣称一条可验证但未实现的契约** ⇒ ⛔ 拒绝（那才是授权契约内容）。

## D-3 · 归档语义：走 delta 而非 `skip_specs`

⚠️ 实测：`openspec archive` 在 change **无 delta** 时要求 `.openspec.yaml` 设 `skip_specs: true`（或 CLI `--skip-specs`）⇒ 而仓内 `AGENTS.md` 规定 `--skip-specs` = **⏸ 暂存归档**语义（断言「设计保留,能力未落地」）⇒ 与本 change「**实现已完成**」矛盾。

⇒ **走 delta 路径**（归档时合并 delta ⇒ 明确是 **✅ 已完成归档**语义，无暂存歧义）。代价 = D-4 的 3 行空白噪声。

⚠️ 另实测：`openspec` 的 `ChangeParser` 要求 proposal 章节标题**精确**为 `## Why` / `## What Changes`（`## Why（为什么）` **不被接受**）⇒ 已订正（本仓 CEP 提案惯用的 `## 为什么` 会触发警告，Entry #424 L-1 已记过同一坑）。

## D-4 · 合并器空白噪声归因（3 行，非语义改动）

`vram-controller-functional-mode` 归档 diff = **增 4 / 删 3**，其中：

| 行 | 内容 | 归因 |
|---|---|---|
| ×2 | `## Requirements` 段头**前后**插入空行 | MODIFIED 合并器**重写整个 Requirements 段**时的规范化 |
| ×1 | 文件**末尾**多余空行被裁剪 | 同上 |
| ×2 | `### Scenario:` → `#### Scenario:` | **本 change 的语义改动** |

⇒ **AC-04 已订正**为「**语义改动 = 3 处标题** + 空白规范化 ≤ 3 行（已归因）」，proposal / improvement / tasks 三处同步留痕。

## D-5 · 边界（零放宽）

⛔ `include/**` / `src/**` / `test/*.cc` / `docs/adr/**` 零 diff · ⛔ 不写 Purpose · ⛔ 不裁正文（属 R-1 批 2）· ⛔ 不动 43 份 `cep-*` · ⛔ 不新增闸（45 不变）· ⛔ 两份 spec 均不在 `FROZEN_GLOBS` ⇒ **零冻结产物 ⇒ 免五步链**（`artifact_count` 86 不变）。
