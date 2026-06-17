# cpptlm_config/ — Python 配置验证与生成工具 (DEPRECATED)

**版本**: 1.0
**最后更新**: 2026-06-17
**状态**: ⚠️ **Legacy / Deprecated** — 已被 `cpptlm.topo` + `cpptlm.topo.CxxCompatibleEmitter` + `cpptlm.library` 替代

## 状态更新 (2026-06-17)

本包保留仅为向后兼容, 每次 `import cpptlm_config` 触发 `DeprecationWarning`. 合并迁移在下一期 (见 `openspec/changes/unified-config-emitter/`).

**已废弃字段**:
- `module_groups` 数组 (C++ 端不识别, 见 `src/core/module_factory.cc:297-305`). 已迁移到 `groups` 字典形式.

**新代码请用**:
- `cpptlm.topo.TopoLayer` (递归嵌套拓扑层)
- `cpptlm.topo.CxxCompatibleEmitter` (Python → C++ JSON 展开)
- `cpptlm.library.{cpu_l1_cluster, mesh_cluster, SoC}` 等 (cluster 工厂)
- 详见 `cpptlm/AGENTS.md`

## 概述 (历史)

Python 配置包，提供拓扑验证和配置生成功能。主要组件：

| 文件 | 作用 | 关键类/函数 | 状态 |
|------|------|-------------|------|
| `validator.py` | 两阶段验证（结构+参数） | `TopologyValidator`, `ValidationResult` | 保留 |
| `topology_adapter.py` | Mesh/Ring 拓扑生成适配器 | `TopologyAdapter` | DEPRECATED (backend 缺失) |
| `builder.py` | Pydantic 模型 + ConfigBuilder | `ConfigBuilder` | DEPRECATED (仍可用) |
| `models.py` | Pydantic 数据模型 | `ModuleSpec`, `ConnectionSpec`, `module_groups` 字段 | DEPRECATED |
| `types.py` | 类型枚举 | `RouterPort`, `NICPort`, `ModuleType` | 保留 |

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