# CppTLM 拓扑配置与仿真流程指南

## 1. 当前架构状态

### ✅ 已实现
- `cpptlm_sim` - 主仿真器，读取 JSON 配置，通过 ModuleFactory 实例化模块
- `StreamingReporter` - 流式统计输出到 JSONL 文件
- `stats_annotator.py` - 生成 HTML 性能报告
- `stats_watcher.py` - 实时监控统计流

### ⚠️ 设计问题
- `topology_generator.py` 生成的类型名（如 `MeshRouter`、`Processor`）**与实际注册的模块类型不匹配**
- 实际注册的类型：`CPUSim`, `TrafficGenTLM`, `CacheTLM`, `CrossbarTLM`, `MemoryTLM`, `Router` 等

---

## 2. 当前可用模块类型

```
Registered SimObjects:
  - CPUSim           # 简单 CPU
  - MemorySim        # 简单内存
  - TrafficGenTLM    # 流量生成器 (支持 SEQUENTIAL/RANDOM/HOTSPOT/STRIDED 模式)
  - CacheTLM         # TLM Cache
  - CacheSim         # 简单 Cache
  - CrossbarTLM      # 交叉开关
  - Router           # 路由器
  - TrafficGenerator # 流量生成器

Registered SimModules:
  - CpuCluster       # CPU 集群
```

---

## 3. 完整工作流

### Step 1: 选择或创建配置文件

```bash
# 查看现有配置
ls configs/

# 复制一个现有配置作为模板
cp configs/cpu_simple.json configs/my_config.json
```

### Step 2: 查看配置文件格式

```bash
cat configs/stress_full_system.json
```

典型格式：
```json
{
  "modules": [
    { "name": "cpu0", "type": "TrafficGenTLM", "config": { "pattern": "RANDOM", ... } },
    { "name": "l1_0", "type": "CacheTLM" },
    { "name": "xbar", "type": "CrossbarTLM" },
    { "name": "mem0", "type": "MemoryTLM" }
  ],
  "connections": [
    { "src": "cpu0", "dst": "l1_0", "latency": 1 },
    { "src": "l1_0", "dst": "xbar.0", "latency": 1 },  // .0 表示端口索引
    { "src": "xbar.0", "dst": "mem0", "latency": 2 }
  ]
}
```

### Step 3: 运行仿真

```bash
# 基础仿真
./build/bin/cpptlm_sim configs/my_config.json --cycles 10000

# 带流式统计
./build/bin/cpptlm_sim configs/my_config.json \
    --stream-stats \
    --stream-interval 1000 \
    --stream-path output/stats.jsonl \
    --cycles 10000
```

### Step 4: 查看性能报告

```bash
# 生成 HTML 报告
python3 scripts/stats_annotator.py \
    --config configs/my_config.json \
    --stats output/stats.jsonl \
    --output output/report.html

# 查看报告
# Linux: xdg-open output/report.html
# macOS: open output/report.html
```

---

## 4. 已有配置示例

### 4.1 简单 CPU-Memory 系统
```bash
./build/bin/cpptlm_sim configs/cpu_simple.json --cycles 5000
```

### 4.2 多核 CPU + Cache + Crossbar
```bash
./build/bin/cpptlm_sim configs/stress_full_system.json \
    --stream-stats \
    --stream-interval 1000 \
    --cycles 10000
```

### 4.3 使用 streaming_demo（独立演示，有内置统计）
```bash
./build/bin/streaming_demo \
    --output output/stats.jsonl \
    --interval 1000 \
    --cycles 5000
```

---

## 5. JSON 配置格式说明

### 5.1 modules 数组

| 字段 | 必需 | 说明 |
|------|------|------|
| `name` | 是 | 模块实例名，唯一标识 |
| `type` | 是 | 模块类型，必须是已注册类型 |
| `config` | 否 | 模块特定配置（取决于类型） |

### 5.2 connections 数组

| 字段 | 必需 | 说明 |
|------|------|------|
| `src` | 是 | 源模块名，或 `regex:pattern` 或 `group:groupname` |
| `dst` | 是 | 目标模块名，支持端口索引语法 `module.port` |
| `latency` | 否 | 延迟周期数，默认 1 |
| `bandwidth` | 否 | 带宽，默认 100 |
| `exclude` | 否 | 排除的模块列表（用于 group/regex 源） |

### 5.3 groups 对象

用于批量连接：
```json
"groups": {
  "cpus": ["cpu0", "cpu1", "cpu2", "cpu3"]
}
"connections": [
  { "src": "group:cpus", "dst": "l1_shared", "latency": 2 }
]
```

### 5.4 端口索引语法

Crossbar 等多端口模块使用 `.N` 语法：
```json
{ "src": "l1_0", "dst": "xbar.0" }  // 连接到 xbar 的端口 0
{ "src": "l1_1", "dst": "xbar.1" }  // 连接到 xbar 的端口 1
```

---

## 6. 拓扑可视化

### 6.1 仿真时自动生成 DOT

`cpptlm_sim` 运行后会自动生成 `topology.dot`：
```bash
ls topology.dot
```

### 6.2 转换为 PNG
```bash
# 安装 graphviz
sudo apt install graphviz

# 转换
dot -Tpng topology.dot -o topology.png
```

### 6.3 使用 topology_generator 生成布局

```bash
# 注意：当前生成的类型与实际模块不匹配，仅用于可视化参考
python3 scripts/topology_generator.py \
    --type mesh --size 4x4 \
    --output output/my_mesh.json \
    --dot output/my_mesh.dot

dot -Tpng output/my_mesh.dot -o output/my_mesh.png
```

---

## 7. 实时监控

### 7.1 控制台模式
```bash
python3 scripts/stats_watcher.py \
    --stream output/stats.jsonl \
    --config configs/my_config.json \
    --no-gui
```

### 7.2 Web 仪表板模式
```bash
# 需要安装 dash: pip install dash plotly
python3 scripts/stats_watcher.py \
    --stream output/stats.jsonl \
    --config configs/my_config.json \
    --port 8050
# 然后浏览器打开 http://localhost:8050
```

---

## 8. 已知限制与待修复问题

### 8.1 topology_generator.py 类型不匹配

**问题**: `topology_generator.py` 生成的类型名（`MeshRouter`, `NetworkInterface`, `Processor`）与实际注册的模块类型（`Router`, `CPUSim`）不匹配。

**临时解决方案**: 手动编辑生成的 JSON，将类型名替换为实际注册的模块类型。

**建议修复方案**:
```
topology_generator.py 应增加 --target runtime 选项，
生成与 cpptlm_sim 实际注册类型匹配的配置
```

### 8.2 模块不支持动态统计注册

**问题**: `StreamingReporter` 需要模块主动注册 StatGroup，但大多数模块（CacheTLM, CrossbarTLM 等）未实现统计。

**临时解决方案**: 使用 `streaming_demo`，它内置了 `StatsCPU` 和 `StatsCache` 类的完整统计。

---

## 9. 快速参考命令

```bash
# 1. 编译
cmake --build build -j$(nproc)

# 2. 列出可用配置
ls configs/

# 3. 运行简单仿真
./build/bin/cpptlm_sim configs/cpu_simple.json --cycles 5000

# 4. 运行带统计的仿真
./build/bin/cpptlm_sim configs/stress_full_system.json \
    --stream-stats --stream-interval 1000 --cycles 10000

# 5. 运行 streaming_demo
./build/bin/streaming_demo --output output/stats.jsonl --interval 1000

# 6. 生成报告
python3 scripts/stats_annotator.py \
    --config output/streaming_config.json \
    --stats output/stats.jsonl \
    --output output/report.html

# 7. 查看可视化
dot -Tpng topology.dot -o topology.png
xdg-open topology.png  # Linux
open topology.png      # macOS
```

---

## 10. 参考配置文件

| 文件 | 描述 | 适用场景 |
|------|------|----------|
| `configs/cpu_simple.json` | 1 CPU + 1 Cache + 1 Memory | 入门演示 |
| `configs/cpu_group.json` | 多 CPU + 分组连接 | 测试分组/正则连接 |
| `configs/stress_full_system.json` | 4 TrafficGen + Cache + Crossbar + 4 Memory | 完整系统仿真 |
| `configs/noc_mesh.json` | 2x2 Mesh 拓扑 | NoC 研究 |
| `configs/crossbar_test.json` | Crossbar 交叉开关 | 交叉开关测试 |

---

## 11. 下一步：SOC 层次化拓扑

参考 gem5 的配置方式，未来应支持：

```json
{
  "name": "soc_4核",
  "subnetworks": {
    "cluster0": { /* 本地 mesh */ },
    "cluster1": { /* 本地 mesh */ },
    "interconnect": { /* 核间 crossbar */ },
    "memory_controller": { /* 内存控制器 */ }
  },
  "connections": [
    { "src": "cluster0.router", "dst": "interconnect" },
    { "src": "interconnect", "dst": "memory_controller" }
  ]
}
```

这种层次化配置可以组合出多核 CPU、GPU 等复杂 SOC 拓扑。
