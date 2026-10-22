# ADR-CEP-02: Coherence Interface 10 条抽象事务流清单 + barrier 流语义澄清

> **状态**: 🟡 起草中(v0.6)
> **日期**: 2026-10-19(初版)/ 2026-10-20(v0.5 修订)
> **作者**: CppTLM build agent(architect mode)
> **关联真源**:
> - 真源 1:`06-inter-module-network.md` v0.4 §2.1 L1-L6 仿真层层模型(正交,非 CEP 6 层架构)
> - 真源 1:`06-bis-coherence-as-network-extension.md` v0.3(锚点 per `ADR-CEP-00A §3.2`;**§11.5 非 P3-P6 演进路线**,§11.5 = "相关测试",全文 grep "barrier" 0 匹配 — per oracle 评审 B6)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节
> - 配套:`ADR-CEP-05-stub-cache-core-interface.md`(折叠映射表引用本 ADR canonical 10 流)

---

## 1. 背景

### 1.1 Coherence Interface 抽象事务流 — CEP 首次定义

之前讨论中提及的"ADR-I01 / 06 §15.1 / 06-bis §11.5"中 10 条抽象事务流的描述,**经 oracle 评审实证**:
- `06-inter-module-network.md` §15.1 行 1170-1179 内容为引用 ArchForge 文档清单,无 10 条流清单
- `06-bis-coherence-as-network-extension.md` 全文 grep "barrier" 0 匹配
- "ADR-I01" 不存在,全仓 grep 仅 CEP ADR 自引用

**结论**:Coherence Interface 10 条抽象事务流清单 **由本 ADR 首次定义**(per oracle B1 修复方向 (b)),作为后续 ADR 与 I 实施契约的 canonical 锚点。

### 1.2 barrier 流语义歧义风险

CEP 抽象事务流必须避免歧义 — `barrier` 流存在混入两种 barrier 语义的风险:

1. **Ordering Point barrier**:requester-scoped,排空本请求者先前在途操作(per CHI DVM-Sync 类)
2. **Group rendezvous barrier**:group-scoped,全组成员到达后 release(per NVLink SHARP / UALink barrier)

如果不钉死 barrier 流语义,无论 ADR-CEP-01 选哪个 Fabric Collective 归属方案,"NVLink rendezvous barrier 走哪条"都会成为下一个争议点。

---

## 2. 决策:Coherence Interface Canonical 10 条抽象事务流清单

| # | 流名 | 方向 | 完成纪律 | 备注 |
|---|------|------|---------|------|
| 1 | `request` | node → center | requester-scoped | 缓存请求:read / unique / writeback / evict |
| 2 | `completion` | center → node | requester-scoped | 请求完成 + data(若 inline)+ perm + error |
| 3 | `snoop` | center → node | requester-scoped | snoop 查询 / invalidate / clean / forward |
| 4 | `snoop_resp` | node → center | requester-scoped | snoop 响应:hit + state + dirty + data + txn_id |
| 5 | `writeback` | node → center | requester-scoped | 驱逐/写回:addr + data + dirty |
| 6 | `forward_data` | node ↔ node(经中心调度)| requester-scoped | 数据转发:src_addr + dst_addr + data + perm(per `ADR-CEP-05` §2 折叠映射表 不可分辨性)|
| 7 | `back_invalidate` | **center → node**| requester-scoped | 反向失效:addr + type(invalidate/clean)+ txn_id(CXL 3.0 BISnp 走这里,host→device 方向)|
| 8 | `address_resolve` | node ↔ center | requester-scoped | 地址归属解析:pa + node_id → home_node + coherent_flag |
| 9 | `barrier` | center → node ↔ center(双向)| **requester-scoped**(per Decision 3) | 单请求者排空 / 可见性点;**不携带 group_id / participant_count / epoch_id** |
| 10 | `snoop_completion_ack` | node → center | requester-scoped | snoop 完成后 ack(CHI CompAck 类);不与 `completion` 流重叠 |

**不变量集合**(CEN-Ib1 ~ CEN-Ib11,与本 ADR 一一对应):
- CEN-Ib1: `request` 必有配对 `completion`(per `ADR-CEP-05` §4.3 INV-SC4 配对性)
- CEN-Ib2: `completion` 不携带 unsolicited 数据(per INV-SC4)
- CEN-Ib3: `snoop` 必有 `snoop_resp`
- CEN-Ib4: `snoop_resp` 必须含 `txn_id` 用于 ack 配对
- CEN-Ib5: `writeback` 不与 `request` 竞态(per INV-SC2 evict 互斥)
- CEN-Ib6: `forward_data` 对 stub 透明(per `ADR-CEP-05` §2 折叠映射表 不可分辨性,契约测试 1)
- CEN-Ib7: `back_invalidate` 是 requester-scoped,**不**是 group rendezvous
- CEN-Ib8: `address_resolve` 在 ProtocolEngine 内终结(per `ADR-CEP-05` §6 折叠表)
- CEN-Ib9: `barrier` 流**不**携带 group_id / participant_count / epoch_id(per Decision 3)
- CEN-Ib10: `snoop_completion_ack` 配对 `snoop_resp` 不配对 `completion`
- **CEN-Ib11**(2026-10-20 新增):atomic 意图作为 `request` 流的**独立属性**承载;`perm` 枚举**维持 4 值锁**(`NONE`/`READ`/`UNIQUE`/`WRITEBACK`),**不新增 "ATOMIC" perm 值**

**跨 ADR 一致性要求**:
- `ADR-CEP-05` §2 必须逐字对齐本表 10 条流
- `ADR-CEP-03` / `ADR-CEP-04` / `ADR-CEP-01` 引用本表时使用本 ADR canonical 名

### 决策 3:barrier 流钉死为 Ordering Point 语义

**barrier 流的语义 = requester-scoped**(requester-scoped 完成纪律):

- **barrier 类型**:quiescence point / visibility point(静默/可见性点)
- **完成条件**:本请求者先前所有在途 coherent 操作已获最终 ack(CompData / SnpResp_I 等),数据可见性已建立
- **典型场景**:
  - CHI DVM-Sync(`之前 DVM 操作全局可见后再前进`)
  - CXL Fence(`之前写对 X 可见后再前进`)
  - GPU sys-scope atomic release 语义(排空部分)
- **不承载**:rendezvous barrier(NVLink SHARP / UALink INC barrier)— 这类走 Fabric Collective Plane 的 `collective_barrier` 原语

**关键不变量**(per ADR-CEP-01 完成域判别规则):
```
requester-scoped completion → Coherence flow (含 `request` / `completion` / `snoop` / `snoop_resp` / `writeback` / `forward_data` / `back_invalidate` / `address_resolve` / `barrier` / `snoop_completion_ack`,10 条流 per §2 Decision 0 表格 — per oracle v0.3 S10 修订:初版漏列 `forward_data` 和 `barrier`)
group-scoped completion     → Fabric Collective Plane (含 collective_barrier)
```

### 决策 1(精简):零流数变更(守 canonical 10 条流清单)

- **不增加** Coherence Interface 抽象事务数量(10 → 11)
- **不修改** 任何现有流的字段语义
- 本 ADR 是首次定义(canonical),不修改既有(per oracle B1 修复方向 (b))

### 决策 2:CXL Fence 走 barrier 流

CXL Fence 的语义 = requester-scoped("我之前的写对 X 可见后再前进"),**留在 coherence `barrier` 流**。

**理由**:CXL 是 host 中心的内存一致性,无 group 概念,barrier 语义本质是 DVM-Sync 类的"我之前的写对 X 可见后再前进" → Ordering Point。

**未来扩展**:若未来出现多 CXL 设备会合需求,再按完成域规则判到 Fabric Collective Plane — 不需要现在预留。

---

> **⚠️ v0.3 补充(per `ADR-CEP-17 §2 Decision 6` + oracle 全面架构审查 Q7)**:新增 **CEN-Ib11** —— atomic 意图的承载方式。
>
> **一手依据**:ProtoGen ISCA 2018 p.7 局限 L2:"ProtoGen **cannot generate new protocol actions not explicitly specified in the SSP**. For example, **it cannot infer how atomic read-modify-writes must be implemented without it being specified in the SSP**" —— atomic RMW 是**协议动作**,须由协议规格显式给出,不能由框架推导。
>
> **结论**:atomic 的**语义**归 Protocol Engine 插件;atomic 的**意图**通过流 1 `request` 的**独立属性**(与 `perm` 正交)传递。**canonical 10 流不变**;**`perm` 枚举不变**。
>
> **配套**:一致性标签(SC / TSO / RC / PLO)对同一 `barrier` 的效果差异归 `ADR-CEP-12` 的 `consistency_labels`,**不改本 ADR 的流语义**(per `ADR-CEP-17` 决策 7 / F7)。

## 3. 关键约束

## 3.1 barrier 分类 litmus test(per ADR-CEP-01 §3.1)

```text
// 判断标准:
// 1. barrier 的 key 是地址还是参与者组?
//    地址 → Ordering (per-address 串行化)
//    组   → Collective (group rendezvous)
// 2. 完成条件数的是事务还是参与者?
//    数事务 (drain/flush) → Ordering
//    数参与者 (N 个成员到达) → Collective
```

**复合流**:`release-drain → rendezvous` = Ordering drain + Collective rendezvous 的复合,由 Core / Traffic Generator 层编排。

## 3.2 barrier 流的契约规范

```text
barrier 流接口信号(per ADR-CEP-02 锁定语义):
  barrier_valid     // 请求者发起 barrier
  barrier_ready     // 协议插件 ack
  barrier_domain    // 一致性域 ID(per `ADR-CEP-13` 待起草,per NUMBER-MAP §2 Decision 1 — per oracle B3-6 实证:ADR-CEP-11 不在预分配表)
  barrier_txn_id    // txn_id(沿用 ADR-X.1-transaction-id.md 命名,非独立定义)
  barrier_direction // 0: center→node, 1: node→center
```

**禁止**:
- barrier 流**不携带** `group_id` 字段(per ADR-CEP-01 完成域判别规则 + CEN-Ib9)
- barrier 流**不携带** `participant_count` 字段(同上)
- barrier 流**不携带** `epoch_id` 字段(同上,group lifecycle 属 Collective Plane)

## 3.3 关于 `barrier_direction` 字段说明

`barrier_direction` 字段(per 初版"06-bis v0.3 雏形"已 oracle B6 删 — 06-bis 全文无 barrier 章节):本字段由本 ADR **首次定义**,允许 barrier 在两个方向上进入(0: center→node, 1: node→center)。ProtocolEngine 适配器层按此区分(CHI 内部 barrier 走方向 0;CXL Fence 走方向 1)。

---

## 4. 与 P2.0-P5 演进路线对齐(per `06-inter-module-network.md` v0.4 §15.4,**无 P6**,per oracle v0.3 B3-7 实证)

| 06 §15.4 阶段 | barrier 流承担 | collective_barrier 承担 |
|--------------|---------------|----------------------|
| **P2.0** CoherenceProtocol C++ 实现 | CHI DVM-Sync 类 drain barrier 实现 | — |
| **P2.1** CoherenceBridge C++ 实施 | barrier 流跨域映射(per-domain barrier) | — |
| **P3** SnoopFilter directory-based | barrier 流 + Ordering Point 配合 | — |
| **P3.5** JSON 端口路径打通 | barrier 域 ID 解析 | — |
| **P4** CHI 完整协议 | CHI CompAck + barrier 完整语义 | — |
| **P5** IOMMU 集成 | CXL Fence 走 barrier 流(NVLink barrier 走 Collective) | NVLink SHARP / UALink INC barrier 走 collective_barrier |

---

## 5. 关联 ADR

- 本体:`06-inter-module-network.md` v0.4 §2.1 L1-L6 仿真层(正交,非 barrier 流真源)
- 配套:`ADR-CEP-01-fabric-collective-plane.md`(旁路平面决策 + 完成域判别规则)
- 配套:`ADR-CEP-03-fabric-collective-plane-internals.md`(collective_barrier 接口)
- 配套:`ADR-CEP-05-stub-cache-core-interface.md`(折叠映射表引用本 ADR canonical 10 流)
- 关联:`06-bis-coherence-as-network-extension.md` v0.3 §7.3-§7.7 阶段范围(P2.0-P5,**§7.8 非阶段范围**,per `ADR-CEP-00A §3.2` 实证)— 未来 supersede by CEP ADR-CEP-00
- 关联:`ADR-X.1-transaction-id.md`(txn_id 命名空间沿用)

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 10 条流清单 canonical | §2 Decision 0(表格) | ✅ 首次定义(per B1) |
| 跨 ADR 一致性 | 后续 ADR 引用本表 10 条流时使用 canonical 名 | ✅ 已拍板 |
| barrier 流语义 | requester-scoped (Ordering Point) | ✅ 已拍板 |
| rendezvous barrier 归属 | Fabric Collective Plane `collective_barrier` | ✅ 已拍板 |
| 流数变更 | 零变更(守 canonical 10 流) | ✅ 已拍板 |
| CXL Fence | 走 barrier 流(非 future) | ✅ 已拍板 |
| barrier_direction 字段 | 本 ADR 首次定义,允许两个方向 | ✅ 首次定义(per B6) |
| barrier_txn_id 命名 | 沿用 `ADR-X.1-transaction-id.md` snake_case 命名 | ✅ 已拍板(per S4) |

---

## 7. v0.2 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
