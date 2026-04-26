# TGMS Configuration Specifications

**Document ID**: SPEC-010  
**Version**: 2.0  
**Date**: 2026-04-26

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

### 2.1 Port Index Format

Port indices are appended to module names using a dot notation: `module_name.port_index`.

- Valid: `router_0_0`, `router_0_0.1`, `ni_0_0.2`
- Invalid: `router_0_0.a`, `router_0_0.01`, `.router_0_0`

The C++ `parsePortSpec()` function uses `std::isdigit()` to parse the index portion.

### 2.2 RouterTLM Port Assignment

| Index | Direction | Connection Target | Description |
|-------|-----------|-------------------|-------------|
| 0 | NORTH | `router_(y-1)_x.SOUTH` | North neighbor (port 2) |
| 1 | EAST | `router_y_(x+1).WEST` | East neighbor (port 3) |
| 2 | SOUTH | `router_(y+1)_x.NORTH` | South neighbor (port 0) |
| 3 | WEST | `router_y_(x-1).EAST` | West neighbor (port 1) |
| 4 | LOCAL | `ni_y_x.NETWORK` | Local NI (port 1 for NICTLM) |

### 2.3 NICTLM Port Assignment

NICTLM uses DualPortStreamAdapter with 2 port groups:

| Group Index | Side | Internal Ports | Description |
|-------------|------|----------------|-------------|
| 0 | PE | req_in, resp_out | Processor/Cache interface |
| 1 | Network | req_out, resp_in | NoC router interface |

In connection specs, NICTLM ports are referenced by group index:
- `ni_0_0` or `ni_0_0.0` = PE side (group 0)
- `ni_0_0.1` = Network side (group 1)

### 2.4 Connection Rules

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
| Step 2.5 | 传递配置 | `instance->set_config(mod["params"])` ← **新增** |
| Step 3 | 创建端口 | `factory.createPorts(instance, type)` |
| Step 4 | 创建适配器 | `factory.createAdapters(instance, type)` |
| Step 5 | 注册适配器 | `ch_adapter_factory.register(instance, type)` |
| Step 6 | 注册多端口 | `ch_adapter_factory.registerBidirectionalPortAdapter<T>(type, N)` |
| Step 7 | 绑定端口 | `factory.bindPorts(connection)` |

### 6.2 NICTLM `port_count` 语义澄清

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

## 7. Version History

| Version | Date | Changes |
|---------|------|---------|
| v2.0 | 2026-04-26 | Added v4.0 schema extensions: hierarchy, coherence_domains, bridges. Added CoherenceDomain and ProtocolBridge parameter specs. Updated type registry with v4.0 types. |
| v1.0 | 2026-04-26 | Initial specification document |

