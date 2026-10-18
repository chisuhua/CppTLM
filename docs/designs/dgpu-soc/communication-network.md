# CppTLM 模块间通信网络 — 全栈设计蓝图

> **文档状态**: ✅ v1.0, 2026-10
> **作者**: CppTLM build agent
> **范围**: 仿真框架层通信基础设施，类比 Gem5 Ruby network
> **对齐对象**: `01-hybrid-architecture-v2.1.md` §1.1-1.2（v2.1.10 修订）+ Phase 1-8 PCIe EP 全链路交付实况

---

## 0. 文档目的

`01-hybrid-architecture-v2.1.md` 已锁定 2026-04-22，10 个月演进未在主文档反映。本文是**通信网络专题**的补充蓝图，承载：
- **代码层全貌画像**（基于 `include/core/`, `include/framework/`, `include/bundles/` 实地读到的 9 个关键文件）
- **数据流的 5-hop 拆解**（从 host ABI 到 module internal data）
- **与 Gem5 Ruby MessageBuffer 的精确类比**（含差异警告）
- **设计决策的 4 件事实**（C++17 POD / 同步直调 / 编译期锁 / 类型擦除边界）

---

## 1. 通信网络 6 层全貌

```
┌── HOST (UE 进程) ──────────────────────────────────────────────────────┐
│ L7 用户态 driver / 23 ABI (cpptlm_emulator.h) ──── 冻结边界 [ABI]       │
└──┬──────────────────────────────────────────────────────────────────────┘
   │ C ABI（跨进程/SO，0 变更）
┌──▼────────── DGpuBoard (shell, src/tlm/gpu/dgpu_board_shell.cc) ───────┐
│ L6 板级门面: BAR0 MMIO / BAR1 FB fast-path / BAR2 VRAM aperture        │
│ simulation_mode → set_mode(Functional|Timing) [MODE SWITCH]            │
│ bind_memory_backings(): 7 处 programmatic 桥接 (current architecture debt)│
└──┬──┬──┬──┬──┬──┬──┬──┬──┬──────────────────────────────────────────────┘
   ▼  ▼  ▼  ▼  ▼  ▼  ▼  ▼  ▼
   L5 Bundle 类型 (PcieTlp / CrBundle / DoorbellBundle / MsixBundle / ...)
       ▼ [SOAC INTERNAL BOUNDARY: PcieTlpBundle 止步]
   L4 StreamAdapter 5 子类 (协议中立, 模板只绑 BundleT)
       ├─ StreamAdapter<Module, ReqB, RespB> (单端口)
       ├─ MultiPortStreamAdapter<Module, Rq, Rp, N> (N 端口同构)
       ├─ DualPortStreamAdapter<Module, PE_Rq, PE_Rp, Net_Rq, Net_Rp> (2 组非对称)
       ├─ BidirectionalPortAdapter<Module, B, N> (N 端口 + Credit)
       └─ Axi4StreamAdapter (3 通道: master_out / slave_in / cfg_slave_in) + Axi4Mapper (OOO)
   L3 ChStream 抽象 (Gem5 MessageBuffer 级别)
       ├─ InputStreamAdapter<BundleT> (valid/ready + Bundle 反序列化)
       ├─ OutputStreamAdapter<BundleT> (valid/ready + Bundle 序列化)
       └─ ChStreamInitiatorPort / ChStreamTargetPort (具体 Port 实例)
   L2 MasterPort / SlavePort (继承 SimplePort, +PortStats)
   L1 SimplePort + PortPair (同步直调, 见 §3.1)
   L0 Packet (tlm::tlm_generic_payload* payload + 5 流控 + 7 路由 + 4 ext API)
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 类比 Gem5 Ruby — 精确对照

| 维度 | Gem5 Ruby | CppTLM L3+L4 | 差异警告 |
|------|-----------|--------------|----------|
| **抽象对象** | MessageBuffer（opaque bytes FIFO） | `InputStreamAdapter<BundleT>` + `OutputStreamAdapter<BundleT>`（valid/ready 握手） | Gem5 是真正 opaque；CppTLM Bundle 是 typed POD，模块可 `req.address.read()` |
| **协议绑定点** | SLICC DSL 编译期烧进 protocol controller | `StreamAdapter<ModuleT, ReqBundleT, RespBundleT>` 模板参数编译期绑定 | 都是编译期，无运行时反射 |
| **协议切换成本** | 改 SLICC 文件 + 重新生成 C++ | 改 `StreamAdapter` 模板参数 + 重新实例化 | 类似量级 |
| **拓扑灵活性** | Ruby Topology 参数化（mesh/ring/butterfly/crossbar） | `MultiPortStreamAdapter<M,Rq,Rp,N>` (N 端口同构) + `BidirectionalPortAdapter<M,B,N>` (N 端口 + Credit) | Gem5 更成熟，CppTLM 通过 N 模板参数达到等效 |
| **PE↔Network 协议不同** | NetworkInterface（NI）做协议转换 | `DualPortStreamAdapter<Module, PE_Rq, PE_Rp, Net_Rq, Net_Rp>` | **完全等效**，NI/DualPort 都在 PE 侧和 Network 侧各一套 |
| **protocol controller 抽象** | SLICC 生成 state machine | 模块自身 `tick()` 实现（无 DSL） | Gem5 强在 DSL，CppTLM 强在 C++ 直接 |
| **Coherence 协议表达** | SLICC 的 state transition table | 无内建（Phase 7.C 才规划 6×6 state table，per ADR-SOC-01） | **CppTLM 缺 coherence 协议抽象** |
| **Opaque vs Typed** | 100% opaque（开发者自己解释） | 60% opaque（Bundle 字段命名即语义） | 60% typed 换 100% 编译期类型安全 |

**核心结论**: CppTLM L3+L4 已达 Gem5 MessageBuffer 水准，**60% Gem5 化**是合理取舍。**新增抽象层**不是出路。

---

## 3. 数据流的 5-hop 拆解（关键真相）

### 3.1 L1 SimplePort 的同步直调（不是 event-driven）

`include/core/simple_port.hh:52-58`：

```cpp
inline bool SimplePort::send(Packet* pkt) {
    if (pair) {
        SimplePort* other = (my_side == 0) ? pair->side_b : pair->side_a;
        return other->recv(pkt);   // ← 同步函数直调, 不是 event
    }
    return false;
}
```

**关键真相 1**: CppTLM 跨模块数据传输是**同步函数调用**——`MasterPort::send` 立即触发对端 `SlavePort::recv`，**不是** SystemC TLM 2.0 的 `b_transport` 阻塞调用，也不是 event-driven 调度。

**关键真相 2**: `Packet` 在 `send` 调用栈中是**栈上传递**（非堆分配），生命周期由调用方管理。

### 3.2 Packet 的真实结构（277 行，9 类字段）

`include/core/packet.hh:36-275` 的 `Packet` 类是个**复合头**：

| 字段 | 类型 | 作用 | 实例位置 |
|------|------|------|----------|
| `payload` | `tlm::tlm_generic_payload*` | **唯一数据载体** | 4 |
| `stream_id` | `uint64_t` | 流 ID（per ChStream Initiator） | 7 |
| `seq_num` | `uint64_t` | 流内序号 | 8 |
| `cmd` | `CmdType` (enum) | READ/WRITE/INVALID | 9 |
| `type` | `PacketType` (enum) | REQ/RESP/STREAM_DATA/CREDIT_RETURN | 10 |
| `src_cycle` / `dst_cycle` | `uint64_t` | 时间统计 | 11-12 |
| `original_req` | `Packet*` | 原始请求引用（用于 E2E 延迟） | 13 |
| `dependents` | `vector<Packet*>` | 依赖图 | 14 |
| `route_path` | `vector<string>` | 路由追踪 | 16 |
| `hop_count` | `int` | 跳数 | 17 |
| `priority` / `flow_id` / `vc_id` | `uint8_t`/`uint64_t`/`int` | 流控/优先级/VC | 18-20 |
| 4 个 TLM extension API | `get/set_transaction_id`, `add_trace`, `get/set_error_code` | TransactionContextExt + ErrorContextExt 桥接 | 22-26 |

**关键真相 3**: `Packet` 不是"单一数据容器"——它把 `tlm_generic_payload` 当 payload 字段用，**额外承载 9 类 20 字段**用于流控/路由/统计/ext。

### 3.3 一次完整数据流 hop（5 步调用链）

以 `CacheTLM → CrossbarTLM → MemoryTLM` 路径的 **CacheTLM.req_out → CrossbarTLM.req_in[0]** 为例：

```
Step 1: CacheTLM::tick()                          [module 内部, ch_stream 语义]
  └─ module->req_out().write(bundle)              [OutputStreamAdapter<CacheReqBundle>]
                                                    设置 stream_valid_=true, stream_data_=bundle

Step 2: StreamAdapter::tick()                      [cpptlm::StreamAdapter 模板, port.cc]
  └─ module->req_out().send(req_out_port_, PKT_REQ)
     └─ OutputStreamAdapter<CacheReqBundle>::send  [framework/stream_adapter.hh:136]
        ├─ Packet* pkt = PacketPool::get().acquire()       [拿 Packet]
        ├─ serialize_bundle(stream_data_, pkt->payload)    [memcpy 进 tlm_generic_payload 的 data ptr]
        └─ port->send(pkt)                                 [MasterPort::send]

Step 3: MasterPort::send                           [core/master_port.hh:57]
  └─ updateStats(pkt) + SimplePort::send(pkt)             [更新 req_count, byte_count]

Step 4: SimplePort::send                           [core/simple_port.hh:52]
  └─ other = pair->side_b;  return other->recv(pkt)       [同步函数直调到对端]

Step 5: ChStreamTargetPort::recvReq                [core/chstream_port.hh:67]
  ├─ adapter_->process_request_input(pkt, port_idx_)      [port_idx=0]
  └─ PacketPool::get().release(pkt)                       [立刻销毁 Packet, 不持有]

Step 6: StreamAdapter::process_request_input       [framework/stream_adapter.hh:232]
  └─ pkt->type == PKT_REQ → module->req_in().process(pkt) [按 pkt->type 路由]

Step 7: InputStreamAdapter<CacheReqBundle>::process [framework/stream_adapter.hh:42]
  ├─ deserialize_bundle(pkt->payload)                    [memcpy 反序列化]
  └─ stream_valid_ = true                                [Bundle 已就位, 等模块 tick() 消费]
```

**关键真相 4**:
- **Bundle 不离开 module 边界**——`CacheReqBundle` 在 CacheTLM 内部定义，序列化到 `tlm_generic_payload::data` 后传走，对端反序列化进同一个 Bundle 类型
- **Packet 是一次性**——`recvReq` 后立刻 `PacketPool::release(pkt)`，**ChStream 端不持有 Packet**
- **ChStream 端只持有 Bundle**（在 `InputStreamAdapter::stream_data_` 字段里），等下一拍 `module->tick()` 消费

### 3.4 functional/timing 双模式（替代 ImplMode 5 模式）

`include/tlm/vram_controller_tlm.hh` 提供的双模式，**才是 CppTLM 实际承担 ImplMode 抽象的能力**：

| ImplMode 原计划 | 实际等价物 | 触发方式 |
|----------------|------------|----------|
| TLM（粗粒度快速） | `simulation_mode="functional"` JSON 字段 | `VramControllerTLM::set_mode(FunctionalZeroDelay)` |
| RTL（周期精确） | `simulation_mode="timing"` | `VramControllerTLM::set_mode(TimingCycleApproximate)` |
| HYBRID | 单 config 内 functional + timing 模块共存 | JSON 混合 |
| COMPARE | ❌ 无实施计划 | — |
| SHADOW | ❌ 无实施计划 | — |

`pcie_encoding_latency_model.hh` / `pcie_phy_digital_ctrl_tlm.cc` 等 PCIe 模块**有独立的 timing 字段**，但**不通过 ImplMode 抽象**——而是各自 `set_*_delay()` API。

---

## 4. 5 种子类 StreamAdapter 全景

| 适配器 | 模板签名 | 用在模块 | N | Bundle | 关键特性 |
|--------|----------|---------|---|--------|----------|
| **StreamAdapter** | `<ModuleT, ReqBundleT, RespBundleT>` | CacheTLM / MemoryTLM / VramControllerTLM / GmmuTLM / CPUTLM / TrafficGenTLM / GPUTLM / StreamingMultiprocessorTLM / LinkTLM / HybridCacheWrapper | 1 | Req/Resp 同类型 | 标准单端口，1-1 连接 |
| **MultiPortStreamAdapter** | `<ModuleT, ReqT, RespT, N>` | CrossbarTLM (4) / CoherentXBarTLM (4) / ArbiterTLM (2/4) / PcieEndpointTLM (4) / PcieMemoryDevice (4) / LinkPhyMuxTLM (17) / SdmaEngineTLM (5) | N | Req/Resp 同类型 | **N 端口同构**，port_idx 路由 |
| **DualPortStreamAdapter** | `<ModuleT, PE_Rq, PE_Rp, Net_Rq, Net_Rp>` | NICTLM | 2 组非对称 | **PE 侧 = CacheReqBundle, Net 侧 = NoCFlitBundle** | **两侧协议不同**——最接近"前期抽象、后期特化"的现有实现 |
| **BidirectionalPortAdapter** | `<ModuleT, BundleT, N>` | RouterTLM (5) | N | 统一 Bundle（NoCFlitBundle） | **N 端口 + Credit 机制**（`pending_credits_` 1 周期延迟） |
| **Axi4StreamAdapter** | 非模板，3 通道实例 | PcieAxiAdapterTLM | 3 (master_out/slave_in/cfg_slave_in) | Axi4Bundle + Axi4LiteBundle | **OOO + rid 关联**（与 Axi4Mapper 组合） |

**最被低估的资产**: `DualPortStreamAdapter`（`include/framework/dual_port_stream_adapter.hh:35-37`）—— NICTLM 的 PE 侧走 `CacheReqBundle`、Network 侧走 `NoCFlitBundle`，由该 adapter 显式字段映射。**这是 CppTLM "两侧协议不同"模式的唯一代码实现**，但**主架构文档零覆盖**。

---

## 5. ModuleFactory 装配 Step 1-9 数据流

`src/core/module_factory.cc:142-866` 的 `instantiateAll()` 9 步流程：

| Step | 行号 | 职责 | 与通信相关 |
|------|------|------|-----------|
| 1 | 247-257 | `PluginLoader` dlopen 所有 .so | 间接 |
| 2 | 262-345 | 从 REGISTRY 创建所有 SimObject/SimModule 实例 | 间接 |
| 3 | 347-358 | 解析 groups dict | 否 |
| 4 | 361-389 | `sim_mod->instantiate(cfg)` 内部 JSON 递归 | 间接 |
| 4.5 | 391-415 | `simulate_instantiate(mod)` 对每个顶层 SimModule | **嵌套子模块递归** |
| 5 | 420-490 | `ConnectionResolver.resolveConnections` 处理 `connections[]` | **建立 port 对** |
| 6 | 492-615 | 遍历 deduplicated_connections | **建立 PortPair（src→dst）** |
| **7a** | **620-735** | `ChStreamAdapterFactory::create(type, obj)` | **创建适配器 + N 组 ChStreamPort** + **mirror 暴露** |
| **7b** | 755-847 | ChStream 连接：req_out[src_idx]→req_in[dst_idx] + resp_out[dst_idx]→resp_in[src_idx] | **完成 request/response 双向连接** |
| 8 | 737-753 | `StatsManager::register_group` 自动注册 | 否 |
| **9** | **849-859** | `incorporate_parent(nullptr)` 对每个 SimModule | **跨域 late-binding wiring**（如 ApuSoC peer cache） |

**关键真相 5**:
- **Step 7 mirror 机制**（module_factory.cc:691-701）：`pm.mirrorExistingDownstreamPort("req_out", ...)`——这是 ChStream 端口能通过 `soc.sdma.req_out` 跨 SimModule 路径被 JSON 引用的**关键实现**。主架构文档**完全没提**。
- **Step 9 incorporate_parent**（`src/tlm/cluster/apu_soc.cc:99-120`）：递归遍历整棵 `CacheTLM` 子树，调用 `xbar->registerPeerCache(full_name, req_out)`——这是**唯一**的跨域 late-binding wiring 入口。

---

## 6. Bundle 的"轻量级 + 编译期锁"双护栏

`include/bundles/bundle_serialization.hh:22-27` 的 `serialize_bundle`：

```cpp
template <typename BundleT>
bool serialize_bundle(const BundleT &bundle, void *buf, size_t len) {
  if (len < sizeof(BundleT)) return false;
  std::memcpy(buf, &bundle, sizeof(BundleT));
  return true;
}
```

**关键真相 6**:
- **`std::memcpy` 序列化**——是 `is_trivially_copyable` 的隐式约束（`include/bundles/cpphdl_types.hh:14-18` 注释明示 `bundle_base` 是空基类无虚函数）
- **C++17 std::memcpy 合法性**：在所有 Bundle 字段都是 POD（`ch_uint<N> = {uint64_t value_}` + `ch_bool = {bool value_}` + `bundle_base`）的前提下，memcpy 合法
- **跨进程不适用**——仅在同一仿真进程内（`bundle_serialization.hh:18-21` 注释明示）

`include/utils/port_contract_asserts.hh:1-12` 提供 `BUNDLE_FIELD_WIDTH` 编译期锁（已在 PcieTlpBundle / CrBundle / DoorbellBundle / MsixBundle 等 7+ Bundle 中使用）：

```cpp
BUNDLE_FIELD_WIDTH(bundles::PcieTlpBundle, kind, 8);
BUNDLE_FIELD_WIDTH(bundles::PcieTlpBundle, bar_index, 8);
BUNDLE_FIELD_WIDTH(bundles::PcieTlpBundle, offset, 64);
// ...
```

**关键真相 7**: Bundle 字段宽度是**编译期常量**——任何"运行时协议切换"都需重新编译，无法热插拔。这是 L5 的"硬约束"。

---

## 7. 架构债务清单（当前需承认的"不完美"）

| 编号 | 位置 | 现状 | 演进方向 |
|------|------|------|----------|
| **D1** | `dgpu_soc_minimal_v1.json` | 内部 `connections:[]` 只有 1 条 (`sdma.2→pcie_memory.0`) | 7 处 programmatic 桥接 (DGpuBoard::bind_memory_backings) 缺 JSON 表达 |
| **D2** | `sdma_engine_tlm.hh:82-86` | SDMA 5 端口**全 PcieTlpBundle** | 违反 ADR-DGPU-06 chip-internal vs board-level 边界；`cpptlm-dgpu-soc-physical-ip-port-boundary` change 推进 NoCFlit 切型 |
| **D3** | `FRAGMENT_MAPPER_DECISIONS.md` | Bridge/Mapper 两层定义完整，**主架构文档未引入** | 文档化补全（见本文档 §4.5 已提及） |
| **D4** | `docs-archived/DESIGN_creditstream.md` | CreditStream 36 行 stub 0 引用 | 实施层面以 `FcTokenBucket` + `BidirectionalPortAdapter` 为权威；`CREDIT_FLOW_USER_GUIDE.md` 已加漂移警告（v2.1.10） |
| **D5** | `impl_mode.hh` | 文件不存在，§4.7 文档自标"待实现" | 由 `simulation_mode` JSON + `VramControllerTLM::set_mode()` 承载（v2.1.10 修订） |
| **D6** | `dgpu_realip_refactor_change` Phase 0/1 | Adapter 复用度未量化 | 测试 `[realip][pin-contract]` 3 cases / 21 assertions 已锁 8 字段契约 |

**关键真相 8**: CppTLM **承认**这些债务，不假装完美；演进路径见 `openspec/changes/cpptlm-dgpu-soc-physical-ip-port-boundary` + `cpptlm-dgpu-soc-control-hub`（已在 `Proposed Draft` 状态）。

---

## 8. "前期抽象 + 后期特化"路径的最终答案

### 8.1 你（用户）的问题

> "在架构讨论阶段，避免过早固化某种通信总线"

### 8.2 答案

**当前 CppTLM 已经做到 60%**——L3+L4（ChStream + 5 种子类 StreamAdapter）**已 Gem5 化**，**模块不直接依赖具体协议**：

| 模块视角 | 不需要知道的"具体通信总线"细节 |
|----------|---------------------------------|
| 写 `tick()` 时 | 不知道 MasterPort/SlavePort 是谁连的（adapter 屏蔽） |
| 调 `req_out().write(bundle)` 时 | 不知道 Bundle 被 serialize 到哪个 tlm_payload data 指针 |
| 读 `req_in().data()` 时 | 不知道对端 MasterPort 来自哪个 module |

**模块设计**只需：
1. 继承 `ChStreamModuleBase`
2. 声明 `InputStreamAdapter<MyReqBundle>` / `OutputStreamAdapter<MyRespBundle>` 字段
3. 在 `tick()` 内按 `valid/ready` 语义读写
4. 用 `REGISTER_CHSTREAM(MyModule)` 注册

**所有"总线特化"都在 L4 StreamAdapter 5 子类中隔离**——后期换具体协议（如换成 `Axi4StreamAdapter` 替代通用 `StreamAdapter`）**不需要改模块代码**。

### 8.3 还未做（未来选项，非必须）

| 项 | 当前状态 | 触发条件 | 建议时点 |
|----|---------|---------|---------|
| L5 Bundle 命名重塑为 `TransportReqBase<ProtocolTag>` | 18 个 Bundle 命名直接带协议前缀（PcieTlp / Cr） | 出现 ≥2 个 chip-internal 协议特化需求 | **不启动**——SDMA 1 个用户不够"≥2" |
| LT/AT/CA 三档精度切换 | 仅 functional/timing 双模式（per ADR-DGPU-11） | CA 档缺真实需求方 | **不启动**——`cpptlm-dgpu-soc-control-hub` 提议已涵盖 |
| 跨抽象层切换（Carbon Swap&Play） | ❌ | 验证过"单向不可逆"风险（CppTLM 无 checkpoint 机制） | **不启动**——见 `docs/designs/dgpu-soc/architecture.md` §2.5 |

**核心结论**:
- **L3+L4 已 Gem5 化**——模块只与框架交互，不需要在架构阶段固化具体总线
- **L5 是工程取舍而非架构问题**——18 个 Bundle 命名保守（不破 frozen headers），不引入风险
- **新增抽象层 = 引入 over-engineering 风险**——历史教训（`ChStreamInitiatorPort<Bundle>` 模板化被 Momus REJECT）

---

## 9. 验证清单（v2.1.10 文档修订自检）

- [x] §1 修订：新增"通信框架层 vs 具体总线层"分层
- [x] §1 引用 Gem5 Ruby MessageBuffer 作为类比（参考坐标系）
- [x] §1 明确 Bundle 命名重塑是"未来选项而非必须"
- [x] §4.7 修订：删除 `impl_mode.hh` 实现引用，加 v2.1.10 状态段
- [x] `CREDIT_FLOW_USER_GUIDE.md` 加漂移警告头
- [x] 新建本文档（`communication-network.md`）含 6 层全貌 + 5-hop 拆解 + Gem5 对比 + 5 适配器表 + 9 步装配 + 8 关键真相
- [x] 8 项架构债务公开承认
- [x] "前期抽象 + 后期特化"路径的明确答案：L3+L4 已达 Gem5 水准

---

**版本**: v1.0
**创建日期**: 2026-10
**最后更新**: 2026-10
**批准状态**: 🟡 待 Oracle 评审
**维护者**: CppTLM build agent
