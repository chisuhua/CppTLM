# Spec: CEP 核心契约层(Stage 0)

> **状态**: 📋 Draft v1.0(per tasks.md Phase 1~8)
> **配套**: [`../../proposal.md`](../../proposal.md) · [`../../design.md`](../../design.md) · [`../../tasks.md`](../../tasks.md)
> **版本**: v1.0(2026-10-21,planner 阶段 P1 三项完成后交 builder)
> **上游真源**: [`docs/architecture/07-coherence-exploration-platform.md`](../../../../../docs/architecture/07-coherence-exploration-platform.md) v0.1 §3~§7 + `docs/adr/ADR-CEP-*.md` 共 16 份
> **验收归属**: 本 spec 的**契约存在性与自洽性**由 Stage 0 机器检查;**实现满足性**由 Stage 1~3 各自 change 的 AC 落地

---

## ADDED Requirements

> **编号位口径**:**67 个编号位 / 66 条实际守门 invariant**(`INV-CEP-04-IC4` **唯一跳空**)。
> 15 个 requirement 与 `objective-cep-platform §0 DoD 7` 的 **15 组**一一对应,合计 67 编号位 / 66 实际。
> ⚠️ **计数口径纪律**:跳空 ID 被多处**提及**(说明为何跳空)但**从未定义** —— 校验脚本**必须区分「提及」与「定义」**。

### Requirement: cep-coherence-interface-contract

CEP SHALL 定义 **canonical 10 条 Coherence Interface 流**及其 **11 条 `CEN-Ib` 不变量**(`CEN-Ib1` ~ `CEN-Ib11`),作为一切协议实现的唯一事务抽象。

流清单(`ADR-CEP-02 §2`):`request` / `completion` / `snoop` / `snoop_resp` / `writeback` / `forward_data` / `back_invalidate` / `address_resolve` / `barrier` / `snoop_completion_ack`。

#### Scenario: 10 条流清单存在且与 ADR 逐字一致

- **WHEN** 契约测试读取 `docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md` 的流清单
- **THEN** 恰好得到 **10** 个流名,且与下述集合逐字相等:`request` / `completion` / `snoop` / `snoop_resp` / `writeback` / `forward_data` / `back_invalidate` / `address_resolve` / `barrier` / `snoop_completion_ack`
- **AND** 清单**不含**第 11 条流(per `ADR-CEP-02` 零流数变更承诺)
- **AND** `CEN-Ib1` ~ `CEN-Ib11` 共 **11** 条编号连续、无跳空

#### Scenario: 流配对纪律(CEN-Ib1 / CEN-Ib3 / CEN-Ib4 / CEN-Ib10)

- **WHEN** 契约测试枚举流间的配对关系
- **THEN** `request` 必有配对 `completion`(`CEN-Ib1`)
- **AND** `snoop` 必有 `snoop_resp`(`CEN-Ib3`)
- **AND** `snoop_resp` 必须含 `txn_id` 用于 ack 配对(`CEN-Ib4`)
- **AND** `snoop_completion_ack` 配对 `snoop_resp` 而**非** `completion`(`CEN-Ib10`)

#### Scenario: barrier 流不承载 group 语义(CEN-Ib9)

- **WHEN** 契约测试检查流 9 `barrier` 的字段契约
- **THEN** `barrier` 流**不携带** `group_id` / `participant_count` / `epoch_id`
- **AND** 其语义为 **requester-scoped**(排空 / 可见性点)
- **AND** group-scoped 完成纪律归 Fabric Collective Plane 的 `collective_barrier` 原语(per `ADR-CEP-02 §2 Decision 3`)

#### Scenario: atomic 意图的承载方式(CEN-Ib11)

- **WHEN** 契约测试检查流 1 `request` 的属性契约
- **THEN** atomic 意图作为 `request` 流的**独立属性**承载(与 `perm` **正交**)
- **AND** `perm` 枚举**维持 4 值锁**:`NONE` / `READ` / `UNIQUE` / `WRITEBACK`
- **AND** `perm` 值域中**不存在** `"ATOMIC"` 之类的第 5 个值
- **AND** 不变量集合中**不存在** `CEN-Ib12`(`perm` 扩展被显式禁止)

### Requirement: cep-stub-cache-contract

CEP SHALL 定义 Stub Cache Core 的 **RN 侧投影**接口与 **7 条不变量**(`INV-SC1` ~ `INV-SC6` + `INV-CEP-05-IC1`),并显式声明其**定位**。

**接口(6 个)**:net-side 5(`request(addr, perm)` / `snoop(addr, type, txn_id)` / `snoop_resp(addr, hit, state, dirty, data, txn_id)` / `writeback(addr, data, dirty)` / `completion(txn_id, data, perm, error)`)+ core-side 1(`evict_line(addr)`)。

**定位(契约的一部分,per `ADR-CEP-05 §2` v0.3 澄清)**:Stub Cache Core **不是缓存性能模型**,而是**客户端侧协议状态投影**;只暴露**稳定状态** `NONE` / `I` / `S` / `E` / `M`。

#### Scenario: 折叠映射表完整(5 隐藏流 → stub 原语)

- **WHEN** 契约测试检查 `ADR-CEP-05 §3` 的折叠映射表
- **THEN** 恰好 **5** 条隐藏流有映射:`forward_data` → `completion` 不可分辨性 / `back_invalidate` → 普通 invalidate snoop / `barrier` → 对 stub 不可见 / `address_resolve` → ProtocolEngine 内终结 / `snoop_completion_ack` → ProtocolEngine 内部消化
- **AND** stub 的净侧接口数为 **5**(`request` / `snoop` / `snoop_resp` / `writeback` / `completion`)+ core-side 原语 **1**(`evict_line`)= **6** 总数
- **AND** 折叠表中的每条 `key 不变量` 均可回指到一条 `CEN-Ib` 或 `INV-SC`

#### Scenario: stub 只暴露稳定状态(INV-CEP-05-IC1)

- **WHEN** 契约测试检查 Stub Cache 的权限状态表达
- **THEN** 权限枚举**恰好 5 个值**:`NONE` / `I` / `S` / `E` / `M`
- **AND** 枚举中**不含**任何瞬态态(per ProtoGen 生成的 non-stalling MSI 约 18-20 状态,per ISCA 2018 p.1 / p.12)
- **AND** 枚举中**不含**协议私有状态(token / HTM 的 read-set / write-set)
- **AND** `perm` 值域**不含** CHI 的 `UC` / `SC` / `UD` / `SD` 七态

#### Scenario: stub 行为约束(INV-SC1~6)

- **WHEN** 契约测试逐条检查 6 条 `INV-SC`
- **THEN** 6 条**全部存在且语义与 ADR 一致**:同一 addr 至多 1 个 outstanding request / evict 不选 outstanding line / snoop 响应在固定延迟内返回 / completion 与 request 严格配对且 `txn_id` 沿用 `ADR-X.1` 命名 / dirty writeback 必带 data 且 clean 不带 / hit 路径零网络流量
- **AND** `INV-SC1` 的"每 addr 至多 1 个 outstanding"被标注为**刻意的保守简化**(与 ProtoGen 的 cache 侧并发度上限一致)

### Requirement: cep-sharer-tracker-contract

CEP SHALL 定义 Sharer Tracker 的 **5 种实现** + **caps descriptor** + **6 条不变量**(`INV-CEP-04-IC1/2/3/5/6/7`;`IC4` 永久跳空)。

#### Scenario: 5 实现 + caps 四属性齐备

- **WHEN** 契约测试检查 `ADR-CEP-04 §2` 的实现表
- **THEN** 恰好 **5** 个实现:`FullMapDirectory` / `SparseSnoopFilter` / `DcohSymmetric` / `TreeManager` / `BroadcastNull`
- **AND** 每个实现的 caps **恰好 4 属性**:`scope` / `completeness` / `authority` / `aggregation_at`
- **AND** 实现表中**不含** `is_distributed` / `is_central` 之类的 bool flag

#### Scenario: caps 值域收敛(IC1 / IC6)

- **WHEN** 契约测试枚举 caps 的全部枚举值
- **THEN** 每个属性字段的取值**全部落在已声明的 enum 字面值集合内**(机器可检查)
- **AND** `Hazard_*` 枚举(`Hazard_NONE` / `Hazard_STALL` / `Hazard_CHAIN`)在 5 种实现间**字面值收敛**

#### Scenario: token 类协议不跟踪 sharer set(IC7)

- **WHEN** 契约测试检查 token 类协议实例的 Sharer Tracker 选型
- **THEN** 该实例的 `sharer_tracker.impl` 为 `BroadcastNull` 或标记为**不参与**
- **AND** **不得**为 `FullMapDirectory`(token 由数据副本携带权限,不依赖中心 sharer set)
- **AND** 自失效 / write-through 类协议(TSO-CC / RCC / RCC-O / GPU / PLO-CC)**不得**填 `SparseSnoopFilter`(snoop filter 本身即一种 sharer tracker)

### Requirement: cep-experiment-output-contract

CEP SHALL 定义实验输出的 **4 件套** + **13 个必纳 metric** + **5 条不变量**(`INV-CEP-06-IC1` ~ `IC5`),并定义**验证深度契约**。

4 件套:`simulation_config.json` + `trace.h5` + `metrics.json` + `report.html`。

#### Scenario: 4 件套与 13 metric 齐备(IC1 / IC2)

- **WHEN** 契约测试检查 `ADR-CEP-06 §2` 的定义
- **THEN** 4 件套**恰好 4 个**,缺一件 = fail
- **AND** `metrics.json` 含 **13** 个必纳字段:`total_cycles` / `total_transactions` / `transactions_per_cycle` / `avg_latency_cycles` / `p50_latency_cycles` / `p95_latency_cycles` / `p99_latency_cycles` / `max_latency_cycles` / `coherence_stalls` / `collective_stalls` / `directory_misses` / `deadlock_violations` / `memory_violations`
- **AND** 不可观测 metric 标 `not-collected` + `_not_collected_reason`,**禁止**省略 / 写 0 / 写 -1(IC4)

#### Scenario: 一致性闸门与违例类型(IC3 / IC5)

- **WHEN** 契约测试检查一致性闸门定义
- **THEN** `deadlock_violations == 0 && memory_violations == 0` 才判 pass,否则 = 一致性违例,fail
- **AND** `memory_violations` 的判定**必须覆盖 SWMR + data-value 双不变量**
- **AND** 报告**能区分违例类型**(SWMR 违例 / data-value 违例)

#### Scenario: 验证深度必须声明且不得越界表述

- **WHEN** 契约测试检查实验元数据契约
- **THEN** 每次实验**必须**声明 `verification_mode` ∈ {`exhaustive`, `bounded_random`, `sampled`} 与 `verification_scope`(规模上限描述)
- **AND** `bounded_random` / `sampled` 的结果**不得**被表述为"已证明无死锁 / 无违例" —— `deadlock_violations == 0` 只代表"**在声明的 `verification_scope` 内未观测到**"
- **AND** `INV-CEP-06-IC3` 的"应 0"语义**随 `verification_mode` 变化**,须在报告中一并呈现
- **AND** 平台**不承诺**"任意配置都能穷尽验证"(极端配置需 1TB RAM + 数小时,per HeteroGen 仓库 README)

### Requirement: cep-integration-seam-contract

CEP SHALL 定义 **simulation_mode 单一维度** + **ModuleFactory 真实注册 seam** + **Null 默认实现** + **7 条不变量**(`INV-CEP-07-IC1` ~ `IC7`)。

#### Scenario: simulation_mode 单一维度(IC1)

- **WHEN** 契约测试检查 JSON 顶层字段
- **THEN** `simulation_mode` ∈ {`functional`, `timing`} 是**唯一**模式维度
- **AND** 配置中**不存在** `network.mode` 字段
- **AND** 协议插件**不引入** `network.mode`(机器 grep 可查)

#### Scenario: ModuleFactory 注册走真实机制(IC4 / IC5)

- **WHEN** 契约测试检查 CEP 模块注册路径
- **THEN** ProtocolEngine 派生类经 `ModuleFactory::registerObject<T>` 注册
- **AND** SharerTracker / CollectivePlane 派生类经 `REGISTER_MODULE(T)` 注册
- **AND** 注册路径**不含**任何新发明的宏形式
- **AND** `include/abi/cpptlm_emulator.h`(23 ABI)与 4 个 frozen headers 的 `git diff` **为空**

#### Scenario: Null 默认实现(IC2 / IC3)

- **WHEN** 契约测试检查默认绑定
- **THEN** `NullProtocolEngine` 是 ProtocolEngine 默认实现,10 条流**全部**有覆写(覆盖矩阵完整,无遗漏)
- **AND** `NullCollectivePlane` 是 Collective Plane 默认实现
- **AND** functional 模式下 `avg_latency_cycles == 0` 成立(IC6)

#### Scenario: 插件接口为组合预留接缝(IC7)

- **WHEN** 契约测试检查 ProtocolEngine 插件的声明面
- **THEN** 插件**声明**其 `ordering_assumption` + `vc_binding`
- **AND** 插件**不实现**跨域 / 跨层组合逻辑(组合归 Directory & Ordering Plane 编排层)
- **AND** 组合接缝点可被合同测试覆盖

### Requirement: cep-numbering-contract

CEP SHALL 维护 **CEP-NN 编号单一真源**与 **3 条不变量**(`INV-CEP-NUM-IC1` ~ `IC3`)。

#### Scenario: 编号表是唯一真源(IC1 / IC2)

- **WHEN** 契约测试检查 `ADR-CEP-NUMBER-MAP §2`
- **THEN** `CEP-00` ~ `CEP-18` 每个编号**恰好一条**表项,含议题 / 依赖 / 状态
- **AND** 新增 ADR 前**必须**先查该表(无重号)
- **AND** 跨 ADR 不变量引用**统一加 `INV-` 前缀**(`INV-CEP-04-IC1` 而非 `CEP-04-IC1`)
- **AND** ADR 编号与不变量 id 的**命名空间分离**(`ADR-CEP-13` 与 `INV-CEP-13-NC` 不混用)

#### Scenario: 06 §15.4 口径冻结(IC3)

- **WHEN** 契约测试检查引用 06 P 编号的 CEP 文档
- **THEN** 引用的 P 编号**全部**落在 `06 §15.4` 的口径内(P2.0 / P2.1 / P3 / P3.5 / P4 / P5)
- **AND** **不**出现 `06 §11.4` 的 P6 口径

### Requirement: cep-ordering-point-contract

CEP SHALL 定义 **Ordering Point** 语义、**可迁移性三判据**与 **3 条不变量**(`INV-CEP-08-IC1` ~ `IC3`)。

#### Scenario: 唯一序列化点(IC1)

- **WHEN** 契约测试检查 Ordering Point 定义
- **THEN** 同一一致性域内,任意两条冲突事务**恰在一个** Ordering Point 序列化
- **AND** Ordering Point 被定义为**唯一逻辑点**,**不绑定**物理实体

#### Scenario: 可迁移性三判据齐备

- **WHEN** 契约测试检查 `ADR-CEP-08 §2` 决策 3
- **THEN** **恰好 3** 条判据:`J1` 单点性(所有冲突事务必经该点)/ `J2` 全序可观测性(序对所有 sharer 一致可见)/ `J3` 唯一性保持(不得被两点定出冲突的序)
- **AND** `J2` 的成立条件显式依赖 Sharer Tracker `completeness` **非** `PARTIAL`(或具备 broadcast fallback)

#### Scenario: E 态静默升级为组合必查项(IC2)

- **WHEN** 契约测试检查跨层 / 跨域组合的可行性检查项
- **THEN** 检查项集合**必须包含** "E 态静默升级" 项
- **AND** 该项**缺失**时检查结果为 fail(不得静默跳过)
- **AND** `ADR-CEP-05 §7` 存在**第二条** α 失效判据(与"协议信息泄漏给 stub"方向相反的"E 态信息不足"方向)

### Requirement: cep-multi-vc-deadlock-freedom-contract

CEP SHALL 定义 **通道依赖图**、**装配期无环性判据**与 **3 条不变量**(`INV-CEP-09-IC1` ~ `IC3`),并明确 VC 分配的**首要用途是死锁自由性**而非 QoS 调优。

#### Scenario: 装配期无环性检查为必过项(IC1)

- **WHEN** 契约测试检查拓扑 / 配置校验的义务
- **THEN** 每个协议实例组合**必须**通过**装配期**通道依赖图无环性检查**
- **AND** 不通过 = **配置 fail**,**不得**以"运行时注意"替代
- **AND** 检查**不得**被实现为运行时计数器(运行时 `deadlock_violations` 是**发现**手段,非**证明**手段)

#### Scenario: 仲裁策略与依赖图一致(IC2 / IC3)

- **WHEN** 契约测试检查仲裁策略的约束
- **THEN** 仲裁策略**不得**在通道依赖图上引入新环
- **AND** 任一协议实例的 `ordering_assumption` 或 `vc_binding` 变更 ⇒ 依赖图**重算** ⇒ 无环性检查**重跑**

#### Scenario: VC 分配与跨平面隔离的边界

- **WHEN** 契约测试检查 VC 规则的归属
- **THEN** 平面内 VC 无环性规则由本 requirement 承载
- **AND** 跨平面(coherence ↔ collective)VC 隔离规则由 `ADR-CEP-01` 承载,**不重复**
- **AND** 文档中关于"总线亦可作序列化点"的引用**必须**标注"Future Work,未实现"(per 博士论文 §9.3.4)

### Requirement: cep-capability-map-contract

CEP SHALL 定义 **协议实例能力声明 schema**(`include/tlm/cep/protocol_instance.schema.json`)与 **3 条不变量**(`INV-CEP-12-IC1` ~ `IC3`)。

#### Scenario: schema 必含 7 个字段组 / 13 个顶层属性(IC1)

- **WHEN** 契约测试校验 schema 定义
- **THEN** schema 必含 **7** 个字段组(共 **13** 个顶层属性):`id`/`family`/`adapter`/`status`/`protocol_version` / `flow_support` / `perm`+`atomic` / `consistency_labels` / `ordering_assumption`+`vc_binding` / `sharer_tracker` / `combination_capabilities`
  > ⚠️ **口径修订(2026-10-21,per 代码复审)**:原写「8 个字段组」但枚举只有 **7** 组;
  > `ADR-CEP-12` IC1 原写「6 个」且枚举 7 组、`ADR-CEP-12` 决策汇总写「8 个」、
  > `ADR-CEP-17 §3.4` 写「6 个」—— **四处互不一致且无人断言**。
  > 现统一为 **7 个字段组 / 13 个顶层属性**(= schema 实际属性数),并由测试机器断言。
- **AND** `flow_support` **恰好 10 项**,与 canonical 10 流**逐名对齐**
- **AND** `perm` 值域**恰好 4 值**(`NONE` / `READ` / `UNIQUE` / `WRITEBACK`)
- **AND** `combination_capabilities` **恰好 3 维**(`concurrent` / `hierarchical` / `heterogeneous`),且**正交于** `family`
- **AND** `consistency_labels` 必含 `read` / `write` 两级 **+** `request_level` 映射(承载"同一 wire 请求在不同标签下语义不同")

#### Scenario: 逐网络校验 ordering_assumption / vc_binding(IC2,2026-10-22 修订)

- **WHEN** 契约测试加载一份 `ordering_assumption` / `vc_binding` 为 **per-network 嵌套形态**的协议实例声明
- **THEN** 对每个 network `n` ∈ `ordering_assumption` ∪ `vc_binding`:
  - `ordering_assumption[n]` 的每个 `vc_id` ∈ `vc_binding[n].values()`
  - `vc_binding[n].values()` 中每个 `vc_id` **<** `fabric_config.networks[n].vc_count`(槽位上界)
  - `vc_binding[n]` 的每个消息类 ∈ **6 类规范词表** `{request,response,snoop,snoop_resp,atomic,query}`
- **AND** network 须在 fabric 网络命名空间中登记(per `22-nsa §0.5.4.1` NS-R1);未登记 ⇒ **配置加载 fail**
- **AND** `vc_id` 取值 ∈ 已声明域(per `noc_bundles_tlm.hh:38` 的 0-3)
- **AND** `ordering_assumption` 与 `vc_binding` 的 network 键集**必须相等**(逐网络对称,per ArchForge `32 号 §6.2`)

> ⚠️ **取代说明(2026-10-22,per ADR-CEP-12 §8 Status Update)**:本 scenario 原为
> 「named_network 必须解析到唯一 `vc_id`」——**「唯一性」这一谓词在 per-network 嵌套形态下整体消失**
> (network 即 key,其下 `vc_id` 是**显式**给出的,不存在多义)。
> **机器校验**:`scripts/test/cep_decl_alignment_check.sh` 第 ②/③/④/⑤ 项。

#### Scenario: status 是计数口径的唯一依据(IC3)

- **WHEN** 契约测试统计 adapter 实例数
- **THEN** 仅 `status == implemented` 的实例计入
- **AND** `planned` / `excluded` **不**计入
- **AND** 统计结果与 `07 号 §5.1` 的口径表述**一致**

### Requirement: cep-coherence-domain-contract

CEP SHALL 定义 **一致性域划界**、**跨域组合判据**与 **3 条不变量**(`INV-CEP-13-IC1` ~ `IC3`),并明确**组合承诺边界**。

#### Scenario: 复合一致性判据(IC1)

- **WHEN** 契约测试检查跨域组合的正确性判据
- **THEN** 判据采用**复合一致性模型**:`acyclic( ppo_com ∪ rfe ∪ fr ∪ ws )`,其中 `ppo_com(t) ≡ ppo_i`(t 属于域 i)
- **AND** 域的定义 = **一个 Ordering Point 生效 + 一套自己的 `ppo` 生效**
- **AND** `barrier_domain` 被显式标注为**作用域标签**,**不等于**一致性域边界

#### Scenario: 域可见写触发(IC2)

- **WHEN** 契约测试检查跨域写传播的触发条件
- **THEN** 传播**必须**由"域可见写"判据触发:(a) 终态 `s1` 允许 store 命中且无需外部通信 **且** (b) `s1` 或 `s2` 接受导致数据值响应的转发请求
- **AND** (a)(b) **必须同时**评估 —— 仅满足 (a) 的读请求**不得**被误判为写

#### Scenario: 桥接并发粒度由协议特征决定(IC3)

- **WHEN** 契约测试检查跨域桥接的并发粒度选择
- **THEN** 粒度由"**对端协议是否允许提前 ack**"决定:允许 ⇒ 保守(全串行);不允许 ⇒ 激进(同地址串行,跨地址可并发)
- **AND** 选择依据**必须被记录**(可追溯到协议实例声明)

#### Scenario: 组合承诺边界显式化

- **WHEN** 契约测试检查 CEP 对"分层 + 异构"组合的承诺表述
- **THEN** 文档**承诺**三件事:判据可复用(结构化声明项)/ 逐组合**实验证伪**(在声明的 `verification_scope` 内)/ 接口预留组合接缝
- **AND** 文档**不承诺**:自动化组合生成 / 自动兼容性判定器 / 无违例保证 / 任意配置穷尽验证
- **AND** 任何文档**不得**把 `deadlock_violations == 0` / `memory_violations == 0` 表述为"已证明"
- **AND** Stage 0 验收清单**必须包含一项独立 AI 复审**(per `ADR-CEP-13` 决策 5 ② 的替代机制 M4;先例:2026-10-21 Oracle 独立架构复审)

### Requirement: cep-protocol-version-locking-contract

CEP SHALL 以**锁定表**为规范版本的**唯一真源**,并要求每个协议实例声明其规范版本。

#### Scenario: 版本取值必须落在锁定表内(IC1)

- **WHEN** 契约测试校验 `ADR-CEP-11 §3` 的锁定表与 schema 的 `protocol_version`
- **THEN** 锁定表**恰好 6 个协议族**:CHI / CXL 2.0 非对称 / CXL 3.0 对称 / TileLink-C / NVLink / UALink
- **AND** 每个协议实例的 `protocol_version` **必须**落在锁定表内
- **AND** 表外取值 ⇒ **装配期 fail**(不得静默降级)

#### Scenario: 锁定表是唯一真源(IC2)

- **WHEN** 契约测试扫描 schema / ADR / 代码
- **THEN** 规范版本号**只出现在** `ADR-CEP-11 §3` 的锁定表中
- **AND** 其余位置**只引用不复制**

#### Scenario: 版本变更必须留痕(IC3)

- **WHEN** 契约测试检查变更留痕机制
- **THEN** 规范版本变更**必须**在 `objective-cep-platform §11` 台账留 `scope-change` / `adr-amendment` entry
- **AND** 变更路径**必须**落在三选一内:仅锁表更新 / 语义适配 + adapter 变更 / 并行版本(后者**不支持**)

### Requirement: cep-stub-contract-tests-contract

CEP SHALL 以 **C1~C10** 十条合同测试承载 Stub Cache Core 的契约,且合同测试**必须**含**负向断言**。

#### Scenario: 每条合同测试有 invariant 锚点(IC1)

- **WHEN** 契约测试校验合同测试集
- **THEN** C1~C10 **每一条**可定位到至少一条 `INV-SC*` / `INV-CEP-05-IC1` / `CEN-Ib*` / `INV-CEP-07-IC7` / `INV-CEP-08-IC2`
- **AND** **不存在**无锚点的合同测试

#### Scenario: 必须含负向断言(IC2)

- **WHEN** 契约测试校验合同集的**方向完整性**
- **THEN** 合同集**必须**含至少一条**负向断言**(C9:Stub 不泄漏协议信息)
- **AND** 该负向断言**必须**覆盖:Stub 公开接口不含瞬态态 / token / read-set;Stub 源码**不含**协议名与协议族标识符;Stub **不感知** `ordering_assumption` / `vc_binding` / `consistency_labels`
- **AND** 缺负向断言 = 合同集**不完整**

#### Scenario: 组合接缝检查项不可静默跳过(IC3)

- **WHEN** 组合检查器执行跨层 / 跨域组合的可行性检查
- **THEN** 检查项**缺失**(含 "E 态静默升级" 项)时,结果**必须**判 **fail**
- **AND** Stage 0 校验"检查项**存在**",Stage 1 校验"检查项**被执行**"

### Requirement: cep-cachetlm-alignment-contract

CEP SHALL 与既有 `CacheTLM` **共存不替换**,且 `CacheTLM` 保持冻结面。

#### Scenario: CacheTLM 零 diff(IC1)

- **WHEN** 契约测试检查冻结面
- **THEN** `include/tlm/cache_tlm.hh` 与 `src/tlm/cache_tlm.cc` 的 `git diff` **为空**
- **AND** `CacheTLM` 源码**不含** `snoop` / `coherence` / `MESI` / `MOESI` / `directory` 标识符(它无协议知识)

#### Scenario: CEP 侧只有一个缓存实现(IC2)

- **WHEN** 契约测试检查 CEP 的缓存侧类
- **THEN** `include/tlm/cep/` 下**只有** Stub Cache Core 一个缓存实现
- **AND** **不存在** `CacheTLM` 的派生类或副本

#### Scenario: 两侧只经 adapter 交互(IC3)

- **WHEN** 契约测试检查 CEP 与 `CacheTLM` 的交互面
- **THEN** `CacheTLM` **不实现** canonical 10 流的任何一条
- **AND** 二者**只允许**通过 adapter 交互(Stage 4+);Stage 1~3 **不**交付该 adapter

### Requirement: cep-evolution-route-mapping-contract

CEP SHALL 明确 `06 §15.4` 的 P2.0~P5 与 CEP Stage 的逐项对应,并**排除** P6。

#### Scenario: P 编号口径唯一(IC1)

- **WHEN** 契约测试扫描 CEP 文档引用的 `06` P 编号
- **THEN** 全部落在 `06 §15.4` 口径内(P2.0 / P2.1 / P3 / P3.5 / P4 / P5)
- **AND** **不出现** P6(`06 §11.4` 口径**不得**被引用)

#### Scenario: 每个 Stage 有承接声明(IC2)

- **WHEN** 契约测试检查 Stage 与 06 §15.4 条目的对应
- **THEN** 每个 Stage **至少**有一条 §15.4 承接声明(或显式声明"不承接")

#### Scenario: 协议语义项归 ArchForge(IC3)

- **WHEN** 契约测试扫描 CppTLM 仓的协议语义实现
- **THEN** `MesiGpuProtocol` / `MesiThreeLevelProtocol` / `translate_snoop` **不得**有实现
- **AND** 三项由 ArchForge 承接;CppTLM 侧只提供 adapter 载体 + 能力声明 schema + 实验床

### Requirement: cep-06bis-artifact-disposition-contract

CEP SHALL 对 06-bis 的 7 项既有产物给出**逐项处置**,并对**协议泄漏**施加通用禁令。

#### Scenario: 既有产物已废弃且被标注(IC1)

- **WHEN** 契约测试检查 `include/core/coherence_domain.hh` / `src/core/coherence_domain.cc`
- **THEN** 两者**带** `[[deprecated]]` 标记 + 指向 `ADR-CEP-07A` 的顶部注释
- **AND** **不被**任何新代码引用
- **AND** **文件不删除**(Stage 4+ 再议)

#### Scenario: 协议泄漏通用禁令(IC2)

- **WHEN** 契约测试扫描与协议无关的层(Stub Cache Core / Directory & Ordering Plane / Traffic Generator / 拓扑 Python 层)
- **THEN** 这些层的源码**不得**出现协议标识符(MESI / MOESI / CHI / CXL / TileLink / NVLink / UALink)
- **AND** 协议标识符**只**允许出现在 ProtocolEngine 插件内部与 capability schema 的**数据值**
- **AND** 既有两处违规(`coherence_domain.hh` 的 `enum Protocol` / 拓扑层的 `protocol=` 参数)**必须**被本禁令覆盖

#### Scenario: coherence_domains 字段保留但改语义(IC3)

- **WHEN** 契约测试检查 JSON 字段 `coherence_domains`
- **THEN** 该字段**必须保留**(4 个既有 config 依赖),且解析**不报错**
- **AND** 其 stub 告警文案**必须**指向 `ADR-CEP-13`(消费阶段)而非 `ADR-X.14`
- **AND** **消费必须**在 Stage 2 落地;Stage 0~1 期间标 `not-consumed`

---

**版本**: v1.0
**最后更新**: 2026-10-21
**维护者**: CppTLM Team(builder mode)
