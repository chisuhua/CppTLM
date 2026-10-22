# ADR-CEP-09: 多 VC 路由器仲裁与 VC 分配的死锁自由性论证

> **状态**: 🟡 起草中(v0.1)
> **日期**: 2026-10-21
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **多 VC 仲裁策略 + VC 分配的死锁自由性论证义务(两段式)**
> **关联真源**:
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §2.2(Fabric & Transport Plane)+ §5.1 + §8.3
> - 真源 1:`docs/architecture/06-inter-module-network.md` v0.4 §5.1(`simulation_mode`)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节
> - 调研:`docs/research/cep-coherence-protocol-space-survey.md` v0.4 §4.1.6(ProtoGen 局限 L4)+ §4.5.3(论文 §9.3.1)+ §5.6 F18/F26
>
> **一手锚点**:
> - 博士论文 §9.3.1:「ProtoGen **does not automatically assign messages to virtual networks yet for ensuring deadlock freedom**. We are currently working on adding this feature.」(**E2 —— Future Work,未实现**)
> - ProtoGen ISCA 2018 **p.7** 局限 L4:「it requires the user to **manually define virtual channels and assign messages to channels**」(E1)
> - 源码 `HieraGen/Algorithms/NetworkAnalysis/ClassNetworkClassification.py`:「# Automatic virtual channel assignment can be implemented here. **Possibly reuse model checker features**」(E1)
> - 源码 `include/bundles/noc_bundles_tlm.hh:38`:`ch_uint<8> vc_id;` 0-3(E1)

---

## 1. 背景

### 1.1 问题

`ADR-CEP-01 §3.3` 抓取点 3 已确立**跨平面**的通道隔离规则:集合流量走独立 VC,coherence 事务依赖的 fabric 资源不得被 collective 占用;并注明"路由器侧多 VC 仲裁**待 CEP-09 起草**"。

调研 F18 进一步揭示了 VC 分配的**真实目的**:

> 「ProtoGen does not automatically assign messages to virtual networks yet **for ensuring deadlock freedom**.」
> —— 博士论文 §9.3.1(**E2,Future Work**)

即:**VC 分配不是为了 QoS 调优,是为了保证死锁自由**。CEP 的 `Fabric & Transport Plane` 若只把 VC 当"带宽分桶",就丢掉了这个维度。

### 1.2 两个必须先厘清的边界

**(a) F18 的主语是"消息→VC 绑定",不是"路由器仲裁"**
原文是 "assign **messages** to virtual networks"。即绑定的决定权在**协议侧**(插件声明什么消息走哪条 VC),Fabric 只是**执行**。→ 义务是**三方联合**,不是 Fabric 独占(见决策 3)。

**(b) 论证必须发生在装配期,不是运行时**
`ADR-CEP-06` 的 `deadlock_violations` 是**运行时计数器** —— 它能**发现**死锁,**不能证明**无死锁。若把"死锁自由性"寄托在运行时,那只是"跑过没死锁"。→ 论证必须是**配置/装配期的静态检查**(决策 1)。

### 1.3 与 `ADR-CEP-08`(排序假设面)的关系

`ADR-CEP-08` 决策 6 定义了排序假设的**语义边界**(协议侧声明、逐 `vc_id`)。本 ADR 消费它,与 `vc_binding` 一起构成通道依赖图的输入。

---

## 2. 决策

### 决策 1:两段式结构 —— **Decision A(装配期静态论证)+ Decision B(运行时仲裁)**

| 段 | 交付物 | 时机 | 性质 |
|----|--------|------|------|
| **Decision A** | **通道依赖图 + 无环性判据** | **装配期**(配置加载 / 拓扑校验) | **证明**:满足判据 ⇒ 通道层面无死锁 |
| **Decision B** | 多 VC 仲裁策略 | 运行期 | **执行**:在 Decision A 的约束内分配带宽/信用 |

**Decision A 必须先于 Decision B**。理由:若先写仲裁策略而无环判据,"死锁自由"就退化为一句口号。

**依据**:源码 `ClassNetworkClassification.py` 的预留位注释「Automatic virtual channel assignment can be implemented here. **Possibly reuse model checker features**」(E1)—— 作者给出的实现路径正是"**复用模型检查器特性**",即**无环性/可达性分析**,属 Decision A 而非 Decision B。

### 决策 2:通道依赖图(Channel Dependency Graph)

**定义**:

| 元素 | 定义 |
|------|------|
| **顶点** | 每个 `(协议实例, vc_id)` 对;协议实例标识取自 `ADR-CEP-12` 的 `id` |
| **边** | `u → v` ⟺ 存在一类事务,其在 `u` 上**等待一个只有 `v` 能提供的资源或响应**才能完成(如"在 resp VC 上等 req VC 的 ack"、"在 data VC 上等 snoop VC 的 invalidation ack") |
| **来源** | 由协议实例的 `vc_binding`(消息类→`vc_id`)与 `ordering_assumption`(该 VC 是否保序)共同导出 |

**无环性判据**:图 **DAG** ⟹ **通道层面无死锁**。

**论证前提(必须显式声明)**:
1. 每个 VC 内部的消息**按发出序被处理**(VC 是 FIFO 队列)—— 这是 VC 存在的基本前提
2. 依赖关系是**静态**的(不随运行时状态改变)—— 若依赖随状态变化,静态 DAG 不再充分
3. 每个协议实例声明的 `ordering_assumption` 与实际 fabric 行为**一致** —— 声明与实现不符则判据失效

> ⚠️ **前提 2 是本 ADR 的已知局限**:某些协议的依赖关系是**状态相关**的(如"仅当 sharer 存在时才需要 invalidation ack")。这类依赖会使静态图**过近似**(保守),可能报出实际不存在的环。→ 处置:过近似导致**保守拒绝**(false negative 而非 false positive),并在报告中标注;不追求精确。

### 决策 3:三方联合契约(排序假设 + VC 绑定 + 仲裁一致性)

| 段 | 谁提供 | 产物 | 落在哪 |
|----|--------|------|------|
| **① 排序假设** | ProtocolEngine 插件**声明** | `ordering_assumption`(per-VC) | `ADR-CEP-12` schema |
| **② 消息→VC 绑定** | ProtocolEngine 插件**声明** | `vc_binding` + 通道依赖图 | 本 ADR Decision A |
| **③ 仲裁一致性** | Fabric & Transport **执行** | 仲裁策略须与依赖图一致 | 本 ADR Decision B |

**耦合契约文字**:

> 一个协议实例的 `ordering_assumption` 声明 + 其消息→VC 绑定,共同构成该实例的**通道依赖图**;本 ADR 须给出该图**无环**的可判定判据(装配期静态检查),并要求仲裁策略与该判据一致。**任一协议实例的 `ordering_assumption` 或 `vc_binding` 变更 ⇒ 依赖图重算 ⇒ 无环性检查重跑。**

**依据**:ProtoGen 局限 L4(E1,VC 绑定由用户手工定义);`ADR-CEP-08` 决策 6(E1,排序假设是协议规格的一部分)。

### 决策 4:多 VC 仲裁策略(Decision B)

- 仲裁策略本身**不是**本 ADR 的重点 —— 重点是它**不得破坏** Decision A 的无环性
- **约束**:仲裁策略**不得**在通道依赖图上引入新环(例如,让某 VC 的服务依赖另一 VC 的服务)
- **与 `ADR-CEP-01` 的关系**:跨平面 VC 隔离(coherence ↔ collective)已由 `ADR-CEP-01 §3.3` 抓取点 3 确立;本 ADR 是**平面内**的同类问题,**两者不重复**(coherence ↔ collective 是跨平面;VC 内部无环性是平面内)

### 决策 5:`INV-CEP-09-IC1` —— 装配期无环性检查为**必过项**

每个协议实例组合**必须**通过装配期无环性检查;不通过 = **配置 fail**,不得以"运行时注意"替代。

### 决策 6:`INV-CEP-09-IC2` —— 仲裁策略与依赖图一致

仲裁策略**不得**在通道依赖图上引入新环。

### 决策 7:`INV-CEP-09-IC3` —— 依赖传播契约

任一协议实例的 `ordering_assumption` 或 `vc_binding` 变更 ⇒ 依赖图**重算** ⇒ 无环性检查**重跑**。

### 决策 8:显式排除

| 排除项 | 理由 | 证据 |
|--------|------|------|
| **不承诺**自动 VC 分配 | CEP 不做协议生成器;学术侧该能力**至今未实现** | 博士论文 §9.3.1(E2,Future Work)+ `ADR-CEP-17` 决策 10 |
| **不承诺**穷尽死锁检测 | `deadlock_violations` 是运行时**发现**手段,不是**证明**手段 | `ADR-CEP-06 §3` 的 `verification_mode` 契约 |
| **不承诺**依赖图精确 | 状态相关依赖导致**过近似**;处置为保守拒绝 | 决策 2 前提 2 |
| **不重复** `ADR-CEP-01` 的跨平面 VC 隔离 | 本 ADR 是**平面内**问题 | `ADR-CEP-01 §3.3` 抓取点 3 |

---

## 3. 不变量

- **INV-CEP-09-IC1**:每个协议实例组合**必须**通过**装配期**通道依赖图无环性检查;不通过 = 配置 fail
- **INV-CEP-09-IC2**:多 VC 仲裁策略**不得**在通道依赖图上引入新环
- **INV-CEP-09-IC3**:`ordering_assumption` / `vc_binding` 变更 ⇒ 依赖图重算 + 无环性检查重跑

> **编号位说明**:`INV-CEP-09-IC1` ~ `IC3` 共 **3** 条。

---

## 4. 抓取点

1. **装配期无环性检查存在性**:拓扑校验 target **必须**执行通道依赖图无环性检查(per `INV-CEP-09-IC1`)
2. **依赖图与 schema 的耦合**:`ordering_assumption` / `vc_binding` 变更**必须**触发重算(per `INV-CEP-09-IC3`)
3. **`vc_id` 域**:`vc_id` 取值须 ∈ `noc_bundles_tlm.hh:38` 声明的 0-3(现有 Bundle 字段,已存在)
4. **禁止用运行时替代**:代码审查中若发现"依赖运行时计数判断死锁自由",驳回
5. **不重复 `ADR-CEP-01`**:跨平面 VC 隔离规则不在本 ADR 重复实现

---

## 5. 关联 ADR / 后续

| 项 | 状态 | 关系 |
|----|------|------|
| `ADR-CEP-01`(Collective 旁路平面)| ✅ v0.6 | 跨平面 VC 隔离(`§3.3` 抓取点 3);本 ADR 是**平面内**问题 |
| `ADR-CEP-06`(实验输出)| ✅ v0.3 | `deadlock_violations` 是**发现**手段;`verification_mode` 契约 |
| `ADR-CEP-07`(仿真语义 + seam)| ✅ v0.7 | 决策 9:插件声明 `ordering_assumption` + `vc_binding` |
| `ADR-CEP-08`(Ordering Point)| 🟡 **本批起草** | 排序假设面的**语义**边界 |
| `ADR-CEP-12`(能力映射表)| 🟡 **本批起草** | **提供输入**:`ordering_assumption` / `vc_binding` 字段 |
| `ADR-CEP-13`(一致性域归属)| 🟡 **本批起草** | 跨域桥接的并发粒度与保守度(per 决策 8 的关联) |
| `ADR-CEP-18`(协议生成路线)| 🟡 **本批起草** | "不自动生成 `vc_binding`" 的定位依据 |
| `ADR-CEP-09` 的 TileLink-C Tree Tip 映射 | 🟡 本 ADR 内 | per `NUMBER-MAP` 原议题(A5);**多 VC 仲裁**部分 |

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 两段式结构 | Decision A 装配期无环论证 + Decision B 运行时仲裁,**A 先于 B** | ✅ 已拍板 |
| 通道依赖图 | 顶点 = (实例, `vc_id`);边 = 跨 VC 等待依赖;**DAG ⟹ 通道无死锁** | ✅ 已拍板 |
| 三方联合契约 | ①排序假设 ②VC 绑定 ③仲裁一致性 | ✅ 已拍板 |
| 仲裁策略 | 不得在依赖图上引入新环 | ✅ 已拍板 |
| `INV-CEP-09-IC1` | 装配期无环性检查为必过项 | ✅ 已拍板 |
| `INV-CEP-09-IC2` | 仲裁与依赖图一致 | ✅ 已拍板 |
| `INV-CEP-09-IC3` | 依赖传播契约 | ✅ 已拍板 |
| 显式排除 | 自动 VC 分配 / 穷尽死锁检测 / 依赖图精确 / 重复 ADR-CEP-01 | ✅ 已拍板 |

---

## 7. 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
## 8. Status Update(2026-10-22):通道依赖图的顶点升为 `(协议实例, network, vc_id)`

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)——
> §2 决策 2 的「顶点 / 来源」两行与 §2 决策 3 表的「① 排序假设」行的**正文不改**,
> **自本段起以本段文本为准**。
> **触发**: ArchForge 侧 coherence 接口规约合入(commit `8290b4c`)后的跨仓对齐 ——
> VC 分配改为 **per-network 矩阵**(`ordering_assumption` / `vc_binding` 升嵌套形态)。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` Entry #9(`adr-amendment`)

> **兜底声明**:本 ADR **全文**中,凡出现「逐 `vc_id`」「`(协议实例, vc_id)`」等**扁平/单维**表述者
> (含 §1 关联 ADR 列表与 §1.3 与 `ADR-CEP-08` 的关系段),**一律以本段为准** ——
> per-network 化后,正确的对象是 **`(协议实例, network, vc_id)`**。

### 8.1 取代范围

> **取代范围**:§2 决策 2「顶点 / 来源 / 论证前提 3」行、§2 决策 3 表「① 排序假设」行 —— **取代后文本**见 §8.2。
### 8.2 取代后文本(规范)

**决策 2 · 顶点(取代后)**:

| 元素 | 定义 |
|------|------|
| **顶点** | 每个 **`(协议实例, network, vc_id)`** 三元组;协议实例标识取自 `ADR-CEP-12` 的 `id`,network 取自 fabric 网络命名空间(per `ADR-CEP-12` §8 `INV-CEP-12-IC2`) |

**决策 2 · 来源(取代后)**:

| 元素 | 定义 |
|------|------|
| **来源** | 由协议实例的 `vc_binding`(**逐网络**的消息类→`vc_id`)与 `ordering_assumption`(**逐网络 × 逐 `vc_id`** 的保序标注)共同导出 |

**决策 2 · 论证前提 3(取代后)**:

> 3. 每个 **`(协议实例, network)`** 声明的 `ordering_assumption` 与**该网络**的实际 fabric 行为**一致**
>    —— 声明与实现不符则判据失效

**决策 3 · ① 排序假设(取代后)**:

| 段 | 谁提供 | 产物 | 落在哪 |
|----|--------|------|------|
| **① 排序假设** | ProtocolEngine 插件**声明** | `ordering_assumption`(**逐网络 × 逐 `vc_id`**) | `ADR-CEP-12` schema |

### 8.3 为何这是**数学对象**的变更(不是措辞)

两张网(`compute_noc` / `ualink_fabric`)**共享同一 `vc_id` 值域 0-3**;图顶点若只带 `(协议实例, vc_id)`
⇒ **跨网依赖边被合并** ⇒ 无环判据可能给出**错误 verdict**(false negative 与 false positive 均可能)。
> ⚠️ **对 Stage 1 的硬前置**:在图顶点按 `(协议实例, network, vc_id)` 建立之前,**不得**用本 ADR 的无环判据放行任何协议实例。
>
> **已知局限不变**:§2 前提 2 的「状态相关依赖 → 静态图过近似」依然成立;但多网场景下**过近似方向不再单一**
> (合并边既可能制造假环,也可能掩盖真环)⇒ verdict 必须**逐 (network, vc) 标注**,不得只给全局结论。

**机器校验**:`scripts/test/cep_decl_alignment_check.sh` 第 ② 项**网络键对称性**
(每 network 必须**同时**出现在 `ordering_assumption` 与 `vc_binding`,per ArchForge `32 号 §6.2`)
+ 第 ③ 项(`ordering ⊆ vc_binding`,逐网络)。
