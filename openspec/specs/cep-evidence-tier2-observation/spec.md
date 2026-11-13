# cep-evidence-tier2-observation Specification

## Purpose
CEP 一致性证据的**档 2**(协议级语义)在本仓**尚无载体** —— `07 号` 的三档阶梯表里档 2 的机器载体写「⭕ 观测面未建(另立 change 评估)」(`docs/architecture/07-coherence-exploration-platform.md:696`)。而 `ADR-CEP-20` 决策 5 已判「**判别力的强度上限在本仓的协议引擎面**(⛔ 非跨仓专属)」⇒ 天花板档**就在本仓**。

同时,现有实现面**无法产出协议级观测事实**:

- **引擎族无观测面** —— 协议引擎基类只有**聚合计数器**(`protocol_engine_base.hh` 的 `counts()` 家族)⇒ **无 per-txn / per-event 观测钩子**;
- **行数据写入点与状态迁移点分离** —— `mesif_protocol_engine.hh` 的 `apply_event` 只改**状态**,行数据的**唯一**写入点是完成回调 ⇒ 在迁移点采到的数据恒为旧值;
- **sharer 集合可查但查询有副作用** —— `SharerTrackerIf::lookup` 返回含 sharer 位掩码的视图,但 `FullMapDirectory::lookup` 在首次转发时会分配目录项并记事件 ⇒ 观测点**不得**自行调用它。

本 capability 把**档 2 的观测面**契约化 —— **协议引擎暴露 per-event 原始观测事实**,并规定其**判据口径**、**归因边界**与**不可判语义**。

> **演进方式**:本 spec 经由 OpenSpec delta(**ADDED**)演进,**不直接改写**本文件。
> **规范载体**:`docs/architecture/07-coherence-exploration-platform.md` 的新 `## Status Update` + `docs/guide/cep-usage.md`
> **canonical 实现**:`include/tlm/cep/protocol_engine_base.hh` 的 `EventObservation` + `include/tlm/cep/mesif_protocol_engine.hh` 的两处观测落点 + `test/test_cep_tier2_observation.cc`
> ⚠️ **本档位不产 4 件套 run** ⇒ **无 `ic3_verdict` / 无「通过态」**(per architect 裁决 R3)⇒ ⛔ 本 spec **不**定义任何 verdict 词表。

## Requirements

### Requirement: cep-evidence-tier2-observation

CEP 的**协议级引擎** SHALL 暴露 **per-event** 的原始观测事实(地址 / 事件 / 迁移前后态 / 数据前后值 / sharer 快照 / 归因),观测面 SHALL **默认关闭**且**有界**,并 SHALL 使「写时该地址仍被他人持有」与「读到的值与 home 最近一次写值不符」两类判据**可机判**;同时 SHALL 把「**不可判**」与「**零违例**」严格区分。

#### Scenario: 观测面 SHALL 暴露 per-event 原始事实

- **WHEN** 一个协议引擎在**状态迁移**与**行数据装入**两个落点上产生观测
- **THEN** 每次 SHALL 产生**恰好 1 条**观测记录,记录 SHALL 含地址 / 事件码 / 迁移前后态 / 数据前后值 / sharer 快照 / 归因来源
- **AND** 记录 SHALL **不**含版本号字段(per-engine 私有写序在自读自写下为恒等式 ⇒ 无判别力)
- **AND** sharer 快照 SHALL 取自**只读**查询(⛔ SHALL NOT 使用有副作用的查找路径 —— 那会污染目录的事件流)
- **AND** 迁移落点的 sharer 快照 SHALL 为**迁移前**视图(目录同步发生在迁移之后)

#### Scenario: 观测面 SHALL 默认关闭且有界

- **WHEN** 未显式开启观测的引擎执行任意迁移
- **THEN** SHALL **不**产生任何观测记录(⇒ 既有行为与性能零影响)
- **AND** 观测缓冲 SHALL 有**容量上限**;溢出时 SHALL 递增**丢弃计数** ⇒ ⛔ SHALL NOT 静默丢弃
- **AND** 丢弃计数非零 SHALL 使该次观测**不可判**(⛔ SHALL NOT 被读作「零违例」)

#### Scenario: 装配前提不成立 SHALL 判「不可判」

- **WHEN** 协议引擎的 sharer 目录接线前提不成立(目录未接线 / 节点未映射到 sharer 位 / 目录层级越界)
- **THEN** SHALL 递增**观测面失效计数**
- **AND** 失效计数非零 SHALL 使该次观测**不可判** ⇒ ⛔ SHALL NOT 记为零违例
- **AND** SHALL **不**改变引擎的既有状态迁移行为

#### Scenario: SWMR 判据 SHALL 以「写时存在他人 sharer」为口径

- **WHEN** 判定一次写是否构成 sharer 违例
- **THEN** 判定 SHALL **只**取「状态迁移落点 + 写事件 + 迁移后为脏态」的记录 ⇒ 装行落点 SHALL **显式豁免**(同一次写会产生 2 条迁移记录 + 1 条装行记录 ⇒ 逐行计数会重复计数)
- **AND** 违例 SHALL 判为「该地址的 sharer 快照中**除本节点外**仍有其他持有者」
- **AND** 该判据的归因 SHALL 为 **home 侧可见性**(run 内 home 行为)⇒ ⛔ SHALL NOT 表述为「引擎实现错误」

#### Scenario: data-value 判据 SHALL 只在可达形态下成立

- **WHEN** 判定一次读到的值是否与 home 最近一次写的值不符
- **THEN** 判定 SHALL 比较装行落点的观测值与 home 侧最近写值
- **AND** 在**生产流**中 home 总是回最新值 ⇒ 该判据**恒为零** ⇒ 该零 SHALL 登记为「**本装置无该形态**」⇒ ⛔ SHALL NOT 表述为「已证明协议层无违例」
- **AND** 其可达形态 SHALL **仅限**「注入陈旧副本」的负控

#### Scenario: 档 2 SHALL NOT 与档 0 / 档 1 的结论并列

- **WHEN** 引用档 2 的观测结论
- **THEN** SHALL 同时声明「**单族 / 单拓扑 / 无通过态**」⇒ 档 2 ⭔ 不产 run ⇒ ⛔ SHALL NOT 存在档 2 的「通过态」
- **AND** 档 2 与档 1 SHALL 为**正交**(协议引擎状态机与目录可见性 ↔ 完成域模型)⇒ ⛔ SHALL NOT 表述为档 2 强于档 1
- **AND** 档位阶梯的排序语义 SHALL 为「**装置归属层级**」⇒ 跨档强度 SHALL NOT 可比
- **AND** 档 2 的判别力 SHALL NOT 被读作「协议语义全覆盖」(权限维与标签维不在本档位范围内)

#### Scenario: 判据 SHALL 可机判且有反向 fixture

- **WHEN** 校验本档位的判据
- **THEN** 每条判据 SHALL 有**同流 + 单差异**的正负配对(违例计数 SHALL 为**精确值**,⛔ SHALL NOT 以「大于零」代替)
- **AND** 「观测面失效」「丢弃计数非零」「前提不成立」SHALL 各有**独立红因** ⇒ ⛔ SHALL NOT 以无签名失败呈现
- **AND** 同一流重跑 SHALL 产生**逐字相等**的观测记录
