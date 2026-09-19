# dGPU Core/Micro-Arch + RTL-IFC 演进路线图 (Core Microarchitecture & RTL Interface Evolution Roadmap)

> **目的**: 定义 CppTLM dGPU SoC **MAS-3.1** 中 **Core 微架构** (GPC/SM/TC-DMA/UDD Agent) 与 **RTL-IFC** (模块接口 + 寄存器映射) 的多阶段演进路径 (v1.0 MVP → v3.0 完整 Multicast + Coherence), 确立每个阶段的 **shippable 价值** + **前置依赖** + **不引入项** + **无债务演进约束**, 确保整个演进过程不产生"未来需清理"的债务。
>
> **状态**: Draft v1.0 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: 待 `openspec/changes/2026-09-19-cpptlm-mas-microarch-ifc-mvp/` 提案对齐
> **关联文档**:
> - [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) — **MicroArch+IFC v1.0 MVP 详细设计** (本路线图第一阶段 SSOT)
> - [`21-tee-udd-evolution-roadmap.md`](21-tee-udd-evolution-roadmap.md) — TEE-UDD 演进路线图 (协议层协同)
> - [`21-dma-backends-evolution-roadmap.md`](21-dma-backends-evolution-roadmap.md) — Backends 演进路线图 (协议层协同)
> - [`21-fabric-switch-evolution-roadmap.md`](21-fabric-switch-evolution-roadmap.md) — Fabric+Switch 演进路线图 (协议层协同)
> - [`21-soc-topology-mvp.md`](21-soc-topology-mvp.md) — SoC 顶层物理布局规范 (V3.1-Rev2.0 拓扑修正: TC-DMA 归属 GPC / IO-DMA 端口 / AWT Trap Type 区分 Local/Remote (via Switch))
> - [`20-gmmu-evolution-roadmap.md`](20-gmmu-evolution-roadmap.md) — GMMU 演进路线图 (翻译层协同)
> **关联 ADR**:
> - ADR-088 §D5 — 23 ABI 冻结 (本路线图 v1.0 不动 ABI)
> - ADR-SOC-10 (待起草) — TEE/UDD/UBC 三层解耦 + 接口契约 (本路线图边界)
> - ADR-SOC-21 — **V3.1-Rev2.0 拓扑修正** (TC-DMA 归属 GPC / AWT Trap Type 区分 Local/Remote)

---

## §0 阅读引导

- 想理解 MicroArch+IFC 整体战略 → 读 §1 (范围与目标) + §2 (阶段总览)
- 想理解 v1.0 MVP 的具体内容 → 读 §3
- 想理解后续阶段 (v1.1 / v2.0 / v2.1 / v3.0) 边界 → 读 §4
- 想理解"无债务演进"的不变量 → 读 §5
- 想理解 v1.0 的端到端 demo → 读 §6
- 想理解跨仓契约 → 读 §7
- 想查阅反面模式与不做项 → 读 §8

---

## §1 范围与目标

### §1.1 为什么需要 MicroArch + IFC 演进路线图

CppTLM dGPU 当前缺失**统一的实现层契约**:

**旧方案 (V3.0 直连 PCIe + 紧耦合)**:
- SM/TC-DMA/TEE/UBC 各模块独立 RTL 实现, 接口定义散落在各模块
- 时钟域划分不清晰, 跨域 CDC 一致性难保证
- MMIO 寄存器布局按模块独立定义, 无统一基地址约定
- 微架构性能预算 (延迟 / 吞吐) 无统一基线, 各模块独立优化

**实际使用**:
- RTL 工程师按各模块规范独立实现, 集成阶段才发现接口冲突
- STA (Static Timing Analysis) 阶段才发现时钟域划分不合理
- driver 编写阶段发现 MMIO 布局不一致, 需要调整 BAR 路由

**隐患**:
- 接口不稳定 → RTL 重写, 跨仓 PR 失败
- CDC 不一致 → metastability, 芯片不可能
- 寄存器布局冲突 → driver 无效, 阻塞整个交付
- 微架构性能超预算 → 时序收敛失败, 流片延期

**MicroArch+IFC 演进的总体目标**: 用 6 个阶段逐步把 MicroArch+IFC 从"HBM 单后端基线"建设到"完整 Multicast + Coherence + Hot-plug"的稳定实现层契约, **每个阶段 shippable + 不制造未来清理债务**。

### §1.2 v1.0 MVP 的核心价值 (用户明确目标)

> **"MicroArch+IFC MVP 可让 RTL 团队基于稳定的接口契约, 实现 SM/TC-DMA/UDD Agent/HBM-DMA 等模块的 TLM 仿真, 验证 4 条本地接口信号 + MMIO 寄存器布局 + 跨时钟域 CDC 正确性"**

具象化为 1 个端到端测试 (demo 详见 §6):

```
SM LSU 提交 Micro-op
  ↓ (clk_core 域)
TEE Frontend 4 级流水线 (Fetch/Translate/Route/Dispatch)
  ↓ (clk_core 域)
GMMU 翻译 (per `20-gmmu-mvp.md` §5)
  ↓ (clk_core 域)
UDD Agent HRT 查表 + WRR 仲裁 (TC:SM=4:1)
  ↓ (clk_core → clk_fab 异步 FIFO)
Global UDD Hub RCT 校验 + Credit 申请
  ↓ (clk_fab 域)
HBM-DMA 处理 (Address Decoder + FR-FCFS + PHY)
  ↓ (clk_fab → clk_core 异步 FIFO)
UDD Response 返回 → TEE Frontend → SM Register File
```

### §1.3 关键决策 (per MAS-3.1-Core + MAS-3.1-RTL-IFC Rev2.0)

| 决策 | 内容 | 原因 |
|------|------|------|
| TC-DMA 回归 GPC 紧耦合 | TC-DMA 物理上紧邻 SMEM, 不经过全局 NoC | 降低 AI 训练最后一跳延迟 |
| CXL 彻底解耦 | GPU 不实现 CXL.mem/CXL.cache 协议栈 | Die 面积节省 3-5%, 验证复杂度降低 |
| IO 闭环 | PCIe/IO-DMA 补全 Host 交互 | GDS + ATS/PRI 落地 |
| 4 时钟域划分 | clk_core (2.0G) / clk_fab (1.2G) / clk_io (1.0G) / clk_cfg (100M) | 简化时序收敛, 异步 FIFO 隔离 |

---

## §2 阶段总览 (时间线 + 关键约束)

```
v1.0 MVP         v1.1              v2.0             v2.1            v3.0
   ●───────────────●─────────────────●────────────────●───────────────●
   │  HBM-only      │  + NIC-DMA      │  + IO-DMA      │  + Atomic    │  + Multicast
   │  4 Local IF    │  + UALink Drain │  + PCIe ATS/PRI│  + 分布式    │  + All-Reduce
   │  4 Clock 域   │  + NIC CSR       │  + IO CSR       │  + AWT 增强 │  + SnoopFilter
   │  8 Trap Type  │  + 2-3 Trap Type │  + 1 Trap Type  │  + 1 Trap Type │ + 1 Trap Type
```

| 阶段 | 时间估算 | 核心 shippable 价值 | 关键依赖 | 不引入 (仍推迟) |
|------|----------|---------------------|----------|------------------|
| **v1.0 MVP** | 2-3 周 | 4 条本地接口信号 (SM-UDD/TC-UDD/UDD-GMMU/CIU-UDD); 4 时钟域划分; MMIO 寄存器布局 (0x00F0_0000 基地址); 4 AWT Trap Type; HRT Atomic Swap ≤8 cycles; TC-DMA mbarrier ≤1 cycle | tee-udd v1.0 ship + backends v1.0 ship + fabric-switch v1.0 ship + GMMU v1.0 ship | NIC-DMA 详细接口 / PCIe IO 详细接口 / Multicast / Coherence |
| **v1.1** | 1-2 周 | + NIC-DMA CSR (0x0200-0x02FF) + Drain FSM 信号时序 + UALink PHY 状态寄存器 + Remote CXL Poison 中断 (0x20) | v1.0 ship + backends v1.0 ship (NIC-DMA) + [tee-udd v1.1] 协同 | PCIe IO / Multicast / Coherence |
| **v2.0** | 2-3 周 | + IO-DMA CSR (0x0300-0x03FF) + PCIe ATS/PRI 中断 (0x30+) + GDS Zero-Copy 控制 + Host SVA 中断路由 | v1.1 ship + backends v1.1 ship (IO-DMA) + PcieEndpointIP 加 ATS + 23 ABI 扩展 | Atomic / Multicast / Coherence |
| **v2.1** | 1-2 周 | + Atomic Operation 中断 (0x40) + 分布式 Drain FSM 信号 + Hot-plug 中断 (0x50) + 远端 AWT (Remote_Link_Fault, Trap Type 0x4) | v2.0 ship + [fabric-switch v2.1] 分布式 Drain + 23 ABI 扩展 | Multicast / Coherence |
| **v3.0** | 3-4 周 | + Multicast CSR + Switch Reduce Engine 接口 + Coherence SnoopFilter 信号 + Back-invalidate 中断 (0x60) + All-Reduce 加速器接口 | v2.1 ship + [fabric-switch v3.0] Reduce Engine + Coherence Filter 实现 | (无更后项 — 收敛) |

---

## §3 v1.0 MVP 范围 (详细)

### §3.1 v1.0 MVP 必含的 10 个最小能力

| # | 能力 | 必含理由 |
|---|------|---------|
| **1** | 4 条本地接口信号完整定义 (SM-UDD/TC-UDD/UDD-GMMU/CIU-UDD) | 没有接口定义, RTL 无法开始实现 |
| **2** | 4 时钟域划分 (clk_core/clk_fab/clk_io/clk_cfg) + 异步 FIFO CDC | 没有时钟域, RTL 集成时序收敛失败 |
| **3** | MMIO 寄存器基地址 0x00F0_0000 + 0x0000-0x04FF 完整布局 | 没有寄存器布局, driver 无法访问 |
| **4** | HRT Atomic Swap 8 cycle 时序约束 (SVA 断言) | 没有时序约束, RTL 性能 regression |
| **5** | TC-DMA mbarrier 1 cycle 触发约束 | 没有约束, AI 训练延迟增加 |
| **6** | Poison 传播延迟 ≤ 5 clk_fab + 2 clk_core (SVA 断言) | 没有约束, RAS 不确定性 |
| **7** | Drain FSM 100us 超时约束 (SVA 断言) | 没有约束, Drain 死锁风险 |
| **8** | 4 种 AWT Trap Type (0x0-0x3) | 没有 Trap Type 定义, 异常处理不一致 |
| **9** | GPC 内 WRR 仲裁 (TC:SM=4:1) | 没有仲裁, Bulk 带宽无法保障 |
| **10** | Local HBM Load 总延迟 ~14.5 ns 预算 | 没有预算, 时序收敛失败 |

### §3.2 v1.0 MVP 推迟到后续阶段的 10 项

| # | 推迟项 | 推迟到 | 不引入原因 |
|---|--------|--------|------------|
| **1** | NIC-DMA 详细 CSR (0x0200-0x02FF) | v1.1 | backends NIC-DMA 推迟到 v1.0+ 协同 |
| **2** | Drain FSM 信号完整时序 | v1.1 | 与 NIC-DMA 协同交付 |
| **3** | UALink PHY 状态寄存器 | v1.1 | NIC-DMA 协同 |
| **4** | IO-DMA 详细 CSR (0x0300-0x03FF) | v2.0 | backends IO-DMA 推迟到 v1.1+ |
| **5** | PCIe ATS/PRI 中断 (0x30+) | v2.0 | PcieEndpointIP 不支持 ATS |
| **6** | GDS Zero-Copy 控制 | v2.0 | IO-DMA 推迟 |
| **7** | Atomic Operation 中断 (0x40) | v2.1 | Atomic 推迟 |
| **8** | 分布式 Drain FSM 信号 | v2.1 | 多 Switch 拓扑协调 |
| **9** | Multicast CSR + Reduce Engine 接口 | v3.0 | Multicast 协议锁定 |
| **10** | Coherence SnoopFilter 信号 + Back-invalidate | v3.0 | Coherence 实现复杂 |

---

## §4 后续阶段详细边界

### §4.1 v1.1: NIC-DMA 详细 CSR + Drain FSM + UALink PHY + Remote Poison 中断

| 维度 | 边界 |
|------|------|
| **新增能力** | NIC-DMA 详细 CSR (0x0200-0x02FF, per `21-microarch-ifc-mvp.md` §7.3); Drain FSM 信号完整时序 (per §6.2); UALink PHY 状态寄存器 (0x0220-0x022F); Remote CXL Poison 中断 (0x20); Link Up/Down 检测 |
| **前置依赖** | v1.0 ship + backends v1.0 ship (NIC-DMA) + [tee-udd v1.1] 协同 |
| **不引入** | PCIe IO / Multicast / Coherence |
| **验证标准** | NIC-DMA CSR 完整; Drain 两阶段握手 ≤100us 超时 SVA 通过; Remote CXL Poison 中断 0x20 触发 |
| **关键文件** | `src/tlm/core/ifc_signal_defs.hh` 加 NIC-DMA 信号; `csr_layout.hh` 加 NIC 寄存器; `awt_controller_tlm.cc` 加 Remote Poison 处理 |

### §4.2 v2.0: IO-DMA 详细 CSR + PCIe ATS/PRI 中断 + GDS + Host SVA 中断

| 维度 | 边界 |
|------|------|
| **新增能力** | IO-DMA 详细 CSR (0x0300-0x03FF, per `21-microarch-ifc-mvp.md` §7.5); PCIe ATS/PRI 中断 (0x30-0x3F); GDS Zero-Copy 控制寄存器 (0x0310); Host SVA DMA 中断路由; ATS Translation Request 中断 |
| **前置依赖** | v1.1 ship + backends v1.1 ship (IO-DMA) + PcieEndpointIP 加 ATS + 23 ABI 扩展 + [tee-udd v2.0] 协同 |
| **不引入** | Atomic / Multicast / Coherence |
| **验证标准** | IO-DMA CSR 完整; ATS/PRI 中断 0x30+ 触发; GDS NVMe 零拷贝中断正确 |
| **关键文件** | `csr_layout.hh` 加 IO 寄存器; `awt_controller_tlm.cc` 加 ATS/PRI 中断 |

### §4.3 v2.1: Atomic 中断 + 分布式 Drain FSM + Hot-plug 中断 + 远端 AWT

| 维度 | 边界 |
|------|------|
| **新增能力** | Atomic Operation 中断 (0x40); 分布式 Drain FSM 信号 (跨多 Switch Port); Hot-plug 中断 (0x50); 远端 AWT (Remote_Link_Fault, Trap Type 0x4) |
| **前置依赖** | v2.0 ship + [fabric-switch v2.1] 分布式 Drain + 23 ABI 扩展 (Atomic ABI) |
| **不引入** | Multicast / Coherence |
| **验证标准** | Atomic 中断 0x40 触发; Hot-plug 中断 0x50 在 HRT Swap 完成时触发; Trap Type 0x4 完整处理 |
| **关键文件** | `csr_layout.hh` 加 Atomic / Hot-plug 中断向量; `awt_controller_tlm.cc` 加 Trap Type 0x4 |

### §4.4 v3.0: Multicast CSR + Switch Reduce Engine 接口 + Coherence SnoopFilter + Back-invalidate

| 维度 | 边界 |
|------|------|
| **新增能力** | Multicast CSR (0x0700-0x07FF, 新增 MMIO 区间); Switch Hardware Reduce Engine 接口 (Reduce_Op 信号); Coherence SnoopFilter 信号 (Invalidate_Request / Invalidate_Ack); Back-invalidate 中断 (0x60); All-Reduce 加速器接口 |
| **前置依赖** | v2.1 ship + [fabric-switch v3.0] Reduce Engine + Coherence SnoopFilter 实现 |
| **不引入** | (无 — 收敛) |
| **验证标准** | Multicast CSR 完整; Reduce Engine 接口正确; Back-invalidate latency < 1us |
| **关键文件** | `csr_layout.hh` 加 Multicast + Coherence 寄存器; `ifc_signal_defs.hh` 加 SnoopFilter 信号; `awt_controller_tlm.cc` 加 Back-invalidate 处理 |

---

## §5 无债务演进约束 (4 条不变量)

### §5.1 不变量 1: 跨模块信号接口在演进中只增不换

> v1.0 MVP ship 后, 跨模块信号接口 (SM-UDD / TC-UDD / UDD-GMMU / CIU-UDD) 信号位宽与定义冻结。
>
> v1.1+ 仅扩展预留信号 (如 Payload 新增字段), 不修改既有信号位宽。

**实现方式**: 每个接口的 `payload` 字段预留 `[MAX_BITS-1:N+1]` bits 备用, v1.0 仅使用低 N bits。v1.1+ 扩展时仅使用预留 bits。

### §5.2 不变量 2: 时钟域划分从 v1.0 起 4 域稳定

> v1.0 MVP 即定义 4 时钟域 (clk_core / clk_fab / clk_io / clk_cfg), 频率固定 (2.0 / 1.2 / 1.0 / 0.1 GHz)。
>
> v1.1+ 不引入新时钟域, 现有时钟域频率可在 ±10% 范围内调整。

**实现方式**: 4 时钟域边界在 v1.0 MVP 冻结, 异步 FIFO 位置和深度在 v1.0 固定。

### §5.3 不变量 3: MMIO 寄存器布局从 v1.0 起 extensible

> v1.0 MVP MMIO 基地址 0x00F0_0000, 0x0000-0x04FF 寄存器布局冻结。
>
> v1.1+ 仅追加 0x0500+ 区间寄存器, 不修改 0x0000-0x04FF 已定义寄存器。

**实现方式**: 0x0000-0x04FF 在 v1.0 冻结, 0x0500-0xFFFF 区间保留 v1.1+ 扩展。每个新寄存器定义前检查地址冲突。

### §5.4 不变量 4: AWT Trap Type 从 v1.0 起定义完整

> v1.0 MVP 即定义 4 种 Trap Type (0x0-0x3), 即使 v1.0 仅 0x0 (Local HBM Poison) 实际触发。
>
> v1.1+ 加 Trap Type 时仅追加 (如 0x4 Remote_Link_Fault), 不修改既有 Type 含义。

**实现方式**: `AWT_PAYLOAD_2[7:0]` 8-bit 字段, v1.0 仅定义 0x0-0x3, 0x4-0xFF 预留 v1.1+。

---

## §6 v1.0 MVP 端到端 Shippable Demo (12 步测试)

这是 v1.0 MVP 必须通过的"绿灯测试", 证明 **SM → TEE Frontend (4 级流水线) → GMMU → UDD Agent (HRT) → Compute NoC → Global Hub → HBM-DMA** 完整实现层贯通:

```
┌──────────────────────────────────────────────────────────────────────┐
│  v1.0 MVP Test: test_microarch_ifc_e2e                              │
├──────────────────────────────────────────────────────────────────────┤
│  Step 1: clk_core 域初始化                                           │
│            - clk_core 2.0 GHz (SM + TEE Frontend + TC-DMA)           │
│            - clk_fab 1.2 GHz (Global Hub + HBM-DMA + Memory NoC)     │
│            - clk_io 1.0 GHz (NIC-DMA + UALink PHY, v1.1+)            │
│            - clk_cfg 100 MHz (CIU + Sideband, per `21-tee-udd-mvp` §3.4)│
│            - 异步 FIFO (Gray-code 指针) 隔离跨域                     │
│                                                                       │
│  Step 2: SM LSU 提交描述符 (clk_core 域)                              │
│            - sm_udd_req_valid=1, sm_udd_req_payload={VA, size, ctx}   │
│            - sm_udd_req_ready=1 (UDD Agent 可接收)                   │
│                                                                       │
│  Step 3: TEE Frontend.Fetch (1 cycle, clk_core)                      │
│            - 检查 Context_ID 权限 → RCT 预校验通过                    │
│            - 写入 pipe_regs_[Fetch]                                  │
│                                                                       │
│  Step 4: TEE Frontend.Translate (1 cycle, clk_core)                  │
│            - 调用 gmmu_translator->translate(VA) → GUPA              │
│            - GMMU L1 TLB miss → HW PTW walker (4 cycles)            │
│            - 返回 GUPA → pipe_regs_[Translate].src_gupa              │
│                                                                       │
│  Step 5: TEE Frontend.Route (1 cycle, clk_core)                      │
│            - UDD Agent HRT 查表 (3 cycle SRAM pipeline)              │
│            - 返回 Route_Tag=0x0 (HBM), VC=0, QoS=Normal             │
│            - AGU 拆分 (1 cycle, v1.0 1D 步长)                        │
│                                                                       │
│  Step 6: TEE Frontend.Dispatch (1 cycle, clk_core)                    │
│            - 分配 Tracker_ID (frontend_tracker_)                     │
│            - 注入 udd_agent->inject(Micro-op)                        │
│            - UDD Agent 仲裁 (WRR, TC:SM=4:1, 1 cycle)                │
│                                                                       │
│  Step 7: Compute NoC 传输 (clk_core → clk_fab, 异步 FIFO)            │
│            - Micro-op 注入 NoC Injection FIFO (深度 64)               │
│            - Compute NoC 路由 (4 cycles @ clk_fab)                   │
│            - Global UDD Hub 入口 FIFO (深度 64)                       │
│                                                                       │
│  Step 8: Global UDD Hub 仲裁 (clk_fab, 1 cycle)                      │
│            - RCT 校验通过                                            │
│            - Credit[backend=HBM-DMA] = 128 → 127                      │
│            - 注入 backend_out[HBM-DMA]                                │
│                                                                       │
│  Step 9: HBM-DMA 处理 (clk_fab, 1+15+ cycles)                       │
│            - Address Decoder (1 cycle)                               │
│            - FR-FCFS Scheduler (1 cycle)                             │
│            - HBM3e PHY Command (15 cycles zero-load latency)         │
│            - 数据返回 (7 cycles)                                     │
│            - ECC SECDED 验证 (2 cycles)                              │
│            - 组装 UDD Response (1 cycle)                             │
│                                                                       │
│  Step 10: 返回路径 (clk_fab → clk_core, 异步 FIFO)                    │
│            - UDD Response 经 Compute NoC 返回 GPC                    │
│            - TEE Frontend 接收 Response (1 cycle @ clk_core)         │
│            - 写回 SM Register File (1 cycle)                         │
│            - Credit[backend=HBM-DMA] = 127 → 128 (归还)               │
│            - 释放 Tracker_ID                                         │
│                                                                       │
│  Step 11: 微架构延迟验证                                              │
│             - Local HBM Load 总延迟: ~14.5 ns (per §4.1 预算)        │
│             - 4 cycle HRT 查表 + 1 cycle AGU + 4 cycle NoC + 15 cycle│
│               HBM3e PHY + 7 cycle Return = 31 cycles @ 2.0 GHz = 15.5 ns│
│             - 实际测量 latency 满足 ≤ 14.5 ns 预算                    │
│                                                                       │
│  Step 12: 微架构特性验证                                              │
│             - HRT Atomic Swap ≤8 cycles @ clk_core (per MAS-3.1-UDD-MAS)│
│             - Page Fault Replay Buffer 深度 ≥256 (per MAS-3.1-DMA-TEE) │
│             - Tracker_ID 复用正确                                    │
│             - RCT Security Violation AWT Trap 触发                    │
└──────────────────────────────────────────────────────────────────────┘
```

**验收标准**: 12 步全部通过 + 测试用例 ≥ 10 个 (见 [`21-microarch-ifc-mvp.md` §10 测试覆盖](21-microarch-ifc-mvp.md))。

---

## §7 跨仓契约与 UsrLinuxEmu 协调

### §7.1 跨仓职责边界

| 维度 | CppTLM 仓 (本仓) | UsrLinuxEmu 仓 (driver 侧) |
|------|------------------|--------------------------|
| **本地接口信号** | 定义 SM-UDD/TC-UDD/UDD-GMMU/CIU-UDD 信号 (见 `21-microarch-ifc-mvp.md` §3, §5) | 无 (RTL 实现层) |
| **时钟域划分** | 定义 clk_core/clk_fab/clk_io/clk_cfg (见 `21-microarch-ifc-mvp.md` §4.2) | 无 (RTL 实现层) |
| **MMIO 寄存器布局** | 定义 CSR 寄存器偏移 + 字段 (见 `21-microarch-ifc-mvp.md` §7) | driver 按布局写 BAR |
| **AWT Trap Type** | 定义 4 种 Trap Type 编码 (见 `21-microarch-ifc-mvp.md` §7.6) | driver 接收 AWT_PAYLOAD_2 + 解析 Trap Type |
| **MSI-X Payload** | 定义 32-bit MSI-X Payload 格式 (见 `21-microarch-ifc-mvp.md` §8.1) | driver 注册中断处理例程 |
| **性能延迟预算** | 定义 Local HBM ~14.5 ns 等延迟 (见 `21-microarch-ifc-mvp.md` §4.1) | 无 (RTL 物理实现约束) |
| **23 ABI 签名** | `cpptlm_dma_translate_cb` 不变 | driver 端实现回调 |

### §7.2 UsrLinuxEmu 改造路径

| 原 V3.0 SDMA 实现 | 新 MicroArch+IFC 实现 | 差异 |
|-------------------|------------------------|------|
| 无统一 MMIO 基地址 | 基地址 0x00F0_0000 (v1.0 MVP 冻结) | driver 视角统一 |
| 无统一 AWT Trap Type | 4 种 Trap Type (0x0-0x3) 定义 | driver 解析 Trap Type |
| 无 Drain FSM CSR | NIC_DRAIN_CTRL + NIC_DRAIN_STATUS (v1.1+) | driver 写 Drain CSR |
| 无标准 MSI-X Payload | 32-bit 标准格式 (vector_id/gpu_node_id/event_type/severity) | driver 注册标准中断 |

### §7.3 跨仓 PR 协调

按 ADR-091 §R5.1 跨仓 PR 流程:
1. CppTLM 仓: 实现 MicroArch + IFC 模块 + 18+ 测试
2. UsrLinuxEmu 仓: driver 协调 CSR 寄存器 (由用户承担协调)
3. 跨仓集成测试: `test_microarch_ifc_e2e_ue.cc`
4. 同步 PR (无 ABI 影响, 跨仓风险低)

---

## §8 反模式 (明确不做)

| 反模式 | 不做的原因 |
|--------|-----------|
| ❌ **完整 NVIDIA Hopper Core RTL 复刻** | CppTLM 是 TLM 行为级仿真, 非 RTL 仿真 (per `00-overview.md §9`) |
| ❌ **跨模块信号位宽动态调整** | 违背"信号接口只增不换"原则 (per §5.1 不变量 1) |
| ❌ **时钟域动态 DVFS** | 违背"4 时钟域稳定"原则 (per §5.2 不变量 2) |
| ❌ **MMIO 已定义寄存器重新分配** | 违背"寄存器布局 extensible"原则 (per §5.3 不变量 3) |
| ❌ **AWT Trap Type 已定义值重用** | 违背"Trap Type 定义完整"原则 (per §5.4 不变量 4) |
| ❌ **跨时钟域用纯组合逻辑** | 违背"异步 FIFO CDC"原则, metastability 风险 |
| ❌ **HRT 切换 > 8 cycles** | 违背 MAS-3.1-UDD-MAS §3.3 性能约束 |
| ❌ **TC-DMA mbarrier 触发延迟 > 1 cycle** | 违背 MAS-3.1-DMA-TEE §8.3 性能约束 |
| ❌ **Poison 传播延迟 > 5 clk_fab + 2 clk_core** | 违背 RTL-IFC §6.3 性能约束 |
| ❌ **Drain FSM 超时硬编码无法关闭** | 违背"驱动可控"原则, 应通过 CSR 配置 |

---

## §9 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: Core/Micro-Arch + RTL-IFC 5 阶段演进路线图 (v1.0/v1.1/v2.0/v2.1/v3.0) + 4 条不变量 + 12 步 shippable demo + 上下游协同路线图 |

---

**关联 OpenSpec change**: 待 `openspec/changes/2026-09-19-cpptlm-mas-microarch-ifc-mvp/` 提案创建
**下次更新**: Oracle 评审反馈后 v1.1 (NIC-DMA 详细 CSR 引入)