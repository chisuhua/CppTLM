# Spec: cpptlm-stage-1-3-sdma

## ADDED Requirements

### Requirement: SDMA Ring Buffer Implementation (1.3a)

The system MUST implement `SdmaRingBuffer` with `cfg.ring_size ∈ {4KB, 8KB, 16KB, 64KB}`, max 1024 entries @64B, 32-bit RPTR/WPTR. Doorbell at `BAR1 + 0x10010000` triggers only on WPTR write.

#### Scenario: Ring Buffer 4 config sizes
- **WHEN** `SdmaRingBuffer(cfg_size=4KB|8KB|16KB|64KB, entry_size=32|64)` constructed
- **THEN** capacity matches cfg_size
- **AND** entry count = cfg_size / entry_size (max 1024 @64B)

#### Scenario: RPTR/WPTR atomic 32-bit
- **WHEN** wptr++ and rptr++ concurrently
- **THEN** no data race (std::atomic<uint32_t>)
- **AND** wptr ≥ rptr always

#### Scenario: Doorbell write triggers
- **WHEN** WPTR written to `BAR1 + 0x10010000`
- **THEN** ring buffer processes new entries

#### Scenario: SG descriptor chain ≥ 8
- **WHEN** packet has 8 SG descriptors
- **THEN** chain valid, all descriptors processed

### Requirement: D2D NoC Payload Forwarding (1.3b)

The system MUST implement D2D NoC payload forwarding with bandwidth ≥ 100 GB/s, host_out zero transactions assertion.

#### Scenario: NoC payload forward ≥ 100 GB/s
- **WHEN** d2d_noc_forward(src_va, dst_va, len=1MB) called
- **THEN** payload transferred
- **AND** simulated bandwidth ≥ 100 GB/s

#### Scenario: host_out zero transactions
- **WHEN** D2D path used (no PCIe TLP)
- **THEN** host_out port has 0 transactions

### Requirement: dma_translate_cb Real Implementation (1.3c, 修复 #2)

The system MUST implement `cpptlm_emulator_register_dma_translate_cb` without `(void)cb` stub. Identity mode returns 0 with pa=iova; IOMMU mode propagates negative errno (-ENOSYS/-EIO) to error_cb.

#### Scenario: identity mode pa=iova
- **WHEN** `register_dma_translate_cb(cb)` + `dma_translate(iova=0x1000, mode=identity)`
- **THEN** returns 0
- **AND** pa = iova (0x1000)

#### Scenario: IOMMU mode cb failure → negative errno
- **WHEN** IOMMU mode cb returns -EIO
- **THEN** error_cb invoked with -EIO

#### Scenario: PM4 opcode 0x4600-0x4900 dispatch
- **WHEN** `command_processor` receives PM4 opcode 0x4600/0x4700/0x4800/0x4900
- **THEN** SDMA dispatch triggered

### Requirement: SDMA Completion Notification (1.3d)

The system MUST implement Fence command triggering completion events; done_out → CompletionRing → MSI-X → intr_cb within 200ms timeout window.

#### Scenario: Fence triggers completion
- **WHEN** Fence descriptor (opcode=0x04) submitted to SDMA Ring
- **THEN** completion event triggered after drain

#### Scenario: CompletionRing → MSI-X → intr_cb 200ms
- **WHEN** done_out fires
- **THEN** CompletionRing forwards to MSI-X trigger_msix(vector)
- **AND** intr_cb invoked within 200ms

## Cross-References

- sdma-engine-design.md §2-§11
- ADR-023 HAL append-only
- 上游 `stage-1-2-msix`
- 下游 `ue-stage-1-3-sdma-integration`
