# design: cpptlm-router-credit-flit-leak-to-consumer

> 配套 `proposal.md`(Why / What Changes / Acceptance / Capabilities / Out of Scope)。

## D-1 — 机理链(三段,逐段可验证)

| 段 | 事实 | 位置 |
|:--:|---|---|
| ① credit 被编成 flit,走**数据同一通道** | `fill_credit` 写 `flit_category = CATEGORY_CREDIT`(`dst_node`/`hops` 保持 0);`send_credit_impl` 经 `resp_out_port_` 发出,仅 `Packet::type = PKT_CREDIT_RETURN` 二次区分 | `include/framework/bidirectional_port_adapter.hh:126-139, 299-330` |
| ② router 对每个入向 flit 回 credit;`reverse_port(LOCAL) == LOCAL` | `return_credit_to_upstream(in_port, …)` 取 `reverse_port(in_port)` ⇒ **只有 NIC 自己注入的 flit** 换来落在 NIC 上的 credit | `src/tlm/router_tlm.cc:323/359/383`;`include/tlm/router_tlm.hh:223-232` |
| ③ 消费者不辨 credit ⇒ 三面污染 | 无条件计数 + 覆写 `last_recv_*`(0/0)+ 投影全零伪 Completion | `include/tlm/cep/cohif_nic.hh:260-272`, `:232-243` |

## D-2 — 判据设计

| 用例 | 判据 | 设计要点 |
|---|---|---|
| **MESH-7** | AC-01 | ⚠️ **先注入真 flit 再注入 credit** —— 否则「`last_recv_*` 不变」在从未收到过东西时是 `0 == 0`,自动通过(空断言)。真 flit 注入后先 `pe_resp_out().clear_valid()` 模拟下游消费 ⇒ 之后「是否被写入」才有判别力。credit flit 故意**复用同一 `transaction_id`** ⇒ 排除「因字段不同而蒙对」。 |
| **MESH-8** | AC-02 / AC-03 | 端到端跑**层次化**与**平铺**各一次 ⇒ 两侧 `credits_dropped() > 0`(过滤确实发生)+ `nic_b.last_recv_dst_node() == 3 && last_recv_hops() == 3`(观测不再被覆写)。⚠️ 这是**新增**断言(修复前该组合不可能成立)—— 现有测试 0 条 `last_recv_*` 断言,`grep -c last_recv_dst_node test/test_cep_mesh_e2e.cc` = 0 可复核。 |

## D-3 — 等价判据形态(per 台账 Entry #455 的 planner 裁决)

### D-3.1 — 事实:原判据在修复后破裂,且**并非**回归

builder 实测三组对照(临时诊断,已清理):

| 条件 | hier | flat | 相等? |
|---|---|---|---|
| 过滤**关**(= 修复前, `BUFFER_DEPTH=8`) | 15/8/7/15 | **15/8/7/15** | ✅ |
| 过滤**开**(修复后, `BUFFER_DEPTH=8`) | 15/8/7/15 | **16/8/8/16** | ❌ 差 1 |
| 过滤开 + `BUFFER_DEPTH=64`(消掉 LOCAL credit 上限) | 39/20/19/39 | **38/19/19/38** | ❌ 仍差 1 |

⇒ (a) 修复**只改变 flat**,**hier 与修复前基线逐项相同** ⇒ mesh 转发语义未变(router 拓扑与转发规则未动);
⇒ (b) 差异**不是** LOCAL credit 耗尽造成(第 3 行排除)⇒ 与另一 improvement 无关;
⇒ (c) 差异来自**闭环反馈回路**:伪 Completion 不再喂给业务侧 ⇒ flat 的下游响应流变化 ⇒ 注入流变化 ⇒ 转发数差 1(hier 的容器 tick 成组使其未受影响);
⇒ (d) 故原判据成立**部分依赖 credit 污染的巧合** ⇒ 属**判据脆弱**。

### D-3.2 — 裁决:等价性的**度量面**从「闭环稳态计数」换为「给定相同注入时的转发语义」

- **MESH-3a(开环等价 · 权威判据)**:把 `home.params.emit_completions = false`(`CohIfResponderTLM` 的**既有**开关,`configs/cep_mesh_e2e.json` 已用 `true`)⇒ **断开跨 mesh 回程** ⇒ mesh 上只剩单向流量 ⇒ 转发计数只由注入决定 ⇒ 层次化与平铺**逐项严格相等**。
  - ⛔ `==` **不得**放宽为 `> 0`;两条断言**不得**删除。
- **MESH-3b(闭环不变式)**:闭环下各自断言 `flits > 0` + `hops > flits`(确实多跳)+ 无静默丢弃 + 层次化内部 router 数 == 4。

### D-3.3 — 为什么这是**收紧**而非放宽

等价性要证明的是「**层次化不改变 mesh 转发行为**」。闭环下两侧的**输入流本身**就因反馈回路而不同(注入量 20 vs 19)⇒ 比较它们的**输出总量**在逻辑上不成立(输入不同,输出理应不同)。开环把输入固定为同一序列后,输出相等才是「转发语义未变」的准确命题。⇒ 判据被**换到正确的度量面上**,不是被削弱。

## D-4 — 实现选择

| 选择 | 取舍 |
|---|---|
| 判据用 **bundle 字段** `flit_category == CATEGORY_CREDIT`(非 `Packet::type`) | `Packet::type` 在 `InputStreamAdapter` 路径上**不一定可取**(模块只见 bundle);bundle 字段经 `bundle_serialization` 整结构 memcpy 完整传递 ⇒ 传输无关且可静态核对 |
| 过滤点放在 `unpack_flit` **之前** | 与既有 `unpack_flit` 的 `flit_category` 语义用法天然一致(`:232-243`),不新增第二套分类逻辑 |
| 范围界定为「裸 `InputStreamAdapter<NoCFlitBundle>` 的消费者」 | `CohIfBundle` 侧消费者已由 `BidirectionalPortAdapter` 的 `receive_credit` 拦截(`:255-274`)⇒ 本判据**不可**推广到 `CohIfBundle`(它无 `flit_category` / `vc_id`) |
| 丢弃**计数**而非静默 | 「过滤生效」与「未来新消费者忘过滤」均可观测(否则同类泄漏不可见) |

## D-5 — 已知限制(不在本 change 解决)

1. **LOCAL 口 credit 耗尽**:router 对 LOCAL 输出同样记账(`consume_credit`,`router_tlm.cc:307/367`),初值 `BUFFER_DEPTH = 8`,而终端消费者无归还通道 ⇒ 每 `(LOCAL, vc)` 8 个 flit 后本地投递停滞(`credit_timeout_ = 0` 无检测)。⇒ 另立 `.rddf/improvements/cpptlm-router-local-port-credit-exhaustion.md`(🔴 未立项 · 需 architect 决策)。
2. **闭环计数不等**:见 D-3.1,已知并已在 AC-06 登记。
