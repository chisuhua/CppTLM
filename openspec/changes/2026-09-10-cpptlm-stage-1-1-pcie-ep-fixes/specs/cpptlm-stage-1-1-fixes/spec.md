# Spec: cpptlm-stage-1-1-fixes

> **Capability**: cpptlm-stage-1-1-fixes
> **Owner**: CppTLM Architecture Team
> **状态**: 🔄 Proposed（2026-09-10）
> **Created**: 2026-09-10
> **关联**: [proposal.md](../proposal.md) + [design.md](../design.md) + [tasks.md](../tasks.md)

## Purpose

补全 `2026-09-09-cpptlm-pcie-ep-foundation` change 阶段 1.1 暴露的 4 个根本性错误（Oracle 三轮审查：PCIe EP 评审 3.5/10 + CP attach 评审 3.5/10 + 5.5.8 立项评审 8.7/10）。本 spec 是父 change 阶段 1.1 的聚焦子集，独立推进可加速 UsrLinuxEmu 5.5.7+ dGPU E2E 主线解锁。

## ADDED Requirements

### Requirement: PCIe Configuration Space Forwarding (修复 #3)

The system MUST forward `DGpuBoard::pcie_config_read` / `_write` to `PcieEndpointTLM::cfg_space_->read` / `write` instead of returning -ENOSYS stub. When `soc_->getInternalInstance("pcie_ep")` returns null or `cfg_space_` is null, MUST return -ENOSYS (not segfault).

#### Scenario: Vendor ID readable via config_read

- **WHEN** `pcie_config_read(emu, 0x00, 4, &val)` is called with valid soc+cfg_space
- **THEN** the function returns 0
- **AND** `val` contains the configured Vendor ID (0x10DE for NVIDIA or 0x1002 for AMD)

#### Scenario: Null soc instance returns -ENOSYS

- **WHEN** `pcie_config_read` is called before soc initialization
- **THEN** the function returns -ENOSYS
- **AND** does not segfault

#### Scenario: Null val pointer returns -EINVAL

- **WHEN** `pcie_config_read(emu, 0x00, 4, nullptr)` is called
- **THEN** the function returns -EINVAL

### Requirement: Backdoor Read Miss Returns -ENOENT (修复 #6)

The system MUST return -ENOENT (-38) from `DGpuBoard::backdoor_read` when the requested vram_offset is not found in vram_segments_, instead of returning the length as a "fake success" value.

#### Scenario: Miss returns -ENOENT not len

- **WHEN** `backdoor_read(0xDEADBEEF, buf, 64)` is called with unregistered vram_offset
- **THEN** the function returns -ENOENT (-38)
- **AND** `buf` is NOT modified (no false data fill)

#### Scenario: Hit returns 0 with buf filled

- **WHEN** `backdoor_read` is called with registered vram_offset and matching size
- **THEN** the function returns 0
- **AND** `buf` contains the segment data

#### Scenario: Size mismatch returns -EINVAL

- **WHEN** `backdoor_read` is called with registered vram_offset but size mismatch
- **THEN** the function returns -EINVAL

#### Scenario: Null buf returns -EINVAL

- **WHEN** `backdoor_read(0x0, nullptr, 64)` is called
- **THEN** the function returns -EINVAL

### Requirement: MMIO Read Data Copy (修复 #5)

The system MUST copy the response data from sim_loop drain into the caller's buffer in `DGpuBoard::mmio_read`, instead of leaving buf uninitialized. Return value MUST be the byte count on success or negative errno on failure (not just the status code).

#### Scenario: MMIO read returns real data

- **WHEN** `mmio_read(0, 0, buf, 4)` is called and sim_loop responds with data
- **THEN** the function returns a non-negative byte count (typically 4)
- **AND** `buf` contains the actual response data (not garbage)
- **AND** NOT all zeros (the TODO T-bs-3c stub behavior)

#### Scenario: MMIO read timeout returns -ETIMEDOUT

- **WHEN** `mmio_read` is called but sim_loop doesn't respond within WAIT_TIMEOUT_MS
- **THEN** the function returns -110 (ETIMEDOUT)
- **AND** `buf` is NOT modified

#### Scenario: MMIO read sim_loop error returns negative errno

- **WHEN** sim_loop response contains error status
- **THEN** the function returns the negative errno (e.g., -EIO)
- **AND** `buf` is NOT modified

#### Scenario: Null buf returns -EINVAL

- **WHEN** `mmio_read(0, 0, nullptr, 4)` is called
- **THEN** the function returns -EINVAL

### Requirement: MMIO Write Async Semantics Clarification (修复 #7)

The system documentation MUST clarify that `mmio_write` is asynchronous and returns 0 immediately, with data drained by sim_loop on the next tick. The current code behavior (async return 0) is correct.

#### Scenario: design.md §3.1 reflects async semantics

- **WHEN** `openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/design.md` §3.1 is read
- **THEN** it explicitly states "mmio_write returns 0 immediately (async); data is drained by sim_loop on next tick"
- **AND** it does NOT contain "blocks synchronously" or equivalent blocking language

#### Scenario: architecture §2.1 and design.md §3.1 are consistent

- **WHEN** both docs are read together
- **THEN** they describe the same async semantics for mmio_write
- **AND** there is no contradiction

## MODIFIED Requirements

(N/A — this change does not modify existing specs)

## REMOVED Requirements

(N/A — this change does not remove existing specs)

## Cross-References

- Parent change `2026-09-09-cpptlm-pcie-ep-foundation` — overall PCIe EP foundation
- Sibling change `2026-09-10-ue-stage-1-1-bridge-sync` (UsrLinuxEmu) — UE-side bridge sync
- ADR-088 dGPU 仿真边界 — 23 ABI 冻结
- ADR-091 4 象限布局 — PcieBypassController 3 态
