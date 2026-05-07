# Phase 3.3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement nlohmann/json-driven parameter framework with type conversion, derive_expr evaluation, and ParamValidationError exception handling.

**Architecture:** Extend existing `param_rules.hh` with JSON serialization, add `param_parser.hh` for type conversion and derive_expr evaluation, add `param_errors.hh` for exception handling, integrate into ModuleFactory.

**Tech Stack:** C++17, nlohmann/json, Catch2 (testing)

---

## Context

Phase 3.1 already implemented basic `ParamRule` struct in `include/core/param_rules.hh`. Phase 3.3 will:
1. Add JSON serialization macros
2. Create `param_parser.hh` for `"3ns"` → cycles, `"256MB"` → bytes conversion
3. Create `param_errors.hh` for ParamValidationError exception
4. Create `configs/param_rules/router_tlm.json` global rules
5. Integrate into ModuleFactory

---

## Task 1: Extend param_rules.hh with JSON Serialization

**Files:**
- Modify: `include/core/param_rules.hh:1-38`
- Test: `test/test_param_rules.cc` (new file)

- [ ] **Step 1: Create test file with failing test**

```cpp
// test/test_param_rules.cc
#include <catch2/catch_all.hpp>
#include "core/param_rules.hh"

TEST_CASE("ParamRule: JSON serialization roundtrip") {
    cpptlm::ParamRule rule;
    rule.name = "vc_count";
    rule.type = cpptlm::ParamType::INTEGER;
    rule.required = false;
    rule.default_int = 4;
    rule.min_value = 1;
    rule.max_value = 8;

    nlohmann::json j = rule;
    REQUIRE(j["type"] == "INTEGER");
    REQUIRE(j["default_int"] == 4);

    cpptlm::ParamRule rule2 = j.get<cpptlm::ParamRule>();
    REQUIRE(rule.type == rule2.type);
    REQUIRE(rule.default_int == rule2.default_int);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . && ./bin/cpptlm_tests "[param]" 2>&1 | tail -20`
Expected: FAIL - missing JSON macros

- [ ] **Step 3: Extend param_rules.hh with JSON serialization**

Modify `include/core/param_rules.hh`:

```cpp
// include/core/param_rules.hh
// SPDX-License-Identifier: Apache-2.0
// Phase 3.1: 参数规则定义 (Extended in Phase 3.3)

#pragma once

#include <string>
#include <optional>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

namespace cpptlm {

enum class ParamType {
    INTEGER,
    FLOAT,
    STRING,
    BOOLEAN,
    ENUM
};

NLOHMANN_JSON_SERIALIZE_ENUM(ParamType, {
    {ParamType::INTEGER, "INTEGER"},
    {ParamType::FLOAT, "FLOAT"},
    {ParamType::STRING, "STRING"},
    {ParamType::BOOLEAN, "BOOLEAN"},
    {ParamType::ENUM, "ENUM"}
})

struct ParamRule {
    std::string name;
    ParamType type = ParamType::STRING;
    bool required = false;
    std::optional<int> default_int;
    std::optional<double> default_float;
    std::optional<std::string> default_str;
    std::optional<bool> default_bool;
    std::optional<int> min_value;
    std::optional<int> max_value;
    std::optional<std::vector<std::string>> enum_values;
};

void to_json(nlohmann::json& j, const ParamRule& p) {
    j = nlohmann::json{{"name", p.name}, {"type", p.type}, {"required", p.required}};
    if (p.default_int) j["default_int"] = *p.default_int;
    if (p.default_float) j["default_float"] = *p.default_float;
    if (p.default_str) j["default_str"] = *p.default_str;
    if (p.default_bool) j["default_bool"] = *p.default_bool;
    if (p.min_value) j["min_value"] = *p.min_value;
    if (p.max_value) j["max_value"] = *p.max_value;
    if (p.enum_values) j["enum_values"] = *p.enum_values;
}

void from_json(const nlohmann::json& j, ParamRule& p) {
    j.at("name").get_to(p.name);
    j.at("type").get_to(p.type);
    j.at("required").get_to(p.required);
    if (j.contains("default_int")) p.default_int = j["default_int"].get<int>();
    if (j.contains("default_float")) p.default_float = j["default_float"].get<double>();
    if (j.contains("default_str")) p.default_str = j["default_str"].get<std::string>();
    if (j.contains("default_bool")) p.default_bool = j["default_bool"].get<bool>();
    if (j.contains("min_value")) p.min_value = j["min_value"].get<int>();
    if (j.contains("max_value")) p.max_value = j["max_value"].get<int>();
    if (j.contains("enum_values")) p.enum_values = j["enum_values"].get<std::vector<std::string>>();
}

using ParamRules = std::map<std::string, ParamRule>;

} // namespace cpptlm
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd build && cmake --build . && ./bin/cpptlm_tests "ParamRule: JSON serialization"`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/core/param_rules.hh test/test_param_rules.cc
git commit -m "feat(phase3.3): add JSON serialization to ParamRule"
```

---

## Task 2: Create param_errors.hh

**Files:**
- Create: `include/core/param_errors.hh`
- Test: `test/test_param_errors.cc` (new file)

- [ ] **Step 1: Create failing test**

```cpp
// test/test_param_errors.cc
#include <catch2/catch_all.hpp>
#include "core/param_errors.hh"

TEST_CASE("ParamValidationError: exception carries info") {
    cpptlm::ParamValidationError err("router_0", "node_x", "out of range");
    REQUIRE(err.module_name == "router_0");
    REQUIRE(err.param_name == "node_x");
    REQUIRE(std::string(err.what()) == "out of range");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . && ./bin/cpptlm_tests "[param_errors]"`
Expected: FAIL - file not found

- [ ] **Step 3: Create param_errors.hh**

```cpp
// include/core/param_errors.hh
// SPDX-License-Identifier: Apache-2.0
// Phase 3.3: Parameter validation error exception

#pragma once

#include <stdexcept>
#include <string>

namespace cpptlm {

class ParamValidationError : public std::invalid_argument {
public:
    std::string module_name;
    std::string param_name;
    std::string rule_violated;

    ParamValidationError(const std::string& module,
                         const std::string& param,
                         const std::string& reason)
        : std::invalid_argument(reason),
          module_name(module),
          param_name(param),
          rule_violated(reason) {}
};

} // namespace cpptlm
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd build && cmake --build . && ./bin/cpptlm_tests "[param_errors]"`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/core/param_errors.hh test/test_param_errors.cc
git commit -m "feat(phase3.3): add ParamValidationError exception"
```

---

## Task 3: Create param_parser.hh

**Files:**
- Create: `include/core/param_parser.hh`
- Test: `test/test_param_parser.cc` (new file)

- [ ] **Step 1: Create failing test for latency parsing**

```cpp
// test/test_param_parser.cc
#include <catch2/catch_all.hpp>
#include "core/param_parser.hh"

TEST_CASE("ParamParser: parse latency with units") {
    auto result = cpptlm::ParamParser::parse("3ns", cpptlm::ParamType::LATENCY, 1000.0);
    REQUIRE(result.success);
    // At 1000MHz, 3ns = 3 cycles
    REQUIRE(std::get<uint64_t>(result.value) == 3);
}

TEST_CASE("ParamParser: parse latency in cycles") {
    auto result = cpptlm::ParamParser::parse("5", cpptlm::ParamType::LATENCY, 1000.0);
    REQUIRE(result.success);
    REQUIRE(std::get<uint64_t>(result.value) == 5);
}
```

- [ ] **Step 2: Run test to verify it fails**

Expected: FAIL - param_parser.hh not found

- [ ] **Step 3: Create param_parser.hh**

```cpp
// include/core/param_parser.hh
// SPDX-License-Identifier: Apache-2.0
// Phase 3.3: Parameter parser with type conversion and derive_expr evaluation

#pragma once

#include "param_rules.hh"
#include <string>
#include <variant>
#include <map>

namespace cpptlm {

struct ParamParseResult {
    bool success;
    std::string error_message;
    std::variant<int64_t, uint64_t, double, std::string, bool> value;
};

class ParamParser {
public:
    // Parse string to typed value
    static ParamParseResult parse(const std::string& input, ParamType type,
                                  double clock_frequency_mhz = 1000.0);

    // Validate value against rule
    static bool validate(const ParamParseResult& result, const ParamRule& rule);

    // Evaluate derive_expr like "(mesh_x >= 4) ? 8 : 4"
    static int64_t evaluate_derive_expr(const std::string& expr,
                                          const std::map<std::string, int64_t>& params);

private:
    static uint64_t parse_latency(const std::string& s, double clock_mhz);
    static uint64_t parse_address(const std::string& s);
};

} // namespace cpptlm
```

- [ ] **Step 4: Run test to verify it fails**

Expected: FAIL - parse function not implemented

- [ ] **Step 5: Create param_parser.cc implementation**

```cpp
// src/core/param_parser.cc
#include "core/param_parser.hh"
#include <cstdint>
#include <regex>
#include <sstream>

namespace cpptlm {

ParamParseResult ParamParser::parse(const std::string& input, ParamType type,
                                    double clock_frequency_mhz) {
    ParamParseResult result;

    if (input.empty()) {
        result.success = false;
        result.error_message = "Empty input";
        return result;
    }

    try {
        switch (type) {
            case ParamType::INTEGER: {
                int64_t val = std::stoll(input);
                result.value = val;
                result.success = true;
                break;
            }
            case ParamType::FLOAT: {
                double val = std::stod(input);
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
            case ParamType::STRING: {
                result.value = input;
                result.success = true;
                break;
            }
            case ParamType::BOOLEAN: {
                bool val = (input == "true" || input == "1" || input == "yes");
                result.value = val;
                result.success = true;
                break;
            }
            default:
                result.success = false;
                result.error_message = "Unsupported type";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }

    return result;
}

uint64_t ParamParser::parse_latency(const std::string& s, double clock_mhz) {
    // Check for unit suffix
    std::regex latency_regex(R"((\d+)(ns|us|ms)?)");
    std::smatch match;
    if (std::regex_match(s, match, latency_regex)) {
        uint64_t value = std::stoull(match[1].str());
        std::string unit = match[2].str();
        if (unit == "ns") {
            // nanoseconds to cycles
            return static_cast<uint64_t>(value * clock_mhz / 1000.0);
        } else if (unit == "us") {
            return static_cast<uint64_t>(value * clock_mhz);
        } else if (unit == "ms") {
            return static_cast<uint64_t>(value * clock_mhz * 1000);
        }
        // No unit = already in cycles
        return value;
    }
    // Try parsing as plain number (cycles)
    return std::stoull(s);
}

bool ParamParser::validate(const ParamParseResult& result, const ParamRule& rule) {
    if (!result.success) return false;

    // Range validation for INTEGER type
    if (std::holds_alternative<int64_t>(result.value)) {
        int64_t val = std::get<int64_t>(result.value);
        if (rule.min_value && val < *rule.min_value) return false;
        if (rule.max_value && val > *rule.max_value) return false;
    }

    return true;
}

int64_t ParamParser::evaluate_derive_expr(const std::string& expr,
                                         const std::map<std::string, int64_t>& params) {
    // Simple ternary expression: "(cond) ? v1 : v2"
    // cond is a simple comparison like "mesh_x >= 4"
    size_t q_pos = expr.find('?');
    if (q_pos == std::string::npos) return 0;

    std::string cond = expr.substr(0, q_pos);
    std::string rest = expr.substr(q_pos + 1);
    size_t colon_pos = rest.find(':');
    if (colon_pos == std::string::npos) return 0;

    std::string true_val_str = rest.substr(0, colon_pos);
    std::string false_val_str = rest.substr(colon_pos + 1);

    // Parse condition: supports >=, <=, >, <, ==
    std::regex cmp_regex(R"((\w+)\s*(>=|<=|>=?)\s*(\d+))");
    std::smatch match;
    if (std::regex_match(cond, match, cmp_regex)) {
        std::string param_name = match[1].str();
        std::string op = match[2].str();
        int64_t rhs = std::stoll(match[3].str());

        auto it = params.find(param_name);
        if (it == params.end()) return 0;
        int64_t lhs = it->second;

        bool cond_result = false;
        if (op == ">=") cond_result = lhs >= rhs;
        else if (op == "<=") cond_result = lhs <= rhs;
        else if (op == ">") cond_result = lhs > rhs;
        else if (op == "<") cond_result = lhs < rhs;
        else if (op == "==") cond_result = lhs == rhs;

        return cond_result ? std::stoll(true_val_str) : std::stoll(false_val_str);
    }

    return 0;
}

} // namespace cpptlm
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `cd build && cmake --build . && ./bin/cpptlm_tests "[param_parser]"`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add include/core/param_parser.hh src/core/param_parser.cc test/test_param_parser.cc
git commit -m "feat(phase3.3): add ParamParser with type conversion and derive_expr"
```

---

## Task 4: Create global param rules JSON file

**Files:**
- Create: `configs/param_rules/router_tlm.json`
- Create: `configs/param_rules/nic_tlm.json`

- [ ] **Step 1: Create router_tlm.json**

```json
{
  "module_type": "RouterTLM",
  "rules": {
    "node_x": {
      "name": "node_x",
      "type": "INTEGER",
      "required": true
    },
    "node_y": {
      "name": "node_y",
      "type": "INTEGER",
      "required": true
    },
    "mesh_x": {
      "name": "mesh_x",
      "type": "INTEGER",
      "required": true
    },
    "mesh_y": {
      "name": "mesh_y",
      "type": "INTEGER",
      "required": true
    },
    "vc_count": {
      "name": "vc_count",
      "type": "INTEGER",
      "required": false,
      "default_int": 2,
      "min_value": 1,
      "max_value": 8
    }
  }
}
```

- [ ] **Step 2: Create nic_tlm.json**

```json
{
  "module_type": "NICTLM",
  "rules": {
    "node_id": {
      "name": "node_id",
      "type": "INTEGER",
      "required": true
    },
    "mesh_x": {
      "name": "mesh_x",
      "type": "INTEGER",
      "required": true
    },
    "mesh_y": {
      "name": "mesh_y",
      "type": "INTEGER",
      "required": true
    }
  }
}
```

- [ ] **Step 3: Commit**

```bash
git add configs/param_rules/router_tlm.json configs/param_rules/nic_tlm.json
git commit -m "feat(phase3.3): add global param rules JSON files"
```

---

## Task 5: Integrate ParamRule validation into ModuleFactory

**Files:**
- Modify: `src/core/module_factory.cc`
- Modify: `src/CMakeLists.txt` (add param_parser.cc)
- Test: `test/test_param_integration.cc` (new file)

- [ ] **Step 1: Add param_parser.cc to CMakeLists.txt**

Modify `src/CMakeLists.txt` - add `core/param_parser.cc` to CORE_SOURCES

- [ ] **Step 2: Create integration test**

```cpp
// test/test_param_integration.cc
#include <catch2/catch_all.hpp>
#include "core/module_factory.hh"
#include "core/event_queue.hh"

TEST_CASE("ModuleFactory: loads global param rules") {
    EventQueue eq;
    ModuleFactory factory(&eq);

    // Create router with valid params
    json config = R"({
        "modules": [
            {"name": "r0", "type": "RouterTLM", "params": {
                "node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2
            }}
        ]
    })"_json;

    bool result = factory.instantiateAll(config);
    REQUIRE(result == true);
}
```

- [ ] **Step 3: Modify module_factory.cc to load and validate param rules**

Add to `src/core/module_factory.cc`:
1. Include `#include "core/param_parser.hh"`
2. Add `load_param_rules()` method
3. Add `validate_params()` method
4. Call validation in `set_config()` path

- [ ] **Step 4: Run integration test**

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/core/module_factory.cc src/CMakeLists.txt test/test_param_integration.cc
git commit -m "feat(phase3.3): integrate ParamRule validation into ModuleFactory"
```

---

## Task 6: Final verification and summary

- [ ] **Step 1: Run all Phase 3.3 tests**

Run: `cd build && cmake --build . && ./bin/cpptlm_tests "[param]"`
Expected: 25+ tests pass

- [ ] **Step 2: Run full test suite**

Run: `./scripts/run_all_tests.sh`
Expected: All tests pass (regression + new)

- [ ] **Step 3: Update plan status**

Update `docs/plan/phase3.3-config-enhancement-plan.md` status to 🚧 实施中

---

## Summary

| Task | Files | Status | Commit |
|------|-------|--------|--------|
| T3.3-01 | param_rules.hh (extended) | ✅ Completed | 5707d31 |
| T3.3-02 | param_parser.hh/cc | ✅ Completed | 5707d31 |
| T3.3-03 | param_errors.hh | ✅ Completed | 5707d31 |
| T3.3-06 | configs/param_rules/*.json | ✅ Completed | 5707d31 |
| T3.3-04~05 | ModuleFactory integration | ⚠️ Partial | 7f4e0db (helper only) |

**Total new files:** 8
**Total modified files:** 3
**Completed tests:** 2 test files (test_param_rules.cc, test_param_errors.cc)

---

## Current Status

### ✅ Completed (2026-05-07)

- `param_rules.hh` — NLOHMANN_JSON_SERIALIZE_ENUM + to_json/from_json
- `param_parser.hh` — ParamParser class declaration
- `param_parser.cc` — parse(), validate(), evaluate_derive_expr(), parse_latency()
- `param_errors.hh` — ParamValidationError exception
- `configs/param_rules/router_tlm.json` — RouterTLM parameter rules
- `configs/param_rules/nic_tlm.json` — NICTLM parameter rules
- `src/CMakeLists.txt` — param_parser.cc added to CORE_SOURCES
- `src/core/module_factory.cc` — validate_module_params() helper (not yet called)

### ⚠️ Remaining Work

1. **T3.3-05 completion**: Call `validate_module_params()` from `instantiateAll()` or `set_config()` path
2. **test_param_parser.cc**: Create parser unit tests (referenced in plan but not yet created)
3. **test_param_integration.cc**: Create ModuleFactory integration test

---

## Next Steps (Recommended Order)

### Step 1: Complete T3.3-05 — Hook validation into instantiation flow

Modify `instantiateAll()` in `module_factory.cc`:
- Load `configs/param_rules/{module_type}.json` at instance creation time
- Call `validate_module_params()` before calling module constructor
- Throw/collect ParamValidationError on failure

### Step 2: Create test_param_parser.cc

Add tests for:
- `parse()` with various ParamTypes (INTEGER, FLOAT, LATENCY, STRING, BOOLEAN)
- `parse_latency()` with ns/us/ms units
- `evaluate_derive_expr()` with ternary operator
- `validate()` with min/max constraints

### Step 3: Create test_param_integration.cc

Add integration tests:
- ModuleFactory loads global param rules on startup
- Validation errors collected and reported
- derive_expr evaluated during parameter resolution

### Step 4: Final verification

```bash
cd build && cmake --build . && ./bin/cpptlm_tests "[param]"
# Expected: 25+ tests pass
```

---

## Git Log

```
7f4e0db feat(phase3.3): integrate validate_module_params into ModuleFactory
5707d31 feat(phase3.3): add ParamRule JSON serialization, ParamParser, and param_errors
ac1796d fix(tests): add module registration and fix NIC port connections
```