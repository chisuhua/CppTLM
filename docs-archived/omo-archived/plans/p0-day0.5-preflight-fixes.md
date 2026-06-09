# Day 0.5: Pre-flight Fixes for P0 Alignment Remediation

> **TL;DR**: Patching 8 blocking gaps in `plans/p0-alignment-remediation-plan.md` before the main Sprint starts. Each task fixes one gap that would cause the original Sprint to fail (compile error, test regression, or incomplete documentation alignment). Must finish in ≤4 hours before Day 1 of the original Sprint.

> **Deliverables**:
> - Fixed namespace qualifier in original T3.3.2 patch (`tlm::ArbiterTLM` → `ArbiterTLM`)
> - 6 downstream files updated to lowercase PortRole/BundleType strings (aligns with original T1.1)
> - `[PORT INFO]` → `[WARN]` in `module_factory.cc:79`
> - v2.1 §6 cross-references to ADR-X.1/6/9 + hybrid_v4
> - v4 doc test count claim corrected (14/14 → 17/17)
> - Hardcoded "593" replaced with dynamic grep pattern in original plan
> - 5 `ch_bool(true/false)` literals replaced with `ch_reg<ch_bool>` in hybrid_cache_component.cc
> - AGENTS.md test file count updated (43 → 75)

> **Estimated Effort**: 4 hours (hard cap)
> **Parallel Execution**: YES - Wave 0.5a (5 tasks) + Wave 0.5b (4 tasks)
> **Critical Path**: T0.1 (enumeration) → T0.2 (string replacement) → T0.9 (verify)

---

## Context

### Original Request
Review `plans/p0-alignment-remediation-plan.md` (a 7-day Sprint plan fixing 4 P0 issues + 2 new risks in CppTLM) and check if it covers all documented issues. Found 8 gaps that would cause the Sprint to fail.

### Interview Summary
**Key Discussions**:
- **Gap 1 (compile bug)**: T3.3.2 uses `tlm::ArbiterTLM<2>` but the class is in global namespace — will fail to compile
- **Gap 2 (downstream impact)**: P0-#1 only modifies `port_types.hh`; 6 files with 35+ uppercase strings will break
- **Gap 3 (log level)**: `module_factory.cc:79` uses `[PORT INFO]` but ADR-X.9 §决策 1.5 requires `[WARN]`
- **Gap 4 (cross-refs)**: v2.1 §6 missing ADR-X.1/6/9 and hybrid_v4 links
- **Gap 5 (test count)**: v4 doc claims 14/14 but actual code has 17 TEST_CASEs
- **Gap 6 (hardcoded count)**: Original plan's PC-4 uses hardcoded "593" instead of dynamic command
- **Gap 7 (ch_bool literals)**: hybrid_cache_component.cc has 5 `ch_bool(true/false)` lines v3 already fixed
- **Gap 8 (AGENTS.md stale)**: Claims 43 test files; actual count is 75

### Scope Boundaries

**MUST Fix** (this plan):
| # | File(s) | Change | Reason |
| :-: | :--- | :--- | :--- |
| 1 | `plans/p0-alignment-remediation-plan.md` | Fix `tlm::ArbiterTLM` → `ArbiterTLM` | Blocking compile |
| 2 | 6 downstream files | Lowercase PortRole/BundleType strings | Blocking test pass |
| 3 | `src/core/module_factory.cc:79` | `[PORT INFO]` → `[WARN]` | ADR compliance |
| 4 | `docs/architecture/01-hybrid-architecture-v2.1.md` §6 | Add ADR-X.1/6/9 + hybrid_v4 xrefs | Doc traceability |
| 5 | `docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v4.md` | Test count 14/14 → 17/17 | Accuracy |
| 6 | `plans/p0-alignment-remediation-plan.md` | "593" → `cpptlm_tests --list-tests \| wc -l` | Maintainability |
| 7 | `include/rtl/hybrid_cache_component.cc` | ch_bool(true/false) → ch_reg\<ch_bool\> | SEGV prevention |
| 8 | `AGENTS.md` in root | 43 → 75 test files | Accuracy |

**MUST NOT Fix** (out of scope):
- Full rewrite of the original plan
- TLMModule/RTLModule/ImplMode/ErrorContextExt/Packet dead fields (用户标注 已记录)
- Any logic changes beyond the gap fixes
- New tests, configs, dependencies, or scripts

---

## Verification Strategy

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No exceptions.

### Test Decision
- **Infrastructure exists**: YES
- **Automated tests**: YES (after implementation — each task verified by compile + targeted grep/test)
- **Framework**: Catch2 v3.7.0

### QA Policy
Each task MUST have agent-executed verification (see TODO template). Evidence saved to `build/logs/day0_g{N}.log`.

- **Compile check**: `cmake --build build 2>&1 | grep -i error`
- **String check**: `grep -rE '"(MASTER|SLAVE|INITIATOR|TARGET)"' include/ src/ test/`
- **Test count**: `grep -c "TEST_CASE" test/test_fragment_mapper.cc`
- **Log level**: `grep -nE "\[PORT INFO\]|\[WARN\]" src/core/module_factory.cc`

---

## Execution Strategy

```
Wave 0.5a (Start Immediately — enumeration + independent fixes):
├── T0.1: Enumerate Gap 2 downstream files [quick]
├── T0.3: Fix Gap 1 — namespace qualifier [quick]
├── T0.4: Fix Gap 3 — log level [quick]
├── T0.5: Fix Gap 5 — test count sync [quick]
└── T0.6: Fix Gap 6 — hardcoded count [quick]

Wave 0.5b (After Wave 0.5a — mechanical edits, depends on T0.1):
├── T0.2: Fix Gap 2 — 6 downstream files (depends: T0.1) [quick]
├── T0.7: Fix Gap 7 — ch_bool literals [quick]
├── T0.8: Fix Gap 8 — AGENTS.md count [quick]
└── T0.9: Fix Gap 4 — v2.1 §6 cross-refs [quick]

Wave FINAL (After ALL tasks):
├── F1: Compile check (cmake --build)
├── F2: cpptlm_tests baseline confirm
└── F3: grep sweep for remaining uppercase strings
```

### Dependency Matrix

- **T0.1**: None → T0.2
- **T0.2**: T0.1 → T0.9
- **T0.3**: None → F1
- **T0.4**: None → F1
- **T0.5**: None → T0.9
- **T0.6**: None → F1
- **T0.7**: None → F1
- **T0.8**: None → T0.9
- **T0.9**: T0.2, T0.5, T0.8 → F1
- **F1-F3**: All → done

### Agent Dispatch Summary

- **Wave 0.5a**: 5 tasks → `quick`
- **Wave 0.5b**: 4 tasks → `quick`
- **FINAL**: 3 tasks → `unspecified-low` (verification only)

---

## TODOs

- [x] 1. **T0.1 — Enumerate Gap 2 downstream files** (⏱️ 15 min)

  **What to do**:
  - Run `lsp_find_references` or grep on all uppercase PortRole/BundleType strings to enumerate every downstream file, not just the 6 known ones
  - For each file found: record file path, line numbers, state whether it's a string literal or enum value
  - Output a machine-readable list to `build/logs/day0_gap2_enumeration.txt`

  **Must NOT do**:
  - Modify any files in this task (enumeration only)
  - Skip `lsp_find_references` (grep is fallback only)

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: [] (no specialized skills needed)

  **Parallelization**:
  - **Can Run In Parallel**: YES — Wave 0.5a
  - **Blocks**: T0.2

  **References**:
  - `src/core/port_compatibility.cc:69-73`
  - `src/core/module_factory_validate.cc:334-335`
  - `test/test_nic_port_groups.cc:37,40,41,48,49,83,105`
  - `test/test_phase3_2_port_management.cc:209-223,250-262`
  - `test/test_port_types.cc:222-226`
  - `configs/test/nic_tlm_port_groups.json:14,17,18,25,26`

  **Acceptance Criteria**:
  - [ ] Enumeration file created at `build/logs/day0_gap2_enumeration.txt`
  - [ ] Each hit has file:line format, not just file counts
  - [ ] All 6 known files present in output

  **Evidence to Capture**:
  - [ ] `build/logs/day0_gap2_enumeration.txt`

  **Commit**: NO (enumeration artifact only)


- [x] 2. **T0.3 — Fix Gap 1: namespace qualifier in original plan patch** (⏱️ 10 min)

  **What to do**:
  - Edit `plans/p0-alignment-remediation-plan.md` section T3.3.2
  - Find every occurrence of `tlm::ArbiterTLM<2>` and `tlm::ArbiterTLM<4>`
  - Remove the `tlm::` namespace qualifier — change to `ArbiterTLM<2>` / `ArbiterTLM<4>` (no prefix; class is in GLOBAL namespace per arbiter_tlm.hh:14)
  - Also verify `tlm::RouterTLM` is correct (RouterTLM IS in `namespace tlm {}` — keep that one)

  **Must NOT do**:
  - Remove `tlm::` from `tlm::RouterTLM` (that one IS correct)
  - Change any other namespace qualifiers in the plan

  **Recommended Agent Profile**:
  - **Category**: `quick`

  **Parallelization**:
  - **Can Run In Parallel**: YES — Wave 0.5a
  - **Blocks**: None
  - **Blocked By**: None

  **References**:
  - `include/tlm/arbiter_tlm.hh:14` — `template<unsigned N_PORTS> class ArbiterTLM` (GLOBAL namespace, no `tlm::`)
  - `include/tlm/router_tlm.hh` — RouterTLM IS in `namespace tlm {}` block
  - `plans/p0-alignment-remediation-plan.md` T3.3.2 — current patch with wrong qualifier

  **Acceptance Criteria**:
  - [ ] `grep -n "tlm::ArbiterTLM" plans/p0-alignment-remediation-plan.md` returns empty (no wrong qualifiers)
  - [ ] `grep -n "tlm::RouterTLM" plans/p0-alignment-remediation-plan.md` still matches (correct one preserved)
  - [ ] `grep -n "ArbiterTLM" plans/p0-alignment-remediation-plan.md` shows matches with NO `tlm::` prefix and NO `cpptlm::` prefix (bare `ArbiterTLM` only)

  **Evidence to Capture**:
  - [ ] `build/logs/day0_g1_ns_check.log`

  **Commit**: YES — `fix: correct namespace qualifier for ArbiterTLM in original plan (Gap 1)`


- [x] 3. **T0.4 — Fix Gap 3: log level [PORT INFO] → [WARN]** (⏱️ 15 min)

  **What to do**:
  - Read `src/core/module_factory.cc` around line 79
  - Change `DPRINTF(CONN, "[PORT INFO] ..."` to `DPRINTF(CONN, "[WARN] ..."`
  - Also check if `[PORT INFO]` appears anywhere else in `module_factory.cc` (if only line 79, scope is clean)

  **Must NOT do**:
  - Change any other log level in the codebase (single-line change only)
  - Invent a new logging macro

  **Recommended Agent Profile**:
  - **Category**: `quick`

  **Parallelization**:
  - **Can Run In Parallel**: YES — Wave 0.5a
  - **Blocks**: None
  - **Blocked By**: None

  **References**:
  - `src/core/module_factory.cc:76-84`

  **Acceptance Criteria**:
  - [ ] `grep -n "\[PORT INFO\]" src/core/module_factory.cc` returns empty
  - [ ] `grep -n "\[WARN\]" src/core/module_factory.cc` matches line 79+
  - [ ] `cmake --build build -j$(nproc) 2>&1 | grep -iE "error"` returns empty

  **Evidence to Capture**:
  - [ ] `build/logs/day0_g3_log_check.log`

  **Commit**: YES — `fix(factory): upgrade deprecation log [PORT INFO]→[WARN] (ADR-X.9 §1.5)`


- [x] 4. **T0.5 — Fix Gap 5: v4 doc test count sync** (⏱️ 10 min)

  **What to do**:
  - Open `docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v4.md`
  - Find all occurrences of "14/14" or "14 测试" related to FragmentMapper test count
  - Replace with the actual code count (run: `grep -c "TEST_CASE" test/test_fragment_mapper.cc` → currently 17)
  - Check for `` "pending_tid_" → "TransactionContextExt::transaction_id" `` or similar lines that reference 14
  - Update the status line: `✅ FragmentMapper 已验证(14/14 测试通过` → `✅ FragmentMapper 已验证(17/17 测试通过`
  - Also update any "64 assertions in 14 test cases" references

  **Must NOT do**:
  - Change the 593/581 baseline numbers (those need actual cpptlm_tests run to verify)
  - Touch v3 or v2 docs

  **Recommended Agent Profile**:
  - **Category**: `quick`

  **Parallelization**:
  - **Can Run In Parallel**: YES — Wave 0.5a
  - **Blocks**: None
  - **Blocked By**: None

  **References**:
  - `test/test_fragment_mapper.cc` (ground truth: grep -c "TEST_CASE" → 17)
  - `docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v4.md` lines 5, 30, 31, 32, 33, 142, 324

  **Acceptance Criteria**:
  - [ ] `grep -c "\"14/14\"" hybrid_tlm_cppHDL_design_v4.md` returns 0 (all occurrences replaced)
  - [ ] `grep -c "\"17/17\"" hybrid_tlm_cppHDL_design_v4.md` matches ≥ 3 lines
  - [ ] `grep -c "TEST_CASE" test/test_fragment_mapper.cc` still returns 17 (sanity check)

  **Evidence to Capture**:
  - [ ] `build/logs/day0_g5_count_check.log`

  **Commit**: YES — `docs(hybrid): sync FragmentMapper test count 14→17 (Gap 5)`


- [x] 5. **T0.6 — Fix Gap 6: hardcoded "593" in original plan** (⏱️ 10 min)

  **What to do**:
  - In `plans/p0-alignment-remediation-plan.md`, find all occurrences of "593" (the hardcoded baseline test count)
  - Replace each "593" with the command: `` `./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1` ``
  - Keep the surrounding context: e.g., "≥ 594 (baseline 593 + 1 新 E2E)" → "≥ N+1 (baseline N + 1 新 E2E, where N = `./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1`)"
  - Update PC-4: change the grep pattern from hardcoded 593 to dynamic count

  **Must NOT do**:
  - Remove the "593" reference from documentation context (ADR docs or v4 docs may reference it — leave those)
  - Add a new CI script or automation

  **Recommended Agent Profile**:
  - **Category**: `quick`

  **Parallelization**:
  - **Can Run In Parallel**: YES — Wave 0.5a
  - **Blocks**: None
  - **Blocked By**: None

  **References**:
  - `plans/p0-alignment-remediation-plan.md` everywhere "593" appears (Day 1 PC-4, Day 3 T3.5.3, Day 5 T5.1.1, etc.)

  **Acceptance Criteria**:
  - [ ] `grep -c "593" plans/p0-alignment-remediation-plan.md` returns 0
  - [ ] `grep -c "grep -E.*test cases.*tail" plans/p0-alignment-remediation-plan.md` matches ≥ 3 lines

  **Evidence to Capture**:
  - [ ] `build/logs/day0_g6_dynamic_count_check.log`

  **Commit**: YES — `chore: replace hardcoded 593 with dynamic grep pattern (Gap 6)`


- [x] 6. **T0.2 — Fix Gap 2: lowercase 6 downstream files** (⏱️ 1 h)

  **Depends on**: T0.1 (enumeration output)

  **What to do**:
  - Using the enumeration file from T0.1, open each file and replace uppercase PortRole/BundleType string literals with lowercase:
    - `"INITIATOR"` → `"initiator"`, `"TARGET"` → `"target"`, `"BI_DIRECTIONAL"` → `"bi_directional"`, `"NETWORK"` → `"network"`, `"PE"` → `"pe"`
    - `"CACHE_REQ"` → `"cache_req"`, `"CACHE_RESP"` → `"cache_resp"`, `"NOC_FLIT"` → `"noc_flit"`
  - For each file, determine if the string is:
    - Category A: **String literal comparison** (e.g., `if (role == "MASTER")`) → change directly to lowercase
    - Category B: **JSON config fixture** (e.g., `"role": "MASTER"`) → change to lowercase
    - Category C: **Enum value passed to NLOHMANN_JSON_SERIALIZE_ENUM** (e.g., in port_types.hh itself) → already handled by original T1.1, skip
  - Known files to update:
    - `src/core/port_compatibility.cc:69-73`
    - `src/core/module_factory_validate.cc:334-335`
    - `test/test_nic_port_groups.cc:37,40,41,48,49,83,105`
    - `test/test_phase3_2_port_management.cc:209-223,250-262`
    - `test/test_port_types.cc:222-226`
    - `configs/test/nic_tlm_port_groups.json:14,17,18,25,26`

  **Must NOT do**:
  - Change enum values in `port_types.hh` (that's T1.1 in the original plan)
  - Reformat files (only string replacement)
  - Refactor category C references (they become enum-based through the original T1.1 fix)

  **Recommended Agent Profile**:
  - **Category**: `quick`

  **Parallelization**:
  - **Can Run In Parallel**: NO (depends on T0.1 enumeration)
  - **Blocks**: None (but T0.9 dependent)
  - **Blocked By**: T0.1

  **References**:
  - `build/logs/day0_gap2_enumeration.txt` (from T0.1)
  - `include/core/port_types.hh:23-44` (case reference: what lowercase looks like)
  - `docs/adr/ADR-X.9-port-type-system.md §决策 1` (authoritative lowercase requirement)

  **Acceptance Criteria**:
  - [ ] `grep -rE '"INITIATOR"|"TARGET"|"BI_DIRECTIONAL"|"NETWORK"|"PE"|"CACHE_REQ"|"CACHE_RESP"|"NOC_FLIT"' include/ src/ test/ configs/ | grep -v port_types.hh | wc -l` returns 0
  - [ ] `cmake --build build -j$(nproc) 2>&1 | grep -iE "error"` returns empty
  - [ ] `./build/bin/cpptlm_tests "[port_types]" 2>&1 | tail -5` shows PASS

  **Evidence to Capture**:
  - [ ] `build/logs/day0_g2_grep_check.log`

  **Commit**: YES — `fix: downstream PortRole/BundleType lowercase alignment (Gap 2)`


- [x] 7. **T0.7 — Fix Gap 7: ch_bool literals → ch_reg\<ch_bool\>** (⏱️ 30 min)

  **What to do**:
  - Open `src/rtl/hybrid_cache_component.cc`
  - Replace the 5 `ch_bool(true/false)` literal assignments:
    - Line 25: `ch_reg<ch_bool>     saved_is_write(ch_bool(false));` → `ch_reg<ch_bool>     saved_is_write(0_d);` (use reg default)
    - Line 52: `io().resp_out.payload.is_hit         = ch_bool(true);` → `io().resp_out.payload.is_hit         = ch_bool(state == 1_d);` (derived from FSM state)
    - Line 54: `io().resp_out.payload.first          = ch_bool(true);` → `io().resp_out.payload.first          = ch_bool(state == 1_d);`
    - Line 55: `io().resp_out.payload.last           = ch_bool(true);` → `io().resp_out.payload.last           = ch_bool(state == 1_d);`
    - Line 56: `io().resp_out.valid = ch_bool(true);` → keep as wire assignment (this IS valid — it's a valid signal driven by state)
    - Line 60: `io().resp_out.valid = ch_bool(false);` → `io().resp_out.valid = ch_bool(state == 0_d);` (in the same case as ready)
  - Verify with `chppHDL_api_verification.md §3.1 CRITICAL #6` pattern: use `ch_reg<ch_bool>` state registers or derived expressions, not literal booleans assigned to port fields
  - The key SEGV trigger (per §2.5) is `io().req_in.ready = ch_bool(false);` — verify this pattern doesn't exist

  **Must NOT do**:
  - Change port structure of `__io(ch_stream<Bundle>)` (that's P0-#2 decision, separate from this gap)
  - Modify `hybrid_cache_wrapper.cc` (wrapper's set_value() calls use different patterns)
  - Reformat or restructure any FSM logic

  **Recommended Agent Profile**:
  - **Category**: `quick`

  **Parallelization**:
  - **Can Run In Parallel**: YES — Wave 0.5b
  - **Blocks**: None
  - **Blocked By**: None

  **References**:
  - `src/rtl/hybrid_cache_component.cc:25,52,54,55,56,60`
  - `docs/architecture/examples/hybrid/chppHDL_api_verification.md §2.5` (SEGV evidence)
  - `docs/architecture/examples/hybrid/chppHDL_api_verification.md §3.1 CRITICAL #6` (ch_bool literal fix pattern)

  **Acceptance Criteria**:
  - [ ] `grep -n "ch_bool(true)\|ch_bool(false)" src/rtl/hybrid_cache_component.cc` returns only lines that are acceptable (e.g., `io().resp_out.valid = ch_bool(true/false)` where it's a synthesized valid signal driven by clock context)
  - [ ] `ast_grep_search pattern="ch_bool(true)|ch_bool(false)" lang=cpp paths=["src/rtl/hybrid_cache_component.cc"]` returns 0 or only acceptable lines

  **Evidence to Capture**:
  - [ ] `build/logs/day0_g7_chbool_check.log`

  **Commit**: YES — `fix(rtl): replace ch_bool literal port assignments with state-derived signals (Gap 7)`


- [x] 8. **T0.8 — Fix Gap 8: AGENTS.md test count** (⏱️ 5 min)

  **What to do**:
  - Open `AGENTS.md` (root) or `include/AGENTS.md`
  - Find the line that says "43 文件" in the test/ section
  - Update it to the current count: `find test -name "*.cc" -not -name "*.disabled" | wc -l` (currently 75)
  - Also update `AGENTS.md` in `test/` directory if it exists and has a similar stale count

  **Must NOT do**:
  - Reformat the entire AGENTS.md
  - Change any other section of AGENTS.md

  **Recommended Agent Profile**:
  - **Category**: `quick`

  **Parallelization**:
  - **Can Run In Parallel**: YES — Wave 0.5b
  - **Blocks**: T0.9

  **References**:
  - `AGENTS.md` at root (main one)
  - `test/AGENTS.md` if exists
  - `find test -name "*.cc" -not -name "*.disabled" | wc -l` for current count

  **Acceptance Criteria**:
  - [ ] `grep -c "43 文件" AGENTS.md 2>/dev/null; grep -c "43 文件" test/AGENTS.md 2>/dev/null` returns 0
  - [ ] The updated count matches `find test -name "*.cc" -not -name "*.disabled" | wc -l`

  **Evidence to Capture**:
  - [ ] `build/logs/day0_g8_agents_check.log`

  **Commit**: YES — `docs: sync AGENTS.md test file count 43→N (Gap 8)`


- [x] 9. **T0.9 — Fix Gap 4: v2.1 §6 cross-references** (⏱️ 20 min)

  **Depends on**: T0.2, T0.5, T0.8 (gap-adjacent doc updates should complete first)

  **What to do**:
  - Open `docs/architecture/01-hybrid-architecture-v2.1.md` section §6 (around line 482)
  - Add the following rows to the existing reference table:
    ```
    | Transaction ID 架构决策 | docs/adr/ADR-X.1-transaction-id.md | ✅ 对齐 |
    | Transaction 整合方案 | docs/adr/ADR-X.6-transaction-integration.md | ✅ 对齐 |
    | 端口类型系统决策 | docs/adr/ADR-X.9-port-type-system.md | ✅ 对齐 |
    | Hybrid TLM+CppHDL v4 设计 | docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v4.md | 📋 v4 替代 v3 |
    ```
  - Verify all referenced files exist at the specified paths
  - Ensure the "hybrid_v4" entry uses the correct relative path

  **Must NOT do**:
  - Add content that doesn't reference existing files (would break links)
  - Restructure §6 or modify any other sections of v2.1
  - Change the existing table entries

  **Recommended Agent Profile**:
  - **Category**: `quick`

  **Parallelization**:
  - **Can Run In Parallel**: NO (sequential after T0.2, T0.5, T0.8)
  - **Blocks**: None
  - **Blocked By**: T0.2, T0.5, T0.8

  **References**:
  - `docs/architecture/01-hybrid-architecture-v2.1.md:482-490` (current §6 table)
  - `docs/adr/ADR-X.1-transaction-id.md` (verify exists)
  - `docs/adr/ADR-X.6-transaction-integration.md` (verify exists)
  - `docs/adr/ADR-X.9-port-type-system.md` (verify exists)
  - `docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v4.md` (verify exists)

  **Acceptance Criteria**:
  - [ ] `ls docs/adr/ADR-X.1-transaction-id.md ADR-X.6-transaction-integration.md ADR-X.9-port-type-system.md` all exist
  - [ ] `ls docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v4.md` exists
  - [ ] `grep -c "ADR-X.1\|ADR-X.6\|ADR-X.9" docs/architecture/01-hybrid-architecture-v2.1.md` returns ≥ 3
  - [ ] `grep -c "hybrid_v4\|hybrid_tlm_cppHDL_design_v4" docs/architecture/01-hybrid-architecture-v2.1.md` returns ≥ 1

  **Evidence to Capture**:
  - [ ] `build/logs/day0_g4_xref_check.log`

  **Commit**: YES — `docs(v2.1): add ADR-X.1/6/9 and hybrid_v4 cross-references to §6 (Gap 4)`

---

## Final Verification Wave

- [x] F1. **Full compile check** — `quick`
  ```bash
  cmake --build build -j$(nproc) 2>&1 | grep -iE "error|ArbiterTLM"
  ```
  Expected: empty output
  
- [x] F2. **cpptlm_tests baseline** — `quick`
  ```bash
  ./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1
  ```
  Expected: "All tests passed (...)" with stable count
  
- [x] F3. **Grep sweep** — `quick`
  ```bash
  grep -rE '"MASTER"|"SLAVE"|"INITIATOR"|"TARGET"|"BI_DIRECTIONAL"|"NETWORK"|"PE"|"CACHE_REQ"|"CACHE_RESP"|"NOC_FLIT"' \
    include/ src/ test/ configs/ 2>/dev/null | grep -v "port_types.hh" | grep -v "\.git" | head -20
  ```
  Expected: empty output (all replaced)

---

## Commit Strategy

- **T0.3, T0.4, T0.7**: `fix(rtl|factory): [gap detail] prefix`
- **T0.2, T0.5, T0.6, T0.8, T0.9**: `docs|chore: [gap detail]`
- **Final**: Single squash commit: `fix: pre-flight patches for p0-alignment-remediation Sprint`

---

## Success Criteria

### Verification Commands
```bash
cmake --build build -j$(nproc) 2>&1 | grep -iE "error"
grep -rE '"INITIATOR"|"TARGET"' include/ src/ test/ configs/ | grep -v port_types.hh | grep -v "\.git"
grep -c "TEST_CASE" test/test_fragment_mapper.cc  # → 17
grep -n "\[PORT INFO\]" src/core/module_factory.cc  # → 0
```

### Final Checklist
- [ ] All 8 gap patches applied
- [ ] cmake --build compiles clean
- [ ] cpptlm_tests passes (stable baseline)
- [ ] Remaining hardcoded uppercase strings: 0
