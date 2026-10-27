# Roadmap

> **最后更新**: 2026-10-23 (v1.4-planner 增量, **CEP 载体分工协议落地**;per `obj-cep-platform` **Entry #25 → #26** —— obj §4.7 五类载体分工 + 状态机 + 两种归档语义 + 六步同步清单;入口见 §4.7.4)
> **真源原则**: 本文件代表项目长期路线图 (rdd-workflow 主文档), **真源 1 (docs/architecture/)** 与 **真源 2 (docs/designs/)** 是技术真源, 本文件是协调真源
> **OpenSpec KPI 政策**: per AGENTS.md §DOC HYGIENE line 420, **当前真实 baseline = 1/3**(2026-10-27:2026-10-27 转正:`cpptlm-cep-cohif-carrier`(队列 #7)**已建 change 目录**(proposal + design + tasks + 2 份 delta spec;`openspec validate --strict` rc=0)→ 占 1 slot,per Entry #81;历史:2026-10-27:`cpptlm-cep-moesi-gpu-adapter`(队列 #6)**已完成归档**(完成归档语义 —— 14 Scenario 并入 2 份 main spec(`cep-moesi-gpu-adapter` 7 / `cep-adapter-schema-contracts` 7))→ slot 已释放,per Entry #80;历史:2026-10-26:`cpptlm-cep-moesi-gpu-adapter`(队列 #6)**已建 change 目录** → 占 1 slot,per Entry #71;历史:2026-10-26:`cpptlm-cep-collective-plane-base`(队列 #5)**已完成归档** → slot 已释放,per Entry #70;历史:建目录时曾占 1 slot,per Entry #66;2026-10-25:`cpptlm-cep-directory-ordering-plane`(队列 #4)**已完成归档** → slot 已释放,per Entry #64;历史:建目录时曾占用 1 slot;2026-10-25 `cpptlm-cep-stub-cache-core`(队列 #3)**已完成归档** → 释放 1 slot;2026-10-24:`cpptlm-cep-poc-spike`(队列 #2)**已完成归档** → 释放 slot;该 change 建目录后曾为 1/3,已建 change 目录**并占用 1 个 slot;2026-10-23:`cpptlm-cep-umbrella` Stage 0 umbrella **已完成归档**,释放 1 个 slot,per `obj-cep-platform` Entry #28/#29;`openspec validate cep-core-contracts --strict` **rc=0**,2 条预存 WARNING 已随批 4 消除,per Entry #27;裸 `openspec validate --strict` 恒 rc=1,不可作验收证据)。此前 2026-10-20 的 3 个 active 已全部 **⏸ 暂存归档** `--skip-specs`(实现均 0%,保留供以后重建,per AGENTS.md「两种归档语义必须区分」)。**单一时钟真源**: `ADR-CEP-00 §2 Decision 3`(per oracle v0.4 路线图评审 B1 修订)
> **rdd-planner owns**: `.rddf/roadmap/{objectives,features,phases}/*.md` + `.rddf/improvements/*.md`(唯一写入方, per ADR-0028 + ADR-0054 D5)
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

> **当前无跨 phase feature fragment** (`.rddf/roadmap/features/` 不存在). 跨 phase 改进通过 OpenSpec change + 调研附属 (`docs/research/`) 跟踪. **建特征触发条件**: phase-11 注册且 ≥2 CEP changes 跨 phase-10/11 分布时, 走 `rddf roadmap add-feature cep --phase-refs phase-10,phase-11` 评估(per oracle v0.4 路线图评审 Q1:现在建 feature 是单 phase 退化形态,零信息量)。

### Objectives

> **2026-10-26 更新**: 排队扩至 **13** change / **13** 季度(`moesi_gpu` / `mesif` 排入 Stage 1,per 台账 Entry #71)—— 季度跨度**见 `obj §4.6`**。
> **2026-10-27 更新**: 排队扩至 **14** change / **14** 季度(新增队列 #7 `cpptlm-cep-cohif-carrier` —— CohIf 载体,per 台账 Entry #74)—— 季度跨度**见 `obj §4.6`**。
> **2026-10-20 已建**: [`objective-cep-platform`](roadmap/objectives/obj-cep-platform.md) — CEP 跨 **14** 季度治理叙事(季度跨度**见 `obj §4.6`**)。触发依据:`rdd-planner/SKILL.md:150` 决策表「跨 sprint 目标叙事 + 复盘台账 + deferral rationale → objective」三项全命中(per oracle v0.4 路线图评审 B5 修订:删除不存在的「预埋触发条件」表述)。§11 决策台账**持续追加**(条目数会增长 —— 实测 `grep -c "^### Entry #" .rddf/roadmap/objectives/obj-cep-platform.md`,**不要**引用转述值)。**2026-10-23 口径同步**(per Entry #22 → #25 → #26):Stage 0 已归档(2026-10-23,tasks 99/99,R10 已按 `obj §9.3` 处置)· Stage 1 准入前置 S1-1 ~ S1-7 在 **`obj §4.0` 排队表**可寻址 · change / 季度数统一 11 / 11 · **载体分工与状态更新协议 = `obj §4.7`**。**后续 objective 触发**: 出现第二个跨 sprint 复杂目标时按 rdd-planner SKILL.md:150 流程评估。

<!-- AUTO-SPRINT -->