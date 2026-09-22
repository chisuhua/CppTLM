# 分布式 Scale-Up 拓扑规范 v0.1 (方案 B: 传统分布式, 非 NSA-aware)

> **目的**: 定义 CppTLM dGPU SoC **MAS-3.1 v3.1-Rev2.0** 向 **分布式 Scale-Up** 演进的 **中间过渡方案** — **方案 B** (传统分布式, **不依赖 NSA-aware 硬件**), 作为 **方案 C (NSA + 分布式)** 的**降级替代**或**演进过渡**。本规范不依赖 Remote Atomic Unit / Hardware Directory 等新硬件, 仅依赖 **PCIe over UALink 协议栈 + Switch Tray Linux 协调**, **2-3 年可量产**。
>
> **状态**: Draft v0.1 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: 待 `openspec/changes/2026-09-19-cpptlm-mas-dist-scale-up-b/` 提案
> **关联文档**:
> - [`21-soc-topology-mvp.md`](21-soc-topology-mvp.md) V3.1-Rev2.0 SoC 拓扑基础
> - [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) Fabric + Switch 协议 (方案 B 扩展)
> - [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) TEE-UDD 数据面 (方案 B 升级)
> - [`23-dist-scale-up-topology.md`](23-dist-scale-up-topology.md) **方案 C (NSA-aware 分布式)** 完整拓扑
> - [`28-cxl-3-fabric.md`](28-cxl-3-fabric.md) CXL 3.0 Fabric 兼容 (方案 C 增强)
> - [`29-nsa-evolution-roadmap.md`](29-nsa-evolution-roadmap.md) 5 阶段演进时间线

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

> **关联 ADR**: ADR-SOC-21 (V3.1-Rev2.0 拓扑修正)

---

## §0 阅读引导

- 想理解方案 B 定位 → 读 §1 (概述) + §2 (与方案 A/C 对比)
- 想看 Compute Tray 独立化方案 → 读 §3 (Compute Tray 独立 CPU + Linux)
- 想看 PCIe over UALink 协议 → 读 §4 (PCIe over UALink 协议栈)
- 想看 Switch Tray 协调机制 → 读 §5 (Switch Tray Linux 协调)
- 想看 v1.0 MVP 范围 → 读 §6 (v1.0 MVP 范围)
- 想看开放问题 → 读 §7 (开放问题)

---

## §1 概述

### §1.1 问题陈述

V3.1-Rev2.0 (`21-soc-topology-mvp.md`) 是 **1 个 Linux 节点 + 1 个 PCIe Hierarchy** 架构:
- Host Tray (双 CPU + 8 GPU 通过 PCIe Switch 桥接)
- Compute Tray (无 CPU, 8 GPU + 4 Switches)
- Switch Tray (ARM Cortex-M 固件)
- 单一 Linux OS 管理所有 8 GPU

但用户提出**分布式 Scale-Up** 设想 (per 第 10 轮讨论):
- 每 Compute Tray **独立 CPU + Linux OS**
- 多 Compute Tray 通过 **PCIe over UALink** 通信
- **无 Host Tray** (Host CPU 在每个 Compute Tray 上)
- GPU 通过 SR-IOV 共享给其他 Compute Tray
- Switch Tray **独立 Linux OS** 协调 CXL

本规范定义这一架构的 **方案 B (传统分布式)**: **不依赖 NSA-aware 硬件**, 仅依赖 PCIe over UALink 协议栈 + Switch Tray Linux 协调。

### §1.2 方案 A / B / C 三方对比

| 维度 | 方案 A (V3.1-Rev2.0) | **方案 B (本规范)** | 方案 C (NSA-aware) |
|------|---------------------|---------------------|---------------------|
| **架构成熟度** | 量产 (NVIDIA HGX) | **概念 + POC** | 概念 (2-4 年) |
| **Linux 节点数** | 1 (Host Tray) | **N+1** (N Compute + 1 Switch) | N+1 |
| **PCIe Hierarchy** | 1 个大树 | **N 个独立树** | N 个独立树 |
| **GPU Page Table** | 集中 Host DRAM | **分散各 Compute DRAM** | 分散 + NSA-aware |
| **跨 tray GPU↔GPU** | N/A | **1.8 μs (PCIe over UALink)** | 500 ns (NSA 目标) |
| **Host↔GPU** | 1 μs (PCIe Switch) | **1 μs (本地 PCIe Switch)** | 1 μs (本地) |
| **NSA-aware 硬件** | 不需要 | **不需要** | 需要 (Phase 2/3) |
| **PCIe over UALink 协议栈** | 不需要 | **需要** | 需要 |
| **Switch Tray Linux** | 不需要 (ARM 固件) | **需要** | 需要 |
| **量产时间窗** | 现在 ✅ | **2026-2027** ⚠️ | 2027+ ⚠️ |

### §1.3 方案 B 的核心权衡

```
优势:
  + 每 Compute Tray 独立故障域 (vs V3.1-Rev2.0 单点)
  + 弹性扩展 (按 Compute Tray 采购)
  + 多租户友好 (每 Compute Tray 独立租赁)
  + 不需要 NSA-aware 硬件 (相比方案 C 简单)
  + 2-3 年可量产 (PCIe over UALink + Switch Linux 已成熟)

代价:
  - 跨 tray GPU↔GPU ~1.8 μs (vs V3.1-Rev2.0 同 tray 600 ns, 慢 3×)
  - 跨 tray 带宽 ~256 GB/s (vs 同 tray 3.6 TB/s, 慢 14×)
  - 多 Linux 节点管理复杂 (GMMU/TLB 一致性)
  - PCIe over UALink 协议开销 (~500 ns × 2 = 1 μs)
  - 跨 Compute Tray KMD 协调 (driver 改动)
```

---

## §2 Compute Tray 独立化方案 (vs V3.1-Rev2.0)

### §2.1 Compute Tray 物理构成

```
┌─────────────────────────────────────────────────────────────────┐
│              Compute Tray (方案 B, 独立 CPU + Linux)          │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐│
│  │  CPU (新, per Compute Tray)                                ││
│  │  - Intel Xeon 或 AMD EPYC (单路或双路)                   ││
│  │  - PCIe Gen5 Root Complex                                 ││
│  │  - 256GB+ DDR5                                            ││
│  │  - 独立 Linux OS (per Compute Tray)                     ││
│  │  - 独立 KMD (per Compute Tray)                            ││
│  │  - 独立 Host FM daemon (可选)                              ││
│  └────────────────────────────────────────────────────────────┘│
│       │                                                          │
│       ▼ PCIe Switch 桥接器                                       │
│  ┌────────────────────────────────────────────────────────────┐│
│  │  8 GPU (per Compute Tray)                                ││
│  │  - 独立 PCIe Hierarchy (per Compute Tray)                ││
│  │  - GMMU + Page Table 在 Compute Tray DRAM                ││
│  │  - HRT / RCT / Credit 全量沿用 V3.1-Rev2.0              ││
│  └────────────────────────────────────────────────────────────┘│
│       │                                                          │
│       ▼ UALink Port 0 (上行)                                       │
│  ┌────────────────────────────────────────────────────────────┐│
│  │  4 Scale-Up Switches (per Compute Tray, NVSwitch 风格)   ││
│  │  - UALink MAC/PHY (per 21-fabric-switch-mvp.md §5.8)     ││
│  │  - 跨 tray 通信 (per §3)                                  ││
│  └────────────────────────────────────────────────────────────┘│
│       │                                                          │
│       ▼ UALink 上行 (至 Switch Tray)                               │
└─────────────────────────────────────────────────────────────────┘
```

### §2.2 与 V3.1-Rev2.0 Compute Tray 的关键差异

| 维度 | V3.1-Rev2.0 Compute Tray | 方案 B Compute Tray |
|------|------------------------|---------------------|
| **CPU** | ❌ 无 | ✅ Intel Xeon / AMD EPYC |
| **PCIe RC** | ❌ 无 (经 Host CPU RC) | ✅ 本地 PCIe RC |
| **Linux OS** | ❌ 无 | ✅ 独立 Linux (per Compute Tray) |
| **KMD** | ❌ 无 | ✅ 独立 KMD |
| **GMMU Page Table** | ❌ 无 (在 Host DRAM) | ✅ 在 Compute Tray DRAM |
| **Host FM daemon** | ❌ 无 | ✅ 可选 (本地 FM) |
| **PCIe Hierarchy** | 1 个 (Host Tray) | **N 个独立** |
| **跨 GPU Page Table 同步** | N/A | ⚠️ 需 PCIe over UALink 协议 |
| **SR-IOV GPU** | 不需要 | ✅ 可选 (v1.0 MVP 暂不实施) |

### §2.3 多 Linux 节点的 GMMU / TLB 一致性挑战

```
V3.1-Rev2.0 (1 Linux 节点):
  - GMMU Page Table 集中 Host DRAM
  - TLB Invalidation 单一 KMD 控制 (MSI-X 中断)
  - 跨 GPU 共享内存: 集中管理 (per `20-gmmu-mvp.md`)

方案 B (N+1 Linux 节点):
  - GMMU Page Table 分散各 Compute Tray DRAM
  - TLB Invalidation 需要跨节点协调
    - Compute Tray 1 释放 iova → 必须通知 Compute Tray 2 同步 invalidate
  - 跨 GPU 共享内存: 需要 PCIe over UALink 协议 (GMMU/FM 协同)
  - KMD 协同: 跨 Compute Tray KMD 通信协议

⚠️ 这是一个**重大工程挑战**, 不是简单实施工作量
- 解决方案: PCIe over UALink 隧道化 MSI-X 中断
- 解决方案: 跨节点 GMMU Page Table 同步协议
- 解决方案: 分布式 KMD (类似 RDMA over IB 子系统)
```

---

## §3 PCIe over UALink 协议栈 (跨 Compute Tray)

### §3.1 PCIe over UALink 路径

```
GPU 0 (Compute Tray 1) → 跨 Compute Tray GPU 4 (Compute Tray 2):

物理路径:
  GPU 0 HBM
  → GPU 0 NIC-DMA TX
  → GPU 0 UALink MAC/PHY
  → Compute Tray 1 Switch (本地 NVSwitch)
  → UALink 上行 PHY
  → Switch Tray Switch 0 (跨 Fabric Switch, 含 Linux 协调)
  → UALink 下行 PHY
  → Compute Tray 2 Switch (本地 NVSwitch)
  → GPU 4 NIC-DMA RX
  → GPU 4 HBM

跳数: 4 跳 (含 2 级 UALink 物理层 + 2 级 Switch)

每跳延时 (per `21-fabric-switch-mvp.md`):
  - UALink PHY: 10 ns
  - UALink MAC: 30 ns
  - Flit 编码: 30 ns
  - 物理传输: ~3-30 ns (随距离)
  - Switch PTE 查表: 100 ns (本地) / ~300 ns (跨 tray, 含 Switch Tray Linux 协调)
  - Switch Crossbar: 50 ns

PCIe over UALink 协议转换 (入口 + 出口):
  - PCIe TLP 封装: 200 ns
  - UALink Flit 封装: 100 ns
  - Switch PTE 跨域: 300 ns (含 Switch Tray Linux 协调开销)
  - UALink Flit 解封装: 100 ns
  - PCIe TLP 解封装: 200 ns
  - 总协议转换: 900 ns

跨 Compute Tray 总延时: ~1.8 μs (per 第 13 轮讨论)
```

### §3.2 PCIe over UALink 协议要求

```
PCIe over UALink 协议栈 (方案 B 关键):

Layer 1: PCIe TLP (Transaction Layer Packet)
  - 入口: GPU 0 发出 PCIe TLP (MRd/MWr/CplD 等)
  - 出口: 远端 GPU 接收 PCIe TLP

Layer 2: PCIe↔UALink Bridge (在 Switch Tray Linux 协调)
  - PCIe TLP 封装到 UALink Flit
  - UALink Flit 解封装到 PCIe TLP
  - 硬件实现 (类似 Mellanox ConnectX-6/7 RDMA 网卡)

Layer 3: UALink Flit (256B, per `21-fabric-switch-mvp.md` §5.1)
  - UALink MAC 帧 + 路由
  - Switch Tray Linux 协调 (per §4)

Layer 4: Switch PTE (per `21-fabric-switch-mvp.md` §8.5)
  - Base/Limit 寄存器 (经 Sideband 配置)
  - 路由决策: 本地 GPU / 远端 GPU / CXL Pool

Layer 5: Host 软件栈 (per Compute Tray Linux)
  - KMD 拦截 PCIe MMIO (与 V3.1-Rev2.0 类似)
  - DMA Buffer 分配 (per Compute Tray 独立)
  - 跨节点 GMMU Page Table 同步协议 (新)
  - 跨节点 TLB Invalidate 协议 (新)
```

### §3.3 跨 Compute Tray GMMU 协议 (新)

```
跨节点 GMMU Page Table 同步 (方案 B 关键):

场景: Compute Tray 1 进程 unmap() 共享 VA → 必须通知 Compute Tray 2 同步 invalidate

V3.1-Rev2.0 (1 Linux 节点):
  Linux munmap() → KMD 经 PCIe MMIO 写 GPU GMMU → 触发 TLB Invalidate

方案 B (N+1 Linux 节点):
  Linux munmap() (Compute Tray 1) → KMD #1 经 PCIe over UALink 通知 KMD #2
  → KMD #2 经 PCIe MMIO 写 Compute Tray 2 GPU GMMU → 触发 TLB Invalidate
  → 跨节点 PCIe over UALink 中断传递 (~5-10 μs)
  → KMD #1 与 KMD #2 协同完成同步

⚠️ 同步协议挑战:
- 跨节点时延: ~5-10 μs (PCIe over UALink + Linux IPC)
- 一致性窗口: ~5-10 μs (期间可能 stale TLB)
- 解决方案: 严格 invalidation 协议 (类似分布式数据库 2PC)
```

### §3.4 跨 Compute Tray SR-IOV GPU 共享 (可选)

```
方案 B 设想 (per 用户第 10 轮):
  GPU 0 (Compute Tray 1) 通过 SR-IOV 把 GPU 4 (Compute Tray 2) 暴露给本 Compute Tray 的 Linux

实现路径:
  1. Compute Tray 2 GPU 4 创建 SR-IOV VF (PF/VF 划分)
  2. PF 在 Compute Tray 2 PCIe Hierarchy 内注册
  3. PCIe over UALink 将 PCIe 设备描述 (BDF, BAR) 隧道化到 Compute Tray 1
  4. Compute Tray 1 Linux 看到 VF, 像本地 PCIe 设备一样访问

⚠️ 工业界现状:
- NVIDIA MIG: 硬件切片, 但限本地 PCIe Hierarchy
- AMD MI300X CXD: 类似 MIG
- Intel SR-IOV GPU: 概念成熟, 但跨节点 SR-IOV 未量产
- v1.0 MVP 暂不实施 SR-IOV 跨节点 (per `21-dist-scale-up-evolution-roadmap.md` §4.3)
```

---

## §4 Switch Tray Linux 协调机制

### §4.1 Switch Tray Linux OS (vs V3.1-Rev2.0 ARM Cortex-M 固件)

```
V3.1-Rev2.0 Switch Tray:
  - ARM Cortex-M (200-400 MHz) + Bare-metal RTOS
  - 功能: PTE 查表 + UALink 协议 + SMBus 管理
  - 无 Linux, 无复杂决策算法

方案 B Switch Tray:
  - ARM Cortex-A (1-2 GHz) + 完整 Linux OS
  - 2-4 GB DDR + 16 GB SSD
  - 功能: PTE 路由 + UALink ↔ PCIe over UALink 转换 + 多 Compute Tray FM 协调
  - 与 NVIDIA NVSwitch 3 内部 ARM Cortex-A 协同 (NVOS)

代价:
  - 物料成本增加: ARM Cortex-A 比 Cortex-M 贵 5-10×
  - DDR + Flash 增加物料成本
  - 维护负担: Switch Linux 打补丁/升级
  - 故障域扩大: Switch Linux 故障 → 整个 Scale-Up 不可用

优势:
  + 复杂 PCIe over UALink 协议可在 Linux 上实施
  + 跨 Compute Tray FM 协调算法 (复杂决策)
  + PCIe device emulation (跨节点 SR-IOV)
  + 与 OpenBMC 等 BMC 生态集成
```

### §4.2 Switch Tray Linux FM 4 大服务

```
FM 服务 (方案 B):

1. Fabric Address Manager (vs 方案 C 的 NSA-aware):
   - 维护 GUPA 64-bit 路由域划分 (per V3.1-Rev2.0 §4.0)
   - 跨 Compute Tray PCIe over UALink 地址映射
   - PTE Base/Limit 寄存器配置

2. PCIe over UALink Bridge:
   - PCIe TLP ↔ UALink Flit 协议转换
   - Switch Tray 内 PCIe device emulation (SR-IOV)
   - BAR 空间映射 (跨 Compute Tray)

3. Multi-Compute-Tray FM Coordinator:
   - 跨 Compute Tray 拓扑发现
   - 跨 Compute Tray 拓扑变更执行 (Drain / HRT Swap)
   - 跨 Compute Tray KMD 协调
   - CXL 内存池热插拔 (经 CXL Switch)

4. Host Comm Interface:
   - VirtIO 设备 (PCI Function)
   - 与各 Compute Tray Host FM 通信
   - 与 BMC (IPMI / Redfish) 通信
```

### §4.3 三方协调: GPU KMD ↔ Switch FM ↔ Host FM

```
跨 Compute Tray Drain 协议 (方案 B):

阶段 0: 决策 (Host FM or Switch FM)
  - 故障检测 → Host FM 决策需要 Drain
  - 通知目标 Compute Tray FM (经 Sideband SMBus)

阶段 1: 目标 Compute Tray NIC-DMA (KMD)
  - KMD 经 PCIe MMIO 写 NIC_DRAIN_CTRL.START_DRAIN
  - NIC-DMA 拒绝新 Tx Micro-op
  - 向 Switch Tray FM 发送 CTRL_QUIESCE (经 PCIe over UALink)

阶段 2: Switch Tray 协调 (Linux FM)
  - 通知所有相关 Compute Tray 的 NIC-DMA
  - 等待所有 Compute Tray 的 ROB 归零

阶段 3: 各 Compute Tray 排空
  - 每个 Compute Tray NIC-DMA 排空本地 ROB
  - 通知 Switch Tray FM

阶段 4: Switch Tray FM 通知 Host FM
  - 经 Sideband SMBus 通知 Host FM 排空完成

阶段 5: Host FM 决策恢复
  - Host FM 决定恢复或迁移
  - 经 PCIe MMIO 通知各 Compute Tray

⚠️ 方案 B 协调复杂度:
- 跨节点通信: PCIe over UALink + Switch Linux IPC (~10-50 μs)
- 总三方协调时延: ~50-100 μs (vs V3.1-Rev2.0 单节点 100 μs)
```

### §4.4 Switch Tray CXL 内存池管理

```
方案 B CXL 内存池接入 (经 Switch Tray):

路径:
  CXL Memory Pool
  → CXL Switch (外部设备, per `21-fabric-switch-mvp.md` §8.5)
  → Switch Tray Linux FM (PTE Base/Limit 配置)
  → PCIe over UALink 隧道化 CXL 协议
  → 目标 Compute Tray NIC-DMA
  → GPU GMMU

注意: CXL Memory Pool 仍由外部 CXL Switch 提供, **不在 Switch Tray Linux 内**
但 CXL 地址空间映射由 Switch Tray Linux FM 维护 (per §4.2)

⚠️ 方案 B CXL 路径:
- CXL.mem 协议封装在 PCIe over UALink 内 (隧道化)
- CXL.cache 协议不支持跨 Fabric (方案 B 不实施)
- 仅 CXL.mem (HDM-DB) 跨 Fabric 访问
```

---

## §5 v1.0 MVP 范围

### §5.1 v1.0 MVP 实施范围

- [x] **Compute Tray 独立 CPU + Linux OS** 方案定义 (§2)
- [x] **PCIe over UALink 协议栈** 路径定义 (§3)
- [x] **Switch Tray Linux OS** 协调机制 (§4)
- [x] **三方协调 (GPU KMD ↔ Switch FM ↔ Host FM)** 协议 (§4.3)
- [x] **跨 Compute Tray GMMU 协议** 协议框架 (§3.3)
- [x] **v1.0 MVP 不实施 SR-IOV 跨节点** (per `21-dist-scale-up-evolution-roadmap.md` §4.3)
- [x] **V3.1-Rev2.0 兼容路径** (Compute Tray 0 = Host Tray 默认状态)

### §5.2 v1.0 MVP 不实施范围 (推迟到方案 C / NSA-aware)

- ❌ **NSA-aware MMU / Fabric Address**: 推迟到方案 C (`22-nsa-fabric-address-spec.md`)
- ❌ **Remote Atomic Unit 硬件**: 推迟到方案 C (`25-nsa-hardware.md`)
- ❌ **Hardware Directory 跨 Fabric**: 推迟到方案 C
- ❌ **CXL 3.0 Fabric Switch 硬件加速**: 推迟到方案 C (`28-cxl-3-fabric.md`)
- ❌ **跨 Compute Tray SR-IOV GPU**: 推迟到 v1.1+ (per evolution-roadmap)
- ❌ **Capability-Based 多租户**: 推迟到方案 C (`27-nsa-capability.md`)

### §5.3 v1.0 MVP 验证标准 (10 项 Acceptance Gate)

- [ ] **AG1**: Compute Tray 独立 CPU + Linux OS 拓扑定义完成 (§2.1)
- [ ] **AG2**: PCIe over UALink 协议栈分层定义完成 (§3.2)
- [ ] **AG3**: 跨 Compute Tray GPU↔GPU 延时 ~1.8 μs 验证 (per `21-fabric-switch-mvp.md` §7)
- [ ] **AG4**: Switch Tray Linux OS FM 4 大服务定义完成 (§4.2)
- [ ] **AG5**: 三方协调 (GPU KMD ↔ Switch FM ↔ Host FM) 协议定义 (§4.3)
- [ ] **AG6**: 跨 Compute Tray GMMU Page Table 同步协议 (§3.3)
- [ ] **AG7**: 跨 Compute Tray TLB Invalidate 协议 (基于 MSI-X over PCIe over UALink)
- [ ] **AG8**: Switch Tray CXL 内存池路径 (§4.4)
- [ ] **AG9**: V3.1-Rev2.0 兼容路径 (Compute Tray 0 = Host Tray 默认)
- [ ] **AG10**: 0 个新 ABI 函数 (per ADR-088 §D5, 方案 B 是 RTL/固件层, 不改 ABI)

---

## §6 跨子系统 Cross-Reference

### §6.1 上游依赖

| 依赖文档 | 依赖内容 | 依赖强度 |
|---------|---------|----------|
| [`21-soc-topology-mvp.md`](21-soc-topology-mvp.md) | V3.1-Rev2.0 SoC 拓扑 (本规范扩展) | 强 |
| [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) | Switch PTE + 协议 (§3, §4) | 强 |
| [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) §4.0 | GUPA 划分 (方案 B 沿用) | 强 |
| [`ADR-SOC-21-v31-rev2-topology-correction.md`](../soc_arch/adr/ADR-SOC-21-v31-rev2-topology-correction.md) | V3.1-Rev2.0 决策 | 中 |

### §6.2 下游依赖

| 下游文档 | 依赖本规范内容 |
|---------|---------------|
| [`21-dist-scale-up-evolution-roadmap.md`](21-dist-scale-up-evolution-roadmap.md) | 方案 B 演进路径 |
| [`23-dist-scale-up-topology.md`](23-dist-scale-up-topology.md) | **方案 C (NSA-aware) 完整拓扑** (本规范是过渡方案) |
| [`29-nsa-evolution-roadmap.md`](29-nsa-evolution-roadmap.md) | 5 阶段演进时间线 |

---

## §6.4 降级触发条件与降级步骤 (Oracle B4 修正)

> **本节为 Oracle B4 修正**: 补充方案 B 作为**降级路径**的具体触发条件 + 详细降级步骤。

### §6.4.1 降级触发条件 (任一)

```
触发条件 1: NSA Switch (CXL 3.0 Fabric Switch Tier 1) 量产延期 > 6 个月
  - 计划时间: NSA Stage 2 启动时 (2027-Q2)
  - 触发: Astera Labs / Microchip CXL Switch 量产延期 > 2027-12

触发条件 2: Remote Atomic Unit 硬件量产失败
  - NSA-aware 硬件良率 < 80%
  - HW 一致性验证不通过

触发条件 3: Hardware Directory (L1/L2/L3) 良率不足
  - SRAM 容量超预算
  - MESIF 一致性验证不通过

触发条件 4: CXL 3.0 Fabric 量产延期
  - 规范量产化推迟到 2028+
  - 跨 Fabric 透明路径无法实施
```

### §6.4.2 降级步骤 (5 步)

```
降级路径: 方案 C (NSA-aware) → 方案 B (传统分布式)

步骤 1: 保留 Compute Tray 独立 CPU + Linux OS (方案 B 基础)
  - 不回收已部署的 Compute Tray CPU
  - Compute Tray 继续独立运行 PCIe Hierarchy

步骤 2: Switch Tray 升级为独立 Linux FM (per `21-dist-scale-up-topology-b.md` §4)
  - ARM Cortex-A + Linux OS (替代 ARM Cortex-M)
  - Switch Tray Linux FM 协调 PCIe over UALink 协议栈
  - Tier 1 FM 接口 + Tier 2 FM 通信

步骤 3: Capability 切换到软件层
  - Capability Token 仍在 GSP-RM Tenant Manager 签发 (HW 强制保留)
  - 但 Capability 数据库切换到软件层 (per `27-nsa-capability.md` §3.2)
  - Capability 校验仍在 NSA-aware MMU (HW, per `25-nsa-hardware.md` §2.3)
  - 性能损失: Capability 校验 ~50 ns (软件) vs ~2 ns (HW 强制)

步骤 4: HRT 维持 V3.1-Rev2.0 (Fabric ID = 0, 单 Compute Tray 内寻址)
  - 跨 Compute Tray 寻址通过 PCIe over UALink 协议栈 (软件)
  - 跨 tray 延时: ~1.8 μs (vs NSA-aware 0.5 μs, 性能损失 3.6×)

步骤 5: 文档状态同步
  - `21-dist-scale-up-topology-b.md` (本规范) 状态: Active (替代 V3.1-Rev2.0)
  - `23-dist-scale-up-topology.md` (方案 C) 状态: Pending (待 NSA 硬件就绪)
  - `29-nsa-evolution-roadmap.md` (演进路线图) 更新降级状态

降级总时延影响:
  - 跨 tray GPU↔GPU: 1.8 μs (vs NSA-aware 0.5 μs, 性能损失 3.6×)
  - Capability 校验: ~50 ns (软件) vs ~2 ns (HW 强制, 性能损失 25×)
  - 总性能影响: ~30% 退化 (但功能完整, 不损失多租户隔离)

降级路径不损失:
  - 多 Linux 节点 (Compute Tray 独立)
  - 多租户隔离 (Capability HW 强制保留)
  - 故障域隔离 (Compute Tray 独立故障域)
  - PCIe over UALink 软件协议栈 (成熟方案)
```

### §6.4.3 降级路径与方案 C 关系

```
┌─────────────────────────────────────────────────────────────────┐
│  NSA-aware Scale-Up 5 阶段演进 + 降级路径                      │
│                                                                  │
│  NSA Stage 0 (V3.1-Rev2.0, 2024-2026, ✅ ship)               │
│    └─ 1 Linux 节点, 8 GPU, 单 PCIe Hierarchy                │
│                                                                  │
│  NSA Stage 1 (v1.x, 2026-2027)                                │
│    ├─ NSA-aware MMU (8-bit Fabric ID)                       │
│    ├─ GSP-RM (RISC-V + NV-RTOS)                              │
│    ├─ Capability HW 校验 (NSA-aware MMU)                     │
│    └─ 仍单 Linux 节点 (兼容 V3.1-Rev2.0)                    │
│                                                                  │
│  NSA Stage 2 (v3.x, 2027-2028)                                │
│    ├─ 触发降级 (若 NSA 硬件延期): 方案 B                     │
│    │   ├─ Compute Tray 独立 CPU + Linux (新增)             │
│    │   ├─ Switch Tray Linux FM (ARM Cortex-A + Linux)      │
│    │   ├─ PCIe over UALink 协议栈 (软件)                    │
│    │   ├─ 跨 tray 延时 ~1.8 μs                              │
│    │   └─ 多租户隔离 (Capability 软件层, HW 保留)          │
│    │                                                          │
│    └─ 主路径 (NSA 硬件就绪): NSA-aware + Remote Atomic + HW Directory │
│                                                                  │
│  NSA Stage 3 (v3.x, 2028-2029)                                │
│    ├─ NSA-aware 硬件完整 (Remote Atomic + Directory)        │
│    ├─ 跨 tray 延时 ~500 ns                                    │
│    ├─ CXL 3.0 Fabric 完整兼容                                │
│    └─ Capability 完全替代 RCT                                │
│                                                                  │
│  NSA Stage 4 (2029+) — 商业化                                │
│    ├─ 与 NVIDIA NVL72 / AMD MI300X 对标                      │
│    └─ 商业化模型 (License / Open-source)                    │
└─────────────────────────────────────────────────────────────────┘
```

---

## §7 开放问题 (待新 session 讨论)

| # | 开放问题 | 优先级 | 关联草案 |
|---|---------|--------|---------|
| 1 | **跨 Compute Tray GMMU Page Table 一致性协议**: 类似 2PC 还是 Paxos? | P1 | 草案 2 |
| 2 | **跨节点 TLB Invalidate 时延上限**: 100 μs 还是 500 μs? | P1 | 草案 2 |
| 3 | **Switch Tray Linux 性能基准**: PCIe over UALink 协议栈时延实测 | P1 | 草案 5 |
| 4 | **多 Linux 节点 KMD 通信协议**: PCIe over UALink 中断 vs Sideband SMBus? | P1 | 草案 5 |
| 5 | **方案 B 与方案 C 演进路径**: 直接跳方案 C 还是方案 B 是必经阶段? | P2 | 草案 8 |
| 6 | **跨节点 SR-IOV GPU 共享可行性**: 业界支持度? | P2 | 草案 7 |
| 7 | **Switch Tray Linux 故障恢复**: 故障时降级到 V3.1-Rev2.0 单节点模式? | P2 | 草案 5 |
| 8 | **Compute Tray 内 GMMU 是否需要 NSA-aware**: 方案 B 是否为 NSA-aware 做准备? | P2 | 草案 2, 4 |
| 9 | **方案 B 商业化模型**: 与 NVIDIA HGX SuperPOD 对比定位? | P3 | 草案 8 |
| 10 | **PCIe over UALink 协议兼容性**: 与 CXL 3.0 Fabric 兼容性? | P3 | 草案 7 |

---

## §8 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v0.1-draft | Sisyphus | 首版: 分布式 Scale-Up 方案 B (传统分布式, 非 NSA-aware) v0.1 (10 项 Acceptance Gate + 10 个开放问题 + 与方案 A/C 横向对比) |
| 2026-09-19 | v0.2-draft | Sisyphus | Oracle B4 修正: §6.4 降级触发条件 + 5 步降级步骤 + 与方案 C 关系图 |

---

**关联 OpenSpec change**: 待 `openspec/changes/2026-09-19-cpptlm-mas-dist-scale-up-b/` 提案创建
**下次更新**: Oracle 评审反馈后 v0.2

**关键定位**: 本规范是 **方案 C (NSA-aware 分布式)** 的**降级替代**或**过渡方案**, 不依赖 NSA-aware 硬件, 2-3 年可量产。完整 NSA-aware 方案见 [`23-dist-scale-up-topology.md`](23-dist-scale-up-topology.md)。