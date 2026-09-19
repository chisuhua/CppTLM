# DMA Backends v1.0 MVP 详细设计 (Unified Bridge Core + HBM/NIC/IO Protocol Engines — Minimum Viable Product)

> **目的**: 定义 CppTLM dGPU SoC **MAS-3.1** 中 **Backends 层** (UBC 共享基座 + HBM/NIC/IO 协议引擎) 的 v1.0 MVP 微架构——作为 dGPU 数据搬运的**后端物理实现层**，与上游 [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) (TEE-UDD 数据面) 通过 160-bit UDD Micro-op 标准接口解耦。v1.0 MVP 仅交付 **HBM-DMA + NIC-DMA (UALink 1.1) 两种 Backend**；IO-DMA (PCIe) 在 v1.1 引入（与 [fabric-switch-mvp] v1.1 协同）。
>
> **状态**: Draft v1.0 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: 待 `openspec/changes/2026-09-19-cpptlm-mas-dma-backends-mvp/` 提案对齐
> **关联文档**:
> - [`21-dma-backends-evolution-roadmap.md`](21-dma-backends-evolution-roadmap.md) — Backends 5 阶段演进路线图 (v1.0/v1.1/v2.0/v2.1/v3.0)
> - [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) — TEE + UDD MVP (本模块上游, 通过 UDD Micro-op 接口对接)
> - [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) — Fabric 协议 + Scale-Up Switch (本模块 NIC-DMA 协议对端)
> - [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) — Core/Micro-Arch + RTL-IFC (本模块微架构/接口)
> - [`21-soc-topology-mvp.md`](21-soc-topology-mvp.md) — SoC 顶层物理布局规范 (V3.1-Rev2.0 拓扑修正: NIC-DMA 在 GPU Die / CXL Memory Pool 在外部 Switch 下 / HRT Route_Tag 分配)
> - [`16-pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT
> **关联 ADR**:
> - ADR-088 §D5 — 23 ABI 冻结 (Backends v1.0 不动 ABI)
> - ADR-SOC-10 (待起草) — TEE/UDD/UBC 三层解耦 (本模块边界)
> - ADR-SOC-21 — **V3.1-Rev2.0 拓扑修正** (NIC-DMA 是 GPU 与外部 Switch 唯一接口)

---

## §1 概述

### §1.1 Backends 层在 dGPU SoC 中的位置

Backends 层是 dGPU SoC 数据搬运的**后端物理实现**，位于 UDD (统一数据分发) 与物理介质 (HBM/UALink/PCIe) 之间：

```
┌─────────────────────────────────────────────────────────────────┐
│                       GPU 内部 (dGPU SoC)                        │
│                                                                 │
│         [TEE-UDD 数据面] (见 tee-udd 文档)                      │
│                       │                                         │
│                       │ 160-bit UDD Micro-op                   │
│                       ▼                                         │
│  ╔═══════════════════════════════════════════════════════════╗  │
│  ║              ★ Backends 层 (本模块)                       ║  │
│  ║                                                           ║  │
│  ║  ┌─────────────────────────────────────────────────────┐  ║  │
│  ║  │     UBC × Backend (共享基座 + 协议引擎)              │  ║  │
│  ║  │                                                     │  ║  │
│  ║  │  [UBC]──[HBM-DMA]      (HBM3e Controller Engine)   │  ║  │
│  ║  │  [UBC]──[NIC-DMA]      (UALink 1.1 Scale-Up)       │  ║  │
│  ║  │  [UBC]──[IO-DMA]       (PCIe Gen6 + ATS/PRI)  v1.1 │  ║  │
│  ║  │                                                     │  ║  │
│  ║  │  UBC 共享: ROB + QoS + RAS Injector (本模块核心)  │  ║  │
│  ║  └─────────────────────────────────────────────────────┘  ║  │
│  ╚═══════════════════════════════════════════════════════════╝  │
│                       │                                         │
└──────────────────────┼──────────────────────────────────────────┘
                       │
                       ▼ 物理介质
         [HBM3e Stacks]    [UALink Switch]    [PCIe Gen6 to Host]
```

### §1.2 为什么需要 UBC 共享基座 (替代 V3.0 独立 DMA)

| 维度 | V3.0 独立 DMA | UBC + 协议引擎 v1.0 MVP (新) |
|------|----------------|------------------------------|
| **NoC 接口** | 每个 DMA 独立实现 | UBC 共享 NoC Flit Terminator (CRC + VC 解复用) |
| **Tracker / ROB** | 每个 DMA 独立实现 | UBC 共享 1024-entry Tracker |
| **QoS 仲裁** | 每个 DMA 独立实现 | UBC 共享 QoS Arbiter + Token Bucket |
| **RAS 注入** | 每个 DMA 独立实现 | UBC 共享 Unified RAS Injector (标准化 Status Code) |
| **面积** | 三个独立 DMA 控制器 | UBC 复用 ~40% NoC 接口/Tracker 逻辑, **整体 Backend 面积减少约 12%** |
| **验证方法** | 三个 DMA 各自写 UVM | UBC UVM 一次开发 + 三种 Protocol Engine VIP 复用 |
| **RTL 复用** | 无复用 | UBC 单份代码 + HBM/NIC/IO 三种 Protocol Engine |

**核心设计原则** (per MAS-3.1-DMA-Backends Rev2.0):
> **UBC = Backend 共享微架构基座 (NoC 接口 + Tracker + QoS + RAS)**
> **Protocol Engine = Backend 物理协议实现 (HBM3e / UALink / PCIe)**
> **UBC 与 Protocol Engine 之间通过标准异步 FIFO (CDC) 隔离**

### §1.3 命名约定 (per MAS-3.1 Rev2.0)

| 旧名 (V3.0) | 新名 (V3.1-Rev2.0) | 原因 |
|--------------|---------------------|------|
| `NicDmaTLM` | `NicDmaUBC` (UBC + NIC Protocol Engine) | 强调"UBC 共享基座 + UALink 协议引擎" |
| `IoDmaTLM` | `IoDmaUBC` (v1.1 引入) | 同上, IO-DMA 在 v1.1 交付 |
| `HbmDmaTLM` | `HbmDmaUBC` | 同上 |
| **新增** | `UbcBaseTLM` (UBC 共享基座基类) | 共享基座模板 |
| **新增** | `UbcHbmEngineTLM` | HBM Protocol Engine 子模块 |
| **新增** | `UbcNicEngineTLM` | UALink Protocol Engine 子模块 |
| **新增** | `UbcIoEngineTLM` (v1.1) | PCIe Protocol Engine 子模块 |
| `PcieEndpointTLM` (旧) | `PcieEndpointIP` (V3.1, 4 端口冻结) | Phase 7.A 已 deprecated (per `19-pcie-ip-microarchitecture.md`) |

**重要边界**: 命名变更**不**影响跨仓 ABI 签名, 仅影响仓内代码可读性 + 与 MAS-3.1 Rev2.0 的概念对齐。

### §1.4 与 [TEE-UDD / Fabric-Switch / MicroArch-IFC] 文档的边界

| 子系统 | 文档 | 与本模块边界 |
|--------|------|---------------|
| **TEE + UDD** | [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) | 上游: 通过 160-bit UDD Micro-op 接口向本模块注入请求 |
| **Fabric 协议 + Switch** | [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) | NIC-DMA 协议对端 (UALink Flit 经 Switch 转换至 CXL) |
| **MicroArch + RTL-IFC** | [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) | 时钟域 / 流水线 / 信号定义 / CSR |

---

## §2 Backends v1.0 MVP 端到端 Shippable Demo

v1.0 MVP 必须通过的"绿灯测试"，证明 **TEE-UDD → UBC → HBM-DMA/NIC-DMA → HBM/UALink Switch** 链路贯通（v1.0 仅 HBM + NIC 两种 Backend，IO-DMA 在 v1.1 引入）：

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

**验收标准**: 12 步全部通过 + 测试用例 ≥ 10 个 (见 §12 测试覆盖)。

---

## §3 UBC 共享基座模块类定义

### §3.1 UBC 基类签名

```cpp
// include/tlm/dma/ubc_base_tlm.hh
namespace cpptlm::dma {

// UDD Micro-op (per tee-udd-mvp §10.1)
struct UddMicroOp {
    uint32_t header;         // [159:128] Opcode, Size, VC_ID, QoS, Context_ID
    uint64_t gupa;           // [127:064] GUPA
    uint32_t payload_meta;   // [63:032] Write Data / BE / Atomic Operand
    uint16_t tracker_id;     // [31:016] Tracker_ID
    uint16_t seq_route;      // [15:000] Sequence_Num + Route_Tag
};

// UDD Response (per tee-udd-mvp §10.2)
struct UddResponse {
    uint16_t tracker_id;     // [63:48] Tracker_ID
    uint16_t status;         // [47:32] Status Code
    uint32_t data_meta;      // [31:00] Data / Timestamp
};

// UBC Status Code (标准化, per MAS-3.1-DMA-Backends §5)
enum class UbcStatusCode : uint8_t {
    Success = 0x0,
    LocalHbmPoison = 0x0,    // HBM-DMA: DED 不可纠正 (per MAS-3.1-DMA-Backends §2.2)
    RemoteCxlPoison = 0x1,   // NIC-DMA: Switch 返回 Remote_Error
    LocalFault = 0x2,        // HBM-DMA: Row Retirement / NIC-DMA: Drain Timeout
    PageFault = 0x3,         // HBM-DMA: TLB Miss (per GMMU 协同)
    HostIoFault = 0x4        // IO-DMA: PCIe Link Down / UR (v1.1+)
};

// UBC Tracker Entry (1024-entry ROB, per MAS-3.1-DMA-Backends §1.1.2)
struct UbcTrackerEntry {
    uint16_t tracker_id;
    uint16_t source_gpc_id;
    uint16_t warp_id;
    uint8_t  context_id;
    uint64_t gupa;
    uint64_t timestamp;
    uint8_t  backend_id;     // 0:HBM, 1~E:NIC, F:IO (per HRT Route_Tag)
    uint8_t  valid;
    uint8_t  poison_flag;
};

class UbcBaseTLM : public sc_module {
public:
    SC_HAS_PROCESS(UbcBaseTLM);

    // 上游: Global UDD Hub 注入的 Micro-op (per tee-udd-mvp §3.3)
    sc_port<udd_microop_if> udd_hub_in;

    // 下游: Protocol Engine (派生类实现)
    // 跨时钟域异步 (UBC: clk_core 2.0GHz / Protocol Engine: clk_io 1.0~1.2GHz)
    sc_port<protocol_engine_req_if> engine_req_out;
    sc_port<protocol_engine_resp_if> engine_resp_in;

    UbcBaseTLM(sc_module_name name, uint8_t backend_id);
    ~UbcBaseTLM() override = default;

    // 主 tick (clk_core 域)
    void tick();
    // 跨时钟域处理 (异步 FIFO CDC)
    void engine_tick();  // clk_io 域

    // 标准化 RAS 注入 (per §7)
    UbcStatusCode inject_ras_status(uint16_t tracker_id, UbcStatusCode code);

protected:
    // ROB Tracker (per MAS-3.1-DMA-Backends §1.1.2)
    static constexpr size_t ROB_ENTRIES = 1024;
    std::array<UbcTrackerEntry, ROB_ENTRIES> rob_{};
    size_t rob_alloc_idx_ = 0;
    uint16_t next_tracker_id_ = 0;

    // QoS Token Bucket (per MAS-3.1-DMA-Backends §1.1.3)
    struct TokenBucket {
        uint32_t tokens;
        uint32_t rate;        // tokens / cycle
        uint32_t burst;       // max tokens
        uint8_t  priority;    // 0=highest, 255=lowest
    };
    TokenBucket qos_buckets_[8] = {};  // 8 QoS class

    // 跨时钟域 FIFO (clk_core → clk_io)
    sc_fifo<UddMicroOp> req_cdc_fifo_{"req_cdc", 64};
    sc_fifo<UddResponse> resp_cdc_fifo_{"resp_cdc", 64};

    // Unified RAS Injector (per §7)
    uint64_t poison_inject_count_ = 0;
    uint64_t local_fault_count_ = 0;
    uint64_t remote_error_count_ = 0;

    // PMU 计数器
    uint64_t pmu_total_requests_ = 0;
    uint64_t pmu_completed_requests_ = 0;
    uint64_t pmu_rohit_hits_ = 0;        // (HBM-DMA) Row Buffer Hit
    uint64_t pmu_qos_throttled_ = 0;     // QoS Token Bucket 限速计数
};

// 派生类: HBM/NIC/IO-DMA UBC (组合 UBC 共享基座 + 协议引擎)
class HbmDmaUbcTLM : public UbcBaseTLM {
    // 添加 HBM Protocol Engine (FR-FCFS + Bank Manager + ECC)
};

class NicDmaUbcTLM : public UbcBaseTLM {
    // 添加 UALink Protocol Engine (Flit Packager + Retry/Replay + Drain FSM)
};

class IoDmaUbcTLM : public UbcBaseTLM {  // v1.1+ 引入
    // 添加 PCIe Protocol Engine (TLP Generator + ATS/PRI + GDS)
};

}  // namespace cpptlm::dma
```

### §3.2 HBM Protocol Engine 子模块

```cpp
// include/tlm/dma/ubc_hbm_engine_tlm.hh
namespace cpptlm::dma {

// HBM Bank Manager 状态机 (per MAS-3.1-DMA-Backends §2.1.1)
enum class HbmBankState : uint8_t {
    IDLE, ACTIVE, PRECHARGING, REFRESHING
};

// HBM Pseudo-Channel (16 PC × 16 BG/Bank/Row/Col)
struct HbmAddressDecoded {
    uint8_t pc;
    uint8_t bg;
    uint8_t bank;
    uint32_t row;
    uint16_t col;
};

class UbcHbmEngineTLM : public sc_module {
public:
    SC_HAS_PROCESS(UbcHbmEngineTLM);

    // 上游: UBC 基座 req_cdc_fifo
    sc_port<protocol_engine_req_if> ubc_req_in;
    // 下游: HBM3e PHY 命令接口
    sc_port<hbm_phy_cmd_if> hbm_phy_out;

    UbcHbmEngineTLM(sc_module_name name);
    ~UbcHbmEngineTLM() override = default;

    // 主 tick (clk_fab 域 1.2GHz, per MAS-3.1-DMA-Backends §6.3)
    void tick();

    // GUPA 解码 (per MAS-3.1-DMA-Backends §2.1.1)
    HbmAddressDecoded decode_gupa(uint64_t gupa);

    // FR-FCFS 调度 (per MAS-3.1-DMA-Backends §2.1.2)
    void fr_fcfs_scheduler();

    // Refresh & ZQ Calibration (per MAS-3.1-DMA-Backends §2.1.3)
    void refresh_manager();

private:
    // 16 PC × 16 Bank 状态机
    std::array<std::array<HbmBankState, 16>, 16> bank_states_{};

    // FR-FCFS 队列
    static constexpr size_t READ_QUEUE_DEPTH = 256;
    static constexpr size_t WRITE_QUEUE_DEPTH = 256;
    std::array<UddMicroOp, READ_QUEUE_DEPTH> read_queue_{};
    std::array<UddMicroOp, WRITE_QUEUE_DEPTH> write_queue_{};
    size_t read_head_ = 0, read_tail_ = 0, read_count_ = 0;
    size_t write_head_ = 0, write_tail_ = 0, write_count_ = 0;

    // Open Row Table (per-bank, 用于 First-Ready 调度)
    std::array<uint32_t, 16> open_row_per_bank_{};  // PC × Bank
    uint32_t open_pc_per_bank_[16] = {};

    // Read/Write 切换阈值 (per MAS-3.1-DMA-Backends §2.1.2: 75%)
    static constexpr size_t WRITE_DRAIN_THRESHOLD = WRITE_QUEUE_DEPTH * 75 / 100;

    // Refresh Timer (ABR + PBR)
    uint64_t t_abr_ = 0;       // All-Bank Refresh 周期 (7.8 us per HBM3e spec)
    uint64_t t_pbr_ = 0;       // Per-Bank Refresh 周期
    uint64_t t_zq_ = 0;        // ZQ Calibration 周期

    // Page Retirement (per MAS-3.1-DMA-Backends §2.2)
    struct RetiredRow {
        uint32_t row;
        uint8_t  pc;
        uint8_t  bank;
        uint64_t ce_count;  // 可纠正错误计数 (阈值 3)
        bool     retired;
    };
    std::array<RetiredRow, 64> retired_rows_{};

    // ECC (SECDED)
    uint64_t sec_count_ = 0;   // 单比特纠错计数
    uint64_t ded_count_ = 0;   // 双比特检错计数
};

}  // namespace cpptlm::dma
```

### §3.3 NIC Protocol Engine 子模块

```cpp
// include/tlm/dma/ubc_nic_engine_tlm.hh
namespace cpptlm::dma {

// UALink Flit Format (256B, per MAS-3.1-Fabric §3.1)
struct UALinkFlit {
    uint32_t header;          // [2047:2016] Valid, SeqNum, SrcNode, DstNode, VC, MsgType
    uint32_t addr_tag_status; // [2015:1984] Address / Tag / Status
    uint8_t  payload[1984/8]; // [1983:0]    Payload
};

// UALink Message Type (per MAS-3.1-Fabric §3.2)
enum class UALinkMsgType : uint8_t {
    MEM_RD_REQ = 0x1,
    MEM_WR_REQ = 0x2,
    MEM_RESP_D = 0x3,
    MEM_RESP_ND = 0x4,
    CTRL_QUIESCE = 0x5,
    CTRL_Q_ACK = 0x6,
    CTRL_RESUME = 0x7
};

class UbcNicEngineTLM : public sc_module {
public:
    SC_HAS_PROCESS(UbcNicEngineTLM);

    // 上游: UBC 基座 req_cdc_fifo
    sc_port<protocol_engine_req_if> ubc_req_in;
    // 下游: UALink PHY (SerDes)
    sc_port<ualink_phy_if> ualink_phy_out;

    UbcNicEngineTLM(sc_module_name name);
    ~UbcNicEngineTLM() override = default;

    // 主 tick (clk_io 域 1.0GHz, per MAS-3.1-DMA-Backends §6.3)
    void tick();

    // UALink Flit 封装 (per MAS-3.1-Fabric §3.1)
    UALinkFlit pack_microop_to_flit(const UddMicroOp& muop);

    // Retry/Replay (Go-Back-N, per MAS-3.1-DMA-Backends §3.1.2)
    void retry_replay_engine();

    // 两阶段 Drain FSM (per MAS-3.1-DMA-Backends §3.2)
    void drain_fsm_tick();

private:
    // Outstanding Tracker / Reorder Buffer (1024-entry, per MAS-3.1-DMA-Backends §3.1.2)
    static constexpr size_t ROB_ENTRIES = 1024;
    std::array<UbcTrackerEntry, ROB_ENTRIES> rob_{};
    size_t rob_alloc_idx_ = 0;

    // Tx Replay Buffer (256 Flits, per MAS-3.1-DMA-Backends §3.1.2)
    static constexpr size_t REPLAY_BUFFER_ENTRIES = 256;
    std::array<UALinkFlit, REPLAY_BUFFER_ENTRIES> tx_replay_buffer_{};
    size_t replay_head_ = 0;
    size_t replay_tail_ = 0;

    // Sliding Window (Selective Repeat, per MAS-3.1-DMA-Backends §3.1.2)
    static constexpr size_t WINDOW_SIZE = 64;
    uint16_t window_base_ = 0;
    uint16_t window_next_seq_ = 0;
    std::array<bool, WINDOW_SIZE> window_acked_{};

    // P2P Bypass Path (per MAS-3.1-DMA-Backends §3.1.3)
    bool p2p_bypass_active_ = false;

    // Drain FSM (per MAS-3.1-DMA-Backends §3.2)
    enum class DrainFsmState : uint8_t {
        IDLE, BLOCK_NEW, WAIT_SWITCH, DRAIN_LOCAL
    };
    DrainFsmState drain_state_ = DrainFsmState::IDLE;
    uint8_t drain_target_port_ = 0;
    uint64_t drain_timeout_counter_ = 0;  // 100us @ clk_io = 1.0GHz = 100k cycles

    // Remote Poison Mapper (per MAS-3.1-DMA-Backends §3.1.3)
    uint64_t remote_poison_count_ = 0;
    uint64_t cxl_poison_count_ = 0;
};

}  // namespace cpptlm::dma
```

### §3.4 IO Protocol Engine 子模块 (v1.1 引入)

> **v1.0 MVP 不交付 IO-DMA**, 数据结构预留, v1.1+ 实现 (per evolution-roadmap §4.1)。

```cpp
// include/tlm/dma/ubc_io_engine_tlm.hh
namespace cpptlm::dma {

// v1.0 MVP 仅预留接口, 实现推迟到 v1.1
class UbcIoEngineTLM : public UbcBaseTLM {
    // v1.1 加: TLP Generator/Parser
    // v1.1 加: ATS/PRI Engine
    // v1.1 加: GDS Zero-Copy Bypass Path
};

}  // namespace cpptlm::dma
```

---

## §4 UBC 共享子模块微架构

### §4.1 NoC Flit Terminator (per MAS-3.1-DMA-Backends §1.1.1)

```
每个 clk_core 周期:
1. 从 udd_hub_in 端口读取 1 个 UDD Micro-op
2. CRC 校验 (per UDD-MAS §5 标准化)
   - 校验失败 → 直接丢弃 + 记录 CRC 错误计数
3. VC 解复用 (header.vc_id)
   - VC0 = Control/Drain (per MAS-3.1-Fabric §3.3)
   - VC1 = Read Request
   - VC2 = Write Request
   - VC3 = Response
4. 分配 Tracker_ID (从 1024-entry ROB 池)
5. 写入 UbcTrackerEntry{} 到 rob_[tracker_id]
6. 注入 req_cdc_fifo_ (跨 clk_core → clk_io 域)
```

### §4.2 Outstanding Tracker / ROB (per MAS-3.1-DMA-Backends §1.1.2)

```
tracker 分配:
- rob_alloc_idx_ 单调递增
- next_tracker_id_ = (next_tracker_id_ + 1) % 1024
- rob_[tracker_id].valid = 1
- rob_[tracker_id].source_gpc_id = header.source_gpc
- rob_[tracker_id].warp_id = header.warp_id
- rob_[tracker_id].context_id = header.context_id
- rob_[tracker_id].gupa = muop.gupa
- rob_[tracker_id].timestamp = 当前 cycle
- rob_[tracker_id].backend_id = Route_Tag (header 解码)

tracker 释放:
- Protocol Engine 返回 Completion
- 通过 Tracker_ID 匹配
- 组装 UddResponse{} (tracker_id + status + data)
- rob_[tracker_id].valid = 0
- 注入 resp_cdc_fifo_ (跨 clk_io → clk_core 域)
- pmu_completed_requests_++
```

### §4.3 QoS Arbiter & Shaper (per MAS-3.1-DMA-Backends §1.1.3)

```
Token Bucket 限速 (per QoS class):
qos_buckets_[priority]:
  tokens_ += rate_  // 每 cycle 累加 rate
  if tokens_ > burst_: tokens_ = burst_  // 限制上限
  if tokens_ > 0:
    调度 1 个 Request 到 Protocol Engine
    tokens_--
    pmu_qos_throttled_ 不增
  else:
    Stall 该 QoS class Request
    pmu_qos_throttled_++

带宽分配:
- High Priority (TC-DMA): rate=4 tokens/cycle, burst=8
- Normal Priority (SM LSU): rate=2 tokens/cycle, burst=4
- Low Priority (Bulk Streaming): rate=1 tokens/cycle, burst=2
```

### §4.4 Unified RAS Injector (per MAS-3.1-DMA-Backends §1.1.4)

```
Protocol Engine 上报错误 → 标准化为 UDD Status Code:

HBM-DMA:
  - HBM ECC DED → Status=0x0 (Local_HBM_Poison)
  - HBM Row Retirement → Status=0x0 (Local_HBM_Poison)
NIC-DMA:
  - UALink Link Down → Status=0x1 (Remote_CXL_Poison, per MAS-3.1-DMA-Backends §3.2)
  - Switch CXL Poison → Status=0x1 (Remote_CXL_Poison)
  - Drain Timeout → Status=0x2 (Local_Fault)
IO-DMA (v1.1+):
  - PCIe Link Down / UR → Status=0x4 (Host_IO_Fault)
  - ATS Translation Fault → 不生成 Poison, 触发 PRI (per MAS-3.1-DMA-Backends §4.1)
```

---

## §5 HBM-DMA 协议引擎微架构

### §5.1 Address Decoder & Bank Manager (per MAS-3.1-DMA-Backends §2.1.1)

```cpp
HbmAddressDecoded UbcHbmEngineTLM::decode_gupa(uint64_t gupa) {
    HbmAddressDecoded addr;
    // GUPA[63:48] = Local HBM 前缀 (per tee-udd-mvp §5.1 HRT 查表保证)
    addr.pc   = (gupa >> 40) & 0xF;    // 4-bit Pseudo-Channel (16 PC)
    addr.bg   = (gupa >> 36) & 0x3;    // 2-bit Bank Group (4 BG)
    addr.bank = (gupa >> 32) & 0x3;    // 2-bit Bank (4 Bank per BG)
    addr.row  = (gupa >> 16) & 0xFFFF; // 16-bit Row
    addr.col  = gupa & 0xFFFF;         // 16-bit Column
    return addr;
}

void update_bank_state(uint8_t pc, uint8_t bg, uint8_t bank, HbmBankState new_state) {
    bank_states_[pc * 4 + bg][bank] = new_state;
}
```

### §5.2 FR-FCFS Scheduler (per MAS-3.1-DMA-Backends §2.1.2)

```
fr_fcfs_scheduler():
  1. 优先检查 Write Drain:
     if write_count_ > WRITE_DRAIN_THRESHOLD (75%):
       切换总线方向 (Write Mode)
       调度 1 个 Write 请求
       return
  2. 优先调度 Row Hit:
     遍历 read_queue_ + write_queue_:
       if open_row_per_bank[bank] == request.row:
         调度该请求 (First-Ready)
         update open_row_per_bank[bank] = request.row
         return
  3. FCFS 调度:
     调度队列头部请求 (无 Row Hit 时)
     update open_row_per_bank[bank] = request.row
```

### §5.3 Refresh & ZQ Calibration Manager (per MAS-3.1-DMA-Backends §2.1.3)

```
refresh_manager():
  - t_abr_ 7.8us 触发 All-Bank Refresh (ABR)
    - 所有 Bank 进入 REFRESHING 状态
    - UBC QoS Arbiter 暂缓下发新请求
  - t_pbr_ 1.95us 触发 Per-Bank Refresh (PBR)
    - 仅单个 Bank 进入 REFRESHING 状态
  - t_zq_ 100ms 触发 ZQ Calibration
    - 调整 HBM I/O 阻抗
```

### §5.4 ECC & Page Retirement (per MAS-3.1-DMA-Backends §2.2)

```
ECC 处理 (Inline SECDED):
  - 每 256B 数据附带 32B ECC
  - SEC (Single-bit Error Correct):
    - 修正单比特错误 + sec_count_++
    - 透传修正后数据
  - DED (Double-bit Error Detect):
    - 不可纠正 + ded_count_++
    - 立即向 UBC ROB 写入 Status=0x0 (Local_HBM_Poison)

Page Retirement:
  - 同一 Row 发生 ≥ 3 次 SEC (ce_count_ >= 3)
    - 标记该 Row 为 retired_rows_[i].retired = true
    - 后续访问直接返回 Status=0x0 (Local_HBM_Poison)
    - 触发 SMI (System Management Interrupt) 通知 Host driver
    - driver 进行逻辑页替换 (物理页退役)
```

---

## §6 NIC-DMA 协议引擎微架构 (UALink 1.1)

### §6.1 UALink Flit Packager/Parser (per MAS-3.1-DMA-Backends §3.1.1)

```
pack_microop_to_flit(muop):
  flit.header = {
    Valid=1, SeqNum=window_next_seq_,
    SrcNode=gpu_node_id, DstNode=decode_from_gupa(muop.gupa),
    VC=muop.header.vc_id, MsgType=MEM_RD_REQ (Read) / MEM_WR_REQ (Write)
  }
  flit.addr_tag_status = { GUPA=muop.gupa, Tag=muop.tracker_id }
  if Write:
    flit.payload = muop.payload_meta (256B payload)
  window_next_seq_++ (if < WINDOW_SIZE)
  return flit

parse_response_flit(flit):
  if flit.header.MsgType == MEM_RESP_D:
    response.tracker_id = flit.addr_tag_status.Tag
    response.status = decode_status(flit.addr_tag_status.Status)
    response.data_meta = flit.payload[0..32] (256B data)
  else if MSG_TYPE == CTRL_Q_ACK:
    drain_state_ = DRAIN_LOCAL (Drain 阶段2)
```

### §6.2 Retry & Replay Engine (per MAS-3.1-DMA-Backends §3.1.2)

```
retry_replay_engine():
  - 维护 Go-Back-N 滑动窗口 (WINDOW_SIZE = 64)
  - window_base_ 指向最早未确认 SeqNum
  - window_next_seq_ 指向下一个可分配 SeqNum
  - 收到 ACK (SeqNum=K):
    advance window_base_ = K+1
    release tx_replay_buffer_ entries ≤ K
  - 收到 NAK (SeqNum=K):
    Go-Back-N 重传: 从 K 开始重传所有未 ACK entries
    pmu_replay_count_++
  - Timeout (T_retry = 10us @ clk_io 1.0GHz = 10k cycles):
    Force Replay from window_base_
    pmu_timeout_replay_count_++
```

### §6.3 P2P Bypass Path (per MAS-3.1-DMA-Backends §3.1.3)

```
P2P 旁路触发条件:
  - 收到来自 Switch 的 UALink Flit
  - flit.header.DstNode == gpu_node_id (本节点)
  - 解码 flit.addr_tag_status.GUPA → 落在 Local HBM 范围 (per HRT Route_Tag=0)
  → p2p_bypass_active_ = true
  → 不经过 UBC + Memory NoC
  → 直接通过内部旁路总线写入 HBM-DMA Write Queue
  → 降低 15% P2P 延迟 (per MAS-3.1-DMA-Backends §3.1.3)
```

### §6.4 两阶段 Drain FSM (per MAS-3.1-DMA-Backends §3.2)

```
drain_fsm_tick():
  [IDLE]
    if FM 写入 NIC_DRAIN_CTRL.START_DRAIN:
      drain_state_ = BLOCK_NEW
      drain_target_port_ = NIC_DRAIN_CTRL.target_port
      drain_timeout_counter_ = 0

  [BLOCK_NEW]
    - UBC 停止接收新 Tx Micro-op, 返回 Abort 给上游
    - 向 Switch 发送 CTRL_QUIESCE (UALink Control VC)
    - drain_timeout_counter_ 累加
    if timeout_counter_ > 100us @ clk_io = 100k cycles:
      触发 Fatal RAS 中断 + 允许 FM 写 FORCE_ABORT 强制清零 ROB

  [WAIT_SWITCH]
    - 等待 Switch 返回 CTRL_Q_ACK
    - 收到 → drain_state_ = DRAIN_LOCAL
    - drain_timeout_counter_ 累加

  [DRAIN_LOCAL]
    - 等待 NIC-DMA ROB 归零 (outstanding count == 0)
    - rob_ valid entries == 0 → drain_state_ = IDLE
    - 拉高 DRAIN_COMPLETE 中断
```

### §6.5 Remote Poison Mapper (per MAS-3.1-DMA-Backends §3.1.3)

```
收到 UALink Response Flit:
  if flit.header.MsgType == MEM_RESP_D:
    if flit.addr_tag_status.Status == Remote_Error (0x3):
      response.status = 0x1 (Remote_CXL_Poison, per MAS-3.1-DMA-Backends §5)
      remote_poison_count_++
      cxl_poison_count_++ (来自 CXL Switch)
      log_error(flit, "Remote CXL Poison")
```

---

## §7 RAS 处理矩阵 (per MAS-3.1-DMA-Backends §5)

| Backend 引擎 | 故障类型 | UDD Status Code | GPC AWT Trap Type | 软件/驱动恢复动作 |
|------|----------|-----------------|-------------------|---------------------|
| **HBM-DMA** | HBM ECC DED (不可纠正) | `0x0` (Local HBM Poison) | `Local_Memory_Fault` | 隔离物理页, 迁移数据, 重启 Kernel |
| **HBM-DMA** | HBM Row 退役 (Page Retire) | `0x0` (Local HBM Poison) | `Local_Memory_Fault` | 更新 OS 页表, 避免再次访问该页 |
| **HBM-DMA** | HBM Refresh 超时 | `0x2` (Local Fault) | `Local_Memory_Fault` | 触发 SMI, KMD 重训练 HBM PHY |
| **NIC-DMA** | UALink 链路 Down / Drain 超时 | `0x1` (Remote CXL/Link Poison) | `Remote_Link_Fault` | FM 隔离故障链路, 重路由 Scale-Up 流量 |
| **NIC-DMA** | Switch 返回 CXL Poison | `0x1` (Remote CXL/Link Poison) | `Remote_CXL_Fault` | FM 隔离远端 CXL 节点, 触发 Checkpoint 恢复 |
| **NIC-DMA** | Retry/Replay 超限 | `0x2` (Local Fault) | `Remote_Link_Fault` | FM 隔离故障链路 |
| **IO-DMA** (v1.1+) | PCIe 链路 Down / UR | `0x4` (Host_IO_Fault) | `Host_IO_Fault` | 重置 PCIe 链路, 通知 Host OS 驱动 |
| **IO-DMA** (v1.1+) | ATS Translation Fault | 不生成 Poison, 触发 PRI | 不触发 AWT, Warp 挂起 | Host OS 调页, 返回 PRI Response |

---

## §8 物理实现 (Physical Design) 与面积预估

### §8.1 模块化布局 (per MAS-3.1-DMA-Backends §6.1)

```
Floorplan:
┌─────────────────────────────────────────────────────┐
│  [UBC 共享基座]      [HBM Protocol Engine]          │
│  Standard Cells       Hard Macro (靠近 Bump Map)   │
│  - NoC Terminator     - HBM3e PHY (16 PC)          │
│  - ROB (1024 entry)   - Bank Manager (16×16)       │
│  - QoS Arbiter                                     │
│  - RAS Injector       [NIC Protocol Engine]              │
│                       - UALink SerDes (x4/8)          │
│                       - Retry/Replay Buffer            │
│                       - Drain FSM                      │
└─────────────────────────────────────────────────────┘
```

### §8.2 面积收益 (per MAS-3.1-DMA-Backends §6.2)

| 维度 | V3.0 独立 DMA | UBC + 协议引擎 v1.0 | 收益 |
|------|---------------|----------------------|------|
| NoC 接口逻辑 | 3 份独立 | 1 份 UBC 共享 | -40% |
| Tracker / ROB | 3 份独立 | 1 份 UBC 共享 (1024-entry) | -50% |
| QoS Arbiter | 3 份独立 | 1 份 UBC 共享 (8 class) | -40% |
| RAS Injector | 3 份独立 | 1 份 UBC 共享 | -50% |
| **整体 Backend 面积** | **100%** | **88%** | **-12%** |

### §8.3 时钟域 (per MAS-3.1-DMA-Backends §6.3)

```
时钟域分布:
- clk_core (2.0 GHz): SM/TC-DMA/TEE Frontend (不在本模块)
- clk_fab (1.2 GHz): HBM-DMA Protocol Engine + Memory NoC
- clk_io (1.0 GHz): NIC-DMA (UALink) + PHY (v1.1+ IO-DMA PCIe)

跨时钟域:
- UBC 与 Protocol Engine 之间: 标准异步 FIFO (Gray-code 指针)
- CDC 隔离在 req_cdc_fifo_ + resp_cdc_fifo_ (per §3.1)
```

---

## §9 寄存器与接口布局

### §9.1 NIC-DMA (Scale-Up) 控制与状态寄存器 (per `21-microarch-ifc-mvp.md` §4.2)

基地址: `0x00F0_0000`

| 偏移地址 | 寄存器名称 | R/W | 描述 |
|----------|-----------|-----|------|
| `0x0200` | `NIC_PORT_CTRL` | W | `[3:0]`: Port Enable, `[7:4]`: UALink Speed Config |
| `0x0210` | `NIC_DRAIN_CTRL` | W | Bit 0: `START_DRAIN` (触发两阶段 Drain), Bit 1: `FORCE_ABORT` |
| `0x0214` | `NIC_DRAIN_STATUS` | R | Bit 0: `DRAINING`, Bit 1: `SWITCH_QUIESCED`, Bit 2: `DRAIN_COMPLETE`, `[31:16]`: Outstanding Count |
| `0x0220` | `NIC_PHY_STATUS` | R | `[3:0]`: Link Speed (1.0/1.1), `[7:4]`: Lane Width (x4/x8/x16), Bit 8: `LINK_UP` |
| `0x0230` | `NIC_PMU` | R | Retry/Replay/Timeout/Drain 计数 (per §6.2) |

### §9.2 HBM-DMA 控制与状态寄存器 (新增)

| 偏移地址 | 寄存器名称 | R/W | 描述 |
|----------|-----------|-----|------|
| `0x0400` | `HBM_CTRL` | W | Bit 0: `ENABLE`, Bit 1: `ECC_BYPASS` (调试用) |
| `0x0410` | `HBM_PHY_STATUS` | R | `[3:0]`: Speed (HBM3e), `[7:4]`: PC Enabled Mask, Bit 8: `ZQ_DONE` |
| `0x0420` | `HBM_PMU_ROHIT` | R | Row Buffer Hit 计数 (FR-FCFS First-Ready) |
| `0x0428` | `HBM_PMU_ECC` | R | `[31:0]`: SEC 计数, `[63:32]`: DED 计数 |
| `0x0430` | `HBM_RETIRED_ROWS` | R | Retired Row 数量 + 索引 |

### §9.3 TEE-UDD 与 Backends 接口 (per `21-microarch-ifc-mvp.md` §3.4)

```
统一 Dispatch 信号定义 (Global UDD Hub → Backend UBC):

udd_be_req_*:
  - udd_be_req_valid (1 bit)
  - udd_be_req_tag (4 bit Route_Tag: 0:HBM, 1~E:NIC, F:IO)
  - udd_be_req_vc (3 bit VC)
  - udd_be_req_gupa (64 bit)

udd_be_resp_*:
  - udd_be_resp_valid (1 bit)
  - udd_be_resp_tracker_id (16 bit)
  - udd_be_resp_status (4 bit Status Code)
  - udd_be_resp_data (256 bit)
```

### §9.4 IO-DMA 寄存器 (v1.1+ 预留)

| 偏移地址 | 寄存器名称 | R/W | 描述 |
|----------|-----------|-----|------|
| `0x0300` | `PCIE_CTRL` | W | Bit 0: `LINK_ENABLE`, Bit 1: `ATS_ENABLE` |
| `0x0304` | `PCIE_STATUS` | R | `[3:0]`: Link Speed/Width, Bit 4: `ATS_ACTIVE` |
| `0x0310` | `GDS_CTRL` | W | GPU Direct Storage 零拷贝控制 |

---

## §10 TC-DMA 与 Backends 边界

> **TC-DMA 是 SM 内部特例, 不经过 Backend (per `21-tee-udd-mvp.md` §9.1)**

- TC-DMA 复用 TEE Frontend (描述符解析) 和 Midend (AGU/DTE)
- TC-DMA **不**生成 UDD Micro-op, 而是生成 SMEM Read/Write 事务
- TC-DMA **不**经过 UDD Agent 与 NoC, 也**不**经过 Backends/UBC
- v1.0 MVP TC-DMA 接口见 [`21-tee-udd-mvp.md` §9](21-tee-udd-mvp.md)

---

## §11 验证与测试 (v1.0 MVP)

### §11.1 单元测试 (`test_ubc_*.cc`, `test_hbm_*.cc`, `test_nic_*.cc`)

| # | 测试用例 | 覆盖能力 | 断言 |
|---|---------|---------|------|
| 1 | `UBC.ROB.allocate/release` | ROB Tracker 分配/释放 | 1024 entry 循环复用正确 |
| 2 | `UBC.QoS.TokenBucket` | Token Bucket 限速 | low priority throttled 计数正确 |
| 3 | `UBC.RAS.Status Code mapping` | RAS Injector 标准化 | HBM DED → 0x0, NIC Remote → 0x1 |
| 4 | `UBC.CDC.async fifo` | 跨时钟域异步 FIFO | Gray-code 指针, 无 metastability |
| 5 | `HBM.FR_FCFS.row hit priority` | First-Ready 调度 | Row Hit 优先, FCFS 兜底 |
| 6 | `HBM.WriteDrain.75% threshold` | Write Drain 切换 | write_count > 75% 强制 Write Mode |
| 7 | `HBM.Refresh.ABR/PBR` | Refresh Manager | 7.8us ABR + 1.95us PBR 触发正确 |
| 8 | `HBM.ECC.SEC correct` | SECDED 单比特纠错 | sec_count_++, 数据透明 |
| 9 | `HBM.ECC.DED poison` | SECDED 双比特检错 | ded_count_++, Status=0x0 |
| 10 | `HBM.PageRetirement.3 CE` | Page Retire | 同一 Row 3 次 SEC → retired |
| 11 | `NIC.UALinkFlit.pack` | Flit 封装 | 256B 格式, SeqNum/SrcNode 正确 |
| 12 | `NIC.RetryReplay.GoBackN` | 滑动窗口 | NAK → 从 K 重传, pmu_replay_count_++ |
| 13 | `NIC.Drain.TwoPhase` | 两阶段 Drain | IDLE → BLOCK_NEW → WAIT_SWITCH → DRAIN_LOCAL |
| 14 | `NIC.Drain.Timeout` | Drain 超时 | 100us 超时 → Fatal RAS 中断 |
| 15 | `NIC.P2PBypass.local HBM` | P2P 旁路 | Local HBM target → 不经过 UBC + Memory NoC |
| 16 | `NIC.RemotePoison.mapping` | CXL Poison 映射 | Remote_Error → Status=0x1 |

### §11.2 集成测试 (`test_backends_hbm_e2e.cc`, `test_backends_nic_e2e.cc`)

**核心 demo 测试** (per §2 端到端 shippable demo):
- HBM E2E: 12 步全链路贯通 (TEE-UDD → UBC → HBM-DMA → HBM)
- NIC E2E: TEE-UDD → UBC → NIC-DMA → UALink Switch Mock → Peer GPU HBM
- 测试标签: `[backends][hbm][nic][e2e]`

### §11.3 跨仓集成测试

- UsrLinuxEmu 端 driver 配置 UDD HRT + Backends
- CppTLM 端 TEE-UDD + Backends 处理请求
- 端到端 HBM 访存校验

### §11.4 Oracle 评审清单

- [ ] UBC 共享基座代码复用率 ≥ 40% (per MAS-3.1-DMA-Backends §1.2)
- [ ] HBM-DMA FR-FCFS Row Buffer Hit > 80% (SPEC workload)
- [ ] HBM-DMA SECDED 1-bit 纠错正确, 2-bit 检测正确
- [ ] HBM-DMA Page Retirement 阈值 = 3 CE (per MAS-3.1-DMA-Backends §2.2)
- [ ] NIC-DMA UALink Flit 格式与 SLA 一致 (per MAS-3.1-Fabric §3.1)
- [ ] NIC-DMA Drain 两阶段握手 ≤100us (per MAS-3.1-Fabric §4.2)
- [ ] NIC-DMA Remote CXL Poison → Status=0x1 (per MAS-3.1-DMA-Backends §5)
- [ ] 跨时钟域 FIFO 无 metastability (Gray-code + 2-flop 同步)
- [ ] 整体 Backend 面积 ≤ V3.0 88% (per MAS-3.1-DMA-Backends §6.2)
- [ ] 跨仓 ABI 影响: 0 个新 ABI 函数 (v1.0 MVP 严格遵守)
- [ ] 5 阶段演进路线图无债务 (per evolution-roadmap §5)

---

## §12 演进不变量 (4 条契约)

**详细定义见 [`21-dma-backends-evolution-roadmap.md` §5](21-dma-backends-evolution-roadmap.md)**。本节列出 v1.0 MVP 的具体实现要求。

### §12.1 不变量 1: UDD Micro-op 接口在演进中只增不换

```
v1.0: 160-bit Micro-op 格式 (per MAS-3.1-DMA-TEE §5.1)
v2.0+: 仅扩展 header 字段新增位 (如 Remote_Poison_Type), 不破坏现有字段布局
实现: header 字段固定 32-bit, 演进通过预留位扩展
```

### §12.2 不变量 2: UBC 共享基座从 v1.0 起定义所有子模块

```cpp
// v1.0 即定义 UBC 共享基座接口 (per MAS-3.1-DMA-Backends §1.1)
// 即使 v1.0 仅 HBM/NIC 两种 Protocol Engine, 也预留 IO Protocol Engine 虚函数
class UbcBaseTLM {
    // 派生类必须实现的虚函数 (Strategy Pattern)
    virtual void engine_tick() = 0;  // clk_io 域处理
};

// v1.1 加 IO Protocol Engine 时, 仅新增派生类, 不修改基类
```

### §12.3 不变量 3: Status Code 从 v1.0 起定义完整 RAS 映射表

```cpp
enum class UbcStatusCode : uint8_t {
    Success = 0x0,
    LocalHbmPoison = 0x0,
    RemoteCxlPoison = 0x1,
    LocalFault = 0x2,
    PageFault = 0x3,
    HostIoFault = 0x4  // v1.0 预留, v1.1 IO-DMA 启用
};
// v1.0 即定义完整枚举, 即使 v1.0 仅使用前 3 个值
```

### §12.4 不变量 4: ROB Tracker 容量从 v1.0 起按 Backend 类型预留

```cpp
// v1.0: 1024 entry ROB per UBC (per MAS-3.1-DMA-Backends §1.1.2)
static constexpr size_t ROB_ENTRIES = 1024;
// v1.0 即定义, 即使 v1.0 仅 HBM/NIC 实际使用
// v1.1+ 加 IO Protocol Engine 时, 可选支持独立 ROB 容量 (不强制修改)
```

---

## §13 边界与限制

### §13.1 不实现的功能 (v1.0 MVP 明确边界)

- ❌ **IO-DMA (PCIe Gen6 + ATS/PRI + GDS)** — v1.0 仅预留接口, v1.1 引入
- ❌ **Multi-Backend 并行 (NIC 多 Port × N 同时 active)** — v1.0 单 Port active 验证, v1.1+ 多 Port
- ❌ **PCIe Gen6 PAM4 物理层** — v1.0 模拟 PHY, v1.1+ RTL 实现
- ❌ **CXL.cache / CXL.io 协议** — v1.0 严格剥离, 仅处理 UALink Mem (per MAS-3.1-Fabric §1)
- ❌ **Switch Multicast (All-Reduce 硬件)** — v1.0 仅基本 Mem 路由, v2.0 加 (per `21-fabric-switch-evolution-roadmap.md`)
- ❌ **原子操作 (Atomic Add/Sub)** — v1.0 仅 Read/Write, v2.0 加
- ❌ **HBM Page Policy 动态切换** — v1.0 固定 Open-Page, v2.0 加 Closed-Page
- ❌ **TC-DMA 经 Backends** — TC-DMA 不经过本模块 (per `21-tee-udd-mvp.md` §9.1)
- ❌ **Driver 直接访问 Backends 寄存器** — Driver 经 UDD Hub 注入, Backends 不直暴露给 Driver

### §13.2 已知限制

- UBC 共享基座代码复用率: 目标 ≥40%, 实际取决于 Protocol Engine 接口契合度
- ROB Tracker: 1024 entry, 满 → Stall 上游 (per GMMU 协同)
- QoS Token Bucket: 8 class, 静态配置 (不动态学习)
- HBM-DMA 物理 PHY: v1.0 TLM 模拟, 不含 RTL 级时序
- NIC-DMA Retry/Replay: Go-Back-N (Selective Repeat 推迟 v2.0)
- P2P Bypass: 仅 Local HBM target, 不支持 Local HBM ↔ Peer GPU 双向
- Drain Timeout: 100us 硬编码, 不动态调整

---

## §14 跨仓契约

### §14.1 UsrLinuxEmu 侧 (不修改 23 ABI)

Backends v1.0 MVP **不引入任何新 CppTLM ABI 函数**——所有 driver↔GPU 通信通过**现有 23 ABI 函数** (`syms->mmio_write/read`).

### §14.2 CppTLM 侧 (新模块 + 命名变更)

| 文件 | 状态 |
|------|------|
| `include/tlm/dma/ubc_base_tlm.{hh,cc}` | **新建** (UBC 共享基座) |
| `include/tlm/dma/ubc_hbm_engine_tlm.{hh,cc}` | **新建** (HBM Protocol Engine) |
| `include/tlm/dma/ubc_nic_engine_tlm.{hh,cc}` | **新建** (UALink Protocol Engine) |
| `include/tlm/dma/ubc_io_engine_tlm.{hh,cc}` | **预留** (v1.1 引入) |
| `include/tlm/dma/hbm_dma_ubc_tlm.{hh,cc}` | **新建** (HBM-DMA UBC 派生类) |
| `include/tlm/dma/nic_dma_ubc_tlm.{hh,cc}` | **新建** (NIC-DMA UBC 派生类) |
| `include/tlm/gpu/nic_dma_tlm.{hh,cc}` | **归档** (per §1.3 命名变更) |
| `include/tlm/gpu/io_dma_tlm.{hh,cc}` | **归档** (per §1.3 命名变更) |
| `include/tlm/gpu/hbm_dma_tlm.{hh,cc}` | **归档** (per §1.3 命名变更) |
| `test/test_ubc_*.cc` (4+ 用例) | **新建** |
| `test/test_hbm_*.cc` (5+ 用例) | **新建** |
| `test/test_nic_*.cc` (7+ 用例) | **新建** |
| `test/test_backends_hbm_e2e.cc` | **新建** (HBM 端到端 demo) |
| `test/test_backends_nic_e2e.cc` | **新建** (NIC 端到端 demo) |
| `src/tlm/dma/ubc_shell.cc` | **修改** (注入 Backends 拓扑) |
| `include/chstream_register.hh` | **修改** (注册 UbcBaseTLM/HbmDmaUbcTLM/NicDmaUbcTLM) |
| **23 ABI 头冻结** | ✅ **不变** (per ADR-088 §D5) |

### §14.3 跨仓 PR 协调

按 ADR-091 §R5.1 跨仓 PR 流程:
1. CppTLM 仓: 实现 UBC + HBM/NIC Protocol Engine + 16+ 测试
2. UsrLinuxEmu 仓: driver 协调 Backends 寄存器 (由用户承担协调)
3. 跨仓集成测试: `test_backends_hbm_e2e_ue.cc` + `test_backends_nic_e2e_ue.cc`
4. 同步 PR (无 ABI 影响, 跨仓风险低)

---

## §15 引用

### §15.1 内部引用

- [`21-dma-backends-evolution-roadmap.md`](21-dma-backends-evolution-roadmap.md) — Backends 5 阶段演进路线图 (SSOT for evolution)
- [`21-tee-udd-mvp.md`](21-tee-udd-mvp.md) — TEE + UDD MVP (上游)
- [`21-fabric-switch-mvp.md`](21-fabric-switch-mvp.md) — Fabric 协议 + Scale-Up Switch (NIC 协议对端)
- [`21-microarch-ifc-mvp.md`](21-microarch-ifc-mvp.md) — Core/Micro-Arch + RTL-IFC (微架构/接口)
- [`16-pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT

### §15.2 跨仓引用

- UsrLinuxEmu AGENTS.md §CppTLM 通信架构 — **PCIe-only 原则**
- UsrLinuxEmu ADR-088 §D5 — 23 ABI 冻结

### §15.3 MAS-3.1 上游原始设计

- `MAS-3.1-DMA-Backends Rev2.0` (UBC + Protocol Engines 原始设计)

### §15.4 业界参考

- NVIDIA HBM3 Controller (FR-FCFS + Bank Manager)
- AMD Infinity Fabric (Retry/Replay + Drain FSM)
- Intel CXL.mem Controller (Coherence Fabric)
- ARM CMN-700 HN-F (Home Node, Router)

---

## §16 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: Backends v1.0 MVP 详细设计 (UBC 共享基座 + HBM/NIC Protocol Engine + 12 步 shippable demo + 16 测试用例 + 4 条不变量) |

---

**关联 OpenSpec change**: 待 `openspec/changes/2026-09-19-cpptlm-mas-dma-backends-mvp/` 提案创建
**下次更新**: Oracle 评审反馈后 v1.1 (IO-DMA 引入)