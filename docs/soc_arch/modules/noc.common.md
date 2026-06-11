# noc.common 微架构文档

> **类别**: noc > (common)
> **状态**: 🟡 规划中（跨 Phase 7.E 共享概念）
> **Header**: (无独立文件 — 概念文档)
> **蓝图来源**: gem5 `src/mem/ruby/network/`（Garnet 2.0）+ `src/mem/ruby/network/BasicLink.hh`
> **首版 commit**: 蓝图（来自调研 §2.4）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

---

## 1. 设计目标（蓝图）

本文件是 **NoC 路径的跨 phase 概念文档**——记录 v0 RouterTLM/NICTLM/LinkTLM 的设计概念、抽象接口、字段语义对位。**与 gem5 对位**: `gem5::GarnetNetwork` + `GarnetRouter` + `NetworkLink/CreditLink`（~5000 行 C++/Python，CppTLM 用现有 3 个类简化）。

**目标读者**: `noc-router.md` (✅ v0) / `noc-nic.md` (✅ v0) / `noc-link.md` (✅ v0) / Phase 7.E 未来 GPU 内部 mesh 集成者。

## 2. 通用概念（规划）

### 2.1 NoC 抽象层次

```
┌─────────────────────────────────────────────────────────────┐
│                  NoC 抽象层次                                  │
├─────────────────────────────────────────────────────────────┤
│ Application: JSON params { topology, num_routers, flit_size }│
│   ↓ 构造                                                       │
│ GarnetNetwork (v2.2 引入，Phase 7.E 启用)                     │
│   ├─ Routers: vector<RouterTLM>                              │
│   ├─ Links: vector<LinkTLM> (NetworkLink + CreditLink)      │
│   ├─ NICs: vector<NICTLM> (PE 侧 ↔ Net 侧 packetize)         │
│   └─ Topology: Mesh / Torus / Crossbar / Ring / Custom       │
│   ↓ 路由算法                                                   │
│ RoutingAlgorithm (v0 简化：XY 路由，Phase 7.E+ 真实算法)      │
│   - Dimension-order / Odd-Even / Adaptive                     │
│   ↓ 流量控制                                                   │
│ FlowControl (v0 简化：Credit-based，Phase 7.E+ 多机制)        │
│   - Credit-based / On-off / VC-based                          │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 关键术语

| 术语 | 含义 | v0 现状 | Phase 7.E+ |
|------|------|---------|-----------|
| **Flit** | flow control unit（最小传输单位） | `NoCFlitBundle`（REQUEST=0/RESPONSE=1/CREDIT=2） | 同 v0 |
| **Packet** | 由多个 flit 组成 | `NICTLM::FLITS_PER_PACKET=4` | 同 v0 |
| **VC** | Virtual Channel（虚拟通道） | **无** | Phase 7.E+ 真实 VC（4-8 VC/port） |
| **Router** | 路由节点 | `RouterTLM` 5 端口 (N/E/S/W/Local) 六阶段 | 同 v0 + 真实 VC allocator |
| **Link** | 物理链路 | `LinkTLM` 延迟+Credit | 同 v0 + NetworkLink/CreditLink 分离 |
| **NIC** | Network Interface | `NICTLM` 双端口 Cache↔NoC packetize | 同 v0 + multi-VC |
| **Routing** | 路径选择算法 | XY 路由（v0 硬编码） | 真 Dimension-order / Adaptive |
| **Flow Control** | 流量控制 | Credit-based（v0） | Credit / On-off / VC-based |
| **Topology** | 网络拓扑 | Crossbar (CrossbarTLM) | Mesh / Torus / Crossbar / Ring / Custom |
| **Latency** | 端到端延迟 | 0 (NoC 内部 0 延迟) | 真实 link_latency + router_latency |
| **Throughput** | 吞吐 | 0（v0 无吞吐模型） | 真实 bytes/cycle/link |
| **Buffer Depth** | 链路缓冲 | 0（v0 无 back-pressure 真实模型） | 4-8 flits/VC |

### 2.3 v0 三模块分工

```
┌─────────────────────────────────────────────────────────────┐
│  v0 NoC 简化架构 (已实施)                                     │
│                                                             │
│  ┌─────────┐  PE 侧 (CacheBundle)   ┌─────────┐             │
│  │   PE    │◄──────────────────────►│  NICTLM │             │
│  │ (Cache) │  Packetize/Depacketize │         │             │
│  └─────────┘                        └────┬────┘             │
│                                          │ Net 侧           │
│                                          │ (NoCFlitBundle)  │
│                                          ▼                  │
│                                ┌──────────────────┐         │
│                                │   LinkTLM        │         │
│                                │   (delay+credit) │         │
│                                └────┬─────────────┘         │
│                                     │                       │
│                                     ▼                       │
│                          ┌──────────────────┐              │
│                          │   RouterTLM      │              │
│                          │   5 端口 + XY 路由│              │
│                          │   + Credit 控流  │              │
│                          └──────────────────┘              │
└─────────────────────────────────────────────────────────────┘
```

## 3. 设计决策

| # | 决策 | 采纳方案 |
|---|------|----------|
| **D1** | Flit 大小 | 32B (NoCFlitBundle data 字段宽度) |
| **D2** | Packet 组成 | 4 flits/packet (NICTLM::FLITS_PER_PACKET 硬编码) |
| **D3** | 路由算法 | XY 路由（v0 硬编码），Phase 7.E+ 真实算法 |
| **D4** | 流量控制 | Credit-based（v0），Phase 7.E+ 多机制 |
| **D5** | 拓扑 | 通用 Crossbar (CrossbarTLM)，Mesh/Torus Phase 7.E+ |
| **D6** | 链路延迟 | `LinkTLM::link_latency_`（v0 可配，0-100 cycle） |
| **D7** | 缓冲深度 | **无真实模型**（v0 back-pressure 简化） |
| **D8** | VC | **无**（v0 单 VC/port） |
| **D9** | 多 NoC 域 | **无**（v0 单 NoC） |

## 4. v0 实施对照表

| NoC 概念 | v0 CppTLM 实现 | 蓝图（gem5） | 简化程度 |
|----------|---------------|------------|----------|
| **Network** | 无 Network 类（仅 Router 集合） | `GarnetNetwork` | **未实施** |
| **Router** | `RouterTLM` (5 端口 + XY + Credit) | `GarnetRouter` | 简化：无 VC、无真实 router 微架构 |
| **Link** | `LinkTLM` (delay+credit 合并) | `NetworkLink` + `CreditLink` 分离 | 简化：合并 NetworkLink + CreditLink |
| **NIC** | `NICTLM` (双端口 Cache↔NoC) | `MessageBuffer` + 各类 NIC | 简化：固定 4 flits/packet |
| **Routing** | XY 硬编码 | 多算法可配 | **固定** |
| **Flow Control** | Credit-based | Credit / On-off / VC | **固定** |
| **Topology** | Crossbar (CrossbarTLM) | Mesh / Torus / Crossbar / 自定义 | **固定 Crossbar** |
| **Topology DSL** | **无**（JSON 端口索引语法） | Ruby 配置文件 | **简化** |
| **VC Allocator** | **无** | 真实 VC allocator | **未实施** |
| **Switch Allocator** | **无** | 真实 switch allocator | **未实施** |

## 5. Phase 7.E 演进路径

### 5.1 GPU 内部 mesh

gem5 蓝图：`CUs ↔ GPU Crossbar ↔ TCC` 通过 `GarnetRouter` mesh 互连。
v0 现状：`RouterTLM` 已存在，但无 mesh 拓扑构造器。
Phase 7.E 目标：
1. 新建 `NoCTopologyBuilder` 类（接受 JSON `topology="4x4_mesh"`，构造 16 个 RouterTLM + 24 条 LinkTLM）
2. 新建 `RoutingAlgorithm` 抽象基类（XY / Odd-Even / Adaptive）
3. RouterTLM 扩展为模板化 `RouterTLM<NUM_VC>`（默认 1 VC）
4. NIC 扩展支持 multi-VC

### 5.2 dGPU 离散 NoC

gem5 蓝图：`Disjoint_VIPER`（双独立 NoC：APU 内部 mesh + dGPU 内部 mesh）。
v0 现状：无 disjoint NoC 支持。
Phase 7 备选 dGPU 目标：
1. `GarnetNetwork` 类支持 `disjoint=true` 参数
2. 双 NoC 通过 `BridgeTLM`（延迟 100-500 cycle）桥接
3. 跨 NoC coherence 通过 SnoopFilter 协调

## 6. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/mem/ruby/network/Network.hh` | (v0 无 Network 类) | Phase 7.E 引入 GarnetNetwork |
| `src/mem/ruby/network/simple/SimpleLink.hh` | `LinkTLM` | 合并 NetworkLink + CreditLink |
| `src/mem/ruby/network/simple/Switch.hh` | `RouterTLM` | 简化：无 VC allocator |
| `src/mem/ruby/network/simple/Throttle.hh` | (v0 无 Throttle) | Phase 7.E+ 引入 |
| `src/mem/ruby/network/Topology.hh` | (v0 无 Topology) | Phase 7.E+ 引入 |
| `src/mem/ruby/network/RoutingUnit.hh` | XY 硬编码 | 模板化 |
| `src/mem/ruby/network/NetDest.hh` | (v0 无 NetDest) | Phase 7.E+ 引入（多播支持） |
| `src/mem/ruby/network/MessageBuffer.hh` | (v0 用 NoCFlitBundle FIFO) | 同语义但简化 |

## 7. NoC 风险

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **NoC 内部 0 延迟**——v0 跨 router 跳转无延迟 | 中 | 中 | v0 简化（端到端延迟 = sum(link_latency_)） |
| R2 | **无 back-pressure**——v0 总是 ready | 高 | 中 | v0 简化；Credit 控流部分通过 LinkTLM 模拟 |
| R3 | **XY 路由硬编码**——不支持 torus/odd-even | 中 | 中 | Phase 7.E 模板化 |
| R4 | **Packet 长度固定 4 flits**——大消息需多 packet | 中 | 中 | 文档明确限制；Phase 7.E+ 支持变长 |
| R5 | **VC 缺失**——单 VC/port，head-of-line blocking | 高 | 中 | Phase 7.E+ 引入 VC |
| R6 | **No topology DSL**——Mesh 拓扑需手写 JSON | 中 | 中 | Phase 7.E NoCTopologyBuilder |
| R7 | **跨域 NoC 桥接缺失**——dGPU 无法连接 APU NoC | 中 | 中 | Phase 7 备选 dGPU BridgeTLM |
| R8 | **throughput 0**——v0 无吞吐模型 | 中 | 中 | Phase 7.E+ 真实 bytes/cycle/link |

## 8. 实施路径

### 8.1 Phase 7.E 步骤

1. 实施 `NoCTopologyBuilder`（JSON → Router/Link 集合）
2. 实施 `RoutingAlgorithm` 抽象基类 + XY / Odd-Even / Adaptive 三实现
3. 模板化 `RouterTLM<NUM_VC>`（默认 NUM_VC=1 保持 v0 行为）
4. 加 Catch2 测试：`test/test_noc_topology.cc`
5. 新增 `configs/gpu_mesh_4x4.json`（4 CU × 4 CU 内部 mesh）

### 8.2 Phase 7 备选 dGPU 步骤

1. 实施 `GarnetNetwork` 类（含 disjoint NoC 支持）
2. 实施 `BridgeTLM` 跨 NoC 桥接
3. SnoopFilter 跨 NoC 协调

### 8.3 验收标准

- [ ] Phase 7.E：mesh 4×4 拓扑端到端运行
- [ ] Phase 7.E：3 种 routing 算法可切换
- [ ] Phase 7.E：VC 真实生效（NUM_VC=4）
- [ ] Phase 7 备选 dGPU：disjoint NoC 桥接延迟注入

### 8.4 估计工作量

- Phase 7.E 基础版（NoCTopology + Routing + VC）: 4-5 周
- Phase 7 备选 dGPU（disjoint + Bridge）: 2-3 周
- **总计: 6-8 周**

## 9. 决策点

### D3 路由算法默认

- **Q**: 默认 XY 还是 Adaptive？
- **状态**: 留待 Phase 7.E 设计时确定
- **建议**: XY（v0 一致；Adaptive 需拥塞信息）
- **依赖**: Phase 7.E+ 拥塞追踪

### D4 VC 数量

- **Q**: 默认 NUM_VC = 1（v0）还是 4（典型）？
- **状态**: 留待 Phase 7.E 设计时确定
- **建议**: NUM_VC=4（典型 mesh 路由器）
- **依赖**: 性能 vs 复杂度权衡

### D5 拓扑 DSL

- **Q**: NoCTopology DSL 风格（JSON 嵌套 vs 自定义 .topo）？
- **状态**: 留待 Phase 7.E 设计时确定
- **建议**: JSON 嵌套（与现有 JSON 配置一致）
- **依赖**: JSON 解析器扩展

### D6 跨域 NoC 延迟模型

- **Q**: 跨 NoC BridgeTLM 默认延迟？
- **状态**: 留待 Phase 7 备选 dGPU 设计时确定
- **建议**: 200 cycle (PCIe 物理层)
- **依赖**: PCIe 物理层规范

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.4）
- **2026-06-12**: B3 批次设计 — 提取 D1-D9 + 蓝图对齐 + 风险列表
- **Phase 7.E (未来)**: NoCTopologyBuilder + Routing 算法 + VC 实施
- **Phase 7 备选 dGPU (未来)**: disjoint NoC + 跨 NoC Bridge
