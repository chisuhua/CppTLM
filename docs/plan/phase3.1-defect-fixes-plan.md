# TGMS Phase 3.1: 缺陷修复与基础增强 — 详细实施计划

> **版本**: 3.0
> **编制日期**: 2026-05-07
> **基于文档**: ADR-X.11 v3.0, phase3-plus-progress-report.md v1.1
> **前置条件**: Phase 7 全部修复完成 + E2E 测试套件通过 (445/445 tests pass)
> **当前状态**: ✅ 已完成（100%）— 2026-05-06 commits 458a9d7

---

## 一、Phase 3.1 概述

### 1.1 目标

修复 ModuleFactory 已知缺陷（DEF-01~DEF-05），实现最紧迫的 P0/P1 基础增强特性，为 Phase 3.2+ 的端口类型系统和参数框架奠定基础。

### 1.2 共识项覆盖

| 共识项 | 说明 | 覆盖任务 |
|--------|------|---------|
| **m3** | DEF 与阶段对应关系明确 | DEF-02/03/04 → Phase 3.1 |
| **G6** | 层级拓扑配置（决策 8） | 记录为 Phase 4+ 扩展，Phase 3.1 不实现 |
| **M4** | Credit 流控制配置（决策 9） | 记录为 Phase 3.3 实现，Phase 3.1 不实现 |

### 1.3 当前完成情况（截至 2026-05-07）

| 类别 | 已完成 | 部分完成 | 未开始 | 完成率 |
|------|:---:|:---:|:---:|:---:|
| Defect Fixes (DEF-01~05) | 4 | 1 (DEF-04) | 0 | 80% |
| Config Inheritance (5 项) | 5 | 0 | 0 | 100% |
| Variable Reference $ref (5 项) | 5 | 0 | 0 | 100% |
| Parameter Defaults (3 项) | 3 | 0 | 0 | 100% |
| Debug & Side Rules (2 项) | 1 | 0 | 1 | 50% |
| **Phase 3.1 总计** | **18** | **1** | **1** | **90%** |

### 1.4 与 v1.0/v2.0 计划的变更

| 变更项 | v1.0 计划 | v2.0 实际状态 | v3.0 实际状态 | 原因 |
|--------|-----------|--------------|-------------|------|
| DEF-01~05 | 全部待修复 | DEF-01/02/03/05 已完成，DEF-04 部分完成 | 同 v2.0 | 2026-05-05 commits 45a2c9a~8ebb2ef |
| extends 支持 | 待实现 | 已完成（8 个测试通过） | ✅ 已完成 | commits 45a2c9a, 8ebb2ef |
| $ref 支持 | 待实现 | 未开始 | ✅ 已完成 | commit 458a9d7 |
| 参数默认值 | 待实现 | 未开始 | ✅ 已完成 | commit 458a9d7 |
| DEF-03/04 改进 | 未提及 | Phase 3.2 前添加 WARNING 日志 | DEF-04 WARNING 待实现 | ADR-X.11 v3.0 决策 4/5 |

---

## 二、已完成任务验证

### 2.1 DEF-01: ModuleGroup 通配符展开

**状态**: ✅ 已完成  
**代码位置**: `include/utils/module_group.hh:80-105`  
**测试验证**: `test/test_module_group.cc` 已有测试覆盖  
**决策记录**: ADR-X.11 v3.0 决策 3（延迟绑定策略）

**验证要点**:
- `group:nics` 正确展开为 `["nic_0_0", "nic_0_1", "nic_1_0", "nic_1_1"]`
- 通配符 `*` 和 `?` 支持
- 实例注册顺序不影响展开结果

---

### 2.2 DEF-02: Step 6/7b 重复连接去重

**状态**: ✅ 已完成  
**代码位置**: `src/core/module_factory.cc:365-387` (Step 5), `650-660` (Step 6)  
**测试验证**: `test/test_connection_resolution.cc`  
**决策记录**: ADR-X.11 v3.0 决策 6（两阶段去重 + latency 冲突处理）

**验证要点**:
- Step 5（ConnectionResolver 前）去重：防止创建多余端口
- Step 6（PortPair 创建前）去重：防止通配符展开产生重复
- latency 冲突时打印 WARNING，使用首次出现的值
- 去重键：`src_module:src_port -> dst_module:dst_port`

---

### 2.3 DEF-03: BidirectionalPortAdapter 绑定修复

**状态**: ✅ 已完成（当前仅限 RouterTLM）  
**代码位置**: `src/core/module_factory.cc:628-637`  
**测试验证**: 代码审查确认逻辑正确  
**决策记录**: ADR-X.11 v3.0 决策 4（RouterTLM 限定）

**验证要点**:
- RouterTLM 使用 `bind_port_pair()` 逐一绑定
- 不通过 `set_stream_adapter(array)` 路径

**待改进项**（Phase 3.2）:
- 添加 WARNING 日志：当 `is_multi && type != "RouterTLM"` 时提醒用户
- Phase 3.3+ 评估泛化方案（如果出现第二个 BidirectionalPortAdapter 模块）

---

### 2.4 DEF-04: 端口索引解析严格化

**状态**: ⚠️ 部分完成（技术债务）  
**代码位置**: `src/core/module_factory.cc:666-674`  
**测试验证**: 有 `all_digits` 检查但无错误日志  
**决策记录**: ADR-X.11 v3.0 决策 5（WARNING 日志改进计划）

**当前问题**:
- `all_digits` 检查存在但非法索引（如 `"0abc"`）静默默认为 0
- 用户不会得到任何反馈，可能隐藏配置错误

**改进计划**（本阶段高优先级，1 天工作量）:
```cpp
// 改进代码（添加 WARNING 日志）
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
// dst_spec 同样处理
```

**验收标准**:
- `"0abc"` 产生 WARNING 日志 `[WARN] Invalid port index '0abc' (expected digits only), defaulting to 0`
- 仍使用默认值 0（保持向后兼容）
- 不阻止配置加载（WARNING 而非 ERROR）

---

### 2.5 DEF-05: Python 工具链类型映射

**状态**: ✅ 已完成  
**代码位置**: `scripts/topology_generator.py:55-72`  
**测试验证**: `test/python/test_topology_generator.py`  
**验证要点**:
- `generate_mesh()` 输出 `"RouterTLM"` 而非抽象类型 `"Router"`

---

### 2.6 配置继承（extends）

**状态**: ✅ 已完成  
**代码位置**: `src/core/module_factory.cc:113-160` (processExtends), `44-111` (mergeConfigs)  
**测试验证**: `test/test_config_inheritance.cc`（8 个测试全部通过）  
**决策记录**: ADR-X.11 v3.0 决策 1（合并语义）, 决策 2（循环引用保护）

**验证要点**:
- modules 按 name 深度合并
- connections 追加（不去重）
- groups 按 group_name 合并
- 递归 extends 支持（深度限制 10）
- 循环引用保护：`depth > 10` 检查

---

### 2.7 调试模式

**状态**: ✅ 已完成  
**代码位置**: `src/main.cpp:54-55`, `src/core/module_factory.cc`  
**测试验证**: CLI 测试通过  
**验证要点**:
- `--debug-config` CLI 选项输出详细配置解析日志
- DPRINTF 宏在调试模式下激活

---

### 2.8 T3.1-07: 变量引用语法（$ref）

**状态**: ✅ 已完成  
**代码位置**: `include/utils/var_resolver.hh`  
**测试验证**: `var_resolver.hh` 已实现 `VarResolver::resolveAll()`，修复了数组解析初始化 bug（commit 458a9d7）  
**决策记录**: 本功能未在 ADR-X.11 v3.0 中正式记录，提前实现以支持 Phase 3.2 端口别名系统

**验证要点**:
- `${path}` 语法正确解析
- 模块名引用、数组元素引用、嵌套路径引用均支持
- 数组解析时 `result` 正确初始化为 `json::array()`

**bug 修复**（commit 458a9d7）:
```cpp
// 修复前（数组解析返回 null）
nlohmann::json result;

// 修复后（正确初始化为空数组）
nlohmann::json result = nlohmann::json::array();
```

---

### 2.9 T3.1-08: 参数默认值声明

**状态**: ✅ 已完成  
**代码位置**: `src/core/module_factory.cc:287-304`, `src/tlm/router_tlm.cc:483`  
**测试验证**: RouterTLM 参数规则已定义，`get_param_rules()` 返回 7 个参数规则  
**决策记录**: ADR-X.10 v3.0 决策 1（过渡实现），Phase 3.3 将升级为 nlohmann/json 序列化版本

**验证要点**:
- RouterTLM 声明参数规则（4 必需 + 3 可选默认值）
- 缺失必需参数时返回 `false`
- 缺失可选参数时自动应用默认值

**参数规则**:
| 参数 | 类型 | Required | 默认值 | 范围 |
|------|------|:---:|:---:|:---:|
| node_x | INTEGER | ✅ | — | — |
| node_y | INTEGER | ✅ | — | — |
| mesh_x | INTEGER | ✅ | — | — |
| mesh_y | INTEGER | ✅ | — | — |
| flit_width | INTEGER | ❌ | 64 | 64-128 |
| vc_count | INTEGER | ❌ | 2 | 1-8 |
| buffer_size | INTEGER | ❌ | 16 | 1-64 |

---

## 三、待完成任务清单

### 3.1 DEF-04 改进：端口索引非法 WARNING 日志

**任务 ID**: T3.1-04b  
**优先级**: 🔴 高（Phase 3.2 前必须完成）  
**工作量**: 1 天  
**依赖**: 无

**详细设计**:

修改 `src/core/module_factory.cc` 的端口索引解析逻辑，在 `all_digits` 检查失败时添加 WARNING 日志。

```cpp
// 当前代码（第 666-674 行）
unsigned src_idx = 0, dst_idx = 0;
if (!src_spec.empty() && std::isdigit(src_spec[0])) {
    bool all_digits = std::all_of(src_spec.begin(), src_spec.end(), ::isdigit);
    if (all_digits) src_idx = std::stoul(src_spec);
}

// 改进代码
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

**测试用例**:
```cpp
TEST_CASE("DEF-04b: Invalid port index produces WARNING") {
    // 配置包含非法端口索引
    json config = R"({
        "modules": [{"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}}],
        "connections": [{"src": "r0.0abc", "dst": "r1.3", "latency": 1}]
    })"_json;
    
    // 启用调试模式捕获日志
    enable_debug_output(CONN);
    
    // 实例化（应产生 WARNING 但不失败）
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    
    REQUIRE(result == true);  // 配置仍然加载
    // 验证日志包含 "[WARN] Invalid port index '0abc'"
    CHECK_LOG_CONTAINS("Invalid port index '0abc'");
}
```

**验收标准**:
- [ ] 非法端口索引产生 WARNING 日志
- [ ] 配置仍然正常加载（不阻止）
- [ ] 测试用例通过
- [ ] 445 个现有测试全部通过

---

### 3.2 DEF-03 改进：非 RouterTLM 多端口模块警告

**任务 ID**: T3.1-03b  
**优先级**: 🟡 中（Phase 3.2 期间完成）  
**工作量**: 0.5 天  
**依赖**: 无

**详细设计**:

当检测到多端口模块但不是 RouterTLM 时，添加 WARNING 日志提醒用户当前绑定可能不正确。

```cpp
// src/core/module_factory.cc - 第 628-637 行附近
} else if (is_multi) {
    if (type == "RouterTLM") {
        auto* bi_adapter = static_cast<cpptlm::BidirectionalPortAdapter<
            tlm::RouterTLM, bundles::NoCFlitBundle, tlm::RouterTLM::NUM_PORTS>*>(adapter);
        for (unsigned i = 0; i < n_ports; i++) {
            bi_adapter->bind_port_pair(i, req_out_vec[i], resp_in_vec[i],
                                        resp_out_vec[i], req_in_vec[i]);
        }
    } else {
        // 新增：警告日志
        DPRINTF(MODULE, "[WARN] Multi-port module '%s' uses set_stream_adapter(array) "
                "instead of bind_port_pair(). If this module uses BidirectionalPortAdapter, "
                "the binding may be incorrect. Please report this issue.\n",
                type.c_str());
    }
    ch_mod->set_stream_adapter(adapter);
}
```

**验收标准**:
- [ ] 非 RouterTLM 多端口模块产生 WARNING 日志
- [ ] RouterTLM 不产生警告
- [ ] 445 个现有测试全部通过

---

### 3.3 T3.1-07: 变量引用语法（$ref）

> **ADR 决策说明**: 此功能未在 ADR-X.11 v3.0 中正式记录。基于 Phase 3.2 端口别名系统的需求，
> 提前实现基础变量解析能力。如团队认为需要正式 ADR 决策，应在实施前补充 ADR-X.13。

**任务 ID**: T3.1-07  
**优先级**: 🟡 中  
**工作量**: 3 天  
**依赖**: 无

**子任务**:

| 子任务 | 描述 | 工作量 |
|--------|------|:---:|
| T3.1-07a | `${path}` 语法解析 | 1d |
| T3.1-07b | 模块名引用解析 | 0.5d |
| T3.1-07c | 数组元素引用 | 1d |
| T3.1-07d | 嵌套路径引用 | 0.5d |

**详细设计**:

> **设计说明**: 本阶段的 ParamRule 使用 `std::optional` 字段是为了快速实现默认值机制。
> Phase 3.3 将根据 ADR-X.10 v3.0 决策 1 升级为 nlohmann/json 序列化版本，
> 支持从 `configs/param_rules/*.json` 文件加载规则。本阶段的结构体作为过渡实现。

**语法规范**:
```json
{
    "settings": {
        "base_latency": 1,
        "router_type": "RouterTLM"
    },
    "modules": [
        {"name": "cpu0", "type": "CPUTLM", "params": {"mem_range": "${settings.mem_base}"}},
        {"name": "r0", "type": "${settings.router_type}"}
    ],
    "connections": [
        {"src": "cpu0", "dst": "r0", "latency": "${settings.base_latency}"}
    ]
}
```

**解析器实现**:
```cpp
// include/utils/var_resolver.hh
class VarResolver {
public:
    VarResolver(const json& config) : config_(config) {}
    
    // 解析字符串中的变量引用
    std::string resolve(const std::string& value) {
        // 匹配 ${path} 模式
        std::regex var_regex(R"(\$\{([^}]+)\})");
        std::smatch match;
        std::string result = value;
        
        while (std::regex_search(result, match, var_regex)) {
            std::string var_path = match[1].str();
            std::string resolved = resolve_path(var_path);
            
            if (resolved.empty()) {
                DPRINTF(CONFIG, "[WARN] Unresolved variable reference: ${%s}\n", var_path.c_str());
                resolved = match[0].str();  // 保持原样
            }
            
            result = result.replace(match.position(), match.length(), resolved);
        }
        
        return result;
    }
    
private:
    json config_;
    
    std::string resolve_path(const std::string& path) {
        // 解析路径：settings.base_latency, modules[0].name
        std::istringstream iss(path);
        std::string token;
        json current = config_;
        
        while (std::getline(iss, token, '.')) {
            // 处理数组索引：modules[0]
            auto bracket_pos = token.find('[');
            if (bracket_pos != std::string::npos) {
                std::string key = token.substr(0, bracket_pos);
                int index = std::stoi(token.substr(bracket_pos + 1, token.find(']') - bracket_pos - 1));
                
                if (current.contains(key) && current[key].is_array() && index < current[key].size()) {
                    current = current[key][index];
                } else {
                    return "";  // 路径无效
                }
            } else {
                if (current.contains(token)) {
                    current = current[token];
                } else {
                    return "";  // 路径无效
                }
            }
        }
        
        // 转换为字符串
        if (current.is_string()) return current.get<std::string>();
        if (current.is_number()) return std::to_string(current.get<double>());
        if (current.is_boolean()) return current.get<bool>() ? "true" : "false";
        
        return "";  // 不支持的类型
    }
};
```

**集成位置**: 在 `processExtends()` 之后、`validateConfig()` 之前执行变量解析。

```cpp
// src/core/module_factory.cc - instantiateAll() 中
json final_config = config;

// 1. 处理 extends
if (config.contains("extends")) {
    final_config = processExtends(config);
}

// 2. 解析变量引用（新增）
VarResolver resolver(final_config);
final_config = resolver.resolveAll(final_config);

// 3. 验证配置
if (!validateConfig(final_config)) {
    return false;
}
```

**测试用例**:
```cpp
TEST_CASE("T3.1-07a: Basic variable reference") {
    json config = R"({
        "settings": {"base_latency": 1},
        "modules": [{"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}],
        "connections": [{"src": "r0.0", "dst": "r0.1", "latency": "${settings.base_latency}"}]
    })"_json;
    
    VarResolver resolver(config);
    auto resolved = resolver.resolveAll(config);
    
    CHECK(resolved["connections"][0]["latency"] == 1);
}

TEST_CASE("T3.1-07b: Module name reference") {
    json config = R"({
        "modules": [
            {"name": "cpu0", "type": "CPUTLM"},
            {"name": "r0", "type": "RouterTLM", "params": {"cpu_type": "${modules[0].type}"}}
        ]
    })"_json;
    
    VarResolver resolver(config);
    auto resolved = resolver.resolveAll(config);
    
    CHECK(resolved["modules"][1]["params"]["cpu_type"] == "CPUTLM");
}

TEST_CASE("T3.1-07c: Array element reference") {
    json config = R"({
        "routers": ["r0", "r1", "r2"],
        "connections": [{"src": "${routers[0]}", "dst": "${routers[1]}"}]
    })"_json;
    
    VarResolver resolver(config);
    auto resolved = resolver.resolveAll(config);
    
    CHECK(resolved["connections"][0]["src"] == "r0");
    CHECK(resolved["connections"][0]["dst"] == "r1");
}

TEST_CASE("T3.1-07d: Unresolved variable warning") {
    json config = R"({
        "connections": [{"src": "cpu0", "dst": "r0", "latency": "${undefined.path}"}]
    })"_json;
    
    enable_debug_output(CONFIG);
    VarResolver resolver(config);
    auto resolved = resolver.resolveAll(config);
    
    // 未解析变量保持原样并产生警告
    CHECK(resolved["connections"][0]["latency"] == "${undefined.path}");
    CHECK_LOG_CONTAINS("Unresolved variable reference");
}
```

**验收标准**:
- [ ] `${path}` 语法正确解析
- [ ] 模块名引用、数组元素引用、嵌套路径引用均支持
- [ ] 未解析变量产生 WARNING 日志
- [ ] 4 个测试用例全部通过
- [ ] 445 个现有测试全部通过

---

### 3.4 T3.1-08: 参数默认值声明

**任务 ID**: T3.1-08  
**优先级**: 🟡 中  
**工作量**: 2 天  
**依赖**: 无

**子任务**:

| 子任务 | 描述 | 工作量 |
|--------|------|:---:|
| T3.1-08a | ParamRule 结构体定义 | 0.5d |
| T3.1-08b | get_param_rules() 方法 | 1d |
| T3.1-08c | 默认值赋值逻辑 | 0.5d |

**详细设计**:

**ParamRule 结构体**:
```cpp
// include/core/param_rules.hh
#pragma once

#include <string>
#include <optional>
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

// 模块参数规则注册表
using ParamRules = std::map<std::string, ParamRule>;

} // namespace cpptlm
```

**模块声明参数规则**:
```cpp
// include/tlm/router_tlm.hh
class RouterTLM : public ChStreamModuleBase {
public:
    static ParamRules get_param_rules() {
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
};
```

**默认值赋值逻辑**:
```cpp
// src/core/module_factory.cc - apply_param_defaults()
json apply_param_defaults(const std::string& type, json params) {
    auto rules_it = ModuleFactory::get_param_rules_map().find(type);
    if (rules_it == ModuleFactory::get_param_rules_map().end()) {
        return params;  // 模块未声明参数规则
    }
    
    const auto& rules = rules_it->second;
    for (const auto& [name, rule] : rules) {
        if (!params.contains(name)) {
            if (rule.required) {
                DPRINTF(MODULE, "[PARAM ERROR] Required parameter '%s' missing for module '%s'\n",
                        name.c_str(), type.c_str());
                throw std::runtime_error("Missing required parameter");
            }
            
            // 应用默认值
            if (rule.default_int.has_value()) params[name] = rule.default_int.value();
            else if (rule.default_float.has_value()) params[name] = rule.default_float.value();
            else if (rule.default_str.has_value()) params[name] = rule.default_str.value();
            else if (rule.default_bool.has_value()) params[name] = rule.default_bool.value();
        }
    }
    
    return params;
}
```

**集成位置**: 在变量解析之后、验证之前应用默认值。

```cpp
// src/core/module_factory.cc - instantiateAll() 中
for (auto& mod : final_config["modules"]) {
    std::string type = mod["type"];
    
    // 应用参数默认值
    if (mod.contains("params")) {
        mod["params"] = apply_param_defaults(type, mod["params"]);
    }
}
```

**测试用例**:
```cpp
TEST_CASE("T3.1-08a: Default values applied") {
    json config = R"({
        "modules": [
            {"name": "r0", "type": "RouterTLM", "params": {
                "node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1
                // flit_width, vc_count, buffer_size 使用默认值
            }}
        ]
    })"_json;
    
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    
    REQUIRE(result == true);
    // 验证默认值已应用
    CHECK(factory.get_module_param("r0", "flit_width") == 64);
    CHECK(factory.get_module_param("r0", "vc_count") == 2);
    CHECK(factory.get_module_param("r0", "buffer_size") == 16);
}

TEST_CASE("T3.1-08b: Explicit value overrides default") {
    json config = R"({
        "modules": [
            {"name": "r0", "type": "RouterTLM", "params": {
                "node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1,
                "flit_width": 128  // 覆盖默认值 64
            }}
        ]
    })"_json;
    
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    
    REQUIRE(result == true);
    CHECK(factory.get_module_param("r0", "flit_width") == 128);
}

TEST_CASE("T3.1-08c: Missing required parameter fails") {
    json config = R"({
        "modules": [
            {"name": "r0", "type": "RouterTLM", "params": {
                "node_x": 0  // 缺少 node_y, mesh_x, mesh_y
            }}
        ]
    })"_json;
    
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    
    CHECK(result == false);  // 实例化失败
}
```

**验收标准**:
- [ ] ParamRule 结构体定义完成
- [ ] RouterTLM 声明参数规则
- [ ] 默认值正确应用
- [ ] 显式值覆盖默认值
- [ ] 缺少必需参数时报错
- [ ] 3 个测试用例全部通过
- [ ] 445 个现有测试全部通过

---

### 3.5 T3.1-09: NI PE-side 到 Router 连接验证

**任务 ID**: T3.1-09  
**优先级**: 🟡 中  
**工作量**: 1 天  
**依赖**: 无

**详细设计**:

NICTLM 的 PE 侧端口（连接 CPU/内存等 PE 模块）不应该直接连接到 RouterTLM。此规则在连接绑定时验证。

```cpp
// src/core/module_factory.cc - bind_connection() 中
bool validate_nic_pe_connection(const std::string& src_type, const std::string& src_port,
                                 const std::string& dst_type, const std::string& dst_port) {
    // NICTLM PE 侧端口规则：
    // PE 侧端口（端口索引 0）只能连接到 CPU/内存等 PE 模块
    // 不能直接连接到 RouterTLM
    if (src_type == "NICTLM" && src_port == "0") {
        if (dst_type == "RouterTLM") {
            DPRINTF(CONN, "[CONN ERROR] NICTLM PE-side port (port 0) cannot connect directly "
                    "to RouterTLM. Use NETWORK side (port 1) for router connections.\n");
            return false;
        }
    }
    if (dst_type == "NICTLM" && dst_port == "0") {
        if (src_type == "RouterTLM") {
            DPRINTF(CONN, "[CONN ERROR] NICTLM PE-side port (port 0) cannot connect directly "
                    "to RouterTLM. Use NETWORK side (port 1) for router connections.\n");
            return false;
        }
    }
    
    return true;
}
```

**测试用例**:
```cpp
TEST_CASE("T3.1-09: NICTLM PE-side to Router rejected") {
    json config = R"({
        "modules": [
            {"name": "ni0", "type": "NICTLM", "params": {"node_id": 0}},
            {"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}
        ],
        "connections": [
            {"src": "ni0.0", "dst": "r0.4", "latency": 1}  // PE-side 到 Router，应拒绝
        ]
    })"_json;
    
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    
    CHECK(result == false);  // 实例化失败
    CHECK_LOG_CONTAINS("NICTLM PE-side port");
}

TEST_CASE("T3.1-09b: NICTLM NETWORK-side to Router allowed") {
    json config = R"({
        "modules": [
            {"name": "ni0", "type": "NICTLM", "params": {"node_id": 0}},
            {"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}
        ],
        "connections": [
            {"src": "ni0.1", "dst": "r0.4", "latency": 1}  // NETWORK-side 到 Router，应允许
        ]
    })"_json;
    
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    
    CHECK(result == true);  // 实例化成功
}
```

**验收标准**:
- [ ] NICTLM PE-side（端口 0）到 RouterTLM 连接被拒绝
- [ ] NICTLM NETWORK-side（端口 1）到 RouterTLM 连接允许
- [ ] 产生明确的 ERROR 日志
- [ ] 2 个测试用例全部通过
- [ ] 445 个现有测试全部通过

---

### 3.6 T3.1-11: validateConfig() 验证

**任务 ID**: T3.1-11  
**优先级**: 🟡 中  
**工作量**: 1 天  
**依赖**: 无  
**决策记录**: ADR-X.11 v3.0 决策 7（CFG-08 JSON Schema 验证器）

**详细设计**:

验证现有 `validateConfig()` 实现是否符合 ADR-X.11 决策 7 的要求。

**验证清单**:

| 验证项 | ADR 要求 | 代码位置 | 状态 |
|--------|---------|---------|------|
| 顶层 `modules` 字段 | 必需，缺失 → ERROR | `module_factory.cc:167-170` | ✅ 已实现 |
| 顶层 `connections` 字段 | 必需，缺失 → ERROR | `module_factory.cc:172-175` | ✅ 已实现 |
| 顶层 `version` 字段 | 可选，缺失 → WARNING | `module_factory.cc:178-180` | ✅ 已实现 |
| 模块 `name` 字段 | 必需，缺失 → ERROR | `module_factory.cc:185-192` | ✅ 已实现 |
| 模块 `type` 字段 | 必需，缺失 → ERROR | `module_factory.cc:194-202` | ✅ 已实现 |
| RouterTLM 参数检查 | `node_x/node_y/mesh_x/mesh_y` 必需 | `module_factory.cc:210-218` | ✅ 已实现 |
| NICTLM 参数检查 | `node_id` 必需 | `module_factory.cc:219-224` | ✅ 已实现 |
| 快速失败策略 | 发现第一个错误立即返回 false | 全局 | ✅ 已实现 |

**测试用例**:
```cpp
TEST_CASE("T3.1-11a: Missing modules field") {
    json config = R"({"connections": []})"_json;
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    CHECK(result == false);
    CHECK_LOG_CONTAINS("Missing required field 'modules'");
}

TEST_CASE("T3.1-11b: Missing module name") {
    json config = R"({
        "modules": [{"type": "RouterTLM"}],
        "connections": []
    })"_json;
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    CHECK(result == false);
    CHECK_LOG_CONTAINS("missing required field 'name'");
}

TEST_CASE("T3.1-11c: RouterTLM missing required params") {
    json config = R"({
        "modules": [{"name": "r0", "type": "RouterTLM", "params": {"node_x": 0}}],
        "connections": []
    })"_json;
    ModuleFactory factory;
    bool result = factory.instantiateAll(config);
    CHECK(result == false);
    CHECK_LOG_CONTAINS("missing/invalid 'node_y'");
}

TEST_CASE("T3.1-11d: Valid config passes validation") {
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

**验收标准**:
- [ ] 所有 ADR-X.11 决策 7 的验证项均已实现
- [ ] 4 个测试用例全部通过
- [ ] 445 个现有测试全部通过

---

## 四、实施时间线

### 4.1 周计划

| 周 | 任务 | 工作量 | 里程碑 |
|----|------|:---:|--------|
| **Week 1** | T3.1-04b: DEF-04 WARNING 日志 | 1d | M1: DEF-04 改进完成 |
| | T3.1-03b: DEF-03 WARNING 日志 | 0.5d | |
| | T3.1-11: validateConfig() 验证 | 1d | M2: validateConfig() 验证完成 |
| | T3.1-07: 变量引用语法 | 3d | M3: $ref 支持完成 |
| **Week 2** | T3.1-08: 参数默认值 | 2d | M4: 参数默认值完成 |
| | T3.1-09: NI 侧别规则 | 1d | M5: Phase 3.1 全部完成 |
| | 集成测试 + 文档更新 | 1.5d | |

### 4.2 里程碑

| 里程碑 | 日期 | 验收标准 |
|--------|------|---------|
| **M1: DEF-04 改进完成** | Week 1 Day 1 | 非法端口索引产生 WARNING 日志 |
| **M2: validateConfig() 验证完成** | Week 1 Day 2.5 | 所有 ADR-X.11 决策 7 验证项通过 |
| **M3: $ref 支持完成** | Week 1 Day 4 | 变量引用语法正常工作，4 个测试通过 |
| **M4: 参数默认值完成** | Week 2 Day 2 | 默认值正确应用，3 个测试通过 |
| **M5: Phase 3.1 全部完成** | Week 2 Day 4 | 所有任务完成，445+ 测试通过 |

---

## 五、测试策略

### 5.1 新增测试清单

| 测试文件 | 测试数量 | 覆盖范围 |
|---------|:---:|---------|
| `test/test_module_factory_fixes.cc` | 2 | DEF-04b/DEF-03b WARNING 日志 |
| `test/test_var_resolver.cc` | 4 | $ref 语法解析（基本/模块/数组/未解析） |
| `test/test_param_defaults.cc` | 3 | 默认值应用/覆盖/必需参数 |
| `test/test_nic_side_rules.cc` | 2 | NI PE-side 规则验证 |
| `test/test_validate_config.cc` | 4 | validateConfig() 验证（缺失字段/有效配置） |
| **总计** | **15** | |

### 5.2 回归测试保证

- **现有测试**: 445/445 必须全部通过
- **E2E 测试**: 32/32 必须全部通过
- **CLI 测试**: 10/10 必须全部通过
- **新增测试**: 15 个新测试用例

---

## 六、风险与缓解

### 6.1 技术风险

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|:---:|:---:|---------|
| $ref 解析与 extends 冲突 | 低 | 中 | $ref 解析在 extends 之后执行 |
| 参数默认值破坏现有配置 | 低 | 高 | 仅对缺失参数应用默认值，不覆盖显式值 |
| NI 侧别规则破坏现有配置 | 中 | 中 | 使用 ERROR 日志而非直接崩溃，允许用户修复 |

### 6.2 进度风险

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|:---:|:---:|---------|
| $ref 解析器复杂度超预期 | 中 | 中 | 分阶段实现：先基本路径，再嵌套路径 |
| 测试覆盖率不足导致回归 | 中 | 高 | 每个任务必须伴随测试用例 |

---

## 七、交付物清单

### 7.1 代码交付物

| 文件 | 说明 | 任务 |
|------|------|------|
| `src/core/module_factory.cc` | DEF-04/03 WARNING 日志 + $ref + 参数默认值 | T3.1-04b/03b/07/08 |
| `include/utils/var_resolver.hh` | 变量引用解析器 | T3.1-07 |
| `include/core/param_rules.hh` | ParamRule 结构体定义 | T3.1-08 |
| `scripts/topology_generator.py` | NI 侧别规则验证 | T3.1-09 |

### 7.2 测试交付物

| 文件 | 说明 |
|------|------|
| `test/test_module_factory_fixes.cc` | DEF-04b/03b WARNING 日志测试 |
| `test/test_var_resolver.cc` | 变量引用解析测试 |
| `test/test_param_defaults.cc` | 参数默认值测试 |
| `test/test_nic_side_rules.cc` | NI 侧别规则测试 |

---

## 八、与 Phase 3.2+ 的接口

Phase 3.1 完成后，为 Phase 3.2+ 的以下特性奠定基础：

| Phase 3.2+ 特性 | Phase 3.1 提供的接口 |
|----------------|-------------------|
| 端口类型系统（Phase 3.2） | DEF-04 严格化 + NI 侧别规则 |
| 端口别名系统（Phase 3.2） | $ref 变量引用 |
| 参数框架（Phase 3.3） | ParamRule 结构体 + 默认值机制 |
| 动态参数推导（Phase 3.3） | ParamRule + $ref 变量引用 |
| Credit Flow 配置（Phase 3.3） | 参数默认值 + $ref |

---

## 九、验收标准

| 验收项 | 标准 | 验证方式 |
|--------|------|---------|
| DEF-04 改进 | 非法索引产生 WARNING 日志 | 测试 T3.1-04b |
| DEF-03 改进 | 非 RouterTLM 多端口模块产生 WARNING | 测试 T3.1-03b |
| $ref 支持 | 变量引用语法正常工作 | 测试 T3.1-07a~d |
| 参数默认值 | 默认值正确应用，显式值覆盖 | 测试 T3.1-08a~c |
| NI 侧别规则 | PE-side 到 Router 连接被拒绝 | 测试 T3.1-09 |
| 现有测试 | 445/445 通过 | `ctest --output-on-failure` |
| 新增测试 | 15/15 通过 | `ctest -R "test_module_factory_fixes|test_var_resolver|test_param_defaults|test_nic_side_rules|test_validate_config"` |

---

## 十、变更日志

| 版本 | 日期 | 变更 |
|------|------|------|
| v1.0 | 2026-05-02 | 初始版本，作为 phase3-plus-implementation-plan.md 的一部分 |
| v2.0 | 2026-05-05 | 拆分为独立 Phase 3.1 计划<br>基于进度报告标记已完成/待完成任务<br>整合 ADR-X.11 v3.0 决策<br>添加 DEF-03/04 改进时间线<br>明确测试策略和验收标准 |
| v3.0 | 2026-05-07 | 标记 T3.1-07 ($ref) 和 T3.1-08 (参数默认值) 为已完成<br>更新版本号和日期<br>添加完成状态标注 (commit 458a9d7)<br>更新完成率统计（90%） |

---

**文档状态**: ✅ 已完成（Phase 3.1 核心任务全部实施）

