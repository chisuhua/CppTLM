# 07 — Coherence Exploration Platform(CEP)一致性互联仿真平台

> **状态**: ✅ **v0.2 治理收口**(2026-11-11,per 台账 Entry #339)—— **总纲**(唯一入口)+ 9 篇 CEP 子文档(`cep/`);⛔ **本文不承载「逐实例 / 逐族」的实施状态**(那是台账 `§4.0-a` / `§6.2` / `§11` 的职责;§2 / §8.3 里残留的**交付物起草状态列**属文档自述,不是实施达成度);⛔ **本文不承载架构决策**(见 `docs/adr/ADR-CEP-*.md`)
> **作者**: CppTLM build agent(architect mode)
> **真源分级**: 本文件是 **真源 1**(通用基础架构,per AGENTS.md §DOC HYGIENE 真源原则)
> **关联真源 1**:
> - [`01-hybrid-architecture-v2.1.md`](01-hybrid-architecture-v2.1.md) v2.1.10 §1.1(6 层 L1-L6 仿真层模型 — **与本文 §2 CEP 6 层正交**)
> - [`06-inter-module-network.md`](06-inter-module-network.md) v0.4(模块互联网络 + Bundle 二分类 + Python 拓扑契约)
> - [`06-bis-coherence-as-network-extension.md`](06-bis-coherence-as-network-extension.md) v0.3(将被本系列 supersede,顶部加 Status Update 段)
> **关联真源 3**(10 份 ADR v0.6):
> - [`ADR-CEP-00`](../adr/ADR-CEP-00-platform-charter-and-governance.md)(跨仓治理 + KPI 排队)
> - [`ADR-CEP-00A`](../adr/ADR-CEP-00A-real-source-anchor-map.md)(真源章节映射)
> - [`ADR-CEP-01`](../adr/ADR-CEP-01-fabric-collective-plane.md)(旁路平面 + 完成域判别)
> - [`ADR-CEP-02`](../adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md)(canonical 10 流清单)
> - [`ADR-CEP-03`](../adr/ADR-CEP-03-fabric-collective-plane-internals.md)(Collective 内部 + SHARP R1-R5)
> - [`ADR-CEP-04`](../adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md)(Sharer Tracker D-5 重构)
> - [`ADR-CEP-05`](../adr/ADR-CEP-05-stub-cache-core-interface.md)(Stub Cache RN 投影)
> - [`ADR-CEP-06`](../adr/ADR-CEP-06-experiment-protocol.md)(实验输出协议)
> - [`ADR-CEP-07`](../adr/ADR-CEP-07-simulation-and-integration-semantics.md)(仿真语义 + 集成 seam)
> - [`ADR-CEP-NUMBER-MAP`](../adr/ADR-CEP-NUMBER-MAP.md)(编号预分配)
> - [`ADR-CEP-17`](../adr/ADR-CEP-17-ruby-slicc-protocol-family.md)(Ruby-SLICC 协议族接入 + 协议私有状态边界)
> **关联路线图**: [`phase-10`](../../.rddf/roadmap/phases/phase-10.md)+ [`objective-cep-platform`](../../.rddf/roadmap/objectives/obj-cep-platform.md)
> **关联跨仓**: ArchForge `09-coherence-protocol.md` + `microarchitecture/coherence-{protocol,bridge,domain}.md`(接口草案,本文仅引用)

> ## 📐 文件性质声明(**机器可读 · 批 3 拆分后**)
>
> **本文件 = 总纲(唯一入口)**。按层拆分的子文档在 [`docs/architecture/cep/`](cep/)。
>
> | 节 | 性质 | 实体位置 |
> |---|---|---|
> | **§4** invariant 总表 | **正文(定义处)** | ⛔ **就在本文件** —— 4 个消费方钉住(`cep_docs_sync_check.sh` / `test/cep_adr_parser.hh` / `test_cep_canonical_invariants.cc` / `ADR-CEP-00A §3.9`) |
> | §1 / §2 | 正文(导航图 + 6 层职责) | 本文件 |
> | §3 / §5 / §6 / §7 | **指针(非定义处)** | `cep/` 8 篇按层子文档(逐节桩首行写实体路径)+ **`cep/parity-baseline.md`**(H7 对拍基准规格 —— **不在 `§N` 体系内**,独立**验证手段**面) |
> | §8 | 正文(演进路线 + 治理不变量) | 本文件 |
> | §9 / §10 | 正文(列表) | 本文件 |
> | §11 / §12 | 指针桩 | 台账 / `docs/guide/cep-usage.md` |
| Status Update 附录 | **归宿指针**(决策 → ADR / 蓝图 → 正文 / 状态 → 台账) | 本文件(10 则全量 SU 已按台账 Entry #339 收敛) |
>
> ⛔ **引用纪律**:指 `§N` 时**以本文件为准**(节号体系不迁移);⛔ 不得在子文档另立节号。

---

## 1. 文档定位与真源关系

### 1.1 CEP 是什么

**Coherence Exploration Platform(CEP)** 是一个**协议中立的一致性互联仿真平台**,目标是让"用 CHI / CXL / TileLink-C / NVLink / UALink 中哪一个"这类**跨协议取舍问题**从"靠人脑推演"变成"可跑实验出数据"。

**框架级定位**(✅ 2026-10-20 拍板):**"支持做互相协议探索的"归入框架级,类比 gem5 Ruby network 做一致性探索** — gem5 从未把一致性探索做成 gem5 之外的项目,它是 gem5 框架能力;CEP 同理是 **CppTLM 框架能力**(per §1.4)。

它回答三类问题(per 平台目标):

| 类别 | 典型问题 | 依赖的 CEP 能力 |
|------|---------|----------------|
| **协议层** | snoop vs 目录 / 集中 Home vs 分布式 Home / 共享集跟踪粒度对延迟与带宽的影响 | §3 canonical 10 流 + §5 协议适配 + §4 守门 invariant |
| **互联层** | mesh / ring / tree、VC 数量、credit 深度、路由策略对一致性事务吞吐的影响 | §2 6 层骨架 + §6 仿真语义 |
| **目录层** | full-map / sparse / bloom / 分层目录对存储开销与误判流量的影响 | §5 Sharer Tracker 5 实现(caps 策略可插拔)|

### 1.2 与既有真源 1 的关系(不重发明)

**关键区分** — 两套 6 层是**正交**的,不是替代关系:

| 文档 | 6 层是什么 | 视角 |
|------|----------|------|
| **01 §1.1 / 06 §2.1** | L1 `tlm_generic_payload`+PacketPool / L2 MasterPort·SlavePort·PortPair / L3 ChStream / L4 StreamAdapter / L5 Bundle / L6 具体协议字段 | **仿真层 payload 传输模型** — 一次事务在 TLM 里怎么流动 |
| **本文 §2** | Core·TrafficGen / Stub Cache / Coherence Interface / Protocol Engine / Directory & Ordering Plane / Fabric Collective Plane / Fabric & Transport Plane | **一致性互联语义模型** — 一次一致性事务在协议意义上怎么流动 |

**关系**:01/06 回答"**怎么传**",本文回答"**传什么语义**"。CEP 的 L3(ChStream)+ L4(StreamAdapter)直接复用 01 §1.1 的既有机制,**不新造传输层**。

### 1.3 与 06-bis 的关系(supersede)

`06-bis-coherence-as-network-extension.md` v0.3 §7.3-§7.7 定义的 P2.0-P5 范围(CoherenceProtocol / CoherenceBridge / SnoopFilter / MesiGpuProtocol / CHI 完整协议 / IOMMU)被 CEP 完整继承。

**处置**(per `ADR-CEP-00 §3`):
- 06-bis 顶部加 `## Status Update` 段声明 supersede by CEP(正文**不动**,per AGENTS.md "ADR 不可变 + Status Update" 先例)
- 06-bis §5.2 `SnoopProbe` / §5.3 `SnoopRespBundle` / §5.4 `CoherenceMsg` / §2.2 跨仓同步契约 = **CEP 沿用**(不重发明)
- **P5 IOMMU 范围 reduction**:CEP 5 协议族**不覆盖** IOMMU 完整协议,需独立 ADR 签发 scope reduction 决策(per `ADR-CEP-00 §3` supersede 表 P5 行)

### 1.4 归口与治理(✅ 2026-10-20 拍板)

**定性**(用户决策):**"支持做互相协议探索的"归入框架级,类比 gem5 Ruby network 做一致性探索**。

**gem5 Ruby 类比**(实证,per `docs/research/gem5-coherence-protocols-survey.md:16-23` + `gem5-ruby-protocol-bridge-survey.md:43-48`):

| 维度 | gem5 Ruby | CppTLM CEP |
|------|-----------|-------------|
| 宿主框架 | gem5 | CppTLM |
| 仓归属 | **gem5 仓内** `src/mem/ruby/` | **CppTLM 仓内** `docs/adr/` + `docs/architecture/` + `src/tlm/cep/` |
| 协议可插拔机制 | SLICC `.slicc` 文件 | ProtocolEngine 插件 + 能力映射表 |
| 协议实现数 | 6+ 协议族(MESI_Two_Level / MESI_Three_Level / MOESI_CMP_directory / MOESI_hammer / MOESI_AMD_Base / GPU_VIPER)| **2 来源族 / 16 adapter 实例**(工业 6 族 8 实例 + Ruby 1 族 8 实例,per §5.1)|
| 目录 / 探测结构多样性 | 不同协议各自实现不同 `.sm` 组件(`-dir.sm` / `-probeFilter.sm` / token 机制),**非**同一 `.slicc` 内变体 | SharerTracker **6** 实现 + caps descriptor(§5.2)|
| 探索产出 | 协议 / 拓扑 / 目录对比实验 | 同左(per `ADR-CEP-06` 4 件套)|

> **关键含义**:gem5 从未把"协议一致性探索"做成 gem5 之外的项目 — **它是 gem5 框架能力**。CEP 同理:**是 CppTLM 框架能力**,不是 dGPU 业务扩展,不是独立项目。

**能力覆盖类比(2026-10-20 调研补充,per `ADR-CEP-17`)**:既然 CEP 被定性为"框架级一致性探索能力",它**应该**能覆盖 gem5 Ruby 能探索的协议空间。调研结论:

| 维度 | gem5 Ruby | CEP 现状(2026-10-20) |
|------|-----------|---------------------|
| 协议实例数 | 8 个独立 `.slicc` | §5.1.2 已纳入 8 个(Ruby-SLICC 族),6 直接支持 + 1 需扩展 + 1 排除 |
| 消息模型 | RubyRequest / RubyResponse / RubySnoopResponse | canonical 10 流 **100% 覆盖,零新增流** |
| 目录机制 | directory / token / probeFilter 三形态 | SharerTracker **6** 实现 + caps descriptor(token 归属见 §5.1.3)|
| 协议可插拔机制 | SLICC 代码生成 | ProtocolEngine 插件(能力映射表待 CEP-12)|
| 验证手段 | Ruby 随机测试 | `deadlock_violations` / `memory_violations` 闸(per `ADR-CEP-06`)|

> **调研源**:[`docs/research/cep-coherence-protocol-space-survey.md`](../research/cep-coherence-protocol-space-survey.md) **v0.3**(5 类 21 份源:ArchForge 09 + gem5 Ruby 3 篇 + *Gen 算法族 **PDF 原文 3 篇 + 178 页博士论文 PDF 原文** + wiki 12 条目;36 条引文已逐条回溯 PDF)

**归口**:

| 范围 | 归 |
|------|----|
| CEP 框架级能力(本文 §2~§7)| **CppTLM 真源 1**(本文件) |
| 10 份 CEP ADR(真源 3)| **CppTLM 仓**(`docs/adr/ADR-CEP-*.md`,per `ADR-CEP-00 §5.1` 签发仓 = CppTLM)|
| 全部 C++ 实现 | **CppTLM 仓**(per `ADR-CEP-00 §2 Decision 1` 代码归口)|
| 5 个具体协议的**语义建模设计文档** | **ArchForge 真源 2**(跨仓镜像,**可选**,不构成 ADR 签发依赖)|
| topology 探索研究报告 | **ArchForge**(可选)|

**AGENTS.md:423 边界条款不触发**:CEP 属"建模框架(CppTLM)"而非"SoC 架构组件/拓扑/演进类"(per `ADR-CEP-00 §2 Decision 4` 豁免论证)。

**跨仓准入门**:ArchForge 协议接口 PR 未合入前,CppTLM 对应 OpenSpec change 不得从 Proposed Draft 转 Proposed(per `ADR-CEP-00 §2 Decision 3` 单 DRI + 准入门)。**注意**:这是**实施顺序**约束,不是**归属**约束 — CEP ADR 与 07 号真源 1 都在 CppTLM 仓(per `ADR-CEP-00 §5.1`)。

---

## 2. 6 层 CEP 架构

### 2.1 分层图(per `ADR-CEP-01 §2 Decision 1`)

```
Core / Traffic Generator
        │
        ▼
   Stub Cache Core                    ← §3 RN 侧投影,协议无关
        │
        ▼
Coherence Interface Layer             ← §3 canonical 10 流,协议无关
        │
        ▼
Protocol Engine                       ← §5 CHI / CXL / TileLink / NVLink / UALink 插件
        │
        ▼
Directory & Ordering Plane    ∥(横向并列,非堆叠)
        │                            │
        ▼                            ▼
Fabric & Transport Plane  ←──  Fabric Collective / Sync Plane
                                      ↑
                                §2.3 旁路平面(仅此层与上一层的"目录"并列)
```

**关键结构特征**:
- **Directory & Ordering Plane** 与 **Fabric Collective Plane** 横向并列,**彼此零调用边**(per `ADR-CEP-01 §3.3` 抓取点 1,可机器检查)
- 两者都向下消费 **Fabric & Transport Plane** 的服务
- CHI / CXL 配置下 Collective Plane 绑 `NullCollectivePlane` 可关闭(per `ADR-CEP-03 §2 Decision 3`)

### 2.2 各层职责

| 层 | 职责 | 协议无关? | 关键约束 |
|----|------|----------|---------|
| **Core / Traffic Generator** | 合成流量 / workload trace replay / snoop storm | ✅ | 参数化:命中率、MSHR 容量、响应延迟(per `ADR-CEP-05 §5`) |
| **Stub Cache Core** | 缓存行状态(I/S/E/M)、MSHR、驱逐、写回、snoop 命中查询 | ✅ | 只认 cache line + 权限,**不认 CHI UC/SC/UD/SD** |
| **Coherence Interface** | 一致性事务语义抽象 | ✅ | **canonical 10 流**(§3),不新增端口 |
| **Protocol Engine** | 抽象事务 → 具体协议通道映射 + 协议状态机 + 排序规则 **+ 声明本协议的排序假设与消息→VC 绑定**(`ordering_assumption` / `vc_binding`,per `ADR-CEP-12` schema) | ❌ | 只做映射,**不维护目录 / 不维护缓存行状态**(per `ADR-CEP-04` / `ADR-CEP-07` 边界);**排序假设 + VC 绑定属协议侧输入**,非框架开关(per 调研 F25,一手依据 `.pcc` 的 `Network { Ordered fwd; ... }`)|
| **Directory & Ordering Plane** | 地址归属解析 / sharer 跟踪 / 排序点 / 响应聚合 | ✅ | Address Resolver + Sharer Tracker + Ordering Point 三子平面(per `ADR-CEP-04`) |
| **Fabric Collective / Sync Plane** | 集合通信原语(REDUCE / BROADCAST / MULTICAST / BARRIER)| ✅ | **旁路平面**;group-scoped 完成纪律(per `ADR-CEP-03`) |
| **Fabric & Transport Plane** | 拓扑 / credit / 路由 / 背压 / **VC 分配(首要用途 = 死锁自由性论证载体;QoS 为次要用途)** | ✅ | 不解析 MESI/MOESI/CHI 状态(per `ADR-CEP-01` ADR-G 原则);**执行**(而非单边承担)死锁自由性论证 —— 输入来自 Protocol Engine 的排序假设 + VC 绑定声明(per 调研 F18 / 博士论文 §9.3.1;一手:VC 分配主语是 "assign messages to virtual networks",属协议侧)|

### 2.3 完成域判别规则(单规则推导所有同类争议)

```
requester-scoped completion → Coherence flow
group-scoped completion     → Fabric Collective Plane
```

**判别基础**:平面相对通用性 — 字段 F 可进入平面 P 的共享接口,当且仅当所有映射进 P 的协议都能为 F 赋予语义(允许赋值为"不支持")。

**两条 litmus test**(barrier 归属,per `ADR-CEP-01 §3.1`):
1. barrier 的 key 是**地址**还是**参与者组**?地址 → Ordering;组 → Collective
2. 完成条件数的是**事务**还是**参与者**?数事务(drain)→ Ordering;数参与者(N 个到达)→ Collective

---

## 3. canonical 10 条 Coherence Interface 流

> ⚠️ **本节为指针,非定义处** —— 实体见 [`docs/architecture/cep/coherence-interface.md`](../cep/coherence-interface.md)。
> ⚠️ **本节的 3.1 实体在 `coherence-interface.md`;3.2 / 3.3 实体在 `stub-cache-core.md`。**

### 3.1 清单(per `ADR-CEP-02 §2`,首次定义)

### 3.2 Stub Cache Core 的 RN 侧投影(per `ADR-CEP-05 §2`)

### 3.3 缓存核心协议无关性

## 4. 67 编号位 / 66 条守门 invariant 总表(单一真源)

> **计数(三段式口径,per oracle 全面架构审查 N1 / N2 修订 + 2026-10-21 两批新 ADR)**:
> - **实际定义数**:**66** = 11 + 7 + 6 + 5 + 7 + 3 × 10(**15 组**,各组条数之和)
> - **编号位**:**67** = 66(实际定义) + 1(`INV-CEP-04-IC4` **永久跳空**,被提及但从未定义)
> - **已落 owner ADR**:**66** —— 全部 invariant 均在其 owner ADR 正文中定义
> - 变动轨迹:Stage 0 基线 36 编号位 → `CEN-Ib11` 37 → `ADR-CEP-08`/`09`/`12`/`13` 各 3 条 + `ADR-CEP-07-IC7` + `ADR-CEP-06-IC5` ⇒ 51 → **2026-10-21 `ADR-CEP-11`/`14`/`15`/`16`/`07A` 各 3 条 ⇒ 67**
>
> ⚠️ **计数口径纪律**:`INV-CEP-04-IC4` 是**唯一跳空**;它被多处**提及**(说明为何跳空)但**从未定义** —— 计数脚本**必须区分「提及」与「定义」**,否则会多算 1 条(per 2026-10-21 实测教训)
>
> ⚠️ 本表是 **Stage 0 基线**;后续 ADR 落地后需同步本表 + `objective-cep-platform DoD 7` + `improvement A2` + `phase-10` **四处**。

| 组 | 数量 | ID 范围 | 定义位置 |
|----|------|---------|---------|
| **Coherence Interface** | 11 | `CEN-Ib1` ~ `CEN-Ib11` | [§3.1](#31-清单per-adr-cep-02-2首次定义) |
| **Stub Cache Core** | 7 | `INV-SC1` ~ `INV-SC6` + `INV-CEP-05-IC1` | [§3.2](#32-stub-cache-core-的-rn-侧投影per-adr-cep-05-2) |
| **Sharer Tracker** | 6 | `INV-CEP-04-IC1/2/3/5/6`(IC4 跳空)+ `INV-CEP-04-IC7` | `ADR-CEP-04 §11` |
| **Experiment Protocol** | 5 | `INV-CEP-06-IC1` ~ `INV-CEP-06-IC5` | `ADR-CEP-06 §3` |
| **Simulation + Seam** | 7 | `INV-CEP-07-IC1` ~ `INV-CEP-07-IC7` | `ADR-CEP-07 §3` |
| **Numbering** | 3 | `INV-CEP-NUM-IC1` ~ `INV-CEP-NUM-IC3` | `ADR-CEP-NUMBER-MAP §3` |
| **Ordering Point** | 3 | `INV-CEP-08-IC1` ~ `IC3` | `ADR-CEP-08 §3` |
| **Multi-VC / Deadlock Freedom** | 3 | `INV-CEP-09-IC1` ~ `IC3` | `ADR-CEP-09 §3` |
| **Capability Map** | 3 | `INV-CEP-12-IC1` ~ `IC3` | `ADR-CEP-12 §3` |
| **Coherence Domain** | 3 | `INV-CEP-13-IC1` ~ `IC3` | `ADR-CEP-13 §3` |
| **Protocol Version Locking** | 3 | `INV-CEP-11-IC1` ~ `IC3` | `ADR-CEP-11 §4` |
| **Stub Contract Tests** | 3 | `INV-CEP-14-IC1` ~ `IC3` | `ADR-CEP-14 §3` |
| **CacheTLM Alignment** | 3 | `INV-CEP-15-IC1` ~ `IC3` | `ADR-CEP-15 §3` |
| **06 Evolution Mapping** | 3 | `INV-CEP-16-IC1` ~ `IC3` | `ADR-CEP-16 §3` |
| **06-bis Artifact Disposition** | 3 | `INV-CEP-07A-IC1` ~ `IC3` | `ADR-CEP-07A §3` |
| **合计** | **67 编号位 / 66 实际** | | ⚠️ IC4 唯一跳空;后续 ADR 落地后需四处同步 |

**Sharer Tracker 6 条**(per `ADR-CEP-04 §11`):
- IC1:caps struct 字段值必须 ∈ enum 字面值(机器可检查)
- IC2:Sharer Tracker 内部 lookup / snoop_begin / snoop_fold 幂等
- IC3:5 个条目生命周期事件名 + 时序契约在 **6** 种实现一致
- IC5:`fabric_ingress_classify` 中 "table 命中 + epoch invalid" 窗口 = 正常服务放行(per R4)
- IC6:**6** 种实现 hazard 行为契约 `Hazard_NONE` / `Hazard_STALL` / `Hazard_CHAIN` 字面值收敛
- (IC4 未定义 — 编号跳空,per oracle v0.5 复审实证:ADR-CEP-04 仅定义 IC1/2/3/5/6)

**Experiment Protocol 5 条**(per `ADR-CEP-06 §3`):
- IC1:每次实验产出 4 件套,缺一件 = fail
- IC2:`metrics.json` 含 13 个必纳字段
- IC3:`deadlock_violations == 0 && memory_violations == 0`,否则 = 一致性违例,fail
- IC4:不可观测 metric 标 `not-collected` + reason,禁止省略/0/-1
- IC5:`memory_violations` 的判定必须覆盖 **SWMR + data-value 双不变量**,且报告须能区分违例类型

**Simulation + Seam 7 条**(per `ADR-CEP-07 §3`):
- IC1:`simulation_mode` 顶层唯一维度,协议插件无 `network.mode`
- IC2:`NullProtocolEngine` 是 ProtocolEngine 默认实现,10 流覆盖矩阵
- IC3:`NullCollectivePlane` 是 Collective Plane 默认实现
- IC4:CEP 模块注册走真实机制(`ModuleFactory::registerObject<T>` + `REGISTER_MODULE(T)`),**不发明宏形式**
- IC5:CEP 不破 23 ABI / frozen headers / `[[deprecated]]` 状态
- IC6:functional 模式 `avg_latency_cycles == 0` 必须成立
- IC7:ProtocolEngine 插件接口必须为组合维度预留接缝(声明 `ordering_assumption` + `vc_binding`,不实现组合逻辑)

**Numbering 3 条**(per `ADR-CEP-NUMBER-MAP §3`):
- IC1:`CEP-NN` 编号预分配表是编号单一真源
- IC2:不变量 id 与 ADR 编号命名空间分离,跨 ADR 引用必须加 `INV-` 前缀
- IC3:A14 编号口径冻结为 `06 §15.4`(P2.0-P5,无 P6)

---

## 5. Bundle 与协议适配

> ⚠️ **本节为指针,非定义处** —— 实体见 [`docs/architecture/cep/protocol-engine.md`](../cep/protocol-engine.md)。
> ⚠️ **按层分流**:5.1 → `protocol-engine.md` · 5.2 → `directory-ordering.md` · 5.3 → `collective-plane.md` · 5.4 → `coherence-interface.md`。

### 5.1 协议族 → 协议实例 → adapter 映射

#### 5.1.1 工业硬件协议族(6 族 / 8 实例)

#### 5.1.2 Ruby-SLICC 协议族(1 族 / 8 实例,per `ADR-CEP-17 §2 Decision 2`)

#### 5.1.3 协议私有状态边界(per `ADR-CEP-17 §2 Decision 4`)

#### 5.1.4 协议语义实例(按一致性模型索引)(5 语义,per `ADR-CEP-17 §2 Decision 7`)

#### 5.1.5 「转移表真源」判定式(T1–T4)与来源效力

### 5.2 目录策略可插拔(Sharer Tracker 5 实现,per `ADR-CEP-04 §2`)

### 5.3 集合通信旁路平面(per `ADR-CEP-03`)

### 5.4 Bundle 约束

## 6. 仿真语义与集成 seam

> ⚠️ **本节为指针,非定义处** —— 实体见 [`docs/architecture/cep/simulation-and-experiment.md`](../cep/simulation-and-experiment.md)。
> ⚠️ **按层分流**:6.1 / 6.2 / 6.4 → `simulation-and-experiment.md` · **6.3 / 6.3.1(传输层装配 seam)→ `fabric-transport.md`**。

### 6.1 simulation_mode 单一维度(per `ADR-CEP-07 §2 Decision 1`)

### 6.2 两种模式的推进差异

### 6.3 ModuleFactory 集成 seam(per `ADR-CEP-07 §2 Decision 3`)

#### 6.3.1 「经网络」的定义与拓扑分档(J-9)

### 6.4 Null 实现(解耦测试兜底)

## 7. 实验输出协议

> ⚠️ **本节为指针,非定义处** —— 实体见 [`docs/architecture/cep/simulation-and-experiment.md`](../cep/simulation-and-experiment.md)。
> ⚠️ **本节全部实体**在 `simulation-and-experiment.md`。

### 7.1 4 件套(per `ADR-CEP-06 §2`)

### 7.2 13 个必纳 metric

### 7.2.1 实验验证深度契约(per `ADR-CEP-06 §3`)

#### 7.2.2 「全绿」的两级判定(run-validity / hypothesis-verdict)

### 7.3 sweep / A-B 工具

## 8. 演进路线与阶段边界

### 8.1 Stage 划分(per `ADR-CEP-00 §2 Decision 3` KPI 排队)

| Stage | 内容 | change 数 |
|-------|------|-----------|
| **Stage 0** | Umbrella 启动(契约测试 + experimental + tools + 顶层真源 + lint)| 1 |
| **Stage 1** | Stub Cache + Sharer Tracker + Collective Plane + **CohIf 载体** + 5 协议实例(RN-F subset / `moesi_gpu` / `mesif` / CXL 非对称 / TileLink-C) | **10** |
| **Stage 2** | CXL 对称 DCOH + NVLink SHARP | 2 |
| **Stage 3** | UALink In-Network Compute | 1 |
| **合计** | | **14** |

**KPI 约束**:任一时刻 Proposed 并发 ≤ 1(CEP 内部);全仓 ≤ 3(AGENTS.md 硬上限)。**单一时钟真源**:`ADR-CEP-00 §2 Decision 3`。

> **本表只承载「Stage 划分 + change 数」**(2026-10-23 精简,per 台账 Entry #27):
> - **时间跨度(排期)不在本表** —— 排期真源 = `.rddf/roadmap/objectives/obj-cep-platform.md` **§4.0 排队总表**。单源纪律:具体季度值只在该处出现;本文件此前复制的 5 处季度值**已移除**。
> - **估时不在本表** —— 各 Stage 估时见同一文件 **§4.1 ~ §4.4**(Stage 0 的 **46 人日**是逐行重算值;`ADR-CEP-00 §2 Decision 3` 记的 30 人日为**初始估**)。
> - 原表用的是**变更前**的 change 数与季度数(与 `ADR-CEP-00 §2 Decision 3` 冲突)—— **已按权威口径更正**,对照见台账 Entry #27。

**创建 umbrella 前须先 archive ≥1 个既有 active change**(per AGENTS.md line 422 新提案门槛)。**✅ 前置已满足(2026-10-20)**:3 个既有 active 全部 **⏸ 暂存归档**(`--skip-specs`,0% 未实施),当前 baseline **0/3** → `cpptlm-cep-umbrella` **可立即创建**(per AGENTS.md「⏸ 两种归档语义必须区分」)。

**Stage DoD 两条回指**(per `ADR-CEP-17 §2 Decision 9` + `ADR-CEP-06 §3`):
1. **组合能力不进 Stage 1-3 DoD** —— "分层 + 异构"组合**可做但走人工声明 + 实验证伪**,**不承诺自动化**;不得写入任何 Stage 的 DoD
2. **验证深度按 `verification_mode` 声明** —— 每次实验须声明 `exhaustive` / `bounded_random` / `sampled` 与 `verification_scope`;**不含**穷尽验证保证(per §7.2.1)

### 8.2 06-bis P2.0-P5 → CEP 承接映射

| 06-bis 阶段(per `ADR-CEP-00A §3.2` 实证) | CEP 承接 |
|---------------------------------------------|---------|
| **§7.3 P2.0** CoherenceProtocol C++ 实现 | `ADR-CEP-04 §6` + `ADR-CEP-03 §3.2` R3 |
| **§7.3 P2.1** CoherenceBridge C++ 实施 | `ADR-CEP-01 §2 Decision 1` programmatic bridge 路径 |
| **§7.4 P3** SnoopFilter directory-based | `ADR-CEP-04 §2` SharerTracker **6** 实现 |
| **§7.5 P3.5** JSON 端口路径打通 | 沿用 `06 §7` Python 拓扑 + ProtocolEngine 插件描述符 |
| **§7.6 P4** CHI 完整协议 | `ChiRnfAdapter` 完整实现(subset → full)|
| **§7.7 P5** IOMMU + 跨 vendor | **🟡 scope reduction 待签发独立 ADR**(CEP 5 协议族不覆盖 IOMMU 完整协议)|

> **章节号校准**:`06-bis §7.8` = "v0.3 总 KPI 调整 — 串行激活,并发 ≤ 1",**非阶段范围**(per oracle v0.4 B1 实证)。

### 8.3 后续演进 seam(per `ADR-CEP-NUMBER-MAP §2`)

| 编号 | 议题 | 阶段 |
|------|------|------|
| **CEP-07A** | 06-bis 既有产物处置(`coherence_bundles_tlm.hh` 创建时机 + `coherence_domains` JSON 字段)| Stage 1 前 |
| **CEP-08** | Ordering Point 可迁移性规则 | ✅ **v0.1 已起草 2026-10-21** | Stage 1~2 |
| **CEP-09** | TileLink-C Tree Tip 映射 + **多 VC 路由器仲裁 + VC 分配的**死锁自由性论证**义务**(per 调研 F18) | ✅ **v0.1 已起草 2026-10-21**(两段式:A 装配期无环判据 / B 运行时仲裁) | Stage 1~4 |
| **CEP-10** | 可观测性归属(横切 vs 顶层独立)| Stage 1 |
| **CEP-11** | 协议版本锁定(CHI / CXL / TileLink / NVLink / UALink 全族)| **Stage 1 前签发**(R2 缓解)|
| **CEP-12** | ProtocolEngine 插件能力映射表 schema | ✅ **v0.1 已起草 2026-10-21** | Stage 2 |
| **CEP-13** | 一致性域归属 | ✅ **v0.1 已起草 2026-10-21** | Stage 2 |
| **CEP-14** | Stub 可验证性合同测试 | Stage 1 |
| **CEP-15** | 与 CppTLM 现有 CacheTLM 对齐(A13)| Stage 1 |
| **CEP-16** | `06 §15.4` P2.0-P5 演进路线对应 | Stage 1 |
| **CEP-17** | Ruby-SLICC 协议族接入 + 协议私有状态边界(**已起草 v0.3**)| Stage 1 |
| **CEP-18** | 协议生成路线范围(SSP 描述 → 插件生成 vs 纯手写插件) | ✅ **v0.1 已起草 2026-10-21** | Stage 3 |
| **CEP-11** | 协议版本锁定(规范版本锁定表 = 唯一真源) | ✅ **v0.1 已起草 2026-10-21** | **Stage 1 前签发**(R2 缓解) |
| **CEP-14** | Stub 可验证性合同测试(C1~C10 + 负向断言) | ✅ **v0.1 已起草 2026-10-21** | Stage 0~1 |
| **CEP-15** | CppTLM CacheTLM 对齐边界 | ✅ **v0.1 已起草 2026-10-21** | Stage 1 前 |
| **CEP-16** | `06 §15.4` P2.0-P5 演进对应 | ✅ **v0.1 已起草 2026-10-21** | Stage 1 |
| **CEP-07A** | 06-bis 既有产物处置(7 项逐项) | ✅ **v0.1 已起草 2026-10-21** | Stage 0~2 |

**演进 seam 的外部证据锚点**(per 调研 v0.3,`docs/research/cep-coherence-protocol-space-survey.md`):
- **CEP-08** 需采纳 *Gen 族的"唯一序列化点"不变量 + "E 态静默升级"兼容性检查**;且 Ordering Point **不绑定"目录"这一物理实体**(论文 §9.3.4:总线上也可作为序列化点)
- **CEP-09** 需承载"**VC 分配须论证死锁自由性**"(论文 §9.3.1:VC 分配的目的不是 QoS 调优;源码 `ClassNetworkClassification.py` 已预留实现位并提示"**复用模型检查器特性**")
- **CEP-06** 需写入**实验规模上限**:*Gen 族死锁测试极端配置需 **1TB RAM + 数小时**(源码 `HeteroGen/README.md`)→ 不得承诺"任何配置都能穷尽验证"
- **CEP-12** 能力映射表**二维化**:协议族/实例 × 组合能力(concurrent / hierarchical / heterogeneous),并承载 `ordering_assumption`(**逐网络映射**,per 源码 `.pcc` 的 `Network { Ordered fwd; Unordered resp; ... }`)+ `consistency_labels`(per 决策 8)
- **CEP-13** 须采纳复合一致性公理 + "全局可见写"静态判据,并**显式声明"分层 + 异构组合可做但走人工声明 + 实验验证,不承诺自动化"**(per `ADR-CEP-17 §2 Decision 9`)

### 8.4 Kill / Replan 判据(per `objective-cep-platform §8.2`)

| 触发 | 处置 |
|------|------|
| **Kill** | Stage 0 验收发现 4 件套无法产出可比较数据 | 终止 CEP,归档 objective,回退 06-bis 路线 |
| **Pivot** | Stage 1 中期发现协议族选型需变 | §11 `scope-change` entry + 重排 §8.1 |
| **Replan** | 任一 Stage 延期 >2 季度,或全仓 Proposed 连续 2 季度无法为 CEP 腾 slot | §11 `deferral-rationale` entry + 重排季度列 + 评估拆 phase-11 |
| **Priority 仲裁** | CEP 与 phase-9 同时需唯一 slot | 默认 **phase-9 优先**(业务交付 > 探索平台)|

---


### 8.5 文档层治理不变量

**① 跨仓锚点的两类处置(⛔ 不可混用)**：

| 锚点类型 | 处置 | 理由 |
|---|---|---|
| **散文引用**(如对齐清单里的 `ADR-CEP-02:55`) | 改为**节号引用**(如 `ADR-CEP-02 §X`) | 无机器校验,纯给人看 ⇒ 行号漂移**无感知** |
| **机器锚点块**(`(file, line, symbol)` **三元校验**) | **保留行号不动** | 设计意图 = fail-loud tripwire(故意让漂移立刻炸);且「正文不改 + SU 只追加末尾」不变量下**追加永不移动既有行号** |

**② 治理不变量(强制)**:**凡在 ADR 既有行之前插入内容的编辑 = 立即破坏跨仓对齐锚点 = 禁止**。
⇒ ADR 的编辑**只允许**两种形态:**「正文就地澄清(零行数变化)」** 或 **「末尾追加 SU」**。

**③ 决策块计数口径**:**规范载体 = [`ADR-CEP-00`](../adr/ADR-CEP-00-platform-charter-and-governance.md) 的对应 Status Update §⑤**(治理不变量)⇒ 本节只作**镜像指针**,⛔ **不复述口径**(per 台账 Entry #338;该 ADR 另登记了「本口径与 `test/cep_adr_parser.hh` 的标题口径不一致」这一**已知待办**)。

## 9. Primary ADRs

| ADR | 决策主题 |
|-----|---------|
| [ADR-CEP-00](../adr/ADR-CEP-00-platform-charter-and-governance.md) | 跨仓治理 + 归口 + KPI 排队 + AGENTS.md:423 豁免 |
| [ADR-CEP-00A](../adr/ADR-CEP-00A-real-source-anchor-map.md) | 真源章节映射 + 14 条伪引清单 |
| [ADR-CEP-01](../adr/ADR-CEP-01-fabric-collective-plane.md) | Fabric Collective 旁路平面 + 完成域判别规则 |
| [ADR-CEP-02](../adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md) | canonical 10 流清单 + barrier 流语义 |
| [ADR-CEP-03](../adr/ADR-CEP-03-fabric-collective-plane-internals.md) | Collective 内部接口 + SHARP R1-R5 + 跨平面契约 |
| [ADR-CEP-04](../adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md) | Sharer Tracker D-5 重构 + caps descriptor |
| [ADR-CEP-05](../adr/ADR-CEP-05-stub-cache-core-interface.md) | Stub Cache RN 投影 + 折叠映射表 + 10 条契约测试 |
| [ADR-CEP-06](../adr/ADR-CEP-06-experiment-protocol.md) | 实验输出 4 件套 + 13 必纳 metric |
| [ADR-CEP-07](../adr/ADR-CEP-07-simulation-and-integration-semantics.md) | simulation_mode + ModuleFactory 真实注册 + Null 实现 |
| [ADR-CEP-NUMBER-MAP](../adr/ADR-CEP-NUMBER-MAP.md) | CEP-07A ~ CEP-16 编号预分配 + 命名空间分离 |
| [ADR-CEP-17](../adr/ADR-CEP-17-ruby-slicc-protocol-family.md) | Ruby-SLICC 协议族接入 + 协议私有状态边界(token / 瞬态态)+ 学术协议族 + 组合承诺边界 |
| [ADR-CEP-08](../adr/ADR-CEP-08-ordering-point-portability.md) | Ordering Point 定义 + 可迁移性三判据 + E 态静默升级检查 + 排序假设面边界 |
| [ADR-CEP-09](../adr/ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md) | 通道依赖图无环判据(装配期)+ 多 VC 仲裁(运行时),两段式 |
| [ADR-CEP-12](../adr/ADR-CEP-12-protocol-engine-capability-map.md) | 协议实例能力 / 前提假设 / 组合能力的结构化声明 schema |
| [ADR-CEP-13](../adr/ADR-CEP-13-coherence-domain-ownership.md) | 一致性域划界 + 跨域组合判据(复合一致性 / 域可见写 / 桥接粒度)+ 组合承诺边界 |
| [ADR-CEP-18](../adr/ADR-CEP-18-protocol-generation-scope.md) | 协议生产路线边界(纯手写插件)+ 竞合定位 + 引入生成路线的切换判据 |
| [ADR-CEP-11](../adr/ADR-CEP-11-protocol-version-locking.md) | 规范版本锁定表(唯一真源)+ 升级三路径 + 装配期校验 |
| [ADR-CEP-14](../adr/ADR-CEP-14-stub-contract-tests.md) | Stub 合同测试边界 + C1~C10 清单 + **负向断言** + 组合接缝两阶段 |
| [ADR-CEP-15](../adr/ADR-CEP-15-cachetlm-alignment.md) | 与既有 `CacheTLM` 的边界(共存不替换)+ 冻结面 + adapter 未来路径 |
| [ADR-CEP-16](../adr/ADR-CEP-16-06-evolution-route-mapping.md) | `06 §15.4` P2.0~P5 → CEP Stage 逐项映射 + P6 显式排除 |
| [ADR-CEP-07A](../adr/ADR-CEP-07A-06bis-artifact-disposition.md) | 06-bis 7 项产物逐项处置 + **协议泄漏通用禁令** |

---

## 10. References

- **真源 1**:
  - [`01-hybrid-architecture-v2.1.md`](01-hybrid-architecture-v2.1.md) v2.1.10 §1.1(6 层 L1-L6 仿真层)
  - [`06-inter-module-network.md`](06-inter-module-network.md) v0.4(模块互联网络 + Bundle 二分类)
  - [`06-bis-coherence-as-network-extension.md`](06-bis-coherence-as-network-extension.md) v0.3(将被 supersede)
- **真源 3**:`docs/adr/ADR-CEP-*.md` 10 份 + `docs/adr/ADR-DGPU-06/08/09/11/12` + `docs/adr/ADR-X.1-transaction-id.md`
- **路线图**:`.rddf/roadmap/phases/phase-10.md` + `.rddf/roadmap/objectives/obj-cep-platform.md` + `.rddf/improvements/cpptlm-cep-umbrella.md`
- **跨仓(ArchForge)**:`09-coherence-protocol.md` + `microarchitecture/coherence-{protocol,bridge,domain}.md`(仅引用,**0 改**)
- **代码真源**:
  - `include/core/module_factory.hh:122`(`registerObject<T>`)/ `:135`(`is_base_of_v<SimModule, T>` 断言)
  - `include/modules_cluster.hh:41-105`(`REGISTER_MODULE(T)`)/ `include/modules.hh:30`(`REGISTER_OBJECT` 无参)/ `include/chstream_register.hh:50`(`REGISTER_CHSTREAM` 无参)
  - `include/core/chstream_module.hh:73`(`advance_cycle()`)
  - `include/bundles/noc_bundles_tlm.hh:38`(`vc_id` 0-3)/ `:39`(`flit_type`)/ `:45`(`flit_category` 注释)/ `:60-62`(`CATEGORY_REQUEST/RESPONSE/CREDIT`)
  - `include/bundles/cpphdl_types.hh`(`BUNDLE_FIELD_WIDTH` 编译期锁)
- **调研附属**:
  - [`docs/research/cep-coherence-protocol-space-survey.md`](../research/cep-coherence-protocol-space-survey.md) **v0.4**(CEP 协议空间调研:ArchForge 09 + gem5 Ruby + *Gen 算法族**PDF 原文级 + 源码级**;含 28 条发现 F1~F28,**经 Oracle 独立架构复审**)
  - `docs/research/gem5-ruby-protocol-bridge-survey.md` + `gem5-messagebuffer-network-interface.md` + `gem5-garnet-network-survey.md` + `gem5-soc-survey.md`

---

## 11. Gap → 实现状态映射

> **本节已迁出**(per 台账 Entry #333 架构文档治理):
> 落地状态的唯一真源 = **`objective-cep-platform §4.0` 排队总表**(队列位状态)+ **`objective-cep-platform §4.0-a` 协议实例状态表**(实例级状态)+ **`objective-cep-platform §11` 决策台账**(残余缺口与欠账登记)。
>
> **架构文档不承载实施状态** —— 本节原表(能力 → 实现状态 → 承接 change)是**实施视图**,不属于蓝图。
> 能力清单本身仍在 **§2 / §3 / §5.1** 定义,本节不重复。
## §附录 · Status Update 2026-10-28(第八则 · per 台账 Entry #88 —— JSON 装配连通性落地 + `INV-CEP-07-IC4` 闭合 + egress 通道语义登记)

> ⛔ **决策正文见 [`ADR-CEP-19`**](../adr/ADR-CEP-19-*.md)的对应 Status Update**(egress 通道语义 / JSON 装配连通性 / 钩子死→活)。
> 本节原为实施状态登记,per 台账 Entry #333 架构文档治理已移除;**状态见 `objective-cep-platform §4.0` / `§4.0-a` / `§11`**。

## 附录 · Status Update 2026-10-29(第十二则 · per 台账 Entry #96 —— 队列 #9 的 R-1 裁决落地)

> ⛔ **决策正文见 [`ADR-CEP-04`**](../adr/ADR-CEP-04-*.md)的对应 Status Update**(`HierarchicalDirectory` 第 6 种 SharerTracker 实现 / R-1 裁决)。
> 本节原为实施状态登记,per 台账 Entry #333 架构文档治理已移除;**状态见 `objective-cep-platform §4.0` / `§4.0-a` / `§11`**。

## 附录 · Status Update 2026-10-30(第十六则 · per 台账 Entry #106 —— **S1-3 触发后的真实义务**结论)
> **收口指针**(per 台账 **Entry #339**):① **SLICC 式备选重评**结论(维持 per-network 嵌套形态)+ 触发条件集合 → **`ADR-CEP-12` 的 `Status Update 2026-11-11` ①**;备选的**选项对比表**原文 → `ADR-CEP-12 §8.4`(未改动);
> 　⚠️ **该对比表缺失的一格**(oracle 复核补回):**C-4 双向映射** —— 现行形态 = **两段(声明 + fabric),判据已闭合**(SLICC 侧为三段;`ADR-CEP-12 §8.4` 只载 SLICC 侧优点);
> ② 「承接项 2」滚动复核(第 2 次)→ **台账 `objective-cep-platform §9.4` ②**;③ 残余风险(R-1 / R-3 / 残留 A / 残留 C / E-1~E-6)**不得写成「已消除」** → **`Entry #106 §⑤`** + **台账 `§9.4` ③**;
> ④ 残余风险的另一半(按档批量 14 格证据强度弱于逐格 / 写回事实在 4 值 `perm` 上不可分辨)→ **台账 `§9.4` ③** + **`Entry #105` 的相关小节**(⛔ **不是** `Entry #105 §③` —— 那节是「计划外冻结面事实 + 3 条假绿来源」)。
> ⛔ 本则原文不回改;**段内锚点**(一 / 二 / 三)以本指针的对应条目为准。

## Status Update(2026-11-01,per 台账 Entry #131)—— §5.1.1 **CXL 行**的措辞对齐(裁决 ③⑦)+ `cxl_2_0` 实例登记
> **收口指针**(per 台账 **Entry #339**):① CXL 2.0 行的两处措辞冲突的**裁决依据** → **`ADR-CEP-11` 的 `Status Update(2026-11-01,per 台账 Entry #122)` 的 ③ 与 ⑦**(真源 3 权威);
> 　⚠️ **诚实边界**:正文 §5.1.1 的该行**仍保留未订正原措辞**(实体 = [`cep/protocol-engine.md`](cep/protocol-engine.md) §5.1.1 的 CXL 2.0 行)⇒ **措辞对齐是未结义务**,见台账 **`Entry #126 ⑥`**(⛔ 不得读作「已对齐」);
> ② 协议族计数不变(6 族 / 8 实例)→ **正文 §5.1.1**;
> ③ `cxl_2_0` 实例登记 + ④ 后果 / 代价(实施状态)→ **台账 `objective-cep-platform §4.0-a` 协议实例状态表** + **Entry #131**。
> ⛔ 本则原文不回改;**段内锚点**(①~④)以本指针的对应条目为准。

## Status Update(2026-11-02,per 台账 Entry #154)—— §5.1.1 / §5.4 / §11 的 **5 处口径订正** + **对称模式的 adapter 形态裁决**
> **收口指针**(per 台账 **Entry #339**):① 事实复核(公开来源核对;S4 = *An Introduction to the CXL Interconnect*,ACM CSUR 2024)→ **[`docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md`](../research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md)**(未改动);
> ② 正文订正 6 处 → **正文 §5.1.1**(实体 = `cep/protocol-engine.md` §5.1.1)+ **正文 §3.1 的流清单表**(实体 = [`cep/coherence-interface.md`](cep/coherence-interface.md) §3.1 第 7 行 `back_invalidate` —— `BISnp` 方向订正在此,⛔ **不是** §5.4;§5.4 是 Bundle 约束);
> ③ **对称模式的 adapter 形态裁决**(A = 独立载体 `CxlDcohAdapter`,⛔ 不扩 `CxlCacheAdapter`)→ **`include/tlm/cep/cxl_dcoh_adapter.hh` 头注释「⚠️ 为什么是独立 adapter」**(逐字给出 B / C 的否决理由:载体选择逻辑 ⇒ 破坏结构性保证)+ **台账 `Entry #154` ① 表行 4**;⚠️ `ADR-CEP-04` 的 `Status Update(2026-11-02)` 承载的是**聚合点二元**裁决(同 Entry #154 ① 表行 1),⛔ **不是**本项;
> ④ 后果 / 代价 + ⑤ Verification(8 项)→ **台账 `objective-cep-platform §11`** + **`Entry #167 §③`**。
> ⛔ 本则原文不回改;**段内锚点**(①~⑤)以本指针的对应条目为准。

## Status Update 2026-11-04 —— Stage 3 收口的 **5 项架构级裁决**(B3 口径拆分 · 3 项声明假设处置 · `FullMapDirectory` 触发条件 · 跨仓锚点治理不变量 · MOESI-GPU 形态登记)
> **收口指针**(per 台账 **Entry #339**):① DoD B3「一词两义」拆两行 + **⛔ 防自伪条款** → **台账 `objective-cep-platform §6.2`**(⚠️ 本条 ① 的**现行载体**即该节 —— 台账 `§6.2` 引用「`07 号` SU 2026-11-04 ①」时请按本行解析,避免绕圈);
> ② 声明层 3 项建模假设处置 → **`ADR-CEP-12` 的 `Status Update 2026-11-11` ②**(⚠️ 该 SU 的**实测口径已更正**:分母 = **8 实例** · `writeback` 是 **`flow_support`** 成员 · `combination_capabilities` 三维全 true = **7/8**);
> ③ `FullMapDirectory` 的显式触发条件 → **`Entry #215` ① 表行 ⑤**(裁决)+ **`Entry #216` ① 表行 ⑤**(落地,载体 `include/tlm/cep/sharer_tracker.hh`)+ **台账 `§11`**;⚠️ ⛔ 不是 `Entry #215` 顶层 `**⑤ 关联**`;
> ④ 跨仓锚点治理不变量(散文引用节号化 / 机器锚点保留行号 / ADR 编辑禁令)→ **`ADR-CEP-00A` 的 `Status Update 2026-11-11` ①**;
> ⑤ `MoesiGpuProtocolEngine` 形态限制 → **`ADR-CEP-12` 的 `Status Update 2026-11-11` ③**(⚠️ 该 SU 的**实测口径已更正**:11 个 `override` = 10 流回调 + 1 析构 · `on_address_resolve` 另写 home_node · **已注册** + **已有 JSON 拓扑**);
> ⑥ Verification 的**两条未勾项** → 已分别由 **台账 `§6.2`**(B3 未达成部分的触发条件)与 **`Entry #216` ① 表行 ②** 闭合。
> ⛔ 本则原文不回改;**段内锚点**(①~⑥)以本指针的对应条目为准。

## Status Update 2026-11-04(#2)—— 实验输出契约的**可执行化**(C1)+ ADR 一决策一文档化的**残留登记与触发条件**(C2)
> **收口指针**(per 台账 **Entry #339**):① C1 实验输出契约可执行化 → **`ADR-CEP-06` 的 `Status Update 2026-11-04(#1)`**(决策 4 / 决策 5 的可执行化裁决);
> ② 决策块计数口径 → **`ADR-CEP-00` 的 `Status Update 2026-11-11` §⑤**(**规范载体**;⚠️ 该 SU 自陈本计数口径与 `test/cep_adr_parser.hh` 的标题口径**不一致**,为已登记待办);
> ③ C2「一决策一文档化」残留 + T1 / T2 / T3 可判定触发条件 + D1 预定义路径 → **`ADR-CEP-00` 的 `Status Update 2026-11-11` ①~④**;
> 　⚠️ **该处 D0 堵法 ③ 的「理由」原文补回**(oracle 复核发现无归宿):**「触发时反正要动它们,现在加只是多 3 次冻结基线重算,零收益」**;
> 　⚠️ **D0 堵法 ② 的机器检查尚未落地**(`ADR-CEP-00` 同 SU 已登记为待办),且其原始规格是「断言 **`07 号`** 含 3 份 ADR 的残留登记 + 3 条触发条件」⇒ **收口后该规格必假**,实现时须改指 **`ADR-CEP-00` 的 `Status Update 2026-11-11` ①~④**;
> ④ 正文勘误指针表 → **`ADR-CEP-06` 的 `Status Update 2026-11-04(#1)` §⑥** + **台账 `Entry #218`**;
> ⑤ Verification → **台账 `objective-cep-platform §11`**。
> ⛔ 本则原文不回改;**段内锚点**(①~⑤)以本指针的对应条目为准。

## Status Update(2026-11-06,per 台账 Entry #238)—— 「转移表真源」的**判定式(T1–T4)** + **TileLink-C 阻塞解除** + 逐族重判

> **收口指针**(per 台账 **Entry #339**):② 判定式 **T1–T4** + ⑤ T3 普查三要素 → **正文 §5.1.5**(实体 = [`cep/protocol-engine.md`](cep/protocol-engine.md) §5.1.5,per 批 2);
> ① 根因 + ③ 逐族重判(达成度与闸评估分列)+ ④ TileLink-C 阻塞解除 → **台账 `objective-cep-platform §4.0-a` / `§11`** + **Entry #238**;
> ⑥ 后果 / 代价 + ⑦ Verification → **台账 `objective-cep-platform §11`**。
> ⛔ 本则原文不回改;**段内锚点**(①~⑦)以本指针的对应条目为准。

## Status Update(2026-11-06 · 四,per 台账 Entry #249)—— 「转移表真源」的**来源效力**与 **B3b 计入口径**
> **收口指针**(per 台账 **Entry #339**;⚠️ 本则的 **`D-1` / `D-2` / `D-3` / `D-4` 标签被全仓 ≥14 处引用**(台账 / ADR / improvements / specs),⛔ 故在指针中**逐条保留标签** —— 否则这些引用在 07 号内全部落空):
> ⓿ 范围 + ① 问题 + ③ 判据纪律 → **正文 §5.1.5**(实体 = [`cep/protocol-engine.md`](cep/protocol-engine.md) §5.1.5);
> ② 裁决 = **`D-1` 来源效力分档** → **正文 §5.1.5**(⚠️ 该分档原判为 **A 类**,经裁定表 §十二 ② **修订为 E 类** ⇒ ⛔ **不落 ADR**);
> 　· **`D-2` 两轴达成 + 载体形态分类** → **正文 §5.1.5** + **`Entry #257` ②**(命题切分 + 载体覆盖账 + carve-out + 例外);
> 　· **`D-3` 逐族终态** → **台账 `objective-cep-platform §6.2` 的 B3b-1 / B3b-2** + **`Entry #249` ①**;
> 　· **`D-4` 升级路径(4 条)** → **`Entry #249` ①** + **`Entry #238` ⑤**(reason / trigger_condition / ledger_entry 三字段);
> ④ 对 DoD 的影响(**五选项 甲~戊**)→ **`Entry #249` ⑤**;⑤ 后果 / 代价 + ⑥ Verification → **台账 `§6.2` / `§11`**。
> ⛔ 本则原文不回改;**段内锚点**(⓿~⑥ 与 `D-N`)以本指针的对应条目为准。

## Status Update(2026-11-06 · 五,per 台账 Entry #257)—— D-1「来源效力」的**机器可判落地回填**(命题范围收窄 + 载体覆盖账 + D-4 ③/④ 回填)
> **收口指针**(per 台账 **Entry #339**):① D-1 诚实声明的收窄 + ② 载体**来源类型**的落地映射 + ③ `CellKind::Free` 的落地与边界 → **正文 §5.1.5**(实体 = `cep/protocol-engine.md` §5.1.5)的**分档表 / 域边界 / carve-out**;
> 　⚠️ **§5.1.5 不含的明细在本则的台账登记里** → **`Entry #257` ②**:**命题切分表**(2 条可机器判 + 4 条人工判定)· **载体覆盖账**(4 已接入 / 1 N/A / 2 未接入)· `CellKind::Free` 的**覆盖率口径**(真实格 = 0);
> 　· **机器载体** = `include/tlm/cep/source_registry.hh` + `sources.json`;④ 契约面登记 + ⑤ D-4 回填与 B3b 两域记账 + ⑥ Verification → **台账 `§4.0-a` / `§6.2` / `§11`** + **`Entry #257`**。
> ⛔ 本则原文不回改;**段内锚点**(⓿~⑥)以本指针的对应条目为准。

## Status Update(2026-11-07 · 八,per 台账 Entry #282)—— **G1~G3 的 4 条待裁决点裁定**(architect)+ G1 形态修正为「组合式 wrapper」

> ⛔ **决策正文见 [`ADR-CEP-19`**](../adr/ADR-CEP-19-*.md)的对应 Status Update**(G1~G3 的 4 条待裁决点裁定 / 组合式 wrapper 形态修正)。
> 本节原为实施状态登记,per 台账 Entry #333 架构文档治理已移除;**状态见 `objective-cep-platform §4.0` / `§4.0-a` / `§11`**。

## Status Update(2026-11-08 · 十一,per 台账 Entry #288)—— **「Core·TrafficGen 主干」的架构决策登记**(10 条统一)+ 实施前**不得宣称已落地**
> **收口指针**(per 台账 **Entry #339**):① 决策登记 **`D-0` ~ `D-10`** 中**改变架构约束者**(**ASM-1 / 3 / 4 / 7 / 8 / 9**)→ **`ADR-CEP-19` 的 `Status Update 2026-11-11`**;
> 　· 其余 5 条由该 ADR 的**既有 SU** 承载:**D-0**(完成者角色)→ 其 `Status Update(2026-11-08 · 二)` ③;`D-2`(命中路径回响应)· `D-5`(两拓扑并存)· `D-6`(冻结 spec MODIFIES)→ 其既有 SU 对应项;`D-10`(计数纪律)→ 其 `Status Update(2026-11-08)` 的逐字禁令句;
> ①-b 「经网络」的**定义钉死** → **正文 §6.3.1**(实体 = [`cep/fabric-transport.md`](cep/fabric-transport.md) §6.3.1);
> ①-c 桥挂载结构裁定 + 拓扑 B 的如实命名 → **`ADR-CEP-19` 的 `Status Update(2026-11-08 · 二)`**;
> ② 实施前的文档纪律(⛔ 不得预先宣称)→ **`ADR-CEP-19` 的 `Status Update 2026-11-11` ②**;③ 计数纪律 → **`ADR-CEP-19` 的 `Status Update(2026-11-08)` 的逐字禁令句**;
> ④ 冻结面清单与两条授权链的分离 + ⑤ Verification → **台账 `objective-cep-platform §11`** + **`Entry #288` / `#291` / `#292`**。
> ⛔ 本则原文不回改;**段内锚点**(①~⑤)以本指针的对应条目为准。

## Status Update(2026-11-08 · 十二,per 台账 Entry #292)—— **4 项决策面勘误与补充**(D-0 定位改写 / `:71` 撤销理由 / 状态追认 / 拓扑分档判据 **J-9**)
> **收口指针**(per 台账 **Entry #339**):① 勘误与补充总表(4 条,**F-01 / F-04 / F-05 / F-11**)→ **台账 `Entry #292` ① 表** + **`ADR-CEP-19` 的 `Status Update(2026-11-08 · 二)` ①~④**;其中 **F-11 的 J-9 子项** → **正文 §6.3.1**(实体 = `cep/fabric-transport.md` §6.3.1);
> ② `D-0` 行**逐字更正**(⛔ 原 `D-0` 行原文保留)→ **`ADR-CEP-19` 的 `Status Update(2026-11-08 · 二)` ③**(逐字更正对照表 + 防误读句);⚠️ ⛔ **不是** `Status Update 2026-11-11` —— 该 SU 自陈 **ASM-0 由既有 SU 覆盖、不重复**;
> ③ 拓扑分档:**「A 必须零延迟」的理由 + `J-9` 拓扑分档判据** → **正文 §6.3.1**;
> ④ 声明面落点二选一 → **已由 `Status Update(2026-11-08 · 十三)` 裁定(= 甲)**;
> ⑤ 防误读句(逐字)+ ⑥ Verification → **台账 `objective-cep-platform §11`** + **`Entry #292` / `#293`**。
> ⛔ 本则原文不回改;**段内锚点**(①~⑥)以本指针的对应条目为准。

## Status Update(2026-11-08 · 十三,per 台账 Entry #293)—— **声明面落点裁定回填**(= **甲**)+ **D-9 元数据字段增补**(F-12)

> ⛔ **决策正文见 [`ADR-CEP-06`**](../adr/ADR-CEP-06-*.md)的对应 Status Update**(声明面落点裁定(= 甲)+ D-9 元数据字段增补)。
> 本节原为实施状态登记,per 台账 Entry #333 架构文档治理已移除;**状态见 `objective-cep-platform §4.0` / `§4.0-a` / `§11`**。

## Status Update(2026-11-08 · 十四,per 台账 Entry #295)—— **F-08 残余裁定**(索引**移到文末** + 新纪律 + 具名闸)+ **retention 裁定 = 丙**(指针)

> ⛔ **决策正文见 [`ADR-CEP-06`**](../adr/ADR-CEP-06-*.md)的对应 Status Update**(F-08 残余裁定(索引移到文末 + 新纪律 + 具名闸)/ retention = 丙)。
> 本节原为实施状态登记,per 台账 Entry #333 架构文档治理已移除;**状态见 `objective-cep-platform §4.0` / `§4.0-a` / `§11`**。

## 12. CEP 使用手册(Stage 4+ 维护态交接)

> **本节已迁出**(per 台账 Entry #333 架构文档治理 + 用户裁决 B):
> 全文见 [`docs/guide/cep-usage.md`](../guide/cep-usage.md) —— 10 分钟上手 / 载体地图 / 新增协议族标准流程 / 实验 4 件套跑法 / 闸清单 / 冻结面五步授权链 / DoD 读法 / 未结义务清单。
>
> **本节在架构文档中只保留入口指针** —— 操作步骤不属架构蓝图(per `AGENTS.md §DOC HYGIENE` 真源原则)。
> ⚠️ **本标题保留「CEP 使用手册」字样** —— `objective-cep-platform §8.3` ④ 的判定命令 `grep -rln "CEP 使用手册"` 依赖它(per 台账 Entry #334 ②)。

## Status Update 2026-11-12(per 台账 Entry #363)—— **R-c 机器检查已落地**(D0 堵法 ② 兑现)+ 收口指针改指新闸

> **性质**:本则**追加**于本文件 `## §末 文档索引` **之前**(per 该索引的新纪律:「此后**新增段落 SHALL 插在本索引之前** ⇒ 新增内容永不使既有 `path:line` 锚点位移」)⇒ **本索引区整体后移**,既有正文锚点**不位移**。
> ⛔ 本则**不改**本文件任何既有正文 / 既有 Status Update 段(append-only)。

**① 逐字更正上一则(:501)的当前态**

`:501` 原文(**保留为历史**,不回改)写:「⚠️ **D0 堵法 ② 的机器检查尚未落地**(`ADR-CEP-00` 同 SU 已登记为待办),且其原始规格是「断言 **`07 号`** 含 3 份 ADR 的残留登记 + 3 条触发条件」⇒ **收口后该规格必假**,实现时须改指 **`ADR-CEP-00` 的 `Status Update 2026-11-11` ①~④**」。

⇒ **自本则起**:

1. 「**尚未落地**」**已失效** —— D0 堵法 ② 的机器检查**已于 2026-11-12 落地**(载体见 ②);该句**仅作历史叙述保留**;
2. 规格「断言 `07 号` 含 3 份 ADR 的残留登记 + 3 条触发条件」**确已必假** —— 实现按当时裁决**改指 `ADR-CEP-00`**;**现行实现口径** = `ADR-CEP-00` 的 `## Status Update 2026-11-12`(取代声明 + 三值换算表 + `### ⑥` 重述块)+ 闸 `scripts/test/cep_decision_block_check.{sh,py}`。

**② 落地载体(指针改指)**

| 段 | 载体 | 判据 | 期望值来源 |
|---|---|---|---|
| **R-c** | `scripts/test/cep_decision_block_check.py` | 双向对账:正向(三份精确 basename + **无同号拆出件**)+ 反向(`ADR-CEP-00` 的 `### ⑥` 重述块)+ 触发条件表可解析性(4 类) | **闸内源码常量** |
| **R-d** | 同上 | 22 份 ADR 的 §2 决策块计数**逐份** == 快照(`test/cep_decision_block_snapshot.json` · 22 键 + `total`=125) | **闸外快照** |

⇒ 决策块计数口径的**规范载体** = `ADR-CEP-00` 的 `## Status Update 2026-11-12`(§⑤ 的「唯一规范载体」地位由该 SU 继承,§⑤ 文本**保留为历史**);**canonical 实现** = 上述闸的 Python 实现。

**③ 诚实边界**

- 本则**只登记落地状态**,⛔ 不改本文件的架构蓝图(G1「07 号 只放蓝图、状态迁台账 / ADR」被**轻度偏离** —— 权衡结果:「**入口不撒谎**」压过「蓝图不放状态」,per Entry #361 ③ D-7)。
- 本则**新增内容不位移内容锚**;索引区整体后移 ⇒ 本文件的**既有锚点债**(如 `:684` 早已越界、其内容已删,per 台账 `obj §18761`)**不在** C3 修。
- R-c 闸只验「**引用良形 + 被指物存在**」,⛔ **不验**「被指物真实现了该判定」;**新编号拆出件不在 R-c 覆盖内**(由 `cep_adr_structural_check.sh` 的分区完整性 + `ADR-CEP-00 §3` supersede 表兜底)。

**关联**:台账 **Entry #363** / **#364** · **`ADR-CEP-00` 的 `Status Update 2026-11-12`** · change `cpptlm-cep-invariant-gate-teeth` · `scripts/test/cep_decision_block_check.{sh,py}`

## §末 文档索引(2026-11-08 移置文末,per 台账 Entry #295)

> **用途**:本文件已 **29 个一级节 / 16 则 Status Update** —— **全部** `## ` 级标题与**全部** Status Update 逐条列于下表(索引 A / 索引 B 由 `scripts/cep/generate_doc_index.py` **全量机械生成**,含**实测行号列**)⇒ 故设本索引(以**零影响**既有 `path:line` 锚点 —— ArchForge 对齐清单按行号锚定)

> ⚠️ **新纪律(per 台账 Entry #295,逐字)**:本索引**永久置于文末**;此后**新增段落 SHALL 插在本索引之前** ⇒ **新增内容永不使既有 `path:line` 锚点位移**;⚠️ **最新 SU 在本索引之前**(⛔ 勿在本文末找最新段)。

> ⚠️ **本索引由脚本生成(零手抄)⇒ 每次追加后重生成**;生成器 = `scripts/cep/generate_doc_index.py`(2026-11-12 落仓,per 台账 Entry #342)⇒ `--write` 就地重写 / `--check` 判漂移。

> ⚠️ **2026-11-12 起**:索引 A / 索引 B **全量列出**(⛔ 不再有「另有 N 个未列入」的 `## ` 标题);自述数 == 索引行数 == 文件**实际** `## ` / SU 数(`cep_index_position_check.sh` 三合一判据 + 行号逐行判据)。⚠️ **待人工判断的导航标注不在索引 A / B**(索引 A/B 是机械视图)⇒ 见索引 C 与 `§11` / `§12` 的正文说明。

### 索引 A · 一级节(29)

| # | 章节 | 行 |
|---|---|---|
| 1 | 1. 文档定位与真源关系 | 43 |
| 2 | 2. 6 层 CEP 架构 | 124 |
| 3 | 3. canonical 10 条 Coherence Interface 流 | 181 |
| 4 | 4. 67 编号位 / 66 条守门 invariant 总表(单一真源) | 192 |
| 5 | 5. Bundle 与协议适配 | 254 |
| 6 | 6. 仿真语义与集成 seam | 277 |
| 7 | 7. 实验输出协议 | 292 |
| 8 | 8. 演进路线与阶段边界 | 307 |
| 9 | 9. Primary ADRs | 400 |
| 10 | 10. References | 428 |
| 11 | 11. Gap → 实现状态映射 | 449 |
| 12 | §附录 · Status Update 2026-10-28(第八则 · per 台账 Entry #88 —— JSON 装配连通性落地 + `INV-CEP-07-IC4` 闭合 + egress 通道语义登记) | 456 |
| 13 | 附录 · Status Update 2026-10-29(第十二则 · per 台账 Entry #96 —— 队列 #9 的 R-1 裁决落地) | 461 |
| 14 | 附录 · Status Update 2026-10-30(第十六则 · per 台账 Entry #106 —— **S1-3 触发后的真实义务**结论) | 466 |
| 15 | Status Update(2026-11-01,per 台账 Entry #131)—— §5.1.1 **CXL 行**的措辞对齐(裁决 ③⑦)+ `cxl_2_0` 实例登记 | 473 |
| 16 | Status Update(2026-11-02,per 台账 Entry #154)—— §5.1.1 / §5.4 / §11 的 **5 处口径订正** + **对称模式的 adapter 形态裁决** | 480 |
| 17 | Status Update 2026-11-04 —— Stage 3 收口的 **5 项架构级裁决**(B3 口径拆分 · 3 项声明假设处置 · `FullMapDirectory` 触发条件 · 跨仓锚点治理不变量 · MOESI-GPU 形态登记) | 487 |
| 18 | Status Update 2026-11-04(#2)—— 实验输出契约的**可执行化**(C1)+ ADR 一决策一文档化的**残留登记与触发条件**(C2) | 496 |
| 19 | Status Update(2026-11-06,per 台账 Entry #238)—— 「转移表真源」的**判定式(T1–T4)** + **TileLink-C 阻塞解除** + 逐族重判 | 506 |
| 20 | Status Update(2026-11-06 · 四,per 台账 Entry #249)—— 「转移表真源」的**来源效力**与 **B3b 计入口径** | 513 |
| 21 | Status Update(2026-11-06 · 五,per 台账 Entry #257)—— D-1「来源效力」的**机器可判落地回填**(命题范围收窄 + 载体覆盖账 + D-4 ③/④ 回填) | 523 |
| 22 | Status Update(2026-11-07 · 八,per 台账 Entry #282)—— **G1~G3 的 4 条待裁决点裁定**(architect)+ G1 形态修正为「组合式 wrapper」 | 529 |
| 23 | Status Update(2026-11-08 · 十一,per 台账 Entry #288)—— **「Core·TrafficGen 主干」的架构决策登记**(10 条统一)+ 实施前**不得宣称已落地** | 534 |
| 24 | Status Update(2026-11-08 · 十二,per 台账 Entry #292)—— **4 项决策面勘误与补充**(D-0 定位改写 / `:71` 撤销理由 / 状态追认 / 拓扑分档判据 **J-9**) | 543 |
| 25 | Status Update(2026-11-08 · 十三,per 台账 Entry #293)—— **声明面落点裁定回填**(= **甲**)+ **D-9 元数据字段增补**(F-12) | 551 |
| 26 | Status Update(2026-11-08 · 十四,per 台账 Entry #295)—— **F-08 残余裁定**(索引**移到文末** + 新纪律 + 具名闸)+ **retention 裁定 = 丙**(指针) | 556 |
| 27 | 12. CEP 使用手册(Stage 4+ 维护态交接) | 561 |
| 28 | Status Update 2026-11-12(per 台账 Entry #363)—— **R-c 机器检查已落地**(D0 堵法 ② 兑现)+ 收口指针改指新闸 | 569 |
| 29 | §末 文档索引(2026-11-08 移置文末,per 台账 Entry #295) | 600 |

### 索引 B · Status Update(16 · 2026-10-28 起)

| # | 日期 | 主题 | 台账 | 行 |
|---|---|---|---|---|
| 1 | 2026-10-28 | §附录 · Status Update 2026-10-28(第八则 · per 台账 Entry #88 —— JSON 装配连通性落地 + `INV-CEP-07-IC4` 闭合 + egress 通道语义登记) | Entry #88 | 456 |
| 2 | 2026-10-29 | 附录 · Status Update 2026-10-29(第十二则 · per 台账 Entry #96 —— 队列 #9 的 R-1 裁决落地) | Entry #96 | 461 |
| 3 | 2026-10-30 | 附录 · Status Update 2026-10-30(第十六则 · per 台账 Entry #106 —— **S1-3 触发后的真实义务**结论) | Entry #106 | 466 |
| 4 | 2026-11-01 | Status Update(2026-11-01,per 台账 Entry #131)—— §5.1.1 **CXL 行**的措辞对齐(裁决 ③⑦)+ `cxl_2_0` 实例登记 | Entry #131 | 473 |
| 5 | 2026-11-02 | Status Update(2026-11-02,per 台账 Entry #154)—— §5.1.1 / §5.4 / §11 的 **5 处口径订正** + **对称模式的 adapter 形态裁决** | Entry #154 | 480 |
| 6 | 2026-11-04 | Status Update 2026-11-04 —— Stage 3 收口的 **5 项架构级裁决**(B3 口径拆分 · 3 项声明假设处置 · `FullMapDirectory` 触发条件 · 跨仓锚点治理不变量 · MOESI-GPU 形态登记) | — | 487 |
| 7 | 2026-11-04 | Status Update 2026-11-04(#2)—— 实验输出契约的**可执行化**(C1)+ ADR 一决策一文档化的**残留登记与触发条件**(C2) | — | 496 |
| 8 | 2026-11-06 | Status Update(2026-11-06,per 台账 Entry #238)—— 「转移表真源」的**判定式(T1–T4)** + **TileLink-C 阻塞解除** + 逐族重判 | Entry #238 | 506 |
| 9 | 2026-11-06 | Status Update(2026-11-06 · 四,per 台账 Entry #249)—— 「转移表真源」的**来源效力**与 **B3b 计入口径** | Entry #249 | 513 |
| 10 | 2026-11-06 | Status Update(2026-11-06 · 五,per 台账 Entry #257)—— D-1「来源效力」的**机器可判落地回填**(命题范围收窄 + 载体覆盖账 + D-4 ③/④ 回填) | Entry #257 | 523 |
| 11 | 2026-11-07 | Status Update(2026-11-07 · 八,per 台账 Entry #282)—— **G1~G3 的 4 条待裁决点裁定**(architect)+ G1 形态修正为「组合式 wrapper」 | Entry #282 | 529 |
| 12 | 2026-11-08 | Status Update(2026-11-08 · 十一,per 台账 Entry #288)—— **「Core·TrafficGen 主干」的架构决策登记**(10 条统一)+ 实施前**不得宣称已落地** | Entry #288 | 534 |
| 13 | 2026-11-08 | Status Update(2026-11-08 · 十二,per 台账 Entry #292)—— **4 项决策面勘误与补充**(D-0 定位改写 / `:71` 撤销理由 / 状态追认 / 拓扑分档判据 **J-9**) | Entry #292 | 543 |
| 14 | 2026-11-08 | Status Update(2026-11-08 · 十三,per 台账 Entry #293)—— **声明面落点裁定回填**(= **甲**)+ **D-9 元数据字段增补**(F-12) | Entry #293 | 551 |
| 15 | 2026-11-08 | Status Update(2026-11-08 · 十四,per 台账 Entry #295)—— **F-08 残余裁定**(索引**移到文末** + 新纪律 + 具名闸)+ **retention 裁定 = 丙**(指针) | Entry #295 | 556 |
| 16 | 2026-11-12 | Status Update 2026-11-12(per 台账 Entry #363)—— **R-c 机器检查已落地**(D0 堵法 ② 兑现)+ 收口指针改指新闸 | Entry #363 | 569 |

### 索引 C · 关键表指针(高频查阅)

| 表 | 位置 | 用途 |
|---|---|---|
| §5.1.1 工业硬件协议族 | [`cep/protocol-engine.md`](cep/protocol-engine.md) §5.1.1 | 8 实例 / 6 族(**状态列已迁出** → `obj §4.0-a`) |
| §3.1 canonical 10 流 | [`cep/coherence-interface.md`](cep/coherence-interface.md) §3.1 | 一致性流的唯一清单 |
| §6.4 Null 实现 | [`cep/simulation-and-experiment.md`](cep/simulation-and-experiment.md) §6.4 | 解耦测试兜底 |
| ~~§11 落地状态表~~ | **已迁出** | → `objective-cep-platform §4.0` / `§4.0-a` |
| ~~§12 CEP 使用手册~~ | **已迁出** | → [`docs/guide/cep-usage.md`](../guide/cep-usage.md) |
| §5.1.5 「转移表真源」判定式(T1–T4) | [`cep/protocol-engine.md`](cep/protocol-engine.md) §5.1.5 | **补运行时引擎的触发条件**的唯一判据(四要件合取) |
| §6.3.1 「经网络」定义 + J-9 拓扑分档 | [`cep/fabric-transport.md`](cep/fabric-transport.md) §6.3.1 | 拓扑 A/B 的**可机判判据**;⛔ 防「事务穿过桥」假绿 |
| §7.2.2 「全绿」两级判定 | [`cep/simulation-and-experiment.md`](cep/simulation-and-experiment.md) §7.2.2 | run-validity / hypothesis-verdict 分离 |
| §8.5 文档层治理不变量 | 行 386 | 跨仓锚点两类处置 + **ADR 编辑禁令** + 决策块计数口径的**镜像指针**(规范载体 = `ADR-CEP-00` SU §⑤) |
| **子文档:第 1 层** | [`cep/stub-cache-core.md`](cep/stub-cache-core.md) | §3.2 / §3.3 实体 |
| **子文档:第 2 层** | [`cep/coherence-interface.md`](cep/coherence-interface.md) | §3.1 / §5.4 实体 |
| **子文档:第 3 层** | [`cep/protocol-engine.md`](cep/protocol-engine.md) | §5.1(含 5.1.1~5.1.5)实体 |
| **子文档:第 4 层** | [`cep/directory-ordering.md`](cep/directory-ordering.md) | §5.2 实体 |
| **子文档:第 5 层** | [`cep/collective-plane.md`](cep/collective-plane.md) | §5.3 实体 |
| **子文档:第 6 层(传输层)** | [`cep/fabric-transport.md`](cep/fabric-transport.md) | §6.3 / §6.3.1 实体 |
| **子文档:仿真与实验** | [`cep/simulation-and-experiment.md`](cep/simulation-and-experiment.md) | §6.1/6.2/6.4 + §7 实体 |
| **子文档:Core/TrafficGen** | [`cep/core-traffic-gen.md`](cep/core-traffic-gen.md) | 本层实体 + **决策载体三分**(ASM-1/3/4/7/8/9 + 既有 SU 的 5 条 + §6.3.1);per 批 4 = **已落 ADR**(⛔ 非骨架) |
| **子文档:对拍基准** | [`cep/parity-baseline.md`](cep/parity-baseline.md) | **H7 对拍基准规格**(独立**验证手段**面;上游 = `obj §8.1 R7` / `§9.1 H7` / `§9.2 M3`;⛔ **不在 `§N` 体系内**) |
| `obj-cep-platform.md` §6.2 | 台账 | 终局 DoD 12 条 |

---

**版本**: v0.2 治理收口(2026-11-11,per 台账 Entry #339)—— **总纲 + 9 篇 CEP 子文档**(`docs/architecture/cep/`);v0.1 起草(2026-10-20)+ 17 则 Status Update(2026-10-25 ~ 2026-11-08)**已全部收敛为归宿指针**(per 批 1 瘦身 / 批 2 E 类提炼 / 批 3 按层拆分 / 批 4 A 类落 ADR / 批 5 SU 收口)。
**批准状态**: 🟡 待 Oracle 复审
**维护者**: CppTLM build agent(architect mode)
