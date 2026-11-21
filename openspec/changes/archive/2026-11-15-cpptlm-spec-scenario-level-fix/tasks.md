# tasks: cpptlm-spec-scenario-level-fix

> **AC 映射**: proposal AC-01 ~ AC-09
> **上游**: improvement `cpptlm-spec-scenario-level-fix.md` · 台账 **Entry #438**

---

## Phase 0 · 立项态（planner）

- [x] **T0.1** 根因实测（`openspec validate --json` ⇒ `requirements.5.scenarios` / `requirements.1` / `requirements.2` ⇒ 定位到 3 处 `###` 级非 Requirement 标题）
- [x] **T0.2** **原型验证**（影子根 3 处改完 ⇒ 2 份 spec 非 strict 即已 `valid` ⇒ 方案成立）
- [x] **T0.3** improvement + proposal + **1 份 delta**（`vram-controller-functional-mode` MODIFIED）
- [x] **T0.4** 台账 **Entry #438** · KPI **0/3 → 1/3**
- [x] **T0.5** **状态卫生**：8 份 improvement 的「状态」行 stale（实际已归档却写「待实施/已立项」）⇒ 订正

## Phase 1 · 实施

- [x] **T1.1** `vram-controller-functional-mode`：2 处 `### Scenario:` → `#### Scenario:`（**经 1 份 MODIFIED delta 合并**）⇒ 实测 `#### Scenario:` 14 → **16**、`### Requirement:` 7 → **7**
- [x] **T1.2** `pcie-slice-prerequisites`：1 处伪 Requirement 标题 → **非标题**（**直接编辑主 spec**，⛔ MODIFIED 块只能重发**具名 Requirement** ⇒ 无 delta 途径）⇒ 实测 `### Requirement:` 5 → **5**、Scenario 15 → **15**
- [x] **T1.3** **diff 逐行核对**：**语义改动 = 3 处标题**；**正文 0 改**；另 3 行空白 = MODIFIED 合并器段头/文件尾规范化（已归因）

## Phase 2 · 复验

- [x] **T2.1** 2 份 spec 非 strict ⇒ rc=0（AC-01）
- [x] **T2.2** `--json` ERROR 计数 = 0（AC-02）
- [x] **T2.3** 全仓 129 份 spec 逐份非 strict ⇒ rc≠0 = 0（AC-03）
- [x] **T2.4** 43 份 `cep-*` 逐份 `--strict` ⇒ rc≠0 = 0（AC-07）
- [x] **T2.5** 范围零 diff + `artifact_count` 86 不变（AC-06）

## Phase 3 · 归档（**零冻结产物 ⇒ 免五步链**）

- [x] **T3.1** 影子根**预演** `openspec archive` ⇒ 归档后 diff = **3 处标题 + 3 行合并器空白规范化**（`## Requirements` 段头 ×2 + 文件尾 ×1，已归因）
- [x] **T3.2** 真仓 `openspec archive --yes` + 复验 2 份 spec 非 strict rc=0
- [x] **T3.3** 产物 commit
- [x] **T3.4** KPI **1/3 → 0/3**（6 处活口径 + obj 并发句）+ 台账闭合回执 Entry **#439**
- [x] **T3.5** `cep_freeze_check.sh` 无参复验 rc=0（**零冻结产物** —— 两份 spec 均不在 `FROZEN_GLOBS` ⇒ 免五步链；`artifact_count` 86 不变）
