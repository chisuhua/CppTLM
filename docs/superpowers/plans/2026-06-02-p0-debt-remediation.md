# P0 Technical Debt Remediation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix 3 P0 critical issues: PortPair memory leak (3 locations), dead Python files (2 files), and empty catch block in wildcard.hh. All 3 tasks are independent and can be executed in parallel.

**Architecture:** Three independent fixes, each with TDD approach (write failing test first, then fix). No cross-task dependencies.

**Tech Stack:** C++17, ASan for memory leak detection, Catch2 for testing

---

## File Map

| File | Action | Purpose |
|------|--------|---------|
| `src/core/module_factory.cc:752,934,941` | **Modify** | Replace `new PortPair` with `std::make_unique` |
| `python/noc_mesh.py` | **Delete** | Dead code - references undefined symbols |
| `python/noc_builder.py` | **Delete** | Dead code - only consumer is noc_mesh.py |
| `include/utils/wildcard.hh:28` | **Modify** | Replace empty `catch(...)` with specific exception handlers |

---

## Task 1: P0.1 - Fix PortPair Memory Leak

**Files:**
- Modify: `src/core/module_factory.cc:752, 934, 941`
- Test: Run ASan build to verify leak is fixed

- [ ] **Step 1: Verify current behavior with ASan**

Build with AddressSanitizer to confirm the leak exists:

```bash
cd /workspace/project/CppTLM
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON -DUSE_SYSTEMC=OFF
cmake --build build -j$(nproc)
# Run a test that exercises ModuleFactory instantiation
./build/bin/cpptlm_tests "[phase6]" --output-on-failure 2>&1 | grep -i "leak"
```

Expected: ASan reports "Direct leak" for PortPair at module_factory.cc lines 752, 934, 941

- [ ] **Step 2: Examine the 3 leak locations**

Read the relevant sections of module_factory.cc to understand how PortPair is created and used:

```bash
# Show lines around each leak location
sed -n '745,760p' src/core/module_factory.cc
sed -n '928,945p' src/core/module_factory.cc
```

Each location follows the pattern:
```cpp
new PortPair(src_port, dst_port);  // allocated but never stored or deleted
```

- [ ] **Step 3: Write a test to verify leak is fixed**

Create a minimal test that instantiates modules via ModuleFactory and verifies no PortPair leak:

```cpp
// test/test_portpair_leak.cc
// Test to verify PortPair memory leak is fixed
#include <catch2/catch_all.hpp>
#include "core/module_factory.hh"
#include "core/event_queue.hh"
#include "modules.hh"
#include "chstream_register.hh"

TEST_CASE("PortPair no memory leak", "[memory][p0]") {
    EventQueue eq;
    registerAllModules();  // from modules.hh + chstream_register.hh
    ModuleFactory factory(&eq);

    // Load a config that creates PortPair connections
    json config = loadConfig("configs/mesh_2x2_tlm.json");
    REQUIRE(factory.instantiateAll(config));

    // Let simulation run briefly
    factory.startAllTicks();
    eq.run(10);

    // Factory destruction should not leak PortPair
    // ASan will catch any leak during destruction
}
```

- [ ] **Step 4: Run test to confirm it fails (or passes after fix verification)**

```bash
./build/bin/cpptlm_tests "PortPair no memory leak" --output-on-failure
```

- [ ] **Step 5: Fix the 3 leak locations**

Edit `src/core/module_factory.cc` at each location (752, 934, 941):

**Location 752 - BEFORE:**
```cpp
new PortPair(src_port, dst_port);
```

**Location 752 - AFTER:**
```cpp
auto pp = std::make_unique<PortPair>(src_port, dst_port);
```

Apply the same change at lines 934 and 941.

- [ ] **Step 6: Verify fix with ASan**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[phase6]" --output-on-failure 2>&1 | grep -i "leak"
```

Expected: No "Direct leak" reported for PortPair

- [ ] **Step 7: Commit**

```bash
git add src/core/module_factory.cc
git commit -m "fix(memory): use unique_ptr for PortPair in module_factory

3处 new PortPair 无对应 delete，每次仿真泄漏。
ASan verified clean.
Refs: module_factory.cc:752,934,941"
```

---

## Task 2: P0.2 - Delete Dead Python Files

**Files:**
- Delete: `python/noc_mesh.py`, `python/noc_builder.py`
- Verify: Confirm no other references to these files

- [ ] **Step 1: Verify no other consumers**

Confirm these files have no other consumers in the codebase:

```bash
# Check for imports/uses of noc_mesh
git grep -r "noc_mesh" -- "*.py" "*.cc" "*.hh" "*.md" docs/ configs/

# Check for imports/uses of noc_builder
git grep -r "noc_builder" -- "*.py" "*.cc" "*.hh" "*.md" docs/ configs/
```

Expected: Only the files themselves should be found (or nothing if already dead)

- [ ] **Step 2: Verify noc_mesh.py has undefined references**

```bash
python -c "from python import noc_mesh" 2>&1
```

Expected: ImportError with undefined symbols (VcRouter, TerminalNode, etc.)

- [ ] **Step 3: Delete the files**

```bash
git rm python/noc_mesh.py python/noc_builder.py
```

- [ ] **Step 4: Verify deletion**

```bash
# These should now fail with ImportError
python -c "from python import noc_builder" 2>&1  # Should fail
python -c "from python import noc_mesh" 2>&1    # Should fail
```

- [ ] **Step 5: Run full test suite to ensure nothing broke**

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git commit -m "chore(python): remove dead noc_builder.py and noc_mesh.py

两个文件互相引用但均无其他消费者。
noc_mesh.py 引用未定义符号 (VcRouter, TerminalNode)。
Refs: python/noc_builder.py, python/noc_mesh.py"
```

---

## Task 3: P0.3 - Fix Empty Catch Block in wildcard.hh

**Files:**
- Modify: `include/utils/wildcard.hh:28`
- Test: Add test case for invalid regex pattern

- [ ] **Step 1: Read the current wildcard.hh implementation**

```bash
cat include/utils/wildcard.hh
```

Focus on the catch block at line 28 that swallows all exceptions.

- [ ] **Step 2: Add a test case for invalid regex handling**

Add to `test/test_wildcard.cc` (or create if doesn't exist):

```cpp
TEST_CASE("wildcard throws on invalid regex", "[wildcard][p0]") {
    WildcardMatcher matcher;

    // Invalid regex pattern - should log warning, not crash
    CHECK_FALSE(matcher.match("[invalid", "test_string"));

    // Verify warning is logged (check stderr or DPRINTF output)
    // The key behavior: no exception propagates, returns false
}
```

- [ ] **Step 3: Run test to verify current behavior (catch block swallows error)**

```bash
./build/bin/cpptlm_tests "[wildcard]" --output-on-failure
```

- [ ] **Step 4: Fix the catch block**

Edit `include/utils/wildcard.hh` at line 28:

**BEFORE:**
```cpp
} catch (...) {
    return pattern == str;
}
```

**AFTER:**
```cpp
} catch (const std::regex_error& e) {
    DPRINTF(WILDCARD, "[WARN] Invalid regex pattern '%s': %s\n",
            pattern_str.c_str(), e.what());
    return false;
} catch (const std::exception& e) {
    DPRINTF(WILDCARD, "[WARN] Unexpected error in wildcard match: %s\n",
            e.what());
    return false;
}
```

- [ ] **Step 5: Verify fix with test**

```bash
./build/bin/cpptlm_tests "[wildcard]" --output-on-failure
```

Expected: PASS - invalid regex returns false instead of swallowing exception

- [ ] **Step 6: Commit**

```bash
git add include/utils/wildcard.hh
git commit -m "fix(wildcard): log regex errors instead of swallowing

catch(...) 静默吞噬 std::regex_error。
添加 DPRINTF 日志输出具体错误信息。
Refs: include/utils/wildcard.hh:28"
```

---

## Execution Checkpoints

- [ ] P0.1: PortPair memory leak fixed + ASan verified clean
- [ ] P0.2: Dead Python files deleted + no import errors from removal
- [ ] P0.3: wildcard catch block fixed + wildcard tests pass
- [ ] All 3 tasks complete + full test suite passing

---

## Dependencies

```
P0.1 ──┐
P0.2 ──┤──► All independent, execute in parallel
P0.3 ──┘
```

---

## Validation Commands

```bash
# Full build with ASan
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON -DUSE_SYSTEMC=OFF
cmake --build build -j$(nproc)

# Run all tests
ctest --test-dir build --output-on-failure

# Module-specific tests
./build/bin/cpptlm_tests "[phase6]"      # P0.1 verification
./build/bin/cpptlm_tests "[wildcard]"    # P0.3 verification
```
