---
name: cpptlm-cep-canonical-flow-parser-scope
priority: P1
phase: phase-10
依赖:
  - ADR-CEP-02
  - ADR-CEP-01
状态: 🟡 **草稿已就绪**(2026-11-02)—— **待 planner 落** `.rddf/improvements/`
created: 2026-11-02
---

# improvement: cpptlm-cep-canonical-flow-parser-scope

**主题**: CEP 判据 / 契约**基础设施**修复 —— CP-02「canonical 10 流零 diff」的解析器**范围限定**。

> **状态**: 🟡 草稿已就绪(2026-11-02,staged 于 `.rddf/drafts/cpptlm-cep-canonical-flow-parser-scope/`)
> · **待 planner 落** `.rddf/improvements/cpptlm-cep-canonical-flow-parser-scope.md`(planner 域)
> **台账**: `obj-cep-platform.md` **Entry #149**(裁决 + 陷阱条件精确化 + 修法验证矩阵)
> **KPI**: 占 **1 slot**(代码改动非 doc-only ⇒ 须走 OpenSpec;并发上限 1,per `obj §5.4`;当前实测 **0/3**)
> **估时**: **1 周**(Phase 0 治理 0.5d + Phase 1 修法 1d + Phase 2 判据 1d + Phase 3 连带 0.5d + Phase 4 验收归档 1d)
> **不属** §4.0 的 14 个主线协议族队列 ⇒ **不触及** `ADR-CEP-00 §2 Decision 3` 的治理口径
> (照先例 `cpptlm-test-sh-arg-parsing-timeout` = 非 CEP 主线能力的修复 change)

---

## ① Why

### ①.1 判据会把**合法**的 ADR 追加判成红

`ADR-CEP-02` 是**不可变 ADR** ⇒ 状态变化**只能追加** `## Status Update` 段(per `ADR-CEP-00 §8.1`)。
而 CP-02 判据的解析器 `parse_canonical_flow_names`(`include/tlm/cep/collective_plane_contract.hh:388-427`)
**扫描全文每一行**,把形如 `| <ASCII 数字> | <名字> |` 的行一律当作 canonical 流名行
⇒ **任何**在 ADR 里追加的、首格为 ASCII 数字且第二格为**新标识符**的表格,都会让 CP-02 判据红
⇒ **判据惩罚了 ADR 的正常演进**。

### ①.2 该陷阱**已经**咬过一次,且当前规避**不可持续**

- **已咬**:2026-11-02 批 2(台账 Entry #141)给 `ADR-CEP-02` 追加 SU 后,`ctest` **131 → 129**,
  5 条断言红(per Entry #142 ②)。
- **当前规避 = 文档侧人肉约定**:把 SU 内 3 行表格的首格由 ASCII 数字改成**圈号 `①②③`**。
  ⚠️ 这之所以有效,**仅因为 C++ 判据是 ASCII 数字**(`c < '0' || c > '9'`)—— 圈号不是 ASCII 数字。
  ⇒ **不是**范围限定带来的保护 ⇒ 下次作者写 `| 1 | ... |` 就会复发。
- ⇒ Entry #142 ④ 登记的正确修法 = **改代码** ⇒ 本 improvement。

### ①.3 触发面比原记录**更宽**(本 improvement 的复核发现)

Entry #142 ④ 记的触发面是「SU」。实测复核:现行实现扫**全文** ⇒ `## 3.` 之后**任意段**
(§3 ~ §7 / SU / 未来附录)追加同类表格**同样触发** ⇒ **覆盖面被低估**。
⇒ 修法必须一次性挡住**全部非 §2 段**,而不是只挡 SU。

### ①.4 这是**基础设施**缺陷,不是单点 bug

同一「全文扫描表格行」的解析思路在 CEP 的判据层是**可复用模式**;若不在此处一次性修死,
后续任何「解析 ADR 表格」的判据都会重蹈 ⇒ 修一次,**堵一类**。

---

## ② What Changes

| 面 | 改动 |
|---|---|
| **解析器** | `parse_canonical_flow_names` 的解析范围**限定为 `## 2.` 段**(到下一个 `## ` 为止)⇒ 只解析 canonical 10 流表的**权威位置** |
| **判据(正向)** | 真实 `ADR-CEP-02` 解析出**恰好 10** 条 + 10 个流名各出现 1 次(零回归) |
| **判据(反向 / 回归)** | 4 个**合成 ADR 文本**用例:① 干净 §2 段(正控)② SU 内追加 `| 11 \| `fake_flow` \|` ③ §3 内追加 `| 12 \| `fake_flow2` \|` ④ 无 §2 段 ⇒ 前三者解析集**恒为 10 且不含注入名**;第 ④ 个**安全失败**(0 条 ⇒ 判据红 + 提示「权威源格式已变」) |
| **零改动** | `ADR-CEP-02` / `ADR-CEP-01` **正文零改动** · canonical 10 流**清单零改动** · `collective_plane_contract.hh` 的**其余判据零改动** · **0 新增 ABI** · **0 新增 invariant 编号数** |

> ⚠️ **反向 fixture 的形态与既有 `*_reverse_verify.sh` 相反**:那些是「毒化 ⇒ 判据**必红**」(鉴别力);
> 本 change 修的是**误报** ⇒ 正确形态是「毒化 ⇒ 判据**仍绿**」⇒ **不新建** reverse_verify 脚本,
> 改为**同源**的合成文本用例(随 ctest 跑)⇒ **铁律 ②** 由「每条毒化用例都断言解析集 == 10 且不含注入名」满足。

---

## ③ Acceptance

| # | 判据 | 判据形式 |
|---|---|---|
| **CFP-01** | 解析范围 SHALL 限定于 `## 2.` 段 | 合成用例:§2 段外的表格**不**进入解析集 |
| **CFP-02** | SU / §3+ 追加表格 SHALL NOT 影响解析集 | 合成用例 ×2(各自注入新标识符)⇒ 解析集恒 == 10 |
| **CFP-03** | 权威源结构异常 SHALL **安全失败** | 合成用例:无 `## 2.` 段 ⇒ 解析集 == 0 ⇒ `evaluate_canonical_flows` = **FAIL** + detail 含「权威源格式已变」 |
| **CFP-04** | 真实 `ADR-CEP-02` **零回归** | 既有断言 `flows.size() == 10` + 10 个流名各 1 次**保持绿** |

⇒ **验收命令**:`ctest --test-dir build` 全绿 + `openspec validate cpptlm-cep-canonical-flow-parser-scope --strict` rc=0
+ 门禁全 rc=0(`cep_docs_sync_check --strict` / `cep_gate_selftest` / `cep_doc_ref_check --strict` /
`docs_sync_check --strict` / `cep_freeze_check` / `cep_decl_alignment_check`)+ `format.sh --check` rc=0。

---

## ④ Capabilities

| capability | 内容 | 状态 |
|---|---|---|
| `cep-canonical-flow-parser-scope` | CP-02 解析器的**范围限定契约** + 4 条判据(CFP-01 ~ CFP-04) | 🟡 新增(本 change 的 delta spec) |

---

## ⑤ Impact

| 面 | 影响 |
|---|---|
| **代码** | `include/tlm/cep/collective_plane_contract.hh` —— **1 个 inline 函数**的解析范围(其余判据**零改动**) |
| **测试** | `test/test_cep_collective_plane_contract.cc` —— 既有用例**零改动**(只增 4 个合成用例) |
| **冻结面** | 预期 **0 件**(`ADR-CEP-02` 零改动;`collective_plane_contract.hh` **不在** M1 冻结集内——**须实测确认**) |
| **契约面** | **0 新增** ABI / **0 新增** invariant 编号数 / canonical 10 流清单**零改动** |
| **KPI** | 1 slot(Proposed 并发 **0/3 → 1/3** ⇒ 归档后回 **0/3**) |
| **风险** | 判据**放宽** ⇒ 若 `ADR-CEP-02` 的 §2 段表格被**移位/改名**,解析集变化 ⇒ **安全失败**(0 条 ⇒ FAIL)而非静默通过 ⇒ **不引入漏检** |

---

## ⑥ 遗留(本 change **不**处理,仅登记)

- **B5「闸边表 ≠ SU 边表」**:`protocol_instances.json` 的 `compute_noc` 边集与 `ADR-CEP-09` SU 的 4 条边**仍不同**
  ⇒ **显式登记**(不改代码)⇒ 见 tasks Phase 3。
- **G-3**(IC3 变更留痕无自动闸)—— 开放,per §8.1 R2。
