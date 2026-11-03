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
| V6-a | VC 预留现状恒真登记 | 声明层检查 · 队列 #5 已交付(`include/tlm/cep/collective_planes.json`) | ✅ **已存在**(既有 id `CP-03`);⚠️ **恒真** ⇒ 见 V6-b |
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
