# CppTLM Python 用户指南

## 目录

1. [环境准备](#1-环境准备)
2. [E2E 性能分析 Demo](#2-e2e-性能分析-demo)
3. [层次化 SoC 配置](#3-层次化-soc-配置)
4. [ConfigBuilder API 参考](#4-configbuilder-api-参考)
5. [统一可视化 Dashboard](#5-统一可视化-dashboard)

---

## 1. 环境准备

### 1.1 编译 C++ 仿真器

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_SYSTEMC=OFF
cmake --build build -j$(nproc)
```

验证二进制可用：

```bash
./build/bin/cpptlm_sim --help
```

### 1.2 Python 依赖

```bash
pip install numpy                  # 统计分析必需
pip install dash plotly            # 可视化 Dashboard（可选）
# pip install matplotlib pillow    # 拓扑 PNG 渲染（可选，有 dot 即可）
```

拓扑可视化依赖系统安装的 Graphviz（不含 matplotlib/Pillow 也可生成 PNG）：

```bash
sudo apt install graphviz          # 提供 dot 命令，用于拓扑图渲染
```

### 1.3 验证安装

```bash
python3 -m pytest cpptlm/tests/ cpptlm/analysis/tests/ -v
# 期望: 所有测试通过
```

---

## 2. E2E SoC 性能建模模板（推荐）

`examples/demo_e2e_soc.py` 是一个端到端的 SoC 性能建模**模板**。
用户应当拷贝此脚本，修改 `build_my_topology()` 来定义自己的 SoC 设计。
脚本要求真实的 C++ 仿真器二进制（不支持 mock 模式）。

### 2.1 快速开始

```bash
# 先编译 C++ 仿真器
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_SYSTEMC=OFF
cmake --build build -j$(nproc)

# 运行单集群 SoC（4 CPU + 4 L1 + Crossbar + 2 Memory）
python3 examples/demo_e2e_soc.py

# 双集群 SoC
python3 examples/demo_e2e_soc.py --dual
```

### 2.2 预期输出

```
  [Step 1/5] Building SoC topology...
  [Config]  configs/single_cluster_soc.json
  [Topology] configs/single_cluster_soc.png    ← 新增：拓扑可视化
  [Topology] DOT:  configs/single_cluster_soc.dot

  [Step 2/5] Running C++ simulation...
  [Simulation] /path/to/cpptlm_sim configs/single_cluster_soc.json ...
  [INFO] Streaming stats enabled (interval: 5000 cycles, output: output/stats_...jsonl)
  [INFO] Running simulation for 10000 cycles...
  [INFO] Simulation finished.
  [Stats]   output/stats_single_cluster_soc.jsonl (2048 bytes)

  [Step 4/5] Analyzing simulation results...

  ── Latency by Group ──
  Group                     Mean      P95      P99  Count
  ───────────────────────── ──────── ──────── ──────── ──────
  system.crossbar            ...
  system.l1_0               ...

  ── Bottlenecks ──
  system.mem0               latency=xxx.xx  severity=high

  [Step 5/5] Generating performance report...
  [Report]  /path/to/reports/single_cluster_soc.html
```

### 2.3 命令行选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `--config PATH` | (自动生成) | 直接使用现有 JSON 配置，跳过拓扑构建 |
| `--cycles N` | 10000 | 仿真运行周期数 |
| `--interval N` | 5000 | 统计报告间隔（周期数） |
| `--dual` | off | 使用双集群 SoC 拓扑 |
| `--generate-only` | off | 仅生成 JSON 配置，不运行仿真 |
| `--binary PATH` | (自动查找) | 指定 cpptlm_sim 二进制路径 |

### 2.4 流水线流程

```
demo_e2e_soc.py (Template)
  │
  ├── 1. ConfigBuilder API 构建拓扑     ← 用户自定义区域
  │    ModuleSpec + ConnectionSpec      修改 build_my_topology()
  │    输出: configs/<name>.json
  │
  ├── 1.5 拓扑可视化                    ← 新增
  │    generate_dot() + dot 渲染
  │    输出: configs/<name>.dot / <name>.png
  │    嵌入 HTML 报告
  │
  ├── 2. C++ 仿真 (subprocess)
  │    cpptlm_sim --stream-stats
  │    输出: output/stats_<name>.jsonl
  │
  ├── 3. 实时监控 (daemon thread)
  │    tail JSONL, 打印 cycle/延迟/请求
  │
  ├── 4. 深度分析
  │    MetricSummary (延迟/吞吐量/命中率)
  │    AnomalyDetector (z-score 异常/瓶颈)
  │    CppStatsAdapter (扁平化 C++ 嵌套格式)
  │
  └── 5. HTML 报告
       ReportGenerator + 拓扑图
       输出: reports/<name>.html
```

### 2.5 作为模板使用

用户应拷贝 `demo_e2e_soc.py` 并修改 `build_my_topology()` 函数：

```python
# my_soc_design.py

from cpptlm_config.builder import ConfigBuilder
from cpptlm_config.models import ModuleSpec, ConnectionSpec

def build_my_topology() -> str:
    """定义自己的 SoC 结构。"""
    b = (
        ConfigBuilder("my_soc", "My custom SoC design")
        .add_module(ModuleSpec(name="cpu_core", type="TrafficGenTLM", params={
            "pattern": "SEQUENTIAL",
            "num_requests": 50000,
            "start_addr": "0x1000",
            "end_addr": "0xFFFF",
        }))
        .add_module(ModuleSpec(name="l1_cache", type="CacheTLM"))
        .add_module(ModuleSpec(name="noc_router", type="CrossbarTLM"))
        .add_module(ModuleSpec(name="dram_ctrl", type="MemoryTLM"))
        .add_connection(ConnectionSpec(src="cpu_core", dst="l1_cache", latency=1))
        .add_connection(ConnectionSpec(src="l1_cache", dst="noc_router.0", latency=5))
        .add_connection(ConnectionSpec(src="noc_router.0", dst="dram_ctrl", latency=20))
    )
    return _save_config(b.build(), "my_soc.json")

def _save_config(schema, filename):
    import os, json
    os.makedirs("configs", exist_ok=True)
    schema.save(f"configs/{filename}")
    return f"configs/{filename}"

# 其余流水线步骤复用 demo_e2e_soc.py 的 `run_simulation()`、`analyze()`、
# `generate_report()` 函数
```

### 2.6 API 方式调用（拆分各步骤）

```python
from cpptlm.simulation.runner import SimulationRunner
from cpptlm.simulation.result import Result
from cpptlm.analysis import MetricSummary, AnomalyDetector
from cpptlm.analysis.adapters import adapt_result

# 1. 运行仿真
runner = SimulationRunner(config_path="configs/my_soc.json")
runner.run_with_stats(stats_output="output/stats.jsonl", interval=50000)
runner.run()

# 2. 加载并适配结果
result = Result.from_jsonl("output/stats.jsonl")
adapted = adapt_result(result)

# 3. 分析
metrics = MetricSummary(adapted)
for group in adapted.groups():
    stats = metrics.latency_statistics(group=group)
    print(f"{group}: mean={stats['mean']:.2f}, p95={stats['p95']:.2f}")

# 4. 瓶颈检测
detector = AnomalyDetector(adapted)
for b in detector.identify_bottlenecks():
    print(f"Bottleneck: {b['group']} ({b['severity']})")
```

### 2.7 已知限制

- **需要 C++ 仿真器二进制**：`demo_e2e_soc.py` 不支持 mock 模式。必须先编译 cpptlm_sim
- **extends 链不支持 include**：C++ `JsonIncluder::processIncludesJson()` 不保留文件目录上下文，导致 `extends` 链中的 `include` 路径解析失败。使用 `extends` 时需将公共模块内联到基配置中

---

## 3. 层次化 SoC 配置

### 3.1 配置层次结构

```
examples/demo_configs/
├── common/
│   └── modules.json          # 公共模块定义（被 include 引用）
├── include_demo.json         # 演示 include 指令
├── soc_cluster.json          # 单集群 CPU + 缓存 + groups
└── dual_cluster_soc.json     # 双集群（extends 继承）
```

### 3.2 运行演示

```bash
# 生成并运行所有层次化配置
python3 examples/demo_hierarchical_configs.py

# 仅生成配置（不运行仿真）
python3 examples/demo_hierarchical_configs.py --generate-only
```

### 3.3 使用 include 指令

`include` 让配置文件复用公共模块定义。路径相对于当前配置文件目录。

```json
{
  "name": "include_demo",
  "include": "common/modules.json",    ← 引用公共模块
  "modules": [
    { "name": "cpu0", "type": "TrafficGenTLM" }
  ],
  "connections": [
    { "src": "cpu0", "dst": "l1_0", "latency": 1 }  ← l1_0 来自 include
  ]
}
```

C++ `JsonIncluder` 自动解析 `include` 字段，合并结果等价于：

```json
{ "modules": [
    { "name": "l1_0", "type": "CacheTLM" },
    { "name": "cpu0", "type": "TrafficGenTLM" }
  ],
  "module_groups": [
    { "name": "l1_caches", "members": ["l1_0", "l1_1"] }
  ]
}
```

Python API:
```python
ConfigBuilder("my_config").set_include("common/modules.json")
```

### 3.4 使用 extends 继承

`extends` 让配置文件继承另一个配置的所有模块和连接，并支持覆盖/新增。

```json
{
  "extends": "examples/demo_configs/soc_cluster.json",  ← 继承基础配置
  "modules": [
    { "name": "xbar", "type": "CrossbarTLM" },          ← 新增模块
    { "name": "mem0", "type": "MemoryTLM" }
  ],
  "connections": [
    { "src": "l1_0", "dst": "xbar.0", "latency": 5 }    ← 新增连接
  ]
}
```

Python API:
```python
ConfigBuilder("dual_cluster").set_extends("base_config.json")
```

### 3.5 使用 module_groups

`module_groups` 定义命名模块组，用于连接展开。

```json
{
  "module_groups": [
    { "name": "cluster_cpus", "members": ["cpu0", "cpu1"] },
    { "name": "memories", "members": ["mem0", "mem1"], "exclude": ["mem1"] }
  ]
}
```

Python API:
```python
builder.add_group("cluster_cpus", ["cpu0", "cpu1"])
builder.add_group("memories", ["mem0", "mem1"], exclude=["mem1"])
```

### 3.6 已知限制

| 限制 | 说明 | 影响 |
|------|------|------|
| `extends` + `include` 不兼容 | C++ `JsonIncluder::processIncludesJson()` 不保留文件目录上下文，链式调用时 include 路径解析失败 | `extends` 链中的配置不能含有 `include` 指令。基配置需将公共模块内联 |
| `include` 路径相对当前文件目录 | include 路径解析依赖运行时文件路径，不支持绝对路径 | 所有 include 路径需相对于配置文件所在目录 |

### 3.7 连接模式前缀

在连接中使用特殊前缀实现批量展开：

| 前缀 | 示例 | 说明 |
|------|------|------|
| `group:` | `"group:cluster_cpus"` | 展开为组内所有成员的连接 |
| `regex:` | `"regex:cpu[0-1]"` | 按正则匹配模块名 |
| 无前缀 | `"cpu0"` | 直接连接单一模块 |

```json
{
  "connections": [
    { "src": "group:cluster_cpus", "dst": "l1_0" },    ← 展开为 cpu0→l1_0, cpu1→l1_0
    { "src": "regex:cpu[1]", "dst": "l1_1" },          ← 展开为 cpu1→l1_1
    { "src": "cpu0", "dst": "l1_0", "latency": 1 }     ← 直接连接
  ]
}
```

---

## 4. ConfigBuilder API 参考

### 4.1 基础用法

```python
from cpptlm_config.builder import ConfigBuilder
from cpptlm_config.models import ModuleSpec, ConnectionSpec

schema = (ConfigBuilder("my_soc", "My SoC Description")
    .add_module(ModuleSpec(name="cpu0", type="TrafficGenTLM", params={...}))
    .add_connection(ConnectionSpec(src="cpu0", dst="l1", latency=1))
    .build()
)
schema.save("config.json")
```

### 4.2 全部方法

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `add_module(spec)` | `ModuleSpec` | `self` | 添加模块 |
| `add_connection(spec)` | `ConnectionSpec` | `self` | 添加连接 |
| `add_group(name, members, exclude=[])` | `str, list[str], list[str]` | `self` | 添加模块组 |
| `set_include(path)` | `str` | `self` | 设置 JSON include 指令 |
| `set_extends(path)` | `str` | `self` | 设置配置继承 |
| `build()` | — | `ConfigSchema` | 构建并返回 schema |

### 4.3 完整示例：层次化 SoC

```python
from cpptlm_config.builder import ConfigBuilder
from cpptlm_config.models import ModuleSpec, ConnectionSpec

# 单集群 SoC：groups + pattern 连接
cluster = (
    ConfigBuilder("soc_cluster", "CPU cluster with groups")
    .add_group("cpus", ["cpu0", "cpu1"])
    .add_module(ModuleSpec(name="cpu0", type="TrafficGenTLM",
                          params={"pattern": "SEQUENTIAL", "num_requests": 5000}))
    .add_module(ModuleSpec(name="cpu1", type="TrafficGenTLM",
                          params={"pattern": "RANDOM", "num_requests": 5000}))
    .add_module(ModuleSpec(name="l1_0", type="CacheTLM"))
    .add_module(ModuleSpec(name="l1_1", type="CacheTLM"))
    .add_connection(ConnectionSpec(src="group:cpus", dst="l1_0", latency=1))
    .add_connection(ConnectionSpec(src="regex:cpu[1]", dst="l1_1", latency=1))
    .build()
)
cluster.save("soc_cluster.json")

# 双集群 SoC：extends 继承
dual = (
    ConfigBuilder("dual_cluster", "Extending base cluster")
    .set_extends("soc_cluster.json")
    .add_group("memories", ["mem0", "mem1"])
    .add_module(ModuleSpec(name="xbar", type="CrossbarTLM"))
    .add_module(ModuleSpec(name="mem0", type="MemoryTLM"))
    .add_module(ModuleSpec(name="mem1", type="MemoryTLM"))
    .add_connection(ConnectionSpec(src="l1_0", dst="xbar.0", latency=5))
    .build()
)
dual.save("dual_cluster.json")
```

---

## 5. 统一可视化 Dashboard

CppTLM 提供统一的 Web Dashboard，支持实时监控仿真运行、查看历史结果、编辑配置和重新运行。

### 5.1 runs/ 目录结构

每次仿真运行在 `runs/` 目录下创建独立子目录：

```
runs/
  run_2026-05-12_143052/
    config.json           # 原始配置
    topology.dot          # DOT 文件
    topology.png          # 渲染后的拓扑图
    stats.jsonl           # 仿真原始数据流
    report.html           # 静态 HTML 报告
    metrics.json          # 解析后的指标摘要
    meta.json             # 运行元信息（时间、参数、版本等）
    pid                   # 仿真进程 PID（运行时存在）
```

### 5.2 启动 Dashboard

**独立启动 Dashboard**（不运行仿真，仅浏览已有结果）：

```bash
python -m cpptlm dashboard
# Starting Dashboard on http://localhost:8050
```

指定端口和 runs 目录：

```bash
python -m cpptlm dashboard --port 9000 --runs-dir /path/to/runs
```

### 5.3 运行仿真并打开 Dashboard

使用 `--dashboard` 选项，仿真结束后 Dashboard 自动切换为静态浏览模式：

```bash
python -m cpptlm run --config my_config.json --dashboard
```

也可在运行过程中实时查看。默认端口 8050。

### 5.4 主页导航

Dashboard 主页列出所有历史运行记录：

```
┌─────────────────────────────────────────────────────┐
│ CppTLM Unified Dashboard            [8050] [? 帮助] │
├─────────────────────────────────────────────────────┤
│  Runs                                               │
│  ┌───────────────────────────────────────────────┐ │
│  │ run_2026-05-12_143052   2026-05-12 14:30:52  │ │
│  │ Cycles: 50000   Status: ● Completed          │ │
│  │ [View] [Re-run] [Delete]                       │ │
│  └───────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

点击 **[View]** 进入单一运行视图，点击 **[Re-run]** 重新运行。

### 5.5 单一运行视图

单一运行视图包含四个 Tab：

| Tab | 内容 |
|-----|------|
| **Topology** | 渲染后的拓扑图（topology.png） |
| **Metrics** | 实时/历史性能指标图表 |
| **Config** | Monaco Editor 编辑配置 JSON |
| **Report** | 静态 HTML 性能报告 |

**实时模式**：仿真仍在运行时，Dashboard 自动轮询（2s 间隔）获取最新统计数据，图表实时更新。

**历史模式**：仿真结束后，显示静态聚合指标和历史图表。

### 5.6 配置编辑与降级

在 **Config** Tab 中：
- 使用 Monaco Editor 直接编辑 JSON 配置
- 修改参数（cycles、interval 等）
- 点击 **Save** 保存配置
- 点击 **Re-run** 重新运行仿真

配置保存后自动覆盖 `runs/<run_id>/config.json`。

### 5.7 重新运行功能

在运行视图点击 **Re-run** 或主页点击 **[Re-run]**：
- 读取当前运行的 `config.json`
- 结合表单参数（cycles、seed 等）重新执行仿真
- 覆盖当前目录，Dashboard 自动切换为实时模式

---

### 5.8 命令行参考

| 命令 | 说明 |
|------|------|
| `python -m cpptlm dashboard` | 启动 Dashboard（浏览已有结果） |
| `python -m cpptlm run --config X --dashboard` | 运行仿真并打开 Dashboard |
| `python -m cpptlm run --config X --generate-only` | 仅生成配置，不运行仿真 |
