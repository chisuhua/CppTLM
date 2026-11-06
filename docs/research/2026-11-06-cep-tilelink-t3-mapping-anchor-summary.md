# TileLink-C · T3 映射锚点摘要(CohIf 平台消息/意图面 → 载体事件轴)

> **状态**: ✅ **已产出**(2026-11-06)· **来源**: change `cpptlm-cep-tilelink-t3-mapping-survey`(per 台账 **Entry #241**)
> **用途**:`B3b` 运行时引擎的最后一个前置 —— `07 号` **Status Update(2026-11-06)** §⑤ 的 **T3(可消费)** 判据:
> 存在「**平台消息/意图面 → 载体事件轴**」的映射,**每行有条款级 `ref`**,无锚行显式 `pending` 且 **fail-closed**。
> **唯一人工审计面**。载体 = `include/tlm/cep/tilelink_state_spec.hh` 的 `kT3MappingCells`(11 行)。
> **上游**:`docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md`(状态集合 / 事件词表 / 10 条消息表 / 5 通道语义 / canonical 10 流映射)
> **复核周期**: 来源改版即失效(见 §7)

---

## §0 范围与合规声明(⚠️ 必读)

| # | 约束 | 依据 | 本轮执行 |
|---|---|---|---|
| 1 | ⛔ **不得**把 TileLink-C 的规范正文**整段**投喂任何 AI | TileLink-C © SiFive(**非** CC-BY)⇒ 只记**条款级定位 + 断言摘要**,不抄录长段原文(per 上游锚点摘要 §0 同款纪律) | 本文件**只记条款号 + 一句语义要点**;§2 的 `ref` 列全部是 `D1 §8.1.x` / `Table 8.3` 级定位 |
| 2 | ✅ TileLink-C 是**免费开放标准** ⇒ 规范正文**可读** | 上游锚点摘要 §0(与 CXL 全程禁读规范正文**完全不同**) | 本轮的 T3 普查**可得条款级 basis** ⇒ 这是**能闭合**的普查,⛔ 不是「推理填空」 |
| 3 | ⛔ **不得**以「公开来源(`A-n`)」充当**规范**依据 | 承上游锚点摘要 §0 的同类纪律 | §2 的 11 行 `basis` **全部为 `Direct`**(条款级),**零** `A-n` 冒充 |
| 4 | ⛔ **不得**只做「抽样」 | `07 号` SU 2026-11-06 §⑤「行宇宙 = **逐行列出,不是抽样**」 | §1 给出**闭集**(canonical 10 流 + atomic 意图 = 11 行),§2 逐行填满 |
| 5 | ⛔ **不得**把 `ADR-CEP-12` 的「10 行逐消息表」当 T3 已满足 | 那是 **canonical 10 流 → 6 类消息类**(`vc_binding` 轴),**目标轴不同** ⇒ 不同 artifact | 见 §5 |

---

## §1 行宇宙(canonical 10 流 + atomic 意图 = **恰 11 行**)

平台消息/意图面 = **canonical 10 流**(per `ADR-CEP-12` §2 决策 1)+ **atomic 意图**(per 决策 2)。

| # | 平台意图 | `flow_support` | 说明 |
|---:|---|:--:|---|
| 1 | `request` | true | PE 发 |
| 2 | `completion` | true | PE 收 |
| 3 | `snoop` | true | PE 收 |
| 4 | `snoop_resp` | true | PE 发 |
| 5 | `writeback` | true | PE 发 |
| 6 | `forward_data` | false | 无独立流 |
| 7 | `back_invalidate` | false | 无独立流 |
| 8 | `address_resolve` | false | 无 |
| 9 | `barrier` | false | 规范无 barrier 消息 |
| 10 | `snoop_completion_ack` | false | 无独立流 |
| 11 | atomic 意图 | — | 不在 `flow_support` 内(per 决策 2:atomic 意图是 `request` 流的**独立属性**) |

⇒ **闭集 11 行** ⇒ §2 逐行填满,⛔ 无遗漏、无抽样。

---

## §2 逐行映射表(**11 行**,载体 `kT3MappingCells` 的**唯一**目标)

> **事件语义** = **PE 自己副本**的权限转移(4 类事件,per 上游锚点摘要 §2.2 的 `Cap`/`Grow`/`Prune`/`Report`)。
> **空掩码** = 该意图**不产生**事件(⛔ 与「未锚」不同 —— 后者是 `Pending`,**fail-closed**)。

| 行 id | 平台意图 | TileLink-C 消息(通道) | 事件 | basis | ref(条款级) |
|---|---|---|:--:|:--:|---|
| **TLC-M-01** | `request` | `Acquire`(A · master→slave) | ∅ | `Direct` | D1 §8.1.1(`Acquire` = 创建副本/升级权限)+ §8.1.2 通道 D(权限**随 Grant 回来**)⇒ **PE 侧此刻未变** |
| **TLC-M-02** | `completion` | `Grant` / `GrantData`(D · slave→master) | **`Grow`** | `Direct` | D1 §8.1.4 `Table 8.3`(`Grow` = 权限升级,长树)+ §8.1.2 通道 D(「provides data or permissions」) |
| **TLC-M-03** | `snoop` | `Probe`(B · slave→master) | **`Prune`** | `Direct` | D1 §8.1.1(`Probe` = 强制移除副本/权限)+ §8.1.4 `Table 8.3`(`Prune` = 权限降级,缩树) |
| **TLC-M-04** | `snoop_resp` | `ProbeAck` / `ProbeAckData`(C · master→slave) | **`Prune`** | `Direct` | D1 §8.1.2 通道 C(「acknowledges a Channel B message, **potentially releasing permissions**」)+ §8.1.4 `Table 8.3`(「potentially」⇒ 事件**可能**发生 ⇒ 集合语义) |
| **TLC-M-05** | `writeback` | `Release` / `ReleaseData`(C · master→slave) | **`Prune`** | `Direct` | D1 §8.1.1(`Release` = 自愿放弃副本/权限)+ §8.1.4 `Table 8.3` |
| **TLC-M-06** | `forward_data` | — | ∅ | `Direct` | 上游锚点摘要 §4(无独立流:数据恒附着于 `GrantData`/`ProbeAckData`/`ReleaseData`) |
| **TLC-M-07** | `back_invalidate` | — | ∅ | `Direct` | 上游锚点摘要 §4(无独立流:强制撤销由 **B**:`Probe` 承载,属 `snoop`) |
| **TLC-M-08** | `address_resolve` | — | ∅ | `Direct` | 上游锚点摘要 §4(路由静态,D2 §6.3) |
| **TLC-M-09** | `barrier` | — | ∅ | `Direct` | 上游锚点摘要 §4(规范**无** barrier 消息) |
| **TLC-M-10** | `snoop_completion_ack` | — | ∅ | `Direct` | 上游锚点摘要 §4(`ProbeAck` 本身即 ack;`E`:`GrantAck` 配对的是 `completion` 而非 `snoop_resp`) |
| **TLC-M-11** | atomic 意图 | — | ∅ | `Direct` | D1 §8.1.3(10 条消息表**无** atomic 类消息)+ `TileLinkCAdapter::kMessageClassMap`(本实例只用 4 类:无 `atomic`/`query`) |

⇒ **11 行全部 `Direct`;`Pending` 行数 = 0**。

---

## §3 PE 角色 = **master**(推导,⛔ 非选择)

事件轴 = **PE 自己副本**的权限转移 ⇒ 先定 PE 在 TileLink 拓扑里的角色。
**上游锚点摘要 §4** 的 canonical 10 流 ↔ TL-C 消息**方向**映射给出 **5/5 一致**:

| canonical 流 | TL-C 消息 | 通道方向 | PE 侧 |
|---|---|---|---|
| `request` | `Acquire` | A · master→slave | **发** |
| `completion` | `Grant`/`GrantData` | D · slave→master | **收** |
| `snoop` | `Probe` | B · slave→master | **收** |
| `snoop_resp` | `ProbeAck`/`ProbeAckData` | C · master→slave | **发** |
| `writeback` | `Release`/`ReleaseData` | C · master→slave | **发** |

⇒ PE 发 A/C 类、收 B/D 类 ⇒ **PE = master** ⇒ 该结论是**推导**(5/5 一致),⛔ **不是**建模选择。

⚠️ **12 格矩阵描述的是「任一 agent 的副本」**(D1 §8.1:「the permissions it is possible for **a particular agent's copy** of a block to have」)
⇒ **不限定** slave ⇒ master 侧同样适用 ⇒ 无「跨节点借用事件」问题。

---

## §4 空掩码 6 行的依据(逐条)

| 行 | 为什么**不产生**事件 |
|---|---|
| TLC-M-01 `request` | PE 发出 `Acquire` 时**尚未**持有副本(权限随 D 通道的 `Grant` 回来)⇒ PE 副本状态**此刻未变** ⇒ ∅(事件落在 TLC-M-02) |
| TLC-M-06 `forward_data` | TileLink-C **无独立流**(数据恒附着于 `GrantData`/`ProbeAckData`/`ReleaseData`)⇒ 不产生**独立**事件 |
| TLC-M-07 `back_invalidate` | 同上(强制撤销由 **B**:`Probe` 承载 ⇒ 属 `snoop` ⇒ 见 TLC-M-03) |
| TLC-M-08 `address_resolve` | TileLink-C **无**地址解析面(路由静态,D2 §6.3) |
| TLC-M-09 `barrier` | TileLink-C **无** barrier 消息(规范未定义) |
| TLC-M-10 `snoop_completion_ack` | `ProbeAck` 本身即 ack(配对已在 `snoop_resp` 内);`E`:`GrantAck` 配对的是 `completion` ⇒ CEP canonical 10 流**无**该位置 |
| TLC-M-11 atomic 意图 | TileLink-C 的 10 条消息表**无** atomic 类消息;本实例 `vc_binding` 只用 4 类 ⇒ 无 atomic 面 |

---

## §5 与 `ADR-CEP-12`「10 行逐消息表」的**关系**(⚠️ 不同 artifact)

| 表 | 轴 | 载体 | 满足 T3? |
|---|---|---|:--:|
| `ADR-CEP-12` Status Update(2026-11-01)的 10 行逐消息表 | **协议消息 → 6 类消息类**(`vc_binding` 轴) | `tilelink_c_adapter.hh` 的 `kProtocolMessageMap` | ❌ **不满足**(目标轴不同) |
| **本表** | **平台消息/意图 → 4 类事件**(权限转移轴) | `tilelink_state_spec.hh` 的 `kT3MappingCells` | ✅ **满足** |

⇒ 二者**互补,不可互代**;⛔ **不得**据「adapter 已有 10 行表」宣称 T3 已满足。

---

## §6 结论:**全行有条款级锚**

| 指标 | 值 |
|---|---:|
| 行宇宙 | 11(闭集) |
| `Direct` 行 | **11** |
| `SelfDerived` 行 | 0 |
| `Pending` 行(须 fail-closed) | **0** |
| 事件分布 | `Grow` × 1 · `Prune` × 3 · ∅ × 7 |

⇒ per `07 号` SU 2026-11-06 §⑤ 的**二分支验收**:产出**全行有条款级锚**
⇒ ⇒ **可转引擎 change**(下一个 change:TileLink-C 运行时引擎 + 装配 + 1 条 L4 事务级 E2E)
⇒ **无**行需登记 ⏭(TileLink-C **不**进 B3b 的 ⏭ 名单)。

---

## §7 失效条件

- 上游锚点摘要(`2026-11-01-…`)失效(来源改版 / 钉版变更)⇒ 本文件失效
- `ADR-CEP-12` 的 canonical 10 流词表变更 ⇒ §1 的行宇宙失效
- TileLink-C 规范版本从 **D1(1.7.1)** 升到 **D2(1.9.3)** ⇒ 消息表增 2 条(`AcquireBlock`/`AcquirePerm`/`ProbeBlock`/`ProbePerm`)⇒ **TLC-M-01/03 的消息集合**须重判(⚠️ 事件**类别**不变 —— 4 类事件词表随版本变化见上游 §2.4 的 R-T1 提示)
- 本仓把 TileLink-C 的 PE 角色改为 **slave** ⇒ §3 的推导失效、§2 的事件轴须重判(本轮 PE = master 由 5/5 方向一致性**唯一**确定 ⇒ 改角色即改全部行)

---

## §8 遗留登记(**不阻塞**,随载体演进)

| # | 遗留 | 处置 |
|---|---|---|
| 1 | §2 的事件是**协议层**语义(哪个 TL-C 消息触发哪类权限转移),而 CEP 的 PE **不建模 TileLink 树** | ⇒ 引擎 change 须自决:建模树 / 或只做事件映射消费。⛔ **不是** T3 的缺口(T3 只要求映射存在且有锚) |
| 2 | TLC-M-04 的「**potentially** releasing permissions」⇒ 事件**可能**不发生 | 已按**集合语义**处理(可能触发的事件集合 = {`Prune`});引擎侧按「可能」实现 |
| 3 | D2(1.9.3)的 4 条新消息 | 见 §7 的失效条件;本轮锁 D1(1.7.1) |
