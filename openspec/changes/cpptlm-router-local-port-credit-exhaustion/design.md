# design: cpptlm-router-local-port-credit-exhaustion

> 配套 `proposal.md`（Why / What Changes / Acceptance / Capabilities / Out of Scope）。
> 决策源：`docs/adr/ADR-X.16-router-local-port-terminal-credit.md`（Accepted）。

## D-1 — 实现形态（方案 A）

| 落点 | 改动 |
|---|---|
| `src/tlm/router_tlm.cc` · `has_credit(out_port, vc)` | `out_port == LOCAL` ⇒ 恒真（**在 `vc >= NUM_VCS` 边界检查之后** ⇒ 非法 VC 不被短路放行） |
| `src/tlm/router_tlm.cc` · `consume_credit(out_port, vc)` | `out_port == LOCAL` ⇒ 不扣减 |
| `src/tlm/router_tlm.cc` · `downstream_credits_[LOCAL][*]` | 死状态处置**二选一**：(a) 注释说明不再维护，或 (b) 从记账循环跳过 |
| `src/tlm/router_tlm.cc` · VA / SA 拒绝分支（`:226-229` / `:266`） | stall 周期计数 +1（锚点见 D-3） |
| `include/tlm/router_tlm.hh` · `stat_group_` | 增标量 `credit_stall_cycles`（跨 `(port, vc)` 累计，与 `flits_forwarded` 同面） |
| `include/tlm/router_tlm.hh` · `RouterPort::LOCAL` | 注释改为**显式声明终端端口 + 不参与 credit 流控**（现注释仅「本地（连接 NICTLM）」） |

**零配置面变更**：不动 JSON schema、不动 `ModuleFactory`、不动任何 `configs/*.json`。

## D-2 — 测试装置与观测面（metis 写死，AC-01 / AC-02 通用）

| 项 | 规定 | 依据 |
|---|---|---|
| **装置** | **裸 `RouterTLM`（`adapter_ == nullptr`）+ 直接向 `req_in[4]` 注入**，`dst_node` = 本节点 ⇒ `computeRoute` 选 `out_port = LOCAL` | 复用 `test/test_noc_integration.cc:14-38` 的自环手法；**不需要**新拓扑、**不需要**「来自 mesh 另一侧」的注入源、**不需要**新 sink 模块 |
| **注入节拍** | 全局 **1 flit/cycle** | 否则输入 buffer 满 ⇒ `router_tlm.cc:149-155` **静默丢弃** flit ⇒ 得到与 D1 无关的假失败 |
| **观测** | 每周期读 `resp_out[4]` 后**立即 `clear_valid()`**，以「**读到的 flit 数**」计 | ⛔ 不可用「valid 为真的周期数」—— `OutputStreamAdapter::write()` 不清 valid（`stream_adapter.hh:111-114`）⇒ 恒为真且每周期覆盖写 |
| **辅助量** | `router.stats()` 的 `flits_forwarded` 仅作辅助 | 它在**入 `pending_link_` 时** +1，而 `pending_link_` 是**全局单队列、每周期只 drain 1 个**（`:400-404`）⇒ 领先真实 egress |
| **周期预算** | 按**全局 egress 1 flit/cycle** 放大：40 flit 用例 **≥ 120 cycles**；80 flit 用例 **≥ 200 cycles** | 含 pipeline 填充与队列积压 |
| **事务 id** | 每个 packet **唯一 `transaction_id`** | `routing_table_` 以其为键；复用会导致 TAIL 擦除后查不到路由 |

**全拓扑口径**（`CohIfNICTLM` 作下游）作为**第二条独立用例**：此时 `credits_dropped() > 0` 属正常（入向 credit 仍回流），**不得**断言其为 0。

## D-3 — stall 计数的锚点订正（ADR D4 原文 → 实施口径）

| 版本 | 锚点 | 后果 |
|---|---|---|
| **ADR-X.16 D4 原文** | 「该 `(port, vc)` 处于 `waiting_for_credit` 每周期 +1」 | ❌ **恒为 0** —— `waiting_for_credit` 只在 ST 阶段「SA 已放行、ST 复查发现 credit 为 0」时置位（`:352-361`）；而 credit 归零后 VA 阶段先 `!has_credit` 即 `release_vc` 并跳过（`:226-229`）、SA 阶段直接不选 winner（`:266`）⇒ flit 停在 `input_buffer_`，该位**保持 false** |
| **实施口径（metis 订正）** | 「**存在待发 flit 且 `has_credit(out_port, out_vc) == false`**」的周期累计（即 VA / SA 的**拒绝条件**） | ✅ 与真实停滞状态重合 |

⇒ ADR 文本**不改**（不可变原则）；由 **architect** 在 `ADR-X.16` 追加 Status Update 澄清锚点（**planner 无权改 ADR 决策**）。

**反向判据**（防「每周期无脑 +1」的假实现）：挂上 `BidirectionalPortAdapter`（或双 router 互联）后，同一计数**停止增长**。

## D-4 — 数值变化的处理纪律（AC-04b）

D1 会改变**既有** `LOCAL` 口流量大的用例的接收数（不再停滞 ⇒ 收得更多）。处理纪律：

1. 变化**必须**在 `design.md` / tasks 里逐条列出「**旧值 → 新值 → 为何是 D1 的必然结果**」；
2. ⛔ **禁止就地改 `REQUIRE` 常量**（禁止静默重基线）；
3. 「router `flits`/`packets`/`hops` 逐项不变」的绝对断言**只适用于 `LOCAL` 未饱和的用例**（否则与第 1 条冲突）。

## D-5 — spec 落点与真源同步

| 动作 | 内容 |
|---|---|
| **新建** capability | `openspec/specs/router-local-port-flow-control/` —— 两条 Requirement：`router-local-port-terminal-semantics`（D1 + 端口注释 + 死状态处置）/ `router-mesh-credit-stall-observability`（D4 + 锚点口径 + LOCAL 不误报 + 不靠重置掩盖） |
| **修订** 既有 spec | `openspec/specs/cep-cohif-network-adapter/spec.md` 的「已知限制」句 —— 「`LOCAL` 口 credit 耗尽属**另一**缺陷」在 D1 落地后**变成过期陈述** ⇒ 改为「已由 `router-local-port-flow-control` 承接并解决」，并保留「本模块仍不归还 credit ⇒ ingress 仍丢弃 credit flit」 |
| **对齐** 另一 spec | `openspec/specs/port-type-system/spec.md` 里 `LOCAL` 的 `role = BI_DIRECTIONAL` **不变**（本决策只终止 credit 记账语义，不改端口角色）⇒ 新 spec 显式声明，避免两处口径无人对齐 |

## D-6 — 已知限制（不在本 change 解决）

1. **LOCAL↔router 误用不可见**：D1 后 LOCAL 永不进入 stall 计数 ⇒ 该误用的真实失败模式是**下游 router 的 `input_buffer` 满后静默丢 flit**（`router_tlm.cc:152-154`）⇒ 唯一防线是端口注释（V-6）+ ADR D5 的演进决策。
2. **沉默边界迁移**：D1 之前 8-flit 停滞客观上把 router→终端节流在终端最低消费速率；D1 之后若某终端消费 < 1 flit/cycle，丢包点会移到**终端 ingress**（`process_net_response_input` 静默丢包且不 release packet，`dual_port_stream_adapter.hh:237-243`）。现行 LOCAL 终端均 1/cycle 消费 ⇒ 无当前回归。
3. **范围外三项**：`buffer_size` param 不生效 · `NICTLM` 缺 credit flit 过滤 · `vc_count` param 语义 ⇒ 须另行立项。
