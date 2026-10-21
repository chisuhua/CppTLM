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
| 1 | `cpptlm-cep-stub-cache-core` | 2026-Q4 | ADR-CEP-05 + ADR-CEP-04 stub | 8 周 |
| 2 | **`cpptlm-cep-directory-ordering-plane`** ← 2026-10-21 重排(原 `sharer-tracker-base` 扩容) | 2026-Q4 ~ 2027-Q1 | **ADR-CEP-04**(Sharer Tracker 5 实现 + caps)+ **ADR-CEP-08**(Address Resolver + Ordering Point 三判据)+ **ADR-CEP-09 Decision A**(装配期通道依赖图无环检查) | **12 周**(原 8 周) |
| 3 | `cpptlm-cep-collective-plane-base` | 2027-Q1 | ADR-CEP-03 + ADR-CEP-01 | 8 周 |
| 4 | `cpptlm-cep-chi-rnf-subset` | 2027-Q1 ~ 2027-Q2 | ADR-CEP-01 + ADR-CEP-02 + **ADR-CEP-12**(首个实例声明:填 `ordering_assumption` / `vc_binding` / `consistency_labels` / `sharer_tracker`) | 8 周 |
| 5 | `cpptlm-cep-cxl-nonsymmetric` | 2027-Q2 | ADR-CEP-04 + ADR-CEP-03 | 8 周 |
| 6 | `cpptlm-cep-tilelink-tree-manager` | 2027-Q2 ~ 2027-Q3 | ADR-CEP-04 + ADR-CEP-08 + ADR-CEP-12 | 8 周 |

**Stage 1 跨度**:2026-Q4 ~ 2027-Q3(6 个 change 串行)

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

→ **当前 Proposed 并发 = 1/3**(2026-10-21:builder 创建 `cpptlm-cep-umbrella`,`openspec validate --strict` PASS)

### 5.3 CEP 启动的前置条件(per AGENTS.md §DOC HYGIENE line 422 新提案门槛)

> **创建 `cpptlm-cep-umbrella` 前须先 archive ≥1 个既有 active change**,把 baseline 降到 2/3,创建后回到 3/3。

**✅ 前置条件已满足(2026-10-20)**:3 个 active 全部 **⏸ 暂存归档**(`--skip-specs`),baseline **0/3** → `cpptlm-cep-umbrella` **可立即创建**。

**✅ 已创建(2026-10-21)**:builder 起草完成 —— proposal + design + specs(`cep-core-contracts`)+ tasks **4 件套全落**,`openspec validate cpptlm-cep-umbrella --strict` **PASS**;baseline **0/3 → 1/3**。

⚠️ **原候选路径已过期**:"路径 C: archive `cpptlm-gmmu-pte-bus-correction`" —— 该 change **已于 2026-10-12 实际归档**,不再是 active,不能再作为 archive 对象。真实候选是上列 3 个(均已暂存归档)。

**决策须登记 §11 台账**(`kind: go-decision`)。

### 5.4 串行约束

- 任一时刻 **Proposed 并发 ≤ 1**(CEP 系列内部)
- 全仓 Proposed 并发 ≤ 3(AGENTS.md 硬上限)
- 10 个 change 串行 10 季度(per §5.1 真源)

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
| **R10** | **验收独立性实质缺口**(2026-10-21 新增)—— §9.2 三角色均指向单一作者;M1~M4 只是**部分**替代 | 🟡 中 | 纳入 Stage 0 验收清单的 M4(独立 AI 复审);真外部真人评审待建立;**不在本轮闭合** |
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
- 全部 10 个 change archived with sync
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
| **M2** | **跨仓准入门(既有)** | ArchForge 侧 coherence 接口 PR **未合入** → CppTLM change **不升 Proposed** —— 制度化的跨仓 checkpoint | `ADR-CEP-00 §2 Decision 3`(已有,本轮确认其**就是**验收方的独立性依据) |
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