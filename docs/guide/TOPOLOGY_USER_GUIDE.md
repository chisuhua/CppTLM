# CppTLM 拓扑配置与仿真流程指南

> **版本**: v2.1
> **更新日期**: 2026-05-09
> **状态**: ✅ 完整（已更新 Phase 3.x Python 工具链）

---

## 1. 概述

本文档描述 CppTLM v2.1 的拓扑配置生成、验证和仿真流程。包括：

- **`cpptlm_config/validator.py`** - Python 两阶段验证（结构+参数）
- **`cpptlm_config/topology_adapter.py`** - Mesh/Ring 拓扑生成适配器
- **`cpptlm_config/builder.py`** - Pydantic 配置构建器
- **`scripts/topology_generator.py`** - 自动生成 mesh/ring/hierarchical 拓扑配置
- **`scripts/topology_validator.py`** - 命令行验证包装器
- **`run_full_pipeline.sh`** - 完整的生成→验证→仿真→可视化流程

**新增（Phase 3.x）**:
- Python 验证工具链（87 个 pytest 用例）
- PARAM-01/02 参数验证规则
- PORT-03 Bundle 类型兼容性检查

---

## 2. 快速开始

### 2.1 使用 Python API（推荐）

```python
from cpptlm_config.topology_adapter import TopologyAdapter
from cpptlm_config.validator import TopologyValidator
import json

# 1. 生成 2x2 Mesh 拓扑
adapter = TopologyAdapter.from_mesh(2, 2)
config = adapter.to_dict()

# 2. 验证配置
v = TopologyValidator(config)
result = v.validate()
assert result.is_valid, f"验证失败: {result.errors}"

# 3. 导出 JSON
with open("mesh_2x2.json", "w") as f:
    json.dump(config, f, indent=2)
```

### 2.2 使用命令行工具

```bash
# 1. 生成 2x2 Mesh 拓扑
python3 scripts/topology_generator.py --type mesh --size 2x2 --output output/topology.json

# 2. 验证配置（Python 工具链）
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

## 4. 拓扑验证器

CppTLM 提供两种验证方式：
1. **Python 验证器** (`cpptlm_config/validator.py`) — 两阶段验证（结构+参数）
2. **命令行包装器** (`scripts/topology_validator.py`) — 便捷命令行接口

### 4.1 Python API 验证（推荐）

```python
from cpptlm_config.validator import TopologyValidator
import json

# 加载配置
with open("configs/mesh_2x2_tlm.json") as f:
    config = json.load(f)

# 执行验证
v = TopologyValidator(config)
result = v.validate()

if result.is_valid:
    print("✅ 所有验证通过")
else:
    print(f"❌ {len(result.errors)} 个错误, {len(result.warnings)} 个警告")
    for e in result.errors:
        print(f"  [{e.code}] {e.message}")
```

### 4.2 命令行验证

```bash
python3 scripts/topology_validator.py <config.json> [-v]
```

### 4.3 验证规则

| 规则 ID | 说明 | 描述 | 严重级别 |
|---------|------|------|---------|
| `VALID-01` | 模块连通性 | 所有连接的模块都已在 modules 中定义 | 错误 |
| `VALID-02` | BFS 可达性 | 所有终端节点（Processor/NICTLM）从任意起点可达 | 错误 |
| `PORT-01` | 路由器端口方向 | Router-to-Router 连接使用正确的端口方向 | 警告 |
| `PORT-03` | Bundle 类型兼容性 | Router-Local(4) 必须连接 NICTLM-Network(1) | 错误 |
| `PARAM-01` | 必需参数检查 | 模块必需参数存在（如 RouterTLM 的 node_x/node_y） | 错误 |
| `PARAM-02` | 参数范围检查 | 数值在 min_value/max_value 范围内 | 错误 |

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

## 5. Python 配置构建器 (builder.py)

### 5.1 使用 ConfigBuilder

```python
from cpptlm_config.builder import ConfigBuilder
from cpptlm_config.validator import TopologyValidator
import json

builder = ConfigBuilder()

# 添加模块
builder.add_router("router_0_0", node_x=0, node_y=0, mesh_x=2, mesh_y=2)
builder.add_nic("ni0", node_id=0, mesh_x=2, mesh_y=2)
builder.add_cpu("cpu0")

# 添加连接
builder.add_connection("router_0_0.4", "ni0.1")
builder.add_connection("ni0.0", "cpu0")

# 验证
config = builder.build()
v = TopologyValidator(config)
assert v.validate().is_valid

# 导出
builder.export_json("my_mesh.json")
```

### 5.2 手动构建配置

```python
from cpptlm_config.builder import ConfigBuilder

builder = ConfigBuilder()
builder.config["modules"].append({
    "name": "router_0_0",
    "type": "RouterTLM",
    "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}
})
builder.config["connections"].append({
    "src": "router_0_0.4",
    "dst": "ni0.1",
    "latency": 1
})
```

---

## 7. 完整流程脚本 (run_full_pipeline.sh)

### 7.1 基本用法

```bash
bash scripts/run_full_pipeline.sh [topology] [size] [output_dir]
```

### 7.2 参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `topology` | 拓扑类型: `mesh`, `ring`, `hierarchical` | `mesh` |
| `size` | 拓扑尺寸 | `4x4` |
| `output_dir` | 输出目录 | `output` |

### 7.3 流程步骤

1. **生成拓扑** - 调用 `topology_generator.py` 生成 JSON 配置
2. **验证配置** - 调用 `cpptlm_config.validator` 验证
3. **运行仿真** - 调用 `cpptlm_sim`（如已编译）
4. **生成报告** - 调用 `stats_annotator.py` 生成 HTML 报告
5. **统计监控** - 可选启动 `stats_watcher.py` Web 仪表板

### 7.4 示例

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

### 7.5 环境变量

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `ENABLE_WATCHER` | 启用 stats_watcher Web 仪表板 | `0` (禁用) |

```bash
# 启用 Web 仪表板
ENABLE_WATCHER=1 bash scripts/run_full_pipeline.sh mesh 4x4
# 访问 http://localhost:8050
```

---

## 8. 配置文件格式 (v3.1)

### 8.1 基本结构

```json
{
  "$schema": "./tgms_v3.1_schema.json",
  "version": "3.1",
  "modules": [
    {
      "name": "模块名",
      "type": "模块类型",
      "params": { ... },
      "port_spec": {          // 可选：显式端口规格（Phase 3.2）
        "module_name": "RouterTLM",
        "ports": [
          {"name": "NORTH", "role": "bi_directional", "bundle": "noc_flit", "width": 64}
        ]
      }
    }
  ],
  "groups": {
    "组名": ["模块通配符模式"]
  },
  "connections": [
    { "src": "源", "dst": "目标", "latency": 1, "bandwidth": 100 }
  ]
}
```

### 8.2 模块类型映射

| CppTLM 类型 | 说明 | 必需参数 | 默认端口 |
|-------------|------|---------|---------|
| `RouterTLM` | 路由器 | `node_x`, `node_y`, `mesh_x`, `mesh_y` | 5 端口 (N/E/S/W/Local) |
| `NICTLM` | 网络接口 | `node_id`, `mesh_x`, `mesh_y` | 2 端口 (PE/Network) |
| `MemoryTLM` | 内存 | - | 1 端口 (Target) |
| `CPUSim` | CPU | - | - |
| `CacheTLM` | 缓存 | - | 2 端口 (Initiator/Target) |
| `CrossbarTLM` | 交叉开关 | `num_ports` | 4 端口 (Bidirectional) |

**注意**: 如果未提供 `port_spec`，ModuleFactory 会根据模块类型使用默认端口规格（ADR-X.9 定义）。

### 8.3 连接语法

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

### 8.4 组通配符

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

## 9. 端到端示例

### 9.1 2x2 Mesh 仿真（Python API）

```python
from cpptlm_config.topology_adapter import TopologyAdapter
from cpptlm_config.validator import TopologyValidator
import json

# 1. 生成拓扑
adapter = TopologyAdapter.from_mesh(2, 2)
config = adapter.to_dict()

# 2. 验证
v = TopologyValidator(config)
result = v.validate()
assert result.is_valid

# 3. 导出
with open("mesh_2x2.json", "w") as f:
    json.dump(config, f, indent=2)

# 4. 运行仿真
# ./build/bin/cpptlm_sim mesh_2x2.json --cycles 10000
```

### 9.2 2x2 Mesh 仿真（命令行）

```bash
# 1. 生成并验证配置
python3 scripts/topology_generator.py --type mesh --size 2x2 --target cpptlm --output configs/mesh_2x2.json
python3 scripts/topology_validator.py configs/mesh_2x2.json -v

# 2. 运行仿真
./build/bin/cpptlm_sim configs/mesh_2x2.json --cycles 10000

# 3. 生成可视化
dot -Tpng topology.dot -o mesh.png
```

### 9.3 使用完整流程脚本

```bash
# 一步完成所有操作
bash scripts/run_full_pipeline.sh mesh 2x2 output

# 查看报告
xdg-open output/report.html  # Linux
open output/report.html       # macOS
```

### 9.4 自定义拓扑（Python）

```python
from cpptlm_config.builder import ConfigBuilder
from cpptlm_config.validator import TopologyValidator

# 1. 构建自定义拓扑
builder = ConfigBuilder()
builder.add_router("router_0_0", node_x=0, node_y=0, mesh_x=4, mesh_y=4)
builder.add_router("router_1_0", node_x=1, node_y=0, mesh_x=4, mesh_y=4)
builder.add_nic("ni0", node_id=0, mesh_x=4, mesh_y=4)
builder.add_cpu("cpu0")

# 2. 添加自定义连接
builder.add_connection("router_0_0.1", "router_1_0.3", latency=2)
builder.add_connection("router_0_0.4", "ni0.1")
builder.add_connection("ni0.0", "cpu0")

# 3. 验证并导出
config = builder.build()
v = TopologyValidator(config)
assert v.validate().is_valid
builder.export_json("custom_mesh.json")
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

## 10. 快速参考

### 命令汇总

```bash
# === 生成拓扑 ===
# 命令行
python3 scripts/topology_generator.py --type mesh --size 2x2 --target cpptlm --output configs/mesh.json

# Python API
python3 cpptlm_config/examples/mesh_2x2.py > mesh.json

# === 验证拓扑 ===
# 命令行
python3 scripts/topology_validator.py configs/mesh.json -v

# Python API
python3 -c "from cpptlm_config.validator import TopologyValidator; import json; \
  config=json.load(open('configs/mesh.json')); \
  print(TopologyValidator(config).validate().is_valid)"

# === 完整流程 ===
bash scripts/run_full_pipeline.sh mesh 2x2 output/

# === 运行仿真 ===
./build/bin/cpptlm_sim configs/mesh.json --cycles 10000

# === 生成可视化 ===
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
