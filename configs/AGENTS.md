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
  "groups": {
    "cluster0": ["cache0", "cache1"]
  }
}
```

> **注意**: `module_groups` (数组形式, 老 schema) 已废弃 (2026-06-17, 见
> `openspec/changes/unified-config-emitter/`)。请用 `groups` (dict 形式)。
> C++ 端不识别 `module_groups` 字段 (`src/core/module_factory.cc:297-305` 只读
> `groups` 字典)。新生成的 JSON 不再包含 `module_groups`。

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
- **组内连接**: `groups` 字典定义命名模块组, 连接 src 用 `group:<name>` 前缀展开
- **正则展开**: 连接 src 用 `regex:<pattern>` 前缀按正则匹配模块名
- **延迟注入**: `"latency": N` 在 StreamAdapter 层面注入周期延迟

## 约定

- **JSON 验证**: ModuleFactory::validateConfig() 检查必需字段
- **拓扑文件命名**: `<type>_<scale>.json`（如 `mesh_4x4.json`）
- **配置继承**: `base.json` 定义公共参数

## 注意事项

- 修改拓扑后重新运行 `./build/bin/cpptlm_tests "[phase6]"` 验证
- `configs/example_hier2/`、`example_hierarchy/`、`example_layout/` 已在 2026-06-15 归档至 `docs-archived/dead-configs-2026-q2/`（legacy 模块类型 CPUSim/CPUCluster/MemCluster/CacheSim/Router/Arbiter 在 `BUILD_LEGACY_MODULES=OFF` 默认配置下无注册）；现代示例见 `configs/{mesh_4x4_tlm,hierarchical_2x2_tlm,ring_8_tlm}.json`
- `configs/include_chain_demo.json` 已在 2026-06-17 归档至 `docs-archived/dead-configs-2026-q2/include_chain_demo.json`（C++ 端不识别 `$include` 数组内联语法, 仅 `"include": "path"` 字符串被支持）
- `coherence_domains` 字段当前为 **stub**（C++ 解析但不消费, 详见 `docs/adr/ADR-X.14-coherence-domains-stub.md`）。声明此字段不影响运行行为, 仅作为未来实现的占位符。