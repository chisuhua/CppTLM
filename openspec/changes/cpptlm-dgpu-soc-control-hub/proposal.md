# cpptlm-dgpu-soc-control-hub: 新建 ControlHub(Chub)+ 移除 PcieMemoryDevice + 完全 JSON 拓扑化 (真实物理 IP RTL 对齐)

> **状态**: 🟡 Proposed Draft v0.5.0 (草稿, 2026-10-13 起草, 待评审)
> **作者**: CppTLM Team
> **版本**: v0.5.0 (初始草稿, P0 架构级, 与 `cpptlm-dgpu-soc-physical-ip-port-boundary` 起草**正交协同**)
> **关联 OpenSpec**:
>   - ✅ `cpptlm-vram-controller-functional-integration` archived (Change A, Phase 0)
>   - ✅ `cpptlm-lwc-bus-hub-mvp` archived (Change B, Phase 0)
>   - ✅ `cpptlm-gmmu-pte-bus-correction` archived (Change C, Phase 1)
>   - 🟡 `cpptlm-dgpu-soc-physical-ip-port-boundary` Proposed Draft (per commit `12cc50de`, 端口切型 + 4 adapter)
>   - 🟡 `cpptlm-p2-integration-unblock` Proposed (P2 EP↔SoC 4 方向 AXI 桥接, 与本 change 提议 1 深度协同)
>   - **业务定位锁定**: `docs/designs/dgpu-soc/architecture.md` §0.1 原则 3 (SoC 内部模块接口贴近真实物理 IP 边界) + D-AXI v1.5 B25 撤回后的真正落地方向
>
> **🔗 Cross-Reference (per 根 AGENTS.md P0-revision 2026-10-12)**:
> 旧版 MVP 临时方案 + `PcieMemoryDevice` 外观层 + 6 处 programmatic bridge **完全违背**真实 dGPU 物理 RTL 拓扑(NVIDIA H100 / ARM CMN-700 / Synopsys DW PCIe AXI Bridge / HBM3 Controller)。本 change 推动**真实物理 IP 边界 + JSON 完整拓扑**。
> **修复范围**: 新建 ControlHub(Chub)模块 + 移除 PcieMemoryDevice + 5 模块端口切型 + 4 个 wire-format adapter + SDMA/GMMU translate 独立接口 + 完全 JSON 拓扑化
> **优先级**: **P0** (架构根因级, 对齐真实 dGPU 物理 RTL 拓扑)
> **估时**: **21-30d** (Chub 新建 + 5 模块切型 + 4 adapter + PcieMemoryDevice 移除 + 回归测试)
> **依赖**: 全部 archived changes 已是 ✅;physical-ip-port-boundary + p2-integration-unblock 草稿就绪待协同
> **frozen headers (pcie_endpoint_tlm.h / pcie_display_device.hh / pcie_bundles_tlm.hh / cpptlm_emulator.h) 零 diff** (per ADR-DGPU-08 §2.3)
> **目标 tag**: `cpptlm-dgpu-soc-control-hub-completion`

---

## Why

### 1. 背景:当前 dGPU SoC 拓扑违背真实 dGPU 物理 RTL 设计

#### 1.1 当前实装 (per HEAD `12cc50de` 2026-10-13, 6 业务模块 + 2 基础设施 + 1 外观层)

```
PCIe 入口侧:
  pcie_ctrl   : PcieControllerIP  (4 BAR decode facade, 但无 AXI slave 输入)
  pcie_ep     : PcieEndpointIP    (17 端口 PcieTlpBundle composite, frozen by Phase 4)
  pcie_memory : PcieMemoryDevice  (BAR2 8GB vram aperture 外观层, 违背物理 IP)

GPGPU 引擎侧:
  sdma        : SdmaEngineTLM    (5 端口全 PcieTlpBundle, MVP 临时, 无独立 translate 端口)
  gmmu        : GmmuTLM          (1-level page table walker, programmatic bridge 走 set_vram_target)
  completion  : CompletionRingTLM (dormant, 等 Change E 实施)

存储 / 内存侧:
  vram_ctrl   : VramControllerTLM (继承 MemoryTLM, 上游 CacheReqBundle)

内部互联基础设施:
  soc_noc     : SocInternalNoc    (mesh DataNoC, 当前 callback 化 SimModule, 4 target programmatic bridge)
  lwc_hub     : LwcBusHub         (32-port cap, 同步路由, 当前 register_cr_target + register_doorbell_target programmatic)
```

**JSON 拓扑**: 1 条 connection (`sdma.2 → pcie_memory.0`)
**Programmatic bridge**: 6 处 (4 soc_noc target + 2 fabric 桥 + 2 lwc_hub register + gmmu set_vram_target + sdma set_translate_cb 已删)

#### 1.2 真实 dGPU 物理 RTL 拓扑(per Oracle RTL 调研, NVIDIA H100 / ARM CMN-700 / Synopsys DW PCIe AXI Bridge / HBM3 Controller)

```
host 板级 (PCIe TLP wire):
  driver ←──→ PCIe EP Controller (PcieEndpointIP)
                  │
                  ├── AXI master (EP→SoC 写/读, e.g. MSI-X TLP 注入, ATS translation 请求)
                  ├── AXI slave  (SoC→EP 写/读, e.g. BAR0/1/2/3 mmio 处理)
                  └── cfg AXI4-Lite slave (SoC→EP config space 访问)

SoC 内部 ControlHub (Chub, 树形/星形拓扑):
  上游:
    - AXI slave ←─ PCIe AXI master
    - cfg AXI4-Lite slave ←─ PCIe cfg_slave
  下游控制面 (4 BAR decode + 控制分发):
    - BAR0 → CrBundle (APB-lite) → GMMU reg_in (PT_BASE_LO/HI, GMMU_CTRL)
    - BAR1 → DoorbellBundle → SDMA ring consumer
    - MSI-X (PBA/TABLE) → MsixBundle → LWC hub → MsixGeneratorIP
    - ErrBundle (PCIe AER) → ErrorCollector (Phase 3 Change F)
  下游数据面 (NoC master):
    - NoCFlitBundle → SocInternalNoc (mesh DataNoC)

SoC 内部 DataNoC (mesh 拓扑, SocInternalNoc):
  - SDMA bulk DMA (sdma.mem_out NoCFlitBundle)
  - GMMU PTE fetch (gmmu.req_out NoCFlitBundle, dedicated memory client)
  - fabric IP 流量
  - Chub 数据面转发 (BAR2 8GB vram aperture 直走)

GPGPU 引擎:
  SDMA Engine:
    上游: AXI master (per physical IP, 接 PCIe AXI slave 端口)  ← **新提议**
    下游: NoC master (SDMA bulk DMA, NoCFlitBundle)
    独立: translate_req/resp 接口 (SDMA→GMMU VA→PA 翻译)
  GMMU:
    上游: translate_in 接口 (接 SDMA translate_req)
    下游: NoC master (GMMU PTE fetch, dedicated memory client)
    接口: CrBundle slave (PT_BASE reg 读/写, 接 Chub 控制面)
  HBM Controller (VramControllerTLM):
    上游 NoC (NoCFlitBundle)
    内部 CacheReqBundle 协议真实 (MemoryTLM 行缓冲 + 协议状态机)
    backdoor 接口 (host 特权 + internal IP 直访)
```

#### 1.3 当前实装违背真实 RTL 的 6 个点

| 违背点 | 当前实装 | 真实 RTL | 影响 |
|--------|---------|---------|------|
| **1. PCIe soc 侧无 AXI master/slave 端口** | PcieEndpointIP 17 端口全 PcieTlpBundle composite, SoC 侧无 AXI 边界 | PcieAxiAdapter 已持有 Axi4StreamAdapter 3 端口 (axi_master_out + axi_slave_in + cfg_slave_in, per `pcie_axi_adapter_tlm.hh:32-40`) **但** EP 17 端口是 Phy/Link/Mux/FC 内部, **外部 SoC 视角** 应是 AXI master/slave | 拓扑混杂 PCIe 内部 + 外部 SoC 协议 |
| **2. 无 ControlHub 集中控制分发** | LwcBusHub + PcieControllerIP 分裂承担 4 BAR decode + 控制路由, 6 处 programmatic bridge | 真实 dGPU 都有 ControlHub 集中处理 BAR decode + MSI-X 聚合 + Error 聚合 | 拓扑碎片化, JSON 无法完整声明 |
| **3. SDMA 缺独立 translate 端口** | sdma→gmmu 通过 programmatic `set_translate_cb`/`translate_target_` 桥接 | 真实 SDMA 有专用 translate_req/resp 接口 (e.g. ARM SMMU 翻译请求) | 违背物理 IP, programmatic 注入 |
| **4. SDMA 5 端口全 PcieTlpBundle (MVP 临时)** | desc_in/host_out/done_out/mem_in/mem_out 全 PcieTlpBundle | SDMA 上游 AXI master (接 PCIe) + 下游 NoC master (bulk DMA) | wire-format 错配 |
| **5. GMMU 走 programmatic bridge 访问 VRAM** | `gmmu_->set_vram_target(vram_ctrl)` C++ 端 set 指针 | GMMU 应有 NoC master 接口 (PTE fetch, dedicated memory client) | 违背物理 IP, programmatic 注入 |
| **6. PcieMemoryDevice 外观层冗余** | 单独 PcieMemoryDevice 模块承担 BAR2 8GB vram aperture 外观 + SDMA bulk DMA 委托 | 真实 dGPU 没有 "PCIe Memory Device" 模块 — driver BAR2 mmio 走 PCIe AXI slave → Chub → NoC → HBM Controller 完整路径 | 多余模块, 拓扑复杂, JSON 声明不闭环 |

### 2. 根因:D-AXI v1.5 B25 撤回后没有真正落地 AXI/NoC 类型 + PcieMemoryDevice 是 MVP 临时外观层

**ADR-DGPU-06** 标题就是"axi-mem-bundle-boundary" (chip-internal AXI vs board-level PCIe 边界严格分离),但**实装时**: SDMA 5 端口 + GMMU req_out_ + VramController 端口 + GpuInternalFabric 内部 wire **全 PcieTlpBundle** (MVP 临时方案),违背该 ADR。

**D-AXI v1.5 B25** (per AGENTS.md KEY INVARIANTS): "撤回 v1.3 B2 SDMA mem_in/mem_out → AxiMemBundle 切型决策 (避免异构 multi-port adapter 框架限制); D3+ 演进路径待 Change C/D3 协同设计" — **B25 只是撤回切型决策, 并未真正落地 AXI/NoC 类型**。

**PcieMemoryDevice** 是 D-AXI v1.4 (2027-09-26) 引入的"PCIe 外观层",per ADR-DGPU-05 v1.4 B7 决策保留 — 但**真实 dGPU 物理 RTL 没有这个模块**,它是 MVP 简化产物。提议 5 立即移除,让 driver BAR2 路径与 SDMA bulk DMA 路径**完全统一**走 Chub→NoC→VramController。

### 3. 后果(已观察到的 3 类问题)

1. **e1 v1.1 反复修订** (per `cpptlm-dgpu-soc-internal-noc-e1` v1.1): 4 P0 + 3 CRITICAL + 3 MAJOR 大量工作是因为"SDMA 发 PcieTlpBundle vs NoC 内部 NoCFlitBundle vs VramCtrl 收 CacheReqBundle"三方类型不一致
2. **Change C Gap G1 反复挣扎** (per `docs/implementation/2026-10-09-gmmu-pte-bus-correction.md` §6 Deviation 1): GMMU programmatic bridge + VramController functional port 是 workaround
3. **p2-integration-unblock 4 断点** (per `openspec/changes/cpptlm-p2-integration-unblock/design.md` §1.1): EP↔SoC 4 方向 AXI 数据流只有 D1/D2 ✅, D3/D4 ❌, 需 Axi4CacheAdapter 桥接
4. **JSON 拓扑不完整**: 1 条 connection + 6 处 programmatic bridge, 与 `dgpu_board_v1.json:80-94` (SimModule 内部完整 connection) 模式严重不一致

### 4. 物理 IP 对应表(本 change 推动的真实对齐)

| 物理 IP wire 类型 | 当前 CppTLM | 目标 CppTLM (本 change) |
|------------------|------------|----------------------|
| **NVIDIA H100 SDMA 上游** (AXI4 master) | PcieTlpBundle (MVP 临时) | **Axi4Bundle** (master 侧, 接 PCIe axi_slave) |
| **NVIDIA H100 SDMA 下游** (NoC master) | PcieTlpBundle (MVP 临时) | **NoCFlitBundle** (接 SocInternalNoc) |
| **NVIDIA H100 SDMA translate** (独立 req/resp) | programmatic `set_translate_cb` | **新 TranslateReqBundle + TranslateRespBundle** |
| **ARM CMN-700 mesh NoC** (NoC flit) | NoCFlitBundle 内部 | NoCFlitBundle 内部 (保留) |
| **HBM3 Controller 上游** (NoC slave + AXI-like 内部) | CacheReqBundle | **NoCFlitBundle 上游 + CacheReqBundle 内部协议真实** |
| **NVIDIA H100 GMMU** (NoC master + CrBundle slave) | programmatic bridge | **NoCFlitBundle (PTE fetch) + CrBundle (reg_in)** |
| **Synopsys DW PCIe AXI Bridge** (ControlHub) | 缺失 (LwcBusHub + PcieControllerIP 分裂) | **新建 ControlHub (Chub) 集中** |
| **PCIe EP ↔ host 板级** (PCIe TLP) | PcieTlpBundle (frozen) | **保留** |
| **PCIe Memory Device** (外观层) | 独立 PcieMemoryDevice 模块 | **移除** (per 提议 5) |

### 5. 4 原则 (per `docs/designs/dgpu-soc/architecture.md` §0.1 锁定)

- **原则 1** driver-visible SoC 拓扑: 本 change 推动**真实物理 IP 边界 + JSON 完整拓扑**, driver 视角闭环不变
- **原则 2** chip-level main memory by DGpuBoard: `vram_storage_` 所有权不动, 本 change 只改 wire-format
- **原则 3** SoC 内部模块接口贴近真实物理 IP 边界 + 双重无注入约束: **本 change 直接落地此原则** (移除所有 programmatic bridge, 全部 JSON 化)
- **原则 4** VramControllerTLM 单独支持 functional-mode + backdoor: `set_mode(FunctionalZeroDelay)` API 不动, 本 change 只改 wire-format

### 6. 与 physical-ip-port-boundary change 的正交协同关系

**本 change (Chub) vs physical-ip-port-boundary**:

| 维度 | physical-ip-port-boundary | 本 change (Chub) |
|------|--------------------------|------------------|
| **范围** | 5 模块端口切型 + 4 adapter + 2 SimModule ChStream 化 | **新建 Chub + 移除 PcieMemoryDevice + SDMA 独立 translate + 完全 JSON 拓扑** |
| **PcieMemoryDevice** | 保留 (backward compat) | **移除** |
| **PcieControllerIP** | 保留 (callback 化 SimModule ChStream 化) | **保留 facade 角色, 4 BAR decode 逻辑迁出到 Chub** |
| **SDMA 端口数** | 5 → 5 (2 PCIe + 3 NoC 二分) | **5 → 6** (新增 translate_req/resp) |
| **LwcBusHub** | 保留 | **保留** (Chub 的 lwc_out 接入 LwcBusHub) |
| **SocInternalNoc** | ChStream 化 (callback 取消) | **保留 ChStream 化结果** |
| **JSON connection 数** | 1 → 5 | **1 → 13** (per §7.1) |
| **可并行?** | ✅ | ✅ (本 change 范围更广, 包含前者) |
| **依赖顺序** | 独立 | 可与 physical-ip-port-boundary **并行实施** (本 change 范围超集) |
| **合并建议** | 单独 change | **可合并为 1 个 change (本 change superset)** |

**提议**: 本 change 与 physical-ip-port-boundary **合并为 1 个 change**, 因范围超集。命名建议: `cpptlm-dgpu-soc-physical-ip-rtl-topology` (覆盖更准确)。

---

## What Changes

### 新增 ① — `ControlHub` (Chub) 模块

**位置**: `include/tlm/cluster/control_hub.{hh,cc}` + `src/tlm/cluster/control_hub.cc`

**基类**: `SimModule` (ChStream 化, per `include/core/sim_module.hh` 既有 SimModule ChStream 化模式, 类似 `GpuInternalFabric`)

**接口拓扑** (per 提议 2 + 真实 RTL 物理 IP):

| 端口 | 类型 | 方向 | Bundle | 对接目标 |
|------|------|------|--------|---------|
| `axi_slave_in` | AXI slave | 接收 | `Axi4Bundle` | `pcie_ep.axi_master_out` (per PcieAxiAdapter 既有) |
| `cfg_slave_in` | AXI4-Lite slave | 接收 | `Axi4LiteBundle` | `pcie_ep.cfg_slave_out` (per PcieAxiAdapter 既有) |
| `cr_master_out` | CrBundle master | 发送 | `CrBundle` (8-field APB-lite) | `lwc_hub` → `gmmu.cr_in` (reg read/write) |
| `doorbell_out` | DoorbellBundle master | 发送 | `DoorbellBundle` (4-field posted-write) | `lwc_hub` → `sdma.doorbell_in` (ring submit) |
| `noc_master_out` | NoC master | 发送 | `NoCFlitBundle` (16-field v2.1) | `soc_noc.0` (BAR2 8GB vram aperture data path) |
| `msix_in` | MsixBundle slave | 接收 | `MsixBundle` (vector+addr+data+masked) | `lwc_hub` → `msix_generator` (per Change E) |
| `err_out` | ErrBundle master | 发送 | `ErrBundle` (err_pin+severity+ext_data) | `error_collector` (Phase 3 Change F backlog) |

**内部职责**:
- **4 BAR decode 路由** (per `PcieControllerIP::match_bar` 既有逻辑迁移):
  - BAR0 (4KB MMIO) → `cr_master_out` → `lwc_hub` → `gmmu.cr_in`
  - BAR1 (16MB window) → `doorbell_out` → `lwc_hub` → `sdma.doorbell_in`
  - BAR2 (8GB vram aperture) → `noc_master_out` → `soc_noc` → `vram_ctrl`
  - BAR3 (隐含, per ADR-DGPU-09 4 BAR scope) → (TBD, 预留)
- **MSI-X 中断聚合** → `msix_in` 接收 + 内部聚合 → (per Change E 准备)
- **Error Aggregation** → `err_out` 发送 → (per Change F backlog)

**新增 Bundle** (per Chub 端口类型):
- `RegMasterBundle` (可选, **建议复用 CrBundle** 避免 Bundle 数量爆炸)
- `MsixBundle` (per Change B 已有, 复用)
- `ErrBundle` (per Change F backlog 待起草)

### 切型 ② — `SdmaEngineTLM` 端口按"上游 PCIe AXI master + 下游 NoC + 独立 translate"三分

| 端口 | 当前类型 | 目标类型 | 理由 |
|------|---------|---------|------|
| `axi_master_out` (port 0, **新**) | 缺失 | **`Axi4Bundle`** (AXI master) | 上游 PCIe 侧, SDMA 经 PCIe AXI slave 接收 host 端 DMA descriptor (per 真实 RTL AXI master 发起) |
| `desc_in` (port 0, 旧) | `PcieTlpBundle` | **删除** (合并到 `axi_master_out`) | desc_in 是 host 端 DMA descriptor 走 PCIe AXI master 接收, 不再是 PcieTlpBundle |
| `mem_in` (port 1) | `PcieTlpBundle` | **`NoCFlitBundle`** | 下游 NoC 侧, VRAM 读响应 (D2H 路径) 经 SocInternalNoc mesh |
| `mem_out` (port 2) | `PcieTlpBundle` | **`NoCFlitBundle`** | 下游 NoC 侧, VRAM 读/写经 SocInternalNoc mesh |
| `host_out` (port 3) | `PcieTlpBundle` | **删除** (合并到 `axi_master_out`) | host_out 是 host 端 DMA 走 PCIe AXI master 发送, 不再是 PcieTlpBundle |
| `done_out` (port 4) | `PcieTlpBundle` | **删除** (合并到 `axi_master_out` 响应) | done_out 是 host 端 completion 走 PCIe AXI master 响应, 不再是 PcieTlpBundle |
| `translate_req` (port 5, **新**) | 缺失 (programmatic `set_translate_cb`) | **`TranslateReqBundle`** (新) | SDMA→GMMU VA→PA 翻译请求 (per 真实 RTL 独立 translate 接口) |
| `translate_resp` (port 6, **新**) | 缺失 | **`TranslateRespBundle`** (新) | GMMU→SDMA 翻译响应 (含 paddr + 错误码) |
| `doorbell_in` (port 7, **新**) | 缺失 (programmatic `set_translate_cb` 已删) | **`DoorbellBundle`** | LWC hub → SDMA ring submit (per Change B 既有) |

**总端口数**: 5 → 8 (新增 axi_master_out + translate_req/resp + doorbell_in, 删除 desc_in/host_out/done_out)

**新增 wire-format 转换层** (per physical-ip-port-boundary 起草):
- `SDMA_PcieTlp_To_NocAdapter` (PcieTlpBundle → NoCFlitBundle, mem_out 出口侧, **deprecated** 因 SDMA 端口切型)
- `SDMA_Noc_To_PcieTlpAdapter` (NoCFlitBundle → PcieTlpBundle, mem_in 入口侧, **deprecated**)
- `SDMA_Axi_Master_Adapter` (Axi4Bundle ↔ DMA descriptor, axi_master_out 端口)

**删除 programmatic bridge**:
- `sdma->set_translate_cb(lambda)` 已删 (per Change C T6)
- `sdma->set_translate_target(gmmu_)` (per Change C T6) **删除**, 改 JSON `sdma.translate_req → gmmu.translate_in` 直连

### 切型 ③ — `GmmuTLM` 端口"translate slave + NoC master + CrBundle slave"三分

| 端口 | 当前类型 | 目标类型 | 理由 |
|------|---------|---------|------|
| `translate_in` (port 0, **新**) | 缺失 (programmatic `set_translate_target`) | **`TranslateReqBundle`** (新) | 上游 SDMA translate slave, JSON `sdma.translate_req → gmmu.translate_in` 直连 |
| `translate_resp` (port 1, **新**) | 缺失 | **`TranslateRespBundle`** (新) | 下游 GMMU→SDMA 翻译响应 |
| `req_out` (port 2) | `PcieTlpBundle` | **`NoCFlitBundle`** (GMMU dedicated memory client) | 下游 NoC 侧, PTE fetch 直连 VramController (per Gap I 修复, NVIDIA H100 模式) |
| `resp_in` (port 3) | `PcieTlpBundle` | **`NoCFlitBundle`** | 下游 NoC 侧, PTE fetch 响应回 GMMU |
| `cr_in` (port 4, **新**) | 缺失 (programmatic `gmmu_->set_pt_base_*()` 已删) | **`CrBundle`** (8-field APB-lite) | 上游 Chub/LWC 控制面, PT_BASE 寄存器读写 (per Change B 已有) |
| `mem_view_` (programmatic bridge) | (callback) | **保留** (timing-mode H-C6 不动) | timing-mode 走 mem_view_/translate_timing TLB 路径 |

**总端口数**: 2 (req_out/resp_in PcieTlpBundle) → 5 (translate_in/resp + req_out/resp_in NoCFlitBundle + cr_in CrBundle)

**删除 programmatic bridge**:
- `gmmu_->set_translate_target(sdma_)` (per Change C T6) **删除**, 改 JSON 直连
- `gmmu_->set_vram_target(vram_ctrl)` (per Change C T4) **删除**, 改 JSON `gmmu.req_out → soc_noc → vram_ctrl.req_in` 直连

### 切型 ④ — `VramControllerTLM` 端口保持上游 NoC 化 + 内部 CacheReqBundle 协议真实

| 端口 | 当前类型 | 目标类型 | 理由 |
|------|---------|---------|------|
| `req_in_/req_out_/resp_in_/resp_out_` (上游) | `CacheReqBundle` (继承 MemoryTLM) | **`NoCFlitBundle`** (上游 NoC 化) | 接受 SDMA/GMMU/Chub(SOC NOC) 经 SocInternalNoc mesh 来的请求 |
| `req_internal/resp_internal` (内部) | `CacheReqBundle` | **`CacheReqBundle`** (保留) | 内部 MemoryTLM 行缓冲 + 协议状态机 |
| `handle_functional_request()` | (C++ API) | **保留** (per Change A) | 接受 Chub/SDMA/GMMU 委托的 functional request |
| `set_backing_view()` | (C++ API) | **保留** (per ADR-DGPU-10) | host 特权 + internal IP 直访 backdoor |

**新增 wire-format 转换层** (per physical-ip-port-boundary 起草):
- `VramCtrl_Noc_To_AxiAdapter` (NoCFlitBundle → CacheReqBundle, 入口侧)
- `VramCtrl_Axi_To_NocAdapter` (CacheRespBundle → NoCFlitBundle, 出口侧)

### 切型 ⑤ — `PcieMemoryDevice` 移除 (per 提议 5)

**当前承担 2 件事** (per `pcie_memory_device.{hh,cc}`):
1. **BAR2 8GB vram aperture 外观层**: driver 通过 23 ABI 的 `cpptlm_emulator_dma_*` 系列访问 vram
2. **SDMA bulk DMA 委托 VramController functional port**: `handle_slave_port(port=0)` → `vram_controller_->handle_functional_request()`

**移除后路径**:
- **driver BAR2 写 vram**: `driver → cpptlm_emulator_dma_write() → DGpuBoard::mmio_write(bar=2) → Chub.bar2_decode() → NoCFlitBundle → SocInternalNoc → VramController.handle_functional_request() → vram_storage_`
- **SDMA bulk DMA**: `sdma.mem_out (NoCFlitBundle) → SocInternalNoc → VramController.handle_functional_request() → vram_storage_`
- **两条路径经同一 NoC + VramController 通道** — driver BAR2 与 SDMA 视角**完全统一**

**测试迁移**:
- `[pcie-memory]` 17 cases 全部迁移到 `[pcie][bar2]` 或 `[soc_noc][bar2]` 新标签
- `[pcie]` 430 cases 0 回归
- `[minimal_dgpu_soc]` E2E 重写 BAR2 路径 (per Chub→NoC→VRAM)

**保留**:
- **23 ABI 函数**: 0 diff (BAR2 路径从 PcieMemoryDevice 委托 → Chub/NoC 委托, 但 ABI 入口不变)
- **4 frozen headers**: 0 diff (PcieMemoryDevice 不在 frozen list)

### 切型 ⑥ — `SocInternalNoc` 沿用 physical-ip-port-boundary 切型结果

per physical-ip-port-boundary draft 切型: callback 化 → ChStream 化, 4 target programmatic bridge 取消, JSON `connections[]` 声明 N 组 ChStreamPort。

### 切型 ⑦ — `LwcBusHub` 沿用 Change B 既有

per Change B 既有: 32-port cap + 同步路由 + `register_cr_target/register_doorbell_target` programmatic, **本 change 保留** (Chub 的 `cr_master_out/doorbell_out` 接入 LwcBusHub, LwcBusHub 内部 `register_cr_target("gmmu", &gmmu_->cr_in())` 等保持)。

**注**: per 提议 6 完全 JSON 化目标, LwcBusHub 的 programmatic `register_*_target` 也可改造为 JSON 声明 (e.g. `lwc_hub.targets: { cr: ["gmmu.cr_in"], doorbell: ["sdma.doorbell_in"] }`), 这是**未来可选增强**, 本 change 不强求。

### 新增 ⑧ — `TranslateReqBundle` + `TranslateRespBundle` (新 Bundle 类型)

**位置**: `include/bundles/translate_bundles_tlm.hh` (新建)

**TranslateReqBundle 字段** (per ARM SMMUv3 翻译请求规范):
- `ch_uint<64> iova` — 输入虚拟地址
- `ch_uint<32> size` — 翻译长度
- `ch_uint<8>  access_type` — 读/写/原子
- `ch_uint<8>  task_id` — SDMA 任务 ID (用于响应匹配)
- `ch_uint<16> request_id` — 请求 ID (用于 OOO 匹配)

**TranslateRespBundle 字段**:
- `ch_uint<64> paddr` — 物理地址 (翻译成功)
- `ch_uint<8>  status` — OK/PERMISSION_FAULT/PAGE_FAULT
- `ch_uint<8>  task_id` — SDMA 任务 ID (回写)
- `ch_uint<16> request_id` — 请求 ID (回写)

**编译期 `BUNDLE_FIELD_WIDTH` 锁** (per `include/utils/port_contract_asserts.hh` 既有)

### 不在范围内

- **23 ABI 函数** (`include/abi/cpptlm_emulator.h`): 零 diff
- **4 frozen headers**: 零 diff
- **PcieEndpointIP 内部 17 端口 PcieTlpBundle composite**: 保留 (frozen by Phase 4 桥接)
- **PcieAxiAdapter 3 端口** (axi_master_out + axi_slave_in + cfg_slave_in): 保留 (per PcieEndpointIP 内部 composite)
- **PCIe Controller ↔ host 板级边界**: 保留 PcieTlpBundle
- **timing-mode** (dgpu_soc_timing_v1.json): 不动 (per H-C6)
- **LWC 总线语义** (CrBundle + DoorbellBundle + MsixBundle): 不动 (per Change B 既有)
- **Change E (MSI-X Generator IP)**: 独立 change, 本 change 准备接口 (Chub.msix_in)
- **Change F (Error/CDC Bus)**: 独立 change, 本 change 准备接口 (Chub.err_out)

---

## Acceptance Criteria

- [ ] `ControlHub` (Chub) 模块新建 (include/tlm/cluster/control_hub.{hh,cc})
- [ ] Chub 6 类端口实现 (axi_slave_in + cfg_slave_in + cr_master_out + doorbell_out + noc_master_out + msix_in)
- [ ] Chub 4 BAR decode 路由实现 (BAR0/1/2/3)
- [ ] `SdmaEngineTLM` 端口切型完成 (8 端口: axi_master_out + mem_in/out NoCFlit + translate_req/resp + doorbell_in)
- [ ] `GmmuTLM` 端口切型完成 (5 端口: translate_in/resp + req_out/resp_in NoC + cr_in CrBundle)
- [ ] `VramControllerTLM` 上游 NoC 化 (per physical-ip-port-boundary 起草)
- [ ] `PcieMemoryDevice` 模块移除 (含 17 cases `[pcie-memory]` → `[pcie][bar2]` 迁移)
- [ ] `SocInternalNoc` ChStream 化 (per physical-ip-port-boundary)
- [ ] `TranslateReqBundle` + `TranslateRespBundle` 新建 (include/bundles/translate_bundles_tlm.hh)
- [ ] JSON `sdma.translate_req → gmmu.translate_in` 直连走通
- [ ] JSON `gmmu.req_out → soc_noc.3 → soc_noc.1 → vram_ctrl.req_in` 直连走通
- [ ] JSON `pcie_ep.axi_master_out → chub.axi_slave_in` 直连走通
- [ ] JSON `chub.noc_master_out → soc_noc.0` 直连走通 (BAR2 data path)
- [ ] JSON `chub.cr_master_out → lwc_hub.0 → gmmu.cr_in` 直连走通
- [ ] JSON 拓扑 1 条 → 13 条
- [ ] 0 回归 (per Phase 0+1 实测基线): [pcie] 430/36672 + [gmmu] 25/5273 + [noc] 64/304 + [sdma] 68/12945 + [minimal_dgpu_soc] 24/170 + [dgpu_soc_timing] 37/12024 + [driver_visible] 4/40
- [ ] frozen headers 0 diff + 23 ABI 0 diff
- [ ] 实施笔记 (`docs/implementation/2026-XX-XX-...md`) 完整 + post-flight 验证
- [ ] 同步更新 `dgpu_soc_minimal_v1.json` JSON 拓扑 (1 → 13 条 connection)
- [ ] `[pcie-memory]` 17 cases 全部迁移到新标签 + 0 回归

---

## Capabilities

### New Capabilities

- `dgpu-soc-control-hub`: 新建 ControlHub (Chub) 模块, 集中 4 BAR decode + MSI-X 聚合 + Error 聚合
- `dgpu-soc-sdma-translate-port`: SDMA 独立 translate_req/resp 接口, 替代 programmatic `set_translate_cb`
- `dgpu-soc-gmmu-translate-slave`: GMMU translate_in/resp 接口, 接受 SDMA 翻译请求
- `dgpu-soc-translate-bundle`: TranslateReqBundle + TranslateRespBundle 新 Bundle 类型 (per ARM SMMUv3 规范)
- `dgpu-soc-bar2-no-pcie-memory`: BAR2 8GB vram aperture 完全走 Chub→NoC→VramController 路径, 移除 PcieMemoryDevice 外观层
- `dgpu-soc-full-json-topology`: JSON `connections[]` 1 → 13 条, 完全 JSON 声明, 0 处 programmatic bridge

### Modified Capabilities

- `dgpu-board-abi-hygiene`: 不动 (23 ABI 冻结)
- `driver-visible-minimal-soc`: 同步更新 JSON 拓扑 (1 → 13 条) + DGpuBoard::bind_memory_backings() 装配简化
- `soc-internal-noc`: 从 callback 化 → ChStream 化 (per physical-ip-port-boundary)
- `sdma-engine-tlm`: 端口切型 (5 → 8 端口, 上游 AXI + 下游 NoC + 独立 translate)
- `vram-controller-functional-mode`: 上游 NoC 化 (per physical-ip-port-boundary)
- `gmmu-tlm`: 端口切型 (2 → 5 端口, translate slave + NoC + CrBundle)
- `pcie-ip-microarch`: 不动 (PcieEndpointIP 内部 frozen)
- `p2-integration-unblock`: Chub 替代 Axi4CacheAdapter 桥接, 提议 1 直接落地 (D3/D4 断点由 Chub 解决)

### Removed Capabilities

- `pcie-memory-device`: PcieMemoryDevice 模块完全移除 (per 提议 5)
- `dgpu-soc-programmatic-bridge-pattern`: 移除 6 处 programmatic bridge (4 soc_noc target + 2 fabric 桥 + gmmu set_vram_target + sdma set_translate_cb) → 0 处, 全部 JSON 化

### Supersedes (与 physical-ip-port-boundary 合并)

- `cpptlm-dgpu-soc-physical-ip-port-boundary`: 本 change 范围**超集**, 建议合并为一个 change。命名建议改为 `cpptlm-dgpu-soc-physical-ip-rtl-topology` 更准确。

---

## Impact

### Affected Code

- `include/tlm/cluster/control_hub.{hh,cc}` (新建, ~400 行)
- `src/tlm/cluster/control_hub.cc` (新建, ~300 行)
- `include/bundles/translate_bundles_tlm.hh` (新建, ~150 行)
- `include/tlm/gpu/sdma_engine_tlm.{hh,cc}`: 端口切型 (5 → 8 端口, 5 端口类型变更)
- `include/tlm/gpu/gmmu_tlm.{hh,cc}`: 端口切型 (2 → 5 端口, 2 端口类型变更)
- `include/tlm/vram_controller_tlm.{hh,cc}`: 端口切型 (上游 NoC 化)
- `include/tlm/gpu/pcie_memory_device.{hh,cc}`: **完全删除** (per 提议 5)
- `include/tlm/cluster/soc_internal_noc.{hh,cc}`: ChStream 化 (per physical-ip-port-boundary)
- `include/tlm/cluster/gpu_internal_fabric.{hh,cc}`: ChStream 化 (per physical-ip-port-boundary)
- `src/tlm/gpu/dgpu_board_shell.cc`: bind_memory_backings() 简化 (~30 行 → ~3 行)
- `configs/dgpu_soc_minimal_v1.json`: connections 1 → 13 条
- `chstream_register.hh`: 新增 Chub + TranslateBundle 注册
- `modules_cluster.hh`: 新增 `REGISTER_MODULE(ControlHub)`

### Affected Tests

- `[pcie]` 430 cases: 0 回归
- `[gmmu]` 25 cases: 0 回归 + 新增 translate cases
- `[noc]` 64 cases: 0 回归
- `[sdma]` 68 cases: 0 回归 + 新增 translate cases
- `[vram]` (新) 0 cases: 待新增
- `[control_hub]` (新) ≥ 6 cases: Chub 4 BAR decode + MSI-X 聚合 + Error 聚合
- `[translate]` (新) ≥ 4 cases: SDMA↔GMMU translate 直连
- `[bar2]` (新) ≥ 8 cases: 替换 [pcie-memory] 17 cases (driver BAR2 → Chub → NoC → VRAM)
- `[minimal_dgpu_soc]` 24 cases: 0 回归 + 新增 E2E 直连 case
- `[dgpu_soc_timing]` 37 cases: 0 回归 (timing-mode 不动)
- `[driver_visible]` 4 cases: 0 回归
- `[pcie-memory]` 17 cases: **迁移到 [bar2] 8 cases 或删除**
- `[pcie-ep-soc-bridge]` (per p2-integration-unblock): 0 回归
- `[port-typing]` 22 cases (per physical-ip-port-boundary 起草): 0 回归
- `[soc_noc][chstream]` 4 cases: 0 回归
- `[gpu_fabric][chstream]` 4 cases: 0 回归

### Affected Specs

- `openspec/specs/dgpu-soc-control-hub/spec.md` (新增, 本 change 起草, 5 ADDED Requirements)
- `openspec/specs/dgpu-soc-physical-ip-port-boundary/spec.md` (合并 superset, 5 ADDED Requirements)
- `openspec/specs/sdma-engine-tlm/spec.md` (修改, 8 端口 + translate 接口)
- `openspec/specs/vram-controller-functional-mode/spec.md` (修改, 上游 NoC 化)
- `openspec/specs/gmmu-pte-wf/spec.md` (修改, 5 端口 + translate slave)
- `openspec/specs/soc-internal-noc/spec.md` (修改, ChStream 化)
- `openspec/specs/lwc-bus-hub-mvp/spec.md` (修改, 保留)

### Affected ADRs

- **ADR-DGPU-05** (vram-storage-ownership): Status Update 2026-10-13 追加, 标记"PcieMemoryDevice 移除"
- **ADR-DGPU-06** (axi-mem-bundle-boundary): Status Update 2026-10-13 追加, 标记"已落地" (per physical-ip-port-boundary + Chub)
- **ADR-DGPU-08** (abi-freeze-policy): 不动 (23 ABI 0 diff)
- **ADR-DGPU-09** (driver-visible-minimal-soc-scope): Status Update 2026-10-13 追加, 标记"BAR2 路径改走 Chub→NoC"
- **ADR-DGPU-10** (backing-naming-convention): 不动
- **ADR-DGPU-11** (timing-mode-soc-scope): 不动 (timing-mode H-C6)

### Risk Assessment

- **High Risk**: 新建 Chub 模块 + 5 模块端口切型 + PcieMemoryDevice 移除 + 8 个新端口 + 2 个新 Bundle, 影响面广 (10+ 标签 1200+ cases)
- **Mitigation**:
  - TDD 5-step 严格 RED→GREEN→E2E→Notes
  - 0 回归锁 (per Phase 0+1 实测基线)
  - frozen headers 0 diff + 23 ABI 0 diff (per ADR-DGPU-08)
  - 实施期保留 programmatic bridge fallback (per Change C §6 Deviation 4 先例)
  - 拆分 8 sub-task (per tasks.md), 每 sub-task 独立 TDD + commit
  - 与 physical-ip-port-boundary 合并实施 (范围超集)
  - 实施后 doc-only cleanup (per cpptlm-dgpu-soc-arch-truth-cleanup v1.3 §6 先例, 不消耗 KPI)

### KPI Status (起草时)

- **当前 baseline (2026-10-13)**: 4/3 (e1 v1.1 + flr-rebar v1.1 archive 后, 当前 active 4: physical-ip-port-boundary + p2-integration-unblock + pcie-tlp-entry-layer + msix-generator-ip), **触顶**
- **本 change 起草状态**: 🟡 Proposed Draft (per AGENTS.md "Draft 不计入 KPI 上限" 复审规则), **不消耗 KPI**
- **activate 时机**: 任一 active change archive 后, KPI 释放 1 slot, 再 activate 本 change
- **归档时**: 1 slot 释放, 给 Change F (Error/CDC) backlog 留位

### 与 physical-ip-port-boundary 合并建议

本 change 与 `cpptlm-dgpu-soc-physical-ip-port-boundary` (commit `12cc50de`) **范围超集**:
- 包含后者全部内容 (5 模块切型 + 4 adapter + 2 SimModule ChStream 化)
- 增量: 新建 Chub + 移除 PcieMemoryDevice + SDMA 独立 translate 接口 + 2 新 Bundle + 13 条 JSON 拓扑

**建议**: 与 physical-ip-port-boundary **合并为 1 个 change**, 命名 `cpptlm-dgpu-soc-physical-ip-rtl-topology` (更准确反映范围)。两种路径:
- **路径 A**: 保留 2 个 change, 实施时合并 task list (并行推进)
- **路径 B**: 物理合并 (删 physical-ip-port-boundary 4 件套, 全部纳入本 change)

**本草案采用路径 A** (per doc-only cleanup 先例, 保留独立 4 件套但范围说明超集关系), 由用户在 review 时决定。
