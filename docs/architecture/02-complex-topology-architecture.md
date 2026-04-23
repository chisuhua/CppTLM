# CppTLM 复杂拓扑实现架构文档

**版本**: v2.2 (规划中)
**作者**: CppTLM Team
**日期**: 2026-04-23
**状态**: 架构设计文档

---

## 1. 现状分析

### 1.1 CppTLM 当前拓扑架构

#### 1.1.1 模块注册体系

CppTLM 采用**双注册表**机制：

```
┌─────────────────────────────────────────────────────────────┐
│                    REGISTER_OBJECT                           │
│  (modules.hh - Legacy SimObject 派生类)                     │
│  例: CPUSim, MemorySim, CacheSim, Router, TrafficGenTLM     │
└─────────────────────────────────────────────────────────────┘
                           +
┌─────────────────────────────────────────────────────────────┐
│                    REGISTER_CHSTREAM                        │
│  (chstream_register.hh - ChStreamModuleBase 派生类)         │
│  例: CacheTLM, MemoryTLM, CrossbarTLM                      │
└─────────────────────────────────────────────────────────────┘
```

#### 1.1.2 JSON 配置格式

```json
{
  "modules": [
    { "name": "cpu0", "type": "TrafficGenTLM", "config": { ... } },
    { "name": "l1_0", "type": "CacheTLM" },
    { "name": "xbar", "type": "CrossbarTLM" },
    { "name": "mem0", "type": "MemoryTLM" }
  ],
  "groups": {
    "cpus": ["cpu0", "cpu1", "cpu2", "cpu3"]
  },
  "connections": [
    { "src": "cpu0", "dst": "l1_0", "latency": 1 },
    { "src": "group:cpus", "dst": "xbar.0", "latency": 1 },
    { "src": "xbar.0", "dst": "mem0", "latency": 2 }
  ]
}
```

#### 1.1.3 实例化流程 (ModuleFactory::instantiateAll)

```
Step 1: loadAndInclude(config)     - 加载 JSON + 解析 $include
Step 2: 创建所有模块实例           - 根据 type 在注册表中查找
Step 3: 解析 groups               - 定义模块组
Step 4: 实例化 SimModule 内部配置  - 处理 config 字段
Step 5: 创建端口                   - ConnectionResolver 处理连接
Step 6: 建立连接                   - MasterPort → SlavePort
Step 7: StreamAdapter 注入         - ChStream 模块特殊处理
```

### 1.2 当前问题

| 问题 | 描述 | 影响 |
|------|------|------|
| **类型名不匹配** | `topology_generator.py` 生成 `MeshRouter`/`Processor`，但注册表只有 `Router`/`CPUSim` | 无法直接使用生成的配置 |
| **无层次化拓扑** | 所有模块扁平化，无 SOC/子系统概念 | 难以描述多核、GPU 等复杂拓扑 |
| **无路由器抽象** | 只有简单 Router 类，无网络拓扑构建能力 | 无法构建 Mesh/Ring/Crossbar 等 NoC |
| **无路由算法** | 连接是静态的，无 XY/West-First 等路由策略 | 无法模拟真实 NoC 行为 |
| **无 NetworkInterface** | 没有 NI 概念，CPU 直接连路由器 | 与 gem5/SystemC 建模方式不一致 |

---

## 2. gem5 拓扑架构分析

### 2.1 gem5 Ruby 拓扑体系

gem5 采用**Python 配置 + C++ 网络模型**的分离架构：

```
configs/topologies/          Python 拓扑定义
    ├── BaseTopology.py      基类 SimpleTopology
    ├── Mesh_XY.py          XY 路由 Mesh
    ├── MeshDirCorners_XY.py  4 角落目录的 Mesh
    ├── Crossbar.py         Crossbar 拓扑
    └── Pt2Pt.py            点到点拓扑

src/mem/ruby/network/       C++ 网络实现
    ├── Network.py          网络参数配置
    ├── BasicLink.py       链路定义
    ├── BasicRouter.py     路由器定义
    ├── Topology.cc        拓扑构建 + 路由表生成
    └── garnet2.0/         cycle-accurate 路由器实现
```

### 2.2 gem5 核心概念

#### 2.2.1 SimpleTopology 基类

```python
class SimpleTopology:
    """所有拓扑的基类"""
    description = 'BaseTopology'

    def __init__(self, controllers):
        self.nodes = controllers  # 所有终端节点 (CPU, Cache, Directory)

    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        """
        构建拓扑:
        1. 创建路由器
        2. 创建外部链路 (NI ↔ Router)
        3. 创建内部链路 (Router ↔ Router)
        4. 设置链路权重 (用于路由算法)
        """
        pass
```

#### 2.2.2 Mesh_XY 拓扑示例

```python
class Mesh_XY(SimpleTopology):
    description = 'Mesh_XY'

    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        num_routers = options.num_cpus
        num_rows = options.mesh_rows
        num_columns = num_routers / num_rows

        # 1. 创建路由器
        routers = [Router(router_id=i, latency=router_latency)
                   for i in range(num_routers)]
        network.routers = routers

        # 2. 创建外部链路 (CPU → Router)
        for i in range(num_routers):
            ExtLink(link_id=i, src_node=cpu_nodes[i], dst_node=routers[i],
                    src_outport="West", dst_inport="Local",
                    weight=1, latency=link_latency)

        # 3. 创建内部链路 (Router ↔ Router)
        for r in range(num_rows):
            for c in range(num_columns):
                rid = r * num_columns + c
                # 东向链路
                if c < num_columns - 1:
                    IntLink(link_id=..., src_node=routers[rid],
                            dst_node=routers[rid+1],
                            src_outport="East", dst_inport="West",
                            weight=1, latency=link_latency)
                # 南向链路
                if r < num_rows - 1:
                    IntLink(link_id=..., src_node=routers[rid],
                            dst_node=routers[rid+num_columns],
                            src_outport="South", dst_inport="North",
                            weight=2, latency=link_latency)  # XY 路由: Y 权重为 2
```

#### 2.2.3 链路类型

| 类型 | 方向 | 描述 |
|------|------|------|
| `ExtLink` | 双向 | 终端节点 (CPU/Cache) ↔ 路由器 |
| `IntLink` | 单向 | 路由器 ↔ 路由器 (可设不同权重) |

#### 2.2.4 路由算法

**表驱动路由** (默认):
- `Topology.cc` 根据拓扑图生成路由表
- 最短路径优先
- 链路权重影响路由选择

**XY 路由实现**:
```cpp
// src/mem/ruby/network/Topology.cc
// Y 方向链路权重 = 2, X 方向链路权重 = 1
// 结果: 先走 X, 再走 Y (XY 路由)
```

### 2.3 gem5 网络接口 (NI)

```cpp
// gem5/src/mem/ruby/network/NetworkInterface.hh
class NetworkInterface {
    MessageBuffer* inNode_queue;    // 来自终端节点
    MessageBuffer* outNode_queue;  // 到终端节点
    MessageBuffer* inNet_queue;    // 来自网络
    MessageBuffer* outNet_queue;   // 到网络

    void wakeup();                  // 处理消息
    void sendCredit();             // 流量控制
    void flitize();               // 分组化
};
```

### 2.4 Garnet 路由器微架构

```
        in_link_0 ──→ [NI] ──→ [Input VC] ──→ [Routing] ──┐
                          │                                │
        in_link_1 ──→ [NI] ──→ [Input VC] ──→ [Routing] ──→ [Crossbar] ──→ [Output VC] ──→ [NI] ──→ out_link_0
                          │                                │
        in_link_2 ──→ [NI] ──→ [Input VC] ──→ [Routing] ──┘
                          │
                      (更多输入)

Pipeline: 1-cycle default, 可配置多周期
```

---

## 3. 复杂拓扑架构设计

### 3.1 设计目标

1. **层次化拓扑**: 支持 SOC、子系统、集群的多层描述
2. **类型映射**: `topology_generator.py` 生成的类型与注册表匹配
3. **NoC 抽象**: Mesh/Ring/Crossbar/Hierarchical 路由器网络
4. **路由算法**: XY/West-First/自适应路由
5. **NetworkInterface**: 终端节点与网络的桥接

### 3.2 目标架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SOC Topology                                 │
├─────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │
│  │ Cluster 0   │  │ Cluster 1   │  │ Cluster 2   │  ┌───────────┐ │
│  │ ┌───┬───┐   │  │ ┌───┬───┐   │  │ ┌───┬───┐   │  │   GPU    │ │
│  │ │CPU│CPU│   │  │ │CPU│CPU│   │  │ │CPU│CPU│   │  │  Cluster │ │
│  │ └───┴───┘   │  │ └───┴───┘   │  │ └───┴───┘   │  └────┬────┘ │
│  │ ┌───┬───┐   │  │ ┌───┬───┐   │  │ ┌───┬───┐   │       │      │
│  │ │L1 │L1 │   │  │ │L1 │L1 │   │  │ │L1 │L1 │   │       │      │
│  │ └───┴───┘   │  │ └───┴───┘   │  │ └───┴───┘   │       │      │
│  │     ↓        │  │     ↓        │  │     ↓        │       ↓      │
│  │   [NI]       │  │   [NI]       │  │   [NI]       │    [NI]       │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └────┬────┘ │
│         │                 │                 │               │       │
│         └────────┬────────┴────────┬────────┴───────────────┘       │
│                  ↓                 ↓                                 │
│           ┌────────────┐     ┌────────────┐                         │
│           │  Mesh NoC  │     │  Crossbar │                         │
│           │  2x2 Router │     │  Interconnect │                    │
│           └──────┬──────┘     └──────┬──────┘                         │
│                  ↓                    ↓                                │
│           ┌────────────┐     ┌────────────┐                          │
│           │  Directory  │     │    Memory   │                          │
│           │  Controller │     │  Controller │                          │
│           └────────────┘     └────────────┘                          │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.3 模块类型扩展

| 类型 | 类别 | 描述 | 对应 gem5 |
|------|------|------|-----------|
| `Processor` | 终端 | 处理器核 | CPU |
| `Cache` | 终端 | 缓存控制器 | L1/L2/L3 Cache |
| `Directory` | 终端 | 目录控制器 | Directory |
| `Memory` | 终端 | 内存控制器 | Memory Controller |
| `NetworkInterface` | 终端 | 网络接口 | NetworkInterface |
| `Router` | 网络 | 路由器 | Switch/Router |
| `MeshRouter` | 网络 | Mesh 专用路由器 | Mesh Router |
| `Bus` | 网络 | 总线 | Bus |
| `Crossbar` | 网络 | 交叉开关 | Crossbar |

### 3.4 JSON 格式扩展

#### 3.4.1 层次化配置

```json
{
  "name": "soc_quad_core",
  "version": "2.2",

  "subnetworks": {
    "clusters": {
      "type": "mesh",
      "size": [2, 2],
      "router_type": "MeshRouter",
      "node_type": "Processor",
      "leaf_type": "Cache"
    },
    "interconnect": {
      "type": "crossbar",
      "ports": 8
    },
    "memory": {
      "type": "directory",
      "controllers": 4
    }
  },

  "connections": [
    { "src": "cluster.*.ni", "dst": "interconnect", "latency": 1 },
    { "src": "interconnect", "dst": "memory.ctrl.*", "latency": 2 }
  ],

  "routing": {
    "algorithm": "xy",
    "fallback": "adaptive"
  },

  "params": {
    "link_latency": 1,
    "router_latency": 1,
    "flit_width": 128,
    "vcs_per_vnet": 4
  }
}
```

#### 3.4.2 详细配置 (等价展开)

```json
{
  "name": "soc_quad_core",

  "modules": [
    // Cluster 0
    { "name": "cluster0_cpu0", "type": "Processor" },
    { "name": "cluster0_cpu1", "type": "Processor" },
    { "name": "cluster0_l1_0", "type": "Cache" },
    { "name": "cluster0_l1_1", "type": "Cache" },
    { "name": "cluster0_ni_0", "type": "NetworkInterface" },
    { "name": "cluster0_ni_1", "type": "NetworkInterface" },
    { "name": "cluster0_router", "type": "MeshRouter", "ports": 4 },

    // Cluster 1
    { "name": "cluster1_cpu0", "type": "Processor" },
    { "name": "cluster1_cpu1", "type": "Processor" },
    { "name": "cluster1_l1_0", "type": "Cache" },
    { "name": "cluster1_l1_1", "type": "Cache" },
    { "name": "cluster1_ni_0", "type": "NetworkInterface" },
    { "name": "cluster1_ni_1", "type": "NetworkInterface" },
    { "name": "cluster1_router", "type": "MeshRouter", "ports": 4 },

    // Interconnect
    { "name": "interconnect", "type": "Crossbar", "ports": 8 },

    // Memory Controllers
    { "name": "mem_ctrl_0", "type": "Directory" },
    { "name": "mem_ctrl_1", "type": "Directory" },
    { "name": "mem_ctrl_2", "type": "Directory" },
    { "name": "mem_ctrl_3", "type": "Directory" },
    { "name": "memory", "type": "Memory" }
  ],

  "connections": [
    // Cluster 0 内部
    { "src": "cluster0_cpu0", "dst": "cluster0_l1_0" },
    { "src": "cluster0_cpu1", "dst": "cluster0_l1_1" },
    { "src": "cluster0_l1_0", "dst": "cluster0_ni_0" },
    { "src": "cluster0_l1_1", "dst": "cluster0_ni_1" },
    { "src": "cluster0_ni_0", "dst": "cluster0_router.0" },
    { "src": "cluster0_ni_1", "dst": "cluster0_router.1" },

    // Cluster 1 内部
    { "src": "cluster1_cpu0", "dst": "cluster1_l1_0" },
    { "src": "cluster1_cpu1", "dst": "cluster1_l1_1" },
    { "src": "cluster1_l1_0", "dst": "cluster1_ni_0" },
    { "src": "cluster1_l1_1", "dst": "cluster1_ni_1" },
    { "src": "cluster1_ni_0", "dst": "cluster1_router.0" },
    { "src": "cluster1_ni_1", "dst": "cluster1_router.1" },

    // Cluster 间连接 (通过 Crossbar)
    { "src": "cluster0_router", "dst": "interconnect.0" },
    { "src": "cluster1_router", "dst": "interconnect.1" },

    // 内存连接
    { "src": "interconnect.0", "dst": "mem_ctrl_0", "latency": 2 },
    { "src": "interconnect.1", "dst": "mem_ctrl_1", "latency": 2 },
    { "src": "interconnect.2", "dst": "mem_ctrl_2", "latency": 2 },
    { "src": "interconnect.3", "dst": "mem_ctrl_3", "latency": 2 },
    { "src": "mem_ctrl_0", "dst": "memory" },
    { "src": "mem_ctrl_1", "dst": "memory" },
    { "src": "mem_ctrl_2", "dst": "memory" },
    { "src": "mem_ctrl_3", "dst": "memory" }
  ],

  "routing": {
    "algorithm": "table_based",
    "tables": {
      "cluster0_router": {
        "cluster0_ni_0": ["interconnect"],
        "cluster0_ni_1": ["interconnect"]
      },
      "interconnect": {
        "mem_ctrl_0": ["mem_ctrl_0"],
        "mem_ctrl_1": ["mem_ctrl_1"],
        "mem_ctrl_2": ["mem_ctrl_2"],
        "mem_ctrl_3": ["mem_ctrl_3"]
      }
    }
  }
}
```

---

## 4. 核心模块设计

### 4.1 NetworkInterface 模块

```cpp
// include/tlm/network_interface.hh
class NetworkInterface : public ChStreamModuleBase {
private:
    // 终端侧端口 (连接到 CPU/Cache)
    cpptlm::OutputStreamAdapter<bundles::NocBundle> req_out_;
    cpptlm::InputStreamAdapter<bundles::NocBundle> resp_in_;

    // 网络侧端口 (连接到 Router)
    cpptlm::InputStreamAdapter<bundles::NocBundle> req_in_;
    cpptlm::OutputStreamAdapter<bundles::NocBundle> resp_out_;

    // 缓冲区
    std::queue<NocMessage> inject_queue_;
    std::queue<NocMessage> eject_queue_;

    // 统计
    tlm_stats::StatGroup stats_;
    tlm_stats::Scalar& stats_packets_injected_;
    tlm_stats::Scalar& stats_packets_ejected_;
    tlm_stats::Distribution& stats_latency_;

public:
    NetworkInterface(const std::string& name, EventQueue* eq);

    void tick() override;
    void inject_packet(const NocBundle& packet);
    bool can_inject() const;
};
```

### 4.2 MeshRouter 模块

```cpp
// include/tlm/mesh_router.hh
class MeshRouter : public ChStreamModuleBase {
public:
    static constexpr unsigned MAX_PORTS = 6;  // N/S/E/W + local + VC

    struct Port {
        cpptlm::InputStreamAdapter<NocBundle> in;
        cpptlm::OutputStreamAdapter<NocBundle> out;
        std::vector<NocBundle> vc_buffers_[NUM_VCS];
        int vc_head_[NUM_VCS];
    };

private:
    unsigned rows_, cols_;  // Mesh 位置
    unsigned radix_;        // 端口数
    Port ports_[MAX_PORTS];

    // 路由表
    std::vector<std::vector<int>> routing_table_;

    // 路由计算
    int route_xy(int dst_x, int dst_y);

public:
    MeshRouter(const std::string& name, EventQueue* eq,
               unsigned rows, unsigned cols, unsigned radix = 5);

    void tick() override;
    const char* get_module_type() const override { return "MeshRouter"; }
};
```

### 4.3 Router 基类

```cpp
// include/tlm/router.hh
class Router : public ChStreamModuleBase {
protected:
    unsigned radix_;  // 端口数
    unsigned latency_;  // 流水线级数

    std::vector<Port> ports_;
    RoutingAlgorithm* routing_algo_;

public:
    Router(const std::string& name, EventQueue* eq,
           unsigned radix, unsigned latency = 1);

    virtual int compute_route(int in_port, const NocHeader& header) = 0;
    void tick() override;
};

// 子类实现不同路由算法
class XYRouter : public Router {
public:
    XYRouter(const std::string& name, EventQueue* eq, unsigned radix)
        : Router(name, eq, radix) {}

    int compute_route(int in_port, const NocHeader& header) override;
};

class WestFirstRouter : public Router {
public:
    WestFirstRouter(const std::string& name, EventQueue* eq, unsigned radix)
        : Router(name, eq, radix) {}

    int compute_route(int in_port, const NocHeader& header) override;
};
```

---

## 5. topology_generator.py 重构

### 5.1 类型映射表

```python
# scripts/topology_generator.py

# CppTLM 注册表类型映射
CPPTLM_TYPE_MAP = {
    # 终端类型
    'Processor': 'CPUSim',       # 处理器
    'Cache': 'CacheTLM',         # 缓存
    'Memory': 'MemoryTLM',       # 内存
    'Directory': 'DirectoryCtrl', # 目录控制器
    'NetworkInterface': 'NITLM',  # 网络接口

    # 网络类型
    'Router': 'Router',          # 通用路由器
    'MeshRouter': 'Router',       # Mesh 路由器 (使用通用 Router)
    'Bus': 'BusSim',             # 总线
    'Crossbar': 'CrossbarTLM',   # 交叉开关
}

def generate_mesh(self, rows, cols, node_type='Processor'):
    # 生成节点时使用映射后的类型
    cpp_type = CPPTLM_TYPE_MAP.get(node_type, node_type)
    self.graph.add_node(node_id, type=cpp_type)
```

### 5.2 生成器接口

```python
class TopologyGenerator:
    def __init__(self, name, target='cpptlm'):
        """
        Args:
            name: 拓扑名称
            target: 'cpptlm' 或 'gem5' 或 'systemc'
        """
        self.target = target
        self.graph = nx.DiGraph()

    def generate_mesh(self, rows, cols,
                      router_type='MeshRouter',
                      node_type='Processor') -> 'TopologyGenerator':
        """生成 2D Mesh 拓扑"""
        # 根据 target 生成对应类型名
        cpp_type = CPPTLM_TYPE_MAP.get(node_type, node_type)
        router_cpp_type = CPPTLM_TYPE_MAP.get(router_type, router_type)

        # ... 创建路由器和节点
        return self

    def generate_ring(self, num_nodes, node_type='Processor') -> 'TopologyGenerator':
        """生成 Ring 拓扑"""
        # ...

    def generate_hierarchical(self, levels, factor,
                              router_type='Router',
                              leaf_type='Processor') -> 'TopologyGenerator':
        """生成层次化树状拓扑"""
        # ...

    def generate_crossbar(self, num_inputs, num_outputs) -> 'TopologyGenerator':
        """生成 Crossbar 拓扑"""
        # ...

    def export_json_config(self, include_ni=True) -> dict:
        """
        导出 CppTLM JSON 配置

        Args:
            include_ni: 是否自动插入 NetworkInterface
        """
        # 自动生成 NI 并连接
        # 生成完整的 modules + connections
```

---

## 6. 路由算法实现

### 6.1 路由表驱动

```cpp
// src/noc/routing_table.hh
class RoutingTable {
private:
    std::vector<std::vector<std::vector<int>>> table_;  // [router][in_port][dest] → out_ports

public:
    void add_route(int router, int in_port, int dest, const std::vector<int>& out_ports);
    std::vector<int> lookup(int router, int in_port, int dest) const;
    void build_from_topology(const TopologyGraph& graph, const std::string& algo);
};
```

### 6.2 XY 路由实现

```cpp
// src/noc/routing_xy.cc
int XYRouter::compute_route(int in_port, const NocHeader& header) {
    int dest_x = header.dest_x;
    int dest_y = header.dest_y;

    // XY 路由: 先 X (水平), 后 Y (垂直)
    if (dest_x > my_x_) {
        return PORT_EAST;
    } else if (dest_x < my_x_) {
        return PORT_WEST;
    } else if (dest_y > my_y_) {
        return PORT_SOUTH;
    } else if (dest_y < my_y_) {
        return PORT_NORTH;
    } else {
        return PORT_LOCAL;  // 本地节点
    }
}
```

### 6.3 自适应路由

```cpp
// src/noc/routing_adaptive.cc
class AdaptiveRouting : public RoutingAlgorithm {
private:
    std::vector<std::vector<int>> link_load_;  // 链路负载估计

public:
    int compute_route(int in_port, const NocHeader& header) override {
        // 拥塞感知路由: 选择负载最小的可用输出端口
        int xy_route = xy_route(header);
        int min_load_port = xy_route;

        // 检查各方向负载，选择最空闲的
        for (int port : candidate_ports(header)) {
            if (link_load_[port] < link_load_[min_load_port]) {
                min_load_port = port;
            }
        }
        return min_load_port;
    }
};
```

---

## 7. 实现计划

### 7.1 Phase 1: 基础类型扩展 (1-2 周)

| 任务 | 描述 | 优先级 |
|------|------|--------|
| 添加 `NITLM` | NetworkInterface 模块 | P0 |
| 扩展 `topology_generator.py` | 添加类型映射表 | P0 |
| 修复现有配置 | 使 `noc_mesh.json` 可运行 | P1 |
| 添加 `Router` 多端口支持 | 支持 5 端口路由器 | P1 |

### 7.2 Phase 2: Mesh NoC (2-3 周)

| 任务 | 描述 | 优先级 |
|------|------|--------|
| 实现 `MeshRouter` | 5 端口 Mesh 路由器 | P0 |
| 实现 XY 路由算法 | 表驱动 + XY 路由 | P0 |
| 实现 `MeshTopologyGenerator` | Mesh 拓扑生成器 | P1 |
| 性能统计 | 链路利用率、路由器负载 | P2 |

### 7.3 Phase 3: 层次化拓扑 (2-3 周)

| 任务 | 描述 | 优先级 |
|------|------|--------|
| SOC 配置格式 | 层次化 JSON Schema | P0 |
| 子系统抽象 | Cluster/SubSystem 类 | P1 |
| 多级连接 | 集群内 + 集群间连接 | P1 |
| GPU 集群支持 | 异构拓扑 | P2 |

### 7.4 Phase 4: 高级特性 (3-4 周)

| 任务 | 描述 | 优先级 |
|------|------|--------|
| VC 支持 | 虚拟通道 | P1 |
| 自适应路由 | 拥塞感知 | P2 |
| Garnet 兼容 | cycle-accurate 路由器 | P3 |
| SystemC 集成 | TLM 2.0 导出 | P3 |

---

## 8. 参考实现

### 8.1 gem5 关键文件

```
configs/topologies/
    BaseTopology.py       - 拓扑基类
    Mesh_XY.py           - XY 路由 Mesh
    MeshDirCorners_XY.py - 多目录 Mesh

src/mem/ruby/network/
    Topology.cc          - 拓扑构建 + 路由表生成
    BasicLink.py         - 链路定义
    BasicRouter.py       - 路由器定义
    Network.py           - 网络参数

src/mem/ruby/network/garnet2.0/
    Router.cc           - 路由器微架构
    NetworkInterface.cc  - NI 实现
    RoutingUnit.cc      - 路由计算
    SwitchAllocator.cc  - 交换分配
```

### 8.2 CppTLM 目标文件

```
include/tlm/
    network_interface.hh  - NI 模块
    router.hh            - 路由器基类
    mesh_router.hh       - Mesh 路由器
    crossbar_router.hh   - Crossbar 路由器

src/noc/
    routing_table.cc     - 路由表
    routing_xy.cc        - XY 路由
    topology_builder.cc  - 拓扑构建器

scripts/
    topology_generator.py - 重构后的生成器
    noc_config.py        - NoC 配置工具
```

---

## 9. 附录

### 9.1 gem5 Mesh_XY 完整配置

```python
# configs/topologies/Mesh_XY.py
class Mesh_XY(SimpleTopology):
    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        num_routers = options.num_cpus
        num_rows = options.mesh_rows
        num_columns = num_routers / num_rows
        link_latency = options.link_latency
        router_latency = options.router_latency

        # 创建路由器
        routers = [Router(router_id=i, latency=router_latency)
                   for i in range(num_routers)]

        # 外部链路 (CPU → Router)
        for i in range(num_routers):
            ExtLink(link_id=i,
                    src_node=NodeID(i), dst_node=routers[i],
                    src_outport='Local', dst_inport='Local',
                    weight=0, latency=link_latency)

        # 内部链路 (Router ↔ Router)
        link_id = num_routers
        for r in range(num_rows):
            for c in range(num_columns):
                rid = r * num_columns + c

                # 东
                if c < num_columns - 1:
                    IntLink(link_id=link_id++,
                            src_node=routers[rid], dst_node=routers[rid+1],
                            src_outport='East', dst_inport='West',
                            weight=1, latency=link_latency)

                # 南
                if r < num_rows - 1:
                    IntLink(link_id=link_id++,
                            src_node=routers[rid], dst_node=routers[rid+num_columns],
                            src_outport='South', dst_inport='North',
                            weight=2, latency=link_latency)
```

### 9.2 术语表

| 术语 | 说明 |
|------|------|
| NI | Network Interface，网络接口 |
| VC | Virtual Channel，虚拟通道 |
| flit | Flow control digit，流量控制单元 |
| XY Routing | 先水平后垂直的确定性路由 |
| West-First | 西向优先的自适应路由 |
| Table-Based Routing | 查表驱动的路由方式 |
| Garnet | gem5 的 cycle-accurate NoC 模型 |
| Ruby | gem5 的内存系统框架 |

---

## 10. 变更历史

| 版本 | 日期 | 作者 | 描述 |
|------|------|------|------|
| 1.0 | 2026-04-23 | CppTLM Team | 初始文档 |
