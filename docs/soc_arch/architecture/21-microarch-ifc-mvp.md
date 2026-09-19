# Core/Micro-Arch + RTL-IFC v1.0 MVP 详细设计 (Core Microarchitecture & RTL Interface — Minimum Viable Product)

> **目的**: 定义 CppTLM dGPU SoC **MAS-3.1** 中 **Core 微架构** (GPC/SM/TC-DMA/UDD Agent) 与 **RTL-IFC** (模块接口 + 寄存器映射) 的 v1.0 MVP——作为 dGPU 数据搬运的**实现层契约**，将上游 [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) (TEE-UDD) 和 [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) (Backends) 的协议层语义落地到 **时钟域 / 流水线 / 信号定义 / CSR** 的硬件实现层。v1.0 MVP 仅交付 **Local HBM 完整通路** + **基础 RTL 接口**；UALink/PCIe/Transit 推迟到 v1.1+。
>
> **状态**: Draft v1.0 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: 待 `openspec/changes/2026-09-19-cpptlm-mas-microarch-ifc-mvp/` 提案对齐
> **关联文档**:
> - [`21-microarch-ifc-evolution-roadmap.md`](21-microarch-ifc-evolution-roadmap.md) — MicroArch+IFC 5 阶段演进路线图 (v1.0/v1.1/v2.0/v2.1/v3.0)
> - [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) — TEE + UDD MVP (协议层上游)
> - [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) — Backends MVP (协议层下游)
> - [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) — Fabric + Switch MVP (协议层对端)
> - [`21-soc-topology-mvp.md`](21-soc-topology-mvp.md) — SoC 顶层物理布局规范 (V3.1-Rev2.0 拓扑修正: TC-DMA 归属 GPC / IO-DMA 端口 / CXL 不在 GPU Die)
> - [`16-pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT
> **关联 ADR**:
> - ADR-088 §D5 — 23 ABI 冻结 (MicroArch+IFC v1.0 不动 ABI)
> - ADR-SOC-10 (待起草) — TEE/UDD/UBC 三层解耦 + 接口契约 (本模块边界)
> - ADR-SOC-21 — **V3.1-Rev2.0 拓扑修正** (TC-DMA 归属 / IO-DMA 端口 / CXL 不在 GPU Die)

---

## §1 概述

### §1.1 MicroArch + IFC 层在 dGPU SoC 中的位置

MicroArch + IFC 层是 dGPU SoC **实现层契约**，定义 GPC 内部微架构、跨模块信号接口与 CSR 寄存器映射：

```
┌─────────────────────────────────────────────────────────────────┐
│                       GPU 内部 (dGPU Die)                        │
│                                                                 │
│  ┌─────────────────────────────────────────────────────┐        │
│  │  GPC × 8 (核心计算 + Local UDD Agent)               │        │
│  │                                                     │        │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐   │        │
│  │  │ SM ×16    │  │ TC-DMA ×2  │  │ UDD Agent  │   │        │
│  │  │ (LSU+RF)  │  │ (TMA路径)  │  │ (HRT查表) │   │        │
│  │  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘   │        │
│  │        │                │                │          │        │
│  │        └────────────────┴────────────────┘          │        │
│  │                         │                            │        │
│  │                         ▼                            │        │
│  │              [TEE Frontend] (tee-udd 文档)            │        │
│  └─────────────────────────┬───────────────────────────┘        │
│                            │                                     │
│                            ▼                                     │
│  ╔═══════════════════════════════════════════════════════════╗  │
│  ║   ★ MicroArch + IFC 层 (本模块: 实现层契约)              ║  │
│  ║                                                           ║  │
│  ║  - 时钟域与复位 (3 个独立 clk_core/clk_fab/clk_io)        ║  │
│  ║  - 流水线设计 (4 级 Frontend + AGU + DTE)                  ║  │
│  ║  - 核心数据通路接口 (SM/TC-DMA/UDD-Backends 信号定义)    ║  │
│  ║  - 控制面接口 (CIU-UDD HRT Shadow + ScaleUp Drain)        ║  │
│  ║  - 寄存器映射 (CSR MMIO)                                  ║  │
│  ║  - 中断/异常接口 (MSI-X)                                  ║  │
│  ║  - 微架构性能延迟预算                                     ║  │
│  ╚═══════════════════════════════════════════════════════════╝  │
│                            │                                     │
└────────────────────────────┼─────────────────────────────────────┘
                             │
                             ▼ (实现细节下沉到 RTL)
```

### §1.2 为什么需要 MicroArch + IFC 单独文档 (vs. 协议层独立)

| 维度 | 协议层 (TEE-UDD / Backends / Fabric-Switch) | 实现层 (MicroArch + IFC) |
|------|----------------------------------------------|---------------------------|
| **关注点** | 协议语义, 状态机, 数据格式 | 时钟域, 流水线, 信号定义, CSR |
| **变更频率** | 协议升级 (UALink 1.1 → 1.2) | 性能优化 (流水线级数, 仲裁策略) |
| **验证方法** | UVM (Functional Verification) | Formal (SVA) + UVM + Emulation |
| **RTL 工程师视角** | "我要实现什么" | "我怎么实现" |
| **运维视角** | 协议互通测试 | 时序收敛, 物理布局 |

**核心设计原则** (per MAS-3.1-Core + MAS-3.1-RTL-IFC Rev2.0):
> **MicroArch 定义 GPC 内部 SM/TC-DMA/UDD Agent 微架构 + 性能延迟预算**
> **RTL-IFC 定义跨模块信号接口 + CSR 寄存器映射**
> **两者必须协同冻结, RTL 团队才能开始 Implementation**

### §1.3 与其他文档的边界

| 子系统 | 文档 | 与本模块边界 |
|--------|------|---------------|
| **TEE + UDD** | [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) | 上游: 本文档定义 TEE Frontend 流水线 + UDD Agent 微架构 + HRT 原子切换时序 |
| **Backends** | [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) | 下游: 本文档定义 HBM-DMA/NIC-DMA 微架构 + 跨时钟域 CDC |
| **Fabric + Switch** | [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) | 协议对端: 本文档定义 UALink Drain 时序 + Poison 传播延迟约束 |
| **GMMU** | [`20-gmmu-mvp.md`](20-gmmu-mvp.md) | 翻译层: 本文档定义 UDD Agent ↔ GMMU 接口 |

---

## §2 MicroArch + IFC v1.0 MVP 端到端 Shippable Demo

v1.0 MVP 必须通过的"绿灯测试"，证明 **SM → TEE Frontend (4 级流水线) → GMMU → UDD Agent (HRT) → Compute NoC → Global Hub → HBM-DMA** 完整实现层贯通：

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
│             - Local HBM Load 总延迟: ~14.5 ns (per §6.1 预算)        │
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

**验收标准**: 12 步全部通过 + 测试用例 ≥ 10 个 (见 §10 测试覆盖)。

---

## §3 GPC 微架构设计 (per MAS-3.1-Core §1)

### §3.1 GPC 内部组成

```
GPC (Graphics Processing Cluster):
┌──────────────────────────────────────────────────────────┐
│                                                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐    │
│  │ SM × 16    │  │ TC-DMA × 2  │  │ UDD Agent   │    │
│  │ (LSU+RF)   │  │ (TMA路径)   │  │ (HRT查表)   │    │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘    │
│         │                │                │              │
│         └────────────────┴────────────────┘              │
│                          │                               │
│                          ▼                               │
│              [TEE Frontend] (4 级流水线)                │
│                          │                               │
│                          ▼                               │
│              [WRR Arbiter (TC:SM=4:1)]                  │
│                          │                               │
│                          ▼                               │
│              [Local HRT Lookup]                          │
│                          │                               │
│                          ▼                               │
│              [Compute NoC Injection FIFO]                │
│                                                          │
│  ┌──────────────────────────────────────────────┐      │
│  │  L1 Cache (per GPC, 64 KB) + L2 Slice        │      │
│  └──────────────────────────────────────────────┘      │
└──────────────────────────────────────────────────────────┘
```

### §3.2 SM (Streaming Multiprocessor) 访存流水线 (per MAS-3.1-Core §1.1)

```cpp
// include/tlm/core/sm_lsu_tlm.hh
namespace cpptlm::core {

class SmLsuTLM : public sc_module {
public:
    SC_HAS_PROCESS(SmLsuTLM);

    // 上游: Warp Scheduler 提交 LSU 请求
    sc_port<sm_lsu_req_if> warp_in;

    // 下游: UDD Agent (per §3.1)
    sc_port<sm_udd_req_if> udd_out;

    SmLsuTLM(sc_module_name name);
    ~SmLsuTLM() override = default;

    // LSU 处理
    void lsu_tick();

private:
    // 4 个 LSU 通道 (per MAS-3.1-Core §1.1)
    static constexpr size_t LSU_LANES = 4;

    // Coalescer (地址合并器)
    struct CoalescerEntry {
        uint64_t va;
        uint32_t size;
        uint8_t  warp_id;
        uint8_t  valid;
    };
    std::array<CoalescerEntry, LSU_LANES> coalescer_{};

    // L1 TLB (64 entries, per GPC)
    static constexpr size_t L1_TLB_ENTRIES = 64;
    std::array<TlbEntry, L1_TLB_ENTRIES> l1_tlb_{};

    // 性能统计
    uint64_t pmu_lsu_throughput_ = 0;       // LSU 提交率
    uint64_t pmu_coalescer_hits_ = 0;       // Coalescer 合并命中
};

}  // namespace cpptlm::core
```

### §3.3 TC-DMA (TMA Engine) 微架构 (per MAS-3.1-Core §1.2)

```cpp
// include/tlm/core/tc_dma_tlm.hh
namespace cpptlm::core {

class TcDmaTLM : public sc_module {
public:
    SC_HAS_PROCESS(TcDmaTLM);

    // 上游: SM Warp 提交 TMA 描述符
    sc_port<tma_descriptor_if> warp_in;

    // 下游: UDD Agent (per §3.1)
    sc_port<tc_udd_req_if> udd_out;

    // 直达: SMEM (绕过 L1/L2 Cache)
    sc_port<smem_write_if> smem_direct_out;

    TcDmaTLM(sc_module_name name);
    ~TcDmaTLM() override = default;

    // TC-DMA 处理
    void tc_dma_tick();

private:
    // Descriptor Cache (16 entries)
    static constexpr size_t DESC_CACHE_ENTRIES = 16;
    std::array<TmaDescriptor, DESC_CACHE_ENTRIES> desc_cache_{};

    // mbarrier Controller (64 entries, per MAS-3.1-DMA-TEE §8.3)
    static constexpr size_t MBARRIER_ENTRIES = 64;
    struct MbarrierEntry {
        uint32_t count;
        uint8_t  valid;
        uint64_t smem_addr;  // mbarrier object 地址
    };
    std::array<MbarrierEntry, MBARRIER_ENTRIES> mbarrier_table_{};

    // Swizzle 引擎 (Bank Conflict-Free)
    uint32_t swizzle_pattern_[16] = {};

    // 性能统计
    uint64_t pmu_tma_throughput_ = 0;
    uint64_t pmu_smem_writes_ = 0;
    uint64_t pmu_mbarrier_triggers_ = 0;
};

}  // namespace cpptlm::core
```

### §3.4 GPC 内 UDD Agent 微架构 (per MAS-3.1-Core §1.3)

```cpp
// include/tlm/core/gpc_udd_agent_tlm.hh
namespace cpptlm::core {

class GpcUddAgentTLM : public sc_module {
public:
    SC_HAS_PROCESS(GpcUddAgentTLM);

    // 上游: SM LSU + TC-DMA
    sc_port<sm_udd_req_if> sm_in;
    sc_port<tc_udd_req_if> tc_in;

    // 下游: Compute NoC 注入
    sc_port<noc_inject_if> noc_out;

    // HRT Shadow Bank (per tee-udd-mvp §7.1)
    // 配置由 CIU 经 APB 写入 (per `21-tee-udd-mvp.md` §3.4)

    GpcUddAgentTLM(sc_module_name name);
    ~GpcUddAgentTLM() override = default;

    // 主 tick (clk_core)
    void tick();

    // HRT 查表 (per tee-udd-mvp §7.2)
    HrtEntry hrt_lookup(uint64_t gupa);

    // WRR 仲裁 (per tee-udd-mvp §7.4)
    bool arbiter_wr_r(SmRequest& sm_req, TcRequest& tc_req,
                      bool& tc_won, uint64_t& selected_gupa);

    // HRT Atomic Swap (per tee-udd-mvp §7.3)
    void hrt_atomic_swap();

private:
    // HRT 双 Bank
    static constexpr size_t HRT_ENTRIES = 4096;
    std::array<HrtEntry, HRT_ENTRIES> hrt_bank_a_{};
    std::array<HrtEntry, HRT_ENTRIES> hrt_bank_b_{};
    HrtActiveBank active_bank_ = HrtActiveBank::BankA;

    // WRR 仲裁状态
    uint32_t wrr_sm_credit_ = 1;
    uint32_t wrr_tc_credit_ = 4;

    // Injection FIFO (Compute NoC 入口)
    static constexpr size_t INJECT_FIFO_DEPTH = 64;
    std::array<UddMicroOp, INJECT_FIFO_DEPTH> inject_fifo_{};
    size_t inject_head_ = 0, inject_tail_ = 0, inject_count_ = 0;

    // Local Tracker (256 entries, per MAS-3.1-UDD-MAS §6.1)
    static constexpr size_t TRACKER_ENTRIES = 256;
    std::array<TrackerEntry, TRACKER_ENTRIES> tracker_pool_{};

    // HRT 切换延迟统计
    uint64_t hrt_swap_pending_cycles_ = 0;
    bool     hrt_swap_pending_ = false;
};

}  // namespace cpptlm::core
```

---

## §4 微架构性能与延迟预算 (per MAS-3.1-Core §5)

### §4.1 核心访存路径延迟预算

| 访存路径 | 微架构节点延迟拆解 | 总延迟预算 (ns) | v1.0 MVP 验证标准 |
|----------|---------------------|-----------------|---------------------|
| **Local HBM Load** | SM(2) + UDD(1) + NoC(4) + HBM-DMA(15) + 返回(7) | ~14.5 ns | ✅ 基线延迟, L2 Miss 后 |
| **Local HBM Store** | SM(2) + UDD(1) + NoC(4) + HBM-DMA(15) | ~11.0 ns | ✅ 写延迟 (无返回路径) |
| **TMA Bulk 写入 SMEM** | TC-DMA(1) + UDD+NoC+HBM(22) + **SMEM 直达(1)** + mbarrier(1) | ~12.5 ns | ✅ Bypass L2, 写入极快 |
| **HRT 查表 (独立)** | Index(1) + SRAM Read(1) + Decode(1) | ~1.5 ns @ 2.0 GHz | ✅ 3 cycle pipeline |
| **GMMU L1 TLB Hit** | Lookup(5 cycles) + 返回(1) | ~3.0 ns | ✅ L1 TLB hit latency |
| **GMMU L1 TLB Miss + PTW** | Lookup(5) + PTW 4-level(40) + Fill(5) | ~25.0 ns | ✅ Worst case (per `20-gmmu-mvp.md` §6.3) |
| **HRT Atomic Swap** | Fence(1) + Drain(3) + Toggle(1) + Resume(3) | ~4.0 ns | ✅ ≤8 cycles @ clk_core |

### §4.2 时钟域频率 (per MAS-3.1-Core §5)

```
v1.0 MVP 时钟域:
- clk_core (2.0 GHz): SM + TC-DMA + TEE Frontend + UDD Agent + CIU APB Master
- clk_fab (1.2 GHz): Global UDD Hub + HBM-DMA + Memory NoC
- clk_io (1.0 GHz): NIC-DMA + UALink PHY (v1.1+); IO-DMA + PCIe PHY (v1.1+)
- clk_cfg (100 MHz): CIU MMIO + Sideband (per `21-tee-udd-mvp.md` §3.4)

跨时钟域:
- clk_core → clk_fab: 异步 FIFO (Gray-code 指针, 深度 64)
- clk_fab → clk_io: 异步 FIFO (UBC → Protocol Engine, per `21-dma-backends-mvp.md` §3.1)
- clk_core → clk_cfg: 异步 FIFO (UDD Agent HRT Shadow 写入)
```

### §4.3 AWT (Asynchronous Warp Trap) RAS 微架构 (per MAS-3.1-Core §4)

```
v1.0 MVP AWT 处理:
1. 传播路径: Backend (HBM/NIC) → NoC Response Flit → GPC UDD Agent → SM LSU
2. LSU 拦截: 在 Memory Writeback Stage 检查 Poison_Bit
   if Poison_Bit == 1:
     rf_we = 0  (阻断寄存器写回)
     发送 Poison 事件 + Warp ID 给 AWT Controller
3. AWT Controller:
     挂起 Warp (status = TRAPPED)
     保存 PC + GPR 摘要到 AWT FIFO
     if AWT FIFO 满:
       Stall 整个 SM 的发射

Trap Type 区分 (per `21-microarch-ifc-mvp.md` §6.5):
- 0x0 (Local HBM Poison): Driver 隔离物理页, 迁移数据, 重启 Kernel
- 0x1 (Remote CXL Poison): Driver 通知 FM 隔离远端 CXL 节点, 触发 Checkpoint
- 0x2 (Security Violation): 越权访问 (per `21-tee-udd-mvp.md` §6.3)
- 0x3 (PCIe IO Abort): IO 错误 (v1.1+, per `21-fabric-switch-mvp.md` §13.3)
```

---

## §5 核心数据通路接口 (per MAS-3.1-RTL-IFC §2)

### §5.1 SM ↔ UDD Agent 接口 (GPC 内部, per RTL-IFC §2.1)

| 信号名 | 位宽 | 方向 | 描述 |
|--------|------|------|------|
| `sm_udd_req_valid` | 1 | → | 请求有效 |
| `sm_udd_req_ready` | 1 | ← | UDD Agent 接收就绪 |
| `sm_udd_req_payload` | 128 | → | Micro-op Payload (含 opcode, warp_id, sm_id, VA/GUPA, data) |

**v1.0 MVP 约束**:
- `sm_udd_req_payload` 128 bits 包含 Opcode, Warp_ID, SM_ID, VA(64)
- 不携带 GUPA (由 TEE Frontend.Translate 阶段填入)

### §5.2 TC-DMA ↔ UDD Agent 接口 (GPC 内部, TMA 通路, per RTL-IFC §2.2)

| 信号名 | 位宽 | 方向 | 描述 |
|--------|------|------|------|
| `tc_udd_req_valid` | 1 | → | TMA Burst 请求有效 |
| `tc_udd_req_ready` | 1 | ← | UDD Agent 接收就绪 |
| `tc_udd_req_payload` | 160 | → | TMA 描述符展开后的 Micro-op (含 GUPA, Burst_Length, SMEM_Offset) |
| `udd_tc_resp_valid` | 1 | → | 数据返回有效 (直接触发 SMEM 写入) |
| `udd_tc_resp_data` | 256 | → | 返回的 256B 缓存行数据 |
| `udd_tc_resp_status` | 2 | → | 0:OK, 1:Poison, 2:Abort |
| `udd_tc_mbarrier_trig` | 1 | → | **硬件触发**: 数据写完后自动拉高, 触发 SMEM 侧 mbarrier.arrive |

**v1.0 MVP 时序约束** (per MAS-3.1-RTL-IFC §6.1):
1. TC-DMA 收到 `udd_tc_resp_valid` 且 `status == OK`
2. TC-DMA 必须在**同一个 clk_core 周期内**将 256B 数据写入 SMEM Bank
3. 若 TMA 描述符配置自动同步, TC-DMA 必须在 SMEM 写入完成的**下一个 clk_core 上升沿**拉高 `udd_tc_mbarrier_trig`
4. SMEM 写入与 mbarrier 触发之间**不允许**插入任何 Stall 周期

### §5.3 UDD Agent ↔ GMMU 接口 (Translation, per RTL-IFC §2.3)

| 信号名 | 位宽 | 方向 | 描述 |
|--------|------|------|------|
| `udd_gmu_req_valid` | 1 | → | VA 翻译请求有效 |
| `udd_gmu_req_va` | 64 | → | 待翻译 Virtual Address |
| `udd_gmu_req_size` | 8 | → | 翻译请求大小 (字节) |
| `udd_gmu_req_ctx` | 8 | → | Context_ID |
| `udd_gmu_resp_valid` | 1 | ← | 翻译响应有效 |
| `udd_gmu_resp_gupa` | 64 | ← | 翻译后 GUPA |
| `udd_gmu_resp_perms` | 3 | ← | 权限位 (R/W/X) |
| `udd_gmu_resp_status` | 2 | ← | 0:OK, 1:Page_Fault, 2:Permission_Fault |

**v1.0 MVP 时序约束** (per `20-gmmu-mvp.md` §5.1):
- 翻译流程总延迟 ≤ 5 cycles @ clk_core (L1 TLB hit) / ≤ 45 cycles (L1 miss + PTW)

### §5.4 Global UDD Hub ↔ Backends 接口 (Dispatch, per RTL-IFC §2.4)

| 信号名 | 位宽 | 描述 |
|--------|------|------|
| `udd_be_req_valid` | 1 | 请求有效 |
| `udd_be_req_tag` | 4 | `Route_Tag` (0:HBM, 1~E:UALink, F:PCIe) |
| `udd_be_req_vc` | 3 | 虚拟通道 ID |
| `udd_be_req_gupa` | 64 | 目标 GUPA |
| `udd_be_resp_valid` | 1 | 响应有效 |
| `udd_be_resp_tracker_id` | 16 | Tracker_ID (匹配原始请求) |
| `udd_be_resp_status` | 4 | Status Code (per `21-dma-backends-mvp.md` §3.1) |
| `udd_be_resp_data` | 256 | 返回数据 (256B) |

**v1.0 MVP Backend 特定响应差异**:
- **HBM-DMA**: 返回标准 256B 数据或 Poison 状态
- **NIC-DMA** (v1.0+): 封装为 UALink Mem Flit → Scale-Up Switch
- **IO-DMA** (v1.1+): 封装为 PCIe TLP (ATS/PRI 支持)

---

## §6 控制面与配置接口 (per MAS-3.1-RTL-IFC §3)

### §6.1 CIU ↔ UDD HRT Shadow 原子更新接口 (per RTL-IFC §3.1)

| 信号名 | 位宽 | 方向 | 描述 |
|--------|------|------|------|
| `ciu_apb_paddr` | 32 | → | APB 地址 (HRT Shadow 索引) |
| `ciu_apb_pwdata` | 32 | → | APB 写入数据 (32-bit HRT Entry) |
| `ciu_apb_pwrite` | 1 | → | APB 写使能 |
| `ciu_apb_psel` | 1 | → | APB 片选 |
| `ciu_apb_penable` | 1 | → | APB 传输使能 |
| `ciu_apb_pready` | 1 | ← | APB 传输就绪 |
| `ciu_swap_bit` | 1 | → | HRT Atomic Swap 触发 (per tee-udd-mvp §7.3) |
| `udd_swap_done` | 1 | ← | HRT Swap 完成 (触发 MSI-X) |

**v1.0 MVP 时序约束** (per `21-tee-udd-mvp.md` §7.3):
- 整个 Atomic Swap 过程导致 SM Stall **不得超过 8 个 clk_core 周期**
- `udd_swap_done` 在 swap 完成时拉高, 保持 1 cycle

### §6.2 Scale-Up Drain 控制接口 (per RTL-IFC §3.2)

| 信号名 | 方向 | 描述 |
|--------|------|------|
| `fm_nic_drain_req` | → | FM 触发 Drain (携带目标 Switch Port ID) |
| `nic_fm_drain_ack` | ← | NIC-DMA 确认已停止向该 Port 发送新请求 |
| `nic_fm_drain_done` | ← | NIC-DMA 内部 Outstanding Counter 归零后拉高 |

**v1.0 MVP 时序约束** (per `21-fabric-switch-mvp.md` §6.2 + `21-microarch-ifc-mvp.md` §6.2):
1. FM 写入 `NIC_DRAIN_CTRL.START_DRAIN = 1`
2. NIC-DMA 在 HRT 中将对应 Scale-Up Port 标记为 `DRAINING`, 拒绝新 Micro-op (直接返回 Abort)
3. NIC-DMA 通过 UALink 控制通道 (或 SMBus Sideband) 向 Switch 发送 `QUIESCE_PORT`
4. **超时保护**: 若 NIC-DMA 在 **100 万个 clk_io 周期 (100 μs @ 1.0 GHz)** 内未收到 Switch `PORT_QUIESCED`, 触发 Fatal RAS 中断
5. 收到 Switch 确认且本地 Counter 归零后, 拉高 `DRAIN_COMPLETE` 并触发 MSI-X

### §6.3 远端 CXL Poison 传播延迟约束 (per RTL-IFC §6.3)

```
v1.0 MVP Poison 传播延迟:
1. 外部 CXL 设备发生 ECC 错误, CXL Poison 传至 Scale-Up Switch
2. Switch 将 CXL Poison 映射为 UALink Flit 中的 Poison Bit (per `21-fabric-switch-mvp.md` §7)
3. NIC-DMA 解封装后, 将 Poison 附加在 UDD Response 上
4. **总延迟约束**: 从 NIC-DMA 收到带 Poison 的 UALink Flit, 到 SM 挂起对应 Warp,
   **总延迟不得超过 5 个 clk_fab 周期 + 2 个 clk_core 周期**
5. SM 收到 Poison 后, 必须在 1 个周期内阻断该 Warp 的寄存器写回 (rf_we = 0)
```

---

## §7 寄存器映射规范 (per MAS-3.1-RTL-IFC §4)

### §7.1 基地址约定

```
v1.0 MVP MMIO 基地址: 0x00F0_0000

控制寄存器由 Host CPU (Driver) 或 FM 经 PCIe BAR 访问。
所有寄存器为 32-bit 或 64-bit 宽, 4-byte 或 8-byte aligned。
```

### §7.2 UDD 控制与状态寄存器 (per RTL-IFC §4.1)

| 偏移地址 | 寄存器名称 | R/W | 描述 |
|----------|-----------|-----|------|
| `0x0000` | `UDD_VERSION` | R | UDD 硬件版本号 |
| `0x0010` | `HRT_CTRL` | W | Bit 0: `SWAP_BIT` (触发原子切换) |
| `0x0014` | `HRT_STATUS` | R | Bit 0: `SWAP_PENDING`, Bit 1: `UPDATE_ACK` |
| `0x0020` | `RCT_CTRL` | W | Context 切换控制 |
| **移除** | **`CXL_DRAIN_CTRL`** | - | **[V3.1-Rev2.0 移除]** 片上无 CXL 控制器 |

### §7.3 NIC-DMA (Scale-Up) 控制与状态寄存器 (per RTL-IFC §4.2)

| 偏移地址 | 寄存器名称 | R/W | 描述 |
|----------|-----------|-----|------|
| `0x0200` | `NIC_PORT_CTRL` | W | `[3:0]`: Port Enable, `[7:4]`: UALink Speed Config |
| `0x0210` | `NIC_DRAIN_CTRL` | W | Bit 0: `START_DRAIN`, Bit 1: `FORCE_ABORT` |
| `0x0214` | `NIC_DRAIN_STATUS` | R | Bit 0: `DRAINING`, Bit 1: `SWITCH_QUIESCED`, Bit 2: `DRAIN_COMPLETE`, `[31:16]`: Outstanding Count |
| `0x0220` | `NIC_PHY_STATUS` | R | `[3:0]`: Link Speed, `[7:4]`: Lane Width, Bit 8: `LINK_UP` |
| `0x0230` | `NIC_PMU` | R | Retry/Replay/Timeout/Drain 计数 |

### §7.4 HBM-DMA 控制与状态寄存器 (per `21-dma-backends-mvp.md` §9.2)

| 偏移地址 | 寄存器名称 | R/W | 描述 |
|----------|-----------|-----|------|
| `0x0400` | `HBM_CTRL` | W | Bit 0: `ENABLE`, Bit 1: `ECC_BYPASS` (调试) |
| `0x0410` | `HBM_PHY_STATUS` | R | `[3:0]`: Speed, `[7:4]`: PC Enabled Mask |
| `0x0420` | `HBM_PMU_ROHIT` | R | Row Buffer Hit 计数 |
| `0x0428` | `HBM_PMU_ECC` | R | `[31:0]`: SEC 计数, `[63:32]`: DED 计数 |
| `0x0430` | `HBM_RETIRED_ROWS` | R | Retired Row 数量 + 索引 |

### §7.5 IO-DMA 寄存器 (v1.1+ 预留, per RTL-IFC §4.3)

| 偏移地址 | 寄存器名称 | R/W | 描述 |
|----------|-----------|-----|------|
| `0x0300` | `PCIE_CTRL` | W | Bit 0: `LINK_ENABLE`, Bit 1: `ATS_ENABLE` |
| `0x0304` | `PCIE_STATUS` | R | `[3:0]`: Link Speed/Width, Bit 4: `ATS_ACTIVE` |
| `0x0310` | `GDS_CTRL` | W | GPU Direct Storage 零拷贝控制 |

### §7.6 AWT (Asynchronous Warp Trap) 与 RAS 寄存器 (per RTL-IFC §4.4 + V3.1-Rev2.0 拓扑修正)

| 偏移地址 | 寄存器名称 | R/W | 描述 |
|----------|-----------|-----|------|
| `0x010C` | `AWT_PAYLOAD_2` | R | `[7:0]`: Trap Type |

**Trap Type 定义** (per RTL-IFC §4.4 + V3.1-Rev2.0 拓扑修正, `21-soc-topology-mvp.md` §4.3):

| Trap Type | 名称 (V3.1-Rev2.0) | 来源识别 | 物理介质归属 | v1.0 MVP |
|:----------|:-------------------|:---------|:------------|:---------|
| `0x0` | `LOCAL_HBM_POISON` | HBM-DMA 内 SECDED DED / Page Retire | GPU Die 内 (HBM3e Stack) | ✅ |
| `0x1` | `REMOTE_CXL_POISON_VIA_SWITCH` | NIC-DMA 收到 UALink Remote_Error Flit (来自外部 Scale-Up Switch PTE 转换) | 外部 CXL Memory Pool (经 Scale-Up Switch) | ✅ |
| `0x2` | `SECURITY_VIOLATION` | Global UDD Hub RCT 校验失败 (越权访问 GUPA) | GPU Die 内 | ✅ |
| `0x3` | `PCIE_IO_ABORT` | IO-DMA PCIe Link Down / UR (Unsupported Request) | 外部 PCIe 端点 | ❌ v1.1+ |

> **⚠️ V3.1-Rev2.0 关键修正**:
> Trap Type `0x1` 的完整名称是 `REMOTE_CXL_POISON_VIA_SWITCH`——
> **显式标注 Poison 来源是外部 Scale-Up Switch 转换的 CXL Memory Pool**，
> 而**非** GPU 直连 CXL (V3.0 错误理解, GPU Die 上**无 CXL PHY/Controller**)。
>
> Poison 传播完整路径 (per `21-fabric-switch-mvp.md` §7.1):
> 1. CXL Memory Pool 发生 ECC DED → CXL.mem Completion 携带 Poison Bit (`0b10`)
> 2. 外部 Scale-Up Switch PTE 检测 → 转换为 UALink `MEM_RESP_D` with `Status=0x11` (Remote_Error)
> 3. GPU NIC-DMA 接收 UALink Flit → 组装 UDD Response `Status=0x1` (Remote_CXL_Poison_Via_Switch)
> 4. Global UDD Hub → GPC UDD Agent → SM LSU 检测
> 5. LSU 阻断 rf_we, 触发 AWT Controller, Trap Type 写入 `0x1`

**Trap Type 区分对软件/驱动的影响**:

| Trap Type | 软件处理责任方 | 推荐恢复动作 | v1.0 MVP |
|:----------|:---------------|:-------------|:---------|
| `0x0` LOCAL_HBM_POISON | KMD (Kernel Mode Driver) | 隔离物理页, 迁移数据, 重启 Kernel | ✅ |
| `0x1` REMOTE_CXL_POISON_VIA_SWITCH | FM (Fabric Manager) + KMD | FM 隔离远端 CXL 节点, KMD 触发 Checkpoint 恢复 | ✅ |
| `0x2` SECURITY_VIOLATION | KMD + 虚拟化 Hypervisor | 拒绝 GUPA, 记录恶意行为, 触发 Guest 重启 | ✅ |
| `0x3` PCIE_IO_ABORT | KMD (Host 端驱动) | 重置 PCIe 链路, 通知 Host OS 驱动 | ❌ v1.1+ |

---

## §8 中断与异常接口 (per MAS-3.1-RTL-IFC §5)

### §8.1 MSI-X Payload 格式 (32-bit, per RTL-IFC §5.1)

```cpp
struct mas_msi_payload {
    uint8_t  vector_id;    // 中断向量号 (AWT=0x10, HRT_SWAP=0x11, DRAIN=0x12)
    uint8_t  gpu_node_id;  // 物理 GPU ID
    uint8_t  event_type;   // 0:Trap, 1:Config Done, 2:Drain Complete
    uint8_t  severity;     // 0:Info, 1:Warning, 2:Fatal
};
```

### §8.2 v1.0 MVP 中断向量分配

| 向量号 | 名称 | 触发条件 | Severity |
|--------|------|---------|----------|
| `0x10` | AWT (Asynchronous Warp Trap) | UDD Agent 检测到 Poison | 0:Info / 2:Fatal |
| `0x11` | HRT_SWAP (HRT Atomic Swap 完成) | CIU 触发 Swap, UDD Agent 完成 | 0:Info |
| `0x12` | DRAIN (Scale-Up Drain 完成) | NIC-DMA ROB 归零 | 0:Info |
| `0x13` | DRAIN_TIMEOUT | NIC-DMA 100us 超时未收到 Switch ACK | 2:Fatal |
| `0x14` | HBM_ECC_DED | HBM-DMA 检测到不可纠正错误 | 2:Fatal |
| `0x15` | HBM_RETIRED | HBM Page Retirement 触发 | 1:Warning |
| `0x20` | SECURITY_VIOLATION | RCT 越权访问 | 2:Fatal |

**v1.0 MVP 必须实现**: 0x10 / 0x11 / 0x12 / 0x13 / 0x14 / 0x15 / 0x20; v1.1+ 加 0x30+ (PCIe 相关)。

### §8.3 中断聚合 (per RTL-IFC §5.1)

```
v1.0 MVP 中断聚合:
- 单个 MSI-X 中断可携带多个事件 (bitmask 编码)
- 支持基于时间 (Timer) 或计数 (Count) 的中断聚合
- 聚合阈值通过 CIU_INT_AGGREGATE_CTRL 配置
- 默认阈值: 100 us 或 16 events, 满足任一触发 MSI-X
```

---

## §9 关键时序与微架构约束 (per MAS-3.1-RTL-IFC §6)

### §9.1 TC-DMA SMEM 直达与 mbarrier 触发时序 (per RTL-IFC §6.1)

```
v1.0 MVP 严格时序约束:
1. TC-DMA 收到 UDD Agent 返回的 udd_tc_resp_valid 且 status == OK
2. TC-DMA 必须在同一个 clk_core 周期内将 256B 数据写入 SMEM Bank
3. 若 TMA 描述符配置了自动同步, TC-DMA 必须在 SMEM 写入完成的下一个 clk_core 上升沿拉高 udd_tc_mbarrier_trig
4. SMEM 写入与 mbarrier 触发之间不允许插入任何 Stall 周期
```

### §9.2 HRT Atomic Swap 时序约束 (per RTL-IFC §6.1 + tee-udd-mvp §7.3)

```
v1.0 MVP HRT Atomic Swap:
1. CIU 写入 HRT_CTRL.SWAP_BIT = 1
2. UDD Agent 立即拒绝新 LSU 请求 (拉低 sm_udd_req_ready)
3. 等待当前 in-flight 请求完成 (≤3 cycles)
4. clk_core 上升沿: Active/Shadow 指针原子翻转
5. UDD Agent 恢复接收新请求, 使用新 Active Bank 查表
6. 拉高 swap_done 信号, 触发 CIU 侧 MSI-X

总 Stall ≤8 cycles @ clk_core (per MAS-3.1-UDD-MAS §3.3)
```

### §9.3 远端 CXL Poison 传播延迟约束 (per RTL-IFC §6.3)

```
v1.0 MVP Poison 传播延迟:
1. 外部 CXL 设备 ECC 错误 → Switch PTE 检测 → 标记 UALink Flit Status=Remote_Error
2. NIC-DMA 接收带 Poison 的 UALink Flit (clk_io 域)
3. NIC-DMA 通过 clk_io → clk_fab 异步 FIFO 传递 Status
4. Global UDD Hub 接收 Response, 通过 clk_fab → clk_core 异步 FIFO 传递
5. GPC UDD Agent 路由至 SM LSU
6. LSU 在 Writeback Stage 检测 Poison, 阻断 rf_we

总延迟约束:
- NIC-DMA 收到 Poison Flit → SM 挂起 Warp: ≤ 5 clk_fab + 2 clk_core cycles
- SM 阻断寄存器写回: 1 cycle
```

---

## §10 测试覆盖

### §10.1 单元测试 (`test_microarch_*.cc`, `test_ifc_*.cc`)

| # | 测试用例 | 覆盖能力 | 断言 |
|---|---------|---------|------|
| 1 | `MicroArch.SM.LSU throughput` | LSU 4 通道吞吐 | 4 LSU/cycle |
| 2 | `MicroArch.SM.L1 TLB hit` | L1 TLB | 5 cycle hit latency |
| 3 | `MicroArch.TC-DMA.Descriptor Cache` | 描述符预取 | 16 entry cache hit |
| 4 | `MicroArch.TC-DMA.Swizzle` | Bank Conflict-Free | 5D Tensor 无冲突 |
| 5 | `MicroArch.TC-DMA.mbarrier trigger` | 硬件同步 | 1 cycle 触发 |
| 6 | `MicroArch.UDD Agent.WRR arbitration` | TC:SM=4:1 公平 | 4 TC : 1 SM 比例正确 |
| 7 | `MicroArch.UDD Agent.HRT lookup pipeline` | 3 cycle pipeline | Cycle 1+2+3 输出正确 |
| 8 | `MicroArch.UDD Agent.HRT Atomic Swap` | 双 Bank 切换 | ≤8 cycles Stall |
| 9 | `MicroArch.Global Hub.RCT check` | RCT 校验 | 越权 → AWT Trap |
| 10 | `MicroArch.Global Hub.Credit` | Credit 流控 | Credit 满 → Stall |
| 11 | `IFC.SM UDD handshake` | Valid/Ready 协议 | 正确握手 |
| 12 | `IFC.TC-DMA SMEM direct write` | SMEM 直达 | 同一 cycle 写入 |
| 13 | `IFC.UDD GMMU translation` | 翻译接口 | 5/45 cycle latency |
| 14 | `IFC.CIU HRT Shadow APB` | APB 协议 | 正确写入 |
| 15 | `IFC.HRT Atomic Swap timing` | 8 cycle 约束 | swap_done 在 8 cycles 内拉高 |
| 16 | `IFC.NIC Drain two-phase` | 两阶段握手 | 100us 超时触发 |
| 17 | `IFC.Poison propagation latency` | 5+2 cycle 约束 | 总延迟 ≤ 7 cycles |
| 18 | `CSR.MMIO layout` | 寄存器映射 | 所有偏移地址正确 |

### §10.2 集成测试 (`test_microarch_ifc_e2e.cc`)

**核心 demo 测试** (per §2 端到端 shippable demo):
- 12 步全链路贯通 (SM → TEE → GMMU → UDD → HBM-DMA)
- 测试标签: `[microarch][ifc][hbm][e2e]`

### §10.3 时序验证 (Timing Verification)

- Static Timing Analysis (STA) on clk_core / clk_fab / clk_io / clk_cfg
- 跨时钟域 CDC 验证 (Gray-code + 2-flop 同步)
- 形式验证 (Formal Verification) on HRT Atomic Swap 状态机

### §10.4 Oracle 评审清单

- [ ] GPC 内部 4 条本地接口信号定义完整 (SM-UDD/TC-UDD/UDD-GMMU/CIU-UDD)
- [ ] HRT 原子切换 ≤8 cycles (per MAS-3.1-UDD-MAS §3.3)
- [ ] TC-DMA mbarrier 触发延迟 ≤1 cycle (per MAS-3.1-DMA-TEE §8.3)
- [ ] WRR 仲裁公平性 (TC-DMA : SM = 4:1)
- [ ] Local HBM Load 总延迟 ~14.5 ns (per §4.1)
- [ ] Poison 传播延迟 ≤ 5 clk_fab + 2 clk_core cycles (per §9.3)
- [ ] Drain FSM 100us 超时正确 (per §6.2)
- [ ] 跨时钟域 CDC 无 metastability (Gray-code + 2-flop)
- [ ] MMIO 寄存器布局与 Host driver 约定一致
- [ ] 跨仓 ABI 影响: 0 个新 ABI 函数 (v1.0 MVP 严格遵守)
- [ ] 5 阶段演进路线图无债务 (per evolution-roadmap §5)

---

## §11 演进不变量 (4 条契约)

**详细定义见 [`21-microarch-ifc-evolution-roadmap.md` §5](21-microarch-ifc-evolution-roadmap.md)**。本节列出 v1.0 MVP 的具体实现要求。

### §11.1 不变量 1: 跨模块信号接口在演进中只增不换

```
v1.0 MVP: 4 条本地接口 (SM-UDD/TC-UDD/UDD-GMMU/CIU-UDD) 信号位宽与定义冻结
v2.0+: 仅扩展预留信号 (如 Payload 新增字段), 不修改既有信号
实现: 每个接口预留 [MAX_BITS-1:N+1] bits 备用
```

### §11.2 不变量 2: 时钟域划分从 v1.0 起 4 域稳定

```
v1.0 MVP: clk_core (2.0GHz) / clk_fab (1.2GHz) / clk_io (1.0GHz) / clk_cfg (100MHz)
v2.0+: 不引入新时钟域, 现有时钟域频率可在 ±10% 范围内调整
```

### §11.3 不变量 3: MMIO 寄存器布局从 v1.0 起 extensible

```
v1.0 MVP MMIO 基地址 0x00F0_0000, 寄存器映射见 §7
v2.0+: 仅追加 0x0500+ 区间寄存器 (不修改 0x0000-0x04FF 已定义寄存器)
实现: 0x0500-0xFFFF 区间保留 v1.1+ 扩展
```

### §11.4 不变量 4: AWT Trap Type 从 v1.0 起定义完整

```
v1.0 MVP: AWT_PAYLOAD_2 定义 4 种 Trap Type (0x0/0x1/0x2/0x3)
v2.0+: 仅追加 Trap Type (如 0x4 Remote_Link_Fault), 不修改既有 Type 含义
实现: Trap Type 8-bit 字段, 高 4-bit 预留演进
```

---

## §12 边界与限制

### §12.1 不实现的功能 (v1.0 MVP 明确边界)

- ❌ **L1 Link (PCIe / CXL.io) 在 v1.0** — backends IO-DMA 推迟 v1.1+
- ❌ **Multicast / All-Reduce / Hardware Reduce** — v3.0+
- ❌ **Coherence SnoopFilter** — v3.0+
- ❌ **Hot-plug / Hot-unplug** — v2.0+
- ❌ **Atomic Operations** — v2.1+
- ❌ **时钟域频率动态调整 (DVFS)** — 不在 v1.0 MVP 范围
- ❌ **远端 AWT (Remote_Link_Fault)** — v2.0+
- ❌ **真实 RTL 时序 (Static Timing Analysis)** — v1.0 TLM 仿真, RTL 时序推迟实施阶段

### §12.2 已知限制

- 时钟域固定 4 域 (clk_core/clk_fab/clk_io/clk_cfg), 不支持动态 DVFS
- 跨时钟域 FIFO 深度固定 (64/128), 不支持运行时调整
- MMIO 寄存器布局 32-bit / 64-bit 混合, 不支持 128-bit 寄存器
- AWT FIFO 深度: per-SM 16 entry, 满 → Stall 整个 SM 发射
- HRT 切换期间 SM LSU 必须 Stall (无法旁路)
- TC-DMA mbarrier 64 entry, 单 Tensor 不超过 64 个 mbarrier

---

## §13 跨仓契约

### §13.1 UsrLinuxEmu 侧 (不修改 23 ABI)

MicroArch+IFC v1.0 MVP **不引入任何新 CppTLM ABI 函数**——所有 driver↔GPU 通信通过**现有 23 ABI 函数** (`syms->mmio_write/read`).

### §13.2 CppTLM 侧 (新模块 + 信号定义)

| 文件 | 状态 |
|------|------|
| `include/tlm/core/sm_lsu_tlm.{hh,cc}` | **新建** (SM LSU 微架构) |
| `include/tlm/core/tc_dma_tlm.{hh,cc}` | **新建** (TC-DMA TMA 微架构) |
| `include/tlm/core/gpc_udd_agent_tlm.{hh,cc}` | **新建** (GPC 内 UDD Agent 微架构) |
| `include/tlm/core/global_udd_hub_tlm.{hh,cc}` | **新建** (Global Hub 微架构, 与 tee-udd 共享) |
| `include/tlm/core/awt_controller_tlm.{hh,cc}` | **新建** (AWT RAS 控制器) |
| `include/tlm/core/ifc_signal_defs.hh` | **新建** (跨模块信号定义头文件) |
| `include/tlm/core/csr_layout.hh` | **新建** (CSR 寄存器布局头文件) |
| `test/test_microarch_*.cc` (10+ 用例) | **新建** |
| `test/test_ifc_*.cc` (8+ 用例) | **新建** |
| `test/test_microarch_ifc_e2e.cc` | **新建** (端到端 demo) |
| **23 ABI 头冻结** | ✅ **不变** (per ADR-088 §D5) |

### §13.3 跨仓 PR 协调

按 ADR-091 §R5.1 跨仓 PR 流程:
1. CppTLM 仓: 实现 MicroArch + IFC 模块 + 18+ 测试
2. UsrLinuxEmu 仓: driver 协调 CSR 寄存器 (由用户承担协调)
3. 跨仓集成测试: `test_microarch_ifc_e2e_ue.cc`
4. 同步 PR (无 ABI 影响, 跨仓风险低)

---

## §14 引用

### §14.1 内部引用

- [`21-microarch-ifc-evolution-roadmap.md`](21-microarch-ifc-evolution-roadmap.md) — MicroArch+IFC 5 阶段演进路线图 (SSOT for evolution)
- [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) — TEE + UDD MVP (协议层)
- [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) — Backends MVP (协议层)
- [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) — Fabric + Switch MVP (协议层)
- [`20-gmmu-mvp.md`](20-gmmu-mvp.md) — GMMU MVP (翻译层协同)
- [`16-pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT

### §14.2 跨仓引用

- UsrLinuxEmu AGENTS.md §CppTLM 通信架构 — **PCIe-only 原则**
- UsrLinuxEmu ADR-088 §D5 — 23 ABI 冻结

### §14.3 MAS-3.1 上游原始设计

- `MAS-3.1-Core/Micro-Arch Rev2.0` (Core 微架构原始设计)
- `MAS-3.1-RTL-IFC Rev2.0` (RTL 接口原始设计)

### §14.4 业界参考

- NVIDIA Hopper SM + TMA + Distributed Shared Memory
- AMD CDNA3 Compute Unit + MFMA + LDS
- Intel Xe Link + Tile-to-Tile Fabric
- ARM CMN-700 (NoC + HN-F Router)

---

## §15 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: Core/Micro-Arch + RTL-IFC v1.0 MVP 详细设计 (GPC 微架构 + 4 条本地接口信号 + CSR 寄存器布局 + AWT RAS + 12 步 shippable demo + 18 测试用例 + 4 条不变量) |

---

**关联 OpenSpec change**: 待 `openspec/changes/2026-09-19-cpptlm-mas-microarch-ifc-mvp/` 提案创建
**下次更新**: Oracle 评审反馈后 v1.1 (NIC-DMA + IO-DMA 引入)