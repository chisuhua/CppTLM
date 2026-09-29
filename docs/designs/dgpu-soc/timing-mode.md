# Minimal DGpu SoC — Timing Mode (AT) 架构视图

> **版本**: v0.2 (H1-H6 + 12 处残余 + 7 处新识别硬伤全部落地, **Oracle 五轮 FAIL 已清理, 待六轮复评 PASS**)
> **日期**: 2027-02-09
> **状态**: 🆕 **提案草案** — 独立 timing-mode SoC(与 minimal_v1 functional-mode **并列共存**,不替换)
> **前置**: `docs/designs/dgpu-soc/architecture.md` v1.0 (functional-mode minimal_v1, ✅ 已交付) + ADR-DGPU-05~10 + D-AXI v1.4 实施真相
> **配套 OpenSpec**: `openspec/changes/cpptlm-dgpu-soc-timing-mvp/` (本 change 同步创建)
> **配套 ADR** (待签发): ADR-DGPU-11-timing-mode-soc-scope.md
> **Owner**: CppTLM Team
> **权威源**: 本文档为 **timing-mode 业务架构视图**(why);代码权威源待 OpenSpec change 实施后归档为 `dgpu_soc_timing_v1.json` 配置 + 对应模块 .cc 实现

## 关联文档与变更

- **第 1 层 (架构视图)**:
  - [`./architecture.md`](./architecture.md) — functional-mode minimal_v1 SoC 业务架构 (本目录主页,**不变**)
  - **本文档** — timing-mode (AT) SoC 业务架构 (并列视图,新增)
- **第 2 层 (ADR)**:
  - ADR-DGPU-05 (单一 VRAM 所有权) — **沿用**(同一 framebuffer_storage_ 真源)
  - ADR-DGPU-06 (AxiMemBundle 边界) — **沿用**(chip-internal 边界不变)
  - ADR-DGPU-07 (演进 seam) — **触发**: D3/VramController 提前到 v1.0
  - **ADR-DGPU-11 (新增)**: timing-mode SoC 范围与 functional-mode 共存边界
- **第 3 层 (OpenSpec changes)**:
  - [`cpptlm-driver-visible-minimal-soc`](../../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.4 active) — functional-mode 实施真相
  - [`cpptlm-minimal-dgpu-soc-v1-architecture`](../../../openspec/changes/cpptlm-minimal-dgpu-soc-v1-architecture/) (✅ archived) — 命名约定落地
  - **`cpptlm-dgpu-soc-timing-mvp`** (🆕 本 change) — timing-mode SoC 实施提案
- **第 4 层 (实施跟踪)**: 待 `docs/superpowers/plans/cpptlm-dgpu-soc-timing-mvp.md` 创建

**同步规则**: 本文档代表 timing-mode 长期设计意图(why);实施指导 (how) 见 OpenSpec change 的 design.md;逆向同步不强制。修改 functional-mode `architecture.md` 不影响本文档独立性(per §1.3 边界)。

---

## 0. 为什么需要独立 timing-mode SoC

### 0.1 gem5 等价物参照

| gem5 仿真模式 | TLM 2.0 等价 | 用途 | 是否已覆盖 |
|---------------|--------------|------|-----------|
| **atomic mode** | LT (Loose Timing) — 零时延 memcpy | driver 正确性验证 | ✅ minimal_v1 (本仓 functional-mode) |
| **timing mode** | AT (Approximately Timed) — 周期近似建模 | 性能/带宽/竞争分析 | ❌ **未覆盖 (本提案填补)** |
| **full-system** | Cycle-accurate RTL co-sim | 硬件验证 | 🚫 不在 SoC 仿真范围 |

**gem5 的 `simulate.py` 必须显式选择 `--mem-type` + `--cpu-type` (atomic vs timing)**,两者并存于同一代码基。本提案镜像这一原则:**timing-mode 与 functional-mode 共存于同一 DGpuBoard 框架,通过 JSON `simulation_mode` 字段切换**。

### 0.2 用户决策: 不替换 minimal_v1

| 候选方案 | 优点 | 缺点 | 决策 |
|----------|------|------|------|
| A. 改造 minimal_v1 为 timing | 单一 SoC,工作量看似小 | **违反 6 条 Invariants**,破坏 66951 assertions (per AGENTS.md baseline; 含 [minimal_dgpu_soc] 41 + [pcie-memory] 24 + [abi][minimal_dgpu_soc] 28 + D2 memory 67 + Phase 1-8 全链路),driver 视角闭环重测 | ❌ |
| B. 新增 timing-mode 并列(本提案) | functional-mode 不变,timing-mode 独立配置 + 独立测试 | 双套模块需双套测试套件 | ✅ |
| C. v2.0 完全重写 | 单一 spec,无历史包袱 | 等价于重新设计 MVP,失去 driver 闭环已交付价值 | ❌ |

**采用方案 B**。minimal_v1 的 §1.4 5 项"简化是设计意图"被保留;timing-mode 在**独立配置** + **独立测试套件**下提供性能建模能力。

### 0.3 当前 SoC 范围图

```
┌─────────────────────────────────────────────────────────────┐
│                DGpuBoard (shell + 23 ABI 冻结)              │
├─────────────────────────────────────────────────────────────┤
│  ┌────────────┐         ┌────────────┐                      │
│  │ minimal_v1 │         │timing-mode │  ← 同一 board shell │
│  │  (LT)      │         │   (AT)     │     不同 DGpuSoc    │
│  └────────────┘         └────────────┘     实例             │
│       │                       │                              │
│  ┌────▼────────────┐    ┌──────▼──────────┐                  │
│  │ dgpu_soc_       │    │ dgpu_soc_       │                  │
│  │ minimal_v1.json │    │ timing_v1.json  │                  │
│  │ (✅ 已交付)      │    │ (🆕 本提案)     │                  │
│  └─────────────────┘    └─────────────────┘                  │
└─────────────────────────────────────────────────────────────┘
        ↓                              ↓
   driver 闭环正确性              性能/带宽/竞争分析
   (CT 优先)                      (性能回归)
```

---

## 1. 设计目标与边界

### 1.1 目标

构建一个**周期近似的 timing-mode dGPU SoC 示例**,采用 **AT (Approximately Timed) 仿真模式**,贯通 **driver ABI → PCIe BAR 时序化路径 → SDMA outstanding DMA → GMMU 异步 page walk → NoC 仲裁 → 周期级 memory controller → framebuffer_storage_** 路径:

- **NoC 仲裁周期** (v0.2): CrossbarTLM 现有 4-port loopback 内 arb_latency (可配,默认 4 cyc);GMMU **不**在 Crossbar 数据路径 (per M9)
- **SDMA cycle accounting** (v0.2): 沿用既有 5-port + backdoor path,`translate_cb_` 内调 `translate_timing` 累加 TLB miss latency + 记录 `last_descriptor_complete_cycle_` (per H3 反馈,**不**拆分 descriptor、**不**新增 AXI master port)
- **GMMU page walk**: TLB miss 时经 4 级页表 walk,每级 **N cycles** 延迟(可配,默认 50),最终返回 paddr + total_latency
- **MemoryTLM cycle accounting**: 读延迟 **N cycles**(行命中 100 / 行未命中 200),写延迟 **N cycles**(默认 120),数据搬运走 ChStream req/resp
- **VramControllerTLM (D3 触发)**: 行缓冲 + 周期调度,bandwidth 限制可配(默认 32 GB/s)
- **Stats 完整**: 所有模块启用 `stats_latency_*` Distribution,bandwidth = `bytes / total_cycles`

### 1.2 非目标

- **NG1**: 不替换 minimal_v1 (functional-mode),两模式并列
- **NG2**: 不做多级页表硬实现(本版本 GMMU 仍一级,TLB miss 走 4-cycle 模拟延迟)
- **NG3**: 不改 23 ABI 签名(冻结,per ADR-088 §D5)
- **NG4**: 不改 SDMA 既有 5 端口语义 (v0.2);timing-mode **不**新增 AXI master port、**不**新增 outstanding (per H3 Oracle 反馈)
- **NG5**: 不做多通道 HBM 仿真(单通道 HBM timing,D4 后续)
- **NG6**: 不引入第三方依赖(Cycle-accurate NoC 用既有 `crossbar_tlm.hh` + `link_tlm.hh`)
- **NG7**: 不改 framebuffer/vram 所有权(沿用 ADR-DGPU-05,真源仍 `DGpuBoard::framebuffer_storage_`)

### 1.3 与 minimal_v1 functional-mode 的边界

| 维度 | functional-mode (minimal_v1) | timing-mode (本提案) |
|------|------------------------------|----------------------|
| **仿真模式** | LT, zero-delay memcpy | AT, cycle-approximate |
| **NoC** | 无(直 memcpy) | `CrossbarTLM` 4 routes |
| **SDMA 搬运** | `memcpy(host_backdoor_, vram_backdoor_, size)` | cycle accounting (沿用既有 5-port + backdoor, **不**新增端口, per H3, 六轮再确认) |
| **GMMU** | 同步 `translate()` 直读 PTE | 同步 `translate()` + `page_walk_cycles_` 累加 + 模拟 wait |
| **MemoryTLM** | seam holder(零时 memcpy) | cycle-approximate 读/写延迟,ChStream req/resp |
| **Cache coherence** | 天然(单线程无 cache) | **必须显式**(cycle 内 ordering 决定可见性) |
| **backdoor** | host 直读直写 framebuffer | **禁用于性能测试**(绕过 cycle accounting) |
| **测试目标** | driver 闭环正确性 | 性能回归 + 带宽分析 |
| **JSON 配置** | `dgpu_soc_minimal_v1.json` | `dgpu_soc_timing_v1.json` |
| **注册模块** | 6 个 (`PcieEndpointIP` + `SdmaEngineTLM` + `GmmuTLM` + `MemoryTLM` + `CompletionRingTLM` + `DGpuSoc`) | **8 个** (+ `CrossbarTLM` + `VramControllerTLM`) |
| **测试标签** | `[minimal_dgpu_soc]` | `[dgpu_soc_timing]` (新增标签) |
| **测试周期** | ≤ 1000 cycles (driver 闭环) | 100K - 10M cycles (workload 级) |

**两种模式在同一 DGpuBoard 框架内并存**:
- 通过 `simulation_mode: "functional" | "timing"` JSON 字段切换
- `DGpuBoard::init()` 根据 mode 决定实例化哪种 DGpuSoc 子类
- 23 ABI 入口(不变)+ framebuffer_size_bytes(不变)+ framebuffer_storage_(共享 owner)

### 1.4 关键设计原则

1. **同一 framebuffer_storage_ 真源**: `DGpuBoard::framebuffer_storage_` 仍是唯一 backing (per ADR-DGPU-05)。timing-mode 通过 `MemoryTLM::set_backing_store` 注入,与 minimal_v1 同源。天然 coherence 由 **cycle accounting** 保证,而非"无 cache"假设。
2. **cycle accounting 而非真实延迟**: 本版本延迟参数(N cycles)为**仿真建模值**,**不**精确匹配真实硬件时序;用于 driver 性能**回归对比**(同 workload、不同 memory access pattern),不用于绝对带宽数字。
4. **NoC 仲裁统计可见**: CrossbarTLM 必须暴露 `stats_arb_cycles_`, `stats_port_utilization_`, 用于分析 SDMA/GMMU 竞争。
5. **backdoor 仅用于 correctness sanity**: timing-mode 测试**禁止**用 backdoor 验证数据;backdoor 仅用于 sanity check(确认 cycle accounting 完成后数据正确)。性能回归必须用 cycle-approximate 路径。
6. **统计与 minimal_v1 隔离**: timing-mode 引入新 stats namespace(`system.dgpu_soc.timing.*`),不污染 functional-mode 的 `system.memory.*`。

### 1.5 仿真模式声明 (Simulation Mode)

**v0.2 模式**: **Timing Mode (AT, cycle-approximate)** — 周期近似仿真模式

本提案明确采用 AT (TLM 2.0 `Loose Timing` 之对立面),含义:

1. **数据搬运走 ChStream req/resp 时序**
   - 实际路径: `SDMA` 发出 AXI req → `CrossbarTLM` 仲裁 → `VramControllerTLM` 调度 → `MemoryTLM` 累加 cycles → resp 返回
   - **等价于** gem5 `SimpleMemory::recvTimingReq()` + `Port::sendTimingResp()`
   - **不模拟**: 真实硬件 wire delay、gate-level delay、process variation

2. **GMMU 翻译带 page walk 延迟**
   - 实际路径: `translate()` 查 TLB → miss 时累加 `page_walk_cycles_` → 同步返回 paddr + total_latency
   - **等价于** gem5 `MMU::translateTiming()` (同步翻译但带 cycle 累加)
   - **不模拟**: 真实 MMU pipeline stall、TLB refill contention

3. **NoC 仲裁周期可见**
   - 实际路径: `CrossbarTLM::tick()` 内读 valid/ready 握手,busy 时排队,expose `stats_arb_wait_cycles_`
   - **等价于** gem5 `Bus::arbitrate()`
   - **不模拟**: 真实 NoC wire latency、credit-based flow control、VC 路由

4. **MemoryTLM 周期级响应**
   - 实际路径: `MemoryTLM::tick()` 接 req 后,根据 row hit/miss 决定延迟(100 / 200 cycles),`defer_response()` 调度 N cycle 后发 resp
   - **等价于** gem5 `SimpleMemory::accessAndRespond()`
   - **不模拟**: DRAM refresh、bank conflict、channel rank 调度

5. **VramControllerTLM 行缓冲模拟**
   - 实际路径: D3 触发,继承 `MemoryTLM` 行为 + 加 row buffer hit/miss + bandwidth 上限
   - **等价于** gem5 `DRAMSim2` interface (简化版)
   - **不模拟**: 多通道并行、bank group rotation、refresh overhead

**模式依据**: gem5 `MemSim` 的 `timing` mode 是工业级 timing 仿真 baseline;TLM 等价词为 **AT (Approximately Timed)**,本提案**不**追求 cycle-approximate RTL 级精度。

---

## 2. 模块清单与拓扑

> **🔧 v0.2 重构注 (per Oracle 二轮 H2/H3 反馈)**:
> 原 v0.1 提案的 multi-hop cascade 拓扑 (`SDMA → Crossbar → VramCtrl → MemoryTLM`) **不可实施**,原因:
> - `CrossbarTLM` 仅有 `req_in[4]` + `resp_out[4]` 两组 loopback 端口 (`crossbar_tlm.hh:53-54`),无下游 `req_out`/`resp_in` 转发能力
> - `PcieEndpointIP` 无 `host_egress`/`host_ingress` 端口 (全仓 grep 零命中)
> - `CacheReqBundle` 无 `rid`/`user` 字段,`size` 是 `ch_uint<8>` (上限 255B),SDMA outstanding 走 `AXI master port` 无 bundle 载体
>
> **v0.2 简化策略**: timing-mode **不**改造 interconnect 拓扑,而是**复用既有 functional-mode 路径**,在以下 3 个层级加 cycle accounting:
> 1. `GmmuTLM::translate_timing()` — TLB miss cycle 累加
> 2. `MemoryTLM::tick()` — read/write cycle 累加 + priority queue defer_response
> 3. `CrossbarTLM::tick()` — arb_latency + stats_arb_cycles (现有 4 端口不变)
>
> `VramControllerTLM` **不**是单独的 cascade 模块,而是**同位置替换** `MemoryTLM` 的 D3 seam 占用 (per ADR-DGPU-07): timing-mode 配置中可选 `vram_ctrl` 替代 `memory`,不引入新端口。

### 2.1 拓扑 (v0.2 简化)

```
DGpuBoard (shell, 23 ABI 冻结)
└── DGpuSoc (SimModule 容器, simulation_mode="timing")
    ├── PcieEndpointIP          (SimModule, BAR 存储 + config space + MSI-X + PcieAxiAdapter 周期, 不变)
    │   └── PcieBarRouter       (BAR0 寄存器表,不变)
    ├── SdmaEngineTLM           (🔧 扩展 5-port + cycle accounting, 沿用 backdoor path, **不**新增 AXI master port)
    ├── GmmuTLM                 (🔧 扩展: translate_timing + TLB + page_walk_cycles_)
    ├── CrossbarTLM             (🔧 扩展: arb_latency + stats_arb_cycles + stats_port_utilization, **现有 4-port loopback 不变**)
    ├── VramControllerTLM       (🆕 可选, **同位置替换** MemoryTLM;选 vram_ctrl 时不实例化 memory)
    ├── MemoryTLM               (🔧 扩展: defer_response priority queue + stats_latency, 默认 zero_delay=true)
    └── CompletionRingTLM       (fence 完成 → MSI-X,不变)
```

> **与 minimal_v1 拓扑对比**: timing-mode **不增加新端口,不改既有连接**,只在以下 3 模块加 cycle accounting + 1 个 stats 字段 (CrossbarTLM)。
> 拓扑形状与 minimal_v1 functional-mode **完全一致**;所有改动是模块内部实现。

### 2.2 模块清单(8 个注册类型 / 7 个实例化,1 个新建 + 4 个扩展 + 3 个复用,VramCtrl/Memory 互斥实例化)

| 组件 | 来源 | 职责 (timing-mode) |
|------|------|---------------------|
| `PcieEndpointIP` | ✅ 复用 | BAR 存储 + config space + MSI-X + `PcieAxiAdapter` 周期 (Phase 5 既有) |
| `PcieBarRouter` | ✅ 复用 | BAR0 寄存器表(不变) |
| `SdmaEngineTLM` | 🔧 扩展 | DMA 搬运 (沿用 5-port + backdoor path, **不**新增端口) + cycle accounting (descriptor 处理 + `translate_timing` 累加) |
| `GmmuTLM` | 🔧 扩展 | 一级页表 + 32-entry 直接映射 TLB + `translate_timing` (SimModule 同步,返回 latency) |
| `CrossbarTLM` | 🔧 扩展 | 现有 4-port loopback 不变 + 新增 `set_arb_latency()` + `stats_arb_cycles_` + `stats_port_utilization_[4]` |
| `MemoryTLM` | 🔧 扩展 | `defer_response()` (priority queue, per M1) + stats_latency_read_/write_ 启用 (默认 `use_zero_delay_for_test_=true`, per M5) + `current_cycle_` setter |
| **`VramControllerTLM`** | 🆕 新建 (可选) | **同位置替换** MemoryTLM (继承,行缓冲 hit/miss + bandwidth 上限,per D3 seam);JSON 配置可选 `vram_ctrl` 替代 `memory` |
| `CompletionRingTLM` | ✅ 复用 | fence 完成汇聚 → MSI-X vector 0 (不变) |
| `DGpuBoard` + `DGpuSoc` | shell + 容器 | 23 ABI 入口(不变) + 新 `simulation_mode_` 字段 + `init_timing_mode()` 分支 |

### 2.3 不在 v0.1 范围(后续工作)

- **多通道 HBM (D4)**: 单 channel VramControllerTLM,D4 替换为 MemoryClusterTLM 多通道
- **Cache (L1/L2)**: 无 cache 模块,MemoryTLM 直接面对 backing
- **Compute Pipeline (CP)**: GPU kernel launch 不模拟,仅测 DMA/内存子系统
- **Display IO device (D1)**: 不集成,本期聚焦 timing-mode 存储侧
- **真实硬件校准**: cycles 数值是仿真建模值,不与具体 GPU SKU 对齐

---

## 3. 时序子系统(Cycle-Approximate 模型)

### 3.1 设计原理

镜像 gem5 `SimpleMemory::recvTimingReq()` + `Bus::arbitrate()` + `MMU::translateTiming()` 三件套:

- **gem5 模式**: `SimpleMemory` 接 timing req,记录 arrival time,根据 access latency 调度 future event,event 触发后发 timing resp。`Port::schedTimingResp()` 通过 EventQueue。
- **CppTLM v0.1 映射**: `MemoryTLM::tick()` 接 req 后,记录 `req.arrival_cycle_`,调用 `EventQueue::schedule(resp, current_cycle + latency)`,在 `defer_response()` 内 push resp 到延迟队列。

### 3.2 模块时序表

| 模块 | 时序来源 | 延迟参数 (cycle) | 配置字段 |
|------|----------|------------------|----------|
| `MemoryTLM` (读) | `stats_latency_read_` Distribution | row hit: 100, row miss: 200 | `read_latency_hit_cycles`, `read_latency_miss_cycles` |
| `MemoryTLM` (写) | `stats_latency_write_` Distribution | default: 120 | `write_latency_cycles` |
| `GmmuTLM` (TLB hit) | 同步 | 0(无延迟) | (硬编码 0) |
| `GmmuTLM` (TLB miss) | 累加 `page_walk_cycles_` | default: 50 | `tlb_miss_latency_cycles` |
| `CrossbarTLM` 仲裁 | 内部 busy counter | default: 4 | `arb_latency_cycles` |
| `VramControllerTLM` | row buffer hit/miss + bandwidth | hit: 100, miss: 200 | `vram_read_hit_cycles`, `vram_read_miss_cycles` |
| `SdmaEngineTLM` 请求发起 | 同步(逻辑 0 cycle) | 0 | (硬编码 0) |
| PCIe BAR write (host→device) | TLP propagation (简化) | 2 (1 cycle TLP 编码 + 1 cycle req) | `pcie_tlp_latency_cycles` |
| MSI-X (device→host) | 简化 | 1 | `msix_latency_cycles` |

### 3.3 MemoryTLM 时序扩展

```cpp
// include/tlm/memory_tlm.hh (扩展 v0.1, 新增 defer_response 路径)
class MemoryTLM : public ChStreamModuleBase {
private:
    // ... 既有 backing_ptr_/size_cap_/stats_ 不变 ...

    // 🆕 v0.2: timing-mode 延迟响应队列 (priority queue 按 ready_cycle 升序)
    // per M1 Oracle 二轮反馈: std::deque + FIFO front() 会导致 head-of-line blocking
    // (req B ready N+55 被 req A ready N+100 阻塞)。必须用 priority queue 才能保证
    // TInv-2 强 ordering。
    struct PendingResp {
        bundles::CacheRespBundle resp;
        uint64_t ready_cycle;
        bool operator>(const PendingResp& o) const noexcept { return ready_cycle > o.ready_cycle; }
    };
    std::priority_queue<PendingResp, std::vector<PendingResp>, std::greater<PendingResp>>
        pending_resps_;

    // 🆕 v0.1: 当前 cycle (per DGpuBoard::tick 调用 advance)
    uint64_t current_cycle_ = 0;

public:
    // ... 既有 API 不变 ...

    // 🆕 v0.1: cycle advance (DGpuBoard::tick 调用)
    void advance_cycle() noexcept { ++current_cycle_; }

    void tick() override {
        if (req_in_.valid() && req_in_.ready()) {
            const auto& req = req_in_.data();
            const uint64_t tid = req.transaction_id.read();
            const uint64_t addr = req.address.read();
            const uint64_t sz   = req.size.read();
            const bool is_write = req.is_write.read();

            // backing 零时路径 (默认开启,保 functional 零回归)
            // timing-mode 须在 init_timing_mode() 内显式 set_use_zero_delay_for_test(false) 触发延迟路径
            if (backing_ptr_ != nullptr && use_zero_delay_for_test_) {
                // ... 既有 v2.2 行为 (per docs/designs/dgpu-soc/architecture.md §3.3) ...
            }

            // 🆕 v0.1 timing-mode 路径: 立即计算 resp payload,延迟发出
            bundles::CacheRespBundle resp;
            resp.transaction_id.write(tid);
            uint64_t latency = 0;

            if (backing_ptr_ == nullptr) {
                // legacy 路径 (兼容 functional-mode 测试)
                latency = is_write ? write_latency_cycles_ : (row_hit(addr) ? read_latency_hit_cycles_ : read_latency_miss_cycles_);
            } else {
                if (addr + sz > (size_cap_ ? size_cap_ : backing_ptr_size_)) {
                    resp.error_code.write(1);  // OUT_OF_RANGE
                    resp.is_hit.write(0);
                    resp.data.write(0);
                    latency = 1;  // 错误响应立即
                } else if (is_write) {
                    uint64_t val = req.data.read();
                    std::memcpy(backing_ptr_ + addr, &val, std::min<size_t>(sz, 8));
                    resp.error_code.write(0);
                    resp.is_hit.write(1);
                    resp.data.write(0);
                    latency = write_latency_cycles_;
                } else {
                    uint64_t val = 0;
                    std::memcpy(&val, backing_ptr_ + addr, std::min<size_t>(sz, 8));
                    resp.error_code.write(0);
                    resp.is_hit.write(1);
                    resp.data.write(val);
                    latency = row_hit(addr) ? read_latency_hit_cycles_ : read_latency_miss_cycles_;
                }
                if (is_write) ++stats_requests_write_;
                else          ++stats_requests_read_;
            }

            // sample latency stats (functional 模式不动;timing 启用)
            if (is_write) stats_latency_write_.sample(latency);
            else          stats_latency_read_.sample(latency);

            // 🆕 v0.2: schedule resp to priority queue (按 ready_cycle 升序自动排序)
            pending_resps_.push({resp, current_cycle_ + latency});
            req_in_.consume();
        }

        // 🆕 v0.2: drain priority queue (top 必为 ready_cycle 最小者,避免 FIFO head-of-line blocking)
        while (!pending_resps_.empty() && pending_resps_.top().ready_cycle <= current_cycle_) {
            resp_out_.write(pending_resps_.top().resp);
            pending_resps_.pop();
        }

        if (adapter_) adapter_->tick();
    }

    // 🆕 v0.1: 5 个时序参数 setter
    void set_timing_params(uint64_t read_hit, uint64_t read_miss, uint64_t write, bool use_zero) noexcept {
        read_latency_hit_cycles_  = read_hit;
        read_latency_miss_cycles_ = read_miss;
        write_latency_cycles_     = write;
        use_zero_delay_for_test_  = use_zero;
    }

private:
    bool row_hit(uint64_t addr) const noexcept {
        // 行命中假设: 低 16 bit 落在同一行 (4K row),简化模型
        return (addr & 0xF000ULL) == ((addr >> 16) & 0xF000ULL);
    }

    uint64_t read_latency_hit_cycles_  = 100;
    uint64_t read_latency_miss_cycles_ = 200;
    uint64_t write_latency_cycles_     = 120;
    bool     use_zero_delay_for_test_  = true;  // 默认 true 保 functional 零回归 (per M5 Oracle 反馈)

    // ... backing_ptr_ / stats_ 不变 ...
};
```

### 3.4 GmmuTLM 时序扩展

```cpp
// include/tlm/gpu/gmmu_tlm.{hh,cc} (v0.1 扩展)
class GmmuTLM : public ::SimModule {
public:
    // ... 既有 API (set_pt_base_lo/hi/enabled + set_backing + translate) ...

    // 🆕 v0.1: page_walk_cycles_ 累加 + stats
    struct TranslateStats {
        uint64_t hits = 0;       // TLB hit
        uint64_t misses = 0;     // TLB miss → page walk
        uint64_t total_walk_cycles = 0;
    };
    TranslateStats translate_stats_;

    // 🆕 v0.1: TLB (32-entry, 直接映射,简化模型)
    struct TlbEntry {
        uint64_t iova_page = 0;  // page-aligned iova
        uint64_t paddr = 0;
        bool valid = false;
    };
    std::array<TlbEntry, 32> tlb_entries_;

    // 🆕 v0.1: timing-mode translate (同步返回 + 累加 cycle)
    // 返回: 0 = 成功 (paddr 写入); -EIO = 失败; latency_cycles 输出
    int translate_timing(uint64_t iova, uint32_t size, uint64_t& out_paddr, uint64_t& latency_cycles) {
        const uint64_t page_num = iova >> 12;
        latency_cycles = 0;

        // 1. TLB lookup
        const uint64_t tlb_idx = page_num % 32;
        TlbEntry& entry = tlb_entries_[tlb_idx];
        if (entry.valid && entry.iova_page == page_num) {
            // TLB hit, 0 cycle
            ++translate_stats_.hits;
            out_paddr = entry.paddr | (iova & 0xFFF);
            return 0;
        }

        // 2. TLB miss → page walk (4 级 walk,每级 tlb_miss_latency_cycles_)
        // 一级页表: pte = *(backing_ + pt_base() + page_num * 8)
        const uint64_t pte_addr = pt_base() + page_num * 8;
        if (pte_addr + 8 > backing_size_) return -EIO;

        uint64_t pte = 0;
        std::memcpy(&pte, backing_ + pte_addr, sizeof(pte));
        if (!(pte & 1ULL)) return -EIO;  // invalid PTE

        // 3. 累加 walk cycles
        latency_cycles = tlb_miss_latency_cycles_;  // 简化: 1 级 = N cycles
        ++translate_stats_.misses;
        translate_stats_.total_walk_cycles += latency_cycles;

        // 4. TLB fill
        entry.iova_page = page_num;
        entry.paddr = pte & ~0xFFFULL;
        entry.valid = true;

        out_paddr = entry.paddr | (iova & 0xFFF);
        return 0;
    }

    // 🆕 v0.1: 既有 translate() 内部转调 translate_timing(), latency 0
    // 注: GmmuTLM::translate 非虚函数,**不**加 override 关键字 (per M7 Oracle 反馈)
    int translate(uint64_t iova, uint32_t size, uint64_t& out_paddr) {
        uint64_t lat = 0;
        return translate_timing(iova, size, out_paddr, lat);
    }

    void set_timing_params(uint64_t tlb_miss_latency) noexcept {
        tlb_miss_latency_cycles_ = tlb_miss_latency;
    }

private:
    uint64_t tlb_miss_latency_cycles_ = 50;
};
```

### 3.5 SDMA cycle accounting (沿用既有 5-port + backdoor path)

> **🔧 v0.2 重构 (per Oracle H2/H3 反馈)**:
> 原 v0.1 提案的"SDMA outstanding + AXI master port + rid 关联"**不可实施**:
> - `CacheReqBundle` 无 `rid`/`user` 字段 (`cache_bundles_tlm.hh:38-69` 实际字段只有 `transaction_id`/`parent_id`/`fragment_id`/`fragment_total`/`address`/`size`/`is_write`/`data`)
> - `size` 是 `ch_uint<8>` 上限 255 字节,4KB sub-req 溢出
> - "Phase 5 PcieAxiAdapter 已实现模式"伪引用,`pcie_axi_adapter_tlm.cc` 无 chunk/split 逻辑
>
> **v0.2 简化策略**: timing-mode SDMA **不新增端口**,沿用既有 5-port + `set_vram_backdoor()` 路径 (functional-mode 流程),仅在 descriptor 处理路径中加 cycle accounting:
> 1. `translate_timing()` 返回 `latency_cycles` 经 SDMA::last_tlb_latency_ 成员回传, 由 `DGpuBoard::sdma_fence_complete` 回调注入 `last_descriptor_complete_cycle_` (= board.current_cycle_, per 七轮 P0-13 终稿, **不**SDMA 自加 current_cycle_)
> 2. memcpy 搬运本身仍 zero-delay (per functional-mode zero-time),但 `current_cycle_` 累加后,fence 完成 cycle = `current_cycle_ + tlb_miss_latency + ...`,可被外部测试断言
> 3. **无** outstanding / AXI master port / rid 关联 / 拆 chunk

```cpp
// include/tlm/gpu/sdma_engine_tlm.{hh,cc} (v0.2 简化: 不新增端口)
class SdmaEngineTLM : public ChStreamModuleBase {
public:
    // ... 既有 5 端口 + set_vram_backdoor + set_translate_cb **不变** ...

    // 🆕 v0.2: cycle accounting (沿用既有 translate_cb_, 加 latency 累加)
    // 不再调用 submit_descriptor_timing 新 API,直接复用既有 submit_descriptor + set_translate_cb
    // translate_cb_ 内调 gmmu_->translate_timing(iova, size, &phys, &lat)
    // v0.2 (per 七轮 P0-13): SDMA **不**自加 current_cycle_;SDMA 仅记录 last_descriptor_complete_cycle_ (由 board 回调注入 = board.current_cycle_);TInv-3 强制由 board 集中推进 cycle
    uint64_t last_descriptor_complete_cycle() const noexcept {
        return last_descriptor_complete_cycle_;
    }

private:
    uint64_t last_descriptor_complete_cycle_ = 0;
    // backing_ptr_ / translate_cb_ / ring buffer / outstanding map 全部不变
};
```

> **设计要点**:
> - **无新端口**:SDMA 5 端口 (desc_in / mem_in / mem_out / host_out / done_out) **不变**
> - **无 outstanding 拆分**:Descriptor 沿用 functional-mode 的 memcpy 路径 (`memcpy(host_backdoor_+phys, vram_backdoor_+offset, size)`)
> - **cycle accounting 在哪里**: 通过 `set_translate_cb` 注册的 `translate_timing` 累加 TLB miss latency 到 `current_cycle_`;SDMA fence 完成时记录 `last_descriptor_complete_cycle_` 供外部断言
> - **stats 暴露**:`stats_latency_*` 仍由 GMMU 维护 (TLB hit/miss 分布)

### 3.6 CrossbarTLM 增强 + JSON 配置 (v0.2 简化)

> **🔧 v0.2 重构 (per Oracle H2)**:
> 原 v0.1 提案的 11 条 cascade connection (`vram_ctrl.resp_out → memory.req_in` + `crossbar.req_out[*]` + `pcie_ep.host_egress/ingress`) **不可实施**:
> - `CrossbarTLM` 仅有 `req_in[4]` + `resp_out[4]` loopback (`crossbar_tlm.hh:53-54`),无 `req_out`/`resp_in` 下游端口
> - `PcieEndpointIP` 无 `host_egress`/`host_ingress` (全仓 grep 零命中)
> - `VramControllerTLM` 继承 MemoryTLM,**无新增 mem 侧端口**
> - `memory.req_in` 双驱动导致 routing 冲突

> **v0.2 简化策略**: timing-mode **不引入新 connection**,沿用 minimal_v1 functional-mode 的 connection 拓扑;CrossbarTLM 仅在现有 4-port loopback 上加 `arb_latency` + stats,VramControllerTLM 与 MemoryTLM **互斥实例化**(JSON 二选一)。

#### 3.6.1 `dgpu_soc_timing_v1.json` 配置示例 (v0.2)

```json
{
  "name": "dgpu_soc_timing_v1",
  "simulation_mode": "timing",
  "description": "Timing-mode (AT) DGpu SoC v0.2 (per ADR-DGPU-11 + Oracle 二轮 H2 拓扑重构)",
  "framebuffer_storage_size_bytes": 8589934592,
  "framebuffer_size_bytes": 268435456,
  "modules": [
    {
      "name": "soc",
      "type": "DGpuSoc",
      "simulation_mode": "timing",
      "modules": [
        {
          "name": "pcie_ep",
          "type": "PcieEndpointIP",
          "params": {
            "config_size": 4096,
            "num_msix_vectors": 16,
            "bar_sizes": [4096, 268435464]
          }
        },
        {
          "name": "sdma",
          "type": "SdmaEngineTLM",
          "params": {
            "max_inflight": 4,
            "vram_size_bytes": 8589934592,
            "enable_cycle_accounting": true   // 🆕 timing 标志
          }
        },
        {
          "name": "gmmu",
          "type": "GmmuTLM",
          "params": {
            "page_size_bytes": 4096,
            "tlb_miss_latency_cycles": 50,
            "tlb_size": 32
          }
        },
        {
          "name": "crossbar",
          "type": "CrossbarTLM",
          "params": {
            "num_ports": 4,
            "arb_latency_cycles": 4,
            "stats_arb_cycles": true,
            "stats_port_utilization": true
          }
        },
        {
          "name": "vram_ctrl",                  // 🆕 可选 (D3 seam,同位置替换 MemoryTLM)
          "type": "VramControllerTLM",
          "params": {
            "vram_size_bytes": 8589934592,
            "row_hit_cycles": 100,
            "row_miss_cycles": 200,
            "bandwidth_gbps": 32
          }
        }
        // 注: 当 vram_ctrl 已实例化,不再实例化 memory (同位置互斥)
      ]
    }
  ]
  // 注: connections 与 minimal_v1 完全一致 (沿用 functional-mode 拓扑)
}
```

#### 3.6.2 拓扑说明 (v0.2)

- **GMMU 不在 Crossbar 数据路径**: GmmuTLM 是 SimModule,`translate_timing()` 同步调用,结果 `phys` 注入 SDMA req 的 address;GMMU 不走 Crossbar
- **VramCtrl 与 Memory 互斥**: JSON 配置只能选其一 (`vram_ctrl` 替代 `memory`,不引入新 connection);共享 `set_backing_store` 同一份 `framebuffer_storage_`
- **Crossbar 4-port loopback**: port 0=SDMA, port 1=Host (PcieAxiAdapter via Phase 5), port 2=CPU/Other, port 3=Memory/VramCtrl;**新增** `set_arb_latency` + `stats_arb_cycles_` + `stats_port_utilization_[4]` 三个成员
- **PcieAxiAdapter (Phase 5)** 已建模 PCIe TLP 时序 (2 cycle TLP propagation),timing-mode **不**再独立加 host_egress/ingress 端口
- **SDMA 沿用 functional-mode 5-port + backdoor**: 不新增 AXI master port,不引入 outstanding/rid 关联
```

---

## 4. BAR 路由与读写路径

### 4.1 BAR 布局

沿用 minimal_v1 §4.1 约束(`bar_sizes: [4096, 268435464]`,doorbell offset 0x10010000 hardcoded),timing-mode **不**改变 BAR 布局与路由 flag。

### 4.2 BAR 路由表 (timing-mode v0.2 简化)

> **🔧 v0.2 重构 (per Oracle H2)**: BAR 路由拓扑与 minimal_v1 functional-mode **完全一致**(不引入新 connection)。timing-mode **不**在 BAR 路径上加 cycle accumulation (Crossbar arb / VramCtrl row hit/miss 都**不**存在于此路径),**仅**通过 PcieAxiAdapter (Phase 5) 既有 TLM 时序计 PCIe TLP 延迟,SDMA descriptor 内部通过 `translate_timing` 计 GMMU TLB latency。

| Host 操作 | 路径 | 命中 | timing-mode 时序累加 |
|----------|------|------|----------|
| `mmio_write(BAR0, off)` | PcieBarRouter → DGpuBoard BAR0 hook → GmmuTLM | GMMU 寄存器 | `pcie_tlp_latency` (2 cyc, Phase 5) + GMMU set register (0 cyc) |
| `mmio_write(BAR1, off ∈ [0, 0x10010000))` | PcieStorage → **framebuffer_storage_** (沿用 functional-mode fast-path) | 存储空间 | `pcie_tlp_latency` (2 cyc) + functional-mode backdoor (0 cyc, 沿用) |
| `mmio_write(BAR1, off == 0x10010000)` | doorbell 路径 → SDMA ring consume | SDMA | `pcie_tlp_latency` (2 cyc) + doorbell parse (1 cyc) |
| `mmio_write(BAR2, off ∈ [0, 8GB))` | PcieMemoryDevice route → **framebuffer_storage_** | vram aperture | `pcie_tlp_latency` (2 cyc) + functional-mode backdoor (0 cyc, 沿用) |
| **SDMA H2D (4KB)** | translate(iova) → paddr | **timing-mode 路径** | `GmmuTLM::translate_timing` TLB hit (0 cyc) 或 TLB miss (`tlb_miss_latency_cycles`, 默认 50) + functional-mode memcpy (0 cyc, 沿用 backdoor) |

### 4.3 与 minimal_v1 的关键差异 (v0.2 简化)

| 路径 | minimal_v1 (functional) | timing-mode (v0.2) |
|------|--------------------------|----------------------|
| **BAR1 存储写** | 直 memcpy 到 framebuffer_storage_,0 cycle | 同 functional (0 cycle,沿用 backdoor) |
| **BAR2 vram aperture** | 直 memcpy 到 framebuffer_storage_,0 cycle | 同 functional (0 cycle,沿用 backdoor) |
| **GMMU translate** | 同步 0 cycle | TLB hit 0 cyc, miss 累加 `tlb_miss_latency_cycles` (默认 50) |
| **PCIe TLP propagation** | (无周期,沿用 functional-mode) | Phase 5 `PcieAxiAdapter` 已有 2 cyc TLP 计时 |
| **SDMA H2D 搬运** | 1 次 memcpy,0 cycle | 同 functional (0 cycle) + GMMU TLB miss latency 累加 |
| **多 desc 并发** | 串行 (SDMA FIFO) | 串行 (SDMA FIFO 不变) — **无** outstanding (per H3 Oracle 反馈) |

### 4.4 双 ingress 路径钩点 (沿用 minimal_v1 §4.3)

timing-mode 同样保留**两条** ingress 路径(ABI 路径 + TLP/AXI 路径),**cycle accounting 在两者都生效**:

1. **ABI 路径** (`DGpuBoard::mmio_write`): v0.2 时序累加 = `pcie_tlp_latency` (2 cyc, per Phase 5) + functional-mode backdoor (0 cyc)
2. **TLP/AXI 路径** (`PcieEndpointIP::tick()` AXI slave): 已有 `PcieAxiAdapter` 计时 (per Phase 5,2 cyc TLP),**不**接 Crossbar 仲裁 (per H2 Oracle 反馈:CrossbarTLM 无 req_out/resp_in 下游转发)

> **v0.2 cycle accounting 总览**: BAR 路径周期来源仅 3 处 — Phase 5 PcieAxiAdapter TLP propagation (2 cyc) + GMMU TLB miss (50 cyc) + doorbell parse (1 cyc)。**无** Crossbar / VramCtrl cascade cycle。

---

## 5. GMMU 设计 (Timing-Mode 异步翻译)

### 5.1 API 扩展

```cpp
class GmmuTLM : public ::SimModule {
public:
    // ... 既有 minimal_v1 API (set_pt_base_lo/hi/enabled + set_backing + translate) ...

    // 🆕 v0.1: timing-mode 翻译 (同步返回 + 累加 cycle)
    // 返回: 0 = 成功; -EIO = 失败
    // out_paddr 仅成功时写入
    // out_latency_cycles 输出本次翻译的 cycle 累加值
    int translate_timing(uint64_t iova, uint32_t size, uint64_t& out_paddr, uint64_t& out_latency_cycles);

    // 🆕 v0.1: 统计访问
    const TranslateStats& stats() const noexcept { return translate_stats_; }
    void reset_stats() noexcept { translate_stats_ = {}; }

    // 🆕 v0.1: 时序参数
    void set_timing_params(uint64_t tlb_miss_latency) noexcept;
};
```

### 5.2 翻译流程 (timing-mode)

```
SDMA translate_cb_(iova, size, &phys)
  → GmmuTLM::translate_timing(iova, size, phys, latency_cycles)
    1. TLB lookup (idx = page_num % 32)
       → hit: latency += 0, return paddr
    2. miss: page walk
       → latency += tlb_miss_latency_cycles_ (50)
       → 累加到 translate_stats_.total_walk_cycles
       → TLB fill
    3. return paddr + total_latency_cycles

SDMA 通过 `last_tlb_latency_` 成员接收 lat (TInv-3 强制由 board 集中推进 cycle, SDMA 仅记录 `last_descriptor_complete_cycle_`, per 七轮 P0-13, **不**调度 outstanding resp)
  → SDMA 内部记录 last_tlb_latency_ + last_descriptor_complete_cycle_ (board 回调注入, per TInv-3)
  → SDMA memcpy (functional-mode 0 cycle, **不**经 Crossbar/VramCtrl cascade, per H2)
  → fence → submit_fence → process_fence_queue → completion_ring_->push(entry)
  → fence → MSI-X (累加 msix_latency_cycles)
```

### 5.3 TLB 模型 (v0.1 简化)

- **32-entry**, 直接映射(`idx = page_num % 32`)
- **无 eviction 策略** (v0.1 仅 fill,不 evict);D4 后续可加 LRU
- **TLB flush**: `gmmu_->reset()` / `set_pt_base_lo/hi` 时全清(per minimal_v1 §5.1)

### 5.4 升级路径 (D4 后续)

- **多级页表 + walk cycle 模型**: 4 级 walk,每级 50 cycle,总 200 cycle
- **TLB LRU eviction**: 32-entry + LRU
- **Context table**: GMMU MVP proposal 完整版

---

## 6. SDMA / Fence / MSI-X 链路 (timing-mode v0.2 简化)

> **🔧 v0.2 重构 (per Oracle H2/H3)**: SDMA 数据流**与 minimal_v1 functional-mode 完全一致**——沿用 5-port (`desc_in/mem_in/mem_out/host_out/done_out`) + `set_vram_backdoor()`。timing-mode **不新增端口、不引入 outstanding、不拆 4KB sub-req、不接 Crossbar 仲裁、不接 VramCtrl cascade**。Cycle accounting 仅在 `translate_cb_` 回调中累加 GMMU TLB miss latency。

### 6.1 数据流 (timing-mode v0.2)

```
host mmio_write(BAR1, 0x10010000, wptr)        ← hardcoded doorbell offset
  → DGpuBoard::mmio_write 检测 kBar1DoorbellOffset
    → sdma_engine_->mmio_write(1, kBar1DoorbellOffset, wptr)
      → SdmaEngineTLM::mmio_write (ring mode consume)
        → RPTR..WPTR 区间 descriptors
          → DmaDescriptor 解析
            → 既有 submit_descriptor(desc)  (沿用 functional-mode API)
              → translate_cb_(iova, size, &phys) 调用
                → GmmuTLM::translate_timing(iova, size, &phys, &lat)  (累加 GMMU TLB lat)
                → SDMA **不**自加 current_cycle_ (per TInv-3);last_descriptor_complete_cycle_ 由 DGpuBoard 回调注入 (per 六轮 P0-13)
              → memcpy(vram_backdoor_+offset, host_backdoor_+phys, size)  (functional-mode 0 cycle)
              → fence → submit_fence → process_fence_queue
                → completion_ring_->push(entry)
                  → DGpuBoard::sdma_fence_complete
                    → SDMA::last_descriptor_complete_cycle_ = current_cycle_  (🆕 timing 标志)
                    → msix_update_pending (Phase 5 累加 msix_latency_cycles)
                      → host irq_cb_
```

### 6.2 时序累加图 (SDMA H2D 4KB 一次 descriptor, v0.2 简化)

> v0.2 时序来源仅 **3 处**: Phase 5 PcieAxiAdapter TLP (2 cyc) + GMMU TLB miss (50 cyc) + doorbell parse (1 cyc) + MSI-X (1 cyc)。**无** Crossbar arb / VramCtrl row hit/miss / 4 outstanding cycle。

| 阶段 | 来源 cycle | 累加 | 备注 |
|------|-------------|------|------|
| PCIe TLP propagation | 2 | 2 | Phase 5 PcieAxiAdapter 既有 |
| SDMA ring consume + parse | 1 | 3 | doorbell 路径 |
| GMMU translate (TLB hit) | 0 | 3 | `translate_timing` 返回 lat=0 |
| GMMU translate (TLB miss) | 50 | 53 | `translate_timing` 累加 50 cyc |
| SDMA memcpy 搬运 | 0 | (不变) | functional-mode backdoor 0 cyc |
| MSI-X | 1 | 54 | Phase 5 |
| **Total (TLB hit)** | — | **~4 cycles** | 3 + 1 MSI-X |
| **Total (TLB miss)** | — | **~54 cycles** | 53 + 1 MSI-X |

> **注意**: v0.2 时序数字 **远小于** v0.1 的 108/358 cyc,因为 timing-mode **不**包含 Crossbar arb / VramCtrl row hit/miss。**这是简化设计的真实结果**——timing-mode 在 cycle accounting 上更接近 functional-mode,主要差异在 GMMU TLB miss latency。如果需要 cycle 数逼近 v0.1,后续可在 MemoryTLM defer_response (priority queue, 100-200 cyc) 路径上**单独**做性能回归测试,但**不**进 SDMA 数据流。

### 6.3 复用 vs 新增

| 项 | 来源 | v0.1 改动 |
|----|------|-----------|
| `set_translate_cb` | 既有 | 不变;内部转 `translate_timing` |
| `set_vram_backdoor` | 既有 | timing-mode 不用 backdoor;改用 `set_backing` + MemoryTLM |
| `submit_fence` / `set_completion_ring` | 既有 | 不变;fence 周期由 `last_descriptor_complete_cycle_` 记录 |
| `submit_descriptor` | ✅ 复用 | 不变 (v0.2 **不**新增 `submit_descriptor_timing`) |
| AXI master port | ❌ **不**新增 | 沿用 5-port + backdoor path (per H2 Oracle 反馈) |
| `translate_cb_` 回调 | 🔧 扩展 | 内部转调 `gmmu_->translate_timing()` 累加 latency |
| `last_descriptor_complete_cycle_` | 🆕 新增成员 | 记录 SDMA 完成 cycle,外部测试可断言 |

---

## 7. JSON 配置

### 7.1 最小 timing-mode 配置示例

参见 §3.6 `dgpu_soc_timing_v1.json` 示例。关键差异:

```json
{
  "name": "dgpu_soc_timing_v1",
  "simulation_mode": "timing",   // 🆕 与 minimal_v1 区分
  ...
}
```

### 7.2 JSON 字段说明 (v0.2 简化)

| 字段 | 位置 | 含义 | timing-mode 新增 |
|------|------|------|------------------|
| `simulation_mode` | 顶层 | `"functional"` 或 `"timing"` | 🆕 必填 |
| `framebuffer_storage_size_bytes` | 顶层 | VRAM 容量 (per ADR-DGPU-05 单一真源) | 🆕 |
| `framebuffer_size_bytes` | 顶层 | BAR1 fast-path 窗口大小 | 🆕 |
| `tlb_miss_latency_cycles` | gmmu.params | GMMU TLB miss 延迟 | 🆕 |
| `tlb_size` | gmmu.params | TLB entry 数 (默认 32) | 🆕 |
| `num_ports` / `arb_latency_cycles` | crossbar.params | NoC 端口数 (固定 4) + 仲裁延迟 (默认 4) | 🆕 |
| `stats_arb_cycles` / `stats_port_utilization` | crossbar.params | stats 开关 | 🆕 |
| `row_hit_cycles` / `row_miss_cycles` | vram_ctrl.params | 行缓冲 hit/miss 延迟 | 🆕 |
| `bandwidth_gbps` | vram_ctrl.params | bandwidth 上限 | 🆕 |
| `read_latency_hit_cycles` / `read_latency_miss_cycles` / `write_latency_cycles` | memory.params | 时序参数 (MemoryTLM priority queue 路径) | 🆕 |
| `enable_cycle_accounting` | sdma.params | SDMA cycle 累加开关 (默认 true) | 🆕 |

> **v0.2 移除字段** (相对 v0.1):
> - `max_outstanding`: SDMA **不**新增 outstanding (per H2/H3 反馈),无此字段
> - `crossbar.num_ports: 5`: 改回 `4` (per H2 反馈,无 host_egress 端口)

### 7.3 JSON 层级约束

沿用 minimal_v1 §7.3 (顶层 `modules[0]` 必须是 DGpuSoc 节点)。

### 7.4 模块注册 (新增 1 个, per H1 修正)

```cpp
// include/chstream_register.hh:46 宏体内追加 (per H1 Oracle 反馈)
// 既有 REGISTER_CHSTREAM 是无参硬编码宏 (chstream_register.hh:46),**不**接受参数
// 正确做法: 在宏体内追加 registerObject + registerAdapter 两行 (per M10 + H1)
#define REGISTER_CHSTREAM                                                                           \
    ModuleFactory::registerObject<CacheTLM>("CacheTLM");                                            \
    ModuleFactory::registerObject<MemoryTLM>("MemoryTLM");                                          \
    ModuleFactory::registerObject<CrossbarTLM>("CrossbarTLM");                                      \
    /* 🆕 v0.2 (per ADR-DGPU-11 + Oracle H1): VramControllerTLM 同位置替换 MemoryTLM */               \
    ModuleFactory::registerObject<VramControllerTLM>("VramControllerTLM");                          \
    /* ... 既有其他模块注册保持不变 ... */
```

> **注意 (per H1 修正)**:
> - 原 v0.1 提案写 `REGISTER_CHSTREAM(VramControllerTLM)` 是**错误**的 (`REGISTER_CHSTREAM` 是无参宏,`chstream_register.hh:46`)
> - v0.2 正确做法: 在宏体内追加 `registerObject<VramControllerTLM>`(与既有 `MemoryTLM` 同一模式)
> - **不**需要 `REGISTER_MODULE(VramControllerTLM)`:MemoryTLM 继承自 `ChStreamModuleBase` → `SimObject`,`REGISTER_MODULE` 的 `is_base_of<SimModule>` 静态断言**会**失败
> - `CrossbarTLM` 已在 `include/tlm/crossbar_tlm.hh` 注册,本提案**复用既有**(`registerObject<CrossbarTLM>("CrossbarTLM")` 已在宏体内)
> - `VramControllerTLM` 继承 `MemoryTLM`,共享同一 registerObject + registerAdapter 框架

---

## 8. 关键不变性 (Invariants)

### TInv-1: 共享 framebuffer_storage_ 真源 (timing-mode 适用)

`DGpuBoard::framebuffer_storage_` 仍是 SoC 全部访存的唯一 backing owner(per ADR-DGPU-05)。timing-mode **不**重新分配 framebuffer_storage_;与 minimal_v1 **同一份 backing**(同一进程内可通过 JSON `framebuffer_storage_size_bytes` 一致保证)。

#### Scenario
- **GIVEN** `DGpuBoard::framebuffer_storage_` 已分配 (8GB)
- **WHEN** timing-mode 通过 `MemoryTLM::set_backing_store(ptr, size)` 注入
- **THEN** MemoryTLM::tick() 写 backing 的字节立即对 Crossbar / SDMA / GMMU 可见(同 cycle 内,需加 ordering 约束)

### TInv-2: 同一 cycle 内 ordering (timing-mode 新增)

timing-mode **禁止**以下场景:同一 cycle 内 port A 写 + port B 读同一地址(因为 cycle 边界未确定)。MemoryTLM 必须按 req 到达顺序串行处理 pending_resps_。

#### Scenario
- **GIVEN** cycle N: port A 发 req write addr 0x1000
- **AND** cycle N+5: port B 发 req read addr 0x1000
- **WHEN** MemoryTLM::tick() 处理
- **THEN** port B 读到的数据**必**是 port A 写入后的(强 ordering)

### TInv-3: cycle advance 与 EventQueue 对齐

`MemoryTLM::current_cycle_` 由 `DGpuBoard::tick()` 统一 advance,**不**自行递增。避免跨模块 cycle 漂移。

#### Scenario
- **GIVEN** DGpuBoard tick counter = N
- **WHEN** MemoryTLM::tick() 内部读 `current_cycle_`
- **THEN** 必返回 N(由 board 注入)
- **AND** MemoryTLM 不可自行 ++current_cycle_

### TInv-4: backdoor 禁用于性能测试

timing-mode 性能测试**禁止**用 `DGpuBoard::backdoor_read/write` 验证数据;backdoor 仅用于 correctness sanity(确认 cycle accounting 后数据正确)。

#### Scenario
- **GIVEN** SDMA 提交 4KB H2D, GMMU TLB miss + VramCtrl row miss
- **WHEN** 测试验证数据正确性
- **THEN** 必须等 fence 完成 + MSI-X 触发,**或**用 `inflight_count()==0` 条件
- **AND** **不可**用 backdoor_read 在 fence 完成前验证(可能看到陈旧值)

### TInv-5: BAR 路由优先级 (沿用 minimal_v1 §8 Inv-3)

`mmio_write(BAR1, off == 0x10010000)` → SDMA doorbell;否则 → PcieStorage route → framebuffer_storage_。timing-mode 在 doorbell 路径上累加 1 cyc parse latency。

### TInv-6: translate_cb 签名 (沿用 minimal_v1 §8 Inv-5)

`using DmaTranslateCb = std::function<int(uint64_t iova, uint32_t size, uint64_t& phys)>;`

GmmuTLM::translate_timing 必须严格匹配此签名(`uint32_t size`, `uint64_t& phys`,返回值 0/-EIO);`out_latency_cycles` 通过额外参数或成员变量传递(**不**破坏 callback 签名兼容性)。

---

## 9. 实施路径 (4 阶段, 4 周)

| 阶段 | 任务 | 估时 |
|------|------|------|
| **T0** | OpenSpec change `cpptlm-dgpu-soc-timing-mvp` 立项 + ADR-DGPU-11 签发 | 1d |
| **T1** | `MemoryTLM` cycle-approximate 扩展(defer_response + stats_latency) + 参数化 setter | 1d |
| **T2** | `GmmuTLM` TLB miss + page_walk_cycles_ 扩展 + `translate_timing` API | 1d |
| **T3** | `SdmaEngineTLM` cycle accounting (v0.2 简化, 沿用既有 5-port + backdoor, **不**新增端口) | **0.5d** |
| **T4** | `VramControllerTLM` 新建 (行缓冲 + bandwidth 上限, 含 T4.0 MemoryTLM protected 化) | **1.5d** |
| **T5** | `CrossbarTLM` C++ 扩展 (4 routes + arb_latency + stats) + DGpuBoard::init_timing_mode() 分支 | **2d** |
| **T6** | `dgpu_soc_timing_v1.json` 配置 + minimal test suite (8 cases) | 1d |
| **T7** | E2E 性能回归测试: H2D/D2H throughput + fence cycle accounting | 2d |
| **T8** | 文档同步: `architecture.md` §14 引用本文档 + ADR-DGPU-11 签发 + AGENTS.md 状态更新 | 0.5d |

**总计**: 10.5 人日 ≈ 2 周(单 dev)/4 周(含评审与 buffer)

### 阶段依赖图

```
T0 (立项 + ADR)
  ↓
T2 (GmmuTLM TLB) ──┐
                   ├─→ T3 (SdmaEngineTLM cycle accounting, v0.2 简化) ──┐
T1 (MemoryTLM cyc)─┘                                       ├─→ T5 (Crossbar + DGpuBoard)
                                                        ┌──┘
                                              T4 (VramCtrl) ┘
                                                        ↓
                                                  T6 (JSON + minimal test)
                                                        ↓
                                                  T7 (E2E 性能回归)
                                                        ↓
                                                  T8 (文档同步)
```

---

## 10. 测试策略

### 10.1 单元测试

| 模块 | 测试用例 (新增 `[dgpu_soc_timing]` 标签) |
|------|------------------------------------------|
| `MemoryTLM` (timing) | (1) read hit → stats_latency_read_.sample(100) (2) read miss → 200 cyc (3) write → 120 cyc (4) out-of-range → 1 cyc error (5) pending_resps_ 顺序正确 (6) use_zero_delay_for_test=true → 0 cyc |
| `GmmuTLM` (timing) | (1) TLB hit → translate_timing lat=0 (2) TLB miss → lat=50 (3) TLB fill 后再访问 → hit (4) 32-entry TLB 全满 → 仍 fill (无 evict) (5) page walk 跨页 → -EIO |
| `SdmaEngineTLM` (timing) | (1) 4KB H2D TLB hit → fence ≈ 4 cyc (2) TLB miss → ≈ 54 cyc (3) 翻译失败 → 立即 fence err (4) 公共 API 零回归 (5) last_descriptor_complete_cycle_ 记录 (v0.2 简化,删 outstanding/rid 场景)|
| `VramControllerTLM` | (1) row hit → 100 cyc (2) row miss → 200 cyc (3) bandwidth 上限 → 周期降速 (4) invalidate 后 row miss |
| `CrossbarTLM` | (1) 4 routes 仲裁 (2) 忙端口 → 其他端口 wait (3) stats_arb_cycles_ 正确 |

### 10.2 E2E 性能回归测试

```cpp
TEST_CASE("dgpu-soc-timing: H2D throughput regression", "[dgpu_soc_timing]") {
    // 1. 加载 dgpu_soc_timing_v1.json
    DGpuBoard board("test");
    REQUIRE(board.load_soc_config(timing_cfg));
    REQUIRE(board.init());

    // 2. 注入 host 4MB 数据
    std::vector<uint8_t> host_buf(4 * 1024 * 1024, 0xAB);
    auto* sdma = dynamic_cast<SdmaEngineTLM*>(board.get_internal_instance("sdma"));
    REQUIRE(sdma != nullptr);
    sdma->set_host_backdoor(host_buf.data(), host_buf.size());

    // 3. 配置 GMMU + 写入 PTE
    uint64_t pt_base = 0x10000;
    board.mmio_write(0, 0, &pt_base_lo, 4);
    board.mmio_write(0, 4, &pt_base_hi, 4);
    board.mmio_write(0, 8, &ctrl, 4);  // enable

    // 4. 提交 1024 个 4KB H2D descriptor
    constexpr uint64_t kTotalBytes = 4 * 1024 * 1024;
    constexpr uint64_t kDescSize   = 4096;
    constexpr uint64_t kNumDescs   = kTotalBytes / kDescSize;
    for (uint64_t i = 0; i < kNumDescs; ++i) {
        DmaDescriptor desc(DmaDescriptor::Dir::H2D, /*iova=*/i * kDescSize, /*vram_offset=*/i * kDescSize, kDescSize, /*tag=*/i);
        sdma->submit_descriptor(desc);  // v0.2 沿用 functional-mode API
    }

    // 5. 跑仿真 1M cycles
    const uint64_t start_cycle = board.current_cycle();
    for (int i = 0; i < 1000000 && sdma->inflight_count() > 0; ++i) {
        board.tick();
    }
    const uint64_t end_cycle = board.current_cycle();
    const uint64_t elapsed = end_cycle - start_cycle;

    // 6. 性能断言
    const double throughput_GBps = (kTotalBytes / 1e9) / (elapsed / 1e9);  // 简化: cycles=ns
    REQUIRE(throughput_GBps >= 10.0);  // 期望 ≥ 10 GB/s (VramCtrl 32 GB/s 上限 × ~30% 利用率)
    REQUIRE(throughput_GBps <= 32.0);  // 不超过 bandwidth 上限

    // 7. Stats 验证
    auto mem_stats = board.get_stats_group("system.memory");
    REQUIRE(mem_stats->getDistribution("latency_read").value() == Approx(150).margin(50));  // 平均 100-200
    REQUIRE(mem_stats->getScalar("requests_read").value() == kTotalBytes / 8);  // 8-byte req

    auto gmmu_stats = board.get_stats_group("system.gmmu");
    REQUIRE(gmmu_stats->getScalar("tlb_hits").value() + gmmu_stats->getScalar("tlb_misses").value() == kNumDescs);
}
```

### 10.3 回归测试

确保 minimal_v1 functional-mode **不**被破坏:
- `[minimal_dgpu_soc]` (41 assertions) + `[abi][minimal_dgpu_soc]` (28 assertions) + `[pcie-memory]` (24 cases) **全绿**
- timing-mode `[dgpu_soc_timing]` 新增 ≥ 30 cases,**与** minimal_v1 套件互不干扰

---

## 11. 兼容性约束

| 项 | 影响 |
|----|------|
| 23 ABI 签名 | 0 修改(冻结,per ADR-088 §D5) |
| 23 ABI 语义 | 0 修改(timing-mode 是仿真模式切换,不改变 ABI 含义) |
| DGpuBoard v2.0.2 API | 0 修改(仅新增 `simulation_mode_` 字段 + `init_timing_mode()` 分支) |
| minimal_v1 functional-mode 行为 | 0 修改(JSON `simulation_mode="functional"` 完全沿用) |
| SdmaEngineTLM 5 端口 API | 0 修改 (v0.2 **不**新增 AXI master port, per H3, 六轮再确认) |
| MemoryTLM 公共 API | 0 修改(仅 `set_timing_params()` 新增 setter,既有 `set_backing_store` 不变) |
| GmmuTLM 公共 API | 0 修改(既有 `translate()` 内部转 `translate_timing()`,新增 `translate_timing()` 暴露 latency) |
| JSON schema | 扩展(顶层 `simulation_mode` + 新模块 `VramControllerTLM`) |
| 既有 functional-mode 配置 | 兼容(默认 `simulation_mode="functional"`) |
| ADR-DGPU-05 (单一 VRAM 所有权) | 0 修改(沿用) |
| ADR-DGPU-06 (AxiMemBundle 边界) | 0 修改(沿用) |
| ADR-DGPU-07 (演进 seam) | 触发 D3 提前到 v0.1 (`VramControllerTLM` 是 seam 占用) |

---

## 12. 度量与验证

### 12.1 完成定义 (DoD)

- [ ] `MemoryTLM::defer_response()` + 5 个时序参数 setter 实现
- [ ] `GmmuTLM::translate_timing()` + 32-entry TLB + stats 实现
- [ ] `SdmaEngineTLM` cycle accounting 实现 (沿用既有 5-port,无新端口;translate_cb_ 内调 translate_timing 累加 lat + `last_descriptor_complete_cycle_` 记录)
- [ ] `VramControllerTLM` 新建 (行缓冲 + bandwidth)
- [ ] `CrossbarTLM` 4 routes 配置 (`dgpu_soc_timing_v1.json`)
- [ ] `DGpuBoard::init_timing_mode()` 分支(根据 `simulation_mode` 字段路由)
- [x] ✅ ADR-DGPU-11-timing-mode-soc-scope.md 已签发 (2027-02-09, Oracle 八轮复评 PASS, timing-mode SoC 范围与 functional-mode 共存边界)
- [ ] 单元测试 100% PASS(MemoryTLM + GmmuTLM + SDMA + VramCtrl + Crossbar, ≥ 30 cases)
- [ ] E2E 性能回归测试 100% PASS (H2D throughput + fence cycle accounting)
- [ ] minimal_v1 functional-mode 66951 assertions (per AGENTS.md baseline; 含 [minimal_dgpu_soc] 41 + [pcie-memory] 24 + [abi][minimal_dgpu_soc] 28 + D2 memory 67 + Phase 1-8 全链路) **零回归**
- [ ] 23 ABI 字节级兼容(`git diff HEAD -- include/abi/cpptlm_emulator.h` 仅包含 deprecation marker,**不**有 signature 改动)

### 12.2 性能目标 (timing-mode)

| 指标 | 目标 | 度量方式 |
|------|------|----------|
| H2D throughput (4MB 工作集) | 10-32 GB/s | E2E 测试吞吐 |
| TLB hit rate (128KB 工作集重复访问) | ≥ 95% | GMMU stats (initial fill 32 miss + 992 repeat hit ≈ 96.875%) |
| Crossbar port utilization | < 80% (避免饱和) | Crossbar stats |
| MemoryTLM 平均读延迟 | 100-200 cyc | stats_latency_read_ 平均 |
| fence cycle accuracy | ±5% (vs analytical model) | E2E 测试 vs 计算预期 |
| Backward compat (minimal_v1 不破) | 0 regression | `[minimal_dgpu_soc]` 全绿 |

> **重要**: cycles 是**仿真建模值**,**不**与真实硬件 cycle 对齐。timing-mode 用于**性能回归对比**(同 workload、不同 access pattern),不用于绝对带宽数字。

---

## 13. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: timing-mode cycle 参数失真(与真实硬件偏差大) | 高 | 性能数字无意义 | 文档明示"仿真建模值";提供 analytical 公式对照 |
| ~~R2: outstanding + 乱序完成引入 fence race~~ | (v0.2 删除 SDMA outstanding, per H3) | N/A | N/A |
| **R3**: TLB 32-entry 简化模型失真 | 中 | miss rate 偏高 | v0.1 文档明示"简化模型";D4 加 LRU + 多级页表 |
| **R4**: VramControllerTLM bandwidth 限制拖慢 E2E | 低 | 测试慢 | 默认 32 GB/s,`use_zero_delay_for_test=true` 旁路 |
| **R5**: Crossbar 4 routes + C++ 改动复杂 (per M4 Oracle 反馈) | 中 | 接线 bug / C++ 改动遗漏 | 沿用 minimal_v1 + Phase 6 既有接线模式;模板化 JSON;T5 范围已纳入 CrossbarTLM C++ 改动 (2d 预算) |
| **R6**: minimal_v1 functional-mode 被 timing 副作用污染 | 中 | 66951 assertions (per AGENTS.md baseline; 含 [minimal_dgpu_soc] 41 + [pcie-memory] 24 + [abi][minimal_dgpu_soc] 28 + D2 memory 67 + Phase 1-8 全链路) 回归 | 严格模块隔离 + `[dgpu_soc_timing]` 标签独立测试套件 |
| **R7**: 23 ABI 兼容性破坏 | 低 | driver 端失效 | 严格约束 23 ABI 签名不变;`simulation_mode` 仅影响内部仿真,不暴露 ABI |
| **R8**: cycle advance 与 EventQueue 不同步 | 低 | cycle 漂移 | Inv-3 强制 board 统一 advance |
| **R9**: Performance 数字被误读为"真实硬件" | 中 | 用户误信 | 文档 §1.5 明示"cycle-approximate 非 cycle-approximate";性能数字标 `simulation cycles` |

---

## 14. 已知推迟项 (v1.0 backlog)

| 项 | 来源 | 说明 |
|----|------|------|
| **多通道 HBM (D4)** | 文档 backlog | 单 channel VramControllerTLM,D4 替换为 MemoryClusterTLM 多通道 + 多 channel 仲裁 |
| **多级页表 walk (D4)** | `cpptlm-dgpu-gmmu-mvp` proposal | 4 级 walk,每级 50 cycle,总 200 cycle |
| **TLB LRU eviction** | 简化 v0.1 | 32-entry + LRU 替换 |
| **Cache (L1/L2)** | 非目标 | 无 cache 模块,MemoryTLM 直接面对 backing |
| **Compute Pipeline (CP)** | 非目标 | GPU kernel launch 不模拟 |
| **Display IO (D1)** | 非目标 | 不集成 timing-mode |
| **真实硬件校准** | 非目标 | cycles 数值是仿真建模值,不与具体 GPU SKU 对齐 |
| **NS (Network-on-chip)** | 非目标 | 单 CrossbarTLM;多级 NoC mesh 后续 |

---

## 15. 参考

### 15.1 内部文档
- **DGpuBoard 架构**: [`../dgpu-board/architecture.md`](../dgpu-board/architecture.md) (v2.0.2)
- **functional-mode SoC**: [`./architecture.md`](./architecture.md) (v1.0, 本目录主页,**不变**)
- **Driver 视角**: [`../dgpu-driver/architecture.md`](../dgpu-driver/architecture.md)
- **ADR-DGPU-05~10**: 单一 VRAM 所有权 + AxiMemBundle 边界 + 演进 seam + 命名约定
- **[ADR-DGPU-11-timing-mode-soc-scope.md](../adr/ADR-DGPU-11-timing-mode-soc-scope.md) (✅ 已签发 2027-02-09, Oracle 八轮复评 PASS)**: timing-mode SoC 范围与 functional-mode 共存边界
- **Phase 6 AXI4Mapper**: `include/framework/axi4_mapper.{hh,cc}` (pattern 借鉴,v0.2 已删除 outstanding, per H3)
- **Phase 5 PcieAxiAdapter**: `include/tlm/pcie/pcie_axi_adapter_tlm.{hh,cc}` (PCIe TLP 时序已建模)
- **D-AXI v1.4**: `openspec/changes/cpptlm-driver-visible-minimal-soc/` (functional-mode 实施真相)
- **D2 PcieMemoryDevice**: `openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp/` (archived, BAR2 路由基础)
- **GMMU MVP Proposal**: `openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/` (D3/D4 完整版路线)

### 15.2 gem5 等价物
- `SimpleMemory::recvTimingReq()` + `Port::schedTimingResp()` — MemoryTLM timing 路径
- `Bus::arbitrate()` — CrossbarTLM 仲裁
- `MMU::translateTiming()` — GmmuTLM::translate_timing
- `AbstractMemory::timingAccess()` + `DRAMSim2` interface — VramControllerTLM 行缓冲
- gem5 `simulate.py` `--mem-type=timing` vs `--cpu-type=atomic` 切换模式(本提案镜像)

### 15.3 TLM 2.0 标准
- OSCI TLM 2.0: LT (Loose Timing) vs AT (Approximately Timed) — 本提案采用 AT
- `tlm_blocking_put_if` + `tlm_nonblocking_put_if` — 异步时序端口
- `tlm_phase` enum — `BEGIN_REQ` / `END_REQ` / `BEGIN_RESP` / `END_RESP` 四阶段握手(本提案 v0.1 简化,后续 D4 完整化)

---

## 16. 文档维护

- **版本**: **v0.2** (Oracle 二/三轮反馈全面落地)
- **最后更新**: 2027-02-09
- **下次评审触发点**:
  - OpenSpec change `cpptlm-dgpu-soc-timing-mvp` Proposal 评审通过
  - T1-T5 任一阶段完成后 (实施层偏差校验)
  - ADR-DGPU-11 签发后 (设计意图正式记录)
- **下游依赖**:
  - OpenSpec change `cpptlm-dgpu-soc-timing-mvp/` (proposal + design + spec + tasks)
  - ADR-DGPU-11 (timing-mode SoC 范围)
  - 测试用例 (`test/test_dgpu_soc_timing_*.cc` 待开)
  - JSON 配置 (`configs/dgpu_soc_timing_v1.json` 待开)

---

**Owner**: CppTLM Team
**版本**: **v0.2** (Oracle 二/三轮反馈全面落地)
**最后更新**: 2027-02-09