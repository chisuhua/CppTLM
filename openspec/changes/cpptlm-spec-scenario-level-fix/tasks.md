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

- [ ] **T1.1** `vram-controller-functional-mode`：2 处 `### Scenario:` → `#### Scenario:`（**经 delta 合并**，⛔ 不直接改主 spec）
- [ ] **T1.2** `pcie-slice-prerequisites`：1 处伪 Requirement 标题 → **非标题**（**直接编辑主 spec**，⛔ 无 delta 途径）
- [ ] **T1.3** **diff 逐行核对**：增行 = 3 / 删行 = 3；**正文 0 改**

## Phase 2 · 复验

- [ ] **T2.1** 2 份 spec 非 strict ⇒ rc=0（AC-01）
- [ ] **T2.2** `--json` ERROR 计数 = 0（AC-02）
- [ ] **T2.3** 全仓 129 份 spec 逐份非 strict ⇒ rc≠0 = 0（AC-03）
- [ ] **T2.4** 43 份 `cep-*` 逐份 `--strict` ⇒ rc≠0 = 0（AC-07）
- [ ] **T2.5** 范围零 diff + `artifact_count` 86 不变（AC-06）

## Phase 3 · 归档（**零冻结产物 ⇒ 免五步链**）

- [ ] **T3.1** 影子根**预演** `openspec archive` ⇒ 归档后 diff 只落目标段
- [ ] **T3.2** 真仓 `openspec archive --yes` + 复验 2 份 spec 非 strict rc=0
- [ ] **T3.3** 产物 commit
- [ ] **T3.4** KPI **1/3 → 0/3**（6 处活口径 + obj 并发句）+ 台账闭合回执 Entry **#439**
- [ ] **T3.5** `cep_freeze_check.sh` 无参复验 rc=0（`frozen_at_commit ≠ HEAD` 是常态告警，⛔ 不重跑 `--update`）
