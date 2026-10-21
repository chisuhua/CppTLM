# Roadmap

> **最后更新**: 2026-10-20 (v1.2-planner 增量, **objective `obj-cep-platform` 新建**;per rdd-planner SKILL.md:150 跨 sprint 复杂目标触发)
> **真源原则**: 本文件代表项目长期路线图 (rdd-workflow 主文档), **真源 1 (docs/architecture/)** 与 **真源 2 (docs/designs/)** 是技术真源, 本文件是协调真源
> **OpenSpec KPI 政策**: per AGENTS.md §DOC HYGIENE line 420, **当前真实 baseline = 1/3**(2026-10-21:builder 创建 `cpptlm-cep-umbrella` Stage 0 umbrella,`openspec validate --strict` PASS)。此前 2026-10-20 的 3 个 active 已全部 **⏸ 暂存归档** `--skip-specs`(实现均 0%,保留供以后重建,per AGENTS.md「两种归档语义必须区分」)。**单一时钟真源**: `ADR-CEP-00 §2 Decision 3`(per oracle v0.4 路线图评审 B1 修订)
> **rdd-planner owns**: `.rddf/roadmap/{objectives,features,phases}/*.md` + `.rddf/improvements/*.md`(唯一写入方, per ADR-0028 + ADR-0054 D5)

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
| **phase-10** | **Coherence Exploration Platform (CEP) — 协议中立一致性互联仿真平台** | **planning** | **2026-10** | **—** |

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
- `phase-10` — **Coherence Exploration Platform (CEP) — 协议中立一致性互联仿真平台** · planning

### Features

> **当前无跨 phase feature fragment** (`.rddf/roadmap/features/` 不存在). 跨 phase 改进通过 OpenSpec change + 调研附属 (`docs/research/`) 跟踪. **建特征触发条件**: phase-11 注册且 ≥2 CEP changes 跨 phase-10/11 分布时, 走 `rddf roadmap add-feature cep --phase-refs phase-10,phase-11` 评估(per oracle v0.4 路线图评审 Q1:现在建 feature 是单 phase 退化形态,零信息量)。

### Objectives

> **2026-10-20 已建**: [`objective-cep-platform`](roadmap/objectives/obj-cep-platform.md) — CEP 跨 10 季度治理叙事(2026-Q4 ~ 2029-Q1)。触发依据:`rdd-planner/SKILL.md:150` 决策表「跨 sprint 目标叙事 + 复盘台账 + deferral rationale → objective」三项全命中(per oracle v0.4 路线图评审 B5 修订:删除不存在的「预埋触发条件」表述)。§11 决策台账已建 3 条 entry(2026-10-20 go-decision / adr-amendment / scope-change)。**后续 objective 触发**: 出现第二个跨 sprint 复杂目标时按 rdd-planner SKILL.md:150 流程评估。

<!-- AUTO-SPRINT -->