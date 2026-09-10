# Proposal: cpptlm-stage-1-3-sdma — SDMA 引擎 4 子阶段

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **工期**: 2.5-3 周
> **前置依赖**: CppTLM `2026-09-10-cpptlm-stage-1-2-msix` ship
> **关联 change**: 上游 stage-1-1-pcie-ep-fixes + stage-1-2-msix；下游 `2026-09-10-ue-stage-1-3-sdma-integration`
> **关联 ADR**: ADR-091 4 象限 / ADR-088 dGPU 仿真边界 / sdma-engine-design.md §2-§11

---

## Why

阶段 1.3 SDMA 引擎完整实施（4 子阶段）。Oracle 三轮审查揭示"`sdma_engine_tlm.cc` 是 descriptor 直投而非 Ring Buffer 架构，需"补架构"而非"修 bug""。按 sdma-engine-design.md §1-§14 拆分 4 子阶段。

**Why 单独 change（方案 B）**：1.3a/1.3b/1.3c/1.3d 紧密耦合（共享 `sdma_engine_tlm.cc` 改造），作为 1 个 atomic change 实施；但 5.5.7 gate 解锁仅需 1.3a ship。

## What Changes

### 1.3a SDMA Ring Buffer（1 周）
- 新建 `sdma_ring_buffer.h/cc`：cfg.ring_size ∈ {4KB, 8KB, 16KB, 64KB}，max 1024 entries @64B，32-bit RPTR/WPTR
- 新建 `sdma_packet.h/cc`：SG 描述符链（MAX_SG_ENTRIES=8）
- `sdma_engine_tlm.cc` 改造：descriptor 直投 → Ring Buffer + Doorbell (`BAR1 + 0x10010000`)
- `dma_descriptor_mvp.hh` + `dma_bundles_tlm.hh`：Dir::D2D + SG 扩展

### 1.3b D2D NoC（0.5-1 周）
- 新建 `d2d_noc_path.h/cc`：D2D NoC payload 转发
- `gpu_mesh_noc.h/cc`：延迟模型 → payload 转发（≥100 GB/s）
- 显存控制器 bypass：host_out 零事务断言

### 1.3c dma_translate_cb + GART/IOMMU + CP→SDMA（0.5 周，修复 #2）
- `cpptlm_emulator.cc:443-460`：移除 `(void)cb`，真实调用
- identity 模式：pa=iova, ret=0；IOMMU 模式：负 errno (-ENOSYS/-EIO) → error_cb
- `pcie_endpoint_ip.cc`：GART/IOMMU 4 级翻译链
- `command_processor_mvp.cc`：DISPATCH dma_req 分支（PM4 opcode 0x4600-0x4900）

### 1.3d SDMA 完成通知（0.5 周）
- `sdma_engine_tlm.cc`：Fence 命令支持（Ring 内 Fence descriptor）
- 新建 `sdma_completion_ring.h/cc`：done_out → CompletionRing 转发
- MSI-X vector 路由（driver 指定）

## Impact

- **新建**：`sdma_ring_buffer.h/cc` + `sdma_packet.h/cc` + `d2d_noc_path.h/cc` + `sdma_completion_ring.h/cc`
- **修改**：`sdma_engine_tlm.cc` + `gpu_mesh_noc.h/cc` + `pcie_endpoint_ip.cc` + `cpptlm_emulator.cc` + `command_processor_mvp.cc` + `dma_descriptor_mvp.hh` + `dma_bundles_tlm.hh`
- **不改**：23 ABI / 5 ports wire-format
- **5.5.8 阶段 3 gate**：本 change §3（1.3c）ship 后 5.5.8 阶段 3 可启动

## Oracle 复审（验收）

- 4 次轻量复审（每子阶段 1 次）
- 验收标准：见 specs/cpptlm-stage-1-3-sdma/spec.md

## refs

- sdma-engine-design.md §2-§11（SDMA 内部设计）
- ADR-023 HAL append-only
- Oracle d99ab2e0（量化 AC 复审，5 AMEND 应用）
