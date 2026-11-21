# design: cpptlm-cep-invariant-modeling-batch2

> **裁决来源**：`.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §二（D-1 ~ D-5）· 用户 ratify R-2a~R-2f。
> 本文件只记「裁决内容 → 落地形态」的映射 + **实施侧实测的偏离**；⛔ 不重述裁决论证（避免第二真源）。

## D-1 · `combination_contract` 的 schema 形态（裁决 §2.2 的**逐字**落地）

新增顶层属性 `combination_contract`（**可选** · ⛔ 不进 `required`），结构**逐字**取裁决 §2.2 的规范形态：

- `bridging.early_ack`：`enum = ["allow-early-write-ack", "deny-early-completion", "unknown"]`（**恰 3**）· `bridging.required == ["early_ack"]` · `additionalProperties: false` · `minProperties >= 1`。
- `compound_consistency`：`required == ["adopted"]` · `adopted`（boolean）· `edge_classes`（array，items enum `["rfe","fr","ws"]`）· `verification_scope`（string，`minLength >= 1`）· `additionalProperties: false` · `minProperties >= 1`。

⚠️ **两个子对象都带 `minProperties >= 1`** ⇒ 禁「空壳声明」（`{}` 不入库）—— 裁决 §2.2「禁空对象入库（否则『声明了』不可判）」。

## D-2 · `early_ack` 的逐实例取值 = **全 6 个 `unknown`**（P-5 的落地）

**裁决 §2.2 的取值域前两项**（`allow-early-write-ack` / `deny-early-completion`）的措辞锚点是 `ADR-CEP-13` 决策 4 的**行为类**表（两行：「允许提前写 ack」/「不允许提前完成」）—— ⛔ **不是逐协议表**。

**实测（2026-11-21）**：全仓 grep「提前 ack / 提前写 ack / 提前完成 / early_ack」在 `docs/` + `include/` + `openspec/specs/` 下**只命中该 ADR 的行为类表与公理散文**，⛔ **无任何逐协议的一手锚点**。

⇒ 按 **P-5**（「为 `early_ack` 编造协议事实值」禁止）与裁决 §2.2（「**显式 `unknown` 远好于编值**」）⇒ **本仓权威域 6 个实例全部声明 `unknown`**。

⚠️ **诚实边界（AC-9 明文）**：`unknown_count == 6` 是**变更检测器**字段，⛔ **不是**「声明面已有信息量」的证据 ⇒ 台账与报告**不得**表述为「已取得协议事实」。

## D-3 · 载体落点与「咬合」的实现形态（AC-11）

**单一新载体**：`test/test_cep_combination_contract.cc`（`.cc` · `test/` 下 ⇒ 满足 `scan_carriers` 的扫描面与 AC-11 的「载体必须是 `.cc`」）。

两个 `INV-CARRIER` 标记**同文件**（AC-11 允许；⛔ 不强制拆 2 个文件 ⇒ 第二份重复 fixture）：

| 标记 | 邻域断言（**针对该 id 语义**）|
|---|---|
| `// INV-CARRIER: INV-CEP-13-IC3` | ① schema 的 `early_ack` 枚举**恰 3** 值 ∧ **不含** `not-applicable` ② `bridging.required == ["early_ack"]` ③ 本仓权威域**逐个**实例含 `combination_contract.bridging.early_ack`（逐实例值 == fixture）|
| `// INV-CARRIER: INV-CEP-13-IC1` | ① `adopted == true ⇒ edge_classes` 非空 ∧ `verification_scope` 非空（**合成实例**三例，AC-10）② `edge_classes ⊆ {rfe, fr, ws}` ③ `compound_consistency.required == ["adopted"]` |

⚠️ **邻域 ±8 行**（`cep_invariant_map_check.py` 的 `NEIGHBORHOOD`）必须见断言宏 ⇒ 标记与断言**紧邻**（标记下方立即是该 id 的断言块）。

## D-4 · 条件判据（AC-10）的实现形态

`schema` 不写 `if/then` ⇒ 条件落**判据**（裁决 §2.2 必填策略第 3 层）。判据 = **合成实例**三例（⛔ 不入库、⛔ 不用仓内实例 —— 现状无实例声明 `compound_consistency` ⇒ 用仓内实例做反 fixture 会**永远空跑**）。

`valid(x)` 的定义 = **schema 声明的约束**（`required` / `enum` / `type` / `minLength` / `minItems` / `additionalProperties` / `minProperties`）∧ **条件规则**（`adopted == true ⇒ edge_classes 非空 ∧ verification_scope 非空`）。

| 合成实例 | schema 层 | 条件层 | `valid` | 意义 |
|:--|:--|:--:|:--:|---|
| `{adopted:true, edge_classes:[]}` | ✅ 通过（`edge_classes` 无 `minItems`）| ❌ 违反（`edge_classes` 空）| **假** | 条件规则**有牙**（schema 单独抓不到）|
| `{adopted:true, verification_scope:""}` | ❌ 违反（`minLength >= 1`）| ❌ 违反 | **假** | 两层都抓 |
| `{adopted:false}` | ✅ 通过 | ✅ 真空（`false` 允许空）| **真** | ⛔ 不把 `false` 误判为违规 |

⇒ **反 fixture 的落点 = 条件规则那一层**（第一例）—— 若只实现 schema 层 ⇒ 第一例会**恒真** ⇒ 反 fixture 无鉴别力。

## D-5 · 台账锚独立闸（AC-1④）

**为什么必须独立**：`cep_invariant_map_check.py` 的台账锚判据（`:145-151`）**只对仍登记在 deferrals 里的条目执行** ⇒ 本单元把 2 条删光后**一次都不跑**（批 1 已因此另立闸，先例 `scripts/test/cep_invariant_batch1_ledger_check.py`）。

**判据**（照批 1 范式）：同一 `### Entry #<n>` 段内**同时**含 `INV-CEP-13-IC1` 与 `INV-CEP-13-IC3` **且**含本单元的载体文件路径 ∧ 该段**未**把二者表述为「已建模 / 可判」。

⚠️ **必须取编号最大的匹配段**（历史 Entry 提到这 2 条 id 属登记溯源，⛔ 不构成本批锚）—— 台账现存 **13 / 9 处**含这两个 id ⇒ **全文判据恒绿**。

## D-6 · ⚠️ 与 AC-5⑤ 的**实质偏离**（4 个 hash 而非 3 个）

| 事实 | 依据 |
|---|---|
| `openspec/specs/cep-core-contracts/spec.md` **是冻结面成员** | 冻结清单 `globs` #17（`openspec/specs/cep-*/spec.md`）+ **显式成员** #19 |
| 本单元的 schema 变更**必须**进 delta spec | 单元 Capabilities 段：「⚠️ schema 变更属契约面 ⇒ 须在 delta spec 中体现」 |
| delta 归档**必然**改写该 main spec | openspec 合并器把 delta 写回 main spec |

⇒ 冻结清单的 hash 变更为 **4 个**（schema + `ADR-CEP-12` + `ADR-CEP-13` + `cep-core-contracts/spec.md`），⛔ **不是** AC-5⑤ 的「只改 3 个 hash」。

**裁决**：取**「schema 变更须入 delta spec」**（契约面纪律 + 防「假绿在契约层」的形态），并**显式登记**该偏离。理由：

1. AC-5⑤ 的**意图**是「防 `--update` 顺手吸收**无关**漂移」⇒ 4 个 hash **全部相关** ⇒ 意图仍满足。
2. 若跳过 delta ⇒ `cep-core-contracts` 的 IC1 Scenario 仍写「13 个顶层属性(= schema 实际属性数)」⇒ 与 14 化后的 schema **字面相撞** ⇒ 契约面留一处**长期绿的矛盾**（= 本单元要消灭的那类形态）。
3. ⛔ **不新建** `cep-*` main spec（会被 glob #17 捕获 ⇒ `artifact_count` 90 → 91 ⇒ 违反 AC-5①）。

## D-7 · 两处 openspec 工具约束（**I-12 实测的先例**）

1. **MODIFIED 内禁止重命名 Scenario** ⇒ delta 的 IC1 Scenario **标题保留**「schema 必含 7 个字段组 / 13 个顶层属性(IC1)」，改由**正文**写明「13 个**必含**（`required`）属性」+「schema 属性总数 14，第 14 项为可选声明项」；**新增**一个 Scenario 承载 `combination_contract` 的规范条款。
2. **MODIFIED 的 Requirement 标题须存在于现行 spec** ⇒ delta 只改内容、不改 `cep-capability-map-contract` 的标题。

## D-8 · 五步链的 **commit 形态**（照 I-6a 先例 `ab7b649a` → `51a7babd`）

1. 台账 Entry **#514**（**以落库时实测最大值 + 1** 为准，⛔ 不预留编号）落库；
2. 改 schema + 实例 + 测试 + 两份 ADR SU + 新载体 + fixture + 闸 + deferrals + 映射表 + 语料 + CMakeLists；
3. **commit A**（含清单的 `--update` 结果：`artifact_count` 不变 + 4 个 hash 已是新值 + `frozen_at_commit` = 上一 commit）；
4. `--update --authorized-by "Entry #514"` **再跑一次** ⇒ `frozen_at_commit` = commit A 的 SHA；
5. **commit B**（**清单单独 commit** · ⛔ 台账与清单不得合并提交）；
6. 无参复验 rc=0。

⚠️ **顺序不可颠倒**：台账 Entry（授权）⇒ 改产物 ⇒ commit ⇒ `--update` ⇒ 清单 commit。
