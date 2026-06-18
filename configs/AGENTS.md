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

> **模块字段语义边界** (2026-06-17, see `openspec/changes/field-name-unification`):
> - `params`: optional object, **模块参数 dict** (主路径, 通过 `ModuleFactory::validateConfig` 校验, 注入到 `SimObject::set_config()`)
> - `config`: optional string, **外部配置文件路径** (SimModule 路径专用, 例如 `CpuCluster` 加载独立 JSON). **不要**把参数 dict 错放在此字段, C++ 端会**静默忽略** (LINT005 报错).
> - 详见 `cpptlm/AGENTS.md` "推荐用法" 段.

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
- `configs/example_hier2/`、`example_hierarchy/`、`example_layout/` 已在 2026-06-15 归档至 `docs-archived/dead-configs-2026-q2/`（legacy 模块类型 CPUSim/CPUCluster/MemCluster/CacheSim/Router/Arbiter 在 v2.1 起无注册, v2.2 已彻底移除）；现代示例见 `configs/{mesh_4x4_tlm,hierarchical_2x2_tlm,ring_8_tlm}.json`
- `configs/include_chain_demo.json` 已在 2026-06-17 归档至 `docs-archived/dead-configs-2026-q2/include_chain_demo.json`（C++ 端不识别 `$include` 数组内联语法, 仅 `"include": "path"` 字符串被支持）
- `coherence_domains` 字段当前为 **stub**（C++ 解析但不消费, 详见 `docs/adr/ADR-X.14-coherence-domains-stub.md`）。声明此字段不影响运行行为, 仅作为未来实现的占位符。

## SimModule 嵌套 JSON (v2.2 新增)

`SimModule` 派生类（如 `CpuCluster`）支持 JSON 内联 `modules` 数组实现多层嵌套实例化，**不限深度但有运行时保护**：

```json
{
  "modules": [
    {
      "name": "cluster0",
      "type": "CpuCluster",
      "params": { "num_cpus": 4, "cluster_id": "outer" },
      "modules": [
        { "name": "cpu0", "type": "CPUTLM", "params": { "pattern": "SEQUENTIAL" } },
        { "name": "cache", "type": "CacheTLM", "params": { "size": "32KB" } },
        { "name": "mem", "type": "MemoryTLM", "params": { "capacity_gb": 4 } }
      ],
      "connections": [
        { "src": "cpu0", "dst": "cache", "latency": 1 },
        { "src": "cache", "dst": "mem", "latency": 10 }
      ],
      "outputs": [
        { "internal": "cpu0.req_out", "external": "cpu0_to_bus" }
      ],
      "inputs": [
        { "internal": "cpu0.resp_in", "external": "bus_to_cpu0" }
      ]
    }
  ]
}
```

**关键字段**:

| 字段 | 用途 | 适用范围 |
|------|------|----------|
| 内联 `modules` | 子模块声明数组 | `SimModule` 类型（如 `CpuCluster`） |
| 内联 `connections` | 子模块间连接（含 `latency`） | `SimModule` 内部 |
| `outputs` | 暴露内部子模块端口到外部命名空间 | `SimModule` 类型 |
| `inputs` | 暴露外部端口到内部子模块 | `SimModule` 类型 |
| `params.num_cpus` | CPU 数量（CpuCluster 专用） | `CpuCluster` |
| `params.cluster_id` | 集群 ID（debug 用） | `CpuCluster` |

**限深保护**: `SimModule::MAX_DEPTH = 8`（`include/core/sim_module.hh`），超出抛 `std::runtime_error`。`thread_local` 计数器保证多线程仿真独立。

**活跃示例**:
- `configs/example_simmodule_nested_2level.json` — 顶层 CpuCluster + 4 CPUTLM + CacheTLM + MemoryTLM
- `configs/example_simmodule_nested_3level_static.json` — 3 层 CpuCluster 嵌套（outer → mid → inner）

**生成器**: `examples/generate_nested_soc.py` 用 `cpptlm/topo/` + `cpptlm/library/` API 程序化生成嵌套 JSON。

**测试覆盖**:
- C++ Catch2: `test/test_simmodule_nested.cc`（7 用例 `[simmodule]` 标签）
- Python pytest: `test/python/test_simmodule_emitter.py`（6 用例）