# design —— `ualink_4` 的记账级专用承载引擎

> ⚠️ **强度标签(全文适用)**:本 change 的引擎 = **记账级**(bookkeeping-only)。⛔ 零发射 / ⛔ 零协议状态机 / ⛔ 零 UPLI 通道级语义 ⇒ ⛔ **不得**作为「UALink 协议语义已实现」的证据;⇒ **不计入 B3b**(per `07 号` SU · 四 D-1)。

## D-1 三分法观测面(不复用 M-6「一律记账」)

| 状态 | 流 | 计数器 |
|---|---|---|
| **记账**(声明 `true`) | `request` · `completion` | `flow_events[10]`(**全部 10 流**计抵达) |
| **拒绝**(声明 `false`) | `snoop` · `writeback` · `forward_data` · `back_invalidate` · `address_resolve` · `barrier` · `snoop_completion_ack`(7 条) | `unsupported_rejected[10]`(**per-flow**) |
| **pending 不消费**(语义待定) | `snoop_resp`(1 条) | `snoop_resp_pending_not_consumed` |

**为什么不用 M-6**:`MoesiGpuProtocolEngine` 的 M-6「10 流一律只记账」会让本引擎的观测面与 `flow_support` 声明**矛盾**(8 条声明 `false` 的流被正常记账)⇒ 复犯 `07 号` SU · 四 D-1 点名的「静默 … 且看起来权威」。**既有范式**:`MesifProtocolEngine::gate()`(「声明 false ⇒ 拒绝计数,不静默降级」)⇒ 三分法 = **与声明对齐,引入的语义维度为零**。

**4 条机器可判不变量**:
1. `unsupported_rejected[Request] == 0 ∧ unsupported_rejected[Completion] == 0`;
2. `∀f ∉ {request, completion, snoop_resp}: flow_events[f] == unsupported_rejected[f]`;
3. `flow_events[SnoopResp] == snoop_resp_pending_not_consumed` ∧ `unsupported_rejected[SnoopResp] == 0`;
4. 正常运行路径 `completion_unmatched == 0`。

## D-2 completion 不依赖有序性(判据须**有牙**)

⚠️ **判据退化警告**:若引擎是**纯计数器**,则「乱序投递 ⇒ 断言不变」**恒真**(整数自增可交换)⇒ **判据无牙**。
✅ **本 design 的形态**:引擎维护**最小 order-free 在途集合** —— `on_request` 按 `txn_id` **插入**,`on_completion` 按 `txn_id` **恒等擦除**。
**依据 = 平台自有相关性字段**:`dispatch_cohif` 把同一个 `b.transaction_id.read()` 传给两条流 ⇒ 恒等匹配是**协议中立**的**记账决定**,**零 UALink 宣称**。

⚠️ **「零 TxnID 配对」的边界**:**零 UPLI 协议层配对语义**(零 tag 配对规则 / 零 request↔response 配对协议 / 零超时 / 零重试 / 零排序 / 零资源门控);**平台自有 `transaction_id` 的恒等记账属平台层** ⇒ **允许**,且是判据有牙的**必要条件**。⛔ 文档禁用「TxnID 管理」(CHI 真插件词汇,CR-13)。

**鉴别力构造(两段)**:
- **中途探针**:request `t1,t2,t3` → 仅 completion **`t2`**(⚠️ **中间 id**,非最小 id)⇒ 在途集合 **== `{t1,t3}`}**(**逐格相等**)⇒ 恒等 `{1,3}` / FIFO-pop `{2,3}` / LIFO-pop `{1,2}` **三者互不相同** ⇒ **逐格相等**断言有鉴别力(若只断言「仅剩 1 个」则三者都过 ⇒ 无牙);
- **端态**:request `t1,t2,t3` → completion **反序** `t3,t1,t2` ⇒ 集合空 / `unmatched == 0` / 计数与正序**逐字段相等** ⇒ **order-coupling 类** mutant 必红;
- **决定性变异**:测试内构造「插入序回退且不比对 `txn_id`」的 mutant ⇒ 证明它在中途探针下**必红**(⇒ fixture 有牙)。

⚠️ **鉴别力边界(诚实登记)**:fixture **抓不到**「插入序回退**且带**恒等比对」与「以地址作键」两类 **order-oblivious** 逃逸 ⇒ 契约面 **⛔ 不得**声称覆盖键型变异;键型(`txn_id` 为唯一键)由**代码评审 + 头文件注释**保证。

## D-3 `snoop_resp` 的不对称(四句齐)

① **为何第三态**:声明给了它 `vc_binding`(VC3)却给 `flow_support = false`,其余 7 条流**无任何正向陈述** ⇒ per `ADR-CEP-12` 决策 6,`mapping ≠ support`;② **`pending` 是终态处置,不是暂态**(语义待定,**恒不消费**);③ **机器绑定**(恰好一次计数 + 零发射 + 零状态变更 + 对 request/completion 零影响);④ **计数器名自带否定语义**。

## D-4 命名与文档纪律

- 类名 / 注册短名 / 配置名 = `UalinkProtocolEngine` / `UalinkProtocolEngine` / `cep_ualink_engine_pe_minimal.json` ⇒ **不构成越界宣称**(`ProtocolEngineBase` 是「ChStream 端口面基类」;先例 `CohIfProtocolEngine` / `MoesiGpuProtocolEngine`);
- ⛔ 名字里**不得**出现语义载荷词(`Upli` / `Transaction` / `ReqRsp`);
- ✅ 注册块注释**必须**带**强度标签**(镜像 MoesiGpu 注册块写法);
- ⛔ 文档**禁用**「TxnID 管理」;⛔ 「弱档」**不得单独出现**(必须全称「公开非规范来源 + pending + fail-closed」)。

## D-5 文件面(最小化,per oracle Q4)

配置文件**不可省**(持久可 grep 的装配证据)· 注册**不可省**(无注册 = 无实例化路径,MGE-01/NIT-1 教训)· delta spec **不可省**(AC 的机器可判家)· ADR SU **不可省**(边界裁决记录)· `07 号` 行更新**不可省**(外部读者面)。
⛔ **不改**已归档 change 的 `configs/cep_ualink_pe_minimal.json`(Entry #258)⇒ 新配置更便宜。
