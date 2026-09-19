# TEE + UDD v1.0 MVP 详细设计 (Task Execution Engine & Unified Data Dispatcher — Minimum Viable Product)

> **目的**: 定义 CppTLM dGPU SoC **MAS-3.1** 中 **TEE (Task Execution Engine)** 与 **UDD (Unified Data Dispatcher)** 的 v1.0 MVP 微架构——作为 dGPU 数据搬运 + 数据路由的**核心数据面**，与后端 `21-dma-backends-mvp.md` (UBC + HBM/NIC/IO-DMA) 通过 160-bit UDD Micro-op 标准接口解耦。所有 v1.0 路径仅使用 **Local HBM** 一种 Backend，**UALink / PCIe IO** 在 v1.1+ 引入。
>
> **状态**: Draft v1.0 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: 待 `openspec/changes/2026-09-19-cpptlm-mas-tee-udd-mvp/` 提案对齐
> **关联文档**:
> - [`21-tee-udd-evolution-roadmap.md`](21-tee-udd-evolution-roadmap.md) — TEE-UDD 5 阶段演进路线图 (v1.0/v1.1/v2.0/v2.1/v3.0)
> - [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) — UBC + HBM/NIC/IO-DMA MVP (本模块下游)
> - [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) — Fabric 协议 + Scale-Up Switch MVP (本模块协议层)
> - [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) — Core/Micro-Arch + RTL-IFC MVP (本模块微架构与接口)
> - [`21-soc-topology-mvp.md`](21-soc-topology-mvp.md) — SoC 顶层物理布局规范 (V3.1-Rev2.0 拓扑修正: CXL 不在 GPU Die / TC-DMA 归属 GPC / HRT 无 CXL_DIRECT)
> - [`20-gmmu-mvp.md`](20-gmmu-mvp.md) — GMMU v1.0 MVP (GMMU = 翻译层, UDD = 路由层, 二者协同)
> - [`16-pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT
> **关联 ADR**:
> - ADR-088 §D5 — 23 ABI 冻结 (TEE-UDD v1.0 不动 ABI)
> - ADR-SOC-09 — v1.0 NVIDIA+AMD dual vendor 战略 (TEE/UDD 共享 vendor 路径)
> - ADR-SOC-10 (待起草) — TEE/UDD/UBC 三层解耦 (本模块边界)
> - ADR-SOC-21 — **V3.1-Rev2.0 拓扑修正** (CXL 不在 GPU Die / HRT 无 CXL_DIRECT)

---

## §1 概述

### §1.1 TEE + UDD 在 dGPU SoC 中的位置

TEE 与 UDD 共同构成 dGPU SoC 的**核心数据面**，位于计算核心 (SM/TC-DMA) 与后端物理介质 (HBM/UALink/PCIe) 之间。

```
┌─────────────────────────────────────────────────────────────────┐
│                       GPU 内部 (dGPU SoC)                        │
│                                                                 │
│  ┌─────────────────┐                                            │
│  │  SM / TC-DMA    │  (发起 Load/Store/TMA 描述符)              │
│  └────────┬────────┘                                            │
│           │                                                      │
│           ▼                                                      │
│  ╔═══════════════════════════════════════════════════════════╗  │
│  ║              ★ TEE (Task Execution Engine)                ║  │
│  ║                                                           ║  │
│  ║  ┌──────────────┐    ┌──────────────┐                     ║  │
│  ║  │   Frontend    │    │   Midend     │                     ║  │
│  ║  │ (Fetch+Trans │───▶│ (AGU + DTE)  │                     ║  │
│  ║  │  +Route+Disp)│    │              │                     ║  │
│  ║  └──────┬───────┘    └──────┬───────┘                     ║  │
│  ║         │                   │                              ║  │
│  ║         └─────────┬─────────┘                              ║  │
│  ║                   │ 160-bit UDD Micro-op                  ║  │
│  ╚═══════════════════╪═══════════════════════════════════════╝  │
│                      │                                           │
│                      ▼                                           │
│  ╔═══════════════════════════════════════════════════════════╗  │
│  ║              ★ UDD (Unified Data Dispatcher)              ║  │
│  ║                                                           ║  │
│  ║  [UDD Agent ×N] ──▶ [Global UDD Hub] ──▶ [UBC × Backend] ║  │
│  ║  (per-GPC HRT查表)   (RCT + 路由仲裁)     (见 backends)   ║  │
│  ╚═══════════════════════════════════════════════════════════╝  │
│                      │                                           │
└──────────────────────┼──────────────────────────────────────────┘
                       │
                       ▼ NoC Injection → Backend (见 backends 文档)
```

### §1.2 为什么需要 TEE / UDD 解耦 (替代 V3.0 紧耦合)

| 维度 | V3.0 紧耦合 DMA | TEE + UDD v1.0 MVP (新) |
|------|------------------|--------------------------|
| **任务执行** | 与 Backend Adapter 紧耦合 | TEE 仅产出 160-bit Micro-op, 与 Backend 解耦 |
| **地址翻译** | Frontend 内部硬编码 HBM/NIC/IO 路径 | UDD 集中查 HRT, 输出统一 GUPA + Route_Tag |
| **协议演进** | 每加一种 Backend 改 Frontend | 仅 UDD HRT 表 + Backend Adapter 增量更新 |
| **RTL 复用** | 三个 DMA 控制器独立开发 | TEE 单份代码 + UDD 通用查表 + Backend 复用 UBC |
| **验证方法** | 每个 DMA 单独写 UVM | TEE UVM + UDD UVM + Backend Protocol VIP 复用 |
| **演进路径** | ❌ 闭路, 加新 Backend 必重构 Frontend | ✅ 5 阶段路线图 (无债务, 见 evolution-roadmap) |

**核心设计原则** (per MAS-3.1 Rev2.0):
> **TEE = 软件任务到硬件 Micro-op 的转换层, 不知道任何后端协议**
> **UDD = GUPA 到物理 Route_Tag 的路由层, 不感知后端协议细节**
> **UBC = Backend 共享基座 (见 backends 文档), 屏蔽 HBM/UALink/PCIe 差异**

### §1.3 命名约定

| 旧名 (V3.0) | 新名 (V3.1-Rev2.0) | 原因 |
|--------------|---------------------|------|
| `SdmaEngineTLM` | `TeeTLM` | V3.0 SDMA 是单一引擎, V3.1 拆为 TEE (任务执行) + UBC (后端桥接) |
| `NicDmaTLM` (旧) | `NicDmaUBC` (V3.1) | NIC-DMA 后端逻辑归入 UBC (见 backends 文档) |
| `IoDmaTLM` (旧) | `IoDmaUBC` (V3.1) | IO-DMA 后端逻辑归入 UBC (见 backends 文档) |
| `HbmDmaTLM` (旧) | `HbmDmaUBC` (V3.1) | HBM-DMA 后端逻辑归入 UBC (见 backends 文档) |
| `GUPA` (V3.1 新概念) | 不变 | 64-bit 全局统一物理地址 (UDD 输出) |
| `UDD Micro-op` (V3.1 新概念) | 不变 | 160-bit 标准请求格式 (TEE → UDD 内部或 TEE → UBC 跨边界) |
| **`cpptlm_dma_translate_cb`** | **不变** (ABI 冻结) | 跨仓契约保持 (per ADR-088 §D3.8) |

**重要边界**: 命名变更**不**影响跨仓 ABI 签名, 仅影响仓内代码可读性 + 与 MAS-3.1 Rev2.0 的概念对齐。

### §1.4 与 GMMU 的关系 (per `20-gmmu-mvp.md` §1.2)

| 维度 | GMMU (翻译层) | TEE-UDD (路由层) |
|------|----------------|--------------------|
| **职责** | VA → GUPA 翻译 | GUPA → 物理后端路由 |
| **触发者** | TEE Frontend 在 translate 阶段查询 | UDD Agent 在 route 阶段查询 |
| **数据结构** | L1/L2 TLB + Context_ID 表 | HRT Shadow Table + RCT |
| **原子更新** | TLB Invalidate MMIO 命令 | HRT Atomic Swap 机制 |
| **多租户** | Context_ID (per-process ASID) | RCT (per-tenant Route_Tag override) |
| **演进阶段** | v1.0 (本项目已 ship) | **v1.0 MVP (本模块交付)** |

**协同流程**: SM 发起 `load VA →` TEE Frontend `→` GMMU.translate(VA) → GUPA → TEE 注入 UDD `→` UDD Agent HRT 查表 → Route_Tag + VC_ID → NoC Injection → Backend UBC。

### §1.5 与 [Backends / Fabric / MicroArch] 文档的边界

| 子系统 | 文档 | 与本模块边界 |
|--------|------|---------------|
| **UBC + HBM/NIC/IO-DMA** | [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) | UDD 下游: 接收 Micro-op, 屏蔽协议差异 |
| **Fabric 协议 + Switch** | [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) | 协议层: TEE-UBC 标准化接口由 Fabric 定义 |
| **Micro-Arch + RTL-IFC** | [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) | 实现层: 时钟域 / 流水线 / 信号定义 / CSR |

---

## §2 TEE-UDD v1.0 MVP 端到端 Shippable Demo

v1.0 MVP 必须通过的"绿灯测试", 证明 **SM → TEE → UDD → HBM-DMA → HBM** 链路贯通 (v1.0 仅 HBM 后端):

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

**验收标准**: 9 步全部通过 + 测试用例 ≥ 10 个 (见 §12 测试覆盖)。

---

## §3 TEE 模块类定义

### §3.1 TEE 类签名

```cpp
// include/tlm/dma/tee_tlm.hh
namespace cpptlm::dma {

// TEE Frontend 4 级流水线状态(per MAS-3.1-DMA-TEE §3.1)
enum class TeePipelineStage : uint8_t {
    Fetch = 0,     // 拉取描述符
    Translate = 1, // GMMU 查询
    Route = 2,     // UDD HRT 查表 + Chunking
    Dispatch = 3   // NoC Injection
};

// TEE Descriptor (128B 对齐, per MAS-3.1-DMA-TEE §2.1)
struct TeeDescriptor {
    uint32_t opcode_flags;   // [31:000] Copy/Memset/Atomic/Fence/Bypass_L2
    uint32_t size_qos_ctx;   // [63:032] Size, QoS, Context_ID
    uint64_t src_va;         // [127:064] Src VA
    uint64_t dst_va;         // [191:128] Dst VA
    uint64_t tee_ext;        // [255:192] TEE 扩展 (TC-DMA Tensor Desc / Semaphore / Event)
};

// TEE UDD Micro-op (160-bit, per MAS-3.1-DMA-TEE §5.1)
struct UddMicroOp {
    uint32_t header;         // [159:128] Opcode, Size, VC_ID, QoS, Context_ID
    uint64_t gupa;           // [127:064] GUPA
    uint32_t payload_meta;   // [63:032]  Write Data / BE / Atomic Operand
    uint16_t tracker_id;     // [31:016] Tracker_ID
    uint16_t seq_route;      // [15:000] Sequence_Num + Route_Tag 隐式
};

class TeeTLM : public sc_module {
public:
    SC_HAS_PROCESS(TeeTLM);

    // 上游接口 (SM/TC-DMA 提交描述符)
    sc_port<tee_descriptor_if> sm_desc_in;     // SM LSU 描述符入口
    sc_port<tee_descriptor_if> tc_desc_in;     // TC-DMA 描述符入口 (per MAS-3.1 §8)

    // 下游接口 (UDD Agent, 通过 NoC 抽象)
    sc_port<udd_microop_if> udd_inject_out;    // Micro-op 注入 UDD

    // GMMU 翻译端口 (per gmmu-mvp §3.2)
    sc_port<gmmu_translator_if> gmmu_translator;

    // CIU 配置端口 (HRT Shadow 写入)
    sc_port<ciu_config_if> ciu;

    TeeTLM(sc_module_name name);
    ~TeeTLM() override = default;

    // 主 tick (每个 clk_core 推进 1 cycle)
    void tick();

    // TEE Pipeline 处理
    void pipeline_fetch();
    void pipeline_translate();
    void pipeline_route();
    void pipeline_dispatch();

    // 测试 helper: 描述符 in-flight 计数
    uint32_t inflight_count() const { return inflight_count_; }

    // 测试 helper: HRT 切换延迟统计
    double hrt_swap_latency_cycles() const {
        return hrt_swap_total_cycles_ / double(hrt_swap_count_ + 1);
    }

private:
    // 4 级流水线寄存器
    TeeDescriptor pipe_regs_[4] = {};           // 每个 stage 一份
    TeePipelineStage pipe_valid_ = TeePipelineStage::Fetch;

    // Descriptor Ring Buffer (per SM 上下文)
    static constexpr size_t DESC_RING_ENTRIES = 256;
    std::array<TeeDescriptor, DESC_RING_ENTRIES> desc_ring_{};
    size_t desc_ring_head_ = 0;
    size_t desc_ring_tail_ = 0;

    // In-flight tracker (per Frontend Shadow Queue, per MAS-3.1 §7)
    static constexpr size_t FRONTEND_TRACKER_ENTRIES = 256;
    struct TrackerEntry {
        uint64_t src_va;
        uint64_t dst_va;
        uint16_t tracker_id;
        uint8_t  context_id;
        uint8_t  valid;
    };
    std::array<TrackerEntry, FRONTEND_TRACKER_ENTRIES> frontend_tracker_{};
    uint16_t next_tracker_id_ = 0;
    uint32_t inflight_count_ = 0;

    // HRT 切换统计
    uint64_t hrt_swap_count_ = 0;
    uint64_t hrt_swap_total_cycles_ = 0;

    // Midend 配置
    uint32_t midend_chunk_size_ = 128;  // 默认 128B chunk
    bool     dte_bypass_ = true;         // v1.0 MVP 不做格式转换

    // 内联子例程
    void chunk_descriptor(const TeeDescriptor& desc, std::vector<UddMicroOp>& out);
    void emit_microop(const UddMicroOp& muop);
    void handle_replay_buffer_entry(uint16_t tracker_id);  // per §6 Page Fault
};

}  // namespace cpptlm::dma
```

### §3.2 UDD Agent 类签名 (per-GPC 分布式)

```cpp
// include/tlm/dma/udd_agent_tlm.hh
namespace cpptlm::dma {

// HRT Entry (32-bit, per MAS-3.1-UDD-MAS §3.1)
struct HrtEntry {
    uint8_t  route_tag;       // [31:28] 0:HBM, 1~E:UALink, F:PCIe
    uint8_t  vc_id;           // [27:24] Virtual Channel
    uint8_t  qos_priority;    // [23:16] QoS / Throttle_Group
    uint16_t addr_offset;     // [15:00] 细粒度地址偏移/Mask
};

// UDD Agent 状态
enum class HrtActiveBank : uint8_t { BankA, BankB };

class UddAgentTLM : public sc_module {
public:
    SC_HAS_PROCESS(UddAgentTLM);

    // 上游: TEE 注入的 Micro-op
    sc_port<udd_microop_if> tee_in;

    // 下游: NoC Injection (Compute NoC → Global Hub)
    sc_port<noc_flit_if> noc_inject_out;

    // CIU 配置 (HRT Shadow 写入)
    sc_port<ciu_config_if> ciu;

    UddAgentTLM(sc_module_name name);
    ~UddAgentTLM() override = default;

    // 主 tick
    void tick();

    // HRT 查表 (per MAS-3.1-UDD-MAS §3.2)
    HrtEntry hrt_lookup(uint64_t gupa);

    // 仲裁 (WRR, per MAS-3.1-Core §1.3)
    // SM 权重 = 1, TC-DMA 权重 = 4
    struct ArbRequest {
        bool     sm_valid;
        uint64_t sm_gupa;
        bool     tc_valid;
        uint64_t tc_gupa;
    };
    bool arbiter_wr_r(ArbRequest& req, bool& tc_won, uint64_t& selected_gupa);

private:
    // HRT 双 Bank (per MAS-3.1-UDD-MAS §3.1)
    static constexpr size_t HRT_ENTRIES = 4096;
    std::array<HrtEntry, HRT_ENTRIES> hrt_bank_a_{};
    std::array<HrtEntry, HRT_ENTRIES> hrt_bank_b_{};
    HrtActiveBank active_bank_ = HrtActiveBank::BankA;

    // WRR 仲裁状态
    uint32_t wrr_sm_credit_ = 1;     // SM 权重
    uint32_t wrr_tc_credit_ = 4;     // TC-DMA 权重

    // 注入 FIFO (Compute NoC 入口)
    static constexpr size_t INJECT_FIFO_DEPTH = 64;
    std::array<UddMicroOp, INJECT_FIFO_DEPTH> inject_fifo_{};
    size_t inject_head_ = 0;
    size_t inject_tail_ = 0;
    size_t inject_count_ = 0;

    // Local Tracker (per MAS-3.1-UDD-MAS §6.1)
    static constexpr size_t TRACKER_ENTRIES = 256;
    std::array<uint16_t, TRACKER_ENTRIES> tracker_pool_{};
    size_t tracker_alloc_idx_ = 0;

    // HRT 切换延迟统计
    uint64_t hrt_swap_pending_cycles_ = 0;
    bool     hrt_swap_pending_ = false;
};

}  // namespace cpptlm::dma
```

### §3.3 Global UDD Hub 类签名 (Die 中央集中式)

```cpp
// include/tlm/dma/global_udd_hub_tlm.hh
namespace cpptlm::dma {

// RCT Entry (per MAS-3.1-UDD-MAS §4.2)
struct RctEntry {
    uint64_t gupa_base;     // 授权 GUPA 区间起点
    uint64_t gupa_limit;    // 授权 GUPA 区间终点
    uint8_t  route_override; // 可选: 强制覆写 Route_Tag
    uint8_t  enable;
};

// Global Tracker (per MAS-3.1-UDD-MAS §6.2)
struct GlobalCreditCounter {
    uint32_t credits_total;       // 初始 128
    uint32_t credits_available;
    bool     stalled;
};

class GlobalUddHubTLM : public sc_module {
public:
    SC_HAS_PROCESS(GlobalUddHubTLM);

    // 上游: 多 GPC UDD Agent 的 Micro-op (per-GPC)
    static constexpr size_t MAX_GPCS = 8;
    sc_port<udd_microop_if> gpc_in[MAX_GPCS];

    // 下游: Backend (HBM/NIC/IO-DMA) 注入
    static constexpr size_t MAX_BACKENDS = 8;
    sc_port<udd_microop_if> backend_out[MAX_BACKENDS];

    // RCT 配置端口
    sc_port<ciu_config_if> ciu;

    GlobalUddHubTLM(sc_module_name name);
    ~GlobalUddHubTLM() override = default;

    // 主 tick (clk_fab 频率)
    void tick();

    // RCT Base/Limit 校验
    bool rct_check(uint8_t ctx_id, uint64_t gupa);

    // Credit 申请/归还
    bool credit_acquire(uint8_t backend_id);   // 申请 1 Credit
    void credit_return(uint8_t backend_id);    // 归还 1 Credit (Response 回流时)

private:
    // RCT (per MAS-3.1-UDD-MAS §4.1)
    static constexpr size_t MAX_CONTEXTS = 256;
    std::array<RctEntry, MAX_CONTEXTS> rct_table_{};

    // Per-Backend Credit
    std::array<GlobalCreditCounter, MAX_BACKENDS> credit_counters_{};

    // Flit Demultiplexer
    // Per-GPC 入口 FIFO (深度 64)
    std::array<UddMicroOp, 64> gpc_entry_fifo_[MAX_GPCS]{};
    size_t gpc_entry_head_[MAX_GPCS] = {0};
    size_t gpc_entry_tail_[MAX_GPCS] = {0};

    // 异常日志 (per MAS-3.1-UDD-MAS §5.1 CIU)
    uint64_t security_violation_count_ = 0;
    uint64_t last_violation_gupa_ = 0;
    uint8_t  last_violation_ctx_ = 0;
};

}  // namespace cpptlm::dma
```

### §3.4 CIU (Configuration & Isolation Unit) 类签名

```cpp
// include/tlm/dma/ciu_tlm.hh
namespace cpptlm::dma {

class CiutLM : public sc_module {
public:
    SC_HAS_PROCESS(CIUTLM);

    // 上游: PCIe MMIO (Host CPU / FM 经 Driver)
    sc_port<mmio_target_if> mmio_aperture;

    // 下游: 多 UDD Agent + Global Hub (APB 总线)
    static constexpr size_t MAX_AGENTS = 8;
    sc_port<ciu_config_if> udd_agent_apb[MAX_AGENTS];
    sc_port<ciu_config_if> global_hub_apb;

    CIUTLM(sc_module_name name);
    ~CIUTLM() override = default;

    // MMIO write 回调
    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);

private:
    // Route Update FSM (per MAS-3.1-UDD-MAS §5.2)
    enum class CiutFsmState : uint8_t {
        IDLE, PARSE_CMD, SHADOW_WRITE, WAIT_DRAIN, ATOMIC_SWAP, COMPLETE
    };
    CiutFsmState fsm_state_ = CiutFsmState::IDLE;

    // 跨时钟域 (clk_cfg 100MHz ↔ clk_core 2.0GHz)
    sc_fifo<HrtShadowUpdate> apb_to_core_fifo_{"apb_to_core", 16};
    sc_event atomic_swap_done_;

    // FSM tick (clk_cfg 域)
    void fsm_tick();
};

}  // namespace cpptlm::dma
```

---

## §4 TEE / UDD 寄存器与接口布局

### §4.0 GUPA 地址空间划分 + HRT Route_Tag 分配 (V3.1-Rev2.0 拓扑修正)

**GUPA 64-bit 地址空间划分 (per `21-soc-topology-mvp.md` §1.2 + `21-fabric-switch-mvp.md` §2)**:

| GUPA 区间 (高位 `[63:48]`) | 路由域 | 目标 Backend | 物理介质 | Cache 属性 |
|:---------------------------|:-------|:-------------|:---------|:-----------|
| `0x0000 ~ 0x0FFF` | **Local HBM** | HBM-DMA | HBM3e Stack (GPU Die 内) | Cacheable (L2) |
| `0x1000 ~ 0x7FFF` | **Scale-Up** | NIC-DMA (Port 0~N) | 外部 Scale-Up Switch (Peer GPU HBM 或 CXL Memory Pool) | Non-Cacheable / L2 旁路 |
| `0x8000 ~ 0x8FFF` | **PCIe IO / Host** | IO-DMA | Host CPU + NVMe (PCIe Gen6 x16) | Non-Cacheable |
| `0x9000 ~ 0x9FFF` | **MMIO / CSR** | Config Bus (APB/AXI-L) | GPU 内部寄存器 | Device (Strongly Ordered) |
| `0xA000 ~ 0xFFFF` | *Reserved / Invalid* | - | 触发 AWT Trap (Security Violation) | - |

**HRT Route_Tag 分配 (V3.1-Rev2.0 关键修正)**:

| HRT Entry `route_tag` (4-bit) | 目标 Backend | 物理介质归属 | v1.0 MVP |
|:------------------------------|:-------------|:------------|:---------|
| `0x0` | HBM-DMA | GPU Die 内 (HBM3e Stack) | ✅ |
| `0x1 ~ 0xE` | NIC-DMA (Port 0~N) | 外部 Scale-Up Switch (Peer GPU HBM **或** CXL Memory Pool) | ✅ |
| `0xF` | IO-DMA | Host CPU + NVMe (外部 PCIe 端点) | ❌ v1.1+ |

**关键约束 (per `21-soc-topology-mvp.md` §7.3 不变量 3)**:
> **HRT 中没有 `CXL_DIRECT` Route_Tag**——所有 CXL Memory Pool 访问统一通过 `0x1 ~ 0xE` (NIC-DMA Port N) 路由。
> GPU 侧 HRT 仅告诉 UDD "这个 GUPA 前缀应该发给哪个 Switch Port"；
> 具体目标是 **Peer GPU HBM** 还是 **CXL Memory Pool** 由**外部 Scale-Up Switch 的 PTE 路由表**决定，
> GPU 完全不感知。

**Scale-Up 路由域细分 (`0x1000 ~ 0x7FFF`)**:

| GUPA 高位 `[63:48]` | NIC-DMA Port | Switch 目标候选 |
|:--------------------|:-------------|:----------------|
| `0x1000 ~ 0x1FFF` | Port 0 | Peer GPU 0 HBM (默认) / CXL Pool A |
| `0x2000 ~ 0x2FFF` | Port 1 | Peer GPU 1 HBM / CXL Pool B |
| `0x3000 ~ 0x3FFF` | Port 2 | Peer GPU 2 HBM / CXL Pool C |
| `0x4000 ~ 0x4FFF` | Port 3 | Peer GPU 3 HBM / CXL Pool D |
| `0x5000 ~ 0x7FFF` | Port 4~E | 预留 v1.1+ (更多 GPU / CXL) |

> **重要边界**: GUPA 0x1000 表示 **"发往 NIC-DMA Port 0 的所有访存"**，**不**区分是 Peer GPU 0 HBM 还是 CXL Pool A。这两个目标都由外部 Scale-Up Switch 内部 PTE 区分 (per `21-fabric-switch-mvp.md` §8.5)。

### §4.1 CIU MMIO Aperture 地址映射 (基地址 0x00F0_0000)

| 偏移 | 大小 | 名称 | 描述 |
|------|------|------|------|
| `0x4000` | 4 B | `CIU_REG_HRT_CTRL` | HRT 控制 (Bit 0: SWAP_BIT) |
| `0x4004` | 4 B | `CIU_REG_HRT_STATUS` | HRT 状态 (Bit 0: SWAP_PENDING, Bit 1: UPDATE_ACK) |
| `0x4010` | 8 B | `CIU_REG_HRT_SHADOW_ADDR` | Shadow Bank 写入地址 (HRT Index) |
| `0x4018` | 4 B | `CIU_REG_HRT_SHADOW_DATA` | Shadow Bank 写入数据 (32-bit HRT Entry) |
| `0x4020` | 8 B | `CIU_REG_RCT[ctx].BASE` | Context ctx GUPA Base |
| `0x4028` | 8 B | `CIU_REG_RCT[ctx].LIMIT` | Context ctx GUPA Limit |
| `0x4030` | 4 B | `CIU_REG_RCT[ctx].CTRL` | Context ctx 控制 (enable + route_override) |
| `0x4100` | 8 B | `CIU_REG_PMU_HRT_LOOKUP` | PMU: HRT 查表总数 (RO) |
| `0x4108` | 8 B | `CIU_REG_PMU_HRT_MISS` | PMU: HRT Invalid 命中数 (RO) |
| `0x4110` | 8 B | `CIU_REG_PMU_NOC_INJECT` | PMU: NoC 注入 Micro-op 数 (RO) |
| `0x4118` | 8 B | `CIU_REG_PMU_STALL_TRACKER_FULL` | PMU: Tracker 满导致 Stall 数 (RO) |
| `0x4120` | 8 B | `CIU_REG_PMU_STALL_CREDIT_EMPTY` | PMU: Credit 耗尽导致 Stall 数 (RO) |
| `0x4128` | 8 B | `CIU_REG_PMU_SWAP_COUNT` | PMU: HRT Atomic Swap 触发次数 (RO) |

> **V3.1-Rev2.0 HRT Shadow Entry 32-bit 字段位分配** (per §4.0 GUPA Route_Tag 分配):
> ```
> [31:28] route_tag       (4 bits: 0:HBM, 1~E:UALink, F:PCIe)
> [27:24] vc_id           (4 bits Virtual Channel)
> [23:16] qos_priority    (8 bits QoS / Throttle_Group)
> [15:00] addr_offset     (16 bits 细粒度地址偏移/Mask)
> ```
> **v1.0 MVP 仅启用 route_tag=0 (HBM) + 1~4 (UALink Port 0~3)**。
> route_tag=F (PCIe IO) 推迟 v1.1+。
> route_tag=5~E 预留 v1.1+ (更多 Switch Port)。
> 详细位分配见 `21-microarch-ifc-mvp.md` §3.1 + `21-fabric-switch-mvp.md` §4.4。

### §4.2 HRT 控制寄存器 (`CIU_REG_HRT_CTRL`)

```cpp
struct CiuHrtCtrlReg {
    uint32_t swap_bit       : 1;   // Bit 0: 写 1 触发 Atomic Swap (脉冲型)
    uint32_t swap_pending   : 1;   // Bit 1: Swap 进行中 (RO, 1=busy)
    uint32_t update_ack     : 1;   // Bit 2: Swap 完成中断状态 (W1C)
    uint32_t reserved_3_31  : 29;
};
// MMIO write 0x4000 (value=0x1) → 触发 Swap, 拉高 swap_pending
// 等待 swap_pending == 0 (≤8 cycles) → update_ack 拉高, 触发 MSI-X
```

### §4.3 RCT 寄存器 (`CIU_REG_RCT[ctx]`)

```cpp
struct CiuRctBaseReg { uint64_t gupa_base : 64; };    // 64KB aligned
struct CiuRctLimitReg { uint64_t gupa_limit : 64; };
struct CiuRctCtrlReg {
    uint32_t enable          : 1;  // Context 启用
    uint32_t route_override  : 4;  // 强制覆写 Route_Tag (0xF = 不覆写)
    uint32_t reserved_5_31   : 27;
};
// v1.0 MVP: 仅 ctx=0 可用, 其他 Context 写入忽略
```

### §4.4 PMU 寄存器说明 (per MAS-3.1-UDD-MAS §7)

| PMU 事件 | 架构师关注点 |
|----------|--------------|
| `PMU_HRT_LOOKUP` | 评估访存密度 |
| `PMU_HRT_MISS` | 评估地址空间碎片或配置错误 |
| `PMU_NOC_INJECT` | 评估 NoC 带宽利用率 |
| `PMU_STALL_TRACKER_FULL` | 评估 Backend 延迟瓶颈 |
| `PMU_STALL_CREDIT_EMPTY` | 评估流控参数合理性 |
| `PMU_SWAP_COUNT` | 评估动态内存重配的频繁度 |

### §4.5 TEE-UDD 与其他模块的接口 (per [`21-microarch-ifc-mvp.md` §3](21-microarch-ifc-mvp.md))

| 接口 | 信号/端口 | 描述 |
|------|-----------|------|
| `SM → TEE` | `sm_tee_req_*` (128-bit payload) | SM LSU 描述符入口 (per MicroArch §3.1) |
| `TC-DMA → TEE` | `tc_tee_req_*` (160-bit payload) | TC-DMA Burst 描述符 (per MicroArch §3.2) |
| `TEE → UDD Agent` | `tee_udd_microop_*` (160-bit) | UDD Micro-op 注入 (per TEE §5.1) |
| `UDD Agent → Compute NoC` | NoC Flit (160-bit + parity) | NoC 入口注入 |
| `CIU → UDD Agent` | APB (128-bit) | HRT Shadow 写入 + Swap 触发 |
| `CIU → Global Hub` | APB (128-bit) | RCT 表写入 |

---

## §5 TEE Frontend 流水线

### §5.1 4 级流水线主流程

```
每个 clk_core 周期:
┌─────────────────────────────────────────────────────────────────┐
│  pipe_regs_[0] (Fetch)   : 拉取 1 个 TeeDescriptor              │
│  pipe_regs_[1] (Translate): 调 gmmu.translate(VA) → GUPA       │
│  pipe_regs_[2] (Route)    : HRT 查表 → Route_Tag + Chunking     │
│  pipe_regs_[3] (Dispatch) : 注入 UDD Agent                      │
└─────────────────────────────────────────────────────────────────┘
```

### §5.2 Fetch Stage

```
pipeline_fetch():
  if sm_desc_in->nb_read(desc):
    if context_id_check(desc.ctx_id):        // per §6.3 RCT 预校验
      pipe_regs_[Fetch] = desc
      pipe_valid_ = Translate
    else:
      emit_security_violation_trap(desc)     // AWT Trap type 0x2
      desc_ring_tail_++                       // 跳过该描述符
```

### §5.3 Translate Stage (per MAS-3.1-DMA-TEE §3.1.2)

```
pipeline_translate():
  desc = pipe_regs_[Translate]
  phys = 0
  rc = gmmu_translator->translate(desc.src_va, desc.size, phys)
  if rc == 0:
    // VA → GUPA 翻译成功
    if desc.dst_va != 0:
      rc2 = gmmu_translator->translate(desc.dst_va, desc.size, phys2)
      // Src + Dst 双域翻译
    pipe_regs_[Route].src_gupa = phys
    pipe_regs_[Route].dst_gupa = phys2 (if applicable)
  else if rc == -EFAULT:
    // Page Fault → 压入 Replay Buffer (per §7.1)
    push_replay_buffer(desc, FRONTEND_TRACKER)
    pipe_regs_[Translate].valid = 0  // 不占流水线
    // CIU 触发 PRI (v1.0 不实现, v2.0+ 引入)
  else:
    // 其他错误 → AWT Trap
    emit_awt_trap(...)
```

### §5.4 Route Stage (per MAS-3.1-DMA-TEE §3.1.3)

```
pipeline_route():
  desc = pipe_regs_[Route]
  // HRT 查表 (per UDD-MAS §3.2)
  src_entry = udd_agent->hrt_lookup(desc.src_gupa)
  // Transit 流量识别 (per MAS-3.1 §3.2)
  if is_transit_traffic(desc.src_gupa, desc.dst_gupa):
    // 旁路 GMMU 深度翻译 + 旁路 Midend 缓冲
    emit_transit_grant_microop(desc, src_entry)
    return
  // Chunking (per MAS-3.1 §4.1)
  chunk_descriptor(desc, chunked_microops)
  pipe_regs_[Dispatch].microops = chunked_microops
```

### §5.5 Dispatch Stage (per MAS-3.1-DMA-TEE §3.1.4)

```
pipeline_dispatch():
  for muop in pipe_regs_[Dispatch].microops:
    muop.tracker_id = alloc_tracker()
    muop.header.vc_id = hrt_entry.vc_id
    muop.header.qos = hrt_entry.qos_priority
    muop.header.ctx = desc.context_id
    udd_agent->inject(muop)
```

---

## §6 TEE Midend (AGU + DTE)

### §6.1 AGU (Address Generation Unit) — v1.0 MVP 简化

```
v1.0 MVP 限制:
- 仅 1D 步长 (v1.1+ 加 2D/3D)
- 无请求合并 (Coalescing 推迟 v1.1)
- Chunking 粒度固定 64B/128B/256B (per MAS-3.1 §4.1)

chunk_descriptor(desc, out):
  remaining = desc.size
  base_gupa = desc.src_gupa
  while remaining > 0:
    chunk_size = min(remaining, MAX_CHUNK)  // MAX_CHUNK = 256B
    muop.gupa = base_gupa
    muop.size = chunk_size
    muop.opcode = (desc.flags.is_write ? Write : Read)
    out.push(muop)
    base_gupa += chunk_size
    remaining -= chunk_size
```

### §6.2 DTE (Data Transform Engine) — v1.0 MVP 透传

```
v1.0 MVP 限制:
- 无格式转换 (FP32↔FP16/BF16 推迟 v1.1)
- 无 Memset/Mask (推迟 v1.1)
- 透传模式 = FIFO Pass-Through

DTE 模式控制:
  dte_mode_t = {Bypass, FmtConvert, Memset, Reduction}
  v1.0 MVP: 仅 Bypass 模式
```

### §6.3 RCT 预校验 (v1.0 MVP 加固)

```
context_id_check(ctx_id, gupa):
  if global_hub->rct_check(ctx_id, gupa) == false:
    return false   // 越权
  return true
```

**v1.0 MVP 必须实现**: 即使 v1.0 仅 1 Context, 也必须在 TEE Frontend Fetch 阶段就做预校验 (per 演进不变量 2)。

---

## §7 UDD Agent HRT 查表与原子切换

### §7.1 HRT Entry 格式 (per MAS-3.1-UDD-MAS §3.1)

```cpp
struct HrtEntry {
    uint8_t  route_tag;       // [31:28] 0:HBM, 1~E:UALink, F:PCIe
    uint8_t  vc_id;           // [27:24] Virtual Channel
    uint8_t  qos_priority;    // [23:16] QoS / Throttle_Group
    uint16_t addr_offset;     // [15:00] 细粒度地址偏移/Mask
};
static_assert(sizeof(HrtEntry) == 4, "HRT entry 必须紧凑以节省 SRAM");

// v1.0 MVP: HRT Index 由 GUPA[47:36] 提取 (per MAS-3.1-UDD-MAS §3.2)
// 4096 entries × 4 bytes = 16 KB per Bank
// 双 Bank (Active + Shadow) = 32 KB per UDD Agent
```

### §7.2 HRT 查表流水线

```
每个 clk_core 周期:
Cycle 1: LSU 生成 GUPA, 提取 GUPA[47:36] 作为 HRT Index
Cycle 2: SRAM Read (Active Bank), 输出 32-bit Entry
Cycle 3: Decode & Route, 解析 Route_Tag + VC_ID
```

### §7.3 HRT 原子切换状态机 (per MAS-3.1-UDD-MAS §3.3)

```
CIU 触发 SWAP_BIT → UDD Agent 状态机:

[ IDLE ]
  │ CIU 写 SWAP_BIT=1
  ▼
[ SHADOW_WRITE_DONE ]
  │ 拒绝新 LSU 请求 (拉低 sm_udd_req_ready)
  │ 等待当前 in-flight 请求完成 (≤3 cycles)
  ▼
[ BANK_TOGGLE ]
  │ clk_core 上升沿: Active/Shadow 指针原子翻转
  │ 拉高 swap_done 信号
  ▼
[ RESUME ]
  │ 恢复接收新请求, 使用新 Active Bank 查表
  │ 触发 MSI-X (CIU_REG_HRT_CTRL.update_ack = 1)
```

**延迟约束** (per MAS-3.1-UDD-MAS §3.3): 整个 Atomic Swap 过程导致的 SM Stall **不得超过 8 个 clk_core 周期**。

### §7.4 WRR 仲裁 (per MAS-3.1-Core §1.3)

```
arbiter_wr_r(req, tc_won, selected_gupa):
  if req.tc_valid && wrr_tc_credit_ > 0:
    tc_won = true
    selected_gupa = req.tc_gupa
    wrr_tc_credit_--
    if wrr_tc_credit_ == 0:
      wrr_sm_credit_ = 1
      wrr_tc_credit_ = 4
    return true
  if req.sm_valid && wrr_sm_credit_ > 0:
    tc_won = false
    selected_gupa = req.sm_gupa
    wrr_sm_credit_--
    if wrr_sm_credit_ == 0:
      wrr_sm_credit_ = 1
      wrr_tc_credit_ = 4
    return true
  return false  // 背压
```

---

## §8 Global UDD Hub 微架构

### §8.1 入口仲裁与 RCT 校验

```
每个 clk_fab 周期:
1. Flit Demultiplexer: 从 MAX_GPCS 入口 FIFO 选择最早 valid Micro-op
2. RCT 校验: 调用 rct_check(ctx_id, gupa)
   - 失败 → security_violation_count_++ + emit AWT Trap
   - 通过 → 继续
3. Backend Dispatcher: 按 Route_Tag 选择 backend_out[] 端口
4. Credit Acquire: credit_acquire(backend_id) 申请 1 Credit
   - 成功 → 注入 backend_out[backend_id]
   - 失败 → Stall 当前 GPC (避免 HOL Blocking)
```

### §8.2 Credit-Based 流控 (per MAS-3.1-Fabric §2.3)

```
v1.0 MVP 初始 Credit:
- GPC → Hub 通路: 64 Credits per GPC
- Hub → Backend 通路: 128 Credits per Backend (per Backend)

stall 行为:
- Backend Credit == 0 → 阻塞发往该 Backend 的 Micro-op (via VC 隔离)
- 不阻塞其他 Backend (Head-of-Line Blocking 避免, per MAS-3.1-UDD-MAS §6.2)
```

### §8.3 RCT 多租户隔离 (per MAS-3.1-UDD-MAS §4)

```
v1.0 MVP:
- Context_ID 8-bit (支持 256 个硬件隔离租户)
- MAX_CONTEXTS = 256
- 仅 ctx=0 启用 (其他 ctx 写入忽略)
- Base/Limit 校验: 检查 GUPA 是否落在 ctx 授权范围
- 越权 → Security Violation AWT Trap (type 0x2)
```

**v1.0 必须实现数据结构预留** (per 演进不变量 2): `MAX_CONTEXTS = 256` array 必须从 v1.0 起定义, v1.1 增加活跃 ctx 时不重构。

---

## §9 TC-DMA 集成特例

### §9.1 TC-DMA 与 TEE 关系

TC-DMA 是 SM 内部特例 (per MAS-3.1-DMA-TEE §8, MAS-3.1-Core §1.2):
- 复用 TEE Frontend (描述符解析) 和 Midend (AGU/DTE)
- **不**生成 UDD Micro-op, 而是生成 SMEM Read/Write 事务
- **不**经过 UDD Agent 与 NoC

### §9.2 v1.0 MVP TC-DMA 简化

```
v1.0 MVP TC-DMA 必须实现:
- 描述符解析 (复用 TEE Frontend Fetch)
- 5D Tensor 描述符格式 (per MAS-3.1-DMA-TEE §8.2)
- AGU 多维地址生成 (per Midend §6.1)
- Swizzle 引擎 (Bank Conflict-Free, per MAS-3.1 §8.2)
- mbarrier 硬件触发 (per MAS-3.1-DMA-TEE §8.3)
- **不实现**: 复杂数据变换 (DTE 简化 = 透传)
```

### §9.3 TC-DMA 与 SM 的仲裁

```
per MAS-3.1-Core §1.3:
- WRR 仲裁: TC-DMA 权重 = 4 (保障 Bulk 带宽)
- SM LSU 权重 = 1
- TC-DMA 完成 4 个请求后, 切换至 SM 1 个请求
```

---

## §10 TEE ↔ UDD ↔ UBC 接口契约 (per MAS-3.1-DMA-TEE §5)

### §10.1 UDD Micro-op 格式 (160-bit)

```cpp
struct UddMicroOp {
    uint32_t header;         // [159:128] Opcode, Size, VC_ID, QoS_Class, Context_ID
    uint64_t gupa;           // [127:064] GUPA
    uint32_t payload_meta;   // [63:032] Write Data / BE / Atomic Operand
    uint16_t tracker_id;     // [31:016] Tracker_ID
    uint16_t seq_route;      // [15:000] Sequence_Num + Route_Tag
};
```

### §10.2 UDD Response 格式 (64-bit)

```cpp
struct UddResponse {
    uint16_t tracker_id;     // [63:48] Tracker_ID (匹配原始请求)
    uint16_t status;         // [47:32] 0x0=Success, 0x1=Remote_Poison, 0x2=Local_Fault, 0x3=Page_Fault
    uint32_t data_meta;      // [31:00] Read Data (小负载) / Timestamp
};
```

### §10.3 流控与背压

```
v1.0 MVP 流控:
- Credit-Based: TEE 与 UDD Agent 之间按 VC 维护独立 Credit
- 1 Credit = 1 UDD Micro-op
- 全局背压: UDD Agent 拉低 sm_udd_req_ready / tc_udd_req_ready
- 仅 Stall 发往该 Route_Tag 的 Channel, 不影响其他 (per MAS-3.1-DMA-TEE §5.3)
```

---

## §11 RAS 与 Page Fault 处理

### §11.1 非阻塞 Page Fault (per MAS-3.1-DMA-TEE §7.1)

```
v1.0 MVP Page Fault 处理:
1. GMMU 返回 PAGE_FAULT (-EFAULT)
2. TEE Frontend.Translate 捕获, 压入 Replay Buffer (深度 256, per Frontend Tracker)
3. Channel 不挂起, 继续执行后续合法描述符
4. v1.0 不触发 PRI (per 演进路线图 v1.0 推迟项)
5. Driver 通过 polling GMMU_REG_STATUS.fault_valid 识别 fault
6. Host OS 调页后, 写 MMIO REPLAY_KICK 触发 TEE 从 Replay Buffer 恢复执行

Replay Buffer 实现: 复用 Frontend Tracker (per §3.1), 增加 fault_pending 标记
```

### §11.2 Poison 处理 (v1.0 MVP 仅 Local HBM)

```
v1.0 MVP Poison 来源:
- 仅 HBM-DMA 返回 Local HBM Poison (type 0x0)
- 远端 Poison (UALink / CXL) 推迟到 v2.0+

处理流程 (per MAS-3.1-DMA-TEE §7.2):
1. UDD Agent 从 UDD Response Flit 收到 Status=0x1 (Remote_Poison) 或 0x2 (Local_Fault)
2. 对于全局 DMA 任务: TEE 标记 Channel Fatal Error, 触发 MCE
3. 对于 TC-DMA 任务: TEE 标记 SMEM Bank Poison, 后续 Warp 读触发 AWT (type 0x0)
4. 记录到 TEE 内部 RAS 寄存器
```

### §11.3 L2 Cache 一致性协同 (v1.0 MVP 简化)

```
v1.0 MVP 简化:
- Bypass_L2 标志支持 (per MAS-3.1-DMA-TEE §7.3)
- v1.0 不实现 Invalidate/Snoop 请求 (L2 一致性协议由 L2 Cache 模块自行保证)
- v1.1+ 引入 snoop 协同
```

---

## §12 测试覆盖

### §12.1 单元测试 (`test_tee_*.cc`, `test_udd_*.cc`)

| # | 测试用例 | 覆盖能力 | 断言 |
|---|---------|---------|------|
| 1 | `TEE.Fetch.4-stage pipeline` | Frontend 流水线 | 4 描述符/4 cycles 全部进入 Dispatch |
| 2 | `TEE.Translate.GMMU L1 hit` | GMMU TLB hit 路径 | latency < 50% PTW |
| 3 | `TEE.Translate.GMMU L1 miss + PTW` | GMMU TLB miss 触发 PTW | 返回正确 GUPA |
| 4 | `TEE.Route.HRT lookup` | UDD HRT 查表 | Route_Tag 正确 |
| 5 | `TEE.Route.Transit bypass` | Transit 流量识别 | 不进入 Midend, 直接 Transit_Grant |
| 6 | `TEE.Midend.Chunking` | AGU 拆分 | 128B / 256B / 512B 拆分正确 |
| 7 | `UDD.HRT.bank toggle` | Atomic Swap | ≤8 cycles 完成 |
| 8 | `UDD.HRT.shadow write` | Shadow Bank 写入 | 不影响 Active Bank 流量 |
| 9 | `UDD.RCT.base/limit check` | RCT 校验 | 越权 → Security Violation |
| 10 | `UDD.RCT.route override` | RCT Route_Tag 覆写 | ctx=A → Port 0, ctx=B → Port 1 |
| 11 | `Hub.Credit.acquire/return` | 流控 | Credit 满 → Stall, Response → Return |
| 12 | `Hub.RCT.security violation` | AWT Trap 触发 | 越权 → security_violation_count_++ |
| 13 | `TC-DMA.mbarrier trigger` | 硬件同步 | 数据写入完成 → 下一个 clk_core 拉高 mbarrier |
| 14 | `TEE.PageFault.replay buffer` | Page Fault 处理 | Replay Buffer 深度 ≤256 |
| 15 | `PMU.counter accuracy` | PMU 计数 | lookup/miss/stall 计数正确 |

### §12.2 集成测试 (`test_tee_udd_hbm_e2e.cc`)

**核心 demo 测试** (per §2 端到端 shippable demo):
- 9 步全链路贯通 (SM → TEE → GMMU → UDD → HBM-DMA → HBM)
- 测试标签: `[tee][udd][hbm][e2e]`

### §12.3 跨仓集成测试

- UsrLinuxEmu 端 driver 配置 HRT + RCT
- CppTLM 端 TEE/UDD 处理请求
- 端到端 HBM 访存校验

### §12.4 Oracle 评审清单

- [ ] TEE 4 级流水线无气泡 (理想吞吐 = 1 描述符/cycle)
- [ ] HRT 原子切换 ≤8 cycles (per MAS-3.1-UDD-MAS §3.3)
- [ ] RCT 校验延迟 ≤3 cycles (RCT array 读 + compare)
- [ ] WRR 仲裁公平性 (TC-DMA : SM = 4:1)
- [ ] Page Fault Replay Buffer 深度 ≥256 (per MAS-3.1-DMA-TEE §7.1)
- [ ] TC-DMA mbarrier 触发延迟 ≤1 cycle (per MAS-3.1-DMA-TEE §8.3)
- [ ] 跨仓 ABI 影响: 0 个新 ABI 函数 (v1.0 MVP 严格遵守)
- [ ] 5 阶段演进路线图无债务 (per evolution-roadmap §5)

---

## §13 演进不变量 (4 条契约)

**详细定义见 [`21-tee-udd-evolution-roadmap.md` §5](21-tee-udd-evolution-roadmap.md)**。本节列出 v1.0 MVP 的具体实现要求。

### §13.1 不变量 1: GUPA 输出语义在演进中只增不换

```
v1.0 MVP: GUPA 输出格式固定 64-bit, Route_Tag 由 UDD HRT 查表附加
v3.0+ 扩展点: GUPA 可携带 tenant_id / mig_instance 等元数据 (元数据封装, 不破坏 GUPA 格式)
实现: UddMicroOp.gupa 字段固定 64-bit, 演进通过额外字段扩展
```

### §13.2 不变量 2: Context_ID / RCT 数据结构从 v1.0 起 extensible

```cpp
// v1.0 即定义 array + 控制寄存器, 预留扩展
static constexpr size_t MAX_CONTEXTS = 256;  // v1.0 仅 1 ctx 启用, 数据结构预留
std::array<RctEntry, MAX_CONTEXTS> rct_table_{};
// v1.1+ 加活跃 ctx 时仅追加数组项, 不重构
```

### §13.3 不变量 3: HRT Entry 格式从 v1.0 起包含所有 16 个 Route_Tag 槽位

```cpp
// v1.0 即定义 4-bit Route_Tag, 即使 v1.0 仅 Route_Tag=0 (HBM) 实际使用
struct HrtEntry {
    uint8_t route_tag : 4;  // 0:HBM, 1~E:UALink, F:PCIe (16 槽位预留)
    // ...
};
// v1.1+ 加 UALink 时仅扩展 HRT 路由表填充逻辑, 不重构 Entry 格式
```

### §13.4 不变量 4: HRT Atomic Swap 状态机从 v1.0 起按 Ping-Pong 双 Bank

```cpp
// v1.0 即定义 Active + Shadow 双 Bank, 即使 v1.0 不实现 Shadow 写入路径
enum class HrtActiveBank : uint8_t { BankA, BankB };
HrtActiveBank active_bank_ = HrtActiveBank::BankA;
std::array<HrtEntry, HRT_ENTRIES> hrt_bank_a_{};
std::array<HrtEntry, HRT_ENTRIES> hrt_bank_b_{};
// v1.1+ 加 CIU Shadow 写入路径时仅扩 FSM, 不重构 Bank 结构
```

---

## §14 边界与限制

### §14.1 不实现的功能 (v1.0 MVP 明确边界)

- ❌ **AGU 多维地址生成 (2D/3D)** — v1.0 仅 1D 步长, v1.1 加
- ❌ **DTE 格式转换 (FP32↔FP16/BF16/FP8)** — v1.0 透传, v1.1 加
- ❌ **DTE Memset/Mask/Reduction** — v1.0 不实现, v1.1 加
- ❌ **请求合并 (Coalescing)** — v1.0 不实现, v1.1 加
- ❌ **UALink / PCIe IO 路由** — v1.0 仅 HBM, v1.1+ 引入 (per backends 文档)
- ❌ **Multi-Context 实际使用** — v1.0 仅 1 ctx, 数据结构预留 v1.1+
- ❌ **PRI Page Request Interface** — v1.0 polling, v2.0+ 引入 (per gmmu-mvp §14.1)
- ❌ **Stage 2 (GUPA → GUPA+Route_Tag 拆分)** — v1.0 隐式, v3.0 显式
- ❌ **AWT Trap 完整处理** — v1.0 仅本地 MCE, v2.0+ 远端 AWT
- ❌ **HRT 多级 (L1 + L2)** — v1.0 仅 L1, v1.1 加 L2
- ❌ **Transit 旁路 (Remote A → Local A → Remote B)** — v1.0 识别但不优化, v2.0 加 Transit_Grant
- ❌ **TC-DMA 多流并发** — v1.0 单流, v1.1+ 多流

### §14.2 已知限制

- HRT 容量限制: 4096 entries per Bank, GUPA[47:36] 12-bit 索引
- 双 Bank SRAM: 32 KB per UDD Agent (8 GPC × 32 KB = 256 KB total)
- RCT 容量: 256 Contexts, 仅 1 active in v1.0
- Tracker 容量: 256 entries per UDD Agent (Replay Buffer 复用)
- Replay Buffer 满 → Stall (per MAS-3.1-DMA-TEE §7.1)
- Page Fault 处理: 仅 -EFAULT, 无 MSI-X 中断
- TC-DMA 复用 TEE IP, 但**不**经过 UDD Agent (per §9.1)
- Global UDD Hub 仅 8 GPC 入口 + 8 Backend 出口 (per §3.3)

---

## §15 跨仓契约

### §15.1 UsrLinuxEmu 侧 (不修改 23 ABI)

TEE-UDD v1.0 MVP **不引入任何新 CppTLM ABI 函数**——所有 driver↔GPU 通信通过**现有 23 ABI 函数** (`syms->mmio_write/read` + `cpptlm_emulator_register_dma_translate_cb`).

per UsrLinuxEmu AGENTS.md §CppTLM 通信架构 — **PCIe-only 原则**:
> 真实硬件不允许 driver 直接调 GPU 内部函数。所有通信必须通过 PCIe (MMIO + DMA)。

### §15.2 CppTLM 侧 (新模块 + 命名变更)

| 文件 | 状态 |
|------|------|
| `include/tlm/dma/tee_tlm.{hh,cc}` | **新建** (本模块) |
| `include/tlm/dma/udd_agent_tlm.{hh,cc}` | **新建** (本模块) |
| `include/tlm/dma/global_udd_hub_tlm.{hh,cc}` | **新建** (本模块) |
| `include/tlm/dma/ciu_tlm.{hh,cc}` | **新建** (本模块) |
| `include/tlm/gpu/sdma_engine_tlm.{hh,cc}` | **归档** (按 §1.3 命名变更) |
| `test/test_tee_*.cc` (5+ 用例) | **新建** |
| `test/test_udd_*.cc` (5+ 用例) | **新建** |
| `test/test_tee_udd_hbm_e2e.cc` | **新建** (端到端 demo) |
| `src/tlm/dma/tee_shell.cc` | **修改** (注入 TEE-UDD 拓扑) |
| `include/chstream_register.hh` | **修改** (注册 TeeTLM/UddAgentTLM/GlobalUddHubTLM/CIUTLM) |
| **23 ABI 头冻结** | ✅ **不变** (per ADR-088 §D5) |

### §15.3 跨仓 PR 协调

按 ADR-091 §R5.1 跨仓 PR 流程:
1. CppTLM 仓: 实现 TEE + UDD + CIU 模块 + 15+ 测试
2. UsrLinuxEmu 仓: driver 配置 HRT + RCT (由用户承担协调)
3. 跨仓集成测试: `test_tee_udd_hbm_e2e_ue.cc`
4. 同步 PR (无 ABI 影响, 跨仓风险低)

---

## §16 引用

### §16.1 内部引用

- [`21-tee-udd-evolution-roadmap.md`](21-tee-udd-evolution-roadmap.md) — TEE-UDD 5 阶段演进路线图 (SSOT for evolution)
- [`21-dma-backends-mvp.md`](21-dma-backends-mvp.md) — UBC + HBM/NIC/IO-DMA MVP (下游)
- [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) — Fabric 协议 + Scale-Up Switch (协议层)
- [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) — Core/Micro-Arch + RTL-IFC (微架构/接口)
- [`20-gmmu-mvp.md`](20-gmmu-mvp.md) — GMMU v1.0 MVP (翻译层协同)
- [`16-pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT
- [`00-overview.md`](00-overview.md) §3.1 L1 Host Interface + §3.7 L7 Memory

### §16.2 跨仓引用

- UsrLinuxEmu AGENTS.md §CppTLM 通信架构 — **PCIe-only 原则**
- UsrLinuxEmu ADR-088 §D3.8 — `cpptlm_dma_translate_cb` 契约
- UsrLinuxEmu ADR-088 §D5 — 23 ABI 冻结
- UsrLinuxEmu change `2026-09-19-cpptlm-dgpu-gmmu-mvp` — driver-side 协调

### §16.3 MAS-3.1 上游原始设计

- `MAS-3.1-DMA-TEE Rev2.0` (TEE 任务执行层原始设计)
- `MAS-3.1-UDD-MAS Rev2.0` (UDD 地址空间原始设计)
- `MAS-3.1-Fabric/Protocol Rev2.0` (Fabric 协议)
- `MAS-3.1-Core/Micro-Arch Rev2.0` (Core 微架构)

### §16.4 业界参考

- NVIDIA Hopper/Blackwell GPC 内部 DMA + Routing
- AMD CDNA Compute Unit + LDS 路径
- ARM CMN-700 NoC + HN-F (Home Node) 路由表
- Intel Xe Link + Fabric 协议

---

## §17 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: TEE + UDD v1.0 MVP 详细设计 (4 级流水线 + HRT 双 Bank 原子切换 + RCT 多租户隔离 + 4 条不变量 + 9 步 shippable demo + 15+ 测试用例 + SDMA→TeeTLM 重命名) |

---

**关联 OpenSpec change**: 待 `openspec/changes/2026-09-19-cpptlm-mas-tee-udd-mvp/` 提案创建
**下次更新**: Oracle 评审反馈后 v1.1