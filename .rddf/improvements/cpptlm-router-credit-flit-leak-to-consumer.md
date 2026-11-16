---
name: cpptlm-router-credit-flit-leak-to-consumer
priority: P1
phase: phase-10
category: bugfix
type: bug
状态: 🔴 **未立项(backlog)** —— 由 verifier 在两 change 验收中登记;根因于 2026-11-16 planner 复核时**订正**(原草稿误判为「routing_table_ 键不含方向」,已证伪:所有 CohIf flit 均 `FLIT_HEAD_TAIL` ⇒ `is_head()` 恒真 ⇒ BODY/TAIL 查表分支不可达)。见台账 Entry **#452**(初判)/ **#453**(根因订正)
created: 2026-11-16
---

# improvement: cpptlm-router-credit-flit-leak-to-consumer

**主题**：`RouterTLM` 的 **credit 回流以「flit」形式**走在 `resp_out` 通道上（仅靠 `Packet::type == PKT_CREDIT_RETURN` 与 bundle 的 `flit_category == CATEGORY_CREDIT` 区分）。当 `resp_out` 的下游**不是 router 而是普通消费者**（如 `CohIfNICTLM`）时，消费者若不做 credit 过滤，就会把 credit flit 当成数据 flit 消费 —— 造成**计数虚增**、**观测计数器被覆写**，并向业务侧灌入**全零伪事务**。本 improvement 定义该缺陷的判据与修复方向。

> **上游**:`include/framework/bidirectional_port_adapter.hh:299-330`（`send_credit` / `send_credit_impl`）、`:126-139`（`NoCFlitBundle` 的 `fill_credit`）、`src/tlm/router_tlm.cc:474-500`（`send_credit_to_upstream` / `send_credit_signal`）、`include/tlm/router_tlm.hh` 的 `reverse_port`（`case 4: return 4; // LOCAL → LOCAL`）
> **发现场景**:层次化 CEP mesh 验收（`docs/adr/ADR-X.15` 期间）

---

## Why

### 第 1 条 — credit 是**带内**（in-band）信号，靠 bundle 字段与 Packet 类型双标记

```cpp
// bidirectional_port_adapter.hh:132-136（NoCFlitBundle 特化）
static void fill_credit(noC::NoCFlitBundle& b, unsigned vc) {
    b.vc_id.write(vc);
    b.flit_category.write(NoCFlitBundle::CATEGORY_CREDIT);   // ← bundle 级标记
    b.flit_type.write(NoCFlitBundle::FLIT_HEAD_TAIL);
}
// :321-324 —— 与数据 flit **同一通道** resp_out_port_ 发出
pkt->type = PKT_CREDIT_RETURN;                                // ← Packet 级标记
```

⚠️ **`dst_node` / `hops` 均保持默认值 0**（`BundleT credit_bundle{}` 值初始化）⇒ credit flit 的 `dst_node==0`、`hops==0`。

### 第 2 条 — router 对**每个**入向 flit 都回 credit，**包括来自 LOCAL 口的**

`stage_switch_traversal` 每次转发成功即 `return_credit_to_upstream(in_port, in_vc)`；`reverse_port(LOCAL) == LOCAL` ⇒ 经 LOCAL 口进入 router 的 flit，其 credit 会被送回 `resp_out[LOCAL]` —— 即**跟着 data flit 一起流向挂在 LOCAL 上的消费者**。

⇒ 每个「NIC → mesh」的请求，会在 **NIC 侧的 `resp_out[LOCAL]` 通道**上换回 1 个 credit flit；每个「mesh → NIC」的响应到达 LOCAL 时同理回 1 个 credit 给该 NIC。

### 第 3 条 — `CohIfNICTLM::handle_net_ingress` 不辨 credit（缺陷点）

```cpp
inline void CohIfNICTLM::handle_net_ingress() {
    if (!net_resp_in_.valid() || !net_resp_in_.ready()) return;
    const Flit flit = net_resp_in_.data();
    net_resp_in_.consume();
    ++stats_flits_received_;                 // ← ① credit 也算「收到」
    last_recv_dst_node_ = flit.dst_node.read();  // ← ② 被覆写成 0
    last_recv_hops_    = flit.hops.read();       // ← ② 被覆写成 0
    pe_resp_out_.write(unpack_flit(flit));   // ← ③ credit 变成全零 CohIf 灌给业务侧
}
```
`unpack_flit` 按 `flit_category != CATEGORY_REQUEST ⇒ Completion` 投影 ⇒ **credit 被投影成一条 `Completion`**（事务号/地址/数据全 0）。

### 第 4 条 — 实测证据（逐周期观察，层次化 `cep_mesh_e2e.json`，300 周期）

```
cyc= 11 | nic_a recv=1 last(dst=0 hops=0) | nic_b recv=0 last(dst=0 hops=0)
cyc= 13 | nic_a recv=2 last(dst=0 hops=0) | nic_b recv=1 last(dst=3 hops=3)   ← 真请求(hops=3)
cyc= 18 | nic_a recv=4 last(dst=0 hops=3) | nic_b recv=4 last(dst=0 hops=0)   ← credit(hops=0) 紧跟真请求
cyc= 30 | nic_a recv=6 last(dst=0 hops=0) | nic_b recv=6 last(dst=3 hops=3)
...（真请求 dst=3/hops=3 与 credit dst=0/hops=0 交替出现）
```

⇒ 目的 NIC **每收到 1 个真请求，就紧跟着收到 1 个 `dst=0, hops=0` 的 credit flit**。这解释了：
- `nic_*.flits_received()` **大于**真实事务数；
- `last_recv_dst_node()/last_recv_hops()` 取决于「最后一个到达的是数据还是 credit」⇒ **顺序敏感、跨装配方式不稳定**（这才是 AC-07 / mesh AC-04 判据必须改为 mesh 级的根本原因）；
- **平铺基线同样存在**（`nic_a.last_recv_hops==0` 即 credit），只是命中时机不同。

### 第 5 条 — **已证伪**的替代根因（勿再采用）

原草稿一度判定为「`routing_table_` 以 `transaction_id` 单独为键 ⇒ 请求/响应同 tx id 时响应命中残留 `out_port=LOCAL`」。**已证伪**：`CohIfNICTLM::pack_flit` 把每个 flit 设为 `FLIT_HEAD_TAIL`（`include/tlm/cep/cohif_nic.hh`），而 `is_head()` 对 `FLIT_HEAD_TAIL` 返回 true ⇒ `stage_route_computation` **永远走 HEAD 分支（`computeRoute`）**，BODY/TAIL 的查表分支**不可达** ⇒ 路由表碰撞不可能发生在本场景。

---

## What Changes

1. **消费者侧过滤（主修，最小且稳健）**：任何**非 router** 的下游消费者（`CohIfNICTLM`，以及未来任何挂 router `resp_out` 的模块）MUST 在 ingress 处丢弃 credit packet —— 判据用 **bundle 字段 `flit_category == NoCFlitBundle::CATEGORY_CREDIT`**（传输无关；`Packet::type` 在 `InputStreamAdapter<Flit>` 路径上不一定可取）。丢弃**不得**计入 `flits_received` / **不得**写 `last_recv_*` / **不得**转发到业务侧。
2. **（可选·语义清理）router 侧**：`RouterTLM` 对 `in_port == LOCAL` 的入向 flit **不回 credit**（LOCAL 上游不具备 credit 协议语义），或在装配期声明「LOCAL 口上游是否为 credit-aware」。⚠️ 该改动会改变既有 `resp_out[LOCAL]` 流量，须评估所有把 credit-aware 模块挂在 LOCAL 上的拓扑。
3. **单测**：`CohIfNICTLM` 收到 credit packet ⇒ 不计数、不改 `last_recv_*`、不产出 PE 侧响应。

> **建议先做 1（消费者侧）** —— 它是「消费者必须健壮」的一般性要求，且不动 router 的既有语义；2 作为后续清理另议。

## Acceptance

| # | 判据（机器可判） |
|:--:|---|
| AC-01 | 注入 1 个 credit packet 到 `CohIfNICTLM` 的 net ingress ⇒ `flits_received()` **不变**、`last_recv_dst_node/hops` **不变**、`pe_resp_out_` **无输出** |
| AC-02 | 端到端（`cep_mesh_e2e` 层次化 + 平铺）：`nic_*.flits_received()` == 真实事务数（**零 credit 污染**）；`stub` 侧 `net_completions_in` == 真实 completion 数 |
| AC-03 | `nic_b.last_recv_dst_node() == 3` 在**两种装配方式下都稳定成立**（不再依赖到达顺序）⇒ `last_recv_*` 类断言重新可用 |
| AC-04 | router 的 `flits/packets/hops` **逐项不变**（过滤只发生在消费者侧，不改 mesh 转发） |
| AC-05 | `[cep]` / `[e2e]` / `[pcie]` 全绿；允许对既有 e2e 的 `last_recv_*` / `completions_in` 类断言按**新（干净）值**重基线，但须在 change 内逐项列出 |

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|:--:|
| C-1 | 非 router 消费者不再被 in-band credit 污染（计数 + 观测 + 业务流三面） | ❌ 0 |
| C-2 | `last_recv_*` 类观测恢复稳定 ⇒ 判据可简化回「最后到达」语义 | ❌ 0 |

## Impact

- **改动（主修）**：`include/tlm/cep/cohif_nic.hh`（`handle_net_ingress` 加 credit 过滤）+ 新增单测。
  ⚠️ 该文件是 `cpptlm-cep-mesh-e2e-carrier` 的产物；本项目「23 ABI 冻结」不涉及它，但**须同步其所属 spec**（`openspec/specs/cep-cohif-network-adapter/spec.md` 增一条「credit 过滤」要求）。
- **改动（可选）**：`src/tlm/router_tlm.cc`（LOCAL 口不回 credit）—— 需评估回归面。
- **⛔ 不动**：`module_factory.cc`（Step 7c 无关）、`CohIfBundle`（协议面不变）。
- **回归面**：`[cep]` / `[e2e]` / mesh / 任何「router `resp_out` 接非 router」的拓扑。
- **计数**：`INV-CEP-*` 新增 **0**。

## ⚠️ 关联

Entry **#452**（verifier 登记的初判，根因有误）· **#453**（根因订正）· `.rddf/reviews/cpptlm-simmodule-exposed-port-wiring-verify.md`（F-3，根因需同步订正）· `.rddf/reviews/cpptlm-cep-mesh-interconnect-submodule-verify.md`（F-3 同上）· `docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md`（其 Status Update 的发现 2 需同步订正）· `openspec/specs/cep-cohif-network-adapter/spec.md`
