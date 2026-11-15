# proposal: cpptlm-spec-scenario-level-fix —— 闭合 R-7（主 spec「Requirement 无 Scenario」ERROR 级）

> **状态**: 🟡 Proposed（2026-11-15 立项 · planner 起草 · 待 builder 实施）
> **依据**: improvement `.rddf/improvements/cpptlm-spec-scenario-level-fix.md` · 台账 **Entry #438**
> **KPI**: 立项后 Proposed **0/3 → 1/3**

## 为什么

**全仓只有 2 份 spec 连非 `--strict` 都 `rc=1`** —— 性质高于 R-1 的 77 份 `--strict` 红（后者只是 WARNING 级）。

实测（2026-11-15，`openspec validate <spec> --type spec` **不带** `--strict`）：

| 类别 | 份数 | 级别 |
|---|:--:|---|
| `cep-*` | 0 | — |
| 非 `cep-*` | **2** | **ERROR**（Requirement 无 Scenario ⇒ 不可验证） |
| 非 `cep-*`（其余 75 份） | 75 | WARNING（Purpose 占位 / 正文超长）⇒ 仅 `--strict` 红 |

而**根因不是「作者忘了写 Scenario」**（初稿误判为需授权契约内容）：

- `pcie-slice-prerequisites` 报 `requirements.5.scenarios`，但该 spec 只有 **5** 个 `### Requirement:` ⇒ **索引 5 = 第 6 个 `###` 标题** —— `### 【延至 change C/D】 Backdoor-vs-BAR0 隔离`（一个**占位小节**，不是 Requirement）。
- `vram-controller-functional-mode` 报 `requirements.1` / `requirements.2` ⇒ 那 2 个是 `### Scenario:`（level 3，**本来就是 Scenario**，正文含 GIVEN/WHEN/THEN），只是层级写错。

⇒ **真相 = `## Requirements` 段内的 `###` 级非 Requirement 标题被解析器当作 Requirement** ⇒ 修复 = **改标题层级**，**零语义变化、零契约授权**。

## 变更内容

| # | 内容 | 载体 |
|:--:|---|---|
| **W1** | `vram-controller-functional-mode`：2 处 `### Scenario:` → `#### Scenario:`（正文逐字不动） | **1 份 MODIFIED delta**（`vc-functional-mode-data-path (新)` 整条 Requirement 重发，3 个 Scenario 全在） |
| **W2** | `pcie-slice-prerequisites`：1 处伪 Requirement `### 【延至 change C/D】 …` → **非标题**（内容逐字保留、位置不变） | **直接编辑主 spec**（⛔ MODIFIED 块只能重发**具名 Requirement** ⇒ 该标题无 delta 途径；同 openspec 对 Purpose 的直改授权） |
| **W3** | 影子根预演 + 逐份复验 | — |

**⛔ 边界**：零实现层 · 不写 Purpose / 不裁正文（属 R-1 批 2）· 不动 43 份 `cep-*` · 不新增闸（45 不变）· **不把伪 Requirement 改写成 Scenario**（那会宣称一条「待实现」的契约）。

**冻结面**: 两份 spec **均不在** `FROZEN_GLOBS`（只含 `docs/adr/ADR-CEP-*.md` / `docs/architecture/07-*.md` / `openspec/specs/cep-*/spec.md`）⇒ **零冻结产物 ⇒ 免五步链**。

## 验收标准

| # | 判据 |
|:--:|---|
| **AC-01** | 2 份 spec **非 strict** ⇒ `openspec validate <spec> --type spec` rc=0（2/2） |
| **AC-02** | `--json` 输出中 `level == "ERROR"` 计数 = **0**（2 份） |
| **AC-03** | 全仓 **129** 份 spec 逐份非 strict ⇒ rc≠0 计数 = **0**（当前 2） |
| **AC-04** | diff 逐行核对：增行 = **3** / 删行 = **3**，且**正文 0 改** |
| **AC-05** | `vc-functional-mode-data-path` 的 `#### Scenario:` 数 = **3**（改前 1 正确 + 2 错级）；`pcie-slice-prerequisites` 的 Requirement 数 **5 → 5** |
| **AC-06** | `include/**` / `src/**` / `test/*.cc` / `docs/adr/**` 零 diff；`artifact_count` **86 不变** |
| **AC-07** | 43 份 `cep-*` 逐份 `--strict` ⇒ rc≠0 = **0** |
| **AC-08** | 预演 diff 只落目标段；归档目录 `openspec/changes/archive/2026-11-15-cpptlm-spec-scenario-level-fix/` |
| **AC-09** | KPI **0/3 → 1/3**（立项）→ **0/3**（归档） |

## 不做什么

① 不写 Purpose · ② 不裁正文（属 R-1 批 2，77 份）· ③ 不动 `cep-*` · ④ 不动实现层 · ⑤ 不把伪 Requirement 写成 Scenario · ⑥ 不裁决 R-2 / R-3 / R-6 / R-9。
