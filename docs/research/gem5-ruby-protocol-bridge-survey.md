# Gem5 Ruby + Garnet 协议中立通信网络调研

> **调研目的**: 为 CppTLM PNN（Protocol-Neutral Network）方案提供 Gem5 Ruby 完整工作流对照
> **调研时间**: 2026-10
> **作者**: CppTLM build agent
> **状态**: 📋 Research Reference (不进入 ABI 契约)
> **关联方案**: [`docs/designs/dgpu-soc/pnn-protocol-neutral-network.md`](../designs/dgpu-soc/pnn-protocol-neutral-network.md)（已存在）
> **关联 ADR**: [`ADR-DGPU-13-pnn-protocol-neutral-network.md`](../adr/ADR-DGPU-13-pnn-protocol-neutral-network.md)（已存在）

---

## 0. 调研范围与版本

**调研范围**:
- Gem5 Ruby 子系统（Sequencer / CacheMemory / SLICC Controller / MessageBuffer / NetworkInterface / Garnet）
- Garnet 3.0 on-chip network model（**2026-10 校正：Gem5 main 已合并 Garnet 3.0；`--network=garnet2.0` 是 legacy 别名**）
- Ruby Topology 参数化
- SLICC 协议描述语言完整编译工作流（PLY parser + AST + C++ 后端 + SCons 集成）
- 一致性协议族全覆盖：Directory（MESI_Two_Level / MOESI_CMP_directory）/ Token（MOESI_CMP_token）/ Hammer（AMD probe filter）/ GPU（GPU_VIPER）/ CHI（ARM AMBA 5）/ HTM / CMP
- 性能/精度切换（Simple vs Garnet）

**调研版本**:
- Gem5 `main` 分支（2026-10 快照，本地 `/workspace/main/gem5/`）
- Garnet 3.0（`GarnetNetwork.hh:70` `garnetVersion = "3.0"`）
- SLICC 编译器：`src/mem/slicc/`（PLI/slicc-emitter 已废弃，纯 Python + PLY 实现）
- 11 类一致性协议：`src/mem/ruby/protocol/` 下 11 个 `.slicc` 入口（CHI 独立 `chi/` 子目录）

**调研方法**:
- webfetch 官方文档（gem5.org/documentation/general_docs/ruby/*）+ 本地源码深度扫描
- 3 份 subagent 并行调研：SLICC + CHI（librarian）、Garnet 网络（explore）、coherence 协议族（oracle），结果存于 `/tmp/opencode/gem5-{slicc-chi,garnet-network,coherence-protocols}-survey.md` 共 1054 行
- 与 CppTLM 实地代码（`include/core/`, `include/framework/`, `include/bundles/`）做逐文件映射
- 不复制任何代码片段（避免版权 + 保持调研文档独立性）

**配套深度调研文档**（新创建 2026-10）:
- **[`gem5-slicc-chi-survey.md`](gem5-slicc-chi-survey.md)** (299 行) — SLICC 编译链 + CHI 协议族
- **[`gem5-garnet-network-survey.md`](gem5-garnet-network-survey.md)** (256 行) — Garnet 3.0 内部 + 拓扑 + routing
- **[`gem5-coherence-protocols-survey.md`](gem5-coherence-protocols-survey.md)** (499 行) — Directory/Token/Hammer/GPU-VIPER/CHI 一致性协议族
- **[`gem5-messagebuffer-network-interface.md`](gem5-messagebuffer-network-interface.md)** (新, 2026-10) — MessageBuffer / NetworkInterface / vnet 协议消息流转详解
- **[`gem5-coherence-network-evolution.md`](gem5-coherence-network-evolution.md)** (新, 2026-10) — 一致性网络 20+ 年演进史 + 学术/工业对照 + CppTLM PNN 借鉴

---

## 1. Ruby 子系统结构（来自 gem5.org 官方文档）

> **来源**: [gem5.org/documentation/general_docs/ruby/](https://www.gem5.org/documentation/general_docs/ruby/)

**原文关键摘录**:
> "Ruby implements a detailed simulation model for the memory subsystem. It models inclusive/exclusive cache hierarchies with various replacement policies, coherence protocol implementations, interconnection networks, DMA and memory controllers, various sequencers that initiate memory requests and handle responses."

**Three key aspects**:
1. **Separation of concerns** – coherence protocol specifications are separate from replacement policies and cache index mapping; network topology is specified separately from implementation.
2. **Rich configurability** – almost any aspect affecting the memory hierarchy functionality and timing can be controlled.
3. **Rapid prototyping** – SLICC, a high-level specification language, is used to specify functionality of various controllers.

**核心架构图**（来自 GEMS 2005 ISCA Tutorial，已在 gem5.org 引用）:

```
   Core 0..N        DMA
     │              │
     ▼              ▼
  ┌─────────────────────┐
  │    RubyPort         │ (1 per core/thread)
  │    (Sequencer)      │
  └──────┬──────────────┘
         │ MessageBuffer
         ▼
  ┌─────────────────────┐
  │ SLICC-generated     │ ← coherence protocol MESI/MOESI/CHI
  │ CacheController     │   (编译器: src/mem/slicc/)
  └──────┬──────────────┘
         │ MessageBuffer
         ▼
  ┌─────────────────────┐
  │ NetworkInterface    │ (per controller, side: chip)
  └──────┬──────────────┘
         │ NetworkLink (Garnet flits)
         ▼
  ┌─────────────────────┐
  │ Garnet Router/NW    │ (topology-mesh/crossbar/etc)
  └─────────────────────┘
```

### 1.1 Protocol-independent 内存组件（来自 gem5.org/ruby 原文）

> "Protocol independent memory components comprises of the Sequencer, Cache Memory structure, Cache Replacement policies and the Memory controller."

**4 类组件**：
1. **Sequencer** — 1 per hardware thread/core
   - 职责: feeding memory subsystem with load/store/atomic requests
   - 接口: `Sequencer::makeRequest`（从 RubyPort::recvTiming 接收）
2. **Cache Memory** — instance for L1/L2/L3
   - 职责: set-associative cache structure (size, associativity, replacement policy)
3. **Replacement Policies** — modular from CacheMemory
   - 当前实现: LRU / Pseudo-LRU
4. **Memory Controller** — DRAM bank contention + refresh + close-page policy

**关键洞察**: 这 4 类组件**与 coherence 协议完全解耦**。任何 `.slicc` 文件改动只影响 SLICC-generated controller，不动这 4 类。

### 1.2 SLICC（来自 gem5.org/ruby/slicc 原文）

> "**SLICC** stands for **Specification Language for Implementing Cache Coherence**. It is a domain specific language that is used for specifying cache coherence protocols... SLICC is used for specifying the behavior of the state machine."

**SLICC 工作流**:
- 输入: `src/mem/ruby/protocol/*.slicc`
- 编译: `src/mem/slicc/` parser + code generator
- 输出: C++ controller classes（StateMachine, TransitionTable, Event, Action, InPort/OutPort）
- 与 MessageBuffer 接口: `mandatoryQueue` / `optionalQueue`

**支持的协议列表**（来自 gem5.org/ruby 列表）:
1. MI_example (1-level, 教学)
2. MESI_Two_Level (single chip, 2-level, strictly-inclusive)
3. MOESI_CMP_directory (multi-chip, 2-level, non-inclusive)
4. MOESI_CMP_token
5. MOESI_hammer (single chip, 2-level private, strictly-exclusive)
6. Garnet_standalone (无 coherence, 纯网络)
7. MESI_Three_Level (3-level, 基于 MESI_Two_Level + L0)
8. **CHI** — flexible, AMBA5 CHI, MESI 或 MOESI 缓存层

**CHI 协议特别重要**: 是 ARM AMBA 5 CHI 事务的灵活实现，**支持可配置 cache hierarchy**——证明 Gem5 后期切具体协议的真实工作流。

### 1.3 Interconnection Network 组件（来自 gem5.org/ruby/interconnection-network）

> "The interconnection network connects the various components of the memory hierarchy (cache, memory, dma controllers) together."

**4 个关键子组件**:
1. **Topology** — `configs/topologies/`（Python, runtime）+ `src/python/gem5/components/cachehierarchies/ruby/topologies/`（库 API）— *2026-10 校正：旧 `src/mem/ruby/network/topologies/` 已被移除*
2. **Routing** — 默认表查找（weight-based），可改 XY/Custom
3. **Flow Control** — Virtual Channel Flow Control（garnet）/ 带宽流控（simple）
4. **Router Microarchitecture** — Garnet **3.0** 流水线（详见 §1.4）

**Simple vs Garnet**:
- **Simple Network**（默认）— `src/mem/ruby/network/simple/` + `PerfectSwitch.cc`（message 粒度路由 + 仲裁）+ `Throttle.cc`（带宽预算流控）— 抽象掉 switch 内部细节
- **Garnet 3.0**（详细）— `src/mem/ruby/network/garnet/`（*2026-10 校正：`garnet2.0/` 目录已合并入 `garnet/`*）— cycle-accurate 5-stage router pipeline（garnet1.0 有独立 VA 级；garnet3.0 折叠进 SA-II，见 `SwitchAllocator.cc:81-88`）
- **切换**: 命令行 `--network=garnet` 或 `--network=simple`（`--network=garnet2.0` 仅作 legacy 别名，warn 后映射为 garnet，见 `configs/network/Network.py:131-137`）
- **HeteroGarnet 新增**: `NetworkBridge` 做 CDC/SerDes（`README.txt:72-78`）

**关键洞察**: **这是 CppTLM `simulation_mode: functional|timing` 的 Gem5 对应物**——通过命令行切换 simple vs detailed。Garnet 3.0 内部细节（pipeline + credit + flit_type + NI）见配套 [`gem5-garnet-network-survey.md`](gem5-garnet-network-survey.md) §A-C。

### 1.4 Protocol-independent 内存组件完整边界（来自 gem5.org/ruby 原文 + 本地源码）

> "Protocol independent memory components comprises of the Sequencer, Cache Memory structure, Cache Replacement policies and the Memory controller."

**4 类组件（+ 1 类 boundary）**：
1. **Sequencer** — 1 per hardware thread/core
   - 职责: feeding memory subsystem with load/store/atomic requests
   - 接口: `Sequencer::makeRequest`（从 RubyPort::recvTiming 接收）
   - 文件: `src/mem/ruby/system/Sequencer.{hh,cc}` / `src/mem/ruby/system/RubyPort.{hh,cc}`
2. **Cache Memory** — instance for L1/L2/L3
   - 职责: set-associative cache structure (size, associativity, replacement policy)
   - 文件: `src/mem/ruby/system/CacheMemory.{hh,cc}`（以 `RubyCache` 注册于 `src/mem/ruby/system/RubySystem.py`）
3. **Replacement Policies** — modular from CacheMemory
   - 当前实现: LRU / Pseudo-LRU / TreePLRU / Random（`src/mem/ruby/replacement_policies/`）
4. **Memory Controller** — DRAM bank contention + refresh + close-page policy
   - 文件: `src/mem/ruby/system/MemoryController.{hh,cc}` + DRAM 设备（`ext/dramsim/` 等）
5. **NetworkInterface 边界组件**（*2026-10 校正*）— NI 本身**部分 protocol-dependent**（切分 flit 时需知 control_msg_size 与 vnet 数量），但**对协议字段透明**——只切不解析
   - 文件: `src/mem/ruby/network/NetworkInterface.{hh,cc}` + `flitisizeMessage()`（`NetworkInterface.cc:373-465`）

**关键洞察（深化）**: 这 4+1 类组件**与 coherence 协议高度解耦**。任何 `.slicc` 文件改动只影响 SLICC-generated controller，不动这些组件。**NetworkInterface 是唯一的"半透"边界**——协议字段语义由 controller 解释，NI 只负责把 message 切成 flit、贴 vnet 路由标签、回收 credit。**这是 CppTLM PNN 方案"前期 abstract、后期具体协议"的工程化锚点**。

---

## 2. Garnet 3.0 详细架构（来自 gem5.org/ruby/garnet-2 + 本地源码校正）

> **来源**: [gem5.org/documentation/general_docs/ruby/garnet-2](https://www.gem5.org/documentation/general_docs/ruby/garnet-2)（原始论文） + 本地 `src/mem/ruby/network/garnet/`（garnetVersion="3.0"，见 `GarnetNetwork.hh:70`）
> **论文**: Agarwal et al. 2009 ISPASS "GARNET: A detailed on-chip network model inside a full-system simulator"
> **配套调研**: [`gem5-garnet-network-survey.md`](gem5-garnet-network-survey.md) — 256 行深度调研

**Garnet 3.0 特性**（*2026-10 校正——基于 `garnet/` 源码而非旧 doc*）:
- Cycle-accurate micro-architectural implementation of on-chip network router
- 1-cycle 流水线：BW/RC/SA/VS/ST 全部 1 cycle，LT 下一周期（**VA 已合并入 SA-II**，见 `SwitchAllocator.cc:81-88` "There is no separate VCAllocator stage like the one in garnet1.0"）
- 可配置多周期 router（topology 文件 per-router latency）
- 复用 gem5 Ruby 的 Topology + Routing infrastructure
- 任何 heterogeneous topology 都能建模（含 HeteroGarnet CDC/SerDes 桥）
- **5 组件**：GarnetNetwork / NetworkInterface / Router / NetworkLink / CreditLink（`src/mem/ruby/network/garnet/`）

### 2.1 5-Stage Router Pipeline（详细；*与§1.3 校正一致*）

来自 `src/mem/ruby/network/garnet/Router.cc:71-97` 精确顺序：

1. **Buffer Write (BW) + Route Compute (RC)** — `InputUnit::wakeup()`（`InputUnit.cc:76-140`）
   - HEAD/HEAD_TAIL 在 `InputUnit.cc:97` 调 `Router::route_compute()`（`Router.cc:161-165`）→ `RoutingUnit::outportCompute()`
2. **Switch Allocation (SA)** — `SwitchAllocator::wakeup()`（`SwitchAllocator.cc:90-98`）
   - SA-I 输入仲裁（`arbitrate_inports`，`SwitchAllocator.cc:110-146`，round-robin）
   - SA-II 输出仲裁 + **VC 分配**（`arbitrate_outports`，`SwitchAllocator.cc:162-267`）
   - `send_allowed()`（`SwitchAllocator.cc:283-338`）门控：HEAD 需空闲 VC、BODY 需 credit、ordered vnet 入队顺序检查
3. **VC Selection (VS) — 已折叠进 SA-II**
4. **Switch Traversal (ST)** — `CrossbarSwitch::wakeup()`（`CrossbarSwitch.cc:65-92`），`OutputUnit::insert_flit`
5. **Link Traversal (LT)** — `NetworkLink::wakeup()`（`NetworkLink.cc:82-110`），`clockEdge(m_latency)`

**Multi-cycle router**: per-router `pipe_stages`（见 `BasicRouter.py:36-39` `latency`），flit 等 `(latency-1)` cycles

### 2.2 Garnet 3.0 Network Components（含校正）

| 组件 | 作用 | CppTLM 对应（既有） |
|------|------|---------------------|
| **GarnetNetwork** | top-level: instantiate all NIs, routers, links. `GarnetNetwork.hh:70` `garnetVersion="3.0"` | `ModuleFactory` 装配层 |
| **NetworkInterface** | controller ↔ router bridge. `flitisizeMessage()`（`NetworkInterface.cc:373-465`）切分；`wakeup()` 入 port 内联重组（`NetworkInterface.cc:229-284`） | `DualPortStreamAdapter` 既有 |
| **Router** | `Router.cc:71-97` wakeup 5-stage + `SwitchAllocator`/`CrossbarSwitch` | `RouterTLM` 既有（缺 cycle-accurate 5-stage 详情） |
| **NetworkLink** | carries flits. `NetworkLink.cc:82-110` 流水线时延 | `MasterPort/SlavePort + PortPair` |
| **CreditLink** | carries VC/buffer credits. `CreditLink.hh:46` 是 `NetworkLink` 空子类 | `BidirectionalPortAdapter::pending_credits_` |
| **NetworkBridge**（*新增于 HeteroGarnet*） | CDC/SerDes 时钟域/位宽转换（`README.txt:72-78`） | 无（PNN P3 候选） |

**关键映射（深化）**: `NetworkInterface` 是**协议边界转换器**——controller 发 protocol message，NI 打包成 flit 进 network；controller 不知道网络细节。NI 还**双侧流控**——NI↔router 走 credit，NI↔controller 走 MessageBuffer。**这正是 PNN 方案的"ProtocolBridge"抽象的 Gem5 先例**。
- NI 接收 flit 的反向重组**没有单独命名函数**，逻辑内联于 `NetworkInterface::wakeup()`（`NetworkInterface.cc:229-284`）：TAIL/HEAD_TAIL → 重组 message → 推入 `outNode_ptr[vnet]`；非 tail → 直接回 credit 并删除 flit
- NI 死锁探测：`calculateVC()`（`NetworkInterface.cc:468-490`）当某 vnet 长期无空闲 VC 且计数超 `garnet-deadlock-threshold`（默认 50000）则 `panic`

**Garnet 2.0 特性**:
- Cycle-accurate micro-architectural implementation of on-chip network router
- 1-cycle pipeline 默认（BW/RC/SA/VS/ST 全部 1 cycle，LT 下一周期）
- 可配置多周期 router（topology 文件 per-router latency）
- 复用 gem5 Ruby 的 Topology + Routing infrastructure
- 任何 heterogeneous topology 都能建模
- 5 组件：**GarnetNetwork / NetworkInterface / Router / NetworkLink / CreditLink**

### 2.1 5-Stage Router Pipeline（详细）

来自 gem5.org/ruby/garnet-2 原文:

1. **Buffer Write (BW)** — incoming flit buffered in its VC
2. **Route Compute (RC)** — buffered flit computes output port
3. **Switch Allocation (SA)** — all buffered flits try to reserve switch ports
   - SA-I: input VCs request output ports (input arbiters)
   - SA-II: output ports break conflicts (output arbiters)
   - **ordered virtual networks** = *queueing* arbiters
   - **其他** = *round-robin*
4. **VC Selection (VS)** — winner of SA selects a free VC from output port
5. **Switch Traversal (ST)** — winning flits traverse crossbar switch
6. **Link Traversal (LT)** — flits traverse links to next routers (next cycle)

**默认**: BW/RC/SA/VS/ST 全部 1-cycle, LT next cycle.

**Multi-cycle**: 通过 topology file per-router latency 实现 (flit 等 (latency-1) cycles)

### 2.2 Garnet 2.0 Network Components

| 组件 | 作用 | CppTLM 对应（既有） |
|------|------|---------------------|
| **GarnetNetwork** | top-level: instantiate all NIs, routers, links | `ModuleFactory` 装配层 |
| **NetworkInterface** | controller ↔ router bridge. 1-flit control / 5-flit data（default） | `DualPortStreamAdapter` 既有 |
| **Router** | manages output link arbitration + flow control | `RouterTLM` 既有 |
| **NetworkLink** | carries flits. 3 types: EXT_OUT/EXT_IN/INT | `MasterPort/SlavePort + PortPair` |
| **CreditLink** | carries VC/buffer credits for flow control | `BidirectionalPortAdapter::pending_credits_` |

**关键映射**: `NetworkInterface` 是**协议边界转换器**——controller 发 protocol message，NI 打包成 flit 进 network；controller 不知道网络细节。**这正是 PNN 方案的"ProtocolBridge"抽象的 Gem5 先例**。

### 2.3 Garnet 3.0 配置参数（含 vcs_per_vnet 默认值校正）

| 参数 | 默认 | 含义 |
|------|------|------|
| `number_of_virtual_networks` | (by protocol，CHI=4，MOESI=3，GPU_VIPER=11) | max vnet 数 |
| `control_msg_size` | 8 bytes | control message size |
| `ni_flit_size` | 16 bytes (= 128 bits) | flit size, also `--link-width-bits` |
| `vcs_per_vnet` | **4**（`GarnetNetwork.py:44-54`，`vnet_type=response` 仅影响 data/ctrl 信用容量差异，不存在 escape VC） | VC per vnet |
| `buffers_per_data_vc` | 4（data msg 通常 5 flits） | flit-buffers per data VC |
| `buffers_per_ctrl_vc` | 1（ctrl msg 1 flit） | flit-buffers per ctrl VC |
| `routing_algorithm` | 0 (table) / 1 (XY) / 2 (custom) | runtime switch in `RoutingUnit::outportCompute()`（`RoutingUnit.cc:168-202`） |
| `garnet_deadlock_threshold` | 50000 | `NetworkInterface::calculateVC()` panic 阈值 |

**m_data_msg_size** = `block_size + control_msg_size`（typically cache block = 64 bytes + 8 = 72, 在 16-byte flits 下 = 5 flits）

### 2.4 Lifecycle of Network Traversal（深化 + file:line 引用）

**`NetworkInterface.cc::wakeup()`（`NetworkInterface.cc:229-456` 双段）**：
- *入 port 段*（:229-284）：收到 flit → TAIL/HEAD_TAIL 重组 message 推入 `outNode_ptr[vnet]` → 非 tail 直接回 credit
- *出 port 段*（:373-465）：`flitisizeMessage(msg)` 切分 → multicast → unicast 拆分 → `RouteInfo` 构建 → 入 `niOutVcs[vc]`
- 管理 NI↔router 的 credit（`NetworkInterface.cc:288-299` 收 credit；`530` 发扣 credit）
- output link 调度在全局 event queue

**`NetworkLink.cc::wakeup()`（`NetworkLink.cc:82-110`）**：
- 取 `link_srcQueue` 顶 flit → `set_time(clockEdge(m_latency))`（:100）→ 插入 linkBuffer → `link_consumer->scheduleEventAbsolute(clockEdge(m_latency))`（:102）
- Default latency: 命令行 `--link-latency`；Per-link latency: topology file

**`Router.cc::wakeup()`（`Router.cc:71-97` 精确顺序）**：
1. 循环 `m_input_units[*]->wakeup()`（:78-80）
2. 循环 `m_output_units[*]->wakeup()`（:88-90，**credit 更新先于 SA**，注释 :82-87）
3. `switchAllocator.wakeup()`（:93）→ SA-I → SA-II（含 VC alloc）
4. `crossbarSwitch.wakeup()`（:96）→ ST
5. Reschedule if any flit/credit ready

**`InputUnit.cc::wakeup()`（`InputUnit.cc:76-140`）**：
- 从 upstream router 读 input flit
- HEAD/HEAD_TAIL → RC（call `Router::route_compute()`，:97）→ `grant_outport`（:103）
- flit 插入 VC（:111）
- `pipe_stages==1` 直接 SA（:120-123），否则等 `pipe_stages-1` 周期（:124-134）

**`SwitchAllocator.cc::wakeup()`（`SwitchAllocator.cc:90-373`）**：
- `arbitrate_inports()`（:110-146）SA-I round-robin
- `arbitrate_outports()`（:162-267）SA-II：`vc_allocate()`（:185）→ `getTopFlit`（:189）→ `decrement_credit`（:217）→ `advance_stage(ST_)`（:220）→ `grant_switch`（:221）
- TAIL/HEAD_TAIL 释放 VC + 发 `free=true` credit（:224-235）；其他 flit 发 `free=false` credit（:236-240）
- `check_for_wakeup()`（:356-373）下周期仍有 ready SA flit → `schedule_wakeup(1)`

### 2.5 Garnet 3.0 vs Simple Network 真实差异（一表说清）

| 维度 | **Simple**（`src/mem/ruby/network/simple/`） | **Garnet 3.0**（`src/mem/ruby/network/garnet/`） |
|------|-----------------------------------------------|-------------------------------------------------|
| 转发粒度 | **message**（`PerfectSwitch.cc:185-275`） | **flit**（`NetworkInterface.cc:386-387`） |
| 流控 | 带宽预算（`Throttle.cc:139-147`，bytes/cycle） | Credit（`CreditLink.hh:46` + `OutVcState.cc:55-63`） |
| 路由器微架构 | abstract switch（`PerfectSwitch.cc:277-300`） | 5-stage 真实流水线（`Router.cc:71-97`） |
| VC | 无 | `vcs_per_vnet` default 4（`GarnetNetwork.py:44-54`） |
| 死锁避免 | 无显式机制（message 粒度悲观保证） | weight + 维序 + VC + credit + `calculateVC` panic |
| priority 反转 | `PRIORITY_SWITCH_LIMIT=128`（`PerfectSwitch.cc:60`） | ordered vnet 入队时间顺序（`SwitchAllocator.cc:318-335`） |
| 单位 | `MESSAGE_SIZE_MULTIPLIER = 1000`（`PerfectSwitch.cc:60`） | flits 数 = `ceil(MessageSize / bitWidth)`（`NetworkInterface.cc:386-387`） |
| Hetero | ❌ | ✅（`NetworkBridge.cc` CDC/SerDes） |

**关键洞察**: Simple 适合**功能正确性快速验证**（message 粒度，零死锁风险），Garnet 3.0 适合**时序精确建模**（5-stage + flit-level + VC）。**这正是 CppTLM 已有 `simulation_mode: functional|timing` 双模式的 Ruby 对应物**——但 gem5 是命令行切换，CppTLM 是 JSON 字段切换，更适合脚本化配置。

---

## 3. Ruby Topology（来自 gem5.org/ruby/interconnection-network）

**Topology 文件**（Python）:
- `src/mem/ruby/network/topologies/Crossbar.py`
- `src/mem/ruby/network/topologies/CrossbarGarnet.py`
- `src/mem/ruby/network/topologies/Mesh_XY.py`
- `src/mem/ruby/network/topologies/Mesh_westfirst.py`
- `src/mem/ruby/network/topologies/MeshDirCorners_XY.py`
- `src/mem/ruby/network/topologies/Pt2Pt.py`
- `src/mem/ruby/network/Network.py`
- `src/mem/ruby/network/BasicLink.py`
- `src/mem/ruby/network/BasicRouter.py`

**Invocation**:
```
./build/<ISA>/gem5.debug configs/example/ruby_random_test.py \
  --num-cpus=16 --num-dirs=16 \
  --network=garnet2.0 --topology=Mesh_XY --mesh-rows=4
```

### 3.1 Topology 类型

| Topology | 描述 | 命令行 |
|----------|------|--------|
| **Crossbar** | L1/L2/Dir → switch → central switch | `--topology=Crossbar` |
| **CrossbarGarnet** | L1/L2/Dir → garnet router（内部建模 crossbar + allocator） | `--topology=CrossbarGarnet` |
| **Mesh_XY** | routers = cpus 数，每个 router 接 1 L1 + 1 L2 + 1 Dir。Y links weight=2 强制 XY | `--topology=Mesh_XY --mesh-rows=N` |
| **Mesh_westfirst** | west links weight=1，其他 weight=2 | `--topology=Mesh_westfirst` |
| **MeshDirCorners_XY** | 4 corner routers 各接 1 Dir，其他 router 接 L1+L2 | `--topology=MeshDirCorners_XY` |
| **Pt2Pt** | 全互连 point-to-point | `--topology=Pt2Pt` |

### 3.2 Link 参数（per-link 独立可调）

| 参数 | 适用 | 含义 |
|------|------|------|
| `latency` | 通用 | traversal latency |
| `weight` | 通用 | routing table weight（bias routing） |
| `bandwidth_factor` | simple only | link width in bytes |
| `src_outport` | garnet | string output port name（custom routing） |
| `dst_inport` | garnet | string input port name |

**Router 参数**:
- `latency` — **仅 garnet 支持**（multi-cycle router 建模）

### 3.3 Routing

**Default**: 表查找 + shortest path
- Topology.cc 计算 routing table
- Weight 决定优先级
- Garnet: equal weight 随机
- Simple: equal weight 静态

**Custom**: 写 `outportComputeCustom()` in `src/mem/ruby/network/garnet2.0/RoutingUnit.cc`, 启用 `--routing-algorithm=2`

---

## 4. SLICC 完整工作流（基于本地 `src/mem/slicc/` 源码深度调研）

> **配套深度调研**: [`gem5-slicc-chi-survey.md`](gem5-slicc-chi-survey.md) — 299 行，含 file:line + AST 类型 + C++ 后端代码生成函数清单

### 4.1 SLICC 编译器全流水线（基于源码）

SLICC = **Specification Language for Implementing Cache Coherence**，是基于 **Python + PLY** 的编译器，把 `.slicc` / `.sm` 源文件编译为 **C++ controller 代码 + Python SimObject 代码**。

| 阶段 | 位置 | 输入→输出 |
|------|------|----------|
| CLI 入口 | `src/mem/slicc/main.py:59` `main()` | `-C/--code-path` 默认 `generated` |
| 构造解析器 | `src/mem/slicc/main.py:114` | `SLICC(slicc_file, [RubySlicc_interfaces.slicc], protocol_base, ...)` |
| 词法/语法 (PLY) | `src/mem/slicc/parser.py:55` `class SLICC(Grammar)` | LALR；保留字 `parser.py:146-177` |
| AST 生成 | `src/mem/slicc/parser.py:113` `process()` → `decl_list.generate()` | AST 在 `src/mem/slicc/ast/`（~50 节点） |
| 代码输出 | `src/mem/slicc/parser.py:116` `writeCodeFiles()` → `SymbolTable.writeCodeFiles()` | 见 §4.2 |
| 解析顺序 | `parser.py:89-97` | 先 include → `setShared()` → 协议 self |

### 4.2 关键 SLICC 元素 + 生成产物清单

| SLICC 元素 | 语法规则（`parser.py:line`） | 生成函数 | C++ 产物 |
|-----------|------------------------------|----------|----------|
| `protocol "X" opt;` | `p_decl__protocol:320` | `parser.process()` | `<Proto>ProtocolInfo.hh` (`SymbolTable.py:165-212`) |
| `machine(MachineType:Cache, "...")` | `p_decl__machine:350-356` | `MachineAST.generate():63` | 5 文件/machine（见下） |
| `action(Name) { ... }` | `p_decl__action:358` | `printControllerCC:595` | `<Machine>_Controller.cc` 内嵌 |
| `in_port(p, Type, buf, rank=N) { ... }` | `p_decl__in_port:362` | `printControllerHH:340` | `<Machine>_Controller.hh` |
| `out_port(p, Type, buf);` | `p_decl__out_port:366` | `printControllerHH:340` | 同上 |
| `transition(State, Event) { ... }` | `p_decl__trans:370-384` | `printCSwitch:1642` | `<Machine>_Transitions.cc` |
| `structure(Name, interface="Message") { fields }` | `p_decl__struct:396` | `Type.writeCodeFiles():225-235` | `<Type>.{hh,cc}` |
| `enumeration(Name) { A; B; }` | `p_decl__enum:400` | `printEnumHH/CC` | `<Type>.{hh,cc}` |
| `state_declaration(State) { S, AccessPermission:X; }` | `p_decl__state_decl:405` | `printControllerCC:595` | `<Machine>_Controller.cc` |
| `peek(buf, Type) { ... }` | `p_statement__peek:704` | `printControllerCC:595` | 嵌入 controller |
| `enqueue(port, Type, latency) { ... }` | `p_statement__enqueue:680-690` | 同上 | 嵌入 controller |
| `stall_and_wait(a,b)` / `wakeup_port(a,b)` | `:696-702` | 同上 | 嵌入 controller |
| `check_allocate` / `check_next_cycle` / `check_stop_slots` / `check_on_cache_probe` | `:708-722` | 同上 | 嵌入 controller |

**每个 `machine(...)` 必产 5 文件**（`StateMachine.py:257-262` 分派）:
1. `<Proto>_<Machine>_Controller.py` — RubyController SimObject（`StateMachine.py:264`）
2. `<Machine>_Controller.hh` — 类声明（`:340`）
3. `<Machine>_Controller.cc` — 构造/init/regStats/所有 action 函数体（`:595`）
4. `<Machine>_Transitions.cc` — `doTransition()` 大 switch（`:1642`，`HASH_FUN(state,event):1665`）
5. `<Machine>_Wakeup.cc` — `wakeup()` 事件循环（`:1482`，遍历 `port["c_code_in_port"]:1590`）

**构建系统集成**（`src/mem/ruby/protocol/SConscript`）:
- `:47` `from slicc.parser import SLICC`
- `:72` `slicc_emitter` / `:104` `slicc_action`（调 `slicc.process()` + `slicc.writeCodeFiles(output_dir, slicc_includes)`）
- `:123-124` 注册 SLICC Builder（Transform）
- `:142-163` 从 Kconfig 变量 `RUBY_PROTOCOL_*` 收集协议
- `:165-167` `env.SLICC([], sources)` → 输出 `build/.../mem/ruby/protocol/`

### 4.3 SLICC ↔ 手写 C++ 精确边界

**由 SLICC 生成**（输出到 `build/.../mem/ruby/protocol/<Proto>/`）:
- 每个 `machine(...)`：上述 5 文件
- 每个 `state_declaration` / `enumeration`：`<Machine>_State.{hh,cc}` / `_Event.{hh,cc}` / `_RequestType.{hh,cc}`
- 每个 `structure` / `enumeration`（协议内）：`<Type>.{hh,cc}`（CHI 例：`CHIRequestMsg`、`CHIResponseMsg`、`CHIDataMsg`）
- 聚合头：`<Proto>/Types.hh`（`SymbolTable.py:158`）

**手写 C++（不受 SLICC 生成）— SLICC 运行时接口层**:
- `src/mem/ruby/slicc_interface/AbstractController.{hh,cc}` — **所有生成的 `*_Controller` 的基类**
- `src/mem/ruby/slicc_interface/AbstractCacheEntry.{hh,cc}` — cache entry 基类
- `src/mem/ruby/slicc_interface/Message.hh` — **所有生成 message 结构的基类**（生成代码 `: public Message`）
- `src/mem/ruby/slicc_interface/ProtocolInfo.hh` — `<Proto>ProtocolInfo` 基类
- `src/mem/ruby/system/CacheMemory.{hh,cc}`（Python 名 `RubyCache`）、`Sequencer`/`DMASequencer`
- `src/mem/ruby/network/MessageBuffer.{hh,cc}`
- 网络模型 `SimpleNetwork` / `GarnetNetwork`

**CHI 专属手写扩展**（CHI 协议层既用 SLICC 生成消息 + cache 状态机，又用手写 facade）:
- `chi/generic/CHIGenericController.{hh,cc}` — 手写 controller（AbstractController 派生；include SLICC 生成的 msg headers，`CHIGenericController.hh:50-53`）
- `chi/tlm/*` — ARM CHI-over-TLM 手写实现（`controller.hh:41-45` include `arm_chi.h`）

### 4.4 协议切换的具体 diff（深化 + PNN 锚定）

**例子**: MESI_Two_Level → CHI 切换实际工作量

1. **改 `.slicc` 文件**（CHI 协议族 12 个 `.sm` + 1 个 `.slicc`，见 `src/mem/ruby/protocol/chi/`，2026-10-16 critic 校正：原"14 .sm"是误算）
2. **改 NetworkInterface 配置** — CHI 用 flit-based 多 channel 需重新声明 vnet 数量（从 3 → 4：`CHI.py:236-237` `number_of_virtual_networks=4`）
3. **改 config 模板** — `configs/ruby/CHI.py` + `CHI_config.py` 145 行（master/slave 节点类型类）
4. **CHIGenericController 手写** — 兼容 SLICC 生成代码的 facade（CHI 特有）
5. **完全不动**:
   - 4+1 类 protocol-independent 组件（Sequencer / CacheMemory / Replacement / MemoryController + NI 边界）
   - Garnet（或 Simple）网络
   - Topology 文件（CHI 仅用 `Mesh_XY` / `CustomMesh`）

**PNN 锚点**: CppTLM PNN P1 阶段目标 = **用 C++ 模板 `ProtocolBridge<InB, OutB>` 替代 SLICC DSL**——
- 丢 DSL（无 SLICC 编译链、AST、SConscript 集成），换 C++ 直接表达
- 模板参数化对应 SLICC machine 的 `MachineType` 枚举
- 模板虚函数对应 SLICC transition + action
- 桥内藏状态对应 SLICC `state_declaration` + `TBE`

---

## 5. 一致性协议全景（Gem5 20+ 年最重要的协议层更新）

> **配套深度调研**:
> - [`gem5-slicc-chi-survey.md`](gem5-slicc-chi-survey.md) — CHI 协议族（**12 个 `.sm` 校正** + 4 通道 → 4 vnet 映射）（299 行）
> - [`gem5-coherence-protocols-survey.md`](gem5-coherence-protocols-survey.md) — Directory / Token / Hammer / GPU-VIPER / CMP / HTM 全覆盖（499 行）
> - [`gem5-coherence-network-evolution.md`](gem5-coherence-network-evolution.md) — 20+ 年演进史 + 学术/工业对照（2026-10 新建）

### 5.1 Gem5 11 类一致性协议 + 实测 vnet 计数（基于本地源码全量清单）

> **2026-10-16 critic 复审校正**：原文档 vnet 计数表存在多处错误，本表已按 `configs/ruby/*.py` 中 `number_of_virtual_networks` 实测值重写。

`src/mem/ruby/protocol/` 下扁平协议目录（CHI 独立子目录）：

| 协议 | vnet 数 | 实测 `python:line` | .slicc 选项 | 核心 machine | 类别 |
|------|--------:|-------------------|-----------|-------------|------|
| **MI_example** | **5** | `MI_example.py:202` | — | `-msg/-cache/-dir/-dma` | 教学最小 |
| **MESI_Two_Level** | **3** | `MESI_Two_Level.py:252` | `use_secondary_load_linked` | `-msg/-L1cache/-L2cache/-dir/-dma` | Directory, 2-level |
| **MESI_Three_Level** | **3** | （继承自 MESI_2L） | `use_secondary_load_linked, supports_flushes` | 复用 MESI_2L + `-L0cache/-L1cache` | Directory, 3-level |
| **MESI_Three_Level_HTM** | **3** | （继承自 MESI_2L） | `use_secondary_store_conditional` | 复用 MESI_3L + `HTMSequencer*` L0Cache | Directory + HTM |
| **MOESI_CMP_directory** | **3** | `MOESI_CMP_directory.py:291` | — | `-msg/-L1cache/-L2cache/-dir/-dma` | **Directory, MOESI, CMP** |
| **MOESI_CMP_token** | **6** ⚠️不是 4 | `MOESI_CMP_token.py:297` | — | `-msg/-L1cache/-L2cache/-dir/-dma` + PersistentTable | **Token coherence**（学术） |
| **MOESI_hammer** | **6** ⚠️不是 4 | `MOESI_hammer.py:321`（`hammer-dir.sm:41-63` 用 vnet 0-5） | — | `-msg/-cache/-dir/-dma` + `probeFilter` | **AMD Hammer** (probe filter, 可替换) |
| **MOESI_AMD_Base** | **10** | `MOESI_AMD_Base.py:436` | — | `-msg/-CorePair/-L3cache/-dir/-probeFilter/-Region*` | AMD APU 基座 |
| **GPU_VIPER** | **11** ✅ | `GPU_VIPER.py:1158` | — | 复用 AMD_Base + `-TCP/-TCC/-SQC` | **GPU 专用**（VIPERCoalescer + GLC/SLC bypass） |
| **Garnet_standalone** | **3**（**第三轮 critic 校正**：原"2 推断"是错的，`Garnet_standalone.py:124` 实测 = 3） | `Garnet_standalone.py:124` | — | `-msg/-cache/-dir` | 网络 standalone 测试 |
| **CHI** | **4** ✅ | `CHI.py:237` | `partial_func_reads` | `chi/` 子目录 12 `.sm` + 1 `.slicc` | **ARM CHI** |

> ⚠️ 本地代码库**不存在 `MESI_CMP_directory`**（历史 gem5 ≈2015 前曾存在，已删除；MESI_Directory 的最近等价物是 MESI_Two_Level）
> ⚠️ 原文档 "MOESI=3" 表述仅对 `MOESI_CMP_directory`；其他 MOESI 协议 token/hammer 各有 6 vnet，与 CMP_directory 不能等同。

### 5.2 CHI 协议（Gem5 最现代的协议实现）

> **来源**: [gem5.org/documentation/general_docs/ruby/CHI](https://www.gem5.org/documentation/general_docs/ruby/CHI) + 本地 `src/mem/ruby/protocol/chi/`

**CHI 特性**:
- 灵活实现 ARM AMBA5 CHI 事务
- 支持可配置 cache hierarchy
- MESI 或 MOESI coherency
- **4 通道 → 4 vnet 一一映射**（详见 §5.3）
- **DVM/TLB 一致性**通过独立 `MiscNode` machine（CHI-dvm-misc-node.sm:39）
- **可作为 PNN 后期具体化的标杆**：从 MESI/MOESI 切到 CHI 是 Gem5 已验证的真实工作流

**CHI 协议族文件清单（按机器×方面二维拆分 — 全库唯一）**:
```
chi/Kconfig, chi/SConsopts              — 构建选项
chi/CHI.slicc:1-6                       — 协议入口 ("CHI" partial_func_reads)
chi/CHI-msg.sm                          — CHI 消息结构（3 structure + 3 enum）
chi/CHI-cache.sm        (+transitions/-ports/-funcs/-actions 4 拆分文件)
                                         — RN-F cache 状态机 (MachineType:Cache, CHI-cache.sm:39)
chi/CHI-mem.sm                          — SN 内存节点 (MachineType:Memory, CHI-mem.sm:39)
chi/CHI-dvm-misc-node.sm (+transitions/-ports/-funcs 4 拆分文件)
                                         — MN: DVM 操作分发 (MachineType:MiscNode, :39)
chi/generic/CHIGenericController.{hh,cc} — 手写 facade controller
chi/tlm/*                               — ARM CHI-over-TLM 实现
```

> **2026-10-16 critic 复审校正**：实测 `glob chi/*.sm` = **12 个**（非原文档所写 13/14/15）。完整清单见 `gem5-coherence-protocols-survey.md` §5.1。

### 5.3 CHI 4 通道 → 4 vnet 映射（核心架构特征）

CHI 的 REQ/SNP/RSP/DAT 四通道直接一对一映射到 4 个 Ruby vnet，
在**每种节点中布局完全一致**（CHI-cache.sm:189-197 / CHI-mem.sm:55-63 / CHI-dvm-misc-node.sm:100-108）：

| CHI 通道 | Ruby vnet | 方向 | vnet_type（Garnet 用） |
|----------|-----------|------|------------------------|
| REQ | 0 | To/From 各一 | none |
| SNP | 1 | To/From 各一 | none |
| RSP | 2 | To/From 各一 | none |
| DAT | 3 | To/From 各一 | **response**（唯一标记，影响 credit 容量而非 VC 数；详见 §2.3 校正） |

即：`reqOut/reqIn`/`snpOut/snpIn`/`rspOut/rspIn`/`datOut/datIn`。
**通道级分离保证 CHI 协议层防死锁（SNP 不被 REQ 阻塞）**——配置见 `CHI.py:236-237` `number_of_virtual_networks=4`。

### 5.4 CHI 状态机设计（与 MESI_Two_Level 核心差异）

**CHI cache 状态机**（`CHI-cache.sm:231-285`）:
- **Stable**: `I, SC, UC, SD, UD, UD_T`（UD 带 use-timeout）
- **只被上游持有的 "R" 状态**: `RU, RSC, RSD, RUSC, RUSD`
- **本地+上游组合状态**: `SC_RSC, SD_RSC, SD_RSD, UC_RSC, UC_RU, UD_RU, UD_RSD, UD_RSC`（**8 种组合，CHI-cache.sm:251-258**）
- **DVM 状态**: `DvmTlbi_Unconfirmed / DvmSync_Unsent / ...`（6 种）
- **外部 DVM 状态**: `DvmExtTlbi_Partial / ...`（5 种）
- **通用 transient**: `BUSY_INTR`（snoop 可回应）/ `BUSY_BLKD`（阻塞 snoop）

> **2026-10-16 critic 复审校正**：原文档写"12 种组合"，实测 `CHI-cache.sm:251-258` 是 **8 种**。

**关键设计**：`CHI-cache.sm:274-281` 注释——**CHI 只有一个通用 transient "BUSY" 状态族**，具体行为与最终 stable 状态由 **TBE 中信息动态决定**。

| 维度 | MESI_Two_Level | CHI |
|------|----------------|-----|
| 状态枚举 | `NP,I,S,E,M` + 显式 transient `IS,IM,SM,IS_I,M_I,LLSC_*` | stable = 大量组合态 + 通用 `BUSY_INTR/BUSY_BLKD` |
| transient 风格 | **每 transition 一对显式 transient 状态** | **单一 BUSY + TBE 携带意图**（动态决定） |
| 通道/vnet | 3（`MESI_Two_Level.py:252`） | 4（`CHI.py:237`） |
| 一致性模型 | 目录式 MESI | MOESI + DCT/DMT 转发 + DVM/TLB + retry/credit（`RetryAck`/`PCrdGrant`） |
| Machine 类型 | `L1Cache/L2Cache/Dir/DMA` 多种 | 仅 `Cache` / `Memory` / `MiscNode` 3 种 |

### 5.5 其他一致性协议族 1-句话定位

| 协议族 | 核心特征 | 工程适配 |
|--------|---------|----------|
| **MOESI_CMP_directory** | 精确目录（每行 Sharers+Owner，不可替换）；S/O/M/I + 4 稳定态 + **19 transient**（MOESI_CMP_directory-dir.sm:72-94，**第三轮 critic 校正**：原"14 transient"是误算） | 通用 CMP baseline；forward 给 owner + invalidation 广播 |
| **MOESI_CMP_token** | `max_tokens=1024` (RubySlicc_Util.hh:108-111)；Owner/Sharers 是 soft state；持票守恒 + 写独占（assert 落码 token-L1cache.sm:302-362）；持久化 vnet=3 防饿死 | 学术（CMP + persistent request）；GPU 规模下广播风暴不可接受——gem5 实际没用它做 GPU |
| **MOESI_hammer** | **probe filter**（可替换的 CacheMemory！）；目录可驱逐 + `SilentAcks` 支持 silent eviction；`PfEntry.Owner/PfEntry.Sharers` | AMD Opteron/HyperTransport；元数据缺失时反向 probe 全部 cache（hammer-dir.sm:424-430） |
| **MOESI_AMD_Base** | 加 `probeFilter`/`Region*`/`CorePair`；APU 基座 | 现代 AMD CPU+GPU |
| **GPU_VIPER** | **唯一带 VIPERCoalescer 的协议**；TCP 稳定态仅 `I/V/A/F`（无 M/O/S/E！kernel 边界 flush/inv）；TBE 含 GLC/SLC bypass 位直接对应 GCN/RDMA ISA（TCP.sm:108-109）；vnet=11（全协议库最多） | AMD MI 系列 dGPU；kernel 内 L1 无硬件一致性 |
| **MESI_Three_Level_HTM** | **HTM 内嵌为 L0Cache 状态机能力**（非独立 MachineType）：`HTMSequencer* sequencer` + 事件 `HTM_Start/Commit/Abort`（`MESI_Three_Level_HTM-L0cache.sm:151-158`） | 硬件事务内存研究 |
| **Garnet_standalone** | 仅 L1+Dir，无 DMA（configs/ruby/Garnet_standalone.py:61 assert） | 网络 standalone 流量测试 |

### 5.6 一致性协议族工程经验对 CppTLM 的启示

1. **目录协议的正确性内核** = Owner/Sharers 记账 + recycle 兜底（`MOESI_CMP_directory-dir.sm:947-949`）
2. **Token 思想的价值** = "正确性不变量（票守恒）vs 性能结构（目录）解耦"，但**传输走单播/多播，不要广播 vnet**
3. **GPU_VIPER 的 I/V/A/F + kernel-boundary flush/inv** 是 GPU L1 的正确简化（与 CppTLM `LsuGlobal/LsuLDS` GPU memory 语义对齐成本最低）
4. **CHI 4 通道 → 4 vnet 一一映射** 与 CppTLM e1 v1.1 `vc_counts_[4]` 按 flit_category 分桶设计**数量对齐**（**非语义同构**：vnet 计数 4 vs 4 一致；**但 `noc_bundles_tlm.hh` 实际只有 3 类 CATEGORY_***，第 4 bucket 的 DAT/SNP 语义与 CHI DAT 通道不可直接对应，2026-10-16 critic 校正）—— 可作 vnet 数量参考，**不可作语义交叉验证基准**
5. **MessageBuffer 装配纪律**（triggerQueue ordered、目录↔内存直连不占 NoC、节点创建顺序=NetDest 编号）是 Ruby 防死锁与确定性的工程地基

---

## 6. CppTLM 现有资产与 Ruby 1-1 映射（含 coherence 协议族 + 9 大 CppTLM 缺口）

### 6.1 基础设施层映射（已深化）

| Gem5 Ruby | CppTLM 现有 | 路径 | 匹配度 |
|---|---|---|---|
| **Sequencer** | `ChStreamModuleBase` | `include/core/chstream_module.hh:8-56` | **60%**（缺 `makeRequest` 完整 API） |
| **CacheMemory** | `InputStreamAdapter<BundleT>` + `OutputStreamAdapter<BundleT>` | `framework/stream_adapter.hh:30-180` | **70%**（valid/ready 类似 MessageBuffer） |
| **MessageBuffer** | `InputStreamAdapter<BundleT>::stream_data_/stream_valid_` | `framework/stream_adapter.hh:32-93` | **70%**（缺 priority/ordered 显式 API；Ruby `triggerQueue ordered=True` 需复刻） |
| **NetworkInterface** | `DualPortStreamAdapter` | `framework/dual_port_stream_adapter.hh:95-226` | **80%**（已实现 PE↔Net 协议转换；缺 flit-level credit 边到边追踪） |
| **Router (5-stage pipeline)** | `RouterTLM` | `include/tlm/router_tlm.hh:129` | **50%**（已有 credit + 简单路由，缺 cycle-accurate 5-stage 详情） |
| **GarnetNetwork** | `ModuleFactory` 装配 | `src/core/module_factory.cc:142-866` | **90%**（Step 1-9 9 步装配流水线） |
| **NetworkLink** | `SimplePort + PortPair` | `core/simple_port.hh:13-58` | **80%**（已有 delay_cycles + bind pair） |
| **CreditLink** | `BidirectionalPortAdapter::pending_credits_` | `framework/bidirectional_port_adapter.hh:172-209` | **85%**（已有 1 周期 credit 延迟） |
| **Topology (Python)** | JSON `connections[]` + port_index 语法 | `src/core/module_factory.cc:420-490` | **85%**（已有 group/regex/delay；缺 weight bias 路由） |
| **SLICC 协议控制器** | 手写 C++（无 DSL） | `tlm/pcie/pcie_link_layer_tlm.cc` 等 | **40%**（缺 DSL 编译链 + state machine 抽象——PNN P1 改 `ProtocolBridge<InB, OutB>`） |
| **Simple vs Garnet (性能/精度)** | `simulation_mode` JSON 字段 | `dgpu_soc_timing_v1.json:4` | **90%**（已落地 functional/timing 双模式） |

### 6.2 一致性协议层映射（PNN P2 候选）

| Gem5 一致性协议 | CppTLM 现有 | 路径 | 匹配度 |
|---|---|---|---|
| **MOESI_CMP_directory**（vnet=3） | 隐式（CPUTLM CacheTLM + DirectoryTLM） | `include/tlm/cache_tlm.hh`、`memory_tlm.hh` | **30%**（缺 Sharers/Owner 显式位图） |
| **GPU_VIPER**（vnet=11, I/V/A/F + GLC/SLC bypass） | 隐式（DMA descriptor + doorbell） | `include/tlm/gpu/doorbell_mvp.hh` 等 | **25%**（缺 kernel-boundary flush/inv 协议） |
| **CHI 协议**（vnet=4 REQ/SNP/RSP/DAT） | e1 v1.1 `vc_counts_[4]` flit_category 分桶 | `openspec/changes/archive/2026-10-09-cpptlm-dgpu-soc-internal-noc-e1/design.md`（**第三轮校正**：e1 已归档到 archive/） | **40%**（数量对齐；**语义不等价**——`noc_bundles_tlm.hh:60-62` 实测只有 3 类 CATEGORY，缺 CATEGORY_DAT，2026-10-16 critic 校正） |
| **MOESI_hammer**（probe filter 可替换） | 无 | — | **0%**（PNN P3 候选） |
| **MOESI_CMP_token**（max_tokens=1024） | 无 | — | **0%**（学术，PNN 不直接采纳；仅借鉴 soft-state 思想） |
| **MESI_Three_Level_HTM**（HTM 内嵌 L0） | 无 | — | **0%**（非 dGPU 范围） |
| **Simple Network**（message 粒度） | 隐式（`simulation_mode=functional`） | `dgpu_soc_minimal_v1.json` | **70%**（与 funmode 同效） |
| **Garnet 3.0**（5-stage + flit + credit） | `RouterTLM` partial（缺 cycle 准确 5-stage） | `include/tlm/router_tlm.hh` | **50%**（PNN P3 深化） |

### 6.3 SLICC 编译链分阶段映射（PNN 借鉴）

| SLICC 元素 | CppTLM 表达层 | 借鉴阶段 |
|-----------|---------------|---------|
| `protocol "X" option;` | JSON `network.protocol: chi\|amoeba-hammer\|gpu-viper`（**字符串枚举**——避免硬编码 enum class） | P1 |
| `machine(MachineType:Cache, "...")` | C++ `class CacheBridge : public ProtocolBridge<InB, OutB>` | P1 |
| `state_declaration(State) { S, AccessPermission:X; }` | C++ `enum class CacheState { I, S, E, M };` 内嵌于 bridge | P1 |
| `enumeration(Event) { GetS, GetM, ... }` | C++ `enum class Event { GetS, GetM, ... };` 内嵌于 bridge | P1 |
| `transition(State, Event) { Actions; }` | C++ `void on_event(State&, Event) override` 虚函数 | P1 |
| `action(Name) { send msg; update state; }` | C++ `void send_msg(...)` + `update_state(...)` | P1 |
| `in_port(p, Type, buf)` / `out_port(p, Type, buf)` | `InputStreamAdapter<InB>` / `OutputStreamAdapter<OutB>` 模板参数化 | P0 既有 |
| `enqueue(port, Type, latency)` | `OutputStreamAdapter::push(data, latency_cycles)` | P0 既有 |
| `peek(buf, Type)` | `InputStreamAdapter::peek()` | P0 既有 |
| `check_allocate` / `check_next_cycle` | `Bridge::check_resource()` 显式虚函数 | P2 |
| `protocol "..." partial_func_reads;` (option) | C++20 `if constexpr (BridgeOptions::PartialFuncReads)` | P2 |
| 编译器产物 5 文件 | **无**——CppTLM 不引入 DSL，保留 C++ 直接（per §8.3） | — |

### 6.4 CppTLM vs Gem5 Ruby 能力对比（一表说清）

| 维度 | Gem5 Ruby | CppTLM 现状 | CppTLM PNN 后 |
|------|-----------|-------------|----------------|
| 协议无关组件 | Sequencer/CacheMemory/Repl/MemCtrl | L3 ChStream + L4 StreamAdapter | + `ProtocolBridge<>` 抽象 |
| 协议切换 | 改 SLICC + 重编译（需 SCons） | 改模板参数 + 重编译 | + JSON 字段切换（语义层） |
| 拓扑参数化 | Python + 命令行 | JSON + port_index | + `network.topology` 字段 |
| 性能/精度切换 | `--network=garnet\|simple` | `simulation_mode: functional\|timing` | + `network.mode: abstract` |
| 协议字段表达 | SLICC DSL `structure/enum` | 手写 C++（pcie_link_layer_tlm.cc） | + `ProtocolBridge::pack/unpack` |
| **vnet 通道数（CHI 4 通道）** | `--num-vnets=4` 运行时 | ❌ 当前单一 vnet | **e1 v1.1 `vc_counts_[4]`（数量对齐；语义不等价，见 §6.2 校正）** |
| **GPU L1 简化（I/V/A/F）** | GPU_VIPER TCP/TCC | ❌ kernel-boundary 协议缺失 | P3 借鉴 |
| **目录式协议（MESI/MOESI）** | MOESI_CMP_directory sharers+owner | ❌ Sharers 位图缺 | P3 候选 |
| HTM | L0 内嵌 | ❌ 非范围 | — |
| Token coherence | 学术 soft-state | ❌ 非范围 | — |
| **CHI (Arm AMBA 5)** | ✅ gem5 mainline | ❌ | **PNN P2 候选（已 e1 v1.1 对位）** |
| CXL protocol | work-in-progress | ❌ | PNN P3 候选 |
| DSL 编译链 | SLICC（PLI/slicc-emitter 已废，Python+PLY） | 无 | **保留 C++ 直接（拒 DSL）** |
| HeteroGarnet CDC | `NetworkBridge.cc` | ❌ | P3 候选 |

### 6.5 关键结论：CppTLM 现有 + PNN 借鉴的 9 大缺口

1. **协议控制器抽象**（SLICC 替代）— PNN `ProtocolBridge<InB, OutB>` — P1
2. **CHI 协议**（vnet=4 REQ/SNP/RSP/DAT）— PNN P2 — **e1 v1.1 vc_counts_[4] 数量对齐；语义不等价见 §6.2**
3. **5-stage router 细节**（garnet3.0 flit-level + credit 精确）— PNN P3
4. **Sharers/Owner 显式位图**（目录协议基线）— P3
5. **triggerQueue ordered 显式 API**（消息保序）— P1
6. **weight bias 路由**（无环路由保证）— P2
7. **GLC/SLC bypass 字段**（GPU L1 协议字段）— P3
8. **HeteroGarnet CDC/SerDes**（异构 NoC）— P3
9. **flit-level credit 边到边追踪**（避免 NI 处 buffer）— P2

---

## 7. CppTLM PNN 借鉴 Ruby 工作流的具体 5 个改造点

### 7.1 Ruby: SLICC DSL → CppTLM: C++ 模板 `ProtocolBridge<InB, OutB>`

- Ruby: 改 `.slicc` 文件 + 重编译
- CppTLM: 改 `ProtocolBridge` 派生类 + 重编译
- **取舍**: 丢 DSL 换可读性（**C++ 直接**, 无 slicc 编译链）
- **对应 PNN 阶段**: P1

### 7.2 Ruby: `mandatoryQueue` / `optionalQueue` → CppTLM: 复用 `InputStreamAdapter<Bridge::InB>`

- Ruby: controller 内部 queue 命名
- CppTLM: `InputStreamAdapter<Bridge::InB>`（= `Bridge::InB` 类型参数化）
- **对应 PNN 阶段**: P0 既有

### 7.3 Ruby: `NetworkInterface` 协议转换 → CppTLM: `ProtocolBridge` 通用化

- Ruby: NI 内部把 protocol msg 打包成 flit
- CppTLM: `ProtocolBridge::pack(InB) → OutB` 虚函数 + `unpack(OutB) → InB`
- **对应 PNN 阶段**: P1

### 7.4 Ruby: `Topology.py` + `--topology=` → CppTLM: JSON `network.topology` + `connections[]`

- Ruby: Python 拓扑文件 + 命令行选择
- CppTLM: JSON 字段 + `ModuleFactory` 解析
- **对应 PNN 阶段**: P1 增量

### 7.5 Ruby: `--network=garnet2.0` / `--network=simple` → CppTLM: `simulation_mode` + `network.mode`

- Ruby: simple vs detailed 切换
- CppTLM: `simulation_mode: functional|timing` (既有) + `network.mode: abstract|axi4|chi` (新增)
- **对应 PNN 阶段**: P0 既有 + P1 增量

---

## 8. 风险与失败模式（Ruby 20+ 年经验教训）

### 8.1 Ruby 接口稳定性 vs 演进

**稳定的**（20+ 年未变）:
- `RubyPort::recvTiming` / `recvFunctional` / `recvAtomic` API
- `MessageBuffer::enqueue/dequeue` 基础 API

**5 年内被废弃的**:
- Garnet 1.0 → Garnet 2.0（router pipeline 重新设计）
- 旧 `se.py` / `fs.py` 脚本被 `gem5_library/` 取代（v23+ `fatal()` 拒绝运行）
- 多个旧 config scripts 删除（per `gem5-soc-survey.md` §0 路径勘误）

**对 CppTLM PNN 的启示**:
- `ProtocolBridge<InB, OutB>` 接口必须**保留稳定**（不轻易改模板签名）
- `network.mode` 字符串枚举保留扩展空间（避免硬编码 enum class）
- 不要把 PNN 与 `simulation_mode` 合并——两者正交（功能/时序 vs 协议轴）

### 8.2 协议切换的"痛点"（gem5-dev 邮件列表反馈）

- CHI 接入需要修改 NetworkInterface 内部状态机（不是只改 .slicc）
- CXL 接入 gem5 主线仍在 work-in-progress
- **关键痛点**: 协议字段在不同 protocol 之间语义不一致（如 Inv / InvAck 在 MESI vs CHI 是不同消息）

**对 CppTLM PNN 的启示**:
- `ProtocolBridge::pack/unpack` 必须显式表达**语义映射**（不是字段拷贝）
- 桥内藏状态（OOO rid 表）使**协议切换不可逆**——契约规定**仅在 idle drain 后**切换
- 测试矩阵爆炸风险：N 协议 × 2 模式 → 每协议只保 1 golden 配置 + `[pnn]` 标签隔离

### 8.3 编译期 vs 运行时的权衡

- **Ruby**: SLICC 编译期烧协议（无运行时反射）
- **CppTLM 现有**: C++ 模板编译期绑定（类似 Ruby）
- **PNN 选择**: 沿用编译期绑定，**不引入运行时反射**（避免类型擦除）

---

## 9. 调研结论

### 9.1 PNN 方案基于 Ruby 经验的 3 个核心决策

1. **`NoCFlitBundle` 作为 canonical wire**（类比 RubyRequest / RubyMsg）— 16 字段超集
2. **`ProtocolBridge<InB, OutB>` 模板**（类比 SLICC controller）— 编译期协议绑定
3. **`network.mode` 配置驱动切换**（类比 `--network=garnet2.0` / `--network=simple`）— JSON 注入

### 9.2 CppTLM vs Gem5 Ruby 能力对比

| 维度 | Gem5 Ruby | CppTLM 现状 | CppTLM PNN 后 |
|---|---|---|---|
| Protocol-independent 组件 | Sequencer/CacheMemory/Replacement/MemCtrl | L3 ChStream + L4 StreamAdapter | + ProtocolBridge 抽象 |
| 协议切换 | 改 SLICC 重编译 | 改模板参数重编译 | + JSON 字段切换（语义层） |
| 拓扑参数化 | Python + 命令行 | JSON + port_index | + `network.topology` |
| 性能/精度切换 | simple vs garnet2.0 | functional vs timing | + `network.mode: abstract` |
| 协议字段表达 | protocol controller 内部 | 手写 C++（`pcie_link_layer_tlm.cc`） | + `ProtocolBridge::pack/unpack` |
| CHI 协议支持 | ✅ gem5 主线 | ❌ | P2 候选 |
| CXL 协议支持 | work-in-progress | ❌ | P3 候选 |
| DSL 编译链 | SLICC | 无 | 保留 C++ 直接（拒 DSL） |

### 9.3 PNN 阶段的 Ruby 锚点

| PNN 阶段 | 对标 Ruby 工作 |
|---|---|
| **P0** | simple_generic + Garnet standalone 跑 baseline |
| **P1** | Simple Network（`--network=simple`）评估 |
| **P2** | Garnet 2.0 detailed + MESI_Two_Level → CHI 切换 |
| **P3** | CXL 接入（gem5 仍在 WIP，CppTLM 与之同步） |

---

## 10. 引用方式（代码内）

```cpp
// 引用示例（仅 header doc-comment）
class ProtocolBridge {
    // Reference: docs/research/gem5-ruby-protocol-bridge-survey.md
    // 类比 Gem5 Ruby: SLICC-generated controller + NetworkInterface 协议转换
    // Gem5 命令行类比: --network=garnet2.0 / --network=simple
    virtual void pack(...) = 0;
    virtual void unpack(...) = 0;
};
```

**规则**: 仅在 header doc-comment 引用，**代码内不 include 任何 `docs/research/*.md`**（保持编译防火墙）。

---

## 11. 关联文档

- **架构方案**: [`docs/designs/dgpu-soc/pnn-protocol-neutral-network.md`](../designs/dgpu-soc/pnn-protocol-neutral-network.md)（已存在）
- **通信网络现状**: `docs/designs/dgpu-soc/communication-network.md`（v1.0 已落地）
- **ADR 决策**: [`ADR-DGPU-13-pnn-protocol-neutral-network.md`](../adr/ADR-DGPU-13-pnn-protocol-neutral-network.md)（已存在）
- **相关 ADR**: [`ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md`](../adr/ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md)（已存在，PNN Bridge 实现 seam）
- **现有 ADR 关联**:
  - `ADR-DGPU-06` chip-internal AXI vs board-level PCIe 边界（PNN 兼容）
  - `ADR-DGPU-11` timing-mode SoC scope（PNN 沿用 functional/timing 切换）
  - `ADR-DGPU-12` SoC 内部 TLM 接线原则（PNN 不动既有 JSON 拓扑）

> **2026-10-16 critic 复审校正**：原文档标注"待起草/待定"是过期表述——`docs/designs/dgpu-soc/pnn-protocol-neutral-network.md`、`ADR-DGPU-13` 与 `ADR-DGPU-14` 均已落地。

### 11.1 配套深度调研文档（2026-10 新建）

| 文档 | 行数 | 调研深度 | 关联章节 |
|------|------|---------|---------|
| [`gem5-slicc-chi-survey.md`](gem5-slicc-chi-survey.md) | 299 | SLICC 编译链（PLI+Python+PLY）+ AST 节点 + C++ 后端代码生成函数清单 + **12 个 CHI `.sm` 文件**（**第四轮 critic 校正**：原"13 个"是误算，glob 实测 12） | §1.2, §4.1-4.4, §5.2 |
| [`gem5-garnet-network-survey.md`](gem5-garnet-network-survey.md) | 256 | Garnet 3.0 完整文件清单 + 5-stage pipeline + credit 双向流控 + NI message↔flit 切分重组 + 拓扑抽象 + routing 三模式 | §2.1-2.5 |
| [`gem5-coherence-protocols-survey.md`](gem5-coherence-protocols-survey.md) | 499 | 11 类一致性协议族 + Directory 状态转换表 + Token 持票转移表 + GPU_VIPER 拓扑 + CMP 控制器互连 + HTM 内嵌 L0 | §5.1-5.6 |
| [`gem5-messagebuffer-network-interface.md`](gem5-messagebuffer-network-interface.md) | 新 | MessageBuffer / NetworkInterface / vnet 协议消息流转详解 + ordered/mandatory 语义 + NI ↔ router ↔ controller 三方流控 | §1.3-1.4, §6.1 |
| [`gem5-coherence-network-evolution.md`](gem5-coherence-network-evolution.md) | 新 | 20+ 年演进史（`M5` → `GEMS` → `gem5 Ruby` → `Garnet 1.0/2.0/3.0`）+ 学术/工业对照 + PNN 借鉴矩阵 | §5, §8, §9 |

---

## 12. 跨仓同步

- 文件入仓: `git add docs/research/gem5-ruby-protocol-bridge-survey.md`
- 跟踪状态: ✅ tracked（`docs/research/AGENTS.md` §2.6 顶层 1 文档已跟踪）
- `scripts/test/docs_sync_check.sh --strict` 验证（VIRTUAL_PATHS 不需要）
- 不入 openspec 契约，**仅作 background reference**

---

**调研结束时间**: 2026-10
**调研者**: CppTLM build agent
**下次复审**: PNN P1 实施前（2026-11 估）
**复审触发**: Gem5 CHI 协议更新 / Garnet 3.0 公开 / CppTLM P0 baseline 报告产出
