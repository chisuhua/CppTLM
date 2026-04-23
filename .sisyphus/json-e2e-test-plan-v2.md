# Plan: JSON Config E2E Test + Simulation Execution Fix

**Created:** 2026-04-14
**Author:** Sisyphus (Orchestrator)
**Type:** TDD Implementation Plan
**Status:** Draft

---

## Executive Summary

**Problem:**
1. Existing tests only verify object existence, never call `event_queue->run()`
2. Cycle counter never advances
3. Module communication is never exercised
4. No test loads external JSON config files from `configs/`

**Goal:**
1. Create new E2E test that loads `configs/crossbar_test.json` and runs real simulation
2. Fix existing tests to call `eq.run(N)` and verify cycle advancement
3. Add communication verification (not just object existence)

---

## Investigation Findings

### Finding 1: Phase 6 tests don't run simulation

```cpp
// test_phase6_integration.cc line 42
factory.startAllTicks();
// Missing: eq.run(50) - cycle counter never advances!
```

Only 2 out of 9 test cases call `eq.run()`:
- Line 66: `eq.run(50)` - but no assertion on cycle advancement
- Line 106: `eq.run(20)` - but no assertion

### Finding 2: No test loads external JSON files

All Phase 6 tests use inline JSON strings:
```cpp
json config = R"({...})"_json;  // Not from file
```

### Finding 3: Config files are loadable

| File | Modules | Status |
|------|---------|--------|
| `crossbar_test.json` | CacheTLM, CrossbarTLM, MemoryTLM | ✅ Loadable |
| `cache_chstream_test.json` | CacheTLM, MemoryTLM | ✅ Loadable |

---

## Test Plan

### Phase 1: Create New JSON Config E2E Test

**File:** `test/test_json_config_e2e.cc`

| Step | Action | Description |
|------|--------|-------------|
| 1.1 | Create test file | New file with 3 test cases |
| 1.2 | Load crossbar_test.json | Verify 3 modules created |
| 1.3 | Run simulation | `eq.run(50)` + verify cycle advancement |
| 1.4 | Load cache_chstream_test.json | Verify 2 modules, run 20 cycles |

**Tags:** `[e2e][config][chstream]`

### Phase 2: Fix Existing Phase 6 Tests

**File:** `test/test_phase6_integration.cc`

| Step | Test Case | Fix |
|------|-----------|-----|
| 2.1 | "Phase 6: Full integration" | Add cycle advancement assertion after `eq.run(50)` |
| 2.2 | "Multi-port Crossbar" | Add cycle advancement assertion after `eq.run(20)` |
| 2.3 | Remove empty test case | Line 239 has empty body |

### Phase 3: Add Communication Verification Tests

**New File:** `test/test_simulation_communication.cc`

| Step | Test | Description |
|------|------|-------------|
| 3.1 | EventQueue cycle test | Verify `getCurrentCycle()` advances after `run(N)` |
| 3.2 | Cache→Memory communication | Send request, verify response received |
| 3.3 | Crossbar routing + communication | Send request through xbar, verify reaches memory |

**Tags:** `[e2e][simulation][chstream]`

---

## Task List

- [ ] Phase 1.1: Create `test/test_json_config_e2e.cc`
- [ ] Phase 1.2: Test loading `crossbar_test.json`
- [ ] Phase 1.3: Verify cycle advancement
- [ ] Phase 1.4: Test loading `cache_chstream_test.json`
- [ ] Phase 2.1: Fix full integration test cycle assertion
- [ ] Phase 2.2: Fix multi-port test cycle assertion
- [ ] Phase 2.3: Remove empty test case at line 239
- [ ] Phase 3.1: Create EventQueue cycle test
- [ ] Phase 3.2: Create Cache→Memory communication test
- [ ] Phase 3.3: Create Crossbar routing + communication test
- [ ] Run all tests, verify pass

---

## Acceptance Criteria

1. ✅ New `test_json_config_e2e.cc` loads external JSON files
2. ✅ `configs/crossbar_test.json` creates 3 modules
3. ✅ `eq.getCurrentCycle()` advances after `run(N)` (verified in 3+ tests)
4. ✅ Cache→Memory communication verified by response receipt
5. ✅ All existing `[phase6]` tests still pass (no regression)
6. ✅ Zero new `.disabled` files

---

## Commit Strategy (Atomic)

1. `test: Add JSON config E2E test file` — Phase 1
2. `test: Fix Phase 6 simulation execution` — Phase 2
3. `test: Add communication verification tests` — Phase 3

---

## Execution Commands

```bash
# Build
cmake --build build --target cpptlm_tests

# Run E2E config tests
./build/bin/cpptlm_tests "[e2e][config]"

# Run Phase 6 tests
./build/bin/cpptlm_tests "[phase6]"

# Run communication tests
./build/bin/cpptlm_tests "[e2e][simulation]"

# Run all
./build/bin/cpptlm_tests
```

---

## Appendix: Minimal Test Skeleton

```cpp
// test/test_json_config_e2e.cc
#include <catch2/catch_all.hpp>
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

static json loadConfig(const std::string& path) {
    std::ifstream f(path);
    REQUIRE(f.is_open());
    return json::parse(f);
}

TEST_CASE("E2E: Load crossbar_test.json", "[e2e][config][chstream]") {
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);
    
    auto config = loadConfig("configs/crossbar_test.json");
    factory.instantiateAll(config);
    factory.startAllTicks();
    
    REQUIRE(factory.getInstance("cache") != nullptr);
    REQUIRE(factory.getInstance("xbar") != nullptr);
    REQUIRE(factory.getInstance("mem") != nullptr);
    
    uint64_t before = eq.getCurrentCycle();
    eq.run(50);
    REQUIRE(eq.getCurrentCycle() == before + 50);
}
```

---

**END OF PLAN**
