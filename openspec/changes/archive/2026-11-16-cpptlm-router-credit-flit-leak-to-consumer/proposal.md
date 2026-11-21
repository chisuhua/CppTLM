# change: cpptlm-router-credit-flit-leak-to-consumer

**主题**: 让 `CohIfNICTLM` 在网络 ingress **丢弃 in-band credit flit** —— 修复「credit 被当数据消费」导致的三面污染(计数虚增 / 观测被覆写 / 向业务侧灌全零伪 Completion),使 NIC 侧观测恢复可信。

> **上游**: verifier 验收 `cpptlm-simmodule-exposed-port-wiring` / `cpptlm-cep-mesh-interconnect-submodule` 时登记的 F-3(台账 Entry #452 初判 · **#453 根因订正** · **#454 立项 + metis 审查修订** · **#455 planner 裁决判据形态**)
> **决策源**: `docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md` 的 Status Update「真根因 = in-band credit 泄漏给非 router 消费者」
> **improvement**: `.rddf/improvements/cpptlm-router-credit-flit-leak-to-consumer.md`(5 段 + AC-01~AC-07)

---

## Why

### 第 1 条 —— credit 是**带内**信号,与数据 flit 共用同一通道。

`include/framework/bidirectional_port_adapter.hh:132-136` 的 `fill_credit(NoCFlitBundle&, vc)` 把 credit **编成一个 flit**(`flit_category = CATEGORY_CREDIT`、`flit_type = FLIT_HEAD_TAIL`、`dst_node`/`hops` 保持 0),并在 `:319-324` 经 **与数据同一** 的 `resp_out_port_` 发出(仅靠 `Packet::type = PKT_CREDIT_RETURN` 二次区分)。

### 第 2 条 —— router 对每个入向 flit 都回 credit,且 `reverse_port(LOCAL) == LOCAL`。

`src/tlm/router_tlm.cc:323/359/383` 的 `return_credit_to_upstream(in_port, …)` 取 `reverse_port(in_port)`(`include/tlm/router_tlm.hh:223-232`)⇒ **只有 NIC 自己注入的 flit** 会换来一个落在 NIC 上的 credit(经 LOCAL 进 ⇒ credit 回 LOCAL);被投递给 NIC 的响应,其 credit 回**上一跳 router**。

### 第 3 条 —— `CohIfNICTLM::handle_net_ingress` 不辨 credit ⇒ 三面污染。

`include/tlm/cep/cohif_nic.hh:260-272`:无条件 `++stats_flits_received_` + 覆写 `last_recv_dst_node_` / `last_recv_hops_`(被写成 0/0)+ `pe_resp_out_.write(unpack_flit(flit))`;而 `unpack_flit`(`:232-243`)把「非 REQUEST」一律投影为 Completion ⇒ **全零伪 Completion** 灌给业务侧。

### 第 4 条 —— 这解释了 mesh 验收中「NIC 侧计数时序敏感」的全部现象。

逐周期实测(层次化 `cep_mesh_e2e.json`,300 周期):真请求(`dst=3, hops=3`)与 credit(`dst=0, hops=0`)交替到达 ⇒ ① `flits_received` 虚增;② `last_recv_*` 取决于「最后到达的是数据还是 credit」⇒ **顺序敏感、跨装配方式不稳定**;③ `stub_completions_in` 被伪 Completion 混杂 ⇒ 只敢断言 `> 0`。**平铺基线同样存在**(非层次化引入)。

### 第 5 条 —— 已证伪的替代根因(勿再采用)。

曾判「`routing_table_` 以 `transaction_id` 单独为键、不含方向」——**证伪**:`pack_flit` 一律写 `FLIT_HEAD_TAIL`,而 `is_head()` 对 `HEAD_TAIL` 返回 true ⇒ `stage_route_computation` **恒走 HEAD 分支**(`computeRoute`),BODY/TAIL 的查表分支**不可达**。

---

## What Changes

1. **消费者侧过滤(唯一实施项)**:`CohIfNICTLM::handle_net_ingress` 在 ingress 处丢弃 `flit_category == CATEGORY_CREDIT` 的 flit —— **不**计入 `flits_received`、**不**写 `last_recv_*`、**不**向 PE 侧投影。
   - 判据用 **bundle 字段**(非 `Packet::type`):`NoCFlitBundle` 的 `flit_category` 在该路径上完整可读(字段 `noc_bundles_tlm.hh:45` → `fill_credit` 写入 → `bundle_serialization` 整结构 memcpy → `InputStreamAdapter::data()` 暴露)。
   - **范围界定**:仅「net ingress 为**裸** `InputStreamAdapter<NoCFlitBundle>` 的模块」需自带过滤 —— 当前仓内该类模块**仅 `CohIfNICTLM`**;`CohIfBundle` 侧消费者(`CohIfResponderTLM` / `StubCacheEndpointTLM` / ProtocolEngine 派生)由 `BidirectionalPortAdapter::tick()` 的 `receive_credit` 拦截(`bidirectional_port_adapter.hh:255-274`),**不受影响**。
2. **可观测性**:新增 `stats_credits_dropped_` + 访问器 `credits_dropped()`(照既有 `dropped_unmapped` 范式)⇒ 「过滤生效」与「未来新消费者忘过滤」均可观测。
3. **等价判据形态(per 台账 Entry #455 的 planner 裁决)**:MESH-3 拆为 **MESH-3a(开环等价·权威)** 与 **MESH-3b(闭环不变式)** —— 详见 `design.md` D-3。
4. **spec 条款**:`cep-cohif-network-adapter` 增「网络 ingress SHALL 丢弃 credit flit」Scenario + 观测面增 `credits_dropped_`(delta spec,归档时合并)。

---

## Acceptance

判据直接对应 improvement 的 AC-01 ~ AC-07(AC-05 / AC-06 已按 Entry #455 修订):

| # | 判据 | 可验收点（机器可判） |
|:--:|---|---|
| **AC-01** | credit flit 三面零变化 | 先注入 1 个真 flit(`dst=3, hops=2`)并断言已消费;再注入 1 个 credit flit ⇒ `flits_received()` 不变、`last_recv_dst_node/hops` 不变、`pe_resp_out` 无输出、`credits_dropped()` 增 1 |
| **AC-02** | 端到端过滤生效 | 层次化与平铺各跑一次 ⇒ 两侧 `credits_dropped() > 0`;且不再有 `hops == 0` 的 flit 计入「最后到达」 |
| **AC-03** | 观测稳定性 | `nic_b.last_recv_dst_node() == 3 && nic_b.last_recv_hops() == 3` 在**层次化与平铺两种装配下分别成立**(⚠️ 新增断言,非恢复既有) |
| **AC-04** | 回归守卫 | router 四元组与 change 内记录的基线逐项相等(零判别力,仅防顺手改 router) |
| **AC-05** | 等价判据形态 | **MESH-3a**:用 `home.params.emit_completions = false` 断开跨 mesh 回程得开环拓扑 ⇒ 层次化与平铺的 router `flits/packets/hops` **逐项严格相等**;**MESH-3b**:闭环下各自 `flits>0` + `hops>flits` + 无静默丢弃 + 层次化内部 router 数 == 4。⛔ MESH-3a 的 `==` **不得**放宽为 `> 0`、两条断言**不得**删除 |
| **AC-06** | 已知限制显式化 | ①长 run(> `BUFFER_DEPTH × NUM_VCS` = 32 flit/LOCAL 口)因 LOCAL credit 耗尽而停滞(既有缺陷,另一 improvement)⇒ 测试 run 明确在 32 flit 内;②**闭环**下层次化与平铺的 router 计数不再严格相等(实测差 1)⇒ 原因是伪 Completion 消除后的**闭环反馈**变化,非 mesh 转发语义变化 |
| **AC-07** | 伪 Completion 消失（**实施后收敛为可机械取证的两条**） | `home.counters().foreign_consumed == 0`;`w.net_completions_in <= home.counters().requests_consumed`。⚠️ 原「每条 `CohIf` 的 `transaction_id != 0`」不可机械取证(`consume()` 的 default 分支不记录 transaction_id)⇒ 由前两条机械覆盖 |

---

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|:--:|
| C-1 | 裸 `InputStreamAdapter` 消费者不再被 in-band credit 污染(计数 / 观测 / 业务流三面) | ❌ 0 |

---

## Out of Scope

- ⛔ **不改 `RouterTLM`**(不改 credit 回流语义、不改 LOCAL 口流控)—— 实测已证明 credit 耗尽**不能**解决等价判据问题,且该修复属**流控语义取舍**(归 architect)⇒ 另立 `cpptlm-router-local-port-credit-exhaustion`(🔴 未立项)。
- ⛔ 不改 `CohIfBundle`(协议面不变)、不改 `module_factory.cc`。
- ⛔ 不动 `include/tlm/cep/cohif_nic.hh` 以外的 CEP 模块(其余消费者已由适配器 `receive_credit` 拦截)。
- ⛔ 不改 `RouterTLM::routing_table_` 的键(该方向的怀疑**已证伪**)。
