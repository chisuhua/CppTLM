# Minimal DGpu SoC 架构视图 (基于 D-AXI v1.4 + e1 v1.1 + flr-rebar v1.1)

> **版本**: v1.4-view + **e1 v1.1 + flr-rebar v1.1 修订同步** (业务架构视图)
> **日期**: 2027-02-09 (初稿) / 2026-10-09 (e1 v1.1 + flr-rebar v1.1 修订同步)
> **状态**: ✅ **业务架构视图**(已与代码真相对齐,**§1.4 仿真模式声明 + §1.4.5 名词定义 已按用户评审 v0.4 锁定** — VramControllerTLM 单独 functional-mode,不是 SoC 整体 zero-delay)
> **修订记录 v0.4 (2026-10-09)**: 用户评审指出仿真模式段错误,本版本重写 §1.1 / §1.3 / §1.4 + 同步 §0.1 原则 4 / §0.2 Gap 矩阵 G3 / §0.2 Change A 措辞
> **前置**: `DGpuBoard v2.0.2`(架构文档 [../dgpu-board/architecture.md](../dgpu-board/architecture.md) + 4 个 ADR DGPU-01~04)+ gem5 `AbstractMemory`/`PhysicalMemory`/`MemBackdoor` 参考模式
> **前置**: `DGpuBoard v2.0.2`(架构文档 [../dgpu-board/architecture.md](../dgpu-board/architecture.md) + 4 个 ADR DGPU-01~04)+ gem5 `AbstractMemory`/`PhysicalMemory`/`MemBackdoor` 参考模式
> **配套 ADR**: [ADR-DGPU-05](../adr/ADR-DGPU-05-vram-storage-ownership.md) (单一 VRAM 所有权) + [ADR-DGPU-06](../adr/ADR-DGPU-06-axi-mem-bundle-boundary.md) (AxiMemBundle 边界) + [ADR-DGPU-07](../adr/ADR-DGPU-07-minimal-soc-evolution-seam.md) (演进 seam) + [ADR-DGPU-10](../adr/ADR-DGPU-10-backing-naming-convention.md) (backing 字段命名约定 owner/injected 两级)
> **Owner**: CppTLM Team
> **权威源**: 本文档为 **业务架构视图**,代码权威源是 D-AXI v1.4(commit `429327d`)+ `dgpu_soc_minimal_v1.json` 配置(见 §1.4 末尾)。
> **目标存档**: 本目录 `architecture.md`(原 `docs/designs/2027-02-09-minimal-dgpu-soc.md`,2026-09-26 迁移至此;本仓不与外部 ArchForge 仓同步)

## ⚠️ v0.6 Lock Note (2026-10-13) — Chub change supersedes 部分描述

> **本节不是新内容,是对当前文档与未来 OpenSpec 变化关系的"指针声明",避免读者读到 outdated 描述时困惑。**

本文档**当前反映 v1.8 实施真相**(per `dgpu_soc_minimal_v1.json` + D-AXI v1.4 commit `429327d`),文档中部分 PcieMemoryDevice 路径描述(`sdma.mem_out → pcie_memory.handle_slave_port → vram_controller->handle_functional_request` 等)在 Chub change 实施后将**过时**。

**不影响的内容**(本 Lock Note 不动):
- §0.1 4 原则 (driver-visible / VRAM 所有权 / 物理 IP 边界 + 无 callback / VramController functional-mode) — 与 Chub change 完全一致
- §1.4 仿真模式声明 (混合仿真模式 SoC, VramController 单独 functional-mode) — Chub change 仅动 wire-format, 不动仿真模式
- §5 GMMU 一级页表翻译 — Chub change 推 GMMU 5 端口切型, 翻译语义保持

**会被 Chub change 实施 supersedes 的内容**(本 Lock Note 显式标记, 等 Chub 实施后完整重写):
- §0.1 原则 4 "数据路径": "Phase 0: `sdma.mem_out → pcie_memory.handle_slave_port → vram_controller`" → 改为 "**Chub 化后: `chub.noc_master_out → soc_noc → vram_controller.req_in`**" (per [cpptlm-dgpu-soc-control-hub](../../../openspec/changes/cpptlm-dgpu-soc-control-hub/) design.md §1.1)
- §0.2 Gap 矩阵: 当前 11 项 Gap, Chub change 推动后整体修复 (含 PcieMemoryDevice 移除 + SDMA 独立 translate + GMMU 5 端口切型)
- §0.2 实施计划: 当前 6 changes (A/B/C/D/E/F), Chub change (P0 架构级) 推动 4 changes 合并 + 1 new change (control-hub) superset
- §1.4.3 SDMA ChStream 接口描述: "AxiMemBundle" → 改为 "**NoCFlitBundle** (per physical-ip-port-boundary + control-hub 起草)"
- §1.4.6 GMMU 仍 sync translate 描述: 保留, 但 GMMU 端口数 2 → 5 (per Chub change §4)
- §2.1 拓扑图: 6 业务模块 (含 PcieMemoryDevice) → 7 业务模块 (+Chub, -PcieMemoryDevice, 净 +1) — 等 Chub 实施后重画
- §2.4 5-IP Real-IP Boundary 表: 5 个 IP 边界, 需加 ControlHub(Chub) 作为第 6 个 IP — Chub change 推动后修订
- §2.5 HostPcieTestStub 仿真测试桩 (per flr-rebar v1.1): 不动, 与 Chub change 正交
- §2.6 FLR + Resizable BAR 测试扩展: 不动, 与 Chub change 正交

**关联 OpenSpec changes (本 Lock Note 新增) — Chub change superset 关系**:
- **[cpptlm-dgpu-soc-physical-ip-port-boundary](../../../openspec/changes/cpptlm-dgpu-soc-physical-ip-port-boundary/) (Proposed Draft 2026-10-12)**: 5 模块端口切型 + 4 wire-format adapter + SocInternalNoc/GpuInternalFabric ChStream 化 (per commit `12cc50de`)
- **[cpptlm-dgpu-soc-control-hub](../../../openspec/changes/cpptlm-dgpu-soc-control-hub/) (Proposed Draft 2026-10-13)**: 新建 ControlHub (Chub) + 移除 PcieMemoryDevice + SDMA 独立 translate 接口 + 完全 JSON 拓扑化 (per commit `b3334d36`)

**预期完整重写时点**: 任意 active change archive 后, KPI 释放 1 slot, 激活 Chub change (估时 21-30d), 实施完成后**本 Lock Note 段**会被替换为完整 v0.6 重写(per cpptlm-dgpu-soc-arch-truth-cleanup v1.3 修订流程)。

## 关联 OpenSpec changes

- [cpptlm-driver-visible-minimal-soc](../../../openspec/specs/driver-visible-minimal-soc/) (**v1.4** 当前,2026-09-26;6 模块拓扑 + 单一 VRAM 真源 + chip-internal AXI 边界 + 7 实施铁律)
- [2026-09-20-cpptlm-pcie-memory-device-mvp](../../../openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp/) (D2 v1.1, archived;本设计扩展自 D2 PcieMemoryDevice MVP)
- **[cpptlm-dgpu-soc-internal-noc-e1](../../../openspec/changes/cpptlm-dgpu-soc-internal-noc-e1/) (v1.1, 2026-10-09, ✅ archived 2026-10-09; 设计意图源保留; 代码已落地 per `soc_internal_noc.hh:114-151`)**: SocInternalNoc **DataNoC only** 物理保真 (SDMA bulk DMA 路径) + 2 adapter (SDMA + VRAM) + VC 计数器 (REQ/VC0, RSP/VC1, DAT/VC2, SNP/VC3) + VramNocAdapter 内部 NoC→AXI4 字段映射。**修复 4 P0 + 3 CRITICAL + 3 MAJOR** (per Oracle/Metis/RTL 调研三轮 + 真实 dGPU IP 调研)。PT_BASE / doorbell / page walk 走独立通道 deferred to E.2 (ArchForge)。详见 §2.4.3。
- **[cpptlm-pcie-host-test-flr-rebar](../../../openspec/changes/cpptlm-pcie-host-test-flr-rebar/) (v1.1, 2026-10-09, ✅ archived 2026-10-09; 设计意图源保留; 代码已落地 per Phase 8 桥接 + HostBypassTLM)**: HostPcieTestStub in-process C++ 仿真测试桩 + **复用既有** `flr_pf()/flr_vf()` + `enable_resizable_bar()/on_bar_resize()` (per `pcie_endpoint_ip.hh:201-202` + `pcie_endpoint_ip.cc:670-694`)。**修复 3 CRITICAL** (ADR-DGPU-08 §2.3 0 新增约束 + 3 处 PCIe spec 错误 + 2 spec 矛盾)。`ResizableBar::new_size_bytes_` 拓宽 uint32→**uint64** (4GB 溢出修复)。详见 §2.5 + §2.6。

**同步规则**：本架构文档代表 SoC 业务层长期设计意图（why）；具体实施指导与 5 轮 Oracle/Metis 评审补丁见上述 OpenSpec change 的 design.md（how）。修改 OpenSpec design.md 时若涉及设计意图变化，应同步更新本文档；反之不强制。

**e1 v1.1 + flr-rebar v1.1 关键变更** (per 2026-10-09 双 agent 复审 + RTL/真实 IP 调研):
1. e1 从"4 target wire format 全迁移"重定位为"DataNoC only"（删 PcieControllerIP/GMMU adapter；保留 SDMA/VRAM adapter）
2. flr-rebar 从"重建 FLR/ReBAR 协议"重定位为"复用既有 + HostPcieTestStub 测试桩"（不新建 PcieFlrHandler/ResizableBarRuntime 类）
3. 23 ABI 签名零 diff + 4 frozen headers 零 diff（per ADR-DGPU-08 §2.3 0 新增约束严格遵守）
4. `ResizableBar::new_size_bytes_` 字段类型 `uint32_t` → **`uint64_t`** (4GB resize 修复)
5. PCIe Base Spec §7.5.3.4 (PCIe 4.0/5.0) / §7.8.4 (PCIe 3.x): FLR bit 在 Device Control Register **offset 0x08 bit 15**（修 v1.0 offset 0x04 错误）
6. PCIe Base Spec §7.8.6: 4GB 64-bit BAR pair 编码 low dword=0, high dword=1（修 v1.0 编码错误）

---

## 0. 文档版本与评审记录

| 阶段 | 日期 | 评审方 | 结论 |
|------|------|--------|------|
| 探索 | 2027-02-09 | explore ×3 + librarian ×1 | 收集既有组件现状 + gem5 内存模式 |
| 头脑风暴 | 2027-02-09 | 用户澄清(2 轮: GMMU 抽象级别 + BAR 布局 + 访存路径) | 选定一级页表 + BAR0 寄存器 + BAR1 存储+门铃 + 零时路径 |
| Oracle 一轮复审 | 2027-02-09 | oracle | 5 项 Top-5 修复 + 文档落地 |
| Oracle 二轮复审 | 2027-02-09 | oracle | 4 项硬伤(JSON 嵌套/`as<T>()`/translate_cb 签名/落盘路径)+ 6 项次级 |

本 v1.0 已纳入所有 Oracle 二轮复审修订。

---

## 0.1 业务定位 (Business Positioning, 2026-10-09 lock)

> **本文档为后续会话与新提案的强约束 (Hard Constraint)**。所有未明确偏离的新提案/OpenSpec change 必须遵循此 4 原则定位,禁止 silent 偏离。本段经与用户多轮精确化 (2026-10-09) 锁定;同步约束在本段末尾"同步规则"小节。

本 minimal dGPU SoC 的业务定位由 **4 个原则**确定,与真实物理 IP/商业 dGPU (NVIDIA H100 / AMD Instinct) 设计意图对齐:

### 原则 1 — driver-visible SoC 拓扑

- UsrLinuxEmu 端 Linux driver 通过 **标准 PCIe BAR + 内部 chip-internal AXI 总线**看到真实、可驱动验证的最小 dGPU 抽象
- **入口**: 23 ABI 函数 + 4 BAR (BAR0 4KB MMIO + BAR1 16MB window + BAR2 8GB vram aperture, per ADR-DGPU-09 scope)
- **闭环证据**: `[minimal_dgpu_soc][driver_visible]` E2E 测试覆盖 (per D-AXI v1.4 §1 + §9.4)

### 原则 2 — chip-level main memory by DGpuBoard 持有

- **soc 芯片外接**一个 Functional Mode 抽象内存 (类比 HBM 物理内存)
- **所有权归 `DGpuBoard`**,不是 SoC 内部模块 (per [ADR-DGPU-05](../adr/ADR-DGPU-05-vram-storage-ownership.md) + D-AXI v1.4 B7 single VRAM 真源决策)
- 借鉴 gem5 `PhysicalMemory::backingStore` + `AbstractMemory::pmemAddr` + `MemBackdoor::ptr()` 模式
- 两种入口:
  - **backdoor 旁路**: host 特权直访 (`DGpuBoard::backdoor_read/write`),不经 PCIe TLP
  - **mmio 正路**: driver 视角经 BAR2 → `PcieMemoryDevice::memory_read/write`

### 原则 3 — SoC 内部模块接口贴近真实物理 IP 边界;双重无注入约束

- **目标态**: SoC 内部模块 = `GmmuTLM` / `SdmaEngineTLM` / `VramControllerTLM` / `PcieControllerIP` / `PcieEndpointIP` / `GpuInternalFabric` (per `dgpu-realip-pin-contract` 6-IP realip 边界;**双重计数说明**: §0.1 列 6 个 SoC 内部模块含 GmmuTLM/SdmaEngineTLM (GPGPU 端引擎); §2.4 表格列 5 个 IP 边界模块 (PcieControllerIP/PcieAxiBridgeIP/GpuInternalFabric/SocInternalNoc/VramControllerTLM) 不含 GMMU/SDMA 引擎)
- **GmmuTLM**: 一级页表实现 (functional-mode 同步翻译服务, per §5)
- **SdmaEngineTLM** H2D/D2H/D2D 服务经两类接口:
  - **chip-internal NOC 接口** (→ VramController, per `SocInternalNoc` e1 v1.1 DataNoC)
  - **PCIe 模块接口** (→ PcieEndpointIP, PcieTlpBundle)
- **双重无注入约束 (Hard Invariant)**:
  1. **无 callback 注入**: SDMA 调用 GMMU 必须经**真实 wire 接口**(NOC message 或 chip-internal AXI),**禁用** `set_translate_cb(λ)` lambda 注入。当前 minimal_v1 用了 lambda,**D3+ 须废弃**
  2. **无 host-side data bypass 注入**: 真实数据必须走 PCIe TLP / NOC,**禁** host 旁路注入 bypass 芯片路径。**保留**: host backdoor **特权只读** access 用于 driver 视角验证 (只读不写不构成"数据注入")

### 原则 4 — VramControllerTLM 单独支持 functional-mode + backdoor 接口

> **2026-10-09 修订注**: 措辞经 §1.4 v0.4 lock 后精炼,强调 **"只有 VramControllerTLM 单独 functional-mode"** 而非 "SoC 整体 zero-delay".

- **VramControllerTLM 是 minimal_v1 唯一 functional-mode 模块**: 通过 `set_mode(VramControllerMode::FunctionalZeroDelay)` 切到 zero-delay 但协议真实接口;与 timing-mode (`dgpu_soc_timing_v1.json`) **共用同一 `VramControllerTLM` 类**,仅配置差异 (functional 零时延迟 vs timing cycle accounting, per ADR-DGPU-11 H2 互斥)
- **VramControllerTLM backdoor 接口**: 通过 `set_backing_view(uint8_t*, uint64_t)` 暴露 (继承自 MemoryTLM, per ADR-DGPU-10 新名约定; deprecated wrapper `set_backing_store(deprecated)` 仍可调用), host 特权 (`DGpuBoard::backdoor_*`) 与内部 IP 都可经此接口直读 board-level 内存模块;类比 gem5 `MemBackdoor::ptr()`
- **SDMA 经 ChStream + NOC 接入**: SDMA 必须实现 `cpptlm::OutputStreamAdapter<bundles::AxiMemBundle>& mem_out()` 才能经 `SocInternalNoc` mesh (chip-internal) 接入 VramController functional-mode (per §1.4.3 修订)
- **本期 minimal_v1 即时落地**: VramControllerTLM 在 minimal_v1 数据路径主入口激活. **Phase 0 (Change A) 阶段**: `sdma.mem_out → pcie_memory.handle_slave_port(port=0) → vram_controller->handle_functional_request` (pcie_memory 作为桥接, 内部委托给 VramController). **Phase 1 (Change C) 阶段**: `sdma.mem_out → soc_internal_noc mesh → vram_noc_adapter → vram_controller->handle_functional_request` (NOC 直连, pcie_memory 不参与). 两阶段路径演进, D-AXI §7 D3 seam 已激活. **不是 v0.4 之前描述的 "直接 sdma.mem_out → vram_ctrl 绕过 pcie_memory"** (v0.5.1 修订 per Oracle C17)

### 当前状态 vs 目标态 Gap 矩阵 (2026-10-09 lock)

| 原则 | 当前状态 (minimal_v1) | 目标态 | 差距与处置 |
|------|----------------------|--------|-----------|
| 1 driver-visible | ✅ 已达 (4 BAR + 23 ABI + E2E) | 已达 | 0 |
| 2 VRAM 归 DGpuBoard | ✅ 已达 (D-AXI v1.4 B7 single VRAM) | 已达 | 0 |
| 3-模块名贴近 IP | ✅ 已重命名 `PcieControllerIP` / `PcieEndpointIP` / `GmmuTLM` / `SdmaEngineTLM` (D-AXI v1.5+) | 已达 | 0 |
| 3-Gmmu 一级页表 | ✅ 已达 (`gmmu_tlm.hh:45-71`) | 已达 | 0 |
| 3-SDMA↔GMMU 协调 | ⚠️ lambda `set_translate_cb` 注入 | 经 NOC / chip-internal AXI | **D3+ 须废弃 lambda** |
| 3-chip-internal NOC | ⚠️ 2 条 AxiMemBundle 直连 (`sdma.2`/`gmmu.0`→`pcie_memory`) | 经 `SocInternalNoc` mesh NoC (per e1 v1.1 DataNoC) | **D3 演进中** (e1 v1.1 archived 2026-10-09 + 代码已落地 commit 13af5d51) |
| 3-无 host data bypass | ✅ 已达 (`vram_segments_` 第三实例已删, per D-AXI v1.5 B14) | 已达 | 0 |
| **3-CR-Bus 寄存器访问** | ⚠️ BAR0 mmio 直调 `gmmu_->set_pt_base_lo/hi/enabled()` 4 处成员函数 (per `src/tlm/gpu/dgpu_board_shell.cc:463-472`);违反 §0.1 原则 3.1 "无 callback 注入" | 经 **CR-Bus (APB-lite)** transaction → IP register file (real RTL: PCIe Controller 收 TLP cfg/MMIO → 解码 → CR-Bus → RegFile R/W) | **HIGH (Gap H)**, D3 优先修; 修复路径见 §0.1.1 LightweightInterconnect + CrBundle |
| **3-GMMU page-walk wire-format** | ⚠️ **病因 = ADR-DGPU-06 §2.3 + AGENTS.md KEY INVARIANT line 513 (SDMA mem_in/mem_out → AxiMemBundle 切型决策) 与代码 (sdma_engine_tlm.hh:82-86, gmmu_tlm.hh:50-51, pcie_memory_device.hh:94-95 — 全部 PcieTlpBundle) 不一致** (D-AXI v1.5 B25 撤回 v1.3 B2 切型决策, 但 ADR-DGPU-06 §2.3 + AGENTS.md KEY INVARIANT 未同步) — 详见 ADR-DGPU-06 Status Update 2026-10-10 | **GMMU PTE fetch 改走 VramControllerTLM 专用 functional port** (NVIDIA H100 GMMU dedicated memory client 模式): `gmmu.req_out (AxMemBundle-like) → VramControllerTLM::handle_functional_request` 直连, **不经 pcie_memory, 不经 NOC mesh**, 不冲突 §1.4.3 强约束 (GMMU 是 dedicated client 而非 generic module);**Gap I 真矛盾诊断 — 待 Change C author 在 v0.5.2 修订时收敛 GMMU PTE fetch 路径** | **HIGH (Gap I)**, 与 Gap G2 (SocInternalNoc e1 v1.1) 同步修; GMMU PTE fetch 走 dedicated memory client, bypass mesh NoC |
| **3-Doorbell 直调** | ⚠️ BAR1 offset 0x10010000 → `sdma_engine_->mmio_write(1, kBar1DoorbellOffset, wptr)` 函数调用 (`dgpu_board_shell.cc:537-538`);违反 §0.1 原则 3.1 | 经 **Doorbell Posting Bus** → SDMA ring consumer (polling / posted-write-FIFO) (real RTL: PCIe posted write → SDMA queue;AMD GFX12 / NVIDIA HDA doorbell ring) | **MEDIUM (Gap J)**, 与 Gap H 同步修; 复用 §0.1.1 LightweightInterconnect + DoorbellBundle |
| **3-MMIO Read Response** | ⚠️ write-mirror (`mmio_regs_[bar,offset]=payload` + read roundtrip `dgpu_board_shell.cc:495-501, 526-529`) + 异步 `inject_q_` drain + 50ms wait timeout (`dgpu_board_shell.cc:367-388`);违反 §0.1 原则 3.1 + 3 (无真实 AR/R 握手) | 经 **CR-Bus AR/R** 同步 ready/valid handshake → CplD TLP 报文回 Host | **MEDIUM (Gap K)**, 与 Gap H 同步修 (同一条 CrBundle) |
| **3-MSI-X 中断交付** | ⚠️ SDMA fence → `msix_update_pending()` 同步直调 → `trigger_irq_async()` (`dgpu_board_shell.cc:1122-1127, 849-874`); CompletionRingTLM 在 dormant 状态 (per §1.4 注 3);违反 §0.1 原则 3.1 + 3 (0 个真实 MSI-X TLP 报文生成路径) | 经 **MSI-X Generator IP** → PBA → MSI-X table address lookup → MSI-X TLP 报文 (Msg/MsgD) → Host RC (PCIe Base Spec §6.8) | **MEDIUM (Gap L)**, D3 阶段实现; 复用 §0.1.1 LightweightInterconnect + MsixBundle |
| **3-PCIe 入口 TLP 序列化层** | ⚠️ `DGpuBoard::mmio_read/write/config_read/write` 直调 (host API 直达 chip-internal, 0 个 TLP 编解码层) | 经 **PcieTlpSerializer** (host) → `PcieEndpointIP` TLP 解码 (Per PCIe Base Spec §2) → 路由 | **MEDIUM (Gap M)**, 由 `cpptlm-pcie-host-test-flr-rebar v1.1` HostPcieTestStub + `cpptlm-p2-integration-unblock` 一并修 |
| 4 VramController 数据路径 | ⚠️ 当前数据路径直 memcpy 绕过 VramController (`pcie_memory.handle_slave_port ↔ vram_storage_` 直连); SDMA 不经 VramController 协议层直接 memcpy | **VramControllerTLM functional-mode 接口** (zero-delay 时序, 控制器协议真实; 类比 gem5 `AbstractMemory::functionalAccess` + HBM Controller); **VramControllerTLM backdoor 接口** 通过 `set_backing_view(ptr, sz)` 暴露 board-level 内存模块直访 (类比 gem5 `MemBackdoor::ptr()`, per ADR-DGPU-10 新名); 与 timing-mode (`dgpu_soc_timing_v1.json`) 共用同一类, 仅配置 `VramControllerMode` 差异 (per ADR-DGPU-11 H2 互斥) | **本期落地 (Gap G3)** per Change A, 本更新触发 |
| **Long issue: CDC / Power Gate** | ❌ 全局 sim 时钟, 0 个 CDC FIFO + 0 个 IP-level power gate; PCIe (100/250 MHz) / NoC (1 GHz) / HBM 各频率间无跨域建模 | 异步 cross-domain sample FIFO + per-IP clock gating + power domain 仿真 | **LOW (Gap N)**, long-term D3+ backlog, 不阻塞本期 |
| **Long issue: Error Aggregation Bus** | ❌ `error_code` 仅在 ChStream bundle 字段, 0 个 SoC-level error collector; per-IP `err` pin 未聚合 | 经 **ErrBus** → `ErrorCollectorIP` → PCIe AER 上报 Host (PCIe Base Spec §6.2) | **LOW (Gap O)**, long-term D3+ backlog, 不阻塞本期 |

### 同步规则 (Sync Rules, 2026-10-09 lock)

- **同步源**: 本 §0.1 是 minimal dGPU SoC 的**业务定位权威源 (Authoritative Source)**;`docs/pcie/driver-visible-minimal-soc.md` §1 后同步段仅作最小回放,详见该文档
- **强约束**: OpenSpec change 的 `design.md` 修改若涉及业务定位变化,**必须**同步更新本文档 §0.1 (否则视为 silently 违反 Hard Invariant)
- **新提案门槛**: 任何新提案的 `design.md` **必须**显式引用本文档 §0.1 四原则,并在 Gap 矩阵列出该提案影响的行;若新行 (此前未在 Gap 矩阵中) 必须先 justify 增加的必要性
- **LLM agent 强制**: 后续会话与 LLM agent 必须以本 §0.1 + Gap 矩阵作为判断基准,**禁止 silent 偏离**;有任何偏离必须显式标注且获得用户确认
- **演进路线图对应**: §0.2 总体实施计划 4 Phases (Phase 0/1/2/3) + Phase 3 backlog 必须与 Gap 矩阵"差距与处置"列严格对应,不可分离维护;具体修复路径见 §0.1.1

### 0.1.1 修复路线图方向 (Fix Roadmap Direction, 2026-10-09 lock)

**核心基础设施 — `LightweightInterconnect` (LWC) Bus Hub + 3+1 个 Bundle 类型**:

针对 Gap H/J/K (与 Gap L 后续) 共同依赖,引入统一的"轻量级非数据路径总线"基础设施:

| Bundle | 文件 | 拓扑/语义 | 修复 Gap |
|--------|------|----------|----------|
| **`CrBundle`** | **将新增于** `include/bundles/` (per Change B) | APB-lite 风格 (addr + wdata/rdata + write + ready/valid) | Gap H + Gap K (CR-Bus 寄存器访问 + MMIO Read Response) |
| **`DoorbellBundle`** | **将新增于** `include/bundles/` (per Change B) | 4B wdata + ring_id + posted bit | Gap J (SDMA ring 提交) |
| **`MsixBundle`** | **将新增于** `include/bundles/` (per Change B) | vector + addr_lo/hi + data + masked | Gap L (MSI-X 中断交付, D3+) |
| **(复用)** `ErrBundle` | (future) | err_pin + severity + ext_data | Gap O (Error Aggregation, D3+ long-term) |

**`LightweightInterconnect` LWC Bus Hub** (**将新增于** `include/tlm/lwc/` (per Change B)):
- **非 mesh 结构** (与 `SocInternalNoc` 区别, 树形/星形 / 单一 Hub + N IP port)
- 1 个 Hub + N 个 IP port (每个 IP 添加 1 个 ChStream SlavePort 接 LWC)
- LWC Bundle 可配置为 CR/Doorbell/Msix 三种语义 (决定 ready/valid 握手时序与 posted 标志)

**修复顺序与依赖关系**:

```
[ P1: LightweightInterconnect + CrBundle ]            ← 解 Gap H + Gap K (BAR0 mmio + MMIO Read 改走 CR-Bus)
              ↓
[ P1: DoorbellBundle ]                                ← 解 Gap J (BAR1 0x10010000 改走 Doorbell Posting)
              ↓
[ P2: GMMU page-walk wire-format 修复 ]               ← 解 Gap I (与 SocInternalNoc e1 v1.1 同步)
              ↓
[ D3+: MsixBundle + MSI-X Generator IP ]              ← 解 Gap L (TLP 报文生成)
              ↓
[ flr-rebar v1.1 + p2-integration-unblock ]           ← 解 Gap M (PcieTlpSerializer + TLP 解码层)
              ↓
[ long-term: CDC / Error Aggregation ]                ← 解 Gap N + Gap O
```

**为何不用 CrossbarTLM 复用** (核心选型决定):
- CrossbarTLM 是 **mesh NOC 拓扑**, 用于高吞吐数据路径 (Cache↔Memory, SDMA↔VRAM)
- CR-Bus / Doorbell / MSI-X 是 **低吞吐轻量级总线**, 树形/星形 + 简单握手协议
- 强行复用 CrossbarTLM 会制造"轻量级配置总线走 NOC 端口"的语义混乱 (违反 gap 矩阵 §0.1 原则 3 "贴近真实物理 IP 边界")
- LWC 是 **独立的 per-IP cluster 结构**,与 SoC 顶层的 SocInternalNoc (mesh) 正交分层: LWC 负责"配置 / 中断 / 提交"等非数据流量,SocInternalNoc 负责"读写数据 / 跨 IP bulk DMA"等高吞吐流量

### 0.2 总体实施计划 (Overall Implementation Plan, 2026-10-09 lock)

**目标**: 通过 6 个阶段化 OpenSpec Change,**完全修复 §0.1 Gap 矩阵所有 11 项 Gap** (含 2 项 long-issue N + O), 实现 minimal dGPU SoC **真实物理 IP 模块接口对齐**: 每个 IP 都拥有**真实 wire-format 接口, 0 个 callback 注入** (除显式边界 handler 例外).

**Phase Overview**:

| Phase | 时段 | 主题 | Changes | 修 Gap |
|-------|------|------|---------|--------|
| **Phase 0** 本期 | 2026-10 W4 ~ 2026-11 W1 (~3 周) | 数据路径 + 基础设施首发 | A + B (并行) | G3 + H + J + K |
| **Phase 1** D3 同步期 | 2026-11 ~ 2026-12 (~4 周) | NOC + TLP Entry | C + D (并行) | I + G2 + G1 + M |
| **Phase 2** D3+ 完整化 | 2027-Q1 (~5 周) | MSI-X Generator | E | L |
| **Phase 3** 长期 backlog | 2027-Q2+ | Error + CDC | F | N + O |

#### Phase 0: 本期 (2026-10 W4 ~ 2026-11 W1)

| Change | 名称 | Scope | 修 Gap | 优先级 | 估时 |
|--------|------|-------|--------|--------|------|
| **A** | `cpptlm-vram-controller-functional-integration` | minimal_v1 数据路径经 VramControllerTLM functional-mode (zero-delay, 协议真实); VramControllerTLM 暴露 `backdoor_read/write` 供 host 特权 + 内部 IP 直访 (per §1.4.1 + §1.4.2); SDMA 必须实现 ChStream 接口经 chip-internal NOC 接入 VramController (per §1.4.3) **[✅ archived 2026-10-11 per Tranche 2, commit `353f8aa8`; spec merged to `openspec/specs/vram-controller-functional-mode/`]** | **G3** | **P0** | 3-5d |
| **B** | `cpptlm-lwc-bus-hub-mvp` | 引入 LightweightInterconnect + **CrBundle** (BAR0 mmio + MMIO Read) + **DoorbellBundle** (BAR1 0x10010000 SDMA ring 提交) **[✅ archived 2026-10-11 per Tranche 3, 12 commits HEAD `1eb51d27`; spec merged to `openspec/specs/lwc-bus-hub-mvp/`]** | **H + J + K** | **P1** | 7-10d |

**并行可行性**: A 改数据路径 (`pcie_memory.handle_slave_port ↔ backing_view_` seam); B 改基础设施 (BAR 路由 + Doorbell 路径); **不同模块 + 不同路径**, 可 100% 并行实施 + 独立 review.

#### Phase 1: D3 同步期 (2026-11 ~ 2026-12)

| Change | 名称 | Scope | 修 Gap | 优先级 | 估时 | 依赖 |
|--------|------|-------|--------|--------|------|------|
| **C** | `cpptlm-gmmu-pte-bus-correction` | 修复 GMMU page-walk wire-format 错配 (恢复 AxiMemBundle 或 CHI REQ channel); 接入 SocInternalNoc DataNoC (协同 e1 v1.1 落地) | **I + G2 + G1** | **P2** | 5-7d | B + e1 v1.1 上线 |
| **D** | `cpptlm-pcie-tlp-entry-layer` | 引入 PcieTlpSerializer + TLP 解码层 (协同 flr-rebar v1.1 + p2-integration-unblock 落地) | **M** | **P2** | 7-10d | flr-rebar 上线 + p2-integration-unblock |

**并行可行性**: C 改 GMMU + NOC 路径; D 改 PCIe 入口; **不同模块**, 可并行实施.

#### Phase 2: D3+ 完整化 (2027-Q1)

| Change | 名称 | Scope | 修 Gap | 优先级 | 估时 | 依赖 |
|--------|------|-------|--------|--------|------|------|
| **E** | `cpptlm-msix-generator-ip` | 引入 MSI-X Generator IP + **MsixBundle** (复用 Change B LWC 基础设施) | **L** | **P2** | 5-7d | B 完成 |

#### Phase 3: 长期 backlog (2027-Q2+)

| Change | 名称 | Scope | 修 Gap | 优先级 | 估时 | 依赖 |
|--------|------|-------|--------|--------|------|------|
| **F** | `cpptlm-soc-error-cdc-bus` | 引入 **ErrBundle** + ErrorCollectorIP + CDC FIFO + per-IP power gating | **N + O** | **P3** | 10-15d | E 完成 |

#### Gap 完全修复覆盖度 (Final Coverage)

| Gap | 来源 (现行实现偏离处) | Phase | Change |
|-----|----------------------|-------|--------|
| **G3** ✅ VramController functional-mode + backdoor | `dgpu_board_shell.cc::bind_memory_backings()` 数据路径直 memcpy 绕过 VramController; SDMA 不经 VramController 协议层; `VramControllerTLM::set_backing_view` 缺失, host 特权无 backdoor 入口 | Phase 0 | **A** ✅ archived 2026-10-11 (Tranche 2, commit `353f8aa8`; `[minimal_dgpu_soc]` 12→18 cases / 66→125 assertions, +50%; `[dgpu_soc_timing]` 37/12024 0 回归; `[pcie-memory]` 17/48 0 回归) |
| **H** ✅ CR-Bus 寄存器访问缺失 | `dgpu_board_shell.cc:463-472` 4 处 `gmmu_->set_pt_base_*()` 直调 | Phase 0 | **B** ✅ archived 2026-10-11 (Tranche 3, commit `1eb51d27`; 改写为 `gmmu_tlm.cc::handle_cr_transaction()` 走 CrBundle) |
| **J** ✅ Doorbell 直调 | `dgpu_board_shell.cc:537-538` `sdma_engine_->mmio_write()` 直调 | Phase 0 | **B** ✅ archived 2026-10-11 (Tranche 3, commit `1eb51d27`; 改写为 `sdma_engine_tlm.cc::handle_doorbell_transaction()` 走 DoorbellBundle) |
| **K** ✅ MMIO Read write-mirror | `dgpu_board_shell.cc:495-501, 526-529, 367-388` `mmio_regs_` + `inject_q_` 异步 drain | Phase 0 | **B** ✅ archived 2026-10-11 (Tranche 3, commit `1eb51d27`; mmio_read 走同步 CR-Bus, `mmio_regs_` 保留作无-hub fallback 给 `[pcie]` characterization tests) |
| **I** GMMU page-walk wire-format 错配 | **真矛盾 = ADR-DGPU-06 §2.3 + AGENTS.md KEY INVARIANT line 513 切型决策与代码漂移** (per ADR-DGPU-06 Status Update 2026-10-10);`gmmu_tlm.hh:12,86` PcieTlpBundle vs JSON "AxiMemBundle" 为假矛盾 (Change C author 误诊) | Phase 1 | **C (e1 v1.1 ✅ archived 2026-10-09; GMMU PTE fetch 改走 VramControllerTLM 专用 functional port 程序化 binding)** |
| **G2** chip-internal NOC 2 条 AxiMemBundle 直连 | `dgpu_soc_minimal_v1.json:58-61` `sdma.2`/`gmmu.0`→`pcie_memory` 直连 | Phase 1 | C (e1 v1.1) |
| **G1** SDMA↔GMMU lambda 注入 | `dgpu_board_shell.cc:714-718` `sdma->set_translate_cb(λ)` | Phase 1 | C (随 NOC 接入废弃) |
| **M** PCIe 入口 TLP 序列化层缺失 | `dgpu_board_shell.cc:305, 543` `mmio_*`/`config_*` 直调绕开 TLP | Phase 1 | D |
| **L** MSI-X 直调 + CompletionRing dormant | `dgpu_board_shell.cc:1122-1127` `trigger_irq_async()` 直调, 0 个真实 MSI-X TLP 报文 | Phase 2 | E |
| **N** CDC / Power Gate (long-issue) | 全局 sim 时钟, 0 个 CDC FIFO + 0 个 per-IP power gate | Phase 3 | F |
| **O** Error Aggregation Bus (long-issue) | `error_code` 仅在 bundle 字段, 0 个 SoC-level error collector | Phase 3 | F |

**总修 Gap: 11 / 11 = 100% 覆盖** (含 2 项 long-issue).

#### 与已有/进行中 Changes 关系

| 已有/进行中 Change | 状态 | 与本计划 Change 关系 |
|-------------------|------|----------------------|
| `cpptlm-dgpu-soc-internal-noc-e1` v1.1 | ✅ archived 2026-10-09 (设计意图源保留; 代码已落地) | **协同**: 由 Change C 推动落地 (修复 GMMU PTE fetch + DataNoC 骨架一起实施) |
| `cpptlm-pcie-host-test-flr-rebar` v1.1 | ✅ archived 2026-10-09 (设计意图源保留; 代码已落地 per Phase 8 桥接 + HostBypassTLM) | **协同**: 由 Change D 推动落地 (HostPcieTestStub 调用路径不变, 底层加 TLP 序列化层) |
| `cpptlm-p2-integration-unblock` | 🟢 Proposed | **协同**: 与 Change D 并行 (CP 跨仓接入 unblock) |
| `cpptlm-dgpu-soc-timing-mvp` | ✅ 完成 | **独立**: 与本计划无冲突 (timing-mode 走 dgpu_soc_timing_v1, 与 minimal_v1 是不同 SoC 拓扑) |
| `cpptlm-driver-visible-minimal-soc` | ✅ 完成 v1.8 | **基础**: 本计划 Change A 基于其 D3 seam (`handle_slave_port ↔ backing_view_`) |

#### OpenSpec KPI 释放节奏 (KPI ≤3 限制)

**当前 baseline (2026-10-09)**: 2 Draft + 1 Proposed = **3/3 (上限触发, 无法立即新建 Proposed)**

**当前实际状态 (2026-10-10 per `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3 修订)**: 5/3 (4 Proposed Draft v0.5.1 [A/B/C/D] + 1 existing Proposed p2-integration-unblock); doc-only cleanup 不计入 Proposed KPI 上限 per cpptlm-dgpu-soc-arch-truth-cleanup proposal §6 + AGENTS.md §DOC HYGIENE 治理类豁免

**释放 + 推进顺序**:

| Step | Action | KPI | 备注 |
|------|--------|-----|------|
| Step 0 | (2026-10-09 起草后现态) | **5/3 (临时超限)** | e1 v1.1 ✅ archived 2026-10-09 + flr-rebar v1.1 ✅ archived + p2-integration-unblock (3/3 baseline) + **4 个新起草 change 全 Proposed**: A (P0) + B (P1) + C (P2) + D (P2) |
| ✅ Step 1 | A archive (Phase 0 实施完成) → 释放 1 KPI slot | **4/3 (2026-10-11 完成, 仍超 1)** | Change A 实施 (Tranche 2, commit `353f8aa8`) + archive → `archive/2026-10-11-cpptlm-vram-controller-functional-integration/`; spec merged to `openspec/specs/vram-controller-functional-mode/spec.md` (7 ADDED Requirements); frozen headers 零改动 + 23 ABI 零变更 |
| ✅ Step 2 | B archive (Phase 0 实施完成) → 释放 1 KPI slot | **3/3 (2026-10-11 完成, 回到 KPI 上限)** | Change B 实施 (Tranche 3, 12 commits HEAD `1eb51d27`) + archive → `archive/2026-10-11-cpptlm-lwc-bus-hub-mvp/`; spec merged to `openspec/specs/lwc-bus-hub-mvp/spec.md` (9 ADDED Requirements); Gap H/J/K 修复 (`set_pt_base`/`sdma mmio_write`/异步 write-mirror 直调 → 0 处); `[lwc]` 19 cases / 118 assertions + `[minimal_dgpu_soc]` 18→21 cases / 145 assertions (0 回归); frozen headers 零改动 + 23 ABI 零变更 |
| Step 3 | C archive (Phase 1 完成) → Proposed Change E (P2) | 3/3 | Phase 2 启动 (Gap L fix) |
| Step 4 | D archive (Phase 1 完成) → Proposed Change F (P3) | 3/3 | Phase 3 启动 (Gap N+O fix) |
| Step 5 | E archive → Phase 2 完成; F archive → **Phase 3 完成** | - | **100% Gap 修复** (11/11) |

**注 v0.5**: Step 0 当前实际 5/3 临时超限 2 个 KPI slot — 因 Phase 0 + Phase 1 全部 4 change 已起草完毕 (per v0.5 用户评审 + Oracle/Metis 复审 + 修订完成). 用户决策后应**先 archive A 与 B** (Phase 0 实施完成时) 回到 ≤3 上限, 再 activate C/D. Step 1-4 严格按 archive 节奏保持 3/3 上限.

**每 change 在 archive 前不重复修改设计.md/proposal.md/spec.md**: Phase 0 实施时按 A + B 4 件套正文 (Phase A 已回填 v1.0.1 + Phase C 修订); Phase 1 实施时按 C + D 4 件套正文 (Phase B + C 已修订).

**加速选项** (KPI 释放):
- 若用户允许 KPI 临时超限 (如 4/4 一段时间), 可并行 Proposed 多于 1 个, **提前 ~2 个月**完成全部 6 个 Change
- 风险: 影响 [OpenSpec Proposed 存量 KPI ≤3] 复审 (per AGENTS.md §DOC HYGIENE)

#### 关键风险与缓解

| 风险 | 影响 Change | 缓解 |
|------|------------|------|
| **LWC 异构 Bundle Multi-Port Adapter 框架限制** (per D-AXI v1.5 B25 决策沿袭) | B | 同步扩展 `framework/stream_adapter.hh`, 把"多 Port + 异构 Bundle"加到 framework |
| **GMMU wire-format 切换可能破坏现有异步状态机** (`test_pcie_endpoint_ip_basic.cc` 已有测试覆盖) | C | 保留 PcieTlpBundle 兼容 wrapper, 设 flag 切换; D3 release 后再删 wrapper |
| **VramController functional-mode 集成可能影响 SDMA 性能断言** (`[dgpu_soc_timing]` 测试) | A | 加 gating 测试, 仅在 `simulation_mode="functional"` 下生效; timing-mode 不变 |
| **PcieTlpSerializer 与 23 ABI 兼容性** | D | 序列化层在 abi 层**之后**, 23 ABI 签名零变更 (per ADR-088 §D5 + D-AXI v1.6 §12) |
| **MSI-X Generator IP 与 FLR/ReBar 测试 stub 兼容性** | E | 复审时确保 HostPcieTestStub 不重新 mock, 仍走既有 DGpuBoard 入口 |
| **已 Proposed KPI 3/3 上限** | C/D/E/F 入 Proposed | 按 Step 1-7 archive 节奏分批 (本计划已规划) |

#### 完成目标 (Definition of Done, DoD)

Phase 0-3 全完成时:

- ✅ §0.1 Gap 矩阵 **16 行全部归零** (其中 14 行 ✅ + 2 long-issue N/O ✅)
- ✅ 真实物理 IP 边界 (8 大类): PcieControllerIP / PcieEndpointIP / GmmuTLM (含 dedicated memory client, bypass mesh NoC) / SdmaEngineTLM (含 NOC + Ring) / VramControllerTLM / GpuInternalFabric / **MSI-X Generator IP (新)** / **ErrorCollector IP (新)**
- ✅ LWC 总线统一基础设施 (1 次基础设施 + 4 种 Bundle: CrBundle / DoorbellBundle / MsixBundle / ErrBundle)
- ✅ SocInternalNoc mesh + LWC 树形/星形 **正交分层**
- ✅ 23 ABI 签名零变更 (per ADR-088 §D5)
- ✅ PcieTlpSerializer 是唯一的 host↔PCIe TLP 边界 (与 D-AXI v1.6 §1.4 一致)
- ✅ 既有 `[minimal_dgpu_soc]` / `[driver_visible]` / `[pcie]` / `[pcie-memory]` / `[sdma]` / `[realip]` / `[noc]` 测试保持全绿 + 新增对应 `[lwc]` / `[msix]` 标签

#### LWC Bundle 增量演进路径

```
LWC Bus Hub + 3+1 Bundle:
├── CrBundle      → Change B (Phase 0) → 解 Gap H + K
├── DoorbellBundle → Change B (Phase 0) → 解 Gap J
├── MsixBundle    → Change E (Phase 2) → 解 Gap L
└── ErrBundle     → Change F (Phase 3) → 解 Gap O
```

注: 4 个 Bundle 复用同 1 个 LWC Hub 基础设施 (Change B 一次落地, 后续 3 个 Bundle 仅加 wire-format 注册 + 接收端逻辑).

---

## 1. 设计目标与边界

### 1.1 目标

构建一个**最小可驱动验证**的 dGPU SoC 示例,端到端贯通 **host ABI → PCIe BAR fast-path / SDMA 经 chip-internal NOC → VramControllerTLM functional-mode → vram_storage_** 路径(per §1.4 仿真模式声明 [修订版, 2026-10-09]):

> **注**: 本段措辞已由"零时延功能仿真模式 (functional-mode DMA, zero-delay memcpy)"修订,不再把整个 minimal_v1 SoC 标为 "zero-delay functional-mode SoC"——正确的定位是 **mixed-mode SoC**: 只有 `VramControllerTLM` 单独支持 functional-mode (per §1.4.1),SDMA 经 ChStream 接口到 NOC 后通过 VramController 协议层访问 backing (per §1.4.3),其他模块按各自 cycle-approximate 行为仿真.

- **存储 (host)**: host 通过 BAR1 fast-path (`storage_routing_enabled_=true`) 直接读写 vram_storage_,**不走 ChStream** (BAR 域快路径, 不计 SoC 内部仿真)
- **VramController**: functional-mode 数据路径主入口,SDMA 经 NOC 接入 VramController 后,zero-delay 但控制器协议真实 (per §1.4.1)
- **SDMA**: 必须实现 `ChStream::OutputStreamAdapter<AxiMemBundle>& mem_out()`, 经 `SocInternalNoc` mesh 桥接 VramController (per e1 v1.1 + Change C); SDMA 自身仍可在 cycle-approximate 模式下仿真时序
- **GMMU**: 同步一级页表翻译服务 (functional-mode internal), PT_BASE 寄存器 (经 CR-Bus/LWC, per Change B), 无 TLB, 每次 DMA 即时返回 SoC PA
- **Host backdoor 特权**: 通过 `DGpuBoard::backdoor_read/write` (13 ABI) + `VramControllerTLM::backdoor_*` 直接验证 board-level 内存模块 (per §1.4.2)
- **(后续)** CP 模块: 本期不做, 留接口

### 1.2 非目标

- **NG1**: 不做多级页表/TLB(留待 v2.1 GMMU 扩展,符合 `openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp` 路线)
- **NG2**: 不做 PcieDisplayDevice 集成(D1 已交付,本期聚焦存储侧)
- **NG3**: 不改 23 ABI 签名(冻结,per ADR-088 §D5)
- **NG4**: 不改 SdmaEngineTLM 既有 API(完整复用)
- **NG5**: 不引入第三方依赖

### 1.3 关键设计原则

> **2026-10-09 修订注**: 旧版本原则 2 描述 "SoC 内部 zero-delay 路径" 与 §1.4 修订后定位矛盾 (revision 后, 只有 VramControllerTLM 单独 functional-mode). 本段已按 §1.4 修订版同步重写.

1. **单一 backing 源**: `DGpuBoard::vram_storage_` 是 SoC 全部访存的**真源**,镜像 gem5 `PhysicalMemory::backingStore[].pmem` 角色。MemoryTLM/GMMU/SDMA/host backdoor/VramControllerTLM 都直读直写同一份 `std::vector<uint8_t>`,天然 coherence,无需 flush。
2. **VramControllerTLM 单独支持 functional-mode (zero-delay, 协议真实)**: `VramControllerTLM` 通过 `set_mode(VramControllerMode::FunctionalZeroDelay)` 切到 functional-mode,主数据路径 zero-delay 但经真实控制器协议 (类比 gem5 `AbstractMemory::functionalAccess` + HBM Controller 字段映射)。Phase 0 实际: `sdma.mem_out → pcie_memory.handle_slave_port → vram_controller->handle_functional_request` (pcie_memory 桥接);Phase 1 演进: `sdma.mem_out → soc_internal_noc → vram_noc_adapter → vram_controller->handle_functional_request` (NOC 直连, per v0.5.1 C17 修订). **其他模块** (GMMU/MemoryTLM/SDMA) 仍可走各自 cycle-approximate 行为,SoC 不是统一 zero-delay 仿真。
3. **VramControllerTLM backdoor 接口**: `VramControllerTLM` 通过 `set_backing_view(uint8_t*, uint64_t)` 暴露 backdoor (per ADR-DGPU-10 新名, 继承自 MemoryTLM; `set_backing_store(deprecated)` 为 deprecated wrapper), host 特权 (`DGpuBoard::backdoor_read/write`) 与内部 IP 都可经此接口直读直写 board-level 内存模块 (per `DGpuSoc::sim_terminate` 时验证)。
4. **callback 单一所有权**: callback 完全在 `CallbackWorker` 内(DGPU v2.0.2),本期不增加 callback 类型。
5. **配置注入**: `framebuffer_size_bytes` 从 `pcie_ep.params.bar_sizes[1]` 派生,避免双真相源;顶层 `framebuffer_size_bytes` 仅作 override。

### 1.4 仿真模式声明 (Simulation Mode)

> **2026-10-09 lock 注**: 本段由用户评审指出错误表述,全面重写. **`minimal_v1` 整体不是 "zero-delay functional-mode (LT) 仿真模式 SoC"**;而是一个**混合仿真模式 SoC**——其中只有 **`VramControllerTLM` 单独支持 functional-mode** (zero-delay 但协议真实),其他模块按各自 cycle-approximate 行为仿真.

#### 1.4.1 VramControllerTLM 唯一 functional-mode 模块 (修订, 2026-10-09)

| 维度 | 描述 |
|------|------|
| **唯一模式** | `VramControllerTLM` (继承自 `MemoryTLM`) 是 minimal_v1 SoC 中**唯一**支持 functional-mode (zero-delay, 协议真实) 的模块 (**as Phase 0 Change A target state**, per `cpptlm-vram-controller-functional-integration` Proposal §Why) |
| **配置** | 通过 `set_mode(VramControllerMode::FunctionalZeroDelay)` 切换 (per Change A `cpptlm-vram-controller-functional-integration`); 默认 `TimingCycleApproximate` 行为不变 |
| **数据 path** | `sdma.mem_out → vram_controller->handle_functional_request(addr, size, is_write, data)` 经 chip-internal AXI 路径,控制器协议 (HBM PHY / DFI-lite 字段映射) 真实,zero-delay |
| **回退 fallback** | 当 vram_controller_ 指针为 nullptr 或 mode = TimingCycleApproximate 时,直接 memcpy backing_view_ (per D-AXI v1.4 B11 fallback path) |

#### 1.4.2 VramControllerTLM 支持 board-level memory module backdoor 读写 (修订, 2026-10-09)

`VramControllerTLM` 通过 `set_backing_view(uint8_t*, uint64_t)` 暴露**backdoor 接口** (per ADR-DGPU-10 新名), **host 特权直访 board-level 内存模块** (类比 gem5 `MemBackdoor::ptr()`):

| 入口 | 路径 | 用途 |
|------|------|------|
| **`DGpuBoard::backdoor_read/write`** | `vram_storage_[off..off+len]` | driver 视角验证 (特权只读); 13 ABI 中 `cpptlm_emulator_backdoor_read/write` 直接调用 |
| **`VramControllerTLM::backdoor_read/write`** (新增) | 同样 `vram_storage_[off..off+len]` | 当 SDMA/GMMU 经 chip-internal NOC 走后, internal IP 也可经 VramController backdoor 验证板级内存一致性 (per ADR-DGPU-05) |
| **5 消费者共指针** | `vram_storage_.get()` 是唯一真源 | backdoor / BAR1 fast-path / MemoryTLM backing / PcieMemoryDevice / SDMA-GMMU legacy 指针 全共享同 8GB backing |

#### 1.4.3 SDMA 必须实现 ChStream 接口才能经内部 NOC 接入 (修订, 2026-10-09)

SDMA 必须实现 `cpptlm::OutputStreamAdapter<bundles::AxiMemBundle>& mem_out()` (ChStream 接口), 才能经 `SocInternalNoc` mesh (chip-internal) 接入 `VramControllerTLM` functional-mode 接口:

```
SDMA H2D/D2H 数据 path (实施后):
    sdma.mem_out (AxiMemBundle, ChStream interface)  ←── 必须实现 ChStream 接口
        │
        ▼ 经 soc_internal_noc mesh (per e1 v1.1 / Change C §0.2)
    vram_noc_adapter (per e1 v1.1 design) 接受 NoCFlitBundle
        │
        ▼ AXI4-like 字段映射 (per HBM3 Controller 真协议)
    vram_ctrl->handle_functional_request(addr, size, is_write, data)
        │
        ▼ zero-delay (functional-mode)
    memcpy(vram_storage_)  ←── SDMA 不再直接 memcpy,而是经 VramController 协议层
```

**强约束**:
- SDMA 必须实现 ChStream 接口 (ChStreamModuleBase + OutputStreamAdapter<bundles::AxiMemBundle>);
- 任何模块接入 VramController functional-mode **必须**经 chip-internal NOC / 桥接口,**禁**直 memcpy (`sdma_engine_tlm.cc:241-256` 旧路径已废,per Change A 集成);
- timing-mode (`dgpu_soc_timing_v1.json`) 与 functional-mode (`dgpu_soc_minimal_v1.json`) 共用同一 `VramControllerTLM` 类,仅配置 mode 差异 (per ADR-DGPU-11 H2 互斥).

#### 1.4.4 SoC 仿真模式命名修订 (vs v1.0 旧版本)

| 旧 (错) | 新 (修订) |
|--------|----------|
| `minimal_v1` 整体 = `Functional Mode (LT, zero-delay)` SoC | `minimal_v1` = **混合仿真模式 SoC**: `VramControllerTLM` 单独 functional-mode,其他模块按各自 cycle-approximate 行为 (SDMA/GMMU/MemoryTLM 可走 cycle-approximate timing) |
| 5 条简化 (SDMA 直 memcpy + GMMU sync translate + MemoryTLM seam holder + 无 AXI Crossbar + 端口混合 wire-format) | 上述 5 条已**部分失效**:MemoryTLM 不再是 seam holder,主数据 path 经 VramController (per Change A);SDMA 仍可能在 ChStream AXI path 上 zero-delay 经 NOC (per e1 v1.1);GMMU 仍 sync 翻译 (per Q9) |
| 命名参照 gem5 `MemBackdoor` / `AbstractMemory::backingStore` / `translateFunctional()` | 命名参照 gem5 的**组合** (而非全为 zero-delay):`MemBackdoor` + `AbstractMemory::backingStore` (backing 真源) + `translateFunctional` (GMMU 同步翻译) + **HBM Controller** (`AbstractMemory::functionalAccess`, per VramController 协议层) |
| `LT (Loose Timing)`/`AT (Approximately Timed)` | LT/AT 已不再准确 —— 这是**模块级**而非**SoC 整体** 仿真模式分类,详细见每个模块的 timing 文档 |

#### 1.4.5 名词定义锁 (locked 2026-10-09)

为后续会话与新提案明确这些名词含义, **禁止 silent 漂移**:

| 术语 | 锁定含义 |
|------|---------|
| `functional-mode` (形容词) | **限定模块级**:描述某个 IP 模块是否支持 zero-delay functional 接口 (如 "VramController functional-mode").**不** 用于描述 SoC 整体仿真模式 |
| `VramControllerMode::FunctionalZeroDelay` | VramControllerTLM 内部 mode enum 值,zero-delay 时序 + 协议真实,仅 VramController 用 |
| `VramControllerMode::TimingCycleApproximate` | VramControllerTLM 内部 mode enum 值,cycle-approximate + 时序统计,timing-mode SoC 用 |
| `backdoor` (名词) | VramControllerTLM 提供的 board-level 内存模块直访接口 (`set_backing_view(void*, size_t)` per ADR-DGPU-10 新名; `set_backing_store(deprecated)` 为 deprecated wrapper);可由 host/IP/任何具备特权的 consumer 使用 |
| `混合仿真模式 SoC` (mixed-mode SoC) | minimal_v1 当前定位: 内部不同 IP 模块采用不同仿真模式 (functional / cycle-approximate / cycle-accurate),**不** 强行统一 |

#### 1.4.6 强化: GMMU 仍同步翻译 (per §5 + Q9)

> 注: 即便 SDMA 现在走 ChStream 经 NOC,`GmmuTLM::translate()` 仍然是**同步函数调用** (per `gmmu_tlm.hh:45-71`),立即返回 backing offset (SoC PA = backing offset, per §5.2).GMMU 翻译不模拟 TLB miss 延迟 / page walk cycle 数 — 这是 GMMU 自身的 functional-mode 特性 (等价于 gem5 `translateFunctional`),与 SoC 是否 zero-delay 整体仿真无关.

> 进一步: **GMMU PTE fetch 走 VramControllerTLM 专用 functional port** (NVIDIA H100 GMMU dedicated memory client 模式, per v0.5 修订 + Change C 三锁叠加裁决). 与 §1.4.3 强约束不矛盾 — GMMU 是 dedicated memory client, 直连 VramController, **不经 pcie_memory, 不经 NOC mesh**. GmmuTLM 自身的 `translate()` 同步返回是 GMMU 内部的 (语义保持 PcieTlpBundle 容器 + functional 内部封装; PTE fetch 协议级走 VramControllerTLM::handle_functional_request 的 functional-mode 通道).

#### 1.4.7 Hard Invariant (lessons from §1.4.4 旧 vs 新 修订对照表 + §1.4.5 名词定义锁 + §1.4.6 GMMU 仍同步翻译)

- 任何后续 change 表述 "X 是 minimal_v1 zero-delay functional-mode SoC" 视为**违反业务定位**;
- 任何后续 change 表述 "SDMA 直 memcpy" 而不经 VramController 协议层,视为**违反 Hard Invariant**;
- 任何后续 change 描述 "MemoryTLM 是 seam holder" 视为**历史陈旧描述**(per Change A 集成后,MemoryTLM 不再是 seam holder,VramController 是新 seam holder).

---

**模式依据**: 本次重写参照 gem5 `MemBackdoor` (`MemBackdoor::ptr()`) + `AbstractMemory::backingStore` + **HBM Controller** (`AbstractMemory::functionalAccess`, per VramController 协议层) + `translateFunctional` (GMMU 同步翻译). **完整正确位置**: TL-DR (This Layered Discrepancy Resolution) —— 模块级仿真模式由各自 IP 的文档明确,SoC 是这些 IP 的组合.

**功能模式简化项表**(避免被误读为"未实施"):

| 简化项 | 当前实现 | gem5 等价物 | 解除条件 (v1.1+) |
|--------|----------|-------------|------------------|
| SDMA H2D/D2H 零时 memcpy | `sdma_engine_tlm.cc:241-256` | `MemBackdoor` | 引入 CrossbarTLM + SDMA 切 AXI master |
| GMMU 同步 translate | `gmmu_tlm.hh:45-71` | `translateFunctional()` | GMMU 异步 page walk |
| MemoryTLM 无 caller | `memory_tlm.hh:142 legacy 路径` | (无 gem5 等价,seam only) | 接真 ChStream 数据路径 |
| 无 SoC 内部 AXI | (无 Interconnect 模块) | (timing mode only) | 引入 CrossbarTLM |
| SDMA 5 端口混合 wire-format | `sdma_engine_tlm.hh:82-89` | (无,TLM 边界自由) | SDMA mem 端口切 AxiMemBundle |

**与 D-AXI v1.4 的关系**: 本文档代表 **Minimal SoC 业务架构视图**(基于 D-AXI v1.4 已实施真相)。代码权威源是 D-AXI v1.4(commit 429327d)+ `dgpu_soc_minimal_v1.json` 配置。

---

## 2. 模块清单与拓扑

### 2.1 拓扑

```
DGpuBoard (shell, 23 ABI 冻结, v2.0.2)
└── DGpuSoc (SimModule 容器)
    ├── PcieEndpointIP          (SimModule, 1 PF + 16 VF;BAR0 4KB MMIO + BAR1 16MB fast-path 窗口 + BAR2 8GB vram aperture via PcieMemoryDevice)
    │   ├── PcieBarRouter       (BAR0 寄存器表: GMMU_PT_BASE_LO/HI / GMMU_CTRL / SDMA_STATUS)
    │   └── memory_device_      (raw ptr → PcieMemoryDevice, Phase 3 T1.2/T4.2 N12 注入)
    ├── PcieMemoryDevice        (ChStreamModuleBase, 2 SlavePorts;SDMA mem_out + GMMU req_out;backing_view_ 注入式)
    ├── SdmaEngineTLM           (5-port, ring mode + fence, translate_cb → GmmuTLM, N3 retry driver + N7 slot-2 resp)
    ├── GmmuTLM                 (新建: 一级页表翻译, 无 TLB, 异步 translate -EAGAIN)
    └── CompletionRingTLM       (fence 完成 → MSI-X)
```

> **Phase 3 T4 (v1.8 F12)**: `MemoryTLM` 已从 `dgpu_soc_minimal_v1.json` 移除,`PcieMemoryDevice` 取代其 minimal_v1 存储职责 (零连接零消费者冗余);`bind_memory_backings()` N12 条件注入:有 `pcie_memory` → 注入 PcieMemoryDevice + `ep->set_memory_device()`,否则 legacy `memory` fallback。


### 2.2 模块清单(7 个,3 个新建)

| 组件 | 类型 | 来源 | 职责 |
|------|------|------|------|
| `PcieEndpointIP` | 17-port SimModule | ✅ 复用 | BAR 存储 + config space + MSI-X;`set_memory_device` 注入 (T1.2) |
| `PcieMemoryDevice` | 2-port ChStreamModule | ✅ D2 复用 | BAR2 8GB vram aperture 外观层;backing_view_ 注入式 (v1.4 B7) |
| `SdmaEngineTLM` | 5-port ChStreamModule | ✅ 复用 | DMA 搬运 (H2D/D2H), ring mode, fence, N3 retry driver |
| `CompletionRingTLM` | 4-port ChStreamModule | ✅ 复用 | fence 完成汇聚 → MSI-X vector 0 |
| `PcieBarRouter` | 工具 | ✅ 复用 | BAR0 寄存器表 (PT_BASE_LO/HI 等) |
| **`GmmuTLM`** | **新建** SimModule | 🆕 | 一级页表翻译 (PT_BASE 寄存器, 无 TLB, 异步 -EAGAIN) |
| `DGpuBoard` + `DGpuSoc` | shell + 容器 | ✅ 复用(v2.0.2) | 23 ABI 入口 + 路由 + 新 vram_storage_ + BAR2 fast-path |

### 2.3 不在 v1.0 范围的组件(后续工作)

- **CP** (Compute Pipeline): v2.0 后续
- **Display IO device** (D1): 已交付 v1.1.1,本期 `display_routing_enabled=false` 不启用
- **Memory Device** (D2): `openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp` 提案;本期通过 vram_storage_ 简化实现其能力
- **Host Bypass / Root Complex**: Phase 8 已交付,本期测试用 host 直连模式(经 `DGpuBoard::mmio_*`)

### 2.4 5-IP Real-IP Boundary (dgpu-realip-refactor P3 + soc-internal-noc-minimal, 进行中)

> **状态**: 🟡 进行中。Per plan `2026-10-06-dgpu-realip-refactor.md` Status Update (2026-10-07, P3 Amendment + Phase A.2)。当前 SoC 是 6 模块 god-class (D-AXI v1.4 minimal_v1); P3 阶段将抽象层提升为 5 个**真实物理 IP 边界**对齐的 TLM 模块(可对照 RTL 实现硅片)。完成 PR-B Phase Gate (B1/B2/B3 全收, `ad13991d` + `385c7c35` + `e59004b9`)。Phase A.2 已修 P0 双阻塞(短名注册 + MUST NOT #3 措辞) + P1 5 项修订。2026-10-08 新增 `SocInternalNoc` (per openspec/changes/cpptlm-dgpu-soc-internal-noc-minimal), SoC 范围 4 target 路由骨架 (PcieControllerIP / SdmaEngineTLM / GmmuTLM / VramControllerTLM), 端口契约对齐 Arteris FlexNoC / Custom Mesh IP。

| IP 边界 | 真实芯片对应 | CppTLM 类 (现状) | 端口契约 | State ownership |
|---------|--------------|---------------------|------------|------------------|
| **`PcieControllerIP`** (顶层 facade) | DWC i5122 / Cadence PCIe Controller | `include/tlm/pcie/pcie_controller_ip.hh` (B1 组合 refactor: `SimModule` + `PcieEndpointIP* ep_` non-owning) | 4 BAR decode (`Bar0Mmio` / `Bar1FbFastPath` / `Bar2VramAperture` / `Unknown`) | **stateless facade** — 所有 state 经 `ep_->...` 委托 |
| **`PcieAxiBridgeIP`** (EP composite, NOT 顶层) | Synopsys AXI↔PCIe bridge / ChipLink | `include/tlm/pcie/pcie_axi_bridge_ip.hh` (类保留, `REGISTER_MODULE` REMOVED per Oracle bg_c60653e0 §4) | AXI4-slave → EP-internal composite (lazy_init) | EP-internal, JSON 顶层不可实例化 |
| **`GpuInternalFabric`** (新增, P3 T22) | Custom mesh / Arteris NoC IP | `include/tlm/cluster/gpu_internal_fabric.hh` (scaffold commit `95a68889`; T22b pin-contract test commit `139534ac` + Phase A.2 短名注册 commit pending) | 4 端口 (默认 = CrossbarTLM::NUM_PORTS) | non-owning `crossbar_` raw ptr (ModuleFactory owns) |
| **`SocInternalNoc`** (E0 路由骨架, 2026-10-08, soc-internal-noc-minimal; **E.1 v1.1 DataNoC only**, 2026-10-09 ✅ archived 2026-10-09 — 代码已落地 per `soc_internal_noc.hh:114-151`, 设计意图源保留) | Arteris FlexNoC / Custom Mesh IP (per Driver 视角 driver 路径闭环); **ARM CHI 4 通道 + 4 VC 物理对应** (per Oracle RTL 调研) | `include/tlm/cluster/soc_internal_noc.hh` (E0 stub; **E.1 v1.1 升级 route_flit 实际推送 + send_request/response API + vc_counts_[4] 计数器**) | `NoCFlitBundle` **16 字段** (per `include/bundles/noc_bundles_tlm.hh:26-50`, v2.1 决策: 统一类型 + `flit_category` 区分 REQ/RESP); **E.1 v1.1 0 新增 dual bundle** (与既有 v2.1 决策对齐); 编译期 `BUNDLE_FIELD_WIDTH` 锁 16 字段宽度 | **E.1 v1.1 范围缩减**: stateless 路由骨架 + **DataNoC only** (per e1 v1.1): 仅承载 **SDMA bulk DMA 流量** (SDMA→VRAM); PT_BASE / doorbell / page walk 走**独立通道** deferred to E.2 (ArchForge); 4 raw target ptr (PcieControllerIP / SdmaEngineTLM / GmmuTLM / VramControllerTLM) + **2 计数器** (`flits_routed_`/`unknown_dst_count_` 既有 + **`flits_dispatched_` + `vc_counts_[4]` 新增**); **2 adapter** (SDMANocAdapter + VramNocAdapter, **删 PcieNocAdapter/GMMUNocAdapter** per CRITICAL C1/C2); VC 计数器按 `flit_category` 分桶 (REQ=VC0/RSP=VC1/DAT=VC2/SNP=VC3) 为 E.2 multi-VC 铺设; **code already implemented per `soc_internal_noc.hh:114-151`** (per Metis M-1/M-2, 实际已实现, 非 Proposed) |
| **`VramControllerTLM`** (timing-mode 已有, P3 promote) | NVIDIA FB / AMD MC (HBM PHY, DFI interface) | `include/tlm/vram_controller_tlm.hh` (Phase 4 T4 timing-mvp 已实现; P3 promote via JSON top-level per spec §6.4 E5) | AXI4-slave (from GpuInternalFabric) + DRAM command abstraction (DFI-lite) internal | Backing ptr (injected from DGpuBoard `vram_storage_` per ADR-DGPU-05) + row buffer + bandwidth counters |

**保留模块 (P3 promote 后)**:
- `PcieEndpointIP` — 保留为 EP-internal composite (PcieAxiBridgeIP 与 PcieControllerIP facade 都依赖其 bar_router/bar_store/bar_sizes)
- `PcieMemoryDevice` — 保留 (D2 v1.1, 退化 PCIe 外观层; backing 来自 DGpuBoard `vram_storage_`)
- `GmmuTLM` — 维持 + 服务 timing-controller (上层)
- `SdmaEngineTLM` — 5-port 复用 (mem_in/mem_out 切 AxiMemBundle, desc_in/done_out/host_out 保持 PcieTlpBundle)
- `CompletionRingTLM` — 维持 + fence → MSI-X

**P3 实施约束 (per Amendment 修订)**:
- T23C backing 注入走 `DGpuBoard::bind_memory_backings()` (per ADR-DGPU-05 决策 2, vram_storage_ 所有权归 DGpuBoard)
- T23B JSON 顶层嵌套 DGpuSoc 容器 (per architecture.md §7.3 硬约束)
- T23A 删 `dgpu_soc_v2_realip_p2.json:17` 死 PcieAxiBridgeIP, **保留类文件 + REGISTER 注释** (本次 Phase B 已落地)
- T22b GpuInternalFabric pin-contract test (本次 Phase B 已落地, 含 forward-decl bug 修复)
- 3 MUST NOT: 不修改 VramControllerTLM 类定义 / 不删 PcieAxiBridgeIP 类 / 不改任何注册宏

**进度同步**:
- 2026-10-02 Phase 1-7 PCIe EP 微架构 (链路层 / 编码 / PHY / SR-IOV / AXI Adapter / Mapper / Host Bypass + RC) **已交付**
- 2026-10-02 PR-B Phase Gate B2 v2 (dgpu_board_shell.cc:319-353 / :433-467 闸门) **已交付**
- 2026-10-07 PR-B Phase Gate B1 (组合 refactor commit `ad13991d`, tag `pr-b-phase-gate-fix-b1`) **已交付**
- 🟡 Phase B 并行任务 (本次提交): T23A 死引用删除 + T22b fabric pin-contract + T27 文档同步 3 文件
- ⏭ Phase C 剩余: T23B / T23C / T25A / T25B / T26 / T28

### 2.4.1 Chip-Internal Port Contract — fabric ↔ vram_ctrl (Change A, 2026-10-08)

Per `openspec/changes/dgpu-realip-pin-contract/` (Change A scope, 2026-10-08, **Momus 修订**):

| Src Port | → | Dst Port | Bundle | Direction |
|----------|---|------------|--------|-----------|
| `GpuInternalFabric::vram_ctrl_` | → | `VramControllerTLM::fabric_` | `Fabric2VramCtrlBundle` | Programmatic (setter-only, per Phase 8 HB/RC precedent + cpptlm-p2-integration-unblock D7) |

**Fabric2VramCtrlBundle 字段表** (8 字段, max width 64-bit, per `include/bundles/dgpu_fabric_bundles_tlm.hh`):

| field | width | semantic |
|------|:-----:|----------|
| `address` | 64 | DRAM byte address |
| `size` | 8 | transaction size in bytes |
| `is_write` | 1 | 1=write, 0=read |
| `data` | 64 | write data (≤64-bit avoids ch_uint wide-path shim) |
| `transaction_id` | 32 | request id (changed from CacheReqBundle 64→32) |
| `resp_data` | 64 | read response data |
| `error_code` | 8 | 0=OK, 1=DECERR, 2=SLVERR |
| `chip_select` | 8 | chip-select (multi-chip HCS topology) |

**Programmatic bridge** (per Oracle AD-4): `DGpuBoard::bind_memory_backings()` 末尾调
```cpp
fabric->set_vram_target(vram_ctrl);
vram_ctrl->set_fabric_source(fabric);
```
通过 `soc_->getInternalInstance("fabric")` / `"vram_ctrl"` 取真实指针 (模块名见 `configs/dgpu_soc_v2_realip_p2.json:22,33`)。JSON 不声明此端口 (避免 §14.2.3 静默丢弃)。

**Compile-time contract**: `include/utils/port_contract_asserts.hh` 提供 1 macro (Momus 修订版):
- `BUNDLE_FIELD_WIDTH(BundleType, Member, ExpectedWidth)` — width check via `decltype(&BundleType::Member)::width == ExpectedWidth` (依赖 `cpphdl_types.hh` 加的 `static constexpr unsigned width = W;`)
- **已删除** (Momus REJECT 修复): `PORT_IS_INITIATOR` / `PORT_IS_TARGET` (基于不存在的 `chstream::ChStreamInitiatorPort<Bundle>` 模板 — 实际 `cpptlm::ChStreamInitiatorPort` 是非模板类)

**运行时契约**: `test/test_realip_pin_contract.cc` TEST_CASE #3 round-trip — 19 assertions in 3 test cases PASS per `[realip][pin-contract]` tag。

**Out of Change A scope** (deferred to Change B/C):
- `match_bar` BAR0 offset-aware sub-window (Oracle C2 修复) — **REJECTED in Change B by Oracle `bg_14b6dd83`** (0x800000 物理不可能 in 4KB BAR0; 改 target-side decode characterization + regression lock,per §2.4.2)
- Legacy flag 删除 (5 routing_enabled + PciePath enum) — Change C,门控于 Change B 完成
- `mmio_read/write` 简化 (Strangler gate 保留) — Change C,门控于 Change B 完成
- JSON 顶层 `routing_enabled` 字段退役 — Change C,门控于 Change B 完成

### 2.4.2 Facade Production Wiring (Change B + Change C, 2026-10-08)

Per `openspec/changes/dgpu-realip-pin-contract/specs/dgpu-4ip-port-contract/spec.md`:

| Source | Dst | Action |
|--------|-----|--------|
| `DGpuBoard::init()` 公共路径(早于 timing/functional 分支) | `PcieControllerIP::attach_endpoint(ep)` | 程序化注入 EP 引用 |
| `DGpuBoard::init()` | `DGpuBoard::set_pcie_controller_ip(ctrl)` | 激活 facade |

**Dispatch (Change C)**: mmio_read/write + backdoor 共 **11 处** flag-keyed 分支 (mmio_read 3 + mmio_write 4 + backdoor 4) key 到 `match_bar` verdict + presence 谓词 (per Metis `bg_fd20b1ed` A1 校正: Plan 原 "9 处" 错数)。
- mmio_read/write: `switch (verdict)` 4 cases (Bar0Mmio / Bar1FbFastPath / Bar2VramAperture / default Unknown)
  - Bar0Mmio: display first (gmmu/display 互斥, per AGENTS.md) → return; GMMU sub-window (offset<0x100, len>=4, fall-through)
  - Bar1FbFastPath: doorbell exclusion (offset != kBar1DoorbellOffset) + storage_routing_enabled_ + framebuffer_ptr_ presence
  - Bar2VramAperture: memory_routing_enabled_ + has_memory_device() presence
  - default (Unknown): falls through to mmio_regs_ + inject_q_ + doorbell handler
- backdoor: display → memory → framebuffer → vram_segments_ (4 处, 复用 632/687 framebuffer 模板)
- 保留 4 条非 facade 语义: INV-A D3 power gate 最前 / kBar1DoorbellOffset 排除 / target-presence 谓词 / GMMU sub-window + display-first 优先级

**Null-controller fallback (per Metis `bg_fd20b1ed` A3)**: `controller_ip_ == nullptr || controller_ip_->endpoint() == nullptr` 时,用 `classify_naive()` helper (mirror 抽出) 替代 `match_bar`,防止无 controller 场景所有路由丢失 (无 controller 单测如 `[a3] dgpu_mini_cfg()` / `[pcie-bypass-tlp]` 验证)。

**Strangler gate (Change C 终态)**:
- **Release build**: `std::abort` 路径删除,`expected` mirror 计算删除 (无字符串 + 无运行时开销); `strings | grep -c "Strangler gate"` = 0 (G-C6)
- **DEBUG build**: 保留 `assert(verdict == naive && ...)` 防止 `controller->ep_` vs `board->pcie_ep()` 不对称静默漂移 (per AC-C3 Metis A6/B5 修订;原 "mirror 与 match_bar 同源" 事实错误 — mirror 是独立朴素计算)
- 回归锁: B.3' `[realip][facade-wiring]` characterization suite 8/8 (行为逐位一致)

**Target-side decode 实际实现** (per Metis `bg_03094fec` 修订 — 收敛描述, 不夸大):
- `DGpuBoard::mmio_write`: GMMU 寄存器窗 (BAR0 offset<0x100) 走 `gmmu_->set_*` (write-only), display 整窗走 `ep->display_device().mmio_write` (whole-BAR fast path)
- `DGpuBoard::mmio_read`: **无 GMMU 读路径** — GMMU 寄存器读回走 `mmio_regs_` roundtrip (mmio_write 时 write-through 写入 mmio_regs_); display 整窗走 `ep->display_device().mmio_read`
- 后续 D3 GpuInternalFabric target decode 是 **future scope**, 不在 Change B 内承诺

**Strangler gate 是 observation-only** (per Metis `bg_03094fec` 修订):
- gate 仅在 `match_bar` verdict ≠ mirror 期望时调 `std::abort` (`dgpu_board_shell.cc:344,457`)
- gate 不路由分发 — 分发仍走 legacy routing flag (`display_routing_enabled_` / `gmmu_routing_enabled_` / `memory_routing_enabled_` 等)
- Change C 删除 legacy flag 时, **不可**把 gate verdict 当作分发依据 — 必须先把分发路径从 legacy flag 迁到 facade verdict, 再删 flag

**4KB BAR0 子窗口 GMMU vs display 不可 offset 区分**:
- GMMU 寄存器 (0x00-0x1F, write-only via mmio_write, read via mmio_regs_ roundtrip)
- Display 寄存器 (0x00-0xF0, 整窗走 display_device)
- 在任何单一 config 中**从不共存** (`gmmu_routing_enabled_` 与 `display_routing_enabled_` 互斥门控)
- 因此 offset-level 子分发**物理上不可区分**, controller 端必须 whole-BAR 锁定

---

### 2.4.3 DataNoC Only 范围声明 (E.1 v1.1, 2026-10-09)

Per [cpptlm-dgpu-soc-internal-noc-e1 v1.1 design.md §3.5](../../openspec/changes/cpptlm-dgpu-soc-internal-noc-e1/design.md), **E.1 v1.1 将 SocInternalNoc 范围从"全 SoC 4 target 路由"重定位为 DataNoC only**:

**E.1 v1.1 承载范围**:
- ✅ **SDMA bulk DMA 流量** (SDMA→VramControllerTLM, 经 SDMANocAdapter + VramNocAdapter)
- ✅ **DataNoC 物理保真**: VramControllerTLM 内部 NoC→AXI4/Fabric2VramCtrlBundle 字段映射 (per HBM3 Controller 真实协议)

**E.1 v1.1 明确排除** (deferred to E.2 / ArchForge):
- ❌ **PT_BASE 配置写** (`dgpu_board_shell.cc:467` `gmmu_->set_pt_base_lo(val32)` 直调, **不进 SocInternalNoc**)
- ❌ **BAR0 寄存器读** (走既有 `mmio_regs_` roundtrip 路径)
- ❌ **doorbell 提交** (走既有 `SdmaEngineTLM` 5 端口 PcieTlpBundle 路径)
- ❌ **GMMU page walk** (`gmmu_tlm.hh:105-117` async PcieTlpBundle MasterPort, **走独立 memory client**, 不经 SocInternalNoc)
- ❌ **ConfigBus / TranslationBus** (deferred to E.2 multi-VC NoC in ArchForge, per `cpptlm-dgpu-soc-internal-noc-minimal` §8 + AGENTS.md 季度复审产物)

**物理 IP 对应** (per Oracle RTL 调研, 2026-10-09):
| 真实 IP | 物理路径 | E.1 v1.1 处理 |
|---------|---------|--------------|
| **NVIDIA H100 GMMU** | 专用 memory client (bypass mesh NoC) | ✅ GMMU 不经 SocInternalNoc |
| **ARM CMN-700 + SMMUv3** | DVM 独立通道 | ✅ 同上 |
| **Synopsys DW PCIe** | AXI Bridge Module 输出 AXI4 | ✅ PcieControllerIP 不输出 NoC bundle |
| **ARM CHI 协议** | 4 通道 (REQ/RSP/DAT/SNP) + 4 独立 VC | ✅ `flit_category` + `vc_counts_[4]` |
| **HBM3 Controller** | AXI4 5 通道 (AW/W/AR/R/B) | ✅ VramNocAdapter 内部 AXI4 转换 |
| **PCIe EP MSI-X** | 硬件 MSI-X Generator | ✅ 不引用 `PcieEndpointIP::trigger_msix` |

**修复的 P0 / CRITICAL / MAJOR** (per 2026-10-09 双 agent 复审):
- ✅ **P0-1**: AC-E1-4 vs scope 不可达 → v1.1 改写为 adapter-level 注入
- ✅ **P0-2**: GMMU TLB 重复实现 → 删 Task 4.2, `translate_timing()` 已实现 (`gmmu_tlm.hh:147-193`)
- ✅ **P0-3**: fence→MSI-X 重复 + 幽灵 API → 删 Task 4.3, 链路已闭环 (`sdma_engine_tlm.cc:738` → `dgpu_board_shell.cc:1125`)
- ✅ **P0-4**: GMMU PTE fetch 三处矛盾 → design §2.2 删 GMMU→NoC 边, fuzz T1 改 `≤ 注入总数`
- ✅ **CRITICAL C1**: PCIe Controller 边界混淆 → 不实现 `emit_noc_request`, 复用 PcieAxiAdapter AXI4 路径
- ✅ **CRITICAL C2**: GMMU page walk 经 NoC 物理不可能 → GMMU 走独立 PcieTlpBundle MasterPort
- ✅ **MAJOR M1**: 单一 NoC 承载 4 类流量 → DataNoC only 重定位
- ✅ **MAJOR M2**: REQ/RESP dual bundle 漏 DAT/SNP → 不创建, 用现有 NoCFlitBundle + `flit_category`
- ✅ **MAJOR M4**: VramControllerTLM 协议错配 → VramNocAdapter 内部 NoC→Fabric2VramCtrlBundle 转换

**估时**: v1.0 16d → **v1.1 10d** (-37%, 主要省在删除 5 项重复实现 + 3 项错误实现)。

---

## 2.5 HostPcieTestStub 仿真测试桩 (cpptlm-pcie-host-test-flr-rebar v1.1, 2026-10-09)

Per [cpptlm-pcie-host-test-flr-rebar v1.1 design.md §3.1](../../openspec/changes/cpptlm-pcie-host-test-flr-rebar/design.md), HostPcieTestStub 是 **in-process C++ 仿真测试桩**, **不依赖 UsrLinuxEmu 跨仓 driver**。

**统一构造签名** (v1.1 修复 v1.0 三处不一致):
```cpp
HostPcieTestStub(DGpuBoard* board, const std::string& config_path);
```

**API surface** (in `include/tlm/pcie/host_pcie_test_stub.hh`):
```cpp
class HostPcieTestStub {
public:
    // TLP 流量生成
    void send_config_read(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset, uint8_t width);
    void send_config_write(uint8_t bus, uint8_t device, uint8_t func, uint16_t offset, uint8_t width, uint32_t value);
    void send_mmio_read(uint8_t bar, uint64_t offset, uint32_t size);
    void send_mmio_write(uint8_t bar, uint64_t offset, uint32_t size, const uint8_t* data);

    // 错误注入钩子 (X3 AER 基础设施)
    void inject_error(ErrorType type, ErrorParams params);

    // 状态触发器 (v1.1: 直接调既有 PcieEndpointIP EP API, 0 新增 ABI)
    void trigger_flr() { ep_->flr_pf(); }                                   // 既有 PcieEndpointIP::flr_pf (per pcie_endpoint_ip.hh:201-202)
    void trigger_resize_bar(uint8_t bar_idx, uint64_t new_size) {            // uint64, 修 v1.0 uint32 溢出
        ep_->enable_resizable_bar(bar_idx);                                  // 既有 PcieEndpointIP::enable_resizable_bar (per pcie_endpoint_ip.cc:670-694)
        // new_size 由 config write 路径先落存 ResizableBar::new_size_bytes_, 然后 ep_->on_bar_resize(bar_idx) 触发实际 resize
    }

    // MSI-X 订阅 (既有 msix_update_pending 路径)
    void register_msix_handler(std::function<void(uint32_t vector)> handler);

private:
    PcieTlpSerializer serializer_;
    DGpuBoard* board_;
    std::string config_path_;
    std::function<void(uint32_t)> msix_handler_;
};
```

**修复 v1.0 三处不一致**:
| v1.0 文本 | v1.1 修复 |
|----------|----------|
| design §3.1: `HostPcieTestStub(DGpuBoard* board)` | ✅ 统一为 `(DGpuBoard* board, const std::string& config_path)` |
| spec lifecycle: `HostPcieTestStub(emu, config)` | ✅ 同上 |
| tasks Phase 2 T1: "DGpuBoard 句柄注入" | ✅ 同上 |

**修复 v1.0 TLP 发送接收路径 ambiguity**:
- v1.0 serializer encode 出 PcieTlpBundle → 怎么映射到 ABI 调用？设计未写
- v1.1 **消除**: HostPcieTestStub 直接调既有 `PcieEndpointIP::flr_pf()/flr_vf() + enable_resizable_bar()/on_bar_resize() + mmio_read/write + config_read/write` C++ 入口, **无 serializer→ABI 映射层** (0 新增 ABI, per ADR-DGPU-08 §2.3)

**修复 v1.0 ADR-DGPU-08 §2.3 0 新增约束违反**:
- v1.0 提议新增 `cpptlm_emulator_trigger_flr()` + `cpptlm_emulator_resize_bar()` 在新头文件 `cpptlm_emulator_flr.h` / `cpptlm_emulator_resizable_bar.h`
- v1.1 **删除**（per CRITICAL C2）：HostPcieTestStub 是 in-process C++ fixture, UsrLinuxEmu 已排除（per Non-Goal #4）, **0 ABI 消费者**

---

## 2.6 FLR + Resizable BAR 测试扩展 (cpptlm-pcie-host-test-flr-rebar v1.1, 2026-10-09)

### 2.6.1 FLR 协议层测试 (v1.1 复用既有)

Per [cpptlm-pcie-host-test-flr-rebar v1.1 design.md §3.2](../../openspec/changes/cpptlm-pcie-host-test-flr-rebar/design.md), **E.1 v1.1 不重建 PcieFlrHandler 4-state 状态机**, **复用既有** `PcieEndpointIP::flr_pf()/flr_vf()` (per `pcie_endpoint_ip.hh:201-202`) + `pool_.flr_pf()` 全状态复位 (per `pcie_sriov_vf_pool_tlm.hh:110`).

**真实 FLR 链路** (per PCIe Base Spec §6.6.2):
```
Host → HostPcieTestStub::trigger_flr()
    │
    └─► PcieEndpointIP::flr_pf() (既有 API, per `pcie_endpoint_ip.hh:201-202`)
         │
         └─► pool_.flr_pf() 全状态复位 (既有 SR-IOV VF pool reset)
                   - 17 端口 config space 寄存器 reset
                   - per-VF MSI-X + FC + seq# 独立 reset
                   - CompletionTracker::flr_pf reset
                   - **MSI-X PBA 全清** (per PCIe Base Spec §6.6.2: FLR 复位所有 Function state)
```

**修复 v1.0 spec 错误** (3 处):
- v1.0 spec: "Device Control Register offset 0x04 bit 15" (错)
- v1.1 spec: **"Device Control Register offset 0x08 bit 15"** (per PCIe Base Spec §7.5.3.4 in PCIe 4.0/5.0 / §7.8.4 in PCIe 3.x: Initiate FLR)

**修复 v1.0 spec 矛盾** (FLR + MSI-X pending):
- v1.0 `pcie-flr/spec.md`: FLR 后 MSI-X pending 不清零
- v1.0 `pcie-resizable-bar-runtime/spec.md`: FLR 后 MSI-X PBA 全清
- v1.1 统一: **FLR 后 MSI-X PBA 全清** (设计选择 per PCIe Base Spec §6.6.2; MSI-X Table/PBA 驻留 BAR memory 是否清零为设计选择)

### 2.6.2 Resizable BAR Runtime 测试 (v1.1 复用既有 + uint64)

Per [cpptlm-pcie-host-test-flr-rebar v1.1 design.md §3.3](../../openspec/changes/cpptlm-pcie-host-test-flr-rebar/design.md), **v1.1 不新建 ResizableBarRuntime 类**, **复用既有** `enable_resizable_bar()/on_bar_resize()` API (per `pcie_endpoint_ip.cc:670-694`) + `ResizableBar` INV-C 状态机 (per `pcie_resizable_bar.hh`).

**关键修复**: `ResizableBar::new_size_bytes_` 字段类型 `uint32_t` → **`uint64_t`** (per CRITICAL C3: 4GB resize 溢出修复):
```cpp
class ResizableBar {
public:
    bool reprogram_size(uint64_t new_size_bytes) noexcept {  // ← uint64 (v1.1)
        if (state_ == State::Enabled) return false;  // INV-C: 必须先 disable
        new_size_bytes_ = new_size_bytes;  // ← uint64
        state_ = State::Programming;
        return true;
    }
    [[nodiscard]] uint64_t size_bytes() const noexcept { return new_size_bytes_; }  // ← uint64
private:
    uint64_t new_size_bytes_ = 0;  // ← uint64 (v1.1 修复)
};
```

**真实 Resizable BAR 链路** (per PCIe Base Spec §7.8.6):
```
Host → HostPcieTestStub::trigger_resize_bar(2, 0x100000000ULL)  // 4GB, uint64
    │
    └─► PcieEndpointIP::enable_resizable_bar(2)  // 既有 (INV-C disable/reprogram/enable, per `pcie_endpoint_ip.cc:670-694`)
         │
         ├─► rb->reprogram_size(0x100000000ULL)  // ← uint64 (v1.1 修复, ResizableBar::new_size_bytes_)
         └─► ep->on_bar_resize(2)  // 既有 callback (1 参签名, per `pcie_endpoint_ip.cc:670-694`)
```

**修复 v1.0 spec 编码错误**:
- v1.0 spec: "0x18 (BAR2 register): low dword = 4GB encoding, high dword = 0 (32-bit addressing)" (错, 4GB = 2^32 需要 33 bits)
- v1.1 spec: **"0x18 low dword = 0, 0x1C (BAR3 upper dword, 64-bit BAR pair) high dword = 1"** (per PCIe Base Spec §7.8.6: 64-bit BAR pair 编码)

**修复 v1.0 路由归属三处不一致**:
| v1.0 文本 | v1.1 修复 |
|----------|----------|
| design §2.4: `PcieControllerIP::resize_bar()` (不存在方法) | ✅ 改为 `PcieEndpointIP::enable_resizable_bar(bar_idx) + on_bar_resize(bar_idx)` (1 参签名, per `pcie_endpoint_ip.cc:670-694`) |
| tasks Phase 7.1: "DGpuBoard 添加入口 + 路由到 PcieControllerIP::resize_bar()" | ✅ 同上 |
| spec `pcie-resizable-bar-runtime/resizable-bar-abi-entry`: `DGpuBoard::handle_resize_bar()` | ✅ v1.1 HostPcieTestStub 调用既有 `PcieEndpointIP::enable_resizable_bar(bar_idx) + on_bar_resize(bar_idx)` |

---


## §3+ 指针化段 (Section 3+ Simplified to Pointers, 2026-10-09 v0.4)

> **本段由用户评审 v0.4 后简化**: 原 §3-§14 内容(含存储子系统 gem5 映射 + BAR 路由表 + GMMU 设计 + SDMA 链路 + JSON 配置 + Invariants + 实施路径 + 测试策略 + 兼容性 + 度量 + 风险 + 推迟项)**与 §0.1 业务定位 + 4 个 Phase 0-3 OpenSpec change 4 件套严重重叠**,按"保留主要的内容"原则指针化。完整内容在原始 git history (`git log -- docs/designs/dgpu-soc/architecture.md`) 中可查。

| 原章节 | 指针 (实际权威源) |
|--------|------------------|
| §3 存储子系统 + §3.1-§3.6 | 已合并到 §0.1 原则 2 + D-AXI v1.4 §7 + ADR-DGPU-05 单一 VRAM 真源 |
| §4 BAR 路由与读写路径 + §4.1-§4.4 | 已合并到 §0.1 原则 4 + Change B 集成 BAR0/1/2 (修复 Gap H + K) |
| §5 GMMU 设计 + §5.1 API 骨架 + §5.2 翻译流程 | 已由 Phase 1 Change C `cpptlm-gmmu-pte-bus-correction` 接管 (修复 Gap I + G2 + G1) |
| §6 SDMA / Fence / MSI-X 链路 | 已由 Phase 0 Change B 接管 Doorbell (Gap J) + Phase 2 Change E `cpptlm-msix-generator-ip` 接管 MSI-X (Gap L) |
| §7 JSON 配置 + §7.1-§7.4 | `configs/dgpu_soc_minimal_v1.json` + `configs/dgpu_soc_timing_v1.json` 实际配置 + §0.2 总体实施计划 4 Phases / 6 Changes |
| §8 关键不变性 (Inv-1~Inv-6) | Inv-1/2 已合并到 §0.1 原则 2; Inv-3-6 与 §0.1 原则 1/3 (driver-visible + 双重无注入约束) 合并 |
| §9 实施路径 (4 阶段 3 周) | 已被 §0.2 总体实施计划 (4 Phases 6 Changes) 替代 |
| §10 测试策略 + §10.1-§10.3 | 已由 §0.2 Acceptance + 各 Change `tasks.md` TDD 5-step 接管 |
| §11 兼容性约束 | 已合并到 §0.1 原则 + §0.1 同步规则 (23 ABI 零 diff) + §0.1 Frozen Surface 4 frozen + 1 quasi |
| §12 度量与验证 + §12.1 DoD + §12.2 性能 | §0.2 Acceptance (DoD) + 各 Change tasks.md Phase 5 (性能 sanity-check) 接管 |
| §13 风险与缓解 | §0.2 关键风险表 (7 项 R1-R7 风险 + 缓解) 接管 |
| §14 已知推迟项 (v2.1 backlog) | §0.2 Phase 3 backlog (Change F) 接管 |

> **指针段作用**: 既消除内容重复,又明确各 OpenSpec change 是这些段落的**唯一实施真相源**。后续会话与新提案**禁止**复制 §3-§14 内容到此文档,应直接引用对应 change 的 4 件套。


## 16. 文档维护

- **版本**: v0.5 (2026-10-09 v0.4 §3+ 简化 — 指针化 §3-§14 段)
- **最后更新**: 2026-10-09
- **前置 lock 段**: §0.1 业务定位 + §0.1.1 修复路线图方向 + §0.2 总体实施计划 + §1.4 仿真模式声明 v0.4
- **下游权威源**: 各 Phase 0-3 OpenSpec change 4 件套 (`cpptlm-{vram-controller,lwc-bus-hub,gmmu-pte-bus,pcie-tlp-entry,msix-generator,error-cdc}-*`)
- **维护规则**: 修改前必读 §0.1 + §1.4 v0.4;新提案需显式引用本文档 §0 关联 changes 段

---

**Owner**: CppTLM Team · CppTLM
