# Plan: Restore Disabled Config Loader Test + Create E2E Configuration Test

**Created:** 2026-04-14  
**Author:** Prometheus  
**Type:** TDD Implementation Plan with Atomic Commits  
**Exec Mode:** Ultrawork-compatible

---

## Summary

**Objective:** 
1. Fix and re-enable `test_config_loader.cc.disabled` 
2. Create new JSON file-based end-to-end integration test
3. Verify simulation actually executes (not just config parsing)

**Current State:**
- 4 disabled tests archived in `docs-archived/disabled-tests/`
- `test_config_loader.cc.disabled` has API mismatches (deprecated `input_buffer_sizes`, wrong port types)
- `configs/` has 17 JSON files NOT exercised by tests
- Phase 6 test uses hardcoded JSON, not file loading

**Target State:**
- ✅ `test/test_config_loader.cc` compiles and passes
- ✅ `test/test_e2e_config_runner.cc` created with real JSON file loading
- ✅ All acceptance criteria verified

---

## Research-Based Best Practices (from librarian)

### Catch2 Testing Patterns

1. **Use `[.disabled]` tag** for tests that need fixes (instead of `.cc.disabled` file extension)
2. **Tag-based filtering**: `./cpptlm_tests "[config]"` or `./cpptlm_tests "~[.disabled]"`
3. **Runtime skipping** for dynamic conditions using `SKIP()` macro

### Simulation Verification Anti-Patterns

**❌ BAD (config-only validation):**
```cpp
TEST_CASE("Bad test") {
    Config c;
    REQUIRE(c.load("config.json"));  // Only validates JSON syntax!
}
```

**✅ GOOD (execution verification):**
```cpp
TEST_CASE("Good test") {
    Simulation sim("config.json");
    auto stats = sim.run();
    
    REQUIRE(stats.configLoaded);
    REQUIRE(stats.simulationTime > 0);    // Time advanced
    REQUIRE(stats.eventsProcessed > 0);   // Events processed
    REQUIRE(stats.transactions > 0);      // Transactions occurred
}
```

### Recommended Verification Metrics for CppTLM

```cpp
eq.run(100);
REQUIRE(eq.getCurrentCycle() == 100);           // Cycle counter advanced
REQUIRE(factory.getInstanceCount() > 0);        // Modules created
// Optional: Check module-specific stats if available
```

---

## Investigation Findings (from background research)

### Disabled Test Issues Identified

1. **API Mismatch - JSON Schema:**
   - OLD: `input_buffer_sizes`, `output_buffer_sizes`, `vc_priorities`
   - Current: Need to verify actual field names in `ModuleFactory::instantiateAll()`

2. **Port Type Deprecation:**
   - OLD: `DownstreamPort<MockSim>`, `UpstreamPort<MockSim>`
   - Current: `ChStreamInitiatorPort`, `ChStreamTargetPort` (namespace `cpptlm::`)

3. **Event Scheduling:**
   - Disabled test uses `LambdaEvent` class
   - Need to verify if this exists or needs replacement

4. **Missing Files:**
   - Test file is in `docs-archived/disabled-tests/` (must move to `test/`)

### Working Reference: Phase 6 Integration Test

```cpp
// From test_phase6_integration.cc (working):
factory.instantiateAll(config);  // Uses inline JSON
factory.startAllTicks();
eq.run(50);
REQUIRE(factory.getInstance("cache") != nullptr);
```

---

## Implementation Steps

### Phase 1: Fix test_config_loader.cc

| Step | Action | Commit |
|------|--------|--------|
| 1.1 | Move `.disabled` file to `test/` directory | `chore: Move test_config_loader.cc from archived to test/` |
| 1.2 | Fix MockSim callback signatures | `fix(test): Update MockSim callback to match current SimObject API` |
| 1.3 | Update JSON schema (field names) | `fix(test): Update JSON config schema to match current API` |
| 1.4 | Replace `DownstreamPort` with current port types | `fix(test): Use ChStreamInitiatorPort instead of deprecated DownstreamPort` |
| 1.5 | Fix delay injection API | `fix(test): Update setDelay() calls to match current MasterPort API` |
| 1.6 | Verify compilation | `build: Verify test_config_loader.cc compiles` |
| 1.7 | Run tests, verify pass | `test: Verify test_config_loader.cc passes` |

### Phase 2: Create test_e2e_config_runner.cc

| Step | Action | Commit |
|------|--------|--------|
| 2.1 | Select JSON configs to test (cache_chstream_test.json, cpu_simple.json) | `test: Add JSON config files as test resources` |
| 2.2 | Create test with file loading helper | `test: Add test_e2e_config_runner.cc with JSON file loading` |
| 2.3 | Add module instantiation verification | `test: Add module existence assertions to E2E test` |
| 2.4 | Add simulation execution verification (cycle counter) | `test: Add cycle counter verification to prove simulation runs` |
| 2.5 | Add multiple config file tests | `test: Expand E2E test to cover multiple JSON configs` |

### Phase 3: Verification & Documentation

| Step | Action | Commit |
|------|--------|--------|
| 3.1 | Run all `[config]` tests | `ci: Verify [config] tests pass` |
| 3.2 | Run all `[e2e]` tests | `ci: Verify [e2e] tests pass` |
| 3.3 | Run full test suite (regression check) | `ci: Verify no regression in existing tests` |
| 3.4 | Update disabled-tests README | `docs: Remove test_config_loader from disabled list` |
| 3.5 | Update test/AGENTS.md | `docs: Add test_e2e_config_runner.cc to test inventory` |

---

## Test Structure (E2E Test Template)

```cpp
// test/test_e2e_config_runner.cc

#include <catch2/catch_all.hpp>
#include "core/module_factory.hh"
#include "core/event_queue.hh"
#include "chstream_register.hh"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Helper: Load JSON file from configs/
static json loadConfig(const std::string& filename) {
    std::ifstream f("configs/" + filename);
    REQUIRE(f.is_open());
    return json::parse(f);
}

TEST_CASE("E2E: Cache→Crossbar→Memory from JSON file", "[e2e][config]") {
    registerChStreamModules();
    
    EventQueue eq;
    ModuleFactory factory(&eq);
    
    json config = loadConfig("cache_chstream_test.json");
    factory.instantiateAll(config);
    factory.startAllTicks();
    
    // Verify modules created (not just config parsed)
    REQUIRE(factory.getInstance("cache") != nullptr);
    REQUIRE(factory.getInstance("xbar") != nullptr);
    REQUIRE(factory.getInstance("mem") != nullptr);
    
    // Run simulation - VERIFY ACTUAL EXECUTION
    uint64_t start_cycle = eq.getCurrentCycle();
    eq.run(100);
    
    // CRITICAL: Prove simulation ran, not just setup
    REQUIRE(eq.getCurrentCycle() == start_cycle + 100);  // Time advanced
    REQUIRE(start_cycle >= 0);  // Sanity check
}

TEST_CASE("E2E: CPU Simple Configuration", "[e2e][config]") {
    registerChStreamModules();
    
    EventQueue eq;
    ModuleFactory factory(&eq);
    
    json config = loadConfig("cpu_simple.json");
    factory.instantiateAll(config);
    factory.startAllTicks();
    
    // Verify all modules from config exist
    REQUIRE(factory.getInstance("cpu") != nullptr);
    REQUIRE(factory.getInstance("mem") != nullptr);
    
    // Verify simulation executed
    eq.run(50);
    REQUIRE(eq.getCurrentCycle() == 50);
}
```

---

## Acceptance Criteria

### Functional (Must All Pass)

- [ ] `test_config_loader.cc` compiles without errors
- [ ] All `[config]` tag tests pass
- [ ] New `[e2e]` tests exist and pass
- [ ] Tests load JSON from `configs/` directory (file I/O, not hardcoded)
- [ ] **Simulation execution verified** using ALL of these metrics:
  - [ ] Cycle counter advances (`eq.getCurrentCycle() > start_cycle`)
  - [ ] Modules instantiated (`getInstance()` returns non-null)
  - [ ] At least one tick executed (if tick counter available)
- [ ] No regression in `[phase6]` tests
- [ ] Test uses proper Catch2 tags (`[e2e]`, `[config]`, no `[.disabled]`)

### Documentation

- [ ] `docs-archived/disabled-tests/README.md` updated (remove restored test)
- [ ] `test/AGENTS.md` updated (add new test to inventory)
- [ ] No new `.disabled` files created (zero debt)
- [ ] Test includes comments explaining verification strategy

---

## Execution Commands

### Build & Run

```bash
# Build
cmake --build build --target cpptlm_tests

# Run config tests only
./build/bin/cpptlm_tests "[config]" --success

# Run E2E tests only
./build/bin/cpptlm_tests "[e2e]" --success

# Run all tests
./build/bin/cpptlm_tests

# CTest alternative
ctest --test-dir build -R "config|e2e" --output-on-failure
```

### Verification

```bash
# Test exists in binary
./build/bin/cpptlm_tests --list-tests | grep -E "config|e2e"

# Check compilation
cmake --build build --target cpptlm_tests 2>&1 | grep -i error || echo "OK"
```

---

## Commit Strategy (Atomic, 15 total)

1. `chore: Move test_config_loader.cc from archived to test/`
2. `fix(test): Update MockSim callback signatures`
3. `fix(test): Update JSON config schema`
4. `fix(test): Replace DownstreamPort with ChStreamInitiatorPort`
5. `fix(test): Update delay injection API`
6. `build: Verify test_config_loader.cc compiles`
7. `test: Verify test_config_loader.cc passes`
8. `test: Add test_e2e_config_runner.cc with JSON file loading`
9. `test: Add module existence assertions`
10. `test: Add cycle counter verification`
11. `test: Expand E2E to multiple JSON configs`
12. `ci: Verify [config] tests pass`
13. `ci: Verify [e2e] tests pass`
14. `ci: Verify no regression in existing tests`
15. `docs: Update test documentation and remove restored test from disabled list`

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| API too deprecated to fix easily | Create new test from scratch using Phase 6 as template |
| JSON configs have schema issues | Fix config files as needed, test with minimal config first |
| ChStream ports complex for mocks | Use simple mock pattern from working Phase 6 test |

---

## TDD Workflow

For each step: **Red** (write test) → **Green** (make pass) → **Refactor** (clean up) → **Verify** (no regression)

---

## Handoff Instructions

**Next Agent:** Implementation agent (Hephaestus/Atlas)

**Actions:**
1. Read this plan file
2. Start with Phase 1, Step 1.1
3. Follow atomic commit strategy
4. Report progress after each commit
5. Flag any blockers immediately

**Todo List:** Use todowrite tool to track each step above

---

**Status:** ✅ Ready for implementation  
**Estimated Time:** ~3 hours (4 sessions: investigation, fix, create, verify)
