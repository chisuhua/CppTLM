# cpptlm/ — Python library for CppTLM

**版本**: 0.1.0 (Phase 7 阶段)
**状态**: 活跃开发

## 包结构

```
cpptlm/
├── __init__.py             # 顶层 re-export: ConfigBuilder, SimulationRunner, PerformanceDashboard
├── cli.py                  # `python -m cpptlm` 入口 (run/dashboard 子命令)
├── config/                 # 高级 topology 构造器 (部分依赖 cpptlm_config legacy)
│   ├── generator.py        # TopologyGenerator 包装 (DEPRECATED)
│   └── topologies.py       # MeshTopology/RingTopology/CrossbarTopology (DEPRECATED)
├── simulation/             # C++ 仿真运行 + 结果分析
├── visualization/          # 性能 dashboard + topology viewer
├── topo/                   # 分层拓扑核心抽象 (活跃)
│   ├── layer.py            # TopoLayer/ModuleSpec/ConnectionSpec
│   ├── patch.py            # TopoPatch + glob/regex Selector
│   ├── variant.py          # TopoVariant + TopoVariantSet
│   ├── orchestrator.py     # TopoOrchestrator (高层组合)
│   ├── emitter.py          # CxxCompatibleEmitter (Python → C++ JSON)
│   └── cli.py              # `python -m cpptlm.topo.cli`
└── library/                # 预置 cluster 工厂 + SoC 编排器 (活跃)
    ├── standard.py         # cpu_l1_cluster, memory_cluster, crossbar_cluster
    ├── interconnect.py     # mesh_cluster, ring_cluster
    └── soc.py              # SoC fluent API
```

## 核心抽象（活跃）

| 抽象 | 路径 | 用途 |
|------|------|------|
| `TopoLayer` | `cpptlm.topo.layer` | 递归嵌套拓扑层, 含 modules / connections / sublayers / tags / metadata |
| `TopoPatch` | `cpptlm.topo.patch` | glob/regex 选择器 + add/remove/replace/rewire 动作, 局部修改 layer |
| `TopoVariant` | `cpptlm.topo.variant` | 基于 base + patches 的差异化变体 |
| `TopoOrchestrator` | `cpptlm.topo.orchestrator` | 高层 layer/variant/patch 组合 |
| `CxxCompatibleEmitter` | `cpptlm.topo.emitter` | TopoLayer 树 → C++ ModuleFactory 可加载 JSON dict |
| `cpu_l1_cluster()` 等 | `cpptlm.library.standard` | 预置 cluster 工厂 |
| `SoC` | `cpptlm.library.soc` | fluent API SoC 顶层组合器 |

## 弃用/历史（保留向后兼容）

- `cpptlm_config.*` (旧 Pydantic 库) — 已被 `cpptlm.topo` + `cpptlm.library` 替代. 每次 import 触发 `DeprecationWarning`. 合并迁移在下一期 (见 `openspec/changes/unified-config-emitter/`).
- `cpptlm.config.topologies` (MeshTopology 等) — 依赖已不存在的 `scripts.topology_generator` backend. 保留仅为向后兼容. 新代码用 `cpptlm.library.mesh_cluster` 等.

## 推荐用法 (Phase 7+)

```python
from cpptlm.library import SoC, cpu_l1_cluster, memory_cluster, crossbar_cluster
from cpptlm.topo.emitter import CxxCompatibleEmitter  # 内部被 SoC.save() 调用

soc = SoC("my_soc")
soc.add_cluster(cpu_l1_cluster(0).layout_grid(dx=3, dy=1, x_offset=0)).tag("compute")
soc.add_cluster(cpu_l1_cluster(1).layout_grid(dx=3, dy=1, x_offset=200))
soc.add_module("xbar", "CrossbarTLM")
soc.connect_group("compute", "xbar.0", latency=5)
soc.save("configs/my_soc.json")
```

输出的 JSON 字段与 C++ `ModuleFactory` 完整兼容, 不含 `module_groups`/`sublayers` 等死字段.

> **模块字段语义边界** (2026-06-17, see `openspec/changes/field-name-unification`):
> - **use `params` for module config; `config` is reserved for external file path**.
> - `params` 字段是模块参数 dict 的规范位置 (`ModuleSpec.params` → emitted JSON `"params": {...}`).
> - `config` 字段**仅**用于外部配置文件路径 (string), 适用于 `CpuCluster` 等 SimModule 加载独立 JSON 的场景.
> - 把参数 dict 放在 `config` 字段会触发 C++ 端 `LINT005` 错误 (`config-lint` 规则), 不会被静默忽略.
> - 例: `ModuleSpec(name="cpu", type="TrafficGenTLM", params={...})` (✅ 正确) vs `config={...}` (❌ LINT005).

## 测试

```bash
pytest test/python/test_topo_layer.py test/python/test_topo_emitter.py \
       test/python/test_library.py -v
```
