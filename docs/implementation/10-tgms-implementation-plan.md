# TGMS Implementation Plan

**Document ID**: IMPL-010  
**Version**: 2.0  
**Date**: 2026-04-26

## 1. Overview

This document provides the detailed implementation plan for TGMS (Topology Generation, Maintenance and Management System). It covers code changes, test strategies, and phased rollout.

**Architecture reference**: [10-topology-generation-management-system.md](../architecture/10-topology-generation-management-system.md)  
**Specifications**: [10-tgms-specifications.md](../architecture/10-tgms-specifications.md)

### 1.1 Version Scope

| Version | Scope | Status |
|---------|-------|--------|
| v3.0 | Single-plane NoC (Phase 1-3) | Planned |
| v4.0 | Hierarchical heterogeneous SoC with coherence domains (Phase 4-6) | Planned |

## 2. Implementation Roadmap

### Phase 1: Core Infrastructure (2-3 weeks)

**Goal**: Enable JSON-driven parameter passing to modules

| Task | Gap | Effort | Dependencies |
|------|-----|--------|-------------|
| 1.1 Add set_config to SimObject | G1 | 1 day | None |
| 1.2 Add set_config to RouterTLM | G1 | 1 day | 1.1 |
| 1.3 Add set_config to NICTLM | G1 | 1 day | 1.1 |
| 1.4 ModuleFactory Step 2.5 | G6 | 1 day | 1.1 |
| 1.5 Update config format to v3.0 | G7 | 2 days | 1.4 |
| 1.6 Unit tests | G1, G6 | 3 days | 1.2-1.5 |

### Phase 2: Python Toolchain (2-3 weeks)

**Goal**: Generate correct TGMS v3.0 configs with port indices

| Task | Gap | Effort | Dependencies |
|------|-----|--------|-------------|
| 2.1 Port index generation | G2 | 3 days | Phase 1 |
| 2.2 NoCBuilder migration | G4 | 3 days | 2.1 |
| 2.3 NoCMesh cleanup | G5 | 2 days | 2.1 |
| 2.4 Integration tests | G2, G4 | 3 days | 2.1-2.3 |

### Phase 3: Validation & Examples (1-2 weeks)

**Goal**: End-to-end validation with example configs

| Task | Gap | Effort | Dependencies |
|------|-----|--------|-------------|
| 3.1 2x2 Mesh validation | All | 2 days | Phase 2 |
| 3.2 4x4 Mesh validation | All | 3 days | 3.1 |
| 3.3 Example configs | G7 | 1 day | 3.1-3.2 |

### Phase 4: Hierarchy Core (v4.0, 3-4 weeks)

**Goal**: Support hierarchical topology tree and CoherenceDomain

| Task | Gap | Effort | Dependencies |
|------|-----|--------|-------------|
| 4.1 Hierarchy tree parser | G8 | 3 days | Phase 1 |
| 4.2 CoherenceDomain C++ module | G9 | 5 days | 4.1 |
| 4.3 Domain boundary validation | G9 | 2 days | 4.2 |
| 4.4 Snoop routing logic | G9 | 3 days | 4.2 |
| 4.5 Directory protocol stub | G9 | 5 days | 4.2 |
| 4.6 Python hierarchy generator | G8 | 5 days | 4.1-4.3 |

### Phase 5: Protocol Bridge (v4.0, 2-3 weeks)

**Goal**: ProtocolBridge module and cross-protocol routing

| Task | Gap | Effort | Dependencies |
|------|-----|--------|-------------|
| 5.1 ProtocolBridge C++ module | G10 | 5 days | Phase 4 |
| 5.2 Address translation engine | G10 | 3 days | 5.1 |
| 5.3 Protocol conversion logic | G10 | 5 days | 5.1 |
| 5.4 Cross-protocol validation | G10 | 3 days | 5.1-5.3 |
| 5.5 Python bridge config generator | G10 | 3 days | 5.1-5.4 |

### Phase 6: Multi-Cluster SoC Validation (v4.0, 2-3 weeks)

**Goal**: End-to-end validation with realistic SoC config

| Task | Gap | Effort | Dependencies |
|------|-----|--------|-------------|
| 6.1 2x CPU Cluster + GPU Cluster config | G8, G9 | 3 days | Phase 4, 5 |
| 6.2 Cross-cluster coherence validation | G9 | 5 days | 6.1 |
| 6.3 Protocol bridge integration test | G10 | 3 days | 6.1 |
| 6.4 Full SoC example (4 CPU + 1 GPU) | All | 5 days | 6.2-6.3 |

## 3. Detailed Implementation Steps

### 3.1 Phase 1.1: Add set_config to SimObject

**File**: `include/core/sim_object.hh`

Add to `SimObject` class:
```cpp
#include <nlohmann/json.hpp>

class SimObject {
protected:
    nlohmann::json config_params_;

public:
    virtual void set_config(const nlohmann::json& params) {
        config_params_ = params;
    }
    
    const nlohmann::json& get_config() const {
        return config_params_;
    }
};
```

### 3.2 Phase 1.2: Add set_config to RouterTLM

**File**: `include/tlm/router_tlm.hh`

Add override:
```cpp
class RouterTLM : public ChStreamModuleBase {
public:
    void set_config(const nlohmann::json& params) override {
        ChStreamModuleBase::set_config(params);
        if (params.contains("node_x")) node_x_ = params["node_x"].get<int>();
        if (params.contains("node_y")) node_y_ = params["node_y"].get<int>();
        if (params.contains("mesh_x")) mesh_x_ = params["mesh_x"].get<int>();
        if (params.contains("mesh_y")) mesh_y_ = params["mesh_y"].get<int>();
    }
};
```

### 3.3 Phase 1.3: Add set_config to NICTLM

**File**: `include/tlm/nic_tlm.hh`

Add override:
```cpp
class NICTLM : public ChStreamModuleBase {
public:
    void set_config(const nlohmann::json& params) override {
        ChStreamModuleBase::set_config(params);
        if (params.contains("node_id")) {
            node_id_ = params["node_id"].get<int>();
        }
        if (params.contains("address_regions")) {
            for (auto& region : params["address_regions"]) {
                add_address_region(
                    region["base"].get<uint64_t>(),
                    region["size"].get<uint64_t>(),
                    region["target_node"].get<int>(),
                    region.value("type", "DEFAULT")
                );
            }
        }
    }
};
```

### 3.4 Phase 1.4: ModuleFactory Step 2.5

**File**: `src/core/module_factory.cc`

Insert between Step 2 (create instances) and Step 3 (create ports):

```cpp
// Step 2.5: Pass JSON config parameters to module instances (NEW)
for (auto& mod : final_config["modules"]) {
    if (!mod.contains("name")) continue;
    std::string name = mod["name"];
    auto it = object_instances.find(name);
    if (it != object_instances.end() && mod.contains("params")) {
        it->second->set_config(mod["params"]);
        DPRINTF(TGMConfig, "[CONFIG] Set params for module: %s\n", name.c_str());
    }
}
```

### 3.5 Phase 1.5: Update Config Format to v3.0

Migrate existing configs to put module parameters in `params` object.

**Before (v2.x)**:
```json
{"name": "router_0_0", "type": "RouterTLM", "node_x": 0, "node_y": 0}
```

**After (v3.0)**:
```json
{"name": "router_0_0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 4, "mesh_y": 4}}
```

### 3.6 Phase 2.1: Port Index Generation

**File**: `scripts/topology_generator.py`

Update `generate_mesh()` to append port indices:

```python
def generate_mesh(mesh_x, mesh_y):
    connections = []
    for y in range(mesh_y):
        for x in range(mesh_x):
            # East-West connections
            if x < mesh_x - 1:
                connections.append({
                    "src": f"router_{y}_{x}.1",  # EAST port
                    "dst": f"router_{y}_{x+1}.3"  # WEST port
                })
                connections.append({
                    "src": f"router_{y}_{x+1}.3",  # WEST port
                    "dst": f"router_{y}_{x}.1"  # EAST port
                })
            # North-South connections
            if y < mesh_y - 1:
                connections.append({
                    "src": f"router_{y}_{x}.2",  # SOUTH port
                    "dst": f"router_{y+1}_{x}.0"  # NORTH port
                })
                connections.append({
                    "src": f"router_{y+1}_{x}.0",  # NORTH port
                    "dst": f"router_{y}_{x}.2"  # SOUTH port
                })
            # NI-Router connections
            connections.append({
                "src": f"ni_{y}_{x}.1",  # NICTLM Network side (group 1)
                "dst": f"router_{y}_{x}.4"  # Router LOCAL port
            })
    return connections
```

### 3.7 Phase 2.2: NoCBuilder Migration

**File**: `scripts/noc_builder.py`

Replace named port references (`.E_out`, `.W_in`) with numeric indices:

```python
# Before (v2.x)
port_map = {"NORTH": ".N_out", "EAST": ".E_out", ...}

# After (v3.0)
port_map = {"NORTH": ".0", "EAST": ".1", "SOUTH": ".2", "WEST": ".3", "LOCAL": ".4"}
```

### 3.8 Phase 4.1: Hierarchy Tree Parser

**File**: `src/core/module_factory.cc`

Add Step 0 to parse hierarchy tree before module instantiation:

```cpp
// Step 0: Parse hierarchy tree (v4.0)
if (config.contains("hierarchy")) {
    parse_hierarchy_tree(config["hierarchy"]);
}

// Step 1: Parse coherence domains (v4.0)
if (config.contains("coherence_domains")) {
    for (auto& domain : config["coherence_domains"]) {
        auto* coh_domain = new CoherenceDomain(domain["name"]);
        coh_domain->set_protocol(domain["protocol"]);
        coh_domain->set_members(domain["members"]);
        coherence_domains_[domain["name"]] = coh_domain;
    }
}
```

### 3.9 Phase 4.2: CoherenceDomain C++ Module

**File**: `include/core/coherence_domain.hh` (NEW)

```cpp
class CoherenceDomain : public SimObject {
private:
    std::string protocol_;
    std::vector<std::string> members_;
    int snoop_fanout_;
    std::unique_ptr<Directory> directory_;

public:
    void set_protocol(const std::string& proto) { protocol_ = proto; }
    void set_members(const json& member_list) {
        members_ = member_list.get<std::vector<std::string>>();
    }
    void set_snoop_fanout(int fanout) { snoop_fanout_ = fanout; }

    bool is_member(const std::string& module_name) const;
    std::vector<std::string> get_snoop_targets(const std::string& requester) const;
    int lookup_home_node(uint64_t addr) const;  // Directory-based routing
};
```

### 3.10 Phase 5.1: ProtocolBridge C++ Module

**File**: `include/core/protocol_bridge.hh` (NEW)

```cpp
struct AddressTranslationRule {
    uint64_t input_base;
    uint64_t input_size;
    uint64_t output_offset;
};

class ProtocolBridge : public ChStreamModuleBase {
private:
    std::string input_protocol_;
    std::string output_protocol_;
    std::string domain_in_;
    std::string domain_out_;
    std::vector<AddressTranslationRule> addr_rules_;

public:
    void set_config(const json& params) override {
        ChStreamModuleBase::set_config(params);
        input_protocol_ = params["input_protocol"].get<std::string>();
        output_protocol_ = params["output_protocol"].get<std::string>();
        domain_in_ = params["domain_in"].get<std::string>();
        domain_out_ = params["domain_out"].get<std::string>();

        if (params.contains("address_translation")) {
            for (auto& rule : params["address_translation"]) {
                addr_rules_.push_back({
                    parse_hex(rule["input_range"][0]),
                    parse_hex(rule["input_range"][1]),
                    parse_hex(rule["output_offset"])
                });
            }
        }
    }

    uint64_t translate_address(uint64_t input_addr) const;
    std::string get_output_protocol() const { return output_protocol_; }
};
```

### 3.11 Phase 4.6: Python Hierarchy Generator

**File**: `scripts/topology_generator.py`

Add hierarchical topology generation:

```python
class HierarchicalTopologyGenerator:
    def __init__(self):
        self.system = TopologyNode("soc", "System")

    def add_cluster(self, name, cluster_type, interconnect_type=None):
        cluster = TopologyNode(name, cluster_type)
        cluster.interconnect = interconnect_type
        self.system.add_child(cluster)
        return cluster

    def add_coherence_domain(self, parent, name, protocol, members):
        domain = TopologyNode(name, "CoherenceDomain")
        domain.protocol = protocol
        domain.members = members
        parent.add_child(domain)
        return domain

    def to_json(self) -> dict:
        return {
            "version": "4.0",
            "hierarchy": self.system.to_dict(),
            "coherence_domains": self._collect_domains(),
            "modules": self._collect_modules(),
            "connections": self._collect_connections()
        }
```

## 4. Validation Strategy

### 4.1 Unit Tests (Phase 1)

| Test | Description | Pass Criteria |
|------|-------------|---------------|
| TC-01 | SimObject.set_config() stores params | `get_config()` returns stored JSON |
| TC-02 | RouterTLM.set_config() sets coordinates | `node_x_`, `node_y_`, `mesh_x_`, `mesh_y_` match params |
| TC-03 | NICTLM.set_config() sets node_id | `node_id_` matches params |
| TC-04 | NICTLM.set_config() adds address regions | `addr_map_` contains expected regions |
| TC-05 | ModuleFactory calls set_config | DPRINTF output shows `[CONFIG] Set params` |
| TC-06 | ModuleFactory ignores missing params | No crash when `params` field absent |
| TC-07 | Existing 367 tests pass | Zero regressions |

### 4.2 Integration Tests (Phase 2)

| Test | Description | Pass Criteria |
|------|-------------|---------------|
| TC-08 | topology_generator.py 2x2 output | All connections have port indices |
| TC-09 | topology_generator.py 4x4 output | All connections have port indices |
| TC-10 | Generated config loads in ModuleFactory | No parse errors, all modules instantiated |
| TC-11 | NoCBuilder v3.0 output | No named port references |

### 4.3 End-to-End Tests (Phase 3)

| Test | Description | Pass Criteria |
|------|-------------|---------------|
| TC-12 | 2x2 Mesh flit routing | Flit from proc_0_0 reaches proc_1_1 |
| TC-13 | 4x4 Mesh flit routing | Flit from proc_0_0 reaches proc_3_3 via XY routing |
| TC-14 | XY routing path verification | Path: (0,0)->(0,1)->(0,2)->(0,3)->(1,3)->(2,3)->(3,3) |
| TC-15 | NICTLM address mapping | Traffic to memory address routes to correct node |

### 4.4 Hierarchy Tests (Phase 4)

| Test | Description | Pass Criteria |
|------|-------------|---------------|
| TC-16 | Hierarchy tree parsing | JSON hierarchy correctly parsed into tree structure |
| TC-17 | CoherenceDomain member validation | All members correctly assigned to domain |
| TC-18 | Domain boundary enforcement | Cross-domain connections rejected without bridge |
| TC-19 | Snoop routing within domain | Snoop message reaches all domain members |
| TC-20 | Directory-based home node lookup | Home node correctly determined for address |
| TC-21 | Python hierarchy generator output | Generated JSON matches v4.0 schema |

### 4.5 Protocol Bridge Tests (Phase 5)

| Test | Description | Pass Criteria |
|------|-------------|---------------|
| TC-22 | ProtocolBridge address translation | Input address correctly translated to output range |
| TC-23 | Cross-protocol message conversion | CHI-L2 message converted to CHI-L3 format |
| TC-24 | Protocol attribute conversion | Coherence attributes mapped correctly |
| TC-25 | Bridge domain validation | domain_in/domain_out must exist in config |

### 4.6 Multi-Cluster SoC Tests (Phase 6)

| Test | Description | Pass Criteria |
|------|-------------|---------------|
| TC-26 | 2x CPU Cluster + GPU Cluster config | Config loads without errors |
| TC-27 | Cross-cluster coherence | Cache line shared between clusters via bridge |
| TC-28 | Directory protocol correctness | Directory entries updated on read/write |
| TC-29 | Full SoC routing | Packet from CPU Cluster 0 to GPU traverses bridge |
| TC-30 | Snoop filtering across domains | Cross-domain snoops filtered by directory |

## 5. Dependency Graph

### 5.1 v3.0 Dependencies

```
Phase 1.1 (SimObject set_config)
    |
    +-- Phase 1.2 (RouterTLM set_config)
    |       |
    |       +-- Phase 1.6 (Unit tests)
    +-- Phase 1.3 (NICTLM set_config)
    |       |
    |       +-- Phase 1.6 (Unit tests)
    +-- Phase 1.4 (ModuleFactory Step 2.5)
            |
            +-- Phase 1.5 (Config format v3.0)
                    |
                    +-- Phase 1.6 (Unit tests)
                            |
Phase 2.1 (Port index gen) <----+
    |
    +-- Phase 2.2 (NoCBuilder migration)
    |       |
    |       +-- Phase 2.4 (Integration tests)
    +-- Phase 2.3 (NoCMesh cleanup)
            |
            +-- Phase 2.4 (Integration tests)
                    |
Phase 3.1 (2x2 validation) <----+
    |
    +-- Phase 3.2 (4x4 validation)
            |
            +-- Phase 3.3 (Example configs)
```

### 5.2 v4.0 Dependencies

```
Phase 4.1 (Hierarchy tree parser)
    |
    +-- Phase 4.2 (CoherenceDomain C++)
            |
            +-- Phase 4.3 (Domain boundary validation)
            |       |
            |       +-- Phase 4.4 (Snoop routing)
            |               |
            |               +-- Phase 4.5 (Directory protocol)
            |                       |
            |                       +-- Phase 4.6 (Python hierarchy generator)
            |
            +-- Phase 5.1 (ProtocolBridge C++)
                    |
                    +-- Phase 5.2 (Address translation)
                    |       |
                    |       +-- Phase 5.3 (Protocol conversion)
                    |               |
                    |               +-- Phase 5.4 (Cross-protocol validation)
                    |                       |
                    |                       +-- Phase 5.5 (Python bridge generator)
                    |
                    +-- Phase 6.1 (2x CPU + GPU Cluster config)
                            |
                            +-- Phase 6.2 (Cross-cluster coherence)
                            |       |
                            |       +-- Phase 6.4 (Full SoC example)
                            +-- Phase 6.3 (Bridge integration test)
                                    |
                                    +-- Phase 6.4 (Full SoC example)
```

## 6. Phase 1 Acceptance Checklist

- [ ] Compilation passes (cmake + make, zero warnings)
- [ ] All 367 existing tests pass without modification
- [ ] `topology_generator.py --type mesh --size 2x2` outputs connections with port indices
- [ ] Generated `mesh_2x2.json` has Router-to-Router connections as `router_X_Y.N -> router_X_Y.M`
- [ ] NICTLM-to-Router connections use index 1 (Network side)
- [ ] ModuleFactory DPRINTF output shows `[CONFIG] Set params for module: <name>`
- [ ] RouterTLM instances have correct `node_x/y` values matching JSON params
- [ ] 4x4 Mesh end-to-end: flit from proc_0_0 to proc_3_3 traverses 6 hops
- [ ] XY routing path verified: (0,0)->(0,1)->(0,2)->(0,3)->(1,3)->(2,3)->(3,3)

## 7. Rollout & Monitoring

### 7.1 Deployment Strategy

| Environment | Strategy | Notes |
|------------|----------|-------|
| Development | Local build + manual test | Fast iteration |
| CI/CD | GitHub Actions automated | On every PR |
| Staging | Manual deployment | Pre-release validation |
| Production | Blue-green deployment | Zero downtime |

### 7.2 Monitoring Points

| Metric | Target | Alert Threshold |
|--------|--------|-----------------|
| Build time | < 5 min | > 10 min |
| Test pass rate | 100% | < 99% |
| Mesh routing accuracy | 100% | < 99% |

### 7.3 Rollback Plan

If Phase 1 acceptance criteria fail after deployment:
1. Revert JSON config to v2.x format
2. Disable topology_generator.py port index generation
3. Notify team via GitHub issue

**Note**: This section will be expanded after Phase 1 deployment validation.

## 8. 补充任务（ARCH-012 识别）

以下任务来自 ARCH-012 差距分析审查，在原 IMPL-010 中未覆盖：

### 8.1 Phase 1 补充任务

| 任务 | 对应差距 | 工作量 | 说明 |
|------|---------|--------|------|
| JSON Schema 验证器 | CFG-08 | 2 days | 使用 `nlohmann/json-schema` 库验证配置格式 |
| set_config() 类型检查 | PARAM-01, PARAM-05 | 1 day | 添加 `is_number()`/`contains()` 检查，必填参数校验 |
| 端口索引解析严格性 | DEF-04 | 0.5 day | `std::stoul` + 位置参数检查完整字符串 |
| ModuleGroup 通配符优化 | DEF-01 | 1 day | 添加组存在性检查，优化 O(n²) 匹配 |
| BidirectionalPortAdapter 绑定配置 | DEF-03 | 1 day | 允许 JSON 定义 PE/Net 端口顺序 |
| type_registry.json 自动生成 | DEF-05 | 1 day | 从 C++ `REGISTER_CHSTREAM` 宏自动生成 |

### 8.2 Phase 2 补充任务

| 任务 | 对应差距 | 工作量 | 说明 |
|------|---------|--------|------|
| 端口方向检查 | PORT-01 | 1 day | 防止 req_out → req_out 等方向错误 |
| 端口类型兼容性检查 | PORT-02 | 1 day | 验证 MasterPort/SlavePort Bundle 类型匹配 |
| Bundle 类型验证 | PORT-03 | 1 day | 扩展 ChStreamAdapterFactory 维护 Bundle 注册表 |

### 8.3 Phase 3 补充任务

| 任务 | 对应差距 | 工作量 | 说明 |
|------|---------|--------|------|
| Python TopologyValidator | VALID-01, VALID-02 | 3 days | 连接完整性检查 + BFS 可达性验证 |
| 路由表自动生成器 | SIM-03 | 3 days | 从拓扑图自动计算 XY/自定义路由表 |
| C++ 验证器集成 | VALID-01, VALID-02 | 2 days | C++ 端验证接口，与 Python 验证器结果交叉确认 |

## 9. Version History

| Version | Date | Changes |
|---------|------|---------|
| v2.1 | 2026-04-27 | Added ARCH-012 supplementary tasks: JSON Schema validation, parameter type checking, port validation, topology validation, routing table generation, and ModuleFactory defect fixes (DEF-01/03/04/05). |
| v2.0 | 2026-04-26 | Added v4.0 implementation phases (4-6): hierarchy core, protocol bridge, multi-cluster validation. Added G8-G10 gap implementations. Added CoherenceDomain and ProtocolBridge C++ designs. Added 15 new test cases (TC-16 to TC-30). |
| v1.0 | 2026-04-26 | Initial implementation plan |

