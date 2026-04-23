# 配置生成与性能可视化架构

**文档类型**: 架构设计
**状态**: 草案
**创建日期**: 2026-04-22
**更新日期**: 2026-04-22

## 1. 概述

本文档描述 CppTLM 仿真系统的**配置生成**与**性能可视化**完整流水线。

### 1.1 设计目标

| 目标 | 描述 |
|------|------|
| **配置自动化** | 通过 Python 脚本根据拓扑类型自动生成 JSON 配置文件 |
| **布局可视化** | 支持模块位置和连接关系的 DOT 布局图生成 |
| **布局可编辑** | 支持导入/导出带坐标的布局，可手动调整 |
| **性能标注** | 仿真完成后，将性能数据（带宽、延迟等）标注到布局图 |
| **实时监控** | 支持边仿真边看性能指标（流式导出 + Web 仪表板） |

### 1.2 整体架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           Python 前置处理                                │
│  ┌──────────────────┐  ┌─────────────────┐  ┌──────────────────────┐  │
│  │ topology_        │  │ layout_manager   │  │ stats_annotator       │  │
│  │ generator         │  │ (导入/导出坐标)  │  │ (性能数据标注)       │  │
│  └────────┬─────────┘  └────────┬─────────┘  └──────────┬───────────┘  │
│           │                     │                      │                │
│           ▼                     ▼                      ▼                │
│  ┌──────────────────┐  ┌─────────────────┐  ┌──────────────────────┐  │
│  │ *.json           │  │ *_layout.json   │  │ annotated_*.html      │  │
│  │ (仿真配置)        │  │ (坐标)          │  │ (带性能的可视化)     │  │
│  └────────┬─────────┘  └────────┬─────────┘  └──────────────────────┘  │
└───────────┼─────────────────────┼───────────────────────────────────────┘
            │                     │
            ▼                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                        C++ 仿真引擎 (cpptlm)                            │
│  ┌──────────────────┐  ┌─────────────────┐  ┌──────────────────────┐  │
│  │ JSON Config      │→ │ Simulation Run   │→ │ stats.json           │  │
│  │ (输入)            │  │                 │  │ (输出)               │  │
│  └──────────────────┘  └─────────────────┘  └──────────────────────┘  │
│                              │                                        │
│                              ▼                                        │
│                      ┌─────────────────┐                                │
│                      │ Streaming      │                                │
│                      │ stats.jsonl    │                                │
│                      │ (实时流)        │                                │
│                      └────────┬────────┘                                │
└───────────────────────────────┼─────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                        Python 后置处理                                  │
│  ┌──────────────────┐  ┌─────────────────┐  ┌──────────────────────┐  │
│  │ stats_watcher     │  │ stats_aggregator │  │ dashboard_server    │  │
│  │ (文件监控)         │→ │ (增量聚合)       │→ │ (Web 实时更新)       │  │
│  └──────────────────┘  └─────────────────┘  └──────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
                            Browser
                     (交互式仪表板: http://localhost:8050)
```

---

## 2. Python 前置处理

### 2.1 拓扑生成器 (topology_generator.py)

**文件位置**: `scripts/topology_generator.py`

#### 功能

- 根据拓扑类型（mesh, ring, bus, hierarchical）自动生成 JSON 配置
- 同时生成对应的 DOT 布局图
- 支持自定义边缘列表导入

#### 支持的拓扑类型

| 类型 | 参数 | 描述 |
|------|------|------|
| `mesh` | `--size WxH` | 2D Mesh NoC，例: `--size 4x4` |
| `ring` | `--nodes N` | Ring 总线拓扑 |
| `bus` | `--nodes N` | 共享总线拓扑 |
| `hierarchical` | `--levels N --factor M` | 多层级树状拓扑 |
| `custom` | `--edges FILE` | 从文件加载边缘列表 |

#### 输出文件

```
configs/
├── mesh_4x4.json           # C++ 仿真配置
├── mesh_4x4_layout.json     # 布局坐标
└── mesh_4x4.dot            # DOT 布局图
```

#### 使用示例

```bash
# 生成 4x4 Mesh 拓扑
python3 scripts/topology_generator.py \
    --type mesh --size 4x4 \
    --output configs/mesh_4x4.json \
    --layout configs/mesh_4x4_layout.json

# 生成 8 节点 Ring 拓扑
python3 scripts/topology_generator.py \
    --type ring --nodes 8 \
    --output configs/ring_8.json

# 应用 Kamada-Kawai 布局算法
python3 scripts/topology_generator.py \
    --type custom --edges my_edges.txt \
    --layout-algo kamada_kawai
```

#### 核心类设计

```python
class TopologyGenerator:
    def __init__(self):
        self.graph = nx.DiGraph()      # NetworkX 有向图
        self.layout_coords = {}        # node_name -> (x, y)
    
    def generate_mesh(self, rows: int, cols: int, router_type: str = "MeshRouter"):
        """生成 Mesh NoC 拓扑"""
        ...
    
    def generate_ring(self, num_nodes: int):
        """生成 Ring 总线拓扑"""
        ...
    
    def export_json_config(self) -> Dict:
        """导出为 CppTLM JSON 配置格式"""
        ...
    
    def export_dot(self, filepath: str):
        """导出为 DOT 文件（Graphviz 格式）"""
        ...
    
    def export_layout(self) -> Dict:
        """导出布局坐标 JSON"""
        ...
    
    def import_layout(self, layout_json: Dict):
        """导入布局坐标"""
        ...
    
    def apply_networkx_layout(self, algo: str = "spring"):
        """应用 NetworkX 自动布局算法"""
        # 可选算法: spring, circular, kamada_kawai, shell, spectral
        ...
```

### 2.2 布局管理器 (layout_manager.py)

**文件位置**: `scripts/layout_manager.py`

#### 功能

- 布局坐标的导入/导出
- 支持多种编辑工具生成的布局格式
- DOT 属性注入（固定节点位置）

#### 支持的布局格式

| 格式 | 扩展名 | 描述 |
|------|--------|------|
| JSON | `*_layout.json` | 简单坐标格式，易于程序处理 |
| DOT | `*.dot` | Graphviz 固定位置属性 (`pos="x,y"`) |
| SVG | `*.svg` | 可视化编辑，可导入 Inkscape/Illustrator |
| draw.io | `*.drawio` | 交互式图表编辑器 XML 格式 |

#### 使用示例

```python
from layout_manager import LayoutManager

# 加载布局
lm = LayoutManager("configs/mesh_4x4_layout.json")

# 手动设置节点位置
lm.set_position("router_0_0", x=0, y=0)
lm.set_position("router_0_1", x=100, y=0)

# 导出为 DOT（带固定位置）
lm.export_dot_with_positions("output/mesh_fixed.dot")

# 保存修改后的布局
lm.save("configs/mesh_4x4_layout_v2.json")
```

### 2.3 DOT 布局图生成

#### DOT 文件格式

```dot
digraph simulation {
    rankdir=LR;
    node [shape=box, style="rounded"];
    edge [color="#333333"];

    // 节点定义
    cache [label="CacheTLM\ncache", pos="100,200"];
    xbar  [label="CrossbarTLM\nxbar", pos="300,200"];
    mem   [label="MemoryTLM\nmem", pos="500,200"];

    // 连接定义
    cache -> xbar [label="1 cy", latency=1];
    xbar -> mem   [label="2 cy", latency=2];
}
```

#### 布局算法对比

| 算法 | 适用场景 | 特点 |
|------|---------|------|
| `spring` | 通用 | 基于力导向，节点自然分散 |
| `circular` | Ring/总线 | 节点沿圆周分布 |
| `kamada_kawai` | 规则拓扑 | 边长最小化，布局美观 |
| `shell` | 分层结构 | 核心节点在内层 |
| `spectral` | 小图 | 基于图谱分解，适合规则结构 |

---

## 3. C++ 仿真引擎

### 3.1 标准 JSON 配置文件格式

**参考**: `configs/crossbar_test.json`

```json
{
  "name": "CrossbarTLM Test Configuration",
  "description": "CacheTLM → CrossbarTLM → MemoryTLM",
  "modules": [
    { "name": "cache", "type": "CacheTLM" },
    { "name": "xbar", "type": "CrossbarTLM" },
    { "name": "mem", "type": "MemoryTLM" }
  ],
  "connections": [
    { "src": "cache", "dst": "xbar.0", "latency": 1 },
    { "src": "xbar.0", "dst": "mem", "latency": 2 }
  ]
}
```

### 3.2 流式统计导出 (StreamingReporter)

**文件位置**: `include/metrics/streaming_reporter.hh`

#### 功能

- 每 N 周期输出统计快照（JSON Lines 格式）
- 后台线程异步写入，避免干扰仿真
- 支持文件滚动，防止单文件过大

#### 输出格式

```jsonl
{"timestamp_ns":1713769234567890123,"simulation_cycle":10000,"group":"system.cache","data":{"hits":95000,"misses":5000}}
{"timestamp_ns":1713769234568000000,"simulation_cycle":20000,"group":"system.cache","data":{"hits":190000,"misses":10000}}
...
```

#### 命令行参数

| 参数 | 描述 | 默认值 |
|------|------|--------|
| `--stream-stats` | 启用流式统计导出 | `false` |
| `--stream-interval N` | 每 N 周期输出一次 | `10000` |
| `--stream-path PATH` | 输出文件路径 | `output/stats_stream.jsonl` |

#### 使用示例

```bash
./build/bin/cpptlm_sim \
    --config configs/mesh_4x4.json \
    --stream-stats \
    --stream-interval 5000 \
    --stream-path output/stats_stream.jsonl
```

---

## 4. Python 后置处理

### 4.1 统计监控器 (stats_watcher.py)

**文件位置**: `scripts/stats_watcher.py`

#### 功能

- 监控 `stats_stream.jsonl` 文件变化
- 增量解析并聚合数据
- 支持 WebSocket 推送更新到浏览器

#### 依赖

```bash
pip install watchdog dash plotly
```

#### 使用示例

```bash
# 启动监控 + Web 仪表板
python3 scripts/stats_watcher.py \
    --stream output/stats_stream.jsonl \
    --port 8050

# 仅控制台输出模式
python3 scripts/stats_watcher.py \
    --stream output/stats_stream.jsonl \
    --no-gui
```

### 4.2 统计聚合器 (StatsAggregator)

#### 功能

- 增量解析 JSON Lines 格式
- 维护指标历史记录
- 支持时间窗口聚合

#### 数据结构

```python
class StatsAggregator:
    history = {
        "system.cache.hits": [
            {"timestamp": 1234.567, "cycle": 10000, "value": 95000},
            {"timestamp": 1234.568, "cycle": 20000, "value": 190000},
            ...
        ],
        "system.xbar.latency.avg": [
            {"timestamp": 1234.567, "cycle": 10000, "value": 4.5},
            ...
        ]
    }
    latest = {
        "system.cache.hits": 190000,
        "system.xbar.latency.avg": 4.5
    }
```

### 4.3 Dashboard 服务器 (DashboardServer)

**框架**: Dash (Plotly)

#### 功能

- 实时更新的指标卡片
- 延迟分布直方图
- 吞吐量时间线图
- 每秒自动刷新

#### 访问地址

```
http://localhost:8050
```

---

## 5. 性能标注工具 (stats_annotator.py)

**文件位置**: `scripts/stats_annotator.py`

### 功能

将仿真统计结果标注到 DOT 布局图，生成带性能数据的可视化报告。

### 使用示例

```bash
python3 scripts/stats_annotator.py \
    --config configs/mesh_4x4.json \
    --stats output/stats_stream.jsonl \
    --layout configs/mesh_4x4_layout.json \
    --output docs/report_mesh_4x4.html
```

### 输出内容

1. **模块统计表格**
   - 模块名、指标名、值

2. **性能标注的 DOT 图**
   - 节点标签显示延迟
   - 边标签显示带宽利用率

3. **交互式 HTML 报告**
   - Plotly 图表
   - 可缩放、平移

---

## 6. 完整工作流

### 6.1 批处理脚本

```bash
#!/bin/bash
# run_full_pipeline.sh

set -e

TOPOLOGY_TYPE=${1:-mesh}
SIZE=${2:-4x4}

# 1. Python 前置：生成配置和布局
echo "[1/6] Generating topology..."
python3 scripts/topology_generator.py \
    --type $TOPOLOGY_TYPE --size $SIZE \
    --output configs/${TOPOLOGY_TYPE}_${SIZE//x/_}.json \
    --layout configs/${TOPOLOGY_TYPE}_${SIZE//x/_}_layout.json

# 2. (可选) 手动调整布局
echo "[2/6] Layout editing (if needed)..."
# edit configs/${TOPOLOGY_TYPE}_${SIZE//x/_}_layout.json

# 3. 生成 DOT 布局图
echo "[3/6] Generating DOT layout..."
python3 scripts/layout_to_dot.py \
    --layout configs/${TOPOLOGY_TYPE}_${SIZE//x/_}_layout.json \
    --output configs/${TOPOLOGY_TYPE}_${SIZE//x/_}.dot

# 4. 启动 Python 监控服务器（后台）
echo "[4/6] Starting stats watcher..."
python3 scripts/stats_watcher.py \
    --stream output/stats_stream.jsonl \
    --port 8050 &
WATCHER_PID=$!

# 5. 运行 C++ 仿真（流式输出）
echo "[5/6] Running simulation..."
./build/bin/cpptlm_sim \
    --config configs/${TOPOLOGY_TYPE}_${SIZE//x/_}.json \
    --stream-stats \
    --stream-interval 10000 \
    --stream-path output/stats_stream.jsonl

# 6. Python 后置：生成带性能标注的报告
echo "[6/6] Generating annotated report..."
python3 scripts/stats_annotator.py \
    --config configs/${TOPOLOGY_TYPE}_${SIZE//x/_}.json \
    --stats output/stats_stream.jsonl \
    --layout configs/${TOPOLOGY_TYPE}_${SIZE//x/_}_layout.json \
    --output docs/report_${TOPOLOGY_TYPE}_${SIZE//x/_}.html

# 停止 watcher
kill $WATCHER_PID 2>/dev/null || true

echo "Done!"
echo "  - Final report: docs/report_${TOPOLOGY_TYPE}_${SIZE//x/_}.html"
echo "  - Real-time dashboard: http://localhost:8050"
```

### 6.2 执行流程图

```
┌──────────────────────────────────────────────────────────────────────────┐
│                              用户执行                                    │
│                         ./run_full_pipeline.sh mesh 4x4                  │
└──────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ [1] topology_generator.py                                                │
│     输入: --type mesh --size 4x4                                        │
│     输出: configs/mesh_4x4.json (配置)                                  │
│           configs/mesh_4x4_layout.json (坐标)                           │
│           configs/mesh_4x4.dot (布局图)                                 │
└──────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ [2] stats_watcher.py (后台运行)                                          │
│     监控: output/stats_stream.jsonl                                      │
│     服务: http://localhost:8050                                         │
└──────────────────────────────────────────────────────────────────────────┘
                                    │
                    ┌───────────────┴───────────────┐
                    ▼                               ▼
┌────────────────────────────┐    ┌────────────────────────────┐
│     C++ 仿真引擎           │    │     Python Dashboard        │
│  cpptlm_sim               │    │     (实时更新)              │
│  --stream-stats           │───→│     http://localhost:8050   │
│  output/stats_stream.jsonl│    │                            │
└────────────────────────────┘    └────────────────────────────┘
                    │
                    ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ [3] stats_annotator.py                                                  │
│     输入: configs/mesh_4x4.json (配置)                                  │
│           output/stats_stream.jsonl (性能数据)                          │
│           configs/mesh_4x4_layout.json (坐标)                           │
│     输出: docs/report_mesh_4x4.html (带标注的可视化)                    │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 7. 数据流映射

### 7.1 配置中的模块名 ↔ 统计中的路径

```
JSON Config 中的模块名      Stats JSON 中的路径          DOT 中的节点
        ↓                           ↓                        ↓
    "cache"              system.cache.l1_icache.hits      cache (有标注)
    "xbar"               system.xbar.latency.avg         xbar (有标注)
```

### 7.2 连接信息 ↔ 性能数据

```
JSON Config 中的连接              Stats 中的连接统计           DOT 中的边
        ↓                               ↓                        ↓
"cache" → "xbar.0"              cache_to_xbar.bandwidth     "cache" → "xbar" [label="8.5 GB/s"]
"xbar.0" → "mem"               xbar_to_mem.latency         "xbar" → "mem" [label="2 cy"]
```

### 7.3 布局坐标映射

```json
// configs/mesh_4x4_layout.json
{
  "version": "1.0",
  "nodes": {
    "cache": { "x": 100, "y": 200 },
    "xbar":  { "x": 300, "y": 200 },
    "mem":   { "x": 500, "y": 200 },
    "router_0_0": { "x": 0, "y": 0 },
    "router_0_1": { "x": 100, "y": 0 },
    ...
  }
}
```

---

## 8. 文件结构

```
CppTLM/
├── scripts/
│   ├── topology_generator.py     # 拓扑生成器
│   ├── layout_manager.py          # 布局管理器
│   ├── layout_to_dot.py           # 布局转 DOT
│   ├── stats_watcher.py           # 统计监控器
│   └── stats_annotator.py         # 性能标注工具
│
├── configs/
│   ├── mesh_4x4.json              # 仿真配置
│   ├── mesh_4x4_layout.json       # 布局坐标
│   └── mesh_4x4.dot               # DOT 布局图
│
├── output/
│   └── stats_stream.jsonl         # 流式统计输出
│
├── docs/
│   └── report_mesh_4x4.html       # 最终报告
│
└── include/
    └── metrics/
        ├── streaming_reporter.hh  # C++ 流式导出
        ├── stats.hh              # 统计类型
        ├── metrics_reporter.hh   # 报告生成器
        └── histogram.hh          # 直方图
```

---

## 9. 依赖清单

### Python 依赖

```txt
networkx>=3.0           # 图数据结构和布局算法
pydot>=1.4.2            # DOT 文件生成/解析
graphviz                 # DOT 渲染工具
watchdog>=3.0           # 文件系统监控
dash>=2.14.0            # Web 仪表板框架
plotly>=5.18.0          # 图表库
```

### 安装命令

```bash
pip install networkx pydot watchdog dash plotly
# graphviz 需要系统级安装
apt install graphviz  # Ubuntu/Debian
brew install graphviz # macOS
```

---

## 10. 扩展功能

### 10.1 时序动画

将仿真切分为多个时间窗口，生成动画帧：

```python
class TimelineAnimator:
    def generate_frames(self, stats_stream: str, num_frames: int):
        """生成动画帧"""
        ...
    
    def export_gif(self, frames: List[str], output: str):
        """导出为 GIF"""
        ...
```

### 10.2 热点检测

自动在布局图上标记性能瓶颈：

```python
class HotspotDetector:
    def detect(self, stats: Dict, threshold: float) -> List[str]:
        """返回高延迟/高拥塞节点列表"""
        ...
    
    def annotate_hotspots(self, dot_path: str, hotspots: List[str]):
        """在 DOT 图上标注热点（红色高亮）"""
        ...
```

### 10.3 对比模式

加载两个 stats.json，生成并排对比图：

```python
class StatsComparator:
    def compare(self, baseline: str, current: str) -> Dict:
        """返回差异字典"""
        ...
    
    def generate_diff_report(self, baseline: str, current: str) -> str:
        """生成对比 HTML 报告"""
        ...
```

### 10.4 Prometheus 集成

可选：推送到 Prometheus PushGateway：

```python
from prometheus_client import Counter, Gauge, push_to_gateway

def push_to_prometheus(stats_path: str, gateway: str = "localhost:9091"):
    """将统计推送到 Prometheus"""
    ...
```

---

## 11. 配置验证

### 11.1 JSON 配置验证器

```python
def validate_config(config: Dict) -> ValidationResult:
    """验证配置合法性"""
    errors = []
    warnings = []
    
    # 检查模块类型是否存在
    for module in config.get("modules", []):
        if not module.get("type"):
            errors.append(f"Module {module.get('name')} missing type")
    
    # 检查连接两端是否有效
    module_names = {m["name"] for m in config.get("modules", [])}
    for conn in config.get("connections", []):
        src = conn.get("src", "").split(".")[0]
        dst = conn.get("dst", "").split(".")[0]
        if src not in module_names:
            errors.append(f"Connection src '{src}' not found")
        if dst not in module_names:
            errors.append(f"Connection dst '{dst}' not found")
    
    return ValidationResult(valid=len(errors)==0, errors=errors, warnings=warnings)
```

---

## 12. 已知限制

| 限制 | 描述 | 解决方案 |
|------|------|---------|
| 流式导出性能 | 高频输出会影响仿真性能 | 适当增大 `--stream-interval` |
| 大规模拓扑 | 万级节点布局可能慢 | 使用 `spectral` 算法或预计算布局 |
| 多实例汇聚 | 暂不支持多仿真实例统计聚合 | 未来可加 Redis/Kafka |

---

## 13. 未来工作

- [ ] 实现 `topology_generator.py`
- [ ] 实现 `layout_manager.py`
- [ ] 实现 `stats_watcher.py` + `DashboardServer`
- [ ] 实现 `stats_annotator.py`
- [ ] 集成测试完整流水线
- [ ] 添加时序动画功能
- [ ] 添加热点检测功能
- [ ] 添加 Prometheus 集成

---

## 附录 A: 命令参考

### 拓扑生成

```bash
python3 scripts/topology_generator.py --help

选项:
  --type TYPE              拓扑类型: mesh, ring, bus, hierarchical, custom
  --size WxH               Mesh 尺寸, 例: 4x4
  --nodes N                节点数量 (ring/bus)
  --edges FILE             自定义边缘列表文件
  --output JSON            输出 JSON 配置文件
  --layout JSON            输出布局 JSON
  --layout-algo ALGO       布局算法: spring, circular, kamada_kawai, shell, spectral
```

### 统计监控

```bash
python3 scripts/stats_watcher.py --help

选项:
  --stream FILE            监控的流文件路径
  --port PORT              Dashboard 端口 (默认: 8050)
  --no-gui                 仅控制台输出模式
```

### 性能标注

```bash
python3 scripts/stats_annotator.py --help

选项:
  --config JSON            仿真配置文件
  --stats FILE             统计输出文件
  --layout JSON            布局坐标文件
  --output HTML            输出 HTML 报告
```

---

## 附录 B: 示例输出

### 流式统计文件 (stats_stream.jsonl)

```jsonl
{"timestamp_ns":1713769234567890123,"simulation_cycle":10000,"group":"system.cache","data":{"hits":95000,"misses":5000,"latency":{"count":100000,"min":2,"avg":4.5,"max":12,"stddev":1.2}}}
{"timestamp_ns":1713769234568000000,"simulation_cycle":10000,"group":"system.xbar","data":{"throughput":8.5,"latency":{"avg":2.3}}}
{"timestamp_ns":1713769234568111222,"simulation_cycle":20000,"group":"system.cache","data":{"hits":190000,"misses":10000,"latency":{"count":200000,"min":2,"avg":4.6,"max":15,"stddev":1.3}}}
```

### HTML 报告片段

```html
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
    <tr><td>cache</td><td>hits</td><td>190000</td></tr>
    <tr><td>cache</td><td>misses</td><td>10000</td></tr>
    <tr><td>xbar</td><td>throughput</td><td>8.5 GB/s</td></tr>
    </table>
</body>
</html>
```