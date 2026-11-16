---
name: cpptlm-router-routing-table-key-direction
priority: P1
phase: phase-10
category: bugfix
type: bug
状态: 🔴 **未立项(backlog)** —— 由 verifier 在 `cpptlm-simmodule-exposed-port-wiring` / `cpptlm-cep-mesh-interconnect-submodule` 验收中登记(台账 Entry #452 · 报告 `.rddf/reviews/cpptlm-simmodule-exposed-port-wiring-verify.md` F-3)
created: 2026-11-16
---

# improvement: cpptlm-router-routing-table-key-direction

**主题**：`RouterTLM` 的路由表以 `transaction_id` 单独为键、**不含方向**，导致「请求与其响应共用同一 tx id」时，响应的 BODY/TAIL flit 命中请求残留的 `out_port=LOCAL` 陈旧条目而被**误投回本地口**（回声）。本 improvement 把路由表键补上方向/类别维度，使路由判定与流量方向解耦。

> **上游**：`RouterTLM` 的 `routing_table_` + `stage_route_computation`（`src/tlm/router_tlm.cc:190-200` 的 BODY/TAIL 查表分支、`:326-330` 的 TAIL 擦除）
> **发现场景**：层次化 CEP mesh 验收（`docs/adr/ADR-X.15` 期间的实测）

---

## Why

### 第 1 条 —— 这是一个**既有缺陷**，与本次两 change 无关（两 change 均未碰 `RouterTLM`）。

`stage_route_computation()` 对 HEAD flit 用 `computeRoute(dst_node)` 计算出口；对 **BODY/TAIL** flit 则按 `packet_id = transaction_id` 查 `routing_table_` 复用先前算出的 `out_port`，并在 TAIL 转发时擦除。

⇒ 若两个方向相反、但 `transaction_id` **相同**的包在时间上重叠，后者的 BODY/TAIL 会复用前者的 `out_port`。

### 第 2 条 —— CohIf 业务流量**天然满足**「请求与响应同 tx id」。

`CohIfNICTLM` 的响应沿原 `transaction_id` 返回（`unpack_flit` 保留 `transaction_id`）⇒ 请求（dst=对端节点）与响应（dst=源节点）**共用 tx id**。当响应到达某 router 时，该 router 的路由表里可能仍留着该 tx 的 `out_port=LOCAL`（请求曾以「本地为目标」被投往 LOCAL 口）⇒ 响应被再次投往 LOCAL ⇒ **回声**：源端 NIC 在 `net_resp_in_` 上收到自己发出的 flit。

### 第 3 条 —— 实测证据（层次化 vs 平铺，同一份互联拓扑、同一 300 周期）。

| 观测 | 平铺基线 | 层次化 |
|---|---|---|
| `nic_a.flits_received()` | 12 | 15 |
| `nic_a.last_recv_dst_node/hops` | 0 / **0**（回声，未穿网） | 0 / 3 |
| `nic_b.last_recv_dst_node/hops` | **3 / 3** | 0 / 0（回声） |
| router `flits/packets/hops` | r00 15/15/29 · r01 8/8/16 · r10 7/7/14 · r11 15/15/31 | **完全相同** |

⇒ **mesh 级转发语义一致**，差异全部落在 NIC 侧计数器上，且**平铺基线同样有回声**（`last_recv_hops==0` 即未穿网）。回声命中次数取决于周内 tick 交织 ⇒ 断言「最后收到的 flit」本身就是脆弱判据（这才是 AC-07/AC-04 需改判据的根因）。

---

## What Changes

1. `src/tlm/router_tlm.cc`：`routing_table_` 的键由 `transaction_id` 扩为 **(transaction_id, 方向/类别)** —— 方向可取 `flit.is_request()` 类语义或 `flit_category`，也可用「HEAD flit 的 (in_port, transaction_id)」作为键。
2. TAIL 擦除逻辑同步按新键擦除，避免残留。
3. 单测：构造「请求 + 响应共用 tx id 且方向相反」的最小拓扑，断言**任何 dst ≠ 本节点**的 flit 都不会从 LOCAL 口射出。

## Acceptance

| # | 判据（机器可判） |
|:--:|---|
| AC-01 | 最小拓扑下，`nic_a.flits_received() == 响应条数`（**零回声**）· 平铺与容器两种装配下均成立 |
| AC-02 | `nic_b.last_recv_dst_node == 3` **稳定成立**（不再依赖 tick 交织）—— 使「最后到达 flit」类断言重新可用 |
| AC-03 | router `flits/packets/hops` 与修复前**逐项不变**（修复只消除误投，不改变正常转发计数） |
| AC-04 | `[cep]` / `[e2e]` / `[pcie]` 全绿；**允许**对既有 e2e 的 `last_recv_*` 类断言按新（稳定）值**重基线**，但须在 change 内显式列出重基线项 |

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|:--:|
| C-1 | 路由判定与流量方向解耦，消除同 tx id 跨方向回声 | ❌ 0 |
| C-2 | 「最后到达 flit」类断言重新可靠（提升判据稳定性） | ❌ 0 |

## Impact

- **改动**：`src/tlm/router_tlm.cc`（`routing_table_` 键 + 查表 + TAIL 擦除），可能触及 `RoutingEntry` 结构；新增单测。
- **⛔ 不动**：`CohIfNICTLM`（响应仍按 tx id 返回，这是协议语义，不是缺陷）、`module_factory.cc`（Step 7c 与此无关）。
- **回归面**：所有基于 `RouterTLM` 的拓扑（`[cep]` / `[e2e]` / mesh / `NICTLM` 相关）。⚠️ 既有 e2e 的 `last_recv_*` 断言可能需要重基线（见 AC-04）。
- **计数**：`INV-CEP-*` 新增 **0**。

## ⚠️ 关联

Entry **#451**（两 change 归档）· **#452**（验收闭合 + 本 backlog 登记）· `.rddf/reviews/cpptlm-simmodule-exposed-port-wiring-verify.md` F-3 · `.rddf/reviews/cpptlm-cep-mesh-interconnect-submodule-verify.md` F-3 · `docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md`（其 Status Update 已登记该发现）· `openspec/specs/cep-mesh-interconnect-submodule/spec.md`
