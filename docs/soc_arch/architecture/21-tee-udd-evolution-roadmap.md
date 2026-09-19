# dGPU TEE + UDD 演进路线图 (Task Execution Engine & Unified Data Dispatcher Evolution Roadmap)

> **目的**: 定义 CppTLM dGPU SoC **MAS-3.1** 中 **TEE (Task Execution Engine)** 与 **UDD (Unified Data Dispatcher)** 的多阶段演进路径 (v1.0 MVP → v3.0 完整两阶段), 确立每个阶段的 **shippable 价值** + **前置依赖** + **不引入项** + **无债务演进约束**, 确保整个演进过程不产生"未来需清理"的债务。
>
> **状态**: Draft v1.0 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: 待 `openspec/changes/2026-09-19-cpptlm-mas-tee-udd-mvp/` 提案对齐
> **关联文档**:
> - [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) — **TEE-UDD v1.0 MVP 详细设计** (本路线图第一阶段 SSOT)
> - [`21-dma-backends-evolution-roadmap.md`](21-dma-backends-evolution-roadmap.md) — Backends 演进路线图 (下游协同)
> - [`21-fabric-switch-evolution-roadmap.md`](21-fabric-switch-evolution-roadmap.md) — Fabric+Switch 演进路线图 (协议层协同)
> - [`21-microarch-ifc-evolution-roadmap.md`](21-microarch-ifc-evolution-roadmap.md) — MicroArch+IFC 演进路线图 (实现层协同)
> - [`21-soc-topology-mvp.md`](21-soc-topology-mvp.md) — SoC 顶层物理布局规范 (V3.1-Rev2.0 拓扑修正: HRT Route_Tag 分配 / TC-DMA 归属 / CXL 不在 GPU Die)
> - [`20-gmmu-evolution-roadmap.md`](20-gmmu-evolution-roadmap.md) — GMMU 演进路线图 (翻译层协同)
> - [`00-overview.md`](00-overview.md) §3.1 L1 Host Interface + §3.7 L7 Memory
> **关联 ADR**:
> - ADR-088 §D5 — 23 ABI 冻结 (本路线图 v1.0 不动 ABI)
> - ADR-SOC-09 — v1.0 NVIDIA+AMD dual vendor 战略
> - ADR-SOC-10 (待起草) — TEE/UDD/UBC 三层解耦 (本路线图边界)
> - ADR-SOC-21 — **V3.1-Rev2.0 拓扑修正** (HRT Route_Tag 无 CXL_DIRECT)

---

## §0 阅读引导

- 想理解 TEE-UDD 整体战略 → 读 §1 (范围与目标) + §2 (阶段总览)
- 想理解 v1.0 MVP 的具体内容 → 读 §3
- 想理解后续阶段 (v1.1 / v2.0 / v2.1 / v3.0) 边界 → 读 §4
- 想理解"无债务演进"的不变量 → 读 §5
- 想理解 v1.0 的端到端 demo → 读 §6
- 想理解跨仓契约 → 读 §7
- 想查阅反面模式与不做项 → 读 §8

---

## §1 范围与目标

### §1.1 为什么需要 TEE / UDD 演进路线图

CppTLM dGPU 当前缺失**统一的"任务执行 + 数据路由"分层架构**:

**旧方案 (V3.0 紧耦合 DMA)**:
- `SdmaEngineTLM` 一个模块硬编码 HBM/NIC/IO 三种 Backend 路径
- 每加一种 Backend 必须修改 Frontend + Midend + Adapter
- 无统一 GUPA 地址空间, 各 Backend 私有地址编码
- 无 RCT 多租户隔离, MIG 场景无法支持
- 无 HRT 原子切换, 动态内存拓扑重配需要 GPU Reset

**实际使用**:
- Host CPU 经 PCIe BAR MMIO 配置 `sdma_engine_tlm` 的 descriptor ring
- GPU 内部 `SdmaEngineTLM` 通过抽象回调 `cpptlm_dma_translate_cb` (per ADR-088 §D3.8) 由 DGpuBoard 注入一个 stub
- 后端硬编码三条路径 (HBM / NIC-DMA / IO-DMA), 改一种协议要动整个 engine

**隐患**:
- 无 Backend 解耦 → 不可支撑 UALink 1.1 / CXL 3.0 / NVMe GDS 异构协议共存
- 无 RCT 隔离 → 不可生产 NVIDIA MIG / 云端多租户场景
- 无 HRT 动态切换 → 不可生产热插拔 / 故障隔离场景
- 无 Transit 旁路 → Scale-Up 流量绕远路, AI 训练 All-Reduce 性能 regression

**TEE-UDD 演进的总体目标**: 用 6 个阶段逐步把 TEE-UDD 从"HBM-only 单后端"建设到"完整两阶段翻译 + 多 Backend + 多租户 + Transit 旁路"的全功能架构,**每个阶段 shippable + 不制造未来清理债务**。

### §1.2 v1.0 MVP 的核心价值 (用户明确目标)

> **"TEE-UDD MVP 可让现有 SM/TC-DMA 通过统一 160-bit UDD Micro-op 接口访问 Local HBM"**

具象化为 1 个端到端测试 (demo 详见 §6):

```
SM 发起 Load 请求 (VA)
  ↓
TEE Frontend.Fetch 接收 + RCT 预校验
  ↓
TEE Frontend.Translate → GMMU.translate(VA) → GUPA
  ↓
TEE Frontend.Route → UDD Agent HRT 查表 → Route_Tag=0 (HBM)
  ↓
TEE Midend.Chunking → 128B Micro-op
  ↓
UDD Agent 仲裁 (WRR, TC-DMA:SM=4:1) → NoC Injection
  ↓
Global UDD Hub → RCT 校验 → Credit Acquire → Backend Dispatcher
  ↓
HBM-DMA 接收 Micro-op → HBM3e Command → 数据返回
  ↓
UDD Response → TEE → SM Register File
```

### §1.3 命名约定 (per MVP §1.3)

| 旧名 (V3.0) | 新名 (V3.1-Rev2.0) | 原因 |
|--------------|---------------------|------|
| `SdmaEngineTLM` | `TeeTLM` | V3.0 SDMA 是单一引擎, V3.1 拆为 TEE (任务执行) + UBC (后端桥接) |
| `NicDmaTLM` (旧) | `NicDmaUBC` (V3.1) | NIC-DMA 后端逻辑归入 UBC (见 backends 文档) |
| `IoDmaTLM` (旧) | `IoDmaUBC` (V3.1) | IO-DMA 后端逻辑归入 UBC (见 backends 文档) |
| `HbmDmaTLM` (旧) | `HbmDmaUBC` (V3.1) | HBM-DMA 后端逻辑归入 UBC (见 backends 文档) |
| `GUPA` (V3.1 新概念) | 不变 | 64-bit 全局统一物理地址 (UDD 输出) |
| `UDD Micro-op` (V3.1 新概念) | 不变 | 160-bit 标准请求格式 |

---

## §2 阶段总览 (时间线 + 关键约束)

```
v1.0 MVP         v1.1              v2.0             v2.1            v3.0
   ●───────────────●─────────────────●────────────────●───────────────●
   │  HBM-only      │  + UALink       │  + PCIe IO      │  + RCT       │  + Transit
   │  1 Context     │  + Multi-ctx    │  + ATS/PRI      │  + Tag Override│ + Stage 2
   │  HRT 双 Bank  │  + L2 HRT       │  + SVA zero-cp  │  + 远端隔离  │ + GUPA 拆分
   │  1D AGU        │  + 2D/3D AGU    │  + GDS 旁路     │  + Security Trap│
```

| 阶段 | 时间估算 | 核心 shippable 价值 | 关键依赖 | 不引入 (仍推迟) |
|------|----------|---------------------|----------|------------------|
| **v1.0 MVP** | 2-3 周 | SM/TC-DMA → TEE → UDD → HBM-DMA → HBM (单 Context, 1D AGU, HRT 双 Bank, WRR=4:1, Page Fault Replay Buffer) | GMMU v1.0 ship + GMMU TLB 命中率 > 80% | UALink / PCIe IO / Multi-ctx / ATS / PRI / Transit / Stage 2 |
| **v1.1** | 2 周 | + UALink Routing (HRT Route_Tag=1~E);+ 2D/3D AGU;+ Multi-Context 实际使用 (8 ctx 启用);+ HRT L2 (Per-Context, 4-way) | v1.0 ship + UALink 立项 + [backends NIC-DMA] MVP ship | PCIe IO / ATS / PRI / Transit / Stage 2 |
| **v2.0** | 3-4 周 | + PCIe IO Routing (HRT Route_Tag=F);+ ATS Translation Request 经 IO-DMA;+ PRI 触发机制;+ GDS 零拷贝旁路 | v1.1 ship + [backends IO-DMA] MVP ship + PcieEndpointIP 加 ATS 能力 | Transit / Stage 2 / RCT Tag Override |
| **v2.1** | 2 周 | + RCT Tag Override (per-tenant 物理链路硬隔离);+ Security Violation AWT Trap 完整处理 | v2.0 ship + 多租户 driver 协调 | Stage 2 |
| **v3.0** | 4-6 周 | + Transit 旁路 (Remote A → Local → Remote B);+ Stage 2 拆分 (GUPA → GUPA + Route_Tag 二元组);+ HRT 多级 (L1+L2) 透明切换 | v2.1 ship + CXL/UALink 任一立项 + Switch PTE 协议锁定 | (无更后项 — 收敛) |

---

## §3 v1.0 MVP 范围 (详细)

### §3.1 v1.0 MVP 必含的 8 个最小能力

| # | 能力 | 必含理由 |
|---|------|---------|
| **1** | TEE Frontend 4 级流水线 (Fetch/Translate/Route/Dispatch) | 没有流水线无法实现 1 描述符/cycle 吞吐 |
| **2** | GMMU 协同接口 (translate() 调用) | 没有翻译层, VA 无法映射到 GUPA |
| **3** | UDD Agent HRT 双 Bank + 原子切换 | 没有双 Bank, 动态拓扑重配需要 Reset |
| **4** | Global UDD Hub RCT 多租户隔离 (≥1 Context, extensible) | 不引入 = 未来 MIG 加 Context 时必重构, 违反"无债务"原则 |
| **5** | Credit-Based 流控 (GPC↔Hub 64 Credits, Hub↔Backend 128 Credits) | 没有流控, Backend 拥塞会拖死整个 NoC |
| **6** | TEE Page Fault Replay Buffer (深度 256) | 没有 Replay Buffer, Channel 挂起导致后续任务饿死 |
| **7** | TC-DMA 复用 TEE Frontend + Midend (per MAS-3.1-DMA-TEE §8) | 不复用 = TC-DMA 独立实现一份 AGU/DTE, RTL 浪费 |
| **8** | HRT Atomic Swap ≤8 cycles (per MAS-3.1-UDD-MAS §3.3) | 不约束延迟 = 动态重配导致 SM Stall 数 cycle, 性能不可接受 |

### §3.2 v1.0 MVP 推迟到后续阶段的 8 项

| # | 推迟项 | 推迟到 | 不引入原因 |
|---|--------|--------|------------|
| **1** | AGU 多维地址生成 (2D/3D 步长) | v1.1 | 1D MVP 先跑通; 2D/3D 是性能优化 |
| **2** | DTE 格式转换 (FP32↔FP16/BF16/FP8) | v1.1 | 透传模式够 v1.0 E2E demo |
| **3** | 请求合并 (Coalescing) | v1.1 | 细碎读写合并是性能优化 |
| **4** | UALink / PCIe IO 路由 | v1.1 / v2.0 | backends 文档分阶段交付 NIC-DMA / IO-DMA |
| **5** | Multi-Context 实际使用 | v1.1 | 仅 1 ctx 启用, 数据结构预留 v1.1+ 扩展 |
| **6** | ATS / PRI 触发 | v2.0 | PcieEndpointIP 当前不支持 ATS (per `dgpu-soc-pcie-slice.md §0.1`) |
| **7** | Transit 旁路 (Remote A → Local → Remote B) | v3.0 | UALink 未立项, 无 Transit 流量 |
| **8** | Stage 2 (GUPA → GUPA+Route_Tag 拆分) | v3.0 | 收敛阶段, 引入对 NoC / UDD 影响大 |

---

## §4 后续阶段详细边界

### §4.1 v1.1: UALink Routing + 多维 AGU + Multi-Context 启用

| 维度 | 边界 |
|------|------|
| **新增能力** | UALink Routing (HRT Route_Tag=1~E 启用); 2D/3D AGU (矩阵转置 / Tensor 切片); Multi-Context 实际使用 (8 ctx 启用, 支持 MIG 雏形); HRT L2 (Per-Context, 4-way associative, ~256 entries); 请求合并 (Coalescing); DTE 格式转换 (FP32↔FP16/BF16); Memset/Mask/Reduction |
| **前置依赖** | v1.0 ship + [backends NIC-DMA] MVP ship + Page Fault 路径稳定 + Context_ID 数据结构已 extensible (per §5.2 不变量 2) |
| **不引入** | PCIe IO / ATS / PRI / Transit / Stage 2 |
| **验证标准** | UALink Routing 端到端: Local GPU → Peer GPU HBM 完整通路; 2D AGU 正确生成 Tensor 切片; Context 切换 < 1us; L2 HRT hit rate > 70% |
| **关键文件** | `src/tlm/dma/tee_midend_2d3d.{hh,cc}` 新建; `udd_agent_tlm.cc` 加 L2 HRT + Context 扩展; HRT 路由表填充逻辑加 UALink 路径 |

### §4.2 v2.0: PCIe IO Routing + ATS/PRI + GDS 旁路

| 维度 | 边界 |
|------|------|
| **新增能力** | PCIe IO Routing (HRT Route_Tag=F 启用); ATS Translation Request 协议 (TEE Frontend 经 IO-DMA 拦截 Host SVA DMA VA, 转 GMMU); PRI 触发机制 (Page Fault → PCIe PRI Request TLP → 自动重试); Host Driver 注册页表根 (经 23 ABI 扩展); GDS 零拷贝旁路 (per MAS-3.1-DMA-TEE §6.3) |
| **前置依赖** | v1.1 ship + [backends IO-DMA] MVP ship + PcieEndpointIP 加 ATS 能力 (per `dgpu-soc-pcie-slice.md §0.1` 高级可选阶段启动) + 23 ABI 扩展流程 (per ADR-088 §D5) |
| **不引入** | Transit / Stage 2 / RCT Tag Override |
| **验证标准** | Host SVA DMA 完整 zero-copy path; Page Fault → PRI → 自动重试 latency < 100us; GDS NVMe → HBM 零拷贝延迟 < PCIe DMA 80% |
| **关键文件** | `src/tlm/dma/tee_ats_bridge.{hh,cc}` 新建; `global_udd_hub_tlm.cc` 加 ATS/PRI 状态机; `tee_tlm.cc` 加 IO Route_Tag 路径; **23 ABI 扩展**: `cpptlm_emulator_ats_translate_request` 新增 |

### §4.3 v2.1: RCT Tag Override + 远端 Security Trap

| 维度 | 边界 |
|------|------|
| **新增能力** | RCT Tag Override (per-tenant Route_Tag 强制覆写, 实现物理链路硬隔离, per MAS-3.1-UDD-MAS §4.2); 远端 Security Violation AWT Trap 完整处理 (per MAS-3.1-DMA-TEE §7.2); 多租户 driver 协调路径 |
| **前置依赖** | v2.0 ship + 多租户 driver (UsrLinuxEmu MIG 雏形) + 23 ABI 扩展 |
| **不引入** | Stage 2 |
| **验证标准** | 租户 A 越权访问租户 B 链路 → Security Violation AWT Trap; 合法跨租户访问正常完成; 物理链路硬隔离 (即使 GUPA 重叠, 不同租户走不同 Port) |
| **关键文件** | `src/tlm/dma/global_udd_hub_rct.{hh,cc}` 新建 (RCT 增强); `tee_tlm.cc` 加 Security Trap 处理; 跨仓 driver 协调变更 |

### §4.4 v3.0: Transit 旁路 + Stage 2 + HRT 多级透明切换

| 维度 | 边界 |
|------|------|
| **新增能力** | Transit 旁路 (Remote A → Local → Remote B, GUPA 前缀均为 Scale-Up 域, 旁路 GMMU 深度翻译 + 旁路 Midend 数据缓冲, per MAS-3.1-DMA-TEE §3.2); Stage 2 拆分 (GUPA → GUPA + Route_Tag 二元组, per MAS-3.1-UDD-MAS §6 终态); HRT 多级透明切换 (L1+L2, 路由变更不触发全量刷新) |
| **前置依赖** | v2.1 ship + CXL/UALink 任一立项 + Switch PTE 协议锁定 + [fabric-switch] v3.0 ship |
| **不引入** | (无 — 收敛) |
| **验证标准** | Transit 流量不消耗本地 HBM 带宽 (per MAS-3.1-DMA-TEE §3.2); Stage 2 GUPA 高位 [63:48] 正确路由到 HBM/UALink/PCIe; HRT 变更不触发 GMMU TLB 全量刷新 |
| **关键文件** | `src/tlm/dma/tee_transit_bypass.{hh,cc}` 新建; `udd_agent_tlm.cc` 加 Stage 2 拆分 + HRT L2; 跨仓 driver 协调变更 |

---

## §5 无债务演进约束 (4 条不变量)

### §5.1 不变量 1: GUPA 输出语义在演进中只增不换

> v1.0 MVP ship 后, 通过 UDD 内部看到的 `GUPA` 参数含义:
> - **v1.0**: `GUPA == 64-bit 全局统一物理地址` (Route_Tag 由 UDD HRT 查表附加)
> - **v3.0**: `GUPA == 64-bit + Route_Tag 二元组` (Stage 2 引入)
>
> **承诺**: v3.0 引入时, 旧调用点不受破坏, **新增** Route_Tag 作为独立字段, 旧 GUPA 字段保留含义不变。

**实现方式**: v1.0 MVP 的 `UddMicroOp.gupa` 字段固定 64-bit, 演进通过 `header` 字段新增 Route_Tag 位 (header 扩展位预留)。

### §5.2 不变量 2: Context_ID / RCT 数据结构从 v1.0 起 extensible

> v1.0 MVP 即定义 Context_ID 表为 `std::array<RctEntry, MAX_CONTEXTS>`, 即使 v1.0 仅 1 个 Context 启用。
>
> v1.1 增加 Context 时**只追加**数组项启用, 不重构数据结构。

**实现方式**: `MAX_CONTEXTS = 256` array, v1.0 仅 `rct_table_[0].enable = 1`, 其余 `enable = 0`。v1.1+ 增加活跃 ctx 时仅修改对应数组项的 enable 位。

### §5.3 不变量 3: HRT Entry 格式从 v1.0 起包含所有 16 个 Route_Tag 槽位

> v1.0 MVP 即使仅 Route_Tag=0 (HBM) 实际使用, HRT Entry 的 `route_tag` 字段也定义 4-bit (16 槽位预留)。
>
> v1.1+ 加 UALink 时**仅扩展** HRT 路由表填充逻辑, 不重构 Entry 格式。

**实现方式**: `struct HrtEntry { uint8_t route_tag : 4; /* 0:HBM, 1~E:UALink, F:PCIe */ };` v1.0 仅填 Route_Tag=0。

### §5.4 不变量 4: HRT Atomic Swap 状态机从 v1.0 起按 Ping-Pong 双 Bank

> v1.0 MVP 即定义 Active + Shadow 双 Bank, 即使 v1.0 不实现 Shadow 写入路径。
>
> v1.1+ 加 CIU Shadow 写入路径时**仅扩展** FSM 状态, 不重构 Bank 结构。

**实现方式**: `enum class HrtActiveBank { BankA, BankB };` + 双 Bank SRAM 数组。v1.0 仅实现 `IDLE → BANK_TOGGLE → RESUME` 三个状态, v1.1+ 加 `SHADOW_WRITE` 和 `WAIT_DRAIN` 状态。

---

## §6 v1.0 MVP 端到端 Shippable Demo (9 步测试)

这是 v1.0 MVP 必须通过的"绿灯测试", 证明整个 **SM → TEE → UDD → HBM-DMA → HBM** 链路贯通:

```
┌──────────────────────────────────────────────────────────────────────┐
│  v1.0 MVP Test: test_tee_udd_hbm_e2e                                │
├──────────────────────────────────────────────────────────────────────┤
│  Step 1: Driver 经 PCIe MMIO 写 UDD HRT                              │
│            - CIU_REG_HRT_SHADOW[0] = {Route_Tag=0x0(本地HBM),        │
│                                        VC=0, QoS=Normal}            │
│            - CIU_REG_HRT_CTRL.SWAP_BIT = 1 (触发原子切换)            │
│            - 验证: UDD Agent 在 ≤8 个 clk_core 周期内完成切换         │
│                                                                       │
│  Step 2: Driver 写 RCT (v1.0 仅 1 context, 默认通过)                │
│            - CIU_REG_RCT[ctx=0].enable = 1                           │
│                                                                       │
│  Step 3: SM 发起 Load 请求                                            │
│            - LSU 提交 Micro-op: VA=0x1000_0000, ctx=0, size=128B      │
│            - TEE Frontend.Fetch 接收, 检查 ctx 权限通过               │
│                                                                       │
│  Step 4: GMMU 翻译 (per gmmu-mvp §5.1)                                │
│            - TEE Frontend.Translate 调用 gmmu.translate(VA)            │
│            - GMMU L1 TLB miss → PTW walk → 返回 GUPA=0x8000_0000     │
│                                                                       │
│  Step 5: UDD HRT 查表                                                 │
│            - TEE Frontend.Route 提取 GUPA[63:48]=0x8000 → HRT 命中    │
│            - 解析 Route_Tag=0x0 (HBM), VC=0, QoS=Normal              │
│                                                                       │
│  Step 6: TEE Midend 处理                                              │
│            - AGU 拆分 128B 为 1 个 128B Micro-op                      │
│            - DTE 透传 (无格式转换)                                     │
│            - 注入 NoC (Route_Tag=0x0)                                 │
│                                                                       │
│  Step 7: UDD Agent 路由 + Global Hub 仲裁                            │
│            - UDD Agent 仲裁 (WRR, LSU 权重=1)                         │
│            - Global UDD Hub 按 Route_Tag=0x0 路由至 HBM-DMA           │
│                                                                       │
│  Step 8: HBM-DMA 返回数据                                             │
│            - HBM-DMA 通过 UDD Response Flit 返回 128B 数据            │
│            - TEE Frontend 接收, 写回 SM Register File                │
│                                                                       │
│  Step 9: 验证                                                         │
│            - SM 读到 expected value                                   │
│            - 第二次相同 VA 请求 → TEE 内部 shortcut (latency < 50%)  │
│            - VA 越权 (RCT ctx 禁用) → Security Violation AWT Trap    │
│            - HRT Atomic Swap 后, 路由立即生效 (≤8 cycles)            │
└──────────────────────────────────────────────────────────────────────┘
```

**验收标准**: 9 步全部通过 + 测试用例 ≥ 10 个 (见 [`21-tee-udd-mvp.md` §12 测试覆盖](21-tee-udd-mvp.md))。

---

## §7 跨仓契约与 UsrLinuxEmu 协调

### §7.1 跨仓职责边界

| 维度 | CppTLM 仓 (本仓) | UsrLinuxEmu 仓 (driver 侧) |
|------|------------------|--------------------------|
| **MMIO 寄存器布局** | 定义 CIU / UDD 寄存器 (见 `21-tee-udd-mvp.md` §4) | driver 按布局写 BAR |
| **HRT 表初始化** | 接受 driver 配置的 HRT Shadow 数据 | driver 按 GUPA 空间划分填充 HRT (per `21-fabric-switch-mvp.md` §2 GUPA 划分) |
| **RCT 表初始化** | 接受 driver 配置的 Base/Limit | driver 分配 ctx_id + 写 Base/Limit |
| **Context_ID 注入** | 接受 SM / TC-DMA 描述符携带的 ctx_id | driver 配置 (v1.0 默认 ctx=0) |
| **Page Fault 处理** | TEE 内部 Replay Buffer + 返回 -EFAULT | driver 接收 -EFAULT → 调页 + 写 REPLAY_KICK 触发恢复 |
| **HRT Atomic Swap** | 接受 CIU 触发 Swap | driver 按需触发 (CXL 内存池扩容 / 故障隔离) |
| **23 ABI 签名** | `cpptlm_dma_translate_cb` 不变 | driver 端实现回调, 内部委托 GMMU |

### §7.2 UsrLinuxEmu 改造路径

| 原 SDMA 实现 | 新 TEE-UDD 实现 | 差异 |
|--------------|------------------|------|
| `sdma_engine` BAR MMIO descriptor ring | driver 写 CIU BAR MMIO 配置 HRT + RCT + 触发 Swap | driver 视角从"描述符 ring"变为"HRT/RCT 配置" |
| 无地址空间划分 | 按 `21-fabric-switch-mvp.md` §2 GUPA 划分填 HRT | driver 维护 GUPA 空间映射 |
| 无多租户 | driver 写 RCT 分配 ctx_id | MIG 雏形 |
| 单 outstanding | driver 配置 Tracker 深度 (v1.0 默认 256) | 多 outstanding 支持 |

### §7.3 跨仓 PR 协调

按 ADR-091 §R5.1 跨仓 PR 流程:
1. CppTLM 仓: 实现 TEE + UDD + CIU 模块 + 15+ 测试 (本文档 + `21-tee-udd-mvp.md` + tasks.md)
2. UsrLinuxEmu 仓: driver 改造 SDMA → TEE-UDD 风格 (由用户承担协调)
3. 跨仓集成测试: `test_tee_udd_hbm_e2e_ue.cc` (per §6 demo)
4. 同步 PR (无 ABI 破坏, 跨仓风险低)

---

## §8 反模式 (明确不做)

| 反模式 | 不做的原因 |
|--------|-----------|
| ❌ **完整 NVIDIA Hopper TEE RTL 复刻** | CppTLM 是 TLM 行为级仿真, 非 RTL 仿真 (per `00-overview.md §9`) |
| ❌ **TEE 与 UBC 紧耦合** | 本路线图核心解耦, 违反 = 失去 RTL 复用 |
| ❌ **每个 Backend 独立 HRT 表** | 违背"统一 HRT"原则, 增加面积 |
| ❌ **HRT 切换 > 8 cycles** | 性能 regression, 动态重配场景不可接受 |
| ❌ **Page Fault 同步阻塞 Channel** | 违背"非阻塞 Page Fault"原则 (per MAS-3.1-DMA-TEE §7.1) |
| ❌ **RCT 多租户从 v1.1 起定义数据结构** | 违背无债务演进约束 (per §5.2 不变量 2) |
| ❌ **HRT Entry 不预留 16 Route_Tag 槽位** | 违背无债务演进约束 (per §5.3 不变量 3) |
| ❌ **单 Bank HRT (无 Shadow)** | 违背无债务演进约束 (per §5.4 不变量 4) |
| ❌ **TC-DMA 独立实现一份 AGU/DTE** | 违背 RTL 复用原则, 增加面积 |
| ❌ **Transit 流量绕远路 (经本地 HBM)** | 违背 Transit 旁路原则, AI 训练性能 regression |

---

## §9 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: TEE-UDD 5 阶段演进路线图 (v1.0/v1.1/v2.0/v2.1/v3.0) + 4 条不变量 + 9 步 shippable demo + SDMA→TeeTLM 重命名 + 上下游协同路线图 |

---

**关联 OpenSpec change**: 待 `openspec/changes/2026-09-19-cpptlm-mas-tee-udd-mvp/` 提案创建
**下次更新**: Oracle 评审反馈后 v1.1