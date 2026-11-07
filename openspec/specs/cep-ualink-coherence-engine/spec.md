# cep-ualink-coherence-engine Specification

## Purpose
TBD - created by archiving change cpptlm-cep-ualink-coherence-engine. Update Purpose after archive.

## Requirements

### Requirement: cep-ualink-coherence-engine

`ualink_4` 的**专用承载引擎** SHALL 是**记账级专用承载**(bookkeeping-only dedicated carrier):其对 canonical 10 流的全部行为 SHALL 为**可观测的记账 / 拒绝 / pending 登记**,且 SHALL **零发射**(zero egress)、SHALL **零协议状态机**、SHALL **零 UPLI 通道级语义**(`ReqCmd` 编码 / 4 通道时序 / `transaction tag` 配对 **均不实现**)。

观测面 SHALL 为**三分法**,并与 `protocol_instances.json` 的 `flow_support` 声明**逐流对齐**:

| 状态 | 流 | 计数器 |
|---|---|---|
| **记账**(声明 `true`) | `request` · `completion` | `flow_events[10]`(**全部 10 流**计抵达) |
| **拒绝**(声明 `false`) | `snoop` · `writeback` · `forward_data` · `back_invalidate` · `address_resolve` · `barrier` · `snoop_completion_ack`(7 条) | `unsupported_rejected[10]`(**per-flow**) |
| **pending 不消费**(语义待定) | `snoop_resp`(1 条) | `snoop_resp_pending_not_consumed` |

⚠️ **SHALL NOT** 以「10 流一律记账」形态实现 —— 那会让观测面与 `flow_support` 声明矛盾(8 条声明 `false` 的流被正常记账)⇒ 复犯 `07 号` SU · 四 D-1 点名的「静默 … 且看起来权威」失败模式。

SHALL 满足以下 **4 条机器可判不变量**:

1. `unsupported_rejected[Request] == 0` ∧ `unsupported_rejected[Completion] == 0` —— 被声明支持的流**永不被拒**;
2. `∀f ∉ {request, completion, snoop_resp}: flow_events[f] == unsupported_rejected[f]` —— 声明不支持的流**每次抵达都被拒绝**,且**零其他可观测效果**;
3. `flow_events[SnoopResp] == snoop_resp_pending_not_consumed` ∧ `unsupported_rejected[SnoopResp] == 0` —— 每次抵达**恰好一次** pending 登记;
4. 正常运行路径 `completion_unmatched == 0`。

`completion` 的记账 SHALL **仅**按 `txn_id` **恒等匹配**;SHALL **NOT** 以到达次序作为匹配/记账依据;J1 的 `p2p_ordered` SHALL **NOT** 作为引擎行为前提(该假设属 channel dependency 判据面,非协议保证)。`txn_id` SHALL 为在途集合的**唯一键**(⛔ 不得以地址 / 到达序 / 任何派生量作键)。

`pending` SHALL 为**终态处置**(语义待定,**恒不消费**);⛔ **不得**读作「已支持、待完成 / 待处理」。每次 `snoop_resp` 抵达 SHALL 产生**恰好一次** pending 登记 + **零发射** + **零状态变更** + 对 `request` / `completion` 记账**零影响**。

⚠️ 引擎 SHALL **NOT** 为 `response` 消息类 / collective 结果回传路径新增任何 handler(= 锚点摘要 I-6 路径)⇒ 该路径的承载者仍是 `FabricCollectivePlane`。

⚠️ 本 requirement **不**改变 `ualink_4` 的 13 键声明(逐字不动),且 **不**改变 B3b 计数(**B3b-2 维持 0/4**);`test/cep_b3b2_deferrals.json` 的 4 族登记 SHALL 保持完整(登记 ≠ 达成)。

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
