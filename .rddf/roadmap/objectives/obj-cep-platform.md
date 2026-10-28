---
id: objective-cep-platform
status: active
created: 2026-10-20
last_revised: 2026-10-25
review_by: 2027-01-18
owner: rdd-planner
priority: P1
manual_deps:
  - phase-10
theme: Coherence Exploration Platform(CEP)— 跨 14 季度协议中立一致性互联仿真平台治理
kind: objective
phase_refs:
  - phase-10
---

# objective-cep-platform: CEP 跨 14 季度治理叙事

> **写入权**: rdd-planner(per `rdd-planner/SKILL.md:156` + ADR-0028 + ADR-0054 D5 唯一写入方)
> **schema**: `_lib/schemas/objective_schema.json` v1(9 必填字段齐;`kind` / `phase_refs` 为扩展字段)
> **关联**: `.rddf/roadmap/phases/phase-10.md`(CEP 实施 phase)+ `.rddf/improvements/cpptlm-cep-umbrella.md`(5-segment 草稿)
> **关联真源**: **21 份** ADR `docs/adr/ADR-CEP-*.md`(per `ADR-CEP-NUMBER-MAP §2 Decision 1`;基座 00/00A/01~07 + NUMBER-MAP,实例/排序/版本/契约/演进 08/09/11~18/07A)
> **关联变更**: `cpptlm-cep-umbrella`(Stage 0)+ Stage 1~3 **13** 个串行 change(2026-Q4 ~ **2030-Q1**)

---

## §0 Goal

> **⚠️ architect 阶段同步(2026-10-21,两批)**:invariant 计数已同步为 **67 编号位 / 66 实际 / 15 组**(第一批新增 `ADR-CEP-08`/`09`/`12`/`13` + `ADR-CEP-07-IC7` + `ADR-CEP-06-IC5`;第二批新增 `ADR-CEP-11`/`14`/`15`/`16`/`07A`)。**阶段重排、DoD 7 分解(10 → 15 组)、change 清单**等非机械部分已由 planner 阶段完成。计数单一真源 = `docs/architecture/07-coherence-exploration-platform.md §4`。
> **⚠️ planner 阶段同步(2026-10-23,per Entry #22)**:①**队列口径** —— `ADR-CEP-00 §2 Decision 3` 为 **11 个 change / 串行 11 季度**(2026-10-22 per Entry #12 由 10 → 11),终态验收顺延至 **2029-Q2**;②**Stage 0 实施态** —— umbrella **已完成归档**(2026-10-23;tasks **99/99** 全勾,delta spec 已合并进 `openspec/specs/cep-core-contracts/spec.md`;R10 已按 **§9.3** 闭合判据处置,per 台账 Entry #28/#29/#30);③**ADR-CEP 族两轮精简已完成**(契约面零改动,per Entry #20/#21);④**Stage 1 准入前置 S1-1 ~ S1-7 已在 §4.0 排队表可寻址**(逐项 owner → 队列 #2 / #4 / #6)。计数单一真源 = `07 号 §4`;change / 季度单一真源 = `ADR-CEP-00 §2 Decision 3`。

跨 14 季度(2026-Q4 ~ 2030-Q1)推进 Coherence Exploration Platform(CEP)从"协议中立基线"(06 §1.1 v2.1.10 已落地 60-70%)走向"5 协议族(CHI / CXL 2.0+3.0 对称 / TileLink-C / NVLink / UALink;**族 / 实例计数口径见 `07 号 §5.1.1`「6 族 / 8 实例」**)可插拔 + 实验产物可比较"的完整探索平台,期间守 14 个 change 串行 KPI 排队 + **66 条守门 invariant**(编号位 67,per `ADR-CEP-06/07` + oracle v0.4 B5 命名空间统一)。

**完成定义 (DoD, 2030-Q1 终)**:

1. ✅ **14** 个 OpenSpec change 全部 archived with sync
2. ✅ Stage 0 umbrella change `cpptlm-cep-umbrella` 落地(2026-Q4)
3. ✅ Stage 1 实施完成(Stub Cache + Sharer Tracker + Collective Plane + 3 协议 RN-F subset)
4. ✅ Stage 2 实施完成(CXL 对称 DCOH + NVLink SHARP)
5. ✅ Stage 3 实施完成(UALink In-Network Compute)
6. ✅ 实验输出 4 件套契约(`simulation_config.json` + `trace.h5` + `metrics.json` + `report.html`)per `ADR-CEP-06`
7. ✅ **66 条守门 invariant 机器检查 100% 覆盖**(编号位 67,`INV-CEP-04-IC4` **唯一跳空**)—— **按 15 组分解验收**:

| # | 组 | 条数 | 机器检查落点 |
|---|----|:----:|------------|
| 1 | Coherence Interface | 11 | `test/test_cep_canonical_invariants.cc`(CEN-Ib1~11) |
| 2 | Stub Cache | 7 | 同上(INV-SC1~6 + `INV-CEP-05-IC1`) |
| 3 | Sharer Tracker | 6 | 同上(`INV-CEP-04-IC1/2/3/5/6/7`) |
| 4 | Experiment Protocol | 5 | 同上 + `test/test_cep_simulation_mode_lock.cc`(4 件套契约) |
| 5 | Simulation + Seam | 7 | `test/test_cep_module_factory_seam.cc` + `test/test_cep_simulation_mode_lock.cc` |
| 6 | Numbering | 3 | `scripts/test/cep_docs_sync_check.sh` |
| 7 | Ordering Point | 3 | `test/test_cep_canonical_invariants.cc`(契约层)+ Stage 1 实现 |
| 8 | Multi-VC / Deadlock Freedom | 3 | 同上(契约层)+ Stage 1 装配期无环检查器 |
| 9 | Capability Map | 3 | 同上 + `include/tlm/cep/protocol_instance.schema.json` |
| 10 | Coherence Domain | 3 | 同上(契约层)+ Stage 2 实现 |
| 11 | Protocol Version Locking | 3 | 同上(契约层)+ 装配期版本校验(Stage 1) |
| 12 | Stub Contract Tests | 3 | 同上(契约层)+ C1~C10 落地(Stage 0~1) |
| 13 | CacheTLM Alignment | 3 | 同上(契约层)+ 冻结面 0 diff 守门(Stage 0) |
| 14 | 06 Evolution Mapping | 3 | `scripts/test/cep_docs_sync_check.sh`(P 编号口径) |
| 15 | 06-bis Artifact Disposition | 3 | 同上(协议泄漏通用禁令扫描) |

> **落点口径(per Entry #22)**:本表「机器检查落点」= **口径校验**所在位置;**66 条 invariant 的逐条断言统一落 `test/test_cep_canonical_invariants.cc`**(组 6/14/15 另有脚本层口径校验,**不替代**逐条断言)。15 组分解本身**不复述** —— 真源在 `07 号 §4`。

8. ✅ ModuleFactory 集成 0 发明宏形式(per oracle v0.4 B7)
9. ✅ simulation_mode 单一维度(functional / timing),不引入 network.mode
10. ✅ **§9 研究问题清单 8 个假设全部有实验结论**(per §9.1)
11. ✅ §11 决策台账记录所有 KPI 让位决策 + Go 节点地址 + Sprint 复盘
12. ✅ §8 Kill / Replan 判据未触发(或已按判据处置)

> **验收层级委托**(per oracle S1 修订 / 2026-10-23 指针修正):本 DoD 是**终态判定**。
> - **逐条 assertion 的执行真源** = `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/proposal.md` §Acceptance(**实测值在此回填**);
> - **上游规划输入** = `.rddf/improvements/cpptlm-cep-umbrella.md` §Acceptance **A1 ~ A7**(A7 = **前置 gate 汇总**,不是 assertion);
> - Stage 1~3 各 change 的条款落**各自** OpenSpec change 的 AC。
>
> 本文件**不重复**逐条 assertion,避免三处复述漂移。

---

## §1 Background

### 1.1 现状评估(per `phase-10.md` §关键依赖)

- **06 §1.1 v2.1.10**:已落地 L1-L6 仿真层层模型(60-70% 协议中立)
- **06 §15.4 P2.0-P5**:Coherence 演进 seam,但**无 coherence 协议中立抽象层**
- **06-bis §7.3-§7.7**:CoherenceBridge / CoherenceProtocol / SnoopFilter 等接口草案(90% markdown,无 C++ 实现)
- **dGPU Phase 2/3**:协议特化层(phase-9,active),与 CEP 是**不同并列关系**,不接管

### 1.2 核心矛盾(per oracle + metis G1)

phase-1~9 完成(2026-04 ~ 2027-02)后,**协议中立基线缺位**:
- 协议中立层(06 §1.1)已落地 60-70%
- 但**复用 06-bis 范围但实质 0 复用**(06-bis 90% 是 markdown 草案,几乎所有 CEP 契约都"本 ADR 首次定义")
- 探索平台无实验产物(metric / sweep / A-B 对比 — metis G1 盲点)
- 真实 IP 对应实证缺口(NVIDIA H100 GMMU / ARM CMN-700 / UALink Common 2.0)

### 1.3 立项依据(如实陈述,per oracle B5 修订)

本 objective 由 planner 于 2026-10-20 **新建**,触发判断依据:
- `rdd-planner/SKILL.md:150` 决策表:"跨 sprint 目标叙事 + 复盘台账 + deferral rationale → objective"
- CEP 三项全命中:跨 14 季度叙事 / §11 append-only 台账刚需(KPI 并发 ≤1 + 季度让位可追溯)/ Stage 晋级复盘仪式

> **说明**:本 objective **不**基于任何"预埋触发条件"文本。`.rddf/roadmap.md` 的 Features / Objectives 占位段由 planner 本回合同步改写,不存在 CEP 之前的历史预埋声明。

---

## §2 范围

### 2.1 目标愿景

把"用 CHI / CXL / UALink 的这个 SoC,coherence 开销 / 停顿 / 死锁风险如何"这类**跨协议取舍问题**,从"当前靠人脑推演"变成"可跑实验出数据"。

### 2.2 In-Scope(per `ADR-CEP-00 §3` + `ADR-CEP-07 §2`)

- **6 层架构骨架**:Core / Traffic Generator / Stub Cache / Coherence Interface / Protocol Engine / Directory & Ordering Plane / Fabric Collective Plane / Fabric & Transport Plane
- **canonical 10 条 Coherence Interface 流清单**(per `ADR-CEP-02 §2`):request / completion / snoop / snoop_resp / writeback / forward_data / back_invalidate / address_resolve / barrier / snoop_completion_ack
- **StubCacheCore 6 接口 + 6 不变量**:net-side 5 (`request` / `snoop` / `snoop_resp` / `writeback` / `completion`)+ core-side 1 (`evict_line`)
- **SharerTracker 5 实现 + caps descriptor + D-5 重构**:`FullMapDirectory` / `SparseSnoopFilter` / `DcohSymmetric` / `TreeManager` / `BroadcastNull`
- **FabricCollective Plane 旁路平面 + 5 条防误分类规则(R1-R5)**
- **NullProtocolEngine / NullCollectivePlane**:10 流覆盖矩阵(per oracle v0.4 B6)
- **Experiment Output Protocol**:4 件套 + 13 必纳 metric + 4 可选 metric + 一致性 fail 闸
- **ModuleFactory 集成 seam**:真实注册机制(`ModuleFactory::registerObject<T>` + `REGISTER_MODULE(T)`),**不发明宏形式**
- **simulation_mode 单一维度**:functional / timing,真源在 `06 §5.1` + `01-hybrid §4.7`(per oracle v0.4 B3-5)

### 2.3 Out-of-Scope

- ❌ **完整缓存核心微架构** — Stage 0 仅 Stub Cache 接口;不做完整 tag/data SRAM / 复杂预取器 / 精确功耗模型
- ❌ **真实 RTL / FPGA 加速** — 推 ArchForge 仓
- ❌ **完整 UsrLinuxEmu driver 集成** — 不在 CEP scope
- ❌ **IOMMU 完整协议** — Stage 5 范围 reduction 待签发独立 ADR(per `ADR-CEP-00 §3` supersede 表 P5 行 scope reduction 声明)
- ❌ **Multi-VC 路由器全仲裁** — Stage 0 仅 `vc_id` 字段 0-3 沿用;多 VC 仲裁推迟到 CEP-09(per `ADR-CEP-NUMBER-MAP §2 Decision 1`)
- ❌ **6 协议平行** — Stage 0~3 串行排队(per `ADR-CEP-00 §2 Decision 3` 14 季度)

---

## §3 架构依据(引用 ADR,per roadmap/SKILL.md:73 §3 语义)

| 依据 | ADR | 决策要点 |
|------|-----|---------|
| 归口 + 边界豁免 | `ADR-CEP-00` §2 Decision 1/3/4 | 代码全归 CppTLM;单 DRI;AGENTS.md:423 不触发 |
| 6 层 + 完成域 | `ADR-CEP-01` §2 Decision 1/2 | Fabric Collective 旁路平面;requester-scoped vs group-scoped |
| canonical 10 流 | `ADR-CEP-02` §2 | 10 条流 + **CEN-Ib1~11** 不变量(11 条,per Entry #19 §3 交叉引用) |
| SHARP 内部 | `ADR-CEP-03` §2 + §3 | CollectivePlaneIf;R1-R5 防误分类;CEP-13 non-cacheable 语义 |
| D-5 重构 | `ADR-CEP-04` §2 + §3 | caps descriptor 替代 bool;LookupResult 枚举;snoop_begin/fold |
| Stub Cache | `ADR-CEP-05` §2 + §3 | RN 投影 6 接口;折叠映射表;10 条契约测试 |
| 实验输出 | `ADR-CEP-06` §2 | 4 件套 + 13 必纳 metric;INV-CEP-06-IC1~4 |
| 仿真 + seam | `ADR-CEP-07` §2 + §3 | simulation_mode 单一维度;ModuleFactory 真实注册;INV-CEP-07-IC1~6 |
| 编号 + 真源锚点 | `ADR-CEP-NUMBER-MAP` §2 + §3 | CEP-08~16 预分配;INV-CEP-NUM-IC1~3 |
| 真源章节映射 | `ADR-CEP-00A` §3 + §4 | 章节行号实证;14 条伪引清单 |

> **完整 21 份 ADR 清单**: `docs/adr/ADR-CEP-*.md`(per `ADR-CEP-NUMBER-MAP §2 Decision 1`)。本表仅列**基座**决策依据;实例 / 排序 / 版本 / 契约 / 演进映射类(08/09/11~16/17/18/07A)见 `phase-10.md` §真源 3 与 `07 号 §4`。

---

## §4 Phases & Changes

> **术语约定**(per metis G2-A 歧义消解):本文件用 **Stage 0/1/2/3** 指 CEP 内部实施段;**roadmap phase-10** 指路线图 phase;**dGPU Phase 2/3**(phase-9)与 **PCIe EP Phase 1-8** 是其它命名空间,勿混。
>
> **🔑 两条真源分工(per Entry #23 —— 排队是 change 维度的治理,须由路线图承载实施路径)**
>
> | 维度 | 唯一真源 | 本文件的角色 |
> |------|---------|-------------|
> | **治理口径**(change 总数 / 串行规则 / 并发上限 / 变更名) | `ADR-CEP-00 §2 Decision 3` | **只引用,不重算** |
> | **实施路径**(顺序 / 季度 / 状态 / 准入前置归属) | **本文件 §4.0 排队总表** | **权威承载** —— 路线图 = 实施路径真相源 |
>
> ⚠️ 二者**不是**同一件事:ADR 决定「能排几个、怎么串」,路线图决定「**先做哪个、落在哪个季度、当前什么状态、前置挂在谁身上**」。
> **任何排期调整只改本文件 §4.0 + §11 台账**;若触及 change 总数 / 串行规则,则须走 ADR 修订(冻结面成员)。

### 4.0 实施路径总排队表(权威 —— 14 个 change × 1 季度,串行)

> **口径**:每个 change 独占 1 KPI slot、独占 1 个季度(per `ADR-CEP-00 §2 Decision 3`「14 个 change,串行 14 季度」+ §5.4 并发 ≤ 1)。
> **本表是实施路径的唯一权威视图** —— §4.2 ~ §4.4 是各 Stage 的**细节展开**(估时 / 关联 ADR / 语义),其**顺序号与季度以本表为准**;`phase-10.md` 的依赖图是**示意**,不作排期承诺。
> **状态口径**:🟡 in-progress(已建、待 archive)· ✅ 完成 · 📋 Planned(未建)· ⏸ 队列位保留(待裁决)。**📋 Planned 行可在末列标注外部阻塞**(如「⚠ R-1 阻塞」)—— **登记 ≠ 就绪**:排期位置是**计划**,阻塞标注是**债的可见性**,二者不冲突。
> **状态怎么改、改哪几处** ⇒ 见 **§4.7 载体分工与状态更新协议**(本表的状态列是 `tasks.md` 勾选的**投影**,不是第二真源)。

| 顺序 | Change | Stage | 季度 | 状态 | 承接的 Stage 1 准入前置 |
|------|--------|-------|------|------|----------------------|
| **1** | `cpptlm-cep-umbrella` | Stage 0 | **2026-Q4** | ✅ **archived**(2026-10-23;**完成归档**语义 —— tasks **99/99** 全勾 + delta spec 已合并进 `openspec/specs/cep-core-contracts/spec.md`;R10 已按 `§9.3` 闭合,per 台账 Entry #28/#29) | — |
| **2** | `cpptlm-cep-poc-spike` | Stage 1 | **2027-Q1** | ✅ **archived**(2026-10-24;**完成归档**语义 —— delta spec 已合并进 `openspec/specs/cep-poc-spike-verification/spec.md`(15 Scenario);tasks **全勾**;critic **7 轮复审**(前 6 轮 REJECT 揭示六类「有牙」缺口 → 全部修复 + mutant 反证,**第 7 轮 APPROVE**,per Entry #49~#54);S1/S2/S3 三 spike 实跑 + ctest 回归门;owner 重指向 per Entry #55) | 原承接的全部装配期前置已**重指向见 §4.2 说明 + Entry #55**(① 定义层已消费;② 建图层→队列 #4;③ 放行闸→全局不变量;M-4 决策→队列 #3;表体→队列 #6;其余→队列 #3 起) |
| **3** | `cpptlm-cep-stub-cache-core` | Stage 1 | **2027-Q2** | ✅ **archived**(2026-10-25;**完成归档**语义 —— tasks **36/36** 全勾;delta capability **2 个**(`cep-vc-mapping-contract` 8 Scenario / `cep-stub-cache-core` 9 Scenario)**已合并**进 `openspec/specs/`;归档路径 `openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core/`;**双 Phase** 全部完成 —— Phase 0 = M-4 决策落盘 + 契约化(协议层 credit **不独占** VC / 传输层**保持索引对齐** / `VC_DAT` 语义正名 / 冲突输入**传输层不仲裁** / 桶名不改名不弃用 / `fabric.schema.json` **零改动**),Phase 1 = stub cache core 实现(6 接口 + 5 态 + `INV-SC1`~`INV-SC6`);**`ADR-CEP-05 §10` 的 `-base`/`-impl` 二分作废**(2026-10-25 裁决),per 台账 Entry #58 / #59 / #60) | **M-4 行为侧决策**(✅ **已消费** —— 队列 #3 归档时落成 `cep-vc-mapping-contract`:协议层 credit **不独占** VC(权威已闭,per ArchForge `32 号 §6.0.1`)+ 传输层**保持索引对齐** + `VC_DAT` 语义正名 + 冲突输入**传输层不仲裁**;契约化于 `cep-vc-mapping-contract`,结论见归档件 design.md D2~D9) |
| **4** | `cpptlm-cep-directory-ordering-plane` | Stage 1 | **2027-Q3** | ✅ **archived**(2026-10-25;**完成归档**语义 —— 22 Scenario 并入 3 份 main spec:`cep-sharer-tracker-base` 9 / `cep-ordering-point-checker` 6 / `cep-channel-dependency-gate` 7;archive 名 `2026-10-25-cpptlm-cep-directory-ordering-plane`;builder Entry #63 / 归档 Entry #64) | **已交付(历史记录,不再承接)**:SharerTrackerIf 单接口 + caps descriptor + D-5 全量契约 + FullMapDirectory 唯一行为实现 · AddressResolverIf + J1/J2/J3 + `INV-CEP-08-IC2` + (β) 重评估裁决 · 装配期通道依赖图(逐 `(network, vc)` verdict,双挂共享判据)。**Stage 1 准入前置的登记与余项已重指向队列 #5 行**(per Entry #64) |
| **5** | `cpptlm-cep-collective-plane-base` | Stage 1 | **2027-Q4** | ✅ **archived**(2026-10-26;**完成归档**语义 —— 21 Scenario 并入 2 份 main spec(`cep-collective-plane-base` 13 / `cep-collective-plane-contract` 8);archive 名 `2026-10-26-cpptlm-cep-collective-plane-base`;planner 见 Entry #66、builder 见 Entry #67、复审修复见 Entry #68/#69、归档见 Entry #70) | **已交付(历史记录)**:CollectivePlaneIf 恰好 7 方法 + 类型集 + NullCollectivePlane(**行为化 `INV-CEP-07-IC3`**)+ LocalCollectivePlane(group 生命周期 / region-epoch 两阶段 / BARRIER rendezvous / **R2 判读侧三态**,**行为化 `INV-CEP-04-IC5` 的 R2 判读侧**)+ 6 条契约判据(零调用边 / canonical 10 流零 diff / VC 预留 / 区域互斥同源 / 完成域判别 / 抓取点可测性登记)+ 独立声明源 `collective_planes.json` + 离线 CLI。**新增 invariant 数 = 0**。|
| **6** | `cpptlm-cep-moesi-gpu-adapter` | Stage 1 | **2028-Q1** | ✅ **archived**(2026-10-27;**完成归档**语义 —— 14 Scenario 并入 2 份 main spec(`cep-moesi-gpu-adapter` 7 / `cep-adapter-schema-contracts` 7);archive 名 `2026-10-27-cpptlm-cep-moesi-gpu-adapter`;planner 见 Entry #71、builder 见 Entry #79、归档见 Entry #80) | **DRI 三问裁决(2026-10-26,per Entry #71)**:① `moesi_gpu` / `mesif` **排入** Stage 1 前段(原 #6 `chi-rnf-subset` 顺延为 #7,`mesif` 新增为 #9)② **承接项 7 ①②**(`perm` 形态 schema 演进)**跟 `moesi_gpu` 走** —— 其**唯一真实消费者**是本实例(`ADR-CEP-05 §13.1` 判据 2 **已触发**:`moesi_gpu` 静默 `E→M` ⇒ 4 值 `perm` 无法表达)③ **承接项 5**(6 类 → `CATEGORY_*` 显式映射表 + contract test)**跟首个 adapter 走**。**交付面**:`MoesiGpuAdapter`(6 态 × 6 事件表驱动;`E→M` **静默升级**) + 6→4 投影 + **平台级** `perm` 静默升级表达 + 6→3 消息类映射表机制 + 首个 `implemented` 实例装配。**Stage 1 准入前置(滚动登记,per Entry #70 / #71 —— 原 owner 队列 #5 已归档,owner 重指向本行)**:**承接项 1 ②建图层** ✅ 已消费(队列 #4 装配闸)· **承接项 2**(首个 `implemented` 实例装配后复核 per-network 嵌套形态)**✅ 本 change 已交付**(2026-10-27 归档;本 change = 首个 `implemented` 实例)· **承接项 3** ⛔ 未触发(需第 3 张网 / 第 3 个协议实例 —— 按新队列于 #9 达成)· **承接项 4** ✅ 已消费(Entry #56,`fabric.json` 已入冻结集)· **承接项 5**(6 类 → `CATEGORY_*` 显式映射表 + contract test)**✅ 本 change 已交付**· **承接项 7 ①②**(`perm` 形态 schema 演进)**✅ 本 change 已交付**(③ (β) 已由队列 #4 消费)。**孤儿前置登记**:本行的 3 项孤儿前置(M-4 行为侧 / verdict 逐 `(network, vc)` 标注义务 / `moesi_gpu`·`mesif` 未排期)**已全部收口** —— 分别落成于队列 #3 的 `cep-vc-mapping-contract`、队列 #4 的 `cep-channel-dependency-gate`、并已于 Entry #71 消解(`mesif` → #9 行)⇒ **登记随本 change 归档转指队列 #7 行**(per 断言 ⑦ ③b/③c;本行不再带标记,内容保留可追) |
| **7** | `cpptlm-cep-cohif-carrier` | Stage 1 | **2028-Q2** | ✅ **已归档**(2026-10-28,Entry #90 —— 「**已完成归档**」语义:2 份 delta spec **已合并**进 `openspec/specs/cep-cohif-carrier/` + `openspec/specs/cep-protocol-bridge-contracts/`;Phase 1 ~ 7 全完成(41/41 task)+ 6 条判据全闭合 + **新增 invariant 数 = 0**;KPI 1/3 → **0/3**;⚠️ **2026-10-27 转正时的活跃路径** `openspec/changes/cpptlm-cep-cohif-carrier/` **已于 2026-10-28 归档时失效** ⇒ 现为 `openspec/changes/archive/2026-10-28-cpptlm-cep-cohif-carrier/`(**不入库**,per `.gitignore`)—— 2026-10-27 转正记录见 Entry #81) | **新准入前置**:无。本 change **服务** #6 的第 ② 项准入前置与 #8 的装配验证 —— 把 #6 的「**逻辑级**装配」(`stub_cache_core.hh:42` 明写 `net_side` 由 **mock ProtocolEngine** 驱动)升级为「**真实载体**装配」。**架构缺口来源**(architect 重审 + critic 四轮;`docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`):**C1** CEP 的 7 个 `.hh` 零 `Bundle`/`ChStream` 引用 · **C2** 「CEP 用 canonical 10 流的 Bundle」在 `ADR-CEP-07A:49` 与 `ADR-CEP-16:45` **两处**均为悬空引用 · **C3** 唯一的 Bundle 化承诺绑在 collective 队列(`ADR-CEP-03:225`)。**交付面(3 项 + 1 宏)**:① `include/bundles/cep_coherence_bundles_tlm.hh`(`bundles::CohIfBundle`,**只覆盖 5 条 RN-side 流**,`flow_id` 区分;无网络层字段) ② `include/tlm/cep/protocol_engine_base.hh`(`ChStreamModuleBase` 派生 + `Input/OutputStreamAdapter<CohIfBundle>`) ③ `include/framework/protocol_bridge.hh`(`ProtocolBridgeEr` + `ProtocolBridge<InB,OutB,Ordered>`,**首次落地**;`CohIfBundle ↔ NoCFlitBundle`) ④ `REGISTER_CHSTREAM_BIDIRECTIONAL` 宏(`registerObject` + `registerBidirectionalPortAdapter` **两件都做**;`static_assert(n>1)`) + `module_factory.hh:313-315` 钩子扩展(`register_protocol_bridge` 死钩子 → 活钩子)。**装配走 JSON 链**(非 programmatic —— `ADR-CEP-16:46/:86` 明确排除 programmatic bridge 的 C++ 路径;`is_multi` 分支 `module_factory.cc:743-750` 是泛型虚分派)。**显式延期**:`StubCacheCore` 的 ChStream 端口面 → `cpptlm-cep-stub-cache-chstream-surface`(需 MODIFIES 冻结 main spec,per `openspec/specs/cep-stub-cache-core/spec.md`)。**已签发**:`ADR-CEP-19`(2026-10-27,per Entry #74;草案文件已移除) · **planner 产出已落地 + 已转正**:`openspec/changes/cpptlm-cep-cohif-carrier/`(proposal + design + **tasks** + 2 份 delta spec;improvement 5 段见 `.rddf/improvements/cpptlm-cep-cohif-carrier.md`,per Entry #75/#76)—— 转正按 `design.md` `D-κ` 六步执行完毕(per Entry #81) · **Stage 1 准入前置 + 孤儿前置登记**:owner 自队列 #6 → #7 → **现转指队列 #8**(per Entry #90;#7 已归档 ⇒ 归档件**不得持有活义务**;明细见队列 #8 行) · **实施进度**:Phase 1 ~ Phase 7 **全部完成**(2026-10-27 / 2026-10-28,per 台账 Entry #84 ~ #89;Phase 3 交付 `ProtocolBridgeEr` + `ProtocolBridge<InB,OutB,Ordered>` + CohIf↔NoC 特化桥(**两层** pack/unpack)+ `register_protocol_bridge` 死钩子 → 活钩子 + `ModuleFactory` Step 7.5 消费路径;Phase 4 交付 `REGISTER_CHSTREAM_BIDIRECTIONAL` 装配宏 + `cpptlm::BundleDirectionTraits`(解耦 `BidirectionalPortAdapter` 的 `NoCFlitBundle` 硬编码);Phase 5 交付 `CohIfNullProtocolEngine` 可注册外壳 + `configs/cep_cohif_pe_minimal.json` + 端到端「一次 CohIf 事务**穿过** adapter 抵达对端」+ 反向 fixture 2 类 + `cpptlm::EgressChannel`(egress 通道由 Bundle 专属 trait 声明);Phase 6 交付 `test/test_cep_cohif_carrier.cc`(17 case)+ 共享 helper;Phase 7 交付 **反向 fixture 登记**(13 条,`scripts/README.md`)+ ctest 显式登记 3 条 + **零 diff** 复核(**新增 invariant 数 = 0**))⇒ **Phase 1 ~ 7 全完成,已归档**(2026-10-28,Entry #90) |
| **8** | `cpptlm-cep-chi-rnf-subset` | Stage 1 | **2028-Q3** | 📋 Planned | **范围已按 Entry #71 收窄**:`perm` 形态 schema 演进 与 6→3 映射表**机制** 均**已移至队列 #6**(跟唯一真实消费者 / 首个 adapter 走)⇒ 本 change **复用**之,只填 CHI 自身的映射行与实例声明。**交付**:CHI RN-F adapter(6 通道 TXREQ/RXRSP/RXDAT/RXSNP/TXRSP/TXDAT;RN-F 只发 REQ 只收 SNP;Credit 流控;TxnID 管理;CompAck)+ 按**已冻结** schema 填实例声明(`ordering_assumption` / `vc_binding` / `consistency_labels` / `sharer_tracker`)+ 装配期校验实测。**新准入前置**:无(前置已由队列 #6 交付) **Stage 1 准入前置(滚动登记;owner 自队列 #6 → #7 → **现转指本行**,per Entry #80 / #90)**:`S1-1` ②建图层 ✅ 已消费(队列 #4 装配闸)· `S1-2`(首个 `implemented` 实例装配后复核 per-network 嵌套形态)✅ **已由队列 #6 交付**(2026-10-27 归档)· `S1-3` ⛔ 未触发(需第 3 张网 / 第 3 个协议实例 —— 按新队列于 #9 达成)· `S1-4` ✅ 已消费(Entry #56,`fabric.json` 已入冻结集)· `S1-5`(6 类 → `CATEGORY_*` 显式映射表 + contract test)✅ **已由队列 #6 交付**· `S1-7 ①②`(`perm` 形态 schema 演进)✅ **已由队列 #6 交付**(③ (β) 已由队列 #4 消费) · **孤儿前置(承接自队列 #6,已收口,per Entry #80;2026-10-28 owner 自 #7 转指本行,per Entry #90)**:⏭(孤儿) **M-4 行为侧** ✅ 落成于队列 #3 的 `cep-vc-mapping-contract` · ⏭(孤儿) **`moesi_gpu` / `mesif` 未排期** ✅ 已于 Entry #71 消解(`mesif` → #9 行) · ⏭(孤儿) **verdict 逐 `(network, vc)` 标注义务** ✅ 落成于队列 #4 的 `cep-channel-dependency-gate` |
| **9** | `cpptlm-cep-mesif-adapter` | Stage 1 | **2028-Q4** | 📋 Planned | **⚠ 显式阻塞登记**(per Entry #71):**R-1 第 6 种 SharerTracker 实现 `HierarchicalDirectory`** —— ArchForge 侧 `mesif` 声明的 `sharer_tracker.impl=SparseSnoopFilter` 与其**三级目录**语义**不自洽** ⇒ 本 change 落地**依赖**该实现,而**本仓尚无 owner**。**另**:ArchForge 侧**无 `mesif` 状态机规约**(仅 5 态词表 M/E/S/I/F)与**无 5→4 投影表** ⇒ 实现须本地推导(**待 ArchForge 追认**)。**排期 ≠ 就绪** —— 本行是**登记**,不是承诺(「登记 ≠ 排期」口径的反向用法:登记的是**依赖**,不是**就绪**) |
| **10** | `cpptlm-cep-cxl-nonsymmetric` | Stage 1 | **2029-Q1** | 📋 Planned | — |
| **11** | `cpptlm-cep-tilelink-tree-manager` | Stage 1 | **2029-Q2** | 📋 Planned | — |
| **12** | `cpptlm-cep-cxl-dcoh-symmetric` | Stage 2 | **2029-Q3** | 📋 Planned | — |
| **13** | `cpptlm-cep-nvlink-sharp` | Stage 2 | **2029-Q4** | 📋 Planned | — |
| **14** | `cpptlm-cep-ualink-inc` | Stage 3 | **2030-Q1** | 📋 Planned | — |

> **终态**:Stage 0~3 实施完成 = **2030-Q1**(per §4.6);Stage 4+ 进入维护态(per §4.5)。
> **排期调整规则**:本表任一行的**顺序 / 季度**变化 ⇒ **必须**在 §11 追加台账 entry(kind ∈ `scope-change` / `deferral-rationale`),并**五处同改**(本表是触发源,不算在内):
> ① §4.2 ~ §4.4 的顺序号 ② **§4.6 终态口径** ③ `phase-10.md` 依赖图 + KPI 预算行 ④ §10「Stage 1~3 远期候选」的顺序指针 ⑤ `.rddf/roadmap.md` Objectives 段的 `Entry` 号引用。
>
> **单源纪律(比"同改"更强)**:具体**季度值**只允许出现在 **objective 内**(本表 / §4.6 / 小节标题 / 表头,同一文档内自洽);`phase-10.md` / `.rddf/roadmap.md` / `.rddf/improvements/*.md` **一律只写「见 §4.6」,不复制数值**。台账 §11 entry 内的历史快照**不算副本**(append-only)。
>
> **改完自检 = 双边化**(新值 **和** 旧值都要查,只查新值会漏掉「旧数复活」):
> **语义规则**:「**引述旧值的历史说明段**」(增补块 / 变更说明 / 新旧对照表)**不算副本** —— grep 只是粗筛,最终以语义判定;真实缺陷是**旧值被当作现行口径复活**。
> ```bash
> # ① 新值:季度值在 objective 之外应零命中(台账 / implementation 快照除外)
> grep -rn "2030-Q1\|2028-Q4" .rddf/ docs/ | grep -v "obj-cep-platform.md" | grep -v "docs/implementation/"
> # ② 旧值:已被取代的旧终态 / 旧串行数,不应在新文本里复活
> grep -rn "2029-Q4" .rddf/ docs/ | grep -v "obj-cep-platform.md" | grep -v "docs/implementation/"
> grep -rn "串行 13 季度\|13 个 change" .rddf/ docs/ | grep -v "obj-cep-platform.md" | grep -v "docs/implementation/"
> ```
> **已知例外(2026-10-27 实测重测,per Entry #74 —— 排期 13 → 14 后模式已重指,旧清单必须重测)**:
> ① 自检 ①(新值 `2030-Q1` / `2028-Q4`):**2 处**命中,均在 `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`(:255「终态 2029-Q4 → **2030-Q1**」的**新旧对照** / :407 待裁决表的**决策记录**)⇒ 属「引述旧值的变更说明段」,按语义规则**合法**。
> ② 自检 ② 的 `2029-Q4`:**1 处**命中(同上 :255 的新旧对照)⇒ **合法**。
> ③ 自检 ② 的 `串行 13 季度|13 个 change`:**3 处**命中 ——
>   · `docs/adr/ADR-CEP-00-platform-charter-and-governance.md:56` / `:138`:**冻结面成员**,待 `ADR-CEP-19` **签发轮**同步(owner = architect;**本 entry 已授权**,见 §11 Entry #74 的冻结面授权表)⇒ **已知未结**,不是复活;
>   · `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md:179`:已加**时态锚定**(「**当时**(2026-10-26,13 个 change 队列)」)⇒ 历史引述,**合法**。
> ④ `.rddf/drafts/cep-architecture-decision-log.md`(:28,记「10 个 change」)→ architect 阶段**草稿**,零入向引用(不参与路线图口径);不在本轮 grep 模式内(模式已重指到 13),保留登记以示**该草稿已落后两代**。
> **除 ① 的 2 处 + ② 的 1 处 + ③ 的 3 处 + ④ 的 1 处外出现新命中 = 旧数复活,必须改回。**
> (2026-10-23 更新:原 3 处例外中的 ① `07 号 §8.1` 与 ② `ADR-CEP-NUMBER-MAP §2` **已随 Entry #27 闭合** —— 旧值已不存在,不再需要例外)

### 4.1 Stage 0: Umbrella 启动(队列 #1 = `cpptlm-cep-umbrella`,2026-Q4)


| 子任务 | 状态 | 范围 | 估时 | 关联 ADR | KPI slot |
|--------|------|------|------|----------|----------|
| 10.A 真源 ADR 起草 | ✅ 完成(2026-10-19~21) | **21 份 ADR**(两批新增 08/09/12/13/18 + 11/14/15/16/07A,修订 8 份) | 12 人日(architect) | ADR-CEP-00~18 全族 + NUM | 0(doc-only)|
| **10.B OpenSpec umbrella** | ✅ **完成 + 已归档(2026-10-23)** —— 4 件套已落;tasks **99 / 99** 全勾(原 105 项中 6 项 Stage 1 准入前置已移出本 change 的任务表);**R10 已闭合**(按 `§9.3` 判据:残余风险已登记 + DRI 书面接受,per 台账 Entry #28);**完成归档** —— delta spec 已合并进 `openspec/specs/cep-core-contracts/spec.md`;`openspec validate cep-core-contracts --strict` **rc=0** | `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/` proposal + design + specs + tasks | 5 人日(builder,已消耗) | umbrella improvement | **1**(计 KPI,已释放)|
| 10.C 契约测试 | ✅ **完成(2026-10-21~22)** | `test/test_cep_canonical_invariants.cc` + `test_cep_dummy_signals.cc` + `test_cep_module_factory_seam.cc` + `test_cep_simulation_mode_lock.cc`(+ 后期补充 `test_cep_schema.cc` / `test_cep_contracts_crosscheck.cc` / `test_cep_tools_contract.cc`) | 10 人日(builder,已消耗) | ADR-CEP-06 + ADR-CEP-07 | 0 |
| 10.D experimental + tools | ✅ **完成(2026-10-22)** | `tools/render_report.py` + `run_sweep.py` + `ab_compare.py` + `experimental/metrics.schema.json` | 5 人日(builder,已消耗) | ADR-CEP-06 §4 | 0 |
| 10.E 顶层真源 | ✅ **完成(2026-10-20~21;后增补至 686 行)** | `docs/architecture/07-coherence-exploration-platform.md` 整合 **21 份** ADR(**686 行**) | 5 人日(architect,已消耗) | ADR-CEP-00 §5.2 | 0 |
| 10.F ADR 锚点 lint | ✅ **完成(2026-10-22;后扩至 **10 组**)** | `scripts/test/cep_docs_sync_check.sh` 机器检查(覆盖 **21 份** ADR;第 10 组 = 传输层 `CATEGORY_*` 分层纪律,per Entry #11) | 6 人日(已消耗) | ADR-CEP-00A §5.1 | 0 |
| **10.G capability schema** | ✅ **完成(2026-10-22)** | `include/tlm/cep/protocol_instance.schema.json` + `INV-CEP-12-IC1~3` 断言(+ `fabric.schema.json` / `fabric.json`) | 3 人日(已消耗) | ADR-CEP-12 §2 + §3 | 0(沿用 10.B)|

**Stage 0 估时(2026-10-23 重算,per Entry #22)**:§4.1 表逐行合计 = **46 人日**(10.A 12 + 10.B 5 + 10.C 10 + 10.D 5 + 10.E 5 + 10.F 6 + 10.G 3)。
> **口径修正**:原「41 人日 / 已消耗 13 / 剩余 28」与表内逐行值**不同步**(10.A 由 8 → 12 人日后未回填算式)→ 本条以表内合计为准。

- **实施侧**:10.A ~ 10.G **全部已落地**(tasks **99 / 99 全勾**)→ **Stage 0 工程量视为已消耗**。
- **剩余工程量**:**0**。umbrella 的 6 项 Stage 1 准入前置(S1-1 ~ S1-5 + S1-7)已**移出该 change 的任务表**(条目 / 判据 / 归属全保留)→ 按 **§4.0 排队表**归属 Stage 1(队列 #2 / #4 / #6),**不计入 Stage 0**。
- **卡口已闭合(2026-10-23)**:R10 已按 **§9.3** 的闭合判据处置(残余风险已登记 + DRI 书面接受,per 台账 Entry #28);Stage 0 **已完成归档**(per Entry #29)。**残余风险**:契约层结论未经独立第二方(人类)复核 —— 详见 §9.3,不被「闭合」二字掩盖。

### 4.2 Stage 1: Stub Cache + Tracker + CohIf 载体 + 5 协议实例(队列 #2 ~ #11,2027-Q1 ~ 2029-Q2)

**串行约束**:每 change 独占 1 KPI slot、独占 1 个季度,任一时刻仅 1 个 active(per §5.4)。**顺序 / 季度 / 「承接的 Stage 1 准入前置」归属三者一律以 §4.0 排队总表为准**;本表只承载**估时 + 关联 ADR + 语义**(末列是 §4.0 的同义复述,漂移时以 §4.0 为准)。

| 顺序 | Change | 关联 ADR | 估时 | 承接的 Stage 1 准入前置 |
|------|--------|----------|------|----------------------|
| **2** | **`cpptlm-cep-poc-spike`** ← 2026-10-22 新增(P-1,置首位)· ✅ **archived**(2026-10-25;**完成归档**语义 —— delta spec 已合并进 main specs;critic 7 轮复审 → 第 7 轮 APPROVE;装配期前置的逐项分层与 owner 见本行末列,2026-10-24 planner 审查 per Entry #46)** | `ADR-CEP-13`(判据 (b) 的**可执行**判定 = Q-3e)+ `ADR-CEP-09`(顶点升级)+ `ADR-CEP-12`(per-network VC) | **3 周**(spike,非实现) | 装配期前置已**全部重指向**(Entry #55):建图层→队列 #4 / 放行闸→全局不变量 / M-4 决策→队列 #3 / 表体→队列 #6 / 其余→队列 #3 起 |
| **3** | `cpptlm-cep-stub-cache-core` ✅ **archived(2026-10-25)** —— 完成归档语义;tasks **36/36** 全勾;2 个 delta capability 已合并进 main specs | ADR-CEP-05 + ADR-CEP-04 stub | 8 周(实际交付面**显著更小** —— 单文件头可实现) | **M-4 行为侧决策**(✅ **已消费** —— 队列 #3 归档时落成 `cep-vc-mapping-contract`:协议层 credit **不独占** VC(权威已闭)+ 传输层**保持索引对齐** + `VC_DAT` 语义正名 + 冲突输入**传输层不仲裁**;契约化于 `cep-vc-mapping-contract`,per 台账 Entry #58 / #59 / #60) |
| **4** | **`cpptlm-cep-directory-ordering-plane`** ✅ **archived(2026-10-25)** ← 2026-10-21 重排(原 `sharer-tracker-base` 扩容) | **ADR-CEP-04**(Sharer Tracker 抽象 + caps)+ **ADR-CEP-08**(Address Resolver + Ordering Point 三判据)+ **ADR-CEP-09 Decision A**(装配期通道依赖图无环检查) | **12 周**(原 8 周;含建图层与逐 `(network, vc)` verdict 义务) | **已交付(历史记录)**:单接口 + caps + D-5 + FullMapDirectory · AddressResolverIf + J1~J3 + `INV-CEP-08-IC2` + (β) 裁决 · 装配期通道依赖图(双挂共享判据,`[verdict] net=… vc=…` 逐行输出)。**余项登记已重指向队列 #5 行**(per Entry #64) |
| **5** | `cpptlm-cep-collective-plane-base` ✅ **archived(2026-10-26)** | ADR-CEP-03 + ADR-CEP-01 | 8 周(实际交付面**小于**标称 —— 2 header + 2 声明源 + 1 CLI + 3 test,per 队列 #2/#3/#4 先例) | **已交付(历史记录)**:CollectivePlaneIf 7 方法 + Null(行为化 `INV-CEP-07-IC3`)+ Local(group/epoch/BARRIER,行为化 `INV-CEP-04-IC5` 的 **R2 判读侧**)+ 6 条契约判据 + 独立声明源 + CLI;**新增 invariant 数 = 0**。**Stage 1 准入前置的余项登记已重指向队列 #6 行**(per Entry #70) |
| **6** | **`cpptlm-cep-moesi-gpu-adapter`** ← 2026-10-26 新增(DRI 三问裁决①,per Entry #71)· ✅ **archived**(2026-10-27;**完成归档**语义 —— 14 Scenario 并入 2 份 main spec;archive 名 `2026-10-27-cpptlm-cep-moesi-gpu-adapter`;归档见 Entry #80) | **ADR-CEP-12**(schema owner:`perm` 形态演进 + 首个实例声明)+ **ADR-CEP-05 §13.1**(判据 2 已触发 —— `moesi_gpu` 静默 `E→M`)+ **ADR-CEP-02**(canonical 10 流)+ **ADR-CEP-13**(域可见写判据 (a)(b)) | 10 周 | **承接项 7 ①②**(`perm` 形态 schema 演进 —— **跟唯一真实消费者走**,per Entry #71 裁决②)+ **承接项 5**(6 类 → `CATEGORY_*` 显式映射表 + contract test —— **跟首个 adapter 走**,per 裁决③)+ **承接项 2**(首个 `implemented` 实例装配后复核 per-network 嵌套形态)**✅ 本 change 已交付**(2026-10-27 归档);**承接项 3** ⛔ 未触发(于 #9 达成)· **承接项 1 ②** / **承接项 4** ✅ 已消费· **承接项 7 ③ (β)** ✅ 已消费(队列 #4)。**孤儿前置**:3 项(M-4 行为侧 / verdict 逐 `(network, vc)` 标注义务 / `moesi_gpu`·`mesif` 未排期)**已全部收口** ⇒ 登记随本行归档转指队列 #7 行(per 断言 ⑦ ③b/③c) |
| **7** | **`cpptlm-cep-cohif-carrier`** ← 2026-10-27 新增(per architect 重审 → critic 四轮;**架构缺口 C1/C2/C3**)· 🟡 **Proposed**(2026-10-27 转正,Entry #81) | **ADR-CEP-19**(✅ 已签发 2026-10-27:`CohIfBundle` + `ProtocolBridge` + JSON 装配)+ **ADR-CEP-16**(programmatic bridge 排除 ⇒ 走 JSON 链) | 6 周(3 项交付 + 1 宏 + 1 钩子扩展;`StubCacheCore` 侧端点已延期) | **新准入前置**:无 —— 本 change **服务** #6 的第 ② 项准入前置与 #8 的装配验证:把 #6 的**逻辑级装配**(`net_side` 由 mock ProtocolEngine 驱动)升级为**真实载体装配**。**显式延期**:`StubCacheCore` 的 ChStream 端口面 → `cpptlm-cep-stub-cache-chstream-surface` · **Stage 1 准入前置 + 孤儿前置登记**:owner 自队列 #6 → #7 → **现转指队列 #8**(per Entry #90;#7 已归档 ⇒ 归档件**不得持有活义务**;明细见队列 #8 行) |
| **8** | `cpptlm-cep-chi-rnf-subset` ← 2026-10-27 顺延(原 #7) | ADR-CEP-01 + ADR-CEP-02 + **ADR-CEP-12**(实例声明:填 `ordering_assumption` / `vc_binding` / `consistency_labels` / `sharer_tracker`) | 8 周 | **范围收窄**(per Entry #71 裁决②③):`perm` 形态演进与 6→3 映射表**机制**已移至队列 #6 ⇒ 本 change **复用**,只填 CHI 自身映射行与实例声明。**新准入前置**:无 —— 但**首次在真实载体上验证**(前置 = #6 的 6→3 字典 + #7 的 `CohIfBundle` / `ProtocolBridge`) **Stage 1 准入前置(滚动登记;owner 自队列 #6 → #7 → **现转指本行**,per Entry #80 / #90)**:`S1-1` ②建图层 ✅ 已消费(队列 #4 装配闸)· `S1-2`(首个 `implemented` 实例装配后复核 per-network 嵌套形态)✅ **已由队列 #6 交付**(2026-10-27 归档)· `S1-3` ⛔ 未触发(需第 3 张网 / 第 3 个协议实例 —— 按新队列于 #9 达成)· `S1-4` ✅ 已消费(Entry #56,`fabric.json` 已入冻结集)· `S1-5`(6 类 → `CATEGORY_*` 显式映射表 + contract test)✅ **已由队列 #6 交付**· `S1-7 ①②`(`perm` 形态 schema 演进)✅ **已由队列 #6 交付**(③ (β) 已由队列 #4 消费) · **孤儿前置(承接自队列 #6,已收口,per Entry #80;2026-10-28 owner 自 #7 转指本行,per Entry #90)**:⏭(孤儿) **M-4 行为侧** ✅ 落成于队列 #3 的 `cep-vc-mapping-contract` · ⏭(孤儿) **`moesi_gpu` / `mesif` 未排期** ✅ 已于 Entry #71 消解(`mesif` → #9 行) · ⏭(孤儿) **verdict 逐 `(network, vc)` 标注义务** ✅ 落成于队列 #4 的 `cep-channel-dependency-gate` |
| **9** | `cpptlm-cep-mesif-adapter` ← 2026-10-27 顺延(原 #8;2026-10-26 新增 per Entry #71) | **ADR-CEP-04**(Sharer Tracker 第 6 实现 `HierarchicalDirectory`)+ **ADR-CEP-12** | 10 周 | **⚠ R-1 阻塞**:依赖**尚不存在**的第 6 种 SharerTracker 实现;ArchForge 侧**无 `mesif` 状态机规约**与**无 5→4 投影表** ⇒ 本地推导待追认。**排期 ≠ 就绪** |
| **10** | `cpptlm-cep-cxl-nonsymmetric` ← 2026-10-27 顺延(原 #9) | ADR-CEP-04 + ADR-CEP-03 | 8 周 | — |
| **11** | `cpptlm-cep-tilelink-tree-manager` ← 2026-10-27 顺延(原 #10) | ADR-CEP-04 + ADR-CEP-08 + ADR-CEP-12 | 8 周 | — |

**Stage 1 跨度**:2027-Q1 ~ 2029-Q2(**10** 个 change × 1 季度,per §4.0)

> **季度口径(per Entry #23 修订)**:原表内「乐观并行估计」季度列**已撤除** —— 它与 §4.0 的串行季度**并存即两个数**,且表内估时逐行合计 55 周 > 1 年,无法自洽。**排期承诺唯一口径 = §4.0 排队总表**(1 change / 1 季度)。本表的「估时」是**工作量参考**,不是排期。
>
> **Stage 1 准入前置归属(per Entry #22)**:上表末列把 `tasks.md` 的 **S1-1 ~ S1-5 + S1-7** 七项落到具体 change —— **不新增 change、不新增 KPI slot**。边界:`perm` **维持 4 值锁**(per `ADR-CEP-02 CEN-Ib11`);R-2 落地形态是**新增字段**而非扩枚举;schema 变更属 **Stage 1**,Stage 0 冻结面不动。
>
> **S1-1 的三层 + S1-2/3/4 的归档后承接(per Entry #55,归档 owner 重指向)**:S1-1 的**①定义层已消费**(`ADR-CEP-09 §8.2`,commit `964455e8`);**②建图层 → 队列 #4**(见上表末列);**③放行闸改判为全局不变量** —— `ADR-CEP-09:221` 的「不得用其无环判据放行任何协议实例」**不挂任何单队列行**,其解除条件 = 「②完成 **+** 首个待放行实例装配验证通过」,故建模为 **每个协议实例 change(`#6+`)spec 的放行 Scenario**。理由:③ 的解除在 #4 的季度内**物理不可完成**(首个 `implemented` 实例在 #6,晚两季)→ 挂 #4 即制造**永久债**(违 `design.md` C4)。S1-2 / S1-3 / S1-4 的承接方 = **队列 #3+**(随首个 `implemented` 实例 / 第 3 张网出现),度量口径与复核节奏见 `design.md` D4.5。

> **P-1 增补说明(2026-10-22,per 台账 Entry #12)**:`cpptlm-cep-poc-spike` 是**可丢弃的 spike**
> (单文件 / 不进 `ModuleFactory` / 不注册 ABI / 无公共 API),**不是**实现 —— 只验证「可组合性 + 契约自洽」。
> **置首位**的理由:其 S2 场景是 `ADR-CEP-13` 判据 (b) 的**可执行**判定 → **解阻 A6** →
> A6 是全部协议插件 change 的前置(`perm` 表达力未定)→ 它是唯一能**加速**其余 Stage 1 change 的项。
> 用例规格:`docs/implementation/2026-10-22-cep-poc-spike-spec.md`。
> **排队长度 +1,并发不变**(仍守「任一时刻 Proposed ≤ 1」)。
>
> **P-1 后续事实登记(2026-10-23,per Entry #22)**:①**S2 已实跑通过**(首次 RUN 证据被 critic 3 BLOCKERS 证伪 → 修正重跑,per Entry #16)→ 实测 `ctest -R cep` **18/18**、`[cep]` **76 case / 1284 assertion**;②**change 目录仍未建**(`openspec/changes/` 下无 poc-spike change 目录)—— umbrella 已于 2026-10-23 **完成归档** → KPI slot **已释放** → **队列 #2 `cpptlm-cep-poc-spike` 可启动**(其 change 目录仍待建,per 下文 ④);③**S1 / S3 未实现**(S1 需 4 值表达力边界 = Stage 1;S3 需 `vc_binding` 真实声明);④**落地形态待 DRI 裁决** —— 保留队列位(保住 11 change 算术),不建 change、不动 ADR。

### 4.3 Stage 2: CXL 对称 + NVLink SHARP(队列 #12 ~ #13,2029-Q3 ~ 2029-Q4)

| 顺序 | Change | 关联 ADR | 估时 |
|------|--------|----------|------|
| **12** | `cpptlm-cep-cxl-dcoh-symmetric` | ADR-CEP-04 §3.5 | 10 周 |
| **13** | `cpptlm-cep-nvlink-sharp` | ADR-CEP-03 + ADR-CEP-01 | 10 周 |

### 4.4 Stage 3: UALink INC(队列 #14,2030-Q1)

| 顺序 | Change | 关联 ADR | 估时 |
|------|--------|----------|------|
| **14** | `cpptlm-cep-ualink-inc` | ADR-CEP-03 | 10 周 |

### 4.5 Stage 4+ 远期(2030-Q1 起 —— 队列 #14 完成之后)

- `CEP-08` / `CEP-09` / `CEP-12` / `CEP-13` —— ✅ **已起草 v0.1(2026-10-21)**,实现按 §4.2~§4.4 分 Stage 落地
- `CEP-17` 协议族接入 + 组合承诺边界 —— ✅ **已起草**(Ruby-SLICC 族;边界声明 per `ADR-CEP-17` 决策 9)
- `CEP-11` 协议版本锁定 —— ✅ **已起草**(Stage 1 前签发,承 R2 缓解)
- `CEP-14` Stub 可验证性合同测试 —— ✅ **已起草**
- `CEP-15` CacheTLM 对齐 —— ✅ **已起草**
- `CEP-16` 06 §15.4 演进对应 —— ✅ **已起草**
- `CEP-07A` 06-bis 既有产物处置 —— ✅ **已起草**
- `CEP-18` 协议生成路线范围 —— ✅ 已起草 v0.1;**Stage 1~3 不依赖任何生成器**(per `ADR-CEP-18` 决策 1)
- **完整 22 份清单**: `docs/adr/ADR-CEP-*.md`(per `ADR-CEP-NUMBER-MAP §2 Decision 1`;21 → 22 待 `ADR-CEP-19` 签发)
- **维护模式**:Stage 4+ 进入维护态(新协议接入走 add-feature + 单 change,不再串行排队)

### 4.6 终态时间口径(per oracle B6 + metis G2-G 修订 + 2026-10-23 季度算术修正)

- **Stage 0~3 实施完成**:**2030-Q1**(队列 #14 = `cpptlm-cep-ualink-inc` 完成;per §4.0)
- **Stage 4+ 维护期**:**2030-Q1 起**(无固定上界;新协议接入走 add-feature + 单 change)
- **objective 终态验收**:**2030-Q1**(与实施完成同点;此后转 90 天复审节奏,per §12)
- **季度口径(per Entry #23 修订)**:**串行 14 季度** —— 2026-Q4 起算,1 change / 1 季度 → 2030-Q1(per `ADR-CEP-00 §2 Decision 3`;2026-10-22 per Entry #12 由 10 → 11;**2026-10-26 per Entry #71 由 11 → 13** —— `moesi_gpu` / `mesif` 排入 Stage 1)。**§4.2~§4.4 表内「估时」是工作量参考,不是排期**;排期承诺唯一口径 = **§4.0 排队总表**。

---

### 4.7 载体分工与状态更新协议(新会话起点 —— 推进实施只读这一节)

> **为什么单列一节**:CEP 的**一个 change 有 5 类载体**。若新会话不知道「谁写入、谁是真源、改哪几处」,必然出现「同一个数字写在 3 处、改 1 处漏 2 处」。**本节是分工的唯一声明** —— 其余文档只放指针,不复述。

#### 4.7.1 五类载体:谁写、答什么、何时更新

| 载体 | 唯一职责(回答什么问题) | 写入方 | 状态写在哪 | 何时更新 |
|------|----------------------|--------|-----------|---------|
| **objective** `obj-cep-platform.md` | **为什么做 / 终态 DoD / 排队(§4.0) / 决策台账(§11)** | planner | §4.0 行的**状态列**(投影) | change 增删或重排 · 阶段完成 · 决策发生 |
| **phase** `phase-10.md` | 本 phase 的**子任务拆解 + 执行态** | planner | front-matter `status` + 子任务表 | 子任务完成 · 卡口变化 |
| **improvement** `.rddf/improvements/<name>.md` | 这个 change **为什么值得做**(5 段规划输入) | planner | front-matter `状态:` | 立项时写一次;范围变更时改 |
| **OpenSpec change** **openspec/changes/&lt;name&gt;/** | **做什么 / 怎么验 / 做到哪** ← **执行真源** | builder | `proposal.md` 状态行 + `tasks.md` 勾选 | **每完成一个 task** |
| **实施笔记** `docs/implementation/` | 某次实施 / 评审的**时点快照证据** | 任何角色 | 版本行 | 事件发生时 append,**不回改** |

**三条硬规则**:

1. **状态唯一权威 = OpenSpec change 的 `tasks.md` 勾选 + `proposal.md` 状态行**。**§4.0 的状态列、`phase-10.md` 的子任务表、`.rddf/improvements/*.md` 的 front-matter `状态:` 三者都是它的投影** —— 任一投影与 change 不一致时**以 change 为准**,并立刻回填该投影。
2. **数量口径只在 ADR 写**(14 change / 14 季度 / 22 份 ADR / 66 条 invariant / **冻结成员数以 `cep_freeze_check.sh` 清单为准**)。objective / phase / improvement / change **一律引用,不复算**。
3. **实施笔记 append-only** —— 过期快照不改写,由新记录覆盖(不「改历史」)。

#### 4.7.2 change 状态机

```
📋 Planned ─────→ 🟡 Proposed / in-progress ─────→ ✅ archived
      │                        │
      └──→ ⏸ 队列位保留 ────────┘     (占 KPI slot,但 change 目录**故意**未建)
```

| 状态 | 含义 | 判据 |
|------|------|------|
| **📋 Planned** | 已排队,未建 change 目录 | §4.0 有行;`openspec/changes/` 下无同名目录 |
| **🟡 Proposed / in-progress** | change 目录已建,任务未全勾 | `openspec validate <name>` **可执行**(rc 不限 —— `--strict` 的预存 WARNING 见 L1);`tasks.md` 有未勾 `- [ ]` |
| **✅ archived** | 归档完成(见下方**两种语义**) | 目录移入 `archive/`;§4.0 状态列已改 |
| **⏸ 队列位保留** | 有实测 / 有设计,change 目录**故意**未建 | §11 有 entry 写明**裁决理由 + 三条出路**(文档审计项,非机器判据) |

> ⚠️ **`✅ archived` 有两种语义,必须区分**(per `AGENTS.md` §DOC HYGIENE「两种归档语义必须区分」)—— **默认 `openspec archive` 会把未实现的 delta spec 合进 main specs,污染系统契约**,AGENTS.md 明文禁止。

| 语义 | 触发条件 | 命令 | delta spec | proposal 标记 |
|------|---------|------|-----------|--------------|
| **✅ 已完成归档** | 实现已落地,契约已在系统内 | `openspec archive <name>` | **合并**进 `openspec/specs/` | 无需特殊标记(实现可自证) |
| **⏸ 暂存归档** | 实现**未启动 / 0%**,但设计与判据仍有价值 | `openspec archive --skip-specs <name>` | **不合并**(保持 main specs 干净) | **必须**在 `proposal.md` 顶部加「⏸ 暂存归档」段(日期 + tasks 完成度 + 重建路径) |

> **判据**:`tasks.md` **全勾** → 完成归档;**未全勾(或 0% 实施)却要让出 KPI slot** → 暂存归档。二者**不可混用**。

**⏸ → 🟡 转正路径(队列位保留项必经,协议内唯一的"出口")**:

0. 🔴 **先确认 KPI slot 可用(硬前置,跳过此步必违规)** —— `openspec list --json` 实测 CEP 内部 Proposed 并发 **< 1**(per §5.4 并发 ≤ 1)。若已有 1 个 active(如当前 `cpptlm-cep-umbrella`),**必须**先按 §4.7.3 ⑥ 让出 slot,**再**建 change;否则一建就是 2 个 Proposed,直接违反 §5.4;
1. DRI 对 §11 该条 entry 写明的**三条出路**(① 建 change ② 移出队列 ③ 降级为设计草案)作出裁决,并**追加** §11 entry 记录裁决 —— 裁决本身不改写原 entry(append-only);
2. 裁决 = ① 建 change ⇒ 取出 §11 该条已登记的设计 / 实测材料,按 ADR-0025 D1/D2 生成 `proposal.md` + `design.md` + `specs/` + `tasks.md`;**若尚无 improvement 草稿,先补草稿**(planner 写);
3. §4.0 状态列 ⏸ → 🟡;`phase-10.md` 对应子任务行同步;§5.2 KPI 并发回填。

#### 4.7.3 一个 change 走完一轮要动哪些文件(实施者照做)

| 时点 | 动作 | 文件 |
|------|------|------|
| **① 起步** | 读自己那一行 → **有 improvement 则读**(无则读 §11 该条 entry —— ⏸ 项常无草稿) → 相关 ADR | §4.0 · `.rddf/improvements/<name>.md`(可能不存在)· `docs/adr/ADR-CEP-*.md` |
| **② 建 change** | 从 improvement 5 段生成 `proposal.md`(per ADR-0025 D1/D2)→ `design.md` → `specs/` → `tasks.md` | **openspec/changes/&lt;name&gt;/** |
| **③ 每完成一个 task** | 勾 `tasks.md`(**唯一状态真源**) | **openspec/changes/&lt;name&gt;/tasks.md** |
| **④ 阶段 / 卡口变化** | 改 §4.0 状态列 + 追加 §11 entry | `obj-cep-platform.md` §4.0 / §11 |
| **⑤ 有实施 / 评审事件** | 写一份时点快照(版本行注明依据) | `docs/implementation/` |
| **⑥ 收尾** | **全勾 → 完成归档**(`openspec archive`);**未全勾只为让 slot → 暂存归档**(`--skip-specs` + proposal 顶部 ⏸ 段)。随后按下方**收尾同步清单**逐项改 | change + objective + phase + roadmap |

**收尾同步清单(归档时 17 个落点,一处都不能漏)**:

> ⚠️ **2026-10-23 补全(critic 第 5 轮 B1 的真根因)**:原清单只有 **7** 个落点,**漏了 6 类** ——
> `obj §4.1` 各 Stage 展开行 / `obj §8.1` 风险行 / `obj` 头部同步注 / improvement 草稿 front-matter /
> `AGENTS.md` 全仓 KPI 基线行 / 门禁脚本的路径绑定。按不完整的清单执行 = **必然漏回填**。
> **清单不全 = 协议缺陷**,不是执行疏忽。

**A. change 自身(3)**

| # | 位置 | 改什么 | 可机器验 |
|---|------|--------|---------|
| A1 | change 的 `tasks.md` | 勾满(**完成归档**);或把**不属于本 change** 的条目**移交**(去 checkbox + 标注归属队列) | 人验 |
| A2 | change 的 `proposal.md` | 状态行(阶段 + task 计数 + 卡口)+ §Acceptance 勾选 + **顶部**「⏸ 暂存归档」段(**仅暂存归档**;完成归档不加) | 人验 |
| A3 | change 的 `design.md` | 若有内容被迁出(归档不携带的文本)→ 在此留去处 | 人验 |

**B. objective(`obj-cep-platform.md`)(7)**

| # | 位置 | 改什么 | 可机器验 |
|---|------|--------|---------|
| B1 | **§4.0** 排队表对应行 | 状态列 → ✅(暂存归档另注 `--skip-specs`) | **机器(①)** |
| B2 | **§4.1 ~ §4.4** 各 Stage 展开行 | 该 Stage 的实施态 / 卡口 / 剩余工程量 | **机器(①)** |
| B3 | **§5.2** | KPI 并发计数回填(以 `openspec list --json` 实测为准) | **机器(①)** |
| B4 | **§8.1** 风险表相关行 | 等级 / 缓解 / 结论(若该风险正是本次卡口) | **机器(①)** |
| B5 | **§9.x** 处置段 | 若本次变更改动了某条判据 → 追加处置段(**不改正文**) | 人验 |
| B6 | **头部同步注**(§0 下的 planner 同步块) | Stage 实施态 / 卡口描述 | **机器(①)** |
| B7 | **§11** 台账 | **追加** entry(完成结论 / 暂存理由 + 重建路径) | 人验 |

**C. 路线图 / 计划(3)**

| # | 位置 | 改什么 | 可机器验 |
|---|------|--------|---------|
| C1 | `phase-10.md` 对应子任务行 | 状态 → ✅(或 ⏸ 暂存)+ 剩余工程量 + **KPI baseline 行**(同文件内易与子任务行口径打架) | **机器(①)(2)** |
| C2 | `.rddf/roadmap.md` | Phase Skeleton 的 phase status(仅整 phase 变化时)+ 顶部「最后更新」引用最新 entry 号 + KPI baseline | **机器(①)** |
| C3 | `.rddf/improvements/<change>.md` | front-matter `状态:` + 「最后」行 | **机器(①)** |

**D. 仓级、门禁与真源(4)**

| # | 位置 | 改什么 | 可机器验 |
|---|------|--------|---------|
| D1 | `AGENTS.md` | 全仓 KPI 基线行(并发数)—— **新会话的第一入口**,错则满盘皆错 | **机器(①)** |
| D2 | 门禁脚本的**路径绑定** | 路径随迁时:`cep_freeze_check` 冻结 glob / `cep_docs_sync_check` 扫描 glob + 4 件套路径 / `cep_doc_ref_check` DOCS 列表 + VIRTUAL_PATHS。**门禁自改须逐个 glob 验证匹配数**(glob 写错 → 覆盖度静默回退) | **机器(⑤)** |
| D3 | `docs/implementation/` 快照 | **只新建,不改历史**(append-only);旧证据失效时在 §11 台账补注 | 人验 |
| **D4** | **相关 ADR 的 `## Status Update`** | **ADR 不可变** → 状态变化**只能追加** Status Update 段、不得改正文;**且该 ADR 可能仍写着旧状态**(per Entry #33:`ADR-CEP-00 §8` 第 5 条仍写着归档前的结论)→ **归档后必须追加 Status Update 登记失效**。§4.7.3「① 起步」要求实施者读 ADR → ADR 是**状态入口之一** | **机器(⑤)** |

> **机器兜底**:`cep_docs_sync_check.sh` **第 11 组** —— **6 条**客观可判定断言:
>
> | 断言 | 判定依据 | 覆盖 |
> |------|---------|------|
> | ① **KPI baseline 数字** | `openspec list --json` 实测的 active 数 / 3 **必须**等于所有**可变活文档**「当前/现行/真实/现在/目前/截至 … N/3」的表述 | 数值漂移(与措辞无关) |
> | ② **归档状态互斥** | 非豁免区块内,含**实体锚点**(`umbrella` / `Stage 0` / `R10`)的**句**若同含**旧状态词**词形且不含**当前态词** → 报红。**扫描集含 ADR**(状态入口之一) | 句级矛盾(依赖词形清单) |
> | ③ **冻结成员数字集合** | 机器真值 == 活文档声明的数字**集合**(须恰为 {30}) | 数值漂移 |
> | ④ **旧任务计数** | 活区段不得出现**归档前的旧任务计数对**(具体形态见 `cep_docs_sync_check.sh` 的 `OLDCOUNT`) | 数值漂移 |
> | ⑤ **ADR 的「必须追加 Status Update」回归锁** | 若某 ADR **任意块**(含历史块、含条件式表述)仍写着旧状态,则该 ADR **必须**存在「自身日期 ≥ 归档日」的 `## Status Update` 块登记其失效 | **机器(⑤)** |
> | ⑥ **glob 覆盖** | 归档件在 P6 扫描集内(命中 ≥ 4) | glob 写错 |
>
> **⚠️ 能力边界(不 overclaim —— 逐条写明,不夸大)**:
> - ③ ④ ⑥ 是**数字 / 匹配**级判定,**与措辞无关**;**① 依赖限定词表**(限定词 + `N/3` 形态)—— 只写数值、**不带限定词**的形式则不覆盖;**② ⑤ 依赖词形清单**(锚点 + 旧状态词 / 当前态词)—— 与措辞高度相关。
> - ① 与 ② **都按块级豁免**:ADR 的**归档日之前的块**跳过(正文不可变,历史数值是合法叙述),**归档日及之后的 Status Update 块必须写当前态** → **不跳过**;① 在块内**按行**判定 —— 该行**任一句**含**当前态强限定词**(当前/现行/真实/现在/目前)即按**当前态**处理并检查,**只取该行第一个 `N/3`**(第一个限定词后的第一个数值;同行后续的历史引用不计入,避免对正确文档误报);否则视为**历史叙述**放行。**「截至」不算强限定词** —— 它通常带日期,是历史叙述的标志。
> - ② 依赖**实体锚点** + **旧状态词词形清单**:**清单外的同义改写同样不覆盖** —— 包括**语序反转**(「归档尚未完成」/「未进入归档」)、**中英混写**(「not yet archived」)、**同义替换**(「回执尚未取得」/「意见暂无回复」)。**要求 NLU 不可行**,故这些属**已知可接受缺口**,不是 bug。
> - ② 的**已知绕过面(诚实披露)**:① 同一**分句**内出现任一**当前态词**即该分句豁免 —— 故「旧状态**且**已释放」这种**无分隔符**的写法可绕过(逗号/分号已纳入切分符,但**同一分句内**的当前态词仍优先豁免);② 条件句被**跳过** —— 判定规则是「条件词必须出现在**分句开头**」,可用条件词仅「若/假如/如若/一旦」;**不含 `当`**(「当…时」的中段可为空 → 「当时…」会被吞 —— 故「当时」**不是**条件词),**也不要求**「若…则…」成对(「若」在句首即生效,否则整句照查);**裸字符一律不算**(「规则/原则/准则/否则/当前/当时/当然」都不会被当成条件词);③ **⑤ 不使用**条件跳过 —— 条件式表述的旧结论在 ⑤ 眼里**仍算**旧结论(宁可要求追加 SU);② 块级豁免按**块自身日期**(标题 → h2)判定,正文里**引用**近期日期**不会**改变块的豁免归属。
> - ② **无实体锚点**的孤立状态词也不覆盖(「治理口径的『Proposed 并发 ≤ 1』」等合法表述与之同形,行级无法区分)。
> - ② 的豁免是**区块级**:`###`/`####` 子节**继承**所属 `##` 的豁免(本节自身**刻意不写旧状态词清单本体** —— 写了会被自己的闸抓到,清单本体在脚本里);`Status Update` 按**块自身日期**(标题日期 → 否则 h2 日期 → 否则视为最新)**判定** —— **归档日之前**的块豁免(历史),**归档日及之后**的块**不豁免**(必须写当前态);改标题会留痕。
> - **跨行的语义矛盾仍需人眼**;机器只兜**可客观判定**的 6 类。

> **同步红线**:改 §4.0 任一行 ⇒ **五处同改** —— 详见 §4.0「排期调整规则」(含单源纪律与双边 grep 自检)。

#### 4.7.4 新会话 3 步文档地图

1. **`obj-cep-platform §4.0`** → 我在队列哪一行、什么状态、前置挂在谁身上
2. **`openspec/changes/<name>/tasks.md`** → 还剩哪些 task(**执行真源**)
3. **`docs/adr/ADR-CEP-*.md`** → 为什么这么设计(**不可变**,只追加 Status Update)

> ⛔ **五个常见误读**:
> - **不要**从 `phase-10.md` 的依赖图推断排期 —— 它是**示意**,排期只认 §4.0。
> - **不要**从 improvement 草稿读验收判据 —— 它是**规划输入**;实测回填在 change 的 `proposal.md` §Acceptance。
> - **不要**直接信 `AGENTS.md` 的 KPI 基线数字 —— 那是**带日期快照**(`2026-10-20 实证 = 0 active`),早于本轮变更。当前并发**以 `openspec list --json` 实测 + §5.2 为准**。
> - **不要**直接信任何「N 条 entry」的转述计数 —— 台账是 append-only,计数会持续增长;实测命令 `grep -c "^### Entry #" .rddf/roadmap/objectives/obj-cep-platform.md`。
> - **不要**把 `07 号 §8.1` 的 Stage 表当**排期**依据 —— 该表**只承载「Stage 划分 + change 数」**(14 = 1+10+2+1),**不含**时间跨度与估时(per Entry #27 精简)。**排期只认 §4.0**;Stage 划分与 change 数以 §4.0 / `ADR-CEP-00 §2 Decision 3` 为准。

---

## §5 KPI 预算与治理

### 5.1 KPI 单一真源(per oracle B1 修订)

**治理口径唯一真源**: `ADR-CEP-00 §2 Decision 3` — 本文件 / `phase-10.md` / `cpptlm-cep-umbrella.md` / `roadmap.md` **只引用,不自行重算**(change 总数 / 串行规则 / 并发上限)。

**实施路径唯一真源**: **本文件 §4.0 排队总表** —— 顺序 / 季度 / 状态 / 准入前置归属**只在这里写**(per Entry #23)。`phase-10.md` 的依赖图是**示意**,不承载排期承诺。

### 5.2 当前真实 baseline(2026-10-20 实证,per oracle B1)

`openspec/changes/` 当前 **0 个 active**(2026-10-20 实证, per `openspec list`):
- 原 3 个 active(`cpptlm-msix-generator-ip` 0/44 / `cpptlm-pcie-tlp-entry-layer` 0/56 / `cpptlm-p2-integration-unblock` 0/45)**均 0% 未实施**
- 2026-10-20 三者全部 **⏸ 暂存归档**(`openspec archive --skip-specs`),delta spec **未合并**进 main specs,proposal 已加 ⏸ 标记
- 归档路径:`openspec/changes/archive/2026-10-20-{cpptlm-msix-generator-ip,cpptlm-pcie-tlp-entry-layer,cpptlm-p2-integration-unblock}/`

→ **当前 Proposed 并发 = 0/3**;队列 #7 `cpptlm-cep-cohif-carrier` **已完成归档**(2026-10-28,per Entry #90)⇒ 下一个可启动 change = **队列 #8**(per §5.4 并发 ≤ 1)(2026-10-28 归档:队列 #7 `cpptlm-cep-cohif-carrier` **已完成归档**(完成归档语义 —— 2 份 delta spec 已合并进 main specs:`cep-cohif-carrier` / `cep-protocol-bridge-contracts`;tasks **41/41** 全勾)→ slot 已释放,per Entry #90;历史:2026-10-27 转正:`cpptlm-cep-cohif-carrier`(队列 #7)**已建 change 目录**(proposal + design + tasks + 2 份 delta spec;`openspec validate --strict` rc=0)→ 占 1 slot,per Entry #81;历史:`cpptlm-cep-moesi-gpu-adapter`(队列 #6)**已完成归档**(完成归档语义 —— 14 Scenario 并入 2 份 main spec(`cep-moesi-gpu-adapter` 7 / `cep-adapter-schema-contracts` 7))→ slot 已释放,per Entry #80;历史:2026-10-26 启动:`cpptlm-cep-moesi-gpu-adapter`(队列 #6)**已建 change 目录** → 占 1 slot,per Entry #71;历史:**当时**(2026-10-23)下一个可启动 change = 队列 #5 `cpptlm-cep-collective-plane-base` —— 队列 #5 现已归档;历史:2026-10-26 归档:`cpptlm-cep-collective-plane-base`(队列 #5)**已完成归档** → slot 已释放,per Entry #70;历史:该 change 建目录时曾占用 1 个 slot(per Entry #66/#67);队列 #4 `cpptlm-cep-directory-ordering-plane` 已于 2026-10-25 **完成归档** → slot 已释放,per Entry #64;该 change 建目录时曾占用 1 个 slot(per Entry #62/#63))。历史:2026-10-25 队列 #3 `cpptlm-cep-stub-cache-core` **已完成归档**(完成归档语义 —— delta capability `cep-vc-mapping-contract` / `cep-stub-cache-core` 已合并进 `openspec/specs/`,归档路径 `openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core/`)→ 释放其 1 个 KPI slot,baseline 1/3 → **0/3**;归档依据见 §11 **Entry #60**。历史:2026-10-25 队列 #3 **建目录**占用 1 个 slot → baseline 0/3 → 1/3(per §11 **Entry #58**);历史:2026-10-24 `cpptlm-cep-poc-spike`(队列 #2)**已完成归档**(完成归档语义,delta spec 已合并进 `openspec/specs/cep-poc-spike-verification/spec.md`)→ 释放其 1 个 KPI slot,baseline 1/3 → **0/3**;归档依据见 §11 **Entry #55**。历史:2026-10-24 曾 0/3 → 1/3(该 change 建目录占用 1 个 slot);历史:2026-10-23 `cpptlm-cep-umbrella` **已完成归档**(完成归档语义,delta spec 已合并进 main specs)→ 释放其 1 个 KPI slot,baseline 1/3 → 0/3;归档依据见 §11 **Entry #28**(R10 闭合判据 + 残余风险登记)与 **#29**(归档动作 + 路径迁移 + 冻结面迁移)。**下一个可启动的 change = 队列 #5 `cpptlm-cep-collective-plane-base`**(per §4.0)。

> **✅ M2 准入门放行(2026-10-22)**:ArchForge 侧 coherence 接口**规约文档**已合入
> (commits `6020f42` + `8290b4c` + `ac956b8`,8+3+2 文件),且本仓侧已完成
> **判据 C-4 的机器化**(检查器 + schema delta + fabric 载体,反向验证 12/12 全绿,见 Entry #9)。
> ✅ **Stage 0 已完成归档(2026-10-23)** —— M2 已放行 · R10 已按 **§9.3** 闭合判据处置(per 台账 Entry #28)· tasks **99/99** 全勾;delta spec 已合并进 `openspec/specs/cep-core-contracts/spec.md`(per Entry #29)。

### 5.3 CEP 启动的前置条件(per AGENTS.md §DOC HYGIENE line 422 新提案门槛)

> **创建 `cpptlm-cep-umbrella` 前须先 archive ≥1 个既有 active change**,把 baseline 降到 2/3,创建后回到 3/3。

**✅ 前置条件已满足(2026-10-20)**:3 个 active 全部 **⏸ 暂存归档**(`--skip-specs`),baseline **0/3** → `cpptlm-cep-umbrella` **可立即创建**。

**✅ 已创建(2026-10-21)**:builder 起草完成 —— proposal + design + specs(`cep-core-contracts`)+ tasks **4 件套全落**;baseline **0/3 → 1/3**。**⚠️ `openspec validate cpptlm-cep-umbrella --strict` 实测 rc=1**(2 条预存 WARNING,per Entry #12 ⑦ 实证为**预存不满足**,非本批引入)。**历史**:2026-10-21 Task 8.4 Step 4 曾因跨仓准入门未放行暂回 **Proposed Draft** → 已被 2026-10-22 M2 放行(Entry #10)取代 → 已被 2026-10-23 **完成归档**(Entry #29)取代。**当前状态**:umbrella **已归档**,KPI slot 已释放;当前全局 Proposed 并发 **0/3**(2026-10-28 队列 #7 `cpptlm-cep-cohif-carrier` **已完成归档** → slot 已释放,per Entry #90;历史:2026-10-27 队列 #7 `cpptlm-cep-cohif-carrier` **已转正** → 占 1 slot,per Entry #81;历史:2026-10-27 队列 #6 `cpptlm-cep-moesi-gpu-adapter` **已完成归档** → slot 已释放,per Entry #80;历史:2026-10-26 队列 #6 `cpptlm-cep-moesi-gpu-adapter` **已建 change 目录** → 占 1 slot,per Entry #71;2026-10-26 队列 #5 `cpptlm-cep-collective-plane-base` **已完成归档** → slot 释放,per Entry #70;2026-10-26 队列 #5 **已建 change 目录** → 曾占 1 slot,per Entry #66;2026-10-25 队列 #4 `cpptlm-cep-directory-ordering-plane` **已完成归档** → slot 释放,per Entry #64;队列 #3 `cpptlm-cep-stub-cache-core` **已完成归档** → slot 已释放;队列 #2 `cpptlm-cep-poc-spike` **已完成归档** → slot 释放;该 change 建目录后曾为 1/3);归档后的有效验收命令 = `openspec validate cep-core-contracts --strict`(rc=0)—— 裸 `openspec validate --strict` **恒为 rc=1**(`Nothing to validate`),不可作验收证据。

⚠️ **原候选路径已过期**:"路径 C: archive `cpptlm-gmmu-pte-bus-correction`" —— 该 change **已于 2026-10-12 实际归档**,不再是 active,不能再作为 archive 对象。真实候选是上列 3 个(均已暂存归档)。

**决策须登记 §11 台账**(`kind: go-decision`)。

### 5.4 串行约束

- 任一时刻 **Proposed 并发 ≤ 1**(CEP 系列内部)
- 全仓 Proposed 并发 ≤ 3(AGENTS.md 硬上限)
- 14 个 change 串行 14 季度(per §5.1 真源;2026-10-22 per Entry #12 由 10 → 11;**2026-10-26 per Entry #71 由 11 → 13**;**2026-10-27 per Entry #74 由 13 → 14**)

---

## §6 验收

### 6.1 Stage 0 验收(2026-Q4)

| 来源 | 位置 | 性质 |
|------|------|------|
| `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/proposal.md` | §Acceptance | ✅ **执行真源**(实测值已回填) |
| `.rddf/improvements/cpptlm-cep-umbrella.md` | §Acceptance **A1 ~ A7** | 📥 上游规划输入(**A7 = 前置 gate 汇总**,非 assertion) |
| Stage 1~3 各 change | 各自 OpenSpec AC | 各自的执行真源 |

本文件**不重复复述**逐条 assertion(per oracle S1 修订);分工见 **§4.7**。

### 6.2 Stage 1~3 验收(2030-Q1)

- B1. **14** 个 OpenSpec change 全部 archived with sync
- B2. **66 条守门 invariant 机器检查 100% 覆盖**(编号位 67,按 §0 DoD 7 的 **15 组**分解)(per §0 DoD 7)
- B3. **6 个 ProtocolEngine 实施完成**:CHI RN-F / CXL 2.0 非对称 / CXL 3.0 对称 DCOH / TileLink-C / NVLink / UALink(per metis G2-F 修订:5 协议族 → 6 个 adapter;**族 / 实例计数口径见 `07 号 §5.1.1`「6 族 / 8 实例」与 `ADR-CEP-11 §3` 锁表「按 `protocol_version` 行计 8 行」—— 两种粒度非矛盾,per Entry #19 §2**);每个实例须按 `ADR-CEP-12` schema 声明能力与前提假设
- B8. **Directory & Ordering Plane 三子平面实施完成**:Address Resolver + Sharer Tracker 5 实现 + Ordering Point 三判据 + **装配期通道依赖图无环检查**(per `ADR-CEP-08` + `ADR-CEP-09 Decision A`)
- B4. 4 件套实验输出契约落地 + 13 必纳 metric 全绿
- B5. ModuleFactory 集成 seam + 0 发明宏形式
- B6. simulation_mode 单一维度
- B7. §9 研究问题 8 个假设全部有实验结论

### 6.3 KPI 平衡验收

- D1. 任一时刻 CEP 系列 Proposed 并发 ≤ 1
- D2. §11 决策台账完整记录 **14 季度** KPI 让位
- D3. AGENTS.md KPI baseline 全程 ≤ 3

---

## §7 Reviewers

- **DRI / Owner**:rdd-planner(架构师本人,per `ADR-CEP-00 §2 Decision 3` 单 DRI + 准入门)
- **签发仓**:✅ CppTLM 仓(2026-10-20 拍板,per `ADR-CEP-00 §5.1` — 框架级定位,类比 gem5 Ruby network 之于 gem5)
- **跨仓 reviewer**:ArchForge owner(`06-bis §2.1` ArchForge 设计真源)
- **验收方**:⚠️ **当前无外部 stake**(per metis G4 隐含责任主体盲点)— 见 §8 R5,Stage 0 前须指定

---

## §8 Risks & Kill Criteria

### 8.1 风险

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | KPI 排队延期(遇 hotfix / phase-9 active 让位)| 🟡 中 | §11 台账追踪;**延期 >2 季度触发 §8.2 replan** |
| **R2** | 规范演进返工(CXL 3.0→4.0 / UALink Common 2.x / NVLink 代际 / CHI 版本)| 🟡 中 | 版本锁定由 `ADR-CEP-11` 承担(Stage 1 前签发);spec 变更走 §11 `scope-change` / `adr-amendment` entry |
| **R3** | Stub Cache 性能模型边界(Stage 0 stub 不够真实)| 🟢 低 | Stage 2 后期可补充完整 cache micro(需新 change + KPI slot) |
| **R4** | 多 VC 仲裁推迟(CEP-09 在 Stage 4+)| 🟢 低 | 影响 fabric 性能建模精度,不影响功能正确性 |
| **R5** | ⚠️ **无外部验收方**(DRI = 自己给自己验收)| 🟡 中(**形式已解;实质由 R10 承接处置** —— 2026-10-23 per §9.3) | §9.2 三项角色 2026-10-21 已指定(形式 gate 解除);但三者指向同一人 → 独立性以 **M1 产物冻结 / M2 跨仓准入门 / M3 对拍基准外置 / M4 独立 AI 复审** 四条机制部分替代;**真外部真人评审待建立** |
| **R6** | 与 phase-9 资源竞争(共用 builder 队列 + 执行容量)| 🟡 中 | §8.2 优先级仲裁规则 |
| **R7** | 探索平台结论不可信(无对拍验证)| 🟡 中 | **Stage 1 前须定义对拍手段**(gem5 Ruby / ArchForge 参考模型 / RTL benchmark 三选一) |
| **R8** | ArchForge 单边依赖(准入门把阻塞全转嫁 CppTLM 侧)| 🟡 中 | 需 ArchForge 侧排期承诺;缺承诺时 §9 假设降级为"仅 CppTLM 内部对比" |
| **R10** | **验收独立性实质缺口**(2026-10-21 新增)—— §9.2 三角色均指向单一作者;M1~M4 只是**部分**替代 | 🟢 低(**已接受残余风险**,2026-10-23 per **§9.3**) | **闭合判据已改写** = 「就 **Stage 0 交付范围**(契约层 + 契约测试 + 冻结产物)而言,**M1 / M2 / M4 三项补偿机制均有落地证据 + 残余风险已登记**」—— M1 冻结清单 **30 成员** + 授权闸 · M2 ArchForge 合入回执 · M4 独立 AI 复审(实证抓出 9 项 + 4 轮 critic 审稿抓 3 BLOCKER)。**M3(对拍基准外置)不适用**:它服务**实现期**对拍,Stage 0 无实现交付物 → 保留为 **Stage 1 的 P0 前置**(见 §10)。**残余风险见 §9.3**(结论性独立缺失,由 DRI 书面接受)。**Stage 0 已于 2026-10-23 完成归档**(per 台账 Entry #29)。**M4 不可替代真人外部评审** 这一残余风险依然成立并已登记 |
| **R9** | **单 change 体积上升**(2026-10-21 新增)—— `cpptlm-cep-directory-ordering-plane` 8→12 周(ADR-CEP-08 + 09 Decision A 并入)| 🟡 中 | 同平面合并是模型自洽而非将就;若该 change 延期 >2 季度,走 §8.2 `Replan` 拆 `phase-11`(并评估建 feature fragment)|

#### R10 状态沿革(历史结论 —— 已被 §9.3 取代,保留供追溯)

> 以下为**当时的**结论,**不是现状**;现状见 §8.1 表格的 R10 行 + §9.3。

- **(历史)** **2026-10-21 Task 8.4 结论**:Stage 0 验收等级**降级为「部分独立」**(partial independence)—— M1 冻结 ✅ / M2 准入门 ✅(2026-10-22 放行,per Entry #10)/ M3 对拍基准 ⏳(Stage 1 前定义)/ M4 独立 AI 复审 ✅;**不得表述为「完全独立验收」**。
- **(历史)** **2026-10-23 复审(per Entry #22)**:M2 已消解;R10 仍为阻塞项 → Stage 0 不 archive。
- **(历史)** **2026-10-23 自审(per Entry #23)**:已出具**自审**回执(评审包 §5 **变体 B**,`docs/implementation/2026-10-23-cep-stage0-self-review-receipt.md`)—— 结论「**无阻断项(自审)**」,但**独立性未满足**;当时的解锁条件 = 一份**真正未参与** Stage 0 契约层撰写者的**外部真人**回执(评审包 §5 变体 A)。
- **(历史)** **2026-10-23 处置(per §9.3 + Entry #28/#29)**:该解锁条件**不再是 Stage 0 的前置**(单开发者仓形态下结构上不可达),改为「**引入第二位开发者或任何外部消费者时补做一次独立复审**」的**增量信心**事项;残余风险已登记并由 DRI 书面接受 → **Stage 0 已完成归档**。

### 8.2 Kill / Pivot / Replan 判据(per metis G4 盲点补充)

| 触发条件 | 处置 |
|---------|------|
| **Kill**:Stage 0 完成后(2026-Q4)验收发现 4 件套实验输出无法产出可比较数据(即 §9 假设无法被验证)| 终止 CEP,归档 objective,回退到 `06-bis` 路线 |
| **Pivot**:Stage 1 中期发现协议族选型需变(如放弃 TileLink,改 UALink 优先)| 走 §11 `scope-change` entry + 重排 §4 表 |
| **Replan**:任一 Stage 延期 >2 季度,或全仓 Proposed 并发连续 2 季度无法为 CEP 腾 slot | 触发 §11 `deferral-rationale` entry + 重排 **§4.0 排队总表**的顺序 / 季度 + 评估是否拆分 roadmap phase(phase-11) |
| **Priority 仲裁**(R6):CEP 与 phase-9 同时需要唯一 slot | 默认 **phase-9 优先**(业务交付 > 探索平台),CEP 让位并登记 §11 deferral-rationale |

### 8.3 退出 / 归档判据(per metis G4 盲点 2)

Stage 4+ 进入维护态后,objective 归档条件:
- 全部 14 个 change archived with sync
- §9 假设全部有结论
- §8.2 未触发 Kill / Pivot
- 交接文档完成(CEP 使用手册 → `docs/architecture/07-coherence-exploration-platform.md` 扩充)

---

## §9 研究问题清单(per metis G1 核心盲点)

> **目的**:防止"交付工具但不产出洞见"。每个 change 至少关联 1 个假设;Stage 3 完成时 8 个假设须全部有实验结论。

### 9.1 假设清单

| # | 假设 | 验证手段 | 关联 Stage | 成功判据 |
|---|------|---------|------------|---------|
| **H1** | 协议抽象层(seam 在 L5↔L4)确实让缓存核心免于协议改动 | Stub Cache 0 协议知识 + 全部 adapter 共用同一 stub | Stage 1 | 全部 adapter 只改 adapter,stub 0 diff(**adapter 实例计数口径见 `07 号 §5.1.1`;本表不复述具体数**,per Entry #22)|
| **H2** | 集中目录(CHI HN-F)vs 分布(DCOH)vs 广播的 latency 差异可量化 | `avg_latency_cycles` + `directory_misses` 对比 | Stage 2 | 3 策略延迟差异 > 20% |
| **H3** | Sharer Tracker caps 的 `CONSERVATIVE` 完备性契约在 CXL 对称下无漏 snoop | `deadlock_violations == 0` + `memory_violations == 0` | Stage 2 | 0 违例(1000+ 事务) |
| **H4** | 完成域判别规则(requester-scoped vs group-scoped)足以同时容纳 coherence + collective | 同一实验跑 coherence 流 + collective 原语 | Stage 1~2 | 两类流同实验内无死锁 |
| **H5** | SHARP 5 条防误分类规则(R1-R5)能可靠区分 SHARP atomic vs cache-coherent atomic | `divert_count` 断言 + fail-stop 触发测试 | Stage 2 | 0 误分类 |
| **H6** | VC 隔离(collective 独立 VC)确实避免协议级死锁 | `vc_stall_per_vc` + `deadlock_violations` | Stage 3 | 0 死锁 + collective stall 不占 coherence VC |
| **H7** | stub cache 性能模型(固定命中率 + 固定延迟)足以支撑 early-stage 取舍判断 | 与 gem5 Ruby 或 ArchForge 参考模型对拍(per R7) | Stage 1 | 延迟偏差 < 15% |
| **H8** | 实验 4 件套 + 13 metric 足以让跨协议取舍决策可复现 | A/B 对比报表 + `tools/recommend.py` 输出建议 | Stage 3 | 3 组 A/B 对比产出可读结论 |

### 9.2 消费者与验收方(per metis G1 隐含责任主体)— **2026-10-21 已指定**

| 角色 | 指定 | 角色职责 | 状态 |
|------|------|---------|------|
| **平台验收方** | **ArchForge coherence owner**(`Chi Suhua`,跨仓唯一作者,per `/workspace/main/ArchForge` git 历史 6/6 commit) | 验收 Stage 0 产物(4 件套 + 50 条 invariant 契约全绿 + 16 ADR lint 通过);**验收依据是 `ADR-CEP-00 §2 Decision 3` 的跨仓准入门** —— ArchForge 侧 coherence 接口 PR 未合入,CppTLM change 不升 Proposed | ✅ **已指定** |
| **结论消费者** | **phase-9 dGPU SoC 架构选型链**(ControlHub / physical-ip-port-boundary 决策者,`Chi Suhua`) | 消费 CEP 实验产物做 dGPU SoC coherence 选型;**具体消费点**:ControlHub 的 coherent 通道选型 + `ADR-DGPU-13` PNN 协议的 CEP 侧验证需求 | ✅ **已指定** |
| **平台维护方** | **CppTLM 维护者**(`Chi Suhua`,全仓 1007 commit 主提交者) | Stage 4+ 维护态的协议接入(add-feature + 单 change);schema / lint / 契约演进的所有者 | ✅ **已指定** |

> **✅ 形式 gate 解除**:三项角色**不再是"待指定"**,Stage 0 验收的**角色缺失** No-Go 条件已消除。

#### ⚠️ 独立性缺口(R5 未解除,显式登记为 R10)

**取证事实(2026-10-21)**:ArchForge 仓 git 历史 **6/6 commit 同一作者**;CppTLM 全仓 **1007 commit 同一作者**;phase-9 决策链同一作者。→ **三项角色指向同一人**,**"换一个人验收"这条路径在当前仓形态下不存在**。

**因此**:§8.2 的 No-Go 条件(角色缺失)**已解除**,但 **R5 的实质(独立验收)** **未解除**。**不掩饰这一点** —— 形式满足而实质未达成,是比"干脆不指定"更危险的状态,必须显式登记。

#### 4 条替代独立性机制(补 R5 的实质)

| # | 机制 | 独立性的来源 | 落点 |
|---|------|-------------|------|
| **M1** | **产物冻结先于解读** | 4 件套由 CI 产出并带 **hash**;冻结后**不可改**;解读与冻结分离 —— 即使同一人,也不能改已冻结的产物 | `ADR-CEP-06 §2` 的 4 件套契约 + `scripts/test/cep_docs_sync_check.sh` |
| **M2** | **跨仓准入门(既有)** | ArchForge 侧 coherence 接口 PR **未合入** → CppTLM change **不升 Proposed** —— 制度化的跨仓 checkpoint | `ADR-CEP-00 §2 Decision 3`(已有,本轮确认其**就是**验收方的独立性依据)。**可执行前置**:接口对齐清单 `docs/implementation/2026-10-21-cep-archforge-interface-alignment.md`(13 项 gap 矩阵 + ArchForge 侧 PR 范围 + 6 问 + 5 条放行判据),漂移由 `scripts/test/cep_archforge_alignment_check.sh` 机器检测 |
| **M3** | **对拍基准外置** | H7 的对拍基准(gem5 Ruby / RTL benchmark)是**第三方既有资产**,不依赖本仓维护者的实现 | `objective §9.1` H7 + `§8.1` R7 |
| **M4** | **独立 AI 复审** | `oracle` / `critic` subagent 通道**已被实证可用且有效** —— 2026-10-21 的 Oracle 独立架构复审**实际抓出 9 项作者 4 轮自查均漏掉的问题**(N1~N9),其中 3 项为高 severity | `docs/research/cep-coherence-protocol-space-survey.md §9.9` 为先例 |

> **M4 的实证强度**:它不是"形式上的第二意见",而是**已经证明能发现作者盲区**的通道 —— 本批的 N1/N2/N3(计数与真源口径问题)正是 4 轮自审全部漏掉、1 次独立复审一击命中的。→ Stage 0 产物的**独立复审**应纳入验收清单(per `tasks.md` Task 8.4)。

**净结论(历史表述,现以 §9.3 为准)**:R5 从"🟡 中"降为"🟡 中(**形式已解,实质以 M1~M4 部分替代**)"。**真正的独立评审**待 AI 通道之外的机制(如外部真人评审)建立后再补 —— 这是**已知缺口**。*2026-10-23 起本节由 §9.3 处置:闭合判据改写 + 残余风险登记 + DRI 书面接受;Stage 0 已完成归档。*

### 9.3 R10 处置(2026-10-23)—— 闭合判据改写 + 残余风险登记

> **触发**:DRI 声明 —— 本仓开发者**仅 1 人**,结构上**不存在**「外部真人评审」的执行者(与 ArchForge 6/6 commit 同一作者同构)。**R10 原判据「等外部真人回执」在该仓形态下永远不可达** —— 与 `ADR-CEP-00 §8.1` 当初把准入门 **C-1** 从「接口 PR」改写为「接口规约文档」**同构**:把**结构上不可达**的门禁,改写为**可达且可验证**的判据。
>
> **性质**:这是**治理判据**的修订,**不是**对独立性缺口的否认 —— 缺口由「⛔ 阻塞」转为「🟢 已接受的残余风险」。

**闭合判据(新)**:R10 闭合 ⇔ 就 **Stage 0 的交付范围**(契约层 + 契约测试 + 冻结产物)**而言**,下列 **①~④ 全部成立**;**⑤ 是「不适用」项**(单列,不参与判据):

| # | 条件 | 证据 | 状态 |
|---|------|------|:---:|
| ① | **M1 产物冻结先于解读**已落地 | 冻结清单 **30 成员**(2026-10-23 起 = 29 + 归档后的 main spec)+ SHA-256 + 授权闸(`--update --authorized-by "Entry #N"`)+ provenance 自检 | ✅ |
| ② | **M2 跨仓准入门**已放行 | ArchForge 侧 coherence 接口**规约文档**已合入(`6020f42` + `8290b4c` + `ac956b8`);准入门措辞修订 per `ADR-CEP-00 §8` | ✅ |
| ③ | **M4 独立 AI 复审**已产出**实质发现**(非走过场) | 2026-10-21 Oracle 独立复审抓出 **9 项**(作者 4 轮自审全漏);2026-10-23 载体分工协议经 **4 轮**对抗审稿(REJECT → APPROVE),抓出 **3 个 BLOCKER**(全闭) | ✅ |
| ④ | **残余风险已登记并由 DRI 书面接受** | 本节 + 台账 **Entry #28** | ✅ |
| ⑤ | **M3 对拍基准外置**(**不适用项,不参与上述判据**) | 服务**实现期**对拍(Stage 1+);Stage 0 **无实现交付物** → 保留为 **Stage 1 的 P0 前置**(见 §10) | ⏳ 不适用 |

**残余风险登记(不被「闭合」二字掩盖)**:

- Stage 0 的**契约层结论**未经**独立第二方(人类)复核** —— 由**同一作者**产出、冻结、解读;M1~M4 提供的是**过程性**独立(冻结不可事后改口 / 跨仓时点 / AI 盲区发现),**不是结论性**独立。
- **触发补做的条件**:一旦引入**第二位开发者**或任何**外部消费者**,补做一次独立复审 —— 属**增量信心**,不阻塞已归档契约。
- **M3 不得省略**:Stage 1 起,「对拍基准外置」是**结论性**独立的唯一现实来源,仍是 **P0 前置**(§10)。

**净结论**:R10 的状态由「⛔ 阻塞」改为「🟢 **已接受残余风险**」;R5 的「实质未闭合」措辞为历史描述,**以本节为准**。

---

## §10 Next Sprint Candidates(per `rdd-planner/SKILL.md:177-188` LLM 推荐)

> **注**: LLM **仅推荐**,不自动生成 improvement 草稿(违反 rdd-planner boundary)。rdd-builder P0 起草 proposal 时再激活。

### Stage 0 候选(2026-Q4 启动)

| 优先级 | 候选 | 推进建议 | 前置 |
|--------|------|---------|------|
| ~~P0~~ | 起草 `cpptlm-cep-umbrella/` proposal + design + specs + tasks | ✅ **已完成 2026-10-21**(builder)**并于 2026-10-23 归档**(tasks 99/99) | — |
| ~~P0~~ | 指定 §9.2 三项角色(验收方 / 结论消费者 / 维护方) | ✅ **已完成 2026-10-21**(三项均已指定;形式 gate 解除,实质独立性缺口登记为 R10) | — |
| **P0** | 指定 §9.1 H7 对拍基准(gem5 Ruby / ArchForge / RTL benchmark) | 调研 3 人日 | ⛔ R7 —— **唯一仍未完成的 P0**(per Entry #22 复核)|
| P1 | 起草 Stage 1 **七个** change 的 improvement 草稿 | 走 add-improve HARD-GATE;每 change 5 人日 | Stage 0 archive(**R10 已按 §9.3 处置**,2026-10-23)|
| ~~P1~~ | 起草 `docs/architecture/07-coherence-exploration-platform.md` 顶层真源 | ✅ **已完成 2026-10-21**(architect;**686 行**) | — |
| ~~P1~~ | 起草 `ADR-CEP-11` / `ADR-CEP-14` / `ADR-CEP-15` / `ADR-CEP-07A` / `ADR-CEP-16` | ✅ **已完成 2026-10-21**(architect;**21 份 ADR 全族**) | — |
| ~~P1~~ | 起草 `scripts/test/cep_docs_sync_check.sh` ADR 锚点 lint | ✅ **已完成 2026-10-22**(10 组检查) | — |
| ~~P2~~ | 实验目录 + tools 3 个(render_report / run_sweep / ab_compare) | ✅ **已完成 2026-10-22** | — |

### Stage 1~3 远期候选(2027-Q1 ~ 2030-Q1)

| 优先级 | 候选 | 推进建议 |
|--------|------|---------|
| P1 | `cpptlm-cep-poc-spike` improvement | ⏸ **队列位保留 · 落地形态待裁决**(S2 已实跑 / change 未建,per Entry #22);§4.0 顺序 #2 |
| P1 | `cpptlm-cep-stub-cache-core` improvement | ✅ **已写**(2026-10-25 planner;5 段草稿 → change 目录已建,🟡 Proposed);§4.0 顺序 #3 |
| P1 | `cpptlm-cep-directory-ordering-plane` improvement | 走 add-improve;§4.0 顺序 #4(原 `sharer-tracker-base`,已重排) |
| P1 | `cpptlm-cep-collective-plane-base` improvement | 走 add-improve;§4.0 顺序 #5 |
| P1 | `cpptlm-cep-moesi-gpu-adapter` improvement | 🟡 **已写**(2026-10-26 planner;5 段草稿 → change 目录已建,🟡 Proposed);§4.0 顺序 #6 |
| P1 | `cpptlm-cep-cohif-carrier` improvement | ✅ **已写**(2026-10-27 planner;5 段草稿 + front-matter,4 件套草稿 staged 于 `.rddf/drafts/cpptlm-cep-cohif-carrier/`,change 目录待 KPI slot,per Entry #75/#76);§4.0 顺序 **#7**(2026-10-27 新增,per architect 重审 + Entry #74);**设计输入已备** = `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md` + `ADR-CEP-19`(✅ 已签发) |
| P1 | `cpptlm-cep-chi-rnf-subset` improvement | 走 add-improve;§4.0 顺序 #8 |
| P1 | `cpptlm-cep-mesif-adapter` improvement | 走 add-improve;§4.0 顺序 #9(**R-1 `HierarchicalDirectory` 阻塞**登记,per Entry #71) |
| P2 | `cpptlm-cep-cxl-nonsymmetric` improvement | 走 add-improve;§4.0 顺序 #10 |
| P2 | `cpptlm-cep-tilelink-tree-manager` improvement | 走 add-improve;§4.0 顺序 #11 |
| P1 | `cpptlm-cep-cxl-dcoh-symmetric` improvement | 走 add-improve;Stage 2.1 启动 |
| P1 | `cpptlm-cep-nvlink-sharp` improvement | 走 add-improve;Stage 2.2 启动 |
| P1 | `cpptlm-cep-ualink-inc` improvement | 走 add-improve;Stage 3 启动 |
| P2 | `ADR-CEP-11` 版本锁定(Stage 1 前签发)| 走 architect;触发 R2 缓解 |
| P2 | phase-11 注册后建 feature fragment | 走 rdd-planner 后续评估;≥2 change 跨 phase-10/11 分布时 |

---

## §11 决策台账(append-only,per `rdd-planner/SKILL.md:158`)

> **append-only**:每次新增 entry 追加到段尾;**不可删除 / 不可改写**早期 entry。
> **kind ∈ {deferral-rationale, go-decision, sprint-review, scope-change, adr-amendment}**
> **⚠️ 枚举外标注(per Entry #22)**:Entry #14 的 kind 记为 `[cross-check]`,**不在上述 5 值域内** —— 早期 entry 不可改写(append-only),故就地保留;**Entry #15 起一律用 5 值域**。

### Entry #1 — 2026-10-20 [go-decision]

**主题**: CEP objective 立项 + phase-10 注册 + 10 份 ADR v0.6 达可签发水准
**决策**: ✅ **GO**
**理由**:
- oracle v0.5 评审:"修完后本批 10 份(CEP-00 待签发仓拍板)即可进入 OpenSpec tasks.md 起草阶段"
- 3 轮评审(v0.3 → v0.4 → v0.5 → v0.6)的 8 BLOCKERS + 7 SHOULD_FIX + 6 NITS 全部实质关闭
- **50 条守门 invariant** 命名空间统一(per oracle v0.4 B5)
- 单 phase-10 + 1 objective + 0 feature 决策(phase-11 注册后再评估 feature)
**关联**: `ADR-CEP-00` ~ `ADR-CEP-07` + `ADR-CEP-NUMBER-MAP` + `ADR-CEP-00A` 共 10 份 v0.6
**review_by**: 2027-01-18

### Entry #2 — 2026-10-20 [adr-amendment]

**主题**: 修正 Entry #1 的 KPI baseline 与 invariant 计数表述
**决策**: ✅ **修正**(Entry #1 正文不改,本条追加说明)
**理由**:
- oracle v0.4 路线图评审 B1:Entry #1 沿用了错误的 "baseline 2/3" 表述。**真实 baseline = 3/3 已满**(3 个 active per `phase-9.md`);创建 umbrella 前须先 archive ≥1 个(per §5.3)
- oracle v0.4 路线图评审 B2:Entry #1 写 "19 条 canonical invariant",**真实 = 67 个 invariant 编号位 / 66 条实际守门 invariant**(11 + 7 + 6 + 5 + 7 + 3 + 3 + 3 + 3 + 3 + 3 + 3 + 3 + 3 + 3,`INV-CEP-04-IC4` **唯一跳空** → 实际 66;两批新增自 `ADR-CEP-08`/`09`/`12`/`13`/`11`/`14`/`15`/`16`/`07A` 各 3 条 + `ADR-CEP-07-IC7` + `ADR-CEP-06-IC5`,per §0 DoD 7 + oracle 全面架构审查 N1/N2 + architect 阶段 2026-10-21)
**影响范围**: `roadmap.md` / `phase-10.md` / `cpptlm-cep-umbrella.md` / 本文件已同步修正
**关联**: oracle v0.4 路线图评审 BLOCKERS B1 / B2

### Entry #3 — 2026-10-20 [scope-change]

**主题**: 补研究问题清单(§9)+ Kill / Replan 判据(§8.2)+ 外部验收方(§9.2)
**决策**: ✅ **采纳**
**理由**:
- metis G1:"研究问题(hypothesis)在全部 4 份文档中零出现 — 只定义了实验输出 4 件套(工具),没定义每个 change 要回答什么假设" → **平台会交付工具而不产出洞见**
- metis G4:"无 CEP 失败/退出/中止判据"、"10 季度后怎么办未定义"、"Phase 1~3 全延期无预案"、"无模型可信度验证"、"跨仓依赖单边"
**影响范围**: 本文件 §0 DoD / §8 / §9 新增;`cpptlm-cep-umbrella.md` §Acceptance 同步
**关联**: metis G1 / G4

### Entry #4 — 2026-10-20 [go-decision]

**主题**: KPI slot 腾出 — 3 个 active change **⏸ 暂存归档**(未实施),CEP umbrella 创建前置条件解除
**决策**: ✅ **采纳**
**理由**:
- Entry #2 记录的 "baseline 3/3 已满" 已过期:3 个 active(`cpptlm-msix-generator-ip` 0/44 / `cpptlm-pcie-tlp-entry-layer` 0/56 / `cpptlm-p2-integration-unblock` 0/45)经实测**均 0% 未实施**
- 以"已完成归档"语义归档会**污染 main specs**(把未实现能力写进系统契约)→ 改用 `openspec archive --skip-specs`(**⏸ 暂存归档**),delta spec **不合并**;`openspec/specs/` 保持 85 个 capability 不变
- `AGENTS.md` 同步新增「⏸ 两种归档语义必须区分」条款,明确暂存归档的 4 项硬约束(`--skip-specs` / proposal ⏸ 标记 / 引用同步 / 不进 DoD)
- phase-9 的 **9.C / 9.D / 9.E** 标 **deferred**,四件套保留于 `openspec/changes/archive/2026-10-20-*/`,以后重建:`cp -r` 回 `openspec/changes/<name>/` → 复核实现现状与依赖 → 重新激活
**影响范围**: `AGENTS.md` / `roadmap.md` / `phase-9.md` / `phase-10.md` / `07 号` / `docs/designs/dgpu-soc/{architecture,README}.md` / `ADR-CEP-00` / `cpptlm-cep-umbrella.md` / 本文件 §5.2 §5.3 已同步
**关联**: 用户裁决 2026-10-20(全部 3 个暂存归档 + 显式区分两种归档语义)

### Entry #5 — 2026-10-21 [scope-change]

**主题**: architect 阶段新增 5 份 ADR(`ADR-CEP-08` / `09` / `12` / `13` / `18`)后的 **Stage 重排 + DoD 7 分解 + change 清单更新**;KPI 总数**保持 10 不变**

**决策**: ✅ **采纳**(planner 阶段)

**理由**:
- **不新增 KPI slot** —— 新 ADR 引入的交付物按"**契约层 / 实现层**"分离落位:
  - **契约层(Stage 0)**:`ADR-CEP-12` 的 schema 定义 + `INV-CEP-12-IC1~3` 断言(新增 `10.G` 子任务,3 人日)→ 进 `test_cep_canonical_invariants.cc`
  - **实现层(Stage 1)**:`ADR-CEP-08` 的 Ordering Point 三判据 + `ADR-CEP-09` Decision A 的装配期通道依赖图无环检查 → 并入 `cpptlm-cep-directory-ordering-plane`
- **`ADR-CEP-08` / `09` / `12` / `13` 属 `07 号 §2.2` 的 **Directory & Ordering Plane 三子平面**(Address Resolver / Sharer Tracker / Ordering Point)—— **同平面合并进同一个 change** 而非拆成两个 change,是**模型自洽**的,而非为省 slot 的将就
- **KPI 真源不动** —— `ADR-CEP-00 §2 Decision 3`(总 10 个 change / 串行 10 季度)**不修订**,避免治理层变更;若 Stage 1 的 `directory-ordering-plane`(12 周)超出承受,走 §8.2 `Replan` 判据拆 `phase-11`
- **`ADR-CEP-13` 的一致性域组合能力** —— 承诺边界为"**可做,但走人工声明 + 实验证伪,不承诺自动化**"(per `ADR-CEP-17` 决策 9 / `ADR-CEP-13` 决策 5)→ **不进任何 Stage 的 DoD**;其实现归 Stage 2+
- **`ADR-CEP-18`** —— Stage 1~3 **不依赖任何协议生成器**(per 决策 1)→ 不产生新 change

**影响范围**:`phase-10.md`(真源 3 表 10→16 份 / Stage 1 依赖图 / 估时 30→41 人日 / 评审闭环)+ 本文件 §0 DoD 7 分解 / §4.1 / §4.2 / §4.5 / §6.2 B2+B8 / §10 + `cpptlm-cep-umbrella.md`(依赖 + 架构依据 + 文件表)

**风险登记**:§8.1 建议新增 **R9 —— 单 change 体积上升**(`directory-ordering-plane` 8→12 周)。缓解:Stage 1 中期若该 change 延期 >2 季度,走 §8.2 `Replan` 拆 `phase-11`(评估 `phase-11` 注册后建 feature fragment)

**关联**: architect 阶段 2026-10-21 修订(新增 5 份 ADR / 修订 8 份)+ oracle 全面架构复审 N1~N9

### Entry #6 — 2026-10-21 [sprint-review]

**主题**: §9.2 三项角色(验收方 / 结论消费者 / 平台维护方)**正式指定** —— Stage 0 验收的角色缺失 No-Go 条件解除

**决策**: ✅ **形式指定完成**;⚠️ **独立性未闭合**,登记为 **R10** 并给 4 条替代机制

**理由**:
- **取证**:ArchForge 仓 git 历史 **6/6 commit 同一作者**(`Chi Suhua`);CppTLM 全仓 **1007 commit 同一作者**;phase-9 决策链同一作者 → **三项角色指向同一人**
- **形式 gate 解除**:§9.2 三项**不再是"待指定"**,§8.2 的"角色缺失 → No-Go"条件消除
- **实质缺口显式登记**:R5 的实质(独立验收)**未解除**;"换一个人验收"在当前仓形态下**不存在** → 登记 **R10**
- **4 条替代机制**(部分替代,非等效):**M1** 产物冻结先于解读(CI 产出带 hash)/ **M2** 跨仓准入门(既有,`ADR-CEP-00 §2 Decision 3`)/ **M3** 对拍基准外置(gem5 Ruby / RTL benchmark)/ **M4** 独立 AI 复审
- **M4 的实证先例**:2026-10-21 Oracle 独立架构复审**实际抓出 9 项作者 4 轮自查全部漏掉的问题**(N1~N9),3 项高 severity → 该通道**已被证明能发现作者盲区**,非形式性第二意见

**影响范围**:本文件 §9.2(新增 4 条替代机制表)/ §8.1(R5 状态 + 新增 R10)/ §8.2 无需改;`cpptlm-cep-umbrella/proposal.md` 的 ⛔ 残留 gate / `spec.md` 组合承诺边界场景 / `tasks.md` Task 8.4 同步

**未闭合**:真外部真人评审机制待建立(不在本轮范围)

### Entry #7 — 2026-10-21 [sprint-review]

**主题**: Task 8.4 的 **5 项验收前置**执行结论 —— M4 独立 AI 复审(Step 2)/ M1 产物冻结(Step 3)/ M2 跨仓准入门(Step 4)/ R10 独立性残余缺口(Step 5)

**决策**: ⚠️ **Stage 0 验收等级 = 「部分独立」(partial independence)**;**⛔ 不得 archive** —— M2 准入门**未放行** + R10 独立验收**未闭合**

**理由**:

**Step 2 — M4 独立 AI 复审:✅ 已执行(2 轮)**

| 轮次 | 通道 | 覆盖 | 结论 |
|------|------|------|------|
| 第 1 轮 | `general`(架构) | 6 个架构问题(契约层边界 / 解析器可靠性 / 15 组划分 / schema 完备性 / 生态类比与 R10 / 架构级缺口) | 抓出解析器形态盲区(注入 `**不变量 \`INV-CEP-04-IC8\`**:` → 7 case 全绿)、字段组口径 4 处不一致、C10 零覆盖、`ADR-CEP-05` 数字与枚举自相矛盾等 |
| 第 2 轮 | `explore`(代码对抗式,默认立场 REJECT) | 6 个代码审查问题(假阴性 / lint 假阴性面 / 解析器边界形态 / owner 前缀歧义 / 工具行为边界 / 断言强度 vs spec) | 抓出 **6 个 BLOCKER**(B1 冻结面 CI 恒绿 / B2 一致性闸门三工具全不生效 / B3「全仓」名不副实 / B4 07 号第 3 列未校验 / B5 tools 从不执行 / B6 `ab_compare` 缺 `import html`)+ 约 15 项 SHOULD_FIX |

- **通道说明**:`oracle` / `critic` 已从 agent 注册表消失(`Agent not found`)→ 改用文档保证可用的 `general` / `explore` 承担;**独立性强度**因此**弱于**原生 `oracle`,已如实登记。
- **处置**:6 个 BLOCKER **全部**修复 + 15 项 SHOULD_FIX 中 15 项修复 + 4 项实质弱断言(S3/S12/S13/S16)实质化;**10 组反向验证**(故意破坏 → 必须红)全部有牙;处置台账落 `docs/implementation/2026-10-21-cep-stage0-contracts.md §7`。
- **M4 实证强度再次验证**:两轮复审共抓出 **21 项**作者 4 轮自审全漏的问题(其中 6 项 BLOCKER)→ 通道**确能发现作者盲区**,非形式性第二意见。

**Step 3 — M1 产物冻结:✅ 已落地(机器可执行)**

- Stage 0 尚无实验 4 件套(那要 Stage 1)→ M1 在 Stage 0 的对应物 = **规范性契约产物**冻结:**28 个产物**(21 ADR + `07 号` + 2 schema + 3 tools + umbrella `spec.md`)。
- 产物:`docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`(逐件 SHA-256 + 冻结基线 commit)
- 校验器:`scripts/test/cep_freeze_check.sh`(`--update` 重新生成);已接入 `cep_docs_sync_check.sh` **第 7 组检查** + 独立 ctest `cep_freeze_check`
- **反向验证**:改一份冻结 ADR → freeze checker 红 + lint `--strict` exit=1;恢复 → 绿 ✅
- ⚠️ **两个 checker 脚本自身不冻结**(checker 必须可修,且不承载契约语义 —— 承载语义的是被它检查的产物)

**Step 4 — M2 跨仓准入门:⛔ 未放行(取证)**

- 条款:`ADR-CEP-00 §2 Decision 3`「ArchForge 协议接口 PR 未合入前,CppTLM 对应 OpenSpec change 不得从 Proposed Draft 转 Proposed」
- 取证:`/workspace/main/ArchForge` git 历史 **6 commit、无 PR / merge 痕迹**;coherence 相关产物**只有设计文档**(`09-coherence-protocol.md` 514 行 / `ADR-SOC-01` 66 行 / `coherence-{bridge,domain,protocol}.md` 1017 行)→ **无「接口 PR 已合入」证据**
- **连带纠正**:`cpptlm-cep-umbrella` 状态由「🟡 Proposed」**降级为「🟡 Proposed Draft」**;`tasks.md` 同步;objective §5.2 baseline **1/3 → 0/3**(Proposed slot 只统计已达 Proposed 的 change;该 change 仍在 10-change 串行队列内)
- **放行条件**:ArchForge 侧 coherence 接口 PR 合入,且与 `09-coherence-protocol.md §5.2 关键 API` 对齐 —— 本 change 的 `protocol_instance.schema.json` 的 `ordering_assumption` / `vc_binding` / `sharer_tracker` 属**接口形状**,存在与 ArchForge 侧冲突的可能 → 由 DRI 人工 gate 放行并记本台账 entry

**Step 5 — R10 独立性残余缺口:✅ 已显式登记**

- Stage 0 验收等级**降级为「部分独立」** —— M1 冻结 ✅ / M2 准入门 ⛔ / M3 对拍基准 ⏳(Stage 1 前定义)/ M4 独立 AI 复审 ✅
- **不得表述为「完全独立验收」**;M4(AI 复审)**不可**替代真人外部评审
- 形式 gate(§9.2 三项角色已指定)✅ 解除,实质 gate(独立验收)**未解除**

**影响范围**:`openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/{proposal.md,tasks.md}`(状态 + gate ④)/ 本文件 §5.2(baseline)+ §8.1(R10 缓解栏)/ `docs/implementation/2026-10-21-cep-stage0-contracts.md` §7(处置台账)/ `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`(新)/ `scripts/test/cep_freeze_check.sh`(新)/ `scripts/test/cep_docs_sync_check.sh`(第 7 组)/ `test/CMakeLists.txt`(ctest 注册)

**未闭合**:① M2 准入门(等 ArchForge 接口 PR)② R10 真人外部评审机制 ③ M3 对拍基准(Stage 1 前定义)④ Stage 0 **不 archive**

**关联**:Task 8.4 Step 2~6;`docs/implementation/2026-10-21-cep-stage0-contracts.md §7`(6 BLOCKER + 架构复审关键项 + 10 组反向验证)

### Entry #8 — 2026-10-21 [adr-amendment]

**主题**: M2 准入门判据 **C-1 措辞**修订(「接口 PR」→「接口规约 PR」)+ ArchForge 答复回执(R-1 ~ R-13 逐条裁定)

**决策**: ✅ **采纳**(Entry #7 正文不改,本条追加说明 + `ADR-CEP-00` 追加 `## §8 Status Update`)

**理由**:

**C-1 措辞修订(治理级,阻塞 M2 永久化)**

- ArchForge 侧答复 `ADR-SOC-25` D11 + `32 号 §2 F8` 的取证:ArchForge 是**纯设计仓**
  (`git log` 全为 `docs(...)` 提交,无 `src/`、无 `include/`)→ **结构上产不出「接口 PR」**
  → 现行措辞下 C-1 **永远无法闭合** → M2 准入门**永久阻塞**
- 修订:C-1 由「ArchForge 侧存在**已合入的** coherence 接口 **PR**(有 commit hash)」
  改为「ArchForge 侧存在**已合入的** coherence **接口规约文档**(有 commit hash)」
  (形态 = `ADR-SOC-25` + `32 号` + roadmap 答复表);**实现型 PR 归 CppTLM 仓**(per §2 决策 1 代码归口)
- ⚠️ **措辞变更不放行 M2** —— 截至 2026-10-21 实测,ArchForge 三份答复文档**尚未提交**
  (`M docs/roadmap/cep-interface-alignment.md` + `?? ADR-SOC-25` + `?? 32 号`)→ C-1 **仍红**

**ArchForge 答复回执(A1)**

- ArchForge 已就提问单 6 问给出书面结论(`roadmap` v0.2 + `ADR-SOC-25` + `32 号`);
  我方**独立复核**:代码级断言 **6/6 属实**(`CoherenceProtocol`/`CoherenceBridge`/`register_cache_callback` 全 0 命中;
  `cache_tlm.hh` 的 `snoop|coherence` 计数 = 0)、契约级引用 **14/14 可追**、
  2 份协议实例声明**经复跑 schema 校验通过**(2/2)
- R-1 ~ R-13 **13 项逐条裁定**:无一项被拒;11 项可直接进执行,2 项需先补一轮
  (R-2 的判据 (b) 论证 / R-3 的 scope 形态)
- 落点修正 **2 处**:R-2 的「`perm` 改对象数组」会击穿 `test_cep_schema.cc:190-198` 的 4 值锁结构断言
  → 改为「`perm` 保持纯枚举数组 + 旁增顶层字段」;R-13 的锁表增行**连带 2 个冻结产物 + 计数 14→16**
- 附带登记 **D-9 → 本仓 CI 风险**:21 个 ArchForge 锚点中 **20 个**指向 `ADR-SOC-25 §3.1` 返工位置
  → D-9 与本仓锚点重生成**必须同批**

**影响范围**: `docs/adr/ADR-CEP-00-platform-charter-and-governance.md`(§8 Status Update)/
`docs/implementation/2026-10-21-cep-archforge-response-receipt.md`(新增)/
`docs/implementation/2026-10-21-cep-archforge-interface-alignment.md`(§3 答复注 + §6 判据状态 + §8 风险)/
`docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`(重生成,因 `ADR-CEP-00` 属 M1 冻结集)

**未闭合**: ① C-1 仍红(等 ArchForge 提交三份规约文档)② C-4 仍部分(R-5 network 清单归 ArchForge /
R-6 消息类词表 / R-3 scope 形态)③ R-2 判据 (b) 待 ArchForge 补 ④ A4 的 4 产物 adr-amendment 待 DRI 拍板
⑤ D-9 未落地,锚点风险未消 ⑥ Stage 0 **不 archive**
⑦ **独立复审通道不可用** —— 本环境 subagent(`oracle`/`critic`/`context-builder`)全部返回 `Invalid token`
(与 Entry #7 记录的通道掉线一致)→ 本轮回执为**自审 + 23 组可复现取证**,**R10 残余未闭合**,
不得表述为「已独立验收」

**追记 2026-10-21(ArchForge 提交 `6020f42` 后复核)**:

- ✅ **C-1 已闭合** —— commit `6020f42`(8 文件 / +1415 行 / 工作区干净);
  提交内容与回执复核对象**逐字一致**;**D-9 未落地**(9 份返工文件一份未动)→ 本仓 33 锚点仍全绿
- ✅ **C-4 判据文本已满足** —— 13/13 顶层属性在 ArchForge 侧有承载物 + 2 份声明 schema 校验通过(2/2)
  → **5 条判据全部满足** → M2 阻塞解除,`cpptlm-cep-umbrella` 可由 DRI gate 放行
  (Proposed Draft → Proposed);**Stage 0 仍不 archive**(R10 未闭合,与 M2 无关)
- 🔴 **提交复核新发现 2 个 P0**:N-1 Compute NoC 的 VC 分配两套互斥(`08 §4.3` vs `ADR-SOC-25` D8,
  VC1/VC2/VC3 全冲突)· N-2 `vc_binding` 无 network 维度而 VC 分配是 per-network 的
  → `INV-CEP-12-IC2` 在协议实例声明内**无解析面**;两者**不得随 M2 放行一并放过去**,须挂 Stage 1 准入前置
- 🟡 **新发现 2 个 P1**:N-3 R-6 词表(5 类)漏列 `snoop_resp`(声明实用 6 类)· N-4 `ADR-SOC-25` 仍为「📋 Proposed」
- **行动**: A7 M2 放行裁决(DRI)· A8 发第二轮请求 N-1~N-4 · A9 发 D-9 锚点对照表

**追记 2026-10-22(ArchForge `8290b4c` + `ac956b8` 后复核)**:

- ✅ **N-1 ~ N-4 全部裁决完毕并回写** —— 本方 2 个 P0 **被采纳**:
  N-1 根因 = D8 把两个 fabric 压成扁平表 → 改 **per-network 矩阵**(`32 号 §6.0.2`,
  `08 §4.3` 四槽不变、`25-nsa` 补 VC3);N-2 裁决 **选项 A(加 network 维度)** + 给出 diff 级 **schema delta**(`32 号 §6.4`)
- ✅ **Q-2 已交付** —— `22-nsa §0.5.4` 网络命名空间(5 网络 + 每网络 `vc_count` + NS-R1/NS-R2);
  升级为 **R-5'**(fabric config 提供 `networks`)+ **R-5''**(`ordering_assumption` 升 per-(network, vc_id))
- ✅ **D-9 批 1 已落地** —— 只动**不含锚点**的 2 份(22-nsa + snoop_filter)→ 本仓 **33 锚点仍全绿**;
  批 2(09 + coherence-{protocol,domain,bridge},含**全部 20 锚点**)留待与本仓锚点重生成同批
- 🔴 **新发现 3 项**:M-2 **C-4 声称「可自动化」但检查器从未实现** → 对方已切嵌套声明、我方 schema 仍扁平,
  实测**各 4 处失配且无 CI 报红** · M-3 **我方无 fabric config 载体**(R-5' 无消费端) ·
  M-1 **R-6' 附带项「扩 `CATEGORY_*` 到 6 类」不可接受**(词表混层,本方裁定不扩)
- 🔶 **Q-3 仍未交付**(R-2 判据 (b),2 轮未答)→ A6 继续阻塞
- ⚠️ **M2 放行建议收紧**:由「放行」改为「**先 A10(schema delta)+ A12(检查器),再放行**」——
  C-4 是 5 条判据中唯一声称可自动化的一条,在检查器缺失 + schema 与声明已失配时放行 = 用人工核验给
  未实现的自动化结案(per R10 防护意图);若仍决定立即放行,须把 A10/A11/A12/M-1 显式写成 Stage 1 准入前置
- **行动**: A10 采纳 schema delta · A11 新增 fabric config 载体 · A12 新增第 9 组检查器 ·
  A13 提交本回执(闭合 ArchForge V-8)· A14 答复 R-6' 附带项 · A15 催 Q-3

**冻结清单说明**: `cep_freeze_check.sh --update` 已重生成(`ADR-CEP-00` 的 SHA-256 变化);
清单的 `frozen_at_commit` 字段当前指向**变更前** HEAD(本次改动尚未 commit)→ **commit 时需再跑一次 `--update`** 校正该字段;
`--update` 的合法前提是**本条台账 entry**(即「授权记录」),hash 校验本身不区分授权与未授权修改。

**关联**: ArchForge `ADR-SOC-25`(D11)/ `32 号 §2 F8` / `roadmap/cep-interface-alignment.md §5,§6` ·
回执 `docs/implementation/2026-10-21-cep-archforge-response-receipt.md`

### Entry #9 — 2026-10-22 [adr-amendment]

**主题**: 契约收敛批 1 —— 采纳 ArchForge 的 per-network schema delta,让放行判据 **C-4 从「人工满足」变成「机器满足」**

**决策**: ✅ **采纳**(形态采纳 + 范围按 oracle「最小充分集」收窄 + 按 metis 边界补齐)

**理由**:

**触发**: ArchForge `8290b4c`(N-1~N-4 裁决 + `32 号 §6.4` schema delta)+ `ac956b8`(D-9 批 1)。
我方 N-1/N-2 两条 P0 被采纳(N-1 = per-network 矩阵;N-2 = 选项 A「加 network 维度」)。
C-4 的**判据文本**已满足(13/13 字段有承载物),但**机器层红** —— 对方声明已切嵌套形态、我方 schema 仍扁平,
实测每份 4 处失配,而**无任何 CI 会报红**(检查器从未实现)。

**① 变更的冻结产物(6 件 + 1 件新增冻结,逐个 + 为什么)**

| 产物 | 改动 | 为什么 |
|------|------|--------|
| `include/tlm/cep/protocol_instance.schema.json` | `ordering_assumption` / `vc_binding` 各嵌一层 network 维度 | 采纳 ArchForge `32 号 §6.4.1` 的 diff 原文(N-2 选项 A) |
| `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` | 追加 `## 8 Status Update`:决策 5 / 决策 6 / `INV-CEP-12-IC2` **取代** + provisional 标注 + SLICC 备选登记 | 形态变更 + 理由措辞修正(oracle §2.3) |
| `docs/adr/ADR-CEP-08-ordering-point-portability.md` | 追加 `## 8 Status Update`:决策 6「key 粒度」+ 抓取点 4 **取代** | 旧文「解析到**唯一** vc_id」这一谓词在嵌套形态下**整体消失** —— 不改则两份 ADR 对同一字段给出两个 fail 判据 |
| `docs/adr/ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md` | 追加 `## 8 Status Update`:图顶点 `(协议实例, vc_id)` → **`(协议实例, network, vc_id)`** | **数学对象变更**(非措辞):两张网共享 vc_id 值域 0-3,顶点不带 network ⇒ 跨网依赖边被合并 ⇒ 无环判据可能给错误 verdict |
| `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md` | IC2 scenario **直接改写**为逐网络校验(spec 是机器校验面,非 ADR) | spec 是判据 C-4 的机器可断言面 |
| `docs/adr/ADR-CEP-17-ruby-slicc-protocol-family.md` | 追加 `## 8 Status Update`:决策 8 的 schema 草图 / §4 模板 / §6 invariant 建议 **取代** | **2026-10-22 代码复审 B6**:该 ADR 仍持「`named_network` 必须解析到唯一 `vc_id`」旧谓词 → 冻结集内 `ADR-CEP-12`(新)与 `ADR-CEP-17`(旧)给出**两个不同 fail 判据** |
| `include/tlm/cep/fabric.schema.json` | **新增并纳入 M1 冻结** | **2026-10-22 复审 S4**:fabric 载体按「**契约面 / 数据面**」拆分 —— `fabric.schema.json`(**类型描述** = 契约)必须冻结,否则 `vc_count.maximum: 4` 可被静默放宽;`fabric.json`(**实例数据** = 派生副本,权威在 ArchForge)**不冻结**,Stage 1 首个 implemented 实例跑过后再纳 |

**新增产物(5 件)**: `include/tlm/cep/fabric.schema.json`(类型 —— **已纳入 M1 冻结**,见上)·
`include/tlm/cep/fabric.json`(实例 —— **不冻结**,标注 `derived_from` = ArchForge `22-nsa §0.5.4.1`,**非独立权威** per NS-R2)·
`scripts/test/cep_decl_alignment_check.sh`(5 项子校验)· `scripts/test/cep_decl_reverse_verify.sh`(反向验证,**入库以便复现**)·
`.github/workflows/ci.yml` 增 pip 行(**前置到 ctest 之前**且覆盖所有 matrix 腿,per 复审 NB3)。

**② RUN 证据(非 skip,2026-10-22 实测)**

```
  ✓ [RUN] ArchForge 协议实例声明 ↔ 本仓 schema/fabric 对齐检查
    · 声明源: /workspace/main/ArchForge/docs/architecture/32-cep-based-coherence-architecture.md §6.1 / §6.2
    · 声明份数: 2 份 —— moesi_gpu, mesif
    · ① 声明 ↔ schema 嵌套结构: ✓      ② vc_id < vc_count + 键对称 + key 解析: ✓
    · ③ ordering ⊆ vc_binding(逐网络): ✓  ④ 消息类 ∈ 6 类规范词表: ✓
    · ⑤ network ⊆ fabric ⊆ 已锁定集 + vc_count 一致: ✓
  ✓ [RUN] 声明对齐检查通过:5 项子校验全绿(2 份声明,2 个网络)
```
跳过态(供对照,证明「跳过」与「跑过」可区分):
```
  · [SKIP] ArchForge 仓不在本环境(/nonexistent)—— 跨仓声明对齐无法在本仓校验
```

**③ 反向验证(12 条,全绿 —— 证明检查器有牙,per M-2 教训)**
**脚本已入库**:`scripts/test/cep_decl_reverse_verify.sh`(可复现)

| # | 破坏动作 | 结果 |
|---|---------|:----:|
| 1 | `fabric.json` 的 `compute_noc.vc_count` 4→2 | ✅ RED(槽位上界) |
| 2 | `fabric.json` 增 ArchForge 清单外的网络 `nsa_switch` | ✅ RED(反向比对) |
| 3 | 声明的 `vc_binding` `vc_id` 3→9 | ✅ RED(schema `maximum:3`) |
| 4 | 声明的 `ordering_assumption` 删掉 `ualink_fabric` | ✅ RED(网络键对称) |
| 5 | 声明的 `vc_binding` 消息类写成 `forward_data` | ✅ RED(6 类词表) |
| 6 | 声明的 `ordering_assumption[n]` 内层 key 写成 `high_pri` | ✅ RED(`int()` 解析失败不崩栈) |
| 7 | 声明的 `family` 改成非法枚举值 | ✅ RED(**13 个 required 属性的枚举校验**) |
| 8 | 声明的 JSON 非法 | ✅ RED(**解析失败不得静默丢弃**) |
| 9 | ArchForge 权威网络清单文件缺失 | ✅ RED(**反向守卫不得静默关闭**) |
| 10 | 基线(未破坏) | ✅ GREEN |
| 11 | 全部恢复(本仓 + 真仓) | ✅ GREEN |
| 12 | 副本恢复 | ✅ GREEN |

> 破坏 3~9 作用于 **ArchForge 文档副本**(`ARCHFORGE_DIR` 指向临时副本),**真仓零改动**。
> 第 7~9 条是 **2026-10-22 代码复审**新增的 —— 它们分别钉住「11 个 required 属性零校验」
> 「解析失败静默丢弃」「反向守卫静默关闭」三个**闸门可被绕过**的缺口。

**④ 遗留声明**

- **白名单**:本批**无任何白名单** —— `protocol_version` ∈ `ADR-CEP-11 §3` 锁表的校验**整项不做**
  (属 A4 批次;校验器内嵌白名单 = M-2 假阴性家族的同款结构,故不做而非「写好白名单」)
- **`07 号` 零 diff** ✅ —— 6 类消息类词表**并入 `INV-CEP-12-IC2` 条件 ③**,**不另立 `INV-CEP-12-IC4`**:
  另立编号会连带触发 **4 处 invariant 计数同步**(`07 号 §4` 总表 + `objective-cep-platform` DoD 7 的
  15 组分解 + `improvement A2` + `phase-10` 交付预期)+ canonical invariant 用例数 —— 超出本批范围。
  invariant 总数维持 **66 实际 / 67 编号位**。
  ⚠️ 该决策在实施中被 lint 第 2 组**实测抓出**(若另立 IC4,lint 会红),已按「最小足迹」裁决。
- **`fabric.*` 不进 M1 冻结集**(零实例新产物不诞生即冻结;Stage 1 首个 implemented 实例跑过后再纳冻)
- **provisional 债**(Stage 1 必须承接):N-1/N-2 的实现侧复核 · SLICC vnet 备选重评触发条件
  (第 3 网/第 3 实例)· `fabric.*` 进冻结集 · `ADR-CEP-09` 图顶点按 `(network, vc_id)` 建图
  **之前不得用其无环判据放行任何协议实例**
- **本条 entry 的 `adr-amendment` 授权范围** = 上表 ① 的 6 件冻结产物 + ②~④ 的新增/变更
- **2026-10-22 代码复审后新增的 3 项机制**(均为「让 M1 的授权与 provenance 可核验」):
  ① `cep_freeze_check.sh --update` **须带 `--authorized-by <台账 entry id>`** 并核对该 id 确实存在于台账 §11 ——
     台账 entry 即**授权记录**,不再靠「自觉」;
  ② 冻结清单新增 `authorized_by` 字段 + `frozen_at` 改为**取自 git**而非硬编码;
  ③ 冻结校验新增 **provenance 自检**:逐个比对「清单 hash」与「`frozen_at_commit` 处该产物的内容」,
     不一致即**响亮报出**(警告不阻断 —— 「先 commit 再 --update」的两步流程会让它天然落后 HEAD 一个 commit)

**影响范围**: `include/tlm/cep/{protocol_instance.schema.json,fabric.schema.json,fabric.json}` ·
`docs/adr/ADR-CEP-{08,09,12}-*.md` · `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md` ·
`scripts/test/cep_decl_alignment_check.sh` · `scripts/test/cep_docs_sync_check.sh`(新增第 9 组) ·
`test/CMakeLists.txt`(新增 ctest `cep_decl_alignment_check`) · `test/test_cep_schema.cc` ·
`docs/implementation/2026-10-21-cep-archforge-response-receipt.md` · 本文件 §11

**未闭合**: ① C-1 仍红(等 ArchForge 提交三份规约文档)② C-4 机器层现已满足,待 M2 gate ④ 采信本条 RUN 证据
③ Q-3(R-2 判据 (b))仍未交付 → A6 阻塞 ④ A4(R-13 锁表 + 计数 14→16)待批 ⑤ D-9 批 2 的 20 个锚点待同批重生成
⑥ 独立复审通道本轮**可用**(oracle + metis + explore 三方评审已执行),但**真人外部评审仍缺**(R10 未闭合)
⑦ **构建环境**:本机 `/usr/bin/g++-14` 缺失(工具链迁至 `~/.bashrc` 的 `/workspace/main/opt/bin`),
已恢复软链 → `build/` 可用

> 📌 **第 2 轮(2026-10-22)的对外动作与 M-4 修复**见 **Entry #11**(独立成条 —— 含代码改动 + 机器契约)。

**关联**: 计划 `.opencode/plan/20261022-cep-contract-convergence-batch1.md`(v2,经三方评审修订) ·
回执 `docs/implementation/2026-10-21-cep-archforge-response-receipt.md` §6/§8.3 ·
ArchForge `8290b4c` / `ac956b8` / `32 号 §6.0.2` / `32 号 §6.4` / `22-nsa §0.5.4.1`

### Entry #10 — 2026-10-22 [go-decision]

**主题**: M2 跨仓准入门**放行** —— `cpptlm-cep-umbrella` 由 🟡 Proposed Draft 升 🟡 **Proposed**

**决策**: ✅ **GO**(DRI 人工 gate;判据 C-1 ~ C-5 全部满足)

**理由**:

| 判据 | 证据 | 结论 |
|------|------|:----:|
| **C-1** | ArchForge 侧接口**规约文档**已合入:`6020f42`(8 文件 / +1415 行)+ `8290b4c`(ADR-SOC-25 → **Accepted** + N-1~N-4 裁决)+ `ac956b8`(D-9 批 1);工作区干净 | ✅ |
| **C-2** | P0-1 ~ P0-5 逐项结论 —— 载体 = `ADR-SOC-25` + `32 号`;我方逐项签收见回执 §3 | ✅ |
| **C-3** | 6 问各有书面结论 —— ArchForge `roadmap/cep-interface-alignment.md §5` 答复表 + `32 号 §10`;我方逐条签收见回执 §4/§5 | ✅ |
| **C-4** | schema ↔ 声明双向映射**已机器化** —— `cep_decl_alignment_check.sh` 5 项子校验全绿(2/2 份声明)+ **反向验证 12/12**;RUN 证据见 Entry #9 ② | ✅ |
| **C-5** | 全量回归 2026-10-22 实测:`ctest` **94/94** · `ctest -R cep` **16/16** · CEP lint **9/9 组** · M1 冻结 **29/29** · 锚点 **33/33** · 全仓路径 **0 缺失** · 失效链接 **115**(= 基线) | ✅ |

**连带变更**: `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/{proposal.md,tasks.md}` 状态 🟡 Proposed Draft → 🟡 **Proposed**;
objective §5.2 baseline **0/3 → 1/3**(升 Proposed 占 1 个 KPI slot)。

> ⚠️ **risk-accept(显式记录,非默认放行)**:
> 本次放行的独立复审由 **oracle + metis + explore 三方 AI agent** 完成(批 1 修订 12 处),
> **不等于真人外部评审** —— **R10(独立性缺口)未闭合**。
> 本仓三项角色(验收方 / 结论消费者 / 平台维护方)仍**指向同一人**。
> → 故 **Stage 0 仍不 archive**,且 `tasks.md` 已把 5 项 **Stage 1 准入前置**(S1-1 ~ S1-5)落成可勾选条目。

**未闭合**: ① **R10** 真人外部评审(→ Stage 0 不 archive)② **Q-3**(R-2 判据 (b))→ A6 阻塞
③ **A4**(R-13 锁表 + 计数 14→16)待批 ④ **D-9 批 2** 的 20 个锚点待同批重生成
⑤ `fabric.*` 待 Stage 1 首个 implemented 实例跑过后再纳冻结集

**关联**: Entry #9(批 1 的授权记录 + RUN 证据 + 反向验证)· 回执 `docs/implementation/2026-10-21-cep-archforge-response-receipt.md` ·
`openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/proposal.md` 前置 gate ④

### Entry #11 — 2026-10-22 [go-decision]

**主题**: 第 2 轮**对外动作批** —— A14 正式答复 + A15 催 Q-3 + P-2 PoC 用例规格 + **M-4 修复及其机器化**

**决策**: ✅ **GO**(DRI 批准零决策成本批;含 1 处代码注释修正 + 1 组新 lint)

**理由**:

> 本批的定位:**不碰任何冻结产物**的对外动作 —— 只把「已定的裁定」变成「可交付的答复」,
> 把「已写的规格」变成「可评审的文档」,把「已发现的缺陷」变成「已修 + 机器强制」。

**① 对外交付(载体 = 回执 v1.1)**

| 产出 | 位置 | 说明 |
|------|------|------|
| **A14 正式答复** | 回执 **§8.4** | 4 条**已定**(A14-1~4)+ 4 条**待确认**(A14-C1~C4)+ **M-4 互斥论证**(§8.4.3) |
| **A15 催 Q-3** | 回执 **§8.5** | **Q-3a~e 五问**;含 **Q-3e**(= P-3)「以 PoC 的 S2 场景**实验判定**替代 (b) 纸面评估」 |
| **P-2 PoC 用例规格** | `docs/implementation/2026-10-22-cep-poc-spike-spec.md` | S1/S2/S3 的输入 / 期望 / 判定 + 4 条硬边界;**不含实现** |

> **Q-3e 是破局设计**:前 2 轮 Q-3 悬空,因 (b) 被当成**纸面论证题**;
> S2 把它变成**可执行的观测题** —— 观测值(读到旧值/新值)即 (b) 的答案。
> 贵方接受 → Q-3 不再阻塞 A6;不接受 → 仍请答 Q-3a。

**② M-4 修复(2026-10-16 critic 已记载但未修 → 本轮重发现 + 修 + 机器化)**

**缺陷**:`include/tlm/cluster/soc_internal_noc.hh:68-71` 的 `flit_category → VC` 注释块描述了一个
**不存在的 4 类** `CATEGORY_` 集(引用 `CATEGORY_DAT`,全仓 **0 命中**),且称 `CATEGORY_CREDIT → VC_SNP`;
而实际按**索引对齐**落到 **`VC_DAT`**。**代码与测试一致**(`test/test_soc_internal_noc_data_flow.cc:107`
断言 `vc_counts_[2]` 增)→ **注释是错的**。该注释正是 R-6' 附带项设想形态的「**幽灵实现**」。

| 项 | 处置 |
|----|------|
| **注释** | ✅ **已修正** —— 改为描述真实映射(`VC_DAT` 标注为「索引对齐的副产物」;`VC_SNP` 标注「当前无类别落入」)+ 显式写出**索引对齐约束**与其失效模式 |
| **行为** | ⚠️ **不变** —— 改行为会改变计数语义并击穿既有断言;`vc_dat_count()` 实际统计的是 **credit 流量** —— 作为**遗留项**登记(见下) |
| **防复发** | ✅ **新增 lint 第 10 组**「传输层 `CATEGORY_*` 分层纪律」(`scripts/test/cep_docs_sync_check.sh`):<br>① `CATEGORY_*` 恰为 3 个声明,且值**可静态求值**且为 0/1/2 连续 —— **不可求值即判红**(堵住表达式绕过)<br>② flit 类别消费点(13 文件)引用的常量**必须真实存在** → **封死「幽灵类别」**<br>③ `VC_COUNT` 与「4 独立 VC」物理模型声明一致 |

**反向验证(4 条,均实测 RED → 恢复 GREEN)**:
① 把 `CATEGORY_*` 扩到 4 类 → ① 红(「实测 4 类,值域 [0 1 2 3]」);
② 在 `soc_internal_noc.hh` 注释里引用不存在的 `CATEGORY_DAT` → ② 红(「幽灵类别」);
③ 写 `CATEGORY_DAT{3};`(花括号初始化)→ ① 红(旧版只认十进制 `= N`,**会漏检**);
④ 写 `CATEGORY_DAT = CATEGORY_CREDIT + 1;`(表达式初始化)→ ① 红(「不可静态求值 → 判红」,旧版**静默漏检**);
⑤ `using namespace bundles;` + 裸 `CATEGORY_GHOST`(新消费点)→ ② 红(旧版作用域启发式**漏检**)。

> ③~⑤ 是 **2026-10-22 critic 复审实测的 3 个绕过点**,已全部堵死并复验。
> 残留局限(明示):② 覆盖**注释与代码**(M-4 的幽灵类别正在注释里)→ 解释性注释若提及不存在的类别会**误报**;
> 作用域为「flit 类别消费点」启发式(非全仓)—— 刻意收窄,因 `CATEGORY_*` 在本仓另有命名空间
> (`include/core/error_category.hh` 的 `CATEGORY_HH`)。

> **这使 A14-1 / A14-2 从「承诺」变成「契约」** —— 「6 类词表不落传输层」现在有机器强制,
> 而不只是一句对外答复。(per M-2 教训:口径无机器强制 → 静默失真)

**③ RUN 证据(2026-10-22 实测)**

```
  ✓ [10/10] 传输层 CATEGORY_* 分层纪律(对应 A14-1 / A14-2)
    ✓ CATEGORY_* 恰为 3 类且值域连续 0..2(传输层类别;6 类一致性词表不落本层)
    ✓ flit 类别消费点(12 个文件)无「幽灵类别」(M-4 防复发)
    ✓ VC_COUNT = 4 与「4 独立 VC」物理模型声明一致
  ✅ CEP 契约同步检查全部通过(10/10)     · ctest 94/94     · ctest -R cep 16/16
  ✅ 冻结 29/29                            · 全仓路径 0 缺失 · 锚点 33/33
```

> ⚠️ **lint 9/9 → 10/10** —— 上表 9/9 是 **M2 gate 当日(第 1 轮)**的实测值(Entry #10 C-5 保持原值不改);
> 本轮新增第 10 组。`ctest -R cep` 仍为 **16/16**(本轮未新增 ctest)。

**④ 遗留声明**

- **M-4 的行为侧**:`CATEGORY_CREDIT` 按索引落入 `VC_DAT` 桶 → `vc_dat_count()` 实际统计 credit 流量。
  改它需要**单独决策**(credit 是否该独占 VC / 映射是否应改为显式表而非索引对齐)→ **登记为 Stage 1 前置**,不在本批。
- **`Q-3` 仍未交付** → A6 继续阻塞;`Q-3e` 的答复决定 A6 是否能由实验裁定。
- **`P-1`**(PoC 登记为 Stage 1 第 7 个 change)、**D1 / D2**(A4 路线与是否同时排期)、**D5**(R10 最小动作)—— **待 DRI 拍板**
  (决策简报:`.opencode/plan/20261022-cep-decision-brief.md`)。
- **计划落点修正**:T2(A4)的落点清单经第 2 轮全量 grep 复核,由 17 项修正为 **22 项**(15 🔴 冻结 + 7 🟢 非冻结;
  冻结产物 7 → **8** 份)—— 详见计划 §3.1 的「第 2 轮实测复核」表。
- **两个预存文档缺陷**(与本批无关,顺手修):`docs/implementation/README.md` 的 2 处失效链接 +
  `docs/README.md` 的 `implementation/` 文档数(3 → 11)与 3 处失效链接(含 `archived/` → `archive/` 目录名错)。

**影响范围**: `include/tlm/cluster/soc_internal_noc.hh`(注释)· `scripts/test/cep_docs_sync_check.sh`(新增第 10 组)·
`docs/implementation/2026-10-21-cep-archforge-response-receipt.md`(§8.4 / §8.5 / §10.2,版本 v1.0 → v1.1)·
`docs/implementation/2026-10-22-cep-poc-spike-spec.md`(新增)· `docs/implementation/README.md` ·
`docs/README.md` · `.opencode/plan/20261022-cep-remaining-batches.md` · 本文件 §11

**未闭合**: ① **R10** 真人外部评审(→ Stage 0 不 archive)② **Q-3**(R-2 判据 (b))→ A6 阻塞
③ **A4**(R-13 锁表 + 计数 14→16)待 DRI 拍板(D1 / D2)④ D-9 批 2 的 20 个锚点待同批重生成
⑤ `fabric.*` 待 Stage 1 首个 implemented 实例跑过后再纳冻结集
⑥ M-4 的**行为侧**(credit 计入 `VC_DAT` 桶)待 Stage 1 前置决策
⑦ **本批冻结产物零改动** → 无需 `adr-amendment`、冻结清单未重生成(`frozen_at_commit` 仍指向 `97ecbf9d`,provenance 自检 29/29 一致)

**关联**: 回执 §8.4 / §8.5 / §10.2 · PoC 规格 `docs/implementation/2026-10-22-cep-poc-spike-spec.md` ·
计划 `.opencode/plan/20261022-cep-remaining-batches.md` · 决策简报 `.opencode/plan/20261022-cep-decision-brief.md`

---

### Entry #12 — 2026-10-22 [adr-amendment]

**主题**: A4 批 2 —— R-13 锁表 + 族/实例计数口径(14 → 16)+ **P-1**(PoC 登记)+ **D5**(R10 最小动作)

**决策**: ✅ **采纳**(D1 = **路线 B** · D2 = **仅改注册表** · D5 = **选项 3** · P-1 = **登记**)

**理由**:

**触发**:台账 **Entry #9** 遗留的「③ A4(R-13 锁表 + 计数 14→16)待批」;
DRI 于 2026-10-22 拍板 4 个决策(决策简报 `.opencode/plan/20261022-cep-decision-brief.md`),
执行本条。**D1 = 路线 B** 的含义:A4 先行 + `ADR-CEP-05` 判据 2 **限定注记** + A6 登记为 **Stage 1 前置**
—— 债**显式化**而非消除,以换取**立刻解锁** ArchForge 侧的装配期校验(`INV-CEP-11-IC1`)。

**① 变更的冻结产物(8 份,逐个 + 为什么)**

| 产物 | 改动 | 为什么 |
|------|------|--------|
| `docs/adr/ADR-CEP-11-protocol-version-locking.md` | **决策 2 表 + §3 表各 +2 行**(`moesi-gpu-1.0` / `mesif-1.0`)+ 补记「两表须同步」+ §7 计数 6 → 8 | `INV-CEP-11-IC1` 要求每个协议实例的 `protocol_version` 落在锁表内 —— `moesi_gpu` / `mesif` 登记进 §5.1 后**必须**入表,否则**装配期 fail** |
| `docs/architecture/07-coherence-exploration-platform.md` | §5.1.1 表 **+2 行** + 标题「6 族 / 8 实例」+ `:74`/`:310`/`:323`/`:364`/`:659` 五处计数 + 族计数口径补记 | 锁表使用规则 2:表中不得含未列入 §5.1 的协议族 → **必须**同批登记 |
| `docs/adr/ADR-CEP-05-stub-cache-core-interface.md` | **正文不改**;追加 **§13 Status Update**(判据 2 的限定 + 状态 + 未变更项明示)+ §7 判据 2 下加限定注记 | 路线 B 的**核心**:`moesi_gpu` 恰有静默 E→M → 原「当前不触发」陈述的**适用范围收窄**;判据 2 的**状态**仍待 (b) |
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | `:165` 计数同步 + **§2 决策 3 排队 10 → 11**(Stage 1: 6 → 7,新增 `cpptlm-cep-poc-spike` **置首位**)+ 增补段 | 计数同步 + **P-1** 的排队登记 |
| `docs/adr/ADR-CEP-00A-real-source-anchor-map.md` | `:169` 计数同步 | 真源映射表 |
| `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` | `:39`(计数漂移风险行)+ `:147`(决策 9 理由句)两处计数同步 | 计数口径的 2 个落点 |
| `docs/adr/ADR-CEP-18-protocol-generation-scope.md` | `:68` 计数同步(含「为 14~18 个实例」→「为 16~20 个实例」) | 生成路线的投入产出论证依赖该数 |
| `docs/adr/ADR-CEP-17-ruby-slicc-protocol-family.md` | `:338`「14 adapter」→「16 adapter」 | 协议族接入的 schema 字段理由句 |
| `docs/adr/ADR-CEP-15-cachetlm-alignment.md` | **正文不改**;追加 **§8 Status Update**(「adapter 数 ≤ 14」阈值已越过的限定)+ 决策 4 末条下加限定注记 | 2026-10-22 **critic 复审发现**:该句是 `ADR-CEP-05:227` **同款活矛盾**(计数相关且已被越过),若不同等处置则口径不一致 |
| `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md` | `:310`「锁定表**恰好 6 个协议族**」→「**恰好 8 个**」+ 补 2 个族名 | 2026-10-22 **critic 复审发现(漏网)**:该文件在 `cep_freeze_check.sh` 的 `FROZEN_GLOBS` 内 —— 是**第 9 份**冻结产物;回执 R-13 早已要求「umbrella `spec.md` 同步」,本批初版漏做。它与 `ADR-CEP-11 §3`(增至 8 族)直接矛盾,且 freeze 校验**只比 hash 不比语义** → 无 checker 能发现 |

> **「表可增,论断不改」原则(本条确立)** —— **表/登记类**(`ADR-CEP-11 §3` 锁表、`07 号 §5.1.1` 实例表)
> **正文直接增行**(表的可增长性即其契约的一部分);**表内计数的一致性陈述**(如 §7「6 个协议族」)**同批机械同步**;
> **独立论断**(如 `ADR-CEP-05:227` 的「当前不触发」)**正文不改**,追加 `## Status Update` 限定。
> 判据:该句**是否随表机械变化**。

**② 落点清单(17 份文件 = 10 份冻结 + 7 份非冻结;关键处数 40)**

| 范围 | 文件 |
|------|------|
| **🔴 冻结产物(10 份)** | `ADR-CEP-00` · `ADR-CEP-00A` · `ADR-CEP-05` · `ADR-CEP-11` · `ADR-CEP-12` · `ADR-CEP-15` · `ADR-CEP-17` · `ADR-CEP-18` · `07 号` · **`umbrella spec.md`** |
| **🟢 非冻结(7 份)** | `docs/architecture/README.md` · `interface-alignment.md`(G10 行 + CI 锚点块)· `research/cep-coherence-protocol-space-survey.md`(2 处注记)· `umbrella/proposal.md` · `umbrella/tasks.md`(计数 + S1-6)· `test/test_cep_schema.cc` · `test/test_cep_contracts_crosscheck.cc` |
| **⚠️ 计划落点清单漏网(2 份)** | ① `umbrella spec.md` —— **第 9 份冻结产物**,回执 R-13 早已要求同步<br>② `umbrella/proposal.md:89` —— 与 `tasks.md:107` 同 change 的另一处计数<br>③ `ADR-CEP-15` —— 计数相关的活矛盾(critic 复审发现)<br>④ `test_cep_schema.cc` 的 **§5.1.1 行数断言**(6 → 8)+ `test_cep_contracts_crosscheck.cc` 的**族循环**(6 → 8)—— **静默失去覆盖**(不红,但断言失效) |
| **计划行号漂移(1 处)** | `test/test_cep_schema.cc` 的硬编码锁表取值:计划写 `:417`,实际 **`:484-485`** |

**③ RUN 证据(2026-10-22 实测 —— 两步 commit 流程完成后回填)**

> ✅ **已回填真实输出**。两步流程:① commit 产物 `da501fc9` → ② `--update --authorized-by "Entry #12"`
> → ③ commit 清单。清单的 `frozen_at_commit` = **44dc623d**(产物所在 commit 的后继,
> 含全部 10 份冻结产物的内容)→ **provenance 自检 29/29 一致** → 非「变更前 HEAD」的假 provenance。

```
  ✓ 授权校验通过:台账 entry 'Entry #12'
  ✓ [cep_freeze_check] 已重新生成冻结清单: 29 个产物
  ✓ provenance 自检通过:29 个产物的 hash 与 commit 44dc623debf5 的内容一致
  ✓ M1 产物冻结校验通过:29 个产物 hash 全部一致(冻结于 2026-10-22, commit 44dc623debf5)
  ✓ [10/10] 传输层 CATEGORY_* 分层纪律(对应 A14-1 / A14-2)
    ✓ CATEGORY_* 恰为 3 类且值域连续 0..2(REQUEST RESPONSE CREDIT;传输层类别,6 类一致性词表不落本层)
    ✓ flit 类别消费点(13 个文件)无「幽灵类别」(M-4 防复发)
    ✓ VC_COUNT = 4 与「4 独立 VC」物理模型声明一致
  ✅ CEP 契约同步检查全部通过(10/10 组)
  ✓ ctest 全量:94/94 · ctest -R cep:16/16
  ✓ 声明对齐检查:5 项子校验全绿(2 份声明,2 个网络)
  ✓ 对齐清单漂移检测通过:33 个锚点全部有效(ArchForge 21 / CppTLM 12)
  ✓ 全仓路径引用:0 缺失
  ✓ openspec validate cpptlm-cep-umbrella --strict:PASS
```

> **两步 commit 流程**(per 计划 §6 AC-8 / 2026-10-22 复审 B4 的教训):
> **先 commit 产物** → `--update --authorized-by "Entry #12"` → **再 commit 清单**。
> 否则清单的 `frozen_at_commit` 指向**变更前** HEAD → provenance 为假 → 自检报出。

**④ 遗留声明**

- **A6 仍是未闭合项** —— 判据 2 的状态(已触发 / 仍未触发)取决于 `INV-CEP-13-IC2` 的 (b);
  已登记为 **Stage 1 前置 S1-6**;输入 = ArchForge **Q-3**(第 3 轮催问含 **Q-3e**:以 PoC S2 实验判定替代纸面评估)
  **或** `cpptlm-cep-poc-spike` 的 S2 观测值 —— **二者取其一即可闭合**
- **`ADR-CEP-05:227` 的正文保留原样**(14 个 / 当前不触发)—— 这是**受限定注记保护的历史陈述**,不是残留;
  grep 会命中它属**预期**(per 计划 §6 AC-5 的「仅剩历史/Status 引述」口径)
- **D5 的摘要已产出**(`docs/implementation/2026-10-22-cep-stage0-external-review-summary.md`,1 页),
  但 **R10 仍未闭合** —— 缺**外部真人**的「无阻断项」回执 → **Stage 0 仍不 archive**
- **PoC 规格已先行,实现未启动** —— `docs/implementation/2026-10-22-cep-poc-spike-spec.md`;
  **循环依赖已澄清**(per 规格 §1.2 item 3):**S2 必须可先行**(它产出 A6 所需的 (b) 观测值),
  故该 change 置于 Stage 1 **首位**
- **排队长度 +1,并发不变** —— 11 个 change 串行 11 季度;「任一时刻 Proposed 并发 ≤ 1」不变
- **本条 entry 的 `adr-amendment` 授权范围** = 上表 ① 的 8 份冻结产物 + ② 的非冻结落点 + P-1 的排队变更

**影响范围**: 上表 8 份冻结产物 · `docs/architecture/README.md` ·
`docs/implementation/{2026-10-21-cep-archforge-interface-alignment.md,2026-10-22-cep-stage0-external-review-summary.md}` ·
`docs/research/cep-coherence-protocol-space-survey.md` ·
`openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/{proposal.md,tasks.md}` · `test/test_cep_schema.cc` ·
`docs/implementation/2026-10-22-cep-poc-spike-spec.md` ·
`.rddf/roadmap/objectives/obj-cep-platform.md` §4.2 / §11 · `.rddf/roadmap/phases/phase-10.md` ·
`docs/implementation/2026-10-21-cep-archforge-response-receipt.md` · 本文件 §11

**未闭合**: ① **R10** 真人外部评审(→ Stage 0 不 archive)② **Q-3**(R-2 判据 (b))→ A6 阻塞
③ D-9 批 2 的 20 个锚点待同批重生成 ④ `fabric.*` 待 Stage 1 首个 implemented 实例跑过后再纳冻结集
⑤ **M-4 行为侧**(credit 计入 `VC_DAT` 桶)待 Stage 1 前置决策 ⑥ **`moesi_gpu` / `mesif` 未排期**(登记 ≠ 排期,D2 决策)
⑦ ⚠️ **`openspec validate cpptlm-cep-umbrella --strict` = rc 1 —— 批次前即如此**,2 个预存 WARNING:
「spec.md 头部元数据块不被 archive 携带」「`## ADDED Requirements` 与首个 Requirement 之间有文本」。
**已实测排除本批责任**:在批次前的 commit(`5b3f2ecd`)上跑同样命令 → **rc=1 + 同样 2 个 warning**;
把本批改过的 `spec.md` 换回去再跑 → **仍 rc=1 + 同样 2 个 warning**。
修复需改 **spec 结构**(冻结产物)→ **另起决策**,不在本批。**AC-7 因此为「预存不满足」而非本批回归**

**⑤ 2026-10-22 收尾发现的**引用错误**(对外文档质量,已修)**

> **`ADR-CEP-05 §10` → `ADR-CEP-05 §7`**(判据 2 的实际所在节)。
> §10 是「关联 ADR / OpenSpec」;**判据 2 在 §7「α 失效判据」**(`### α 失效判据 2`,正文 `:223-225`)。
> 该错误**从初始 commit 即存在**(`7195f7f7` 起 §7/§10 就已是这两个编号)→ 属**原始笔误**,
> 已扩散至 **24 处**(回执 / ADR-CEP-05 §13 / PoC 规格 / tasks.md / 3 份计划文件)。
> **为什么必须修**:它出现在**要发给 ArchForge 的对外文档**里(回执 §8.5 的 **Q-3c** 直接引用该节号)
> → 对方按图索骥会找到「关联 ADR / OpenSpec」而非判据 2。
> **发现方式**:准备「ArchForge 需回应什么 + 该读哪些文件」的答复时**逐节核对引用**。
> **教训**:对外文档的**节号引用**必须与**行号引用**同等校验 —— 本仓有锚点机制管**行号**,
> 但**节号**无任何机器校验(24 处错了 3 个 commit 都没红)。
> 修复在同批(`Entry #12` 授权范围内);**建议**:Stage 1 起为对外文档加「节号引用 ⇔ 目标节标题」的 lint。

**⑥ 上述「建议」的落地:节号引用 lint 已实施(2026-10-22,不等 Stage 1)**

> **工具**:`scripts/test/cep_doc_ref_check.sh` + `cep_doc_ref_aliases.tsv` + `cep_doc_ref_registry.tsv`
> (ctest 名 `cep_doc_ref_check`;`--strict` 有问题返非零)。
> **两层互补设计**(经 critic 复审重构,原单层设计有 1 个 BLOCKER —— 覆盖度断言被证伪):
> - **[3] 自动检测** —— 「`<别名> §N`」的**目标节必须存在**。来源 = 文档自身派生,
>   **无漏登记**问题;抓得住节号错别字 / 别名解析错文件 / 目标删节。
>   ⚠️ 抓不住「`§10` 存在但语义指错」—— 那正是原始 bug 的形态。
> - **[4] 语义断言** —— 目标节**标题必须含人工写定的关键词** + 文档侧**字面量仍在**(防漂移)。
>   **只有这一层能抓住原始 bug 类**。
>   ⚠️ 作者纪律:`kw` **必须以目标文件的真实标题为准**,绝不可照抄文档字面量 ——
>   照抄笔误文档登记 = 精确复刻原始事故(已在 TSV 头写死该警告)。
> **已知边界(如实记录,勿高估)**:`lit` 是**存在性**而非**计数性**校验 ——
> 同一节号出现 N 次、只改 M<N 次时不红;但**统一**写错(原始事故形态)必红。
> **别名表必须显式登记**:实测 glob `NN-*.md` 会把 `07 号` 错解成 ArchForge `07-memory-system.md`
> (文档实指本仓 `07-coherence-exploration-platform.md`)——**错解比不解更危险**。
> **同批交付**:`docs/implementation/2026-10-22-cep-archforge-response-checklist.md`
> (给 ArchForge 的「待回应清单」,10 条开口项 + 最小阅读路径 + 回执格式)。

**关联**: Entry #9(批 1 授权记录)· Entry #11(第 1 轮对外动作 + M-4)·
计划 `.opencode/plan/20261022-cep-remaining-batches.md` §3.1 / §5 Step 3 ·
决策简报 `.opencode/plan/20261022-cep-decision-brief.md` ·
回执 `docs/implementation/2026-10-21-cep-archforge-response-receipt.md` §8.4 / §8.5

---

### Entry #13 — 2026-10-22 [adr-amendment]

> **授权**:本条**授权** A6 收敛所需的 ADR 修订(判据 2 状态 + 锁表行 + 应用口径注记)。
> **触发**:ArchForge 已应答我方「待回应清单」**10 条全部交付**
> (`docs/roadmap/cep-interface-alignment-response-2026-10-22.md`,commit `4ff2fa6`)——
> **Q-3 阻塞解除**,`ADR-CEP-05 §7` 判据 2 的状态可收敛。
> **范围**:仅 ADR **Status Update / 登记类表行**的增量修订;**判据定义与不变量集合零改动**。

**① Q-3 闭合(3 轮催问后交付)**

| # | 问 | 对方答复 | 关键落点 |
|---|----|---------|---------|
| **Q-3a** | `moesi_gpu` 的 `E→M` 是否接受「导致数据值响应的转发请求」? | **是** —— 判据 (b) **成立** | 对方 `32 号 §5.2.1`:判据 (b) 是**析取**(`s1` 或 `s2`);`E` 不成立(干净独占,snoop 响应 `ACK` 不带数据)但 `s2 = M` **成立**(脏独占,snoop 响应 `DATA_AND_ACK` **携带数据值**)→ 析取一支为真即整体成立 |
| **Q-3b** | 若 (b) 不成立,是否仍要求 `perm` 加判别位? | **前提不适用**((b) 成立);**R-2 保留并升格为「已触发」** | 对方补充**约束**(本方采纳,见 ③) |
| **Q-3c** | 判据 2 状态由哪一侧标注? | **由 CEP 侧** —— 写进 `ADR-CEP-05 §13` | 对方**不复制**状态值(防双真源) |
| **Q-3d** | 2 实例进锁表的时点是否需与判据 2 收敛同步? | **可独立**(追认 A4) | 附**路径判定**问(见 ②) |
| **Q-3e** | 是否接受以 PoC S2 实验判定替代 (b) 的纸面评估? | **接受** | 但要求 S2 定位为「**确认 + 回归门**」而非「裁决」,并追加**观测口径约定**(见 ④) |

**② Q-3d 附问「路径 A/B」—— 本方独立判定(接受结论,修正标签)**

> 对方判定为 **`ADR-CEP-11 决策 4` 路径 A**(仅锁表更新 / adapter 代码不动 / 不升版本号),
> 并**请本方确认**。本方**独立**核对后判定:

- **✅ 接受实质结论**:`protocol_version` **不升版**(`moesi-gpu-1.0` 保持)。
  - `perm` 是 **CEP 侧** Crossbar Interface 投影词汇表(`ADR-CEP-02 CEN-Ib11`),
    **不是**协议规范的一部分;`protocol_version` 按 `ADR-CEP-11 决策 1` **只承载规范版本**。
  - `moesi-gpu-1.0` 声明的协议语义(6 态 × 6 事件 / snoop 型 / SoC 内)**完全未变**。
  - 故「不引入 `moesi-gpu-1.1`」+「adapter 关注的语义子集不变」**成立**。
- **⚠️ 但「路径 A」这个标签本方不采用**:
  - `ADR-CEP-11 决策 4` 的三条路径是「**规范版本升级**的处置路径」。
    R-2 落地**不构成规范版本升级** → **决策 4 不适用**(既非 A 也非 B),
    故其「决策登记」义务**不触发**(本条 Entry 已覆盖该登记需求)。
  - 若强行套用路径 A 的**动作**判据(「adapter 代码不动」)→ **不成立**:
    按 Q-3b 的补充约束,`perm` 产出方**必须**能区分静默/显式升级 → adapter/plugin 侧**必然有代码变更**。
    (现状:`perm` **仅在 schema 中定义,无任何 C++ 实现** → 该变更整体属 **Stage 1**。)
- **净结论**:**可观测后果与对方完全一致**(版本号不动 / adapter 语义子集不动 / 不占 adapter change slot),
  差异**仅在标签与登记方式** → 本方记为「**决策 4 不适用;R-2 归 CEP 侧 schema 演进,落 Stage 1**」。

**③ R-2 升格 + `perm` 形态约束(对方 Q-3b 补充,本方采纳)**

- R-2 由「潜在表达力缺口,尚未证实触发」升为「**已触发**」;**不撤回**。
- **`perm` 新字段必须表达「本行的 `UNIQUE` 投影可静默升为 `WRITEBACK」这一具体语义**,
  而**不是**一个泛化的「可升级」布尔** —— 因 `O→M`(需 `INVALIDATE→others`,发外部通信)是**显式**升级,
  二者对跨域传播的触发条件**不同**。
- 落点:Stage 1 的 **CEP 侧 schema 演进**(`ADR-CEP-12` + `protocol_instance.schema.json`),
  **本批不改 schema**(Stage 0 契约面冻结 + `perm` 值域 4 值锁 `CEN-Ib11` 不动)。

**④ S2 观测口径(对方 Q-3e 追加,本方采纳)**

S2 观测记录由「观测值 + 传播路径可达性」扩为**三件**:
① **观测值**(新值 / 旧值)② **数据来源**(D-GPU 的 `M` 态 cache 的数据响应 / 内存 / 无)
③ **跨域传播路径可达性**。
第 ② 项是对方本轮追加 —— snoop 型协议下「读到新值」既可能来自 `M` 态的**数据响应**(b 的直接证据),
也可能来自 `M + INVALIDATE` 的**写回**;二者**证据强度不同**,不区分则观测值无法唯一定位责任侧。
**S2 角色**:由「裁决」降为「**确认 + 回归门**」(Stage 1 前跑一次)。

**⑤ 本方独立发现的一处分句缺口(对方未提,本方主动登记)**

`ADR-CEP-05 §7` 判据 2 的「当前不触发」有**两个**分句:
①「§5.1.1 / §5.1.2 的 14 个 adapter 实例均无「静默 E→M」需求」
②「该判据在 `ADR-CEP-08` 的**组合检查落地后**才可能被触发」

- ① 已因 `moesi_gpu` 登记而失效(Entry #12 限定注记已处理)。
- ② **仍未满足**:`ADR-CEP-08 决策 4` 的组合检查器是 **Stage 1 实现**(未落地);
  且 `moesi_gpu` 状态 = **未排期**(`07 号 §5.1.1`)→ 尚无任何组合会包含它。
- ⇒ **判据 2 的状态 = 「已触发」**(唯一状态值,基于 (b) 成立);
  ② 记为**执行排期约束**(判据 2 触发的**后续动作**——重新评估 (β)——排在组合检查器落地的同批 = **Stage 1**),
  **不**作为判据 2 的第二个状态维度(避免把「何时跑检查器」抬成「判据处于何状态」)。
- 依据:`ADR-CEP-08 决策 4` 规定组合检查**必含** E 态静默升级项(缺项 = fail)→
  ② 约束的是**动作的执行时点**,不改变判据本身的成立与否。
- ⚠️ 对方建议的三值词表(`已触发` / `仍未触发` / `待实验裁定`)**未涵盖**② 这一执行排期约束 →
  本方在 `ADR-CEP-05 §13.1` **另设「后续动作与排期」行**记入,**不**混入状态字段。

**⑥ 早期 entry 的状态表述 —— 声明其效力边界(append-only 纪律)**

> 本节 §11 台账是 **append-only**:**不可删除 / 不可改写**早期 entry。
> 故 Entry #8 / #9 / #10 / #11 / #12 中「Q-3 仍未交付 → A6 阻塞」等表述**按其落笔时点为真**,**一律不改写**。
> **效力规则**:早期 entry 描述的是**该 entry 落笔时点**的状态;
> **当前状态一律以最新 entry 为准**(本条 Entry #13 即当前状态的唯一权威)。
> 读台账判断「现在是什么状态」时,**必须**读最后一条 entry,不可从历史 entry 反推。

**⑦ 落点清单(本条 Entry 授权范围)**

| # | 文件 | 改动 | 性质 |
|---|------|------|------|
| 1 | `docs/adr/ADR-CEP-05-...md` **§13.1** | 判据 2 状态 → **已触发**(分两层记);(b) 状态 → 已交付;落地责任 → A6 闭合、β 评估排 Stage 1 | Status Update(允许) |
| 2 | `docs/adr/ADR-CEP-11-...md` **§3 锁表** | `moesi-gpu-1.0` 行:「争点」→「**已裁定:(b) 成立**」 | **表类**正文(per「表可增,论断不改」先例) |
| 3 | `docs/adr/ADR-CEP-13-...md` **新增 §8 Status Update** | `INV-CEP-13-IC2` **应用口径注记** 3 条(判据**定义不变**) | Status Update(追加,不改正文) |
| 4 | `docs/adr/ADR-CEP-12-...md` **§8 追加** | R-2 已触发 + `perm` 形态约束 + 路径标签澄清 | Status Update(允许) |
| 5 | `docs/architecture/07-...md` **§5.1.1 表行** | `moesi_gpu` 行:「争点所在」→「已裁定 (b) 成立」 | **表类**正文 |
| 6 | `docs/implementation/2026-10-21-cep-archforge-response-receipt.md` | 升 **v1.2**:Q-3 闭合 / A14 已确认 / 行号更正(`:117`→`:126-127`)/ §10.3 执行记录 | 实施笔记 |
| 7 | `docs/implementation/2026-10-22-cep-poc-spike-spec.md` **§3.2** | S2 观测记录增第 ② 项「数据来源」 | 实施笔记 |
| 8 | `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/tasks.md` | **S1-6 勾选**(A6 闭合)+ 新增 **S1-7**(R-2 形态变更 + β 评估) | OpenSpec change |
| 9 | `scripts/test/cep_doc_ref_registry.tsv` | 登记本批新增的节号引用 + **负向断言**(第 7 列 `forbid`:防状态回退) | 校验数据 |
| 10 | `docs/adr/ADR-CEP-14-stub-contract-tests.md` | **有意保留,零改动** —— 其 `:86` 的「Stub 暴露的 4 值 `perm` 可能**不足以**让协议侧区分『静默 E→M』与『显式请求升级』」是**对判据 2 是什么的描述**(判据的固有属性),**非状态断言** → 该表述在 (b) 成立后**仍准确**(且由「可能不足」强化为「确已不足」),故不改写。<br>⚠️ 显式记录此判断,避免后续复审误判为**漏网** | **零改动**(仅台账登记) |

**⑦ 冻结面授权**

本条 Entry 授权上表 #1~#4 的 **ADR 修订**;冻结清单重生成用
`cep_freeze_check.sh --update --authorized-by "Entry #13"`。
**零 ABI 变更**;`protocol_instance.schema.json` **本批不动**(R-2 落 Stage 1)。

**⑧ 对方本轮自查勘误(4 项,已在对方仓落地,本方仅记录)**

`ADR-CEP-05 §10`→`§7`(8 处,对方**同款笔误**)· 「只评估 (a) 就下结论」推理缺环(4 处)·
R-2 实证锚点 `:151`→`:152` · 2 张状态表在 `M + Invalidate` 上不一致(**不影响**本轮结论,列入 D-9 批 2 返工核对项)。
⚠️ 对方第 1 项**与我方 `f034d92a` 是同一处笔误** —— 独立发生在两个仓,
**实证支持**本条 Entry 前置的节号校验机制(`cep_doc_ref_check.sh`)的必要性。

**关联**: ArchForge `docs/roadmap/cep-interface-alignment-response-2026-10-22.md`(commit `4ff2fa6`)·
我方清单 `docs/implementation/2026-10-22-cep-archforge-response-checklist.md`(commit `0cd52a67`)·
回执 `2026-10-21-cep-archforge-response-receipt.md` §8.1 / §8.4 / §8.5 ·
Entry #12(A4 批 2 授权 + 限定注记)

---

### Entry #14 — 2026-10-22 [cross-check]

**主题**: D-9 批 2 待办三项的**事实判位修正**(ArchForge 端核对 + 本方独立抽验全部证实)

**触发**: ArchForge 端对本方「三条待办」转述(2026-10-22 会话)的逐条核对回复。**无 ADR 改动、无授权需求**;回执 `:571` 一处歧义措辞复写修正(非冻结产物)。

**① 逐条核对(对方判定 + 本方抽验)**

| # | 项 | 对方判定 | 本方抽验(实证) | 收敛表述 |
|---|----|---------|----------------|---------|
| 1 | 锚点位移对照表 | ❌ 未兑现(承诺主体即「随批 2 commit 提供」,属未来动作) | `git log 4ff2fa6..HEAD` = **空**(对方仓零新提交) → 对照表**不存在** | 对照表 = **D-9 批 2 commit 的随行交付物**,当前无 |
| 2 | 批间锚点冻结 | ✅ 属实,但**措辞收紧为「至今未破」** | ① `ac956b8`→`4ff2fa6` 唯一提交零触碰锚定文件;② `cep_archforge_alignment_check.sh` 实跑 **33/33 全绿**(ArchForge 21 / CppTLM 12) | **进行时约束**,批 2 合入前仍生效,不是已完成事项 |
| 3 | 「已列入批 2 返工核对项」 | ✅ 属实(三处登记在案) | 对方答复核:回答单 §5.4 + ADR-SOC-25 §3.1 + 本方回执 §10.3 ④ | — |
| 4 | 「确认批 2 里已修」 | ❌ **不属实**(批 2 未落地,遗留未修) | 本方 `sed` 实测两处现行值(见 ②) | 正确表述:「已列入批 2 返工核对项,**待修**」 |

**② 待修遗留的精确定位(批 2 对账基准,本方实测)**

| 文件 | 现行语义 |
|------|---------|
| `ArchForge:docs/architecture/09-coherence-protocol.md:153`(§3.3 M 行,表头 `:140` / 表体 `:148-156`) | `{ 3, 3, 3, 3, 3, 3 }` → M+Inv → M(**保持脏独占**) |
| `ArchForge:docs/microarchitecture/coherence-protocol.md:226`(§4 snoop 表) | M+INVALIDATE → **I**(-self,data 写回) |

**③ 节号防误读(对方提醒,采纳)**: 「§5.4」指 ArchForge **回答单自身**的小节编号;09 号**没有** §5.4 → 三方转述写
「**ArchForge 回答单 §5.4(遗留本体在 09 号 §3.3)**」。

**④ 顺带确认(对方判定,无需我方动作)**: CppTLM `19ea4a9e` 行号漂移更正(`:118`→`:126-127`)与
`6bd53f76` 判据 2 状态落地(实质/程序两层记法)**均与其提交声明一致**。

**⑤ 批 2 合入时的对账基线(取代本轮待办转述)**

- **输入**: D-9 批 2 commit + **随 commit 的位移对照表**(届时才存在)。
- **动作(同批,不隔批)**: 重生成 20 个锚点 → `cep_doc_ref_check.sh` 全绿 → 按 ② 两处行号核对对方是否已在批 2 修复状态表不一致。
- ⚠️ 若对照表未随 commit 出现 → 对方违约在先,催办而非自行猜测行号。

**关联**: 台账 Entry #13(本条为 #13 「D-9 批 2」外部依赖的**事实基线修正**) ·
ArchForge 回答单 `docs/roadmap/cep-interface-alignment-response-2026-10-22.md` §5.4 ·
回执 §10.3(:571 修正后)

---

### Entry #15 — 2026-10-22 [go-decision]

**主题**: DRI 拍板「推送 + 开 S2 spike」并**当轮完成实测** —— S2 = 确认 + 回归门正位

**触发**: Entry #14 对账闭环后,A 矩阵待办即问「三条待办里唯一零外部依赖的活」;
DRI 选「推送 + 开 S2 spike」。S2 的两个前置(A4 / A6)均已于 2026-10-22 闭合
(per 规格 §4.1),无循环依赖。

**① 变更范围(硬边界逐条自证)**

| 项 | 内容 |
|----|------|
| 新文件 | `examples/cep_poc_spike_s2.cc`(单文件 spike;不进 ModuleFactory / 不注册 ABI / 无公共 API) |
| 构建接线 | `examples/CMakeLists.txt` +3 行(cep_poc_spike_s2 三行模式);**零 add_test**(spike 不进 CI,按需手跑) |
| 状态头 | 规格 `:4` 📋 草案 → S2 已实跑通过 |
| **零冻结产物** | fabric.json / protocol_instance.schema.json 只**读**(runtime 加载比对,无写入) |

**② 语义判位(5 条,先对齐再实现)**

1. 判据 (a)(b) 原文照收:`s1=E`(store 命中静默升 `s2=M`,零外部通信)+ (b) 析取逐态评估
   (`ADR-CEP-13:99-103`)。
2. 主路径表义 = `E` 行第 `Wr` 列 = 3(09 号 `:152` —— **与 Q-3a 同一引用点**)。
3. s1 支语义 = E+probe → **ACK 无数据**(ArchForge microarch/coherence-protocol.md`::255-263`)——
   域内实证「E 支不成立」,与 Q-3a 论证体一致。
4. 降级语义 = M+READ_SHARED → **O** + data 转发(同一文件 `:224`)——
   **刻意避开** `:226` 的 M+INVALIDATE→I(D-9 批 2 待裁定单元格,spike 不依赖未裁定语义,
   per Entry #14 ②)。
5. 转发形态 = 流 3 snoop(center→node)↔ 流 4 snoop_resp(node→center)DATA_AND_ACK,
   数据经流 6 forward_data 同槽(32 号 §6.0.1)。

**③ RUN 证据(2026-10-22 实测)**

```
g++ -std=c++17 -I include -I external/json 边编译 = 退出 0
cmake --build build --target cep_poc_spike_s2 = 2/2 OK → bin/cep_poc_spike_s2
./build/bin/cep_poc_spike_s2 → exit=0,断言 22/22 全绿
```

**主场景观测记录(3 项,规格 §3.2 修订版)**:

| # | 项 | 实测值 |
|---|----|--------|
| ① | 观测值 | **新值**(0x2222,DATA_AND_ACK) |
| ② | 数据来源 | **D-GPU M 态 cache 数据响应**(经流 6 forward_data 同槽;内存仍为旧值 0x1111 → 来源唯一定位 = 判据 (b) 的**直接证据**) |
| ③ | 跨域传播路径可达性 | **true**(compute_noc/VC1 snoop → D-GPU → DATA_AND_ACK 回环) |

**④ S2b 对照(Q-3b 实测)**: 静默升级 = **0 条流出线**;显式升级(S→M,他 sharer 在场)=
**1 条 snoop(INVALIDATE)→others** → D-FABRIC S→I 回 ACK。
两路径**终点相同**(perm=WRITEBACK)但**通信成本不可互换** → 直接喂给 S1-7 的
schema 演进设计约束(「perm 形态须承载静默/显式区分」)。

**⑤ 判位留痕(防误读 2 条)**:
- 主场景输入 = 「持有 E」**直接起步**(不模拟读分配路径): 09 号 6×6 的 `I+Rd→S`
  与「载入得 E」的表义张力**不属** S2 观测域(判据 (b) 评估对象是 E→M 的 store 命中,
  非 E 的进入方式);**若批 2 修订该语义,另行核对,不依赖**。
- M 态被 probe 后降级为 **O**(非 S)—— 忠实对齐 `:224`(脏共享保留所有权),
  初稿 S 版表义在判位核对时已纠正。

**⑥ 遗留声明**

- **S1(期望 perm)/ S3(死锁自由)**: 规格在,实现未启动(前置: S1 需 4 值表达力边界 =
  Stage 1;S3 需 vc_binding 真实声明)。
- **`cpptlm-cep-poc-spike` change 目录未建**(排队口径不变,Stage 1 顺位暂不变;
  KPI 维持 1/3)。
- ⚠️ **S2 实测确认 ≠ A6 闭合证明、更 ≠ R10 闭合**: A6 已于 Entry #13 以声明层裁定闭合
  (S2 = 其确认);R10 仍需**真人**「无阻断项」回执(硬边界 §1.2 item 4 未变)。

**关联**: Entry #13①(判据 (b) 声明层裁定)/ Entry #14(D-9 批 2 事实基线)/
规格 §3.2(权威判定定义)/ survey §2(`09 号` 镜像)/ ArchForge 回答单 §1.1(Q-3a 论证体)

---

### Entry #16 — 2026-10-22 [sprint-review]

**主题**: S2 RUN 证据**作废并取代**(critic 3 BLOCKERS)—— 修正重跑 + ctest 回归门落地

**触发**: 提交前 critic(4 项触发件命中:新增文件 + 治理面台账)对 Entry #15 批次
对抗审稿 → **REJECT**:3 BLOCKERS(均涉证据完整性)+ 8 SHOULD_FIX。
⚠️ **Entry #15 ③「RUN 证据」与 ②⑤ 引用中 3 处事实被证伪** —— 按 append-only 纪律
**不可改写** Entry #15 主体(**对齐 user B2 裁决先例:不回滚台账历史改写**),
本条以**修正重跑的新证据**取代其 RUN 部分;#15 的**语义判位 ② 与判位留痕 ⑤ 仍是
有效工作记录**(判位过程真),唯 **③ 实测列为作废值**。

**① 3 BLOCKERS(critic 实测证伪 → 修正)**

| # | 缺陷 | 证伪 | 措置 |
|---|------|------|------|
| B1 | **VC 绑定与权威相反**: spike 自造 `snoop=1`/`snoop_resp=1`,且对 ArchForge 32 号 §6.0.2 矩阵(:367-370)+ `moesi_gpu` 实例声明(:434-442)`snoop:2 / snoop_resp:3`;`vc_slot_valid` 只比上界(0-3 全放行)——「per-network 绑定」名不副实 | 突变 `route_binding()` 仍全绿 | 绑定改 **权威逐字镜像**(32 号 `:434-442`);校验改为 **逐(消息类,槽位)比对**(forward_data 走 snoop 同槽例外);**突变测试(有牙验证)**:`vc = 2→1` 后 **6 处断言即红** |
| B2 | **观测 ②③ 硬编码/自述**: ③ `path_reachable = true` 为常量(注释「已真的下发」失实 —— 无任何转发对象);② 数据来源 = 生产者自写 note;`mem_value` 无写入 → 「来源唯一定位」断言恒真 | grep 证据 n/a | **转发路径对象化**: `Bridge::fabric_read()` 以 tracker 指路 → probe 流真实发出 → `ReadResult` 由**实际选择**生成(值/来源枚举/可达/成链流计数);来源 = 流 6 forward_data 的对象级判决;NEG 侧 fallback **真实读内存** → 「主场景内存未被写」断言获得信息量 |
| B3 | **NEG 反向变体是同义反复**: 断言自己刚写下的字面量(违背「只跑正向等于没跑」的仓内反向验证文化,规格 :135) | 结构性识别 | 与 B2 合并处置:NEG = **同一 Bridge 代码路径** `fwd_enabled=false` → 真实 fallback 读内存旧值 |

**② 8 SHOULD_FIX 处置(逐条)**

1. 断言计数 22 实为 **21**(把函数定义计入)→ 门数改为**动态打印**(断言 N/N);
2. 头注释运行路径 `bin/examples/`(实际 `bin/`/`bin/examples/`)→ 经 CMake 输出目录对齐,统一 **`build/bin/examples/`**;
3. 引用 `:229` 错(是 O+write 的 INVALIDATE→others)→ S+side-effect 更正为 **INVALIDATE→all(microarch :212)**、S+INVALIDATE→I = **:213**;
4. 「32 号 §6.0.1 转发形态」→ 实为 **§5.2.1 转发表(:297-305)**/词表 §6.0.1;
5. CWD 依赖 → 从仓库外运行 exit 134(terminate)→ `read_file` **守开失败** → exit 2 明示(实测复证);
6. 「+3 行」实为 **+17 行**(numstat 实证:横幅+注释+target+add_test 4 号新增);
7. **回归门 ≥ 名实相符**: **注册 ctest**(`add_test` + `WORKING_DIRECTORY=${CMAKE_SOURCE_DIR}`)→ `ctest -R cep_poc_spike_s2` 已入 18/18 组;
8. `:333` 弱断言 → 收紧为直接比对 `*proj == Perm::READ`(带 `has_value` 前置)。

**③ 修正后 RUN 证据(2026-10-22 实测,取代 Entry #15 ③)**

```
g++/CMake 双路径编译 0 warning;
./build/bin/examples/cep_poc_spike_s2 → exit=0;
ctest --test-dir build -R cep → **18/18**(新注册 cep_poc_spike_s2 = Test #96);全绿
```

主场景 3 项观测(语义不变,证据形态升级):

| # | 项 | 实测值(取代 #15 同列) |
|---|----|------------------------|
| ① | 观测值 | 新值 0x2222(流 6 forward_data,VC2) |
| ② | 数据来源 | **D-GPU M 态 cache**(对象级枚举;fallback 路径真实读内存时得旧值 → 主场景内存未读/未写断言非恒真) |
| ③ | 跨域传播路径可达性 | **true** —— probe真实成链 3 条流(compute_noc/VC2 snoop → D-GPU → VC3 snoop_resp + VC2 forward_data → D-FABRIC) |

**④ 权威对齐的新增判位(修订 #15 ②)**

- **VC 绑定不再是 spike 内 provisional 声明** —— 32 号 `:434-442` 已有 `moesi_gpu`
  实例声明,**逐字镜像**权威值;⏸ **不依赖**未落库状态 —— 声明本体仍在 ArchForge,
  本仓镜像纯只读(approver 侧无回执要求;如对方行号漂移,按内容核对)。
- **双语自洽的调和注记**(ArchForge 自身张力,非本仓缺陷): 转发表(:303)称
  snoop_resp `DATA_AND_ACK` **携带数据值**;VC-R2(:391)要求 `snoop_resp` **不携带**
  数据、数据一律 forward_data 同槽(VC2)。spike 以 **流 4(VC3,状态 DATA_AND_ACK 语义)
  + 流 6(VC2,数据值)** 双消息建模两侧同成立 —— VC-R2 得守,与 Q-3a 转发形态一致。
  ⚠️ 该张力本条**显式登记**(非漏网)—— 若对方下轮修订忽略,须回该文件补「如何两可」的裁定。

**⑤ 遗留声明(增补)**

- Entry #15 ⑥ 的三项(前置不变)仍全部生效;此时新增:验证以 **ctest 回归**为准
  (不再依赖手跑);**S1 / S3** 及 `cpptlm-cep-poc-spike` change 目录仍待 Stage 1。
- 门数动态化 → **「21/21」不是固定契约值**(增删断言即漂移);
  对外引用一律写「全部断言通过」而非具体计数。
- `examples/CMakeLists.txt` 的 `RUNTIME_OUTPUT_DIRECTORY` **位置性陷阱**(set() 之后的 target 才生效,该文件先前 target 均落在 `bin/` 根)—— spike 排序在 `set()` 之后借此生效;⚠️ **不另行 refactor**(scope 纪律),留 Stage 1 顺带处理。

**关联**: Entry #15(其 RUN 证据部分作废/取代 + 语义判位与判位留痕维持)·
critic ses_e9caf3ac6ffecU3Mf288D96rpx · 32 号 §6.0.2/:434-442/§5.2.1/VC-R2(权威) ·
microarch coherence-protocol.md :212/:213/:221/:224 · `examples/cep_poc_spike_s2.cc`(修正版)

---

### Entry #17 — 2026-10-22 [go-decision]

**主题**: R10 评审包升 v1.1 —— 并入 S2 实测证据(声明层 / 实测层 / 评审层三层的可分辨性)

**触发**: DRI 拍板执行 P1(R10 材料加固)。理由:R10 是 **Stage 0 能否 archive 的唯一卡口**,
而它缺的只是「真人评审后的无阻断项回执」→ 评审材料的**可执行性**是唯一能由我方提升的变量。

**① 变更范围(1 份非冻结实施笔记 + 台账登记;零 ADR / 零 schema / 零冻结面)**

| 文件 | 改动 |
|------|------|
| `docs/implementation/2026-10-22-cep-stage0-external-review-summary.md` | **v1.0 → v1.1**:§1 补「唯一代码是可丢弃 spike,非 Stage 1 实现」· §2.3 加「实测层」行 · **新增 §2.4**「S2 实测确认」(序列 / 观测 ①②③ / s1 支对照 / S2b 对照 / 有牙验证 / 证据形态 + **诚实边界 4 条**)· §3 加「实测不替代本评审」注 · §4 **新增检查点 ⑤**(三层混淆检查)· §6 证据索引 +2 行(实测源码+ctest 命令 / 声明层论证出处)· 头部依据 +Entry #16 · 页脚 v1.1 + 关联 #13/#16 |
| 台账本条 | 登记本次评审包版本与新增检查点 |

**② 新增检查点 ⑤ 的设计意图**(R10 的「有牙」保障)

前 4 个检查点只问**契约层**;⑤ 专门问**分层诚实**:判据 (b) 的
**声明层裁定**(ArchForge 答复)/ **实测层确认**(§2.4 spike)/ **真人评审**(本评审 = R10)
是否被互相顶替。⚠️ 这条同时是**对我方自身的约束** —— 若 §2.4 越界(把 spike 说成
「已通过评审」)或提前宣布「R10 已闭合」,reviewer 可直接据此判不通过。

**③ 边界重申(不得被本条稀释)**

- **R10 仍未闭合**:本包只是**材料**;闭合条件 = §5 格式的**真人**回执(独立性要求同前)。
- **S2 仍是「确认 + 回归门」**,不升级为裁决、不构成 conformance 证据、不闭合 R10。
- 本包**不新增**任何对外承诺;未触及 `ADR-CEP-*` / schema / 冻结清单(29/29 不变)。

**④ 待 reviewer 动作(单点依赖,零技术门槛)**

reviewer 读 §0(30 秒)→ §2 + §3(10 min)→ §4 五个检查点(20 min)→ 回 §5 格式 1 条结论。
若给阻断项 → Stage 0 不 archive,但拿到具体待修项(优于悬空)。

**关联**: Entry #12(D5 决策 = 可执行最小动作)/ Entry #13(判据 (b) 声明层裁定)/
Entry #16(S2 实测证据)· 评审包 `2026-10-22-cep-stage0-external-review-summary.md` v1.1

---

### Entry #18 — 2026-10-22 [adr-amendment]

**主题**: `ADR-CEP-05 §7` **正文刷新**(判据 1 复核 + 判据 2 状态归位)—— 授权「正文刷新」这一显式口径

**触发**: 评审检查点 ①(契约自洽)时发现 §7 的**可读性缺陷**:注记链已达 4 层
(正文 → 限定注记 → 状态更新 → §13.1),读者第一眼必然读到**过期结论**(「当前不触发」);
且判据 1 的「当前不触发」清单仍是 **5 族旧清单**(未随 `moesi_gpu` / `mesif` 登记复核),
而判据 2 的同类句已被限定注记收窄 → **两判据口径不对称**。

**① 本条授权的变更(`ADR-CEP-05` = 冻结产物 → 须重生成冻结清单)**

| # | 落点 | 变更 | 依据 |
|---|------|------|------|
| 1 | `ADR-CEP-05 §7` | **整段刷新为当前状态 + 压缩**:**14 行**(原 33 行 / 中间稿 57 行)· 判据 1 补编号与方向/触发/后果(**压成 2 行**)· 判据 2「当前不触发」→「**已触发**」+ 论证/结论层级/形态约束/排期**改为指向 §13.1**(不重复)· 两份 2026-10-22 注记**被吸收** | Entry #13(判据 (b) 成立)· Entry #16(S2 实测确认)· 本条 ① 的复核需求 |
| 2 | `ADR-CEP-05 §13.1` | **压缩**:11 行 → **8 行**(合并 3 组重复行:事实变化 ①② / (b) 状态+论证形态 / 处置+落地责任)—— §7 指向本节,本节不再有冗余 | 精简(内涵不变) |
| 3 | `ADR-CEP-05 §13` | **三节收敛为一节**(§13.1 状态表 + §13.2 未变更项 + 变更史指针):合并 `#12` / `#13` / `#18` 三次修订,去掉过程叙述与重复;判据 1 状态并入 §13.1 | 精简(内涵不变) |
| 4 | `ADR-CEP-05 §7 / §13` | **压缩**:§7 33 → **10 行**(两条判据各 2 行 + 头);§13 由 52 → **26 行**。**过程记录归台账,不在 ADR 内累积** | 本条口径:ADR 只承载决策 |
| 5 | `docs/implementation/2026-10-21-cep-archforge-response-receipt.md` | **V-8 表格的行锚** `§7:223-225` → 刷新后的新行号(§7 行号已变,人工行锚会失效) | 路径漂移防护 |

> **净效果**:本 ADR 相对刷新前 **净增 +8 行**(中间稿曾 +98 行)—— **精简而非膨胀**是本条的口径要求之一。

**② 「正文刷新」的口径(此前惯例 = 「正文不改」)—— 显式例外,理由入册**

1. **预告先例**: 决策简报 `.opencode/plan/20261022-cep-decision-brief.md:67` 已预告「届时限定注记
   **被正式陈述取代**,不再需要回退」→ 本条即该预告的落地,**非破例**。
2. **可读性即正确性**: 注记链 4 层 → 读者第一眼读到过期结论 → **实质**误读风险(评审检查点 ① 的直接产物)。
3. **变更史未丢失**: §13.1 保留 §7 判据 2 的**原文**逐字引用 + 事实变化;过程记录(替换了什么、为什么)归**台账 Entry #18**,不在 ADR 内累积。
4. **决策内容零改动**: 两条判据的**触发条件与破裂后果**的内涵未变(判据 2 定义句**逐字保留**);
   被刷新的是**状态陈述**与**结构表述**。ADR 不可变约定本身**不变**。

**③ 判据 1 的复核结论(本次新增的实质结论)**

`moesi_gpu`(snoop 型)确有**数据两来源**的**事实前提**(peer 的 `M` 态 cache 数据响应 / 内存;per S2 实测),
但**来源对协议引擎可见、stub 不需要该信息** → **事实前提成立、需求不成立** → 判据 1 **仍不触发**;
若 Stage 1 出现「stub 侧统计 sharing / c2c 优化决策」需求 → 重评估 (β)(登记为 Stage 1 前置)。

**④ 机器约束核对(刷新前已验)**

- 不可动字面:`### α 失效判据 2(E 态静默升级`(**正向断言锚点**)+ `### 13.1 … 限定 → 状态收敛(已触发)`(逐字断言)→ **均保留**;
  `**仍未定**`(**负向断言**,防状态回退)→ **未引入**。
- §7 标题只需含「α 失效判据」(registry `kw`)→ **未改**。
- 冻结面变更 → 本条授权后 `cep_freeze_check.sh --update --authorized-by "Entry #18"`。

**⑤ 评审发现清单的同步修正**

本轮评审(检查点 ①)提出的「`ADR-CEP-05 §7` 注记与 §13.1 不一致」一条**撤回** ——
该 ADR 内**已有**第二段「2026-10-22 状态更新」注记明确作废前句,评审方此前只读到第一段。
现存 NIT 3 条:`ADR-CEP-06` `INV-CEP-06-IC5` 重复 3 次 + 游离 `#` · 「族」两套口径未交叉引用 ·
`07 号` 状态标签陈旧(起草中 vs 已冻结)。

**关联**: Entry #12(限定注记的原始授权)/ Entry #13(判据 (b) 裁定 + A6 闭合)/ Entry #16(S2 实测)/
Entry #17(R10 评审包 v1.1)· `ADR-CEP-05 §7 / §13` · 决策简报 `:67`(预告先例)

---

### Entry #19 — 2026-10-22 [adr-amendment]

**主题**: 评审 NIT 清理(2 条落地 + **1 条撤回**)—— 服务 R10 评审检查点 ①(契约自洽)

**触发**: DRI 选「A:清 3 条 NIT」(Entry #18 ⑤ 登记的评审发现清单)。

**① 落地 2 条**

| # | 落点 | 改动 | 依据 |
|---|------|------|------|
| 1 | `ADR-CEP-06` | 删除 **2 处** `INV-CEP-06-IC5` **带游离 `#` 的重复行**(`:144` / `:153`;真定义在 `:140`,全篇由 3 处 → **1 处**)。这 2 行是误粘贴,`#` 前缀会造出伪标题 | NIT1(评审检查点 ①) |
| 2 | `ADR-CEP-11 §3` | 使用规则 **+第 4 条**:「协议族」此处按 `protocol_version` **行**计(8 行),与 `07 号 §5.1.1` 的「**6 族 / 8 实例**」是**两种粒度**(CXL 2.0 / 3.0 同族、`moesi_gpu` / `mesif` 同族)→ **非矛盾**,本表不复制实例计数 | NIT3(「族」两套口径) |

**② 撤回 1 条(NIT4:`07 号` 状态标签「起草中 vs 已冻结」)—— 复核后不成立**

- **复核事实**:全仓 **21 份 `ADR-CEP-*` 头部统一写「🟡 起草中(vX.Y)」**(`ADR-CEP-00` ~ `18` + `NUMBER-MAP`),
  `07 号` 与 `README.md:37` 的「v0.1 起草中」**与之一致** → 这是**项目惯例的版本标签**,不是 `07 号` 独有的陈旧状态。
- **判定**:① 与惯例一致 → **不构成契约矛盾**;② 单独改 `07 号` 反而**制造**不一致;③ 若将来要统一改标签语义,须**整批 21 份**处理 —— **不在本批范围**(scope 纪律)。
- ⚠️ 本条同时是**评审方自我修正的留痕**:NIT4 由本仓 build agent 在评审辅助中提出,复核惯例后**主动撤回**(与 Entry #18 ⑤ 撤回 finding #1 同款处置)。

**③ 机器核验与授权**

- `grep -c "INV-CEP-06-IC5"` → **1**;`grep -c "^#\*\*"` → **0**(游离 `#` 清零)。
- `cep_doc_ref_check.sh` 全绿(§3 标题、§5.1.1 标题等断言锚点**未动**)。
- 冻结面变更(2 份 ADR)→ 本条授权 `cep_freeze_check.sh --update --authorized-by "Entry #19"`;
  **两步 commit**:先 commit 产物 → `--update` → 再 commit 清单。
- 零 schema / 零 ABI / 零 `protocol_version` 取值改动(**纯文档清理**,不触碰任何判据或锁表取值)。

**④ 评审发现清单终态(供 R10 评审包引用)**

| # | 发现 | 处置 |
|---|------|------|
| N1 | `ADR-CEP-05 §7` 限定注记与 §13.1 不一致 | **撤回**(Entry #18 ⑤:该 ADR 内已有第二段状态更新注记明确作废前句) |
| N2 | `ADR-CEP-06` `INV-CEP-06-IC5` 重复 3 次 + 游离 `#` | ✅ **本条已清** |
| N3 | 「族」两套口径未交叉引用 | ✅ **本条已补** |
| N4 | `07 号` 状态标签陈旧 | **撤回**(21 份 ADR 统一惯例,非缺陷) |

**关联**: Entry #18(评审检查点 ① 的发现与授权)· `ADR-CEP-06 §3` · `ADR-CEP-11 §3` ·
`07 号 §5.1.1`(族计数补记)

---

### Entry #20 — 2026-10-23 [adr-amendment]

**主题**: ADR-CEP 族**文档精简**(T1 结构修复 + T2 去 review 溯源括注 + T3a 修订记录归档台账)——
「只保留架构关键决策内容」的可读性整改,**不动任何机器校验的契约面**。

**触发**: DRI 指示「审查 `docs/adr/ADR-CEP-*.md`,检查是否可以改进得更加精简清晰,并只保留架构关键决策相关的内容」。

**① 变更的冻结产物清单(逐项 + 为什么)**

| # | 产物 | 改动 | 为什么 |
|---|------|------|--------|
| 1 | `ADR-CEP-02` | **同一「⚠️ v0.3 补充 CEN-Ib11」块重复 4 次 → 保留 1 次**;其中 3 份首行是**畸形 `#>` 标题**(非 Markdown 引用) | 粘贴事故:同一段落在 §2 后 / §3 后 / §3.1 后 / §3.2 后各出现一次;读者会以为有 4 条不同补充 |
| 2 | `ADR-CEP-04` | **文件脚注(版本/批准状态/维护者)从 §10 与 §11 之间移到文末** | 脚注夹在正文中间 → 文档读起来像「没写完」;且 §11 之后无任何收尾 |
| 3 | `ADR-CEP-07` | 头部「关联真源」块**整段重复 7 行**(ADR-CEP-00 / 00A / 06 / DGPU-08 / 09 / 12 各列两遍)→ 去重 | 同上;头部噪声 |
| 4 | `ADR-CEP-00` | §7 **两张相邻的修订记录表**(同一表头重复)→ 合并为一张 | 合并残留 |
| 5 | `ADR-CEP-17` | ① 决策 8 的 **YAML 代码块缺起始围栏**(只有收尾 ``` )→ 补围栏;② §3 子节**乱序**(3.1 / 3.2 / **3.4 / 3.5 / 3.3**)→ §3.3 前移 | 缺围栏导致**全文 9 个 ``` 奇数失衡** → 其后所有代码块渲染错乱(且使「围栏内/外」判定失效);乱序违反编号语义 |
| 6 | 21 份 ADR 的 **「vX.Y 修订记录」节**(共 **348 行**)→ 折叠为 1 行台账指针(**标题保留**,锚点稳定) | 修订史是**过程元数据**而非架构决策;且与本台账 §11 重复。按本系列自身既定口径(`ADR-CEP-05 §13.2`「过程记录归台账,不在本 ADR 内累积」)归档到台账 |
| 7 | 21 份 ADR 正文的**纯标签型 review 溯源括注**(`(per oracle Bxx)` / `(per metis Gx)`,共 **~120 处**)→ 剥离 | 评审过程标记对读者无信息量,且密度过高淹没决策本身;**含实质内容或契约字面量的括注一律保留**(见 ②) |

> **规模**:5426 → **5121 行**(−305 行 / −5.6%);`git diff --stat` = 21 files, +126 / −441。

**② 契约面零改动(本次整改的红线 —— 逐条核验)**

- **不变量定义**:66 条实际 / 67 编号位(`INV-CEP-04-IC4` 仍为永久跳空)—— `cep_docs_sync_check.sh` 组 ② **实测一致**;
  07 号 §4 总表与 15 组条数**零 diff**。
- **契约字面量**(被 `test_cep_contracts_crosscheck.cc` / `cep_doc_ref_registry.tsv` 断言):
  `以 §15.4 为准` · `唯一真源` · `只引用不复制` · `仅锁表更新` · `语义适配` · `scope-change` · `adr-amendment` ·
  `协议泄漏通用禁令` · `不感知` · `MesiGpuProtocol` / `MesiThreeLevelProtocol` / `translate_snoop` ·
  `ADR-CEP-05` §7 / §10 / §13 / §13.1 · `ADR-CEP-11` §3 · `ADR-CEP-12` §8 / §9 / §9.2 / §9.3 ·
  `ADR-CEP-13` §2 / §3 / §4 / §8 / §8.2 · `ADR-CEP-06` §3 —— **逐项自检 0 缺失**。
  > ⚠️ 中途一次误伤:`以 §15.4 为准` 曾随溯源括注被删 → **已回滚重做**,现改为「括注内含契约字面量则**保留**」的保护规则。
- **节号与节标题**:**一律不改**(`## N.` 编号与标题全部保留)—— 外部文档按 `§N` 引用这些 ADR(如 `ADR-CEP-00 §8`、
  `ADR-CEP-05 §13.1`、`ADR-CEP-12 §8`),重编号会打断可达性。
- **Status Update 段**:**全部保留**(含「未变更项」样板)—— 其中 `ADR-CEP-05 §13.2` 含被断言的字面量
  「应用口径注记记于 `ADR-CEP-13` 的 **Status Update**」;且「明示零改动」本身是 M1「防静默自我改写」的一部分。
- **零 schema / 零 ABI / 零 `protocol_version` 取值改动**。

**③ 机器核验与授权**

- `cep_docs_sync_check.sh`:组 ①~⑥ **全绿**(invariant 66/67、P 编号白名单、链接可达、Stage 0 产物);
  组 ⑦ M1 冻结校验在 `--update` 前**预期红**。
- `cep_doc_ref_check.sh`:**全绿**(342 处「别名 §N」引用目标节全部存在;42 条语义断言成立;4 条负向断言成立)。
- `cpptlm_tests "[cep]"`:**76 case / 1284 assertion 全绿**。
- 本条授权 `cep_freeze_check.sh --update --authorized-by "Entry #20"`;**两步 commit**:先 commit 产物 → `--update` → 再 commit 清单。
- 产物数**仍为 29**(未新增 / 未删除冻结面成员)。

**④ 白名单 / 遗留声明**

- **白名单**:无(本批**零**例外 —— 所有改动均可在 git diff 中逐条复核)。
- **遗留**:
  - **「决策汇总」表(21 份)保留** —— 它是架构决策本身的快速索引,属「要保留的内容」,不在本批精简范围。
  - **Status Update 段保留** —— 含契约断言 + M1 防静默改写语义。
  - **`ADR-CEP-17` 的节号空缺保留** —— 原文档即缺第 7 节(第 6 节 修订记录直接跳第 8 节);**重编号**会把
    `ADR-CEP-12 §8` / `ADR-CEP-13 §8` 等外部引用打断 → **不修**,登记为已知 cosmetic 缺口。
  - **代码块内注释中的溯源括注保留**(如 `// canonical lookup 签名(per oracle S6 修订)`)—— 属伪码注释,
    非规范正文,改动收益低。

**关联**: Entry #19(前批 NIT 清理)· `ADR-CEP-02` · `ADR-CEP-04` · `ADR-CEP-07` · `ADR-CEP-17` ·
`scripts/test/cep_freeze_check.sh`(M1 授权闸门)· `scripts/test/cep_docs_sync_check.sh`(契约面守门)

---

### Entry #21 — 2026-10-23 [adr-amendment]

**主题**: ADR-CEP 族**第二轮精简(T4)**—— 削减「样板 / 重复 / 已被取代」内容,
把族总行数从 5122 压到 **4860**(**相对基线 5426 = −10.43%**,达成 DRI 设定的 −10% 目标)。

**触发**: DRI 在 Entry #20 落盘后指示「继续考虑进一步压缩到 −10% 以上」。

**① 变更清单(逐面 + 为什么)**

| # | 面 | 体量 | 处置与理由 |
|---|----|------|-----------|
| 1 | **头部「关联真源」的 ADR-to-ADR 互引** | 118 行 | 与各 ADR 文末「## N. 关联 ADR / 后续」节**逐条重复**(已逐文件比对确认)→ 压成 1 行指针「同系列 ADR 互引见文末『## N. 关联 ADR / 后续』节」。**真源 1(真源文档路径)与「调研 / 一手锚点」全部保留**(真源分级依据不动) |
| 2 | SU「取代范围」表(CEP-08 / 09 / 12 / 17) | 46 行 | 「哪一段被取代」的索引表 → 压成 1 行;**取代后文本(规范)本身一字未动** |
| 3 | SU「未变更项(明示)」样板(CEP-05 / 11 / 12 / 13 / 15) | 70 行 | 5 份「X 零改动」清单 → 各压成 1 行。**M1「防静默自我改写」语义保留**(仍逐项点名「零改动」的对象) |
| 4 | `ADR-CEP-00A §4` 伪引清单 | 17 行 | 14 条伪引**已全部修复** → 压成摘要 + 现行规则;逐条修订位置归 git 历史与台账 |
| 5 | `ADR-CEP-17 §3.4` / `§3.5` | 52 行 | 这两节**自述「已由 ADR-CEP-12 / ADR-CEP-09 承接,不再作为引用来源」** → 压成指针(§3.4 保留「新增 5 个维度的理由」) |
| 6 | SU「为何必须改 / 为何这是」 | 52 行 | 论证压缩为结论式;**CEP-09 的「对 Stage 1 的硬前置」硬约束块逐字保留** |
| 7 | SU 引导块(性质 / 触发 / 台账) | — | 保留(授权链可追溯) |

> **规模**:5122 → **4860** 行(−262);`git diff --stat` = 21 files,+68 / −330。
> **未删**:「决策汇总」表(21 份)—— 它是架构决策的快速索引,属**要保留的内容**;
> Status Update 的**规范文本**、全部不变量、全部「## N.」节号与节标题。

**② 契约面零改动(红线)**

- **不变量**:66 实际 / 67 编号位不变(`INV-CEP-04-IC4` 仍跳空);07 号 §4 零 diff。
- **节号与节标题**:一律不改 —— 本批连「取代范围」表的**行**都压了,但**标题文本一字未动**
  (`ADR-CEP-12 §8` / `§9` / `§9.2` / `§9.3`、`ADR-CEP-13 §8` / `§8.2`、`ADR-CEP-05 §13.1` 等被断言的节全部原样)。
- **契约字面量**:逐项自检 **0 缺失**(以 §15.4 为准 / 唯一真源 / 只引用不复制 / 仅锁表更新 / 语义适配 /
  scope-change / adr-amendment / 协议泄漏通用禁令 / 不感知 / MesiGpuProtocol / 应用口径注记记于 …)。
- **P 编号口径**:守门脚本抓到一次误伤 —— 本批压缩 00A §4 时把 `06 §15.4 P2-P6`(含 **P6**)写进了正文且缺排除语境
  → **已改为不写 P 号范围**,复检通过(P6 37 处全部处于排除语境)。
- 零 schema / 零 ABI / 零 `protocol_version` 取值改动。

**③ 机器核验与授权**

- `cep_docs_sync_check.sh --strict`:组 ①~⑥ / ⑧~⑩ 全绿(组 ⑦ M1 冻结在 `--update` 前预期红)。
- `cep_doc_ref_check.sh`:通过(350 处节号引用目标节全部存在;42 条语义断言 + 4 条负向断言成立)。
- `cep_archforge_alignment_check.sh`:**本批修完 6 处漂移**(见 ④)→ 33 锚点全绿。
- `cpptlm_tests "[cep]"`:**76 case / 1284 assertion 全绿**;`ctest` 全量 **96/96**。
- `markdown_link_check.sh`:失效链接 **108 → 108**(零回归);`docs_sync_check.sh` 缺失路径 0。
- 本条授权 `cep_freeze_check.sh --update --authorized-by "Entry #21"`(两步 commit:先 commit 产物 → `--update` → 再 commit 清单)。
- 冻结面成员**仍为 29**(未增未减)。

**④ 连带修复:6 条 `file:line` 锚点重定位**

`docs/implementation/2026-10-21-cep-archforge-interface-alignment.md` 的 **CEP_ALIGNMENT_ANCHORS** 块含 12 条
指向本仓 ADR 的 `file:line` 锚点。头部去重使行号整体上移 → 6 条漂移(`ADR-CEP-02:55` / `:66`、
`ADR-CEP-05:170`、`ADR-CEP-12:168`、`ADR-CEP-15:121`、`ADR-CEP-07A:99`)。

- **重定位方法**:按「**旧行内容 → 新行内容精确匹配**」映射(去空白后全等),**不按行距猜测** ——
  避免指向同文件内的**同符号他处**(如 `INV-CEP-05-IC1` 在 05 号有 2 处命中)。
- 该文件**不在 M1 冻结集内**,故其改动不需 `--update`;但被冻结的 21 份 ADR 需要 → 走本条授权。
- ⚠️ **该锚点机制对行号敏感**:后续任何 ADR 行数变动都会打断它 —— 新增 ADR 章节时应**同步重定位**,
  或改用「符号锚点」而非「行号锚点」(本批**不**改机制,仅登记为已知脆弱点)。

**⑤ 遗留声明**

- **白名单**:无(本批零例外)。
- **已知脆弱点**:`CEP_ALIGNMENT_ANCHORS` 的行号敏感性(见 ④ 末)。
- **`ADR-CEP-17` 第 7 节编号空缺**仍不修(承 Entry #20 ④,重编号会打断外部 `§8` 引用)。

**关联**: Entry #20(第一轮精简 T1/T2/T3)· `ADR-CEP-08` / `09` / `12` / `17` / `00A` ·
`docs/implementation/2026-10-21-cep-archforge-interface-alignment.md`(CEP_ALIGNMENT_ANCHORS 块)·
`scripts/test/cep_archforge_alignment_check.sh`(锚点守门)

---

### Entry #22 — 2026-10-23 [scope-change]

**主题**: Stage 0 实施态归位 + Stage 1 准入前置(S1-1 ~ S1-7)**可寻址化** + 季度算术修正(10 → 11)+ 路线图四文档口径统一

**触发**: 2026-10-22 ~ 10-23 的密集提交(Entry #13 ~ #21)后,planner 对 `obj-cep-platform` / `phase-10` / `cpptlm-cep-umbrella` / `roadmap.md` 四文档做**漂移复核**(逐条实测 + `metis` 对抗性交叉检查)→ 发现 4 类问题:**① 状态漂移**(Stage 0 产物已落地但仍标 Planned)· **② 口径漂移**(ADR 份数 / task 数 / change 数 / validate 结论 / 族计数多副本不一致)· **③ 结构性缺口**(S1-1 ~ S1-7 七项 Stage 1 准入前置在路线图上不可寻址;`cpptlm-cep-poc-spike` 队列位与实际脱节)· **④ 季度算术错**(真源 11 季度 vs 路线图 10 季度 → 终态 2029-Q1 应为 2029-Q2)。

**决策**: ✅ **采纳** —— DRI 拍板 **批 1 + 批 2 + 批 3**;`poc-spike` 取**第三态**(保留队列位 + 登记已实跑事实);S1-x 取**双落点**(directory-ordering-plane + chi-rnf-subset)。**批 4 暂缓**(动冻结面第 29 成员)。

**① `cpptlm-cep-poc-spike` 队列位 = 第三态(新增)**

| 方案 | 可执行性 | 判定 |
|------|---------|------|
| 补建 OpenSpec change | ❌ `cpptlm-cep-umbrella` 占**唯一** KPI slot 且 R10 未闭合 → 不 archive → 建新 change 令 CEP 内部 Proposed 并发 = 2,违反 §5.4 + `ADR-CEP-00 §2 Decision 3` | 不可执行 |
| 移出 Stage 1 队列 | ❌ `ADR-CEP-00 §2 Decision 3`(真源 3 · 冻结面成员)明列其为 Stage 1 首个 change → 删行 = 10 change,需改 ADR → 触碰冻结面 | 不可执行 |
| **保留队列位 + 登记已实跑事实** | ✅ 保住 11 change 算术;不改 ADR;不新增 KPI slot | **采纳** |

**落地**: §4.2 顺序 #1 状态列 →「⏸ 队列位保留 · S2 已实跑并注册 ctest 回归门(per Entry #15/#16)· change 目录未建(per Entry #15 ⑥)· 落地形态待裁决」;§4.2 P-1 说明**追加**事实登记段。

**② S1-1 ~ S1-7 归属 = 双落点(新增)**

§4.2 表**新增 1 列**「承接的 Stage 1 准入前置」(**不新增 change、不新增 KPI slot**):

| 归属 change | 承接项 | 依据 |
|------------|--------|------|
| 顺序 #1 `cpptlm-cep-poc-spike` | **S1-1 ~ S1-5**(装配期前置,`tasks.md:406-410`) | 均为**首个 `implemented` 实例装配前**的门槛 |
| 顺序 #3 `cpptlm-cep-directory-ordering-plane` | **S1-7 的 (β) 重评估** | `ADR-CEP-05 §13.1` 排期约束:与 `ADR-CEP-08 决策 4` 组合检查器**同批**落地 |
| 顺序 #5 `cpptlm-cep-chi-rnf-subset` | **S1-7 的 `perm` 形态 schema 演进** | `ADR-CEP-12` 为 schema owner;该 change 是**首个真实使用 schema** 的协议 change |

**边界**:`perm` **维持 4 值锁**(per `ADR-CEP-02 CEN-Ib11`);R-2 的落地形态是**新增字段**而非扩枚举;schema 变更属 **Stage 1**,Stage 0 冻结面不动。

**③ 季度算术修正**

- 真源 `ADR-CEP-00 §2 Decision 3` = **11 个 change(1 + 7 + 2 + 1)/ 串行 11 季度**(2026-10-22 per Entry #12 由 10 → 11)→ 路线图**终态验收 2029-Q1 → 2029-Q2**(§4.6)。
- §4.2 表季度列是**乐观并行估计**;**串行下界 = 11 季度** —— 两者不可相加,已加口径脚注(§4.2)。
- ⚠️ **已知缺陷(本批不改)**:`ADR-CEP-NUMBER-MAP §2 决策` 仍写「Stage 1 = 6 串行 / 总 10 个 OpenSpec change」→ **真源 3 内部与 `ADR-CEP-00` 不一致**。改它 = 改冻结面成员 → 需 `--update --authorized-by` + 两步 commit → **登记为已知缺陷,留待单独批次(批 4)**。

**④ 口径统一(机械同步)**

| 量纲 | 统一值 | 落点 |
|------|-------|------|
| ADR 份数 | **21 份**(`docs/adr/ADR-CEP-*.md`) | obj §0/§3/§4.1/§4.5 · phase-10 · improvement · roadmap.md |
| task 数 | **105 / 99 完成** | obj §4.1 · phase-10 · improvement |
| `openspec validate --strict` | **rc=1**(2 预存 WARNING,per Entry #12 ⑦) | obj §4.1/§5.3 · phase-10 · improvement(原 3 处写 "PASS") |
| change / 季度 | **11 / 11** | obj §0/§4.2/§4.6/§5.4/§6.2/§6.3 · improvement · roadmap.md |
| invariant | **66 实际 / 67 编号位 / 15 组** | obj §0 DoD 7(**只加落点口径注,不复述分解**)· improvement(原表合计写 34 实为 35 且缺 `CEN-Ib11` → 改为引用 `07 号 §4`) |
| 冻结成员 | **29** | 不改(真源 = freeze manifest) |

**⑤ 批次切分**

| 批 | 范围 | 冻结面影响 |
|---|------|-----------|
| **批 1** | 4 个路线图文档(obj / phase-10 / improvement / roadmap.md) | 无(均不在 M1 冻结集;objective 受 `cep_doc_ref_check.sh` §N 锚点约束 → 保留全部 `## §N` 节号与标题) |
| **批 2** | umbrella change 侧 `proposal.md` / `tasks.md`(6 处 stale) | 无(`specs/cep-core-contracts/spec.md` = 冻结面第 29 成员,**不动**) |
| **批 3** | `scripts/README.md` + `docs/ONBOARDING.md §5.5` + `docs/README.md` 计数(6 个 `cep_*` 守门脚本未进索引) | 无 |
| **批 4** | `openspec validate --strict` rc=1 根因 + `ADR-CEP-NUMBER-MAP` 10 vs 11 | **动冻结面 → 暂缓**,仅登记 |

**⑥ 契约面零改动(红线)**

- **零 ADR 改动**(21 份零 diff)· **零 `07 号` 改动** · **零 `include/tlm/cep/` 下 3 个 schema / `experimental/metrics.schema.json` / `tools/` 下 3 个 py / umbrella `spec.md` 改动**。
- **§11 Entry #1 ~ #21 正文零改写**(append-only;过期项一律由本条覆盖)。
- **零 `protocol_version` 取值改动 / 零 invariant 定义改动 / 零 ABI 改动**。
- **未新建** `openspec/changes/` 下的 poc-spike change 目录。

**⑦ 验收(可执行)**

```
bash scripts/test/cep_docs_sync_check.sh --strict   == 0
bash scripts/test/cep_doc_ref_check.sh --strict      == 0
bash scripts/test/cep_freeze_check.sh                == 0
bash scripts/test/cep_archforge_alignment_check.sh  == 0
ctest --test-dir build -R cep                        == 18/18
./build/bin/cpptlm_tests "[cep]"                     == 0(76 case / 1284 assertion)
git diff --name-only  → 不含 21 份 ADR / 07 号真源 /
                         include/tlm/cep/ 下 3 个 schema / experimental/metrics.schema.json /
                         tools/ 下 3 个 py / umbrella spec.md
```

**⑧ 明确不改**

§11 历史 entry · ADR 族正文 · `07 号 §4` 的 15 组分解 · `ADR-CEP-17:315` 的「16 adapter」(**adapter 实例数 ≠ ADR 份数**,不同量纲,不可混为一谈)· Stage 0「已完成」表述(**R10 未闭合 → 不 archive**)· 不把 ADR 精简两轮(−10.4%)包装为 objective 成果(仅一行指针)· 不新建「漂移矩阵 / 状态仪表盘 / revision log」文档(避免第 5 处口径源)。

**⑨ 改动清单(本条授权范围内的实际改动 —— 9 份文档,0 冻结面成员)**

| # | 文件 | 改动性质 | 规模 |
|---|------|---------|------|
| 1 | `.rddf/roadmap/objectives/obj-cep-platform.md` | 本文件:元数据 / §0 / §3 / §4.1 / §4.2 / §4.5 / §4.6 / §5.3 / §6 / §8.1 / §9.1 / §10 / §11 头 + 本条 Entry | 40 处替换 + Entry #22 |
| 2 | `.rddf/roadmap/phases/phase-10.md` | 状态 `planning → in-progress` + 10.B~10.G 状态 + 估时 + 依赖图 + KPI baseline + 测试标签 + 签发前置决策 | 17 处替换 |
| 3 | `.rddf/improvements/cpptlm-cep-umbrella.md` | 头部状态 / 前置 gate #3 / Capabilities #1·#9·#10 / What Changes / Acceptance A1~A7 / Impact A~D | 24 处替换 |
| 4 | `.rddf/roadmap.md` | 最后更新 + KPI baseline 口径 + phase-10 状态(表 + AUTO-INDEX)+ Objectives 段 | 5 处替换 |
| 5 | `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/proposal.md` | 真源行数 / ADR 份数 / change 数 / 风险表估时 / Acceptance 实测回填 | 6 处替换 |
| 6 | `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/tasks.md` | 头部状态 + 估时 + 时间线标题 + 验收对照表(实测列) | 5 处替换 |
| 7 | `scripts/README.md` | `test/` 子目录表补 10 个脚本(6 个 `cep_*` + 4 个历史未登记)+ 新增 §CEP 契约守门脚本 章节 | 2 处替换 |
| 8 | `docs/ONBOARDING.md` | §5.5 脚本表新增 5 行 CEP 守门脚本 | 1 处替换 |
| 9 | `docs/README.md` | `implementation/` 计数 12 → **14**(.md 实测)+ 目录树注释 | 2 处替换 |

> **纪律说明**:①`tasks.md` 的**历史执行记录**(Step 1~8 逐步记录)**零改写** —— 只改「当前状态元数据」与「验收对照表」;②`ADR-CEP-05` 等 **21 份 ADR 零 diff**;③`07 号` / `include/tlm/cep/` 下 3 个 schema / `experimental/metrics.schema.json` / `tools/` 下 3 个 py / umbrella `spec.md` **零 diff**(M1 冻结面 29 成员,实测 `git diff --name-only` 对该集合 = **0**)。

**⑩ 遗留登记(本批未处置 —— 逐条给处置)**

| # | 遗留 | 性质 | 处置 |
|---|------|------|------|
| L1 | `openspec validate cpptlm-cep-umbrella --strict` **rc=1**(2 条预存 WARNING) | **预存不满足**(per Entry #12 ⑦);根因在 umbrella `spec.md` 顶部文本不被 archive 携带 | **批 4 暂缓** —— `spec.md` = M1 冻结面第 29 成员,修它需 `--update --authorized-by` + 两步 commit |
| L2 | `ADR-CEP-NUMBER-MAP §2 决策` 仍写「Stage 1 = 6 串行 / 总 10 个 change」 | **真源 3 内部矛盾**(与 `ADR-CEP-00 §2 Decision 3` 的 11 冲突) | **批 4 暂缓** —— 改它 = 改冻结面成员;本批只登记,路线图侧一律按 `ADR-CEP-00` 的 **11** |
| L3 | Entry #20 的变更清单表 rows 6/7 **列数错位**(3 列 vs 表头 4 列) | **历史遗留**(HEAD 版即存在) | **不改** —— append-only 纪律;与 Entry #21 ④ 对 `ADR-CEP-17` 节号空缺的处理先例一致(登记为 cosmetic 缺口) |
| L4 | `docs/ONBOARDING.md §5.5` 的 `--mode auto\|off\|ptx-emu\|both` 管道符未转义 | **历史遗留**(HEAD 版即存在) | **不改** —— 与本批 CEP 无关,避免 scope 膨胀 |
| L5 | ctest 全量曾**偶发失败 1 次**(未能捕获失败用例名 —— ctest 日志已被后续运行覆盖) | **已定位**(per Entry #24 → #25):`cep_tools_contract`(#78)与 `cep_all`(#82)在 `-j4` 下并行跑同一组 `[cep][tools]` 用例 + **固定名临时目录**,竞态 | **已交付 builder** —— improvement 草稿 `cpptlm-cep-tools-test-isolation.md`;修复前 `-j4` 下不可信 |
| L6 | `docs/architecture/07-coherence-exploration-platform.md §8.1`(**:543-553**)仍写「Stage 1 = **6** change / 合计 **10** 个 / **串行 10 季度**」 | **冻结面成员**(M1 29 成员)与 `ADR-CEP-00 §2 Decision 3` 的 **11** 冲突 —— 真源 1 vs 真源 3 的已知矛盾 | **批 4 暂缓** —— 改它需 `--update --authorized-by` + 两步 commit;**本批只登记**:路线图侧一律按 **11**(§4.0 / §4.6 为真源),新会话读 07 号 §8.1 时以本条为准(见 §4.7.4 误读 #5) |

> **✅ L2 / L6 已闭合(2026-10-23,per 本条 Entry #27)** —— 上表两行的「批 4 暂缓」是**历史状态**,**以本条为准**:两处冻结产物已按 `ADR-CEP-00 §2 Decision 3` 的 **11**(1+7+2+1)更正,并已走 `--update --authorized-by "Entry #27"` 授权闸。**本表正文保持历史原样**(append-only 纪律)。

**⑪ critic 复审修正轮次(2026-10-23,per critic REJECT)**

改动触发 critic 对抗审稿(默认立场 REJECT)→ **2 BLOCKERS + 6 SHOULD_FIX + 5 NITS**,逐条核实后**全部属实**,已修正:

| # | critic 判定 | 类别 | 处置 |
|---|-----------|------|------|
| B1 | 写入「1856 cases / 81145 assertions(**实测**)」→ 实测为 **1857 / 81202** | **BLOCKER** —— 虚假「实测」声明(把 `tasks.md` 的历史快照当当前实测) | ✅ phase-10 / improvement 两处改实测值 + 注明 `tasks.md` 的 1856 / 81145 是**该批执行时点快照**(append-only 不改) |
| B2 | obj §0 标题行 `DoD, 2029-Q1 终` 未随 §4.6 改 | **BLOCKER** —— 同文件两套终态日期 | ✅ → `2029-Q2` |
| S1 | roadmap.md 写「21 条 entry」但实测 **22** 条 | SHOULD_FIX | ✅ → 22 条 |
| S2 | improvement Non-Goals 残留「10 季度」 | SHOULD_FIX | ✅ → 11 季度 |
| S3 | phase-10 文件头残留「657 行」(同文件 10.E 表已改 686) | SHOULD_FIX | ✅ → 686 行 |
| S4 | phase-10 同一行「单一时钟真源」**重复 2 次** + 错字形 `⏔`(全仓惯例 `⏸`) | SHOULD_FIX | ✅ 去重 + 改 `⏸` |
| S5 | tasks.md 时间线标题「46 人日」vs 8 Phase 表内合计 **28** 人日 | SHOULD_FIX | ✅ 标题改「**builder 侧 ~28 人日** + architect 侧 17 人日 = Stage 0 合计 46 人日」 |
| S6 | improvement + proposal 的 phase-10 指针仍写 `(planning)` | SHOULD_FIX | ✅ → `(in-progress)` |
| N1/N2 | improvement「fabric **载体载体**」叠字 + 行数 `~30` vs 实测 **51** | NIT | ✅ 去叠字 + → `~51` |
| N3 | docs/README.md `implementation/` 计数口径不一(表 14 vs 树「14 份 + README」) | NIT | ✅ 表 = 14(.md 含 README + 1 份 JSON)/ 树 = 「13 份 + README + 1 份 freeze manifest」 |
| N5 | §4.2 表内估时逐行合计 **55 周 > 1 年** → 季度列不可加总 | NIT | ✅ 脚注补「**不可加总 / 不可逐行复算**;排期承诺只认真源串行下界」 |

> **本次修订自身引入并已修复的 2 处问题**(留痕):①**不可达路径字面量** 3 个(`include/tlm/cep/` 下的 glob 写法等)→ 被 `cep_doc_ref_check.sh --strict` 判红 → 改为目录 / 散文形式;②§10 表 **6 行 4 列被误写为 3 列** → 已恢复 4 列。**教训**:本批的校验必须带 `--strict`(非 strict 模式不判路径不可达)。

**⑫ 动态量快照登记(防「下次提交后再次漂移」)**

| 量 | 快照值(2026-10-23) | 性质 | 真源 |
|---|------------------|------|------|
| 全仓 case / assertion | **1857 / 81202** | **动态** —— 任意测试提交即漂移 | 实测(`./build/bin/cpptlm_tests`) |
| `[cep]` case / assertion | **76 / 1284** | 动态 | 实测(`[cep]`) |
| ctest 用例组数 | **96**(CEP 子集 **18**) | 动态 | 实测(`ctest`) |
| 07 号行数 | **686** | 随 ADR 增补漂移 | 实测 |
| ADR 族总行数 | **4859**(基线 5426,−10.4%) | 快照 | git 历史(Entry #20/#21) |
| ADR 份数 / 冻结成员 / invariant 数 / change 数 / 季度数 | **21 / 29 / 66(67) / 11 / 11** | **结构量** —— 变更须走 ADR | `ADR-CEP-NUMBER-MAP §2` · freeze manifest · `07 号 §4` · `ADR-CEP-00 §2 Decision 3` |

> **规则**:①**动态量**的快照值**允许过期** —— 判读当前状态一律以**实测**为准,**不得**当契约值引用;②**结构量**变更**必须**走 ADR + `--update --authorized-by` + 台账 entry;③ 本表是**快照登记**,**不是**第 5 处口径源 —— 动态量**不设**单一真源(其真源就是"实测"本身)。

### Entry #23 — 2026-10-23 [sprint-review]

**主题**: Stage 0 **自审**回执(变体 B —— 独立性未满足 → **R10 仍未闭合**)+ **实施路径排队真源落到路线图 §4.0** + 评审包刷 **v1.2**

**触发**: DRI 指示三件事 —— ①「治理与排队是基于 change 的内容,可以同时更新路线图文档来体现这个排队吗,**路线图文档才是实施路径的真相源**」②「其他内容我通过自审,可以标注『**自审,独立性未满足**』」③「同时把包刷新为 **v1.2**(补入 3 项披露 + 修正 §6 台账指针)」。

**① 实施路径排队真源 = 本文件 §4.0(新增,权威)**

| 维度 | 唯一真源 | 本文件的角色 |
|------|---------|-------------|
| **治理口径**(change 总数 / 串行规则 / 并发上限 / 变更名) | `ADR-CEP-00 §2 Decision 3` | **只引用,不重算** |
| **实施路径**(顺序 / 季度 / 状态 / 准入前置归属) | **本文件 §4.0 排队总表** | **权威承载** |

- **§4.0 实施路径总排队表**:**11 个 change × 1 季度,串行 2026-Q4 ~ 2029-Q2**(队列 #1 umbrella → #11 ualink-inc),含**状态**(🟡 in-progress / ✅ / 📋 / ⏸)与**承接的 Stage 1 准入前置**。
- **§4.2 ~ §4.4 的「季度」列已撤除** —— 旧的「乐观并行估计」与 §4.0 的串行季度**并存即两个数**,且表内估时逐行合计 55 周 > 1 年,无法自洽 → **统一到串行口径**。三表现只承载**估时 + 关联 ADR + 语义**。
- **顺带修正**:旧 §4.3 首行序号 **7** 与 §4.2 末行 **7** 撞号 → 全局重编为 **#1 ~ #11**(§4.0 为唯一编号来源)。
- ⚠️ **旧序号以本条为准**:**Entry #22 正文内**出现的 `#1 / #3 / #5` 等序号是**重编前**的 Stage 1 局部编号(append-only,不改写);**当前**全局序号 = 本条 §4.0 的 **#1 ~ #11**。
- **§4.5 / §4.6 季度口径同步**:Stage 0~3 实施完成 = **2029-Q2**;Stage 4+ 维护期 = **2029-Q2 起**;原乐观估计 2028-Q2 已废弃。
- **§5.1 拆成两条**:治理口径真源(ADR)/ 实施路径真源(§4.0)—— 此前把二者混为一句「只引用不自行重算」,与「路线图承载排队」相冲突,已拆开。
- **`phase-10.md`** 的依赖图降级为**示意**(明写「不承载排期承诺」)+ 加排期真源指针。

**② 自审回执(评审包 §5 变体 B)—— R10 仍未闭合**

| 项 | 结论 |
|----|------|
| 评审包 | `docs/implementation/2026-10-22-cep-stage0-external-review-summary.md` → **v1.2**(补 §3.1 三项披露 + 修 §6 指针 + 增 §5 变体 B) |
| 自审回执 | `docs/implementation/2026-10-23-cep-stage0-self-review-receipt.md`(**新建**)—— 结论「**无阻断项(自审)**」,5 个检查点逐条给出依据 |
| **独立性** | ⛔ **未满足** —— 复审人即 Stage 0 契约层的实现与撰写者(per §9.2 取证:两仓 commit 同一作者) |
| **R10** | ⛔ **仍未闭合** —— 变体 B 不满足 §5 变体 A 的声明要求 |
| **Stage 0** | ⛔ **仍不 archive** —— `cpptlm-cep-umbrella` 保持 🟡 Proposed / in-progress(105 / 99) |
| 解锁条件 | 一份**真正未参与** Stage 0 契约层撰写者的**外部真人**回执(变体 A) |
| 记录纪律 | 自审回执**不得**被表述为「已完成外部验收」或「R10 已闭合」 |

**③ 评审包 v1.2 的 3 项披露(原 v1.1 缺,直接命中检查点 ①)**

| # | 缺口 | 与评审的关系 |
|---|------|-------------|
| **G-1** | `ADR-CEP-NUMBER-MAP §2 决策` 仍写「Stage 1 = 6 串行 / 总 **10** 个 change」,与 `ADR-CEP-00 §2 决策 3` 的 **11** 冲突 | **直接命中检查点 ①「跨 ADR 无矛盾」** —— 已登记(Entry #22 L2),批 4 暂缓(改它 = 改冻结面成员) |
| **G-2** | `openspec validate --strict` **rc=1**(2 预存 WARNING) | 已登记(L1),批 4 暂缓 |
| **G-3** | ctest 全量曾偶发失败 1 次(**未复现**) | 已登记(L5),观察项 |

> **披露理由**:检查点 ① 问的正是「跨 ADR 无矛盾、计数口径统一」,而 G-1 恰在该领域 —— 不知情则可能对「自洽」给出**失真的**「无阻断项」。

**④ 契约面零改动(红线)**

- **零 ADR 改动**(21 份零 diff)· **零 `07 号` 改动** · **零 `include/tlm/cep/` 下 3 个 schema / `experimental/metrics.schema.json` / `tools/` 下 3 个 py / umbrella `spec.md` 改动**。
- **§11 Entry #1 ~ #22 正文零改写**(append-only;过期项一律由本条覆盖)。
- **零 `protocol_version` 取值 / 零 invariant 定义 / 零 ABI 改动**;**§4.0 排队表未新增 change、未新增 KPI slot**(仍是 11 个 change / 串行 11 季度,与真源一致)。

**⑤ 改动清单**

| # | 文件 | 改动 |
|---|------|------|
| 1 | `.rddf/roadmap/objectives/obj-cep-platform.md` | §4 术语约定(+ 真源分工)· **§4.0 总排队表(新增)** · §4.1 ~ §4.4(去季度列 + 全局重编)· §4.5 / §4.6 季度口径 · §5.1 拆两条真源 · §8.1 R10 行 · 本条 Entry |
| 2 | `.rddf/roadmap/phases/phase-10.md` | 排期真源指针 · 依赖图降级为示意 |
| 3 | `docs/implementation/2026-10-22-cep-stage0-external-review-summary.md` | **v1.2**:头部版本 + 两条评审路径 · **§3.1 三项披露(新增)** · §5 **变体 B(新增)** · §6 指针修正 + 2 行 · 底部版本与关联 |
| 4 | `docs/implementation/2026-10-23-cep-stage0-self-review-receipt.md` | **新建** —— 自审回执(变体 B) |
| 5 | `docs/README.md` | `implementation/` 计数 14 → **15**(.md 含 README;另有 1 份 freeze manifest JSON) |

**⑥ 验收**

```
bash scripts/test/cep_docs_sync_check.sh --strict   == 0
bash scripts/test/cep_doc_ref_check.sh --strict      == 0
bash scripts/test/cep_freeze_check.sh --strict       == 0
bash scripts/test/cep_archforge_alignment_check.sh   == 0
ctest --test-dir build -j4                           == 96/96
./build/bin/cpptlm_tests "[cep]"                     == 0(76 case / 1284 assertion)
git diff --name-only → 冻结面成员集合 = 0 行
```

### Entry #24 — 2026-10-23 [sprint-review]

**主题**: Entry #22 ⑩ **L5**(`ctest` 偶发失败)**根因定位并关闭** —— `cep_tools_contract` 与 `cep_all` **并行跑同一组用例** + **固定名**临时目录 → **竞态**;已起草 improvement 交 builder

**触发**: Entry #23 的最终验收中,`ctest -j4` **再次**复现 1 次失败,且**首次抓到用例名**(`Test #82: cep_all` → `test/test_cep_tools_contract.cc:115`)→ 根因定位。Entry #22 ⑩ L5 记为「偶发 1 次 · **未复现**」—— 本条**关闭该观察项**并给出根因(L5 的登记文字**不改写**,append-only)。

**① 根因(实证)**

| 项 | 事实 |
|---|------|
| **并行源** | ctest 注册 `cep_tools_contract` → `cpptlm_tests "[cep][tools]"` 与 `cep_all` → `cpptlm_tests "[cep]"`(**后者包含前者全部用例**);`-j4` 下两进程并行 |
| **共享路径** | `test/test_cep_tools_contract.cc:54-57` `make_exp_dir()` 用**固定名** `temp_directory_path() / ("cep_tools_" + tag)`(tag = `gate_good` / `gate_bad` / `ab_run`);另 `:124` `cep_tools_ab_out`、`:183` `cep_ast_dep_check.py` 同型 |
| **竞态** | 进程 A 的 `remove_all(base)` 与进程 B 的 `render_report` 读写**同一目录** → B 的 `report.html` 被删 / 被覆盖 → 断言 `html.find("一致性违例")` == npos → 失败 |
| **复现率** | `ctest -j4` **3~4 次复现 1 次**;**串行必绿**(不带 `-j` 与单跑 `[cep]` 均 100% 绿) |
| **性质** | **测试执行环境隔离缺陷**,非产品代码缺陷;**零**冻结面 / 零 ADR / 零 tools 影响 |

**② 处置:已起草 improvement,交 builder(planner 不改测试代码)**

- 草稿:`.rddf/improvements/cpptlm-cep-tools-test-isolation.md`(5 段;改动面 = `test/test_cep_tools_contract.cc` **单文件数行**;零冻结面 / 零 tools 改动)
- **验收核心**:`ctest --test-dir build -j4 --repeat until-fail:20 -R "cep_(all|tools_contract)"` == **0**
- **KPI**:**不新增 change、不新增 slot** —— 落 `cpptlm-cep-umbrella` 的 `tasks.md` 追加一条 task
- ⚠️ **不接受** `RUN_SERIAL TRUE` 绕过 —— 那是**绕过**竞态而非消除;正解 = 临时路径**跨进程唯一**

**③ 本条为台账第 24 条**;Entry #23 的最终验收:4 个守门脚本 + `docs_sync_check` + `markdown_link_check` + `ctest 96/96` + `[cep]` 76/1284 **全绿**,冻结面 29 成员 **0 diff**。

### Entry #25 — 2026-10-23 [scope-change]

**主题**: **载体分工与状态更新协议落地(obj §4.7)** + 修掉 umbrella ↔ 路线图的 **3 处指针缝隙**(M1/M2/M3)

**触发**: DRI 指示「请把分工写到对应文档里,后续新的会话在推进实施时,是否可以清楚他们的关系,以及实施过程中如何更新状态」。前置核查(obj vs umbrella 交叉核对)发现 **8 项口径已对齐**,但存在 3 处**结构性缝隙** —— 数字对、指针不闭合。

**① 新增 obj §4.7「载体分工与状态更新协议」(单一真源)**

| 小节 | 内容 |
|------|------|
| §4.7.1 | **五类载体表** —— objective(为什么/排队/台账,planner 写)· phase(子任务拆解,planner)· improvement(为什么值得做,planner)· **OpenSpec change(做什么/怎么验/做到哪 = 执行真源,builder)** · 实施笔记(时点快照,append-only) |
| §4.7.2 | **change 状态机** —— 📋 Planned → 🟡 Proposed/in-progress → ✅ archived,旁支 ⏸ 队列位保留;每态给**机器判据** |
| §4.7.3 | **六步同步清单** —— 起步 / 建 change / 每勾一个 task / 阶段或卡口变化 / 实施事件 / 收尾,每步列「动哪些文件」 |
| §4.7.4 | **新会话 3 步文档地图** + ⛔ 两个常见误读(依赖图不是排期 / 草稿不是验收判据) |

**三条硬规则**(写入协议,后续以此为判据):
1. **状态唯一权威 = OpenSpec change 的 `tasks.md` 勾选 + `proposal.md` 状态行**;§4.0 状态列是**投影**,不一致时以 change 为准并立刻回填 §4.0。
2. **数量口径只在 ADR 写**(11 change / 11 季度 / 21 ADR / 66 invariant / 29 冻结);其余载体一律**引用不复算**。
3. **实施笔记 append-only**,过期快照不改写。

**② 指针双向闭合(5 类载体各加 1 行,协议只在 §4.7 写一次)**

| 载体 | 新增指针 |
|------|---------|
| `phase-10.md` | 「载体分工/状态怎么更新 = obj §4.7」+ 说明本节只是 Stage 0 子任务展开 |
| `.rddf/roadmap.md` | CEP 载体分工协议 → obj §4.7;实施路径排队真源 → §4.0 |
| improvement 草稿 | 「本文 = 5 段规划输入;执行契约在 change;排队在 §4.0;协议在 §4.7」 |
| `proposal.md` | 「本 change = 执行真源;状态真源 = 本 change `tasks.md` 勾选」 |
| `tasks.md` | 上游补 **§4.0 / §4.7**;两段准入前置各加**归属行**(S1-1~S1-5 → 队列 #2;S1-7 ③ → #4,①② → #6) |

**③ 修掉的 3 处缝隙**

| # | 缝隙 | 修法 |
|---|------|------|
| **M1** | obj §6.1 委托范围写「§Acceptance **A1~A6**」,草稿实为 **A1~A7** | §6.1 改 3 行来源表,明确 **A1~A7**(A7 = 前置 gate 汇总,非 assertion) |
| **M2** | 验收指针指向 **improvement 草稿**,但实测回填在 **change 的 `proposal.md`** | §0 + §6.1 分源标注:**执行真源 = `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/proposal.md` §Acceptance**;草稿 = 上游规划输入 |
| **M3** | 准入前置的**归属**只在 §4.0,umbrella 侧不可寻址 | `tasks.md` 两段准入前置加**归属行**;`tasks.md:3` / `phase-10.md:43` / obj §0 的「§4.2」引用统一改 **§4.0** |

**④ 顺带清理**

- 删除 obj §0 内**重复**的「验收层级」注记(同一句话出现两次,其中一次指针错误)。
- Entry #23 ⑨ 改动清单外,**本条不改任何 ADR / schema / 冻结面成员**。

**⑤ 验收**

```
cep_docs_sync_check.sh --strict / cep_doc_ref_check.sh --strict / cep_freeze_check.sh --strict /
cep_archforge_alignment_check.sh --strict / cep_decl_alignment_check.sh --strict   == 0 (5/5)
docs_sync_check.sh --strict == 0 · markdown_link_check.sh == 0
ctest 96/96 · [cep] 76 case / 1284 assertion · 冻结面成员 diff == 0
```

**关联**: Entry #22(M 系列口径)/ #23(排队真源 §4.0 + 自审 + 包 v1.2)/ #24(tools 测试隔离)· `ADR-CEP-00 §2 Decision 3`(治理口径真源)

### Entry #26 — 2026-10-23 [scope-change]

**主题**: **Entry #25 的协议增补** —— critic 复审 4 项放行条件(S1~S4)+ 3 NIT 全部落地;**本条覆盖 Entry #25 ② 与 Entry #25 ⑤ 中已过期的描述**

**触发**: Entry #25 落地后 critic 对抗复审 → **APPROVE_WITH_CONDITIONS**(3 BLOCKERS 已在上一条落地前关闭,剩 4 项放行条件)。

> ⚠️ **覆盖声明(append-only 纪律下的正确做法)**:Entry #25 正文不改写。凡与下列冲突,**以本条为准**:
> | Entry #25 旧描述 | 本条修正 |
> |---|---|
> | ② 「§4.7.4 **两个**常见误读」 | 现为 **4 条**(+AGENTS.md KPI 快照 / +转述计数),并追加以 **L6** 为核心的**第 5 条**(07 号 §8.1) |
> | ③ 「§4.7.1 **状态唯一权威**...§4.0 状态列是它的**投影**」 | 现明确 **3 个投影**(§4.0 状态列 / phase-10 子任务表 / improvement front-matter),任一漂移都以 change 为准 |
> | ⑤ 「cep_* 5 个守门脚本 --strict 全绿」(当时未含 docs/link) | 现为 **7 项**:5 个 cep 脚本 + `docs_sync_check --strict` + `markdown_link_check` |

**① S1 — 冻结面「10 change」正式登记为 L6**(此前只在批 4 备注里口头提过)

- `docs/architecture/07-coherence-exploration-platform.md §8.1`(:543-553)写「Stage 1 = 6 change / 合计 10 / 串行 10 季度」,**该文件是 M1 冻结面 29 成员之一**(`2026-10-21-cep-stage0-freeze-manifest.json` 实测 `frozen: True`)→ **不改**,登记为 **§11 L6**。
- **§4.7.4 误读第 5 条**:新会话不得据 07 号 §8.1 推断排期;Stage 划分 / change 数一律以 §4.0 + `ADR-CEP-00 §2 Decision 3` 为准。
- **§4.0 自检从「单边」升级为「双边」**:只查新值(`2029-Q2`)会漏「**旧数复活**」;现同时查旧终态(`2029-Q1`)+ 旧串行数(`串行 10 季度` / `10 个 change`),并声明**预期例外 = §4.0 所列 3 处**(`07 号 §8.1` = L6 / `ADR-CEP-NUMBER-MAP §2` = L2 / `.rddf/drafts/` 草稿)—— **不是**只有 07 号一处。

**② S2 — improvement 草稿的归属指针**:`.rddf/improvements/cpptlm-cep-umbrella.md:34` 的「`obj §4.2` 可寻址」→ **`obj §4.0` 排队表**(逐项 owner → 队列 #2 / #4 / #6)。此为该文件**最后一处** §4.2 归属残留。

**③ S3 — 转正路径补 `slot` 硬前置**:§4.7.2 原 3 步转正路径**缺 KPI slot 守卫** —— 若在 umbrella 仍占 slot 时把 ⏸ 项转正,会瞬间出现 **2 个 Proposed**,违反 §5.4(并发 ≤ 1)。现补 **step 0**:`openspec list --json` 实测并发 < 1 才允许建 change,否则**先让出 slot**。

**④ S4 — 本条(NIT 随附)**

| # | NIT | 处置 |
|---|-----|------|
| N1 | 收尾清单 #1 把「⏸ 段」的落点指向 `tasks.md`(**错**,应在 `proposal.md` 顶部) | 拆成 #1(`tasks.md` 勾选)+ **#1b**(`proposal.md` 顶部 ⏸ 段),清单数 6 → **7** |
| N2 | 「六处同改」(§4.0)与「收尾清单六项」(§4.7.3)量词撞车,易混 | §4.0 → **五处同改**(本表为触发源,不计入);§4.7.3 → **收尾 7 个落点** |
| N3 | §4.0 ⑥「本表本身」同义反复(改动就发生在给它) | 删除,改在标题行写「本表是触发源,不算在内」 |

**⑤ 验收(7 项全绿)**

```
cep_docs_sync_check / cep_doc_ref_check / cep_freeze_check / cep_archforge_alignment_check / cep_decl_alignment_check  --strict  == 0 (5/5)
docs_sync_check.sh --strict == 0 · markdown_link_check.sh == 0
ctest 96/96 · [cep] 76 case / 1284 assertion
```
**双边自检实测**:新值 grep → **空**;旧值 grep → **3 处命中,与 §4.0 声明的「已知例外」逐条对应**(`07 号 §8.1` = L6 / `ADR-CEP-NUMBER-MAP §2` = L2 / `.rddf/drafts/` 草稿)。冻结面 29 成员 diff = **0**。

**关联**: Entry #25(协议落地)/ #23(排队真源)· `AGENTS.md` §DOC HYGIENE「两种归档语义必须区分」· `ADR-CEP-00 §2 Decision 3`

**关联**: Entry #12(P-1 增补 / 11 change)/ #13(R-2 已触发 + A6 闭合)/ #15/#16(S2 实测 + ctest 回归门)/ #17(R10 评审包 v1.1)/ #20/#21(ADR 族两轮精简)/ #22(路线图口径同步 + L5 登记)/ #23(排队真源 + 自审 + 评审包 v1.2)· `ADR-CEP-00 §2 Decision 3` · `ADR-CEP-NUMBER-MAP §2 决策`(已知缺陷 G-1)

---

## §12 Review Schedule

- **2027-01-18**(review_by 默认 = created + 90 天):第一次 sprint-review
- 后续按 90 天周期 + Stage 晋级节点追加

**review_by 触发仪式**(per `rdd-planner/SKILL.md:159`):
- `bash skills/rdd-planner/scripts/planner_objective_revise.sh objective-cep-platform --kind sprint-review --content "..." --decision "..." --reason "..."`
- kind ∈ {deferral-rationale, go-decision, sprint-review, scope-change, adr-amendment}

### Entry #27 — 2026-10-23 [adr-amendment]

**主题**: **批 4 —— 冻结面授权修订**:①**L2 闭合**(`ADR-CEP-NUMBER-MAP §2` 的 10 → 11)②**L6 闭合**(`07 号 §8.1` 的 6/10 → 7/11 + 移除复制的季度值)③**umbrella spec.md 的 2 条 openspec WARNING 消除**(#5)④ **tools 测试隔离修复落地**(#4,Entry #24 根因关闭)

> **本条即冻结面授权记录** —— `scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #27"` 的授权依据。
> **冻结面 29 成员**中本条触及 **3 个**:`ADR-CEP-NUMBER-MAP.md` / `07-coherence-exploration-platform.md` / `umbrella specs/cep-core-contracts/spec.md`。

**① L2 闭合 —— `ADR-CEP-NUMBER-MAP §2 决策 4` 的 KPI 预算块**

| | 原值 | 现值 | 权威依据 |
|---|---|---|---|
| Stage 1 | 6 串行 × 1 slot | **7** 串行 × 1 slot | `ADR-CEP-00 §2 Decision 3`(Stage 0=1 + Stage 1=**7** + Stage 2=2 + Stage 3=1) |
| 总计 | 10 个 change,串行 10 季度 | **11** 个 change,串行 **11** 季度 | 同上 |

- **性质**:事实性更正(旧值**从未**是权威值)—— 真源 3 内部矛盾。**写法沿用** `ADR-CEP-00 §2 决策 3` 的「2026-10-22 增补」同款:**原地更正 + 标注增补块 + 台账 entry**。
- **ADR 不可变原则的处理**:AGENTS.md 要求 ADR「签发后不改,状态变化追加 `## Status Update` 段」。本条走的是**第三种形态** —— 原值是**事实错误**而非状态变化,若只追加 Status Update 则错误正文留存、真源 3 内部矛盾继续;故**原地更正 + 增补块留痕**,并在本台账登记授权。

**② L6 闭合 —— `07 号 §8.1` Stage 表**

| | 原值 | 现值 |
|---|---|---|
| Stage 1 change 数 | 6 | **7** |
| 合计 | **10** | **11** |
| 串行季度 | 串行 10 季度 | **串行 11 季度**(并入 KPI 约束行) |
| 时间跨度列 | 5 处季度值(2026-Q4 ~ 2028-Q2) | **整列移除** |
| 估时列 | 30 人日 / 8 周 / 10 周 / 10 周 | **整列移除** |

- **为什么删列而不只是改数**:单源纪律(§4.0「单源纪律」)要求**具体季度值只出现在 objective 内**;07 号复制的 5 处季度值 + 4 处估时是**漂移源**(Stage 0 估时已从 30 人日漂到 46 人日)。现 07 号 **§8.1 只承载「Stage 划分 + change 数」**(ADR-00 派生的稳定事实),排期指 §4.0、估时指 §4.1~§4.4。
- **代价**:真源 1 文档指向路线图层(`.rddf/`)—— 可接受,因为 Stage 划分本身**派生自** `ADR-CEP-00 §2 Decision 3`,且 §4.0 是其实施路径投影。

**③ #5 闭合 —— umbrella `spec.md` 的 2 条 `openspec validate --strict` WARNING**

| WARNING | 根因 | 处置 |
|---|---|---|
| `Text above the first "## " section is not carried into the main spec by archive` | spec.md 头部 5 行元信息(状态/配套/版本/上游真源/验收归属)写在首个 `##` 之前 | **迁出到 `proposal.md` 头部**(元信息本就属 change 层) |
| `Text before the first requirement in "## ADDED Requirements" is not carried into the main spec by archive` | spec.md 的「编号位口径」块(67 编号位 / 66 实际 / 跳空 ID 纪律)写在首个 `### Requirement:` 之前 | **迁出到 `design.md` 头部**(计数纪律属实施/校验侧) |

- **实测**:`openspec validate cpptlm-cep-umbrella --strict` 由 **rc=1(2 WARNING)** → **rc=0(is valid)**。
- **未改**:15 个 requirement、15 个 `#### Scenario:`、任何规范条文 —— **0 条契约语义变更**。
- **这不是「藏起来」**:迁出内容**有去处**且在 proposal/design 中**原文保留**;留在 spec.md 的后果是归档时被**静默丢弃**(比 WARNING 更坏)。

**④ #4 闭合 —— tools 测试隔离(Entry #24 根因 → 修复)**

- **根因**:`ctest` 注册了两个跑同一组 `[cep][tools]` 用例的测试项(`cep_tools_contract` → `[cep][tools]`、`cep_all` → `[cep]`),`-j4` 下并行;而 `test/test_cep_tools_contract.cc` 用**固定名**临时目录(`/tmp/cep_tools_<tag>`)→ 进程 A 的 `remove_all` 与进程 B 的读写**竞态**。
- **修复**:3 处固定名路径全部加 **PID 唯一化 token**(跨进程必异、进程内稳定);判定脚本用完即删。**仅改测试代码**,零冻结面 / 零 ABI。
- **对照实验(修复前 → 修复后)**:同一命令 `ctest -j4 --repeat until-fail:20 -R "cep_(all|tools_contract)"` —— 修复前**第 1 轮即 Failed**,修复后 **20 轮全绿**。命令具备鉴别力,不是碰巧绿。

**⑤ 验收**

```
openspec validate cpptlm-cep-umbrella --strict      == 0(原 rc=1 / 2 WARNING)
cep_docs_sync_check / cep_doc_ref_check / cep_freeze_check /
cep_archforge_alignment_check / cep_decl_alignment_check  --strict  == 0 (5/5)
docs_sync_check --strict == 0 · markdown_link_check == 0
ctest 96/96 · [cep] 76 case / 1284 assertion
冻结面 29 成员 diff == 0(本条改动已走 --update --authorized-by "Entry #27" 授权闸并重生成清单)
双边自检:新值 grep 空 · 旧值 grep 仅 1 处(drafts 草稿)
```

**⑥ 本条之外的遗留状态**

- **L1**(`openspec validate --strict` rc=1)—— **本条已闭合**(见 ③)。
- **L2 / L6** —— **本条已闭合**(见 ① ②);§11 L 表正文保留历史原样,状态以本条为准。
- **L3**(Entry #20 表列数错位)/ **L4**(`ONBOARDING.md` 管道符未转义)/ **L5**(ctest 偶发)—— L5 **本条已闭合**(见 ④);L3 / L4 仍为**历史遗留,不改**。

**关联**: Entry #24(ctest 根因)/ #25(载体分工协议)/ #26(协议增补)· `ADR-CEP-00 §2 Decision 3`(治理口径真源 = 11)· improvement 草稿 `.rddf/improvements/cpptlm-cep-tools-test-isolation.md`

### Entry #28 — 2026-10-23 [scope-change]

**主题**: **R10 处置** —— 闭合判据从「外部真人回执」改写为「**M1/M2/M4 落地证据 + 残余风险登记 + DRI 书面接受**」;Stage 0 的 6 项 Stage 1 准入前置**移出归档判据**

> **本条同时是 Phase B 的冻结面授权依据**(`cep_freeze_check.sh --update --authorized-by "Entry #28"`)。

**触发**:DRI 指示 ——「项目开发者就我一个人,我无法实施外人来进行评审」。这是**结构事实**,不是拖延:原判据「等外部真人回执」在单开发者仓形态下**永远不可达**(与 ArchForge 6/6 commit 同一作者同构)。

**① 判据改写的先例对齐**:`ADR-CEP-00 §8.1` 当初把准入门 **C-1** 从「ArchForge 侧存在已合入的**接口 PR**」改写为「存在已合入的**接口规约文档**」,理由是「ArchForge 是纯设计仓,**结构上无法**产出接口 PR → 修订前措辞下 C-1 **永远无法闭合**」。本条**同构处理 R10**:不改「要独立性」的**意图**,只把**不可达的门禁**换成**可达且可验证**的判据。

**② 闭合判据(逐条 + 证据)**

| # | 条件 | 证据 | 状态 |
|---|------|------|:---:|
| ① | M1 产物冻结先于解读 | 冻结清单 29 成员 + SHA-256 + 授权闸 + provenance 自检 | ✅ |
| ② | M2 跨仓准入门 | ArchForge `6020f42` + `8290b4c` + `ac956b8` | ✅ |
| ③ | M4 独立 AI 复审产出实质发现 | Oracle 9 项(2026-10-21)+ critic 4 轮 3 BLOCKER(2026-10-23) | ✅ |
| ④ | 残余风险登记 + DRI 书面接受 | `obj §9.3` + 本条 | ✅ |
| ⑤ | M3 对拍基准外置 | **不适用**(服务实现期对拍;Stage 0 无实现交付物)→ 保留 Stage 1 的 P0 前置 | ⏳ 不阻塞 |

**③ 残余风险(DRI 书面接受,2026-10-23,本次会话)**

- **风险**:Stage 0 契约层结论**未经独立第二方(人类)复核** —— 同一人产出 / 冻结 / 解读;M1~M4 是**过程性**独立,非**结论性**独立。
- **接受**:由 DRI 承担(本条即书面接受)。**触发补做**:引入第二位开发者或任何外部消费者时补一次独立复审(增量信心,不阻塞已归档契约)。
- **不掩盖**:R5 的「实质未闭合」措辞保留为历史描述;§9.3 明写「不被『闭合』二字掩盖」。

**④ Stage 0 的 6 项 Stage 1 准入前置 → 移出归档判据**

`tasks.md` 的 S1-1 ~ S1-5 + S1-7 **本就属于 Stage 1**(原注:「不计入 Stage 0」),此前以未勾 checkbox 形式留在 Stage 0 的任务表里,导致 `openspec archive` 判为「6 incomplete task(s)」而拒绝归档。现改为**移交条目**(去 checkbox + 标注归属队列),归属不变:

| 条目 | 归属队列(per §4.0) |
|------|------------------|
| S1-1 ~ S1-5 | **队列 #2 `cpptlm-cep-poc-spike`**(2027-Q1) |
| S1-7 ③ (β) 重新评估 | **队列 #4 `cpptlm-cep-directory-ordering-plane`** |
| S1-7 ①② `perm` 形态 schema 演进 | **队列 #6 `cpptlm-cep-chi-rnf-subset`** |

- Stage 0 任务表因此 **99 / 99 全勾**(openspec 的 task 计数 = checkbox 数;原 105 项 = 99 已勾 + 6 移交)→ 满足 §4.7.2「完成归档」判据(`tasks.md` 全勾 + 实现已落地)。
- **不是**「勾掉未做的事」:条目文本、判据、归属全部保留,只是**移出 Stage 0 的归档判据**(判据归属见 §4.0 排队表)。

**⑤ 状态回填**

| 位置 | 变更 |
|------|------|
| `tasks.md` 状态行 | tasks **99 / 99**(原 105 项,6 项移交);R10 **已按 §9.3 闭合** |
| `proposal.md` 状态行 + §Acceptance | 同上(tasks **99 / 99**);「⛔ R10 未闭合」→「✅ 已闭合(残余风险已登记)」 |
| `obj §8.1` R10 行 | 🟡 中 → 🟢 低(**已接受残余风险**) |
| `obj §9.2` | **正文不改**(append-only),新增 §9.3 处置段 |

**⑥ 验收**:`openspec validate cpptlm-cep-umbrella --strict` rc=0 · 7 项守门 rc=0 · `ctest` 96/96 · `[cep]` 76 case / 1284 assertion · 冻结面 29 成员 diff=0。

**关联**: Entry #9/#10(M2 放行)/ #13(判据 2 触发)/ #23(排队真源 §4.0)/ #27(批 4 冻结面授权)· `ADR-CEP-00 §8.1`(判据改写先例)· `obj §9.2` M1~M4

### Entry #29 — 2026-10-23 [scope-change]

**主题**: **`cpptlm-cep-umbrella` 完成归档(Stage 0 收尾)** —— delta spec 合并进 main specs + 全仓路径迁移 + 3 个门禁脚本的路径绑定适配 + 冻结面成员迁移(29 → 30)

> **前置**:Entry #28(R10 闭合判据 + 残余风险登记 + 6 项准入前置移交)—— 归档判据由本条执行。
> **归档语义**:**✅ 已完成归档**(非「⏸ 暂存归档」)—— 实现已落地 + 契约已在系统内(delta spec **已合并**进 `openspec/specs/cep-core-contracts/spec.md`)。

**① 归档动作**

| 项 | 结果 |
|---|---|
| 命令 | `openspec archive cpptlm-cep-umbrella --yes` |
| 归档目录 | `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/`(4 件套 + `.openspec.yaml`) |
| main spec | **新建** `openspec/specs/cep-core-contracts/spec.md` —— **15 个 requirement 合并**(+ 15 / ~ 0 / - 0) |
| Purpose | 归档生成的 `TBD - created by archiving change ...` 占位 → **改为真 Purpose**(契约真源 / 编号位口径 / 演进方式 / 归档前形态) |
| 判据核对 | `openspec validate cpptlm-cep-umbrella --strict` **rc=0**;`openspec validate cep-core-contracts --strict` **is valid** |

**② 路径迁移(活文档 7 份 / 27 处)**

| 文件 | 处数 |
|------|-----:|
| `.rddf/roadmap/objectives/obj-cep-platform.md` | 13 |
| `.opencode/plan/20261022-cep-contract-convergence-batch1.md` | 4 |
| `.rddf/improvements/cpptlm-cep-umbrella.md` | 3 |
| `.opencode/plan/20261022-cep-remaining-batches.md` | 3 |
| `.opencode/plan/20261022-cep-decision-brief.md` | 2 |
| `.rddf/improvements/cpptlm-cep-tools-test-isolation.md` | 1 |
| `docs/research/cep-coherence-protocol-space-survey.md` | 1 |

- **不迁移**(有意):`docs/implementation/*`(历史快照,**append-only**)+ `.rddf/drafts/*`(architect 草稿)。它们指向归档**前**位置,属**时点证据**。
- **兜底**:历史快照中的旧路径由 `cep_doc_ref_check.sh` 的 `VIRTUAL_PATHS` 登记(理由:已归档 change 的原路径)→ 不产生断链误报。

**③ 门禁脚本的路径绑定适配(3 个 checker;checker 自身**不冻结**,但必须可修)**

| 脚本 | 改动 | 为什么必须改 |
|------|------|------------|
| `cep_freeze_check.sh` | 冻结 glob:`openspec/changes/cpptlm-cep-umbrella/specs/.../spec.md` → **`openspec/changes/**/specs/cep-core-contracts/spec.md`**(兼容 active / archived 两态) | 归档后原路径消失 → 冻结面报「缺失」 |
| `cep_docs_sync_check.sh` | ① 扫描 glob 加 **`openspec/changes/archive/*/cpptlm-cep-umbrella/**/*.md`**(否则 umbrella 4 件套脱离 P6 全禁规则 = 守门覆盖度**静默下降**)② 加 `openspec/specs/cep-core-contracts/*.md`(新系统契约纳入 P6 规则)③ `CH` 路径改**动态解析**(active / archive 两态) | 归档使路径带日期前缀,硬编码必失效 |
| `cep_doc_ref_check.sh` | ① `DOCS` 里的 `tasks.md` 指向归档路径(否则 umbrella tasks 脱离「契约面矛盾」检查)② `VIRTUAL_PATHS` 登记旧路径 | 同上 |

> **⚠️ 一次被实测否掉的方案**:归档区**全扫**(`openspec/changes/archive/**/*.md`)→ 引入 **92 处 P6 假阳性** —— 仓内另有 **22 个**历史归档 change,其文档合法地提及旧 `06 §15.4` 的 P6 编号。故收窄为**只扫 umbrella 自己的**归档目录(**保持原覆盖度**,不扩大)。

**④ 冻结面成员迁移(29 → 30)**

| 成员 | 变化 |
|------|------|
| umbrella delta spec | 路径随归档迁移 → glob 化(两态兼容) |
| **`openspec/specs/cep-core-contracts/spec.md`** | **新增冻结** —— 归档后**系统契约**落在 main spec;不冻结它 = 契约面可被静默改写(**M1 失效**) |
| `tools/*.py`(3) | 头注释的 change 路径随迁(注释级,不改行为) |

**⑤ 状态回填**

| 位置 | 变更 |
|------|------|
| `obj §4.0` 队列 #1 | 🟡 in-progress → **✅ archived** |
| `obj §5.2` | Proposed 并发 **1/3 → 0/3**;**下一个可启动 = 队列 #2 `cpptlm-cep-poc-spike`** |
| `.rddf/roadmap.md` | KPI baseline **1/3 → 0/3** |
| `phase-10.md` 10.B | 🟡 Proposed → **✅ 完成 + 已归档** |
| `AGENTS.md` | KPI 基线行已在同日更新为「现行 1/3」→ **本条归档后需再更新为 0/3** |

**⑥ 遗留(非本条引入)**

- **`openspec validate --specs --strict` rc=1** —— **80 个 main spec** 因归档遗留的 `Purpose` 占位符告警(与 CEP 无关,pre-existing);`cep-core-contracts` 自身 **is valid**。
- `.gitignore` 忽略 `openspec/changes/archive/` → 归档目录**需 force-add 入库**(仓内既有 **23 个**归档目录、**109 个**文件已跟踪,含 20 个「已完成归档」先例);否则归档 change 的 design/tasks 在 git 中**消失**。

**⑦ 验收**

```
openspec validate cpptlm-cep-umbrella --strict   == 0
openspec validate cep-core-contracts --strict    == is valid
5 个 cep_* --strict == 0 · docs_sync_check --strict == 0 · markdown_link_check == 0
ctest 96/96 · [cep] 76 case / 1284 assertion
冻结面 30 成员 diff == 0(--update --authorized-by "Entry #28" 授权闸)
```

**关联**: Entry #27(批 4 冻结面修订)/ #28(R10 闭合 + 移交)· `ADR-CEP-00 §2 Decision 3`· `obj §4.0`(队列真源)/ `§4.7.3`(收尾 7 落点)/ `§9.3`(R10 判据)

### Entry #30 — 2026-10-23 [scope-change]

**主题**: **critic 第 5 轮 REJECT 的 4 BLOCKER + 8 SHOULD_FIX 全修**;并**补全 §4.7.3 收尾同步清单**(7 → 16 落点)

> **本条即冻结面授权依据**(`cep_freeze_check.sh --update --authorized-by "Entry #30"`)。

**① 为什么会有这轮 REJECT —— 根因不是执行疏忽,是协议缺陷**

critic 判 REJECT 的 B1 = 「归档后状态投影只回填了 §4.7.3 清单里的 3/7 落点」。追下去:**§4.7.3 的清单本身只有 7 项,漏了 6 类**(§4.1 各 Stage 展开行 / §8.1 风险行 / 头部同步注 / improvement front-matter / AGENTS.md 全仓 KPI 基线 / 门禁脚本路径绑定)。**按不完整的清单执行,必然漏回填** → 本轮把清单补到 **16 个落点**并分 A/B/C/D 四组。

**② B1 — 归档后状态投影全量回填(逐条)**

| 落点 | 修正前 | 修正后 |
|------|--------|--------|
| `obj` 头部同步注 | 「唯一卡口 = R10 真人回执(→ 仍不 archive)」 | 「umbrella **已完成归档**(tasks 99/99,R10 已按 §9.3 闭合)」 |
| `obj §4.1` 10.B 行 | 🟡 Proposed / in-progress | ✅ 完成 + 已归档 |
| `obj §4.1` 小结 | 「唯一卡口 R10 真人回执 → 不 archive」 | 卡口已闭合 + 残余风险指向 §9.3 |
| `obj §5.2` M2 框 | 「⚠️ Stage 0 仍不 archive」 | 「✅ Stage 0 已完成归档」 |
| `obj §8.1` R10 行 | 同格并存互斥结论(S5) | 清理为单一结论 + 指向 §9.3 |
| `phase-10.md` 卡口行 | 「R10 真人外部评审回执 → 不 archive」 | 卡口已闭合 + 已归档 |
| `improvement` front-matter + 「最后」行 | 🟡 Proposed / in-progress / 「R10 真人回执待达」 | ✅ 已完成并归档 + 下一个 = 队列 #2 |
| 归档 `proposal.md` / `tasks.md` 状态行 | 🟡 Proposed / in-progress | ✅ archived |

> **§11 台账内的历史表述不动**(append-only):那里的「不 archive / 未闭合」是**当时**的合法历史记录;**活文档区段**(§11 之前)必须与 §4.0 一致 —— 已加机器断言(见 ⑤)。

**③ B2 — 冻结成员数 29 → 30 全量回填(机器真值 = manifest)**

`obj §4.7.1` 规则 2 · `obj §9.3` ① · `docs/ONBOARDING.md` · `scripts/README.md` · `.rddf/improvements/cpptlm-cep-umbrella.md` · **冻结件 `docs/adr/ADR-CEP-NUMBER-MAP.md`**(其增补块自述「29 成员」)· 归档 `tasks.md` 的 M1 产物数。

> **30 = 29 + 归档后的 main spec**(`openspec/specs/cep-core-contracts/spec.md`)—— 归档后**系统契约**落在 main spec,不冻结它 = M1 失效。

**④ B3 / B4 / S1~S8 — 逐条**

| # | 问题 | 修法 |
|---|------|------|
| **B3** | P6 扫描 glob `archive/*/cpptlm-cep-umbrella/**/*.md` **匹配 0 文件**(归档目录名带**日期前缀**)→ umbrella 4 件套脱离扫描范围 = 守门覆盖度**静默回退** | 改为 `archive/*cpptlm-cep-umbrella/**/*.md`(实测命中 **4**);并加机器断言(见 ⑤) |
| **B4** | `openspec validate cpptlm-cep-umbrella --strict` 在归档后 **rc=1**(`Unknown item`),却被活文档与台账当作 rc=0 的证据 | 活文档改指 `openspec validate cep-core-contracts --strict`(rc=0 / is valid);台账 Entry #27/#28/#29 正文**不改写**(append-only),由本条覆盖声明 |
| **S1** | 冻结 glob `openspec/changes/**/specs/cep-core-contracts/spec.md` 的 `**` 可跨 0 层 → 未来任何 change 复用该 capability 名即被误报「新增未登记的冻结产物」(**前向陷阱**) | 收窄为 `openspec/changes/archive/*cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md` —— 只匹配**已归档的** umbrella |
| **S2** | 归档 `tasks.md` 的 S1-4 说「`fabric.{schema.json,json}` 再纳 M1 冻结集」,但 `fabric.schema.json` **早已**在冻结集内 | 加口径澄清:本条实际待纳的是 `fabric.json`(**实例数据**,不冻结),不是 schema |
| **S3** | 归档 `proposal.md` / `tasks.md` 内的验收结论仍写「rc=1 … 批 4 暂缓」 | 更新为「批 4 已闭合」+ 归档后的有效命令 |
| **S4** | §9.3「下列**全部成立**」含一个**不适用**项(⑤ M3) | 判据改为「**①~④ 全部成立**」,⑤ 单列为「不适用项,不参与判据」 |
| **S5** | §8.1 R10 行同一单元格内并存互斥结论 | 清理为单一结论 + 指向 §9.3 + 已归档事实 |
| **S6** | 自审回执以 `grep -c "^- \[ \]"` = **6** 作证据,移交后实测 **0** | **不改回执**(append-only),由本条声明:该证据**随 Entry #28 的移交而失效**,复现得 0 属预期,**不是**「丢了 6 项」 |
| **S7** | 注释把归档区全扫的假阳性归因为「92 处 P6 假阳性 / 22 个历史归档 change」—— 实测**归因错误**(P6 与 P0.5 混杂、条数随口径漂移) | 注释**不写死条数**,只写**决策**(只扫 umbrella 自己的归档目录 = 保持归档前覆盖度)与**理由**;归因更正见本条 |
| **S8** | 6 项移交后**无任何机器闸**保证队列 #2/#4/#6 承接 | 归属已写进 §4.0 排队表的「承接的 Stage 1 准入前置」列(人工可见);**机器兜底**见 ⑤ 的 ①②(状态投影一致性) |

**⑤ 机器兜底 —— 新增 `cep_docs_sync_check.sh` 第 11 组(3 条断言)**

| 断言 | 防的是 |
|------|--------|
| ① 活文档区段(§11 之前)无「umbrella 尚未归档」旧口径 | B1 重演(§4.0 说已归档、§4.1 说未归档)**且无机器闸** |
| ② 冻结成员数:manifest 真值 == 活文档声明 | B2 重演(29 vs 30 分裂) |
| ③ 归档件在 P6 扫描集内(glob 命中数 ≥ 4) | B3 重演(glob 写错 → 覆盖度静默回退) |

> **鉴别力实测**:人为向 `phase-10.md` 注入一行旧口径 → 第 11 组**立即报红**(rc=1)并指出文件;还原后恢复绿。**断言不是摆设**。

**⑥ 覆盖声明(append-only 纪律)**

> | 旧描述 | 本条修正 |
> |---|---|
> | Entry #29 ③ 的「92 处 P6 假阳性 / 22 个历史归档 change」 | **归因错误**,条数随口径漂移 → 不引用具体数字;以本条 ④ 的 S7 为准 |
> | Entry #27 ⑤ / #28 ⑥ / #29 ①⑦ 的 `openspec validate cpptlm-cep-umbrella --strict` == 0 | 该命令对**已归档** change 返回 rc=1;**现行为** `openspec validate cep-core-contracts --strict` rc=0 |
> | §4.7.3「收尾同步清单 7 个落点」 | **补全为 16 个落点**(A/B/C/D 四组),见 ① |

**⑦ 验收**

```
5 个 cep_* --strict == 0(含新增第 11 组)· docs_sync_check --strict == 0 · markdown_link_check == 0
openspec validate --changes --strict == 0 · openspec validate cep-core-contracts --strict == 0
ctest 96/96 · [cep] 76 case / 1284 assertion
冻结面 30 成员 diff == 0(--update --authorized-by "Entry #30" 授权闸)
新断言鉴别力:注入旧口径 → rc=1;还原 → rc=0
```

**关联**: Entry #27(批 4 冻结面修订)/ #28(R10 闭合 + 移交)/ #29(归档 + 路径迁移)· `ADR-CEP-00 §2 Decision 3`· `obj §4.0` / `§4.7.3` / `§9.3`

### Entry #31 — 2026-10-23 [scope-change]

**主题**: **critic 第 6 轮 REJECT 全修** —— 活区段 9 处陈旧投影 + 2 处失效验收命令 + **2 条机器兜底经实测无鉴别力**

> **本轮最重要的教训**:第 5 轮我声称「鉴别力实测:注入旧口径 → 报红」——**那是自证循环**:注入的串与我的正则**同形**,恰好命中。critic 指出后,本轮改用 **critic 实测列出的真实残留串**做注入矩阵,才发现 ① 正则只锁 4 个字面串、② 只覆盖 1 个文件、且**豁免词可被「加一个『已被取代』」绕过**。
> **判据从「我修过的字面串不许回来」升级为「旧口径的**语义类**一律命中 + 归档事实必须被引用」**。

**① B1 — 活区段 9 处陈旧投影全清(逐条)**

| # | 位置 | 旧口径 | 现口径 |
|---|------|--------|--------|
| 1 | obj §4.1 | `tasks.md 105 task / 99 完成` | `tasks 99 / 99 全勾` |
| 2 | obj §4.1 | `6 项未勾 全部是 Stage 1 准入前置` | `6 项已移出该 change 的任务表(条目/判据/归属全保留)` |
| 3 | obj §4.2 P-1 事实登记 | `因 R10 未闭合 → umbrella 不 archive → 唯一 KPI slot 不可让渡` | `umbrella 已完成归档 → slot 已释放 → 队列 #2 可启动` |
| 4 | obj §5.3 | `现为 Proposed` + 失效的验收命令 | `当前状态:已归档,并发 0/3;有效命令 = openspec validate cep-core-contracts --strict` |
| 5 | obj §8.1 **R10 行** | **5 格塞进 4 列表** → 列数错位,且第 5 格藏着 `R10 本身仍未闭合 → Stage 0 仍不 archive` 等旧结论 | **归一为 4 列**;旧结论移入表下新增的「**R10 状态沿革(历史结论)**」小节(逐条标 **(历史)**) |
| 6 | obj §9.2 净结论 | `已知未闭合的缺口` | 标为 `历史表述,现以 §9.3 为准` + `已知缺口` |
| 7 | obj §9.3 净结论 | `R10 由「⛔ 未闭合」改为` | `R10 的状态由「⛔ 阻塞」改为`(避免与旧口径正则同形) |
| 8 | obj §10 | `105 task / 99 完成` | `并于 2026-10-23 归档(tasks 99/99)` |
| 9 | phase-10 实施侧行 + **页脚** | `105 task / 99 完成` / `R10 真人回执待达 → 闭合后方可 archive 并启动 Stage 1` | `99/99 全勾` / `已归档 · Stage 1 可启动 → 队列 #2` |
| + | improvement 草稿状态行 | 失效命令 + `R10 真人外部评审未闭合 → Stage 0 不 archive` + `KPI 0/3 → 1/3` | 有效命令 + `R10 已按 §9.3 处置` + `0/3 → 1/3 → 0/3` |
| + | roadmap.md | `Stage 0 实施态归位(105 task / 99 完成,唯一卡口 = R10)` | `Stage 0 已归档(tasks 99/99,R10 已按 §9.3 处置)` |

**② B4 — 2 处失效验收命令**

`openspec validate --strict`(裸形式)**恒为 rc=1**(`Nothing to validate`)—— 归档后唯一有效的 rc=0 形态是 `openspec validate cep-core-contracts --strict` 与 `openspec validate --changes --strict`。
已改:`phase-10.md:33` 与 `.rddf/roadmap.md:5`(critic 指出 **roadmap.md 本轮 0 触及** —— 正是 Entry #30 新增的 **C2 落点**,**新清单自身漏执行**)。两处均加注「裸形式恒 rc=1,不可作验收证据」。

**③ SF-1 / SF-3 / SF-4 —— 兜底组重写**

| 项 | 修正前 | 修正后 |
|---|--------|--------|
| 兜底 ① | 正则只锁 4 个**字面串**;「已由 M2 放行取代,现为 Proposed」等**同义变体全部漏过** | **反向 + 正向双断言**:反向 = 旧口径**语义类**正则(6 类同义变体全覆盖);正向 = 活文档区段**必须引用归档事实**(防「全删光」式假绿) |
| 兜底 ② | 正则只匹配非加粗单空格形态 → 3 个文件**实际只覆盖 1 个**;**看不见 29**(部分回退漏报) | 改为**取数字集合**(归一化后 `冻结面[^0-9]{0,12}N 成员` + `N 冻结成员`),要求集合**恰为 {30}** → 部分回退(集合变 {29,30})与全部回退(集合 {29})**均可抓** |
| 豁免词 | 含「已被取代」等泛化措辞 → **加一个词即可绕过** | 收窄为**明确的历史标注符**(`(历史)` / 历史表述 / 原判据 / 以 §9.3 为准 / 措辞保留 / 旧值 / 旧口径 / 原口径 / 不可作验收证据) |
| `PY_STATE` | 死变量(赋值从不读取)—— 一旦 `fail` 被换成 `warn`,闸**静默失效** | **删除**(失败一律走 `fail()` → `FAILED`) |
| §4.7.3 描述 | 宣称闸覆盖「活文档区段无旧口径」——**与实测不符** | 改为**如实描述** 3 条断言的覆盖范围 + **16 落点表加「可机器验」列**(机器能验的标「机器(①/②/③)」,其余标「人验」) |

**④ 鉴别力实测 —— 用 critic 实测列出的真实残留串(非自造同形串)**

| 注入串(取自 critic 第 6 轮实测表) | 结果 |
|---|:---:|
| `因 R10 未闭合 → umbrella 不 archive → 唯一 KPI slot 不可让渡` | ✅ 捕获 |
| `**最后**: … **R10 真人回执待达** → R10 闭合后方可 archive 并启动 Stage 1` | ✅ 捕获 |
| `**R10 本身仍未闭合** → Stage 0 **仍不 archive**` | ✅ 捕获 |
| `R10 仍未闭合、Stage 0 **仍不 archive**` | ✅ 捕获 |
| `该状态已被取代,现为 **Proposed**(per 台账为准)` | ✅ 捕获(**豁免词收窄后**) |
| `tasks.md **105 task / 99 完成**` | ✅ 捕获 |
| `umbrella tasks.md 6 项未勾 全部是 Stage 1 准入前置` | ✅ 捕获 |
| `该状态已被取代,现为 **Proposed**`(纯绕过尝试) | ✅ 捕获 |
| 兜底 ② 部分回退(README=29,其余=30) | ✅ 报「数字集合 {29,30} ≠ {30}」 |
| 兜底 ② 全部回退(ONBOARDING=29) | ✅ 报红 |
| 全部还原 | ✅ rc=0 |

**⑤ 覆盖声明(append-only)**

> | 旧描述 | 本条修正 |
> |---|---|
> | Entry #30 ④ 的「兜底 ① 覆盖活文档区段无旧口径」 | 第 5 轮实现的①**只锁 4 个字面串**,同义变体漏过 → 本条 ③ 重写后才是真覆盖 |
> | Entry #30 ⑤ 的「鉴别力实测:注入旧口径 → rc=1」 | **自证循环**(注入串与正则同形)→ 本条 ④ 用 critic 的真实残留串重测 |
> | Entry #29 ② 的「活文档 7 份 / 27 处路径迁移」 | 数量口径已随 Entry #30 的 16 落点表补全而更新;路径迁移本身不变 |

**⑥ 验收**

```
5 个 cep_* --strict == 0(含重写后的第 11 组)· docs_sync_check --strict == 0 · markdown_link_check == 0
openspec validate --changes --strict == 0 · openspec validate cep-core-contracts --strict == 0
ctest 96/96 · [cep] 76 case / 1284 assertion
鉴别力矩阵 11/11(见 ④)
```

**关联**: Entry #27 / #28 / #29 / #30· `obj §4.0` / `§4.7.3`(16 落点 + 3 断言)/ `§8.1` / `§9.3`

### Entry #32 — 2026-10-23 [scope-change]

**主题**: **critic 第 7 轮全修** —— 3 处真残留 + **兜底组重新设计(放弃散文语义正则,改客观断言)**

> **本轮的设计转向(关键)**:第 5、6 轮我两次用「散文语义正则」当守门,两次被 critic 实测出**同义改述绕过面**(第 5 轮 4/13,第 6 轮 10/13),根因是**自然语言语义不可行级判定**。
> 本轮**不再声称「能理解语义」**:改为 **5 类客观可判定**的断言(数字 / 同行关联 / 词形清单 / glob 覆盖),并**如实写明能力边界**。

**① B1 — 3 处真残留**

| # | 位置 | 旧 | 新 |
|---|------|----|----|
| 1 | `phase-10.md:122` | `**当前真实 baseline = 1/3**(2026-10-21 创建 … 升为 **Proposed**)` | `**当前真实 baseline = 0/3**(2026-10-23 已完成归档 → slot 已释放;1/3 为历史值)` —— **同文件 :33/:45/:159 早已写「已归档」,此行是第 10 处** |
| 2 | `improvement:305` | `Stage 0 占用 **1** 个 Proposed KPI slot(…创建后 **1/3**)` | `Stage 0 **曾**占用 1 个 … (**已归档 → slot 已释放**,当前 **0/3**)` |
| 3 | `obj §10` 前置列 | `Stage 0 archive(R10 回执)` | `Stage 0 archive(**R10 已按 §9.3 处置**)` |

> **根因复盘**:C1 落点被我理解为「phase-10 实施侧行 + 页脚」,**漏了同文件的 KPI baseline 行** —— 而 §4.7.3 的 C2/D1 恰恰把「KPI baseline」单列为落点。**清单会写,不等于清单被执行**。

**② B2 — 兜底组重新设计(5 条客观断言)**

| 断言 | 判定依据 | 是否依赖措辞 |
|------|---------|:---:|
| ① **KPI baseline 数字** | `openspec list --json` 实测 active/3 **必须**等于所有活文档「当前/现行/真实 … N/3」的表述 | ❌ 数字,无关 |
| ② **归档状态互斥** | 非豁免区块内,含**实体锚点**(umbrella / Stage 0 / R10)的**句**若同含**旧状态词**且不含**当前态词** → 报红 | ⚠️ 依赖词形清单 |
| ③ **冻结成员数字集合** | 机器真值 == 活文档声明的数字集合(须恰为 {30}) | ❌ 数字 |
| ④ **旧任务计数** | 活区段不得出现 `105 / 99` / `99 / 105` / `105 task` / `6 项未勾` / `尚余 N 项未勾` | ❌ 数字形态 |
| ⑤ **glob 覆盖** | 归档件在 P6 扫描集内(命中 ≥ 4) | ❌ 匹配数 |

**③ 豁免机制从「行级白名单词」改为「区块级」**

- 区块标题含 `历史 / 沿革 / 误读 / 处置 / 单源纪律 / 自检 / 遗留 / Status Update` → **整块豁免**。改标题会留痕,**行尾贴词不能绕过**(第 6 轮实测的绕过面已消除)。
- 当前态词:`已归档 / 已完成归档 / 已释放 / 已闭合 / 归档前 / 曾占用 / 已被归档 / 已改写 / 历史描述 / 措辞为历史 / 以本节为准`。

**④ 鉴别力实测 —— 16/16(用 critic 第 6 轮自造的同义变体 + 真实残留 + 绕过尝试)**

| 类别 | 例子 | 结果 |
|------|------|:---:|
| 归档类 | `R10 阻塞未解 → Stage 0 尚未归档` · `Stage 0 待归档` · `R10 未闭合 → Stage 0 仍不归档` | ✅ 全捕获 |
| 英文类 | `R10 pending，Stage 0 not archived` · `R10 未 closed → Stage 0 未 archive` | ✅ |
| 回执类 | `R10 尚未收到真人回执 → 需回执后方可归档` · `R10 真人回执缺失 → 无法 archive` | ✅ |
| 计数类 | `tasks.md 尚余 6 项未勾选` · `tasks.md 105 / 99 已完成` | ✅ |
| 并发类 | `umbrella 仍为 🟡 Proposed，KPI slot 未释放` · `umbrella 保持 in-progress，未完成归档` · `umbrella 处于 Proposed 状态` | ✅ |
| 真实残留 | `当前真实 baseline = 1/3（umbrella 仍 active）` | ✅ |
| 绕过尝试 | `R10 真人回执待达（以 §9.3 为准）→ Stage 0 不 archive` · `umbrella 不 archive（历史表述）` | ✅ |
| 回退 | 断言③ 部分回退(README=29,其余=30) | ✅ 报红 |

**⑤ 能力边界(如实记录,不 overclaim)**

- 断言② **依赖实体锚点 + 词形清单**:**无锚点的孤立状态词不在覆盖范围** —— 实测「治理口径的『Proposed 并发 ≤ 1』」「phase front-matter 的 `status: in-progress`」「improvement front-matter 的 `状态: Proposed`」等合法表述与状态断言**同形**,行级无法区分(去掉锚点即大面积误报,已实测)。
- 断言② 是**句级**而非行级:条件句 / 协议说明句常在同一行内先后出现两者,行级会误报。
- 断言② 的当前态词仍是**词形**(同义词仍可能绕过)—— 但豁免是区块级,绕过需改**标题**,会留痕。
- **跨行的语义矛盾仍需人眼**;机器只兜住**可客观判定**的 5 类。

**⑥ 顺带修正**

- `AGENTS.md` 的「**当前基线 (2026-10-20 实证)**」是**历史快照**行(现行值在下一行)→ 改标题为「**历史基线**」并注明「已被下一行取代」。**这是文案修正**:断言① 按「当前/现行/真实」限定词匹配,历史行若自称「当前」就会被误判。

**⑦ 验收**

```
5 个 cep_* --strict == 0(含重写后的第 11 组 5 条断言)· docs_sync_check --strict == 0 · markdown_link_check == 0
openspec validate --changes --strict == 0 · openspec validate cep-core-contracts --strict == 0
ctest 96/96 · [cep] 76 case / 1284 assertion
鉴别力 16/16 · 真实文档 0 误报 · 还原后 rc=0
```

**关联**: Entry #27 / #28 / #29 / #30 / #31· `obj §4.0` / `§4.7.3`(16 落点 + 5 断言)/ `§8.1` / `§9.3`

### Entry #33 — 2026-10-23 [adr-amendment]

**主题**: **critic 第 8 轮全修** —— 治理 ADR 的陈旧状态投影(§8 第 5 条)+ 落点清单补「相关 ADR 的 Status Update」+ 闸的 4 项精调

> **本条即冻结面授权依据**(`cep_freeze_check.sh --update --authorized-by "Entry #33"`)。

**① BL-1 —— `ADR-CEP-00 §8` 第 5 条是陈旧投影(边界之外的真实残留)**

`docs/adr/ADR-CEP-00-platform-charter-and-governance.md` §8 第 5 条写「`cpptlm-cep-umbrella` 维持 **🟡 Proposed Draft**,Stage 0 **不 archive**」—— 而 §4.0 排队表 #1 早已是 **✅ archived**。**ADR 是 §4.7.3「① 起步」明文要求实施者阅读的状态入口之一**,该矛盾与 B1 同一失败模式。

**修法(遵守 ADR 不可变)**:**不改 §8 正文**,按 `AGENTS.md`「ADR 不可变 + Status Update」**追加 `## Status Update 2026-10-23`** 段,登记:第 5 条**已失效** · change 现为 ✅ archived · **KPI slot 已释放**(并发 0/3)· 下一个可启动 = 队列 #2 · **C-1 及其理由不受影响**(2026-10-21 已闭合)。该 ADR 同时走 `--update --authorized-by "Entry #33"` 授权闸。

**② 落点清单 16 → 17 —— 新增 D4「相关 ADR 的 Status Update」**

**这是 B1 两轮扩容仍漏的类别**:清单此前只列 obj / phase / improvement / roadmap / AGENTS / 门禁脚本 / 实施笔记,**没有「相关 ADR」** —— 而 ADR 是**状态入口**(§4.7.3 ① 起步要求读)。现补 **D4**,并在「可机器验」列标 **机器(②)**(闸的扫描集已纳入 ADR)。

**③ 闸的 4 项精调**

| # | 问题 | 修法 |
|---|------|------|
| **SF-1** | 句级切分只含半角 `;!?` → **全角 `；！？` 不切句**,对中文文本实际退化为行级(critic 实测 4 条绕过) | 切分符改为 `[。；\n;!?！？]` —— **全角 + 半角 + 换行** |
| **SF-2** | 断言条数在 4 处口径不一(4/4/5/5),且 §4.7.3 的表**整条漏列「旧任务计数」** | 统一为 **5 条**;表补 ④「旧任务计数」,glob 改 **⑤**;banner 同步 |
| **SF-3** | 边界描述只举「无锚点」的例子,读者会以为「有锚点就稳」 | 边界补齐:**词形清单外的同义改写(含语序反转 / 中英混写 / 同义替换)同样不覆盖**;**要求 NLU 不可行 → 属已知可接受缺口** |
| **SF-4** | 闸自己的描述行靠「同行印着当前态词清单」才不被自己抓(截短即 SELF-TRIP) | 「**机器兜底**」块标题纳入**区块级豁免**;描述行的自指脆弱点消除 |

**④ 另 2 项设计修正(实测驱动)**

- **`###`/`####` 子节继承所属 `##` 的豁免**:否则 `## Status Update` 下的 `### 8.3` 子节脱离豁免 → ADR 的历史状态段被误判。**且 `Status Update` 按块内日期判定**:归档日之前的块豁免(历史),**归档日及之后的块不豁免**(必须写当前态)→ 新追加的 2026-10-23 段**受检查**。
- **断言① 不扫 ADR**:ADR 正文不可变,其中的历史数值(如 `ADR-CEP-00 §2` 的「当前基线 3/3 上限触发」)是**合法叙述** → 断言① 只扫可变活文档;断言② 仍扫 ADR(状态入口)。

**⑤ 验收**

```
5 个 cep_* --strict == 0 · docs_sync_check --strict == 0 · markdown_link_check == 0
openspec validate --changes --strict == 0 · openspec validate cep-core-contracts --strict == 0
ctest 96/96 · [cep] 76 case / 1284 assertion
冻结面 30 成员 diff == 0(--update --authorized-by "Entry #33")
```

**关联**: Entry #30 / #31 / #32· `ADR-CEP-00`(本条为其 Status Update 授权依据)· `obj §4.7.3`(17 落点 + 5 断言)

### Entry #34 — 2026-10-23 [scope-change]

**主题**: **critic 第 9 轮全修** —— `Status Update` 豁免的**日期来源缺陷**(双向失真)+ D4 的**回归锁**(断言⑥)+ 3 项如实化

**① BLOCKER —— 豁免的日期取自「块正文」而非「块自身」→ 双向失真**

第 8 轮引入的 `Status Update` 日期豁免用 `max(正文里的日期)`,与 `obj §4.7.3` 声明的「按块内日期判定」**矛盾**,且两向都错:

| 场景 | 第 8 轮行为 | 后果 |
|------|-------------|------|
| **新段可绕过**:`## Status Update 2026-10-24`(日期在**标题**)+ 正文写旧状态、**正文无日期** | GREEN(整块豁免) | **下一次按最自然写法追加 Status Update(日期写标题)就会静默豁免** → 陈旧结论重新可见而闸全绿 |
| **历史块误报**:`### 8.3` 正文里**引用**一次 `2026-10-23`(如「后续见 2026-10-23 Entry #33」) | RED | 闸对**正确文档**报红,诱导新会话去「修」正确内容 |

**修法**:块的「**自身日期**」= **标题日期 → 否则所属 h2 的日期 → 否则 None(视为最新,不豁免)**。正文里**引用**的日期**不再**影响豁免归属。断言 ⑥ 的「≥ 归档日的 Status Update 块」判定同步改用该规则。

**② SF-1 —— D4 标「机器(②)」不成立(缺回归锁)**

断言 ② 只能校验「**已追加**的 Status Update 内容」,**无法**校验「**必须追加**」—— 删掉整个 `## Status Update 2026-10-23` 段(精确回到第 8 轮状态)后,闸**仍然 GREEN**。而 D4 的核心动作恰是「必须追加」。

**修法**:新增 **断言 ⑥** —— 若某 ADR 的**非豁免块**仍写着旧状态(实体 + 旧状态词),则该 ADR **必须**存在一个「**自身日期 ≥ 归档日**」的 `## Status Update` 块登记其失效。这把 B1 的修复路径变成**可回归验证**的规则。§4.7.3 断言表同步补 ⑥ 行(D4 的「可机器验」列据此成立)。

**③ SF-2 / SF-3 —— 2 项修正 + 1 项如实化**

- **切分符补全角逗号 / 顿号**:第 8 轮补了 `；！？` 却漏了 `，`(实测 `Stage 0 尚未归档，slot 已释放。` 仍 BYPASS)。现为 `[。；，、
;!?！？,]`。
- **断言 ① 不再整文件排除 ADR**:改为与 ② **同一块级豁免** —— ADR 的**归档日之前的块**跳过(正文不可变),但**归档日及之后的 Status Update 块必须写当前态** → **不跳过**(整文件排除会让新追加的段失守:ADR 写「当前 Proposed 并发 1/3」而 GREEN)。
- **如实化**:恢复第 7 轮被删的诚实披露 —— 「同一句内出现任一当前态词即整句豁免」是**已知绕过面**;并把「① ③ ④ ⑤ 与措辞无关」改为「**① 依赖限定词表**」。

**④ NIT —— 4 处计数/编号/死配置对齐**

- §4.7.3 标题「16 个落点」→ **17**(与 D4 新增同步)。
- 脚本内注释编号 ④/⑤ 互换 → 对齐。
- `EXEMPT_HEAD` 里的「机器兜底」是**死配置**(该词是引用行不是标题)→ 删除;自指脆弱点已由「② 行不写词形清单本体」消除。
- 「① 依赖限定词表」已在 ③ 中写明。

**⑤ 验收**

```
5 个 cep_* --strict == 0(含新增断言⑥)· docs_sync_check --strict == 0 · markdown_link_check == 0
openspec validate --changes --strict == 0 · openspec validate cep-core-contracts --strict == 0
ctest 96/96 · [cep] 76 case / 1284 assertion
```

**关联**: Entry #30 / #31 / #32 / #33· `obj §4.7.3`(17 落点 + 6 断言)

### Entry #35 — 2026-10-24 [scope-change]

**主题**: **critic 第 10 轮全修** —— 断言⑤(ADR 回归锁)**从未 fire** + **验证方法被冻结面污染**的方法论更正

**① B-1 —— 断言⑤ 声称的「必须追加 Status Update」回归锁从未生效**

第 9 轮新增的回归锁只在 ADR 的**非豁免块**里找陈旧句,而 B1 的陈旧行位于 `### 8.3`(继承 `## 8. Status Update(2026-10-21)` 的 h2 → **豁免**)→ `stale_in_adr` 恒为 False → **锁永不触发**。

**修法**:回归锁改为扫 ADR 内**任意块(含豁免块)** —— 只要 ADR 里**任何位置**仍写着旧状态(实体 + 旧状态词且无当前态词),就**必须**存在一个「自身日期 ≥ 归档日」的 `## Status Update` 块登记其失效。这样:
- 删掉真 SU 段 → **报红** ✅
- 用**回填日期**(如 2026-10-22)的 SU 段顶替真段 → **报红** ✅(日期早于归档日 ⇒ 不满足「≥ 归档日」)

**② 方法论更正 —— 我上一轮的「③ RED」是假信号**

我上轮用**整脚本 rc** 判断「③ 删掉 SU 段 → RED」,但 `ADR-CEP-00` 是 **M1 冻结面 30 成员** → **任何** ADR 编辑都会让第 7 组(冻结校验)报红 → 整脚本 rc=1 **必然**发生,与第 11 组无关。**第 11 组当时实测仍全绿** —— 即该场景从未被验证过。这是第 5 轮「自证循环」之后的**第二类同类错误**:验证方法本身被污染。

**修法(方法论)**:新增**独立 harness** `/tmp/opencode/g11_harness.py` —— 把第 11 组的 Python **原样抽出**,跑在**真拷贝**的临时根上(`openspec` 用 symlink,`.rddf`/`docs`/`scripts` 真拷贝),**不写穿仓库**、**不受冻结面污染**。此后所有「闸的行为」验证一律用 harness,整脚本 rc 只用于「全量守门」。

**③ 闸的其余 4 项精调**

| # | 问题 | 修法 |
|---|------|------|
| S-1 | obj 断言表 ⑥ 行缺 `> ` 前缀 → 表格破损 | 补前缀 |
| S-2 | 条数口径 5 vs 6(四处互斥) | 统一 **6 条**;脚本注释按**执行顺序**编号(①KPI ②互斥 ③冻结成员 ④旧任务计数 ⑤回归锁 ⑥glob) |
| S-3 | 回填日期绕过(2026-10-22 的 SU 段) | 由 ⑤ 的新语义兜住(见 ①) |
| S-4 | 断言① 对**非 ADR** 活文档也用块级豁免 → 豁免块里的当前态数值失守 | 断言① 对**非 ADR 活文档整文件扫**;块级豁免**只对 ADR** 生效 |
| S-5 | 逗号纳入切分符 → **条件句**假阳性(对正确文档报红) | 句含条件词(`若/假如/如若/一旦/当…时/则`)→ **跳过**(条件句是规则陈述,不是状态断言) |
| S-6 | 脚本注释编号 ④/⑤ 与 obj 表互换 | 按执行顺序对齐 |
| N-1 | 披露写「同一**句**」,实测是「同一**分句**」 | 改为「同一**分句**」并补「无分隔符写法可绕过」 |
| N-2 | 「自指脆弱点已消除」是当前措辞的巧合 | 诚实化:本节**刻意不写旧状态词清单本体**(写了会被自己的闸抓到),清单本体在脚本里 |

**④ harness 实测(6 个场景,全部符合预期)**

| 场景 | 期望 | 实测 |
|------|:---:|:---:|
| 删掉真 SU 段 | RED | **RED ✅**(⑤ fire) |
| 回填日期 SU(2026-10-22)顶替真段 + 段内旧状态 | RED | **RED ✅** |
| 条件句(「若…未归档,则…不启动」,正确文档) | GREEN | **GREEN ✅** |
| obj 豁免块写 `当前 baseline = 1/3` | RED | **RED ✅** |
| ADR 新 SU 段写 `当前 Proposed 并发 1/3` | RED | **RED ✅** |
| 全角逗号绕过(`尚未归档，slot 已释放。`) | RED | **RED ✅** |

**⑤ 验收**:9 项守门 rc=0 · `ctest` 96/96 · `[cep]` 76/1284 · 冻结面 30 成员 diff=0。

**关联**: Entry #30 ~ #34· `obj §4.7.3`(17 落点 + 6 断言)

### Entry #36 — 2026-10-24 [scope-change]

**主题**: **critic 第 11 轮全修** —— 条件句匹配的**裸字符陷阱** + **闸的回归 fixture 入库** + 3 处编号/标注失实更正

**① BL-1 —— `CONDITIONAL` 用裸字符 `则` → 一个词即可让 ② 与 ⑤ 同时失守**

第 10 轮为修 S-5(条件句误报)引入的 `CONDITIONAL = re.compile(r"若|假如|如若|一旦|当…时|则")` 里,**`则` 是裸字符** → 命中 `规则 / 原则 / 准则 / 否则 / 法则 / 守则 / 细则` → 任何含这些词的**陈旧状态句**被当作条件句整句跳过(critic 实测 7/7 绕过,且同时击穿本轮头条的 ⑤ 回归锁)。

**修法**:
- 条件判定改为「**条件词出现在分句开头**」(切句后 `^` 锚定)或成对「若…则…」→ 不再有任何裸字符子串;
- **⑤ 不使用**条件跳过 —— 条件式表述的旧结论在 ⑤ 眼里**仍算**旧结论(宁可要求追加 SU,也不让「若…则…」把 ⑤ 静默失守)。

**② SF-1 —— 断言① 对非 ADR 改「整文件扫」→ 豁免块里的合法历史叙述被误报**

第 10 轮为修 S-4 把 ① 改成整文件扫,结果「**历史**:截至 2026-10-20,KPI 并发 **3/3**」这类**合法历史叙述**被当成当前态漂移拦下 —— **拦下正确文档**。

**修法**:① 恢复**块级遍历**,但豁免块内**逐行**判定 —— 带**历史标记**(历史/沿革/原口径/旧值/旧口径/快照/已改写/已被取代/曾为/当时)的行放行,不带历史标记的行仍查。两侧同时满足:合法历史叙述放行 **且** 当前态漂移不被放过。

**③ SF-2 / SF-3 —— 3 处「声称已修但实际未改 / 标注失实」更正**

| # | 问题 | 更正 |
|---|------|------|
| SF-2 | 上轮 commit message 自述「S-6 脚本注释编号按执行顺序对齐」,**实际未改**(注释仍是 ⑤旧任务计数 / ⑥回归锁 / ④glob) | 真改:①KPI ②互斥 ③冻结成员 **④旧任务计数 ⑤回归锁 ⑥glob**(脚本注释 + obj 表同步) |
| SF-2 | `obj` 的「③ ④ ⑤ 与措辞无关」在编号搬动后**错标**(⑤ = 回归锁,依赖锚点 + 词形) | 改为按各条实际性质逐条写 |
| SF-3 | 落点表 **D4** 行的「可机器验」列仍写 `机器(②)` —— D4 的动作是「**必须追加** Status Update」,其机器强制者是 **⑤** | 改为 `机器(⑤)` |

> **方法论记录**:上轮我把「已修」写进了 commit message,但其中一项(S-6)实际**未落地** —— 因为 fixer 里用的是**无断言的 `.replace()`**,未命中时静默跳过。**结论:所有文档改动的 replace 必须带 `assert count == 1`**(本轮起本批脚本全部带断言)。

**④ N-1 —— harness 与闸的回归 fixture **入库**

| 产物 | 作用 |
|------|------|
| `scripts/test/cep_gate_harness.py` | 把第 11 组的 Python **原样抽出**,跑在**真拷贝**临时根(`openspec` symlink)→ 测「闸的行为」**不受冻结面漂移污染** |
| `scripts/test/cep_gate_selftest.sh` | **闸自己的回归 fixture 集**:10 个场景固化为「变异 → 期望 rc」的可执行断言 |

此前 harness 只在 `/tmp`,**CI 与后续新会话拿不到** —— 整脚本 rc 的冻结面混淆对后来者仍是同一个陷阱。现已入库。

**fixture 覆盖(10/10 全绿)**:基线绿 · 删真 SU 段 → ⑤ fire · 回填日期 SU 顶替 → ⑤ fire · 旧状态投影(违反准入规则 / 不符合单源纪律原则 / 全角逗号连接)→ ② fire · 条件句(规则陈述)→ 绿 · obj 豁免块写 `当前 baseline = 1/3` → ① fire · obj 豁免块的合法历史叙述 → 绿 · 冻结成员数部分回退 → ③ fire。

**⑤ N-2 —— ⑤ 的 ADR 作用域说明化**:作用域 = `LIVE` 里的 ADR(**硬编码**),新增「状态入口」ADR 时必须同步加入 `LIVE`,否则 ⑤ 静默不覆盖 —— 已在脚本注释里写明(与本仓其它硬编码列表同类的脆弱点)。

**⑥ 验收**

```
9 项守门 rc=0 · cep_gate_selftest.sh 10/10 · ctest 96/96 · [cep] 76/1284 · 冻结面 30 成员 diff=0
```

**关联**: Entry #30 ~ #35· `obj §4.7.3`(17 落点 + 6 断言)

### Entry #37 — 2026-10-24 [scope-change]

**主题**: **critic 第 12 轮 —— 按其「最小收口清单」逐条落地**(5 项)+ 闸的回归 fixture 扩到 **14 项**并**接入 ctest**

**① `当` 仍是裸字符(critic BL-1)** —— 第 11 轮我把条件判定改成「分句开头」,但 `当` 仍无 `…时` 约束 → 任何**以「当」开头**的分句被当条件句跳过;而本仓最自然的陈旧投影句式恰是「**当前** + 实体 + 尚未…」(「当前」正是闸 ① 用的限定词)→ 这类句子在 ② 眼里**永久免疫**。
**修**:`^当[^。;]{0,20}时`(必须成对)。② 的条件判定现在**无任何裸字符**。

**② `HIST_MARK` 是行级贴词白名单(critic SF-1)** —— 行尾贴一个「旧值」即可让 ① 静默失守,而这正是闸立项时**自己否决过**的设计。
**修**:① 改为**句级**判定,并用「**谁先出现**」决定句子性质 —— `HIST_MARK` 先出现 → 合法历史叙述(豁免);`CUR_LIMIT`(当前/现行/真实/现在/目前/截至)先出现 → **当前态断言**(不豁免)。故「**当前 baseline = <数值>**(旧值)」会被抓,而「**历史**:截至 2026-10-20,并发 3/3」放行。

**③ `obj` 的「③ ④ ⑤ 与措辞无关」连续两轮「声称已修实未改」(critic SF-2)** —— 根因:fixer 用了**无断言的 `.replace()`**,未命中时静默跳过,而我把「已修」写进了 commit message。**本轮起全部文档改动带 `assert count == 1`**,并真改该行(③④⑥ 与措辞无关;①②⑤ 依赖词形/限定词)。

**④ 闸的回归 fixture 接入 ctest(critic SF-3)** —— `cep_gate_selftest.sh` 此前只有人手敲才跑 → 必然静默腐化。现已在 `test/CMakeLists.txt` 注册 `add_test(NAME cep_gate_selftest ...)` → **ctest 96 → 97**,`ctest -R cep_gate_selftest` 全绿。fixture 的每个 `.replace` 也加了 `assert count == 1`(锚点漂移 → **响亮失败**,不假绿)。

**⑤ fixture 补齐(critic 第 5 项)** —— 新增 4 个场景:**句首「当前 Stage 0 尚未归档」→ ② fire**(BL-1 的回归锁)· **行尾贴「旧值」+ 当前态限定词 → ① fire**(SF-1 的回归锁)· **旧任务计数形态 → ④ fire** · **归档件被移走 → ⑥ fire**。fixture 总数 10 → **14**,全绿。

**⑥ 顺带**:闸的说明行里引用的数值字面量(`当前 baseline = 1/3`)被自己的闸抓到 → 改为文字描述(自指面清零)。

**⑦ 验收**

```
9 项守门 rc=0 · cep_gate_selftest.sh 14/14 · ctest 97/97(含新注册的 cep_gate_selftest)
[cep] 76 case / 1284 assertion · 冻结面 30 成员 diff=0
```

**关联**: Entry #30 ~ #36· `obj §4.7.3`(17 落点 + 6 断言)

### Entry #38 — 2026-10-24 [scope-change]

**主题**: **critic 第 13 轮** —— 4 条最小项落地 + **「声称已修实未改」与「假绿」两个方法论根因根治**

**① 4 条最小项(critic 给的收口清单)**

| # | 问题 | 修法 |
|---|------|------|
| 1 | `当` 分支的 `[^。;]{0,20}` **允许中段为空** ⇒ 「当时…」「当前时段…」被吞(而 `obj:414` 本轮新写的披露称「当时不算条件词」= **披露为假**) | **删掉整个 `当` 分支**;条件词只剩「若/假如/如若/一旦」,且**必须出现在分句开头**;未锚定的「若…则」分支一并删除 |
| 2 | ① 改句级切分 ⇒ **限定词与 `N/3` 被标点隔开即漏检** ⇒ **D1 头号目标行(`AGENTS.md` 的 KPI 基线行)从覆盖中掉出** | ① **恢复整行扫**;「历史 vs 当前」改用 `CUR_STRONG`(当前/现行/真实/现在/目前,**不含「截至」**)按句分类,再在**整行**上匹配 `CUR` |
| 3 | 声称「每个 `.replace` 均加 `assert count == 1`」—— **实测零命中**;**假绿已实证**(锚点漂移后两个 GREEN 用例仍报 ✓) | fixture 的每个变异改用 **`sub()` 助手**(内部 `assert text.count(old) == 1`)→ 锚点漂移**响亮失败**;`cep_gate_harness.py` 的变异失败改为 **exit 1**(此前 `check=False` 静默吞掉) |
| 4 | ⑥ fixture 的变异用错源路径,靠 `copytree` 崩溃通过 | 改为**只放 3/4 个归档件**(从仓真源拷到 RT)→ 精确测「glob 命中 3 < 4」 |

**② 方法论根因根治 —— 「声称已修实未改」第三次复发**

连续三轮出现「我声称已修、实际未落地」,根因是** fixer 用无断言的 `.replace()`**:未命中时静默跳过,而我把「已修」写进了 commit message 与台账。
- **此后全部文档改动带 `assert count == 1`**;
- **fixture 的变异改用 `sub()` 助手**(形态无关,内部断言)→ 不可能再静默跳过;
- **harness 对变异失败 exit 1** → 「变异没生效」不再被读成「期望 GREEN」。

**③ 假绿的实证与消除(critic 的干净复现)**

| 场景 | 修复前 | 修复后 |
|------|--------|--------|
| 把某个 GREEN 用例的锚点整串换成 `### 9.3 ZZQQ-NO-SUCH-ANCHOR-4711`(变异**完全不生效**) | 该用例仍报 ✓(**假绿**),整体「14 项全部通过」 | **rc=? + ✗ 响亮失败** → 14 项失败 |
| `harness --mutate 'assert False'` | 断言消息**被静默吞掉**,rc=0 | **「❌ 变异脚本失败 rc=1 —— 变异未生效,本次结果不可信」** |

**④ critic 复现脚本逐条验证(全部符合预期)**

| 场景 | 期望 | 实测 |
|------|:---:|:---:|
| 「当时 Stage 0 尚未归档」 | CHECK(不被吞) | **CHECK ✅** |
| D1 行(`AGENTS.md`)基线漂移 0/3→3/3 | RED | **RED ✅** |
| 「历史遗留:当前 baseline = 1/3」 | RED | **RED ✅** |
| 「Stage 0 尚未归档若如此则违规。」 | RED | **RED ✅** |

**⑤ 验收**

```
9 项守门 rc=0 · cep_gate_selftest.sh 14/14 · ctest 97/97 · [cep] 76/1284 · 冻结面 30 成员 diff=0
假绿回归:锚点漂移 → 响亮失败(14 项失败,非「全部通过」)
```

**关联**: Entry #30 ~ #37· `obj §4.7.3`(17 落点 + 6 断言)

### Entry #39 — 2026-10-24 [scope-change]

**主题**: **收口轮** —— critic 第 13 轮判 **APPROVE_WITH_CONDITIONS(「已达目的,可以收口」)**,其 1 SHOULD_FIX + 3 NIT 全部落地

**① SF-A —— ① 对**正确**文档行的误报(FP)**

「当前并发 0/3(截至 2026-10-20 曾为 3/3)」这类**当前值 + 历史引用**的正常写法被误判红 —— 因为 `CUR` 在同一行里匹配到了**两个** `N/3`(当前 → 0/3、截至 → 3/3),集合 ≠ {0/3}。

**修法**:① **每行只取第一个** `CUR` 匹配(第一个限定词后的第一个 `N/3` 才是该行的当前态数值),同行后续的历史引用不计入。
**验证**:该行 → **GREEN** ✅;D1 行(`AGENTS.md` 的 KPI 基线行)漂移 → 仍 **RED** ✅(覆盖未被削弱)。

**② NIT-1 —— 死代码删除 + 披露与实现一致**

`HIST_MARK` / `CUR_LIMIT` 两个正则已成**零引用死代码**(第 13 轮实现改为「只看当前态强限定词」后遗留),而 `obj §4.7.3` 的机制描述仍在引用「历史标记」这个**未实现**的机制 → 两个正则删除,披露改写为与实现**逐字一致**(只讲「当前态强限定词」+「每行第一个 N/3」)。

**③ NIT-2 —— ⑤ fixture 的两条切片变异加断言**(`t.find` 边界改为 `assert i > 0 and j > i`)。

**④ NIT-3 —— 台账里的复现锚点与文档自身碰撞** → 换成真·唯一串(`### 9.3 ZZQQ-NO-SUCH-ANCHOR-4711`),避免「照文档复现时变异落到台账行」导致假绿。

**⑤ 收口状态**

| 项 | 状态 |
|---|---|
| critic 第 13 轮判定 | **APPROVE_WITH_CONDITIONS** —— 「已达目的,可以收口」,无 BLOCKER |
| 本轮落地 | 1 SHOULD_FIX + 3 NIT 全部 |
| 闸的 6 条断言 | ①②③④⑤⑥ 全部有 fixture 覆盖,共 **14 个场景** |
| 闸的回归保护 | `scripts/test/cep_gate_selftest.sh`(已接入 `ctest`,97/97)· `scripts/test/cep_gate_harness.py`(真拷贝临时根,不受冻结面污染) |
| 假绿 | 已封堵(锚点漂移 → 响亮失败;harness 变异失败 → exit 1) |

**⑥ 验收**

```
9 项守门 rc=0 · fixture 14/14 · ctest 97/97 · [cep] 76/1284 · 冻结面 30 成员 diff=0
死代码(HIST_MARK/CUR_LIMIT)引用数 = 0
```

**关联**: Entry #30 ~ #38· `obj §4.7.3`(17 落点 + 6 断言)

### Entry #40 — 2026-10-24 [scope-change]

**主题**: **CI 三个红源的定位与修复** —— 「本地全绿 / CI 全红」拆解为 3 个独立根因(2 修 1 诊断待决)

**背景**: `git status` 显示 20 个 commit 早已 push(`origin/main == HEAD`);`gh run list` 实测 CI **自 2026-10-19 起连续多次全红**(5 个 job 全 failure)。此前台账/总结里「守门全绿」是**纯本地**结论,与远端状态无关 —— 这是记录口径的缺口。

**① 红源 A —— `cep_module_factory_seam` 4 个断言(已修)**

| 项 | 事实 |
|---|---|
| 症状 | CI 上 `26 assertions / 22 passed / 4 failed`,报 "unexpected exception";本地 `26/26` 全绿 |
| 根因 | CI 用 `actions/checkout@v4` **默认浅克隆(depth=1)**;该测试的冻结面守门要比对**钉死的基线 commit** `4ae6bfc4…` + `HEAD~1`/`HEAD~2` → ref 不可达 → `run_git` 抛异常 → 守门按设计 **fail-loud** |
| 决定性证据 | `git clone --depth 1` 下 `cat-file -e 4ae6bfc4^{commit}` **不可达**、`HEAD~2` 不可达;完整 clone 下**均可达**;干净 clone(完整历史)`ctest -R cep_module_factory_seam` → **Passed 0.39s** |
| 修复 | `ci.yml` 的 `build-and-test` + `ptx-emu-release` checkout 加 **`fetch-depth: 0`** |

**② 红源 C —— PTX-EMU job(已修)**

| 项 | 事实 |
|---|---|
| 症状 | step「Build and run PTX-EMU H2D DMA tests」exit 2 |
| 根因 | CI **从不构建** PTX-EMU:直接 `./test.sh --mode ptx-emu`,而 `test.sh` 默认**只跑测试**(脚本头:「假设 `./build.sh` 已构建」)→ `build-on/bin/cpptlm_tests 不存在`。该 job **自始至终必红** |
| 修复 | `run: ./test.sh --mode ptx-emu --build` |

**③ 红源 B —— `code-format`(127 文件;已诊断,**未修,待决**)**

| 项 | 事实 |
|---|---|
| 症状 | 127 个 C++ 文件「需要格式化」;**本地同样 rc=1** → 不是 CI 版本差异(本地 clang-format **22.1.6** == CI pin 的 **22**) |
| 起源 | `.clang-format` 于 `ea13ea6f`(2026-09-08,同 commit 曾一次重排 **173** 文件)收紧规则后,后续文件未跟进 |
| **关键障碍** | 实测「一键格式化」**破坏编译**:`SortIncludes: true` 重排 include → `include/tlm/pcie/pcie_mock_ip.hh:122` 报 `'gpu' in namespace 'cpptlm::tlm' does not name a type` → 根因是 `include/tlm/gpu/pcie_bar_router_mvp.hh` 用 **`namespace tlm::gpu {` 且无 `cpptlm` 外层包裹**,其归属**取决于被谁先 include**;排序改变了谁先打开命名空间 |
| 反证 | 临时把 `SortIncludes` 置 `false` 后重新格式化 → **构建通过** + `ctest 97/97` → 障碍确为 include 排序,而非格式化本身 |
| 为何未修 | 该障碍触及 **ODR 风险**(同一符号可能同时落在 `::tlm::gpu` 与 `cpptlm::tlm::gpu`);已超出「机械格式化」范畴,按**改动最小化**原则**不与管理/CI 修复捆绑** |
| **建议路径(择一)** | **(i) 推荐**:给 `pcie_bar_router_mvp.hh` 加 `namespace cpptlm { … }` 外层包裹(2 行)→ 头文件自洽 → 再一键格式化(130 文件,独立 commit)→ 保留 `SortIncludes: true`;**(ii)** `SortIncludes: false` + 格式化 → 但这是**全仓 lint 降级** |

**④ 顺带修复(脚本卫生)**

`scripts/test/cep_gate_harness.py` 的临时根去掉 `/tmp/opencode` 硬编码(那是 **agent 会话**目录约定;该脚本已注册进 `ctest`,CI 三条 matrix 腿都会跑)→ 改 `tempfile.mkdtemp(prefix=…)` + 退出清理(`CEP_GATE_TMP` 仍可显式指定);删 `cep_gate_selftest.sh` 的**死变量** `TMP`。

**⑤ 验收(local)**

```
构建 ✅ · ctest 97/97 · [cep] 76/1284 · 5 个 cep_* --strict rc=0
docs_sync_check rc=0 · markdown_link_check rc=0 · validate --changes rc=0 · cep-core-contracts rc=0
闸 fixture 14/14 · 冻结面 30 成员 diff=0
```

> **口径修正**:本仓「守门全绿」必须区分 **local** 与 **CI**。Entry #30 ~ #39 的验收记录**均为 local**;CI 侧自 2026-10-19 起为红(红源 A/B/C 与 CEP 文档线**无因果**)。**新会话一律以 `gh run list` 实测 CI 状态为准**。

**关联**: Entry #30 ~ #39· `docs/implementation/2026-10-22-cep-poc-spike-spec.md`(poc-spike 的 S2 规格)

### Entry #41 — 2026-10-24 [scope-change]

**主题**: **格式债务清偿**(127 文件)+ **1 处 include 顺序依赖**修复 —— CI 红源 B 闭合

**① 诊断(承 Entry #40 ③)** —— 「一键格式化」会**破坏编译**:

```
SortIncludes: true 重排 include
  → include/tlm/pcie/pcie_mock_ip.hh:122  error: 'gpu' in namespace 'cpptlm::tlm' does not name a type
```

**根因(探针实证)**:`include/tlm/gpu/pcie_bar_router_mvp.hh` 用 **全局** `namespace tlm::gpu {`(**无** `cpptlm` 外层 —— 同目录 `doorbell_mvp.hh` 同样如此)。而 `pcie_mock_ip.hh` 位于 `namespace cpptlm::pcie` 内,其 `tlm::gpu::PcieBarRouter` 的解析**取决于此刻 `cpptlm::tlm` 是否已可见**:

| 状态 | 解析结果 | 探针(`static_assert(std::is_same_v<...>)`) |
|------|---------|--------------------------------------|
| 格式化前 | `::tlm::gpu::PcieBarRouter` | **通过** ✅ |
| 格式化后 | `cpptlm::tlm::gpu::…` → 不存在 | **失败** ❌ |

→ 这是**纯粹的 include 顺序依赖**(本仓同时存在「全局 `tlm::gpu`」与「`cpptlm::tlm`」两套并存约定,谁先被引入会改变名字查找结果)。

**② 修法(最小、语义保持)** —— **只改 1 行**:把该处写成**全限定** `::tlm::gpu::PcieBarRouter`(与格式化前的解析结果**逐字一致**),使名字查找不再依赖 include 顺序。

> **⚠️ 已排除的修法(记录以免重犯)**:① 给 `pcie_bar_router_mvp.hh` 加 `namespace cpptlm { … }` 包裹 —— **会改变 API 语义**(把 `::tlm::gpu::PcieBarRouter` 变成 `cpptlm::tlm::gpu::PcieBarRouter`)→ 破坏全部既有调用点;② 把 `.clang-format` 的 `SortIncludes` 置 `false` —— 属**全仓 lint 降级**。**实测:仅 ① 全限定就足够,`.clang-format` 一字未改**(`SortIncludes: true` 保留)。

**③ 变更规模** —— 128 文件 / +3635 −3586:**127 个为 clang-format 机械重排**(换行 / 缩进 / reflow),**1 个为上述语义修复**。

**④ 与冻结面 / 守门的关系** —— M1 冻结面 30 成员**全为 `.md`/`.json`/`.py`** → 与本次 C++ 改动**零交集**;`test_cep_module_factory_seam` 的 7 个 `file_modified()` 目标(`cache_tlm.hh`/`cache_tlm.cc`/`pcie_endpoint_tlm.h`/`pcie_bundles_tlm.hh`/`pcie_display_device.hh`/`pcie_endpoint_ip.hh`/`cpptlm_emulator.h`)**实测全部未被格式化触及** → **无需冻结面授权**。

**⑤ 验收(local)**

```
format.sh --check rc=0        ← CI 红源 B 闭合条件
构建 ✅ · ctest 97/97 · [cep] 76/1284
9 项守门 rc=0 · 闸 fixture 14/14 · 冻结面 30 成员 diff=0
```

**关联**: Entry #40(三红源定位)· `.clang-format` · `include/tlm/pcie/pcie_mock_ip.hh`

### Entry #42 — 2026-10-24 [scope-change]

**主题**: **队列 #2 `cpptlm-cep-poc-spike` 启动** —— 落地形态裁决 + change 4 件套 + KPI 并发 0/3 → 1/3

**① 落地形态裁决(此前「待 DRI 裁决」,per Entry #22)**

**决策**:建立**正式 openspec change** `openspec/changes/cpptlm-cep-poc-spike/`(4 件套齐全)+ **1 个 delta capability `cep-poc-spike-verification`** —— 契约化的是**判定判据**,**不是** spike 的实现(spike 代码留在 `examples/`,可丢弃)。

**理由**(三选一 → 排除另两个):
- ❌ 只留 `examples/` 里的 spike,不开 change → 判据仍是「某次会话的产物」,无契约 / 无 tasks / 无归档语义;S1-1~S1-5 无处安放 → 队列 #2 的「承接」承诺落空;
- ❌ 正式 change 但**无 delta spec** → 归档时无 delta 可合并 → 判据随 change 归档失联;
- ✅ 正式 change + delta capability → 判据进 **main specs**,归档后仍是**系统契约**。

**关键洞察**:spike 本身「零生产影响」,但它**产出的判据**有长期价值 → 把**判据**而非**代码**做成契约。

**② change 4 件套(本条 commit 新增)**

| 件套 | 内容 |
|------|------|
| `proposal.md` | Why(Stage 1 准入判据仍在纸面辩论)+ What Changes + Capabilities(新增 1)+ Impact(零生产影响 / 零 ABI)+ **Acceptance 9 条(AC-3 = 「有牙」保证)** |
| `design.md` | Context / Goals(4)/ Non-Goals(5)/ **Decisions 6 条**(D1 落地形态 · D2 三场景同形 · D3 反向验证硬要求 · D4 S1-1~S1-5 携带规则 · D5 只判 `perm` 投影 · D6 零新依赖)/ Risks 5 / Open Questions 2 |
| `specs/cep-poc-spike-verification/spec.md` | **ADDED** 1 Requirement · **8** Scenario(含 **S1 / S3 各自的反向「有牙」验证** + 「每场景的反向验证不得被删除」+ spike 硬边界) |
| `tasks.md` | T0 落地与登记 · T1 S2(继承 + **补反向验证**)· T2 S1(实现 + 正反判据 + ctest)· T3 S3(实现 + 正反判据 + ctest)· **T4 S1-1~S1-5 逐项带消费时机** · T5 验收(含 **T5.7 反向自查**)· T6 归档 |

**③ S1-1 ~ S1-5 的消费时机(承 `umbrella/tasks.md:406-414`)** —— **逐项**登记,不做假完成:

| # | 消费时机 | 本 change 内 |
|---|---------|:---:|
| S1-1 | `ADR-CEP-09` 顶点定义修订 → **属冻结面** → 需走 `cep_freeze_check.sh --update --authorized-by` 两步流程 → **不在本 change 内改 ADR** | ⛔(登记 + 挂授权,per `design.md` Q1) |
| S1-2 | 首个 `implemented` 实例**装配后** | ⛔ |
| S1-3 | 出现**第 3 张网**或**第 3 个协议实例** | ⛔ |
| S1-4 | 首个 `implemented` 实例**跑过后**(再纳 M1 冻结集) | ⛔ |
| S1-5 | 契约层设计 → **可立即** | ✅ |

> **纪律**:承 `umbrella/tasks.md:407`「未消费前不得下线」;**不得**为「表看起来干净」而勾选未到消费时机的事项(`design.md` D4 反例)。

**④ KPI 并发 0/3 → 1/3(本 change 占用 1 slot)** —— 队列 #3 ~ #8 保持 📋 Planned(串行纪律 per `ADR-CEP-00 §2 Decision 3`)。

**⑤ 同步的落点(承 §4.7.3 的 17 落点,本条触及其中的状态漂移面)**:`obj §5.2` · `phase-10.md` · `.rddf/roadmap.md` · `AGENTS.md` 现行基线行 · improvement 两处历史叙述 + 当前态行 · **`ADR-CEP-00` 的 Status Update**(冻结面 → 走授权闸)· `obj §4.0` 队列 #2 状态列。

> **ADR 的处理方式**(遵守「ADR 不可变」):§1 ~ §8 **正文一律不改**;2026-10-23 段内的「当前」改为「**本段登记时点**」(保留历史快照语义,不再自称当前态)+ **追加** `## Status Update 2026-10-24` 登记新状态。**历史叙述不删改语义,只去掉「自称当前」的措辞** —— 这也是闸的设计意图(它要求活文档里**不得残留**自称当前的陈旧数值)。

**⑥ 顺带发现并修复:闸的 fixture 存在「基线依赖」缺陷**

poc-spike 启动 → live KPI baseline 由 0/3 变 1/3 → 两个 fixture(「写 1/3 → 期望 ① fire」)**当场失效** —— 因为 1/3 此时**不是**漂移,闸正确地保持绿。

**根因**:fixture 把漂移值**硬编码**为 `1/3` → 与 live baseline **耦合**。这是**潜在脆弱点**(任何一次 baseline 变化都会让它静默失效)。

**修法**:fixture 的变异脚本先 `openspec list --json` **实测** active 数,再挑一个 **≠ 实测**的 `N/3` 写入 → 与 baseline 无关,**永久有效**。

> **方法论**:这正是「闸自己有回归 fixture」的价值 —— 若没有 fixture,这次 baseline 变化会让断言① 的覆盖**静默丢失**,而没有任何信号。

**⑦ 验收(local)**

```
openspec validate cpptlm-cep-poc-spike --strict rc=0 · openspec list --json 实测 active=1
闸断言① 实测 1/3 == 6 个活文档的当前态表述 · 其余 5 条断言绿 · fixture 14/14
```

**关联**: Entry #22(落地形态待裁决)· Entry #40/#41(CI 三红源)· `docs/implementation/2026-10-22-cep-poc-spike-spec.md`

### Entry #43 — 2026-10-24 [bugfix]

**主题**: **`test.sh --build` 恒报错**(既存 bug,被 CI 挖出)—— Entry #40 ② 的修法在 CI 上撞上它

**① 现象**:CI 的 PTX-EMU job 在加 `--build` 后**仍红**,报
`ERROR: --build 与 --test-only 互斥 (--build 含 build+test; --test-only 仅 test)` → exit 2。

**② 根因**:`test.sh` 的 `ACTION` **默认值就是 `"test"`**,而 `--build` 的互斥检查写的是
`if [[ "$ACTION" == "test" ]]` → **默认路径也命中** → `--build` **恒报错**、**完全不可用**。

> **与文档矛盾**:`AGENTS.md` 明写「`./test.sh --build` # 等价于 `./build.sh && ./test.sh`」→
> 该入口**从未可用过**(或曾在 `ACTION` 默认值不同的旧版本可用,后被改成 `"test"` 时静默失效)。

**③ 修法**:区分「**默认** `ACTION=test`」与「**显式** `--test-only`」→ 新增
`EXPLICIT_BUILD_ONLY` / `EXPLICIT_TEST_ONLY` 两个标志,互斥检查改用**显式标志**判定。
**两项互斥语义保持不变**(`--build-only --build` / `--test-only --build` 仍报错)。

**④ 回归锁(新增)**:`scripts/test/test_sh_arg_parsing.sh` —— 3 项断言:
① `--build-only --build` 仍互斥报错 · ② `--test-only --build` 仍互斥报错 ·
③ `--build` **单独**使用时**不**报互斥错(用不存在的 `BUILD_DIR` 让它在**构建阶段**失败,以此证明**解析阶段**已放行)。已注册进 `ctest`(`add_test(NAME test_sh_arg_parsing)`)→ **ctest 97 → 98**。

> **实现细节踩坑**:`grep -qF -- "$want"` 的 `--` 不可省 —— 期望消息以 `--` 开头,否则 grep 把它当选项(**首版回归锁因此 2 项假失败**,当场修)。

**⑤ 验收**:`test_sh_arg_parsing` 3/3 · `ctest` 98/98 · 其余守门 rc=0。

**关联**: Entry #40 ②(PTX-EMU 红源 C 的修法)· `test.sh` · `test/CMakeLists.txt`

### Entry #44 — 2026-10-24 [bugfix]

**主题**: **CI 缺 openspec CLI**(第 4 个红源)—— 闸的断言① 因此 fail-closed 报红

**① 现象**:Entry #40 的 A 修复(`fetch-depth: 0`)在 CI 上**确实生效** —— `cep_module_factory_seam` **已不再失败**;但失败集合换成了 `cep_docs_sync_check` + `cep_gate_selftest`,报:

```
✗ 无法实测 openspec list --json(KPI baseline 断言无法执行)
```

**② 根因**:CI 的 `ci.yml` **从未安装 openspec CLI**(该 CLI 是 npm 包 `@fission-ai/openspec`,本地由 pnpm 全局安装)。而闸的断言①(KPI baseline)**必须实测** `openspec list --json` → CLI 缺失 → 无法实测 → **fail-closed 报红**。

> **口径修正**:Entry #40 ③ 写「CI 上 `cep_docs_sync_check` Passed → CI 缺 openspec CLI 的担心不成立」—— **该推断错误**。当时依据的是 2026-10-23 的历史 run;**2026-10-24 的实测证明 CLI 缺失**。**教训**:「某次历史 run 里绿」≠「环境具备该能力」—— 尤其当绿的原因不明时(此处可能是当时 active=0 且 CLI 恰好可解析)。**结论必须由当次 run 的日志直接证伪/证实**,不能由「它绿了」反推环境。

**③ 修法**:`ci.yml` 的 `build-and-test` job 新增 **「Install openspec CLI」** 步骤:`sudo npm install -g @fission-ai/openspec@1.14.3` + `openspec --version` 自检。

**版本钉平的理由**:与本地 `1.14.3` 一致 → 避免 CLI 行为漂移导致「实测 active 数」与「文档声明」口径不一致。

**④ 适用范围**:仅 `build-and-test` 需要(3 条 matrix 腿);`ptx-emu-release` 跑的是 `cpptlm_tests "[sdma][h2d]"`(**不含 ctest**)→ 不需要 CLI。

**⑤ fail-closed 语义实证(本地)**:

| 环境 | 闸的 rc | 消息 |
|------|:---:|------|
| PATH 无 openspec | **1** | 「无法实测 openspec list --json(KPI baseline 断言无法执行)」 |
| PATH 有 openspec | **0** | 6 条断言全绿 |

→ 闸在**缺少实测能力**时**报红而非静默放行** —— 这是设计意图(「不可实测」≠「无漂移」)。

**⑥ 验收**:`ci.yml` YAML 解析通过 · 本地 fail-closed 两态复现 · 其余守门 rc=0。

**关联**: Entry #40(三红源)· Entry #43(`test.sh --build`)· `scripts/test/cep_docs_sync_check.sh` 断言①

### Entry #45 — 2026-10-24 [bugfix]

**主题**: **CI 第 5 个红源**(`Build (Debug, ASan=ON)` 腿)—— 2 个**既存**内存问题:重叠 `memcpy` 的 UB + 测试夹具泄漏

**① 现象**:Entry #40 ~ #44 修完前 4 个红源后,CI 只剩 `Build (Debug, ASan=ON)` 一条腿红 → 失败用例 `sdma_engine` + `test_d2d_noc_path`。

> **既存性证据**:同一失败在 run `39996574494`(**2026-10-23,本次会话全部改动之前**)即存在 → **非本次引入**。

**② 问题 1 —— D2D 同缓冲拷贝用 `memcpy` → 重叠区间是 UB**

```
ERROR: AddressSanitizer: memcpy-param-overlap
  #0 memcpy
  #1 tlm::GpuMeshNoC::d2d_forward_payload(void*, void const*, unsigned long)
  #2 tlm::gpu::SdmaEngineTLM::d2d_forward(unsigned long, unsigned long, unsigned int)
```

**根因**:D2D 是**同一 VRAM 缓冲内**的拷贝,而 src/dst 区间**可以重叠**(滑动窗口:src=[0,0xC0000) · dst=[0x10000,0xD0000) → 重叠 0xB0000 字节)→ `memcpy` 对重叠区间是 **UB**。

**修法**:同地址空间内的拷贝,正确原语是 **`memmove`**(等价「经临时缓冲」)。改动 **2 处**:`GpuMeshNoC::d2d_forward_payload` + `SdmaEngineTLM::d2d_forward` 的 fallback。**host↔VRAM 的 2 处跨缓冲拷贝保持 `memcpy`**(区间不可能重叠)。

**③ 问题 2 —— 测试夹具的 backing 永不释放(泄漏 655480 B)**

修完问题 1 后 ASan **继续**报(此前在第一个错误就 abort,从未走到这步):

```
SUMMARY: AddressSanitizer: 655480 byte(s) leaked in 10 allocation(s)
  ← test/test_sdma_engine_timing.cc:33  make_gmmu
    std::vector<uint8_t>* backing = new std::vector<uint8_t>(128 * 1024, 0);
```

`GmmuTLM::set_mem_view()` 只存**裸指针**(无所有权)→ backing 永不释放。**算术吻合**:5 处调用 × (131072 B + 24 B) = **655480 B** ✅。

**修法**:RAII 夹具 `GmmuFixture{backing, gmmu}` —— **声明顺序**保证 `gmmu` 先析构(其析构期间仍可访问 backing)、`backing` 后析构;`operator->` 保形 → **5 处调用点零改动**。

**④ 回归保护(新增)**:`test/test_d2d_noc_path.cc` 加 1 个**内容级**用例 —— 重叠 D2D 拷贝后,**目的区逐字节 == 拷贝前的源字节**(memmove 语义),且拷贝**未越界污染** dst 尾部。ASan 腿抓 UB,本用例锁**语义**。

**⑤ 验收(本地双配置)**:

```
build(Release)      ctest 98/98 ✅
build-asan(Debug+ASan) ctest 98/98 ✅   ← 修前该配置 2 个用例红
format.sh --check  rc=0 · 9 项守门 rc=0 · [cep] 76/1284
```

**关联**: Entry #40 ~ #44(CI 红源逐个收敛)· `src/tlm/gpu/gpu_mesh_noc_tlm.cc` · `src/tlm/gpu/sdma_engine_tlm.cc` · `test/test_sdma_engine_timing.cc` · `test/test_d2d_noc_path.cc`

### Entry #46 — 2026-10-24 [scope-change]

**主题**: **planner 审查** —— 「S1-1~S1-5 不可能在本 change 内消费完」这一判断的**复核与修订**(含 5 项逐条溯源 + 2 项结构性缺陷修正)

**① 触发**:用户要求以 planner 角色复核该判断与 `design.md` D4。

**② 复核方法**:不采信任何二手转述 —— 逐条溯源到**文件:行 / commit / 台账 Entry**,并用 `metis` 做对抗性 gap 分析;metis 提出的 5 条关键结论**全部一手实证**(见下)。

**③ 核心发现之一 —— S1-1 是**三层**义务,初稿判定「已消费」**过强**

| 层 | 义务 | 状态 | 证据 |
|---|------|------|------|
| ① | **定义层**:ADR 顶点定义升为 `(协议实例, network, vc_id)` | ✅ **已消费** | `ADR-CEP-09 §8.2` · commit `964455e8`(2026-10-22)· `ADR-CEP-12:264` 已同步 |
| ② | **建图层**:按新顶点**实际建图**并跑装配期无环判据 | ⛔ **未消费** | `ADR-CEP-09:221` 措辞是「**建立**之前」;`07 号:684` = 「📐 契约已定义,**🛠️ 未实现**」 |
| ③ | **放行闸**:「不得用其无环判据放行任何协议实例」的解除 | ⛔ **仍有效** | `ADR-CEP-09:221`;当前 0 个 `implemented` 实例 → vacuous 但未解除 |

**④ 核心发现之二 —— 移交本身基于**陈旧快照**

时序:`964455e8`(2026-10-22,ADR 修订 + SLICC 备选登记 + `fabric.json` 冻结规则) **早于** `5b3c7831`(2026-10-23,把 S1-1~S1-5 移交给队列 #2)→ 移交文本把**已完成的 S1-1 / 已登记的 S1-3 / 已登记的 S1-4** 仍写作「待消费」。

**⑤ 核心发现之三(结构性)—— 「判据退化」与「判定结论为红」被焊死**

`ADR-CEP-05:296` 登记「判据 2 状态 ✅ **已触发**(2026-10-22)—— `moesi_gpu` 有静默 `E→M`,判据 (b) 成立 → **4 值 `perm` 确无法表达**」→ **R-2 已命中** ⇒ **S1 的设计结局本就是「投影表达力不足」**。而初稿的验收口径「3 个用例全绿」会把这个**正确结论**变成**不可归档** → KPI slot 无法释放 → **级联阻塞整个 Stage 1**。

**⑥ 核心发现之四 —— 错误因果会被归档进永久契约**

权威逐网络边表(ArchForge `32 号 §6.0.3`):`compute_noc` 5 条 + `ualink_fabric` 3 条,**跨网边 = 0**。故扁平化后出现的 2↔3 环是「**网络别名造成的假环**」,而规格 §3.3 的失败含义 ① 写「报环 ⇒ 存在**真实的跨网依赖**」—— 该结论**不成立**,且已进 delta spec → 会随归档进 `openspec/specs/`。

**⑦ 核心发现之五 —— 3 项从未被任何 S1-x 覆盖的孤儿前置**:**M-4 行为侧**(与 S1-5 同一决策)· **`moesi_gpu`/`mesif` 未排期**(S1/S3 的**输入依赖**,无 owner)· **verdict 逐 `(network, vc)` 标注义务**(`ADR-CEP-09:223-224`)。

**⑧ 修订清单(planner 产出,已落盘)**

| 产物 | 修订 |
|------|------|
| `design.md` | **D4 整段重写**(逐项三层判定 + owner 重指向 + 复核节奏 + 3 项孤儿前置)· 新增 **D6**(S3 顶点/边集合**机器可导出等值** + 报环**成因标注** + verdict 粒度 + 反向断言有效期)· 新增 **D7**(红绿语义分离 + S1-5 规约**不进**本 capability)· **C6**(spike 结论不得作放行依据)· **C7**(红绿语义分离为硬约束)· **NG2/NG4/NG6** 修订 · **R6~R10** 增补 |
| `specs/cep-poc-spike-verification/spec.md` | 新增 4 条 Scenario(红绿语义分离 / 顶点边集机器等值 / 报环成因标注 / 反向断言有效期 / 放行禁令 / `status: planned` 防虚假触发)· S1/S3 Scenario 改写 · **每条 Scenario 标注消费者** · Requirement 正文压到 397 字符(`openspec validate` 零 warning) |
| `tasks.md` | **T4 段整段重写**(三层判定表 + 动作项 T4.1~T4.5 + owner 重指向)· **T2.6 / T3.6~T3.8** 结论登记与 verdict 粒度 · **T5.1** 口径改为「用例全绿 **且** 结论已登记」· **T5.8** 「结论为红」路径自查 · **T5.9** Scenario 消费者自查 · **T6.2b** 归档时 owner 重指向 |
| `obj §4.0` / `§4.2` / `phase-10.md` | **三处镜像同改**(per `obj:231`「漂移时以 §4.0 为准」—— 只改 §4.0 会**主动制造**自声明的漂移) |

**⑨ 冻结面与 ADR**:**零改动** —— `ADR-CEP-09` 的顶点定义**已于 2026-10-22 改完**,本轮**不需要**再改任何 ADR(追加 SU 反而会强制 30 成员冻结清单重生成)。故**无需** `--update --authorized-by` 两步流程。

**⑩ 遗留(交 builder)**:**承接链的机器化** —— 建议给 `cep_docs_sync_check.sh` 加**第 7 条断言**(归档件里每个 `S1-N` 标记须在活文档承接列**恰好命中 1 次**且 owner 行**非已归档**)。这能防住本次修订所治的那类漂移(Entry #22 S8 自陈「6 项移交后**无任何机器闸**保证队列 #2/#4/#6 承接」)。属**门禁脚本**改动 → planner 只出规格,实施归 builder(per `design.md` Q3)。

**⑪ 验收**:`openspec validate cpptlm-cep-poc-spike --strict` **rc=0 且零 warning** · `openspec validate --changes --strict` rc=0 · 守门 9 项 rc=0 · 冻结面 diff=0。

**关联**: Entry #22(落地形态待裁决)· Entry #42(本 change 启动)· `umbrella/tasks.md:406-414`(S1-1~S1-5 原文)· `ADR-CEP-09:221-224` · `07 号:684` · `ADR-CEP-05:296`

### Entry #47 — 2026-10-24 [scope-change]

**主题**: **T4.6 / T4.7 实施**(承接 Entry #46 遗留)—— 承接链**机器化** + 旧路径引用清理

**① T4.6 —— 一致性闸新增**断言 ⑦**(移交链的可达性与 owner 活性)**

`scripts/test/cep_docs_sync_check.sh` 第 11 组由 **6** 类断言 → **7** 类。4 条子判定:

| # | 子判定 | 依据 |
|---|-------|------|
| ① | **可达**:归档件(umbrella 归档 `tasks.md`)里每个 `⏭(移交)` 标记 SHALL 在 `obj §4.0` 排队表出现 | §4.0 = 权威表(per `obj:231`) |
| ② | **复述一致**:同一集合 SHALL 在 `obj §4.2` 出现 | §4.2 是 §4.0 的同义复述 |
| ③ | **owner 活性**:§4.0 / §4.2 中凡**行内提及移交标记**的行,其 change 名 SHALL **非已归档 change** | 防「指向已归档 change 的无主债」 |
| ④ | **反向不凭空**:活文档出现的 `S1-N` SHALL 在归档件中**存在**(归档件含 S1-1 ~ S1-7) | 防幻觉项 |

**实现要点**:标记展开须支持**区间写法**(`S1-1 ~ S1-5` —— `phase-10.md` 用的是区间写法);判定载体 = §4.0 + §4.2 两张表,`phase-10.md` **仅作反向检查的来源**(它是依赖图,不是承接列真源)。

**鉴别力实测(3 个变异全部按预期报红)**:移除 §4.0 的 S1-3 提及 → RED(①)· §4.0 队列 #2 的 change 名改成已归档的 `cpptlm-cep-umbrella` → RED(③)· 凭空造 `S1-9` → RED(④)。

**回归覆盖**:`cep_gate_selftest.sh` 新增 **4** 个 fixture(1 绿 + 3 变异必红)→ 场景数 **14 → 18**,已接入 `ctest`。

**② T4.7 —— 旧路径引用清理(9 处)**

planner 审查只识别出 **2** 处 → **全仓扫描后补齐到 9 处**(`test/test_cep_{canonical_invariants,contracts_crosscheck,dummy_signals,module_factory_seam,schema,simulation_mode_lock,tools_contract}.cc` + `test/cep_adr_parser.hh` + `scripts/test/cep_docs_sync_check.sh`)。归档前路径 `openspec/changes/cpptlm-cep-umbrella/`(及其子路径)→ `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/`(及其子路径)。

**不改**:`docs/implementation/*`(历史快照,append-only;`cep_doc_ref_check.sh:113` 的 VIRTUAL_PATHS 已把旧路径登记为**允许的历史引用**)· 归档件自身(不可变)· VIRTUAL_PATHS 条目本身 · `cep_docs_sync_check.sh:526`(glob 已同时接受新旧形态)。

**③ 规格与实现对齐(本次审查的同型病,自查)** —— 任务描述原写「**恰好命中 1 次**」且把 `phase-10.md` 列为判定载体,与**实际实现**(4 条子判定 + `phase-10.md` 仅作反向来源)**不符** → 已把 `tasks.md` T4.6 与 `design.md` Q3 改成**逐字描述实现**。**文档≠实现即债**,本次即由本次审查所治。

**④ 实施中踩到的 2 个坑(记录以免重犯)**

- **自伤 ①**:Entry #47 初稿在反引号里写了**带省略号的伪路径** → `cep_doc_ref_check.sh` **当场报红**「引用不存在的文件」—— 引用检查器是对的,是我写了一个**不存在的路径**。→ 改写为**逐字真实的目录路径**(末尾加「(及其子路径)」说明)。**教训**:台账里引用路径必须**逐字真实**,省略号**不是**合法路径。⚠️ 本条补记**自身**又踩了一次(第一版把伪路径包在反引号里)→ 改为纯文字描述后通过。
- **环境 ②**:本地 `build/` 的 CMakeCache 指向 **`/usr/bin/g++-14`(不存在**,本机只有 g++-13)→ 平时**增量构建不触发编译**所以看不出来;本轮改了 7 个 test 文件的注释 → 触发重编译 → 暴露。`build-asan/` 用的是**存在**的 `/workspace/main/opt/bin/g++-14`。→ 本地重新配置 `build/` 指向存在的编译器(注:`CXX=g++-14` 只是**名字**非全路径 → CMake 拒绝重新配置,须 `CXX=<全路径>`)。**纯本地环境问题,不入仓**。

**⑤ 验收**:守门 9 项 rc=0 · `cep_gate_selftest.sh` **18/18** · `ctest` Release **98/98** · `ctest` ASan **98/98** · `[cep]` 76/1284 · `openspec validate --changes --strict` rc=0 · `format.sh --check` rc=0 · 冻结面 diff=0。

**关联**: Entry #46(planner 审查)· `scripts/test/cep_docs_sync_check.sh` 断言 ⑦ · `scripts/test/cep_gate_selftest.sh`

### Entry #48 — 2026-10-24 [scope-change]

**主题**: **T2 / T3 / T5 实施** —— S1 / S3 spike 落地 + S2 补反向验证 + delta spec ↔ 实现对账测试

**① S1 spike**(`examples/cep_poc_spike_s1.cc`,364 行)—— 4 值 `perm` 投影 vs 6 态状态机:

| 项 | 实测 |
|---|---|
| 序列 | 读X(未命中)→ 读X(命中)→ 写X(命中)→ 写回/驱逐 |
| **判定结论** | `S1_VERDICT=FAIL` —— **仅写路径 1 处不一致**(`S1_MISMATCH_STEPS=1`) |
| **R-2 命中** | `S1_R2=TRIGGERED(state_transition[2][1]=3)` —— 与 `ADR-CEP-05:296` 所引的 transition **逐字一致** |
| 写回 | `S1_WRITEBACK_OBSERVED=1`(内存**内容**比对,非计数器自述) |
| 自检 | `S1_SELFCHECK=OK` |
| **退出码** | **0** —— 判定结论为红**不**导致非零(per `design.md` D7-a) |

**关键建模修正**:读/写路径的**暴露不对称** —— 读路径接口在**响应侧**携带 perm(= 授予的权限,32 号 :338)→ 忠实反映行态;写路径接口在**请求侧**携带 perm(Write → UNIQUE,32 号 :560)→ **静默升级不可见**(32 号 :561 的 R-2)。**这个不对称才是 R-2 的发生点**;若两侧都用同一侧,读路径会产生**假不一致**并淹没 R-2 信号。

**② S2 补反向验证**(T1.3)—— 在**既有** spike 上加自检(不动其 (b) 观测路径):桥接**真实关闭** ⇒ 判据 (b) 必不成立;若仍成立 → 判据对「传播是否真发生」不敏感 → **判据退化** → exit 1。实测 `S2_SELFCHECK=OK`,断言 **22/22** 全绿,结论「判据 (b) 声明层裁定被实测确认」**不变**(Entry #16 的 RUN 证据不受影响)。

**③ S3 spike**(`examples/cep_poc_spike_s3.cc`,377 行)—— 通道依赖图:

| 项 | 实测 |
|---|---|
| 顶点集 | **恰好 8**(2 网络 × 4 VC,按 `fabric.json` + 权威 `vc_binding` 导出) |
| 依赖边集 | **恰好 8**(32 号 :383-384 逐网络边表并集)· **跨网边 = 0** |
| 正向 | 升级顶点下**无环** → `S3_VERDICT=ACYCLIC` · verdict **逐 (network, vc)** 标注(8 行) |
| 反向 | 降级到 (实例, vc_id) → 报出环 `vc3 → vc2 → vc3`(= 2↔3 对)→ `S3_DOWNGRADED_CYCLE=vc2<->vc3` |
| **成因标注** | `S3_CAUSE=alias-false-cycle` —— 跨网边 0 ⇒ 该环是**网络别名造成的假环**,**不是**「真实跨网依赖」(per `ADR-CEP-09:223-224`)—— **修正了规格 §3.3 的错误因果** |
| 有效期 | `S3_REVERSE_VALIDITY=until-3rd-network-appears`(第 3 张网后扁平化可能**掩盖真环** → 该反向断言须重新设计) |
| 放行禁令 | `S3_RELEASE_USE=forbidden`(per `ADR-CEP-09:221`) |

**④ delta spec ↔ 实现对账测试**(T5.9)—— 新增 `test/test_cep_poc_spike_verification.cc`(135 断言):
- 解析 delta spec(active change 优先 → main → 归档)抽出**全部 Scenario 标题**;
- **每条** Scenario 必须在「证据映射表」里有行;**每行**必须匹配到一条真实 Scenario(防陈旧行);
- 每行的**证据**必须真实存在(spike 的机器可读输出 / 仓内文件内容);支持「**不**出现」型证据(如 `status: implemented` 不得出现);
- 另附硬边界的**可执行**核查:三个 spike 源码不得含注册宏 / `implemented` 状态。
→ 这把「每条 Scenario 有消费者」从**声明**变成**可回归验证**(防 M-2 病根,per 台账 Entry #8)。

**⑤ ctest**:98 → **101**(新增 `cep_poc_spike_s1` / `cep_poc_spike_s3` / `cep_poc_spike_verification`)。

**⑥ 实施中踩到的 3 个坑(记录以免重犯)**

- **自检语义**:S1 的自检原写「改错期望 → **必须新增**不一致」→ 但**本就 mismatch** 的点(写路径)改错期望后会变成 match → 被误判为「判据退化」。正确语义是「该点的判定**必须翻转**」—— 翻转同样证明比对器**真的读了**该点的期望。
- **环检测返回值**:`find_cycle` 命中时返回了 DFS **路径**(`stack`)而非重建的**环**(`cyc`)→ 打印出 `0→1→3→2` 这条路径而非真正的 2↔3 环。已改为返回 `cyc`。
- **对账测试自伤**:映射表有一行**漏了 `mode` 字段** → 聚合初始化把 `mode` 置 nullptr → `std::string(nullptr)` 抛异常,掩盖了真正的证据缺失。已补字段 + 加 null 防御(缺失字段视为 `present`)。

**⑦ 验收**:`ctest` **101/101** · `[cep]` 76/1284 + 1 新用例 135 断言 · 3 个 spike ctest 全绿 · 守门 9 项 rc=0 · `openspec validate --changes --strict` rc=0 · `format.sh --check` rc=0 · 冻结面 diff=0。

**关联**: Entry #46(planner 审查)· Entry #47(T4.6/T4.7)· `docs/implementation/2026-10-22-cep-poc-spike-spec.md` §3.1/§3.3

---

### Entry #49 — 2026-10-24 [scope-change]

**主题**: **critic 复审 REJECT(4 BLOCKERS)全部修复 + 逐条反向验证有牙** —— 归档前置门(决策 C1)

**复审范围**: commit `39abd1d5`(3 新文件 + delta spec)+ 4 裁决落盘方案。结论:**REJECT**。

**① 4 个 BLOCKERS(全部实测证实 + 修复 + 反向验证)**

| # | BLOCKER | 根因 | 修法 | 反向验证 |
|---|---------|------|------|---------|
| **B1** | S2 反向验证**结构性无判别力** | `b_holds_neg` 在 `fwd_enabled=false` 时**构造性恒 false**(fallback 分支必然置 `source=memory`)⇒ `selfcheck_ok ≡ b_holds_main` ⇒ **删掉整段 [S2][NEG] 实验自检仍报 OK** | 自检加 `neg_is_real_fallback`(须真拿到旧值 `kOldValue` + 来源内存 + 不可达;默认构造的 `neg_obs(observed_value=0)` 不满足)+ 输出 `S2_NEG_REAL_FALLBACK=1` 作对账锚 | 换入 mutant(`s2_nonegexp`,删整段 NEG)→ 对账测试**变红** ✅ |
| **B2** | S3 pitfall② 的修复**无回归锁** | `S3_DOWNGRADED_CYCLE` **硬编码** `"vc2<->vc3"`(不来自实际环,恒真);`has2&&has3` 对错路径 `0→1→3→2` **也恒真** | 改为 `format_cycle(flat_cycle)`(**从实际环**排序去重格式化)+ 断言环**恰好**由 `{vc2,vc3}` 构成(错路径得 `{0,1,2,3}` 即红) | 构造 subtle mutant(`return stack` + 自检移除 → spike **exit 0**)→ 对账测试的 **needle 变红**(输出 `vc0<->vc1<->vc2<->vc3` ≠ `vc2<->vc3`)✅ |
| **B3** | `ctest -R cep_poc_spike` 实为 **4** 个,与 spec 明文「**恰好命中 3**」**直接矛盾**;且 `tasks.md:115` T5.1 是**虚假 `[x]`** | 验证用例命名 `cep_poc_spike_verification` **撞了前缀**(`ctest -R` 匹配 NAME 而非 tag) | 验证用例改名 `cep_spike_delta_verification`(不含前缀)+ 对账测试加**注册数断言**(`add_test(NAME cep_poc_spike*` == 3)+ spec 补「验证用例 SHALL 不用该前缀命名」 | `ctest -R cep_poc_spike -N` → **3** ✅ |
| **B4** | `status: "planned"` Scenario 的**证据全部空转** | 仓内**无实例数据文件** ⇒ 三份 spike 源零 `status` 串 ⇒ `absent` needle **天然通过**;且 needle 带空格(`, `)→ `{"status":"implemented"}` **可绕过** | Scenario 改写为「spike **SHALL NOT** 引入协议实例声明」+ needle 改用**裸串** `status` + 三份源一致性检查 | 注入 `{"status":"implemented"}`(**无空格**变体)→ 对账测试**变红 2 处** ✅ |

**② 5 项 SHOULD_FIX**:`mode` 字段缺失 → **改判失败**(原静默按 `present`,会让漏字段的 `absent` 行静默失效)/ 枚举循环 `REQUIRE` → `CHECK`(累积,避免首败掩盖后续缺口)/ ctest 加 **`DEPENDS`** 边(`ctest -j` 就绪竞态)/ `s3.cc` 显式 `#include <functional>` / 归档路径**动态发现**(原写死 `2026-10-23-`,归档后必落空)。

**③ 3 项 NITS**:`364/377 行` → 实测 **366/379** / `[cep] 76/1284` → 实测 **77/1400** / `titles.size() >= 10` → **恰好 15**(Scenario 条数登记)。

**④ 决策 C2 落盘(丙 + 2 补丁)**

- **新增 Scenario**:「S1 当前判定结论与有效期登记」—— **主体断言状态**(R-2 需求本体成立)+ **事件登记**(`S1_VERDICT=FAIL`,仅写路径)+ **有效期**(`S1_VALIDITY_UNTIL=queue6-perm-schema-evolution`)
- **补丁 1(有效期**必须**有机器检测)**:`cep_docs_sync_check.sh` 新增**断言 ⑧** —— ① 有效期锚须在 spike 源内;② **届满条件**出现时(queue #6 目录出现 **或** schema 引入 `perm` 形态字段)→ **报红**。反向验证:临时造 `cpptlm-cep-chi-rnf-subset/` 目录 → 断言 ⑧ **fire** ✅
- **补丁 2(拆定时炸弹)**:「spike 的硬边界」Scenario 改写 ——「**可丢弃**」SHALL 理解为「不构成生产依赖」,三份 spike SHALL **在本契约有效期内保留**(它们是对账测试的证据源)

**⑤ 关键发现:`cep_gate_selftest.sh` 的沙箱不含 `examples/`** → 断言 ⑧ 首版在 fixture 里报「锚缺失」(4 项 fixture 假红)。已改为**容忍源缺失**(沙箱跳过而非报红)。**这是一次「新断言与既有闸的交互」被 fixture 集当场抓出** —— 印证 fixture 集的价值。

**⑥ meta 观察(本轮两 agent 分析的核心产出)**

oracle 的 4 条推荐都倾向**动作**侧(冻结 / 拆分 / 归主 / 写法丙);metis 的 4 条攻击是**同一个问题** ——「**该动作的落盘机制是否真能兑现承诺**」。判据应统一为:**该决策落盘后,有没有机器判据能防止它漂移?** 按此尺子,4 处缺机器判据(有效期 / S1-1③ 解除条件 / S1-1② 镜像抗漂移 / 断言⑦ 孤儿前置盲区)已**逐条补上或登记**。

**⑦ 验收**:`ctest` **101/101** · `[cep]` **77 case / 1400 断言** · 对账测试 **116 断言** · `ctest -R cep_poc_spike` **3** · 守门(除**既存** `markdown_link_check` 65 处失效链接,**非本轮引入**)全 rc=0 · `cep_gate_selftest` **18/18** · `openspec validate --changes --strict` rc=0 · `format.sh --check` rc=0 · 冻结面 diff=**0**。

**关联**: Entry #48(S1/S3 实施)· Entry #46(planner 审查)· 首轮 critic 会话

---

### Entry #50 — 2026-10-24 [scope-change]

**主题**: **critic 二轮复审 REJECT(1 BLOCKER + 4 SHOULD_FIX)全部修复 + 逐条 mutant 反证**

**二轮结论**: 上轮 4 个 BLOCKER 的**原 mutant 全部被真正拦住**(critic 原话「非仅靠 spike 自身 rc —— 强化变体证明 needle 确有牙」);但发现 **B2 修复对同类缺陷仍可绕过**,且 4 项 SHOULD_FIX。**公平面确认**: 二轮的 REJECT **不是**对上轮修复的否定,而是对**修复深度**的加码。

**① BLOCKER-1(二轮):B2 修复缺「边敏感度」** —— `exactly_2_3` 只比**去重集合**,一个**忽略输入的硬编码检测器**仍能通过

- **critic 的 mutant B2e**:`find_cycle` 对降级图**直接返回硬编码** `{2,3,2}`(完全不看边)→ spike **exit 0** + 对账测试**全绿**。
- **critic 的 mutant B2d**:`find_cycle` 忘记 `cyc.push_back(w)`(环**不闭合**)→ 同样绕过 needle 与自检。
- **修法(新增 2 条自检)**:
  - **③ 边敏感度**:逐条删掉降级图的边,**至少存在一条边**使判定由「有环」翻转为「无环」→ 输出 `S3_EDGE_SENSITIVE`。否则「检测器**不看边**」→ 判据退化。
  - **④ 环闭合性**:环的顶点序列 SHALL 首尾同一顶点 → 输出 `S3_CYCLE_CLOSED`。捕获「忘闭合」类重构 bug。
- **反证**:B2e mutant → `S3_EDGE_SENSITIVE=0` + `SELFCHECK=DEGENERATE` + **exit 1** ✅;B2d mutant → `S3_CYCLE_CLOSED=0` + **exit 1** ✅。

**② 4 项 SHOULD_FIX**

| # | 问题 | 修法 | 反证 |
|---|------|------|------|
| **S-1** | `count_add_test` 用精确前缀 `add_test(NAME `,`add_test( NAME`(括号后带空格,CMake 合法)**绕过** | 正则化 `add_test\s*\(\s*NAME\s+<prefix>` | 空格变体注入 → **红** ✅ |
| **S-2** | `text.find("status")` 裸串代理,**字面量拼接**(`"sta" "tus"`)绕过 | 新增 `contains_token()`(**剥空白与引号**后查 token)+ 二进制级兜底(实测**未使用的 static 变量在 -O2 下被完全消除** → 二进制检查仅当串被实际使用时有效,已在注释说明为**尽力而为**) | 拼接变体注入 → **红** ✅ |
| **S-3** | `neg_is_real_fallback` 校验**字段值**而非**实验来源**,「伪造三字段」可绕过 | NEG 观测**复用主场景同一 `bridge` 对象** + `Bridge::fallback_hits` **由 Bridge 内部递增**(伪造者无法递增)→ 打印 `S2_NEG_FALLBACK_HITS` | forge mutant → `S2_NEG_FALLBACK_HITS=0` + 自检 fire + **exit 1** ✅ |
| **S-4** | `cep_gate_selftest.sh` 沙箱不含 `examples/` → 断言 ⑧ 在闸自检里**永不执行**(覆盖缺口) | `cep_gate_harness.py` 的 `FILES` 纳入 `examples/cep_poc_spike_s1.cc` + `protocol_instance.schema.json`;断言 ⑧ 的「源缺失」由**跳过**改为**报红** | 闸自检 **18/18** 全过 ✅ |

**③ 过程中踩的坑(记录以免重犯)**

- **`git checkout -- <file>` 会丢弃该文件的未提交改动**:本会话两次用 `git checkout` 还原测试注入,把 `s1.cc` 的 `S1_VALIDITY_UNTIL` 一并回滚(另一次把 `s3.cc` 的 `format_cycle` 回滚)。**教训**:对**带未提交改动**的文件做变异测试,必须 `cp` 备份 + `cp` 还原,**禁用 `git checkout`**。
- **同秒时间戳陷阱**:写源后立即 `cmake --build` 可能因时间戳同秒而**不重编** → mutant 未生效却显示「构建成功」。**教训**:变异构建前先 `rm` 目标二进制 + `touch` 源 + `sleep 1.1`。

**④ 三轮累计:每轮 `REJECT` 都揭示了**新的一类**「有牙」缺口**

| 轮 | 缺口类别 | 修法 |
|---|---------|------|
| 一轮 | **判别力**:比对器/检测器恒真 | 加反向验证 + 自检 |
| 二轮 | **真实性**:反向实验可被删除/伪造 | provenance 锚(同一对象 + 内部计数) |
| 三轮 | **敏感度**:检测器忽略输入仍能过 | 边敏感度 + 结构断言 |

→ **规律**:断言「某常量存在」永远不够,必须断言「**输入改变时输出必须改变**」。

**⑤ 验收**:`ctest` **101/101** · `[cep]` 77 case · 对账测试 **125 断言** · `ctest -R cep_poc_spike` **3** · 守门全 rc=0 · `cep_gate_selftest` **18/18** · `openspec validate --changes --strict` rc=0 · `format.sh --check` rc=0 · 冻结面 diff=**0**。

**关联**: Entry #49(一轮修复)· 二轮 critic 会话

---

### Entry #51 — 2026-10-24 [scope-change]

**主题**: **critic 三轮复审 REJECT(2 BLOCKERS)全部修复** —— 三轮累计揭示「有牙」的三类缺口

**三轮结论**: 上轮 4 项(S-1~S-4)**全部被确认修复**;发现 **2 个新独立 BLOCKER**(非上轮残留)。

**① BLOCKER-1(三轮):② 注入环自检**判别力不足**

- critic 的 **m8**(`find_cycle` 只从 `g.begin()` 起 DFS —— 教科书级漏掉遍历根循环)→ 逃逸。
- critic 的 **m6**(只识别互指 **2 环**)→ 逃逸。
- **修法**:② 注入由「1 个 2 元环」扩为 **3 路**:
  - 2 元环 + 跨网边(基础)
  - **3 元环** `1→3→2→1`(刻意避开 `2→0`:既有 `0→2` 会合成 2 环让 m6 仍能检出)
  - **环在非首个弱连通分量**(`compute_noc|0` 孤立 + `ualink|2↔3` 成环 → 捕「只从首顶点 DFS」)
  - 打印 `注入检出(2元环=x / 3元环=x / 隔离分量环=x)` 并进对账 needle。
- **反证**:m8 → `隔离分量环=0` + exit 1 ✅;m6 → `3元环=0` + exit 1 ✅。

**② BLOCKER-2(三轮):`count_add_test` 源码正则与 `ctest -R` 语义不等价**

- **实测**:`ADD_TEST`(大写)/ `add_test(NAME "..")`(引号)/ helper macro 三种 CMake 合法写法**都真实注册**,但源码正则数不到(真实 **3** / 正则 **1**)。
- **修法**:删 `count_add_test`(源码代理),改 `count_registered_tests()` —— 解析 CMake 生成的 `<build>/**/CTestTestfile.cmake`(**那是 ctest 的真实输入**)。
- 断言随之改为读 `CTestTestfile.cmake`。

**③ SHOULD_FIX(三轮)**

| # | 问题 | 修法 |
|---|------|------|
| S-1 | ⑧(本 change 新增断言)**无 fixture** —— 删掉整段闸自检仍 18/18 绿(自称标准没落到自己身上) | 补 **3 条** `run_case`:基线 GREEN / 删有效性锚 → fire / schema 插 `perm_form` → fire |
| S-2 | harness `openspec` **symlink 不密封**(沙箱内新建会**写穿**真仓,已实测污染) | 改 **整目录拷贝**(0.84s/次);注:「目录实体化 + 文件 symlink」不可行 —— `openspec` CLI 解析 symlink 后拒绝沙箱外路径 |
| S-3 | S-2 的 `contains_token` 是**尽力而为**非密封(`\x73tatus` 转义 / 运行时构造仍可绕) | 注释口径改正为「尽力而为、非密封;目标是**防无意引入**」 |

**④ 过程中踩的坑**

- **多行字符串粘连**:我用 `edit` 删了一个空行 → `pass'run_case` 粘连成一条 → ⑦ baseline 恒 `rc=?`。**教训**:shell 里的多行字符串参数,编辑时**不可删结尾空行**。
- **`git checkout` 回滚未提交改动**(已在 Entry #50 记录,本轮复发一次于 `s1.cc`)。

**⑤ 三轮累计规律(本 change 最有价值的产出)**

| 轮 | 「有牙」的哪类缺口 | 修法 |
|---|------------------|------|
| 一轮 | **判别力**:比对器/检测器**恒真** | 加反向验证 + 自检 |
| 二轮 | **真实性**:反向实验**可被删除/伪造** | provenance 锚(同一对象 + 内部计数) |
| 三轮 | **敏感度**:检测器**忽略输入**仍能过 | 多路注入(2/3 元环 + 隔离分量) + 边敏感度 |

→ **通用判据**:断言「某常量存在」永远不够;必须断言「**输入改变时输出必须改变**」。三轮 critic 的每一条 BLOCKER 都是这条判据的缺失实例。

**⑥ 验收**:`ctest` **101/101** · `[cep]` 77 case / 1411 断言 · 对账测试 **125 断言** · `ctest -R cep_poc_spike` **3** · `cep_gate_selftest` **21/21**(18 → 21) · 守门全 rc=0 · `format.sh --check` rc=0 · 冻结面 diff=**0**。

**关联**: Entry #49(一轮)· Entry #50(二轮)· 三轮 critic 会话

---

### Entry #52 — 2026-10-24 [scope-change]

**主题**: **critic 四轮复审 REJECT(1 BLOCKER + 3 SHOULD_FIX)全部修复** —— 四轮揭示「守卫自身的语义等价性」缺口

**四轮结论**: 三轮的 2 个 BLOCKER(注入判别力 / `count_add_test`)与 S2-B1 **确已修复**(critic 实测 m8/m6/4 种 CMake 写法/改名回前缀/硬编码伪造 S2 **全部现红**,非过拟合证据 `m_kahn` 现绿);发现 **1 个新 BLOCKER**(新守卫自身)。

**① BLOCKER(四轮):`count_registered_tests` 用前缀匹配,而 `ctest -R` 是**非锚定**正则**

- **实测**:新增 `add_test(NAME test_cep_poc_spike_extra ...)`(**含前缀但不以之开头**)→ `ctest -R cep_poc_spike` 真实命中 **4**(违反 spec「恰好 3」),而对账测试的守卫仍 **绿**(用 `add_test(cep_poc_spike` 前缀匹配,数不到)。
- **为何是可信路径而非蓄意构造**:按仓内既有 `test_*` 命名习惯新增一条 ctest 即触发,**不需要知道守卫内部实现**。
- **修法**:抽 `add_test(<name> ...)` 的 `<name>`(剥引号)→ 用 `name.find(prefix) != npos`(**与 `ctest -R` 的非锚定语义一致**)。
- **反证**:`test_cep_poc_spike_extra` 注入 → 守卫**红** ✅。

**② 3 项 SHOULD_FIX**

| # | 问题 | 修法 |
|---|------|------|
| **SF-1** | 注入集缺「环长 > 3」与「DFS 前缀污染」→ m9(只认 ≤3 元环)/ m21 逃逸(输出与基线**逐字节相同**) | ② 注入由 3 路扩为 **4 元环 + 隔离分量**(`compute_noc|0` 孤立 + `ualink|0→1→2→3→0`),一次覆盖 m8 与 m9 |
| **SF-2** | `contains_token(text,"status")` 是**裸子串**扫描 → **注释/标识符里的 `status` 也判红**(实测误报面:未来一次无害编辑即红) | 改 `contains_instance_status_key()` —— **结构化**检查键位形态(`{status:` / `,status:`,剥空白+引号后),既不误报又仍捕获拼接变体 |
| **SF-3** | `count_in_binary` 扫**整个二进制**(含静态链接的 `cpptlm_core.a`)→ 未来 core 任意一处出现该子串都会让三份 spike **无理由变红**,且**非密封**(`-O2` 消除未使用的 static 变量) | **删除**二进制兜底,主判据保留在 ⑧ 的结构化源码检查 |

另修 2 项 NIT:`fs::relative` 抛异常重载 → `error_code` 重载;移除不再需要的 `<regex>`。

**③ 反证(全部实测)**

| mutant | 结果 |
|--------|------|
| `test_cep_poc_spike_extra`(含前缀不以之开头) | 真实命中 4 → 守卫**红** ✅ |
| m9(只认环长 ≤ 3) | `4元环+隔离分量=0` → **exit 1** ✅ |
| 注释 + 标识符含 `status`(误报面) | **不误报**(124 断言全绿)✅ |
| 真声明 `R"({"status":"implemented"})"` | **红** ✅ |

**④ 四轮累计规律(补三轮表)**

| 轮 | 「有牙」的哪类缺口 | 修法 |
|---|------------------|------|
| 一轮 | **判别力**:比对器/检测器恒真 | 反向验证 + 自检 |
| 二轮 | **真实性**:反向实验可被删除/伪造 | provenance 锚(同一对象 + 内部计数) |
| 三轮 | **敏感度**:检测器忽略输入仍能过 | 多路注入 + 边敏感度 |
| **四轮** | **等价性**:守卫与它守护的语义**不等价**(前缀 vs 非锚定正则);且**误报面**本身是缺陷 | 抽真实 name + 非锚定匹配;判据收窄为结构化键位 |

→ **补充判据**:守卫必须与**它声称守护的那条命令/语义**等价(此处 `ctest -R` 的非锚定正则);否则守卫会「看着有牙,实则漏风」。另:**误报与漏报同为缺陷** —— 一个会因无害编辑变红的判据,会训练出「忽略红」的习惯。

**⑤ 验收**:`ctest` **101/101** · `[cep]` 77 case · 对账测试 **124 断言** · `ctest -R cep_poc_spike` **3** · `cep_gate_selftest` **21/21** · 守门全 rc=0 · `format.sh --check` rc=0 · 冻结面 diff=**0**。

**关联**: Entry #50(二轮)· Entry #51(三轮)· 四轮 critic 会话

---

### Entry #53 — 2026-10-24 [scope-change]

**主题**: **critic 五审 APPROVE_WITH_CONDITIONS** —— 条件项(SF-A / SF-B)已修;**四轮 BLOCKER + 3 SHOULD_FIX 全部确认到位**

**五审结论**: **无 BLOCKER**。critic 明确「上一轮 4 项修复均**实测有牙**(BLOCKER 现红、m6/m8/m9 现红、SF-2 误报消除、`count_in_binary` 已删)」;并**主动收敛**评审立场:「连续 4 轮 REJECT 后不再为『刻意构造』类逃逸 REJECT —— 本轮残留逃逸全部需要**知道注入集内部实现**或**编辑 fixture**,不构成可信回归路径」。

**① 条件项 SF-A(五审:本轮**新引入**的真实矛盾,归档前必修)**

- **问题**:我为消除误报面(SF-2)把实现改为**键位形态**检查,但 **spec.md:118 与 tasks.md:142 仍写「断言 SHALL 用**裸串** `status`」** → 形成**代码不满足的 SHALL**;若不修,delta spec 归档后会把该矛盾**并入 main specs(永久)**。
- **修法**:spec.md:115/118 + tasks.md:142 改为与实现一致的**结构化键位**口径(`{status:` / `,status:`),并**写明不采用裸串的理由**(误报面会训练出「忽略红」的习惯)+ 明示「目标是防无意引入、不承诺防蓄意规避」。
- **教训**:改动**实现**时,必须同步**所有引用该实现的文本**(spec / tasks / 台账)—— 否则「实现更严/更松」会静默漂移成契约矛盾。

**② 条件项 SF-B(五审:注入 fixture 的判别属性不自守,覆盖侵蚀静默)**

- **问题**:`inj4`(4 元环 + 隔离分量)是为捕 m8/m9 新增的,但其**环长=4**与**分量数=2**无任何断言;对账 needle 只是**硬编码 printf 标签**。critic 实测:`degrade4to3`(4 元环→3 元环)/ `drop_isolated`(删孤立顶点)后,**标签仍印「全 1」而保护静默消失**。
- **修法**:`s3.cc` 断言**实测**环长(`set(cycle).size()`)与**实测**弱连通分量数,并打印 `注入集自守: 环长=(2,3,4) 分量数=2`;不符预期 → 自检 fail。对账 needle 加锚该行。
- **反证**:`degrade4to3` → `环长=(2,3,3)` + **自侵蚀** + exit 1 ✅;`drop_isolated` → `分量数=1` + **自侵蚀** + exit 1 ✅。
- **一般化**:「测试夹具本身」也是判据的一部分 —— **夹具的判别属性必须自守**,否则侵蚀夹具即可静默关闭保护。

**③ 已修 NIT**:`cep_gate_harness.py` 的模块 docstring 仍写「`openspec` → symlink」(与 :77 的 copytree 不符)→ 已更新,并写明改拷贝的**原因**(symlink 写穿真仓)。

**④ 五轮累计规律(完整表)**

| 轮 | 「有牙」的哪类缺口 | 修法 |
|---|------------------|------|
| 一轮 | **判别力**:比对器/检测器恒真 | 反向验证 + 自检 |
| 二轮 | **真实性**:反向实验可被删除/伪造 | provenance 锚(同一对象 + 内部计数) |
| 三轮 | **敏感度**:检测器忽略输入仍能过 | 多路注入(2/3 元环 + 隔离分量)+ 边敏感度 |
| 四轮 | **等价性**:守卫与它守护的语义不等价;误报面同为缺陷 | 抽真实 name + 非锚定匹配;判据收窄为结构化键位 |
| **五轮** | **夹具自守**:注入集被侵蚀后保护静默消失;改实现漏改引用文本 | 断言夹具的判别属性;实现与文本同步 |

**⑤ 已知边界(五审登记,不阻归档)**:N-1/N-2(需知注入集内部实现才能触发的探测矩阵逃逸,如「环长上限 = 最大注入环长」的检测器 —— **任何有限注入集都可被此类检测器击败**,属固有边界)/ N-3(键位形态的窄误报面:注释里写 `{"status":...}` 会红)。

**⑥ 验收**:`ctest` **101/101** · `[cep]` 77 case · 对账测试 **126 断言** · `ctest -R cep_poc_spike` **3** · `cep_gate_selftest` **21/21** · 守门全 rc=0 · `format.sh --check` rc=0 · 冻结面 diff=**0**。

**关联**: Entry #51(三轮)· Entry #52(四轮)· 五轮 critic 会话

---

### Entry #54 — 2026-10-24 [scope-change]

**主题**: **critic 六审 REJECT(1 BLOCKER)修复** —— 判据覆盖承诺与实测不符 + 判据**无正控**

**六审结论**: SF-B(注入集自守)**确认有效**(critic 实测 `degrade4to3` / `drop_isolated` 均 rc=1);但 **SF-A 未真正闭合**,且发现 **1 个新 BLOCKER**。

**① BLOCKER(六审):转义形态的真实 JSON 声明**两层判据全漏** + 该 helper **无正控**

- **漏判路径(无需内部知识,是「无意粘贴真实声明」的自然结果)**:C++ 里 JSON 声明**必然**写成转义形态 `"{\"status\":\"implemented\"}"`。而我的归一化器只剥空白 + 引号,**不剥反斜杠** → 归一化得 `{\status\:...}` → 键位形态 `{status:` **失配**;映射表的裸子串 `"status":` 在该形态下**同样不出现**(`s` 后紧跟 `\`)。⇒ **两层同时静默**。
- **且 `spec.md:118` 做了具体而**错误**的覆盖承诺**(「既捕获 `"sta" "tus"` 拼接变体」)—— 按 C++ 语义,该拼接**只**在转义形态下产生真实声明,而该形态恰**不被捕获**。
- **结构性根因**:该判据**无正控** —— 把 helper 改为恒假(`return false;`)后对账测试仍 **131/131 绿**(修改前 126/126),即「一次把 helper 改坏的重构不会被任何测试拦住」。对照 S1/S2/S3 判据均有显式反向验证,本条「有牙」**无证据**。
- **修法**:
  - 归一化时**同时剥反斜杠** → 转义形态与 raw string 形态归一化一致,均命中 `{status:`;
  - 新增 **5 条正/负控**(阳性:raw / **转义** / 拼接三形态必须判真;阴性:注释 / 标识符必须判假);
  - `spec.md:118` 的覆盖声明改为与实测一致(明写「剥反斜杠是必需的,否则转义形态会漏判」+ 「断言 SHALL 有**正控**」)。
- **反证(全部实测)**:

| 复攻 | 期望 | 实测 |
|------|------|------|
| 自然转义形态(六审 B1 核心) | 红 | **1 failed** ✅ |
| 恒假 helper(正控) | 红 | **3 failed** ✅ |
| 注释 / 标识符(负控) | 绿 | **全绿** ✅ |
| raw string 形态 | 红 | **2 failed** ✅ |

**② SF-1(六审 SHOULD_FIX):`design.md:118` 残留旧口径**

- `design.md:118` 仍写 S1-2 的动作是「spike 内声明 `status` 必须为 `planned`」,而 `spec.md` 已改为「SHALL NOT 引入任何声明键」→ 二者不可同真。
- **修法**:`design.md:118` 对齐为「SHALL NOT 引入声明键」,并注明该口径由 B4 修订。

**③ 六轮累计规律(完整表)**

| 轮 | 「有牙」的哪类缺口 | 修法 |
|---|------------------|------|
| 一轮 | **判别力**:比对器/检测器恒真 | 反向验证 + 自检 |
| 二轮 | **真实性**:反向实验可被删除/伪造 | provenance 锚 |
| 三轮 | **敏感度**:检测器忽略输入仍能过 | 多路注入 + 边敏感度 |
| 四轮 | **等价性**:守卫与它守护的语义不等价;误报面同为缺陷 | 真实 name + 非锚定;结构化键位 |
| 五轮 | **夹具自守**:注入集被侵蚀后保护静默消失 | 断言夹具判别属性 |
| **六轮** | **覆盖承诺 vs 实测**:文档声称的覆盖形态与实现不符;**判据无正控**(恒假也全绿) | 剥反斜杠覆盖转义形态;补正控 |

→ **补充判据**:① 判据的**文档承诺**必须与**实测覆盖**逐形态对齐(不能只写「捕获 X 变体」而不验);② **纯负向断言必须有正控** —— 否则判据退化为恒假时全绿,保护静默消失。

**④ 验收**:`ctest` **101/101** · `[cep]` 77 case · 对账测试 **131 断言** · `ctest -R cep_poc_spike` **3** · `cep_gate_selftest` **21/21** · 守门全 rc=0 · `format.sh --check` rc=0 · 冻结面 diff=**0**。

**关联**: Entry #53(五审)· 六轮 critic 会话

---

### Entry #55 — 2026-10-24 [scope-change]

**主题**: **critic 7 轮复审 APPROVE + 队列 #2 完成归档 + owner 重指向(4 裁决落盘)**

**① critic 复审终局(7 轮)**

| 轮 | 结论 | 揭示的「有牙」缺口类别 | 修法 |
|---|------|---------------------|------|
| 1 | REJECT (4 BLOCKERS) | **判别力**:比对器/检测器**恒真** | 反向验证 + 自检 |
| 2 | REJECT (1 BLOCKER + 4 SF) | **真实性**:反向实验**可被删除/伪造** | provenance 锚(同一对象 + 内部计数) |
| 3 | REJECT (2 BLOCKERS + 3 SF) | **敏感度**:检测器**忽略输入**仍能过 | 多路注入(2/3/4 元环 + 隔离分量)+ 边敏感度 |
| 4 | REJECT (1 BLOCKER + 3 SF) | **等价性**:守卫与它守护的语义**不等价**;误报面同为缺陷 | 真实 name + 非锚定匹配;结构化键位 |
| 5 | APPROVE_WITH_CONDITIONS | **夹具自守**:注入集被侵蚀后保护**静默消失** | 断言夹具判别属性 |
| 6 | REJECT (1 BLOCKER + 1 SF) | **覆盖承诺 vs 实测** + **判据无正控**(恒假也全绿) | 剥反斜杠覆盖转义形态;补正控 |
| 7 | **APPROVE** | —— | 全部闭合;残留 2 NIT 已修(已知边界明示) |

**六类缺口的通用判据(本 change 最有价值的产出)**:① 断言「某常量存在」永远不够 —— 必须断言「**输入改变时输出必须改变**」;② 判据的**文档承诺**须与**实测覆盖**逐形态对齐;③ **纯负向断言必须有正控**;④ **夹具的判别属性必须自守**;⑤ 守卫须与**它声称守护的那条命令/语义**等价;⑥ **误报与漏报同为缺陷**。

**② 队列 #2 完成归档**

- `openspec archive cpptlm-cep-poc-spike --yes` → 归档名 **`2026-10-25-cpptlm-cep-poc-spike`**。
- delta capability `cep-poc-spike-verification`(**15 Scenario**)已合并进 `openspec/specs/cep-poc-spike-verification/spec.md`(**完成归档**语义,非 ⏸ 暂存)。
- tasks **全勾**(T0~T6,含 T6.0 critic 前置门)。
- **KPI 1/3 → 0/3**(实测 `openspec list --json`);下一个可启动 = 队列 #3 `cpptlm-cep-stub-cache-core`。
- **归档后对账测试仍绿**(131 断言)—— `find_delta_spec()` 的 active → main → 归档三级查找**实际生效**(命中 main specs)。

**③ owner 重指向(4 裁决落盘,归档时不产生无主债)**

| 义务 | 新 owner | 落盘要点 |
|------|---------|---------|
| **S1-1 ②** 建图层 | **队列 #4** | 验收:校验器**须在装配路径**调用 + 输出逐 `(network, vc)` verdict;**禁**离线脚本形态。义务未消费但**判据可计算性已由 S3 spike 实证**;**镜像↔权威抗漂移未实证**(metis 反例:改 ArchForge 声明里 `compute_noc.snoop` 的 VC,`cep_decl_alignment_check.sh` 绿、S3 spike 绿、**无人报红**) |
| **S1-1 ③** 放行闸 | **全局不变量**(不挂队列行) | 解除条件 = 「②完成 **+** 首个待放行实例装配验证通过」;首个 `implemented` 实例在 **#6(2028-Q1)**,晚于 #4(2027-Q3)→ 挂 #4 即**永久债**(违 C4)。建模为 **#6+ 每个协议实例 change 的放行 Scenario** |
| **M-4 行为侧决策** | **队列 #3** | **推翻 oracle 的「零先决条件」假设**(metis 反例已验):改它须同步冻结面 `fabric.schema.json` 的 `vc_count.maximum=4` + lint 第 10 组 + 既有断言迁移 |
| **S1-5 表体** | **队列 #6** | 需真实 adapter;「1 协议实例 = 1 adapter」⇒ 表体行天然分形跟随。**依赖方向**:#3 的 M-4 决策是其**输入前置** |
| S1-2 / S1-3 / S1-4 | **队列 #3+** | 度量口径 + 复核节奏迁至活队列行(随首个 `implemented` 实例 / 第 3 张网出现) |

**④ 结构性修正:「同一根因」→「根因分层」**

- 原 `design.md` D4.4 写「S1-5 与 M-4 行为侧合并为同一决策(**同一根因**)」。
- 裁决**改写为根因分层**:M-4 是**传输层**(`CATEGORY_*→VC` 桶)决策;S1-5 是 **adapter 层**(消息类→`CATEGORY_*`)表体 → **根因不同**,故拆到两个 change。
- oracle 的拆分**方向对但判据用错**(它用「需不需要 adapter」这个**实施判据**);裁决改用**耦合面判据**(M-4 改现状语义 → 越早定越好;#6 表体需 adapter → 天然后置)。#3 的决策是 #6 表体的**输入前置**(依赖方向),不构成「跨季度决策不一致」。

**⑤ meta 观察(两 agent 独立分析的核心产出)**

oracle 的 4 条推荐都倾向**动作**侧(冻结 / 拆分 / 归主 / 写法丙);metis 的 4 条攻击是**同一个问题** —— 「**该动作的落盘机制是否真能兑现承诺**」。统一判据:**该决策落盘后,有没有机器判据能防止它漂移?** 按此尺子,4 处缺机器判据(有效期 / S1-1③ 解除条件 / S1-1② 镜像抗漂移 / 断言⑦ 孤儿前置盲区)已**逐条补上或登记**。

**⑥ 决策 B(fabric.json 冻结)状态**:**B1' 已裁决,但按时序红线不在本 change 执行** —— T5.5 要求本 change 的「冻结面 diff = 0」,故 `fabric.json` 纳入 M1 冻结集须作为**归档后独立 entry + 独立 commit** 执行(理由收窄为「ArchForge-less 环境唯一**本地** drift 闸」+ 上游刷新协议成文 + 消灭「30 成员」字面量)。

**⑦ 验收**:`ctest` **101/101** · `[cep]` 77 case · 对账测试 **131 断言** · `ctest -R cep_poc_spike` **3** · `cep_gate_selftest` **21/21** · 守门全 rc=0 · `openspec validate --changes --strict` rc=0 · `format.sh --check` rc=0 · **KPI 0/3**。

**关联**: Entry #49~#54(critic 7 轮)· Entry #46(planner 审查)· 五轮/六轮/七轮 critic 会话

---

### Entry #56 — 2026-10-25 [scope-change]

**主题**: **决策 B 落地(B1')** —— `include/tlm/cep/fabric.json` 纳入 M1 冻结集 + 上游刷新协议成文 + 消灭冻结成员数字字面量

**决策 B 的原裁决**(2026-10-24,per oracle + metis 分析):**B1'** = 立即冻结 `fabric.json`,但附 **3 项条款**。本条是它的**执行**。

**① 为什么 `fabric.json` 该冻结(裁决理由已收窄)**

- **原规则**(Entry #9):实例数据**不冻结**(`fabric.json`),触发条件 = 「Stage 1 首个 `implemented` 实例跑过后」;`fabric.schema.json`(契约面)**冻结**。
- **该规则的前提已破两处**(实测):① spike 消费(`examples/cep_poc_spike_s2.cc:407` / `cep_poc_spike_s3.cc:231`,已注册 ctest);② **非 spike 的回归门消费**(`scripts/test/cep_decl_alignment_check.sh:52`,该脚本是 **ctest #85**,注册于 `test/CMakeLists.txt:267`)→ 按 design.md D4.5 自定的度量口径「已被非 spike 的回归门消费」,**触发条件已满足**。
- **收窄后的裁决理由(否定 oracle 的宏大论证)**:冻结 `fabric.json` 的**唯一新增价值** = 「**ArchForge-less 环境下唯一的本地 drift 闸**」—— 因为 `cep_decl_alignment_check.sh` 在 ArchForge 仓缺席时 **SKIP**(实测 `[SKIP]` 样例)。**不**声称「冻结与 NS-R2 不矛盾」之类的宏大论证。
- **反向张力(已处置)**:`fabric.json` 的 `derived_from` 指向**外部仓** → 冻结 = 把外部数据源的快照冻在仓内 → 上游更新时需走刷新流程(见 ②)。**这不是债,是纪律的机器化**。

**② 上游刷新协议(成文,per B1' 条款 b —— 原为「口头纪律」,现为明文流程)**

| 步 | 动作 | 判据 |
|----|------|------|
| 1 | ArchForge 侧更新 `22-nsa §0.5.4.1`(网络清单增删 / `vc_count` 变更) | **必须先**走 ArchForge 自身的 ADR 流程(per NS-R2「network_id 单点权威,增删必须走 ADR」) |
| 2 | 本仓**刷新** `include/tlm/cep/fabric.json` 的 `networks` 段(仅该段;`derived_from` / `authority_note` / `vc_allocation_note` 不动) | 刷新后 `bash scripts/test/cep_decl_alignment_check.sh --strict` 必须 rc=0(⑤ 双向比对) |
| 3 | 刷新 `fabric.schema.json`(**若**槽位上界变化)—— ⚠️ 它**已在**冻结集 → 须走授权闸 | `vc_count.maximum` 与 `protocol_instance.schema.json` 的 `vc_id` 上界**两处同步** |
| 4 | **走授权闸重生成冻结清单** | `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"`(N = 本次刷新对应的台账 entry id)**两步**:先 commit 产物 → 再 `--update` → 再 commit 清单 |
| 5 | **刷新 SLA**:上游更新后 **≤ 5 个工作日**内完成本仓刷新 | 超期 → `cep_decl_alignment_check.sh` 在有 ArchForge 的环境下报红(⑤ 双向)⇒ 天然被发现 |
| 6 | 若刷新**只**涉及派生副本(§2)而**不**涉及 §1 的 ArchForge ADR | 仍走 §4 的同一授权流程(**无例外**)—— 冻结面不因「是派生物」而豁免 |

> **本仓侧不复制上游的「增删网络必须走 ADR」纪律**(NS-R2 的权威在 ArchForge);本仓只保证「**本仓副本与上游一致**」(由 ⑤ 双向比对强制)。

**③ 消灭冻结成员数字字面量(per B1' 条款 c + metis 债 3)**

- **问题**:`artifact_count` 原写死在 **7 个活文档**里 → 每加/减一个冻结成员就要改 7 处 → **重复债**,且漏改任一处即报红(断言 ③)。
- **修法**:
  - 断言 ③ 改为**双形态**:抓到数字 → 必须 == manifest 值(原防漂移能力**不减**);**一处都没抓到数字** → 豁免(「以清单为准」表述无从写错)。**同时存在互相矛盾的两个数字仍报红**。
  - 7 个活文档的「N 成员」字面量 → 改为「**成员数以 `cep_freeze_check.sh` 清单为准**」。
- **效果**:今后增删冻结成员**不再需要**改 7 个文档;而**写了数字**的地方仍受漂移保护。

**④ 新增 freeze 路径的反向验证(per B1' 条款 ④ —— 两条路径的牙必须分别验)**

- **既有** `scripts/test/cep_decl_reverse_verify.sh` 的破坏对象虽含 `fabric.json`,**走的却是 decl check 路径** —— 验的是「声明 ↔ fabric 语义一致」,**不是**「fabric 的 hash 未被静默改写」。**两条路径的牙必须分别验**。
- **新增** `scripts/test/cep_freeze_reverse_verify.sh`:基线绿 → 破坏 `fabric.json` 一字节 → freeze check **必须报红且点名 fabric.json** → 字节级还原 → 回绿。**刻意不注册进 ctest**(它会临时改写冻结产物,并行下会与其他测试读到中间态)。
- **实测**:在 `fabric.json` **尚未**纳入冻结时跑该脚本 → 破坏后 freeze check **仍绿** ⇒ 脚本正确暴露「闸对派生文件无牙」;纳入冻结后 → **红且点名** ✅。

**⑤ 附带:归档后的 main spec 也纳入冻结**

`openspec/specs/cep-poc-spike-verification/spec.md`(队列 #2 归档后落 main spec)按 `cep-core-contracts` 的**同一先例**纳入冻结 —— 否则归档后的**系统契约面**可被静默改写(与 umbrella 归档时的处理一致)。

**⑥ 冻结成员数**:30 → **32**(新增 `include/tlm/cep/fabric.json` + `openspec/specs/cep-poc-spike-verification/spec.md`)。

**⑦ 验收**:`cep_freeze_check.sh` rc=0 · `cep_freeze_reverse_verify.sh` 闭环成立 · `cep_docs_sync_check.sh --strict` rc=0(断言 ③ 新口径)· `cep_doc_ref_check.sh --strict` rc=0 · `ctest` **101/101** · `[cep]` 77 case / 1415 断言 · `format.sh --check` rc=0。

**关联**: Entry #9(原冻结规则)· Entry #55(队列 #2 归档 + 4 裁决)· 2026-10-24 决策 B(B1')落盘

---

### Entry #57 — 2026-10-25 [scope-change]

**主题**: **S1-4 消费登记 + 断言 ⑦ 孤儿前置盲区封堵**(Phase 3 收尾 + Phase 4)

**① S1-4 已消费**(`fabric.json` 纳 M1 冻结集)

- **原判据**(design.md D4.1):⏸ 「规则已登记 · **前提已破**」—— 规则 = Entry #9「`fabric.json` 不冻结,Stage 1 首个 `implemented` 实例跑过后再纳」;触发条件 = 「已被非 spike 的回归门消费」。
- **消费事实**(Entry #56 已执行):`include/tlm/cep/fabric.json` **已在 M1 冻结集**(成员 30 → 32)→ 规则**已执行**,S1-4 由「⏸ 前提已破」变为 **✅ 已消费**。
- **owner**:本项在队列 #2 归档时已重指向至**队列 #3+**(随首个 `implemented` 实例 / 第 3 张网出现的**复核节奏**);其「登记」义务由本条闭合。

**② 断言 ⑦ 的孤儿前置盲区封堵(per metis 指出的盲区)**

- **盲区**:断言 ⑦ 的 owner 活性(③)只对**归档件里带 `⏭(移交)` 标记的 S1-N** 生效;而 design.md D4.4 补登的 **3 项孤儿前置**(`M-4 行为侧` / `moesi_gpu`·`mesif` 未排期 / `verdict 逐 (network, vc)` 标注义务)**不在 S1-N 集合内** → 连「有主」都**无机器保护**(metis:机器闸保证「有主」,不保证「主接得住」;孤儿项连前者都没有)。
- **修法**(断言 ⑦ 新增 ③b / ③c):
  - **③b** 孤儿前置在活文档用**显式标记** `⏭(孤儿)` 登记;其登记行的 change **若已归档 → 报红**(无主债)。
  - **③c** 孤儿前置**必须**在 §4.0 **与** §4.2 **两处都有**带标记的登记行(与 ①②「两处已漂移」同构 —— 只在一处登记 = 另一处是陈旧副本)。
  - **判定用标记而非裸子串**:同一行可能多次提到该项名(如「**M-4 行为侧决策**」+「**⏭(孤儿)** **M-4 行为侧**」)→ 按裸子串判会漏。**这是本条实现过程中被 fixture 抓出的设计缺陷**(首版按裸子串 → 「移除登记行」fixture 恒绿)。
- **登记落点**:3 项孤儿分别落 **队列 #3**(M-4 行为侧决策)/ **队列 #4**(verdict 逐 `(network, vc)` 标注义务)/ **队列 #6**(`moesi_gpu`/`mesif` 未排期),§4.0 与 §4.2 **两处同步**。
- **fixture**:新增 **2 条** —— ① 孤儿行的 owner 改成已归档 change → 必须 fire;② 孤儿登记行被移除 → 必须 fire(自检后 24/24)。

**③ 顺带修掉的 1 项 fixture 前提失效**

原 fixture「把 README 的 `30 成员` 改成 `29 成员` → ③ fire」的锚点随 Entry #56 的「消灭字面量」消失 → 改为**语义等价**的新 fixture(注入错误数字 → ③ fire)+ 增 1 条 GREEN 样本(全部无数字 → ③ 豁免)。

**④ 验收**:`cep_gate_selftest` **24/24** · `cep_docs_sync_check.sh --strict` rc=0 · `cep_freeze_reverse_verify.sh` 闭环成立 · 守门 7 项 rc=0 · `ctest` **101/101** · `[cep]` 77 case / 1415 断言 · `format.sh --check` rc=0 · 冻结面 diff=**0**。

**关联**: Entry #55(队列 #2 归档 + owner 重指向)· Entry #56(决策 B 落地 B1')· metis 对抗性分析(盲区指出)

---

### Entry #58 — 2026-10-25 [scope-change]

**主题**: **队列 #3 `cpptlm-cep-stub-cache-core` 建 change**(planner 阶段)+ **M-4 行为侧决策落盘**(契约化为 delta capability)

**① 形态裁决(2026-10-25,DRI 两问)**

| 问 | 裁决 |
|---|---|
| **change 命名 / KPI slot** | **一个 change** —— 沿用 §4.0 权威名 `cpptlm-cep-stub-cache-core`;**`ADR-CEP-05 §10` 的 `-base` / `-impl` 二分作废**(给该 ADR 追加 Status Update 声明已被 §4.0 重排取代,**§10 正文不改**)。KPI **0/3 → 1/3** |
| **范围切分** | **单一 change 双 Phase** —— Phase 0 = M-4 决策落盘 + 契约化;Phase 1 = stub cache core 实现。**不拆 change**(拆 2 个会破 `ADR-CEP-00 §2 Decision 3` 的 11 change 算术,且 M-4 会在「无真实消费者」的真空里被裁决) |

**② M-4 行为侧决策 —— 6 条裁定(design.md D2 ~ D9)**

| # | 裁定 | 一句话依据 |
|---|------|-----------|
| **D2** | 协议层:credit **不独占** VC | 跨仓权威**已关闭**该问题(ArchForge `32 号 §6.0.1` 排除表 + `protocol_instance.schema.json:131` + 回执 §8.4.3)—— 本仓**不重新裁决** |
| **D3** | 传输层:**保持索引对齐**,**不建**显式映射表 | ① lint 第 10 组①的机器化**前提**就是索引对齐;② 显式映射表归属**队列 #6**(回执 A14-3 + Entry #55),本仓重造即权责冲突;③ `VC_DAT` 桶**零流量**,建表是为零流量桶建表 |
| **D4** | 语义正名:`VC_DAT` = **预留 DAT 桶**(当前恒 0) | 现状注释描述了一个**从未发生的流量** |
| **D5** | 冲突输入:`vc_id` vs `flit_category` —— **传输层不仲裁** | 仲裁需**一致性词表**知识 = adapter 层职责(队列 #6);传输层加校验 = 协议层语义泄漏进物理层(违 `ADR-CEP-12 §8` IC2 ③ 分层纪律) |
| **D6** | 桶名与 accessor **不改名、不弃用** | `VC_DAT` 已进入 `chip_select=(vc<<4)|src` 的**跨模块 wire 编码契约**;`vc_dat_count()` 外部消费者 **0 命中**;`SocInternalNoc` 是**已归档** e1 产物 → 弃用标注会制造「归档件契约 ↔ 现行行为」漂移 |
| **D7** | `fabric.schema.json` 的 `vc_count.maximum=4` —— **不需要动** | 传输层 `VC_COUNT=4` 不变 + 协议层 `vc_id` 上界不变 + 权威侧 NS-R2 已锁 vc_count=4 + D3 已排除一切需扩 VC / 扩类的方案 |

**③ 三条被实测推翻的规划前提(立项时的假设 → 实测结论)**

| # | 立项时的假设 | 实测结论 | 证据 |
|---|------------|---------|------|
| 1 | 「改 M-4 须同步冻结面 `fabric.schema.json` 的 `vc_count.maximum=4`」 | **不成立** —— 冻结面预算从「至少 2 个成员」降为「**1 个**」 | 权威已排除「扩 VC 数」与「扩 `CATEGORY_*` 到 6 类」两条路(回执 §8.4.3 已判不可接受);`VC_COUNT` 与协议层 `vc_id` 上界均不变 |
| 2 | 「lint 第 10 组须迁移」 | **既有 3 条全部不回退** —— ①(`CATEGORY_*` 恰 3 类 + 值域连续)③(`VC_COUNT=4`)**继续绿**;②(幽灵类别)仅**注释措辞**受约束。**新增** ④⑤⑥ 三条 | D3 保持 `CATEGORY_*` 不增删;`cep_docs_sync_check.sh:583-648` |
| 3 | 「既有断言需迁移」 | **迁移面远小于预期** —— `test_soc_internal_noc_data_flow.cc:108` 是**陈旧注释**(该用例**本就不存在** `vc_counts_[2]` 断言,实际写 `CATEGORY_REQUEST` 且只断言 `flits_dispatched()` / `unknown_dst_count()`);`vc_dat_count()` / `vc_snp_count()` 外部 **0 消费者** | 实测 grep;design.md D9 迁移清单 |

**④ 更深的一层发现 —— M-4 的真问题不是「credit 放哪」**

实测:全仓**唯一**写 `CATEGORY_CREDIT` 的点是 `include/framework/bidirectional_port_adapter.hh:235`(`send_credit_impl`),该 flit 经 `resp_out_port_` 发出,**不经** `SocInternalNoc::route_flit()`;`SocInternalNoc` 的全部入向流量只写 `CATEGORY_REQUEST` / `CATEGORY_RESPONSE` ⇒ **`VC_DAT` 桶在所有真实路径中恒为 0**。

⇒ 「`vc_dat_count()` 统计 credit 流量」描述的是一个**从未发生的流量**。M-4 的真问题是「**命名说谎 + 索引对齐的隐式性**」,而非 credit 的桶归属。**这条发现同时推翻了「credit 是否独占 VC 是开放问题」的提法** —— 权威侧已闭(协议层),传输层则是伪命题(零流量)。

**⑤ 冻结面授权(本 entry 即授权记录)**

| 成员 | 改什么 | 授权 |
|------|--------|------|
| `docs/adr/ADR-CEP-05-stub-cache-core-interface.md` | **仅追加** `## Status Update` 段(声明 §10 的 `-base`/`-impl` 二分已被 §4.0 重排取代) | **本 Entry #58** —— `cep_freeze_check.sh --update --authorized-by "Entry #58"`(两步:先 commit 产物 → `--update` → 再 commit 清单) |
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | **仅追加** `## Status Update 2026-10-25` 段(队列 #3 已启动 → 并发 1/3)+ 上一段的「**当前态**」改为「**当时态(已被下一段取代)**」(数值保留,仅状态措辞 —— per Entry #18 的「定义句零改动、状态陈述刷新」先例) | 同上 |

其余 **30** 个冻结成员 **0 diff**(`fabric.schema.json` / `fabric.json` / 2 份 main spec / 其余 `ADR-CEP-*` / `07 号` / 3 个 tools / `experimental/metrics.schema.json`)。

> **为什么 `ADR-CEP-00` 也须动**:KPI baseline 断言② 扫 ADR 的 **Status Update 块**(归档日及之后的块**不豁免**);队列 #2 归档时追加的段落写「**当前态**:并发 0/3」—— 队列 #3 建目录后该值变成**历史快照**,若不改措辞会被判红。处置与队列 #2 **同形**(该 change 启动时同样把上一段的「当前态」改为「当时态」)。

**⑥ 产出物(本 entry 的可寻址清单)**

| 载体 | 路径 |
|------|------|
| improvement 5 段草稿 | `.rddf/improvements/cpptlm-cep-stub-cache-core.md` |
| change 目录 | `openspec/changes/cpptlm-cep-stub-cache-core/`(proposal + design + **2 个** delta capability) |
| delta capability ① | `cep-vc-mapping-contract`(8 Scenario) |
| delta capability ② | `cep-stub-cache-core`(9 Scenario) |

**⑦ 纪律要点**

- **锚点内嵌**:每个 Scenario 末尾带 `> **锚点**:` 行 → 归档合并进 main spec 后**仍保留**,供对账测试读(满足 A-P1-7「Scenario 数 == 锚点数」)。**不**放在 `## 附录` 段 —— openspec 归档时该段**不并入** main spec。
- **Requirement 文本 < 500 字符**(openspec `--strict` 门槛):元信息(红绿语义 / 消费者 / 不可验证条款处置)登记在 `proposal.md` 的「契约元信息」段 —— delta spec **只保留** `## ADDED Requirements` 段内内容。
- **不可验证条款不进 spec**:`ADR-CEP-05 §3` 契约测试 #1(`forward_data` 不可分辨性)与 #4(barrier 不可见)需**真实 ProtocolEngine**(首个实例在队列 #6)→ 以「触发条件 = 队列 #6」登记。**理由**:本仓已发生过「判据长期标『✅ 可自动化』,而该检查器从未实现」的失败模式 → 不可验证的条款写进 spec = **永假的验收条款**。
- **单开发者仓的独立性缺口**:本轮「评审」由 oracle / metis 两个 subagent 承担 —— **不等价于**真人外部评审;该独立性问题已在 `obj §8.1` R10 登记,本 entry 不重复消解。
- **`tasks.md` 未写** —— 属 builder 阶段职责(per `obj §4.7.1` 载体表);planner 只出 proposal / design / specs。

**⑧ 验收**

`openspec validate cpptlm-cep-stub-cache-core --strict` **rc=0** · `cep_docs_sync_check.sh --strict` rc=0 · `cep_doc_ref_check.sh --strict` rc=0 · `cep_freeze_check.sh` rc=0 · `ctest` 全绿 · `format.sh --check` rc=0 · 冻结面 diff = **1**(仅 `ADR-CEP-05` 的 Status Update 追加段)· KPI **1/3**。

**关联**: Entry #55(队列 #2 归档 + 4 裁决 + owner 重指向)· Entry #56(决策 B 落地)· Entry #57(孤儿前置盲区封堵)· oracle + metis 两路独立分析(2026-10-25)· ArchForge `32 号 §6.0.1` / `22-nsa §0.5.4.1`(只读权威)

---

### Entry #59 — 2026-10-25 [stage-gate]

**主题**: 队列 #3 `cpptlm-cep-stub-cache-core` **Phase 0 实施完成**(M-4 传输层 VC 映射契约闭合)

**① 阶段边界**: **Phase 0 ✅ 完成 → Phase 1(Stub Cache Core 实现)未启动**。change 整体状态**仍为 🟡 Proposed**(未归档);KPI **1/3 不变**(Phase 属同一 change,不额外占 slot)。

**② 交付物(6 个文件,均零行为变更)**

| # | 文件 | 改动性质 |
|---|------|---------|
| 1 | `include/tlm/cluster/soc_internal_noc.hh` | **仅注释** —— `VC_DAT` 语义正名(预留 DAT 桶 / 恒 0)+ 冲突输入规则三子项(`计数归属` / `编码归属` / `不仲裁`)。**零逻辑 / 零 API / 零常量改名**(diff 的非注释行 = **0**) |
| 2 | `test/test_soc_internal_noc_data_flow.cc` | **仅注释** —— 修正陈旧注释(该用例实际写 `CATEGORY_REQUEST` 且只断言 `flits_dispatched()` / `unknown_dst_count()`;**断言集合 0 变化**) |
| 3 | `test/test_cep_vc_mapping_contract.cc`(新增) | **5 cases / 34 assertions** —— 值域连续性(8 条 `static_assert`)/ 三桶计数随输入改变 / **正控**(注入 `CATEGORY_CREDIT` → `vc_dat_count()` 0→1)/ 冲突输入归属 / 传输层不仲裁(不报错·不纠正·不丢弃) |
| 4 | `scripts/test/cep_docs_sync_check.sh` | 第 10 组**新增**断言 **④⑤⑥**(BEGIN/END 标记围出的独立 python 块)+ 第 11 组补 BEGIN/END 标记。**既有 ①②③ 零迁移** |
| 5 | `scripts/test/cep_gate_harness.py` | 支持 `--group 10` / `--group 11`(按标记抽取);`FILES` 新增 3 个输入文件 |
| 6 | `test/CMakeLists.txt` | 注册 ctest 用例 `cep_vc_mapping_contract`(标签 `[cep][vc-mapping-contract]`) |

**③ 新增 lint 断言 ④⑤⑥ 的判据形态(逐条,守门须与它守护的语义等价)**

| 断言 | 判据 | 反向 fixture(变异必红) |
|------|------|----------------------|
| **④** | 禁止正则 `credit[^*]{0,6}流量`(把 `VC_DAT` 说成 credit 流量计数器)+ **要求**存在注释块同时含 `VC_DAT` 与「预留」+ **要求**「恒 0」声明 | ① 改回「本桶统计 credit 流量」→ fire ✅ ② 删除「预留」→ fire ✅ |
| **⑤** | **注释结构**:`计数归属` / `编码归属` / `不仲裁` 三者同时在场(**取带 markdown 强调的完整形态** —— 裸串「不仲裁」可能只是行文用词)+ **代码等价**:解析 `route_flit()` 函数体(大括号配平),链式核对 `cat = flit.flit_category.read()` → vc 变量由 `cat` 派生 → `++vc_counts_[vc]` | ① 删子项标记 → fire ✅ ② **把计数下标改成源自 `vc_id`** → fire ✅ |
| **⑥** | `test/test_soc_internal_noc_data_flow.cc` 不得出现 `vc_counts_`(私有成员名 —— 该测试的注释不得传播内部实现细节,旧注释正因此把桶指向写错) | 写回旧注释 → fire ✅ |

> **⑤ 的代码等价部分是关键**:仅查注释等于没查(注释可以说谎)—— 所以断言 ⑤ 同时核对**代码里计数下标的实际来源**。

**④ 三条被实测推翻的立项前提(收官确认)**

| # | 立项时的假设 | 实测结论 |
|---|------------|---------|
| 1 | 「改 M-4 须同步 `fabric.schema.json` 的 `vc_count.maximum=4`」 | **不成立** —— 冻结面 diff = **0**(实测 32 成员工作区 0 改动);`vc_count.maximum` 仍为 **4** |
| 2 | 「lint 第 10 组须迁移」 | **既有 ①②③ 全部不回退**(实测逐条绿);仅**新增** ④⑤⑥ |
| 3 | 「既有断言需迁移」 | **迁移面 = 0 条断言**(仅 1 处注释文字;`vc_dat_count()` 外部消费者 0 命中) |

**⑤ 验收(全绿,实测)**

| 项 | 实测 |
|---|---|
| `openspec validate cpptlm-cep-stub-cache-core --strict` | **rc=0** |
| `openspec validate --changes --strict` | **rc=0** |
| `cep_docs_sync_check.sh --strict` | **rc=0**(第 10 组 4 项全绿:① 3 类连续 / ② 无幽灵类别 / ③ `VC_COUNT=4` / ④⑤⑥ 通过) |
| `cep_doc_ref_check.sh --strict` · `cep_freeze_check.sh` · `cep_archforge_alignment_check.sh` · `cep_decl_alignment_check.sh` · `docs_sync_check.sh` · `test_sh_arg_parsing.sh` | 全 **rc=0** |
| `cep_gate_selftest.sh` | **31 / 31**(含**新增 7 条**第 10 组 fixture —— 6 条必红 + 1 条防误报必绿) |
| `ctest --test-dir build -j4` | **102 / 102**(新增 1 个;Phase 0 前为 101) |
| `format.sh --check` · `markdown_link_check.sh` | 全 **rc=0** |
| 冻结面 | 32 成员**本次 0 改动**(`authorized_by = Entry #58`;planner 阶段的 2 成员改动已单独授权) |
| **反向验证自查** | 3 个 C++ 断言逐条变异 → 用例**全部变红**(正控 `vc_dat_count()==1→==0` ❌ · 冲突 `vc_req_count()==1→==0` ❌ · 不丢弃 `received==1→==0` ❌),还原后全绿 |

**⑥ 过程中修掉的三个自身缺陷(第 3 条由实施后对抗式自审发现)**

- **harness 抽取歧义**:第 10 组新增块后,第 11 组的「脚本里唯一的 `python3 - "$PROJECT_ROOT" << 'PYEOF'`」定位假设**失效**(实测导致第 11 组既有 fixture **全部假红**)→ 改为**两组各用 BEGIN/END 标记**定位;标记缺失 / 块被掏空 / 块为空 **均显式 `SystemExit` 响亮失败**(实测退出码 ≠ 0,不静默绿)。
- **fixture 依赖裸串**:断言 ⑤ 原检查裸串「不仲裁」,而该词在注释里出现 **2 次**(小节标题 + 子项行)→ 替换子项后标题仍命中 → **fixture 恒绿**。改为取**带 markdown 强调的完整形态**,并让 fixture 替换**全部**匹配。
- **断言 ⑤ 对等价改写误报**(对抗式自审探针 ③ 发现):原判据要求 `const unsigned <vc> = …`,故等价改写 `auto vc = …` 会**误判红**(判据⑥「误报与漏报同为缺陷」)→ 改为「**候选 + 验证**」形态:先取函数体内所有「`X = … <cat> …`」的 X(声明前缀不限),再验证其中至少一个被用作 `++vc_counts_[X]` 的下标。**并补 GREEN fixture**(「等价改写 `auto vc` → 必须不 fire」)锁住该形态。
  - **实测判别力**:① 计数下标改 `vc_id` → fire ✅ ② 硬编码 `vc_counts_[VC_REQ]` → fire ✅ ③ 等价改写 `auto vc` → **不 fire** ✅

**⑦ 纪律要点**

- **零行为变更**:`include/` + `src/` 的 diff **非注释行 = 0**(实测);`cpptlm_core` 与 23 ABI 零改动。
- **新断言的回归 fixture** 走 `--group 10`(harness 按标记抽块、在真拷贝临时根上跑),与第 11 组 fixture 同机制。
- **不可验证条款不进 spec**:`ADR-CEP-05 §3` 契约测试 #1/#4 的降级登记在 Phase 1(T2.6)。
- **单开发者仓的独立性缺口**:本轮实施与自检由同一 agent 完成 —— **不等价于**真人外部评审;缺口已在 `obj §8.1` R10 登记。

**⑧ 下一步**:Phase 1(Stub Cache Core 实现)—— `tasks.md` 的 T2.1 ~ T2.8 待启动。

**关联**: Entry #58(队列 #3 建 change + M-4 决策落盘)· Entry #55(队列 #2 归档 + owner 重指向)· oracle + metis 两路独立分析(2026-10-25)

---

### Entry #60 — 2026-10-25 [stage-gate]

**主题**: 队列 #3 `cpptlm-cep-stub-cache-core` **Phase 1 实施完成**(Stub Cache Core 实现)—— **change 全部任务完成,待归档**

**① 阶段边界**: **Phase 0 + Phase 1 均 ✅ 完成** ⇒ `tasks.md` **35 / 36** 勾选(仅剩 T3.8 归档回填)。change 状态**仍为 🟡 Proposed** —— **尚未归档**,KPI **1/3 不变**(归档后释放 slot)。

**② 交付物(4 个文件 + 1 个注册)**

| # | 文件 | 改动性质 |
|---|------|---------|
| 1 | `include/tlm/cep/stub_cache_core.hh`(**新增**,header-only) | Stub Cache Core 实现:**6 接口**(`request` / `completion` / `snoop` / `snoop_resp` / `writeback` / `evict_line`)+ **5 态**(`NONE/I/S/E/M`)+ **6 条 `INV-SC*`** 的行为实现。**不注册 23 ABI、不进 `ModuleFactory`、不改 frozen header** |
| 2 | `test/test_cep_stub_cache_core.cc`(**新增**) | **14 cases / 204 assertions** —— 9 个 Scenario 逐条对应 |
| 3 | `test/test_cep_scenario_anchor_reconciliation.cc`(**新增**) | **4 cases / 25 assertions** —— A-P1-7 的「Scenario ↔ 锚点」对账(数量等值 + 锚点指向的文件 / ctest 用例真实存在) |
| 4 | `test/CMakeLists.txt` | 注册 2 个 ctest:`cep_stub_cache_core` / `cep_scenario_anchor_reconciliation` |
| 5 | `openspec/.../tasks.md` | Phase 1 段勾选 + 验收段补 T3.6 / T3.7 |

**③ 设计要点(与 `ADR-CEP-05` 的对应关系)**

| 项 | 实现 | 依据 |
|---|------|------|
| **6 接口** | net-side 5 条 CohIf RN 投影 + **1** 条 core-side 原语 `evict_line` | 决策 1 的 6 行表(命名**逐字对齐** `ADR-CEP-02 §2` canonical 10 流) |
| **方向语义** | `request` / `snoop_resp` / `writeback` = 本 stub **发起**;`completion` / `snoop` = 本 stub **消费** | canonical 10 流的方向列 |
| **5 态** | `NONE → I → S → E → M`(协议无关);**不**暴露 CHI RN-F 细节态 | §4.1 + `INV-CEP-05-IC1` |
| **perm** | **4 值锁**(`NONE/READ/UNIQUE/WRITEBACK`);atomic 意图不扩 perm | `ADR-CEP-02` CEN-Ib11 |
| **折叠映射** | 5 隐藏流**全部不作为接口标识符**;流 7(反向量)以普通 `INVALIDATE` 表现 | 决策 3 |
| **`INV-SC1`** | MSHR 以 `addr` 为键(`map` ⇒ 同 addr 至多 1)+ 容量上限 → 重复请求**背压**(计数 `mshr_rejects`) | §4.3 |
| **`INV-SC2`** | `evict_line` 对 outstanding addr **跳过**(计数 `evict_skipped`),line 与状态**保留** | §4.3 |
| **`INV-SC3`** | snoop 响应在**参数化固定延迟**(`Config::snoop_latency_cycles`)后可得;**逐拍**核对 | §4.3 |
| **`INV-SC4`** | `completion` 的 `(addr, txn_id)` **双重配对**;不配对即拒(计数 `unsolicited_completions`) | §4.3 |
| **`INV-SC5`** | `WritebackOut::carries_data == dirty`(**结构编码**);clean 写回 data 置 0 | §4.3 |
| **`INV-SC6`** | hit 路径**直接返回**,不触碰任何 net-side 动作 | §4.3 |

**④ 与 `ADR-CEP-05 §3` 契约测试 #1 / #4 的降级(T2.6)**

两者需**真实 ProtocolEngine**(仓内首个协议实例在队列 #6)⇒ **不进**本 change 的 spec,以「触发条件 = 队列 #6」登记于 `design.md` D10 + Open Questions Q2。

⚠️ **本仓的「不出现」检查是必要条件、非充分条件** —— 头文件里**不得**出现这两个流名(静态核对),但「stub 无从区分 forward 与 memory 完成」这一**真实性质**需 ProtocolEngine 才能判定。spec 的 Scenario 7 的措辞据此设计为「**没有**一条 Scenario 把 #1/#4 立为要求」,而非「spec 不含这些词」—— 后者会**误报**(排除语境本身就合法使用这些词,判据⑥:误报与漏报同为缺陷)。

**⑤ 反向验证自查(T3.6)—— 9 / 9 全部报红**

| # | 变异 | 结果 |
|---|------|------|
| 1 | `INV-SC1`:同 addr 允许重复 outstanding | ❌ 4 assertions failed |
| 2 | `INV-SC2`:evict 允许驱逐 outstanding line | ❌ 5 failed |
| 3 | `INV-SC3`:snoop 响应立即可得(忽略延迟) | ❌ 6 failed |
| 4 | `INV-SC4`:接受 unsolicited completion | ❌ 7 failed |
| 5 | `INV-SC5`:clean 写回也带 data | ❌ 2 failed |
| 6 | `INV-SC6`:hit 路径也发 request | ❌ 3 failed |
| 7 | 折叠映射违规:接口面出现 `barrier` | ❌ 1 failed |
| 8 | CHI 态违规:枚举出现 `SD` | ❌ 1 failed |
| 9 | 集成面违规:代码面出现 `REGISTER_MODULE` | ❌ 1 failed |

**⑥ 过程中修掉的自身缺陷(4 个)**

- **头文件笔误 2 处**:初版 `StubCounters` 引用了未定义的 `mshr_rejects` / `evict_skipped`,且 `request()` 残留哨兵逻辑 → 重写。
- **死计数器**:初版 `StubCounters` 含 4 个「**不可能触发**的违例计数」(`mshr_overcommit` / `evict_of_outstanding` / `snoop_resp_late_or_early` / `writeback_payload_violations`)—— 恒 0 的计数器 = **假覆盖**(判据①)→ 删除,改为只收「**保护性拒绝**」与「**实际动作**」两类计数;`INV-SC3` / `SC5` / `SC6` 改为**纯行为断言**。
- **测试逻辑 bug 3 处**:① `INV-SC3` 的延迟循环对 `lat=0` 误判(改为**逐拍**核对);② `INV-SC4` 的绝对计数随前置用例漂移(改为**增量**断言);③ Scenario 7 断言「spec 不含 `barrier`」**误报**(排除语境合法)→ 改为「无 Scenario 把 #1/#4 立为要求」。
- **GCC 编译错**:`explicit StubCacheCore(Config cfg = {})` 在「嵌套类 + 默认成员初始化器 + 默认实参」组合下触发 `could not convert '<brace-enclosed initializer list>'` → 改为显式两个构造函数。

**⑦ 验收(全绿,实测)**

| 项 | 实测 |
|---|---|
| `openspec validate cpptlm-cep-stub-cache-core --strict` · `--changes --strict` | **rc=0** |
| 守门 7 项(`cep_docs_sync_check` / `cep_doc_ref_check` / `cep_freeze_check` / `cep_archforge_alignment_check` / `cep_decl_alignment_check` / `docs_sync_check` / `test_sh_arg_parsing`) | 全 **rc=0** |
| `cep_gate_selftest.sh` | **31 / 31** |
| `ctest --test-dir build -j4` | **104 / 104**(新增 2 个:Phase 1 前为 102) |
| `format.sh --check` · `markdown_link_check.sh` | 全 **rc=0** |
| 冻结面 32 成员 | **0 改动**(`authorized_by = Entry #58`) |
| 23 ABI · `ModuleFactory` | **零改动 / 零注册** |

**⑧ 剩余与下一步**

- **T3.8 归档回填** —— 按 `obj §4.7.3` 的 **17 个落点**逐项回填;delta capability 合并进 main spec 后按 Entry #56 先例纳入冻结集(走 `cep_freeze_check.sh --update --authorized-by`)。
- **⚠️ 归档语义的二选一(需用户裁决)**:
  - **✅ 已完成归档**(`openspec archive`):实现已落地,契约已在系统内 ⇒ delta spec **合并**进 `openspec/specs/`(新增 2 个 capability:`cep-vc-mapping-contract` / `cep-stub-cache-core`)。**语义**:断言该能力**已在契约内**。
  - **⏸ 暂存归档**(`openspec archive --skip-specs`):**不合并** main specs + proposal 顶部加 `⏸ 暂存归档` 段。**语义**:断言「设计保留,能力未落地」。
  - ⇒ 本 change 的**实现已落地**(非 0%),按 AGENTS.md 的判据应走 **✅ 已完成归档**。
- **下一队列 = #4** `cpptlm-cep-directory-ordering-plane`(2027-Q3)。

**关联**: Entry #59(Phase 0 实施完成)· Entry #58(队列 #3 建 change + M-4 决策落盘)· Entry #55(队列 #2 归档 + owner 重指向)· `ADR-CEP-05`(接口 / 状态机 / `INV-SC*` 定义)· `ADR-CEP-02 §2`(canonical 10 流)

---

### Entry #61 — 2026-10-25 [archive]

**主题**: 队列 #3 `cpptlm-cep-stub-cache-core` **✅ 已完成归档** —— 17 个落点回填 + 冻结集扩容

**① 归档动作**

| 项 | 内容 |
|---|---|
| **命令** | `openspec archive cpptlm-cep-stub-cache-core --yes`(2026-10-25) |
| **归档路径** | `openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core/` |
| **语义** | **✅ 已完成归档**(**非** ⏸ 暂存)—— 判据:实现已落地(tasks **36/36** 全勾、Phase 0 + Phase 1 均完成)⇒ 契约**已在系统内** ⇒ delta spec **合并**进 `openspec/specs/` |
| **合并产物** | `openspec/specs/cep-vc-mapping-contract/spec.md`(**8 Scenario** + 8 锚点)· `openspec/specs/cep-stub-cache-core/spec.md`(**9 Scenario** + 9 锚点)—— 锚点**内嵌**在各自 Scenario 末尾,合并后**仍保留**(供对账测试读) |
| **KPI** | **1/3 → 0/3**(slot 已释放);下一个可启动 = 队列 #4 `cpptlm-cep-directory-ordering-plane` |

**② 17 个落点回填(per `obj §4.7.3` 收尾同步清单)**

| 组 | # | 落点 | 状态 |
|---|----|------|------|
| **A**(change 自身) | A1 | `tasks.md` | ✅ **36/36** 全勾(T3.8 在归档动作中同步勾选) |
| | A2 | `proposal.md` 状态行 | ✅ 改为「✅ 已完成并归档(完成归档语义)」+ tasks 计数 + 归档路径 |
| | A3 | `design.md` 去处说明 | ✅ 无内容迁出 ⇒ 无需留去处 |
| **B**(objective) | B1 | `obj §4.0` 队列 #3 行 | ✅ 状态列 → **✅ archived** |
| | B2 | `obj §4.2` 队列 #3 行 | ✅ 同上 |
| | B3 | `obj §5.2` KPI | ✅ **1/3 → 0/3** |
| | B4 | `obj §8.1` 风险表 | ✅ 本次**未**触发任何风险行变更(M-4 闭合、stub 落地均无新风险) |
| | B5 | `obj §9.x` 处置段 | ✅ 本次**未**改动既有判据 ⇒ 无需追加 |
| | B6 | `obj` 头部同步注 | ✅ 状态行已随之更新(front-matter `last_revised`) |
| | B7 | `obj §11` 台账 | ✅ **本 entry** |
| **C**(路线图) | C1 | `phase-10.md` 子任务行 + KPI baseline | ✅ 两处同改(依赖图 + `:132` KPI 行) |
| | C2 | `.rddf/roadmap.md` | ✅ KPI baseline 行 + 顶部「最后更新」 |
| | C3 | `.rddf/improvements/cpptlm-cep-stub-cache-core.md` | ✅ front-matter `状态: Archived` + 「最后」行 + 全篇改写为归档态 |
| **D**(仓级/门禁) | D1 | `AGENTS.md` 全仓 KPI 基线行 | ✅ **0/3** |
| | D2 | **门禁脚本的路径绑定** | ✅ `cep_freeze_check.sh` 的 `FROZEN_GLOBS` 加 2 条新 main spec;`cep_gate_selftest.sh` 的「§4.0 含活队列行」断言由队列 #3 改为**队列 #4** |
| | D3 | `docs/implementation/` 快照 | ✅ 本 entry 即时点快照(未另建文件 —— 内容与本仓既有「台账承载实施证据」惯例一致) |
| | D4 | **相关 ADR 的 Status Update** | ✅ `ADR-CEP-00` 追加「队列 #3 已完成归档」段 + 上一段的「当前态」改「**当时态**」;`ADR-CEP-05 §14` 的**既有**段保留(其 `-base`/`-impl` 二分作废声明仍有效) |

**③ 冻结面授权(本 entry 即授权记录)**

| 成员 | 改什么 | 授权 |
|------|--------|------|
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | **仅追加** Status Update 2026-10-25(队列 #3 归档)+ 上一段「当前态」→「当时态」(**数值保留**,仅状态措辞) | **本 Entry #61** —— `cep_freeze_check.sh --update --authorized-by "Entry #61"` |
| `openspec/specs/cep-vc-mapping-contract/spec.md` | **新增**(归档合并产物)⇒ 纳入冻结集(Entry #56 先例) | 同上 |
| `openspec/specs/cep-stub-cache-core/spec.md` | **新增**(归档合并产物)⇒ 纳入冻结集(Entry #56 先例) | 同上 |

冻结成员 **32 → 34**。

> ⚠️ **为什么必须纳入冻结**:这 2 个 main spec 是**系统契约面** —— 若可被静默改写,则 M-4 契约与 stub cache 契约的**判据本身**可被改(退化的正是「防无意引入的退化」这条目标本身)。per Entry #56 先例:`fabric.json` 因被 ctest 消费而纳入冻结,同理。

**④ 归档期修掉的 3 处「无主债 / 路径漂移」(机器闸报红暴露)**

| # | 报红 | 根因 | 处置 |
|---|------|------|------|
| 1 | 断言 ⑦:「S1-2 / S1-3 / S1-4: 归档件标为『移交』,但 obj §4.0 排队表**未提及**」 | 队列 #2 归档时把这 3 项的 owner 指给**队列 #3**;队列 #3 归档后其 §4.0 行被改写 → 3 项在活文档**失主** | **owner 重指向队列 #4**(§4.0 + §4.2 两处同改):S1-2「首个 `implemented` 实例装配后复核」⛔ 未触发 · S1-3「第 3 张网 / 第 3 个协议实例出现时 SLICC 备选重评」⛔ 未触发 · S1-4 ✅ 已消费(Entry #56)仅余复核口径 |
| 2 | 断言 ⑦③b:「孤儿前置行(`⏭(孤儿)`)的 change `cpptlm-cep-stub-cache-core` **已归档** → 无主债」 | 孤儿项「M-4 行为侧」的登记行在**队列 #3 行**上 | 该行的**登记**移至**队列 #4 行**(§4.0 + §4.2 两处)并保留 `⏭(孤儿)` 标记 —— 断言 ⑦③c 要求孤儿项在两处都有带标记的登记行(**「已消费」不等于「可从活文档消失」**) |
| 3 | `cep_gate_selftest` 的「§4.0 含**活队列 #3** 的 change 名」断言 | 队列 #3 已归档 ⇒ 该 fixture 的锚点漂移 | 改锚点为**队列 #4**(`cpptlm-cep-directory-ordering-plane`) |

> **教训沉淀**:归档的 17 落点清单**不含**「门禁脚本里的**活队列断言**」这一类 —— 它归 D2「门禁脚本的路径绑定」,但该条目读起来像「路径随迁」,而实际是**断言的锚点实体**随归档而失效。**清单按「文件」列,漏了「文件内的活实体引用」** —— 建议后续把 D2 拆为「路径绑定」与「活实体锚点」两条。

**⑤ 验收(全绿,实测)**

| 项 | 实测 |
|---|---|
| `openspec validate --changes --strict` | **rc=0** |
| `openspec list --json` 实测 active | **0**(KPI **0/3**) |
| 守门 7 项 | 全 **rc=0** |
| `cep_gate_selftest.sh` | **31 / 31** |
| `ctest --test-dir build -j4` | **104 / 104** |
| `format.sh --check` · `markdown_link_check.sh` | 全 **rc=0** |
| 冻结面 | **34 成员**(`authorized_by = Entry #61`) |

**⑥ 下一步**:队列 #4 `cpptlm-cep-directory-ordering-plane`(2027-Q3)—— 承接 **S1-1 ②建图层** + **verdict 逐 `(network, vc)` 标注义务** + **S1-2 / S1-3** 触发条件待命 + **M-4 行为侧**(已由队列 #3 闭合,登记行保留以免孤儿项失主)。

**关联**: Entry #60(Phase 1 实施完成)· Entry #59(Phase 0 实施完成)· Entry #58(队列 #3 建 change + M-4 决策落盘)· Entry #56(冻结集扩容先例:`fabric.json` 纳入)· Entry #55(队列 #2 归档 + owner 重指向先例)· `ADR-CEP-05`(接口 / 状态机 / `INV-SC*` 定义)

---

### Entry #62 — 2026-10-25 [scope-change]

**主题**: **队列 #4 `cpptlm-cep-directory-ordering-plane` 建 change**(planner 阶段)+ 4 项 DRI 裁决落地

**① 形态裁决(2026-10-25,DRI 四问,oracle + metis 两路独立同意)**

| 问 | 裁决 |
|---|---|
| **落地范围** | **切小**:SharerTrackerIf 单接口(4 方法) + caps descriptor(sizeof=8 bitmask,第一天就位) + D-5 全量契约 + **FullMapDirectory 唯一行为实现**;其余 4 种实现跟随各协议族队列 #6~#9 —— 4 个无消费者实现提前落码 = 队列 #2/#3 双实证的失败模式(per Entry #55 ③ 之「M-4 越早定越好」同型警示反向化) |
| **装配闸的输入源** | **新建 `include/tlm/cep/protocol_instances.json`**(本仓机器可读实例声明,诞生**不冻结**,per Entry #56 先例);ArchForge `32 号 §6.1/§6.2` 为单点权威,**本仓只持派生副本**(与 `fabric.json` 同形) |
| **闸的挂点** | **双挂**(`module_factory.cc::instantiateAll` 守卫钩子 + `scripts/CMakeLists.txt::validate_topology` target),**共享同一判据实现**(`channel_dependency_graph.hh`),**禁双实现**(per metis 建议 #8 / 纪律 ⑤「守卫须与它守护的命令等价」) |
| **S1-7 (β) 同批约束** | **同批补入** `INV-CEP-08-IC2`(**E 态静默升级**组合必查项,对 `moesi_gpu` 实跑)—— per `ADR-CEP-05 §13.1` 同批要求(`ADR-CEP-08 决策 4` 组合检查器) |

**② 产出物清单**

| 载体 | 路径 |
|---|---|
| improvement 5 段草稿 | `.rddf/improvements/cpptlm-cep-directory-ordering-plane.md` |
| change 目录 | `openspec/changes/cpptlm-cep-directory-ordering-plane/`(proposal + design + **3 个** delta capability) |
| delta capability ① | `cep-sharer-tracker-base`(**9 Scenario** / 9 锚点) |
| delta capability ② | `cep-ordering-point-checker`(**6** Scenario / 锚点) |
| delta capability ③ | `cep-channel-dependency-gate`(**7** Scenario / 锚点) |
| **合计** | **22 Scenario / 22 锚点等值**(`openspec validate --strict` rc=0) |
| protocol_instances.json | `include/tlm/cep/protocol_instances.json`(**派生副本,首条声明 `moesi_gpu status=planned`,8 顶点与 S3 spike 同构**) |

**③ 冻结面授权(本 entry 即授权记录)**

| 成员 | 改什么 | 授权 |
|------|--------|------|
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | **仅追加** `Status Update 2026-10-25`(队列 #4 启动)+ 上一段「当前态」→「当时态」 | **本 Entry #62** —— `cep_freeze_check.sh --update --authorized-by "Entry #62"` 两步流程 |
| `docs/adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md` | **仅追加** `§10 Status Update`(声明 §8 四个 change 名`cpptlm-cep-sharer-tracker-base/-impls/-cxl-dcoh/-tilelink-tree` 已失指,被 obj §4.0 重排取代;**§8 正文零改动**) | 同上 |
| `docs/architecture/07-coherence-exploration-platform.md` | **仅追加** `§附录 Status Update 2026-10-25`(SharerTracker 行 change 引用修正 + ADR-CEP-04 §8 失指授权记录) | 同上 |

其余 **31** 个冻结成员 **0 diff**(`fabic.json` / `fabric.schema.json` / 2 份既有 main spec / 其余 ADR-CEP 家族成员 / tools)。

**④ KPI 状态**

- baseline: **0/3 → 1/3**(队列 #4 建 change 占用 1 slot)
- `openspec list --json` 实测:`1 个 active` = `cpptlm-cep-directory-ordering-plane`

**⑤ 五处同步(活文档 KPI 基线改 0/3 → 1/3)**

`obj §5.2` · `.rddf/roadmap.md` · `phase-10.md` · `AGENTS.md` · `improvements/cep-umbrella.md` —— **6 处**(含 header 头部)。A-1-2 等锚点在 phase-10 依赖图同步入。

**⑥ 纪律要点与对策**

- **22 Scenario 不 overclaim**:A-1-9 / A-2-6 是显式降级条款(非 FullMap 实现声明层断言、J3 通用形式**不可静态判定**),per 队列 #2 先例
- **装配闸 SKIP 形态不含糊**:per metis blocker,**`protocol_instances.json` 缺失/为空 ⇒ 显式 `[SKIP]`**(与 PASS 机器可区分)
- **verdict 逐 (network, vc)**:per `ADR-CEP-09 §8.3`(不全局硬编码),格式 `[verdict] net=<network> vc=<id> PASS|FAIL` 机器可 grep
- **对账测试**(锚点数 == Scenario 数,per 队列 #3 A-P1-7 先例):每条 Scenario 都点名锚点(3 个新测试文件),锚点数 **22 == Scenario 数 22**
- **`tasks.md` 未写**:属 builder 阶段职责(planner 只出 proposal/design/specs,per `obj §4.7.1`)
- **阶段承载**:**Schedule Say After Proof**(Schedule Say After Proof энергии)以**机检器 first**:`openspec validate --strict` rc=0 ⇒ 守门 7 项 rc=0 ⇒ selftest 31/31 ⇒ `ctest` 全绿
- **单开发者仓的独立性缺口**:oracle + metis subagent 承担本轮对抗式评审,不等价于真人外部评审(独立性问题在 `obj §8.1` R10 登记)

**⑦ 验收(全绿,实测)**

`openspec validate cpptlm-cep-directory-ordering-plane --strict` rc=0 · `openspec validate --changes --strict` rc=0 · `cep_doc_ref_check.sh --strict` rc=0 · `cep_archforge_alignment_check.sh` rc=0 · `cep_decl_alignment_check.sh` rc=0 · `docs_sync_check.sh` rc=0 · `test_sh_arg_parsing.sh` rc=0 · `format.sh --check` rc=0 · `markdown_link_check.sh` rc=0 · **`cep_docs_sync_check.sh` / `cep_freeze_check.sh` 待与 ADR 3 成员走两步授权(`--update --authorized-by "Entry #62"`)后即 rc=0** · KPI **1/3**

**关联**: Entry #61(队列 #3 完成 + 冻结面 34 成员)· Entry #60(Phase 1)· Entry #59(Phase 0)· Entry #58(队列 #3 建)· Entry #55(S1 owner 重指向先例)· Entry #56(`fabric.json` 纳冻结集 + `protocol_instances.json` 同构先例)· ArchForge `32 号 §6.1/§6.2` / `22-nsa §0.5.4.1`(只读权威)

---

### Entry #63 — 2026-10-25 [builder]

**主题**: 队列 #4 `cpptlm-cep-directory-ordering-plane` **builder 阶段实施完成**(22 Scenario / 22 锚点全落地)

**① 产出物**

| 类型 | 路径 | 规模 |
|---|---|---|
| 头文件 | `include/tlm/cep/sharer_tracker.hh` | 单接口(4 虚方法)+ caps(4 属性)+ behavior(2 属性)+ 5×6 traits 表 + IC3 五事件 + `FullMapDirectory`(唯一行为实现) |
| 头文件 | `include/tlm/cep/address_resolver.hh` | 单方法接口 + `WindowAddressResolver` |
| 头文件 | `include/tlm/cep/channel_dependency_graph.hh` | 三元组顶点 + DFS 三色环检测 + 声明解析 + `GateReport`(`per_instance` 逐实例统计) |
| 头文件 | `include/tlm/cep/ordering_point_checker.hh` | J1/J2/J3(**vacuous**)+ `INV-CEP-08-IC2` + `kS1_7BetaVerdict` |
| 声明数据 | `include/tlm/cep/protocol_instances.json` | **13 字段 × 2 实例**(`moesi_gpu` + `mesif`)+ `channel_dependency_edges`(8 条/网表) |
| 测试 | `test/test_cep_sharer_tracker_base.cc` | 9 cases / 138 assertions |
| 测试 | `test/test_cep_ordering_point_checker.cc` | 6 cases / 64 assertions |
| 测试 | `test/test_cep_channel_dependency_gate.cc` | 7 cases / 75 assertions |
| 挂点 | `src/core/module_factory.cc::instantiateAll()` | `validateConfig` 之后 / 实例创建之前;仅 `cep` 段存在时触发;FAIL ⇒ 返回 false |
| 挂点 | `examples/cep_channel_dependency_gate.cc` + `examples/CMakeLists.txt` + `scripts/CMakeLists.txt` | CLI 注册 + `validate_topology` 追加 |
| 守门增强 | `scripts/test/cep_decl_alignment_check.sh` | 新增**第 ⑥ 项**:本仓派生副本 ↔ ArchForge 声明**逐字段**相等 + 边端点已绑定 |

**② 实施期偏差(5 项,已在 `design.md §实施期裁决 D-ι` 逐条登记)**

1. caps 4 属性 = `{scope, completeness, authority, aggregation_at}`(planner 写错源);`hazard`/`broadcast_fallback` 落 `SharerTrackerBehavior`
2. `completeness == PARTIAL` 不可由合法声明表达(schema enum 无 PARTIAL)⇒ J2 的判别力由**合成声明**承担,报告标 `vacuous`
3. 边表不可从 `vc_binding` 推导 ⇒ 作为声明数据(`channel_dependency_edges`,ArchForge `32 号 §6.0.3` 派生副本)
4. 新增第 4 个 header `ordering_point_checker.hh`(三子平面正交,避免与图算法混居)
5. 镜像补齐 **2** 个实例(`moesi_gpu` + `mesif`),planner 只写了 1 个

**③ S1-7 (β) 裁决(本 change 的交付物之一)**

> **不新增第 5 判别位**。`UNIQUE` 已编码 E 态可写;静默升级的**可组合性**由 `combination_capabilities.hierarchical` 承载(冻结 schema 已有)。扩 `perm` 会破 `ADR-CEP-02 CEN-Ib11` 的 4 值锁且不增判别力。
> 落地:`kS1_7BetaVerdict` + `design.md D-ι-4`;不改 `cep-stub-cache-core.spec.md` / 不落 schema 演进(归队列 #6)。

**④ 验收(实测)**

- `ctest` **105/105**(+1 = `cep_channel_dependency_gate` CLI)
- `[cep]` **122 cases / 2043 assertions**(较队列 #3 归档后 100/1730 增 22 cases / 313 assertions)
- 守门 7 项 **全 rc=0**(含新增第 ⑥ 项;并做了反向验证:改镜像 → ⑥ 报红,还原 → 绿)
- `cep_gate_selftest` 全绿 · `format --check` rc=0 · `markdown_link_check` rc=0 · `openspec validate --changes --strict` rc=0
- 冻结集**仍 34 成员**(本轮零冻结面改动,无需 `--update`)
- KPI **1/3**(本 change 仍 active)

**⑤ 遗留发现(不在本 change 范围)**

`validate_topology` target 的**首条**命令引用了不存在的 `build/configs/dgpu_board_v1_mvp.json`(`scripts/CMakeLists.txt:48`,源仓亦无该文件)⇒ 该 target **在本 change 之前**就无法跑完,本 change 追加的 CLI 命令因此跑不到。验证替代:CLI 单独运行 PASS + `ninja -C build -t commands validate_topology` 确认已接线。修复属另一子系统,需单独决策。

**关联**:Entry #62(planner 建目录 + 4 项裁决)· `ADR-CEP-04 §2 决策 3 / §3 / §4` · `ADR-CEP-08 §2 决策 3/4/7` · `ADR-CEP-09 §8` · ArchForge `32 号 §6.0.2/§6.0.3/§6.1/§6.2`

---

### Entry #64 — 2026-10-25 [archive]

**主题**: 队列 #4 `cpptlm-cep-directory-ordering-plane` **完成归档**(22 Scenario 并入 3 份 main spec;KPI 1/3 → 0/3;冻结集 34 → 37)

**① 归档动作(语义 = ✅ 已完成归档)**

| 项 | 值 |
|---|---|
| 命令 | `openspec archive --yes cpptlm-cep-directory-ordering-plane` |
| archive 名 | `2026-10-25-cpptlm-cep-directory-ordering-plane` |
| delta → main | `cep-sharer-tracker-base`(9 Scenario)· `cep-ordering-point-checker`(6)· `cep-channel-dependency-gate`(7)→ 各 1 Requirement / 合计 **22 Scenario** |
| tasks | **53 / 53** 全勾(含 A-FIN-4 归档治理 6 步) |
| KPI | **1/3 → 0/3**(`openspec list --json` 实测 0 个 active) |

**② 归档日治理 6 步(逐条落地)**

| # | 步骤 | 落地 | 验证 |
|---|---|---|---|
| 1 | `FROZEN_GLOBS +3` | `scripts/test/cep_freeze_check.sh` 追加 3 份 main spec(**只冻结 main spec,不冻结归档件 delta**) | 冻结集 **34 → 37** |
| 2 | manifest 重生成 | `cep_freeze_check.sh --update --authorized-by "Entry #64"` | `artifact_count = 37` |
| 3 | selftest 锚点迁移 | `cep_gate_selftest.sh` 两处 fixture 的「活队列 change 名」锚点由队列 #4 → **队列 #5 `cpptlm-cep-collective-plane-base`** | `cep_gate_selftest` 全绿 |
| 4 | `cep_doc_ref_check` 双态 | 代码/测试对 change 目录的引用改为**双态解析**(change 目录 → main spec → 归档件);`kS1_7BetaVerdict.registered_in` 保持**逻辑指针**,读取侧做归档兜底 | 归档前后 `[cep]` 均全绿 |
| 5 | 活文档数字一致性 | 6 处 KPI 基线 `1/3 → 0/3`(obj §5.2/§4.1 · roadmap.md · phase-10 · AGENTS.md · umbrella improvement · ADR-CEP-00)+ 冻结成员数 34 → 37 + `ctest 105` / `[cep] 122 cases / 2050 assertions` | `cep_docs_sync_check --strict` rc=0 |
| 6 | `openspec archive --yes` | 见上表 | rc=0 |

**③ 孤儿前置 owner 活性重指向(per 断言 ⑦③b/③c)**

- 队列 #4 行上的 `⏭(孤儿)` 标记(M-4 行为侧 / verdict 逐 `(network, vc)`)**已消费**并去标记;
- 两项的**孤儿登记行重指向队列 #5**(§4.0 + §4.2 两处,标记与项名同行),`moesi_gpu` / `mesif` 未排期**留在队列 #6 行**(per Entry #57);
- 同一行内保留 `S1-1 / S1-2 / S1-3 / S1-4 / S1-7` 的滚动登记(断言 ⑦①② 要求归档件里的移交标记在活文档两处都出现):S1-1 ✅ 已消费 · S1-7 (β) ✅ 已消费 · S1-2 ⛔ 未触发 · S1-3 ⛔ 未触发 · S1-4 ✅ 已消费。

**④ 冻结面授权(本 entry 即授权记录)**

| 成员 | 改什么 |
|------|--------|
| `docs/adr/ADR-CEP-00-...md` | **仅追加** `Status Update 2026-10-25`(队列 #4 归档,1/3 → 0/3);上一段「当前态」→「当时态」 |
| `openspec/specs/cep-sharer-tracker-base/spec.md` | **新增入集**(归档合并产物) |
| `openspec/specs/cep-ordering-point-checker/spec.md` | 同上 |
| `openspec/specs/cep-channel-dependency-gate/spec.md` | 同上 |
| 其余 33 个成员 | **0 diff** |

**⑤ 验收(实测)**

`openspec validate --changes --strict` rc=0 · 守门 7 项 rc=0 · `cep_gate_selftest` 全绿 · `ctest` **105/105** · `[cep]` **122 cases / 2050 assertions** · `format --check` rc=0 · `markdown_link_check` rc=0 · KPI **0/3** · 冻结集 **37**

**⑥ 遗留(不属本 change)**

`validate_topology` target 首条命令引用不存在的 `build/configs/dgpu_board_v1_mvp.json`(`scripts/CMakeLists.txt:48`)⇒ 该 target 在队列 #4 之前就跑不完,修复需单独决策。

**关联**: Entry #63(builder 实施)· Entry #62(planner 建目录)· Entry #61(队列 #3 归档先例)· `ADR-CEP-04 / 08 / 09` · ArchForge `32 号 §6.0.2/§6.0.3/§6.1/§6.2`

---

### Entry #65 — 2026-10-25 [fix]

**主题**: 队列 #4 对抗式评审（critic）缺陷修复 —— 2 BLOCKER + 6 SHOULD_FIX 落地；3 份 main spec 补 `## Purpose`

**① 触发（首次对抗式评审）**

队列 #4 归档前的自查不等价于对抗式评审（Entry #64 已标注）。critic 恢复可用后补跑，**结论 REJECT**（评审者与被评审代码同源，**无外部真人评审** —— 单开发者仓结构性局限，已在报告中显式声明）：

| 编号 | 问题 | 我的独立验证 |
|------|------|-------------|
| **B1** | ⑥ 镜像闸对 `channel_dependency_edges` **零判别力**：改一条边端点后 ⑥ 全绿、装配闸 PASS | ✅ 实测复现（`[1,3]`→`[1,2]` 全绿）|
| **B2** | 显式声明源不可读/畸形/类型非法 ⇒ 全落 **SKIP**，且被测试钉成契约 | ✅ 实测复现（5 种配置错误全 SKIP；非仓根 CWD 下「漏写声明源」亦 SKIP）|
| S1 | `snoop_handle_t` 16-bit 回绕 ⇒ 存活 handle 被覆盖 ⇒ 旧地址永久 STALL | 读码确认（`next_handle_++` 无存活检查）|
| S2 | `expected_responders` 含未映射位（bit 4..63）⇒ snoop 永不完成 ⇒ 永久 STALL | 读码确认（fold 只能扣已映射位）|
| S3 | `RetireOnZeroSharers` 可伪触发；`set_sharer` 对未映射节点不是 no-op（留可见空条目）| 读码确认 |
| S4 | IC3 五事件中 `RetireOnZeroSharers` **零行为覆盖**（只有名字字符串断言）| grep 确认无 `clear_sharer` 调用 |
| S7 | ⑥ 逐字段比对对 bool/int/float 漂移失明（Python `0 == False`）| ✅ 实测复现（`atomic.supported: false→0` 全绿）|
| S8 | CLI 头注释称「不可读 ⇒ rc=2」但代码 `return 0`；无 `--help` | 读码确认 |

**② 修复（逐条落地）**

| 编号 | 修复 | 位置 |
|------|------|------|
| **B1** | 新增 `archforge_channel_edges()` 解析 ArchForge `32 号 §6.0.3` 的**散文 markdown 表格**（`\| \`compute_noc\` \| VC0→VC1, …`）；⑥ 加第 ③ 子检查**逐边相等**（缺边/多边分别报红；§6.0.3 解析失败判红，不降级 warn）| `cep_decl_alignment_check.sh` |
| **B2** | `ChannelDeclaration` 加 `source_error` / `source_error_detail` / `skip_hint`；`load_channel_declaration(path, explicit_source)`；`parse_channel_declaration_from_config` 对 4 类非法形态置 `source_error`；判据**优先**判 `source_error ⇒ FAIL`（含具体错因）| `channel_dependency_graph.hh` + `module_factory.cc`（无需改，FAIL 已 `return false`）|
| **S1** | 保持 16-bit（**ADR-CEP-04** 钉 `logic[15:0]`，不可 widen）⇒ `allocate_handle()` **线性探测空闲槽** + 满表返回 0；`kHandleSpace = 65535` 提为 public 契约常量 | `sharer_tracker.hh` |
| **S2** | 新增 `kMappedResponderMask`；`snoop_begin` 对 `expected & ~mask` 非零 ⇒ 拒绝（handle=0）| 同上 |
| **S3** | `set_sharer` 未映射节点**提前 return**（不建条目）；`clear_sharer` 对「本非 sharer」no-op（不伪触发 retire）；删死语句 `(void)state;` | 同上 |
| **S4** | 新增 `Scenario 10` 用例：handle 满/未映射位/伪 retire 三条负向 **各有正控**（handle 满 + 首条目存活；仅已映射位接受；真 retire 恰好 1 次）| `test_cep_sharer_tracker_base.cc` |
| **S7** | 新增 `strict_equal()` **递归**类型严格比较（浅层 `type()` 不够 —— `{"supported":0} == {"supported":False}` 为 True）| `cep_decl_alignment_check.sh` |
| **S8** | 退出码修正（0 = PASS/SKIP 外部条件 · 1 = 判据红/配置错误 · 2 = 用法错误）+ `--help` + 显式路径不可读 ⇒ rc=1 + 缺省源 SKIP 打印 cwd | `examples/cep_channel_dependency_gate.cc` |
| **附带** | 缺省源 SKIP 时日志打印 **attempted path + CWD**（B2 的「最低修复线」要求）| `channel_dependency_graph.hh`（`skip_hint`）+ CLI |

**③ 顺带修掉的归档治理遗漏**

`openspec archive` 为 3 份 main spec 写了占位 `## Purpose`（`TBD - created by archiving change …`）⇒ `openspec validate <cap> --strict` **rc=1**。已补实际 Purpose，3 份均恢复 rc=0。

**④ 未修（显式登记，需独立决策）**

| 编号 | 问题 | 为什么不修 |
|------|------|-----------|
| **S5** | 嵌套 `instantiateAll` 调用方（`apu_soc.cc:70` / `pcie_endpoint_ip.cc:563`）吞掉 FAIL；`sim_module.hh:95` 只 DPRINTF 后继续 | 要动 SimModule 契约（depth 判断 / 失败向上传播），非本批范围；当前仓内无 config 带 `cep` 段（`grep -rn '"cep"' configs/` 零命中）⇒ 暴露面为零 |
| **S6** | CI 上 ArchForge 缺席 ⇒ `cep_decl_alignment_check` 恒 SKIP（`.github/workflows/ci.yml` 无 ArchForge checkout）| 需 CI 配置决策（pinned commit 浅克隆，或把 SKIP 拆成显式 not-run 状态）|
| NIT | `cross_net_edges` 结构性恒零（边按网络键控，跨网边不可能被构造）| 死指标，删它要动 spec 的「0 跨网边」Scenario；留待队列 #6 复评 |
| NIT | `kMaxVcId = 3` 硬编码 vs `fabric.json` 的 per-network `vc_count` 双真相源 | 同上，留待队列 #6 |

**⑤ 冻结面授权（本 entry 即授权记录）**

| 成员 | 改什么 |
|------|--------|
| `openspec/specs/cep-channel-dependency-gate/spec.md` | **追加** 2 Scenario（声明源配置错误 ⇒ FAIL / 边表逐边 = §6.0.3）+ 补 `## Purpose`；原 7 Scenario **一字未改** |
| `openspec/specs/cep-sharer-tracker-base/spec.md` | 补 `## Purpose` + **追加 1 Scenario**（拒绝/no-op 语义，见 ⑦ SF-4）；原 9 Scenario 一字未改 |
| `openspec/specs/cep-ordering-point-checker/spec.md` | 同上 |
| 其余 34 个成员 | **0 diff** |

> **为什么 Scenario 6「默认源缺失/为空 ⇒ SKIP」不改**：B2 批评的是**显式指定**的源写错时也 SKIP，那从未被 Scenario 6 覆盖。新增的 Scenario 8 与之互补而非冲突 —— 隔离「外部条件 SKIP」与「配置错误 FAIL」。

**⑥ 验收（实测）**

| 项 | 值 |
|---|---|
| `[cep]` | **124 cases / 2081 assertions**（修前 122 / 2050）|
| 新增用例 | +2（Scenario 8 B2 / Scenario 10 S1·S2·S3 边界）；assertions +31 |
| 反向验证 | `cep_decl_reverse_verify` **16 条**全过（原 12 → 16，见 ⑦ B-NEW-1）|
| 反向验证 | B1 改边 ⇒ 报红（缺边+多边两条明细）· S7 `false→0` ⇒ 报红 · S7 `int→float` ⇒ 报红 · B2 6 形态 ⇒ FAIL |
| 正控 | S1 handle 满后首条目仍存活 · S2 仅已映射位接受 · S3 真 retire 恰好 1 次 |
| 守门 | 7 项 rc=0（含 `cep_freeze_check`，本 entry 授权后）· `cep_gate_selftest` 31/31 |
| `openspec validate <cap> --strict` | 3 份 main spec **rc=0**（修前 rc=1，占位 Purpose）|
| `openspec validate --changes --strict` | rc=0（无 active change）|
| 冻结集 | **37**（成员数不变；3 份 spec 内容更新）|

**⑦ 复审轮（同批 critic 二次评审）**

上一轮 2 BLOCKER + 6 SHOULD_FIX **全部闭合**（critic 逐条实测验收：B1 六方向变异全红 / B2 CLI 矩阵全对 / S1 探针实测回绕后复用空闲槽 / S2~S4 正控齐全 / S7 嵌套漂移报红 / S8 退出码矩阵全对）。复审新发现 1 BLOCKER + 4 SHOULD_FIX，**均已修**：

| 编号 | 问题 | 修复 |
|------|------|------|
| **B-NEW-1** | 新断言**无持久反向 fixture**（违反铁律 2）：spec 自己写了「反向验证 = 改边端点 ⇒ 红」，但 `cep_decl_reverse_verify.sh` 零新增 ⇒ `strict_equal` 回退成 `!=`、或边集比较 fail-open 时**仓内无任何自动化会红** | 加 **4 条 fixture**（§6.0.3 边端点变异 / §6.0.3 整节删除 / 副本 `atomic.supported:false→0` / 副本侧多边）+ `protocol_instances.json` 备份还原机制 ⇒ 反向验证 **12 → 16 条** |
| SF-1 | `instances` 数组混入非对象项（`[42,"junk",null]`）⇒ 双闸 **PASS 且 0 顶点 0 边**（绿灯无牙，比 SKIP 更糟）| parse 时非对象项 / 缺非空 `id` ⇒ `source_error`；通用文案**不得覆盖** parse 的精确 detail（先判 `!d.source_error`）|
| SF-2 | ordering gate 对 `source_error` 失明 ⇒ 日志自相矛盾（channel 说「配置错误」、ordering 说「没有实例」）| `evaluate_ordering_points` 顶部对称判 `source_error ⇒ FAIL` + 透传 `skip_hint` |
| SF-3 | spec 承诺「attempted path **+ 当前工作目录**」，module_factory 路径只兑现一半（有 path 无 cwd）| `skip_hint` 内调 `std::filesystem::current_path()` 填实际 cwd |
| SF-4 | tracker spec 未登记本轮新引入的拒绝/no-op 契约（同 commit 内 channel-gate 追加 2 Scenario，tracker 只补 Purpose ⇒ 两套标准）| 追加 1 Scenario「`snoop_begin` 拒绝语义 + `set_sharer`/`clear_sharer` no-op 语义」（tracker spec 9 → **10** Scenario）|
| NIT | ASCII 箭头（`VC0->VC1`）格式漂移被报成「副本含未声明网络」⇒ 定位方向错 | 解析器区分「行存在但抽不出边」⇒ 报「箭头格式已变?期望 `VC<i>→VC<j>`」 |
| NIT | `strict_equal(None, None)` 为 True ⇒ 一侧缺键 vs JSON null 失明 | 比对改 `local.get(key, _MISSING)` 哨兵 |

**复审未修（登记）**：`snoop_begin(addr, 0, h)` 空 expected 被接受且条目永久泄漏 —— **既有行为，非本轮回归**；拒绝或立即 retire 均属行为变更，留**队列 #6** 裁决。

**复审验收**：`cep_decl_reverse_verify` **16/16** · `[cep]` **124 cases / 2081 assertions** · `[cep][channel-gate]` 8 cases / 91 · `openspec validate cep-sharer-tracker-base --strict` rc=0（10 Scenario）· 基线与复原双向 GREEN

**⑧ 三审（critic 第三次评审）：APPROVE**

**无 BLOCKER、无 SHOULD_FIX**。critic 亲自做了两项**「判据改弱」对抗实验**，验证我声称的回归保护真实有效：

| 实验 | 改什么（临时）| 结果 |
|------|-------------|------|
| ① | `strict_equal(...)` → 裸 `!=` | 破坏 12 变 GREEN ⇒ 反验脚本 **rc=1**（15 通过 / 1 失败）|
| ② | 摘除 ⑥③ 的边集比较循环 | 破坏 10 **和** 13 双双变 GREEN ⇒ 反验 **rc=1**；破坏 11 仍 RED（由「解析失败」守卫独立兜住）⇒ **两条守卫路径正交**，非单一实现 |

另实测：SIGTERM 在破坏 1 窗口中途杀（rc=143）后 `git diff` 0 行 ⇒ `trap cleanup EXIT` 对 SIGTERM/SIGINT 均生效；ArchForge 真仓零改动（脚本只写 `$TMP` 副本）。

**已修 NIT（critic 建议随手修）**：
- NIT-1 `test_cep_sharer_tracker_base.cc` 文件头「9 Scenario / 9 锚点」→ **10 / 10**（Scenario 10 于 `12dbd582` 引入时漏改）
- NIT-6 新 Scenario 的锚点精度：「未映射节点 `set_sharer` 不建空条目」的断言实际在**前序**「caps descriptor」用例内，已在锚点段注明两处合起来才是完整覆盖

**登记不修（NIT 级，风险有界）**：
- `id` 为数字时报「缺少非空的 `id`」而非「类型错」（FAIL 判向正确，仅报文可改进）
- `id: " "`（纯空白）通过非空检查 ⇒ **内联 config** 路径可得 0 顶点 PASS（权威声明文件路径由 schema `pattern: ^[a-z0-9][a-z0-9_-]*$` 兜住）
- 破坏 10 的 `text.replace('VC1→VC3', …, 1)` 是**全文首匹配**、未锚定节内 —— **失败方向是响的**（改错位置 ⇒ 保持 GREEN ⇒ 反验脚本红），故不阻塞
- SIGKILL / 断电无法 trap ⇒ 可能残留被破坏文件（建议后续在脚本头注释补 `git checkout -- include/tlm/cep/` 恢复方法）
- 默认源分支 `if (d.instances.empty())` 缺 `&& !d.source_error` ⇒ skip_hint 死计算（FAIL 分支不打印 hint ⇒ 用户不可见）

**三审验收**：`[cep]` **124 cases / 2081 assertions** · 反验 16/16 · ctest 105/105 · selftest 31/31 · 守门 7 项 rc=0 · `openspec validate <cap> --strict` 3 份 rc=0 · KPI **0/3** · 冻结集 **37**

**关联**: Entry #64（归档）· Entry #63（builder 实施）· critic 评审 2 BLOCKER / 8 SHOULD_FIX · `ADR-CEP-04`（16-bit handle RTL 契约，S1 因此不可 widen）· ArchForge `32 号 §6.0.3`（边表权威）

---

### Entry #66 — 2026-10-26 [plan]

**主题**: 队列 #5 `cpptlm-cep-collective-plane-base` **planner 阶段** —— proposal + design + 2 个 delta capability 落盘;KPI 0/3 → 1/3;3 份 ADR Status Update 授权

**① planner 产出(4 件套之 3;`tasks.md` 属 builder 阶段)**

| 产物 | 规模 | 内容 |
|---|---|---|
| `openspec/changes/cpptlm-cep-collective-plane-base/proposal.md` | 160 行 | 缺口表(5 项,全部实测)+ What Changes + Capabilities + Impact + **Acceptance 20 项**(A-1-1~12 / A-2-1~8)+ 收尾 5 项 |
| `openspec/changes/cpptlm-cep-collective-plane-base/design.md` | 246 行 | Context(现状/目标态/8 条关键约束)+ Goals/Non-Goals + **决策 D-α~D-θ**(8 条)+ 实施期裁决占位 + 9 条 Risks + 5 条 Open Questions + 与队列 #3/#4 先例对照 |
| `openspec/changes/cpptlm-cep-collective-plane-base/specs/cep-collective-plane-base/spec.md` | 13 Scenario | 接口面 / 类型面(含 `CollectiveEvt` Fault 变体)/ Null(行为化 `INV-CEP-07-IC3`)/ Local(group 生命周期 + BARRIER rendezvous + epoch invalid 窗口(行为化 `INV-CEP-04-IC5` 的 **R2 判读侧**))/ 区域互斥(CEP-13)/ 未注册区域报错 / functional 零延迟 / capability 边界 / 零调用边 |
| `openspec/changes/cpptlm-cep-collective-plane-base/specs/cep-collective-plane-contract/spec.md` | 8 Scenario | 声明源形态与 schema / 三态显式化 / include-graph 闸 / 10 流零 diff / VC 不冲突 / 同源禁双实现 / 抓取点可测性登记 / B2 贡献非空 |
| `.rddf/improvements/cpptlm-cep-collective-plane-base.md` | 5 段草稿 | Why / What Changes / Non-Goals / Acceptance / Capabilities / Impact(planning 输入) |

**② 裁决(DRI 四问,2026-10-26,全部取推荐项)**

| 问 | 裁决 | 依据 |
|---|---|---|
| **范围** | **切小**:`CollectivePlaneIf` + `NullCollectivePlane` + `LocalCollectivePlane`(只做 group 生命周期 / region-epoch / **BARRIER rendezvous**);REDUCE·BROADCAST·MULTICAST **数据路径**推迟至 P3 | REDUCE 等需 fabric ingress 分类(per `ADR-CEP-03 §3.1` R2)⇒ 属 P3;BARRIER 是唯一不依赖硬件特定逻辑的原语;且 H4 假设需要**能跑**的载体 |
| **Bundle 路径** | **类型内嵌** `include/tlm/cep/collective_plane.hh`,**不落 Bundle** | `ADR-CEP-03 §5` 假设的 `coherence_bundles_tlm.hh` **现实不存在**(06-bis P2 未实施);提前建该文件会抢 06-bis P2 ⇒ 与 `ADR-CEP-07A` artifact disposition 冲突;Bundle 化推迟至 P3(届时新建 `include/bundles/collective_bundles_tlm.hh`) |
| **注册形态** | **header-only,不进 `ModuleFactory`** | `07 号 §6.3` 读作**最终形态**口径(同表 SharerTracker 在队列 #4 亦未注册为实证);与队列 #3/#4 先例一致 |
| **声明源** | **独立** `include/tlm/cep/collective_planes.json` + schema(诞生即不冻结) | `protocol_instance.schema.json` 是冻结成员且 `additionalProperties: false` ⇒ 扩它要两步授权;声明维度也不同(plane 能力/资源 vs 协议能力) |

**③ 关键设计裁决(非四问,但影响契约)**

| 编号 | 裁决 | 理由 |
|---|---|---|
| **D-ε** | **B2 贡献 = 行为化既有条目,新增 invariant 数 = 0** | `test_cep_canonical_invariants.cc::expected_groups()` 把 15 组 66 条编号**硬编码**;`ADR-CEP-01`/`ADR-CEP-03` 在 15 组里**无组** ⇒ 新增编号即契约违反。贡献 = 行为化 `INV-CEP-07-IC3` + `INV-CEP-04-IC5` 的 **R2 判读侧** + 契约层加固(零调用边 / 10 流零 diff)。**清单为空则自我否决** |
| **D-ζ** | 拍死两处 ADR 留白:AllReduce = **调用方组合**(plane 守 4 原语最小集);`group_destroy` 对 in-flight 操作**发 `Fault` 且不阻塞** | 避免 plane 长出第 5 原语;避免 destroy 引入隐式依赖(违反零调用边精神) |
| **D-η** | **归档日治理 6 步预登记** | 队列 #4 的教训:planner 不预登记,builder 归档时必漏。含 selftest 锚点 **#5 → #6** 的具体值 |

**④ 冻结面授权(本 entry 即授权记录;3 个成员,均**仅追加** `## Status Update` 段)**

| 成员 | 追加内容 |
|------|---------|
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | Status Update 2026-10-26(队列 #5 已启动,KPI 0/3 → 1/3)+ 上一段「当前态」→「当时态」 |
| `docs/adr/ADR-CEP-03-fabric-collective-plane-internals.md` | Status Update 2026-10-26 —— **§5 Bundle 路径偏离登记**(三候选路径取舍 + 未来具体路径 P3 `include/bundles/collective_bundles_tlm.hh`) |
| `docs/adr/ADR-CEP-01-fabric-collective-plane.md` | Status Update 2026-10-26 —— **§3.3 七条抓取点逐条可测性裁决**(可测 4 / 仅声明层 1 / 已满足 1 / 推迟 2) |

> **⚠️ 引用约束(反引号纪律)**:`ADR-CEP-00` / `ADR-CEP-01` / `ADR-CEP-03` 均在 `test/cep_adr_parser.hh::no_inv_adr_files()` 的 **no-inv 白名单**(6 份)⇒ 其 Status Update 里引用 invariant 编号**必须**用反引号包裹(如 `` `INV-CEP-07-IC3` ``)。
> **⚠️ 纪律强度的准确表述(critic 实测修正,2026-10-26)**:写成 `- **INV-...**:` 或 `**INV-...**` 紧邻形态**会**被 `defined_invariant_ids()` 的 P1/P2 形态识别捕获 —— 但**机器闸未必红**:三条相关测试(全仓已定义 / 全仓孤儿 / no-inv 白名单)都按 **`owner == 文件`** 过滤 ⇒ 在 no-inv ADR 里写**有 owner 的**编号(如 `INV-CEP-07-IC3` 的 owner 是 ADR-CEP-07)⇒ **不被拦**;而写**无 owner 的**编号(如 `INV-CEP-17-IC1`)⇒ **必红**。
> ⇒ 即:反引号纪律**仍然正确**(避免把引用误写成定义),但「写成定义形态就必红」是**过强**的表述。
> ⇒ **顺带暴露的机器闸盲区(登记备查,非本 change 范围)**:no-inv 文件里定义形态的**既有编号**不可见 —— 建议后续 change 给 `test_cep_canonical_invariants.cc` 补一条「no-inv ADR 内不得出现定义形态编号(不论 owner)」的断言。

**⑤ 活文档数字一致性(6 处 KPI 0/3 → 1/3)**

`obj §5.2` · `obj §4.1` · `.rddf/roadmap.md` · `.rddf/roadmap/phases/phase-10.md` · `.rddf/improvements/cpptlm-cep-umbrella.md` · `AGENTS.md` 现行基线行 —— 逐处「命中 1 次」断言通过。

**⑥ 验收(实测)**

| 项 | 值 |
|---|---|
| `openspec validate cpptlm-cep-collective-plane-base --strict` | **rc=0,`is valid`**(无 WARNING) |
| `openspec list --json` | **1 个 active** ⇒ KPI **1/3** |
| 机器闸 `cep_docs_sync_check.sh --strict` | rc=0(断言 ⑦ 移交链 / 孤儿登记 / KPI baseline 三项全过) |
| 守门其余 6 项 | rc=0 |
| 冻结集 | **37**(成员数不变;3 份 ADR 内容更新) |
| 机器闸锚点 | `cep_gate_selftest.sh` 活队列锚点**当前钉在 #5** ⇒ 归档日须迁到 #6(per D-η 第 3 步) |

**⑦ 承接与孤儿登记(不动)**

队列 #5 行的 **S1-1 / S1-2 / S1-3 / S1-4 / S1-7** 滚动登记 + **2 条 `⏭(孤儿)` 登记**(M-4 行为侧 / verdict 逐 `(network, vc)`)—— 状态列改写**未触碰**这些标记(per 机器闸断言 ⑦:含 S1-N 的行不得指向已归档 change,而 #5 尚未归档)。

**关联**: Entry #65(队列 #4 对抗式评审修复)· Entry #64(队列 #4 归档)· `ADR-CEP-01` / `ADR-CEP-03`(本 change 的两份权威源,均已追加 Status Update)· `ADR-CEP-07`(`INV-CEP-07-IC3` owner)· `ADR-CEP-04`(`INV-CEP-04-IC5` owner)

---

### Entry #67 — 2026-10-26 [build]

**主题**: 队列 #5 `cpptlm-cep-collective-plane-base` **builder 实施** —— 2 header + 2 声明源 + 1 CLI + 3 test(24 Scenario);tasks 61/61 全勾;新增 invariant 数 = **0**

**① 交付内容**

| 文件 | 形态 | 内容 |
|---|---|---|
| `include/tlm/cep/collective_plane.hh` | header-only,testbench 定位 | `CollectivePlaneIf` **恰好 7 个虚方法**(逐字对齐 `ADR-CEP-03 §2 Decision 2`)+ 类型集(`group_id_t` / `member_set_t` / `GroupAttrs` / `AddrRange` / `AccessMode` / `CollectiveOp` / `ReduceOp` / `DataType` / `PayloadDesc` / `CollectiveReq` / `CollectiveEvt` / `cap_t` / `Status` / `RegionState`)+ `NullCollectivePlane`(**行为化 `INV-CEP-07-IC3`**)+ `LocalCollectivePlane`(group 生命周期 / region-epoch 两阶段 / BARRIER rendezvous / **R2 判读侧三态**) |
| `include/tlm/cep/collective_plane_contract.hh` | header-only | 6 条契约判据(零调用边 / canonical 10 流零 diff / collective VC 预留 / 区域互斥**与行为面同源** / 完成域判别 / 抓取点可测性登记)+ 三态 SKIP/PASS/FAIL + verdict 逐条可 grep |
| `include/tlm/cep/collective_planes.json` + `.schema.json` | 声明源(**诞生即不冻结**) | 3 组字段(`plane_binding` / `registered_regions` / `collective_vc_reservation`);schema `additionalProperties: false` |
| `examples/cep_collective_plane_contract.cc` | 离线 CLI | 与 test 侧**同一份**判据实现;退出码 0/1/2 |
| `test/test_cep_collective_plane_base.cc` | 13 Scenario / 126 assertions | 行为面 |
| `test/test_cep_collective_plane_contract.cc` | 8 Scenario / 102 assertions | 判据面 |
| `test/test_cep_collective_plane_invariants.cc` | 3 Scenario / 39 assertions | **B2 贡献载体**(既有编号的行为化) |

**② B2 贡献(新增 invariant 数 = 0)**

| 方式 | 内容 | 归属组 |
|---|---|---|
| **行为化** | `INV-CEP-07-IC3`:NullCollectivePlane 全部 7 原语 ⇒「不支持」+ 无副作用(对照:Local 不得报不支持) | "Simulation + Seam" |
| **行为化**(R2 **判读侧**) | `INV-CEP-04-IC5`:`table 命中 + epoch invalid` ⇒ **不 divert**(放行),非 fail-stop;epoch commit 后翻转 —— **行为主体 `fabric_ingress_classify` 本仓不存在**(per `ADR-CEP-04:301`),P3 实现真 ingress 时须在那里**重新**行为化 | "Sharer Tracker" |
| **契约层加固** | 零调用边(include-graph 闸)+ canonical 10 流零 diff | "Coherence Interface" / "Simulation + Seam" |

> 15 组 66 条编号**未被改动**(`test_cep_canonical_invariants.cc` 零 diff)⇒ D-ε 的计数口径成立。
> `ADR-CEP-01` / `ADR-CEP-03` 在 no-inv 白名单 ⇒ 本 change **未**在其正文定义任何 invariant
> (由 `test_cep_collective_plane_invariants.cc` 的「零新增编号」断言机器守护)。

**③ 实施期偏差(design D-κ ~ D-π 全文已落盘)**

| # | 偏差 | 性质 |
|---|------|------|
| D-κ | `vc_binding` 权威位置 = `protocol_instances.json`(**不是** `fabric.json`)⇒ CP-03 回填 | 语义修正 |
| D-λ | collective VC 预留默认**空**(实测 2 实例 × 2 网络占满 VC 0-3 ⇒ 本仓当前拓扑**无** collective 可用隔离 VC)⇒ 判别力由反向 fixture 承载,结论登记在声明源 `current_state` | 语义补充 |
| D-μ | 加装配缝 `set_current_member`(barrier 的 7 方法签名**无** member 参数 ⇒ N-1/N 判定需要身份) | 措辞修订 |
| D-ν | 加装配缝 `complete_drain`(epoch 两阶段的 drain 完成回告入口) | 语义补充 |
| D-ξ | canonical 流名匹配改**词边界**(裸子串会把 `collective_barrier(` 误判成 `barrier(`) | 语义修正 |
| D-ο | 新增 `ContractRule::None` 哨兵(原先拿 `ZeroCallEdge` 兼作哨兵 ⇒「未绑定」与「绑定该规则」无法区分) | 语义修正 |
| D-π | R3 drain 只落地「plane 侧不发起」这一半(drain 清单的发出方留队列 #6) | 语义补充 |

> **7 方法接口面零变化** ⇒ A-1-1「逐字对齐 `ADR-CEP-03 §2 Decision 2`」仍成立。

**④ 实施期自查抓到的 5 处真缺陷(已修,登记备查)**

| # | 缺陷 | 后果 |
|---|------|------|
| 1 | 判据编排 `add(rule, evaluate(d), std::move(d))` —— **函数实参求值顺序未指定** ⇒ `std::move(d)` 可能先执行 ⇒ detail 被移空 | 判据红但说不出原因 |
| 2 | `ContractRule` 哨兵冲突 | 抓取点覆盖核对漏判 |
| 3 | canonical 流名裸子串匹配 | CP-02 假阳性 |
| 4 | 抓取点解析三处错(未限定段 / gate 未容忍 `>` blockquote 前缀 / 列计数错位) | 7 条抓取点**一条都解析不出** |
| 5 | `ContractPaths::at()` 无条件拼 root ⇒ 绝对路径指向不存在路径 | 「畸形」被误报成「不可读」 |

> 与队列 #4 的经验一致:**判据实现退化必须靠反向 fixture + 实跑发现**,读代码不够。

**⑤ 与 planner 产出的偏差(需同步回 plan)**

| # | plan 原文 | 实施期 |
|---|----------|--------|
| 1 | 「与 `fabric.json` 既有 `vc_binding` 不冲突」 | 对象错 ⇒ 回填为 `protocol_instances.json` |
| 2 | `capability 1` = 12 Scenario | 补 1 条(已注册区域数据原语行为)⇒ **13**;合计 **21** |
| 3 | 无 CLI | 补 `cep_collective_plane_contract` CLI(判据不能只能靠 test 跑) |
| 4 | 「已注册且 epoch 生效区域上的数据原语行为」未规定 | 补 Scenario + 裁决(明确的「数据路径未实现」状态) |

**⑥ 验收(实测)**

| 项 | 值 |
|---|---|
| `[cep]` | **148 cases / 2374 assertions**(planner 期 122 / 2050) |
| collective 三档 | `24 cases / 281 assertions`(13 + 8 + 3;per 三审实测订正,原写 267 有误)|
| `ctest` | **106/106**(新增 1 = CLI 的 ctest 入口) |
| 守门 7 项 | rc=0 |
| `cep_gate_selftest` | 31/31 |
| `openspec validate cpptlm-cep-collective-plane-base --strict` | **is valid** |
| `openspec list --json` | **1 个 active** ⇒ KPI **1/3** |
| 冻结集 | **37**(成员数不变;本批**零**冻结成员改动) |
| `tasks.md` | **61 / 61** 全勾(T6-7 归档治理留归档阶段) |
| CLI 矩阵 | 无参数 rc=0 · `--help` rc=0 · 错根 rc=1 · 多余参数 rc=2 |

**⑦ 关联**: Entry #66(planner 阶段)· Entry #65(队列 #4 对抗式评审修复)· `ADR-CEP-01` / `ADR-CEP-03`(本 change 的两份权威源)· `ADR-CEP-07`(`INV-CEP-07-IC3` owner)· `ADR-CEP-04`(`INV-CEP-04-IC5` owner)· `ADR-CEP-02`(canonical 10 流权威)

---

### Entry #68 — 2026-10-26 [fix]

**主题**: 队列 #5 builder 实施**对抗式评审修复** —— 3 BLOCKER + 10 SHOULD_FIX + 3 NIT 落地;7 个独立 PoC 逐条复验通过

**① 触发**

critic 对 builder 实施(cff2bb87)的复审:结论 **REJECT**。3 个 BLOCKER 全部由评审者在**仓外**写独立 PoC
(直接链接被评审 header 实跑)坐实,非推测。

> **方法论教训(登记备查)**:第一轮实现「24 Scenario 全绿 · 守门 7 项 rc=0 · CLI 6 判据全 PASS」仍被判 REJECT。
> 原因不是「测试不够多」,而是**测试锚错了对象**:
> - **B-2**:测试断言的恰是 spec 明文**禁止**的行为 ⇒ 测试与 spec 互相矛盾,而两者都「绿」;
> - **B-1 / B-3**:spec 有条款,测试**零覆盖** ⇒ 「有 24 个 Scenario」≠「条款被覆盖」。
> ⇒ 后续 change 的 Scenario 清单应**逐条对账 spec 的每个 AND 子句**,而非只对账 Scenario 标题。

**② 3 个 BLOCKER(必堵,逐条 PoC 复验)**

| # | 缺陷 | 后果 | 修复 | PoC 复验结果 |
|---|------|------|------|------------|
| **B-1** | `group_destroy` 只把 epoch 置 false,**不移除 region** | 残留 ⇒ 重叠检查遍历全表 ⇒ 该地址段**永久报废**(新 group 再也注册不进去);违反 spec「destroy 后 region 表移除」 | destroy 时**从 `regions_` 移除**该 group 的 region | `region_state_of` → `no-region` ✅;重新注册 → `ok` ✅ |
| **B-2** | `send` 在 NoRegion / EpochInvalid 分支**投递 Fault 事件** | 违反 spec「**不得**投递任何事件」;**测试锚定的恰是被禁止的行为** | 错误**只经状态**传达;事件通道留给真正的 partial failure | `pending_events=0` ✅ |
| **B-3** | `collective_barrier` **不校验成员身份** | 非成员到达计入阈值 ⇒「N-1 不释放」被绕过 ⇒ 破掉本 plane **唯一的 ordering 承诺** | 到达前校验 `current_member_ ∈ group.members`,否则 `Status::NotMember` | 非成员 + 2 真成员 ⇒ `pending=0`(不提前释放)✅ |

**③ 10 个 SHOULD_FIX(全部落地)**

| # | 缺陷 | 修复 |
|---|------|------|
| S-1 | `recv` 空队列语义缺失(旧事件原样返回 + status=ok) | 空 ⇒ 清空出参 + `Status::NoEvent` |
| S-2 | `complete_drain` 粒度 = per-gid 全量(部分 drain 无法表达) | 改 **per-region**:`complete_drain(gid, range)` |
| S-3 | CP-06 分类落洞(无法分类的措辞**静默绿**) | 三类措辞全不命中 ⇒ **FAIL** |
| S-4 | CP-06 的 marker **硬编码日期** | 改取**最后一个**含「抓取点」关键词的 Status Update 段(本仓铁律:ADR 不可变 = 追加新段) |
| S-5 | CP-01 只扫 `collective_plane.hh` **一个** header | 扫**两个**(行为面 + 判据面) |
| S-6 | CP-01 整行子串 + **不去注释** ⇒ 注释行误报 | **去注释** + 按 **include 路径**判定 |
| S-7 | CP-02 只匹配 `name(` 形态 ⇒ `struct request` / `void barrier (` 漏检 | 改为**词边界 + 任意形态**匹配 |
| S-8 | CP-04 忽略声明源的 group 字段 | 按**声明的 group 编号**建 group |
| S-9 | spec 区域互斥状态字面与实现不一致 | spec 对齐实现(`RegionOverlap` —— 语义比笼统的 `group_register_error` 更精确) |
| S-10 | CP-05 不去注释 | 去注释后再查 |

**④ 3 个 NIT(落地)**

`GroupAttrs::collective_enabled` 死字段(声称的契约未实施)→ **删除**;`Group::alive` / `BarrierState::released_phases`
死状态 → **删除**;`next_gid_` 绕回撞 `kInvalidGroupId` 哨兵 → 绕回时**跳过哨兵**;
另补 `Scenario 13` 行为面侧的 poisoned fixture(此前反向 fixture 只打在判据面函数上)。

**⑤ 一处对外口径订正**

critic 判定:`INV-CEP-04-IC5` 的**行为主体**是 `fabric_ingress_classify`(本仓不存在),
本 change 行为化的是 R2 谓词的**判读侧** ⇒ 对外口径**不得**声称「IC5 原文主体已被行为化」。
⇒ commit message / 台账措辞统一为「**行为化 IC5 的 R2 判读侧**」,并保留
「P3 实现真 ingress classify 时须在那里**重新**行为化」的登记(design D-ε 已有)。

**⑥ 7 个独立 PoC 复验(全部已堵)**

| PoC | 攻击 | 复验结果 |
|-----|------|---------|
| 1 | 非成员 barrier 到达提前释放 | `pending=0` ✅ |
| 2a | destroy 后 region 残留 | `no-region` ✅ |
| 2b | 残留导致永久阻塞重注册 | `ok`(可重注册)✅ |
| 3 | send 未注册区域投递事件 | `pending_events=0` ✅ |
| 4 | recv 空队列不可区分 | `no-event` ✅ |
| 5 | CP-06 措辞绕过 | `FAIL` ✅ |
| 6 | CP-01 注释行误报 | `PASS`(不误报)✅ |
| 7 | CP-02 非调用形态漏检 | `FAIL`(已检出)✅ |

**⑦ 验收(实测)**

| 项 | 值 |
|---|---|
| `[cep]` | **148 cases / 2388 assertions**(修复前 2374) |
| collective 三档 | 24 cases / **279** assertions(修复前 267) |
| `ctest` | **106/106** |
| 守门 7 项 | rc=0 |
| `cep_gate_selftest` | 31/31 |
| CLI | 6 判据全 PASS(rc=0) |
| `openspec validate` | **is valid** |
| 冻结面 | **37** —— 本批**零**冻结成员改动 ⇒ manifest 无需重生成 |
| `design.md` | 新增 **D-σ**(复审轮全文 + 方法论教训) |

**关联**: Entry #67(builder 实施)· Entry #66(planner 阶段)· critic 复审(REJECT:3 BLOCKER / 10 SHOULD_FIX / 3 NIT)· `ADR-CEP-02`(canonical 10 流权威)· `ADR-CEP-04`(`INV-CEP-04-IC5` owner)

---

### Entry #69 — 2026-10-26 [fix]

**主题**: 队列 #5 **三审轮**(critic 第三次评审)—— APPROVE_WITH_CONDITIONS:5 SHOULD_FIX + 6 NIT 落地;3 个原 BLOCKER 经独立 PoC 复验闭合

**① 三审结论**

critic 三审:**无 BLOCKER**。3 个原 BLOCKER(B-1 region 残留 / B-2 违规投递事件 / B-3 无成员校验)
由评审者**仓外独立 PoC** 逐条复验闭合(并额外构造了「修复转移」场景:二次 destroy、跨 group member、
partial drain、破坏注释/字符串边界的 include 形态 —— 均未发现新的失效路径)。

**B2 贡献清单的诚实性判定(critic 原文)**:「**行为化 IC5 的 R2 判读侧**」表述**准确**
—— 实现交付的是 `region_state_of`(R2 谓词判读)+ 观测口,`fabric_ingress_classify` 确实不存在于本仓;
`INV-CEP-07-IC3` 仍成立(Scenario 3 全原语 `Unsupported` + 无副作用 + 反向 fixture 实测通过)。

**② 5 个 SHOULD_FIX(全部落地)**

| # | 缺陷 | 修复 |
|---|------|------|
| **U-1** | `CatchpointBinding::declared_measurable` 是**只写不读的死字段** —— 与刚删除的 `collective_enabled` **同类复发**,且就发生在修该类问题的同一个 commit 里 | 让它参与**交叉核对**:「映射表声明的覆盖承诺」必须与「ADR 的措辞分类」**严格一致**,不一致 ⇒ FAIL ⇒ 死字段变成**真加固** |
| **U-2** | group 级 `epoch_valid` 旗标在 per-region drain 后**说谎**(2-region group 只 drain 1 个 ⇒ 旗标为真、另一 region 仍在 invalid 窗口) | 删除该旗标;`epoch_valid(gid)` 改为「名下**所有** region 都已 drain 回告」 |
| **U-3** | `Scenario 13` 的「反向 fixture ②」是**同义反复**(拿刚构造的 poisoned 文本找自己的字面量)⇒ 声称堵的洞仍开着;且行为面扫描与 CP-01 **实质分叉**(漏 `directory` 条目 / 漏尖括号形态 / 漏无前缀形态) | 行为面改为与判据面**共享** `extract_include_paths` + `include_path_is_forbidden`;反向 fixture 改为**真**注入(尖括号 / 注释 / 禁用目录三种形态) |
| **U-4** | CP-06 段选择仍**关键词脆弱**:重判段若标题不含「抓取点」三字 ⇒ 读到旧段 ⇒ 重判不可见、闸仍绿 | 段选择改为「含**抓取点登记表的表格形态**(`| **N**(` 行)」⇒ 与标题措辞**解耦** |
| **U-5** | 6 个 NIT | 全部落地(见 ③) |

**③ 6 个 NIT 的落地**

| NIT | 处理 |
|---|---|
| `complete_drain` 文档注释**新旧两段并存** | 删旧段 |
| `recv` 空队列出参语义未在接口注释写明 | 接口注释补「空队列 ⇒ 出参置无效 group 的 Completion + `NoEvent`」 |
| `NotMember` 同态覆盖「未装配身份」与「身份非成员」 | 注释说明**有意合并** |
| commit message 断言数订正(实测 281) | 台账 Entry #67 数字订正 |
| **台账活表格未同步**「R2 判读侧」限定 | obj **3 处**活表格 + Entry #66 **2 处**表述已加限定;`owner)` 引用行不动 |
| CP-04 声明 gid 语义未澄清 | design 新增 **D-τ**:声明 gid = **分组标签**,不是句柄预约 |

**④ 3 个关键 PoC 复验**

| PoC | 攻击 | 复验结果 |
|-----|------|---------|
| SF-2 | 2-region group 只 drain 一个 ⇒ `epoch_valid(gid)` 不得为真 | `false` ✅(R1=active · R2=epoch-invalid) |
| SF-1 | 「映射表覆盖承诺」与「ADR 措辞」不一致 | `FAIL` ✅(已判红) |
| SF-4 | 重判段标题**不含**「抓取点」三字但含表格形态 | `parsed=1` ✅(读到新段) |

**⑤ U-1 的教训(登记备查)**

上一轮刚因「`collective_enabled` 是**声称的契约未实施**」被要求删除,同一轮里**又新增**了同类死字段
(`declared_measurable`)⇒ **「删除死字段」不等于「字段都有用」** —— 新增字段时必须问「谁读它」。
本轮已把它改造成**交叉核对项**(承诺 vs 声明对账),从「死字段」变成「真加固」。

**⑥ 验收(实测)**

| 项 | 值 |
|---|---|
| `[cep]` | **148 cases / 2390 assertions** |
| collective 三档 | 24 cases / **281** assertions(13 + 8 + 3) |
| `ctest` | **106/106** |
| 守门 7 项 | rc=0 |
| `cep_gate_selftest` | 31/31 |
| CLI | 6 判据全 PASS(rc=0) |
| `openspec validate` | **is valid** |
| 冻结面 | **37** —— 本批**零**冻结成员改动 ⇒ manifest 无需重生成 |
| `design.md` | 新增 **D-τ** + **D-υ**(三审轮全文 + U-1 教训) |

**关联**: Entry #68(复审轮:3 BLOCKER 修复)· Entry #67(builder 实施)· Entry #66(planner 阶段)· critic 三审(APPROVE_WITH_CONDITIONS:5 SHOULD_FIX / 6 NIT)· `ADR-CEP-04`(`INV-CEP-04-IC5` owner —— 行为主体 `fabric_ingress_classify` 本仓不存在,P3 须重新行为化)

---

### Entry #70 — 2026-10-26 [archive]

**主题**: 队列 #5 `cpptlm-cep-collective-plane-base` **完成归档** —— 21 Scenario 并入 2 份 main spec;KPI 1/3 → 0/3;冻结集 37 → 39

**① 归档动作(语义 = ✅ 已完成归档)**

| 项 | 值 |
|---|---|
| 命令 | `openspec archive --yes cpptlm-cep-collective-plane-base` |
| archive 名 | `2026-10-26-cpptlm-cep-collective-plane-base` |
| delta → main | `cep-collective-plane-base`(13 Scenario)· `cep-collective-plane-contract`(8 Scenario)⇒ 合计 **21 Scenario** |
| tasks | **61 / 62** 全勾(T6-7 归档治理 6 步 = 本 entry) |
| KPI | **1/3 → 0/3** |

**② 归档日治理 6 步(逐条落地,per design D-η / `Entry #64` 模板)**

| # | 步骤 | 落地 | 验证 |
|---|---|---|---|
| 1 | `FROZEN_GLOBS +2` | 追加 2 份 main spec(**只冻 main spec,不冻归档件 delta**) | 冻结集 **37 → 39** |
| 2 | manifest 重生成 | `cep_freeze_check.sh --update --authorized-by "Entry #70"` | `artifact_count = 39` |
| 3 | selftest 锚点迁移 | `cep_gate_selftest.sh` 两处 fixture 的「活队列 change 名」锚点 **#5 → #6**(`cpptlm-cep-chi-rnf-subset`) | `cep_gate_selftest` 全绿 |
| 4 | `cep_doc_ref_check` VIRTUAL_PATHS 双态注册 | 登记 `openspec/changes/cpptlm-cep-collective-plane-base`(归档前路径保留历史指向) | `cep_doc_ref_check` rc=0 |
| 5 | 活文档数字一致性 | KPI 基线 1/3 → 0/3 的 **6 处** + 队列 #5 行 → ✅ archived + **孤儿登记行重指向队列 #6**(§4.0 + §4.2 两处,标记与项名同行) | `cep_docs_sync_check` rc=0 |
| 6 | `openspec archive --yes` | 见上表 | rc=0 |

**③ 队列 #5 的三段评审闭环(本仓首次)**

| 轮次 | 结论 | 处置 |
|---|---|---|
| planner 产出评审 | APPROVE_WITH_CONDITIONS(1 BLOCKER 级计划洞 + 6 SHOULD_FIX) | 全部落地(design **D-ι** 5 项裁决) |
| builder 实施评审 | **REJECT**(3 BLOCKER + 10 SHOULD_FIX + 3 NIT) | 全部落地(design **D-σ**);3 个 BLOCKER 经 7 个独立 PoC 复验 |
| 三审 | **APPROVE_WITH_CONDITIONS**(5 SHOULD_FIX + 6 NIT) | 全部落地(design **D-υ**) |

> **B2 贡献清单的诚实性**:critic 三审明确判定「**行为化 IC5 的 R2 判读侧**」表述**准确** ——
> `INV-CEP-04-IC5` 的**行为主体** `fabric_ingress_classify` 本仓不存在(per `ADR-CEP-04:301`),
> P3 实现真 ingress 时须在那里**重新**行为化;`INV-CEP-07-IC3` 成立(全原语 `Unsupported` + 无副作用 + 反向 fixture)。
> **新增 invariant 数 = 0** ⇒ 15 组 66 条编号零 diff(per `test_cep_canonical_invariants.cc` 未改动)。

**④ 冻结面授权(本 entry 即授权记录)**

| 成员 | 改什么 |
|------|--------|
| `openspec/specs/cep-collective-plane-base/spec.md` | **新增入集**(归档合并产物) |
| `openspec/specs/cep-collective-plane-contract/spec.md` | 同上 |
| 其余 37 个成员 | **0 diff** |

**⑤ 验收(实测)**

`[cep]` **148 cases / 2390 assertions** · collective 三档 **24 cases / 281 assertions** ·
`ctest` **106/106** · 守门 7 项 rc=0 · `cep_gate_selftest` 31/31 ·
`cep_decl_reverse_verify` 16/16 · `openspec validate <cap> --strict` rc=0 ·
CLI 6 判据全 PASS · KPI **0/3** · 冻结集 **39**

**⑤b 补写 2 份 main spec 的 `## Purpose`**(归档治理第 5 步的补充)

`openspec archive` 为新建 capability 写的是**占位句**(`TBD - created by archiving change …`)⇒
`openspec validate <cap> --strict` **rc=1**。已补实际 Purpose(与队列 #4 的同一遗漏同型),
2 份均恢复 rc=0;manifest 随之重生成(成员数仍 39)。

**⑥ 关联**: Entry #69(三审轮)· Entry #68(复审轮)· Entry #67(builder 实施)· Entry #66(planner 阶段)· `ADR-CEP-01` / `ADR-CEP-03`(两份权威源,均已追加 Status Update)


---

### Entry #71 — 2026-10-26 [scope-change]

**主题**: 队列 #6 **重排** —— `cpptlm-cep-moesi-gpu-adapter` 建 change 目录 + planner 四件套落盘;`moesi_gpu` / `mesif` **排入** Stage 1;全局 **11 → 13** change / **11 → 13** 季度;终态 2029-Q2 → **2029-Q4**;KPI **0/3 → 1/3**

**① 触发**

用户指令:「起队列 #6 的 planner 阶段, 对 moesi_gpu/mesif 等协议进行规划排期」。
原队列 #6 = `cpptlm-cep-chi-rnf-subset`(Stage 1 / 2028-Q1)—— 但它的范围里挂着 **2 项无主的前置**:
**S1-7 ①②**(`perm` 形态 schema 演进)+ **S1-5**(6→3 消息类映射表机制),
以及 **1 条孤儿登记**(`moesi_gpu` / `mesif` **未排期**)。

**② 关键发现(决定裁决的三个事实)**

| # | 发现 | 证据 |
|---|------|------|
| **F-1** | **`moesi_gpu` 是 `perm` 形态演进的**唯一**真实消费者** —— `ADR-CEP-05 §13.1` 判据 2 **已触发**(`moesi_gpu` 的 `state_transition[2][1]=3` = E+Write→M **静默升级**,(a)∧(b) 成立 ⇒ 4 值 `perm` 无法表达);而 CHI RN-F 是**显式**请求升级(`07 号 §5.1.1`)⇒ **不需要**该演进 | `docs/adr/ADR-CEP-05-stub-cache-core-interface.md` §13.1 · `docs/architecture/07-coherence-exploration-platform.md` §5.1.1 |
| **F-2** | **ArchForge 是纯设计仓、零代码**(`find` 搜 `*.slicc`/`*.pcc`/`*.cc` → 0 命中)⇒ `MoesiGpuAdapter` **从零实现**;且 `mesif` **无状态机规约**、**无 5→4 投影表**,依赖**尚不存在**的第 6 种 SharerTracker `HierarchicalDirectory`(R-1) | ArchForge `32 号` §6.1/§6.2 · `25 号` §4.1 · `09 号` §3 |
| **F-3** | **`perm_upgrade` 直接加进 `protocol_instances.json` 会让镜像闸当场红** —— ⑥ 项要求本仓派生副本与 ArchForge 声明**逐字段严格相等**(含「键只存在于一侧」判红)⇒ 该文件**不得**承载本仓单方字段 | `scripts/test/cep_decl_alignment_check.sh:361` |

⇒ **F-1 的推论**:若 `chi-rnf-subset` 先行做 `perm` 演进 ⇒ schema 在**无消费者**的真空里演进 = `Entry #58` 已明确拒绝的形态(「M-4 前置不能与实现分离 —— 裁决在真空里会被实现目标带偏」)⇒ **必须重排**。

**③ DRI 三问裁决(2026-10-26,3/3 全取推荐项)**

| 问 | 裁决 |
|---|---|
| ① `moesi_gpu` / `mesif` 排在哪里? | **插入 Stage 1 前段**(原 #6 顺延为 #7,`mesif` 新增为 #8)—— 而非追加到全局末尾 |
| ② **S1-7 ①②**(`perm` 形态演进)归谁? | **跟 `moesi_gpu` 走** —— 跟唯一真实消费者走 |
| ③ **S1-5**(6→3 映射表)归谁? | **跟首个 adapter 走** —— 跟第一个有真实行的实例走 |

**④ 排队重排(§4.0 触发源)**

| # | Change | Stage | 季度 | 相对原队列 |
|---|--------|-------|------|-----------|
| **6** | `cpptlm-cep-moesi-gpu-adapter` | Stage 1 | **2028-Q1** | **新增**(插在原 #6 之前) |
| **7** | `cpptlm-cep-chi-rnf-subset` | Stage 1 | **2028-Q2** | 原 #6 **顺延** |
| **8** | `cpptlm-cep-mesif-adapter` | Stage 1 | **2028-Q3** | **新增**(排 #7 之后) |
| **9** | `cpptlm-cep-cxl-nonsymmetric` | Stage 1 | **2028-Q4** | 原 #7 顺延 |
| **10** | `cpptlm-cep-tilelink-tree-manager` | Stage 1 | **2029-Q1** | 原 #8 顺延 |
| **11** | `cpptlm-cep-cxl-dcoh-symmetric` | Stage 2 | **2029-Q2** | 原 #9 顺延 |
| **12** | `cpptlm-cep-nvlink-sharp` | Stage 2 | **2029-Q3** | 原 #10 顺延 |
| **13** | `cpptlm-cep-ualink-inc` | Stage 3 | **2029-Q4** | 原 #11 顺延 |

**⑤ 五处同改(per `obj §4.0` 排期调整规则,逐处「命中 1 次」断言通过)**

| # | 落点 | 改什么 |
|---|------|--------|
| ① | `obj §4.2` ~ `§4.4` | 顺序号 + 季度 + Stage 1 标题「3 协议 RN-F Subset」→「**5 协议实例**」+ 跨度 **9** 个 change |
| ② | `obj §4.6` | 终态 2029-Q4;「串行 **13** 季度」(保留 Entry #12 的 10 → 11 历史注记) |
| ③ | `phase-10.md` | 排期真源指针 / KPI 预算(Stage 1 7 → 9 / 总计 11 → 13)/ 图注 / 依赖图协议实例行(+ 顺带订正队列 #5 的状态漂移 🟡 → ✅) |
| ④ | `obj §10` | Stage 1~3 远期候选的顺序指针(+2 行) |
| ⑤ | `.rddf/roadmap.md` | 「跨 **13** 季度治理叙事」 |
| — | `obj §4.0` 表头 / 口径段 / 终态行 / 13 行表体 | 触发源(不算入「五处」) |
| — | `obj §4.5` / `§6.2` | Stage 4+ 起点 / 验收季度 |

**⑥ 准入前置 owner 重指向**

| 前置 | 原状态 | 新状态 |
|---|---|---|
| **S1-2**(首个 `implemented` 实例装配后复核 per-network 嵌套形态) | ⛔ 未触发 | **🟡 由队列 #6 触发**(本 change = 首个 `implemented` 实例) |
| **S1-5**(6→3 映射表 + contract test) | 队列 #6 | **队列 #6 交付**(跟首个 adapter) |
| **S1-7 ①②**(`perm` 形态 schema 演进) | 队列 #6 | **队列 #6 交付**(跟唯一真实消费者) |
| **S1-3**(第 3 张网 / 第 3 个协议实例) | ⛔ 未触发 | ⛔ 未触发,但**触发点前移至 #8**(`moesi_gpu` + `chi-rnf` + `mesif` = 第 3 个) |
| **S1-1 ②** / **S1-4** / **S1-7 ③ (β)** | ✅ 已消费 | ✅ 已消费(不动) |
| **孤儿**:`moesi_gpu` / `mesif` 未排期 | ⏭(孤儿)登记在 #6 行 | **✅ 消解** —— `moesi_gpu` → #6;`mesif` → #8(**显式标注 R-1 阻塞**) |

**⑦ 冻结面授权(本 entry 即授权记录)**

| 成员 | 改什么 |
|------|--------|
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | §2 决策 3 的 Stage 1 名单 **7 → 9** + 总计 **11 → 13** + §5 决策汇总同步 + **追加 Status Update 2026-10-26**(队列 #6 已启动 + 排队扩至 13) |
| `docs/adr/ADR-CEP-05-stub-cache-core-interface.md` | **追加 Status Update §15**(S1-7 落地载体确认 + 「本批不改」口径澄清 + §14.1 旧算术限定) |
| `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` | **追加 Status Update §10**(`perm_upgrade` 落点裁决 + 6→3 映射表首个载体) |
| `docs/adr/ADR-CEP-13-coherence-domain-ownership.md` | **追加 Status Update §9**((b) 成立后的首个可执行载体) |
| `docs/adr/ADR-CEP-NUMBER-MAP.md` | **KPI 预算块** Stage 1 **7 → 9** / 总计 **11 → 13** / 串行 **11 → 13** + 追加 2026-10-26 增补块 |
| 其余 **34** 个冻结成员 | **0 diff** |

> ⚠️ **本表最初写「其余 38 个 0 diff」有误** —— 4 份追加了 Status Update 的 ADR 与 NUMBER-MAP
> **也是冻结成员** ⇒ 实际变动 **5** 个,非 1 个。已订正(评审前自查,per ⑫)。

⇒ 走**两步授权**:先 commit 产物 → `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #71"` → 再 commit 清单。

**⑧ planner 阶段产出(planner 四件套;`tasks.md` 属 builder 阶段)**

| 载体 | 内容 |
|---|---|
| `proposal.md` | Why / What Changes / 冻结面 / Non-Goals / 依赖与风险 |
| `design.md` | **12 个 D-段裁决**(D-α 队列位置 … D-μ 跨实例复用边界;α→μ 共 12 个希腊字母 —— **原写「13 个」经 critic 复审实测订正**,`grep -c "^## D-"` = 12) |
| `specs/cep-moesi-gpu-adapter/spec.md` | **7 Scenario** |
| `specs/cep-adapter-schema-contracts/spec.md` | **7 Scenario** |
| `.rddf/improvements/cpptlm-cep-moesi-gpu-adapter.md` | 5 段草稿(Why / What Changes / Non-Goals / Acceptance / Capabilities / Impact) |

**⑨ 3 个跨仓登记(诚实登记,不掩盖)**

| 项 | 状态 | 对本 change 的影响 |
|---|---|---|
| **`perm_upgrade` 的镜像冲突** | 由 design **D-β** 化解 | 落**独立声明源** `perm_upgrades.json` + schema ⇒ `protocol_instance.schema.json` 与 `protocol_instances.json` 的实例键 **零改动** ⇒ ⑥ 镜像闸不受影响;`protocol_instance.schema.json` 的 13 属性断言零回归 |
| **`status` 翻转的跨仓同步** | design **D-ζ** 化解 | ⑥ 项**排除 `status` 字段**(本仓权威:「本仓是否已实现」)⇒ 跨仓同步**不作为前置**;守卫收窄 ⇒ **须有反向 fixture**(其余 12 键仍严格镜像) |
| **C-1 / D-9 批 2 / R-1** | 既有债 | 登记在案:本仓实施**不阻塞**;`mesif` 的 R-1 阻塞在 #8 行显式登记 |

**⑩ 验收(planner 阶段)**

`openspec validate cpptlm-cep-moesi-gpu-adapter --strict` **rc=0** · delta spec **14 Scenario**(7 + 7)· KPI 实测 **1 个 active** ·
守门 7 项 rc=0 · `cep_gate_selftest` 全绿(锚点 #6 已换名)· `openspec validate --changes --strict` rc=0 · 冻结集 **39**(成员数不变,**5** 个成员的 hash 变:ADR-CEP-00 / 05 / 12 / 13 / NUMBER-MAP)

**⑪ 关联**: Entry #70(队列 #5 归档)· #69 / #68(队列 #5 三段评审)· #67(队列 #5 builder)· #66(队列 #5 planner)· #58(M-4「真空裁决」先例)· **ADR-CEP-05 §13.1**(`perm` 形态约束的权威)· **ADR-CEP-12 §8.3**(6 类词表与 `CATEGORY_*` 的分层纪律)· **ADR-CEP-02 CEN-Ib11**(`perm` 4 值锁)

**⑫ 评审前自查发现的 8 处缺陷(全部已修,登记备查)**

> 触发:context-builder 组装 critic 上下文时对 `git diff` 做独立复核,发现 8 处我方遗漏
> ⇒ **修复后**再送 critic 独立评审(不让 critic 重复报已知项)。

| # | 缺陷 | 性质 | 修复 |
|---|------|------|------|
| **R-1** | **`obj §4.0` 表体 15 行**(应 13)—— 多出 2 行**重复的 #9/#10**且**列数不足**(4 列 vs 表头 6 列);`§4.2` 反而**缺** #9/#10 | **硬缺陷**(我自己引入) | §4.0 删 2 行 + §4.2 补回 2 行;**机器闸 `_rows()` 不校验列数 ⇒ 全绿时也漏过** ⇒ 闸盲区登记 |
| **R-2** | **`obj` 活区 15 处旧数**(theme / 标题 / 关联变更 / Goal / DoD / §1.1 / §2.2 / §2.3 / §4.0 注 / §5.4 / §6.2 B1 / §6.3 D2 / §8.3)⇒ 「五处同改」**只覆盖了 §4.0 触发的 5 处**,未覆盖 objective **正文自身**的活区 | 旧数复活 | 逐处同步为 13 / 2029-Q4;历史说明块(`:30` planner 阶段同步)与自检 grep 样例(`:201`)**保留** |
| **R-3** | **断言 ⑧(S1 有效期)两个届满条件都因改名而恒假**:① 硬编码旧目录名 `chi-rnf-subset`(改名后永不存在)② 键名 `perm_form`/`perm_variant`/`upgrade_kind` 与队列 #6 实际采用的 `perm_upgrade` **不匹配** | **闸静默失守** | ① 改为「队列 #6 的 perm 演进 change **已归档**」(「**开始做** ≠ **做完**」,与 `Entry #64` ③放行闸解除条件同构)② 补入 `perm_upgrade` 键名 |
| **R-4** | **`ADR-CEP-NUMBER-MAP` 的 KPI 预算块是「活口径」**(非历史快照)却仍写 7 / 11 / 11 | 旧数复活 | 同批同步 9 / 13 / 13 + 追加增补块 |
| **R-5** | **冻结面授权表不实**:原写「其余 **38** 个成员 0 diff」,实际变动 **5** 个 | 授权记录不完整 | 授权表列全 5 个 + 「其余 34 个 0 diff」 |
| **R-6** | **`cep_freeze_check.sh` 的 provenance 不成立只 warn 不阻断**(rc=0)⇒ 两步授权的第 3 步「再 commit 清单」**无机器保证** | 闸无牙 | 非 `--update` 模式下 provenance 不成立 ⇒ **rc=1**(`--update` 模式天然豁免:此刻产物未提交,provenance 必然不成立) |
| **R-7** | **proposal 数字/路径错**:「共 18 条 Scenario」实际 13;「后续 5 个 adapter」实际 7;delta spec 路径写成 `openspec/specs/…`(实为 `openspec/changes/…/specs/…`);「16 个注册实例」与本仓声明源仅 2 份实例**粒度混用** | 事实性错误 | 逐项订正;「16」明确标注为 `07 号 §5.1` 的**设计口径** |
| **R-8** | **design D-γ 的 `$ref` 措辞过强**:称「**机制上**不可能表达 5 值」,但仓内**无 JSON-Schema validator**(实测 `grep -rn jsonschema` 零命中)⇒ `$ref` 是**声明性**约束 | 声明强度失真 | 改为「**判据层**不可能」,并注明执行靠 AC-01 ② 的手写判据 |

> **R-1 的教训**:`§4.0` 的「五处同改」规则管的是**跨文件**同步,而**同一张表内部的行结构错误**不在其覆盖范围
> ⇒ `_rows()` 只按 `|` 抓行、**不校验列数** ⇒ 一张 15 行 / 列数错乱的表能**全绿通过**所有机器闸
> ⇒ **表格结构需人工核对**(闸只保证「标记在两处出现」,不保证「表是合法的表」)。

---

### Entry #72 — 2026-10-26 [fix]

**主题**: 队列 #6 planner 产出 **critic 对抗式评审轮** —— **REJECT**(2 BLOCKER + 8 SHOULD_FIX + 6 NIT)全部落地

**① 评审结论**

critic 独立评审(仓外 AI agent,**不等于真人外部评审** —— per `obj §9.3` R10 先例,本结论应登记为「残余风险 + DRI 书面接受」,而非「已闭环」)。
判定 **REJECT**。评审前先由 context-builder 组装上下文并做了一次独立 diff 复核 ⇒ 挖出 8 处我方遗漏(R-1~R-8,已先修),再送 critic ⇒ critic 另挖 2 BLOCKER + 8 SHOULD_FIX + 6 NIT。

**② 2 个 BLOCKER**

| # | 缺陷 | 修复 |
|---|------|------|
| **B-1** | **design D-γ 的承重证据是假的** —— 原文称「仓内**无** JSON-Schema validator(实测 `grep -rn jsonschema` 零命中)」;critic 反证:仓内**有**(`scripts/test/cep_decl_alignment_check.sh:98/122/170` 用 `jsonschema.Draft202012Validator`;CI pip 已装;缺依赖时**不降级运行`)。**我的测量方法错了** —— grep 只搜了 `test/` + `CMakeLists.txt`,**漏搜 `scripts/`** ⇒ 假测量 ⇒ 设计结论被错误证据带偏(误以为「无机制」而放弃了本可拿到的**机制级**保证);且 spec 仍写「**机制上**不可能」而 design 已改口「判据层不可能」⇒ **验收面(spec)与设计在同一句话上互相矛盾** | ① design D-γ 的证据链**改正**:仓内**有** validator ⇒ 机制级保证**可达**(builder 把 `perm_upgrades.schema.json` 接入既有设施 + `referencing` registry 解析跨文件 `$ref`)⇒ 「机制上不可能」**成立**;② spec AC-01 ② 补**前提**(接入前只是声明性);③ proposal 风险表同步口径 |
| **B-2** | **R-5 的修复未传播** —— 台账 Entry #71 §⑦ 已订正为「5 个冻结成员变动」,但**同批产物**三处仍是旧错误口径:`proposal.md` 冻结面表(只列 1 个 + 「其余 **38** 个 0 diff」,且**修改表漏列** NUMBER-MAP)、`improvements` 同款、Entry #71 ⑩(「仅 ADR-CEP-00 的 hash 变」)⇒ 且 proposal **自己的修改表**与**自己的冻结面表**自相矛盾 ⇒ ⑫ 段「8 处全部已修」的登记**不实** | proposal 冻结面表列全 5 个 + 「其余 **34** 个 0 diff」;proposal / improvements 的**修改表**补 NUMBER-MAP;improvements Impact 表冻结面口径改 5 个;Entry #71 ⑩ 改「5 个成员的 hash 变」 |

**③ 8 个 SHOULD_FIX**

| # | 缺陷 | 修复 |
|---|------|------|
| **S-1** | 断言⑧ 条件② **查错文件** —— 仍读 `protocol_instance.schema.json` 找 `perm_upgrade`,而 design D-β 的裁决正是该文件**零改动** ⇒ 条件② **恒假**(与旧键名一样装饰性) | 改为**两个**候选文件任一命中即算演进已发生(`protocol_instance.schema.json` / `perm_upgrades.schema.json`) |
| **S-2** | 断言⑧ 条件① 不区分「✅ 完成归档」与「⏸ 暂存归档」 —— 暂存归档下 perm 演进**未落地**却照样宣布「有效期届满」 | 额外要求 **完成归档**语义 = delta capability **已并入** `openspec/specs/`(暂存归档不并入)⇒ 用 main spec 存在性判定 |
| **S-3** | §4.0 的「**双边化自检**」块没随 11→13 重指 —— 自检② 仍 grep `2029-Q1` / `串行 10 季度` / `10 个 change`(那是 10→11 那轮的旧值),本轮新旧值一条没查;自检① 也没加新终态 `2029-Q4` | 自检① 改 `2029-Q4`;自检② 改 `2029-Q2` / `串行 11 季度` / `11 个 change` |
| **S-4** | `obj:457` 活区残留「下一个可启动 = 队列 #5 `cpptlm-cep-collective-plane-base`」—— 队列 #5 已归档、#6 已建目录 ⇒ **双重错误** | 改「下一个可启动 = 队列 #7 `cpptlm-cep-chi-rnf-subset`」 |
| **S-5** | `cpptlm-cep-umbrella.md` 活区旧数(:161 「11 季度」/ :307 「后续 **10** 个 change」/ :308 「占用 **11** slots,跨 **11** 季度」)—— 本批改了该文件却漏扫同文旧数 | 三处同步为 13 / 12 / 13 |
| **S-6** | **本批自己引入单源纪律违反** —— improvements 把具体季度值「终态 2029-Q2 → **2029-Q4**」复制到 objective 之外,而 design D-α 自己重申「improvements 一律只写『见 §4.6』」 | 两处改为「见 `obj §4.6`」 |
| **S-7** | `phase-10.md:44` 准入前置归属多列 #7 —— §4.0 的 #7 行明确写「**新准入前置：无**」 | 归属改为 #2 / #4 / #6 / #8,并注明 #7 不持有任何 S1-x |
| **S-8** | **⑥ 闸排除 `status` 的收窄没有 Scenario 级反向 fixture** —— design D-ζ 写了「必须有反向 fixture」,但 13 个 Scenario 无一条承载它;AC-03 只覆盖 `mesif` 注入;既有 `cep_decl_reverse_verify.sh` 不含 `status` 用例 ⇒ **收窄一个既有闸而不写进验收面 = builder 可静默漏做且归档仍绿** | **补第 7 个 Scenario**(`cep-adapter-schema-contracts`):**正控**(仅 `status` 不同 ⇒ ⑥ 不红)+ **反向 fixture**(改任一非 `status` 键 ⇒ 必红;增删实例 ⇒ 必红)+ **须登记进 `cep_decl_reverse_verify.sh`**(不得只存在于 spec 文本)⇒ Scenario 数 13 → **14** |

**④ 6 个 NIT(全部落地)**

N-1 proposal「其余 13 字段零改动」→ **12**(实例共 13 键,`status` 翻转 ⇒ 余 12)· N-2 `roadmap.md` 加 2026-10-26 更新注(不与旧日期注并置)· N-3 `role.json` 时间戳单调(不再倒序)· N-4 §4.0 状态口径行补「📋 Planned 行**可**在末列标注外部阻塞」(`登记 ≠ 就绪`:排期位置是计划,阻塞标注是债的可见性)· N-5 proposal 风险表补「**6×6 全表漂移缺口**」行(D-ε 已在 design 登记,风险应浮到 proposal 层)· N-6 两份 Requirement 正文压缩(503/488 → **478/449**,留余量给 builder 增字;`openspec validate --strict` rc=0 —— **实测 ±1 差异**是尾换行计数口径,per critic 复审)

**⑤ 复审前自查(修完 critic 意见后)**

| 项 | 结果 |
|---|---|
| 守门 7 项 | rc=0 |
| `cep_gate_selftest` | **31/31** |
| `openspec validate --changes --strict` | rc=0 |
| `ctest` | **106/106** |
| delta spec | **14 Scenario**(7 + 7) |

**⑥ 关联**: critic 评审轮(REJECT:2 BLOCKER / 8 SHOULD_FIX / 6 NIT)· ⑫ 段的 8 处评审前自查缺陷(R-1~R-8)· Entry #70(队列 #5 归档)· `ADR-CEP-05 §13.1` · `ADR-CEP-12 §8.3` · `ADR-CEP-02 CEN-Ib11`

---

### Entry #73 — 2026-10-26 [fix]

**主题**: 队列 #6 critic **复审轮** —— **APPROVE_WITH_CONDITIONS**(无 BLOCKER;上轮 16 项独立验证为真修)→ 本轮 3 SHOULD_FIX + 5 NIT 落地

**① 复审结论**

critic 复审判定 **APPROVE_WITH_CONDITIONS**,**无 BLOCKER**。critic **独立重跑了全部声称的验证**(未采信描述):`openspec validate --strict` rc=0 · `--changes --strict` rc=0 · `openspec list` = 1 active(KPI 1/3)· 守门 4 项 rc=0 · `cep_freeze_check` rc=0 + provenance 39 成员 · `cep_decl_alignment_check` 6 项全绿 · `cep_gate_selftest` **31/31** · `ctest` **106/106** ⇒ Entry #72 ⑤ 的自查表**全部属实**。

> **独立性标注**:本轮复审由 AI agent 完成,**不等于真人外部评审** ⇒ 结论应登记为「**残余风险 + DRI 书面接受**」,而非「已闭环」(per `obj §9.3` R10 先例)。

**② 上轮 16 项(2 BLOCKER + 8 SHOULD_FIX + 6 NIT)的独立验证结论**

| # | critic 的独立验证 | 结论 |
|---|-----------------|------|
| **B-1** | `design.md:76` 证据链已改正(`:98` import、`:122/:170` validator、CI pip 实测 `.github/workflows/ci.yml:56-57`);spec AC-01 ② 已补前提;`grep "机制上\|判据层"` 三处命中**互相自洽**,无残留矛盾 | ✅ 真修 |
| **B-2** | `git diff 88e6b88b..ea2ec6e7 -- docs/adr/ \| grep -c "^diff --git"` = **5**;manifest 恰 5 个 hash 变;proposal「其余 **34** 个」算术自洽(39−5=34);全仓旧口径仅剩 `obj:4702` / `obj:4768` 两处**合法历史记录** | ✅ 真修 |
| **S-1~S-8** | 逐条独立验证(见 Entry #72 的修复清单) | ✅ 真修 |
| **N-1~N-6** | 同上 | ✅ 真修 |

**③ 本轮新发现 3 个 SHOULD_FIX(全部已修)**

| # | 缺陷 | 修复 |
|---|------|------|
| **SF-1** | **台账登记不实(与 B-2 同类,出现在同一份台账里)** —— Entry #71 ⑧ 写「**13 个** D-段裁决」,实测 `grep -c "^## D-"` = **12**(α→μ 共 12 个希腊字母)。**两轮评审均漏** | 订正为 **12** 并在台账内注明「原写 13,经 critic 复审实测订正」 |
| **SF-2** | **§4.0「双边化自检」块的「已知例外(实测 1 处)」清单未随模式重测** —— 模式已正确重指(①`2029-Q4` 0 命中、②`2029-Q2` 0 命中),但 ③ 的 `串行 11 季度\|11 个 change` 实测 **7 处**命中,**全部**在 `docs/adr/` 的 Status Update **历史引述**段 ⇒ 操作句「除这 1 处外出现新命中 = 旧数复活,必须改回」会**误导**照做的人去改历史快照 | 例外清单按**实测**重写:① 0 处 / ② 0 处 / ③ **7 处**(逐个列出文件行号)+ ④ 草稿 1 处;并把操作句改为「除 ③ 的 7 处 + ④ 的 1 处外」 |
| **SF-3** | **第 7 个 Scenario 的反向 fixture 键枚举漏 `atomic`** —— 原文列 11 个非 `status` 键,而 schema 的 `required` 是 **13** 键(含 `atomic`)⇒ 措辞会被 builder 当**穷举清单**照抄 ⇒ `atomic` 键的镜像漂移**无 fixture 钉住**(闸本体是全键并集比较,不漏;缺的是 fixture 枚举的完备性) | 改为「改 `status` 之外**全部 12 键**中的**任一**」+ **逐个列出** 12 个键名 + 注明「穷举清单 = schema 的 `required` 去掉 `status`;**不得**只挑其中几个」 |

**④ 本轮 5 个 NIT(全部落地)**

| NIT | 处理 |
|-----|------|
| `:98/122/170` 的行号引用不精确(`:98` 实为 `import jsonschema`) | design / spec / proposal 三处改为「`:98` import + `:122/:170` 的 `Draft202012Validator`」 |
| `obj:457` 历史链尾句「下一个可启动 = 队列 #5」缺「当时」锚定 | 补「**当时**(2026-10-23)」锚定前缀 |
| 断言⑧ 条件② 的**未来键名**待 builder 落地后重验 | 登记进 design **D-θ** + proposal 的「不改清单」旁 ⇒ **builder 落地 `perm_upgrades.schema.json` 后必须重验** |
| Entry #72 声称 Requirement 正文 479/450,实测 478/449 | 订正并注明「±1 差异是尾换行计数口径」 |
| critic prompt 称 26 文件、实测 21 | **prompt 记述误差**,非仓内缺陷 ⇒ 不改 |

**⑤ 两条「未能完全验证」⇒ 已登记为 builder 前置(critic 提出)**

| # | 项 | 登记落点 |
|---|---|---|
| ① | **S1 有效期届满的**重登记 owner** —— 断言⑧ 的条件① 在 #6 **完成归档**瞬间转红,报红信息指向 `cep-poc-spike-verification` main spec 的 S1 有效期登记,而该 main spec **不属任何 active change** ⇒ **builder 必须在 `tasks.md` 显式列「归档前完成 S1 有效期重登记」任务**,否则 #7 planner 开门即遇红闸且**无 runbook** | design **D-θ** + proposal「不改清单」旁 + 本 entry |
| ② | **条件② 的未来键名** —— `perm_upgrades.schema.json` 的顶层 `properties` 须真含 `perm_upgrade`,否则条件② **再度装饰化**(条件① 的归档判定仍兜底,非恒假) | design **D-θ** + 本 entry |

**⑥ 验收(实测)**

守门 7 项 rc=0 · `cep_gate_selftest` **31/31** · `openspec validate --changes --strict` rc=0 · `ctest` **106/106** · delta spec **14 Scenario**(7 + 7)· 冻结集 **39**(本批冻结成员零 diff)· KPI **1/3**

**⑦ 关联**: critic 复审轮(APPROVE_WITH_CONDITIONS:3 SHOULD_FIX / 5 NIT)· Entry #72(critic 首轮 REJECT 的修复)· Entry #71(排队重排 + planner 四件套)· `obj §9.3`(R10 独立性缺口登记先例)

---

### Entry #74 — 2026-10-27 [scope-change]

**主题**: 队列插入 —— **新增 #7 `cpptlm-cep-cohif-carrier`** + 排队 **13 → 14** change / 终态 → **2030-Q1**;登记 architect 重审结论 + 授权 `ADR-CEP-19` 签发轮(冻结面 **39 → 40**)

**① 触发**

architect 角色的重审结论(per 用户指令「切 architect 重审队列 #6 planner 产出是否符合 `01 号` 核心立场:① `ch_stream` 承载业务类 bundle;② 互联网络可特化到具体通信网络」),经 **context-builder + critic 四轮**对抗式评审
(REJECT 2 B + 10 S + 7 N → APPROVE_WITH_CONDITIONS ×3 → `mergeable: yes`)后由用户裁决「**新开载体队列,插在 #6 与 #7 之间**」。
产出载体 = `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`(v3,420 行)+ **`docs/adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md`**(v1.0 已签发;签发前的草案形态为 `docs/research/cep-adr-19-cohif-carrier-draft.md`,**已随签发移除**),5 个 commit `805c8f95 → 07e0e02d → c37667a5 → 86c9ae4f → c45a2bdb`。

> **本 entry 同时履行 architect 重审的「签发前置第 0 项」** —— 架构重审文档 §9 自定纪律:「本轮 architect 重审自身必须有台账 Entry,否则与该文 D-8 批评队列 #6 未登记偏离的标准不一致」。**本 entry 即该登记**。

**② 三个架构断点(全部 `file:line` 实测,构成本队列的立项依据)**

| # | 断点 | 证据 |
|---|---|---|
| **C1** | **CEP 零 ChStream 承载体** | `grep -rl "bundles::\|Bundle\|ChStream\|StreamAdapter" include/tlm/cep/` → **空**(7 个 `.hh`);`stub_cache_core.hh:154` 实为普通 `class StubCacheCore` —— 而 `ADR-CEP-07:64/67` 与 `07 号 §6.3` 明写 ProtocolEngine / StubCacheCore **都应是** `ChStreamModuleBase` |
| **C2** | **「canonical 10 流的 Bundle」是悬空引用,且同源两处** | `ADR-CEP-07A:49` + `ADR-CEP-16:45` 均写「CEP 用 canonical 10 流的 Bundle(per `ADR-CEP-02` / `07 号 §5.4`)」;但 `ADR-CEP-02` 全文 `Bundle` **零命中**、`07 号 §5.4` 业务 Bundle 列表(7 项)**无它**、`include/bundles/` 17 个 `.hh` grep `coherence` **零命中** |
| **C3** | **唯一的 Bundle 化承诺绑在集合通信队列** | `ADR-CEP-03:225` 推迟至 P3(`cpptlm-cep-nvlink-sharp-adapter`),新建 `collective_bundles_tlm.hh` ⇒ **主干流无人认领** |

**③ 新队列的交付面(3 项 + 1 宏 + 1 显式延期)**

| # | 交付物 | 承接来源 |
|---|---|---|
| 1 | `include/bundles/cep_coherence_bundles_tlm.hh` —— `bundles::CohIfBundle`(**只覆盖 5 条 RN-side 流**,`flow_id` 区分;无网络层字段) | `06-bis §5.2~§5.4` |
| 2 | `include/tlm/cep/protocol_engine_base.hh` —— `ChStreamModuleBase` 派生 + `Input/OutputStreamAdapter<CohIfBundle>` | `ADR-CEP-07:64` + `router_tlm.hh:136-142` 范式 |
| 3 | `include/framework/protocol_bridge.hh` —— `ProtocolBridgeEr` + `ProtocolBridge<InB,OutB,Ordered>`(**首次落地**;`CohIfBundle ↔ NoCFlitBundle`) | `06 号 §4.4` |
| 4 | `REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)` 宏(**`registerObject` + `registerBidirectionalPortAdapter` 两件都做**;`static_assert(n>1)`)+ `module_factory.hh:313-315` 钩子扩展(死钩子 → 活钩子) | `06 号 §9.3` + `chstream_register.hh:63/:119` |
| **延期** | `StubCacheCore` 的 ChStream 端口面 → `cpptlm-cep-stub-cache-chstream-surface` | 四条互斥(见 ④) |

**关键裁决**(详 `ADR-CEP-19` 草案 D-1~D-9 ↔ 决策 1~9 映射表,在审查报告 §5):

- **装配走 JSON 链,不走 programmatic** —— **`ADR-CEP-16:46/:86` 明写「不做 / 不实现 programmatic bridge 的 C++ 路径」**,且实测 programmatic 创建 ChStream 端口**仓内零先例**;JSON 链全程实测通(`registerBidirectionalPortAdapter:103` → `port_count_=N:108` → `isMultiPort:59-61` → `module_factory.cc:668-669` → `:743-750` **泛型虚分派** `bind_port_pair`;硬编码 cast 只在 `is_dual` 分支 `:727-741`)
- **`CohIfBundle` 归业务语义、按网络级纪律演进** —— `06 号 §3.1` 的两条判定规则对 CohIf 给出相反答案;落点是 `06 号 §3.3` 首行「模块 owner 可改?」加第三类
- **不动 `StubCacheCore`(决策 9)** —— 四条互斥:`cep-stub-cache-core/spec.md` 在 `FROZEN_GLOBS`(`cep_freeze_check.sh:70`)+ 该 spec 写「`net_side` SHALL 由 **mock ProtocolEngine** 驱动」+ 三流名「SHALL **不**出现作为 stub 对外接口」+ `stub_cache_core.hh:39-40`「**不**经 `ModuleFactory` 注册」⟂ `ADR-CEP-07:67`「应该 `registerObject`」

**④ 排队重排(§4.0 触发源)**

| # | Change | Stage | 季度 | 相对原队列 |
|---|---|---|---|---|
| **6** | `cpptlm-cep-moesi-gpu-adapter` | Stage 1 | 2028-Q1 | 不变 |
| **7** | `cpptlm-cep-cohif-carrier` | Stage 1 | **2028-Q2** | **新增**(本 entry) |
| **8** | `cpptlm-cep-chi-rnf-subset` | Stage 1 | 2028-Q3 | 原 #7 顺延(首次在真实载体上验证) |
| **9** | `cpptlm-cep-mesif-adapter` | Stage 1 | 2028-Q4 | 原 #8 顺延 |
| **10** | `cpptlm-cep-cxl-nonsymmetric` | Stage 1 | 2029-Q1 | 原 #9 顺延 |
| **11** | `cpptlm-cep-tilelink-tree-manager` | Stage 1 | 2029-Q2 | 原 #10 顺延 |
| **12** | `cpptlm-cep-cxl-dcoh-symmetric` | Stage 2 | 2029-Q3 | 原 #11 顺延 |
| **13** | `cpptlm-cep-nvlink-sharp` | Stage 2 | 2029-Q4 | 原 #12 顺延 |
| **14** | `cpptlm-cep-ualink-inc` | Stage 3 | **2030-Q1** | 原 #13 顺延 |

**算术**:13 → **14** change / **14** 季度;**Stage 1 = 9 → 10** 个(2027-Q1 ~ 2029-Q2);`1 + 10 + 2 + 1 = 14`;终态 2029-Q4 → **2030-Q1**。

**⑤ 五处同改 + 连带落点(逐处带「命中 1 次」断言)**

| 组 | 落点 | 改什么 |
|---|---|---|
| **①** | `obj §4.2` | 标题(队列 #2 ~ **#11** / 2027-Q1 ~ **2029-Q2**)+ 表内插 #7 行 + #7~#10 顺延 + 跨度 **9 → 10** 个 |
| **②** | `obj §4.6` | 实施完成 / 维护期 / 终态验收 **2029-Q4 → 2030-Q1**;串行 **13 → 14** 季度 |
| **③** | `phase-10.md` | 真源指针 **13 → 14**;KPI 预算 Stage 1 **9 → 10** 串行、总计 **13 → 14**;依赖图头 + **插入 carrier 节点**;图注改「串行规则与季度跨度见 `obj §4.6`」(去季度数副本,单源纪律) |
| **④** | `obj §10` | 远期候选表**插入 carrier 行** + §4.0 顺序号 #7~#10 顺延;标题 2029-Q4 → 2030-Q1 |
| **⑤** | `.rddf/roadmap.md` | 「跨 **13** 季度治理叙事」→ **14** + 追加 2026-10-27 更新注 |
| — | `obj §4.0` | 表头 **13 → 14** 个 change;口径段「14 个 change,串行 14 季度」;表体 **插 #7 行 + 7 行顺延**;终态行;注「14 = 1+10+2+1」 |
| — | `obj §4.3 / §4.4 / §4.5` | 队列 #12~#13 / #14;季度;**Stage 4+ 起点 2030-Q1**;ADR 清单 **21 → 22**(待签发) |
| — | `obj §1.1 / §2.2 / §2.3 / §5.4 / §6.2 / §6.3 / §8.3` | 活区**旧数逐处同步**(theme / 标题 / 关联变更 / Goal / DoD / 叙事 / 数量口径 / 串行 / 验收标题 / B1 / D2 / 归档清单) |
| — | `obj §5.2` | 「下一个可启动 = 队列 #7 **`cpptlm-cep-cohif-carrier`**」(原为 chi-rnf) |
| — | `obj §4.0` 自检块 | **双边化模式重指**:① 新值 `2030-Q1\|2028-Q4`;② 旧值 `2029-Q4` + `串行 13 季度\|13 个 change`;**例外清单按实测重写**(6 处,见文档内) |
| — | `.rddf/improvements/cpptlm-cep-umbrella.md` | 后续 **12 → 13** 个 change;**13 → 14** KPI slots / 季度 |
| — | `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md` | C3 段落加**时态锚定**(「当时(2026-10-26,13 个 change 队列)」)⇒ 避免被当现行口径 |

**⑥ 冻结面授权(本 entry 即授权记录;成员 **39 → 40**,改动 **7 + 新增 1**)**

| 成员 | 改什么 | 授权 |
|---|---|---|
| **`docs/adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md`** | **新增签发**(草案现居 `docs/research/cep-adr-19-cohif-carrier-draft.md`,9 条决策) | **Entry #74** |
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | **§2 决策 3**:Stage 1 名单 **9 → 10**、总计 **13 → 14**、算术 **1+9+2+1 → 1+10+2+1**、串行 **13 → 14** 季度;§5 汇总同步;**追加 Status Update 2026-10-27** | 同上 |
| `docs/adr/ADR-CEP-16-06-evolution-route-mapping.md` | **追加 Status Update**:① `:45` 的「canonical 10 流的 Bundle」= **C2 第 2 处悬空引用**;② 澄清 `:46` / `:86`「不做 programmatic bridge 的 C++ 路径」**与本队列不冲突**(本队列走 JSON 端口注册,`RouterTLM` 同款既有路径) | 同上 |
| `docs/adr/ADR-CEP-07A-06bis-artifact-disposition.md` | **追加 Status Update**:决策 1 第 3 项的**替代物偏离**(替代物一直是悬空引用)+ 新路径 `include/bundles/cep_coherence_bundles_tlm.hh` | 同上 |
| `docs/adr/ADR-CEP-07-simulation-and-integration-semantics.md` | **追加 Status Update**:ProtocolEngine 基类的**落地排期** = 队列 #7(原 `§6.3` 表为「最终形态」口径) | 同上 |
| `docs/adr/ADR-CEP-15-cachetlm-alignment.md` | **追加 Status Update**:`§1.2` 表格「Stub Cache Core 基类 = `ChStreamModuleBase`」与 `stub_cache_core.hh:154` **不符**(实测漂移;且本队列决策 9 **不改** stub) | 同上 |
| `docs/adr/ADR-CEP-NUMBER-MAP.md` | **ADR 清单 21 → 22** + **KPI 预算块** Stage 1 9 → 10 / 总计 13 → 14 / 串行 13 → 14 + 追加 2026-10-27 增补块 | 同上 |
| `docs/architecture/07-coherence-exploration-platform.md` | **§8.1 Stage 表**:Stage 1 = **7** / 合计 **11**(**已是旧数**,连 13 都没跟上)⇒ 修订为 10 / 14;追加 Status Update | 同上 |
| 其余 **32** 个冻结成员 | **0 diff** | — |

> **两步授权**:先 commit 产物 → `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #74"` → 再 commit 清单。
>
> **⚠️ 顺序依赖(不可颠倒)**:`docs/architecture/06-inter-module-network.md` 的 3 处修订(§3.3 加第三类 / §4.3 `flit_category` 4→3 值 + §3.2 Bundle 数 16→17 / §11.2·§11.3 时间线转隶 + P3 子项偏离登记)**必须在本 ADR 签发之后** —— 否则是「用未签发 ADR 草案为改真源 1 背书」(审查报告 D-1 顺序依赖)。`06 号` **不在冻结集** ⇒ 无需两步授权,但仍受真源 1 门槛(ADR 触发 / Oracle 评审)约束。
>
> **本轮 architect 职责边界**:`ADR-CEP-19` 签发与上表 7 份冻结成员的 Status Update **均属 architect 角色**;本 entry 只提供**授权记录与改动清单**,planner 不代签。

**⑦ 与 Entry #71 的关系**

Entry #71 建立的是 **13 change 队列**(`moesi_gpu` #6 / `chi-rnf` #7 / `mesif` #8 …)。本 entry **不改其裁决**(三问裁决、`perm` 演进跟 `moesi_gpu` 走、6→3 映射表跟首个 adapter 走**全部保持**),只在其**之后插入载体队列**并整体顺延。⇒ 队列 #6 的 change 目录、proposal / design / 2 份 delta spec **零改动**;其行内对 `mesif` / S1-3 的交叉引用已由 #8 改为 #9。

**⑧ 验收(实测)**

| 项 | 值 |
|---|---|
| `obj §4.0` 表体 | **14 行**;顺序 1..14 无重复;列数全对(6 列) |
| `obj §4.2` 表体 | **10 行**;顺序 2..11 无重复;列数全对(5 列) |
| 守门 7 项 | **全 rc=0** |
| `cep_gate_selftest` | **31 / 31** |
| `openspec validate --changes --strict` | rc=0(唯一 active 仍是队列 #6,本 entry **不新建 change 目录**) |
| KPI | **1 / 3**(不变) |
| 冻结集 | **39**(本 entry 的冻结改动由 architect 签发轮执行;签发后 → **40**) |
| `cep_doc_ref_check` | 4 条**设计示意路径**已登记 `VIRTUAL_PATHS`(`ADR-CEP-19` / `cep_coherence_bundles_tlm.hh` / `protocol_engine_base.hh` / `protocol_bridge.hh`)—— 均为 builder / 签发轮才创建 |

**⑨ 关联**: Entry #71(队列 #6 重排,13 change)· #72 / #73(队列 #6 critic 两轮)· `ADR-CEP-19` 草案(待签发)· 审查报告 `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`(v3)· `01 号 §1.1`(L3+L4 协议中立 + L5↔L4 seam)· `06 号 §4.4`(`ProtocolBridge`)/ §9.3(死钩子)· `06-bis §5`(Bundle 字段基线)

---

### Entry #75 — 2026-10-27 [plan]

**主题**: 队列 #7 `cpptlm-cep-cohif-carrier` **planner 阶段产出(部分落地)** —— improvement 草稿 + 4 件套**草稿**已落,change 目录**未建**(KPI slot 硬前置阻塞)

**① 触发**

用户指令:「起 #7 的 planner」。`ADR-CEP-19` 已于 2026-10-27 签发(Entry #74),队列 #7 `cpptlm-cep-cohif-carrier` 的**架构决策已定**,本 entry 落其 **planner 阶段产出**。

**② 🔴 KPI slot 硬前置阻塞(本 entry 的核心事实)**

**约束**(`ADR-CEP-00 §2 决策 3` 冻结面 + `obj §5.4` + `obj §6.3 D1`):**任一时刻 CEP 系列 Proposed 并发 ≤ 1**。

**实测**(2026-10-27):`openspec list --json` = **1 个 active** = `cpptlm-cep-moesi-gpu-adapter`(队列 #6,🟡 Proposed)⇒ **唯一 slot 已被占用**。

**先例**(本台账早期 entry):「补建 OpenSpec change | ❌ `cpptlm-cep-umbrella` 占**唯一** KPI slot 且 R10 未闭合 → 不 archive → 建新 change 令 CEP 内部 Proposed 并发 = 2,**违规**」。

**⇒ 处置**:按 `obj §4.7.2` step 0(「🔴 **先确认 KPI slot 可用(硬前置,跳过此步必违规)**」)与 `§4.7.3` 的「**先有 improvement 草稿 → 再建 change**」流程 —— **本轮先出 improvement 草稿 + 4 件套草稿(staged 于 `.rddf/drafts/`,照 `.rddf/drafts/cpptlm-pcie-aer-ats-pri/` 先例)⇒ 不占 KPI slot**;**change 目录的创建等队列 #6 归档释放 slot**。

**③ 本轮产出(3 载体,全部不占 KPI slot)**

| 载体 | 路径 | 规模 | 内容 |
|------|------|------|------|
| **improvement 草稿** | `.rddf/improvements/cpptlm-cep-cohif-carrier.md` | 5 段 | Why(三断点 C1/C2/C3 + 排序理由)/ What Changes(3 新 + 2 改 + 冻结面)/ Non-Goals(含「不动 StubCacheCore」的四条互斥)/ Acceptance(6 判据 + 流程治理)/ Capabilities(2 个)/ Impact + **阻塞段** |
| **4 件套草稿** | `.rddf/drafts/cpptlm-cep-cohif-carrier/` | proposal + design + **2 份 delta spec** | `proposal.md` · `design.md`(12 个 `D-` 段:α~λ)· `specs/cep-cohif-carrier/spec.md`(**3** Scenario:CC-01 / CC-02 / CC-04)· `specs/cep-protocol-bridge-contracts/spec.md`(**3** Scenario:CC-03 / CC-05 / CC-06)—— **合计 6 条,与 `ADR-CEP-19` 的 CC-01~CC-06 严格一一对应** |
| **本 entry** | `obj §11` | — | 产出登记 + 阻塞登记 + 转正动作清单 |

**④ capability 划分(2 个,与队列 #6 不冲突)**

| capability | 面 | 判据 |
|------------|---|------|
| `cep-cohif-carrier` | **载体面** —— Bundle 字段契约 + ChStream 端口面 + JSON 装配连通性 | CC-01 / CC-02 / CC-04 |
| `cep-protocol-bridge-contracts` | **特化桥面** —— 桥可逆性 + 死钩子激活(含 per-bridge 元数据) | CC-03 / CC-05 / CC-06 |

> **与队列 #6 的 `cep-adapter-schema-contracts` 不同层**:后者是**协议实例声明层**的契约(`perm` / `perm_upgrade` / 6→3 映射表 / ⑥ 镜像闸)⇒ capability 名不同、判据不重叠;**CC-06 明确引用队列 #6 的 AC-04,不重立第二份**(避免两份判据漂移)。

**⑤ 判据面(6 条,全部配**可执行**反向 fixture)**

| ID | 判据 | 反向 fixture 形态 |
|----|------|-------------------|
| **CC-01** | `CohIfBundle` 只含 5 流 + 无网络层字段 | 静态扫描(加 `flow_id` / 加网络层字段)⇒ 红 —— 参照 `cep-stub-cache-core` spec 的「三流名不出现」先例 |
| **CC-02** | `BUNDLE_FIELD_WIDTH` 锁 + `is_trivially_copyable` | test TU 内 `static_assert` + **`try_compile` 毒化副本**(编译期锁**不得**只以「编译红」充当 ctest 判据) |
| **CC-03** | `ProtocolBridge` `pack`/`unpack` 双向可逆 | 篡改字段映射(如 `perm` → `flit_category`)⇒ 红 |
| **CC-04** | JSON 装配端到端连通 + **N ≥ 2** | 断链 ⇒ 红;**N=1** ⇒ 宏 `static_assert` 编译红(防 `bind_ports` 空 shim **静默不绑定**) |
| **CC-05** | `register_protocol_bridge` **注册与消费成对** + per-bridge 元数据齐备 | 只注册不消费 ⇒ 红(否则是新的死钩子);重复 `bridge_id_` ⇒ 红;`protocol_id_` 与 `family` 不符 ⇒ 红 |
| **CC-06** | 6→3 映射落网络 Bundle 的 3 个 `CATEGORY_*` 内 | **引用**队列 #6 的 AC-04 反向 fixture(第 4 类 ⇒ 红) |

> **⚠️ 6 条判据与 `ADR-CEP-19` 的 CC-01~CC-06 严格一一对应** —— planner **不得**给已签发的 ADR 增删判据(增删 = ADR 修订,须走冻结面两步授权)。本轮初稿曾擅自新增 `CC-04b`,**已删除**并把 per-bridge 元数据降为 **CC-05 的子条件**。

**⑥ B2 贡献 —— 新增 invariant 数 = 0**

`test_cep_canonical_invariants.cc::expected_groups()` 硬编码 15 组 66 条;`ADR-CEP-01` / `ADR-CEP-03` 在 15 组里**无组** ⇒ 新增编号即契约违反。⇒ 本 change 贡献 = **行为化既有条目**(`INV-CEP-05-IC1` / `INV-CEP-12-IC1~IC3`)+ 契约层加固 ⇒ **该测试文件零 diff**。

> ⚠️ **双写规则教训(本轮实测踩到)**:ADR 归类清单在**两处** —— `scripts/test/cep_docs_sync_check.sh` 的 `NO_INV` 与 `test/cep_adr_parser.hh` 的 `no_inv_adr_files()`(后者的注释明写「两处规则重复,**修一处必须记得改另一处**,否则守门出现盲区」)。`ADR-CEP-19` 签发时**只改了脚本侧** ⇒ `cep_canonical_invariant` / `cep_all` **ctest 2 项失败**;已同步登记(per commit `d6673757`)。

**⑦ 转正动作清单(slot 释放后,按序执行)**

| 步 | 动作 | 判据 |
|----|------|------|
| 0 | `openspec list --json` 实测 CEP 内部 active == **0** | 🔴 **硬前置**(`obj §4.7.2` step 0) |
| 1 | `git mv .rddf/drafts/cpptlm-cep-cohif-carrier openspec/changes/cpptlm-cep-cohif-carrier` | 目录就位 |
| 2 | 删各文件顶部的「未建 change 目录」标注段;状态行改 🟡 Proposed | — |
| 3 | `openspec validate cpptlm-cep-cohif-carrier --strict` | rc=0 |
| 4 | 台账追加转正 Entry(KPI 0/3 → 1/3) | `obj §11` |
| 5 | `obj §4.0` 状态列 📋 Planned → 🟡 + `phase-10.md` 同步 + `obj §5.2` KPI 回填 | `obj §4.7.3` ④ |

**⑧ 冻结面**

本轮 **零**冻结面改动 —— 产出全在 `.rddf/improvements/` 与 `.rddf/drafts/`(均不在 M1 冻结集)⇒ **无需两步授权**;冻结集成员数**不变**(40)。`ADR-CEP-19` 的冻结面改动已于 Entry #74 走完两步授权(产物 `90877fd8` → 清单 `19342b79`)。

**⑨ 验收(实测)**

| 项 | 值 |
|---|---|
| `openspec list --json` | **1** 个 active(队列 #6)⇒ KPI **1/3** —— 本轮**未**新建 change 目录 ⇒ 并发**未**变 |
| 守门 7 项 | rc=0 |
| `cep_gate_selftest` | 31 / 31 |
| `ctest --test-dir build -j4` | 106 / 106 |
| `openspec validate --changes --strict` | rc=0(唯一 active 仍是队列 #6) |
| 冻结集 | **40**(零改动)· provenance 自检通过 |
| 4 件套草稿的结构自检 | Requirement 正文 **< 500** 字符(实测 432 / 427)· 每 Scenario 带 `> **锚点**`(3/3、3/3 **一一对应**)· 2 份 delta spec 各带实质 `## Purpose` |
| 判据与 `ADR-CEP-19` 的对应 | **CC-01~CC-06 严格一一对应**(本轮初稿擅自新增的 `CC-04b` 已删,per-bridge 元数据降为 CC-05 子条件) |

> **⚠️ 未能跑 `openspec validate`(change 目录未建)** —— 该命令只对 `openspec/changes/<name>/` 生效 ⇒ 本轮以**结构自检**降级替代(Requirement 长度 / Scenario 数 / 锚点完整性 / Purpose 存在性);**转正后必须补跑** `openspec validate cpptlm-cep-cohif-carrier --strict`。

**⑩ 关联**: Entry #74(排队 13 → 14 + `ADR-CEP-19` 签发授权)· #71 / #72 / #73(队列 #6 的 planner + critic 两轮)· `ADR-CEP-19`(✅ v1.0 已签发)· `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`(v3)

---

### Entry #76 — 2026-10-27 [plan]

**主题**: 队列 #7 `cpptlm-cep-cohif-carrier` planner 产出的 **critic 首轮 REJECT 收口**(1 BLOCKER + 5 SHOULD_FIX + 4 NITS)+ **`obj §4.7.2` step 1「三条出路」裁决补注**(补 Entry #75 的漏登)

**① critic 首轮结论**

`mergeable: **no**` · `scope_exceeded: **false**` · `kpi_disposition_correct: **yes**`

| 三问 | 判定 |
|---|---|
| 违反 KPI 硬前置(不建 change 目录)? | **否** —— 处置正确 |
| 无 `tasks.md` 是否符合分工? | **符合**(tasks 属 builder;队列 #6 实测亦无) |
| planner 给已签发 ADR 增删判据? | **字面上否**(ADR 无 CC 定义),但对 research §6.4 **有 2 处增补**(CC-05 ④ / CC-06 ②)⇒ 与「严格一一对应」声明矛盾 |
| 转正后 `openspec validate` 会红? | **是** —— 唯一 BLOCKER |

**独立性缺口**:本仓单开发者环境,评审由 AI agent 完成 ⇒ **不等于真人外部评审**;按 `obj §9.3` R10 登记为「残余风险 + DRI 书面接受」,**不得**表述为「已获独立评审通过」。

**② BLOCKER 修复(复现 + 修法均实测)**

| 项 | 内容 |
|---|---|
| 现象 | `specs/cep-protocol-bridge-contracts/spec.md` 只有 `## Requirements`,**无** `## ADDED Requirements` |
| 后果 | 转正步 3 `openspec validate <change> --strict` 报 **ERROR** `No delta sections found`(openspec 1.14.3 `validator.js:428-434` 逐 spec 文件判定,兄弟 spec 有 ADDED 不赦免) |
| 复现 | `/tmp/opencode/os-fmt` 单文件 delta spec(只 `## Requirements`)⇒ `✗ [ERROR] cap-c/spec.md: No delta sections found` |
| 修法 | 两份 delta spec **统一**为 `## Purpose` + `## ADDED Requirements`;并删 `cep-cohif-carrier` 那份的**空** `## Requirements` |
| 验证 | 同形态单文件 ⇒ `Change 'tc' is valid`(rc=0);**转正形态整目录演练**见 ③ |
| 附带发现 | 空 `## Requirements` 虽不报错,但校验器 **WARNING** 明示「**不随 archive 并入 main spec**」⇒ 空壳无价值且易误导 ⇒ 一并删除 |

**③ S1 + S2 处置 —— 「编号对齐 ≠ 定义对齐」**

critic 的 `S1`(三选一)+ `S2`(改措辞)本轮**合并处置 = S1 的 option ③**(保留增补 + 诚实标注 + 追认路径)。**完整论证落 `design.md` 的 `D-η`,是全仓唯一真源**;`improvement` / `proposal` 两处只留指针(避免三份漂移)。

| 增补 | 上游锚点 | 分类 |
|---|---|---|
| **CC-05 子条件 ④**(per-bridge 元数据齐备:`protocol_id_` 非零 / `bridge_id_` 唯一 / 与实例声明 `family` 一致) | ✅ `ADR-CEP-19 §3.2`(`:157`「per-bridge 元数据**只做静态**」)+ `docs/architecture/06-inter-module-network.md` §4.5(P1 范围) | **判据化已有代价项**(非新决策) |
| **CC-06 子条件 ②**(映射表 SHALL **非恒等**) | ❌ **无上游锚点** —— 仅「特化字典存在理由」的逻辑推论 | **纯 planner 推论** |

**真源层级缺陷(诚实登记)**:`ADR-CEP-19` 全文对 `CC-0x` **只有 3 处编号引用**(`:167` / `:168` / `:205`),**不含定义**;判据定义真源在 `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md §6.4`(`L369-374`,**附属层**)⇒ **真源 3(ADR)不自足**。

**追认路径(已写成 `design.md` `D-κ` 步 0.5 的转正硬前置)**:① architect 给 `ADR-CEP-19` 追加 `## Status Update` 段承认该 2 处(**不改决策正文** ⇒ ADR 不可变纪律成立);② 撤出这 2 处子条件,回到 research §6.4 字面。**未择一落地前,任何产出不得宣称「判据完全来自已签发 ADR」**。

**属细化、不改语义(critic 判定「不动」)**:CC-01 ③(5 个域外流名**类型不可表达**)· CC-02 ③ + 加虚函数 fixture · CC-06 ③(引用队列 #6 的 AC-04,不重立第二份)。

**④ `obj §4.7.2` step 1 的「三条出路」裁决(补 Entry #75 漏登;Entry #75 本身 append-only 不改)**

| # | 出路 | 裁决 |
|---|---|---|
| **①** | **建 change**(proposal + design + specs + tasks) | ✅ **DRI 已选** —— 用户指令「起 #7 的 planner」即指向建 change;但 `§4.7.2` step 0 的 KPI slot 硬前置未满足 ⇒ **暂以 staged 形态备妥**,slot 释放后按 `design.md` `D-κ` 七步(含步 0.5)转正 |
| **②** | 移出队列 | ❌ 不采 —— 载体债会**复制进 #8**(`proposal.md §Why`:#8 将继承 #6 的无载体装配模式) |
| **③** | 降级为设计草案 | ❌ 不采 —— 6 条判据已**可执行化**(逐条带反向 fixture),降级会丢验收面 |

**⑤ 其余收口**

| 项 | 动作 | 状态 |
|---|---|---|
| `S2` 措辞(4 处) | `improvement` / `proposal` / `design` 三处改写完毕;**`Entry #75 ⑤` 因 append-only 不改**,以本 entry 为准 | ✅ |
| `S3` 三条出路 | 本 entry ④ 补齐 | ✅ |
| `S4` P1 清单回填 | `obj:674`「⬜ 待写」→「✅ 已写」+「`ADR-CEP-19` 草案」→「✅ 已签发」(违 `§4.7.1` 硬规则 1 已消) | ✅ |
| `S5` staged 子目录无机器登记 | **planner 侧收口** = `obj:183` 行尾显式登记 `.rddf/drafts/cpptlm-cep-cohif-carrier/`;**闸扫描面扩展**(`cep_docs_sync_check.sh:392` 的 `.rddf/drafts/*.md` → 递归覆盖子目录)owner = **builder**,转正时做 | 部分(planner 侧已做) |
| NIT `obj:183` 状态列 | 📋 Planned → 📋 Planned(**4 件套草稿已落,待 KPI slot 转正**) | ✅ |
| NIT `obj:248` | 「ADR-CEP-19(待签发…)」→「✅ 已签发 2026-10-27」 | ✅ |
| **闸缺陷(本轮新发现)** | `scripts/test/cep_doc_ref_aliases.tsv:18` 把 **`06 号`** 登记为 `archforge:docs/architecture/06-tensor-core.md`,而 CEP 文档里的 `06 号` 指**本仓** `docs/architecture/06-inter-module-network.md` ⇒ 引用被校验到**异构仓文件**上。HEAD 存量引用(§3.1 / §3.3)**恰好**在对方文件里也存在 ⇒ **假阴性**;本轮 `§4.5` 不存在才撞出。**planner 侧收口** = 新写引用改用全路径(已改);**别名表修订** owner = 架构/闸维护者(**改它会重算全仓存量 `06 号 §N`**,可能一次暴露多处,不属 planner 权限) | 已登记,未修 |
| NIT improvement 无 front-matter | 补 `name` / `priority` / `phase` / `主题` / `依赖` / `状态` / `created`(对齐队列 #6 先例;`obj §4.7.1 C3` 声明其为「机器(①)」落点) | ✅ |

**⑥ 验收(本 entry 落地后实测)**

| 项 | 值 |
|---|---|
| 两份 delta spec 形态 | `## Purpose` + `## ADDED Requirements`;各 3 Scenario / 3 锚点;**无** `## Requirements` 空壳 |
| 转正形态整目录演练 | 复制 4 件套到 `/tmp` 假仓的**同名** change 目录下,跑 `openspec validate --strict` ⇒ **rc=0** |
| 判据 ↔ 声明自洽 | 3 处措辞已改;CC-05 ④ / CC-06 ② 标注「planner 增补 · 待追认」+ 转正硬前置步 0.5 |
| 冻结面 | **零改动**(全部在 `.rddf/**`)⇒ 冻结集仍 **40** |
| KPI | 仍 **1/3**(未建 change 目录 ⇒ 并发未变) |

**⑦ 关联**: Entry #75(planner 产出)· #74(`ADR-CEP-19` 签发 + 排队授权)· critic 首轮 `mergeable: no` · `design.md` `D-η`(判据口径真源)/ `D-κ` 步 0.5(追认硬前置)

---

### Entry #77 — 2026-10-27 [review]

**主题**: 队列 #7 `cpptlm-cep-cohif-carrier` planner 产出 **critic 第 2 轮复审 = APPROVE**(预算用尽:同一 change 已用 2/2 轮)

**① 结论**

| 信号 | 值 |
|---|---|
| `A_BLOCKER_CLOSED` | **yes**(critic 实测:`## ADDED Requirements` 各 1 处 / `## Requirements` 0 命中 / Scenario 与锚点结构未动) |
| `B_NEW_BLOCKERS` | **0** |
| `SCOPE_BREACH` | **no**(新增 diff 全在 `.rddf/**`,未越 planner 职责,未超原 finding 收口面) |
| `mergeable_after_round2` | **yes** |

**② 复审预算(硬规则执行情况)**

| 轮 | 日期 | 结论 | 提交给 critic 的范围 |
|---|---|---|---|
| 1 | 2026-10-27 | **REJECT**(1 BLOCKER + 5 SHOULD_FIX + 4 NITS) | planner 产出全量 |
| 2 | 2026-10-27 | **APPROVE** | 仅「BLOCKER 闭合证据 + 本轮新增 diff」—— 未要求全量重审,未写开放式找漏洞指令 |

⇒ **2/2 轮用尽**。后续若再起 critic 须用户明确批准(per 评审预算规则)。

**③ 独立性缺口(强制标注)**

本仓单开发者环境,本轮复审仍由 AI agent 完成 ⇒ **不等于真人外部评审**;按 `obj §9.3` R10 登记为「残余风险 + DRI 书面接受」,**不得**表述为「已获独立评审通过」。

**④ 未闭合项(非 BLOCKER,转正前必须处理)**

| # | 项 | owner | 触发时点 |
|---|---|---|---|
| 1 | **判据增补追认** —— CC-05 ④(有锚点)/ CC-06 ②(无锚点)二者择一:architect 给 `ADR-CEP-19` 追加 `## Status Update` 承认 / 撤出回到 research §6.4 字面 | **architect** | `design.md` `D-κ` **步 0.5**(转正硬前置) |
| 2 | `scripts/test/cep_doc_ref_aliases.tsv:18` 的 `06 号` 别名指向 ArchForge 异构文件(存量引用假阴性) | **闸维护者** | 任何时候;改它会重算全仓存量 `06 号 §N` |
| 3 | `cep_docs_sync_check.sh:392` 的 `.rddf/drafts/*.md` 扫描面未递归覆盖子目录 | **builder** | 转正时 |

**⑤ 关联**: Entry #76(critic 首轮收口)· #75(planner 产出)· #74(`ADR-CEP-19` 签发)· `design.md` `D-η`(判据口径真源)/ `D-κ` 步 0.5

---

### Entry #78 — 2026-10-27 [adr-amendment]

**主题**: 授权 `ADR-CEP-19` 追加 **Status Update** —— 追认队列 #7 的 **2 处判据子条件**(CC-05 ④ / CC-06 ②)+ 登记**判据真源层级缺陷**(冻结面改动的**授权记录**,两步授权第一环)

**① 触发**

队列 #7 `cpptlm-cep-cohif-carrier` 的 planner 产出经 critic **两轮**对抗式评审(第 1 轮 **REJECT** → 第 2 轮 **APPROVE**,per Entry #76 / #77)。第 1 轮指出一个**架构层**问题,planner 阶段**无法自行闭合**:

> `ADR-CEP-19` 全文对 `CC-0x` **只有 3 处编号引用**(`:167` / `:168` / `:205`),**不含判据定义**;判据定义真源在 `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md §6.4`(**附属层**调研文档)⇒ **真源 3(ADR)不自足**。

而 planner 的 delta spec 相对 research §6.4 字面**多出 2 条子条件** ⇒ 「planner **不得**给已签发 ADR 增删判据」的纪律对这两处不成立 ⇒ 须由 **architect** 追认;而 §2 决策正文**不可变** ⇒ 只能**追加 Status Update**。

**② 追认对象(2 处,逐条带锚点)**

| # | 子条件 | 上游锚点 | 追认依据 | 结论 |
|---|---|---|---|---|
| **①** | **CC-05 ④** —— per-bridge 元数据齐备(`protocol_id_` 非零 / `bridge_id_` 唯一 / 与实例声明 `family` 一致) | ✅ `ADR-CEP-19 §3.2`(`:157`「per-bridge 元数据**只做静态**」)+ `docs/architecture/06-inter-module-network.md` §4.5 | 该元数据**本就在本 ADR 的决策面之内**;④ 只是把**已声明的代价项**判据化(机器可检),**不引入新决策** | **追认成立** |
| **②** | **CC-06 ②** —— 映射表 SHALL **非恒等**(至少一行 ≠ 同名) | ❌ **无上游条款** | 逻辑推论:若 6→3 映射**恒等**,则「CohIf 语义 → 网络 Bundle 链路层类别」无需字典,队列 #6 的映射表机制**失去存在意义**;该子条件是**反向护栏**(防字典退化),**不改变** CC-06 的主判据(产出的 `CATEGORY_*` 必须落在网络 Bundle 的 3 个常量内) | **追认成立**(依据标注为**推论**,非条款) |

**编号集合不变**:追认后判据仍为 **CC-01 ~ CC-06 共 6 条**,与 `ADR-CEP-19 §5` 后续表所述「6 条判据 CC-01~CC-06」**一致** ⇒ **不新增判据编号**,不改 §2 决策 1~9 正文。

**③ 治本路径(真源层级缺陷的收口)**

判据定义置于附属层调研文档、真源 3 只留编号引用 ⇒ ADR **不自足**。治本 = 队列 #7 **转正归档**后 `openspec archive` 把 delta spec 并入 `openspec/specs/` ⇒ 判据进入真源层(自足);本 ADR 的追加段届时退为历史沿革。

**④ 授权范围(冻结面两步授权的**第一环**:授权记录)**

| 项 | 内容 |
|---|---|
| 授权对象 | `docs/adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md` —— **仅追加** `## Status Update 2026-10-27` 段 |
| 允许写 | 追认结论(2 处子条件)+ 判据口径(编号对齐 ≠ 定义对齐)+ 真源层级登记 + 台账指针 + 冻结面指针 |
| **禁止** | 改 §1 ~ §7 **任何正文**;新增决策;改判据**编号集合**;触碰 §2 决策 9(`StubCacheCore` 延期) |
| 冻结面 | 本文件属 M1 冻结面 **40 成员** ⇒ 本次改动**必须**走两步授权 |
| 两步授权序列 | ① commit 产物(ADR 追加段)→ ② `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #78"` → ③ commit 清单(`docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`) |

**⑤ 追认的边界(须写进 ADR 段内)**

若实现阶段证明子条件 ④ / ② **不可实现**,须**新开**一段 Status Update 登记其失效,**不得**回改本次追加的段(per AGENTS.md「ADR 不可变 + Status Update」纪律)。

**⑥ 关联**: Entry #77(critic 第 2 轮 APPROVE;本项为其「未闭合项 1」)· #76(critic 首轮收口)· #75(planner 产出)· `ADR-CEP-19`(✅ v1.0 已签发)· `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md §6.4`(判据定义真源)

---

### Entry #79 — 2026-10-27 [build]

**主题**: 队列 #6 `cpptlm-cep-moesi-gpu-adapter` **实施完成** + **✅ 完成归档授权**(首个 `implemented` adapter 落地)

**① 触发**

用户指令「切 architect 做第一项,然后持续推进」⇒ 第一项(判据增补追认,Entry #78)完成后**持续推进**关键路径:队列 #7 转正需先**释放 KPI slot** ⇒ slot 的唯一占用者 = 队列 #6 ⇒ 本 entry 落队列 #6 的 **builder 阶段产出**。

**② 交付面(逐项对应 delta spec 的 14 条 Scenario)**

| 载体 | 内容 |
|---|---|
| `include/tlm/cep/moesi_gpu_adapter.hh` | 6×6 `state_transition`(逐字抄 ArchForge `09 号` §3.3)· 6→4 `perm` 投影(瞬态 `T` **无**投影)· 6→3 消息类映射 + 恒等登记 · `is_silent_upgrade` / `snoop_response_carries_data`(`ADR-CEP-13` 决策 3 判据 a/b) |
| `include/tlm/cep/adapter_schema_contracts.hh` | **判据实现单一真源**:AC-01 ~ AC-05 + 三态(`SKIP`/`PASS`/`FAIL` 可区分)+ 逐条可 grep 的 verdict |
| `include/tlm/cep/adapter_contracts_loader.hh` | 装载层(判据**不**依赖 JSON;装载**只有一份**)⇒ test 与 CLI 共用 |
| `include/tlm/cep/perm_upgrades.json` + `perm_upgrades.schema.json` | 静默升级独立声明源 + 配套 schema(**跨 schema `$ref`** 绑定 `perm` 4 值域 ⇒ 机制级不可能表达第 5 值) |
| `examples/cep_adapter_contracts.cc` | 离线 CLI(与测试**共用**同一份判据实现;测试实跑 CLI 逐行比对) |
| `test/test_cep_moesi_gpu_adapter.cc` + `test/test_cep_adapter_schema_contracts.cc` | 13 cases / 170 assertions(14 条 Scenario 全部落地 + 14 条反向 fixture) |
| `openspec/changes/archive/2026-10-27-cpptlm-cep-moesi-gpu-adapter/tasks.md` | **builder 交付物**(本 change 此前**实测无** `tasks.md`)—— 8 Phase 逐任务 + 归档前复核 8 项 |

**③ `status` 翻转 + ⑥ 镜像闸收窄(per design `D-ζ`)**

| 项 | 内容 |
|---|---|
| 翻转 | `moesi_gpu.status`:`planned` → `implemented`(**其余 12 键逐字零 diff**) |
| ⑥ 收窄 | 逐字段比对**跳过** `status`(本仓权威字段「本仓是否已实现」,非 ArchForge 镜像面) |
| 收窄的**反向 fixture** | `cep_decl_reverse_verify.sh` 破坏 **14**:12 个非 `status` 键**穷举**(逐个改坏 ⇒ 每次必红)+ 破坏 **15**:`status` 改坏 ⇒ **正控绿**(收窄不得退化为「整条不比对」)⇒ **29/29** |
| 归因 | 只有 ⑥ 读本仓副本 ⇒ 副本被改 ⇒ 闸红 ⇒ 归因于 ⑥(变异**保结构** ⇒ 报「漂移」而非崩溃) |

**④ 闸改造:断言 ⑧ 从「届满即永久红」→「届满 ⇒ 必须已重评登记」**

**问题**:队列 #6 落地后 `perm_upgrades.schema.json` 的 `perm_upgrade` 键**永久**满足旧届满条件 ⇒ 旧实现下 #6 **永远无法归档** ⇒ 队列停滞。
**处置**:届满 ⇒ 要求**重评已登记**(新有效期锚出现在契约层);新锚的届满条件**机器可检测**(① Stub 请求侧 `perm_upgrade` 扩展 ② `perm` 枚举 ≠ 4 值)⇒ 新有效期**不**是纯纸面。闸自检 +4 条 fixture ⇒ **34/34**。

**⑤ S1 有效期重评(design `D-θ` 的**归档前强制任务**)**

| 项 | 内容 |
|---|---|
| 触发 | 队列 #6 的 `perm` 形态演进落地 ⇒ `S1_VALIDITY_UNTIL=queue6-perm-schema-evolution` **届满** |
| 重评结论 | **S1 仍为 FAIL**(结论**未翻转** —— R-2 发生点不变:写路径 `perm` 仍在**请求侧**携带 `UNIQUE`,期望投影是 `WRITEBACK`) |
| 可发现性改善 | 「哪个实例有静默升级能力」现有**机器可读**声明源(`perm_upgrades.json` + 跨 schema `$ref`)⇒ 消费端可据此修正请求侧 `perm`;**修正机制本身不在本 change 交付面**(Stub↔PE 腿归 `cpptlm-cep-stub-cache-chstream-surface`) |
| 新有效期 | `S1_VALIDITY_UNTIL=stub-request-side-perm-extension` + 机器可检测的届满条件 |
| 落点 | **契约层**(本 change 的 delta spec ⇒ 归档后并入 `openspec/specs/cep-adapter-schema-contracts/spec.md`)⇒ **零冻结面改动**(冻结的 `cep-poc-spike-verification` main spec **逐字仍成立**:结论仍 FAIL / 机器登记仍 `S1_VERDICT=FAIL` + `S1_MISMATCH_STEPS=1` / spike 仍打印原锚) |

**⑥ 冻结面**

本 change **零**冻结面改动 —— 新增文件均不在 M1 冻结集(`include/tlm/cep/moesi_gpu_adapter.hh` / `include/tlm/cep/adapter_schema_contracts.hh` / `include/tlm/cep/adapter_contracts_loader.hh` / `include/tlm/cep/perm_upgrades.json` / `include/tlm/cep/perm_upgrades.schema.json` / `examples/cep_adapter_contracts.cc` / `test/test_cep_moesi_gpu_adapter.cc` / `test/test_cep_adapter_schema_contracts.cc` 均**非**冻结成员;`scripts/test/` 的两个 checker **按设计不冻结**)⇒ 冻结集仍 **40**,hash 全一致 ⇒ **无需两步授权**。

> ⚠️ 但 `protocol_instances.json` 的 `moesi_gpu.status` 翻转**触及** ⑥ 镜像闸 ⇒ 已按 design `D-ζ` 收窄该闸并配**穷举反向 fixture**(否则「收窄」本身无牙)。

**⑦ 验收(实测)**

| 项 | 值 |
|---|---|
| `openspec validate cpptlm-cep-moesi-gpu-adapter --strict` | rc=0(valid) |
| 新增测试 | **13 cases / 170 assertions** 全绿 |
| `ctest --test-dir build -j4` | **107 / 107** 全绿(实施前 106 ⇒ +1 个新测试文件组) |
| 守门 7 项 | 全 rc=0 |
| `cep_gate_selftest` | **34 / 34** |
| `cep_decl_reverse_verify.sh` | **29 / 29**(串行手动跑,未注册进 ctest) |
| 跨 schema `$ref` 机制级 | 注入 `ATOMIC` / `SHARED` ⇒ **必被挡住**(实测) |
| 冻结集 | **40**(零改动,hash 全一致) |
| KPI | 归档前 **1/3** ⇒ 归档后 **0/3** |

**⑧ 归档授权(✅ 完成归档语义)**

**判据**:tasks **全勾** + 实现已落地 ⇒ **✅ 已完成归档**(`openspec archive cpptlm-cep-moesi-gpu-adapter`,**合并** delta spec 进 `openspec/specs/`)—— **不是** ⏸ 暂存归档(`--skip-specs`)⇒ 判据已进入系统契约。
**归档后**:KPI slot 释放 ⇒ 队列 #7 `cpptlm-cep-cohif-carrier` 可按 `obj §4.7.2` step 0~6 转正(其 4 件套草稿已 staged 于 `.rddf/drafts/`)。

**⑨ 关联**: Entry #78(`ADR-CEP-19` 判据子条件追认)· #77(critic 第 2 轮 APPROVE)· #76(critic 首轮收口)· #75(队列 #7 planner 产出)· #71(队列 #6 建 change 目录 + 冻结面授权)

---

### Entry #80 — 2026-10-27 [archive]

**主题**: 队列 #6 `cpptlm-cep-moesi-gpu-adapter` **✅ 完成归档**(KPI **1/3 → 0/3**)+ 归档后同步 + 授权 `ADR-CEP-00` 追加 Status Update(冻结面)

**① 归档动作(✅ 完成归档语义)**

| 项 | 内容 |
|---|---|
| 命令 | `openspec archive cpptlm-cep-moesi-gpu-adapter --yes` |
| 归档名 | `2026-10-27-cpptlm-cep-moesi-gpu-adapter`(目录 `openspec/changes/archive/2026-10-27-cpptlm-cep-moesi-gpu-adapter/`) |
| delta 并入 | **2 份新 main spec**:`openspec/specs/cep-moesi-gpu-adapter/spec.md`(**7** Scenario)+ `openspec/specs/cep-adapter-schema-contracts/spec.md`(**7** Scenario)⇒ 判据**进入系统契约** |
| 语义判定 | **✅ 已完成归档**(**不是** ⏸ 暂存归档)—— tasks 全勾 + 实现已落地 ⇒ **不**用 `--skip-specs` |
| KPI | **1/3 → 0/3**(`openspec list --json` 实测 **0** 个 active)⇒ slot 释放 ⇒ 队列 #7 可转正 |

> **为何不是暂存归档**:AGENTS.md「两种归档语义必须区分」—— ⏸ 暂存归档用于「实现未启动 / 0%,设计仍有价值」;本 change 实现**已落地**(14 Scenario / 13 cases / 170 assertions / 6 标签回归)⇒ 断言「该能力已在契约内」成立 ⇒ 走**完成归档**。

**② 归档后同步清单(逐处带「命中 1 次」断言)**

| 落点 | 改什么 |
|---|---|
| `obj §4.0` #6 行 | 状态列 🟡 Proposed → ✅ **archived**;**孤儿前置标记撤回**(3 项均已收口,详见 ③) |
| `obj §4.2` #6 行 | 同上(两处须一致,per 断言 ⑦ ①②) |
| `obj §5.2` | 当前 Proposed 并发 **1/3 → 0/3**;下一个可启动 = 队列 #7 |
| `phase-10.md` / `.rddf/roadmap.md` / `.rddf/improvements/cpptlm-cep-umbrella.md` / `AGENTS.md` | KPI baseline **1/3 → 0/3**(+ 现行基线的实测日期与 active 数) |
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | **冻结面** ⇒ 追加 `## Status Update`(队列 #6 已完成归档 + KPI 1/3 → 0/3)⇒ **两步授权**(本 entry = 授权记录) |
| 2 份新 main spec | `## Purpose` 补**实质**文本(归档器写的是 `TBD - created by archiving change …` 占位符 ⇒ `--specs --strict` 会判红) |

**③ 孤儿前置标记撤回(断言 ⑦ ③b 的要求)**

#6 行带 `⏭(孤儿)` 标记(3 项孤儿前置,登记于 2026-10-25 Entry #57);#6 归档后该标记成为**无主债**(闸 ③b 报红:「孤儿前置行的 change 已归档 → 应重指向活队列行」)。
**逐项核对 —— 3 项均已收口,无须转投**:

| # | 孤儿前置项 | 现状 |
|---|---|---|
| ① | M-4 行为侧(VC_DAT 语义正名 / 冲突输入规则 / 陈旧注释防护) | ✅ 已由队列 #3 的 `cep-vc-mapping-contract` 落成 |
| ② | verdict 逐 `(network, vc)` 标注义务 | ✅ 已由队列 #4 的 `cep-channel-dependency-gate` 落成 |
| ③ | `moesi_gpu` / `mesif` 未排期 | ✅ 已于 Entry #71 消解(本行 = `moesi_gpu`;`mesif` → #9 行);`moesi_gpu` 现已 implemented |

⇒ **标记撤回**(改写为「原孤儿前置 3 项已全部收口」+ 逐项出处)⇒ 无主债消失,且**历史可追**(不删内容,只改标记形态)。

**④ S1 有效期重评的落点已随归档迁移**

重评登记原写在 #6 的 delta spec ⇒ 归档后并入 `openspec/specs/cep-adapter-schema-contracts/spec.md`(契约层,长期可查)⇒ 断言 ⑧ 的「重评已登记」检查在新位置仍命中(实测绿)⇒ **S1 有效期不再是纯纸面**。

**⑤ 冻结面授权(本 entry = 两步授权的第一环)**

| 项 | 内容 |
|---|---|
| 授权对象 | `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` —— **仅追加** `## Status Update 2026-10-27` 段 |
| 允许写 | 队列 #6 已完成归档(完成归档语义 + archive 名 + 2 份 main spec 的 Scenario 数)+ KPI **1/3 → 0/3** + 下一个可启动 = 队列 #7 |
| **禁止** | 改 §1 ~ §8 任何正文;改排队算术(仍为 14 change);新增决策 |
| 两步授权序列 | ① commit 产物(ADR 追加段)→ ② `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #80"` → ③ commit 清单 |

**⑥ 验收(实测)**

| 项 | 值 |
|---|---|
| `openspec list --json` | **0** 个 active ⇒ KPI **0/3** |
| 归档 | 2 份 main spec 已创建(`cep-moesi-gpu-adapter` 7 / `cep-adapter-schema-contracts` 7 Scenario) |
| 守门 7 项 + `cep_gate_selftest` + `ctest` | 见本 entry 落地后的提交信息 |

**⑦ 关联**: Entry #79(队列 #6 实施完成 + 归档授权)· #78(`ADR-CEP-19` 判据子条件追认)· #77(critic 第 2 轮 APPROVE)· #76(critic 首轮收口)· #75(队列 #7 planner 产出)· #71(队列 #6 建 change 目录 + 冻结面授权)

---

### Entry #81 — 2026-10-27 [stage-gate]

**主题**: 队列 #7 `cpptlm-cep-cohif-carrier` **转正**(⏸ 队列位 → 🟡 Proposed,KPI **0/3 → 1/3**)+ 授权 `ADR-CEP-00` 追加 Status Update(冻结面两步授权第一环)

**① 触发**

用户指令「继续」推进关键路径。前置硬条件(`obj §4.7.2` step 0「🔴 **先确认 KPI slot 可用**」)已满足:队列 #6 `cpptlm-cep-moesi-gpu-adapter` **✅ 完成归档**(Entry #80)⇒ `openspec list --json` 实测 **0** 个 active ⇒ CEP 内部 Proposed 并发 < 1 ⇒ 允许启动 1 个。

**② 转正动作(`design.md` `D-κ` step 0 ~ 6 全部执行)**

| 步 | 动作 | 结果 |
|---|---|---|
| 0 | `openspec list --json` 实测 active == 0 | ✅ 0 |
| 0.5 | 判据增补追认(CC-05 ④ / CC-06 ②) | ✅ 已落地(出路 ①:`ADR-CEP-19` 追加 `## Status Update 2026-10-27`,正文零改动;Entry #78);`design.md` `D-η` / `D-κ 0.5` 已回填为「已落地」 |
| 1 | `git mv .rddf/drafts/cpptlm-cep-cohif-carrier openspec/changes/cpptlm-cep-cohif-carrier` | ✅ 目录就位(4 个文件:proposal / design / 2 份 delta spec) |
| 2 | 删「未建 change 目录」标注段;状态行改 🟡 Proposed | ✅ proposal / design 顶部已改;`proposal.md` 的「转正命令」段改写为**执行记录**;风险表「KPI slot 未释放」行改「已解除」 |
| 3 | `openspec validate cpptlm-cep-cohif-carrier --strict` | ✅ rc=0 |
| 4 | 台账追加 Entry(本 entry) | ✅ |
| 5 | `obj §4.0` 状态列 📋 Planned → 🟡 | ✅(`§4.2` 同义复述同步) |
| 6 | `phase-10.md` 子任务行 + `obj §5.2` KPI 回填 | ✅ KPI baseline 行同步 |

**③ 新增 `tasks.md`(builder 交付物)**

转正前 4 件套缺 `tasks.md`(planner 阶段不产出;per `obj §4.7.2` 的 🟡 判据「`tasks.md` 有未勾任务」)。本次补齐:**8 Phase / 全部未勾**,逐条对齐 delta spec 的 6 条 Scenario(CC-01 / CC-02 / CC-04 ∈ `cep-cohif-carrier`;CC-03 / CC-05 / CC-06 ∈ `cep-protocol-bridge-contracts`)+ 归档前复核 6 项 + 已知边界 3 条。

**④ 归档后路径修正(随转正的连带)**

| 位置 | 修正 |
|---|---|
| `design.md` `D-η` 的 CC-06 引用 | 队列 #6 的 `design.md:178`(「目标写成第 4 类 ⇒ 红」)→ 改指**归档后**位置 `openspec/changes/archive/2026-10-27-cpptlm-cep-moesi-gpu-adapter/design.md:178`(队列 #6 已归档,原路径已不存在) |
| `proposal.md` 变更件路径 | `.rddf/drafts/cpptlm-cep-cohif-carrier/` → `openspec/changes/cpptlm-cep-cohif-carrier/`(含 `tasks.md`) |

**⑤ KPI 回填(0/3 → 1/3)**

| 文件 | 改什么 |
|---|---|
| `.rddf/roadmap/objectives/obj-cep-platform.md` | §5.2 当前并发 **0/3 → 1/3**;§4.0 / §4.2 的 #7 行状态列 📋 → 🟡 |
| `.rddf/roadmap/phases/phase-10.md` | KPI baseline 行 0/3 → 1/3 |
| `.rddf/roadmap.md` | 顶部 KPI baseline 行 0/3 → 1/3 |
| `.rddf/improvements/cpptlm-cep-umbrella.md` | KPI baseline 行 0/3 → 1/3 |
| `AGENTS.md` | 「现行基线」行 0/3 → 1/3 |
| `.rddf/improvements/cpptlm-cep-cohif-carrier.md` | 状态行(front-matter)+ 转正段 → 已转正 |

**⑥ 冻结面授权(本 entry = 两步授权的第一环)**

| 项 | 内容 |
|---|---|
| 授权对象 | `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` —— **仅追加** `## Status Update 2026-10-27` 段(队列 #7 已启动 + KPI 0/3 → 1/3) |
| 允许写 | 队列 #7 已转正(change 目录已建 + `openspec validate --strict` rc=0)+ KPI **0/3 → 1/3** + 下一个可启动 change(待 #7 归档后) |
| **禁止** | 改 §1 ~ §8 任何正文;改排队算术(仍 14 change / Stage 1 = 10);新增决策 |
| 两步授权序列 | ① commit 产物(ADR 追加段)→ ② `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #81"` → ③ commit 清单 |

**⑦ 验收(实测)**

| 项 | 值 |
|---|---|
| `openspec validate cpptlm-cep-cohif-carrier --strict` | rc=0 |
| `openspec list --json` | **1** 个 active ⇒ KPI **1/3** |
| 守门 7 项 + `cep_gate_selftest` + `ctest` | 见本 entry 落地后的提交信息 |
| 冻结集成员数 | **40**(不变) |

**⑧ 关联**: Entry #80(队列 #6 归档释放 slot)· #79(队列 #6 实施)· #78(判据增补追认)· #77/#76(队列 #7 planner 产出 critic 两轮)· #75(planner 产出)· #74(队列 #7 立项 + `ADR-CEP-19` 签发)

### Entry #82 — 2026-10-27 [fix]

**主题**: 架构文档**漂移回填**批(P0 路线图载体 + P1 真源 1)—— 用户 2026-10-27 直接指令授权;**不改排期、不改 ADR 正文、不新增决策**

**① 触发**

用户指令「请审查 CEP 代码情况,并检查是否和路线图/架构文档对齐」→ 审查结论:CEP 代码侧健康(`[cep]` 161 case / 2600 assertion 全绿;5 个守门脚本全 rc=0),但**文档侧落后代码 2~3 代**。用户随后指令「动手做 P0」→ P0 已落地(commit `d52fff56`),再指令「继续 P1」⇒ 本 entry 记录 P1 的授权与逐项处置。

**② P0(已落地,commit `d52fff56`)**

| 项 | 内容 |
|---|---|
| `phase-10.md` 依赖图 | 队列 #6 补 `✅ archived(2026-10-27)` 块;队列 #7 补 `🟡 Proposed` 块 + 「ADR-CEP-19 待签发」→「已签发 2026-10-27」 |
| `phase-10.md` 测试标签 | `[cep]` 76/1284 → **161/2600**;全仓 baseline 追加 **1943/873046**(增量主因 `[sdma]` 69 case / 803473 assertion,非 CEP) |
| `phase-10.md` / `roadmap.md` 版本行 | v0.2 → v0.3;2026-10-23 → 2026-10-27(v1.5,引用 Entry #80/#81) |
| `.rddf/state/roadmap-state.json` | **删除**(rdd-workflow 遗留;实测全仓零消费者;内容与 `roadmap.md` Phase Skeleton 直接矛盾 ⇒ 第二份阶段状态真源,违 §4.7.1);`roadmap.md` 顶部增「阶段状态真源」说明行 + `docs_sync_check.sh` `VIRTUAL_PATHS` 登记 |

**③ P1 逐项处置(本 entry 授权范围)**

**A. M1 冻结面成员(`07-coherence-exploration-platform.md`)—— 2 处活口径表机械同步 + 1 则 Status Update 追加**

| 项 | 处置 |
|---|---|
| §11「Gap → 实现状态映射」 | 原写「📐 契约已定义,🛠️ 未实现」11 行 —— 落后两代(队列 #3~#6 全部已归档)。**按实测**(代码载体 + 行为化测试 + 归档件三者一致)逐行重写为 🟡/✅/⛔ 三态:✅ 已完全落地 4 行 · 🟡 部分落地 5 行 · **失指引用修正**(`cpptlm-cep-sharer-tracker-base` 作为 change 目录不存在 → 改指队列 #9~#11) |
| §5.1.1 工业协议族表状态列 | `moesi_gpu` / `mesif` 两行「未排期」→ 「✅ 已落地(队列 #6,2026-10-27 归档)」/「📋 已排期(队列 #9,2028-Q4;⚠ R-1 阻塞)」;**族计数(1 族 / 2 实例)与 Entry #12/#13 历史补记不动**;状态值以 §4.0 排队总表为唯一权威 |
| §5.4 Bundle 约束 | **正文不改** —— 沿用本文件 2026-10-27 第一则附录 ③ 的既有处置(待队列 #7 落地 `CohIfBundle` 后再同步第三类);仅在 §11 对应行注明「第三类已由 `06 §3.3` 增补,唯一实例待落地」 |
| 追加 §附录 Status Update 第二则 | 登记授权 + 上述 2 处同步 + 1 处失指修正 + 4 项**未改动**的已知缺口登记(`NullProtocolEngine` 无载体且无 owner / `06 §15.1`+`§15.4` 未加转隶注记 / `06 §9.1` 行号漂移) |
| 冻结清单 | `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #82"`(冻结集成员数 **40**,不变) |

**B. 非冻结面真源 1(`06-inter-module-network.md` / `01-hybrid-architecture-v2.1.md`)—— 漂移回填**

| 位置 | 处置 |
|---|---|
| `06 §0.1` / `§1.3-3` / `§3.2` 正文 / L5 图注 | Bundle **.hh 文件数**口径统一为 **17**(实测 `ls include/bundles/*.hh`);「18 个」(§0.1)/「16 个」(§1.3-3 / §3.2 正文 / 图注 `per §1.3.1` → `per §1.3`)逐处订正 |
| `06 §3.2` 归类表 | **补登漏登记的 `bundle_serialization.hh` 行**(原表体 16 行 vs 标题 17 个 .hh)—— 归类为「(基础设施)」,不归业务/网络 |
| `06 §3.3` 第三类 | 「唯一实例」→「**规划实例(当前零实例)**」+ 标注 `include/bundles/cep_coherence_bundles_tlm.hh` **尚不存在**(交付队列 = #7,2028-Q2);同时在「写作纪律」段追加 Entry #82 例外说明(§3.2 归类表确实被动过 —— 补登行 + 计数同步,§3.1 判定规则零改动) |
| `06 §2.3` / `§2.4` | 标题层号互换订正(L5 StreamAdapter → **L4**;L4 ChStream → **L3**);§2.3 内 `module_factory.cc:707-709` → **731-733** |
| `06 §9.1` | Step 1-9 行号整体刷新(漂移 ≈25 行,成因 = CEP 闸门 `0.25` 段插入)→ 改用**步骤起始注释行**作锚点;**新增登记「预置 0.25 CEP 装配期判据」**(`module_factory.cc:167-186`),并注明它**非 Step 7c**(per §9.2) |
| `06 §0.3` / `§10.1` | 关联文档表 / supersede 矩阵**各增 07 号一行**(双向 cross-ref) |
| `06 §8.1` | `coherence_domains` 行:「§15.1 演进, 不在 P1」→ 改为「**保留**(4 config 依赖)+ **解析已落地**(`module_factory.cc:195-196`)+ **消费延期到 Stage 2**」 |
| `06 §13.2` | `[coherence]` 标签行 → 改标 `[cep]` 为 CEP 实际标签(19 个 `test_cep_*.cc` / 161 case);`[coherence]` 标注为**已被 legacy `test_coherence_domain.cc` 占用** |
| `06 §15.1` / `§15.4` | 补「**时间线转隶 CEP**」注记(样板 = 同文件 §11.2 的 Entry #74 修订段)—— **P 编号(P2.0~P5,无 P6)与条目本身零改动**,只登记「实施分期作废 + 归属转 CEP」+ P→队列逐项对应 |
| `06 §2.1` 尾注 | 「HOST 进程 + 23 ABI 是 **L7** 边界(per 01 §1.1)」伪引用订正 —— 01 §1.1 只有 L1-L6,**无 L7**;改为「属 01 §1.1 范围外」 |
| `01 §1.2` | Bundle 计数口径未定义 + 数值过期(「18 个 Bundle」/「18 中其余 17」)→ 明确为「**17 个 .hh 文件 / 26 个 Bundle 类型**(25 个 `bundle_base` 派生 + `PcieTlpWireBundle`)」并附可复现命令 |
| `01 §8.5.2` | 补 6×6 cross-ref:「未来(Phase 7.C) 引入 6×6 表」→ **已由 CEP 承接并落地**(`moesi_gpu_adapter.hh` `kStateTransition[6][6]`,队列 #6 已归档);CoherentXBarTLM 职责边界不变(per `ADR-CEP-07A` 决策 1 第 2 项 ③) |

**④ 明确不做(范围边界)**

| 不做 | 理由 |
|---|---|
| 改 §4.0 排期 / 季度 / change 数 | 本 entry 是**文档漂移回填**,不涉排期;排期真源 = §4.0,未改一字 |
| 改任何 ADR 正文 | ADR 不可变 —— P2 批(§11 守门覆盖不足 / `IC2` 时效限定 / `NullProtocolEngine` 无载体)只**登记**,不动 ADR |
| 07 号 §5.4 / §2~§10 架构正文 | 沿用本文件既有两则 Status Update 的处置口径(只改活口径表 + 追加 Status Update) |
| 新建台账 kind | 复用既有 kind(`[fix]`,per Entry #72/#73 同款);`cep_freeze_check.sh` 只校验 **entry id 精确命中**,不解析 kind |
| 实施 `NullProtocolEngine` / `ContractVerdict` 同名冲突 / Stub 源码 grep 守门补强 | 属**代码级**变更,归队列 #7 或新 change,本 entry 只登记 |

**⑤ 验收(实测)**

| 项 | 值 |
|---|---|
| `cep_freeze_check`(含 `--update --authorized-by "Entry #82"`) | rc=0,冻结集成员数 **40**(不变) |
| `cep_docs_sync_check` / `cep_doc_ref_check` / `cep_decl_alignment_check` / `cep_archforge_alignment_check` | 4/4 rc=0 |
| `cep_gate_selftest` | rc=0 |
| `scripts/test/docs_sync_check.sh --strict` | rc=0(416 路径 / 0 缺失) |
| `cpptlm_tests "[cep]"` | 161 case / 2600 assertion 全绿 |

**⑥ 关联**: P0 commit `d52fff56` · Entry #81(队列 #7 转正)· #80(队列 #6 归档)· #79(队列 #6 实施)· #74(`ADR-CEP-19` 签发 + 07 号第一则 Status Update)

### Entry #83 — 2026-10-27 [fix]

**主题**: P2 收口 —— 潜伏编译缺陷消解(`tlm::cep::ContractVerdict` 同名冲突)+ C9 协议标识符机器断言补齐 + `NullProtocolEngine` owner 指派

**① 触发**

P1 收尾时列出的 5 项遗留中,**3 项属代码级 / ADR 文字级**(不涉新决策),用户指令「请继续」授权本批次处置。**边界**:另 2 项(`ADR-CEP-07A` `IC2` 的时效限定 = 收紧 invariant;`ADR-CEP-14` C9 第 1 条的「反射 / 类型 trait」形态)属**决策变更**,本 entry **只登记不处置**。

**② 潜伏编译缺陷:`tlm::cep::ContractVerdict` 同名冲突(commit `7f91abd9`)**

| 项 | 内容 |
|---|---|
| 事实 | `include/tlm/cep/adapter_schema_contracts.hh`(队列 #6)与 `include/tlm/cep/collective_plane_contract.hh`(队列 #5)在**同一命名空间 `tlm::cep`** 下各自定义 `ContractVerdict` / `contract_verdict_name` / `ContractVerdictLine` / `ContractReport`,且**底层值序相反**(SKIP=0,PASS=1,FAIL=2 vs PASS=0,FAIL=1,SKIP=2) |
| 后果 | 任一同时 include 两头的 TU **编译失败**(`g++ -fsyntax-only` 实测 rc=2 `multiple definition`);值序相反使「将来合并」存在**静默语义反转**风险 |
| 处置 | 改名**消费面更小**的 adapter 侧(3 个 TU vs collective 侧 4 个):`AdapterContractVerdict` / `adapter_contract_verdict_name` / `AdapterContractVerdictLine` / `AdapterContractReport` |
| 不变性 | ① 枚举**仅按名访问**(无 `static_cast` / 下标依赖值序,grep 实证)② 符号名**从未进入** `openspec/specs/` 或 ADR 契约面(grep 零命中)⇒ **无 delta spec 变更** ③ CLI 渲染输出逐字不变(2 个 example 实测) |
| 未走 OpenSpec 提案的理由 | AGENTS.md「架构性变更未走 OpenSpec」的门槛为「影响 ≥2 模块 / 公共 API / 配置文件 schema」。本次虽触及公共 API **标识符**,但 **零契约语义变化 + 零 spec 变更 + 零行为变化**,且**新 change 会突破「任一时刻 Proposed 并发 ≤ 1」**(队列 #7 已占 1 slot)⇒ 归**缺陷修复**类(per Entry #72 / #73 同款先例:队列 #6 critic 评审轮的 BLOCKER 修复亦未另起 change) |
| 队列 #7 侧登记 | `openspec/changes/cpptlm-cep-cohif-carrier/tasks.md` 新增 **Phase 0 前置**(task 0.1,已勾)—— 理由:本 change task 6.3 要求引用 `adapter_schema_contracts.hh`,而 canonical 10 流的校验面在 `collective_plane_contract.hh` ⇒ **两契约头必须同 TU 共存**,该冲突是其**启动前置** |

**③ C9 协议标识符机器断言补齐(commit `7f91abd9`)**

| 项 | 内容 |
|---|---|
| 事实 | `ADR-CEP-07A §4 验证口径`第 2 项声称「Stage 0 由 `test_cep_canonical_invariants.cc` 的 C9 负向断言覆盖」—— **实测不成立**(C9 零命中于该文件;实际在 `test_cep_schema.cc` 且**只扫 schema JSON 一个文件`)⇒ C++ 侧(Stub / Directory & Ordering Plane)**零机器断言** |
| 补齐 | `test/cep_adr_parser.hh` 上提共享 helper(`strip_comments` / `identifier_words` / `protocol_identifier_hits` / `normalize_identifier` / `contains_identifier`);`test_cep_stub_cache_core.cc` 去重改用共享实现;`test_cep_contracts_crosscheck.cc` 的 IC2 断言扩展为:C++ 侧 5 个「与协议无关的层」去注释后扫协议标识符 + `ADR-CEP-14` C9 第 4 条(Stub 不感知 4 条协议侧声明字段)+ **正控/反控** + 拓扑 Python 层**显式豁免登记**(per `ADR-CEP-07A` 决策 1 第 7 项 ③「Stage 0~1 不改」) |
| 扫描口径 | 去注释后按**标识符词**判定(`chi_rnf` / `ChiRnfAdapter` 命中;`chip_id`「芯片」≠「CHI」协议 / `hierarchical` 不命中);词集 {MESI, MOESI, CHI, CXL, TILELINK, NVLINK, UALINK, MESIF},**不含 TOKEN**(MOESI_CMP 的协议私有状态概念,非协议名) |
| 变异测试反证 | ① `sharer_tracker.hh` 注入 `ChiRnfAdapter` ⇒ 变红(报出文件 + CHI)② `stub_cache_core.hh` 注入 `int vc_binding` ⇒ **首版未变红** ⇒ 暴露首版断言**恒真空**(误用「大写 + 拆词」词集与小写原名比对)⇒ 修正比较口径 + 补正控 ⇒ 重测正确变红 |
| 登记 | 该「负向断言恒真空」是一次**真实缺陷**(纯负向断言若无正控 + 变异反证,**绿不等于有效**)—— 已记入两份 ADR 的 Status Update |

**④ `NullProtocolEngine` owner 指派(无主债消解)**

| 项 | 内容 |
|---|---|
| 事实 | `ADR-CEP-07 §3` `INV-CEP-07-IC2` 规定 `NullProtocolEngine` 是 ProtocolEngine **默认实现**且须有 **10 流覆盖矩阵**(逐流行为 per `07 号 §6.4`:流 1/3/4/5/6/7/10 no-op · 流 2 immediate-complete · 流 8 immediate-resolve · 流 9 immediate-complete)。2026-10-27 实测:**全仓零载体**,且**无队列 owner** ⇒ 重演「无主债」 |
| 指派 | owner = **`cpptlm-cep-cohif-carrier`(队列 #7)task 2.5 / 2.6 / 2.7** —— 理由:队列 #7 Phase 2 交付 `ProtocolEngineBase` 派生 `ChStreamModuleBase`,**建基类者同时交付其 ADR 强制的默认实现**属该 change 的契约面自洽,非范围蔓延;task 2.6 / 2.7 附反向 fixture(覆盖矩阵缺 1 流 / 某流语义退化 ⇒ 必红) |
| 同步 | `07 号 §11`「ProtocolEngine adapter 实例」行补登 owner(活口径)+ `07 号` 追加 Status Update 第三则 + `phase-10.md` 依赖图队列 #7 块同步 task 数 |

**⑤ 本 entry 明确不做(边界)**

| 不做 | 理由 |
|---|---|
| `INV-CEP-07A-IC2` 加时效限定(拓扑 Python 层自 Stage 2 起适用) | **收紧 invariant = 决策变更** ⇒ 需 architect 签发;本 entry 只在 `ADR-CEP-07A` Status Update 登记张力 + 在测试里做**显式豁免登记**(豁免不是静默漏检) |
| `ADR-CEP-14` C9 第 1 条的「反射 / 类型 trait」形态 | 同上(决策形态变更);当前以标识符词扫描近似覆盖,已在 ADR Status Update ④ 登记未覆盖 |
| 改 `cpptlm/topo/layer.py` 的 `protocol=` 参数 | per `ADR-CEP-07A` 决策 1 第 7 项 ③「Stage 0~1 不改」;Stage 2 + `ADR-CEP-13` 落地时改,并把豁免改为纳入扫描 |
| 改 `06 号` / `01 号` 正文 | 本 entry 只动 `07 号` + 2 份 ADR + 队列 #7 tasks + 测试;06/01 号已在 Entry #82 处置完毕 |

**⑥ 验收(实测)**

| 项 | 值 |
|---|---|
| `g++ -fsyntax-only`(同时 include 两契约头) | **rc=0**(冲突消解前 rc=2) |
| 变异测试 | 2 类变异**均变红**(且第 ② 类在修正前**假绿** → 已修) |
| `cpptlm_tests` 全仓 | **1943 case / 873073 assertion** 全绿 |
| `cpptlm_tests "[cep]"` | **161 case / 2627 assertion** 全绿(较改动前 +27) |
| `cep_docs_sync_check` / `cep_doc_ref_check` / `cep_decl_alignment_check` / `cep_archforge_alignment_check` / `cep_gate_selftest` | 5/5 rc=0 |
| 冻结集成员数 | **40**(不变);`cep_freeze_check --update --authorized-by "Entry #83"` 后 provenance 成立 |

**⑦ 关联**: Entry #82(P1 架构文档漂移回填)· #81(队列 #7 转正)· #80(队列 #6 归档)· #79(队列 #6 实施)· #72/#73(`[fix]` 类先例:critic 评审轮修复未另起 change)· commit `7f91abd9`(缺陷消解 + 断言补齐)

### Entry #84 — 2026-10-27 [build]

**主题**: 队列 #7 `cpptlm-cep-cohif-carrier` **Phase 1 实施** —— `CohIfBundle` 载体落地 + 反向 fixture + 真源同步

**① 触发**

用户指令「请开始队列 #7 Phase 1」。Phase 1 = 载体面(`CohIfBundle`),判据 **CC-01 / CC-02**,共 8 个 task。

**② 交付物**

| # | 产物 | 落点 |
|---|------|------|
| 1 | `bundles::CohIfBundle`(**12 字段**) | `include/bundles/cep_coherence_bundles_tlm.hh`(新建) |
| 2 | `bundles::CohIfFlowId`(**强类型**,域恰好 5 值) | 同上 |
| 3 | 12 条 `BUNDLE_FIELD_WIDTH` 锁 + `is_trivially_copyable` 头内断言 | 同上 |
| 4 | 枚举感知的宽度 trait(`BUNDLE_FIELD_WIDTH` 覆盖枚举字段) | `include/utils/port_contract_asserts.hh`(改,**纯增量**) |
| 5 | 共享扫描器 + 毒化副本生成器 | `test/cep_cohif_contract.hh`(新建) |
| 6 | 扫描探针 + 编译探针 + 反向验证脚本 | `scripts/test/cep_cohif_scan_probe.cc` · `cep_cohif_compile_probe.cc` · `cep_cohif_reverse_verify.sh`(新建) |
| 7 | 真源同步 | `06 号`(§1.3 / §1.4 图 / §2 / §3.2 / §3.3)· `01 号`(§1.2)· `07 号`(§5.4 + §11 + 第四则 Status Update) |

**③ 两个关键技术取舍(均在 delta spec 契约面内,非决策变更)**

| 取舍 | 内容 | 理由 |
|------|------|------|
| **`flow_id` 用强类型枚举而非 `ch_uint<8>`** | `enum class CohIfFlowId : std::uint8_t`,恰好 5 个枚举值 | CC-01 ③ 要求域外 5 条流「**不可能**出现在 `flow_id` 域内(⇒ **类型保证**,非运行期纪律)」—— 裸整数会让它们在类型层仍可表达 ⇒ 与 `design.md D-α` 的裁决相反。**代价**:枚举无成员 ⇒ `BUNDLE_FIELD_WIDTH` 取不到 `::width` ⇒ 宽度 trait 增枚举分支(**纯增量**,既有 `ch_uint<W>` 字段行为逐字不变)⇒ CC-02 ③「覆盖**全部**字段」仍成立 |
| **`is_trivially_copyable` 断言写在头文件而非仅测试 TU** | 头内 `static_assert` + 探针 TU 内同款断言 | 该断言是**类型**的结构性不变量(带虚函数 ⇒ 引入 vptr ⇒ 破坏 ChStream 按值拷贝)⇒ 随类型走比只在测试里断言更强;CC-02 ② 要求的「test TU 内」断言由探针 + Phase 6 的 test TU **镜像**保留 |

**④ 反向 fixture(4 类,实测有牙)**

| fixture | 毒化 | 对应判据 | 期望 | 实测 |
|---------|------|---------|------|------|
| `flow_count_6` | 注入第 6 个 `flow_id` | CC-01 ① | 静态扫描**红** | ✅ 红(精确报出「域大小 = 6」) |
| `network_field` | 注入 `ch_uint<32> src_node;` | CC-01 ② | 静态扫描**红** | ✅ 红(报出「出现网络层字段:src_node」) |
| `width_plus_one` | `ch_uint<64>` → `ch_uint<65>` | CC-02 ③ | 编译**失败** | ✅ 失败(`BUNDLE_FIELD_WIDTH` 静态断言触发) |
| `virtual_function` | 注入虚函数 | CC-02 ② | 编译**失败** | ✅ 失败(`is_trivially_copyable` 静态断言触发) |

**⑤ 实施中发现并修正的 2 个自身缺陷(诚实登记)**

| # | 缺陷 | 后果 | 修正 |
|---|------|------|------|
| 1 | 毒化测试**误覆盖真文件**(毒化副本写到仓内真头,因该文件尚未入库 ⇒ `git checkout` 无法还原) | 真头被污染 | **重写真头**并改用**隔离 include 目录**(`-I <tmp>/shadow/<kind>`)做毒化 ⇒ 后续全程零污染 |
| 2 | 反向验证脚本把 **4 个毒化写进同一文件** | 后一个毒化盖掉前一个 ⇒ 扫描读到的是最后一次的毒化 ⇒ **判据「假绿」**(首版实测 2 条 RED 期望落 GREEN) | 每个毒化**各自独立目录** ⇒ 重测 11/11 通过 |

**⑥ 真源同步(计数 + 归类)**

- 新增 1 个 `.hh` ⇒ `ls include/bundles/*.hh` **17 → 18**、`bundle_base` 派生类型 **25 → 26**、Bundle 类型总数 **26 → 27**。
- `06 号`:§3.2 归类表增 `cep_coherence_bundles_tlm.hh::CohIfBundle` 行(第三类)+ 计数 17→18;§3.3「唯一实例」→「已落地(1 个)」;§1.3 第 3 项「不引入新 Bundle 类型」加**例外**;§2 + §1.4 计数同步。
- `01 号`:§1.2 计数 17/26 → 18/27(**旧值加时态锚定**,不删除历史订正)。
- `07 号`(冻结面):§5.4 **第三类同步** —— 第一则附录 ③ 声明的「**待队列 #7 实施后再同步**」**本次即该同步点**;§11 活口径行同步;追加第四则 Status Update。冻结清单按两步授权序列更新。

**⑦ 验收(实测)**

| 项 | 值 |
|---|---|
| `cep_cohif_reverse_verify.sh` | **11/11 通过**(4 类毒化全部有牙 + 基线绿 + 扫描器自检正反控) |
| `CohIfBundle` 编译期锁 | `sizeof == 96` · `is_trivially_copyable` · 12 条 `BUNDLE_FIELD_WIDTH` 全通过 |
| `06 号` / `01 号` / `07 号` 计数同步 | `ls include/bundles/*.hh` = **18**;`bundle_base` 派生类型 = **26** |
| 冻结集成员数 | **40**(不变) |

**⑧ 关联**: Entry #83(P2 收口)· #81(队列 #7 转正)· #80(队列 #6 归档)· #74(`ADR-CEP-19` 签发)· commit `7f91abd9`(Phase 0 前置)

### Entry #85 — 2026-10-27 [build]

**主题**: 队列 #7 `cpptlm-cep-cohif-carrier` **Phase 2 实施** —— ProtocolEngine 端口面 + `NullProtocolEngine` 默认实现

**① 触发**

用户指令「继续」(承接 Entry #84 的 Phase 1)。Phase 2 = 端口面(`ProtocolEngine` 基类),判据 task **2.1 ~ 2.7**(共 7 个 task)。

**② 交付物**

| # | 产物 | 落点 |
|---|------|------|
| 1 | `tlm::cep::ProtocolEngineBase<N>`(派生 `ChStreamModuleBase`) | `include/tlm/cep/protocol_engine_base.hh`(新建) |
| 2 | `tlm::cep::NullProtocolEngine<N>` + 10 流覆盖矩阵 + 2 条编译期锁 | 同上 |
| 3 | `tlm::cep::CanonicalFlow`(canonical 10 流枚举)+ `kCanonicalFlowNames` | 同上 |
| 4 | Bundle ↔ 平台枚举的**编译期值域交叉断言**(13 条) | 同上 |
| 5 | 毒化副本生成器(`MatrixDropOne` / `BehaviorDegrade`) | `test/cep_protocol_engine_contract.hh`(新建) |
| 6 | 编译期锁探针(含 `--poison` 模式)+ 反向验证脚本 | `scripts/test/cep_protocol_engine_compile_probe.cc` · `cep_protocol_engine_reverse_verify.sh`(新建,已注册 ctest) |
| 7 | 真源同步 | `07 号`(§11 两行 + 第五则 Status Update)· `ADR-CEP-07`(第二则 Status Update)· `tasks.md` · `phase-10.md` |

**③ 三个关键技术取舍**

| 取舍 | 内容 | 理由 |
|------|------|------|
| **端口数 N 取编译期模板参数** | `template <std::size_t N> class ProtocolEngineBase` | `ChStreamAdapterFactory::registerBidirectionalPortAdapter<ModuleT, BundleT, N>` 的 `N` 是**模板参数**(`include/framework/chstream_adapter_factory.hh:103`),而 adapter 内部按 `m->req_in()[i]` 索引 ⇒ N 必须编译期确定。否则「adapter 的 N」与「模块的运行时端口数」会出现**两个真源**且可能**静默分歧**。附带 `static_assert(N > 1)`(N=1 落 `bind_ports` **空操作 shim** ⇒ 静默不绑定) |
| **协议桥持**非拥有**指针 + 前向声明** | `cpptlm::ProtocolBridgeEr* bridge_`;完整定义留待 tasks 3.1 | 非拥有 ⇒ 无需完整类型即可析构 ⇒ **Phase 2 不依赖 Phase 3 的产物**;生命周期由 ModuleFactory / 装配链负责(per tasks 3.6「注册与消费成对」) |
| **10 个 `on_*` 为纯虚 + 矩阵为单一真源** | `kNullFlowMatrix`(C 数组 + 显式 size)+ `expected_null_behavior()` 规则函数 | ① 纯虚 ⇒ 「必须全 10 流」由**类型系统**保证(per `ADR-CEP-07` 决策 4 草图注释 + oracle v0.4 B6);② 期望行为编码为**规则**而非第二份表 ⇒ 矩阵与期望不会互相漂移;③ C 数组 + 显式 size ⇒ 反向 fixture「缺 1 流」表现为「size 变 9」并被**完整性**断言抓到(若用 `std::array<T,10>`,少一元素会因**初始化器个数不足**而编不过 ⇒ 判据被「错误的原因」满足) |

**④ 反向 fixture(2 类,实测有牙)**

| fixture | 毒化 | 期望 | 实测 |
|---------|------|------|------|
| `matrix_drop_one` | 删掉矩阵中的 barrier 项(size 9) | **覆盖完整性** `static_assert` 触发 | ✅ 编译失败,错误信息精确命中「10 流覆盖矩阵必须恰好覆盖 canonical 10 流」 |
| `behavior_degrade` | barrier 的 `ImmediateComplete` → `NoOp` | **逐流行为** `static_assert` 触发 | ✅ 编译失败,精确命中「逐流行为必须与 07 号 §6.4 口径一致」 |

⇒ 已注册 ctest `cep_protocol_engine_reverse_verify`(6/6 通过);探针内含**正控**(干净头必须编译成功)。

**⑤ 诚实登记:3 项已知偏离/缺口**

| 项 | 内容 | 归属 |
|---|------|------|
| 落点路径偏离 | `ADR-CEP-07` 决策 4 草图画在 `include/tlm/cep/null_protocol_engine.hh`;实现落 `protocol_engine_base.hh`(per tasks 2.5)⇒ **路径偏离已登记**(语义零变化) | `ADR-CEP-07` 第二则 Status Update ② |
| 接口形态细化 | 草图的 `address_resolve(&home_node, &coherent)` 双出参 ⇒ 改用结果 struct;草图的 `barrier(domain, direction, txn_id)` 含 `direction` ⇒ 实现取 `barrier(domain, txn_id)`(`direction` 在本仓**无定义** ⇒ 不发明未定义语义) | `ADR-CEP-07` 第二则 Status Update ③ |
| **`BidirectionalPortAdapter::tick()` 硬编码 `NoCFlitBundle` 语义** | 该 adapter 读 `flit_category` 决定 `PKT_REQ`/`PKT_RESP`,且 credit 用 `NoCFlitBundle` ⇒ 用它承载 `CohIfBundle` 需要**泛化** | Phase 4 / 5(已在 `07 号` 第五则 ④ 登记) |

**⑥ 附带产出:Bundle ↔ 平台枚举的编译期交叉断言(13 条)**

`include/bundles/` 是最底层、**不**反向依赖 `include/tlm/cep/` ⇒ 值域对应无法在 bundle 头内断言。Phase 1 的头注释曾承诺「值域对应关系由测试层做**编译期交叉断言**」⇒ **本次在 PE 头内兑现**:`Perm`(4 值)· `SnoopType`(3 值)· `LineState`(5 值) ↔ `bundles::CohIfBundle` 的对应常量**逐值 `static_assert`** ⇒ 任何一侧改值序都会**编译失败**,而非静默错配。

**⑦ 验收(实测)**

| 项 | 值 |
|---|---|
| `cep_protocol_engine_reverse_verify.sh` | **6/6 通过** |
| PE 探针(干净头) | 编译成功(正控) |
| `NullProtocolEngine` 行为 | `no_op=1` / `immediate_complete=2` / `immediate_resolve=1`;`per_flow` 逐流计数;`address_resolve` ⇒ `home_node = node_id`、`coherent = false` |
| `cpptlm_tests` 全仓 | 见本 commit 的门禁记录 |
| 冻结集成员数 | **40**(不变) |

**⑧ 关联**: Entry #84(Phase 1 载体面)· #83(`NullProtocolEngine` owner 指派)· #81(队列 #7 转正)· #74(`ADR-CEP-19` 签发)

---

### Entry #86 — 2026-10-28 [build]

**主题**: 队列 #7 `cpptlm-cep-cohif-carrier` **Phase 3 实施** —— ProtocolBridge 特化桥 + `register_protocol_bridge` 死钩子 → 活钩子 + per-bridge 元数据

**① 触发**

用户指令「继续」(承接 Entry #85 的 Phase 2)。Phase 3 = 特化桥,判据 task **3.1 ~ 3.9**(共 9 个 task)。

**② 交付物**

| # | 产物 | 落点 |
|---|------|------|
| 1 | `cpptlm::ProtocolBridgeEr`(类型擦除)+ `ProtocolBridge<InB,OutB,Ordered>` + `AbstractProtocolBridge<FlitT>` | `include/framework/protocol_bridge.hh`(新建) |
| 2 | `tlm::cep::CohIfNoCProtocolBridge`(CohIf ↔ NoC 特化桥,**两层** pack/unpack) | `include/tlm/cep/cohif_noc_bridge.hh`(新建) |
| 3 | `tlm::cep::ProtocolFamily` + 数字编号唯一真源表 | `include/tlm/cep/protocol_family.hh`(新建) |
| 4 | 钩子扩展:`register_protocol_bridge(name, factory_lambda)` + `get_bridges()` 转 public + 2 个可单测判据函数 | `include/core/module_factory.hh` |
| 5 | 消费路径:`ModuleFactory::instantiateAll` **Step 7.5** | `src/core/module_factory.cc` |
| 6 | 可选挂载点:`ChStreamModuleBase::attach_protocol_bridge`(默认 false ⇒ 零波及) | `include/core/chstream_module.hh` |
| 7 | `DeclaredInstance::family` 字段(避免重复解析协议实例 JSON) | `include/tlm/cep/channel_dependency_graph.hh` |
| 8 | 可复用判据函数 + 毒化副本生成器 | `test/cep_protocol_bridge_contract.hh`(新建) |
| 9 | 契约测试(CC-03 / CC-05 / CC-06)+ 5 类反向 fixture | `test/test_cep_protocol_bridge_contracts.cc`(新建) |
| 10 | 运行期反向验证脚本(已注册 ctest) | `scripts/test/cep_protocol_bridge_probe.cc` + `cep_protocol_bridge_reverse_verify.sh`(新建) |
| 11 | 真源同步 | `06 号`(§4.4.1 落地登记 + 行号对照表 + §9.3 实施)· `06-bis` · `07 号`(第六则)· `ADR-CEP-19`(第二则)· `ADR-CEP-07`(第三则)· `ADR-CEP-07A`(第三则)· `ADR-CEP-00`(第五则)· `tasks.md` · `phase-10.md` |

**③ 核心架构发现(本批最重要的产出):「Bundle 级逐字段 round-trip 恒等」在数学上不可达**

- **实测**:`CohIfBundle` 12 字段 = 236 bit;`NoCFlitBundle` 16 字段 = 332 bit,其中网络层字段 112 bit ⇒ 网络可承载 CohIf 的仅 210 bit;CohIf 独有 aux 需 35 bit vs 可用 10 bit ⇒ 缺口 ≈ 25 bit。
- **压宽度 / 按流复用 / 借用 `hops`** 三条绕路**全部不通**;**根本原因**:`CohIfBundle.flow_id` 域 = **5 值**,`NoCFlitBundle.flit_category` 域 = **3 值**,而队列 #6 的 6→3 消息类映射表是**刻意多对一**的 ⇒ **5→3 多对一有损映射** ⇒ 仅靠网络 flit 字段空间**不可能**唯一恢复 `flow_id`(**信息论下界**,非工程细节)。
- **裁决(2026-10-28 DRI)**:**B′ 方案** —— 协议语义落 **Packet payload 数据区**(`06 号 §4.4` 原型**已有** `pack_with_packet`/`unpack_with_packet` ⇒ 零新增抽象、零新增类型;**不**用 `tlm_extension` ⇒ 不触 `06 号 §4.5` 的 P3+ 排除项)。
- **判据形态**:**两层** —— ① 内容层(`pack`/`unpack`)= 共有字段 4 个双向无损 + 协议语义字段显式登记;② 端到端层(`pack_with_packet`/`unpack_with_packet`)= 完整 12 字段逐字段恒等。⇒ **判据未被削弱反而被加强**(端到端层覆盖**跨真实网络**的往返)。
- **已排除**:新增专用网络 Bundle 类型 / 收窄 `CohIfBundle` 宽度(违反 `06 号 §3.4` 第③项)/ 改 `NoCFlitBundle` 加 sideband(破 16 字段编译期锁)/ 放宽判据到「可承载子集」(判据被阉割)。

**④ 三个关键技术取舍**

| 取舍 | 内容 | 理由 |
|------|------|------|
| **挂载点用 `ChStreamModuleBase` 的可选虚函数** | `attach_protocol_bridge(ProtocolBridgeEr*)` 默认返回 false | `ProtocolEngineBase<N>` 的 `N` 是**模板参数** ⇒ ModuleFactory 无法 `dynamic_cast` 到具体类型;独立 mixin 与 `SimObject` **无继承关系** ⇒ `dynamic_cast` **编译不过**(实测);让 mixin 继承 `SimObject` ⇒ **菱形继承** ⇒ 更糟。默认 false ⇒ **零波及** |
| **判据抽成 public 静态函数** | `validate_bridge_metadata(...)` / `check_bridge_id_collision(...)` | 反向 fixture 必须能在**不**跑完整 `instantiateAll` 的前提下断言「违反 ⇒ 红」⇒ 判据逻辑**不可**只存在于装配流程的局部变量里(否则两份实现会漂移) |
| **「只注册不消费」不判装配失败** | 以**死桥清单** `unconsumed_bridges()` 暴露,由判据测试断言其非空 | 多拓扑共用进程级注册表时某个拓扑不引用全部桥是**正常**的 ⇒ 强制失败会误伤 |

**⑤ 反向 fixture(5 条,实测有牙且**因正确原因**红)**

| fixture | 毒化/违反 | 期望 | 实测 |
|---------|----------|------|------|
| 只注册不消费 | 注册 1 桥 + 配置不引用 | `unconsumed_bridges()` 非空 | ✅ 红 |
| 重复 `bridge_id_` | 2 桥同 id | 判据红 + 点名冲突双方 | ✅ 红 |
| `protocol_id_` 与 `family` 不符 | 编号 +1 | 判据红 + 给出两边编号 | ✅ 红 |
| `perm` → `flit_category` | 篡改桥实现 | 字段映射探针红 | ✅ 红(报「`perm` 泄漏」+「`flow_id` 跨类别未改变网络 flit」) |
| `address` ↔ `data` 对调 | 篡改桥实现 | 内容层 round-trip 红 | ✅ 红(报「`address`/`data` 不相等」) |

⇒ 已注册 ctest `cep_protocol_bridge_reverse_verify`(**9/9**);探针内含**正控**(干净实现必须全绿)。

**⑥ 判据抓到的真实缺陷(已修正)—— 诚实登记**

「丢弃清单 ↔ 实际行为」判据用**行为式**判定(逐字段变更 → 看网络 flit 是否跟着变)。初版把 `flow_id` 列为「丢弃」,但它**确实**影响 `flit_category` ⇒ 判据**假红** ⇒ 修正为**字段三分类**(共有 4 / 降维 1 / 丢弃 7)。
⇒ 这条记录的价值:**判据不是装饰** —— 它抓到了设计本身的分类错误(而非只有实现 bug)。

**⑦ 其他踩坑记录(4 条)**

| 坑 | 现象 | 修法 |
|---|---|---|
| **依赖基类的非静态数据成员** | `++stats_` 报「`stats_` was not declared in this scope」 | 按 [temp.dep.expr] 必须写 `this->stats_`(两阶段名字查找) |
| **锚点用 regex** | 毒化生成器报「锚点未找到」 | 锚点含 `(` `)` 等 regex 元字符 ⇒ 改**子串查找** |
| **Step 7.5 查 member `instances`** | 报「目标模块 `pe0` 不存在」 | member `instances` 在 `instantiateAll` 末尾才 `= std::move(object_instances)` ⇒ Step 7.5 必须查**局部** `object_instances` |
| **注册表 early-return + 用例间污染** | 后一个用例的 factory 从未被调用 | `register_protocol_bridge` 对同名 early-return(ODR-safe)⇒ 用例必须用**独立桥名** + **RAII 守卫**清理注册表(否则某个用例的 `REQUIRE` 失败会跳过清理) |

**⑧ 验收(实测)**

| 项 | 值 |
|---|---|
| `cep_protocol_bridge_reverse_verify.sh` | **9/9 通过** |
| `cpptlm_tests "[protocol-bridge]"` | **14 case / 105 assertion 全绿** |
| `cpptlm_tests "[cep]"` | **175 case / 2764 assertion 全绿**(原 161 / 2638) |
| `cpptlm_tests` 全仓 | **1957 case / 873210 assertion 全绿**(原 1943 / 873084) |
| `ctest -R cep` | **31/31 全绿**(新增 `cep_protocol_bridge_reverse_verify`) |
| 门禁 | `cep_freeze_check` / `cep_docs_sync_check` / `cep_doc_ref_check` / `cep_decl_alignment_check` / `cep_archforge_alignment_check` / `cep_gate_selftest` / `cep_cohif_reverse_verify` / `cep_protocol_engine_reverse_verify` / `cep_protocol_bridge_reverse_verify` / `docs_sync_check --strict` / `openspec validate --changes --strict` 全 rc=0 |
| 队列 #7 task 进度 | **25 / 39 已勾** |
| 冻结集成员数 | **40**(不变) |

**⑨ 已知遗留(归 Phase 4 / 5)**

| 项 | 归属 |
|---|---|
| `REGISTER_CHSTREAM_BIDIRECTIONAL` 宏 + CEP 类注册 ⇒ `INV-CEP-07-IC4` 仍无载体 | Phase 4 / 5 |
| `BidirectionalPortAdapter::tick()` 硬编码 `NoCFlitBundle` 语义 ⇒ 承载 `CohIfBundle` 需泛化 | Phase 4 / 5(已在 `07 号` 第五则 ④ 登记,第六则 ⑥ 状态不变) |
| `module_factory.hh` / `chstream_module.hh` **行号漂移**(+11 / +1) | 已在 `06 号` 文末「附录 · 行号对照表」登记;5 个冻结面产物以各自 Status Update 登记(正文按「ADR 不可变」不改) |

**⑩ 关联**: Entry #85(Phase 2 端口面)· #84(Phase 1 载体面)· #83(`NullProtocolEngine` owner 指派)· #81(队列 #7 转正)· #78(判据增补追认)· #74(`ADR-CEP-19` 签发)

---

### Entry #87 — 2026-10-28 [build]

**主题**: 队列 #7 `cpptlm-cep-cohif-carrier` **Phase 4 实施** —— `REGISTER_CHSTREAM_BIDIRECTIONAL` 装配宏 + `BidirectionalPortAdapter` Bundle 无关化

**① 触发**

用户指令「继续」(承接 Entry #86 的 Phase 3)。Phase 4 = 装配宏,判据 task **4.1 ~ 4.4**。

**② 交付物**

| # | 产物 | 落点 |
|---|------|------|
| 1 | `REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)`(两件都做 + `static_assert(n > 1)`) | `include/chstream_register.hh` |
| 2 | `cpptlm::BundleDirectionTraits<BundleT>`(方向判定 + credit 载体;未特化 ⇒ 编译期报错) | `include/framework/bidirectional_port_adapter.hh` |
| 3 | `BundleDirectionTraits<NoCFlitBundle>` 特化(**逐行为等价**于泛化前的硬编码) | 同上 |
| 4 | `BundleDirectionTraits<CohIfBundle>` 特化(CohIf 5 流 → REQ/RESP) | `include/tlm/cep/cohif_direction_traits.hh`(新建) |
| 5 | `ProtocolEngineBase::receive_credit`(默认空实现 ⇒ 零波及) | `include/tlm/cep/protocol_engine_base.hh` |
| 6 | 共享测试 PE 派生类(`tlm::cep::testing::TestProtocolEngine`) | `test/cep_test_protocol_engine.hh`(新建) |
| 7 | 编译期锁探针 + 反向验证脚本(已注册 ctest) | `scripts/test/cep_chstream_bidirectional_macro_probe.cc` + `cep_chstream_bidirectional_macro_reverse_verify.sh`(新建) |
| 8 | 运行期契约测试(宏「两件都做」+ 方向映射不漂移) | `test/test_cep_chstream_bidirectional_macro.cc`(新建) |
| 9 | 真源同步 | `06 号`(3 处行号)· `07 号`(第七则 + §11 活口径)· `ADR-CEP-19`(第三则)· 2 份 improvement 草稿(时态锚定)· `tasks.md` · `phase-10.md` |

**③ 实施中发现的阻塞前提(本批最重要的发现):`BidirectionalPortAdapter` 硬编码 `NoCFlitBundle`**

该 adapter 的 `tick()` 读 `flit.flit_category` 决定 `PKT_REQ`/`PKT_RESP`,且 `send_credit_impl` 用
`bundles::NoCFlitBundle` 构造 credit ⇒ **承载 `CohIfBundle` 编译不过**(`CohIfBundle` 无
`flit_category` / `vc_id` 字段)⇒ **不泛化则 task 4.1~4.4 无法用 CohIfBundle 验证**。

⇒ **解法**:抽成 `cpptlm::BundleDirectionTraits<BundleT>` —— **框架层不认识任何具体 Bundle**,
各 Bundle 在**自己的层**提供特化。⚠️ **未特化的 BundleT ⇒ 编译期报错**(不静默用错方向)。
⚠️ **层次纪律**:特化放 CEP 侧(`tlm/cep` → `framework` ✓),**不放** `include/bundles/`
(bundles 是最底层,**不得**反向依赖 `core/` / `framework/`)。
⚠️ **`NoCFlitBundle` 特化逐行为等价**于泛化前实现 ⇒ **全量回归 0 变化**(1961 case / 873234 assertion)。

**④ 三个关键技术取舍**

| 取舍 | 内容 | 理由 |
|------|------|------|
| **宏体「两件都做」** | `registerObject` + `registerBidirectionalPortAdapter` | 缺 `registerObject` ⇒ ModuleFactory 无法实例化该类型;缺 adapter 注册 ⇒ Step 7 拿不到适配器。⚠️ **不**照抄 `REGISTER_CHSTREAM_DUAL` 的**半截实现**(它只注册 adapter、零调用点) |
| **宏形态 = 参数化 + 函数体内使用** | `REGISTER_CHSTREAM_BIDIRECTIONAL(T, B, N);` | 与 `REGISTER_CHSTREAM`(函数体内调用)同风格;`REGISTER_MODULE` 的逗号表达式形态是给**全局作用域**用的,两套并存 |
| **`static_assert` 放宏体第一条语句** | `static_assert((n) > 1, ...)` | 判据 CC-04 ③ 的**唯一**防线 —— N=1 落 `bind_ports` **空操作 shim** ⇒ 静默不绑定(不报错)⇒ 编译期锁是唯一可检形态 |

**⑤ 反向 fixture(1 类,实测有牙且**因正确原因**红)**

| fixture | 毒化 | 期望 | 实测 |
|---------|------|------|------|
| N=1 | 用 N=1 展开宏 | 编译失败**且**原因命中 `static_assert` | ✅ 编译失败,错误信息精确命中「端口数 N 必须 > 1」(脚本检查错误文本 ⇒ 若失败原因是别的则判红);N=2 ⇒ 编译成功(正控) |

⇒ 已注册 ctest `cep_chstream_bidirectional_macro_reverse_verify`(3/3)。

**⑥ 踩坑记录(3 条)**

| 坑 | 现象 | 修法 |
|---|---|---|
| **探针用 `-std=c++17`** | 正控编译失败,原因是 `std::println`(`module_group.hh`)—— **与判据无关** ⇒ 正控假红 | 探针 include `chstream_register.hh` ⇒ 拉进 `module_group.hh` ⇒ 必须用 **C++23**(与项目一致) |
| **共享测试 PE 放错命名空间** | `cpptlm::cep::testing` ⇒ 找不到基类(「expected template-name before '<' token」)—— `ProtocolEngineBase` 在 `tlm::cep`,而 `cpptlm::cep` 是**另一个**命名空间(测试辅助 `read_file` 所在) | 改放 `tlm::cep::testing` |
| **测试文件缺 `chstream_register.hh` include** | 宏未声明 ⇒ 展开成乱码 | 补 include(宏在既有宏同一头) |

**⑦ 诚实登记:2 项「已完成 ≠ 判据已闭合」**

| 项 | 状态 |
|---|---|
| `INV-CEP-07-IC4`(CEP 模块注册走既有宏体系) | **仍无载体** —— Phase 4 交付的是**宏(能力)**,**没有** CEP 类用它注册 ⇒ 归 Phase 5。**宏是能力,不是注册** |
| 协议引擎的 **credit 回收语义** | 未建模 —— `ProtocolEngineBase::receive_credit` 是**默认空实现** ⇒ 归 Phase 5 |

**⑧ 验收(实测)**

| 项 | 值 |
|---|---|
| `cep_chstream_bidirectional_macro_reverse_verify.sh` | **3/3 通过** |
| `cpptlm_tests "[chstream-macro]"` | **4 case / 15 assertion 全绿** |
| `cpptlm_tests` 全仓 | **1961 case / 873234 assertion 全绿**(原 1957 / 873210;**泛化 0 回归**) |
| 门禁 | 9 项脚本 + `docs_sync_check --strict` + `openspec validate --changes --strict` 全 rc=0 |
| 队列 #7 task 进度 | **30 / 40 已勾**(Phase 4.5 是实施中发现的阻塞前提,登记为独立 task ⇒ 总数 39 → 40) |
| 冻结集成员数 | **40**(不变) |

**⑨ 行号漂移登记**

`include/framework/bidirectional_port_adapter.hh` 因泛化整体下移(`bind_port_pair` 128→**171**、
`bind_ports` 空 shim 141→**184**、`pending_credits_` 105→**148**、`receive_credit` 调用 184→**225**、
`send_credit` 216→**257**、`send_credit_impl` 228→**269**);**`CATEGORY_CREDIT` 写入点** 235→**109**
(迁入 trait 特化,**语义不变**)⇒ 「全仓唯一写 `CATEGORY_CREDIT` 的点」的**位置**变了 ⇒ 相关陈述已加
**时态锚定**。⚠️ `include/chstream_register.hh` 的既有行号(**50 / 119 / 121 / 175 / 177**)**零漂移**。

**⑩ 关联**: Entry #86(Phase 3 特化桥)· #85(Phase 2 端口面)· #84(Phase 1 载体面)· #81(队列 #7 转正)· #78(判据增补追认)· #74(`ADR-CEP-19` 签发)

---

### Entry #88 — 2026-10-28 [build]

**主题**: 队列 #7 `cpptlm-cep-cohif-carrier` **Phase 5 实施** —— JSON 装配连通性 + `INV-CEP-07-IC4` 闭合 + egress 通道语义

**① 触发**

用户指令「继续」(承接 Entry #87 的 Phase 4)。Phase 5 = JSON 装配连通性,判据 task **5.1 ~ 5.4**。

**② 交付物**

| # | 产物 | 落点 |
|---|------|------|
| 1 | `CohIfNullProtocolEngine`(`NullProtocolEngine<2>` 的**非模板可注册外壳**,10 流行为**完全继承**) | `include/tlm/cep/cohif_protocol_engine.hh`(新建) |
| 2 | `cpptlm::register_chstream_bidirectional<>` 共享实现(两个宏复用,避免漂移) | `include/chstream_register.hh` |
| 3 | `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED`(注册表 key 用**短名**) | 同上 |
| 4 | CEP PE 注册 + 协议桥 `cohif_noc` 全局注册 | 同上(`REGISTER_CHSTREAM` 宏体内) |
| 5 | `cpptlm::EgressChannel` + `BundleDirectionTraits::egress_channel()` | `include/framework/bidirectional_port_adapter.hh` |
| 6 | `CohIfBundle` 的 `egress_channel() == Req` 特化 | `include/tlm/cep/cohif_direction_traits.hh` |
| 7 | `ProtocolEngineBase::tick()` 驱动 adapter(与 `RouterTLM::tick()` 惯例一致) | `include/tlm/cep/protocol_engine_base.hh` |
| 8 | `ModuleFactory::getInstanceForTesting`(只读取实例,`CPPTLM_TESTING` 段) | `include/core/module_factory.hh` |
| 9 | 最小 PE 拓扑配置(2 PE,N=2,双向互连 + `cep.bridges`) | `configs/cep_cohif_pe_minimal.json`(新建) |
| 10 | 端到端 + 反向 fixture 测试(5 case) | `test/test_cep_cohif_json_assembly.cc`(新建) |
| 11 | 真源同步 | `06 号`(3 处行号 + 附录漂移表)· `07 号`(第八则 + §11 活口径)· `ADR-CEP-19`(第四则)· `tasks.md` · `phase-10.md` |

**③ 实施中发现的判据前提:egress 走哪条通道(登记为 task 5.5)**

框架的每个端口有 **4** 个端口对象(`req_out` / `req_in` / `resp_out` / `resp_in`)⇒ egress 走哪条**不是**纯实现细节,
而是**判据可判别性**的前提 ⇒ 由 Bundle 专属 trait 声明
(`cpptlm::BundleDirectionTraits<BundleT>::egress_channel()`:`Resp` 默认 = `RouterTLM` 既有行为;`Req` = `CohIf` 语义)。

⚠️ **实测纠偏(避免留下错误口径)**:**两种通道都能把数据送到对端 ingress** —— 框架的
`ChStreamTargetPort::recvReq` 对 `req_in` / `resp_in` **一视同仁**,都投递到
`process_request_input` ⇒ 「走 `resp_out` 会丢数据」的推断**不成立**(实施初稿的注释即此说法,
**已按实测改正**)。

⇒ `CohIf` 选 `req_out` 的**真实理由**是两条:
① **方向语义正确** —— `resp_out` 装的是「**对方→我**」那条 JSON 连接的**响应**通道,
egress 借用它会**倒置连接方向**;
② **判据有牙** —— egress 走 `req_out` 时,「断开我→对方的连接」**确实切得断** egress。

**实测证据(对照实验)**:把 `CohIfBundle` 的 `egress_channel` 改成 `Resp` ⇒
反向 fixture「dst 指向不存在的端口索引」**假通过**(失败信息 `3 != 3`)—— 因为另一条 JSON
连接仍把 `resp_out` 接到对端 ⇒ egress 未被真正断开。
⇒ 该 trait 值已作为**具名判据**钉在 `test/test_cep_cohif_json_assembly.cc`。

**④ 判据形态与纪律**

| 项 | 形态 |
|---|---|
| 正向 | 一次 CohIf 事务**穿过** adapter 抵达对端 ⇒ 断言对端 canonical 流**计数器递增** + 发起侧**未**收到自己的事务 + 其它流**未**被误触发(**非仅「绑定成功」**) |
| 反向 ① | **无连接** ⇒ `ReachOutcome::NotArrived` |
| 反向 ② | **dst 指向不存在的端口索引** ⇒ 非 `Arrived` |
| ⚠️ **纪律** | 辅助函数返回 `ReachOutcome` **枚举**(`AssemblyFailed` / `NotInstantiated` / `NotArrived` / `Arrived`)⇒ 断言「**目标结局**」而非 `!= Arrived` ⇒ 防「装配被拒」被误判成「端到端不通」(**因错误的原因红**) |

**⑤ 三个关键技术取舍**

| 取舍 | 内容 | 理由 |
|------|------|------|
| **可注册外壳 vs 直接注册模板类** | 新建 `CohIfNullProtocolEngine`(非模板)而非直接注册 `NullProtocolEngine<2>` | ① `#mod_type` 字符串化会得到 `"tlm::cep::NullProtocolEngine<2>"`(含尖括号)—— 本仓注册表惯例是**短名**;② `NullProtocolEngine` 是**库内实现细节**(per `INV-CEP-07-IC2`),不应直接做 JSON 里的类型名;③ 外壳**零新增协议语义**(10 流行为完全继承) |
| **共享 helper + 两个宏** | `cpptlm::register_chstream_bidirectional<>(type_name)` + `REGISTER_CHSTREAM_BIDIRECTIONAL` / `_NAMED` | 避免两份实现漂移;`static_assert(n > 1)` 留在**宏**里(它必须作用于**调用点的字面参数**,函数模板做不到) |
| **协议桥全局注册 vs 配置内联** | `register_protocol_bridge("cohif_noc", factory)` 在 `REGISTER_CHSTREAM` 内;**消费**由 `config["cep"]["bridges"]` 驱动 | 注册与消费**分离** ⇒ 多拓扑共用进程级注册表时,某个拓扑不引用全部桥是**正常**的(per Step 7.5「死桥**不判红**,以清单暴露」) |

**⑥ 踩坑记录(4 条)**

| 坑 | 现象 | 修法 |
|---|---|---|
| **宏体行尾续行符 `\` 丢失** | 替换 `REGISTER_CHSTREAM` 宏体时漏了 `\` ⇒ 宏定义**提前结束** ⇒ 后续行被当**顶层代码**解析 ⇒ GCC 报 `expected unqualified-id before 'do'`(位置指向**宏体内**的 `do`,极具迷惑性) | 每次改多行宏体,逐行核对**行尾 `\` 必须在最后一列**(`cat -A` 验字节);⚠️ python 里写续行要用 `\\\\`(否则 python 自己做了行拼接) |
| **`ModuleFactory` 不可拷贝赋值** | 辅助函数里 `factory = ModuleFactory(&eq)` ⇒ `use of deleted function`(内含 `unique_ptr`) | 辅助函数**自持** `EventQueue` + `ModuleFactory` |
| **`OutputStreamAdapter` 无 `set_valid`** | 误用 `set_valid(true)` | 正确 API 是 `write(bundle)`(写数据**并**置 valid) |
| **计数器索引不是 `CohIfFlowId`** | `per_flow[]` 的索引是 **canonical 10 流**(`CanonicalFlow`) | 用 `CanonicalFlow::Request` 等显式索引(前 5 值恰好同序,但语义不同) |

**⑦ 诚实登记:3 项「已完成 ≠ 判据已闭合」**

| 项 | 状态 |
|---|---|
| 协议引擎的 **credit 回收语义** | 未建模 —— `ProtocolEngineBase::receive_credit` 是**默认空实现** ⇒ 归 Phase 6 |
| `ProtocolEngineBase::tick()` 驱动 adapter | 已改(与 `RouterTLM::tick()` 惯例一致),但**协议引擎侧**的 tick 语义尚未在 `07 号 §6.4` 展开登记 ⇒ 归 Phase 6 |
| 「无连接 ⇒ `NotArrived`」的**装配层**语义 | 断链有**两种**形态:配置层拒绝(端口索引非法)vs 传输层不通 ⇒ 反向 fixture ② 覆盖前者、① 覆盖后者,**两者不可合并** |

**⑧ 验收(实测)**

| 项 | 值 |
|---|---|
| `cpptlm_tests "[json-assembly]"` | **5 case / 34 assertion 全绿** |
| `cpptlm_tests "[protocol-bridge]"` | **14 case / 104 assertion 全绿** |
| `cpptlm_tests` 全仓 | **1966 case / 873276 assertion 全绿**(原 1961 / 873234;**0 回归**) |
| 门禁 | 12 项脚本 + `docs_sync_check --strict` + `openspec validate --changes --strict` 全 rc=0 |
| 队列 #7 task 进度 | **35 / 41 已勾**(Phase 5.5 是实施中发现的**判据前提**,登记为独立 task ⇒ 总数 40 → 41) |
| `INV-CEP-07-IC4` | **已闭合**(CEP 类注册走既有宏体系 + JSON 装配端到端通过) |

**⑨ 行号漂移登记**

`include/chstream_register.hh` Phase 5 在**前部**插入「共享 helper + 2 个宏」(+57)、在
`REGISTER_CHSTREAM` 宏体**内部**插入「CEP PE 注册 + 协议桥注册」(+12)⇒ 合计 **+69**
(`REGISTER_CHSTREAM` 50 → **107**、`registerObject<tlm::RouterTLM>` 62 → **119**、
`registerObject<tlm::NICTLM>` 63 → **120**、`registerBidirectionalPortAdapter<tlm::RouterTLM>`
119 → **188**、`registerDualPortAdapter<tlm::NICTLM>` 121 → **190**、`REGISTER_CHSTREAM_DUAL`
175 → **244**、其宏内 `registerDualPortAdapter` 177 → **246**)。
`include/framework/bidirectional_port_adapter.hh` 再 +24/+26/+35(`EgressChannel` 枚举等)。
⚠️ **一处「零漂移」结论已失效**:Entry #87 登记的「`chstream_register.hh` 既有行号零漂移」是**当时**
口径(Phase 4 把新宏插在 `REGISTER_CHSTREAM_DUAL` **之后**)⇒ 已被 Phase 5 推翻。
完整对照表见 `06 号` 文末「附录 · 行号对照表(Entry #88)」。

**⑩ 关联**: Entry #87(Phase 4 装配宏)· #86(Phase 3 特化桥)· #85(Phase 2 端口面)· #84(Phase 1 载体面)· #81(队列 #7 转正)· #78(判据增补追认)· #74(`ADR-CEP-19` 签发)

---

### Entry #89 — 2026-10-28 [build]

**主题**: 队列 #7 `cpptlm-cep-cohif-carrier` **Phase 6 + Phase 7 实施** —— 判据收口 + 反向 fixture 登记 + 零 diff 复核

**① 触发**

用户指令「继续剩余的任务」。Phase 6 = 测试收口(tasks 6.1 ~ 6.3);Phase 7 = 闸与真源同步(tasks 7.1 ~ 7.3)。

**② 交付物**

| # | 产物 | 落点 |
|---|------|------|
| 1 | `test/test_cep_cohif_carrier.cc`(CC-01 / CC-02 / CC-04;17 case / 52 assertion) | 新建 |
| 2 | `test/cep_cohif_assembly_helper.hh`(端到端辅助函数 + `ReachOutcome` 结局枚举) | 新建 |
| 3 | `scan_cohif_width_locks()` + `cohif_expected_width_locks()`(CC-02 ③ 宽度锁表扫描) | `test/cep_cohif_contract.hh` |
| 4 | `test/test_cep_cohif_json_assembly.cc` 改为消费共享 helper(消除重复实现) | 改写 |
| 5 | **反向 fixture 登记表**(判据 ID · fixture 形态 · **登记位置**,13 条) | `scripts/README.md`(新建小节) |
| 6 | 4 个反向 fixture 脚本补登记(此前**未**登记在守门脚本表 / `ONBOARDING §5.5`) | `scripts/README.md` + `docs/ONBOARDING.md` |
| 7 | ctest 显式登记 3 条 | `test/CMakeLists.txt` |
| 8 | 真源同步 | `07 号`(第九则 + §11 活口径)· `ADR-CEP-19`(第五则)· `tasks.md` · `phase-10.md` |

**③ Phase 6:判据面覆盖情况**

| 判据 | 覆盖 | 用例 |
|------|------|------|
| **CC-01** ①②③ | 静态扫描(域大小 / 字段集 / 域外流名)+ 强类型枚举的编译期锁(`is_enum` + 底层类型 + 上界 == 4 + 域外枚举项**不存在**) | `test_cep_cohif_carrier.cc` |
| **CC-02** ①②③ | TU 内 `static_assert(sizeof == 96)` + `static_assert(is_trivially_copyable)` + **宽度锁表扫描**(12 字段全覆盖) | 同上 |
| **CC-04** ①②③ | 端到端(事务穿过 adapter 抵达对端)+ N ≥ 2 + N=1 编译失败 | 同上 |
| **CC-03 / CC-05 / CC-06** | Phase 3 已交付(`test_cep_protocol_bridge_contracts.cc`),Phase 6 复核覆盖完整 | 既有 |

⚠️ **CC-06 引用队列 #6 的 AC-04 实现**(`tlm::cep::category_value_domain()`,来自
`include/tlm/cep/adapter_schema_contracts.hh`)⇒ **不重立第二份判据**(task 6.3)。

**④ 判据纪律的 3 处落实**

| 纪律 | 落实 |
|------|------|
| **编译期锁不得只以「编译红」充当 ctest 判据** | `test_cep_cohif_carrier.cc` 用 `popen` 调编译器,断言「编译**确实**失败」**且**「失败原因**命中**预期的那条 `static_assert`」(宽度锁 ⇒ 输出含 `Bundle field width mismatch`;trivially copyable ⇒ 含 `trivially copyable`;N=1 ⇒ 含 `端口数 N 必须 > 1`) |
| **纯负向断言必须有正控** | 扫描器自检 `cohif_scan_self_check()` + 干净头**编译成功**正控 + 干净头扫描绿正控 |
| **反向 fixture 因正确原因红** | `ReachOutcome` **结局枚举**分类(`AssemblyFailed` / `NotInstantiated` / `NotArrived` / `Arrived`)⇒ 断言「**目标结局**」而非 `!= Arrived` |

**⑤ 反向 fixture 实测有牙(毒化验证)**

临时删掉**头文件**与**编译探针**两处 `is_trivially_copyable` 锁 ⇒
「CC-02 反向 fixture ④」**报红**(17 case 中 1 failed)⇒ 恢复后全绿。
⇒ 证明该 fixture **不是**「因错误的原因红」,也**不是**恒绿。

**⑥ 踩坑记录(2 条)**

| 坑 | 现象 | 修法 |
|---|---|---|
| **非依赖 `requires` 是硬错误** | `static_assert(!requires { bundles::CohIfFlowId::Barrier; })` ⇒ 编译错「`Barrier` is not a member」—— 非依赖的 requires-expression 在非模板上下文**不**走 SFINAE | 改成**依赖** concept(`template <typename T> concept HasBarrierEnumerator = requires { T::Barrier; };`)⇒ 查找失败变成 substitution failure ⇒ concept 求值为 `false` |
| **concept 不能定义在块作用域** | 「a template declaration cannot appear at block scope」 | concept 提到**匿名 namespace** 顶部(用例体内只引用) |

**⑦ 诚实登记:1 处口径偏离 + 2 项「已完成 ≠ 语义已建模」**

| 项 | 状态 |
|---|---|
| **task 7.1 字面指向 `cep_decl_reverse_verify.sh`** | **未改动该脚本** —— 它守的是「**协议实例声明 ↔ schema 一致性**」域(`fabric.json` / `protocol_instances.json` / ArchForge 声明),与本 change 的判据域**零交集** ⇒ 在其中加本 change 的 fixture **等于测另一个闸**(违反「不重发明」)⇒ 改按「**登记位置补齐**」落地(守门脚本表 + `ONBOARDING §5.5` + **反向 fixture 登记表** + ctest 显式条目)⇒ 实质要求(每条判据的 fixture 有可执行登记入口)**已闭合** |
| 协议引擎的 **credit 回收语义** | 未建模(`receive_credit` 默认空实现)⇒ `07 号 §6.4` 未展开登记 ⇒ 归**后续 change** |
| `ProtocolEngineBase::tick()` 驱动 adapter | 已改(框架惯例一致),协议引擎侧语义同样待后续 change 登记 |

**⑧ 验收(实测)**

| 项 | 值 |
|---|---|
| `cpptlm_tests "[cep][cohif-carrier]"` | **17 case / 52 assertion 全绿** |
| `cpptlm_tests "[cep][protocol-bridge]"` | **14 case / 104 assertion 全绿** |
| `cpptlm_tests "[cep][canonical-invariant]"` | **13 case / 687 assertion 全绿** |
| `cpptlm_tests "[cep]"` | **201 case / 2876 assertion 全绿** |
| `cpptlm_tests` 全仓 | **1983 case / 873322 assertion 全绿**(原 1966 / 873276 ⇒ **+17 case / +46 assertion**,**0 回归**) |
| `ctest -R cep` | **35/35**(新增 3 条显式条目) |
| 门禁 | 9 项 CEP 脚本 + `docs_sync_check --strict` + `openspec validate --changes --strict` 全 rc=0;`cep_gate_selftest` rc=0 |
| 队列 #7 task 进度 | **41 / 41 全勾**(总数 39 → 41) |
| 7.2 零 diff | `test/test_cep_canonical_invariants.cc` **零 diff** + 全 change 范围**未新增** `INV-CEP-*` 定义 ⇒ **新增 invariant 数 = 0** |
| `StubCacheCore` | **零 diff**(全 change 范围) |

**⑨ 关联**: Entry #88(Phase 5 JSON 装配连通性)· #87(Phase 4 装配宏)· #86(Phase 3 特化桥)· #85(Phase 2 端口面)· #84(Phase 1 载体面)· #81(队列 #7 转正)· #74(`ADR-CEP-19` 签发)

---

### Entry #90 — 2026-10-28 [build]

**主题**: 队列 #7 `cpptlm-cep-cohif-carrier` **归档** —— 「✅ 已完成归档」语义(2 份 delta spec 合并进 main specs)

**① 触发**

用户裁决:队列 #7 已全完成(41/41 task、6 条判据全闭合、门禁全绿)⇒ 授权执行 `openspec archive`
(「**已完成归档**」语义 —— per AGENTS.md「⏸ 两种归档语义必须区分」表)。

**② 归档动作**

```bash
openspec archive cpptlm-cep-cohif-carrier -y
```

⇒ 输出:`Task status: ✓ Complete` · `cep-cohif-carrier: create` · `cep-protocol-bridge-contracts: create`
· `Totals: + 2, ~ 0, - 0, → 0` · `archived as '2026-10-28-cpptlm-cep-cohif-carrier'`。

| 项 | 结果 |
|---|---|
| **归档语义** | ✅ **已完成归档**(实现已落地、契约已在系统内)⇒ **合并** delta spec 进 `openspec/specs/` |
| 新建 main spec | `openspec/specs/cep-cohif-carrier/spec.md`(4.4 KB)+ `openspec/specs/cep-protocol-bridge-contracts/spec.md`(4.7 KB) |
| change 目录 | 移至 `openspec/changes/archive/2026-10-28-cpptlm-cep-cohif-carrier/` —— ⚠️ **不入库**(per `.gitignore:64`)⇒ 5 个原 tracked 文件在 git 视角为 **删除** |
| KPI | active **1/3 → 0/3**(`openspec list --json` 实测 `0 → []`) |
| ⚠️ **未用** `--skip-specs` | 「已完成归档」**必须**合并 delta spec(per AGENTS.md 硬约束 1:`--skip-specs` 仅用于「⏸ 暂存归档」)⇒ 本次**不适用** |

**③ 活文档引用同步(per AGENTS.md「⏸ 两种归档语义必须区分」硬约束 3)**

归档使路径 `openspec/changes/cpptlm-cep-cohif-carrier/` → `openspec/changes/archive/2026-10-28-cpptlm-cep-cohif-carrier/`
⇒ 全仓**现时**引用点已逐处处理:

| 位置 | 处理 |
|---|---|
| `.rddf/improvements/cpptlm-cep-cohif-carrier.md` 状态行 | 改为「✅ **已归档**」+ 新路径 + **时态锚定**(「2026-10-27 转正时的活跃路径 **已于 2026-10-28 归档时失效**」) |
| `.rddf/roadmap/objectives/obj-cep-platform.md` §4.0 行 | 状态列 → 「✅ **已归档**」+ KPI 1/3 → 0/3 + 路径时态锚定;实施进度尾 → 「已归档」 |
| `.rddf/roadmap/phases/phase-10.md` | 「待归档」→「已归档」+ 归档语义登记 |
| 其余 **7 处**引用(台账 Entry #81 / #84 / #85 / #89 的**历史记录**、`git mv` 命令回放) | **不改** —— 那是**历史事实的记录**,改写即篡改台账(per 单源纪律「旧值引用需带时态锚定」⇒ 历史记录自带时态) |

⚠️ **未受影响**:冻结面产物(`07 号` / `ADR-CEP-19`)**正文与 Status Update 均无** change 目录路径引用 ⇒ 本次归档**不触发**冻结清单重算(`cep_freeze_check` rc=0,40 产物不变)。

**④ 真源自足性(per `tasks.md`「已知边界」)**

归档把 §6.4 的 6 条判据**并入 main specs** ⇒ 判据进入**真源层** ⇒ 本 change 的
「判据定义真源在附属层(`docs/research/…`)、ADR 不自足」这一**真源层级缺陷**由归档**闭合**
(per `design.md D-η` 的「真源自足的收口」路径 ①)⇒ **无需**再追加 ADR Status Update 固化。

**⑤ 验收(实测)**

| 项 | 值 |
|---|---|
| `openspec list --json` | **0** active(`0 → []`) |
| `openspec validate --changes --strict` | rc=0(无 active change 可校验 ⇒ 通过) |
| `openspec validate cpptlm-cep-cohif-carrier --strict`(归档前) | `Change 'cpptlm-cep-cohif-carrier' is valid` rc=0 |
| 门禁 | 12 项脚本全 rc=0(含 `cep_freeze_check` —— **未**因归档变红) |
| 全仓 `cpptlm_tests` | **1983 case / 873322 assertion** 全绿(**0 回归**;归档不触碰实现) |
| `ctest -R cep` | **35/35** |

**⑥ 归档引发的守门连锁修(3 处,均为归档的直接后果)**

| # | 守门报红 | 根因 | 处置 |
|---|---------|------|------|
| 1 | **KPI baseline 与实测(0/3)不一致**(6 个文件) | 并发从 1/3 → **0/3** ⇒ 活文档的「当前态」表述仍写 `1/3` | 逐处改为 `0/3` + 把「2026-10-27 转正 → 占 1 slot」改为「2026-10-28 归档 → slot 已释放」并把原句降级为**历史**锚点(6 文件:`obj` / `phase-10` / `umbrella` improvement / `roadmap.md` / `AGENTS.md` / `ADR-CEP-00`) |
| 2 | **移交链断言 ⑦**:「行内提及移交标记,但其 change `cpptlm-cep-cohif-carrier` 已归档 → 无主债(应重指向活队列行)」 | `S1-N` 移交标记 + 3 项 `⏭(孤儿)` 登记**只挂在行 #7**;行 #7 的 change 现已归档 ⇒ 活义务**无主** | 按 **Entry #80 的同款先例**(#6 → #7)把登记**转指行 #8**(`cpptlm-cep-chi-rnf-subset`,活):§4.0 / §4.2 两处同步;行 #7 只留**无标记**的指针(「owner 自队列 #6 → #7 → 现转指队列 #8」) |
| 3 | `cep_gate_selftest` 的 2 个**变异 fixture** 报「rc=0 期望 1」 | 变异打在**行 #7**(已无标记)⇒ 断言 ⑦ 不 fire ⇒ fixture 假失败 | fixture 锚点**跟随 owner 行**:改变异 `cpptlm-cep-chi-rnf-subset`(行 #8)⇒ 34/34 全绿 |

⚠️ **第 3 项的可复用教训**:守门自身的**变异 fixture** 锚点必须**跟随**「被变异对象的位置」;
owner 转指(归档)时,若 fixture 锚点不跟随 ⇒ fixture **假失败**(不是守门失效)——
2026-10-28 实测即踩到(Entry #80 的同款转指在 #7 → #8 时再次发生)。

**⑦ 关联**: Entry #89(Phase 6/7 判据收口 + 反向 fixture 登记)· #88(Phase 5 JSON 装配连通性)· #87(Phase 4 装配宏)· #86(Phase 3 特化桥)· #85(Phase 2 端口面)· #84(Phase 1 载体面)· #81(队列 #7 转正)· #80(同款 owner 转指先例)· #74(`ADR-CEP-19` 签发)

---

**版本**: v2.1(2026-10-28,per 队列 #7 归档;Entry #84 ~ #90)
**最后**: ⚠️ AGENTS.md 中**不存在** `<!-- AUTO: objectives -->` 哨兵段(实测 0 命中)→ 路线图索引需**手工同步** `.rddf/roadmap.md` §Objectives / §AUTO-INDEX
**作者**: CppTLM build agent(planner mode)