## MODIFIED Requirements

### Requirement: cep-evidence-tier2-observation

CEP 的**协议级观测面** SHALL 以 **per-event** 粒度暴露原始观测事实，观测面 SHALL 由**两种载体形态**承载 —— ① **引擎侧**：协议级引擎在状态迁移与行数据装入落点暴露观测（地址 / 事件 / 迁移前后态 / 数据前后值 / sharer 快照 / 归因）;② **tracker 侧**：home 侧消息簿记层在**驱动边界**暴露观测（地址 / 消息 / 安装动作 / 安装前后副本分布 / sharer 快照 / 归因）—— 两种形态 SHALL **各自满足**本 Requirement 的判据口径、归因边界与不可判语义;**族 1 的 Scenario 实例化「引擎侧」载体形态;本 Requirement 的观测面 = 两形态之并**,观测面 SHALL **默认关闭**且**有界**,并 SHALL 使「写时该地址仍被他人持有」与「读到的值与 home 最近一次写值不符」两类判据**可机判**;同时 SHALL 把「**不可判**」与「**零违例**」严格区分。

#### Scenario: 观测面 SHALL 暴露 per-event 原始事实

- **WHEN** 一个协议引擎在**状态迁移**与**行数据装入**两个落点上产生观测
- **THEN** 每次 SHALL 产生**恰好 1 条**观测记录,记录 SHALL 含地址 / 事件码 / 迁移前后态 / 数据前后值 / sharer 快照 / 归因来源
- **AND** 记录 SHALL **不**含版本号字段(per-engine 私有写序在自读自写下为恒等式 ⇒ 无判别力)
- **AND** sharer 快照 SHALL 取自**只读**查询(⛔ SHALL NOT 使用有副作用的查找路径 —— 那会污染目录的事件流)
- **AND** 迁移落点的 sharer 快照 SHALL 为**迁移前**视图(目录同步发生在迁移之后)

#### Scenario: tracker 侧载体 SHALL 在驱动边界产生 per-installation 观测

- **WHEN** home 侧消息簿记层将一条 TileLink-C 消息翻译为树表安装动作（`set_sharer` / `clear_sharer`）
- **THEN** 每次安装 SHALL 在**驱动边界**产生**恰好 1 条**观测记录，含消息 id / 地址 / 节点 / 安装动作 / **安装前**副本分布快照 / 归因来源（⛔ 记录 SHALL **NOT** 含**安装后**副本分布快照 —— 观测点在安装**前**发出 ⇒ 记录内只有前值;⛔ 记录 SHALL **NOT** 含迁移前后态 / 数据前后值 —— 簿记层无引擎态迁移）
- **AND** 快照 SHALL 取自 `TreeManager::view_of` 的 const 纯查询（⛔ SHALL **NOT** 用有副作用的 `lookup`;⛔ SHALL **NOT** 把观测点放进 `TreeManager` 内部 —— 观测它自己维持的不变量 ⇒ 同义反复 ⇒ 必绿）
- **AND** 快照时点 SHALL 为**安装前**视图
- **AND** 观测面 SHALL **默认关闭**且**有界**，丢弃计数非零 ⇒ 该次观测**不可判**（⛔ SHALL **NOT** 读作零违例）
- **AND** 装配前提不成立（node 未映射 / 多 Tip / 观测面关闭）SHALL 递增**观测面失效计数**并判**不可判**（⛔ 不可判 SHALL **NOT** 计为零违例）
- **AND** **未登记**的（消息, 载荷）组合 SHALL **fail-closed**（不装表 ⇒ 计入不可判），⛔ SHALL **NOT** 被静默解释为最接近的合法动作
- **AND** 每条观测的 sharer 位 SHALL 可溯源到输入流消息 id;**绕过驱动**直接安装产生的位 SHALL 判 `provenance_broken` 且 SHALL **NOT** 出现在任何观测快照中

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
