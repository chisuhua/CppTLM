# cep-hierarchical-directory Specification

## Purpose
定义 **第 6 种 SharerTracker 实现 `HierarchicalDirectory`**(`ADR-CEP-04 §13` 的 R-1 裁决落地义务)
的**可执行契约**:**行为面** + **caps / behavior 描述符三方一致** + **多级聚合差异化行为** +
**行为决策的 `basis` 式登记与跨仓追认债**。

`mesif` 实例声明的 `sharer_tracker.impl = "HierarchicalDirectory"` 指向本实现 ⇒ **本实现不落地,
`mesif` 的装配期校验即不通过** ⇒ 本能力是队列 #9 的**硬依赖**。

> **边界**:不改 `openspec/specs/cep-sharer-tracker-base/spec.md`(M1 冻结面;base 契约
> —— `SharerTrackerIf` 单接口 + caps 4 属性 + D-5 全量契约 + `FullMapDirectory` 行为实现
> —— **零改动**)· 不扩 caps 为 5 维(per `ADR-CEP-04 §13` 决策)· 不改 `protocol_instance.schema.json` ·
> **不**用 `TreeManager` 降级近似(per `ADR-CEP-04 §13` **否决**方案 D)· 新增 invariant 编号数 = 0。
>
> **⚠️ 本实现的「行为规约」在 ArchForge 与本仓均不存在** —— 只有 caps 声明 + 一句注释级描述
> ⇒ 本能力 SHALL 承担**行为决策登记 + 跨仓追认债**的义务(见 HD-04),SHALL **不**把自推语义写成
> 「已裁决」。

## Requirements

### Requirement: cep-hierarchical-directory

CEP SHALL 提供 `HierarchicalDirectory` 的**行为实现**(第 6 种 SharerTracker):分级目录 + 多级聚合 +
`EXACT` sharer 完备性,并 SHALL 把**无真源的行为决策**逐条 `basis` 式登记 + 挂跨仓追认债。

> **为什么另立 capability**:base 契约 `cep-sharer-tracker-base` 交付的是「接口 + caps + `FullMapDirectory`
> 行为实现」;第 6 种实现有**独立的**行为语义、独立的决策登记面、独立的追认债 ⇒ 另立更清晰,
> 且**不触碰**冻结面里的 base 契约。

#### Scenario: HD-01 —— `HierarchicalDirectory` 的 caps + behavior 描述符 SHALL 与实例声明**三方相等**

- **WHEN** 核对 `HierarchicalDirectory` 的能力描述符
- **THEN** 子条件 ① SHALL 成立:caps 4 属性 SHALL = `SYSTEM` / `EXACT` / `GLOBAL` / **`HIERARCHICAL`**
  —— 且 SHALL 与 `protocol_instances.json` 的 `mesif.sharer_tracker.caps` **逐字段相等**
  (三方:实例声明 ↔ `sharer_tracker.hh` 的 `kTrackerTraits[5]` ↔ class 的 caps 成员)
- **AND** 子条件 ② SHALL 成立:behavior 2 属性 SHALL = `LookupHazard::STALL` + **无广播兜底**
  (`broadcast_fallback = false`)—— 且 SHALL 与 `kTrackerTraits[5]` **逐字段相等**
  (⇔ AC-3 的 behavior 两族要求)
- **AND** 子条件 ③ SHALL 成立:`caps` SHALL **仍为 4 属性** —— 层级深度是**实现参数**,不是能力属性
  (per `ADR-CEP-04 §13` 决策);SHALL **不**扩为 5 维
- **AND** **反向 fixture**:改 caps 任一字段 ⇒ 必红;实现行为与 traits 行矛盾 ⇒ 必红
- **AND** **正控**:三方逐字段相等

#### Scenario: HD-02 —— 本实现 SHALL 行使**多级聚合**这一差异化行为,SHALL **不**是 `FullMapDirectory` 克隆

- **WHEN** 核对本实现与 `FullMapDirectory` 的行为差异
- **THEN** 子条件 ① SHALL 成立:本实现 SHALL 具备**多级聚合**的可观测行为
  (层级间 sharer 信息可聚合 / 上卷,per `ADR-CEP-04 §13` 的 `AggregationAt::HIERARCHICAL` 语义)
- **AND** 子条件 ② SHALL 成立:SHALL 存在**行为级断言**证明该差异点 ——
  否则本实现就是「`FullMapDirectory` 克隆 + 纸面 caps」,正是 `ADR-CEP-04 §13` **否决**方案 D 的判词
  (「caps 声称的能力无实现可承载」)
- **AND** 子条件 ③ SHALL 成立:本实现 SHALL **不**以 `TreeManager` 降级近似
  (per `ADR-CEP-04 §13` **否决**「用 `TreeManager` 降级近似」路线 —— 那会把缺口藏进「近似」二字)
- **AND** **反向 fixture**:把聚合路径退化为单级 ⇒ 必红
- **AND** **正控**:聚合路径可观测且与单级行为**不同**

#### Scenario: HD-03 —— contention 下本实现 SHALL 返回 `STALL` hazard

- **WHEN** 在 contention 条件下 `lookup`
- **THEN** 子条件 ① SHALL 成立:`lookup` SHALL 返回 `LookupHazard::STALL`(per `kTrackerTraits[5]`)
- **AND** 子条件 ② SHALL 成立:`LocalView::valid` SHALL 为 `false`(hazard ≠ `NONE` 时 view 无意义,
  per `ADR-CEP-04 §3.1` 注释)
- **AND** **反向 fixture**:实现返回 `NONE` 或 `CHAIN` ⇒ 必红
- **AND** **正控**:无 contention 时 hazard 为 `NONE` 且 view 有效

#### Scenario: HD-04 —— 无真源的行为决策 SHALL **逐条 `basis` 登记** + 挂**跨仓追认债**

- **WHEN** 核对本实现的行为决策登记面
- **THEN** 子条件 ① SHALL 成立:每个**无 ArchForge 规约**的行为决策 SHALL 有 `basis` 式登记
  (推导依据 + 是否待追认)—— 至少覆盖:**层级数与层级语义** / **`ESCALATE_UP` 是否使用** /
  **`snoop_fold` 合并规则** / **handle 满时的拒绝语义** / **F 态(转发点)的表示**
- **AND** 子条件 ② SHALL 成立:每条决策 SHALL 标 `pending = true` ⇒ **待追认标记 SHALL 归零不得**
  —— 本仓 SHALL **不**把自推语义写成「已裁决」
- **AND** 子条件 ③ SHALL 成立:本仓 SHALL 挂**跨仓追认债**(类比规约层 D-α 的 35 格 `pending` +
  ArchForge `42 号` 逐格追认)⇒ 债项 SHALL 在 `design.md` 的**行为决策登记表**中可寻址
- **AND** 子条件 ④ SHALL 成立:登记 SHALL **不得**把 ArchForge **已追认**的内容重新标为待追认
  (登记只覆盖**本仓自推**的部分)
- **AND** **反向 fixture**:删掉任一决策的 `basis` 字段 ⇒ 必红
- **AND** **正控**:全部决策的 `basis` 非空 + `pending` 标记齐备

#### Scenario: HD-05 —— **F 态(转发点)** 在 tracker 中的表示 SHALL **显式决策**

- **WHEN** 核对本实现对 MESIF `F` 态(单 Owner + 多 Sharer + 转发点)的表示
- **THEN** 子条件 ① SHALL 成立:`SnoopState` 只有 `I` / `S` / `E` / `M`(`sharer_tracker.hh:128`)
  ⇒ **无 F** ⇒ 「谁是转发点」对 tracker **不可见** —— 本实现 SHALL **显式**声明该表示方式
  (候选:`LocalView::forward_to`(`:251`)承载 F 节点 / 登记「转发点保真度缺口」)
- **AND** 子条件 ② SHALL 成立:该决策 SHALL 纳入 HD-04 的 `basis` 登记与跨仓追认债
- **AND** 子条件 ③ SHALL 成立:SHALL **不**静默忽略 F 态 ⇒ `mesif` 的 `F` 态不得被静默当作 `E` 或 `M`
- **AND** **反向 fixture**:决策缺失 ⇒ 必红
- **AND** **正控**:决策已登记且可寻址
