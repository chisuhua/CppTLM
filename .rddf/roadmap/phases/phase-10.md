---
id: phase-10
kind: phase
status: in-progress
phase_refs:
  - phase-7
  - phase-8
  - phase-9
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
---

## phase-10: Coherence Exploration Platform(CEP)

> **真源 1 (基础)**: [`docs/architecture/06-inter-module-network.md`](../../../docs/architecture/06-inter-module-network.md) v0.4(仿真层 L1-L6 正交,非 CEP 6 层)
> **真源 1 (基础)**: [`docs/architecture/06-bis-coherence-as-network-extension.md`](../../../docs/architecture/06-bis-coherence-as-network-extension.md) v0.3(将被 supersede by CEP,顶部加 Status Update 段)
> **真源 1 (新,✅ 已拍板 α + 已创建)**: [`docs/architecture/07-coherence-exploration-platform.md`](../../../docs/architecture/07-coherence-exploration-platform.md) v0.1(**686 行**,2026-10-20~21;后增补)
> **真源 3**: **21 份** CEP ADR(per `ADR-CEP-NUMBER-MAP §2 Decision 1`;2026-10-21 architect 阶段两批新增 10 份):
> - 治理:`ADR-CEP-00-platform-charter-and-governance.md` v0.6
> - 真源映射:`ADR-CEP-00A-real-source-anchor-map.md` v0.6
> - 架构基座:6 层 + 旁路平面 + canonical 10 流 + 内部接口 + Stub Cache + 实验 + 仿真 seam + 编号预分配 = `ADR-CEP-01~07` + `NUMBER-MAP`
> - **排序与组合契约(2026-10-21 第一批)**:`ADR-CEP-08` Ordering Point 可迁移性 / `ADR-CEP-09` 多 VC 仲裁 + 死锁自由性论证 / `ADR-CEP-12` 能力映射表 schema / `ADR-CEP-13` 一致性域归属
> - **协议族与路线**:`ADR-CEP-17` 协议族接入 + 组合承诺边界 / `ADR-CEP-18` 协议生成路线范围
> - **对齐与治理(2026-10-21 第二批)**:`ADR-CEP-11` 协议版本锁定 / `ADR-CEP-14` Stub 合同测试 / `ADR-CEP-15` CacheTLM 对齐 / `ADR-CEP-16` `06 §15.4` 演进映射 / `ADR-CEP-07A` 06-bis 产物处置

### 当前活跃 (Active)— Stage 0 Umbrella(队列 #1)

> **排期真源指针**:本 phase 的**实施路径**(14 个 change 的顺序 / 季度 / 状态 / 准入前置归属)真源 = [`obj-cep-platform §4.0 排队总表`](../roadmap/objectives/obj-cep-platform.md);**治理口径**(change 总数 / 串行 / 并发 ≤ 1)真源 = `ADR-CEP-00 §2 Decision 3`。
> **载体分工 / 状态怎么更新** = [`obj-cep-platform §4.7`](../roadmap/objectives/obj-cep-platform.md)(**本节只是 Stage 0 子任务展开**,不承载整条队列的状态)。

| 子任务 | 状态 | 范围 | 估时 | 关联 ADR | KPI slot |
|--------|------|------|------|----------|----------|
| **10.A 真源 ADR 起草与评审** | ✅ 完成(2026-10-19~21) | **22 份 ADR**(原 10 份 + 2026-10-21 两批 + **2026-10-27 ADR-CEP-19 签发**)新增 08/09/12/13/18 + 11/14/15/16/07A,修订 02/04/05/06/07/17) | 12 人日(architect,已消耗) | ADR-CEP-00~18 全族 + NUM | 0(doc-only 起草)|
| **10.B OpenSpec `cpptlm-cep-umbrella` 起草** | ✅ **完成 + 已归档(2026-10-23)** —— 4 件套已落;tasks **99 / 99** 全勾;`openspec validate cep-core-contracts --strict` **rc=0**(2 条预存 WARNING 已随批 4 消除,per Entry #27;**裸 `openspec validate --strict` 恒 rc=1**,不可作验收证据);**R10 已闭合**(按 `obj §9.3` 判据,per Entry #28);**完成归档**(delta spec 已合并进 `openspec/specs/cep-core-contracts/spec.md`,per Entry #29) | proposal + design + specs + tasks **4 件套已落 + 已归档** | 5 人日(builder,已消耗) | improvement `cpptlm-cep-umbrella.md` | **1**(Stage 0 计 KPI,per oracle v0.4 B2-1 修订)|
| **10.C 实验输出 4 件套契约测试** | ✅ **完成(2026-10-21~22)** | `test_cep_canonical_invariants.cc` + `test_cep_dummy_signals.cc` + `test_cep_module_factory_seam.cc` + `test_cep_simulation_mode_lock.cc`(+ 后期补充 `test_cep_schema.cc` / `test_cep_contracts_crosscheck.cc` / `test_cep_tools_contract.cc`) | 10 人日(builder,已消耗) | ADR-CEP-06 §2 + ADR-CEP-07 §2 | 0(沿用 10.B)|
| **10.D `experimental/` 目录 + tools** | ✅ **完成(2026-10-22)** | `tools/render_report.py` + `run_sweep.py` + `ab_compare.py` + `experimental/metrics.schema.json` | 5 人日(builder,已消耗) | ADR-CEP-06 §4 | 0(沿用 10.B)|
| **10.E `07-coherence-exploration-platform.md` 顶层真源** | ✅ **完成(2026-10-20~21;后增补至 686 行)** | 整合 **21 份** ADR 为可读真源 1(**686 行**) | 5 人日(architect,已消耗) | ADR-CEP-00 §5.2 | 0 |
| **10.F `scripts/test/cep_docs_sync_check.sh` ADR 锚点 lint** | ✅ **完成(2026-10-22;后扩至 10 组)** | 真源章节号机器检查(覆盖 **21 份** ADR;第 10 组 = 传输层 `CATEGORY_*` 分层纪律,per Entry #11) | 6 人日(已消耗) | ADR-CEP-00A §5.1 | 0 |
| **10.G capability schema 定义 + 契约** | ✅ **完成(2026-10-22)** | `include/tlm/cep/protocol_instance.schema.json` + `INV-CEP-12-IC1~3` 断言(+ `fabric.schema.json` / `fabric.json`) | 3 人日(已消耗) | ADR-CEP-12 §2 + §3 | 0(沿用 10.B)|

**Stage 0 估时(2026-10-23 重算,per Entry #22)**:上表逐行合计 = **46 人日**(10.A 12 + 10.B 5 + 10.C 10 + 10.D 5 + 10.E 5 + 10.F 6 + 10.G 3)。
> **口径修正**:原「41 人日 / 已消耗 13 / 剩余 28」与表内逐行值不同步(10.A 由 8 → 12 人日后未回填算式)→ 以表内合计为准。

- **实施侧**:10.A ~ 10.G **全部已落地**(tasks **99 / 99 全勾**)→ **Stage 0 工程量视为已消耗**。
- **剩余工程量**:**0**。umbrella `tasks.md` 的 6 项 Stage 1 准入前置(S1-1 ~ S1-5 + S1-7)已**移出该 change 的任务表**(条目与判据保留)→ 按 `obj-cep-platform §4.0` 排队表归属 Stage 1(队列 #2 / #4 / #6 / #8,per Entry #71 重排 —— **#7 `chi-rnf-subset` 不持有任何 S1-x**,其前置已全部移至 #6),**不计入 Stage 0**。
- **卡口已闭合(2026-10-23)**:R10 已按 `obj §9.3` 闭合判据处置(残余风险已登记 + DRI 书面接受,per 台账 Entry #28);Stage 0 **已完成归档**(per Entry #29)。

**KPI 预算**(per `ADR-CEP-00 §2 Decision 3` + oracle v0.4 B8 三处口径对齐):

- Stage 0 = 1 slot(`cpptlm-cep-umbrella`,计 KPI,**不符 doc-only 豁免先例**)
- Stage 1 = **10** 串行 × 1 slot = **10** 季度(含 `cpptlm-cep-poc-spike`,**2026-10-22 P-1 增补**;**2026-10-26 Entry #71 增补 `moesi_gpu` / `mesif`**;**2026-10-27 Entry #74 增补 `cohif-carrier`**)
- Stage 2 = 2 串行 × 1 slot = 2 季度
- Stage 3 = 1 slot = 1 季度
- **总计**:**14** 个 OpenSpec change,串行 **14** 季度,任一时刻 Proposed 并发 ≤ 1(真源 = `ADR-CEP-00 §2 Decision 3`;**终态季度见 `obj-cep-platform §4.6`** —— per Entry #23 单源纪律,本文件不复制数值)

### 子任务依赖图(Stage 0)

```
10.A 真源 ADR 起草 ✅(21 份)──→ 10.B OpenSpec umbrella 起草(同步)
                                    ↓
        ┌───────────────────────────┼───────────────────────────┐
        ↓                           ↓                           ↓
10.C 契约测试                    10.D tools + experimental    10.E 7 文档 ✅
         │                           │                           │
         └─────── 10.G capability schema 契约 ────┘
        ↓                           ↓                           ↓
        └─────────────────────── 10.F 注入 lint ─────────────┘
                                    ↓
                          (Stage 0 验收 → Stage 1 启动)
```

### 子任务依赖图(Stage 1~3 远期)

> ⚠️ **本图是示意,不承载排期承诺** —— 权威排期见 `obj-cep-platform §4.0 排队总表`(1 change / 1 季度;**串行规则与季度跨度见 `obj §4.6`**)。

```
Stage 0 完成 ──→ Stage 1 (10 change 串行 10 季度,**KPI 总数 = 14**,2026-10-22 P-1 增补;2026-10-26 Entry #71 增补 moesi_gpu / mesif;2026-10-27 Entry #74 增补 cohif-carrier):
                 ├─ cpptlm-cep-poc-spike              ← 2026-10-22 新增,置首位
                 │    (可丢弃 spike:解阻 A6 → 解阻全部协议插件 change)
                 │    ✅ **已完成归档(2026-10-25)** —— 完成归档语义;delta spec
                 │    已合并进 main specs(15 Scenario);critic 7 轮复审 → APPROVE
                 │    (per Entry #55)
                 │    承接 Stage 1 准入前置:S1-1 ~ S1-5(2026-10-24 planner 审查
                 │    逐项分层:定义层已消费 / 建图层+放行闸→队列 #4 /
                 │    S1-2 不触发 / S1-3 待命 / S1-4 前提已破 / S1-5 待 adapter
                 │    —— 详见 obj §4.0 队列 #2 行 + Entry #46)
                 ├─ cpptlm-cep-stub-cache-core
                 │    ✅ **archived(2026-10-25)** —— 完成归档语义;tasks 36/36 全勾;
                 │    2 个 delta capability(cep-vc-mapping-contract / cep-stub-cache-core)
                 │    已合并进 openspec/specs/;Phase 0 = M-4 决策落盘 + 契约化 /
                 │    Phase 1 = stub cache core 实现(6 接口 / 5 态 / INV-SC1~6)
                 │    承接 Stage 1 准入前置:M-4 行为侧决策 ✅ 已消费
                 │    —— 详见 obj §4.0 队列 #3 行 + Entry #58/#59/#60/#61)
                 ├─ cpptlm-cep-directory-ordering-plane   ← 2026-10-21 重排
                 │    ✅ **archived(2026-10-25)** —— 完成归档语义;tasks 53/53 全勾;
                 │    22 Scenario 并入 3 份 main spec(cep-sharer-tracker-base 9 /
                 │    cep-ordering-point-checker 6 / cep-channel-dependency-gate 7)
                 │    交付:SharerTrackerIf 单接口 + caps + D-5 + FullMapDirectory 唯一实现 ·
                 │    AddressResolverIf + J1/J2/J3 + INV-CEP-08-IC2 + S1-7 (β) 裁决 ·
                 │    装配期通道依赖图(逐 (network, vc) verdict,双挂共享判据)
                 │    承接 Stage 1 准入前置:S1-1 ②建图层 ✅ + S1-7 (β) ✅ + M-4 行为侧 ✅
                 │    + S1-2/S1-3 ⛔ 未触发 / S1-4 ✅;余项登记已重指向队列 #5
                 │    —— 详见 obj §4.0 队列 #4 行 + Entry #62/#63/#64)
                 ├─ cpptlm-cep-collective-plane-base
                 │    ✅ **archived(2026-10-26)** —— 完成归档语义;21 Scenario 并入 2 份
                 │    main spec(cep-collective-plane-base 13 / cep-collective-plane-contract 8);
                 │    三段评审闭环(planner → REJECT → 三审 APPROVE_WITH_CONDITIONS)
                 │    —— 详见 obj §4.0 队列 #5 行 + Entry #66~#70
                 │    **裁决(DRI 四问,2026-10-26)**:范围切小 —— CollectivePlaneIf +
                 │    NullCollectivePlane + LocalCollectivePlane(仅 group 生命周期 /
                 │    region-epoch / BARRIER rendezvous);Bundle 类型内嵌(§5 路径现实不存在);
                 │    header-only 不进 ModuleFactory;独立声明源 collective_planes.json
                 │    **行为化既有编号**:INV-CEP-07-IC3 + INV-CEP-04-IC5(新增 0 条)
                 ├─ cpptlm-cep-moesi-gpu-adapter      ← 2026-10-26 新增(Entry #71)
                 │    ✅ **archived(2026-10-27)** —— 完成归档语义;14 Scenario 并入 2 份
                 │    main spec(cep-moesi-gpu-adapter 7 / cep-adapter-schema-contracts 7);
                 │    planner 见 Entry #71、builder 见 Entry #79、归档见 Entry #80
                 │    承接 Stage 1 准入前置:S1-7 ①②(perm 形态 schema 演进)、
                 │    S1-5(6→3 映射表机制)、S1-2(首个 implemented 实例装配后复核)
                 ├─ cpptlm-cep-cohif-carrier            ← 2026-10-27 新增(Entry #74)
                 │    🟡 **Proposed**(2026-10-27 转正,Entry #81)—— change 目录已建
                 │    (proposal + design + tasks + 2 份 delta spec;`openspec validate --strict` rc=0;
                 │    tasks **41**(已勾 35 = Phase 0 前置 + Phase 1 载体面 + Phase 2 端口面 + Phase 3 特化桥 + Phase 4 装配宏 + Phase 5 JSON 装配连通性;未勾 6 —— Phase 4.5 / Phase 5.5 是实施中发现的**阻塞前提 / 判据前提**,各登记为独立 task ⇒ 总数 39 → 41))
                 │    CohIf 载体:CohIfBundle(5 流)+ ChStream 端口面 + ProtocolBridge
                 │    (架构缺口 C1/C2/C3;**ADR-CEP-19 已签发 2026-10-27**;StubCacheCore 侧端点延期)
                 │    **Phase 0 前置已闭合**:`tlm::cep::ContractVerdict` 同名冲突消解
                 │    (commit 7f91abd9;两契约头须同 TU 共存 ⇒ 本 change 的启动前置)
                 │    **Phase 1 载体面已落地**:`bundles::CohIfBundle`(12 字段)+ 强类型
                 │    `CohIfFlowId`(域恰好 5 值)+ 12 条 `BUNDLE_FIELD_WIDTH` 锁;
                 │    4 类反向 fixture 实测有牙(2 静态扫描红 / 2 编译红)
                 │    (2026-10-27,per 台账 Entry #84)
                 │    **Phase 2 端口面已落地**:`ProtocolEngineBase<N>`(派生
                 │    `ChStreamModuleBase` + `Input/OutputStreamAdapter<CohIfBundle>`
                 │    端口成员 + 类型擦除 `ProtocolBridgeEr*` + 10 个纯虚 `on_*`)
                 │    + `NullProtocolEngine<N>`(10 流覆盖矩阵 + 2 条编译期锁);
                 │    2 类反向 fixture 实测有牙(2026-10-27,per 台账 Entry #85)
                 │    **Phase 5 JSON 装配连通性已落地**:`CohIfProtocolEngine` 可注册外壳 +
                 │    `configs/cep_cohif_pe_minimal.json`(2 PE,N=2)+ 端到端「一次 CohIf 事务
                 │    **穿过** adapter 抵达对端」+ 反向 fixture 2 类(用 `ReachOutcome` 枚举
                 │    区分装配失败/未抵达,防「因错误的原因红」);**egress 通道 trait**
                 │    (`EgressChannel::Req`)—— 实测纠偏:两种通道**都能**送达对端 ingress,
                 │    选 `req_out` 的真因是**方向语义** + **断链 fixture 有判别力**
                 │    (2026-10-28,per 台账 Entry #88)⇒ `INV-CEP-07-IC4` **已闭合**
                 │    **Phase 4 装配宏已落地**:`REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)`
                 │    (两件都做 + `static_assert(n > 1)`)+ **前置泛化**
                 │    `cpptlm::BundleDirectionTraits`(解耦 `BidirectionalPortAdapter` 的
                 │    `NoCFlitBundle` 硬编码,全量回归 0 变化);
                 │    **反向 fixture 实测有牙**(2026-10-28,per 台账 Entry #87)
                 │    **Phase 3 特化桥已落地**:`ProtocolBridgeEr` + `ProtocolBridge<InB,OutB,Ordered>`
                 │    + `AbstractProtocolBridge`(identity 基线)+ CohIf↔NoC 特化桥
                 │    (**两层** pack/unpack:内容层 + 端到端 payload 层);
                 │    `register_protocol_bridge(name, factory_lambda)` 死钩子 → 活钩子
                 │    + `get_bridges()` 转 public + `ModuleFactory` Step 7.5 消费路径;
                 │    **3 + 2 类反向 fixture 实测有牙**(2026-10-28,per 台账 Entry #86)
                 │    **NullProtocolEngine owner 已定 = 本 change task 2.5~2.7**
                 │    (`INV-CEP-07-IC2` 强制的默认实现 + 10 流覆盖矩阵 + 反向 fixture)
                 ├─ cpptlm-cep-chi-rnf-subset          ← 2026-10-27 顺延(原 #7)
                 │    范围收窄:perm 演进与 6→3 映射表机制已移至队列 #6,本 change 复用;
                 │    首次在真实载体(队列 #7)上验证装配
                 ├─ cpptlm-cep-mesif-adapter           ← 2026-10-26 新增(Entry #71)
                 │    ⚠ R-1 阻塞:依赖尚不存在的 HierarchicalDirectory 第 6 种 SharerTracker
                 ├─ cpptlm-cep-cxl-nonsymmetric
                 └─ cpptlm-cep-tilelink-tree-manager
                                    ↓
                          Stage 2 (2 change 串行 2 季度):
                          ├─ cpptlm-cep-cxl-dcoh-symmetric
                          └─ cpptlm-cep-nvlink-sharp
                                    ↓
                          Stage 3 (1 change 串行 1 季度):
                          └─ cpptlm-cep-ualink-inc
                                    ↓
                          Phase 4+ 远期(per ADR-CEP-NUMBER-MAP §2 Decision 1):
                          ├─ CEP-08/09/12/13 **已起草**(v0.1 2026-10-21),实现按上表分 Stage 落地
                          ├─ CEP-17 协议族接入 + 组合承诺边界 **已起草**
                          ├─ CEP-11 协议版本锁定 **已起草**(Stage 1 前签发)
                          ├─ CEP-14 Stub 可验证性合同测试 **已起草**
                          ├─ CEP-15 CacheTLM 对齐 **已起草**
                          ├─ CEP-16 06 §15.4 演进对应 **已起草**
                          ├─ CEP-07A 06-bis 既有产物处置 **已起草**
                          └─ CEP-18 协议生成路线范围 **已起草**(Stage 1~3 不依赖生成器)
```

### 关键依赖

- **前序依赖**: phase-7(D-AXI Driver-Visible 完成)+ phase-8(timing-mode 完成)+ phase-9(dGPU Phase 2/3 active,无依赖)
- **后续被依赖**: 无(CEP 是探索平台,不接管 dGPU 业务或 PCIe EP 业务)
- **平行 phase**: phase-9(dGPU Phase 2/3)继续 active,CEP 不抢 KPI
- **跨仓依赖**: ArchForge coherence 接口草案(per `06-bis §5.2` SnoopProbe / `coherence-protocol.md` 等)

### 治理与文档约束(per AGENTS.md §DOC HYGIENE)

- **AGENTS.md:423 边界**: 第 3 条"SoC 架构组件/拓扑/演进类提案 → ArchForge"。CEP **核心框架层**(5 层骨架 + Stub Cache + Coherence Interface + 插件机制)**归 CppTLM**;**协议语义建模**(5 个具体协议设计文档)**归 ArchForge**(per `ADR-CEP-00 §2 Decision 4` 豁免论证,边界条款对照表)
- **AGENTS.md KPI 政策**: Proposed 并发 ≤ 3(per AGENTS.md §DOC HYGIENE line 420)。**当前真实 baseline = 1/3**(2026-10-27:2026-10-27 转正:`cpptlm-cep-cohif-carrier`(队列 #7)**已建 change 目录**(proposal + design + tasks + 2 份 delta spec;`openspec validate --strict` rc=0)→ 占 1 slot,per Entry #81;历史:2026-10-27:`cpptlm-cep-moesi-gpu-adapter`(队列 #6)**已完成归档**(完成归档语义 —— 14 Scenario 并入 2 份 main spec(`cep-moesi-gpu-adapter` 7 / `cep-adapter-schema-contracts` 7))→ slot 已释放,per Entry #80;历史:2026-10-26 `cpptlm-cep-moesi-gpu-adapter`(队列 #6)**已建 change 目录** → 占 1 slot,per Entry #71;历史:2026-10-26 `cpptlm-cep-collective-plane-base`(队列 #5)**已完成归档** → slot 已释放,per Entry #70;历史:建目录时曾占 1 slot,per Entry #66;2026-10-25 `cpptlm-cep-directory-ordering-plane`(队列 #4)**已完成归档** → slot 已释放,per Entry #64;历史:建目录时曾 0/3 → 1/3;2026-10-25 `cpptlm-cep-stub-cache-core`(队列 #3)**已完成归档** → 释放 1 个 slot,1/3 → 0/3;change 目录已建,planner 阶段产出 proposal + design + 2 个 delta capability,per 台账 Entry #58;2026-10-24 `cpptlm-cep-poc-spike`(队列 #2)**已完成归档** → 释放 slot;历史:该 change 建目录后曾为 1/3,**已建 change 目录**并占用 1 个 slot;2026-10-23 `cpptlm-cep-umbrella` **已完成归档** → slot 已释放,per 台账 Entry #28/#29;**历史**:2026-10-21 创建时为 1/3、2026-10-22 M2 放行后仍为 1/3;此前 2026-10-20 的 3 个 active 全部 **⏸ 暂存归档** `--skip-specs`,0% 未实施,per AGENTS.md「两种归档语义必须区分」)。**单一时钟真源**: `ADR-CEP-00 §2 Decision 3`,本文件不重算(per oracle v0.4 路线图评审 B1 修订)。
- **ADR 不可变 doc-only 类豁免**: `cpptlm-dgpu-soc-arch-truth-cleanup §6` 显式声明,仅 doc-only cleanup 类不计。**umbrella change 含测试契约骨架,不符豁免**(per oracle v0.4 B2-1)
- **真源 1 命名空间**: `docs/architecture/` 一律真源 1,`docs/adr/` 一律真源 3,跨仓 ArchForge 文档另立

### 配套测试标签(规划)

- `[cep]` **已落地**:实测 **161 case / 2600 assertion**(2026-10-27 实测;原规划 ~30 cases)
- `[canonical-invariant]` **已落地**:**66 条实际守门 invariant 的逐条断言统一落 `test/test_cep_canonical_invariants.cc`**(编号位 67,IC4 **唯一跳空**;组 6/14/15 另有脚本层口径校验 —— **invariant 条数 ≠ case 数**)
- `[sweep]` + `[ab-compare]` **已落地**(tools 契约测试在 `test/test_cep_tools_contract.cc`)
- `[coherence]` **Stage 1 起**(尚未落地)
- 全仓 baseline:Stage 0 实施后 **1857 cases / 81202 assertions**(2026-10-23 实测)→ **1943 cases / 873046 assertions**(2026-10-27 实测;增量主因是 `[sdma]` 69 case / 803473 assertion 的环形缓冲夹具,非 CEP);Stage 1~3 继续增量
> ⚠️ `tasks.md` 第 8 项记录的 `1856 / 81145` 是**该批执行时点快照**(append-only,不改)—— 判读当前值以实测为准(per Entry #22 ⑫ 动态量快照登记)

### 真源现状(per `ADR-CEP-00A` §3.2 实证)

- **06-inter-module-network.md** v0.4 §15.4 P2.0-P5 编号口径(无 P6,per `ADR-CEP-NUMBER-MAP §2 Decision 3` 冻结)
- **06-bis-coherence-as-network-extension.md** v0.3 §7.3-§7.7 阶段范围 + §7.8 KPI 串行约束(per oracle v0.4 B1 实证)
- **01-hybrid-architecture-v2.1.md** v2.1.10 §4.7 = `simulation_mode` 真源之一(per oracle v0.4 B3-5 实证)
- **noc_bundles_tlm.hh** 字段真源:`vc_id` @ :38 / `flit_type` @ :39 / `flit_category` 注释 @ :45 / `CATEGORY_REQUEST/RESPONSE/CREDIT` @ :60-62(per oracle v0.4 B3-4 实证)

### 评审闭环

- **oracle v0.3 REJECT** → 修订 v0.4(per 2026-10-19)
- **oracle v0.4 REJECT**(8 BLOCKERS + 11 SHOULD_FIX + 10 NITS)→ 修订 v0.5(per 2026-10-20)
- **oracle v0.5 窄口 REJECT**(1 BLOCKER B7 + 6 SHOULD_FIX + 8 NITS)→ 修订 v0.6(per 2026-10-20)
- **oracle v0.5 三轮 REJECT 实证**:8/8 + 7/8 + 1/1 BLOCKER 关闭;10 份 ADR 达可签发水准(per oracle v0.5 评级)
- **oracle 全面架构复审(2026-10-21,首次独立复审)**:9 项问题(N1~N9)全部核实并修复 + 5 项架构级修订(CEN-Ib11 / `INV-CEP-06-IC5` / schema 补 5 维度 / `ADR-CEP-09` 两段式 / §7.2.1 验证深度契约);**CEP 骨架判定成立,无需改层**
- **architect 阶段(2026-10-21)**:两批新增 `ADR-CEP-08/09/12/13/18` + `ADR-CEP-11/14/15/16/07A` 共 **10 份** + 修订 8 份 → **21 份**;invariant 编号位 36 → **67**(实际 66);planner 阶段同步 Stage 重排 / DoD 7 分解 / change 清单

### 签发前置决策(per `ADR-CEP-00 §5.1 + §5.2` —— **两项均已拍板 2026-10-20**)

1. **签发仓**:✅ **已拍板 = CppTLM 仓**(2026-10-20)。定性:"支持做互相协议探索的"归入框架级,类比 gem5 Ruby network(per `ADR-CEP-00 §5.1` gem5 Ruby 类比锚点)。**21 份 ADR** + 07 号真源 1 全留 CppTLM,引用不断裂
2. **真源文档路径**:✅ **已拍板 α** = `docs/architecture/07-coherence-exploration-platform.md`(2026-10-20 已创建 v0.1 草案,已注册 README Doc Map)

---

**版本**: v0.3(2026-10-27,per 路线图漂移回填 —— 队列 #6 归档标记 + 队列 #7 转正标记 + 测试标签实测刷新;**不改排期**,排期真源仍 = `obj §4.0`)
**最后**: Stage 0 实施完成并**已归档**(2026-10-23,tasks 99/99)· R10 已按 `obj §9.3` 闭合判据处置(残余风险已登记)· **Stage 1 进行中** → 队列 #2~#6 已归档(至 `cpptlm-cep-moesi-gpu-adapter`,2026-10-27)· 当前 active = 队列 #7 `cpptlm-cep-cohif-carrier`(🟡 Proposed,2026-10-27 转正)
**作者**: CppTLM build agent(planner mode)