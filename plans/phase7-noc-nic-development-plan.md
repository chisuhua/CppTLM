# NoC + NIC TLM 模块开发计划

**版本**: 1.0
**状态**: 待评审
**日期**: 2026-04-14
**作者**: Sisyphus (Agentic)

---

## 一、架构理解：真实 NoC 建模

### 1.1 参考模型分析

基于对 Noxim、BookSim 2、Garnet（gem5 内）、归档死代码的综合分析，NoC 系统的标准建模框架如下：

```
┌─────────────────────────────────────────────────────────────┐
│                     NoC Architecture                        │
│                                                             │
│  ┌──────────┐      ┌──────────┐      ┌──────────┐          │
│  │  PE/Core │      │   PE/    │      │   PE/    │          │
│  │  NIC[0]  │◄─req─┤  Cache   │◄─req─┤  Memory  │          │
│  └────┬─────┘/resp─└────┬─────┘/resp─└────┬─────┘          │
│       │                 │                 │                 │
│  ┌────▼─────┐      ┌────▼─────┐      ┌────▼─────┐          │
│  │ Router[0]│─flit─│ Router[1]│─flit─│ Router[2]│          │
│  │ (N+4port)│      │ (5 port) │      │ (5 port) │          │
│  └──────────┘      └──────────┘      └──────────┘          │
│                                                             │
│  NIC = 网络接口 (PE ↔ Router 协议转换 + 包化/反包化)          │
│  Router = 包交换 (路由计算 + VC分配 + 仲裁 + 交叉开关)          │
│  Link = 物理连接 (延迟建模)                                    │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 Router 五阶段流水线（BookSim/Noxim 标准）

| 阶段 | 周期数 | 功能 |
|------|--------|------|
| **1. Buffer Write (BW)** | 1 | Flit 写入输入缓冲区的 VC slot |
| **2. Route Computation (RC)** | 1 | 头 Flit 计算输出方向 (XY 路由/自适应) |
| **3. VC Allocation (VA)** | 1-3 | 为 Packet 分配下游输出 VC |
| **4. Switch Allocation (SA)** | 1 | 仲裁跨路由器 VC→VC 的交叉开关通路 |
| **5. Switch Traversal (ST)** | 1 | 通过交叉开关，数据转发 |
| **6. Link Traversal (LT)** | 1-2 | 通过物理链路传输到下一跳 |

### 1.3 死代码架构分析总结

**已归档模块** (`docs-archived/dead-code-*`):

| 文件 | 模块 | 保留/淘汰 | 原因 |
|------|------|-----------|------|
| `flit_extension.hh` | FlitExtension | **保留设计理念** | TLM extension 机制正确，但需改用 Bundle 方案 |
| `noc_bundles.hh` | NoCReqBundle/NoCRespBundle | **保留并改进** | 字段设计合理，需适配当前 Bundle 风格 |
| `router_sc.hh` + `router_sc.cc` | ScTlmRouter | **重用架构** | SystemC SC_MODULE 架构正确，但需改为 ChStreamModuleBase |
| `router.cc` | Router (Gem5 style) | **重用设计思路** | VC 分配、路由计算、Flit 转发逻辑可复用 |
| `router_hash.hh` | Router_Hash | **淘汰** | 简化过度，不支持 VC/wormhole |
| `crossbar_rr.hh` | Crossbar_RR | **淘汰** | 与 Phase 5 的 CrossbarTLM 重复 |
| `nic.cc` | TerminalNode (NIC) | **保留设计思路** | 包化/反包化、Flit 注入逻辑可复用 |
| `nic_sc.cc` | TerminalNodeSb | **重用以适配** | SystemC 接口需改为 ChStreamModuleBase |

### 1.4 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| **流控机制** | Wormhole + Virtual Channel | Noxim/BookSim/Garnet 均采用 |
| **路由算法** | XY Routing (Dimension-Order) | 2D Mesh 死锁自由，可后续扩展为自适应 |
| **缓冲区组织** | 输入端口 × 虚拟通道 → FIFO | 每个输入端口含 N 个 VC |
| **仲裁策略** | Round-Robin (可配置) | 简单、公平，后续扩展为优先/信用 |
| **NIC 定位** | ChStreamModuleBase 派生 | 与现有 CacheTLM/CrossbarTLM 统一框架 |
| **Router 定位** | ChStreamModuleBase 派生 | 支持多端口（N 方向 + Local 端口） |
| **Bundle 方案** | 专用 NoC Bundle (非泛型 Cache) | NoC 需要 src/dst/routing metadata |

---

## 二、开发计划：Phase 7 (NoC 基础)

### Phase 7.1: NoC Bundle 定义

**文件**: `include/bundles/noc_bundles_tlm.hh`
**目标**: 定义 NoC 层专用的请求/响应 Bundle

```cpp
// NoCReqBundle — 路由器/交换机使用的传输消息
struct NoCReqBundle {
    ch_uint64 transaction_id;   // 事务 ID
    ch_uint32 src_node;         // 源节点 ID
    ch_uint32 dst_node;         // 目标节点 ID
    ch_uint64 address;          // 目标地址
    ch_uint64 data;             // 数据 (8 字节/拍)
    ch_uint8  size;             // 数据大小
    ch_uint8  vc_id;            // 虚拟通道 ID (0-3)
    ch_uint8  flit_type;        // 0=HEAD, 1=BODY, 2=TAIL, 3=HEAD_TAIL
    ch_uint8  hops;             // 跳数计数
    ch_bool   is_write;         // 读/写
};

// NoCRespBundle — 响应消息
struct NoCRespBundle {
    ch_uint64 transaction_id;   // 事务 ID
    ch_uint64 data;             // 响应数据
    ch_uint32 src_node;         // 响应源节点
    ch_uint32 dst_node;         // 响应目标节点
    ch_bool   is_ok;            // 成功/失败
    ch_uint8  error_code;       // 错误码
    ch_uint8  hops;             // 跳数
};
```

### Phase 7.2: RouterTLM 模块

**文件**: 
- `include/tlm/router_tlm.hh` (头文件)
- `src/tlm/router_tlm.cc` (实现，可选 inline)

**架构**:

```
RouterTLM (ChStreamModuleBase)
├── Port 0-3: N/E/S/W 方向 (多端口)
│   ├── req_in[i]   : InputStreamAdapter<NoCReqBundle>
│   ├── resp_out[i] : OutputStreamAdapter<NoCRespBundle>
├── Port 4: Local (连接 NIC)
│   ├── local_req_in  : InputStreamAdapter<NoCReqBundle>
│   ├── local_resp_out: OutputStreamAdapter<NoCRespBundle>
│
├── 内部状态:
│   ├── input_buffer[5][N_VC]  — 输入缓冲区 (方向×VC)
│   ├── vc_allocator           — VC 分配器
│   ├── switch_allocator       — 交叉开关仲裁器
│   ├── crossbar               — 交叉开关 (方向×方向)
│   └── routing_table          — XY 路由查询
```

**关键参数**（可 JSON 配置）:
- `num_virtual_channels`: VC 数量 (默认 4)
- `input_buffer_size`: 每 VC 缓冲深度 (默认 8)
- `routing_algorithm`: "XY" / "ODD_EVEN" (默认 "XY")
- `link_delay`: 链路延迟周期数 (默认 1)

**端口数量**: 5 (N+E+S+W+Local) → 多端口模式

### Phase 7.3: NIC TLM 模块

**文件**:
- `include/tlm/nic_tlm.hh` (头文件)
- `src/tlm/nic_tlm.cc` (实现，可选 inline)

**架构**:

```
NICTLM (ChStreamModuleBase)
├── Port 0: PE 侧 (连接 Cache/CPU)
│   ├── pe_req_in  : InputStreamAdapter<CacheReqBundle>    // 来自 Core
│   ├── pe_req_out : OutputStreamAdapter<CacheRespBundle>  // 返回 Core
│
├── Port 1: Network 侧 (连接 Router)
│   ├── net_req_out : OutputStreamAdapter<NoCReqBundle>    // 发往 NoC
│   ├── net_req_in  : InputStreamAdapter<NoCRespBundle>    // 来自 NoC
│
└── 内部状态:
    ├── packet_assembler    — 将 CacheReq 切分为 NoC Flits
    ├── packet_reassembler  — 将 NoC Flits 组装为 CacheResp
    ├── packet_id_counter   — 全局 Packet ID
    ├── vc_selector         — VC 选择策略 (默认 0)
    └── traffic_generator   — 可选: 内置流量生成器 [uniform, hotspot, transpose]
```

**核心职责**:
1. **包化 (Packetization)**: 接收 CacheReqBundle，计算 Flit 数量，生成 HEAD/BODY/TAIL Flits
2. **反包化 (Reassembly)**: 接收 NoC 响应的 Flits，重组为 CacheRespBundle 返回 PE
3. **流量注入**: 支持配置流量模式和注入率

---

## 三、Phase 计划详细拆解

### Phase 7A: Bundle + Router 基础 (Week 1-2)

| 步骤 | 任务 | 文件 | 输出 |
|------|------|------|------|
| 7A-1 | 定义 NoCReqBundle / NoCRespBundle | `include/bundles/noc_bundles_tlm.hh` | Bundle 头文件 |
| 7A-2 | RouterTLM 头文件骨架 | `include/tlm/router_tlm.hh` | 类定义 + 端口声明 |
| 7A-3 | XY 路由算法实现 | `include/tlm/router_tlm.hh` (inline) | `computeXYRoute(src_pos, dst_pos) → direction` |
| 7A-4 | 输入缓冲区 (Input Buffer + VC) | `include/tlm/router_tlm.hh` | VC 管理数据结构 |
| 7A-5 | REGISTER_CHSTREAM 更新 | `include/chstream_register.hh` | RouterTLM 注册 |
| 7A-6 | ChStreamAdapterFactory 多端口 adapter | `include/core/chstream_adapter_factory.hh` | REGISTER_MULTI_PORT |
| 7A-7 | JSON 配置扩展：支持 router 参数 | `configs/router_mesh_4x4.json` | 4×4 Mesh 拓扑配置 |

### Phase 7B: Router 完整流水线 (Week 2-3)

| 步骤 | 任务 | 文件 | 输出 |
|------|------|------|------|
| 7B-1 | VC 分配器 (Round-Robin) | `include/tlm/vc_allocator.hh` | allocate/release |
| 7B-2 | 交叉开关分配器 (RR arbiter) | `include/tlm/switch_allocator.hh` | grant/ arbitration |
| 7B-3 | Router tick() 完整流水线 | `src/tlm/router_tlm.cc` | BW→RC→VA→SA→ST→LT 全链路 |
| 7B-4 | 反压机制 (backpressure) | Router tick 内部 | 缓冲区满阻塞上游 |
| 7B-5 | Credit-based flow control (可选) | `src/tlm/router_tlm.cc` | 信用返回机制 |

### Phase 7C: NIC TLM 模块 (Week 3-4)

| 步骤 | 任务 | 文件 | 输出 |
|------|------|------|------|
| 7C-1 | NICTLM 头文件 + 双端口结构 | `include/tlm/nic_tlm.hh` | 类定义 |
| 7C-2 | Packetizer (CacheReq → NoC Flits) | `src/tlm/nic_tlm.cc` | splitIntoFlits() |
| 7C-3 | Reassembler (NoC Flits → CacheResp) | `src/tlm/nic_tlm.cc` | reassemblePacket() |
| 7C-4 | NICTLM tick() 双向转发 | `src/tlm/nic_tlm.cc` | PE→Net + Net→PE |
| 7C-5 | Traffic Generator (内置) | `src/tlm/nic_tlm.cc` | injectTraffic(pattern, rate) |
| 7C-6 | REGISTER_CHSTREAM 更新 | `include/chstream_register.hh` | NICTLM 注册 |

### Phase 7D: 端到端集成测试 (Week 4-5)

| 步骤 | 任务 | 文件 | 输出 |
|------|------|------|------|
| 7D-1 | 测试拓扑：NIC→Router→NIC 直连 | `configs/test/nic_router_nic.json` | 最简拓扑 |
| 7D-2 | Phase 7A 测试：Bundle 序列化 | `test/test_noc_bundles.cc` | Bundle 读写验证 |
| 7D-3 | Phase 7B 测试：Router 路由正确性 | `test/test_router_xy_routing.cc` | XY 路由全覆盖 |
| 7D-4 | Phase 7B 测试：Router VC/仲裁 | `test/test_router_vc_arbiter.cc` | VC 分配 + RR 仲裁 |
| 7D-5 | Phase 7C 测试：NIC 包化/反包化 | `test/test_nic_packetization.cc` | HEAD/BODY/TAIL 切分 |
| 7D-6 | Phase 7D 集成：NIC→Router→NIC 端到端 | `test/test_phase7_integration.cc` | `tag: [phase7]` |
| 7D-7 | 测试拓扑：2×2 Mesh (4 NIC + 4 Router) | `configs/test/mesh_2x2.json` | 完整 NoC 拓扑 |

### Phase 7E: 2D Mesh 拓扑 + 统计 (Week 5-6)

| 步骤 | 任务 | 文件 | 输出 |
|------|------|------|------|
| 7E-1 | JSON 拓扑生成：Mesh N×M 自动展开 | `configs/mesh_4x4.json`, `configs/mesh_8x8.json` | 拓扑文件 |
| 7E-2 | NoCStatistics 类 | `include/tlm/noc_statistics.hh` | 延迟/吞吐/跳数统计 |
| 7E-3 | 流量模式：Uniform Random | NIC TrafficGenerator | 均匀注入 |
| 7E-4 | 流量模式：Hotspot | NIC TrafficGenerator | 指定热点节点 |
| 7E-5 | 流量模式：Transpose / Butterfly | NIC TrafficGenerator | 确定性模式 |
| 7E-6 | NoC 主仿真入口 | `src/noc_main.cpp` | `./build/bin/cpptlm_noc` |

---

## 四、架构约束

### 4.1 与现有框架的一致性

| 维度 | 现有模式 | NoC 模块遵循方式 |
|------|----------|-----------------|
| **基类** | ChStreamModuleBase | RouterTLM, NICTLM 全部继承 |
| **注册** | REGISTER_CHSTREAM | 同时注册对象 + MultiPortAdapter/StandaloneAdapter |
| **Bundle 风格** | `.read()`/`.write()` 方法 | NoC Bundle 统一风格 |
| **多端口** | `set_stream_adapter(adapter[])` + `num_ports()` | RouterTLM 为 5 端口 |
| **双端口** | 2 组 InputStream/OutputStream (NIC) | NIC 为 PE + Net 两组端口 |
| **JSON 配置** | group + connection + latency | 扩展 router 参数 |

### 4.2 关键差异：NIC 双端口非对称

NICTLM 是唯一一个**两组非对称端口**的 TLM 模块：

```
// PE 侧 (面向 Core/Cache)
pe_req_in  : InputStreamAdapter<CacheReqBundle>     // 接收上层请求
pe_resp_out: OutputStreamAdapter<CacheRespBundle>   // 返回上层响应

// Network 侧 (面向 Router)
net_flit_out : OutputStreamAdapter<NoCReqBundle>    // 发送包化 Flits
net_flit_in  : InputStreamAdapter<NoCRespBundle>    // 接收响应 Flits
```

这需要一个特殊的 ChStreamAdapter 变体 — 建议用 **StandaloneDualPortAdapter** 或在现有框架中添加双端口支持。

### 4.3 Router 多端口方向映射

RouterTLM 的 5 端口映射：
```
Port 0: North  (−Y方向)
Port 1: East   (+X方向)
Port 2: South  (+Y方向)
Port 3: West   (−X方向)
Port 4: Local  (连接 NIC)
```

JSON 端口索引语法：
```json
{
  "connection": { "src": "router_0_0.0", "dst": "router_0_1.2" }
}
// router_0_0 的 North 端口 → router_0_1 的 South 端口
```

---

## 五、风险评估

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| ChStreamAdapter 不支持双端口非对称 | 中→高 | 高 | 先实现 standalone 模式，Phase 7 中期扩展 adapter |
| VC 分配死锁 | 低 | 高 | XY 路由天然死锁自由，先不实现自适应 |
| 缓冲区溢出导致仿真崩溃 | 中 | 高 | 反压机制从第一天就实现 |
| Phase 6 的测试覆盖可能受影响 | 低 | 中 | NoC 为全新 phase，不影响现有测试 |
| 多端口注册宏需要扩展 | 中 | 中 | 复用 CrossbarTLM 的 4 端口模式推广到 5 端口 |

---

## 六、验收标准

1. **编译通过**: `cmake --build build` 零警告
2. **测试通过**: `[phase7]` 标签测试 100% 通过 (预计 50+ 用例)
3. **拓扑可用**: 2×2 Mesh 配置能完整仿真 (NICTLM → Router → NICTLM 全链路)
4. **延迟统计**: 仿真结束输出平均延迟/吞吐量
5. **零回归**: 现有 `[phase6]`, `[chstream]`, `[crossbar]` 测试不受影响

---

## 七、文件清单

### 新增文件 (预计 12 个)

| 文件 | 类型 | 行数(估) |
|------|------|----------|
| `include/bundles/noc_bundles_tlm.hh` | 头文件 | ~80 |
| `include/tlm/router_tlm.hh` | 头文件 | ~150 |
| `src/tlm/router_tlm.cc` | 源文件 | ~250 |
| `include/tlm/nic_tlm.hh` | 头文件 | ~120 |
| `src/tlm/nic_tlm.cc` | 源文件 | ~300 |
| `include/tlm/vc_allocator.hh` | 头文件 | ~80 |
| `include/tlm/switch_allocator.hh` | 头文件 | ~80 |
| `include/tlm/noc_statistics.hh` | 头文件 | ~100 |
| `test/test_noc_bundles.cc` | 测试 | ~60 |
| `test/test_router_xy_routing.cc` | 测试 | ~120 |
| `test/test_router_vc_arbiter.cc` | 测试 | ~100 |
| `test/test_nic_packetization.cc` | 测试 | ~100 |
| `test/test_phase7_integration.cc` | 集成测试 | ~150 |

### 修改文件 (预计 3 个)

| 文件 | 修改内容 |
|------|----------|
| `include/chstream_register.hh` | 添加 RouterTLM, NICTLM 注册 |
| `include/core/chstream_adapter_factory.hh` | 可能扩展 2 端口 + 5 端口 adapter 支持 |
| `CMakeLists.txt` | 添加新源文件到 CORE_SOURCES |

### 新增配置 (3 个)

| 文件 | 内容 |
|------|------|
| `configs/test/nic_router_nic.json` | 最简拓扑: NIC→Router→NIC |
| `configs/test/mesh_2x2.json` | 2×2 Mesh |
| `configs/mesh_4x4.json` | 4×4 Mesh |

---

**总计**: 新增 ~30 文件修改, 新增 ~1690 行代码
