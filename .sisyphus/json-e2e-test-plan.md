# JSON Config-Based E2E Test Plan

**Generated:** 2026-04-14  
**Commit:** 5f168db — Phase 6 End-to-End Integration  
**Branch:** main  
**Version:** 1.0.0

---

## Executive Summary

**User Request:** Create a focused plan for JSON config-based E2E testing that:
1. Loads real JSON files from `configs/`
2. Instantiates modules via `ModuleFactory::instantiateAll()`
3. Runs simulation via `EventQueue::run(N)`
4. Verifies modules created AND simulation advanced

**Key Finding:** The existing Phase 6 test (`test_phase6_integration.cc`) uses **inline JSON strings**, not file-based configs. This plan bridges that gap by creating file-based E2E tests.

---

## 1. Architecture Decision: ChStream Modules (Option B)

**Decision:** Target **ChStream modules** (CacheTLM, CrossbarTLM, MemoryTLM), NOT Legacy modules.

### Rationale

| Aspect | Legacy (Option A) | ChStream (Option B) |
|--------|-------------------|---------------------|
| **Module Types** | CPUSim, CacheSim, MemorySim | CacheTLM, CrossbarTLM, MemoryTLM |
| **Port Type** | Direct MasterPort/SlavePort | ChStreamInitiatorPort/ChStreamTargetPort |
| **StreamAdapter** | Not required | Required (injected in Step 7) |
| **Test Location** | test_connection_resolution.cc (legacy) | test_phase6_integration.cc (modern) |
| **Future-Proof** | ❌ Deprecated modules | ✅ V2.1 hybrid architecture |
| **JSON Schema** | Simple connections | Port index syntax `"xbar.0"` |

**Conclusion:** ChStream modules are architecturally correct. The `configs/cache_chstream_test.json` and `configs/crossbar_test.json` files already use ChStream module types.

---

## 2. Config File Inventory

### 2.1 Files Analyzed

| File | Module Types | Connections | Loadable? |
|------|--------------|-------------|-----------|
| `cache_chstream_test.json` | CacheTLM, MemoryTLM | cache → mem | ✅ YES (19 lines) |
| `crossbar_test.json` | CacheTLM, CrossbarTLM, MemoryTLM | cache → xbar.0 → mem | ✅ YES (13 lines) |
| `noc_mesh.json` | Router, CPUSim | 4 CPUs + 4 Routers (mesh) | ⚠️ Legacy types |
| `ring_bus.json` | Crossbar, CPUSim, CacheSim, MemorySim | Hub + ring | ⚠️ Legacy types |
| `cpu_group.json` | Not analyzed | Group syntax | ⚠️ Needs verification |
| `cpu_simple.json` | Not analyzed | Simple CPU | ⚠️ Needs verification |
| `base.json` | Not analyzed | Minimal | ⚠️ Needs verification |

### 2.2 Recommended Test Configs

**Primary:** `configs/crossbar_test.json`
- Complete ChStream topology (Cache → Crossbar → Memory)
- Uses port index syntax (`xbar.0`)
- Tests multi-module routing

**Secondary:** `configs/cache_chstream_test.json`
- Minimal two-module test
- Good for basic injection verification

---

## 3. ModuleFactory Instantiation Flow

### Step-by-Step Analysis (from `src/core/module_factory.cc`)

```
1. Load JSON config (JsonIncluder::loadAndInclude)
2. Load plugins (PluginLoader)
3. Create module instances (Step 2)
   - Look up type in SimModule registry
   - Fall back to SimObject registry
   - Store in object_instances map
4. Process groups (Step 3)
5. Instantiate internal configs (Step 4)
6. Resolve connections (Step 5)
   - ConnectionResolver::resolveConnections()
   - Create PortPairs
7. Build physical connections (Step 6)
   - Parse port specs (parsePortSpec)
   - Link MasterPort → SlavePort
8. Inject ChStream adapters (Step 7) ⭐ CRITICAL
   - Create StreamAdapter for each ChStreamModuleBase
   - Create ChStream ports (req_out, resp_in, req_in, resp_out)
   - Call set_stream_adapter()
   - Create PortPairs for ChStream connections
9. Save instances (this.instances = object_instances)
```

### Critical: Step 7 ChStream Injection

For each ChStream module:
1. Look up adapter factory (`ChStreamAdapterFactory::get()`)
2. Check if multi-port (`isMultiPort(type)`)
3. Create appropriate adapter (SinglePort / MultiPort / DualPort)
4. Create N sets of ChStream ports
5. Bind ports to adapter
6. Call `ch_mod->set_stream_adapter(adapter)`

### Acceptance Criteria for "Simulation Ran"

Minimal criteria:
1. ✅ All modules instantiated (factory.getInstance(name) != nullptr)
2. ✅ ChStream adapters injected (dynamic_cast<ChStreamModuleBase*> succeeds)
3. ✅ Simulation advanced (eq.run(N) completes without crash)
4. ✅ Cycle count increased (eq.getCurrentCycle() > 0 after run)

Enhanced criteria (Phase 6 style):
5. ✅ Module types verified (get_module_type() returns expected)
6. ✅ Crossbar port count correct (num_ports() == 4)
7. ✅ Routing logic works (route_address() returns correct port)

---

## 4. Test Strategy (TDD-Oriented)

### Test File: `test/test_json_config_e2e.cc`

**Tags:** `[e2e][config][chstream][phase6]`

**Test Cases:**

```cpp
TEST_CASE("E2E: Load cache_chstream_test.json", "[e2e][config]") {
    // 1. Load JSON file
    // 2. Instantiate via ModuleFactory
    // 3. Verify modules exist
    // 4. Run simulation
    // 5. Verify cycle advanced
}

TEST_CASE("E2E: Load crossbar_test.json", "[e2e][config]") {
    // 1. Load JSON file (port index syntax)
    // 2. Verify 3 modules created
    // 3. Verify ChStream adapters injected
    // 4. Verify CrossbarTLM.num_ports() == 4
    // 5. Verify routing logic
    // 6. Run 50 cycles
}

TEST_CASE("E2E: EventQueue.run() advances cycle", "[e2e][eventqueue]") {
    // 1. Create EventQueue
    // 2. Record initial cycle
    // 3. Run N cycles
    // 4. Verify final_cycle == initial + N
}
```

---

## 5. Task Dependency Graph

| Task | Depends On | Reason |
|------|------------|--------|
| Task 1: Analyze config schemas | None | Foundation work |
| Task 2: Create test file structure | Task 1 | Need to know which configs to test |
| Task 3: Implement E2E test cases | Task 2 | Tests require file structure |
| Task 4: Add config file verification helper | Task 3 | Helper used by tests |
| Task 5: Run tests and verify | Task 4 | Execute after implementation |
| Task 6: Document findings | Task 5 | Write report after results |

---

## 6. Parallel Execution Graph

```
Wave 1 (Start immediately):
├── Task 1: Analyze config schemas (no dependencies)
└── Task 4: Create EventQueue helper test (no dependencies)

Wave 2 (After Wave 1):
├── Task 2: Create test file structure (depends: Task 1)
└── Task 3: Implement E2E test cases (depends: Task 1)

Wave 3 (After Wave 2):
└── Task 5: Run tests and verify (depends: Task 2, Task 3, Task 4)

Wave 4 (After Wave 3):
└── Task 6: Document findings (depends: Task 5)

Critical Path: Task 1 → Task 2 → Task 5 → Task 6
Estimated Parallel Speedup: 33% faster than sequential
```

---

## 7. Tasks

### Task 1: Analyze Config Schemas

**Description:** Verify which `configs/*.json` files use valid ChStream module types and correct schema.

**Delegation Recommendation:**
- Category: `quick` - Single-file analysis, pattern matching
- Skills: [] - No specialized skills needed for JSON schema verification

**Skills Evaluation:**
- ❌ OMITTED `planning-with-files`: Overkill for simple JSON analysis
- ❌ OMITTED `cpp-pro`: No C++ code analysis required
- ✅ N/A: This is a reconnaissance task

**Depends On:** None

**Acceptance Criteria:**
- [ ] List all 7 config files with module types
- [ ] Mark which are ChStream-compatible (CacheTLM, CrossbarTLM, MemoryTLM)
- [ ] Identify minimum 2 loadable configs for testing

---

### Task 2: Create Test File Structure

**Description:** Create `test/test_json_config_e2e.cc` with Catch2 test harness.

**Delegation Recommendation:**
- Category: `quick` - Single file creation, standard test patterns
- Skills: [] - Follows existing test conventions

**Skills Evaluation:**
- ❌ OMITTED `test-driven-development`: Plan already implements TDD approach
- ✅ OMITTED `cpp-pro`: Basic test file, no advanced C++ patterns needed

**Depends On:** Task 1

**Acceptance Criteria:**
- [ ] File compiles without errors
- [ ] Includes all required headers (catch2, module_factory, event_queue)
- [ ] Has 3 TEST_CASE blocks matching Section 5
- [ ] Uses correct tags `[e2e][config][chstream]`

---

### Task 3: Implement E2E Test Cases

**Description:** Write actual test logic that loads JSON files and verifies simulation.

**Delegation Recommendation:**
- Category: `cpp` - C++ test implementation with framework knowledge
- Skills: [`cpp-pro`] - Modern C++ patterns, RAII, smart pointers for cleanup

**Skills Evaluation:**
- ✅ INCLUDED `cpp-pro`: Ensures modern C++17/20 patterns, proper resource management
- ❌ OMITTED `cpp-debug`: No debugging needed yet (pure implementation)
- ❌ OMITTED `cpp-architecture`: Single-file scope, no architecture analysis needed

**Depends On:** Task 1

**Acceptance Criteria:**
- [ ] `cache_chstream_test.json` test passes (verifies 2 modules, runs 20 cycles)
- [ ] `crossbar_test.json` test passes (verifies 3 modules, port index syntax, runs 50 cycles)
- [ ] EventQueue cycle advancement test passes
- [ ] All tests use REQUIRE (not CHECK) for critical assertions

---

### Task 4: Create EventQueue Helper Test

**Description:** Implement isolated EventQueue::run() verification as a sanity check.

**Delegation Recommendation:**
- Category: `quick` - Single small test case
- Skills: [] - Straightforward cycle counting

**Skills Evaluation:**
- ❌ OMITTED all skills: Trivial single-function test

**Depends On:** None

**Acceptance Criteria:**
- [ ] Test verifies eq.getCurrentCycle() returns 0 initially
- [ ] Test verifies eq.run(50) advances cycle to exactly 50
- [ ] Test completes in < 1 second

---

### Task 5: Run Tests and Verify

**Description:** Execute tests via ctest, capture output, identify failures.

**Delegation Recommendation:**
- Category: `quick` - Command execution and output analysis
- Skills: [] - Standard ctest workflow

**Skills Evaluation:**
- ❌ OMITTED `cpp-debug`: Only if tests fail (will create follow-up task)
- ❌ OMITTED all skills: Execution task, no specialization needed

**Depends On:** Task 2, Task 3, Task 4

**Acceptance Criteria:**
- [ ] All 3 new tests pass: `ctest -R "json_config_e2e" --output-on-failure`
- [ ] Capture test output to `plans/e2e-test-results.md`
- [ ] Document any failures with stack traces

---

### Task 6: Document Findings

**Description:** Write summary report with recommendations for config-based testing.

**Delegation Recommendation:**
- Category: `writing` - Technical documentation
- Skills: [] - Clear prose, structured report

**Skills Evaluation:**
- ✅ INCLUDED: None needed (general writing task)
- ❌ OMITTED `obsidian-markdown`: Standard Markdown sufficient (no Obsidian-specific features)

**Depends On:** Task 5

**Acceptance Criteria:**
- [ ] Report saved to `docs/testing/e2e-config-testing.md`
- [ ] Includes list of loadable config files
- [ ] Includes test execution time and coverage
- [ ] Recommends which configs to use for future E2E tests

---

## 8. Commit Strategy

**Atomic commits following zero-debt principle:**

1. **Commit 1:** `test: Add JSON config E2E test file` (Tasks 2-4)
   - New file: `test/test_json_config_e2e.cc`
   - No modification to existing files
   - Test-only changes (safe to revert)

2. **Commit 2:** `test: Verify E2E tests pass with crossbar_test.json` (Task 5)
   - Update tests if needed based on execution results
   - Add any missing assertions from findings
   - Only test changes

3. **Commit 3:** `docs: Add E2E config testing guide` (Task 6)
   - New file: `docs/testing/e2e-config-testing.md`
   - Documentation only

**Safety:** All commits are test/docs-only. No production code changes.

---

## 9. Success Criteria

**Final Verification:**

```bash
# 1. Build succeeds
cmake --build build

# 2. Run specific E2E tests
./build/bin/cpptlm_tests "[e2e][config]"

# 3. All new tests pass (expected: 3 tests, ~15 assertions)
ctest -R "json_config_e2e" --output-on-failure

# 4. No regression in existing Phase 6 tests
./build/bin/cpptlm_tests "[phase6]"
```

**Exit Conditions:**
- ✅ All 3 new tests pass
- ✅ No existing tests fail (zero regression)
- ✅ Documentation complete
- ✅ Config file inventory documented

---

## 10. Risk Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| Config schema mismatch | High | Task 1 identifies incompatible configs upfront |
| ChStream injection fails | High | Follow Phase 6 test pattern (inline JSON proven to work) |
| Port index syntax error | Medium | Use exact same syntax as `crossbar_test.json` |
| EventQueue run() blocks | Low | Set timeout (30s max per test) |

---

## 11. Appendix: Minimal Test Skeleton

```cpp
// test/test_json_config_e2e.cc
#include <catch2/catch_all.hpp>
#include "core/module_factory.hh"
#include "core/event_queue.hh"
#include "utils/json_includer.hh"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

static json loadConfigFile(const std::string& path) {
    std::ifstream f(path);
    REQUIRE(f.is_open());
    return json::parse(f);
}

TEST_CASE("E2E: Load and instantiate cache_chstream_test.json", "[e2e][config]") {
    EventQueue eq;
    ModuleFactory factory(&eq);
    
    auto config = loadConfigFile("configs/cache_chstream_test.json");
    factory.instantiateAll(config);
    
    REQUIRE(factory.getInstance("cache") != nullptr);
    REQUIRE(factory.getInstance("mem") != nullptr);
    
    // Run simulation
    uint64_t initial_cycle = eq.getCurrentCycle();
    eq.run(20);
    REQUIRE(eq.getCurrentCycle() > initial_cycle);
}

TEST_CASE("E2E: Load and instantiate crossbar_test.json", "[e2e][config]") {
    EventQueue eq;
    ModuleFactory factory(&eq);
    
    auto config = loadConfigFile("configs/crossbar_test.json");
    factory.instantiateAll(config);
    
    REQUIRE(factory.getInstance("cache") != nullptr);
    REQUIRE(factory.getInstance("xbar") != nullptr);
    REQUIRE(factory.getInstance("mem") != nullptr);
    
    // Verify ChStream and routing
    auto* xbar = dynamic_cast<CrossbarTLM*>(factory.getInstance("xbar"));
    REQUIRE(xbar != nullptr);
    REQUIRE(xbar->num_ports() == 4);
    
    eq.run(50);
}
```

---

**END OF PLAN**
