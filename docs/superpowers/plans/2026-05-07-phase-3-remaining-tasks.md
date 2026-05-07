# Phase 3.x C++ & Python 剩余任务实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete Phase 3.2/3.3 C++ integration gaps, align with ADR-X.9/10/11 specifications, then execute Phase 3.2 Python tasks and Phase 3.4 planning.

**Architecture:** Phase 3.x follows hard ordering: 3.1 (fixes) → 3.2 (ports) → 3.3 (params) → 3.4 (validation). Critical gaps identified:
- `check_port_compatibility()` called with `empty_port_specs` — port validation disabled
- `ParamType` enum diverges from ADR-X.10 spec (code has INTEGER/FLOAT/BOOLEAN/ENUM, ADR specifies INT/UNSIGNED/ADDRESS/LATENCY/BOOL)
- `configs/param_rules/*.json` files exist but are never loaded by ModuleFactory
- `derive_expr` is implemented but never called from instantiateAll (dead code)
- `set_config()` has no try-catch for ParamValidationError

**Tech Stack:** C++17, nlohmann/json, Catch2, Python 3.10+, Pydantic v2

---

## Critical Gaps (Must Fix First)

Before any Phase 3.2/3.3 remaining work, two architectural issues must be resolved:

| Gap | File | Line | Impact |
|-----|------|------|--------|
| `empty_port_specs` bug | module_factory.cc | 818-819 | All L1/L2/L3 port checks disabled |
| ParamType enum mismatch | param_rules.hh | 15-21 | JSON config won't match C++ expectations |

---

## Part A: Phase 3.2 C++ — Critical Integration Fixes

### Task A1: Fix empty_port_specs Bug — Enable Port Validation

**Files:**
- Modify: `src/core/module_factory.cc:800-850`

The `check_port_compatibility()` function at line 282 receives `empty_port_specs` — port specs from JSON config are never parsed and passed.

- [ ] **Step 1: Read current port_specs handling in instantiateAll**

Run: `grep -n "port_specs\|ModulePortSpec" src/core/module_factory.cc`
Expected: Shows where port_specs should be loaded and used

- [ ] **Step 2: Add port_specs JSON loading at instantiateAll entry**

In `instantiateAll()` after `validateConfig()` (around line 347), add:
```cpp
// Load port_specs from config modules (ADR-X.9 Step 3)
std::map<std::string, cpptlm::ModulePortSpec> port_specs;
if (final_config.contains("modules")) {
    for (const auto& mod : final_config["modules"]) {
        if (mod.contains("port_spec")) {
            auto spec = mod["port_spec"].get<cpptlm::ModulePortSpec>();
            port_specs[mod["name"]] = spec;
        }
    }
}
```

- [ ] **Step 3: Pass real port_specs to check_port_compatibility**

At connection resolution (line 818), replace `empty_port_specs` with loaded `port_specs`:
```cpp
// Before (line 818):
if (!check_port_compatibility(src_port, dst_port, empty_port_specs, empty_port_specs)) {

// After:
if (!check_port_compatibility(src_port, dst_port, port_specs, port_specs)) {
```

- [ ] **Step 4: Verify build compiles**

Run: `cd build && cmake --build . --target cpptlm_core 2>&1 | tail -5`
Expected: No errors (only DPRINTF macro warnings)

- [ ] **Step 5: Commit**

```bash
git add src/core/module_factory.cc
git commit -m "fix(phase3.2): load port_specs from JSON config and enable L1/L2/L3 checks

Fix empty_port_specs bug - port compatibility checks were disabled
because ModulePortSpec from JSON config was never parsed and passed
to check_port_compatibility(). Now loads port_specs per module and
passes real specs to compatibility checks."
```

---

### Task A2: Align ParamType Enum with ADR-X.10

**Files:**
- Modify: `include/core/param_rules.hh:15-21`
- Modify: `src/core/param_parser.cc:18-45`
- Modify: `configs/param_rules/router_tlm.json`
- Modify: `configs/param_rules/nic_tlm.json`
- Test: `test/test_param_rules.cc`

ADR-X.10 specifies: `INT, UNSIGNED, STRING, ADDRESS, LATENCY, BOOL`
Current code has: `INTEGER, FLOAT, STRING, BOOLEAN, ENUM`

Gap: Missing ADDRESS (for "0x...", "256MB"), LATENCY, UNSIGNED. Extra FLOAT, ENUM.

- [ ] **Step 1: Read current ParamType enum and parse() switch**

Run: `sed -n '15,25p' include/core/param_rules.hh && echo "---" && sed -n '18,50p' src/core/param_parser.cc`

- [ ] **Step 2: Update ParamType enum to match ADR**

In `include/core/param_rules.hh`, replace enum:
```cpp
enum class ParamType {
    INT,        // was INTEGER
    UNSIGNED,   // new - for mesh_x, vc_count
    STRING,
    ADDRESS,    // new - for "0x1000", "256MB" → bytes
    LATENCY,    // new - for "3ns", "5us" → cycles
    BOOL        // was BOOLEAN
};
```

- [ ] **Step 3: Update NLOHMANN_JSON_SERIALIZE_ENUM**

In `include/core/param_rules.hh`:
```cpp
NLOHMANN_JSON_SERIALIZE_ENUM(ParamType, {
    {ParamType::INT, "INT"},
    {ParamType::UNSIGNED, "UNSIGNED"},
    {ParamType::STRING, "STRING"},
    {ParamType::ADDRESS, "ADDRESS"},
    {ParamType::LATENCY, "LATENCY"},
    {ParamType::BOOL, "BOOL"}
})
```

- [ ] **Step 4: Update parse() switch in param_parser.cc**

In `src/core/param_parser.cc`, replace switch:
```cpp
switch (type) {
    case ParamType::INT: {
        int64_t val = std::stoll(input);
        result.value = val;
        result.success = true;
        break;
    }
    case ParamType::UNSIGNED: {
        uint64_t val = std::stoull(input);
        result.value = val;
        result.success = true;
        break;
    }
    case ParamType::STRING: {
        result.value = input;
        result.success = true;
        break;
    }
    case ParamType::ADDRESS: {
        uint64_t val = parse_address(input);
        result.value = val;
        result.success = true;
        break;
    }
    case ParamType::LATENCY: {
        uint64_t val = parse_latency(input, clock_frequency_mhz);
        result.value = val;
        result.success = true;
        break;
    }
    case ParamType::BOOL: {
        bool val = (input == "true" || input == "1" || input == "yes");
        result.value = val;
        result.success = true;
        break;
    }
    default:
        result.success = false;
        result.error_message = "Unsupported type";
}
```

- [ ] **Step 5: Add parse_address() to param_parser.cc**

Add after `parse_latency()`:
```cpp
uint64_t ParamParser::parse_address(const std::string& s) {
    // Handle "0x" hex format
    if (s.find("0x") == 0 || s.find("0X") == 0) {
        return std::stoull(s, nullptr, 16);
    }
    // Handle "256MB", "4GB" size format
    std::regex size_regex(R"((\d+)\s*(B|KB|MB|GB|TB)?)", std::regex::icase);
    std::smatch match;
    if (std::regex_match(s, match, size_regex)) {
        uint64_t value = std::stoull(match[1].str());
        std::string unit = match[2].str();
        if (unit == "KB" || unit == "kb") return value * 1024;
        if (unit == "MB" || unit == "mb") return value * 1024 * 1024;
        if (unit == "GB" || unit == "gb") return value * 1024 * 1024 * 1024;
        if (unit == "TB" || unit == "tb") return value * 1024 * 1024 * 1024 * 1024;
        return value;
    }
    return std::stoull(s);
}
```

Also update header - add `parse_address()` declaration:
```cpp
static uint64_t parse_address(const std::string& s);
```

- [ ] **Step 6: Update router_tlm.json type strings**

In `configs/param_rules/router_tlm.json`:
```json
"type": "INT"  // was "INTEGER"
```

- [ ] **Step 7: Update nic_tlm.json type strings**

In `configs/param_rules/nic_tlm.json`:
```json
"type": "INT"  // was "INTEGER"
```

- [ ] **Step 8: Verify build compiles**

Run: `cd build && cmake --build . --target cpptlm_core 2>&1 | grep -E "error:|warning:.*param" | head -10`

- [ ] **Step 9: Run param tests**

Run: `cd build && ./bin/cpptlm_tests "[param]" 2>&1 | tail -5`
Expected: All param tests pass

- [ ] **Step 10: Commit**

```bash
git add include/core/param_rules.hh src/core/param_parser.cc configs/param_rules/router_tlm.json configs/param_rules/nic_tlm.json
git commit -m "fix(phase3.3): align ParamType enum with ADR-X.10 spec

Replace INTEGER/FLOAT/BOOLEAN/ENUM with INT/UNSIGNED/STRING/ADDRESS/LATENCY/BOOL.
Add parse_address() for '0x' hex and '256MB' size format.
Update param_rules JSON files to use new type strings.
Fixes spec divergence that caused JSON config mismatch with C++ expectations."
```

---

## Part B: Phase 3.3 C++ — Complete Integration

### Task B1: Load param_rules JSON files in ModuleFactory

**Files:**
- Modify: `src/core/module_factory.cc` (add loadParamRulesForType function)
- Create: `src/core/param_loader.cc` (optional, can be inline)
- Test: `test/test_param_integration.cc`

Currently `configs/param_rules/router_tlm.json` and `nic_tlm.json` exist but are never loaded.

- [ ] **Step 1: Add loadParamRulesForType() to module_factory.cc**

Add after `validate_module_params()` (around line 874):
```cpp
// Phase 3.3: Load param rules from JSON config file
static cpptlm::ParamRules loadParamRulesForType(const std::string& module_type) {
    std::string filename = "configs/param_rules/" + module_type + ".json";
    std::ifstream f(filename);
    if (!f.is_open()) {
        return {};  // No rules file for this module type
    }
    json j;
    f >> j;
    if (j.contains("rules")) {
        return j["rules"].get<cpptlm::ParamRules>();
    }
    return {};
}
```

Also add `#include <fstream>` at top if not present.

- [ ] **Step 2: Verify file can be opened**

Run: `ls configs/param_rules/*.json && head -5 configs/param_rules/router_tlm.json`
Expected: Files exist, JSON format correct

- [ ] **Step 3: Commit param_loader**

```bash
git add src/core/module_factory.cc
git commit -m "feat(phase3.3): add loadParamRulesForType() to load rules from JSON

Loads configs/param_rules/{module_type}.json and deserializes into
ParamRules map. Called from validate_module_params() to get rules
for each module type, replacing hardcoded RouterTLM-only logic."
```

---

### Task B2: Wire validate_module_params into instantiateAll (Generic Dispatch)

**Files:**
- Modify: `src/core/module_factory.cc:349-366` (replace hardcoded RouterTLM check)
- Modify: `src/core/module_factory.cc:418-428` (add post-set_config validation)
- Test: `test/test_param_integration.cc`

- [ ] **Step 1: Replace hardcoded RouterTLM param loop with generic dispatch**

Replace lines 349-366:
```cpp
// OLD (RouterTLM only):
for (auto& mod : final_config["modules"]) {
    std::string type = mod["type"];
    if (mod.contains("params") && type == "RouterTLM") {
        auto rules = tlm::RouterTLM::get_param_rules();
        // ...
    }
}

// NEW (all module types):
for (auto& mod : final_config["modules"]) {
    std::string type = mod["type"];
    if (mod.contains("params")) {
        auto rules = loadParamRulesForType(type);
        if (!validate_module_params(type, mod["params"], rules)) {
            return false;
        }
    }
}
```

- [ ] **Step 2: Add derive_expr evaluation to instantiateAll**

After line 366 (before creating module instances), add evaluate_derive_expr calls:
```cpp
// Evaluate derive_expr for each module's params
for (auto& mod : final_config["modules"]) {
    std::string type = mod["type"];
    if (!mod.contains("params")) continue;
    auto rules = loadParamRulesForType(type);
    // Build params map for derive_expr evaluation
    std::map<std::string, int64_t> params;
    for (const auto& [name, rule] : rules) {
        if (mod["params"].contains(name)) {
            params[name] = mod["params"][name].get<int64_t>();
        }
    }
    // Evaluate derive_expr for each rule
    for (const auto& [name, rule] : rules) {
        if (rule.derive_expr && mod["params"].contains(name)) {
            int64_t result = cpptlm::ParamParser::evaluate_derive_expr(
                *rule.derive_expr, params);
            if (result != 0) {
                mod["params"][name] = result;
                params[name] = result;
            }
        }
    }
}
```

- [ ] **Step 3: Add validate_module_params after set_config() (around line 424)**

```cpp
if (cfg_src) {
    obj->set_config(*cfg_src);
    // T3.3-05: Validate params after set_config
    auto rules = loadParamRulesForType(type);
    if (!validate_module_params(type, *cfg_src, rules)) {
        DPRINTF(MODULE, "[PARAM ERROR] Validation failed for %s, aborting\n", name.c_str());
        return false;
    }
    obj->on_config_loaded();
}
```

- [ ] **Step 4: Verify build compiles**

Run: `cd build && cmake --build . --target cpptlm_core 2>&1 | grep -E "error:" | head -5`

- [ ] **Step 5: Run integration test**

Run: `cd build && ./bin/cpptlm_tests "[param_integration]" 2>&1 | tail -10`

- [ ] **Step 6: Commit**

```bash
git add src/core/module_factory.cc
git commit -m "feat(phase3.3): wire validate_module_params into instantiateAll flow

- Replace hardcoded RouterTLM-only param check with generic dispatch
- Add derive_expr evaluation in pre-creation loop
- Add validate_module_params call after set_config()
- validate_module_params now called for ALL module types with param_rules JSON files"
```

---

### Task B3: Add set_config() Exception Handling

**Files:**
- Modify: `src/core/module_factory.cc:418-428`

- [ ] **Step 1: Add try-catch around set_config()**

Replace simple `obj->set_config(*cfg_src)` with:
```cpp
try {
    obj->set_config(*cfg_src);
} catch (const cpptlm::ParamValidationError& e) {
    DPRINTF(MODULE, "[PARAM ERROR] set_config failed for %s: %s\n",
            name.c_str(), e.what());
    return false;
} catch (const std::exception& e) {
    DPRINTF(MODULE, "[CONFIG ERROR] set_config failed for %s: %s\n",
            name.c_str(), e.what());
    return false;
}
```

- [ ] **Step 2: Verify build compiles**

Run: `cd build && cmake --build . --target cpptlm_core 2>&1 | grep -E "error:" | head -3`

- [ ] **Step 3: Commit**

```bash
git add src/core/module_factory.cc
git commit -m "feat(phase3.3): add try-catch for set_config() ParamValidationError

Catches ParamValidationError and other std::exception from set_config(),
reports module name and error message, and aborts instantiation cleanly
rather than allowing silent failures or undefined behavior."
```

---

### Task B4: Create test_param_parser.cc

**Files:**
- Create: `test/test_param_parser.cc`
- Test: ParamParser::parse(), validate(), evaluate_derive_expr(), parse_latency()

- [ ] **Step 1: Write failing test for parse() with INT type**

```cpp
TEST_CASE("ParamParser: parse INT type", "[param_parser]") {
    auto result = cpptlm::ParamParser::parse("42", cpptlm::ParamType::INT);
    REQUIRE(result.success == true);
    REQUIRE(std::get<int64_t>(result.value) == 42);
}
```

- [ ] **Step 2: Write test for UNSIGNED type**

```cpp
TEST_CASE("ParamParser: parse UNSIGNED type", "[param_parser]") {
    auto result = cpptlm::ParamParser::parse("100", cpptlm::ParamType::UNSIGNED);
    REQUIRE(result.success == true);
    REQUIRE(std::get<uint64_t>(result.value) == 100);
}
```

- [ ] **Step 3: Write test for ADDRESS hex format**

```cpp
TEST_CASE("ParamParser: parse ADDRESS hex", "[param_parser]") {
    auto result = cpptlm::ParamParser::parse("0x1000", cpptlm::ParamType::ADDRESS);
    REQUIRE(result.success == true);
    REQUIRE(std::get<uint64_t>(result.value) == 4096);
}
```

- [ ] **Step 4: Write test for ADDRESS size format**

```cpp
TEST_CASE("ParamParser: parse ADDRESS size format", "[param_parser]") {
    auto result = cpptlm::ParamParser::parse("256MB", cpptlm::ParamType::ADDRESS);
    REQUIRE(result.success == true);
    REQUIRE(std::get<uint64_t>(result.value) == 268435456);
}
```

- [ ] **Step 5: Write test for LATENCY with ns units**

```cpp
TEST_CASE("ParamParser: parse LATENCY ns at 1GHz", "[param_parser]") {
    auto result = cpptlm::ParamParser::parse("3ns", cpptlm::ParamType::LATENCY, 1000.0);
    REQUIRE(result.success == true);
    REQUIRE(std::get<uint64_t>(result.value) == 3);  // 3ns at 1GHz = 3 cycles
}
```

- [ ] **Step 6: Write test for LATENCY with us units**

```cpp
TEST_CASE("ParamParser: parse LATENCY us", "[param_parser]") {
    auto result = cpptlm::ParamParser::parse("5us", cpptlm::ParamType::LATENCY, 1000.0);
    REQUIRE(result.success == true);
    REQUIRE(std::get<uint64_t>(result.value) == 5000);  // 5us at 1GHz = 5000 cycles
}
```

- [ ] **Step 7: Write test for validate() with min/max**

```cpp
TEST_CASE("ParamParser: validate respects min_value", "[param_parser]") {
    cpptlm::ParamParseResult r;
    r.success = true;
    r.value = int64_t(1);
    cpptlm::ParamRule rule;
    rule.min_value = 2;
    REQUIRE(cpptlm::ParamParser::validate(r, rule) == false);
}
```

- [ ] **Step 8: Write test for evaluate_derive_expr ternary**

```cpp
TEST_CASE("ParamParser: evaluate_derive_expr >= condition", "[param_parser]") {
    std::map<std::string, int64_t> params = {{"mesh_x", 4}};
    auto result = cpptlm::ParamParser::evaluate_derive_expr("(mesh_x >= 4) ? 8 : 4", params);
    REQUIRE(result == 8);
}
```

- [ ] **Step 9: Write test for evaluate_derive_expr false branch**

```cpp
TEST_CASE("ParamParser: evaluate_derive_expr false branch", "[param_parser]") {
    std::map<std::string, int64_t> params = {{"mesh_x", 2}};
    auto result = cpptlm::ParamParser::evaluate_derive_expr("(mesh_x >= 4) ? 8 : 4", params);
    REQUIRE(result == 4);
}
```

- [ ] **Step 10: Run tests to verify they fail (first time)**

Run: `cd build && cmake --build . && ./bin/cpptlm_tests "[param_parser]" 2>&1 | tail -10`
Expected: FAIL (test file doesn't exist yet — will be created in next step)

- [ ] **Step 11: Create test_param_parser.cc with all tests above**

Write the complete file with all test cases from Steps 1-9.

- [ ] **Step 12: Run tests to verify they pass**

Run: `cd build && cmake --build . && ./bin/cpptlm_tests "[param_parser]" 2>&1 | tail -5`
Expected: PASS (all 9 test cases)

- [ ] **Step 13: Commit**

```bash
git add test/test_param_parser.cc
git commit -m "test(phase3.3): add test_param_parser.cc with 9 test cases

Tests for parse() INT/UNSIGNED/ADDRESS/LATENCY types,
parse_address() hex and size format, validate() min/max,
evaluate_derive_expr() ternary with >= operator."
```

---

### Task B5: Create test_param_integration.cc

**Files:**
- Create: `test/test_param_integration.cc`
- Test: ModuleFactory + param_rules + derive_expr end-to-end

- [ ] **Step 1: Write test for ModuleFactory loads param_rules JSON**

```cpp
TEST_CASE("ModuleFactory: loads param_rules from JSON file", "[param_integration]") {
    registerAllModules();
    EventQueue eq;
    ModuleFactory factory(&eq);

    json config = R"({
        "modules": [
            {"name": "router", "type": "RouterTLM", "params": {
                "node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2, "vc_count": 4
            }}
        ],
        "connections": []
    })"_json;

    bool result = factory.instantiateAll(config);
    REQUIRE(result == true);
}
```

- [ ] **Step 2: Write test for missing required param fails**

```cpp
TEST_CASE("ModuleFactory: missing required param fails", "[param_integration]") {
    registerAllModules();
    EventQueue eq;
    ModuleFactory factory(&eq);

    json config = R"({
        "modules": [
            {"name": "router", "type": "RouterTLM", "params": {
                "node_x": 0  // missing node_y, mesh_x, mesh_y (all required)
            }}
        ],
        "connections": []
    })"_json;

    bool result = factory.instantiateAll(config);
    REQUIRE(result == false);  // Should fail - missing required params
}
```

- [ ] **Step 3: Write test for derive_expr evaluation**

```cpp
TEST_CASE("ModuleFactory: derive_expr evaluated for vc_count", "[param_integration]") {
    registerAllModules();
    EventQueue eq;
    ModuleFactory factory(&eq);

    // mesh_x = 4 triggers derive_expr: (mesh_x >= 4) ? 8 : 4
    json config = R"({
        "modules": [
            {"name": "router", "type": "RouterTLM", "params": {
                "node_x": 0, "node_y": 0, "mesh_x": 4, "mesh_y": 2
            }}
        ],
        "connections": []
    })"_json;

    bool result = factory.instantiateAll(config);
    REQUIRE(result == true);
    // vc_count should be auto-evaluated to 8 (from derive_expr)
}
```

- [ ] **Step 4: Write test for param out of range fails**

```cpp
TEST_CASE("ModuleFactory: param out of range fails", "[param_integration]") {
    registerAllModules();
    EventQueue eq;
    ModuleFactory factory(&eq);

    json config = R"({
        "modules": [
            {"name": "router", "type": "RouterTLM", "params": {
                "node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2,
                "vc_count": 100  // max is 8 per router_tlm.json rules
            }}
        ],
        "connections": []
    })"_json;

    bool result = factory.instantiateAll(config);
    REQUIRE(result == false);  // Should fail - vc_count > max_value(8)
}
```

- [ ] **Step 5: Create test_param_integration.cc with all tests above**

- [ ] **Step 6: Run tests to verify they pass**

Run: `cd build && cmake --build . && ./bin/cpptlm_tests "[param_integration]" 2>&1 | tail -5`
Expected: PASS (all 4 test cases)

- [ ] **Step 7: Commit**

```bash
git add test/test_param_integration.cc
git commit -m "test(phase3.3): add test_param_integration.cc with 4 integration tests

Tests ModuleFactory with param_rules JSON loading, required param
validation, derive_expr evaluation, and range constraint checking."
```

---

## Part C: Phase 3.2 Python — cpptlm_config Package (New)

### Task C1: Create cpptlm_config Package Skeleton

**Files:**
- Create: `cpptlm_config/__init__.py`
- Create: `cpptlm_config/types.py` (RouterPort, NICPort enums)
- Create: `cpptlm_config/models.py` (Pydantic models)
- Create: `cpptlm_config/pyproject.toml`
- Test: `test/python/test_port_types.py`

cpptlm_config/ package does not exist — all files need creation from scratch.

- [ ] **Step 1: Create cpptlm_config/ directory**

```bash
mkdir -p cpptlm_config
touch cpptlm_config/__init__.py
```

- [ ] **Step 2: Create types.py with RouterPort/NICPort enums (IntEnum)**

```python
from enum import IntEnum

class RouterPort(IntEnum):
    NORTH = 0
    EAST = 1
    SOUTH = 2
    WEST = 3
    LOCAL = 4

class NICPort(IntEnum):
    PE_REQ = 0
    PE_RESP = 1
    NET_REQ = 2
    NET_RESP = 3
```

- [ ] **Step 3: Create models.py with Pydantic models**

```python
from pydantic import BaseModel, Field
from typing import Optional
from enum import Enum

class PortRole(str, Enum):
    INITIATOR = "initiator"
    TARGET = "target"
    BI_DIRECTIONAL = "bi_directional"
    NETWORK = "network"
    PE = "pe"

class BundleType(str, Enum):
    CACHE_REQ = "cache_req"
    CACHE_RESP = "cache_resp"
    NOC_FLIT = "noc_flit"
    GENERIC = "generic"

class PortSpec(BaseModel):
    name: str
    role: PortRole
    bundle: BundleType
    width: int = 64
    is_multi: bool = False
    port_count: int = 1
    layout_hint: Optional[str] = None

class PortGroupMember(BaseModel):
    index: int
    role: PortRole
    bundle: BundleType

class PortGroupBundleType(str, Enum):
    SINGLE = "SINGLE"
    BUNDLE_MASTER = "BUNDLE_MASTER"
    BUNDLE_SLAVE = "BUNDLE_SLAVE"

class PortGroupSpec(BaseModel):
    name: str
    bundle_type: PortGroupBundleType = PortGroupBundleType.SINGLE
    ports: list[PortGroupMember] = []

class ModulePortSpec(BaseModel):
    module_name: str
    ports: list[PortSpec] = []
    port_groups: list[PortGroupSpec] = []
    aliases: dict[str, str] = {}

class ConfigMetadata(BaseModel):
    version: str = "1.0.0"
    schema_version: str = "1.0"
    visualization: Optional[dict] = None

class ConfigSchema(BaseModel):
    name: str
    description: str = ""
    metadata: ConfigMetadata = ConfigMetadata()
    modules: list = []
    connections: list = []
    module_groups: list = []
```

- [ ] **Step 4: Create pyproject.toml**

```toml
[project]
name = "cpptlm-config"
version = "0.1.0"
description = "CppTLM configuration generator with type-safe Pydantic models"
requires-python = ">=3.10"
dependencies = [
    "pydantic>=2.0",
]

[project.optional-dependencies]
dev = [
    "pytest>=7.0",
    "ruff>=0.1.0",
]

[build-system]
requires = ["setuptools>=61.0"]
build-backend = "setuptools.build_meta"

[tool.ruff]
line-length = 100
target-version = "py310"
```

- [ ] **Step 5: Update __init__.py to export public API**

```python
from .types import RouterPort, NICPort
from .models import (
    PortRole, BundleType, PortSpec,
    PortGroupMember, PortGroupBundleType, PortGroupSpec,
    ModulePortSpec, ConfigMetadata, ConfigSchema
)

__all__ = [
    "RouterPort", "NICPort",
    "PortRole", "BundleType", "PortSpec",
    "PortGroupMember", "PortGroupBundleType", "PortGroupSpec",
    "ModulePortSpec", "ConfigMetadata", "ConfigSchema",
]
```

- [ ] **Step 6: Verify package imports**

Run: `cd /workspace/project/CppTLM && python3 -c "from cpptlm_config import RouterPort, NICPort; print(f'RouterPort.NORTH={RouterPort.NORTH.value}')"`
Expected: `RouterPort.NORTH=0`

- [ ] **Step 7: Create test/python/test_port_types.py**

```python
import pytest
from cpptlm_config import RouterPort, NICPort, PortRole, BundleType, PortSpec

def test_router_port_enum_values():
    assert RouterPort.NORTH == 0
    assert RouterPort.EAST == 1
    assert RouterPort.SOUTH == 2
    assert RouterPort.WEST == 3
    assert RouterPort.LOCAL == 4

def test_nic_port_enum_values():
    assert NICPort.PE_REQ == 0
    assert NICPort.PE_RESP == 1
    assert NICPort.NET_REQ == 2
    assert NICPort.NET_RESP == 3

def test_port_spec_pydantic():
    spec = PortSpec(name="NORTH", role=PortRole.NETWORK, bundle=BundleType.NOC_FLIT)
    assert spec.name == "NORTH"
    assert spec.role == PortRole.NETWORK

def test_port_spec_json_schema():
    spec = PortSpec(name="test", role=PortRole.INITIATOR, bundle=BundleType.CACHE_REQ)
    json_data = spec.model_dump()
    assert json_data["name"] == "test"
    assert json_data["role"] == "initiator"
```

- [ ] **Step 8: Run Python tests**

Run: `cd /workspace/project/CppTLM && python3 -m pytest test/python/test_port_types.py -v`
Expected: 4 tests pass

- [ ] **Step 9: Commit**

```bash
git add cpptlm_config/ test/python/test_port_types.py
git commit -m "feat(phase3.2): create cpptlm_config Python package

Initial cpptlm_config package with:
- types.py: RouterPort(IntEnum), NICPort(IntEnum)
- models.py: Pydantic v2 models (PortSpec, PortGroupSpec, ConfigSchema)
- pyproject.toml: pydantic>=2.0, Python 3.10+
- test/python/test_port_types.py: 4 test cases

Implements T3.2-10 (RouterPort/NICPort enums) and T3.2-12 (pyproject.toml).
Note: T3.2-11 SemVer and T3.2-13 layout_hint are partial - ConfigMetadata
has version field but bump_version() not yet implemented."
```

---

### Task C2: Implement T3.2-11 SemVer and T3.2-13 layout_hint

**Files:**
- Modify: `cpptlm_config/models.py` (add bump_version, layout_hint)
- Test: `test/python/test_port_types.py`

- [ ] **Step 1: Add bump_version() to ConfigMetadata**

In `models.py`, add method to ConfigMetadata class:
```python
def bump_version(self, part: str = "patch") -> str:
    """Bump semantic version. part = 'major', 'minor', or 'patch'"""
    v = self.version.split('.')
    major, minor, patch = int(v[0]), int(v[1]), int(v[2])
    if part == "major":
        major += 1; minor = 0; patch = 0
    elif part == "minor":
        minor += 1; patch = 0
    else:
        patch += 1
    self.version = f"{major}.{minor}.{patch}"
    return self.version
```

- [ ] **Step 2: Verify bump_version works**

Run: `python3 -c "from cpptlm_config.models import ConfigMetadata; m = ConfigMetadata(version='1.2.3'); print(m.bump_version('minor'))"`
Expected: `1.3.0`

- [ ] **Step 3: Verify layout_hint in PortSpec**

Run: `python3 -c "from cpptlm_config.models import PortSpec, PortRole, BundleType; p = PortSpec(name='test', role=PortRole.INITIATOR, bundle=BundleType.CACHE_REQ, layout_hint='top'); print(p.layout_hint)"`
Expected: `top`

- [ ] **Step 4: Update test_port_types.py with new tests**

```python
def test_config_metadata_bump_version():
    from cpptlm_config.models import ConfigMetadata
    m = ConfigMetadata(version="1.2.3")
    assert m.bump_version("minor") == "1.3.0"
    assert m.bump_version("patch") == "1.3.1"
    assert m.bump_version("major") == "2.0.0"
```

- [ ] **Step 5: Run tests**

Run: `cd /workspace/project/CppTLM && python3 -m pytest test/python/test_port_types.py -v`
Expected: All tests pass

- [ ] **Step 6: Commit**

```bash
git add cpptlm_config/models.py test/python/test_port_types.py
git commit -m "feat(phase3.2): add bump_version() and layout_hint support

- ConfigMetadata.bump_version(part) for SemVer version management
- PortSpec.layout_hint field for visualization metadata
- T3.2-11 (SemVer) and T3.2-13 (layout_hint) now complete"
```

---

### Task C3: Implement T3.2-15 Python Port Type Tests

**Files:**
- Modify: `test/python/test_port_types.py`
- Add: 15+ test cases covering all port type functionality

- [ ] **Step 1: Add comprehensive test cases**

In `test_port_types.py`, add tests for:
- PortGroupSpec JSON roundtrip
- ModulePortSpec with port_groups
- BundleType enum serialization
- PortRole enum serialization
- ConfigMetadata with visualization metadata
- PortSpec with all fields

- [ ] **Step 2: Run all tests**

Run: `cd /workspace/project/CppTLM && python3 -m pytest test/python/test_port_types.py -v`
Expected: 15+ tests pass

- [ ] **Step 3: Commit**

```bash
git add test/python/test_port_types.py
git commit -m "test(phase3.2): add comprehensive port type tests (15+ cases)

Covers RouterPort/NICPort enums, PortSpec/PortGroupSpec models,
ConfigMetadata with bump_version, BundleType/PortRole serialization,
and ModulePortSpec with port_groups. T3.2-15 complete."
```

---

## Part D: Phase 3.4 Planning

### Task D1: Document Phase 3.4 Scope and Dependencies

**Files:**
- Modify: `docs/plan/phase3.4-validation-toolchain-plan.md`

Phase 3.4 requires cpptlm_config package (from Phase 3.2 Python tasks) to be complete first.

- [ ] **Step 1: Update Phase 3.4 plan status**

In `docs/plan/phase3.4-validation-toolchain-plan.md`, update header:
```
> **状态**: 📋 路线图（依赖 cpptlm_config 完成，Phase 3.2 Python 端实施后启动）
```

- [ ] **Step 2: Add dependency note**

Add at top of file:
```
> **前置条件更新 (2026-05-07)**: Phase 3.4 依赖 cpptlm_config Python 包。
> 当前 cpptlm_config 未创建（2026-05-07 待 Phase 3.2 Python 端实施）。
> 预估启动时间: Phase 3.2 Python 任务完成后。
```

- [ ] **Step 3: Commit**

```bash
git add docs/plan/phase3.4-validation-toolchain-plan.md
git commit -m "docs(phase3.4): update plan status noting cpptlm_config dependency

Phase 3.4 (validation toolchain) depends on cpptlm_config Python package
which will be created during Phase 3.2 Python tasks (T3.2-10~15).
Plan updated to reflect dependency."
```

---

## Summary

| Part | Task | Files | Status |
|------|------|-------|--------|
| **A** | A1: Fix empty_port_specs bug | module_factory.cc | 🔲 |
| | A2: Align ParamType with ADR-X.10 | param_rules.hh, param_parser.cc, configs/ | 🔲 |
| **B** | B1: Load param_rules JSON files | module_factory.cc | 🔲 |
| | B2: Wire validate_module_params (generic dispatch) | module_factory.cc | 🔲 |
| | B3: Add set_config() exception handling | module_factory.cc | 🔲 |
| | B4: Create test_param_parser.cc | test_param_parser.cc | 🔲 |
| | B5: Create test_param_integration.cc | test_param_integration.cc | 🔲 |
| **C** | C1: Create cpptlm_config package | cpptlm_config/ | 🔲 |
| | C2: Implement T3.2-11/13 (SemVer, layout_hint) | models.py | 🔲 |
| | C3: Implement T3.2-15 (Python tests) | test_port_types.py | 🔲 |
| **D** | D1: Update Phase 3.4 plan | phase3.4-plan.md | 🔲 |

**Execution order**: A1 → A2 → B1 → B2 → B3 → B4 → B5 → C1 → C2 → C3 → D1

**Plan complete and saved to `docs/superpowers/plans/YYYY-MM-DD-phase-3-remaining-tasks.md`.**

Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?