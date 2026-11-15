# cep-ualink-coherence-engine Specification

## Purpose
为 `ualink_4` 提供**记账级**承载:只观测流抵达、拒绝与 pending,不实现 UPLI 通道级语义 ⇒ 使该实例在探索平台上「可被驱动、可被计数」,同时明确它**不承载协议语义**(避免把记账读成实现)。

## Requirements

### Requirement: cep-ualink-coherence-engine
`ualink_4` 的**专用承载引擎** SHALL 是**记账级专用承载**:对 canonical 10 流的全部行为 SHALL 限于**记账 / 拒绝 / pending 登记**,且 SHALL **零发射**、**零协议状态机**、**零 UPLI 通道级语义**(`UGE-I`)。

观测面 SHALL 为**三分法**并与 `flow_support` **逐流对齐**,满足 4 条不变量(`UGE-A`);⛔ SHALL NOT 以「10 流一律记账」实现。

`completion` SHALL **仅**按 `txn_id` 恒等匹配,SHALL NOT 以到达次序为据;`txn_id` SHALL 为在途**唯一键**(`UGE-F`)。

`pending` SHALL 为**终态处置**(`UGE-C`)。⛔ SHALL NOT 为 `response` / collective 回传新增 handler(`UGE-G`);SHALL NOT 改变 **13 键**声明,SHALL 保持 4 族登记完整(`UGE-H`)。

#### Scenario: UGE-A —— 三分法观测面与 4 条不变量

- **WHEN** 驱动全部 canonical 10 流(5 条经 `dispatch_cohif` + 5 条直调 `on_*`)
- **THEN** `flow_events` SHALL 对**全部 10 流**各计 1 次抵达
- **AND** 7 条拒绝流 SHALL 各满足 `flow_events[f] == unsupported_rejected[f] == 1`
- **AND** `unsupported_rejected[Request] == 0` ∧ `unsupported_rejected[Completion] == 0` ∧ `unsupported_rejected[SnoopResp] == 0`
- **AND** `snoop_resp_pending_not_consumed == 1`
- **AND** `egress_emitted == 0` 且全部端口 `resp_out()` SHALL 无 valid bundle(**零发射**)

#### Scenario: UGE-B —— completion 记账不依赖有序性(判据须**有牙**)

- **WHEN** request `t1,t2,t3` → 仅 completion **`t2`**(中间 id)⇒ 断言在途集合 **== `{t1,t3}`}**(逐格相等)
- **THEN** 恒等匹配 SHALL 擦除 `txn_id == 2`;**且** 该断言 SHALL 对「插入序回退」(FIFO-pop)与「最新序擦除」(LIFO-pop)两类 mutant **必红**(恒等 ⇒ `{1,3}`;FIFO ⇒ `{2,3}`;LIFO ⇒ `{1,2}` ⇒ 三者互不相同)
- **AND** WHEN request `t1,t2,t3` → completion **反序** `t3,t1,t2` ⇒ 在途集合 SHALL **空** ∧ `completion_unmatched == 0` ∧ **全部**计数器 SHALL 与正序运行**逐字段相等**
- **AND** ⚠️ 本 scenario **SHALL NOT** 被读作覆盖「插入序回退**且带**恒等比对」与「以地址作键」两类 order-oblivious 逃逸 ⇒ 键型 SHALL 由**代码评审 + 头文件注释**保证

#### Scenario: UGE-C —— `snoop_resp` 的 pending 终态

- **WHEN** 在途集合非空时驱动一次 `snoop_resp`
- **THEN** `snoop_resp_pending_not_consumed` SHALL **恰增 1** ∧ `flow_events[SnoopResp]` SHALL **恰增 1**
- **AND** `unsupported_rejected[SnoopResp]` SHALL **不增**(它是 pending,**不是** rejected)
- **AND** 在途集合 SHALL **逐格不变**(**恒不消费**)∧ 对 `request` / `completion` 的记账 SHALL **零影响**
- **AND** `egress_emitted` SHALL **不增**

#### Scenario: UGE-D —— 装配闭环

- **WHEN** 对 `configs/cep_ualink_engine_pe_minimal.json` 跑装配闸,`required_plane_instances = {"ualink_4"}`
- **THEN** 闸 SHALL 判 **PASS** ∧ mounted SHALL 含 `ualink_4`
- **AND** WHEN 把 `ualink_4` 的 plane 绑定**置空** ⇒ 闸 SHALL **按 `[IAG-04]` 归因**判红
- **AND** WHEN 把 plane 名改成**未登记**的值 ⇒ 闸 SHALL **按 `[IAG-05]` 归因**判红

#### Scenario: UGE-E —— B3b 计数不变

- **WHEN** 检查 `test/cep_b3b2_deferrals.json` 与 `protocol_instances.json`
- **THEN** 4 族 deferral 登记 SHALL **完整**且 `ualink` 的 disposition SHALL 仍为 `deferred`
- **AND** `ualink_4` 的 `flow_support` 8 条 `false` + 2 条 `true` SHALL **逐字未变** ∧ `perm` SHALL 仍为 `["NONE"]`
- **AND** `ualink-pending` 的来源档位 SHALL 仍为 `public_nonnormative`(⛔ 不升格为 `spec_clause`)⇒ **不计入 B3b**

#### Scenario: UGE-F —— `txn_id` 为在途集合唯一键,且不假设有序性

- **WHEN** 检查引擎的在途集合键型与有序性假设
- **THEN** 键 SHALL **仅为** `txn_id`(**恒等**)
- **AND** SHALL NOT 以地址 / 到达序 / 任何派生量作键
- **AND** J1 的 `p2p_ordered` SHALL **NOT** 作为引擎行为前提(该假设属 channel dependency 判据面,非协议保证)
- **AND** ⚠️ `UGE-B` 只验「插入序回退 / 最新序擦除」两类 mutant 的擦除结果,**不**覆盖键型选择 ⇒ 键型 SHALL 由本 Scenario + 头文件注释共同钉死

#### Scenario: UGE-G —— ⛔ 不为 `response` 类与 collective 回传路径新增 handler

- **WHEN** 检查引擎的 handler 注册面
- **THEN** `response` 消息类 SHALL **无** handler
- **AND** collective 结果回传路径 SHALL **无** handler(承载者 = `FabricCollectivePlane`,锚点摘要 I-6)
- **AND** 引擎的 `flow_support` 中 `response` SHALL 保持声明值不变(⛔ 不得因新增 handler 而需改动声明)

#### Scenario: UGE-H —— `ualink_4` 的 13 键与 B3b 登记逐字不动

- **WHEN** 对比 `ualink_4` 在 `protocol_instances.json` 的 **13 键**声明
- **THEN** 全部 13 键 SHALL **逐字未变**(⛔ 不限于 `flow_support` 与 `perm`)
- **AND** `test/cep_b3b2_deferrals.json` 的 **4 族**登记 SHALL 保持完整,`ualink` 的 disposition SHALL 仍为 `deferred`
- **AND** **B3b-2 SHALL 维持 0/4**(登记 ≠ 达成)

#### Scenario: UGE-I —— **零 UPLI 通道级语义**的操作定义

- **WHEN** 检查引擎是否实现了 UPLI 通道级语义
- **THEN** `ReqCmd` 编码 SHALL **不实现**
- **AND** 4 通道时序 SHALL **不实现**
- **AND** `transaction tag` 配对 SHALL **不实现**
- **AND** 上述三项的缺失 SHALL **不**被读作「未完成」—— 它们是**记账级承载**的设计边界,不是缺陷
