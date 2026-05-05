# ADR-X.11: 配置继承机制与 Phase 3.1 缺陷修复策略

> **版本**: 3.0
> **日期**: 2026-05-05
> **状态**: ✅ 已实施（Phase 3.1），待改进项已标记
> **关联**: TGMS Phase 3.1, design.md Decision 2 & 4, commits 4922dd6~8ebb2ef
> **变更**: 
> - v1.0 → v2.0: 补充 CFG-08 验证器决策、DEF-02 latency 冲突处理、DEF-03/DEF-04 改进时间线
> - v2.0 → v3.0: 补充术语表、层级拓扑决策（G6）、DEF-04 扩展（M4）、阶段编号统一（m3）

---

## 术语

- **Phase 3+**：Phase 3.1-3.4 的统称
- **Phase 3.1**：缺陷修复阶段（DEF-02、DEF-03、DEF-04）
- **Phase 3.2**：端口管理阶段
- **Phase 3.3**：配置增强阶段
- **Phase 3.4**：拓扑验证阶段

**DEF 与阶段对应关系**（m3 共识）：
- DEF-02（重复连接） → Phase 3.1
- DEF-03（BidirectionalPortAdapter） → Phase 3.1（已完成），Phase 3.2（警告日志）
- DEF-04（端口索引严格化） → Phase 3.1（已完成），Phase 3.2（WARNING 日志）

---

## 背景

Phase 3.1 完成了 5 个 ModuleFactory 缺陷修复（DEF-01~05）和配置继承机制（extends）。
这些修复和设计决策影响了核心的模块实例化流水线，需要记录决策理由和权衡，
以便后续开发者和 Phase 3.2+ 实施时参考。

v2.0 更新：基于最新代码审查，补充了 CFG-08 JSON Schema 验证器决策（决策 7）、
DEF-02 latency 冲突处理（决策 6 扩展），并标记了 DEF-03/DEF-04 的改进时间线。

---

## 决策 1: 配置继承合并语义

### 问题

当子配置通过 `extends` 引用父配置时，各字段如何合并？

### 选项对比

| 字段 | 合并策略 | 理由 |
|------|---------|------|
| **modules** | **深合并（按 name 键）** | 子配置可覆盖父配置中同名模块的参数，新增模块被追加 |
| **connections** | **追加（append）** | 子配置添加新连接，不删除父配置的连接 |
| **groups** | **深合并（按 group_name 键）** | 子配置可覆盖同名组的成员，新增组被追加 |
| **其他顶级字段** | **子配置覆盖** | settings、routing 等由子配置完全指定 |

### 决策

✅ **modules 按名深合并，connections 追加，groups 按名合并**

**理由**:
- 符合用户直觉：基础配置提供公共元素，覆盖配置只修改差异部分
- connections 追加而非合并：拓扑连接是累加的，子配置通常"添加"连接而非"替换"
- 递归 extends 支持：A extends B extends C，合并从根到叶子逐层应用

**合并示例**:
```json
// base.json
{
    "modules": [
        {"name": "cpu0", "type": "CPUTLM", "params": {"freq": 1000}}
    ],
    "connections": [
        {"src": "cpu0", "dst": "l1"}
    ]
}

// override.json (extends "base.json")
{
    "modules": [
        {"name": "cpu0", "params": {"freq": 2000}},  // 覆盖 freq
        {"name": "mem", "type": "MemoryTLM"}          // 新增模块
    ],
    "connections": [
        {"src": "l1", "dst": "mem"}                   // 追加连接
    ]
}

// 最终结果
{
    "modules": [
        {"name": "cpu0", "type": "CPUTLM", "params": {"freq": 2000}},
        {"name": "mem", "type": "MemoryTLM"}
    ],
    "connections": [
        {"src": "cpu0", "dst": "l1"},
        {"src": "l1", "dst": "mem"}
    ]
}
```

**实施位置**: `src/core/module_factory.cc` 的 `mergeConfigs()` (第 44-113 行) 和 `processExtends()` (第 115-160 行)

**代码验证** (v2.0 更新):
- modules 合并：第 56-76 行，使用 `std::map<std::string, json>` 按 name 去重合并
- connections 追加：第 79-86 行，直接 `push_back` 子配置连接
- groups 合并：第 88-100 行，按 group_name 键合并成员数组

---

## 决策 2: 循环引用保护策略

### 问题

如何防止 extends 循环引用（A extends B, B extends A）导致无限递归？

### 选项对比

| 选项 | 策略 | 优点 | 缺点 |
|------|------|------|------|
| **A) 深度限制（depth > 10）** ✅ | 递归深度超过 10 时返回空对象 | 实现简单，同时限制合理配置深度 | 不检测真正的循环，只是限制 |
| B) 访问图检测 | 维护 visited 集合，检测重复访问 | 精确检测循环 | 实现复杂，需要跟踪文件路径 |
| C) 拓扑排序 | 预处理所有配置的依赖图 | 最精确，可给出错误位置 | 需要预加载所有配置文件 |

### 决策

✅ **选项 A) 深度限制（depth > 10）**

**理由**:
- 实现简单：在 `processExtends()` 入口加一行检查
- 实际场景够用：10 层嵌套远超实际需求（通常 2-3 层）
- 同时解决两个问题：循环引用（无限递归）和过深嵌套（栈溢出）

**限制**: 循环引用不会产生友好的错误消息（只说 "depth limit exceeded" 而非 "circular reference detected"）。
这是可接受的权衡，因为深度限制的主要目的是防止崩溃而非诊断。

**代码** (第 115-119 行):
```cpp
static json processExtends(const json& config, int depth = 0) {
    if (depth > 10) {
        printf("[CONFIG ERROR] extends depth limit exceeded (possible circular reference)\n");
        return json::object();
    }
    // ...
}
```

**实施位置**: `src/core/module_factory.cc:115-119`

---

## 决策 3: ModuleGroup 通配符延迟绑定

### 问题

ModuleGroup 的通配符模式（如 `"nic_*"`）应该在什么时候解析为具体实例？

### 选项对比

| 选项 | 时机 | 优点 | 缺点 |
|------|------|------|------|
| **A) resolve() 调用时（延迟绑定）** ✅ | 每次调用 resolve() 时扫描当前已注册实例 | 支持任意注册顺序，始终反映最新状态 | 重复调用会重复扫描 |
| B) define() 时（早期绑定） | 定义 group 时立即解析 | 只扫描一次，性能好 | 要求实例先注册，顺序敏感 |
| C) define() 时 + 注册时更新 | 定义时解析，新实例注册时更新 | 兼顾性能和灵活性 | 需要回调机制，复杂度高 |

### 决策

✅ **选项 A) resolve() 调用时延迟绑定**

**理由**:
- 注册顺序无关：实例可以在 group 定义之前或之后注册
- 始终反映最新状态：动态注册的实例也能被通配符匹配
- 性能可接受：实例化阶段只调用一次 resolve()，不是热点路径

**缓存优化**（未来）：如果性能成为问题，可在 resolve() 后缓存结果，
新实例注册时清除缓存。

**实施位置**: `include/utils/module_group.hh:80-105`

---

## 决策 4: DEF-03 BidirectionalPortAdapter 修复范围

### 问题

BidirectionalPortAdapter 的绑定修复应该限定在 RouterTLM 还是泛化到所有类型？

### 决策

⚠️ **当前实现：仅限定 RouterTLM** (v2.0 状态确认)

**代码** (第 628-637 行):
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

**理由**:
- RouterTLM 是目前唯一使用 BidirectionalPortAdapter 的模块
- 其他多端口模块（CrossbarTLM, ArbiterTLM）使用不同的适配器模式
- 过早泛化可能引入不必要的复杂性

**风险**: 如果未来有新的 BidirectionalPortAdapter 模块类型，
需要记得更新此处的 type 检查或实现泛化方案。

### 改进时间线 (v2.0 新增)

| 阶段 | 改进内容 | 触发条件 |
|------|---------|---------|
| **Phase 3.2** | 添加类型检查警告日志 | 当 `is_multi && type != "RouterTLM"` 时打印 WARNING |
| **Phase 3.3** | 评估泛化方案 | 如果出现第二个 BidirectionalPortAdapter 模块类型 |
| **Phase 4** | 实现虚函数方案 A | 如果有 3+ 种 BidirectionalPortAdapter 类型 |

**泛化方案（未来）**:
- 方案 A：在 adapter 基类添加虚函数 `requires_bind_port_pair()` 和 `bind_pair()`
- 方案 B：使用 RTTI `dynamic_cast<BidirectionalPortAdapter<...>*>` 检测类型

**实施位置**: `src/core/module_factory.cc:628-637`

---

## 决策 5: DEF-04 端口索引严格化策略

### 问题

当端口索引格式非法时（如 `"xbar.0abc"`），应该如何处理？

### 选项对比

| 选项 | 处理方式 | 优点 | 缺点 |
|------|---------|------|------|
| **A) all_digits 检查 + 静默默认** ⚠️ | 非纯数字时跳过解析，使用索引 0 | 不会崩溃，向后兼容 | 用户不知道配置有误 |
| B) all_digits 检查 + 错误日志 | 非纯数字时打印 WARNING，使用索引 0 | 用户得到反馈 | 仍需决定是 WARNING 还是 ERROR |
| C) 严格拒绝 | 非纯数字时返回 false，跳过连接 | 最严格，强制用户修复 | 可能破坏现有配置 |

### 决策

⚠️ **当前实现：选项 A) all_digits 检查 + 静默默认** (技术债务)

**代码** (第 666-674 行):
```cpp
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

**问题**: 非法索引 `"0abc"` 会静默默认为 `0`，用户不会得到任何反馈。
这是一个**已知的技术债务**。

### 改进计划 (v2.0 更新)

**目标阶段**: Phase 3.2 开始前（高优先级）

**改进代码**:
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

**理由选择 WARNING 而非 ERROR**:
- 向后兼容：不破坏现有配置
- 用户反馈：让用户知道配置有误
- 渐进式改进：可以在后续版本升级为 ERROR

**实施位置**: `src/core/module_factory.cc:666-674`

---

## 决策 6: DEF-02 两阶段去重 + latency 冲突处理

### 问题

重复连接去重应该在流水线的哪个阶段执行？如何处理重复连接的不同 latency 值？

### 决策

✅ **两阶段去重：Step 5（ConnectionResolver）+ Step 6（PortPair 创建前）**

**理由**:
- Step 5 去重：防止 ConnectionResolver 为同一连接创建多个端口
- Step 6 去重：防止通配符展开产生重复的 (src, dst) 对
- 两阶段缺一不可：只在 Step 6 去重，Step 5 可能已创建了多余端口

**去重键**: `src_module:src_port -> dst_module:dst_port` 的完整字符串

### latency 冲突处理 (v2.0 新增)

**问题**: 当配置中出现重复连接且 latency 值不同时，应该如何处理？

**决策**: 使用首次出现的 latency 值，对冲突打印 WARNING 日志

**代码** (第 366-387 行):
```cpp
// DEF-02: 在 ConnectionResolver 之前去重 connections
json deduplicated_connections = json::array();
std::set<std::string> seen_connections;
std::map<std::string, int> connection_latencies;
for (const auto& conn : final_config["connections"]) {
    if (!conn.contains("src") || !conn.contains("dst")) continue;
    std::string conn_key = conn["src"].get<std::string>() + "->" + conn["dst"].get<std::string>();
    if (seen_connections.count(conn_key)) {
        int existing_latency = connection_latencies[conn_key];
        int this_latency = conn.value("latency", 0);
        if (this_latency != existing_latency) {
            DPRINTF(CONN, "[WARN] Duplicate connection %s has conflicting latency (first=%d, this=%d) - using first\n",
                    conn_key.c_str(), existing_latency, this_latency);
        } else {
            DPRINTF(CONN, "[CONN] Skipped duplicate connection at resolver stage: %s\n", conn_key.c_str());
        }
        continue;
    }
    seen_connections.insert(conn_key);
    connection_latencies[conn_key] = conn.value("latency", 0);
    deduplicated_connections.push_back(conn);
}
```

**理由选择 WARNING 而非 ERROR**:
- 重复连接通常是配置错误，但不应该阻止仿真运行
- 使用第一个 latency 值保持一致性
- WARNING 日志帮助用户发现配置问题

**Step 6 去重** (第 650-660 行):
```cpp
// DEF-02: 使用同一个 processed_connections 集合去重（Step 6 已填充）
for (auto& conn : final_config["connections"]) {
    if (!conn.contains("src") || !conn.contains("dst")) continue;
    std::string src_full = conn["src"];
    std::string dst_full = conn["dst"];

    auto conn_key = std::make_pair(src_full, dst_full);
    if (processed_connections.count(conn_key)) {
        DPRINTF(CONN, "[ChStream] Skipped duplicate connection %s -> %s\n",
                src_full.c_str(), dst_full.c_str());
        continue;
    }
    // ... 创建端口和 PortPair
    processed_connections.insert(conn_key);
}
```

**实施位置**: 
- Step 5 去重：`src/core/module_factory.cc:366-387`
- Step 6 去重：`src/core/module_factory.cc:650-660, 685`

---

## 决策 7: CFG-08 JSON Schema 验证器策略 (v2.0 新增)

### 问题

如何验证 JSON 配置文件的结构正确性？应该在什么时候执行验证？

### 决策

✅ **实现 validateConfig() 函数，在实例化前执行验证**

**验证策略**:

| 验证项 | 类型 | 处理方式 |
|--------|------|---------|
| 顶层 `modules` 字段 | 必需 | 缺失或类型错误 → ERROR，返回 false |
| 顶层 `connections` 字段 | 必需 | 缺失或类型错误 → ERROR，返回 false |
| 顶层 `version` 字段 | 可选 | 缺失 → WARNING，继续验证 |
| 模块 `name` 字段 | 必需 | 缺失或类型错误 → ERROR，返回 false |
| 模块 `type` 字段 | 必需 | 缺失或类型错误 → ERROR，返回 false |
| RouterTLM 参数 | 条件必需 | 缺少 `node_x/node_y/mesh_x/mesh_y` → ERROR |
| NICTLM 参数 | 条件必需 | 缺少 `node_id` → ERROR |

**错误处理策略**: 快速失败（Fail-Fast）

- 发现第一个错误立即返回 `false`
- 不收集所有错误（简化实现）
- 错误消息包含字段名称和位置

**代码** (第 165-243 行):
```cpp
bool ModuleFactory::validateConfig(const json& config) {
    // 1. 检查顶层必需字段
    if (!config.contains("modules")) {
        printf("[CONFIG ERROR] Missing required field 'modules'\n");
        return false;
    }
    if (!config["modules"].is_array()) {
        printf("[CONFIG ERROR] Field 'modules' must be an array\n");
        return false;
    }

    if (!config.contains("connections")) {
        printf("[CONFIG ERROR] Missing required field 'connections'\n");
        return false;
    }
    if (!config["connections"].is_array()) {
        printf("[CONFIG ERROR] Field 'connections' must be an array\n");
        return false;
    }

    // version 字段可选，缺失时警告
    if (!config.contains("version")) {
        DPRINTF(MODULE, "[CONFIG WARN] Missing optional field 'version'\n");
    }

    // 2. 检查每个模块的必需字段
    for (const auto& mod : config["modules"]) {
        // name 字段检查
        if (!mod.contains("name")) {
            printf("[CONFIG ERROR] Module missing required field 'name'\n");
            return false;
        }
        if (!mod["name"].is_string()) {
            printf("[CONFIG ERROR] Module field 'name' must be a string\n");
            return false;
        }
        std::string name = mod["name"].get<std::string>();

        // type 字段检查
        if (!mod.contains("type")) {
            printf("[CONFIG ERROR] Module '%s' missing required field 'type'\n", name.c_str());
            return false;
        }
        if (!mod["type"].is_string()) {
            printf("[CONFIG ERROR] Module '%s' field 'type' must be a string\n", name.c_str());
            return false;
        }
        std::string type = mod["type"].get<std::string>();

        // 参数来源：params 对象或模块特定字段
        const json* params_src = nullptr;
        if (mod.contains("params")) params_src = &mod["params"];
        else if (type == "RouterTLM" && mod.contains("node_x")) params_src = &mod;
        else if (type == "NICTLM" && mod.contains("node_id")) params_src = &mod;

        if (params_src) {
            const auto& params = *params_src;
            if (type == "RouterTLM") {
                for (auto p : {"node_x", "node_y", "mesh_x", "mesh_y"}) {
                    if (!params.contains(p) || !params[p].is_number_integer()) {
                        printf("[CONFIG ERROR] Module '%s' missing/invalid '%s'\n", name.c_str(), p);
                        return false;
                    }
                }
            }
            if (type == "NICTLM") {
                if (!params.contains("node_id") || !params["node_id"].is_number_integer()) {
                    printf("[CONFIG ERROR] Module '%s' missing/invalid 'node_id'\n", name.c_str());
                    return false;
                }
            }
        } else if (type == "RouterTLM" || type == "NICTLM") {
            printf("[CONFIG ERROR] Module '%s' missing required params\n", name.c_str());
            return false;
        }
    }

    DPRINTF(MODULE, "[CONFIG] Schema validation passed\n");
    return true;
}
```

**设计权衡**:

1. **快速失败 vs 错误收集**:
   - 选择快速失败：实现简单，用户修复一个错误后再运行
   - 未来可改进：收集所有错误一次性显示（需要 `std::vector<std::string>` 错误列表）

2. **参数来源灵活性**:
   - RouterTLM 和 NICTLM 支持两种参数格式：
     - 标准格式：`"params": {"node_x": 0, ...}`
     - 扁平格式：`"node_x": 0` 直接在模块对象中
   - 理由：向后兼容旧配置格式

3. **类型检查严格性**:
   - 使用 `is_number_integer()` 严格要求整数类型
   - 不允许浮点数作为整型参数（如 `"node_x": 0.0` 会报错）

**扩展性** (未来):
- 添加自定义验证器注册表：`registerValidator(type, validator_func)`
- 支持 JSON Schema draft-07 标准验证
- 集成 nlohmann/json 的 `json_schema` 库

**实施位置**: `src/core/module_factory.cc:165-243`

---

## 决策 8: 层级拓扑配置和子网络实例化（G6 共识）

### 问题

层级拓扑（hierarchical topology）和子网络（subnetwork）的配置应该如何设计？

### 决策

✅ **子网络独立编号**，每个子网络内部使用独立的端口索引（NORTH=0, EAST=1, SOUTH=2, WEST=3, LOCAL=4）。

**层级拓扑配置格式**：
```json
{
    "modules": [
        {
            "name": "l1_router",
            "type": "RouterTLM",
            "sub_network": {
                "name": "l1_subnet",
                "modules": [
                    {"name": "l1a", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 1}},
                    {"name": "l1b", "type": "RouterTLM", "params": {"node_x": 1, "node_y": 0, "mesh_x": 2, "mesh_y": 1}}
                ],
                "connections": [
                    {"src": "l1a.1", "dst": "l1b.3", "latency": 1}  // l1a.EAST -> l1b.WEST
                ]
            }
        }
    ]
}
```

**端口索引规则**：
- 子网络内部：独立编号，每个路由器使用自己的 5 端口索引
- 跨子网络连接：通过上层桥接模块处理，不直接暴露内部端口索引
- 与 RouterTLM 现有设计一致，保持局部性

**Python API 支持**（G6 共识）：
```python
class ModuleSpec(BaseModel):
    sub_modules: List["ModuleSpec"] = []  # 嵌套子模块

builder.add_module("noc.l1_router", "L1Router").with_subnetwork([
    ModuleSpec(type="noc.router", name="L1A"),
    ModuleSpec(type="noc.router", name="L1B"),
])
```

**ModuleFactory 嵌套实例化**：
```cpp
// src/core/module_factory.cc
void ModuleFactory::instantiateSubNetwork(const json& sub_network, BaseModule* parent) {
    for (const auto& mod : sub_network["modules"]) {
        // 子网络模块实例化，端口索引独立
        auto* child = createModule(mod);
        parent->addChild(child);
    }
    
    for (const auto& conn : sub_network["connections"]) {
        // 子网络连接绑定
        bindConnection(conn);
    }
}
```

**实施阶段**: Phase 4+（层级拓扑和子网络是 Phase 3+ 之后的扩展能力）

---

## 决策 9: DEF-04 Credit 流控制配置扩展（M4 共识）

### 问题

credit_capacity 和 credit_return_latency 的配置策略是什么？

### 决策

✅ **credit_capacity 自动计算，允许手动覆盖**：

**自动计算公式**：
```
credit_capacity = buffer_size × port_count / avg_latency
```

**credit_return_latency 默认值**：
```
credit_return_latency = connection.latency × 2  (往返延迟)
```

**配置格式**：
```json
{
    "connections": [
        {
            "src": "router_0.1",
            "dst": "router_1.3",
            "latency": 1,
            "credit_flow": {
                "enable": true,
                "credit_capacity": 32,  // 可选，默认自动计算
                "credit_return_latency": 2  // 可选，默认 = latency × 2
            }
        }
    ]
}
```

**Python 配置生成器集成**：
```python
def calculate_credit_capacity(router_config, connections):
    buffer_size = router_config.get("buffer_size", 16)
    port_count = len(connections)
    avg_latency = sum(c.latency for c in connections) / port_count
    return int(buffer_size * port_count / max(avg_latency, 1))

# 自动计算示例
for conn in connections:
    if conn.credit_flow_enabled:
        conn.credit_capacity = calculate_credit_capacity(router_config, connections)
        conn.credit_return_latency = conn.latency * 2  # 往返延迟
```

**手动覆盖场景**：
- 性能调优：高级用户根据实际流量模式调整 credit_capacity
- 特殊拓扑：非对称拓扑需要手动设置不同连接的 credit 配置
- 模拟特殊 credit 返回机制（如批量返回、压缩返回）

**与 G2 共识一致性**：
- 连接级配置（非全局默认）
- Router-to-Router 默认启用
- credit_return_latency 默认 = connection.latency × 2

**实施阶段**: Phase 3.3（配置增强阶段）

---

## 风险与权衡汇总

| 风险 | 影响 | 缓解措施 | 状态 |
|------|------|---------|------|
| connections 追加导致基配置连接无法删除 | 中 | 用户可以通过覆盖整个 connections 字段（不推荐） | ✅ 可接受 |
| depth > 10 不够用 | 极低 | 实际场景中 3 层已足够，10 是安全边界 | ✅ 可接受 |
| DEF-03 仅支持 RouterTLM | 低 | 添加新 BidirectionalPortAdapter 模块时需更新 | ⏳ Phase 3.2 添加警告日志 |
| DEF-04 静默默认可能隐藏配置错误 | 中 | **Phase 3.2 前改进为 WARNING 日志** | ⚠️ 技术债务，高优先级 |
| 延迟绑定重复扫描性能 | 低 | 实例化阶段只调用一次，不是热点路径 | ✅ 可接受 |
| validateConfig 快速失败用户体验 | 低 | 用户需多次运行才能发现所有错误 | ⏳ 未来可改进为错误收集 |
| RouterTLM/NICTLM 双参数格式 | 低 | 增加维护复杂度 | ✅ 向后兼容必要 |

---

## 变更记录

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-05-05 | 记录 Phase 3.1 已实施的 6 个设计决策 |
| 2.0 | 2026-05-05 | 补充 CFG-08 验证器决策（决策 7）<br>补充 DEF-02 latency 冲突处理（决策 6 扩展）<br>标记 DEF-03/DEF-04 改进时间线<br>添加风险状态追踪列<br>补充代码行号引用 |
| 3.0 | 2026-05-05 | 补充术语表和阶段编号说明（m3 共识）<br>补充层级拓扑配置决策（决策 8，G6 共识）<br>补充 DEF-04 Credit 流控制扩展（决策 9，M4 共识）<br>DEF 标注所属阶段（m3 共识） |

## 共识追踪

| 议题 | 状态 | 说明 |
|------|------|------|
| G6 | ✅ 已整合 | 层级拓扑配置，子网络独立编号，ConfigBuilder 支持 add_subnetwork() |
| M4 | ✅ 已整合 | credit_capacity 自动计算，credit_return_latency = latency × 2 |
| m3 | ✅ 已整合 | 术语表、阶段编号统一、DEF 标注所属阶段 |

---

## 后续行动项

| 行动项 | 优先级 | 目标阶段 | 关联决策 |
|--------|--------|---------|---------|
| DEF-04: 添加端口索引非法 WARNING 日志 | 🔴 高 | Phase 3.2 前 | 决策 5 |
| DEF-03: 添加非 RouterTLM 多端口模块警告 | 🟡 中 | Phase 3.2 | 决策 4 |
| validateConfig: 错误收集模式 | 🟢 低 | Phase 3.3+ | 决策 7 |
| DEF-03: 评估泛化方案 | 🟢 低 | Phase 3.3+ | 决策 4 |
| extends: 循环引用精确检测 | 🟢 低 | Phase 4 | 决策 2 |

---

**状态**: ✅ 已实施（Phase 3.1），待改进项已标记并规划时间线
