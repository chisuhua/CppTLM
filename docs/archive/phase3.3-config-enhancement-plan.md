# Phase 3.3: 配置能力增强实施计划

> **版本**: v1.1
> **编制日期**: 2026-05-07
> **基于文档**: ADR-X.10 v3.0 (参数框架), ADR-X.11 v3.0 (配置继承), SPEC-010 v3.0
> **前置条件**: Phase 3.2 已完成 + 50+ 新测试通过
> **预计工期**: 4 周 (Week 5-8)
> **目标**: 实现 nlohmann/json 驱动的参数框架，Credit Flow 自动计算
> **状态**: 📋 路线图（Phase 3.1 已实现 ParamRule 结构体，Phase 3.3 将扩展为 JSON 序列化版本）

---

## 一、阶段概述

### 1.1 阶段目标

Phase 3.3 聚焦于建立声明式参数框架和 Credit Flow 配置，核心目标包括:

1. **参数规则声明**: 使用 nlohmann/json 结构体序列化替代静态方法模式
2. **参数验证**: 实现两阶段验证 (Schema 验证 → 语义验证)
3. **参数推导**: 支持三元表达式推导规则
4. **Credit Flow**: 自动计算 credit_capacity 和 credit_return_latency
5. **双重验证**: Python Pydantic + C++ ModuleFactory 协作

### 1.2 与 Phase 3.1 已实现代码的关系

> **重要说明**: Phase 3.1 已实现 `include/core/param_rules.hh`，定义了基础 ParamRule 结构体。
> Phase 3.3 将扩展为 nlohmann/json 序列化版本，支持从 `configs/param_rules/*.json` 加载规则。

| 组件 | Phase 3.1 | Phase 3.3 (计划) |
|------|-----------|-----------------|
| ParamRule 结构体 | ✅ 已实现（std::optional） | 升级为 JSON 序列化 |
| get_param_rules() | ✅ RouterTLM 已实现 | 扩展到所有模块 |
| 默认值应用 | ✅ 已实现 | 扩展 derive_expr |
| 参数验证 | ❌ 未实现 | 实现两阶段验证 |
| 全局规则文件 | ❌ 未实现 | `configs/param_rules/*.json` |

---

## 一、阶段概述

### 1.1 阶段目标

Phase 3.3 聚焦于建立声明式参数框架和 Credit Flow 配置，核心目标包括:

1. **参数规则声明**: 使用 nlohmann/json 结构体序列化替代静态方法模式
2. **参数验证**: 实现两阶段验证 (Schema 验证 → 语义验证)
3. **参数推导**: 支持三元表达式推导规则
4. **Credit Flow**: 自动计算 credit_capacity 和 credit_return_latency
5. **双重验证**: Python Pydantic + C++ ModuleFactory 协作

### 1.2 共识事项覆盖

| 共识编号 | 内容 | 状态 |
|---------|------|------|
| **G2** | Credit Flow 配置决策 | 本阶段实施 |
| **G5** | 参数验证与 set_config() 协作 | 本阶段实施 |
| **G6** | 层级拓扑与子网络决策 | Phase 4+ 预留 |

### 1.3 关键设计变更

| 变更项 | 旧设计 (v1.0) | 新设计 (v2.0) |
|--------|-------------|-------------|
| 参数规则声明 | 静态 `get_param_rules()` 方法 | nlohmann/json 结构体序列化 |
| 参数验证 | 硬编码验证逻辑 | ParamRule 驱动验证 |
| set_config() | 无异常处理 | 抛出 ParamValidationError |
| Credit Flow | 未提及 | 自动计算 + 手动覆盖 |

---

## 二、任务清单

### 2.1 C++ 端任务

| 任务 ID | 任务描述 | 工作量 | 依赖 | 验收标准 |
|---------|---------|:---:|:---:|---------|
| T3.3-01 | **创建 `param_rules.hh`** — ParamRule, ParamType, ModuleParamRules + nlohmann/json 宏 | 1d | 无 | 枚举序列化/反序列化测试通过 |
| T3.3-02 | **创建 `param_parser.hh`** — 类型转换 (latency/address) + derive_expr 评估 | 2d | T3.3-01 | `"3ns"` → cycles, `"256MB"` → bytes |
| T3.3-03 | **创建 `param_errors.hh`** — ParamValidationError 异常 | 0.5d | 无 | 异常携带 module/param/reason |
| T3.3-04 | **ModuleFactory 加载 ParamRule 验证** | 2d | T3.3-01~03 | JSON 参数规则验证正确 |
| T3.3-05 | **set_config() 异常处理** — 验证失败抛出 ParamValidationError | 1d | T3.3-03~04 | 异常触发回滚 |
| T3.3-06 | **全局参数规则文件** — `configs/param_rules/*.json` | 0.5d | T3.3-01 | RouterTLM/NICTLM 规则文件可用 |
| T3.3-10 | **参数推导表达式解析器** — 三元表达式 `(cond) ? v1 : v2` | 1.5d | T3.3-02 | `"(mesh_x >= 4) ? 8 : 4"` 评估正确 |
| T3.3-11 | **参数系统单元测试** — C++ 端验证/解析/推导测试 | 2d | T3.3-01~10 | 25+ 测试用例通过 |

**小计**: 10.5 天 (约 2.5 周)

### 2.2 Python 端任务

| 任务 ID | 任务描述 | 工作量 | 依赖 | 验收标准 |
|---------|---------|:---:|:---:|---------|
| T3.3-07 | **Credit Flow 自动计算** — credit_capacity 公式 + credit_return_latency | 1.5d | 无 | Python 自动计算正确，支持手动覆盖 |
| T3.3-08 | **Credit Flow JSON Schema 扩展** — connection.credit_flow 字段 | 0.5d | T3.3-07 | Schema 验证 credit_flow 字段 |
| T3.3-09 | **两阶段验证集成** — Python Pydantic + C++ ModuleFactory 协作 | 1.5d | T3.3-04, Phase 3.2 | 双重验证行为一致 |
| T3.3-12 | **Python 参数系统测试** | 1d | T3.3-07~09 | 15+ 测试用例通过 |

**小计**: 4.5 天 (约 1 周)

### 2.3 集成与文档任务

| 任务 ID | 任务描述 | 工作量 | 依赖 | 验收标准 |
|---------|---------|:---:|:---:|---------|
| T3.3-13 | **端到端集成测试** — 完整配置加载 + Credit Flow 验证 | 1d | 所有开发任务 | mesh_2x2/4x4 Credit Flow 通过 |
| T3.3-14 | **更新架构文档** — 记录参数框架设计 | 0.5d | T3.3-13 | 文档评审通过 |
| T3.3-15 | **更新用户指南** — 参数配置指南 | 0.5d | T3.3-13 | 文档评审通过 |

**小计**: 2 天 (约 0.5 周)

---

## 三、详细设计

### 3.1 param_rules.hh

**文件路径**: `include/core/param_rules.hh`

**核心内容**:

```cpp
#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <map>

namespace cpptlm {

enum class ParamType { INT, UNSIGNED, STRING, ADDRESS, LATENCY, BOOL };

NLOHMANN_JSON_SERIALIZE_ENUM(ParamType, {
    {ParamType::INT, "int"},
    {ParamType::UNSIGNED, "unsigned"},
    {ParamType::STRING, "string"},
    {ParamType::ADDRESS, "address"},
    {ParamType::LATENCY, "latency"},
    {ParamType::BOOL, "bool"},
})

struct ParamRule {
    ParamType type = ParamType::STRING;
    bool required = false;
    std::string default_val;    // JSON string, 由 ParamParser 转换
    std::string min_val;        // 数值最小值
    std::string max_val;        // 数值最大值
    std::string derive_expr;    // "(mesh_x >= 4) ? 8 : 4"
    std::string description;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    ParamRule, type, required, default_val, min_val, max_val, derive_expr, description)

struct ModuleParamRules {
    std::string module_type;
    std::map<std::string, ParamRule> rules;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ModuleParamRules, module_type, rules)

} // namespace cpptlm
```

### 3.2 param_parser.hh

**文件路径**: `include/core/param_parser.hh`

**核心内容**:

```cpp
#pragma once
#include "param_rules.hh"
#include <string>
#include <map>
#include <variant>

namespace cpptlm {

struct ParamParseResult {
    bool success;
    std::string error_message;
    std::variant<int64_t, uint64_t, double, std::string, bool> value;
};

class ParamParser {
public:
    // 类型转换
    static ParamParseResult parse(const std::string& input, ParamType type,
                                  double clock_frequency_mhz = 1000.0);
    
    // 范围验证
    static bool validate_range(const ParamParseResult& result, const ParamRule& rule);
    
    // 推导表达式评估
    static int64_t evaluate_derive_expr(const std::string& expr,
                                        const std::map<std::string, int64_t>& params);
    
private:
    // Latency 单位解析
    static uint64_t parse_latency(const std::string& s, double clock_frequency_mhz);
    
    // Address 解析
    static uint64_t parse_address(const std::string& s);
    
    // 三元表达式解析
    static int64_t evaluate_ternary(const std::string& expr,
                                    const std::map<std::string, int64_t>& params);
};

} // namespace cpptlm
```

### 3.3 param_errors.hh

**文件路径**: `include/core/param_errors.hh`

**核心内容**:

```cpp
#pragma once
#include <stdexcept>
#include <string>

namespace cpptlm {

class ParamValidationError : public std::invalid_argument {
public:
    std::string module_name;
    std::string param_name;
    std::string rule_violated;
    
    ParamValidationError(const std::string& module, const std::string& param,
                        const std::string& reason)
        : std::invalid_argument(reason),
          module_name(module),
          param_name(param),
          rule_violated(reason) {}
};

} // namespace cpptlm
```

### 3.4 ModuleFactory 参数验证集成

**修改文件**: `src/core/module_factory.cc`

**关键变更**:

1. **加载全局参数规则**:

```cpp
// src/core/module_factory.cc

class ModuleFactory {
private:
    std::map<std::string, ModuleParamRules> param_rules_;  // module_type -> rules
    
    // 加载全局参数规则
    bool load_param_rules(const std::string& rules_dir) {
        // 加载 configs/param_rules/*.json
        for (const auto& entry : std::filesystem::directory_iterator(rules_dir)) {
            if (entry.path().extension() == ".json") {
                std::ifstream f(entry.path());
                json rules_json = json::parse(f);
                ModuleParamRules rules = rules_json.get<ModuleParamRules>();
                param_rules_[rules.module_type] = rules;
            }
        }
        return true;
    }
};
```

2. **验证参数 (G5 共识)**:

```cpp
// src/core/module_factory.cc

bool ModuleFactory::validate_params(const std::string& module_type,
                                     const json& params,
                                     const ModuleParamRules& rules) {
    if (rules.rules.empty()) return true;  // 无规则，跳过
    
    for (const auto& [param_name, rule] : rules.rules) {
        // 必需参数检查
        if (rule.required && !params.contains(param_name)) {
            printf("[PARAM ERROR] Module '%s' missing required param '%s'\n",
                   module_type.c_str(), param_name.c_str());
            return false;
        }
        
        if (params.contains(param_name)) {
            const auto& value = params[param_name];
            
            // 类型检查
            if (rule.type == ParamType::INT && !value.is_number_integer()) {
                printf("[PARAM ERROR] Module '%s' param '%s' must be int\n",
                       module_type.c_str(), param_name.c_str());
                return false;
            }
            
            // 范围检查
            if (!rule.min_val.empty() && value.is_number_integer()) {
                int64_t val = value.get<int64_t>();
                int64_t min_val = std::stoll(rule.min_val);
                if (val < min_val) {
                    printf("[PARAM ERROR] Module '%s' param '%s' (%lld) below min (%lld)\n",
                           module_type.c_str(), param_name.c_str(), val, min_val);
                    return false;
                }
            }
            if (!rule.max_val.empty() && value.is_number_integer()) {
                int64_t val = value.get<int64_t>();
                int64_t max_val = std::stoll(rule.max_val);
                if (val > max_val) {
                    printf("[PARAM ERROR] Module '%s' param '%s' (%lld) above max (%lld)\n",
                           module_type.c_str(), param_name.c_str(), val, max_val);
                    return false;
                }
            }
        }
    }
    return true;
}
```

3. **set_config() 异常处理**:

```cpp
// src/core/module_factory.cc

void ModuleFactory::set_config(BaseModule* module, const json& params) {
    std::string module_type = module->get_type();
    
    // 查找参数规则
    auto it = param_rules_.find(module_type);
    if (it != param_rules_.end()) {
        // 验证参数
        if (!validate_params(module_type, params, it->second)) {
            throw ParamValidationError(
                module->get_name(),
                "multiple",
                "Parameter validation failed (see error log above)"
            );
        }
    }
    
    // 应用参数
    module->set_config(params);
}
```

### 3.5 Credit Flow 自动计算 (G2 共识)

**Python 端** (`cpptlm_config/builder.py` 扩展):

```python
class ConfigBuilder:
    def calculate_credit_capacity(self, router_config: dict, connections: list) -> int:
        """自动计算 credit_capacity (G2 共识)
        
        公式: credit_capacity = buffer_size × port_count / avg_latency
        """
        buffer_size = router_config.get("buffer_size", 16)
        port_count = len(connections)
        if port_count == 0:
            return buffer_size
        avg_latency = sum(c.latency for c in connections) / port_count
        return int(buffer_size * port_count / max(avg_latency, 1))
    
    def auto_configure_credit_flow(self) -> "ConfigBuilder":
        """为所有 Router-to-Router 连接自动配置 Credit Flow (G2 共识)"""
        for conn in self.connections:
            # 识别 Router-to-Router 连接
            src_type = self._get_module_type(conn.src.split(".")[0])
            dst_type = self._get_module_type(conn.dst.split(".")[0])
            
            if src_type == ModuleType.ROUTER_TLM and dst_type == ModuleType.ROUTER_TLM:
                # 自动计算 credit_capacity
                router_conns = [c for c in self.connections 
                               if self._get_module_type(c.src.split(".")[0]) == ModuleType.ROUTER_TLM]
                
                # 应用 Credit Flow 配置
                conn.credit_flow = {
                    "enable": True,
                    "credit_capacity": self.calculate_credit_capacity({}, router_conns),
                    "credit_return_latency": conn.latency * 2  # 往返延迟
                }
        
        return self
```

**JSON 输出格式**:

```json
{
    "connections": [
        {
            "src": "router_0.1",
            "dst": "router_1.3",
            "latency": 1,
            "credit_flow": {
                "enable": true,
                "credit_capacity": 32,
                "credit_return_latency": 2
            }
        }
    ]
}
```

### 3.6 全局参数规则文件

> **ADR-X.10 决策 5 合规**: derive_expr 仅支持简单比较（`>=`, `<`, `==`）和逻辑运算（`&&`, `||`），
> **不支持算术运算**（如 `mesh_x * mesh_y`）。如需基于算术的推导，应由 Python 配置脚本预先计算。
> 参数优先级：**显式值 > derive_expr > default_val**

**文件路径**: `configs/param_rules/router_tlm.json`

```json
{
    "module_type": "RouterTLM",
    "rules": {
        "node_x": {
            "type": "int",
            "required": true,
            "min_val": "0",
            "max_val": "15",
            "description": "Router X coordinate in mesh"
        },
        "node_y": {
            "type": "int",
            "required": true,
            "min_val": "0",
            "max_val": "15",
            "description": "Router Y coordinate in mesh"
        },
        "mesh_x": {
            "type": "int",
            "required": true,
            "min_val": "1",
            "max_val": "16",
            "description": "Mesh width"
        },
        "mesh_y": {
            "type": "int",
            "required": true,
            "min_val": "1",
            "max_val": "16",
            "description": "Mesh height"
        },
        "vc_count": {
            "type": "int",
            "required": false,
            "default_val": "2",
            "min_val": "1",
            "max_val": "8",
            "derive_expr": "(mesh_x >= 4 && mesh_y >= 4) ? 4 : 2",
            "description": "Virtual channel count"
        },
        "flit_width": {
            "type": "int",
            "required": false,
            "default_val": "64",
            "min_val": "32",
            "max_val": "256",
            "derive_expr": "(mesh_x >= 4) ? 128 : 64",
            "description": "Flit width in bits"
        }
    }
}
```

---

## 四、时间线

### 4.1 周度计划

```
Week 5: 参数规则基础
  Day 1-2: param_rules.hh (T3.3-01)
  Day 3-4: param_parser.hh (T3.3-02)
  Day 5:   param_errors.hh (T3.3-03)

Week 6: ModuleFactory 参数验证集成
  Day 1-2: 加载 ParamRule 验证 (T3.3-04)
  Day 3:   set_config() 异常处理 (T3.3-05)
  Day 4:   全局参数规则文件 (T3.3-06)
  Day 5:   参数推导表达式解析器 (T3.3-10) 开始

Week 7: Credit Flow 与双重验证
  Day 1-2: Credit Flow 自动计算 (T3.3-07~08)
  Day 3-4: 两阶段验证集成 (T3.3-09)
  Day 5:   参数推导表达式解析器完成 (T3.3-10)

Week 8: 测试、集成与文档
  Day 1-2: C++ 参数系统单元测试 (T3.3-11)
  Day 3:   Python 参数系统测试 (T3.3-12)
  Day 4:   端到端集成测试 (T3.3-13)
  Day 5:   文档更新 (T3.3-14~15)
```

### 4.2 里程碑

| 里程碑 | 日期 | 验收标准 |
|--------|------|---------|
| M1: 参数规则基础完成 | Week 5 | param_rules.hh + param_parser.hh + param_errors.hh 可用 |
| M2: ModuleFactory 验证完成 | Week 6 | ParamRule 驱动验证 + set_config() 异常处理工作正常 |
| M3: Credit Flow 完成 | Week 7 | 自动计算 + 双重验证集成工作正常 |
| M4: Phase 3.3 发布 | Week 8 | 所有测试通过 (40+)，文档完整，共识事项全部实施 |

---

## 五、测试策略

### 5.1 C++ 单元测试

**测试文件**: `test/test_param_rules.cc`

| 测试类别 | 测试数量 | 覆盖内容 |
|---------|:---:|---------|
| ParamRule 序列化 | 5 | JSON ↔ ParamRule 双向转换、默认值处理 |
| 枚举转换 | 3 | ParamType 字符串映射 |
| 参数验证 | 8 | 必需参数、类型检查、范围检查 |
| 参数推导 | 4 | 三元表达式评估 |
| ParamValidationError | 5 | 异常携带信息、回滚验证 |
| **总计** | **25** | |

**测试用例示例**:

```cpp
TEST_CASE("ParamRule: JSON serialization") {
    ParamRule rule{ParamType::INT, true, "4", "1", "8", "(mesh_x >= 4) ? 8 : 4", "VC count"};
    
    json j = rule;
    REQUIRE(j["type"] == "int");
    REQUIRE(j["required"] == true);
    REQUIRE(j["default_val"] == "4");
    REQUIRE(j["min_val"] == "1");
    REQUIRE(j["max_val"] == "8");
    REQUIRE(j["derive_expr"] == "(mesh_x >= 4) ? 8 : 4");
    
    ParamRule rule2 = j.get<ParamRule>();
    REQUIRE(rule.type == rule2.type);
    REQUIRE(rule.required == rule2.required);
}

TEST_CASE("ParamParser: derive expression") {
    std::map<std::string, int64_t> params = {{"mesh_x", 4}, {"mesh_y", 4}};
    REQUIRE(ParamParser::evaluate_derive_expr("(mesh_x >= 4) ? 8 : 4", params) == 8);
    
    params["mesh_x"] = 2;
    REQUIRE(ParamParser::evaluate_derive_expr("(mesh_x >= 4) ? 8 : 4", params) == 4);
}
```

### 5.2 Python 单元测试

**测试文件**: `cpptlm_config/tests/test_credit_flow.py`

| 测试类别 | 测试数量 | 覆盖内容 |
|---------|:---:|---------|
| Credit Flow 计算 | 5 | 公式正确性、边界条件 |
| Credit Flow JSON | 3 | Schema 验证、字段正确性 |
| 双重验证 | 4 | Python Pydantic + C++ ModuleFactory 一致性 |
| Credit Flow 手动覆盖 | 3 | 手动值优先于自动计算 |
| **总计** | **15** | |

### 5.3 集成测试

**测试文件**: `test/test_credit_flow_integration.cc`

| 测试场景 | 验证内容 |
|---------|---------|
| mesh_2x2 Credit Flow | 自动计算正确，仿真通过 |
| mesh_4x4 Credit Flow | 大规模拓扑 Credit Flow 正确 |
| 参数验证失败 | 必需参数缺失时抛出异常 |
| 推导表达式评估 | vc_count/flit_width 推导正确 |

### 5.4 回归测试保证

- **现有测试**: 434/434 必须全部通过
- **Phase 3.2 测试**: 50/50 必须全部通过
- **新增测试**: 40+ (C++ 25+, Python 15+)

---

## 六、风险与缓解

### 6.1 技术风险

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|:---:|:---:|---------|
| 三元表达式解析器复杂度超预期 | 中 | 中 | 严格限制语法子集 (ADR-X.10 决策 5)，不支持嵌套 |
| Python Pydantic 与 C++ ParamRule 验证不一致 | 中 | 高 | 共享 JSON 规则文件源，CI 检查一致性 |
| Credit Flow 公式在非对称拓扑中不准确 | 中 | 低 | 允许手动覆盖，文档说明适用场景 |
| 推导表达式引用不存在的参数 | 高 | 高 | Stage 1 先验证所有引用参数存在 |

### 6.2 进度风险

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|:---:|:---:|---------|
| ParamParser 类型转换逻辑复杂 | 中 | 低 | 分层实现 (latency → address → derive_expr)，逐步测试 |
| 全局参数规则文件与代码不同步 | 低 | 中 | CI 自动检查规则文件有效性 |

---

## 七、交付物清单

### 7.1 代码交付物

| 文件 | 类型 | 说明 |
|------|------|------|
| `include/core/param_rules.hh` | 新建 | ParamRule, ParamType, ModuleParamRules + JSON 宏 |
| `include/core/param_parser.hh` | 新建 | ParamParser 类型转换 + derive_expr 评估 |
| `include/core/param_errors.hh` | 新建 | ParamValidationError 异常定义 |
| `src/core/module_factory.cc` | 修改 | ParamRule 验证、set_config() 异常 |
| `cpptlm_config/builder.py` | 修改 | Credit Flow 自动计算 |
| `configs/param_rules/router_tlm.json` | 新建 | RouterTLM 参数规则 |
| `configs/param_rules/nic_tlm.json` | 新建 | NICTLM 参数规则 |
| `test/test_param_rules.cc` | 新建 | C++ 参数系统单元测试 |
| `cpptlm_config/tests/test_credit_flow.py` | 新建 | Python Credit Flow 测试 |

### 7.2 文档交付物

| 文件 | 说明 |
|------|------|
| `docs/architecture/14-parameter-system.md` | 参数系统架构设计 |
| `docs/guide/PARAMETER_CONFIGURATION_GUIDE.md` | 参数配置指南 |

---

## 八、验收标准

| 验收项 | 标准 | 验证方式 |
|--------|------|---------|
| param_rules.hh | ParamRule 序列化正确 | 25+ 单元测试通过 |
| ParamParser | `"3ns"`, `"256MB"` 转换正确 | 类型转换测试 |
| ParamValidationError | 异常携带详细信息 | 异常测试 |
| ModuleFactory 验证 | ParamRule 验证逻辑正确 | 参数验证测试 |
| set_config() 异常 | 验证失败抛出异常并回滚 | 集成测试 |
| 全局规则文件 | RouterTLM/NICTLM 规则可用 | 配置加载测试 |
| Credit Flow 自动计算 | 公式计算正确 | Python 单元测试 |
| Credit Flow JSON | 输出包含 credit_flow 字段 | JSON Schema 验证 |
| 推导表达式 | `"(mesh_x >= 4) ? 8 : 4"` 评估正确 | 表达式解析测试 |
| 回归测试 | 484/484 通过 | `ctest --output-on-failure` |

---

## 九、与后续阶段的接口

Phase 3.3 完成后，为 Phase 3.4 和 Phase 4+ 提供以下接口:

| 后续阶段 | 依赖接口 | 说明 |
|---------|---------|------|
| Phase 3.4 | validator.py 使用参数验证 | VALID-05 负载分析依赖参数 |
| Phase 4+ | 子网络嵌套使用参数继承 | HIER-01 子网络参数传递 |
| Phase 4+ | Credit Flow 扩展 | NET-07 虚拟网络 Credit 配置 |

---

**文档状态**: 📋 路线图（Phase 3.1 已实现基础 ParamRule，Phase 3.3 将扩展为 JSON 序列化版本）

| 版本 | 日期 | 变更 |
|------|------|------|
| v1.0 | 2026-05-05 | 初始版本，基于 ADR-X.10/X.11 v3.0 和 Phase 3+ 实施计划 v2.0 编制 |
| v1.1 | 2026-05-07 | 更新状态为路线图；添加与 Phase 3.1 已实现代码的关系说明；标注 ParamRule 已实现（Phase 3.1） |

