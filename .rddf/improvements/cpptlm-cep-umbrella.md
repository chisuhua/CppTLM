---
name: cpptlm-cep-umbrella
priority: P1
phase: phase-10
主题: Coherence Exploration Platform (CEP) — 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-00
  - ADR-CEP-00A
  - ADR-CEP-01
  - ADR-CEP-02
  - ADR-CEP-03
  - ADR-CEP-04
  - ADR-CEP-05
  - ADR-CEP-06
  - ADR-CEP-07
  - ADR-CEP-08
  - ADR-CEP-09
  - ADR-CEP-12
  - ADR-CEP-13
  - ADR-CEP-11
  - ADR-CEP-14
  - ADR-CEP-15
  - ADR-CEP-16
  - ADR-CEP-07A
  - ADR-CEP-17
  - ADR-CEP-18
  - ADR-CEP-NUMBER-MAP
状态: Proposed
created: 2026-10-20
---

# cpptlm-cep-umbrella: Coherence Exploration Platform 顶层治理与 umbrella 启动

> **状态**: ✅ **已完成并归档**(2026-10-23)—— planner 阶段 P1 三项完成(Stage 重排 + DoD 7 分解 + change 清单更新)→ builder 起草完成:`openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/` **4 件套已落并归档**,tasks **99 / 99** 全勾;`openspec validate cep-core-contracts --strict` **rc=0**(2 条预存 WARNING 已随批 4 消除,per Entry #27;归档前的 `openspec validate cpptlm-cep-umbrella --strict` 对已归档 change 返回 rc=1,属预期);KPI baseline **0/3 → 1/3 → 0/3**(归档释放 slot);**R10 已按 `obj §9.3` 闭合判据处置**(残余风险已登记 + DRI 书面接受,per 台账 Entry #28)。**2026-10-23 口径同步**:per `obj-cep-platform` Entry #22(批 1)—— 21 份 ADR / 11 change / 11 季度 / 66 实际 invariant 统一,Stage 1 准入前置 S1-1 ~ S1-7 已在 `obj §4.0` 排队表可寻址(逐项 owner → 队列 #2 / #4 / #6)
> **载体定位**:本文是 **5 段规划输入**(为什么做);**执行契约**在 `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/`(做什么/怎么验),**排队与状态**在 `obj-cep-platform §4.0`,**载体分工与状态更新协议**在 `obj-cep-platform §4.7`。
> **关联路线图**: [`phase-10`](../roadmap/phases/phase-10.md)(in-progress)+ [`objective-cep-platform`](../roadmap/objectives/obj-cep-platform.md)(P1 active)
> **关联真源 1**:
> - [`docs/architecture/06-inter-module-network.md`](../../docs/architecture/06-inter-module-network.md) v0.4
> - [`docs/architecture/06-bis-coherence-as-network-extension.md`](../../docs/architecture/06-bis-coherence-as-network-extension.md) v0.3(将被 supersede by CEP)
> **关联真源 3**: `docs/adr/ADR-CEP-*.md` **共 21 份**(2026-10-21 architect 阶段两批新增 10 份 + 修订 8 份)
> **冻结面**: 23 ABI 零 diff + 4 frozen headers 零 diff(per `ADR-DGPU-08/09`)+ 不进 `cpptlm_emulator.h` + 不改 `[[deprecated]]` 状态(per `ADR-CEP-07 §2 Decision 6`)

> **⛔ 前置 gate(per oracle B5 + metis G3-2 修订)**:
> 1. ~~`ADR-CEP-00 §5.1` 签发仓 + `§5.2` 真源路径~~ ✅ **已拍板 2026-10-20**:签发仓 = **CppTLM 仓**(框架级定位,类比 gem5 Ruby network);真源路径 = `docs/architecture/07-coherence-exploration-platform.md`(已创建 v0.1)。**原 gate 解除**
> 2. ~~**KPI slot 未腾出**~~ ✅ **已解除 2026-10-20**:3 个 active(`cpptlm-msix-generator-ip` / `cpptlm-pcie-tlp-entry-layer` / `cpptlm-p2-integration-unblock`,均 0% 未实施)全部 **⏸ 暂存归档**(`openspec archive --skip-specs`,delta spec 未合并,proposal 已加 ⏸ 标记)→ 本段登记时点(2026-10-20)baseline **0/3**,`cpptlm-cep-umbrella` **可立即创建**(per AGENTS.md「⏸ 两种归档语义必须区分」+ `ADR-CEP-00 §2 Decision 3`)
> 3. ~~**§9.2 三项角色未指定**(验收方 / 结论消费者 / 维护方)~~ ✅ **已指定 2026-10-21**(ArchForge coherence owner / phase-9 决策链 / CppTLM 维护者)—— 形式 gate 解除;**实质独立性缺口未解除**,登记为 R10(per `objective-cep-platform §8.1` / §9.2)

---

## 架构依据

| 依据 | ADR | 决策要点 |
|------|-----|---------|
| 归口 + 边界豁免 | `ADR-CEP-00` §2 | 代码全归 CppTLM;单 DRI + 准入门;AGENTS.md:423 不触发 |
| 6 层 + 完成域 | `ADR-CEP-01` §2 | Fabric Collective 旁路平面;requester-scoped vs group-scoped 判别 |
| canonical 10 流 | `ADR-CEP-02` §2 | 10 条流 + **CEN-Ib1~11**(11 条)|
| SHARP 内部接口 | `ADR-CEP-03` §2 + §3 | CollectivePlaneIf;R1-R5 防误分类;CEP-13 non-cacheable 语义 |
| D-5 重构 | `ADR-CEP-04` §2 + §3 | caps descriptor 替代 bool;LookupResult 枚举;snoop_begin/fold;INV-CEP-04-IC1~6 |
| Stub Cache 接口 | `ADR-CEP-05` §2 + §3 | RN 投影 6 接口;折叠映射表;10 条契约测试;INV-SC1~6 |
| 实验输出协议 | `ADR-CEP-06` §2 | 4 件套 + 13 必纳 metric;INV-CEP-06-IC1~4 |
| 仿真 + 集成 seam | `ADR-CEP-07` §2 + §3 | simulation_mode 单一维度;ModuleFactory 真实注册;INV-CEP-07-IC1~6 |
| 编号 + 锚点 | `ADR-CEP-NUMBER-MAP` §2 + §3 / `ADR-CEP-00A` §3 + §4 | CEP-08~18 编号分配;INV-CEP-NUM-IC1~3;真源章节行号实证 |
| Ordering Point 可迁移性 | `ADR-CEP-08` §2 | 定义 + 可迁移性三判据(J1 单点性 / J2 全序可观测性 / J3 唯一性保持);E 态静默升级必查项;INV-CEP-08-IC1~3 |
| 多 VC 仲裁 + 死锁自由 | `ADR-CEP-09` §2 | **两段式**:Decision A 通道依赖图无环判据(装配期)/ Decision B 仲裁策略;INV-CEP-09-IC1~3 |
| 能力映射表 schema | `ADR-CEP-12` §2 + §8 | 协议实例 **7 字段组 / 13 属性** + `ordering_assumption` **逐网络 × 逐 `vc_id`** + `vc_binding` **逐网络** + `sharer_tracker` + `combination_capabilities`;INV-CEP-12-IC1~3 |
| 一致性域归属 | `ADR-CEP-13` §2 | 复合一致性公理 + 域可见写 (a)(b) 判据 + 桥接并发粒度;**组合承诺边界**;INV-CEP-13-IC1~3 |
| 协议族接入 + 组合边界 | `ADR-CEP-17` §2 | Ruby-SLICC 族 + 学术协议族 + 协议私有状态边界 + **组合承诺边界**(可做但不承诺自动化) |
| 协议生成路线范围 | `ADR-CEP-18` §2 | **纯手写插件路线**;竞合定位(互补非替代);引入生成路线的 T1~T4 判据 |

---

## 范围

**In-Scope(Stage 0 umbrella)**:
- 4 个契约测试文件(守门 invariant + 10 流覆盖 + ModuleFactory seam + simulation_mode)
- **协议实例能力声明 schema**(`include/tlm/cep/protocol_instance.schema.json`,per `ADR-CEP-12`)+ `INV-CEP-12-IC1~3` 契约断言
- `experimental/` 目录约定 + `metrics.json` schema
- `tools/render_report.py` + `run_sweep.py` + `ab_compare.py` 雏形
- `scripts/test/cep_docs_sync_check.sh` ADR 锚点 lint
- `docs/architecture/07-coherence-exploration-platform.md` 顶层真源(待 §5.2 拍板)
- 06-bis supersede Status Update 段

**Out-of-Scope(Stage 0)**:
- ❌ Stub Cache 实际实现(仅接口契约 + 契约测试)→ Stage 1
- ❌ 任何 ProtocolEngine / SharerTracker 实际实现 → Stage 1
- ❌ 完整缓存核心微架构 / RTL / FPGA / UsrLinuxEmu 集成 / IOMMU 完整协议

---

## Why

phase-1~9 完成(2026-04 ~ 2027-02)后,CppTLM 已具备 dGPU SoC 完整链路与 PCIe EP 7 阶段微架构交付。但**一致性互联网络层面**(CHI / CXL / TileLink-C / NVLink / UALink 5 协议族)仍是"协议特化"层:**无协议中立的仿真骨架**,无法做跨协议 / 拓扑 / 目录策略可比性实验。

**主因(按 ROI 排序)**:

1. **协议中立基线缺位**: `06 v0.4 §1.1` L1-L6 是仿真层层模型,**与协议无关**;缺 coherence 协议抽象层(per `ADR-CEP-01 §1.1` 首次声明 CEP 6 层)
2. **复用 06-bis 范围但实质 0 复用**: 06-bis v0.3 §7.3-§7.7 范围名义继承,实质 90% 是 markdown 草案;10 份 CEP ADR 几乎所有契约都标"本 ADR 首次定义"(per oracle + metis G1)
3. **探索平台无实验产物**: phase-9 已有 D-AXI minimal / timing-mode,但无 metric / sweep / A-B 对比格式(per metis G1 盲点 + `ADR-CEP-06` 补齐)
4. **真实 IP 对应实证缺口**: NVIDIA H100 GMMU 专用 memory client / ARM CMN-700 SMMUv3 DVM 独立通道 / UALink Common 2.0 In-Network Compute 等真实 IP 实践需系统化建模(per `ADR-CEP-00 §3`)

**预期收益**(per `ADR-CEP-06` + `objective-cep-platform §9`):
- 协议中立 6 层骨架契约 + canonical 10 条流 + **67 个 invariant 编号位 / 66 条实际守门 invariant**(IC4 **唯一跳空**)
- 4 件套实验输出 + 13 必纳 metric + sweep / A-B 工具
- `NullProtocolEngine` / `NullCollectivePlane` / `StubCacheCore` 解耦测试兜底
- **8 个研究假设有可验证手段**(per `objective-cep-platform §9.1`)

---

## What Changes

### 新增文件(Stage 0,**26 文件**;实测计数 = 7 test + 3 tools + 6 scripts + 3 schema + 1 experimental + 1 真源文档 + 5 openspec change 件)

| 文件 | 行数 | 关键内容 |
|------|:----:|----------|
| `test/test_cep_canonical_invariants.cc` | ~340 | **66 条实际守门 invariant(编号位 67,按 15 组分解)** + 10 条流清单契约 + schema 契约 + **协议泄漏负向断言**(per `ADR-CEP-14` C9)|
| `test/test_cep_schema.cc` | ~700 | 后期补充:能力声明 schema 契约 + `INV-CEP-12-IC1~3` |
| `test/test_cep_contracts_crosscheck.cc` | ~350 | 后期补充:ADR 契约面交叉核对(§N / 字面量)|
| `test/test_cep_dummy_signals.cc` | ~140 | `NullProtocolEngine` / `NullCollectivePlane` 10 流覆盖 |
| `test/test_cep_module_factory_seam.cc` | ~220 | ModuleFactory 注册契约 + 23 ABI 0 diff 守门 |
| `test/test_cep_simulation_mode_lock.cc` | ~90 | `simulation_mode` 单一维度 + 4 件套契约 |
| `test/test_cep_tools_contract.cc` | ~400 | 后期补充:3 个 tools 的契约(CLI 契约 + 输出 schema)|
| `tools/render_report.py` | ~200 | `metrics.json` + `trace.h5` → `report.html` |
| `tools/run_sweep.py` | ~150 | 维度组合 sweep → N 个 4 件套 + `sweep_summary.csv` |
| `tools/ab_compare.py` | ~150 | A/B 对比 → `delta_metrics.json` + CDF overlay |
| `scripts/test/cep_docs_sync_check.sh` | ~650 | 真源章节号机器检查(覆盖 **21 份** ADR,per `ADR-CEP-00A §5.1`)+ invariant 计数单一真源校验(**区分「提及」与「定义」**)+ P 编号口径校验(per `ADR-CEP-16 IC1`)+ **第 10 组「传输层 `CATEGORY_*` 分层纪律」**(per Entry #11)|
| `scripts/test/cep_doc_ref_check.sh` | ~400 | 节号引用可达性 + 语义断言 + 负向断言(per Entry #11)|
| `scripts/test/cep_freeze_check.sh` | ~200 | M1 冻结面授权闸(`--update --authorized-by`;成员数以清单为准)|
| `scripts/test/cep_archforge_alignment_check.sh` | ~200 | 跨仓锚点守门(33 锚点)|
| `scripts/test/cep_decl_alignment_check.sh` + `cep_decl_reverse_verify.sh` | ~300 | 声明 ↔ schema 双向映射(判据 C-4,per Entry #9)|
| `experimental/.gitkeep` + `experimental/metrics.schema.json` | ~60 | 实验目录约定 + schema |
| `include/tlm/cep/protocol_instance.schema.json` | ~187 | **协议实例能力声明 schema**(per `ADR-CEP-12` 决策 1,**7 字段组 / 13 属性**;2026-10-22 起 per-network 嵌套形态)+ `INV-CEP-12-IC1~3` 契约 |
| `include/tlm/cep/fabric.schema.json` + `fabric.json` | ~51 | fabric 载体(per Entry #12)|
| `docs/architecture/07-coherence-exploration-platform.md` | **686** | CEP 顶层真源(整合 **21 份** ADR;✅ **已完成 2026-10-21**,后增补至 686 行)|
| `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/{proposal,design,specs/*,tasks}.md` | — | rdd-builder 阶段起草 |

> **路径修正**(per oracle B7 + metis G3-1):测试文件用 **`test/test_cep_*.cc`**(仓内真实路径,per `test/CMakeLists.txt` `file(GLOB test_*.cc)` 平铺收编,非递归);**不**用 `tests/openspec/`(目录不存在,会逃逸构建导致 acceptance 假绿)

### 修改文件(2 个)

| 文件 | 改动 | 说明 |
|------|------|------|
| `docs/architecture/06-bis-coherence-as-network-extension.md` | 顶部加 `## Status Update` 段 | supersede by CEP(per `ADR-CEP-00 §3`);**正文不动**(per AGENTS.md "ADR 不可变 + Status Update" 先例)|
| `test/CMakeLists.txt` | 无需改 | `file(GLOB test_*.cc)` 已平铺收编 `test/test_cep_*.cc`(per oracle B7 建议,不建子目录)|

### 冻结面(0 修改)

- `include/abi/cpptlm_emulator.h`:0 diff(23 ABI,per `ADR-DGPU-08 §2.0`)
- `include/tlm/gpu/pcie_endpoint_tlm.h`:0 diff(`[[deprecated]]` 保留)—— ⚠️ **路径修正 2026-10-23**:原写 `include/tlm/pcie/…` 在仓内**不存在**
- `include/tlm/cache_tlm.hh`:0 diff(A13 议题 CEP-15 评估,per `ADR-CEP-05 §9` 待拍板)
- `src/tlm/gpu/dgpu_board_shell.cc`:0 diff(CEP 独立探索平台,不接管 dGPU 集成)
- `include/bundles/*`:0 diff(Stage 1 才新增)
- `include/tlm/cep/*.hh`:Stage 0 仅 `.gitkeep` + `NullCollectivePlane` 接口雏形

---

## Non-Goals

- ❌ **完整缓存核心** — Stage 0 仅 Stub Cache 5 接口 + 6 不变量;不做完整 tag/data SRAM / 复杂预取器 / 精确功耗模型(per `ADR-CEP-05 §4.2`)
- ❌ **真实 RTL / FPGA 加速** — 推 ArchForge 仓(per `ADR-CEP-00 §2 Decision 5`)
- ❌ **Multi-VC NoC 全实施** — Stage 0 仅 `vc_id` 字段 0-3 沿用;多 VC 路由器仲裁推迟到 CEP-09(per `ADR-CEP-01 §3.3 抓取点 3`)
- ❌ **6 协议平行** — Stage 0~3 串行(per `ADR-CEP-00 §2 Decision 3` 11 季度)
- ❌ **完整 UsrLinuxEmu driver 集成** — 不在 CEP scope(per `ADR-CEP-07 §2 Decision 6`)
- ❌ **IOMMU 完整协议** — Stage 5 范围 reduction 待签发独立 ADR(per `ADR-CEP-00 §3` supersede 表 P5 行)

---

## Capabilities

### New Capabilities

#### 1. CoherenceInterface Contract(per `ADR-CEP-02 §2`)

canonical 10 条流 + **CEN-Ib1~11**(11 条,per `ADR-CEP-02`;`CEN-Ib11` 为 2026-10-21 架构审查新增):
```
1. request             (node → center, requester-scoped)
2. completion          (center → node, requester-scoped)
3. snoop               (center → node, requester-scoped)
4. snoop_resp          (node → center, requester-scoped)
5. writeback           (node → center, requester-scoped)
6. forward_data        (node ↔ node, requester-scoped)
7. back_invalidate     (center → node, requester-scoped)
8. address_resolve     (node ↔ center, requester-scoped)
9. barrier             (双向, requester-scoped — 不带 group_id/participant_count/epoch_id)
10. snoop_completion_ack (node → center, requester-scoped)
```

#### 2. StubCacheCore(per `ADR-CEP-05`)

6 接口命名逐字对齐 canonical 10 流:`request` / `snoop` / `snoop_resp` / `writeback` / `completion`(net-side 5)+ `evict_line`(core-side 1)。

#### 3. NullProtocolEngine + NullCollectivePlane(per `ADR-CEP-03` + `ADR-CEP-07 §2 Decision 4`)

10 流覆盖矩阵;`advance_cycle()` 空实现;解耦测试兜底。

#### 4. SharerTracker 5 实现(per `ADR-CEP-04 §2`)

`FullMapDirectory` / `SparseSnoopFilter` / `DcohSymmetric` / `TreeManager` / `BroadcastNull` + caps descriptor struct + D-5 重构。

#### 5. FabricCollective Plane(per `ADR-CEP-01` + `ADR-CEP-03`)

旁路平面 + 完成域判别规则 + `NullCollectivePlane` + 5 条防误分类规则(R1-R5)+ 跨平面契约。

#### 6. ModuleFactory 集成 seam(per `ADR-CEP-07 §2 Decision 3`)

真实注册机制:`ModuleFactory::registerObject<T>` + `REGISTER_MODULE(T)`(合法宏)+ `SimModule` 基类声明;**不发明不存在的宏形式**(per oracle v0.4 B7)。

#### 7. SimulationMode(per `ADR-CEP-07 §2 Decision 1`)

单一维度:`functional` / `timing`;真源在 `06 v0.4 §5.1` + `01-hybrid §4.7`;不引入 `network.mode`。

#### 8. Experiment Output Protocol(per `ADR-CEP-06`)

4 件套契约 + 13 必纳 metric + 4 可选 metric + 一致性 fail 闸(`deadlock_violations == 0 && memory_violations == 0`)。

#### 9. 守门 invariant 67 编号位 / 66 条实际(单一真源 per `objective-cep-platform §0 DoD 7`)

> **2026-10-23 修正**:本表原为 **6 组 / 合计 34**,系 architect 阶段**首批 10 份 ADR** 的旧分解,且合计算式本身有误(10+6+6+4+6+3 = 35 ≠ 34)→ **已失效**。
> **现行口径 = 15 组 / 66 条实际 / 67 编号位**(`INV-CEP-04-IC4` **唯一跳空**),真源 = `docs/architecture/07-coherence-exploration-platform.md §4`。
> 本表**不复述**逐组分解(避免三处复述漂移,per oracle S1 修订);**逐条断言统一落 `test/test_cep_canonical_invariants.cc`**。
>
> **下表为早期 6 组口径(合计 36 ≠ 现行 66)—— 仅作沿革留痕,不得据以计总数**;现行 15 组分解真源 = `07 号 §4`。
>
> | 早期组(仅存,已不完整) | 数量 | ID | 定义位置 |
> |------|------|-----|---------|
> | Coherence Interface | **11** | `CEN-Ib1` ~ `CEN-Ib11` | `ADR-CEP-02 §2` |
> | Stub Cache Core | 6 | `INV-SC1` ~ `INV-SC6` | `ADR-CEP-05 §4.3` |
> | Sharer Tracker | 6 | `INV-CEP-04-IC1` ~ `INV-CEP-04-IC6` | `ADR-CEP-04 §11` |
> | Experiment Protocol | 4+ | `INV-CEP-06-IC1` ~ `INV-CEP-06-IC5` | `ADR-CEP-06 §3` |
> | Simulation + Seam | 6 | `INV-CEP-07-IC1` ~ `INV-CEP-07-IC6` | `ADR-CEP-07 §3` |
> | Numbering | 3 | `INV-CEP-NUM-IC1` ~ `INV-CEP-NUM-IC3` | `ADR-CEP-NUMBER-MAP §3` |
> | **其余 9 组** | — | Ordering Point / Multi-VC / Capability Map / Coherence Domain / Version Locking / Stub Contract / CacheTLM / 06 Evolution / 06-bis Disposition | 见 `07 号 §4`(数量不在此复述)|

#### 10. KPI 平行治理(per `ADR-CEP-00 §2 Decision 3` + `ADR-CEP-07 §5`)

**11** 个 OpenSpec change 串行 **11** 季度,任一时刻 Proposed 并发 ≤ 1;Stage 0 umbrella = 1 slot 计入 KPI(per oracle v0.4 B2-1 修订:不符 `cpptlm-dgpu-soc-arch-truth-cleanup §6` doc-only 豁免先例)。change / 季度数**单一真源 = `ADR-CEP-00 §2 Decision 3`**(2026-10-22 per Entry #12 由 10 → 11)。

---

## Acceptance

> **机器验证要求**(per metis G3 / G5 修订):每条 acceptance 必须给出**可执行命令 + 期望值**,否则 verifier 只能主观判定。

### A1. 真源原则(per AGENTS.md §DOC HYGIENE)

> **2026-10-23 口径修正**:下列**朴素 grep** 会被 ADR 里的**否定语境自述**误伤(如「`ADR-I01` 不存在」「既有宏形式 `REGISTER_CHSTREAM(T)` **不存在**」)。
> **机器强制的权威口径 = `scripts/test/cep_docs_sync_check.sh`**(含否定语境判定);朴素 grep 仅作**辅助信号**,判定时须排除否定/修正语境。

- [x] `grep -rn "§7\.8 P" docs/adr/ADR-CEP-*.md | wc -l` == **0** —— ⚠️ **实测 1**(`ADR-CEP-00A:23` 的**修正说明表**,即"§7.8 实为 KPI 调整"的**修正语境**);守门脚本语境判定通过 ✅
- [x] `grep -rn "snoop_response" docs/adr/ADR-CEP-*.md | wc -l` == **0** —— **实测 0** ✅
- [x] `grep -rn "ADR-I01\|ADR-G04" docs/adr/ADR-CEP-*.md | grep -v 00A | wc -l` == **0** —— ⚠️ **实测 4**,全部为**否定语境自述**(`ADR-CEP-01:69` / `02:18` / `02:21` / `04:139` 均在说明"该编号**不存在**");守门脚本语境判定通过 ✅
- [x] `docs/architecture/06-bis-coherence-as-network-extension.md` 顶部含 `## Status Update` 段 —— **实测 1 段** ✅
- [x] `bash scripts/test/cep_docs_sync_check.sh` 退出码 == **0** —— **实测 rc=0**(10 组全绿)✅

### A2. 守门 invariant 66 条机器覆盖(编号位 67)

- [x] `./build/bin/cpptlm_tests "[cep]"` 退出码 == **0** —— **实测 rc=0(76 case / 1284 assertion)** ✅
- [x] `test/test_cep_canonical_invariants.cc` 覆盖 **66** 条实际 invariant ID(逐条断言;**编号位 67**,`INV-CEP-04-IC4` **唯一跳空** → 实际 66,per oracle 全面架构审查 N1/N2 + architect 阶段 2026-10-21 两批修订)—— **实测 70 个唯一 ID 匹配**(含 IC4 负向断言与交叉引用)✅
- [x] canonical 10 条流逐条断言(流 1~10,每流至少 1 case)✅

### A3. ModuleFactory 集成(per `ADR-CEP-07 §2 Decision 3`)

- [x] `grep -rn "REGISTER_CHSTREAM(T)\|REGISTER_OBJECT(T)" docs/adr/ src/ include/ | wc -l` == **0** —— ⚠️ **实测 1**(`ADR-CEP-07:60` 的**否定语境说明**"既有宏形式 … **不存在**");**实质成立**(0 处**肯定**用法),守门脚本语境判定通过 ✅
- [x] `grep -c "REGISTER_MODULE" include/modules_cluster.hh` >= 1 —— **实测 25** ✅
- [x] 23 ABI 0 diff:`git diff --stat -- include/abi/cpptlm_emulator.h` 输出为空 —— **实测空** ✅
- [x] 4 frozen headers 0 diff —— ⚠️ **路径修正 2026-10-23**:原写 `include/tlm/pcie/pcie_endpoint_tlm.h` 在仓内**不存在**;正确路径 = `include/tlm/gpu/pcie_endpoint_tlm.h`(`git diff --stat -- include/tlm/gpu/pcie_endpoint_tlm.h` 实测空)✅

### A4. SimulationMode 单一维度(per `ADR-CEP-07 §2 Decision 1`)

- [x] `grep -rn "network.mode" configs/*.json | wc -l` == **0** —— **实测 0** ✅
- [x] functional 模式下 `avg_latency_cycles == 0` 断言通过(per `ADR-CEP-06 §5`)✅

### A5. 实验输出契约(per `ADR-CEP-06 §2`)

- [x] `./build/bin/cpptlm_tests "[cep][simulation-mode]"` 退出码 == **0** —— **实测 rc=0** ✅
- [x] `experimental/metrics.schema.json` 存在且含 **13** 个必纳字段 —— **实测 13** ✅
- [x] `python3 tools/render_report.py --help` 退出码 == **0** —— **实测 rc=0** ✅
- [x] `python3 tools/run_sweep.py --help` 退出码 == **0** —— **实测 rc=0** ✅
- [x] `python3 tools/ab_compare.py --help` 退出码 == **0** —— **实测 rc=0** ✅

### A6. KPI 平行治理(per `ADR-CEP-00 §2 Decision 3`)

- [x] `ls openspec/changes/ | grep -v archive | wc -l` <= **3**(AGENTS.md 硬上限)—— **实测 1** ✅
- [x] `cpptlm-cep-umbrella` change 创建后,`objective-cep-platform §11` 有对应 `go-decision` entry —— **Entry #1 / #10** ✅
- [x] 归档前 KPI slot 腾出记录在 §11(per AGENTS.md line 422 新提案门槛)—— **Entry #1 / #5 / #7** ✅
- [x] AGENTS.md:423 边界不触发(per `ADR-CEP-00 §2 Decision 4` 豁免论证)✅

### A7. 前置 gate 满足(per 本文档头部 ⛔ 段)

- [x] `ADR-CEP-00 §5.1` 签发仓已拍板 — ✅ **CppTLM 仓**(2026-10-20,框架级定位,类比 gem5 Ruby network)
- [x] `ADR-CEP-00 §5.2` 真源文档路径已拍板 — ✅ **`docs/architecture/07-coherence-exploration-platform.md`**(2026-10-20 创建 v0.1)
- [x] §9.2 三项角色已指定(验收方 / 结论消费者 / 维护方)—— ✅ **2026-10-21 已指定**(形式 gate 解除;实质独立性缺口 = R10,未解除)

---

## Impact

### A. 真源原则保护(per AGENTS.md §DOC HYGIENE line 418)

- **26 个新文件**(实测:7 test + 3 tools + 6 scripts + 3 schema + 1 experimental + 1 真源文档 + 5 openspec change 件)+ 2 个修改文件
- **0 修改** frozen headers + **0 修改** 23 ABI
- **0 重发明**既有真源 1(`06` L1-L6 / `06-bis §5.2` SnoopProbe / `06` §15.4 P2.0-P5)

### B. KPI 影响

- Stage 0 **曾**占用 1 个 Proposed KPI slot(创建前 3 个 0% change 已 **⏸ 暂存归档** `--skip-specs`,baseline 3/3 → 0/3 → 创建后 1/3)—— **2026-10-23 已完成归档 → 该 slot 已释放**,本段登记时点 **0/3**
- **当前真实 baseline = 0/3**(2026-10-24:`cpptlm-cep-poc-spike`(队列 #2)**已完成归档** → 释放 slot;该 change 建目录后曾为 1/3)
- Stage 1~3 后续 **10** 个 change 串行释放 KPI 占用(任一时刻 ≤ 1)
- 总 CEP 系列占用 **11** KPI slots,跨 **11** 季度(具体季度**见 `obj-cep-platform §4.6`** —— per Entry #25 单源纪律,本文不复制数值);change / 季度数真源 = `ADR-CEP-00 §2 Decision 3`

### C. 测试标签增量

- `[cep]` **实测 76 case / 1284 assertion**;`[cep][simulation-mode]` 实测 rc=0
- 配套 baseline 增量:Stage 0 实施后全仓 **1857 cases / 81202 assertions**(2026-10-23 实测)

### D. 跨仓同步(per `ADR-CEP-00 §4`)

- ArchForge 侧:接收 CEP **21 份** ADR 的接口草案
- CppTLM 侧:实施 4 件套 + 66 条实际 invariant(编号位 67)+ 10 流覆盖矩阵
- 跨仓 PR gate:`openspec/changes/archive/2026-10-20-cpptlm-p2-integration-unblock/tasks.md:189-192` 先例 + DRI 人工 gate

---

**版本**: v1.2(2026-10-23,per planner 路线图漂移复核 + Entry #22 批 1)
**最后**: Stage 0 实施完成并**已归档**(2026-10-23,tasks 99/99)· R10 已按 `obj §9.3` 闭合判据处置(残余风险已登记)· **下一个 change = 队列 #2 `cpptlm-cep-poc-spike`**
**作者**: CppTLM build agent(planner mode)