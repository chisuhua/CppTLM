# CppTLM 拓扑生成、维护与管理系统架构设计

**文档编号**: ARCH-010  
**版本**: v1.0  
**日期**: 2026-04-25  
**状态**: 草案  
**依赖**: ARCH-001 (混合架构 v2.1), ARCH-002 (复杂拓扑架构), ARCH-009 (拓扑可视化管线)  
**作者**: CppTLM Architecture Team  

---

## 目录

1. [执行摘要](#1-执行摘要)
2. [术语与约定](#2-术语与约定)
3. [现状分析与差距识别](#3-现状分析与差距识别)
4. [系统架构总览](#4-系统架构总览)
5. [核心组件设计](#5-核心组件设计)
6. [JSON Schema 规范](#6-json-schema-规范)
7. [端口索引规范](#7-端口索引规范)
8. [C++ 模块工厂增强](#8-c-模块工厂增强)
9. [Python 工具链重构](#9-python-工具链重构)
10. [实施路线图](#10-实施路线图)
11. [验证策略](#11-验证策略)
12. [附录](#12-附录)

---

## 1. 执行摘要

### 1.1 目标

本文档定义 CppTLM **拓扑生成、维护与管理系统**（Topology Generation, Maintenance and Management System，简称 **TGMS**）的完整架构设计方案。TGMS 的目标是：

1. **声明式拓扑定义**：通过 JSON Schema 声明网络拓扑结构，支持 Mesh/Ring/Bus/Crossbar/Hierarchical 五种标准拓扑及自定义拓扑
2. **自动生成与验证**：Python 工具链自动生成完整 JSON 配置，包含正确的端口索引、模块参数、布局坐标
3. **参数自动传递**：消除 ModuleFactory 与模块之间的配置断层，JSON 参数自动注入模块实例
4. **拓扑版本管理**：支持拓扑 diff、影响分析、回滚，满足大规模 SoC 设计迭代需求
5. **可视化集成**：与现有拓扑可视化管线（ARCH-009）无缝对接

### 1.2 关键决策

| 决策项 | 选择 | 理由 |
|--------|------|------|
| 端口索引格式 | 纯数字（`.0`, `.1`, `.2`） | 与 C++ `parsePortSpec` + `std::isdigit` 兼容，与 `RouterPort` 枚举一致 |
| 参数传递机制 | `SimObject::set_config()` 虚方法 | 向后兼容，不破坏工厂注册表签名，与 `SimModule::instantiate()` 模式一致 |
| 拓扑生成器框架 | OOP + 注册表模式 | 可扩展，支持自定义拓扑生成器插件 |
| 配置字段统一 | `params` 用于内联参数，`config` 保留用于外部文件引用 | 避免语义混淆，与现有层次化配置兼容 |

### 1.3 范围

**In Scope**:
- 拓扑生成器框架（Python）
- 拓扑验证器（Python）
- ModuleFactory 配置参数传递（C++）
- 端口索引规范（C++ + Python）
- JSON Schema 定义（声明式拓扑）
- 现有工具链重构（topology_generator.py）

**Out of Scope**:
- 路由算法实现（已有 XYRouting）
- Bundle 类型扩展（已有 NoCFlitBundle）
- 仿真核心调度（已有 SimCore/EventQueue）
- 可视化渲染引擎（已有 ARCH-009）

---

## 2. 术语与约定

| 术语 | 定义 |
|------|------|
| **TGMS** | Topology Generation, Maintenance and Management System（本文档定义的系统） |
| **声明式拓扑** | 通过 JSON Schema 描述"想要什么拓扑"，而非命令式代码生成 |
| **端口索引** | 模块端口在连接中的数字标识，如 `router_0_0.1` 表示 router_0_0 的第 1 号端口 |
| **抽象类型** | topology_generator.py 中使用的逻辑类型名，如 `MeshRouter`、`Processor` |
| **映射类型** | CppTLM 注册表中的实际类型名，如 `RouterTLM`、`CPUSim` |
| **子网络 (Subnetwork)** | 层次化拓扑中的独立子系统，可嵌套 |
| **一致性域 (Coherence Domain)** | 共享缓存一致性协议的节点集合 |

---

## 3. 现状分析与差距识别

### 3.1 基础设施现状

Phase 7 完成后，CppTLM 已具备以下能力：

- **RouterTLM**: 5 端口双向路由器（NORTH=0, EAST=1, SOUTH=2, WEST=3, LOCAL=4），六阶段流水线，XY 路由
- **NICTLM**: 4 端口双端口适配器（PE 侧 + Network 侧），packetize/reassemble
- **ChStreamAdapterFactory**: 支持 SinglePort / MultiPort / DualPort / BidirectionalPort 四种适配器
- **ModuleFactory Step 7**: 支持端口索引语法 `module.port_idx`，自动创建 ChStreamPort 向量
- **topology_generator.py**: CPPTLM_TYPE_MAP 类型映射，5 种拓扑生成，DOT/JSON 导出
- **层次化配置**: `configs/example_hier2/` 展示一级嵌套能力

### 3.2 差距分析

#### G1: 路由器拓扑参数未传递（P0）

**问题**: `module_factory.cc` Step 2 只传递 `(name, event_queue)` 给构造函数。

```cpp
// src/core/module_factory.cc:59
SimModule* new_module = module_it->second(name, event_queue);
```

RouterTLM 构造函数签名为：
```cpp
RouterTLM(const std::string& name, EventQueue* eq,
          unsigned node_x = 0, unsigned node_y = 0,
          unsigned mesh_x = DEFAULT_MESH_X,  // = 2
          unsigned mesh_y = DEFAULT_MESH_Y); // = 2
```

所有 RouterTLM 实例默认在 `(0,0)` 位置、2x2 Mesh 中，XY 路由完全错误。

**影响**: Mesh NoC 仿真结果不可信。

---

#### G2: 连接缺少端口索引（P0）

**问题**: `topology_generator.py` 的 `generate_mesh()` 生成连接不带端口索引：

```python
# scripts/topology_generator.py:134-137
self.graph.add_edge(
    f"router_{r}_{c}", f"router_{r}_{c+1}",
    latency=1, bandwidth=100
)
# 生成: {"src": "router_0_0", "dst": "router_0_1"} — 无端口索引
```

`export_json_config()` 同样不处理端口索引。

**影响**: ModuleFactory Step 7c 解析时 `src_idx=0, dst_idx=0`，所有连接绑定到 NORTH 端口。

---

#### G3: BidirectionalPortAdapter 端口索引（文档原论断有误，经代码审查修正）

**原文档论断**: "`isMultiPort("RouterTLM")` 返回 false，BidirectionalPortAdapter 被当作单端口"

**代码审查结果**: **该论断不正确**。

`chstream_adapter_factory.hh:102-109` 的 `registerBidirectionalPortAdapter` 明确设置了 `port_count_[type] = N`：

```cpp
template<typename ModuleT, typename BundleT, std::size_t N>
void registerBidirectionalPortAdapter(const std::string& type) {
    table_[type] = [](SimObject* obj, const nlohmann::json*) { ... };
    port_count_[type] = N;  // ← 明确设置
}
```

而 `isMultiPort()` 的实现（line 59-61）：

```cpp
bool isMultiPort(const std::string& type) const {
    return port_count_.count(type) > 0 && port_count_.at(type) > 1;
}
```

RouterTLM 以 `N=5` 注册，因此 `isMultiPort("RouterTLM")` 返回 **true**。`module_factory.cc:379-380` 不会强制重置 RouterTLM 的端口索引。

**修正后评估**: G3 **不是 P0 阻塞项**。但建议增加端到端 Mesh 测试验证 BidirectionalPortAdapter 的端口绑定在实际仿真中是否完全正确。

---

#### G4: noc_builder.py 端口格式不兼容（P0）→ **已消除**

**审查结果**: `python/noc_builder.py` **不存在于代码库**。原始差距描述基于历史版本，当前代码库中该文件已被移除。

**状态**: ✅ 已消除（脚本不存在，无需修复）

---

#### G5: noc_mesh.py 不可运行（P1）→ **已消除**

**审查结果**: `python/noc_mesh.py` **不存在于代码库**。原始差距描述基于历史版本，当前代码库中该文件已被移除。其功能已被 `topology_generator.py --type mesh` 完全覆盖。

**状态**: ✅ 已消除（脚本不存在，无需修复）

---

### 3.3 差距汇总

| 编号 | 差距 | 优先级 | 文档原判定 | 审查修正 |
|------|------|--------|-----------|----------|
| G1 | 拓扑参数未传递 | P0 | 正确 | 无需修正 |
| G2 | 连接缺少端口索引 | P0 | 正确 | 无需修正 |
| G3 | BidirectionalPortAdapter 索引 | P0 | **错误** | **修正为：非阻塞，但需测试验证** |
| G4 | noc_builder.py 端口格式 | — | 正确 | **已消除（脚本不存在）** |
| G5 | noc_mesh.py 不可运行 | — | 正确 | **已消除（脚本不存在）** |

---

## 4. 系统架构总览

### 4.1 架构全景

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         TGMS — 拓扑生成、维护与管理系统                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                        声明式配置层 (JSON)                            │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │  │
│  │  │ 顶层配置     │  │ 子网络声明   │  │ 全局参数     │  │ 路由策略     │  │  │
│  │  │ chip.json   │  │ subnetworks │  │ global_params│  │ routing     │  │  │
│  │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  │  │
│  │         └─────────────────┴─────────────────┴─────────────────┘        │  │
│  └─────────────────────────────────┬──────────────────────────────────────┘  │
│                                    ▼                                        │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                        拓扑生成器框架 (Python)                        │  │
│  │                                                                      │  │
│  │  ┌─────────────────┐      ┌─────────────────┐      ┌─────────────┐  │  │
│  │  │ TopologyRegistry │─────▶│ TopologyGenerator│─────▶│ JSON Config │  │  │
│  │  │   (注册表)        │      │   (生成器基类)    │      │   (输出)     │  │  │
│  │  └─────────────────┘      └─────────────────┘      └─────────────┘  │  │
│  │           │                        │                                 │  │
│  │           ▼                        ▼                                 │  │
│  │  ┌─────────────────┐      ┌─────────────────┐                       │  │
│  │  │ MeshGenerator   │      │ RingGenerator   │                       │  │
│  │  │ BusGenerator    │      │ CrossbarGenerator│                      │  │
│  │  │ HierarchicalGen │      │ CustomGenerator │ ← 用户自定义扩展       │  │
│  │  └─────────────────┘      └─────────────────┘                       │  │
│  └─────────────────────────────────┬──────────────────────────────────────┘  │
│                                    ▼                                        │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                        拓扑验证层 (Python)                            │  │
│  │                                                                      │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │  │
│  │  │ 连接完整性    │  │ 路由可达性    │  │ 死锁检测      │              │  │
│  │  │ 检查         │  │ 检查 (BFS)   │  │ (循环检测)    │              │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘              │  │
│  └─────────────────────────────────┬──────────────────────────────────────┘  │
│                                    ▼                                        │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                        C++ 仿真引擎                                   │  │
│  │                                                                      │  │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐  │  │
│  │  │ ModuleFactory    │  │ SimObject        │  │ StreamAdapter       │  │  │
│  │  │ · set_config()   │  │ · set_config()   │  │ · 端口索引绑定       │  │  │
│  │  │ · 参数传递        │  │ · 参数读取        │  │ · ChStreamPort 向量  │  │  │
│  │  └─────────────────┘  └─────────────────┘  └─────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 数据流

```
用户输入 ──▶ 声明式 JSON ──▶ TopologyGenerator.generate() ──▶ 原始拓扑图
                                                    │
                                                    ▼
                                            TopologyValidator.validate()
                                                    │
                                          ┌─────────┴─────────┐
                                          ▼                   ▼
                                      验证通过            验证失败
                                          │                   │
                                          ▼                   ▼
                                    export_json_config()   报告错误
                                          │
                                          ▼
                                    C++ ModuleFactory
                                          │
                                          ▼
                                    instantiateAll()
                                    · Step 2: 创建模块
                                    · Step 2.5: set_config()
                                    · Step 7: StreamAdapter 注入
                                    · Step 7c: 端口索引绑定
                                          │
                                          ▼
                                    仿真运行
```

---

## 5. 核心组件设计

### 5.1 TopologyRegistry（拓扑注册表）

**职责**: 统一管理拓扑生成器、模块类型、Bundle 类型、端口特征的注册与查询。

```python
class TopologyRegistry:
    """拓扑注册表 — 单例模式"""
    
    _instance = None
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._generators = {}
            cls._instance._type_info = {}
        return cls._instance
    
    def register_generator(self, name: str, generator_class: Type[TopologyGeneratorBase]):
        """注册拓扑生成器"""
        self._generators[name] = generator_class
    
    def register_type_info(self, abstract_type: str, concrete_type: str, 
                          port_count: int = 1, port_names: Optional[List[str]] = None):
        """注册模块类型信息"""
        self._type_info[abstract_type] = {
            "concrete_type": concrete_type,
            "port_count": port_count,
            "port_names": port_names or [f"port_{i}" for i in range(port_count)]
        }
    
    def get_generator(self, name: str) -> Type[TopologyGeneratorBase]:
        return self._generators.get(name)
    
    def get_type_info(self, abstract_type: str) -> Dict:
        return self._type_info.get(abstract_type)
    
    def list_generators(self) -> List[str]:
        return list(self._generators.keys())
```

**初始化时自动注册内置类型**:

```python
def _register_builtin_types(registry: TopologyRegistry):
    """注册 CppTLM 内置模块类型信息"""
    # 路由器: 5 端口，命名端口
    registry.register_type_info(
        "MeshRouter", "RouterTLM", port_count=5,
        port_names=["NORTH", "EAST", "SOUTH", "WEST", "LOCAL"]
    )
    registry.register_type_info(
        "Router", "RouterTLM", port_count=5,
        port_names=["NORTH", "EAST", "SOUTH", "WEST", "LOCAL"]
    )
    
    # 网络接口: 4 端口（PE 侧 2 个 + Network 侧 2 个）
    registry.register_type_info(
        "NetworkInterface", "NICTLM", port_count=4,
        port_names=["pe_req_in", "pe_resp_out", "net_req_out", "net_resp_in"]
    )
    
    # 单端口模块
    registry.register_type_info("Processor", "CPUSim", port_count=1)
    registry.register_type_info("Cache", "CacheTLM", port_count=1)
    registry.register_type_info("Memory", "MemoryTLM", port_count=1)
    
    # 多端口模块
    registry.register_type_info("Crossbar", "CrossbarTLM", port_count=4)
```

### 5.2 TopologyGeneratorBase（拓扑生成器抽象基类）

```python
from abc import ABC, abstractmethod
from typing import Dict, List, Tuple, Any, Optional

class TopologyGeneratorBase(ABC):
    """拓扑生成器抽象基类
    
    所有具体拓扑生成器必须继承此类并实现 generate() 方法。
    """
    
    def __init__(self, registry: Optional[TopologyRegistry] = None):
        self.registry = registry or TopologyRegistry()
        self.modules: List[Dict] = []
        self.connections: List[Dict] = []
        self.layout: Dict[str, Tuple[float, float]] = {}
    
    @abstractmethod
    def generate(self, **kwargs) -> 'TopologyGeneratorBase':
        """生成拓扑
        
        Returns:
            self (支持链式调用)
        """
        pass
    
    @abstractmethod
    def get_required_params(self) -> List[str]:
        """返回必需参数列表"""
        pass
    
    def get_default_params(self) -> Dict[str, Any]:
        """返回默认参数"""
        return {}
    
    def validate_params(self, **kwargs) -> Tuple[bool, List[str]]:
        """验证参数是否满足要求
        
        Returns:
            (是否通过, 错误信息列表)
        """
        required = self.get_required_params()
        errors = []
        for param in required:
            if param not in kwargs:
                errors.append(f"缺少必需参数: {param}")
        return len(errors) == 0, errors
    
    def to_json_config(self, use_mapping: bool = True) -> Dict:
        """导出为 CppTLM JSON 配置格式"""
        return {
            "name": getattr(self, 'topology_name', 'generated_topology'),
            "version": "3.0",
            "modules": self.modules,
            "connections": self.connections
        }
    
    def to_layout_json(self) -> Dict:
        """导出布局坐标"""
        return {
            "version": "1.0",
            "generator": self.__class__.__name__,
            "nodes": {
                node: {"x": float(coords[0]), "y": float(coords[1])}
                for node, coords in self.layout.items()
            }
        }
```

### 5.3 MeshTopologyGenerator（Mesh 拓扑生成器）

```python
class MeshTopologyGenerator(TopologyGeneratorBase):
    """2D Mesh NoC 拓扑生成器
    
    生成完整的 Mesh 拓扑，包括：
    - Router 节点（带 params: node_x, node_y, mesh_x, mesh_y）
    - NI 节点（带 params: node_id, mesh_x, mesh_y）
    - Processor 节点
    - 带端口索引的连接（Router↔Router, NI↔Router, Processor↔NI）
    """
    
    PORT_MAP = {'NORTH': 0, 'EAST': 1, 'SOUTH': 2, 'WEST': 3, 'LOCAL': 4}
    
    def get_required_params(self) -> List[str]:
        return ['rows', 'cols']
    
    def get_default_params(self) -> Dict[str, Any]:
        return {
            'router_type': 'MeshRouter',
            'ni_type': 'NetworkInterface',
            'processor_type': 'Processor',
            'link_latency': 1
        }
    
    def generate(self, rows: int, cols: int, 
                 router_type: str = 'MeshRouter',
                 ni_type: str = 'NetworkInterface',
                 processor_type: str = 'Processor',
                 link_latency: int = 1,
                 **kwargs) -> 'MeshTopologyGenerator':
        """生成 Mesh 拓扑
        
        Args:
            rows: Mesh 行数
            cols: Mesh 列数
            router_type: Router 抽象类型名
            ni_type: NI 抽象类型名
            processor_type: Processor 抽象类型名
            link_latency: 链路延迟（周期）
        """
        self.topology_name = f"mesh_{rows}x{cols}"
        
        # 1. 创建 Router 节点（带拓扑参数）
        for r in range(rows):
            for c in range(cols):
                router_name = f"router_{r}_{c}"
                self.modules.append({
                    "name": router_name,
                    "type": router_type,
                    "params": {
                        "node_x": c,
                        "node_y": r,
                        "mesh_x": cols,
                        "mesh_y": rows
                    }
                })
                self.layout[router_name] = (c * 100.0, -r * 100.0)
        
        # 2. 创建 NI 节点（带节点 ID 参数）
        for r in range(rows):
            for c in range(cols):
                ni_name = f"ni_{r}_{c}"
                self.modules.append({
                    "name": ni_name,
                    "type": ni_type,
                    "params": {
                        "node_id": r * cols + c,
                        "mesh_x": cols,
                        "mesh_y": rows
                    }
                })
                self.layout[ni_name] = (c * 100.0 + 30.0, -r * 100.0 - 30.0)
        
        # 3. 创建 Processor 节点
        for r in range(rows):
            for c in range(cols):
                proc_name = f"proc_{r}_{c}"
                self.modules.append({
                    "name": proc_name,
                    "type": processor_type
                })
                self.layout[proc_name] = (c * 100.0 + 60.0, -r * 100.0 - 60.0)
        
        # 4. 创建 Router↔Router 连接（带端口索引）
        for r in range(rows):
            for c in range(cols):
                src = f"router_{r}_{c}"
                
                # 东向: src.EAST(1) → dst.WEST(3)
                if c < cols - 1:
                    dst = f"router_{r}_{c+1}"
                    self.connections.append({
                        "src": f"{src}.{self.PORT_MAP['EAST']}",
                        "dst": f"{dst}.{self.PORT_MAP['WEST']}",
                        "latency": link_latency
                    })
                
                # 南向: src.SOUTH(2) → dst.NORTH(0)
                if r < rows - 1:
                    dst = f"router_{r+1}_{c}"
                    self.connections.append({
                        "src": f"{src}.{self.PORT_MAP['SOUTH']}",
                        "dst": f"{dst}.{self.PORT_MAP['NORTH']}",
                        "latency": link_latency
                    })
        
        # 5. 创建 NI↔Router 连接（NI 单端口 ↔ Router LOCAL 端口）
        for r in range(rows):
            for c in range(cols):
                ni_name = f"ni_{r}_{c}"
                router_name = f"router_{r}_{c}"
                # NI → Router.LOCAL
                self.connections.append({
                    "src": ni_name,
                    "dst": f"{router_name}.{self.PORT_MAP['LOCAL']}",
                    "latency": 0
                })
                # Router.LOCAL → NI
                self.connections.append({
                    "src": f"{router_name}.{self.PORT_MAP['LOCAL']}",
                    "dst": ni_name,
                    "latency": 0
                })
        
        # 6. 创建 Processor↔NI 连接（单端口，无需索引）
        for r in range(rows):
            for c in range(cols):
                proc_name = f"proc_{r}_{c}"
                ni_name = f"ni_{r}_{c}"
                self.connections.append({
                    "src": proc_name,
                    "dst": ni_name,
                    "latency": link_latency
                })
                self.connections.append({
                    "src": ni_name,
                    "dst": proc_name,
                    "latency": link_latency
                })
        
        return self
```

### 5.4 TopologyValidator（拓扑验证器）

```python
import networkx as nx
from typing import Dict, List, Set, Tuple

class TopologyValidator:
    """拓扑验证器
    
    对生成的拓扑进行多维验证，确保：
    1. 连接完整性：所有多端口模块的端口已连接
    2. 路由可达性：任意两节点间存在路径
    3. 死锁自由：连接图中无循环依赖（简化检查）
    4. 参数完整性：所有模块的必需参数已设置
    """
    
    def __init__(self, config: Dict, registry: Optional[TopologyRegistry] = None):
        self.config = config
        self.registry = registry or TopologyRegistry()
        self.modules = {m["name"]: m for m in config.get("modules", [])}
        self.connections = config.get("connections", [])
        self._build_graph()
    
    def _build_graph(self):
        """构建有向图用于分析"""
        self.G = nx.DiGraph()
        for conn in self.connections:
            src = conn["src"].split(".")[0]
            dst = conn["dst"].split(".")[0]
            self.G.add_edge(src, dst)
    
    def validate_all(self) -> Dict:
        """执行全部验证"""
        return {
            "parameter_completeness": self._check_parameters(),
            "connection_completeness": self._check_port_connections(),
            "reachability": self._check_reachability(),
            "deadlock_risk": self._check_deadlock_risk(),
            "hotspot_analysis": self._analyze_hotspots()
        }
    
    def _check_parameters(self) -> Dict:
        """检查模块参数完整性"""
        issues = []
        for name, module in self.modules.items():
            mod_type = module.get("type", "")
            type_info = self.registry.get_type_info(mod_type)
            
            # Router 类型必须包含 node_x, node_y, mesh_x, mesh_y
            if mod_type in ["RouterTLM", "MeshRouter", "Router"]:
                params = module.get("params", {})
                for p in ["node_x", "node_y", "mesh_x", "mesh_y"]:
                    if p not in params:
                        issues.append(f"模块 '{name}' (类型 {mod_type}) 缺少必需参数 '{p}'")
            
            # NI 类型必须包含 node_id
            if mod_type in ["NICTLM", "NetworkInterface"]:
                params = module.get("params", {})
                if "node_id" not in params:
                    issues.append(f"模块 '{name}' (类型 {mod_type}) 缺少必需参数 'node_id'")
        
        return {"valid": len(issues) == 0, "issues": issues}
    
    def _check_port_connections(self) -> Dict:
        """检查多端口模块的端口连接情况"""
        issues = []
        
        # 统计每个模块端口的连接数
        port_connections: Dict[str, Dict[int, int]] = {}
        for conn in self.connections:
            for spec in [conn["src"], conn["dst"]]:
                parts = spec.split(".")
                mod_name = parts[0]
                port_idx = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 0
                
                if mod_name not in port_connections:
                    port_connections[mod_name] = {}
                port_connections[mod_name][port_idx] = port_connections[mod_name].get(port_idx, 0) + 1
        
        # 检查每个多端口模块
        for name, module in self.modules.items():
            mod_type = module.get("type", "")
            type_info = self.registry.get_type_info(mod_type)
            if not type_info:
                continue
            
            port_count = type_info.get("port_count", 1)
            if port_count <= 1:
                continue  # 单端口模块无需检查
            
            connected_ports = set(port_connections.get(name, {}).keys())
            expected_ports = set(range(port_count))
            
            missing = expected_ports - connected_ports
            if missing:
                issues.append(
                    f"模块 '{name}' (类型 {mod_type}, {port_count} 端口) "
                    f"缺少端口连接: {sorted(missing)}"
                )
        
        return {"valid": len(issues) == 0, "issues": issues}
    
    def _check_reachability(self) -> Dict:
        """检查所有节点对是否可达"""
        unreachable = []
        nodes = list(self.modules.keys())
        
        for i, src in enumerate(nodes):
            for dst in nodes[i+1:]:
                # 检查双向可达
                if not (nx.has_path(self.G, src, dst) and nx.has_path(self.G, dst, src)):
                    unreachable.append((src, dst))
        
        return {"valid": len(unreachable) == 0, "unreachable": unreachable}
    
    def _check_deadlock_risk(self) -> Dict:
        """检查死锁风险（简化版：检测循环依赖）"""
        cycles = list(nx.simple_cycles(self.G))
        # 过滤掉 2 节点循环（双向连接是正常行为）
        real_cycles = [c for c in cycles if len(c) > 2]
        
        return {"valid": len(real_cycles) == 0, "cycles": real_cycles}
    
    def _analyze_hotspots(self) -> Dict:
        """热点分析：统计每个路由器的连接负载"""
        router_load = {}
        for conn in self.connections:
            for spec in [conn["src"], conn["dst"]]:
                mod_name = spec.split(".")[0]
                mod_type = self.modules.get(mod_name, {}).get("type", "")
                if "Router" in mod_type or "router" in mod_name:
                    router_load[mod_name] = router_load.get(mod_name, 0) + 1
        
        return router_load
```

### 5.5 TopologyDiffEngine（拓扑差异引擎）

```python
import json
from typing import Dict, List, Tuple
from difflib import unified_diff

class TopologyDiffEngine:
    """拓扑差异引擎
    
    比较两个拓扑配置的差异，分析影响范围。
    """
    
    def __init__(self, old_config: Dict, new_config: Dict):
        self.old = old_config
        self.new = new_config
    
    def diff(self) -> Dict:
        """计算差异"""
        old_modules = {m["name"]: m for m in self.old.get("modules", [])}
        new_modules = {m["name"]: m for m in self.new.get("modules", [])}
        
        old_conns = set(self._conn_key(c) for c in self.old.get("connections", []))
        new_conns = set(self._conn_key(c) for c in self.new.get("connections", []))
        
        return {
            "added_modules": [n for n in new_modules if n not in old_modules],
            "removed_modules": [n for n in old_modules if n not in new_modules],
            "modified_modules": self._find_modified_modules(old_modules, new_modules),
            "added_connections": [c for c in self.new.get("connections", []) 
                                  if self._conn_key(c) not in old_conns],
            "removed_connections": [c for c in self.old.get("connections", []) 
                                    if self._conn_key(c) not in new_conns],
        }
    
    def _conn_key(self, conn: Dict) -> str:
        """生成连接的唯一键"""
        return f"{conn.get('src')}->{conn.get('dst')}"
    
    def _find_modified_modules(self, old: Dict, new: Dict) -> List[Dict]:
        """找出参数被修改的模块"""
        modified = []
        for name in old:
            if name in new and old[name] != new[name]:
                modified.append({
                    "name": name,
                    "old": old[name],
                    "new": new[name]
                })
        return modified
    
    def analyze_impact(self, diff: Dict) -> Dict:
        """分析变更影响范围"""
        impact = {
            "routing_affected": [],
            "connectivity_affected": [],
            "performance_affected": []
        }
        
        # 新增/删除模块影响路由
        for mod_name in diff["added_modules"] + diff["removed_modules"]:
            if "router" in mod_name or "ni" in mod_name:
                impact["routing_affected"].append(mod_name)
        
        # 连接变更影响连通性
        if diff["added_connections"] or diff["removed_connections"]:
            impact["connectivity_affected"].extend([
                c["src"].split(".")[0] for c in diff["added_connections"]
            ])
        
        # 参数修改影响性能
        for mod in diff["modified_modules"]:
            if "params" in mod["new"]:
                impact["performance_affected"].append(mod["name"])
        
        return impact
```

---

## 6. JSON Schema 规范

### 6.1 声明式拓扑 Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "CppTLM Topology Configuration",
  "description": "声明式拓扑配置 — 用于 TGMS 自动生成完整仿真配置",
  "type": "object",
  "required": ["name", "version"],
  "properties": {
    "name": {
      "type": "string",
      "description": "拓扑名称"
    },
    "version": {
      "type": "string",
      "enum": ["3.0"],
      "description": "配置格式版本"
    },
    "subnetworks": {
      "type": "object",
      "description": "子网络声明 — 自动生成模块和连接",
      "additionalProperties": {
        "type": "object",
        "required": ["type"],
        "properties": {
          "type": {
            "type": "string",
            "enum": ["mesh", "ring", "bus", "crossbar", "hierarchical"]
          },
          "rows": { "type": "integer", "minimum": 1 },
          "cols": { "type": "integer", "minimum": 1 },
          "router_type": { "type": "string", "default": "MeshRouter" },
          "router_params": { "type": "object" },
          "node_type": { "type": "string", "default": "NetworkInterface" },
          "node_params": { "type": "object" },
          "leaf_type": { "type": "string", "default": "Processor" },
          "leaf_count": { "type": "integer" }
        }
      }
    },
    "modules": {
      "type": "array",
      "description": "显式声明的模块（与子网络生成互补）",
      "items": {
        "type": "object",
        "required": ["name", "type"],
        "properties": {
          "name": { "type": "string" },
          "type": { "type": "string" },
          "params": {
            "type": "object",
            "description": "模块参数 — 通过 set_config() 传递"
          },
          "config": {
            "type": "string",
            "description": "外部配置文件路径（用于 SimModule 内部配置）"
          },
          "layout": {
            "type": "object",
            "properties": {
              "x": { "type": "number" },
              "y": { "type": "number" }
            }
          }
        }
      }
    },
    "connections": {
      "type": "array",
      "description": "显式声明的连接（与子网络生成互补）",
      "items": {
        "type": "object",
        "required": ["src", "dst"],
        "properties": {
          "src": {
            "type": "string",
            "pattern": "^[a-zA-Z_][a-zA-Z0-9_]*(\\.[0-9]+)?$",
            "description": "源模块名，可选端口索引（如 router_0_0.1）"
          },
          "dst": {
            "type": "string",
            "pattern": "^[a-zA-Z_][a-zA-Z0-9_]*(\\.[0-9]+)?$",
            "description": "目标模块名，可选端口索引"
          },
          "latency": { "type": "integer", "minimum": 0, "default": 1 },
          "bandwidth": { "type": "integer", "minimum": 1, "default": 100 },
          "exclude": {
            "type": "array",
            "items": { "type": "string" }
          }
        }
      }
    },
    "inter_subnetwork_connections": {
      "type": "array",
      "description": "子网络间连接",
      "items": {
        "type": "object",
        "required": ["src", "dst"],
        "properties": {
          "src": { "type": "string" },
          "dst": { "type": "string" },
          "latency": { "type": "integer" }
        }
      }
    },
    "routing": {
      "type": "object",
      "properties": {
        "algorithm": {
          "type": "string",
          "enum": ["xy", "west_first", "adaptive", "custom"],
          "default": "xy"
        },
        "fallback": { "type": "string" },
        "vc_count": { "type": "integer", "minimum": 1, "default": 4 }
      }
    },
    "global_params": {
      "type": "object",
      "description": "全局默认参数",
      "properties": {
        "link_latency": { "type": "integer", "default": 1 },
        "router_latency": { "type": "integer", "default": 1 },
        "flit_width": { "type": "integer", "default": 128 }
      }
    }
  }
}
```

### 6.2 完整 4x4 Mesh 配置示例

```json
{
  "name": "mesh_4x4",
  "version": "3.0",
  "modules": [
    {"name": "ni_0_0", "type": "NetworkInterface", "params": {"node_id": 0, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_0_1", "type": "NetworkInterface", "params": {"node_id": 1, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_0_2", "type": "NetworkInterface", "params": {"node_id": 2, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_0_3", "type": "NetworkInterface", "params": {"node_id": 3, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_1_0", "type": "NetworkInterface", "params": {"node_id": 4, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_1_1", "type": "NetworkInterface", "params": {"node_id": 5, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_1_2", "type": "NetworkInterface", "params": {"node_id": 6, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_1_3", "type": "NetworkInterface", "params": {"node_id": 7, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_2_0", "type": "NetworkInterface", "params": {"node_id": 8, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_2_1", "type": "NetworkInterface", "params": {"node_id": 9, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_2_2", "type": "NetworkInterface", "params": {"node_id": 10, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_2_3", "type": "NetworkInterface", "params": {"node_id": 11, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_3_0", "type": "NetworkInterface", "params": {"node_id": 12, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_3_1", "type": "NetworkInterface", "params": {"node_id": 13, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_3_2", "type": "NetworkInterface", "params": {"node_id": 14, "mesh_x": 4, "mesh_y": 4}},
    {"name": "ni_3_3", "type": "NetworkInterface", "params": {"node_id": 15, "mesh_x": 4, "mesh_y": 4}},

    {"name": "router_0_0", "type": "MeshRouter", "params": {"node_x": 0, "node_y": 0, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_0_1", "type": "MeshRouter", "params": {"node_x": 1, "node_y": 0, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_0_2", "type": "MeshRouter", "params": {"node_x": 2, "node_y": 0, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_0_3", "type": "MeshRouter", "params": {"node_x": 3, "node_y": 0, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_1_0", "type": "MeshRouter", "params": {"node_x": 0, "node_y": 1, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_1_1", "type": "MeshRouter", "params": {"node_x": 1, "node_y": 1, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_1_2", "type": "MeshRouter", "params": {"node_x": 2, "node_y": 1, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_1_3", "type": "MeshRouter", "params": {"node_x": 3, "node_y": 1, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_2_0", "type": "MeshRouter", "params": {"node_x": 0, "node_y": 2, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_2_1", "type": "MeshRouter", "params": {"node_x": 1, "node_y": 2, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_2_2", "type": "MeshRouter", "params": {"node_x": 2, "node_y": 2, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_2_3", "type": "MeshRouter", "params": {"node_x": 3, "node_y": 2, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_3_0", "type": "MeshRouter", "params": {"node_x": 0, "node_y": 3, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_3_1", "type": "MeshRouter", "params": {"node_x": 1, "node_y": 3, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_3_2", "type": "MeshRouter", "params": {"node_x": 2, "node_y": 3, "mesh_x": 4, "mesh_y": 4}},
    {"name": "router_3_3", "type": "MeshRouter", "params": {"node_x": 3, "node_y": 3, "mesh_x": 4, "mesh_y": 4}},

    {"name": "proc_0_0", "type": "Processor"},
    {"name": "proc_0_1", "type": "Processor"},
    {"name": "proc_0_2", "type": "Processor"},
    {"name": "proc_0_3", "type": "Processor"},
    {"name": "proc_1_0", "type": "Processor"},
    {"name": "proc_1_1", "type": "Processor"},
    {"name": "proc_1_2", "type": "Processor"},
    {"name": "proc_1_3", "type": "Processor"},
    {"name": "proc_2_0", "type": "Processor"},
    {"name": "proc_2_1", "type": "Processor"},
    {"name": "proc_2_2", "type": "Processor"},
    {"name": "proc_2_3", "type": "Processor"},
    {"name": "proc_3_0", "type": "Processor"},
    {"name": "proc_3_1", "type": "Processor"},
    {"name": "proc_3_2", "type": "Processor"},
    {"name": "proc_3_3", "type": "Processor"},

    {"name": "mem_0", "type": "Memory"},
    {"name": "mem_1", "type": "Memory"}
  ],

  "groups": {
    "nis": ["ni_*"],
    "routers": ["router_*"],
    "processors": ["proc_*"]
  },

  "connections": [
    {"src": "ni_0_0", "dst": "router_0_0.4", "latency": 0},
    {"src": "router_0_0.4", "dst": "ni_0_0", "latency": 0},
    {"src": "ni_0_1", "dst": "router_0_1.4", "latency": 0},
    {"src": "router_0_1.4", "dst": "ni_0_1", "latency": 0},
    {"src": "ni_0_2", "dst": "router_0_2.4", "latency": 0},
    {"src": "router_0_2.4", "dst": "ni_0_2", "latency": 0},
    {"src": "ni_0_3", "dst": "router_0_3.4", "latency": 0},
    {"src": "router_0_3.4", "dst": "ni_0_3", "latency": 0},
    {"src": "ni_1_0", "dst": "router_1_0.4", "latency": 0},
    {"src": "router_1_0.4", "dst": "ni_1_0", "latency": 0},
    {"src": "ni_1_1", "dst": "router_1_1.4", "latency": 0},
    {"src": "router_1_1.4", "dst": "ni_1_1", "latency": 0},
    {"src": "ni_1_2", "dst": "router_1_2.4", "latency": 0},
    {"src": "router_1_2.4", "dst": "ni_1_2", "latency": 0},
    {"src": "ni_1_3", "dst": "router_1_3.4", "latency": 0},
    {"src": "router_1_3.4", "dst": "ni_1_3", "latency": 0},
    {"src": "ni_2_0", "dst": "router_2_0.4", "latency": 0},
    {"src": "router_2_0.4", "dst": "ni_2_0", "latency": 0},
    {"src": "ni_2_1", "dst": "router_2_1.4", "latency": 0},
    {"src": "router_2_1.4", "dst": "ni_2_1", "latency": 0},
    {"src": "ni_2_2", "dst": "router_2_2.4", "latency": 0},
    {"src": "router_2_2.4", "dst": "ni_2_2", "latency": 0},
    {"src": "ni_2_3", "dst": "router_2_3.4", "latency": 0},
    {"src": "router_2_3.4", "dst": "ni_2_3", "latency": 0},
    {"src": "ni_3_0", "dst": "router_3_0.4", "latency": 0},
    {"src": "router_3_0.4", "dst": "ni_3_0", "latency": 0},
    {"src": "ni_3_1", "dst": "router_3_1.4", "latency": 0},
    {"src": "router_3_1.4", "dst": "ni_3_1", "latency": 0},
    {"src": "ni_3_2", "dst": "router_3_2.4", "latency": 0},
    {"src": "router_3_2.4", "dst": "ni_3_2", "latency": 0},
    {"src": "ni_3_3", "dst": "router_3_3.4", "latency": 0},
    {"src": "router_3_3.4", "dst": "ni_3_3", "latency": 0},

    {"src": "router_0_0.1", "dst": "router_0_1.3", "latency": 1},
    {"src": "router_0_0.2", "dst": "router_1_0.0", "latency": 1},
    {"src": "router_0_1.1", "dst": "router_0_2.3", "latency": 1},
    {"src": "router_0_1.2", "dst": "router_1_1.0", "latency": 1},
    {"src": "router_0_2.1", "dst": "router_0_3.3", "latency": 1},
    {"src": "router_0_2.2", "dst": "router_1_2.0", "latency": 1},
    {"src": "router_0_3.2", "dst": "router_1_3.0", "latency": 1},
    {"src": "router_1_0.1", "dst": "router_1_1.3", "latency": 1},
    {"src": "router_1_0.2", "dst": "router_2_0.0", "latency": 1},
    {"src": "router_1_1.1", "dst": "router_1_2.3", "latency": 1},
    {"src": "router_1_1.2", "dst": "router_2_1.0", "latency": 1},
    {"src": "router_1_2.1", "dst": "router_1_3.3", "latency": 1},
    {"src": "router_1_2.2", "dst": "router_2_2.0", "latency": 1},
    {"src": "router_1_3.2", "dst": "router_2_3.0", "latency": 1},
    {"src": "router_2_0.1", "dst": "router_2_1.3", "latency": 1},
    {"src": "router_2_0.2", "dst": "router_3_0.0", "latency": 1},
    {"src": "router_2_1.1", "dst": "router_2_2.3", "latency": 1},
    {"src": "router_2_1.2", "dst": "router_3_1.0", "latency": 1},
    {"src": "router_2_2.1", "dst": "router_2_3.3", "latency": 1},
    {"src": "router_2_2.2", "dst": "router_3_2.0", "latency": 1},
    {"src": "router_2_3.2", "dst": "router_3_3.0", "latency": 1},
    {"src": "router_3_0.1", "dst": "router_3_1.3", "latency": 1},
    {"src": "router_3_1.1", "dst": "router_3_2.3", "latency": 1},
    {"src": "router_3_2.1", "dst": "router_3_3.3", "latency": 1},

    {"src": "proc_0_0", "dst": "ni_0_0", "latency": 1},
    {"src": "ni_0_0", "dst": "proc_0_0", "latency": 1},
    {"src": "proc_0_1", "dst": "ni_0_1", "latency": 1},
    {"src": "ni_0_1", "dst": "proc_0_1", "latency": 1},
    {"src": "proc_0_2", "dst": "ni_0_2", "latency": 1},
    {"src": "ni_0_2", "dst": "proc_0_2", "latency": 1},
    {"src": "proc_0_3", "dst": "ni_0_3", "latency": 1},
    {"src": "ni_0_3", "dst": "proc_0_3", "latency": 1},
    {"src": "proc_1_0", "dst": "ni_1_0", "latency": 1},
    {"src": "ni_1_0", "dst": "proc_1_0", "latency": 1},
    {"src": "proc_1_1", "dst": "ni_1_1", "latency": 1},
    {"src": "ni_1_1", "dst": "proc_1_1", "latency": 1},
    {"src": "proc_1_2", "dst": "ni_1_2", "latency": 1},
    {"src": "ni_1_2", "dst": "proc_1_2", "latency": 1},
    {"src": "proc_1_3", "dst": "ni_1_3", "latency": 1},
    {"src": "ni_1_3", "dst": "proc_1_3", "latency": 1},
    {"src": "proc_2_0", "dst": "ni_2_0", "latency": 1},
    {"src": "ni_2_0", "dst": "proc_2_0", "latency": 1},
    {"src": "proc_2_1", "dst": "ni_2_1", "latency": 1},
    {"src": "ni_2_1", "dst": "proc_2_1", "latency": 1},
    {"src": "proc_2_2", "dst": "ni_2_2", "latency": 1},
    {"src": "ni_2_2", "dst": "proc_2_2", "latency": 1},
    {"src": "proc_2_3", "dst": "ni_2_3", "latency": 1},
    {"src": "ni_2_3", "dst": "proc_2_3", "latency": 1},
    {"src": "proc_3_0", "dst": "ni_3_0", "latency": 1},
    {"src": "ni_3_0", "dst": "proc_3_0", "latency": 1},
    {"src": "proc_3_1", "dst": "ni_3_1", "latency": 1},
    {"src": "ni_3_1", "dst": "proc_3_1", "latency": 1},
    {"src": "proc_3_2", "dst": "ni_3_2", "latency": 1},
    {"src": "ni_3_2", "dst": "proc_3_2", "latency": 1},
    {"src": "proc_3_3", "dst": "ni_3_3", "latency": 1},
    {"src": "ni_3_3", "dst": "proc_3_3", "latency": 1},

    {"src": "router_0_0", "dst": "mem_0", "latency": 2},
    {"src": "router_3_3", "dst": "mem_1", "latency": 2}
  ],

  "routing": {
    "algorithm": "xy",
    "vc_count": 4
  },

  "global_params": {
    "link_latency": 1,
    "router_latency": 1,
    "flit_width": 128
  }
}
```

---

## 7. 端口索引规范

### 7.1 端口索引语法

**格式**: `module_name.port_index`

- `module_name`: 模块实例名称（如 `router_0_0`）
- `port_index`: 非负整数，表示端口编号（如 `0`, `1`, `2`）
- 分隔符: `.`（点号）

**示例**:
- `router_0_0.1` — router_0_0 的端口 1（EAST）
- `ni_0_0` — ni_0_0 的默认端口 0（单端口模块可省略索引）

### 7.2 端口编号约定

| 模块类型 | 端口数 | 端口编号 | 语义 |
|----------|--------|----------|------|
| RouterTLM | 5 | 0 | NORTH |
| | | 1 | EAST |
| | | 2 | SOUTH |
| | | 3 | WEST |
| | | 4 | LOCAL |
| NICTLM | 2 (port groups) | 0 | **PE 侧**（连接 Cache/CPU），组 0：pe_req_in, pe_resp_out, resp_in, req_out |
| | | 1 | **Network 侧**（连接 Router），组 1：net_req_out, net_resp_in, resp_in, req_out |

> **连接说明**: NICTLM 使用 DualPortStreamAdapter，`port_count_ = 2` 表示**2 个端口组**（而非 4 个独立端口）。NI → Router 连接使用端口索引 `1`（Network side → Router.LOCAL）。Processor → NI 连接使用端口索引 `0`（PE side，默认）。每组内部有 4 个 ChStreamPort，但连接索引只区分组。

| CrossbarTLM | 4 | 0-3 | 输入端口 0-3 |
| CacheTLM | 1 | 0 | 唯一端口 |
| MemoryTLM | 1 | 0 | 唯一端口 |
| CPUSim | 1 | 0 | 唯一端口 |

### 7.3 C++ 解析规则

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

### 7.4 废弃命名端口格式

**禁止格式**（与 C++ 解析器不兼容）:
- `router.E_out` / `router.W_in`
- `router.NORTH` / `router.SOUTH`

**原因**: `std::isdigit('E')` 为 false，这些格式会被解析为默认端口 0，导致连接错误。

---

## 8. C++ 模块工厂增强

### 8.1 SimObject::set_config() 设计

在 `include/core/sim_object.hh` 中添加虚方法：

```cpp
class SimObject {
protected:
    std::string name;
    EventQueue* event_queue;
    std::unique_ptr<PortManager> port_manager;
    LayoutInfo layout;
    json config_params_;  // ← 新增：存储 JSON 配置参数
    
    // ... 其余成员不变

public:
    SimObject(const std::string& n, EventQueue* eq) 
        : name(n), event_queue(eq) {}
    
    virtual ~SimObject() = default;

    // ========== 新增：配置参数接口 ==========
    
    /**
     * @brief 设置模块配置参数
     * @param cfg JSON 配置对象
     * 
     * 默认实现仅存储配置，子类可重写以在设置时执行初始化逻辑。
     */
    virtual void set_config(const json& cfg) { 
        config_params_ = cfg; 
    }
    
    /**
     * @brief 获取模块配置参数
     * @return JSON 配置对象引用
     */
    virtual const json& get_config() const { 
        return config_params_; 
    }
    
    /**
     * @brief 检查是否已设置配置参数
     * @return true 如果配置非空
     */
    bool has_config() const {
        return !config_params_.is_null() && !config_params_.empty();
    }

    // ... 其余方法不变
};
```

### 8.2 ModuleFactory Step 2.5 实现

在 `src/core/module_factory.cc` Step 2 和 Step 3 之间插入 Step 2.5：

```cpp
    // ========================
    // 2. 创建所有模块实例
    // ========================
    // ... 现有代码不变 ...

    // ========================
    // 2.5 传递 JSON 配置参数到模块（新增）
    // ========================
    for (auto& mod : final_config["modules"]) {
        if (!mod.contains("name")) continue;
        std::string name = mod["name"];
        
        auto it = object_instances.find(name);
        if (it != object_instances.end() && mod.contains("params")) {
            it->second->set_config(mod["params"]);
            DPRINTF(MODULE, "[CONFIG] Set params for %s: %s\n", 
                    name.c_str(), mod["params"].dump().c_str());
        }
    }

    // ========================
    // 3. 解析 groups
    // ========================
    // ... 现有代码不变 ...
```

### 8.3 RouterTLM 读取配置

在 `src/tlm/router_tlm.cc` 的 `init()` 或构造函数中读取参数：

```cpp
void RouterTLM::init() {
    // 从 JSON 配置读取拓扑参数
    if (has_config()) {
        const auto& cfg = get_config();
        if (cfg.contains("node_x")) node_x_ = cfg["node_x"].get<unsigned>();
        if (cfg.contains("node_y")) node_y_ = cfg["node_y"].get<unsigned>();
        if (cfg.contains("mesh_x")) mesh_x_ = cfg["mesh_x"].get<unsigned>();
        if (cfg.contains("mesh_y")) mesh_y_ = cfg["mesh_y"].get<unsigned>();
        
        DPRINTF(MODULE, "[RouterTLM] %s configured at (%u,%u) in %ux%u mesh\n",
                getName().c_str(), node_x_, node_y_, mesh_x_, mesh_y_);
    }
    
    // 初始化路由算法
    if (!routing_algo_) {
        routing_algo_ = new XYRouting();
    }
    
    // ... 其余初始化逻辑 ...
    
    initialized_ = true;
}
```

### 8.4 NICTLM 读取配置

```cpp
void NICTLM::init() {
    if (has_config()) {
        const auto& cfg = get_config();
        if (cfg.contains("node_id")) node_id_ = cfg["node_id"].get<uint32_t>();
        if (cfg.contains("mesh_x")) mesh_x_ = cfg["mesh_x"].get<uint32_t>();
        if (cfg.contains("mesh_y")) mesh_y_ = cfg["mesh_y"].get<uint32_t>();
    }
    
    // ... 其余初始化逻辑 ...
    
    initialized_ = true;
}
```

### 8.5 向后兼容性说明

- `set_config()` 默认实现为空操作，不影响现有模块
- `has_config()` 返回 false 时，模块使用构造函数默认值
- 现有 JSON 配置（无 `params` 字段）继续正常工作
- `params` 与 `config` 字段共存：
  - `params`: 内联参数，通过 `set_config()` 传递
  - `config`: 外部文件路径，通过 `SimModule::instantiate()` 处理（已有机制）

---

## 9. Python 工具链重构

### 9.1 topology_generator.py 重构

将现有 `TopologyGenerator` 类重构为基于注册表的架构：

```python
#!/usr/bin/env python3
"""
topology_generator.py — CppTLM 拓扑配置生成器 v3.0

重构内容:
1. 引入 TopologyRegistry 注册表
2. Mesh 生成器自动包含端口索引和 params
3. 所有生成器统一继承 TopologyGeneratorBase
4. 内置 TopologyValidator 验证

@author CppTLM Development Team
@date 2026-04-25
"""

import argparse
import json
import sys
from typing import Dict, List, Tuple, Optional, Any, Type

# ... (imports unchanged) ...

# ============================================================================
# 内置类型映射（向后兼容）
# ============================================================================

CPPTLM_TYPE_MAP: Dict[str, str] = {
    'Processor': 'CPUSim',
    'Cache': 'CacheTLM',
    'Memory': 'MemoryTLM',
    'Directory': 'DirectoryCtrl',
    'NetworkInterface': 'NICTLM',
    'Router': 'RouterTLM',
    'MeshRouter': 'RouterTLM',
    'Bus': 'BusSim',
    'Crossbar': 'CrossbarTLM',
    'TrafficGen': 'TrafficGenTLM',
    'Port': 'Port',
}

# ============================================================================
# 主程序入口
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="CppTLM Topology Generator v3.0",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 生成 4x4 Mesh 拓扑（带端口索引和 params）
  python3 topology_generator.py --type mesh --size 4x4 -o configs/mesh.json

  # 生成并验证
  python3 topology_generator.py --type mesh --size 2x2 -o configs/mesh.json --validate
        """
    )
    
    # ... (参数定义与现有版本一致，新增 --validate 参数) ...
    
    parser.add_argument(
        "--validate", "-v",
        action="store_true",
        help="生成后执行拓扑验证"
    )
    
    args = parser.parse_args()
    
    # 初始化注册表
    registry = TopologyRegistry()
    _register_builtin_types(registry)
    
    # 创建生成器
    name = args.name or f"{args.type}_topology"
    
    if args.type == "mesh":
        rows, cols = map(int, args.size.split('x'))
        gen = MeshTopologyGenerator(registry)
        gen.generate(rows=rows, cols=cols)
    # ... (其他拓扑类型类似) ...
    
    # 验证（如果请求）
    if args.validate:
        config = gen.to_json_config(use_mapping=not args.no_mapping)
        validator = TopologyValidator(config, registry)
        result = validator.validate_all()
        
        all_valid = all(r["valid"] for r in result.values() if isinstance(r, dict) and "valid" in r)
        if not all_valid:
            print("[ERROR] 拓扑验证失败:")
            for check_name, check_result in result.items():
                if isinstance(check_result, dict) and not check_result.get("valid", True):
                    print(f"  {check_name}:")
                    for issue in check_result.get("issues", []):
                        print(f"    - {issue}")
            sys.exit(1)
        print("[OK] 拓扑验证通过")
    
    # 导出配置
    config = gen.to_json_config(use_mapping=not args.no_mapping)
    # ... (保存文件) ...

if __name__ == "__main__":
    main()
```

### 9.2 noc_builder.py 重写

```python
# python/noc_builder.py — 兼容 CppTLM v3.0 端口索引格式
from typing import List, Dict, Tuple
from topology_generator import TopologyRegistry, MeshTopologyGenerator

class MeshTopology:
    """兼容层 — 基于 MeshTopologyGenerator 的轻量级包装"""
    
    def __init__(self, rows: int, cols: int):
        self.rows = rows
        self.cols = cols
        self._gen = MeshTopologyGenerator()
        self._gen.generate(rows=rows, cols=cols)
    
    def generate_nodes(self) -> List[Dict]:
        return self._gen.modules
    
    def generate_connections(self) -> List[Dict]:
        return self._gen.connections
    
    def get_coordinates(self) -> Dict[str, Tuple[float, float]]:
        return self._gen.layout
    
    def to_json_config(self) -> Dict:
        return self._gen.to_json_config()


def build_gemsc_config(topology: MeshTopology, 
                       terminal_nodes: List[Dict], 
                       plugin_list: List[str]) -> Dict:
    """构建完整配置"""
    config = topology.to_json_config()
    config["plugin"] = plugin_list
    config["modules"].extend(terminal_nodes)
    return config


def generate_dot_file(config: Dict, output_file: str):
    """生成 Graphviz DOT 文件"""
    # ... (简化实现) ...
    pass


# 废弃类警告（保留以提示用户迁移）
class Topology:
    """已废弃 — 请直接使用 MeshTopologyGenerator"""
    def __init__(self):
        raise DeprecationWarning(
            "Topology 基类已废弃。请使用 topology_generator.py 中的 "
            "MeshTopologyGenerator 或 TopologyGeneratorBase。"
        )
```

### 9.3 noc_mesh.py 删除

`noc_mesh.py` 引用不存在的类（`VcRouter`、`TerminalNode`、`build_mesh_connections`），且功能已被 `topology_generator.py --type mesh` 完全覆盖。

**决策**: 删除 `python/noc_mesh.py`，在 `python/README.md` 中说明迁移路径。

---

## 10. 实施路线图

### Phase 1: P0 修复（第 1-2 周）

| 任务 | 文件 | 工作量 | 验收标准 |
|------|------|--------|----------|
| 添加 SimObject::set_config/get_config | `include/core/sim_object.hh` | ~15 行 | 编译通过，现有测试不受影响 |
| ModuleFactory Step 2.5 参数传递 | `src/core/module_factory.cc` | ~15 行 | 带 params 的模块正确接收配置 |
| RouterTLM::init() 读取配置 | `src/tlm/router_tlm.cc` | ~10 行 | XY 路由使用正确坐标 |
| NICTLM::init() 读取配置 | `src/tlm/nic_tlm.cc` | ~8 行 | node_id 从配置读取 |
| MeshTopologyGenerator 实现 | 新建/重构 | ~150 行 | 生成带端口索引和 params 的配置 |
| export_json_config 端口索引支持 | `scripts/topology_generator.py` | ~20 行 | 连接包含 `.N` 格式端口索引 |
| 端口索引规范文档 | 本文档 | — | 团队评审通过 |

### Phase 2: 验证与测试（第 2-3 周）

| 任务 | 工作量 | 验收标准 |
|------|--------|----------|
| 编写 mesh_2x2.json 端到端测试 | ~100 行 | 仿真成功运行，flit 正确路由 |
| TopologyValidator 实现 | ~200 行 | 能检测参数缺失、端口未连接、不可达节点 |
| 修复 noc_builder.py 端口格式 | ~15 行 | 使用数字端口索引 |
| 删除 noc_mesh.py | — | 文件移除，README 更新 |
| 4x4 Mesh 路由正确性验证 | — | 从 (0,0) 到 (3,3) 的 flit 经过 6 跳 |

### Phase 3: 高级特性（第 4-6 周）

| 任务 | 工作量 | 优先级 |
|------|--------|--------|
| TopologyDiffEngine 实现 | ~150 行 | P2 |
| 声明式 JSON Schema 子网络支持 | ~100 行 | P2 |
| 层次化拓扑生成器 | ~200 行 | P2 |
| 与 ARCH-009 可视化管线集成 | ~50 行 | P2 |

---

## 11. 验证策略

### 11.1 单元测试

```cpp
// test/test_topology_config.cc
TEST_CASE("MeshTopologyGenerator generates correct port indices", "[topology]") {
    // 验证 2x2 Mesh 生成的 JSON 包含正确的端口索引
    // router_0_0.1 → router_0_1.3 (EAST → WEST)
    // router_0_0.2 → router_1_0.0 (SOUTH → NORTH)
}

TEST_CASE("RouterTLM reads config params correctly", "[topology]") {
    RouterTLM router("router_1_2", &eq);
    json cfg = {{"node_x", 1}, {"node_y", 2}, {"mesh_x", 4}, {"mesh_y", 4}};
    router.set_config(cfg);
    router.init();
    
    REQUIRE(router.node_x() == 1);
    REQUIRE(router.node_y() == 2);
    REQUIRE(router.mesh_x() == 4);
    REQUIRE(router.mesh_y() == 4);
}

TEST_CASE("XY routing uses configured coordinates", "[topology]") {
    RouterTLM router("router_1_1", &eq, 1, 1, 4, 4);
    XYRouting xy;
    
    // 从 (1,1) 到 (3,3) 应该优先东向
    unsigned port = xy.computeRoute(4, 15, 1, 1, 4, 4);  // dst_node=15=(3,3)
    REQUIRE(port == 1);  // EAST
}
```

### 11.2 集成测试

```cpp
// test/test_mesh_noc_integration.cc
TEST_CASE("4x4 Mesh NoC end-to-end packet routing", "[mesh][integration]") {
    // 1. 加载 mesh_4x4.json
    // 2. ModuleFactory::instantiateAll()
    // 3. 从 proc_0_0 发送 packet 到 proc_3_3
    // 4. 验证 packet 经过 6 跳到达
    // 5. 验证 flit 经过正确端口序列
}
```

### 11.3 Python 验证

```python
# test/test_topology_validator.py
def test_mesh_port_completeness():
    gen = MeshTopologyGenerator()
    gen.generate(rows=2, cols=2)
    config = gen.to_json_config()
    
    validator = TopologyValidator(config)
    result = validator.validate_all()
    
    assert result["connection_completeness"]["valid"]
    assert result["reachability"]["valid"]
    assert result["parameter_completeness"]["valid"]
```

---

## 12. 附录

### 12.1 与现有架构文档关系

| 本文档 | 前置依赖 | 说明 |
|--------|----------|------|
| ARCH-010 | ARCH-001 | 使用 SimObject/ModuleFactory 基类 |
| | ARCH-002 | 扩展复杂拓扑架构的生成能力 |
| | ARCH-009 | 拓扑可视化管线消费 TGMS 输出 |

### 12.2 变更日志

| 版本 | 日期 | 变更 |
|------|------|------|
| v1.0 | 2026-04-25 | 初始版本，基于 v3.0 差距分析修正 G3 论断 |

### 12.3 参考实现

- **gem5**: SimObject 参数通过 `Param` 宏和 `CxxConfig` 传递，与本文档 `set_config` 方案类似
- **Noxim**: 使用 XML 配置定义拓扑，端口通过方向名（N/E/S/W）索引
- **BookSim**: 拓扑在配置文件中显式声明每个连接，端口隐式按顺序编号

---

**文档结束**
