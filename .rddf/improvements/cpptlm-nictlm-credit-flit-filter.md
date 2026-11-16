---
name: cpptlm-nictlm-credit-flit-filter
priority: P1
phase: phase-10
category: bugfix
type: bug
状态: 🔴 **未立项(backlog)** —— 由 architect 在 ADR-X.15「缺陷已修复」订正时登记(per `ADR-X.16` 的 `## 关联` 章「相邻独立缺陷」第 2 条 + `docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md` 的 `## Status Update` 2026-11-17 追加段第 5 条);与已归档的 `cpptlm-router-credit-flit-leak-to-consumer` **同族但不同消费者**
created: 2026-11-17
---

# improvement: cpptlm-nictlm-credit-flit-filter

**主题**：`RouterTLM` 把 credit 回流**以「flit」形式**走在 `resp_out` 通道上（靠 bundle 的 `flit_category == CATEGORY_CREDIT` + `Packet::type == PKT_CREDIT_RETURN` 双标记）。当 `resp_out` 的下游是**不辨 credit 的消费者**时，credit flit 会被当成数据 flit 消费 —— 污染计数、并向业务侧灌入垃圾响应。`CohIfNICTLM` 侧已由 change `cpptlm-router-credit-flit-leak-to-consumer` 修复并归档；**`NICTLM` 侧未修** —— 通用网卡（`NICTLM`）仍会把 LOCAL ingress 的 credit 回流当成数据 flit 重组并转发。

> **上游**：`include/framework/bidirectional_port_adapter.hh:126-139`（`NoCFlitBundle` 的 `fill_credit`）/ `:299-330`（`send_credit` / `send_credit_impl`）、`src/tlm/router_tlm.cc`（`return_credit_to_upstream`）、`include/tlm/router_tlm.hh:229`（`reverse_port`：`case 4: return 4;  // LOCAL → LOCAL`）
> **缺陷点**：`src/tlm/nic_tlm.cc:80-95`（`handle_net_resp`）+ `:182`（`reassemble` 写 `pe_resp_out_`）

---

## Why

### 第 1 条 —— 泄漏机制与 `CohIfNICTLM` 侧完全相同

`RouterTLM` 对**每个**转发成功的入向 flit 回 credit（`return_credit_to_upstream`），而 `reverse_port(LOCAL) == LOCAL` ⇒ 经 LOCAL 口进入 router 的 flit，其 credit 沿 `resp_out[LOCAL]` **回流到挂在 LOCAL 上的下游**。这与已归档 change `cpptlm-router-credit-flit-leak-to-consumer` 的根因是**同一个**。

### 第 2 条 —— `NICTLM` 侧无任何过滤（缺陷点，实测）

`src/tlm/nic_tlm.cc` **全文零 `credit` / `CATEGORY_CREDIT` 命中**（grep 实测）：

```cpp
// src/tlm/nic_tlm.cc:80-95
bool NICTLM::handle_net_resp() {
    if (!net_resp_in_.valid() || !net_resp_in_.ready()) { return false; }
    bundles::NoCFlitBundle flit = net_resp_in_.data();
    net_resp_in_.consume();

    ++stats_flits_received_;          // ← ① credit 也算「收到」

    if (reassemble(flit)) {           // ← ② credit 被当数据 flit 重组
        ++stats_packets_received_;
    }
    return true;
}
```

`reassemble` 在 `:182` 写 `pe_resp_out_.write(resp)` ⇒ credit flit 到达即产出**垃圾 `pe_resp_out`**（其字段多为 0 / 默认值）。

### 第 3 条 —— 三面后果

① `flits_received()` **虚增**；② `packets_received()` 语义污染；③ 向业务侧（`pe_resp_out_`）灌入**垃圾响应**。

### 第 4 条 —— 受影响拓扑（`ADR-X.16` F-6 已列全 `NICTLM` 挂 LOCAL 的实例）

`mesh_2x2_tlm` / `mesh_4x4_tlm` / `mesh_4x4` / `nic_router_nic` / `test_e2e_simulation` / `test_nic_side_rules` / `test_noc_mesh_binding_semantics`。

### 第 5 条 —— ⚠️ **`ADR-X.16` 落地**不**消解本缺口**

`ADR-X.16` 的 D1 = 「LOCAL 端口**不参与 downstream credit 流控**」⇒ `has_credit(LOCAL, vc)` 恒真、`consume_credit(LOCAL, vc)` 不扣减 —— 这管的是 router 的**下行**（router 从下游收 credit / 向下游发）。

而本缺口的路径是 **router → LOCAL 上游**的**上行**（`return_credit_to_upstream` 把 credit 发回 `resp_out[LOCAL]`）⇒ **不受 D1 影响**。

⇒ 两者**独立**，可并行推进；**不可**假定 ADR-X.16 落地后本缺口自动消失。

---

## What Changes

1. **消费者侧过滤（主修，与 `CohIfNICTLM` 同款）**：`NICTLM::handle_net_resp` 在消费 flit 前判 **`flit.flit_category.read() == bundles::NoCFlitBundle::CATEGORY_CREDIT`** ⇒ 丢弃该 flit。**判据必须用 bundle 字段**（传输无关；`Packet::type` 在 `InputStreamAdapter<NoCFlitBundle>` 路径上不一定可取）。
2. **丢弃的语义**：**不**计入 `stats_flits_received_`、**不**计入 `stats_packets_received_`、**不**进入 `reassemble`、**不**写 `pe_resp_out_`。若需可观测性，另加 `stats_credits_dropped_`（与 `CohIfNICTLM` 的 `credits_dropped()` 对称）。
3. **单测**：向 `NICTLM` 的 net ingress 注入 1 个 credit flit ⇒ 计数不变、`pe_resp_out_` 零输出。

> **⛔ 不改 router 侧**：credit 的产生与回传语义属链路层，`RouterTLM` / `RouterPort` / `reverse_port` **零改动**（与已归档 change 的边界一致）。

---

## Acceptance

| # | 判据（机器可判） |
|:--:|---|
| **AC-01** | 向 `NICTLM` 的 net ingress 注入 1 个 `CATEGORY_CREDIT` flit ⇒ `flits_received()` **不变**、`packets_received()` **不变**、`pe_resp_out_` **无输出** |
| **AC-02** | 端到端（`mesh_2x2_tlm` / `mesh_4x4_tlm` 类拓扑）：`NICTLM` 收到的 flit 数 == **真实事务数**（零 credit 污染） |
| **AC-03** | 既有 `NICTLM` 拓扑回归全绿：`test_e2e_simulation` 的 nic 用例 · `test_nic_side_rules` · `test_noc_mesh_binding_semantics` · `[cep]` / `[e2e]` / `[pcie]` 全量 |
| **AC-04** | 判据形态与 `CohIfNICTLM` 侧**一致**（同用 bundle 字段 `flit_category == CATEGORY_CREDIT`；⛔ 不用 `Packet::type`）⇒ 两侧行为可对拍 |
| **AC-05** | **零 diff 面**：`src/tlm/router_tlm.cc` · `include/tlm/router_tlm.hh` · `include/tlm/nic_tlm.hh` 的**接口面** · `include/abi/**`（23 ABI 冻结）· `include/bundles/**` 全部零 diff |

---

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|:--:|
| C-1 | 通用网卡（`NICTLM`）不再被 in-band credit 污染（计数 + 重组 + 业务流三面） | ❌ 0 |
| C-2 | 与 `CohIfNICTLM` 侧形成**同款可对拍**的 credit 过滤语义 | ❌ 0 |

---

## Impact

- **改动**：`src/tlm/nic_tlm.cc`（`handle_net_resp` 的 credit 分支；若加观测面则 `include/tlm/nic_tlm.hh` 的**新增**计数器，⛔ 不改既有签名）+ 新增单测。
- **⛔ 不动**：`RouterTLM` / `RouterPort` / `reverse_port`（credit 产生与回传语义属链路层）/ `include/abi/**`（23 ABI 冻结）/ `CohIfBundle`。
- **回归面**：所有 `NICTLM` 挂 LOCAL 的拓扑（`ADR-X.16` F-6 列 6 个）+ `[cep]` / `[e2e]` / `[pcie]` 全量。⚠️ 若既有 e2e 对 `flits_received` / `packets_received` / `pe_resp_out` 有精确断言，须按**新（干净）值**逐项重基线并在 change 内列出。
- **与 `ADR-X.16` 的关系**：**独立**（D1 管 router 下行；本缺口在 router → LOCAL 上游的上行路径）⇒ 可并行；**不得**假定 D1 落地后本缺口自动消失。
- **计数**：`INV-CEP-*` 新增 **0**。

---

## ⚠️ 关联

- `docs/adr/ADR-X.16-router-local-port-terminal-credit.md` 的 `## 关联` 章「相邻独立缺陷」第 2 条（该缺口的**原始登记处**）
- `docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md` 的 `## Status Update` **2026-11-17（追加）**段第 5 条（消费者侧同族缺口登记）
- 已归档 change `openspec/changes/archive/2026-11-16-cpptlm-router-credit-flit-leak-to-consumer/`（**同根因**，`CohIfNICTLM` 侧的修复；可直接复用其修复手法与判据形态）
- 已归档 improvement `.rddf/improvements/cpptlm-router-credit-flit-leak-to-consumer.md`
- 同源另一面（已立项）：`cpptlm-router-local-port-credit-exhaustion`（`ADR-X.16` 方案 A · 台账 Entry #459）
- 缺陷代码：`src/tlm/nic_tlm.cc:80-95`（`handle_net_resp`）· `:182`（`reassemble` 写 `pe_resp_out_`）
