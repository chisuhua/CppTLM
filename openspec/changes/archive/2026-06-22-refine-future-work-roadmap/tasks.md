# Tasks: Refine Future-Work Roadmap

> **Type**: Documentation refactor
> **Status**: ✅ Completed (executed in commit `fbed86f`, 2026-06-22)
> **Verification**: 690/690 tests pass (15330 assertions) · 364/364 docs_sync paths valid

## Task 1: Apply corrections to roadmap

**Files**: `docs/superpowers/plans/2026-06-20-future-work-roadmap.md`

### 1.1 Rewrite F11 (remove P0-#4 false positive)

- [x] Remove P0-#4 description from F11 background section
- [x] Add "⚠️ P0-#4 假阳性说明" subsection citing `tlm_stub.hh:155-168` (Phase 1d fix) and Oracle finding
- [x] Rename F11 from "Packet::reset() 显式 release Extension" to "新风险#1: E2E 回归守护"
- [x] Keep only 新风险#1 implementation (delete test_phase6 direct driving + add E2E test)
- [x] Update F11 priority: 🟢 → 🟢→🔴 (elevated alongside F4 in §1.1 per risk severity judgment)

### 1.2 Correct F6 dependency

- [x] Change F6 dependency from "F4 部分" to "F12 (Phase 7.B class implementation)"
- [x] Update F6 detailed section to clarify F4 is orthogonal (crossbar-side vs CU-side)

### 1.3 Rebalance F4 priority

- [x] Move F4 from 🟡 (中优先级) to 🔴 (高优先级, 最高风险) — align with `roadmap.md` L75
- [x] Update F4 detail to mention "🔴" risk label (now in §1.1)

### 1.4 Add F12-F15 (Phase 7.B/D/E/F coverage)

- [x] Add F12: Phase 7.B 三类 (GpuComputeUnitTLM / VectorRegFileTLM / WavefrontTLM)
- [x] Add F13: Phase 7.D TCC Bridge + HBM
- [x] Add F14: Phase 7.E Multi-CU + NoC + BidirectionalPortAdapter
- [x] Add F15: Phase 7.F Full APU Demo (apu_full_soc.json + test_apu_soc.py)
- [x] Update §2 to add Phase C (7.D + 7.E) and Phase D (7.F + 按需扩展)

### 1.5 Add §0 test baseline verification

- [x] Add bash verification command: `./build/bin/cpptlm_tests --reporter compact`
- [x] Document current verified baseline: 690/690 (15330 assertions, 2026-06-22)
- [x] Note that AGENTS.md 659/659 is outdated

### 1.6 Add §0.5 dependency graph + total effort summary

- [x] Add total effort table: 15 tasks × estimated days → ~51 working days (~10 weeks single-person)
- [x] Add Mermaid-style ASCII DAG showing critical path (F12 → F13 → F14 → F15) vs fast path (F2 → F3 → F1)
- [x] Mark critical path 32 working days, fast path 3.5 working days

### 1.7 Minor corrections

- [x] F1.P2.5: narrow to "summary structure exists" (not specific hit rate values)
- [x] F7: add pre-check `grep -rn '"ports"\s*:' configs/` step
- [x] §5 ADR list: add 7B-01, 7D-01, 7E-01, 7F-01, METRIC-01 (5 new ADRs)
- [x] §2 add Phase D: Phase 7.F integration + 按需扩展 (F15 + F9 + F10 + F7)

## Task 2: Create OpenSpec change

**Files**: `openspec/changes/2026-06-22-refine-future-work-roadmap/`

- [x] Create directory
- [x] Write `proposal.md` (Why / What / Impact / Non-Goals / Acceptance / References)
- [x] Write `tasks.md` (this file)
- [x] Verified no `openspec/changes/archive/` entry needed at creation time (deferred to post-merge)

## Task 3: Verify

- [x] Run `scripts/test/docs_sync_check.sh --strict` — **364/364 paths valid, 0 missing** (2026-06-22 verification)
- [x] Run `./build/bin/cpptlm_tests --reporter compact` — **690/690 pass (15330 assertions)** (2026-06-22 verification)
- [x] Read final roadmap to confirm scannability (588 lines, well-structured §0-7)
- [x] Check that F11 priority change is reflected in §1 table AND §2 Phase A order (both show F11 in 🔴 §1.1 + Phase A position)

## Task 4: Commit

- [x] Stage: `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` + `openspec/changes/2026-06-22-refine-future-work-roadmap/`
- [x] Commit: `fbed86f docs(roadmap): apply Oracle review findings + 7 corrections to future-work-roadmap`
- [x] Body: 7 corrections + Oracle session `ses_1114136baffeCDf3lo6G0MFspz`
- [x] Push to origin/main — *pending user authorization* (branch ahead by 1 commit, ready to push)

## Task 5: Archive OpenSpec change (post-execution)

- [x] Update `tasks.md` checkboxes to reflect completion (this commit)
- [x] Move `openspec/changes/2026-06-22-refine-future-work-roadmap/` → `openspec/changes/archive/2026-06-22-refine-future-work-roadmap/`
- [x] Stage the rename
- [x] Commit archive operation: `docs(openspec): archive 2026-06-22-refine-future-work-roadmap` (commit `f3c6067`, 2026-06-22)

## Notes

- This is a documentation-only refactor. No code changes.
- The openspec change tracks the refactor for future traceability.
- After commit, the future-work-roadmap is the single source of truth for F1-F15.
- The openspec change is archived after merge (move to `openspec/changes/archive/`).
