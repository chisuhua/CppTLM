# CppTLM Python Library 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建 CppTLM Python 库，实现芯片设计生成、拓扑可视化、性能可视化三大能力，打通 Python → JSON → C++ → Python 端到端数据流。

**Scope:** 四阶段实施，覆盖 C++ 统计注册修复、Python 配置层增强、仿真运行器、可视化层。

**Prerequisites:**
- C++17 编译器、CMake、Ninja
- Python 3.8+、pip
- 依赖包: pydantic (>=1.10.0), networkx (>=2.6.0), pydot (>=1.4.0) (已有); dash (>=2.0.0), plotly (>=5.0.0), watchdog (>=2.0.0) (可选)

---

## 执行摘要 (Executive Summary)

本计划旨在为 CppTLM 仿真框架构建一套完整的 Python 工具库，使用户能够通过 Python API 完成从芯片拓扑设计、仿真运行到结果可视化的全流程工作。当前存在的关键阻塞问题是：所有 TLM 模块（CacheTLM、MemoryTLM、CrossbarTLM、RouterTLM 等）虽然内部定义了 StatGroup 性能统计组，但从未向 StatsManager 注册，导致 StreamingReporter 输出的 JSON Lines 文件始终为空，下游 Python 可视化脚本无数据可展示。

计划分四阶段实施：
- **Phase 0** (阻塞修复): 在 C++ 层打通统计注册数据流
- **Phase 1** (配置增强): 修复 Python API 不匹配问题，增加高层封装类（纯 Python，不依赖 C++ 构建）
- **Phase 2** (运行器): 实现 Python 驱动的仿真运行和结果解析（依赖 Phase 0）
- **Phase 3** (可视化): 重构现有脚本为可复用的 Python 类库（依赖 Phase 0 和 Phase 2）

---

## 文件清单 (File Inventory)

### 需要修改的文件 (C++ 层)

| 文件 | 当前状态 | 修改内容 |
|------|---------|---------|
| `include/core/chstream_module.hh` | 基类无统计接口 | 添加 `virtual StatGroup* get_stats_group()` |
| `include/tlm/cache_tlm.hh` | 有 `stats_` 成员 | 重写 `get_stats_group()` 返回 `&stats_` |
| `include/tlm/memory_tlm.hh` | 有 `stats_` 成员 | 重写 `get_stats_group()` 返回 `&stats_` |
| `include/tlm/crossbar_tlm.hh` | 有 `stats_` 成员 | 重写 `get_stats_group()` 返回 `&stats_` |
| `include/tlm/router_tlm.hh` | 有 `stat_group_` 成员 | 重写 `get_stats_group()` 返回 `&stat_group_` |
| `include/tlm/nic_tlm.hh` | 有 `stat_group_` 成员 | 重写 `get_stats_group()` 返回 `&stat_group_` |
| `include/tlm/traffic_gen_tlm.hh` | 有 `stats_` 成员 | 重写 `get_stats_group()` 返回 `&stats_` |
| `src/core/module_factory.cc` | 无统计注册逻辑 | Step 7 后添加自动注册/注销 |
| `src/core/module_factory.cc` | 无清理逻辑 | 析构函数/清理方法中添加 unregister |

### 需要修改的文件 (Python 层 — 现有)

| 文件 | 当前状态 | 修改内容 |
|------|---------|---------|
| `cpptlm_config/topology_adapter.py` | 调用 `gen.add_mesh()` (API 不匹配) | 改为 `gen.generate_mesh()`、`gen.generate_ring()` |
| `cpptlm_config/builder.py` | 无 `set_extends()` | 添加配置继承支持 |
| `scripts/topology_generator.py` | 独立脚本 | 保持独立但增强 API，或迁移到包内 |
| `scripts/stats_watcher.py` | 独立脚本，Dash 依赖可选 | 重构为 `PerformanceDashboard` 类，支持 Dash 降级 |
| `scripts/stats_annotator.py` | 独立脚本 | 重构为 `ReportGenerator` 类 |

### 需要创建的新文件

| 文件 | 用途 | 所属阶段 |
|------|------|---------|
| `cpptlm/__init__.py` | Python 包根入口 | Phase 1 |
| `cpptlm/config/__init__.py` | 配置子包 | Phase 1 |
| `cpptlm/config/topologies.py` | MeshTopology/RingTopology/CrossbarTopology 封装类 | Phase 1 |
| `cpptlm/config/generator.py` | 从 `scripts/topology_generator.py` 迁移/封装 | Phase 1 |
| `cpptlm/simulation/__init__.py` | 仿真运行子包 | Phase 2 |
| `cpptlm/simulation/runner.py` | `SimulationRunner` 子进程封装 | Phase 2 |
| `cpptlm/simulation/result.py` | `Result.from_jsonl()` 结果解析器 | Phase 2 |
| `cpptlm/simulation/system.py` | `System` 组合类 (拓扑+负载+时长) | Phase 2 |
| `cpptlm/visualization/__init__.py` | 可视化子包 | Phase 3 |
| `cpptlm/visualization/dashboard.py` | `PerformanceDashboard` 性能仪表板（支持 Dash 降级） | Phase 3 |
| `cpptlm/visualization/topology_viewer.py` | `TopologyViewer` 拓扑可视化 | Phase 3 |
| `cpptlm/visualization/report.py` | `ReportGenerator` HTML 报告生成 | Phase 3 |
| `test/test_phase0_stats_registration.cc` | Phase 0 统计注册验证测试 | Phase 0 |
| `cpptlm/tests/test_config.py` | Phase 1 配置层单元测试（≥5 个测试文件） | Phase 1 |
| `cpptlm/tests/test_simulation_runner.py` | Phase 2 运行器单元测试 | Phase 2 |
| `cpptlm/tests/test_end_to_end.py` | Phase 2 端到端集成测试 | Phase 2 |
| `cpptlm/tests/test_visualization.py` | Phase 3 可视化测试 | Phase 3 |
| `pyproject.toml` | Python 包发布配置 | Phase 1 |

---

## 依赖关系图 (Dependency Graph)

```
Phase 0: 打通数据流 (C++ 统计注册)
    │
    ├──→ Phase 1: Python 配置层增强 ─────┐
    │    (纯 Python，可与 Phase 0 并行)   │
    │                                    │
    └──→ Phase 2: 仿真运行器 ────────────┤
         (依赖 Phase 0 完成)              │
         (可与 Phase 1 并行)              │
                                        │
         Phase 3: 可视化层 ←─────────────┘
         (依赖 Phase 0 + Phase 2)
```

**说明:**
- Phase 0 是 C++ 层的严格阻塞项：必须先完成统计注册，StreamingReporter 才能输出有效 JSONL 数据
- Phase 1 是纯 Python 工作，**不依赖 Phase 0**，可独立并行开发
- Phase 2 依赖 Phase 0（需要编译后的 `cpptlm_sim` 二进制和有效 JSONL 输出）
- Phase 2 可与 Phase 1 并行（互不依赖）
- Phase 3 依赖 Phase 0（需要真实数据）和 Phase 2（需要结果解析器）

---

## Phase 0: 打通数据流 — C++ 统计自动注册 (BLOCKING)

**目标:** 让所有 TLM 模块的 StatGroup 自动注册到 StatsManager，使 StreamingReporter 能够输出非空的 JSON Lines 统计流。

**预计工期:** 1-2 天（仅 C++ 工作，不包含 Python 层）

### 当前问题分析

目前所有 TLM 模块都定义了 StatGroup 成员：

```cpp
// cache_tlm.hh (命名: stats_)
tlm_stats::StatGroup stats_;
tlm_stats::Scalar& stats_requests_;

// router_tlm.hh (命名: stat_group_)  ← 注意命名不一致！
tlm_stats::StatGroup stat_group_;
tlm_stats::Scalar& stats_flits_forwarded_;

// memory_tlm.hh, crossbar_tlm.hh, traffic_gen_tlm.hh, nic_tlm.hh ...
```

但没有任何代码调用 `StatsManager::instance().register_group(&stats_, "system.cache." + name)`。

StreamingReporter 每 N 个周期调用 `StatsManager::instance().dump_json(os)`，但 StatsManager 内部的 `groups_` map 是空的，因此输出为空 JSONL 文件。

### 具体修改步骤

#### Step 0.1: 添加虚拟函数到 ChStreamModuleBase

**文件:** `include/core/chstream_module.hh`

在 `ChStreamModuleBase` 类中添加虚拟函数：

```cpp
/**
 * @brief 获取模块的统计组指针（用于 StatsManager 自动注册）
 * @return StatGroup 指针，无统计则返回 nullptr
 */
virtual tlm_stats::StatGroup* get_stats_group() { return nullptr; }
```

**验证:** 编译通过，不破坏现有模块（默认返回 nullptr）。

#### Step 0.2: 在各 TLM 模块中重写

**注意命名不一致风险:** RouterTLM 和 NICTLM 使用 `stat_group_`，其他使用 `stats_`。

| 模块 | 文件 | 重写代码 |
|------|------|---------|
| CacheTLM | `include/tlm/cache_tlm.hh` | `tlm_stats::StatGroup* get_stats_group() override { return &stats_; }` |
| MemoryTLM | `include/tlm/memory_tlm.hh` | `tlm_stats::StatGroup* get_stats_group() override { return &stats_; }` |
| CrossbarTLM | `include/tlm/crossbar_tlm.hh` | `tlm_stats::StatGroup* get_stats_group() override { return &stats_; }` |
| TrafficGenTLM | `include/tlm/traffic_gen_tlm.hh` | `tlm_stats::StatGroup* get_stats_group() override { return &stats_; }` |
| RouterTLM | `include/tlm/router_tlm.hh` | `tlm_stats::StatGroup* get_stats_group() override { return &stat_group_; }` |
| NICTLM | `include/tlm/nic_tlm.hh` | `tlm_stats::StatGroup* get_stats_group() override { return &stat_group_; }` |

**验证:** 每个修改后执行 `cmake --build build` 确保编译通过。

#### Step 0.3: 在 ModuleFactory 中自动注册

**文件:** `src/core/module_factory.cc`

在 `instantiateAll()` 的 Step 7（StreamAdapter 注入）之后添加 Step 8：

```cpp
// ========================
// 8. 自动注册模块统计组到 StatsManager
// ========================
for (auto& [name, obj] : object_instances) {
    if (!obj) continue;
    
    // 尝试 ChStreamModuleBase 派生类
    if (auto* ch_mod = dynamic_cast<ChStreamModuleBase*>(obj)) {
        if (auto* stat_group = ch_mod->get_stats_group()) {
            std::string path = "system." + name;
            tlm_stats::StatsManager::instance().register_group(stat_group, path);
            DPRINTF(MODULE, "[STATS] Registered StatGroup for '%s' at path '%s'\n",
                    name.c_str(), path.c_str());
        }
    }
}
```

**验证:** 运行仿真测试，确认 StatsManager 中有已注册的组。

#### Step 0.4: 添加注销逻辑

**文件:** `src/core/module_factory.cc`

在 ModuleFactory 的析构函数或清理方法中：

```cpp
// 在析构或 clear() 方法中
for (auto& [name, obj] : object_instances) {
    if (!obj) continue;
    std::string path = "system." + name;
    tlm_stats::StatsManager::instance().unregister_group(path);
}
```

**验证:** 多次运行仿真不泄漏统计组。

#### Step 0.5: 编写验证测试

**文件:** `test/test_phase0_stats_registration.cc` (新建)

```cpp
// 测试：验证模块实例化后 StatsManager 中有注册
TEST_CASE("Phase 0: StatGroup auto-registration", "[phase0][stats]") {
    // 创建最小配置（1 个 Cache + 1 个 Memory）
    // 调用 ModuleFactory::instantiateAll()
    // 验证 StatsManager::find_group() 返回非空
}
```

### Phase 0 验收标准

- [ ] `ctest -R phase0 --output-on-failure` 通过
- [ ] 运行 `cpptlm_sim --stream-stats`，验证 `stats_stream.jsonl` 文件非空
- [ ] 验证 JSONL 文件至少包含一个 `requests` 字段的统计快照
- [ ] 验证 JSONL 文件格式符合附录 A 的 JSONL Schema（包含 `timestamp_ns`, `simulation_cycle`, `group`, `data` 字段）

### Phase 0 集成测试清单

| 测试项 | 验证内容 | 通过标准 |
|--------|---------|---------|
| 编译测试 | `cmake --build build` 无错误 | 编译成功 |
| 单元测试 | `ctest -R phase0` | 100% 通过 |
| 数据流测试 | `cpptlm_sim --stream-stats` | JSONL 非空且含 `requests` |
| 冷/热启动测试 | 连续运行两次仿真 | 两次 JSONL 输出一致（每次独立重置统计） |
| 生命周期测试 | ModuleFactory 析构后重新创建 | 无重复注册/内存泄漏 |

### Phase 0 风险登记

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|---------|
| RouterTLM/NICTLM 命名不一致导致编译错误 | 中 | 高 | 修改前 grep 确认所有 StatGroup 成员名 |
| dynamic_cast 开销在大量模块时显著 | 低 | 低 | 仅在 instantiateAll() 执行一次， negligible |
| StatsManager 路径冲突（同名模块） | 低 | 中 | 使用 `system.{module_name}` 路径，模块名唯一 |
| 内存生命周期问题（StatGroup 在模块销毁前被访问） | 低 | 高 | 确保 ModuleFactory 析构时先 unregister 再 delete 模块 |

---

## Phase 1: Python 配置层增强

**目标:** 修复现有 Python API 问题，增加高层封装，使用户无需直接操作 ConfigBuilder 即可生成标准拓扑。

**预计工期:** 2-3 天（纯 Python 工作，不依赖 C++ 编译，可与 Phase 0 完全并行）

### 当前问题分析

1. **API 不匹配:** `TopologyAdapter.from_mesh()` 调用 `gen.add_mesh(rows, cols)`，但 `TopologyGenerator` 实际方法名为 `generate_mesh(rows, cols)`
2. **缺少配置继承:** `ConfigBuilder` 无 `set_extends()` 方法，无法复用基础配置
3. **缺少高层封装:** 用户必须手动调用 ConfigBuilder + TopologyAdapter + TopologyGenerator
4. **缺少 Python 包发布配置:** 无 `pyproject.toml`，无法通过 pip 安装
5. **缺少依赖版本约束:** networkx/pydot 未指定最低版本

### 具体修改步骤

#### Step 1.1: 修复 TopologyAdapter API 不匹配

**文件:** `cpptlm_config/topology_adapter.py`

将：
```python
gen = TopologyGenerator(name=f"mesh_{rows}x{cols}")
gen.add_mesh(rows, cols)  # ← 错误方法名
```

改为：
```python
gen = TopologyGenerator(name=f"mesh_{rows}x{cols}")
gen.generate_mesh(rows, cols)  # ← 正确方法名
```

同样修复 `from_ring()` 中的 `gen.add_ring(nodes)` → `gen.generate_ring(nodes)`。

**验证:** 运行 `python -c "from cpptlm_config.topology_adapter import TopologyAdapter; ..."` 无 ImportError/AttributeError。

#### Step 1.2: 实现 ConfigBuilder.set_extends()

**文件:** `cpptlm_config/builder.py`

添加配置继承支持：

```python
def set_extends(self, base_config_path: str) -> 'ConfigBuilder':
    """设置基础配置文件路径，build() 时自动合并"""
    self._extends = base_config_path
    return self

def build(self) -> ConfigSchema:
    schema = ConfigSchema(...)
    if hasattr(self, '_extends') and self._extends:
        schema = self._merge_with_base(schema, self._extends)
    return schema
```

**验证:** 编写测试：基础配置 + 子配置覆盖 = 合并后配置正确。

#### Step 1.3: 创建高层拓扑封装类

**文件:** `cpptlm/config/topologies.py` (新建，注意新包结构)

```python
class MeshTopology:
    """Mesh 拓扑高层封装"""
    
    def __init__(self, rows: int, cols: int):
        self.rows = rows
        self.cols = cols
    
    def to_config(self, name: str = None) -> ConfigSchema:
        """生成 ConfigSchema"""
        from cpptlm.config.builder import ConfigBuilder
        from cpptlm.config.topology_adapter import TopologyAdapter
        
        builder = ConfigBuilder(name or f"mesh_{self.rows}x{self.cols}")
        TopologyAdapter.from_mesh(self.rows, self.cols, builder)
        return builder.build()

class RingTopology:
    def __init__(self, nodes: int):
        self.nodes = nodes
    
    def to_config(self, name: str = None) -> ConfigSchema:
        ...

class CrossbarTopology:
    def __init__(self, masters: int, slaves: int):
        self.masters = masters
        self.slaves = slaves
    
    def to_config(self, name: str = None) -> ConfigSchema:
        ...
```

**验证:** 每个类编写单元测试，验证 `to_config()` 输出有效配置。

#### Step 1.4: 重构包结构

将 `cpptlm_config/` 重构为 `cpptlm/config/` 子包：

```
cpptlm/
├── __init__.py
├── config/
│   ├── __init__.py
│   ├── builder.py          (从 cpptlm_config/ 移动)
│   ├── models.py           (从 cpptlm_config/ 移动)
│   ├── types.py            (从 cpptlm_config/ 移动)
│   ├── validator.py        (从 cpptlm_config/ 移动)
│   ├── topology_adapter.py (从 cpptlm_config/ 移动)
│   ├── topologies.py       (新建)
│   └── generator.py        (从 scripts/ 迁移/封装)
```

**注意:** 保留 `cpptlm_config/` 作为兼容 shim（import 转发），避免破坏现有代码。

**验证:** `from cpptlm.config import MeshTopology` 和 `from cpptlm_config.builder import ConfigBuilder` 均工作。

#### Step 1.5: 添加 pyproject.toml 发布配置

**文件:** `pyproject.toml` (新建)

```toml
[build-system]
requires = ["setuptools>=45", "wheel", "setuptools_scm>=6.2"]
build-backend = "setuptools.build_meta"

[project]
name = "cpptlm"
dynamic = ["version"]
description = "Python toolkit for CppTLM simulation framework"
requires-python = ">=3.8"
dependencies = [
    "pydantic>=1.10.0",
    "networkx>=2.6.0",
    "pydot>=1.4.0",
]

[project.optional-dependencies]
dash = ["dash>=2.0.0", "plotly>=5.0.0", "watchdog>=2.0.0"]
dev = ["pytest>=7.0", "pytest-cov", "black", "mypy"]

[project.scripts]
cpptlm-sim = "cpptlm.cli:main"
```

**验证:** `pip install -e .` 成功安装，`python -c "import cpptlm; print(cpptlm.__version__)"` 输出版本号。

#### Step 1.6: 添加依赖版本控制文档

**文件:** `cpptlm/config/__init__.py`

在模块文档字符串中说明 API 版本兼容性：

```python
"""
配置子包

依赖版本要求:
- networkx >= 2.6.0 (generate_mesh/generate_ring API)
- pydot >= 1.4.0 (Graphviz DOT 输出)
- pydantic >= 1.10.0 (配置验证)

networkx API 兼容性说明:
- `generate_mesh()` 使用 `nx.grid_2d_graph()`
- `generate_ring()` 使用 `nx.cycle_graph()`
- 这两个 API 在 networkx 2.6.0+ 稳定可用
"""
```

### Phase 1 验收标准

- [ ] `pytest cpptlm/tests/test_config.py -v` 全部通过（≥5 个测试文件）
- [ ] `MeshTopology(2, 2).to_config().save("/tmp/mesh.json")` 生成有效 JSON
- [ ] 现有 `cpptlm_config` 的 import 方式仍兼容
- [ ] `pip install -e .` 成功安装包
- [ ] `python -c "from cpptlm.config import MeshTopology, RingTopology, CrossbarTopology"` 无错误

### Phase 1 集成测试清单

| 测试项 | 验证内容 | 通过标准 |
|--------|---------|---------|
| API 修复测试 | `TopologyAdapter.from_mesh()` 和 `from_ring()` | 无 AttributeError |
| 封装类测试 | `MeshTopology.to_config()` / `RingTopology.to_config()` / `CrossbarTopology.to_config()` | 输出有效 JSON Schema |
| 配置继承测试 | `ConfigBuilder.set_extends()` + `build()` | 合并逻辑正确 |
| 包安装测试 | `pip install -e .` | 安装成功，import 正常 |
| 向后兼容测试 | `from cpptlm_config.builder import ConfigBuilder` | 仍能工作（含 deprecation warning） |
| 版本约束测试 | `pip install cpptlm[dash]` | dash/plotly/watchdog 正确安装 |

### Phase 1 风险登记

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|---------|
| 包结构重构破坏现有 import | 中 | 高 | 保留 cpptlm_config/ shim，添加 deprecation warning |
| TopologyGenerator 依赖 networkx/pydot 未安装 | 中 | 中 | 在 __init__ 中延迟导入，提供清晰的错误信息 |
| 配置继承合并逻辑与 C++ 层不一致 | 低 | 中 | 复用 C++ 的 mergeConfigs 逻辑，或确保语义一致 |
| pydantic v1/v2 API 不兼容 | 中 | 高 | 指定 `pydantic>=1.10.0,<2.0.0` 或提供 v2 兼容层 |

---

## Phase 2: 仿真运行器

**目标:** 实现 Python 驱动的仿真运行，封装子进程调用、结果解析、系统组合。

**预计工期:** 2-3 天（依赖 Phase 0，可与 Phase 1 并行）

### 具体修改步骤

#### Step 2.1: 实现 SimulationRunner

**文件:** `cpptlm/simulation/runner.py` (新建)

```python
import subprocess
import os

class SimulationRunner:
    """CppTLM 仿真运行器 — 封装 cpptlm_sim 二进制调用"""
    
    def __init__(self, binary_path: str = "build/bin/cpptlm_sim"):
        self.binary_path = binary_path
        self._process = None
    
    def run(self, config_path: str, duration: int = 100000,
            stream_stats: bool = True, output_dir: str = "output") -> 'Result':
        """运行仿真，返回结果对象"""
        
        cmd = [
            self.binary_path,
            "--config", config_path,
            "--duration", str(duration),
        ]
        if stream_stats:
            cmd.extend(["--stream-stats", f"{output_dir}/stats.jsonl"])
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        return Result.from_run(result, output_dir)
    
    def run_async(self, config_path: str, **kwargs):
        """异步运行仿真（返回进程对象）"""
        # TODO: 实现异步运行（返回 asyncio.Future 或 multiprocessing.Process）
        # 当前标记为未实现，计划使用 subprocess.Popen + 轮询机制
        raise NotImplementedError("run_async 尚未实现，计划 Phase 2.2 完成")
```

**验证:** 测试 `SimulationRunner().run("configs/mesh_2x2.json", duration=1000)` 成功返回 Result。

#### Step 2.2: 实现 Result 结果解析器

**文件:** `cpptlm/simulation/result.py` (新建)

```python
import os
import json
from typing import List, Dict

class Result:
    """仿真结果封装"""
    
    def __init__(self, returncode: int, stdout: str, stderr: str,
                 stats_path: str = None):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.stats_path = stats_path
        self._stats_data = None
    
    @classmethod
    def from_run(cls, subprocess_result, output_dir: str) -> 'Result':
        """从 subprocess.CompletedProcess 创建"""
        import os  # 确保 os 已导入
        stats_path = f"{output_dir}/stats.jsonl" if os.path.exists(f"{output_dir}/stats.jsonl") else None
        return cls(
            returncode=subprocess_result.returncode,
            stdout=subprocess_result.stdout,
            stderr=subprocess_result.stderr,
            stats_path=stats_path
        )
    
    @classmethod
    def from_jsonl(cls, path: str) -> 'Result':
        """从已有 JSONL 文件创建（不运行仿真）"""
        return cls(returncode=0, stdout="", stderr="", stats_path=path)
    
    def load_stats(self) -> List[Dict]:
        """加载统计数据（惰性加载）"""
        if self._stats_data is None and self.stats_path:
            self._stats_data = []
            with open(self.stats_path) as f:
                for line in f:
                    if line.strip():
                        self._stats_data.append(json.loads(line))
        return self._stats_data
    
    def get_latest_snapshot(self) -> Dict:
        """获取最后一个统计快照"""
        stats = self.load_stats()
        return stats[-1] if stats else {}
    
    @property
    def success(self) -> bool:
        return self.returncode == 0
```

**验证:** 测试 `Result.from_jsonl("test_data/sample_stats.jsonl").load_stats()` 正确解析。

#### Step 2.3: 实现 System 组合类

**文件:** `cpptlm/simulation/system.py` (新建)

```python
import tempfile
import os

class System:
    """仿真系统组合 — 绑定拓扑 + 负载 + 运行参数"""
    
    def __init__(self, topology: ConfigSchema, 
                 workload: dict = None,
                 duration: int = 100000):
        self.topology = topology
        self.workload = workload or {"type": "uniform_random"}
        self.duration = duration
        self._config_path = None
        self._temp_files = []  # 跟踪临时文件以便清理
    
    def save_config(self, path: str):
        """保存配置到 JSON 文件"""
        self.topology.save(path)
        self._config_path = path
    
    def _cleanup_temp_files(self):
        """清理所有临时文件"""
        for temp_path in self._temp_files:
            try:
                if os.path.exists(temp_path):
                    os.remove(temp_path)
            except OSError:
                pass  # 忽略清理错误
        self._temp_files.clear()
    
    def run(self, runner: SimulationRunner = None, 
            output_dir: str = "output") -> Result:
        """运行完整仿真流程"""
        # 清理之前的临时文件
        self._cleanup_temp_files()
        
        if not self._config_path:
            with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
                self._config_path = f.name
                self._temp_files.append(f.name)
        
        self.save_config(self._config_path)
        runner = runner or SimulationRunner()
        return runner.run(self._config_path, self.duration, output_dir=output_dir)
    
    def __del__(self):
        """析构时清理临时文件"""
        self._cleanup_temp_files()
```

**验证:** 端到端测试：`System(MeshTopology(2,2).to_config()).run()` 成功。

#### Step 2.4: 冷/热启动统计重置策略

**文件:** `cpptlm/simulation/system.py` (在 System 类中添加)

```python
class System:
    # ... 现有代码 ...
    
    def reset_metrics(self):
        """重置统计指标 — 在多次运行之间调用"""
        # 清除上一次的统计缓存
        self._last_result = None
        # 如果 StatsManager 支持重置，调用重置 API
        # 否则依赖 C++ 层每次运行独立初始化
    
    def run(self, runner: SimulationRunner = None, 
            output_dir: str = "output",
            reset_stats: bool = True) -> Result:
        """运行完整仿真流程"""
        if reset_stats:
            self.reset_metrics()
        # ... 现有逻辑 ...
```

**策略说明:**
- **冷启动:** 每次 `System.run()` 默认调用 `reset_metrics()`，确保统计从零开始
- **热启动:** 连续多次运行同一配置时，设置 `reset_stats=False` 保留累积统计
- **临时文件:** 每次运行前清理上一次的临时配置文件

### Phase 2 验收标准

- [ ] `pytest cpptlm/tests/test_simulation.py -v` 全部通过
- [ ] `pytest cpptlm/tests/test_end_to_end.py -v` 端到端集成测试通过
- [ ] 端到端测试：`MeshTopology → Config → run → Result → load_stats` 数据完整
- [ ] `run_async` 标记为 `NotImplementedError`，有明确的 TODO 和实现计划
- [ ] `System.save_config()` 创建的临时文件在 `System` 销毁时自动清理
- [ ] 连续两次运行同一 `System` 实例，统计结果独立（冷启动默认行为）

### Phase 2 集成测试清单

| 测试项 | 验证内容 | 通过标准 |
|--------|---------|---------|
| 运行器单元测试 | `SimulationRunner.run()` 基本调用 | 返回 Result，success=True |
| 结果解析测试 | `Result.from_run()` / `Result.from_jsonl()` | 正确解析 stats.jsonl |
| 惰性加载测试 | `Result.load_stats()` 多次调用 | 只读取一次文件 |
| 系统组合测试 | `System.run()` 完整流程 | 生成有效 JSONL |
| 临时文件清理测试 | `System` 销毁后检查 `/tmp` | 无残留 `.json` 临时文件 |
| 冷启动测试 | 同一 `System` 连续运行两次 | 两次统计值独立 |
| 热启动测试 | `System.run(reset_stats=False)` | 统计累积 |
| 错误处理测试 | `cpptlm_sim` 二进制不存在 | 抛出清晰异常 |
| 端到端测试 | `MeshTopology(2,2) → System → run → ReportGenerator` | 完整链路通过 |

### Phase 2 风险登记

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|---------|
| cpptlm_sim 二进制未编译或路径错误 | 高 | 高 | 自动检测 build/bin/，提供清晰错误信息 |
| 大 JSONL 文件解析内存爆炸 | 中 | 中 | 使用生成器惰性加载，提供分块读取选项 |
| 子进程僵尸进程 | 低 | 中 | 使用 subprocess.run() 超时参数，确保进程终止 |
| 临时文件泄漏 | 中 | 低 | 使用 `_temp_files` 列表跟踪，析构时统一清理 |

---

## Phase 3: 可视化层

**目标:** 将现有独立脚本重构为可复用的 Python 类库，连接真实仿真数据。

**预计工期:** 2-3 天（依赖 Phase 0 和 Phase 2）

### 当前问题分析

- `scripts/stats_watcher.py`: 独立脚本，Dash 依赖可选，但无数据时展示空仪表板
- `scripts/stats_annotator.py`: 独立脚本，生成 HTML 报告，但数据源为空
- 拓扑可视化依赖 Graphviz DOT，与 Python 层无直接连接
- Dash 依赖标记为 optional，但 `PerformanceDashboard` 类直接依赖 Dash

### 具体修改步骤

#### Step 3.1: 重构 PerformanceDashboard（支持 Dash 降级）

**文件:** `cpptlm/visualization/dashboard.py` (新建)

将 `scripts/stats_watcher.py` 重构为类，支持 Dash 不可用时降级为文件输出：

```python
class PerformanceDashboard:
    """性能监控仪表板 — Dash-based 实时可视化
    
    降级策略:
    - 当 dash 不可用时，自动降级为生成静态 HTML 文件
    - 当 plotly 不可用时，降级为纯文本表格输出
    """
    
    def __init__(self, result: Result = None, 
                 stream_path: str = None,
                 port: int = 8050):
        self.result = result
        self.stream_path = stream_path or (result.stats_path if result else None)
        self.port = port
        self.app = None
        self._dash_available = self._check_dash()
    
    def _check_dash(self) -> bool:
        """检查 dash 是否可用"""
        try:
            import dash
            import plotly
            return True
        except ImportError:
            return False
    
    def build_layout(self):
        """构建 Dash 布局（dash 可用时）"""
        if not self._dash_available:
            raise RuntimeError("Dash 不可用，无法构建交互式布局。请安装: pip install cpptlm[dash]")
        # ... Dash 布局代码 ...
    
    def run_server(self, debug: bool = False):
        """启动 Dash 服务器"""
        if not self._dash_available:
            # 降级：生成静态 HTML 文件
            static_html = self.render_static()
            output_path = f"dashboard_{self.result.stats_path or 'output'}.html"
            with open(output_path, 'w') as f:
                f.write(static_html)
            print(f"Dash 不可用，已生成静态报告: {output_path}")
            return
        # ... Dash 服务器启动代码 ...
    
    def render_static(self) -> str:
        """生成静态 HTML（无 Dash 依赖）"""
        # 使用纯 HTML/CSS 生成表格和简单图表
        # 或尝试使用 plotly 的 write_html（如果 plotly 可用）
        stats = self.result.load_stats() if self.result else []
        # ... 生成静态 HTML ...
        return html_content
```

**验证:** 在无 Dash 环境中测试，确认降级为静态 HTML 输出。

#### Step 3.2: 实现 TopologyViewer

**文件:** `cpptlm/visualization/topology_viewer.py` (新建)

```python
class TopologyViewer:
    """拓扑可视化 — 支持 Graphviz DOT 和交互式 HTML"""
    
    def __init__(self, config: ConfigSchema):
        self.config = config
    
    def to_dot(self) -> str:
        """生成 Graphviz DOT 字符串"""
        # 依赖 pydot，已在核心依赖中
        ...
    
    def interactive(self, output_path: str = "topology.html"):
        """生成交互式 HTML（使用 D3.js 或 cytoscape.js）"""
        # 生成独立 HTML 文件，内嵌 D3.js
        ...
    
    def render(self, format: str = "png") -> bytes:
        """渲染为图片（需要 graphviz 二进制）"""
        # 调用 graphviz 将 DOT 转为图片
        ...
```

**验证:** 生成 mesh 拓扑的 DOT 和 HTML，确认可视化正确。

#### Step 3.3: 重构 ReportGenerator

**文件:** `cpptlm/visualization/report.py` (新建)

将 `scripts/stats_annotator.py` 重构为：

```python
class ReportGenerator:
    """HTML 报告生成器"""
    
    def __init__(self, result: Result):
        self.result = result
    
    def generate(self, output_path: str = "report.html",
                 include_topology: bool = True):
        """生成完整 HTML 报告"""
        # 组合统计图表 + 拓扑图 + 配置摘要
        ...
```

**验证:** 生成报告文件，包含统计图表和拓扑图。

#### Step 3.4: 集成示例

**文件:** `cpptlm/examples/mesh_simulation.py` (新建)

```python
from cpptlm.config import MeshTopology
from cpptlm.simulation import System, SimulationRunner
from cpptlm.visualization import PerformanceDashboard, ReportGenerator

# 1. 创建拓扑
topo = MeshTopology(rows=4, cols=4)

# 2. 定义系统
system = System(topology=topo.to_config(), duration=100000)

# 3. 运行仿真
result = system.run()

# 4. 生成报告
ReportGenerator(result).generate("report.html")

# 5. 启动仪表板（可选，Dash 不可用时自动降级）
dashboard = PerformanceDashboard(result)
dashboard.run_server(port=8050)
```

### Phase 3 验收标准

- [ ] `pytest cpptlm/tests/test_visualization.py -v` 全部通过
- [ ] `PerformanceDashboard` 使用真实 JSONL 数据展示图表（Dash 可用时）
- [ ] `PerformanceDashboard` 在无 Dash 环境中降级为静态 HTML（Dash 不可用时）
- [ ] `TopologyViewer.interactive()` 生成可交互的 HTML 拓扑图
- [ ] `ReportGenerator` 生成包含统计和拓扑的完整 HTML 报告

### Phase 3 集成测试清单

| 测试项 | 验证内容 | 通过标准 |
|--------|---------|---------|
| Dash 可用测试 | `PerformanceDashboard` 交互式图表 | 正常显示 |
| Dash 降级测试 | 卸载 dash 后 `PerformanceDashboard.run_server()` | 生成静态 HTML |
| 拓扑 DOT 测试 | `TopologyViewer.to_dot()` | 输出有效 DOT 语法 |
| 拓扑交互测试 | `TopologyViewer.interactive()` | 生成可点击的 HTML |
| 报告生成测试 | `ReportGenerator.generate()` | 输出完整 HTML |
| 真实数据测试 | 使用 Phase 0 的 JSONL 数据 | 图表展示真实统计值 |
| 大数据集测试 | 10MB+ JSONL 文件 | 渲染不卡死 |

### Phase 3 风险登记

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|---------|
| Dash/plotly 未安装 | 高 | 中 | 提供纯静态 HTML 降级方案，标记为 optional dependency |
| 大数据集渲染性能差 | 中 | 中 | 支持数据降采样，分页加载 |
| 浏览器兼容性问题 | 低 | 低 | 使用主流库（D3.js v7+），避免实验性 API |
| graphviz 二进制未安装 | 中 | 中 | `TopologyViewer.render()` 提供清晰错误信息 |

---

## 时间线 (Timeline)

| 阶段 | 工期 | 前置条件 | 关键里程碑 |
|------|------|---------|-----------|
| Phase 0 | 1-2 天 | 无 | StreamingReporter 输出非空 JSONL（含 `requests` 字段） |
| Phase 1 | 2-3 天 | 无（纯 Python，可与 Phase 0 并行） | MeshTopology/RingTopology 封装可用，pip 可安装 |
| Phase 2 | 2-3 天 | Phase 0 完成 | System.run() 端到端通过，临时文件自动清理 |
| Phase 3 | 2-3 天 | Phase 0 + Phase 2 | 可视化仪表板展示真实数据，支持 Dash 降级 |

**总计估计:** 7-11 天（串行）/ 4-6 天（Phase 0 和 Phase 1 并行，Phase 2 依赖 Phase 0，Phase 3 最后）

**资源估算说明:**
- 文件计数仅反映代码规模，实际工期需考虑：
  - 代码审查时间：每个 Phase 增加 0.5 天
  - 调试/集成测试时间：每个 Phase 增加 0.5-1 天
  - 文档同步时间：每个 Phase 增加 0.25 天
- 建议为每个 Phase 预留 20% 缓冲时间

---

## 附录 A: JSONL 输出格式规范

StreamingReporter 输出的 JSON Lines 文件格式如下：

### 行格式

每行一个 JSON 对象，包含以下字段：

```json
{
  "timestamp_ns": 1234567890,
  "simulation_cycle": 1000,
  "group": "system.cache0",
  "data": {
    "requests": 42,
    "hits": 38,
    "misses": 4
  }
}
```

### 字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| `timestamp_ns` | int | Unix 时间戳（纳秒），统计采样时刻 |
| `simulation_cycle` | int | 仿真周期数，对应仿真推进的时钟周期 |
| `group` | string | 统计组路径，格式为 `system.{module_name}` |
| `data` | object | 具体统计指标，键值对形式，模块定义 |

### 约束条件

- 文件扩展名：`.jsonl`（JSON Lines）
- 编码：UTF-8
- 每行必须是一个独立的、有效的 JSON 对象
- 空行应被忽略
- `data` 中的字段由模块的 StatGroup 定义，不强制固定键名
- 至少包含一个模块的统计数据（文件非空）

### 验证方法

```python
import json

def validate_jsonl(path: str) -> bool:
    """验证 JSONL 文件格式"""
    with open(path) as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            obj = json.loads(line)
            assert "timestamp_ns" in obj, f"Line {line_num}: missing timestamp_ns"
            assert "simulation_cycle" in obj, f"Line {line_num}: missing simulation_cycle"
            assert "group" in obj, f"Line {line_num}: missing group"
            assert "data" in obj, f"Line {line_num}: missing data"
            assert isinstance(obj["data"], dict), f"Line {line_num}: data must be object"
    return True
```

---

## 附录 B: 关键代码参考

### C++ 统计注册参考代码

```cpp
// 在 module_factory.cc 的 instantiateAll() Step 7 之后

// 步骤 8: 自动注册统计组
for (auto& [name, obj] : object_instances) {
    if (!obj) continue;
    if (auto* ch_mod = dynamic_cast<ChStreamModuleBase*>(obj)) {
        if (auto* stat_group = ch_mod->get_stats_group()) {
            std::string path = "system." + name;
            tlm_stats::StatsManager::instance().register_group(stat_group, path);
        }
    }
}
```

### Python 端到端使用示例

```python
from cpptlm.config import MeshTopology
from cpptlm.simulation import System
from cpptlm.visualization import ReportGenerator

# 一行代码创建拓扑
topo = MeshTopology(4, 4)

# 一行代码运行仿真
result = System(topo.to_config(), duration=100000).run()

# 一行代码生成报告
ReportGenerator(result).generate("mesh_4x4_report.html")
```

---

*计划创建日期: 2026-05-11*
*修订日期: 2026-05-11*
*基于研究: Oracle 架构分析、Metis 风险评估、现有代码库审查*
*修订依据: Momus 计划审查反馈（问题修复：Phase 0 验收标准、Phase 1 独立性、JSONL Schema、代码质量、Dash 降级、依赖一致性）*
