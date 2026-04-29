# CppTLM 拓扑配置与仿真流程指南

> **版本**: v2.0
> **更新日期**: 2026-04-30
> **状态**: ✅ 完整

---

## 1. 概述

本文档描述 CppTLM v2.0 的拓扑配置生成、验证和仿真流程。包括：

- `topology_generator.py` - 自动生成 mesh/ring/hierarchical 拓扑配置
- `topology_validator.py` - 验证拓扑配置的连通性、端口方向、Bundle 类型
- `run_full_pipeline.sh` - 完整的生成→验证→仿真→可视化流程

---

## 2. 快速开始

```bash
# 1. 生成 2x2 Mesh 拓扑
python3 scripts/topology_generator.py --type mesh --size 2x2 --output output/topology.json

# 2. 验证配置
python3 scripts/topology_validator.py output/topology.json

# 3. 运行完整流程（生成+仿真+报告）
bash scripts/run_full_pipeline.sh mesh 2x2 output/
```

---

## 3. 拓扑生成器 (topology_generator.py)

### 3.1 基本用法

```bash
python3 scripts/topology_generator.py [选项]
```

### 3.2 选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `--type` | 拓扑类型: `mesh`, `ring`, `hierarchical` | `mesh` |
| `--size` | 拓扑尺寸 (如 `2x2`, `4x4`, `8`) | `4x4` |
| `--target` | 目标平台: `cpptlm`, `抽象` | `抽象` |
| `--output` | 输出 JSON 配置文件路径 | (stdout) |
| `--layout` | 输出布局 JSON 路径 | (无) |
| `--dot` | 输出 DOT 文件路径 | (无) |

### 3.3 拓扑类型

#### Mesh 拓扑

```bash
# 2x2 Mesh: 4 routers + 4 NIs + 4 processors
python3 scripts/topology_generator.py --type mesh --size 2x2 --target cpptlm --output configs/mesh_2x2.json
```

生成的模块：
- `router_X_Y` - RouterTLM 路由器
- `ni_X_Y` - NICTLM 网络接口
- `proc_X_Y` - CPUSim 处理器

#### Ring 拓扑

```bash
# 8 节点环形拓扑
python3 scripts/topology_generator.py --type ring --size 8 --target cpptlm --output configs/ring_8.json
```

#### Hierarchical 拓扑

```bash
# 层级拓扑: groups 个 cluster，每个 cluster 大小为 size
python3 scripts/topology_generator.py --type hierarchical --size 2 --groups 2 --target cpptlm
```

### 3.4 目标平台

```bash
# 生成 CppTLM 运行时配置（RouterTLM, NICTLM, CPUSim）
python3 scripts/topology_generator.py --type mesh --size 2x2 --target cpptlm

# 生成抽象配置（MeshRouter, NetworkInterface, Processor）
python3 scripts/topology_generator.py --type mesh --size 2x2 --target 抽象
```

---

## 4. 拓扑验证器 (topology_validator.py)

### 4.1 基本用法

```bash
python3 scripts/topology_validator.py <config.json> [-v]
```

### 4.2 验证规则

| 规则 ID | 说明 | 描述 |
|---------|------|------|
| `VALID-01` | 模块连通性 | 所有连接的模块都已在 modules 中定义 |
| `VALID-02` | BFS 可达性 | 所有终端节点（Processor/NICTLM）从任意起点可达 |
| `PORT-01` | 路由器端口方向 | Router-to-Router 连接使用正确的端口方向 |
| `PORT-03` | Bundle 类型兼容性 | Router-Local(4) 必须连接 NI-Network(1)，反之亦然 |

### 4.3 端口方向约定

RouterTLM 端口映射：

| 端口 | 方向 | 说明 |
|------|------|------|
| 0 | NORTH | 连接到 y-1 的 router |
| 1 | EAST | 连接到 x+1 的 router |
| 2 | SOUTH | 连接到 y+1 的 router |
| 3 | WEST | 连接到 x-1 的 router |
| 4 | LOCAL | 连接到本地 NI |

**方向规则**：
- EAST 连接: `router(X,Y).EAST(1)` → `router(X+1,Y).WEST(3)`
- WEST 连接: `router(X,Y).WEST(3)` → `router(X-1,Y).EAST(1)`
- SOUTH 连接: `router(X,Y).SOUTH(2)` → `router(X,Y+1).NORTH(0)`
- NORTH 连接: `router(X,Y).NORTH(0)` → `router(X,Y-1).SOUTH(2)`

### 4.4 示例

```bash
# 验证配置
python3 scripts/topology_validator.py configs/mesh_2x2.json

# 详细输出
python3 scripts/topology_validator.py configs/mesh_2x2.json -v
```

输出示例：
```
[TopologyValidator] configs/mesh_2x2.json
==================================================
  PASS: VALID-01
  PASS: VALID-02
  PASS: PORT-01
  PASS: PORT-03
==================================================
ALL VALIDATIONS PASSED
```

---

## 5. 完整流程脚本 (run_full_pipeline.sh)

### 5.1 基本用法

```bash
bash scripts/run_full_pipeline.sh [topology] [size] [output_dir]
```

### 5.2 参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `topology` | 拓扑类型: `mesh`, `ring`, `hierarchical` | `mesh` |
| `size` | 拓扑尺寸 | `4x4` |
| `output_dir` | 输出目录 | `output` |

### 5.3 流程步骤

1. **生成拓扑** - 调用 `topology_generator.py` 生成 JSON 配置
2. **运行仿真** - 调用 `cpptlm_sim`（如已编译）
3. **生成报告** - 调用 `stats_annotator.py` 生成 HTML 报告
4. **统计监控** - 可选启动 `stats_watcher.py` Web 仪表板

### 5.4 示例

```bash
# 运行完整流程
bash scripts/run_full_pipeline.sh mesh 2x2 output/

# 查看输出
ls output/
# topology.json      - 生成的拓扑配置
# topology.dot       - GraphViz DOT 文件
# report.html        - HTML 性能报告
# stats_stream.jsonl - 仿真统计流
```

### 5.5 环境变量

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `ENABLE_WATCHER` | 启用 stats_watcher Web 仪表板 | `0` (禁用) |

```bash
# 启用 Web 仪表板
ENABLE_WATCHER=1 bash scripts/run_full_pipeline.sh mesh 4x4
# 访问 http://localhost:8050
```

---

## 6. 配置文件格式 (v3.0)

### 6.1 基本结构

```json
{
  "$schema": "./tgms_v3.0_schema.json",
  "version": "3.0",
  "modules": [
    { "name": "模块名", "type": "模块类型", "params": { ... } }
  ],
  "groups": {
    "组名": ["模块通配符模式"]
  },
  "connections": [
    { "src": "源", "dst": "目标", "latency": 1, "bandwidth": 100 }
  ]
}
```

### 6.2 模块类型映射

| CppTLM 类型 | 说明 | 参数 |
|-------------|------|------|
| `RouterTLM` | 路由器 | `node_x`, `node_y`, `mesh_x`, `mesh_y` |
| `NICTLM` | 网络接口 | `node_id`, `mesh_x`, `mesh_y` |
| `MemoryTLM` | 内存 | - |
| `CPUSim` | CPU | - |
| `CacheTLM` | 缓存 | - |
| `CrossbarTLM` | 交叉开关 | `num_ports` |

### 6.3 连接语法

```json
{
  "connections": [
    // 基本连接
    { "src": "cpu0", "dst": "cache0" },

    // 带端口索引
    { "src": "router_0_0.1", "dst": "router_1_0.3" },

    // 带参数
    { "src": "cpu0", "dst": "cache0", "latency": 2, "bandwidth": 50 }
  ]
}
```

### 6.4 组通配符

```json
{
  "groups": {
    "nics": ["nic_*"],
    "routers": ["router_*"]
  }
}
```

连接时使用：
```json
{ "src": "group:nics", "dst": "group:routers" }
```

---

## 7. 端到端示例

### 7.1 2x2 Mesh 仿真

```bash
# 1. 生成并验证配置
python3 scripts/topology_generator.py --type mesh --size 2x2 --target cpptlm --output configs/mesh_2x2.json
python3 scripts/topology_validator.py configs/mesh_2x2.json

# 2. 运行仿真
./build/bin/cpptlm_sim configs/mesh_2x2.json --cycles 10000

# 3. 生成可视化
dot -Tpng topology.dot -o mesh.png
```

### 7.2 使用完整流程脚本

```bash
# 一步完成所有操作
bash scripts/run_full_pipeline.sh mesh 2x2 output

# 查看报告
xdg-open output/report.html  # Linux
open output/report.html       # macOS
```

### 7.3 自定义拓扑

```bash
# 1. 生成基础配置
python3 scripts/topology_generator.py --type mesh --size 4x4 --target cpptlm --output configs/mesh_4x4.json

# 2. 编辑配置添加自定义模块或连接
vim configs/mesh_4x4.json

# 3. 验证配置
python3 scripts/topology_validator.py configs/mesh_4x4.json -v

# 4. 运行仿真
./build/bin/cpptlm_sim configs/mesh_4x4.json --cycles 50000
```

---

## 8. 端口索引参考

### RouterTLM 端口映射

```
        NORTH(0)
           ↑
    ┌──────┬──────┐
    │      │      │
WEST(3) ←Router→ EAST(1)
    │      │      │
    └──────┴──────┘
           ↓
       SOUTH(2)

        LOCAL(4)
           ↓
        [NI]
```

### NICTLM 端口映射

| 端口 | 说明 | 连接对象 |
|------|------|----------|
| 0 | PE Side | CPUSim / Processor |
| 1 | Network Side | RouterTLM.LOCAL(4) |

### 连接规则

| 源 | 目标 | 源端口 | 目标端口 |
|----|------|--------|----------|
| NICTLM | RouterTLM | 1 (Network) | 4 (LOCAL) |
| RouterTLM | NICTLM | 4 (LOCAL) | 1 (Network) |
| RouterTLM | RouterTLM | 见端口方向约定 | 见端口方向约定 |

---

## 9. 快速参考

### 命令汇总

```bash
# 生成拓扑
python3 scripts/topology_generator.py --type mesh --size 2x2 --target cpptlm --output configs/mesh.json

# 验证拓扑
python3 scripts/topology_validator.py configs/mesh.json -v

# 完整流程
bash scripts/run_full_pipeline.sh mesh 2x2 output/

# 运行仿真
./build/bin/cpptlm_sim configs/mesh.json --cycles 10000

# 生成可视化
dot -Tpng topology.dot -o topology.png
```

### 端口索引速查

| 连接类型 | 源端口 | 目标端口 |
|----------|--------|----------|
| NI → Router | 1 | 4 |
| Router → NI | 4 | 1 |
| Router → Router (X+) | 1 | 3 |
| Router → Router (X-) | 3 | 1 |
| Router → Router (Y+) | 2 | 0 |
| Router → Router (Y-) | 0 | 2 |

---

## 10. 已知问题

### Q: topology_validator.py 报告 PORT-01 失败

**A**: 检查连接方向是否正确。常见错误：
- X 方向连接使用了 Y 方向端口
- 连接方向与端口索引不匹配（如 dst 在东边但使用了 WEST 端口）

正确示例：`router_0_0.1(EAST) → router_1_0.3(WEST)`（dst 在东边，src 用 EAST，dst 用 WEST）

### Q: 生成器输出的类型与运行时不匹配

**A**: 确保使用 `--target cpptlm` 选项生成配置。

### Q: 仿真器找不到配置文件中的模块

**A**: 检查 `modules` 数组中模块类型是否已注册。运行 `./build/bin/cpptlm_sim --help` 查看支持的模块类型。
