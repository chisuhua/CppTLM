# cpptlm-stage-1-4-2-1 Specification

## Purpose
TBD - created by archiving change 2026-09-10-cpptlm-stage-1-4-2-1. Update Purpose after archive.
## Requirements
### Requirement: PCIe PM Capability (Stage 1.4)

The system MUST implement PCIe PM Capability (id=0x01) with INV-A gate narrowed to BAR path (cfg path preserves D3hot access per PCI PM spec), PMCSR PWS[1:0] mask for reserved values (D1/D2 ignored), and PM Cap control word JSON-driven. 修订范围: INV-A gate 收窄到 BAR 路径; PMCSR PWS=1/2 mask; PM Cap control word JSON-driven。系统 MUST 支持以下 5 个 Scenario:

#### Scenario: D3hot cfg write via PMCSR wakes to D0 (INV-E)
- **WHEN** device in D3hot and driver writes `PMCSR[1:0] = 00b` (D0) via cfg path
- **THEN** device transitions to D0
- **AND** subsequent MMIO writes succeed (no DECERR)

#### Scenario: D3hot BAR write returns DECERR (not silent)
- **WHEN** device in D3hot and driver writes to BAR space (awaddr >= config_size)
- **THEN** AXI bresp = DECERR (AXI encoding 3; 勘误: 2=SLVERR, 3=DECERR) returned to host
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
- **WHEN** JSON `params.pm_cap_control = 5251` (0x1483; 顶层键, 勘误: 非 phy_digital 子键)
- **THEN** `pool_.config_pool().config_of(0).read(0x40) & 0xFFFF0000` == 0x14830000 (control 在 dword 0x40 高半; 勘误: read(0x42) 非 4 对齐返回 0xFFFFFFFF)
- (default value 0x0013 if key absent)

### Requirement: P2P DMA Routing + ACS (Stage 2.1)

The system MUST support P2P DMA with optional `AcsPolicy` admin API for explicit ACS grant (per BDF pair), preserving INV-D no-silent-fallback guarantee. 修改范围: AcsPolicy 引入 + SUCCESS 路径支持。系统 MUST 支持以下 2 个 Scenario:

#### Scenario: P2P DMA granted via AcsPolicy
- **WHEN** `AcsPolicy` instance grants `(src_bdf=0x0001, dst_bdf=0x0002)`
- **AND** `p2p_dma_route(0x0001, 0x0002, addr, len, &policy)` called
- **THEN** returns `P2PResult{SUCCESS}`

#### Scenario: P2P DMA not granted → BLOCKED_BY_ACS (default strict)
- **WHEN** `AcsPolicy` instance does NOT grant `(src, dst)`
- **AND** `p2p_dma_route(src, dst, addr, len, &policy)` called
- **THEN** returns `P2PResult{BLOCKED_BY_ACS}` (INV-D preserved)

### Requirement: Resizable BAR Capability (Stage 2.1)

The system MUST integrate `ResizableBar` state machine into `PcieEndpointIP` as 6 independent BAR slots, with INV-G boundary validation: when `enable()` shrinks BAR size, any out-of-bounds `bar_store_` entries MUST be cleared with a warning recorded. 修改范围: 集成到 PcieEndpointIP + INV-G 越界校验。系统 MUST 支持以下 2 个 Scenario:

#### Scenario: PcieEndpointIP bar resize clears out-of-bounds keys (INV-G)
- **WHEN** `resizable_bar(0).reprogram_size(0x100000)` + `enable_resizable_bar(0)` (enable 成功后触发 on_bar_resize)
- **AND** existing `bar_store_[0x80000]` value present (within old size)
- **AND** `resizable_bar(0).disable()` → `resizable_bar(0).reprogram_size(0x100)` + `enable_resizable_bar(0)` (resize down; 勘误: 必须先 disable 才能再 reprogram, INV-C 序列)
- **THEN** `bar_store_[0x80000]` cleared (out of new bounds)
- **AND** `ep.config_warnings()` contains "out of bounds" message
- (NOTE: bar_store_ key 为全局裸地址, 跨 BAR 隔离为 MVP 已知限制, 见 Out of Scope)

#### Scenario: PcieEndpointIP 6 independent resizable BAR slots
- **WHEN** `resizable_bar(0).reprogram_size(0x1000); resizable_bar(2).reprogram_size(0x10000);` (slot 0 = 4KB, slot 2 = 64KB)
- **THEN** `resizable_bar(1).size_bytes() == 0` (untouched slot)
- **AND** slot 0 and 2 states independent

---

### Requirement: ACS Extended Cap (Stage 2.1) — **NEW**

The system MUST implement PCIe ACS Extended Capability (id=0x000D, version=1) per spec §7.7 with `install_acs_extended_cap()` helper.

#### Scenario: install_acs_extended_cap writes standard header
- **WHEN** `install_acs_extended_cap(cfg, 0x100)` called
- **THEN** `cfg.read(0x100) == 0x0001000D` (id=0x000D, version=1, next=0; PCIe Ext Cap dword 布局 bits[15:0]=ID, [19:16]=version, [31:20]=next)
- **AND** `cfg.read(0x104) == 0x00000000` (ACS Cap+Control Reg dword: no caps by default)

#### Scenario: ACS V bit enable via dword RMW
- **WHEN** `set_acs_bit_enabled(cfg, 16, true)` (ACS Control V bit = dword 高 16-bit bit0, 勘误: Control Reg 在 offset+4 dword 高半, 16-bit offset+6 不可直接读写)
- **THEN** `cfg.read(0x104) & 0x00010000 == 0x00010000`

---

