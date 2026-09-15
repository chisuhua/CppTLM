# Spec: cpptlm-stage-1-4-2-1-ue-extensions-unblock

> **Capability**: cpptlm-stage-1-4-2-1-ue-extensions-unblock
> **Owner**: CppTLM Architecture Team
> **状态**: 🔄 Proposed（2026-09-15，依据 Oracle + Metis 双审查 ue-stage-1-4-2-1-extensions）
> **Created**: 2026-09-15
> **关联**: [proposal.md](../proposal.md) + [design.md](../design.md) + [tasks.md](../tasks.md)

## Purpose

Path A 上游接线：把 CppTLM ABI 路径从冻结的 `PcieEndpointTLM`（frozen legacy，仅 MSI-X Cap）切换到 `PcieEndpointIP`（已 ship 完整 PM Cap + PMCSR 拦截 + power state machine + ACS Ext Cap + ReBAR 集成），并补全 24 ABI 不可观测的两条路径：mmio backdoor power-state check + LNKCTL→enable_aspm 接线。解锁 UE 仓 `ue-stage-1-4-2-1-extensions` 5/5 spec Scenario。

## ADDED Requirements

### Requirement: ABI Profile 切换 PcieEndpointIP（A-1）

The system MUST instantiate `pcie_ep` as `PcieEndpointIP` in `configs/dgpu_board_v1.json` so that 24 ABI can reach PM Cap + PCIe Cap + ACS Ext Cap + ReBAR Ext Cap.

#### Scenario: pcie_config_read offset 0x48 = PM Cap (id=0x01)
- **WHEN** `cpptlm_emulator_pcie_config_read(emu, 0x48, 2, &val)` (PM Cap id register @ offset 0x48)
- **THEN** val = 0x01 (lower byte = PCI Cap id 0x01 for Power Management)

#### Scenario: pcie_config_read offset 0x100 = ACS Ext Cap (id=0x000D)
- **WHEN** `cpptlm_emulator_pcie_config_read(emu, 0x100, 4, &val)` (ACS Ext Cap header)
- **THEN** val & 0xFFFF = 0x000D (ACS Extended Cap ID)

#### Scenario: pcie_config_read offset 0x140 = ReBAR Ext Cap (id=0x0020)
- **WHEN** `cpptlm_emulator_pcie_config_read(emu, 0x140, 4, &val)` (ReBAR Ext Cap header)
- **THEN** val & 0xFFFF = 0x0020 (ReBAR Extended Cap ID)

### Requirement: Board Shell Pass-Through to PcieEndpointIP（A-2）

The system MUST NOT use `dynamic_cast<PcieEndpointTLM*>` in `dgpu_board_shell.cc`. All config/mmio operations MUST pass through to the instantiated `PcieEndpointIP` via `pcie-endpoint-ip-simmodule-refactor` accessors.

#### Scenario: All 4 dynamic_cast call sites replaced
- **WHEN** grep `dynamic_cast<PcieEndpointTLM*>` in `src/tlm/gpu/dgpu_board_shell.cc`
- **THEN** 0 matches (all replaced with `getInternalInstanceAs<PcieEndpointIP>` or `dynamic_cast<PcieEndpointIP*>`)

### Requirement: ABI MMIO Power-State Gate（A-3）

The system MUST check `board->is_mmio_gated()` in the ABI mmio read path. When D3hot is active, `cpptlm_emulator_mmio_read` MUST return `-EIO` (per INV-A MMIO gating).

#### Scenario: MMIO read in D3hot returns -EIO
- **WHEN** D3hot active (board->is_mmio_gated() == true) + `cpptlm_emulator_mmio_read(emu, bar=0, offset=0, &buf, 4)`
- **THEN** return value = -EIO (not 0; not data)

#### Scenario: MMIO read in D0 returns 0
- **WHEN** D0 active (board->is_mmio_gated() == false) + `cpptlm_emulator_mmio_read(emu, bar=0, offset=0, &buf, 4)`
- **THEN** return value = 0 (success)

### Requirement: PCIe Cap (0x10) + LNKCTL ASPM 接线（A-4）

The system MUST install PCIe Capability (id=0x10) in `PcieEndpointIP` config space, with LNKCTL register write intercepted to call `phy_->enable_aspm(L0s/L1)`.

#### Scenario: pcie_config_read offset 0x50 = PCIe Cap (id=0x10)
- **WHEN** `cpptlm_emulator_pcie_config_read(emu, 0x50, 2, &val)`
- **THEN** val = 0x0010 (PCIe Cap id 0x10)

#### Scenario: LNKCTL write enables ASPM L1
- **WHEN** `cpptlm_emulator_pcie_config_write(emu, LNKCTL_OFFSET=0x52, 2, 0x0001)` (ASPM L1 enable)
- **THEN** `phy_->enable_aspm(AspmLevel::L1)` is called

#### Scenario: LNKCTL readback reflects ASPM L1 enable
- **WHEN** after write, `pcie_config_read(LNKCTL_OFFSET=0x52, 2, &val)`
- **THEN** val & 0x0003 = 0x0001 (ASPM L1 enabled in readback)

### Requirement: ReBAR Extended Capability 安装（A-5）

The system MUST install Resizable BAR Extended Capability (id=0x0020) in `PcieEndpointIP` config space with header + control register readable via ABI.

#### Scenario: ReBAR Ext Cap header readable
- **WHEN** `pcie_config_read(0x140, 4, &val)`
- **THEN** val & 0xFFFF = 0x0020 (ReBAR Ext Cap id)

#### Scenario: ReBAR Control register reflects default BAR0 size
- **WHEN** `pcie_config_read(0x144, 4, &val)` (ReBAR Control)
- **THEN** val contains BAR size encoding for BAR0 = 256MB (bits[12:8] >= 0xF)

## Cross-References

- 上游 `2026-09-10-cpptlm-stage-1-4-2-1`（archived，含 6 followups）+ `2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor`（archived）
- 解锁 `2026-09-10-ue-stage-1-4-2-1-extensions`（UE 仓 0/8 → 16 checkbox，commit `42b08a6` 必修修订后）
- 父 change `2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma`（5.5.8 阶段 3 ship 后排队）