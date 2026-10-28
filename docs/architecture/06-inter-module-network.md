# CppTLM 模块互联网络 v0.4 — 主文档 (无 v1 占位)

> **文档状态**: 🟡 **v0.4 Revision** (基于 Oracle + Metis + Critic 3 轮评审修订, 2026-10-19)
> **目标存档**: `docs/architecture/06-inter-module-network.md`
> **作者**: CppTLM build agent (architect mode)
> **关联前置文档**:
> - [`docs/architecture/01-hybrid-architecture-v2.1.md` §1.1](01-hybrid-architecture-v2.1.md)（v2.1.10 "通信框架层 vs 具体总线层"分层）
> - [`docs/designs/dgpu-soc/communication-network.md`](../designs/dgpu-soc/communication-network.md) v1.0
> **关联 cohesion 草案**: [`06-bis-coherence-as-network-extension.md`](06-bis-coherence-as-network-extension.md) v0.3(同期修订)
> **关联跨仓文档**: ArchForge `09-coherence-protocol.md` / `microarchitecture/coherence-*.md`(已存在,但仅 markdown 草案,**不重发明**)
> **关联 OpenSpec**: `openspec/changes/2026-10-16-cpptlm-inter-module-network-mvp/`(待起草, KPI 路径 K-D)
> **评审历史**:
> - v1 (2026-10-16) — Oracle REJECT, 8 项 BLOCKER
> - v0.2 (2026-10-17) — 修 v1 BLOCKER, 引入新问题
> - v0.3 (2026-10-18) — 修 v0.2 + Critic BLOCKER,本版本
> - **v0.4 + 2026-10-27 增补** — §3.3 补第三类「跨模块共享的业务 Bundle」· §4.3 / §3.2 漂移订正 · §11.2 / §11.3 时间线转隶 CEP + P3.2 偏离登记(per `ADR-CEP-19`;台账 Entry #74)

---

## ⚠️ 修订说明(reviewer 请读)

v0.3 修 v0.2 的 8 项 BLOCKER + 1 项 KPI 口径修正:
- **B-α**: §2.1 ASCII 图层号 L1-L6 canonical 编号 (v0.4 X-1 修订: 与 01-hybrid §1.1 v2.1.10 严格对齐)
- **B-β**: §11.2 P1 stats `addScalar` 真 API(不是 `add_scalar`)
- **B-γ**: 整文档无 "沿用 v1" 占位, 内嵌全部内容
- **B-δ-KPI**: §11.5 KPI 口径修正("累计 6-7 slots" → "串行激活, 并发占用 ≤1")
- **B-ε**: §11.2 P1 真实消费者路径明确(详见 06-bis v0.3)

与 06-bis v0.3 同步修订(API 名 / struct 命名 / P3.5 选项 C / §3.4 候选 D)。

---

## 0. 摘要

### 0.1 核心立场

CppTLM v1 的模块互联网络目标 = **协议中立基线 + Python 拓扑生成 + SimModule 可装配 + coherence 演进 seam**:

- **协议中立基线 60-70% 已落地**(per `communication-network.md` v1.0 §1): L3 ChStream + L4 StreamAdapter 5 子类(Standalone/MultiPort/DualPort/Bidirectional/Axi4Mapper) + L5 Bundle **18 个 .hh 文件**(frozen headers 0 修改;per `ls include/bundles/*.hh` 实测;**文件数 ≠ Bundle 类型数** —— 类型数因 `*_bundles_tlm.hh` 多类型而更高,per §3.2 归类表),模块业务逻辑**完全不知道**协议特化
- **业务/网络 Bundle 二分类**(用户 A 决策):
  - **业务 Bundle**(如 `CacheReqBundle` / `CacheRespBundle` / `GmmuPteBundle` / `SdmaCmdBundle`) = 模块内部契约,**前期性能建模期就要定**,由各业务模块团队负责,可演进
  - **网络 Bundle**(如 `NoCFlitBundle` 16 字段 / `PcieTlpBundle` frozen) = 模块间契约,**后期协议特化时再定**,是项目级冻结面
- **seam 在 L5 Bundle ↔ L4 StreamAdapter 之间**(Q5 决策 S-A): 不动 L0 Packet,不动 6 标签 0 回归主线
- **互联网络即 SimModule 容器**(用户 A3 + metis B5): `SocInternalNoc` / `GpuInternalFabric` / VramControllerTLM / `ApuSoC` 已存在,**功能上已成立**,不需要新建 NetworkModule 基类
- **Python 一条龙生成**: `cpptlm.topo.TopOrchestrator` 编排 + `CxxCompatibleEmitter` 展开 + `cpptlm.library.{mesh_cluster, ring_cluster, SoC}` 工厂方法 + C++ `ModuleFactory.instantiateAll` 递归装配
- **coherence 演进 seam**(§15 + 06-bis): 与 ArchForge `09-coherence-protocol.md` / `coherence-protocol.md` / `coherence-bridge.md` 跨仓对齐 — **不重发明**,引用既有接口,演进路径明确

### 0.2 设计原则

1. **协议特化是工程,不是魔术**: Bundle 字段命名即字段语义(typed POD),改字段名 = 改模块代码,PNN/05 早期方案假装"运行时无感切换"的诱惑拒绝
2. **编译期绑定,沿用 C++ 模板**: 保持 `BUNDLE_FIELD_WIDTH` 编译期锁 + `is_trivially_copyable` 断言,不引入运行时反射
3. **业务 Bundle 可演进, 网络 Bundle 通过 tlm_generic_payload 承载不同业务的 payload**: 业务 Bundle 由各模块团队负责;网络 Bundle 字段稳定语义通过 payload 业务隔离
4. **互联网络作为 SimModule**: 与 NICTLM/RouterTLM/CrossbarTLM 等现存 module 类型统一视角;多套独立 NoC 通过 Python `SoC` 编排器组合成一个完整系统
5. **bridge 内可藏状态, 切换协议必须 idle drain**(per gem5 MessageBuffer 经验): 桥内可有 OOO rid 表, 但协议切换是离线操作不可逆(这一条 P1 不实施,仅作为演进约束)
6. **不破 frozen 边界**: 不破 frozen headers / 23 ABI / Packet::reset() / Packet 既有 14 字段
7. **仿真精度 2 模式**: functional × abstract + timing × <具体协议>; 但**不暴露 network.mode 4 组合**,只暴露 functional/timing 1 维度(per 用户最新要求)

### 0.3 与现有 10 份前置/关联文档的关系 (v0.4 X-6/X-7/X-8 增 6 行边界标注)

| 前置文档 | 状态 | 关系 |
|----------|------|------|
| `01-hybrid-architecture-v2.1.md` §1.1 (v2.1.10 "通信框架层 vs 总线层") | ✅ v2.1.10 已批准 | §2 严格对齐其 6 层 (L1-L6, per C-1 X-1 修订); §3 详化其 §1.2 Bundle 命名保守现状; §5 详化其 §4.7 simulation_mode |
| `docs/designs/dgpu-soc/communication-network.md` v1.0 | ✅ v1.0 已落地 (dGPU 应用架构, 独立保留 per 2026-10-18 重新规划) | §2 + §6 引用其 6 层 + 8 架构债务; §3-§5 是 §1.1 的落地; §9 引用其 ModuleFactory 9 步装配 |
| [`docs/archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md`](../archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md) v0.1 | ⚠️ 已归档 2026-10-18 (per C-5 X-3) | **命名沿用**: PNN §2.2 `ProtocolBridge<InB, OutB, Ordered>` 沿用到 §4.4 (P3+ 模板); **调研参考**: 5 阶段 P0-P4 时序与 §11 大致对齐; PNN §5 提议 network.mode 被 §5.2 拒绝 |
| [`docs/archive/architecture/05-packet-semantic-network-archived-2026-10.md`](../archive/architecture/05-packet-semantic-network-archived-2026-10.md) v0.1 | ⚠️ 已归档 2026-10-18 (per C-5 X-2) | **立场对立**: 05 提议改 Packet 字段 + L4.5 ProtocolAdapter, 06 0 改 Packet + 不引入 L4.5; **调研价值保留**: Packet 字段分类 (7 个死字段) 被 §2.5 继承, Gem5 MessageBuffer 锚点被 01 §1.1 继承 |
| [`docs/architecture/02-complex-topology-architecture.md`](02-complex-topology-architecture.md) v2.2 (2026-04-23) | ✅ 架构设计 | **范围正交 (X-6)**: 本文讲 TLM 仿真层模块互联, 02 讲 RTL 桥接; 共用 ModuleFactory Step 7 (per §9) |
| [`docs/architecture/09-topology-visualization-pipeline.md`](09-topology-visualization-pipeline.md) v2.0 + [`10-topology-generation-management-system.md`](10-topology-generation-management-system.md) v3.0 + [`12-topology-gap-analysis.md`](12-topology-gap-analysis.md) v1.1 | ✅ 09 草案 / 10 v3.0 已批准 / 12 v1.1 已批准 | **视角不同 (X-7)**: 09/10/12 是 Python `cpptlm.topo` 工具链视角 (如何生成 topology JSON), 06 §7 是 C++ ModuleFactory 契约视角 (怎么消费 JSON); §6.3 引用 10 §4 mesh_cluster / ring_cluster 契约 |
| [`docs/architecture/11-coherence-domain-readiness.md`](11-coherence-domain-readiness.md) v1.0 (2026-04-26) | 草案 | **深度不同 (X-8)**: 11 是 readiness 评估 (做不做), 06-bis §0 阅读引导加 cross-ref 到 11 (per D-3) |
| [`docs/architecture/P0_P1_P2_DECISIONS.md`](P0_P1_P2_DECISIONS.md) | ✅ 已全部确认 (2026-04-07) | **范围正交**: P0_P1_P2 汇总早 Phase 0-2 决策, 06 §11 P0-P6 是后续演进分阶段; §18 修订记录引用其 P0 任务格式 |
| ArchForge `09-coherence-protocol.md` + `microarchitecture/coherence-*.md` | 🟡 接口草案 (markdown, 无 C++ 实现) | **不重发明**: §15 + 06-bis §1 跨仓引用, C++ 实施由 CppTLM 仓 (per 06-bis §2.1 实施切分) |
| [`07-coherence-exploration-platform.md`](07-coherence-exploration-platform.md) v0.1+ | 🟡 已落地 (CEP 顶层真源; 2026-10-21 拍板 "α") | **coherence 演进 seam 的继任真源** —— 06-bis 已被其 supersede(per 06-bis 顶部 Status Update); §15.1 / §15.4 的 coherence 条目**时间线转隶 07 号 + `ADR-CEP-*` 队列**; 双向 cross-ref(07 号 §1.2 引用本文档 §2 / §3 / §5) |

---

## 1. 核心立场与使用场景

### 1.1 用户使用场景(源自 4 轮讨论锁定)

| 场景 | 需求 | 本方案的对应 |
|------|------|------------|
| **A** | 前期对业务模块开发,业务模块用**业务 Bundle**(业务相关)| §3.1 业务 Bundle = 模块内部契约,前期就定;模块 0 改动 |
| **A** | 通信网络**可独立**于业务模块开发 | §6 SimModule 容器化 + §7 Python 拓扑生成 |
| **A** | 互联架构探索(topology 拓扑扫描 / link latency / VC count / routing 算法) | §6 互联网络作为 SimModule + §7.2 `mesh_cluster`/`ring_cluster` Python 工厂 |
| **A** | 一致性探索(MOESI / CHI / GPU_VIPER) | §15 显式演进 seam + 06-bis 详细蓝图 |
| **A3** | 互联网络也是 SimModule;系统可构建多个 SimModule,支持多套独立网络 | §6 互联网络即 SimModule + §7 SoC 多 cluster 组合 |
| **C** | 前期支持非特定协议的 Packet 通信,后期逐步特化协议,加一致性 | §4 seam 位置 + §15 演进 seam |
| **E** | 互联网络 2 模式:functional × abstract(前期)+ timing × <具体协议>(后期)| §5 仿真精度 2 模式 |
| **F** | 只讨论模块间互联网络,模块内模式不在讨论范围内 | §1.2 讨论范围明示 |

### 1.2 讨论范围(per F 决策)

**本章讨论**: 模块与模块**之间**的互联网络(inter-module network)
- 协议层(L5 Bundle)、传输层(L4 StreamAdapter + L3 ChStream)、装配层(ModuleFactory Step 7)

**本章不讨论**:
- 模块**内部**业务逻辑(如 SdmaEngineTLM 的 cmd 处理队列)— 业务 Bundle 在模块内
- 物理层 Verilog / hybrid-cache 加速(per [`docs/architecture/02-complex-topology-architecture.md`](02-complex-topology-architecture.md) RTL 路径)
- 跨进程 ABI(per `cpptlm_emulator.h` 23 ABI 冻结)

### 1.3 不在范围的 5 个边界约束

1. **不引入 NetworkModule 基类**(per metis B5): 互联网络功能上是 SimModule,但**不新建基类**,而是显式标注"NOC 角色 module type"在文档中(per `include/tlm/cluster/AGENTS.md` "未实施原因"段已明确:NoC 是中间件不是端点)
2. **不引入 DSL**(per `cpptlm-dgpu-soc-arch-truth-cleanup §6` 既有先例 + gem5 SLICC 教训)
3. **不引入新 Bundle 类型**: 复用既有 18 个 .hh(per glob 实地,`ls include/bundles/*.hh` = **18**)—— **例外**:`bundles::CohIfBundle`(2026-10-27 per 台账 Entry #84 / `ADR-CEP-19` 决策 1)由 **CEP 队列 #7** 交付,属 §3.3 第三类(**跨模块共享的业务 Bundle**),不在本文件 §3.1 二分类的讨论范围内
4. **不破 Packet 既有 14 字段**(per oracle BLOCKER 1 修订: v1 写 12 是错的)
5. **不重发明 coherence 协议**(per ArchForge 已实现 `CoherenceProtocol` / `CoherenceBridge` 接口,本方案引用既有接口作 §15 演进 seam)

---

## 2. 已有资产:60-70% Gem5 化协议中立基线

### 2.1 6 层结构 (L1-L6, 对齐 `01-hybrid-architecture-v2.1.md` §1.1 v2.1.10 canonical 编号) — **v0.4 X-1 修订**

**v0.4 X-1 修订**: §2.1 ASCII 图从 v0.3 的 L0-L7 (8 层) 重画为 L1-L6 (6 层), 严格对齐 `01-hybrid-architecture-v2.1.md` §1.1 v2.1.10 (L1 `tlm_generic_payload + PacketPool` / L2 `MasterPort/SlavePort/PortPair` / L3 `ChStream 抽象` / L4 `StreamAdapter 适配层` / L5 `Bundle 数据格式` / L6 `具体协议字段`).

**v0.3 H1 bug 修复**: v0.3 §2.1 ASCII 图层号 +1 位移且 L0 = tlm_generic_payload (新造层), L7 = driver/ABI (新造边界), L1 = SimplePort (独立层), 与 01 §1.1 canonical 编号 (L1 = tlm_generic_payload + PacketPool, 无 L7 边界层, SimplePort 不独立) 不一致. v0.4 严格对齐 01 §1.1.

**v0.4 canonical 编号 (L1-L6, per 01-hybrid §1.1 v2.1.10)**:

```
┌── L6 具体协议字段 (后期固化, per 01-hybrid §1.1) ────────────────────┐
│  FC token bucket (PcieFlowControlTokenBucket)                      │
│  ACK/NAK 12-bit seq wrap (pcie_link_layer_tlm.cc 手写)             │
│  posted-write / credit-based / single-outstanding 协议语义          │
├────────────────────────────────────────────────────────────────────┤
│  L5 Bundle 数据格式 (协议特化 typed POD, 18 个 .hh, per §1.3)      │
│   PcieTlpBundle / NoCFlitBundle / CacheReqBundle / CrBundle /      │
│   DoorbellBundle / MsixBundle / ...                                 │
│   ↑─[BUNDLE_FIELD_WIDTH 编译期锁 + is_trivially_copyable 断言]──↑  │
├────────────────────────────────────────────────────────────────────┤
│  L4 StreamAdapter 适配层 (协议中立, 模板只绑 BundleT, 5 子类)      │
│   ├─ StreamAdapter<Module, ReqB, RespB>                            │
│   ├─ MultiPortStreamAdapter<M, Rq, Rp, N>                         │
│   ├─ DualPortStreamAdapter<M, PE_r, PE_p, Net_r, Net_p>  ★         │
│   ├─ BidirectionalPortAdapter<M, B, N>                             │
│   └─ Axi4StreamAdapter (3 通道) + Axi4Mapper (OOO)                 │
├────────────────────────────────────────────────────────────────────┤
│  L3 ChStream 抽象 (Gem5 MessageBuffer 级别)                       │
│   InputStreamAdapter<BundleT> + OutputStreamAdapter<BundleT>      │
│   + ChStreamInitiatorPort / ChStreamTargetPort                       │
├────────────────────────────────────────────────────────────────────┤
│  L2 MasterPort / SlavePort / PortPair (TLM 标准传输)             │
├────────────────────────────────────────────────────────────────────┤
│  L1 tlm_generic_payload + PacketPool (per 01-hybrid §1.1)        │
│   Packet 14 字段 (per §2.5) + PacketPool 引用计数池复用             │
└────────────────────────────────────────────────────────────────────┘
```

**注** (v0.4 X-1):
- `tlm_generic_payload` 是 `Packet::payload` 字段类型 (per `packet.hh:39`), 与 PacketPool 共同构成 L1
- SimplePort **不是**独立层 (per 01 §1.1), 是 L2 MasterPort/SlavePort 的基类 (在 §2.2 同步直调约束中详述)
- DGpuBoard 板级门面 (BAR0/BAR1/BAR2) 在 L6 范围内 (per 01-hybrid §1.1 "板级协议字段" 范畴)
- HOST 进程 + 23 ABI 属 **01 §1.1 范围外**(per 01-hybrid §1.1 v2.1.10 只有 **L1-L6**,**无 L7 层** —— 与本节 §2.1 的 L1-L6 编号体系一致;原表述「L7 边界」为伪引用,per 台账 Entry #82 订正), 本文聚焦 L1-L6 模块互联

**seam 位置**: §4.1 锁定在 **L5 Bundle ↔ L4 StreamAdapter 之间**

**L1/L2 同步直调约束**(`include/core/simple_port.hh:52-58`, `MasterPort`/`SlavePort` 继承 `SimplePort`): CppTLM 跨模块数据传输是同步函数调用, 不是 SystemC TLM event-driven 调度. → 协议特化不能放在 L1 (Packet) 或 L2 (MasterPort/SlavePort).

### 2.2 L2 同步直调的工程约束

per [`include/core/simple_port.hh:52-58`](../../include/core/simple_port.hh):
```cpp
inline bool SimplePort::send(Packet* pkt) {
    if (pair) {
        SimplePort* other = (my_side == 0) ? pair->side_b : pair->side_a;
        return other->recv(pkt);   // ← 同步函数直调
    }
    return false;
}
```

→ **L2 是同步直调**,不是 event-driven。**协议特化不能放在 L2** — 那需要 event-driven 调度(错过 simple_port 设计意图)。**seam 必须往上放**(L4 与 L5 之间,per §4)。

### 2.3 L4 StreamAdapter 5 子类角色边界(per `communication-network.md` v1.0 §4)

| 子类 | 用法 | 双 Bundle 类型? |
|------|------|----------------|
| StreamAdapter<M, ReqB, RespB> | CacheTLM / MemoryTLM / VramControllerTLM / GmmuTLM | ❌ |
| MultiPortStreamAdapter<M, Rq, Rp, N> | CrossbarTLM / CoherentXBarTLM / ArbiterTLM / PcieEndpointTLM / LinkTLM 等 | ❌ |
| DualPortStreamAdapter<M, PE_r, PE_p, Net_r, Net_p> | **NICTLM**(**全仓唯一一处使用**,per `chstream_register.hh:190`)| ✅ |
| BidirectionalPortAdapter<M, B, N> | RouterTLM | ❌(单 Bundle) |
| Axi4StreamAdapter | PcieAxiAdapterTLM | ❌ |

**关键洞察**(per metis / oracle 调研):
- **DualPortStreamAdapter 是 CppTLM 当前唯一"两侧协议不同"的代码实现**(NICTLM PE=CacheReqBundle, Net=NoCFlitBundle)
- `module_factory.cc:731-733` 硬编码 cast 到具体特化(类型上对 NICTLM 是形式 UB,但能跑是因为 dual adapter 内部全是 MasterPort* 裸指针,布局与模板参数无关 — per oracle Q-α 实证)
- 实际"NICTLM 内的 `packetize`/`reassemble` 字段映射代码在 `nic_tlm.cc:97+` 内部,不在 adapter",adapter 只是 passthrough

### 2.4 L3 ChStream 输入/输出(per `stream_adapter.hh:30-180`)

- `InputStreamAdapter<BundleT>::process(Packet* pkt)` — 反序列化 payload → `stream_data_` Bundle + 标 `stream_valid_=true`
- `OutputStreamAdapter<BundleT>::send(MasterPort*, PacketType, uint64_t)` — acquire Packet → serialize → MasterPort::send

**关键洞察**: `Payload` 反序列化 / 序列化发生在 Bundle ↔ tlm_generic_payload 边界,**与协议无关**。

### 2.5 Packet 14 字段清单(per `include/core/packet.hh:36-60` + `packet.hh:243` private ref_count) — **v0.3 BLOCKER B1 修订**

| # | 字段 | 类型 | 行 | 类别 | 实际消费者实证 |
|---|------|------|-----|--------|--------------------|
| 1 | `payload` | `tlm::tlm_generic_payload*` | 39 | **L0 Packet 内字段(业务数据载体)** | 大: `stream_adapter.hh:48,144` / `coherent_xbar_tlm.cc:52` / `master_port.hh:24` / `slave_port.hh:8,18` |
| 2 | `stream_id` | `uint64_t` | 42 | 半死(默认 0) | 写: `stream_adapter.hh:142`;读: `fragment_mapper.hh:71` 当 transaction_id 回退(BUG 温床) |
| 3 | `seq_num` | `uint64_t` | 43 | **死** | 仅 `mock_modules.hh:64` 测试 |
| 4 | `cmd` | `CmdType` enum (3 值) | 44 | **死**(live 0 read) | 写: `coherent_xbar_tlm.cc:54`;读: live 0 处(测试) |
| 5 | `type` | `PacketType` enum (4 值) | 45 | **活跃** | 写: `stream_adapter.hh:141` 多处;读: `stream_adapter.hh:239`(pkt type 路由) |
| 6 | `src_cycle` | `uint64_t` | 48 | **活跃** | `master_port.hh:24` / `slave_port.hh:8` |
| 7 | `dst_cycle` | `uint64_t` | 49 | **活跃** | `master_port.hh:16,24` / `port_manager.hh:43` |
| 8 | `original_req` | `Packet*` | 52 | **活跃** | `master_port.hh:23-24` / `packet_pool.hh:144-147`(引用计数回收) |
| 9 | `dependents` | `vector<Packet*>` | 53 | **死** | `coherent_xbar_tlm.cc:64` 拷贝 0 read |
| 10 | `route_path` | `vector<string>` | 56 | **死** | `coherent_xbar_tlm.cc:65` 拷贝 0 read |
| 11 | `hop_count` | `int` | 57 | **死** | `coherent_xbar_tlm.cc:60` 拷贝 0 read |
| 12 | `priority` | `uint8_t` | 58 | **死** | `coherent_xbar_tlm.cc:61` 拷贝 0 read |
| 13 | `flow_id` | `uint64_t` | 59 | **死** | `coherent_xbar_tlm.cc:62` 拷贝 0 read |
| 14 | `vc_id` | `int` | 60 | **活跃** | `port_manager.hh:37,105` / `bidirectional_port_adapter.hh:225`(credit 接收 → `receive_credit`)/ `:269`(`send_credit_impl`)(VC 仲裁) |

**+ private** `ref_count`(`atomic<uint32>`, line 243) — Pool 引用计数

**死字段累计**: 7 个(`seq_num` / `cmd` / `dependents` / `route_path` / `hop_count` / `priority` / `flow_id`)。

**v0.3 决策**: Packet **不动**(不删死字段、不增新字段、不改 reset())—— §11.2 不实施 packet 改型,§4 拒绝 05-doc 路线。

**`Packet::reset()` 与池复用串包风险**:
- `reset()` private + 仅 PacketPool::acquire 调(`packet.hh:246-264`)— **每包 1 次 reset 兜底池复用**
- 新增字段如不显式重置 → 池复用串包(per metis B2 推断)
- 协议元数据如需 per-packet, 走 tlm_extension 池复用安全路径(per oracle 评估 §3)

**`coherent_xbar_tlm.cc:60-65` 拷贝开销**: 每次 snoop 对每个 peer 拷贝 2 个 `std::vector`(堆分配)。建议未来清理(删 6 行拷贝代码),不破 6 标签 0 回归。

### 2.6 Gem5 4 项资产对应

| Gem5 Ruby 组件 | CppTLM 现状 | 路径 | 匹配度 |
|----------------|-------------|------|-------:|
| Sequencer | `ChStreamModuleBase` | `include/core/chstream_module.hh` | 60% |
| CacheMemory | `InputStreamAdapter<BundleT>` + `OutputStreamAdapter<BundleT>` | `framework/stream_adapter.hh:30-180` | 70% |
| **MessageBuffer** + `m_strict_fifo` ordered | `InputStreamAdapter<>` (**缺 ordered 模板参数**) | 演进 seam §15 | 60%→90% |
| **NetworkInterface (NI)** | **DualPortStreamAdapter**(**全仓唯一一处使用** = NICTLM) | `framework/dual_port_stream_adapter.hh` | 80% |
| GarnetNetwork | ModuleFactory 装配 | `src/core/module_factory.cc` | 90% |
| NetworkLink | SimplePort + PortPair | `core/simple_port.hh` | 80% |
| CreditLink | `BidirectionalPortAdapter::pending_credits_` | `framework/bidirectional_port_adapter.hh` | 85% |
| Topology (Python) | JSON `connections[]` + `cpptlm.library.mesh_cluster()` Python | `cpptlm/topo/orchestrator.py` + `library/interconnect.py` | 90% |
| SLICC 协议控制器 | 手写 C++ | `tlm/pcie/pcie_link_layer_tlm.cc` 等 | 40% |
| **Simple vs Garnet** | `simulation_mode: functional\|timing` + 本方案 2 模式 | `dgpu_soc_timing_v1.json:4` + §5 | 90% |

**已超 gem5 的能力**: CppTLM 的 L2 SimplePort 同步直调 + L4 StreamAdapter 模板绑定,**比 gem5 Ruby 的 MessageBuffer 更轻量**(无 triggerQueue/mandatoryQueue/optionalQueue 三分类,业务流直接走)。

### 2.7 关键事实复盘

1. **L4 StreamAdapter `process_pe_request_input` 仅做 Packet 透传,无字段映射**(per `dual_port_stream_adapter.hh:193-199`): 协议字段映射实际在 NICTLM `packetize`/`reassemble` 内(`nic_tlm.cc:97+`)
2. **`module_factory.cc:707-709` 硬编码 cast 到 NICTLM 具体 Bundle 组合**: 任何"通用化 DualPort"会触发类型擦除 hard cast 改动 + 双注册表边界验证(详 06-bis v0.3 §3)
3. **`register_protocol_bridge` 是 write-only 死钩子**(per `module_factory.hh:313-315` + oracle 实证;**2026-10-27 实测口径,2026-10-28 已实施** —— 现为 **336-343**,见 §9.3 的落地登记与文末行号对照表): `get_bridges()` private,无人读;PNN 文档伪代码模板不存在 — **本方案不重发明,扩展现有 `ModuleFactory::register_protocol_bridge()` 为带 factory lambda**(§9.3 详)
4. **tlm_extension 路径已就绪**(per oracle 评估): 与既有 `TransactionContextExt` / `ErrorContextExt` 同模式,池复用安全,1-2 人日可加 `NetworkProtocolExt`(per oracle §3)

---

## 3. Bundle 二分类

### 3.1 二分类定义

| 维度 | **业务 Bundle** | **网络 Bundle** |
|------|-----------------|-----------------|
| **承载** | 模块内部业务数据(address / size / data / 命令语义等) | 模块间互联协议字段(src/dst node / flit_category / vc_id / snoop bit 等) |
| **定义者** | 业务模块团队(各模块 owner 自己定) | 项目级契约(仿真框架层定义) |
| **何时稳定** | **前期性能建模期就要定**(业务演进驱动) | **后期协议特化时定**(架构探索驱动) |
| **演进粒度** | 字段级 + 类型级(业务模块 owner 自由演进) | 类型级 + frozen-by-fields(BUNDLE_FIELD_WIDTH 编译期锁) |
| **典型例** | `CacheReqBundle` / `CacheRespBundle` / `GmmuPteBundle` / `SdmaCmdBundle` / `DoorbellBundle` / `CrBundle` / `MsixBundle` / 未来 业务模块新增 Bundle | `NoCFlitBundle`(**已存,16 字段网络语义**) / `PcieTlpBundle`(**frozen**) / `Axi4Bundle`(**PCIe 子系统全域, ≥5 消费者**) |
| **tlm_generic_payload 关系** | 业务 Bundle **整体**作为 payload 的 data 序列化(per `bundle_serialization.hh:22-27` `serialize_bundle` memcpy) | 网络 Bundle 也走 payload,但其字段是"网络层",如需持久化业务内容应通过业务 Bundle 字段,而非新增网络 Bundle 字段 |
| **跨 hop 存活** | **否** — per-hop 序列化/反序列化(每个 hop 销毁 + 新建 per `stream_adapter.hh:48-49` deserialize + `:144` serialize) | **否** — 同样 per-hop,但其字段是"网络层上下文" |

### 3.2 实地归类表(per `include/bundles/` **18** 个 .hh + oracle 实证) — **v0.3 B-2 修订**(2026-10-27 per `ADR-CEP-19` D-7 漂移订正:原写「16 个 .hh」,实测 `ls include/bundles/*.hh` = **17**;**2026-10-27 per 台账 Entry #82** 补登 `bundle_serialization.hh` 行 + 正文计数同步 16 → 17;**2026-10-27 per 台账 Entry #84** 增补 `cep_coherence_bundles_tlm.hh` 行 + 正文计数同步 17 → **18**)

**v0.2 B-2 bug**: CrBundle 字段含义错(写 "Cache Request" + 引用 `arch/arm/mpam.hh:51` 外部路径,仓内不存在) + Axi4Bundle 消费者 1→≥5

**v0.3 实地清点**(per `include/bundles/` 18 个 .hh + grep ):

| Bundle | 类别 | owner / 注册路径 | 关键备注 |
|--------|------|-------------------|-----------|
| `noc_bundles_tlm.hh::NoCFlitBundle` (16 字段) | **网络** | 项目框架层 | `transaction_id`/`src_node`/`dst_node`/`address`/`data`/`size`/`vc_id`/`flit_type`/`flit_index`/`flit_count`/`hops`/`flit_category`/`src_port`/`is_write`/`is_ok`/`error_code` — 网络层 wire-format |
| `pcie_bundles_tlm.hh::PcieTlpBundle` (frozen) | **网络** | 项目框架层 | PCIe TLP 板级协议 |
| `pcie_tlp_wire_bundle.hh::PcieTlpWireBundle` | **网络** | 项目框架层 | TLP wire-level, 与 PcieTlpBundle 区分 |
| `pcie_dllp_bundles_tlm.hh::*` | **网络** | 项目框架层 | DLLP wire-format |
| `axi4_bundles_tlm.hh::Axi4Bundle` + `Axi4LiteBundle` | **网络** | 项目框架层, **≥5 消费者**(oracle + grep 实证):`pcie_root_complex_tlm.cc:110,134` + `pcie_mock_ip.cc:196,221` + `pcie_endpoint_ip.cc:407,433,473,499` + `host_bypass_tlm.cc:62,84` + `pcie_axi_adapter_tlm.cc:54,104` + `axi4_stream_adapter.cc` + `axi4_mapper.cc` | AXI4 协议字段(awaddr/awlen/awid/bid/rdata/rid 等) |
| `msix_bundles_tlm.hh::*` | **网络** | 项目框架层 | MSI-X vector table wire-format |
| `cr_bundles_tlm.hh::CrBundle` (**v0.3 修订**) | **业务** | **`cpptlm-lwc-bus-hub-mvp` change**(per AGENTS.md LWC 总线 hub)| **8 字段 APB-lite LWC CR-Bus wire-format**(per `cr_bundles_tlm.hh:1-12` 实地),owner 是 LWC 总线 cluster, **不是** CacheTLM(删 v0.1 错写)+ 删 v0.1 引用 `arch/arm/mpam.hh:51`(仓内不存在)|
| `doorbell_bundles_tlm.hh::*` | **业务** | SdmaEngineTLM | doorbell 命令语义 |
| `cache_bundles_tlm.hh::CacheReqBundle` + `CacheRespBundle` | **业务** | CacheTLM | 业务: `addr`/`size`/`data`/`hit` |
| `compute_bundles_tlm.hh::*` | **业务** | SM 内部 | 指令描述符业务 |
| `dma_bundles_tlm.hh::*` | **业务** | DMA 引擎 | DMA descriptor |
| `sm_bundles_tlm.hh::*` | **业务** | SM 内部 | SM 子模块 wire-format |
| `cep_coherence_bundles_tlm.hh::CohIfBundle` | **跨模块共享的业务**(§3.3 第三类) | **项目级**(CEP 队列 #7) | 承载 canonical 10 流中**跨 Stub↔PE 边界的 5 条 RN-side 流**(`request` / `completion` / `snoop` / `snoop_resp` / `writeback`,由 `flow_id` 区分);**字段形态 = 业务语义**(无 `src_node` / `dst_node` / `flit_category` / `vc_id`)但**取网络侧演进纪律**(`BUNDLE_FIELD_WIDTH` 编译期锁 + `is_trivially_copyable` 断言);per `ADR-CEP-19` 决策 1~3 + `cpptlm-cep-cohif-carrier` Phase 1 |
| `dgpu_fabric_bundles_tlm.hh::*` | **业务/网络混合**(待切型) | GPU 内部 + 板级接口 | 交给 [`cpptlm-dgpu-soc-physical-ip-port-boundary`](../../openspec/changes/cpptlm-dgpu-soc-physical-ip-port-boundary/) 处理 |
| `dgpu_bundles_tlm.hh::*` | **业务/网络** | dGPU 内部 | 同上, 待切型 |
| `cache_bundles_rtl.hh::*` | **业务** | RTL cache 模块 | RT 桥接用 |
| `cpphdl_types.hh` | (基础设施) | 项目框架层 | `bundle_base` + `ch_uint`/`ch_bool` POD, **不归业务/网络** |
| `bundle_serialization.hh` | (基础设施) | 项目框架层 | `serialize_bundle` / `deserialize_bundle` 的 memcpy 序列化机制(per `bundle_serialization.hh:22-27`);**不归业务/网络** —— 它是 §3.1「tlm_generic_payload 关系」维度的**机制载体**,不是 Bundle 类型 |

**判定规则**:
- 字段是否网络层?(src_node/dst_node/flit_category/vc_id/流控 marker) → 网络 Bundle
- 项目级 owner vs 业务模块 owner → 网络 vs 业务

### 3.3 二分类工程意义

| 含义 | 业务 Bundle | 网络 Bundle |
|------|-----------|-----------|
| 模块 owner 可改? | ✅ 加字段 + 改字段宽 + 改序列化都允许(业务自由) | ❌ 编译期 `BUNDLE_FIELD_WIDTH` 锁住,扩展需走 OpenSpec change |
| 跨模块 owner 文档? | 仅自己模块团队需要 | **需要**(跨模块协议契约)— 必须有架构师守门 |
| L5 测试覆盖? | 模块级 TestCase 即可 | 项目级 `[pcie]` / `[realip]` / `[dgpu_soc_timing]` 6 标签 |

> **⚠️ 2026-10-27 修订(per `ADR-CEP-19` 决策 3;台账 Entry #74)** —— 本表原有**两列**,但 §3.1 的两条判定规则对「**跨模块共享、但字段非网络层**」的 Bundle 给出**相反**答案:
> - **字段维度**(§3.1 判定规则第 1 条:是否含 src_node / dst_node / flit_category / vc_id / 流控 marker)⇒ **业务**;
> - **owner 维度**(第 2 条:项目级 owner vs 业务模块 owner)⇒ **网络**。
>
> ⇒ **补第三类**,把「**分类**」(是什么)与「**演进纪律**」(谁能改)解耦:
>
> | 含义 | **跨模块共享的业务 Bundle**(第三类) |
> |------|------|
> | **字段形态** | **业务语义**(无网络层字段)—— 按 §3.1 第 1 条判定 |
> | **owner** | **项目级**(跨模块共享契约)—— 按 §3.1 第 2 条判定 |
> | **模块 owner 可改?** | ❌ **编译期 `BUNDLE_FIELD_WIDTH` 锁住**,扩展需走 OpenSpec change —— **取网络侧的纪律** |
> | **跨模块 owner 文档?** | **需要** —— 必须有架构师守门(取网络侧) |
> | **L5 测试覆盖?** | **项目级**(取网络侧) |
> | **唯一实例** | **已落地(1 个)** —— `bundles::CohIfBundle`(`include/bundles/cep_coherence_bundles_tlm.hh`;承载 canonical 10 流中**跨 Stub↔PE 边界的 5 条 RN-side 流`)—— per `ADR-CEP-19` 决策 2 / 3;✅ **文件已存在**(2026-10-27 per 台账 Entry #84,`cpptlm-cep-cohif-carrier` 队列 #7 Phase 1 交付)⇒ **第三类不再是「规划实例」,实现层已有 1 个实例**;承载面(`ProtocolEngine` 端口面 / `ProtocolBridge` / JSON 装配)仍待 Phase 2~5 落地 |
>
> **为什么必须解耦**:CEP 的 CohIf 是**第一个**触发该二义的实例(业务语义 + 跨模块共享);若不补第三类,后续每个「跨模块共享但非网络字段」的 Bundle 都要重新吵一遍分类。
>
> **写作纪律**:本表**只增第三类**,§3.1 的两条判定规则与 §3.2 的归类表**零改动**;`ADR-CEP-19` 签发**在先**,本修订**在后**(否则构成「用未签发 ADR 草案为改真源 1 背书」)。
> **⚠️ 例外(2026-10-27 per 台账 Entry #82)**:后续的漂移回填**确实动了 §3.2** —— 补登漏登记的 `bundle_serialization.hh` 行 + 正文计数 16 → 17(实测 `ls include/bundles/*.hh` = 17)。该次回填**不触碰** §3.1 的两条判定规则,亦**不改动**本表第三类的**定义**(只把「唯一实例」改为「规划实例(当前零实例)」以对齐「文件尚不存在」的事实)。**§3.2 归类表的其余行零改动**。
>
> **⚠️ 例外(2026-10-27 per 台账 Entry #84 —— 第三类实例落地)**:§3.2 归类表**增补** `cep_coherence_bundles_tlm.hh::CohIfBundle` 行(第三类的**首个实例**)且正文计数 17 → **18**(实测 `ls include/bundles/*.hh` = 18);§3.3 的「唯一实例」由「规划实例(当前零实例)」改为「**已落地(1 个)**」。**§3.1 的两条判定规则与本表第三类的定义零改动** —— 本次只登记**实例**与**计数**,不重新讨论分类规则。

### 3.4 演进粒度(业务 Bundle 可演进粒度)

业务 Bundle 可演进但**有约束**:

1. **字段级加/减**: 局部,不破其他 consumer(**前提**: 字段有 default initializer,旧 reader 忽略新字段,新 reader 容错旧字段)— 实地验证: `cache_bundles_tlm.hh` 字段已 NSDMI 初始化
2. **类型级切换**: 旧 Bundle 保留(frozen),新 Bundle 类型并存(如 `CacheReqBundleV2`)
3. **字段宽度改**: **不可** — `BUNDLE_FIELD_WIDTH` 编译期锁,改宽度重编译所有 consumer,等同于"新 Bundle"

**注**: 业务 Bundle 没有 NoCFlitBundle 那样的"编译期宽度锁",因为它是局部契约 — owner 自己定。但**实际改宽度的工程量相当于新 Bundle**,所以按 3 处理更稳。

### 3.5 与既有架构债务的关系

`communication-network.md` v1.0 §7 列 8 债务,**06 v0.3 不强求解决任何**(per §3.5 表)— 留给用户 / 既有 change 决议:
- D1 (`dgpu_soc_minimal_v1.json` programmatic 多): §7 Python 拓扑生成可生成,但**用户自撰择**
- D2 (SDMA 5 端口全 PcieTlpBundle): 留给 `cpptlm-dgpu-soc-physical-ip-port-boundary` change
- D3 (Bridge/Mapper 两层定义完整, 主架构文档未引入): **§6 引入**(`SocInternalNoc` / `GpuInternalFabric` / `VramControllerTLM` 是 Bridge/Mapper 的实例)
- D4 (CreditStream 36 行 stub 0 引用): **仍 0 引用** — `BidirectionalPortAdapter::pending_credits_` 承载,不退役
- D5 (`impl_mode.hh` 不存在,文档自标"待实现"): **不变** — 已由 `simulation_mode` JSON + `VramControllerTLM::set_mode()` 替代(v2.1.10 修订)
- D6 (dgpu_realip_refactor Phase 0/1 adapter 复用度未量化): **§6 + §15 演进 seam 引用**, 不量化
- D7 (Phase 7 已实施 [pcie] 测试): 不在范围
- D8 (CPU↔GPU coherence): **§15 演进 seam 完整引用 ArchForge 既有**

---

## 4. 协议中立层(协议层在 L5↔L4 之间)

### 4.1 seam 在 L5 Bundle ↔ L4 StreamAdapter 之间(per Q5 S-A)

**核心论断**:
- Gem5 NetworkInterface 在 RubyPort (controller) ↔ Garnet router 之间 → 类比 CppTLM **L4 StreamAdapter ↔ L5 Bundle 之间**
- CppTLM L2 SimplePort 已同步直调,L0 Packet 已 per-hop,业务/传输解耦天然存在(per oracle 实证)
- 协议特化放在"协议层抽象",**不动 Packet**(Packet 14 字段保持现状, §2.5 已承认死字段但不删)

### 4.2 比 PNN / 05-doc 早期方案的关键差异

| 维度 | PNN v0.1 / 05-doc v0.1 | 本方案 v0.3 |
|------|------------------------|----------|
| **seam 位置** | PNN: L5↔L4 之间 / 05: L4↔L0 Packet 之间 | **L5↔L4 之间**(继承 PNN),但**不动 L0 Packet**(拒绝 05) |
| **Packet 改动** | PNN 0 改 / 05 改 2 字段 + 增 3 + reset 改 | **0 改**(per Q1 + §2.5) |
| **L_PNN/L4.5 新抽象类** | 是 | **不新建** — 复用 `module_factory.cc:707-709` 现有"NICTLM + DualPortStreamAdapter"路径(**唯一的例外**)+ 06-bis v0.3 §3 programmatic bridge(跨域桥) |
| **Python 拓扑生成** | 无(仅 JSON 配置 + ModuleFactory) | **新增**(§7) |
| **Coherence seam** | 无 | **§15 + 06-bis v0.3 显式演进** |

### 4.3 网络 Bundle 不需要新加字段(验证当前 NoCFlitBundle 16 字段足够)

per oracle 调研 + gem5 Garnet vnet 语义:

| 当前 `NoCFlitBundle` 字段 | 用途 | 演进 seam 关注 |
|---------------------------|------|----------------|
| `transaction_id` (ch_uint<64>) | 事务追踪,与 `TransactionContextExt` 同步 | ✅ 必保 |
| `src_node` / `dst_node` (ch_uint<32>) | Mesh 节点坐标 | ✅ 必保 |
| `address` (ch_uint<64>) | 业务地址透传 | ✅ 必保 |
| `data` (ch_uint<64>) | 数据载荷(8 字节 / 拍) | ⚠️ **数据 > 8B 时不能 inline** — 这是 gem5 Garnet flit_size 的根本限制;演进 seam §15.2 |
| `size` (ch_uint<8>) | 字节数 | ⚠️ 8 bit 最多 255B,**不够大块传输**(如 4KB AxiMemBundle) — 演进 seam §15.2 |
| `vc_id` / `flit_type` / `flit_index` / `flit_count` / `hops` | vnet + 流控 + 分片 | ✅ 必保 |
| `flit_category` (REQUEST/RESPONSE/CREDIT) | gem5 vnet 3 类对位 | ✅ 必保(per `include/bundles/noc_bundles_tlm.hh:60-62` 实测 **3** 个 `CATEGORY_*` 常量;原写 4 值含 `CREDIT_RESPONSE` —— **全仓零命中**,已订正;3 值与 CEP 的 `CATEGORY_*` 3 类**精确 1:1**) |
| `src_port` / `is_write` / `is_ok` / `error_code` | 路由 + 协议细节 | ✅ 必保 |

**演进 §15.2 给两种 Bundle 兼容方案**:
- 经典流量(小块): NoCFlitBundle 现行即可
- 大块流量(>256B): **memcpy ≤ 8B inline + >8B descriptor pointer**,或直接走业务 Bundle + 网络 Bundle 双载(业务 Bundle 走 payload 大块,网络 Bundle 走 protocol metadata)

### 4.4 协议层实现机制: `ProtocolBridge`(继承 PNN,本方案名称沿用)

借 PNN v0.1 §2.2 + 本方案精简:

```cpp
// include/framework/protocol_bridge.hh (per PNN §2.2, 本方案 §15 演进 §15.1 才实施)
namespace cpptlm {

// 类型擦除基类 — ChStreamModuleBase 持有
class ProtocolBridgeEr {
public:
    virtual ~ProtocolBridgeEr() = default;
    virtual void pack_with_packet(Packet* pkt) = 0;     // 写 Packet 协议字段(如需)
    virtual void unpack_with_packet(Packet* pkt) = 0;
    uint16_t bridge_id() const { return bridge_id_; }
    void set_bridge_id(uint16_t id) { bridge_id_ = id; }
    uint8_t protocol_id() const { return protocol_id_; }
    void set_protocol_id(uint8_t id) { protocol_id_ = id; }
    struct Stats {
        uint64_t pack_cycles = 0;
        uint64_t unpack_cycles = 0;
        uint64_t conversions = 0;
    };
    Stats stats_;
private:
    uint16_t bridge_id_ = 0;
    uint8_t protocol_id_ = 0;
};

// 模板基类 — 子类重写 pack/unpack
template <typename InB, typename OutB, bool Ordered = false>
class ProtocolBridge : public ProtocolBridgeEr {
public:
    virtual void pack(const InB& in, OutB& out) = 0;
    virtual void unpack(const OutB& in, InB& out) = 0;
};

// Identity adapter — 前期 abstract 模式 baseline
template <typename FlitT>
class AbstractProtocolBridge : public ProtocolBridge<FlitT, FlitT, false> {
    void pack(const FlitT& in, FlitT& out) override { out = in; }
    void unpack(const FlitT& in, FlitT& out) override { out = in; }
};

}  // namespace cpptlm
```

**关键决策**:
- 双层结构 `ProtocolBridgeEr` (type-erased) + `ProtocolBridge<InB, OutB, Ordered>` (template): ChStreamModuleBase 持 `ProtocolBridgeEr*`,子类编译期绑 InB/OutB
- `Ordered=true` 借鉴 gem5 `MessageBuffer::m_strict_fifo(p.ordered)` 单参数(per gem5-messagebuffer-network-interface §1.2)
- 1 个 bool 模板参数,**不引入 mandatory/optional 三分类**(per gem5-messagebuffer 实测)

### 4.4.1 落地登记(2026-10-28 · per 台账 Entry #86 · 队列 #7 Phase 3)

**本节原型已首次落地**,落点 `include/framework/protocol_bridge.hh` + `include/tlm/cep/cohif_noc_bridge.hh`。
**原型正文不改**,此处登记落地形态与 3 处**有意的差异**:

| 项 | 原型 | 落地 | 理由 |
|---|---|---|---|
| **pack / unpack 的层次** | 单层 `pack(InB,OutB)` / `unpack(OutB,InB)` | **两层**:① `pack`/`unpack` = **消息内容投影**(共有字段 4 个双向无损);② `pack_with_packet(Packet*)`/`unpack_with_packet(Packet*)` = **端到端完整语义**(`Packet` payload 数据区承载 `InB` 原始字节) | ⚠️ **架构事实**:`CohIfBundle.flow_id` 域 = **5 值**,`NoCFlitBundle.flit_category` 域 = **3 值**,而 6→3 消息类映射表是**刻意多对一**的 ⇒ **仅靠网络 flit 的字段空间不可能唯一恢复 `flow_id`** ⇒ Bundle 级「逐字段 round-trip 恒等」在数学上不可达。协议语义(`perm`/`snoop_type`/`line_state`/`hit`/`dirty`/`error`/`atomic` + `flow_id`)**必须**有自己的落脚点 ⇒ 落在 Packet payload 数据区(**不**用 `tlm_extension` ⇒ 不触 §4.5 的 P3+ 范围) |
| **per-bridge 元数据** | `protocol_id_` / `bridge_id_` | 同上 + **`metadata_ok()` 要求两者非零** | `0` 需能表达「未分配」这一**可测状态**,否则「忘了分配」与「合法 id 0」不可分辨 |
| **挂载方式** | 「`ChStreamModuleBase` 持 `ProtocolBridgeEr*`」 | `ChStreamModuleBase` 新增**可选**虚函数 `attach_protocol_bridge`(默认返回 false ⇒ **零波及**) | `ProtocolEngineBase<N>` 的 `N` 是**模板参数** ⇒ ModuleFactory 无法 `dynamic_cast` 到具体类型;独立 mixin 与 `SimObject` **无继承关系** ⇒ `dynamic_cast` **编译不过**,而让 mixin 继承 `SimObject` 会引入**菱形继承** ⇒ 更糟 |

**字段三分类**(per `CohIfNoCProtocolBridge`,共 12 个字段):

| 类 | 数量 | 字段 | 层 1 行为 |
|---|---|---|---|
| **共有** | 4 | `transaction_id` / `address` / `data` / `size` | 双向**无损** |
| **降维映射** | 1 | `flow_id` → `flit_category` | **多对一 ⇒ 不可逆**(同类别流在网络上不可区分) |
| **丢弃** | 7 | `perm` / `snoop_type` / `line_state` / `hit` / `dirty` / `error` / `atomic` | **完全不进**网络 flit |

⇒ 层 1 **不携带** = 降维 1 + 丢弃 7 = **8** 个 ⇒ 走层 2 的 payload 通道。**该清单显式暴露**(`dropped_fields()` / `lossily_mapped_fields()`)且由判据「**清单 ↔ 实际行为**」机器核对(逐字段变更后看网络 flit 是否跟着变)。

### 4.5 协议元数据承载(per Q6 + 调研复核)

per oracle + Q6: **per-bridge 注入 + 演进 seam per-packet tlm_extension**:

| 时机 | per-bridge (静态) | per-packet (动态) |
|------|-------------------|-------------------|
| 字段位置 | `ChStreamModuleBase::protocol_id_` + `bridge_id_` (各 1 字段) | `pkt->payload->get_extension<NetworkProtocolExt>()` |
| 配置时机 | ModuleFactory Step 7c (构建期) | Step 2.5 (发送) / Step 5.5 (接收) |
| 用途 | 模块级协议声明(如 NICTLM = "noCFlit") | 包级 protocol flavor |
| 引入 | P1 | P3+ 真实需要时 |

**P1 范围**: **仅 per-bridge**(静态)。per-packet 演进 seam §15.3。

---

## 5. 仿真精度 2 模式(functional / timing)

per 用户 E 决策 + 最新要求"不需要过多组合"。

### 5.1 单一 JSON 维度: `simulation_mode`

`simulation_mode` 是已有 JSON 字段(per `dgpu_soc_timing_v1.json:4` + ADR-DGPU-11 + `include/tlm/vram_controller_tlm.hh::set_mode`)。本方案不引入新轴。

```json
{
  "simulation_mode": "functional" | "timing"
}
```

| 模式 | 含义 | 何时用 |
|------|------|--------|
| `functional` | 业务/网络逻辑正确性验证,延迟=0 / 协议字段映射存在 | **前期性能建模期** — 用户验证算法级逻辑 |
| `timing` | 周期精确仿真,Gem5 Simple vs Garnet 类似精度 | **后期协议特化时** — 用户验证时序 |

### 5.2 不引入 network.mode 字段(per gem5 SLICC 经验)

**为什么砍 network.mode**:
- Gem5 SLICC 协议控制器**编译期烧死**协议(per gem5-ruby-protocol-bridge-survey §1.3 + 早期调研的事实更正)—— 用户提到"Gem5 命令包也有 stream_id"经 oracle + librarian 实证为**部分不正确**(`request.hh:432-444` 注释: `_streamId` 是 SMMU/IOMMU 专用,**不是**通用业务身份**; gem5 Ruby 实际无 stream_id 通用字段)
- "运行时协议切换"对 CppTLM 比 gem5 更激进(per §12.5 详)
- 早期 PNN 提议的 `network.mode: "abstract"|"axi4"|"chi"` 字段(PNN §4.2 / §6.1)会带来 N 协议 × N 模式测试矩阵爆炸
- **实际工程上,协议特化 = 业务模块代码改动**(per §0.2 原则 1),不是 JSON 字段切换

**P1 范围**:
- 不引入 `network.mode` JSON 字段
- 不引入 `ProtocolBridgeFactory` 模式名解析
- 不引入 `ProtocolBridge::pack_with_packet` 字段写入入口

### 5.3 simulation_mode 的实际承载(per ADR-DGPU-11 + v2.1.10)

| 模式 | 实际机制 | 路径 |
|------|---------|------|
| `functional` | `VramControllerTLM::set_mode(FunctionalZeroDelay)` + `simulation_mode=functional` JSON 字段 | `include/tlm/vram_controller_tlm.hh:74` |
| `timing` | `VramControllerTLM::set_mode(TimingCycleApproximate)` + `simulation_mode=timing` JSON 字段 | 同上 |

**注**: 网络 Bundle 字段不随 simulation_mode 改变 — 都是 NoCFlitBundle 16 字段。模块自己实现 `advance_cycle()`(时序模式)或 no-op(functional 模式) — per `chstream_module.hh:74` `virtual void advance_cycle() noexcept {}`

**SHOULD-FIX(v0.3 修订)**: §5.3 表述限定 "当前仅 VRAM 路径落地" — RouterTLM / LinkTLM 延迟不受 simulation_mode 影响(per oracle 评审)。

### 5.4 演进 seam:未来增 force_mode?

如未来需要 "force functional"(局部模块强制 functional 而其他是 timing) — 留给 `cpptlm-dgpu-soc-control-hub` Proposed Draft + 顶层 BAR decode 决定,**不引入新 JSON 字段**。

---

## 6. 互联网络作为 SimModule 容器(per A3 + metis B5)

### 6.1 关键洞察(per metis 调研结论)

互联网络功能上是 SimModule,**不需要新建 NetworkModule 基类**。理由:

1. **NICTLM / RouterTLM / CrossbarTLM / LinkTLM 已是 `ChStreamModuleBase` 派生**:
   - `NICTLM`: `chstream_register.hh:189-191` 唯一注册 `DualPortStreamAdapter`
   - `RouterTLM`: `chstream_register.hh:119-120,188` 注册 `BidirectionalPortAdapter<M, BundleT, N=5>`
   - `CrossbarTLM` / `CoherentXBarTLM` / `ArbiterTLM` / `LinkTLM` 等

2. **SimModule 容器已存在**:
   - `GpuInternalFabric`: `include/tlm/cluster/gpu_internal_fabric.hh` — programmatic bridge via `set_vram_target`
   - `SocInternalNoc`: `include/tlm/cluster/soc_internal_noc.hh:88-106` — `set_*_target` + `AcceptFn` callback
   - `VramControllerTLM`: `include/tlm/vram_controller_tlm.hh:58` 继承 `MemoryTLM`(ChStreamModuleBase) + `set_fabric_source`
   - `ApuSoC`: 顶层 SimModule,包含 CPU 侧 + GPU 侧 + Crossbar

3. **架构债务**(per `01-hybrid-architecture-v2.1.md` §1.1 + `port_contract_asserts.hh:42-45`):
   - 容器端口契约弃用 `PORT_IS_INITIATOR/PORT_IS_TARGET` 断言,改 `set_*_target` setter-only 模式
   - 这意味着 **容器端口契约 = setter 注入**,与 `DualPortStreamAdapter`(模板绑类型)不同

### 6.2 "互联网络角色 module type" 标注(最小代价动作,零代码改动)

| Module Type | 角色 | 现有路径 |
|-------------|------|----------|
| `RouterTLM` | Network router(per-hop routing + credit + bidirectional) | `include/tlm/router_tlm.hh` |
| `CrossbarTLM` | Network switch(centralized) | `include/tlm/crossbar_tlm.hh` |
| `LinkTLM` | Network link(one-hop connection) | `include/tlm/link_tlm.hh` |
| `NICTLM` | Network interface(PE↔Network boundary) | `include/tlm/nic_tlm.hh` |
| **`SocInternalNoc`** | Network container(DataNoC only, per e1 v1.1) | `include/tlm/cluster/soc_internal_noc.hh` |
| **`GpuInternalFabric`** | Network container(GPU internal fabric) | `include/tlm/cluster/gpu_internal_fabric.hh` |
| **`VramControllerTLM`** | Memory controller + Network interface | `include/tlm/vram_controller_tlm.hh` |

**零代码改动**, 仅 `docs/architecture/06-inter-module-network.md`(本方案) + `docs/designs/dgpu-soc/architecture.md` 显式标注这些 type 为 NOC 角色。

### 6.3 多套独立网络 + System 组合(per A3 决策)— **v0.3 B-θ 修订示例**

**v0.2 bug**: 示例有 2 处错( `soc.add_module` 在 `add_cluster` 后落到子层 + `n_ports=2` 无效)

**v0.3 三种用法分类**(per oracle Q-ε + critic 评审):

| 用法 | Python 形态 | C++ 形态 | 适用 |
|------|------------|---------|------|
| **A — flat + name_prefix 隔离** | `mesh_cluster(rows, cols, name_prefix="cpu_noc")` 输出 `{prefix}_router_{x}_{y}` flat modules | `ModuleFactory::instantiateAll` 解析 flat modules;多套网络靠 name_prefix 命名空间隔离 | **常规 N+1 个独立 NoC** |
| **B — SimModule 嵌套 JSON** | `SoC.add_cluster(cognitive_cluster)` 子层输出 `simulate_instantiate` 递归 | `SimModule::simulate_instantiate` 递归 `instantiateAll` | **已实施 SimModule 嵌套**(`ApuSoC` 内含 CPU cluster + GPU cluster + xbar) |
| **C — programmatic bridge** | — | `set_*_target` 装配,与 `VramControllerTLM::set_fabric_source` / `GpuInternalFabric::set_vram_target` 模式对位 | **跨域桥**(06-bis v0.3 CoherenceBridge 选此路) |

**v0.3 E2E 范例**(P0 任务):

```python
# examples/dgpu_soc_dual_noc_demo.py (P0 doc-only 阶段起草)
# 关键修复: add_module 在 add_cluster 之前, 让 CrossbarTLM 落到 root 而非 last cluster

from cpptlm.library import SoC, mesh_cluster, ring_cluster
from cpptlm.library.standard import cpu_l1_cluster, memory_cluster

soc = SoC("dgpu_soc_with_dual_noc")

# 1. 顶层 CrossbarTLM(单端口 MultiPortStreamAdapter 路径)
# 注: CrossbarTLM 固定 4 端口(per include/tlm/crossbar_tlm.hh:33 constexpr), 不传 n_ports
soc.add_module("cpu_gpu_xbar", "CrossbarTLM")  # 不传 n_ports, 默认 4

# 2. 多套独立 NoC(用法 A flat)
soc.add_cluster(mesh_cluster(rows=4, cols=4, name_prefix="cpu_noc")).tag("cpu_noc")
soc.add_cluster(ring_cluster(n_nodes=8, name_prefix="gpu_noc")).tag("gpu_noc")

# 3. 跨域连接(用法 A → 顶层 xbar)
soc.connect_group("cpu_noc", "cpu_gpu_xbar.0", latency=2)
soc.connect_group("gpu_noc", "cpu_gpu_xbar.1", latency=2)

# 注: ring_cluster 实际产名 gpu_noc_node_0(per interconnect.py:61, 下划线不是点号)
#     port 命名 module.port_index 由 ModuleFactory parsePortSpec 解析为 module="cpu_gpu_xbar", port=0

soc.save("configs/dgpu_soc_dual_noc.json")
```

**输出 JSON 形状**(已 spec 化):
```json
{
  "name": "dgpu_soc_with_dual_noc",
  "modules": [
    {"name": "cpu_gpu_xbar", "type": "CrossbarTLM", "params": {}},
    {"name": "cpu_noc_router_0_0", "type": "RouterTLM", "params": {...}},
    ... (16 router) ...
    {"name": "gpu_noc_node_0", "type": "RouterTLM", ...},
    ... (8 router) ...
  ],
  "connections": [
    {"src": "cpu_noc_router_0_0.1", "dst": "cpu_noc_router_0_1.3", "latency": 1},
    ... (mesh 内部) ...
    {"src": "group:cpu_noc", "dst": "cpu_gpu_xbar.0", "latency": 2},
    {"src": "group:gpu_noc", "dst": "cpu_gpu_xbar.1", "latency": 2}
  ],
  "groups": {
    "cpu_noc": [...16 router names...],
    "gpu_noc": [...8 router names...]
  }
}
```

### 6.4 NICTLM/RouterTLM 不可替代性

**答:否**——它们已是 SimModule,**不需要新建基类**。证据:
- `c++ std::is_base_of<SimModule, T>` static_assert 在 `registerModule<T>` 时执行(per `module_factory.hh:141`) — NICTLM/RouterTLM 是 ChStreamModuleBase **不是** SimModule,**但功能上是 NetworkModule**(没新基类的需求)
- 若强制改基类,需:重继承链路 + 6 标签回归测试 + 测试 spiral 全套重跑 — 巨大代价零收益

### 6.5 与既有架构债务的关系

- **D3**(Bridge/Mapper 两层定义完整, 主架构文档未引入)→ **§6.2 + §6.3 引入**(`SocInternalNoc` / `GpuInternalFabric` / `VramControllerTLM` 是 Bridge/Mapper 的实例)
- **D1**(`dgpu_soc_minimal_v1.json` 内部 connections 少, 7 处 programmatic 桥接)→ §7 Python 拓扑生成可生成完整 connections,但不强制,留给用户撰择

---

## 7. Python 拓扑生成与 C++ 装配契约

### 7.1 现状链(已落地)

```python
# cpptlm/AGENTS.md 已定义
from cpptlm.library import SoC, cpu_l1_cluster, mesh_cluster
from cpptlm.topo.emitter import CxxCompatibleEmitter  # 内部被 SoC.save() 调
```

- **Python 端**: `cpptlm.topo.TopOrchestrator` 编排 layer(链式 API)
- **layer 递归**: `TopoLayer.add_sublayer()`(per `layer.py:195-198`)
- **预置工厂**: `cpptlm.library.{mesh_cluster, ring_cluster, SoC}`(per `library/interconnect.py` + `library/soc.py`)
- **JSON emit**: `cpptlm.topo.CxxCompatibleEmitter` → C++ JSON dict
- **C++ 装配**: `ModuleFactory.instantiateAll` → `SimModule::simulate_instantiate` 递归(per `module_factory.cc:391-415`)

**字段命名实例**(per `library/interconnect.py:24-55` `mesh_cluster`):
```python
def mesh_cluster(rows: int, cols: int, name_prefix: str = "noc") -> TopoLayer:
    # 模块名: {prefix}_router_{x}_{y}, x∈[0,cols), y∈[0,rows)
    # 连接: 每 router 向右 + 向下 (north=0, east=1, south=2, west=3, local=4)
    # 每个 router 通过 ExtLink controller<->router
```

### 7.2 `mesh_cluster(rows, cols)` 与 C++ `RouterTLM` + `ModuleFactory` 契约

**字段语义映射**:
- `{prefix}_router_{x}_{y}` → C++ 端解析为 module `cpu_noc_router_0_0`(下划线分隔,不是 `_router_{x}_{y}` 双层)
- 连接端口 `north=0, east=1, south=2, west=3, local=4` → C++ JSON `connections[].src = "{prefix}_router_x_y.{0|1|2|3|4}"`, `module_factory.cc:122-127` `parsePortSpec` 解析为 `(module={prefix}_router_x_y, port=1)`
- `RouterTLM::node_x, node_y, mesh_x, mesh_y` 参数(per `library/interconnect.py:45`)

**已有有效契约**(per `library/interconnect.py:50-54`):
```python
if x + 1 < cols:
    right = router_names[y][x + 1]
    l.add_connection(f"{cur}.1", f"{right}.3", latency=1)  # east → west
if y + 1 < rows:
    down = router_names[y + 1][x]
    l.add_connection(f"{cur}.2", f"{down}.0", latency=1)  # south → north
```

→ C++ 端通过 `module_factory.cc:122-127` `parsePortSpec` 把 `{prefix}_router_x_y.1` 解析为 `(module={prefix}_router_x_y, port=1)`(east)

### 7.3 SiblingCluster 接口设计建议(P1 增量, 范围有限)

> **注**: 本节是 06 P1 候选增量的"建议路径",**不是必做**。若 metis/oracle 评审认为过度,可推迟到 P3+。

**目标**: 让 Python `mesh_cluster(rows, cols)` 同时生成**模块 + 跨域 bridge**:

```python
# 设想 API(仅讨论, 不实施)
def mesh_cluster_with_bridge(
    rows: int, cols: int,
    name_prefix: str = "noc",
    bridge_to: TopoLayer | None = None,
) -> TopoLayer:
    """Mesh + 可选 桥接到另一个 cluster."""
    inner = mesh_cluster(rows, cols, name_prefix)
    if bridge_to is not None:
        # 加入一个 NICTLM 桥接 PE (CPU) <-> Net (NoC)
        nic_name = f"{name_prefix}_nic_bridge"
        inner.add_module(nic_name, "NICTLM", xbar_id=0, node_id=0)
    return inner
```

**为什么这一节不强制实施**:
- DualPortStreamAdapter **唯一一处使用 = NICTLM**,通用化会破坏类型擦除硬编码(`module_factory.cc:707-709`)
- `SocInternalNoc` programmatic bridge 模式(`set_*_target`)已存在,足以处理"任意 CPU↔NoC 桥接",无需 adapter 通用化
- **P1 决策**: 维持现状,**不引入新的 NICTLM 配对接口**

### 7.4 E2E 增量采样(per `cpptlm/AGENTS.md` 既有 E2E 示例)

```python
# v0.3.1 修订: add_module 移到 add_cluster 之前(per soc.py:35-58 _last_target 语义)
# 否则 add_module 落到最后一个 add_cluster 子层而非 root
soc = SoC("my_soc")
soc.add_module("xbar", "CrossbarTLM")  # 顶层, add_module 在 add_cluster 之前
soc.add_cluster(cpu_l1_cluster(0).layout_grid(dx=4, dy=1, x_offset=0)).tag("compute")
soc.add_cluster(cpu_l1_cluster(1).layout_grid(dx=4, dy=1, x_offset=200))
soc.connect_group("compute", "xbar.0", latency=5)
soc.save("configs/my_soc.json")
```

**输出 JSON 字段**(per `cpptlm/topo/emitter.py` 设计的契约):
- `name` / `description` / `metadata`
- `modules` (per `ModuleSpec.name/type/params`)
- `connections` (per `ConnectionSpec.src/dst/latency/bandwidth`)
- `groups` (Python tag → C++ `ModuleGroup::define(...)`)
- `hierarchy` (sublayer 递归 — `emitter._collect_hierarchy`)
- `coherence_domains` (per `emitter._serialize_coherence`, **仅 root 层**, 06-bis v0.3 §4.1 修订 P2 增递归)
- `metadata.layout.x/y`

**模块类型对齐到 C++ 端 `REGISTER_MODULE(T)` / `REGISTER_CHSTREAM(T)`**:
- `CPUTLM` → `REGISTER_OBJECT`(`include/chstream_register.hh`)
- `CpuCluster` → `REGISTER_MODULE(CpuCluster)`(`include/modules_cluster.hh`)
- `CrossbarTLM` / `RouterTLM` / `LinkTLM` → `REGISTER_CHSTREAM` 或 `REGISTER_MODULE`
- `NetworkBridge` (新增 Cohrene bridge) → `REGISTER_MODULE(CoherenceBridge)` (06 决策, 不是 ArchForge `coherence-bridge.md` §3 — 该文件 grep `REGISTER_MODULE` 0 匹配)

### 7.5 Python tag → C++ `groups` 字典(现有契约已落地)

per `cpptlm/AGENTS.md`: "标签在 emit 时会展开为顶层 `groups` 字典(C++ `group:` 前缀消费)" + `module_factory.cc:511-521` `isGroupReference` / `RegexMatcher` / `Wildcard`

→ Python 端 `soc.tag("compute")` → C++ 端 `soc.connect_group("compute", "xbar.0", latency=5)` → JSON `"connections": [{"src": "group:compute", "dst": "xbar.0", "latency": 5}]` → C++ `module_factory.cc:511` 解析 `isGroupReference(src_spec)` → `ModuleGroup::resolve()` 展开

**这是 Python→C++ 装配的关键契约,06 §7 锁定不变**。

---

## 8. JSON 配置 Schema 演进

### 8.1 顶层 JSON 字段(保留 / 不动)

| 字段 | 来源 | 本方案处置 |
|------|------|-----------|
| `name` / `description` / `metadata` | 既有 | 保留 |
| `modules` / `connections` | 既有 | 保留 |
| `groups` (Python tag 展开) | 既有 | 保留 |
| `extends` / `include` | 既有 | 保留 |
| `hierarchy` / `sublayers` | TopoLayer 递归 | 保留 — **不引入"network.sublayers" 字段**(沿用顶层 sublayers)|
| `coherence_domains` (06-bis 演进) | 06-bis 引入 | **保留**(4 个既有 config 依赖,**不得删** —— `apu_soc_full` / `apu_soc_phase7a` / `apu_soc_phase7b` / `hierarchy_tree_3level`);**解析已落地**(`src/core/module_factory.cc:195-196` hierarchy 分支)但**消费延期到 Stage 2**(per `ADR-CEP-13`;告警文案已指向 `ADR-CEP-13`,per `ADR-CEP-07A` 产物 4)—— **不在 P1 范围** |
| `simulation_mode` | 既有 | 保留(per §5) |
| `plugin` / `version` | 既有 | 保留 |

### 8.2 不引入的新字段(明确的边界)

| 字段 | 为什么不引入 | 引用的早期方案 |
|------|---------------|----------------|
| `network.mode` ("abstract"/"axi4"/"chi") | runtime 协议切换比 gem5 更激进(§12.5),不需 JSON 字段 | PNN §4.2 + 05-doc §5.2 |
| `network.topology` ("mesh"/"crossbar"/"ring"/"butterfly") | C++ 端靠 module 实例化+ port_index 表达拓扑;Python 端靠 `mesh_cluster()` 工厂生成 | PNN §4.2 |
| `network.<protocol>` (axi4/chi/pcie_tlp/cxl 子配置块) | 协议特化是工程(改模块代码),不是 JSON 切换 | PNN §4.2 / 05-doc §5.2 |
| `network.<transport>` 字段 (link_latency_cycles/link_bw_bytes_per_cycle/vc_count/routing) | per-link/per-router 参数若需表达,在 `connections[].attrs` 或 `modules[].params` | PNN §4.2 |

### 8.3 modules[].params 字段意义澄清

| 字段 | 含义 | 与本方案的关系 |
|------|------|----------------|
| `modules[].params` | 模块参数 dict(配置时 set_config 接收) | **保留**(per `cpptlm/AGENTS.md` LINT005 警告) |
| `modules[].config` | 外部配置文件路径(string), 仅 `CpuCluster` 等 SimModule 加载独立 JSON 用 | **保留**(per LINT005 + `module_factory.cc:363-388`) |
| `simulation_mode` | 顶层 JSON 字段, **不是** module params | **保留**(per §5) |

### 8.4 Python reference 与 C++ 类型

| Python target | C++ type | Lint 验证 |
|---------------|----------|----------|
| `"CPUTLM"` / `"CacheTLM"` / `"MemoryTLM"` / `"VramControllerTLM"` / `"NICTLM"` / `"RouterTLM"` / `"CrossbarTLM"` / `"LinkTLM"` / `"ArbiterTLM"` / `"PcieEndpointTLM"` | `chstream_register.hh` 注册的 StreamAdapter 子类 | ✅ 6 标签 |
| `"CpuCluster"` / `"ComputeCluster"` / `"TpcCluster"` / `"GpcCluster"` / `"GpuCluster"` / `"CacheCluster"` / `"MemoryCluster"` / `"GpuNoC"` / `"ApuSoC"` | `modules_cluster.hh:41-105` 注册的 SimModule 派生类 | ✅ |
| `"SocInternalNoc"` / `"GpuInternalFabric"` / `"VramControllerTLM"`(带 programmatic bridge)| SimModule + `set_*_target` | ✅ |
| **`"CoherenceBridge"`**(06-bis 演进 §15)| `REGISTER_MODULE(CoherenceBridge)` (06 决策, 不是 ArchForge `coherence-bridge.md` §3 — 该文件 grep `REGISTER_MODULE` 0 匹配) | 06-bis 候选 |

---

## 9. ModuleFactory 装配链增量

### 9.1 现有 Step 1-9(保留, 不动)

per `src/core/module_factory.cc`(**154-882**,文件共 898 行):

> ⚠️ **2026-10-27 行号刷新(per 台账 Entry #82)**:原表行号整体**漂移 ≈25 行** —— 成因是 CEP 的装配期闸门(`0.25` 段)插入到 Step 1 之前,以及其后多次改动。本表改用**步骤起始注释行**作锚点(比区间更抗漂移)。**Step 1-9 本身未被改动**。

| 步骤(本文件编号) | 代码锚点(起始注释行) | 内容 |
|------|--------|------|
| 预置 | 154 | 解析变量引用 `${path}` |
| 预置 | 160 | JSON Schema 验证(CFG-08) |
| **预置 0.25** | **167-186** | **CEP 装配期判据**(队列 #4 引入)—— `run_channel_gate` + `run_ordering_gate`,FAIL 即中止实例化。**非 Step 7c**:只跑判据,不注入 protocol bridge、不引入 `ProtocolBridgeFactory`(per §9.2) |
| 预置 | 189 | Hierarchy 树解析(TGMS v4.0 Phase 4.1) |
| 预置 | 239 | 解析 port_specs(Phase 3.2: T3.2-04) |
| **Step 1** | 272-280 | PluginLoader dlopen(插件加载) |
| **Step 2** | 285 | 从 REGISTRY 创建实例 |
| **Step 3** | 372 | 解析 groups |
| **Step 4** | 385 | `sim_mod->instantiate(cfg)` |
| **Step 4.5** | 416 | `simulate_instantiate(mod)` 递归 |
| **Step 5** | 442(调用点 493) | `resolveConnections` |
| **Step 6** | 517 | 建 PortPair |
| **Step 7a** | 642 | `ChStreamAdapterFactory::create(type, obj)` 创建 + mirror |
| **Step 7b** | 668-751 | ChStream 连接(per-port 反向;dual 分支硬编码 cast @ 731-733) |
| **Step 8** | 762(调用点 774) | `StatsManager::register_group` |
| **Step 9** | 874(调用点 882) | `incorporate_parent(nullptr)` 触发 SimModule 子树 wiring |

### 9.2 06 P1 不引入 Step 7c(per §4.5 + §10 supersede 早期方案)

PNN v0.1 §2.6 提议"Step 7b 注入协议桥":**不实施** — 理由:
- `network.mode` JSON 字段不引入(per §8.2)
- 协议桥通过 `ProtocolBridgeFactory::create(mode)` 模式名解析的需求为 0
- 模块级 `protocol_id()` / `bridge_id()` 静态字段在 P3+ 真实需求出现时再加

### 9.3 复用既有 `register_protocol_bridge` 钩子(per oracle 调研确认 dead 钩子)

per `include/core/module_factory.hh:313-315`(**2026-10-27 实测口径**;2026-10-28 已实施 ⇒ 现为 **336-343**,见文末行号对照表):
```cpp
static void register_protocol_bridge(const std::string& bridge_name) {
    get_bridges().insert(bridge_name);
}
```
- 当前是 **write-only 死钩子**(`get_bridges()` private, 无人读, per oracle 实证)
- `validate_domain_boundary`(`module_factory_validate.cc:502-534`) 用另一套 `DomainRegistry` 不消费 `get_bridges()`

**P1 处置(如果真需要)**:
- 当 P3+ 引入 `ProtocolBridgeFactory` 时,**扩展** `register_protocol_bridge` 为 `(bridge_name, factory_lambda)` lambda 形式 — 沿用现有 register 入口,**不重发明**
- ~~当前不实施~~ ⇒ **2026-10-28 已实施**(per 台账 Entry #86,队列 #7 `cpptlm-cep-cohif-carrier` Phase 3):
  - `register_protocol_bridge(bridge_name, factory_lambda)` —— 注册表存 **name → factory**,
    且 `get_bridges()` **转 public 可读** ⇒ 死钩子消除;
  - 消费路径:`ModuleFactory::instantiateAll` 的 **Step 7.5**(per §4.5「per-bridge 配置时机 =
    ModuleFactory Step 7c(构建期)」的同一时机)读 `config["cep"]["bridges"]` → 建桥 →
    校验 per-bridge 元数据 → 注入目标模块;
  - **不**引入独立 factory 抽象类 ⇒ §11.2 P3.2 的偏离**依旧成立**(钩子签名扩展不产生抽象层);
  - 「注册了但本次装配未消费」以**死桥清单**(`unconsumed_bridges()`)暴露,**不**让装配失败 ——
    多拓扑共用进程级注册表时某个拓扑不引用全部桥是正常的 ⇒ 强制失败会误伤。

### 9.4 ModuleFactory 现有 ParamRules 的延伸

per `module_factory.hh:87-101` + Change D `getParamRulesRegistry()`:
- 已支持 per-type ParamRules provider(早期 router_tlm 硬编码特例泛化)
- RouterTLM / VramControllerTLM 自注册软默认
- **06 P1 不动** — 现有机制足够

### 9.5 跨 SimModule peer cache late-binding wiring

per `01-hybrid-architecture-v2.1.md` §1.5 + `apu_soc.cc:84-103`:
- Step 9 `incorporate_parent(nullptr)` 触发 `ApuSoC` 等顶层 SimModule 的 late-binding wiring
- 这是 **已有的跨域同步点**,**06 不重复实施**

---

## 10. supersede 关系

### 10.1 矩阵 — **v0.4 X-13 修订 (按归档动作重排)**

| 前置文档 | 类型/版本 | 状态 (本方案后) | supersede 关系 | 处置 |
|----------|--------|----------------|----------------|------|
| `01-hybrid-architecture-v2.1.md` §1.1 | ✅ v2.1.10 已批准 | 保留(不废) | §2 严格对齐其 6 层 (L1-L6, per C-1 X-1) + §3 详化 §1.2 + §5 详化 §4.7 | 互为补充 + 双向 cross-ref (per D-1/D-2 X-11) |
| `docs/designs/dgpu-soc/communication-network.md` v1.0 | ✅ v1.0 已落地 (dGPU 应用架构, 独立保留 per 2026-10-18 重新规划) | 保留(不废) | §2 + §6 引用其 6 层 + 8 架构债务 | 互为补充 |
| [`docs/archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md`](../archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md) v0.1 | 🟡 v0.1 草稿 → **⚠️ 已归档 2026-10-18** (per A-1) | 保留调研参考 | v0.4 沿用其 §2.2 `ProtocolBridge<InB, OutB, Ordered>` 命名 (06 §4.4 P3+ 模板) + 拒绝 §5 network.mode (06 §5.2) | **归档至 `docs/archive/`** + 顶部加归档说明段 — **已落地 2026-10-18** |
| [`docs/archive/architecture/05-packet-semantic-network-archived-2026-10.md`](../archive/architecture/05-packet-semantic-network-archived-2026-10.md) v0.1 | 🟡 v0.1 草稿 → **⚠️ 已归档 2026-10-18** (per A-1) | 保留调研参考 | v0.4 调研价值 (Packet 字段分类 + Gem5 MessageBuffer 锚点) 被 §2.5 + 01 §1.1 继承 | **归档至 `docs/archive/`** + 顶部加归档说明段 — **已落地 2026-10-18** |
| [`docs/adr/ADR-DGPU-13-pnn-protocol-neutral-network.md`](../adr/ADR-DGPU-13-pnn-protocol-neutral-network.md) | 🟡 ADR **未签发** (v0.1 起草中) → **⚠️ superseded by 06 v0.4** (永不签发 per D-5) | 不动正文 (per AGENTS.md "ADR 不可变 + Status Update" 先例) | ADR-DGPU-13 是 PNN 范围定义 ADR, 内容被 06 §15 演进 seam 完整收纳 | **真 ADR 格式, 顶部加 `## Status Update 2026-10-18: superseded by 06 v0.4 (未签发 ADR, 永不签发)` 段** — **已落地 2026-10-18** |
| [`docs/adr/ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md`](../adr/ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md) | 🟡 ADR **未签发** (v0.1 起草中) → **⚠️ superseded by 06 v0.4** (永不签发 per D-5) | 不动正文 (per AGENTS.md "ADR 不可变 + Status Update" 先例) | ADR-DGPU-14 §2.1 seam 位置 vs 06 §4 是同一块领地, 内容被 06 §4.1 + §9.2 收纳 | **真 ADR 格式, 顶部加 Status Update 段** — **已落地 2026-10-18** |
| ArchForge `09-coherence-protocol.md` + `microarchitecture/coherence-*.md` | 🟡 接口草案 (markdown, 无 C++ 实现) | 保留(不废, **0 改**) | §15 + 06-bis §1 跨仓引用, **不重发明** | 仅引用 + 在 CppTLM 实施 (per 06-bis §2.1 实施切分) |
| [`07-coherence-exploration-platform.md`](07-coherence-exploration-platform.md) v0.1+ | 🟡 已落地 (CEP 顶层真源; 2026-10-21 拍板 "α") | **保留(不废)** —— §15 的 coherence 演进 seam **已被其承接** | 06-bis 已被 07 号 supersede(per 06-bis 顶部 Status Update 2026-10-21);§15.1 / §15.4 的 coherence 条目**时间线转隶 07 号 + `ADR-CEP-*` 队列** | **双向 cross-ref** —— 本表 + §0.3 均增行;§15.1 / §15.4 加转隶注记(per 台账 Entry #82,2026-10-27) |

**v0.4 X-13 修订要点** (vs v0.3 B-4):
1. **ADR-DGPU-13/14 是未签发 ADR**, 按 AGENTS.md "ADR 不可变 + Status Update" 惯例 + "未签发永不签发" 标注 (per D-5)
2. **PNN / 05-doc 已物理归档至 `docs/archive/`** (per A-1, 2026-10-18), 不在真相源目录, 顶部加归档说明段 (不修改正文)
3. **ArchForge 状态保持 🟡 草案**, C++ 实施由 CppTLM 仓负责 (per 06-bis §2.1)
4. **新增** 4 行边界标注 (per §0.3 C-2): 02 / 09-12 / 11 / P0_P1_P2 — 与 06 范围正交或视角不同, 互为补充

### 10.2 supersede 处置动作 (P0 阶段, v0.4 X-13 已落地)

**P0 阶段已执行动作** (2026-10-18, per A-1 + D-5):
1. ✅ `docs/designs/dgpu-soc/pnn-protocol-neutral-network.md` → `docs/archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md` (git mv) + 顶部加归档说明段
2. ✅ `docs/architecture/05-packet-semantic-network.md` → `docs/archive/architecture/05-packet-semantic-network-archived-2026-10.md` (git mv) + 顶部加归档说明段
3. ✅ `docs/adr/ADR-DGPU-13-pnn-protocol-neutral-network.md` 顶部加 `## Status Update 2026-10-18: superseded by 06 v0.4 (未签发 ADR, 永不签发)` 段 (per AGENTS.md "ADR 不可变 + Status Update" 先例)
4. ✅ `docs/adr/ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md` 顶部同理
5. ✅ `06-inter-module-network.md` §10.1 矩阵说明 supersede 关系 (本节)

**不修改 ADR / 归档文档正文** (per AGENTS.md "ADR 不可变 + Status Update" 先例 + `cpptlm-dgpu-soc-arch-truth-cleanup §6` doc-only 豁免先例)

### 10.3 早期调研保留价值

| 来源 | 调研价值 |
|------|---------|
| gem5-ruby-protocol-bridge-survey.md | §6.1 CppTLM vs Gem5 Ruby 60-90% 匹配度映射表;§7.5 simulation_mode 对位 |
| gem5-messagebuffer-network-interface.md | §1.2 MessageBuffer ordered/unordered 单 bool;§2 NI 切分形态 |
| gem5-garnet-network-survey.md | §A Garnet 5-stage pipeline(演进 seam §15.2);Topology 抽象借鉴 |
| gem5-soc-survey.md | §2 gem5 example topology (Simple vs Garnet);§3 ARM Full-System Coherence 跨域实例 |
| ArchForge 09-coherence-protocol.md | v1.0-draft MOESI/GPU 6×6 + CoherenceBridge(06-bis §1-§10 详化) |
| ArchForge microarchitecture/coherence-protocol.md | CoherenceProtocol C++ 接口(06-bis §11 演进引用) |
| ArchForge microarchitecture/coherence-bridge.md | CoherenceBridge ChStream 接口(06-bis §11 演进引用) |

**调研参考保留**,06 v0.3 不复制内容(per gem5-ruby-protocol-bridge-survey §0 "不复制任何代码片段,避免版权 + 保持调研文档独立性")

---

## 11. 实施分阶段与 KPI

### 11.1 P0: doc-only + 文档落地(**不消耗 KPI slot**, per `cpptlm-dgpu-soc-arch-truth-cleanup §6` 先例 + Q8 K-D)

**目标**: 本方案 06 v0.3 落地 + supersede 标志 + Python 拓扑范例

**任务清单**(仅 doc, 无 build 改动):

1. **起草**: `docs/architecture/06-inter-module-network.md` v0.3(本文件)
2. **起草**: [`docs/architecture/06-bis-coherence-as-network-extension.md`](06-bis-coherence-as-network-extension.md) v0.3(同期, coherence 演进 seam 详细化)
3. **supersede 标志**:
   - `pnn-protocol-neutral-network.md` 顶部加 `## Status Update: superseded by 06 v0.3` 段
   - `05-packet-semantic-network.md` 顶部同理段
   - **ADR-DGPU-13 + ADR-DGPU-14 顶部加 `## Status Update 2026-10: superseded by 06 v0.3` 段**
4. **Python 拓扑范本**:
   - 扩 `examples/` 加 `dgpu_soc_dual_noc_demo.py` (per §6.3 v0.3 修订), 验证 Python 一条龙
   - `cpptlm/library/interconnect.py` 不必新增, 仅示范组合
5. **AGENTS.md / docs/ONBOARDING.md 增量**: §5.5 脚本表 / 索引补 `06` + `06-bis` 引用

**不动边界**(全部保持现状, P0 不破任何东西):
- 0 改 frozen headers / 23 ABI / Packet 字段 / ModuleFactory 代码
- 0 改 C++ Bundle 定义
- 0 改 `cpptlm_*` Python 源码(仅示例文件)
- **0 改 ArchForge 文档**(06 + 06-bis 仅引用, 不重发明; 跨仓同步由 P2 实施时处理)

**估时**: 1 周

**触发条件**: 用户 + Oracle/Metis/Critic 评审通过

### 11.2 P1 (实施期, 单 KPI slot) — **v0.4 BLOCKER B-1-A 修订 (方案 A: 收窄 P1 范围)**

**目标**: 协议元数据 seam **文档化** + Bundle 二分类 **测试契约**, **不引入代码** (与 §9.2 一致)

**v0.4 B-1-A 修订关键点 (vs v0.3 BLOCKER)**:
- v0.3 §11.2 把 P1 写成 "ChStreamModuleBase 增 `set_protocol_id/set_bridge_id` + ModuleFactory Step 7c 注入" — 与 §9.2:762/767 "P1 不引入 Step 7c / P3+ 再加静态字段" **直接矛盾**, 经 oracle + build agent 联合审稿判定为 P0 BLOCKER (B-1)
- v0.4 采纳方案 A: **P1 推迟所有代码改动到 P3+**, P1 仅做测试 + 文档 (per §15.3 per-packet tlm_extension 演进 seam 模板)
- v0.3 §11.2 line 885-905 的 `ChStreamModuleBase` 代码示例 **整段删除**, 移到 §15.3 作为 P3+ 实施模板
- v0.3 §11.2 line 909-913 的 `NetworkProtocolExt` 新增任务 **删除**, 移到 §15.3
- v0.3 §11.2 line 919-921 的 Step 7c 增量 **删除**, 与 §9.2 一致: 不引入 Step 7c

**v0.4 P1 任务清单 (5-8d, 仅测试 + 文档)**:

1. **新增** `test/test_bundle_classification_contract.cc` (~120 行, ≥10 cases / ≥40 assertions):
   - 验证 §3.2 归类表的 18 个 .hh (per `ls include/bundles/*.hh`) 全部正确标注 (业务 / 网络 / 跨模块共享的业务(第三类)/ 基础设施)
   - 验证每个业务 Bundle 有 default initializer (per §3.4 演进规则)
   - 验证每个网络 Bundle 通过 `BUNDLE_FIELD_WIDTH` 编译期锁
   - 标签: `[bundle-classification]`

2. **新增** `test/test_simulation_mode_schema.cc` (~80 行, ≥5 cases / ≥20 assertions):
   - 验证 JSON 顶层 `simulation_mode` 字段只有 `functional|timing` 2 值
   - 验证 `network.mode` 字段在所有 `configs/*.json` 0 匹配 (per §8.2 不引入承诺)
   - 验证 `VramControllerTLM::set_mode()` 在 functional/timing 2 模式正确 set
   - 标签: `[simulation-mode]`

3. **新增** `test/test_seam_documentation_locator.cc` (~60 行, ≥5 cases / ≥15 assertions):
   - 验证 §4.1 seam L5↔L4 位置在文档 + 注释 + openspec change 3 处一致引用
   - 验证 §15.3 per-packet tlm_extension 演进 seam 在 06 + 06-bis 2 文档 cross-ref 存在
   - 标签: `[seam-locator]`

4. **CMakeLists.txt 增量**:
   - `test/CMakeLists.txt` 加 3 个新测试文件 (per test/AGENTS.md GLOB 例外)

5. **06-bis v0.3 §11 cross-ref 补完**:
   - 06-bis §11 关联文档段加指向 06 §15.3 + §4.4
   - 06 §15.3 加指向 06-bis §11

**KPI** (v0.4 N-3 加注):
- P1 消耗 1 slot
- 启动前提: KPI slot 释放 — 当前 `openspec/changes/` Proposed 并发 2/3, 有 1 个空 slot (per §11.5 KPI 基线修订)
- **v0.3 旧版 "5/3 超限" 是 raw active 总数口径错误** (2 Proposed + 3 Draft), 实际 KPI 上限按 Proposed 并发口径 (per AGENTS.md)
- 释放候选: **无需释放** (有空 slot), 如需并行 activate 多个 change 再 archive

**估时**: **5-8d** (v0.3 旧版 10-15d 偏宽, v0.4 收窄 ~50%)

**触发条件**: P0 完成 (2026-10-18 已落地) + KPI slot 释放 (当前有空 slot, 可立即 activate)

**P3+ 推迟任务** (替代 v0.3 §11.2 原 line 907-933 的代码增量, 估时待 P3 立项时重估):
- P3.0: `NetworkProtocolExt` 文件框架 (per §15.3 演进 seam)
- P3.1: `ChStreamModuleBase` 增 `set_protocol_id/set_bridge_id` + override `get_stats_group()` (默认空实现)
- P3.2: ModuleFactory Step 7c 精简版 (per-bridge 静态分配, 拒绝 ProtocolBridgeFactory 模式)
- P3 估时: 20-30d 占位 (per §15.3 现状)

> **⚠️ 2026-10-27 修订(per `ADR-CEP-19` 决策 4 / 7;台账 Entry #74)** —— 上列 **P3+ 时间线**与 **P3.2 的「拒绝 ProtocolBridgeFactory 模式」**两处口径,处置如下:
> **① 时间线转隶 CEP(决策 7)**:本节 P1 / §11.3 P2 的**实施分期从未执行** —— 实测 `06 §11.2` 承诺的 3 个测试(`test_bundle_classification_contract.cc` / `test_simulation_mode_schema.cc` / `test_seam_documentation_locator.cc`)与 `include/core/coherence_bridge.hh` / `src/core/coherence_bridge.cc` **全仓均不存在**;`openspec/changes/` 与 `openspec/changes/archive/` **无**对应 change 目录 ⇒ 该分期**已被 06-bis 的 CEP 继任取代**(`06-bis` 顶部 `Status Update (2026-10-21) — superseded by CEP`)。⇒ **本节分期时间线作废**,承载 seam 的代码增量改由 **CEP 排期**承接:`ADR-CEP-19` 决策 1 / 5 落在队列 **#7 `cpptlm-cep-cohif-carrier`**(Stage 1 / 2028-Q2,排期真源 = `obj-cep-platform §4.0`)。**本段不改上列任务条目本身**(它们仍是**技术内容**的权威描述),只登记**时间线**与**归属**的转移。
>
> **② 显式偏离 P3.2 的「拒绝 ProtocolBridgeFactory 模式」(决策 4)**:本段 P3.2 写「per-bridge 静态分配, **拒绝 ProtocolBridgeFactory 模式**」,而本文件 **§9.3** 写「当 P3+ 引入 `ProtocolBridgeFactory` 时,**扩展** `register_protocol_bridge` 为 `(bridge_name, factory_lambda)` 形式 —— 沿用现有 register 入口,**不重发明**」。⇒ **本文件内部两条互斥**。**处置**:
> - **遵从 §9.3** —— 采纳**钩子签名扩展**路径(`module_factory.hh:313-315` 的 `register_protocol_bridge` **2026-10-27 实测为 write-only 死钩子**;`get_bridges()` 在 `private:` 段 `:327`(2026-10-27 实测),全仓无人读;**2026-10-28 已实施**(现为 **346**,**`public:`**) —— 见 §9.3 与文末行号对照表);
> - **显式偏离**本段 P3.2 的「拒绝」条款 —— 偏离理据:该条拒绝的是**独立 factory 抽象层**(与 `01 号 §1.1`「当前 L3+L4 已达 Gem5 MessageBuffer 水准,**不需新增抽象层**」一致);§9.3 的**钩子签名扩展**不产生抽象层 ⇒ 二者**目标一致但字面冲突**,以 §9.3 为准;
> - **不新建** `ProtocolBridgeFactory` 类(per `ADR-CEP-19` 决策 8「不引入新抽象层」);
> - **落地形态**见 `ADR-CEP-19` 决策 5:`REGISTER_CHSTREAM_BIDIRECTIONAL` 宏 + `register_protocol_bridge` 签名扩展,**由队列 #7 交付**。

### 11.3 P2 (实施期, 单 KPI slot — 等 P1 后)

**目标**: `CoherenceBridge` 落地 + Python 拓扑支持多 dGPU 子系统跨域桥接(per 06-bis §11-§12)

> **⚠️ 2026-10-27 时间线转隶(per `ADR-CEP-19` 决策 7;台账 Entry #74)**:本节的 **`CoherenceBridge` C++ 实施**已被 06-bis 的 CEP 继任取代 —— 跨域组合改由 `ADR-CEP-13` 承接(走「人工声明 + 实验证伪」路线),**不做**单一 `CoherenceBridge` C++ 类。**CohIf 侧**的载体由队列 **#7 `cpptlm-cep-cohif-carrier`** 交付(见 §11.2 的 ① 登记)。本节**目标文字零改动**(仍是技术内容的权威描述),只登记**时间线**转隶。

**任务清单**(诚实含 build 改动):

1. **新增** `include/core/coherence_bridge.hh` / `src/core/coherence_bridge.cc`(per ArchForge `coherence-bridge.md` §3 已有完整接口草案)
   - `CoherenceBridge : public ChStreamModuleBase` (programmatic bridge 路径 per 06-bis v0.3 §3.4)
   - `set_*_target` programmatic interface (per `port_contract_asserts.hh:42-45` setter-only 模式)
2. **CoherenceProtocol C++ 实现**(per ArchForge `coherence-protocol.md` §3, 集成基类 + MoesiAmdProtocol 实现,**类型改写** `std::string` → `ch_uint<8>` per 06-bis v0.3 §5)
3. **`cpptlm/library/interconnect.py`** 增量: `coherence_bridge(...)` 工厂方法(per 06-bis v0.3 §3.5 推迟到 P3.5 实施)
4. **`examples/dgpu_soc_with_coherence.py`** Python 范例(per 06-bis v0.3 §6.2)
5. **测试**: `[coherence]` 标签
6. **不动 Packet**(仍 §4.5 per-bridge 静态 + per-packet tlm_extension 演进 seam **不实施 per-packet**)

**KPI**: 1 slot
**估时**: 30-45d
**触发条件**: P1 完成

### 11.4 P3+ (远期, 各自占 KPI 串行)

| 阶段 | 范围 | 估时 | KPI 触发 |
|------|------|------|---------|
| **P3** | Topology 参数化(per-link latency/weight/bandwidth, per-router pipe_stages) + per-packet tlm_extension(Step 2.5/5.5 hooks) | 20-30d | P2 完成 |
| **P3.5** | JSON 端口路径打通(走 06-bis v0.3 §3.4 候选 C, 即扩 Step 7a registerCoherenceAdapter)+ Python 工厂落地 | 30-45d | P3 完成 |
| **P4** | 5-stage router pipeline (Gem5 Garnet 对位) + credit cycle-accurate | 30-45d | P3.5 完成 |
| **P5** | 多 SimModule 跨协议互通 + SnoopFilter directory-based coherence | 60-90d | P4 完成 |
| **P6** | CHI 完整协议 + `DtifCacheTLM` CHI-aware cache | 90+d | P5 完成 |

### 11.5 KPI 现状基线 — **v0.3 BLOCKER B-δ 修订**

**真约束是排队治理 (slot 释放顺序) 不是 slot 总数** — **v0.4 N-3 口径加注**

**口径说明** (v0.4 N-3 修订):
- **"Proposed 并发口径"**: 计入 KPI 上限 (per AGENTS.md "OpenSpec Proposed ≤ 3" 政策)
- **"active 总数口径"**: 含 Proposed + Proposed Draft, **不计入 KPI** (per `cpptlm-dgpu-soc-arch-truth-cleanup §6` doc-only 豁免先例)

| 状态 | 计数 (Proposed 并发) | 计数 (active 总数) | 备注 |
|------|---------------------|-------------------|------|
| 当前 OpenSpec active | 2 | 5 | Draft: control-hub / physical-ip-port-boundary / msix-generator-ip; Proposed: p2-integration-unblock / pcie-tlp-entry-layer |
| Proposed (计入 KPI) | **2** | 2 | p2-integration-unblock + pcie-tlp-entry-layer |
| Proposed Draft (不计 KPI, per §6 doc-only 豁免) | 0 | 3 | control-hub / physical-ip-port-boundary / msix-generator-ip |
| 上限 | **3 (Proposed 并发)** | 不限 (Draft 不计 KPI) | per AGENTS.md |
| **当前 Proposed 并发占用** | **2** | — | **有 1 个空 slot** |
| Draft (待激活为 Proposed) | 0 | 3 | 等 1 个空 slot 释放即可激活一个 |
| 06 P0 (06 v0.4) | 0 slot | 0 slot | doc-only, **已落地 2026-10-18** |
| 06 P1 | 1 slot | 1 slot | 需 slot 释放 — 但**当前有空 slot**, 可直接 activate |
| 06 P2 | 1 slot | 1 slot | 串行 (不与 06 P1 并发) |
| 06-bis P2.0 | 1 slot | 1 slot | 串行 |
| 06-bis P2.1 | 1 slot | 1 slot | 串行 |
| 06-bis P3 | 1 slot | 1 slot | 串行 |
| 06-bis P3.5 | 1 slot | 1 slot | 串行 |
| 06-bis P4 | 1 slot | 1 slot | 串行 |
| 06-bis P5 | 1-2 slot | 1-2 slot | 串行 |

**结论** (v0.4 N-3 修订):
- **P0 doc-only 已落地** (2026-10-18, 0 消耗 KPI)
- **P1 启动无需等 slot 释放** (当前有空 slot, per C-7 §11.2 v0.4 收窄方案 A 5-8d 估时)
- **任何时刻并发占用 Proposed ≤ 3** (06 1 + 06-bis 1 + 现有 2 = 4 ? → 这就是阻塞点; 实际 06 与 06-bis 在不同时间点激活, 任何 1 时刻最多占 1 slot)
- **真约束**: 排队治理, 不是 slot 总数 (per oracle Q-δ)

---

## 12. 风险与失败模式

### 12.1 风险清单

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | **KPI slot 释放不可控** — P1 启动需 archive 一个 change | 🟡 中 | 季度复审兜底;同时启动 P0 doc-only 不依赖 KPI |
| **R2** | **Python 拓扑约定与 C++ 端偏差** — module name 命名、port_index 解析不一致 | 🟡 中 | `library/interconnect.py` 已有契约, 锁住 `mesh_cluster` 的"modules/connections 双层结构 + port_index 0-4" |
| **R3** | **业务 Bundle 演进破坏其他 module** — 字段加宽 / 类型切换影响序列化 | 🟢 低 | §3.4 演进粒度规则文档化 + 模块级 TestCase |
| **R4** | **NoCFlitBundle 16 字段不够覆盖大块传输**(>256B AxiMemBundle) | 🟡 中 | §15.2 演进 seam: 双 Bundle (业务 Bundle 大块 + 网络 Bundle protocol metadata) |
| **R5** | **Step 7c 静态注入与 type-erased ProtocolBridge 矛盾** | 🟡 中 | P1 仅 per-bridge 静态字段(ch_mod 直接持有), 不引入 type-erased 模板基类 |
| **R6** | **CoherenceBridge 引入与现有 ChStreamModule 边界模糊** | 🟡 中 | 06-bis v0.3 §3.4 programmatic bridge 路径(候选 C)与现有 7 类 SimModule 容器风格一致 |
| **R7** | **dual-port 网络已用 NICTLM,** 其通用化会触类型擦除硬编码 | 🟢 低 | **拒绝通用化**(per §7.3 + metis 调研, 1 个用户 + programmatic bridge 模式足够) |
| **R8** | **ADR 不可变性**: §10.2 supersede 标志在 P0 阶段动老 PNN/05 文档 — 可能争议是否"修改 ADR" | 🟢 低 | 仅加 `## Status Update` 段 (per `01-hybrid-architecture-v2.1.md` v2.1.10 Status Update 先例, 不破 ADR 不可变) |
| **R9** | **ChStreamModuleBase P1 增量: 新虚函数破坏既有 75+ 模块** | 🟢 低 | 增量**默认空实现**(per §11.2 Step 2), 向后兼容 |
| **R10** | **PacketPool 池复用串包** (per oracle 评估 §6) | 🟢 低 | §2.5 决定不动 Packet; 新增字段必走 tlm_extension (池复用 delete-on-acquire 已保护) |
| **R11** | **多套独立网络 + System 组合的命名冲突** | 🟡 中 | Python 端 `name_prefix` 参数机制(per `library/interconnect.py:24-28`)锁住前缀避免冲突 |
| **R12** | **06-bis P2.1 programmatic bridge 与 active Draft 方向冲突** | 🟡 中 | 06-bis v0.3 §3.4 声明与 `cpptlm-dgpu-soc-physical-ip-port-boundary` / `cpptlm-dgpu-soc-control-hub` 的豁免条款(永久豁免 vs 临时态) |

### 12.2 失败模式(来自 Gem5 / 工业经验教训)

| 模式 | 来源 | 缓解 |
|------|------|------|
| **F1** — P1 启动时未及先 archive KPI slot, 蓝图长期搁置 | AGENTS.md "OpenSpec Proposed ≤3" 历史教训 | **P0 doc-only 立即启动**, P1 等 KPI |
| **F2** — 协议桥内藏状态后切换 mode, 状态守恒失败 | gem5 Garnet 20+ 年经验 | **P1 不引入 ProtocolBridge 模板基类**; per-bridge 静态字段无状态 |
| **F3** — Topology 字段加了但 C++ 端拒识别 | PNN v0.1 §2.6 Step 7b 复杂度教训 | **不引入 network.mode 字段**(per §8.2) |
| **F4** — 测试矩阵 N 协议 × M 模式 爆炸 | gem5 ruby_random_test.py (per gem5-ruby-protocol-bridge-survey §5) | **P1 仅 per-bridge, 不引入 mode 字段**; 测试矩阵 1 维度 |
| **F5** — Bundle 字段加/减破坏 `is_trivially_copyable` 假设 | `bundle_serialization.hh:22-27` memcpy | §3.4 演进规则: 加字段**必须**给 default initializer, 且 serializer 容错 |
| **F6** — 强制 NICTLM 通用化触碰类型擦除硬编码 | oracle `module_factory.cc:707-709` 调研发现 | §7.3 显式**拒绝通用化** |
| **F7** — Python 工厂函数(soc.py) 与 C++ builder() drift | (P3+ 风险, 持续校准) | 06 §7 已锁定 `mesh_cluster(rows,cols)` 契约 |

### 12.3 风险评估:和 PNN 0.1 比, 本方案少了 3 个高风险

| PNN 0.1 风险 | 本方案对应 |
|--------------|-----------|
| 改 Packet 字段(05-doc §2.2 提, 36672 [pcie] 断言) | §2.5 + §4.2 决定 0 改 |
| Step 7c 新引入 ProtocolBridgeFactory 模式解析 | §9.2 决定 P1 不引入 |
| 新增 NetworkModule 基类 | §6.1 + §6.4 决定不新增 |

→ **P1 启动风险面比 PNN v0.1 小约 60%**(仅 1 个新文件 + ChStreamModuleBase 增量 + ModuleFactory 增量, ~150 行业务代码)

### 12.4 与既有架构债务的关系

`communication-network.md` v1.0 §7 列 8 债务,**06 v0.3 不强求解决任何**(per §3.5 表) — 留给用户 / 既有 change 决议。

### 12.5 "运行时协议切换"比 Gem5 更激进(诚实承认)

per 早期调研(oracle + librarian 调研 PNN v0.1)的事实更正:
- Gem5 协议**编译期烧死**(SLICC), `--protocol=CHI` 触发重编译
- CppTLM 假设 `network.mode` JSON 字段切换协议 = **运行时切换**, **比 Gem5 更激进**
- **本方案拒绝 network.mode 字段**(per §5.2), 沿用"协议特化是工程"原则; **不引入这 3 个新挑战**:
  1. 测试矩阵 N × N(协议 × mode)爆炸
  2. 桥内状态(OOO rid 表)热重启
  3. ProtocolBridgeFactory::create() 模式名解析 + bridge_id 全局分配 + bridge state 切换的 **runtime reflection**
- **保留 P3+ per-packet tlm_extension 演进 seam**(per §15.3)应对真出现 per-packet flavor 需求

### 12.6 cpp-tooling 兼容性与 cpptlm_config 弃用处理

per `cpptlm_config/AGENTS.md`:
- `cpptlm_config` 已 deprecated (2026-06-17)
- 新代码用 `cpptlm.topo` + `cpptlm.topo.CxxCompatibleEmitter` + `cpptlm.library`
- 06 §7 明确**使用 `cpptlm`** 而非 `cpptlm_config`
- P0 阶段: `cpptlm_config/AGENTS.md` 的 deprecation warning 保留, **06 不主动修**(尊重既有迁移计划 `openspec/changes/unified-config-emitter`)

---

## 13. 命名约定

### 13.1 实体命名

| 名称 | 路径 | 命名风格 | 理由 |
|------|------|----------|------|
| **方案代号** | **Inter-Module Network (06)** | 全称,无缩写 | 与 v2.1 hybrid, communication-network 等现有命名风格一致 |
| 文档存档 | `docs/architecture/06-inter-module-network.md` | snake_case + NN- 前缀 | 与 01-05 同层 |
| Bundle 二分类 | **业务 Bundle** / **网络 Bundle** | 中文(v2.1.10 中文风格一致) | 用户 A 决策 |
| SimModule 角色类型 | RouterTLM / CrossbarTLM / LinkTLM / NICTLM / SocInternalNoc / GpuInternalFabric / VramControllerTLM | 沿用 CamelCase | 既有 — 06 不重命名 |
| 协议元数据承载 | `NetworkProtocolExt : tlm_extension<T>` (P1 P3+) | 同 `TransactionContextExt` 风格 | per oracle 评估 §3.1 |
| ChStreamModuleBase API 增量 | `set_protocol_id(uint8)` / `protocol_id()` / `set_bridge_id(uint16)` / `bridge_id()` | snake_case getter/setter | per `cpptlm/tlm` CamelCase 类 + snake_case 方法 |
| JSON 字段 | `simulation_mode` (顶层, **不引入 network.mode**) | snake_case + 嵌套 | per existing |

### 13.2 测试标签

| 标签 | 范围 | 触发 |
|------|------|------|
| `[inter-module-network]` | 06 §11 实施后新增 | P1 完成 |
| `[network-protocol-ext]` | 06 P1 新增 NetworkProtocolExt 测试 | P1 |
| `[cep]` | **CEP(Coherence Exploration Platform)测试标签** —— 19 个 `test_cep_*.cc` 实际使用(实测 161 case / 2600 assertion) | CEP 各 change 落地 |
| `[coherence]` | 06-bis 后续 Phase 2 标签 | ⚠️ **已被 `[cep]` 取代** —— 该标签现被 legacy `test/test_coherence_domain.cc` 占用(`[[deprecated]]` 类 `CoherenceDomain` 的回归锁);CEP 新测试**不**使用它 |
| `[pnn]`, `[pnn-baseline]`, `[pnn-axi4]` | PNN 早期 | **不引入**(per §5.2 不引入 network.mode 字段) |
| `[packet-semantic]`, `[pnn]` | 05-doc 早期 | **不引入**(per §10.1 superseded) |

### 13.3 与既有命名相消解

| 名称冲突 | 处置 |
|----------|------|
| "协议中立层"(pnn 引入) vs "协议抽象层"(coherence-protocol.md) | **沿用 ArchForge 既有 "协议抽象层"** — "协议中立" 仅作为 §0 营销用语 |
| "inter-module network" vs "communication network" (communication-network.md v1.0 既有) | **06 是 communication-network.md v0.1 落地版**, 沿用 "communication network" 术语; 06 文档代号 "inter-module network" 强调"模块间"语义 |
| "ProtocolBridge" (PNN) vs "ProtocolAdapter" (05-doc) | **沿用 PNN `ProtocolBridge`**(semantically clearer), 05-doc 弃 |
| "L_PNN" / "L4.5" 抽象类 | **不沿用** — 06 v0.3 不引入新抽象层(per §4.2 + §6.4) |
| "TopoOrchestrator" typo | 文档全文用 **TopoOrchestrator**(`orchestrator.py:15` 真名) |

---

## 14. 评审清单(13 条 + v0.3 Oracle/Metis/Critic B-1 到 B-δ 修订)

### 14.1 自检表

| # | 自检项 | 结论 |
|---|--------|------|
| 1 | Bundle 二分类边界清晰(业务 vs 网络) | ✅ §3.1 + §3.2 判定规则 + §3.2 13 行归类表 |
| 2 | seam 在 L5 Bundle ↔ L4 StreamAdapter 之间(per Q5 S-A) | ✅ §4.1 锁定 |
| 3 | 不动 Packet 字段(per Q1) | ✅ §2.5 + §4.2 0 改 |
| 4 | 互联网络即 SimModule, 不新建基类(per A3 + metis B5) | ✅ §6.1 + §6.4 |
| 5 | Python 一条龙 + JSON 模板契约(per A1) | ✅ §7.1 + §7.2 + §7.5 |
| 6 | simulation_mode 单一维度, 不引入 network.mode(per E + 最新要求) | ✅ §5.1 + §5.2 |
| 7 | 6 标签 0 回归承诺 | ✅ 06 v0.3 不动 C++ 源码边界(P0 阶段)|
| 8 | frozen headers 0 改 | ✅ §11.1 (P0 不动) + §11.2 (P1 仅 ext 文件) |
| 9 | 23 ABI 0 改 | ✅ |
| 10 | 不引入 DSL(per PNN D-SLICC 教训) | ✅ 沿用 C++ 直接 + Python |
| 11 | 不重发明 ArchForge coherence 既有接口 | ✅ §15 引用 + 06-bis v0.3 §1 详细化 |
| 12 | supersede 关系清楚 | ✅ §10.1 矩阵 |
| 13 | KPI 启动路径明确 | ✅ §11.5 KPI 口径修正 |

### 14.2 v0.3 BLOCKER 修订 (Oracle Metis Critic 3 轮)

| BLOCKER | v0.3 修订位置 |
|---------|--------------|
| **Oracle B1** Packet 12→14 字段 | ✅ §2.5 列表 14 字段 + 活跃/死分类 |
| **Oracle B2** CrBundle 双重错 | ✅ §3.2 归类表重写, 删 gem5 mpam 引用 |
| **Oracle B3** Axi4Bundle ≥5 消费者 | ✅ §3.2 标 ≥5 消费者(列全) |
| **Oracle B4** §10.1 漏 ADR-DGPU-13/14 | ✅ §10.1 + §10.2 加 2 行 + 加 Status Update 段 |
| **Oracle B5** §2.1 L4/L5 编号 | ✅ §2.1 重画 canonical 编号(0-8) |
| **Oracle B6** §6.3 SimModule vs Python layer | ✅ §6.3 区分 flat+name_prefix / SimModule 嵌套 / programmatic bridge 三用法 + 示例 bug 修订 |
| **Oracle B7** ArchForge 状态 | ✅ §10.1 改正为 🟡 草案 + §15.1 同步 |
| **Oracle B8** §11.2 P1 write-only | ✅ §11.2 P1 修订用 `tlm_stats::addScalar` + override `get_stats_group` + real consumer |
| **Metis 8-端口不可行** | ✅ 06-bis v0.3 §3.4 programmatic bridge(候选 C) |
| **Metis SnoopProbe 字段算术** | ✅ 06-bis v0.3 §5 类型改写 |
| **Metis std::string 非 POD** | ✅ 06-bis v0.3 §5 协议 ID 表 |
| **Metis module_factory stub** | ✅ 06-bis v0.3 §4.2 承认 + 06-bis P2.0/P2.1 拆分 |
| **Metis to_dict 增量 no-op** | ✅ 06-bis v0.3 §4.1 撤销任务 |
| **Metis 跨仓 contract** | ✅ 06-bis v0.3 §5.5 加 ArchForge STATUS UPDATE 同步契约 |
| **Critic B-α §2.1 L4/L5 编号** | ✅ §2.1 重画 0-8 canonical(等同 Oracle B5) |
| **Critic B-β §11.2 add_scalar API 错** | ✅ §11.2 改 `addScalar` 真 API + `+=` 累加写入 |
| **Critic B-γ 48 处 v1 占位** | ✅ 全文重写, 0 占位 |
| **Oracle Q-α programmatic bridge 与 active Draft 冲突** | ✅ 06-bis v0.3 §3.4 + 06 R12 声明豁免条款 |
| **Oracle Q-β SnoopRespBundle 命名** | ✅ 06-bis v0.3 §5 改 SnoopRespBundle |
| **Oracle Q-γ DomainRegistry 生命周期** | ✅ 06-bis v0.3 §4.2 加生命周期管理 |
| **Oracle Q-δ KPI 口径错位** | ✅ §11.5 重写 KPI 表, 口径明确 |
| **Oracle Q-ε add_coherence_domain passthrough** | ✅ 06-bis v0.3 §6.2 提 1 行 passthrough |

### 14.3 评审问题(给 Oracle / Metis / 团队)

#### 14.3.1 Bundle 二分类问题

1. §3.2 判定规则(网络层字段 + 项目级所有权)合理吗? 还是需要第三个判定维度(如"per-hop 序列化 vs 持久事务标识")?
2. §3.4 演进粒度(字段级加/减 + 类型级切换)约束足够严吗? 还是需要补"序列化兼容性"测试契约?
3. `dgpu_fabric_bundles_tlm.hh::*` 当前混合(per §3.2 注), 06 留给 `cpptlm-dgpu-soc-physical-ip-port-boundary` 处置 — 是合理分工吗?

#### 14.3.2 seam 与 Packet 不动问题

1. §4.1 seam 位置 L5↔L4 — 是否需要 second seam 在 L4↔L3 之间(借鉴 Gem5 NI/Router 双层)?
2. §2.5 Packet 0 改 + 死字段保留 — 是"保留 = 0 行为变化最小"的稳健选择, 还是"清理 7 死字段 + stream_id 删"的简化方案更利于演化?
3. §4.4 ProtocolBridge<InB, OutB, Ordered> 模板签名 — 是否引入`StateT` 模板参数(per PNN §10.1 Q1)提前预留 OOO 状态机?

#### 14.3.3 互联网络作为 SimModule + Python 拓扑问题

1. §6.2 "互联网络角色 module type" 标注是否需要 JSON schema 显式声明 `kind: "noc"` 字段, 而非仅文档标注?
2. §7.3 SiblingCluster 增量取消, 维持 DualPortStreamAdapter 唯一化 — 是否严格保守? 还是 P3+ 真出现 NICTLM 多种类时必须扩展?
3. §7 `mesh_cluster` + `ring_cluster` + `SoC` 当前 Python 工厂已经足够支撑"多套独立网络 + System 组合"(per §6.3 示例), 还要不要新增 `dtb_noc_cluster` (DisjointNetwork 借鉴 gem5)?

#### 14.3.4 coherence 演进 seam 问题

1. §15 + 06-bis 完整化 ArchForge `09-coherence-protocol.md` L7 Coherence + `coherence-protocol.md` `CoherenceProtocol` + `coherence-bridge.md` `CoherenceBridge`, **06 不重发明** — 跨仓引用是否充分? P1 启动 `cpptlm` 部分前是否需要先在 ArchForge 同步合规性?
2. 06 §15.4 coherence 推进路径 P1(per-bridge 静态) → P2(CoherenceBridge 完整) → P3+(per-packet tlm_extension)— 时序合理吗?
3. §15.1 CoherenceDomain Spec 已经存在(per `layer.py:43-49` + cpptlm Python 端), 但 C++ 端实现可能未完整 — 06 范围内 vs ArchForge 范围内谁落地?

---

## 15. 演进 seam(coherence / topology 参数化 / 5-stage pipeline)

### 15.1 Coherence 演进总览

**06 不实施 coherence 协议**, 仅 **演进 seam 引用 ArchForge 既有**:
- `09-coherence-protocol.md` v1.0-draft: MOESI/GPU 6×6 + CoherenceDomain + CoherenceBridge + SnoopFilter + CoherentXBar
- `microarchitecture/coherence-protocol.md` v0.x: `CoherenceProtocol` 抽象基类 + `MoesiAmdProtocol` / `MesiGpuProtocol` 实现 + `SnoopProbe` / `SnoopResp` Bundle
- `microarchitecture/coherence-bridge.md` v0.x: `CoherenceBridge : ChStreamModuleBase` 8 端口设计 + `translate_request/snoop` + `forward_snoop_*`
- `microarchitecture/coherence-domain.md`: per-cache 状态机 + snoop callback

> **⚠️ 2026-10-27 修订(per 台账 Entry #82 —— 借鉴 §11.2 的「时间线转隶」样板,不改上列条目本身)** —— 上列 4 条是**技术内容**的权威描述(ArchForge 侧既有接口),**本节不改**;但**时间线与归属**已转移:
> - **① coherence 演进的范围已由 CEP 承接** —— 06-bis 已被 [`07-coherence-exploration-platform.md`](07-coherence-exploration-platform.md) **supersede**(per 06-bis 顶部 `Status Update (2026-10-21) — superseded by CEP`);CEP 侧顶层真源 = 07 号,决策真源 = `docs/adr/ADR-CEP-*`(22 份)+ `ADR-CEP-00 §2 Decision 3` 的 14 change 串行队列(排期真源 = `obj-cep-platform §4.0`)。
> - **② 本节列的 4 个 ArchForge 文档仍是**协议语义的**上游引用面**,**不重发明**(per §0.3 / §10.1)—— CEP 侧只提供 adapter 载体 + 能力声明 schema;**协议语义建模归 ArchForge**(per AGENTS.md:423)。
> - **③ `06 §15.4` 的 P 编号仍是** P2.0 / P2.1 / P3 / P3.5 / P4 / P5(无 P6,per `ADR-CEP-NUMBER-MAP §2 Decision 3` 冻结)—— **编号口径不变**,但**实施分期作废**,改由 CEP 队列承接(见 §15.4 转隶注记)。

**关联 OpenSpec 草案**: `openspec/changes/2026-10-16-cpptlm-coherence-extension/` (待起草)
**关联草案 06-bis**: [`docs/architecture/06-bis-coherence-as-network-extension.md`](06-bis-coherence-as-network-extension.md) v0.3(同期, 详化实施路径) —— ⚠️ **已被 07 号 supersede**(per 其顶部 Status Update)

### 15.2 Topology 参数化演进(P3+)

借鉴 Gem5 Garnet 拓扑参数(`gem5-garnet-network-survey.md` §G):

| 参数 | C++ 端字段 | 何时引入 |
|------|------------|---------|
| `--mesh-rows` / `--num-rows`/`--num-cols` | `RouterTLM::mesh_x, mesh_y` (per `library/interconnect.py:45`) | **已落地** |
| `--link-latency` | `connection[].latency` (per `library/interconnect.py:50`) | **已落地** |
| `--vcs-per-vnet` | **演进**: `module.params["vc_count"]` + `NoCFlitBundle.vc_id` 4-bucket (4 vnet per CHI) | P3 |
| `--link-width-bits` | **演进**: `connection[].attrs.bandwidth_bytes_per_cycle` | P3 |
| `--routing-algorithm` (table / XY / custom) | **演进**: `module.params["routing"]` + `RouterTLM::routing_policy` 枚举 + CustomRouting 子类 | P3 |
| `--garnet-deadlock-threshold` (50000) | **演进**: `RouterTLM::deadlock_threshold_` 字段 + panic on overflow | P3 |
| `--router-latency` (multi-cycle pipeline) | **演进**: `RouterTLM::pipe_stages_` 字段 | P4 |
| `--ni-flit-size` (16 bytes default) | **演进**: `module.params["flit_size"]` | P4 |
| `vnet_type=data\|ctrl` credit capacity 差异化 | **演进**: `RouterTLM::vc_credit_strategy_{data, ctrl}` 二元(per gem5 调研 §3.2, 不能 4 类) | P4 |

### 15.3 per-packet tlm_extension 演进(P3+ 真实需要时落)

per oracle 评估 §6 + oracle §3 + §4.4:

**触发条件**: 真出现 per-connection 异构 flavor 需求(per `ConnectionSpec.bandwidth`, 或 CHI snoop flavor per-packet)

**实施路径**:
1. 新增 `NetworkProtocolExt : tlm::tlm_extension<NetworkProtocolExt>` (per §11.2 P1 已增, 但**P1 不写 payload 字段, 只发文件框架**)
2. Step 2.5 + Step 5.5 显式钩子:
   - `OutputStreamAdapter<>::send()` 末尾调 `ch_mod->apply_protocol_adapter_on_send(pkt)`
   - `InputStreamAdapter<>::process()` 中(bundle deserialize 后)调 `ch_mod->apply_protocol_adapter_on_recv(pkt)`
3. `ChStreamModuleBase` 加 2 个虚函数 `apply_protocol_adapter_on_send/recv(pkt)`, 默认空实现
4. `ProtocolAdapter::pack_with_packet(Packet*)` 入口写 `flit_category` / 协议 flavor 到 NetworkProtocolExt

**P3 估时**: 20-30d

### 15.4 Coherence 演进推进路径(与 06-bis v0.3 §7 一致)

| 阶段 | 增量 | 估时 |
|------|------|------|
| **P2.0** | `CoherenceProtocol` C++ 实现 + 类型改写(string→ch_uint<8>) + 3 Bundle 字段定义 | 30-45d |
| **P2.1** | `CoherenceBridge` C++ 实施(走 programmatic bridge 路径 per 06-bis v0.3 §3.4) + parser stub 替换 + 集成测试 | 30-45d |
| **P3** | `SnoopFilter` directory-based + `MesiGpuProtocol` + `MesiThreeLevelProtocol` + `translate_snoop` 协议转换 | 30-45d |
| **P3.5** | JSON 端口路径打通(扩 Step 7a registerCoherenceAdapter 候选 C) + Python 工厂落地 | 30-45d |
| **P4** | CHI 完整协议支持(REQ/SNP/RSP/DAT 4 vnet 借鉴 Gem5 mainline) | 30-45d |
| **P5** | IOMMU 集成(per ArchForge `09-coherence-protocol.md` §13 R5, 不是 `coherence-bridge.md` R5) + 跨 vendor coherence(NVIDIA USRI vs AMD Infinity Fabric) | 60-90d |

> **⚠️ 2026-10-27 修订(per 台账 Entry #82 —— 借鉴 §11.2 样板;**不改 P 编号与上表条目本身**)** —— 本表的 **P 编号口径(P2.0 / P2.1 / P3 / P3.5 / P4 / P5,无 P6)** 是 `ADR-CEP-NUMBER-MAP §2 Decision 3` 冻结的**唯一编号真源**,并被 `06-bis` 顶部 Status Update 引用为「本文档的 P 编号以 `06 §15.4` 为准」⇒ **本表条目与编号一律不动**。变更的只是**实施分期与归属**:
> - **① 实施分期已转隶 CEP(时间线作废)** —— 本表 P2.0 ~ P5 的**实施分期从未执行**;承载 seam 的代码增量改由 **CEP 排期**承接:顶层真源 = [`07-coherence-exploration-platform.md`](07-coherence-exploration-platform.md),决策真源 = `docs/adr/ADR-CEP-*`,**排期真源 = `obj-cep-platform §4.0`**(14 change 串行队列,Stage 1 = 10 / Stage 2 = 2 / Stage 3 = 1)。
> - **② P 阶段 → CEP 队列的逐项对应**(per `ADR-CEP-16 §2` 决策 1 + `07 号 §8.2` 承接映射表):**P2.0** → `ADR-CEP-04 §6` + `ADR-CEP-03 §3.2` R3 · **P2.1** → `ADR-CEP-01 §2 Decision 1`(programmatic bridge 路径)· **P3** → `ADR-CEP-04 §2` SharerTracker 5 实现 · **P3.5** → 沿用 `06 §7` Python 拓扑 + ProtocolEngine 插件描述符 · **P4** → `ChiRnfAdapter` 完整实现(subset → full)· **P5** → **⛔ scope reduction 待签发独立 ADR**(CEP 5 协议族不覆盖 IOMMU 完整协议;跨 vendor 归 ArchForge)。
> - **③ 队列 #7 的载体形态变更** —— `ADR-CEP-19` 决策 5 / 6 落地 `CohIfBundle` + `ProtocolBridge` + `REGISTER_CHSTREAM_BIDIRECTIONAL`,**走 JSON 装配链**(per `ADR-CEP-16` 决策 2 排除 programmatic bridge 的 C++ 路径)—— 与上表 P2.1 的「programmatic bridge 路径」**表述不同**,以 CEP 侧 ADR 为准(per `ADR-CEP-19` Status Update)。

### 15.5 5-stage Pipeline 演进(P4 演进 seam)

per Gem5 Garnet 5-stage + credit-based flow control(`gem5-garnet-network-survey.md` §A 详):

**当前 C++ `RouterTLM`**: 已落地 credit (per `bidirectional_port_adapter.hh:257`(`send_credit`)/ `:269-280`(`send_credit_impl`)) + 简单路由表 + 1-cycle 默认延迟
**缺**: BW/RC/SA/VS/ST/LT 5-stage pipeline 详情(per Gem5 `Router.cc:71-97`)

**P4 路径**: 5-stage 不引入新抽象类, **直接在 RouterTLM 内增量**:
- 新增 `pipeline_phase_` 字段(IDLE/BW_RC/SA/ST/LT)
- `tick()` 内部循环 5 阶段
- 现有 `pending_credits_`(per `bidirectional_port_adapter.hh:148`(声明)/ `:257`(`send_credit`))借位成 5-stage credit bookkeeping

**估时**: 30-45d, `RouterTLM` ~150 行业务代码, 不破 6 标签

### 15.6 大块传输(Bundle + 网络 Bundle 双载)

per §4.3 NoCFlitBundle 16 字段分析 + D-AXI v1.5 B17 AxiMemBundle 4136B(per `AGENTS.md` KEY INVARIANTS "C.3 D-AXI v1.5 B17: AxiMemBundle ~4136B > PacketPool kMinPayloadBytes (256B)" + v1.8 H1):

| 业务 Bundle 大小 | 路径 | 演化阶段 |
|------------------|------|----------|
| **≤ 256B** | `NoCFlitBundle::data` (ch_uint<64>=8B/拍) inline | **已落地** |
| **256B ~ 4KB** | 业务 Bundle 整体走 payload (memcpy), `NoCFlitBundle` 仅 metadata | D-AXI v1.5 已落 `payload_resize()` |
| **> 4KB** | 双 Bundle + descriptor pointer(per Gem5 Garnet §2.3 "data payload: < 8 bytes inline; > 8 bytes 走 dataBlock 指针") | P4+ 演进 |

**实测**: `dgpu_soc_minimal_v1.json` + `[pcie]` 36672 断言下, AxiMemBundle 4KB 通过现有 `payload_resize` 机制可以工作

### 15.7 DisjointNetwork + CDC/SerDes 演进(P5)

per Gem5 `gpufs/DisjointNetwork.py` + HeteroGarnet `NetworkBridge`:

```python
# 06 演进示例 — CPU 侧 mesh + GPU 侧 ring, 通过 CDC bridge 跨域
# v0.3.1 修订: add_module 移到 add_cluster 之前(per soc.py:35-58)
cpu_noc = mesh_cluster(rows=4, cols=4, name_prefix="cpu_noc")
gpu_noc = ring_cluster(n_nodes=8, name_prefix="gpu_noc")
soc.add_module("noc_cdc_bridge", "NetworkBridge",
               src_vc_count=4, dst_vc_count=8, cdc_latency_cycles=2)
soc.add_cluster(cpu_noc).tag("cpu")
soc.add_cluster(gpu_noc).tag("gpu")
soc.connect("group:cpu", "noc_cdc_bridge.src", latency=2)
soc.connect("group:gpu", "noc_cdc_bridge.dst", latency=2)
```

**`NetworkBridge` C++ 端** (P5): 借鉴 Gem5 `NetworkBridge.cc` (HeteroGarnet CDC/SerDes) + ArchForge `CoherenceBridge`

---

## 16. 决策请求(给项目 Owner)

### 16.1 需要批准的 5 个关键决策

1. **方案代号 "06-Inter-Module Network" 是否接受?** 还是有更好的名字(如 "06-Module-Interconnect-Layer")
2. **Bundle 二分类 + 演进粒度约束** 是否接受? 还是再细化(比如加 "持久事务标识" 维度)?
3. **Python `cpptlm.topo` + `cpptlm.library.{mesh_cluster, ring_cluster, SoC}` 作为正文契约** 接受吗? 还是需要把 `cpptlm_config/` legacy 形式共存?
4. **simulation_mode 单一维度, 不引入 network.mode** 接受吗? 还是用户希望 future 可以切具体协议?
5. **coherence 演进 seam 引用 ArchForge, 不在 06 实施 CoherenceProtocol/CoherenceBridge** 接受吗? 还是用户希望 P1 直接 inline 实施(高风险, 06 §12.3 表)?

### 16.2 后续行动

**如果批准**:
- 立即起草 OpenSpec `openspec/changes/2026-10-16-cpptlm-inter-module-network-mvp/proposal.md`(走 K-D doc-only 路径, **不消耗 KPI slot**, 但含 proposal 文件)
- P0 doc-only 任务启动(1 周)
- 06-bis v0.3 草案同期起草
- supersede PNN v0.1 + 05-doc v0.1 标志
- 加 ADR-DGPU-13/14 Status Update 段

**如果不批准**:
- 收集具体反对意见(哪一条不通过)
- 修订本方案 v0.3 → v0.4 → ...
- 重新评审

---

## 17. 关联文档

### 17.1 既有 CppTLM 文档(互补, 不重复)

- [`docs/architecture/01-hybrid-architecture-v2.1.md`](01-hybrid-architecture-v2.1.md) v2.1.10 — 整体 6 层框架 + Gem5 MessageBuffer 类比
- [`docs/architecture/02-transaction-architecture.md`](02-transaction-architecture.md) — 通用事务架构
- [`docs/architecture/02-complex-topology-architecture.md`](02-complex-topology-architecture.md) — 复杂 topology RT 桥接
- [`docs/designs/dgpu-soc/communication-network.md`](../designs/dgpu-soc/communication-network.md) v1.0 — 6 层 + 5-hop + 8 架构债务
- [`docs/designs/dgpu-soc/architecture.md`](../designs/dgpu-soc/architecture.md) — dGPU 应用三视角
- [`cpptlm/AGENTS.md`](../../cpptlm/AGENTS.md) — Python 拓扑工具链
- [`include/tlm/cluster/AGENTS.md`](../../include/tlm/cluster/AGENTS.md) — SimModule vs ChStreamModule 边界

### 17.2 新起草文档

- [`docs/architecture/06-bis-coherence-as-network-extension.md`](06-bis-coherence-as-network-extension.md) v0.3(同期, coherence 演进 seam 详细化 + OpenSpec 草案方向)

### 17.3 被 superseded 的早期草案(保持引用, 正文不动)

- [`docs/designs/dgpu-soc/pnn-protocol-neutral-network.md`](../designs/dgpu-soc/pnn-protocol-neutral-network.md) v0.1
- [`docs/architecture/05-packet-semantic-network.md`](05-packet-semantic-network.md) v0.1
- **`docs/adr/ADR-DGPU-13-pnn-protocol-neutral-network.md`** — v0.3 顶部加 `## Status Update 2026-10: superseded by 06 v0.3` 段
- **`docs/adr/ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md`** — 同上

### 17.4 跨仓 ArchForge 文档(0 改, 06 仅引用)

- `ArchForge/docs/architecture/09-coherence-protocol.md` — v1.0-draft L7 Coherence 子系统架构
- `ArchForge/docs/microarchitecture/coherence-protocol.md` — CoherenceProtocol C++ 接口
- `ArchForge/docs/microarchitecture/coherence-bridge.md` — CoherenceBridge ChStream 接口
- `ArchForge/docs/microarchitecture/coherence-domain.md` — CoherenceDomain per-cache 状态机
- `ArchForge/docs/adr/ADR-SOC-01-coherence-protocol-strategy.md` — Coherence 分步走策略
- `ArchForge/docs/adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md` — v1.0 双 vendor 跨域 coherence

### 17.5 调研综述参考保留(06 v0.3 不重发明)

- [`docs/research/gem5-ruby-protocol-bridge-survey.md`](../../research/gem5-ruby-protocol-bridge-survey.md) — Gem5 Ruby/Garnet 协议中立通信网络调研
- [`docs/research/gem5-messagebuffer-network-interface.md`](../../research/gem5-messagebuffer-network-interface.md) — MessageBuffer + NI + vnet 详解
- [`docs/research/gem5-garnet-network-survey.md`](../../research/gem5-garnet-network-survey.md) — Garnet 3.0 router + Topology
- [`docs/research/gem5-soc-survey.md`](../../research/gem5-soc-survey.md) — Gem5 标准库 topology 例子
- 【ArchForge 仓】`docs/research/PCIe/PCIe_上的保序write.md` — PCIe TLP 保序（2026-10-27 自本仓 `docs/research/PCIe/` 迁出）

### 17.6 相关 ADRs(0 改, 仅引用; 详情见 §10.1)

- ADR-DGPU-06 chip-internal AXI vs board-level PCIe 边界(06 兼容, 不破)
- ADR-DGPU-08 23 ABI 冻结 + 0 新增(06 兼容, 不破)
- ADR-DGPU-09 Driver-Visible Minimal SoC 范围(06 不影响 driver-visible 入口)
- ADR-DGPU-11 timing-mode SoC scope(06 simulation_mode 沿用)
- ADR-DGPU-12 SoC 内部 TLM 接线原则(06 不动既有 JSON 拓扑)
- ADR-DGPU-13 PNN 范围定义 ADR(06 v0.3 **superseded** 标 §10.1 + §10.2 Status Update 段)
- ADR-DGPU-14 Stream Adapter Protocol Bridge Seam 决策 ADR(同上)
- ADR-X.13 ADR 不可变 + Status Update 先例(06 沿用)

### 17.7 相关 OpenSpec change(已存在, 06 关系明示)

- `openspec/changes/cpptlm-dgpu-soc-control-hub/` Proposed Draft(BAR decode + ControlHub — 06 R12 声明豁免条款, programmatic bridge 永久保留 CoherenceBridge 路径)
- `openspec/changes/cpptlm-dgpu-soc-physical-ip-port-boundary/` Proposed Draft(SDMA 5→8 端口切型 — 06 R12 同上)
- `openspec/changes/cpptlm-dgpu-soc-internal-noc-e1/` v1.1 archived(DataNoC only e1 实施 — 06 §6 引用)
- `openspec/changes/cpptlm-lwc-bus-hub-mvp/` archived(同步路由 hub — 06 §3.2 引用)
- `openspec/changes/2026-10-16-cpptlm-inter-module-network-mvp/`(待起草, KPI 路径 K-D)
- `openspec/changes/2026-10-16-cpptlm-coherence-extension/`(待起草, KPI 路径 K-D)

---

## 18. 修订记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-10-16 | v1 (replaced) | build agent (architect) | 初版, 8 BLOCKER + 10 SHOULD_FIX + NIT |
| 2026-10-17 | v0.2 (replaced) | build agent (architect) | 修 v1 BLOCKER, 引入新问题, Metis 6 BLOCKER 未触 |
| 2026-10-18 | v0.3 (replaced) | build agent (architect) | 3 轮评审 22 项修订 + KPI 口径修正 + 6-bis 同步 |
| 2026-10-18 | **v0.4 (本版)** | build agent (architect) | **修 v0.3 BLOCKER B-1 (Oracle + build agent 联合审稿) + 2 SHOULD_FIX + 5 NITS + docs/architecture/ 重新规划 (4 归档 + 2 迁出) + 收窄 P1 范围方案 A** |

### 18.1 v0.3 修订明细 (vs v0.2)

| # | 来源 | v0.2 bug | v0.3 修订位置 |
|---|------|--------|--------------|
| B-α | Oracle B5 + Critic B-α | §2.1 ASCII 图层号 +1 位移 | §2.1 重画 0-8 canonical 编号 |
| B-β | Critic B-β | §11.2 `add_scalar` API 错 | §11.2 改 `addScalar(name, desc, unit)` + `+=` 累加写入 |
| B-γ | Critic B-γ | 48 处 "沿用 v1" 占位 | **全文重写, 0 占位** |
| B-δ | Oracle Q-δ | §7.7 "累计 6-7 slots" 口径错 | §11.5 修正 KPI 口径(串行激活, 并发 ≤ 1) |
| B-ε | Oracle Q-β | SnoopResp struct 命名冲突 | 06-bis v0.3 §5 改 SnoopRespBundle |
| B-ζ | Oracle Q-α | programmatic bridge 与 active Draft 冲突 | 06-bis v0.3 §3.4 + 06 R12 声明豁免条款 |
| B-η | Oracle Q-δ | §3.4 "永久放弃" vs §7.4 P3.5 自相矛盾 | 06-bis v0.3 §3.4 选选项 C, 修订"永久放弃"为"条件放弃" |
| B-θ | Oracle Q-ε | §6.2 示例 `add_module` 落子层 | §6.3 修订示例: add_module 放在 add_cluster 之前 |
| B-ι | Oracle Q-γ | DomainRegistry 生命周期未管理 | 06-bis v0.3 §4.2 P2.1 任务加 |
| B-κ | Oracle Q-β | 协议标识 4 处拼写不一致 | 06-bis v0.3 §5 单一真源 |
| B-λ | Oracle Q-γ | `add_member` API 错 | 06-bis v0.3 §4.2 改 `set_members(const vector<string>&)` |
| B-μ | Oracle Q-β | ArchForge `coherence-protocol.md:177-184` 实际 `std::string` | 06-bis v0.3 §5.2 类型改写 `ch_uint<8>` |
| B-ν | Oracle Q-ε | `SoC._root` 私有成员访问 | 06-bis v0.3 §6.2 加 `SoC.add_coherence_domain` passthrough |
| B-ξ | Oracle Q-ε | 递归提升后重名策略 | 06-bis v0.3 §4.1 P2 加 dedup 报错策略 |
| B-ο | Oracle Q-ε | emitter canonical 声明 | 06-bis v0.3 §4.1 P2 加 emitter 唯一出口声明 |
| B-π | critic NIT | "TopOrchestrator" typo | §7.1 改 TopoOrchestrator |
| B-ρ | critic NIT | `n_ports=2` CrossbarTLM 无效 | §6.3 示例删 n_ports, 默认 4 |
| B-σ | critic NIT | `num_ports()` "C++ 端零调用" 表述 | §14.3 限定"生产路径零调用" |
| B-τ | critic NIT | ADR-X.13 占位符笔误 | §10.1 引用具体 ADR |
| B-υ | critic NIT | "ADR-DGPU-13/14 Status Update" 对未签发 ADR 适用性 | §10.1 注"🟡 起草中, 加 Status Update 段" |
| B-φ | critic SHOULD-FIX | §5.3 过度承诺 functional/timing 网络级效果 | §5.3 加 "当前仅 VRAM 路径落地" |
| B-χ | Metis | CrBundle 字段含义 | §3.2 改 APB-lite LWC CR-Bus |
| B-ψ | Metis | Ax4Bundle 消费者 | §3.2 标 ≥5 |
| B-ω | Metis | ArchForge 状态夸大 | §10.1 + §15.1 改正为 🟡 草案 |
| **B-ω-2** | Metis | `module_factory.cc:165-182` stub | 06-bis v0.3 §4.2 承认 |

### 18.2 v0.3 仍未修订(待你定夺)

| # | 项 | 我的判断 | 你决定? |
|---|----|----------|---------|
| 1 | ArchForge 跨仓 markdown 镜像路径(`file:///workspace/main/ArchForge/...`)在仓内不可验证 | 降级为"ArchForge 设计参考, 实现在 CppTLM 仓" | — |
| 2 | `cpptlm_config` deprecated 处置由既有 OpenSpec change 管 | 06 不主动修 | — |
| 3 | KPI slot 实际可释放时间表依赖 archive 决策 | 06 P0 doc-only 立即可启动, P1+ 等 slot | — |

### 18.3 v0.3 评审通过后下一步

1. 起草 `openspec/changes/2026-10-16-cpptlm-inter-module-network-mvp/proposal.md`(K-D doc-only 路径, 不消耗 KPI)
2. P0 任务启动(1 周)
3. 同步 supersede Status Update 段到 PNN / 05-doc / ADR-DGPU-13 / ADR-DGPU-14
4. 实施 v0.3 §11.2 P1 任务(等 KPI slot 释放)

---

## 附录 · 行号对照表(2026-10-28 · per 台账 Entry #86)

⚠️ **本表登记 `module_factory.hh` / `chstream_module.hh` 的行号漂移** —— 队列 #7 Phase 3
给这两个头文件加了内容(注册表签名扩展 + 2 个私有成员 + 1 个可选虚函数)⇒ **既有**文档
与 ADR 里的行号引用**语义漂移**(文件本身未坏,符号仍在,只是行号变了)。

| 符号 | 旧行号(2026-10-27 实测) | 新行号(2026-10-28) | 漂移 |
|---|---|---|---|
| `module_factory.hh` `getParamRulesRegistry` | 81 | **87** | +6 |
| `module_factory.hh` `registerObject<T>` | 122 | **128** | +6 |
| `module_factory.hh` `registerModule<T>` 的 `static_assert` | 135 | **141** | +6 |
| `module_factory.hh` `validate_domain_boundary` | 307 | **313** | +6 |
| `module_factory.hh` `register_protocol_bridge` | 313-315 | **336-343** | +23(签名已扩展) |
| `module_factory.hh` `get_bridges` | 327(**`private:`**) | **346**(**`public:`**) | +19(可见性已变) |
| `chstream_module.hh` `num_ports` | 54 | **55** | +1 |
| `chstream_module.hh` `advance_cycle` | 73 | **74** | +1 |

**漂移原因**:`module_factory.hh` 加了 ① `#include "framework/protocol_bridge.hh"`
② `#include <unordered_set>` ③ 2 个私有成员(`protocol_bridges_` / `consumed_bridges_`)
④ public 段的新增块 ⇒ 既有行号整体 **+6**;`chstream_module.hh` 加了 ① 前向声明
② `attach_protocol_bridge` 虚函数 ⇒ 既有行号整体 **+1**。

⚠️ **一次踩坑记录(可复用的经验)**:`module_factory.hh` 曾尝试用**前向声明**代替 include
(`namespace cpptlm { class ProtocolBridgeEr; }`)以缩小行号漂移 ⇒ **编译失败**
(`invalid application of 'sizeof' to incomplete type`):`std::vector<std::unique_ptr<T>>`
的析构在**头文件里内联** ⇒ 任何 include 本头的 TU 都会实例化 `unique_ptr<T>` 的析构
⇒ **需要 `T` 完整**。⇒ 改用**自定义 deleter** 又会把漂移从 +6 放大到 +27 ⇒ 最终
**直接 include**(净漂移最小,且 `core/chstream_port.hh` 本就拉进 `core/packet.hh`
⇒ **无新增 header 依赖**)。

⚠️ **ADR 不可变** ⇒ 冻结面 ADR(`ADR-CEP-00` / `07` / `07A` / `19`)与 `07 号` 的**正文**行号
引用**不改**,行号漂移以各自的 `## Status Update` 段登记。

---

## 附录 · 行号对照表(2026-10-28 · per 台账 Entry #88 —— 队列 #7 Phase 5)

⚠️ **本表登记 Phase 5(JSON 装配连通性)给 4 个头文件带来的行号漂移** —— Phase 5 在
`chstream_register.hh` **前部**插入了「共享注册 helper + 2 个装配宏」,在
`REGISTER_CHSTREAM` 宏体**内部**插入了「CEP PE 注册 + 协议桥注册」,并给
`bidirectional_port_adapter.hh` 加了 `EgressChannel` 枚举。

| 符号 | 旧行号(Entry #87 时实测) | 新行号(2026-10-28) | 漂移 |
|---|---|---|---|
| `chstream_register.hh` `#define REGISTER_CHSTREAM` | 50 | **107** | +57 |
| `chstream_register.hh` `registerObject<tlm::RouterTLM>` | 62 | **119** | +57 |
| `chstream_register.hh` `registerObject<tlm::NICTLM>` | 63 | **120** | +57 |
| `chstream_register.hh` `registerBidirectionalPortAdapter<tlm::RouterTLM>` | 119 | **188** | +69 |
| `chstream_register.hh` `registerDualPortAdapter<tlm::NICTLM>` | 121 | **190** | +69 |
| `chstream_register.hh` `#define REGISTER_CHSTREAM_DUAL` | 175 | **244** | +69 |
| `chstream_register.hh` `REGISTER_CHSTREAM_DUAL` 宏内的 `registerDualPortAdapter` | 177 | **246** | +69 |
| `bidirectional_port_adapter.hh` `CATEGORY_CREDIT` 写入点 | 109 | **133** | +24 |
| `bidirectional_port_adapter.hh` `pending_credits_` | 148 | **174** | +26 |
| `bidirectional_port_adapter.hh` `bind_port_pair` | 171 | **197** | +26 |
| `bidirectional_port_adapter.hh` `bind_ports`(**空操作 shim**) | 184 | **210** | +26 |
| `bidirectional_port_adapter.hh` `receive_credit` 调用点 | 225 | **260** | +35 |
| `bidirectional_port_adapter.hh` `send_credit` | 257 | **292** | +35 |
| `bidirectional_port_adapter.hh` `send_credit_impl` | 269 | **304** | +35 |
| `module_factory.hh` `getParamRulesRegistry` / `registerObject<T>` / `static_assert` / `validate_domain_boundary` / `register_protocol_bridge` / `get_bridges` | 87 / 128 / 141 / 313 / 336-343 / 346 | **不变** | 0 |
| `module_factory.hh` `getInstanceForTesting`(Phase 5 新增,`CPPTLM_TESTING` 段) | — | **395** | 新增 |

**漂移原因**:
- `chstream_register.hh` **+57**(`REGISTER_CHSTREAM` 之前):新增「共享注册 helper」
  (`cpptlm::register_chstream_bidirectional`)+ 2 个装配宏(`REGISTER_CHSTREAM_BIDIRECTIONAL`
  / `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED`)。
- `chstream_register.hh` **再 +12**(`REGISTER_CHSTREAM` 宏体**内部**):新增「CEP PE 注册
  (`CohIfProtocolEngine`)+ 协议桥注册(`cohif_noc`)」⇒ 该宏体**之后**的行号再 +12 ⇒
  合计 +69。
- `bidirectional_port_adapter.hh` **+24 / +26 / +35**:新增 `EgressChannel` 枚举(12 行)
  + 2 处 `egress_channel()` 默认实现 + 注释 ⇒ 分段漂移。
- `module_factory.hh` **0**:`getInstanceForTesting` 加在类**末尾**(`addInstanceForTesting`
  之后)⇒ 其**之前**的行号不变。

⚠️ **一处「零漂移」结论已失效**(诚实登记):Entry #87 的第七则登记过「`chstream_register.hh`
的既有行号(**50 / 119 / 121 / 175 / 177**)**零漂移**」⇒ 那是**当时**的实测口径
(Phase 4 把新宏插在 `REGISTER_CHSTREAM_DUAL` **之后**);Phase 5 把 helper + 2 个宏插到
`REGISTER_CHSTREAM` **之前** ⇒ **该结论已过期**,现以上表为准。

---

**版本**: v0.4 Revision
**最后更新**: 2026-10-19 (v0.4 修订, per critic 评审 B3 BLOCKER 修复)
**批准状态**: 🟡 待逐项评审 (Oracle + Metis + Critic 3 轮已给修订判断, 等你最终 B 路径)
**维护者**: CppTLM build agent (architect mode)
**下次更新**: 你最终 B 路径反馈后 → v1.0 PASS
