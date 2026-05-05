# TGMS Configuration Specifications

**Document ID**: SPEC-010  
**Version**: 3.0  
**Date**: 2026-05-05  
**关联 ADR**: ADR-X.9 v3.0 (端口类型系统), ADR-X.10 v3.0 (参数框架), ADR-X.11 v3.0 (配置继承), ADR-X.12 v2.0 (Python 配置生成器)

## 变更日志 (v3.0)

| 日期 | 变更 |
|------|------|
| 2026-05-05 | 与 ADR-X.9/X.10/X.11/X.12 对齐<br>更新端口索引规范（nlohmann/json 序列化）<br>新增 Credit Flow 配置规范<br>新增版本管理字段（SemVer）<br>更新 ModuleFactory 实例化流程（双重验证） |

---

## 0. Version Compatibility

| Version | Description | Compatibility |
|---------|-------------|---------------|
| v3.0 | Single-plane NoC (Mesh, Torus) | Backward compatible |
| v4.0 | Hierarchical heterogeneous SoC with coherence domains | Superset of v3.0 |

## 1. JSON Schema (Draft-07)

### 1.1 v3.0 Schema (Single-Plane NoC)

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "CppTLM NoC Configuration",
  "description": "TGMS v3.0 configuration format for CppTLM NoC simulation",
  "type": "object",
  "required": ["modules", "connections"],
  "properties": {
    "modules": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["name", "type"],
        "properties": {
          "name": {
            "type": "string",
            "pattern": "^[a-zA-Z_][a-zA-Z0-9_]*$",
            "description": "Unique module instance name"
          },
          "type": {
            "type": "string",
            "enum": ["RouterTLM", "NICTLM", "ProcessorTLM", "MemoryTLM"],
            "description": "Module type (must be registered in C++ factory)"
          },
          "params": {
            "type": "object",
            "description": "Module-specific parameters, passed via set_config()",
            "additionalProperties": true
          }
        }
      }
    },
    "connections": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["src", "dst"],
        "properties": {
          "src": {
            "type": "string",
            "pattern": "^[a-zA-Z_][a-zA-Z0-9_]*(\\.[0-9]+)?$",
            "description": "Source module name, optional port index (e.g., router_0_0.1)"
          },
          "dst": {
            "type": "string",
            "pattern": "^[a-zA-Z_][a-zA-Z0-9_]*(\\.[0-9]+)?$",
            "description": "Destination module name, optional port index (e.g., router_0_1.3)"
          },
          "type": {
            "type": "string",
            "description": "Optional connection type hint"
          }
        }
      }
    }
  }
}
```

### 1.2 v4.0 Schema Extensions (Hierarchical SoC)

The v4.0 schema extends v3.0 with three new top-level fields:

```json
{
  "hierarchy": {
    "type": "object",
    "required": ["name", "type"],
    "properties": {
      "name": {"type": "string"},
      "type": {"enum": ["System", "Cluster", "Subsystem"]},
      "children": {"type": "array", "items": {"$ref": "#/definitions/hierarchyNode"}}
    }
  },
  "coherence_domains": {
    "type": "array",
    "items": {
      "type": "object",
      "required": ["name", "protocol", "members"],
      "properties": {
        "name": {"type": "string"},
        "protocol": {"enum": ["MESI", "MOESI", "Directory", "GPU_OWNED", "None"]},
        "members": {"type": "array", "items": {"type": "string"}},
        "snoop_fanout": {"type": "integer", "minimum": 0},
        "directory": {
          "type": "object",
          "properties": {
            "type": {"enum": ["centralized", "distributed"]},
            "home_node_prefix": {"type": "string"}
          }
        }
      }
    }
  },
  "bridges": {
    "type": "array",
    "items": {
      "type": "object",
      "required": ["name", "type", "params"],
      "properties": {
        "name": {"type": "string"},
        "type": {"const": "ProtocolBridge"},
        "params": {
          "type": "object",
          "required": ["input_protocol", "output_protocol", "domain_in", "domain_out"],
          "properties": {
            "input_protocol": {"type": "string"},
            "output_protocol": {"type": "string"},
            "domain_in": {"type": "string"},
            "domain_out": {"type": "string"},
            "address_translation": {
              "type": "array",
              "items": {
                "type": "object",
                "properties": {
                  "input_range": {"type": "array", "items": {"type": "string"}, "minItems": 2, "maxItems": 2},
                  "output_offset": {"type": "string"}
                }
              }
            }
          }
        }
      }
    }
  }
}
```

## 2. Port Index Specification

### 2.1 端口索引定义（与 ADR-X.9 v3.0 对齐）

**端口索引是端口类型的物理实现**。每个模块的端口索引对应特定的物理端口位置。

**格式**: `module_name.port_index`

- `module_name`: 模块实例名称（如 `router_0_0`）
- `port_index`: 非负整数，表示端口编号（如 `0`, `1`, `2`）
- 分隔符: `.`（点号）

**示例**:
- `router_0_0.1` — router_0_0 的端口 1（EAST）
- `ni_0_0` — ni_0_0 的默认端口 0（单端口模块可省略索引）

**已弃用命名格式**（与 ADR-X.9 v3.0 一致）：

以下命名端口格式标记为 **DEPRECATED**，将在 Phase 4 移除：
- `router.NORTH`, `router.EAST`, `router.SOUTH`, `router.WEST`, `router.LOCAL`
- `r0.E_out`, `r0.N_in` 等旧格式

**弃用理由**:
- 命名格式与端口索引映射不一致
- 数字索引是 RouterTLM 的物理实现，更稳定可靠
- `std::isdigit('E')` 为 false，这些格式会被解析为默认端口 0

**迁移建议**:
- 新配置应使用数字索引：`router_0.0`（NORTH）、`router_0.4`（LOCAL）
- 如需可读性，应使用端口别名系统：`{"r0": {"NORTH": "0"}}`

### 2.2 端口编号约定

| 模块类型 | 端口数 | 端口索引 | 语义 | Python 枚举 |
|----------|--------|----------|------|------------|
| RouterTLM | 5 | 0 | NORTH | `RouterPort.NORTH` |
| | | 1 | EAST | `RouterPort.EAST` |
| | | 2 | SOUTH | `RouterPort.SOUTH` |
| | | 3 | WEST | `RouterPort.WEST` |
| | | 4 | LOCAL | `RouterPort.LOCAL` |
| NICTLM | 2 (port groups) | 0 | **PE 侧**（连接 Cache/CPU） | `NICPort.PE` |
| | | 1 | **Network 侧**（连接 Router） | `NICPort.NETWORK` |

> **NICTLM 连接说明**: NICTLM 使用 DualPortStreamAdapter，`port_count = 2` 表示**2 个端口组**（而非 4 个独立端口）。NI → Router 连接使用端口索引 `1`（Network side → Router.LOCAL）。Processor → NI 连接使用端口索引 `0`（PE side，默认）。每组内部有 4 个 ChStreamPort，但连接索引只区分组。

| CrossbarTLM | 4 | 0-3 | 输入端口 0-3 | - |
| CacheTLM | 1 | 0 | 唯一端口 | - |
| MemoryTLM | 1 | 0 | 唯一端口 | - |
| CPUSim | 1 | 0 | 唯一端口 | - |

### 2.3 Python API 端口枚举（新增）

Python 配置生成器提供端口索引枚举，同时允许直接使用数字：

```python
from cpptlm_config import RouterPort, NICPort

# 方式 1：使用枚举（推荐，类型安全）
builder.add_connection(ConnectionSpec(
    src=f"router_{x}_{y}.{RouterPort.NORTH.value}",  # 0
    dst=f"router_{x+1}_{y}.{RouterPort.WEST.value}"  # 3
))

# 方式 2：直接使用数字（简洁）
builder.add_connection(ConnectionSpec(
    src=f"router_{x}_{y}.0",  # NORTH
    dst=f"router_{x+1}_{y}.3"  # WEST
))
```

### 2.4 C++ 解析规则

```cpp
// parsePortSpec 实现（src/core/module_factory.cc:22-28）
std::pair<std::string, std::string> parsePortSpec(const std::string& full_name) {
    size_t dot_pos = full_name.find('.');
    if (dot_pos == std::string::npos) {
        return {full_name, ""};
    }
    return {full_name.substr(0, dot_pos), full_name.substr(dot_pos + 1)};
}
```

**端口索引解析规则**（module_factory.cc:374-380）:

```cpp
unsigned src_idx = 0, dst_idx = 0;
if (!src_spec.empty() && std::isdigit(src_spec[0])) src_idx = std::stoul(src_spec);
if (!dst_spec.empty() && std::isdigit(dst_spec[0])) dst_idx = std::stoul(dst_spec);

// 单端口模块忽略端口索引
if (ch_adapters.count(src_name) && !factory.isMultiPort(module_types[src_name])) src_idx = 0;
if (ch_adapters.count(dst_name) && !factory.isMultiPort(module_types[dst_name])) dst_idx = 0;
```

**关键约束**:
1. 端口索引必须是数字（`std::isdigit` 判断）
2. 非数字索引（如 `.E_out`）被忽略，默认使用 0
3. 单端口模块（`isMultiPort() == false`）强制使用端口 0
4. 多端口模块（`isMultiPort() == true`）使用指定索引，未指定则默认 0

### 2.5 端口组规范（新增，与 ADR-X.9 v3.0 对齐）

**端口组（Port Groups）**是逻辑端口容器，组内端口共享相同的 BundleType 和连接策略。

**端口组配置格式**：
```json
{
    "modules": [
        {
            "name": "nic_0",
            "type": "NICTLM",
            "port_groups": [
                {
                    "name": "eth0_group",
                    "ports": [
                        {"index": 0, "role": "bi_directional", "bundle": "noc_flit"},
                        {"index": 1, "role": "bi_directional", "bundle": "noc_flit"}
                    ],
                    "bundle_type": "bundle_master"
                }
            ]
        }
    ]
}
```

**端口组与端口类型关系**：
- 端口组内的每个端口独立定义 PortRole 和 BundleType
- 端口组级别的 `bundle_type` 定义组的捆绑模式（`single`、`bundle_master`、`bundle_slave`）
- `bundle_master` 端口负责管理整个组的连接

### 2.6 连接规则

1. Router-to-Router connections must specify port indices on both ends
2. NI-to-Router connections: NI uses group index 1 (Network side), Router uses port 4 (LOCAL)
3. Processor-to-NI connections: NI uses group index 0 (PE side)
4. If no port index is specified, default is 0

## 3. Module Parameters

### 3.1 RouterTLM Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `node_x` | int | Yes | X coordinate in mesh (0-based) |
| `node_y` | int | Yes | Y coordinate in mesh (0-based) |
| `mesh_x` | int | Yes | Total mesh width |
| `mesh_y` | int | Yes | Total mesh height |

Example:
```json
{
  "name": "router_0_0",
  "type": "RouterTLM",
  "params": {
    "node_x": 0,
    "node_y": 0,
    "mesh_x": 4,
    "mesh_y": 4
  }
}
```

### 3.2 NICTLM Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `node_id` | int | Yes | Logical node ID |
| `mesh_x` | int | Yes | Mesh width (for address mapping) |
| `mesh_y` | int | Yes | Mesh height (for address mapping) |
| `address_regions` | array | No | Address-to-node mappings |

Example:
```json
{
  "name": "ni_0_0",
  "type": "NICTLM",
  "params": {
    "node_id": 0,
    "mesh_x": 4,
    "mesh_y": 4,
    "address_regions": [
      {
        "base": "0x00000000",
        "size": "0x40000000",
        "target_node": 12,
        "type": "MEMORY_CTRL"
      }
    ]
  }
}
```

### 3.3 v4.0 CoherenceDomain Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `protocol` | string | Yes | Coherence protocol: MESI, MOESI, Directory, GPU_OWNED, None |
| `members` | array | Yes | List of module names in this domain |
| `snoop_fanout` | int | No | Number of snoop targets (0 = no broadcast) |
| `directory.type` | string | No | Directory type: centralized or distributed |
| `directory.home_node_prefix` | string | No | Module name prefix for home nodes |

### 3.4 v4.0 ProtocolBridge Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `input_protocol` | string | Yes | Input protocol type (CHI_L2, CHI_L3, TileLink, AXI, etc.) |
| `output_protocol` | string | Yes | Output protocol type |
| `domain_in` | string | Yes | Source coherence domain name |
| `domain_out` | string | Yes | Destination coherence domain name |
| `address_translation` | array | No | Address range translation rules |

## 4. Configuration Merge Rules

When multiple configuration sources are used:

1. **Base config** loaded from JSON file
2. **Override config** (if specified) merged on top
3. Merge rules:
   - Modules: matched by `name`, `params` merged (override wins)
   - Connections: appended (no deduplication)
   - Top-level fields: override wins

## 5. Python-C++ Type Registry Sync

### 5.1 Type Registry Format

`type_registry.json`:
```json
{
  "modules": {
    "RouterTLM": {
      "cpp_macro": "REGISTER_CHSTREAM(RouterTLM)",
      "port_count": 5,
      "adapter_type": "BidirectionalPortAdapter",
      "params_schema": "router_params",
      "version": "3.0"
    },
    "NICTLM": {
      "cpp_macro": "REGISTER_CHSTREAM(NICTLM)",
      "port_count": 2,
      "adapter_type": "DualPortStreamAdapter",
      "params_schema": "ni_params",
      "version": "3.0"
    },
    "ProtocolBridge": {
      "cpp_macro": "REGISTER_CHSTREAM(ProtocolBridge)",
      "port_count": 2,
      "adapter_type": "UnidirectionalPortAdapter",
      "params_schema": "bridge_params",
      "version": "4.0"
    },
    "CrossbarFabric": {
      "cpp_macro": "REGISTER_CHSTREAM(CrossbarFabric)",
      "port_count": -1,
      "adapter_type": "MultiPortStreamAdapter",
      "params_schema": "crossbar_params",
      "version": "4.0"
    }
  }
}
```

### 5.2 Sync Mechanism

1. C++ `REGISTER_CHSTREAM` macros define available types at compile time
2. Python tools read `type_registry.json` to validate configs
3. CI validation: compare C++ factory registration with `type_registry.json`
4. When C++ adds new type, update `type_registry.json` manually (or via script parsing macros)

## 6. ModuleFactory Instantiation Flow

### 6.1 7-Step Instantiation Sequence

ModuleFactory 按以下顺序实例化 JSON 配置中的模块：

| Step | 操作 | 说明 |
|------|------|------|
| Step 1 | 解析 JSON | 读取 `modules[]` 和 `connections[]` |
| Step 2 | 创建实例 | `new_module = factory.createModule(name, type)` |
| Step 2.5 | 传递配置 | `instance->set_config(mod["params"])` ← **Phase 1 新增** |
| Step 3 | 创建端口 | `factory.createPorts(instance, type)` |
| Step 4 | 创建适配器 | `factory.createAdapters(instance, type)` |
| Step 5 | 注册适配器 | `ch_adapter_factory.register(instance, type)` |
| Step 6 | 注册多端口 | `ch_adapter_factory.registerBidirectionalPortAdapter<T>(type, N)` |
| Step 7 | 绑定端口 | `factory.bindPorts(connection)` |

### 6.2 参数验证流程（与 ADR-X.10 v3.0 对齐）

**双重验证机制**：

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

**ParamRule 统一验证逻辑**：

ModuleFactory 加载 ADR-X.10 定义的 ParamRule 进行验证，避免重复实现验证逻辑：

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
            // 类型检查、范围检查...
        }
    }
    return true;
}
```

### 6.3 set_config() 异常处理（与 ADR-X.10 v3.0 对齐）

**set_config() 在 ParamRule 验证失败时抛出异常**（`std::invalid_argument` 或自定义 `ParamValidationError`）。

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

### 6.4 NICTLM `port_count` 语义澄清

NICTLM 在 `type_registry.json` 中声明 `"port_count": 2`，但此值表示**端口组（Port Group）数量**，而非独立端口数量。

```json
"NICTLM": {
  "port_count": 2,
  "adapter_type": "DualPortStreamAdapter"
}
```

**含义**:
- NICTLM 使用 `DualPortStreamAdapter`，创建 **2 个端口组**
- 组 0（PE 侧）：连接 CPU/Cache，包含 4 个 ChStreamPort（pe_req_in, pe_resp_out, resp_in, req_out）
- 组 1（Network 侧）：连接 Router，包含 4 个 ChStreamPort（net_req_out, net_resp_in, resp_in, req_out）
- 在 JSON `connections[]` 中引用 NICTLM 端口时，使用**组索引**（0 或 1），而非组内端口索引

**连接示例**:
```json
{
  "src": "ni_0_0.1",   // NICTLM 组 1（Network 侧）→ Router.LOCAL
  "dst": "router_0_0.4" // Router 端口 4（LOCAL）
}
```

### 6.3 `set_config` 参数传递流程

Step 2.5 是 Phase 1 新增的关键步骤，确保 JSON `params` 字段能传递到模块实例：

```cpp
// module_factory.cc Step 2.5（新增）
void ModuleFactory::applyConfig(SimObject* instance, const nlohmann::json& params) {
    if (params.contains("node_x")) {
        instance->setConfigInt("node_x", params["node_x"]);
    }
    if (params.contains("node_y")) {
        instance->setConfigInt("node_y", params["node_y"]);
    }
    // ... 其他参数
}
```

RouterTLM 在 `init()` 中从配置读取 `node_x`/`node_y` 覆盖默认值。

## 7. Credit Flow 配置规范（新增，与 ADR-X.11 v3.0 对齐）

### 7.1 Credit Flow 配置格式

Credit-based Flow Control 配置用于 Router 间链路，防止缓冲区溢出。

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

### 7.2 credit_capacity 自动计算

**默认自动计算，允许手动覆盖**。

**自动计算公式**：
```
credit_capacity = buffer_size × port_count / avg_latency
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

### 7.3 credit_return_latency 默认值

**credit_return_latency 默认 = connection.latency × 2**（往返延迟）。

**理由**：
- Credit 返回需要经过相同的物理路径，往返延迟是单向延迟的 2 倍
- 绑定 connection.latency 确保 credit 返回时间与实际网络延迟一致
- 避免 credit 过早返回导致缓冲区溢出
- 允许手动覆盖以模拟特殊 credit 返回机制

### 7.4 与 G2 共识一致性

- 连接级配置（非全局默认）
- Router-to-Router 默认启用
- credit_return_latency 默认 = connection.latency × 2

---

## 8. 版本管理规范（新增，与 ADR-X.12 v2.0 对齐）

### 8.1 版本字段定义

所有拓扑配置必须包含 `version` 字段，采用**语义化版本（SemVer）**格式：

```json
{
    "name": "mesh_4x4",
    "version": "1.2.0",
    "metadata": {
        "created_at": "2026-05-05T10:00:00Z",
        "modified_at": "2026-05-05T15:30:00Z",
        "changelog": ["Added 2 new routers", "Fixed connection latency"],
        "author": "admin"
    },
    "modules": [...],
    "connections": [...]
}
```

### 8.2 SemVer 版本号更新策略

| 版本部分 | 格式 | 变更条件 | 示例 |
|---------|------|---------|------|
| **MAJOR** | `X`.y.z | 拓扑结构破坏性变更（删除模块、改变连接拓扑） | 0.**1**.0 → 0.**2**.0 |
| **MINOR** | x.`Y`.z | 向后兼容的功能添加（新增模块、添加可选参数） | 0.1.**0** → 0.1.**1** |
| **PATCH** | x.y.`Z` | 向后兼容的修复（修正参数值、优化延迟配置） | 0.1.0 → 0.1.**1** |

### 8.3 Python 配置生成器版本管理

```python
from cpptlm_config import ConfigBuilder, ConfigMetadata

builder = ConfigBuilder(name="mesh_4x4")
builder.metadata.version = "1.0.0"

# 升级版本号
def bump_version(self, level: str = "patch"):
    major, minor, patch = map(int, self.metadata.version.split("."))
    if level == "major":
        major += 1
        minor = 0
        patch = 0
    elif level == "minor":
        minor += 1
        patch = 0
    elif level == "patch":
        patch += 1
    self.metadata.version = f"{major}.{minor}.{patch}"

# 添加变更日志
builder.metadata.changelog.append("Added credit flow configuration")
```

### 8.4 与 ARCH-010 TopologyRegistry 对齐

- Python 侧版本管理与 C++ 侧 TopologyRegistry 一致
- 支持版本回滚和 A/B 对比
- 为 Phase 4+ 的拓扑版本历史预留基础

---

## 9. Version History

| Version | Date | Changes |
|---------|------|---------|
| v3.0 | 2026-05-05 | 与 ADR-X.9/X.10/X.11/X.12 对齐<br>更新端口索引规范（nlohmann/json 序列化、端口枚举、端口组）<br>新增 Credit Flow 配置规范（自动计算、往返延迟）<br>新增版本管理字段（SemVer）<br>更新 ModuleFactory 实例化流程（双重验证、异常处理） |
| v2.0 | 2026-04-26 | Added v4.0 schema extensions: hierarchy, coherence_domains, bridges. Added CoherenceDomain and ProtocolBridge parameter specs. Updated type registry with v4.0 types. |
| v1.0 | 2026-04-26 | Initial specification document |

