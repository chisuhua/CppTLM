## MODIFIED Requirements

### Requirement: BAR 路由优先级 (per TInv-5,沿用 minimal_v1)

BAR1 路由 SHALL 沿用 minimal_v1 §4.2 优先级;timing-mode 在 doorbell 路径上累加 1 cyc parse latency。doorbell 识别**基于偏移量**(测试专用合成偏移),**不依赖** BAR1 实窗大小(16MB 实窗下同样命中)。

#### Scenario: doorbell 路径时序

- **GIVEN** host mmio_write(BAR1, off=0x10010000, wptr)
- **WHEN** DGpuBoard::mmio_write 检测 kBar1DoorbellOffset
- **THEN** pcie_tlp_latency (2 cyc) + doorbell parse (1 cyc) = 3 cyc 累加
- **AND** sdma_engine_->mmio_write(1, 0x10010000, wptr) 调用
- **AND** SDMA ring consume (1 cyc 内部)

#### Scenario: storage 路径时序

- **GIVEN** host mmio_write(BAR1, off=0x1000, data) (非 doorbell)
- **WHEN** DGpuBoard::mmio_write 检测 PcieStorage 路由
- **THEN** pcie_tlp_latency (**2 cyc**, per Phase 5 PcieAxiAdapter) + functional-mode backdoor memcpy (**0 cyc**, per v0.2 §4 简化: BAR 路径不引入 Crossbar/VramCtrl cascade)
- **AND** 数据写入 vram_storage_ (同 cycle 内 per TInv-2)
