# Spec: cpptlm-stage-1-4-2-1-followups — MODIFIED Requirements

> **状态**: 🔄 Proposed v1.0 (2027-02-09)
> **Delta type**: MODIFIED Requirements（扩展 `cpptlm-stage-1-4-2-1` 已 archive spec）
> **基础**: `openspec/specs/cpptlm-stage-1-4-2-1/spec.md`

---

## MODIFIED Requirements

### Requirement: PCIe PM Capability (Stage 1.4) — **MODIFIED** `cpptlm-stage-1-4-2-1#PCIe PM Capability`

The system MUST implement PCIe PM Capability (id=0x01) with INV-A gate narrowed to BAR path (cfg path preserves D3hot access per PCI PM spec), PMCSR PWS[1:0] mask for reserved values (D1/D2 ignored), and PM Cap control word JSON-driven. 修订范围: INV-A gate 收窄到 BAR 路径; PMCSR PWS=1/2 mask; PM Cap control word JSON-driven。系统 MUST 支持以下 5 个 Scenario:

#### Scenario: D3hot cfg write via PMCSR wakes to D0 (INV-E)
- **WHEN** device in D3hot and driver writes `PMCSR[1:0] = 00b` (D0) via cfg path
- **THEN** device transitions to D0
- **AND** subsequent MMIO writes succeed (no DECERR)

#### Scenario: D3hot BAR write returns DECERR (not silent)
- **WHEN** device in D3hot and driver writes to BAR space (awaddr >= config_size)
- **THEN** AXI bresp = DECERR (AXI encoding 2) returned to host
- **AND** bar_store_ unchanged

#### Scenario: D3hot cfg read always succeeds
- **WHEN** device in D3hot and driver reads Vendor ID (offset 0x00)
- **THEN** returns 0x10DE (Nvidia vendor ID)
- (PCI PM spec: config space accessible in D3hot)

#### Scenario: PMCSR PWS=1/2 reserved values ignored
- **WHEN** driver writes `PMCSR[1:0] = 01b or 10b` (D1/D2 reserved)
- **THEN** `power_state_` unchanged (PCI PM spec: unsupported state writes ignored)
- **AND** no callback side effect beyond PWS change detected by sentinel

#### Scenario: PM Cap control word JSON-driven
- **WHEN** JSON `params.pm_cap_control = 5251` (0x1483 = version 3 + D1 + D2 + D3hot)
- **THEN** `pool_.config_pool().config_of(0).read(0x42) & 0xFFFF0000` == 0x14830000
- (default value 0x0013 if key absent)

### Requirement: P2P DMA Routing + ACS (Stage 2.1) — **MODIFIED** `cpptlm-stage-1-4-2-1#P2P DMA Routing + ACS`

The system MUST support P2P DMA with optional `AcsPolicy` admin API for explicit ACS grant (per BDF pair), preserving INV-D no-silent-fallback guarantee. 修改范围: AcsPolicy 引入 + SUCCESS 路径支持。系统 MUST 支持以下 2 个 Scenario:

#### Scenario: P2P DMA granted via AcsPolicy
- **WHEN** `AcsPolicy` instance grants `(src_bdf=0x0001, dst_bdf=0x0002)`
- **AND** `p2p_dma_route(0x0001, 0x0002, addr, len, &policy)` called
- **THEN** returns `P2PResult{SUCCESS}`

#### Scenario: P2P DMA not granted → BLOCKED_BY_ACS (default strict)
- **WHEN** `AcsPolicy` instance does NOT grant `(src, dst)`
- **AND** `p2p_dma_route(src, dst, addr, len, &policy)` called
- **THEN** returns `P2PResult{BLOCKED_BY_ACS}` (INV-D preserved)

### Requirement: Resizable BAR Capability (Stage 2.1) — **MODIFIED** `cpptlm-stage-1-4-2-1#Resizable BAR Capability`

The system MUST integrate `ResizableBar` state machine into `PcieEndpointIP` as 6 independent BAR slots, with INV-G boundary validation: when `enable()` shrinks BAR size, any out-of-bounds `bar_store_` entries MUST be cleared with a warning recorded. 修改范围: 集成到 PcieEndpointIP + INV-G 越界校验。系统 MUST 支持以下 2 个 Scenario:

#### Scenario: PcieEndpointIP bar resize clears out-of-bounds keys (INV-G)
- **WHEN** `resizable_bar(0).reprogram_size(0x100000); resizable_bar(0).enable();`
- **AND** existing `bar_store_[0x80000]` value present (within old size)
- **AND** `resizable_bar(0).reprogram_size(0x100); resizable_bar(0).enable();` (resize down)
- **THEN** `bar_store_[0x80000]` cleared (out of new bounds)
- **AND** `ep.config_warnings()` contains "out of bounds" message

#### Scenario: PcieEndpointIP 6 independent resizable BAR slots
- **WHEN** `resizable_bar(0).reprogram_size(0x1000); resizable_bar(2).reprogram_size(0x10000);` (slot 0 = 4KB, slot 2 = 64KB)
- **THEN** `resizable_bar(1).size_bytes() == 0` (untouched slot)
- **AND** slot 0 and 2 states independent

---

## ADDED Requirements

### Requirement: ACS Extended Cap (Stage 2.1) — **NEW**

The system MUST implement PCIe ACS Extended Capability (id=0x000D, version=1) per spec §7.7 with `install_acs_extended_cap()` helper.

#### Scenario: install_acs_extended_cap writes standard header
- **WHEN** `install_acs_extended_cap(cfg, 0xE0)` called
- **THEN** `cfg.read(0xE0) == 0x000D0001` (id=0x000D, version=1, next=0x0001)
- **AND** `cfg.read(0xE4) == 0x00000000` (ACS Cap Reg: no caps by default)
- **AND** `cfg.read(0xE6) == 0x00000000` (ACS Control: all disable)

#### Scenario: ACS V bit enable
- **WHEN** `set_acs_bit_enabled(cfg, 0, true)` (V bit = source validation)
- **THEN** `cfg.read(0xE6) & 0x0001 == 0x0001`

---

## Cross-Reference

- **基础 spec**: `openspec/specs/cpptlm-stage-1-4-2-1/spec.md` (3 ADDED requirements, archived 2026-09-14)
- **proposal**: `openspec/changes/2027-02-09-cpptlm-stage-1-4-2-1-followups/proposal.md`
- **design**: `openspec/changes/2027-02-09-cpptlm-stage-1-4-2-1-followups/design.md`
- **tasks**: `openspec/changes/2027-02-09-cpptlm-stage-1-4-2-1-followups/tasks.md`
- **上游**: 2026-09-10 archive (predecessor)
- **下游**: UE `ue-stage-1-4-2-1-extensions`
