# cpptlm-dgpu-sdma-engine: SdmaEngineTLM 组件（PCIe master / DMA 归属 SOC）

> **状态**: ✅ Implemented — 2026-08-26 · **日期**: 2026-08-26 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D1/D3
> **依赖**: [`2026-08-26-cpptlm-dgpu-pcie-endpoint`](../2026-08-26-cpptlm-dgpu-pcie-endpoint/)（共享 `pcie_bundles_tlm.hh`；可同批实施）
> **被依赖**: [`2026-08-26-cpptlm-dgpu-board-soc-split`](../2026-08-26-cpptlm-dgpu-board-soc-split/)

> **字母代号约定**（与姊妹 changes 共享，本 change 内部不另设字母）：
> - **change A** = `cpptlm-dgpu-pcie-endpoint`（共享 `pcie_bundles_tlm.hh`）
> - **change B** = 本 change（`cpptlm-dgpu-sdma-engine`，自建 `dma_bundles_tlm.hh`）
> - **change C** = `cpptlm-dgpu-board-soc-split`（CP fetch 接线由 C 完成）
> - **change D** = `cpptlm-dgpu-abi-export`（SHARED 库暴露 23 ABI，本 change 的 host_out 翻译回调由 D 实现）

---

## Why

ADR-SOC-07 D3 决策：dGPU SOC 的 PCIe **master 方向**（设备发起 upstream DMA，经系统 IOMMU 翻译 IOVA→PA）必须与 slave 方向（PcieEndpointTLM）分离为独立组件。真实硬件中 SDMA/copy engine 是 SOC 内独立 IP，驱动 VRAM↔host 搬运与 GPFIFO/pushbuffer 抓取。

当前 s2 没有任何 upstream DMA 组件——`DGpuBoardTLM::read_vram/write_vram` 是 host 方向 memcpy，s3 的 CP `mem_read_vram` 也尚未经 DMA 引擎。UsrLinuxEmu ADR-088 §D3.8 已定义 `cpptlm_dma_translate_cb`（CppTLM→UsrLinuxEmu 系统 IOMMU 回调），本 change 交付其 device 侧发起方。

**触发事件**:
- 2026-08-26 Board/SOC 分层修正决策（ADR-SOC-07）
- ADR-088 §D3.8 DMA translate callback 需要 CppTLM 侧调用点（当前 0 命中）

---

## What Changes

### 1. 新建文件

| 文件 | 用途 |
|------|------|
| `include/tlm/gpu/sdma_engine_tlm.hh` + `src/tlm/gpu/sdma_engine_tlm.cc` | **🆕 SdmaEngineTLM**（ChStreamModuleBase，REGISTER_CHSTREAM） |
| `include/tlm/gpu/dma_descriptor_mvp.hh` | DMA 描述符 C++ 类型（src/dst 地址空间标记、长度、完成 tag） |
| `include/bundles/dma_bundles_tlm.hh` | **🆕** DMA bundle：`DmaDescriptorBundle`（按域划分，**不修改** `pcie_bundles_tlm.hh`，与 change A 零共享） |
| `test/test_sdma_engine_h2d.cc` | host→device：host_out 上游读 → VRAM 写 PASS |
| `test/test_sdma_engine_d2h.cc` | device→host：VRAM 读 → host_out 上游写 PASS |
| `test/test_sdma_engine_iommu_fault.cc` | translate callback 返回非 0 → CompleterAbort + error 通道 PASS |
| `test/test_sdma_engine_from_config.cc` | JSON 实例化 + 端口连接 PASS |

### 2. 修改文件

| 文件 | 修改 |
|------|------|
| `include/chstream_register.hh` | 加 `REGISTER_CHSTREAM(SdmaEngineTLM)` + adapter 注册 |
| `include/bundles/pcie_bundles_tlm.hh` | **不动**（DMA bundle 走独立文件 `dma_bundles_tlm.hh`；按能力域分文件，与 cache/noc/compute bundles 惯例一致） |

### 3. 边界（不修改）

- **不实现**系统 IOMMU——`host_out` 事务经 Board shell 回调 UsrLinuxEmu `src/system_hw/iommu/`（per ADR-088 §D6.1，系统 IOMMU 是 UsrLinuxEmu 侧职责）
- **不修改** `CommandProcessor`/`TmuDispatchProcessor`——s3 正在填充；CP 的 GPFIFO fetch 在 change C 接线时改经本组件

---

## 端口协议（ChStream）

| 端口 | 方向 | Bundle | 用途 |
|------|------|--------|------|
| `desc_in` | ingress | `DmaDescriptorBundle`（**锁定**，来自 change B 自建 `include/bundles/dma_bundles_tlm.hh`，与 change A 零共享） | CP/上层提交 DMA 描述符 |
| `mem_in` / `mem_out` | ingress/egress | `PcieTlpBundle`（复用 change A，MEM 块 >8B 数据面走 backdoor，per ADR-SOC-07 Status Update Q3 裁决） | SOC VRAM 读写（接 MemoryCluster） |
| `host_out` | egress | `PcieTlpBundle`（descriptor-only，bulk 数据走 backdoor） | upstream DMA 事务 → Board → IOMMU translate cb → host |
| `done_out` | egress | `CompletionBundle`（包 `task_id, status`） | 完成/错误通知 → CompletionRing |

`num_ports()` = 5（多端口注入）。

---

## Acceptance Gate（本 change 独立）

| Gate | Owner | 状态 | 验证方法 |
|------|-------|:---:|----------|
| **SD-G1** SdmaEngineTLM 编译 + 注册 | CppTLM | ✅ | `cmake --build build` + JSON 创建成功 (libcpptlm_core.a 含 sdma_engine_tlm.cc.o, ModuleFactory 注册 "SdmaEngineTLM" → factory.knows()=true, port_count=5) |
| **SD-G2** H2D 搬运测试 PASS | CppTLM | ✅ | `ctest -R "test_sdma_engine_h2d"` PASS (3 用例: 全流程 + 反压满窗口 + in-order completion) |
| **SD-G3** D2H 搬运测试 PASS | CppTLM | ✅ | `ctest -R "test_sdma_engine_d2h"` PASS (2 用例: 全流程 + host write visibility + 数据面验证) |
| **SD-G4** IOMMU fault → CompleterAbort + error 通道 PASS | CppTLM | ✅ | `ctest -R "test_sdma_engine_iommu_fault"` PASS (4 用例: translate fault + size==0 + OOB + no-cb) |
| **SD-G5** JSON 实例化 + 端口连接 PASS | CppTLM | ✅ | `ctest -R "test_sdma_engine_from_config"` PASS (6 用例: JSON config + 5 端口注入 + factory 注册 + fixture 加载) |

**最终验收**: SD-G1~G5 全部 ✅ + 4 测试文件 PASS + 无回归 + 可独立 archive。

---

## Cross-Repo Coordination

| 仓 | 跟踪载体 | 状态 |
|----|---------|:---:|
| **UsrLinuxEmu** | ADR-088 §D3.8 `cpptlm_dma_translate_cb` 调用点由本组件提供 | ⏳ ADR-088 仍为 planned contract |
| **board-soc-split (CompletionBundle 复用)** | 本 change 的 `bundles::CompletionBundle` 由 board-soc-split T-bs-2 复用（不得在其 `dgpu_bundles_tlm.hh` 中重复定义；如有字段变更由本 change 主导，board-soc-split 同步跟进） | ⏳ 待两 change archive 时核对 |
| **PTX-EMU** | 无 | ✅ N/A |

---

**起草**: Sisyphus (2026-08-26, per ADR-SOC-07 D3)
**Owner**: CppTLM Team
**状态**: ✅ Implemented (2026-08-26, commits 4380c20 → 7fc9cce → c821094 → 17e3c8b → 9d2f2a2)
