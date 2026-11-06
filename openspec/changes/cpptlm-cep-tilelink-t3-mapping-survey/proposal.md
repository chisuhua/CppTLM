# cpptlm-cep-tilelink-t3-mapping-survey — TileLink-C「CohIf 消息/意图面 → 载体事件轴」映射锚点普查(T3)

> **状态**: 🟡 **Proposed**(2026-11-06)· **来源**: `obj-cep-platform` 台账 **Entry #239** §⑥ 步骤 1
> **配套**: `design.md`(待补)· `tasks.md`(待补)· 审计面 = **本 change 新建**的锚点摘要
> **口径**: 台账 **Entry #237**(事实底稿)· **#238**(D-1「转移表真源」T1–T4 判定式)· **#239**(9 键补裁 + 路径 A)· **#240**(路径 A 落地 + 订正 #1 + B3a 收口)
> **裁决源**: `docs/architecture/07-coherence-exploration-platform.md` **Status Update(2026-11-06)** 的 **§⑤ T3 普查三要素**
> **估时**: **2 周** · **KPI**: Proposed 并发 **0/3 → 1/3**
> **性质**: **规约层普查**(只产锚点 + 载体),⛔ **不产引擎**

---

## Why

### ① B3b 的触发条件对 TileLink-C **已客观成立**,但剩余前置被卡在一层**仓内不存在的映射**

`ADR-CEP-07` Status Update 2026-11-04 ① 与 `07 号` **Status Update(2026-11-06)** 的 **T1–T4** 判定式共同确定:

| 要件 | TileLink-C | 依据 |
|---|:--:|---|
| **T1 存在**(有限封闭的转移结构) | ✅ | `include/tlm/cep/tilelink_state_spec.hh` 的 **12 格**矩阵(3 态 × 4 事件) |
| **T2 可判定**(逐格 basis + 条款级 ref) | ✅ | 逐格 `basis` 全 `Direct` · `ref` → 锚点摘要 `TLC-01`~`TLC-12` |
| **T3 可消费**(平台消息/意图面 → 表索引轴的映射) | **❌** | ⛔ **该映射在仓内任何载体都不存在** |
| **T4 完备**(`Unknown` 格占比 = 0) | ✅ | 12 格全 `Known` · 0 格 `Unknown` |

⇒ **2026-11-04 裁决「规约载体无转移表真源 ⇒ 不补引擎」对 TileLink-C 不成立**(事实错误,per 台账 Entry #237 §1.2)
⇒ 但**也不能立即造引擎** —— T3 未锚定前造引擎 = 换一种推理填空(2026-11-04 裁决禁止的正是这个)。

### ② 具体缺的是什么

`tilelink_state_spec.hh` 的矩阵索引轴 = **权限操作**(`Cap` / `Grow` / `Prune` / `Report`);
而运行时引擎的消费面 = **CohIf 消息/意图**(per `protocol_engine_base.hh` 的 `on_request` / `on_snoop` /
`on_writeback` / `on_barrier` 等 10 流 `on_*` 虚函数)。
⇒ 「**CohIf 消息类 → `Cap`/`Grow`/`Prune`/`Report` 事件**」的映射行,**一行都不存在**。

### ③ ⚠️ 不可把 `ADR-CEP-12` 的既有映射表误当 T3 已满足

`tilelink_c_adapter.hh` 的「10 行逐消息表」(`ADR-CEP-12` Status Update 2026-11-01,per Entry #138)把
**协议消息类**映射到 **`ADR-CEP-12` 的 6 类消息类** —— **目标轴与本 change 的 Event 轴不同** ⇒
**两轴不同 artifact** ⇒ 该表**不满足 T3**,⛔ 不得据「adapter 已有 10 行表」宣称 T3 已满足。

### ④ 规范可读 ⇒ 这是一项**低成本**普查,**不属**填空

TileLink-C 是 **SiFive 免费开放标准**(规范正文可读,钉版 `tilelink-c-1.7.1`,PDF sha256 钉版)
⇒ 映射行**可以**取得条款级 basis ⇒ ⛔ 与「规范禁读」情形(CXL 全程禁读 / UALink 受 Evaluation Copy 约束)**完全不同**
⇒ 本 change 是**能闭合**的普查,不是填空。

---

## What Changes

- **新增**锚点摘要 `docs/research/2026-11-06-cep-tilelink-t3-mapping-anchor-summary.md`
  —— 逐行记录「**CohIf 消息类 / 意图 → `Cap`/`Grow`/`Prune`/`Report` 事件**」的映射行
  (行 id 形如 `TLC-M-01` …),每行含:`basis.kind`(`Direct` / `Analogical` / `SelfDerived` / `Pending`)+ **条款级 `ref`**。
- **新增**规约载体扩展 `include/tlm/cep/tilelink_state_spec.hh` 的**映射表**部分
  (逐行 `basis` + `ref` + 编译期锁:行数 / 唯一性 / `basis`↔`ref` 类别自洽 / `Pending` 行**不得**被消费者消费)。
- **不新增**任何 `INV-CEP-*` 编号(本 change 只产锚点与载体)。
- **不改**任何 main spec(⇒ **不触发** M1 冻结面授权);`openspec/specs/` 的 delta **归后续的引擎 change**。

### ③ T3 普查的三要素(per `07 号` SU 2026-11-06 §⑤,缺一即为「下一个未定义术语」)

| 要素 | 本 change 的具体化 |
|---|---|
| **行宇宙** | 「平台消息/意图面 → Event 轴」的**全部行** = 哪些消息类/意图 × 4 个事件 —— **逐行列出,不是抽样**;行数与来源消息类词表(`ADR-CEP-12` 决策 1 的 6 类消息类 + `ADR-CEP-12` 决策 2 的 atomic 意图 + 4 个操作类)逐一对齐 |
| **锚标准** | 合法锚 = **规范条款级定位**;若某行只靠仓内构造(如 `ADR-CEP-12` 定义的 6 类消息类)⇒ 须标 `self-derived` 并**显式登记**,⛔ 不得当规范锚 |
| **交付物与登记位置** | 映射表进**规约载体**(编译期锁)+ 逐行 `basis` + `ref`;⛔ **悬空判据禁令**(per `ADR-CEP-12` 附录)⇒ 载体不存在前该判据**不可执行** |

---

## Capabilities

- **无 spec delta**(本 change 只产锚点摘要 + 规约载体)⇒ `openspec/specs/` **零改动**。

> ⚠️ **若普查产出全行有锚** ⇒ 后续**另起**引擎 change,其 delta 落 `cep-tilelink-tree-manager` / 新 capability;
> 若产出存在无锚行 ⇒ 该行 `pending` + **TileLink-C 登记 ⏭**(实证结论)⇒ **B3b 维持 1/6**。

---

## Impact

- **受影响载体**:`include/tlm/cep/tilelink_state_spec.hh`(扩映射表部分)· 新增锚点摘要 · 新增测试(`test/test_cep_tilelink_state_spec.cc` 或同域新文件)
- **不受影响**:`protocol_instance.schema.json`(**M1 冻结件**,零改动)· `protocol_instances.json`(**本 change 零改动** —— `ualink_4` 已于 Entry #240 声明)·
  `ADR-CEP-11 §3` 锁表 · canonical 10 流 · `ADR-CEP-04` 的 tracker 选型
- **冻结面**:本 change **零冻结面改动** ⇒ **不触发** `--update --authorized-by`
- **KPI**:Proposed 并发 **0/3 → 1/3**

---

## Out of Scope

- ⛔ **不造运行时引擎**(engine / 装配 / L4 事务级 E2E)—— 那是普查**全行有锚**之后的**另一个 change**
- ⛔ **不改** `protocol_instance.schema.json` / `protocol_instances.json` / `ADR-CEP-11 §3`
- ⛔ **不改**任何 main spec(`openspec/specs/`)⇒ 无 delta 合并
- ⛔ **不碰** CXL 2.0 / CXL 3.0 对称 / NVLink / UALink 四族(其「不成立」已登记 per Entry #238 §⑤)
- ⛔ **不**引入 `INV-CEP-*` 新编号

---

## Acceptance

- [ ] **行宇宙完整**:映射表的**全部行**已列出(与来源消息类/意图词表逐一对齐)⇒ ⛔ 非抽样
- [ ] **逐行 `basis` + 条款级 `ref`**:每一行都有 `basis.kind` 与 `ref`;无规范支撑的行标 `Pending` 或 `SelfDerived` 并**显式登记**
- [ ] **禁冒充**:⛔ **无**任何行以「公开来源(`A-n`)」充当规范依据(照 `ualink_inc_state_spec.hh` 的 `basis_kind_matches_ref_class` 纪律)
- [ ] **载体编译期锁**:行数 / id 唯一 / `basis`↔`ref` 类别自洽 / `Pending` 行**不得**被消费者消费 ⇒ 全部 `static_assert` 落地
- [ ] **决定性变异**:删映射表**任一行** ⇒ 对应判据**按正确原因红**(per 判据铁律 1.1)
- [ ] **反向 fixture**:把任一行的 `basis` 改成冒充形态(如 `SelfDerived` 却引 `Pending` / `Pending` 却引公开来源)⇒ 必红
- [ ] **锚点摘要 ↔ 载体逐项相等**(照 `cep_ualink_inc_state_spec_probe.cc` 的 **X-2 元测试**形态:读锚点摘要解析行集合 ⇔ 载体的行集合)
- [ ] **二分支结论已落**:产出**全行有锚** ⇒ 登记「转引擎 change」的触发;产出**存在无锚行** ⇒ 该行 `pending` + **TileLink-C 登记 ⏭**(reason / trigger_condition / ledger_entry)⇒ **B3b 维持 1/6**
- [ ] `[cep]` 全绿 · `ctest` 全绿 · 映射表闸 `--for-achievement` rc=0
