# dgpu-soc-pcie-slice 微架构文档

> **类别**: GPU > dGPU SOC PCIe Slice · **状态**: 🔵 Implemented (per ADR-SOC-07)
> **Header**: `include/tlm/gpu/pcie_endpoint_tlm.h` + `include/tlm/gpu/sdma_engine_tlm.hh`
> **注册**: `REGISTER_CHSTREAM` (`include/chstream_register.hh`, 已注册 `PcieEndpointTLM` / `SdmaEngineTLM`)
> **蓝图来源**: AMD/NVIDIA PCIe Endpoint IP + AMD SDMA/copy engine IP (per gem5 `src/dev/amdgpu/amdgpu_device.py` + `src/dev/pci/pci_host.py`)
> **关联 ADR**:
> - [`ADR-SOC-07-dgpu-board-soc-layering.md`](../adr/ADR-SOC-07-dgpu-board-soc-layering.md) D2/D3 — **本仓 PCI slice 拆分决策**
> - [`ADR-SOC-06-cpptlm-v05-mvp.md`](../adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5 — dGPU MVP 切片总纲
> - UsrLinuxEmu [`ADR-088`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-dgpu-complete-simulation.md) §C2/§D3.8 — **23 ABI + `cpptlm_dma_translate_cb` 外部契约源**
> - UsrLinuxEmu [`ADR-089`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-089-v55-system-hw-simulation.md) v0.5 — **系统级硬件仿真扩展 (VFIO/IOMMUFD/vDPA/Migration)**
> - UsrLinuxEmu [`ADR-090 v2`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md) — PTX-EMU 归属 CppTLM submodule
> **关联 OpenSpec**:
> - [`openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/`](../../../openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/) — PcieEndpointTLM 实施
> - [`openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/`](../../../openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/) — SdmaEngineTLM 实施
> **首版 commit**: `4380c20` T-sd-1 + `7fc9cce` T-sd-2 (2026-08-27) · **最近更新**: 2026-08-28
> **维护者**: CppTLM Team (Sisyphus)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - Board 顶层: [`dgpu-board.md`](./dgpu-board.md) (s2 单体, deprecated per ADR-SOC-07)
> - 下游 Module 文档:
>   - `command-processor.md` · `pm4-decoder.md` · `tmu-dispatch-processor.md`
>   - `submit-queue.md` · `cuda-core-adapter.md` · `ptx-emu-submodule-mvp.md`
> - 跨仓 OpenSpec (UsrLinuxEmu 消费侧): `cpptlm-dgpu-board-soc-split` + `cpptlm-dgpu-abi-export`

---

## 1. 设计目标

`dgpu-soc-pcie-slice` 是 **dGPU SOC 片内 PCIe + DMA 引擎组合**,由两个 ChStreamModuleBase 组件组成:

| 组件 | Header | 端口数 | 方向 | 职责 |
|------|--------|------|------|------|
| **PcieEndpointTLM** | `pcie_endpoint_tlm.h` | 4 | host→device (PCIe slave) | BAR0 MMIO 解码 + 门铃副作用 / BAR1 VRAM 转发 / MSI-X 中断投递 |
| **SdmaEngineTLM** | `sdma_engine_tlm.hh` | 5 | device→host (PCIe master) | 接收 DMA 描述符,发起 upstream DMA 经 IOMMU 翻译访问 host 内存 |

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

### 3.2 跨仓 wire-format 冻结(per ADR-090 v2 §C0 Canonical 仲裁)

本仓交付的 `PcieTlpBundle` / `DmaDescriptorBundle` / `CompletionBundle` 是 UsrLinuxEmu 仓 `cpptlm_bridge.h` 与 `cpptlm_module.h` 解析的目标。**任何字段重排或新增会破坏 UsrLinuxEmu 仓的解析**(per ADR-090 v2 §C0)。详见 §7 前置测试 (E) wire-format 快照。

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

**关键不变量**:UsrLinuxEmu 仿真 VFIO 时,经 23 ABI 调用本仓 PcieEndpointTLM / SdmaEngineTLM 的契约,**必须**与本文档 §4 + §5 描述一致。

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 🔵 Implemented + Tier 2 前置测试待补 (per OpenSpec change 2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites)
**最后更新**: 2026-08-28