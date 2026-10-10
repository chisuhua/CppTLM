# minimal_v1 vs timing_v1 — 数据流 + 拓扑对比

> **状态 v0.4 简化版 (2026-10-09)**:
> - 简化范围: 移除 §13 (`dgpu_board_v1.json` 早期 Board 视角代次对照, 重复内容) + §4 (cycle-by-cycle 时序细节已分流到 timing-mode.md §3) + §11/§12 (概要表, 与 architecture.md §0.1 + timing-mode.md §2 重叠)
> - **核心对比保留**: §1 主对比表 + §2 拓扑图 + §3 数据流节点

> **业务架构视图分层定位**:
> - 主架构: [`./architecture.md`](./architecture.md) §0.1 (业务定位) + §1.4 (仿真模式声明 v0.4) + §0.2 (Gap 修复路线)
> - timing-mode: [`./timing-mode.md`](./timing-mode.md) (平行视图, cycle-approximate 全设计)
> - 配套: [D-AXI v1.4](../pcie/driver-visible-minimal-soc.md) + [ADR-DGPU-11](../adr/ADR-DGPU-11-timing-mode-soc-scope.md)

## 1. 一图概览 (TL;DR, v0.4 修订)

| 维度 | **`dgpu_soc_minimal_v1.json`** (mixed-mode SoC) | **`dgpu_soc_timing_v1.json`** (timing-mode) |
|------|-----------------------------|---------------------------|
| **当前 vs 目标** | **目标 (per Change A)**: `vram_ctrl` 顶层节点 + `simulation_mode` 字段 **尚未实施** (Change A `cpptlm-vram-controller-functional-integration` P0 起草完毕待实施); 本列含目标态描述 | **当前实现** (T1-T8 已实施, 2027-02-11) |
| **仿真模式** | mixed-mode (per §1.4 修订): `VramControllerTLM` 单独 functional-mode, 其他模块 cycle-approximate | timing-mode (`"simulation_mode": "timing"`), 全 SoC cycle-approximate |
| **模块数** | **6** (+ `vram_ctrl` 顶层 per `cpptlm-vram-controller-functional-integration` Change A v1.0.1) | **7** (+ `vram_ctrl` + `soc` 共存) |
| **SDMA 数据 path** | `sdma.mem_out` 经 `SocInternalNoc` mesh → `vram_ctrl` (per §0.1 原则 4 修订 + Change C `cpptlm-gmmu-pte-bus-correction`) | 同上 + cycle accounting |
| **BAR 路由** | 4 BAR (BAR0 MMIO + BAR1 fast-path + BAR2 vram aperture) | 同 (mode 差异) |
| **Backing** | `DGpuBoard::vram_storage_` 8GB unique_ptr (per ADR-DGPU-05 single source) | 同 (per D-AXI v1.4 B7 shared) |
| **GMMU 翻译** | sync 同步 (functional-mode, 翻译即返回 backing offset) | sync + tlb_miss_latency_cycles=50, tlb_size=32 |
| **VRAM 控制** | `VramControllerTLM::FunctionalZeroDelay` mode (zero-delay, 协议真实) | `VramControllerTLM::TimingCycleApproximate` mode (cycle accounting + bandwidth) |
| **routing flags** | 4 (`display=false`/`storage=true`/`gmmu=true`/`memory=true`) | 3 (display 默认 false) |
| **顶层 config 字段** | `description` + 4 routing flags + `simulation_mode="functional"` (per v0.4 修订) | + `simulation_mode="timing"` |
| **关联 spec / 实施 change** | `cpptlm-vram-controller-functional-integration` (Change A, P0) | `cpptlm-dgpu-soc-timing-mvp` (✅ archived 2027-02-11) |
| **tests cases** | `[minimal_dgpu_soc]` 12 cases / 66 assn + `[driver_visible]` 2 / 29 | `[dgpu_soc_timing]` 37 cases / 12024 assn |

**一句话差异**: minimal_v1 + `vram_ctrl` 顶层节点 + `simulation_mode` 字段 + VramController functional-mode = 与 timing_v1 共用同一 `VramControllerTLM` 类, 仅 mode 配置差异 (per ADR-DGPU-11 H2 互斥)。

## 2. 拓扑结构对比 (Graph View)

### 2.1 minimal_v1 拓扑 (mixed-mode)

```
soc (DGpuSoc SimModule)
├── pcie_ep         → PcieEndpointIP (BAR0/1/2 + MSI-X)
├── pcie_ctrl       → PcieControllerIP (4 BAR decode facade)
├── pcie_memory     → PcieMemoryDevice (chip-internal 2 SlavePort)
├── vram_ctrl       → VramControllerTLM (FunctionalZeroDelay mode, NEW per Change A)
├── sdma            → SdmaEngineTLM (5-port, 经 ChStream + SocInternalNoc)
├── gmmu            → GmmuTLM (1 L1 page walk, sync functional)
└── completion      → CompletionRingTLM (dormant state)

connections:
- sdma.2 → soc_internal_noc → pcie_memory.0 (per Change C Gap G2, DataNoC only)
- gmmu.0 → pcie_memory.1 (GMMU dedicated memory client, bypass mesh)
```

### 2.2 timing_v1 拓扑 (timing-mode)

```
soc (DGpuSoc SimModule)
├── pcie_ep         → PcieEndpointIP
├── pcie_ctrl       → PcieControllerIP
├── pcie_memory     → PcieMemoryDevice
├── vram_ctrl       → VramControllerTLM (TimingCycleApproximate mode + row buffer + bandwidth)
├── sdma            → SdmaEngineTLM (含 `h2d_pending_` outstanding + `pending_resps_` priority queue)
├── gmmu            → GmmuTLM (32-entry direct-map TLB + `translate_timing()` + cycle accounting)
├── completion      → CompletionRingTLM (fence → MSI-X)
└── [memory]        → MemoryTLM (legacy fallback)

connections:
- sdma.2 → soc_internal_noc → pcie_memory.0 (DataNoC, with cycles)
- gmmu.0 → pcie_memory.1 (GMMU dedicated memory client)
```

### 2.3 拓扑对比 Side-by-Side

| 维度 | minimal_v1 (mixed) | timing_v1 (timing) |
|------|---------------------|----------------------|
| **soc_internal_noc** | ✅ 启用 (per Change C, P2 待实施) | ✅ 启用 (✅ 实施 T1-T8, 2027-02-11) |
| **VramControllerTLM** | ✅ FunctionalZeroDelay | ✅ TimingCycleApproximate |
| **Backing storage** | `DGpuBoard::vram_storage_` (single source) | 同 |
| **显式 connections** | 2 (sdma+NOC, gmmu bypass) | 2 (per H2: 0 新增) |

## 3. 数据流节点对比

### 3.1 minimal_v1 数据流 (mixed-mode)

```
SDMA H2D/D2H (per §1.4.3):
    sdma.mem_out (AxiMemBundle, ChStream interface required)  ←── HARD CONSTRAINT
        │
        ▼ 经 soc_internal_noc mesh (per Change C, Gap G2)
    vram_noc_adapter 接受 NoCFlitBundle
        │
        ▼ AXI4-like 字段映射 (per HBM3 Controller 真协议)
    vram_ctrl->handle_functional_request(addr, size, is_write, data)
        │
        ▼ VramController functional-mode (zero-delay, 协议真实, per ADR-DGPU-11 H2)
    memcpy(vram_storage_)  ←── SDMA 不再直接 memcpy, 而经 VramController 协议层

GMMU PTE fetch (per Gap I, dedicated memory client):
    gmmu.translate(iova, ...) → PTE MEM_READ via chip-internal AXI
        │
        │ Gap I: GMMU dedicated memory client, BYPASS mesh NoC
        ▼
    gmmu.0 → pcie_memory.handle_slave_port(port=1) (直连, 不经 NOC)

Host backdoor 特权 (per §1.4.2):
    DGpuBoard::backdoor_read/write → vram_storage_[off..off+len] (直访, 类比 MemBackdoor)
```

### 3.2 timing_v1 数据流 (timing-mode)

```
SDMA H2D/D2H:
    sdma.mem_out → soc_internal_noc → vram_noc_adapter → vram_ctrl (cycle accounting)
        │
        ▼ VramController::TimingCycleApproximate mode
        - row_hit_cycles=30 / row_miss_cycles=80
        - bandwidth_gbps=32 (per kOperatingFrequencyHz = 1 GHz, latency = 1/bandwidth cycles)
        - pending_resps_ priority queue (FIFO ordered)

GMMU PTE walk:
    gmmu.translate_timing(iova, ...) → TLB hit (lat=0) 或 TLB miss → page walk (lat=tlb_miss_cycles=50)
    gmmu.0 → pcie_memory.handle_slave_port(port=1) (dedicated)
```

### 3.3 数据流关键节点对比

| 节点 | minimal_v1 行为 | timing_v1 行为 | §0.1 Gap 修复 |
|------|-----------------|----------------|--------------|
| `sdma.mem_out` (AxiMemBundle) | **必须实现 ChStream 接口** (per §1.4.3 HARD) | 同 + cycle accounting | Gap G1 (lambda) + Gap G2 (mesh NOC) |
| `soc_internal_noc` | DataNoC only | DataNoC + cycles | Gap G2 |
| `vram_ctrl` (functional) | zero-delay memcpy, 协议真实 | cycle accounting | Gap G3 (本期 Change A) |
| `gmmu.req_out` (AxiMemBundle) | chip-internal AXI, BYPASS mesh | 同 + latency | Gap I (Phase 1 Change C) |
| `DGpuBoard::backdoor_*` | 13 ABI 特权直访 | 13 ABI + cycle-traced | §1.4.2 backdoor 锁定 |

## 4. 修改状态 (Status 当前)

| 节点 | Gap 修复状态 |
|------|-------------|
| `sdma` 必须 ChStream 经 NOC | **Gap G2 (Phase 1 Change C 起草完毕, P2, 待实施)** |
| `vram_ctrl` 主数据 path | **Gap G3 (Phase 0 Change A 起草完毕, P0, 待实施)** |
| `gmmu` wire-format 修正 + NOC bypass | **Gap I (Phase 1 Change C 起草完毕)** |
| BAR0 CR-Bus / BAR1 Doorbell / MMIO Read | **Gap H + J + K (Phase 0 Change B 起草完毕, P1, 待实施)** |
| PCIe 入口 TLP | **Gap M (Phase 1 Change D 起草完毕, P2, 待实施)** |

详细 Gap 修复路线: [`./architecture.md` §0.2 总体实施计划](./architecture.md) (6 Changes / 4 Phases)

## 5. 维护规则

1. **修改前必读** §0.1 业务定位 + §1.4 仿真模式 v0.4 修订
2. **核心对比表 §1 与 architecture.md §0.1 同步**: 这 2 个表的 ["仿真模式"] 列可能需要同步更新 (v0.4 修订后 minimal_v1 仿真模式描述)
3. **不在本文档重复 architecture.md §0.1 §0.2 内容**: 一概指向指针
4. **拓扑图 (§2)** 与 phase 0 Change A 集成后 actual 集成结果同步 (`vram_ctrl` 顶层节点新增)
5. **数据流 (§3)** 与 architecture.md §0.1 §X §X SDMA 必须实现 ChStream 接口同步
