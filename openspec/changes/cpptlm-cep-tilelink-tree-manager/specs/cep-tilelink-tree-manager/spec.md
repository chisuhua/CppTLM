# cep-tilelink-tree-manager Specification

## Purpose
定义 **TileLink-C**(TileLink Tree Manager · SiFive `tilelink-c-1.7.1`)在 CEP 平台上的
**实现层可执行契约**:`TileLinkCAdapter`(零自有表副本)· `TreeManager` 行为实现
(**`LookupResult::ESCALATE_UP` + `LookupHazard::CHAIN` 的首次真实使用**)·
`tilelink_c` 实例声明 · `AC-05` 判据的**族无关化**。

**口径已裁决**(台账 `obj-cep-platform.md` **Entry #134** / **#135** / **#138** / **#141** / **#143**
+ `ADR-CEP-02` / `ADR-CEP-04` / `ADR-CEP-08` / `ADR-CEP-09` / `ADR-CEP-11` / `ADR-CEP-12` / `ADR-CEP-17`
的 `## Status Update`)⇒ 本能力**不重推**语义,只把裁决**机器化**。

> **边界**:不改 `perm` 4 值锁 · 不改 `protocol_instance.schema.json`(★ 冻结面 · **零改动**)
> · 不改 6 类消息类词表 · 不改 `CATEGORY_*` 3 类 · 不新增 ABI · 不新增 invariant 编号数
> · **不产出任何性能结论** · **不**做 protocol engine · **不**扩 `LocalView`
> (⇒ 复用 `forward_to`,语义代价已登记)。
>
> **合规边界**:TileLink 是 **SiFive 明示的 free and open standard** ⇒ **规范正文可读**
> ⇒ basis 直接来自规范;⚠️ 规范 © SiFive(**非** CC-BY)⇒ 只记**条款级定位 + 断言摘要**。

---

## ADDED Requirements

### Requirement: cep-tilelink-tree-manager

CEP SHALL 为 **TileLink-C**(`tilelink-c-1.7.1`)提供一份**可执行实现层契约**:
`TileLinkCAdapter`(零自有表副本)· `TreeManager` 行为实现(第 4 种行为类)·
`tilelink_c` 实例声明(13 键)· `AC-05` 族无关化。

#### Scenario: TMA-01 —— `TileLinkCAdapter` SHALL **零自有表副本**

- **WHEN** 核对 `TileLinkCAdapter` 的形态
- **THEN** 子条件 ① SHALL 成立:其 `transition()` SHALL **委托** `TileLinkStateSpec::transition`
- **AND** 子条件 ② SHALL 成立:其 `perm_of()` SHALL **委托** `TileLinkStateSpec::perm_of`
- **AND** 子条件 ③ SHALL 成立:其**去注释后**文本 SHALL **不**含 `State::` / `Event::` / `Perm::`
  **任何字面量**,SHALL **不**含任何**表名**(`kTransitions` / `kPermProjection` / `kTable`)
  ⇒ 本类在语法上**不可能**携带自己的表
- **AND** 子条件 ④ SHALL 成立:**反向 fixture**:植入任一字面量或表名 ⇒ **必须因正确原因红**

#### Scenario: TMA-02 —— `perm` 投影 SHALL 为 **3→3** 且 `UNIQUE` **不出现**

- **WHEN** 核对 `TileLinkCAdapter` 的 `perm` 投影
- **THEN** 子条件 ① SHALL 成立:投影 SHALL 为 **3 行**且逐行为
  `Nothing→NONE` / `Branch→READ` / `Tip→WRITEBACK`
- **AND** 子条件 ② SHALL 成立:`UNIQUE` SHALL **NOT** 出现
  (规范只说副本 *may contain dirty data* ⇒ 不承诺干净 ⇒ `UNIQUE` 不可确证 ⇒ 保守投影 `WRITEBACK`)
- **AND** 子条件 ③ SHALL 成立:该投影 SHALL **只消费**规约载体,SHALL **不**另写 switch
- **AND** 子条件 ④ SHALL 成立:`perm_of` SHALL **同时**提供**返回值**形态与**出参**形态
  (出参形态 = 平台 adapter 装载契约的调用约定,per `adapter_contracts_loader.hh`)
  ⇒ 二者 SHALL **同源**(同一张投影表)⇒ **零双真源**

#### Scenario: TMA-03 —— 消息类映射 SHALL **分两张方向不同的表**且逐条等于 **R-T3 归属表**

- **WHEN** 核对 `TileLinkCAdapter` 的消息类映射
- **THEN** 子条件 ① SHALL 成立:**6→3 表**(`kMessageClassMap`:消息类 → `CATEGORY_*`)SHALL 恰为 **4 行**
  (`request`→REQUEST · `snoop`→REQUEST · `snoop_resp`→RESPONSE · `response`→RESPONSE)
  ⇒ TileLink-C **不使用** `atomic` / `query` ⇒ 相对平台方向默认表**非恒等**
- **AND** 子条件 ② SHALL 成立:`kMappingIsIdentity` SHALL 为 **`false`** 且**理由必填**
  (理由 SHALL 指明「非恒等的成因是**词表子集**,**不是**方向不同」)
- **AND** 子条件 ③ SHALL 成立:**逐消息表**(`kProtocolMessageMap`:逐消息 → 6 类)SHALL 恰为 **10 行**
  且逐条等于 **R-T3 归属表**
  (`Acquire`→`request` · `Probe`→`snoop` ·
  `ProbeAck`/`ProbeAckData`/`Release`/`ReleaseData`→`snoop_resp` ·
  `Grant`/`GrantData`/`ReleaseAck`/`GrantAck`→`response`)
- **AND** 子条件 ④ SHALL 成立:逐消息表的归属目标 SHALL ∈ **6 类规范词表**
  `{request, response, snoop, snoop_resp, atomic, query}`
- **AND** 子条件 ⑤ SHALL 成立:⚠️ **`writeback` SHALL NOT** 出现在**任一**表的键或目标中
  (它是 **canonical 10 流名**,不是消息类名)
- **AND** 子条件 ⑥ SHALL 成立:**2 处降维 SHALL 显式登记**:
  `Release`/`ReleaseData` 归 `snoop_resp`(语义是「自愿放弃」)·
  `GrantAck` 归 `response`(词表无「对 completion 的 ack」档)
- **AND** 子条件 ⑦ SHALL 成立:⚠️ **两张表 SHALL 分别断言** —— 合并断言则「平台机制面」与
  「协议族特有面」的混淆**不可分辨**

#### Scenario: TMA-04 —— `TreeManager` 的 caps/behavior SHALL **逐字段**等于 `kTrackerTraits[3]`

- **WHEN** 核对 `TreeManager` 的能力声明
- **THEN** 子条件 ① SHALL 成立:caps SHALL 逐字段为
  `Scope::SUBTREE` · `Completeness::EXACT` · `Authority::LOCAL` · `AggregationAt::TREE_PARENT`
- **AND** 子条件 ② SHALL 成立:`hazard_under_contention` SHALL 为 **`LookupHazard::CHAIN`**
- **AND** 子条件 ③ SHALL 成立:`broadcast_fallback` SHALL 为 **`false`**
- **AND** 子条件 ④ SHALL 成立:该声明 SHALL **从 `kTrackerTraits[3]` 取值**(不另写字面量)

#### Scenario: TMA-05 —— `ESCALATE_UP` 与 `CHAIN` SHALL **各有真实使用**

- **WHEN** 核对 `TreeManager` 的运行期语义
- **THEN** 子条件 ① SHALL 成立:节点为 **Tip**(持 Read+Write)时 `lookup` SHALL 返
  `LookupResult::ANSWER_HERE`
- **AND** 子条件 ② SHALL 成立:节点为 **Branch** 或 **Nothing** 时 `lookup` SHALL 返
  `LookupResult::ESCALATE_UP`,且 `view.forward_to` SHALL 为**树父节点**
- **AND** 子条件 ③ SHALL 成立:`TreeManager` SHALL **永不**返 `LookupResult::BROADCAST`
  (树形有唯一 Tip ⇒ 不存在广播场景)
- **AND** 子条件 ④ SHALL 成立:该地址有 in-flight snoop 时 `lookup` SHALL 返
  `LookupHazard::CHAIN`(加入同一 in-flight snoop)⇒ **不是** `STALL`
- **AND** 子条件 ⑤ SHALL 成立:**反向 fixture**:把 `ESCALATE_UP` 换成 `ANSWER_HERE`
  或把 `CHAIN` 换成 `STALL` ⇒ **必须因正确原因红**

#### Scenario: TMA-06 —— `CHAIN` 的**加入**机械 SHALL 与 `snoop_fold` 的归零逻辑**同源**

- **WHEN** 核对 `SnoopMechanics::snoop_join_core`
- **THEN** 子条件 ① SHALL 成立:该地址**无** in-flight 时 SHALL 返 **0**(无可加入)
- **AND** 子条件 ② SHALL 成立:加入的 responder 集含**未映射位**时 SHALL 返 **0**(同一拒绝语义)
- **AND** 子条件 ③ SHALL 成立:加入集与既有期望集**无新增位**时 SHALL 返**既有 handle**(幂等)
- **AND** 子条件 ④ SHALL 成立:否则 SHALL 把新增位并入 `expected` **与** `remaining`
  ⇒ `snoop_fold` 的归零逻辑 SHALL **不变**(⇒ **零重复实现**)
- **AND** 子条件 ⑤ SHALL 成立:**反向 fixture**:不并入 `remaining` ⇒ 归零判定**必须红**
- **AND** 子条件 ⑥ SHALL 成立:该机械 SHALL 位于 **`SnoopMechanics`**(公共机械)⇒ **禁双实现**;
  SHALL **不**在 `TreeManager` 内自持(否则与 `snoop_fold_core` **双份**归零逻辑)

#### Scenario: TMA-07 —— `LocalView` SHALL **零改动**(复用 `forward_to`)

- **WHEN** 核对 `ESCALATE_UP` 的目标承载
- **THEN** 子条件 ① SHALL 成立:`LocalView` SHALL **不**新增字段
  (⇒ **不**触及 `cep-sharer-tracker-base` 的 `LocalView` 契约)
- **AND** 子条件 ② SHALL 成立:「向上递归的目标」SHALL 由 **`forward_to`** 承载
- **AND** 子条件 ③ SHALL 成立:⚠️ **`forward_to` 的双重语义 SHALL 显式登记** ——
  `FORWARD_TO` 时 = 取数目标;`ESCALATE_UP` 时 = **树父节点** ⇒ 调用方 SHALL **先读 `result`** 再解释该字段
- **AND** 子条件 ④ SHALL 成立:`forward_to` SHALL 为**非** `kInvalidNode`(该场景下必有父节点)

#### Scenario: TMA-08 —— `tilelink_c` 实例 SHALL 满足 **13 键** schema 且 `perm` **3 值**

- **WHEN** 核对 `protocol_instances.json` 的 `tilelink_c` 实例
- **THEN** 子条件 ① SHALL 成立:schema 的 **13 个 required 键** SHALL 齐备
- **AND** 子条件 ② SHALL 成立:`perm` SHALL 恰为 **`["NONE","READ","WRITEBACK"]`**(3 值)
  ⇒ 依据:**R-T4 裁决** = 值域锁(锁定**允许出现的字面值集合**,不要求每实例用满 4 值)
  ⇒ ⚠️ `protocol_instance.schema.json` SHALL **零改动**
  (`items.enum` 是**值级**枚举 + `minItems:1` ⇒ 3 值合法)
- **AND** 子条件 ③ SHALL 成立:⚠️ **SHALL NOT** 为「看起来完整」而填 4 值 ——
  填 `UNIQUE` ⇒ `exposes_unique_perm = true` ⇒ `INV-CEP-08-IC2` 走 hierarchical 分支
  ⇒ **PASS 但 detail 语义为假**(TileLink 无 `E` 态)⇒ **语义为假的绿灯**
- **AND** 子条件 ④ SHALL 成立:`vc_binding` SHALL 逐类等于 **R-T2**
  (`request`→0 · `snoop`→1 · `snoop_resp`→2 · `response`→3)
- **AND** 子条件 ⑤ SHALL 成立:⚠️ `vc_binding` SHALL **NOT** 出现 `writeback`
- **AND** 子条件 ⑥ SHALL 成立:`sharer_tracker.impl` SHALL 为 **`TreeManager`** 且 caps 4 属性
  SHALL 逐字段等于 `kTrackerTraits[3]`
- **AND** 子条件 ⑦ SHALL 成立:`tilelink_c` SHALL 登记进 **`repo_owned_instance_ids`**
  ⇒ ⑥ 镜像比对**跳过**、①~⑤ 校验**覆盖**

#### Scenario: TMA-09 —— `AC-05` SHALL **族无关化**(参数化 + SKIP)

- **WHEN** 核对 `check_adapter_self_consistency` 的判定
- **THEN** 子条件 ① SHALL 成立:「唯一干净态 + 写事件 → 目标态」三元组 SHALL **参数化**
  ⇒ SHALL **NOT** 硬编码 `3`
- **AND** 子条件 ② SHALL 成立:该协议族**无** unique-clean 态时 SHALL 判 **SKIP**
  ⇒ **NOT** FAIL ⇒ 理由 = 「该族不暴露干净独占态 ⇒ `E + Write` 语义不适用」
- **AND** 子条件 ③ SHALL 成立:秩单调判据 SHALL 保留(按传入秩比较)
- **AND** 子条件 ④ SHALL 成立:⚠️ **既有 4 个协议族的 verdict SHALL 逐条不变**(回归锁)
- **AND** 子条件 ⑤ SHALL 成立:⚠️ **SHALL NOT** 通过填假的 `e_write_target_state` 使 TileLink 变绿

#### Scenario: TMA-10 —— `no_upgrade_instances` 派生集 SHALL **恰含** `tilelink_c`

- **WHEN** 核对静默升级派生集
- **THEN** 子条件 ① SHALL 成立:`tilelink_c` SHALL **不**在 `perm_upgrades.json` 中声明静默升级
  (无 `UNIQUE` ⇒ 无静默升级 ⇒ 加了即**假声明**)
- **AND** 子条件 ② SHALL 成立:派生集 SHALL **恰含** `tilelink_c` ⇒ **首次非空**
  ⇒ ⚠️ 既有断言 `no_upgrade_instances.empty()` SHALL 改为「恰含 `tilelink_c`」
  (⇒ 派生量**从恒空转为有判别力** = **判据变强**)
- **AND** 子条件 ③ SHALL 成立:`perm_upgrades.json` 的 `current_state` 自述 SHALL 同步
  (「4 个实例全部具备该能力 ⇒ 派生量恒为空」已**失实**)

#### Scenario: TMA-11 —— **2 件冻结面**同步 SHALL 经**两步授权**

- **WHEN** 核对实现落地后的冻结面状态
- **THEN** 子条件 ① SHALL 成立:`openspec/specs/cep-sharer-tracker-base/spec.md` 的
  「其余 **3** 种实现的行为实现尚未落地」SHALL 更新为「其余 **2** 种」+ 登记 `TreeManager` 落位
- **AND** 子条件 ② SHALL 成立:`docs/architecture/07-coherence-exploration-platform.md` §11 的
  SharerTracker **6** 实现计数 SHALL 由 `3 / 6` 更新为 `4 / 6`
- **AND** 子条件 ③ SHALL 成立:二者 SHALL 经**两步授权**(台账 Entry → `--update --authorized-by`
  → commit → `--update` → commit)⇒ provenance 自检 SHALL 通过
- **AND** 子条件 ④ SHALL 成立:⚠️ `protocol_instance.schema.json`(★ 冻结)SHALL **零改动**

#### Scenario: TMA-12 —— 判据 SHALL 遵守 4 铁律

- **WHEN** 核对本能力的判据设计
- **THEN** 子条件 ① SHALL 成立:纯负向断言 SHALL 有**正控**
- **AND** 子条件 ② SHALL 成立:每条判据 SHALL 有**可执行**反向 fixture 且**因正确原因红**
- **AND** 子条件 ③ SHALL 成立:编译期锁 SHALL **不**只以「编译红」充当 ctest 判据
- **AND** 子条件 ④ SHALL 成立:fixture 状态变迁后 SHALL **重新基线化**
- **AND** 子条件 ⑤ SHALL 成立:既有计数断言的连带更新 SHALL **逐条登记**(不得静默改)
