---
name: cpptlm-cep-canonical-flow-parser-scope
priority: P1
phase: phase-10
依赖:
  - ADR-CEP-02
  - ADR-CEP-01
状态: ✅ **已完成并归档**(2026-11-02,per 台账 Entry #149 / **#150** / **#152**)—— 组 1;Phase 0 ~ 4 全完成;delta capability `cep-canonical-flow-parser-scope` 已合并进 `openspec/specs/cep-canonical-flow-parser-scope/spec.md`
created: 2026-11-02
---

# improvement: cpptlm-cep-canonical-flow-parser-scope

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **状态**: 🟡 进行中(2026-11-02)· **组 1**(判据 / 契约**基础设施**修复)
> **台账**: `obj-cep-platform.md` **Entry #149**(裁决 + 陷阱条件精确化 + 修法验证矩阵)· **#150**(Phase 1 ~ 3 落修)
> **KPI**: 占 **1 slot**(代码改动非 doc-only ⇒ 须走 OpenSpec;并发上限 1,per `obj §5.4`;实测 **1/3**)
> **估时**: **1 周**(Phase 0 治理 0.5d + Phase 1 修法 1d + Phase 2 判据 1d + Phase 3 连带 0.5d + Phase 4 验收归档 1d)
> **不属** §4.0 的 14 个主线协议族队列 ⇒ **不触及** `ADR-CEP-00 §2 Decision 3` 的治理口径
> (照先例 `cpptlm-test-sh-arg-parsing-timeout` = 非 CEP 主线能力的修复 change ⇒ 不进 §4.0)
> **change**: `openspec/changes/cpptlm-cep-canonical-flow-parser-scope/`
> (4 件套 + delta capability `cep-canonical-flow-parser-scope`)

---

## 架构依据

| 依据 | 来源 | 本 change 用到的决策要点 |
|---|---|---|
| `ADR-CEP-02` | §2 决策(canonical 10 条抽象事务流清单)· `## Status Update(2026-11-02,per 台账 Entry #141)`(R-T4 裁决) | canonical 10 流表的**权威位置** = `## 2.` 段(行 40 ~ 49);**ADR 不可变** ⇒ 状态变化**只能追加** `## Status Update` ⇒ 该 ADR 的**合法演进**必须**不影响** CP-02 的解析结果。⚠️ 本 change **不改** `ADR-CEP-02` 的**一个字** |
| `ADR-CEP-01` | §3.3(collective 侧不扩事务集) | CP-02 判据的**存在意义** = collective 旁路平面**不得**出现 canonical 流名 ⇒ 判据**不可**因 ADR 的合法追加而失效 |
| `ADR-CEP-00 §8.1` | 决策 / 状态更新协议 | ADR **不可变** ⇒ 状态变化**只能追加** `## Status Update` 段 —— 这是本 change 根因的**制度前提** |
| 台账 Entry #142 ④ | 「已登记的潜在陷阱」(**唯一显式 `⬜ backlog`**) | 陷阱的正确修法 = 让解析器**只解析**权威段 ⇒ **属代码改动**,须**独立 change + 反向 fixture** ⇒ 本 change 即该 change |
| 台账 Entry #141 ② | 「闸边表 ≠ SU 边表」 | 「装配闸通过」**不得**作为 SU 论证正确的证据 ⇒ 本 change 的判据**只**对**权威位置**负责(收窄),**不**扩张到其它段 |
| 台账 Entry #149 ② ③ | 陷阱条件**精确化** + 修法**验证矩阵** | 本 change 的**唯一**依据面:触发面 = **不止 SU**(§3+ 任意段)+ 触发条件 = 首格 ASCII 数字 **且** 第二格为新标识符;选定修法 = **只解析 `## 2.` 段** |
| `openspec/specs/cep-collective-plane-contract/` | 冻结面 main spec(判据契约) | 本 change 的 delta capability = `cep-canonical-flow-parser-scope`;**0 新增** ABI · **0 新增** invariant 编号数 |

---

## 范围

### In Scope

- `include/tlm/cep/collective_plane_contract.hh` —— **1 个新辅助函数**(`slice_canonical_flow_section`)+ `parse_canonical_flow_names` 的**入参**改为切片结果(解析范围限定)
- `test/test_cep_collective_plane_contract.cc` —— **1 个新 TEST_CASE**(4 个 SECTION)= CFP-01 ~ CFP-04;既有用例**零改动**
- 判据形态:**合成 ADR 文本**用例(**毒化 ⇒ 判据仍绿**的**回归防护**形态)—— 与既有 `*_reverse_verify.sh` 的**鉴别力**形态**相反**
- 台账登记:B5「闸边表 ≠ SU 边表」现状复核 + 存量问题(`collective_plane_contract.hh` 与 `.clang-format` 不兼容)

### Out of Scope

- ❌ **不改** `ADR-CEP-02` / `ADR-CEP-01` 的**任何**正文(含 `## 2.` 段的表格)
- ❌ **不改** canonical 10 流的**清单内容**(不增删流)
- ❌ **不改** `evaluate_canonical_flows` 的 collective 侧扫描逻辑(词边界 / 多形态匹配)
- ❌ **不新建** `*_reverse_verify.sh` 脚本(本组修的是**误报**,不是加判据)
- ❌ **不**改其它任何解析 ADR 表格的判据(本 change 只修 CP-02 这一处**已实测**的实例)
- ❌ **0 新增** ABI · **0 新增** invariant 编号数 · **不**产出性能结论

---

## Why

### ① 判据会把**合法**的 ADR 追加判成红

CP-02 的解析器 `parse_canonical_flow_names`(`include/tlm/cep/collective_plane_contract.hh:388-427`)
**扫描全文每一行**,把 `| <ASCII 数字> | <名字> |` 形状的行一律当作 canonical 流名行。
`ADR-CEP-02` 是**不可变 ADR** ⇒ 状态变化**只能追加** `## Status Update` 段
⇒ **任何**此类追加都会被判据惩罚 ⇒ **判据惩罚了 ADR 的正常演进**。

### ② 该陷阱**已经**咬过一次;当前规避**不可持续**

2026-11-02 批 2(台账 Entry #141)给 `ADR-CEP-02` 追加 SU 后,`ctest` **131 → 129**、5 条断言红(Entry #142 ②)。
当前规避 = 把 SU 内 3 行表格的首格由 ASCII 数字改成**圈号 `①②③`**
⇒ 这之所以有效,**仅因为 C++ 判据是 ASCII 数字**(`c < '0' || c > '9'`)—— 圈号不是 ASCII 数字
⇒ **不是**范围限定带来的保护 ⇒ 下次作者写 `| 1 | ... |` 就会**复发**。

### ③ 触发面比原记录**更宽**(本 change 的复核发现)

Entry #142 ④ 记的触发面是「SU」;**实测复核**:现行实现扫**全文** ⇒ `## 3.` 之后**任意段**
(§3 ~ §7 / SU / 未来附录)追加同类表格**同样触发** ⇒ **覆盖面被低估**
⇒ 修法必须一次性挡住**全部非 §2 段**,而不是只挡 SU。

### ④ 这是**基础设施**缺陷,不是单点 bug

「全文扫描表格行」的解析思路在 CEP 的判据层是**可复用模式**;若不在此处一次性修死,
后续任何「解析 ADR 表格」的判据都会重蹈 ⇒ 修一次,**堵一类**。

### ⑤ 修法必须**失败方向安全**

判据的**放宽**有风险:若解析范围收窄后**取错了表**,判据会**静默通过** ⇒ 漏检。
⇒ 选定修法必须满足「结构异常 ⇒ **红**」而非「结构异常 ⇒ 绿」。

---

## What Changes

| 面 | 改动 |
|---|---|
| **解析器** | 新增 `slice_canonical_flow_section()`(截取 `## 2.` 段,到下一个 `## ` 为止)+ `parse_canonical_flow_names` 的入参改为切片结果 ⇒ **一次挡住 SU 与 §3+ 全部段** |
| **判据(正向)** | 真实 `ADR-CEP-02` 解析出**恰好 10** 条 + 10 个流名各出现 1 次(**零回归**) |
| **判据(回归 / 反向)** | **4 个合成 ADR 文本用例**:① 干净 §2 段(正控)② SU 内追加 `| 11 \| `fake_flow` \|` ③ `## 3.` 内追加 `| 12 \| `fake_flow2` \|` ④ 无 `## 2.` 段 ⇒ 前三者解析集**恒为 10 且不含注入名**;第 ④ 个**安全失败**(0 条 ⇒ 判据红 + detail 提示「权威源格式已变」) |
| **零改动** | `ADR-CEP-02` / `ADR-CEP-01` 正文 · canonical 10 流清单 · `evaluate_canonical_flows` 的 collective 侧扫描逻辑 · **0 新增** ABI · **0 新增** invariant 编号数 |

> ⚠️ **反向 fixture 的形态与既有 `*_reverse_verify.sh` 相反**:那些是「毒化 ⇒ 判据**必红**」(鉴别力);
> 本 change 修的是**误报** ⇒ 正确形态 = 「毒化 ⇒ 判据**仍绿**」(回归防护)
> ⇒ **不新建**脚本,改为**同源**的合成文本用例(随 ctest 跑 ⇒ **零副作用**、不碰真实 ADR 文件)
> ⇒ **铁律 ②** 由「每个毒化用例都断言解析集 == 10 **且**不含注入名」满足
> (若解析器退化为「仍收 §2 段外的表格」⇒ 集合大小变 11 ⇒ 用例**必红且归因命中**)。

---

## Acceptance

- [x] **CFP-01** 解析范围 SHALL 限定于 `## 2.` 段 —— §2 段外的表格**不**进入解析集(合成用例)
- [x] **CFP-02** SU / `## 3.`+ 追加表格 SHALL NOT 影响解析集 —— 解析集恒 == 10 **且**不含注入名(2 个合成用例)
- [x] **CFP-03** 权威源结构异常 SHALL **安全失败** —— 无 `## 2.` 段 ⇒ 0 条 ⇒ `evaluate_canonical_flows` = **FAIL** + detail 含「权威源格式已变」
- [x] **CFP-04** 真实 `ADR-CEP-02` SHALL **零回归** —— 既有断言 `flows.size() == 10` + 10 个流名各 1 次**保持绿**
- [x] **TDD 红已见证** —— CFP-02 两例在**现行实现**上实测红(`11 == 10` + 注入名计数 `1 != 0`)⇒ **归因命中**
- [x] **冻结面实测 = 0 件** —— `include/tlm/cep/collective_plane_contract.hh` **不在** M1 冻结集内 ⇒ **无需两步授权**
- [ ] **Phase 4 验收 + 归档** —— 全门禁 rc=0 + `ctest` 全绿 + KPI 1/3 → 0/3 + delta capability 合并进 `openspec/specs/`

---

## Capabilities

| capability | 内容 | 状态 |
|---|---|---|
| `cep-canonical-flow-parser-scope` | CP-02 解析器的**范围限定契约** + 4 条判据(CFP-01 ~ CFP-04) | 🟡 新增(本 change 的 delta spec) |

---

## Impact

| 面 | 影响 |
|---|---|
| **代码** | `include/tlm/cep/collective_plane_contract.hh` —— **1 个新 inline 函数** + 1 行调用替换;最终 diff **39 insertions / 1 deletion** |
| **测试** | `test/test_cep_collective_plane_contract.cc` —— 既有 9 cases / 120 assertions **零改动**;新增 1 TEST_CASE(4 SECTION)= **18 assertions** |
| **冻结面** | **0 件**(实测 M1 冻结集含 `include/` 下仅 3 件)⇒ **无需两步授权** |
| **契约面** | **0 新增** ABI · **0 新增** invariant 编号数 · canonical 10 流清单**零改动** |
| **KPI** | 1 slot(Proposed 并发 **0/3 → 1/3** ⇒ 归档后回 **0/3**)+ 7 处 baseline 同步 |
| **风险** | 判据**放宽** ⇒ 若 `## 2.` 段被**移位/改名**,解析集变化 ⇒ **安全失败**(0 条 ⇒ FAIL)而非静默通过 ⇒ **不引入漏检** |
| **存量问题(仅登记)** | `include/tlm/cep/collective_plane_contract.hh` **与 `.clang-format` 不兼容** ⇒ 对它跑 `clang-format -i` 会产生 **1737 行**整文件重排 ⇒ 本次已 `git checkout` 还原 + 只重放语义改动;⚠️ 批量格式化该文件前须先评估(`format.sh --check` 对它 rc=0 ⇒ 不影响门禁) |

---

## 遗留(本 change **不**处理,仅登记)

- **B5「闸边表 ≠ SU 边表」** —— `protocol_instances.json` 的 `compute_noc` 边集与 `ADR-CEP-09` SU 的 4 条边**仍不同** ⇒ 显式登记(台账 Entry #150 ④),不改代码
- **G-3**(IC3 变更留痕无自动闸)—— 开放,per §8.1 R2
- **A12** 台账 Entry #143 ⑥ item 3 的「§5.1.1 行数 `== 8` ⇒ 9」预测**不成立** —— 已由 Entry #148 澄清
