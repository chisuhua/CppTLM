# Improvement Suggestions

> 索引由 `add-improve` 维护；每条指向 `.rddf/improvements/<name>.md`。

| Improvement | Priority | 来源 | 创建日期 |
|---|---|---|---|
| [cpptlm-cep-basis-source-kind-and-cell-free](improvements/cpptlm-cep-basis-source-kind-and-cell-free.md) | P1 | 台账 Entry #249 / #251（D-1 可机器判 + DoD 戊） | 2026-11-06 |
| [cpptlm-cep-stub-cache-chstream-surface](improvements/cpptlm-cep-stub-cache-chstream-surface.md) | P0 | ADR-CEP-19 决策 9（显式延期→实施态）/ 台账 Entry #291~#296 | 2026-11-08 |
| [cpptlm-cep-bidirectional-multiport-inbound](improvements/cpptlm-cep-bidirectional-multiport-inbound.md) | P0 | 台账 Entry #310(R-5/R-6 残余)+ #317(全量盘点 · P1 实施轨最高价值) | 2026-11-09 |
| [cpptlm-cep-gate-strength-tooling](improvements/cpptlm-cep-gate-strength-tooling.md) | P1 | 台账 Entry #340 §13.2（C1）+ Entry #341 之后立项；oracle 审查 1 轮（需改后批准,5 MUST 全落）；来源 = `docs/guide/cep-usage.md` §12.8 未结义务清单 R-a / R-e / R-f / R-g / R-h | 2026-11-12 |
| [cpptlm-cep-invariant-gate-teeth](improvements/cpptlm-cep-invariant-gate-teeth.md) | P1 | 台账 Entry #359（D-1~D-5）+ Entry #361（D-6~D-9 + C3a/C3b 切片）；来源 = architect 三次报告 §五 4 处裁决 + oracle 1 轮判定；metis 1 轮 12 条已落盘 | 2026-11-12 |
| [cpptlm-cep-dod-reading-consistency](improvements/cpptlm-cep-dod-reading-consistency.md) | P1 | 台账 Entry #340 的 C4 行（范围纪律：只做 MUST-FIX-1/2）+ Entry #332 ⑧ + Entry #359 ⑤ 四行判据；来源 = architect 二次报告 §六 Q3（锚点重定位，planner 域答复）；metis 1 轮 P0×4 / P1×4 / P2×3 全部落盘 | 2026-11-12 |

## 2026-11-20 · CEP 平台剩余任务规划批次（10 份）

> **来源**：登记册 `.rddf/roadmap/objectives/obj-cep-platform-remaining.md` §6（CEP 缺口清单 v4 + 三队列计划）。
> **审查轨迹**：metis 1 轮（gap 分析）+ oracle 1 轮 + critic 3 轮 → §6 收敛；随后 **metis 1 轮 + critic 1 轮**对用户拍板传播后的修订面复审（两轮均判 REJECT ⇒ findings 已逐条闭合）。
> **登记说明**：本节由用户 2026-11-20 放行登记（`add-improve` 索引由 skill 维护，本节为**批量补登**）。

| Improvement | Priority | 来源 | 创建日期 |
|---|---|---|---|
| [cep-d1-archive-verify-backfill](improvements/cep-d1-archive-verify-backfill.md) | **P0** | 登记册 §1.1 D-1 / §6.2 S3-5；user 拍板 Q-8（分层处置）+ Q-13（统一覆盖，闸不按 `cep` 前缀特判）⇒ **乙层 8 份追溯补验 + 新闸 `archive_verify_check.sh`** | 2026-11-20 |
| [cep-stage-gate-p5-hardening](improvements/cep-stage-gate-p5-hardening.md) | P1 | 登记册 §6.2 S3-11 / 队列 2 **I-0**；architect Q-6a 裁决（P-5 判据形态 = **措辞锚定 + 命中数计数**，⛔ 不用行号）+ Q-6b/c/d（登记面收敛 / TODO 行规则 / 闸头转指）⇒ **顺序机关落地 + P-5 判据修正** | 2026-11-20 |
| [cep-invariant-modeling-batch1](improvements/cep-invariant-modeling-batch1.md) | P1 | 登记册 §6.2 S1-1 批 1；architect Q-4（3 条 invariant 判据形态，载体 = 裁决记录 §2.4）+ M-1 + C-1 + C-2 + Q-5（13 键 golden）⇒ **3 条处置落地**（⛔ 非「3 条建模」）| 2026-11-20 |
| [cep-invariant-modeling-batch2](improvements/cep-invariant-modeling-batch2.md) | P2 | 登记册 §6.2 S1-1 批 2；architect Q-4（schema 13→14 形态）+ 队列 2 **I-6a**（litmus change）⇒ **⏳ 阻塞**：user ratify 未落 | 2026-11-20 |
| [cep-tier2-second-family-tilelink](improvements/cep-tier2-second-family-tilelink.md) | P1 | ⛔ **⏸ 已拆分 · 不可开工**(2026-11-21 architect 裁决:engine 侧观测落点路径与 TLE-05 冲突 ⇒ 目标由 **I-7a**(`TreeManager::view_of`)+ **I-7b**(`cep-tilelink-home-side-state-driver`)承接)｜原登记:登记册 §6.2 S1-2；队列 2 **I-7**；⇒ **⛔ BLOCKED**：`TreeManager` 无 `view_of`（仅 `lookup`，有副作用）⇒ 须 architect 裁 Q-a | 2026-11-20 |
| [cep-doc-hygiene-kpi-and-impl-state](improvements/cep-doc-hygiene-kpi-and-impl-state.md) | P2 | 登记册 §6.2 S2-6 / S3-6 / S3-7 / S3-8（A 类 2 项）+ 队列 2 I-1 / I-2 / I-3 | 2026-11-20 |
| [cep-registry-stale-cleanup](improvements/cep-registry-stale-cleanup.md) | P1 | 登记册 §6.2 S2-6b/d / S3-1 / S3-2 / S3-9；architect Q-7（5 处 canonical = 台账 §11）+ Q-6e（R 编号空间冻结）⇒ **登记面收敛** | 2026-11-20 |
| [cep-spec-stale-refs-r14-r15-r20](improvements/cep-spec-stale-refs-r14-r15-r20.md) | P2 | 登记册 §6.2 S3-8（B 类 **5** 项）；user Q-9（B → openspec change）+ 裁决记录 §10.9.3（R-12 / R-13 由 A 类改归 B 类）⇒ **走 openspec change**（占 1 KPI slot）| 2026-11-20 |
| [cep-archforge-ci-failclosed](improvements/cep-archforge-ci-failclosed.md) | P2 | 登记册 §6.2 S2-8；user Q-10 拍板**方案甲**（缺席打 `[SKIP]` + CMake `SKIP_REGULAR_EXPRESSION` ⇒ ctest 记 **Skipped 而非 Passed**）⇒ 队列 2 **I-9** | 2026-11-20 |
| [cep-openspec-version-pinning](improvements/cep-openspec-version-pinning.md) | P3 | 登记册 §6.2 S3-4；队列 2 **I-8**（`openspec` 版本未 pinning ⇒ 版本漂移不可察）| 2026-11-20 |

## 2026-11-21 · I-6 / I-7 架构裁决后的两份 backlog（2 份）

| Improvement | Priority | 来源 | 创建日期 |
|---|---|---|---|
| [cep-memory-model-operational-trace](improvements/cep-memory-model-operational-trace.md) | P2 | architect 裁决记录 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §三（登记册 §6.4 队列 2 的 **I-6 决策 2**：本仓**不定义** `rfe/fr/ws` 语义 ⇒ 独立立项）| 2026-11-21 |
| [cep-tilelink-tree-manager-view-of](improvements/cep-tilelink-tree-manager-view-of.md) | P2 | architect 裁决记录同文件 §四（登记册 §6.4 队列 2 的 **I-7 决策 3** = I-7 拆 a/b；本单元 = **I-7a** = `TreeManager::view_of` const 纯查询；⚠️ 旧单元 `cep-tier2-second-family-tilelink` 的 engine 侧观测落点路径与 TLE-05 冲突 ⇒ 已标 ⏸ 不可开工）| 2026-11-21 |
| [cep-tilelink-home-side-state-driver](improvements/cep-tilelink-home-side-state-driver.md) | P2 | architect 裁决记录同文件 §四（登记册 §6.4 队列 2 的 **I-7 决策 3** = I-7 拆 a/b；本单元 = **I-7b**；前置 = **I-7a** `TreeManager::view_of`）| 2026-11-21 |
