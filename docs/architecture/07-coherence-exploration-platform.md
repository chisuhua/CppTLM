# 07 — Coherence Exploration Platform(CEP)一致性互联仿真平台

> **状态**: 🟡 **v0.1 起草中**(2026-10-20,per `ADR-CEP-00 §5.2` 候选 α 拍板)
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
> | §3 / §5 / §6 / §7 | **指针(非定义处)** | `cep/` 8 篇子文档(逐节桩首行写实体路径) |
> | §8 | 正文(演进路线 + 治理不变量) | 本文件 |
> | §9 / §10 | 正文(列表) | 本文件 |
> | §11 / §12 | 指针桩 | 台账 / `docs/guide/cep-usage.md` |
> | Status Update 附录 | 决策与状态登记(过渡态) | 本文件(⛔ A 类部分待批 4 落 ADR 后收口) |
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

**③ 决策块计数口径(治理不变量,落本文件而非 `NUMBER-MAP`)**:
**决策块** = **ADR 的** §2 章下的**独立决策载体**(「主决策段」+ 各 `### 决策 N` 段);ADR §2.0 的选项对比表是
决策的**支撑物**,**不计块**。⛔ 落点理由:`ADR-CEP-NUMBER-MAP` 的职权域是**编号空间**,
为一个计数口径破其冻结基线 + 耦合 `test/cep_adr_parser.hh` 的 owner 解析器,收益不成比例。

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

> **正文吸收**:本则的架构面(per-network 嵌套形态的充分性论证 + SLICC 式备选的触发条件)已提炼进 **§5.1.5**(per 批 2;⛔ 本则原文不回改)。

> **为何单独一则**:第十五则登记的是队列 #9 的归档事实与**触发**(S1-3 由 ⛔ → ✅);
> 本则登记的是**触发之后必须做的两件事**的结论(`ADR-CEP-12 §8.4` 的备选重评 +
> `objective` 的「承接项 2」滚动复核)。两则同批、同来源,便于回溯。

### 一、S1-3 的真实义务:`ADR-CEP-12 §8.4` **SLICC 式备选重评**

**备选内容**(登记在 `ADR-CEP-12 §8.4`,当时**明确不实施**):协议只声明**协议本地虚拟网络**
(`fwd` / `resp` / `req`,各带 ordering 属性 —— 等价 `.pcc` 的 `Network {}` 块),另设**装配绑定产物**
承载 `(协议, vnet) → (network, vc_id)`。登记理由原文:「当前为 **0 个 `implemented` 实例** ——
这是将来最便宜的改点,**别让这个选项无声死掉**」。

**重评时点**:`implemented` 实例数 = **3**(`moesi_gpu` / `chi_rnf` / `mesif`),正是 §8.4 写的触发条件。

| 评估维度 | SLICC 式(协议本地 vnet + 装配绑定) | 现行嵌套形态(实例内声明 `vc_binding[network]`) |
|---|---|---|
| 排序声明次数 | **1 次**(协议本地) | 每实例每网络各一次(mesif 现有 2 网络 × 5–4 类) |
| 跨协议实例重复 | 零重复 | 机械重复(`mesif` 与 `moesi_gpu` 的 `compute_noc` 绑定**逐字相同**) |
| 装配期校验强度 | `ADR-CEP-09` 前提 3 由弱校验 → **强校验** | 现状:三条件逐网络校验(`INV-CEP-12-IC2`)+ 跨仓镜像闸,**已机器化** |
| C-4 双向映射 | 声明 vnet ↔ 绑定表 ↔ fabric 三段各管一段(更干净) | 两段(声明 + fabric),**判据已闭合** |
| 代价 | **推翻 ArchForge N-2 已交付的嵌套形态** ⇒ 跨仓谈判重开;且需新增一份**装配绑定产物**及其消费者 | 重复是**机械**的,可被判据锁住(现已有 `cep_decl_alignment_check.sh`) |

**结论:维持现行嵌套形态**(**不**迁移 SLICC 式),理由按权重排序:
1. **收益尚不成立**:3 个实例的重复是**机械**的,已被 `INV-CEP-12-IC2`(逐网络三条件)与
   `cep_decl_alignment_check.sh`(双向映射 + VC 上界 + 6 类词表 + 网络集)锁住 ⇒ 重复**未产生**
   过实际缺陷;而 SLICC 式的**新增装配绑定产物**在 Stage 1 **没有消费者**(fabric 侧尚无绑定面)。
2. **代价是跨仓的**:迁移推翻 ArchForge `N-2` 已交付形态 ⇒ 需要重开跨仓谈判,而当前跨仓口径
   刚在 Entry #102 达成逐字段一致(12 键全等)——**在刚收敛时重开谈判**的成本远高于收益。
3. **选项不被杀死**(§8.4 的原始诉求):本条**不注销**该备选,而是**改绑到更强的触发条件**:
   「出现**第 3 张网**」或「某实例需要**跨网络的联合排序**(即单实例无法用 per-network 绑定表达)」
   时**再评**。当前 3 个实例**都**能用 per-network 绑定完整表达(mesif 的 `atomic` 只在
   `ualink_fabric`、`snoop` 只在 `compute_noc`,是**网络特化**而非联合排序)。

### 二、「承接项 2」滚动复核(**第 2 次**)

**承接项 2** = per-network 嵌套形态的充分性复核(节奏源:umbrella design `D4.5`);
第 1 次已随队列 #6(`moesi_gpu`)交付并复核。

**本次复核(第 2 次)结论**:在 `chi_rnf` + `mesif` 两个实例落地后,嵌套形态**仍然充分**:
- 三实例的 `ordering_assumption` / `vc_binding` **均**以 `network` 为外层键,**无**跨网络联合约束;
- `mesif` 是**首个目录型/跨卡**实例(两个网络:`compute_noc` + `ualink_fabric`),它把
  `atomic` / `query` 只声明在 `ualink_fabric`、`snoop` 只声明在 `compute_noc` ——
  这仍是**网络特化**,可用嵌套表直接表达;
- 装配闸在 3 实例共存下 verdict **无环**(`evaluate_channel_dependency_gate`,逐
  `(instance, network, vc)` 8 条 `mesif` 记录全 PASS)。

⇒ **无需修改嵌套形态**;下次滚动复核的触发 = 出现**第 4 个** `implemented` 实例,或任一实例
需要跨网络联合排序。

### 三、残余风险(不得写成「已消除」)

`R-1`(按档批量 14 格证据强度弱于逐格)· `R-3`(写回事实在 4 值 `perm` 上不可分辨 ⇒ 由
`LocalView.dirty` 掩码 + 写回接线补偿)· 残留 A(`HierarchicalDirectory` 行为语义无真源 ⇒
逐条 `basis` + `pending` + 跨仓追认债)· 残留 C(`atomic` 的 `consistency_intent` 无独立字段)·
引擎建模决策 `E-1 ~ E-6` —— **均未消除**,见 `Entry #105` §③ 与 design `D-β` / `D-λ`。


---

---

**版本**: v0.1 起草(2026-10-20) + Status Update 2026-10-25(队列 #4 启动)+ Status Update 2026-10-27(队列 #7 立项 + 排队 13 → 14)+ Status Update 2026-10-27 第二则(Entry #82 —— 实现状态映射漂移回填)+ Status Update 2026-10-27 第三则(Entry #83 —— 载体缺口登记与 owner 指派)+ **Status Update 2026-10-27 第四则(Entry #84 —— CohIf 载体落地 + §5.4 第三类同步)** + **Status Update 2026-10-27 第五则(Entry #85 —— ProtocolEngine 端口面落地 + Null 默认实现)** + **Status Update 2026-10-28 第六则(Entry #86 —— ProtocolBridge 特化桥落地 + 钩子死→活 + 判据分层)** + **Status Update 2026-10-28 第七则(Entry #87 —— `REGISTER_CHSTREAM_BIDIRECTIONAL` 落地 + `BidirectionalPortAdapter` 泛化)** + **Status Update 2026-10-28 第八则(Entry #88 —— JSON 装配连通性落地 + `INV-CEP-07-IC4` 闭合 + egress 通道语义登记)** + **Status Update 2026-10-28 第九则(Entry #89 —— 判据收口 + 反向 fixture 登记 + 零 diff 复核)** + **Status Update 2026-10-28 第十则(Entry #93 —— 队列 #8 立项 + §5.1.1 CHI 行同步 + UC→UD 静默升级事实登记)** + **Status Update 2026-10-28 第十一则(Entry #95 —— 队列 #8 归档 + §5.1.1 CHI 行状态列同步)** + **Status Update 2026-10-29 第十二则(Entry #96 —— 队列 #9 的 R-1 裁决落地 + §5.1.1 MESIF 行同步)** + **Status Update 2026-10-29 第十三则(Entry #101 —— 队列 #9 的规约层已闭合 + §5.1.1 MESIF 行同步)** + **Status Update 2026-10-30 第十四则(Entry #102 —— MESIF 规约的跨仓逐格追认闭合 + §5.1.1 MESIF 行同步)** + **Status Update 2026-10-30 第十五则(Entry #105 / #106 —— 队列 #9 实现层归档 + SharerTracker 6 实现 / adapter 3 实例口径同步)** + **Status Update 2026-10-30 第十六则(Entry #106 —— S1-3 触发后的 SLICC 备选重评结论「维持嵌套形态」+ 承接项 2 第 2 次复核)** + **Status Update 2026-11-01 第十七则(Entry #131 —— 队列 #10 实现层 §5.1.1 CXL 行措辞对齐裁决 ③⑦ + `cxl_2_0` 实例登记)**
**批准状态**: 🟡 待 Oracle 复审
**维护者**: CppTLM build agent(architect mode)
---

## Status Update(2026-11-01,per 台账 Entry #131)—— §5.1.1 **CXL 行**的措辞对齐(裁决 ③⑦)+ `cxl_2_0` 实例登记

> **正文吸收**:本则的正文订正已就地落在 **§5.1.1**(per 批 2;⛔ 本则原文不回改)。

> **正文不改**(07 号 §5.1.1 表格**零改动**);本段只登记**措辞对齐**与**实例状态**。
> ⚠️ **冻结面两步授权**:授权记录 = 台账 **Entry #131**(`--update --authorized-by "Entry #131"`)。

### ① §5.1.1 CXL 2.0 行:两处措辞与队列 #10 的裁决 ③⑦ **冲突**(现予消解)

| # | §5.1.1 现行措辞 | 裁决后的**准确**表述 | 处置 |
|---|---|---|---|
| 1 | 「Host 侧 Snoop Filter;**设备不感知其他设备**」 | 「Host 侧 Snoop Filter(**唯一**一致性点);**设备互不感知其他设备**在 CXL 2.0 **平凡成立**(每 root port 只 1 个 cache device)⇒ **不得**当作 2.0 的**区分性语义**」 | **仅登记**(表格正文不改);新增实例行的「关键差异」列以**追加**方式对齐 ⇒ 见 ③ |
| 2 | 「**CXL 3.0 对称**」行(「DCOH;设备可主动反向 snoop」) | 按裁决 ⑦:「CXL 3.0 对称」指 **HDM-DB / CXL.mem 的多 host 共享内存**;**CXL.cache 全代次都非对称** ⇒ 该行**不得**被读成「CXL.cache 在 3.0 变对称」 | **仅登记**;⇒ **CXL 3.0 的 BI 通道 / DCOH 仍属队列 #12**(per 队列 #10 的 Non-Goal #5) |

⚠️ **冲突性质**:真源 1(本文件)与真源 3(`ADR-CEP-11` 的 Status Update ③⑦)措辞不一致
⇒ per 真源原则冲突规则 3(真源 3 不可变)⇒ 以**真源 3**为准 ⇒ 本段登记对齐结论。

### ② §5.1.1 的**协议族计数**未变

⚠️ §5.1.1 的「6 族 / 8 实例」计数**不变**:CXL 2.0 与 3.0 仍是**同一 adapter 的两种模式**
⇒ 本次只新增**一个协议实例登记**(CXL 2.0 的 `cxl_2_0`),**不是**新增协议族
⇒ 族计数仍 6;⚠️ **实例计数口径**:§5.1.1 的「8 实例」指的是**协议族内的模式行**,与
`protocol_instances.json` 的 `instances[]`(**协议实例声明**,含 `chi_rnf` / `cxl_2_0` 等)**不是同一口径**
⇒ **两者不得互相换算**(per `ADR-CEP-12 §11` 的实例级权威分区)。

### ③ `cxl_2_0` 协议实例登记(2026-11-01,per 队列 #10 实现层)

| 面 | 值 |
|---|---|
| 实例 id / adapter | `cxl_2_0` / `CxlCacheAdapter` |
| 协议版本 | `cxl-2.0`(**`ADR-CEP-11 §3` 锁表已有该行** ⇒ 无需改锁表) |
| 状态 | `implemented`(adapter 已落地;**无 protocol engine** —— 照 `moesi_gpu` 先例 = adapter-only) |
| `sharer_tracker` | **`不参与`** —— CXL.cache 的 peer 集**平凡为空**(host-centric / 单一一致性点)⇒ 挂 SF 是**过度声明** |
| 消息类 → VC | `request`→0 / `response`→1 / `snoop`→2 / `snoop_resp`→3(逐条依据 = 锚点摘要 §2 的 D2H/H2D 通道表) |
| 权威分区 | **本仓自有**(`repo_owned_instance_ids[]`)⇒ **不镜像**进 ArchForge `32 号 §6.1` |

### ④ 后果 / 代价

| 面 | 后果 |
|---|---|
| **本文件** | **零正文改动**;§5.1.1 表格与「6 族 / 8 实例」计数**不变** |
| **CXL 3.0 范围** | 本次**不**动 CXL 3.0 ⇒ BI 通道 / DCOH 仍归**队列 #12** |
| **声明面** | `protocol_instances.json` 新增 1 条 ⇒ `implemented` 实例数 **3 → 4**(⚠️ 与 ② 的「8 实例」**不是同一口径**) |

**关联**: 台账 `obj-cep-platform.md` **Entry #131**(授权记录)· #122(口径 7 条)· #127(规约层归档)· #128(实现层转正)
· `ADR-CEP-11` 的 `## Status Update(2026-11-01)` ③⑦ · `ADR-CEP-12 §11`(实例级权威分区)

---

## Status Update(2026-11-02,per 台账 Entry #154)—— §5.1.1 / §5.4 / §11 的 **5 处口径订正** + **对称模式的 adapter 形态裁决**

> **正文吸收**:本则的正文订正已就地落在 **§5.1.1 / §5.4**(per 批 2;⛔ 本则原文不回改)。

> **本则含**:**正文订正 5 处**(见 ②)+ **adapter 形态裁决**(见 ③)—— ⚠️ **本文件属 M1 冻结面**
> ⇒ 走**两步授权**(授权记录 = 台账 **Entry #154**;`--update --authorized-by "Entry #154"` → commit 产物 → `--update` → commit)
> · **表体行数不变**(§5.1.1 仍 **8** 行)⇒ `test/test_cep_schema.cc` 的 `rows511 == 8` **零改动**
> · **触发**:队列 #12 `cpptlm-cep-cxl-dcoh-symmetric` 的 **Task 0 公开来源普查**
> (产物 `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md`,**零规范正文暴露**)

### ① 事实复核(公开来源核对;architect 一手复核,非转述)

**比对源** = **S4** = *An Introduction to the CXL Interconnect*,ACM CSUR 2024
(arXiv `2306.11227v3` · sha256 `30f5baac34bb23e478834536db334c45338ec77324f25db799ef6e0deb3075bf` —— **本次重新下载并校验 sha256 一致**)。

| # | 本文件原断言 | 公开来源核对 | 判定 |
|---|---|---|---|
| 1 | §5.1.1 「DCOH(**Device Cache Ownership History**)」 | S4 p.12:「the term **Device Coherence (DCOH) agent** to describe the agent in the device managing/tracking coherence between the device and the host」;S6 p.30:「**Device Coherence Engine (Dcoh)**」 | ❌ **展开错误** ⇒ 订正为 **Device Coherence(agent / engine)** |
| 2 | §5.1.1 「设备可主动反向 snoop」(无限定) | S4 p.21:snoop 目标是 **host processor**(memory controller back-snoops the host)⇒ **不对 peer device** | ❌ **缺限定** ⇒ 订正为「对 **host** 发起」 |
| 3 | §5.1.1 「基于 **DPA** 而非 PA 归属」 | S4 p.11:CXL.mem 使用 **Host Physical Addresses**,设备内部翻译到 media address space | ❌ **反证** ⇒ 订正为 **HPA**(设备内部翻译) |
| 4 | §5.4 `back_invalidate` 行「CXL 3.0 BISnp 走这里,**host→device**」 | S4 p.20:「Back-Invalidate (BI) in the **S2M** direction and the response BI-Rsp in the **M2S** direction」 | ❌ **方向反了** ⇒ 订正为 **device→host(S2M)**;其响应 BIRsp 为 M2S |
| 5 | §11 caps 表 `DcohSymmetric` 的 `aggregation_at` = 「**REQUESTER 或 HOME**」 | `ADR-CEP-04 §3.5` 的二元被 S4 **反证**(聚合点两侧场景均在 home 一侧)⇒ 收敛为 `HOME` | ❌ **二元作废** ⇒ 订正为 **HOME**(per `ADR-CEP-04` **第 3 则 SU**) |

### ② 正文订正(已落;逐条可核)

| # | 位置 | 订正后 |
|---|---|---|
| 1 | §5.1.1「CXL 3.0 对称」行的 **DCOH 展开** | **Device Coherence(agent / engine)**(设备内的一致性代理) |
| 2 | 同行的 **反向 snoop 表述** | 「设备可对 **host** 发起反向 snoop(BI;**不**对 peer device)」 |
| 3 | 同行的 **归属口径** | 「归属基于 **HPA**(设备内部翻译到 media address)」 |
| 4 | 同行的 **通道描述** | 「+ CXL.mem 增 **BISnp(S2M)/ BIRsp(M2S)** 两通道」(原写「反向通道」不精确) |
| 5 | §5.4 `back_invalidate` 行的 **BISnp 方向** | 「**device→host(S2M)**;其响应 BIRsp 为 M2S」 |
| 6 | §11 caps 表 `DcohSymmetric` 的 `aggregation_at` | **HOME**(原「REQUESTER 或 HOME」二元作废) |

### ③ ⚠️ 裁决:对称模式的 **adapter 形态** = **独立载体**,**不**扩 `CxlCacheAdapter`

**问题**:本表 §5.1.1 原写「CXL 3.0 对称 → `CxlCacheAdapter`(对称模式)」;但实测
`include/tlm/cep/cxl_cache_adapter.hh` 是 **`cxl_2_0` 实例专用**的载体 ——
**零自有表副本** + **单一委托** `cxl_state_spec.hh`(4 态 × 17 事件 · MESI 4 态)。

| # | 方案 | 优点 | 代价 | 判定 |
|---|---|---|---|---|
| **A** | **独立 adapter** `CxlDcohAdapter` + 独立规约载体 | 零副本原则**结构性不变**;两族语义**各自独立**(CXL 2.0 非对称 = 1 套 4 态;CXL 3.0 对称 = **3 套** 4/3/3 态)⇒ 一个 adapter 一套真相源 | 本表 §5.1.1 的 adapter 列需改(已改);**多一个** adapter 类 | ✅ **裁决采用** |
| **B** | 扩 `CxlCacheAdapter` 支持**双载体** | 类少一个 | ⚠️ 迫使该类引入**载体选择逻辑** ⇒ **破坏「去注释后文本不含 `State::`/`Event::`/`Perm::`/表名」的结构性保证**;两套状态空间共处一载体 ⇒ 两套真相源 | ❌ |
| **C** | `CxlCacheAdapter` 的规约载体**参数化**(模板参数) | 复用类骨架 | ⚠️ 同样引入载体选择;且零副本判据(静态扫描)需重写 ⇒ **判据面扩大** | ❌ |

⇒ **裁决 A**。⇒ 连带:队列 #12 的**第 1 个 change**(实现层)交付 `CxlDcohAdapter`(**新**)+ 独立规约载体
⇒ **不**改 `CxlCacheAdapter`(`cxl_2_0` 载体**零改动**)。

### ④ 后果 / 代价

| 面 | 后果 |
|---|---|
| **本文件** | **正文订正 6 处** + 追加本则;§5.1.1 **表体行数不变**(8)· §8.1 Stage 表**不变** · **6 族 / 8 实例**计数**不变** |
| **测试面** | `test/test_cep_schema.cc` 的 `rows511 == 8` **零改动**(表体未变) |
| **代码面(连带,builder 域)** | ① `sharer_tracker.hh` 的 `DcohSymmetric` traits:`AggregationAt::REQUESTER` ⇒ **`HOME`**(该实现**行为未落地** ⇒ **零行为影响**(⚠️ **2026-11-02 后更新**(per 台账 Entry #167):行为实现已于队列 #12 落地 `cxl_dcoh_tracker.hh` ⇒ 该括注**仅对当次 traits 改动成立**,**不是**现状描述))② `test_cep_sharer_tracker_base.cc` 的**二元断言**(`∈ {REQUESTER, HOME}`)⇒ **收紧**为 `== HOME` ③ `protocol_instance.schema.json` **零改动**(枚举**保留** `REQUESTER` ⇒ 该值转为「保留但无消费者」) |
| **ADR 面** | `ADR-CEP-04` **第 3 则 SU** 已追加(聚合点二元裁决 + 选项对比 + 待确认路径);**正文零改动** |
| **合规** | 本则**不含**任何规范正文引用 ⇒ 全部依据为**公开非规范来源**(白皮书 / 会议 tutorial / 同行评审论文)+ **文献级定位 + 断言摘要** |
| **残余** | ⚠️ **规范性依据待**「人类成员条款摘要」确认(per `ADR-CEP-04` 第 3 则 SU ④)⇒ 登记态 |

### ⑤ Verification(前瞻判据 · 可勾选)

> ⚠️ **2026-11-02 复核(per 台账 Entry #167 §③)**:下列 **8 项全部满足** ⇒ 已勾选;
> 逐项复核证据见台账 Entry #167(其中 ⑦ schema 零改动 · ⑧ `rows511 == 8` 仍绿)。

- [x] §5.1.1「CXL 3.0 对称」行的 adapter 列 == `CxlDcohAdapter`
- [x] 同行的 DCOH 展开 **不含** "Ownership History";反向 snoop 表述含「**对 host**」;归属含「**HPA**」且**不含** "DPA"
- [x] §5.4 `back_invalidate` 行的 BISnp 方向 == **device→host(S2M)**
- [x] §11 caps 表 `DcohSymmetric` 的 `aggregation_at` == **HOME**(不含 "REQUESTER 或")
- [x] `sharer_tracker.hh` 的 `DcohSymmetric` traits `aggregation_at` == `AggregationAt::HOME`(builder 域)
- [x] `test_cep_sharer_tracker_base.cc` 的该行断言**收紧**为 `== HOME`(builder 域)
- [x] `protocol_instance.schema.json` **零改动**(`AggregationAt` 枚举仍含 `REQUESTER`)
- [x] `test/test_cep_schema.cc` 的 `rows511 == 8` **零改动**且仍绿
- [x] 冻结面 provenance 通过(40/40 hash 一致 + 与产物 commit 一致) —— 本则的 provenance 在**两步授权的第 2 步**核验(产物 commit 后 `--update` 对齐)

**关联**: 台账 `obj-cep-platform.md` **Entry #154**(授权记录 + 4 项裁决摘要)· **#153**(Task 0 普查登记 + architect 裁决面)
· #152(组 1 归档)· #148(组 0 收尾债清理 · §5.1.1 状态列同步先例)· #139(`FullMapDirectory` 落位裁决)
· `ADR-CEP-04` 的 **第 3 则 Status Update(2026-11-02)**(聚合点二元裁决)
· `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md`(§3.4 / §6 —— **本则依据**)
· `include/tlm/cep/cxl_cache_adapter.hh`(`cxl_2_0` 专用载体的**零副本 + 单一委托**结构 —— ③ 的判据依据)

## Status Update 2026-11-04 —— Stage 3 收口的 **5 项架构级裁决**(B3 口径拆分 · 3 项声明假设处置 · `FullMapDirectory` 触发条件 · 跨仓锚点治理不变量 · MOESI-GPU 形态登记)

> **正文吸收**:本则的判据面(B3 拆分 / 声明假设处置)已提炼进 **§8.5** 与 §5.1.5(per 批 2;⛔ 本则原文不回改)。

**主题**:14 个 change 全部归档后,一次全平台梳理暴露 8 项口径/登记缺口;本则裁决其中 5 项
(另 3 项已由 `ADR-CEP-04` SU#1 与台账 Entry #214 处置)。
**性质**:architect 域的**口径裁决 + 真源 1 同步**。objective 的原文措辞属 planner 域 ⇒ 由 planner 落地。

### ① DoD §6.2 B3「6 个 ProtocolEngine 实施完成」—— **一词两义,拆两行裁决**

**问题**:B3 的「ProtocolEngine」既可读作「**运行时状态机引擎**」,也可读作「**协议族适配器**」。
两读法的达成度天差地别:

| 读法 | 实测达成 |
|---|---|
| 运行时引擎 | **B3 的 6 族清单中只有 `CHI RN-F` 落地**(`chi_rnf_protocol_engine.hh`,648 行,且有 L4 事务级 E2E) |
| 协议族适配器 | **6 族全部达成**(7 个 adapter 类实现,per 本文件 §5.1.1 与 §11 caps 表) |

**裁决**:B3 **必须拆成两行**,且两行**各自独立可判**:

- **B3a(已达成)**:6 个协议族的 **adapter 契约**全部实施完成(7 个 adapter 类;逐实例可按 JSON 选择)。
- **B3b(部分达成)**:**运行时引擎** = **2 达成 / 分母 6 / {1 显式冻结, 3 受限-⏭}**(CHI RN-F + TileLink-C 已达成;CXL 2.0 冻结;CXL 3.0 / NVLink / UALink 受限 —— per 本文件末尾 SU 2026-11-06 · 四 D-3)。
- **未达成部分的触发条件**:某协议族的**规约载体出现转移表真源**时,才补运行时引擎
  (per oracle 2026-11-04 裁决:规约载体无转移表时补引擎 = 诱发推理填空 ⇒ **不补**;「转移表真源」的**判定式 = 本文件末尾 Status Update(2026-11-06) 的 T1–T4**)。

**⛔ 防自欺条款(强制,不可省略)**:objective 的 B3 **不得**整行改写为「已达成」。
必须保留「运行时引擎未达成」的**显式未勾项行**,并在同一行标注触发条件。
⇒ 「DoD 降格后仍宣称达成」是本裁决明确禁止的失败模式。

**⛔ 两条不得计入的口径**(避免冒充):
① **MESIF / MOESI-GPU / CohIfNull 不在 B3 的 6 族清单内** ⇒ 既不计入 B3、也不冒充;
② **UALink 无实例声明**(`ualink_4` 取值不可判定,per 本文件 §5.1.1 的登记)⇒ 不算「实例已声明」。

### ② 声明层 3 项建模假设 —— 逐项处置(**含实测口径更正**)

**实测更正(本次裁决的事实基础)**:`consistency_labels = SC/SC` 是 **5/7** 实例
(`chi_rnf` / `cxl_2_0` / `cxl_3_0_symmetric` / `tilelink_c` / `nvlink_4`);
`moesi_gpu` = `RC-data`/`RC-release`;`mesif` = `RC-acquire`/`RC-release`。
且 `writeback` 是 **`message_classes` 的成员**(7/7 实例含 `"writeback"`),**不是**实例级布尔字段。

| 假设 | 处置 | 依据 |
|---|---|---|
| `combination_capabilities` 三维全 true(7/7) | **维持** + 登记为「**声明入口 + 实验证伪**」 | 本文件既有的「组合能力不进 Stage 1~3 DoD,走人工声明 + 实验证伪」裁决同向;H2/H3/H6 即证伪通道 |
| `consistency_labels = SC/SC`(5 工业实例) | **补 litmus 证伪判据**:排序 / litmus 实验结果与 SC 期望不符 ⇒ 该实例假设标 `violated` | SC 是**强假设**,必须可证伪;当前 H 系列实验已提供载体,属低成本 |
| `writeback` 消息类存在(7/7) | **登记为建模简化** + 加限定语 | ⚠️ 最可疑项:`sharer_tracker.caps = NONE/NONE/NONE/NONE` 的实例仍声明 writeback 消息类 ⇒ 存在语义张力。限定语:**「表示协议语义支持写回,不表示该实例的 tracker 会利用」**;暂不删(删需改 7 个实例声明,违反最小改动) |

### ③ `FullMapDirectory` —— 维持「不排期」+ **补显式触发条件**

- **维持**台账 Entry #139 的裁决(当前队列序列无消费者 ⇒ 不排期、保留登记)。
- **补触发条件**:当某个 `protocol_instance` 声明 `sharer_tracker.impl = FullMapDirectory` 时 ⇒
  **才**排期该实例的装配与验证。现状「不排期」原本**无边界**,本则补上边界。
- **保留实现的论据**:它是 tracker 侧**唯一的单级 EXACT 目录**实现(header-only,无实例则无运行时面,维护成本近零)⇒ 删除即丢失未来复用锚点。

### ④ 跨仓对齐锚点 —— **散文引用节号化,机器锚点保留行号** + 立治理不变量

- **散文引用**(如对齐清单里的 `ADR-CEP-02:55` / `:66`):改为**节号引用**(如 `ADR-CEP-02 §X(CEN-Ib11)`)——
  它们**无机器校验**,纯给人看,行号漂移无感知。
- **机器锚点块**(`cep_archforge_alignment_check.sh` 的 (file, line, symbol) **三元校验**):**保留行号不动** ——
  其设计意图就是 fail-loud tripwire(故意让漂移立刻炸);且「ADR 正文不改 + SU 只追加末尾」不变量下
  **追加永不移动既有行号**。
- **⚠️ 治理不变量(新立,强制)**:**凡在 ADR 既有行之前插入内容的编辑 = 立即破坏跨仓对齐锚点 = 禁止**。
  ⇒ ADR 的编辑**只允许**「正文就地澄清(零行数变化)」或「末尾追加 SU」两种形态。

### ⑤ `MoesiGpuProtocolEngine` —— **形态限制登记**(设计内限制,非债务)

- **实测**:11 个 override **全为空体计数器**(canonical 10 流 + `on_address_resolve`,per 决策 M-6「只记账」);
  **无 emit / egress**;`include/chstream_register.hh` **零注册**;`configs/` 无该 engine 类的 JSON 声明。
- **⇒ 形态**:它是 **10 流记账壳**,不是协议状态机。L4 事务级 E2E **只能单向**
  (外部驱动事件 → 验证计数 + 补偿记录);反向(引擎发起 → 对端响应)**无载体**。
- **处置**:登记为**设计内限制**(per R11 V-2 交付时的显式设计裁决),**不**列为债务、**不**补 emit
  (补 emit = 重开决策 M-6 且无端到端消费者 ⇒ 过度工程)。

### ⑥ Verification(前瞻判据 · 可勾选)

- [x] B3 的两读法达成度已用**实测**分列(运行时 1/6 族 vs adapter 6 族)
- [x] 防自欺条款已写入本则(「不得整行改写为已达成」+ 触发条件行)
- [x] 两条「不得计入」口径已写明(MESIF/MOESI-GPU/CohIfNull 不在清单;UALink 无实例声明)
- [x] 3 项声明假设的处置已逐项给出,且**修正**了 SC/SC 的覆盖口径(5/7 非全实例)与 `writeback` 的字段形态
- [x] `FullMapDirectory` 的触发条件已补(某实例声明该 impl ⇒ 才排期)
- [x] 跨仓锚点的两类处置已区分(散文节号化 / 机器锚点保留行号)+ 治理不变量已立
- [x] MOESI-GPU 的形态已按**实测**(11 override 全空体)登记,非「仅 1 处 override」
- [ ] objective 的 B3 拆分措辞与未勾项行已落地(planner 域)
- [ ] `protocol_instance.schema.json` 的 `perm` 描述补「域锁 ≠ 每实例全列」(builder 域 + 冻结面授权)

**关联**: 台账 **Entry #214**(代码域注释订正)· **#213**(债务第一批 + E2E 覆盖矩阵)· **#212**(H2/H3/H6 首轮实验)·
**#190** ⑦(3 项声明假设的原始登记)· **#170** ⑤(行号锚点 + BISnp 登记)· **#139**(`FullMapDirectory` 落位裁决)·
`docs/adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md` **SU#1**(BISnp 方向订正)·
`docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md`(H2/H3/H6 结论与口径限定)·
`docs/validation/2026-11-04-cep-protocol-e2e-coverage.md`(逐协议 E2E 覆盖矩阵)

## Status Update 2026-11-04(#2)—— 实验输出契约的**可执行化**(C1)+ ADR 一决策一文档化的**残留登记与触发条件**(C2)

> **正文吸收**:本则的判据面(「全绿」两级 / 决策块计数口径)已提炼进 **§7.2.2 / §8.5**(per 批 2;⛔ 本则原文不回改)。

**主题**:Stage 0~3 收尾后一次全平台梳理暴露的 2 项架构级缺口,经 architect 裁决(2026-11-04,
oracle 复核通过 —— 含 3 处事实修正)后落地。本则是**真源 1 的同步面**;
**规范本体**在 `ADR-CEP-06` **SU#1**(C1)与本则 §③(C2)。

**性质**:architect 域的架构级裁决同步(⛔ 本则不改本文件 §1 ~ §11 正文,只追加)。
**关联**: `ADR-CEP-06` **SU#1**(C1 规范本体)· 台账 **Entry #215** / **#216**(前序 architect 裁决)·
`docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md`(受影响的已落盘产出)

---

### ① C1 摘要:`ADR-CEP-06` 决策 4 / 5 的**可执行化**

**根因(实测)**:决策 4 的兜底契约(`"not-collected"` + reason)**在本 ADR 自己规定的 schema 里不可表达**
(13 个 `required` 字段类型全是 `number`/`integer`,无 `oneOf`;实测 `jsonschema` 校验 ⇒ FAIL),
在 3 件冻结 tools 里**同样不可执行**(非数值即判红);决策 5 的「timing ⇒ 延迟 > 0」在测试层**零断言**
⇒ 实测 h2/h3/h6 出现「明知不采却写 0」的 run(h6 的 `verification_scope` 自陈「本实验不采延迟(avg/p50/p95 恒 0)」)。

**裁决 3 条(规范本体见 `ADR-CEP-06` SU#1 §②~§④)**:

1. **载体 = 值即类型**:13 必纳字段取值放宽为「数值 **或** `"not-collected"`」;
   **新增** per-metric `_not_collected` map(取代单字符串 `_not_collected_reason`)。
   ⛔ 不选「伴随标注 + 数值恒为 0」—— 实测 0 与未采集的 0 在**数据层同形**,机器闸查不出漏采集。
   ⛔ 不选「拆分必纳集」—— `directory_misses` 的空转是**产出者缺位**,不是契约错。
2. **决策 5 三分支**:原文两条保留,追加「`timing` **且**预声明不采延迟 ⇒ 延迟族必须 `"not-collected"`,⛔ 禁写 0」。
   ⛔ **不**把 h6 改标 `functional` —— `simulation_mode` 是**仿真的属性**,改标会写入第二个谎言。
3. **「全绿」拆两级**:**run-validity**(13 键齐备 + 每值要么真实数值要么带 reason 的 not-collected
   + `not-collected ⊆` 预登记 out-of-scope 集 + 违例类 not-collected ⇒ IC3 判「不适用」)
   与 **hypothesis-verdict**(§9 判据逐条标「测量了/未测量/fail」;未测量**不继承 pass**)。
   ⇒ **B4 的可判定性由本裁决确立**,但 B4 的**落地措辞**属 objective 域(planner)。

**⚠️ 预登记闸是本裁决的承重结构**:若 emit 端可**事后自由**标 `not-collected`,
则「结构零冒充实测」只是被替换成「`not-collected` 冒充预声明」—— 自欺**升级**而非消除。
⇒ 预登记集合的载体在 **objective / plan 域(评审过的设计面)**,**不在** `metrics.json`(被试者自证的报表面)。

**⚠️ 违例类允许 `not-collected`(订正裁决初稿的禁令)**:若禁止,emit 的唯一合规出路就是**把未测量写成 0**
(= h3 的实际失败)。正确规则 = **允许 + 连带证据降级**(IC3 判「不适用」+ 不得作一致性安全断言的证据 + 报告层 badge)。

**⚠️ 已落盘 h2/h3/h6 的处置 = 只勘误不重跑**:
`docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md` 追加「判据分支测量状态」小节
(H3 的 `deadlock_violations` 分支 = **未测量**;h2 的 `directory_misses` 半支 = **未测量**);
⛔ 历史 run 的 `metrics.json` **不动**(历史产物);⛔ 结论段**不重写**。

### ② 决策块计数口径(治理不变量 · 落本文件而非 `NUMBER-MAP`)

**口径**:**决策块** = §2 章下的**独立决策载体**(「主决策段」+ 各 `### 决策 N` 段);
§2.0 的选项对比表是决策的**支撑物**,**不计块**。

**实测落数**:`ADR-CEP-01` = **3** · `ADR-CEP-02` = **4**(§2 主决策「canonical 10 流清单」+ 决策 1/2/3)· `ADR-CEP-03` = **3**。
⇒ 与台账 **Entry #169** ⑤ 的登记值**一致**(此前口头按 `^### 决策` 计数得 `-02` = 3 是**漏计主决策段**,已更正)。

**落点理由**:`ADR-CEP-NUMBER-MAP` 的职权域是**编号空间**;为一个计数口径破它的冻结基线 + 耦合
`test/cep_adr_parser.hh` 的 owner 解析器,收益不成比例 ⇒ 落**本文件**(治理真源 1)。

### ③ C2:`ADR-CEP-01`/`-02`/`-03` 一决策一文档化 —— **残留登记 + 触发条件**(D0)

**裁决**:**登记为「已知残留」**(契约给出的第二条出路),⛔ **今日不拆**。

**⚠️ 这不是豁免,是带触发条件的延期** —— 台账 Entry #139 的「不排期」曾因**无边界**而被 Entry #215 补触发条件;
本则不重复该缺陷。

**触发条件(3 条,均可客观判定)**:

| # | 触发条件 | 判定方式 |
|---|---|---|
| **T1** | 某次修订 / supersede 的**取代范围**只覆盖 §2 的**单一决策块**(取代范围声明 ∩ 决策块清单 = 单元素)⇒ 触发该块的拆出 | 取代范围声明 + 决策块清单做**交集**,单元素即命中(先例:`ADR-CEP-01` D.3/D.4 的「取代范围」范式) |
| **T2** | 台账裁决(Entry 级)的**事实前提只涉及** `-01`/`-02`/`-03` 中的**某一个决策块**、且要求修改该块语义 ⇒ 触发 | Entry 的事实前提 ∩ 决策块清单 = 单元素且含「修改语义」(先例:`ADR-CEP-01` Decision D 重开) |
| **T3** | 某决策块定义的语义被新决策取代、其 **invariant 需迁移 owner** ⇒ 触发 | `test/cep_adr_parser.hh` 的 `owner_specs()` 需变更 ⇒ 即命中(实测:`ADR-CEP-02` 定义 11 条 `CEN-Ib*`,owner 前缀 `{"CEN-Ib"}` → 文件名映射) |

**触发 ⇒ 预定义执行路径 D1(不在今日执行,但路径已定 ⇒ 触发时不需重新设计)**:
新建 ADR 承载拆出决策 + 原 3 份**逐字保留**(机器行号锚零破坏)+ 各追加 SU 声明「决策 N 已由新件承接」
+ `ADR-CEP-00 §3` supersede 表更新(该表在 **M1 冻结集**内 ⇒ 此步**需冻结面授权**)。

⚠️ **先例的准确边界**(per critic 2026-11-04 SHOULD_FIX —— 修正本则初稿「先例已存在」的过强表述):
**有先例**的是 **SU 承接 / supersede 的**形态**:
① 「原文冻结 + 顶部 SU 声明 superseded + 继任真源」(`06-bis` 被 CEP 取代先例);
② 「SU 携带取代范围 + 取代后文本(规范)」(`ADR-CEP-01` D.3/D.4)。
**⛔ 无先例**的是「**拆出 + invariant owner 迁移**」这个**组合** ——
而 D1 可行性里**最难的恰是 owner 重映射**(`ADR-CEP-02` 的 11 条 `CEN-Ib*` 由 `owner_specs()` 绑**文件名**)。
⇒ **D1 的第一步 = 先跑通 `owner_specs()` 变更的 parser 回归**(`test/cep_adr_parser.hh` + `test/test_cep_canonical_invariants.cc`),
再动 ADR 拆分 —— 顺序颠倒 ⇒ invariant owner 歧义。

**⛔ 明确不采的替代路径**:
- **只在 07 号建「ADR → 决策块」索引表就算达成** —— 无触发条件的豁免登记 = Entry #139 已付过的学费再付一次;
- **ADR-01/02/03 重签 v1.0、放弃不可变** —— 违 AGENTS.md 核心不变量;63~132 处引用语义集体失效;
- **只拆 `-01`/`-03` 而 `-02` 不动** —— `-02` 恰是最重的一块(4 决策 + 11 invariants + 2 条跨仓行号锚 + 132 引用),
  拆容易的留最难的 = 产出形态契约警示的「顺手改写」反向版,且 owner 重映射这个真问题原地不动。

**⚠️ D0 的「怎么坏」与堵法**:只登记在 07 号 ⇒ 可能被读成「ADR 形态不合规被豁免」。
⇒ ① 本则明写「**这不是豁免,是带触发条件的延期,触发 ⇒ D1**」;
② **加一行机器检查**(落 `scripts/test/cep_docs_sync_check.sh` 或 CEP 测试套件,均**非冻结**)
断言「07 号含 3 份 ADR 的残留登记 + 3 条触发条件条目存在」⇒ 防**沉默删除**;
③ **3 份 ADR 自身的 SU 推迟到首次触发时再落**(触发时反正要动它们,现在加只是多 3 次冻结基线重算,零收益)
⇒ 单读某份 ADR 的读者看不到残留标记,但任何机器检查 / 治理文档都能定位到登记。

### ④ 正文勘误指针表(裁决前文本 → **以 Status Update #2 / `ADR-CEP-06` SU#1 为准**)

⚠️ 本则**不改本文件 §1 ~ §11 正文**;以下位置是**裁决前的历史文本** ——
单读本文件 / 写新 gate 的读者须以 **SU#2 + `ADR-CEP-06` SU#1** 为准。

| 位置 | 裁决前文本(历史) | 裁决后口径 |
|---|---|---|
| 本文件 `:285`(IC4 摘录) | 「不可观测 metric 标 `not-collected` + reason 字段,禁止省略/0/-1」 | 语义不变;**载体**改 per-metric `_not_collected` map(单字符串字段**废弃**)—— 规范见 `ADR-CEP-06` SU#1 §② |
| 本文件 `:521`(兜底契约段) | 同上 + `_not_collected_reason` 单字符串 | 同上 |
| 本文件 §7.2 的 13 必纳 metric 表 | 类型列只写 `int` / `float` | 取值类型放宽为「数值 **或** `"not-collected"`」—— 见 `ADR-CEP-06` SU#1 §② |
| `ADR-CEP-06` 决策 4 示例 / `:196` / `IC4` | 旧形状 | 同上(规范本体在 `ADR-CEP-06` SU#1) |
| `openspec/specs/cep-core-contracts/spec.md` `:116` / `:121` | 旧形状 + IC3 无「不适用」分支 | 须与新契约对齐(§④ 已列入未勾项) |

### ⑤ Verification(前瞻判据 · 可勾选)

- [x] C1 的三方冲突有**实测**证据(schema `jsonschema` FAIL / 3 tools 判红代码 / 测试层盖章 / h2·h3·h6 结构零)
- [x] 载体选型**排除了 B 与 C** 并给出各自不可接受的理由
- [x] 决策 5 的三分支已定,且**明确不采**「h6 改标 functional」
- [x] 「全绿」已拆成 **run-validity / hypothesis-verdict** 两级,且**预登记闸**的载体域已定
- [x] 违例类规则已订正为「允许 + 证据降级」(非「禁止 not-collected」)
- [x] 决策块计数口径已落(3 / 4 / 3),与台账 Entry #169 ⑤ 一致
- [x] C2 的残留登记 + **3 条可判定触发条件** + **D1 预定义路径**已落
- [x] `07 号` 自身的机器可查锚(防沉默删除)已指派给 builder 域
- [x] schema + 3 件冻结 tools + 测试已按 C1 落地(builder 域 + 冻结面授权)—— 2026-11-05,per 台账 **Entry #218** §⑤-2
- [x] ⚠️ **`openspec/specs/cep-core-contracts/spec.md`** 的 IC4(`:116` 旧形状)与 IC3(`:121` 无「不适用」分支)
      已与新契约对齐(builder 域 + 冻结面授权)—— 2026-11-05,per 台账 **Entry #218** §⑤-2;⛔ 未留待日后:两件冻结面互斥后再修需**两次**授权且漂移期无限
- [x] objective §9 已改造为「判据-指标需求表」(planner 域)—— **预登记闸的真源**;2026-11-04,per 台账 **Entry #218**
      ⚠️ **此为 blocker-of**:schema 的「`not-collected ⊆` 预登记」校验在 objective §9 存在前**无法实现**
      (仓内 `grep 预登记` 零载体)⇒ ⛔ builder **不得**先落 schema 并把该校验写成**恒真**占位
      (那会形成**第二个假绿**)⇒ **落地顺序强制**:objective §9 → schema/tools
- [ ] `docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md` 已追加「判据分支测量状态」小节

**关联**: `ADR-CEP-06` **SU#1**(C1 规范本体)· 台账 **Entry #215** / **#216** · **#169** ⑤(拆分登记)·
**#139**(`FullMapDirectory` 落位裁决 —— 「不排期无边界」的先例与被补先例)· **#210**(队列 #14 critic 第 2 轮的 R3/R6/R7)·
`docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md` · `docs/validation/2026-11-04-cep-protocol-e2e-coverage.md`


---

## Status Update(2026-11-06,per 台账 Entry #238)—— 「转移表真源」的**判定式(T1–T4)** + **TileLink-C 阻塞解除** + 逐族重判

> **正文吸收**:本则的**判定式 T1–T4 + T3 普查三要素**已提炼进 **§5.1.5**(per 批 2;⛔ 本则原文不回改)。

> **性质**:追加式(per 本文件 §6.2 的跨仓锚点治理不变量)—— §5.1.1 的**正文不改**,
> 触发条件句只做**零行数变化的就地澄清**;本段为**末尾追加 SU**。
> **触发**:台账 `obj-cep-platform.md` **Entry #237**(事实底稿 + 三个裁决请求)· 用户裁决 **D-3**(B3a 记 ⏭ 阻塞)。
> **审查**:`.rddf/reviews/cep-b3a-b3b-ruling.md`(**oracle → critic**,顺序不可颠倒;critic 结论 **APPROVE_WITH_CONDITIONS**,三档均无 BLOCKER)。
> **关联 change**:`backlog: cpptlm-cep-b3a-b3b-advancement`(占位,立项后回填)。

### ① 根因:「转移表真源」**从未被定义**

§5.1.1 的触发条件用了「转移表真源」这个词,但**仓内任何载体都没有它的判定式**
⇒ 2026-11-04 的裁决按裁决者心智模型(「转移表 = 按**消息**索引的协议消息状态机」)解读,
把 **TileLink-C** 与 CXL 3.0 对称 DCOH / NVLink 并列登记进「规约载体无转移表真源」组。

⚠️ **该登记对 TileLink-C 是事实错误**(per 台账 Entry #237 §1.1 实测):
`include/tlm/cep/tilelink_state_spec.hh` 自 **2026-11-02** 起即持有
**12 格全 `Known`、逐格 basis 全 `Direct`、0 格 `Unknown`** 的封闭转移矩阵
(逐格 `ref` → 锚点摘要 `TLC-01`~`TLC-12`;规范 `tilelink-c-1.7.1`,SiFive **免费开放标准**,PDF sha256 钉版)。
⇒ 触发条件所依赖的事实**当时已经成立**,裁决所写的阻塞理由**不成立**。

### ② 判定式(「转移表真源」= 四要件**全满足**才算触发)

| 要件 | 内容 | 机器可判标志 |
|---|---|---|
| **T1 存在** | 仓内规约载体持有**有限封闭**的转移结构(态集 × 事件集 ⇒ 目标集),非散文非示例 | 载体有编译期矩阵 + 计数常量 |
| **T2 可判定** | 每格有 `basis.kind` + 条款级 `ref`;`Unknown` 比例可机器统计 | **决定性变异**:删一格 ⇒ 判据必红(per 判据铁律 1.1) |
| **T3 可消费** | 存在「平台消息/意图面 → 该表索引轴」的映射,**映射每一行同样有条款级 `ref`**;无锚行显式 `pending` 且 **fail-closed**(引擎 SHALL NOT 消费) | 映射表进规约载体(编译期锁)+ 毒化实测 |
| **T4 完备** | `Unknown` 格占比 = 0(或空集语义被规范明确) | 计数交叉核对(编译期 `static_assert` 已有) |

### ③ 逐族重判(**达成度**与**闸评估**分列 —— 两者不是同一维度)

| 协议族 | B3b 达成度 | T1 | T2 | T3 | T4 | 判定 |
|---|---|:--:|:--:|:--:|:--:|---|
| CHI RN-F | **1/6 达成**(自带 `kChiOpcodeTable`,无独立 state spec 载体) | ✅ | ✅ | ✅ | ✅ | 已达成 |
| **TileLink-C** | **✅ 已达成**(B3b **2 达成 / 分母 6**,2026-11-06 · 三:运行时引擎落地,per 台账 **Entry #246**;口径见 SU · 四) | ✅ | ✅ | **✅** | ✅ | ✅ **已达成**(引擎 + L4 事务级 E2E + 判据 4 件套) |
| CXL 2.0 非对称 | 未达成 | ✅ | ✅ | — | **❌** | ⛔ **显式冻结**(42/68 `Unknown`,覆盖率 26/68) |
| CXL 3.0 对称 | 未达成 | ❌ | — | — | — | 未触发(载体无矩阵) |
| NVLink | 未达成 | ❌ | — | — | — | 未触发(载体无矩阵) |
| UALink | 未达成 | ❌ | — | — | — | 未触发(载体无矩阵) |

### ④ TileLink-C:阻塞**已解除**,剩余是**工程项**而非**条件闸**

- **解除**:`tilelink_state_spec.hh` 的 12 格矩阵**全部有条款级锚** ⇒ 补运行时引擎**不再构成推理填空**
  ⇒ 2026-11-04 裁决的阻塞理由(「规约载体无转移表真源」)对 TileLink-C **不再成立**。
- **剩余**:**T3 普查** —— 「CohIf 消息/意图面 → `Cap`/`Grow`/`Prune`/`Report` 事件轴」的映射
  (⛔ 该映射**在仓内任何载体都不存在**)。
- **(2026-11-06 · 二,per 台账 Entry #242)**:上述映射**已普查落地** —— 行 id `TLC-M-01`~`TLC-M-11`(**11 行闭集 = canonical 10 流 + atomic 意图**),逐行条款级 `ref` 全 `Direct`、`Pending` 0 行、fail-closed 访问器 + 编译期锁 + 反向 fixture;载体 = `tilelink_state_spec.hh` 的 `kT3MappingCells`。剩余前移为**引擎 change**(见 `docs/research/2026-11-06-cep-tilelink-t3-mapping-anchor-summary.md` §6)。
- ⚠️ **不是**「条件未触发 ⇒ 不用做」:触发事件**已客观发生**,剩余项是**有实证出口的工程项**。
- ⚠️ **也不是**「已触发 ⇒ 立即开引擎」:T3 未锚定前造引擎 = 换一种推理填空。
- **规范可读性** = T3 可得条款级 basis 的前提(TileLink-C 规范正文可读;这与 CXL 全程禁读规范正文**完全相反**)
  ⇒ T3 属**低成本**普查,**不属**填空。

### ⑤ T3 普查的三要素(**定义** —— 防下一个未定义术语)

| 要素 | 内容 |
|---|---|
| **行宇宙** | 「平台消息/意图面 → Event 轴」的**全部行** = 哪些消息/意图 × 4 个事件(**逐行列出,不是抽样**) |
| **锚标准** | 映射行的合法锚 = **规范条款级定位**(TileLink-C 规范可读 ⇒ 可得);若某行只靠仓内构造(如 `ADR-CEP-12` 定义的 6 类消息类)⇒ 须标 `self-derived` 并**显式登记**,⛔ 不得当规范锚 |
| **交付物与登记位置** | 映射表进**规约载体**(编译期锁)+ 逐行 `basis` + `ref`;⛔ 悬空判据禁令(per `ADR-CEP-02` 附录)⇒ 载体不存在前该判据**不可执行** |

> ⚠️ **与 `ADR-CEP-12` 既有映射表的关系**:`tilelink_c_adapter.hh` 的「10 行逐消息表」映射目标是
> `ADR-CEP-12` 的 **6 类消息类**,**不是**本 SU 的 Event 轴 ⇒ **两轴不同 artifact** ⇒
> 该表**不满足 T3**,⛔ 不得据「adapter 已有 10 行表」宣称 T3 已满足。

### ⑥ 后果 / 代价

| 面 | 后果 |
|---|---|
| **§5.1.1 正文** | **零改动**(触发条件句只做**零行数变化**的就地澄清) |
| **B3b 达成度** | **维持 1/6**;⛔ 本 SU **不**把 B3b 改写为「已达成」 |
| **TileLink-C** | 阻塞**解除**;剩余前置 = T3 普查(工程项) |
| **CXL 2.0** | ⛔ **显式冻结**(T4❌,42/68 `Unknown`)—— 该族**不得**因「有 68 格矩阵」被误推进 |
| **CXL 3.0 / NVLink / UALink** | 逐族单列(⛔ 不得混排在 B 类)⇒ 各登记 ⏭(`reason` + `trigger_condition` + `ledger_entry`,对齐 B2 的 5 授权⏭ 形态) |
| **新增 invariant 数** | **0**(本 SU 只定口径与逐族判定,不新增编号) |
| **归因 id** | 本 SU **不**新造 id 体系;逐族 ⏭ 的登记归因沿用 `obj-cep-platform` 台账既有形态(per `ADR-CEP-02` 附录的归因纪律) |

### ⑦ Verification(前瞻判据 · 可勾选)

- [x] §5.1.1 触发条件句已完成**零行数变化**的就地澄清(指向本 SU 的 T1–T4) —— **2026-11-06 · 三 回填**(per 台账 **Entry #245**;证据:§5.1.1「未达成部分的触发条件」句已带 T1–T4 指针,内容完成于 Entry #238)
- [x] `docs/validation/2026-11-04-cep-protocol-e2e-coverage.md` §3 B 类已改写(TileLink-C 归类)+ **逐族单列** —— **2026-11-06 · 三 回填**(per 台账 **Entry #245**;证据:该文 §B「2026-11-06 逐族改判」+ §B-2 TileLink-C 单列)
- [x] 四族(CXL 2.0 / CXL 3.0 / NVLink / UALink)的 ⏭ 登记已落 `obj-cep-platform` 台账(`reason` + `trigger_condition` + `ledger_entry`) —— **2026-11-06 · 三 回填**(per 台账 **Entry #245**;证据:台账「逐族 ⏭ 登记」表四行齐,per Entry #238/#233)
- [x] T3 普查已立项(change 目录 + 规约层载体落点 + 逐行 `basis`/`ref` 载体),且**行宇宙完整**(非抽样)(2026-11-06 · 二:Entry #241 立项 + #242 实施)
- [x] T3 普查的每一行**要么**有条款级 `ref`,**要么**显式 `pending` + fail-closed(引擎 SHALL NOT 消费)(2026-11-06 · 二:11 行全 Direct,0 pending,`t3_consumable_event_mask` fail-closed)
- [x] **决定性变异**:删 T3 映射表任一行 ⇒ 对应判据**按正确原因红**(per 判据铁律 1.1)(2026-11-06 · 二:编译期 `static_assert` 行数锁 + 4 反向 fixture 运行时红,见 Entry #242)
- [x] `ualink_4` 声明落地后,§5.1.1 UALink 行与 §11 计数已同步(⚠️ 声明 ≠ 装配 ⇒ 「实例级装配仍 backlog」的措辞**须保留**) —— **2026-11-06 · 三 回填**(per 台账 **Entry #245**;证据:§5.1.1 UALink 行含「✅ `ualink_4` 实例已声明」+「声明 ≠ 装配」措辞保留 + 「6 族 / 8 实例」计数,per Entry #239/#240)

**关联**:台账 **Entry #237**(事实底稿 + 裁决请求)· **#238**(本裁决)· **#231** · **#233**(⏭ 登记形态)· **#236**(B2 达成)·
`ADR-CEP-02` **SU 2026-11-02 R-T4**(`perm` 值域锁)+ **SU 2026-11-06**(`perm` 实例级语义)·
`ADR-CEP-04` **SU 2026-11-03**(tracker 选型 + 理由登记)+ **SU 2026-11-06**(`ualink_4` 取值裁决)·
`ADR-CEP-11 §3`(`protocol_version` 锁表)· `ADR-CEP-12 §2` 决策 1(canonical 10 流)·
`ADR-CEP-07` Status Update 2026-11-04 ① · `include/tlm/cep/tilelink_state_spec.hh` · `include/tlm/cep/cxl_state_spec.hh` ·
`.rddf/reviews/cep-b3a-b3b-ruling.md`(审稿报告)


---

## Status Update(2026-11-06 · 四,per 台账 Entry #249)—— 「转移表真源」的**来源效力**与 **B3b 计入口径**

> **正文吸收**:本则的**载体形态分类 + 来源效力分档**已提炼进 **§5.1.5**(per 批 2;⛔ 本则原文不回改)。

> **性质**:末尾追加 SU(append-only)。⛔ 不改 §5.1.1 行数;⛔ 不新增 invariant 编号;⛔ 不改 schema / ABI。
> **触发**:台账 **Entry #248**(剩余任务盘点:4 族缺口全在规约层)· 用户裁决请求(「先起 architect 裁决」)。
> **审查**:`.rddf/reviews/cep-t1-source-effect-ruling.md`(**oracle 1 轮 + critic 2 轮**;critic 第 2 轮 **APPROVE · mergeable: yes**)。
> **关联 change**(⚠️ **原 `backlog: cpptlm-cep-b3a-b3b-advancement` 占位已回填** —— per 台账 Entry #250):
> `openspec/changes/archive/2026-11-06-cpptlm-cep-tilelink-t3-mapping-survey/`(T3 映射锚点普查,✅ 已完成归档)· `openspec/changes/archive/2026-11-06-cpptlm-cep-tilelink-runtime-engine/`(TileLink-C 运行时引擎,✅ 已完成归档)· **本条不新建 change**。

### ⓿ 范围(⛔ 必读:本条**不**做什么)

| 做 | ⛔ 不做 |
|---|---|
| 只裁 **B3b 的计入口径**(哪些族可计入达成度、以什么证据计入) | ⛔ **不**重定义任何既有载体的 `basis` 枚举语义;⛔ **不**对既有格做**追溯降级**(CXL 22 格 `PublicDirect` / MESIF 35 格 `Primary` 等**维持原状**) |
| 明确「四要件 = 合取」与「唯一状态 token」 | ⛔ **不**改 `obj` 的 DoD 文本(属 planner/用户域,见 §④) |
| 登记 2 项 backlog(来源类型字段 / 格粒度 DoD) | ⛔ **不**改 schema、⛔ 不新增 invariant 编号 |

**既有载体的 basis 语义已不统一**(4 套:`Direct/Analogical/Inferred` · `PublicDirect/PublicAnalogical/ArchforgeAnchor/SelfDerived` · `Primary/Structural/Derived/Analogical` · `SelfDerived/Pending`)—— 统一是**独立 change** 的活(backlog ①),不属于本条。本条只解决「**能不能计入 B3b**」。

### ① 问题

4 族缺口全在规约层:CXL 2.0 `T4❌`(26/68)、CXL 3.0 / NVLink / UALink `T1❌`(无矩阵)。三族规范受限(CXL ECA / UALink Evaluation Copy / NVLink 无公开规范)。
**待裁**:由公开非规范来源 + 一步推断构建的矩阵,能否计入 B3b 的「转移表真源」?

### ② 裁决

**D-1(来源效力)** — 适用于**新族是否计入**,不追溯既有格:
- `Direct` 的语义 = **规范正文条款级**(§/Table 级);
- 公开非规范来源(博客 / 内核文档 / 厂商补丁 / webinar / arXiv 摘要)**不构成** `Direct`;
- 由非规范来源构建的矩阵**可以**作为**探索载体**存在(basis 标注为弱档、`pending=true`、**fail-closed 禁止引擎消费**),但**不计入 B3b**;
- **carve-out(⛔ 不追溯)**:既有载体的枚举**注释语义不变** —— `cxl_state_spec.hh` 的 `PublicDirect`(「公开来源明确这样写」)与 22 格 `pending=false` **维持原状**,其效力由**其自身 change 链**(台账 Entry #122/#123 + `ADR-CEP-11` SU + `cxl_state_spec` 口径 ⑤)授权;本条**不 reopen**、不改其 basis、不改其计数(CXL 2.0 已因 `T4❌` 冻结,B3b 计数从不依赖该 22 格);
- **域边界(B3a vs B3b)**:同为非规范材料,**声明域**可被机器消费(`ualink_4` 13 键经 `channel_dependency_graph` / 装配闸消费,per Entry #239),**矩阵域**必须 fail-closed —— 差异理由是**错误代价不对称**:声明错 ⇒ 能力声明失准(可由实验证伪、可回滚);矩阵格错 ⇒ **引擎静默实现错误状态转移**且看起来权威(零副本契约:`cxl_state_spec.hh` 的「规约改一格、adapter 必红」);
- ⚠️ **判据可执行性的诚实声明**:当前载体**没有**「来源类型」字段(`Transition.ref` 为自由字符串)⇒ 本条的「效力」判定**不可机器自动判**,只能由**人工抽样审计** + 来源注册表(backlog ①)承担;⛔ **不得**宣称本条有 grep 级自动闸。

**D-2(两轴达成)** — B3b 的达成 = **触发轴 ∧ 交付轴**,两轴**都不许**部分计分:
- **触发轴** = T1–T4 **合取**(任一子集 ✅ 不构成触发;T1 单独成立**不构成**立项依据);
- **交付轴** = 运行时引擎 + L4 事务级 E2E(其判据形态见 TileLink-C 的先例);
- ⚠️ **载体形态分类**(否则 D-1 与既有计数矛盾):
  | 形态 | 族 | T1/T2 的等价要求 |
  |---|---|---|
  | **状态矩阵型** | TileLink-C · CXL 2.0/3.0 | 态集×事件集矩阵 + 逐格 `basis`/`ref` + 删格必红 |
  | **opcode/事件表型** | CHI RN-F | 表级 `ref` + 表完备性 `static_assert`(⛔ 不要求逐格 basis —— 其表是 opcode→channel/flow,**不是**态×事件矩阵;形态依据 = 2026-11-04 architect 裁决 + `07 号` SU 2026-11-06 §③ 的 CHI 逐要件已裁 ⇒ 本条只**登记分类,不改其计数**) |

**D-3(逐族终态,现行纪律下)**:
| 族 | 状态 | 理由 |
|---|---|---|
| CHI RN-F | ✅ 计入(1) | 规范可读;opcode 表型(T1/T2 按其形态成立);交付轴已有引擎 + E2E |
| TileLink-C | ✅ 计入(2) | 规范可读;状态矩阵型 12 格全 `Direct` + T3 已闭合;交付轴已有引擎 + E2E |
| CXL 2.0 非对称 | ⛔ 显式冻结 | `T4❌`(26/68);规范禁读 ⇒ 纯 AI 作业下覆盖率不可补 |
| CXL 3.0 对称 | ⏭ 受限 | 载体无矩阵;非规范来源矩阵**不计入**(D-1) |
| NVLink | ⏭ 受限 | 同上;且**无公开规范** |
| UALink | ⏭ 受限 | 同上;Evaluation Copy 禁读 |

⇒ **B3b = 2 / 分母 6 / 处置 {1 显式冻结, 3 受限-⏭}** —— ⛔ **不得**写单一标量 `2/6`(会把「建 68 格后冻结」与「0 起步」等权),⛔ **不得**写 `2/2`(分母收缩,违反 B2 的「分母永不可缩」先例)。

**D-4(升级路径,⛔ 不自动)** — 某族满足任一条件须**重新普查**并按 T1–T4 重判:
1. **规范可读化**:CXL/UALink 的 ECA/Evaluation Copy 解除,或联盟发布可读规范;NVLink 发布**条款级**规范文档(⚠️ 白皮书/编程指南**不算**);
2. **人类成员条款摘要**(AI 不接触正文):已有 spec 级先例(`cep-cxl-dcoh-symmetric-state-spec` / `cep-nvlink-sharp-state-spec`);**证据档位** = 人类成员按条款摘录 ⇒ 该格 `ref` 可标**条款级**(§/Table 级定位)⇒ 可计 `Direct`(前提:摘录含条款定位 + 人类成员签署);⚠️ 阻塞 4 族的是**外部条件 + 人力资源**,⛔ 不是「永久外部阻塞」;
3. **来源类型机制落地**(backlog ①:载体 `RefKind` + 来源注册表 + 决定性变异)⇒ 届时「效力」才**可机器判**;
4. **`CellKind::Free`**(backlog ②:规范明确「implementation defined」的格)⇒ 届时 T4 的「空集语义被规范明确」才有载体。

### ③ 判据纪律(硬句)

- T1–T4 为**合取**;任一子集 ✅ **不构成**触发、**不构成**达成度、**不得**作为立项/补引擎依据;
- `obj` 的 B3b 行**只许一个状态 token**(`✅达成 / ⛔冻结 / ⏭受限`),**不得**出现「T1✅」「T2✅」字样(避免「部分达成」误读);
- 逐要件 grid 只许留在 `07 号` 附录,表头须写「**非判定粒度**」。

### ④ 对 DoD 的影响(⛔ 本条不代改 DoD —— 交用户/planner 裁决)

| 选项 | 内容 | 评价 |
|---|---|---|
| 甲 | B3b 口径改为「可读规范族 N/N」 | ⛔ **不建议**:分母收缩,违反 B2 先例(`obj` §6.2 B2 的「分母永不可缩」) |
| 乙 | 维持 6/6 | ⚠️ 措辞须改:「永久外部阻塞」→「**外部条件 + 人力**受限」(D-4 已有 4 条升级路径) |
| 丙 | 冻结 objective | ⚠️ 放弃期权 |
| 丁 | B2 形态:`6/6 = 2 有引擎 + 4 授权⏭` | ⚠️ **不推荐**:把外部许可条件计入 DoD 达成,正是 07 号 §5.1.1 防自欺条款要禁的「降格后宣称达成」 |
| **戊(建议)** | **DoD 条目拆分**:`B3b-1 可读规范族运行时引擎 = 2/2 ✅` + `B3b-2 受限族 = 0/4 ⏭(含触发条件)`;终态 Go 要求两条**各有显式裁决** | ✅ 与 B3a 的「拆两行、各自独立可判」同型;不会出现「B3b ✅」而看不到 4 族审计缺口 |

### ⑤ 后果 / 代价

- **正面**:DoD 不被不可读规范挟持;`Direct` 档保持含义;4 族不被「推理填空」污染;探索价值由「影子矩阵(不消费、不计入)」保留。
- **负面**(随 §④ 选项而异):若选**戊** ⇒ `B3b-1 = 2/2 ✅` + `B3b-2 = 0/4 ⏭`;若选**乙** ⇒ `2/6`;若选**甲** ⇒ `2/2`(⚠️ 分母收缩,不建议);若选**丙** ⇒ objective 冻结;若选**丁** ⇒ `6/6 = 2 引擎 + 4 授权⏭`(⚠️ 不推荐)。未来规范开放须重做普查(成本 = 一次 T1–T4 全流程)。
- **风险**:4 族的 ⏭ 目前只有台账文字登记(强度弱于 B2 的 4 项机器校验);本条**登记**该缺口,不假装等价。

### ⑥ Verification(前瞻判据 · 可勾选)

- [ ] **状态/达成度记账句**(⛔ 不含 DoD 目标文本)全仓 **≥12 处**同批改写,统一为「**四要件 grid 只留 `07 号` 附录 + 状态行写 `2 达成 / 分母 6 / {1 冻结, 3 受限}`**」(逐处具名:`obj` 6 处 / `07 号` 4 处 / coverage 2 处);⛔ 不得只改 1 处(per `ADR-CEP-12` 记录的同型「双记账复发」事故)。⚠️ **DoD 目标文本本身**(`obj §6.2` 的 B3b 条目)的措辞形态由 §④ **用户裁决后的选项**决定(甲/乙/丙/丁/戊 各不同)⇒ ⛔ 本条**不**预设任何选项的形态
- [ ] 4 族 ⏭ 的 `reason` 已改写:从「T1 未触发(载体无矩阵)」→「T1 可满足 / 不计入(D-1:非规范来源不构成 `Direct`)」;**判定结论不变,理由换**(⛔ 不得沿用与事实不符的旧理由)
- [ ] 逐族套用 D-1 后 **B3b 计数仍为 2**(CHI 按 opcode 表型、TileLink-C 按状态矩阵型)—— 一致性最低判据
- [ ] D-4 的 4 条升级路径已落台账(可审计:reason / trigger_condition / ledger_entry)
- [ ] §④ 五选项交用户裁决;⛔ 用户裁决前不得宣告终态 DoD Go、不得单方修改 DoD 文本
- [ ] 冻结面授权:`07 号` 追加 SU 走两步授权(`cep_freeze_check.sh --update --authorized-by "Entry #249"` → 清单单独 commit)

---

## Status Update(2026-11-06 · 五,per 台账 Entry #257)—— D-1「来源效力」的**机器可判落地回填**(命题范围收窄 + 载体覆盖账 + D-4 ③/④ 回填)

> **正文吸收**:本则的机器可判落地口径已随 **§5.1.5** 的来源效力分档收敛(per 批 2;⛔ 本则原文不回改)。

> **性质**:末尾追加 SU(append-only)。⛔ 不改 §5.1.1 行数;⛔ 不新增 invariant 编号;⛔ 不改 schema / ABI;⛔ **不改** SU · 四 D-1 的核心裁定。
> **触发**:台账 **Entry #252**(P1 实施)/ **#253**(P1 验收 PASS)/ **#255**~**#256**(契约对齐 + 归档)+ 用户裁决「按建议顺序实施」。
> **审查**:`.rddf/reviews/cep-su5-d1-machine-checkable.md`(**oracle 1 轮**(18 条建议全吸收)+ **critic 1 轮**(**APPROVE_WITH_CONDITIONS · mergeable: yes**;2 SHOULD_FIX + 3 NIT 全吸收))。
> **关联 change**:`openspec/changes/archive/2026-11-06-cpptlm-cep-basis-source-kind-and-cell-free/`(✅ 已完成归档 · delta 已合并进 `openspec/specs/cep-basis-source-kind/spec.md`)· `openspec/changes/archive/2026-11-06-cpptlm-cep-source-kind-contract-alignment/`(✅ 已完成归档 · delta 已合并进 `openspec/specs/cep-ualink-inc-state-spec/spec.md`)。
> **冻结面**:本段追加 ⇒ 两步授权(`cep_freeze_check.sh --update --authorized-by "Entry #257"` → 清单**单独** commit)。

### ⓿ 范围(⛔ 本条**不**做什么)

| 做 | ⛔ 不做 |
|---|---|
| 回填 SU · 四 D-1 的「诚实声明」(收窄到**可机器判的那部分**) | ⛔ **不**改 D-1 的**核心裁定**(公开非规范来源仍**不构成** `Direct`、仍**不计入** B3b) |
| 登记「载体**来源类型**的落地映射」+ 其**例外与缺口** | ⛔ **不**改 §5.1.1 行数;⛔ 不新增 invariant 编号;⛔ 不改 schema / ABI |
| 回填 D-4 ③/④ 与 B3b 两域记账 | ⛔ **不**宣称「条款级来源的**充分性**已被机器证明」;⛔ 不顺手修 §11 的既存 `7` vs `8` 漂移 |

---

### ① D-1 诚实声明的**收窄**(判定结论不变,命题范围换)

SU · 四 D-1 原文自陈:「当前载体**没有**来源类型字段 ⇒ 本条的『效力』判定**不可机器自动判**,只能由**人工抽样审计** + 来源注册表(backlog ①)承担;⛔ **不得**宣称本条有 grep 级自动闸。」

**落地后的命题切分(2026-11-06)—— 2 条可机器判 + 4 条人工(⛔ 不是「四命题」)**:

| # | 命题 | 可否机器判 | 载体 |
|---|---|---|---|
| ① | `ref` → **已登记来源 id** 的**路由** | ✅ **可** | `source_registry.hh::resolve` + 4 载体 `resolve_ref()` |
| ② | `basis` ↔ 注册表**所声明** `kind` 的**一致性** | ✅ **可** | 4 载体 `basis_ref_kind_consistent()` + 闸 4 态决定性变异 |
| ③ | 注册表对来源 `kind` 的判定**本身** | ❌ **人工签署**常量 | `kSources[]` 是人工签署的常量 |
| ④ | `ref` **是否确实指向**该条款(指向性) | ❌ **人工/抽样** | — |
| ⑤ | 该条款**是否足以**支撑该格语义(充分性) | ❌ **人工/抽样** | — |
| ⑥ | `CellKind::Free` 的「规范明确」判定 | ❌ **人工/抽样** | — |

⇒ **机器保证「不自相矛盾」,不保证「为真」**;D-1 的**效力裁定仍不可全自动**。

⚠️ **与既有纪律的切分**:`07 号` SU · 四 §① 的「⛔ 不得宣称本条有 grep 级自动闸」**约束对象 = ③④⑤⑥**;本闸只覆盖 ①② ⇒ **两条不矛盾**(不是对旧纪律的反转)。

⚠️ **术语切分**:`ADR-CEP-12` 的「认识论三档」(规则执行 / 来源锚定 / 锚定建模声明)与本条的 `RefKind` 三档(`spec_clause` / `public_nonnormative` / `repo_derived`)是**两套正交分级**,⛔ 不得互换引用。

⚠️ **可审计性边界**:8 条来源中**仅 1 条**有截断 sha256、其余为 `n/a` ⇒ ⛔ 不得据此宣称「来源字节可复现」。

**工件归属**(避免双记账式误指):① 与 ② 的**路由/一致性**由闸与载体函数承担;**注册表双载体逐条一致**(`id`/`kind`/`title`)由**单测** `test/test_cep_source_registry.cc` SRK-03 承担,闸只比对 **id 集合 + 顺序**。

### ② 载体**来源类型**的落地映射(⛔ 按**载体**,不按 token 名)

> ⚠️ **术语**:SU · 四 D-2 的「载体形态」= 状态矩阵型 / opcode 表型;本节**不**复用该词。

| 载体 | 格数 | `basis` 档 | ⇒ `ref_kind` | 备注 |
|---|---|---|---|---|
| tilelink 主矩阵 | 12 | `BasisKind::Direct` | `spec_clause` | TileLink = free & open standard,规范正文可读 |
| tilelink T3 映射 | 11 | `T3BasisKind::Direct` | `spec_clause` | ⚠️ 其中 **5 行**(`TLC-M-06~10`)的 `ref` 指向**仓内锚点摘要** ⇒ 见下方**例外** |
| cxl | 68 | `BasisKind::PublicDirect` | `public_nonnormative` | **24 格** |
| cxl | 68 | `BasisKind::SelfDerived` | `repo_derived`(规则实为「≠ `spec_clause`」) | **44 格**;其中 **42 格** `Unknown` 且 `ref` 为空 ⇒ **无外部来源**,非「已锚定」 |
| mesif | 35 + 5 | **全 4 档** | `repo_derived` | `Primary` 额外要求 `ref` **非空** |
| ualink | 11 + 4 | `SelfDerived` / `Pending` | `repo_derived` / `public_nonnormative` | 经 **token 文法**(`C-` / `N-`)判定,⛔ 非直接比较 `RefKind`;⚠️ `ualink-c10` 条目标题已披露「**非规范公开来源**支撑的建模声明」⇒ `SelfDerived ⇒ repo_derived` 掩盖了「仓内锚点 → 公开非规范来源」这一跳 |

**例外登记(⛔ 人工签署,不得静默改判)**:注册表把 `tilelink-t3-anchor`(仓内锚点摘要)登记为 `spec_clause` —— 这是 **D-4 ②「人类条款摘要」形态的首次实例**;若改判 `repo_derived`,该 5 行的 `Direct` 将不一致并牵动 T3 / B3b ⇒ **只能按例外登记**。
> **签署授权出处(可审计)**:注册表条目的落地授权 = 台账 **Entry #252**(P1 实施回执)/ **#256**(契约对齐归档);该 5 行 `Direct` 的原始依据 = 台账 **Entry #241**(T3 映射锚点普查,11 行全 `Direct`)。
> ⚠️ **该例外是**两级转述链**(T3 摘要 → 上游锚点摘要 §4),**行内无 `D1 §x.x` 级定位** ⇒ 它**不是** D-4 ② 的合规实例(后者要求「摘录含条款定位 + 人类成员签署」),而是**已披露的例外**;⛔ 不得据此宣称 D-4 ② 已落地。

**⛔ carve-out 保护(不构成追溯降级)**:CXL 22 格 `PublicDirect` + `pending=false` 的 `basis` / `pending` / 计数**三轴均不变**;`public_nonnormative` 是**新增的跨载体维度**,不是对原枚举的降级。SU · 四 D-1 的 carve-out 原文不变。

**载体覆盖账与后果**:
- ✅ **已接入(4)**:`tilelink` · `cxl` · `mesif` · `ualink_inc`;
- ⛔ **N/A(1)**:`CHI RN-F`(opcode 表型,无逐格 `basis` ⇒ 无 `ref` 可判);
- ⚠️ **未接入(2)**:`cxl_dcoh`(`EvidenceKind` 第三套 token)· `nvlink_sharp`(同)⇒ 二者恰为 **B3b-2 的 CXL 3.0 / NVLink** ⇒ **本闸不保护这两族的升级路径**;二者升格前**须先迁移**到 `RefKind`(登记为**升级前置**,⛔ 不是「已覆盖」)。

**⚠️ 已登记的实现漂移**:归档件 `design.md` D-2 表与代码存在差异(cxl 弱档 / ualink `Pending` 写成「⇒ `repo_derived`」,代码实为「≠ `spec_clause`」)⇒ **代码为准**;归档件不可改,在此登记。

### ③ `CellKind::Free` 的落地与边界

- 语义 = 规范**明确**「实现定义 / 自由选择」,与 `Known` / `Unknown` / `Illegal` 互斥;**仅存在于 2 个有 `CellKind` 的载体**(tilelink **三态** · cxl **四态**);
- 消费端显式分支 `transition_is_free()`(⛔ 不得归入 `Known`/`Unknown`);
- **校验器已同步接入**(2026-11-06 补齐):tilelink / cxl 的 `transitions_are_valid_for` 均计入 `Free`,并校验「目标掩码/目标态无意义 ⇒ MUST 为 0 · `ref` 非空 · `note` 非空 · basis 必须是**规范明确**档」⇒ 首个真实 Free 格落下**不会**编译红;
- **覆盖率口径统一**:cxl 的 `kLegalTransitionCoverageNum` 与 `coverage_is_not_regressed()` **同口径**(含 `Free`)⇒ ⛔ 不得以 `Free` 填充抬高 AC-9 覆盖率;
- ⚠️ **真实 `Free` 格 = 0**(规范可读化后填充)⇒ 本条只登记**机制**,⛔ 不宣称已有 Free 语义数据。

### ④ 契约面登记

`openspec/specs/cep-ualink-inc-state-spec/spec.md` 已含 requirement `cep-ualink-inc-state-spec-self-containment`(**载体自足 + 唯一白名单 `"tlm/cep/source_registry.hh"` + 扫描器鉴别力 + 反向 fixture**)—— **登记既有 requirement**(Entry #256 归档时合并),非本条新增。

### ⑤ D-4 回填与 B3b 两域记账

**D-4 ③(来源类型机制)**:✅ **已落地**(`source_registry.hh` + `sources.json` + 4 载体 + 闸 4 态变异;Entry #252/#256)—— ⚠️ 可机器判范围**收窄**为「类型路由 + 一致性」(①②),充分性仍人工(④⑤⑥);⛔ backlog ① 中的「**basis 枚举统一**」维度**仍未做**(4 套枚举并存,靠**按载体映射**绕开)。

**D-4 ④(`CellKind::Free`)**:枚举 / 计数 / 消费分支 / 校验器 / 覆盖率口径**已落地**;**`Free` 类语义无真实格**(= 0)⇒ T4 的「空集语义被规范明确」对 **`Free` 这一类**仍**无真实载体**(⚠️ 与 tilelink 既有的 2 个「规范明确**空集**」格(`Known ∧ mask = 0`,TLC-06/07)**不是**同一件事,勿混读)。

**B3b 两域记账**(SU · 四 §④ 的**选项戊**已经用户裁决,Entry #251,并落 `obj §6.2`:`B3b-1 = 2/2 ✅` + `B3b-2 = 0/4 ⏭`):本文件为 append-only ⇒ **不回写** SU · 四 §④ 表格;后续**DoD 语境**用 `B3b-1` / `B3b-2`,**族级审计语境**用 SU · 四 D-3 三元组(`2 达成 / 分母 6 / {1 冻结, 3 受限}`);两套记账**不得互相覆盖**;⛔ 不得把 `B3b-1` 的 ✅ 扩写为「B3b 整体达成」。

⇒ **对 B3b 计数零影响**:仍 = **2 达成 / 分母 6 / {1 显式冻结, 3 受限-⏭}**。

### ⑥ Verification(可勾选验收判据 · 前瞻)

- [ ] `07 号` 已追加本 SU,且 D-1 的**核心裁定**(公开非规范来源不构成 `Direct`、不计入 B3b)字面未变
- [ ] 载体覆盖账(4 已接入 / 1 N/A / 2 未接入)与代码**逐条一致**;未接入的 2 族已登记为**升级前置**
- [ ] `CellKind::Free` 的校验器接入 + 覆盖率口径统一,**有测试载体**(SRK-05 合成矩阵:合法通过 / 违规必红)
- [ ] 判据的「可机器判 / 不可机器判」四命题切分已写入,且与 SU · 四 的「不得宣称 grep 级自动闸」**不矛盾**
- [ ] D-4 ③/④ 与 B3b 两域记账已回填;`obj §6.2` 的 `B3b-1` / `B3b-2` 未被本 SU 重复记账
- [ ] 冻结面两步授权已执行:台账 **Entry #257** 授权 → **commit 本 SU** → `cep_freeze_check.sh --update --authorized-by "Entry #257"` → **清单单独 commit** → 无参复验 provenance 通过


---

## Status Update(2026-11-07 · 八,per 台账 Entry #282)—— **G1~G3 的 4 条待裁决点裁定**(architect)+ G1 形态修正为「组合式 wrapper」

> ⛔ **决策正文见 [`ADR-CEP-19`**](../adr/ADR-CEP-19-*.md)的对应 Status Update**(G1~G3 的 4 条待裁决点裁定 / 组合式 wrapper 形态修正)。
> 本节原为实施状态登记,per 台账 Entry #333 架构文档治理已移除;**状态见 `objective-cep-platform §4.0` / `§4.0-a` / `§11`**。

## Status Update(2026-11-08 · 十一,per 台账 Entry #288)—— **「Core·TrafficGen 主干」的架构决策登记**(10 条统一)+ 实施前**不得宣称已落地**

> **正文吸收**:本则的「**经网络**」定义已提炼进 **§6.3.1**(per 批 2;⛔ 本则原文不回改)。

> **性质**:末尾追加 SU(append-only)。⛔ 不改 §2 / §3.1 / §5.1.1 / §11 与 SU · 一 ~ 十 的任何一段;⛔ **零**代码 / 契约 / 冻结 main spec 改动;⛔ **不**改任何 DoD 结论。
> **触发**:用户对 CEP 架构定位的复核(「CEP 应是在现有拓扑链接上对互连模块做带一致性协议的建模,且原业务 Bundle 可接入到 CEP 互联网络」)⇒ 逐条讨论 10 条决策(讨论纪要 `.rddf/reviews/cep-ualink-e2e-framework-discussion.md`)⇒ 全部统一 ⇒ 落决策登记。
> **审查**:`oracle`(10 条决策的独立架构复核)→ `critic`(本段 + `ADR-CEP-19` 对应段的架构审稿),顺序不可颠倒。

### ① 决策登记(摘要;逐条依据见讨论纪要 + `ADR-CEP-19` SU 2026-11-08)

| # | 决策 | 结论 |
|---|---|---|
| D-1 | 可观测面与 13 必纳 metric 的来源 | **latency 测发起端(wrapper)**;wrapper **只暴露原始观测事实**,聚合由 harness(`test/`)消费 ⇒ **零基类改动、零既有引擎波及** |
| D-2 | 双向映射的对侧语义 | **命中路径也回 CPU 侧响应**(wrapper 就地生成 `CacheRespBundle`)⇒ 「一请求一响应」恒成立;响应侧映射**归 wrapper**;⚠️ 须写明「**承载层回 NoD 响应 ≠ 协议引擎发射**」。⚠️ **勘误**:`CacheRespBundle` **实有** `is_hit` / `error_code`(`cache_bundles_tlm.hh:91-92`)⇒ 纪要 §四 ① 的「无 status / hit-miss 标志」**有误** ⇒ 正面后果:拒绝路径**可**回 `error_code` 错误响应 |
| D-3 | 引擎可观测面的扩展边界 | wrapper 暴露**公开观测 API**;违例类在不适用场景 ⇒ `not-collected` + **显式登记降格**;第一批目标族 = **CHI RN-F**(有回响应 + 有并发写) |
| D-4 | 「接收器」的角色 | **不新建 Sink**(后置为 `ab_compare` 的 baseline 侧)+ wrapper **单一模块双角色** |
| D-5 | 拓扑形态 | **两个拓扑并存**:**A(直连)** = 全绿载体;**B(经网络)** = 互联可达 + 桥投影验证,⛔ **不承载全绿**(桥的 flow_id 5→3 是**有损**投影) |
| D-6 | 冻结 spec MODIFIES | `openspec/specs/cep-stub-cache-core/spec.md` 改 **`:56` 单条**;⛔ **`:71` 已撤销**(2026-11-08 勘误:其 WHEN 限定在**命中**路径,与拓扑 B 的 miss/request flit 路径**不冲突**);⛔ `:63` **不动** ⇒ 连带 `ADR-CEP-19` 决策 9 的**延期依据变更** |
| D-7 | 流量模式与可复现性 | `SEQUENTIAL`(基线)+ `RANDOM`(**固定种子并入 config 快照**;因需真实并发写以使 `memory_violations = 0` 有意义);⛔ `TRACE` 本轮不做。⚠️ **漂移登记**:纪要原为「`SEQUENTIAL` + `hotspot`」,最终采用 `RANDOM` ⇒ `hotspot` **移除**;⚠️ **机制缺口**:`TrafficGenTLM` **无** seed / 地址段 setter(`rng_(42)` 硬编码)⇒ 「种子入 config 快照」**今无落点** ⇒ 须新增 setter;⚠️ `uniform_int_distribution` 跨 stdlib 非位精确 ⇒ config 快照须记工具链 |
| D-8 | 判据清单 | 8 条(J-1 装配 · J-2 事务抵达 · J-3 端到端响应完整 · J-4 **IC3 绿 + run-validity 绿**(⛔ **不是**「13 键全部有值」——保留 `not-collected` 合法出口)· J-5 **决定性变异** · J-6 指标自洽 · J-7 桥投影对称 · J-8 B 不承载全绿)。⚠️ **待补**:① run 的 `hypothesis_id` **绑定 `H3`**(唯一双违例类均须实测的已登记行)⇒ 否则 J-4 在机器闸下不可计算;② 三键(`coherence_stalls` / `collective_stalls` / `directory_misses`)的**测量归属与「真实 0」口径**待定 |
| D-9 | 实验产物规范 | `experimental/run-<ts>-cep-e2e-{A\|B}/` + 4 件套;⛔ 产物不入库 ⇒ **入台账的是元数据**(run id · config 快照 sha256 · 13 键结论 · IC3 verdict · 拓扑标记);`preregistration.json` **不动** |
| D-10 | 计数纪律 | 见 ③ |
| **D-0** | **「完成者」角色**(拓扑 A/B 的「全绿」前置) | ✅ **已裁**(2026-11-08,用户)—— 角色 = **home 应答端的协议中立替身**(消费 CohIf `Request`/`Writeback`、经参数化延迟回 `Completion`、可配发 `Snoop`);⛔ **不**产生协议语义声明、⛔ **不**计入 B3b。**拓扑准入判据(治理纪律,逐字)**:「**JSON 拓扑代表真实物理存在的**;…**只为测试而创建**的元素 ⇒ **不进 JSON 拓扑**;否则 ⇒ **要进**」⇒ ⇒ home **物理存在** ⇒ ⇒ **进 JSON 拓扑** ⇒ ⇒ 落 **契约面**(`include/tlm/cep/`,可注册的协议中立 ChStream 模块)⇒ ⇒ **⛔ 不进** `protocol_instances.json` ⇒ ⇒ **装配闸不判红**(闸只要求模块**存在于拓扑**)⇒ ⇒ ⚠️ **命名**待裁(见 ①-b 的命名裁定);桥挂载结构**已裁 = B-1 + 语义降级**(见 ①-c) |

### ①-b ⚠️ 「经网络」的**定义钉死**(metis P0 —— 否则验收会**假绿**)

> **「网络」= ChStream `connections`(可带 `latency`);**桥不在数据路径上** —— 桥是**装配期投影登记器**(无状态投影器:`pack`/`unpack` 只搬共有字段 + `flow_id→flit_category` 有损投影;`src/` 中**零**调用点)⇒ ⇒ **「事务穿过桥抵达对端」是**假绿表述** ⇒ 顶栏的「互联可达 + 桥投影」必须**拆成两条独立判据**:
> ① **互联可达** = 「一次事务经**带 latency 的 ChStream 连接**抵达对端」(**不**经桥);
> ② **桥投影** = **测试级**断言(构造 `CohIfBundle` → `pack` → 断言 `NoCFlitBundle.flit_category` 正确 → `unpack` → 断言共有字段恒等)⇒ ⛔ **不得**写成「事务穿过桥」。

⚠️ **三条实施期约束(自 oracle / metis 复核)**:① **拓扑 A 必须省略 `cep.bridges` 键**(⛔ **空数组 `[]` 是 FAIL `[IAG-01]`**,⛔ 非 SKIP)⇒ ⇒ **J-1 按拓扑分档**(A: 闸 `SKIP` **属正确语义、非缺陷**;B: 闸 `PASS`);② 拓扑 A 的判据 **⛔ 不得**含 `unconsumed_bridges().empty()`(`cohif_noc` 是**全局注册**的,拓扑 A 不消费它 ⇒ 该断言必假);③ **跨 bridge entry 的模块必须互斥** —— `ProtocolEngineBase::bridge_` 是**单槽**且 `attach_protocol_bridge` 直接 `set_bridge()` **赋值** ⇒ 重叠 ⇒ **桥被静默覆盖**而 `instantiateAll` **仍返回 true** ⇒ ⇒ ⛔ **不得**让同一模块出现在两个 entry;⇒ 建议在 Step 7.5 增「同一模块被多个 entry 挂载 ⇒ 失败」的重挂检测(**独立 backlog**,⛔ 不在本轮)。

### ①-c 桥挂载结构裁定(2026-11-08,用户)+ 拓扑 B 的**如实命名**

**裁定 = B-1 + 显式语义降级**:**一个** bridge entry,`modules:[StubCacheEndpointTLM, ChiRnfProtocolEngine, CohIfResponderTLM]` ⇒ ⇒ **零代码、零新机制**;语义降级 = 「**一个 entry 多模块 = 同一 CohIf 投影语境**;端点间链路关系由 `connections[]` 独立表达;⛔ 桥不建模星型 / 两两互联,⛔ 桥不产连通性**」⇒ ⇒ **J-7** 改为「测试级 `pack`/`unpack` 断言 + 逐模块 `bridge()->bridge_id()` 指向**同一投影语境**」(⛔ **不**声称 per-endpoint 归因)。

**拓扑 B 的如实命名**:**「网络」= ChStream `connections` 的延迟属性;桥 = 装配期投影登记器** ⇒ ⇒ ⛔ **不**宣称 flit 级路径(仓内**不存在** Router+flit 路径)⇒ ⇒ 拓扑 B 的名称/描述 SHALL 如实反映这一点。

### ② ⚠️ 实施前的文档纪律(**不得**预先宣称)

- 本段登记的是**架构决策**,⛔ **不是**落地事实 ⇒ ⇒ 凡本段涉及的文档面 **不得**出现「已具备端到端可测能力」「TrafficGen 已接入 CEP」「13 键已全绿」类表述;
- ⇒ 落地状态的更新(§5.1.1 各族行 / §11 状态表)**待实施完成 + verifier 验收后**再另行追加(⛔ 不在本段)。

### ③ 计数纪律(禁令句,逐字)

> 本框架(CEP 端到端可测)**只**提升**可实验性**(真实业务流量下的行为面可测);⛔ **不**构成 `B3b-2` 的解除证据 —— `B3b-2` 的计数依据是**规范条款级** T1–T4(per **SU · 四 D-1**),而本框架的**链路跑通**⛔ **不是**条款级依据 ⇒ ⇒ **`B3b-2` 维持 0/4**;且本框架**不改变** 4 受限族(CXL 2.0 / CXL 3.0 / NVLink / UALink)的任何状态或 deferral 登记(⛔ `test/cep_b3b2_deferrals.json` **零 diff**)。

### ④ 冻结面清单与**两条授权链的分离**

| 冻结面成员 | 本轮改动 | 授权链 |
|---|---|---|
| `docs/adr/ADR-CEP-19-*.md` | 追加 SU(决策 9 延期项状态 + 实施形态 + MODIFIES 预告) | **本轮**:Entry #288 ⇒ 产物 commit → `--update --authorized-by "Entry #288"` → 清单**单独** commit → 无参复验 |
| `docs/architecture/07-*.md`(本文件) | 追加本段 SU(决策登记) | **同上**(同 commit) |
| `openspec/specs/cep-stub-cache-core/spec.md` | **`:56` 单条 MODIFIES**(⛔ `:71` 经实测**撤销** —— 其 WHEN 限定在**命中**路径,与拓扑 B 的 miss/request flit 路径**不冲突**) | ⛔ **不在本轮** ⇒ 属**实施期**动作 ⇒ 须 **builder 在其 change 内**执行 + **另立台账 Entry** 走两步授权(per `ADR-CEP-19` 决策 9 理由 ①)⇒ ⇒ **本段不预授权** |

⚠️ **新增契约面元素(实施期)**:**两个** —— ① 「完成者」模块 `CohIfResponderTLM`(落 `include/tlm/cep/`)+ ② wrapper `StubCacheEndpointTLM`(落 `include/tlm/cep/`);两者的**注册**均在 `include/chstream_register.hh`(⛔ 该文件**不在** M1 冻结集 ⇒ **零冻结面增量**)⇒ ⇒ **两者均不在 M1 冻结集** ⇒ ⇒ **零冻结面增量** ⇒ ⇒ 但其**契约面**(delta spec)须随 change 落盘。

⚠️ **拓扑准入判据(治理纪律,逐字,2026-11-08 用户裁定)**:

> 「**JSON 拓扑代表真实物理存在的**;如果响应者元素是**物理上不存在**、只是为了测试而创建 ⇒ 那就**不进 JSON 拓扑**;否则 ⇒ **就要进 JSON 拓扑**。」

⇒ ⇒ 适用范围:凡拟新增的拓扑元素(模块 / 桥 / 端点)⇒ **「仅为测试而创建」者** ⇒ **不进 JSON 拓扑**(⛔ 不得以「夹具直调」冒充端到端)⇒ ⇒ **「代表真实存在者」** ⇒ **必须进**。

### ⑤ Verification(可勾选验收判据 · 前瞻)

- [ ] 本段 SU 已追加,且 §2 / §3.1 / §5.1.1 / §11 与 SU · 一 ~ 十 **逐字未变**
- [ ] 10 条决策各有**可执行文本**且**互相不矛盾**;逐条依据可追至讨论纪要
- [ ] ② 的**实施前不得宣称**纪律已登记;本段**无**任何落地宣称
- [ ] ③ 的计数纪律禁令句已逐字登记;`test/cep_b3b2_deferrals.json` **零 diff**
- [ ] ④ 的**两条授权链分离**已登记(spec MODIFIES **不在**本轮)
- [ ] 冻结面两步授权已执行:台账 **Entry #288** 授权 → **commit 本段 + `ADR-CEP-19` 对应段** → `cep_freeze_check.sh --update --authorized-by "Entry #288"` → **清单单独 commit** → 无参复验 provenance 通过

**关联**:台账 **Entry #288**(10 条决策 + 授权)· **#287**(下一步任务推荐)· **#285**(架构文档审查)· **#282**(4 条 architect 裁决)· **#280**/**#269**(`07 号` SU · 七 / `ADR-CEP-19` 决策 9)· `.rddf/reviews/cep-ualink-e2e-framework-discussion.md`(讨论纪要 · 10 条逐条依据)· `openspec/specs/cep-stub-cache-core/spec.md`(`:56` / `:63` / `:71`)

---

## Status Update(2026-11-08 · 十二,per 台账 Entry #292)—— **4 项决策面勘误与补充**(D-0 定位改写 / `:71` 撤销理由 / 状态追认 / 拓扑分档判据 **J-9**)

> **正文吸收**:本则的 **J-9 拓扑分档判据**已提炼进 **§6.3.1**(per 批 2;⛔ 本则原文不回改)。

> **性质**:末尾追加 SU(append-only)。⛔ 不改 §2 / §3.1 / §5.1.1 / §11 与 SU · 一 ~ 十一 的任何一段(⛔ **SU · 十一** 的 D-0 / D-5 / D-6 / D-8 等行**逐字保留**,⛔ 其措辞**由本段勘误取代**,原文不回改);⛔ **零**代码 / 契约 / 冻结 main spec 改动;⛔ 不改任何 DoD 结论。
> **触发**:台账 **Entry #291**(两条用户裁决 · F-05 home 简化模型 / F-11 拓扑 B 独立)+ 第二轮审查 `.rddf/reviews/cep-arch-planner-review-findings-2026-11-08.md`(**F-01 / F-04 / F-05 / F-11**)。
> **审查**:`oracle`(4 项勘误的**理由是否站得住** + 新增边界是否引入越界宣称)→ `critic`(本段 + `ADR-CEP-19` SU 2026-11-08 · 二 的架构审稿),顺序不可颠倒。
> **关联**:`ADR-CEP-19` SU 2026-11-08 · 二 · `ADR-CEP-12` SU 2026-11-08 · 台账 **Entry #291 / #292** · `openspec/specs/cep-stub-cache-core/spec.md`(`:56` / `:63` / `:71`)· `include/tlm/cep/protocol_instance.schema.json`

### ① 勘误与补充总表(4 条)

| # | 勘误 / 补充 | 与 **SU · 十一** 的关系 | 落点(决策面) |
|---|---|---|---|
| **F-01** | **状态追认**:SU · 十一 ① / ①-c 与 `ADR-CEP-19` Entry #288 SU §④-a 的两处「**待裁**」= **已过时** ⇒ 现行状态 = **D-0 已裁** + **桥挂载结构 = B-1 + 显式语义降级** | ⛔ SU · 十一 的**裁决内容逐字有效**;本段只**追认状态** + **精确化「零代码」** | `ADR-CEP-19` SU · 二 ① |
| **F-04** | **`:71` 撤销理由勘误**:真实理由 = 「**命中路径限定 + 命中路径下 stub 行为与 net_side 驱动者无关**」;⛔ 原措辞中「**拓扑 B 的 flit 路径**」这一**命名**作废(SU · 十一 ①-c 已钉死仓内无 Router+flit 路径)⇒ ⇒ **结论不变**、且 `:71` **今日有牙**(`INV-SC6` 计数断言) | SU · 十一 ①-b / ①-c 的「无 flit 路径」结论**不变**;被勘误的是 `:71` 那行的**措辞依据** | `ADR-CEP-19` SU · 二 ② |
| **F-05** | **「完成者」定位改写** = **真实 home 的简化模型**(承载**最小协议语义**)⇒ ⛔「协议中立」措辞作废;⇒ 进 JSON / 不进 `protocol_instances.json` / 装配闸不判红 / 不计入 B3b **四项结论不变** | SU · 十一 ① 的 D-0 行**结论不变**,**限定词**被取代 | `ADR-CEP-19` SU · 二 ③ · 本段 ② |
| **F-11** | **拓扑 B 独立保留** + 「**A 必须零延迟**」的判据级理由 + **J-9 拓扑分档判据**(机器判)+ 两条实施契约 | SU · 十一 ① 的 D-5「两拓扑并存」**维持**;本段补**理由**与**判据** | `ADR-CEP-19` SU · 二 ④ · 本段 ③ |

### ② D-0 行**逐字更正**(⛔ SU · 十一 ① 的 D-0 行原文保留)

| 项 | SU · 十一 ① D-0 行原文(⛔ 不回改) | **本段更正后** |
|---|---|---|
| 角色 | 「home 应答端的**协议中立替身**(消费 CohIf `Request`/`Writeback`、经参数化延迟回 `Completion`、可配发 `Snoop`)」 | **真实 home 的简化模型**(承载**最小协议语义**)⇒ ⛔「协议中立」限定词**作废**(该模块是**协议相关实现**;协议中立即在**平台层**成立) |
| 承载面 | 「⛔ **不**产生协议语义声明」 | **承载**第一族(**CHI RN-F**)的**最小应答语义** = ① 消费 `Request` ⇒ 生成 `Completion`;② 按读请求**可配发** `Snoop`;③ 消费 `Writeback` ⇒ **接收即消费、零响应**(fire-and-forget);⛔ **不**承载家族级 / 条款级语义(**credit · retry · ordering point · 多拍聚合 · Perm 规则 · 容量与队列模型**) |
| 计数 | 「⛔ **不**计入 B3b」 | **维持**;⚠️ 追加**防误读句**(见 ⑤)⇒ 承载最小应答语义 ⛔ **不**构成任何家族的 **T1–T4 条款级**证据 |
| 契约面归类 | 「落 **契约面**(`include/tlm/cep/`,可注册的**协议中立** ChStream 模块)」 | 落**契约面**(`include/tlm/cep/`,可注册的 ChStream 模块)⇒ 归入**与 adapter / engine / bridge 并列的独立类目:home 模型** ⇒ ⛔ 不是第二个 engine、⛔ 不是 adapter、⛔ 不是 protocol instance |
| 命名 | `CohIfResponderTLM` | **维持不变**(per ①-c 的 B-1 模块清单 + 台账 Entry #291 裁决 A);⛔ SU · 十一 ① 的 D-0 行原文写「⚠️ **命名待裁**」—— 该措辞**由本段追认取代** ⇒ ⚠️ **不得**据类名中的 `CohIf` 推断行为面为协议中立 |
| 拓扑准入 | 「home **物理存在** ⇒ ⇒ **进 JSON 拓扑**」 | **维持**(且推导链更严):元素 = **真实 home 的简化模型** ⇒ 代表真实物理元素 ⇒ ⇒(**用户裁决**,per 台账 Entry #291 ①)按用户自裁的**拓扑准入判据**(逐字)⇒ ⇒ **进 JSON 拓扑** ⇒ ⇒ **不进** `protocol_instances.json` ⇒ ⇒ **装配闸不判红**(闸只要求 `bridges[].modules` 的成员**存在于拓扑**)—— **四项结论逐字不变**;⚠️ 「角色真实性 ⇒ 实例真实性」的滑移是**被裁决消解**的,⛔ 不是逻辑蕴含 ⇒ 不得当作通用规则复用 |

### ③ 拓扑分档:「**A 必须零延迟**」的理由 + **J-9 拓扑分档判据**(机器判)

- **理由(B-1)—— ⛔ 因果序不可颠倒**(防后人只引半句):
  - **主理由 = 判别力**:若 A 也带 `latency` ⇒ B 的「**互联可达**」面**退化为 A 的子集** ⇒ ⇒ B 的独立价值收窄到只剩「桥投影装配面」⇒ ⇒ 「两拓扑并存」失去**主要**依据 ⇒ ⇒ 故 **A 的 `connections` 一律 `latency = 0`**(⚠️ 「桥投影」面因 A 按 J-9 定义**永不带** bridge entry ⇒ 不会因 A 带 latency 而退化 ⇒ ⛔ 不得把该面算进退化项);
  - **次理由 = latency 断言纯度**:**A 是全绿载体** ⇒ 其 latency 断言必须**只**反映**模块 / 协议处理延迟** ⇒ ⛔ **不**混入**连接延迟**;
  - ⇒ ⇒ 两拓扑**不重叠**(B 专门承载「**带连接延迟时事务仍抵达对端**」这一面)。
- **J-9 拓扑分档判据(机器判)**:

  | 拓扑 | 判据 |
  |---|---|
  | **A** | 装配闸 `SKIP`(⇒ `cep.bridges` 键**缺省**)∧ `cep.bridges` **不存在** ∧ 所有 `connections` **无 `latency > 0`** |
  | **B** | 装配闸 `PASS` ∧ `cep.bridges` **恰好 1 条** ∧ 该条 `modules` **恰好 3** ∧ **至少 1 条** `connections` 带 `latency > 0` ∧ 三个模块的**桥指针**指向**同一 `bridge_id`** |

  - ⚠️ **两条实施契约(缺一 ⇒ J-9 落空)**:① B 的 bridge entry **必须**带 `protocol_instance` 且命名一个**已声明**实例(实测 `[IAG-02]`:缺失或未声明 ⇒ 判红)⇒ config 作者若不知此约束 ⇒ B **永远 FAIL**;② `ChStreamModuleBase` **无 `bridge()` 访问器**(实测仅有 `attach_protocol_bridge` 虚函数;`bridge()` 只在 `ProtocolEngineBase`)⇒ 「逐模块桥指针指向同一 `bridge_id`」预设了**当前不存在**的能力 ⇒ 契约须写明:**wrapper / home 模型自行存储桥指针并暴露只读访问器**(⛔ **不动基类**,per D-1「零基类改动」)。
- ⚠️ **与既有判据的关系**:J-9 是 **J-1 的分档细化**(J-1 = 装配;J-9 = 装配**按拓扑分档**的可判化)⇒ ⛔ **不**改动 J-1 ~ J-8 的编号与含义。
- ⚠️ **B 仍不承载全绿**(沿用 SU · 十一 ① 的 D-5 边界)⇒ ⛔ 不得把 B 的连通性读成「13 键全绿」。

### ④ 声明面落点二选一(**待 architect 裁**;⛔ 本段不预裁)

| 路线 | 内容 | 冻结面代价 |
|---|---|---|
| **甲(建议)** | **不进** `protocol_instances.json`(它**不是**协议实例)+ 契约面 delta 登记**明示豁免**(「声明 ↔ 实现对齐」的例外登记) | **零** —— `include/tlm/cep/protocol_instances.json` **不在** M1 冻结集;`protocol_instance.schema.json` 不动 |
| **乙** | 给实例加**第 14 键**(如 `home_model`) | ⚠️ **触及 M1 冻结件** `include/tlm/cep/protocol_instance.schema.json`(实测 `additionalProperties: false` ⇒ 加键即违 schema)⇒ 须**另立台账 Entry + 两步授权** |

⇒ ⇒ 裁定的**归属** = architect;实施 = builder。

### ⑤ 防误读句(**逐字**;须同时进契约面 delta 与台账)

> home 简化模型承载**最小应答语义** ⇒ ⛔ **不**构成任何家族的 **T1–T4 条款级**证据(计数依据 = 规范条款级,per SU · 四 D-1)⇒ ⇒ **`B3b-2` 维持 0/4**;⛔ **不得**以「CEP 有了 home 模型」推出协议语义达成;⛔ 本框架**不改变** 4 受限族(CXL 2.0 / CXL 3.0 / NVLink / UALink)的任何状态或 deferral 登记(⛔ `test/cep_b3b2_deferrals.json` **零 diff**)。

### ⑥ Verification(可勾选验收判据 · 前瞻)

- [ ] 本段 SU 已追加,且 §2 / §3.1 / §5.1.1 / §11 与 SU · 一 ~ 十一 **逐字未变**
- [ ] ① 的 4 条勘误 / 补充已逐条登记,且**与 SU · 十一 的关系**已写明(⛔ 未回改任何原段)
- [ ] ② 的 D-0 更正对照表齐备;「进 JSON / 不进 `protocol_instances.json` / 装配闸不判红 / 不计入 B3b」四项结论**逐字不变**;「协议中立」限定词的作废 + `Writeback` 行为面 + 命名锚点 + 「用户裁决」标注均已登记
- [ ] ③ 的「A 零延迟」理由(**判别力为主 / 断言纯度为次**)+ **J-9** 判据 + 两条实施契约(`[IAG-02]` 的 `protocol_instance` / 只读桥指针访问器)已登记;J-1 ~ J-8 编号与含义**未变**;B 不承载全绿的边界保留
- [ ] ④ 的声明面二选一已登记为**待裁**,⛔ **未预裁**
- [ ] ⑤ 的防误读句已**逐字**登记
- [ ] ⛔ 本段**无**任何落地宣称;`test/cep_b3b2_deferrals.json` **零 diff**;**`B3b-2` 维持 0/4**
- [ ] 冻结面两步授权已执行:台账 **Entry #292** 授权 → **commit 本段 + `ADR-CEP-19` SU · 二 + `ADR-CEP-12` SU** → `cep_freeze_check.sh --update --authorized-by "Entry #292"` → **清单单独 commit** → 无参复验 provenance 通过

**关联**:台账 **Entry #292**(授权)· **#291**(两条用户裁决 + 落地义务清单)· **#288**(12 条决策 + 授权)· **#285**(架构文档审查)· `.rddf/reviews/cep-arch-planner-review-findings-2026-11-08.md`(F-01 / F-04 / F-05 / F-11)· `ADR-CEP-19` SU 2026-11-08 · 二 · `ADR-CEP-12` SU 2026-11-08 · `openspec/specs/cep-stub-cache-core/spec.md`(`:56` / `:63` / `:71`)· `test/test_cep_stub_cache_core.cc`(`INV-SC6` 计数断言)· `include/tlm/cep/protocol_instance.schema.json`


---

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

## §末 文档索引(2026-11-08 移置文末,per 台账 Entry #295)

> **用途**:本文件已 **23 个一级节 / 9 则 Status Update**(全部列于下表)⇒ 故设本索引(以**零影响**既有 `path:line` 锚点 —— ArchForge 对齐清单按行号锚定)。

> ⚠️ **新纪律(per 台账 Entry #295,逐字)**:本索引**永久置于文末**;此后**新增段落 SHALL 插在本索引之前** ⇒ **新增内容永不使既有 `path:line` 锚点位移**;⚠️ **最新 SU 在本索引之前**(⛔ 勿在本文末找最新段)。

> ⚠️ **本索引由脚本生成(零手抄)⇒ 每次追加后重生成**;**生成器不在仓内**(登记为 backlog:落 `scripts/`)。

> ⚠️ **2026-11-10 批 1 后**:本索引已按新文件重建(索引 A **50 → 23** 行 · 索引 B **21 → 9** 行)⇒ 与本文件现存的一级节 / SU **逐条对应**;自述数 **23 / 9** == 索引 A / 索引 B 行数(`cep_index_position_check.sh` 判据)。

### 索引 A · 一级节(23)

| # | 章节 | 行 |
|---|---|---|
| 1 | 1. 文档定位与真源关系 | 27 |
| 2 | 2. 6 层 CEP 架构 | 108 |
| 3 | 3. canonical 10 条 Coherence Interface 流 | 165 |
| 4 | 4. 67 编号位 / 66 条守门 invariant 总表(单一真源) | 242 |
| 5 | 5. Bundle 与协议适配 | 304 |
| 6 | 6. 仿真语义与集成 seam | 443 |
| 7 | 7. 实验输出协议 | 487 |
| 8 | 8. 演进路线与阶段边界 | 542 |
| 9 | 9. Primary ADRs | 620 |
| 10 | 10. References | 648 |
| 11 | 11. Gap → 实现状态映射(**已迁出,仅存指针**) | 669 |
| 12 | 12. CEP 使用手册(**已迁出 → `docs/guide/cep-usage.md`**,仅存指针) | 1556 |
| 13 | 附录 · Status Update 2026-10-30(第十六则 · per 台账 Entry #106 —— **S1-3 触发后的真实义务**结论) | 686 |
| 14 | Status Update(2026-11-01,per 台账 Entry #131)—— §5.1.1 **CXL 行**的措辞对齐(裁决 ③⑦)+ `cxl_2_0` 实例登记 | 753 |
| 15 | Status Update(2026-11-02,per 台账 Entry #154)—— §5.1.1 / §5.4 / §11 的 **5 处口径订正** + **对称模式的 adapter 形态裁决** | 800 |
| 16 | Status Update 2026-11-04 —— Stage 3 收口的 **5 项架构级裁决**(B3 口径拆分 · 3 项声明假设处置 · `FullMapDirectory` 触发条件 · 跨仓锚点治理不变量 · MOESI-GPU 形态登记) | 879 |
| 17 | Status Update 2026-11-04(#2)—— 实验输出契约的**可执行化**(C1)+ ADR 一决策一文档化的**残留登记与触发条件**(C2) | 967 |
| 18 | Status Update(2026-11-06,per 台账 Entry #238)—— 「转移表真源」的**判定式(T1–T4)** + **TileLink-C 阻塞解除** + 逐族重判 | 1102 |
| 19 | Status Update(2026-11-06 · 四,per 台账 Entry #249)—— 「转移表真源」的**来源效力**与 **B3b 计入口径** | 1198 |
| 20 | Status Update(2026-11-06 · 五,per 台账 Entry #257)—— D-1「来源效力」的**机器可判落地回填**(命题范围收窄 + 载体覆盖账 + D-4 ③/④ 回填) | 1291 |
| 21 | Status Update(2026-11-08 · 十一,per 台账 Entry #288)—— **「Core·TrafficGen 主干」的架构决策登记**(10 条统一)+ 实施前**不得宣称已落地** | 1399 |
| 22 | Status Update(2026-11-08 · 十二,per 台账 Entry #292)—— **4 项决策面勘误与补充**(D-0 定位改写 / `:71` 撤销理由 / 状态追认 / 拓扑分档判据 **J-9**) | 1473 |
| 23 | §末 文档索引 | None |

### 索引 B · Status Update(9 · 追加层 2026-11-01 起)

| # | 日期 | 主题 | 台账 | 行 |
|---|---|---|---|---|
| 1 | 2026-11-01 | Status Update(2026-11-01,per 台账 Entry #131)—— §5.1.1 **CXL 行**的措辞对齐(裁决 ③⑦)+ `cxl_2_0` 实例登记 | Entry #131 | 753 |
| 2 | 2026-11-02 | Status Update(2026-11-02,per 台账 Entry #154)—— §5.1.1 / §5.4 / §11 的 **5 处口径订正** + **对称模式的 adapter 形态裁决** | Entry #154 | 800 |
| 3 | 2026-11-04 | Status Update 2026-11-04 —— Stage 3 收口的 **5 项架构级裁决**(B3 口径拆分 · 3 项声明假设处置 · `FullMapDirectory` 触发条件 · 跨仓锚点治理不变量 · MOESI-GPU 形态登记) | — | 879 |
| 4 | 2026-11-04 | Status Update 2026-11-04(#2)—— 实验输出契约的**可执行化**(C1)+ ADR 一决策一文档化的**残留登记与触发条件**(C2) | — | 967 |
| 5 | 2026-11-06 | Status Update(2026-11-06,per 台账 Entry #238)—— 「转移表真源」的**判定式(T1–T4)** + **TileLink-C 阻塞解除** + 逐族重判 | Entry #238 | 1102 |
| 6 | 2026-11-06 | Status Update(2026-11-06 · 四,per 台账 Entry #249)—— 「转移表真源」的**来源效力**与 **B3b 计入口径** | Entry #249 | 1198 |
| 7 | 2026-11-06 | Status Update(2026-11-06 · 五,per 台账 Entry #257)—— D-1「来源效力」的**机器可判落地回填**(命题范围收窄 + 载体覆盖账 + D-4 ③/④ 回填) | Entry #257 | 1291 |
| 8 | 2026-11-08 | Status Update(2026-11-08 · 十一,per 台账 Entry #288)—— **「Core·TrafficGen 主干」的架构决策登记**(10 条统一)+ 实施前**不得宣称已落地** | Entry #288 | 1399 |
| 9 | 2026-11-08 | Status Update(2026-11-08 · 十二,per 台账 Entry #292)—— **4 项决策面勘误与补充**(D-0 定位改写 / `:71` 撤销理由 / 状态追认 / 拓扑分档判据 **J-9**) | Entry #292 | 1473 |

### 索引 C · 关键表指针(高频查阅)

| 表 | 位置 | 用途 |
|---|---|---|
| §5.1.1 工业硬件协议族 | 行 312 | 8 实例 / 6 族(**状态列已迁出** → `obj §4.0-a`) |
| §3.1 canonical 10 流 | 行 167 | 一致性流的唯一清单 |
| §6.4 Null 实现 | 行 480 | 解耦测试兜底 |
| ~~§11 落地状态表~~ | **已迁出** | → `objective-cep-platform §4.0` / `§4.0-a` |
| ~~§12 CEP 使用手册~~ | **已迁出** | → [`docs/guide/cep-usage.md`](../guide/cep-usage.md) |
| §5.1.5 「转移表真源」判定式(T1–T4) | 行 404 | **补运行时引擎的触发条件**的唯一判据(四要件合取) |
| §6.3.1 「经网络」定义 + J-9 拓扑分档 | 行 534 | 拓扑 A/B 的**可机判判据**;⛔ 防「事务穿过桥」假绿 |
| §7.2.2 「全绿」两级判定 | 行 624 | run-validity / hypothesis-verdict 分离 |
| §8.5 文档层治理不变量 | 行 731 | 跨仓锚点两类处置 + ADR 编辑禁令 + 决策块计数口径 |
| **子文档:第 1 层** | [`cep/stub-cache-core.md`](cep/stub-cache-core.md) | §3.2 / §3.3 实体 |
| **子文档:第 2 层** | [`cep/coherence-interface.md`](cep/coherence-interface.md) | §3.1 / §5.4 实体 |
| **子文档:第 3 层** | [`cep/protocol-engine.md`](cep/protocol-engine.md) | §5.1(含 5.1.1~5.1.5)实体 |
| **子文档:第 4 层** | [`cep/directory-ordering.md`](cep/directory-ordering.md) | §5.2 实体 |
| **子文档:第 5 层** | [`cep/collective-plane.md`](cep/collective-plane.md) | §5.3 实体 |
| **子文档:第 6 层(传输层)** | [`cep/fabric-transport.md`](cep/fabric-transport.md) | §6.3 / §6.3.1 实体 |
| **子文档:仿真与实验** | [`cep/simulation-and-experiment.md`](cep/simulation-and-experiment.md) | §6.1/6.2/6.4 + §7 实体 |
| **子文档:Core/TrafficGen** | [`cep/core-traffic-gen.md`](cep/core-traffic-gen.md) | 骨架(A 类待批 4) |
| `obj-cep-platform.md` §6.2 | 台账 | 终局 DoD 12 条 |
