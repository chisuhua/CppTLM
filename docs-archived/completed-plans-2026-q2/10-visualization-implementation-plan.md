# 配置生成与性能可视化 — 实施计划

> **版本**: 1.0
> **日期**: 2026-04-22
> **前置文档**: `docs/architecture/09-topology-visualization-pipeline.md`
> **目标**: 实现 Python 拓扑配置生成 + C++ 流式统计导出 + Python 可视化流水线

---

## 一、实施概述

### 1.1 目标

建立完整的**配置生成 → 仿真 → 性能可视化**流水线，支持：

| 组件 | 功能 |
|------|------|
| `topology_generator.py` | 根据拓扑类型自动生成 JSON 配置 + DOT 布局图 |
| `layout_manager.py` | 布局坐标导入/导出，支持手动编辑 |
| `StreamingReporter` (C++) | 运行时流式统计导出（JSON Lines 格式） |
| `stats_watcher.py` | 实时监控统计流 + Web 仪表板 |
| `stats_annotator.py` | 仿真后性能数据标注到布局图 |

### 1.2 工作量估算

| 组件 | 预估工时 | 复杂度 |
|------|---------|--------|
| `topology_generator.py` | 2-3 小时 | 中 |
| `include/metrics/streaming_reporter.hh` | 2-3 小时 | 中 |
| `layout_manager.py` | 1-2 小时 | 低 |
| `stats_watcher.py` + Dashboard | 3-4 小时 | 高 |
| `stats_annotator.py` | 2 小时 | 中 |
| 集成测试 | 2 小时 | 低 |
| **总计** | **12-16 小时** | — |

### 1.3 实施策略

**Phase 驱动开发**，每个 Phase 独立可验证：

```
Phase A: Python 拓扑生成器
Phase B: C++ 流式统计导出
Phase C: Python 实时监控 + Dashboard
Phase D: Python 性能标注工具
Phase E: 集成测试 + 文档更新
```

---

## 二、Phase 分解

### Phase A: Python 拓扑生成器

**目标**: 实现 `topology_generator.py`，支持 Mesh/Ring/Bus/Hierarchical 拓扑自动生成

**预估工时**: 2-3 小时

**前置条件**: 无

---

#### A.1 创建基础框架

**文件**: `scripts/topology_generator.py`

**核心类**:

```python
import argparse
import json
import networkx as nx
from typing import Dict, List, Tuple, Optional

class TopologyGenerator:
    def __init__(self, name: str = "topology"):
        self.graph = nx.DiGraph()
        self.graph.name = name
        self.layout_coords = {}
    
    # ========== 拓扑生成方法 ==========
    
    def generate_mesh(self, rows: int, cols: int, router_type: str = "MeshRouter") -> 'TopologyGenerator':
        """生成 2D Mesh NoC"""
        # 1. 创建 Router 节点
        for r in range(rows):
            for c in range(cols):
                node_id = f"router_{r}_{c}"
                self.graph.add_node(node_id, type=router_type)
                self.layout_coords[node_id] = (c * 100, -r * 100)
        
        # 2. 创建网格连接 (XY 路由)
        for r in range(rows):
            for c in range(cols):
                if c < cols - 1:
                    self.graph.add_edge(f"router_{r}_{c}", f"router_{r}_{c+1}",
                                        latency=1, bandwidth=100)
                if r < rows - 1:
                    self.graph.add_edge(f"router_{r}_{c}", f"router_{r+1}_{c}",
                                        latency=1, bandwidth=100)
        
        # 3. 添加 NI (Network Interface)
        for r in range(rows):
            for c in range(cols):
                ni_id = f"ni_{r}_{c}"
                self.graph.add_node(ni_id, type="NetworkInterface")
                self.graph.add_edge(ni_id, f"router_{r}_{c}", latency=0)
                self.layout_coords[ni_id] = (c * 100 + 30, -r * 100 - 30)
        
        return self
    
    def generate_ring(self, num_nodes: int) -> 'TopologyGenerator':
        """生成 Ring 总线拓扑"""
        for i in range(num_nodes):
            node_id = f"node_{i}"
            self.graph.add_node(node_id, type="Processor")
            angle = 2 * 3.14159 * i / num_nodes
            radius = 150
            self.layout_coords[node_id] = (radius * (1 - 2 * i / num_nodes),
                                           radius * 0.5)
        
        for i in range(num_nodes):
            self.graph.add_edge(f"node_{i}", f"node_{(i+1) % num_nodes}",
                               latency=2, bandwidth=100)
        
        return self
    
    def generate_bus(self, num_nodes: int) -> 'TopologyGenerator':
        """生成共享总线拓扑"""
        # 总线节点
        self.graph.add_node("bus", type="Bus")
        self.layout_coords["bus"] = (250, 0)
        
        # 连接的节点
        for i in range(num_nodes):
            node_id = f"node_{i}"
            self.graph.add_node(node_id, type="Processor")
            self.layout_coords[node_id] = (50 + i * 50, 100)
            self.graph.add_edge(node_id, "bus", latency=1, bandwidth=10)
            self.graph.add_edge("bus", node_id, latency=1, bandwidth=10)
        
        return self
    
    def generate_hierarchical(self, levels: int, factor: int) -> 'TopologyGenerator':
        """生成多层级树状拓扑"""
        if levels < 1:
            return self
        
        # Level 0: 根节点
        self.graph.add_node("root", type="Router")
        self.layout_coords["root"] = (250, 0)
        
        current_level_nodes = ["root"]
        
        for level in range(1, levels + 1):
            next_level_nodes = []
            y = -level * 100
            
            for i, parent in enumerate(current_level_nodes):
                for j in range(factor):
                    node_id = f"l{level}_n{i*f}_{j}"
                    self.graph.add_node(node_id, type="Router" if level < levels else "Processor")
                    x = 50 + i * factor * (500 / (len(current_level_nodes) * factor)) + j * 30
                    self.layout_coords[node_id] = (x, y)
                    
                    self.graph.add_edge(parent, node_id, latency=1, bandwidth=100)
                    self.graph.add_edge(node_id, parent, latency=1, bandwidth=100)
                    next_level_nodes.append(node_id)
            
            current_level_nodes = next_level_nodes
        
        return self
    
    # ========== 导入/导出方法 ==========
    
    def export_json_config(self) -> Dict:
        """导出为 CppTLM JSON 配置格式"""
        modules = []
        for node, attrs in self.graph.nodes(data=True):
            modules.append({
                "name": node,
                "type": attrs.get("type", "Unknown")
            })
        
        connections = []
        for src, dst, attrs in self.graph.edges(data=True):
            connections.append({
                "src": src,
                "dst": dst,
                "latency": attrs.get("latency", 1),
                "bandwidth": attrs.get("bandwidth", 100)
            })
        
        return {
            "name": self.graph.name or "generated_topology",
            "modules": modules,
            "connections": connections
        }
    
    def export_layout(self) -> Dict:
        """导出布局坐标"""
        return {
            "version": "1.0",
            "nodes": {
                node: {"x": float(coords[0]), "y": float(coords[1])}
                for node, coords in self.layout_coords.items()
            }
        }
    
    def import_layout(self, layout_json: Dict):
        """导入布局坐标"""
        for node, pos in layout_json.get("nodes", {}).items():
            if node in self.layout_coords:
                self.layout_coords[node] = (pos["x"], pos["y"])
    
    def apply_networkx_layout(self, algo: str = "kamada_kawai"):
        """应用 NetworkX 自动布局算法"""
        layouts = {
            "spring": nx.spring_layout,
            "circular": nx.circular_layout,
            "kamada_kawai": nx.kamada_kawai_layout,
            "shell": nx.shell_layout,
            "spectral": nx.spectral_layout,
            "random": nx.random_layout,
        }
        
        if algo in layouts:
            pos = layouts[algo](self.graph)
            self.layout_coords = {node: tuple(v) for node, v in pos.items()}
    
    def export_dot(self, filepath: str, title: Optional[str] = None):
        """导出为 DOT 文件"""
        import pydot
        
        # 创建 pydot 图
        dot_graph = pydot.Dot(graph_type='digraph')
        dot_graph.set_rankdir('LR')
        dot_graph.set_label(title or self.graph.name)
        
        # 添加节点
        for node, attrs in self.graph.nodes(data=True):
            node_type = attrs.get("type", "Unknown")
            label = f"{node}\n({node_type})"
            
            # 根据类型设置节点样式
            if "Router" in node_type:
                style = "filled,bold"
                fillcolor = "#E3F2FD"
            elif "NI" in node_type or "Interface" in node_type:
                style = "filled"
                fillcolor = "#FFF3E0"
            elif "Processor" in node_type or "CPU" in node_type:
                style = "filled,bold"
                fillcolor = "#E8F5E9"
            else:
                style = "filled"
                fillcolor = "#FAFAFA"
            
            pydot_node = pydot.Node(node, label=label,
                                   shape="box", style=style,
                                   fillcolor=fillcolor)
            dot_graph.add_node(pydot_node)
        
        # 添加边
        for src, dst, attrs in self.graph.edges(data=True):
            latency = attrs.get("latency", 1)
            bandwidth = attrs.get("bandwidth", 100)
            label = f"{latency}cy\n{Bandwidth}GB/s"
            
            pydot_edge = pydot.Edge(src, dst, label=label,
                                   fontsize=9, color="#333333")
            dot_graph.add_edge(pydot_edge)
        
        dot_graph.write(filepath, format='dot')


def main():
    parser = argparse.ArgumentParser(description="CppTLM Topology Generator")
    parser.add_argument("--type", "-t", required=True,
                       choices=["mesh", "ring", "bus", "hierarchical"],
                       help="Topology type")
    parser.add_argument("--size", "-s", default="4x4",
                       help="Mesh size, e.g., 4x4 (for mesh type)")
    parser.add_argument("--nodes", "-n", type=int, default=8,
                       help="Number of nodes (for ring/bus type)")
    parser.add_argument("--levels", "-l", type=int, default=3,
                       help="Number of levels (for hierarchical type)")
    parser.add_argument("--factor", "-f", type=int, default=2,
                       help="Branching factor (for hierarchical type)")
    parser.add_argument("--output", "-o", required=True,
                       help="Output JSON config file")
    parser.add_argument("--layout", default=None,
                       help="Output layout JSON file")
    parser.add_argument("--dot", default=None,
                       help="Output DOT file")
    parser.add_argument("--layout-algo", default="kamada_kawai",
                       choices=["spring", "circular", "kamada_kawai", "shell", "spectral"],
                       help="Layout algorithm")
    
    args = parser.parse_args()
    
    # 创建生成器
    gen = TopologyGenerator(name=f"{args.type}_topology")
    
    # 根据类型生成拓扑
    if args.type == "mesh":
        rows, cols = map(int, args.size.split('x'))
        gen.generate_mesh(rows, cols)
    elif args.type == "ring":
        gen.generate_ring(args.nodes)
    elif args.type == "bus":
        gen.generate_bus(args.nodes)
    elif args.type == "hierarchical":
        gen.generate_hierarchical(args.levels, args.factor)
    
    # 应用布局算法
    if args.layout_algo:
        gen.apply_networkx_layout(args.layout_algo)
    
    # 导出配置
    config = gen.export_json_config()
    with open(args.output, 'w') as f:
        json.dump(config, f, indent=2, ensure_ascii=False)
    print(f"[OK] Config saved to {args.output}")
    
    # 导出布局
    if args.layout:
        layout = gen.export_layout()
        with open(args.layout, 'w') as f:
            json.dump(layout, f, indent=2)
        print(f"[OK] Layout saved to {args.layout}")
    
    # 导出 DOT
    if args.dot:
        gen.export_dot(args.dot)
        print(f"[OK] DOT saved to {args.dot}")


if __name__ == "__main__":
    main()
```

---

#### A.2 创建 layout_manager.py

**文件**: `scripts/layout_manager.py`

```python
#!/usr/bin/env python3
"""
layout_manager.py — 布局坐标的导入导出

功能:
  - 布局坐标的导入/导出 (JSON)
  - DOT 属性注入（固定节点位置）
  - SVG 生成（可导入 Inkscape 编辑）
"""

import argparse
import json
from typing import Dict, Tuple, Optional

class LayoutManager:
    def __init__(self, layout_file: str = None):
        self.layouts = {}
        if layout_file:
            self.load(layout_file)
    
    def load(self, filepath: str):
        with open(filepath, 'r') as f:
            self.layouts = json.load(f)
        print(f"[OK] Loaded {len(self.layouts.get('nodes', {}))} node positions")
    
    def save(self, filepath: str):
        with open(filepath, 'w') as f:
            json.dump(self.layouts, f, indent=2)
        print(f"[OK] Saved layout to {filepath}")
    
    def set_position(self, node: str, x: float, y: float):
        if node not in self.layouts.get("nodes", {}):
            if "nodes" not in self.layouts:
                self.layouts["nodes"] = {}
            self.layouts["nodes"][node] = {}
        self.layouts["nodes"][node]["x"] = x
        self.layouts["nodes"][node]["y"] = y
    
    def get_position(self, node: str) -> Tuple[float, float]:
        pos = self.layouts.get("nodes", {}).get(node, {})
        return pos.get("x", 0), pos.get("y", 0)
    
    def to_dot_attributes(self) -> Dict[str, Dict]:
        """生成 DOT 的 pos 属性"""
        attrs = {}
        for node, pos in self.layouts.get("nodes", {}).items():
            # Graphviz 使用 "x,y" 格式
            attrs[node] = {"pos": f'{pos["x"]},{pos["y"]}'}
        return attrs
    
    def export_dot_with_positions(self, dot_input: str, dot_output: str):
        """读取 DOT 文件，注入位置属性，输出新 DOT"""
        with open(dot_input, 'r') as f:
            content = f.read()
        
        attrs = self.to_dot_attributes()
        
        # 简单的节点位置注入（实际应用中可能需要更 robust 的 DOT 解析）
        for node, attr in attrs.items():
            # 查找节点定义行，注入 pos 属性
            # 例如: cache [label="cache\n(CacheTLM)"];
            # 变为: cache [label="cache\n(CacheTLM)", pos="100,200"];
            import re
            pattern = rf'({node})\s*\['
            replacement = f'{node} [pos="{attr["pos"]}", '
            content = re.sub(pattern, replacement, content)
        
        with open(dot_output, 'w') as f:
            f.write(content)
        
        print(f"[OK] DOT with positions saved to {dot_output}")


def main():
    parser = argparse.ArgumentParser(description="CppTLM Layout Manager")
    parser.add_argument("--load", "-l", help="Load layout JSON file")
    parser.add_argument("--save", "-s", help="Save layout JSON file")
    parser.add_argument("--set", nargs=3, metavar=("NODE", "X", "Y"),
                       help="Set node position, e.g., --set router_0_0 100 200")
    parser.add_argument("--dot-in", help="Input DOT file")
    parser.add_argument("--dot-out", help="Output DOT file with positions")
    
    args = parser.parse_args()
    
    manager = LayoutManager(args.load) if args.load else LayoutManager()
    
    if args.set:
        node, x, y = args.set
        manager.set_position(node, float(x), float(y))
        print(f"[OK] Set {node} position to ({x}, {y})")
    
    if args.save:
        manager.save(args.save)
    
    if args.dot_in and args.dot_out:
        manager.export_dot_with_positions(args.dot_in, args.dot_out)


if __name__ == "__main__":
    main()
```

---

#### A.3 创建 layout_to_dot.py

**文件**: `scripts/layout_to_dot.py`

```python
#!/usr/bin/env python3
"""
layout_to_dot.py — 将布局 JSON 转换为带位置的 DOT 文件

用法:
  python layout_to_dot.py \
      --layout configs/mesh_4x4_layout.json \
      --dot configs/mesh_4x4.dot \
      --output configs/mesh_4x4_with_pos.dot
"""

import argparse
import json
import networkx as nx
import pydot

def main():
    parser = argparse.ArgumentParser(description="Layout to DOT converter")
    parser.add_argument("--layout", "-l", required=True, help="Layout JSON file")
    parser.add_argument("--dot", "-d", help="Optional: DOT template file")
    parser.add_argument("--output", "-o", required=True, help="Output DOT file")
    parser.add_argument("--graph-name", default="simulation", help="Graph name")
    
    args = parser.parse_args()
    
    # 加载布局
    with open(args.layout, 'r') as f:
        layout_data = json.load(f)
    
    positions = {
        node: (data["x"], data["y"])
        for node, data in layout_data.get("nodes", {}).items()
    }
    
    # 创建图
    if args.dot:
        # 从模板 DOT 读取
        (graphs,) = pydot.graph_from_dot_file(args.dot)
        graph = graphs
    else:
        # 创建新图
        graph = pydot.Dot(graph_type='digraph')
        graph.set_rankdir('LR')
    
    graph.set_name(args.graph_name)
    
    # 更新节点位置
    for node in graph.get_nodes():
        name = node.get_name()
        if name in positions:
            x, y = positions[name]
            node.set("pos", f"{x},{y}")
            node.set("fixed", "true")
    
    graph.write(args.output, format='dot')
    print(f"[OK] DOT with positions saved to {args.output}")


if __name__ == "__main__":
    main()
```

---

#### A.4 验收标准

```
□ topology_generator.py 支持 mesh/ring/bus/hierarchical 四种拓扑
□ topology_generator.py --type mesh --size 4x4 -o configs/mesh_4x4.json
□ 生成 configs/mesh_4x4_layout.json 包含所有节点坐标
□ 生成 configs/mesh_4x4.dot 可用 graphviz 渲染
□ layout_manager.py 可导入/导出布局 JSON
□ layout_manager.py --dot-in mesh_4x4.dot --dot-out mesh_fixed.dot 注入坐标
□ Python 依赖: networkx, pydot (已验证)
```

---

### Phase B: C++ 流式统计导出

**目标**: 实现 `StreamingReporter`，支持运行时统计流式输出

**预估工时**: 2-3 小时

**前置条件**: Phase A 完成

---

#### B.1 实现 StreamingReporter

**文件**: `include/metrics/streaming_reporter.hh`

**设计要点**:

1. **后台线程**: 独立输出线程，避免阻塞仿真
2. **JSON Lines 格式**: 每行一个 JSON 对象，便于流式解析
3. **非阻塞入队**: `enqueue_snapshot()` 线程安全
4. **可配置间隔**: `set_interval(Counter)` 设置输出频率

**核心接口**:

```cpp
class StreamingReporter {
public:
    // 构造: 指定输出文件路径和刷新间隔(毫秒)
    explicit StreamingReporter(const std::string& path, 
                               int flush_interval_ms = 500);
    
    // 设置输出间隔（周期数）
    void set_interval(Counter cycles);
    
    // 设置当前仿真周期
    void set_current_cycle(Counter cycle);
    
    // 启动/停止后台输出线程
    void start();
    void stop();
    
    // 入队单个统计组快照（非阻塞）
    void enqueue_snapshot(const std::string& group_name, StatGroup* group);
    
    // 入队所有统计
    void enqueue_all();
    
    // 注册监控的模块（空=全部监控）
    void watch_module(const std::string& module_name);
};
```

**输出格式**:

```jsonl
{"timestamp_ns":1713769234567890123,"simulation_cycle":10000,"group":"system.cache","data":{"hits":95000,"misses":5000}}
{"timestamp_ns":1713769234568000000,"simulation_cycle":10000,"group":"system.xbar","data":{"throughput":8.5}}
...
```

**命令行参数扩展**:

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `--stream-stats` | bool | false | 启用流式统计 |
| `--stream-interval` | int | 10000 | 输出间隔（周期） |
| `--stream-path` | string | `output/stats_stream.jsonl` | 输出文件路径 |

---

#### B.2 集成到仿真主程序

**修改文件**: `src/main.cpp`

```cpp
// 在 main() 中添加:
std::unique_ptr<tlm_stats::StreamingReporter> reporter;

if (FLAGS_stream_stats) {
    reporter = std::make_unique<tlm_stats::StreamingReporter>(
        FLAGS_stream_path, FLAGS_stream_flush_ms);
    reporter->set_interval(FLAGS_stream_interval);
    reporter->start();
}

// 仿真循环
for (Counter cycle = 0; cycle < max_cycles; ++cycle) {
    simulate_cycle(cycle);
    
    if (reporter && cycle % reporter->get_interval() == 0) {
        reporter->set_current_cycle(cycle);
        reporter->enqueue_all();
    }
}

if (reporter) {
    reporter->stop();
}
```

---

#### B.3 验收标准

```
□ StreamingReporter 编译通过
□ --stream-stats --stream-interval 10000 --stream-path output/stats.jsonl
□ 仿真运行期间 stats_stream.jsonl 文件实时增长
□ 仿真结束后文件包含完整的统计快照序列
□ 多次运行不覆盖（追加模式 or 生成唯一文件名）
□ 后台输出线程不影响仿真性能（<5% 开销）
```

---

### Phase C: Python 实时监控 + Dashboard

**目标**: 实现 `stats_watcher.py` 和 Web 仪表板

**预估工时**: 3-4 小时

**前置条件**: Phase B 完成（流式输出就绪）

---

#### C.1 实现 stats_watcher.py

**文件**: `scripts/stats_watcher.py`

**核心组件**:

1. **StatsStreamHandler**: 使用 `watchdog` 监控文件变化
2. **StatsAggregator**: 增量解析 JSON Lines，维护历史数据
3. **DashboardServer**: Dash Web 应用

**StatsAggregator 数据结构**:

```python
class StatsAggregator:
    def __init__(self):
        self.history = defaultdict(list)  # metric_path -> [{timestamp, cycle, value}, ...]
        self.latest = {}                   # metric_path -> latest_value
        self.lock = threading.Lock()
    
    def add_snapshot(self, record: Dict):
        timestamp = record["timestamp_ns"] / 1e9
        cycle = record["simulation_cycle"]
        self._flatten_and_store(record["data"], timestamp, cycle)
    
    def _flatten_and_store(self, data: Dict, timestamp: float, cycle: int, prefix: str = ""):
        for key, value in data.items():
            path = f"{prefix}.{key}" if prefix else key
            if isinstance(value, dict):
                self._flatten_and_store(value, timestamp, cycle, path)
            else:
                self.history[path].append({
                    "timestamp": timestamp,
                    "cycle": cycle,
                    "value": value
                })
                self.latest[path] = value
```

**DashboardServer 布局**:

```python
class DashboardServer:
    def _setup_layout(self):
        return html.Div([
            html.H1("CppTLM Real-Time Performance Dashboard"),
            
            # 实时指标卡片区域
            html.Div(id="metrics-cards"),
            
            # 延迟分布图
            dcc.Graph(id="latency-chart"),
            
            # 吞吐量时间线
            dcc.Graph(id="throughput-chart"),
            
            # 实时刷新组件
            dcc.Interval(
                id="refresh-interval",
                interval=1 * 1000,  # 每秒刷新
                n_intervals=0
            )
        ])
    
    def _setup_callbacks(self):
        # 每秒更新指标卡片
        @self.app.callback(
            Output("metrics-cards", "children"),
            Input("refresh-interval", "n_intervals")
        )
        def update_metrics(n):
            latest = self.aggregator.get_latest()
            # 生成 HTML 指标卡片
            ...
        
        # 更新延迟图
        @self.app.callback(
            Output("latency-chart", "figure"),
            Input("refresh-interval", "n_intervals")
        )
        def update_latency_chart(n):
            hist = self.aggregator.get_history("*.latency.avg", limit=100)
            # 生成 Plotly 图表
            ...
```

---

#### C.2 命令行参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--stream` | `output/stats_stream.jsonl` | 监控的流文件 |
| `--port` | `8050` | Dashboard 端口 |
| `--no-gui` | `False` | 仅控制台输出模式 |

---

#### C.3 验收标准

```
□ stats_watcher.py --stream output/stats_stream.jsonl --port 8050
□ 浏览器访问 http://localhost:8050 显示实时仪表板
□ 指标卡片每秒刷新（显示最新值）
□ 延迟图表显示时间序列
□ --no-gui 模式控制台输出实时数值
□ Ctrl+C 优雅退出
□ 文件不存在时等待，不报错
```

---

### Phase D: Python 性能标注工具

**目标**: 实现 `stats_annotator.py`，将性能数据标注到布局图

**预估工时**: 2 小时

**前置条件**: Phase A, B 完成

---

#### D.1 实现 stats_annotator.py

**文件**: `scripts/stats_annotator.py`

```python
class StatsAnnotator:
    def __init__(self, config_path: str, stats_path: str, layout_path: str):
        # 加载配置、统计、布局
        ...
    
    def generate_html_report(self) -> str:
        """生成带性能标注的交互式 HTML 报告"""
        html = """
<!DOCTYPE html>
<html>
<head>
    <title>CppTLM Performance Report</title>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
</head>
<body>
    <h1>CppTLM Simulation Performance Report</h1>
    
    <h2>Module Statistics</h2>
    <table>
    <tr><th>Module</th><th>Metric</th><th>Value</th></tr>
    {module_rows}
    </table>
    
    <h2>Performance Distribution</h2>
    <div id="latency-chart"></div>
    
    <script>
        // 性能图表
    </script>
</body>
</html>
"""
        ...
        return html
    
    def generate_annotated_dot(self) -> str:
        """生成带性能标注的 DOT 文件"""
        lines = [
            "digraph simulation {",
            '    rankdir=LR;',
            '    node [shape=box, style="rounded,filled"];',
            '    edge [color="#333333"];',
            ''
        ]
        
        # 节点
        for module in self.config.get("modules", []):
            name = module["name"]
            stats = self._get_module_stats(name)
            
            attrs = [f'label="{name}"']
            if stats:
                attrs.append(f'xlabel="hits: {stats.get("hits", 0)}"')
                if "latency" in stats:
                    attrs.append(f'tooltip="latency: {stats["latency"]} cy"')
            
            lines.append(f'    {name} [{" + ", ".join(attrs)}];')
        
        # 边
        for conn in self.config.get("connections", []):
            src, dst = conn["src"], conn["dst"]
            latency = conn.get("latency", 1)
            lines.append(f'    {src} -> {dst} [label="{latency}cy"];')
        
        lines.append("}")
        return '\n'.join(lines)
    
    def export_html(self, output_path: str):
        with open(output_path, 'w') as f:
            f.write(self.generate_html_report())
```

---

#### D.2 命令行参数

| 参数 | 说明 |
|------|------|
| `--config` | 仿真配置文件 (JSON) |
| `--stats` | 统计输出文件 (JSON or JSONL) |
| `--layout` | 布局坐标文件 (JSON) |
| `--output` | 输出 HTML 报告 |

---

#### D.3 验收标准

```
□ stats_annotator.py 生成 HTML 报告包含统计表格
□ HTML 中包含 Plotly 图表（延迟分布、吞吐量）
□ DOT 输出包含性能标注（节点 label、边 label）
□ 正确解析 JSON Lines 格式（流式统计）
□ 正确解析标准 JSON 格式（最终统计）
```

---

### Phase E: 集成测试 + 文档更新

**目标**: 完整流水线验证 + 文档同步

**预估工时**: 2 小时

**前置条件**: Phase A-D 全部完成

---

#### E.1 创建集成测试脚本

**文件**: `scripts/run_full_pipeline.sh`

```bash
#!/bin/bash
# run_full_pipeline.sh — 完整的 生成 → 仿真 → 可视化 流程

set -e

TOPOLOGY_TYPE=${1:-mesh}
SIZE=${2:-4x4}
OUTPUT_DIR=${3:-output}

mkdir -p "$OUTPUT_DIR"

# 1. Python 前置：生成配置和布局
echo "[1/5] Generating topology..."
python3 scripts/topology_generator.py \
    --type $TOPOLOGY_TYPE --size ${SIZE}x${SIZE} \
    --output "$OUTPUT_DIR/topology.json" \
    --layout "$OUTPUT_DIR/topology_layout.json" \
    --dot "$OUTPUT_DIR/topology.dot"

# 2. 运行 C++ 仿真（流式输出）
echo "[2/5] Running simulation..."
./build/bin/cpptlm_sim \
    --config "$OUTPUT_DIR/topology.json" \
    --stream-stats \
    --stream-interval 5000 \
    --stream-path "$OUTPUT_DIR/stats_stream.jsonl"

# 3. 启动 Python 监控（后台）
echo "[3/5] Starting stats watcher..."
python3 scripts/stats_watcher.py \
    --stream "$OUTPUT_DIR/stats_stream.jsonl" \
    --port 8050 &
WATCHER_PID=$!

# 4. 等待仿真完成
wait
sleep 2

# 5. 生成最终报告
echo "[4/5] Generating annotated report..."
python3 scripts/stats_annotator.py \
    --config "$OUTPUT_DIR/topology.json" \
    --stats "$OUTPUT_DIR/stats_stream.jsonl" \
    --layout "$OUTPUT_DIR/topology_layout.json" \
    --output "$OUTPUT_DIR/report.html"

# 停止 watcher
kill $WATCHER_PID 2>/dev/null || true

echo "[5/5] Done!"
echo "  - Report: $OUTPUT_DIR/report.html"
echo "  - Stats stream: $OUTPUT_DIR/stats_stream.jsonl"
```

---

#### E.2 Python 依赖验证

**文件**: `requirements-visualization.txt`

```txt
networkx>=3.0
pydot>=1.4.2
watchdog>=3.0
dash>=2.14.0
plotly>=5.18.0
```

```bash
pip install -r requirements-visualization.txt
```

---

#### E.3 文档更新

| 文档 | 变更 |
|------|------|
| `docs/README.md` | 添加 "配置与可视化" 章节 |
| `docs/architecture/README.md` | 添加架构文档索引 |
| `plans/README.md` | 添加本计划索引 |
| `AGENTS.md` | 添加可视化流水线相关约定 |

---

#### E.4 验收标准

```
□ run_full_pipeline.sh mesh 4x4 输出完整报告
□ stats_stream.jsonl 包含至少 10 个快照
□ report.html 可用浏览器打开，显示统计表格
□ Dashboard http://localhost:8050 实时更新
□ 文档已更新
```

---

## 三、实施顺序与依赖

```
Phase A: Python 拓扑生成器 (2-3h)
   │
   ├─→ Phase B: C++ 流式统计导出 (2-3h)
   │       │
   │       └─→ Phase C: Python 实时监控 (3-4h) ──→ Phase E: 集成测试
   │
   └─→ Phase D: Python 性能标注 (2h) ─────────────────────→ Phase E: 集成测试
                                                            │
                                                            ▼
                                                    ✅ 完整流水线验证
```

---

## 四、风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| Dash Dashboard 在无头环境崩溃 | 低 | 中 | 提供 `--no-gui` 控制台模式 |
| JSON Lines 解析性能问题 | 低 | 低 | 使用流式解析，不加载到内存 |
| Graphviz 布局在极端规模下发散 | 中 | 低 | 提供多种布局算法选择 |
| C++ StreamingReporter 线程开销 | 中 | 低 | 可选的 --stream-stats 开关 |

---

## 五、验收标准汇总

### 功能验收

```
✅ topology_generator.py 支持 4 种拓扑类型
✅ layout_manager.py 支持布局导入/导出
✅ StreamingReporter 实时输出 JSON Lines
✅ stats_watcher.py 监控文件变化并聚合
✅ DashboardServer Web 界面实时更新
✅ stats_annotator.py 生成带标注的 HTML 报告
```

### 性能验收

```
✅ 流式输出不影响仿真性能（<5% 开销）
✅ Dashboard 刷新延迟 <1 秒
✅ 支持至少 10000 个节点的拓扑（NetworkX 约束）
```

### 文档验收

```
✅ docs/architecture/09-topology-visualization-pipeline.md 已创建
✅ plans/10-visualization-implementation-plan.md 已创建
✅ AGENTS.md 已更新
✅ Python 依赖已记录
```

---

## 六、文件清单

```
scripts/
├── topology_generator.py      # [NEW] 拓扑生成器
├── layout_manager.py          # [NEW] 布局管理器
├── layout_to_dot.py           # [NEW] 布局转 DOT
├── stats_watcher.py           # [NEW] 统计监控器
├── stats_annotator.py         # [NEW] 性能标注工具
└── run_full_pipeline.sh       # [NEW] 集成测试脚本

include/metrics/
└── streaming_reporter.hh      # [NEW] C++ 流式导出

docs/
├── architecture/
│   └── 09-topology-visualization-pipeline.md  # [NEW] 架构文档
└── plans/
    └── 10-visualization-implementation-plan.md # [NEW] 本计划

requirements-visualization.txt   # [NEW] Python 依赖
```

---

**文档状态**: ✅ 完整
**可执行状态**: 待用户确认

---

**维护**: CppTLM 开发团队
**版本**: 1.0
**最后更新**: 2026-04-22