---
name: cpptlm-router-local-port-credit-exhaustion
priority: P1
phase: phase-10
category: bugfix
type: bug
状态: 🔴 **未立项(backlog)** —— 由 planner 在 metis 审查 `cpptlm-router-credit-flit-leak-to-consumer` 时分离出来(台账 Entry **#454**)。**修复涉及 `RouterTLM` 流控语义变更 ⇒ 需 architect 决策**(planner 不拍板架构取舍)
created: 2026-11-16
---

# improvement: cpptlm-router-local-port-credit-exhaustion

**主题**：`RouterTLM` 对**所有**出端口（含 `LOCAL`）都做 downstream credit 记账（转发前 `has_credit` 检查 + `consume_credit`），但 `LOCAL` 口的典型下游是**终端消费者**（`CohIfNICTLM` / PE 侧端点），它们**没有归还 credit 的通道**（`net_resp_in_` 是单向下行 `InputStreamAdapter`）⇒ 每个 `(LOCAL, vc)` 投递满 `BUFFER_DEPTH = 8` 个 flit 后，本地投递**静默停滞**，且 `credit_timeout_` 默认 0（死锁检测关闭）⇒ 无任何告警。

> **上游**：`include/tlm/router_tlm.hh:134`（`BUFFER_DEPTH = 8`）/ `:277`（`downstream_credits_`）/ `:298`（`credit_timeout_ = 0`）、`src/tlm/router_tlm.cc:57`（初值）/ `:307`、`:367`（`consume_credit` 对所有 `out_port`）/ `:450-467`（`has_credit` / `consume_credit` / `return_credit`）
> **发现场景**：metis 审查 `cpptlm-router-credit-flit-leak-to-consumer` 时指出（同源缺陷的另一面）

---

## Why

### 第 1 条 — `LOCAL` 与 mesh 端口走**同一套** credit 记账，无终端例外

```cpp
// router_tlm.cc:363-367（sw 阶段；st 阶段 :305-311 同理）
buf.pop();
consume_credit(out_port, out_vc);   // ← 不区分 out_port == LOCAL
```

初值 `downstream_credits_[p][v] = BUFFER_DEPTH`（`:57`，= 8）。

### 第 2 条 — 终端消费者无法归还 credit

credit 归还靠 `return_credit_to_upstream(in_port, in_vc)`（`:323/359/383`）→ `reverse_port(in_port)` → `BidirectionalPortAdapter::send_credit()`，即归还只发生在 **router 之间**（每条链路的 credit 由下游 router 补回）。

挂在 `LOCAL` 上的 `CohIfNICTLM` 的入向只有 `net_resp_in_`（`InputStreamAdapter<Flit>`，单向下行），**没有反向发送 credit 的能力** ⇒ `downstream_credits_[LOCAL][vc]` 只减不增。

### 第 3 条 — 后果：静默停滞，且无检测

- 每 `(LOCAL, vc)` 8 个 flit 后 `has_credit(LOCAL, vc) == false` ⇒ flit 停在 `pipe_reg_[LOCAL][vc]` 的 `waiting_for_credit` 态，**永远不推进**；
- `NUM_VCS = 4` ⇒ 每个 LOCAL 口最多 32 个 flit 后彻底停；
- `credit_timeout_` 默认 **0 = 关闭**（`router_tlm.hh:298`）⇒ `credit_safety_reset()` 直接 `return`（`router_tlm.cc:503-506`）⇒ 无死锁恢复、无告警。

### 第 4 条 — 与 `cpptlm-router-credit-flit-leak-to-consumer` 的关系

同一根因的**另一面**：「`LOCAL` 下游不是 credit-aware 实体」。那个 improvement 修的是**credit flit 污染消费者数据面**；本 improvement 修的是 **credit 记账把终端端口饿死**。两者可独立修，但**都修完才算 `LOCAL` 口语义闭环**。

⚠️ 不要在那个 change 里顺手改 `RouterTLM` —— 它的 AC 已显式声明「不声称解掉长 run 等价性」（见其 AC-06）。

### 第 5 条 — 当前为何尚未暴露

既有 fixture 规模小（`cep_mesh_e2e` 300 周期、单 NIC 注入 12~15 flit），部分点位刚好在 8 附近或未达 ⇒ 未被断言捕获。**任何把 run 拉长 / 流量加大 / 多 VC 用满的测试都会撞上。**

---

## What Changes（两个候选方案，须 architect 决策）

### 方案 A — `LOCAL` 视为终端端口，不参与 credit 记账（最小）

`has_credit` / `consume_credit` 对 `out_port == LOCAL` 短路（或 `downstream_credits_[LOCAL][*]` 恒为 `BUFFER_DEPTH`）。

- **代价**：若有人把 router 挂在另一个 router 的 `LOCAL` 上，该链路失去流控（当前仓内无此用法：`configs/interconnect/cep_mesh_2x2.json` 的 `LOCAL` 全挂 NIC，mesh 互连走 N/E/S/W）。
- **收益**：零配置面变更。

### 方案 B — per-port `credit_aware` 声明（通用）

新增装配期配置（per-port 或 per-port-class 的 `credit_aware`，默认 `true`），`LOCAL` 缺省 `false`（或由蓝图显式声明）。

- **代价**：JSON schema 新增字段 + 蓝图改动 + 装配期校验；属**配置契约变更**。
- **收益**：支持「LOCAL 挂 router」等未来拓扑；语义显式。

### 无论哪个方案，都应补的

1. **死锁可观测性**：`credit_timeout_` 默认由 0 改为非 0（或在 stall 持续 N 周期时打 `DPRINTF` + 计数 `stats_credit_stalls_`），避免再次静默。
2. **单测**：单 `LOCAL` 下游注入 > `BUFFER_DEPTH × NUM_VCS` 个 flit，断言投递不出现平台期。
3. **spec 落点**：`openspec/specs/` 下 router / mesh 相关 spec 增一条「`LOCAL` 口流控语义」要求（方案 A/B 二选一后写实）。

## Acceptance

| # | 判据（机器可判） |
|:--:|---|
| AC-01 | 单 `LOCAL` 下游、单 VC 注入 40 个 flit ⇒ `下游接收数 == 40`（不出现 8 的平台期） |
| AC-02 | 4 个 VC 同时注入各 20 个 ⇒ 全部到达（累计 80），无 flit 滞留 `waiting_for_credit` |
| AC-03 | 停滞可观测：故意构造不归还 credit 的场景 ⇒ `stats_credit_stalls_ > 0` 或 `credit_timeout_` 触发告警（不得静默） |
| AC-04 | `[cep]` / `[e2e]` / `[pcie]` 全绿；mesh 拓扑的 `router flits/packets/hops` 逐项不变（只改流控记账，不改转发路径） |
| AC-05 | `credit_aware=false` 的端口其 `downstream_credits_` 恒为初值（方案 B 专属判据） |

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|:--:|
| C-1 | `LOCAL` 口终端下游不再被 credit 记账饿死 | ❌ 0 |
| C-2 | credit 停滞可观测（不再静默） | ❌ 0 |

## Impact

- **改动**：`src/tlm/router_tlm.cc`（credit 记账分支）、`include/tlm/router_tlm.hh`（可能新增 stall 计数）、`test/` 新单测；方案 B 另加 `configs/interconnect/*.json` + 装配期校验。
- **⛔ 不动**：`CohIfNICTLM`（消费者侧过滤是另一个 improvement 的事）、`module_factory.cc`。
- **回归面**：所有基于 `RouterTLM` 的拓扑（`[cep]` / `[e2e]` / mesh）。⚠️ 既有 `local` 口流量大的测试可能因「不再停滞」而**行为变化**（接收数变多）⇒ 需逐项复核，不得静默重基线。
- **计数**：`INV-CEP-*` 新增 **0**。

## ⚠️ 关联

Entry **#454**（分离登记）· `.rddf/improvements/cpptlm-router-credit-flit-leak-to-consumer.md`（同源缺陷的另一面）· `include/tlm/router_tlm.hh:134,277,298` · `src/tlm/router_tlm.cc:57,307,367,450-467` · `configs/interconnect/cep_mesh_2x2.json`
