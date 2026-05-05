# 架构评审研究报告：CppTLM Phase 7 P0 阻塞问题

**日期**: 2026-04-25
**评审范围**: ModuleFactory、拓扑生成器、BidirectionalPortAdapter、noc_builder/noc_mesh
**参考项目**: gem5 (v24+)、BookSim2、Noxim、OMNeT++/HNOCS

---

## 执行摘要

对 CppTLM Phase 7 架构文档中声明的 5 个 P0 阻塞问题进行了全面调研。结论：**全部 5 个问题均为真实存在的架构缺陷**，但提出的修复方案需要进一步细化。建议优先级：G5（编译阻塞）> G1（功能缺失）> G3（运行时错误）> G4（格式不匹配）> G2（设计改进）。

| 问题 | 严重程度 | 验证状态 | 修复方案评估 |
|------|---------|---------|-------------|
| G1 ModuleFactory 不传递 JSON 参数 | **高** | ✅ 确认 | `set_config/get_config` 方案合理，但建议参考 gem5 CxxConfigParams |
| G2 topology_generator.py 不生成端口索引 | **中** | ✅ 确认 | 需要统一端口索引语法 |
| G3 BidirectionalPortAdapter 被错误处理 | **高** | ✅ 确认 | `isMultiEndpoint` 区分必要，需修改 Step 7 绑定逻辑 |
| G4 noc_builder.py 端口格式不兼容 | **中** | ✅ 确认 | 统一为数值索引或扩展 parsePortSpec |
| G5 noc_mesh.py 引用不存在类 | **高** | ✅ 确认 | 需要补充 VcRouter/TerminalNode 实现或删除该文件 |

---

## G1: ModuleFactory 未将 JSON 参数传递给构造函数

### 问题确认

**证据** (`src/core/module_factory.cc:49-71`):
```cpp
for (auto& mod : final_config["modules"]) {
    std::string name = mod["name"];
    std::string type = mod["type"];
    
    auto module_it = module_registry.find(type);
    if (module_it != module_registry.end()) {
        // 只传递 name 和 event_queue！
        SimModule* new_module = module_it->second(name, event_queue);
    }
}
```

**RouterTLM 构造函数签名** (`include/tlm/router_tlm.hh:151-153`):
```cpp
RouterTLM(const std::string& name, EventQueue* eq,
          unsigned node_x = 0, unsigned node_y = 0,
          unsigned mesh_x = DEFAULT_MESH_X, unsigned mesh_y = DEFAULT_MESH_Y);
```

**后果**: RouterTLM 总是被构造为 `(0, 0, 2, 2)`，无论 JSON 配置中的 mesh 尺寸和节点坐标如何。

### 参考实现 1: gem5 CxxConfigParams

gem5 使用代码生成 + 参数结构体模式：

```cpp
// gem5: src/sim/cxx_config.hh (已获取源码)
class CxxConfigParams {
public:
    virtual bool setParam(const std::string& name,
                          const std::string& value, const Flags flags);
    virtual bool setSimObject(const std::string& name, SimObject* simObject);
    virtual bool setPortConnectionCount(const std::string& name, unsigned int count);
    virtual SimObject* simObjectCreate() = 0;
};
```

gem5 的 `SimObject` 构造函数接收 `const Params& p`：
```cpp
// gem5: sim/sim_object.hh
SimObject(const Params& p);
```

参数结构体由 Python 类定义自动生成（`build_tools/sim_object_param_struct_cc.py`），每个 SimObject 类有对应的 `FooParams` 结构体。

**gem5 工厂调用链**:
1. Python 配置脚本设置参数 → `FooParams` 对象
2. `m5.instantiate()` 调用 `FooParams::create()` 
3. `create()` 调用 `new Foo(this)` 传递参数结构体

### 参考实现 2: Noxim 全局配置表

Noxim 使用全局 `GlobalParams` 单例存储所有配置：

```cpp
// Noxim: src/NoC.cpp (已获取源码)
void NoC::buildMesh() {
    // 所有参数从 GlobalParams 读取
    t[i][j]->r->configure(tile_id,
                          GlobalParams::stats_warm_up_time,
                          GlobalParams::buffer_depth,
                          grtable);
}
```

### 建议修复方案

**方案 A: 两阶段构造（推荐，与现有架构兼容）**

```cpp
// 阶段 1: ModuleFactory 用最小参数构造
SimModule* new_module = module_it->second(name, event_queue);

// 阶段 2: 如果模块支持配置，传递 JSON 参数
if (auto* configurable = dynamic_cast<Configurable*>(new_module)) {
    if (mod.contains("config")) {
        configurable->set_config(mod["config"]);
    }
}
```

**方案 B: 扩展构造函数签名（参考 gem5）**

```cpp
// 修改 CreateSimModuleFunc 签名
using CreateSimModuleFunc = std::function<SimModule*(
    const std::string&, EventQueue*, const nlohmann::json&)>;

// RouterTLM 构造函数改为：
RouterTLM(const std::string& name, EventQueue* eq, const json& config) {
    node_x_ = config.value("node_x", 0);
    node_y_ = config.value("node_y", 0);
    mesh_x_ = config.value("mesh_x", 2);
    mesh_y_ = config.value("mesh_y", 2);
}
```

**评估**: 方案 A 更轻量，不需要修改所有现有模块的构造函数。`set_config/get_config` 接口是合理的。建议增加 `Configurable` 混入接口：

```cpp
class Configurable {
public:
    virtual void set_config(const nlohmann::json& config) {}
    virtual nlohmann::json get_config() const { return {}; }
};
```

---

## G2: topology_generator.py 未生成 Mesh 连接的端口索引

### 问题确认

**topology_generator.py 输出** (`scripts/topology_generator.py:385-392`):
```python
for src, dst, attrs in self.graph.edges(data=True):
    connections.append({
        "src": src,        # 无端口索引！
        "dst": dst,        # 无端口索引！
        "latency": attrs.get("latency", 1),
    })
```

**ModuleFactory Step 7 期望的格式** (`src/core/module_factory.cc:366-376`):
```cpp
// 端口索引语法: "xbar.0" → xbar 的第 0 端口
auto [src_name, src_spec] = parsePortSpec(src_full);
auto [dst_name, dst_spec] = parsePortSpec(dst_full);
unsigned src_idx = 0, dst_idx = 0;
if (!src_spec.empty() && std::isdigit(src_spec[0])) 
    src_idx = std::stoul(src_spec);
```

**后果**: Mesh 拓扑中所有 Router 都使用端口 0 连接，导致 N/E/S/W/Local 端口混淆。

### 参考实现 1: gem5 Garnet 网络（命名端口）

gem5 使用**命名端口** + 方向字符串：

```python
# gem5: configs/topologies/Mesh_XY.py
int_links.append(IntLink(
    link_id=link_count,
    src_node=routers[east_out],
    dst_node=routers[west_in],
    src_outport="East",      # 命名端口
    dst_inport="West",       # 命名端口
    latency=link_latency,
    weight=1))
```

在 C++ 侧，gem5 使用 `PortDirection`（字符串别名）映射到路由器端口索引：
```cpp
// gem5: src/mem/ruby/network/garnet/GarnetNetwork.cc
PortDirection dst_inport_dirn = "Local";
m_routers[dest]->addInPort(dst_inport_dirn, net_link, credit_link);
```

### 参考实现 2: BookSim（数值通道索引）

BookSim 使用纯数值通道索引，通过公式计算：

```cpp
// BookSim: src/networks/kncube.cpp
int KNCube::_RightChannel(int node, int dim) {
    int base = 2*_n*node;   // 节点基址
    int off  = 2*dim;       // 维度偏移
    return base + off;      // 通道编号
}

int KNCube::_LeftChannel(int node, int dim) {
    int base = 2*_n*node;
    int off  = 2*dim + 1;
    return base + off;
}
```

对于 k-ary n-cube，每个节点有 `2*n` 个方向通道（每个维度左右各一），加上注入/弹出通道。

### 参考实现 3: Noxim（命名方向枚举）

Noxim 使用方向枚举映射到端口索引：

```cpp
// Noxim: src/NoC.h (方向定义)
#define DIRECTIONS 4
// 端口: 0=NORTH, 1=EAST, 2=SOUTH, 3=WEST, 4=LOCAL
```

### 建议修复方案

**推荐方案: 混合模式（数值索引 + 方向语义）**

CppTLM 已定义端口方向映射 (`include/tlm/router_tlm.hh:23-29`):
```cpp
enum class RouterPort : unsigned {
    NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3, LOCAL = 4
};
```

topology_generator.py 的 Mesh 生成应输出端口索引：

```python
# topology_generator.py 修正
def generate_mesh(self, rows, cols):
    # ... 创建节点 ...
    
    for r in range(rows):
        for c in range(cols):
            current = f"router_{r}_{c}"
            if c < cols - 1:
                east = f"router_{r}_{c+1}"
                # East=1 → West=3
                self.graph.add_edge(
                    current, east,
                    src_port=1, dst_port=3,  # 新增端口索引
                    latency=1
                )
            if r < rows - 1:
                south = f"router_{r+1}_{c}"
                # South=2 → North=0
                self.graph.add_edge(
                    current, south,
                    src_port=2, dst_port=0,
                    latency=1
                )
```

导出时格式化为 `"module.port_index"`：
```python
def export_json_config(self):
    connections = []
    for src, dst, attrs in self.graph.edges(data=True):
        src_port = attrs.get("src_port", 0)
        dst_port = attrs.get("dst_port", 0)
        connections.append({
            "src": f"{src}.{src_port}",
            "dst": f"{dst}.{dst_port}",
            "latency": attrs.get("latency", 1),
        })
```

**与 noc_builder.py (G4) 的关系**: noc_builder.py 使用 `.E_out/.W_in` 命名格式，与 topology_generator.py 的纯模块名格式不同。两者需要统一。

---

## G3: BidirectionalPortAdapter 在 module_factory.cc 中被当作单端口处理

### 问题确认

**ChStreamAdapterFactory 注册** (`include/core/chstream_adapter_factory.hh:102-109`):
```cpp
template<typename ModuleT, typename BundleT, std::size_t N>
void registerBidirectionalPortAdapter(const std::string& type) {
    table_[type] = [](SimObject* obj, const nlohmann::json*) {
        auto* mod = static_cast<ModuleT*>(obj);
        return new cpptlm::BidirectionalPortAdapter<ModuleT, BundleT, N>(mod);
    };
    port_count_[type] = N;   // 设置端口数为 N
}
```

**isMultiPort 判断** (`include/core/chstream_adapter_factory.hh:59-61`):
```cpp
bool isMultiPort(const std::string& type) const {
    return port_count_.count(type) > 0 && port_count_.at(type) > 1;
}
```

由于 `port_count_["RouterTLM"] = 5 > 1`，`isMultiPort("RouterTLM")` 返回 `true`。

**ModuleFactory Step 7 处理** (`src/core/module_factory.cc:297-359`):
```cpp
bool is_multi = factory.isMultiPort(type);  // true for RouterTLM
bool is_dual = factory.isDualPort(type);    // false for RouterTLM

if (is_dual) {
    // NICTLM 走这里
} else if (is_multi) {
    // RouterTLM 错误地走这里！
    std::vector<cpptlm::StreamAdapterBase*> adapter_vec(n_ports, adapter);
    ch_mod->set_stream_adapter(adapter_vec.data());
    // ❌ 没有调用 bind_ports_array()！
} else {
    adapter->bind_ports(...);
    ch_mod->set_stream_adapter(adapter);
}
```

**RouterTLM set_stream_adapter** (`src/tlm/router_tlm.cc:72-74`):
```cpp
void RouterTLM::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
    adapter_ = static_cast<PortAdapter*>(adapter);
}
```

**BidirectionalPortAdapter 需要端口绑定** (`include/framework/bidirectional_port_adapter.hh:50-62`):
```cpp
void bind_ports_array(
    std::array<MasterPort*, N> req_out,
    std::array<SlavePort*,  N> resp_in,
    std::array<MasterPort*, N> resp_out = {},
    std::array<SlavePort*,  N> req_in = {}
) {
    for (std::size_t i = 0; i < N; i++) {
        req_out_port_[i] = req_out[i];
        resp_in_port_[i] = resp_in[i];
        // ...
    }
}
```

**后果分析**:
1. RouterTLM 被判定为 `is_multi` → 进入多端口分支
2. 多端口分支创建 `adapter_vec(n_ports, adapter)` — N 个相同指针的数组
3. 调用 `ch_mod->set_stream_adapter(adapter_vec.data())`
4. RouterTLM 没有重写数组版本 `set_stream_adapter(adapters[])`， fallback 到 `ChStreamModuleBase` 默认实现：
   ```cpp
   virtual void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) {
       if (adapters) set_stream_adapter(adapters[0]);
   }
   ```
5. 最终只保存了 `adapters[0]`（即 BidirectionalPortAdapter 单指针）
6. **致命问题**: `bind_ports_array()` 从未被调用，`req_out_port_[i]` 全部为 `nullptr`
7. tick() 时所有端口发送失败：`if (req_out_port_[i])` 永远为 false

### 参考实现: gem5 Port 绑定

gem5 在拓扑构建阶段显式调用 `addInPort/addOutPort`：
```cpp
// gem5: GarnetNetwork.cc
_routers[dest]->addInPort(dst_inport_dirn, net_link, credit_link);
_routers[src]->addOutPort(src_outport_dirn, net_link, credit_link);
```

### 建议修复方案

**需要区分三种适配器类型**：

| 类型 | 端口数 | 绑定方法 | 代表模块 |
|------|--------|---------|---------|
| 单端口 | 1 | `bind_ports()` | CacheTLM, MemoryTLM |
| 多端口（独立）| N | `bind_port_pair(idx, ...)` × N | CrossbarTLM |
| **双向端口（统一）**| N | `bind_ports_array(...)` | **RouterTLM** |
| 双端口非对称 | 2 组 | `bind_pe_ports() + bind_net_ports()` | NICTLM |

**修改 ChStreamAdapterFactory**:

```cpp
enum class AdapterCategory {
    SINGLE,           // 单端口
    MULTI_INDEPENDENT, // 多端口独立（CrossbarTLM）
    MULTI_BIDIRECTIONAL, // 双向端口统一（RouterTLM）← 新增
    DUAL_ASYMMETRIC   // 双端口非对称（NICTLM）
};

class ChStreamAdapterFactory {
    // 新增方法
    bool isBidirectionalPort(const std::string& type) const {
        return bidirectional_types_.count(type) > 0;
    }
    
private:
    std::unordered_set<std::string> bidirectional_types_;  // 新增集合
};
```

**修改 ModuleFactory Step 7**:

```cpp
bool is_multi = factory.isMultiPort(type);
bool is_dual = factory.isDualPort(type);
bool is_bidirectional = factory.isBidirectionalPort(type);  // 新增

if (is_bidirectional) {
    // RouterTLM 专用路径
    auto* bi_adapter = static_cast<cpptlm::BidirectionalPortAdapter<
        tlm::RouterTLM, bundles::NoCFlitBundle, 5>*>(adapter);
    
    // 从 ch_req_out/ch_resp_in 等收集端口数组
    std::array<MasterPort*, 5> req_out_arr, resp_out_arr;
    std::array<SlavePort*, 5>  resp_in_arr, req_in_arr;
    for (unsigned i = 0; i < n_ports; i++) {
        req_out_arr[i]  = ch_req_out[name][i];
        resp_in_arr[i]  = ch_resp_in[name][i];
        resp_out_arr[i] = ch_resp_out[name][i];
        req_in_arr[i]   = ch_req_in[name][i];
    }
    bi_adapter->bind_ports_array(req_out_arr, resp_in_arr, 
                                  resp_out_arr, req_in_arr);
    ch_mod->set_stream_adapter(adapter);  // 单指针
}
```

**替代方案（更简洁）**: 让 `BidirectionalPortAdapter` 实现 `bind_ports()` 并在其中调用 `bind_ports_array()`，将端口收集逻辑下沉到适配器内部。但这需要 ModuleFactory 在创建端口后重新组织传递方式。

---

## G4: noc_builder.py 使用不兼容的端口命名格式

### 问题确认

**noc_builder.py 输出格式** (`python/noc_builder.py:42-48`):
```python
connections.append({"src": f"{current}.E_out", "dst": f"{east}.W_in"})
connections.append({"src": f"{east}.W_out", "dst": f"{current}.E_in"})
```

**ModuleFactory parsePortSpec** (`src/core/module_factory.cc:22-28`):
```cpp
std::pair<std::string, std::string> parsePortSpec(const std::string& full_name) {
    size_t dot_pos = full_name.find('.');
    if (dot_pos == std::string::npos) return {full_name, ""};
    return {full_name.substr(0, dot_pos), full_name.substr(dot_pos + 1)};
}
```

**ModuleFactory 端口索引解析** (`src/core/module_factory.cc:374-380`):
```cpp
if (!src_spec.empty() && std::isdigit(src_spec[0])) 
    src_idx = std::stoul(src_spec);
// E_out 不是数字 → src_idx 保持为 0
```

**noc_builder.py 的端口命名语义**:
- `.E_out`: 东向输出端口
- `.W_in`: 西向输入端口
- `.S_out`: 南向输出端口
- `.N_in`: 北向输入端口

这与 CppTLM 的 ChStream 端口模型**根本不兼容**：
- ChStream 端口是**双向请求/响应对**：`req_out`/`resp_in`/`req_in`/`resp_out`
- noc_builder 假设的是**单向物理通道**：`E_out` → `W_in`
- ModuleFactory Step 7 自动创建**双向**连接（req + resp 两条路径）

### 参考实现: gem5 链接方向模型

gem5 明确区分单向内部链接（路由器之间）和双向外部链接（控制器到路由器）：

```python
# gem5: configs/topologies/Mesh_XY.py
# 内部链接：单向（允许每方向权重不同）
int_links.append(IntLink(
    src_node=routers[east_out],
    dst_node=routers[west_in],
    src_outport="East",   # 源路由器输出端口名
    dst_inport="West",    # 目标路由器输入端口名
))

# 外部链接：双向（控制器 ↔ 路由器）
ext_links.append(ExtLink(
    ext_node=n,           # 控制器
    int_node=routers[router_id],  # 路由器
))
```

### 建议修复方案

**方案 A: 统一为数值索引（推荐）**

将 noc_builder.py 改为输出数值索引，与 topology_generator.py 一致：

```python
# noc_builder.py 修正
DIRECTION_EAST = 1
DIRECTION_WEST = 3
DIRECTION_SOUTH = 2
DIRECTION_NORTH = 0

if x < self.cols - 1:
    east = f"r{y}_{x+1}"
    connections.append({"src": f"{current}.{DIRECTION_EAST}", 
                        "dst": f"{east}.{DIRECTION_WEST}"})
```

**方案 B: 扩展 parsePortSpec 支持命名映射**

```cpp
// config_utils.hh 新增
unsigned parseDirectionName(const std::string& name) {
    if (name == "N" || name == "North" || name == "N_out") return 0;
    if (name == "E" || name == "East"  || name == "E_out") return 1;
    if (name == "S" || name == "South" || name == "S_out") return 2;
    if (name == "W" || name == "West"  || name == "W_out") return 3;
    if (name == "L" || name == "Local") return 4;
    if (std::isdigit(name[0])) return std::stoul(name);
    return 0;
}
```

**评估**: 方案 A 更简单且与现有 `parsePortSpec` 兼容。noc_builder.py 的双向连接语法（两个 connections 条目）与 ModuleFactory 的自动双向 ChStream 连接存在**根本语义冲突**，建议废弃 noc_builder.py，改用 topology_generator.py 作为唯一拓扑生成器。

---

## G5: noc_mesh.py 引用不存在的类

### 问题确认

**noc_mesh.py** (`python/noc_mesh.py:1-26`):
```python
from noc_builder import *  # 导入所有内容

routers = []
for y in range(4):
    for x in range(4):
        r = VcRouter(f"r{y}_{x}", router_id=y*4+x, network_dim=4)  # ❌ VcRouter 不存在
        r.instantiate()
        routers.append(r)

terminals = [TerminalNode(f"term{i}") for i in range(4)]  # ❌ TerminalNode 不存在

connections = build_mesh_connections(routers) + connect_terminals(terminals, routers)  # ❌ 函数不存在
```

**noc_builder.py 实际内容** (`python/noc_builder.py`):
- 定义了 `Topology` 抽象基类
- 定义了 `MeshTopology` 类
- 定义了 `build_gemsc_config()` 和 `generate_dot_file()`
- **没有** `VcRouter`、`TerminalNode`、`build_mesh_connections`、`connect_terminals`

**后果**: `python noc_mesh.py` 会直接抛出 `NameError`。

### 建议修复方案

**选项 1: 删除 noc_mesh.py**（推荐）

noc_mesh.py 的用途已被 `topology_generator.py` 完全覆盖：
- `topology_generator.py --type mesh --size 4x4` 生成等效的 mesh 拓扑
- 使用标准类型名（RouterTLM、CPUSim 等）而非不存在的 VcRouter
- 输出格式与 ModuleFactory 兼容

**选项 2: 修复 noc_mesh.py**

如果要保留，需要：
1. 定义 `VcRouter` 类（或改为使用 topology_generator.py 的类型映射）
2. 定义 `TerminalNode` 类
3. 实现 `build_mesh_connections()` 和 `connect_terminals()`
4. 统一端口格式为数值索引

**评估**: 选项 1 是最小维护负担的方案。noc_mesh.py 是早期原型代码，与当前架构脱节。topology_generator.py 已包含 CPPTLM_TYPE_MAP 解决类型映射问题，是更成熟的方案。

---

## 综合建议：架构修复路线图

### Phase 1: 紧急修复（编译/运行阻塞）

1. **删除或修复 noc_mesh.py**（G5）
2. **修改 module_factory.cc Step 7** 添加 `isBidirectionalPort` 分支（G3）
3. **修改 topology_generator.py** 输出端口索引（G2）

### Phase 2: 功能完善（参数传递 + 格式统一）

4. **实现 Configurable 接口** 和 ModuleFactory 参数传递（G1）
5. **统一 noc_builder.py 和 topology_generator.py** 的端口格式（G4）

### 代码变更清单

| 文件 | 修改类型 | 说明 |
|------|---------|------|
| `include/core/configurable.hh` | 新增 | Configurable 混入接口 |
| `include/core/chstream_adapter_factory.hh` | 修改 | 添加 `isBidirectionalPort()` + `bidirectional_types_` |
| `src/core/module_factory.cc` | 修改 | Step 7 添加 bidirectional 分支；Step 2 添加 `set_config` 调用 |
| `scripts/topology_generator.py` | 修改 | `generate_mesh()` 输出端口索引 |
| `python/noc_builder.py` | 修改 | 统一端口格式为数值索引（或标记为废弃） |
| `python/noc_mesh.py` | 删除 | 被 topology_generator.py 取代 |
| `include/tlm/router_tlm.hh` | 修改 | 可选：继承 `Configurable` |

### 与参考项目的架构对比

| 特性 | CppTLM (当前) | gem5 | BookSim | Noxim |
|------|--------------|------|---------|-------|
| 工厂参数传递 | ❌ 不支持 | ✅ CxxConfigParams | ⚠️ 全局 Config | ⚠️ 全局 GlobalParams |
| 端口索引格式 | 数值 `.0` | 命名字符串 | 数值通道 ID | 方向枚举 |
| Mesh 端口生成 | ❌ 缺失 | ✅ Python 脚本 | ✅ C++ 公式 | ✅ C++ 循环 |
| 双向端口适配 | ❌ 未绑定 | N/A (gem5 不同模型) | N/A | N/A |
| 拓扑配置生成 | Python + JSON | Python | C++ 解析 | YAML |

---

## 附录：参考代码来源

| 项目 | 文件 | URL |
|------|------|-----|
| gem5 | `cxx_config.hh` | https://raw.githubusercontent.com/gem5/gem5/develop/src/sim/cxx_config.hh |
| gem5 | `Mesh_XY.py` | https://gem5.googlesource.com/public/gem5/+/705351768c1becd685be1037ec1103051e58830a/configs/topologies/Mesh_XY.py |
| BookSim2 | `kncube.cpp` | https://raw.githubusercontent.com/booksim/booksim2/master/src/networks/kncube.cpp |
| Noxim | `NoC.cpp` | https://raw.githubusercontent.com/davidepatti/noxim/master/src/NoC.cpp |
| Noxim | `ConfigurationManager.cpp` | https://github.com/davidepatti/noxim/blob/master/src/ConfigurationManager.cpp |

---

*报告生成时间: 2026-04-25*
*研究方法: 代码审查 + 跨项目参考实现对比 + 架构文档追踪*
