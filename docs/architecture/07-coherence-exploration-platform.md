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
| 目录 / 探测结构多样性 | 不同协议各自实现不同 `.sm` 组件(`-dir.sm` / `-probeFilter.sm` / token 机制),**非**同一 `.slicc` 内变体 | SharerTracker 5 实现 + caps descriptor(§5.2)|
| 探索产出 | 协议 / 拓扑 / 目录对比实验 | 同左(per `ADR-CEP-06` 4 件套)|

> **关键含义**:gem5 从未把"协议一致性探索"做成 gem5 之外的项目 — **它是 gem5 框架能力**。CEP 同理:**是 CppTLM 框架能力**,不是 dGPU 业务扩展,不是独立项目。

**能力覆盖类比(2026-10-20 调研补充,per `ADR-CEP-17`)**:既然 CEP 被定性为"框架级一致性探索能力",它**应该**能覆盖 gem5 Ruby 能探索的协议空间。调研结论:

| 维度 | gem5 Ruby | CEP 现状(2026-10-20) |
|------|-----------|---------------------|
| 协议实例数 | 8 个独立 `.slicc` | §5.1.2 已纳入 8 个(Ruby-SLICC 族),6 直接支持 + 1 需扩展 + 1 排除 |
| 消息模型 | RubyRequest / RubyResponse / RubySnoopResponse | canonical 10 流 **100% 覆盖,零新增流** |
| 目录机制 | directory / token / probeFilter 三形态 | SharerTracker 5 实现 + caps descriptor(token 归属见 §5.1.3)|
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

### 3.1 清单(per `ADR-CEP-02 §2`,首次定义)

| # | 流名 | 方向 | 完成纪律 | 语义 |
|---|------|------|---------|------|
| 1 | `request` | node → center | requester-scoped | 缓存行请求(读 / 唯一化 / 写回 / 驱逐) |
| 2 | `completion` | center → node | requester-scoped | 请求完成 + data(若 inline)+ perm + error |
| 3 | `snoop` | center → node | requester-scoped | snoop 查询 / 失效 / clean / forward |
| 4 | `snoop_resp` | node → center | requester-scoped | snoop 响应:hit + state + dirty + data + txn_id |
| 5 | `writeback` | node → center | requester-scoped | 驱逐/写回:addr + data + dirty |
| 6 | `forward_data` | node ↔ node(经中心调度)| requester-scoped | 数据转发:src_addr + dst_addr + data + perm |
| 7 | `back_invalidate` | center → node | requester-scoped | 反向失效:addr + type(invalidate/clean)+ txn_id(CXL 3.0 BISnp 走这里,host→device)|
| 8 | `address_resolve` | node ↔ center | requester-scoped | 地址归属解析:pa + node_id → home_node + coherent_flag |
| 9 | `barrier` | center → node ↔ center(双向)| requester-scoped | 单请求者排空/可见性点;**不携带 group_id / participant_count / epoch_id** |
| 10 | `snoop_completion_ack` | node → center | requester-scoped | snoop 完成后 ack;配对 `snoop_resp` 而非 `completion` |

**不变量**(per `ADR-CEP-02 §2` CEN-Ib1~10):

| ID | 规则 |
|----|------|
| CEN-Ib1 | `request` 必有配对 `completion` |
| CEN-Ib2 | `completion` 不携带 unsolicited 数据 |
| CEN-Ib3 | `snoop` 必有 `snoop_resp` |
| CEN-Ib4 | `snoop_resp` 必须含 `txn_id` 用于 ack 配对 |
| CEN-Ib5 | `writeback` 不与 `request` 竞态 |
| CEN-Ib6 | `forward_data` 对 stub 透明(不可分辨性) |
| CEN-Ib7 | `back_invalidate` 是 requester-scoped,**不是** group rendezvous |
| CEN-Ib8 | `address_resolve` 在 ProtocolEngine 内终结 |
| CEN-Ib9 | `barrier` 流不携带 group_id / participant_count / epoch_id |
| CEN-Ib10 | `snoop_completion_ack` 配对 `snoop_resp` 不配对 `completion` |
| **CEN-Ib11** | **atomic 意图作为 `request` 流的独立属性承载;`perm` 枚举维持 4 值锁,不新增 "ATOMIC" perm 值**(per `ADR-CEP-02`,一手:ProtoGen p.7 局限 L2)|

### 3.2 Stub Cache Core 的 RN 侧投影(per `ADR-CEP-05 §2`)

**Stub 只感知 5 条流**(net-side),另 5 条由 ProtocolEngine 内部消化:

| Stub 接口 | 对应 canonical 流 | 折叠说明 |
|-----------|----------------|---------|
| `request(addr, perm)` | 流 1 | — |
| `completion(txn_id, data, perm, error)` | 流 2 | — |
| `snoop(addr, type, txn_id)` | 流 3 | — |
| `snoop_resp(addr, hit, state, dirty, data, txn_id)` | 流 4 | — |
| `writeback(addr, data, dirty)` | 流 5 | — |
| `evict_line(addr)` | **core-side 原语** | 非 CohIf 流投影;net-side 表现为流 5 + 流 2 |

**5 隐藏流 → stub 原语折叠映射表**(α 方案正确性的唯一审计点):

| 隐藏流 | stub 表现 | 关键不变量 |
|--------|----------|-----------|
| 流 6 `forward_data` | 不可显式为接口;stub 收到流 2 `completion` 时数据可用即视为"forward 或 memory 完成" | **CEN-Ib6 不可分辨性** |
| 流 7 `back_invalidate` | 表现为普通 invalidate snoop | stub 无协议特异行为 |
| 流 9 `barrier` | 对 stub 不可见 | barrier 穿插时 stub 的 MSHR 占用、snoop 响应、写回顺序均不变 |
| 流 8 `address_resolve` | 在 ProtocolEngine 内终结 | 不泄漏到 stub 任何接口 |
| 流 10 `snoop_completion_ack` | 由 ProtocolEngine 内部消化 | stub 不知道 snoop ack 单独流存在 |

**Stub Cache 7 条不变量**(per `ADR-CEP-05 §4.3`):

| ID | 规则 |
|----|------|
| INV-SC1 | 同一 addr 在 stub 内部任何时刻最多 1 个 outstanding request |
| INV-SC2 | evict 不允许选取 outstanding line |
| INV-SC3 | snoop 响应必须在固定延迟内返回(参数化)|
| INV-SC4 | completion 与 request 严格配对(无 unsolicited completion);`txn_id` 沿用 `ADR-X.1-transaction-id.md` 命名 |
| INV-SC5 | dirty writeback 必须带 data;clean writeback 不带 |
| INV-SC6 | hit 路径不产生网络流量(cpu_side 直接返回)|
| INV-CEP-05-IC1 | 只暴露**稳定状态**;瞬态态 + 协议私有态(token 等)住 Protocol Engine 插件内部(per `ADR-CEP-05 §4.3`)|

### 3.3 缓存核心协议无关性

- **缓存行粒度**:64B,地址对齐
- **权限状态**:`NONE / I / S / E / M`(MESI 抽象),**不暴露** CHI 的 UC/SC/UD/SD 七态
- **内部资源**:tag/data SRAM、MSHR、写回缓冲、替换策略、预取器
- **禁止**:缓存核心直接处理 REQ/SNP/DAT 通道,或直接处理 TxnID / HomeNID / CompAck

---

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
- IC3:5 个条目生命周期事件名 + 时序契约在 5 种实现一致
- IC5:`fabric_ingress_classify` 中 "table 命中 + epoch invalid" 窗口 = 正常服务放行(per R4)
- IC6:5 种实现 hazard 行为契约 `Hazard_NONE` / `Hazard_STALL` / `Hazard_CHAIN` 字面值收敛
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

### 5.1 协议族 → 协议实例 → adapter 映射

> **表述纪律**(per `ADR-CEP-17 §3.2`):"协议族" = **协议实例的来源集合**;"协议实例" = 一个具体可实现协议(1 个 `.slicc` / 1 个 adapter)。**adapter 数量 = 协议实例数**,不是族数。
>
> **当前口径**:**2 个来源族(工业 6 族 / Ruby 1 族)= 16 个 adapter 实例** + §5.1.4 的 **5 个一致性模型语义索引**(交叉视图,不重复计 adapter)。

#### 5.1.1 工业硬件协议族(6 族 / 8 实例)

| 协议族 | 协议实例 | adapter | 状态 | 通道模型 | 关键差异 |
|--------|---------|---------|------|----------|---------|
| **CHI** | CHI RN-F | `ChiRnfAdapter` | Stage 1 subset → Stage 2 full | RN-F 6 通道(TXREQ/RXRSP/RXDAT/RXSNP/TXRSP/TXDAT)| RN-F 只发 REQ 只收 SNP;Credit 流控;TxnID 管理;CompAck |
| **CXL 2.0 非对称** | CXL 2.0 | `CxlCacheAdapter` | Stage 1 | D2H/H2D 各 3 通道(Req/Rsp/Data)| Host 侧 Snoop Filter;设备不感知其他设备 |
| **CXL 3.0 对称** | CXL 3.0 对称 | `CxlCacheAdapter`(对称模式)| Stage 2 | + BISnp/BIRsp 反向通道 | DCOH(Device Cache Ownership History);设备可主动反向 snoop;基于 DPA 而非 PA 归属 |
| **TileLink-C** | TileLink-C | `TileLinkCAdapter` | Stage 1 | A/B/C/D/E 通道 | Tree Tip 权限模型;Probe/Acquire/Grant 家族,无传统 snoop 广播;lookup 可能 ESCALATE_UP |
| **NVLink** | NVLink | `NvlinkAdapter` | Stage 2 | NVLink TL Read/Write/Atomic/Probe | NVSwitch 内嵌 SHARP/atomic/barrier;排序点可落在 Switch/SPU |
| **UALink** | UALink | `UalinkAdapter` | Stage 3 | Station/Port/Lane;TL Load/Store/Atomic | In-Network Compute(ReadReduce/WriteMulticast/Broadcast/AllReduce) |
| **MOESI/GPU** | `moesi_gpu` | `MoesiGpuAdapter` | **✅ 已落地**(队列 #6,2026-10-27 归档;首个 `status=implemented` 实例) | 6 态 × 6 事件;snoop 型(SoC 内) | ArchForge 贡献(per `32 号` §6.1);**E→M 静默升级**是 `ADR-CEP-13` 域可见写判据 (a)(b) 的**争点所在**(**该争点已裁定 → 状态见 `ADR-CEP-05 §13.1`**) |
| **MESIF** | `mesif` | `MesifAdapter` | **📋 已排期**(队列 #9;季度见 `obj §4.0`;⚠ R-1 阻塞) | 分级目录型(跨卡);目录查询 / 定向 snoop | ArchForge 贡献(per `32 号` §6.2);与 `moesi_gpu` **互不承认**(per `32 号` C1) |

> **协议数 6 族 / 8 实例**(CXL 2.0 与 3.0 对称是同一 adapter 的两种模式,但实现阶段是 2 个 change)
>
> **族计数的口径**(2026-10-22 补记,per 台账 **Entry #12**):`moesi_gpu` / `mesif` 计为**同一来源族**
> (ArchForge 声明族,含 2 套互不承认的一致性架构,per `32 号` C1)→ **+1 族 / +2 实例**,
> 镜像 CXL「1 族 / 2 实例」的先例。两者 `family` 字段均为 `industrial`(per `32 号` 声明)。
> **状态 = 按队列实况(2026-10-27 更新,per 台账 Entry #82 授权)** —— `moesi_gpu` = 队列 #6 **已落地归档**(2026-10-27;首个 `status=implemented` 实例);`mesif` = 队列 #9 **已排期**(季度见 `obj §4.0`;⚠ R-1 阻塞登记 —— **排期 ≠ 就绪**)。**状态值以 `obj §4.0` 排队总表为唯一权威**(本表**不复制**排期,per 防双真源立场)。
>
> **2026-10-22 补记**(per 台账 **Entry #13** [adr-amendment]):`moesi_gpu` 行的「**争点所在**」表述
> **已失效** —— ArchForge 已裁定判据 (b) **成立**(`4ff2fa6`,Q-3a = 是)→ 该静默 `E→M` **是域可见写**。
> **状态值以 `ADR-CEP-05 §13.1` 为唯一权威**(本表**不复制**状态值,per 防双真源立场);
> 本行的「已落地 / 已排期」状态**不受影响**(排期 ≠ 就绪)。

#### 5.1.2 Ruby-SLICC 协议族(1 族 / 8 实例,per `ADR-CEP-17 §2 Decision 2`)

> **来源**: gem5 Ruby `src/mem/ruby/protocol/*.slicc`(per `docs/research/gem5-coherence-protocols-survey.md:16-23` 实证)
> **定位**: **学术 / 教学协议族**,与 §5.1.1 的工业硬件协议族**互补而非重叠**

| 协议实例 | 判定 | Sharer Tracker 选型 | 关键说明 |
|---------|------|-------------------|---------|
| MESI_Two_Level | ✅ 直接支持 | `FullMapDirectory` | 标准 2 级 directory-based |
| MESI_Three_Level | ✅ 直接支持 | `FullMapDirectory` | 3 级;多层状态机在插件内 |
| MOESI_CMP_directory | ✅ 直接支持 | `FullMapDirectory` | `dir.sm` = 中心目录 = `FullMapDirectory` |
| MOESI_hammer | ✅ 直接支持 | `SparseSnoopFilter` | `probeFilter` 保守过滤 = caps `CONSERVATIVE` 语义 ✅ |
| MOESI_AMD_Base | ✅ 直接支持 | `SparseSnoopFilter` | 含 `probeFilter` / `Region*` / `CorePair` |
| GPU_VIPER | ✅ 直接支持 | 复用 MOESI_AMD_Base | GPU 侧 coherence 在插件内 |
| **MOESI_CMP_token** | ⚠️ **需扩展 1** | **`BroadcastNull` 或不参与** | token 由副本携带,不依赖中心 sharer set(per `ADR-CEP-17 §2 Decision 3`)|
| **MESI_Three_Level_HTM** | ❌ **明确排除** | — | 事务内存边界(HTM 需 read-set / write-set / 事务中止语义),超出纯缓存一致性模型(per `ADR-CEP-17 §2 Decision 5`)|

**关键映射证据**:gem5 Ruby 消息模型(RubyRequest / RubyResponse / RubySnoopResponse)→ CEP canonical 10 流 **100% 覆盖,零新增流**(per `docs/research/cep-coherence-protocol-space-survey.md §3.3`)。

> **注**:调研文档 `gem5-coherence-protocols-survey.md:27` 指出本快照中 `MESI_CMP_directory` 协议**不存在**(历史 gem5 曾有,现已删除,仅 MOESI 版存活);上表按调研文档原文照录。

#### 5.1.3 协议私有状态边界(per `ADR-CEP-17 §2 Decision 4`)

| 状态类别 | 例子 | 归属 |
|---------|------|------|
| **稳定状态(SSP)** | `I / S / E / M`(CEP Stub Cache 暴露) | Stub Cache Core `perm` 枚举 |
| **瞬态状态** | `T`(Temporary,per ArchForge 09 MOESI/GPU 6×6);ProtoGen 生成的 transient states | **Protocol Engine 插件内部** |
| **协议私有状态** | token(per MOESI_CMP_token);HTM 的 read-set / write-set | **Protocol Engine 插件内部** |

**依据**:Oswald **ProtoGen** 方法论建立在"从 SSP(稳定状态协议)生成非原子协议"之上 — 瞬态状态分配是其算法第 2 步(per `docs/research/cep-coherence-protocol-space-survey.md §4.1.1` / §4.1.2)。CEP 的分层与之一致:Stub Cache = SSP 层,Protocol Engine = 非原子事务层。

**守门 invariant**:`INV-CEP-05-IC1`(Stub 只暴露稳定状态)+ `INV-CEP-04-IC7`(token 类协议 tracker 不跟踪 sharer set)。

> **v0.2 证据补强**(per `docs/research/cep-coherence-protocol-space-survey.md` v0.2 §4.1.2 / §4.1.5):ProtoGen 论文实测 **MSI 协议含 18 个瞬态状态**,non-stalling MSI/MESI/MOSI 为 **18-20 状态 / 46-60 转换**(`ProtoGen_ISCA2018-draft.md:89-90`, `:1282-1285`)。→ **CEP Stub Cache 的 4 个稳定状态不是缓存模型,而是客户端侧协议状态投影**;真实协议的瞬态复杂度全部归 Protocol Engine 插件。

#### 5.1.4 协议语义实例(按一致性模型索引)(5 语义,per `ADR-CEP-17 §2 Decision 7`)

> **来源**:Oswald *Gen 算法族*(ProtoGen ISCA 2018 / HieraGen ISCA 2020 / HeteroGen HPCA 2022)论文 case study 集(per `HeteroGen_HPCA2022-draft.md:1266-1276` Table I)

> **⚠️ 分类维度澄清(v0.3 复审 R8)**:本小节**不是**与 §5.1.1 / §5.1.2 并列的"第三个族",而是**协议语义的索引视图**:
> - **"族"= 协议实例的来源集合** —— §5.1.1(工业硬件标准,6 族 / 8 实例)+ §5.1.2(gem5 Ruby 实现,1 族 / 8 实例)
> - **"一致性模型"= 协议实例的语义属性**,用于**索引语义**,**不是**来源分类
> - ⚠️ **两者交叉**:**MSI / MESI** 既是 §5.1.2 gem5 Ruby 族的实例,又是本表的 SC 语义 —— **同一协议语义的不同实现**,adapter 按 §5.1.1/§5.1.2 计,**不重复计数**
> - **落地口径**:本表列出的是**协议语义**;其 adapter 是否实现、归哪一族,由 §5.1.1 / §5.1.2 决定

| 协议语义 | 一致性模型 | 判定 | Sharer Tracker 选型 | adapter 归属 | 关键说明 |
|---------|-----------|------|-------------------|-------------|---------|
| MSI / MESI | **SC** | ✅ 直接支持 | `FullMapDirectory` | **§5.1.2**(gem5 Ruby MESI_Two_Level / Three_Level)| SWMR + writer-initiated invalidation;CEP 基线协议 |
| TSO-CC | **TSO** | ✅ 直接支持 | **`BroadcastNull` 或不参与** | 新增(若选为实现目标)| **主动放弃 sharer tracking** 以利用 TSO 松弛(per HeteroGen §VI-B)→ 配 snoop filter 自相矛盾 |
| RCC / RCC-O | **RC** | ✅ 直接支持 | **`BroadcastNull` 或不参与** | 新增(若选为实现目标)| 写缓冲 + release 时写回 + acquire 时自失效(不做中心目录)|
| **GPU**(源自 Spandex) | **RC** | ✅ 直接支持 | **`BroadcastNull` 或不参与** | 新增(若选为实现目标)| **write-through 直写** + 局部缓冲 —— **与 ArchForge 09 §4.1 的 GPU write-through 策略直接对应**;Spandex 协议不做 sharer tracking 式中心目录(per 调研 §3.5)|
| PLO-CC | **PLO**(partial-load-order) | ✅ 直接支持 | **`BroadcastNull` 或不参与** | 新增(若选为实现目标)| 只强制 W→W 与 R→W 序;属自失效类协议,不做 sharer tracking |

**Sharer Tracker 选型修正说明(per oracle 全面架构审查 N5)**:`SparseSnoopFilter` **本身即一种 sharer tracker**,与「主动放弃 sharer tracking」的协议特征**自相矛盾**。上述语义(TSO-CC / RCC / RCC-O / GPU / PLO-CC)均属自失效 / write-through 类,统一取 `BroadcastNull` 或「不参与」 —— 与 `ADR-CEP-17` 决策 3(token 类协议同原则)一致。

**前置条件(硬)**:**这组协议语义必须**依赖**一致性标签维度**(`ADR-CEP-12` schema 的 `consistency_labels` 字段),否则无法表达"同一 `barrier` 在 SC / TSO / RC 下效果不同"。per `ADR-CEP-17 §2 Decision 7` 与调研 §5.3 F6。

**诚实边界(必读,per `ADR-CEP-17 §2 Decision 9` + 调研 F19)**:
*Gen 族作者自陈其**两个独立工具**"无法在**无人工介入**下自动完成"分层 + 异构组合("...is **not possible with these tools without manual intervention**",博士论文 §9.2)。⚠️ 注意:这是**工具工程限制**,**不是算法不可能** —— 论文 §1.3 明示 HieraGen 阶段**可以**消费 HeteroGen 阶段的输出。
→ **CEP 的对应结论**:多层目录组合 + 跨一致性域组合**可做**,但走**人工声明 + 实验证伪**路线 —— 正确性由**随机 / litmus 实验在声明的 `verification_scope` 内尝试证伪**;**平台不提供无违例保证**(per `ADR-CEP-06 §3` 的 `verification_mode` 契约)。**不承诺自动化**;且 `ADR-CEP-07` 的插件接口须为组合预留接缝。

> **口径澄清(per oracle 全面架构审查 Q4)**:*Gen 族的限制分两层 —— **工具集成负担**(CEP **不继承**,因其不做自动合成)与**语义组合负担**(CEP **完全继承**,因其须人工套用全部组合判据)。准确表述是「**工具集成负担不继承;语义组合负担完全继承**」,而非「该限制不直接适用」。

**规模与验证参考**:
- HeteroGen 在 **gem5** 上以 64 核(60 tiny + 4 big)对比手工异构协议 **HCC**,平均性能相当、网络流量差异 < 5%(per `hetergen:1382-1400`)→ **gem5 已是该研究线的标准评测床**
- 验证手段为 Murϕ(Safety + Deadlock Freedom)+ **111 个异构 litmus test**(per `hetergen:1277-1316`)→ 对应 CEP `ADR-CEP-06` 的 `deadlock_violations` / `memory_violations` 闸

### 5.2 目录策略可插拔(Sharer Tracker 5 实现,per `ADR-CEP-04 §2`)

| 实现 | scope | completeness | authority | aggregation_at | 适用 |
|------|-------|--------------|-----------|----------------|------|
| `FullMapDirectory` | SYSTEM | EXACT | GLOBAL | HOME | CHI HN-F 精确目录 |
| `SparseSnoopFilter` | SYSTEM | CONSERVATIVE | GLOBAL | HOME | CHI SF / CXL 2.0 Host SF(宁多探不漏探)|
| `DcohSymmetric` | LOCAL_MEMORY_REGION | **必须 CONSERVATIVE** | LOCAL_HINT | REQUESTER 或 HOME | CXL 3.0 对称(per-device DCOH)|
| `TreeManager` | SUBTREE | EXACT | LOCAL | TREE_PARENT | TileLink ClientMetadata |
| `BroadcastNull` | NONE | EXACT(degenerate) | NONE | NONE | 无目录基线(target = all)|

**caps descriptor 替代 bool**(per `ADR-CEP-04 §2 Decision 3`):`scope` / `completeness` / `authority` / `aggregation_at` 四属性 struct,禁止任何 `is_distributed` / `is_central` bool flag。

**DCOH 完备性契约**(正确性命门):`CONSERVATIVE` = **绝不漏 sharer**(可超集);`PARTIAL` 必须声明 broadcast fallback;禁止无 fallback 的 PARTIAL。

### 5.3 集合通信旁路平面(per `ADR-CEP-03`)

**原语最小集**:`{REDUCE, BROADCAST, MULTICAST, BARRIER}`(AllReduce = REDUCE + BROADCAST 复合,不列原语)。

**SHARP atomic vs cache-coherent atomic 区分**(5 条防误分类规则,per `ADR-CEP-03 §3.1`):

| 规则 | 内容 |
|------|------|
| **R1** | opcode 永远不作分类器(源端插件只发一种 ATOMIC)|
| **R2** | 唯一分类点在 fabric ingress:`(dest_addr ∩ collective_region_table) && group_epoch_valid` |
| **R3** | 区域互斥强制(一个地址不得同时 SHARP 已注册 + coherence-cacheable;组注册时 drain)|
| **R4** | epoch 纪律(group_destroy 后区域回归 coherence 域;epoch 未 commit 前 coherence atomic = 正常服务)|
| **R5** | 可观测性(divert 计数器 + fail-stop 断言)|

**跨平面 ordering 契约**(5 场景,per `ADR-CEP-03 §3.3`):Coherent atomic ↔ SHARP reduce 同地址时,前者须先 completion、后者须先 commit;复合流由 Core 层编排,**禁止**通过拓宽 coherence 事务集表达。

### 5.4 Bundle 约束

- **协议 Bundle**(网络层,项目级):`NoCFlitBundle`(16 字段)/ `PcieTlpBundle`(frozen)/ `Axi4Bundle`(≥5 消费者)/ MSI-X / DLLP / TLP wire
- **业务 Bundle**(业务层,模块 owner):`CacheReqBundle` / `CacheRespBundle` / `GmmuPteBundle` / `SdmaCmdBundle` / `DoorbellBundle` / `CrBundle`(APB-lite LWC)/ `MsixBundle`
- **跨模块共享的业务 Bundle**(第三类:业务语义字段 + 项目级 owner,真源 1 `06 号 §3.3`):`CohIfBundle`(`include/bundles/cep_coherence_bundles_tlm.hh`)—— 承载 canonical 10 流中**跨 Stub↔PE 边界的 5 条 RN-side 流**(`request` / `completion` / `snoop` / `snoop_resp` / `writeback`;由**强类型** `flow_id` 区分,域外 5 条流**类型不可表达**);**不含网络层字段**;**取网络侧演进纪律**
- **编译期锁**:`BUNDLE_FIELD_WIDTH` static_assert + `is_trivially_copyable` 断言
- **不做**:协议专属字段泄漏进共享抽象(per `ADR-CEP-04 §3.2` 规范 intent 原则)

---

## 6. 仿真语义与集成 seam

### 6.1 simulation_mode 单一维度(per `ADR-CEP-07 §2 Decision 1`)

| 模式 | 含义 | 延迟 | 何时用 |
|------|------|------|--------|
| `functional` | 业务/网络逻辑正确性验证 | **0** | 前期性能建模期 |
| `timing` | 周期精确仿真 | > 0 | 后期协议特化时 |

**真源**:`06 v0.4 §5.1` + `01-hybrid §4.7`(per oracle v0.4 B3-5 实证 — 06 §4.7 不存在)。

**不引入 `network.mode`**:协议特化是**工程**(改模块代码),不是 JSON 字段切换(per `06 §5.2` gem5 SLICC 经验)。

### 6.2 两种模式的推进差异

| 仿真语义 | coherence 流推进 | group-scoped completion 推进 |
|---------|------------------|-------------------------------|
| `functional` | 同步直调(per `SimplePort::send`);事务完成 = 单次 tick 即达 | rendezvous 等待 = 占位(0 cycle),全组 ack 后即时 emit |
| `timing` | cycle-accurate;跨 cycle 累加 | rendezvous 等待 = 累计 cycle;未到齐不推进仿真周期 |

**实现**:`ProtocolEngine::advance_cycle()` 虚函数(per `chstream_module.hh:73` 默认空实现)按 mode 分发 — functional: no-op;timing: cycle accumulation。

### 6.3 ModuleFactory 集成 seam(per `ADR-CEP-07 §2 Decision 3`)

**真实注册机制**(不发明不存在的宏形式,per oracle v0.4 B7 实证):

| 模块类型 | 基类 | 注册机制 |
|---------|------|----------|
| **ProtocolEngine** 派生类 | `ChStreamModuleBase` | `ModuleFactory::registerObject<T>`(per `include/core/module_factory.hh:122`)|
| **SharerTracker** 派生类 | `SimModule`(per `module_factory.hh:135` `static_assert std::is_base_of_v<SimModule, T>`)| `REGISTER_MODULE(T)`(per `include/modules_cluster.hh:41-105`,合法参数化宏)|
| **FabricCollectivePlane** 派生类 | `SimModule` | `REGISTER_MODULE(T)` |
| **StubCacheCore** | `ChStreamModuleBase` | `ModuleFactory::registerObject<T>` + `ChStreamAdapterFactory::create` |

> **注意**:`REGISTER_CHSTREAM`(`include/chstream_register.hh:50`)与 `REGISTER_OBJECT`(`include/modules.hh:30`)**均无参数**,不存在 `REGISTER_CHSTREAM(T)` / `REGISTER_OBJECT(T)` 形式。

**冻结面**:CEP 不进 `cpptlm_emulator.h`(23 ABI 零 diff);不改 `include/tlm/pcie/pcie_endpoint_tlm.h` `[[deprecated]]` 状态;不接管 `dgpu_board_shell.cc`;不破 `include/tlm/cache_tlm.hh`。

### 6.4 Null 实现(解耦测试兜底)

- **`NullProtocolEngine`**:10 流覆盖矩阵 — 流 1/3/4/5/6/7/10 no-op;流 2 immediate-complete;流 8 immediate-resolve;流 9 immediate-complete
- **`NullCollectivePlane`**:所有原语返回"不支持" — CHI / CXL 配置下的默认绑定,是"启停干净"的可测试载体

---

## 7. 实验输出协议

### 7.1 4 件套(per `ADR-CEP-06 §2`)

```
experimental/<run_id>/
  ├── simulation_config.json      # JSON 配置快照(输入)
  ├── trace.h5                   # HDF5 trace(per cycle per txn 事件)
  ├── metrics.json               # 关键 metric 数值
  └── report.html                # A/B 对比报表
```

`run_id` 格式:`{protocol}-{topology}-{directory}-{timestamp}`

### 7.2 13 个必纳 metric

| Metric | 类型 | 单位 | 含义 |
|--------|------|------|------|
| `total_cycles` | int | cycles | 仿真总周期数 |
| `total_transactions` | int | count | 完成事务总数 |
| `transactions_per_cycle` | float | txn/cycle | 吞吐 |
| `avg_latency_cycles` | float | cycles | 平均事务延迟(request→completion) |
| `p50_latency_cycles` | float | cycles | 50 分位延迟 |
| `p95_latency_cycles` | float | cycles | 95 分位延迟 |
| `p99_latency_cycles` | float | cycles | 99 分位延迟 |
| `max_latency_cycles` | float | cycles | 最大延迟 |
| `coherence_stalls` | int | count | coherence 事务因 Ordering/credit/snoop 等待的周期数 |
| `collective_stalls` | int | count | collective 事务因 group rendezvous 等待的周期数 |
| `directory_misses` | int | count | directory lookup 失败次数 |
| `deadlock_violations` | int | count | 死锁违例次数(应 0,否则 fail)|
| `memory_violations` | int | count | 一致性违例次数(应 0,否则 fail)|

**4 个可选 metric**:`link_utilization_per_link` / `vc_stall_per_vc` / `snoop_fanout_per_request` / `directory_size_distribution`

**兜底契约**:不可观测 metric 标 `not-collected` + `_not_collected_reason`,**禁止**省略 / 写 0 / 写 -1。

### 7.2.1 实验验证深度契约(per `ADR-CEP-06 §3`)

- 每次实验**必须**声明 `verification_mode` ∈ {`exhaustive`, `bounded_random`, `sampled`} 与 `verification_scope`(规模上限描述)
- `bounded_random` / `sampled` 的结果**不得**表述为「已证明无死锁 / 无违例」—— `deadlock_violations == 0` 只代表「在声明的 `verification_scope` 内未观测到」
- 平台**不承诺**「任意配置都能穷尽验证」—— 这是规模事实,不是实现缺陷

> 完整契约见 [`ADR-CEP-06 §3`](../adr/ADR-CEP-06-experiment-protocol.md)(含一手规模上限依据:极端配置需 1TB RAM + 数小时)。**本节不复制具体数字,避免第二真源。**

### 7.3 sweep / A-B 工具

| 工具 | 功能 |
|------|------|
| `tools/run_sweep.py` | 维度组合(protocol × topology × directory)→ N 个 4 件套 + `sweep_summary.csv` |
| `tools/ab_compare.py` | baseline vs target → `delta_metrics.json` + CDF overlay + `report.html` |
| `tools/render_report.py` | `metrics.json` + `trace.h5` → `report.html`(summary 表 + latency CDF + violation badges) |
| `tools/aggregate.py` / `plot.py` / `recommend.py` | 聚合 / 排名图 / 阈值建议 |

---

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
| **§7.4 P3** SnoopFilter directory-based | `ADR-CEP-04 §2` SharerTracker 5 实现 |
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

> **口径(2026-10-27 活口径表同步,per 台账 Entry #82 授权)**:本表是**实现状态的活映射** —— 状态以**实测**为准(代码载体 + 行为化测试 + 归档件三者一致),**不复制** ADR 判据文本。
> **图例**:🟡 = 部分落地 / 降级实现(逐行标注缺口与承接队列)· ✅ = 已落地且有行为化载体 · ⛔ = scope reduction。
> **本表只登记状态**,不改 §3 ~ §8 的架构正文(per ADR 不可变 + Status Update 先例)。

| 能力 | 定义位置 | 实现状态(2026-10-27 实测) | 承接 change |
|------|---------|-------------------------|-----------|
| canonical 10 流契约 | §3.1 | 🟡 **部分落地** —— Stub Cache RN 侧 5 流(`stub_cache_core.hh` 6 接口)已行为化;`MoesiGpuAdapter` 6 事件已行为化;`protocol_instances.json` 的 `flow_support` 10 布尔已填;**载体已落地**(`CohIfBundle` —— `include/bundles/cep_coherence_bundles_tlm.hh`,Entry #84;`ProtocolEngine` 基类 —— `include/tlm/cep/protocol_engine_base.hh`,Entry #85) | 载体 = `cpptlm-cep-cohif-carrier`(队列 #7) |
| Stub Cache Core | §3.2 | ✅ **已落地** —— 6 接口 / 5 态 / `INV-SC1`~`SC6` + `INV-CEP-05-IC1` 行为化(14 测试) | `cpptlm-cep-stub-cache-core`(2026-10-25 已归档) |
| SharerTracker 5 实现 | §5.2 | 🟡 **1 / 5** —— `FullMapDirectory` 唯一行为实现已落地(10 测试);其余 4 个(`SparseSnoopFilter` / `DcohSymmetric` / `TreeManager` / `BroadcastNull`)**仅声明层 traits**(`sharer_tracker.hh:178-195`) | 队列 #9 ~ #11(队列 #9 另需第 6 实现 `HierarchicalDirectory`) |
| Collective Plane | §5.3 | 🟡 **骨架已落地** —— `CollectivePlaneIf` 恰 7 方法 + `NullCollectivePlane` + `LocalCollectivePlane`(group 生命周期 / region-epoch / BARRIER rendezvous);**REDUCE / BROADCAST / MULTICAST 数据路径未实现**(返 `Status::DataPathNotImplemented`);`fabric_ingress_classify` 路由接线未实现 | 骨架 = `cpptlm-cep-collective-plane-base`(2026-10-26 已归档);数据路径 = Stage 3 |
| ProtocolEngine adapter 实例(§5.1.1 工业 8 + §5.1.2 Ruby 8 = 16;§5.1.4 候选语义落地后为 20) | §5.1 | 🟡 **1 个已落地** —— `MoesiGpuAdapter`(6 态 × 6 事件表驱动 + 6→4 perm 投影 + 6→3 消息类映射),是**首个 `status=implemented` 实例**;**`ProtocolEngine` 基类已落地**(`ProtocolEngineBase<N>`,Entry #85);**`NullProtocolEngine`(§6.4 + `INV-CEP-07-IC2`)亦已落地**(10 流覆盖矩阵 + 2 条编译期锁,Entry #85)—— **但 adapter 实例仍是 1 个**(`MoesiGpuAdapter`):基类落地 ≠ 插件实例落地 | `cpptlm-cep-moesi-gpu-adapter`(2026-10-27 已归档);载体 = 队列 #7 |
| ModuleFactory seam | §6.3 | 🟡 **部分落地** —— 装配期判据闸门已接(`src/core/module_factory.cc:167-186`,`run_channel_gate` + `run_ordering_gate`,FAIL 即中止实例化);**协议桥钩子已从死钩子变活钩子**(`register_protocol_bridge(name, factory_lambda)` + `get_bridges()` 转 public + `ModuleFactory` **Step 7.5** 消费路径,Entry #86);**`REGISTER_CHSTREAM_BIDIRECTIONAL` 宏仍未开始**(Phase 4)⇒ **0 个 CEP 类注册**(`REGISTER_MODULE` / `REGISTER_OBJECT` 零命中)⇒ `INV-CEP-07-IC4` **仍无载体** | 队列 #7 Phase 4(`REGISTER_CHSTREAM_BIDIRECTIONAL` + CEP 类注册) |
| 实验输出 4 件套 | §7 | ✅ **已落地** —— `tools/{render_report,run_sweep,ab_compare}.py` + `experimental/metrics.schema.json` + 4 件套契约测试(`test_cep_canonical_invariants.cc` / `test_cep_dummy_signals.cc` / `test_cep_module_factory_seam.cc` / `test_cep_simulation_mode_lock.cc`) | Stage 0(`cpptlm-cep-umbrella`,2026-10-23 已归档) |
| simulation_mode 单一维度 | §6.1 | ✅ 已落地(`06 §5.1` + `01-hybrid §4.7` + `VramControllerTLM::set_mode`)| — |
| Bundle 二分类 | §5.4 | ✅ 已落地(`06 §3.2` 18 个 .hh;**第三类已由 `06 §3.3` 增补**,其唯一实例 `CohIfBundle` **已落地**(2026-10-27 per 台账 Entry #84)—— 见第四则 Status Update ③)| — |
| Python 拓扑生成 | §8.1 沿用 `06 §7` | ✅ 已落地(`cpptlm.library.mesh_cluster/ring_cluster`)| — |
| IOMMU 完整协议 | §8.2 P5 | ⛔ scope reduction **待签发独立 ADR**(per `ADR-CEP-00 §3`)| 待定 |
| Ordering Point 可迁移性判据 | `ADR-CEP-08 §3` | ✅ **已落地** —— J1 / J2 / J3 + `INV-CEP-08-IC2` 行为化(6 测试);**J3 通用形式不可静态判定 ⇒ 多点显式 `SKIP`**;J2 在合法声明上 vacuous PASS | `cpptlm-cep-directory-ordering-plane`(2026-10-25 已归档) |
| 通道依赖图无环检查 | `ADR-CEP-09 §3` | ✅ **已落地** —— 装配期逐 `(network, vc)` verdict + DFS 三色环检测(8 测试) | `cpptlm-cep-directory-ordering-plane`(2026-10-25 已归档) |
| 协议实例能力声明 schema | `ADR-CEP-12 §3` | ✅ **已落地** —— `protocol_instance.schema.json`(M1 冻结集)+ 5 条 AC 判据 + `protocol_instances.json` 声明源(2 实例);**首个 `implemented` 实例的装配期复核已通过** | `cpptlm-cep-stub-cache-core` / `cpptlm-cep-moesi-gpu-adapter`(均已归档) |
| 跨域组合判据 | `ADR-CEP-13 §3` | 🟡 **部分落地** —— 判据 (a)(b) 已裁定(`moesi_gpu` 静默 `E→M` = 域可见写);**跨域人工声明 + 实验证伪未落地** | Stage 2(队列 #12 ~ #14) |

---

## §附录 · Status Update 2026-10-25(per 台账 Entry #62)

> **授权**:Entry #62(队列 #4 建 change + DRI 四项裁决:S1-1 ②建图层 + S1-7 (β) 同批 + M-4 行为侧登记保留 + scope 切小)。
> **口径**:本节不改动正文;只**登记授权** + 3 个 change 引用修正。
> **引用修正清单**:
> - `§7.4 P3 SnoopFilter → SharerTracker 5 实现` 的 change 关联:队列 #3(已完成归档)的 M-4 契约 + 队列 #4(即将实施)的**FullMapDirectory 唯一行为实现**(per 2026-10-25 DRI 裁决「范围切小」,其余 4 实现跟随队列 #6~#9)
> - `ADR-CEP-04 §8` 的 4 个后续 change 名(`cpptlm-cep-sharer-tracker-base/-impls/-cxl-dcoh/-tilelink-tree`)**失指已处置**(该 ADR §10 Status Update)
> - **SharerTracker 实施载体**改为:**`cpptlm-cep-directory-ordering-plane`**(单 change,1 KPI slot)
> **冻结面**:本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #62)。

## §附录 · Status Update 2026-10-27(per 台账 Entry #74)

> **授权**:Entry #74(队列 #7 `cpptlm-cep-cohif-carrier` 立项 + KPI 排队 13 → 14 + `ADR-CEP-19` 签发)。
> **口径**:本节**不改**正文,仅登记授权 + 1 处**活口径表**的机械同步 + 3 处引用修正。
> **① §8.1 Stage 表的机械同步(活口径,per Entry #27 / #71 同款先例)**:该表原写「Stage 1 = **7** / 合计 **11**」—— **已落后两代**(连 2026-10-26 的 13 都未跟上)。`ADR-CEP-00 §2 Decision 3` 的排队现为 **14**(Stage 1 = **10**)⇒ **同批机械同步**为 Stage 1 = **10** / 合计 = **14**,并把 Stage 1 的内容描述从「3 协议 RN-F subset」补全为「**CohIf 载体** + 5 协议实例(RN-F subset / `moesi_gpu` / `mesif` / CXL 非对称 / TileLink-C)」。**本表仍只承载「Stage 划分 + change 数」**,**不含**时间跨度与估时(排期真源 = `obj §4.0`)。
> **② §6.3 ModuleFactory 集成 seam 的落地排期**:该节表规定「**ProtocolEngine** 派生类 = `ChStreamModuleBase` + `ModuleFactory::registerObject<T>`」等四类派生类 —— 仍是**最终形态**口径;**落地**排在队列 #7 `cpptlm-cep-cohif-carrier`(见 `ADR-CEP-07` Status Update 同日登记)。
> **③ §5.4 Bundle 约束的落点**:该节列的业务 / 网络 Bundle 清单**不含** CohIf Bundle —— 其交付队列为 #7,路径 `include/bundles/cep_coherence_bundles_tlm.hh`(per `ADR-CEP-19` 决策 6;**避开** `ADR-CEP-07A` 明令「不创建」的 `coherence_bundles_tlm.hh`)。**本节正文不改**,待队列 #7 实施后再同步。
> **冻结面**:本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #74)。

## §附录 · Status Update 2026-10-27(第二则 · per 台账 Entry #82 —— 实现状态映射漂移回填)

> **授权**:Entry #82(P1 架构文档漂移回填 —— 由用户 2026-10-27 直接指令授权;配套 P0 = commit `d52fff56`)。
> **口径**:本节**不改**架构正文(§2 ~ §10 的定义 / 职责 / 判据一律不动),仅登记授权 + **2 处活口径表的机械同步** + 失指引用修正。**冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #82"`。
>
> **① §11「Gap → 实现状态映射」全表机械同步(活口径表,per Entry #27 / #71 / #74 同款先例)**:该表原写「📐 契约已定义,🛠️ 未实现」11 行 —— **落后两代**(队列 #3 ~ #6 全部已归档,状态早已翻转)。本次按**实测**(代码载体 + 行为化测试 + 归档件三者一致)逐行重写为 🟡 / ✅ / ⛔ 三态并标注缺口与承接队列:
> - **✅ 已完全落地(4 行)**:Stub Cache Core(`cpptlm-cep-stub-cache-core` 已归档)· 实验输出 4 件套(Stage 0 `cpptlm-cep-umbrella` 已归档)· Ordering Point 可迁移性判据 + 通道依赖图无环检查(均 `cpptlm-cep-directory-ordering-plane` 已归档)· 协议实例能力声明 schema(`cpptlm-cep-stub-cache-core` / `-moesi-gpu-adapter` 已归档)
> - **🟡 部分落地(5 行)**:canonical 10 流契约(载体未落地)· SharerTracker 5 实现(**1/5**,其余 4 个仅 traits)· Collective Plane(骨架已落地,3 原语数据路径未实现)· ProtocolEngine adapter 实例(**1 个**,且基类本身不存在)· ModuleFactory seam(闸门已接,**0 个 CEP 类注册** ⇒ `INV-CEP-07-IC4` 无载体)· 跨域组合判据(判据已裁定,人工声明未落地)
> - **失指引用修正**:原「承接 change」列的 `cpptlm-cep-sharer-tracker-base` —— 该**change 目录不存在**(仅存同名 main spec `openspec/specs/cep-sharer-tracker-base/spec.md`);实施载体是 `cpptlm-cep-directory-ordering-plane`(同文件 2026-10-25 附录「SharerTracker 实施载体改为」已声明,本次 §11 漏同步)⇒ 已改为队列 #9 ~ #11 并标注队列 #9 另需第 6 实现 `HierarchicalDirectory`
>
> **② §5.1.1 工业协议族表的**状态列**机械同步(活口径,per 同款先例)**:原 `moesi_gpu` / `mesif` 两行状态列均写「**未排期**」—— `moesi_gpu` 已于 2026-10-27 随队列 #6 **完成归档**(首个 `status=implemented` 实例),`mesif` 已**排期至队列 #9**(⚠ R-1 阻塞登记;季度见 `obj §4.0`)⇒ 同步为「✅ 已落地」/「📋 已排期」。**族计数(1 族 / 2 实例)与 `Entry #12` / `#13` 的历史补记不动**;**状态值以 `obj §4.0` 排队总表为唯一权威**,本表不复制排期。
>
> **③ §5.4 Bundle 约束:本节正文仍不改**(per 本文件 2026-10-27 第一则附录 ③ 的既有处置 —— 待队列 #7 落地 `CohIfBundle` 后再同步第三类)。本则仅在 §11 的「Bundle 二分类」行注明「**第三类已由 `06 §3.3` 增补**,其唯一实例待落地」。
>
> **④ 未随本则改动的已知缺口(登记,不处理)**:`NullProtocolEngine`(`§6.4` + `INV-CEP-07-IC2`)**无代码载体且无队列 owner**;`06 号 §15.1` / `§15.4` 尚未加「时间线转隶 CEP」注记;`06 号 §9.1` 的 `module_factory.cc` 行号漂移 ≈25 行 —— 三项均在**本仓 06 号 / 01 号**(非本冻结面成员),由同批 P1 在 06 号 / 01 号侧处置,本文件不代改。
>
> **⚠️ 后续处置(2026-10-27,per 台账 Entry #83)**:上列 3 项**已全部处置** —— ① `NullProtocolEngine` owner = 队列 #7 task 2.5 / 2.6 / 2.7(增补);② `06 §15.1` / `§15.4`「时间线转隶 CEP」注记已加(commit `4d1e54fa`);③ `06 §9.1` 行号已刷新(commit `4d1e54fa`)。
>
> **冻结面**:本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #82)。

## §附录 · Status Update 2026-10-27(第三则 · per 台账 Entry #83 —— 载体缺口登记与 owner 指派)

> **授权**:Entry #83(P2 收口:潜伏编译缺陷消解 + C9 机器断言补齐 + `NullProtocolEngine` owner 指派;用户 2026-10-27 直接指令授权)。
> **口径**:本节**不改**架构正文(§2 ~ §10 的定义 / 职责 / 判据一律不动),仅登记 ① §11 一处**活口径**的 owner 补登 + ② 第二则 ④ 的处置回填 + ③ 两份 ADR 的 Status Update 指向。
> **冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #83"`。

> **① §11「ProtocolEngine adapter 实例」行的 owner 补登(活口径)**:该行原文记「**`ProtocolEngine` 基类本身不存在**」—— 事实仍成立,但**`NullProtocolEngine`(§6.4 + `INV-CEP-07-IC2`)此前既无载体又无 owner** ⇒ 重演「无主债」。**本次指派**:owner = **`cpptlm-cep-cohif-carrier`(队列 #7)task 2.5 / 2.6 / 2.7** —— 理由:队列 #7 的 Phase 2 交付 `ProtocolEngineBase` 派生 `ChStreamModuleBase`,而 `INV-CEP-07-IC2` 规定 `NullProtocolEngine` 是 ProtocolEngine 的**默认实现**且须有 **10 流覆盖矩阵**(逐流行为 per §6.4)⇒ **建基类者同时交付其 ADR 强制的默认实现**,属该 change 的契约面自洽,非范围蔓延。task 2.6 / 2.7 附**反向 fixture**(覆盖矩阵缺 1 流 / 某流语义退化 ⇒ 必红)。

> **② 第二则 ④「未随本则改动的已知缺口」的处置回填**:该处登记的 3 项**已全部处置** —— ① `NullProtocolEngine` owner = 队列 #7 task 2.5~2.7(见 ①);② `06 号 §15.1` / `§15.4`「时间线转隶 CEP」注记已加(commit `4d1e54fa`);③ `06 号 §9.1` 行号已刷新(commit `4d1e54fa`)。

> **③ 本则关联的两份 ADR Status Update(同日追加,授权同源)**:
> - `ADR-CEP-14` Status Update 2026-10-27 —— C9 四条要求的**覆盖现状逐条对账** + C++ 侧机器断言补齐 + **变异测试反证**(含一次真实的「负向断言恒真空」缺陷登记)。
> - `ADR-CEP-07A` Status Update 2026-10-27(第二则)—— §4 验证口径第 2 项的**不实声称订正**(「Stage 0 由 `test_cep_canonical_invariants.cc` 的 C9 覆盖」实测不成立)+ `INV-CEP-07A-IC2` 与决策 1 第 7 项 ③ 的**时效张力登记** + C++ 侧 IC2 覆盖现状表。
>
> **④ 本则未处理、已登记的项**:① `INV-CEP-07A-IC2` 的**时效限定**(拓扑 Python 层自 Stage 2 起适用)属**收紧 invariant** = 决策变更 —— **待 architect 裁决**,builder 不自行签发;② `ADR-CEP-14` C9 第 1 条的「反射 / 类型 trait」形态仍未落地(当前以标识符词扫描近似覆盖);③ `tlm::cep::ContractVerdict` 同名冲突已消解(commit `7f91abd9`),但 **`openspec/specs/` 与 ADR 契约面均无该符号名** ⇒ 无 delta spec 变更需求。

> **冻结面**:本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #83)。

## §附录 · Status Update 2026-10-27(第四则 · per 台账 Entry #84 —— CohIf 载体落地 + §5.4 第三类同步)

> **授权**:Entry #84(队列 #7 `cpptlm-cep-cohif-carrier` **Phase 1 实施** —— `CohIfBundle` 载体落地;用户 2026-10-27 直接指令授权)。
> **口径**:本节仅登记 ① §5.4 的**已到期同步**(第一则附录 ③ 明确「待队列 #7 实施后再同步」⇒ **本次即该同步点**)+ ② §11 一处**活口径**的计数/状态机械同步。**§2 ~ §4 / §6 ~ §10 的定义 / 职责 / 判据零改动**。
> **冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #84"`。

> **① §5.4 Bundle 约束:第三类同步(第一则附录 ③ 的已到期动作)** —— 该节原列「协议 Bundle / 业务 Bundle」**二分**,`CohIfBundle` **不在其中**;第一则附录 ③ 当时的处置是「**本节正文不改,待队列 #7 实施后再同步**」。**队列 #7 Phase 1 已交付 `CohIfBundle`**(2026-10-27,`include/bundles/cep_coherence_bundles_tlm.hh`)⇒ **本次执行该同步**:§5.4 增列**第三类**「跨模块共享的业务 Bundle」+ 其唯一实例 `CohIfBundle`。**§5.4 的其余三条(协议 Bundle 清单 / 业务 Bundle 清单 / 编译期锁 / 不做)** 与 §3.1 / §3.2 / §6 ~ §10 一律零改动。

> **② §11「Bundle 二分类」行的活口径同步** —— 原写「`06 §3.2` **17** 个 .hh …其唯一实例**待落地**」⇒ 现为「**18** 个 .hh …其唯一实例 `CohIfBundle` **已落地**(Entry #84)」。

> **③ `06 号` / `01 号` 侧的配套计数同步(真源 1,非冻结面成员)** —— 新增 1 个 `.hh` ⇒ `ls include/bundles/*.hh` 由 17 → **18**、`bundle_base` 派生类型由 25 → **26**、Bundle 类型总数 26 → **27**。`06 号` §1.3 / §1.4 图 / §2 / §3.2 / §3.3 与 `01 号` §1.2 的计数已同批同步(per 台账 Entry #84);`06 号` §1.3 第 3 项「不引入新 Bundle 类型」已加**例外**说明(`CohIfBundle` 属 §3.3 第三类,不在该文件二分讨论范围内)。

> **④ 本则未处理、已登记的项** —— ① `CohIfBundle` 的**承载面**(ProtocolEngine 端口面 / `ProtocolBridge` / JSON 装配连通性)尚未落地 —— 归队列 #7 **Phase 2 ~ 5**;② `07 号` §6.4 的 `NullProtocolEngine` 仍无载体 —— 归队列 #7 task 2.5~2.7(owner 已于 Entry #83 指派);③ `INV-CEP-07A-IC2` 的时效限定与 `ADR-CEP-14` C9 第 1 条的「反射 / 类型 trait」形态仍**待 architect 裁决**(Entry #83 已登记,本则不重复)。

> **冻结面**:本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #84)。

## §附录 · Status Update 2026-10-27(第五则 · per 台账 Entry #85 —— ProtocolEngine 端口面落地 + Null 默认实现)

> **授权**:Entry #85(队列 #7 `cpptlm-cep-cohif-carrier` **Phase 2 实施** —— ProtocolEngine 端口面 + `NullProtocolEngine`;用户 2026-10-27 直接指令授权)。
> **口径**:本节仅登记 ① §11 两处**活口径**的机械同步 + ② §6.4 的**实现状态**登记。**§2 ~ §5 / §6.1 ~ §6.3 / §7 ~ §10 的定义 / 职责 / 判据零改动**。
> **冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #85"`。

> **① §11 两处**活口径**的机械同步**:「canonical 10 流契约」行的载体口径由「`CohIfBundle` 已落地 / `ProtocolEngine` 基类仍不存在」改为「**两者均已落地**」;「ProtocolEngine adapter 实例」行补登 `ProtocolEngineBase<N>` 与 `NullProtocolEngine` 已落地 —— **但 adapter 实例仍是 1 个**(`MoesiGpuAdapter`):**基类落地 ≠ 插件实例落地**,该行的主计数不变。

> **② §6.4 Null 实现的**状态登记**(正文不改)**:§6.4 的「`NullProtocolEngine`:10 流覆盖矩阵 — 流 1/3/4/5/6/7/10 no-op;流 2 immediate-complete;流 8 immediate-resolve;流 9 immediate-complete」**仍是口径本身**,本次不改;登记其**已落地**:`include/tlm/cep/protocol_engine_base.hh`(`ProtocolEngineBase<N>` + `NullProtocolEngine<N>`),10 流覆盖矩阵以**单一真源**(`kNullFlowMatrix`)落盘,并由**两条编译期锁**钉住(① 覆盖完整性:canonical 10 流各恰好一次且总数 10;② 逐流行为与本节口径一致)。⇒ **`INV-CEP-07-IC2` 的「默认实现 + 10 流覆盖矩阵」两条要求已可执行验证**(此前全仓零载体)。

> **③ 与 `ADR-CEP-07` 的同步**:决策 4 草图的落点路径(`include/tlm/cep/null_protocol_engine.hh`)与实现落点(`protocol_engine_base.hh`)**不同**,已登记为**路径偏离**(语义零变化)—— 详见 `ADR-CEP-07` Status Update 2026-10-27(第二则)②③。

> **④ 本则未处理、已登记的项**:① ProtocolEngine 的**注册与 JSON 装配**(`REGISTER_CHSTREAM_BIDIRECTIONAL` 宏 + `module_factory.hh` 钩子扩展)尚未落地 ⇒ 归队列 #7 **Phase 3 ~ 5**;② `INV-CEP-07-IC4`(0 个 CEP 类注册)**仍无载体** —— 同上;③ 本仓 `BidirectionalPortAdapter::tick()` 的**实现**目前**硬编码 `NoCFlitBundle` 语义**(读 `flit_category` 决定 `PKT_REQ`/`PKT_RESP`、credit 用 `NoCFlitBundle`)⇒ 用它承载 `CohIfBundle` 需要**泛化** —— 归 Phase 4 / 5,**本则不处理**。

> **冻结面**:本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #85)。

## §附录 · Status Update 2026-10-28(第六则 · per 台账 Entry #86 —— ProtocolBridge 特化桥落地 + 钩子死→活 + 判据分层)

> **授权**:Entry #86(队列 #7 `cpptlm-cep-cohif-carrier` **Phase 3 实施** —— ProtocolBridge 特化桥 + `register_protocol_bridge` 死钩子变活钩子 + per-bridge 元数据;用户 2026-10-28 指令授权)。
> **口径**:本节仅登记 ① §11 活口径的**机械同步** + ② Phase 3 的**落地状态** + ③ **判据分层的架构事实**。**§2 ~ §5 / §6.1 ~ §6.3 / §7 ~ §10 的定义 / 职责 / 判据零改动**。
> **冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #86"`。

> **① §11「ModuleFactory seam」行的活口径同步**:原写「0 个 CEP 类注册 ⇒ `INV-CEP-07-IC4` 无载体 | 队列 #7(`REGISTER_CHSTREAM_BIDIRECTIONAL` + `module_factory.hh:313-315` 死钩子扩展)」⇒ 现为「**死钩子扩展已实施**(`register_protocol_bridge(name, factory_lambda)` + `get_bridges()` 转 public + `ModuleFactory` Step 7.5 消费路径,Entry #86);**`REGISTER_CHSTREAM_BIDIRECTIONAL` 宏仍未开始**(Phase 4)⇒ **0 个 CEP 类注册** ⇒ `INV-CEP-07-IC4` **仍无载体**」。**该行的主结论(0 个 CEP 类注册)不变**。

> **② Phase 3 落地登记(正文不改)**:交付 `include/framework/protocol_bridge.hh`(`ProtocolBridgeEr` 类型擦除基类 + `ProtocolBridge<InB,OutB,Ordered>` 模板子类 + `AbstractProtocolBridge<FlitT>` identity 基线)+ `include/tlm/cep/cohif_noc_bridge.hh`(CohIf ↔ NoC 特化桥)+ `include/tlm/cep/protocol_family.hh`(协议来源族 ↔ 数字编号的唯一真源)。
> 判据状态:**CC-03** 两层落地(① 消息内容层:共有字段 4 个双向无损 + 协议语义字段显式登记丢弃/降维;② 端到端层:`Packet` payload 数据区承载完整 12 字段 ⇒ 逐字段恒等)· **CC-05** 四子条件全部落地(注册表可读 / Step 7.5 消费 / `(name, factory_lambda)` 形态 / per-bridge 元数据齐备)· **CC-06** **引用**队列 #6 的 AC-04 实现(未重立第二份)。
> ⚠️ **「端到端层」是判据的**加强**而非削弱** —— 「CohIf 语义跨网络不透损」由**跨真实网络的往返**保证(原单层判据只覆盖同进程内 Bundle 往返)。详见 `ADR-CEP-19` Status Update 2026-10-28(第二则)②。

> **③ 架构事实登记(本则核心)—— 「Bundle 级逐字段 round-trip 恒等」不可达**:`CohIfBundle.flow_id` 域 = **5 值**;`NoCFlitBundle.flit_category` 域 = **3 值**;队列 #6 的 6→3 消息类映射表是**刻意多对一**的 ⇒ **5→3 多对一有损** ⇒ 仅靠网络 flit 字段空间**不可能**唯一恢复 `flow_id` ⇒ 协议语义(`perm`/`snoop_type`/`line_state`/`hit`/`dirty`/`error`/`atomic` + `flow_id`)**必须**有自己的落脚点 ⇒ 落在 **Packet payload 数据区**(2026-10-28 DRI 裁决的 **B′ 方案**;**不**用 `tlm_extension` ⇒ 不触 `06 号 §4.5` 的 P3+ 排除项)。

> **④ per-bridge 元数据口径(2026-10-28 DRI 裁决)**:**`protocol_id_` = 协议来源族的数字编号**(`family` ∈ {`industrial`,`ruby-slicc`,`academic`},编号真源在 `include/tlm/cep/protocol_family.hh`);**实例级唯一性由 `bridge_id_` 负责**。⚠️ 判据 CC-05 ④ 原措辞「`protocol_id_` 与实例声明的 `family` 一致」若按「相等」读则**恒真**(多个 industrial 实例同族)⇒ 本次按**编号对齐**落地,判据**未被削弱**反而可机器检查。

> **⑤ 行号漂移登记(本文件正文的行号引用按「ADR 不可变」不改)**:本文件引用的 `include/core/module_factory.hh:122`(现 **128**)/ `:135`(现 **141**)/ `module_factory.hh:313-315`(现 **336-343**)与 `chstream_module.hh:73`(现 **74**)/ `:54`(现 **55**)**均已语义漂移**(符号仍在,行号变了)⇒ 完整对照表见 `06 号` 文末「附录 · 行号对照表」(2026-10-28)。

> **⑥ 本则未处理、已登记的项**:① `REGISTER_CHSTREAM_BIDIRECTIONAL` 宏与 CEP 类注册 ⇒ 归队列 #7 **Phase 4 / 5**(`INV-CEP-07-IC4` 仍无载体);② `BidirectionalPortAdapter::tick()` 仍**硬编码 `NoCFlitBundle` 语义** ⇒ 承载 `CohIfBundle` 需泛化 ⇒ 归 Phase 4 / 5(第五则 ④ 已登记,本则**状态不变**)。

---

**版本**: v0.1 起草(2026-10-20) + Status Update 2026-10-25(队列 #4 启动)+ Status Update 2026-10-27(队列 #7 立项 + 排队 13 → 14)+ Status Update 2026-10-27 第二则(Entry #82 —— 实现状态映射漂移回填)+ Status Update 2026-10-27 第三则(Entry #83 —— 载体缺口登记与 owner 指派)+ **Status Update 2026-10-27 第四则(Entry #84 —— CohIf 载体落地 + §5.4 第三类同步)** + **Status Update 2026-10-27 第五则(Entry #85 —— ProtocolEngine 端口面落地 + Null 默认实现)** + **Status Update 2026-10-28 第六则(Entry #86 —— ProtocolBridge 特化桥落地 + 钩子死→活 + 判据分层)**
**批准状态**: 🟡 待 Oracle 复审
**维护者**: CppTLM build agent(architect mode)