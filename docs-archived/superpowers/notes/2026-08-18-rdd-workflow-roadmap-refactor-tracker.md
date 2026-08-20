# Roadmap Refactor — rdd-workflow 侧实施追踪

> **状态**: 🟡 rdd-workflow 侧已 push,等待 review/merge;**CppTLM 侧 roadmap 重写暂缓**
> **创建日期**: 2026-08-18
> **关联提案**: [`2026-08-18-roadmap-rdd-workflow-refactor`](./2026-08-18-roadmap-rdd-workflow-refactor.md) (plan) · [`2026-08-18-roadmap-rdd-workflow-refactor-design`](../specs/2026-08-18-roadmap-rdd-workflow-refactor-design.md) (design spec)

---

## 背景

[CppTLM roadmap.md](../../../../roadmap.md) v2.3 当前有强烈子阶段需求 (Phase 7.A-7.F / Phase 9.0-9.6),但 rdd-workflow `_lib/roadmap_state.py` 默认 regex 仅识别平铺 `phase-N`,不支持 `phase-N.M` 嵌套。直接重写 CppTLM roadmap 会导致 rdd-workflow 工具链全失效。

**结论**:rdd-workflow 侧先添加嵌套语法支持 → CppTLM 侧再消费。

---

## rdd-workflow 侧实施状态

| 项目 | 状态 | 引用 |
|------|:----:|------|
| **分支** | ✅ 已 push | [`feature/roadmap-nested-phase`](https://github.com/chisuhua/rdd-workflow/tree/feature/roadmap-nested-phase) |
| **PR URL** | 🟡 待创建 | <https://github.com/chisuhua/rdd-workflow/pull/new/feature/roadmap-nested-phase> |
| **3 个 commit** | ✅ 已落地 | 见下 |
| **测试** | ✅ 24/24 pass(test_roadmap_state.py)+ 1825/1825 pass(全 unit suite)+ 9/9 smoke(bats) | — |

### Commits

| SHA | 类型 | 摘要 |
|-----|------|------|
| `9dd2c60` | feat | add nested-phase regex constants (PHASE_ID_RE/TOP_PHASE_RE/SUB_PHASE_RE) |
| `e14cf8f` | fix | aggregate phase-N.M sub-phases in advance_phase pre-check + skip sub-phases for next-phase search |
| `fd5e01e` | docs | document nested phase-N.M syntax + parent-umbrella-before-subheading constraint |

### 修改的文件

- `_lib/roadmap_state.py` — +15 行(3 常量 + pre-check 聚合 + next-phase 显式过滤)
- `tests/unit/test_roadmap_state.py` — +127 行(3 新测试 + import re)
- `skills/roadmap/SKILL.md` — +33 行(新增"嵌套阶段语法(可选扩展)"章节)

---

## CppTLM 侧 — 暂缓 (pending rdd-workflow merge)

按 plan §10 风险与执行顺序:**必须 rdd-workflow PR 合并后才能开始 CppTLM 改动**。

### CppTLM 侧待执行项(plan Phase B/C,共 8 Tasks)

| Task | 内容 | 依赖 |
|------|------|------|
| 6 | backup roadmap.md via `git tag roadmap-v2.3 main` | — |
| 7-9 | 重写 roadmap.md 为 phase-N.M 嵌套语法 | rdd-workflow merge |
| 10 | append Phase 9 + 跨仓 ADR-088 链接 | — |
| 11 | append 待办 + AUTO-SPRINT 哨兵 | — |
| 12 | 创建 `.rddf/state/roadmap-state.json` | — |
| 13-15 | 3 个 openspec changes 加 `roadmap-meta.yaml` | — |
| 16-18 | (plan 末尾 Tasks) | — |

### 阻塞条件

| 阻塞项 | 解除条件 |
|--------|----------|
| rdd-workflow PR 未合并 | merge `feature/roadmap-nested-phase` 到 master |
| CppTLM ` M roadmap.md` 与 ` M scripts/test/docs_sync_check.sh` 既有改动未提交 | 决定保留/丢弃/合并后再处理 |

---

## 下一步 Actions

1. **rdd-workflow 侧**(用户 / maintainer 决定):
   - [ ] review 3 个 commit(`9dd2c60` / `e14cf8f` / `fd5e01e`)
   - [ ] 在 GitHub 创建 PR(URL 见上)
   - [ ] merge 到 master(推荐 squash)

2. **CppTLM 侧**(rdd-workflow merge 后启动):
   - [ ] 决定 `M roadmap.md` 既有改动的处置
   - [ ] 启动 Phase B Tasks 7-9(roadmap.md → phase-N.M 嵌套)
   - [ ] 启动 Phase B Task 12(state JSON)
   - [ ] 启动 Phase C Tasks 13-15(roadmap-meta.yaml × 3 changes)

---

## 验证证据

- **TDD 纪律**:
  - Task 2 RED:`AttributeError: module 'skills._lib.roadmap_state' has no attribute 'PHASE_ID_RE'` ✓
  - Task 3 RED:`assert rc == 1` 失败(实得 rc=0,父 phase 平凡通过)✓
  - Task 3 GREEN:24/24 pass ✓
- **测试覆盖**:`tests/unit/test_roadmap_state.py` 21 → 24 tests
- **回归**:`tests/unit/` 全 suite 1825 passed, 2 skipped(无 regression)
- **Smoke**:`tests/smoke.bats` 9/9 ok
- **Push**:branch `feature/roadmap-nested-phase` → origin 成功

---

*维护: Sisyphus session 2026-08-18 · 更新触发条件: rdd-workflow PR 状态变更或 CppTLM 侧启动*