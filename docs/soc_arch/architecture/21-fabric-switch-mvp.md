# Fabric + Scale-Up Switch v1.0 MVP 详细设计 (Interconnect Architecture & Protocol + Scale-Up Switch — Minimum Viable Product)

> **目的**: 定义 CppTLM dGPU SoC **MAS-3.1** 中 **Fabric 协议层** + **Scale-Up Switch** 的 v1.0 MVP 微架构——作为 dGPU 内部 Die 间 (GPC↔HBM/NIC/IO) 与跨节点 (GPU↔Switch↔Peer GPU/CXL Memory) 的**互联契约层**，与上游 [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) (TEE-UDD 数据面) 和 [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) (Backends 后端物理实现) 通过标准 160-bit UDD Micro-op + UALink Flit 接口解耦。v1.0 MVP 仅交付 **UALink 1.1 Mem 协议 + 基础 Switch PTE**；PCIe/CXL.io 协议在 v1.1+ 引入（与 backends v1.1 IO-DMA 协同）。
>
> **状态**: Draft v1.0 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: 待 `openspec/changes/2026-09-19-cpptlm-mas-fabric-switch-mvp/` 提案对齐
> **关联文档**:
> - [`21-fabric-switch-evolution-roadmap.md`](21-fabric-switch-evolution-roadmap.md) — Fabric+Switch 5 阶段演进路线图 (v1.0/v1.1/v2.0/v2.1/v3.0)
> - [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) — TEE + UDD MVP (本模块上游, 通过 UDD Micro-op 接口对接)
> - [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) — Backends MVP (本模块下游, 通过 UALink Flit 对接)
> - [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) — Core/Micro-Arch + RTL-IFC (微架构/接口)
> - [`21-soc-topology-mvp.md`](21-soc-topology-mvp.md) — SoC 顶层物理布局规范 (V3.1-Rev2.0 拓扑修正: CXL Memory Pool 在外部 Switch 下, 不在 GPU Die)
> - [`16-pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT
> **关联 ADR**:
> - ADR-088 §D5 — 23 ABI 冻结 (Fabric+Switch v1.0 不动 ABI)
> - ADR-SOC-10 (待起草) — TEE/UDD/UBC 三层解耦 + Switch 协议契约 (本模块边界)
> - ADR-SOC-21 — **V3.1-Rev2.0 拓扑修正** (CXL 在外部 Switch 下 / HRT 无 CXL_DIRECT / REMOTE_CXL_POISON_VIA_SWITCH)

---

## §1 概述

### §1.1 Fabric + Switch 层在 dGPU SoC 中的位置

Fabric + Switch 层是 dGPU SoC **互联契约层**，定义从 GPC 内部到外部 CXL Memory Pool 的全链路协议语义、流控机制与跨域转换规则：

```
┌─────────────────────────────────────────────────────────────────┐
│                       GPU 内部 (dGPU Die)                        │
│                                                                 │
│  ┌─────────────────────────────────────────────────────┐        │
│  │  [GPC × 8] (SM + TC-DMA + UDD Agent + TEE Frontend) │        │
│  └────────────────────────┬────────────────────────────┘        │
│                           │                                      │
│                           ▼                                      │
│  ╔═══════════════════════════════════════════════════════════╗  │
│  ║   Compute NoC + Memory NoC (Die 内 NoC, 不在本模块)      ║  │
│  ╚═══════════════════════════════════════════════════════════╝  │
│                           │                                      │
│                           ▼                                      │
│  ╔═══════════════════════════════════════════════════════════╗  │
│  ║   ★ Fabric 协议层 (本模块: Die 内 + 跨节点)              ║  │
│  ║                                                           ║  │
│  ║  - L4 Application: TMA / GDS / P2P                      ║  │
│  ║  - L3 Transport: UDD Micro-op (160-bit, 与 tee-udd 对接)║  │
│  ║  - L2 Network: Compute NoC / Mem NoC                     ║  │
│  ║  - L1 Link: UALink 1.1 Mem (与 backends NIC-DMA 对接)   ║  │
│  ╚═══════════════════════════════════════════════════════════╝  │
│                           │                                      │
└───────────────────────────┼──────────────────────────────────────┘
                            │ UALink Flit (256B)
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│   ★ Scale-Up Switch (本模块: 外部设备, GPU Die 边界外)          │
│                                                                 │
│  - N 个 UALink x16 端口 (GPU 侧)                                │
│  - M 个 CXL 3.0 x16 端口 (Memory 侧)                            │
│  - Protocol Translation Engine (PTE) (UALink ↔ CXL.mem)        │
│  - Crossbar (Flit-level Non-blocking)                            │
│  - Sideband/Management Port (SMBus/I2C)                         │
│                                                                 │
└────────────┬─────────────────────┬──────────────────────────────┘
             │ CXL.mem              │ UALink Flit
             ▼                     ▼
    [CXL Memory Pool]      [Peer GPU HBM]
```

### §1.2 为什么需要 Fabric + Switch 分层 (替代 V3.0 直连 PCIe)

| 维度 | V3.0 直连 PCIe | Fabric + Switch v1.0 MVP (新) |
|------|------------------|--------------------------------|
| **协议栈** | GPU 直连 PCIe/CXL.mem, 协议栈复杂 | GPU 仅处理 UALink 1.1 Mem 子集, CXL 协议在 Switch |
| **链路协议** | 单一 PCIe (片内 PCIe Controller) | UALink (Scale-Up) + PCIe (Host IO) 分离 |
| **GPU Die 面积** | 需含完整 PCIe + CXL Controller | 仅含 UALink Flit Packager + Drain FSM, Die 面积节省 3-5% |
| **验证复杂度** | PCIe + CXL 联调 | UALink 单协议 + Switch PTE (CXL 黑盒) 联调 |
| **互操作性** | 与单一 PCIe/CXL 厂商绑定 | UALink 1.1 CTS 通用, 多 CXL 厂商可选 |
| **演进路径** | ❌ 加新协议必重构 GPU | ✅ Switch PTE 仅替换 CXL Backend, GPU 不变 |

**核心设计原则** (per MAS-3.1-Fabric §1):
> **GPU Die 内不存在 CXL.mem/CXL.cache 协议状态机**
> **所有跨节点访存均通过 UALink 传输, CXL 协议转换完全由外部 Scale-Up Switch 黑盒完成**
> **GPU 协议栈极简 = UALink 1.1 Mem 子集**

> **⚠️ V3.1-Rev2.0 拓扑修正 (per `21-soc-topology-mvp.md` §1.1)**:
> **GPU Die 上无 CXL PHY/Controller**——CXL Memory Pool 是挂在外部 Scale-Up Switch 下方的独立设备 (不在 GPU Die 上)。GPU 对 CXL 内存的访问本质上是"Over-the-Fabric"的远程访存:
> - HRT 中**没有** `CXL_DIRECT` Route_Tag, 仅有 `SWITCH_PORT_N` (per `21-soc-topology-mvp.md` §4.1)
> - CXL 地址空间在 HRT 中表现为"Scale-Up 域", 由 NIC-DMA 封装为 UALink Flit → 外部 Switch → Switch PTE 转换为 CXL.mem
> - 详见 `21-soc-topology-mvp.md` §1.2 (SoC 顶层拓扑图) 与 §3.2 (CXL 访存路径)

**SoC 拓扑角色定位** (per `21-soc-topology-mvp.md` §2.5):
- **GPU Die 内**: NIC-DMA (UALink Flit Packager + Drain FSM) **是 GPU 唯一与外部 Scale-Up Switch 交互的接口**
- **外部 Scale-Up Switch (非 GPU Die)**: 双重角色 = ① UALink Fabric (GPU-to-GPU 路由) + ② CXL Expander Controller (UALink → CXL.mem 协议转换)
- **外部 CXL Memory Pool (非 GPU Die)**: 通过 Scale-Up Switch 的 CXL 端口下挂, 与 GPU 完全解耦
- **HRT 中 GUPA 高位 [63:48] 落在 Scale-Up 域** (`0x1000 ~ 0x7FFF`) → HRT Route_Tag = `0x1~0xE` (NIC-DMA Port N) → NIC-DMA 封装 UALink Flit → Switch 决定目标是 Peer GPU HBM 还是 CXL Memory Pool

### §1.3 与 [TEE-UDD / Backends / MicroArch-IFC / SoC-Topology] 文档的边界

| 子系统 | 文档 | 与本模块边界 |
|--------|------|---------------|
| **TEE + UDD** | [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) | 上游: 通过 160-bit UDD Micro-op 与本模块的 L3 Transport 对接 |
| **Backends** | [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) | 下游: NIC-DMA 通过本模块定义的 UALink Flit 与 Switch 对接 |
| **MicroArch + RTL-IFC** | [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) | 实现层: 时钟域 / 流水线 / 信号定义 / CSR |
| **SoC Topology** | [`21-soc-topology-mvp.md`](21-soc-topology-mvp.md) | 顶层契约: CXL Memory Pool 物理位置归属 / TC-DMA 归属 / HRT Route_Tag 分配 (V3.1-Rev2.0 修正) |

---

## §2 Fabric + Switch v1.0 MVP 端到端 Shippable Demo

v1.0 MVP 必须通过的"绿灯测试"，证明 **GPU ↔ Scale-Up Switch ↔ Peer GPU HBM** 链路贯通（v1.0 仅 UALink Mem 协议）：

```
┌──────────────────────────────────────────────────────────────────────┐
│  v1.0 MVP Test: test_fabric_switch_p2p_e2e                          │
├──────────────────────────────────────────────────────────────────────┤
│  Step 1: GPU 0 发起 Load (Peer GPU 1 HBM)                            │
│            - SM 发起 VA → TEE → GMMU → GUPA=0x1000_0000_0000        │
│              (高位 0x1000 = Scale-Up Domain, per HRT Route_Tag=1)     │
│            - HRT 查表 → Route_Tag=1 (UALink Port 0)                 │
│            - AGU 拆分 → 128B Micro-op                                │
│            - UDD Agent 仲裁 → NoC Injection                          │
│                                                                       │
│  Step 2: Global UDD Hub 路由                                          │
│            - RCT 校验通过 (ctx=0)                                    │
│            - Credit[backend=NIC-DMA Port 0] = 128 → 127              │
│            - Micro-op 注入 backend_out[NIC-DMA Port 0]                │
│                                                                       │
│  Step 3: NIC-DMA (GPU 0) UALink Flit 封装                            │
│            - pack_microop_to_flit(muop)                              │
│              → flit.header = {Valid=1, SeqNum=0, SrcNode=0,          │
│                                DstNode=1, VC=0, MsgType=MEM_RD_REQ} │
│              → flit.addr_tag_status.GUPA = 0x1000_0000_0000         │
│            - 注入 UALink SerDes PHY → Switch                         │
│                                                                       │
│  Step 4: Switch UALink Port 0 接收 Flit                              │
│            - UALink MAC/PHY 解封装 (256B Flit)                       │
│            - Crossbar 路由决策 (per PTE Base/Limit 寄存器)            │
│              → GUPA 在 [Base=0x1000_0000_0000, Limit=0x1FFF_FFFF_FFFF] │
│              → 目标 = UALink Port 1 (GPU 1 NIC-DMA)                   │
│              → **P2P 路由决策**: Target 在 Peer GPU HBM, 不经过 PTE  │
│                                                                       │
│  Step 5: Switch Crossbar Flit 转发 (Port 0 → Port 1)                 │
│            - Non-blocking Crossbar (Flit-level)                      │
│            - 转发至 UALink Port 1 → GPU 1 NIC-DMA                    │
│                                                                       │
│  Step 6: NIC-DMA (GPU 1) 接收 Flit + P2P Bypass                     │
│            - parse_response_flit: DstNode=1 = 本节点                  │
│            - 解码 flit.addr_tag_status.GUPA → 落在 Local HBM (per HRT)│
│            - 触发 p2p_bypass_active_ = true                           │
│            - 直接通过内部旁路总线写入 HBM-DMA Write Queue             │
│            - **不经过** UBC + Memory NoC (降低 15% P2P 延迟)         │
│                                                                       │
│  Step 7: HBM-DMA (GPU 1) 处理                                        │
│            - FR-FCFS 调度 + HBM3e PHY Command                        │
│            - 数据返回 → NIC-DMA (GPU 1) 封装 UALink Response Flit    │
│            - flit.header.MsgType = MEM_RESP_D                          │
│            - flit.payload = 128B data + Status                       │
│                                                                       │
│  Step 8: NIC-DMA (GPU 1) → Switch → GPU 0                            │
│            - Switch Crossbar 转发 (Port 1 → Port 0)                   │
│            - GPU 0 NIC-DMA 接收 Response Flit                         │
│            - Tracker_ID 匹配 → 释放 ROB Entry                          │
│            - 组装 UddResponse → 注入 NoC 返回 GPC                     │
│            - Credit[backend=NIC-DMA Port 0] = 127 → 128 (归还)         │
│                                                                       │
│  Step 9: TEE Frontend (GPU 0) 接收 Response + 写回 SM Register File │
│                                                                       │
│  Step 10: 验证                                                        │
│             - SM 读到 expected value (来自 GPU 1 HBM)                  │
│             - P2P Bypass latency 比常规路径低 ~45%                    │
│             - ROB Tracker_ID 复用正确                                 │
│             - Switch Crossbar Non-blocking (Port 0 + Port 1 并行)     │
└──────────────────────────────────────────────────────────────────────┘
```

**验收标准**: 10 步全部通过 + 测试用例 ≥ 12 个 (见 §12 测试覆盖)。

---

## §3 协议栈分层架构 (per MAS-3.1-Fabric §1)

### §3.1 层级定义

| 层级 | 协议名称 | 作用域 | v1.0 MVP 实现 | 关键特性 |
|------|----------|--------|---------------|----------|
| **L4: Application** | TMA / GDS / P2P | SM, TC-DMA, IO-DMA | ❌ 推迟到 v1.1+ | 面向 Tensor / 存储零拷贝 / Peer-to-Peer 高层语义 |
| **L3: Transport** | **UDD Micro-op** | GPC ↔ Backend | ✅ v1.0 | 统一访存抽象 (160-bit), 携带 Route_Tag + GUPA |
| **L2: Network** | Compute NoC / Mem NoC | Die 内 | ✅ v1.0 (在 MicroArch 文档) | Credit-based Flit 路由, VC 隔离 |
| **L1: Link (Scale-Up)** | **UALink 1.1 Mem** | GPU NIC-DMA ↔ Switch | ✅ v1.0 | 256B Flit, 低延迟, 支持 Remote_Error |
| **L1: Link (IO)** | PCIe Gen6 / CXL.io | IO-DMA ↔ Host | ❌ 推迟到 v1.1 (backends 协同) | TLP 封装, ATS/PRI 支持 |
| **L0: Physical** | SerDes / PHY | 所有外部接口 | ✅ v1.0 (模拟 PHY) | 电气层, LTSSM, 链路训练 |

### §3.2 v1.0 MVP 严格范围

> **v1.0 MVP 仅实现 L1 (UALink 1.1 Mem) + L3 (UDD Micro-op), L2 (Die 内 NoC) 在 [MicroArch 文档](21-microarch-ifc-mvp.md) 实现, L4 在后续阶段。**
> **L1 (PCIe/CXL.io) v1.0 不实现, 推迟到 v1.1+ (per backends evolution-roadmap §4.1)**

---

## §4 L3 Transport: UDD Micro-op 协议

### §4.1 Micro-op 格式 (160-bit Payload, per MAS-3.1-Fabric §2.1)

```cpp
struct UddMicroOp {
    uint32_t header;         // [159:128] Opcode, Size, VC_ID, QoS_Class, Context_ID
    uint64_t gupa;           // [127:064] GUPA (Global Unified Physical Address)
    uint32_t payload_meta;   // [63:032]  Write Data / Byte Enable / Atomic Operand
    uint16_t tracker_id;     // [31:016] Tracker_ID (16-bit, 1024 entry ROB)
    uint16_t seq_route;      // [15:000] Sequence_Num + Route_Tag (隐式包含在 VC 路由中)
};
```

### §4.2 Header 字段位分配 (v1.0 MVP)

```
header (32 bits):
[31:24] Reserved        (演进扩展位, per tee-udd-evolution-roadmap §5.1)
[23:20] Route_Tag       (4 bits: 0:HBM, 1~E:UALink, F:PCIe)
[19:16] QoS_Class       (4 bits: 0=Highest, F=Lowest)
[15:12] VC_ID           (4 bits: 0=Control, 1=Read, 2=Write, 3=Response)
[11:08] Context_ID      (4 bits, v1.0 仅 0 启用)
[07:06] Size            (2 bits: 0=64B, 1=128B, 2=256B, 3=512B)
[05:04] Opcode          (2 bits: 0=Read, 1=Write, 2=Atomic, 3=Ctrl)
[03:00] Flags           (4 bits: Bypass_L2, Fence_Scope, ...)
```

### §4.3 UDD Response 格式 (64-bit, per MAS-3.1-Fabric §2)

```cpp
struct UddResponse {
    uint16_t tracker_id;     // [63:48] Tracker_ID (匹配原始请求)
    uint16_t status;         // [47:32] Status: 0x0=Success, 0x1=Remote_Poison, 0x2=Local_Fault, 0x3=Page_Fault
    uint32_t data_meta;      // [31:00] Read Return Data (小负载) / Timestamp
};
```

### §4.4 Route_Tag 语义 (per MAS-3.1-Fabric §2.2 + V3.1-Rev2.0 拓扑修正)

| Route_Tag | 目标 Backend | 后续协议转换 | v1.0 MVP |
|-----------|--------------|---------------|----------|
| `0x0` | HBM-DMA | 直接转换为 HBM3e Command | ✅ |
| `0x1 ~ 0xE` | NIC-DMA (Port N) | 封装为 UALink Mem Flit → Scale-Up Switch (内部 PTE 决定目标是 Peer GPU HBM 或 CXL Memory Pool) | ✅ |
| `0xF` | IO-DMA | 封装为 PCIe TLP | ❌ v1.1+ |

**v1.0 MVP 仅启用 Route_Tag=0 (HBM) + 1~4 (UALink Port 0~3)**，5~E 预留 v1.1+，F (PCIe) 推迟 v1.1+。

> **⚠️ V3.1-Rev2.0 拓扑修正 (per `21-soc-topology-mvp.md` §4.1)**:
> **HRT 中没有 `CXL_DIRECT` Route_Tag**——CXL Memory Pool 访问统一通过 `0x1 ~ 0xE` (NIC-DMA Port N) 路由。GPU 侧的 HRT 仅告诉 UDD "这个 GUPA 前缀应该发给哪个 Switch Port"，具体目标是 Peer GPU HBM 还是 CXL Memory Pool **完全由外部 Scale-Up Switch 的 PTE 路由表决定**。
>
> 例: 访问 CXL Memory Pool X 的 GUPA `0x1500_0000_0000` (高位 0x1500 落在 Scale-Up 域)
> → HRT 查表 → Route_Tag = `0x1` (NIC-DMA Port 0)
> → NIC-DMA 封装为 UALink Mem-Read Flit → 发往 Scale-Up Switch
> → Scale-Up Switch PTE 根据 Switch 路由表判断 → 转换为 CXL.mem Read → 发往 CXL Memory Pool X
> → GPU 不感知目标是 Peer GPU HBM 还是 CXL Memory Pool

### §4.5 流控与背压 (per MAS-3.1-Fabric §2.3)

```
v1.0 MVP Credit 配置:
- GPC → Hub 通路: 64 Credits per GPC
- Hub → Backend 通路: 128 Credits per Backend
- 1 Credit = 1 Micro-op (160b)

背压传播:
- Backend (如 NIC-DMA) 因 UALink 链路拥塞或 Drain 阻塞
- 消耗完 Hub → Backend Credit
- Hub 停止向该 Backend 调度, 通过 NoC 反压上游 GPC
- 不阻塞其他 Backend (Head-of-Line Blocking 避免)
```

---

## §5 L1 Link (UALink 1.1 Mem 协议)

### §5.1 UALink Flit 格式 (256B, per MAS-3.1-Fabric §3.1)

```cpp
struct UALinkFlit {
    uint32_t header;          // [2047:2016] Valid, SeqNum, SrcNode, DstNode, VC, MsgType
    uint32_t addr_tag_status; // [2015:1984] Address / Tag / Status
    uint8_t  payload[1984/8]; // [1983:0]    Payload (256B)
};
// 注: 实际数据 payload = 256B - 8B header = 248B
```

### §5.2 Header 字段位分配 (v1.0 MVP)

```
header (32 bits):
[31]    Valid
[30:24] SeqNum (7 bits, Sliding Window WINDOW_SIZE=64)
[23:20] SrcNode (4 bits, GPU 0~15)
[19:16] DstNode (4 bits, GPU/Switch/CXL)
[15:14] VC (2 bits: 0=Control, 1=Read, 2=Write, 3=Response)
[13:08] MsgType (6 bits, per §5.3)
[07:00] Reserved

addr_tag_status (32 bits):
[31:16] Tag/Status
  - For Request: Tag = Tracker_ID
  - For Response: Status = Remote_Error(0x3), Success(0x0), Poison(0x2)
[15:00] Reserved
```

### §5.3 支持的 Message Type (per MAS-3.1-Fabric §3.2)

| MsgType | 方向 | 描述 | CXL 映射 (Switch 侧) | v1.0 MVP |
|---------|------|------|------------------------|----------|
| `MEM_RD_REQ` | GPU → SW | 读请求 (64/128/256B) | `CXL.mem Read` | ✅ |
| `MEM_WR_REQ` | GPU → SW | 写请求 (含 BE) | `CXL.mem Write` | ✅ |
| `MEM_RESP_D` | SW → GPU | 带数据响应 | `CXL.mem Completion w/ Data` | ✅ |
| `MEM_RESP_ND` | SW → GPU | 无数据响应 (Write Ack) | `CXL.mem Completion No Data` | ✅ |
| `CTRL_QUIESCE` | GPU → SW | Drain 阶段1: 请求排空 | Switch FSM Trigger | ✅ |
| `CTRL_Q_ACK` | SW → GPU | Drain 阶段2: 确认排空 | Switch FSM Response | ✅ |
| `CTRL_RESUME` | GPU → SW | Drain 完成: 恢复流量 | Switch FSM Reset | ✅ |

### §5.4 虚拟通道 (VC, per MAS-3.1-Fabric §3.3)

| VC ID | 用途 | 优先级 | v1.0 MVP |
|-------|------|--------|----------|
| VC0 | Control / Drain | Highest | ✅ 永不阻塞 |
| VC1 | Read Request | High | ✅ |
| VC2 | Write Request | Medium | ✅ |
| VC3 | Response | Low | ✅ |

**v1.0 必须严格遵守 4 VC 隔离**: VC0 (Drain 命令) 独立于数据 VC, 永不阻塞, 防止 Drain 期间数据 VC 反压导致 Drain 死锁。

---

## §6 两阶段 Drain 协议时序

### §6.1 正常 Drain 序列 (per MAS-3.1-Fabric §4.1)

```text
GPU NIC-DMA                              Scale-Up Switch
    │                                         │
    │──── CTRL_QUIESCE(Port=X) ────────────►│  T0: GPU 发起 Drain
    │                                         │  T1: Switch 停止向 CXL Port X 发新请求
    │                                         │      (继续接收 CXL Completion)
    │                                         │  T2: Switch CXL Outstanding Counter == 0
    │◄──── CTRL_Q_ACK(Port=X) ─────────────│  T3: Switch 确认 CXL 侧已排空
    │                                         │
    │  [GPU 等待本地 ROB 归零]                 │
    │                                         │
    │──── CTRL_RESUME(Port=X) ─────────────►│  T4: GPU 本地排空完成, 通知 Switch 恢复
    │                                         │  T5: Switch 恢复正常调度
```

### §6.2 时序约束与超时 (per MAS-3.1-Fabric §4.2)

```
v1.0 MVP 严格时序约束:
1. Switch ACK 延迟 (T_ack): T3 - T0 ≤ 50 μs
2. GPU 本地排空延迟 (T_local): T4 - T3 ≤ 20 μs
3. GPU 总超时 (T_timeout): 从 T0 起计, 若 100 μs 内未收到 CTRL_Q_ACK:
   - 置位 RAS_ERROR_STATUS.DRAIN_TIMEOUT
   - 触发 Fatal MSI-X 中断
   - 允许 FM 写入 FORCE_ABORT 强制清零 ROB
4. 非目标端口不受影响: Drain Port X 期间, Port Y/Z 的 MEM_RD/WR_REQ 和 MEM_RESP 必须正常传输, 严禁全局停顿
```

**v1.0 SVA 断言要求**: 上述 4 条时序约束必须全部有对应 SystemVerilog Assertions, 在 Formal Verification 中证明无违反 (per MAS-3.1-Fabric §7.3)。

### §6.3 GPU 侧 Drain FSM 状态机

```cpp
enum class DrainFsmState : uint8_t {
    IDLE,         // 正常流量
    BLOCK_NEW,    // 拒绝新 Micro-op, 返回 Abort
    WAIT_SWITCH,  // 等待 Switch CTRL_Q_ACK
    DRAIN_LOCAL   // 等待本地 ROB 归零
};

// 触发条件 (per `21-microarch-ifc-mvp.md` §4.2 NIC_DRAIN_CTRL):
// FM 写 NIC_DRAIN_CTRL.START_DRAIN = 1 → 进入 BLOCK_NEW
// FM 写 NIC_DRAIN_CTRL.FORCE_ABORT = 1 → 强制清零 ROB
```

---

## §7 RAS 与 Poison 传播协议

### §7.1 Poison 编码映射 (per MAS-3.1-Fabric §5.1 + V3.1-Rev2.0 拓扑修正)

| 协议层 | Poison 编码位置 | 值 / 含义 | v1.0 MVP |
|--------|------------------|-----------|----------|
| **CXL.mem** (Switch 侧) | Completion Header `[Status]` | `0b10` = Poisoned Data | ✅ Switch PTE 透明转换 (CXL 在 GPU Die 之外) |
| **UALink** | `MEM_RESP_D` Flit Header `[Status]` | `0x11` = Remote_Error (Poison) | ✅ Switch → GPU NIC-DMA |
| **UDD Micro-op Response** | Response Payload `[Status]` | `0x1` = Remote_CXL_Poison_Via_Switch | ✅ |
| **SM AWT** (GPC 侧) | `AWT_PAYLOAD_2[Trap_Type]` | `0x1` = REMOTE_CXL_POISON_VIA_SWITCH Trap | ✅ (per `21-microarch-ifc-mvp.md` §7.6) |

> **⚠️ V3.1-Rev2.0 拓扑修正 (per `21-soc-topology-mvp.md` §4.3)**:
> SM AWT Trap Type `0x1` 的完整名称为 `REMOTE_CXL_POISON_VIA_SWITCH`——
> **显式标注 Poison 来源是外部 Scale-Up Switch 转换的 CXL Memory Pool**，
> 而**非** GPU 直连 CXL (V3.0 错误理解, GPU Die 上无 CXL Bridge)。
> 处理流程见 `21-soc-topology-mvp.md` §4.3 与 `21-microarch-ifc-mvp.md` §7.6。

### §7.2 Poison 处理语义 (per MAS-3.1-Fabric §5.2)

```
v1.0 MVP Poison 处理:
1. Switch 侧: 收到 CXL Poison Completion 后, 必须生成 UALink MEM_RESP_D with Remote_Error
   禁止静默丢弃或替换为正常数据

2. GPU NIC-DMA 侧: 收到 Remote_Error Flit 后, 将 UDD Response Status 设为 0x1, 通过 NoC 返回 GPC

3. GPC UDD Agent 侧: 识别 Remote_CXL_Poison, 触发 SM AWT Trap, 阻断寄存器写回

4. 日志记录: Switch 和 NIC-DMA 均需记录 Poison 事件的 Source Port, Address, Timestamp
```

### §7.3 链路级错误传播 (per MAS-3.1-Fabric §5.3)

```
v1.0 MVP 链路级错误:
- UALink CRC/Replay Failure: 超过重传阈值 → Link Down
  → Switch 向所有关联 CXL Port 发送 QUIESCE
  → 向 GPU 返回 Remote_Error (模拟 Poison)

- CXL Link Failure: CXL PHY 错误 → CXL Port Link Down
  → Switch 向所有访问该 CXL 地址范围的 GPU UALink Port 返回 Remote_Error
```

---

## §8 Scale-Up Switch 微架构

### §8.1 内部模块

```text
┌──────────────────────────────────────────────────────────────┐
│                    Scale-Up Switch Core                      │
│                                                              │
│  [UALink Port 0..N] ◄──► ┌───────────────────────────────┐   │
│      (GPU NIC-DMA)       │      Crossbar / Router        │   │
│                          │  (Flit-level Non-blocking)    │   │
│  [CXL Port 0..M]   ◄──►  ├───────────────────────────────┤   │
│   (Memory Pool)          │   Protocol Translation Engine │   │
│                          │      (UALink <-> CXL.mem)     │   │
│                          ├───────────────────────────────┤   │
│  [Sideband/Mgmt]   ◄──►  │   Control & Config Registers  │   │
│                          │   (Base/Limit, Drain FSM)     │   │
│                          └───────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

### §8.2 端口配置 (v1.0 MVP)

```
v1.0 MVP 默认配置:
- Upstream Ports (GPU 侧): N=4 个 UALink x16 端口
- Downstream Ports (Memory 侧): M=2 个 CXL 3.0 x16 端口
- Sideband/Management Port: SMBus/I2C (Host CPU / FM)

v1.1+ 可扩展:
- N=8 (更多 GPU)
- M=4 (更多 CXL Memory Pool)
- PCIe Gen5 x4 sideband (per MAS-3.1-ScaleUp-Switch §1.1)
```

### §8.3 Protocol Translation Engine (PTE) 微架构 (per MAS-3.1-ScaleUp-Switch §2)

```cpp
// include/tlm/fabric/switch_pte_tlm.hh
namespace cpptlm::fabric {

// PTE 地址映射窗口 (per Switch Spec §2.2)
struct PteAddrWindow {
    uint64_t base_gupa;      // [63:0] Base GUPA (4KB aligned)
    uint64_t limit_gupa;     // [63:0] Limit GUPA (4KB aligned)
    uint8_t  target_cxl_port; // 目标 CXL Port ID
    uint64_t hpa_offset;     // CXL HPA 偏移 = (UALink_GUPA - Base) + HPA_Offset
    uint8_t  enable;
};

class SwitchPteTLM : public sc_module {
public:
    SC_HAS_PROCESS(SwitchPteTLM);

    // 上游: UALink Port N (从 GPU 接收 Flit)
    static constexpr size_t MAX_UALINK_PORTS = 4;
    sc_port<ualink_flit_if> ualink_in[MAX_UALINK_PORTS];

    // 下游: CXL Port M (发往 Memory Pool)
    static constexpr size_t MAX_CXL_PORTS = 2;
    sc_port<cxl_mem_if> cxl_out[MAX_CXL_PORTS];

    SwitchPteTLM(sc_module_name name);
    ~SwitchPteTLM() override = default;

    // 主 tick
    void tick();

    // P2P Bypass 决策 (per §3.1.3 backends)
    bool is_p2p_target(uint64_t gupa, uint8_t& target_ualink_port);

    // CXL Address Translation
    uint64_t translate_to_cxl_hpa(uint64_t gupa);

private:
    // 地址映射窗口 (per §6 CSR)
    static constexpr size_t MAX_ADDR_WINDOWS = 8;
    std::array<PteAddrWindow, MAX_ADDR_WINDOWS> addr_windows_{};

    // Drain FSM (per §6.2 Switch Spec)
    enum class SwitchDrainState : uint8_t {
        IDLE, BLOCK_NEW, DRAIN_CXL, SEND_ACK, QUIESCED
    };
    std::array<SwitchDrainState, MAX_CXL_PORTS> cxl_drain_state_{};
    uint64_t switch_quiesce_start_cycle_[MAX_CXL_PORTS] = {};

    // 错误日志
    uint64_t poison_propagation_count_ = 0;
    uint64_t drain_ack_latency_total_ = 0;
    uint64_t drain_ack_count_ = 0;
};

}  // namespace cpptlm::fabric
```

### §8.4 事务映射规则 (per MAS-3.1-ScaleUp-Switch §2.1)

| UALink 事务 (Initiator: GPU) | Switch 动作 | CXL.mem 事务 (Target: Memory Pool) | v1.0 MVP |
|------------------------------|--------------|------------------------------------|----------|
| `Mem-Read` | 转换 Header, 附加 CXL Tag | `CXL.mem Read` | ✅ |
| `Mem-Write` | 转换 Header, 数据透传 | `CXL.mem Write` | ✅ |
| `Mem-Response` (Data) | CXL Completion 转 UALink Flit | `CXL.mem Completion` | ✅ |
| `Mem-Response` (No Data) | 转换状态码 | `CXL.mem Completion` (No Data) | ✅ |
| `Atomic` (Fetch-and-Add) | **拦截并在 Switch 内执行** (可选) | 若 CXL 支持则透传, 否则降级为 RMW | ❌ v2.1+ |

### §8.5 地址空间映射 (per MAS-3.1-ScaleUp-Switch §2.2)

```
映射公式:
  CXL_HPA = (UALink_GUPA - Base_Register) + HPA_Offset

路由决策:
  if UALink_GUPA ∈ [Base, Limit]:
    → 路由至 PTE 进行 CXL 转换 → CXL Port
  elif UALink_GUPA ∈ 其他 GPU Local HBM 范围:
    → 直接通过 Crossbar 路由至目标 GPU 的 UALink Port (P2P 旁路, 不经过 PTE)
  else:
    → 触发 Address Error, 记录 RAS 日志
```

### §8.6 P2P Bypass 路径 (per MAS-3.1-DMA-Backends §3.1.3)

```
v1.0 MVP P2P 触发条件:
- GPU A 访问 GPU B 的 Local HBM
- Switch 收到 UALink Flit, 解析 GUPA
- GUPA 落在 [其他 GPU Local HBM 范围] (per Base/Limit 配置)
- 直接通过 Crossbar 路由至 GPU B UALink Port
- **不经过 PTE (无 CXL 协议转换)**

GPU B NIC-DMA 收到 P2P Flit:
- 识别 DstNode = 本节点
- 解码 GUPA → 落在 Local HBM
- 触发 p2p_bypass_active_ = true (per backends §6.3)
- 直接通过内部旁路总线写入 HBM-DMA Write Queue
- 不经过 UBC + Memory NoC
- 降低 15% P2P 延迟
```

---

## §9 Switch Drain 状态机 (per MAS-3.1-ScaleUp-Switch §3.2)

### §9.1 控制通道定义

```
v1.0 MVP Drain 命令 (UALink Control Flit / Sideband):
- QUIESCE_PORT_REQ (GPU → Switch): 请求排空指定 CXL Port 的在途事务
- PORT_QUIESCED_ACK (Switch → GPU): Switch 确认 CXL 侧已排空
- RESUME_PORT (GPU → Switch): Drain 完成后恢复正常流量
```

### §9.2 握手状态机 (Switch 侧 FSM)

```text
[ IDLE ] ──(收到 QUIESCE_PORT_REQ)──► [ BLOCK_NEW ]
   ▲                                      │
   │                                      ├─ 停止向目标 CXL Port 发送新的 CXL.mem Req
   │                                      ├─ 继续接收 CXL.mem Completion
   │                                      ▼
   │                                  [ DRAIN_CXL ]
   │                                      │
   │                                      ├─ 等待目标 CXL Port 的 Outstanding Counter == 0
   │                                      ▼
   │                                  [ SEND_ACK ]
   │                                      │
   │                                      ├─ 发送 PORT_QUIESCED_ACK 给 GPU
   │                                      ▼
   └────────(收到 RESUME_PORT)──────── [ QUIESCED ]
```

### §9.3 时序与超时约束

```
v1.0 MVP Switch 侧约束:
1. Switch 排空延迟: 从收到 QUIESCE_PORT_REQ 到发送 PORT_QUIESCED_ACK
   必须在 50 μs 内完成 (假设 CXL 介质最大延迟 10 μs, 预留重试余量)

2. GPU 侧超时联动: GPU NIC-DMA Drain 超时设定为 100 μs
   若 Switch 未能及时回复 ACK, GPU 触发 Fatal RAS 中断

4. 死锁避免: 在 BLOCK_NEW 状态下, Switch 必须继续响应 GPU 发往其他 Port
   (包括其他 CXL Port 或 P2P GPU Port) 的请求, 严禁全局停顿
```

---

## §10 关键 CSR 映射 (Switch Sideband MMIO, per MAS-3.1-ScaleUp-Switch §6)

| 偏移地址 | 寄存器名称 | R/W | 描述 | v1.0 MVP |
|----------|-----------|-----|------|----------|
| `0x0000` | `SWITCH_DEV_ID` | R | 设备 ID 与版本号 | ✅ |
| `0x0100` | `UALINK_PORT_CTRL[N]` | W | UALink 端口使能, 速率配置, Drain 状态查询 | ✅ |
| `0x0200` | `CXL_PORT_CTRL[M]` | W | CXL 端口使能, HDM 状态机控制 | ✅ |
| `0x0300` | `ADDR_MAP_BASE[K]` | W | 第 K 个地址映射窗口的 Base GUPA | ✅ |
| `0x0304` | `ADDR_MAP_LIMIT[K]` | W | 第 K 个地址映射窗口的 Limit GUPA | ✅ |
| `0x0308` | `ADDR_MAP_TARGET[K]` | W | 目标 CXL Port ID 及 HPA Offset | ✅ |
| `0x0400` | `RAS_ERROR_STATUS` | R | 链路错误, Poison 传播计数, Drain 超时标志 | ✅ |
| `0x0500` | `PMU_COUNTERS` | R | 性能监控 (Drain 延迟, P2P 命中率等) | ✅ |

**v1.0 MVP 必须实现**: `0x0100` (UALink Port Ctrl) + `0x0200` (CXL Port Ctrl) + `0x0300-0x0308` (Address Mapping) + `0x0400` (RAS Error Status), `0x0500` (PMU) 可选。

---

## §11 协议合规性与验证要求 (per MAS-3.1-Fabric §7)

### §11.1 v1.0 MVP 合规性要求

```
v1.0 MVP 必须满足:
1. UALink Compliance: 通过 UALink Consortium 官方 Compliance Test Suite (CTS) v1.1
   - 仅 Mem 子集, 不含 Cache/IO

2. SVA Assertions: 所有 Drain 时序约束 (§6.2) 和 Poison 传播路径 (§7.1) 
   必须有对应的 SystemVerilog Assertions, 在 Formal Verification 中证明无违反

3. Protocol Fuzzing: 对 UALink 接口进行随机 Flit 注入测试, 验证异常处理逻辑
   不会导致死锁或数据损坏

4. P2P Bypass 延迟验证: P2P 路径延迟 < 常规路径 85% (15% 优化目标)
```

### §11.2 v1.0 MVP 不实现的合规性

- ❌ CXL 3.0 HDM Compliance Test (Switch PTE 模拟 CXL, 真实 CXL 联调推迟 v2.0+)
- ❌ PCIe Compliance (PCIe 推迟 v1.1+)
- ❌ 多厂商 CXL Memory Expander 互操作 (推迟 v2.0+)

---

## §12 测试覆盖

### §12.1 单元测试 (`test_fabric_*.cc`, `test_switch_*.cc`)

| # | 测试用例 | 覆盖能力 | 断言 |
|---|---------|---------|------|
| 1 | `Fabric.UDD Micro-op header decode` | Header 位分配 | 各字段解码正确 |
| 2 | `Fabric.Route_Tag.0=HBM` | Route_Tag 路由 | Route_Tag=0 路由至 HBM-DMA |
| 3 | `Fabric.Route_Tag.1~4=UALink` | Route_Tag 路由 | Route_Tag=1~4 路由至对应 NIC-DMA Port |
| 4 | `Fabric.Credit.acquire/return` | Credit 流控 | Credit 满 → Stall, Response → Return |
| 5 | `Fabric.UALink Flit encode` | UALink Flit 256B 格式 | 各字段编码正确 |
| 6 | `Fabric.UALink.VC0=Drain` | VC 隔离 | Drain 命令独立 VC, 不阻塞 |
| 7 | `Fabric.UALink.GoBackN` | Retry/Replay | NAK → 从 K 重传 |
| 8 | `Fabric.Drain.TwoPhase normal` | 两阶段 Drain | IDLE → BLOCK_NEW → WAIT_SWITCH → DRAIN_LOCAL |
| 9 | `Fabric.Drain.Timeout` | Drain 超时 | 100us 超时 → Fatal RAS |
| 10 | `Fabric.Poison.Remote_Error` | Poison 传播 | Remote_Error → UDD Status=0x1 |
| 11 | `Switch.PTE.BaseLimit match` | 地址映射 | GUPA 落在 [Base, Limit] 路由至目标 CXL Port |
| 12 | `Switch.PTE.P2P bypass` | P2P 旁路 | GUPA 落其他 GPU Local HBM 范围 → 直 Crossbar |
| 13 | `Switch.Drain.Switch side` | Switch Drain FSM | BLOCK_NEW → DRAIN_CXL → SEND_ACK |
| 14 | `Switch.Poison.CXL → UALink` | CXL Poison 转换 | CXL Status=Poison → UALink Remote_Error |
| 15 | `Switch.CSR.Sideband access` | Sideband MMIO | FM 写 CSR 正确 |

### §12.2 集成测试 (`test_fabric_switch_p2p_e2e.cc`, `test_switch_cxl_e2e.cc`)

**核心 demo 测试** (per §2 端到端 shippable demo):
- P2P E2E: 10 步全链路贯通 (GPU 0 → Switch → GPU 1 HBM)
- CXL E2E: GPU 0 → Switch → CXL Memory Pool (模拟)
- 测试标签: `[fabric][switch][p2p][cxl][e2e]`

### §12.3 形式验证 (Formal Verification)

- SVA Assertions for Drain 时序 (§6.2 4 条约束)
- SVA Assertions for Poison 传播路径 (§7.1 4 层映射)
- 死锁/活锁检测 (Liveness)

### §12.4 Oracle 评审清单

- [ ] UALink Flit 256B 格式与 SLA v1.1 一致
- [ ] 4 VC 严格隔离 (Drain 命令独立 VC0)
- [ ] 两阶段 Drain ≤100us 超时约束 SVA 通过
- [ ] 4 层 Poison 映射正确 (CXL → UALink → UDD → AWT)
- [ ] Switch PTE Base/Limit 校验延迟 ≤3 cycles
- [ ] P2P Bypass 延迟 < 常规路径 85%
- [ ] Switch Drain ≤50us 排空延迟
- [ ] 跨仓 ABI 影响: 0 个新 ABI 函数 (v1.0 MVP 严格遵守)
- [ ] 5 阶段演进路线图无债务 (per evolution-roadmap §5)

---

## §13 演进不变量 (4 条契约)

**详细定义见 [`21-fabric-switch-evolution-roadmap.md` §5](21-fabric-switch-evolution-roadmap.md)**。本节列出 v1.0 MVP 的具体实现要求。

### §13.1 不变量 1: UDD Micro-op / UALink Flit 接口在演进中只增不换

```
v1.0: 160-bit Micro-op + 256B UALink Flit (per §4.1, §5.1)
v2.0+: 仅扩展预留字段, 不破坏现有字段布局
实现: header 字段固定 32-bit, 演进通过 Reserved 扩展位
```

### §13.2 不变量 2: Switch PTE 地址映射窗口从 v1.0 起 extensible

```cpp
// v1.0 即定义 8-entry PteAddrWindow array, 即使 v1.0 仅 2-4 窗口启用
static constexpr size_t MAX_ADDR_WINDOWS = 8;
std::array<PteAddrWindow, MAX_ADDR_WINDOWS> addr_windows_{};
// v1.1+ 加窗口时仅追加数组项启用, 不重构数据结构
```

### §13.3 不变量 3: UALink MsgType 从 v1.0 起定义完整 (7 种)

```cpp
// v1.0 即定义完整 UALink MsgType 枚举, 即使 v1.0 仅 5 种实际使用
enum class UALinkMsgType : uint8_t {
    MEM_RD_REQ = 0x1,
    MEM_WR_REQ = 0x2,
    MEM_RESP_D = 0x3,
    MEM_RESP_ND = 0x4,
    CTRL_QUIESCE = 0x5,
    CTRL_Q_ACK = 0x6,
    CTRL_RESUME = 0x7
    // Multicast (0x8) + Reduce (0x9) 推迟 v3.0
};
```

### §13.4 不变量 4: Drain FSM 从 v1.0 起按 GPU-Switch 协同双 FSM

```
v1.0 MVP 实现: GPU FSM (IDLE → BLOCK_NEW → WAIT_SWITCH → DRAIN_LOCAL)
              + Switch FSM (IDLE → BLOCK_NEW → DRAIN_CXL → SEND_ACK → QUIESCED)
v2.0+ 加分布式 Drain (跨多个 Switch Port): 仅扩展 FSM 状态机, 不破坏既有状态
```

---

## §14 边界与限制

### §14.1 不实现的功能 (v1.0 MVP 明确边界)

- ❌ **L4 Application (TMA / GDS / P2P 高层语义)** — TMA 在 tee-udd 文档, GDS/P2P 推迟 v1.1+
- ❌ **L1 PCIe / CXL.io 协议** — IO-DMA 推迟 v1.1+ (backends 协同)
- ❌ **Atomic Operations (Atomic Add/Sub/CmpAndSwap)** — v2.1+
- ❌ **UALink Multicast (All-Reduce / All-Gather)** — CXL.mem 不支持, 需 Switch 拆分, v3.0
- ❌ **Switch Hardware Reduce Engine** — v3.0
- ❌ **Switch Multicast 路由** — v3.0
- ❌ **CXL.cache / CXL.io 在 GPU 侧** — 严格剥离 (per MAS-3.1-Fabric §1)
- ❌ **L2 Coherence 协议 (Back-invalidate 经 SnoopFilter)** — v3.0
- ❌ **真实 CXL Memory Pool 联调** — v1.0 模拟, v2.0+ 真实

### §14.2 已知限制

- UALink Flit 256B 数据 payload = 248B (header 8B), 不含 ECC
- 4 VC 严格隔离, 不支持用户配置 VC 优先级
- Switch PTE 地址映射窗口: 8 entry, 静态配置 (不支持运行时动态添加)
- Switch Crossbar Non-blocking 端口上限: 4 UALink + 2 CXL (per §8.2)
- P2P Bypass 仅支持 Peer GPU Local HBM target, 不支持 Peer GPU CXL target
- Drain FSM 超时硬编码 (100us), 不动态调整
- Poison 仅传播 Status Code, 不传播错误类型细节 (per MAS-3.1-Fabric §5.2)

---

## §15 跨仓契约

### §15.1 UsrLinuxEmu 侧 (不修改 23 ABI)

Fabric+Switch v1.0 MVP **不引入任何新 CppTLM ABI 函数**——所有 driver↔GPU/Switch 通信通过**现有 23 ABI 函数** (`syms->mmio_write/read`).

### §15.2 CppTLM 侧 (新模块 + 命名变更)

| 文件 | 状态 |
|------|------|
| `include/tlm/fabric/fabric_protocol_tlm.{hh,cc}` | **新建** (Fabric 协议层基础) |
| `include/tlm/fabric/switch_pte_tlm.{hh,cc}` | **新建** (Switch PTE 微架构) |
| `include/tlm/fabric/switch_crossbar_tlm.{hh,cc}` | **新建** (Switch Crossbar) |
| `include/tlm/fabric/switch_sideband_tlm.{hh,cc}` | **新建** (Switch Sideband MMIO) |
| `test/test_fabric_*.cc` (10+ 用例) | **新建** |
| `test/test_switch_*.cc` (5+ 用例) | **新建** |
| `test/test_fabric_switch_p2p_e2e.cc` | **新建** (P2P 端到端 demo) |
| `test/test_switch_cxl_e2e.cc` | **新建** (CXL 端到端 demo) |
| **23 ABI 头冻结** | ✅ **不变** (per ADR-088 §D5) |

### §15.3 跨仓 PR 协调

按 ADR-091 §R5.1 跨仓 PR 流程:
1. CppTLM 仓: 实现 Fabric + Switch + 15+ 测试
2. UsrLinuxEmu 仓: driver 协调 Switch CSR (由用户承担协调)
3. 跨仓集成测试: `test_fabric_switch_p2p_e2e_ue.cc`
4. 同步 PR (无 ABI 影响, 跨仓风险低)

---

## §16 引用

### §16.1 内部引用

- [`21-fabric-switch-evolution-roadmap.md`](21-fabric-switch-evolution-roadmap.md) — Fabric+Switch 5 阶段演进路线图 (SSOT for evolution)
- [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) — TEE + UDD MVP (上游)
- [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) — Backends MVP (下游)
- [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) — Core/Micro-Arch + RTL-IFC (微架构/接口)
- [`16-pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT

### §16.2 跨仓引用

- UsrLinuxEmu AGENTS.md §CppTLM 通信架构 — **PCIe-only 原则**
- UsrLinuxEmu ADR-088 §D5 — 23 ABI 冻结

### §16.3 MAS-3.1 上游原始设计

- `MAS-3.1-Fabric/Protocol Rev2.0` (Fabric 协议原始设计)
- `MAS-3.1-ScaleUp-Switch-Spec Rev2.0` (Scale-Up Switch 原始设计)

### §16.4 业界参考

- UALink Consortium UALink 1.1 Specification (Mem 子集)
- CXL 3.0 Specification (HDM-DB, 仅 Switch 侧实现)
- PCI-SIG ATS/PRI 1.0 (v1.1+ 引入)
- ARM AMBA CHI / CMN-700 (NoC + HN-F Router)

---

## §17 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: Fabric + Scale-Up Switch v1.0 MVP 详细设计 (UALink 1.1 Mem + Switch PTE + 两阶段 Drain + 4 层 Poison 映射 + 10 步 shippable demo + 15 测试用例 + 4 条不变量) |

---

**关联 OpenSpec change**: 待 `openspec/changes/2026-09-19-cpptlm-mas-fabric-switch-mvp/` 提案创建
**下次更新**: Oracle 评审反馈后 v1.1 (PCIe/CXL.io 引入)