---
id: objective-cep-platform
status: active
created: 2026-10-20
last_revised: 2026-10-20
review_by: 2027-01-18
owner: rdd-planner
priority: P1
manual_deps:
  - phase-10
theme: Coherence Exploration Platform(CEP)— 跨 10 季度协议中立一致性互联仿真平台治理
kind: objective
phase_refs:
  - phase-10
---

# objective-cep-platform: CEP 跨 10 季度治理叙事

> **写入权**: rdd-planner(per `rdd-planner/SKILL.md:156` + ADR-0028 + ADR-0054 D5 唯一写入方)
> **schema**: `_lib/schemas/objective_schema.json` v1(9 必填字段齐;`kind` / `phase_refs` 为扩展字段)
> **关联**: `.rddf/roadmap/phases/phase-10.md`(CEP 实施 phase)+ `.rddf/improvements/cpptlm-cep-umbrella.md`(5-segment 草稿)
> **关联真源**: 10 份 ADR v0.6 `docs/adr/ADR-CEP-{00,00A,01,02,03,04,05,06,07} + NUMBER-MAP.md`
> **关联变更**: `cpptlm-cep-umbrella`(Stage 0)+ Stage 1~3 9 个串行 change(2026-Q4 ~ 2029-Q1)

---

## §0 Goal

> **⚠️ architect 阶段同步(2026-10-21,两批)**:invariant 计数已同步为 **67 编号位 / 66 实际 / 15 组**(第一批新增 `ADR-CEP-08`/`09`/`12`/`13` + `ADR-CEP-07-IC7` + `ADR-CEP-06-IC5`;第二批新增 `ADR-CEP-11`/`14`/`15`/`16`/`07A`)。**阶段重排、DoD 7 分解(10 → 15 组)、change 清单**等非机械部分已由 planner 阶段完成。计数单一真源 = `docs/architecture/07-coherence-exploration-platform.md §4`。

跨 10 季度(2026-Q4 ~ 2029-Q1)推进 Coherence Exploration Platform(CEP)从"协议中立基线"(06 §1.1 v2.1.10 已落地 60-70%)走向"5 协议族(CHI / CXL 2.0+3.0 对称 / TileLink-C / NVLink / UALink)可插拔 + 实验产物可比较"的完整探索平台,期间守 10 个 change 串行 KPI 排队 + **66 条守门 invariant**(编号位 67,per `ADR-CEP-06/07` + oracle v0.4 B5 命名空间统一)。

**完成定义 (DoD, 2029-Q1 终)**:

1. ✅ 10 个 OpenSpec change 全部 archived with sync
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

> **验收层级**:本 DoD 是**终态判定**;逐条 assertion 在 `cpptlm-cep-umbrella.md` §Acceptance(Stage 0)与各 change 的 OpenSpec AC 落地。本文件**不重复**逐条 assertion,避免三处复述漂移(per oracle S1 修订)。
8. ✅ ModuleFactory 集成 0 发明宏形式(per oracle v0.4 B7)
9. ✅ simulation_mode 单一维度(functional / timing),不引入 network.mode
10. ✅ **§9 研究问题清单 8 个假设全部有实验结论**(per §9.1)
11. ✅ §11 决策台账记录所有 KPI 让位决策 + Go 节点地址 + Sprint 复盘
12. ✅ §8 Kill / Replan 判据未触发(或已按判据处置)

> **验收层级委托**(per oracle S1 修订):本 DoD 是**终态判定**;具体条款的机器验证在 `cpptlm-cep-umbrella.md` §Acceptance(Stage 0)与各 change 的 OpenSpec AC 落地。本文件**不重复**逐条 assertion,避免三处复述漂移。

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
- CEP 三项全命中:跨 10 季度叙事 / §11 append-only 台账刚需(KPI 并发 ≤1 + 季度让位可追溯)/ Stage 晋级复盘仪式

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
- ❌ **6 协议平行** — Stage 0~3 串行排队(per `ADR-CEP-00 §2 Decision 3` 10 季度)

---

## §3 架构依据(引用 ADR,per roadmap/SKILL.md:73 §3 语义)

| 依据 | ADR | 决策要点 |
|------|-----|---------|
| 归口 + 边界豁免 | `ADR-CEP-00` §2 Decision 1/3/4 | 代码全归 CppTLM;单 DRI;AGENTS.md:423 不触发 |
| 6 层 + 完成域 | `ADR-CEP-01` §2 Decision 1/2 | Fabric Collective 旁路平面;requester-scoped vs group-scoped |
| canonical 10 流 | `ADR-CEP-02` §2 | 10 条流 + CEN-Ib1~10 不变量 |
| SHARP 内部 | `ADR-CEP-03` §2 + §3 | CollectivePlaneIf;R1-R5 防误分类;CEP-13 non-cacheable 语义 |
| D-5 重构 | `ADR-CEP-04` §2 + §3 | caps descriptor 替代 bool;LookupResult 枚举;snoop_begin/fold |
| Stub Cache | `ADR-CEP-05` §2 + §3 | RN 投影 6 接口;折叠映射表;10 条契约测试 |
| 实验输出 | `ADR-CEP-06` §2 | 4 件套 + 13 必纳 metric;INV-CEP-06-IC1~4 |
| 仿真 + seam | `ADR-CEP-07` §2 + §3 | simulation_mode 单一维度;ModuleFactory 真实注册;INV-CEP-07-IC1~6 |
| 编号 + 真源锚点 | `ADR-CEP-NUMBER-MAP` §2 + §3 | CEP-08~16 预分配;INV-CEP-NUM-IC1~3 |
| 真源章节映射 | `ADR-CEP-00A` §3 + §4 | 章节行号实证;14 条伪引清单 |

---

## §4 Phases & Changes

> **术语约定**(per metis G2-A 歧义消解):本文件用 **Stage 0/1/2/3** 指 CEP 内部实施段;**roadmap phase-10** 指路线图 phase;**dGPU Phase 2/3**(phase-9)与 **PCIe EP Phase 1-8** 是其它命名空间,勿混。

### 4.1 Stage 0: Umbrella 启动(2026-Q4,5 周)

| 子任务 | 状态 | 范围 | 估时 | 关联 ADR | KPI slot |
|--------|------|------|------|----------|----------|
| 10.A 真源 ADR 起草 | ✅ 完成(2026-10-19~21) | **21 份 ADR**(两批新增 08/09/12/13/18 + 11/14/15/16/07A,修订 8 份) | 12 人日(architect) | ADR-CEP-00~18 全族 + NUM | 0(doc-only)|
| **10.B OpenSpec umbrella** | 🟡 **Proposed(2026-10-21 已创建;`openspec validate --strict` PASS;4 件套已落 / 95 task)** | `cpptlm-cep-umbrella/` proposal + design + specs + tasks | 5 人日(builder) | umbrella improvement | **1**(计 KPI)|
| 10.C 契约测试 | 📋 Planned | `test/test_cep_canonical_invariants.cc` + `test/test_cep_dummy_signals.cc` + `test/test_cep_module_factory_seam.cc` + `test/test_cep_simulation_mode_lock.cc` | 10 人日(builder) | ADR-CEP-06 + ADR-CEP-07 | 0 |
| 10.D experimental + tools | 📋 Planned | `tools/render_report.py` + `run_sweep.py` + `ab_compare.py` + `metrics.json` schema | 5 人日(builder) | ADR-CEP-06 §4 | 0 |
| 10.E 顶层真源 | ✅ **完成(2026-10-20~21)** | `docs/architecture/07-coherence-exploration-platform.md` 整合 **21 份** ADR(657 行) | 5 人日(architect,已消耗) | ADR-CEP-00 §5.2 | 0 |
| 10.F ADR 锚点 lint | 📋 Planned | `scripts/test/cep_docs_sync_check.sh` 机器检查(覆盖 **16 份** ADR) | 6 人日 | ADR-CEP-00A §5.1 | 0 |
| **10.G capability schema** | 📋 Planned | `include/tlm/cep/protocol_instance.schema.json` + `INV-CEP-12-IC1~3` 断言 | 3 人日 | ADR-CEP-12 §2 + §3 | 0(沿用 10.B)|

**Stage 0 总估时**:**41 人日**(builder + architect 混合;**已消耗 13 人日** = 10.A 8 + 10.E 5;**剩余 28 人日** = 10.B 5 + 10.C 10 + 10.D 5 + 10.F 6 + 10.G 3 —— per oracle S5 人日口径 + 2026-10-21 architect 阶段修订)

### 4.2 Stage 1: Stub Cache + Tracker + Plane + 3 协议 RN-F Subset(2026-Q4 ~ 2027-Q2)

**串行约束**:每 change 独占 1 KPI slot,任一时刻仅 1 个 active(per §5.4)。

| 顺序 | Change | 季度 | 关联 ADR | 估时 |
|------|--------|------|----------|------|
| 1 | **`cpptlm-cep-poc-spike`** ← 2026-10-22 新增(P-1,置首位) | 2026-Q4 | `ADR-CEP-13`(判据 (b) 的**可执行**判定 = Q-3e)+ `ADR-CEP-09`(顶点升级)+ `ADR-CEP-12`(per-network VC) | **3 周**(spike,非实现) |
| 2 | `cpptlm-cep-stub-cache-core` | 2026-Q4 | ADR-CEP-05 + ADR-CEP-04 stub | 8 周 |
| 3 | **`cpptlm-cep-directory-ordering-plane`** ← 2026-10-21 重排(原 `sharer-tracker-base` 扩容) | 2026-Q4 ~ 2027-Q1 | **ADR-CEP-04**(Sharer Tracker 5 实现 + caps)+ **ADR-CEP-08**(Address Resolver + Ordering Point 三判据)+ **ADR-CEP-09 Decision A**(装配期通道依赖图无环检查) | **12 周**(原 8 周) |
| 4 | `cpptlm-cep-collective-plane-base` | 2027-Q1 | ADR-CEP-03 + ADR-CEP-01 | 8 周 |
| 5 | `cpptlm-cep-chi-rnf-subset` | 2027-Q1 ~ 2027-Q2 | ADR-CEP-01 + ADR-CEP-02 + **ADR-CEP-12**(首个实例声明:填 `ordering_assumption` / `vc_binding` / `consistency_labels` / `sharer_tracker`) | 8 周 |
| 6 | `cpptlm-cep-cxl-nonsymmetric` | 2027-Q2 | ADR-CEP-04 + ADR-CEP-03 | 8 周 |
| 7 | `cpptlm-cep-tilelink-tree-manager` | 2027-Q2 ~ 2027-Q3 | ADR-CEP-04 + ADR-CEP-08 + ADR-CEP-12 | 8 周 |

**Stage 1 跨度**:2026-Q4 ~ 2027-Q3(**7** 个 change 串行)

> **P-1 增补说明(2026-10-22,per 台账 Entry #12)**:`cpptlm-cep-poc-spike` 是**可丢弃的 spike**
> (单文件 / 不进 `ModuleFactory` / 不注册 ABI / 无公共 API),**不是**实现 —— 只验证「可组合性 + 契约自洽」。
> **置首位**的理由:其 S2 场景是 `ADR-CEP-13` 判据 (b) 的**可执行**判定 → **解阻 A6** →
> A6 是全部协议插件 change 的前置(`perm` 表达力未定)→ 它是唯一能**加速**其余 Stage 1 change 的项。
> 用例规格:`docs/implementation/2026-10-22-cep-poc-spike-spec.md`。
> **排队长度 +1,并发不变**(仍守「任一时刻 Proposed ≤ 1」)。

### 4.3 Stage 2: CXL 对称 + NVLink SHARP(2027-Q3 ~ 2028-Q1)

| 顺序 | Change | 季度 | 关联 ADR | 估时 |
|------|--------|------|----------|------|
| 7 | `cpptlm-cep-cxl-dcoh-symmetric` | 2027-Q3 ~ 2027-Q4 | ADR-CEP-04 §3.5 | 10 周 |
| 8 | `cpptlm-cep-nvlink-sharp` | 2027-Q4 ~ 2028-Q1 | ADR-CEP-03 + ADR-CEP-01 | 10 周 |

### 4.4 Stage 3: UALink INC(2028-Q1 ~ 2028-Q2)

| 顺序 | Change | 季度 | 关联 ADR | 估时 |
|------|--------|------|----------|------|
| 9 | `cpptlm-cep-ualink-inc` | 2028-Q1 ~ 2028-Q2 | ADR-CEP-03 | 10 周 |

### 4.5 Stage 4+ 远期(2028-Q2 ~ 2029-Q1)

- `CEP-08` / `CEP-09` / `CEP-12` / `CEP-13` —— ✅ **已起草 v0.1(2026-10-21)**,实现按 §4.2~§4.4 分 Stage 落地
- `CEP-11` 协议版本锁定(待 architect 起草,Stage 1 前)
- `CEP-14` Stub 可验证性合同测试(待 architect 起草)
- `CEP-15` CacheTLM 对齐(待 architect 起草)
- `CEP-16` 06 §15.4 演进对应(待 architect 起草)
- `CEP-07A` 06-bis 既有产物处置(待 architect 起草)
- `CEP-18` 协议生成路线范围 —— ✅ 已起草 v0.1;**Stage 1~3 不依赖任何生成器**(per `ADR-CEP-18` 决策 1)
- **维护模式**:Stage 4+ 进入维护态(新协议接入走 add-feature + 单 change,不再串行排队)

### 4.6 终态时间口径(per oracle B6 + metis G2-G 修订)

- **Stage 0~3 实施完成**:2028-Q2
- **Stage 4+ 维护期**:2028-Q2 ~ 2029-Q1
- **objective 终态验收**:2029-Q1(与 §12 review 节奏对齐)

---

## §5 KPI 预算与治理

### 5.1 KPI 单一真源(per oracle B1 修订)

**唯一真源**: `ADR-CEP-00 §2 Decision 3` — 本文件 / `phase-10.md` / `cpptlm-cep-umbrella.md` / `roadmap.md` **只引用,不自行重算**。

### 5.2 当前真实 baseline(2026-10-20 实证,per oracle B1)

`openspec/changes/` 当前 **0 个 active**(2026-10-20 实证, per `openspec list`):
- 原 3 个 active(`cpptlm-msix-generator-ip` 0/44 / `cpptlm-pcie-tlp-entry-layer` 0/56 / `cpptlm-p2-integration-unblock` 0/45)**均 0% 未实施**
- 2026-10-20 三者全部 **⏸ 暂存归档**(`openspec archive --skip-specs`),delta spec **未合并**进 main specs,proposal 已加 ⏸ 标记
- 归档路径:`openspec/changes/archive/2026-10-20-{cpptlm-msix-generator-ip,cpptlm-pcie-tlp-entry-layer,cpptlm-p2-integration-unblock}/`

→ **当前 Proposed 并发 = 1/3**(2026-10-22 修正:`cpptlm-cep-umbrella` 因**跨仓准入门(M2)已放行**升为 **Proposed**,占用 1 个 KPI slot;baseline 0/3 → 1/3。放行依据见 §11 **Entry #10**)

> **✅ M2 准入门放行(2026-10-22)**:ArchForge 侧 coherence 接口**规约文档**已合入
> (commits `6020f42` + `8290b4c` + `ac956b8`,8+3+2 文件),且本仓侧已完成
> **判据 C-4 的机器化**(检查器 + schema delta + fabric 载体,反向验证 12/12 全绿,见 Entry #9)。
> ⚠️ **Stage 0 仍不 archive** —— R10(独立性缺口)未闭合,与 M2 无关。

### 5.3 CEP 启动的前置条件(per AGENTS.md §DOC HYGIENE line 422 新提案门槛)

> **创建 `cpptlm-cep-umbrella` 前须先 archive ≥1 个既有 active change**,把 baseline 降到 2/3,创建后回到 3/3。

**✅ 前置条件已满足(2026-10-20)**:3 个 active 全部 **⏸ 暂存归档**(`--skip-specs`),baseline **0/3** → `cpptlm-cep-umbrella` **可立即创建**。

**✅ 已创建(2026-10-21)**:builder 起草完成 —— proposal + design + specs(`cep-core-contracts`)+ tasks **4 件套全落**,`openspec validate cpptlm-cep-umbrella --strict` **PASS**;baseline **0/3 → 1/3**。**⚠️ 2026-10-21 Task 8.4 Step 4 修正**:因**跨仓准入门未放行**,该 change 停留 **Proposed Draft** → baseline 回到 **0/3**(Proposed slot 只统计已达 Proposed 的 change)。

⚠️ **原候选路径已过期**:"路径 C: archive `cpptlm-gmmu-pte-bus-correction`" —— 该 change **已于 2026-10-12 实际归档**,不再是 active,不能再作为 archive 对象。真实候选是上列 3 个(均已暂存归档)。

**决策须登记 §11 台账**(`kind: go-decision`)。

### 5.4 串行约束

- 任一时刻 **Proposed 并发 ≤ 1**(CEP 系列内部)
- 全仓 Proposed 并发 ≤ 3(AGENTS.md 硬上限)
- 11 个 change 串行 11 季度(per §5.1 真源;2026-10-22 per Entry #12 由 10 → 11)

---

## §6 验收

### 6.1 Stage 0 验收(2026-Q4)

委托 `cpptlm-cep-umbrella.md` §Acceptance A1~A6(不重复复述,per oracle S1 修订)。

### 6.2 Stage 1~3 验收(2028-Q2)

- B1. 10 个 OpenSpec change 全部 archived with sync
- B2. **66 条守门 invariant 机器检查 100% 覆盖**(编号位 67,按 §0 DoD 7 的 **15 组**分解)(per §0 DoD 7)
- B3. **6 个 ProtocolEngine 实施完成**:CHI RN-F / CXL 2.0 非对称 / CXL 3.0 对称 DCOH / TileLink-C / NVLink / UALink(per metis G2-F 修订:5 协议族 → 6 个 adapter);每个实例须按 `ADR-CEP-12` schema 声明能力与前提假设
- B8. **Directory & Ordering Plane 三子平面实施完成**:Address Resolver + Sharer Tracker 5 实现 + Ordering Point 三判据 + **装配期通道依赖图无环检查**(per `ADR-CEP-08` + `ADR-CEP-09 Decision A`)
- B4. 4 件套实验输出契约落地 + 13 必纳 metric 全绿
- B5. ModuleFactory 集成 seam + 0 发明宏形式
- B6. simulation_mode 单一维度
- B7. §9 研究问题 8 个假设全部有实验结论

### 6.3 KPI 平衡验收

- D1. 任一时刻 CEP 系列 Proposed 并发 ≤ 1
- D2. §11 决策台账完整记录 10 季度 KPI 让位
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
| **R5** | ⚠️ **无外部验收方**(DRI = 自己给自己验收)| 🟡 中(**形式已解,实质未闭合**) | §9.2 三项角色 2026-10-21 已指定(形式 gate 解除);但三者指向同一人 → 独立性以 **M1 产物冻结 / M2 跨仓准入门 / M3 对拍基准外置 / M4 独立 AI 复审** 四条机制部分替代;**真外部真人评审待建立** |
| **R6** | 与 phase-9 资源竞争(共用 builder 队列 + 执行容量)| 🟡 中 | §8.2 优先级仲裁规则 |
| **R7** | 探索平台结论不可信(无对拍验证)| 🟡 中 | **Stage 1 前须定义对拍手段**(gem5 Ruby / ArchForge 参考模型 / RTL benchmark 三选一) |
| **R8** | ArchForge 单边依赖(准入门把阻塞全转嫁 CppTLM 侧)| 🟡 中 | 需 ArchForge 侧排期承诺;缺承诺时 §9 假设降级为"仅 CppTLM 内部对比" |
| **R10** | **验收独立性实质缺口**(2026-10-21 新增)—— §9.2 三角色均指向单一作者;M1~M4 只是**部分**替代 | 🟡 中 | 纳入 Stage 0 验收清单的 M4(独立 AI 复审);真外部真人评审待建立;**不在本轮闭合**。**2026-10-21 Task 8.4 结论**:Stage 0 验收等级**降级为「部分独立」**(partial independence)—— M1 冻结 ✅ / M2 准入门 ⛔未放行 / M3 对拍基准 ⏳Stage 1 前定义 / M4 独立 AI 复审 ✅(2 轮,6 BLOCKER 全处置);**不得表述为「完全独立验收」**,且 M4 **不可**替代真人外部评审 |
| **R9** | **单 change 体积上升**(2026-10-21 新增)—— `cpptlm-cep-directory-ordering-plane` 8→12 周(ADR-CEP-08 + 09 Decision A 并入)| 🟡 中 | 同平面合并是模型自洽而非将就;若该 change 延期 >2 季度,走 §8.2 `Replan` 拆 `phase-11`(并评估建 feature fragment)|

### 8.2 Kill / Pivot / Replan 判据(per metis G4 盲点补充)

| 触发条件 | 处置 |
|---------|------|
| **Kill**:Stage 0 完成后(2026-Q4)验收发现 4 件套实验输出无法产出可比较数据(即 §9 假设无法被验证)| 终止 CEP,归档 objective,回退到 `06-bis` 路线 |
| **Pivot**:Stage 1 中期发现协议族选型需变(如放弃 TileLink,改 UALink 优先)| 走 §11 `scope-change` entry + 重排 §4 表 |
| **Replan**:任一 Stage 延期 >2 季度,或全仓 Proposed 并发连续 2 季度无法为 CEP 腾 slot | 触发 §11 `deferral-rationale` entry + 重排 §4 季度列 + 评估是否拆分 roadmap phase(phase-11) |
| **Priority 仲裁**(R6):CEP 与 phase-9 同时需要唯一 slot | 默认 **phase-9 优先**(业务交付 > 探索平台),CEP 让位并登记 §11 deferral-rationale |

### 8.3 退出 / 归档判据(per metis G4 盲点 2)

Stage 4+ 进入维护态后,objective 归档条件:
- 全部 11 个 change archived with sync
- §9 假设全部有结论
- §8.2 未触发 Kill / Pivot
- 交接文档完成(CEP 使用手册 → `docs/architecture/07-coherence-exploration-platform.md` 扩充)

---

## §9 研究问题清单(per metis G1 核心盲点)

> **目的**:防止"交付工具但不产出洞见"。每个 change 至少关联 1 个假设;Stage 3 完成时 8 个假设须全部有实验结论。

### 9.1 假设清单

| # | 假设 | 验证手段 | 关联 Stage | 成功判据 |
|---|------|---------|------------|---------|
| **H1** | 协议抽象层(seam 在 L5↔L4)确实让缓存核心免于协议改动 | Stub Cache 0 协议知识 + 5 adapter 共用同一 stub | Stage 1 | 5 adapter 全部只改 adapter,stub 0 diff |
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

**净结论**:R5 从"🟡 中"降为"🟡 中(**形式已解,实质以 M1~M4 部分替代**)"。**真正的独立评审**待 AI 通道之外的机制(如外部真人评审)建立后再补 —— 这是**已知未闭合**的缺口,不是本轮能解决的。

---

## §10 Next Sprint Candidates(per `rdd-planner/SKILL.md:177-188` LLM 推荐)

> **注**: LLM **仅推荐**,不自动生成 improvement 草稿(违反 rdd-planner boundary)。rdd-builder P0 起草 proposal 时再激活。

### Stage 0 候选(2026-Q4 启动)

| 优先级 | 候选 | 推进建议 | 前置 |
|--------|------|---------|------|
| **P0** | 起草 `cpptlm-cep-umbrella/` proposal + design + specs + tasks | 走 rdd-builder P0;5 人日 | ⛔ `ADR-CEP-00 §5.1/§5.2` 两决策拍板 + KPI slot 腾出(per §5.3)|
| **P0** | 指定 §9.2 三项角色(验收方 / 结论消费者 / 维护方) | planner 协调;2 人日 | ⛔ Stage 0 验收 No-Go 风险 |
| **P0** | 指定 §9.1 H7 对拍基准(gem5 Ruby / ArchForge / RTL benchmark) | 调研 3 人日 | ⛔ R7 |
| P1 | 起草 Stage 1 六个 change 的 improvement 草稿 | 走 add-improve HARD-GATE;每 change 5 人日 | Stage 0 acceptance 全绿 |
| ~~起草 `docs/architecture/07-coherence-exploration-platform.md` 顶层真源~~ | ✅ **已完成 2026-10-21**(architect;657 行) | — |
| P1 | 起草 `ADR-CEP-11` / `ADR-CEP-14` / `ADR-CEP-15` / `ADR-CEP-07A` / `ADR-CEP-16` | 走 architect;每份 3~5 人日 | `ADR-CEP-11` 须 Stage 1 前签发(R2 缓解) |
| P1 | 起草 `scripts/test/cep_docs_sync_check.sh` ADR 锚点 lint | 走 builder;5 人日 | Stage 0 |
| P2 | 实验目录 + tools 3 个(render_report / run_sweep / ab_compare)| 走 builder;5 人日 | Stage 0 |

### Stage 1~3 远期候选(2027-Q1 ~ 2028-Q2)

| 优先级 | 候选 | 推进建议 |
|--------|------|---------|
| P1 | `cpptlm-cep-stub-cache-core` improvement | 走 add-improve;Stage 1.1 启动 |
| P1 | `cpptlm-cep-sharer-tracker-base` improvement | 走 add-improve;Stage 1.2 启动 |
| P1 | `cpptlm-cep-collective-plane-base` improvement | 走 add-improve;Stage 1.3 启动 |
| P1 | `cpptlm-cep-chi-rnf-subset` improvement | 走 add-improve;Stage 1.4 启动 |
| P2 | `cpptlm-cep-cxl-nonsymmetric` improvement | 走 add-improve;Stage 1.5 启动 |
| P2 | `cpptlm-cep-tilelink-tree-manager` improvement | 走 add-improve;Stage 1.6 启动 |
| P1 | `cpptlm-cep-cxl-dcoh-symmetric` improvement | 走 add-improve;Stage 2.1 启动 |
| P1 | `cpptlm-cep-nvlink-sharp` improvement | 走 add-improve;Stage 2.2 启动 |
| P1 | `cpptlm-cep-ualink-inc` improvement | 走 add-improve;Stage 3 启动 |
| P2 | `ADR-CEP-11` 版本锁定(Stage 1 前签发)| 走 architect;触发 R2 缓解 |
| P2 | phase-11 注册后建 feature fragment | 走 rdd-planner 后续评估;≥2 change 跨 phase-10/11 分布时 |

---

## §11 决策台账(append-only,per `rdd-planner/SKILL.md:158`)

> **append-only**:每次新增 entry 追加到段尾;**不可删除 / 不可改写**早期 entry。
> **kind ∈ {deferral-rationale, go-decision, sprint-review, scope-change, adr-amendment}**

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

**影响范围**:`openspec/changes/cpptlm-cep-umbrella/{proposal.md,tasks.md}`(状态 + gate ④)/ 本文件 §5.2(baseline)+ §8.1(R10 缓解栏)/ `docs/implementation/2026-10-21-cep-stage0-contracts.md` §7(处置台账)/ `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`(新)/ `scripts/test/cep_freeze_check.sh`(新)/ `scripts/test/cep_docs_sync_check.sh`(第 7 组)/ `test/CMakeLists.txt`(ctest 注册)

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
| `openspec/changes/cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md` | IC2 scenario **直接改写**为逐网络校验(spec 是机器校验面,非 ADR) | spec 是判据 C-4 的机器可断言面 |
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
`docs/adr/ADR-CEP-{08,09,12}-*.md` · `openspec/changes/cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md` ·
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

**连带变更**: `openspec/changes/cpptlm-cep-umbrella/{proposal.md,tasks.md}` 状态 🟡 Proposed Draft → 🟡 **Proposed**;
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
`openspec/changes/cpptlm-cep-umbrella/proposal.md` 前置 gate ④

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
| `openspec/changes/cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md` | `:310`「锁定表**恰好 6 个协议族**」→「**恰好 8 个**」+ 补 2 个族名 | 2026-10-22 **critic 复审发现(漏网)**:该文件在 `cep_freeze_check.sh` 的 `FROZEN_GLOBS` 内 —— 是**第 9 份**冻结产物;回执 R-13 早已要求「umbrella `spec.md` 同步」,本批初版漏做。它与 `ADR-CEP-11 §3`(增至 8 族)直接矛盾,且 freeze 校验**只比 hash 不比语义** → 无 checker 能发现 |

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
`openspec/changes/cpptlm-cep-umbrella/{proposal.md,tasks.md}` · `test/test_cep_schema.cc` ·
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
| 8 | `openspec/changes/cpptlm-cep-umbrella/tasks.md` | **S1-6 勾选**(A6 闭合)+ 新增 **S1-7**(R-2 形态变更 + β 评估) | OpenSpec change |
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

## §12 Review Schedule

- **2027-01-18**(review_by 默认 = created + 90 天):第一次 sprint-review
- 后续按 90 天周期 + Stage 晋级节点追加

**review_by 触发仪式**(per `rdd-planner/SKILL.md:159`):
- `bash skills/rdd-planner/scripts/planner_objective_revise.sh objective-cep-platform --kind sprint-review --content "..." --decision "..." --reason "..."`
- kind ∈ {deferral-rationale, go-decision, sprint-review, scope-change, adr-amendment}

---

**版本**: v1.1(2026-10-20,per oracle v0.4 路线图评审 B1/B2/B4/B5/B6 + metis G1/G4 修订)
**最后**: 待 rdd-planner stage exit 自动刷新 AGENTS.md `<!-- AUTO: objectives -->` 哨兵段
**作者**: CppTLM build agent(planner mode)