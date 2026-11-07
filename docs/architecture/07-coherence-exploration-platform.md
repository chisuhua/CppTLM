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

### 3.1 清单(per `ADR-CEP-02 §2`,首次定义)

| # | 流名 | 方向 | 完成纪律 | 语义 |
|---|------|------|---------|------|
| 1 | `request` | node → center | requester-scoped | 缓存行请求(读 / 唯一化 / 写回 / 驱逐) |
| 2 | `completion` | center → node | requester-scoped | 请求完成 + data(若 inline)+ perm + error |
| 3 | `snoop` | center → node | requester-scoped | snoop 查询 / 失效 / clean / forward |
| 4 | `snoop_resp` | node → center | requester-scoped | snoop 响应:hit + state + dirty + data + txn_id |
| 5 | `writeback` | node → center | requester-scoped | 驱逐/写回:addr + data + dirty |
| 6 | `forward_data` | node ↔ node(经中心调度)| requester-scoped | 数据转发:src_addr + dst_addr + data + perm |
| 7 | `back_invalidate` | center → node | requester-scoped | 反向失效:addr + type(invalidate/clean)+ txn_id(CXL 3.0 DCOH 的 BISnp 走这里;⚠️ **方向反转例外** = `node → center`(device→host,S2M),其响应 BIRsp = M2S —— 方向反转例外说明见 `ADR-CEP-02` §2 表下)|
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

### 5.1 协议族 → 协议实例 → adapter 映射

> **表述纪律**(per `ADR-CEP-17 §3.2`):"协议族" = **协议实例的来源集合**;"协议实例" = 一个具体可实现协议(1 个 `.slicc` / 1 个 adapter)。**adapter 数量 = 协议实例数**,不是族数。
>
> **当前口径**:**2 个来源族(工业 6 族 / Ruby 1 族)= 16 个 adapter 实例** + §5.1.4 的 **5 个一致性模型语义索引**(交叉视图,不重复计 adapter)。

#### 5.1.1 工业硬件协议族(6 族 / 8 实例)

| 协议族 | 协议实例 | adapter | 状态 | 通道模型 | 关键差异 |
|--------|---------|---------|------|----------|---------|
| **CHI** | CHI RN-F | `ChiRnfAdapter` | **✅ 已落地**(队列 #8,2026-10-28 归档;Stage 1 subset → Stage 2 full) | RN-F 6 通道(TXREQ/RXRSP/RXDAT/RXSNP/TXRSP/TXDAT)| RN-F 只发 REQ 只收 SNP;Credit 流控;TxnID 管理;CompAck;**UC→UD 静默升级**(轴 2)与 `moesi_gpu` 的 `E→M` **同型** —— 4 值 `perm` 无法表达 ⇒ 静默升级声明归 `perm_upgrades.json`(per `ADR-CEP-12 §11`) |
| **CXL 2.0 非对称** | CXL 2.0 | `CxlCacheAdapter` | **✅ 已落地**(队列 #10,2026-11-01 归档;**两段式** —— 规约层 + 实现层) | D2H/H2D 各 3 通道(Req/Rsp/Data)| Host 侧 Snoop Filter;设备不感知其他设备 |
| **CXL 3.0 对称** | CXL 3.0 对称 | `CxlDcohAdapter`(独立载体;**非** `CxlCacheAdapter` 的模式)| **✅ 已落地**(队列 #12,2026-11-02 归档;**两段式** —— 规约层 + 实现层) | + CXL.mem 增 **BISnp(S2M)/ BIRsp(M2S)** 两通道 | **DCOH = Device Coherence(agent / engine)**(设备内的一致性代理,**非** "Ownership History");设备可对 **host** 发起反向 snoop(BI;**不**对 peer device);归属基于 **HPA**(设备内部翻译到 media address,**非** DPA 归属) |
| **TileLink-C** | TileLink-C | `TileLinkCAdapter` + `TileLinkProtocolEngine` | **✅ 已落地**(队列 #11,2026-11-02 归档;**两段式** —— 规约层 + 实现层)· **✅ 运行时引擎已落地**(2026-11-06,per 台账 **Entry #246** —— `TileLinkProtocolEngine` = **B3b 第 2 个实例**,事件语义**零自有副本**地委托 T3 表 `kT3MappingCells`) | A/B/C/D/E 通道 | Tree Tip 权限模型;Probe/Acquire/Grant 家族,无传统 snoop 广播;lookup 可能 ESCALATE_UP |
| **NVLink** | NVLink | `NvlinkAdapter` | **✅ 已落地**(队列 #13,2026-11-03 归档;**两段式** —— 规约层 `cpptlm-cep-nvlink-sharp-state-spec` + 实现层 `cpptlm-cep-nvlink-sharp`;实例 `nvlink_4` · `nvlink-4.0` · `BroadcastNull` tracker) | NVLink TL Read/Write/Atomic/Probe | NVSwitch 内嵌 SHARP/atomic/barrier;排序点可落在 Switch/SPU |
| **UALink** | UALink | `UalinkAdapter` | 🟡 **部分落地**(2026-11-04 —— **两段式**:规约层 `cpptlm-cep-ualink-inc-state-spec` 已归档 + 实现层 `cpptlm-cep-ualink-inc` 已归档 —— `UalinkAdapter`(零自有表副本)+ `FabricCollectivePlane`(数据路径真可达);✅ **`ualink_4` 实例已声明**(2026-11-06 —— 13 键取值**全部裁决**:4 键 per `ADR-CEP-04` SU 2026-11-06 + 9 键 per `ADR-CEP-12` SU 2026-11-06(含订正 #1);取值锚 = 锚点摘要 `C-10`——**4 个互相独立的公开非规范来源** + **一步推断**留档,⛔ 未读任何规范正文)⇒ ✅ **实例级装配已闭合**(2026-11-06 —— per 台账 **Entry #258**:`ualink_4` 挂进拓扑 `configs/cep_ualink_pe_minimal.json`(2 个 `CohIfProtocolEngine` + `cep.bridges[].protocol_instance`)+ collective plane 绑 **`FabricCollectivePlane`**(承载真实 fabric 集合流量)+ 装配闸 `instance_assembly_gate.hh`(`[IAG-01]`~`[IAG-07]`,含反控))⇒ ⚠️ **流语义已闭环(记账级弱档建模声明)**(2026-11-07 —— per 台账 **Entry #269**:`request`/`completion` 由 **`UalinkProtocolEngine`** 专用承载 —— **记账级**(bookkeeping-only):观测面 = **三分法**(记账 2 / 拒绝 7 / pending 1)+ **order-free 在途集合**(`txn_id` 恒等匹配);⛔ **零发射** / ⛔ **零协议状态机** / ⛔ **零 UPLI 通道级语义**(`ReqCmd` / 4 通道 / tag 配对**均不实现**);依据 = **4 个互相独立的公开非规范来源** + **一步推断** ⇒ 证据档位 `public_nonnormative`)⇒ ⛔ **不得**读成「UALink 协议语义已实现」(per `ADR-CEP-12` SU 2026-11-07 ②③ 的**字面禁令**)⇒ ⛔ **不得**读成「协议能力已完整」;**B3b 的 UALink 行维持 ⏭ 受限(仍 0/4)** | Station/Port/Lane;TL Load/Store/Atomic | In-Network Compute(ReadReduce/WriteMulticast/Broadcast/AllReduce) |
| **MOESI/GPU** | `moesi_gpu` | `MoesiGpuAdapter` | **✅ 已落地**(队列 #6,2026-10-27 归档;首个 `status=implemented` 实例) | 6 态 × 6 事件;snoop 型(SoC 内) | ArchForge 贡献(per `32 号` §6.1);**E→M 静默升级**是 `ADR-CEP-13` 域可见写判据 (a)(b) 的**争点所在**(**该争点已裁定 → 状态见 `ADR-CEP-05 §13.1`**) |
| **MESIF** | `mesif` | `MesifAdapter` | **✅ 已落地**(队列 #9,2026-10-30 归档;R-1 **已裁决**(Entry #96 —— 新增第 6 种 SharerTracker 实现 `HierarchicalDirectory` + `AggregationAt::HIERARCHICAL`,per `ADR-CEP-04 §13`)+ 声明权威**已迁到本仓**(per `ADR-CEP-12 §13`);**规约层已闭合 + 已追认**(Entry #101 归档 + Entry #102 追认闭合:ArchForge `42 号` **35 格逐格追认闭合**、**纠正 1 格**(`M+SnpShared`:`S`→`F`)、待追认 **31/35 → 0/35**;⚠ **残余风险**(按档批量 14 格证据强度弱于逐格)见 `docs/implementation/2026-10-29-cep-mesif-state-spec-recognition-checklist.md` §5)· **实现层已落地归档**(2026-10-30,Entry #105 / #106:`cpptlm-cep-mesif-adapter` —— `MesifAdapter`(零自有表副本,委托消费规约载体)+ `MesifProtocolEngine`(真插件 + atomic 最小可用路径 + R-3 写回接线)+ `HierarchicalDirectory` 行为实现;落点见 `.rddf/improvements/cpptlm-cep-mesif-adapter.md` 与 `openspec/specs/cep-mesif-adapter/`) | 分级目录型(跨卡);目录查询 / 定向 snoop;**SharerTracker 选型 = `HierarchicalDirectory`**(`SYSTEM/EXACT/GLOBAL/HIERARCHICAL` —— 多级聚合) | ArchForge 贡献(per `32 号` §6.2);与 `moesi_gpu` **互不承认**(per `32 号` C1);⚠ 声明权威已迁本仓 ⇒ 与 `32 号 §6.2` **分叉**(跨仓追认债) |

> **协议数 6 族 / 8 实例**(✅ **2026-11-06 一致性修复** —— per 台账 Entry #239:新增 `ualink_4` 实例 ⇒ 本计数与 `protocol_instances.json` 的 8 实例**首次相符**;此前本处写 8 而 JSON 为 7);CXL 2.0 与 3.0 对称在**实现阶段**是两个 change;⚠️ **2026-11-02 订正**(per 台账 Entry #167)—— adapter **不共用**:2.0 = `CxlCacheAdapter` · 3.0 对称 = `CxlDcohAdapter`(**独立载体**;per 本文件 `Status Update(2026-11-02)` ③ + 台账 Entry #154)⇒ 原「同一 adapter 的两种模式」的表述**已失效**)
>
> **族计数的口径**(2026-10-22 补记,per 台账 **Entry #12**):`moesi_gpu` / `mesif` 计为**同一来源族**
> (ArchForge 声明族,含 2 套互不承认的一致性架构,per `32 号` C1)→ **+1 族 / +2 实例**,
> 镜像 CXL「1 族 / 2 实例」的先例。两者 `family` 字段均为 `industrial`(per `32 号` 声明)。
> **状态 = 按队列实况(2026-11-02 更新,per 台账 Entry #148 授权)** —— `moesi_gpu` = 队列 #6 **已落地归档**(2026-10-27;首个 `status=implemented` 实例);`mesif` = 队列 #9 **已落地归档**(2026-10-30;**第 3 个** `status=implemented` 实例;R-1 裁决 Entry #96 已落地为 `HierarchicalDirectory` 行为实现;2 项缺口(状态机规约 / 5→4 投影表)**已闭**(Entry #101 归档 + Entry #102 追认)⇒ **已就绪**);`chi_rnf` = 队列 #8 **已落地归档**(2026-10-28;**第 2 个** `status=implemented` 实例);`cxl_2_0` = 队列 #10 **已落地归档**(2026-11-01;**第 4 个** `status=implemented` 实例;⚠️ **两段式** —— 规约层 + 实现层;`sharer_tracker` = 「不参与」);`tilelink_c` = 队列 #11 **已落地归档**(2026-11-02;**第 5 个** `status=implemented` 实例;⚠️ **两段式** —— 规约层 + 实现层;`sharer_tracker` = `TreeManager`);`ualink_4` = **2026-11-06 声明落地**(per 台账 Entry #239 —— 路径 A;13 键取值**全部裁决** + `ADR-CEP-12` 订正 #1)⇒ **`status=implemented` 实例 7 → 8**;⚠️ 但 `sharer_tracker` = 「**不参与**」(无硬件一致性 ⇒ 无 sharer-set 知识需求)。**状态值以 `obj §4.0` 排队总表为唯一权威**(本表**不复制**排期,per 防双真源立场)。
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
| `DcohSymmetric` | LOCAL_MEMORY_REGION | **必须 CONSERVATIVE** | LOCAL_HINT | **HOME** | CXL 3.0 对称(per-device DCOH)|
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

| 能力 | 定义位置 | 实现状态(2026-11-02 实测) | 承接 change |
|------|---------|-------------------------|-----------|
| canonical 10 流契约 | §3.1 | 🟡 **部分落地** —— Stub Cache RN 侧 5 流(`stub_cache_core.hh` 6 接口)已行为化;`MoesiGpuAdapter` 6 事件已行为化;`protocol_instances.json` 的 `flow_support` 10 布尔已填;**载体已落地**(`CohIfBundle` —— `include/bundles/cep_coherence_bundles_tlm.hh`,Entry #84;`ProtocolEngine` 基类 —— `include/tlm/cep/protocol_engine_base.hh`,Entry #85) | 载体 = `cpptlm-cep-cohif-carrier`(队列 #7) |
| Stub Cache Core | §3.2 | ✅ **已落地** —— 6 接口 / 5 态 / `INV-SC1`~`SC6` + `INV-CEP-05-IC1` 行为化(14 测试) | `cpptlm-cep-stub-cache-core`(2026-10-25 已归档) |
| SharerTracker **6** 实现 | §5.2 | ✅ **6 / 6** —— ⚠️ 2026-11-03(队列 #13 实现层)`BroadcastNull` 行为实现落地(`broadcast_null_tracker.hh`,协议无关)⇒ 6 个 tracker **全部**有行为面 —— `FullMapDirectory`(队列 #4,**参考实现**,当前**无实例声明**)+ `HierarchicalDirectory`(队列 #9,多级聚合 + F 态缺口登记)+ `SparseSnoopFilter`(队列 #10 实现层,**CONSERVATIVE** 保守型)+ `TreeManager`(队列 #11 实现层,**树形**:`SUBTREE`/ `FullMapDirectory`(队列 #4,**参考实现**,当前**无实例声明**)+ `HierarchicalDirectory`(队列 #9,多级聚合 + F 态缺口登记)+ `SparseSnoopFilter`(队列 #10 实现层,**CONSERVATIVE** 保守型)+ `TreeManager`(队列 #11 实现层,**树形**:`SUBTREE`/`EXACT`/`LOCAL`/`TREE_PARENT` + `CHAIN` hazard;`ESCALATE_UP` 的**首次真实使用**)+ `DcohSymmetric`(队列 #12 实现层,**保守型**:`LOCAL_MEMORY_REGION`/`CONSERVATIVE`/`LOCAL_HINT`/`HOME` + `STALL` hazard;**独立头** `cxl_dcoh_tracker.hh`)**五个行为实现已落地**;其余 1 个(`BroadcastNull`)**仅声明层 traits**(`sharer_tracker.hh:200` 的 `kTrackerTraits` 数组) | 行为实现落位登记:`DcohSymmetric` ↔ #12 **✅ 已落地**(2026-11-02)· `BroadcastNull` = **无消费者 ⇒ 不排期、保留登记**;⚠️ `TreeManager` 的**行为实现已于 2026-11-02 落地**(队列 #11 实现层,per 台账 Entry #146)⇒ **本行的实现数由 3 / 6 改为 4 / 6 → 5 / 6**(第 5 个 = `DcohSymmetric`,队列 #12 实现层,2026-11-02,per 台账 Entry #166 / **#167**);⚠️ **`FullMapDirectory` = 同样「无消费者 ⇒ 不排期、保留登记」**(2026-11-01 per 台账 Entry #139 裁决 —— **反向缺口**:有行为实现 · 零实例声明;⚠️ 与 R11 的「声明超前于实现」**方向相反**,不因 R11 降级而闭合;**实现不废弃**,未来 CHI-full 队列出现时直接可用);per 台账 Entry #107 / #112 / #129 / #139) |
| Collective Plane | §5.3 | 🟡 **部分落地** —— `CollectivePlaneIf` 恰 7 方法 + `NullCollectivePlane` + `LocalCollectivePlane`(group 生命周期 / region-epoch / BARRIER rendezvous);⚠️ **2026-11-04(队列 #14 实现层)新增 `FabricCollectivePlane`**(承载**真实 fabric 集合流量**):**REDUCE / BROADCAST / MULTICAST 数据路径真可达** + **C-3 控制面与数据面互斥**(in-flight ⇒ 拒绝 register / destroy,`Status::ControlPlaneConflict`);⚠️ `LocalCollectivePlane` 的数据路径**仍**未实现(返 `Status::DataPathNotImplemented`);`fabric_ingress_classify` 路由接线未实现 | 骨架 = `cpptlm-cep-collective-plane-base`(2026-10-26 已归档);数据路径 = `cpptlm-cep-ualink-inc`(2026-11-04 已归档) |
| ProtocolEngine adapter 实例(§5.1.1 工业 8 + §5.1.2 Ruby 8 = 16;§5.1.4 候选语义落地后为 20) | §5.1 | 🟡 **8 个已落地**(⚠️ 2026-11-03 队列 #13 实现层 +1 `NvlinkAdapter`;⚠️ 2026-11-06(per 台账 Entry #239)路径 A +1 `UalinkAdapter`;⚠️ 本格原有「adapter 实例 **5 个**」与「**6 个已落地**」**两个互相矛盾的数字** ⇒ 一并订正为 **7**) —— `MoesiGpuAdapter`(6 态 × 6 事件表驱动 + 6→4 perm 投影 + 6→3 消息类映射;首个 `status=implemented` 实例)· `ChiRnfAdapter`(队列 #8)· `MesifAdapter`(队列 #9,零自有表副本 + 6→3 映射)· `CxlDcohAdapter`(队列 #12,零自有表副本 + 独立规约载体;**6 实例**)· `CxlCacheAdapter`(队列 #10,委托规约载体 + 三态查询面)· `TileLinkCAdapter`(队列 #11,零自有表副本 + 6→3 映射 + 10 行逐消息表)· `NvlinkAdapter`(队列 #13,零自有表副本)· `UalinkAdapter`(队列 #14,零自有表副本;**无硬件一致性** ⇒ 无 snoop 类流);**`ProtocolEngine` 基类已落地**(`ProtocolEngineBase<N>`,Entry #85);**`NullProtocolEngine`(§6.4 + `INV-CEP-07-IC2`)亦已落地**(10 流覆盖矩阵 + 2 条编译期锁,Entry #85)—— **adapter 实例 8 个**(`MoesiGpuAdapter` / `ChiRnfAdapter` / `MesifAdapter` / `CxlDcohAdapter` / `CxlCacheAdapter` / `TileLinkCAdapter` / `NvlinkAdapter` / `UalinkAdapter`);基类落地 ≠ 插件实例落地 | `cpptlm-cep-moesi-gpu-adapter`(2026-10-27 已归档);载体 = 队列 #7 |
| ModuleFactory seam | §6.3 | 🟡 **部分落地** —— 装配期判据闸门已接(`src/core/module_factory.cc:167-186`,`run_channel_gate` + `run_ordering_gate`,FAIL 即中止实例化);**协议桥钩子已从死钩子变活钩子**(`register_protocol_bridge(name, factory_lambda)` + `get_bridges()` 转 public + `ModuleFactory` **Step 7.5** 消费路径,Entry #86);**`REGISTER_CHSTREAM_BIDIRECTIONAL` 宏已落地**(`include/chstream_register.hh`,Entry #87;含 `static_assert(n > 1)` + 运行期验证)⇒ **CEP 类注册已落地**(`CohIfProtocolEngine`,Entry #88)+ **JSON 装配端到端通过**(一次 CohIf 事务**穿过** adapter 抵达对端)⇒ `INV-CEP-07-IC4` **已闭合** | **已完成**(Entry #89):Phase 6 判据收口(`test/test_cep_cohif_carrier.cc` 17 case)+ Phase 7 反向 fixture **登记位置**补齐(`scripts/README.md` §反向 fixture 登记 + `docs/ONBOARDING.md §5.5`)+ **零 diff** 复核通过 |
| 实验输出 4 件套 | §7 | ✅ **已落地** —— `tools/{render_report,run_sweep,ab_compare}.py` + `experimental/metrics.schema.json` + 4 件套契约测试(`test_cep_canonical_invariants.cc` / `test_cep_dummy_signals.cc` / `test_cep_module_factory_seam.cc` / `test_cep_simulation_mode_lock.cc`) | Stage 0(`cpptlm-cep-umbrella`,2026-10-23 已归档) |
| simulation_mode 单一维度 | §6.1 | ✅ 已落地(`06 §5.1` + `01-hybrid §4.7` + `VramControllerTLM::set_mode`)| — |
| Bundle 二分类 | §5.4 | ✅ 已落地(`06 §3.2` 18 个 .hh;**第三类已由 `06 §3.3` 增补**,其唯一实例 `CohIfBundle` **已落地**(2026-10-27 per 台账 Entry #84)—— 见第四则 Status Update ③)| — |
| Python 拓扑生成 | §8.1 沿用 `06 §7` | ✅ 已落地(`cpptlm.library.mesh_cluster/ring_cluster`)| — |
| IOMMU 完整协议 | §8.2 P5 | ⛔ scope reduction **待签发独立 ADR**(per `ADR-CEP-00 §3`)| 待定 |
| Ordering Point 可迁移性判据 | `ADR-CEP-08 §3` | ✅ **已落地** —— J1 / J2 / J3 + `INV-CEP-08-IC2` 行为化(6 测试);**J3 通用形式不可静态判定 ⇒ 多点显式 `SKIP`**;J2 在合法声明上 vacuous PASS | `cpptlm-cep-directory-ordering-plane`(2026-10-25 已归档) |
| 通道依赖图无环检查 | `ADR-CEP-09 §3` | ✅ **已落地** —— 装配期逐 `(network, vc)` verdict + DFS 三色环检测(8 测试) | `cpptlm-cep-directory-ordering-plane`(2026-10-25 已归档) |
| 协议实例能力声明 schema | `ADR-CEP-12 §3` | ✅ **已落地** —— `protocol_instance.schema.json`(M1 冻结集)+ 5 条 AC 判据 + `protocol_instances.json` 声明源(**7** 实例:`moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0` / `tilelink_c` / **`cxl_3_0_symmetric`** / **`nvlink_4`** —— ⚠️ 2026-11-03 队列 #13 实现层 +1);**首个 `implemented` 实例的装配期复核已通过**(⚠️ **2026-11-02 订正**(per 台账 Entry #167):队列 #12 新增第 6 个实例) | `cpptlm-cep-stub-cache-core` / `cpptlm-cep-moesi-gpu-adapter`(均已归档) |
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
> - **🟡 部分落地(5 行)**:canonical 10 流契约(载体未落地)· SharerTracker 5 实现(**1/5**,其余 4 个仅 traits)· Collective Plane(骨架已落地,3 原语数据路径未实现)· ProtocolEngine adapter 实例(**1 个**,且基类本身不存在)· ModuleFactory seam(闸门已接 + **CEP 类注册已落地**,Entry #88 ⇒ `INV-CEP-07-IC4` **已闭合**)· 跨域组合判据(判据已裁定,人工声明未落地)
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

## §附录 · Status Update 2026-10-28(第七则 · per 台账 Entry #87 —— `REGISTER_CHSTREAM_BIDIRECTIONAL` 落地 + `BidirectionalPortAdapter` 泛化)

> **授权**:Entry #87(队列 #7 `cpptlm-cep-cohif-carrier` **Phase 4 实施** —— 装配宏 + 编译期锁 + 反向 fixture;用户 2026-10-28 指令授权)。
> **口径**:本节仅登记 ① §11 活口径的**机械同步** + ② Phase 4 的**落地状态** + ③ **泛化的必要性与边界**。**§2 ~ §5 / §6.1 ~ §6.3 / §7 ~ §10 的定义 / 职责 / 判据零改动**。
> **冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #87"`。

> **① §11「ModuleFactory seam」行的活口径同步**:原写「**`REGISTER_CHSTREAM_BIDIRECTIONAL` 宏仍未开始**(Phase 4)」⇒ 现为「**宏已落地**(`include/chstream_register.hh`,Entry #87;含 `static_assert(n > 1)` 与**运行期**验证:adapter 工厂可查到该类型且端口数一致 + JSON 顶层可实例化该类型)」。⚠️ **该行的主结论不变**:**0 个 CEP 类注册**(`REGISTER_MODULE` / `REGISTER_OBJECT` 对 **CEP 类**零命中)⇒ `INV-CEP-07-IC4` **仍无载体** —— 宏是**能力**,不是**注册**。

> **② Phase 4 落地登记(正文不改)**:交付 `REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)` —— ① **两件都做**(`ModuleFactory::registerObject<T>` + `ChStreamAdapterFactory::registerBidirectionalPortAdapter<T,B,N>`)② **不**照抄 `REGISTER_CHSTREAM_DUAL` 的**半截实现**(那个宏**只**注册 adapter、**不含** `registerObject`,且全仓零调用点 ⇒ 不能作为形态基准)③ 宏内 `static_assert(n > 1)`(判据 CC-04 ③)。

> **③ 必要的前置:`BidirectionalPortAdapter` 的 Bundle 无关化(泛化)** —— 该 adapter 原本**硬编码** `NoCFlitBundle` 语义:读 `flit_category` 决定 `PKT_REQ`/`PKT_RESP`,且 credit 用 `NoCFlitBundle` 构造 ⇒ 承载 `CohIfBundle` 会**编译不过**(`CohIfBundle` 无 `flit_category` / `vc_id`)。⇒ 抽成 `cpptlm::BundleDirectionTraits<BundleT>`:**框架层不认识任何具体 Bundle**,各 Bundle 在**自己的层**提供特化(CEP 侧先例 `include/tlm/cep/cohif_direction_traits.hh`)。⚠️ **未特化的 BundleT ⇒ 编译期报错**(不是静默用错方向)。⇒ **`NoCFlitBundle` 特化逐行为等价于泛化前的硬编码实现**(实测全量回归 **0 变化**)。

> **④ 判据状态**:CC-04 ③ 的「**N ≥ 2**」由**编译期锁**(`static_assert(n > 1)`)保证,且**反向 fixture 实测有牙**:N=1 ⇒ 编译失败且失败原因**精确命中**该断言(若失败原因是别的 ⇒ 判据「因错误的原因红」⇒ 脚本判红);N=2 ⇒ 编译成功(正控)。⇒ 已注册 ctest `cep_chstream_bidirectional_macro_reverse_verify`(3/3)。

> **⑤ 行号漂移登记(本文件正文的行号引用按「ADR 不可变」不改)**:`include/framework/bidirectional_port_adapter.hh` 因 Phase 4 的泛化(新增 trait 块 + 模板参数)整体下移 —— `bind_port_pair` 128 → **171**、`bind_ports`(**空操作 shim**)141 → **184**、`pending_credits_` 105 → **148**、`receive_credit` 调用 184 → **225**、`send_credit` 216 → **257**、`send_credit_impl` 228 → **269**。⚠️ **`CATEGORY_CREDIT` 的写入点位置迁移**:原 `:235`(在 `send_credit_impl` 内)⇒ 现 `:109`(在 `BundleDirectionTraits<NoCFlitBundle>::fill_credit` 内)—— **语义不变**,但「全仓唯一写 `CATEGORY_CREDIT` 的点」的**位置**变了 ⇒ 相关陈述已加**时态锚定**(见 `.rddf/improvements/cpptlm-cep-stub-cache-core.md`)。⚠️ `include/chstream_register.hh` 的既有行号(**50 / 119 / 121 / 175 / 177**)**零漂移**(新宏插在 `REGISTER_CHSTREAM_DUAL` 之后)。

> **⑥ 本则未处理、已登记的项**:① **CEP 类注册**(`REGISTER_CHSTREAM_BIDIRECTIONAL` 的**实际使用者**)⇒ 归队列 #7 **Phase 5**(`INV-CEP-07-IC4` 仍无载体);② `BidirectionalPortAdapter::tick()` 的其余部分仍**假定模块提供 `receive_credit`** ⇒ `ProtocolEngineBase` 已加**默认空实现**(零波及),但**协议引擎的 credit 回收语义**未建模 ⇒ 归 Phase 5。

## §附录 · Status Update 2026-10-28(第八则 · per 台账 Entry #88 —— JSON 装配连通性落地 + `INV-CEP-07-IC4` 闭合 + egress 通道语义登记)

> **授权**:Entry #88(队列 #7 `cpptlm-cep-cohif-carrier` **Phase 5 实施** —— JSON 装配连通性;用户 2026-10-28 指令授权)。
> **口径**:本节仅登记 ① §11 活口径的**机械同步** + ② Phase 5 的**落地状态** + ③ **egress 通道语义**这一新决策面 + ④ 行号漂移。**§2 ~ §5 / §6.1 ~ §6.3 / §7 ~ §10 的定义 / 职责 / 判据零改动**。
> **冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #88"`。

> **① §11「ModuleFactory seam」行的活口径同步**:原写「但 **0 个 CEP 类注册**(`REGISTER_MODULE` / `REGISTER_OBJECT` 对 CEP 类零命中)⇒ `INV-CEP-07-IC4` **仍无载体**(宏是**能力**,不是**注册**)」⇒ 现为「**CEP 类注册已落地**(`CohIfProtocolEngine` = `CohIfNullProtocolEngine` 的非模板可注册外壳,`REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED` 注册,Entry #88)+ **JSON 装配端到端通过**(一次 CohIf 事务**穿过** adapter 抵达对端)⇒ `INV-CEP-07-IC4` **已闭合**」。⚠️ **口径限定**:闭合的是「**CEP 类走既有宏体系注册**」这一条;**协议桥**的**消费**仍由 `config["cep"]["bridges"]` 驱动(配置驱动,非自动挂载)。

> **② Phase 5 落地登记(正文不改)**:task **5.1 ~ 5.4** 全闭合 —— ① **走既有 JSON 端口注册链**(`ChStreamAdapterFactory` → `ModuleFactory` 的 `is_multi` 分支 → `bind_port_pair`),**零** programmatic 端口注入;② `configs/cep_cohif_pe_minimal.json`(2 个 PE,N=2,双向互连 + `cep.bridges` 挂载);③ **端到端**:一次 CohIf 事务**穿过** adapter 抵达对端(判据形态 = 对端 canonical 流**计数器递增** + 发起侧**未**收到自己的事务 + 其它流**未**被误触发);④ **反向 fixture 2 类**(「无连接」⇒ `NotArrived`;「dst 指向不存在的端口索引」⇒ 非 `Arrived`)。⚠️ **判据纪律**:反向 fixture 用 `ReachOutcome` **枚举**区分「装配失败 / 未实例化 / 未抵达 / 已抵达」⇒ 防止「因错误的原因红」(装配被拒也算「红」)。

> **③ 新决策面登记 —— egress 通道(CohIf ⇒ `req_out`)**:Phase 5 发现 egress 走哪条通道**不是**纯实现细节,而是**判据可判别性**的前提 ⇒ 由 Bundle 专属 trait 声明:`cpptlm::BundleDirectionTraits<BundleT>::egress_channel()`(`Resp` 默认 = `RouterTLM` 既有行为;`Req` = `CohIf` 语义)。
> ⚠️ **登记一条实测纠偏**(避免留下错误口径):**两种通道都能把数据送到对端 ingress** —— 框架的 `ChStreamTargetPort::recvReq` 对 `req_in` / `resp_in` **一视同仁**,都投递到 `process_request_input` ⇒ 「走 `resp_out` 会丢数据」的说法**不成立**。⇒ `CohIf` 选 `req_out` 的**真实理由**是:① **方向语义正确** —— `resp_out` 装的是「**对方→我**」那条 JSON 连接的响应通道,egress 借用它会**倒置连接方向**;② **判据有牙** —— egress 走 `req_out` 时「断开我→对方的连接」**确实切得断** egress;实测把 `CohIfBundle` 的 `egress_channel` 改成 `Resp` ⇒ 反向 fixture「dst 指向不存在的端口索引」**假通过**(另一条 JSON 连接仍把 `resp_out` 接到对端)⇒ 该 trait 值已作为**具名判据**钉在 `test/test_cep_cohif_json_assembly.cc`。

> **④ 判据状态**:CC-04 ①②③ **全部有可执行判据 + 反向 fixture**(实测有牙,且**因正确原因**红)。`INV-CEP-07-IC4` 由「无载体」转「**已闭合**」。

> **⑤ 行号漂移登记(本文件正文的行号引用按「ADR 不可变」不改)**:`include/chstream_register.hh` 因 Phase 5 在 **前部**插入「共享注册 helper + 2 个装配宏」且在 `REGISTER_CHSTREAM` 宏体**内部**插入「CEP PE 注册 + 协议桥注册」⇒ `REGISTER_CHSTREAM` 50 → **107**、`registerObject<tlm::RouterTLM>` 62 → **119**、`registerObject<tlm::NICTLM>` 63 → **120**、`registerBidirectionalPortAdapter<tlm::RouterTLM>` 119 → **188**、`registerDualPortAdapter<tlm::NICTLM>` 121 → **190**、`REGISTER_CHSTREAM_DUAL` 175 → **244**、其宏内 `registerDualPortAdapter` 177 → **246**。⚠️ **一处「零漂移」结论已失效**:第七则 ⑤ 登记的「`chstream_register.hh` 既有行号零漂移」是**当时**的口径(Phase 4 把新宏插在 `REGISTER_CHSTREAM_DUAL` **之后**),现已被 Phase 5 推翻。`include/framework/bidirectional_port_adapter.hh` 再 +24/+26/+35(`EgressChannel` 枚举等)。完整对照表见 `06 号` 文末「附录 · 行号对照表(Entry #88)」。

> **⑥ 本则未处理、已登记的项**:① **协议引擎的 credit 回收语义**仍未建模(`ProtocolEngineBase::receive_credit` 是**默认空实现**)⇒ 归 Phase 6;② `ProtocolEngineBase::tick()` 现在会**驱动 adapter**(与 `RouterTLM::tick()` 惯例一致)—— 该行为变更已进入**框架惯例**,但**协议引擎侧**的 tick 语义尚未在 `07 号 §6.4` 展开登记 ⇒ 归 Phase 6;③ Phase 6(测试收口)/ Phase 7(闸与真源同步)未开始。

## §附录 · Status Update 2026-10-28(第九则 · per 台账 Entry #89 —— 判据收口(Phase 6)+ 闸与真源同步(Phase 7))

> **授权**:Entry #89(队列 #7 `cpptlm-cep-cohif-carrier` **Phase 6 + Phase 7 实施** —— 判据收口 + 反向 fixture 登记 + 零 diff 复核;用户 2026-10-28 指令授权)。
> **口径**:本节仅登记 ① §11 活口径的**机械同步** + ② Phase 6/7 的**落地状态**。**§2 ~ §5 / §6.1 ~ §6.3 / §7 ~ §10 的定义 / 职责 / 判据零改动**。
> **冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #89"`。

> **① §11「ModuleFactory seam」行的活口径同步**:末列「队列 #7 Phase 6(测试收口)/ Phase 7(闸与真源同步)」⇒ 现为「**已完成**(Entry #89):Phase 6 判据收口(`test/test_cep_cohif_carrier.cc` 17 case)+ Phase 7 反向 fixture **登记位置**补齐(`scripts/README.md` §反向 fixture 登记 + `docs/ONBOARDING.md §5.5`)+ **零 diff** 复核通过」。⚠️ **该行的主结论不变**:`🟡 **部分落地**`(闸门已接 + 钩子已活 + CEP 类已注册 + 端到端已通;但**跨域组合判据**等其它行仍非本 change 范围)。

> **② Phase 6 落地登记(正文不改)**:① `test/test_cep_cohif_carrier.cc`(新建,**17 case / 52 assertion**)覆盖 **CC-01 ①②③ / CC-02 ①②③ / CC-04 ①②③**;② `test/test_cep_protocol_bridge_contracts.cc`(Phase 3 交付,Phase 6 复核覆盖完整,**14 case / 104 assertion**)覆盖 **CC-03 / CC-05 / CC-06**;③ **CC-06 引用**队列 #6 的 AC-04 实现(`tlm::cep::category_value_domain()`,来自 `include/tlm/cep/adapter_schema_contracts.hh`)⇒ **不重立第二份判据**。
> ⚠️ **编译期锁在 ctest 内可执行**(满足「编译期锁**不得**只以『编译红』充当 ctest 判据」):该文件用 `popen` 调编译器,断言「编译**确实**失败」**且**「失败原因**命中**预期的那条 `static_assert`」⇒ 防「因错误的原因红」。
> ⚠️ **反向 fixture 实测有牙**:临时删掉**头文件 + 编译探针**两处 `is_trivially_copyable` 锁 ⇒ 「CC-02 反向 fixture ④」**报红**;恢复后全绿。

> **③ Phase 7 落地登记(正文不改)**:① **反向 fixture 登记位置补齐** —— 本 change 的 4 个反向 fixture 脚本此前**未登记**在 `scripts/README.md` 守门脚本表与 `docs/ONBOARDING.md §5.5` ⇒ 已补登记,并新增 **`scripts/README.md` §反向 fixture 登记**表(判据 ID · fixture 形态 · **登记位置**,共 **13** 条);② ctest 显式登记 3 条(`cep_cohif_carrier` / `cep_protocol_bridge_contracts` / `cep_json_assembly`);③ **零 diff** 复核:`test/test_cep_canonical_invariants.cc` 零 diff + 全 change 范围**未新增** `INV-CEP-*` 定义 ⇒ **新增 invariant 数 = 0**(`[cep][canonical-invariant]` 13 case / 687 assertion 全绿)。
> ⚠️ **一处口径偏离已诚实登记**:Phase 7 的 task 7.1 字面指向 `scripts/test/cep_decl_reverse_verify.sh`,但该脚本守的是「**协议实例声明 ↔ schema 一致性**」域(`fabric.json` / `protocol_instances.json` / ArchForge 声明),与本 change 的判据域(CohIf 载体 / ProtocolEngine 端口面 / ProtocolBridge / JSON 装配)**零交集** ⇒ 在其中加本 change 的 fixture **等于测另一个闸**(违反「不重发明」)⇒ **未改动该脚本**,改按「登记位置补齐」落地(见 `tasks.md` 7.1 的口径偏离登记)。

> **④ 门禁实测**:9 项 CEP 脚本 + `docs_sync_check --strict` + `openspec validate --changes --strict` 全 rc=0;`cep_gate_selftest` rc=0;`ctest -R cep` **35/35**;全仓 `cpptlm_tests` **1983 case / 873322 assertion** 全绿(原 1966 / 873276 ⇒ **+17 case / +46 assertion**,**0 回归**)。

> **⑤ 本则未处理、已登记的项**:① **协议引擎的 credit 回收语义**仍未建模(`ProtocolEngineBase::receive_credit` 是**默认空实现**)—— `07 号 §6.4` 尚未展开登记该语义 ⇒ 归**后续 change**(不属本 change 范围);② `ProtocolEngineBase::tick()` 驱动 adapter 的行为已进**框架惯例**,其协议引擎侧语义同样待后续 change 登记。

## 附录 · Status Update 2026-10-28(第十则 · per 台账 Entry #93 —— 队列 #8 `cpptlm-cep-chi-rnf-subset` 立项 + §5.1.1 CHI 行同步)

> **授权**:Entry #93(队列 #8 立项 + 转正;Phase 0 治理前置)。
> **口径**:本节仅登记 §5.1.1 **状态列**的机械同步(活口径,per 同款先例)+ CHI 行的**协议事实**补充。**§2 ~ §5 / §6.1 ~ §6.3 / §7 ~ §10 的定义 / 职责 / 判据零改动**。
> **冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #93"`。

> **① §5.1.1 CHI 行的状态列同步**:原写「Stage 1 subset → Stage 2 full」(阶段路线,与本列「按队列实况」的口径不符)⇒ 同步为「🟡 **实施中**(队列 #8;Stage 1 subset → Stage 2 full)」。**状态值以 `obj §4.0` 排队总表为唯一权威**,本表不复制排期(per 防双真源立场);队列 #8 归档后须再同步为「✅ 已落地」。

> **② CHI 行的协议事实补充 —— `UC→UD` 静默升级**:CHI RN-F 在**轴 2**(独占干净 → 脏)上的 `UC→UD` 转换**零总线事务** ⇒ 与 `moesi_gpu` 的 `E→M` **同型** ⇒ 4 值 `perm` 同样无法表达。
> ⚠️ **与台账 Entry #71 的 F-1 区分(勘误,per Entry #91 ⑤)**:F-1 称「CHI RN-F 是**显式**请求升级 ⇒ 不需要该演进」—— 该结论仅适用于**轴 1**(获取独占 S→U,CHI **显式**:ReadUnique / MakeUnique / CleanUnique 走 TXREQ);**轴 2** 上 CHI 是**静默**的。F-1 的**排期结论不受影响**(perm 形态演进的**机制**已由队列 #6 交付,队列 #8 只**填行**)。
> **判据 2 的状态仍以 `ADR-CEP-05 §13.1` 为唯一权威**(本表**不复制**状态值);CHI 的静默升级声明落 `perm_upgrades.json`(本仓自持声明源,per `ADR-CEP-12 §11` 的权威分区)。

> **③ 实例声明的权威分区**:CHI RN-F 是**首个非 ArchForge 贡献**的工业协议实例 ⇒ 其实例声明的**权威归属**机制(镜像域 vs 本仓域 + `repo_owned_instance_ids[]` 登记)见 **`ADR-CEP-12 §11`**;本文件不复述。

> **④ 门禁实测**:11 项 CEP 脚本 + `docs_sync_check --strict` + `openspec validate --changes --strict` 全 rc=0;`ctest -R cep` **35/35**;全仓 `cpptlm_tests` **1983 case / 873322 assertion** 全绿(0 回归)。

---

## 附录 · Status Update 2026-10-28(第十一则 · per 台账 Entry #95 —— 队列 #8 `cpptlm-cep-chi-rnf-subset` 归档)

> **授权**:Entry #95(队列 #8 归档 + 活口径同步)。
> **口径**:本节仅登记 §5.1.1 **CHI 行状态列**的机械同步(活口径,per 同款先例)。**§2 ~ §5 / §6.1 ~ §6.3 / §7 ~ §10 的定义 / 职责 / 判据零改动**。
> **冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #95"`。

> **① §5.1.1 CHI 行的状态列同步**:「🟡 实施中(队列 #8)」⇒「**✅ 已落地**(队列 #8,2026-10-28 归档)」。**状态值以 `obj §4.0` 排队总表为唯一权威**,本表不复制排期(per 防双真源立场)。

> **② 落地内容(CHI RN-F subset,5 项交付面)**:① `include/tlm/cep/chi_rnf_adapter.hh` —— 5 稳定态 × 6 事件 + 6→4 `perm` 投影 + 6→3 消息类映射 4 行;**E1 一手锚** = IHI0050 Issue G §B4.1/§B1.5.2(`UC` 是 Unique 态 ⇒ 唯一副本 ⇒ store 命中**零总线事务** ⇒ `UC→UD` **静默升级**);② `include/tlm/cep/chi_rnf_protocol_engine.hh` —— **真插件**(6 通道观测点 + Credit 记账 + TxnID 管理 + CompAck);③ `protocol_instances.json` 的 `chi_rnf` 实例 + 顶层 `repo_owned_instance_ids[]`(**实例级权威分区**,per `ADR-CEP-12 §11`);④ 判据 CR-01 ~ CR-14 收口(19 case / 344 assertion)+ 反向 fixture 27 条;⑤ 装配实测(`configs/cep_chi_rnf_pe_minimal.json` + 逐 `(network, vc)` verdict)。

> **③ 已知缺口登记(诚实)**:① 「是否携带数据」以 `data != 0` 近似 —— `CohIfBundle` **无** `has_data` 位(冻结面)⇒ 数据恰为 0 的 `CompData` 会被归到 `RXRSP`;② CompAck 无 CohIf 载体(`flow_id` 域只有 5 值)⇒ 通道级记账,非真实发射;③ AC-04 的恒等判定遍历**全 6 类**默认表 ⇒ 只用 4 类的子集 adapter **机械判「非恒等」**(结构上真、语义上误导)⇒ 修归独立 change。

> **④ 门禁实测**:11 项 CEP 脚本 + `docs_sync_check --strict` + `openspec validate --changes --strict` 全 rc=0;`ctest -R cep` **36/36**;全仓 `cpptlm_tests` **2002 case / 873729 assertion** 全绿(0 回归)。

---


## 附录 · Status Update 2026-10-29(第十二则 · per 台账 Entry #96 —— 队列 #9 的 R-1 裁决落地)

> **授权**:Entry #96(R-1 裁决 + 权威迁移落地;architect 裁决 / builder 实施)。
> **口径**:本节仅登记 §5.1.1 **MESIF 行**的活口径同步 + 裁决要点索引。**§2 ~ §5 / §6.1 ~ §6.3 / §7 ~ §10 的定义 / 职责 / 判据零改动**。
> **冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #96"`。

> **① R-1 的裁决(决策正文见 `ADR-CEP-04 §13`,本表不复述)**:SharerTracker 增**第 6 种实现** `HierarchicalDirectory`(`SYSTEM` / `EXACT` / `GLOBAL` / `HIERARCHICAL`)+ `AggregationAt` 增枚举值 `HIERARCHICAL`;`caps` **仍为 4 属性**(层级深度是实现参数,不是能力属性)。**否决**了「扩 caps 为 5 维」与「用 `TreeManager` 降级近似」两条路线(后者会把缺口藏进「近似」二字)。

> **② `mesif` 声明权威的迁移(决策正文见 `ADR-CEP-12 §13`)**:ArchForge `32 号 §6.2` 声明 `mesif` 的 `sharer_tracker.impl = SparseSnoopFilter`,与同文件 `§9.1` 的三级目录**不自洽**,且 `SparseSnoopFilter` 已被 `moesi_gpu` 占用 ⇒ 该 id 的声明权威**迁到本仓**(顶层 `authority_migrations[]` 登记,带理由 + 台账锚 + 原声明快照锚)。**代价已登记**:本仓与 `32 号 §6.2` 的 `mesif` 声明**分叉** ⇒ 跨仓追认债(ArchForge 侧动作)。

> **③ 余 2 项缺口(不在本次裁决范围,另立)**:① ArchForge 侧**无** MESIF 状态机规约(仅 5 态词表 `M/E/S/I/F`);② ArchForge 侧**无** MESIF 的 5→4 `perm` 投影表(仅 `F → WRITEBACK` 单条)。⇒ 队列 #9 **仍不可启动**。

> **④ 门禁实测**:11 项 CEP 脚本 + `docs_sync_check --strict` + `openspec validate --changes --strict` 全 rc=0;全仓 `cpptlm_tests` **2005 case / 873788 assertion** 全绿。

---


## 附录 · Status Update 2026-10-29(第十三则 · per 台账 Entry #101 —— 队列 #9 的**规约层**已闭合)

> **授权**:Entry #101(规约层 change `cpptlm-cep-mesif-state-spec` **归档**;architect/builder 实施)。
> **口径**:本节仅登记 §5.1.1 **MESIF 行**的活口径同步 + 交付物索引。**§2 ~ §5 / §6.1 ~ §6.3 / §7 ~ §10 的定义 / 职责 / 判据零改动**。
> **冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #101"`。

> **① 规约层已闭合**:change `cpptlm-cep-mesif-state-spec` **归档**(「**已完成归档**」语义 ⇒ 2 份 delta spec 合并进 `openspec/specs/cep-mesif-state-spec/` 与 `openspec/specs/cep-chi-rnf-subset/`)。交付:MESIF **5 态 × 6 事件**转换表(30 格)+ **5→4 `perm` 投影表**(5 格)+ **逐格依据**(`kind` / `ref` / `note`)+ **8 条偏离清单** + **2 项语义损失**登记;规范性载体 = `include/tlm/cep/mesif_state_spec.hh`。

> **② 判据口径边界**:判据锁**完备性 / 可审计性 / 待追认标记 / 内部自洽**,**不**锁推导值的**语义正确性** —— 未追认的值**带着 `pending` 标记**留在规约里,**不**被固化为「既定事实」;**唯一例外**是 3 个**锚点格**(`I→NONE` / `E+Wr→M` / `S+Wr→S`),锁定的是**自洽性不变量**(per change 的 design `D-ζ` 锚点格例外段)。

> **③ ⚠️ 跨仓追认债仍在**:**31 / 35 格待追认**(机器可查:`MesifStateSpec::pending_count()`);逐格追认清单 = `docs/implementation/2026-10-29-cep-mesif-state-spec-recognition-checklist.md`(**由载体生成**,含 `--check` 模式);跨仓承接 = `ArchForge:.rddf/improvements/cep-alignment-debt-cleanup.md`。⇒ **队列 #9 的实现层**须等逐格追认(或明确接受本仓推导)后才可启动。

> **④ 门禁实测**:12 项 CEP 脚本 + `docs_sync_check --strict` + `openspec validate --changes --strict` 全 rc=0;`ctest -R cep` 39/39;全仓 `cpptlm_tests` **2011 case / 874139 assertion** 全绿。

---

## 附录 · Status Update 2026-10-30(第十四则 · per 台账 Entry #102 —— MESIF 规约的**跨仓逐格追认闭合**)

> **授权**:Entry #102(收讫 ArchForge 跨仓通告 + 执行 X-1 / X-3;builder 实施)。
> **口径**:本节仅登记 §5.1.1 **MESIF 行**的活口径同步 + 追认结论索引。**§2 ~ §5 / §6.1 ~ §6.3 / §7 ~ §10 的定义 / 职责 / 判据零改动**。
> **冻结面**走 `cep_freeze_check.sh --update --authorized-by "Entry #102"`。

> **① 追认闭合**:ArchForge `42 号`《MESIF 状态机规约逐格追认结论登记表》给出 **35 格逐格结论**(路径 A 混合变体:**20 格逐格裁决 + 15 格按档批量**,豁免 1)⇒ 本仓载体 35 格**全部**转为**一手原文**(`kind = Primary` · `pending = false`)⇒ **待追认 31/35 → 0/35**。

> **② 纠正 1 格**:`M + SnpShared` 的目标态由 `S` 纠正为 **`F`** —— 脏 Owner 写回后**保留 Owner 身份**成为转发点(与 `E+SnpShared→F` 同型)。⚠️ 该纠正暴露本仓载体的一处**自相矛盾**(表头注释并列 `E→F` 与 `M→S`,而 `M→S` 与其**自身** D2 原则「干净 Owner 保留 Owner 身份」不符)⇒ 连带新增**偏离 D5**(8 → **9** 条)。

> **③ 连带后果(已登记为语义损失 ③)**:`M → F` 之后,脏 Owner 的**写回路径**落在 `F`/`M` 同投影的**不可分辨点** ⇒ 「已写回(干净)」与「仍脏」在 4 值 `perm` 接口上**同值** ⇒ 写回事实在 perm 上**不可见** ⇒ 该后果**不可消除**(消除需扩 `perm` 值域 ⇒ 违反 4 值锁)⇒ 实现层须依赖 **Sharer Tracker** 判别「谁持有」。

> **④ 权威迁移条目已删除(X-1)**:ArchForge `32 号 §6.2` 的 `mesif` 声明已对齐本仓口径 ⇒ 两端声明**逐字段全等**(12 个非 `status` 键,实测)⇒ 本仓 `protocol_instances.json` 顶层 `authority_migrations[]` 的 `mesif` 条目**已删除** ⇒ ⑥ 镜像闸对 `mesif` **恢复逐字段校验**(迁移**审计痕迹**保留在 `derived_from.freeze_state` + 台账 Entry #96 / #102)。

> **⑤ 实现层可启动**:队列 #9 的**规约层缺口已完全闭合**(规约 + 追认 + 权威对齐)⇒ **实现层**(`MesifAdapter` / `MesifProtocolEngine`)**可启动**,但 SHALL 以追认清单 §5 的**残余风险**为已知输入。

> **⑥ 门禁实测**:12 项 CEP 脚本 + `docs_sync_check --strict` + `openspec validate --all --strict` 全 rc=0;`ctest -R cep` 全绿;全仓 `cpptlm_tests` 全绿(0 回归)。

---

## 附录 · Status Update 2026-10-30(第十五则 · per 台账 Entry #105 / #106 —— 队列 #9 `cpptlm-cep-mesif-adapter` **实现层**归档)

> **本则覆盖以下几处过时口径**(历史块 append-only,**不回改**):
> · 2026-10-27 第二则(Entry #82)§11 映射表里的 `SharerTracker 5 实现 / 🟡 1 / 5` 与
>   `adapter 实例 / 🟡 1 个` —— 本则在**活跃区**(§5.1.1 / §5.2 / §11)已把数值与措辞对齐,
>   历史块由其自身日期标定,不再代表当前态;
> · 2026-10-29 第十二则(Entry #96)末句「**余 2 项缺口**未闭 ⇒ 队列 #9 仍不可启动」——
>   2 项缺口已闭(Entry #101 归档 + Entry #102 逐格追认),实现层已归档。

### 实现状态变更(活口径)

| 项 | 变更前 | 变更后 | 依据 |
|---|--------|--------|------|
| SharerTracker 实现 | **5** 实现(`1 / 5` 有行为) | **6** 实现(`2 / 6` 有行为) | `ADR-CEP-04 §13`(Entry #96 裁决)+ 第 6 种 `HierarchicalDirectory` 行为实现落地(队列 #9) |
| `mesif` 实例 | `status = planned`(📋 已排期) | **`status = implemented`**(✅ 已落地归档) | `INV-CEP-12-IC3`(status 变更 ⇒ 计数同步);`implemented` 实例 **2 → 3** |
| ProtocolEngine adapter 实例 | **1** 个(`MoesiGpuAdapter`) | **3** 个(`MoesiGpuAdapter` / `ChiRnfAdapter` / `MesifAdapter`) | 队列 #8 已落地 `ChiRnfAdapter`;队列 #9 交付 `MesifAdapter` |
| `mesif` 的静默升级 | 声明源自述「**无**该能力」 | **有**:`UNIQUE → WRITEBACK`(`E + Wr → M`) | `mesif_state_spec.hh` 的 `is_silent_upgrade`;`perm_upgrades.json` 已修订 |

### 队列 #9 的实现层交付(可寻址)

- **载体**:`include/tlm/cep/mesif_adapter.hh`(零自有表副本,委托消费规约载体)·
  `include/tlm/cep/mesif_protocol_engine.hh`(真插件 + atomic 最小可用路径 + R-3 写回接线)·
  `include/tlm/cep/sharer_tracker.hh`(`HierarchicalDirectory`:多级聚合 + `kBehaviorDecisions[5]` + F 态缺口登记)
- **契约**:`openspec/specs/cep-mesif-adapter/` + `openspec/specs/cep-hierarchical-directory/`(归档时由 delta spec 合并)
- **判据**:`test/test_cep_mesif_adapter.cc` / `test_cep_hierarchical_directory.cc` /
  `test_cep_mesif_protocol_engine.cc` / `test_cep_mesif_instance_decl.cc` +
  `scripts/test/cep_mesif_adapter_reverse_verify.sh`(16 条毒化,「因正确原因红」机器判定)
- **残余风险**(不得写成「已消除」):R-1(按档批量 14 格证据强度弱于逐格)· R-3(写回事实在
  4 值 `perm` 上不可分辨 ⇒ 由 `LocalView.dirty` 掩码 + 写回接线补偿)· 残留 A(`HierarchicalDirectory`
  行为语义无真源 ⇒ 逐条 `basis` + `pending` + 跨仓追认债)· 残留 C(`atomic` 的 `consistency_intent`
  无独立字段 ⇒ 显式登记)· 引擎建模决策 E-1 ~ E-6(`design D-λ`)

### 后续义务

- **S1-3 已触发**(口径 = `implemented` 实例 ≥ 3)⇒ 真实义务 = `ADR-CEP-12 §8.4` 的
  **SLICC 式「协议本地虚拟网络 + 装配期绑定」备选重评**(结论将入后续台账条目);
- 「承接项 2」(per-network 嵌套形态)**滚动复核第 2 次**已落。


---

## 附录 · Status Update 2026-10-30(第十六则 · per 台账 Entry #106 —— **S1-3 触发后的真实义务**结论)

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

## Status Update(2026-11-02,per 台账 Entry #148)—— §5.1.1 / §11 的**状态列机械同步**(活口径)+ 声明源实例数订正

> **正文改动限于「状态 / 计数」机械同步**(§5.1.1 表的**状态列** + §11 两处计数 + 一处锚点);
> **族 / 实例口径表体零改动** ⇒ §5.1.1 仍为 **8 行**(TileLink-C 行**本就存在**)⇒
> `test/test_cep_schema.cc` 的 `rows511 == 8` **保持不变**。
> ⚠️ **冻结面两步授权**:授权记录 = 台账 **Entry #148**(`--update --authorized-by "Entry #148"`)。

### ① §5.1.1 协议族表的**状态列**陈旧(3 行)—— 机械同步(活口径)

| 行 | 原状态列 | 现行状态列 | 依据 |
|---|---|---|---|
| **CXL 2.0 非对称** | `Stage 1` | **✅ 已落地**(队列 #10,2026-11-01 归档;**两段式**) | 台账 Entry #127 / #132 |
| **TileLink-C** | `Stage 1` | **✅ 已落地**(队列 #11,2026-11-02 归档;**两段式**) | 台账 Entry #143 / #147 |
| **MESIF** | **📋 已排期** | **✅ 已落地**(队列 #9,2026-10-30 归档) | 台账 Entry #105 / #106 |

> ⚠️ **MESIF 行原状态列与同行后半段自相矛盾** —— 状态标签写「📋 已排期」,而同一格后半段写
> 「**实现层已落地归档**(2026-10-30,Entry #105 / #106)」⇒ 本则消解该互斥。
> ⚠️ **状态列一律以 `obj §4.0` 排队总表为唯一权威**(per 本表既有立场)—— 本则**不复制**季度值。

### ② §5.1.1 回写注:补 `cxl_2_0` / `tilelink_c` 两行(原只回写 3 行,落后两代)

- `cxl_2_0` = 队列 #10 **已落地归档**(2026-11-01;**第 4 个** `status=implemented` 实例;两段式;`sharer_tracker` = 「不参与」);
- `tilelink_c` = 队列 #11 **已落地归档**(2026-11-02;**第 5 个** `status=implemented` 实例;两段式;`sharer_tracker` = `TreeManager`)。
- ⇒ **`implemented` 实例数 = 5**;⚠️ **与 §5.1.1 的「6 族 / 8 实例」**不是同一口径**(后者是**族内模式行**数)。

### ③ §11 两处计数 + 一处锚点漂移

| # | 位置 | 原 | 现行 | 依据 |
|---|---|---|---|---|
| 1 | §11 ProtocolEngine adapter 实例行 | 🟡 **3 个已落地**(`MoesiGpuAdapter` / `ChiRnfAdapter` / `MesifAdapter`) | 🟡 **5 个已落地**(+ `CxlCacheAdapter` / `TileLinkCAdapter`) | 队列 #10 / #11 |
| 2 | §11 协议实例能力声明 schema 行 | `protocol_instances.json` 声明源(**2 实例**) | 声明源(**5** 实例) | 同上 |
| 3 | §11 SharerTracker 行 | 仅声明层 traits(`sharer_tracker.hh:188`) | `sharer_tracker.hh:200` 的 `kTrackerTraits` 数组 | **锚点漂移修正**(:188 指向 `struct TrackerTraits`,traits **数组**在 :200) |
| 4 | §11 能力表头 | 实现状态(2026-10-27 实测) | 实现状态(2026-11-02 实测) | — |

### ④ ⚠️ 已知残留(**本则不处理**,仅登记)

- **§5.1.1 表体行数 = 8 不变** ⇒ 台账 Entry #143 ⑥ item 3 预测的「`== 8` ⇒ 9」**不成立**
  (TileLink-C 行**本就存在**,只是**状态列**陈旧)⇒ `test/test_cep_schema.cc` 的
  `REQUIRE(rows511 == 8)` **保持不变**;⚠️ 该预测偏差由台账 **Entry #148** 澄清。
- **§11 本则**只改「状态 / 计数」;**SharerTracker 6 实现的清单本身不变**(4 行为实现已落地),
  `FullMapDirectory` 的**反向缺口**按 Entry #139 ③ **保持开放**(登记态)。

### ⑤ 后果 / 代价

| 面 | 后果 |
|---|---|
| **本文件** | **状态列 + 2 处计数 + 1 处锚点 + 回写注**;§5.1.1 表体与「6 族 / 8 实例」计数**不变** |
| **测试面** | `test/test_cep_schema.cc` 的 `rows511 == 8` **零改动**(断言本就成立) |
| **下游** | `protocol_instances.json` 已是 **5** 实例 ⇒ 本则只是把**真源 1** 的**落后计数**追平 |

**关联**: 台账 `obj-cep-platform.md` **Entry #148**(授权记录)· #147(队列 #11 归档)· #132(队列 #10 归档)
· #143(队列 #11 规约层归档)· #139(`FullMapDirectory` 落位裁决)· #137(R11 降级)
· `ADR-CEP-04` 的 `## Status Update(2026-11-01)` · `ADR-CEP-12 §11`(实例级权威分区)
· `openspec/specs/cep-sharer-tracker-base/spec.md`(同批追加段)

---

## Status Update(2026-11-02,per 台账 Entry #154)—— §5.1.1 / §5.4 / §11 的 **5 处口径订正** + **对称模式的 adapter 形态裁决**

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

## Status Update(2026-11-06 · 三,per 台账 Entry #246)—— TileLink-C **运行时引擎落地**(B3b 达成度 **1/6 → 2/6**)

**主题**:change `cpptlm-cep-tilelink-runtime-engine`(台账 Entry #243 立项 / **Entry #246** 实施回执)落地 —— TileLink-C 成为 **B3b 第 2 个运行时引擎实例**(第 1 个 = CHI RN-F)。
**性质**:⛔ 本 SU **不改** §5.1.1 的**行数**(仅**行内**追加引擎落地标注)⇒ 跨仓对齐锚点行号**不变**;⛔ 不新增 invariant 编号;⛔ 冻结面 `protocol_instance.schema.json` / `protocol_instances.json` / `perm_upgrades.json` **零改动**。

### ① 交付面

| 面 | 内容 |
|---|---|
| **引擎载体** | `include/tlm/cep/tilelink_protocol_engine.hh`(header-only,`ProtocolEngineBase<2>` 派生)—— **CohIf 意图面 → 权限转移事件面**的**事件计数/上报器** |
| **事件真源** | **零自有副本** —— 事件语义一律经唯一消费载具 `consumable_mask_of(PlatformIntent)` 委托 T3 表 `kT3MappingCells`(per `tilelink_state_spec.hh`) |
| **注册面** | `chstream_register.hh` 注册第 4 个引擎名 `TileLinkProtocolEngine`(N=2 ⇒ JSON 侧可逐实例选择;⛔ 引擎**不解析 JSON**) |
| **装配面** | `configs/cep_tilelink_pe_minimal.json`(2 × `TileLinkProtocolEngine` + `cohif_noc` 桥绑定 `tilelink_c` 实例) |
| **契约面** | `test/test_cep_tilelink_engine.cc`(7 cases · 101 assertions,标签 `[cep][tilelink][engine]`)+ delta spec `TLE-01`~`TLE-08` |
| **构建期取证** | `scripts/test/cep_tilelink_engine_failclosed.sh`(影子树三态:clean / pending / deleted)+ `cep_tilelink_engine_probe.cc` |

### ② 判据 4 件套(证据)

| 判据 | 证据 |
|---|---|
| **正控** | `[cep][tilelink][engine]` 7 cases / 101 assertions 绿;`[cep]` 全量 441 cases / 10337 assertions 绿;`ctest` 全量绿 |
| **反控** | 4 条反向 fixture(`Pending` 声明事件 / `Pending` 可消费掩码归零 / `row_ref` 重复 / 掩码越界)+ Null 引擎对照(同一 5 流:真插件 **4 事件** vs Null **零事件面**)+ JSON 装配反控(换 Null ⇒ 类型判据判否) |
| **决定性变异** | 影子树删 `TLC-M-02` 行 ⇒ 编译红且**签名命中** T3 编译期锁(`static_assert` 文案含 `T3:`);真源 sha256 **未变** ⇒ 取证后无需复原 |
| **fail-closed(消费面)** | 影子树把该行改 `Pending` ⇒ **引擎事件发射出口计数 = 0**(⛔ 非掩码函数返回值 —— 那是 state_spec 层重复) |

### ③ 认识论边界(⛔ 不得被误读)

- **引擎 ≠ 协议实现**:引擎只做「意图 → 事件类别」的**计数/上报**;⛔ **不**建 PE 本地树状态机(Nothing/Branch/Tip)—— 12 格矩阵是**规约层**真源,运行面**只读**。
- **不携带源态**:TileLink `Cap`「源不定」(D1 §8.1.4)⇒ 引擎只声明**事件类别**,⛔ 不预设 from-state(per `TLE-05` 的 grep 级判据)。
- **不连带推进**:CXL 2.0(冻结)/ CXL 3.0 / NVLink / UALink 四族状态**不变**(⏭ / 未触发,per Entry #238 的逐族登记)。
- **B3b 达成度**:1/6 → **2/6**;⛔ **不得**改写为「B3b 已达成」—— 终态 DoD Go 的剩余前置 = 其余 4 族。

### ④ Verification(前瞻判据 · 可勾选)

- [x] 引擎 10 个 `on_*` 全覆盖 + 11 意图参数化断言(7 零事件意图 **basis == Direct** 双断言)
- [x] **零自有事件表副本**:禁止性 lint(0 处 `kT3MappingCells` / `event_mask` 字段 / 掩码成员数组)+ 唯一载具计数判据
- [x] L4 事务级贯通:5 流 `dispatch_cohif` ⇒ Grow × 1 + Prune × 3;⛔ `request` 自身**零事件**(反直觉点单独钉死)
- [x] JSON 装配成功 + 类型判别(正控/反控)+ 注册端口数 = 2
- [x] fail-closed 消费面零发射 + 决定性变异签名命中(构建期影子树取证,真源零改动)

**关联**:台账 **Entry #246**(实施回执 + 冻结面授权)· **#245**(F2 漏勾回填)· **#244**(T3 归档)· **#243**(立项 + metis 4 BLOCKER 全修)· **#242**(T3 实施)·
`openspec/changes/cpptlm-cep-tilelink-runtime-engine/` · `include/tlm/cep/tilelink_protocol_engine.hh` · `include/tlm/cep/tilelink_state_spec.hh` ·
`configs/cep_tilelink_pe_minimal.json` · `scripts/test/cep_tilelink_engine_failclosed.sh` · `test/test_cep_tilelink_engine.cc`

---

## Status Update(2026-11-06 · 四,per 台账 Entry #249)—— 「转移表真源」的**来源效力**与 **B3b 计入口径**

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

## Status Update(2026-11-07 · 六,per 台账 Entry #264)—— SU · 四 §④ **选项戊已裁决** + **终态 DoD Go 挂起**(口径 ②)

> **性质**:末尾追加 SU(append-only)。⛔ 不改 §5.1.1 行数;⛔ 不回改 SU · 四 §④ 的五选项表格(⛔ 只登记其**裁决结果**);⛔ 不改任何 invariant 编号。
> **触发**:台账 **Entry #251**(选项戊裁决)· **#263**(剩余任务盘点)· 用户授权「按 oracle 建议」+ oracle 对三候选口径的建议。
> **审查**:`oracle`(终态裁决口径建议)⇒ **建议口径 ②**;`critic`(P2/P3 补复核 + 契约对齐复审,见台账 Entry #261/#262)。

### ① §④ 五选项的裁决结果(登记,不回改表格)

**选项戊已由用户裁决(2026-11-06,per 台账 Entry #251)** ⇒ DoD 条目拆分:`B3b-1`(可读规范族运行时引擎)= **2/2 ✅** + `B3b-2`(受限族)= **0/4 ⏭**(含触发条件),两条**各自独立可判**。

### ② 终态 DoD Go 的裁决(口径 ②:挂起)

**判定句**:**「B3b-2 终态裁决 = 0/4 ⏭ 未达成(维持),终态 DoD Go 挂起;解除条件 = 任一受限族经 D-4 升级路径满足 T1–T4 并交付引擎 + L4 E2E(重新普查后改判),或用户另行裁决 DoD 结构。」**

- ⚠️ **挂起(pending)≠ 拒绝**:Stage 1~3 验收窗口为 **2030-Q1**,当前属中期判定(与 `obj` §5.2 的「当时的合法中间态」同型);
- ⚠️ **与 B2「N ≥ 1」判例对齐**:B2 明禁「全 ⏭ 登记为达成」(`cep-b2-unmodelled-verdict.md` §127)⇒ **B3b-2 = 0/4(全 ⏭)** **不得**登记为达成 ⇒ 同一 objective 内不得双标;
- ⛔ **排除口径 ①**(`⏭ ⇒ Go`):即 SU · 四 §④ **选项丁**的单条目复刻 —— 撞 B2 判例 + 选项丁否决理由 + §5.1.1 防自欺条款(三重阻挡);若将来要采用,**缺一不可**的前置 = ① 4 族 deferral 的**机器可查登记** ② 「条目未达成」与「Go 判定规则」**两层文本分离** ③ architect 追加 SU 说明改的是**判定规则** ④ 触发条件**逐族可解析**到真实台账 Entry;
- ⛔ **排除口径 ③**(降为非阻塞项):三种实现形态分别撞「分母 6→2 收缩(违反 B2 先例)」「退化为口径 ①」「目标文本自身降格」⇒ **不自洽**;且需先由用户修订 Entry #251 的戊 ⇒ 非本轮可执行项。

### ③ 计数与语义的边界(⛔ 不得混读)

- **DoD 语境**用 `B3b-1` / `B3b-2`(各自独立可判);**族级审计语境**用 SU · 四 **D-3** 三元组(`2 达成 / 分母 6 / {1 显式冻结, 3 受限-⏭}`)⇒ 两套记账**不得互相覆盖**;
- ⛔ 不得写「B3b-2 ✅(⏭)」形态(⛔ 单一状态 token);⛔ 不得把 `B3b-1` 的 ✅ 扩写为「B3b 整体达成」。

### ④ 已登记的强度缺口(⛔ 本条**不**假装闭合)

4 族的 ⏭ 目前**只有台账文字登记**,弱于 B2 的机器校验 ⇒ **登记**该缺口,**不假装等价**(per SU · 四 §⑤ 的同款表述)⇒ 补齐动作(4 族 deferral 的机器可查登记 + 闸校验)与 `cxl_dcoh` / `nvlink_sharp` 的 `RefKind` 迁移**同轮实施**(per 台账 Entry #263 ② 的 improvement)。

### ⑤ Verification(可勾选验收判据)

- [ ] `obj §6.2` 的 B3b-2 行含**终态裁决**子 bullet(挂起语义 + 解除条件指针 + B2「N ≥ 1」对齐句)
- [ ] `obj` 的终态行标注「⏳ 未发生(挂起)」且指向本 Entry
- [ ] 4 族 ⏭ 的**机器可查登记**已立项(与 `RefKind` 迁移同轮)⇒ 本条登记该缺口而**不**宣称已闭合
- [ ] 冻结面两步授权已执行(`cep_freeze_check.sh --update --authorized-by "Entry #264"` → 清单单独 commit)
