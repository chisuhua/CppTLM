# CppTLM Roadmap Refactor (rdd-workflow v3.0+ Compatible) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite `/workspace/project/CppTLM/roadmap.md` to comply with `/workspace/project/rdd-workflow` v3.0+ template (phase-N + 任务分类 table + kebab-case cat_id + AUTO-SPRINT sentinels) while preserving CppTLM's APU/dGPU business semantics via `phase-N.M` nested sub-phase syntax.

**Architecture:** Two-repo coordinated refactor. (1) rdd-workflow: add 3 regex constants + 1 sub-phase aggregation function (~15 lines) + 4 new unit tests + SKILL.md nested-phase docs. (2) CppTLM: rewrite roadmap.md to nested phase format + create `.rddf/state/roadmap-state.json` + add `roadmap-meta.yaml` to 3 openspec changes.

**Tech Stack:** Python 3.11+ (rdd-workflow `_lib/roadmap_state.py`), pytest (unit tests), YAML (openspec change meta), Markdown (roadmap.md), Git (commits + tag).

**Spec:** `/workspace/project/CppTLM/docs/superpowers/specs/2026-08-18-roadmap-rdd-workflow-refactor-design.md` (997 lines)

---

## File Structure

This refactor touches 2 repos:

| Repo | Files Created | Files Modified |
|------|--------------|----------------|
| `/workspace/project/rdd-workflow` | — | `_lib/roadmap_state.py` (~15 lines), `tests/unit/test_roadmap_state.py` (~45 lines), `skills/roadmap/SKILL.md` (~30 lines) |
| `/workspace/project/CppTLM` | `.rddf/state/roadmap-state.json`, `openspec/changes/{cpptlm-d1-p1-pipeline-scoreboard,2026-06-24-gpu-soc-phase8b-core,2026-06-24-gpu-soc-phase8c-advanced}/roadmap-meta.yaml` | `roadmap.md` (~300 lines rewrite) |

No new Python modules; no new dependencies; no file splits. All changes are additive to existing files.

---

## Phase A: rdd-workflow Patches (TDD)

### Task 1: Setup Worktrees

**Files:**
- Worktrees: `/workspace/project/rdd-workflow/.worktrees/refactor-nested-phase` + `/workspace/project/CppTLM/.worktrees/refactor-roadmap`

- [ ] **Step 1: Verify clean state in both repos**

```bash
cd /workspace/project/rdd-workflow && git status --short
cd /workspace/project/CppTLM && git status --short
```

Expected: Both empty (or only untracked files).

- [ ] **Step 2: Create rdd-workflow worktree on a feature branch**

```bash
cd /workspace/project/rdd-workflow
git worktree add .worktrees/refactor-nested-phase -b feature/roadmap-nested-phase
```

Expected: New worktree at `.worktrees/refactor-nested-phase` on branch `feature/roadmap-nested-phase`.

- [ ] **Step 3: Create CppTLM worktree on a feature branch**

```bash
cd /workspace/project/CppTLM
git worktree add .worktrees/refactor-roadmap -b feature/roadmap-rdd-workflow-compat
```

Expected: New worktree at `.worktrees/refactor-roadmap` on branch `feature/roadmap-rdd-workflow-compat`.

- [ ] **Step 4: Verify pytest works in rdd-workflow worktree**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
python -m pytest tests/unit/test_roadmap_state.py -v 2>&1 | tail -5
```

Expected: All existing tests PASS.

- [ ] **Step 5: Commit worktree setup**

Worktrees are untracked state — no commit needed. Mark step complete.

---

### Task 2: Add Nested-Phase Regex Constants

**Files:**
- Modify: `/workspace/project/rdd-workflow/_lib/roadmap_state.py` (add 3 constants at top, after imports)
- Test: `/workspace/project/rdd-workflow/tests/unit/test_roadmap_state.py` (add 1 test)

- [ ] **Step 1: Write the failing test**

Add to `tests/unit/test_roadmap_state.py` (append before the last line):

```python
# ----- nested phase regex constants -----

def test_phase_id_constants_match_nested_and_top_level():
    """Verify PHASE_ID_RE matches both phase-N and phase-N.M; TOP_PHASE_RE only phase-N."""
    import re
    # phase-N.M should match PHASE_ID_RE but not TOP_PHASE_RE
    assert re.fullmatch(roadmap_state.PHASE_ID_RE, "phase-3.1")
    assert not re.fullmatch(roadmap_state.TOP_PHASE_RE, "phase-3.1")
    # phase-N should match both
    assert re.fullmatch(roadmap_state.PHASE_ID_RE, "phase-3")
    assert re.fullmatch(roadmap_state.TOP_PHASE_RE, "phase-3")
    # SUB_PHASE_RE captures parent and sub index
    m = re.fullmatch(roadmap_state.SUB_PHASE_RE, "phase-3.1")
    assert m is not None
    assert m.group(1) == "3"
    assert m.group(2) == "1"
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
python -m pytest tests/unit/test_roadmap_state.py::test_phase_id_constants_match_nested_and_top_level -v
```

Expected: FAIL with `AttributeError: module 'skills._lib.roadmap_state' has no attribute 'PHASE_ID_RE'`.

- [ ] **Step 3: Add the constants to roadmap_state.py**

Edit `/workspace/project/rdd-workflow/.worktrees/refactor-nested-phase/skills/_lib/roadmap_state.py`. Add immediately after the imports (after line 37, before `_now_iso`):

```python
# Nested phase ID support (backward compatible — flat phase-N still works)
PHASE_ID_RE = r"phase-\d+(?:\.\d+)?"          # any phase ID (top-level + sub-phase)
TOP_PHASE_RE = r"phase-\d+"                     # top-level only
SUB_PHASE_RE = r"phase-(\d+)\.(\d+)"            # sub-phase (captures parent + sub index)
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
python -m pytest tests/unit/test_roadmap_state.py::test_phase_id_constants_match_nested_and_top_level -v
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
git add skills/_lib/roadmap_state.py tests/unit/test_roadmap_state.py
git commit -m "feat(roadmap): add nested-phase regex constants (PHASE_ID_RE/TOP_PHASE_RE/SUB_PHASE_RE)"
```

---

### Task 3: Fix advance_phase Sub-Phase Aggregation

**Files:**
- Modify: `/workspace/project/rdd-workflow/skills/_lib/roadmap_state.py` (lines 353-373 + 385)
- Test: `/workspace/project/rdd-workflow/tests/unit/test_roadmap_state.py`

- [ ] **Step 1: Write the failing test**

Add to `tests/unit/test_roadmap_state.py`:

```python
# ----- advance_phase sub-phase aggregation -----

def test_advance_phase_aggregates_subphases_prevents_trivial_pass(tmp_roadmap_repo, capsys):
    """Parent phase-3 with empty categories must NOT pass pre-check when sub-phase phase-3.2 incomplete."""
    state_file = tmp_roadmap_repo["state_file"]
    roadmap_file = tmp_roadmap_repo["roadmap_file"]

    # Write roadmap containing phase-3 with sub-phase heading (parent umbrella + sub-phase table)
    Path(roadmap_file).write_text(
        "**当前阶段**: phase-3\n\n"
        "### Phase 7: APU (phase-3)\n"
        "**状态**: 🔄 进行中\n"
        "**完成条件**:\n  - [ ] phase-3.1 ~ phase-3.6 全部完成\n\n"
        "#### 任务分类\n"
        "| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |\n"
        "|--------|------|------|--------|--------------|\n"
        "| gpu-infra | 7.A GPU 基础设施 | (phase-3.1) | P0 | gpu |\n\n"
        "### 7.A GPU 基础设施 (phase-3.1)\n"
        "**状态**: ✅ 完成\n\n"
        "#### 任务分类\n"
        "| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |\n"
        "|--------|------|------|--------|--------------|\n"
        "| gpu-bundle | GPU Bundle | bundle schema | P0 | ... |\n\n"
        "### 7.B ComputeUnit (phase-3.2)\n"
        "**状态**: 🟡 Pending\n\n"
        "#### 任务分类\n"
        "| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |\n"
        "|--------|------|------|--------|--------------|\n"
        "| cu-tickloop | CU tick loop | ... | P0 | ... |\n"
    )

    # Build state: phase-3 umbrella empty, phase-3.1 complete, phase-3.2 incomplete
    state = {
        "version": 1,
        "current_phase": "phase-3",
        "phases": {
            "phase-3": {
                "status": "in_progress",
                "categories": {},
                "gate_status": {"all_changes_complete": False, "checklist": {}},
            },
            "phase-3.1": {
                "status": "completed",
                "categories": {
                    "gpu-bundle": {"changes": ["c1"], "completed_changes": ["c1"]},
                },
                "gate_status": {"all_changes_complete": True, "checklist": {}},
            },
            "phase-3.2": {
                "status": "in_progress",
                "categories": {
                    "cu-tickloop": {"changes": ["c2"], "completed_changes": []},
                },
                "gate_status": {"all_changes_complete": False, "checklist": {}},
            },
        },
    }
    Path(state_file).write_text(json.dumps(state, indent=2))

    rc = roadmap_state.advance_phase(roadmap_file, state_file)
    out = capsys.readouterr().out
    # Must report phase-3.2 incomplete and return 1
    assert rc == 1
    assert "phase-3.2" in out
    assert "未完成" in out


def test_advance_phase_skips_subphases_for_next_phase(tmp_roadmap_repo, capsys):
    """advance_phase next-phase search must skip phase-N.M and find phase-4 after phase-3."""
    state_file = tmp_roadmap_repo["state_file"]
    roadmap_file = tmp_roadmap_repo["roadmap_file"]

    Path(roadmap_file).write_text(
        "**当前阶段**: phase-3\n\n"
        "### Phase 7: APU (phase-3)\n"
        "**状态**: ✅ 完成\n"
        "#### 任务分类\n"
        "| 分类ID | 名称 | 描述 | 优先级 |\n"
        "|--------|------|------|--------|\n"
        "| gpu-infra | GPU | ... | P0 |\n\n"
        "### 7.A GPU (phase-3.1)\n"
        "#### 任务分类\n"
        "| 分类ID | 名称 | 描述 | 优先级 |\n"
        "|--------|------|------|--------|\n"
        "| gpu-bundle | bundle | ... | P0 |\n\n"
        "### Phase 8: Validation (phase-4)\n"
        "#### 任务分类\n"
        "| 分类ID | 名称 | 描述 | 优先级 |\n"
        "|--------|------|------|--------|\n"
        "| trace-align | trace | ... | P0 |\n"
    )

    # Build state: phase-3 fully complete (including sub-phases), phase-4 pending
    state = {
        "version": 1,
        "current_phase": "phase-3",
        "phases": {
            "phase-3": {
                "status": "in_progress",
                "categories": {
                    "gpu-infra": {"changes": ["c0"], "completed_changes": ["c0"]},
                },
                "gate_status": {"all_changes_complete": True, "checklist": {}},
            },
            "phase-3.1": {
                "status": "completed",
                "categories": {
                    "gpu-bundle": {"changes": ["c1"], "completed_changes": ["c1"]},
                },
                "gate_status": {"all_changes_complete": True, "checklist": {}},
            },
            "phase-4": {
                "status": "pending",
                "categories": {
                    "trace-align": {"changes": [], "completed_changes": []},
                },
                "gate_status": {"all_changes_complete": True, "checklist": {}},
            },
        },
    }
    Path(state_file).write_text(json.dumps(state, indent=2))

    rc = roadmap_state.advance_phase(roadmap_file, state_file)
    out = capsys.readouterr().out
    # Should advance to phase-4 (skipping phase-3.1 sub-phase)
    assert rc == 0
    assert "phase-4" in out
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
python -m pytest tests/unit/test_roadmap_state.py::test_advance_phase_aggregates_subphases_prevents_trivial_pass tests/unit/test_roadmap_state.py::test_advance_phase_skips_subphases_for_next_phase -v
```

Expected: Both FAIL (current advance_phase does not aggregate sub-phases).

- [ ] **Step 3: Modify advance_phase to aggregate sub-phases**

Edit `/workspace/project/rdd-workflow/.worktrees/refactor-nested-phase/skills/_lib/roadmap_state.py`. In the `advance_phase` function (around line 353-373), replace the existing pre-check loop with the aggregated version:

```python
    # Pre-check: all changes complete (aggregates parent + phase-N.M sub-phases)
    all_complete = True
    # 1. Check current phase's own categories
    for cat_id, cat_data in phase_data.get("categories", {}).items():
        total = len(cat_data.get("changes", []))
        completed = len(cat_data.get("completed_changes", []))
        if completed < total:
            all_complete = False
            print(f"❌ 分类 {cat_id} 未完成: {completed}/{total}")
    # 2. Aggregate all sub-phases matching phase-N.M
    sub_ids = sorted(pid for pid in state.get("phases", {})
                    if re.match(rf"^{re.escape(current)}\.\d+$", pid))
    for pid in sub_ids:
        for cat_id, cat_data in state["phases"][pid].get("categories", {}).items():
            total = len(cat_data.get("changes", []))
            completed = len(cat_data.get("completed_changes", []))
            if completed < total:
                all_complete = False
                print(f"❌ 子阶段 {pid} 分类 {cat_id} 未完成: {completed}/{total}")
```

- [ ] **Step 4: Modify advance_phase to skip sub-phases for next-phase search**

In the same function (around line 385), replace:

```python
    phases = re.findall(r"\((phase-\d+)\)", content)
```

with:

```python
    phases = [p for p in re.findall(r"\((phase-\d+(?:\.\d+)?)\)", content) if "." not in p]
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
python -m pytest tests/unit/test_roadmap_state.py::test_advance_phase_aggregates_subphases_prevents_trivial_pass tests/unit/test_roadmap_state.py::test_advance_phase_skips_subphases_for_next_phase -v
```

Expected: Both PASS.

- [ ] **Step 6: Run full test suite to verify no regressions**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
python -m pytest tests/unit/test_roadmap_state.py -v
```

Expected: All tests PASS (existing + 2 new).

- [ ] **Step 7: Commit**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
git add skills/_lib/roadmap_state.py tests/unit/test_roadmap_state.py
git commit -m "fix(roadmap): aggregate phase-N.M sub-phases in advance_phase pre-check + skip sub-phases for next-phase search"
```

---

### Task 4: Update SKILL.md with Nested-Phase Documentation

**Files:**
- Modify: `/workspace/project/rdd-workflow/skills/roadmap/SKILL.md` (append section after line 484, before "关键约束")

- [ ] **Step 1: Write the failing documentation snippet**

No automated test for docs — manual verification only.

- [ ] **Step 2: Add the nested-phase section**

Edit `/workspace/project/rdd-workflow/.worktrees/refactor-nested-phase/skills/roadmap/SKILL.md`. Append immediately before the "## 关键约束" section (line 478):

````markdown
## 嵌套阶段语法(可选扩展)

对于业务演进包含子阶段的项目(如 CppTLM Phase 7.A-7.F),roadmap 支持嵌套 ID:

```markdown
### Phase 7: CPU+GPGPU Fused SoC (APU-first) (phase-3)
**目标**: ...
**完成条件**:
  - [ ] phase-3.1 ~ phase-3.6 全部完成

#### 任务分类                    ← 【硬性约束】必须先于所有 ### 子阶段 heading
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| gpu-infra | 7.A GPU 基础设施 | (phase-3.1) | P0 | ... |
| ... | | | | |

### 7.A GPU 基础设施 (phase-3.1)  ← 子阶段 heading
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| gpu-bundle | ... | ... | P0 | ... |
```

**约束**:
1. 父 phase 伞表必须先于所有 `###` 子阶段 heading(`get_phase_categories` section 边界 regex 限制)
2. change meta `roadmap.phase` 可指向子阶段 ID(`phase-3.3`)
3. `advance_phase` 自动聚合子阶段完成度
4. 嵌套 ID 语法:`phase-N.M`(数字后缀),业务标签(7.A/9.0)出现在 heading 文本
5. 嵌套是正向扩展,平铺 `phase-N` 仍完全支持

---

````

- [ ] **Step 3: Verify markdown renders correctly**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
grep -n "嵌套阶段语法" skills/roadmap/SKILL.md
```

Expected: One match (the new section heading).

- [ ] **Step 4: Commit**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
git add skills/roadmap/SKILL.md
git commit -m "docs(roadmap): document nested phase-N.M syntax + parent-umbrella-before-subheading constraint"
```

---

### Task 5: Verify All rdd-workflow Tests Pass + Push Branch

**Files:** (verification only)

- [ ] **Step 1: Run full test suite**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
python -m pytest tests/unit/test_roadmap_state.py -v 2>&1 | tail -20
```

Expected: All tests PASS (existing + 3 new from Tasks 2 & 3).

- [ ] **Step 2: Run smoke tests**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
bash tests/smoke.bats 2>&1 | tail -10
```

Expected: All smoke tests PASS (or skipped if bats not installed).

- [ ] **Step 3: Push branch (don't merge yet — wait for CppTLM refactor to validate)**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
git push -u origin feature/roadmap-nested-phase
```

If push fails (no remote / no auth), record branch as local-only and proceed.

Expected: Branch published (or noted as local-only).

---

## Phase B: CppTLM Roadmap Rewrite

### Task 6: Backup Old roadmap.md + Initialize CppTLM Worktree

**Files:**
- Tag (no file change): git tag `roadmap-v2.3` on main
- Worktree: already created in Task 1 Step 3

- [ ] **Step 1: Backup old roadmap via git tag**

```bash
cd /workspace/project/CppTLM
git tag roadmap-v2.3 main
git tag -l roadmap-v2.3
```

Expected: Tag `roadmap-v2.3` listed.

- [ ] **Step 2: Verify worktree exists and points to feature branch**

```bash
cd /workspace/project/CppTLM
git worktree list
```

Expected: Two worktrees listed — main + `.worktrees/refactor-roadmap` on `feature/roadmap-rdd-workflow-compat`.

- [ ] **Step 3: Verify current worktree clean**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git status --short
```

Expected: Empty output (clean).

---

### Task 7: Write New roadmap.md Header + 元信息 + Phase 5 + Phase 6

**Files:**
- Modify: `/workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md` (replace entire file)

- [ ] **Step 1: Write the file header + 元信息 + Phase 5 + Phase 6 sections**

Write to `/workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md`:

````markdown
# CppTLM Roadmap

> **版本**: v3.0 (rdd-workflow v3.0+ 兼容)
> **当前阶段**: phase-3 (Phase 7: APU-first)
> **最后更新**: 2026-08-18
> **历史**: Phase 4 已于 2026-Q2 完成(见 [`AGENTS.md`](AGENTS.md) §已完成里程碑 / git history);Phase 10 为占位预留,启动条件未达,不纳入本 roadmap。
> **依赖框架**: [rdd-workflow v3.0+](../rdd-workflow)(`_lib/roadmap_state.py` + `_lib/roadmap_sprint.py`)
> **关联文档**:
> - [`AGENTS.md`](AGENTS.md) — 项目总览(包含 Phase 4 历史细节)
> - [`docs/architecture/01-hybrid-architecture-v2.1.md`](docs/architecture/01-hybrid-architecture-v2.1.md) — 架构参考

## 元信息

- **当前阶段**: phase-3
- **最后更新**: 2026-08-18
- **版本**: v3.0

### Phase 5: Protocol Bridge (phase-1)

**目标**: 实现协议桥接基础(参考 gem5 `src/mem/protocol/` 简化版)
**状态**: ⏳ 未开始
**完成条件**:
  - [ ] 所有分类的 change 完成
  - [ ] `cpptlm_tests "[bridge]"` 标签用例通过

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| protocol-bridge | Protocol Bridge 集成 | 5.1-5.5 子任务(单阶段,未拆分) | P0 | 协议桥接；MOESI 升级路径 |

### Phase 6: Multi-Cluster SoC Validation (phase-2)

**目标**: 跨 cluster 的 coherence 不变量检查 + Protocol Bridge 集成测试
**状态**: ⏳ 未开始
**前置阶段**: phase-1
**完成条件**:
  - [ ] 所有分类的 change 完成
  - [ ] 6.2 / 6.3 子任务验证通过(注:6.1 / 6.4 已拆至 phase-3.6)

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| multicluster-validation | 多集群验证 | 6.2 cross-cluster coherence + 6.3 protocol bridge 集成 | P0 | coherence 不变量；端到端 trace |
````

- [ ] **Step 2: Verify file is valid markdown**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
wc -l roadmap.md
head -5 roadmap.md
```

Expected: ~50 lines, first line `# CppTLM Roadmap`.

- [ ] **Step 3: Commit partial file**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git add roadmap.md
git commit -m "wip(roadmap): add file header + 元信息 + Phase 5 + Phase 6 (WIP, more phases incoming)"
```

---

### Task 8: Append Phase 7 (Umbrella + 7.A-7.F Sub-Phases)

**Files:**
- Modify: `/workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md` (append)

- [ ] **Step 1: Append Phase 7 section to roadmap.md**

Run:

```bash
cat >> /workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md << 'PHASE7_EOF'

### Phase 7: CPU+GPGPU Fused SoC (APU-first) (phase-3)

**目标**: 端到端实现 APU 形态 CPU+GPGPU 融合 SoC 仿真——2 个 CPU 流量源 + 4 个 GPU Compute Unit + 共享 memory + 单一 Coherence 域
**状态**: 🔄 进行中
**前置阶段**: phase-2
**完成条件**:
  - [ ] phase-3.1 ~ phase-3.6 全部完成
  - [ ] `test/python/test_apu_soc.py` 端到端运行通过
  - [ ] `cpptlm_tests "[gpu][phase6]"` 100% PASS

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| gpu-infra | 7.A GPU 基础设施 | ✅ 完成 2026-06-11 (phase-3.1) | P0 | 发起器建模；内存层次 |
| cu-blackbox | 7.B ComputeUnit 黑盒 | (phase-3.2) | P0 | tick loop；inflight kernel |
| coherence | 7.C Coherence 集成 | 🔴 最高风险 (phase-3.3) | P0 | MOESI 简化版；snoop fanout |
| tcc-bridge | 7.D TCC Bridge + 内存层次 | (phase-3.4) | P1 | write coalescing；snoop fan-in |
| multi-cu-noc | 7.E Multi-CU + NoC | (phase-3.5) | P1 | BidirectionalPortAdapter |
| apu-demo | 7.F Full APU SoC Demo | 承接原 6.1/6.4 (phase-3.6) | P0 | 端到端验证 |

### 7.A GPU 基础设施 (phase-3.1)

**业务标签**: Phase 7.A
**状态**: ✅ 完成 2026-06-11
**验收**:
  - `cpptlm_tests "[gpu]"` 通过
  - `configs/gpu_standalone.json` 可执行

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| gpu-bundle | GPU Bundle 扩展 | `include/bundles/compute_bundles_tlm.hh` 新增 kernel_id/workgroup_id/wavefront_id 字段 | P0 | bundle schema 扩展 |
| gpu-init | GPU 基础设施 | `include/tlm/gpu/gpu_tlm.hh` v0 + REGISTER_CHSTREAM + `configs/gpu_standalone.json` | P0 | gpu_standalone 端到端 |

### 7.B ComputeUnit 黑盒 (phase-3.2)

**业务标签**: Phase 7.B
**状态**: 🟡 Pending
**风险**: 🟡 Med-High
**范围**: `ComputeUnitTLM`(tick loop + inflight_kernel_reqs_ map + workgroup_progress_);CrossbarTLM 扩展 GPU 地址路由;CacheTLM 临时 bypass GPU 请求
**验收**:
  - `configs/apu_demo_v1.json` 端到端运行
  - `cpptlm_tests "[gpu][phase6]"` PASS

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| cu-tickloop | ComputeUnitTLM tick loop | tick loop + inflight_kernel_reqs_ map | P0 | tick 实现；inflight |
| xbar-gpu-routing | CrossbarTLM GPU 地址路由 | GPU 地址空间路由 | P1 | GPU 地址解析 |
| cache-bypass | CacheTLM 临时 bypass GPU | 临时 bypass 路径 | P1 | bypass 路径 |

### 7.C Coherence Protocol 集成 (phase-3.3)

**业务标签**: Phase 7.C
**状态**: 🟡 Pending
**风险**: 🔴 **最高风险**
**范围**: CacheTLM protocol-aware 改造(`CacheLine = {data, state, sharers}` + 6×6 状态转换表);CoherenceDomain 与 CacheTLM 集成(snoop callback + lookup_home_node)
**验收**:
  - `configs/apu_demo_v2.json` 跨 cache 一致性
  - `cpptlm_tests "[coherence][gpu]"` PASS

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| cache-state-machine | CacheTLM state machine | 6×6 转换表(不复制 gem5 slicc) | P0 | MOESI 简化版 |
| snoop-callback | CoherenceDomain snoop callback | snoop fanout 集成 | P0 | snoop fanout |
| lookup-home-node | lookup_home_node() 集成 | address mapping | P1 | address mapping |

### 7.D TCC Bridge + 内存层次 (phase-3.4)

**业务标签**: Phase 7.D
**状态**: 🟡 Pending
**风险**: 🟡 Med
**范围**: `TCC_TLM`(DualPortStreamAdapter,write coalescing + snoop fan-in);`MemoryTLM` 扩展 `hbm_mode` 参数
**验收**:
  - `configs/apu_demo_v3.json` 写合并
  - `cpptlm_tests "[gpu][tcc]"` PASS

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| tcc-dualport | TCC DualPortStreamAdapter | write coalescing + snoop fan-in | P0 | 写合并；fan-in |
| memory-hbm | MemoryTLM hbm_mode | HBM 内存模式参数 | P1 | hbm 配置 |

### 7.E Multi-CU + NoC (phase-3.5)

**业务标签**: Phase 7.E
**状态**: 🟡 Pending
**风险**: 🟢 Low
**范围**: `ComputeUnitTLM` 数组模式(`num_cus=4`);`BidirectionalPortAdapter<N>` 连接 CU ↔ GPU Crossbar;复用 `RouterTLM` / `LinkTLM` / `NoCFlitBundle`
**验收**:
  - `configs/apu_demo_v4.json` 4 CU 并发
  - `cpptlm_tests "[gpu][noc]"` PASS

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| multi-cu-noc | Multi-CU 数组 + NoC | `ComputeUnitTLM` 数组 + BidirectionalPortAdapter<N> | P0 | NoC 拓扑；4 CU 并发 |
| cu-bidir-port | BidirectionalPortAdapter<N> | CU ↔ GPU Crossbar 连接 | P1 | 双向端口 |

### 7.F Full APU SoC Demo (phase-3.6)

**业务标签**: Phase 7.F
**状态**: 🟡 Pending
**风险**: 🟢 Low
**范围**: `configs/apu_full_soc.json`(2× TrafficGenTLM + 4× ComputeUnitTLM + TCC + Crossbar + Memory + 单 `CoherenceDomain("apu_domain")`);`test/python/test_apu_soc.py` 端到端验证;统计 dashboard
**验收**:
  - JSON 配置可读、trace 可观察、测试可断言

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| apu-demo | Full APU SoC Demo | `apu_full_soc.json` + `test_apu_soc.py` | P0 | 端到端验证 |
| apu-dashboard | 统计 dashboard | 性能/带宽/延迟可视化 | P1 | dashboard 输出 |
PHASE7_EOF
```

- [ ] **Step 2: Verify file structure**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
grep -c "^### Phase 7:\|^### 7\." roadmap.md
wc -l roadmap.md
```

Expected: 7 matches (1 parent + 6 sub), ~120 lines total.

- [ ] **Step 3: Commit**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git add roadmap.md
git commit -m "feat(roadmap): add Phase 7 (APU-first) with 6 sub-phases (phase-3.1 ~ phase-3.6)"
```

---

### Task 9: Append Phase 8 (Umbrella + 8.1-8.4 Sub-Phases)

**Files:**
- Modify: `/workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md` (append)

- [ ] **Step 1: Append Phase 8 section**

Run:

```bash
cat >> /workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md << 'PHASE8_EOF'

### Phase 8: APU 验证 + 性能基准 (phase-4)

**目标**: 端到端 trace 验证 + 性能 dashboard + 与 gem5 `apu_se.py` 输出对齐
**状态**: 📋 计划中
**前置阶段**: phase-3
**完成条件**:
  - [ ] phase-4.1 ~ phase-4.4 全部完成

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| apu-validation | 8.1 Trace alignment tooling | CppTLM trace → gem5-compatible 格式转换器 | P0 | trace2gem5 CLI |
| perf-baseline | 8.2 端到端 correctness check | 关键 kernel (vector add / matrix mul) 行为对标 | P0 | < 1% 误差 |
| regression | 8.3 性能 dashboard | IPC / 带宽 / 延迟 / TCC hit rate dashboard | P1 | dashboard 可视化 |
| perf-report | 8.4 Regression baseline | 锁定 v2.2 性能基线;后续改动 ±5% | P1 | CI 集成 regression |

### 8.1 Trace alignment tooling (phase-4.1)

**业务标签**: Phase 8.1
**状态**: 📋 计划中
**风险**: 🟢 Low
**验收**:
  - `cpptlm trace2gem5` CLI 输出可被 gem5 解析

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| trace-align | trace2gem5 转换器 | CppTLM trace → gem5 兼容格式 | P0 | trace 格式 |

### 8.2 端到端 correctness check (phase-4.2)

**业务标签**: Phase 8.2
**状态**: 📋 计划中
**风险**: 🟡 Med
**验收**:
  - 输出 trace 与 gem5 reference 比对误差 < 1%

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| e2e-correctness | kernel 行为对标 | vector add / matrix mul 行为对标 | P0 | < 1% 误差 |

### 8.3 性能 dashboard (phase-4.3)

**业务标签**: Phase 8.3
**状态**: 📋 计划中
**风险**: 🟢 Low
**验收**:
  - `cpptlm dashboard` 输出可视化报告

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| perf-dashboard | 性能 dashboard | IPC / 带宽 / 延迟 / TCC hit rate 可视化 | P1 | dashboard 输出 |

### 8.4 Regression baseline (phase-4.4)

**业务标签**: Phase 8.4
**状态**: 📋 计划中
**风险**: 🟡 Med
**验收**:
  - CI 集成 regression check

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| regression-baseline | 性能基线锁定 | v2.2 性能基线 ±5% | P1 | CI 集成 |
PHASE8_EOF
```

- [ ] **Step 2: Verify**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
grep -c "^### Phase 8:\|^### 8\." roadmap.md
```

Expected: 5 matches (1 parent + 4 sub).

- [ ] **Step 3: Commit**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git add roadmap.md
git commit -m "feat(roadmap): add Phase 8 (APU validation) with 4 sub-phases (phase-4.1 ~ phase-4.4)"
```

---

### Task 10: Append Phase 9 (Umbrella + 9.0-9.6 Sub-Phases + Cross-Repo Link)

**Files:**
- Modify: `/workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md` (append)

- [ ] **Step 1: Append Phase 9 section**

Run:

```bash
cat >> /workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md << 'PHASE9_EOF'

### Phase 9: dGPU-first SoC 集成 (phase-5)

**目标**: dGPU-first 形态实现(双独立 CoherenceDomain + PCIe Bridge);为 UsrLinuxEmu GPU 驱动开发提供真硬件行为仿真
**状态**: 🔄 启动中
**前置阶段**: phase-4
**跨仓依赖**: [UsrLinuxEmu ADR-088 v3](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-cpptlm-emu-bridge.md)
  — dlopen("libcpptlm_emulator.so") 消费契约(72 C ABI: 60 forward + 11 callback + 1 register),Phase 9 强绑定
**完成条件**:
  - [ ] phase-5.0 ~ phase-5.6 全部完成(9.5 optional)
  - [ ] 98 个 UsrLinuxEmu Catch2 测试在 `USR_LINUX_EMU_USE_CPPTLM=1` 下全 PASS(0 regression)

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| cpptlm-infra | 9.0 CppTLM 基础设施 | `cpptlm_core` STATIC→SHARED + `libcpptlm_emulator.so` (phase-5.0) | P0 | C ABI 治理 |
| pcie-module | 9.1 CppTLM PCIe 模块 | 🔴 最高风险 (phase-5.1) | P0 | BAR mmap |
| dma-computeunit | 9.2 CppTLM DMA + ComputeUnit | (phase-5.2) | P0 | DMA + CU 完整 |
| amdgpu-cabi | 9.3 dGPU amdgpu C ABI 完整 | UsrLinuxEmu hal_user.cpp dispatch (phase-5.3) | P0 | mode A/B 验证 |
| callback-path | 9.4 callback 路径 + 集成 | 11 callback + 1 register (phase-5.4) | P0 | kernel_workqueue |
| nouveau-path | 9.5 (可选) nouveau path | 26 forward + 3 callback (phase-5.5) | P2 | nouveau 特有 |
| docs-archive | 9.6 文档同步 + 归档 | `docs/superpowers/specs/cpptlm-emu-bridge.md` (phase-5.6) | P1 | ADR 升 Accepted |

### 9.0 CppTLM 基础设施 (phase-5.0)

**业务标签**: Phase 9.0
**状态**: 🟡 Pending
**风险**: 🟡 Med
**范围**: `cpptlm_core` STATIC→SHARED;新增 `libcpptlm_emulator.so` 目标(visibility hidden);`cpptlm_emulator.h`(72 ABI);`c_emulator_api.cc` + `c_emulator_callbacks.cc`;写 ADR-SOC-06/07/08
**验收**:
  - `nm -D libcpptlm_emulator.so` 显示 60+ `cpptlm_emulator_*` 符号

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| cpptlm-core-shared | cpptlm_core STATIC→SHARED | STATIC→SHARED 转换 + visibility hidden | P0 | 符号导出控制 |
| c-abi-header | cpptlm_emulator.h | 72 C ABI 函数声明 | P0 | C ABI 治理 |
| c-emulator-api | c_emulator_api.cc + callbacks | 60 forward + 11 callback + 1 register | P0 | C ABI 实现 |

### 9.1 CppTLM PCIe 模块 (phase-5.1)

**业务标签**: Phase 9.1
**状态**: 🟡 Pending
**风险**: 🔴 **最高风险**
**范围**: `PciHostTLM`(io-pci.md blueprint)+ `PciDeviceTLM`(256B Config Space + MSI-X capability table)+ `PCIBridgeTLM`(BAR0/1/2/5 routing + latency injection)+ BAR backing store + mmap hook
**验收**:
  - 8 个新 Catch2 测试 PASS(v1 5 + v2 3:HMM/migrate_to_ram、PRAMIN/BAR1_window、MMU fault buffer)
  - `dgpu_v0.json` 端到端运行

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| pcie-host | PciHostTLM | io-pci.md blueprint | P0 | host bridge |
| pcie-device | PciDeviceTLM | 256B Config Space + MSI-X | P0 | device 模拟 |
| pcie-bridge | PCIBridgeTLM | BAR0/1/2/5 routing + latency injection | P0 | BAR mmap |
| bar-mmap-hook | BAR backing store + mmap hook | 跨进程 mmap 集成 | P0 | in-process mmap |

### 9.2 CppTLM DMA + ComputeUnit (phase-5.2)

**业务标签**: Phase 9.2
**状态**: 🟡 Pending
**风险**: 🟡 Med
**验收**:
  - `[gpu][pcie][dma]` 标签用例通过

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| dma-device | DmaDeviceTLM | io-dma.md blueprint | P0 | DMA 引擎 |
| computeunit-full | ComputeUnitTLM 完整实现 | gpu-compute-unit.md 完整版 | P0 | CU 完整 |
| memory-hbm-mode | MemoryTLM hbm_mode | HBM 内存模式参数 | P1 | hbm 配置 |

### 9.3 dGPU amdgpu C ABI 完整 (phase-5.3)

**业务标签**: Phase 9.3
**状态**: 🟡 Pending
**风险**: 🟡 Med
**验收**:
  - 98 个 UsrLinuxEmu Catch2 测试在 `USR_LINUX_EMU_USE_CPPTLM=1` 下全 PASS(0 regression)

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| hal-dispatch | hal_user.cpp dispatch | 48 forward fn-ptr 加 CppTLM dispatch 分支 | P0 | mode A/B 验证 |
| amdgpu-abi-tests | 98 个 Catch2 测试 | 0 regression 验证 | P0 | CI 集成 |

### 9.4 callback 路径 + 集成 (phase-5.4)

**业务标签**: Phase 9.4
**状态**: 🟡 Pending
**风险**: 🟡 Med
**验收**:
  - TaskRunner smoke PASS
  - Oracle APPROVED-WITH-CONDITIONS

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| callback-impl | 11 callback + 1 register | callback 实现 | P0 | callback path |
| workqueue-dispatch | kernel_workqueue async dispatch | per ADR-060 异步 dispatch | P0 | 异步 dispatch |

### 9.5 (可选项) nouveau path (phase-5.5)

**业务标签**: Phase 9.5
**状态**: 📋 Optional
**风险**: 🟢 Low
**范围**: 额外 26 forward + 3 callback(nouveau 特有:PRAMIN/BAR1 反转/Nouveau Fence 三代/MMU fault buffer/PFIFO IB-mode push)
**验收**:
  - nouveau 路径 e2e 通过
  - `hal_user.cpp` mode 扩展为 nouveau

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| nouveau-cabi | nouveau C ABI | nouveau 特有 fn-ptr | P2 | nouveau 路径 |

### 9.6 文档同步 + 归档 (phase-5.6)

**业务标签**: Phase 9.6
**状态**: 🟡 Pending
**风险**: 🟢 Low
**验收**:
  - Oracle session 关闭
  - ADR-088 升 Accepted

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| emu-bridge-spec | cpptlm-emu-bridge.md | 开发者文档 | P1 | 文档 |
| adr-archive | ADR-SOC-06/07/08 升 Accepted | ADR 归档 | P1 | ADR 治理 |
PHASE9_EOF
```

- [ ] **Step 2: Verify**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
grep -c "^### Phase 9:\|^### 9\." roadmap.md
wc -l roadmap.md
```

Expected: 8 matches (1 parent + 7 sub), ~280 lines total.

- [ ] **Step 3: Commit**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git add roadmap.md
git commit -m "feat(roadmap): add Phase 9 (dGPU-first) with 7 sub-phases (phase-5.0 ~ phase-5.6) + cross-repo ADR-088 link"
```

---

### Task 11: Append 待办 Section + AUTO-SPRINT Sentinels

**Files:**
- Modify: `/workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md` (append)

- [ ] **Step 1: Append 待办 + AUTO-SPRINT sections**

Run:

```bash
cat >> /workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md << 'TODO_EOF'

## 待办

跨阶段临时任务(roadmap_sprint.py 自动维护下方 sprint 表,本段人工维护):

- [x] ~~Phase 5(ProtocolBridge)启动后回填 5.1-5.5 子任务细节~~ ✅ 2026-08-15 已完成 (roadmap v2.2)
- [x] ~~Phase 6.2 / 6.3 启动后回填 cross-cluster coherence + bridge 集成测试细节~~ ✅ 2026-08-15
- [x] ~~Phase 7.A 启动前需更新 AGENTS.md STRUCTURE 节(添加 `include/tlm/gpu/` 子目录)~~ ✅ 2026-06-11
- [x] ~~Phase 7.F 完成后把 roadmap.md 升级为 v2.2~~ ✅ 2026-08-15
- [x] ~~Phase 9 启动前评估 UsrLinuxEmu ADR-088 v3 影响并修订 roadmap.md~~ ✅ 2026-08-15 (roadmap v2.3)
- [x] ~~roadmap.md 升级到 rdd-workflow v3.0+ 兼容 (嵌套 phase-N.M 语法)~~ ✅ 2026-08-18 (roadmap v3.0)
- [ ] **Phase 9 启动前与 UsrLinuxEmu Architecture Team 协调时序**(避免与 Phase 8.B 冲突)
- [ ] **Phase 9.0 启动前写 ADR-SOC-06 (C ABI 治理)、ADR-SOC-07 (BAR mmap)、ADR-SOC-08 (dGPU-first 策略)**
- [ ] **Phase 9.0 启动前评估 `cpptlm_core` STATIC→SHARED 单行改动的兼容性影响**
- [ ] **Phase 9.1 启动前评估 `include/tlm/pcie/` 子目录创建**
- [ ] Phase 7.C 启动前需更新 `scripts/README.md`(如新增 `gpu_kernel_trace_gen.py`)
- [ ] 考虑扩展 `docs_sync_check.sh` 扫描范围(Markdown 链接、include/AGENTS.md 一致性)
- [ ] Phase 7.B 启动前评估 `docs/soc_arch/specs/apu-soc-design.md` 是否需要补全
- [ ] Phase 7.F 完成后写 ADR `ADR-0022-apu-validation-methodology.md` (Phase 8 入口)
- [ ] Phase 8 启动前评估是否新增 `docs/superpowers/specs/cpptlm-validation-spec.md`
- [ ] Phase 9 完成后评估是否新增 `docs/superpowers/specs/cpptlm-emu-bridge.md`
- [ ] Phase 10 启动前写 ADR `ADR-SOC-09-apu-upgrade-from-dgpu.md`

<!-- AUTO-SPRINT-START -->
_Phase: `phase-3` · Active: 0 · Archived: 0 · Last deps: never_

| Change | Phase | Cat | Status | Blocker | Group | Conflicts | Tasks | Plan |
|--------|-------|-----|--------|---------|-------|-----------|-------|------|
| _（无 active change — 运行 `skill_use("propose", "<name>")` 添加）_ | | | | | | | | |

<!-- AUTO-SPRINT-END -->
TODO_EOF
```

- [ ] **Step 2: Verify file structure**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
grep -c "^## 待办\|^<!-- AUTO-SPRINT-" roadmap.md
wc -l roadmap.md
```

Expected: 3 matches (1 ## 待办 + 2 sentinels), ~340 lines total.

- [ ] **Step 3: Commit**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git add roadmap.md
git commit -m "feat(roadmap): add 待办 section + AUTO-SPRINT sentinels (machine-managed sprint table)"
```

---

### Task 12: Create roadmap-state.json

**Files:**
- Create: `/workspace/project/CppTLM/.worktrees/refactor-roadmap/.rddf/state/roadmap-state.json`

- [ ] **Step 1: Create directory + write state JSON**

Run:

```bash
mkdir -p /workspace/project/CppTLM/.worktrees/refactor-roadmap/.rddf/state
cat > /workspace/project/CppTLM/.worktrees/refactor-roadmap/.rddf/state/roadmap-state.json << 'STATE_EOF'
{
  "version": 1,
  "updated_at": "2026-08-18T00:00:00+00:00",
  "current_phase": "phase-3",
  "phases": {
    "phase-1": {
      "status": "pending",
      "started_at": null,
      "completed_at": null,
      "categories": {
        "protocol-bridge": {"total_changes": 0, "completed_changes": [], "changes": []}
      },
      "gate_status": {"all_changes_complete": false, "checklist": {}}
    },
    "phase-2": {
      "status": "pending",
      "started_at": null,
      "completed_at": null,
      "categories": {
        "multicluster-validation": {"total_changes": 0, "completed_changes": [], "changes": []}
      },
      "gate_status": {"all_changes_complete": false, "checklist": {}}
    },
    "phase-3": {
      "status": "in_progress",
      "started_at": "2026-06-11T00:00:00+00:00",
      "completed_at": null,
      "categories": {
        "gpu-infra": {"total_changes": 1, "completed_changes": ["phase-7a-gpu-infra"], "changes": ["phase-7a-gpu-infra"]},
        "cu-blackbox": {"total_changes": 0, "completed_changes": [], "changes": []},
        "coherence": {"total_changes": 0, "completed_changes": [], "changes": []},
        "tcc-bridge": {"total_changes": 0, "completed_changes": [], "changes": []},
        "multi-cu-noc": {"total_changes": 0, "completed_changes": [], "changes": []},
        "apu-demo": {"total_changes": 0, "completed_changes": [], "changes": []}
      },
      "gate_status": {"all_changes_complete": false, "checklist": {}}
    },
    "phase-3.1": {
      "status": "completed",
      "started_at": "2026-05-15T00:00:00+00:00",
      "completed_at": "2026-06-11T00:00:00+00:00",
      "categories": {
        "gpu-bundle": {"total_changes": 1, "completed_changes": ["phase-7a-gpu-bundle"], "changes": ["phase-7a-gpu-bundle"]},
        "gpu-init": {"total_changes": 1, "completed_changes": ["phase-7a-gpu-init"], "changes": ["phase-7a-gpu-init"]}
      },
      "gate_status": {"all_changes_complete": true, "checklist": {}}
    },
    "phase-3.2": {
      "status": "in_progress",
      "started_at": null,
      "completed_at": null,
      "categories": {
        "cu-tickloop": {"total_changes": 0, "completed_changes": [], "changes": []},
        "xbar-gpu-routing": {"total_changes": 0, "completed_changes": [], "changes": []},
        "cache-bypass": {"total_changes": 0, "completed_changes": [], "changes": []}
      },
      "gate_status": {"all_changes_complete": false, "checklist": {}}
    },
    "phase-3.3": {
      "status": "pending",
      "started_at": null,
      "completed_at": null,
      "categories": {
        "cache-state-machine": {"total_changes": 0, "completed_changes": [], "changes": []},
        "snoop-callback": {"total_changes": 0, "completed_changes": [], "changes": []},
        "lookup-home-node": {"total_changes": 0, "completed_changes": [], "changes": []}
      },
      "gate_status": {"all_changes_complete": false, "checklist": {}}
    },
    "phase-3.4": {
      "status": "pending",
      "started_at": null,
      "completed_at": null,
      "categories": {
        "tcc-dualport": {"total_changes": 0, "completed_changes": [], "changes": []},
        "memory-hbm": {"total_changes": 0, "completed_changes": [], "changes": []}
      },
      "gate_status": {"all_changes_complete": false, "checklist": {}}
    },
    "phase-3.5": {
      "status": "pending",
      "started_at": null,
      "completed_at": null,
      "categories": {
        "multi-cu-noc": {"total_changes": 0, "completed_changes": [], "changes": []},
        "cu-bidir-port": {"total_changes": 0, "completed_changes": [], "changes": []}
      },
      "gate_status": {"all_changes_complete": false, "checklist": {}}
    },
    "phase-3.6": {
      "status": "pending",
      "started_at": null,
      "completed_at": null,
      "categories": {
        "apu-demo": {"total_changes": 0, "completed_changes": [], "changes": []},
        "apu-dashboard": {"total_changes": 0, "completed_changes": [], "changes": []}
      },
      "gate_status": {"all_changes_complete": false, "checklist": {}}
    },
    "phase-4": {
      "status": "pending",
      "started_at": null,
      "completed_at": null,
      "categories": {
        "apu-validation": {"total_changes": 0, "completed_changes": [], "changes": []},
        "perf-baseline": {"total_changes": 0, "completed_changes": [], "changes": []},
        "regression": {"total_changes": 0, "completed_changes": [], "changes": []},
        "perf-report": {"total_changes": 0, "completed_changes": [], "changes": []}
      },
      "gate_status": {"all_changes_complete": false, "checklist": {}}
    },
    "phase-4.1": {"status": "pending", "started_at": null, "completed_at": null, "categories": {"trace-align": {"total_changes": 0, "completed_changes": [], "changes": []}}, "gate_status": {"all_changes_complete": false, "checklist": {}}},
    "phase-4.2": {"status": "pending", "started_at": null, "completed_at": null, "categories": {"e2e-correctness": {"total_changes": 0, "completed_changes": [], "changes": []}}, "gate_status": {"all_changes_complete": false, "checklist": {}}},
    "phase-4.3": {"status": "pending", "started_at": null, "completed_at": null, "categories": {"perf-dashboard": {"total_changes": 0, "completed_changes": [], "changes": []}}, "gate_status": {"all_changes_complete": false, "checklist": {}}},
    "phase-4.4": {"status": "pending", "started_at": null, "completed_at": null, "categories": {"regression-baseline": {"total_changes": 0, "completed_changes": [], "changes": []}}, "gate_status": {"all_changes_complete": false, "checklist": {}}},
    "phase-5": {
      "status": "in_progress",
      "started_at": "2026-08-15T00:00:00+00:00",
      "completed_at": null,
      "categories": {
        "cpptlm-infra": {"total_changes": 0, "completed_changes": [], "changes": []},
        "pcie-module": {"total_changes": 0, "completed_changes": [], "changes": []},
        "dma-computeunit": {"total_changes": 0, "completed_changes": [], "changes": []},
        "amdgpu-cabi": {"total_changes": 0, "completed_changes": [], "changes": []},
        "callback-path": {"total_changes": 0, "completed_changes": [], "changes": []},
        "nouveau-path": {"total_changes": 0, "completed_changes": [], "changes": []},
        "docs-archive": {"total_changes": 0, "completed_changes": [], "changes": []}
      },
      "gate_status": {"all_changes_complete": false, "checklist": {}}
    },
    "phase-5.0": {"status": "pending", "started_at": null, "completed_at": null, "categories": {"cpptlm-core-shared": {"total_changes": 0, "completed_changes": [], "changes": []}, "c-abi-header": {"total_changes": 0, "completed_changes": [], "changes": []}, "c-emulator-api": {"total_changes": 0, "completed_changes": [], "changes": []}}, "gate_status": {"all_changes_complete": false, "checklist": {}}},
    "phase-5.1": {"status": "pending", "started_at": null, "completed_at": null, "categories": {"pcie-host": {"total_changes": 0, "completed_changes": [], "changes": []}, "pcie-device": {"total_changes": 0, "completed_changes": [], "changes": []}, "pcie-bridge": {"total_changes": 0, "completed_changes": [], "changes": []}, "bar-mmap-hook": {"total_changes": 0, "completed_changes": [], "changes": []}}, "gate_status": {"all_changes_complete": false, "checklist": {}}},
    "phase-5.2": {"status": "pending", "started_at": null, "completed_at": null, "categories": {"dma-device": {"total_changes": 0, "completed_changes": [], "changes": []}, "computeunit-full": {"total_changes": 0, "completed_changes": [], "changes": []}, "memory-hbm-mode": {"total_changes": 0, "completed_changes": [], "changes": []}}, "gate_status": {"all_changes_complete": false, "checklist": {}}},
    "phase-5.3": {"status": "pending", "started_at": null, "completed_at": null, "categories": {"hal-dispatch": {"total_changes": 0, "completed_changes": [], "changes": []}, "amdgpu-abi-tests": {"total_changes": 0, "completed_changes": [], "changes": []}}, "gate_status": {"all_changes_complete": false, "checklist": {}}},
    "phase-5.4": {"status": "pending", "started_at": null, "completed_at": null, "categories": {"callback-impl": {"total_changes": 0, "completed_changes": [], "changes": []}, "workqueue-dispatch": {"total_changes": 0, "completed_changes": [], "changes": []}}, "gate_status": {"all_changes_complete": false, "checklist": {}}},
    "phase-5.5": {"status": "pending", "started_at": null, "completed_at": null, "categories": {"nouveau-cabi": {"total_changes": 0, "completed_changes": [], "changes": []}}, "gate_status": {"all_changes_complete": false, "checklist": {}}},
    "phase-5.6": {"status": "pending", "started_at": null, "completed_at": null, "categories": {"emu-bridge-spec": {"total_changes": 0, "completed_changes": [], "changes": []}, "adr-archive": {"total_changes": 0, "completed_changes": [], "changes": []}}, "gate_status": {"all_changes_complete": false, "checklist": {}}}
  }
}
STATE_EOF
```

- [ ] **Step 2: Verify JSON valid**

```bash
python3 -c "import json; print(json.load(open('/workspace/project/CppTLM/.worktrees/refactor-roadmap/.rddf/state/roadmap-state.json'))['current_phase'])"
```

Expected: `phase-3`.

- [ ] **Step 3: Verify with rdd-doctor**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
bash /workspace/project/rdd-workflow/skills/rdd-doctor/scripts/doctor.sh
```

Expected: `✅ All 6 categories OK` (state / plan-tdd / roadmap-meta / proposal-table / tasks-checkbox / migration-residue).

- [ ] **Step 4: Commit**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git add .rddf/state/roadmap-state.json
git commit -m "feat(state): initialize roadmap-state.json with phase-3 in_progress + phase-3.1 completed"
```

---

## Phase C: OpenSpec Change Meta Files

### Task 13: Add roadmap-meta.yaml to cpptlm-d1-p1-pipeline-scoreboard

**Files:**
- Create: `/workspace/project/CppTLM/.worktrees/refactor-roadmap/openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/roadmap-meta.yaml`

- [ ] **Step 1: Create meta file**

Run:

```bash
cat > /workspace/project/CppTLM/.worktrees/refactor-roadmap/openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/roadmap-meta.yaml << 'META_EOF'
# Roadmap metadata for cpptlm-d1-p1-pipeline-scoreboard
# Maps this change to a specific phase-N.M sub-phase in /workspace/project/CppTLM/roadmap.md

roadmap:
  phase: phase-3.2           # 7.B ComputeUnit 黑盒
  category: cu-tickloop      # ComputeUnitTLM tick loop (Scoreboard/Pipeline/TensorCore are ComputeUnit sub-features)
  priority: P0
  expected_themes:
    - ComputeUnitTLM tick loop
    - inflight_kernel_reqs_
    - D1-Full Compute injection
META_EOF
```

- [ ] **Step 2: Verify file**

```bash
cat /workspace/project/CppTLM/.worktrees/refactor-roadmap/openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/roadmap-meta.yaml
```

Expected: 9-line YAML with phase/category fields.

- [ ] **Step 3: Commit**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git add openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/roadmap-meta.yaml
git commit -m "feat(meta): add roadmap-meta.yaml for cpptlm-d1-p1-pipeline-scoreboard → phase-3.2/cu-tickloop"
```

---

### Task 14: Add roadmap-meta.yaml to 2026-06-24-gpu-soc-phase8b-core

**Files:**
- Create: `/workspace/project/CppTLM/.worktrees/refactor-roadmap/openspec/changes/2026-06-24-gpu-soc-phase8b-core/roadmap-meta.yaml`

- [ ] **Step 1: Create meta file**

Run:

```bash
cat > /workspace/project/CppTLM/.worktrees/refactor-roadmap/openspec/changes/2026-06-24-gpu-soc-phase8b-core/roadmap-meta.yaml << 'META_EOF'
# Roadmap metadata for 2026-06-24-gpu-soc-phase8b-core
# Original change was Phase 8.B 核心仿真; mapped to phase-3.2 (cu-blackbox) in new model.

roadmap:
  phase: phase-3.2           # 7.B ComputeUnit 黑盒 (Phase 8.B 核心仿真时代)
  category: cu-tickloop      # ComputeUnitTLM tick loop implementation
  priority: P0
  expected_themes:
    - ComputeUnitTLM core
    - 8.B 核心仿真 (legacy Phase 8 model)
    - test_gpu_soc_phase8b.cc 集成
META_EOF
```

- [ ] **Step 2: Verify**

```bash
cat /workspace/project/CppTLM/.worktrees/refactor-roadmap/openspec/changes/2026-06-24-gpu-soc-phase8b-core/roadmap-meta.yaml
```

Expected: 11-line YAML.

- [ ] **Step 3: Commit**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git add openspec/changes/2026-06-24-gpu-soc-phase8b-core/roadmap-meta.yaml
git commit -m "feat(meta): add roadmap-meta.yaml for 2026-06-24-gpu-soc-phase8b-core → phase-3.2/cu-tickloop"
```

---

### Task 15: Add roadmap-meta.yaml to 2026-06-24-gpu-soc-phase8c-advanced

**Files:**
- Create: `/workspace/project/CppTLM/.worktrees/refactor-roadmap/openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/roadmap-meta.yaml`

- [ ] **Step 1: Create meta file**

Run:

```bash
cat > /workspace/project/CppTLM/.worktrees/refactor-roadmap/openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/roadmap-meta.yaml << 'META_EOF'
# Roadmap metadata for 2026-06-24-gpu-soc-phase8c-advanced
# Original change was Phase 8.C 高级特性; mapped to phase-3.5 (multi-cu-noc) in new model.

roadmap:
  phase: phase-3.5           # 7.E Multi-CU + NoC (Phase 8.C 高级特性时代)
  category: multi-cu-noc     # Multi-CU 数组 + NoC
  priority: P0
  expected_themes:
    - ComputeUnitTLM 数组模式
    - 8.C 高级特性 (legacy Phase 8 model)
    - test_gpu_soc_phase8c.cc apu_soc 集成
META_EOF
```

- [ ] **Step 2: Verify**

```bash
cat /workspace/project/CppTLM/.worktrees/refactor-roadmap/openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/roadmap-meta.yaml
```

Expected: 11-line YAML.

- [ ] **Step 3: Commit**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git add openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/roadmap-meta.yaml
git commit -m "feat(meta): add roadmap-meta.yaml for 2026-06-24-gpu-soc-phase8c-advanced → phase-3.5/multi-cu-noc"
```

---

### Task 16: Verify All 3 Changes Validate Against New Roadmap

**Files:** (verification only)

- [ ] **Step 1: Test change 1 validation**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
python3 -c "
import sys
sys.path.insert(0, '/workspace/project/rdd-workflow/.worktrees/refactor-nested-phase')
from skills._lib import roadmap_state
import os
os.chdir('/workspace/project/CppTLM/.worktrees/refactor-roadmap')
rc = roadmap_state.validate_change(
    'roadmap.md',
    'openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/roadmap-meta.yaml',
    'cpptlm-d1-p1-pipeline-scoreboard'
)
sys.exit(rc)
"
echo "Exit code: $?"
```

Expected: Exit code 0.

- [ ] **Step 2: Test change 2 validation**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
python3 -c "
import sys
sys.path.insert(0, '/workspace/project/rdd-workflow/.worktrees/refactor-nested-phase')
from skills._lib import roadmap_state
import os
os.chdir('/workspace/project/CppTLM/.worktrees/refactor-roadmap')
rc = roadmap_state.validate_change(
    'roadmap.md',
    'openspec/changes/2026-06-24-gpu-soc-phase8b-core/roadmap-meta.yaml',
    '2026-06-24-gpu-soc-phase8b-core'
)
sys.exit(rc)
"
echo "Exit code: $?"
```

Expected: Exit code 0.

- [ ] **Step 3: Test change 3 validation**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
python3 -c "
import sys
sys.path.insert(0, '/workspace/project/rdd-workflow/.worktrees/refactor-nested-phase')
from skills._lib import roadmap_state
import os
os.chdir('/workspace/project/CppTLM/.worktrees/refactor-roadmap')
rc = roadmap_state.validate_change(
    'roadmap.md',
    'openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/roadmap-meta.yaml',
    '2026-06-24-gpu-soc-phase8c-advanced'
)
sys.exit(rc)
"
echo "Exit code: $?"
```

Expected: Exit code 0.

- [ ] **Step 4: Run rdd-doctor final check**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
bash /workspace/project/rdd-workflow/skills/rdd-doctor/scripts/doctor.sh
```

Expected: `✅ All 6 categories OK` (state now has roadmap-state.json; tasks-checkbox may show 0/0 for new tasks which is expected).

---

## Phase D: Cross-Repo Verification

### Task 17: Cross-Repo Broken Link Check

**Files:** (verification only)

- [ ] **Step 1: Check for anchor links to old roadmap**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
# Check AGENTS.md, README.md, ONBOARDING.md, docs/superpowers/specs/ for roadmap.md# links
grep -rn "roadmap.md#" --include="*.md" AGENTS.md README.md docs/ 2>/dev/null || echo "No anchor links found"
```

Expected: Either "No anchor links found" OR a list of links to verify manually.

- [ ] **Step 2: If anchor links found, verify each still works**

For each link found:
1. Extract anchor (e.g., `roadmap.md#phase-7`)
2. Run: `grep -n "^### Phase 7\|^### 7\." roadmap.md | head -3`
3. Verify the heading still exists

If any link is broken, the new GitHub anchor (generated from heading text) is different. Add a note to commit:

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
# Document any breaking anchor changes in CHANGELOG or commit message
echo "## Roadmap anchor changes (v2.3 → v3.0)" >> docs/superpowers/specs/2026-08-18-roadmap-rdd-workflow-refactor-design.md
# ... (add details if needed)
```

- [ ] **Step 3: Check cross-repo UsrLinuxEmu ADR-088 link**

```bash
# Verify GitHub URL is valid (may fail in sandbox; if so, skip)
curl -sI https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-cpptlm-emu-bridge.md 2>&1 | head -1
```

Expected: `HTTP/2 200` or `HTTP/2 404` (404 means ADR may have moved; record for follow-up). In sandbox: may fail entirely — that's OK.

- [ ] **Step 4: No commit needed if no broken links; otherwise amend a commit**

If any anchor changes need to be documented, commit:

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git add docs/superpowers/specs/2026-08-18-roadmap-rdd-workflow-refactor-design.md
git commit -m "docs: document any anchor link migrations from roadmap v2.3 → v3.0"
```

---

### Task 18: AUTO-SPRINT Sentinel Test

**Files:** (verification only)

- [ ] **Step 1: Snapshot roadmap.md before sprint update**

```bash
cp /workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md /tmp/roadmap.before.md
```

- [ ] **Step 2: Run roadmap_sprint.update_roadmap**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
python3 -c "
import sys
sys.path.insert(0, '/workspace/project/rdd-workflow/.worktrees/refactor-nested-phase')
from skills._lib.roadmap_sprint import update_roadmap
update_roadmap('roadmap.md', {'changes': [], 'current_phase': 'phase-3'})
print('update_roadmap succeeded')
"
```

Expected: `update_roadmap succeeded`.

- [ ] **Step 3: Verify sentinel-外 content unchanged**

```bash
# Strip sentinel + inner sprint table from both files; diff must be empty
diff <(grep -v -E '<!-- AUTO-SPRINT-(START|END) -->|^_Phase:|^| Change |^|--------|^| _（无|^_🗄️' /tmp/roadmap.before.md) \
     <(grep -v -E '<!-- AUTO-SPRINT-(START|END) -->|^_Phase:|^| Change |^|--------|^| _（无|^_🗄️' /workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md)
echo "Diff exit: $?"
```

Expected: No diff output, exit 0.

- [ ] **Step 4: Verify sentinel-内 sprint table updated**

```bash
grep -A 3 "AUTO-SPRINT-START" /workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md | head -5
```

Expected: Shows `_Phase: \`phase-3\` · Active: 0 · Archived: 0 · Last deps: never_` line.

- [ ] **Step 5: No commit needed (sprint table is auto-managed)**

If you want to commit the initial sprint table:

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git add roadmap.md
git commit -m "chore(roadmap): initialize AUTO-SPRINT table for phase-3"
```

---

### Task 19: Final Verification + Push Branches

**Files:** (verification only)

- [ ] **Step 1: rdd-workflow side: all tests pass**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
python -m pytest tests/unit/test_roadmap_state.py -v 2>&1 | tail -10
```

Expected: All PASS.

- [ ] **Step 2: CppTLM side: rdd-doctor clean**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
bash /workspace/project/rdd-workflow/skills/rdd-doctor/scripts/doctor.sh
```

Expected: `✅ All 6 categories OK`.

- [ ] **Step 3: CppTLM side: render_status_view works**

```bash
cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
python3 -c "
import sys
sys.path.insert(0, '/workspace/project/rdd-workflow/.worktrees/refactor-nested-phase')
from skills._lib import roadmap_state
roadmap_state.render_status_view('roadmap.md', '.rddf/state/roadmap-state.json')
"
```

Expected: Output showing phase-3 in_progress, phase-3.1 completed, other phases pending.

- [ ] **Step 4: Push both branches**

```bash
cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase
git push -u origin feature/roadmap-nested-phase 2>&1 | head -3 || echo "(no remote — branch is local-only)"

cd /workspace/project/CppTLM/.worktrees/refactor-roadmap
git push -u origin feature/roadmap-rdd-workflow-compat 2>&1 | head -3 || echo "(no remote — branch is local-only)"
```

Expected: Either push success OR "no remote" note (both acceptable).

- [ ] **Step 5: Print final summary**

```bash
echo "=== Refactor Summary ==="
echo "rdd-workflow: $(cd /workspace/project/rdd-workflow/.worktrees/refactor-nested-phase && git log --oneline -5)"
echo "CppTLM:       $(cd /workspace/project/CppTLM/.worktrees/refactor-roadmap && git log --oneline -10)"
echo "Old roadmap:  tagged as roadmap-v2.3 in CppTLM main"
echo "New roadmap:  /workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md ($(wc -l < /workspace/project/CppTLM/.worktrees/refactor-roadmap/roadmap.md) lines)"
```

---

## Self-Review Checklist (Run Before Reporting Completion)

- [ ] All 19 tasks executed in order
- [ ] Every commit message follows `<type>(<scope>): <subject>` format
- [ ] `rdd-workflow/tests/unit/test_roadmap_state.py` has 3 new tests (regex constants + 2 advance_phase aggregation)
- [ ] `rdd-workflow/skills/_lib/roadmap_state.py` has 3 new constants + 2 modified functions
- [ ] `rdd-workflow/skills/roadmap/SKILL.md` has new "嵌套阶段语法" section
- [ ] `CppTLM/roadmap.md` rewritten to ~340 lines with 5 parent phases + 17 sub-phases
- [ ] `CppTLM/.rddf/state/roadmap-state.json` exists with 22 phase entries
- [ ] 3 openspec/changes each have `roadmap-meta.yaml` pointing to valid phase-N.M/category
- [ ] `rdd-doctor --json` returns 0 findings
- [ ] 3 change validations return exit code 0
- [ ] AUTO-SPRINT sentinel test passes (no diff in sentinel-外 content)

## Execution Handoff

Plan complete and saved to `/workspace/project/CppTLM/docs/superpowers/plans/2026-08-18-roadmap-rdd-workflow-refactor.md`.

**Two execution options:**

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration
2. **Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
