# Spec: cpptlm-stage-1-4-2-1

## ADDED Requirements

### Requirement: PCIe PM Capability (1.4)

The system MUST implement `PciePowerState` transitions (D0/D3hot/D3cold) and ASPM (L0s/L1).

#### Scenario: D0 to D3 transition
- **WHEN** `set_power_state(D3hot)` called on D0 device
- **THEN** PMCSR register reflects D3
- **AND** MMIO disabled
- **AND** can wake to D0

#### Scenario: ASPM L1 entry
- **WHEN** `enable_aspm(L1)` + idle 100µs
- **THEN** link enters L1 state

### Requirement: P2P DMA Routing (2.1)

The system MUST support Peer-to-Peer DMA with ACS (Access Control Services).

#### Scenario: P2P DMA between two root ports
- **WHEN** `p2p_dma_route(src_bdf=0001:00.0, dst_bdf=0002:00.0, addr, len)`
- **THEN** DMA routed without host memory hop

#### Scenario: ACS denies peer request
- **WHEN** ACS blocks P2P between src and dst
- **THEN** returns -EPERM

### Requirement: Resizable BAR Capability (2.1)

The system MUST implement Resizable BAR allowing dynamic BAR size adjustment.

#### Scenario: BAR size adjustment
- **WHEN** `resize_bar(bar=0, new_size=256MB)` called
- **THEN** BAR0 size updated to 256MB
- **AND** MMIO mapping updated

## Cross-References

- pcie-config-space spec
- ADR-069 BAR + ioremap
- 上游 `stage-1-3-sdma`
