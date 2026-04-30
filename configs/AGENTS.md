# configs/ — JSON 拓扑配置

**域**: 仿真拓扑配置文件（18 文件）
**作用**: 定义模块实例、连接关系、延迟参数、分组层级

## 核心 Schema

```json
{
  "modules": [
    { "name": "cache0", "type": "CacheTLM", "params": {...} },
    { "name": "xbar", "type": "CrossbarTLM", "params": {...} }
  ],
  "connections": [
    { "src": "cache0.req_out", "dst": "xbar.0", "latency": 1 }
  ],
  "module_groups": [
    { "name": "cluster0", "members": ["cache0", "cache1"] }
  ]
}
```

## 文件分类

| 文件 | 拓扑类型 | 规模 |
|------|----------|------|
| `mesh_2x2.json` | 2×2 mesh | 4 节点 |
| `mesh_4x4.json` | 4×4 mesh | 16 节点 |
| `hierarchical_2x2.json` | 分层 2×2 | 分组结构 |
| `ring_8.json` | 8 节点 ring | 环状 |
| `crossbar_test.json` | 单 crossbar | 测试用 |
| `cache_chstream_test.json` | Cache→Xbar→Memory | ChStream 测试 |

## 端口索引语法

- `"module.port_index"` — 模块名 + 端口号（`.0` 表示第 0 端口）
- `"xbar.0"` → CrossbarTLM 的第 0 个端口
- CrossbarTLM 支持多端口（`xbar.0` ~ `xbar.3`）

## 连接类型

- **直连**: `"src": "cache.req_out", "dst": "xbar.0"`
- **组内连接**: `module_groups` 定义层级关系
- **延迟注入**: `"latency": N` 在 StreamAdapter 层面注入周期延迟

## 约定

- **JSON 验证**: ModuleFactory::validateConfig() 检查必需字段
- **拓扑文件命名**: `<type>_<scale>.json`（如 `mesh_4x4.json`）
- **配置继承**: `base.json` 定义公共参数

## 注意事项

- 修改拓扑后重新运行 `./build/bin/cpptlm_tests "[phase6]"` 验证
- `configs/example_hierarchy/` 和 `configs/example_layout/` 包含示例结构