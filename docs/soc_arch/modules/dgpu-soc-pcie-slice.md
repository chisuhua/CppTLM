# dgpu-soc-pcie-slice 微架构文档

> **类别**: GPU > dGPU SOC PCIe Slice · **状态**: 🔵 Implemented (per ADR-SOC-07) + ⚠️ **Phase 4-8 演进**:PcieEndpointTLM (4 端口) → **PcieEndpointIP (17 端口)** per [`ADR-SOC-11`](../adr/ADR-SOC-11-pcie-endpoint-ip.md) + **Phase 9+ 演进** per [`ADR-SOC-17`](../adr/ADR-SOC-17-pcie-mock-ip.md) / [`ADR-SOC-18`](../adr/ADR-SOC-18-cpptlm-abi-slimming.md) / [`ADR-SOC-19`](../adr/ADR-SOC-19-axi-master-outbound-bridge.md)
> **Header**: ~~`include/tlm/gpu/pcie_endpoint_tlm.h`~~ (`[[deprecated("use PcieEndpointIP")]]` per commit `429327d`) + **`include/tlm/pcie/pcie_endpoint_ip.hh`** (active) + `include/tlm/gpu/sdma_engine_tlm.hh`
> **注册**: `REGISTER_CHSTREAM` (`include/chstream_register.hh`, 保留 `PcieEndpointTLM` 注册以保证既有测试零回归;新代码统一用 `PcieEndpointIP`)
> **蓝图来源**: AMD/NVIDIA PCIe Endpoint IP + AMD SDMA/copy engine IP (per gem5 `src/dev/amdgpu/amdgpu_device.py` + `src/dev/pci/pci_host.py`)
> **关联 ADR**:
> - [`ADR-SOC-07-dgpu-board-soc-layering.md`](../adr/ADR-SOC-07-dgpu-board-soc-layering.md) D2/D3 — **本仓 PCI slice 拆分决策**(原 4 端口 PcieEndpointTLM)
> - [`ADR-SOC-11-pcie-endpoint-ip.md`](../adr/ADR-SOC-11-pcie-endpoint-ip.md) — **PcieEndpointIP 17 端口替代决策**(Phase 4-8 演进)
> - [`ADR-SOC-12-host-bypass-and-rc.md`](../adr/ADR-SOC-12-host-bypass-and-rc.md) — Host Bypass 软件 bring-up + 自研 RC
> - [`ADR-SOC-13-axi-stream-adapter-mapper.md`](../adr/ADR-SOC-13-axi-stream-adapter-mapper.md) — AXI Stream Adapter + AXI4Mapper
> - [`ADR-SOC-17-pcie-mock-ip.md`](../adr/ADR-SOC-17-pcie-mock-ip.md) — **PcieMockIP**(Phase 9+ 独立组件)
> - [`ADR-SOC-18-cpptlm-abi-slimming.md`](../adr/ADR-SOC-18-cpptlm-abi-slimming.md) — **ABI 22→18 精简**(Phase 9+ follow-up)
> - [`ADR-SOC-19-axi-master-outbound-bridge.md`](../adr/ADR-SOC-19-axi-master-outbound-bridge.md) — **AXI Master/Slave 角色边界明确化**(Phase 9+ 评估缺口)
> - [`ADR-SOC-06-cpptlm-v05-mvp.md`](../adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5 — dGPU MVP 切片总纲
> - UsrLinuxEmu [`ADR-088`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-dgpu-complete-simulation.md) §C2/§D3.8 — **23 ABI + `cpptlm_dma_translate_cb` 外部契约源**
> - UsrLinuxEmu [`ADR-089`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-089-v55-system-hw-simulation.md) v0.5 — **系统级硬件仿真扩展 (VFIO/IOMMUFD/vDPA/Migration)**
> - UsrLinuxEmu [`ADR-090 v2`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md) — PTX-EMU 归属 CppTLM submodule
> **关联 OpenSpec**:
> - [`openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/`](../../../openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/) — PcieEndpointTLM 原始 4 端口实施
> - [`openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/`](../../../openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/) — SdmaEngineTLM 实施
> - [`openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/`](../../../openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/) — Phase 4 SR-IOV VF Pool(引入 17 端口 PcieEndpointIP)
> - [`openspec/changes/2027-02-09-cpptlm-dgpu-pcie-ip-integration/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-pcie-ip-integration/) — Phase 8 整合交付(HEAD `429327d`)
> - [`openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/`](../../../openspec/changes/archive/2026-09-16-2026-09-16-cpptlm-pcie-tlp-wire-datapath/) — Phase 9+ 完整 TLP 链路(13 commits, 8 ADDED Requirements)
> - [`openspec/changes/2026-09-16-cpptlm-abi-slimming/`](../../../openspec/changes/archive/2026-09-16-cpptlm-abi-slimming/) — ABI 精简 22→18(follow-up)
> **首版 commit**: `4380c20` T-sd-1 + `7fc9cce` T-sd-2 (2026-08-27) · **最近更新**: 2027-09-17 (Phase 9+ + ADR-SOC-17/18/19 同步)
> **维护者**: CppTLM Team (Sisyphus)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - Board 顶层: [`dgpu-board.md`](./dgpu-board.md) (s2 单体, deprecated per ADR-SOC-07)
> - L1 Host Interface 子系统架构: [`docs/soc_arch/architecture/01-host-interface.md`](../architecture/01-host-interface.md)
> - 下游 Module 文档:
>   - `command-processor.md` · `pm4-decoder.md` · `tmu-dispatch-processor.md`
>   - `submit-queue.md` · `cuda-core-adapter.md` · `ptx-emu-submodule-mvp.md`
> - 跨仓 OpenSpec (UsrLinuxEmu 消费侧): `cpptlm-dgpu-board-soc-split` + `cpptlm-dgpu-abi-export`

---

## 1. 设计目标

`dgpu-soc-pcie-slice` 是 **dGPU SOC 片内 PCIe + DMA 引擎组合**,由两个组件组成:

| 组件 | Header | 端口数 | 方向 | 职责 |
|------|--------|------|------|------|
| **PcieEndpointIP** ⭐ 新 | `include/tlm/pcie/pcie_endpoint_ip.hh` | **17 ports** (`req_in[17]` + `resp_out[17]`,NUM_PORTS=17) | host↔device (PCIe slave+master) | 1 PF + 16 VF + 内部 `stream_id` 路由;per-VF Config Space / MSI-X / FC / seq# 独立 |
| **PcieEndpointTLM** ⚠️ 旧 | `pcie_endpoint_tlm.h` | 4 | host→device (PCIe slave) | BAR0 MMIO 解码 + 门铃副作用 / BAR1 VRAM 转发 / MSI-X 中断投递(已 `[[deprecated]]` 标注 per `429327d`)|
| **SdmaEngineTLM** | `sdma_engine_tlm.hh` | 5 | device→host (PCIe master) | 接收 DMA 描述符,发起 upstream DMA 经 IOMMU 翻译访问 host 内存 |

**⭐ PcieEndpointIP 17 端口替代决策**(per [`ADR-SOC-11`](../adr/ADR-SOC-11-pcie-endpoint-ip.md),Phase 4-8 演进):

- **NUM_PORTS = 17** = 1 PF + 16 VF(per `include/tlm/pcie/pcie_endpoint_ip.hh:48`)
- **`req_in[NUM_PORTS]` + `resp_out[NUM_PORTS]` 数组**(per L50-53),非功能命名端口(避免 N×16 端口爆炸)
- **内部 `stream_id` 路由**:用 `stream_id`(PCIe Requester ID 的 function 部分)区分 VF,避免端口按 VF 数量级展开(per `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` §3 端口图)
- **Q12 Completion 单一真源**:`PcieEndpointIP.completions()` 委托 `PcieSriovVfPool.completions()`(per L82-83),避免双份 outstanding 失配
- **`PcieEndpointTLM` 已 deprecated**:头文件 `include/tlm/gpu/pcie_endpoint_tlm.h` 添加 `[[deprecated("use PcieEndpointIP")]]`(per commit `429327d`),**layout 完全不变**(仅加属性,23 ABI 冻结不变量)

**核心特性**:

- **拓扑保真**:PCIe slave + master 分置不同组件,对应真实 dGPU SOC die 内的两个独立 IP(per ADR-SOC-07 D3)
- **JSON 拓扑驱动**:两个组件均 `REGISTER_CHSTREAM` + `ModuleFactory::registerObject<>`,可通过 `DGpuSoc` JSON 嵌套注册
- **跨仓契约冻结**:两组件共同实现 ADR-088 §D5 的 **23 ABI 外部契约**(由 Board shell 包装后,通过 `cpptlm_emulator_*` C 符号暴露)
- **bulk data 走 backdoor**:PCIe TLP `data` 字段 descriptor-only,bulk data 通过 `set_vram_backdoor()` / `set_host_backdoor()` API(测试)/ `cpptlm_backdoor_read/write`(生产)直接搬运(per ADR-SOC-07 Status Update Q3 裁决)

## 2. PCIe 拓扑角色

真实 dGPU 板卡的 PCIe 角色分工:

```text
真实硬件 (SOC die)
├── PcieEndpoint IP       (PCIe slave / Completer)    ← PcieEndpointTLM 模拟
│   ├── 接收 host→endpoint TLP (CFG/MMIO/MEM)
│   ├── BAR0 解码 + 门铃副作用 → SQ tail 推进
│   ├── BAR1 MEM 转发 → VRAM aperture
│   └── MSI-X 中断投递到 host
│
└── SDMA Engine IP       (PCIe master / Requester)   ← SdmaEngineTLM 模拟
    ├── 接收 DMA 描述符 (H2D/D2H)
    ├── 发起 upstream MEM_READ/MEM_WRITE 经 IOMMU 翻译
    └── 完成通知 done_out → CompletionRing
```

**关键边界**:

| 边界 | 责任方 | 验证机制 |
|------|--------|---------|
| **host→device TLP** (MMIO/CFG) | PcieEndpointTLM | BAR0/BAR1/Config Space 解码 |
| **device→host TLP** (upstream DMA) | SdmaEngineTLM + IOMMU | `cpptlm_dma_translate_cb` (per ADR-088 §D3.8) |
| **MSI-X 投递** | PcieEndpointTLM.irq_out | `MsiXTable` 内部状态机 |
| **Doorbell 副作用** | PcieEndpointTLM → SQ tail | `Doorbell` 内部状态机 (per s2 250-700ns 区间) |
| **DMA 完成通知** | SdmaEngineTLM.done_out → CompletionRing | `CompletionBundle` (per ADR-SOC-07 §D3.1 + `cpptlm-dgpu-sdma-engine` change) |

## 3. Wire-Format 设计

### 3.1 Bundle 类型 (按能力域分文件,per AGENTS.md)

| Bundle | 文件 | 所有者 change | 用途 |
|--------|------|--------------|------|
| `PcieTlpBundle` | `include/bundles/pcie_bundles_tlm.hh` | `cpptlm-dgpu-pcie-endpoint` | PCIe TLP / MSI-X 投递 (复用,本仓不重定义) |
| `DmaDescriptorBundle` | `include/bundles/dma_bundles_tlm.hh` | **`cpptlm-dgpu-sdma-engine` (本 change)** | DMA 描述符 |
| `CompletionBundle` | `include/bundles/dma_bundles_tlm.hh` | **`cpptlm-dgpu-sdma-engine` (本 change, 独占所有者)** | 完成/错误通知 |

**所有权声明**:`CompletionBundle` 由 `cpptlm-dgpu-sdma-engine` 独占所有者(per design.md §2 ownership),`board-soc-split` T-bs-2 复用本类型,不得在其 `dgpu_bundles_tlm.hh` 中重复定义。

### 3.2 跨仓 wire-format 冻结(per Oracle 审查 2026-08-28 scope 修正)

**重要边界澄清**:
- `bundle_serialization.hh:23-27` 自带注释"**仅在单一仿真进程内使用**",UsrLinuxEmu 经 23 ABI C 符号消费,从不 memcpy 这些 TLM Bundle
- **真正的跨仓 ABI 由 `include/cudart/abi_guards.h` G-D4 17 条静态断言覆盖**(per AGENTS.md CROSS-PROJECT),与本仓 `PcieTlpBundle` / `DmaDescriptorBundle` 等**不**共享字段布局
- 本仓 wire-format 快照测试(`test_pcie_slice_wire_format_snapshot.cc`)的**真正作用是仓内布局守卫**:防止 `cpptlm-dgpu-board-soc-split` change 集成时,本仓 PcieEndpointTLM/SdmaEngineTLM 复用的 bundle 布局被意外修改而未发现

详见 §7 前置测试 (E) wire-format 快照。

## 4. 接口契约

### 4.1 PcieEndpointTLM 接口(per pcie-endpoint change spec)

| 端口 | 方向 | Bundle | 行为契约 |
|------|------|--------|---------|
| `slave_in` (0) | ingress | `PcieTlpBundle` | 接收 host→endpoint TLP (CFG/MMIO/MEM) |
| `mmio_out` (1) | egress | `PcieTlpBundle` | BAR0 解码后 MMIO_WRITE 响应 + 门铃副作用 |
| `mem_out` (2) | egress | `PcieTlpBundle` | BAR1 MEM 转发 (descriptor-only,size>8 时 data=0) |
| `irq_out` (3) | egress | `PcieTlpBundle` (`IRQ_DELIVERY` kind) | MSI-X 中断投递 |

### 4.2 SdmaEngineTLM 接口(per sdma-engine change spec)

| 端口 | 方向 | Bundle | 行为契约 |
|------|------|--------|---------|
| `desc_in` (0) | ingress | `PcieTlpBundle` (`KIND_DMA_DESC=7`) | DMA 描述符 (H2D / D2H) |
| `mem_in` (1) | ingress | `PcieTlpBundle` | VRAM 读响应 (D2H 路径) |
| `mem_out` (2) | egress | `PcieTlpBundle` | VRAM 读/写 (H2D: MEM_WRITE / D2H: MEM_READ) |
| `host_out` (3) | egress | `PcieTlpBundle` | upstream DMA 事务 → 经 IOMMU 翻译 → host 内存 |
| `done_out` (4) | egress | `PcieTlpBundle` (`KIND_DMA_DONE=8`) | 完成/错误通知 |

### 4.3 跨仓契约(per ADR-088 §D3.8)

```cpp
// CppTLM 侧(本仓)头文件
typedef int (*cpptlm_dma_translate_cb)(uint64_t iova, uint32_t size, uint64_t* phys);
typedef void (*cpptlm_error_cb_t)(int err_code, const char* msg);

// 注入点(per SdmaEngineTLM API)
sdma.set_translate_cb(cb);  // IOMMU 翻译回调 (DGpuBoard 注入)
sdma.set_error_cb(cb);       // 错误上报回调 (DGpuBoard 注入)
```

**契约规则**(per ADR-088 §D3.8 + ADR-SOC-07 §D3.1):

1. **`cpptlm_dma_translate_cb` 同步签名**(v1.0);v1.x 可能扩展为异步(PASID/SVM)
2. 返回 0 = 成功;`<0` = 失败(per errno 语义,如 `-EFAULT=-14`)
3. **PA 越界由 CppTLM 拒绝**(待前置测试 (A) 覆盖)
4. **回调异常由 CppTLM 捕获**(待前置测试 (A) 覆盖)
5. **跨线程调用由 DGpuBoard 调度**(per board-soc-split `dgpu-board-execution-model`)

## 6. 测试覆盖现状

| 类别 | 测试文件 | 覆盖度 |
|------|----------|--------|
| **PcieEndpointTLM** | `test_pcie_endpoint_*` (5 文件) | 37 用例 / 175 assertions ✅ |
| **SdmaEngineTLM** | `test_sdma_engine_*` (4 文件) | 18 用例 / 126 assertions ✅ |
| **DMA translate cb 协议** | `test_sdma_engine_iommu_fault.cc` (4 用例) | 强 ✅ |
| **R3-S1 VRAM write visibility** | `test_sdma_engine_h2d.cc` 主用例 | 强 ✅ |
| **wire-format 快照(防 cross-repo drift)** | ❌ 无 | ⚠️ **Tier 2 (E) 待补** |
| **DMA translate 边界(phys 越界/异常)** | ❌ 无 | ⚠️ **Tier 2 (A) 待补** |
| **MSI-X 状态机细粒度(mask/unmask/PBA)** | ❌ 粗粒度 | ⚠️ **Tier 2 (C) 待补** |
| **Doorbell 排队/并发/取消** | ❌ 粗粒度 | ⚠️ **Tier 2 (B) 待补** |
| **backdoor ABI 隔离(MMIO vs backdoor)** | ❌ 无 | ⚠️ **Tier 2 (D) 待补** |

## 7. 前置测试建议(per 2026-08-28 评估)

为帮助 UsrLinuxEmu 端到端测试 (per ADR-089 v5.5+) 更顺畅,本仓可以(也建议)补充以下前置测试。详见 [`ADR-SOC-08-v55-system-hw-integration-preconditions.md`](../adr/ADR-SOC-08-v55-system-hw-integration-preconditions.md) §D3。

| 优先级 | 测试 | 工作量 | ROI |
|---|---|---:|---|
| **P0** | (E) wire-format 快照 | 半天 | **防止跨仓不兼容** |
| **P0** | (A) DMA translate 边界 | 半天 | IOMMUFD 集成时真实失败模式 |
| **P1** | (C) MSI-X 状态机 | 1 天 | VFIO SET_IRQS 行为契约 |
| **P1** | (B) Doorbell 排队 | 1 天 | GPU driver 启动标准路径 |
| **P2** | (D) backdoor 隔离 | 半天 | 23 ABI backdoor 路径正确性 |

OpenSpec change: [`openspec/changes/2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/`](../../../openspec/changes/2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/)

## 8. 跨仓职责边界(per ADR-036 3 区分 + ADR-088 §C2)

| 边界 | 本仓 (CppTLM) | UsrLinuxEmu 仓 | 备注 |
|------|----------------|----------------|------|
| **dGPU 板卡仿真** | ✅ 23 ABI + PcieEndpointTLM + SdmaEngineTLM | — | 本仓独占 |
| **host driver 仿真** | — | ✅ KFD / amdgpu / nouveau | UsrLinuxEmu 独占 |
| **VFIO 字符设备** | — | ✅ v5.5.1 (per ADR-089) | UsrLinuxEmu 独占 |
| **IOMMUFD uapi** | — | ✅ v5.5.2 (per ADR-089) | UsrLinuxEmu 独占 |
| **vDPA / Live Migration** | — | ✅ v5.5.3/v5.5.4 (per ADR-089) | UsrLinuxEmu 独占 |
| **CPUs/Memory/NoC** | ✅ TLM 仿真 | — | 本仓独占 |
| **PTX-EMU Image Executor** | ✅ submodule (per ADR-090 v2) | — | 本仓独占 |

**关键不变量**:UsrLinuxEmu 仿真 VFIO 时,经 23 ABI 调用本仓 PcieEndpointTLM / SdmaEngineTLM 的契约,**必须**与本文档 §4 + §6 描述一致。

---

## 5. 跨仓契约与 UsrLinuxEmu ADR 治理边界(per Oracle 审查 2026-08-28)

### 5.1 跨仓 wire-format 边界澄清

本仓的 `PcieTlpBundle` / `DmaDescriptorBundle` / `CompletionBundle` **不**与 UsrLinuxEmu 仓的 `cpptlm_module.h` 直接共享字段布局:
- `bundle_serialization.hh:23-27` 自带注释"**仅在单一仿真进程内使用**"
- UsrLinuxEmu 经 23 ABI C 符号消费,从不 memcpy 这些 TLM Bundle
- 真正的跨仓 ABI 由 `include/cudart/abi_guards.h` G-D4 17 条静态断言覆盖(per AGENTS.md CROSS-PROJECT)
- 本仓 wire-format 快照测试(`test_pcie_slice_wire_format_snapshot.cc`)的**真正作用是仓内布局守卫**:防止 `cpptlm-dgpu-board-soc-split` change 集成时本仓 bundle 布局被意外修改

### 5.2 跨仓 ADR 引用规范(per Oracle 审查 Minor 修复)

跨仓 ADR 引用必须**显式标注仓名**,避免本仓 ADR-X/SOC 与 UsrLinuxEmu ADR-035/036 编号冲突:

| 编号体系 | 仓 | 示例 |
|----------|-----|------|
| **ADR-SOC-0X** | CppTLM(本仓)| ADR-SOC-06 / ADR-SOC-07 / ADR-SOC-08 |
| **ADR-0XX (无 SOC 前缀)**)** | UsrLinuxEmu | UsrLinuxEmu ADR-035 / ADR-036 / ADR-088 / ADR-089 / ADR-090 |

**约定**:本文档与姊妹 ADR 引用 UsrLinuxEmu ADR 时,必须加 "UsrLinuxEmu ADR-XXX" 前缀。已在本文档所有跨仓引用中采用。

---

## 9. Phase 9+ 更新: PCIe Slice 边界扩展 (2026-09-17)

> **关联 change**: `openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/`
> **架构文档**: `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md §12` Phase 9+ 完整 TLP 链路 + profile 选路

### 9.1 PCIe IP 类型 (Phase 9+ 引入)

| 类型 | 路径 | 依赖 |
|------|------|------|
| **PcieEndpointIP** | `tlm/pcie/pcie_endpoint_ip` | LL + PHY + Bypass Mux + 17 端口 SR-IOV (完整 PCIe 端点, Phase 4 既有) |
| **PcieMockIP** (新) | `tlm/pcie/pcie_mock_ip` | **独立**, 仅 BAR + MSI-X + D3hot + AXI payload (无 TLP/FC/LL/PHY) |
| **PcieCompleterEngine** (新) | `tlm/pcie/pcie_completer_engine` | 被 PcieEndpointIP 持有, 替换 dispatch_tlp no-op 占位 |
| **PcieRequesterEngine** (新) | `tlm/pcie/pcie_requester_engine` | 持有 PcieLinkLayer*, 用于 SDMA H2D + MSI-X MWr 投递 |
| **HostBypassTLM** | `tlm/pcie/host_bypass_tlm` | Phase 7 既有, 用于 axi_bypass profile 路径 |

### 9.2 profile 选路 (Phase 9+ 新增)

`profile.pcie_path` 字段决定 mmio_write/read 路径:

| 值 | 触发条件 | 路径分流 | 行为对齐 |
|----|---------|---------|---------|
| `"tlp"` | 非默认 | 完整 TLP 链路 (Encoder -> LL -> CompleterEngine -> bar_store_) | 最完整路径; 含 CSR 重定向 + 读泵环阻塞 |
| `"axi_bypass"` | 显式设置 | HostBypassTLM::bar_write (跳过 TLP, 直接 AXI) | 无 TLP 延迟, 无 DWORD 编解码 |
| `"mock"` | 显式设置 | PcieMockIP (独立组件, 无协议栈) | 极简响应; backdoor 行为通过 mock 路径实现 |
| `"legacy"` | 默认 | mmio_regs_ 既有行为 | 兼容遗留测试 |

### 9.3 ABI 表面 (Phase 9+ 状态, 修订版)

`include/abi/cpptlm_emulator.h` 经过**两轮精简**:

- **第一轮 (ADR-SOC-18, 2027-09-17 完成)**: 22 → 18 函数, 删除 4 个 backdoor 辅助函数 (`cpptlm_emulator_backdoor_read/write` + `cpptlm_emulator_register_backdoor_cb` + `cpptlm_emulator_lookup_register`)
- **第二轮 (ADR-SOC-20, 2027-09-17 Proposed, 修订版, 待 Hub ack 启动)**: 18 → **15 函数 + 1 宏**, 删除真冗余 2 函数 (`create_by_id` + `get_adapter_info`) + `get_version` 改宏; **保留 `open/close`** (per 用户反馈 2027-09-17, fd 风格 + 生命周期分层语义价值)

**当前状态**: 18 函数表 (等待 P5 启动后变 15 函数 + 1 宏)

**修订前后对比** (per ADR-SOC-20 §1.2 修订理由):
| 维度 | 初版 (18→14+宏) | 修订 (18→15+宏) |
|------|:---------------:|:---------------:|
| 删除函数数 | 5 (含 open/close) | **3** (真冗余 + 宏) |
| `open/close` 决策 | 删除 | **保留** |
| Hub ack 风险 | 🟡 中 | 🟢 低 |
| Driver 功能影响 | 🟡 场景 3+6 | 🟢 **零** |

详见 HSK-10/11/12 (`docs/cross_repo/HSK-1[012]-*.md`) + ADR-SOC-18 / ADR-SOC-20 + phase9-p5-secondary-slimming.md

### 9.4 数据流 (Phase 9+)

```
mmio_write (C ABI)
  |
  +-- pcie_path="tlp"       -> PcieTlpEncoder -> PcieLinkLayer::rx_tlp_from_host -> CompleterEngine
  |                              -> bar_store_  -> CplD (MWr: no Cpl | MRd: CplD via tx_tlp -> host)
  +-- pcie_path="axi_bypass" -> HostBypassTLM::bar_write -> PcieAxiAdapter -> bar_store_
  +-- pcie_path="mock"       -> PcieMockIP::mmio_write   -> bar_regs_
  +-- pcie_path="legacy"     -> mmio_regs_ (既有)

mmio_read (C ABI, tlp path)
  -> RequesterEngine MRd -> tx_tlp -> host -> rx_tlp(CplD) -> pending_data_ -> buf
     (读泵环: eq_->run() <= 1000 虚拟周期, 超时降级 mmio_regs_)
```

### 9.5 AXI 接口语义矩阵 (Phase 9+ 明确化, per ADR-SOC-19)

> **关键决策**:`PcieEndpointIP` 的 `axi_slave_in` / `cfg_slave_in` **不承担 "AXI-to-PCIe bridge" 角色**。详见 [`ADR-SOC-19`](../adr/ADR-SOC-19-axi-master-outbound-bridge.md)。

#### 9.5.1 EP AXI 三端口角色定义

`PcieAxiAdapter` 持有 `Axi4StreamAdapter` 三端口(per `include/framework/axi4_stream_adapter.hh:25-28` 官方文档):

| 端口 | 角色 (AXI 标准) | 触发方 | 接收方 | 数据落点 | 用途 |
|------|----------------|--------|--------|---------|------|
| `axi_master_out` | **EP 是 Master** | EP | SoC(被读/写) | SoC interconnect → SoC memory | 接收 PCIe Rx 数据推 SoC(如 host MWr → bar_store_ 落地后副作用) |
| `axi_slave_in` | **EP 是 Slave** | SoC(Master) | EP | EP 本地 `bar_store_` | Host 经 HostBypass 写 EP BAR(Phase 8 M1 桥接) |
| `cfg_slave_in` | **EP 是 Slave** | SoC(Master) | EP | EP 本地 config space | Host 写 PMCSR / 其他 CFG 寄存器 |

#### 9.5.2 dGPU 双路径 (用户视角)

| 路径 | PCIe 事务 | 触发方 | EP 行为 | SoC 接口使用 |
|------|----------|--------|---------|-------------|
| **dGPU receive** (Host → Device) | PCIe MWr / MRd→CplD | Host driver | EP 是 PCIe Completer | 写本地 bar_store_,**EP 主动 `axi_master_out` 推 SoC** |
| **dGPU send** (Device → Host) | PCIe MWr / MRd→CplD | SDMA / MSI-X | EP 是 PCIe Requester | **不走 `axi_slave_in`** — 直接 C++ API 调 `PcieRequesterEngine::mrd_read()` |

#### 9.5.3 SoC → Host memory 三种实现方式 (Phase 9+ 现状)

| 方式 | 状态 | 适用场景 | 路径 |
|------|:----:|---------|------|
| **C++ API 直调** | ✅ | SDMA / MSI-X 投递链等专用组件 | `SoC 组件` → `PcieRequesterEngine::mrd_read()` → `LL::tx_tlp()` |
| **标准 AXI 桥接** | ❌ | 通用 SoC AXI Master(CUDA core / GPC core / Display) | **当前未提供** — 详见 [`ADR-SOC-19`](../adr/ADR-SOC-19-axi-master-outbound-bridge.md) + [`phase9-p4-axi-outbound-bridge.md`](../roadmap/phase9-p4-axi-outbound-bridge.md) |
| **PcieMockIP device-side** | ✅ | Mock profile (`pcie_path="mock"`) | `PcieMockIP::device_axi_write()` → 内部 `Axi4StreamAdapter` |

**SDMA 实际路径证据**(per `src/tlm/gpu/sdma_engine_tlm.cc:420-424`):
```cpp
} else if (d.dir == DmaDescriptor::Dir::H2D && request_engine_) {
    // T-P10-2 (实为 T-P11-2): H2D 经 RequesterEngine 发起 MRd (EP→host TLP)
    process_h2d_with_requester(d);  // ← 直接 C++ API, 不走 AXI
    requester_path = true;
}
```

**`RequesterEngine` 实际路径证据**(per `src/tlm/pcie/pcie_requester_engine.cc:32-60`):
```cpp
bool PcieRequesterEngine::mrd_read(...) {
    // ...
    if (!link_layer_->tx_tlp(tlp, bdf)) {  // ← 直接调 PCIe LL, 绕开 AXI
        return false;
    }
    // ...
}
```

#### 9.5.4 EP `axi_slave_in` 实际行为 (per `src/tlm/pcie/pcie_endpoint_ip.cc:333-433`)

**EP 消费 `axi_slave_in` 的方式**:
1. 接收 AW (写) / AR (读) 请求
2. **仅写入本地 `bar_store_` / config space**(不转发为 PCIe Outbound TLP)
3. 返回 AXI 响应(BRESP=0 OK / =3 DECERR 在 D3hot 下)

**关键限制**:`axi_slave_in` 不充当 "AXI-to-PCIe bridge"。即使 SoC 侧 master 把 PCIe 地址空间的目标地址写到 `axi_slave_in`,**EP 不会自动发起 PCIe MRd/MWr**。

#### 9.5.5 缺口与未来扩展 (per ADR-SOC-19)

如果将来需要支持**通用 SoC AXI Master → host memory R/W**:
- ❌ **当前无法支持**——缺少桥接组件
- 📋 **待 Phase 10+ 评估**——见 [`phase9-p4-axi-outbound-bridge.md`](../roadmap/phase9-p4-axi-outbound-bridge.md) 三方案对比:
  - 方案 A:新增 `axi_to_pcie_bridge` 通用组件(5-10 人天)
  - 方案 B:`PcieRequesterEngine` 重构为标准 AXI4 Master 接口(3-5 人天)
  - 方案 C:**维持现状 + 文档化**(**当前决策**, ROI 最高)
- 🔍 **触发条件**:CUDA zero-copy / Display controller 拉 host buffer / GPU user-mode driver 等需求出现时再升级

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 🔵 Implemented + Tier 2 前置测试待补 (per OpenSpec change 2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites) + Phase 9+ TLP 链路完成 (2026-09-17 archive) + ABI 18 精简完成 (2026-09-17 follow-up) + AXI Master/Slave 边界明确化 (2027-09-17 ADR-SOC-19)
**最后更新**: 2027-09-17