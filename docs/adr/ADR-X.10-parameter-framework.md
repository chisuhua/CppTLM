# ADR-X.10: Phase 3+ 参数框架

> **版本**: 3.0
> **日期**: 2026-05-05
> **状态**: 📋 待实施（Phase 3.3）
> **关联**: TGMS Phase 3.3, proposal.md Decision 3, ARCH-012 Gap Analysis, ADR-X.9 (端口类型系统)
> **变更**: v2.0 → v3.0: 补充术语统一、ParamRule 验证协作、set_config() 异常处理

---

## 术语

本 ADR 使用以下术语体系（与 ADR-X.9 一致）：

| 术语 | 定义 |
|------|------|
| **Port Type** | 端口类型的广义术语，包含 PortRole 和 BundleType |
| **PortRole** | 端口角色枚举（`initiator`, `target`, `bi_directional`, `network`, `pe`） |
| **BundleType** | 捆绑类型枚举（`cache_req`, `cache_resp`, `noc_flit`, `generic`） |
| **ParamRule** | 参数规则，定义参数的类型、范围、默认值、推导表达式 |
| **ParamType** | 参数类型枚举（`int`, `unsigned`, `string`, `address`, `latency`, `bool`） |

**阶段编号说明**（与 ADR-X.11 一致）：
- **Phase 3+**：Phase 3.1-3.4 的统称
- **Phase 3.2**：端口管理阶段
- **Phase 3.3**：配置增强阶段（本 ADR 的实施阶段）
- **Phase 3.4**：拓扑验证阶段

**命名规范**（m1 共识）：
- Python 包名：`cpptlm_config`（下划线），PyPI 发布名：`cpptlm-config`（连字符）
- 中文文档使用"配置生成器"，首次出现时标注 `（Config Generator）`

---

## 背景

当前 CppTLM 的参数处理极为简单：参数直接作为 JSON 字段传递到模块构造函数，缺少：
- 默认值声明（每个模块在构造函数中硬编码默认值）
- 范围验证（mesh_x 可以设为 -1 或 999 而不报错）
- 参数推导（flit_width、vc_count 应根据拓扑规模自动计算）
- 类型转换（不支持 "3ns"、"256MB" 等人类友好格式）

Phase 3.3 的目标是建立声明式参数框架，将参数逻辑从构造函数中抽离。

---

## 决策 1: 使用 nlohmann/json 结构体序列化

### 问题

如何设计参数规则声明系统，让用户通过 JSON 配置完全控制参数行为？

### 选项对比

| 选项 | 设计 | 优点 | 缺点 |
|------|------|------|------|
| **A) nlohmann/json 结构体序列化** ✅ | 定义 ParamRule struct + JSON 宏，配置驱动 | 用户通过 JSON 控制参数规则，无需改 C++ 代码 | 需要 C++ struct 与 JSON schema 同步 |
| B) 静态 get_param_rules() 方法 | 模块类实现静态方法返回规则映射 | 编译时验证 | 每次修改规则需要改 C++ 代码 |
| C) 外部规则文件 | JSON/YAML 文件声明规则 | 规则与代码分离 | 规则与实现可能不同步 |

### 决策

✅ **选项 A) nlohmann/json 结构体序列化**

与端口类型系统一致，使用 nlohmann/json 宏实现 ParamRule 的自动序列化/反序列化。

**核心模式**:
```cpp
// include/core/param_rules.hh
#include <nlohmann/json.hpp>

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

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ParamRule, type, required, default_val, min_val, max_val, derive_expr, description)

// 模块参数规则定义 - 直接在 JSON 配置中声明
struct ModuleParamRules {
    std::string module_type;
    std::map<std::string, ParamRule> rules;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ModuleParamRules, module_type, rules)
```

**用户使用方式（JSON 配置或全局规则文件）**:

方案 A：在模块配置中直接声明规则
```json
{
    "modules": [
        {
            "name": "r0",
            "type": "RouterTLM",
            "param_rules": {
                "mesh_x": {"type": "int", "required": true, "min_val": "1", "max_val": "16"},
                "mesh_y": {"type": "int", "required": true, "min_val": "1", "max_val": "16"},
                "vc_count": {"type": "int", "default_val": "4", "min_val": "1", "max_val": "8",
                             "derive_expr": "(mesh_x >= 4) ? 8 : 4"},
                "flit_width": {"type": "int", "default_val": "64", "min_val": "32", "max_val": "256",
                               "derive_expr": "(mesh_x * mesh_y > 16) ? 128 : 64"}
            },
            "params": {"mesh_x": 2, "mesh_y": 2}
        }
    ]
}
```

方案 B：全局参数规则文件（推荐，规则与实例分离）
```json
// configs/param_rules/router_tlm.json
{
    "module_type": "RouterTLM",
    "rules": {
        "mesh_x": {"type": "int", "required": true, "min_val": "1", "max_val": "16"},
        "mesh_y": {"type": "int", "required": true, "min_val": "1", "max_val": "16"},
        "vc_count": {"type": "int", "default_val": "4", "min_val": "1", "max_val": "8",
                     "derive_expr": "(mesh_x >= 4) ? 8 : 4"},
        "flit_width": {"type": "int", "default_val": "64", "min_val": "32", "max_val": "256",
                       "derive_expr": "(mesh_x * mesh_y > 16) ? 128 : 64"}
    }
}
```

**ModuleFactory 自动反序列化和应用**:
```cpp
// src/core/module_factory.cc

// 1. 加载全局参数规则
ModuleParamRules rules = json::parse(rule_file).get<ModuleParamRules>();

// 2. 实例化时自动应用规则
for (const auto& [param_name, param_value] : module_json["params"].items()) {
    if (rules.rules.count(param_name)) {
        const ParamRule& rule = rules.rules.at(param_name);
        // 自动类型转换、范围验证、推导评估
        applyParam(param_name, param_value, rule, resolved_params);
    }
}
```

**理由**:
- 用户通过 JSON 完全控制参数规则（类型、范围、默认值、推导）
- 规则与模块代码解耦，新增参数规则无需修改 C++ 代码
- `WITH_DEFAULT` 宏自动处理可选字段
- 与 Phase 3.2 端口类型系统保持一致的设计模式

---

## 决策 4.5: ParamRule 验证与 ModuleFactory 协作（G5 共识）

### 问题

Python 配置生成器的 Pydantic 验证与 C++ ModuleFactory.validate_params() 的关系是什么？

### 决策

✅ **双重验证，职责分层**：

| 验证层 | 时机 | 位置 | 验证内容 |
|--------|------|------|---------|
| **生成时验证** | Python 配置生成时 | Pydantic Models | 结构错误（类型错误、必填字段缺失、枚举值非法） |
| **运行时验证** | C++ 模块实例化前 | ModuleFactory.validate_params() | 语义错误（参数值超出模块支持范围、参数组合不合法） |

**协作流程**：
```
Python ConfigBuilder.build()
    ↓ Pydantic 验证（生成时验证）
    ↓ 捕获结构错误
    ↓ 生成 JSON 配置
    ↓
C++ ModuleFactory::instantiateAll()
    ↓ ModuleFactory.validate_params()（运行时验证）
    ↓ 捕获语义错误
    ↓ set_config() 应用到模块
```

**ParamRule 统一验证逻辑**（G5 共识）：

ModuleFactory 应加载 ADR-X.10 定义的 ParamRule 进行验证，避免重复实现验证逻辑：

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
            if (!rule.min_val.empty() && value.get<int64_t>() < std::stoll(rule.min_val)) {
                printf("[PARAM ERROR] Module '%s' param '%s' below min %s\n",
                       module_type.c_str(), param_name.c_str(), rule.min_val.c_str());
                return false;
            }
            if (!rule.max_val.empty() && value.get<int64_t>() > std::stoll(rule.max_val)) {
                printf("[PARAM ERROR] Module '%s' param '%s' above max %s\n",
                       module_type.c_str(), param_name.c_str(), rule.max_val.c_str());
                return false;
            }
        }
    }
    return true;
}
```

**与 Pydantic 验证的一致性**：
- Python 侧使用 `ParamRule` Pydantic Model 验证
- C++ 侧使用 `ParamRule` struct 验证
- 两者共享同一数据源（JSON 配置或全局规则文件）
- 确保验证规则一致

---

## 决策 4.6: set_config() 异常处理（G5 共识）

### 问题

set_config() 在 ParamRule 验证失败时应抛出异常还是返回错误码？

### 决策

✅ **抛出异常**（`std::invalid_argument` 或自定义 `ParamValidationError`）。

**理由**：
- set_config() 是模块实例化前的关键步骤，失败应立即终止流程
- 异常携带详细错误信息（哪个参数、为什么失败），便于调试
- CppTLM 现有的异常处理机制支持模块实例化失败的回滚
- 与 Pydantic 验证失败抛出 `ValidationError` 的行为一致

**异常定义**：
```cpp
// include/core/param_errors.hh
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
```

**set_config() 实现**：
```cpp
// src/core/module_factory.cc
void ModuleFactory::set_config(BaseModule* module, const json& params,
                               const ModuleParamRules& rules) {
    // 验证参数
    if (!validate_params(module->get_type(), params, rules)) {
        throw ParamValidationError(
            module->get_name(),
            "multiple",
            "Parameter validation failed (see error log above)"
        );
    }
    
    // 应用参数
    module->set_config(params);
}
```

**异常处理**：
```cpp
// ModuleFactory::instantiateAll() 中的异常捕获
try {
    set_config(module, params, rules);
} catch (const ParamValidationError& e) {
    printf("[PARAM ERROR] Failed to configure module '%s': %s\n",
           e.module_name.c_str(), e.what());
    // 清理已创建的模块，回滚
    cleanup();
    return false;
}
```

---

## 决策 5: 参数推导表达式格式

### 问题

推导表达式（derive_expr）使用什么语法格式？

### 选项对比

| 选项 | 格式 | 示例 | 优点 | 缺点 |
|------|------|------|------|------|
| **A) 字符串三元表达式** ✅ | `"(cond) ? v1 : v2"` | `"(mesh_x >= 4) ? 8 : 4"` | 简单，覆盖 90% 场景 | 不支持复杂逻辑 |
| B) Python 表达式 | `"8 if mesh_x >= 4 else 4"` | 同上 | Python 用户熟悉 | 需要 Python 运行时或解析器 |
| C) JSON 规则对象 | `{"if": "...", "then": 8, "else": 4}` | 同上 | 结构化，易验证 | 冗长，嵌套复杂 |

### 决策

✅ **选项 A) 字符串三元表达式**

**理由**:
- 简单直接：C++ 开发者熟悉三元表达式语法
- 实现成本低：简单的 token 解析器即可处理
- 覆盖常见场景：条件推导基本都是 "if-then-else" 模式
- 复杂推导场景：可以用显式参数值覆盖推导结果

**表达式语法子集**:
```
derive_expr ::= ternary | literal
ternary     ::= "(" condition ")" "?" value ":" value
condition   ::= identifier comparator literal
comparator  ::= ">=" | "<=" | "==" | "!=" | ">" | "<"
value       ::= literal | identifier
literal     ::= number | string
```

**不支持的操作**（故意限制，保持简单）:
- 算术运算（`mesh_x * mesh_y`）— 用显式声明或外部工具计算
- 逻辑运算（`&&`, `||`）— 用嵌套三元表达式替代
- 函数调用 — 不在推导表达式中支持

---

## 决策 6: 参数类型转换

### 问题

如何处理人类友好的参数格式（如 `"3ns"`, `"256MB"`, `"0x10000000"`）？

### 决策

采用类型感知的 ParamParser，在参数赋值前进行格式转换：

| 声明类型 | 输入格式 | 转换结果 | 示例 |
|----------|---------|---------|------|
| `int` | 纯数字 | 直接解析 | `"42"` → 42 |
| `unsigned` | 纯数字 | 无符号解析 | `"256"` → 256u |
| `latency` | `<number><unit>` | 转换为 cycle | `"3ns"` → 取决于时钟频率 |
| `address` | `0x<hex>` 或 `<size>` | 转换为 uint64 | `"0x10000000"` → 0x10000000 |
| `address` | `"256MB"` | 字节数 | `"256MB"` → 0x10000000 |
| `string` | 任意 | 直接使用 | `"LRU"` → "LRU" |
| `bool` | `true/false` | 布尔值 | `"true"` → true |

**Latency 转换规则**:
- 需要模块声明时钟频率（`clock_frequency` 参数，单位 MHz）
- `"3ns"` → `(3.0 / (1000.0 / clock_frequency))` cycles → 向上取整
- `"100ps"` → 同上，ps 单位
- `"2cycles"` → 直接使用 2

**Address 转换规则**:
- `"0x..."` → `std::stoull(str, nullptr, 16)`
- `"256MB"` → `256 * 1024 * 1024`
- `"1GB"` → `1024 * 1024 * 1024`
- 支持单位：B, KB, MB, GB, TB（二进制，1KB=1024B）

**ParamParser 结构体**:
```cpp
struct ParamParseResult {
    bool success;
    std::string error_message;
    std::variant<int64_t, uint64_t, double, std::string, bool> value;
};

struct ParamParser {
    static ParamParseResult parse(const std::string& input, ParamType type,
                                  double clock_frequency_mhz = 1000.0);
    static bool validate_range(const ParamParseResult& result, const ParamRule& rule);
    static int64_t evaluate_derive_expr(const std::string& expr,
                                        const std::map<std::string, int64_t>& params);
};
```

**实施位置**: `include/core/param_parser.hh`

---

## 决策 7: 参数验证时机

### 问题

参数验证在什么时候执行？

### 决策

采用**两阶段验证**：

| 阶段 | 时机 | 检查内容 | 错误级别 |
|------|------|---------|---------|
| **Stage 1: Schema 验证** | instantiateAll() 初期 | 必需参数存在、类型正确 | ERROR |
| **Stage 2: 语义验证** | 默认值赋值后 | 范围检查、推导评估、依赖关系 | ERROR/WARNING |

**Stage 1 流程**:
```
1. 检查 required=true 的参数是否存在
2. 检查参数值类型是否与声明匹配
3. 缺失的 optional 参数使用 default_val 填充
```

**Stage 2 流程**:
```
1. 评估 derive_expr（如果参数有 derive_expr 且未显式赋值）
2. 检查 min_val/max_val 范围
3. 检查参数依赖关系（如 mesh_x * mesh_y == num_routers）
```

---

## 风险与权衡

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| JSON 结构体字段变更导致旧配置不兼容 | 高 | 使用 `WITH_DEFAULT` 宏，新增字段必须提供默认值 |
| 三元表达式解析器复杂度超出预期 | 中 | 严格限制语法子集，不支持嵌套 |
| Latency 转换依赖时钟频率 | 低 | 时钟频率为必需参数或全局默认值 |
| 推导表达式引用不存在的参数 | 高 | Stage 1 先验证所有引用参数存在 |
| 默认值与 derive_expr 冲突 | 中 | derive_expr 优先于 default_val，显式值最高 |

---

## 优先级排序

参数推导优先级：
1. **RouterTLM**: flit_width, vc_count（基于 mesh 规模）
2. **NICTLM**: buffer_depth（基于网络规模）
3. **其他模块**: 按需添加

参数验证优先级：
1. mesh_x/y 范围（1-16）
2. vc_count 范围（1-8）
3. buffer_depth 范围（1-64）

---

## 迁移计划

1. **Step 1**: 创建 `param_rules.hh` 定义 ParamRule、ParamType 和 JSON 宏
2. **Step 2**: 创建 `param_parser.hh` 实现类型转换和 derive_expr 评估
3. **Step 3**: 创建全局参数规则文件 `configs/param_rules/*.json`
4. **Step 4**: ModuleFactory 添加 Stage 1 Schema 验证
5. **Step 5**: 添加默认值赋值和 derive_expr 评估
6. **Step 6**: 添加 Stage 2 范围验证
7. **Step 7**: RouterTLM 配置示例和测试

**向后兼容**: 现有配置不需要修改，参数框架是纯增量功能。模块构造函数仍可直接使用 JSON 参数。

---

## 开放问题

1. 是否支持参数依赖图？（如 A 依赖 B，B 变化时自动重新推导 A）
2. 是否需要参数继承？（子模块继承父模块的参数规则）
3. derive_expr 是否支持引用其他模块的参数？（如 `${settings.clock_frequency}`）

---

## 变更记录

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-05-05 | 初始提案，设计参数框架 |
| 2.0 | 2026-05-05 | 更新参数推导和验证决策 |
| 3.0 | 2026-05-05 | 补充术语统一（m1 共识）<br>补充 ParamRule 验证与 ModuleFactory 协作（G5 共识）<br>补充 set_config() 异常处理（G5 共识） |

## 共识追踪

| 议题 | 状态 | 说明 |
|------|------|------|
| G5 | ✅ 已整合 | 双重验证（Pydantic + ModuleFactory），ParamRule 统一验证逻辑，set_config() 抛异常 |
| m1 | ✅ 已整合 | 术语统一（port type/port role/bundle type） |

---

**下一步**: Phase 3.3 实施时遵循此 ADR
