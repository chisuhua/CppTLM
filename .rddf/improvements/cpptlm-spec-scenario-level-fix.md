---
name: cpptlm-spec-scenario-level-fix
priority: P0
phase: phase-10
主题: CppTLM 规范面 —— 主 spec「Requirement 无 Scenario」ERROR 级结构缺陷闭合
依赖:
  - ADR-CEP-00
状态: 🟡 **已立项**(2026-11-15 · planner 起草 · 台账 Entry #438 · KPI **0/3 → 1/3**)
created: 2026-11-15
---

# improvement: cpptlm-spec-scenario-level-fix

**主题**: 闭合 **R-7** —— 2 份主 spec 因**标题层级错误**被判「Requirement 无 Scenario」⇒ **连非 `--strict` 都 `rc=1`**（ERROR 级）。

> **范围纪律**: ⛔ 零实现层（`include/**` / `src/**` / `test/*.cc` 零 diff）· ⛔ 不动 `cep-*` 冻结面 · ⛔ **不写 Purpose / 不裁正文**（属 R-1 批 2）· ⛔ 不新增闸。
> **估时**: **0.2 人日**（3 处定点改 + 影子根预演 + 逐份 `openspec validate` 复验）。
> **KPI**: 占 OpenSpec slot **1/3**。

---

## 架构依据

| 依据 | 来源（一手） | 要点 |
|---|---|---|
| **解析器把 `## Requirements` 段内的 `###` 一律当 Requirement** | `openspec validate --json` 的错误路径 `requirements.5.scenarios` | `pcie-slice-prerequisites` 只有 **5** 个 `### Requirement:`，却报 `requirements.5` ⇒ **索引 5 = 第 6 个 `###` 标题** —— 即那个**伪 Requirement**（`### 【延至 change C/D】 …`） |
| 同上（另 2 处） | `vram-controller-functional-mode` 报 `requirements.1` / `requirements.2` | 该 spec 只有 **7** 个 `### Requirement:`，但 `### Scenario:` ×2 出现在 `## Requirements` **段内** ⇒ 被当 Requirement ⇒ 无 `#### Scenario:` ⇒ ERROR |
| **Purpose 可直改主 spec 是工具明文授权** | `openspec validate` 的 WARNING 原文 | 「Replace it … **editing the main spec directly**: a `## Purpose` in a delta is read only when the capability is created, so it cannot replace this one.」⇒ 同性质的**结构级**编辑亦以直接编辑为准（⛔ 但仍须留痕 + diff 校验） |
| **上一 change 已建立「直接编辑主 spec + delta」的混合范式** | `openspec/changes/archive/2026-11-15-cpptlm-cep-spec-strict-compliance/` | W3 = 5 份 delta（Requirement 内容变更）· W4 = 8 份 Purpose **直改主 spec** ⇒ 本 change 沿用同一范式 |
| **两份 spec 均不在冻结面** | `scripts/test/cep_freeze_check.sh` 的 `FROZEN_GLOBS` | 冻结面 = `docs/adr/ADR-CEP-*.md` / `docs/architecture/07-*.md` / `openspec/specs/cep-*/spec.md` ⇒ `pcie-slice-prerequisites` 与 `vram-controller-functional-mode` **均不在** ⇒ **零冻结产物 ⇒ 免五步链** |

## 范围

**In Scope**:

### W1 · `vram-controller-functional-mode`：2 处 `### Scenario:` → `#### Scenario:`

这 2 处**本来就是 Scenario**（正文含 `GIVEN` / `WHEN` / `THEN` / `NOTE`），只是标题写成 level-3 ⇒ 被解析器当 Requirement。

| 行 | 标题 | 正文 |
|---|---|---|
| `:20` | `### Scenario: handle_request 返回 SLVERR 传播 (per D-AXI v1.4 B9)` | GIVEN/AND/WHEN/THEN/AND/NOTE 齐 ⇒ **合法 Scenario** |
| `:29` | `### Scenario: GMMU PTE fetch 改走 VramControllerTLM 专用 functional port …` | GIVEN/WHEN/THEN/NOTE/NOTE 齐 ⇒ **合法 Scenario** |

**改法**：level 3 → level 4，**正文逐字不动**。
**载体**：**1 份 MODIFIED delta**（`vc-functional-mode-data-path (新)` 整条 Requirement 重发，**3 个 Scenario 全在**，其余文字逐字保留）⇒ 正规规范演进路径。

### W2 · `pcie-slice-prerequisites`：1 处伪 Requirement 降级

`:108` 的 `### 【延至 change C/D】 Backdoor-vs-BAR0 隔离 (本 change 无法测试)` —— 它**不是 Requirement**，是「延至 change C/D」的**占位小节**（正文是「待实现场景」bullet + 跨 change 依赖注记）。

**改法**：标题降为**非标题**（`**…**` 加粗段首），**内容逐字保留、位置不变**。

⚠️ **为什么不能用 delta 表达**：MODIFIED 块只能重发**具名 Requirement**；该标题**不是** Requirement ⇒ 解析器无从匹配 ⇒ **无 delta 途径** ⇒ 只能直接编辑主 spec（与 Purpose 同性质的直改，openspec 工具自己如此指引）。
⚠️ **为什么不能改成 `#### Scenario:`**：它的内容是「**待实现**场景（本 change Out of Scope）」⇒ 写成 Scenario 等于**宣称一条可验证但未实现的契约** ⇒ ⛔ 拒绝（那才是授权契约内容）。

### W3 · 影子根预演 + 逐份复验

- 预演 `openspec archive` ⇒ 归档后主 spec 的 diff **只落目标段**（vram-controller = 1 份 delta 合并的 Requirement 段；pcie-slice = 1 处标题行）。
- 逐份 `openspec validate <spec> --type spec`（**非 strict**）⇒ **2 份 rc=0**。

**Out of Scope**:

- ⛔ **不写 Purpose**（2 份仍是 `TBD` 占位 ⇒ WARNING）· ⛔ **不裁正文**（`requirements[0]` >500 字符 ⇒ WARNING）—— 二者均属 **R-1 批 2**（77 份批量工程）。
- ⛔ 不动 43 份 `cep-*`（**已实测 --strict 全绿**）。
- ⛔ 不动实现层 / 不新增闸 / 不改冻结面。

## Why

**第 1 条 —— 这是全仓唯一「连非 strict 都红」的类别，性质高于 R-1 的 77 份 strict 红。**

实测（2026-11-15，`openspec validate <spec> --type spec` **不带** `--strict`）：

| 类别 | 份数 | 判据级别 |
|---|:--:|---|
| `cep-*` | 0 | — |
| 非 `cep-*` | **2** | **ERROR** ⇒ **rc=1** |
| 非 `cep-*`（其余） | 75 | WARNING ⇒ 仅 `--strict` 红 |

**ERROR 级 = 规范**本身**结构不合法**（Requirement 没有 Scenario ⇒ 该 Requirement 不可验证）⇒ 与 WARNING 级（风格/长度）差一个量级。而这 2 份恰好是 **PCIe slice 前置**与 **VRAM functional-mode** —— 两条**已被实现的**能力的规范面。

**第 2 条 —— 根因是解析器口径，不是「作者忘了写 Scenario」。**

初稿（Entry #436）把 R-7 记作「Requirement 无 Scenario ⇒ 需撰写 Scenario ⇒ **授权契约内容**⇒ 另立」。**实测推翻**：`openspec validate --json` 的错误路径 `requirements.5.scenarios`（`pcie-slice-prerequisites` 只有 **5** 个 Requirement）⇒ 索引 5 指向**第 6 个 `###` 标题**，而那根本不是 Requirement。⇒ **真相 = 标题层级错**，修复 = 改层级，**零语义变化、零契约授权**。

⇒ **本 change 因此从「需 architect 判权的授权型任务」降级为「纯结构修复」**，可由 builder 直接实施。

**第 3 条 —— 认知边界（诚实声明）。**

- 本 change **只**让 2 份 spec 达到「非 strict 绿」。它们**仍然** `--strict` 红（Purpose 占位 + 正文超长）⇒ **R-1 未闭合**。
- 本 change **不主张**「规范面已合规」—— 全仓 `--strict` 仍有 **77** 份红。
- 本 change **不新增**任何判据、**不授权**任何新契约；改的 3 处**全是标题层级**，正文**逐字未动**。

## What Changes

1. `openspec/specs/vram-controller-functional-mode/spec.md`：2 处 `### Scenario:` → `#### Scenario:`（经 **1 份 MODIFIED delta** 合并）。
2. `openspec/specs/pcie-slice-prerequisites/spec.md`：1 处伪 Requirement 标题 → 非标题（**直接编辑主 spec**；⛔ 无 delta 途径）。
3. 1 份 delta（`vram-controller-functional-mode`）· 1 份 improvement · 1 份 proposal / tasks · 台账 Entry #438。

## Acceptance

| # | 判据 | 可验收点（机器可判） |
|:--:|---|---|
| **AC-01** | 2 份 spec **非 strict** 绿 | `openspec validate <spec> --type spec`（**不带** `--strict`）⇒ **2/2 rc=0** |
| **AC-02** | ERROR 级清零 | `openspec validate --json` ⇒ `items[0].issues` 中 `level == "ERROR"` **计数 = 0**（2 份） |
| **AC-03** | 全仓非 strict 全绿 | 129 份 spec 逐份 `validate --type spec`（非 strict）⇒ **rc≠0 计数 = 0**（当前 2 ⇒ 目标 0） |
| **AC-04** | 改的只有标题层级 | diff 逐行核对：vram-controller 的 2 行 = `### Scenario:` → `#### Scenario:`（**正文 0 改**）；pcie-slice 的 1 行 = 标题降级（**内容 0 改**）⇒ **diff 的增行 = 3 / 删行 = 3** |
| **AC-05** | Scenario 数量不丢 | vram-controller 的 `vc-functional-mode-data-path` ⇒ **3 个 `#### Scenario:`**（改前：1 正确 + 2 错级）；`pcie-slice-prerequisites` 的 Requirement 数 **5 → 5** |
| **AC-06** | 零实现层 / 零冻结面 | `include/**` / `src/**` / `test/*.cc` / `docs/adr/**` 零 diff · `cep_freeze_check.sh` 的 `artifact_count` **86 不变** |
| **AC-07** | `cep-*` 不受影响 | 43 份 `cep-*` 逐份 `--strict` ⇒ **rc≠0 = 0**（与改前一致） |
| **AC-08** | 归档链（**零冻结产物 ⇒ 免五步链**） | 预演 diff 只落目标段 ⇒ 真仓 `openspec archive` ⇒ 归档目录 `openspec/changes/archive/2026-11-15-cpptlm-spec-scenario-level-fix/` |
| **AC-09** | KPI | 立项 **0/3 → 1/3**；归档 **1/3 → 0/3**（6 处活口径 + obj 并发句） |

## Capabilities

| # | 能力 | 判据落点 | 新增 `INV-CEP-*` |
|:--:|---|---|:--:|
| **C-1** | 主 spec「`###` 级非 Requirement 标题」不再被解析器误判 | AC-01/02/03 | ❌ 0 |
| **C-2** | 修复零语义变化（正文逐字不动） | AC-04/05 | ❌ 0 |

## Impact

| 面 | 影响 |
|---|---|
| **实现层** | ⛔ 零改动 |
| **契约** | 2 份**非 CEP** 主 spec 的 **3 处标题层级**；1 份 delta（`vram-controller-functional-mode`）⇒ **零冻结产物** ⇒ 免五步链 |
| **闸** | 不新增、不改 ⇒ 闸数 **45 不变** |
| **文档** | 台账 Entry #438 + 闭合回执 Entry #439 |
| **残留** | **R-1 仍开放**（77 份 `--strict` 红：75 Purpose 占位 + 70 正文 >500）⇒ 下一批 |

**⚠️ 残余（3 项 · 本 change 不做）**：

| # | 残余 | 实测 | 路由 |
|:--:|---|---|---|
| **R-1** | 非 `cep-*` 的 **77** 份 spec `--strict` 红（75 Purpose 占位 + 70 正文 >500，有重叠） | 2 份已绿 ⇒ **仍 77 份红**（含刚修的 2 份） | **下一批**（分批：先 Purpose 后裁剪） |
| **R-1 口径澄清** | 2 份修完后仍红 ⇒ **R-1 的「77」是 `--strict` 口径**，**非 strict 口径下修后为 0** | 实测 | 台账登记 |
| **R-9** | 清单增殖（5 处手工维护） | 本 change 不涉及 | backlog（需定 canonical） |

## Non-Goals

- ⛔ 不写 Purpose · ⛔ 不裁正文（属 R-1 批 2）· ⛔ 不动 `cep-*` · ⛔ 不动实现层。
- ⛔ 不把伪 Requirement 改写成 Scenario（那会宣称未实现的契约）。
- ⛔ 不裁决 R-2 / R-3 / R-6 / R-9。
