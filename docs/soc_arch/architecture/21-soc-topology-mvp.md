# dGPU SoC 顶层物理布局规范 v1.0 MVP (MAS-3.1-SoC: Chip-Level Topology & Integration)

> **目的**: 定义 CppTLM dGPU SoC **MAS-3.1** 的 **芯片级顶层物理布局**——明确五大子系统 (GPC / Memory / Fabric-IO / Control / External Bridge) 的**物理归属**、**模块边界**、**互联拓扑**。本规范是 8 份子系统文档 ([`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) / [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) / [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) / [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md)) 的**顶层集成契约**，解决"模块归属错误 + 接口缺失"问题。
>
> **状态**: Draft v1.0 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 ≥9.0/10 PASS)
> **变更摘要**: **V3.1-Rev2.0 拓扑修正** —— ① TC-DMA 物理归属修正为 GPC 紧耦合; ② IO-DMA/PCIe 端口补全; ③ 移除片上 CXL PHY/Controller, 明确 CXL Memory Pool 是 Scale-Up Switch 下挂设备
> **关联文档**:
> - [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) — TEE + UDD (核心数据面)
> - [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) — UBC + HBM/NIC/IO-DMA (后端物理实现)
> - [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) — Fabric 协议 + Scale-Up Switch (互联契约层)
> - [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) — Core/Micro-Arch + RTL-IFC (实现层契约)
> - [`21-tee-udd-evolution-roadmap.md`](21-tee-udd-evolution-roadmap.md) — TEE-UDD 演进路线图
> - [`00-overview.md`](00-overview.md) §3 SoC 顶层架构
> - [`16-pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT

> **NSA 命名含义澄清** (适用于所有 NSA 草案, 2026-09-19 决策):
>
> 本草案中 **NSA** 含义为 **"Network-System Architecture"** (多 Linux 节点 + 跨 Fabric 寻址架构), 与美国 **National Security Agency (国家安全局)** **无关**, 仅 CppTLM 内部使用。
>
> NSA 衍生术语对应关系:
>
> | NSA 衍生术语 | 含义 | 与 CXL 3.0 / NVLink / Infinity Fabric 对应 |
> |---|---|---|
> | NSA Switch | NSA Switch (跨 Fabric 路由器) | ≈ CXL Fabric Switch Tier 1 |
> | NSA Fabric Address | 64-bit NSA-aware 地址 (16+48 bits) | ≈ CXL Fabric Address |
> | NSA-aware MMU | 含 Fabric ID + Capability 的 MMU | ≈ CXL-aware IOMMU |
> | NSA Stage 1/2/3 | 5 阶段演进阶段 | ≈ CXL 3.0 Fabric 量产节奏 |
> | NSA-aware SoC | NSA-aware 分布式 SoC 终态 | ≈ CXL 3.0 Fabric-aware SoC |
>
> **替代命名参考** (若未来需替换): GFS (Global Fabric System) / FAS (Fabric Address Space) / UFA (Unified Fabric Address), **当前决策保持 NSA + 备注澄清**。
>

> **关联 ADR**:
> - ADR-088 §D5 — 23 ABI 冻结 (本规范不动 ABI)
> - ADR-SOC-10 (待起草) — TEE/UDD/UBC 三层解耦 (本规范边界)
> - ADR-SOC-21 — **V3.1-Rev2.0 拓扑修正** (本规范核心变更)

---

## §0 阅读引导

- 想理解 SoC 顶层物理布局 → 读 §1 (修正后的物理布局) + §2 (关键模块划分)
- 想理解模块间数据流 → 读 §3 (关键数据流路径)
- 想理解控制流和 HRT/RCT 含义 → 读 §4 (控制流修正)
- 想理解 RTL-IFC 联动变更 → 读 §5 (接口修订影响)
- 想理解 v1.0 MVP 端到端 demo → 读 §6
- 想理解演进约束 → 读 §7 (3 条不变量)
- 想理解边界 → 读 §8 (SoC 不实现的功能)
- 想理解跨仓契约 → 读 §9
- 想查阅反面模式 → 读 §10
- 想理解引用 → 读 §11

---

## §1 修正后的 SoC 顶层物理布局

### §1.1 V3.1-Rev2.0 三点关键修正

| # | 修正点 | 修正前（错误） | 修正后（正确） |
|---|--------|-----------------|------------------|
| **1** | **TC-DMA 物理归属** | 与 HBM-DMA 平级, 可直连 HBM | **GPC 内紧耦合组件**, 仅通过 GPC UDD Agent 间接访问 HBM |
| **2** | **IO-DMA / PCIe 端口** | 拓扑图中缺失或与 NIC-DMA 混为一体 | **Fabric & Edge IO Subsystem 内独立 Backend**, 与 NIC-DMA 平级但物理端口分离 |
| **3** | **CXL 接口位置** | GPU Die 内有 CXL PHY/Controller | **GPU Die 上无 CXL**, CXL Memory Pool 挂载在外部 Scale-Up Switch 下 |

### §1.2 修正后的五大子系统划分

```
MAS-3.1 dGPU SoC 顶层子系统划分 (V3.1-Rev2.0):

┌─ GPC Subsystem (计算集群) ────────────────────────────────┐
│  - SM × 16/32 (per GPC)                                   │
│  - TC-DMA (TMA Engine) ← 紧耦合, 不直接连接 HBM           │
│  - L2 Cache Slice                                          │
│  - UDD Agent (per-GPC HRT 查表)                            │
│  - TEE Frontend (4 级流水线)                               │
└───────────────────────────────────────────────────────────┘

┌─ Memory Subsystem (本地内存) ─────────────────────────────┐
│  - HBM-DMA × N (HBM3e Controllers)                        │
│  * 严格剥离 TC-DMA (归属 GPC Subsystem)                    │
└───────────────────────────────────────────────────────────┘

┌─ Fabric & Edge IO Subsystem (互联 + 边缘 IO) ─────────────┐
│  - Global UDD Hub (集中式 RCT/HRT/仲裁)                  │
│  - Memory & IO NoC                                         │
│  - NIC-DMA (Scale-Up / UALink)            ← 跨节点       │
│  - IO-DMA (PCIe Gen6 / CXL.io Host-to-GPU) ← Host 通信    │
└───────────────────────────────────────────────────────────┘

┌─ Control Subsystem (配置与安全控制岛) ────────────────────┐
│  - CIU (Configuration & Isolation Unit)                    │
│  - AWT Controller (Asynchronous Warp Trap)                 │
│  - GMMU Hub (per-GPC GMMU 协同)                            │
│  - Boot ROM                                                │
└───────────────────────────────────────────────────────────┘

┌─ External (GPU Die 之外) ─────────────────────────────────┐
│  - Scale-Up Switch (UALink Fabric + CXL Expander Ctrl)    │
│  - CXL Memory Pool (CXL 3.0 HDM Device)                   │
│  - Host CPU + NVMe (PCIe 端点)                            │
└───────────────────────────────────────────────────────────┘
```

### §1.3 修正后的 SoC 顶层互联拓扑图

```text
┌─────────────────────────────────────────────────────────────────────────────────┐
│                        MAS-3.1 GPU SoC (Die Boundary)                           │
│                                                                                 │
│  ╔══════════════════════════ GPC Subsystem ════════════════════════════════╗   │
│  ║  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐   ║   │
│  ║  │    GPC 0     │ │    GPC 1     │ │    GPC 2     │ │    GPC N     │   ║   │
│  ║  │ ┌──────────┐ │ │ ┌──────────┐ │ │ ┌──────────┐ │ │ ┌──────────┐ │   ║   │
│  ║  │ │ SM Array │ │ │ │ SM Array │ │ │ │ SM Array │ │ │ │ SM Array │ │   ║   │
│  ║  │ ├──────────┤ │ │ ├──────────┤ │ │ ├──────────┤ │ │ ├──────────┤ │   ║   │
│  ║  │ │ TC-DMA ◄─┼─┼─┼─► TC-DMA ◄─┼─┼─┼─► TC-DMA ◄─┼─┼─┼─► TC-DMA │ │   ║   │
│  ║  │ │ (TMA)    │ │ │ │ (TMA)    │ │ │ │ (TMA)    │ │ │ │ (TMA)    │ │   ║   │
│  ║  │ ├──────────┤ │ │ ├──────────┤ │ │ ├──────────┤ │ │ ├──────────┤ │   ║   │
│  ║  │ │ L2 Slice │ │ │ │ L2 Slice │ │ │ │ L2 Slice │ │ │ │ L2 Slice │ │   ║   │
│  ║  │ ├──────────┤ │ │ ├──────────┤ │ │ ├──────────┤ │ │ ├──────────┤ │   ║   │
│  ║  │ │UDD Agent │ │ │ │UDD Agent │ │ │ │UDD Agent │ │ │ │UDD Agent │ │   ║   │
│  ║  │ └────┬─────┘ │ │ └────┬─────┘ │ │ └────┬─────┘ │ │ └────┬─────┘ │   ║   │
│  ║  └──────┼───────┘ └──────┼───────┘ └──────┼───────┘ └──────┼───────┘   ║   │
│  ╚═════════╪════════════════╪════════════════╪════════════════╪════════════╝   │
│            │                │                │                │                │
│  ══════════╪════════════════╪════════════════╪════════════════╪════════════════ │
│            │        Compute NoC (Low-Latency, Flit-based)                    │
│  ══════════╪════════════════╪════════════════╪════════════════╪════════════════ │
│            │                │                │                │                │
│  ╔═════════╪════════════════╪════════════════╪════════════════╪═════════════╗ │
│  ║      Global UDD Hub + RCT/HRT (Fabric Subsystem 核心)                  ║ │
│  ║                                                                         ║ │
│  ║   * 注：此 Hub 仍属 GPU Die 内 (与 V3.0 紧耦合设计的关键区别)         ║ │
│  ╚══┬──────────────┬──────────────────────┬───────────────────┬─────────┘ │
│     │              │                      │                   │             │
│  ═══╪══════════════╪══════════════════════╪═══════════════════╪══════════════ │
│     │      Memory & IO NoC (High-Bandwidth, Flit)                           │
│  ═══╪══════════════╪══════════════════════╪═══════════════════╪══════════════ │
│     │              │                      │                   │             │
│  ┌──▼──────┐  ┌───▼────────┐       ┌────▼─────────┐   ┌─────▼──────────┐  │
│  │ HBM-DMA │  │  HBM-DMA   │       │  NIC-DMA     │   │   IO-DMA       │  │
│  │ (Ctrl0) │  │  (Ctrl1)   │       │ (UALink/RDMA)│   │ (PCIe Gen6/    │  │
│  └────┬────┘  └─────┬──────┘       │ Scale-Up Port│   │  Host-to-GPU)  │  │
│       │            │               └──────┬───────┘   └───────┬──────────┘  │
│       ▼            ▼                      │                   │             │
│  [HBM3e Stack] [HBM3e Stack]               │                   │             │
│                                              │                   │             │
└──────────────────────────────────────────────┼───────────────────┼─────────────┘
                                               │ UALink x16        │ PCIe x16
                                               ▼                   ▼
                                ┌─────────────────────────┐   [Host CPU + NVMe]
                                │   Scale-Up Switch       │
                                │  (UALink Fabric + CXL   │
                                │   Expander Controller)  │
                                │   * CXL 在此转换*        │
                                └────────┬────────────────┘
                                         │ CXL 3.0 Mem Protocol
                                         ▼
                                ┌─────────────────────┐
                                │ CXL Memory Pool     │ ← 不在 GPU Die 上
                                │ (DDR5/HBM Expander) │
                                └─────────────────────┘
```

### §1.4 子系统边界与接口对照表

| 子系统边界 | 上游接口 | 下游接口 | 文档 SSOT |
|-----------|---------|---------|----------|
| **GPC → Compute NoC** | `gpc_inject_*` (UDD Micro-op) | N/A | [`21-tee-udd-mvp.md` §4.5](21-tee-udd-mvp.md) |
| **Compute NoC → Global Hub** | Compute NoC Flit | Global Hub Flit | [`21-microarch-ifc-mvp.md` §5.4](21-microarch-ifc-mvp.md) |
| **Global Hub → HBM-DMA / NIC-DMA / IO-DMA** | `udd_be_req_*` | Backend Micro-op | [`21-dma-backends-mvp.md` §3.1](21-dma-backends-mvp.md) |
| **HBM-DMA → HBM3e Stacks** | HBM3e Command | HBM3e Data | [`21-dma-backends-mvp.md` §5](21-dma-backends-mvp.md) |
| **NIC-DMA → Scale-Up Switch** | UALink Flit (256B) | UALink Flit | [`21-fabric-switch-mvp.md` §5](21-fabric-switch-mvp.md) |
| **IO-DMA → Host CPU** | PCIe Gen6 TLP | PCIe TLP | [`21-dma-backends-mvp.md` §3.4 (v1.1+)](21-dma-backends-mvp.md) |
| **Scale-Up Switch → CXL Memory Pool** | UALink Flit → CXL.mem Flit (PTE 转换) | CXL.mem Response | [`21-fabric-switch-mvp.md` §8.4](21-fabric-switch-mvp.md) |
| **CIU → GPC/Memory/Fabric (APB)** | APB (32-bit data + 32-bit addr) | N/A | [`21-tee-udd-mvp.md` §3.4](21-tee-udd-mvp.md) |

---

## §2 关键模块重新划分 (V3.1-Rev2.0)

### §2.1 GPC Subsystem 内部组成

```
GPC (Graphics Processing Cluster) — 计算集群单元:
┌──────────────────────────────────────────────────────────────┐
│  GPC x N (N = 8 typical)                                       │
│                                                              │
│  ┌──────────────────────────────────────────────┐          │
│  │ SM Cluster (per GPC: 16-32 SMs)              │          │
│  │  - SM: LSU + RF + Warp Scheduler + Tensor Core│          │
│  │  - LSU ↔ GMMU (per-SM L1 TLB, 64 entries)   │          │
│  └──────────────────────────────────────────────┘          │
│                          │                                    │
│  ┌──────────────────────────────────────────────┐          │
│  │ TC-DMA × 2-8 (TMA Engine, **GPC 内紧耦合**) │          │
│  │  - 描述符预取 + AGU + Swizzle + mbarrier     │          │
│  │  - 直达 SMEM (256B 旁路 L2)                 │          │
│  │  - **不直接连接 HBM, 必须经 GPC UDD Agent** │          │
│  └──────────────────────────────────────────────┘          │
│                          │                                    │
│  ┌──────────────────────────────────────────────┐          │
│  │ UDD Agent (per-GPC, HRT 查表)                │          │
│  │  - WRR 仲裁 (TC-DMA:SM = 4:1)               │          │
│  │  - HRT 双 Bank + Atomic Swap                │          │
│  │  - NoC Injection FIFO                       │          │
│  └──────────────────────────────────────────────┘          │
│                          │                                    │
│  ┌──────────────────────────────────────────────┐          │
│  │ L2 Cache Slice (per-GPC, 64-256 KB)         │          │
│  └──────────────────────────────────────────────┘          │
└──────────────────────────────────────────────────────────────┘
```

### §2.2 Memory Subsystem 严格剥离 TC-DMA

```
Memory Subsystem — 严格剥离 TC-DMA:
┌──────────────────────────────────────────────────────────────┐
│  HBM-DMA × N (HBM3e Controllers)                             │
│                                                              │
│  ┌──────────────────────────────────────────────┐          │
│  │ HBM-DMA (per Backends 文档)                   │          │
│  │  - GUPA → PC/BG/Bank/Row/Col 解码            │          │
│  │  - FR-FCFS 调度 (First-Ready)                │          │
│  │  - SECDED ECC + Page Retirement              │          │
│  └──────────────────────────────────────────────┘          │
│                          │                                    │
│                          ▼                                    │
│  ┌──────────────────────────────────────────────┐          │
│  │ HBM3e Stacks (4H/8H stacks, 16 PC)            │          │
│  └──────────────────────────────────────────────┘          │
│                                                              │
│  * 错误理解纠正：V3.0 拓扑曾把 TC-DMA 放在此层             │
│  * TC-DMA 已正确归属 GPC Subsystem (§2.1)                    │
└──────────────────────────────────────────────────────────────┘
```

### §2.3 Fabric & Edge IO Subsystem 补全

```
Fabric & Edge IO Subsystem — 互联 + 边缘 IO (V3.1-Rev2.0 新增 IO-DMA):
┌──────────────────────────────────────────────────────────────────┐
│  Fabric & Edge IO Subsystem (Fabric 子系统 + 边缘 IO 子系统合并)  │
│                                                                  │
│  ┌──────────────────────────────────────────────┐              │
│  │ Global UDD Hub (Fabric 子系统核心)            │              │
│  │  - RCT 多租户隔离 + HRT 路由仲裁              │              │
│  │  - Per-Backend Credit 管理                   │              │
│  └─────┬──────────────┬─────────────┬────────────┘              │
│        │              │             │                            │
│  ┌─────▼─────┐  ┌─────▼──────┐  ┌──▼───────────┐              │
│  │ HBM-DMA   │  │ NIC-DMA    │  │ IO-DMA       │ ← V3.1 新增 │
│  │ (in MM   │  │(UALink/    │  │(PCIe Gen6/   │              │
│  │  Subsys) │  │ Scale-Up)  │  │ CXL.io Host) │              │
│  └───────────┘  └──────┬─────┘  └──────┬───────┘              │
│                         │              │                        │
│                         ▼              ▼                        │
│                   [UALink x16]   [PCIe Gen6 x16]                │
│                         │              │                        │
└─────────────────────────┼──────────────┼────────────────────────┘
                          │              │
                          ▼              ▼
              [Scale-Up Switch]  [Host CPU / NVMe]
                          │
                          ▼ (UALink → CXL.mem 协议转换)
                  [CXL Memory Pool]
```

### §2.4 Control Subsystem

```
Control Subsystem — 配置与安全控制岛:
┌──────────────────────────────────────────────────────────────┐
│  Control Subsystem (运行在 clk_cfg 100MHz)                   │
│                                                                  │
│  ┌──────────────────────────────────────────────┐            │
│  │ CIU (Configuration & Isolation Unit)         │            │
│  │  - 接受 PCIe MMIO (Host CPU / FM 经 Driver) │            │
│  │  - HRT Shadow Bank 写入 + Atomic Swap 触发   │            │
│  │  - RAS 异常拦截                              │            │
│  └──────────────────────────────────────────────┘            │
│                                                                  │
│  ┌──────────────────────────────────────────────┐            │
│  │ AWT Controller (Asynchronous Warp Trap)       │            │
│  │  - 拦截 Poison / 越权访问                    │            │
│  │  - 挂起 Warp (status = TRAPPED)              │            │
│  │  - 保存 PC + GPR 摘要                       │            │
│  └──────────────────────────────────────────────┘            │
│                                                                  │
│  ┌──────────────────────────────────────────────┐            │
│  │ GMMU Hub (per-GPC GMMU 协同)                  │            │
│  │  - 接受 Driver 配置的 host 页表根              │            │
│  │  - HW PTW walker 调度                        │            │
│  └──────────────────────────────────────────────┘            │
│                                                                  │
│  ┌──────────────────────────────────────────────┐            │
│  │ Boot ROM                                       │            │
│  └──────────────────────────────────────────────┘            │
└──────────────────────────────────────────────────────────────┘
```

### §2.5 External (GPU Die 之外) — V3.1-Rev2.0 显式标注

```
External (GPU Die 之外) — 物理上不在 GPU Die:
┌──────────────────────────────────────────────────────────────┐
│  ┌──────────────────────────────────────────────┐            │
│  │ Scale-Up Switch (外部设备)                    │            │
│  │  - 双重角色:                                   │            │
│  │    ① UALink Fabric (GPU-to-GPU 路由)          │            │
│  │    ② CXL Expander Controller (CXL.mem 转换)  │            │
│  │  - N 个 UALink x16 端口 (GPU 侧)              │            │
│  │  - M 个 CXL 3.0 x16 端口 (Memory 侧)          │            │
│  │  - Sideband/Management (SMBus/I2C)           │            │
│  └────────────┬─────────────────────────────────┘            │
│               │ CXL 3.0                                       │
│               ▼                                                │
│  ┌──────────────────────────────────────────────┐            │
│  │ CXL Memory Pool (独立设备, 不在 GPU Die)       │            │
│  │  - DDR5 / HBM Expander                        │            │
│  │  - CXL Type-3 HDM Device                      │            │
│  └──────────────────────────────────────────────┘            │
│                                                              │
│  ┌──────────────────────────────────────────────┐            │
│  │ Host CPU (外部 PCIe 端点)                     │            │
│  │  - Driver 协调 (经 PCIe MMIO + DMA)           │            │
│  │  - NVMe (GPU Direct Storage 路径)            │            │
│  └──────────────────────────────────────────────┘            │
└──────────────────────────────────────────────────────────────┘
```

---

## §3 关键数据流路径修正说明

### §3.1 TMA 跨域拷贝路径 (修正后, V3.1-Rev2.0)

```
TMA 跨域拷贝完整路径 (5 步):

1. SM Warp 下发 TMA 描述符 → GPC 内的 TC-DMA
   (描述符含 5D Tensor: Base, Shape[5], Stride[5], Offset[5], Tile_Size)

2. TC-DMA 解析描述符, 向 GPC 内 UDD Agent 发起 Burst Read Micro-op
   (通过 tc_udd_req_* 接口, per `21-microarch-ifc-mvp.md` §5.2)

3. UDD Agent 查 HRT, 若目标为 Local HBM:
   - HRT 查表 → Route_Tag=0x0 (HBM)
   - 经 Compute NoC → Global UDD Hub → HBM-DMA

4. HBM-DMA 返回数据 → 经原路回到 TC-DMA
   - UDD Response Flit (Response + Data)
   - 经 Compute NoC 返回 → GPC UDD Agent → TC-DMA

5. TC-DMA 将数据直接写入本地 SMEM (旁路 L2)
   - 通过 smem_direct_out 接口 (per `21-microarch-ifc-mvp.md` §5.2)
   - 同一个 clk_core 周期内写入 256B
   - 硬件触发 udd_tc_mbarrier_trig (下一个 clk_core 上升沿)
   - SM Warp 通过 mbarrier.wait 唤醒

关键点:
- TC-DMA 始终是请求的 Initiator 和数据的 Sink
- TC-DMA 通过 GPC UDD Agent 间接访问 HBM, 而非直连
- TC-DMA 不经过 Global UDD Hub (在 GPC 内闭环)
- 但 HBM 访问仍需经 UDD Agent → Compute NoC → Global UDD → HBM-DMA 路径
```

### §3.2 CXL 内存池访存路径 (修正后, V3.1-Rev2.0)

```
CXL Memory Pool 访存完整路径 (6 步):

1. SM 发起 Load 请求
   - VA → TEE Frontend → GMMU 翻译 → GUPA
   - GUPA 高位 [63:48] 落在 Scale-Up 域 (per HRT Route_Tag = SWITCH_PORT_N)
   * 错误纠正: HRT 中**没有** "CXL_DIRECT" Route_Tag

2. UDD Agent 查 HRT → Route_Tag = SWITCH_PORT_N (NIC-DMA Port N)
   - HRT 告诉 UDD: "这个 GUPA 前缀应该发给哪个 Switch Port"
   - CXL 地址范围的映射完全由 Scale-Up Switch 的路由表决定

3. UDD Agent 路由至 NIC-DMA (Port N) (经 Global UDD Hub 仲裁)

4. NIC-DMA 将 Micro-op 封装为 UALink Mem-Read Flit
   - flit.header.DstNode = Scale-Up Switch 节点
   - 发往外部 Scale-Up Switch

5. Scale-Up Switch 内部执行协议转换 (PTE 引擎):
   - 解析 UALink Flit → UALink Mem-Read
   - 查 Switch PTE Base/Limit 寄存器 → 目标为 CXL Memory Pool
   - 转换为 CXL.mem Read Flit → 发往 CXL Memory Pool

6. CXL Memory Pool 返回数据 → Switch PTE 转换为 UALink Mem-Response
   → 返回 GPU NIC-DMA → 解封装 → 注入 UDD Response 通路 → SM

关键点:
- GPU 对 CXL 内存的访问是 **"Native UALink + Remote CXL Translation"**
- GPU 无需实现 CXL.mem 协议栈, 仅需支持 UALink 语义
- CXL Poison / Back-Invalidate 等特性由 Switch 映射为 UALink 等效信号
- 详见 `21-fabric-switch-mvp.md` §7.1 (4 层 Poison 编码映射)
```

### §3.3 IO-DMA / PCIe 路径 (新增, V3.1-Rev2.0)

```
IO-DMA 完整路径:

Host CPU 发起 DMA / PCIe Peer-to-Peer
  ↓ (PCIe Gen6 TLP)
[PCIe x16 PHY (Die 边界)]
  ↓
IO-DMA (Fabric & Edge IO Subsystem, per `21-dma-backends-mvp.md` §3.4)
  - TLP Parser/Generator
  - ATS Translation Request (SVA VA → GMMU, v1.1+)
  - PRI Page Request (Page Fault → PCIe PRI TLP, v1.1+)
  - GDS Zero-Copy Bypass (NVMe P2P Write → HBM-DMA, v1.1+)
  ↓
Memory & IO NoC (注入 UDD Micro-op, Route_Tag=0xF)
  ↓
Global UDD Hub (RCT 校验 + Credit 申请)
  ↓
GPC/HBM (经 Compute NoC)

IO-DMA vs NIC-DMA 关键区别:

| 维度         | IO-DMA                          | NIC-DMA                          |
|--------------|--------------------------------|----------------------------------|
| 面向对象     | Host CPU + NVMe                 | Peer GPU + Scale-Up Switch        |
| 协议         | PCIe Gen6 TLP                   | UALink 1.1 Mem Flit              |
| 延迟容忍     | 微秒级                          | 纳秒级                           |
| ATS/PRI      | ✅ (v1.1+)                      | ❌ (UALink 无 ATS)                |
| 集合通信     | ❌                               | ✅ (Multicast 硬件加速, v3.0+)    |
| 中断机制     | MSI-X 向量                      | UALink Response Flit Status     |
| RAS Poison   | Host_IO_Fault (Status=0x4)     | Remote_CXL_Poison (via Switch)   |
```

---

## §4 控制流修正说明

### §4.1 HRT 中 Route_Tag 含义变更

```
V3.1-Rev2.0 HRT Route_Tag 分配 (关键变更):

| Route_Tag     | 目标 Backend        | 后续处理                              |
|---------------|---------------------|---------------------------------------|
| 0x0           | HBM-DMA             | 直接转换为 HBM3e Command              |
| 0x1 ~ 0xE     | NIC-DMA (Port N)    | 封装为 UALink Flit → Scale-Up Switch  |
| 0xF           | IO-DMA              | 封装为 PCIe TLP                       |

**关键变更**:
- ❌ 移除 "CXL_DIRECT" Route_Tag (V3.0 错误理解)
- ✅ CXL Memory Pool 访问统一通过 NIC-DMA + Scale-Up Switch 间接完成
- ✅ HRT 仅告诉 UDD "这个 GUPA 前缀应该发给哪个 Switch Port"
- ✅ CXL 地址范围的具体映射由 Scale-Up Switch 的 PTE 路由表决定

CXL 地址空间在 HRT 中的体现:
- HRT 中 GUPA 高位 [63:48] 落在 Scale-Up 域 (per GUPA 空间划分, v1.0 MVP 默认 0x1000-0x7FFF)
- HRT 查表 → Route_Tag = 0x1 (NIC-DMA Port 0)
- NIC-DMA 封装为 UALink Flit → 发往 Scale-Up Switch
- Scale-Up Switch 内部 PTE 根据 Switch 路由表判断是 Peer GPU HBM 还是 CXL Memory Pool
- GPU 不感知目标是 Peer GPU 还是 CXL Memory (per `21-fabric-switch-mvp.md` §2.2)
```

### §4.2 CXL Drain 两阶段协同机制

```
CXL Drain 三方协调机制 (V3.1-Rev2.0 修正):

旧机制 (错误): GPU 内部 CXL Bridge 维护 Outstanding Counter
新机制 (正确): GPU-Switch 协同 + FM 三方协调

阶段 0: FM 决策 (Fabric Manager)
  - FM 决定需要 Drain (CXL 内存池热插拔 / 链路重训练 / 故障隔离)
  - FM 写 NIC_DRAIN_CTRL.START_DRAIN (per `21-microarch-ifc-mvp.md` §4.2)

阶段 1: GPU → Switch (QUIESCE)
  - NIC-DMA 立即拒绝新 Tx Micro-op (返回 Abort 给上游)
  - NIC-DMA 通过 UALink 控制通道 (VC0) 向 Switch 发送 CTRL_QUIESCE
  - NIC-DMA 启动 100us 硬件定时器

阶段 2: Switch 排空
  - Switch PTE 停止向目标 CXL Port 发新 CXL.mem Req
  - Switch 等待目标 CXL Port 的 Outstanding Counter == 0
  - Switch 通过 UALink VC0 返回 CTRL_Q_ACK 给 NIC-DMA

阶段 3: GPU 本地排空
  - NIC-DMA 收到 CTRL_Q_ACK → 进入 DRAIN_LOCAL
  - NIC-DMA 等待本地 ROB 归零 (per `21-dma-backends-mvp.md` §6.4)
  - rob_ valid entries == 0 → drain_state_ = IDLE

阶段 4: Switch 通知 GPU FM
  - Switch PTE 通知 FM (经 Sideband SMBus)
  - FM 确认 Switch CXL 侧已排空

阶段 5: FM 通知 NIC-DMA 恢复
  - FM 写 NIC_DRAIN_CTRL.FORCE_ABORT = 1 (强制清零 Counter) 或
  - FM 写 NIC_DRAIN_CTRL.START_DRAIN = 0 (恢复正常流量)

超时保护: 若 NIC-DMA 在 100us 内未收到 CTRL_Q_ACK, 触发 Fatal RAS 中断

关键点: Drain 变为 GPU-Switch-FM 三方协调的两阶段协议, 而非 GPU 单端行为
```

### §4.3 AWT Trap Type 含义修正

```
V3.1-Rev2.0 AWT Trap Type 定义 (per `21-microarch-ifc-mvp.md` §7.6):

| Trap Type | 含义                              | 来源识别                              |
|-----------|-----------------------------------|---------------------------------------|
| 0x0       | LOCAL_HBM_POISON                  | HBM-DMA 内 SECDED DED / Page Retire   |
| 0x1       | REMOTE_CXL_POISON_VIA_SWITCH      | NIC-DMA 收到 UALink Remote_Error Flit |
| 0x2       | SECURITY_VIOLATION (RCT 越权)     | Global UDD Hub RCT 校验失败           |
| 0x3       | PCIE_IO_ABORT                     | IO-DMA PCIe Link Down / UR (v1.1+)    |

关键修正: REMOTE_CXL_POISON_VIA_SWITCH 显式标注 "via Switch"
- 表明 Poison 来源是外部 CXL 内存 (通过 Scale-Up Switch 转换)
- 而非 GPU 直连 CXL (V3.0 错误理解)
- 处理流程见 `21-fabric-switch-mvp.md` §7.2
```

---

## §5 模块接口修订影响 (RTL-IFC 联动)

### §5.1 必改的 RTL-IFC 项

```
V3.1-Rev2.0 RTL-IFC 修订影响:

1. 【移除】UDD ↔ CXL Bridge 接口定义
   - V3.0 拓扑曾定义 UDD Agent → CXL Bridge (片上)
   - V3.1 移除此接口 (GPU Die 上无 CXL Bridge)
   - 替代: UDD Agent → NIC-DMA (UALink Flit → Scale-Up Switch)

2. 【新增】TC-DMA ↔ UDD Agent (GPC 内) 接口
   - tc_udd_req_* (160-bit Micro-op)
   - udd_tc_resp_* (256B data + status + mbarrier trigger)
   - per `21-microarch-ifc-mvp.md` §5.2

3. 【新增】IO-DMA ↔ Memory & IO NoC 接口
   - udd_be_req_* (per `21-microarch-ifc-mvp.md` §5.4)
   - IO-DMA 特定寄存器: PCIE_CTRL / PCIE_STATUS / GDS_CTRL
   - per `21-microarch-ifc-mvp.md` §7.5

4. 【修改】NIC-DMA 接口
   - 增加 UALink 协议相关信号 (per `21-fabric-switch-mvp.md` §5.1)
   - 移除原生 CXL 信号 (V3.0 错误设计)
   - 仅保留 UALink Flit + Drain FSM 信号

5. 【修改】AWT Trap Payload
   - 0x1 Trap Type 名称从 "Remote CXL Poison" 改为 "REMOTE_CXL_POISON_VIA_SWITCH"
   - 显式标注 Poison 来源是 Scale-Up Switch (非 GPU 直连 CXL)
   - per `21-microarch-ifc-mvp.md` §7.6
```

### §5.2 接口修改影响矩阵

| 接口 | V3.0 状态 | V3.1-Rev2.0 状态 | 影响模块 | 文档 SSOT |
|------|----------|-------------------|---------|----------|
| `UDD ↔ CXL Bridge` | 已定义 | **移除** | NIC-DMA, UDD Agent | `21-fabric-switch-mvp.md` §1.2 |
| `TC-DMA ↔ UDD Agent` | 已定义 | 修正: GPC 内紧耦合 | TC-DMA, GPC UDD Agent | `21-microarch-ifc-mvp.md` §5.2 |
| `IO-DMA ↔ Mem & IO NoC` | 缺失 | **新增** | IO-DMA, Global Hub | `21-dma-backends-mvp.md` §3.4 |
| `NIC-DMA ↔ Scale-Up Switch` | 已定义 | 修正: 仅 UALink Flit | NIC-DMA, Switch PTE | `21-fabric-switch-mvp.md` §5 |
| `AWT Trap Payload[7:0]` | 已定义 | 修正: 0x1 名称明确 | AWT Controller, GPC | `21-microarch-ifc-mvp.md` §7.6 |

---

## §6 v1.0 MVP 端到端 Shippable Demo (10 步测试)

```
v1.0 MVP Test: test_soc_topology_e2e

1. Driver 经 PCIe MMIO 写 UDD HRT (per `21-tee-udd-mvp.md` §2)
   - CIU_REG_HRT_SHADOW[0] = {Route_Tag=0x0(本地HBM), VC=0, QoS=Normal}
   - CIU_REG_HRT_CTRL.SWAP_BIT = 1
   - HRT Atomic Swap ≤8 cycles @ clk_core

2. SM 发起 Load 请求 (Local HBM)
   - sm_udd_req_valid=1, payload={VA=0x1000_0000, size=128B, ctx=0}
   - TEE Frontend.Fetch + RCT 预校验通过

3. GMMU 翻译
   - L1 TLB miss → HW PTW walker (4 cycles)
   - 返回 GUPA=0x8000_0000

4. UDD Agent HRT 查表
   - GUPA[63:48]=0x8000 → HRT 命中
   - Route_Tag=0x0 (HBM), VC=0, QoS=Normal
   - AGU 拆分 128B → 1 个 128B Micro-op

5. Compute NoC 传输 → Global UDD Hub
   - WRR 仲裁 (TC:SM=4:1)
   - RCT 校验通过 + Credit[backend=HBM-DMA] = 128 → 127
   - 注入 backend_out[HBM-DMA]

6. HBM-DMA 处理 (Memory Subsystem, per `21-dma-backends-mvp.md` §5)
   - Address Decoder → PC=0, BG=0, Bank=3
   - FR-FCFS 调度 (Row Hit 优先)
   - HBM3e PHY Command → 数据返回
   - ECC SECDED 验证

7. UDD Response 返回 GPC
   - 组装 UddResponse + Tracker_ID 匹配
   - Credit[backend=HBM-DMA] = 127 → 128 (归还)

8. TEE Frontend 接收 → 写回 SM Register File

9. SM 读到 expected value

10. 验证 SoC 拓扑正确性
    - TC-DMA 物理归属 GPC 内 (不直连 HBM)
    - IO-DMA 在 Fabric & Edge IO Subsystem (与 NIC-DMA 平级)
    - CXL Memory Pool 不在 GPU Die 上 (在 Scale-Up Switch 下)
    - HRT 无 "CXL_DIRECT" Route_Tag (统一通过 SWITCH_PORT_N)
```

**验收标准**: 10 步全部通过 + 测试用例 ≥ 10 个 (per 各子系统 MVP 文档)。

---

## §7 无债务演进约束 (3 条不变量)

### §7.1 不变量 1: TC-DMA 物理归属 GPC 内 (不可变)

> v1.0 MVP ship 后, TC-DMA 物理位置固定在 GPC 内 (与 SMEM/L2 紧耦合)。
>
> v1.1+ 演进时**不**可将 TC-DMA 移出 GPC (否则破坏 mbarrier 硬件触发确定性延迟约束, per `21-microarch-ifc-mvp.md` §9.1)。

### §7.2 不变量 2: GPU Die 上无 CXL PHY/Controller (严格剥离)

> v1.0 MVP ship 后, GPU Die 上**不**包含任何 CXL PHY/CXL Controller/CXL Bridge。
>
> 所有跨节点 CXL 内存访存必须通过 NIC-DMA (UALink) → 外部 Scale-Up Switch → CXL Memory Pool。
> v1.1+ 演进时**不**可在 GPU Die 上添加 CXL 协议栈 (违背"GPU 协议栈极简"原则, per `21-fabric-switch-mvp.md` §1.2)。

### §7.3 不变量 3: HRT 无 CXL_DIRECT Route_Tag (统一通过 SWITCH_PORT_N)

> v1.0 MVP ship 后, HRT Route_Tag 分配固定为 0x0 (HBM) + 0x1~0xE (UALink Port) + 0xF (PCIe)。
>
> v1.1+ 演进时**不**可新增 "CXL_DIRECT" Route_Tag (破坏 GPU-Switch 协议层解耦)。
> 所有 CXL Memory Pool 访问统一通过 SWITCH_PORT_N → NIC-DMA → Scale-Up Switch (PTE 内部转换)。

---

## §8 边界与限制 (SoC 顶层)

### §8.1 SoC 不实现的功能 (v1.0 MVP 明确边界)

- ❌ **GPU Die 上无 CXL PHY/Controller** (per §7.2 不变量 2)
- ❌ **TC-DMA 不直连 HBM** (per §7.1 不变量 1)
- ❌ **HRT 无 CXL_DIRECT Route_Tag** (per §7.3 不变量 3)
- ❌ **Scale-Up Switch PTE 不在 GPU Die** (在外部 Switch 设备)
- ❌ **CXL Memory Pool 不在 GPU Die** (在外部独立设备)
- ❌ **Host CPU 不在 GPU Die** (在外部 PCIe 端点)

### §8.2 已知限制

- 五大子系统物理边界在 v1.0 MVP 冻结, 不支持运行时动态调整
- GPC 数量 v1.0 MVP 默认 8, 可调整 4-16 (RTL 重映射需求)
- HBM-DMA 数量 v1.0 MVP 默认 2, 对应 4H/8H stacks
- NIC-DMA Port 数量 v1.0 MVP 默认 4 (per `21-fabric-switch-mvp.md` §8.2)
- IO-DMA Port 数量 v1.0 MVP 默认 1 (x16, v1.1+ 可扩展 2 Port)

---

## §9 跨仓契约

### §9.1 UsrLinuxEmu 侧 (不修改 23 ABI)

SoC Topology v1.0 MVP **不引入任何新 CppTLM ABI 函数**——所有 driver↔GPU 通信通过**现有 23 ABI 函数** (`syms->mmio_write/read`).

### §9.2 CppTLM 侧 (新模块 + 命名变更)

| 文件 | 状态 |
|------|------|
| `docs/soc_arch/architecture/21-soc-topology-mvp.md` | **新建** (本规范) |
| `docs/soc_arch/architecture/21-soc-topology-evolution-roadmap.md` | **预留** (v1.1 创建) |
| `src/tlm/soc/soc_shell.cc` | **新建** (SoC 顶层注入拓扑) |
| `include/tlm/soc/soc_topology_defs.hh` | **新建** (子系统边界宏定义) |
| `test/test_soc_topology_e2e.cc` | **新建** (SoC 端到端 demo) |
| **23 ABI 头冻结** | ✅ **不变** (per ADR-088 §D5) |

### §9.3 跨仓 PR 协调

按 ADR-091 §R5.1 跨仓 PR 流程:
1. CppTLM 仓: 实现 SoC 顶层拓扑注入 + 10+ 测试
2. UsrLinuxEmu 仓: driver 适配新拓扑 (由用户承担协调)
3. 跨仓集成测试: `test_soc_topology_e2e_ue.cc`
4. 同步 PR (无 ABI 影响, 跨仓风险低)

---

## §10 反模式 (明确不做)

| 反模式 | 不做的原因 |
|--------|-----------|
| ❌ **GPU Die 内放置 CXL Controller** | 违背"GPU 协议栈极简"原则 (per §7.2 不变量), Die 面积浪费 3-5% |
| ❌ **TC-DMA 与 HBM-DMA 平级摆放** | 违背"TC-DMA 物理归属 GPC"原则 (per §7.1 不变量), 破坏 mbarrier 确定性 |
| ❌ **HRT 中新增 CXL_DIRECT Route_Tag** | 违背"HRT 无 CXL_DIRECT"原则 (per §7.3 不变量), 破坏协议层解耦 |
| ❌ **省略 IO-DMA** | 违背"IO 闭环"原则, Host 通信 + GDS 路径缺失 |
| ❌ **CXL Memory Pool 放在 GPU Die 拓扑图** | 违背"GPU Die 边界"原则, 误导 RTL 集成 |
| ❌ **TC-DMA 与 HBM 直连** | 违背 TMA 跨域 5 步路径, 性能 regression + 失去 mbarrier 同步 |
| ❌ **Scale-Up Switch 内部 PTE 集成在 GPU Die** | 违背"GPU 协议栈极简"原则, Die 面积压力 |
| ❌ **Drain 协议由 GPU 单端主导** | 违背"三方协调"原则 (per §4.2), 易死锁 |

---

## §11 引用

### §11.1 内部引用

- [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) — TEE + UDD MVP (核心数据面)
- [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) — UBC + HBM/NIC/IO-DMA MVP (后端物理实现)
- [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) — Fabric 协议 + Scale-Up Switch MVP (互联契约层)
- [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) — Core/Micro-Arch + RTL-IFC MVP (实现层契约)
- [`21-tee-udd-evolution-roadmap.md`](21-tee-udd-evolution-roadmap.md) — TEE-UDD 演进路线图
- [`00-overview.md`](00-overview.md) §3 SoC 顶层架构
- [`16-pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT

### §11.2 跨仓引用

- UsrLinuxEmu AGENTS.md §CppTLM 通信架构 — **PCIe-only 原则**
- UsrLinuxEmu ADR-088 §D5 — 23 ABI 冻结

### §11.3 MAS-3.1 上游原始设计

- `MAS-3.1-SoC Rev2.0` (SoC 顶层物理布局原始设计)
- `MAS-3.1-DMA-TEE Rev2.0` (TEE 任务执行层)
- `MAS-3.1-DMA-Backends Rev2.0` (Backends 后端)
- `MAS-3.1-UDD-MAS Rev2.0` (UDD 地址空间)
- `MAS-3.1-Fabric/Protocol Rev2.0` (Fabric 协议)
- `MAS-3.1-Core/Micro-Arch Rev2.0` (Core 微架构)
- `MAS-3.1-RTL-IFC Rev2.0` (RTL 接口)

### §11.4 业界参考

- NVIDIA Hopper dGPU SoC Topology (GPC + HBM + PCIe + NVLink)
- AMD CDNA3 SoC (CU + HBM + PCIe + Infinity Fabric)
- Intel Xe-HPC (Tile + Fabric + CXL)
- ARM CMN-700 + HN-F (NoC + Memory Controller)

---

## §12 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: SoC 顶层物理布局规范 V3.1-Rev2.0 (5 大子系统划分 + 修正后拓扑图 + 3 条不变量 + TMA/CXL/IO 三路径修正 + RTL-IFC 联动影响 + 10 步 shippable demo) |

---

**关联 OpenSpec change**: 待 `openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/` 提案创建
**下次更新**: Oracle 评审反馈后 v1.1 (v1.1+ 演进路线图创建)