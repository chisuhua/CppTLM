---
name: cep-invariant-modeling-batch2
priority: P2
phase: phase-10
category: core-impl
type: feature
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-13
  - ADR-CEP-12
状态: "🟡 **已立项 · 裁决已落 · 已 ratify**(2026-11-21 · architect 裁决 = `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §二(**oracle APPROVE** + **critic APPROVE_WITH_CONDITIONS** · 无 BLOCKER)· **用户 ratify R-2a~R-2f**(2026-11-21)· 前置 ① 裁决落盘 ✅ ② **R-2 已 ratify** ✅ ③ **litmus change 已归档**(= I-6a)✅ · 经 **metis 1 轮**(重写轮 · P0×6/P1×8/P2×3 逐条处置见 §审查轨迹)· ⏳ **待立项**(openspec change)⇒ builder 实施)"
created: 2026-11-20
---

# cep-invariant-modeling-batch2: 2 条守门 invariant 的建模（批 2 · 跨 change 规格修订）

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不改任何已签批 ADR 正文**（`ADR-CEP-12` / `ADR-CEP-13` 只能**追加式**修订，⛔ 不改原文）· ⛔ **新增 invariant 数 = 0**（批 1 已把 `⏭-unmodelled` 从 5 降到 2；本批把最后 2 条变成有载体 ⇒ 仍 66 条 / 编号位 67）· ⛔ **`INV-CEP-04-IC4` 永久跳空** · ⛔ **不改 `INV-CEP-12-IC1` 的语义**（只联动）· ⛔ **第 14 个顶层属性不得进 `required`**（见 §裁决 已落）。
> ⚠️ **码位纪律**：本仓 `⏭`（U+23ED）= 数据与脚本里的 disposition 码位（`test/cep_invariant_deferrals.json`、`scripts/test/cep_invariant_map_check.py:135` 的白名单）⇒ 判据里引用该字面量时**必须用 `⏭`**，⛔ 不得用 `⭕`（U+2B55）—— 否则任何按字面量写的 grep/断言**恒 0 命中**。

## 架构依据

批 1 之后的最后 2 条 `⏭-unmodelled`（`test/cep_invariant_deferrals.json`）：

| id | 缺口 | 触发条件原文 | 代价 |
|---|---|---|---|
| `INV-CEP-13-IC1` | 「『采用该公理为判据』**无机器可检声明项**；输入侧 vocabulary 与 `INV-CEP-12-IC1` **共享** ⇒ 不计专属证据；运行期待 litmus」 | 「`ADR-CEP-13` 决策 5 ① 的剩余转写（组合判据成为结构化声明项）+ **litmus change 立项**」 | 需 **litmus change**（运行期验证一次公理可被证伪）|
| `INV-CEP-13-IC3` | 「adapter 提前 ack 行为成为实例声明项」 | 「触发 = adapter 提前 ack 行为成为声明项；代价 = **schema 13→14** + `INV-CEP-12-IC1` 联动修订 + `ADR-CEP-12` **追加式**修订」 | 需 **schema 版本递增** + 跨 invariant 联动 |

⚠️ 这两条**与批 1 不同型**：批 1 的三条是「**载体面**缺失」（工程可做），批 2 的两条明确要求「**规格面修订**」（schema + ADR 联动）⇒ **审批链不同** ⇒ 拆批是必要的，不是便利。

**与 spec 的关系**：`include/tlm/cep/protocol_instance.schema.json` 是**冻结面成员**（`FROZEN_GLOBS`）⇒ schema 变更 **触五步链**。

## 范围

- **In-Scope**：
  - `INV-CEP-13-IC1` / `INV-CEP-13-IC3` 的载体（`test/` 下的 `.cc` 载体，⛔ 必须带 `// INV-CARRIER:` 刚性标记 + 邻域断言）
  - `include/tlm/cep/protocol_instance.schema.json` —— `properties` **13→14**（新增**可选**属性 `combination_contract`）· `required` **仍 13**（**冻结面成员** ⇒ 五步链）
  - `include/tlm/cep/protocol_instances.json` —— **本仓权威域**实例补 `combination_contract`（⛔ 该文件不在冻结集）
  - `docs/adr/ADR-CEP-12-*.md` —— **追加式**修订（⛔ 原文不改）
  - `docs/adr/ADR-CEP-13-*.md` —— **追加式**修订（`V-13-4` / `V-13-5` 的「机器载体不可得」判读被取代；⛔ 原文不改）
  - `test/cep_invariant_deferrals.json` —— 移除这 2 条（⇒ `⏭-unmodelled` 数 2 → 0；⛔ **文件不得删除**）
  - `test/cep_litmus_corpus.json` —— `_fill_when` 文案修订（⛔ 语料**保持为空**）
  - `scripts/test/cep_invariant_batch2_ledger_check.py`（**新建**）+ `test/CMakeLists.txt` 注册 —— 台账锚独立闸（见 AC-1④）
- **Out-of-Scope**：
  - ⛔ `INV-CEP-13-IC1` 的 **litmus change 本身**（⇒ 独立 change = **I-6a**，已归档）
  - ⛔ `INV-CEP-12-IC1` 的语义改动（**只联动，不改语义**）
  - ⛔ 批 1 的 3 条（⇒ `cep-invariant-modeling-batch1`）
  - ⛔ `B3b-2` 计数 / 档 2 扩面
  - ⛔ **边类语义定义**（`rfe`/`fr`/`ws`）⇒ 归 `.rddf/improvements/cep-memory-model-operational-trace.md`
  - ⛔ **镜像域实例**（`moesi_gpu` / `mesif`，权威在 ArchForge）加 `combination_contract` ⇒ 禁（见 AC-8）
  - ⛔ **新增实例**（不得靠新增第 9 个实例来造正控 ⇒ 会连带红 `test/test_cep_instance_declaration_contract.cc:135` 的 `n == 8u` 与 `:605` 的 `owned.size() == 6u`）

## Why

**不做的代价**:这 2 条是 `66` 条里**最后**的 `⏭-unmodelled` ⇒ 不建模 ⇒ `⏭-unmodelled` 恒 > 0 ⇒ 「66 条全部有判定级载体」永远不成立 ⇒ §6.2 S1-1 无法闭合 ⇒ 平台在 invariant 维度永久停在 61/66。

**为什么现在单独立项**:批 1 落地后，「61 → 64」的收益已经拿到；剩这 2 条的**边际成本远高于边际收益**（要动冻结面 schema + 已签批 ADR + 立 litmus change）⇒ **单独立项 + 单独立审批**。

**本单元真正的交付物 = 处置记录的「可机检性」**, schema 14 化只是它的载体 —— 目标函数是「**不引入任何新的不可判面**」（architect 三次否决「进 `required`」/「另立 sidecar」/「嵌套进既有容器」的理由都是语义面，不是工程量）⇒ 因此每条 AC 都必须能**按正确原因红**，⛔ 「加了断言就算有牙」是本单元最需要防的失败模式。

**为什么不动 `ADR-CEP-12` / `ADR-CEP-13` 原文**:ADR 不可变（只追加 SU）⇒ 形态合规；但**追加什么内容**属架构决策 ⇒ 已由 architect 裁决给定（§裁决 已落）。

## What Changes

> 形态与顺序 = **architect 裁决给定**（`.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §二）；⛔ 实施侧不得自决字段名 / 取值域。

| # | 变更 | 形态 / 约束（**已 ratify**）|
|:--:|---|---|
| 1 | **schema `properties` 13→14** | 新增**可选**顶层属性 `combination_contract`（`bridging.early_ack` + `compound_consistency.{adopted, edge_classes, verification_scope}`）；⛔ **不进 `required`** ⇒ `INV-CEP-12-IC1` 载体零改动 · 镜像域零跨仓 · 4 处 `required.size()==13u` 断言零改动 |
| 2 | **实例声明项补齐** | **本仓权威域** = `repo_owned_instance_ids` ∪ `authority_migrations[].instance_id`（口径同 `scripts/test/cep_decl_alignment_check.sh:257-258`）⇒ 逐个声明 `combination_contract.bridging.early_ack`；值域 3 值：`allow-early-write-ack` / `deny-early-completion` / `unknown`（⛔ 不得编值）|
| 3 | **条件判据** | `compound_consistency.adopted == true` ⇒ `edge_classes` 非空 ∧ `verification_scope` 非空（schema 不写 `if/then` ⇒ 落闸判据）|
| 4 | **载体（咬合）** | `// INV-CARRIER: INV-CEP-13-IC1` / `IC3` 刚性标记 + 邻域 ±8 行含**针对该 id 语义**的断言（⛔ `REQUIRE(true)` / `CHECK(1 == 1)` 之类**凑数**不算咬合 —— `scripts/test/cep_invariant_map_check.py:117-122` 只查**宏名是否出现**）|
| 5 | **deferrals 移除 2 条** | `test/cep_invariant_deferrals.json` 的 `deferrals` 长度 2 → **0**（⛔ 文件**不得删除**：`cep_invariant_map_check.py:126-127` 缺文件即红）+ `python3 scripts/test/cep_invariant_map_check.py --generate` 重生成映射表；⛔ **并集闸硬约束**：`载体 ∪ ⏭ == 66` ∧ **交集为空**（`:152-165`）⇒ 只删不加载 / 只加载不删**都判红**；⚠️ `--generate` **不**跑邻域校验 ⇒ generate 后**必须**立刻 `--check` |
| 6 | **台账锚独立闸** | 新建 `scripts/test/cep_invariant_batch2_ledger_check.py`（照批 1 的 `cep_invariant_batch1_ledger_check.py` 范式）+ `test/CMakeLists.txt` 注册 ⇒ 理由：`cep_invariant_map_check.py` 的台账锚判据（`:145-151`）**只对仍登记的 `⏭` 条目执行** ⇒ 本单元把 2 条删光后**一次都不跑**（批 1 已因此另立闸）|
| 7 | **两份 ADR 追加 SU** | `ADR-CEP-12`（新增声明项 + 口径澄清 + 边类语义**不定义** + 禁派生填语料）· `ADR-CEP-13`（`V-13-4` / `V-13-5` 的「载体不可得」判读被取代；⛔ 判据内容逐字不变）⇒ ⛔ 两份正文**只增行**；⚠️ 追加段落在**文末**，⛔ 不插入既有 `## N. Status Update` 编号链中间（`scripts/test/cep_adr_structural_check.py` 的 SU 正则只认 `^##\s*(Status Update\|状态更新)`）|
| 8 | **litmus 语料保持为空** | `test/cep_litmus_corpus.json` 的 `compound_edges` 仍为 **[]**（既有机器载体：`test/test_cep_litmus_falsification.cc:118` 的 `CHECK(corpus...empty())`）；`_fill_when` 文案改为「**待操作级模型落地**」+「通道图 ≠ 操作图」说明（⛔ 禁止由声明项派生填充 —— 范畴错误 ⇒ 假绿）；⚠️ `litmus_falsifier.hh` 的 `reachable` **只由语料决定** ⇒ 加声明项**不会**翻转闸臂① |
| 9 | **冻结面五步链** | 台账 Entry（**以落库时实测最大值 + 1** 为准，⛔ 不预留编号）→ 改产物 → commit → `--update --authorized-by "Entry #5xx"` → **清单单独 commit** → 无参复验 rc=0 |

## 裁决 已落（**architect 裁决记录 §二 · 用户 ratify R-2a~R-2f · 2026-11-21**）

| # | ratify 项 | **已 ratify 的取值** |
|:--:|---|---|
| R-2a | 顶层属性名 | `combination_contract`（与既有 `combination_capabilities` 平行）|
| R-2b | 子声明项与字段名 | `bridging.early_ack` · `compound_consistency.{adopted, edge_classes, verification_scope}` |
| R-2c | `early_ack` 取值域 | `allow-early-write-ack` · `deny-early-completion` · `unknown`（**3** 值；前两项与 `ADR-CEP-13` 决策 4 表两行「允许提前写 ack」/「不允许提前完成」**语义逐字对应**；⛔ 已删 `not-applicable` —— 无适用 predicate ⇒ 不可判的声明项）|
| R-2d | `edge_classes` 取值域 | `rfe` · `fr` · `ws`（⛔ **只登记取值域**，本仓不定义语义）|
| R-2e | 必填策略 | 顶层**可选**（`minProperties >= 1`）；子对象声明则其必填字段必填；`adopted == true` ⇒ `edge_classes` 非空 ∧ `verification_scope` 非空 |
| R-2f | 消费方向 | 桥 A↔B 的并发粒度读 **B**（对端）实例的 `early_ack` |

⚠️ **⛔ 硬约束（P-1）**：第 14 个属性**不得**加入 `required` 数组 —— 否则 IC1 语义变更 + 4 处断言 + ADR 取代范围 + 镜像域跨仓 PR ⇒ 须**重开裁决**。

## Acceptance

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。

- [ ] **AC-1（处置落地 · 四小项）**：① `test/cep_invariant_deferrals.json` **文件仍存在** ∧ `deferrals == []` ② `python3 scripts/test/cep_invariant_map_check.py --check` rc=0 ∧ 输出含「**66 项有牙载体**」∧ `⏭-unmodelled` 计数 == **0** ∧ `⏭-pending` 计数 == **0**（B2 达成宣称要求 pending 亦为 0）③ `--generate` 后 `--check` 绿 ∧ `git diff --name-only` **含** `test/cep_invariant_assertion_map.json` ④ **台账锚独立闸绿**：`bash scripts/test/cep_invariant_batch2_ledger_check.py` rc=0（判据 = **同一 Entry 段**内含 `INV-CEP-13-IC1` 与 `INV-CEP-13-IC3` **且**含本单元的载体文件路径 ∧ 该段不含「已建模 / 可判」类禁述；⚠️ **反 fixture**：把 Entry 段内任一 id 改掉 ⇒ **按签名红** ⇒ 还原绿；⚠️ **不得**用「台账全文含该 id」作判据 —— 台账现存 **15 处**含这两个 id ⇒ 全文判据**恒绿**）
- [ ] **AC-2（66/66 全有**载体** —— ⛔ 不是「终态达成」）**：`len(test/cep_invariant_assertion_map.json` 的 `carriers`) == **66**（⚠️ 按 **dict 键数**计，⛔ 不是 `"file"` 条目数 —— 当前后者为 68）；⚠️ `INV-CEP-04-IC4` **不在**其中（永久跳空 ⇒ 66 条已定义 + 1 跳空 = 67 编号位）。⛔ **禁止用负断言关闭 66/66** ⇒ **「66/66」只能读作「66 条全部有载体」，⛔ 不得读作「66 条全部可判 / 全部达成」**；⚠️ 跨单元约束：`obj §0 DoD 7` 现写「66 条守门 invariant **机器检查 100% 覆盖**」⇒ 该措辞在 I-6 落地后会**看起来被验证为真** ⇒ **Q-2（DoD 7 / B2 口径修复面判定）必须先于本单元完成**，⛔ 否则制造 S0
- [ ] **AC-3（schema 形态变更的机器面）**：`properties` 由 13 增至 **14**（新增 `combination_contract`）· `required` **仍 13**（⛔ 第 14 项**不进** `required`）· `scripts/test/cep_decl_alignment_check.sh` + `test/test_cep_schema.cc` 全绿；⚠️ **反 fixture 两条**：① 把第 14 个属性加进 `required` ⇒ **按签名红**（落点 = `test/test_cep_instance_declaration_contract.cc:104-105` 的 `required.size()==13u` + `:114` 的 `from_schema == from_groups`）② 把 `properties` 改回 13 ⇒ **按签名红**（`test/test_cep_schema.cc:423`）⇒ 还原绿；⚠️ ⛔ **禁改** `test/test_cep_instance_declaration_contract.cc:105` / `:114` 与 `test/test_cep_schema.cc:257` 三处（否则反 fixture ① 失效 ⇒ 反 fixture 的落点不得依赖「另一条 AC 的自我约束」）
- [ ] **AC-4（派生断言改形 · 4 个断言点 / 2 个用例标题）**：`test/test_cep_schema.cc` ① `:234` 用例（标题改「必含 13 个顶层属性 + 1 个可选声明项」· `:252` 与 `:257` **拆成两个列表**）② `:418` 用例（标题改「7 组 / 13 个必含属性 + 1 个可选属性」· `:423` ⇒ 14）③ `:447` **改写为三条**（`REQUIRE(union_ids == set(required))` ∧ `REQUIRE(props.size() - union_ids.size() == 1)` ∧ `REQUIRE(props.contains("combination_contract"))`）—— ⚠️ 单改成「分组并集 == `required` 集合」是**净损失**（原 `union_ids.size() == props.size()` 还证明了「7 组覆盖 schema 全部属性」，弱化后新属性漏登记将不再被发现）④ `:401`（`combination_capabilities` 恰 3 维）· `:454`（ADR grep）· `:455`（ADR 全文**不得**含「6 个字段组」串）**零改动 / 保持绿**（⇒ 追加 SU 落盘后须**实跑复验** `:455`）；⚠️ 4 处 per-family `required.size()==13u` 与 `test/test_cep_instance_declaration_contract.cc:135` 的 `n == 8u` **零改动**
- [ ] **AC-5（冻结面五步链 ×2 + manifest 核对）**：① `cep_freeze_check.sh` 的 `artifact_count` **逐字节不变**（`protocol_instance.schema.json` 已是冻结成员）② `--update --authorized-by "Entry #5xx"` **执行一次** ③ 清单**单独 commit** ④ 无参复验 rc=0 ⑤ `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json` 的 diff **只改 3 个 hash**（`protocol_instance.schema.json` + 两份 `ADR-CEP-1{2,3}`）⇒ 防 `--update` 顺手吸收无关漂移 ⑥ 台账 Entry 编号**唯一性** grep 计数 == 1；⚠️ **台账与清单不得合并提交**
- [ ] **AC-6（ADR 追加式 · 原文零改动）**：`git diff` 对 `docs/adr/ADR-CEP-12-*.md` **与** `docs/adr/ADR-CEP-13-*.md` 的改动**只增行**（既有行 diff 行数 == **0**）；⚠️ 反 fixture：改任一既有行 ⇒ 判红；⚠️ `ADR-CEP-12:158` 的口径修订括注「(= schema 实际属性数)」属**正文**（⛔ 不可改）⇒ 只能由 SU 记为「被取代表述」（否则与 schema 14 化**字面相撞**且被 `:454` 的正向 grep 掩盖）；⚠️ 追加段**文末**（见 What Changes #7）
- [ ] **AC-7（`INV-CEP-12-IC1` 只联动不改语义）**：该 invariant 的**载体** `test/test_cep_instance_declaration_contract.cc:98-115` 的 `git diff` 行数 == **0**（`required` 仍 13 ⇒ 键集合逐名一致仍真）· `docs/adr/ADR-CEP-12-*.md:155` 的 IC1 行 `git diff` 行数 == **0**；⚠️ ⛔ **不适用**「`reason` / `trigger_condition` / `ledger_entry` 逐字未变」—— `INV-CEP-12-IC1` 是**载体型** invariant，`test/cep_invariant_deferrals.json` 里**从来没有**它的这三字段（那是批 1 的 `⏭` 登记型字段，抄过来即为空条）
- [ ] **AC-8（范围纪律 · 零越权 + 镜像禁改）**：`git diff --name-only` **不含** `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `docs/architecture/**`；⚠️ **禁给镜像域实例**（`moesi_gpu` / `mesif`）或 `test/cep_instance_golden.json` 加 `combination_contract`（⇒ 破 golden 方向①「键不在 13 键表内」`scripts/test/cep_instance_golden_check.py:80-83` + 镜像闸 ⑥ `:551`）⇒ **golden 零改动**（其 `_scope = [moesi_gpu, mesif]` 与本仓权威域**不相交**）；⚠️ ⛔ 连 `test/cep_instance_golden.json` 的 `_note` / `_scope_rationale` **注释也不得改**（该注释与仓内权威分区口径不符 ⇒ **另立 ticket**，⛔ 不在本单元顺手修）；⚠️ `include/tlm/cep/protocol_instance.schema.json`（冻结面）**允许**改但须走 AC-5 五步链
- [ ] **AC-9（`early_ack` 逐实例声明 + `unknown` 计数机器化）**：**本仓权威域**（`repo_owned_instance_ids` ∪ `authority_migrations[].instance_id`）的每个实例都声明 `combination_contract.bridging.early_ack`（值可为 `unknown`）⇒ 判据 = **专用 expected fixture**（新增 `test/cep_combination_contract_expected.json`，形如 `schema: cep-combination-contract-expected/v1`）· 域**双向集合相等**（`owned == fixture ids`，防「新增实例无覆盖且绿」）· 逐实例期望值 · `unknown` **精确计数**；⚠️ **反 fixture 三条**：① 删某实例的 `bridging.early_ack` ⇒ 红 ② 改 fixture 某实例期望值 ⇒ 红 ③ 域侧加一个假 id ⇒ 双向集合相等红；⚠️ `unknown_count` 字段是**变更检测器**，⛔ **不是**「未编值」的证据（同 `test/cep_flow_support_expected.json` 的自陈口径）⇒ 若 6 个实例全为 `unknown`，⛔ **不得**在报告/台账里表述为「声明面已有信息量」
- [ ] **AC-10（条件判据 + 反 fixture · 防空跑）**：`adopted == true` ⇒ `edge_classes` 非空 ∧ `verification_scope` 非空 ⇒ 判据 = **合成实例**（不需入库）的正反三例：`valid({adopted:true, edge_classes:[]})` 假 · `valid({adopted:true, verification_scope:""})` 假 · `valid({adopted:false})` 真（`false` 允许空）；⚠️ 现状**无任何实例必须声明 `compound_consistency`**（What Changes #2 只强制 `bridging.early_ack`）⇒ ⛔ 不写这条反 fixture，该分支**永远空跑**（恒绿）
- [ ] **AC-11（咬合 · 每条载体断言**针对该 id 语义**）**：`INV-CEP-13-IC3` 的载体须断言三件：schema 的 `early_ack` 枚举**恰 3 值且不含 `not-applicable`** · `bridging.required == ["early_ack"]` · 本仓权威域实例逐个含 `combination_contract.bridging.early_ack`；`INV-CEP-13-IC1` 的载体须断言三件：`adopted==true ⇒ edge_classes 非空 ∧ verification_scope 非空` · `edge_classes ⊆ {rfe, fr, ws}` · `compound_consistency.required == ["adopted"]`；⛔ **不接受**只有 `INV-CARRIER` 标记 + 邻域宏名的形态（`scripts/test/cep_invariant_map_check.py:117-122` 只查**宏名是否出现** ⇒ `REQUIRE(true)` 也能过）；⚠️ 载体必须是 `.cc`（`test/` 或 `scripts/test/` 下 —— `scan_carriers` 只扫这两处），⛔ **不放** `include/`
- [ ] **AC-12（litmus 语料保持为空）**：`test/cep_litmus_corpus.json` 的 `compound_edges` 仍为 **[]**（既有机器载体：`test/test_cep_litmus_falsification.cc:118`）· `bash scripts/test/cep_litmus_falsification_check.sh` 两臂 rc=0；⛔ **不得**由 `combination_contract` 派生填充（⇒ 范畴错误 ⇒ 假绿）；⚠️ `litmus_falsifier.hh` 的 `reachable` **只由语料决定** ⇒ 加声明项**不会**翻转闸臂①（⛔ 不得把「加声明项」写成「公理已被检验」）
- [ ] **AC-13（诚实边界 · 禁夸大）**：⛔ 只 grep **机器产物**（判定产物 `report.json` / 新载体源码）与台账 Entry 的**断言句**；⛔ **不 grep** ADR SU / improvement 自身（禁述串必现 ⇒ 自指红）；禁述集合 = 「公理已成立」/「已被检验」/「已证明」；载体 = `test/test_cep_tools_contract.cc`（同款禁述判据的既有载体，per `ADR-CEP-13` V-13-3）；⚠️ **声明 ≠ 判定** —— 声明面成立 **不等于** 复合一致性公理已被检验（其判定载体归 `cep-memory-model-operational-trace`）
- [ ] **AC-14（0 回归 · 禁 SKIP 冒充绿）**：ctest 全绿（⛔ **不写死数字**）· 全部 `scripts/test/cep_*.sh`（**数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准**）逐个 rc=0 **且输出不含 `[SKIP]`**（⚠️ `scripts/test/cep_litmus_falsification_check.sh` 在缺 `build/bin/cpptlm_tests` 时打印 `[SKIP]` 并 `exit 0` ⇒ 「逐个 rc=0」在未构建时是 0 判据 ⇒ **必须先确认 `$BUILD_DIR/bin/cpptlm_tests` 存在**）· `bash scripts/test/cep_adr_structural_check.py` rc=0 · `bash scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| （无新增 invariant） | — | ⛔ **新增 invariant 数 = 0** —— 本单元只把**已定义**的 2 条变成有判定级载体；⚠️ schema 变更属契约面 ⇒ 须在 delta spec 中体现 |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | **≥1 个** `.cc` 载体（允许两个 `INV-CARRIER` 标记同文件 —— `scan_carriers` 支持；⚠️ ⛔ 不得强制拆成 2 个文件 ⇒ 第二份重复 fixture）· `test/cep_combination_contract_expected.json`（`early_ack` 期望 fixture）· `scripts/test/cep_invariant_batch2_ledger_check.py`（台账锚闸）· 台账 Entry（追加）|
| **修改文件** | `include/tlm/cep/protocol_instance.schema.json`（**冻结面** · `properties` 13→14）· `include/tlm/cep/protocol_instances.json`（**本仓权威域**实例）· `docs/adr/ADR-CEP-12-*.md`（**只增行**）· `docs/adr/ADR-CEP-13-*.md`（**只增行**）· `test/cep_invariant_deferrals.json`（**只删 2 条**）· `test/cep_invariant_assertion_map.json`（重生成）· `test/cep_litmus_corpus.json`（`_fill_when` 文案）· `test/test_cep_schema.cc`（派生断言改形）· `test/CMakeLists.txt`（注册新闸）|
| **零改动（硬查）** | `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `docs/architecture/**` · `test/cep_instance_golden.json`（**含其 `_note` / `_scope_rationale` 注释**）· **镜像域实例**（`moesi_gpu` / `mesif`）· `test/test_cep_instance_declaration_contract.cc:105/:114/:135` · 4 处 per-family `required.size()==13u` |
| **冻结面** | `include/tlm/cep/protocol_instance.schema.json` + 两份 ADR 已在 `FROZEN_GLOBS` ⇒ 改动 ⇒ **一次** `--update --authorized-by` ⇒ 冻结产物数**不变** ⇒ 清单**单独 commit** |
| **闸** | `cep_invariant_map_check.sh` 判据 ③ 口径不变（载体 ∪ ⏭ == 66）· `⏭-unmodelled` 2 → 0 · **新闸** `cep_invariant_batch2_ledger_check.py`（ctest 注册）· `cep_decl_alignment_check.sh` 自动覆盖新增可选属性（jsonschema 全量校验）· `test_cep_schema.cc` 派生断言改形 |
| **KPI** | `0/3 → 1/3` ⇒ 归档 ⇒ `0/3` |
| **风险** | ① 步骤 0 机械件漏做 ⇒ 「⏭ 由 2 降为 0」不成立 ② 台账锚判据用「全文含 id」⇒ **恒绿**（台账现存 15 处）③ 第 14 属性误入 `required` ⇒ IC1 语义变更 ④ 追加 SU 误改正文 / 插入编号链中间 ⇒ ADR 不可变或结构闸红 ⑤ `early_ack` 编值 ⇒ 假声明面 ⑥ 派生填充 litmus 语料 ⇒ 假绿 ⑦ 载体用 `REQUIRE(true)` 凑数 ⇒ 66/66 恒绿 ⑧ `[SKIP]` 冒充绿 ⇒ 「逐个 rc=0」无判别力 |

## 审查轨迹（planner 起草 · 2026-11-20 · 2026-11-21 修订）

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|:--:|---|
| 1 | metis | 指出 G-04 范围错（3 条 vs 实测 5 条）+ 后 2 条属「跨 change 规格修订」 | ⇒ 本单元 = 批 2，**范围重估为规格修订面** |
| 2 | oracle | **REJECT** · 修法 1 | ⇒ v3 显式拆分批次构成（批 1 / 批 2）|
| 3 | critic（v2） | **REJECT** · 3 BLOCKER | ⇒ 本单元不碰 B2 行、不碰 KPI 面 |
| 4 | critic（v3） | **REJECT** · BLOCKER-1 | ⇒ 本单元不含 KPI 活口径取值 |
| 5 | critic（v4） | **APPROVE_WITH_CONDITIONS** | 不涉及本单元 |
| 6 | **architect**（裁决记录 §二）| **形态已裁** · oracle **APPROVE** + critic **APPROVE_WITH_CONDITIONS**（无 BLOCKER）| ⇒ 形态/取值域/必填策略/顺序/联动边界/禁止项全部给定；oracle 抓出 v1 的 3 处事实错误（步骤 0 机械件 / golden「重生成」/ 漏 `ADR-CEP-13` SU）；critic 补 5 个断言点 + `unknown` 计数改 fixture 形态 + 「逐字取自」→「语义逐字对应」|
| 7 | **user** | **ratify R-2a ~ R-2f**（2026-11-21）| ⇒ 契约面取值已定 ⇒ **builder 可开工**（⛔ 前置 = 立项 openspec change）|
| 8 | **metis**（重写轮）| **P0 × 6 + P1 × 8 + P2 × 3** | ✅ **全部处置**：P0-1 台账锚改**独立闸**（机制错位：判据 ④ 只对仍登记的 ⏭ 执行）· P0-2 反 fixture 落点改 `:105/:114/:257` 三处并禁改 · P0-3 补**咬合 AC**（防 `REQUIRE(true)` 凑数）· P0-4 补**条件判据反 fixture**（否则该分支永远空跑）· P0-5 码位统一 `⏭`(U+23ED) · P0-6 `:447` 改写为**三条**（原改法是净损失）· P1-7 AC-1 去噪 + 合并 · P1-8 AC-9 域口径/反 fixture/`unknown_count` 口径 · P1-9 AC-10 改引既有载体（`test_cep_litmus_falsification.cc:118`）· P1-10 AC-11 限定只 grep 机器产物（禁述串会自吞）· P1-11 AC-7 去掉批 1 的空字段 · P1-12 AC-5 补 manifest 3 键 + Entry 唯一性 · P1-13 AC-12 补 `[SKIP]` 禁令 + `BUILD_DIR` · P1-14 Impact 改「≥1 个 `.cc`」· P2-15/16/17 计数写法 + golden 注释禁改 + SU 文末追加 + 「处置记录可机检性」写入 Why |
