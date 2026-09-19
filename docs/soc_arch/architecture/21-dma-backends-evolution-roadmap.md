# dGPU DMA Backends 演进路线图 (Unified Bridge Core + Protocol Engines Evolution Roadmap)

> **目的**: 定义 CppTLM dGPU SoC **MAS-3.1** 中 **Backends 层** (UBC 共享基座 + HBM/NIC/IO 协议引擎) 的多阶段演进路径 (v1.0 MVP → v3.0 完整 Backend 矩阵), 确立每个阶段的 **shippable 价值** + **前置依赖** + **不引入项** + **无债务演进约束**, 确保整个演进过程不产生"未来需清理"的债务。
>
> **状态**: Draft v1.0 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: 待 `openspec/changes/2026-09-19-cpptlm-mas-dma-backends-mvp/` 提案对齐
> **关联文档**:
> - [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) — **Backends v1.0 MVP 详细设计** (本路线图第一阶段 SSOT)
> - [`21-tee-udd-evolution-roadmap.md`](21-tee-udd-evolution-roadmap.md) — TEE-UDD 演进路线图 (上游协同)
> - [`21-fabric-switch-evolution-roadmap.md`](21-fabric-switch-evolution-roadmap.md) — Fabric+Switch 演进路线图 (协议层协同)
> - [`21-microarch-ifc-evolution-roadmap.md`](21-microarch-ifc-evolution-roadmap.md) — MicroArch+IFC 演进路线图 (实现层协同)
> - [`21-soc-topology-mvp.md`](21-soc-topology-mvp.md) — SoC 顶层物理布局规范 (V3.1-Rev2.0 拓扑修正: NIC-DMA 是 GPU 与外部 Switch 唯一接口)
> - [`20-gmmu-evolution-roadmap.md`](20-gmmu-evolution-roadmap.md) — GMMU 演进路线图 (翻译层协同)
> **关联 ADR**:
> - ADR-088 §D5 — 23 ABI 冻结 (本路线图 v1.0 不动 ABI)
> - ADR-SOC-10 (待起草) — TEE/UDD/UBC 三层解耦 (本路线图边界)
> - ADR-SOC-21 — **V3.1-Rev2.0 拓扑修正** (NIC-DMA 是 GPU 与外部 Switch 唯一接口)

---

## §0 阅读引导

- 想理解 Backends 整体战略 → 读 §1 (范围与目标) + §2 (阶段总览)
- 想理解 v1.0 MVP 的具体内容 → 读 §3
- 想理解后续阶段 (v1.1 / v2.0 / v2.1 / v3.0) 边界 → 读 §4
- 想理解"无债务演进"的不变量 → 读 §5
- 想理解 v1.0 的端到端 demo → 读 §6
- 想理解跨仓契约 → 读 §7
- 想查阅反面模式与不做项 → 读 §8

---

## §1 范围与目标

### §1.1 为什么需要 Backends 演进路线图

CppTLM dGPU 当前缺失**统一的 Backend 微架构基座**:

**旧方案 (V3.0 独立 DMA)**:
- `NicDmaTLM` / `IoDmaTLM` / `HbmDmaTLM` 三个独立模块
- 每个 DMA 独立实现 NoC 接口 + Tracker + QoS + RAS
- 无协议层抽象, 每个 DMA 与物理协议强耦合
- 无面积复用, 三个独立控制器占 Die 面积大

**实际使用**:
- 三个 DMA 各自配置在 `dgpu_board_shell.cc` 中
- 各自走不同的 BAR MMIO 配置寄存器
- 各自写 UVM testbench, 验证方法复用度低

**隐患**:
- 无协议抽象 → 加新 Backend (如 UALink 1.1, CXL 3.0) 必须新增独立 DMA 控制器
- 无面积复用 → Die 面积压力, 难以生产 16 GPU Die
- 无统一 RAS → 错误码不一致, GPC AWT Trap 无法统一处理
- 无统一流控 → 三个 DMA 拥塞策略不一致, NoC 死锁风险

**Backends 演进的总体目标**: 用 6 个阶段逐步把 Backends 从"HBM 单后端独立 DMA"建设到"完整 UBC + 协议引擎 + 多 Backend 矩阵 + 高级特性"的统一架构, **每个阶段 shippable + 不制造未来清理债务**。

### §1.2 v1.0 MVP 的核心价值 (用户明确目标)

> **"Backends MVP 可让 TEE-UDD 通过统一 UBC 接口访问 Local HBM, 并通过 NIC-DMA (UALink) 访问 Peer GPU HBM"**

具象化为 1 个端到端测试 (demo 详见 §6):

```
TEE Frontend 完成 GMMU 翻译 + HRT 查表 → Route_Tag=0 (HBM)
  ↓
Global UDD Hub 仲裁 + Credit Acquire → backend_out[HBM-DMA]
  ↓
UBC NoC Flit Terminator (CRC + VC 解复用) → 分配 Tracker_ID
  ↓
UBC QoS Arbiter (Token Bucket 调度) → HBM-DMA Protocol Engine
  ↓
HBM-DMA Address Decoder (GUPA → PC/Bank/Row/Col) + FR-FCFS 调度
  ↓
HBM3e PHY Command 发出 → 数据返回 → ECC SECDED 验证
  ↓
UBC ROB 组装 UDD Response → 注入 NoC 返回 GPC → TEE Frontend 接收
```

### §1.3 命名约定 (per MVP §1.3)

| 旧名 (V3.0) | 新名 (V3.1-Rev2.0) | 原因 |
|--------------|---------------------|------|
| `NicDmaTLM` | `NicDmaUbc` (UBC + NIC Protocol Engine) | 强调"UBC 共享基座 + UALink 协议引擎" |
| `IoDmaTLM` | `IoDmaUbc` (v1.1 引入) | 同上 |
| `HbmDmaTLM` | `HbmDmaUbc` | 同上 |
| **新增** | `UbcBaseTLM` | UBC 共享基座基类 |
| **新增** | `UbcHbmEngineTLM` / `UbcNicEngineTLM` / `UbcIoEngineTLM` | 协议引擎子模块 |

---

## §2 阶段总览 (时间线 + 关键约束)

```
v1.0 MVP         v1.1              v2.0             v2.1            v3.0
   ●───────────────●─────────────────●────────────────●───────────────●
   │  HBM + UALink  │  + PCIe IO       │  + CXL Backend │  + Atomic    │  + Multicast
   │  ROB=1024     │  + ATS/PRI        │  + PCIe Gen6  │  + Switch Rdy │  + All-Reduce
   │  Go-Back-N    │  + GDS 零拷贝     │  + Hot-plug    │  + Page Retire│  + HW Reduce
   │  Drain FSM    │  + L1/L2 Cache   │  + Refresh Opt  │  + Drain FSM  │  + Coherence
```

| 阶段 | 时间估算 | 核心 shippable 价值 | 关键依赖 | 不引入 (仍推迟) |
|------|----------|---------------------|----------|------------------|
| **v1.0 MVP** | 2-3 周 | HBM-DMA + NIC-DMA (UALink 1.1) 完整通路; UBC 共享基座 (NoC + ROB + QoS + RAS); HBM FR-FCFS + ECC + Page Retirement; UALink Flit + Retry/Replay + Drain FSM | TEE-UDD v1.0 ship + GMMU v1.0 ship + UALink Protocol Engine BFM (per `21-fabric-switch-mvp.md`) | PCIe IO / ATS / PRI / GDS / CXL Backend / Atomic / Multicast |
| **v1.1** | 2-3 周 | + IO-DMA (PCIe Gen6 + ATS/PRI + GDS Zero-Copy); + PCIe P2P Write TLP Bypass; + Host SVA DMA 完整 zero-copy path | v1.0 ship + PcieEndpointIP 加 ATS 能力 + 23 ABI 扩展 (per ADR-088 §D5) + [tee-udd v2.0] 协同 | CXL Backend / Atomic / Multicast |
| **v2.0** | 3-4 周 | + CXL Backend (通过外部 Scale-Up Switch PTE); + Hot-plug / Hot-unplug 支持; + Refresh 优化 (PBR 比例调整); + L1/L2 Cache 协同 Invalidate | v1.1 ship + [fabric-switch v2.0] CXL PTE 协议锁定 + Switch 物理搭建 + 23 ABI 扩展 (CXL Type-3 Memory) | Atomic / Multicast / All-Reduce |
| **v2.1** | 2 周 | + Atomic Operations (Atomic Add/Sub/CmpAndSwap); + 分布式 Switch 协同 Drain FSM (跨多个 Switch Port); + HBM-DMA Page Retirement 阈值动态化 | v2.0 ship + 多 Switch 拓扑协调 | Multicast / All-Reduce / Coherence |
| **v3.0** | 4-6 周 | + UALink Multicast (All-Reduce / All-Gather 硬件加速); + Switch Hardware Reduce Engine (FP8/FP16/BF16 加法树); + L2 Coherence 协议 (CXL Back-invalidate 经 SnoopFilter) | v2.1 ship + CXL/UALink Multicast 协议锁定 + Coherence SnoopFilter 实现 | (无更后项 — 收敛) |

---

## §3 v1.0 MVP 范围 (详细)

### §3.1 v1.0 MVP 必含的 9 个最小能力

| # | 能力 | 必含理由 |
|---|------|---------|
| **1** | UBC 共享基座 (NoC Terminator + ROB 1024 + QoS Token Bucket + RAS Injector) | 没有 UBC, 三种 Backend 各自重复实现, 违背 RTL 复用原则 |
| **2** | HBM-DMA 完整通路 (Address Decoder + FR-FCFS + Bank Manager + ECC + Refresh) | 没有 HBM-DMA, GPU 无法访问主存, 整个数据面失效 |
| **3** | HBM-DMA FR-FCFS First-Ready 调度 | 没有 First-Ready, Row Buffer 命中率低, 性能 regression |
| **4** | HBM-DMA ECC SECDED (单比特纠错 + 双比特检错) | 没有 ECC, 不可生产真实硬件容错场景 |
| **5** | HBM-DMA Page Retirement (3 CE 阈值) | 没有 Page Retire, 物理坏页无法隔离, 长期使用累积错误 |
| **6** | NIC-DMA UALink Flit Packager/Parser | 没有 Flit 封装, Scale-Up 协议失效 |
| **7** | NIC-DMA Retry/Replay (Go-Back-N) | 没有 Retry/Replay, UALink 链路错误无法恢复 |
| **8** | NIC-DMA 两阶段 Drain FSM (100us 超时) | 没有 Drain, CXL 内存池热插拔不可行 |
| **9** | NIC-DMA Remote CXL Poison Mapper (UALink → Status=0x1) | 没有 Poison 映射, CXL 内存错误无法传递到 Warp |

### §3.2 v1.0 MVP 推迟到后续阶段的 9 项

| # | 推迟项 | 推迟到 | 不引入原因 |
|---|--------|--------|------------|
| **1** | IO-DMA (PCIe Gen6 + ATS/PRI + GDS Zero-Copy) | v1.1 | PcieEndpointIP 当前不支持 ATS (per `dgpu-soc-pcie-slice.md §0.1`) |
| **2** | PCIe P2P Write TLP Bypass | v1.1 | IO-DMA 推迟到 v1.1, P2P 同步推迟 |
| **3** | Host SVA DMA 完整 zero-copy | v1.1 | 需要 ATS 协议支持 |
| **4** | CXL Backend (通过外部 Switch PTE) | v2.0 | Scale-Up Switch CXL PTE 协议未锁定 |
| **5** | Hot-plug / Hot-unplug | v2.0 | 需要 Switch 物理搭建 |
| **6** | Atomic Operations (Atomic Add/Sub/CmpAndSwap) | v2.1 | HBM3e 支持, 但需 CXL Backend 协同 |
| **7** | 分布式 Switch Drain FSM | v2.1 | 需要多 Switch 拓扑协调 |
| **8** | UALink Multicast (All-Reduce / All-Gather) | v3.0 | CXL.mem 原生不支持 Multicast, 需 Switch 拆分 |
| **9** | Hardware Reduce Engine (Switch 加法树) | v3.0 | 基础版仅支持标准 Mem 路由, Multicast 推迟 |

---

## §4 后续阶段详细边界

### §4.1 v1.1: IO-DMA (PCIe Gen6) + ATS/PRI + GDS Zero-Copy

| 维度 | 边界 |
|------|------|
| **新增能力** | IO-DMA (UBC + PCIe Protocol Engine); PCIe Gen6 x16 (64 GT/s, PAM4); ATS Translation Request (SVA VA → GMMU); PRI Page Request (GMMU Page Fault → PCIe PRI TLP); GDS Zero-Copy Bypass (NVMe P2P Write → HBM-DMA); 中断聚合 (MSI-X 向量化) |
| **前置依赖** | v1.0 ship + PcieEndpointIP 加 ATS 能力 (per `dgpu-soc-pcie-slice.md §0.1` 高级可选阶段启动) + 23 ABI 扩展流程 (per ADR-088 §D5) + [tee-udd v2.0] 协同 |
| **不引入** | CXL Backend / Atomic / Multicast |
| **验证标准** | Host SVA DMA 完整 zero-copy path; Page Fault → PRI → 自动重试 latency < 100us; GDS NVMe → HBM 延迟 < PCIe DMA 80% |
| **关键文件** | `src/tlm/dma/ubc_io_engine_tlm.{hh,cc}` 实现; `src/tlm/dma/io_dma_ubc_tlm.{hh,cc}` 派生; `src/tlm/pcie/pcie_endpoint_ip.cc` 加 ATS Translation 拦截; **23 ABI 扩展**: `cpptlm_emulator_ats_translate_request` 新增 |

### §4.2 v2.0: CXL Backend + Hot-plug + Refresh 优化

| 维度 | 边界 |
|------|------|
| **新增能力** | CXL Backend (经外部 Scale-Up Switch PTE, GPU 侧仅处理 UALink); Hot-plug / Hot-unplug (FM 经 Sideband 通知 Switch + GPU HRT Atomic Swap); Refresh 优化 (PBR 比例动态调整, 节省 Refresh 开销); L1/L2 Cache 协同 Invalidate (TEE 写入 Local HBM 时, 若 L2 存在 Dirty 副本, 自动 Invalidate) |
| **前置依赖** | v1.1 ship + [fabric-switch v2.0] CXL PTE 协议锁定 + Switch 物理搭建 + 23 ABI 扩展 (CXL Type-3 Memory driver 协调) |
| **不引入** | Atomic / Multicast / All-Reduce |
| **验证标准** | CXL 内存池动态挂载/卸载延迟 < 100ms; HRT Atomic Swap 期间 GPU 无功能中断 (per `21-tee-udd-mvp.md` §7.3); Refresh 开销降低 30% |
| **关键文件** | `src/tlm/dma/ubc_cxl_engine_tlm.{hh,cc}` 新建 (经 NIC-DMA 复用); `hbm_dma_ubc_tlm.cc` 加 Refresh 优化; `tee_tlm.cc` 加 L2 Cache Invalidate 协同 |

### §4.3 v2.1: Atomic Operations + 分布式 Drain FSM

| 维度 | 边界 |
|------|------|
| **新增能力** | Atomic Operations (Atomic Add/Sub/CmpAndSwap 经 HBM-DMA, 支持 FP32/INT32/INT128); 分布式 Switch Drain FSM (跨多个 Switch Port 并行 Drain, 协调 Backends); HBM-DMA Page Retirement 阈值动态化 (按温度/电压调整, 3~10 CE 范围) |
| **前置依赖** | v2.0 ship + 多 Switch 拓扑协调 (per `21-fabric-switch-evolution-roadmap.md` §4.2) + 23 ABI 扩展 (Atomic ABI) |
| **不引入** | Multicast / All-Reduce / Coherence |
| **验证标准** | Atomic Add 完整 HBM 通路, latency < 200ns; 分布式 Drain 多 Port 协同 < 200us; Page Retirement 动态阈值正确 |
| **关键文件** | `src/tlm/dma/hbm_dma_atomic.{hh,cc}` 新建; `nic_dma_ubc_tlm.cc` 加分布式 Drain; `hbm_dma_ubc_tlm.cc` 加动态 Page Retirement |

### §4.4 v3.0: UALink Multicast + Hardware Reduce + Coherence

| 维度 | 边界 |
|------|------|
| **新增能力** | UALink Multicast (NIC-DMA 支持 Multicast-Write, 跨多 Peer GPU); Switch Hardware Reduce Engine (FP8/FP16/BF16 加法树, 经 UALink Reduce_Op 拦截); L2 Coherence 协议 (CXL Back-invalidate 经 SnoopFilter 精准 Invalidate GMMU L2) |
| **前置依赖** | v2.1 ship + CXL/UALink Multicast 协议锁定 + [fabric-switch v3.0] Reduce Engine 实现 + Coherence SnoopFilter 实现 |
| **不引入** | (无 — 收敛) |
| **验证标准** | All-Reduce 8 GPU 完成时间 < 10us (8x4096 element, FP16); Switch Hardware Reduce 正确性 (与软件 Reduce 一致性 < 1 ULP); Coherence Back-invalidate latency < 1us |
| **关键文件** | `src/tlm/dma/nic_dma_multicast.{hh,cc}` 新建; `src/tlm/pcie/switch_reduce_engine_tlm.{hh,cc}` 新建 (per fabric-switch); `src/tlm/gpu/coherence_snoop_filter.{hh,cc}` 新建; `gmmu_tlm.cc` 加 Back-invalidate 接收 |

---

## §5 无债务演进约束 (4 条不变量)

### §5.1 不变量 1: UDD Micro-op 接口在演进中只增不换

> v1.0 MVP ship 后, 通过 UBC 看到的 `UddMicroOp` 格式:
> - **v1.0**: 160-bit (header + gupa + payload + tracker + seq_route)
> - **v2.0+**: 仅扩展 header 字段新增位 (如 Remote_Poison_Type, Atomic_Op_Type), 不破坏现有字段布局
>
> **承诺**: v2.0+ 引入新特性时, 旧调用点不受破坏, **新增位仅在 header 字段预留位扩展**, 旧字段布局保留含义不变。

**实现方式**: v1.0 MVP 的 `header` 字段 32-bit, 高 8-bit (bits[31:24]) 预留演进扩展位。v1.0 仅使用低 24-bit (Opcode, Size, VC, QoS, Context_ID)。

### §5.2 不变量 2: UBC 共享基座从 v1.0 起定义所有子模块

> v1.0 MVP 即定义 UBC 共享基座接口 (`UbcBaseTLM` 基类), 即使 v1.0 仅 HBM/NIC 两种 Protocol Engine。
>
> v1.1+ 加 IO Protocol Engine 时**仅新增派生类**, 不修改基类。

**实现方式**: `UbcBaseTLM` 基类从 v1.0 起定义完整虚函数接口 (Strategy Pattern), 派生类必须实现 `engine_tick()` (clk_io 域处理)。v1.1 加 IO 时仅新增 `IoDmaUbcTLM` 派生类。

### §5.3 不变量 3: Status Code 从 v1.0 起定义完整 RAS 映射表

> v1.0 MVP 即使仅 HBM/NIC 实际使用, `UbcStatusCode` 枚举也定义完整 (Success + LocalHbmPoison + RemoteCxlPoison + LocalFault + PageFault + HostIoFault)。
>
> v1.1+ 加 IO Protocol Engine 时**仅启用** `HostIoFault`, 不重构枚举。

**实现方式**: `enum class UbcStatusCode { Success, LocalHbmPoison, RemoteCxlPoison, LocalFault, PageFault, HostIoFault }` 从 v1.0 起定义。v1.0 仅使用前 3 个值, v1.1 启用 `HostIoFault`。

### §5.4 不变量 4: ROB Tracker 容量从 v1.0 起按 Backend 类型预留

> v1.0 MVP 即定义 1024-entry ROB 容量, 即使 v1.0 仅 HBM/NIC 实际使用。
>
> v1.1+ 加 IO Protocol Engine 时**沿用** 1024-entry 容量 (不强制修改), 或可选支持独立 ROB 容量。

**实现方式**: `static constexpr size_t ROB_ENTRIES = 1024` 从 v1.0 起固定。v1.1+ 派生类可 override ROB_ENTRIES, 但默认保持 1024-entry 不变。

---

## §6 v1.0 MVP 端到端 Shippable Demo (12 步测试)

这是 v1.0 MVP 必须通过的"绿灯测试", 证明整个 **TEE-UDD → UBC → HBM-DMA → HBM** 链路贯通:

```
┌──────────────────────────────────────────────────────────────────────┐
│  v1.0 MVP Test: test_backends_hbm_e2e                                │
├──────────────────────────────────────────────────────────────────────┤
│  Step 1: Driver 经 PCIe MMIO 写 UDD HRT (per tee-udd-mvp §2)          │
│            - CIU_REG_HRT_SHADOW[0] = {Route_Tag=0x0(本地HBM),        │
│                                        VC=0, QoS=Normal}            │
│            - CIU_REG_HRT_CTRL.SWAP_BIT = 1                            │
│                                                                       │
│  Step 2: SM 发起 Load 请求 (per tee-udd-mvp Step 3)                   │
│            - VA=0x1000_0000, ctx=0, size=128B                         │
│                                                                       │
│  Step 3: TEE Frontend 流水线处理                                       │
│            - GMMU translate → GUPA=0x8000_0000                       │
│            - HRT 查表 → Route_Tag=0x0, VC=0                           │
│            - AGU 拆分 128B → 1 个 128B Micro-op                       │
│                                                                       │
│  Step 4: UDD Agent 仲裁 (WRR, TC:SM=4:1) + NoC Injection              │
│                                                                       │
│  Step 5: Global UDD Hub 路由 (RCT 校验通过) + Credit Acquire          │
│            - Credit[backend=HBM-DMA] = 128 → 127                      │
│            - Micro-op 注入 backend_out[HBM-DMA]                       │
│                                                                       │
│  Step 6: UBC NoC Flit Terminator                                       │
│            - CRC 校验 + VC 解复用 (VC=0)                              │
│            - 分配 Tracker_ID (1024-entry ROB 池)                      │
│                                                                       │
│  Step 7: UBC QoS Arbiter                                               │
│            - 按 QoS_Priority 调度 → HBM-DMA Protocol Engine           │
│            - Token Bucket 限速 (low priority throttle)                │
│                                                                       │
│  Step 8: HBM-DMA Protocol Engine                                       │
│            - Address Decoder & Bank Manager                          │
│              → GUPA 解码: PC=0, BG=0, Bank=3, Row=0x1000, Col=0x100  │
│            - FR-FCFS Scheduler: Row Hit → 优先调度                    │
│            - HBM3e Command 发出 (RD + 128B)                           │
│            - 数据从 HBM Stack 返回                                     │
│                                                                       │
│  Step 9: HBM-DMA ECC 处理                                             │
│            - SECDED: 单比特纠错 + 双比特检错                          │
│            - 正常情况 → 透传数据 + Status=0x0                          │
│            - 异常情况 → Status=0x2 (Local_Fault)                       │
│                                                                       │
│  Step 10: UBC ROB 释放 + 组装 UDD Response                             │
│            - Tracker_ID 匹配原始请求                                  │
│            - 注入 NoC 返回 GPC                                        │
│            - Credit[backend=HBM-DMA] = 127 → 128 (归还)                │
│                                                                       │
│  Step 11: TEE Frontend 接收 Response + 写回 SM Register File         │
│                                                                       │
│  Step 12: 验证                                                         │
│             - SM 读到 expected value                                   │
│             - ROB Tracker_ID 复用正确                                 │
│             - QoS Arbiter 公平调度 (Token Bucket 限速生效)            │
│             - ECC SECDED 单比特纠错触发后, 数据透明传输                │
│             - ECC DED 触发后, Status=0x2 (Local_Fault) 返回          │
└──────────────────────────────────────────────────────────────────────┘
```

**验收标准**: 12 步全部通过 + 测试用例 ≥ 10 个 (见 [`21-dma-backends-mvp.md` §11 测试覆盖](21-dma-backends-mvp.md))。

---

## §7 跨仓契约与 UsrLinuxEmu 协调

### §7.1 跨仓职责边界

| 维度 | CppTLM 仓 (本仓) | UsrLinuxEmu 仓 (driver 侧) |
|------|------------------|--------------------------|
| **UBC 寄存器布局** | 定义 Backends MMIO (见 `21-dma-backends-mvp.md` §9) | driver 按布局写 BAR |
| **HBM PHY 训练** | CppTLM 模拟 PHY 训练序列 | driver 协调 Host BIOS 设置 HBM 频率 |
| **UALink 链路训练** | CppTLM 模拟 UALink PHY LTSSM | driver 通过 Sideband (SMBus) 配置 Switch |
| **Drain 触发** | NIC-DMA 接受 FM 触发 (per `21-microarch-ifc-mvp.md` §4.2) | driver 写 NIC_DRAIN_CTRL.START_DRAIN |
| **Page Retirement** | HBM-DMA 自动检测 + SMI 中断 | driver 接 SMI + 替换逻辑页 |
| **23 ABI 签名** | `cpptlm_dma_translate_cb` 不变 | driver 端实现回调 |

### §7.2 UsrLinuxEmu 改造路径

| 原 SDMA 实现 | 新 Backends 实现 | 差异 |
|--------------|-------------------|------|
| `sdma_engine` BAR MMIO descriptor ring | driver 写 Backends BAR MMIO (HBM/NIC 各自 CSR) | driver 视角从"统一 SDMA"变为"HBM-DMA / NIC-DMA 独立 CSR" |
| 无 UBIC Tracker 概念 | driver 配置 ROB 容量 (v1.0 默认 1024) | driver 需理解 ROB 概念 |
| 无 QoS Token Bucket | driver 配置 QoS class (8 class, 默认 Normal) | driver 可按任务类型配置优先级 |
| 无统一 RAS | driver 接收 MSI-X + 解析 Status Code | driver 需实现统一 RAS 处理 |

### §7.3 跨仓 PR 协调

按 ADR-091 §R5.1 跨仓 PR 流程:
1. CppTLM 仓: 实现 UBC + HBM/NIC Protocol Engine + 16+ 测试
2. UsrLinuxEmu 仓: driver 改造 SDMA → Backends 风格 (由用户承担协调)
3. 跨仓集成测试: `test_backends_hbm_e2e_ue.cc` + `test_backends_nic_e2e_ue.cc`
4. 同步 PR (无 ABI 破坏, 跨仓风险低)

---

## §8 反模式 (明确不做)

| 反模式 | 不做的原因 |
|--------|-----------|
| ❌ **完整 NVIDIA Hopper HBM Controller RTL 复刻** | CppTLM 是 TLM 行为级仿真, 非 RTL 仿真 (per `00-overview.md §9`) |
| ❌ **三个独立 DMA 控制器** | 违背"UBC 共享"原则, 增加面积 12% |
| ❌ **每个 Backend 独立 RAS 码** | 违背"统一 RAS 注入器"原则, GPC AWT Trap 无法统一处理 |
| ❌ **Page Retirement 阈值硬编码无法关闭** | 违背"驱动可控"原则, driver 应能 disable |
| ❌ **Drain FSM 超时硬编码** | 违背"驱动可配置"原则, 应通过 CSR 配置 |
| ❌ **UALink Retry/Replay 无超时** | 违背"链路错误恢复"原则, 必须有 Timeout fallback |
| ❌ **P2P Bypass 经 UBC + Memory NoC** | 违背"低延迟 P2P"原则, 增加 15% 延迟 (per MAS-3.1-DMA-Backends §3.1.3) |
| ❌ **CXL Backend 在 GPU 侧实现** | 违背"GPU 协议栈极简"原则 (per MAS-3.1-Fabric §1), CXL 应在 Switch 侧 |
| ❌ **Host 真实 CXL Type-3 driver 在 v1.0** | v1.0 仅 HBM + UALink, v2.0 才加 CXL |
| ❌ **Multicast 在 v1.0** | CXL.mem 不支持, 需 UALink 协议 + Switch Reduce Engine 协同 (v3.0) |

---

## §9 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: Backends 5 阶段演进路线图 (v1.0/v1.1/v2.0/v2.1/v3.0) + 4 条不变量 + 12 步 shippable demo + 上下游协同路线图 |

---

**关联 OpenSpec change**: 待 `openspec/changes/2026-09-19-cpptlm-mas-dma-backends-mvp/` 提案创建
**下次更新**: Oracle 评审反馈后 v1.1 (IO-DMA 引入)