# cpptlm-cep-canonical-flow-parser-scope: CP-02「canonical 10 流零 diff」解析器的**范围限定**(契约 / 判据基础设施修复)

> **状态**: 🟡 **Proposed**(2026-11-02)· **台账**: `obj-cep-platform.md` **Entry #149**
> **口径**: 台账 **Entry #142**(陷阱登记)· **#141**(`ADR-CEP-02` 的 R-T4 SU —— **本 change 的诱因**)· **#148**(组 0 复盘登记 B1)
> **规划输入**: `.rddf/drafts/cpptlm-cep-canonical-flow-parser-scope/improvement.md`(**草稿已就绪,待 planner 落** `.rddf/improvements/`)
> **审计面**: 台账 **Entry #149 ② ③**(陷阱条件精确化 + 修法验证矩阵)—— 本 change 的**唯一**依据面
> **估时**: **1 周** · **KPI**: Proposed 并发 **0/3 → 1/3**
> **不属** §4.0 的 14 个主线协议族队列 ⇒ **不触及** `ADR-CEP-00 §2 Decision 3` 的治理口径
> **冻结面**: 预期 **0 件**(`ADR-CEP-02` 正文零改动;`collective_plane_contract.hh` 不在 M1 冻结集内 —— **须实测确认**)

---

## Why

### ① 判据会把**合法**的 ADR 追加判成红

CP-02 的解析器 `parse_canonical_flow_names`(`include/tlm/cep/collective_plane_contract.hh:388-427`)
**扫描全文每一行**,把 `| <ASCII 数字> | <名字> |` 形状的行一律当作 canonical 流名行。
`ADR-CEP-02` 是**不可变 ADR** ⇒ 状态变化**只能追加** `## Status Update`
⇒ **任何**此类追加都会被判据惩罚 ⇒ **判据惩罚了 ADR 的正常演进**。

### ② 该陷阱**已经咬过一次**;当前规避**不可持续**

2026-11-02 批 2(Entry #141)给 `ADR-CEP-02` 追加 SU 后,`ctest` **131 → 129**、5 条断言红(Entry #142 ②)。
当前规避 = 把 SU 内 3 行表格首格由 ASCII 数字改成**圈号 `①②③`** ⇒ **之所以有效,仅因为 C++ 判据是 ASCII 数字**
(`c < '0' || c > '9'`)⇒ **不是**范围限定带来的保护 ⇒ 下次作者写 `| 1 |` 即复发。

### ③ 触发面比原记录**更宽**(复核发现)

Entry #142 ④ 记的触发面是「SU」;实测:现行实现扫**全文** ⇒ `## 3.` 之后**任意段**
(§3 ~ §7 / SU / 未来附录)追加同类表格**同样触发** ⇒ **覆盖面被低估** ⇒ 修法须一次性挡住**全部非 §2 段**。

### ④ 修一次,堵一类

「全文扫描表格行」的解析思路在 CEP 判据层是**可复用模式**;不在此处修死,后续任何
「解析 ADR 表格」的判据都会重蹈 ⇒ 本 change 的价值不止于修一个函数。

---

## What Changes

| 面 | 改动 |
|---|---|
| **解析器** | `parse_canonical_flow_names` 的解析范围**限定为 `## 2.` 段**(到下一个 `## ` 为止) |
| **判据(正向)** | 真实 `ADR-CEP-02` 解析出**恰好 10** 条 + 10 个流名各 1 次(零回归) |
| **判据(回归)** | **4 个合成 ADR 文本用例**(正控 + SU 注入 + §3 注入 + 结构异常)⇒ 前三者恒 10 条且**不含注入名**;第 ④ 个**安全失败** |
| **零改动** | `ADR-CEP-02` / `ADR-CEP-01` 正文 · canonical 10 流清单 · 其余判据 · **0 新增 ABI** · **0 新增 invariant 编号数** |

> ⚠️ **反向 fixture 形态与既有 `*_reverse_verify.sh` 相反**:那些是「毒化 ⇒ 判据**必红**」(鉴别力);
> 本 change 修的是**误报** ⇒ 正确形态 = 「毒化 ⇒ 判据**仍绿**」⇒ **不新建**脚本,改为**同源**合成文本用例
> (随 ctest 跑)⇒ **铁律 ②** 由「每个毒化用例都断言解析集 == 10 且不含注入名」满足。

---

## Acceptance

| # | 判据 | 形式 |
|---|---|---|
| **CFP-01** | 解析范围 SHALL 限定于 `## 2.` 段 | §2 段外表格**不**进入解析集 |
| **CFP-02** | SU / §3+ 追加表格 SHALL NOT 影响解析集 | 合成用例 ×2 ⇒ 解析集恒 == 10 |
| **CFP-03** | 权威源结构异常 SHALL **安全失败** | 无 `## 2.` 段 ⇒ 0 条 ⇒ **FAIL** + detail 含「权威源格式已变」 |
| **CFP-04** | 真实 `ADR-CEP-02` **零回归** | 既有断言 `flows.size() == 10` + 10 个流名各 1 次**保持绿** |

⇒ `ctest` 全绿 · `openspec validate <change> --strict` rc=0 · 门禁全 rc=0 · `format.sh --check` rc=0。

---

## Impact

| 面 | 影响 |
|---|---|
| **代码** | `include/tlm/cep/collective_plane_contract.hh` —— **1 个 inline 函数** |
| **测试** | `test/test_cep_collective_plane_contract.cc` —— 既有用例**零改动** + 4 个合成用例 |
| **契约** | **0 新增** ABI · **0 新增** invariant 编号数 · canonical 10 流清单**零改动** |
| **KPI** | 1 slot(0/3 → 1/3 ⇒ 归档回 0/3) |
| **风险** | 判据**放宽** ⇒ §2 段被移位/改名 ⇒ **安全失败**(0 条 ⇒ FAIL)而非静默通过 ⇒ **不引入漏检** |
