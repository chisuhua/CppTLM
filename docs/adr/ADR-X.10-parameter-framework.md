# ADR-X.10: Phase 3+ 参数框架

> **版本**: 3.0
> **日期**: 2026-05-05
> **状态**: 🟡 **部分实施**（2026-11-17 起：决策 4.6 通路已落地，4.5/5/6/7 未落地；Phase 3.3 计划整体未实施 —— 见 §Status Update）
> **关联**: TGMS Phase 3.3, proposal.md Decision 3, ARCH-012 Gap Analysis, ADR-X.9 (端口类型系统)
> **变更**: v2.0 → v3.0: 补充术语统一、ParamRule 验证协作、set_config() 异常处理
> **审稿报告**: `.rddf/reviews/ADR-X.10.md`（oracle 1 轮 APPROVE + critic 1 轮 APPROVE_WITH_CONDITIONS · 0 BLOCKER · SHOULD_FIX S1~S7 与 NITS N1~N3 已闭合 · N4 记理由不适用 · scope_exceeded 1 项记 backlog）

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

## Status Update

### 2026-11-17 — 实施状态登记 + 事实订正（architect）

**背景**：change `cpptlm-router-buffer-size-param`（`RouterTLM` 的 `buffer_size` param 真正生效）于 2026-11-17 实施、验收、归档（台账 Entry #467 / #469 / #470 / #471）。归档后复核（残留任务 T0.4 / T6.1）发现本 ADR 的**实施状态**与若干**事实**需登记。本段**只记事实与实施状态，不改本 ADR 正文任何决策**（ADR 不可变原则）。

**本段收口的两项裁决**（残留任务 T0.4 / T6.1）：

- **T0.4** ⇒ change `cpptlm-router-buffer-size-param` 的 `design.md` **D-1 默认值裁决（选项 A：声明默认值 16 → 8）仍然成立**（论证见 D 段）;D-1 与 D-7.2 的两处**表述缺陷**以 **errata** 形式登记在本段（归档件不可改）。
- **T6.1** ⇒ **不新造**「参数声明-实际一致性」纪律 ADR —— 该纪律已是既有决策的推论（决策 1 的缺点栏已把「C++ struct 与 JSON schema 同步」写成**已接受**的义务；决策 4.5 要求运行时 type/min/max 校验）⇒ **无新不可逆选择**;新造 ADR 将破「一文档一决策」，且其 Verification 章会退化为测试计划。纪律的权威载体分工见 G 段。

**A. 决策实施状态（逐条 · 全部实测）**

| 决策 | 状态 | 证据 |
|---|---|---|
| 决策 1（nlohmann/json 结构体序列化;规则**存放位置**见 B 段） | ⚠️ **载体已实施 · 存放位置部分偏离** | 载体 ✅:`ParamRule` 为 nlohmann/json 可序列化结构(`include/core/param_rules.hh`);⚠️ **字段级 schema 偏离**:本 ADR `:83-93` 的 `default_val`/`min_val`/`max_val`/`derive_expr`/`description`（字符串型）与实现 `include/core/param_rules.hh:31-42` 的 `default_int`/`default_float`/`default_str`/`default_bool`/`min_value`/`max_value`/`enum_values`（typed optional,**无 `derive_expr`/`description`**）不一致;`configs/param_rules/router_tlm.json` 用的是**实现侧**字段名 |
| 决策 4.5（`ModuleFactory.validate_params()` 运行时做 type + min/max 校验） | ❌ **未实施** | `src/core/module_factory_validate.cc:474-499` 的 `validate_module_params`（`static`）**零调用方 ⇒ 死代码**；`validateConfig`（`:199`）不调它；`ParamParser::validate`（`src/core/param_parser.cc:54-67`，实现 min/max）的唯一调用点就在该死函数内 |
| 决策 4.6（`set_config()` 失败抛异常 + `instantiateAll` catch ⇒ return false） | ⚠️ **部分实施**（2026-11-17）—— **通路**已就位，但本决策的**三个成分仅落地其一** | **通路** ✅:模块 `on_config_loaded` 自校验抛异常 + `src/core/module_factory.cc:416-423` catch ⇒ `return false`（与 Step 4.5 `simulate_instantiate` 同款既有范式）。⛔ **① 校验位置**（决策要求 `set_config` → `validate_params`）未实施 —— 全仓**无 `validate_params` 函数**（仅 `static validate_module_params`，零调用方）;⛔ **② 异常类型**（`std::invalid_argument` / `ParamValidationError`）未实施 —— 实际抛 `std::runtime_error`（`src/tlm/router_tlm.cc:90`/`:95`），`include/core/param_errors.hh:11` 的 `ParamValidationError` **无任何 throw 点**;⛔ **③ 回滚**（`cleanup(); return false;`）未实施 —— `src/` 全仓无 `cleanup()`，仅 `return false` |
| 决策 5（`derive_expr` 三元表达式推导） | ❌ **未实施** | `include/core/param_rules.hh:31-42` 的 `ParamRule` **无 `derive_expr` 字段**；`configs/param_rules/router_tlm.json:31` 的 `derive_expr` 在 C++ / Python 两侧皆无人执行 ⇒ 静默丢弃 |
| 决策 6（类型感知转换：latency / address / 人类友好格式） | ❌ **未实施** | `ParamType` 实际为 `INTEGER/FLOAT/STRING/BOOLEAN/ENUM`，与本 ADR 的 `INT/UNSIGNED/STRING/ADDRESS/LATENCY/BOOL` 不一致；无 latency / address 转换 |
| 决策 7 Stage 1（required 存在 + default 填充） | ⚠️ **部分实施** | `src/core/module_factory.cc:263-291` 只做 `required` 缺失 ⇒ false + `default_int` 注入（**注入回 `mod["params"]`**）。**`type` 声明未被规则层消费** —— 但**类型保护并非全无**：模块 `on_config_loaded` 读取时，`nlohmann::json::get<T>()` 的 `json::type_error`（继承链 `json::exception` ← `std::exception`，见 `external/json/nlohmann/json.hpp:4469`）会被 `module_factory.cc:416-423` 的 catch 捕获 ⇒ 装配失败。⇒ 精确表述：**类型保护存在，但不在规则层，且仅覆盖「模块确实读取了的参数」**;未被模块读取的参数（即便声明了 `type`）零检查 |
| 决策 7 Stage 2（derive 评估 + min/max + 参数依赖关系） | ❌ **未实施** | 同决策 4.5 |

**B. 「决策 1 选了什么」存在文本歧义（标签冲突）**

本 ADR 决策 1 的**选项表**（`:55-59`）与同章**「用户使用方式」**（`:104-141`）共用 A/B 标签却指不同事物：选项表 B = 「静态 `get_param_rules()` 方法」、C = 「外部规则文件」；使用方式方案 A = 「在模块配置中直接声明规则」、方案 B = 「全局参数规则文件」（= 选项表的 C）。⇒ 决策落点「✅ 选项 A」说的是**序列化载体**（nlohmann/json），规则**存放位置**则由「使用方式」段给出（全局文件，推荐）⇒ 单读文本无法判定「决策 1 选了谁」。**后续引用本决策须同时指名「载体」与「存放位置」两个维度。**

**C. 现役的两套声明面与各自消费者（实测）**

| 面 | 载体 | 消费者 | 活的面 |
|---|---|---|---|
| ① C++ 运行时声明面 | `RouterTLM::get_param_rules()`（`src/tlm/router_tlm.cc:580-606`；经 `registerParamRules` 自注册 —— **生产代码中唯一注册方**;测试侧另有一处幂等调用 `test/test_d3_param_resolver_registry.cc:53`） | `src/core/module_factory.cc:263-291` | **活**（消费 `required` + `default_int`）；`type` / `min` / `max` 面**死**（见 A 表决策 4.5） |
| ② JSON 生成期声明面 | `configs/param_rules/{router_tlm,nic_tlm}.json` | `cpptlm_config/validator.py:16-26` 的 `load_param_rules()` ⇒ PARAM-01（`:263`）/ PARAM-02（`:281`）；标准测试路径 `scripts/test/test_off.sh:84` 跑 `pytest cpptlm_config/tests/` | **活**（生成期校验） |
| ③ 实际约束面 | 各模块 `on_config_loaded` 逐模块自校验 | 散落、逐模块各自为政 | 仅 `buffer_size` 一条闭合 |

⇒ **实测漂移四处（参数名级）**：`flit_width` 只在 ① 有（`router_tlm.cc:594-596`）、② 无 · `vc_count` 声明 2 / 实际 `NUM_VCS = 4` · `credit_timeout` 生效（`router_tlm.cc:80-82`）但 ①② 皆无 · `derive_expr` 只在 ② 有且两侧皆死。
⇒ **另有字段级 schema 偏离**（见 A 表决策 1 行）:本 ADR 正文的 `ParamRule` 字段集与实现结构体不一致 ⇒ 即便**参数名**对齐，**字段**仍不可互译。
⇒ **决策 1 的缺点栏原文已写「需要 C++ struct 与 JSON schema 同步」** ⇒ 两面相等是本 ADR **已接受**的义务，**不是新选择**。⚠️ 但「以 `getParamRulesRegistry()` 为 C++ 运行时唯一 rules 源」相对决策 4.5 正文「两者共享同一数据源（JSON 配置或全局规则文件）」构成一处**已登记偏离**（证据 = 本段漂移清单）—— C++ registry 是**第三个**物化面;**该偏离的归正方式待 F 段独立裁决**（不属本段范围）。

**D. 事实订正（归档 change 的表述缺陷 · errata）**

change `cpptlm-router-buffer-size-param` 的 `design.md` **D-1 理由②** 与 **D-7.2 的理由** 各有一处表述缺陷。归档件不可改（且不在 architect 可写范围），此处登记 errata：

1. **D-1 理由②「16 无仓内依据」** —— 对**实现 / 拓扑**成立，对**文档**不成立：16 的仓内出处是 `docs/archive/phase3.1-defect-fixes-plan.md:221`（`| buffer_size | INTEGER | ❌ | 16 | 1-64 |`）、`:366`、`:435`（`CHECK(factory.get_module_param("r0","buffer_size") == 16)`）与 `docs/archive/phase3.3-config-enhancement-plan.md:361`；该 plan `:205` 自述「ADR-X.10 v3.0 决策 1（**过渡实现**），Phase 3.3 将升级为 nlohmann/json 序列化版本」，而 Phase 3.3 的**参数框架升级（决策 1/4.5/5/6/7）从未整体实施**（其 plan 亦已归档）⇒ 16 是**过渡态被永久化**的占位值。**该 plan 的 `CHECK == 16` 不构成活契约** —— `get_module_param` 全仓零命中。
   ⇒ **裁决结论不受影响**：本 ADR 全文不含 `buffer_size`，只给 `buffer_depth` 的**范围** 1-64 ⇒ **从未规定默认值** ⇒ 声明默认值 16 → 8（对齐实现 `BUFFER_DEPTH = 8`）不与任何已签发决策冲突。
   ⇒ D-1 理由②的括号称 `grep -rn "\"buffer_size\"" configs/` 命中 `configs/param_rules/router_tlm.json` 的声明本身 —— **在裁决时点为假**（该文件当时**无 `buffer_size` 条目** —— 实测当时有 5 条规则 `node_x`/`node_y`/`mesh_x`/`mesh_y`/`vc_count`，见 `git show 708b13c8^:configs/param_rules/router_tlm.json`）；正确表述是「`configs/` 下**零命中**」。
2. **D-7.2 的理由**（「否则 `validateConfig` 路径（若将来被修复）也无从校验它」）**不成立** —— 未来的修复按决策 4.5 从 **C++ registry** 取 rules，**永不读 json** ⇒ json 条目对 C++ 侧永远无效。**但处置本身站得住**：`configs/param_rules/router_tlm.json` 补 `buffer_size`（1..64）后，**Python 生成期 PARAM-02 开始校验该参数范围**（改动前该键不在 json ⇒ 不校验）⇒ 这是**新增一条真实约束**，与 C++ 运行时 1..64 校验对齐。

**E. 仓外行为面（对外口径）**

仓内**零行为变更**（无任何 config 使用 `buffer_size`）。仓外风险面是**显式写过 `buffer_size` 的用户配置**：行为从「参数被忽略（深度恒 8）」变为「参数生效（深度 = 显式值，credit 预算同源）」；越界值（0 / >64）从「静默忽略」变为「装配失败」。**默认值 16 → 8 本身无行为面**：registry 注入的默认值 8 == `BUFFER_DEPTH`，缺省路径两侧均为深度 8。

**F. 未裁决子议题（登记 · 本段不裁决）**

「`configs/param_rules/*.json` 的存废定位（保留为生成期规则面 / 降级为派生副本 / 废弃）」—— 有不可逆后果（生成期校验能力、对外契约面、迁移成本），且 B 段的标签冲突使文本无法判定其既有定位 ⇒ 需**独立的 ADR 级裁决**。⚠️ **「删除 json 文件」不是可选项**：json 是 Python 生成期 PARAM-01/02 的**活**数据源（`cpptlm_config/validator.py:16-26`），删除会静默关闭生成期校验并打挂标准 pytest（`scripts/test/test_off.sh:84`）。
⚠️ **存续寿命另受一处约束**：其唯一消费方 `cpptlm_config` 已被标记 **Legacy / Deprecated**（`cpptlm_config/AGENTS.md:4`，2026-06-17，已被 `cpptlm.topo` + `CxxCompatibleEmitter` + `cpptlm.library` 替代），而继任者 `cpptlm/` 全仓**零** `param_rules` 引用 ⇒ 本项裁决须一并回答「生成期校验能力迁往何处」，否则该面可能被整体弃用而非迁移。

**G. 后续载体（纪律的权威落点）**

- **可测不变量**（「声明集合 ↔ 运行时读取集合」双向相等 · 每个 `default_int` 有「未配置 ⇒ 运行时值 == 声明值」的锁定测试 · 非法值装配期 fail 泛化到全模块）⇒ 归 `openspec/specs/param-derivation/` + 闸，**不在本 ADR**（ADR 回答「为什么」，spec 回答「是什么」，闸回答「不准偏」）。
- ⚠️ `openspec/specs/param-derivation/spec.md` 现状含**未实现条款**（「ParamRule stores all fields … accessible via getter methods」按字面为假 —— 结构体是公有字段、无 getter、无 `derive_expr`；「Dynamic derivation evaluation」零实现）与 **TBD 占位 Purpose** ⇒ 扩写前须先对齐，否则新闸会以未实现条款为基线。
- **实施载体**：`.rddf/improvements/cpptlm-validate-params-dead-code.md`（接回死代码）· `.rddf/improvements/cpptlm-router-vc-count-param-inert.md`（`vc_count` 声明-实际不一致）。
- **待订正的实现注释（architect 域外 · 交 builder）**：`src/tlm/router_tlm.cc:86-89` 的注释称「`instantiateAll` 会先跑 `validateConfig` → `validate_params`（已按 ParamRules 的 min/max 做 parse + 范围校验）⇒ 本分支的抛异常在标准装配路径上**不可达**」—— 与 A 表决策 4.5 相反：`validateConfig` **不调**校验器，且**没有** `validate_params` 这个函数（只有 `static validate_module_params`）⇒ 该分支正是 AC-04 的**唯一**失败通道，⛔ 不可删。

### 2026-11-17 — 指向 `ADR-X.17`（模块参数「读取即声明」机制）

本 ADR 的**部分取代**与**部分闭合**关系由 `docs/adr/ADR-X.17-module-param-read-declares.md` 承载：

- **部分取代**：决策 1 的「**规则存放位置**」维度（「序列化载体」维度不变）。
- **部分闭合**（逐条见 ADR-X.17 的 Metadata 表）：决策 7 Stage 1 与 Stage 2 的 min/max 子项（**仅对已迁移模块**）· 决策 4.6 的 ①**校验位置**（全体类型，位置已就位）与 ②**校验依据**（仅已迁移模块）。
- **未闭合**（须各自具名 owner change）：决策 7 Stage 2 的 `derive_expr` 评估与参数依赖关系 · 决策 5 · 决策 6 · 决策 4.6 的 ②异常类型与 ③回滚 · 决策 4.5 的「参数组合不合法」（跨参数约束）。
- ⚠️ 本 ADR 正文与上述 §A~§G **既有内容零改动**（本子段为 append-only）。
