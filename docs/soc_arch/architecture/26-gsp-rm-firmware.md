# GSP-RM: GPU System Processor + Resource Manager 固件架构 v0.1 (草案 5)

> **目的**: 定义 CppTLM dGPU SoC **MAS-3.1 v3.1-Rev2.0 → NSA-aware 升级** 所需的 **GPU 内部微控制器 (GSP) + RM 固件架构**。对应你提出的"把 Host 内核驱动的相关代码下沉到 GPU 内"设想 (per 第 11 轮讨论)。本规范参考 NVIDIA H100 GSP-RM 业界实践 + seL4 微内核思想, 定义 4 大服务 (Memory / Fault / Fabric / Tenant)。
>
> **状态**: Draft v0.1 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: 待 `openspec/changes/2026-09-19-cpptlm-mas-gsp-rm-firmware/` 提案
> **关联文档**:
> - [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) §3.4 GPC UDD Agent Host Comm 接口
> - [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) §3.4 CIU 接口
> - [`22-nsa-fabric-address-spec.md`](22-nsa-fabric-address-spec.md) NSA-aware MMU 协同
> - [`23-dist-scale-up-topology.md`](23-dist-scale-up-topology.md) 分布式 Scale-Up 拓扑
> - [`25-nsa-hardware.md`](25-nsa-hardware.md) NSA-aware MMU 硬件
> - [`27-nsa-capability.md`](27-nsa-capability.md) Capability 多租户隔离
> - [`28-cxl-3-fabric.md`](28-cxl-3-fabric.md) CXL 3.0 Fabric 兼容

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

> **关联 ADR**: 待起草 GSP-RM 控制面下沉 ADR

---

## §0 阅读引导

- 想理解 GSP-RM 总体 → 读 §1 (概述 + 与 NVIDIA H100 对比)
- 想看 GSP 硬件规格 → 读 §2
- 想看微内核选型 → 读 §3 (seL4 vs NV-RTOS)
- 想看 4 大服务实现 → 读 §4 (Memory / Fault / Fabric / Tenant)
- 想看 **FM↔GSP-RM 控制面契约** (Oracle P0 修正) → 读 §4.6 (新增)
- 想看 Host Comm (VirtIO) 接口 → 读 §5
- 想看 v1.0 MVP 范围 → 读 §6
- 想看开放问题 → 读 §7

---

## §0.5 FM↔GSP-RM 控制面契约 (Oracle P0 修正)

> **本节为 Oracle P0 修正**: FM (Fabric Manager) 与 GSP-RM 角色不同但协同 — FM 决策, GSP-RM 执行。本节定义完整契约, 避免 split-brain 风险。

### §0.5.1 角色定位 (决策 vs 执行)

```
┌─────────────────────────────────────────────────────────────────────────┐
│  控制面分层 (Decision vs Execution)                                  │
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │  Fabric Manager (FM) — DECISION LAYER (上层)                 │  │
│  │  - 部署位置: Host CPU Linux daemon + Switch Linux FM           │  │
│  │  - 核心职责: "做什么" (What)                                  │  │
│  │    + 拓扑发现 (Topology Discovery)                            │  │
│  │    + 路由计算 (Route Computation)                             │  │
│  │    + 拓扑变更决策 (Topology Mutation)                         │  │
│  │    + 跨节点协调 (Multi-Compute-Tray Coordination)            │  │
│  │  - 用户态进程, 可被 kill / restart, 故障可恢复              │  │
│  │  - 决策算法复杂, 智能在 FM                                  │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                              │                                       │
│                              │ 通信 (Sideband SMBus + PCIe MMIO) │
│                              ▼                                       │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │  GSP-RM 微控制器 — EXECUTION LAYER (下层)                    │  │
│  │  - 部署位置: GPU Die 内 (RISC-V 200-400 MHz, per §2)      │  │
│  │  - 核心职责: "如何做" (How)                                   │  │
│  │    + HRT Atomic Swap 执行 (<8 cycles @ clk_core)              │  │
│  │    + Page Fault 处理 (<5 μs)                                   │  │
│  │    + Capability 签发 (<1 μs)                                 │  │
│  │    + Drain 协议执行 (<100 μs 跨节点)                        │  │
│  │  - 内核态固件, 不可被用户态中断, 始终在线                  │  │
│  │  - 执行机制确定, 智能在 FM                                  │  │
│  └────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘

⚠️ 关键边界: FM 决策 → 经 Host Comm (VirtIO) 通知 GSP-RM → GSP-RM 执行
  - FM 不可直接写 GPU 寄存器 (需经 Host Comm 中转)
  - GSP-RM 不可自主拓扑变更 (需 FM 决策)
```

### §0.5.2 Owner Directory 同步协议

```
FM ↔ GSP-RM Owner Directory 同步:

1. 拓扑发现 (FM → GSP-RM):
   - FM 启动时扫描所有 Compute Tray + Switch + CXL Pool
   - FM 构建全局拓扑视图 (FM Local DB)
   - FM 经 Host Comm 写入 GSP-RM Owner Directory 寄存器
   - GSP-RM Fabric Service 接收并写入 per-GPU Owner Directory

2. 拓扑变更 (FM → GSP-RM):
   - FM 决策: 新增 CXL Pool X
   - FM 经 Host Comm 通知 GSP-RM (VirtIO Command)
   - GSP-RM Fabric Service 验证 (per Capability, per RCT)
   - GSP-RM 写 HRT Shadow + 触发 Atomic Swap
   - GSP-RM 经 VirtIO Notification 通知 FM (swap_done)
   - FM 更新全局拓扑视图 (FM Local DB)

3. 状态查询 (FM → GSP-RM):
   - FM 经 Host Comm 发起状态查询 (VirtIO Command)
   - GSP-RM Fabric Service 返回 HRT/RCT/Capability 状态
   - FM 同步全局视图

⚠️ Split-Brain 风险:
  - FM 失联 (Host OS 崩溃) → GSP-RM 无法接收新决策, 但维持当前状态
  - GSP-RM 失联 (GPU 硬件故障) → FM 经 MSI-X 检测, 触发故障隔离
  - 双方状态不一致 → 严重问题 (Capability 签发与实际不一致)
  - 解决: FM 失联时 GSP-RM 进入 "保守模式" (Capability 仅允许本地寻址, 拒绝远程)
```

### §0.5.3 Drain 协议时序 (FM 决策 + GSP-RM 执行)

```
CXL Drain 三阶段协议 (FM ↔ GSP-RM 协同):

阶段 0: FM 决策 (用户态, ~10 ms)
  - FM 决定: 需要 Drain (CXL 内存池热插拔/故障隔离)
  - FM 经 Host Comm 通知 GSP-RM (VirtIO Command: drain)
  - Payload: target_port, timeout_ms

阶段 1: GSP-RM 触发本地 NIC-DMA Drain (<100 μs)
  - GSP-RM Fabric Service 经 CIU 写 NIC_DRAIN_CTRL.START_DRAIN
  - NIC-DMA 拒绝新 Tx Micro-op, 返回 Abort
  - NIC-DMA 通过 UALink VC0 向 Switch FM 发送 CTRL_QUIESCE
  - NIC-DMA 启动 100us 硬件定时器

阶段 2: Switch FM 排空 + GSP-RM 等待 (<50 μs 跨节点)
  - Switch PTE 停止向目标 CXL Port 发新 CXL.mem Req
  - Switch FM 等待目标 CXL Port 的 Outstanding Counter == 0
  - Switch FM 经 Sideband SMBus 通知 GSP-RM (CTRL_Q_ACK)
  - GSP-RM 接收 CTRL_Q_ACK (经 UALink VC0)

阶段 3: GSP-RM 排空本地 ROB + 通知 FM (<50 μs)
  - GSP-RM 通知 NIC-DMA: 等待本地 ROB 归零
  - NIC-DMA ROB valid entries == 0 → DRAIN_COMPLETE
  - GSP-RM 经 VirtIO Notification 通知 FM (drain_done)
  - FM 决定恢复或迁移
  - FM 经 Host Comm 写 NIC_DRAIN_CTRL.FORCE_ABORT 或恢复正常

超时保护:
  - 阶段 1-2 总超时: 100 μs @ clk_io
  - 阶段 3 总超时: 50 μs
  - 跨节点总时延: ~200 μs (vs V3.1-Rev2.0 ~100 μs 单节点)
```

### §0.5.4 Capability 签发时序 (FM 决策 + GSP-RM 执行)

```
Tenant Capability 签发 5 阶段时序 (FM ↔ GSP-RM 协同):

阶段 1: Host Driver 请求 (Host 用户态, ~10 μs)
  - Host Driver 经 VirtIO 通知 FM (request: capability_request)
  - Payload: Object Type, Permissions, Base, Length, Tenant ID

阶段 2: FM 验证配额 (Host 用户态, ~50 μs)
  - FM 验证 Tenant 配额 (HBM bandwidth, Compute SM 等)
  - FM 检查与其他 Tenant 冲突
  - FM 决策: 允许或拒绝

阶段 3: FM → GSP-RM Capability 签发 (Host Comm, ~10 μs)
  - FM 经 Host Comm (VirtIO) 通知 GSP-RM (request: capability_sign)
  - Payload: Tenant ID, Base, Length, Permissions
  - GSP-RM Tenant Manager 接收

阶段 4: GSP-RM Capability 签发 (<1 μs)
  - GSP-RM Tenant Manager 生成 Capability Token (128 bits)
  - GSP-RM 写入 CIU Capability 寄存器 (经 CIU APB)
  - GSP-RM 注入 NSA-aware TLB (per `25-nsa-hardware.md` §2)
  - GSP-RM 记录 Capability Database

阶段 5: GSP-RM → FM → Host Driver 返回 (Host Comm, ~10 μs)
  - GSP-RM 经 VirtIO Notification 通知 FM (capability_signed)
  - FM 经 Host Comm 返回 Host Driver (response: capability_token)
  - Host Driver 缓存 Capability Token

总时延: ~80 μs (Host 用户态 + Host Comm + GSP-RM)
对比 V3.1-Rev2.0 (无 GSP-RM):
  ~100-200 μs (Host KMD 全部处理)
加速: 1.5-2.5x (与 NVIDIA Hopper GSP-RM 一致)
```

### §0.5.5 异常路径与 Split-Brain 处理

```
异常路径 (5 类):

1. FM 失联 (Host OS 崩溃):
   - GSP-RM 检测: FM Watchdog Timer 超时 (5 s)
   - GSP-RM 进入 "保守模式":
     - Capability 仅允许本地寻址 (Fabric ID = 0)
     - 拒绝跨 Fabric Capability 签发
     - HRT 维持当前状态 (不切换)
   - 运维介入: 重启 Host OS, FM 自动恢复

2. GSP-RM 失联 (GPU 硬件故障):
   - FM 经 MSI-X 检测: GSP-RM 心跳超时
   - FM 决策: 故障隔离
   - FM 通知所有 Compute Tray KMD: 该 GPU 不可用
   - 工作负载迁移 (FM 协调)

3. FM ↔ GSP-RM 状态不一致 (Split-Brain):
   - 检测: Capability Token 版本号校验 (per `27-nsa-capability.md` §2)
   - 恢复: FM 重新发送 Owner Directory, GSP-RM 强制刷新
   - 预防: Capability Token 含 Version 字段 (per §2.1)

4. Drain 中止 (FM 决策中途取消):
   - FM 写 NIC_DRAIN_CTRL.FORCE_ABORT = 1
   - GSP-RM 强制清零 ROB, 恢复流量
   - 通知 Switch FM: Drain 中止

5. 跨节点 PCIe over UALink 故障:
   - Switch FM 检测 (经 Sideband SMBus): UALink Link Down
   - FM 决策: 故障隔离
   - FM 通知 GSP-RM: 暂停跨 Fabric 访问
   - GSP-RM 进入 "本地模式" (仅本地寻址)
```

---

## §1 概述

### §1.1 问题陈述

V3.1-Rev2.0 (`21-microarch-ifc-mvp.md` §3.4) 的 GPC UDD Agent 由 **Host KMD (Linux Kernel Module)** 控制:
- Host Driver 经 PCIe MMIO 写 GPU 寄存器 (CIU, HRT, UDD Agent 等)
- Host Driver 处理 MSI-X 中断 (TLB Invalidate 完成, Page Fault, RAS 等)
- 复杂业务逻辑 (Memory Service, Fault Service) 全部在 Host CPU 上跑

这导致:
- ❌ **Host 介入热路径**: Host 系统调用 5-10 μs 开销
- ❌ **Host 攻击面**: 恶意 Host OS 可控制 GPU 资源
- ❌ **Host KMD 复杂**: Linux 内核驱动代码 100K+ LOC
- ❌ **多租户隔离薄弱**: Host OS 信任链不可靠

NVIDIA H100 Hopper 通过 **GSP (GPU System Processor) + RM (Resource Manager)** 解决了这个问题 (per 第 11 轮调研):
- GSP 是 GPU 内部的 RISC-V 微控制器
- RM 代码下沉到 GSP 固件
- Host Driver 变成 "薄前端" (~20K LOC)
- 控制面下沉, 数据面仍在 Host 协调

本规范定义 CppTLM dGPU 的 GSP-RM 架构, **借鉴 NVIDIA 业界实践 + 你提出的 seL4 微内核思想**。

### §1.2 NVIDIA H100 GSP-RM 借鉴

```
NVIDIA H100 GSP-RM 实际架构 (per 公开资料):

┌─────────────────────────────────────────────────────────────┐
│  Host Linux (薄前端)                                        │
│  ┌────────────────────────────────────────┐                │
│  │  nvidia.ko (~100K LOC, 减少 5x)       │                │
│  │  - VirtIO frontend                    │                │
│  │  - 简单 MMIO 透传                       │                │
│  │  - DMA buffer 分配                    │                │
│  └────────────────────────────────────────┘                │
└────────────────────────────────────────────────────────────┘
        │ PCIe MMIO + DMA + VirtIO 消息
        ▼
┌─────────────────────────────────────────────────────────────┐
│  GPU Die (Hopper H100)                                      │
│  ┌────────────────────────────────────────┐                │
│  │  GSP (RISC-V 微控制器)                │                │
│  │  ┌──────────────────────────────────┐  │                │
│  │  │  GSP-RM (NVIDIA NV-RTOS)         │  │                │
│  │  │  - RM 业务逻辑                    │  │                │
│  │  │  - Channel manager                │  │                │
│  │  │  - Memory allocator              │  │                │
│  │  │  - Power management               │  │                │
│  │  │  - Error handling                 │  │                │
│  │  └──────────────────────────────────┘  │                │
│  └────────────────────────────────────────┘                │
└─────────────────────────────────────────────────────────────┘

NVIDIA GSP-RM 不能消除:
- PCIe 设备枚举 (Host 启动 GPU)
- DMA buffer 分配 (Host 分配 Host DRAM)
- 跨 GPU 共享内存 (Host 协调 P2P)

NVIDIA GSP-RM 能消除:
- Host KMD 大部分业务逻辑 (5-10 μs 系统调用 → <1 μs GSP 直处理)
- Host-FG 信任边界问题
- Host 多租户隔离脆弱性
```

### §1.3 CppTLM GSP-RM 设计目标

```
设计目标:
  ✅ Host 介入热路径 20-35 μs → GSP-RM 直处理 <1 μs (5-35x 加速)
  ✅ Host KMD 5x 简化 (100K → 20K LOC)
  ✅ Host 攻击面最小化
  ✅ 多租户隔离由 GSP-RM 强制
  ✅ 与 V3.1-Rev2.0 CIU / HRT / GMMU 全量兼容
  ✅ 与 NSA-aware MMU / Fabric Address 协同 (per `22-nsa-fabric-address-spec.md`)
```

---

## §2 GSP 硬件规格

### §2.1 GSP 微控制器规格

```
┌─────────────────────────────────────────────────────────────┐
│              GSP (GPU System Processor)                     │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐│
│  │  CPU Core: RISC-V (推荐) 或 ARM Cortex-R              ││
│  │  - 频率: 200-400 MHz                                    ││
│  │  - ISA: RV32IMAC + F (单精度 FPU) + D (调试)         ││
│  │  - Privilege: M/S/U Mode (Machine/Supervisor/User)     ││
│  └─────────────────────────────────────────────────────────┘│
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐│
│  │  Memory:                                                   ││
│  │  - Code SRAM: 256 KB (GSP-RM 固件代码)               ││
│  │  - Data SRAM: 1 MB (GSP-RM 工作内存)                  ││
│  │  - Shared DRAM: 4 MB (经 DMA 与 Host DRAM 共享)        ││
│  │  - ROM: 32 KB (Boot ROM, GSP 启动)                    ││
│  └─────────────────────────────────────────────────────────┘│
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐│
│  │  Peripherals:                                              ││
│  │  - Mailbox Register: GSP ↔ GPU Compute 接口             ││
│  │  - Interrupt Controller: GSP 中断源 (HW Fault, MSI-X) ││
│  │  - DMA Engine: GSP ↔ Host DRAM (经 PCIe)              ││
│  │  - Watchdog Timer: GSP 心跳检测                         ││
│  └─────────────────────────────────────────────────────────┘│
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐│
│  │  Host Interface (GPU Compute ↔ Host):                   ││
│  │  - BAR0: Control Register (经 PCIe MMIO)               ││
│  │  - BAR1: Shared Memory (经 PCIe MMIO)                  ││
│  │  - MSI-X: 32 向量 (HW Fault, RAS, HRT Swap 等)        ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

### §2.2 GSP 启动流程

```
GSP 启动流程:

1. GPU 上电 → GSP Boot ROM 执行
2. GSP 等待 Host 加载固件 (经 PCIe)
   - Host Driver 写 GSP Boot Address 寄存器 (BAR0)
   - Host Driver 写 GSP Firmware (经 DMA 到 Code SRAM)
3. GSP 跳转 Code SRAM, 执行 GSP-RM 固件
4. GSP-RM 初始化:
   - 初始化微内核 (RTOS 或 seL4)
   - 初始化 4 大服务 (Memory / Fault / Fabric / Tenant)
   - 经 VirtIO 与 Host Driver 建立通信
5. GSP-RM 进入稳态: 处理 Host 命令 + GPU HW 事件

⚠️ 关键: GSP-RM 不能启动 GPU 自己 (Host 必须启动 PCIe 设备)
- 这意味着 Host Driver 仍保留 PCIe 设备枚举代码 (但很少)
- GSP-RM 不能控制 PCIe PHY / Link Training
```

### §2.3 GSP 与 GPU Compute 接口

```
GSP ↔ GPU Compute 接口 (经内部 NoC):

┌─────────────────────────────────────────────────────────────┐
│  GSP Side                          GPU Compute Side          │
│  ┌──────────────────────┐        ┌──────────────────────┐ │
│  │  Mailbox Register    │◄──────►│  Mailbox Register    │ │
│  │  (Command Queue)    │ 内部   │  (GSP-RM Request)  │ │
│  │                      │ NoC    │                      │ │
│  │  Interrupt Vector    │◄──────►│  Interrupt Source    │ │
│  │  (HW Fault 等)       │        │  (HW Fault 等)        │ │
│  │                      │        │                      │ │
│  │  Shared Memory       │◄──────►│  Shared Memory        │ │
│  │  (Page Table, etc.) │  DMA   │  (Page Table, etc.) │ │
│  └──────────────────────┘        └──────────────────────┘ │
└─────────────────────────────────────────────────────────────┘

关键交互:
1. GPU HW Fault → Interrupt Vector → GSP
2. GSP-RM 处理 Fault (查 Page Table, 调用 Memory Service)
3. GSP-RM 写 Mailbox (回复 Page Fault Resolution)
4. GPU Compute 接收 Mailbox (恢复 SM 指令)
```

---

## §3 微内核选型 (seL4 vs NV-RTOS)

### §3.1 选型对比

| 维度 | seL4 | NV-RTOS (NVIDIA Hopper) | 选型 |
|------|------|------------------------|------|
| **安全保证** | ✅ 形式化验证 (最强) | ⚠️ 闭源, 未知 | seL4 优 |
| **性能** | ⚠️ IPC 慢 50-100x | ✅ 裸 RTOS 快 | NV-RTOS 优 |
| **生态** | ⚠️ 学术 / 小众 | ✅ NVIDIA 私有 (成熟) | NV-RTOS 优 |
| **代码量** | ✅ 微内核 (~10K LOC) | ⚠️ 闭源 (未知) | seL4 优 |
| **多核支持** | ⚠️ 多核调度复杂 | ✅ 简单 | NV-RTOS 优 |
| **Debug 难度** | ⚠️ IPC 跟踪难 | ✅ 简单直接 | NV-RTOS 优 |
| **可验证性** | ✅ 形式化可验证 | ❌ 黑盒 | seL4 优 |
| **GPU 控制面适配** | ⚠️ 需 fast path 优化 | ✅ 天然适配 | NV-RTOS 优 |

### §3.2 推荐混合架构

```
推荐架构 (混合):

GSP-RM 微控制器:
  - 微内核: NV-RTOS (类 NVIDIA, 控制面高频)
  - 4 大服务直接运行 (无 IPC 开销)
  - 4 大服务间消息传递: 共享内存 + 原子操作 (无 IPC syscall)

Switch Tray Linux FM (per `21-dist-scale-up-topology-b.md` §4):
  - 微内核: seL4 (管理面低频, 安全优先)
  - 4 大服务以独立进程运行 (IPC + Capability)
  - 形式化验证保证安全 (per seL4 卖点)

理由:
  - GSP-RM 性能优先 (高频控制面): NV-RTOS
  - Switch FM 安全优先 (管理面低频): seL4
  - 与 NVIDIA Hopper 实际做法对齐 (NV-RTOS, 闭源)
```

### §3.3 seL4 vs NV-RTOS 性能对比

```
性能基准 (预估, 待实测):

NV-RTOS (GSP-RM):
  - Mailbox 写入 → 唤醒处理: < 1 μs
  - HW Fault 中断处理: < 5 μs
  - HRT Atomic Swap: < 1 μs
  - Capability 校验 (HW): < 100 ns

seL4 (Switch FM):
  - IPC 消息: ~5-10 μs (syscall + capability 校验)
  - HW Fault 处理: < 50 μs (含 IPC)
  - HRT 跨节点 Swap: < 100 μs (含 PCIe over UALink)
  - Capability 校验: 5-50 ns (HW + 缓存)

⚠️ seL4 IPC 开销对 Switch FM 可接受 (管理面低频, 不在热路径)
```

---

## §4 GSP-RM 4 大服务

### §4.1 4 大服务总览

```
GSP-RM 4 大服务 (NV-RTOS):

┌─────────────────────────────────────────────────────────────┐
│  Service 1: Memory Service                                  │
│  - Owner Directory 管理 (per `25-nsa-hardware.md` §4)         │
│  - Fabric Handle 生命周期                                  │
│  - Page Table 构建 (跨 Fabric, per `22-nsa-fabric-address-spec.md` §4) │
│  - Capability 签发/撤销 (per `27-nsa-capability.md`)            │
│  - HW Directory 协同 (atomic update)                          │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  Service 2: Fault Service                                    │
│  - HW Fault Interrupt 处理 (Page Fault, Remote-Fault)         │
│  - 远程 Page Table 获取 (跨 Compute Tray)                    │
│  - TLB 注入 (经 CIU APB)                                    │
│  - Capability 校验 (per `27-nsa-capability.md`)              │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  Service 3: Fabric Service                                   │
│  - NSA Switch 控制平面消息 (per `28-cxl-3-fabric.md`)          │
│  - Multicast 组管理 (Phase 3+ AllReduce)                    │
│  - 拓扑变更执行 (HRT Atomic Swap)                            │
│  - Drain 协议协调 (per `21-dist-scale-up-topology-b.md` §4.3) │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  Service 4: Tenant Manager                                   │
│  - Tenant 创建/销毁 (经 Host Comm)                           │
│  - Capability 签发 (per `27-nsa-capability.md` §3)            │
│  - 资源配额 (HBM bandwidth, Compute SM 等)                   │
│  - 多租户隔离强制 (HW-enforced)                              │
└─────────────────────────────────────────────────────────────┘
```

### §4.2 Memory Service (分布式 UVM 控制面核心)

```
Memory Service 详细功能:

输入:
  - Host Comm (VirtIO): mmap / munmap / capability 申请
  - GPU HW Fault: Page Fault / Remote-Fault / Capability 违规

输出:
  - 经 CIU APB 写 Page Table / TLB / HRT / Capability
  - 经 Mailbox 通知 GPU Compute
  - 经 Host Comm 返回 Host Driver

处理流程 (Host mmap):

1. Host 进程 mmap(VA, size, prot)
2. Host Driver 经 VirtIO 通知 GSP-RM (request: mmap)
3. GSP-RM Tenant Manager 验证配额
4. GSP-RM Memory Service:
   a. 分配物理页 (本 GPU HBM 或 Host DRAM 共享)
   b. 建立 Page Table Entry (VA → Fabric Addr)
   c. 注入 NSA-aware TLB (经 CIU APB)
   d. 签发 Capability Token
5. GSP-RM 经 VirtIO 返回 Host Driver (response: capability_token)
6. Host Driver 经 PCIe MMIO 写 GPU Capability 寄存器

处理流程 (GPU HW Page Fault):

1. GPU SM 发起 Load, TLB Miss
2. GPU HW PTW Walker 触发, 返回 Page Fault
3. GPU HW 触发 Page Fault Interrupt → GSP
4. GSP-RM Fault Service:
   a. 接收 Page Fault (fault_va, fault_ctx)
   b. Memory Service 查 Page Table
   c. 若跨 Fabric (Remote-Fault): HW Directory 查询 (per `25-nsa-hardware.md` §4)
   d. Page Table Hit → 注入 TLB (经 CIU APB)
   e. Page Table Miss → 通知 Host Driver (需 Host 调页)
5. GPU HW 恢复 SM 指令
```

### §4.3 Fault Service (HW Fault 处理)

```
Fault Service 详细功能:

处理 5 类 HW Fault:

1. Page Fault (TLB Miss + PTW Fail):
   - 来源: GPU HW PTW Walker
   - 处理: Memory Service 查 Page Table → 注入 TLB
   - 延时: < 5 μs (本地) / < 50 μs (跨 Fabric, Phase 3)

2. Remote-Fault (NSA-aware TLB):
   - 来源: NSA-aware TLB (per `25-nsa-hardware.md` §2.2)
   - 处理: HW Directory 查询 → Owner 响应 → 注入 TLB
   - 延时: < 300 ns (HW 处理, 无固件介入)

4. RAS Fault (ECC Error / Poison):
   - 来源: HBM-DMA 或 NIC-DMA
   - 处理: RAS CSR 记录 → 通知 Host Driver
   - 延时: < 5 μs

5. Security Violation (Capability 违规):
   - 来源: NSA-aware MMU (per `25-nsa-hardware.md` §2.3)
   - 处理: HW Drop → 触发 AWT Trap → KMD 处理
   - 延时: < 1 μs

6. HRT Swap Fault (拓扑变更错误):
   - 来源: HRT Atomic Swap 失败
   - 处理: 回滚到旧 HRT → 通知 Host FM
   - 延时: < 100 μs (含跨节点 PCIe over UALink)
```

### §4.4 Fabric Service (NSA-aware Fabric 控制平面)

```
Fabric Service 详细功能:

输入:
  - Host FM daemon (经 VirtIO + Host Comm): 拓扑变更指令
  - Switch Tray FM (经 Sideband SMBus + PCIe over UALink): CXL 状态

输出:
  - 经 CIU APB 写 HRT Shadow + 触发 Swap
  - 经 Mailbox 通知 GPU NIC-DMA (Drain 协议)
  - 经 Host Comm 返回 Host Driver

处理流程 (HRT Atomic Swap):

1. Host FM 决定: 新增 CXL Pool X → 需更新 HRT
2. Host FM 经 VirtIO 通知 GSP-RM (request: hrt_update)
3. GSP-RM Fabric Service:
   a. 验证 HRT 条目 (不能引入 CXL_DIRECT, per V3.1-Rev2.0 不变量)
   b. 写 HRT Shadow Bank (经 CIU APB)
   c. 触发 HRT Atomic Swap (经 CIU SWAP_BIT 寄存器)
4. GSP-RM 经 VirtIO 返回 Host FM (response: swap_done)

处理流程 (Drain 协议):

1. Host FM 决定: Drain GPU 0 ↔ Switch Port Y
2. Host FM 经 VirtIO 通知 GSP-RM (request: drain)
3. GSP-RM Fabric Service:
   a. 经 CIU 写 NIC_DRAIN_CTRL.START_DRAIN
   b. 等待 NIC-DMA ROB 归零 (经 Mailbox 通知)
   c. 通过 Switch Tray FM (经 Sideband) 通知 Switch
   d. 等待 Switch ACK (经 Sideband)
   e. ROB 归零 + Switch ACK → DRAIN_COMPLETE
4. GSP-RM 经 VirtIO 返回 Host FM (response: drain_done)
```

### §4.5 Tenant Manager (Capability 多租户)

```
Tenant Manager 详细功能:

输入:
  - Host Driver (经 VirtIO): tenant_create / tenant_destroy
  - Tenant Manager 内部: 配额检查, Capability 签发

输出:
  - 经 Mailbox 通知 Memory Service / Fault Service
  - 经 VirtIO 返回 Host Driver

Capability 签发流程 (per `27-nsa-capability.md` §3):

1. Host Driver 经 VirtIO 通知 GSP-RM (request: capability_request)
2. GSP-RM Tenant Manager:
   a. 验证 Tenant 配额 (HBM bandwidth, Compute SM 等)
   b. 生成 Capability Token (128 bits, per `27-nsa-capability.md` §2)
   c. 写入 CIU Capability 寄存器 (经 CIU APB)
   d. 记录 Capability Database (per Tenant)
3. GSP-RM 经 VirtIO 返回 Host Driver (response: capability_token)
4. Host Driver 缓存 Capability Token
5. SM Load/Store 触发 NSA-aware MMU 校验 (HW, per `25-nsa-hardware.md` §2.3)
```

---

## §5 Host Comm 接口 (VirtIO)

### §5.1 VirtIO 设备定义

```
GSP-RM 作为 VirtIO PCI Device:

┌─────────────────────────────────────────────────────────────┐
│  GSP-RM VirtIO Device (per GPU)                             │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐│
│  │  VirtIO Queue 0: Command Queue (Host → GSP)         ││
│  │  - Host Driver 提交: mmap / munmap / ioctl / drain ││
│  │  - GSP-RM 消费: 处理 + 返回 response                ││
│  └─────────────────────────────────────────────────────────┘│
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐│
│  │  VirtIO Queue 1: Notification Queue (GSP → Host)   ││
│  │  - GSP-RM 提交: Fault / RAS / Swap Done / etc.      ││
│  │  - Host Driver 消费: 处理 (如调页, 故障恢复)         ││
│  └─────────────────────────────────────────────────────────┘│
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐│
│  │  VirtIO Shared Memory (Bulk Transfer)                  ││
│  │  - Page Table 传输 (>1 KB 大块数据)                  ││
│  │  - Capability Token 列表 (多 Tenant 批量传输)        ││
│  │  - 经 PCIe DMA 传输 (避免逐 MMIO 写)                ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

### §5.2 VirtIO 消息格式

```
VirtIO Command Format (Host → GSP):

┌─────────────────────────────────────────────────────────────────┐
│  VirtIO Command (16 bytes header + payload)                    │
│                                                                  │
│  [15:8]  Command Type (8 bits):                                 │
│         0x01: MMAP (申请 capability + page table)              │
│         0x02: MUNMAP (释放 capability + page table)            │
│         0x03: DRAIN (触发 drain 协议)                          │
│         0x04: HRT_UPDATE (HRT Shadow 写入)                     │
│         0x05: TENANT_CREATE / TENANT_DESTROY                   │
│         0x06: CAPABILITY_REQUEST (签发 capability)             │
│         ...                                                     │
│  [7:0]   Flags (8 bits):                                       │
│         Bit 0: Sync (同步 / 异步)                              │
│         Bit 1: Bulk (经 Shared Memory, 不经 Queue)            │
│         Bit 2-7: Reserved                                       │
│                                                                  │
│  Payload: 变长 (per Command Type)                              │
│  - MMAP: VA, size, prot, flags                                 │
│  - MUNMAP: VA, size                                            │
│  - DRAIN: target_port, timeout_ms                             │
│  - HRT_UPDATE: HRT_index, hrt_data                            │
│  - TENANT_CREATE: tenant_id, quota                            │
│  - CAPABILITY_REQUEST: capability_spec                        │
└─────────────────────────────────────────────────────────────────┘

VirtIO Notification Format (GSP → Host):

┌─────────────────────────────────────────────────────────────────┐
│  VirtIO Notification (16 bytes)                                 │
│                                                                  │
│  [15:8]  Event Type (8 bits):                                  │
│         0x01: PAGE_FAULT_RESOLVED                              │
│         0x02: RAS_ERROR (ECC Error, Poison)                    │
│         0x03: HRT_SWAP_DONE                                     │
│         0x04: DRAIN_COMPLETE                                    │
│         0x05: CAPABILITY_REVOKED                                │
│         0x06: FABRIC_TOPOLOGY_CHANGE                            │
│         ...                                                     │
│  [7:0]   Severity (8 bits):                                    │
│         0x00: Info, 0x01: Warning, 0x02: Fatal               │
│                                                                  │
│  Payload: 变长 (per Event Type)                                │
│  - PAGE_FAULT_RESOLVED: VA, ctx, fabric_addr                   │
│  - RAS_ERROR: hbm_bank, ecc_type, syndrome                    │
│  - HRT_SWAP_DONE: swap_count, duration_us                     │
│  - ...                                                          │
└─────────────────────────────────────────────────────────────────┘
```

### §5.3 Host Comm 性能

```
Host Comm 性能估算 (per VirtIO 消息):

Control 路径 (Command Queue):
  - 1 次消息往返 (Host → GSP → Host): ~5-10 μs
  - 含 PCIe MMIO + Queue 操作 + GSP 处理

Notification 路径 (Notification Queue):
  - GSP → Host MSI-X 中断: ~1 μs
  - Host 处理 MSI-X + 读 Queue: ~5 μs

Bulk Transfer 路径 (Shared Memory):
  - 经 PCIe DMA 4 KB: ~1 μs (per Host-GPU PCIe 协议)
  - 适合: Page Table 大块传输, Capability Token 列表

对比:
  - V3.1-Rev2.0 Host KMD 直处理: ~5-10 μs (ioctl + MMIO)
  - GSP-RM 直处理: <1 μs (HW + NV-RTOS)
  - 加速: 5-10x (与 NVIDIA Hopper 业界一致)
```

---

## §6 v1.0 MVP 范围

### §6.1 v1.0 MVP 实施范围

- [x] **GSP 硬件规格定义** (RISC-V + 256 KB Code SRAM + 1 MB Data SRAM)
- [x] **GSP 启动流程定义** (Boot ROM → Host 加载固件 → NV-RTOS 启动)
- [x] **GSP ↔ GPU Compute 接口定义** (Mailbox + Interrupt + Shared Memory)
- [x] **微内核选型** (GSP-RM 用 NV-RTOS, Switch FM 用 seL4)
- [x] **4 大服务接口定义** (Memory / Fault / Fabric / Tenant Manager)
- [x] **Host Comm VirtIO 接口** (Command + Notification + Shared Memory)
- [x] **与 V3.1-Rev2.0 CIU / HRT / GMMU 兼容性定义**

### §6.2 v1.0 MVP 不实施范围 (推迟到 v1.x / v3.x)

- ❌ **GSP 硬件 RTL 实现**: 推迟到 RTL 实施阶段
- ❌ **GSP-RM 固件完整代码**: 推迟到 v1.x (参考 NVIDIA NV-RTOS)
- ❌ **4 大服务完整实现**: 推迟到 v1.x (Phase 1: Memory + Fault)
- ❌ **seL4 在 Switch FM 实施**: 推迟到 v1.x (per `21-dist-scale-up-topology-b.md`)
- ❌ **Capability 完整支持**: 推迟到 v1.x (per `27-nsa-capability.md`)
- ❌ **NSA-aware MMU 协同**: 推迟到 v1.x (per `25-nsa-hardware.md`)

### §6.3 v1.0 MVP 验证标准 (10 项 Acceptance Gate)

- [ ] **AG1**: GSP 硬件规格定义完成 (RISC-V + SRAM + Mailbox)
- [ ] **AG2**: GSP 启动流程定义 (Boot ROM → Host 加载)
- [ ] **AG3**: GSP ↔ GPU Compute 接口 (Mailbox + Interrupt + Shared Memory)
- [ ] **AG4**: 微内核选型推荐 (NV-RTOS for GSP-RM, seL4 for Switch FM)
- [ ] **AG5**: 4 大服务接口定义 (Memory / Fault / Fabric / Tenant)
- [ ] **AG6**: Memory Service 详细功能 (Page Table 构建 + HW Directory 协同)
- [ ] **AG7**: Fault Service 详细功能 (5 类 HW Fault 处理)
- [ ] **AG8**: Fabric Service 详细功能 (HRT Swap + Drain 协议)
- [ ] **AG9**: Tenant Manager 详细功能 (Capability 签发)
- [ ] **AG10**: Host Comm VirtIO 接口 (Command + Notification + Shared Memory)
- [ ] **AG11**: 与 V3.1-Rev2.0 CIU / HRT / GMMU 兼容性定义
- [ ] **AG12**: 0 个新 ABI 函数 (per ADR-088 §D5)

---

## §7 开放问题 (待新 session 讨论)

| # | 开放问题 | 优先级 | 关联草案 |
|---|---------|--------|---------|
| 1 | **GSP 微控制器选型**: RISC-V vs ARM Cortex-R? (RISC-V 生态优势) | P1 | 草案 5 |
| 2 | **GSP 频率选择**: 200 MHz vs 400 MHz? (性能 vs 功耗) | P1 | 草案 5 |
| 3 | **Memory Service HW Directory 协同**: HW atomic vs SW lock? | P1 | 草案 4, 5 |
| 4 | **Host Comm 消息格式**: 固定 16 bytes vs 变长? | P2 | 草案 5 |
| 5 | **Bulk Transfer Shared Memory 大小**: 4 MB / 16 MB? | P2 | 草案 5 |
| 6 | **GSP Watchdog 超时时间**: 1 s vs 5 s? (Host 失联检测) | P2 | 草案 5 |
| 7 | **Memory Service 跨节点 Page Table 查询**: 直接经 PCIe over UALink vs 经 Switch FM? | P2 | 草案 5, 7 |
| 8 | **Capability Token 撤销延迟**: 即时 vs 批量? | P3 | 草案 6 |
| 9 | **GSP-RM 与 Switch FM 通信**: Sideband SMBus vs PCIe over UALink? | P3 | 草案 5, 7 |
| 10 | **GSP-RM 形式化验证范围**: seL4-only vs NV-RTOS-only vs 全 GSP-RM? | P3 | 草案 5 |

---

## §8 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v0.1-draft | Sisyphus | 首版: GSP-RM 微控制器 + 固件架构 v0.1 (草案 5, 8 章节 + 12 项 Acceptance Gate + 10 个开放问题) |

---

**关联 OpenSpec change**: 待 `openspec/changes/2026-09-19-cpptlm-mas-gsp-rm-firmware/` 提案创建
**下次更新**: Oracle 评审反馈后 v0.2

**关键定位**: 本规范是 NSA-aware 分布式 Scale-Up (方案 C) 的**控制面下沉核心**, 借鉴 NVIDIA H100 GSP-RM + 你提出的 seL4 微内核思想。v1.0 MVP 仅定义规格, 完整实施推迟到 v1.x。地址格式见 [`22-nsa-fabric-address-spec.md`](22-nsa-fabric-address-spec.md) 草案 1, 硬件见 [`25-nsa-hardware.md`](25-nsa-hardware.md) 草案 4。