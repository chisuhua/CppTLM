# Design — cpptlm-dgpu-soc-control-hub

> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/dgpu-soc-control-hub/spec.md`](specs/dgpu-soc-control-hub/spec.md)
> **目标 tag**: `cpptlm-dgpu-soc-control-hub-completion`
> **估时**: **21-30d** (Chub 新建 + 5 模块切型 + 4 adapter + PcieMemoryDevice 移除 + 回归测试)
> **依赖**: 全部 archived changes 已是 ✅;physical-ip-port-boundary + p2-integration-unblock 草稿就绪待协同
> **范围超集**: 本 change 包含 `cpptlm-dgpu-soc-physical-ip-port-boundary` 全部内容, 实施时可并行推进

---

## 1. 总体架构:真实 dGPU 物理 RTL 对齐的 3 层拓扑

### 1.1 3 层架构总览 (per 提议 6 完全 JSON 拓扑化)

```
┌──────────────────────────────────────────────────────────────────────────┐
│ L1: PCIe 板级层 (frozen, host ↔ chip)                                    │
│   driver ←── 23 ABI ──→ DGpuBoard                                       │
│                              │                                           │
│                              ├─→ PcieEndpointIP (17 端口 PcieTlpBundle   │
│                              │      composite, frozen by Phase 4)       │
│                              │      内部含 PcieAxiAdapter (3 端口)      │
│                              │      ├─ axi_master_out (Axi4Bundle)      │
│                              │      ├─ axi_slave_in (Axi4Bundle)        │
│                              │      └─ cfg_slave_in (Axi4LiteBundle)    │
│                              │                                           │
│                              │  **本 change 新增**: EP 暴露 AXI 端口      │
│                              │  外部 SoC 视角 (per PcieAxiAdapter 既有)  │
└──────────────────────────────────────────────────────────────────────────┘
                                  │ AXI master + AXI slave + cfg AXI4-Lite
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ L2: SoC 内部 ControlHub (Chub, 本 change 新建)                          │
│   ControlHub (SimModule, ChStream 化):                                   │
│                                                                          │
│   上游接口:                                                              │
│     - axi_slave_in (Axi4Bundle) ←─ pcie_ep.axi_master_out                │
│     - cfg_slave_in (Axi4LiteBundle) ←─ pcie_ep.cfg_slave_out             │
│                                                                          │
│   内部 4 BAR decode + 路由:                                              │
│     - BAR0 (4KB MMIO)  → cr_master_out (CrBundle) → lwc_hub → gmmu.cr_in│
│     - BAR1 (16MB win)  → doorbell_out (DoorbellBundle) → lwc_hub → sdma │
│     - BAR2 (8GB vram)  → noc_master_out (NoCFlitBundle) → soc_noc → vram│
│     - BAR3 (隐含)      → (TBD, 预留)                                     │
│                                                                          │
│   MSI-X 聚合 (per Change E 准备):                                        │
│     - msix_in (MsixBundle) ←─ lwc_hub → msix_generator                   │
│                                                                          │
│   Error 聚合 (per Change F backlog):                                     │
│     - err_out (ErrBundle) → error_collector                              │
└──────────────────────────────────────────────────────────────────────────┘
                                  │ CrBundle + DoorbellBundle + NoCFlitBundle + MsixBundle
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ L3a: SoC 内部 LWC 总线 (per Change B 既有)                                │
│   LwcBusHub (32-port cap, 同步路由):                                     │
│     - register_cr_target("gmmu", &gmmu_->cr_in())                        │
│     - register_doorbell_target("sdma", &sdma_engine_->doorbell_in())     │
│     - (Phase 2) register_msix_target("msix_gen", ...)                    │
│                                                                          │
│   3 类 Bundle 语义:                                                      │
│     - CrBundle (8-field APB-lite)                                        │
│     - DoorbellBundle (4-field posted-write)                              │
│     - MsixBundle (vector+addr+data+masked)                               │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ L3b: SoC 内部 DataNoC (per physical-ip-port-boundary ChStream 化)         │
│   SocInternalNoc (mesh, ChStream 化):                                    │
│     - N 组 ChStreamPort (NoCFlitBundle 类型)                              │
│     - route_flit() 内部 mesh 路由                                        │
│     - vc_counts_[4] 计数器 (REQ/VC0, RSP/VC1, DAT/VC2, SNP/VC3)        │
│     - send_request/response API                                          │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ L4: SoC 内部 GPGPU 引擎 + 存储                                            │
│   SDMA Engine (8 端口):                                                  │
│     - axi_master_out (Axi4Bundle, 上游 PCIe AXI)                         │
│     - mem_in/mem_out (NoCFlitBundle, 下游 NoC)                           │
│     - translate_req/resp (TranslateReqBundle, SDMA→GMMU)                 │
│     - doorbell_in (DoorbellBundle, 接 LWC hub)                           │
│                                                                          │
│   GMMU (5 端口):                                                         │
│     - translate_in/resp (TranslateReqBundle, 接 SDMA)                    │
│     - req_out/resp_in (NoCFlitBundle, GMMU dedicated client → VramCtrl) │
│     - cr_in (CrBundle, 接 Chub/LWC, PT_BASE reg)                        │
│                                                                          │
│   VramController (上游 NoC 化, 内部 CacheReqBundle 协议真实):            │
│     - req_in_/req_out_/resp_in_/resp_out_ (上游, NoCFlitBundle)          │
│     - req_internal/resp_internal (内部, CacheReqBundle)                  │
│     - handle_functional_request() (C++ API, 保留)                        │
│     - set_backing_view() (backdoor, 保留)                                │
│                                                                          │
│   vram_storage_ (8GB unique_ptr, DGpuBoard 持有, ADR-DGPU-05)           │
└──────────────────────────────────────────────────────────────────────────┘
```

### 1.2 提议前后对比

| 维度 | 提议前 (HEAD `12cc50de`) | 提议后 (本 change 目标) |
|------|-------------------------|------------------------|
| **L1 PCIe 板级** | 17 端口 PcieTlpBundle composite | 17 端口 PcieTlpBundle composite (frozen) + PcieAxiAdapter 3 端口 (AXI master/slave/cfg) 暴露 |
| **L2 SoC 内部** | ❌ **缺失 ControlHub** (LwcBusHub + PcieControllerIP 分裂承担, 6 处 programmatic bridge) | ✅ **新建 ControlHub (Chub) 集中** (4 BAR decode + MSI-X 聚合 + Error 聚合) |
| **L3a LWC 总线** | LwcBusHub (32-port, 同步路由) | 保留 (Chub 接入) |
| **L3b DataNoC** | SocInternalNoc (callback 化) | ChStream 化 (per physical-ip-port-boundary) |
| **L4 GPGPU + 存储** | SDMA 5 端口 PcieTlpBundle + GMMU 2 端口 PcieTlpBundle + VramCtrl 继承 MemoryTLM | SDMA 8 端口 (AXI + NoC + translate + doorbell) + GMMU 5 端口 (translate + NoC + CrBundle) + VramCtrl 上游 NoC 化 + 内部 Cache 协议真实 |
| **外观层 PcieMemoryDevice** | 独立模块 (BAR2 8GB vram aperture) | **移除** (per 提议 5) |
| **JSON connection 数** | 1 条 | 13 条 |
| **Programmatic bridge 数** | 6 处 | **0 处** (全部 JSON 化) |

---

## 2. ControlHub (Chub) 详细设计 (本 change 核心新增)

### 2.1 类设计

```cpp
// include/tlm/cluster/control_hub.hh
class ControlHub : public SimModule {
public:
    // ========== 端口常量 (per design §1.1) ==========
    static constexpr unsigned NUM_PORTS = 6;  // 6 类端口

    // ========== 上游接口 (从 PCIe EP 接收) ==========
    // port 0: axi_slave_in (Axi4Bundle, master 端 PCIe 写入)
    cpptlm::InputStreamAdapter<bundles::Axi4Bundle> axi_slave_in;

    // port 1: cfg_slave_in (Axi4LiteBundle, master 端 PCIe cfg 写入)
    cpptlm::InputStreamAdapter<bundles::Axi4LiteBundle> cfg_slave_in;

    // ========== 下游控制面接口 (Chub 是 master) ==========
    // port 2: cr_master_out (CrBundle, master 端 GMMU reg 读写)
    cpptlm::OutputStreamAdapter<bundles::CrBundle> cr_master_out[NUM_CR_TARGETS];

    // port 3: doorbell_out (DoorbellBundle, master 端 SDMA ring submit)
    cpptlm::OutputStreamAdapter<bundles::DoorbellBundle> doorbell_out;

    // port 4: noc_master_out (NoCFlitBundle, master 端 NoC 推 data)
    cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle> noc_master_out;

    // ========== 下游接收接口 (Chub 是 slave) ==========
    // port 5: msix_in (MsixBundle, slave 接收 MSI-X 中断聚合, per Change E)
    cpptlm::InputStreamAdapter<bundles::MsixBundle> msix_in;

    // port 6 (预留): err_out (ErrBundle, master 端 Error 聚合, per Change F)
    cpptlm::OutputStreamAdapter<bundles::ErrBundle> err_out;

    // ========== ChStream 化 (per include/core/sim_module.hh 既有模式) ==========
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override;
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override;
    unsigned num_ports() const override { return NUM_PORTS; }

    // ========== 4 BAR decode 路由 (per PcieControllerIP::match_bar 既有逻辑迁移) ==========
    enum class BarTarget {
        Bar0Cr,           // BAR0 4KB MMIO → CrBundle
        Bar1Doorbell,     // BAR1 16MB window → DoorbellBundle
        Bar2VramAperture, // BAR2 8GB vram → NoCFlitBundle
        Bar3Unknown,      // BAR3 (预留)
    };

    // BAR decode 函数 (per PcieControllerIP::match_bar 既有 verdict 风格)
    BarTarget decode_bar(uint8_t bar, uint64_t offset, uint64_t& out_sub_offset) noexcept;

    // tick 路由
    void tick() override;

    // 内部 4 BAR decode state machine
    void route_bar0_cr(const bundles::Axi4Bundle& req);
    void route_bar1_doorbell(const bundles::Axi4Bundle& req);
    void route_bar2_vram(const bundles::Axi4Bundle& req);

    // ========== BAR 容量 (per dgpu_soc_minimal_v1.json bar_sizes) ==========
    static constexpr uint64_t kBar0Size = 4096;            // 4KB MMIO
    static constexpr uint64_t kBar1Size = 16 * 1024 * 1024; // 16MB window
    static constexpr uint64_t kBar2Size = 8ULL * 1024 * 1024 * 1024; // 8GB vram

private:
    // 4 BAR base/size 数组 (per dgpu_soc_minimal_v1.json bar_bases/bar_sizes)
    std::array<uint64_t, 4> bar_bases_;
    std::array<uint64_t, 4> bar_sizes_;

    // 内部 MSI-X 聚合 state (per Change E 准备)
    // 内部 Error 聚合 state (per Change F backlog)
};
```

### 2.2 4 BAR decode 路由表 (per PcieControllerIP::match_bar 既有 + BAR2 改走 NoC)

| BAR | 容量 | offset 范围 | 路由目标 | 转换层 |
|-----|------|------------|---------|--------|
| **BAR0** | 4KB | [0, 4KB) | GMMU reg 读写 → `cr_master_out[0]` → LwcBusHub → `gmmu.cr_in` | `Axi4Lite → CrBundle` (Chub 内部) |
| **BAR1** | 16MB | [0, 16MB) | SDMA ring submit → `doorbell_out` → LwcBusHub → `sdma.doorbell_in` | `Axi4 → DoorbellBundle` (Chub 内部) |
| **BAR2** | 8GB | [0, 8GB) | vram aperture → `noc_master_out` → SocInternalNoc → VramController | `Axi4 → NoCFlitBundle` (Chub 内部) |
| **BAR3** | (预留) | - | (TBD) | (预留) |

### 2.3 BAR decode 函数 (per `PcieControllerIP::match_bar` 既有 verdict 风格)

```cpp
ControlHub::BarTarget ControlHub::decode_bar(uint8_t bar, uint64_t offset,
                                              uint64_t& out_sub_offset) noexcept {
    if (bar >= 4) return BarTarget::Bar3Unknown;
    const uint64_t base = bar_bases_[bar];
    const uint64_t size = bar_sizes_[bar];
    if (offset >= size) return BarTarget::Bar3Unknown;

    out_sub_offset = offset;  // BAR 内 offset 直通
    switch (bar) {
        case 0: return BarTarget::Bar0Cr;
        case 1: return BarTarget::Bar1Doorbell;
        case 2: return BarTarget::Bar2VramAperture;
        case 3: return BarTarget::Bar3Unknown;  // (TBD)
    }
    return BarTarget::Bar3Unknown;
}
```

**关键约束** (per AGENTS.md KEY INVARIANTS "PcieControllerIP::match_bar whole-BAR 锁定"):
- **whole-BAR 锁定**: BAR 内不做 offset 子分发, offset 整体透传
- **4 枚举 verdict** 保持 (与 `PcieControllerIP::match_bar` 既有 4 枚举一致)
- **decode 属 Chub 内部**, 与 `PcieControllerIP` facade 角色**互补** (Chub 是 master, PcieControllerIP 是 facade)

### 2.4 Chub 内部转换层 (3 个, 全部 Chub 内部)

| 转换 | 输入 Bundle | 输出 Bundle | 字段映射 |
|------|------------|------------|---------|
| **`Chub_Axi4_To_CrAdapter`** (BAR0) | `Axi4Bundle` (aw channel) | `CrBundle` (8-field APB-lite) | `awaddr → cr_addr`, `awid → cr_id`, `wdata → cr_wdata`, `awlen/awsize → cr_size`, `awburst → cr_burst` |
| **`Chub_Axi4_To_DoorbellAdapter`** (BAR1) | `Axi4Bundle` (aw channel) | `DoorbellBundle` (4-field posted-write) | `awaddr → db_addr`, `wdata → db_wdata`, `awid → db_id`, `awlen=0 → db_posted=1` |
| **`Chub_Axi4_To_NocAdapter`** (BAR2) | `Axi4Bundle` (aw/ar channel) | `NoCFlitBundle` (16-field v2.1) | `awaddr/araddr → noc.address`, `awid/arid → noc.transaction_id`, `wdata/rdata → noc.data`, `awsize/arsize → noc.size`, `awburst/arburst → noc.burst`, `is_write = true` (aw) |

### 2.5 Chub 实施要点

- **ChStream 化**: 加 ChStreamPort 字段 + `set_stream_adapter()` override + `num_ports() = 6` (per `include/core/sim_module.hh:132-161` 既有模式)
- **AXI slave 接收**: `axi_slave_in` 端口接收 PCIe axi_master 写入, 通过 StreamAdapter 桥接到 `CrBundle/DoorbellBundle/NoCFlitBundle`
- **cfg_slave 接收**: `cfg_slave_in` 端口接收 PCIe cfg_master 写入, 直接转发到 PcieConfigSpace (per `PcieEndpointIP` 内部既有 config space)
- **下游 master 发送**: 3 类 master 端口 (`cr_master_out` + `doorbell_out` + `noc_master_out`) + 1 类 slave 端口 (`msix_in`) + 1 预留 (`err_out`)
- **状态机**: 4 BAR decode 路由状态机 + MSI-X 聚合 state + Error 聚合 state

### 2.6 Chub 与现有模块的边界

| 现有模块 | Chub 化后关系 | 备注 |
|---------|-------------|------|
| `PcieEndpointIP` | **保留** (frozen by Phase 4 桥接) | EP 内部 PcieAxiAdapter 3 端口 (axi_master_out + axi_slave_in + cfg_slave_in) 直接接 Chub |
| `PcieControllerIP` | **保留** (4 BAR facade 角色保留) | PcieControllerIP::match_bar 4 枚举 verdict 保留作 facade 验证, **不再承担控制路由** (控制路由迁到 Chub) |
| `LwcBusHub` | **保留** (per Change B 既有) | LwcBusHub 内部 `register_cr_target("gmmu", &gmmu_->cr_in())` 保留, Chub.cr_master_out 推给 LwcBusHub |
| `SocInternalNoc` | **保留** (per physical-ip-port-boundary 切型) | ChStream 化 + 4 target programmatic bridge 取消, JSON `chub.noc_master_out → soc_noc.0` 直连 |
| `GpuInternalFabric` | **保留** (per physical-ip-port-boundary 切型) | ChStream 化, JSON `fabric.0 → soc_noc.2` 直连 (GMMU dedicated client 经 NoC mesh) |
| `VramControllerTLM` | **保留** (上游 NoC 化 per physical-ip-port-boundary) | `vram_ctrl.req_in_` (NoCFlitBundle) 接 SocInternalNoc mesh 路由 |
| `SdmaEngineTLM` | **保留** + 端口切型 (5 → 8) | per §3 |
| `GmmuTLM` | **保留** + 端口切型 (2 → 5) | per §4 |
| `PcieMemoryDevice` | **移除** (per §6) | - |

---

## 3. SdmaEngineTLM 端口切型详细设计 (per §2.2 提议)

### 3.1 端口拓扑 (5 → 8 端口)

| 端口 | 当前类型 | 目标类型 | 备注 |
|------|---------|---------|------|
| `axi_master_out` (port 0, **新**) | 缺失 | `Axi4Bundle` (AXI master, 上游 PCIe) | 替代旧 `desc_in/host_out/done_out` |
| `mem_in` (port 1) | `PcieTlpBundle` | `NoCFlitBundle` (下游 NoC) | per physical-ip-port-boundary |
| `mem_out` (port 2) | `PcieTlpBundle` | `NoCFlitBundle` (下游 NoC) | per physical-ip-port-boundary |
| `translate_req` (port 3, **新**) | 缺失 (programmatic `set_translate_cb`) | `TranslateReqBundle` (新) | SDMA→GMMU VA→PA 翻译请求 |
| `translate_resp` (port 4, **新**) | 缺失 | `TranslateRespBundle` (新) | GMMU→SDMA 翻译响应 |
| `doorbell_in` (port 5, **新**) | 缺失 | `DoorbellBundle` | LWC hub → SDMA ring submit |
| ~~`desc_in` (旧 port 0)~~ | `PcieTlpBundle` | **删除** (合并到 axi_master_out) | 旧 desc_in 是 PCIe TLP, 改为 AXI |
| ~~`host_out` (旧 port 3)~~ | `PcieTlpBundle` | **删除** (合并到 axi_master_out) | 旧 host_out 是 PCIe TLP, 改为 AXI |
| ~~`done_out` (旧 port 4)~~ | `PcieTlpBundle` | **删除** (合并到 axi_master_out 响应) | 旧 done_out 是 PCIe TLP, 改为 AXI |

### 3.2 TranslateReqBundle + TranslateRespBundle 字段 (新)

```cpp
// include/bundles/translate_bundles_tlm.hh
struct TranslateReqBundle : public bundle_base {
    ch_uint<64> iova;          // 输入虚拟地址 (per ARM SMMUv3 规范)
    ch_uint<32> size;          // 翻译长度
    ch_uint<8>  access_type;   // 读/写/原子
    ch_uint<8>  task_id;       // SDMA 任务 ID (用于响应匹配)
    ch_uint<16> request_id;    // 请求 ID (用于 OOO 匹配)
};

struct TranslateRespBundle : public bundle_base {
    ch_uint<64> paddr;         // 物理地址 (翻译成功)
    ch_uint<8>  status;        // OK / PERMISSION_FAULT / PAGE_FAULT
    ch_uint<8>  task_id;       // SDMA 任务 ID (回写)
    ch_uint<16> request_id;    // 请求 ID (回写)
};
```

**编译期 `BUNDLE_FIELD_WIDTH` 锁** (per `include/utils/port_contract_asserts.hh` 既有)

### 3.3 SDMA 状态机改造

**当前 SDMA 状态机** (per `sdma_engine_tlm.cc:233-256` `process_h2d`):
```cpp
void process_h2d() {
    // 1. desc_in 接收 PcieTlpBundle DMA descriptor
    // 2. translate_cb_(iova, ...) 调 programmatic callback 翻译
    // 3. mem_out 发 PcieTlpBundle VRAM 写
    // 4. mem_in 收 PcieTlpBundle VRAM 响应
    // 5. done_out 发 PcieTlpBundle completion
}
```

**改造后**:
```cpp
void process_h2d() {
    // 1. axi_master_out 接收 Axi4Bundle DMA descriptor (从 PCIe AXI slave)
    // 2. translate_req 发 TranslateReqBundle → gmmu.translate_in
    // 3. translate_resp 收 TranslateRespBundle ← gmmu.translate_resp
    // 4. mem_out 发 NoCFlitBundle VRAM 写 (经 SocInternalNoc mesh)
    // 5. mem_in 收 NoCFlitBundle VRAM 响应
    // 6. axi_master_out 发 Axi4Bundle completion (经 PCIe AXI slave → host)
}
```

### 3.4 删除的 programmatic bridge

- `sdma->set_translate_cb(lambda)` **已删** (per Change C T6, commit `0443e9ff`)
- `sdma->set_translate_target(gmmu_)` (per Change C T6) **删除**, 改 JSON `sdma.translate_req → gmmu.translate_in` 直连

---

## 4. GmmuTLM 端口切型详细设计 (per §3 提议)

### 4.1 端口拓扑 (2 → 5 端口)

| 端口 | 当前类型 | 目标类型 | 备注 |
|------|---------|---------|------|
| `translate_in` (port 0, **新**) | 缺失 (programmatic `set_translate_target`) | `TranslateReqBundle` (新) | 上游 SDMA translate slave, JSON 直连 |
| `translate_resp` (port 1, **新**) | 缺失 | `TranslateRespBundle` (新) | 下游 GMMU→SDMA 翻译响应 |
| `req_out` (port 2) | `PcieTlpBundle` | `NoCFlitBundle` (GMMU dedicated client) | 下游 NoC 侧, PTE fetch 直连 VramController (per Gap I 修复) |
| `resp_in` (port 3) | `PcieTlpBundle` | `NoCFlitBundle` | 下游 NoC 侧, PTE fetch 响应回 GMMU |
| `cr_in` (port 4, **新**) | 缺失 (programmatic `gmmu_->set_pt_base_*()` 已删) | `CrBundle` (8-field APB-lite) | 上游 Chub/LWC 控制面, PT_BASE 寄存器读写 (per Change B 既有) |
| ~~`mem_view_` (programmatic)~~ | (callback) | **保留** (timing-mode H-C6 不动) | timing-mode 走 mem_view_/translate_timing TLB 路径 |

### 4.2 GMMU 状态机改造

**当前 GMMU 状态机** (per `gmmu_tlm.cc:131-193` `translate`):
```cpp
TranslateResult translate(uint64_t iova, uint32_t size, bool is_write) {
    // 1. mem_view_ check (programmatic, timing-mode H-C6)
    // 2. vram_ctrl_ check (programmatic, functional-mode 装配)
    // 3. handle_functional_request(pte_addr, 8, false, nullptr) 直 memcpy
    // 4. return paddr
}
```

**改造后**:
```cpp
TranslateResult translate(uint64_t iova, uint32_t size, bool is_write) {
    // 1. mem_view_ check (programmatic, 保留作 timing-mode H-C6 fallback)
    // 2. translate_in 接收 TranslateReqBundle (上游 SDMA)
    // 3. req_out 发 NoCFlitBundle (GMMU dedicated client → SocInternalNoc → VramController)
    // 4. resp_in 收 NoCFlitBundle (PTE fetch 响应)
    // 5. translate_resp 发 TranslateRespBundle (paddr + status)
}
```

### 4.3 删除的 programmatic bridge

- `gmmu_->set_translate_target(sdma_)` (per Change C T6) **删除**, 改 JSON 直连
- `gmmu_->set_vram_target(vram_ctrl)` (per Change C T4) **删除**, 改 JSON `gmmu.req_out → soc_noc.3 → soc_noc.1 → vram_ctrl.req_in` 直连

---

## 5. VramControllerTLM 端口切型详细设计 (per physical-ip-port-boundary)

### 5.1 端口拓扑 (1 组 4 端口 → 1 组 4 端口, 类型变更)

| 端口 | 当前类型 | 目标类型 | 备注 |
|------|---------|---------|------|
| `req_in_` (上游) | `CacheReqBundle` (继承 MemoryTLM) | `NoCFlitBundle` (上游 NoC 化) | 接受 SDMA/GMMU/Chub 经 SocInternalNoc mesh 来的请求 |
| `req_out_` (上游) | `CacheReqBundle` | `NoCFlitBundle` | 同上 (注: VramController 是 slave, req_out_ 是 response 路径) |
| `resp_in_` (上游) | `CacheRespBundle` | `NoCFlitBundle` | 同上 |
| `resp_out_` (上游) | `CacheRespBundle` | `NoCFlitBundle` | 同上 |
| `req_internal/resp_internal` (内部) | `CacheReqBundle/CacheRespBundle` | **保留** | 内部 MemoryTLM 行缓冲 + 协议状态机 |

### 5.2 4 个 wire-format adapter (per physical-ip-port-boundary)

| Adapter | 输入 | 输出 | 位置 |
|---------|------|------|------|
| `VramCtrl_Noc_To_AxiAdapter` | `NoCFlitBundle` (上游 req_in_) | `CacheReqBundle` (内部) | VramController 入口侧 |
| `VramCtrl_Axi_To_NocAdapter` | `CacheRespBundle` (内部) | `NoCFlitBundle` (上游 resp_out_) | VramController 出口侧 |

**保留 C++ API**:
- `handle_functional_request()` 保留 (per Change A) — 接受 Chub/SDMA/GMMU 委托的 functional request
- `set_backing_view()` 保留 (per ADR-DGPU-10) — host 特权 + internal IP 直访 backdoor
- `set_mode()` 保留 (per ADR-DGPU-11) — functional/timing 模式切换

---

## 6. PcieMemoryDevice 移除详细设计 (per 提议 5)

### 6.1 当前承担 2 件事

1. **BAR2 8GB vram aperture 外观层**: driver 通过 23 ABI 的 `cpptlm_emulator_dma_*` 系列访问 vram
2. **SDMA bulk DMA 委托 VramController functional port**: `handle_slave_port(port=0)` → `vram_controller_->handle_functional_request()`

### 6.2 移除后路径

#### 路径 ① — driver BAR2 写 vram (per 提议 5)

```
driver 写 vram:
  driver → cpptlm_emulator_dma_write(iova, data)
    → DGpuBoard::mmio_write(bar=2, offset, data)
    → Chub.axi_slave_in (Axi4Bundle) ←─ pcie_ep.axi_master_out (PcieAxiAdapter)
        ↓ (Chub 内部 BAR2 decode → 走 noc_master_out 路径)
    → Chub.route_bar2_vram(req)
        ↓ 字段转换: Axi4 → NoCFlitBundle (per Chub 内部 Chub_Axi4_To_NocAdapter)
    → Chub.noc_master_out (NoCFlitBundle)
    → [JSON: chub.noc_master_out → soc_noc.0]  (StreamAdapter: Chub_Axi4_To_NocAdapter)
    → SocInternalNoc.req_in_[0] → mesh 路由 → SocInternalNoc.req_out_[1]
    → [JSON: soc_noc.1 → vram_ctrl.req_in]  (StreamAdapter: VramCtrl_Noc_To_AxiAdapter)
    → VramControllerTLM.req_in_ (NoCFlitBundle 上游)
        ↓ 内部转换: noc_flit_to_cache_req()
    → VramControllerTLM.req_internal (CacheReqBundle 内部协议真实)
        ↓ 行缓冲 + MemoryTLM 协议状态机
    → handle_functional_request() (per Change A 既有)
    → memcpy ↔ vram_storage_ (8GB unique_ptr, DGpuBoard 持有, per ADR-DGPU-05)
```

#### 路径 ② — SDMA bulk DMA (与路径 ① 经同一 NoC + VramController 通道)

```
SDMA H2D bulk DMA:
  sdma.mem_out (NoCFlitBundle, port 2, egress)
    → [JSON: sdma.2 → soc_noc.X]  (StreamAdapter: SDMA 端口切型后)
    → SocInternalNoc.req_in_[X] → mesh 路由 → SocInternalNoc.req_out_[1]
    → vram_ctrl.req_in_ (NoCFlitBundle)
    → handle_functional_request() → vram_storage_
```

**两条路径经同一 NoC + VramController 通道** — driver BAR2 与 SDMA 视角**完全统一**。

### 6.3 测试迁移

| 旧标签 | 旧 cases | 新标签 | 新 cases |
|--------|---------|--------|---------|
| `[pcie-memory]` | 17 | `[pcie][bar2]` | 8 |
| `[pcie-memory]` | 17 | `[soc_noc][bar2]` | 4 |
| `[pcie-memory]` | 17 | `[chub][bar2]` | 5 |
| 总计 | 17 | 总计 | 17 (拆分到 3 个新标签, 全过) |

### 6.4 23 ABI 函数 0 diff

- `cpptlm_emulator_dma_read/write` (`include/abi/cpptlm_emulator.h`): 0 diff
- BAR2 路径内部从 PcieMemoryDevice 委托 → Chub/NoC 委托, **ABI 入口不变**
- driver 通过 23 ABI 看到的 BAR2 行为完全一致 (round-trip byte-level match)

---

## 7. 完整 JSON 拓扑 (per 提议 6)

### 7.1 目标 `configs/dgpu_soc_minimal_v1.json` (1 → 13 条 connection)

```json
{
  "name": "dgpu_soc_minimal_v1",
  "description": "Minimal dGPU SoC v2.0 终态配置 (per cpptlm-dgpu-soc-control-hub 实施完成). 真实物理 RTL 拓扑对齐 (NVIDIA H100 / ARM CMN-700 / HBM3 Controller). 7 模块 + 13 条 JSON connection + 0 处 programmatic bridge.",
  "modules": [
    {
      "name": "soc",
      "type": "DGpuSoc",
      "modules": [
        { "name": "pcie_ep",   "type": "PcieEndpointIP" },
        { "name": "chub",      "type": "ControlHub" },
        { "name": "sdma",      "type": "SdmaEngineTLM" },
        { "name": "gmmu",      "type": "GmmuTLM" },
        { "name": "vram_ctrl", "type": "VramControllerTLM" },
        { "name": "lwc_hub",   "type": "LwcBusHub" },
        { "name": "soc_noc",   "type": "SocInternalNoc" }
      ],
      "connections": [
        // ========== L1 PCIe 板级 ↔ L2 Chub (AXI) ==========
        { "src": "pcie_ep.axi_master_out",   "dst": "chub.axi_slave_in",     "latency": 1 },
        { "src": "chub.axi_master_out",      "dst": "pcie_ep.axi_slave_in",   "latency": 1 },
        { "src": "pcie_ep.cfg_master_out",    "dst": "chub.cfg_slave_in",      "latency": 1 },

        // ========== L2 Chub 控制面 → L3a LWC 总线 ==========
        { "src": "chub.cr_master_out",        "dst": "lwc_hub.0",             "latency": 1 },
        { "src": "chub.doorbell_out",         "dst": "lwc_hub.1",             "latency": 1 },

        // ========== L3a LWC 总线 → L4 GMMU/SDMA 控制面 ==========
        { "src": "lwc_hub.cr_target_gmmu",    "dst": "gmmu.cr_in",            "latency": 1 },
        { "src": "lwc_hub.doorbell_target_sdma","dst":"sdma.doorbell_in",     "latency": 1 },

        // ========== L2 Chub 数据面 → L3b DataNoC (BAR2 8GB vram aperture) ==========
        { "src": "chub.noc_master_out",       "dst": "soc_noc.0",             "latency": 1 },

        // ========== L4 SDMA/GMMU 数据面 → L3b DataNoC ==========
        { "src": "sdma.mem_out",              "dst": "soc_noc.1",             "latency": 1 },
        { "src": "gmmu.req_out",              "dst": "soc_noc.2",             "latency": 1 },

        // ========== L3b DataNoC → L4 VramController ==========
        { "src": "soc_noc.3",                 "dst": "vram_ctrl.req_in",      "latency": 1 },

        // ========== L4 SDMA ↔ L4 GMMU (独立 translate) ==========
        { "src": "sdma.translate_req",        "dst": "gmmu.translate_in",     "latency": 1 },
        { "src": "gmmu.translate_resp",       "dst": "sdma.translate_resp_in","latency": 1 }
      ]
    }
  ]
}
```

### 7.2 13 条 connection 详细分类

| # | 分类 | 源端 | 目标端 | Bundle | Adapter |
|---|------|------|-------|--------|---------|
| 1 | PCIe ↔ Chub AXI master | pcie_ep.axi_master_out | chub.axi_slave_in | Axi4Bundle | (直通) |
| 2 | PCIe ↔ Chub AXI slave | chub.axi_master_out | pcie_ep.axi_slave_in | Axi4Bundle | (直通) |
| 3 | PCIe ↔ Chub cfg | pcie_ep.cfg_master_out | chub.cfg_slave_in | Axi4LiteBundle | (直通) |
| 4 | Chub → LWC CR | chub.cr_master_out | lwc_hub.0 | CrBundle | (直通) |
| 5 | Chub → LWC Doorbell | chub.doorbell_out | lwc_hub.1 | DoorbellBundle | (直通) |
| 6 | LWC → GMMU CR | lwc_hub.cr_target_gmmu | gmmu.cr_in | CrBundle | (直通) |
| 7 | LWC → SDMA Doorbell | lwc_hub.doorbell_target_sdma | sdma.doorbell_in | DoorbellBundle | (直通) |
| 8 | Chub → SoC NoC | chub.noc_master_out | soc_noc.0 | NoCFlitBundle | **Chub_Axi4_To_NocAdapter** (BAR2 内部) |
| 9 | SDMA → SoC NoC | sdma.mem_out | soc_noc.1 | NoCFlitBundle | (直通) |
| 10 | GMMU → SoC NoC | gmmu.req_out | soc_noc.2 | NoCFlitBundle | (直通) |
| 11 | SoC NoC → VramCtrl | soc_noc.3 | vram_ctrl.req_in | NoCFlitBundle | **VramCtrl_Noc_To_AxiAdapter** (入口) |
| 12 | SDMA → GMMU translate req | sdma.translate_req | gmmu.translate_in | TranslateReqBundle | (直通) |
| 13 | GMMU → SDMA translate resp | gmmu.translate_resp | sdma.translate_resp_in | TranslateRespBundle | (直通) |

### 7.3 programmatic bridge 全部退役

**DGpuBoard::bind_memory_backings() 简化** (per `dgpu_board_shell.cc:780-870`):
- 旧版 ~30 行
- 新版 ~3 行 (只保留 vram_ctrl set_backing_view + 可能的 LWC hub register_*, 但 LWC hub register 可改 JSON)

**退役的 6 处 programmatic bridge**:
1. `soc_noc->set_pcie_target(...)` → JSON `chub.noc_master_out → soc_noc.0` 替代
2. `soc_noc->set_sdma_target(...)` → JSON `sdma.mem_out → soc_noc.1` 替代
3. `soc_noc->set_gmmu_target(...)` → JSON `gmmu.req_out → soc_noc.2` 替代
4. `soc_noc->set_vram_target(...)` → JSON `soc_noc.3 → vram_ctrl.req_in` 替代
5. `fabric->set_vram_target(...)` (per physical-ip-port-boundary) → JSON `fabric.0 → soc_noc` 替代
6. `gmmu_->set_vram_target(vram_ctrl)` (per Change C T4) → JSON `gmmu.req_out → soc_noc → vram_ctrl.req_in` 替代

**已删除的 2 处 programmatic bridge** (per Change C T6):
- `sdma->set_translate_cb(lambda)` (commit `0443e9ff`)
- `sdma->set_translate_target(gmmu_)` (per Change C T6) → JSON `sdma.translate_req → gmmu.translate_in` 替代

**总**: 0 处 programmatic bridge + 13 条 JSON connection = **完全 JSON 化**

---

## 8. 测试覆盖 (per spec.md 5 ADDED Requirements)

### 8.1 新增测试 (估时 5-7d)

| 标签 | 数量 | 内容 |
|------|------|------|
| `[control_hub]` | 6+ cases | Chub 4 BAR decode + AXI/CR/Doorbell/NoC 3 类转换 + MSI-X 聚合 + Error 聚合 |
| `[translate]` | 4+ cases | SDMA↔GMMU translate 直连 (request/response/status 错误码) |
| `[pcie][bar2]` | 8 cases | driver BAR2 路径 (per 提议 5, 替代 [pcie-memory] 17) |
| `[soc_noc][bar2]` | 4 cases | BAR2 经 SoC NoC 路径 |
| `[chub][bar2]` | 5 cases | Chub BAR2 decode + 路由 |
| `[sdma][translate]` | 3 cases | SDMA translate 端口 |
| `[gmmu][translate]` | 2 cases | GMMU translate 端口 |
| `[port-typing]` | 6 cases | 4 个 adapter 双向 + edge cases (per physical-ip-port-boundary) |
| `[soc_noc][chstream]` | 4 cases | SocInternalNoc ChStream 化 |
| `[gpu_fabric][chstream]` | 4 cases | GpuInternalFabric ChStream 化 |
| `[sdma][noc-bridge]` | 3 cases | SDMA 端口切型 + adapter 注入 |
| `[vram][noc-bridge]` | 3 cases | VramController 端口切型 + adapter 注入 |
| `[gmmu][noc-bridge]` | 2 cases | GmmuTLM 端口切型 |

**总新增**: ≥52 cases

### 8.2 回归测试 (估时 3-5d)

| 标签 | 基线 | 目标 |
|------|------|------|
| `[pcie]` | 430/36672 | 0 回归 |
| `[gmmu]` | 25/5273 | 0 回归 |
| `[noc]` | 64/304 | 0 回归 |
| `[sdma]` | 68/12945 | 0 回归 |
| `[minimal_dgpu_soc]` | 24/170 | 0 回归 + E2E 直连 case |
| `[dgpu_soc_timing]` | 37/12024 | 0 回归 (timing-mode 不动) |
| `[driver_visible]` | 4/40 | 0 回归 |
| `[pcie-memory]` | 17/48 | **迁移到 [pcie][bar2] + [soc_noc][bar2] + [chub][bar2]**, 0 回归 |
| `[pcie-ep-soc-bridge]` | 8 cases (per p2-integration-unblock) | 0 回归 |

### 8.3 frozen headers + 23 ABI 0 diff 锁

- 4 frozen headers 0 行 diff
- 23 ABI 函数 0 增 0 改
- 实施期所有 `git diff include/abi/cpptlm_emulator.h` 必须为 0 行

---

## 9. 关键路径图 (目标态完整数据流, 13 条 connection)

```
driver ←── 23 ABI ──→ DGpuBoard
                       │
                       ├─→ PcieEndpointIP (17 端口 PcieTlpBundle, frozen)
                       │      │ 内部 PcieAxiAdapter (3 端口 AXI):
                       │      ├─ axi_master_out (Axi4Bundle) ─────────┐
                       │      ├─ axi_slave_in (Axi4Bundle) ←───────────┤
                       │      └─ cfg_master_out (Axi4LiteBundle) ──────┤
                       │                                                 │
                       │            ┌────────────────────────────────────┘
                       │            ▼
                       │   ControlHub (Chub, ChStream 化)
                       │      ├─ axi_slave_in 接收 (port 0)
                       │      ├─ cfg_slave_in 接收 (port 1)
                       │      ├─ 4 BAR decode + 路由:
                       │      │   - BAR0 → cr_master_out (CrBundle)
                       │      │   - BAR1 → doorbell_out (DoorbellBundle)
                       │      │   - BAR2 → noc_master_out (NoCFlitBundle)  ← [8]
                       │      │   - BAR3 → (预留)
                       │      ├─ cr_master_out → LwcBusHub → gmmu.cr_in    ← [4][6]
                       │      ├─ doorbell_out → LwcBusHub → sdma.doorbell_in ← [5][7]
                       │      ├─ noc_master_out → soc_noc.0                ← [8]
                       │      └─ axi_master_out → pcie_ep.axi_slave_in    ← [2]
                       │
                       │   LwcBusHub (32-port, 同步路由, per Change B)
                       │      ├─ register_cr_target("gmmu", &gmmu_->cr_in()) [6]
                       │      └─ register_doorbell_target("sdma", ...)   [7]
                       │
                       │   SocInternalNoc (mesh, ChStream 化)
                       │      ├─ soc_noc.0 接收 chub.noc_master_out        [8]
                       │      ├─ soc_noc.1 接收 sdma.mem_out              [9]
                       │      ├─ soc_noc.2 接收 gmmu.req_out              [10]
                       │      ├─ soc_noc.3 → vram_ctrl.req_in             [11]
                       │      └─ route_flit() 内部 mesh 路由
                       │
                       │   SdmaEngineTLM (8 端口, 端口切型)
                       │      ├─ axi_master_out (Axi4Bundle, port 0)
                       │      ├─ mem_in/mem_out (NoCFlitBundle, port 1/2) [9]
                       │      ├─ translate_req (TranslateReqBundle, port 3) [12]
                       │      ├─ translate_resp (TranslateRespBundle, port 4) [13]
                       │      └─ doorbell_in (DoorbellBundle, port 5)     [7]
                       │
                       │   GmmuTLM (5 端口, 端口切型)
                       │      ├─ translate_in (TranslateReqBundle, port 0) [12]
                       │      ├─ translate_resp (TranslateRespBundle, port 1) [13]
                       │      ├─ req_out/resp_in (NoCFlitBundle, port 2/3) [10]
                       │      └─ cr_in (CrBundle, port 4)                 [6]
                       │
                       │   VramControllerTLM (上游 NoC 化 + 内部 CacheReqBundle)
                       │      ├─ req_in_ (NoCFlitBundle 上游)            [11]
                       │      ├─ req_internal (CacheReqBundle 内部)
                       │      └─ handle_functional_request() → vram_storage_
                       │
                       ▼
                  vram_storage_ (8GB, DGpuBoard 持有, ADR-DGPU-05)
```

---

## 10. 实施期与归档 (per tasks.md)

### 10.1 8 sub-task 分解 (估时 21-30d)

- T1: Chub 新建 (5-7d) — 4 BAR decode + 3 类转换层 + 6 端口实现
- T2: SdmaEngineTLM 端口切型 (3-4d) — 5 → 8 端口 + TranslateBundle 接入
- T3: GmmuTLM 端口切型 (1-2d) — 2 → 5 端口 + translate slave + CrBundle slave
- T4: VramControllerTLM 端口切型 (3-4d) — per physical-ip-port-boundary
- T5: PcieMemoryDevice 移除 (1d) — 包含 [pcie-memory] 17 cases 迁移
- T6: 4 个新 wire-format adapter + TranslateBundle 新建 (3-5d, 与 T1-T5 并行)
- T7: 0 回归锁 + 13 条 JSON 直连 E2E (3-5d)
- T8: 实施笔记 + post-flight 验证 (1d)

### 10.2 归档

- spec 落盘到 `openspec/specs/dgpu-soc-control-hub/spec.md` (5 ADDED Requirements)
- 同步 spec 修改: `sdma-engine-tlm` / `gmmu-pte-wf` / `vram-controller-functional-mode` / `soc-internal-noc` / `lwc-bus-hub-mvp` / `dgpu-soc-physical-ip-port-boundary` (合并 superset)
- 提交 archive 命令: `openspec archive cpptlm-dgpu-soc-control-hub`
- 跟踪 proposal.md 从 index 移除
- KPI: 1 slot 释放 (给 Change F backlog)
- 同步 archive `cpptlm-dgpu-soc-physical-ip-port-boundary` (per 范围超集关系)

### 10.3 与 physical-ip-port-boundary 合并建议

**提议**: 实施前决定合并策略:

| 策略 | 优 | 劣 |
|------|----|----|
| **A: 保留 2 个独立 change** (推荐) | 实施灵活, 可并行推进; 评审独立 | 范围有重叠, 需协调依赖 |
| **B: 物理合并为 1 个 change** | 范围清晰, 实施统一 | 需删除 physical-ip-port-boundary 4 件套; KPI 占用合并计算 |

**本草案采用 A** (per doc-only cleanup 先例, 保留独立 4 件套), 由用户在 review 时决定。

---

## 11. 不在范围内 (per proposal.md "What Changes" 末)

- 23 ABI 函数: 零 diff
- 4 frozen headers: 零 diff
- PcieEndpointIP 17 端口: 保留
- PcieAxiAdapter 3 端口: 保留 (per PcieEndpointIP 内部)
- PCIe Controller ↔ host 边界: 保留 PcieTlpBundle
- timing-mode (dgpu_soc_timing_v1.json): 不动
- LWC 总线语义 (CrBundle/DoorbellBundle/MsixBundle): 不动 (per Change B 既有)
- Change E (MSI-X Generator IP): 独立 change, 本 change 准备接口 (Chub.msix_in)
- Change F (Error/CDC Bus): 独立 change, 本 change 准备接口 (Chub.err_out)
- LwcBusHub programmatic `register_*_target`: 保留 (本 change 不强求 JSON 化 LWC 总线)
- PcieControllerIP facade 角色: 保留 (4 BAR decode 验证 facade)
