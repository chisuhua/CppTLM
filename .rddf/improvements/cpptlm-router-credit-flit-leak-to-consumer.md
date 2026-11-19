---
name: cpptlm-router-credit-flit-leak-to-consumer
priority: P1
phase: phase-10
category: bugfix
type: bug
状态: ✅ **已完成并归档**（2026-11-16 立项 #454 → 实施 #456 → 归档 **#457**;✅ **已完成归档语义** · delta 已合并进 `openspec/specs/cep-cohif-network-adapter/`;归档件 `openspec/changes/archive/2026-11-16-cpptlm-router-credit-flit-leak-to-consumer/`，⚠️ 该目录被 `.gitignore` 忽略 ⇒ 持久记录以台账 Entry #457 为准;经 metis 审查修订:根因订正 + scope 收敛为消费者侧过滤）
created: 2026-11-16
revision_count: 1
last_feedback_id: feedback-20261116-001
last_feedback_at: '2026-11-16T11:55:29+00:00'
feedback_status: needs-revision
---

# improvement: cpptlm-router-credit-flit-leak-to-consumer

**主题**：`RouterTLM` 的 **credit 回流以「flit」形式**走在 `resp_out` 通道上（靠 bundle 的 `flit_category == CATEGORY_CREDIT` + `Packet::type == PKT_CREDIT_RETURN` 双标记）。当 `resp_out` 的下游是**裸 `InputStreamAdapter<NoCFlitBundle>` 的消费者**（今天恰好只有 `CohIfNICTLM`）时，消费者不辨 credit ⇒ **计数虚增**、**观测计数器被覆写为 0**、并向业务侧灌入**全零伪 Completion**。

> **上游**：`include/framework/bidirectional_port_adapter.hh:126-139`（`NoCFlitBundle` 的 `fill_credit`）/ `:299-330`（`send_credit` / `send_credit_impl`）、`src/tlm/router_tlm.cc:321-330,357-390`（`return_credit_to_upstream`）、`include/tlm/router_tlm.hh:223-232`（`reverse_port`）、`include/tlm/cep/cohif_nic.hh:260-272`（`handle_net_ingress`）
> **发现场景**：层次化 CEP mesh 验收（`docs/adr/ADR-X.15` 期间）

---

## Why

### 第 1 条 — credit 是**带内**（in-band）信号，与数据 flit 共用同一通道

```cpp
// bidirectional_port_adapter.hh:132-136（NoCFlitBundle 特化）
static void fill_credit(arg_t& b, unsigned vc) {
    b.vc_id.write(vc);
    b.flit_category.write(NoCFlitBundle::CATEGORY_CREDIT);   // ← bundle 级标记（值 2）
    b.flit_type.write(NoCFlitBundle::FLIT_HEAD_TAIL);
}
// :321-324 —— 与数据 flit **同一个** resp_out_port_ 发出
pkt->type = PKT_CREDIT_RETURN;                                // ← Packet 级标记
```

⚠️ `dst_node` / `hops` 保持值初始化的 **0**。

### 第 2 条 — credit 回 `reverse_port(in_port)`：只有 NIC **自己注入**的 flit 会换来落在 NIC 上的 credit

`return_credit_to_upstream(waiting_flit.in_port, …)` 在 router_tlm.cc:323 / 359 / 383 三处调用，取 `reverse_port(in_port)`（router_tlm.hh:223-232）：

| 场景 | in_port | credit 回到 | 是否落在 NIC |
|---|---|---|---|
| NIC 注入的请求进 mesh | LOCAL(4) | `reverse_port(4) = 4` = LOCAL | **是** |
| mesh 响应被投递给 NIC | WEST/EAST/… | 上一跳 router | 否 |

⇒ **NIC 侧的 credit 数 == 该 NIC 自己注入的 flit 数**。
（⚠️ 此处曾误写为「每收到 1 个真请求就紧跟着 1 个 credit」—— 由 metis 审查订正；实测数据与此一致：`nic_b` 的 `flits_received` 在它开始注入 completion 后才跳增。）

### 第 3 条 — `CohIfNICTLM::handle_net_ingress` 不辨 credit（缺陷点）

```cpp
inline void CohIfNICTLM::handle_net_ingress() {          // cohif_nic.hh:260-272
    if (!net_resp_in_.valid() || !net_resp_in_.ready()) return;
    const Flit flit = net_resp_in_.data();
    net_resp_in_.consume();
    ++stats_flits_received_;                     // ① credit 也算「收到」
    last_recv_dst_node_ = flit.dst_node.read();  // ② 被覆写成 0
    last_recv_hops_    = flit.hops.read();       // ② 被覆写成 0
    pe_resp_out_.write(unpack_flit(flit));       // ③ credit → 全零伪 Completion
}
```

`unpack_flit`（`cohif_nic.hh:232-243`）按「`flit_category != CATEGORY_REQUEST` ⇒ Completion」投影 ⇒ credit 被投影成事务号/地址/数据全 0 的 Completion 灌给业务侧。

### 第 4 条 — 实测证据（逐周期，层次化 `cep_mesh_e2e.json`，300 周期）

```
cyc=13 | nic_b recv=1 last(dst=3 hops=3)   ← 真请求穿网 3 跳
cyc=18 | nic_b recv=4 last(dst=0 hops=0)   ← credit 批量到达（覆写观测）
cyc=30 | nic_b recv=6 last(dst=3 hops=3)
...        （dst=3/hops=3 与 dst=0/hops=0 交替）
```

⇒ 解释了 `flits_received` 虚增、`last_recv_*` **顺序敏感**、以及 `stub_completions_in` 只敢断言 `> 0`。**平铺基线同样存在**此污染（非层次化引入）。

### 第 5 条 — 已证伪的替代根因（勿再采用）

曾判「`routing_table_` 以 `transaction_id` 单独为键 ⇒ 请求/响应同 tx id 时响应命中残留 `out_port=LOCAL`」——**证伪**：`pack_flit` 对每个 flit 写 `flit_type = FLIT_HEAD_TAIL`，而 `is_head()` 对 `HEAD_TAIL` 返回 true ⇒ `stage_route_computation` **永远走 HEAD 分支（`computeRoute`）**，BODY/TAIL 的查表分支不可达 ⇒ 该碰撞在本场景不可能发生。

### 第 6 条 — **同源缺陷的另一面**（本 change 不含，已另立）

`LOCAL` 输出同样 `consume_credit`（`router_tlm.cc:307,367`），初值 `BUFFER_DEPTH = 8`（`router_tlm.hh:134`），而终端消费者**无归还通道** ⇒ 每 `(LOCAL, vc)` 满 8 个 flit 后本地投递**静默停滞**（`credit_timeout_` 默认 0 ⇒ 无检测）。⇒ 已另立 improvement **`.rddf/improvements/cpptlm-router-local-port-credit-exhaustion.md`**（🔴 未立项 · 需 architect 决策，因触及 `RouterTLM` 流控语义）。

⚠️ **因此本 change 不声称「长 run 完全等价」**——见 AC-06。

---

## What Changes

1. **消费者侧过滤（唯一实施项）**：
   **net ingress 为裸 `InputStreamAdapter<NoCFlitBundle>` 的模块** MUST 在 ingress 处丢弃 credit flit —— 判据 `flit.flit_category.read() == Flit::CATEGORY_CREDIT`（bundle 字段，已核实在该路径上可读，见「边界」）。
   丢弃**不得**计入 `flits_received` / **不得**写 `last_recv_*` / **不得**转发到业务侧。
   - 当前仓内该类模块**仅 `CohIfNICTLM`**（出现在 `configs/cep_mesh_e2e.json` 与 `test/test_cep_mesh_e2e.cc` 的 `FLAT_BASELINE`）。
   - `CohIfBundle` 侧消费者（`CohIfResponderTLM` / `StubCacheEndpointTLM` / ProtocolEngine 派生）不受影响：它们走 `BidirectionalPortAdapter`，其 `tick()` 已把 `isCredit()` 的包截走并调 `receive_credit()`（`bidirectional_port_adapter.hh:255-274`）。
   - **新增此类模块时的检查点**：若模块的 net ingress 是裸 `InputStreamAdapter<NoCFlitBundle>`，必须自带 credit 过滤（这条写进 spec 与头注释）。

2. **可观测性**：`CohIfNICTLM` 增 `stats_credits_dropped_` + 访问器 `credits_dropped()`（照既有 `dropped_unmapped_` 范式），使「过滤生效」与「未来新消费者忘过滤」均可观测。

3. **单测**：注入真 flit 后再注入 credit ⇒ 计数/观测/业务侧三面零变化、`credits_dropped()` 递增。

> **明确不做**：`RouterTLM` 侧改动（LOCAL 不回 credit / per-port `credit_aware` 声明）—— 那是另一个 improvement，且需 architect 决策。本 change 的源码改动**限于 `include/tlm/cep/cohif_nic.hh` + 新测试 + spec 条款**。

## Acceptance

| # | 判据（机器可判） |
|:--:|---|
| **AC-01** | **单测**：先注入 1 个真 flit（`dst_node=3, hops=2, flit_category=CATEGORY_REQUEST`）并断言已被消费（`last_recv_dst_node()==3 && last_recv_hops()==2`、`flits_received()` 增 1）；**再**注入 1 个 credit flit（同 `serialize_bundle` 路径构造）⇒ `flits_received()` **不变**、`last_recv_*` 仍 `3/2`、`!pe_resp_out().valid()`（无输出）、`credits_dropped()` 增 1 |
| **AC-02** | **端到端**（`cep_mesh_e2e` 层次化 + 平铺各跑一次）：`credits_dropped() > 0`（过滤确实发生）**且** 不再有 `hops == 0` 的 flit 计入 `flits_received`（判据：注入过真 flit 后 `last_recv_hops() > 0`）**且** `nic_*.flits_received() <= 该 NIC 侧 router LOCAL 口的实际投递上限`（不再虚增；实测值记入 change） |
| **AC-03** | **观测稳定性**：`nic_b.last_recv_dst_node() == 3 && nic_b.last_recv_hops() == 3`（前提：流量单向 `tg → w → nic_a → node3`；`nic_b` 的 PE 侧只回 Completion、不发起自身事务）。**层次化与平铺两种装配下分别断言**。⚠️ 这是**新增**断言（现有测试 0 条 `last_recv_*` 断言，`grep -c last_recv_dst_node test/test_cep_mesh_e2e.cc` = 0 可复核），不是「恢复既有断言」 |
| **AC-04** | **回归守卫**（零判别力，仅防实施者顺手改 router）：router 四元组 `flits / packets / hops / stats_flits_forwarded` 与 change 内记录的基线逐项相等 |
| **AC-05** | **等价判据形态（2026-11-16 builder 实测后由 planner 裁决修订）**：原「MESH-3 单条闭环 router 计数相等」判据被证明**脆弱且部分依赖 credit 污染**（见下「修订依据」），拆为两条 —— **MESH-3a（开环等价·权威判据）**：用 `home.params.emit_completions = false` **断开跨 mesh 回程**得开环拓扑，断言层次化与平铺的 router `flits/packets/hops` **逐项严格相等**；**MESH-3b（闭环不变式）**：闭环下各自断言 `flits>0` + `hops>flits` + 无静默丢弃 + 层次化内部 router 数 == 4。**禁止**把 MESH-3a 的 `==` 放宽为 `> 0`、**禁止**删除 MESH-3a/MESH-3b 任一断言、**禁止**以「闭环计数不等」为由回退本 change 的过滤实现。其余重基线仍受控：只允许重基线 `hier.nic_b_recv == flat.nic_b_recv`(:291)、`nic_b_recv > 0`(:292)、`stub_completions_in > 0`(:304)、`nic_a_recv > 0`(:305) 四处，须逐条列出「旧值 → 新值 + 推导依据」；**禁止**把 `==` 放宽为 `> 0`、**禁止**删除断言、**禁止**改 router 级期望（**MESH-3a 除外 —— 其形态变更即本条所授权**）。另须同步订正 `test/test_cep_mesh_e2e.cc:17-21` 的注释（其「NIC 计数时序敏感」论证在过滤后只剩一半成立） |
| **AC-06** | **已知限制显式化**：①长 run（> `BUFFER_DEPTH × NUM_VCS` = 32 个 flit / LOCAL 口）会因 LOCAL credit 耗尽而停滞（既有缺陷，见 `.rddf/improvements/cpptlm-router-local-port-credit-exhaustion.md`）⇒ 测试 run 须**明确**在 32 个 flit 内，且 change 的 design.md 与 spec 条款须写明「本 change 只解 credit 污染，不解 credit 耗尽」；②**闭环**下层次化与平铺的 router 计数**不再严格相等**（实测差 1）—— 原因是伪 Completion 消除后**闭环反馈回路**的行为变化（builder 三组对照实验见台账 Entry #455），**不是** mesh 转发语义变化（hier 侧与修复前基线逐项相同、router 拓扑与转发规则未动）⇒ 等价性的权威判据是 **MESH-3a 的开环逐项相等** |
| **AC-07** | **伪 Completion 消失（2026-11-16 实施后收敛为可机械取证的两条）**：① `home.counters().foreign_consumed == 0`（home 收到 credit 投影出的全零 Completion ⇒ 走 `CohIfResponderTLM::consume()` 的 default 分支 ⇒ 计数 > 0）；② `w.net_completions_in <= home.counters().requests_consumed`（伪 Completion 只经 `nic_a` 的 PE 侧流进 `w` ⇒ 修复前必然超出）。⚠️ 原第三条「到达 PE 侧的每条 `CohIf` 其 `transaction_id != 0`」**不可机械取证** —— `consume()` 的 default 分支不记录 transaction_id 且不暴露逐条观测 ⇒ 该条由 ①② **机械覆盖**（伪 Completion 的**唯一**来源是 credit ⇒ 无伪 Completion 到达 PE 侧 ⟺ 无全零 Completion）。**判别力对照实测**：修复前 `foreign_consumed = 7`、`net_completions_in = 11~12 > requests_consumed = 7`（必红）；修复后 `0`、`5~7 <= 7~8`（绿）。 |

## 📌 修订依据（2026-11-16 · builder 实测 · planner 裁决）

builder 在实施中发现：**原 AC-04 的「router 四元组不变」与「不改 `RouterTLM`」的 scope 决策，在实测下不可同时满足** —— MESH-3 的闭环 router 计数判据由绿转红。三组对照实验（`test/test_cep_mesh_e2e.cc` 临时诊断，已清理）：

| 条件 | hier | flat | 相等? |
|---|---|---|---|
| 过滤**关**（= 修复前, `BUFFER_DEPTH=8`） | 15/8/7/15 | **15/8/7/15** | ✅ |
| 过滤**开**（修复后, `BUFFER_DEPTH=8`） | 15/8/7/15 | **16/8/8/16** | ❌ 差 1 |
| 过滤开 + `BUFFER_DEPTH=64`（消掉 LOCAL credit 上限） | 39/20/19/39 | **38/19/19/38** | ❌ 仍差 1 |

⇒ (a) 修复**只改变 flat**，hier 与修复前基线**逐项相同** ⇒ mesh 转发语义未变；(b) 差异**不是** credit 耗尽造成（第 3 行排除）；(c) 差异来自**闭环反馈回路** —— 伪 Completion 不再喂给业务侧 ⇒ flat 的下游响应流变化 ⇒ 注入流变化 ⇒ 转发数差 1（hier 的 tick 成组使其未受影响）；(d) 故原判据成立**部分依赖 credit 污染的巧合**，属**判据脆弱**而非回归。

**裁决**：等价性判据从「闭环稳态计数相等」改为「**开环转发语义相等**」（MESH-3a，`emit_completions=false` 断开回程）+「闭环不变式」（MESH-3b）。**不**放宽为 `> 0`，**不**回退过滤实现。

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|:--:|
| C-1 | 裸 `InputStreamAdapter` 消费者不再被 in-band credit 污染（计数 / 观测 / 业务流三面） | ❌ 0 |

## Impact

- **改动（源码）**：`include/tlm/cep/cohif_nic.hh`（`handle_net_ingress` 加 credit 过滤 + `stats_credits_dropped_`/访问器）+ 新单测（建议 `test/test_cep_mesh_e2e.cc` 内新增 `MESH-7`，或独立 `test_cep_cohif_nic_credit_filter.cc`）。
- **改动（spec）**：`openspec/specs/cep-cohif-network-adapter/spec.md:52` 现有条款「本模块 SHALL **NOT** 承载协议语义(credit / retry / ordering point / …)」**加从句**而非新增并列条款：
  「… ⇒ 本模块 SHALL **NOT** 解释/转发/记账 credit；SHALL 在 net ingress **丢弃** `flit_category == CATEGORY_CREDIT` 的 flit（不计入 `flits_received`，不写 `last_recv_*`，不转发 PE 侧）」。
  （避免与既有条款自相矛盾而被 `scripts/test/cep_*` 闸检出。）
- **⛔ 不动**：`src/tlm/router_tlm.cc` / `include/tlm/router_tlm.hh`（→ 另一个 improvement）、`module_factory.cc`、`CohIfBundle`（协议面不变）。
- **回归面**：`[cep]` / `[e2e]` / mesh。AC-05 已限定可重基线范围。
- **计数**：`INV-CEP-*` 新增 **0**。

## ⚠️ 关联

Entry **#452**（F-3 初判，根因有误）· **#453**（根因订正）· **#454**（本 change 立项 + metis 审查修订 + credit 耗尽面分离）· `.rddf/improvements/cpptlm-router-local-port-credit-exhaustion.md`（同源另一面）· `.rddf/reviews/cpptlm-simmodule-exposed-port-wiring-verify.md`（F-3）· `.rddf/reviews/cpptlm-cep-mesh-interconnect-submodule-verify.md`（F-3）· `docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md`（Status Update 的「发现 2」仍写旧根因，待 architect 订正）· `openspec/specs/cep-cohif-network-adapter/spec.md`

## Feedback

### feedback-20261116-001

- **source**: rdd-builder
- **kind**: needs-revision
- **created_at**: 2026-11-16T11:55:29+00:00
- **resolution**: open

#### Body

① kind: needs-revision（plan_drift）—— 需改 MESH-3 的判据形态（router 级期望），AC-05 明令禁止 builder 自行修改。

② 阻塞点: test/test_cep_mesh_e2e.cc:293 `REQUIRE(hier.routers[i].flits == flat.routers[i].flits)` 实测 15 vs 16（r10 7 vs 8、r11 15 vs 16 同差 1）。

③ 需要什么决策（二选一，或给第三种）：
   【实测事实（builder 已做三组对照实验）】
   T1 过滤关（= 修复前，BUFFER_DEPTH=8）: hier 15/8/7/15 · flat 15/8/7/15 ⇒ 相等
   T2 过滤开（修复后，BUFFER_DEPTH=8）: hier 15/8/7/15 · flat 16/8/8/16 ⇒ 差 1
   T3 过滤开 + BUFFER_DEPTH=64（消除 LOCAL credit 上限）: hier 39/20/19/39 · flat 38/19/19/38 ⇒ 仍差 1
   ⇒ (a) 修复只改变 flat、hier 与修复前基线逐项相同；
     (b) 差异不是 credit 耗尽造成的（T3 已排除）；
     (c) 差异来自**闭环反馈回路**：伪 Completion 不再喂给业务侧 ⇒ flat 的下游响应流变化 ⇒ 注入流变化 ⇒ 转发数差 1（hier 的 tick 成组使其未受影响）；
     (d) 故 MESH-3 原来成立的「闭环 router 计数严格相等」部分是 **credit 污染凑成的**，修复只是揭示了判据本身的脆弱性 —— 不是 mesh 转发语义变了（MESH-4/5 仍过）。
   【选项 A（推荐）】把 MESH-3 的等价判据从「闭环计数相等」改为**开环/结构等价**：新增一个固定注入的最小闭环-free 变体（无 stub/home 回程；或用单一 tg 单向注入 + 足够周期），断言两侧 router 转发计数相等；保留原有闭环用例但改断言为「各自 flits>0 + hops>flits + 无静默丢弃」。需授权修改 AC-05 的「禁止改 router 级期望」条。
   【选项 B】保留 MESH-3 原样，把本轮 credit 过滤整体回退 —— 不可接受（等于不修缺陷）。
   【选项 C】承认「层次化与平铺在闭环内不等价」，在 spec/design 里登记该已知限制并删除 MESH-3 的等价断言（最弱，但不诚实——等价性是层次的唯一卖点）。

④ 已完成 / 未完成：
   已完成 —— include/tlm/cep/cohif_nic.hh（handle_net_ingress 丢弃 CATEGORY_CREDIT + stats_credits_dropped_ + 访问器 + 文件头边界说明）；openspec/specs/cep-cohif-network-adapter/spec.md（加从句 + 新增「网络 ingress SHALL 丢弃 credit flit」Scenario + 观测面加 credits_dropped）；test/test_cep_mesh_e2e.cc 新增 MESH-7（AC-01，31 断言）与 MESH-8（AC-02/AC-03，含 nic_b_last_recv_dst==3 && hops==3，两侧各自成立）—— [credit] 57 断言 3 用例全绿；AC-05 要求的 test/test_cep_mesh_e2e.cc:17-21 注释订正尚未做（依赖判据形态定型）。
   未完成 —— MESH-3 判据形态待决策；KPI slot 未回填（openspec change 目录尚未建立）；归档/验收未走。

⑤ 建议的下一步指令草案：
   「以 planner 身份裁决 MESH-3 判据形态：选选项 A（开环等价），据此修订 improvement 的 AC-05 与 AC-06，并授权 builder 在 test/test_cep_mesh_e2e.cc 内新增开环等价用例 + 订正 :17-21 注释。同时决定是否在本 change 内建立 openspec change 目录（回填 KPI 1/3）。」

