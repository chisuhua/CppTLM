# cpptlm_config/ — Python 配置验证与生成工具

**版本**: 1.0
**最后更新**: 2026-05-09
**状态**: ✅ 已实施

## 概述

Python 配置包，提供拓扑验证和配置生成功能。主要组件：

| 文件 | 作用 | 关键类/函数 |
|------|------|-------------|
| `validator.py` | 两阶段验证（结构+参数） | `TopologyValidator`, `ValidationResult` |
| `topology_adapter.py` | Mesh/Ring 拓扑生成适配器 | `TopologyAdapter` |
| `builder.py` | Pydantic 模型 + ConfigBuilder | `ConfigBuilder` |
| `models.py` | Pydantic 数据模型 | `ModuleSpec`, `ConnectionSpec` |
| `types.py` | 类型枚举 | `RouterPort`, `NICPort`, `ModuleType` |

## 验证规则

| 规则 | 代码 | 说明 |
|------|------|------|
| 所有连接模块已定义 | VALID-01 | |
| BFS 可达性 | VALID-02 | 从任一终端可达所有其他终端 |
| 路由器端口方向 | PORT-01 | XY mesh 物理方向匹配 |
| Router-Local → NI-Network | PORT-03 | Router 端口 4 → NICTLM 端口 1 |
| 必需参数检查 | PARAM-01 | RouterTLM 需 node_x/node_y/mesh_x/mesh_y |
| 参数范围检查 | PARAM-02 | 数值在 min_value/max_value 范围内 |

## 使用方式

```bash
# 验证配置
python3 scripts/topology_validator.py configs/mesh_2x2.json -v

# Python API
from cpptlm_config.validator import TopologyValidator
from cpptlm_config.builder import ConfigBuilder

config = {"modules": [...], "connections": [...]}
v = TopologyValidator(config)
result = v.validate()
```

## 测试

87 个 Python 测试（pytest）：
```bash
python3 -m pytest test/python/ -v
```

## 依赖

- Python 3.8+
- pydantic >= 2.0