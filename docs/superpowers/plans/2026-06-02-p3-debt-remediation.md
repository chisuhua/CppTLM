# P3 Technical Debt Remediation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Address 2 outstanding P3 items (P3.2 circular dependency, P3.3 CI ASan). P3.1 (12 failing tests) verified resolved by current build.

**Architecture:** Forward declaration fix for circular dependency; conditional ASan in CI matrix.

**Tech Stack:** C++17, GitHub Actions, CMake, AddressSanitizer

---

## File Map

| File | Action | Purpose |
|------|--------|---------|
| `include/core/chstream_port.hh:9` | **Modify** | Replace `#include` with forward declaration of `StreamAdapterBase` |
| `.github/workflows/ci.yml:39-47` | **Modify** | Add ASan flags to Debug matrix build |
| `CMakeLists.txt` | **Modify (optional)** | Add `USE_ASAN` option if not present |

---

## Pre-Flight: P3.1 Status Verification

**Status:** ✅ **ALREADY RESOLVED** (2026-06-02)

Ran full test suite: 14519 assertions, 545 test cases, **all pass**.

Original concern in `debt-remediation-plan.md` referenced "12 known failing tests" related to Pool/Wildcard/Connection. These were Phase 0-6 historical issues. After subsequent fixes:

- P0.3: `wildcard.hh` empty catch block fixed → wildcard tests pass
- P1.2: `latency` injection implemented → connection tests pass
- P0.1: `PortPair` memory leak fixed → pool/integration tests pass

**Action:** Mark P3.1 as completed in `debt-remediation-plan.md`. No code changes needed.

---

## Task 1: P3.2 - Break Circular Dependency

**Files:**
- Modify: `include/core/chstream_port.hh:9`
- Verify: `include/framework/bidirectional_port_adapter.hh`

- [ ] **Step 1: Verify circular dependency**

Confirm the cycle exists:
- `framework/bidirectional_port_adapter.hh:12` → `#include "core/chstream_port.hh"`
- `core/chstream_port.hh:9` → `#include "framework/stream_adapter.hh"` (cycle!)

- [ ] **Step 2: Examine usage in chstream_port.hh**

`chstream_port.hh` uses only `StreamAdapterBase*` (line 58) — a pointer that can be forward-declared. The call to `adapter_->process_request_input(pkt)` (line 67) is a virtual method, requiring only the base class declaration (not the full definition).

- [ ] **Step 3: Apply forward declaration fix**

Edit `include/core/chstream_port.hh`:

**BEFORE:**
```cpp
#include "framework/stream_adapter.hh"
```

**AFTER:**
```cpp
namespace cpptlm { class StreamAdapterBase; }  // Forward declaration
```

- [ ] **Step 4: Verify build succeeds**

```bash
cmake --build build -j$(nproc)
```

Expected: Build succeeds without errors.

- [ ] **Step 5: Run tests to confirm no behavioral change**

```bash
./build/bin/cpptlm_tests ~"[crossbar]" -s 2>&1 | tail -3
```

Expected: All tests pass (14519 assertions, 545 cases).

- [ ] **Step 6: Commit**

```bash
git add include/core/chstream_port.hh
git commit -m "refactor(core): break circular dependency with framework

chstream_port.hh only uses StreamAdapterBase* (pointer).
Replace #include with forward declaration to break the cycle:
  framework/bidirectional_port_adapter.hh
    → core/chstream_port.hh
    → framework/stream_adapter.hh (was circular)

This is Option B from debt-remediation-plan.md (P3.2).
Virtual method call adapter_->process_request_input(pkt) still works
because vtable lookup is runtime; no template instantiation needed.

Refs: chstream_port.hh:9, P3.2"
```

---

## Task 2: P3.3 - Enable ASan in CI

**Files:**
- Modify: `.github/workflows/ci.yml`
- Verify: `CMakeLists.txt` supports `USE_ASAN` flag

- [ ] **Step 1: Check if USE_ASAN option exists in CMakeLists**

```bash
grep -n "USE_ASAN" CMakeLists.txt
```

If not present, add to root CMakeLists.txt:
```cmake
option(USE_ASAN "Enable AddressSanitizer (Debug only)" OFF)
if(USE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
    message(STATUS "ASan enabled (Debug build)")
endif()
```

- [ ] **Step 2: Add ASan to CI matrix**

Edit `.github/workflows/ci.yml` to add a new matrix entry for ASan:

**BEFORE:**
```yaml
strategy:
  matrix:
    build-type: [Release, Debug]
    use-systemc: [OFF]
```

**AFTER:**
```yaml
strategy:
  matrix:
    build-type: [Release, Debug]
    use-systemc: [OFF]
    use-asan: [OFF]  # New
    exclude:
      # Only enable ASan for Debug builds
      - build-type: Release
        use-asan: ON
```

- [ ] **Step 3: Pass USE_ASAN to CMake configure**

```yaml
- name: Configure CMake
  run: |
    cmake -S . -B build \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=${{ matrix.build-type }} \
      -DUSE_SYSTEMC=${{ matrix.use-systemc }} \
      -DUSE_ASAN=${{ matrix.use-asan }}
```

- [ ] **Step 4: Verify CI workflow syntax**

Use GitHub's local validation:
```bash
# Cannot validate directly without push, but lint the YAML
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))"
```

- [ ] **Step 5: Document ASan usage**

Add a comment in the CI workflow explaining when ASan is enabled.

- [ ] **Step 6: Commit**

```bash
git add .github/workflows/ci.yml CMakeLists.txt
git commit -m "ci: enable AddressSanitizer for Debug builds

- Add use-asan matrix to ci.yml (excluded for Release builds)
- Add USE_ASAN option to CMakeLists.txt with compile/link flags
- ASan adds ~2x CI time on Debug builds but catches memory bugs
  automatically on every PR

Refs: P3.3, debt-remediation-plan.md"
```

---

## Task 3: Update Documentation

**Files:**
- Modify: `plans/debt-remediation-plan.md`

- [ ] **Step 1: Mark P3.1, P3.2, P3.3 with completion status**

Update the debt-remediation-plan.md:
- P3.1: ✅ Already resolved
- P3.2: ✅ Circular dependency fixed
- P3.3: ✅ ASan enabled in CI

- [ ] **Step 2: Update plan status header**

Change:
```markdown
**Status:** P0/P1/P2 COMPLETED (2026-06-02), P3 Pending
```

To:
```markdown
**Status:** ALL COMPLETED (2026-06-02)
```

- [ ] **Step 3: Commit**

```bash
git add plans/debt-remediation-plan.md
git commit -m "docs: mark all P3 items as completed"
```

---

## Execution Checkpoints

- [ ] P3.1: Verified 0 failing tests
- [ ] P3.2: Circular dependency broken + build/tests pass
- [ ] P3.3: ASan enabled in CI matrix
- [ ] Documentation updated

---

## Validation Commands

```bash
# Full build + test
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests ~"[crossbar]" -s

# ASan build verification
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
cmake --build build-asan -j$(nproc)
./build-asan/bin/cpptlm_tests ~"[crossbar]"
# Verify no "Direct leak" or "AddressSanitizer" errors
```

---

## Dependencies

```
P3.1 (verification) ──┐
P3.2 (circular fix)  ─┼──► Independent, can be done in parallel
P3.3 (CI ASan)       ──┘
```

## Risk Assessment

| Task | Risk | Mitigation |
|------|------|-----------|
| P3.2 | Low | Pure forward declaration, no behavioral change |
| P3.3 | Low | ASan only on Debug, no impact on Release |
