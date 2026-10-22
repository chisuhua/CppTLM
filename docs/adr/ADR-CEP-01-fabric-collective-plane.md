# ADR-CEP-01: Fabric Collective Plane 作为旁路平面

> **状态**: 🟡 起草中(v0.6)
> **日期**: 2026-10-19(初版)/ 2026-10-20(v0.5 修订)
> **作者**: CppTLM build agent(architect mode)
> **关联真源**:
> - 真源 1:[`../architecture/06-inter-module-network.md`](../architecture/06-inter-module-network.md) v0.4(锚点 per `ADR-CEP-00A §3.1`;06 §2.1 是 L1-L6 payload/port 层模型,与 CEP 6 层正交共存)
> - 真源 1:[`../architecture/06-bis-coherence-as-network-extension.md`](../architecture/06-bis-coherence-as-network-extension.md) v0.3(锚点 per `ADR-CEP-00A §3.2`;本 ADR supersede 其 P3.5 路径)
> - 真源 3:`ADR-CEP-00-platform-charter-and-governance.md`(归口,草案,循环引用待签发解除)
> - 真源 3:`ADR-CEP-00A-real-source-anchor-map.md`(章节映射,本批起草)
> - 真源 3:`ADR-CEP-06-experiment-protocol.md`(实验输出,本批起草)
> - 真源 3:`ADR-CEP-07-simulation-and-integration-semantics.md`(仿真语义,本批起草)
> - 真源 3:`ADR-CEP-NUMBER-MAP.md`(编号预分配,本批起草)
> - 真源 3:`ADR-DGPU-13-pnn-protocol-neutral-network.md`(已 superseded by 06 v0.4)
> - 真源 3:`ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md`(已 superseded by 06 v0.4)
> - 配套 ADR:`ADR-CEP-02-coherence-barrier-semantics-clarification.md`(canonical 10 流清单)+ `ADR-CEP-03-fabric-collective-plane-internals.md`(内部接口)

---

## 1. 背景

### 1.1 CEP 6 层架构首次定义

`06-inter-module-network.md` v0.4 §2.1 锁定的 L1-L6 是**仿真层 payload/port 层模型**:

```
L1 tlm_generic_payload + PacketPool
L2 MasterPort / SlavePort / PortPair
L3 ChStream 抽象
L4 StreamAdapter 适配层
L5 Bundle 数据格式
L6 具体协议字段
```

**CEP 首次定义**的 6 层架构(per 本 ADR)是**模块互联网络 + coherence 协议 + fabric 视角**:

```
Core / Traffic Generator
        │
        ▼
   Stub Cache Core
        │
        ▼
Coherence Interface Layer
        │
        ▼
Protocol Engine
        │
        ▼
Directory & Ordering Plane   ∥(并列,非堆叠)
        │                       │
        ▼                       ▼
Fabric & Transport Plane  ←──  Fabric Collective / Sync Plane
```

两套层模型**正交共存**,不可混淆。

### 1.2 为什么需要独立 Collective Plane

NVLink SHARP(可聚合原语)和 UALink In-Network Compute(ReadReduce / WriteMulticast / Broadcast / AllReduce)不是传统缓存一致性事务,而是**集合通信原语**:

- **终结点** = "reduce result" 或 "broadcast completion",不是 cache line 状态(64B)
- **完成语义** = group-scoped(全组 ack),不是 requester-scoped(单点 ack)
- **生命周期** = per-op(短),与目录状态(per-line, 长)不匹配

如果把集合通信塞进 Directory & Ordering Plane:
- 腐蚀 Ordering Point 的 per-address 串行化不变量(死锁分析、单一序证明都假设 address-keyed)
- 目录状态(长寿命)与 reduction 状态(短寿命)混居污染 eviction/rollback 语义

如果把集合通信塞进 Fabric & Transport Plane:
- 层语义从"传输"漂移到"传输+计算"
- 关闭 collective 需要在传输层内部做手术,"启停干净"做不到

如果把集合通信塞进 Coherence Interface 抽象事务集(加 `collective_request` + `group_id`):
- 破坏 canonical 10 条流不变量(守 Coherence Interface 10 条流不变量,具体清单 per `ADR-CEP-02 §2` 首次定义 — per oracle B3-6 实证:`ADR-I01` 不存在,改描述性引用)
- group_id 对 Coherence Interface 违反协议无关性(CHI / CXL / PCIe 无 group 概念)

---

## 2. 决策

### 决策 1:6 层架构 = 5 层 + 1 旁路平面

**关键约束**:
- Directory & Ordering Plane 与 Fabric Collective Plane **横向并列**,**彼此无调用边**
- 两者都向下消费 Fabric & Transport Plane 的服务
- CHI / CXL 配置下用 `{multicast_sync}` 裁剪实例 + `NullCollectivePlane` 可关闭
- NVLink / UALink 配置下用全集(reduce / broadcast / multicast / allreduce / barrier)

### 决策 2:完成域判别规则(单规则推导所有同类争议)

```
requester-scoped completion → Coherence flow
group-scoped completion     → Fabric Collective Plane
```

**判别基础**:平面相对通用性 — 字段 F 可进入平面 P 的共享接口,当且仅当所有映射进 P 的协议都能为 F 赋予语义(允许赋值为"不支持")。

### 决策 3:barrier 流重钉为 Ordering Point(per ADR-CEP-02 详化)

- Coherence Interface 已有的 `barrier` 流钉死为 **requester-scoped 语义**(单请求者排空/可见性点)
- **流数不变**(守 ADR-CEP-02 §2 canonical 10 条流清单)
- rendezvous barrier(group-scoped)一律走 Fabric Collective Plane 的 `collective_barrier` 原语

---

## 3. 关键约束

### 3.1 barrier 分类 litmus test(写进 ADR 正文)

**判断标准**:
1. **barrier 的 key 是地址还是参与者组?**
   - 地址 → Ordering(per-address 串行化)
   - 组 → Collective(group rendezvous)
2. **完成条件数的是事务还是参与者?**
   - 数事务(drain/flush,等先前请求全部 completion ack)→ Ordering
   - 数参与者(N 个成员到达后 release)→ Collective

**复合流**:`release-drain → rendezvous` = Ordering drain + Collective rendezvous 的复合,由 Core / Traffic Generator 层编排,两个平面各自保持单纯。

### 3.2 完成域判别规则的形式化(写进 ADR)

- `Coherence Interface` 10 条流的隐含不变量 = **pairwise 完成纪律**(一个请求者发出,一个终结点应答,完成 = 单点 ack)
- `Fabric Collective Plane` 流的隐含不变量 = **group-scoped 完成纪律**(完成 = 全组 ack / 全组到达)
- 死锁分析、ordering 规则、形式化验证全部建立在完成纪律上,**两套纪律不能混用同一分析工具**

### 3.3 抓取点(评审检查单,按风险排序)

**高 impact / 必查**:
1. **Collective Plane → Directory 的调用边必须为零**(图谱/grep 机器检查)。存在即层级纠缠。
2. **组播投递单实现**:coherence snoop broadcast 与 collective multicast 共用 Fabric 层投递机制,Collective Plane 只拥有语义(reduction 函数、成员、到达计数)。发现两套组播路由 = 驳回。
3. **死锁通道隔离**:collective barrier 等待期间不得占用 coherence 事务依赖的 fabric 资源。规则:集合流量走独立 VC(`include/bundles/noc_bundles_tlm.hh:38` `vc_id` 字段 0-3,已存在);路由器侧多 VC 仲裁**待 CEP-09 起草**(per `ADR-CEP-NUMBER-MAP §2 Decision 1` 预分配)。**v0.3 补充**:本条只处理"coherence ↔ collective"的**跨平面**通道隔离;"coherence 内部 VC 分配如何保死锁自由"是**平面内**的同类问题,由 `ADR-CEP-09` 承担 —— 依据:VC 分配的目的**不是 QoS 调优,而是保证死锁自由**(per 调研 F18 / 博士论文 §9.3.1 "for ensuring deadlock freedom")。注:`noc_bundles_tlm.hh:60-62` 定义 `flit_category` 3 类常量(REQUEST=0 / RESPONSE=1 / CREDIT=2),初版 §3.3 写"2 类"系 oracle B2-4 实证错误,**已修订为 3 类**。

**中**:
4. **Coherence Interface canonical 10 条流清单零 diff**(机器 diff 检查,per `ADR-CEP-02` §2 首次定义清单)。
5. **barrier 分类规则**(§3.1 两条 litmus test)写进 ADR 正文,不留口头约定。
6. **partial failure**:barrier/reduce 的成员掉线路径必须有显式建模(timeout 或 error completion),否则平台自带一个不可见死锁点。

**低 / 可选**:
7. **复合流**(release-drain → rendezvous)用跨平面依赖令牌在 Core 层编排,作为示例写进设计文档,防后人走捷径拓宽事务集。

---

## 4. 关联 ADR / OpenSpec

- 取代:[`06-bis-coherence-as-network-extension.md`](../architecture/06-bis-coherence-as-network-extension.md) v0.3 §3.4 programmatic bridge 路径中的 Fabric Collective 候选(如有)
- 配套:`ADR-CEP-02-coherence-barrier-semantics-clarification.md`(barrier 流附注 + canonical 10 流清单)
- 配套:`ADR-CEP-03-fabric-collective-plane-internals.md`(内部接口 + 跨 plane 契约)
- 关联:`ADR-CEP-00-platform-charter-and-governance.md`(归口,草案)
- 关联:`ADR-CEP-04-sharer-tracker-abstraction-redesign.md`(Sharer Tracker 与 Collective Plane 关系)

---

## 5. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 平面归属 | 独立第 6 层(横向并列,非堆叠) | ✅ 已拍板 |
| 完成域判别规则 | requester-scoped → Coherence / group-scoped → Collective | ✅ 已拍板 |
| barrier 流重钉 | Ordering Point(零流数变更) | ✅ 已拍板 |
| CHI 裁剪实例 | `{multicast_sync}` 能力集 | ✅ 已拍板 |
| NVLink/UALink | 全集(reduce/broadcast/multicast/allreduce/barrier) | ✅ 已拍板 |
| NullCollectivePlane | 作为"启停干净"验收测试 | ✅ 已拍板 |

---

## 6. v0.2 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
## 7. v0.3 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
