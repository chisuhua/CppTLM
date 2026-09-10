# Tasks: cpptlm-stage-1-3-sdma

> **工期**: 2.5-3 周 | TDD 5 步纪律

## §1 阶段 1.3a SDMA Ring Buffer（1 周）
- [ ] **写失败测试**: `test_sdma_ring_rptr_wptr` + `test_sg_descriptor_chain`
- [ ] **实施**: `sdma_ring_buffer.h/cc` (cfg.ring_size 4 档 + 1024 max + 32-bit RPTR/WPTR) + `sdma_packet.h/cc` (SG≥8)
- [ ] **改造**: `sdma_engine_tlm.cc` descriptor 直投 → Ring Buffer + Doorbell (`BAR1+0x10010000`)
- [ ] **扩展**: `dma_descriptor_mvp.hh` + `dma_bundles_tlm.hh` Dir::D2D + SG
- [ ] **Verify**: 测试全 PASS（Ring RPTR/WPTR + SG 链 + Doorbell 绑定）
- [ ] **Commit**: `feat(sdma): ring buffer + RPTR/WPTR + doorbell + SG (stage 1.3a)`

## §2 阶段 1.3b D2D NoC（0.5-1 周）
- [ ] **写失败测试**: `test_d2d_noc_path` + `test_host_out_zero_transactions`
- [ ] **实施**: `d2d_noc_path.h/cc` payload 转发
- [ ] **改造**: `gpu_mesh_noc.h/cc` 延迟模型 → payload (≥100 GB/s)
- [ ] **显存 bypass**: host_out 零事务断言
- [ ] **Verify**: D2D + host_out 零事务
- [ ] **Commit**: `feat(sdma): D2D NoC payload + memctl bypass (stage 1.3b)`

## §3 阶段 1.3c dma_translate + IOMMU + CP→SDMA（0.5 周，修复 #2）
- [ ] **写失败测试**: `test_register_dma_translate_cb_returns_iova` + `test_dma_translate_iommu`
- [ ] **修改**: `cpptlm_emulator.cc:443-460` 移除 `(void)cb`，identity 返 0 / IOMMU 负 errno
- [ ] **实施**: `pcie_endpoint_ip.cc` GART/IOMMU 4 级翻译链
- [ ] **改造**: `command_processor_mvp.cc` DISPATCH dma_req (PM4 0x4600-0x4900)
- [ ] **Verify**: cb 真实调用，pa==iova; IOMMU 4 级翻译正确
- [ ] **Commit**: `fix(cpptlm): 修复 #2 dma_translate_cb 真实化 (stage 1.3c)`

## §4 阶段 1.3d SDMA 完成通知（0.5 周）
- [ ] **写失败测试**: `test_sdma_fence` + `test_msix_completion`
- [ ] **改造**: `sdma_engine_tlm.cc` Fence 命令支持
- [ ] **新建**: `sdma_completion_ring.h/cc` done_out → CompletionRing
- [ ] **修改**: `cpptlm_emulator.cc` MSI-X vector 路由
- [ ] **Verify**: Fence + MSI-X 接线测试全 PASS（200ms 内 intr_cb ≥1）
- [ ] **Commit**: `feat(sdma): fence + MSI-X 接线 (stage 1.3d)`

## §5 双仓 entry sync
- [ ] **UE entry**: `docs/02_architecture/pcie-endpoint-entry.md §12` v0.6/v0.7/v0.8/v0.9 阶段 1.3a-d 各次 ship
- [ ] **CppTLM 18-doc mirror**: `docs/soc_arch/architecture/18-pcie-endpoint-entry.md §12` 同步

## §6 Oracle 复审（4 次轻量）

> **Oracle 复审位置（Metis M6 修订 2026-09-10）**：实施 commit 后、docs mirror commit **前**进行。复审发现问题需追加 commit 而非 amend docs。

- [ ] 每子阶段 1 次轻量复审，确认 Ring Buffer wire-format / D2D / 翻译 / 完成通知

## §7 总计

- **CppTLM commits**: 8（4 修复 + 4 entry sync mirror）
- **工期**: 2.5-3 周
- **Oracle**: 4 次轻量
- **下游**: UE `ue-stage-1-3-sdma-integration` 集成 + 5.5.8 阶段 3 gate（§3 ship 后）
