## MODIFIED Requirements

### Requirement: sdma-doorbell-in-handling (新增, Gap J 核心)

`SdmaEngineTLM` SHALL 加 `doorbell_in` SlavePort (ChStreamInputStreamAdapter<DoorbellBundle>), 在 `tick()` 处理 DoorbellBundle transaction, 把 wptr 写入 ring consumer (替换 BAR1 0x10010000 直调).doorbell 识别**基于偏移量**(测试专用合成偏移),**不依赖** BAR1 实窗大小(16MB 实窗下同样命中)。

#### Scenario: SDMA 收 DoorbellBundle 写 ring 0 (per Gap J 修复)

- **GIVEN** `sdma.doorbell_in().valid() == true` 且 DoorbellBundle `wdata=42, ring_id=0, posted=1, ready=1`
- **WHEN** `sdma.tick()` 调用
- **THEN** `sdma.ring_consumer_wptr_ == 42`
- **AND** ring consumer 触发 1 次 wptr 提交
- **NOTE**: 替换 `dgpu_board_shell.cc:537` `sdma_engine_->mmio_write(1, kBar1DoorbellOffset, 42)` 直调

#### Scenario: 旧 `mmio_write` 兼容 (per H-B3)

- **GIVEN** 编译时调用 `sdma.mmio_write(1, kBar1DoorbellOffset, wptr)`
- **THEN** 编译器 emit `[[deprecated]]` warning
- **AND** 行为 fallback 到直接 ring 提交
