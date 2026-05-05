# ADR-X.12: Python 配置生成器设计

> **版本**: 2.0
> **日期**: 2026-05-05
> **状态**: 📝 提案（Phase 3.2）
> **关联**: TGMS Phase 3.2, ADR-X.9 (端口类型系统), ADR-X.10 (参数框架), ADR-X.11 (配置继承), ARCH-009 (可视化流水线)
> **变更**: v1.0 → v2.0: 补充可视化集成、版本管理、技术栈、端口组、验证器集成决策

---

## 术语

本 ADR 使用以下术语体系（与 ADR-X.9、ADR-X.10 一致）：

| 术语 | 定义 |
|------|------|
| **配置生成器** | Python 配置生成器（Config Generator），即 cpptlm_config 包 |
| **Port Type** | 端口类型的广义术语，包含 PortRole 和 BundleType |
| **PortRole** | 端口角色枚举 |
| **BundleType** | 捆绑类型枚举 |
| **Port Group** | 逻辑端口组 |

**阶段编号说明**（与 ADR-X.11 一致）：
- **Phase 3+**：Phase 3.1-3.4 的统称
- **Phase 3.2**：端口管理阶段（本 ADR 的实施阶段）
- **Phase 3.3**：配置增强阶段
- **Phase 3.4**：拓扑验证阶段

**命名规范**（m1 共识）：
- Python 包名：`cpptlm_config`（下划线），PyPI 发布名：`cpptlm-config`（连字符）
- 中文文档使用"配置生成器"，首次出现时标注 `（Config Generator）`

---

## 背景

当前 CppTLM 用户需要直接手写 JSON 配置文件，存在以下问题：

1. **类型不安全**: JSON 是动态类型，无法在编写时捕获类型错误
2. **缺少验证**: 只有运行时 C++ 端的 `validateConfig()` 能捕获错误
3. **重复工作**: 每次创建新配置需要复制粘贴大量相似结构
4. **端口错误**: 端口索引和类型容易写错（如 `router.0` vs `router.req_in[0]`）
5. **学习曲线**: 用户需要记住 JSON schema 和模块参数要求

现有 `topology_generator.py` 只解决了**拓扑结构**生成，缺少：
- 端口类型定义（initiator/target/bidirectional）
- 模块参数验证和默认值
- Bundle 类型配置
- 完整的配置生成工作流

### 开源社区参考

**DRAMSys 模式**: 使用 C++ 结构体 + nlohmann/json 宏实现配置驱动仿真。
用户只需修改 JSON，无需修改 C++ 代码。

**Python 生态最佳实践**:
- **Pydantic**: dataclass + 验证 + JSON Schema 生成
- ** attrs**: 轻量级 dataclass 替代方案
- **标准库 dataclasses**: Python 3.7+ 内置

---

## 决策 1: 使用 Pydantic v2 作为核心框架

### 问题

Python 端应该使用什么框架来实现类型安全的配置生成？

### 选项对比

| 选项 | 优点 | 缺点 | 适用性 |
|------|------|------|--------|
| **Pydantic v2** ✅ | 内置验证、JSON Schema 生成、IDE 支持好、性能优秀（Rust 核心） | 需要外部依赖 `pip install pydantic` | ⭐⭐⭐⭐⭐ 最适合 |
| 标准库 dataclasses | 无依赖、Python 3.7+ 内置 | 无验证、需手动实现 `to_json()` | ⭐⭐ 功能不足 |
| attrs | 轻量、灵活 | 生态不如 Pydantic、无内置 JSON Schema | ⭐⭐⭐ 中等 |
| 纯字典 + 手动验证 | 无依赖、灵活 | 类型不安全、验证代码冗长 | ⭐ 不推荐 |

### 决策

✅ **使用 Pydantic v2**

**理由**:
1. **类型验证**: 在 Python 端捕获配置错误，而非等到 C++ 运行时
2. **JSON Schema 生成**: `model_json_schema()` 可自动生成 CppTLM 配置 schema
3. **IDE 支持**: VS Code/PyCharm 自动补全和类型检查
4. **默认值处理**: 字段级默认值，减少用户配置工作量
5. **序列化控制**: 自定义 `to_json()` 方法，精确控制输出格式
6. **性能**: Pydantic v2 使用 Rust 核心，序列化速度快

**依赖管理**:
```bash
pip install pydantic>=2.0
pip install pydantic-settings  # 可选：环境变量配置
```

**最低 Python 版本**: Python 3.10+ (Pydantic v2 要求)

---

## 决策 2: 配置生成器 API 设计

### 设计目标

1. **用户无需手写 JSON**: 所有配置通过 Python API 生成
2. **类型安全**: 端口类型、参数类型在 Python 端验证
3. **与 topology_generator 集成**: 复用现有拓扑生成逻辑
4. **向后兼容**: 生成的 JSON 格式与现有 C++ 解析器完全兼容

### API 层次设计

```
┌─────────────────────────────────────────────────┐
│  Layer 3: 高级配置 API (用户主要使用)            │
│  - ConfigBuilder: 构建完整配置                    │
│  - ModuleFactory: 创建模块实例                    │
│  - ConnectionBuilder: 定义连接                    │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│  Layer 2: 类型定义层 (Pydantic Models)           │
│  - ModuleSpec: 模块规格                          │
│  - PortSpec: 端口规格 (ADR-X.9)                  │
│  - ParamSpec: 参数规格 (ADR-X.10)                │
│  - ConnectionSpec: 连接规格                      │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│  Layer 1: 枚举和基础类型                         │
│  - PortRole, BundleType, ParamType               │
│  - ModuleType, TopologyType                      │
└─────────────────────────────────────────────────┘
```

### 核心 API 示例

#### 2.1 基础模块定义

```python
from cpptlm_config import (
    ConfigBuilder, ModuleSpec, PortSpec, ConnectionSpec,
    PortRole, BundleType, ModuleType
)

# 创建配置构建器
builder = ConfigBuilder(name="mesh_2x2", description="2x2 Mesh NoC with TLM modules")

# 添加 RouterTLM 模块（自动应用默认端口配置）
for y in range(2):
    for x in range(2):
        builder.add_module(
            ModuleSpec(
                name=f"router_{x}_{y}",
                type=ModuleType.ROUTER_TLM,
                params={"node_x": x, "node_y": y, "mesh_x": 2, "mesh_y": 2}
            )
        )

# 添加 NICTLM 模块（自动配置端口）
for y in range(2):
    for x in range(2):
        builder.add_module(
            ModuleSpec(
                name=f"ni_{x}_{y}",
                type=ModuleType.NIC_TLM,
                params={"node_id": y * 2 + x, "mesh_x": 2, "mesh_y": 2}
            )
        )

# 添加 CPUTLM 模块
for y in range(2):
    for x in range(2):
        builder.add_module(
            ModuleSpec(
                name=f"cpu_{x}_{y}",
                type=ModuleType.CPU_TLM
                # params 使用默认值
            )
        )
```

#### 2.2 端口类型定义 (集成 ADR-X.9)

```python
from cpptlm_config import PortSpec, PortRole, BundleType

# 定义 RouterTLM 的端口规范
router_ports = [
    PortSpec(name="req_in", role=PortRole.TARGET, bundle=BundleType.CACHE_REQ, is_multi=True, port_count=5),
    PortSpec(name="resp_out", role=PortRole.TARGET, bundle=BundleType.CACHE_RESP, is_multi=True, port_count=5),
    PortSpec(name="resp_in", role=PortRole.INITIATOR, bundle=BundleType.CACHE_RESP, is_multi=True, port_count=5),
    PortSpec(name="req_out", role=PortRole.INITIATOR, bundle=BundleType.CACHE_REQ, is_multi=True, port_count=5),
]

# 将端口规范附加到模块
builder.add_module(
    ModuleSpec(
        name="router_0_0",
        type=ModuleType.ROUTER_TLM,
        ports=router_ports,
        params={"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}
    )
)
```

#### 2.3 参数验证 (集成 ADR-X.10)

```python
from cpptlm_config import ParamRule, ParamType

# 定义 RouterTLM 参数规则
router_param_rules = {
    "node_x": ParamRule(
        type=ParamType.INT,
        required=True,
        min_val="0",
        max_val="mesh_x - 1",
        description="Router X coordinate in mesh"
    ),
    "node_y": ParamRule(
        type=ParamType.INT,
        required=True,
        min_val="0",
        max_val="mesh_y - 1",
        description="Router Y coordinate in mesh"
    ),
    "mesh_x": ParamRule(
        type=ParamType.INT,
        required=True,
        min_val="1",
        description="Mesh width"
    ),
    "mesh_y": ParamRule(
        type=ParamType.INT,
        required=True,
        min_val="1",
        description="Mesh height"
    ),
}

# 注册参数规则
builder.register_param_rules(ModuleType.ROUTER_TLM, router_param_rules)

# 使用时自动验证
builder.add_module(
    ModuleSpec(
        name="router_0_0",
        type=ModuleType.ROUTER_TLM,
        params={"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}
    )
)
# 如果 params 缺少必需字段或类型错误，Python 端立即报错
```

#### 2.4 连接定义

```python
# 简单连接（单端口模块）
builder.add_connection(
    ConnectionSpec(src="cpu_0_0", dst="ni_0_0.0", latency=1, bandwidth=100)
)

# 多端口连接（带端口索引）
builder.add_connection(
    ConnectionSpec(src="router_0_0.2", dst="router_0_1.0", latency=1, bandwidth=100)
)

# 批量连接（使用通配符）
builder.add_connections_from_pattern(
    src_pattern="cpu_*.*",
    dst_pattern="ni_*.*",
    latency=1,
    bandwidth=100
)
```

#### 2.5 生成 JSON

```python
# 生成 JSON 配置
config = builder.build()
json_str = config.to_json(indent=2)

# 保存到文件
config.save("configs/mesh_2x2.json")

# 生成 JSON Schema（用于文档和验证）
schema = builder.generate_json_schema()
with open("configs/schema.json", "w") as f:
    json.dump(schema, f, indent=2)
```

---

## 决策 3: 与 topology_generator.py 集成策略

### 问题

新的 Python 配置生成器如何与现有的 `topology_generator.py` 集成？

### 选项对比

| 选项 | 优点 | 缺点 |
|------|------|------|
| **A) 封装包装** ✅ | 复用现有拓扑生成逻辑，向后兼容 | 需要适配层 |
| B) 完全重写 | 设计更清晰，无历史包袱 | 工作量大，丢失已有功能 |
| C) 并行共存 | 用户可选择使用哪个 | 维护两套代码 |

### 决策

✅ **选项 A) 封装包装**

**实施方案**:

```python
# cpptlm_config/topology_adapter.py

from scripts.topology_generator import TopologyGenerator
from .builder import ConfigBuilder, ModuleSpec, ConnectionSpec

class TopologyAdapter:
    """将 TopologyGenerator 的输出适配到 ConfigBuilder"""
    
    @staticmethod
    def from_mesh(rows: int, cols: int, builder: ConfigBuilder) -> ConfigBuilder:
        """从 Mesh 拓扑生成配置"""
        gen = TopologyGenerator(name=f"mesh_{rows}x{cols}")
        gen.add_mesh(rows, cols)
        
        # 将拓扑图转换为模块和连接
        for node, attrs in gen.graph.nodes(data=True):
            builder.add_module(ModuleSpec(
                name=node,
                type=attrs.get("type", "RouterTLM"),
                params=attrs.get("params", {})
            ))
        
        for src, dst, attrs in gen.graph.edges(data=True):
            builder.add_connection(ConnectionSpec(
                src=src,
                dst=dst,
                latency=attrs.get("latency", 1),
                bandwidth=attrs.get("bandwidth", 100)
            ))
        
        return builder
```

**用户工作流**:

```python
from cpptlm_config import ConfigBuilder, TopologyAdapter

# 方式 1: 使用拓扑适配器快速生成
builder = TopologyAdapter.from_mesh(
    rows=4, cols=4,
    builder=ConfigBuilder(name="mesh_4x4")
)

# 方式 2: 手动精细控制
builder = ConfigBuilder(name="custom_topology")
builder.add_module(...)
builder.add_connection(...)

# 生成配置
builder.build().save("configs/output.json")
```

---

## 决策 4: Pydantic Model 设计

### 4.1 枚举类型定义

```python
# cpptlm_config/types.py

from enum import Enum
from pydantic import BaseModel, Field

class PortRole(str, Enum):
    INITIATOR = "initiator"
    TARGET = "target"
    BI_DIRECTIONAL = "bi_directional"
    NETWORK = "network"
    PE = "pe"

class BundleType(str, Enum):
    CACHE_REQ = "cache_req"
    CACHE_RESP = "cache_resp"
    NOC_FLIT = "noc_flit"
    GENERIC = "generic"

class ModuleType(str, Enum):
    CPU_TLM = "CPUTLM"
    MEMORY_TLM = "MemoryTLM"
    ROUTER_TLM = "RouterTLM"
    NIC_TLM = "NICTLM"
    CROSSBAR_TLM = "CrossbarTLM"
    ARBITER_TLM = "ArbiterTLM"
    TRAFFIC_GEN_TLM = "TrafficGenTLM"

class ParamType(str, Enum):
    INT = "int"
    FLOAT = "float"
    STRING = "string"
    BOOL = "bool"
```

### 4.2 核心 Pydantic Models

```python
# cpptlm_config/models.py

from pydantic import BaseModel, Field, field_validator, model_serializer
from typing import Optional, Dict, List, Any

class PortSpec(BaseModel):
    """端口规格 (对应 ADR-X.9)"""
    name: str
    role: PortRole
    bundle: BundleType
    width: int = Field(default=64, ge=1, le=1024)
    is_multi: bool = False
    port_count: int = Field(default=1, ge=1)
    
    @field_validator("port_count")
    @classmethod
    def validate_multi_port_count(cls, v, info):
        if info.data.get("is_multi") and v < 2:
            raise ValueError("Multi-port must have port_count >= 2")
        return v

class ParamRule(BaseModel):
    """参数规则 (对应 ADR-X.10)"""
    type: ParamType
    required: bool = False
    default_val: Optional[str] = None
    min_val: Optional[str] = None
    max_val: Optional[str] = None
    derive_expr: Optional[str] = None
    description: str = ""

class ModuleSpec(BaseModel):
    """模块规格"""
    name: str
    type: ModuleType
    params: Dict[str, Any] = Field(default_factory=dict)
    ports: Optional[List[PortSpec]] = None
    
    @field_validator("name")
    @classmethod
    def validate_name(cls, v):
        if not v.replace("_", "").replace("-", "").isalnum():
            raise ValueError("Module name must be alphanumeric (underscores and hyphens allowed)")
        return v
    
    def to_json_dict(self) -> Dict:
        """转换为 CppTLM JSON 格式"""
        result = {"name": self.name, "type": self.type.value}
        if self.params:
            result["params"] = self.params
        return result

class ConnectionSpec(BaseModel):
    """连接规格"""
    src: str
    dst: str
    latency: int = Field(default=0, ge=0)
    bandwidth: Optional[int] = Field(default=None, ge=1)
    
    @field_validator("src", "dst")
    @classmethod
    def validate_port_spec(cls, v):
        # 验证格式: module_name 或 module_name.port_index
        parts = v.split(".")
        if len(parts) > 2:
            raise ValueError(f"Invalid port spec '{v}': expected 'module' or 'module.port'")
        if len(parts) == 2:
            port_part = parts[1]
            if not port_part.isdigit() and not port_part.replace("_", "").isalnum():
                raise ValueError(f"Invalid port index '{port_part}'")
        return v
    
    def to_json_dict(self) -> Dict:
        """转换为 CppTLM JSON 格式"""
        result = {"src": self.src, "dst": self.dst, "latency": self.latency}
        if self.bandwidth is not None:
            result["bandwidth"] = self.bandwidth
        return result

class ConfigSchema(BaseModel):
    """完整配置 Schema"""
    name: str
    description: str = ""
    version: str = "1.0"
    modules: List[ModuleSpec]
    connections: List[ConnectionSpec]
    groups: Optional[Dict[str, List[str]]] = None
    extends: Optional[str] = None
    
    def to_json(self, indent: int = 2) -> str:
        """生成 CppTLM JSON 字符串"""
        import json
        
        data = {
            "name": self.name,
            "description": self.description,
            "version": self.version,
            "modules": [m.to_json_dict() for m in self.modules],
            "connections": [c.to_json_dict() for c in self.connections],
        }
        if self.groups:
            data["groups"] = self.groups
        if self.extends:
            data["extends"] = self.extends
        
        return json.dumps(data, indent=indent)
    
    def save(self, filepath: str):
        """保存到 JSON 文件"""
        with open(filepath, "w") as f:
            f.write(self.to_json())
```

### 4.3 ConfigBuilder 实现

```python
# cpptlm_config/builder.py

from .models import ConfigSchema, ModuleSpec, ConnectionSpec, ParamRule
from .types import ModuleType
from typing import Dict, Optional

class ConfigBuilder:
    """配置构建器 - 高级 API"""
    
    def __init__(self, name: str, description: str = ""):
        self.name = name
        self.description = description
        self.modules: Dict[str, ModuleSpec] = {}
        self.connections: list[ConnectionSpec] = []
        self.groups: Dict[str, list[str]] = {}
        self.param_rules: Dict[ModuleType, Dict[str, ParamRule]] = {}
        self.extends: Optional[str] = None
    
    def add_module(self, module: ModuleSpec) -> "ConfigBuilder":
        """添加模块"""
        if module.name in self.modules:
            raise ValueError(f"Module '{module.name}' already exists")
        
        # 验证参数
        self._validate_module_params(module)
        
        self.modules[module.name] = module
        return self
    
    def add_connection(self, conn: ConnectionSpec) -> "ConfigBuilder":
        """添加连接"""
        self._validate_connection(conn)
        self.connections.append(conn)
        return self
    
    def add_connections_from_pattern(
        self,
        src_pattern: str,
        dst_pattern: str,
        latency: int = 0,
        bandwidth: Optional[int] = None
    ) -> "ConfigBuilder":
        """从通配符模式批量添加连接"""
        import fnmatch
        
        src_modules = [m for m in self.modules if fnmatch.fnmatch(m, src_pattern)]
        dst_modules = [m for m in self.modules if fnmatch.fnmatch(m, dst_pattern)]
        
        for src in src_modules:
            for dst in dst_modules:
                self.add_connection(ConnectionSpec(
                    src=src, dst=dst, latency=latency, bandwidth=bandwidth
                ))
        
        return self
    
    def register_param_rules(
        self,
        module_type: ModuleType,
        rules: Dict[str, ParamRule]
    ) -> "ConfigBuilder":
        """注册模块类型的参数规则"""
        self.param_rules[module_type] = rules
        return self
    
    def set_extends(self, base_config: str) -> "ConfigBuilder":
        """设置继承的基础配置"""
        self.extends = base_config
        return self
    
    def build(self) -> ConfigSchema:
        """构建最终配置"""
        return ConfigSchema(
            name=self.name,
            description=self.description,
            modules=list(self.modules.values()),
            connections=self.connections,
            groups=self.groups if self.groups else None,
            extends=self.extends
        )
    
    def generate_json_schema(self) -> Dict:
        """生成 JSON Schema (用于文档和外部验证)"""
        return ConfigSchema.model_json_schema()
    
    def _validate_module_params(self, module: ModuleSpec):
        """验证模块参数"""
        if module.type not in self.param_rules:
            return  # 无规则，跳过
        
        rules = self.param_rules[module.type]
        for param_name, rule in rules.items():
            if rule.required and param_name not in module.params:
                raise ValueError(
                    f"Module '{module.name}' missing required param '{param_name}'"
                )
            
            if param_name in module.params:
                value = module.params[param_name]
                # 类型检查
                if rule.type.value == "int" and not isinstance(value, int):
                    raise ValueError(
                        f"Module '{module.name}' param '{param_name}' must be int"
                    )
                # 范围检查 (简化版)
                if rule.min_val and rule.max_val:
                    if value < int(rule.min_val) or value > int(rule.max_val):
                        raise ValueError(
                            f"Module '{module.name}' param '{param_name}' out of range"
                        )
    
    def _validate_connection(self, conn: ConnectionSpec):
        """验证连接"""
        src_module = conn.src.split(".")[0]
        dst_module = conn.dst.split(".")[0]
        
        if src_module not in self.modules:
            raise ValueError(f"Connection references non-existent module '{src_module}'")
        if dst_module not in self.modules:
            raise ValueError(f"Connection references non-existent module '{dst_module}'")
```

---

## 决策 4.5: 可视化流水线集成（G4 共识）

### 问题

Python 配置生成器是否应直接输出可视化流水线所需的元数据？

### 决策

✅ **ConfigSchema 包含可视化元数据**，PortSpec 增加可选的 `layout_hint` 字段。

**可视化元数据格式**：
```python
class ConfigMetadata(BaseModel):
    version: str = "1.0.0"
    visualization: Optional[Dict[str, Any]] = None  # 可视化元数据

class PortSpec(BaseModel):
    layout_hint: Optional[str] = None  # "north", "east", "south", "west", "local"

class ConfigSchema(BaseModel):
    metadata: ConfigMetadata
    modules: List[ModuleSpec]
    connections: List[ConnectionSpec]
    
    def to_json_dict(self) -> Dict:
        return {
            "name": self.name,
            "description": self.description,
            "version": self.metadata.version,
            "modules": [m.to_json_dict() for m in self.modules],
            "connections": [c.to_json_dict() for c in self.connections],
            "visualization_metadata": self.metadata.visualization
        }
```

**与 ARCH-009 可视化流水线协作**：
- `stats_annotator` 使用 ConfigSchema 的模块类型、连接延迟信息标注统计
- `layout_manager` 使用 PortSpec.layout_hint 自动排列模块
- 避免二次计算，保持单一数据源

**Python API 示例**：
```python
builder.add_module(ModuleSpec(
    name="router_0_0",
    type=ModuleType.ROUTER_TLM,
    ports=[
        PortSpec(name="NORTH", role=PortRole.BIDIRECTIONAL, layout_hint="north"),
        PortSpec(name="EAST", role=PortRole.BIDIRECTIONAL, layout_hint="east"),
    ]
))

config = builder.build()
config.metadata.visualization = {
    "layout": "hierarchical",
    "auto_route": True
}
```

---

## 决策 5: 拓扑版本管理和变更追踪（M2 共识）

### 问题

拓扑配置应该如何进行版本管理和变更追踪？

### 决策

✅ **语义化版本（SemVer）**，格式 `major.minor.patch`。

**版本字段定义**：
```python
from datetime import datetime
from typing import List, Optional

class ConfigMetadata(BaseModel):
    version: str = "1.0.0"  # SemVer
    created_at: datetime = Field(default_factory=datetime.now)
    modified_at: datetime = Field(default_factory=datetime.now)
    changelog: List[str] = []
    author: Optional[str] = None
    description: str = ""

class ConfigSchema(BaseModel):
    metadata: ConfigMetadata
    modules: List[ModuleSpec]
    connections: List[ConnectionSpec]
```

**版本号更新策略**：
- `major`：拓扑结构破坏性变更（删除模块、改变连接拓扑）
- `minor`：向后兼容的功能添加（新增模块、添加可选参数）
- `patch`：向后兼容的修复（修正参数值、优化延迟配置）

**ConfigBuilder 版本管理**：
```python
class ConfigBuilder:
    def bump_version(self, level: str = "patch"):
        """升级版本号"""
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
        self.metadata.modified_at = datetime.now()
    
    def add_changelog(self, entry: str):
        """添加变更日志"""
        self.metadata.changelog.append(entry)
        self.metadata.modified_at = datetime.now()
```

**与 ARCH-010 TopologyRegistry 对齐**：
- Python 侧版本管理与 C++ 侧 TopologyRegistry 一致
- 支持版本回滚和 A/B 对比
- 为 Phase 4+ 的拓扑版本历史预留基础

---

## 决策 6: Python 技术栈（M3 共识）

### 问题

Python 配置生成器的技术栈和依赖应该如何管理？

### 决策

✅ **pyproject.toml 声明依赖**，Pydantic v2 约束。

**pyproject.toml**：
```toml
[project]
name = "cpptlm-config"
version = "0.1.0"
description = "CppTLM configuration generator with type safety and validation"
requires-python = ">=3.10"
dependencies = [
    "pydantic>=2.0,<3.0",
]

[project.optional-dependencies]
visualization = ["matplotlib>=3.5"]
dev = ["pytest>=7.0", "mypy>=1.0", "ruff>=0.1"]

[build-system]
requires = ["setuptools>=68.0", "wheel"]
build-backend = "setuptools.backends._legacy:_Backend"

[tool.ruff]
target-version = "py310"
line-length = 100
```

**技术栈决策理由**：
- Python 3.10+：Pydantic v2 要求，支持 modern type hints
- Pydantic v2：使用 `model_validator`、`field_validator` 等新 API，性能优秀（Rust 核心）
- Pydantic v1 和 v2 API 不兼容，明确排除 v1
- 可选依赖分离：visualization、dev 独立安装

**ADR 记录**：本 ADR 是 Python 技术栈的权威决策文档，技术栈变更应更新此处。

---

## 决策 7: 拓扑验证器集成（M1 共识）

### 问题

topology_validator.py 与配置生成器的关系是什么？

### 决策

✅ **整合为 cpptlm_config.validator 模块**，两阶段验证。

**验证器整合**：
```python
# cpptlm_config/validator.py

from pydantic import BaseModel
from typing import List, Optional

class ValidationIssue(BaseModel):
    severity: str  # "error", "warning", "info"
    message: str
    suggestion: Optional[str] = None  # 修复建议

class ValidationResult(BaseModel):
    is_valid: bool
    errors: List[ValidationIssue] = []
    warnings: List[ValidationIssue] = []
    
    def add_error(self, message: str, suggestion: Optional[str] = None):
        self.errors.append(ValidationIssue(severity="error", message=message, suggestion=suggestion))
        self.is_valid = False
    
    def add_warning(self, message: str, suggestion: Optional[str] = None):
        self.warnings.append(ValidationIssue(severity="warning", message=message, suggestion=suggestion))

class TopologyValidator:
    """拓扑验证器"""
    
    @staticmethod
    def validate_config(config: ConfigSchema) -> ValidationResult:
        """Python 侧基础验证"""
        result = ValidationResult(is_valid=True)
        
        # 检查孤立节点
        module_names = {m.name for m in config.modules}
        connected_modules = set()
        for conn in config.connections:
            connected_modules.add(conn.src.split(".")[0])
            connected_modules.add(conn.dst.split(".")[0])
        
        orphaned = module_names - connected_modules
        if orphaned:
            result.add_error(
                f"Orphaned modules: {', '.join(orphaned)}",
                "Add connections for these modules or remove them"
            )
        
        # 检查重复连接
        seen_connections = set()
        for conn in config.connections:
            key = f"{conn.src}->{conn.dst}"
            if key in seen_connections:
                result.add_warning(
                    f"Duplicate connection: {key}",
                    "Remove duplicate connection"
                )
            seen_connections.add(key)
        
        return result
```

**两阶段验证**：
1. Python 侧：ConfigBuilder.build() 时自动调用基础验证
2. C++ 侧：ModuleFactory 实例化前调用完整验证

**验证失败修复建议**（M1 共识）：
- 简单错误自动生成修复建议（如"连接 A→B 缺少反向连接，是否添加？"）
- 复杂错误标记为需要人工干预
- 为未来自动化修复流程预留基础

---

## 决策 8: 端口组配置（m2 共识）

### 问题

端口组（port_groups）在 Python 配置生成器中如何配置？

### 决策

✅ **ConfigBuilder 提供 add_port_group() 方法**。

**端口组模型**：
```python
class PortGroup(BaseModel):
    name: str
    ports: List[PortSpec]
    bundle_type: BundleType = BundleType.SINGLE

class ModuleSpec(BaseModel):
    port_groups: List[PortGroup] = []
```

**ConfigBuilder API**：
```python
class ConfigBuilder:
    def add_port_group(
        self,
        module_name: str,
        group_name: str,
        ports: List[PortSpec],
        bundle_type: BundleType = BundleType.SINGLE
    ) -> "ConfigBuilder":
        """添加端口组到指定模块"""
        if module_name not in self.modules:
            raise ValueError(f"Module '{module_name}' not found")
        
        group = PortGroup(name=group_name, ports=ports, bundle_type=bundle_type)
        self.modules[module_name].port_groups.append(group)
        return self
```

**使用示例**：
```python
builder.add_module(ModuleSpec(
    name="nic_0",
    type=ModuleType.NIC_TLM,
    params={"node_id": 0}
))

builder.add_port_group("nic_0", "eth0_group", [
    PortSpec(role=PortRole.BIDIRECTIONAL, index=0, bundle=BundleType.NOC_FLIT),
    PortSpec(role=PortRole.BIDIRECTIONAL, index=1, bundle=BundleType.NOC_FLIT),
], bundle_type=BundleType.BUNDLE_MASTER)
```

---

## 决策 9: 用户工作流设计

### 5.1 简单配置工作流

```python
#!/usr/bin/env python3
"""生成 2x2 Mesh NoC 配置"""

from cpptlm_config import (
    ConfigBuilder, ModuleSpec, ConnectionSpec,
    ModuleType, TopologyAdapter
)

def main():
    # 方式 1: 使用拓扑适配器（推荐快速原型）
    builder = TopologyAdapter.from_mesh(
        rows=2, cols=2,
        builder=ConfigBuilder(
            name="mesh_2x2",
            description="2x2 Mesh NoC with TLM modules"
        )
    )
    
    # 方式 2: 手动精细控制（推荐生产配置）
    # builder = ConfigBuilder(name="mesh_2x2")
    # builder.add_module(ModuleSpec(name="router_0_0", type=ModuleType.ROUTER_TLM, ...))
    
    # 构建并保存
    config = builder.build()
    config.save("configs/mesh_2x2.json")
    print(f"✓ Generated config: configs/mesh_2x2.json")
    
    # 可选：生成 JSON Schema
    schema = builder.generate_json_schema()
    import json
    with open("configs/mesh_2x2_schema.json", "w") as f:
        json.dump(schema, f, indent=2)
    print(f"✓ Generated schema: configs/mesh_2x2_schema.json")

if __name__ == "__main__":
    main()
```

### 5.2 高级配置工作流（带参数验证）

```python
#!/usr/bin/env python3
"""生成带参数验证的 4x4 Mesh NoC 配置"""

from cpptlm_config import (
    ConfigBuilder, ModuleSpec, ConnectionSpec,
    ParamRule, ParamType, ModuleType, PortSpec,
    PortRole, BundleType, TopologyAdapter
)

def main():
    builder = ConfigBuilder(
        name="mesh_4x4_full",
        description="4x4 Mesh NoC with full parameter validation"
    )
    
    # 注册 RouterTLM 参数规则
    builder.register_param_rules(ModuleType.ROUTER_TLM, {
        "node_x": ParamRule(
            type=ParamType.INT, required=True,
            min_val="0", max_val="3",
            description="Router X coordinate"
        ),
        "node_y": ParamRule(
            type=ParamType.INT, required=True,
            min_val="0", max_val="3",
            description="Router Y coordinate"
        ),
        "mesh_x": ParamRule(type=ParamType.INT, required=True, description="Mesh width"),
        "mesh_y": ParamRule(type=ParamType.INT, required=True, description="Mesh height"),
    })
    
    # 注册 NICTLM 参数规则
    builder.register_param_rules(ModuleType.NIC_TLM, {
        "node_id": ParamRule(type=ParamType.INT, required=True, min_val="0", max_val="15"),
        "mesh_x": ParamRule(type=ParamType.INT, required=True),
        "mesh_y": ParamRule(type=ParamType.INT, required=True),
    })
    
    # 使用拓扑适配器生成
    builder = TopologyAdapter.from_mesh(4, 4, builder)
    
    # 添加额外验证：检查所有模块参数
    config = builder.build()
    
    # 保存
    config.save("configs/mesh_4x4_full.json")
    print("✓ Generated validated config: configs/mesh_4x4_full.json")

if __name__ == "__main__":
    main()
```

### 5.3 继承配置工作流

```python
#!/usr/bin/env python3
"""生成继承基础配置的覆盖配置"""

from cpptlm_config import ConfigBuilder, ModuleSpec, ModuleType

def main():
    # 创建覆盖配置
    builder = ConfigBuilder(
        name="mesh_4x4_high_freq",
        description="4x4 Mesh with higher CPU frequency"
    )
    
    # 继承基础配置
    builder.set_extends("configs/mesh_4x4_full.json")
    
    # 只覆盖需要修改的模块参数
    builder.add_module(ModuleSpec(
        name="cpu_0_0",
        type=ModuleType.CPU_TLM,
        params={"frequency": 2000}  # 覆盖默认频率
    ))
    
    # 构建并保存
    config = builder.build()
    config.save("configs/mesh_4x4_high_freq.json")
    print("✓ Generated override config: configs/mesh_4x4_high_freq.json")

if __name__ == "__main__":
    main()
```

---

## 决策 10: 项目结构

```
cpptlm_config/
├── __init__.py              # 导出公共 API
├── types.py                 # 枚举类型定义
├── models.py                # Pydantic Models
├── builder.py               # ConfigBuilder 实现
├── topology_adapter.py      # 与 topology_generator 集成
├── validators.py            # 自定义验证器
└── examples/
    ├── mesh_2x2.py          # 简单示例
    ├── mesh_4x4_validated.py # 带验证示例
    └── hierarchical.py      # 继承配置示例

scripts/
├── topology_generator.py    # 现有拓扑生成器（保持不变）
└── generate_configs.py      # 批量生成配置的脚本

configs/
├── mesh_2x2.json            # 生成的配置
├── mesh_4x4_full.json       # 生成的配置
└── *.json                   # 其他配置
```

---

## 实施计划

### Phase 3.2 实施步骤

| 步骤 | 任务 | 估计工时 | 依赖 |
|------|------|---------|------|
| 1 | 创建 `cpptlm_config/` 包结构 | 2h | 无 |
| 2 | 实现 `types.py` 枚举定义 | 1h | 无 |
| 3 | 实现 `models.py` Pydantic Models | 4h | 步骤 2 |
| 4 | 实现 `builder.py` ConfigBuilder | 6h | 步骤 3 |
| 5 | 实现 `topology_adapter.py` 集成 | 4h | 步骤 4 + topology_generator.py |
| 6 | 编写示例和文档 | 3h | 步骤 5 |
| 7 | 单元测试 | 4h | 步骤 6 |
| **总计** | | **24h** | |

### 验证标准

1. **生成的 JSON 通过 C++ validateConfig()**: 所有生成的配置必须通过 C++ 端验证
2. **Python 端捕获类型错误**: 非法参数类型在 Python 端报错，而非运行时
3. **向后兼容**: 现有 `configs/*.json` 仍可被 C++ 解析
4. **文档完整**: 提供 3 个示例脚本 + API 文档

---

## 风险与权衡

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| Pydantic v2 依赖 | 需要 `pip install` | 提供 `requirements.txt`，文档说明 |
| Python 3.10+ 要求 | 老旧 Python 版本不支持 | 文档明确要求，提供降级方案（dataclasses） |
| 学习曲线 | 用户需要学习 Pydantic | 提供示例和文档，API 设计直观 |
| 与 topology_generator 重复 | 两套拓扑生成逻辑 | 通过 TopologyAdapter 封装复用 |

---

## 变更记录

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-05-05 | 初始提案，设计 Python 配置生成器完整方案 |
| 2.0 | 2026-05-05 | 补充术语表和阶段编号说明（m3 共识）<br>补充可视化流水线集成决策（决策 4.5，G4 共识）<br>补充拓扑版本管理决策（决策 5，M2 共识）<br>补充 Python 技术栈决策（决策 6，M3 共识）<br>补充拓扑验证器集成决策（决策 7，M1 共识）<br>补充端口组配置决策（决策 8，m2 共识） |

## 共识追踪

| 议题 | 状态 | 说明 |
|------|------|------|
| G4 | ✅ 已整合 | ConfigSchema 包含可视化元数据，PortSpec 增加 layout_hint |
| M1 | ✅ 已整合 | topology_validator.py 整合为 cpptlm_config.validator，两阶段验证 |
| M2 | ✅ 已整合 | SemVer 版本管理，ConfigMetadata 包含版本和变更日志 |
| M3 | ✅ 已整合 | pyproject.toml 声明依赖，Pydantic v2 约束 |
| m2 | ✅ 已整合 | ConfigBuilder 提供 add_port_group() 方法 |

---

**状态**: 📝 提案，待 Phase 3.2 实施
