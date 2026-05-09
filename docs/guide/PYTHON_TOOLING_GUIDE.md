# CppTLM Python 工具链指南

> **版本**: 1.0
> **更新日期**: 2026-05-09
> **状态**: ✅ 已实施

---

## 1. 概述

CppTLM Python 工具链提供拓扑验证和配置生成功能，与 C++ 仿真器形成互补的两阶段验证体系。

**核心组件**:

| 组件 | 文件 | 作用 |
|------|------|------|
| 验证器 | `cpptlm_config/validator.py` | 拓扑结构验证、端口检查、参数校验 |
| 适配器 | `cpptlm_config/topology_adapter.py` | Mesh/Ring 拓扑生成适配器 |
| 构建器 | `cpptlm_config/builder.py` | Pydantic 模型 + ConfigBuilder |
| 包装器 | `scripts/topology_validator.py` | 命令行验证工具 |

---

## 2. 快速开始

### 2.1 安装依赖

```bash
pip install "pydantic>=2.0"
```

### 2.2 验证现有配置

```bash
# 验证内置配置
python3 scripts/topology_validator.py configs/mesh_2x2_tlm.json -v

# 验证自定义配置
python3 scripts/topology_validator.py my_config.json
```

### 2.3 生成新配置

```bash
# 使用示例脚本
python3 cpptlm_config/examples/mesh_2x2.py > mesh_2x2.json

# 验证生成的配置
python3 scripts/topology_validator.py mesh_2x2.json -v
```

---

## 3. 验证器 (validator.py)

### 3.1 验证规则

| 规则 | 代码 | 说明 | 严重级别 |
|------|------|------|---------|
| 模块连通性 | VALID-01 | 所有连接的模块都已在 modules 中定义 | 错误 |
| BFS 可达性 | VALID-02 | 所有终端节点从任意起点可达 | 错误 |
| 路由器端口方向 | PORT-01 | XY mesh 物理方向匹配 | 警告 |
| NI-Network 连接 | PORT-03 | Router-Local(4) → NICTLM-Network(1) | 错误 |
| 必需参数 | PARAM-01 | 模块必需参数存在 | 错误 |
| 参数范围 | PARAM-02 | 数值在 min/max 范围内 | 错误 |

### 3.2 Python API 使用

```python
from cpptlm_config.validator import TopologyValidator

# 加载配置
config = {
    "modules": [
        {"name": "router_0_0", "type": "RouterTLM", 
         "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}},
        {"name": "ni0", "type": "NICTLM",
         "params": {"node_id": 0, "mesh_x": 2, "mesh_y": 2}},
    ],
    "connections": [
        {"src": "router_0_0.4", "dst": "ni0.1"}
    ]
}

# 执行验证
v = TopologyValidator(config)
result = v.validate()

# 检查结果
if result.is_valid:
    print("✅ 配置有效")
else:
    print(f"❌ 发现 {len(result.errors)} 个错误")
    for e in result.errors:
        print(f"  [{e.code}] {e.message}")
        if e.suggestion:
            print(f"    建议: {e.suggestion}")

# 检查警告
if result.warnings:
    print(f"⚠️  {len(result.warnings)} 个警告")
    for w in result.warnings:
        print(f"  [{w.code}] {w.message}")
```

### 3.3 命令行使用

```bash
# 基本验证
python3 scripts/topology_validator.py configs/mesh_2x2_tlm.json

# 详细输出
python3 scripts/topology_validator.py configs/mesh_2x2_tlm.json -v

# 从 JSON 文件加载
python3 -c "
from cpptlm_config.validator import TopologyValidator
import json
with open('configs/mesh_2x2_tlm.json') as f:
    config = json.load(f)
v = TopologyValidator(config)
print(v.validate().is_valid)
"
```

---

## 4. 配置构建器 (builder.py)

### 4.1 使用 ConfigBuilder

```python
from cpptlm_config.builder import ConfigBuilder

builder = ConfigBuilder()

# 添加模块
builder.add_router("router_0_0", node_x=0, node_y=0, mesh_x=2, mesh_y=2)
builder.add_nic("ni0", node_id=0, mesh_x=2, mesh_y=2)
builder.add_cpu("cpu0")

# 添加连接
builder.add_connection("router_0_0.4", "ni0.1")

# 构建配置
config = builder.build()

# 导出 JSON
builder.export_json("my_config.json")
```

### 4.2 手动构建配置

```python
from cpptlm_config.builder import ConfigBuilder

builder = ConfigBuilder()

# 手动添加模块
builder.config["modules"].append({
    "name": "router_0_0",
    "type": "RouterTLM",
    "params": {
        "node_x": 0,
        "node_y": 0,
        "mesh_x": 2,
        "mesh_y": 2
    }
})

# 手动添加连接
builder.config["connections"].append({
    "src": "router_0_0.4",
    "dst": "ni0.1",
    "latency": 1
})
```

---

## 5. 拓扑适配器 (topology_adapter.py)

### 5.1 生成 Mesh 拓扑

```python
from cpptlm_config.topology_adapter import TopologyAdapter

# 生成 2x2 Mesh
adapter = TopologyAdapter.from_mesh(2, 2)
config = adapter.to_dict()

# 验证
from cpptlm_config.validator import TopologyValidator
v = TopologyValidator(config)
assert v.validate().is_valid
```

### 5.2 生成 Ring 拓扑

```python
from cpptlm_config.topology_adapter import TopologyAdapter

# 生成 8 节点 Ring
adapter = TopologyAdapter.from_ring(8)
config = adapter.to_dict()
```

### 5.3 导出配置

```python
import json

# 导出为 JSON
with open("ring_8.json", "w") as f:
    json.dump(config, f, indent=2)
```

---

## 6. 示例脚本

### 6.1 mesh_2x2.py

```bash
python3 cpptlm_config/examples/mesh_2x2.py
```

输出：
```json
{
  "modules": [
    {"name": "router_0_0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}},
    {"name": "ni0", "type": "NICTLM", "params": {"node_id": 0, "mesh_x": 2, "mesh_y": 2}},
    {"name": "cpu0", "type": "CPUSim"}
  ],
  "connections": [
    {"src": "router_0_0.4", "dst": "ni0.1"},
    {"src": "ni0.0", "dst": "cpu0"}
  ]
}
```

### 6.2 mesh_4x4_validated.py

```bash
python3 cpptlm_config/examples/mesh_4x4_validated.py
```

此脚本生成 4x4 Mesh 并自动验证，输出验证结果。

---

## 7. 测试

### 7.1 运行 Python 测试

```bash
# 全部 Python 测试
python3 -m pytest test/python/ -v

# 特定模块
python3 -m pytest test/python/test_validator.py -v
python3 -m pytest test/python/test_analyzer.py -v
```

### 7.2 测试覆盖

| 测试文件 | 用例数 | 覆盖范围 |
|---------|--------|---------|
| test_port_types.py | 15 | PortSpec, ModulePortSpec, ConfigMetadata |
| test_topology_generator.py | 25 | Mesh, Ring, Crossbar, Bus, Hierarchical |
| test_analyzer.py | 5 | TopologyAnalyzer（模块计数、连接计数） |
| test_credit_flow.py | 4 | calculate_vc_credits |
| test_derive_expr.py | 5 | DeriveExprParser（三元表达式、算术） |
| test_linter.py | 8 | TopologyLinter（模块/连接验证） |
| test_path_tracer.py | 5 | PathTracer（XY 路由、跳数） |

---

## 8. 常见问题

### Q: 验证器报告 PARAM-01 错误（缺少必需参数）

**A**: 检查模块参数是否完整：
- RouterTLM: `node_x`, `node_y`, `mesh_x`, `mesh_y`
- NICTLM: `node_id`, `mesh_x`, `mesh_y`

### Q: 验证器报告 PORT-03 错误

**A**: Router-Local(4) 必须连接 NICTLM-Network(1)：
```json
{"src": "router_0_0.4", "dst": "ni0.1"}  // ✅ 正确
{"src": "router_0_0.4", "dst": "ni0.0"}  // ❌ 错误（ni0.0 是 PE 端口）
```

### Q: 如何添加自定义验证规则

**A**: 继承 `TopologyValidator` 类：
```python
from cpptlm_config.validator import TopologyValidator

class MyValidator(TopologyValidator):
    def validate_custom(self) -> "MyValidator":
        # 自定义检查逻辑
        for mod in self.config.get("modules", []):
            if mod.get("type") == "MyModule":
                if "custom_param" not in mod.get("params", {}):
                    self.result.add_error("CUSTOM-01", f"缺少 custom_param")
        return self
```

---

## 9. 参考

| 资源 | 位置 |
|------|------|
| 验证器实现 | `cpptlm_config/validator.py` |
| 构建器实现 | `cpptlm_config/builder.py` |
| 适配器实现 | `cpptlm_config/topology_adapter.py` |
| 示例脚本 | `cpptlm_config/examples/` |
| Python 测试 | `test/python/` |

---

**维护**: CppTLM 开发团队
**版本**: 1.0 | **最后更新**: 2026-05-09