# Roadmap

> **最后更新**: 2026-10-27 (v1.5-planner 增量, **队列 #6 归档 + 队列 #7 转正后的口径回填**;per `obj-cep-platform` **Entry #80 / #81** —— 队列 #6 `cpptlm-cep-moesi-gpu-adapter` 完成归档释放 KPI slot + 队列 #7 `cpptlm-cep-cohif-carrier` 转正占 1 slot;**不改排期**,排期真源 = `obj §4.0`;载体分工与状态更新协议见 `obj §4.7`,入口见 §4.7.4)
> **真源原则**: 本文件代表项目长期路线图 (rdd-workflow 主文档), **真源 1 (docs/architecture/)** 与 **真源 2 (docs/designs/)** 是技术真源, 本文件是协调真源
> **OpenSpec KPI 政策**: per AGENTS.md §DOC HYGIENE line 420, **当前真实 baseline = 1/3**（C4 `cpptlm-cep-dod-reading-consistency` 于 2026-11-13 立项 ⇒ 占 1 slot,per 台账 Entry #373;`cpptlm-cep-invariant-gate-teeth`(C3)于 2026-11-12 归档 ⇒ slot 已释放,per 台账 Entry #367;`cpptlm-cep-transport-vc-consistency`(C2)于 2026-11-12 归档 ⇒ slot 已释放,per 台账 Entry #354（`cpptlm-cep-transport-vc-consistency`(C2 传输层 VC 语义一致性判据落地)于 2026-11-12 立项 ⇒ 占 1 个 slot,per 台账 Entry #345;此前 `cpptlm-cep-bridge-reattach-guard`(R-3 装配路径关口)已归档 ⇒ slot 已释放,per 台账 Entry #320;**活口径**:逐次 KPI 变更的**归档流水**见 objective `obj-cep-platform.md` §11 台账(Entry #28 ~ #311 的 KPI 条目)⇒ **本行不堆历史快照**;⚠️ `docs_sync_check` 的KPI 断言只抓「当前/现行/真实」限定的当前态 ⇒ 历史叙述不影响校验）**单一时钟真源**: `ADR-CEP-00 §2 Decision 3`(per oracle v0.4 路线图评审 B1 修订)
> **rdd-planner owns**: `.rddf/roadmap/{objectives,features,phases}/*.md` + `.rddf/improvements/*.md`(唯一写入方, per ADR-0028 + ADR-0054 D5)
> **阶段状态真源**: 本文件的 **Phase Skeleton** 是阶段级状态的唯一真源。`.rddf/state/roadmap-state.json`(rdd-workflow 遗留,2026-08-25 初始化)已于 2026-10-27 **删除** —— 实测全仓**无任何脚本 / 代码读取**它,且其内容(`current_phase=phase-1` / `phase-1=in_progress`)与本文件直接矛盾(phase-1 已 completed、phase-9 active、phase-10 in-progress);保留即制造**第二份阶段状态真源**,违 `obj §4.7.1` 载体分工(一个状态只写一处)。
> **CEP 载体分工 / 状态更新协议**: 见 `obj-cep-platform §4.7`(objective / phase / improvement / OpenSpec change / 实施笔记 五类载体 + 状态机 + 六步同步清单);**实施路径排队真源** = 同文件 §4.0

## Phase Skeleton

| Phase | Theme | Status | Started | Done |
|-------|-------|--------|---------|------|
| phase-1 | 通用基础架构 (事务/错误/复位/拓扑/metrics) v1.2 治理完成 | completed | 2026-04 | 2026-10 |
| phase-2 | GPGPU 多层 SimModule + ApuSoC 顶层容器 | completed | 2026-06 | 2026-08 |
| phase-3 | PCIe EP 链路层 + 编码 + PHY (Phase 1-3 微架构) | completed | 2026-03 | 2026-09 |
| phase-4 | PCIe EP SR-IOV VF Pool (Phase 4 微架构) | completed | 2026-08 | 2026-10 |
| phase-5 | PCIe EP AXI Stream Adapter + AXI4Mapper (Phase 5-6 微架构) | completed | 2026-09 | 2026-10 |
| phase-6 | PCIe EP Host Bypass + RC + 整合交付 (Phase 7-8 微架构) | completed | 2026-10 | 2027-02 |
| phase-7 | D-AXI Driver-Visible Minimal SoC v1.4 | completed | 2026-09 | 2026-09 |
| phase-8 | timing-mode SoC v0.5 (per ADR-DGPU-11) | completed | 2027-02 | 2027-02 |
| phase-9 | dGPU Phase 2/3: ControlHub + 06 v0.4 + 06-bis + 5 active changes | active | 2026-10 | — |
| **phase-10** | **Coherence Exploration Platform (CEP) — 协议中立一致性互联仿真平台** | **in-progress** | **2026-10** | **—** |

<!-- AUTO-INDEX -->

### Phases

- `phase-1` — 通用基础架构 (事务/错误/复位/拓扑/metrics) v1.2 治理完成 · completed
- `phase-2` — GPGPU 多层 SimModule + ApuSoC 顶层容器 · completed
- `phase-3` — PCIe EP 链路层 + 编码 + PHY (Phase 1-3 微架构) · completed
- `phase-4` — PCIe EP SR-IOV VF Pool (Phase 4 微架构) · completed
- `phase-5` — PCIe EP AXI Stream Adapter + AXI4Mapper (Phase 5-6 微架构) · completed
- `phase-6` — PCIe EP Host Bypass + RC + 整合交付 (Phase 7-8 微架构) · completed
- `phase-7` — D-AXI Driver-Visible Minimal SoC v1.4 · completed
- `phase-8` — timing-mode SoC v0.5 (per ADR-DGPU-11) · completed
- `phase-9` — dGPU Phase 2/3: ControlHub + 06 v0.4 + 06-bis + 5 active changes · active
- `phase-10` — **Coherence Exploration Platform (CEP) — 协议中立一致性互联仿真平台** · in-progress

### Features

> **当前无跨 phase feature fragment** (`.rddf/roadmap/features/` 不存在). **CEP 下一阶段任务**不在此处 —— 见 [`objective-cep-platform` §13](roadmap/objectives/obj-cep-platform.md)(4 个候选 change · 串行执行序 W0~W4,per 台账 **Entry #340**). 跨 phase 改进通过 OpenSpec change + 调研附属 (`docs/research/`) 跟踪. **建特征触发条件**: phase-11 注册且 ≥2 CEP changes 跨 phase-10/11 分布时, 走 `rddf roadmap add-feature cep --phase-refs phase-10,phase-11` 评估(per oracle v0.4 路线图评审 Q1:现在建 feature 是单 phase 退化形态,零信息量)。

### Objectives

> **2026-10-26 更新**: 排队扩至 **13** change / **13** 季度(`moesi_gpu` / `mesif` 排入 Stage 1,per 台账 Entry #71)—— 季度跨度**见 `obj §4.6`**。
> **2026-10-27 更新**: 排队扩至 **14** change / **14** 季度(新增队列 #7 `cpptlm-cep-cohif-carrier` —— CohIf 载体,per 台账 Entry #74)—— 季度跨度**见 `obj §4.6`**。
> **2026-11-11 更新(治理收口)**: CEP 架构文档治理批 1~6 + 收口批已完成 —— 07 号 **2763 → 648 行**,15 则 Status Update **全为归宿指针**;**决策**在 `docs/adr/ADR-CEP-*.md` · **蓝图**在 `docs/architecture/`(1 主纲 + 8 子文档)· **实施状态**在本台账;**下一步 = §13 的 4 个候选 change**(⛔ 不是继续治理文档);梳理报告见 `.rddf/reviews/cep-governance-goal-scope-review.md`。
> **2026-11-11 更新**: CEP 架构文档已**按层拆分**为 **1 主纲 + 8 子文档**(主纲唯一入口 = `docs/architecture/07-coherence-exploration-platform.md`;子文档在 `docs/architecture/cep/`)—— **决策**在 `docs/adr/ADR-CEP-*.md`、**实施状态**在本台账 `§4.0` / `§4.0-a` / `§6.2` / `§11`、**操作步骤**在 `docs/guide/cep-usage.md`(per 台账 Entry #333 ~ #339)。
> **2026-10-20 已建**: [`objective-cep-platform`](roadmap/objectives/obj-cep-platform.md) — CEP 跨 **14** 季度治理叙事(季度跨度**见 `obj §4.6`**)。触发依据:`rdd-planner/SKILL.md:150` 决策表「跨 sprint 目标叙事 + 复盘台账 + deferral rationale → objective」三项全命中(per oracle v0.4 路线图评审 B5 修订:删除不存在的「预埋触发条件」表述)。§11 决策台账**持续追加**(条目数会增长 —— 实测 `grep -c "^### Entry #" .rddf/roadmap/objectives/obj-cep-platform.md`,**不要**引用转述值)。**2026-10-23 口径同步**(per Entry #22 → #25 → #26):Stage 0 已归档(2026-10-23,tasks 99/99,R10 已按 `obj §9.3` 处置)· Stage 1 准入前置 S1-1 ~ S1-7 在 **`obj §4.0` 排队表**可寻址 · change / 季度数统一 11 / 11 · **载体分工与状态更新协议 = `obj §4.7`**。**后续 objective 触发**: 出现第二个跨 sprint 复杂目标时按 rdd-planner SKILL.md:150 流程评估。

<!-- AUTO-SPRINT -->