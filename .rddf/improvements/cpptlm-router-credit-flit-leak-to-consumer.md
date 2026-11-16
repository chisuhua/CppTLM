---
name: cpptlm-router-credit-flit-leak-to-consumer
priority: P1
phase: phase-10
category: bugfix
type: bug
状态: 🟡 **已立项**(2026-11-16 · 台账 Entry **#454** · KPI **保持 0/3** —— slot 在 builder P0 建立 `openspec/changes/<name>/` 时回填 1/3 · 经 metis 审查修订:根因订正 + scope 收敛为消费者侧过滤)
created: 2026-11-16
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
| **AC-05** | **重基线受控**：只允许重基线 `test/test_cep_mesh_e2e.cc` 中 **`hier.nic_b_recv == flat.nic_b_recv`(:291)** 与 **`nic_b_recv > 0`(:292)**、**`stub_completions_in > 0`(:304)**、**`nic_a_recv > 0`(:305)** 四处，且须在 change 内逐条列出「旧值 → 新值 + 推导依据」。**禁止**把 `==` 放宽为 `> 0`、**禁止**删除断言、**禁止**改 router 级期望。另须同步订正 `test/test_cep_mesh_e2e.cc:17-21` 的注释（其「NIC 计数时序敏感」论证在过滤后只剩一半成立） |
| **AC-06** | **已知限制显式化**：长 run（> `BUFFER_DEPTH × NUM_VCS` = 32 个 flit / LOCAL 口）会因 LOCAL credit 耗尽而停滞（既有缺陷，见 `.rddf/improvements/cpptlm-router-local-port-credit-exhaustion.md`）。⇒ 测试 run 须**明确**在 32 个 flit 内，且 change 的 design.md 与 spec 条款须写明「本 change 只解 credit 污染，不解 credit 耗尽」 |
| **AC-07** | **伪 Completion 消失**：`w.net_completions_in == w.net_requests_out`（若因 AC-06 的停滞不成立，则退为 `net_completions_in <= net_requests_out` 且记明差额来自 LOCAT credit 耗尽）；到达 PE 侧的每条 `CohIf` 其 `transaction_id != 0`；`home.counters().foreign_consumed == 0` |

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
