# cpptlm-cep-umbrella: Coherence Exploration Platform(CEP)核心契约与实验床骨架

> **状态**: 🟡 **Proposed / in-progress** —— Stage 0 umbrella(tasks **105 / 99**);**跨仓准入门(M2)已放行**(2026-10-22,per `ADR-CEP-00 §8` + 台账 Entry #9/#10);**R10 未闭合 → 不 archive**;余 6 项 = Stage 1 准入前置(归属见 `obj §4.0`)
> **作者**: CppTLM Team(builder mode)
> **版本**: v1.1(2026-10-22 · M2 放行)
> **载体定位**:本 change = **执行真源**(做什么 / 怎么验 / 做到哪);**上游规划输入** = `.rddf/improvements/cpptlm-cep-umbrella.md`;**排队与状态** = `obj-cep-platform §4.0`(队列 #1);**载体分工与状态更新协议** = `obj-cep-platform §4.7`(状态真源 = 本 change 的 `tasks.md` 勾选)
> **关联路线图**: [`phase-10`](../../../.rddf/roadmap/phases/phase-10.md)(in-progress)+ [`objective-cep-platform`](../../../.rddf/roadmap/objectives/obj-cep-platform.md)(P1 active)
> **关联真源 1**: [`docs/architecture/07-coherence-exploration-platform.md`](../../../docs/architecture/07-coherence-exploration-platform.md) v0.1(**686 行**,✅ 2026-10-21 完成)+ [`06-inter-module-network.md`](../../../docs/architecture/06-inter-module-network.md) v0.4 + [`06-bis-coherence-as-network-extension.md`](../../../docs/architecture/06-bis-coherence-as-network-extension.md) v0.3(将被 supersede)
> **关联真源 3**: `docs/adr/ADR-CEP-*.md` **共 21 份**(2026-10-21 architect 阶段两批:新增 `ADR-CEP-08/09/12/13/18` + `ADR-CEP-11/14/15/16/07A`,修订 8 份)
> **spec 元信息**(2026-10-23 自 `specs/cep-core-contracts/spec.md` 头部迁入,per 台账 Entry #27):**上游真源** = [`docs/architecture/07-coherence-exploration-platform.md`](../../../docs/architecture/07-coherence-exploration-platform.md) v0.1;**验收归属** = 本 change spec 的**契约存在性与自洽性**由 Stage 0 机器检查,**实现满足性**由 Stage 1~3 各自 change 的 AC 落地;**配套** = `specs/cep-core-contracts/spec.md` · [`design.md`](design.md) · [`tasks.md`](tasks.md)
> **KPI**: Stage 0 = **1 slot**(计 KPI,per `ADR-CEP-00 §2 Decision 3`;**不符** `cpptlm-dgpu-soc-arch-truth-cleanup §6` doc-only 豁免先例,因本 change 含测试契约骨架)
> **前置 gate**: ① 签发仓 = CppTLM(2026-10-20 拍板)✅ ② 真源路径 = `07-*.md`(已创建)✅ ③ KPI slot 腾出(3 个 active 已 ⏸ 暂存归档,baseline **0/3**)✅ ④ **跨仓准入门(M2)✅ 已放行**(2026-10-22)
>
> **✅ gate ④ 跨仓准入门(M2)—— 2026-10-22 放行结论**
>
> | 判据 | 取证 | 结论 |
> |------|------|------|
> | **C-1** | ArchForge 侧**接口规约文档已合入**:commits `6020f42`(8 文件 / +1415 行)+ `8290b4c`(ADR-SOC-25 → Accepted + N-1~N-4 裁决)+ `ac956b8`(D-9 批 1) | ✅ **已闭合**(措辞按 `ADR-CEP-00 §8` 修订为「接口**规约文档**」) |
> | **C-2** | P0-1 ~ P0-5 逐项结论:载体 = ArchForge `ADR-SOC-25` + `32 号`;我方逐项签收见 `docs/implementation/2026-10-21-cep-archforge-response-receipt.md §3` | ✅ **已满足** |
> | **C-3** | §5 的 6 问各有书面结论(ArchForge `roadmap/cep-interface-alignment.md §5` 答复表 + `32 号 §10`) | ✅ **已满足** |
> | **C-4** | schema ↔ 声明双向映射:**机器化** —— `scripts/test/cep_decl_alignment_check.sh`(依赖 `jsonschema` 做**全 13 个 required 属性**校验(刻意不降级运行),5 项子校验)+ `include/tlm/cep/{protocol_instance.schema.json,fabric.schema.json,fabric.json}`;**反向验证 12/12 全绿**;RUN 证据见台账 Entry #9 | ✅ **已满足**(声明 2/2 份校验通过) |
> | **C-5** | 反向验证 **12 条** + lint + ctest 全绿(2026-10-22 实测:`ctest` **94/94**、`ctest -R cep` **16/16**、CEP lint **9/9 组**、M1 冻结 **29/29**、锚点 **33/33**) | ✅ **已满足** |
> | 结论 | 准入门**放行** | **本 change 升 Proposed**;Proposed 并发 **1/3** |
>
> **放行依据**:ArchForge 侧 coherence 接口**规约文档**已合入 + 本仓侧判据 C-4 的**机器化**已落地
> (`ADR-CEP-12` §8 / `ADR-CEP-08` §8 / `ADR-CEP-09` §8 三个 Status Update + umbrella spec IC2 scenario 改写
> + `protocol_instance.schema.json` 采纳 per-network 嵌套形态 + `fabric.{schema.json,json}` 载体 + 第 9 组检查器)。
> → 由 DRI 人工 gate 放行并记 §11 台账 **Entry #10**。
>
> ⚠️ **Stage 1 必须承接的 4 项 provisional 债**(per 台账 Entry #9 ④):
> ① `ADR-CEP-09` 的图顶点按 `(协议实例, network, vc_id)` 建立**之前**,不得用其无环判据放行任何协议实例;
> ② N-1/N-2 的实现侧复核;③ SLICC 式「vnet + 装配期绑定」备选的重评触发条件(第 3 网 / 第 3 实例);
> ④ `fabric.*` 在首个 `implemented` 实例跑过后**再纳 M1 冻结集**。
> ⚠️ **R10 未闭合** —— 本次放行是 **AI 三方评审(见回执 §8)+ M1 冻结 + M4 复审** 的组合,**真外部真人评审仍缺**。
>
> **📋 对齐清单**:`docs/implementation/2026-10-21-cep-archforge-interface-alignment.md`
> —— 13 项 gap 矩阵(带两侧 file:line)+ ArchForge 侧 PR 范围(P0 5 项 / P1 4 项 / P2 4 项)
> + 需 DRI 澄清的 **6 问** + M2 的 **5 条可检查放行判据**。
> 清单的引用漂移由 `scripts/test/cep_archforge_alignment_check.sh` 机器检测(33 个锚点;
> ArchForge 仓不在本环境时优雅跳过);
> **声明 ↔ schema/fabric 的对齐**由 `scripts/test/cep_decl_alignment_check.sh` 机器检测(5 项子校验,反向验证 12/12)。
> **✅ 残留 gate 已解除(2026-10-21)**: `objective-cep-platform §9.2` 三项角色(验收方 / 结论消费者 / 平台维护方)已**正式指定** —— 形式 gate 解除,Stage 0 验收的"角色缺失 No-Go"条件消除
> **⚠️ 独立性未闭合(已登记 R10)**:三项角色在当前仓形态下**指向同一人**(ArchForge 与 CppTLM commit 同一作者)→ 独立性以 **M1 产物冻结 / M2 跨仓准入门 / M3 对拍基准外置 / M4 独立 AI 复审** 四条机制**部分**替代;M4 已有实证先例(2026-10-21 Oracle 独立复审抓出 9 项作者 4 轮自审全漏的问题;2026-10-22 批 1 再经 oracle + metis + explore 三方评审修订 12 处)。**真外部真人评审待建立**

---

## Why

phase-1~9 完成(2026-04 ~ 2027-02)后,CppTLM 已具备 dGPU SoC 完整链路与 PCIe EP 7 阶段微架构交付,但**一致性互联网络层面仍是"协议特化"层** —— `06 v0.4 §1.1` 的 L1-L6 仿真层层模型虽已落地 60-70%,却**不覆盖 coherence 协议**:没有协议中立的仿真骨架,跨协议 / 目录策略 / 拓扑的取舍**只能靠人脑推演**,无法出可比较数据。

三个具体缺口(per `objective-cep-platform §1.2`):

| 缺口 | 现状 | 后果 |
|------|------|------|
| **协议中立基线缺位** | `06-bis §7.3-§7.7` 的 CoherenceBridge / CoherenceProtocol / SnoopFilter 等**90% 是 markdown 草案**,无 C++ 实现 | 所有 CEP 契约都标"本 ADR 首次定义",实质 0 复用 |
| **探索平台无实验产物** | phase-9 有 D-AXI minimal / timing-mode,但无 metric / sweep / A-B 对比格式 | 交付工具 ≠ 产出洞见(per metis G1) |
| **契约不可机器检查** | 21 份 ADR 的 **66 条守门 invariant** 目前只存在于 Markdown | 契约漂移无感知;ADR 引用无锚点校验 |

**为什么现在**:**21 份** ADR 已达可签发水准(3 轮 oracle REJECT 关闭 + 2026-10-21 首次**独立** Oracle 架构复审判定"CEP 骨架成立,无需改层"),再不动手,契约与实现之间的**语义漂移**会随 Stage 1 实现加速而放大。

**为什么是 umbrella**:Stage 0 不交付任何协议实现,只交付**契约层 + 实验床骨架** —— 让后续 **10** 个 change 全部基于同一套机器可执行的契约,而不是各自解读 Markdown。

---

## What Changes

### 新增文件(Stage 0,~12 文件,~2720 行)

| 文件 | 行数 | 内容 |
|------|:----:|------|
| `test/test_cep_canonical_invariants.cc` | ~340 | **66 条实际守门 invariant(编号位 67,按 15 组分解)** + 10 条流清单契约 + capability schema 契约 + **协议泄漏负向断言**(per `ADR-CEP-14` C9)|
| `test/test_cep_dummy_signals.cc` | ~140 | `NullProtocolEngine` / `NullCollectivePlane` 的 10 流覆盖矩阵 |
| `test/test_cep_module_factory_seam.cc` | ~220 | ModuleFactory 注册契约 + 23 ABI / frozen headers **0 diff** 守门 |
| `test/test_cep_simulation_mode_lock.cc` | ~90 | `simulation_mode` 单一维度 + 4 件套契约 + `verification_mode` 契约 |
| `include/tlm/cep/protocol_instance.schema.json` | ~150 | **协议实例能力声明 schema**(per `ADR-CEP-12` 决策 1,**7 字段组 / 13 顶层属性**;2026-10-22 起 `ordering_assumption` / `vc_binding` 为 **per-network 嵌套形态**,per `ADR-CEP-12 §8`) |
| `tools/render_report.py` | ~200 | `metrics.json` + `trace.h5` → `report.html` |
| `tools/run_sweep.py` | ~150 | 维度组合 sweep → N 个 4 件套 + `sweep_summary.csv` |
| `tools/ab_compare.py` | ~150 | A/B 对比 → `delta_metrics.json` + CDF overlay |
| `scripts/test/cep_docs_sync_check.sh` | ~220 | 真源章节号机器检查(覆盖 **21 份** ADR)+ invariant 计数单一真源校验(区分「提及」与「定义」)+ P 编号口径校验 |
| `experimental/.gitkeep` + `experimental/metrics.schema.json` | ~60 | 实验目录约定 + schema |
| `include/tlm/cep/.gitkeep` | ~1 | CEP 头文件目录占位(Stage 0 仅 schema + Null 实现雏形) |
| `docs/implementation/2026-10-21-cep-stage0-contracts.md` | ~120 | 实施笔记(TDD 5-step + commits 索引) |

> **路径修正**(per oracle B7 + metis G3-1):测试文件用 **`test/test_cep_*.cc`**(仓内真实路径,`test/CMakeLists.txt` 的 `file(GLOB test_*.cc)` 平铺收编,非递归);**不**用 `tests/openspec/`(目录不存在,会逃逸构建导致 acceptance 假绿)

### 修改文件(2 个)

| 文件 | 改动 | 说明 |
|------|------|------|
| `docs/architecture/06-bis-coherence-as-network-extension.md` | 顶部加 `## Status Update` 段 | supersede by CEP(per `ADR-CEP-00 §3`);**正文不动**(per AGENTS.md「ADR 不可变 + Status Update」先例) |
| `docs/architecture/README.md` | Doc Map 行同步 | invariant 计数(66 实际 / 67 编号位)+ adapter 计数(2 来源族 / **16** 实例)—— **本次已由 architect 阶段同步,本 change 仅复核**(计数于 2026-10-22 per 台账 Entry #12 由 14 → 16) |

### 冻结面(0 修改)

- `include/abi/cpptlm_emulator.h`:**0 diff**(23 ABI,per `ADR-DGPU-08 §2.0`)
- `include/tlm/pcie/pcie_endpoint_tlm.h`:**0 diff**(`[[deprecated]]` 保留)
- `include/tlm/cache_tlm.hh`:**0 diff**(A13 议题 `ADR-CEP-15` 评估,待 architect 起草)
- `src/tlm/gpu/dgpu_board_shell.cc`:**0 diff**(CEP 是独立探索平台,不接管 dGPU 集成)
- `include/bundles/*`:**0 diff**(Stage 1 才新增)
- `include/tlm/cep/*.hh`:**Stage 0 仅 `.gitkeep`** + `NullCollectivePlane` 接口雏形

---

## Capabilities

### New Capabilities

- `cep-core-contracts`:CEP Stage 0 核心契约层 —— canonical 10 条 Coherence Interface 流清单(11 条 `CEN-Ib`)、Stub Cache 契约(7 条)、Sharer Tracker 契约(6 条)、实验输出契约(5 条 + 4 件套 + 13 metric + `verification_mode`)、集成 seam 契约(7 条)、编号契约(3 条)、Ordering Point 可迁移性契约(3 条)、多 VC 死锁自由性契约(3 条)、协议实例能力声明 schema 契约(3 条)、一致性域归属契约(3 条)

### Modified Capabilities

(无 —— `openspec/specs/` 现有 85 个 capability 中**无 coherence / cep / 协议抽象**相关项,本次全部为新建,零 delta 冲突)

---

## Impact

### 影响范围

| 受影响模块 / 文件 | 影响范围 | 兼容性策略 |
|------------------|---------|-----------|
| `test/` 平铺收编 4 个新测试 | 新增 `[cep]` / `[cep][canonical-invariant]` 等标签 | `file(GLOB test_*.cc)` 已平铺收编,`test/CMakeLists.txt` **无需改** |
| `include/tlm/cep/` | 新增 1 个目录 + schema JSON + `.gitkeep` | 纯新增,无既有文件改动 |
| `tools/` | 新增 3 个 Python 脚本 | 纯新增;不依赖 `cpptlm/` 包的私有 API(可独立运行) |
| `scripts/test/` | 新增 1 个 lint 脚本 | 纯新增;接入 pre-commit 时**须**与 `docs_sync_check.sh --strict` 协同,避免重复扫描 |
| `experimental/` | 新增目录约定 + schema | 纯新增;.gitignore 需放行 `.gitkeep` |
| 全部既有测试 | **0 回归** | 新测试用独立标签 `[cep]`,不改任何既有 case |

### 受影响测试

| 测试 | 当前状态 | 实施后 |
|------|---------|--------|
| 既有全量(`~1241 cases`) | ✅ Pass | ✅ **保持 Pass,0 回归** |
| **`[cep][canonical-invariant]`** | 0 cases | ✅ ~50 cases(10 组 × 每组 2~6 断言) |
| **`[cep][dummy-signals]`** | 0 cases | ✅ ~10 cases(10 流覆盖矩阵) |
| **`[cep][module-factory-seam]`** | 0 cases | ✅ ~8 cases(注册机制 + 冻结面 0 diff) |
| **`[cep][simulation-mode]`** | 0 cases | ✅ ~6 cases(`simulation_mode` + `verification_mode` 锁定) |
| **`[cep][schema]`** | 0 cases | ✅ ~9 cases(capability schema **7 字段组 / 13 属性** + per-network 形态 + fabric 载体) |
| **`[cep][docs-lint]`**(shell) | 0 | ✅ **21 份** ADR 锚点 + invariant 计数单一真源 |

### 风险与缓解(per `objective-cep-platform §8.1`)

| 风险 | 等级 | 缓解 |
|------|-----|------|
| **R5 无外部验收方** | 🟡 中 | `objective §9.2` 三项角色**仍未指定**;Stage 0 验收按 §8.2 判 **No-Go**。**本 change 可实施,但验收须先解除该 gate** |
| 契约测试与 ADR 漂移 | 🟡 中 | `scripts/test/cep_docs_sync_check.sh` 把 invariant 计数 + ADR 锚点做成**机器检查**,进 pre-commit |
| `verification_mode` 被误读为"已证明" | 🟡 中 | `INV-CEP-06` 契约明确禁止 bounded/sampled 结果表述为"已证明无死锁 / 无违例" |
| 单 change 体积(umbrella 含 26 文件) | 🟢 低 | Stage 0 总估时 **46 人日**(已消耗,per Entry #22 重算),tasks 按 8 个 Phase 切分,每 Phase 独立可验证 |
| schema 与 ADR 后续修订脱节 | 🟢 低 | `ADR-CEP-12` 为 schema 唯一真源;lint 脚本校验 ADR 引用存在性 |

---

## Acceptance

- [x] **TDD 5-step 全部 GREEN**(red → green → refactor → regress → doc)—— ✅ tasks.md Phase 1~8 全绿
- [x] **66 条实际守门 invariant 机器检查 100% 覆盖**(编号位 67;`INV-CEP-04-IC4` **唯一跳空**)—— 按 `objective-cep-platform §0 DoD 7` 的 **15 组**逐组断言 ✅
- [x] `[cep][canonical-invariant]` PASS —— **实测 13 case / 684 assertion**(2026-10-23;case 数少于原规划 ~50,断言数远超 —— invariant 以**断言**计,非 case 数)
- [x] `[cep][dummy-signals]` PASS(10 流覆盖矩阵全覆盖)—— **实测 6 case / 71 assertion**
- [x] `[cep][module-factory-seam]` PASS —— **实测 9 case / 36 assertion**
- [x] `[cep][simulation-mode]` PASS(含 `verification_mode` 三值)—— **实测 8 case / 68 assertion**
- [x] `[cep][schema]` PASS(capability schema **7 字段组 / 13 属性** + **per-network** `ordering_assumption` / `vc_binding` 校验:network 须登记、其下 `vc_id` 须落在槽位上界内且 ∈ 同网络 `vc_binding` 值域、消息类 ∈ 6 类词表)—— per `ADR-CEP-12 §8` 的 `INV-CEP-12-IC2` ①②③ —— **实测 18 case / 228 assertion**
- [x] `scripts/test/cep_docs_sync_check.sh` PASS(**21 份** ADR 锚点 + invariant 计数单一真源)—— **实测 rc=0(10 组全绿)**
- [x] **23 ABI 签名 0 变更**(`git diff HEAD -- include/abi/` 为空,per `ADR-DGPU-08`)—— **实测空** ✅
- [x] **4 frozen headers 0 diff**(per `ADR-DGPU-08` / `ADR-DGPU-09`)—— **实测空** ✅
- [x] 既有全量测试 **0 回归**(`ctest --test-dir build` 全绿)—— **实测 96/96** ✅
- [ ] `openspec validate cpptlm-cep-umbrella --strict` PASS —— ⚠️ **实测 rc=1**(2 条预存 WARNING:`spec.md` 顶部文本不被 archive 携带;per Entry #12 ⑦ 实证为**预存不满足**)。`spec.md` = M1 冻结面第 29 成员 → **不修**;修复属 **批 4**(暂缓,per Entry #22)
- [x] 文档同步:`docs/implementation/2026-10-21-cep-stage0-contracts.md` + `06-bis` Status Update 段 + `07 号` §11 Gap 映射复核 ✅
- [x] **`objective-cep-platform §9.2` 三项角色已指定**(验收方 / 结论消费者 / 平台维护方)—— ✅ **2026-10-21 已指定**(形式 gate 解除;实质独立性缺口 = R10,未解除)

### Commit 策略(TDD 5-step,预计 12~14 commits)

```
1.  test(cep): RED stub — 50 条 invariant 契约测试骨架 (Phase 1)
2.  docs(cep): 07 号真源复核 + 06-bis Status Update (Phase 2)
3.  feat(cep): capability schema JSON + INV-CEP-12-IC1~3 契约 (Phase 3)
4.  test(cep): GREEN — schema 契约断言 (Phase 3)
5.  feat(cep): NullProtocolEngine / NullCollectivePlane 覆盖矩阵 + test_cep_dummy_signals (Phase 4)
6.  test(cep): GREEN — 10 流覆盖矩阵 (Phase 4)
7.  feat(cep): ModuleFactory seam 契约 + test_cep_module_factory_seam (Phase 5)
8.  test(cep): GREEN — seam + 冻结面 0 diff 守门 (Phase 5)
9.  feat(cep): simulation_mode + verification_mode 锁 + test_cep_simulation_mode_lock (Phase 6)
10. test(cep): GREEN — 4 件套契约 (Phase 6)
11. feat(tools): render_report / run_sweep / ab_compare + experimental/ schema (Phase 7)
12. feat(ci): scripts/test/cep_docs_sync_check.sh (16 ADR 锚点 + invariant 计数) (Phase 7)
13. test(cep): GREEN — 50 条 invariant 全绿 + 全量 0 回归 (Phase 8)
14. docs(implementation): 实施笔记 + README Doc Map 复核 (Phase 8)
```

---

## 相关变更

| Change | 关系 |
|--------|------|
| `cpptlm-msix-generator-ip` ⏸ 暂存归档 | KPI 前置(已解除) |
| `cpptlm-p2-integration-unblock` ⏸ 暂存归档 | KPI 前置(已解除) |
| `cpptlm-pcie-tlp-entry-layer` ⏸ 暂存归档 | KPI 前置(已解除) |
| `cpptlm-cep-stub-cache-core`(Stage 1.1) | **后续** —— 本 change 提供其依赖的契约层 |
| `cpptlm-cep-directory-ordering-plane`(Stage 1.2) | **后续** —— 由 `sharer-tracker-base` 扩容而来(per `objective §4.2`),承载 `ADR-CEP-08` / `ADR-CEP-09 Decision A` 的实现 |

---

## OpenSpec KPI 状态

- **实施前**: baseline **0/3**(3 个 active 已 ⏸ 暂存归档)
- **本 change 期间**:**1/3**
- **本 change archive 后**:**0/3**(归位)

---

**Owner**: CppTLM Team
**最后更新**: 2026-10-21(v1.0 起草,planner 阶段 P1 三项完成后交 builder)
