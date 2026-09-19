# dGPU Fabric + Scale-Up Switch 演进路线图 (Interconnect & Switch Evolution Roadmap)

> **目的**: 定义 CppTLM dGPU SoC **MAS-3.1** 中 **Fabric 协议层** + **Scale-Up Switch** 的多阶段演进路径 (v1.0 MVP → v3.0 完整 Multicast + Coherence), 确立每个阶段的 **shippable 价值** + **前置依赖** + **不引入项** + **无债务演进约束**, 确保整个演进过程不产生"未来需清理"的债务。
>
> **状态**: Draft v1.0 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: 待 `openspec/changes/2026-09-19-cpptlm-mas-fabric-switch-mvp/` 提案对齐
> **关联文档**:
> - [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) — **Fabric+Switch v1.0 MVP 详细设计** (本路线图第一阶段 SSOT)
> - [`21-tee-udd-evolution-roadmap.md`](21-tee-udd-evolution-roadmap.md) — TEE-UDD 演进路线图 (上游协同)
> - [`21-dma-backends-evolution-roadmap.md`](21-dma-backends-evolution-roadmap.md) — Backends 演进路线图 (下游协同)
> - [`21-microarch-ifc-evolution-roadmap.md`](21-microarch-ifc-evolution-roadmap.md) — MicroArch+IFC 演进路线图 (实现层协同)
> - [`21-soc-topology-mvp.md`](21-soc-topology-mvp.md) — SoC 顶层物理布局规范 (V3.1-Rev2.0 拓扑修正: CXL Memory Pool 在外部 Switch 下, 不在 GPU Die)
> - [`20-gmmu-evolution-roadmap.md`](20-gmmu-evolution-roadmap.md) — GMMU 演进路线图 (翻译层协同)
> **关联 ADR**:
> - ADR-088 §D5 — 23 ABI 冻结 (本路线图 v1.0 不动 ABI)
> - ADR-SOC-10 (待起草) — TEE/UDD/UBC 三层解耦 + Switch 协议契约 (本路线图边界)
> - ADR-SOC-21 — **V3.1-Rev2.0 拓扑修正** (CXL 在外部 Switch 下 / HRT 无 CXL_DIRECT)

---

## §0 阅读引导

- 想理解 Fabric+Switch 整体战略 → 读 §1 (范围与目标) + §2 (阶段总览)
- 想理解 v1.0 MVP 的具体内容 → 读 §3
- 想理解后续阶段 (v1.1 / v2.0 / v2.1 / v3.0) 边界 → 读 §4
- 想理解"无债务演进"的不变量 → 读 §5
- 想理解 v1.0 的端到端 demo → 读 §6
- 想理解跨仓契约 → 读 §7
- 想查阅反面模式与不做项 → 读 §8

---

## §1 范围与目标

### §1.1 为什么需要 Fabric + Switch 演进路线图

CppTLM dGPU 当前缺失**统一的全栈互联契约层**:

**旧方案 (V3.0 直连 PCIe)**:
- GPU Die 内同时存在 PCIe + CXL Controller, 协议栈复杂
- Scale-Up 与 IO 协议混用, RTL 难以复用
- 无 Drain 协议时序定义, 故障隔离依赖 driver 手动
- 无 Poison 编码映射, 错误传播依赖各模块独立处理

**实际使用**:
- 单一 PCIe 通道负责 Host IO + Scale-Up (P2P via PCIe)
- CXL.mem 在 GPU 侧实现完整协议栈
- Drain 协议无标准, 每次热插拔需要 driver 协调多个模块

**隐患**:
- GPU Die 面积压力 (含完整 PCIe + CXL Controller, 3-5% 面积浪费)
- 验证复杂 (PCIe + CXL 联调, 兼容性测试昂贵)
- 互操作性差 (与单一 CXL 厂商绑定)
- Drain 协议不一致 (各 Backend 独立处理, 易死锁)

**Fabric+Switch 演进的总体目标**: 用 6 个阶段逐步把 Fabric+Switch 从"PCIe 直连"建设到"UALink + Switch PTE + Multicast + Coherence"的完整互联架构, **每个阶段 shippable + 不制造未来清理债务**。

### §1.2 v1.0 MVP 的核心价值 (用户明确目标)

> **"Fabric+Switch MVP 可让 GPU 通过 UALink 1.1 Mem 协议访问 Peer GPU Local HBM 与外部 Switch"**

具象化为 1 个端到端测试 (demo 详见 §6):

```
GPU 0 SM 发起 Load (Peer GPU 1 HBM)
  ↓
TEE-UDD 处理 → Route_Tag=1 (UALink Port 0)
  ↓
NIC-DMA (GPU 0) UALink Flit 封装 → Switch
  ↓
Switch PTE Base/Limit 决策: GUPA 落其他 GPU Local HBM 范围
  → P2P 旁路 → Crossbar 直路由至 GPU 1 UALink Port 1
  ↓
NIC-DMA (GPU 1) P2P Bypass → 直接写入 HBM-DMA Write Queue
  ↓
HBM-DMA (GPU 1) FR-FCFS → 数据返回 → NIC-DMA 封装 UALink Response Flit
  ↓
Switch Crossbar 转发 (Port 1 → Port 0) → GPU 0 NIC-DMA
  ↓
NIC-DMA (GPU 0) Tracker_ID 匹配 → 释放 ROB → UDD Response → TEE-UDD
  ↓
SM (GPU 0) 读到 Peer GPU 1 HBM 数据
```

### §1.3 关键决策 (per MAS-3.1-Fabric Rev2.0)

| 决策 | 内容 | 原因 |
|------|------|------|
| GPU 侧严格剥离 CXL.mem/CXL.cache | GPU 仅处理 UALink 1.1 Mem 子集 | Die 面积节省 3-5%, 验证复杂度降低一个数量级 |
| 所有跨节点访存经 Switch PTE | Switch 负责 UALink ↔ CXL.mem 转换 | CXL 协议黑盒化, GPU 不感知 |
| 两阶段 Drain 严格时序 | 100us 总超时, 4 VC 隔离 | 防止 Drain 期间死锁, 互操作性 |
| 4 层 Poison 编码映射 | CXL → UALink → UDD → AWT | RAS 端到端闭环 |

---

## §2 阶段总览 (时间线 + 关键约束)

```
v1.0 MVP         v1.1              v2.0             v2.1            v3.0
   ●───────────────●─────────────────●────────────────●───────────────●
   │  UALink 1.1    │  + PCIe Gen6    │  + CXL Type-3  │  + Atomic    │  + Multicast
   │  + Switch PTE  │  + ATS/PRI      │  + Hot-plug    │  + 分布式    │  + All-Reduce
   │  + 4 VC        │  + GDS          │  + CXL CTS    │  + Drain FSM  │  + Coherence
   │  + Drain FSM   │  + P2P via PCIe  │  + Sideband PCIe│               │  + SnoopFilter
```

| 阶段 | 时间估算 | 核心 shippable 价值 | 关键依赖 | 不引入 (仍推迟) |
|------|----------|---------------------|----------|------------------|
| **v1.0 MVP** | 2-3 周 | UALink 1.1 Mem 完整通路; Switch PTE (Base/Limit 路由 + CXL 黑盒); 两阶段 Drain FSM (100us 超时); 4 层 Poison 映射; P2P Bypass | backends v1.0 ship (NIC-DMA) + tee-udd v1.0 ship (HRT Route_Tag=1~E) + Scale-Up Switch 物理或 BFM | PCIe / CXL.io / ATS / PRI / GDS / CXL Type-3 / Atomic / Multicast |
| **v1.1** | 2-3 周 | + PCIe Gen6 协议 (L1 Link); + ATS Translation Request; + PRI Page Request; + GDS Zero-Copy 旁路; + Host SVA DMA 完整 zero-copy | v1.0 ship + backends v1.1 ship (IO-DMA) + PcieEndpointIP 加 ATS 能力 + 23 ABI 扩展 | CXL Type-3 / Hot-plug / Atomic / Multicast |
| **v2.0** | 3-4 周 | + CXL Type-3 Memory Backend (经 Switch PTE); + Hot-plug / Hot-unplug; + CXL 3.0 HDM Compliance; + Sideband PCIe (per Switch Spec §1.1) | v1.1 ship + backends v2.0 ship (CXL Backend) + 真实 CXL Memory Pool + 23 ABI 扩展 | Atomic / Multicast / Coherence |
| **v2.1** | 2 周 | + Atomic Operations (Atomic Add/Sub/CmpAndSwap 经 UALink); + 分布式 Drain FSM (跨多 Switch Port); + Switch CXL Poison 完整分类 | v2.0 ship + 多 Switch 拓扑协调 + 23 ABI 扩展 | Multicast / Coherence |
| **v3.0** | 4-6 周 | + UALink Multicast (NIC-DMA + Switch); + Switch Hardware Reduce Engine (FP8/FP16 加法树); + L2 Coherence (CXL Back-invalidate 经 SnoopFilter); + All-Reduce / All-Gather 硬件加速 | v2.1 ship + CXL/UALink Multicast 协议锁定 + Coherence SnoopFilter 实现 + [backends v3.0] Multicast 协同 | (无更后项 — 收敛) |

---

## §3 v1.0 MVP 范围 (详细)

### §3.1 v1.0 MVP 必含的 9 个最小能力

| # | 能力 | 必含理由 |
|---|------|---------|
| **1** | L1 Link (UALink 1.1 Mem) 完整通路 | 没有 UALink, Scale-Up 协议失效 |
| **2** | 256B UALink Flit 格式 + 7 种 MsgType | 没有 Flit 格式, 协议对端无法解析 |
| **3** | 4 VC 严格隔离 (Control/Read/Write/Response) | 没有 VC 隔离, Drain 命令与数据流竞争 |
| **4** | Switch PTE Base/Limit 地址映射 | 没有地址映射, Switch 无法路由至 CXL Port |
| **5** | Switch Crossbar (Flit-level Non-blocking) | 没有 Non-blocking, 拥塞反压影响其他 Port |
| **6** | 两阶段 Drain FSM (≤100us 超时) | 没有 Drain, CXL 内存池热插拔不可行 |
| **7** | 4 层 Poison 编码映射 (CXL → UALink → UDD → AWT) | 没有 Poison 映射, CXL 错误无法传递到 Warp |
| **8** | P2P Bypass 路径 (Peer GPU Local HBM 直路由) | 没有 P2P 旁路, P2P 延迟增加 15% |
| **9** | Switch Sideband MMIO (SMBus 访问 CSR) | 没有 Sideband, FM 无法配置 Switch |

### §3.2 v1.0 MVP 推迟到后续阶段的 9 项

| # | 推迟项 | 推迟到 | 不引入原因 |
|---|--------|--------|------------|
| **1** | L1 Link (PCIe / CXL.io) | v1.1 | backends IO-DMA 推迟到 v1.1 |
| **2** | ATS Translation Request | v1.1 | PcieEndpointIP 当前不支持 ATS |
| **3** | PRI Page Request | v1.1 | 同上 |
| **4** | GDS Zero-Copy Bypass | v1.1 | IO-DMA 推迟到 v1.1 |
| **5** | CXL Type-3 Memory Backend (真实 CXL 联调) | v2.0 | v1.0 用模拟 CXL Pool, 真实联调昂贵 |
| **6** | Hot-plug / Hot-unplug | v2.0 | 需要真实 CXL Memory 物理设备 |
| **7** | Atomic Operations | v2.1 | CXL.mem 原生支持但需联调 |
| **8** | UALink Multicast (All-Reduce) | v3.0 | CXL.mem 不支持, 需 Switch 拆分 |
| **9** | L2 Coherence (Back-invalidate 经 SnoopFilter) | v3.0 | 需要 CXL.cache + Coherence Filter, 复杂 |

---

## §4 后续阶段详细边界

### §4.1 v1.1: PCIe Gen6 + ATS/PRI + GDS

| 维度 | 边界 |
|------|------|
| **新增能力** | L1 Link (PCIe Gen6) 完整通路; PCIe Gen6 x16 (64 GT/s, PAM4); ATS Translation Request (Host SVA VA → GMMU); PRI Page Request (GMMU Page Fault → PCIe PRI TLP); GDS Zero-Copy Bypass (NVMe P2P Write → HBM-DMA); 中断聚合 (MSI-X 向量化) |
| **前置依赖** | v1.0 ship + backends v1.1 ship (IO-DMA) + PcieEndpointIP 加 ATS 能力 (per `dgpu-soc-pcie-slice.md §0.1`) + 23 ABI 扩展 (per ADR-088 §D5) + [tee-udd v2.0] 协同 |
| **不引入** | CXL Type-3 / Hot-plug / Atomic / Multicast |
| **验证标准** | PCIe Gen6 Compliance Test 通过; ATS/PRI 完整路径 latency < 100us; GDS NVMe → HBM 延迟 < PCIe DMA 80% |
| **关键文件** | `src/tlm/fabric/fabric_protocol_tlm.cc` 加 PCIe L1; `src/tlm/fabric/switch_pte_tlm.cc` 加 PCIe 路径 (v1.1 仍模拟 PCIe); **23 ABI 扩展**: `cpptlm_emulator_ats_translate_request` 新增 |

### §4.2 v2.0: CXL Type-3 Memory + Hot-plug + Sideband PCIe

| 维度 | 边界 |
|------|------|
| **新增能力** | CXL Type-3 Memory Backend (经 Switch PTE, GPU 侧仍仅 UALink); Hot-plug / Hot-unplug (FM 经 Sideband 通知 Switch + GPU HRT Atomic Swap); CXL 3.0 HDM Compliance Test 通过; Sideband PCIe (per Switch Spec §1.1, 替换 SMBus) |
| **前置依赖** | v1.1 ship + backends v2.0 ship (CXL Backend) + 真实 CXL Memory Pool (≥1 家供应商) + 23 ABI 扩展 (CXL Type-3 driver) + [tee-udd v2.1] 协同 |
| **不引入** | Atomic / Multicast / Coherence |
| **验证标准** | CXL 内存池动态挂载/卸载延迟 < 100ms; 与 ≥2 家 CXL Memory Expander 厂商互操作 (per MAS-3.1-ScaleUp-Switch §7.1); HRT Atomic Swap 期间 GPU 无功能中断 |
| **关键文件** | `src/tlm/fabric/switch_pte_tlm.cc` 加 CXL Type-3 PTE; `src/tlm/fabric/switch_sideband_tlm.cc` 替换 PCIe; **23 ABI 扩展**: CXL Type-3 driver 协调 |

### §4.3 v2.1: Atomic Operations + 分布式 Drain FSM

| 维度 | 边界 |
|------|------|
| **新增能力** | Atomic Operations (Atomic Add/Sub/CmpAndSwap 经 UALink); 分布式 Drain FSM (跨多 Switch Port 并行 Drain, 协调 Backends); Switch CXL Poison 完整分类 (per MAS-3.1-DMA-Backends §5 RAS 矩阵) |
| **前置依赖** | v2.0 ship + 多 Switch 拓扑协调 (per [backends v2.1] §4.3) + 23 ABI 扩展 (Atomic ABI) |
| **不引入** | Multicast / Coherence |
| **验证标准** | Atomic Add 完整 UALink 通路, latency < 200ns; 分布式 Drain 多 Port 协同 < 200us; CXL Poison 分类 (Remote_Poison, Remote_CXL_Fault, Remote_Link_Fault) 正确 |
| **关键文件** | `src/tlm/fabric/ualink_atomic.{hh,cc}` 新建; `switch_pte_tlm.cc` 加分布式 Drain; `fabric_protocol_tlm.cc` 加 Atomic Flit |

### §4.4 v3.0: UALink Multicast + Hardware Reduce + L2 Coherence

| 维度 | 边界 |
|------|------|
| **新增能力** | UALink Multicast (NIC-DMA 支持 Multicast-Write, 跨多 Peer GPU); Switch Hardware Reduce Engine (FP8/FP16/BF16 加法树, 经 UALink Reduce_Op 拦截); L2 Coherence (CXL Back-invalidate 经 SnoopFilter 精准 Invalidate GMMU L2); All-Reduce / All-Gather 硬件加速 |
| **前置依赖** | v2.1 ship + CXL/UALink Multicast 协议锁定 + Coherence SnoopFilter 实现 + [backends v3.0] Multicast 协同 |
| **不引入** | (无 — 收敛) |
| **验证标准** | All-Reduce 8 GPU 完成时间 < 10us (8x4096 element, FP16); Switch Hardware Reduce 正确性 (与软件 Reduce 一致性 < 1 ULP); Coherence Back-invalidate latency < 1us |
| **关键文件** | `src/tlm/fabric/ualink_multicast.{hh,cc}` 新建; `src/tlm/fabric/switch_reduce_engine_tlm.{hh,cc}` 新建; `src/tlm/gpu/coherence_snoop_filter.{hh,cc}` 新建; `gmmu_tlm.cc` 加 Back-invalidate 接收 |

---

## §5 无债务演进约束 (4 条不变量)

### §5.1 不变量 1: UDD Micro-op / UALink Flit 接口在演进中只增不换

> v1.0 MVP ship 后, 通过 Fabric 协议层看到的 Micro-op / Flit 格式:
> - **v1.0**: 160-bit Micro-op + 256B UALink Flit (per `21-fabric-switch-mvp.md` §4.1, §5.1)
> - **v2.0+**: 仅扩展 header 字段预留位 (header[31:24] reserved, per `21-tee-udd-evolution-roadmap.md` §5.1)
>
> **承诺**: v2.0+ 引入新特性时, 旧调用点不受破坏, **新增位仅在 header 字段预留位扩展**, 旧字段布局保留含义不变。

**实现方式**: v1.0 MVP 的 `header` 字段 32-bit, 高 8-bit (bits[31:24]) 预留演进扩展位。v1.0 仅使用低 24-bit。

### §5.2 不变量 2: Switch PTE 地址映射窗口从 v1.0 起 extensible

> v1.0 MVP 即定义 `MAX_ADDR_WINDOWS = 8` array, 即使 v1.0 仅 2-4 窗口启用。
>
> v2.0+ 加窗口时**仅追加数组项启用**, 不重构数据结构。

**实现方式**: `std::array<PteAddrWindow, MAX_ADDR_WINDOWS> addr_windows_{};`, v1.0 仅 2-4 项 enable=1, 其余 enable=0。v2.0+ 加窗口时仅修改 enable 位。

### §5.3 不变量 3: UALink MsgType 从 v1.0 起定义完整 (7+2 种)

> v1.0 MVP 即使仅 7 种 MsgType 实际使用 (MEM_RD_REQ, MEM_WR_REQ, MEM_RESP_D, MEM_RESP_ND, CTRL_QUIESCE, CTRL_Q_ACK, CTRL_RESUME), 也定义完整 (含预留位 0x8=Multicast, 0x9=Reduce)。
>
> v3.0+ 加 Multicast / Reduce 时**仅启用**预留 MsgType, 不重构枚举。

**实现方式**: `enum class UALinkMsgType : uint8_t { MEM_RD_REQ=0x1, ..., CTRL_RESUME=0x7, Multicast=0x8, Reduce=0x9 }`, v1.0 仅使用前 7 个值。

### §5.4 不变量 4: Drain FSM 从 v1.0 起按 GPU-Switch 协同双 FSM

> v1.0 MVP 即实现 GPU FSM (IDLE → BLOCK_NEW → WAIT_SWITCH → DRAIN_LOCAL) + Switch FSM (IDLE → BLOCK_NEW → DRAIN_CXL → SEND_ACK → QUIESCED)。
>
> v2.0+ 加分布式 Drain (跨多个 Switch Port) 时**仅扩展** FSM 状态机, 不破坏既有状态。

**实现方式**: v1.0 GPU FSM 与 Switch FSM 严格按 §6.3 / §9.2 状态实现, v2.0+ 加分布式状态仅追加新状态 (如 PORT_QUIESCING), 不修改原状态转换。

---

## §6 v1.0 MVP 端到端 Shippable Demo (10 步测试)

这是 v1.0 MVP 必须通过的"绿灯测试", 证明整个 **GPU 0 → Switch → GPU 1 HBM** 链路贯通:

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

**验收标准**: 10 步全部通过 + 测试用例 ≥ 12 个 (见 [`21-fabric-switch-mvp.md` §12 测试覆盖](21-fabric-switch-mvp.md))。

---

## §7 跨仓契约与 UsrLinuxEmu 协调

### §7.1 跨仓职责边界

| 维度 | CppTLM 仓 (本仓) | UsrLinuxEmu 仓 (driver 侧) |
|------|------------------|--------------------------|
| **Fabric 协议格式** | 定义 UDD Micro-op + UALink Flit 格式 (见 `21-fabric-switch-mvp.md` §4, §5) | driver 按格式解析 / 构造 |
| **Switch PTE 寄存器** | 定义 Sideband MMIO (见 `21-fabric-switch-mvp.md` §10) | driver 按布局写 Sideband |
| **Switch CXL 协议** | v1.0 模拟 CXL.mem (per `21-fabric-switch-mvp.md` §8.4) | driver 协调 Host BIOS (v2.0+ 真实 CXL) |
| **Drain FSM 触发** | GPU 侧接受 FM 写 NIC_DRAIN_CTRL (per `21-microarch-ifc-mvp.md` §4.2) | driver 写 NIC_DRAIN_CTRL.START_DRAIN |
| **UALink Compliance** | 模拟 UALink PHY + MAC | driver 协调 Switch 固件升级 (v2.0+) |
| **Poison 传播** | Fabric 标准化 Status Code | driver 接收 MSI-X + 解析 Status |
| **23 ABI 签名** | `cpptlm_dma_translate_cb` 不变 | driver 端实现回调 |

### §7.2 UsrLinuxEmu 改造路径

| 原 PCIe 直连实现 | 新 Fabric+Switch 实现 | 差异 |
|------------------|------------------------|------|
| 单一 PCIe BAR MMIO | driver 写 GPU BAR (HRT/RCT) + Switch Sideband (PTE Base/Limit) | driver 视角从"统一 PCIe"变为"GPU + Switch 双侧" |
| 无 Drain 协议 | driver 写 NIC_DRAIN_CTRL + 接收 Switch CTRL_Q_ACK | driver 需理解两阶段 Drain 时序 |
| 无 Poison 编码 | 设计完全统一 (per `21-fabric-switch-mvp.md` §7.1) | driver 解析 4 层 Poison |
| 无 P2P Bypass | 设计完全实现 (per `21-fabric-switch-mvp.md` §8.6) | driver 配置 HRT Route_Tag |

### §7.3 跨仓 PR 协调

按 ADR-091 §R5.1 跨仓 PR 流程:
1. CppTLM 仓: 实现 Fabric + Switch + 15+ 测试
2. UsrLinuxEmu 仓: driver 改造 PCIe 直连 → Fabric+Switch 风格 (由用户承担协调)
3. 跨仓集成测试: `test_fabric_switch_p2p_e2e_ue.cc`
4. 同步 PR (无 ABI 破坏, 跨仓风险低)

---

## §8 反模式 (明确不做)

| 反模式 | 不做的原因 |
|--------|-----------|
| ❌ **完整 NVIDIA Hopper Fabric RTL 复刻** | CppTLM 是 TLM 行为级仿真, 非 RTL 仿真 (per `00-overview.md §9`) |
| ❌ **GPU 侧实现 CXL.mem/CXL.cache 协议** | 违背"GPU 协议栈极简"原则 (per MAS-3.1-Fabric §1), Die 面积浪费 |
| ❌ **Drain FSM 单方主导** | 违背"GPU-Switch 协同"原则, 易死锁 |
| ❌ **Drain Timeout 硬编码不可调** | 违背"驱动可配置"原则, 应通过 CSR 配置 |
| ❌ **Poison 编码各 Backend 独立** | 违背"统一 RAS"原则, GPC AWT 无法统一 |
| ❌ **P2P Bypass 经 PTE 转换** | 违背"低延迟 P2P"原则, 增加 PTE 处理延迟 |
| ❌ **Multicast 拆分为多次 Unicast** | v1.0 仅 Unicast, Multicast 推迟 v3.0; 避免性能 regression |
| ❌ **真实 CXL Memory Pool 联调在 v1.0** | v1.0 模拟, v2.0+ 真实; 避免开发周期被硬件联调阻塞 |
| ❌ **Host Driver 直接访问 Switch 寄存器** | 违背 "PCIe-only 原则" (per UsrLinuxEmu AGENTS.md §CppTLM 通信架构) |
| ❌ **Drain 期间全局停顿** | 违背"非目标端口不受影响"原则 (per MAS-3.1-Fabric §4.2.4), 死锁风险 |

---

## §9 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: Fabric + Scale-Up Switch 5 阶段演进路线图 (v1.0/v1.1/v2.0/v2.1/v3.0) + 4 条不变量 + 10 步 shippable demo + 上下游协同路线图 |

---

**关联 OpenSpec change**: 待 `openspec/changes/2026-09-19-cpptlm-mas-fabric-switch-mvp/` 提案创建
**下次更新**: Oracle 评审反馈后 v1.1 (PCIe/CXL.io 引入)