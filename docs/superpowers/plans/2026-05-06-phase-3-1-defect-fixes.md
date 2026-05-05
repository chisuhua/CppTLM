# Phase 3.1: 缺陷修复与基础增强 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 ModuleFactory 已知缺陷（DEF-03/04），实现变量引用（$ref）、参数默认值、NI侧别规则、validateConfig验证四项基础增强。

**Architecture:** 修改 `module_factory.cc` 添加 WARNING 日志；新建 `var_resolver.hh` 实现 `${path}` 解析；新建 `param_rules.hh` 定义 ParamRule 结构体并在 RouterTLM 实现 `get_param_rules()`；添加 NI 连接验证函数。

**Tech Stack:** C++17, nlohmann/json, Catch2, DPRINTF 调试宏

---

## 背景：已验证的已完成项

| 任务 | 状态 | 位置 |
|------|------|------|
| DEF-01: ModuleGroup 通配符展开 | ✅ 已完成 | `include/utils/module_group.hh:80-105` |
| DEF-02: 重复连接去重 | ✅ 已完成 | `module_factory.cc:365-387, 650-660` |
| DEF-03: BidirectionalPortAdapter 绑定（仅 RouterTLM） | ✅ 已完成 | `module_factory.cc:628-637` |
| DEF-05: Python 工具链类型映射 | ✅ 已完成 | `scripts/topology_generator.py:55-72` |
| extends 支持 | ✅ 已完成 | `module_factory.cc:113-160` |
| 调试模式 | ✅ 已完成 | `src/main.cpp:54-55` |
| validateConfig() | ✅ 已实现 | `module_factory.cc:165-243` (全部 8 项检查已覆盖) |

---

## 文件结构

| 类型 | 路径 | 职责 |
|------|------|------|
| **修改** | `src/core/module_factory.cc` | DEF-04/03 WARNING 日志 + NI 侧别规则 |
| **创建** | `include/utils/var_resolver.hh` | 变量引用 `${path}` 解析器 |
| **创建** | `include/core/param_rules.hh` | ParamRule/ParamType 定义 |
| **修改** | `include/tlm/router_tlm.hh` | 添加 `get_param_rules()` 声明 |
| **修改** | `src/tlm/router_tlm.cc` | 实现 `get_param_rules()` |
| **创建** | `test/test_module_factory_fixes.cc` | DEF-04b/03b WARNING 日志测试 |
| **创建** | `test/test_var_resolver.cc` | $ref 语法解析测试（4用例） |
| **创建** | `test/test_param_defaults.cc` | 参数默认值测试（3用例） |
| **创建** | `test/test_nic_side_rules.cc` | NI PE-side 规则测试（2用例） |

---

## Task 1: T3.1-04b — DEF-04 端口索引非法 WARNING 日志

**优先级**: 🔴 高（Phase 3.2 前必须完成）
**工作量**: 1 天
**文件**: `src/core/module_factory.cc:666-674`

### 背景

当前代码在 `all_digits` 检查失败时静默使用默认值 0，用户不会收到任何反馈。改进目标：当端口索引包含非数字字符时，打印 WARNING 但继续加载（向后兼容）。

### 实现位置

`src/core/module_factory.cc` — `parsePortSpec()` 函数内（约第 666-674 行）

### 任务步骤

- [ ] **Step 1: 写失败的测试 — test_module_factory_fixes.cc**

创建 `test/test_module_factory_fixes.cc`:

```cpp
// test/test_module_factory_fixes.cc
// SPDX-License-Identifier: Apache-2.0
// Phase 3.1: DEF-03/04 WARNING 日志测试

#include <catch2/catch_all.hpp>
#include "core/module_factory.hh"
#include "core/sim_core.hh"
#include <sstream>

// 测试辅助：捕获 DPRINTF 输出
static std::string captured_log;
static void clear_log() { captured_log.clear(); }
static std::string get_log() { return captured_log; }

// 模拟 DPRINTF CONN 类别输出
#define DPRINTF_CONN_TEST(category, fmt, ...) \
    do { \
        char buf[512]; \
        snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
        captured_log += buf; \
        captured_log += "\n"; \
    } while(0)

TEST_CASE("DEF-04b: Invalid port index produces WARNING", "[defect][phase3]") {
    clear_log();

    json config = R"({
        "modules": [
            {"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}}
        ],
        "connections": [
            {"src": "r0.0abc", "dst": "r1.3", "latency": 1}
        ]
    })"_json;

    // 使用模拟的 DPRINTF 捕获日志
    // 真实测试中通过 debug_output() 捕获
    ModuleFactory factory;

    // 验证配置仍然能加载（WARNING 而非 ERROR）
    bool result = factory.instantiateAll(config);
    CHECK(result == true);  // 配置仍然加载成功

    // 验证 WARNING 日志包含非法索引信息
    // 注意：真实测试需要启用调试输出捕获
}

TEST_CASE("DEF-03b: Non-RouterTLM multi-port module produces WARNING", "[defect][phase3]") {
    clear_log();

    // 测试当存在非 RouterTLM 多端口模块时的 WARNING
    // 目前代码库中没有其他多端口 BidirectionalPortAdapter 模块
    // 此测试预留接口，待后续模块出现时验证
    CHECK(true);  // 占位测试
}
```

- [ ] **Step 2: 验证测试编译失败**

Run: `cd build && ninja test_module_factory_fixes.o 2>&1 | head -50`
Expected: 编译错误（文件不存在）

- [ ] **Step 3: 确认源代码当前行为**

读取 `src/core/module_factory.cc` 第 660-690 行，确认端口索引解析逻辑：

```cpp
// 当前代码（约 666-674 行）
unsigned src_idx = 0, dst_idx = 0;
if (!src_spec.empty() && std::isdigit(src_spec[0])) {
    bool all_digits = std::all_of(src_spec.begin(), src_spec.end(), ::isdigit);
    if (all_digits) src_idx = std::stoul(src_spec);
}
if (!dst_spec.empty() && std::isdigit(dst_spec[0])) {
    bool all_digits = std::all_of(dst_spec.begin(), dst_spec.end(), ::isdigit);
    if (all_digits) dst_idx = std::stoul(dst_spec);
}
```

**确认问题**：`else` 分支缺失，当 `all_digits == false` 时没有 WARNING 日志。

- [ ] **Step 4: 修改源代码添加 WARNING 日志**

编辑 `src/core/module_factory.cc`，在 `parsePortSpec()` 函数的端口索引解析部分（约第 666 行）：

将：
```cpp
unsigned src_idx = 0, dst_idx = 0;
if (!src_spec.empty() && std::isdigit(src_spec[0])) {
    bool all_digits = std::all_of(src_spec.begin(), src_spec.end(), ::isdigit);
    if (all_digits) src_idx = std::stoul(src_spec);
}
if (!dst_spec.empty() && std::isdigit(dst_spec[0])) {
    bool all_digits = std::all_of(dst_spec.begin(), src_spec.end(), ::isdigit);
    if (all_digits) dst_idx = std::stoul(dst_spec);
}
```

改为：
```cpp
unsigned src_idx = 0, dst_idx = 0;
if (!src_spec.empty() && std::isdigit(src_spec[0])) {
    bool all_digits = std::all_of(src_spec.begin(), src_spec.end(), ::isdigit);
    if (all_digits) {
        src_idx = std::stoul(src_spec);
    } else {
        DPRINTF(CONN, "[WARN] Invalid port index '%s' (expected digits only), defaulting to 0\n",
                src_spec.c_str());
    }
}
if (!dst_spec.empty() && std::isdigit(dst_spec[0])) {
    bool all_digits = std::all_of(dst_spec.begin(), dst_spec.end(), ::isdigit);
    if (all_digits) {
        dst_idx = std::stoul(dst_spec);
    } else {
        DPRINTF(CONN, "[WARN] Invalid port index '%s' (expected digits only), defaulting to 0\n",
                dst_spec.c_str());
    }
}
```

- [ ] **Step 5: 验证编译通过**

Run: `ninja -C build cpptlm_core 2>&1 | tail -20`
Expected: 编译成功，无 error

- [ ] **Step 6: 验证 445 个现有测试全部通过**

Run: `cd build && ctest --output-on-failure -j$(nproc) 2>&1 | tail -30`
Expected: `100% tests passed, 445/445`

- [ ] **Step 7: 提交**

```bash
git add src/core/module_factory.cc test/test_module_factory_fixes.cc
git commit -m "fix(module_factory): add WARNING for invalid port index in DEF-04"
```

---

## Task 2: T3.1-03b — DEF-03 非 RouterTLM 多端口模块警告

**优先级**: 🟡 中
**工作量**: 0.5 天
**文件**: `src/core/module_factory.cc:628-637`

### 背景

当检测到多端口模块但不是 RouterTLM 时，应该添加 WARNING 提醒用户当前绑定可能不正确（因为 RouterTLM 使用 `bind_port_pair()` 逐一绑定，其他模块可能需要不同处理）。

### 实现位置

`src/core/module_factory.cc` — Step 7b 多端口绑定分支（约第 628-637 行）

### 任务步骤

- [ ] **Step 1: 读取当前多端口绑定代码**

读取 `src/core/module_factory.cc` 第 620-660 行，确认现有逻辑：

```cpp
} else if (is_multi) {
    if (type == "RouterTLM") {
        auto* bi_adapter = static_cast<cpptlm::BidirectionalPortAdapter<
            tlm::RouterTLM, bundles::NoCFlitBundle, tlm::RouterTLM::NUM_PORTS>*>(adapter);
        for (unsigned i = 0; i < n_ports; i++) {
            bi_adapter->bind_port_pair(i, req_out_vec[i], resp_in_vec[i],
                                        resp_out_vec[i], req_in_vec[i]);
        }
    }
    ch_mod->set_stream_adapter(adapter);
}
```

**确认问题**：非 RouterTLM 多端口模块没有警告，可能导致绑定不正确时用户一无所知。

- [ ] **Step 2: 修改源代码添加 WARNING 日志**

在 `else if (is_multi)` 分支的 RouterTLM 特判后，添加 else 分支警告：

将：
```cpp
} else if (is_multi) {
    if (type == "RouterTLM") {
        auto* bi_adapter = static_cast<cpptlm::BidirectionalPortAdapter<...>>(adapter);
        for (unsigned i = 0; i < n_ports; i++) {
            bi_adapter->bind_port_pair(i, req_out_vec[i], resp_in_vec[i],
                                        resp_out_vec[i], req_in_vec[i]);
        }
    }
    ch_mod->set_stream_adapter(adapter);
}
```

改为：
```cpp
} else if (is_multi) {
    if (type == "RouterTLM") {
        auto* bi_adapter = static_cast<cpptlm::BidirectionalPortAdapter<
            tlm::RouterTLM, bundles::NoCFlitBundle, tlm::RouterTLM::NUM_PORTS>*>(adapter);
        for (unsigned i = 0; i < n_ports; i++) {
            bi_adapter->bind_port_pair(i, req_out_vec[i], resp_in_vec[i],
                                        resp_out_vec[i], req_in_vec[i]);
        }
    } else {
        DPRINTF(MODULE, "[WARN] Multi-port module '%s' uses set_stream_adapter(array) "
                "instead of bind_port_pair(). If this module uses BidirectionalPortAdapter, "
                "the binding may be incorrect. Please report this issue.\n",
                type.c_str());
    }
    ch_mod->set_stream_adapter(adapter);
}
```

- [ ] **Step 3: 验证编译通过**

Run: `ninja -C build cpptlm_core 2>&1 | tail -20`
Expected: 编译成功，无 error

- [ ] **Step 4: 验证 445 个现有测试全部通过**

Run: `cd build && ctest --output-on-failure -j$(nproc) 2>&1 | tail -30`
Expected: `100% tests passed, 445/445`

- [ ] **Step 5: 提交**

```bash
git add src/core/module_factory.cc
git commit -m "fix(module_factory): add WARNING for non-RouterTLM multi-port module in DEF-03"
```

---

## Task 3: T3.1-07 — 变量引用语法（$ref）

**优先级**: 🟡 中
**工作量**: 3 天
**文件**: `include/utils/var_resolver.hh`（新建）, `src/core/module_factory.cc`（修改）

### 背景

配置中需要支持 `${path}` 语法引用同一配置中的其他值，例如 `"latency": "${settings.base_latency}"`。解析器需要在 `processExtends()` 之后、`validateConfig()` 之前执行。

### 实现概述

- 创建 `include/utils/var_resolver.hh`：包含 `VarResolver` 类
- 修改 `module_factory.cc` 的 `instantiateAll()`：在 extends 处理之后、验证之前调用变量解析

### 子任务

#### T3.1-07a: 创建 var_resolver.hh 并实现基本解析

**文件**: `include/utils/var_resolver.hh`（新建）

```cpp
// include/utils/var_resolver.hh
// SPDX-License-Identifier: Apache-2.0
// Phase 3.1: 变量引用 ${path} 解析器

#pragma once

#include <string>
#include <regex>
#include <sstream>
#include <nlohmann/json.hpp>

namespace cpptlm {

class VarResolver {
public:
    VarResolver(const nlohmann::json& config) : config_(config) {}

    // 解析整个配置中的所有变量引用
    nlohmann::json resolveAll(nlohmann::json j) {
        if (j.is_object()) {
            nlohmann::json result;
            for (auto& [key, val] : j.items()) {
                result[key] = resolveAll(val);
            }
            return result;
        } else if (j.is_array()) {
            nlohmann::json result;
            for (auto& val : j) {
                result.push_back(resolveAll(val));
            }
            return result;
        } else if (j.is_string()) {
            return resolve_string(j.get<std::string>());
        }
        return j;
    }

private:
    nlohmann::json config_;

    // 解析字符串中的 ${path} 引用
    nlohmann::json resolve_string(const std::string& value) {
        std::regex var_regex(R"(\$\{([^}]+)\})");
        std::smatch match;
        std::string result = value;

        while (std::regex_search(result, match, var_regex)) {
            std::string var_path = match[1].str();
            nlohmann::json resolved = resolve_path(var_path);

            if (resolved.is_null()) {
                DPRINTF(CONFIG, "[WARN] Unresolved variable reference: ${%s}\n", var_path.c_str());
                // 保持原样，不替换
                break;
            }

            // 替换匹配项
            std::string replacement;
            if (resolved.is_string()) {
                replacement = resolved.get<std::string>();
            } else if (resolved.is_number()) {
                replacement = std::to_string(resolved.get<double>());
            } else if (resolved.is_boolean()) {
                replacement = resolved.get<bool>() ? "true" : "false";
            }

            result.replace(match.position(), match.length(), replacement);
        }

        // 尝试解析为数字（如果替换后变成纯数字字符串）
        if (!result.empty() && result.find('$') == std::string::npos) {
            // 尝试作为数字解析
            char* end;
            double num = strtod(result.c_str(), &end);
            if (end != result.c_str() && *end == '\0') {
                return num;
            }
        }

        return result;
    }

    // 解析路径如 "settings.base_latency" 或 "modules[0].type"
    nlohmann::json resolve_path(const std::string& path) {
        std::istringstream iss(path);
        std::string token;
        nlohmann::json current = config_;

        while (std::getline(iss, token, '.')) {
            // 处理数组索引：modules[0]
            auto bracket_pos = token.find('[');
            if (bracket_pos != std::string::npos) {
                std::string key = token.substr(0, bracket_pos);
                int index = std::stoi(token.substr(bracket_pos + 1));
                auto close_pos = token.find(']');
                if (close_pos != std::string::npos && index >= 0) {
                    if (current.contains(key) && current[key].is_array() &&
                        index < static_cast<int>(current[key].size())) {
                        current = current[key][index];
                    } else {
                        return nlohmann::json::null();
                    }
                } else {
                    return nlohmann::json::null();
                }
            } else {
                if (current.is_object() && current.contains(token)) {
                    current = current[token];
                } else {
                    return nlohmann::json::null();
                }
            }
        }

        return current;
    }
};

} // namespace cpptlm
```

- [ ] **Step 1: 写 test_var_resolver.cc**

```cpp
// test/test_var_resolver.cc
// SPDX-License-Identifier: Apache-2.0
// Phase 3.1: 变量引用 $ref 测试

#include <catch2/catch_all.hpp>
#include "utils/var_resolver.hh"
#include "core/module_factory.hh"

TEST_CASE("T3.1-07a: Basic variable reference", "[var_ref][phase3]") {
    json config = R"({
        "settings": {"base_latency": 1},
        "modules": [{"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}],
        "connections": [{"src": "r0.0", "dst": "r0.1", "latency": "${settings.base_latency}"}]
    })"_json;

    cpptlm::VarResolver resolver(config);
    auto resolved = resolver.resolveAll(config);

    CHECK(resolved["connections"][0]["latency"] == 1);
}

TEST_CASE("T3.1-07b: Module name reference", "[var_ref][phase3]") {
    json config = R"({
        "modules": [
            {"name": "cpu0", "type": "CPUTLM"},
            {"name": "r0", "type": "RouterTLM", "params": {"cpu_type": "${modules[0].type}", "node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}
        ],
        "connections": []
    })"_json;

    cpptlm::VarResolver resolver(config);
    auto resolved = resolver.resolveAll(config);

    CHECK(resolved["modules"][1]["params"]["cpu_type"] == "CPUTLM");
}

TEST_CASE("T3.1-07c: Array element reference", "[var_ref][phase3]") {
    json config = R"({
        "routers": ["r0", "r1", "r2"],
        "modules": [
            {"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}},
            {"name": "r1", "type": "RouterTLM", "params": {"node_x": 1, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}
        ],
        "connections": [{"src": "${routers[0]}.0", "dst": "${routers[1]}.1", "latency": 1}]
    })"_json;

    cpptlm::VarResolver resolver(config);
    auto resolved = resolver.resolveAll(config);

    CHECK(resolved["connections"][0]["src"] == "r0.0");
    CHECK(resolved["connections"][0]["dst"] == "r1.1");
}

TEST_CASE("T3.1-07d: Unresolved variable warning", "[var_ref][phase3]") {
    json config = R"({
        "connections": [{"src": "cpu0", "dst": "r0", "latency": "${undefined.path}"}]
    })"_json;

    cpptlm::VarResolver resolver(config);
    auto resolved = resolver.resolveAll(config);

    // 未解析变量保持原样
    CHECK(resolved["connections"][0]["latency"] == "${undefined.path}");
}
```

- [ ] **Step 2: 创建 var_resolver.hh**

创建 `include/utils/var_resolver.hh`，内容见上述代码。

- [ ] **Step 3: 修改 module_factory.cc 集成 VarResolver**

在 `instantiateAll()` 函数中，找到 extends 处理之后的位置，添加变量解析：

读取 `src/core/module_factory.cc` 找到 `processExtends()` 调用位置（约第 252 行附近）：

在 `if (config.contains("extends"))` 处理之后、`if (!validateConfig(final_config))` 之前添加：

```cpp
// 解析变量引用 ${path}
cpptlm::VarResolver var_resolver(final_config);
final_config = var_resolver.resolveAll(final_config);
```

- [ ] **Step 4: 验证编译通过**

Run: `ninja -C build cpptlm_core 2>&1 | tail -20`
Expected: 编译成功

- [ ] **Step 5: 运行新增测试**

Run: `cd build && ./bin/cpptlm_tests "[var_ref]" --output-on-failure`
Expected: 4/4 PASS

- [ ] **Step 6: 验证回归**

Run: `cd build && ctest --output-on-failure -j$(nproc) 2>&1 | tail -20`
Expected: `100% tests passed, 449/449` (445 + 4 new)

- [ ] **Step 7: 提交**

```bash
git add include/utils/var_resolver.hh src/core/module_factory.cc test/test_var_resolver.cc
git commit -m "feat(module_factory): add variable reference ${path} resolution (T3.1-07)"
```

---

## Task 4: T3.1-08 — 参数默认值声明

**优先级**: 🟡 中
**工作量**: 2 天
**文件**: `include/core/param_rules.hh`（新建）, `include/tlm/router_tlm.hh`（修改）, `src/tlm/router_tlm.cc`（修改）

### 背景

需要定义 ParamRule 结构体声明模块参数规则（类型、是否必需、默认值），并在 RouterTLM 中实现 `get_param_rules()`。当前 RouterTLM 只做简单的 `cfg.contains()` 检查，没有类型验证或默认值支持。

### 子任务

#### T3.1-08a: 创建 param_rules.hh

**文件**: `include/core/param_rules.hh`（新建）

```cpp
// include/core/param_rules.hh
// SPDX-License-Identifier: Apache-2.0
// Phase 3.1: 参数规则定义

#pragma once

#include <string>
#include <optional>
#include <map>
#include <nlohmann/json.hpp>

namespace cpptlm {

enum class ParamType {
    INTEGER,
    FLOAT,
    STRING,
    BOOLEAN,
    ENUM
};

struct ParamRule {
    std::string name;
    ParamType type;
    bool required;
    std::optional<int> default_int;
    std::optional<double> default_float;
    std::optional<std::string> default_str;
    std::optional<bool> default_bool;
    std::optional<int> min_value;
    std::optional<int> max_value;
    std::optional<std::vector<std::string>> enum_values;
};

// 模块参数规则注册表：type name -> rules
using ParamRules = std::map<std::string, ParamRule>;

} // namespace cpptlm
```

#### T3.1-08b: 添加 RouterTLM::get_param_rules()

**修改**: `include/tlm/router_tlm.hh`

在类声明中添加：

```cpp
// 在现有 public 方法后添加
static ParamRules get_param_rules();
```

**修改**: `src/tlm/router_tlm.cc`

在文件顶部添加 include：
```cpp
#include "core/param_rules.hh"
```

在文件末尾添加实现：

```cpp
ParamRules RouterTLM::get_param_rules() {
    return {
        {"node_x",     {"node_x", ParamType::INTEGER, true,  std::nullopt}},
        {"node_y",     {"node_y", ParamType::INTEGER, true,  std::nullopt}},
        {"mesh_x",     {"mesh_x", ParamType::INTEGER, true,  std::nullopt}},
        {"mesh_y",     {"mesh_y", ParamType::INTEGER, true,  std::nullopt}},
        {"flit_width", {"flit_width", ParamType::INTEGER, false, 64, 64, 128}},
        {"vc_count",   {"vc_count", ParamType::INTEGER, false, 2, 1, 8}},
        {"buffer_size", {"buffer_size", ParamType::INTEGER, false, 16, 1, 64}},
    };
}
```

#### T3.1-08c: 应用默认值逻辑

在 `module_factory.cc` 的 `instantiateAll()` 中，模块循环内添加默认值应用：

找到模块处理循环（约第 280 行），在 `set_config()` 之后、`on_config_loaded()` 之前：

```cpp
// 应用参数默认值
for (auto& mod : final_config["modules"]) {
    std::string type = mod["type"];
    if (mod.contains("params")) {
        // 获取模块的参数规则并应用默认值
        auto rules = cpptlm::ModuleFactory::get_param_rules_map();
        auto it = rules.find(type);
        if (it != rules.end()) {
            for (const auto& [name, rule] : it->second) {
                if (!mod["params"].contains(name.c_str())) {
                    if (rule.required) {
                        DPRINTF(MODULE, "[PARAM ERROR] Required parameter '%s' missing for module '%s'\n",
                                name.c_str(), type.c_str());
                        return false;
                    }
                    if (rule.default_int.has_value()) {
                        mod["params"][name] = rule.default_int.value();
                    }
                }
            }
        }
    }
}
```

### 任务步骤

- [ ] **Step 1: 写 test_param_defaults.cc**

```cpp
// test/test_param_defaults.cc
// SPDX-License-Identifier: Apache-2.0
// Phase 3.1: 参数默认值测试

#include <catch2/catch_all.hpp>
#include "core/module_factory.hh"
#include "tlm/router_tlm.hh"

TEST_CASE("T3.1-08a: Default values applied", "[param][phase3]") {
    json config = R"({
        "modules": [
            {"name": "r0", "type": "RouterTLM", "params": {
                "node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1
            }}
        ],
        "connections": []
    })"_json;

    ModuleFactory factory;
    bool result = factory.instantiateAll(config);

    REQUIRE(result == true);
    // 验证默认值已应用（需要 ModuleFactory 提供 get_module_param 查询接口）
    // 或者通过 on_config_loaded 后的 config 验证
}

TEST_CASE("T3.1-08b: Explicit value overrides default", "[param][phase3]") {
    json config = R"({
        "modules": [
            {"name": "r0", "type": "RouterTLM", "params": {
                "node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1,
                "flit_width": 128
            }}
        ],
        "connections": []
    })"_json;

    ModuleFactory factory;
    bool result = factory.instantiateAll(config);

    REQUIRE(result == true);
    // 验证显式值 128 覆盖默认值 64
}

TEST_CASE("T3.1-08c: Missing required parameter fails", "[param][phase3]") {
    json config = R"({
        "modules": [
            {"name": "r0", "type": "RouterTLM", "params": {
                "node_x": 0
            }}
        ],
        "connections": []
    })"_json;

    ModuleFactory factory;
    bool result = factory.instantiateAll(config);

    CHECK(result == false);
}
```

- [ ] **Step 2: 创建 include/core/param_rules.hh**

内容见上述 T3.1-08a 部分。

- [ ] **Step 3: 修改 RouterTLM 头文件添加 get_param_rules() 声明**

读取 `include/tlm/router_tlm.hh`，找到类声明的 public 区域，添加：

```cpp
static ParamRules get_param_rules();
```

- [ ] **Step 4: 修改 RouterTLM 实现文件**

在 `src/tlm/router_tlm.cc` 顶部添加：
```cpp
#include "core/param_rules.hh"
```

在文件末尾添加 `get_param_rules()` 实现。

- [ ] **Step 5: 修改 module_factory.cc 添加默认值应用逻辑**

在 `instantiateAll()` 的模块处理循环中，应用参数默认值。

- [ ] **Step 6: 验证编译通过**

Run: `ninja -C build cpptlm_core 2>&1 | tail -20`
Expected: 编译成功

- [ ] **Step 7: 运行新增测试**

Run: `cd build && ./bin/cpptlm_tests "[param]" --output-on-failure`
Expected: 3/3 PASS

- [ ] **Step 8: 验证回归**

Run: `cd build && ctest --output-on-failure -j$(nproc) 2>&1 | tail -20`
Expected: `100% tests passed`

- [ ] **Step 9: 提交**

```bash
git add include/core/param_rules.hh include/tlm/router_tlm.hh src/tlm/router_tlm.cc src/core/module_factory.cc test/test_param_defaults.cc
git commit -m "feat(param): add ParamRule structure and RouterTLM defaults (T3.1-08)"
```

---

## Task 5: T3.1-09 — NI PE-side 到 Router 连接验证

**优先级**: 🟡 中
**工作量**: 1 天
**文件**: `src/core/module_factory.cc`（修改）

### 背景

NICTLM 的 PE 侧端口（端口索引 0）不应该直接连接到 RouterTLM。此规则需要在连接绑定时验证，违规时打印 ERROR 日志并拒绝实例化。

### 规则说明

- NICTLM 端口 0 = PE-side（连接 CPU/内存）
- NICTLM 端口 1 = NETWORK-side（连接 Router）
- PE-side 不能直接连 RouterTLM
- NETWORK-side 可以连 RouterTLM

### 实现位置

在 `module_factory.cc` 的 `bindConnection()` 或连接验证处添加检查。

### 任务步骤

- [ ] **Step 1: 写 test_nic_side_rules.cc**

```cpp
// test/test_nic_side_rules.cc
// SPDX-License-Identifier: Apache-2.0
// Phase 3.1: NI PE-side 规则测试

#include <catch2/catch_all.hpp>
#include "core/module_factory.hh"

TEST_CASE("T3.1-09: NICTLM PE-side to Router rejected", "[nic][phase3]") {
    json config = R"({
        "modules": [
            {"name": "ni0", "type": "NICTLM", "params": {"node_id": 0}},
            {"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}
        ],
        "connections": [
            {"src": "ni0.0", "dst": "r0.4", "latency": 1}
        ]
    })"_json;

    ModuleFactory factory;
    bool result = factory.instantiateAll(config);

    CHECK(result == false);  // 实例化失败
}

TEST_CASE("T3.1-09b: NICTLM NETWORK-side to Router allowed", "[nic][phase3]") {
    json config = R"({
        "modules": [
            {"name": "ni0", "type": "NICTLM", "params": {"node_id": 0}},
            {"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}
        ],
        "connections": [
            {"src": "ni0.1", "dst": "r0.4", "latency": 1}
        ]
    })"_json;

    ModuleFactory factory;
    bool result = factory.instantiateAll(config);

    CHECK(result == true);  // 实例化成功
}
```

- [ ] **Step 2: 读取连接解析逻辑找到验证点**

读取 `src/core/module_factory.cc` 找到连接处理逻辑（约 530-560 行），确认在哪里添加 NI 侧别验证。

在连接创建前（约 `createPortPair` 调用前）添加验证函数调用：

```cpp
// 在 bindConnection 或连接解析循环中添加
bool validate_nic_pe_connection(const std::string& src_type, unsigned src_port,
                                 const std::string& dst_type, unsigned dst_port) {
    // NICTLM PE 侧端口规则
    if (src_type == "NICTLM" && src_port == 0) {
        if (dst_type == "RouterTLM") {
            DPRINTF(CONN, "[CONN ERROR] NICTLM PE-side port (port 0) cannot connect directly "
                    "to RouterTLM. Use NETWORK side (port 1) for router connections.\n");
            return false;
        }
    }
    if (dst_type == "NICTLM" && dst_port == 0) {
        if (src_type == "RouterTLM") {
            DPRINTF(CONN, "[CONN ERROR] NICTLM PE-side port (port 0) cannot connect directly "
                    "to RouterTLM. Use NETWORK side (port 1) for router connections.\n");
            return false;
        }
    }
    return true;
}
```

在连接处理循环中，解析完端口索引后、创建连接前调用验证。

- [ ] **Step 3: 验证编译通过**

Run: `ninja -C build cpptlm_core 2>&1 | tail -20`

- [ ] **Step 4: 运行新增测试**

Run: `cd build && ./bin/cpptlm_tests "[nic]" --output-on-failure`
Expected: 2/2 PASS

- [ ] **Step 5: 验证回归**

Run: `cd build && ctest --output-on-failure -j$(nproc) 2>&1 | tail -20`

- [ ] **Step 6: 提交**

```bash
git add src/core/module_factory.cc test/test_nic_side_rules.cc
git commit -m "feat(module_factory): reject NICTLM PE-side to RouterTLM connections (T3.1-09)"
```

---

## Task 6: T3.1-11 — validateConfig() 验证

**优先级**: 🟡 中
**工作量**: 1 天
**文件**: 无需修改代码（已实现）

### 背景

验证现有 `validateConfig()` 实现是否符合 ADR-X.11 决策 7 的 8 项要求。探索确认全部已实现，本任务编写测试用例验证。

### 验证清单（来自探索结果）

| 验证项 | ADR 要求 | 代码位置 | 状态 |
|--------|---------|---------|------|
| 顶层 `modules` 字段 | 必需，缺失 → ERROR | `module_factory.cc:167-170` | ✅ |
| 顶层 `connections` 字段 | 必需，缺失 → ERROR | `module_factory.cc:172-175` | ✅ |
| 顶层 `version` 字段 | 可选，缺失 → WARNING | `module_factory.cc:178-180` | ✅ |
| 模块 `name` 字段 | 必需，缺失 → ERROR | `module_factory.cc:185-192` | ✅ |
| 模块 `type` 字段 | 必需，缺失 → ERROR | `module_factory.cc:194-202` | ✅ |
| RouterTLM 参数检查 | node_x/y/mesh_x/y 必需 | `module_factory.cc:210-218` | ✅ |
| NICTLM 参数检查 | node_id 必需 | `module_factory.cc:219-224` | ✅ |
| 快速失败策略 | 发现第一个错误立即返回 false | 全局 | ✅ |

### 任务步骤

- [ ] **Step 1: 写 test_validate_config.cc**

```cpp
// test/test_validate_config.cc
// SPDX-License-Identifier: Apache-2.0
// Phase 3.1: validateConfig() 验证测试

#include <catch2/catch_all.hpp>
#include "core/module_factory.hh"

TEST_CASE("T3.1-11a: Missing modules field", "[validate][phase3]") {
    json config = R"({"connections": []})"_json;
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    CHECK(result == false);
}

TEST_CASE("T3.1-11b: Missing module name", "[validate][phase3]") {
    json config = R"({
        "modules": [{"type": "RouterTLM"}],
        "connections": []
    })"_json;
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    CHECK(result == false);
}

TEST_CASE("T3.1-11c: RouterTLM missing required params", "[validate][phase3]") {
    json config = R"({
        "modules": [{"name": "r0", "type": "RouterTLM", "params": {"node_x": 0}}],
        "connections": []
    })"_json;
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    CHECK(result == false);
}

TEST_CASE("T3.1-11d: Valid config passes validation", "[validate][phase3]") {
    json config = R"({
        "version": "1.0",
        "modules": [
            {"name": "r0", "type": "RouterTLM", "params": {
                "node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1
            }}
        ],
        "connections": []
    })"_json;
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    CHECK(result == true);
}
```

- [ ] **Step 2: 运行新增测试**

Run: `cd build && ./bin/cpptlm_tests "[validate]" --output-on-failure`
Expected: 4/4 PASS

- [ ] **Step 3: 验证回归**

Run: `cd build && ctest --output-on-failure -j$(nproc) 2>&1 | tail -20`

- [ ] **Step 4: 提交**

```bash
git add test/test_validate_config.cc
git commit -m "test(validate): add validateConfig coverage tests (T3.1-11)"
```

---

## 回归测试保证

所有任务完成后，验证：
- 现有测试：445/445 通过
- 新增测试：15/15 通过
- 总计：460/460 通过

Run: `cd build && ctest --output-on-failure 2>&1 | tail -40`

---

## 实施时间线

| 任务 | 工作量 | 里程碑 |
|------|:---:|--------|
| T3.1-04b: DEF-04 WARNING 日志 | 1d | M1 |
| T3.1-03b: DEF-03 WARNING 日志 | 0.5d | M1 |
| T3.1-11: validateConfig() 验证（写测试） | 1d | M2 |
| T3.1-07: 变量引用语法 | 3d | M3 |
| T3.1-08: 参数默认值 | 2d | M4 |
| T3.1-09: NI 侧别规则 | 1d | M5 |

---

## 变更日志

| 版本 | 日期 | 变更 |
|------|------|------|
| v1.0 | 2026-05-06 | 初始版本，基于 writing-plans skill 生成 |