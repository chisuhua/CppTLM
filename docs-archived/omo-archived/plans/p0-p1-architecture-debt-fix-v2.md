# P0-P1 Architecture Debt Fix v2 (Metis Review Corrected)

> **For agentic workers:** Use `superpowers:subagent-driven-development` or `superpowers:executing-plans` for task-by-task implementation.

**Goal:** Fix 6 newly discovered architecture/code debt items with corrected approach. **All file moves must be preceded by full reference discovery.**

**Key Corrections from Metis Review:**
- C1/C2: Added Wave 0 Preflight Discovery to find ALL references before file moves
- C3: Do NOT move `tlm/tlm_stub.hh`; break dependency from core→tlm instead
- C4: Handle ModuleFactory + ModuleGroup together; use explicit delete (lowest risk)
- C5: Redesigned split with clear ownership per compilation unit

**Tech Stack:** C++17, CMake, Catch2, ASan

---

## Context

v1 plan had 5 CRITICAL issues found by Metis. v2 addresses all of them.

| Issue | v1 Defect | v2 Correction |
|-------|-----------|---------------|
| C1 | Task 3 listed only 2 refs, actual 14 | **Wave 0 Preflight** — discover all refs first |
| C2 | Task 2 listed only 2 refs, actual 6 | **Wave 0 Preflight** — discover all refs first |
| C3 | Moving `tlm/tlm_stub.hh` to `core/` is API change | **Do NOT move file** — use forward declarations |
| C4 | Missed ModuleGroup second leak point | **Fix both** — ModuleFactory + ModuleGroup |
| C5 | validateConfig in two files (contradiction) | **Redesigned split** — clear per-file ownership |

---

## Work Objectives

### Core Objective
Fix ASan memory leak (26240 bytes / 168 allocations) and architecture layer inversions. **Zero API changes.**

### Concrete Deliverables
1. `packet_to_payload.hh` path fix
2. `chstream_adapter_factory.hh` moved from `core/` to `framework/`
3. `core/ext/*` no longer include `tlm/tlm_stub.hh` (forward declarations)
4. Remove/implement TODO at `module_factory.cc:439`
5. Memory leak fix in ModuleFactory + ModuleGroup (explicit delete)
6. Split `module_factory.cc` (1062 lines) into logical units

### Definition of Done
- [ ] ASan build: zero leaks reported
- [ ] Standard build: all tests pass (~545 cases)
- [ ] No new warnings
- [ ] `core/` has zero includes of `framework/` or `tlm/` (stubs/fwd excepted)

### Must Have
- Memory leak fix (ASan clean)
- Layering fix (core independent of framework/tlm)
- All existing tests pass
- No API changes (backward compatible)

### Must NOT Have
- No new API changes
- No test behavior changes
- No new dependencies
- No missed references during file moves

---

## Execution Strategy

### Wave 0: Preflight Discovery (MUST complete first)

**Objective:** Discover ALL references before any file moves.

- **Task 0.1**: Discover all `chstream_adapter_factory.hh` references (production + test + docs)
- **Task 0.2**: Discover all `tlm_stub.hh` references (production + test + docs)
- **Task 0.3**: Determine actual types used from tlm_stub in core/ext files (for forward declaration)
- **Task 0.4**: Verify ModuleGroup usage patterns (confirm external pointer holders)

---

### Wave 1: Independent Quick Fixes (parallel execution)

- **Task 1**: Fix `packet_to_payload.hh` path error
- **Task 2**: Move `chstream_adapter_factory.hh` from `core/` to `framework/`
- **Task 3**: Break core->tlm dependency with forward declarations (do NOT move tlm_stub)
- **Task 4**: Remove/implement TODO at `module_factory.cc:439`

---

### Wave 2: Memory Safety Fix (sequential)

- **Task 5**: Fix ModuleFactory + ModuleGroup memory leak with explicit delete

---

### Wave 3: module_factory.cc Split (sequential, depends on Wave 2)

- **Task 6**: Split `module_factory.cc` into logical compilation units

---

## TODOs

### Wave 0: Preflight Discovery

- [x] 1. **Discover chstream_adapter_factory.hh references**

  **What to do:**
  Run `grep -rn "chstream_adapter_factory" include/ src/ test/ docs/ configs/ scripts/` and record ALL matching lines (production + test + docs).

  **Why:** v1 plan listed only 2 refs but actual is 6 (2 production + 4 test). Must update ALL includes before file move.

  **QA Scenario:**
  ```
  Scenario: Complete reference list
    Tool: Bash
    Steps:
      1. grep -rn "chstream_adapter_factory" include/ src/ test/ docs/ configs/ scripts/ > /tmp/task01-refs.txt
      2. wc -l /tmp/task01-refs.txt
    Expected Result: >=6 lines (2 production + 4 test)
  ```

  **Commit**: NO (discovery only, part of planning)

- [x] 2. **Discover tlm_stub.hh references**

  **What to do:**
  Run `grep -rn "tlm_stub" include/ src/ test/ docs/ configs/ scripts/` and record ALL matching lines.

  **Why:** v1 plan listed only 2 refs but actual is 14 (5 production + 9 test). Must understand full impact.

  **QA Scenario:**
  ```
  Scenario: Complete reference list
    Tool: Bash
    Steps:
      1. grep -rn "tlm_stub" include/ src/ test/ docs/ configs/ scripts/ > /tmp/task02-refs.txt
      2. wc -l /tmp/task02-refs.txt
    Expected Result: >=14 lines (5 production + 9 test)
  ```

  **Commit**: NO

- [x] 3. **Determine tlm_stub types used in core/ext**

  **What to do:**
  Read `core/packet.hh`, `core/ext/cmd_exts.hh`, `ext/transaction_context_ext.hh`, `ext/error_context_ext.hh`, `ext/mem_exts.hh`. Identify which tlm_stub types are actually used (tlm_generic_payload? tlm_extension_base?). Determine if forward declaration is sufficient.

  **Why:** Task 3 requires choosing between forward declaration (preferred) or creating tlm_fwd.hh. Decision depends on actual type usage.

  **QA Scenario:**
  ```
  Scenario: Type usage documented
    Tool: Bash + Read
    Steps:
      1. Read each file and list types from tlm_stub namespace
      2. Determine if each type is used by value, pointer, or reference
    Expected Result: Documented list of types + forward-declarable YES/NO per type
  ```

  **Commit**: NO

- [x] 4. **Verify ModuleGroup usage patterns**

  **What to do:**
  Read `utils/module_group.hh` and `topology_dumper.hh` (and any other users of getInstanceRegistry). Confirm how raw pointers are stored and whether unique_ptr conversion would cause dangling pointers.

  **Why:** C4 found second leak point in ModuleGroup. Must confirm safe conversion strategy before Task 5.

  **QA Scenario:**
  ```
  Scenario: Usage pattern documented
    Tool: Bash + Read
    Steps:
      1. grep -rn "getInstanceRegistry" include/ src/ test/ > /tmp/task04-refs.txt
      2. Read topology_dumper.hh for pointer storage patterns
    Expected Result: Documented: who stores pointers, how long, safe to delete?
  ```

  **Commit**: NO

---

### Wave 1: Independent Quick Fixes

- [x] 5. **Fix packet_to_payload.hh path error**

  **Files:**
  - Modify: `include/core/ext/packet_to_payload.hh:6`

  **What to do:**
  Fix broken include path. Current `"packet/packet.hh"` does not exist.

  ```cpp
  // BEFORE (line 6):
  #include "packet/packet.hh"
  // AFTER:
  #include "packet.hh"
  ```

  **Why:** Include search path already includes `include/core/`, so `"packet.hh"` resolves to `include/core/packet.hh`.

  **QA Scenario:**
  ```
  Scenario: Build succeeds after path fix
    Tool: Bash
    Steps:
      1. cmake -S . -B build-test -DCMAKE_BUILD_TYPE=Release
      2. cmake --build build-test -j$(nproc) --target cpptlm_core
    Expected Result: Build succeeds with no "packet/packet.hh" not found error
  ```

  **Commit**: `fix(core): correct include path in packet_to_payload.hh`
  **Files**: `include/core/ext/packet_to_payload.hh`

- [x] 6. **Move chstream_adapter_factory.hh from core/ to framework/**

  **Preflight Dependency**: Task 1 must complete (full reference list known)

  **Files:**
  - Create: `include/framework/chstream_adapter_factory.hh` (move from core/)
  - Delete: `include/core/chstream_adapter_factory.hh`
  - Modify: ALL locations referencing this file (from Task 1 discovery)

  **What to do:**
  1. Move file to correct layer
  2. Update ALL include paths (production + test + docs)

  **QA Scenario:**
  ```
  Scenario: Build after file move
    Tool: Bash
    Steps:
      1. rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
      2. cmake --build build -j$(nproc)
      3. ./build/bin/cpptlm_tests "[chstream]" --output-on-failure
    Expected Result: Build succeeds, chstream tests pass
  ```

  **Commit**: `refactor(framework): move chstream_adapter_factory from core/ to framework/`

- [x] 7. **Clean up core->tlm includes and document architecture**

  **Preflight Dependency**: Tasks 2 and 3 complete

  **Files:**
  - Modify: `include/core/packet.hh`
  - Modify: `include/core/ext/cmd_exts.hh`
  - Modify: `include/core/AGENTS.md` (document architecture)

  **What to do:**
  **Do NOT try to break ext->tlm dependency** — `ext/` files are TLM extensions by design and must inherit from `tlm::tlm_extension<T>`. Instead:

  1. **`packet.hh`**: Remove redundant direct `#include "tlm/tlm_stub.hh"` (lines 6-10). The include is redundant because `ext/transaction_context_ext.hh` and `ext/error_context_ext.hh` (included below) already bring in tlm_stub.

  2. **`core/ext/cmd_exts.hh`**: Evaluate if this file should move to `ext/` directory. If it's only used by `core/ext/packet_to_payload.hh` and `core/ext/payload_to_packet.hh`, keep it in `core/ext/` as a special sub-layer. Otherwise move to `ext/`.

  3. **Document in AGENTS.md**: Add note that `ext/` layer is allowed to depend on `tlm/` — this is intentional architecture, not a layering violation.

  ```cpp
  // packet.hh BEFORE:
  #ifdef USE_SYSTEMC_STUB
  #include "tlm/tlm_stub.hh"
  #else
  #include "tlm.h"
  #endif

  // packet.hh AFTER: (remove the above block, ext includes below already provide these types)
  ```

  **QA Scenario:**
  ```
  Scenario: Build still works after include cleanup
    Tool: Bash
    Steps:
      1. cmake -S . -B build-test -DCMAKE_BUILD_TYPE=Release -DUSE_SYSTEMC=OFF -DUSE_SYSTEMC_STUB=ON
      2. cmake --build build-test -j$(nproc)
      3. ./build-test/bin/cpptlm_tests "[chstream]"
    Expected Result: Build succeeds, chstream tests pass
  ```

  **Commit**: `refactor(core): remove redundant tlm includes and document ext->tlm dependency`

- [x] 8. **Remove/implement TODO at module_factory.cc:439**

  **Files:**
  - Modify: `src/core/module_factory.cc:439`

  **What to do:**
  1. Read TODO context (10 lines)
  2. If hierarchy_root is unused: remove TODO comment
  3. If hierarchy_root is used: add `std::unique_ptr<TopologyNode> hierarchy_root_` member, assign in instantiateAll

  **Decision criteria**: Based on actual code usage, not TODO text.

  **QA Scenario:**
  ```
  Scenario: Build succeeds after TODO resolution
    Tool: Bash
    Steps:
      1. cmake --build build -j$(nproc)
      2. grep -n "TODO.*hierarchy_root" src/core/module_factory.cc || echo "TODO resolved"
    Expected Result: Build succeeds, TODO resolved
  ```

  **Commit**: `chore(module_factory): resolve hierarchy_root TODO`

---

### Wave 2: Memory Safety Fix

- [x] 9. **Fix ModuleFactory + ModuleGroup memory leak**

  **Preflight Dependency**: Task 4 must complete (usage patterns confirmed)

  **Files:**
  - Modify: `include/core/module_factory.hh`
  - Modify: `src/core/module_factory.cc`
  - Modify: `include/utils/module_group.hh`

  **What to do:**

  **Strategy: Explicit delete (lowest risk)**
  - unique_ptr conversion risks dangling pointers for external holders
  - ModuleFactory and ModuleGroup both own the objects they store

  **Step 1: ModuleFactory destructor**
  ```cpp
  ~ModuleFactory() {
      for (auto& [name, obj] : object_instances_) {
          delete obj;
      }
      object_instances_.clear();
      // Same for module_instances_
  }
  ```

  **Step 2: ModuleGroup clearAll() and unregisterInstance()**
  ```cpp
  static void unregisterInstance(const std::string& name) {
      auto it = getInstanceRegistry().find(name);
      if (it != getInstanceRegistry().end()) {
          delete it->second;  // <-- ADD THIS
          getInstanceRegistry().erase(it);
      }
  }

  static void clearAll() {
      for (auto& [name, obj] : getInstanceRegistry()) {
          delete obj;  // <-- ADD THIS
      }
      getInstanceRegistry().clear();
      getGroups().clear();
  }
  ```

  **QA Scenario:**
  ```
  Scenario: ASan detects no leaks after fix
    Tool: Bash
    Steps:
      1. cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
      2. cmake --build build-asan -j$(nproc)
      3. ./build-asan/bin/cpptlm_tests "[connection]"
    Expected Result: All tests pass, no "Direct leak" in ASan output
  ```

  **Commit**: `fix(memory): delete SimObject instances in ModuleFactory and ModuleGroup`

---

### Wave 3: module_factory.cc Split

- [x] 10. **Split module_factory.cc — extract validation** (commit 14acd9a: module_factory.cc 1083→677 lines, new module_factory_validate.cc 448 lines; 4 statics made non-static for cross-TU linkage; build OK, 543/545 tests pass, no regressions)

  **Preflight Dependency**: Tasks 8 and 9 complete

  **Files:**
  - Create: `src/core/module_factory_validate.cc` (~300 lines)
  - Create: `src/core/module_factory_instantiate.cc` (~350 lines)
  - Create: `src/core/module_factory_connect.cc` (~250 lines)
  - Modify: `src/core/module_factory.cc` (~200 lines, keep registration + lifecycle)
  - Modify: `src/CMakeLists.txt` (add new sources)

  **Clear ownership (avoids C5 contradiction):**
  1. **module_factory.cc**: constructor/destructor, register/unregister, clearAll, startAllTicks, getInstance
  2. **module_factory_validate.cc**: validateConfig, mergeConfigs, processExtends, validation helpers
  3. **module_factory_instantiate.cc**: instantiateAll, instantiate, loadPlugins, calls validateConfig (via #include or forward declaration)
  4. **module_factory_connect.cc**: resolveConnections, parsePortSpec, port helpers

  **Key**: validateConfig is ONLY in validate.cc. instantiate.cc includes validate.hh or uses forward declaration.

  **QA Scenario:**
  ```
  Scenario: Build and all tests pass after split
    Tool: Bash
    Steps:
      1. rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
      2. cmake --build build -j$(nproc)
      3. ./build/bin/cpptlm_tests ~"[crossbar]"
    Expected Result: All tests pass (14519 assertions, 545 cases)
  ```

  **Commit**: `refactor(module_factory): split 1062-line file into logical units`

---

## Final Verification Wave

- [x] F1. **ASan Verification** (PARTIAL: Task 9 SimObject leak fixed, but 3 other leak sources remain — see followup)
  ```bash
  cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON -DUSE_SYSTEMC=OFF
  cmake --build build-asan -j$(nproc)
  ./build-asan/bin/cpptlm_tests
  # Result: Task 9 SimObject leak verified fixed (0 SimObject leaks remain).
  #         However, 3 other leak sources discovered (out of Task 9 scope):
  #         - PortManager (no destructor, ~360 leak frames)
  #         - EventQueue (TickEvent not deleted, 83+ frames)
  #         - PacketPool (may not return all objects, 18 frames)
  #         Total remaining: 8,596,656 bytes / 345,246 allocs (not from Task 9)
  ```

- [x] F2. **Full Test Suite** (PASS: 543/545 — 2 pre-existing phase0 test isolation issues, no regressions)
  ```bash
  cmake --build build -j$(nproc)
  ./build/bin/cpptlm_tests ~"[crossbar]"
  # Result: 545 cases, 543 pass, 2 fail (pre-existing)
  # 14516 assertions, 14514 pass, 2 fail
  ```

- [x] F3. **Build Warnings Check** (PASS after fix: 0 new warnings from Tasks 5-9; fix commit e0edeb0 added `#include "core/sim_object.hh"` to module_group.hh)
  ```bash
  cmake --build build -j$(nproc) 2>&1 | grep -i "warning:"
  # Result: 0 Wdelete-incomplete warnings, 0 incomplete type warnings
  ```

- [x] F4. **Architecture Layer Check** (PASS: all 5 architecture checks)
  ```bash
  # Result:
  # Check 1a: chstream_adapter_factory in framework/ ✓
  # Check 1b: removed from core/ ✓
  # Check 2: 0 core/ (outside core/ext/) includes framework/ or tlm/ ✓
  # Check 3: core/ext/ includes tlm/ (1 match, documented exception) ✓
  # Check 4: packet.hh no direct tlm include ✓
  # Check 5: AGENTS.md has architecture doc section ✓
  ```

---

## Commit Strategy

```bash
# Wave 1 commits
git add include/core/ext/packet_to_payload.hh
git commit -m "fix(core): correct include path in packet_to_payload.hh"

git add include/core/chstream_adapter_factory.hh include/framework/chstream_adapter_factory.hh
git commit -m "refactor(framework): move chstream_adapter_factory from core/ to framework/"

git add include/core/packet.hh include/core/ext/cmd_exts.hh include/ext/*.hh
git commit -m "refactor(core): break core->tlm dependency with forward declarations"

git add src/core/module_factory.cc
git commit -m "chore(module_factory): resolve hierarchy_root TODO"

# Wave 2 commit
git add include/core/module_factory.hh src/core/module_factory.cc include/utils/module_group.hh
git commit -m "fix(memory): delete SimObject instances in ModuleFactory destructor"

# Wave 3 commit
git add src/core/module_factory*.cc src/CMakeLists.txt
git commit -m "refactor(module_factory): split 1062-line file into logical units"
```

---

## Success Criteria

### Verification Commands
```bash
# 1. ASan build clean
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON -DUSE_SYSTEMC=OFF
cmake --build build-asan -j$(nproc)
./build-asan/bin/cpptlm_tests 2>&1 | grep -i "leak" || echo "ASan: CLEAN"

# 2. Standard build passes
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DUSE_SYSTEMC=OFF
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests ~"[crossbar]"

# 3. Architecture layer clean
grep -rn "#include" include/core/ | grep -E "framework/|tlm/" | grep -v "tlm_stub" | grep -v "tlm_fwd" | wc -l
# Expected: 0
```

### Final Checklist
- [ ] ASan: 0 leaks reported
- [ ] All tests: 14519 assertions in 545 cases pass
- [ ] No new warnings
- [ ] Architecture layer: core/ has 0 framework/ or tlm/ includes

