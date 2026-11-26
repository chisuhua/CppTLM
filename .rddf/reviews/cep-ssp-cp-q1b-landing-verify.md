# verify: cep-ssp-cp-q1b-landing —— 立项产物验收

> **验收对象**：`cep-ssp-cp-q1b-landing` 的**立项产物**（openspec change 骨架 + plan + tasks）
> **验收时间**：2026-11-26
> **执行角色**：builder（角色内建 verifier 契约 · 本地验收路径）
> **⚠️ 范围界定**：本次交付 = **立项**（P0 → P1.5），**实现尚未开始**（`tasks.md` 全部 `- [ ]`）⇒ 本报告**不判定实现是否满足 AC**，只判定**立项产物是否合规可用**。

---

## 判据来源（优先级 1 → 4）

| # | 来源 | 本次采用 |
|:--:|---|---|
| 1 | 用户本次明确给出的验收条件 | 「切 planner 对 cep-ssp-cp-q1b-landing 立项」⇒ 立项产物齐备且可被下游消费 |
| 2 | `openspec/changes/<name>/proposal.md` 的 `## 验收标准` 段 | ✅ 7 条（**本报告验「骨架层」的可机判子集**；「实现层」判据待 P2/P3 验）|
| 3 | 本次变更自己声明的行为 | `design.md` D-1~D-8 + `.rddf/plans/cep-ssp-cp-q1b-landing.md` 的 Done Definition |
| 4 | ADR 的 Verification 章 | `ADR-CEP-11` SU(2026-11-26) ③ 第 1~7 面落地清单（本报告用于**范围对账**）|

---

## 逐条判定

### 判据 ①：openspec 骨架合规（`openspec validate --strict` rc=0）

- **证据**：`openspec validate cep-ssp-cp-q1b-landing --strict` ⇒ `Change 'cep-ssp-cp-q1b-landing' is valid` · **rc=0**
- **附加**：`openspec validate --changes --strict` ⇒ `Totals: 1 passed, 0 failed (1 items)`
- **判定**：✅ **PASS**

### 判据 ②：proposal 含 `## 验收标准` 段且 AC 条数与 improvement 严格一致（零漂移）

- **证据**：`grep -nE '^## (验收标准|Acceptance Criteria)' proposal.md` ⇒ `:72` 命中 · AC checkbox **7** 条
- **逐字比对**：`diff <(grep '^- \[ \]' improvement) <(sed -n '/^## 验收标准/,$p' proposal \| grep '^- \[ \]')` ⇒ **零差异**
- **判定**：✅ **PASS**（7 = improvement 7，无增删、无改写）

### 判据 ③：delta 形态正确（MODIFIED + 场景名守恒 + 差异恰为 IC1）

- **证据 1**：`head -1 specs/cep-core-contracts/spec.md` ⇒ `## MODIFIED Requirements`（**非** 生成器原产的 `## ADDED Requirements`）
- **证据 2**：场景名守恒脚本 ⇒ main spec 的 3 个 Scenario（`版本取值必须落在锁定表内(IC1)` / `锁定表是唯一真源(IC2)` / `版本变更必须留痕(IC3)`）**全部**在 delta 中，`MISSING = ∅`
- **证据 3**：差异脚本 ⇒ delta 与 main spec 的 requirement 差异**恰为 IC1 的 THEN 段**（IC2 / IC3 **逐字零改动**）
- **判据依据**：openspec `dist/core/specs-apply.js:392-394` —— MODIFIED 块**必须含既有全部 Scenario**，否则 archive 抛 `current spec contains scenario(s) not present in the modified block`
- **判定**：✅ **PASS**

### 判据 ④：plan 满足消费者契约（`rdd-workflow-writing-plans` 明列 5 项）

| 契约项 | 实测 | 判定 |
|---|---|:--:|
| 路径 `.rddf/plans/<CHANGE_NAME>.md` | `.rddf/plans/cep-ssp-cp-q1b-landing.md` | ✅ |
| `### Task N:` ≥ 1 | **6** | ✅ |
| `- [ ]` checkbox ≥ 1 | **35** | ✅ |
| Header Goal / Architecture / Tech Stack | **3/3** 齐 | ✅ |
| 每个 Task 有 `**Files:**` | **6/6** | ✅ |
- **判定**：✅ **PASS**

### 判据 ⑤：plan 无占位符（skill 明列的 9 类 plan 失败标志）

- **证据**：`grep -nE "TBD|TODO|implement later|fill in details|Similar to Task|Add appropriate|add validation|handle edge cases|Write tests for the above|待补"` 扫 plan + 三件套 + delta ⇒ **空**
- **判定**：✅ **PASS**

### 判据 ⑥：遗留清扫（builder 自查项 · 本次范围内）

| 项 | 取证 | 判定 |
|---|---|:--:|
| TODO / FIXME / XXX 残留 | `grep -rnE "TODO|FIXME|XXX"` 于 change 目录 + plan ⇒ **空** | ✅ |
| 临时调试日志（`static FILE* diag` / `printf` / `std::cerr`） | 同上 grep ⇒ **空** | ✅ |
| 注释掉的代码 / 被 skip 的测试 | 本次**零 C++ 代码改动**（`git status` 无 `.cc/.hh`）⇒ 不适用 | ✅ |
| 未替换的 mock / 临时 fallback | 同上，不适用 | ✅ |
| 未跟踪的临时文件 | `git status --porcelain` ⇒ 仅 plan / state / change 目录（**均为交付物**），⛔ 无 `/tmp` 残留进仓 | ✅ |

### 判据 ⑦：仓内路径同步闸（`docs_sync_check.sh --strict`）

- **证据**：⇒ `✅ AGENTS.md 结构 / 体量 / 行长全部合规` · **rc=0**
- **判定**：✅ **PASS**

### 判据 ⑧：In Scope / Out of Scope 与 improvement 逐字一致

- **证据**：`diff` 于 improvement 与 proposal 的两段 ⇒ **唯一差异是标题标点**（`**In Scope（6 项 · contract 面）**` → `**In Scope**（6 项 · contract 面）：`、`**Out of Scope**` → `**Out of Scope**：`）⇒ **6 项 In Scope + 5 项 Out of Scope 的正文逐字一致**
- **判定**：✅ **PASS**（标点差异 = 仓内 house style 修正，不改语义）

### 判据 ⑨：范围对账 —— ADR SU ③ 的 7 面落地清单 ↔ 本 change 的 6 项 In Scope

| ADR-CEP-11 SU ③ 面 | 本 change 落点 | 判定 |
|:--:|---|:--:|
| ① 本 SU（决策 3 / IC1 / 决策 5 三处）| ✅ 已落盘（`ADR-CEP-11` SU(2026-11-26)）· 本 change **零改动** | ✅ |
| ② schema `pattern` 必改 | In Scope #1 | ✅ |
| ③ 共享 helper（单一实现）| In Scope #5 + design.md D-1/D-2 | ✅ |
| ④ `test_cep_schema.cc:573` IC1 通用闸 | In Scope #3 | ✅ |
| ⑤ `test_cep_schema.cc:512` pattern 容纳性用例 | In Scope #3 | ✅ |
| ⑥ `openspec/specs/cep-core-contracts/spec.md:335-339` IC1 Scenario | In Scope #4 + change delta | ✅ |
| ⑦ 完备性口径（1~6 同一批落地）| tasks.md Task 1~6 同批 | ✅ |
- **判定**：✅ **PASS**（7/7 覆盖，无遗漏面）

---

## ⛔ 未覆盖项（**非本次范围** · 明确登记）

| # | 未覆盖 | 原因 | 何时验 |
|:--:|---|---|---|
| 1 | AC-1 `family` 4 值（`protocol_family_id(UserDefined) == 4` 等）| **实现未开始**（Task 1 未执行）| P2 实施后 |
| 2 | AC-2 `protocol_version` 两分支 + 4 条反向 fixture | 同上（Task 2/3/4）| P2 |
| 3 | AC-3 既有 8 实例零回归 | 同上（Task 4）| P2 |
| 4 | AC-4 main spec IC1 Scenario 已两分支化 | 同上（Task 6）| P2 |
| 5 | AC-5 两分支谓词唯一实现 | 同上（Task 5）| P2 |
| 6 | AC-6 五步链 rc=0 | 同上（Task 6 · 需台账 Entry 授权）| P2 + 归档 |
| 7 | AC-7 `[cep][schema][ic1]` 全绿 | 同上 | P2 |
| 8 | 全量 ctest / freeze check / docs_sync / format | 同上 | P2/P3 |

⇒ **本报告不构成「实现达标」结论**；实现层 AC 的权威判定在 **P3 archive gate**（`rdd-verifier` skill 路径 · 需 `iteration.json` 存在 + `tasks_done == tasks_total > 0`）。

---

## ⚠️ 存量问题（**非本次引入** · 登记不修）

| # | 现象 | 取证 | 处置 |
|:--:|---|---|---|
| 1 | `format.sh --check` 报 **5 个文件**需格式化 | `test/test_cep_compound_graph_falsification.cc` / `test_cep_engine_capabilities.cc` / `test_cep_fence_effects.cc` / `test_cep_hop_timing_capabilities.cc` / `test_cep_ssp_declaration.cc` | **本次零 C++ 改动** ⇒ 存量问题；⛔ 不在本次范围内顺手修（避免扩大 scope / 污染变更集）· 登记待独立批次 |
| 2 | `.rddf/state/iteration.json` 不存在 ⇒ P0 未把 change 登记进 iteration | P0 输出 `⚠️ phase0: iteration.json not found — change not registered`；P1 同款告警 | **非阻塞**（脚本设计为 non-blocking）· `rddf status` 已能列出该 change（`📋 planned 0/40`）· iteration 登记在 archive 阶段同步 |

---

## 结论

**立项产物验收：PASS**（9/9 判据取证通过 · 0 项失败）

- `openspec validate --strict` rc=0 · delta 形态与场景名守恒正确 · plan 满足消费者契约且零占位符 · 遗留清扫零残留 · ADR SU 的 7 面落地清单 100% 覆盖
- **⛔ 本结论不覆盖实现层 AC**（实现未开始）—— 实现层判定归 P3 archive gate

**路由**：本 change 已就绪于 **P1.5**（`execution_mode = worktree` · 0 blockers · 0 manual deps）⇒ 下一步 = **P2 实施**（需用户切 builder 并授权 worktree + 台账 Entry 冻结面授权）。
