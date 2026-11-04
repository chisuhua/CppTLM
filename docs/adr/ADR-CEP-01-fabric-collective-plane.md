# ADR-CEP-01: Fabric Collective Plane 作为旁路平面

> **状态**: ✅ **已签发(v1.0)**(2026-11-03 定稿;per 台账 **Entry #169** —— 队列 #13 `cpptlm-cep-nvlink-sharp` 的**开工前置**)
> **日期**: 2026-10-19(初版)/ 2026-10-20(v0.5 修订)/ **2026-11-03 定稿**

> **作者**: CppTLM build agent(architect mode)
> **关联真源**:
> - 真源 1:[`../architecture/06-inter-module-network.md`](../architecture/06-inter-module-network.md) v0.4(锚点 per `ADR-CEP-00A §3.1`;06 §2.1 是 L1-L6 payload/port 层模型,与 CEP 6 层正交共存)
> - 真源 1:[`../architecture/06-bis-coherence-as-network-extension.md`](../architecture/06-bis-coherence-as-network-extension.md) v0.3(锚点 per `ADR-CEP-00A §3.2`;本 ADR supersede 其 P3.5 路径)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节
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

### 2.0 选项对比表(定稿补入 —— 把 §1.2 的三个反例固化成可审计的取舍表)

| 方案 | 结论 | 理由 |
|---|---|---|
| **A** 集合通信塞进 **Directory & Ordering Plane** | ❌ **否决** | 腐蚀 Ordering Point 的 **per-address 串行化**不变量(死锁分析 / 单一序证明都假设 address-keyed);目录状态(**长**寿命)与 reduction 状态(**短**寿命)混居 ⇒ 污染 eviction / rollback 语义 |
| **B** 集合通信塞进 **Fabric & Transport Plane** | ❌ **否决** | 层语义从「传输」漂移到「传输 + 计算」;关闭 collective 需在传输层内部做手术 ⇒ **「启停干净」做不到** |
| **C** Coherence Interface 增**第 11 条流** `collective_request`(带 `group_id`) | ❌ **否决** | 破坏 **canonical 10 条流**不变量(per `ADR-CEP-02 §2`);`group_id` 对 Coherence Interface **违反协议无关性**(CHI / CXL / PCIe 无 group 概念) |
| **D** **独立第 6 层旁路平面**(横向并列,非堆叠) | ✅ **采纳** | 两套完成纪律(requester-scoped / group-scoped)**物理隔离** ⇒ 死锁分析 / ordering 规则 / 形式化验证**不混用同一工具**(per §3.2);平面可**整体关闭**(`NullCollectivePlane`)⇒ 「启停干净」**可测**;同一接口承载 CHI 裁剪实例 / SHARP op 集 / UALink 全集合族 |

> ⚠️ **代价不在选项里** ⇒ 见 **§6 后果 / 代价**(跨平面 ordering 契约是**必需**而非可选)。

### 决策 1:6 层架构 = 5 层 + 1 旁路平面

**关键约束**:
- Directory & Ordering Plane 与 Fabric Collective Plane **横向并列**,**彼此无调用边**
- 两者都向下消费 Fabric & Transport Plane 的服务
- CHI / CXL 配置下用 `{multicast_sync}` 裁剪实例 + `NullCollectivePlane` 可关闭
- NVLink / UALink 配置下用**全集**(= **能力族集合**:reduce / broadcast / multicast / allreduce / barrier)⇒ ⚠️ **原语最小集 = 4**(`REDUCE` / `BROADCAST` / `MULTICAST` / `BARRIER`;`AllReduce` = REDUCE + BROADCAST 的**复合**,**不**列为原语,per `ADR-CEP-03 §2` 决策 2)—— ⚠️ **2026-11-03 订正**(per 台账 Entry #170 ⑤-补 ⑪):原措辞把「allreduce」与「原语」混列,与同文档 §7 判据 + `ADR-CEP-03 §2` 决策 2 **打架** ⇒ 收敛为「全集 = 能力族」

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
3. **死锁通道隔离**:collective barrier 等待期间不得占用 coherence 事务依赖的 fabric 资源。规则:集合流量走独立 VC(`include/bundles/noc_bundles_tlm.hh:38` `vc_id` 字段 0-3,已存在);路由器侧多 VC 仲裁属 **`ADR-CEP-09`**(⚠️ **2026-11-03 订正**:原写「待 CEP-09 起草」**失实** —— `ADR-CEP-09` 已于 2026-10-21 起草(v0.1);**仲裁策略属其决策 4**;per `ADR-CEP-NUMBER-MAP §2 Decision 1` 预分配)。**v0.3 补充**:本条只处理"coherence ↔ collective"的**跨平面**通道隔离;"coherence 内部 VC 分配如何保死锁自由"是**平面内**的同类问题,由 `ADR-CEP-09` 承担 —— 依据:VC 分配的目的**不是 QoS 调优,而是保证死锁自由**(per 调研 F18 / 博士论文 §9.3.1 "for ensuring deadlock freedom")。注:`noc_bundles_tlm.hh:60-62` 定义 `flit_category` 3 类常量(REQUEST=0 / RESPONSE=1 / CREDIT=2),初版 §3.3 写"2 类"系 oracle B2-4 实证错误,**已修订为 3 类**。

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
| NVLink/UALink | 全集(能力族:reduce/broadcast/multicast/allreduce/barrier);**原语最小集 = 4**(`AllReduce` = 复合,不列为原语) | ✅ 已拍板(⚠️ 2026-11-03 措辞收敛,per Entry #170 ⑤-补 ⑪) |
| NullCollectivePlane | 作为"启停干净"验收测试 | ✅ 已拍板 |
| NVLink / UALink 的 SHARP 语义 | **不在本 ADR 定义**(本 ADR 只定**平面归属 + 完成域**)⇒ 具体契约见 `ADR-CEP-03 §3`(R1~R5 + A~D)与队列 #13 / #14 的**规约层** | ✅ 已拍板 |

---

## 6. 后果 / 代价

### 6.1 正面

| # | 后果 |
|---|---|
| 1 | 集合通信与一致性事务**物理隔离** ⇒ 两套完成纪律各自可形式化(死锁分析 / 单一序证明**不混用同一工具**,per §3.2) |
| 2 | collective 平面可**整体关闭**(CHI / CXL 配置默认 `NullCollectivePlane`)⇒ 「启停干净」**可测**(不是口头承诺) |
| 3 | 平面**协议无关**:同一接口承载 CHI 裁剪实例 `{multicast_sync}` / NVLink SHARP op 集 / UALink 全集合族 ⇒ 新协议族**不新增平面** |
| 4 | `group_id` 归属**明确**(collective plane 的一等公民,**不进** Coherence Interface)⇒ 「地址 vs 组」的边界**一次说清**,不留口头约定 |

### 6.2 负面 / 代价

| # | 代价 |
|---|---|
| 1 | **多一个平面** ⇒ 多一套接口 / 类型 / VC / 判据(维护成本) |
| 2 | **跨平面 ordering 必须另立契约** —— 平面并列**不自动**带来可见性次序 ⇒ `ADR-CEP-03 §3` 的 R1~R5 + A~D 是**必需**而非可选(否则 SHARP reduce 与 coherent atomic 的混合流**无序**) |
| 3 | **barrier 语义双轨**:requester-scoped 走 coherence `barrier` 流;group-scoped 走 `collective_barrier` ⇒ **分类错误 = 静默死锁** ⇒ 必须有 litmus test(§3.1)且写入正文(per §3.3 抓取点 5) |
| 4 | 「可关闭」**不是所有配置都成立**:CHI / CXL 配置用 `NullCollectivePlane`;**NVLink / UALink 配置用全集** ⇒ 关闭路径**只在裁剪实例配置下**可测 |

### 6.3 风险

| # | 风险 | 缓解 |
|---|---|---|
| 1 | **纸面契约**:旁路平面只有接口、没有行为面 ⇒ 与 R11(「声明超前于实现」)**同型** | 由 `ADR-CEP-03` 决策 1 的 **3 生效条件** + 本 ADR **§7 Verification** 兜住 |
| 2 | **不可测抓取点被写成恒真判据**:§3.3 抓取点 **2 / 7** 在 Stage 1 **不可测** | per **Entry #66**:推迟 P3 ⇒ **禁**写成恒真 / 恒 SKIP 判据 ⇒ §7 **不收录**该两条 |
| 3 | **分类漂移**:把 group-scoped 完成误判为 requester-scoped(或反之) | §3.1 两条 litmus test 入正文 + §7 **V3** 双向判据(正控 + 反控) |

---

## 7. Verification(前瞻判据 · 可勾选 —— **不是**已跑完的实测日志)

| # | 判据 | 载体 | 反向 fixture(毒化 ⇒ 必红 + 归因 id) |
|---|---|---|---|
| **V1** | Collective Plane → Directory 的调用边**恒为零** | include-graph 闸(grep + 图谱) | 注入一条 collective → directory 边 ⇒ 闸必红(`CE-PLANE-DIR-EDGE`) |
| **V2** | canonical **10 流清单零 diff** —— ⚠️ **判据本体在 `ADR-CEP-02 §8 V1`**(去重)⇒ 本行**不重复**实现 | 见 `ADR-CEP-02 §8 V1` | 归因 id **唯一** = `CP-02`(与 `ADR-CEP-02 §8 V1` **同一套** harness) |
| **V3** | barrier 分类 litmus **双向**成立(地址 key ⇒ Ordering;组 key ⇒ Collective) | 分类函数 + 判据 | ① **正控**:两类各一例通过 ② **反控**:毒化「两者同判」⇒ 必红(`BARRIER-CLASSIFY`) |
| **V4** | CHI / CXL 配置下 collective 原语**零调用**(boot / shutdown 期间) | `NullCollectivePlane` + 调用计数 | 毒化:CHI 配置下发起一次 reduce ⇒ 必红(`NULL-PLANE-CALLED`) |
| **V5** | partial failure 有**显式**建模(`CollectiveEvt::Fault` 存在 + **拒绝静默**) | 静态扫描 + 运行期 | 毒化:成员掉线被静默吞掉 ⇒ 必红(`COLLECTIVE-PARTIAL-FAIL`) |
| **V6-a** | ⚠️ **现状恒真登记**:`collective_vc_reservation` 为空 ⇒ `CP-03`(`collective_plane_contract.hh`)**恒 PASS** ⇒ 该 PASS **不构成**「已满足抓取点 3」的证据(⚠️ 实测 `compute_noc` / `ualink_fabric` 的 `vc_id` **0-3 全被 coherence 占满** ⇒ 无空闲 VC) | 声明层检查 · 既有 id `CP-03` | **无有效反向 fixture**(不填预留 ⇒ 恒 PASS;填预留 ⇒ 必撞号)⇒ ⚠️ **禁**当作鉴别力判据(per 铁律 ①) |
| **V6-b** | **队列 #13 交付门**:预留表**非空** ⇒ 须给出**不撞号**的 `(network, vc_id)` 组合;若**不存在** ⇒ **#13 必须先裁决 VC 空间方案**(扩 VC 上限 / 新开 network / 走 ArchForge 侧 ADR)⇒ **不得**以空预留交差 | 队列 #13 的 `include/tlm/cep/collective_planes.json` 预留表 | 毒化:预留撞号 ⇒ 必红(`CP-03`);⚠️ 归因 id 沿用既有 `CP-03`,**不新造** |
| **V7** | 原语最小集**恰 4** —— ⚠️ **判据本体在 `ADR-CEP-03 §9 V2`**(去重)⇒ 本行**不重复**实现 | 见 `ADR-CEP-03 §9 V2` | 归因 id **唯一** = `CP-PRIM-SET`(与 `ADR-CEP-03 §9 V2` **同一套** harness) |
| **V8** | 平面**可关闭**且关闭后**无残留**(禁「以后再说」式关闭) | 绑定检查 + 关闭路径 | 毒化:关闭后仍有 collective 流量 ⇒ 必红(`PLANE-OFF-LEAK`) |

---

## 8. v0.2 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
## Status Update 2026-10-26 —— §3.3 抓取点的**可测性裁决**(队列 #5 `cpptlm-cep-collective-plane-base`)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §7 正文一律不改**,仅登记一项**可测性裁决**。
> ⚠️ **编号漂移说明**(2026-11-03,per 台账 Entry #170 ⑤-补-3):本行的「**§1 ~ §7**」按**当时编号**理解 —— 定稿新增两章(**§6 后果/代价** · **§7 Verification**)⇒ 原 §6「v0.2 修订记录」/ §7「v0.3 修订记录」**已顺延为 §8 / §9** ⇒ 本行的「一律不改」**不**覆盖新增的两章。
> **背景**: §3.3 的 **7 条抓取点**是「评审检查单」,但**未逐条声明「哪一条在哪个 Stage 可机器检查」** ⇒ 实施期容易出现两种坏形态:(a) 把抓取点抄成 spec 的文本断言(对行为覆盖贡献为 0);(b) 把 Stage 1 物理不可测的条款写成**恒真 / 恒 SKIP** 的判据。
> **裁决(逐条)**:
>
> | 抓取点 | 内容 | 本 Stage 可测性 | 处置 |
> |---|---|---|---|
> | **1**(高) | Collective Plane → Directory 的调用边必须为零 | ✅ **可机器检查**(include-graph 闸) | 队列 #5 落地;**反向 fixture** 必配 |
> | **2**(高) | 组播投递单实现(coherence snoop broadcast 与 collective multicast 共用 Fabric 层投递机制) | ❌ **不可测**(fabric 组播机制不存在) | 推迟至 **P3**;**禁**在 Stage 1 写成恒真 / 恒 SKIP 判据 |
> | **3**(高) | 死锁通道隔离(集合流量走独立 VC) | ⚠️ **仅声明层可测**(collective VC 预留与既有 `vc_binding` 不冲突) | 队列 #5 落地声明层检查;真实 VC 分配论证属 P3 / `ADR-CEP-09` |
> | **4**(中) | Coherence Interface canonical 10 条流清单零 diff | ✅ **可机器检查** | 队列 #5 落地;**反向 fixture** 必配 |
> | **5**(中) | barrier 分类规则写进 ADR 正文,不留口头约定 | ✅ **已满足**(§3.1 两条 litmus test 已在正文) | 无需实施动作 |
> | **6**(中) | partial failure 必须有显式建模 | ✅ **可机器检查**(`CollectiveEvt` 的 `Fault` 变体存在性 + 拒绝静默) | 队列 #5 落地 |
> | **7**(低) | 复合流(release-drain → rendezvous)用跨平面依赖令牌在 Core 层编排 | ❌ **不可测**(无 Core 编排层载体) | 推迟;**禁**在 Stage 1 写成判据 |
>
> **配套约束**: 推迟项 SHALL **不**出现在 Stage 1 的判据清单中(per 抓取点 6「不得静默跳过」精神)⇒ 队列 #5 的契约判据 capability 已把该约束写成可机器核对的条款。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #66**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #66)。

## 9. v0.3 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。

---

## 附录 · Verification 判据的**载体归属**与**归因 id 登记**(2026-11-03,per 台账 Entry #170)

**⚠️ 判据 id 体系(须与既有机器闸一致)** —— 本仓既有归因 id = **`CP-01`(零调用边)· `CP-02`(canonical 10 流零 diff)· `CP-03`(collective VC 预留)· `CP-04`(区域互斥)· `CP-05`(完成域判别)· `CP-06`(抓点可测性登记)**(见 `include/tlm/cep/collective_plane_contract.hh`)⇒ 本章判据**一律复用该体系**;**尚未纳入既有体系**者 ⇒ 归因 id **待队列 #13 建立**(登记于 #13 的 tasks)⇒ **禁**在本仓新造平行 id 体系。

**载体归属总表(S3 —— 每条判据落在**哪个队列的哪个载体**上)**

| # | 判据 | 载体归属 | 载体状态 |
|---|---|---|---|
| V1 | 调用边恒为零 | include-graph 闸 · 队列 #5 已交付(`include/tlm/cep/collective_plane.hh` / `collective_plane_contract.hh`) | ✅ **已存在**(既有 id `CP-01`) |
| V2 | 10 流零 diff | **判据本体 = `ADR-CEP-02 §8 V1`**(去重) | ⚠️ 四方 diff **部分实现**(ADR 自解析 + collective 侧无泄漏 + schema 编译期锁)⇒ **07 号 §3.1 / ADR-CEP-05 侧 diff 未实现** ⇒ 由队列 #13 / #14 补齐(既有 id `CP-02`) |
| V3 | barrier 分类 litmus 双向 | 分类函数(`fabric_ingress_classify`) | ⚠️ **载体尚不存在**(属 P3 / 队列 #13)⇒ 在 #13 落地前**不可执行**;归因 id **待 #13 建立** |
| V4 | CHI / CXL 配置零调用 | `NullCollectivePlane` + 调用计数 · 队列 #5 已交付 | ✅ **已存在** |
| V5 | partial failure 显式建模 | 静态扫描 + 运行期(`CollectiveEvt::Fault`)· 队列 #5 已交付 | ✅ **已存在**;归因 id **待 #13 建立** |
| V6-a | 空预留**显式声明门**(complement: **非**恒真) | 声明层检查 · `include/tlm/cep/collective_planes.json` 的 `vc_reservation_declaration` | ✅ **已落地**(`2ea09607`;既有 id `CP-03`)。⚠️ **以 SU 2026-11-03 的 D.4 为准** —— 原「恒真登记」表述已被取代 |
| V6-c | 前提约束:声明 `unavailable` 时不得存在真实 fabric collective 流量载体 | 队列 #13 规约层 / 实现层的载体清单 | ⚠️ **载体尚不存在** ⇒ 由队列 #13 交付 (归因 id `CP-03-VC-PRECONDITION`) |
| V6-b | VC 预留交付门 | 队列 #13 的预留表 | ⚠️ **载体尚不存在** ⇒ 由队列 #13 交付;⚠️ 与台账 Entry #170 ⑤-补 ⑩ 联动(两个网络 VC 已占满 ⇒ 须先裁决 VC 空间方案) |
| V7 | 原语恰 4 | **判据本体 = `ADR-CEP-03 §9 V2`**(去重) | ✅ **已存在**(队列 #5 的 `CollectiveOp` 恰 4 值) |
| V8 | 关闭后无残留 | 绑定检查 + 关闭路径 · 队列 #5 已交付 | ✅ **已存在**;归因 id **待 #13 建立** |

> ⚠️ **悬空判据禁令**(per Entry #66 对抓取点 2 / 7 的处置):**载体尚不存在**者 ⇒ 该判据在载体落地前**不可执行** ⇒ **禁**写成恒真 / 恒 SKIP 判据 ⇒ 一律标注为「**载体由队列 #N 交付**」。

> ⚠️ **纪律**(per 队列 #12 沉淀的 4 铁律):① **纯负向断言须配正控** ② 每条判据须有**可执行反向 fixture 且因正确原因红**(须写明**载体**与**归因 id**)③ **编译期锁不得只以「编译红」充当 ctest 判据** ⇒ `static_assert` 须另有一条**运行期**判据兜住。
> ⚠️ **不收录**:§3.3 抓取点 **2 / 7**(Stage 1 不可测,per Entry #66)⇒ 禁写恒真 / 恒 SKIP 判据。


---

## 定稿说明(2026-11-03,per 台账 **Entry #169** —— 队列 #13 `cpptlm-cep-nvlink-sharp` 的**开工前置**)

> **性质**:本段是**追加式**(per AGENTS.md「ADR 不可变 + Status Update」先例)—— §1 ~ §7 **正文内容**除下列**订正**外**一律不改**。

**① 本次定稿的实质**

| # | 动作 | 说明 |
|---|---|---|
| 1 | 补 **§2.0 选项对比表** | 把 §1.2 的三个反例(塞 Directory & Ordering Plane / 塞 Fabric & Transport Plane / 加第 11 条 `collective_request` 流)**固化成可审计的取舍表**;新增的第四行 = 本 ADR 的采纳方案(独立第 6 层旁路平面)⇒ 决策内容**未变** |
| 2 | 补 **§6 后果 / 代价** | 正面 4 条 + 负面 4 条 + 风险 3 条(含**缓解**);⇒ 把「代价不在选项里」显式落盘 |
| 3 | 补 **§7 Verification** | 8 条**前瞻**判据(可勾选)⇒ 每条写明**载体**与**反向 fixture 的归因 id**;⚠️ **不收录** §3.3 抓取点 **2 / 7**(Stage 1 不可测,per Entry #66) |
| 4 | §5 决策汇总**补 1 行** | 「NVLink / UALink 的 SHARP 语义」**不在本 ADR 定义**(本 ADR 只定**平面归属 + 完成域**)⇒ 具体契约归 `ADR-CEP-03 §3` 与队列 #13 / #14 的**规约层** ⇒ 消除「本 ADR 到底管到哪」的歧义 |
| 5 | 状态 ⇒ **已签发(v1.0)** | 元数据补 **关联 change**;**正文行数口径** = §1 ~ §7(**不含**修订记录与 Status Update 追加段)⇒ 本 ADR 正文 = **219 行**(2026-11-03 定稿后实测;口径 = §1 ~ §7 + Verification 章,**不含**修订记录 / Status Update 追加段 / **附录**) ≤ 250 |

**② 关联 change**:`cpptlm-cep-collective-plane-base`(✅ 已归档 2026-10-26)· `cpptlm-cep-nvlink-sharp`(队列 #13 · 📋 Planned)· `cpptlm-cep-ualink-inc`(队列 #14 · 📋 Planned)

**③ ⚠️ 决策内容零改动** —— 本次**不新增 / 不修改任何决策**;只补形式要素 + 把 §1.2 的既有论证结构化 + 补 1 行汇总口径。
⇒ 「一决策一文档」的**存量越界**(本 ADR 含 3 个决策)按契约「**存量文档不追溯**」⇒ **记 backlog**,不在本次拆分(per 台账 Entry #169 ⑤)。

---

## Status Update 2026-11-03 —— 队列 #13 的 **VC 空间裁决**(**Decision D**:显式声明门)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §7 与附录正文一律不改**,
> 仅**取代** §7 Verification 的 **`V6-a` / `V6-b` 两行**。**取代形态**照 `ADR-CEP-09 §8.1(取代范围)/ §8.2(取代后文本)` 先例。
> **台账**: `obj-cep-platform §11` **Entry #171**(`decision`)· **用户裁决 2026-11-03**。

### D.1 背景:VC 空间实测**饱和**

| 网络 | `vc_count` | coherence 占用 | 空闲 VC |
|---|---|---|---|
| `compute_noc` | 4 | `{0,1,2,3}` | **无** |
| `ualink_fabric` | 4 | `{0,1,2,3}` | **无** |

- 6 个实例的 `vc_binding` 见 `include/tlm/cep/protocol_instances.json`(`tilelink_c` 用 `{0,2,1,3}`,同样占满)。
- ⚠️ **`vc_id` 是 `ch_uint<8>`**(`include/bundles/noc_bundles_tlm.hh:38`)⇒ **"0-3" 是 schema 策略锁,不是物理上限**。
- ⚠️ **`ADR-CEP-09 §2 决策 4` 显式排除**跨平面 VC 隔离(「本 ADR 是**平面内**问题,两者不重复」)⇒ **不存在**「共享 VC + 仲裁保无环」的现成解。⚠️ **该 ADR 当前仍为草案(v0.1)**,本引用的是**草案措辞**;D.6 已把「`ADR-CEP-09` 定稿」列为重访触发。
- ⚠️ `include/tlm/cep/collective_plane_contract.hh` 的 `evaluate_vc_reservation`:预留表为空 ⇒ **直接 PASS**(`detail` 仅为散文,**不可判定**)⇒ **CP-03 恒真**(违反铁律「纯负向断言须配正控」)。⚠️ 这是 **Decision D 裁决前**的状态;D.4 已取代之(载体见下)。

### D.2 裁决

> **抓取点 3 不变** —— 集合流量若上 fabric,**必须**走**独立** VC。
> **改变的是 CP-03 对「空预留」的处置**:`空预留 + 无显式声明 = FAIL`(不再是 PASS)。

`collective_planes.json` 在 `collective_vc_reservation: []` 时**必须**给出 `vc_reservation_declaration`:

| 字段 | 语义 |
|---|---|
| `status` | 恒为 `"unavailable"`(本拓扑不提供隔离 VC) |
| `reason` | 不可用原因(须可归因:VC 饱和 ∧ 抓取点 3 要求独立) |
| `owner` | 重访责任人(裁决方 / 队列号) |
| `revisit_trigger` | 何时**必须**重访(≥5 VC / 首次需真实网内归约 / `ADR-CEP-09` 给出跨平面无环论证) |

**前提约束**:本拓扑下 **collective 流量不得上 fabric**(只允许 `LocalCollectivePlane` 类本地模型);
将来若需真实网内归约 ⇒ **必须**先解决隔离 VC(即 `revisit_trigger` 命中)。

### D.3 取代范围

§7 Verification 的 **`V6-a` / `V6-b` 两行** —— 自本段起以 **D.4** 为准。

### D.4 取代后文本(规范)

| # | 判据 | 载体 | 反向 fixture(毒化 ⇒ 必红 + 归因 id) |
|---|---|---|---|
| **V6-a** | **空预留门**:`collective_vc_reservation` 为空 ⇒ **必须**有 `vc_reservation_declaration` 且 `status` / `reason` / `owner` / `revisit_trigger` **四字段齐全**;缺任一 ⇒ 必红 | `include/tlm/cep/collective_planes.json` + `collective_plane_contract.hh` 的 `evaluate_vc_reservation` —— ✅ **载体已落地**(commit `2ea09607`:schema 的 `if/then` + JSON 四字段 + 判据 + 3 条反 fixture + 1 条文件级 E2E 反 fixture) | **正控**:四字段齐全 ⇒ PASS · **反 fixture**:删 `revisit_trigger`(或整段删声明)⇒ 必红(`CP-03`) |
| **V6-b** | **非空预留门**:预留非空 ⇒ 每个 `(network, vc_id)` 须 `vc_id ≤ 3` ∧ `vc_id < vc_count` ∧ **不与**该网络既有 coherence `vc_binding` 撞号 | 同上(既有实现) | 毒化:预留撞号 / 越界 ⇒ 必红(`CP-03`) |
| **V6-c**(新增) | **前提约束**:声明 `status: unavailable` 时,拓扑中**不得**存在真实 fabric collective 流量载体(仅本地模型) | 队列 #13 规约层 / 实现层的载体清单 | 毒化:声明 `unavailable` 却接入 fabric collective 通道 ⇒ 必红(`CP-03-VC-PRECONDITION`) |

> ⚠️ **载体状态**:`V6-a` **已落地**(`2ea09607`,含文件级 E2E 反 fixture:删 `revisit_trigger` ⇒ 报告出现 **CP-03** 行且 detail 点名缺项);**`V6-c` 仍由队列 #13 交付**(规约层载体清单)。⇒ 在 `V6-c` 落地前,**不得**宣称「本拓扑的 collective 前提约束已被机器验证」。

### D.5 未采取的路径(用户裁决时逐条比较)

| 路 | 不采理由 |
|---|---|
| **A** 扩 `vc_count` → 5 | 需改本仓 3 个 schema(`protocol_instance` / `collective_planes` / `fabric`)**+ ArchForge 仓**的 `fabric.json`(该文件 `derived_from` = ArchForge `22-nsa` §0.5.4.1)⇒ **跨仓阻塞** ⇒ 队列 #13 在本仓无法推进 |
| **B** 新增 network | 同 A + 拓扑侵入性更强(需 ArchForge 边表) |
| **C** 复用 VC + 时序隔离 | 需以 SU 改抓取点 3 的「独立 VC」表述,且把死锁隔离从**结构性**退化为**论证性** |

### D.6 重访触发条件(命中任一 ⇒ **必须**重开本裁决,**不自动升级**)

1. 拓扑提供 **≥5 VC**(本仓 schema 或 ArchForge 侧变更);
2. 队列 #13 实现层或后续队列**首次需要**真实 fabric collective 流量;
3. `ADR-CEP-09` 定稿且给出**跨平面** VC 共享的无环性论证(目前它显式排除该问题)。

---

## Status Update 2026-11-03(#2)—— `fabric` 口径收窄(S1)+ 归因 id 唯一化(S6)+ V6-b 上界随重访迁移(N1)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §9 正文与 2026-11-03(#1) 段一律不改**。
> 本段**收窄**(#1) 段 **D.2** 的前提约束措辞,并声明 id 唯一化与 V6-b 的上界迁移。
> **台账**: `obj-cep-platform §11` **Entry #178**。

### G.1 **`fabric` 口径收窄(取代 #1 段 D.2 的前提约束句)**

> **原措辞**:「本拓扑下 **collective 流量不得上 fabric**(只允许 `LocalCollectivePlane` 类的本地模型)」
> **收窄后**:「本拓扑下 collective 流量 **不得占用承载 coherence 的 `compute_noc` / `ualink_fabric` 的 VC 与传输通道**
> (只允许 `LocalCollectivePlane` 类的本地模型)」

⚠️ **收窄理由**:「fabric」在两处被用于**不同**含义 —— ① **传输平面 / VC 隔离**(本约束的对象)② **节点域 / 拓扑范围**
(能力域判定的对象)⇒ 原措辞字面会**误禁**「NVSwitch 节点内归约」这一目标场景。
⇒ **抓取点 3 管的是 VC 隔离,不是拓扑域**;**能力域 = 节点内**(NVSwitch 节点内)**不受**本约束限制。

### G.2 **归因 id 唯一化(S6)**

| 义务 | **唯一归因 id** | 说明 |
|---|---|---|
| 调用边恒为零 | **`CP-01`** | 原附录的 `CE-PLANE-DIR-EDGE` 为同义别名 ⇒ **以 `CP-01` 为准** |
| canonical 10 流零 diff | **`CP-02`** | 原 `CEN-10-DIFF` 为同义别名 ⇒ **以 `CP-02` 为准** |
| collective VC 预留(空预留门 / 非空预留门) | **`CP-03`** | — |
| `V6-c` 前提约束 | **移交实现层** | **唯一 id = `CP-03-VC-PRECONDITION`**(本 ADR §7 与附录已印出该 id ⇒ **不再**另起别名);载体由队列 #13 **实现层**交付 |

⇒ **同一义务不得有两个 id**;别名只在本表登记一次,判据实现与 fixture **一律用 `CP-0x`**。

### G.3 **V6-b 的上界随重访迁移(N1)**

`V6-b` 的「`vc_id ≤ 3`」是 **schema 策略锁**(per D.1:0-3 **不是**物理上限,`vc_id` 字段是 `ch_uint<8>`)
⇒ 当 **D.6.1**(拓扑提供 **≥5 VC**)命中并重裁时,**该上界 SHALL 随之一并迁移** ⇒
否则重访会先以「越界」红、而非以「预留冲突」红 ⇒ 归因误导。

### G.4 **V3 的 owner(S2)**

`V3`(barrier 分类 litmus:地址 key ⇒ Ordering / 组 key ⇒ Collective)的载体 owner = 队列 #13 **实现层**
(函数名由实现层定);它与 `ADR-CEP-03` 的 `classify_at_fabric_ingress`(reduce / coherence 分流)**不是同一判据**
⇒ 规约层的 `NSS-01` **不覆盖** V3 ⇒ V3 的归因 id **待实现层建立**。

---

## Status Update 2026-11-03(#3)—— `V6-c` 的归因 id 唯一化(critic 第 2 轮 S6-a)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §9 正文与 2026-11-03(#1)/(#2) 段一律不改**。
> **台账**: `obj-cep-platform §11` **Entry #179**。

### I.1 `V6-c` 的唯一归因 id = `CP-03-VC-PRECONDITION`

(#2) 段 G.2 曾写「id **待实现层建立**」,而本 ADR §7 的 `V6-c` 行与附录**已印出** `CP-03-VC-PRECONDITION`
⇒ 同一义务出现「已有 id 名 / id 待建立」两种状态 ⇒ fixture 该报哪个 id 无定论。

⇒ **裁决**:**唯一 id = `CP-03-VC-PRECONDITION`**;其**载体**由队列 #13 **实现层**交付
(规约层与实现层的载体清单)⇒ 在载体落地前该判据**不可执行**(per 悬空判据禁令)。

## Status Update 2026-11-03(#4)—— **Decision D 重开**:抓取点 3 的**两种满足模式**(路径 D)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §9 正文、附录、以及 2026-11-03(#1)/(#2)/(#3) 三段一律不改**。
> **取代范围**: **D.2 的规范句**(「集合流量若上 fabric,**必须**走**独立** VC」)+ **D.4 的 `V6-a` 行**(单值 `status` ⇒ **双值**)+ **D.4 的 `V6-c` 行**(单值 `status`)⇒ 以 **E.6** 为准。
> ⚠️ **`V6-a` 必须在取代范围内**:其**已落地**的实现(`include/tlm/cep/collective_plane_contract.hh:659-662`)
> 对 `status != "unavailable"` **一律 FAIL** ⇒ 若不取代该行 ⇒ **M-2 模式下 CP-03 必红** ⇒ 队列 #14 的目标场景无法闭合(见 **E.7 负面 ⑥**)。
> **不取代**: **D.1 的背景事实**(VC 空间实测**饱和** —— 该事实**未变**)、**D.3**、**D.5**(未采路径表原文保留)、**D.6**(前 3 条原文不改)。
> **触发**: **D.6 ②** 命中(实测,见 E.1)。
> **台账**: `obj-cep-platform §11` **Entry #193**(授权记录)· **用户裁决 2026-11-03**(路径 D + 证书形态)。
> **形态**: 本段**第 1 稿经 critic 第 1 轮 REJECT(2 BLOCKER)后重写** ⇒ 逐条修订见 **E.10**。

### E.1 触发:D.6 ② 命中,且是**第一次**命中

| 面 | 实测 |
|---|---|
| 队列 #13(NVLink SHARP) | 走 `LocalCollectivePlane`(节点内本地模型,`collective_planes.json` 的 `plane_binding`)⇒ **未**产生真实 fabric 集合流量 ⇒ 未触发 |
| 队列 #14(`cpptlm-cep-ualink-inc`,Stage 3) | 语义**即**网内归约(`ReadReduce` / `WriteMulticast` / `Broadcast` / `AllReduce`,per 07 号 §5.1.1 的 UALink 行)且跑在 `ualink_fabric` 上 ⇒ **必然**产生真实 fabric 集合流量 |

⇒ D.6 ② 原文:「队列 #13 实现层或**后续队列**首次需要真实 fabric collective 流量」⇒ **命中**
⇒ 按 D.6 的「**必须**重开本裁决,**不自动升级**」重开。

### E.2 裁决:抓取点 3 由「单一要求」改为「**两种满足模式**」

**抓取点 3 的义务不变**(集合流量不得与 coherence 事务**共享未论证的**通道依赖)⇒ 变的是**满足方式**:

| 模式 | 形态 | 何时可用 |
|---|---|---|
| **M-1 隔离**(原 D.2 的唯一模式) | 集合流量走**独立** VC ⇒ `collective_vc_reservation` 非空 | 拓扑提供空闲 VC(**本拓扑当前不满足** —— D.1 的事实未变) |
| **M-2 共享 + 证书**(新增) | 集合流量**复用** coherence 已占用的 VC ⇒ 必须同时满足 **E.3 的四条证书** | 声明 `vc_reservation_declaration.status = "shared_no_isolation"` 且证书四条全绿 |

⚠️ **M-2 不是「降级为不需要论证」**:它把「**结构性**隔离」(D.5 所称的优势)换成「**可机器判定的论证**」。
二者**是否等价** —— 见 **E.5**(对 D.5 否决路径 C 的正面回应)。

### E.3 证书四条(**规范定义**)+ 载体与准入条件

#### E.3.1 四条判据

| # | 判据 | 可判形式 | 反向 fixture(毒化 ⇒ 必红) | 归因 |
|---|---|---|---|---|
| **C-1′** | **槽位语义继承**:集合平面 SHALL **不声明**自己的 VC 绑定;它**继承**该 network 上**全部「在该 network 上有 `vc_binding` 的」既有实例**的 `(message_class → vc_id)` 映射;该继承**良定义**的充要条件 = 该 network 上**全部有绑定的**既有实例的映射**逐类相等** | ① 集合平面**无**独立 VC 绑定声明(若声明 ⇒ 必红:意味着它想偏离继承);② 该 network 上全部既有实例的映射**逐类相等**(不等 ⇒ 必红) | 某网实例间映射不一致却用 M-2 ⇒ 必红;集合平面自带独立绑定 ⇒ 必红 | `CP-03-VC-CERTIFICATE` |
| **C-2** | **VC 覆盖完整**:集合平面在该 network 上绑定的 `vc_id` SHALL 覆盖 **`fabric.json` 该 network 的 `vc_count` 全部取值** | 直接对 `fabric.json` 的 `vc_count` 判定(不依赖「边表是否引用某 VC」) | 只覆盖部分 `vc_id` ⇒ 必红 | `CP-09-IC1`(复用,**降为良构性见证 / 正控**) |
| **C-3** | **控制面与数据面互斥**:某 group 有 op **in-flight** 时,SHALL **拒绝** `group_register_region`(R3 drain)与 `group_destroy`(R4 回归)—— 二者是控制面操作 | 运行期断言(fail-fast)+ 调用序列 fixture | in-flight 期间调 `group_register_region` ⇒ 必红 | `CP-03-VC-CERTIFICATE` |
| **C-4** | **未检验项显式登记**:见 **E.3.4** 的两项 | 元测试:登记项存在、非空、且**不**含「已验证 / 已满足」字样 | 把任一未检验项标成「已验证」⇒ 必红 | `CP-03-VC-CERTIFICATE` |

**归因 id 纪律**:四条**共用** `CP-03-VC-CERTIFICATE`(**同一义务一个 id**,per `ADR-CEP-03` F.3 归一表);
⇒ ⛔ 四条共用一个 id ⇒ 反向 fixture **无法区分**哪一条失败 ⇒ `detail` 前缀 SHALL 带**子标记**
(`[CP-03-VC-CERTIFICATE/C-1′]` / `/C-2` / `/C-3` / `/C-4`)⇒ 归因匹配按**完整前缀精确相等**
(per 本仓「精确相等」纪律 —— 前缀匹配会让 `CP-03` 命中 `CP-03-VC-PRECONDITION`);
C-2 复用 `ADR-CEP-09` 既有的 `CP-09-IC1`(**不**新造)。⛔ **不新增** `ContractRule`
(`kContractRuleCount == 6` 是被测试硬断言的契约面)⇒ 四条折进 `CP-03` 行的 `detail` 前缀
`[CP-03-VC-CERTIFICATE]`;⛔ **不得**复用 `[CP-03-VC-PRECONDITION]`(那是 `V6-c` 的精确匹配前缀)。

#### E.3.2 ⭐ 载体:集合平面**不是协议实例**,且**不声明**自己的 VC 绑定 ⇒ **零新声明组**

**第 1 稿的两个载体方案均被否决**:

| 被否决的方案 | 障碍 | 结论 |
|---|---|---|
| **伪实例**:把集合平面塞进 `protocol_instances.json` | ① `id` 的 `pattern` = `^[a-z0-9][a-z0-9_-]*$`(`protocol_instance.schema.json`)⇒ 任何 `collective_plane@<net>` 形态**含 `@`** ⇒ **非法**;② `required` **13** 项 ⇒ 须填 `family`(枚举仅 `industrial` / `ruby-slicc` / `academic`,**无 collective`)+ `adapter`(per `ADR-CEP-12` 决策 1「1 协议实例 = 1 adapter」)⇒ **只能填假值** = 假声明;③ `additionalProperties: false` ⇒ 无处安放证书字段 | **不可实现** |
| **新增声明组**:在 `collective_planes.json` 根级加第 5 组 `collective_plane_vc_binding` | `openspec/specs/cep-collective-plane-contract/spec.md:35`(**M1 冻结件**)写「声明最小集 SHALL **恰含四组**」⇒ 根级第 5 组与之**正面相撞**;而 `vc_reservation_declaration` / `registered_regions` / `collective_vc_reservation` 三组的项均为 `additionalProperties: false` + 固定键 ⇒ **塞不进** | **不可实现**(除非另立声明源文件,而那又与冻结 spec `:27`「声明源 = `collective_planes.json` + schema」相撞 ⇒ **不比现方案省**) |

⇒ **裁决(第 2 稿)**:集合平面**既不进 `protocol_instances.json`,也不新增任何声明组** ——
它**不声明**自己的 VC 绑定,而是**继承**该 network 上既有实例的映射(C-1′ 的判据即「继承**良定义**」)。
⇒ **载体 = 只读的两份既有声明**:`protocol_instances.json`(该 network 上全部实例的 `vc_binding`)
+ `fabric.json`(该 network 的 `vc_count`)⇒ **零新字段 / 零新声明组 / 零 schema 改动**。
⇒ ⛔ **本段不改** `protocol_instance.schema.json`、**不改** `protocol_instances.json`、**不改** `fabric.json`
⇒ **零 M1 冻结件改动**
⇒ ⚠️ **例外(必改)**:`collective_planes.json` / `collective_planes.schema.json` —— E.6 的 `status`
**第二取值**(schema 的 `status` `enum` + 描述串 + JSON 的 `status` 值)⇒ 二者**不在** M1 冻结集
(实测 `cep_freeze_check.sh` 的 `FROZEN_GLOBS` 无命中)⇒ **不需冻结授权**。
⇒ 且「声明最小集**恰含四组**」条款**不受影响**(本段**不新增声明组** —— 见 E.6 / E.7 负面 ④)。

#### E.3.3 C-1′ 的**参照域**(两域并集)与**不一致 ⇒ 该网不得用 M-2**

| 域 | 成员 | 出处 |
|---|---|---|
| 跨仓(ArchForge 派生域) | `moesi_gpu` / `mesif` | ⚠️ **不是** `authority_migrations`(该数组现已**为空**,per Entry #102)⇒ 域 = `derived_from.source_doc` 所指声明 + `repo_owned_instance_ids[]` 的**补集**(现为 `moesi_gpu` / `mesif`) |
| 本仓自持 | `repo_owned_instance_ids[]`(`chi_rnf` / `cxl_2_0` / `tilelink_c` / `cxl_3_0_symmetric` / `nvlink_4` 等) | 同文件 |

⇒ **参照集 = 两域并集**;集合平面的映射 SHALL 与**并集内该 network 上的全部实例**逐类相等。
⇒ **若该 network 上存在实例间映射不一致 ⇒ 该 network 不得使用 M-2**(须回 M-1 或先消除不一致)。
⚠️ 现状:`ualink_fabric` 上 `moesi_gpu` / `mesif` 两实例**一致** ⇒ 该网**可用** M-2;
`compute_noc` 上 `tilelink_c` 与其余 6 实例**不一致** ⇒ **该网不得用 M-2**(登记)。

#### E.3.4 C-4 的两项未检验项

| # | 未检验项 | 为什么**不可检验** |
|---|---|---|
| **U-1** | 「VC 隔离确实避免协议级死锁」(H6) | M-2 **主动放弃**隔离 ⇒ 该假设在共享配置下**永远无法被检验** ⇒ 只能登记,不得当作已满足的前提引用 |
| **U-2** | 「**同一 VC 内** coherence 流量与 collective 流量的**相互阻塞**」 | VC 级依赖图**不表达**同 VC 内两类流量的相互依赖(边按 network 声明、端点同 network、每条边只表达 VC 间的依赖)⇒ 该风险**在模型之外** ⇒ 必须登记,不得沉默 |

### E.4 ⚠️ 为什么**不**采「重跑 DAG」当证书(构造性论证)

「把集合平面喂进 `evaluate_channel_dependency_gate` 重跑 `INV-CEP-09-IC1`」是**直觉上最自然**的证书,
但它**在结构上恒真**,不可作判据。**两条前提**(缺一则结论不成立):

| # | 前提 | 依据 | 今天成立? |
|---|---|---|---|
| **P1** | 声明边表**自身无环** | `protocol_instances.json` 的 `channel_dependency_edges.per_network` | ✅ `compute_noc` = `{0→1,0→2,0→3,1→3,3→2}`(拓扑序 `0<1<3<2`);`ualink_fabric` = `{0→3,1→3,2→3}`(星形) |
| **P2** | 边**按 network 声明**、**不按 instance**,且**两端同 network** | **实现细节**:`include/tlm/cep/channel_dependency_graph.hh:552-568` | ✅ 但**非 ADR 不变量** ⇒ 见下方警告 |

⇒ **结论(构造性,非实测巧合)**:任一实例的图 = 该 network 声明边表的**诱导子图** ⇒ **DAG 的任一子图仍是 DAG**
⇒ 该闸**在结构上不可能**报出由 `vc_binding` 引起的环 ⇒ 它的 verdict **与类→VC 的语义分配无关**。

**实测印证**(3 组,探针一次性、**不入库**;结论可由 **P1 + P2 + 上表两份边表 + `channel_dependency_graph.hh:552-568`** 重推):

| 实验(`ualink_fabric`) | 集合平面的类→VC | 闸 verdict |
|---|---|---|
| 复用既有映射 | `request→0 / atomic→1 / query→2 / response→3 / snoop_resp→3` | PASS(`cycles=0`) |
| **语义旋转**(明显违反 `ualink_fabric` 的锁定槽位语义) | `request→3 / response→0 / atomic→1 / query→2` | **PASS**(`cycles=0, errs=0`) |
| 只声明一个类(其余 VC 未绑定) | `request→0` | FAIL(3 条「边端点未绑定」) |

⇒ 故 **`IC1` 的鉴别力**须重新表述为**三项**,不是一项:

| # | `IC1` 仍有的鉴别力 | 位置 |
|---|---|---|
| ① | **边端点良构**:声明边表引用到的每个 `vc_id` 都必须被绑定 | `channel_dependency_graph.hh:558-565` |
| ② | **`vc_id` 越界** | 同文件 `:538-542`(`kMaxVcId = 3`) |
| ③ | **声明边表自身无环**(P1) | 闸的图算法 |
| ⛔ | **`vc_binding` 的语义分配** —— **零鉴别力** | 实测(语义旋转仍 PASS) |

⇒ 因此**证书的语义部分由 C-1′(语义继承)+ C-3(控制面互斥)承担**,C-2 只承担**覆盖完整性**;
**C-2 亦不依赖 `IC1`**(直接对 `fabric.json` 的 `vc_count` 判)⇒ `IC1` 降为**良构性见证 / 正控**。

> ⚠️ **P2 是实现细节,不是 ADR 不变量**:`channel_dependency_graph.hh` 已有 `cross_net_edges` 计数器
> (同文件 `:92` / `:147`) ⇒ **跨 network 边**的方向**已被预留** ⇒ 若将来引入跨网边 / 回边补全,
> 「诱导子图 ⇒ 恒无环」的构造性论证**失效** ⇒ 已登记为**重访触发**(E.8 第 5 条)。
> ⚠️ **方法论**:任何「把新实例喂给既有闸」的方案,**先做毒化实验证明闸会红**;变绿**不等于**判据有效
> —— 它可能只是「闸看不见这个维度」。

### E.5 ⚠️ 对 D.5 **否决路径 C** 的正面回应(可判性是否与结构性等价)

D.5 否决路径 C 的理由原文:「需以 SU 改抓取点 3 的『独立 VC』表述,**且把死锁隔离从结构性退化为论证性**」。
⇒ 本段**接受**该代价的**前半**(确实要改表述),但**拒绝**其后半的隐含前提(「论证性 = 不可机器判定」):

| 面 | M-1 隔离(结构性) | M-2 共享 + 证书(本段) |
|---|---|---|
| 机制的**真实鉴别力** | 「该 VC 上没有其它流量」= **可机器判定**(预留表非空且不撞号) | 四条判据**全部可机器判定**(E.3.1 每行都有可执行的反向 fixture) |
| 论证是否**可复查** | 不需要论证 | 需要,但**落在声明数据与运行期断言**,不落在散文 |
| 失效形态 | 撞号 ⇒ V6-b 红 | 语义漂移(C-1′) / 覆盖不全(C-2) / 控制面污染(C-3) / 未检验项被冒充(C-4)⇒ 各自必红 |
| **不可判定的残余** | 无 | **U-1 与 U-2**(E.3.4)⇒ **不判定,只登记**(C-4) |

⇒ **判据**:本段**不主张** M-2 与 M-1 等价。M-2 **弱于** M-1(它少了一条结构性保证),但**强于**「散文论证」。
**代价必须显式登记**(E.7),⛔ 不得写成「已等价」。

### E.6 取代后文本(规范)

**取代 D.2 的规范句**:

> 抓取点 3 的义务:**集合流量不得与 coherence 事务共享未论证的通道依赖**。
> ⇒ **两种满足模式**:M-1(独立 VC,`collective_vc_reservation` 非空)或 M-2(共享 VC + E.3 的四条证书)。
> ⇒ `collective_planes.json` 的 `vc_reservation_declaration.status` 取值域:**`"unavailable"` | `"shared_no_isolation"`**。
> ⇒ `status: unavailable` 时 `V6-c` 的**原语义不变**(不得存在真实 fabric 集合流量载体,仅本地模型);
> `status: shared_no_isolation` 时 `V6-c` **不适用**,改由 E.3 四条判据守门。
> ⇒ **载体**:集合平面**不声明**自己的 VC 绑定 ⇒ 它**继承**该 network 上全部既有实例的映射
> (C-1′ 判「继承**良定义**」);载体 = **只读**的 `protocol_instances.json` + `fabric.json`
> ⇒ ⛔ 集合平面**不**进 `protocol_instances.json`(它**不是协议实例** —— 见 E.3.2 的三条障碍);
> ⛔ **不**在 `collective_planes.json` 新增任何声明组 ⇒ 声明最小集**仍为四组**。
> ⇒ **作用域**:证书是**逐 `(plane, network)`** 的;`plane_binding` 为 `null` 的实例**不在**范围内;
> **不产生 fabric 集合流量**的 plane(如 `LocalCollectivePlane`)**不在**范围内。
> ⚠️ `vc_reservation_declaration` 是**拓扑级单值**(根级**属性**;其自身 `required` = 四字段
> `status` / `reason` / `owner` / `revisit_trigger` —— ⚠️ **根级 `required` 列表并不含它**
> ⇒ 它的存在性由 **schema 的 `allOf/if-then`**(`collective_vc_reservation` `maxItems: 0` ⇒ `then.required`
> 含 `vc_reservation_declaration`,实测 `collective_planes.schema.json`)+ **运行时判据 V6-a** **双重要求**),
> 而证书是**逐 (plane, network)** ⇒ **拓扑级开关承载逐 network 证书** ⇒ 声明源 SHALL 记录证书适用的
> `(plane, network)` 清单,未列入清单者 SHALL 视为**不在** M-2 覆盖内。

**取代 D.4 的 `V6-c` 行**:在上表三行(V6-a / V6-b / V6-c)之后追加一行:

| # | 判据 | 载体 | 反向 fixture |
|---|---|---|---|
| **V6-d**(新增) | **共享模式证书**:`status: shared_no_isolation` ⇒ E.3 的 **C-1′ / C-2 / C-3 / C-4** 全绿 | `collective_planes.json`(声明面)+ `collective_plane_contract.hh`(判定面)+ 运行期断言(C-3) | 四条各一(见 E.3.1) |

### E.7 后果 / 代价(独立章,⛔ 不并入决策段)

| 面 | 后果 |
|---|---|
| **正面** | 队列 #14 可在**零 M1 冻结件改动 / 零跨仓依赖**下开工(3 处 schema + `kMaxVcId` + 3 处测试断言**全部不动**);`ualink_fabric` 的 4 VC 硬件语义**不被改写**(保真度不倒退) |
| **正面 ②** | 集合平面**复用**既有实例的 VC 空间 ⇒ 图上**不引入新顶点、不引入新边** ⇒ 无环性结论**直接继承**既有实例的图结论(⇒ 无需重跑,重跑也无新信息) |
| **负面 ① 结构性保证降级** | M-2 少一条「物理上不可能共享」的保证 ⇒ 死锁自由**改由声明数据 + 控制面断言**承担 |
| **负面 ② 未检验项 +2** | **U-1**(H6 隔离假设)与 **U-2**(同 VC 内两类流量的相互阻塞)**不可检验** ⇒ 登记(C-4);假设 H6 从此**不能再被当作已满足的前提**引用 |
| **负面 ③ 判据面 +1 行** | `V6-d` 新增 4 条 fixture;四条共用 id ⇒ 归因靠 `detail` 前缀**精确匹配**(⛔ 前缀匹配会让 `CP-03` 命中) |
| **负面 ④ ⚠️ 下游 spec 层须同步** | `openspec/specs/cep-collective-plane-contract/spec.md`(**M1 冻结件**)须同步三项:`V6-c` Scenario 的**适用条件** + **新增 `V6-d` Scenario** + `status` 第二取值的判定语义 ⇒ 队列 #14 归档时走**两步授权**(本段**不**改它)。⚠️ **该 spec `:35` 的「声明最小集**恰含四组**」条款**不受影响** —— 本段**不新增声明组**(E.3.2)⇒ 组数仍为 4 |
| **负面 ⑥ ⚠️ 已落地运行期判定的单值锁定** | `evaluate_vc_reservation`(`include/tlm/cep/collective_plane_contract.hh:659-662`)对 `status != "unavailable"` **一律 FAIL** ⇒ **M-2 模式下 CP-03 必红** ⇒ 本段**必须**同步放宽该判定(取值域 = 2 值)+ schema 的 `status` 描述串(现文「空预留只允许声明 unavailable」)⇒ **不改则队列 #14 的目标场景无法闭合** |
| **负面 ⑤ ⚠️ 拓扑级开关 vs 逐 network 证书** | 一个 `status` 值承载**逐 (plane, network)** 的证书 ⇒ 声明源须自带适用清单(见 E.6)⇒ **语义负担**转移到声明面 |
| **风险** | 若将来 ArchForge 提供 ≥5 VC **且有硬件保真依据** ⇒ 应**回到 M-1**(E.8 第 4 条)⇒ 本段是**过渡解**,不是终态 |
| **负面 ⑦ ⚠️ 「恰含四组」当前是**纸面**条款** | 实测:仓内 `test/` 与 `scripts/test/` **无**任何「声明最小集组数 == 4」的机器断言(`test_cep_collective_plane_contract.cc` 只查 `additionalProperties == false` + `required.size() == 3`)⇒ 该条款**当前不由机器守门** ⇒ 本段「不新增声明组」的决定**不能**靠 CI 兜底 ⇒ 须由 **E.9 的「冲突已消解」勾选项**人工守门;⇒ ⛔ **不得**因此认为「加第 5 组无害」—— 危害是**写坏冻结 spec 条款**并使该条款降级为不可执行 |
| **可逆性** | 高:删 `status` 的第二取值 + 撤 `V6-d` + 恢复 `evaluate_vc_reservation` 的单值判定 ⇒ 回到 D.4 原文本(M-1 语义未变) |

### E.8 重访触发条件(追加第 4 / 5 条,前 3 条原文不改)

1. 拓扑提供 **≥5 VC**(原,不变);
2. 首次需要真实 fabric collective 流量(**已命中**,本段即其裁决);
3. `ADR-CEP-09` 定稿且给出**跨平面** VC 共享的无环性论证(原,不变);
4. **新增**:ArchForge 给出 **≥5 VC 的硬件保真依据**(真实 UALink 器件确有第 5 个 VC)⇒ 必须**回到 M-1** 并撤 `V6-d`;
   ⛔ 仅因「建模便利」扩 VC **不**构成触发(会**降低**保真度);
5. **新增**:通道依赖图的建图模型**引入跨 network 边 / 回边补全**(即 **P2** 被打破,`channel_dependency_graph.hh` 的
   `cross_net_edges` 计数器转为非零)⇒ E.4 的构造性论证失效 ⇒ **必须**重开本段并重做证书。

### E.9 Verification(前瞻判据 · 可勾选 —— **不是**已跑完的实测日志)

- [ ] `collective_planes.schema.json` 的 `status` `enum` = **2 值**(`unavailable` / `shared_no_isolation`)+ 描述串已改;且 schema 根级**不新增**任何声明组(保持 **4 组**语义)
- [ ] `acyclicity_certificate` **不存在**(证书是**四条判据**的结果,不是一个布尔自证字段)—— 若实现为布尔自证 ⇒ **判红**
- [ ] `evaluate_vc_precondition` 的 `status: unavailable` 分支**行为零变化**(队列 #13 的 4 条 fixture + 文件级 E2E 仍绿)
- [ ] ⭐ **`evaluate_vc_reservation` 的 `status` 值域判定已放宽为 `{unavailable, shared_no_isolation}`**
      (现文 `collective_plane_contract.hh:659-662` 对 `status != "unavailable"` 一律 FAIL ⇒ **不改则 M-2 必红**)
      ⇒ **正控**:`status=shared_no_isolation` 且 C-1′~C-4 全绿 ⇒ **CP-03 PASS**
      ⇒ **反 fixture**:`status` 取**第三值** ⇒ 必红(归因 `CP-03`,精确相等)
- [ ] **C-1′** 的反 fixture:在 `collective_planes.json` 注入 `collective_plane_vc_binding` 块(集合平面**自带独立绑定**)⇒ 必红(归因 `CP-03-VC-CERTIFICATE/C-1′`,完整前缀精确相等)
- [ ] **C-1′** 的反 fixture:某网实例间映射不一致却用 M-2 ⇒ 必红
- [ ] **C-2** 的反 fixture:只覆盖部分 `vc_id` ⇒ 必红;且判定**直接对 `fabric.json` 的 `vc_count`**(不依赖边表)
- [ ] **C-3** 的 in-flight 拒绝路径有运行期断言 + 必红 fixture
- [ ] **C-4** 的元测试能抓到「把 U-1 / U-2 标成已验证」
- [ ] `kContractRuleCount` 仍 == **6**(四条折进 `CP-03` 的 `detail` 前缀,不新增 `ContractRule`)
- [ ] **零 M1 冻结件改动**:本队列**可**改的声明面(`collective_planes.json` /
      `collective_planes.schema.json` —— 实测**不在** M1 冻结集)+ `collective_plane_contract.hh` /
      `collective_plane.hh` + 测试
      ⇒ **必须零 diff** 的 M1 冻结件 = `protocol_instance.schema.json` + `fabric.json`
      (⚠️ 本清单列的是「**本队列**可能触碰的 M1 冻结件子集」;**本 SU 自身的三份 ADR** 亦在 M1 集内
      ⇒ 走台账 **Entry #193** 的两步授权)
      ⇒ ⚠️ `protocol_instances.json` **非** M1 冻结件,但受 `cep_decl_alignment_check.sh` ⑥ 的
      **ArchForge 逐字段镜像闸**约束 ⇒ 同样**零 diff**(集合平面**不进**它 —— E.3.2)
      ⇒ ⚠️ **唯一**需两步授权同步的 M1 冻结产物 = `openspec/specs/cep-collective-plane-contract/spec.md`
      (范围见 E.7 负面 ④;**不含**其 `:35` 的「恰含四组」)
- [ ] **「恰含四组」冲突已消解**:`cep-collective-plane-contract/spec.md:35` 的「声明最小集 SHALL 恰含四组」
      ⇒ 本段**不新增声明组**(E.3.2)⇒ 组数仍为 4 ⇒ 该条款**不需修改**;归档时的 spec delta **不含**该条款
- [ ] `ADR-CEP-09` 已出窄化 SU(见该 ADR 的 2026-11-03 段)—— **否则「机制归 09」无授权依据**

### E.10 critic 第 1 轮(REJECT,2 BLOCKER)的修订对照

| # | critic 的 finding | 处置 |
|---|---|---|
| **B1** | `ADR-CEP-04` SU 的「由 `flow_support` 派生」规则被仓内 **6/7** 存量声明反证;且与 `INV-CEP-04-IC7` 矛盾;先例引用事实错误 | ⇒ **撤回**该派生规则,改为「候选集 + 判别依据 + 逐实例理由登记」(见 `ADR-CEP-04` 同日 SU) |
| **B2** | C-1′/C-2 的载体在 M1 冻结 schema 下**不可按字面实现**;`acyclicity_certificate` **无规范定义** | ⇒ **重构载体**:集合平面**不进** `protocol_instances.json`,改在 `collective_planes.json` 新增 `collective_plane_vc_binding`(E.3.2)⇒ 伪实例方案作废;`acyclicity_certificate` **删除**(证书 = 四条判据的判定结果,⛔ 不得是布尔自证) |
| **S1** | 构造性论证的**两条前提未写**;`IC1` 鉴别力被说窄 | ⇒ E.4 补 **P1 / P2** 前提表 + `IC1` 的**三项**鉴别力;**P2 被打破**登记为 E.8 第 5 条 |
| **S2** | `ADR-CEP-09` 的取代范围**不完整**(决策 4 与 §4 抓取点 5 原文未动) | ⇒ 该 ADR 的 SU 加**兜底声明** |
| **S3** | 「跨平面**边**」在当前模型里**不存在** | ⇒ 载体重构后,集合平面**不引入新顶点/新边** ⇒ 无跨平面边;改述见该 ADR 的 SU ① |
| **S4** | C-1′ 的**参照实例未钉**(两网各有不一致实例) | ⇒ E.3.3 钉死:参照集 = **两域并集**;**该网映射不一致 ⇒ 不得用 M-2** |
| **S5** | C-2 只**代理地**实现谓词(IC1 只在边表引用到某 VC 时才要求其被绑定) | ⇒ C-2 改为**直接对 `fabric.json` 的 `vc_count` 判定**;`IC1` 降为**良构性见证 / 正控** |
| **S6** | `vc_reservation_declaration` 是**拓扑级单值** ⇒ 作用域未钉 | ⇒ E.6 补**作用域**(逐 `(plane, network)` + 适用清单 + 明确排除项);E.7 登记「下游 spec 须同步」 |
| **S7** | C-3 标题与正文**不是同一命题** | ⇒ C-3 改为「**控制面与数据面互斥**」并写明拒绝对象 |
| **S8** | 探针一次性、不入库 ⇒ 结论不可复跑 | ⇒ E.4 补「可由 P1 + P2 + 两份边表 + `channel_dependency_graph.hh:552-568` 重推」 |
| **N1/N2/N3** | 实验表跨度 / 派生表「二值」措辞 / 边界三条缺「今天成立所依赖的事实」 | ⇒ E.4 表加**节选**说明;`ADR-CEP-04` 代价栏改「派生输入不承载选型所需信息」;`ADR-CEP-09` ③ 三条各补依赖事实 |

### E.11 critic 第 2 轮(REJECT,2 新 BLOCKER)的修订对照

| # | critic 的 finding | 处置 |
|---|---|---|
| **B3** | 新载体(根级第 5 组 `collective_plane_vc_binding`)与 **M1 冻结** `cep-collective-plane-contract/spec.md:35` 的「恰含四组」正面相撞;且 **E.9**(零冻结件改动)与 **E.7 负面 ④**(队列 #14 要改冻结 spec)**自相矛盾** | ⇒ **载体再次重构**(E.3.2):集合平面**不声明**自己的 VC 绑定 ⇒ **继承**既有实例的映射 ⇒ **零新声明组** ⇒ 「恰含四组」**不受影响**;**E.7 负面 ④** 的同步范围收敛为三项且**明写**「恰含四组不需修改」;**E.9** 补「冲突已消解」勾选项 + `protocol_instances.json` 的「非 M1 冻结件但受镜像闸约束」措辞 |
| **S-1** | E.6 原写「存在性由运行时判据 V6-a 强制,而非 schema 级强制」—— **不成立**:schema 的 `allOf/if-then` **已在 schema 级强制** | ⇒ E.6 改为「**schema 的 `allOf/if-then` + 运行时判据 V6-a 双重要求**」 |
| **N-1** | 跨仓域的出处引用失效(`authority_migrations[]` 现为**空数组**) | ⇒ E.3.3 改为「`derived_from.source_doc` + `repo_owned_instance_ids[]` 的**补集**」 |
| **N-3** | 四条判据共用一个 id ⇒ 反向 fixture 无法区分哪条失败 | ⇒ E.3.1 归因脚注补**子标记**约定(`[CP-03-VC-CERTIFICATE/C-1′]` 等)+ 完整前缀精确相等 |

### E.12 critic 第 3 轮(REJECT;B4 闭合,B3 有残影 + 2 个新 BLOCKER)的修订对照

| # | critic 的 finding | 处置 |
|---|---|---|
| **B3 残影 ×4** | 被否决的载体 `collective_plane_vc_binding` 仍活在 4 处规范文本里 ⇒ 实施方按 checklist 执行会**重新造出第 5 组** | ⇒ **E.9**(2 处:删「块有 schema 定义」+ C-1′ 反 fixture 改为「注入该块 ⇒ 必红」)、**E.7 可逆性行**(删「删 `collective_plane_vc_binding`」)、**`ADR-CEP-09` ⑤**(改为「继承 + 该块**不出现**,出现即必红」)全部改为**继承式**表述 |
| **BLOCKER-A** | E.3.2 收尾把 `collective_planes.json` / `collective_planes.schema.json` 写进「**不改**」清单,与同段 E.6 / E.9 相反 ⇒ **同一 SU 内二义** | ⇒ E.3.2 收尾:把这两个文件移出「不改」清单,并加 **⚠️ 例外(必改)** 段(`status` 第二取值)⇒ 零 M1 冻结件改动**仍然成立**(二者不在 M1 集内) |
| **BLOCKER-B ①** | 取代范围只列 D.4 的 `V6-c` 行,**未取代 `V6-a` 行** ⇒ 与已签批 ADR 冲突且未走取代流程 | ⇒ **取代范围**补「+ **D.4 的 `V6-a` 行**(单值 `status` ⇒ **双值**)」+ 一句「不取代则 M-2 模式下 CP-03 必红」 |
| **BLOCKER-B ②** | 已落地运行期判定 `evaluate_vc_reservation`(`collective_plane_contract.hh:659-662`)对 `status != "unavailable"` **一律 FAIL** ⇒ **M-2 模式必红** ⇒ 队列 #14 目标场景无法闭合 | ⇒ **E.7 新增「负面 ⑥」**登记该锁定 + 同步放宽要求;**E.9 新增勾选项**(2 值判定的正控 / 反 fixture) |
| **SHOULD_FIX** | ① C-1′ 的「全部既有实例」字面读法含 5 个**无绑定**实例 ⇒ 目标网络会被判不一致 ⇒ 不可用;② M1 冻结件清单未标注「本队列可能触碰的子集」+ 未提本 SU 自身三份 ADR | ⇒ ① C-1′ 措辞改为「全部**在该 network 上有 `vc_binding` 的**既有实例」;② E.9 加两句标注 |
| **NIT** | E.3.2 与 E.9 对 `protocol_instances.json` 是同一事实的两种说法 | ⇒ E.3.2 与 E.9 统一为「**非** M1 冻结件,但受 ⑥ 镜像闸约束 ⇒ 零 diff」 |

**关联**: 台账 **Entry #193**(授权记录)· #171(原 Decision D)· #179(`V6-c` id 唯一化)· #190(队列 #13 实现层归档)
· D.5(未采路径表 —— 本段选的是其路径 C)· `ADR-CEP-09` 2026-11-03 段(机制侧窄化 + `IC1` 鉴别力边界)
· `ADR-CEP-04` 2026-11-03 段(tracker 选型改为候选集 + 理由登记)· `ADR-CEP-09` SU 2026-11-02 §⑤(同型先例)
· `include/tlm/cep/channel_dependency_graph.hh`(E.4 的实测对象)· `include/tlm/cep/protocol_instance.schema.json`(E.3.2 的三条障碍)
· ArchForge `22-nsa-fabric-address-spec.md` §0.5.4.1 表 `:147`(VC 槽位语义;**本仓无机器可读副本** ⇒ 见 E.3.4 的 U-2 与本段 C-1′ 的依赖登记)

---

## Status Update 2026-11-04(#5)—— 队列 #14 **实现层**的落地订正(3 项,per 台账 Entry #209)

> **正文不改**(E.1 ~ E.12 与 §1 ~ §9 一律不改;本段**纯追加**)。
> ⚠️ **冻结面两步授权**:授权记录 = 台账 **Entry #209**(`--update --authorized-by "Entry #209"`)。

### ① E.9 的「**唯一**需两步授权同步的 M1 冻结产物」实为 **3 份**(订正)

E.9 的清单按**实施前**的预估写「唯一 = `openspec/specs/cep-collective-plane-contract/spec.md`」⇒ **实测为 3 份**:

| # | 冻结产物 | 落地原因 |
|---|---|---|
| 1 | `openspec/specs/cep-collective-plane-contract/spec.md` | **V6-d** + `status` 2 值(既有 Requirement 逐字不变 ⇒ 只 ADDED) |
| 2 | `openspec/specs/cep-collective-plane-base/spec.md` | 既有 Scenario「数据原语 SHALL 显式声明未实现」是**能力级**表述 ⇒ 与新增的**真可达** plane **直接矛盾** ⇒ 须**收窄到 `LocalCollectivePlane`**(标题逐字不变 ⇒ 场景集 1:1) |
| 3 | `docs/architecture/07-coherence-exploration-platform.md` | §5.1.1 UALink 行状态 + §11 Collective Plane 行 |

⇒ **判据读取口径**:「**唯一**」应读作「**至少**」⇒ 实施方**不得**据此假定只动 1 份。

### ② **C-3 的两级归因**澄清(声明级 ≠ 运行级)

E.3.1 的 C-3 行把 **可判形式**写作「运行期断言(fail-fast)+ 调用序列 fixture」⇒ 实现上分**两级**:

| 级 | 面 | 归因 id | 载体 |
|---|---|---|---|
| **声明级** | plane 是否**声明**承担 C-3(注册表 `enforces_control_plane_exclusion`) | `[CP-03-VC-CERTIFICATE/C-3]` | `evaluate_shared_mode_certificate` |
| **运行级** | group 有 op **in-flight** 时是否**真拒绝**控制面操作 | `UIA-04`(实现层**新建**的行为义务 id) | `FabricCollectivePlane` 的调用序列 fixture |

⇒ ⛔ 两级**不共用**一个 id(声明级是**证书判定**、运行级是**行为义务** ⇒ 归属不同面)
⇒ 归因匹配按**完整前缀精确相等**(声明级 `[CP-03-VC-CERTIFICATE/C-3]`;运行级 `UIA-04`)。

### ③ E.9 的 **C-1′ 反 fixture** 已落地(⚠️ **本条即该勾选项的落地记录** ——
E.9 清单内的 `- [ ]` **不改**(ADR 不可变),勾选状态以本 SU 为准)

E.9 的「在 `collective_planes.json` 注入 `collective_plane_vc_binding` 块(集合平面**自带独立绑定**)
⇒ 必红(归因 `CP-03-VC-CERTIFICATE/C-1′`,完整前缀精确相等)」⇒ **已落地**:
声明面 loader **检测**该块 ⇒ `evaluate_shared_mode_certificate` 的 C-1′ **结构面**判 **FAIL**
⇒ 影子 `collective_planes.json` 的反 fixture 必红且归因精确命中。
⚠️ **实现侧注记**:C-1′ 的**结构面**不依赖「plane 不得作为协议实例出现」(实例 id 的 schema 形态
`^[a-z0-9][a-z0-9_-]*$` 与 plane 名 CamelCase **不同形** ⇒ 该判定**恒假**)⇒ 结构面**只**认声明源块。

### ④ 归约的**整数边界**登记位置(补记)

`FabricCollectivePlane` 的归约算术**仅**在整数 datatype(`U8`/`U32`/`U64`)上进行;`F32`/`F64` 的 `REDUCE`
⇒ `Status::Unsupported`(**显式「不支持」**,非静默近似)⇒ 该边界登记在
`openspec/specs/cep-collective-plane-base/spec.md` 的 `cep-collective-plane-base-fabric-plane` Requirement
(⛔ 本段**不**改 E.1 ~ E.12 正文)。

**关联**: 台账 **Entry #209**(授权记录 + 落地订正)· #207(队列 #14 实现层归档)· #206(冻结面授权记录)
· #193(SU#4 的授权记录)· `ADR-CEP-04` 2026-11-03 段(选型候选集 + 逐实例理由登记)
· `include/tlm/cep/collective_plane_contract.hh`(V6-d + C-1′ 结构面)· `include/tlm/cep/fabric_collective_plane.hh`(C-3 运行级)
