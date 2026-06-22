# Refine Future-Work Roadmap (Apply Oracle Review)

**Change ID**: `2026-06-22-refine-future-work-roadmap`
**Author**: Sisyphus
**Created**: 2026-06-22
**Status**: Proposed
**Scope**: Documentation only (no code changes)

## Why

The active planning document at `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` was created on 2026-06-20 to consolidate P2+ follow-up work after P0+P1+P1.5 completion. After its initial publication, Sisyphus dispatched Oracle (high-IQ reasoning model) to validate the roadmap.

Oracle identified **3 critical issues** that need to be fixed before the roadmap is used as a basis for work dispatch:

1. **F11 P0-#4 is a false positive** — The 2026-06-09 audit it cites is stale. `tlm_generic_payload::reset()` (Phase 1d, `include/tlm/tlm_stub.hh:155-168`) **already** loop-deletes + nullifies all extensions when `Packet::reset()` calls it. The proposed fix (`payload->release_extension<>()` or `reset_extensions()`) would either cause double-delete or reference a non-existent API.

2. **Phase 7.D / 7.E / 7.F completely missing** — The roadmap claims "complete P2+ index" but `roadmap.md` L76-78 lists Phase 7.D (TCC Bridge), 7.E (Multi-CU+NoC), 7.F (Full APU Demo) as pending, and the future-work roadmap has no entries for them.

3. **F6 → F4 dependency is incorrect** — F6 (compute_unit blueprint upgrade) actually depends on Phase 7.B (GpuComputeUnitTLM/VectorRegFileTLM/WavefrontTLM class implementation), not on F4 (CoherenceXBarTLM state table). F4 and F6 are orthogonal (crossbar-side vs CU-side).

Additionally, **4 minor issues**:

4. **Priority inconsistency** — F4 is 🟡 in future-work-roadmap but `roadmap.md` L75 explicitly says "🔴 High (最高风险)". F11 🟢 (memory safety + regression guard) is more critical than F8 🟢 (cosmetic).

5. **Test baseline drift** — Roadmap shows 684/684, 690/690; AGENTS.md shows 659/659. No single source of truth documented.

6. **F1.P2.5 ↔ F10 overlap** — F1's "performance metric assertion" depends on `metrics_reporter` having data, but F10 is the task that adds collection. Without F10, F1.P2.5 can only assert structure (not specific hit rate values).

7. **F7 missing pre-check step** — `params.ports: 8` schema validation may break existing `apu_soc_v1.json` (silent ignore → hard error). Need to grep configs/ first to assess impact before implementing.

## What Changes

Apply all 7 corrections to `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` in a single commit:

1. **F11 rewrite**: Remove P0-#4 (false positive), keep only 新风险#1 (E2E regression guard). Document the false positive finding (Phase 1d fix) inline.

2. **F6 dependency correction**: "F4 部分" → "F12 (Phase 7.B class implementation)".

3. **F4 priority**: 🟡 → 🔴 (align with `roadmap.md` risk grading).

4. **F11 priority**: 🟢 → 🟡 (memory safety + regression guard outranks cosmetic F8).

5. **Add F12-F15**: New entries for Phase 7.B / 7.D / 7.E / 7.F to cover the full Phase 7 spectrum (previously roadmap claimed completeness but only covered 7.B-partial + 7.C).

6. **§0 test baseline**: Add verification command and single source of truth statement (690/690 verified, AGENTS.md 659/659 is outdated).

7. **§0.5 new section**: Add dependency graph (Mermaid-style ASCII DAG) + total effort summary table (~51 working days / 10 weeks single-person).

Additionally as part of corrections:
- **F1.P2.5 narrow**: "hit rate value" → "summary structure exists" (F10 collects, F1 asserts)
- **F7 add pre-check**: `grep -rn '"ports"\s*:' configs/` step before implementation
- **§5 ADR list**: Add 7B-01, 7D-01, 7E-01, 7F-01, METRIC-01 (5 new ADRs)
- **§2 new Phase D**: Phase 7.F integration + 按需扩展 (F15 + F9 + F10 + F7)

## Impact

- **Active planning doc improved**: Roadmap becomes accurate enough to dispatch F2/F3/F11/F1 (Phase A) without risk of wasted work (no false-positive tasks) and provides a complete Phase 7 roadmap (7.B-7.F all covered).
- **No code changes**: This change is documentation only. The roadmap content is the artifact, not code.
- **Test baseline verified**: 690/690 (15330 assertions) confirmed via `./build/bin/cpptlm_tests --reporter compact`.

## Non-Goals

- This change does NOT execute any of the F1-F15 tasks. Each F# remains a future work item.
- This change does NOT modify the actual `roadmap.md` (project root) — only the future-work-roadmap in `docs/superpowers/plans/`.
- This change does NOT fix any of the items in F1-F15. The F11 false positive finding is documented in the roadmap for traceability, but no code change is made to `include/core/packet.hh` (Phase 1d already fixed the underlying concern).

## Acceptance Criteria

- [ ] `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` reflects all 7 corrections
- [ ] No false-positive tasks remain (F11 P0-#4 removed; Phase 1d fix cited)
- [ ] Phase 7.A-F all represented (7.A done, 7.B-F12, 7.C-F4, 7.D-F13, 7.E-F14, 7.F-F15)
- [ ] F6 dependency points to F12, not F4
- [ ] Priority table consistent with `roadmap.md` risk grading
- [ ] §0 has test baseline verification command
- [ ] §0.5 has dependency graph + effort summary
- [ ] §2 has Phase A/B/C/D execution order
- [ ] §3 has F11/F12/F13/F14/F15 detailed sections
- [ ] §5 has all 10 ADRs (including 5 new)
- [ ] `scripts/test/docs_sync_check.sh --strict` passes (0 missing paths)
- [ ] `./build/bin/cpptlm_tests --reporter compact` still shows 690/690

## References

- Original roadmap: `docs/superpowers/plans/2026-06-20-future-work-roadmap.md`
- Oracle consultation session: `ses_1114136baffeCDf3lo6G0MFspz`
- Project root roadmap: `roadmap.md`
- Archived P0 alignment audit: `docs-archived/plans/p0-alignment-remediation-plan.md`
- P0 fix commit (Phase 1d): `fb56cc3` (and successors)
- Handoff: `docs/superpowers/handoffs/2026-06-19-p0-discussion-handoff.md`
