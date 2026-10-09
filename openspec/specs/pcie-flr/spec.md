# pcie-flr Specification

## Purpose
TBD - created by archiving change cpptlm-pcie-host-test-flr-rebar. Update Purpose after archive.
## Requirements
### Requirement: flr-tlp-handler (v1.1 复用既有)

`PcieEndpointIP` SHALL **复用既有** `flr_pf()/flr_vf()` API (per `pcie_endpoint_ip.hh:201-202`) + `pool_.flr_pf()` 全状态复位 (per `pcie_sriov_vf_pool_tlm.hh:110`), **不新建 PcieFlrHandler 类** (per 双审查: 既有实现完整).

#### Scenario: FLR TLP received → PCIe state reset (per AC G3)

- **GIVEN** PcieEndpointIP 处于 D0 active state, BAR 寄存器 / config space 寄存器有非默认值
- **WHEN** host 触发 `HostPcieTestStub::trigger_flr()` → 调既有 `DGpuBoard::trigger_flr()` → `PcieEndpointIP::flr_pf()`
- **THEN** PcieEndpointIP 内部 state 全清 (per `pool_.flr_pf()`):
  - 17 端口 config space 寄存器重置（per SRIOV VF pool）
  - per-VF MSI-X + FC + seq# 独立 reset
  - CompletionTracker 关联 reset
- **AND** 4 BAR routing (per Change B facade `match_bar` 4 枚举) 仍可正常工作 (FLR 不破 routing)
- **AND** `DGpuBoard::trigger_flr()` 不重建 `DGpuBoard::eq_` (per Step 0 P0 fix, commit `4d35faee`)

#### Scenario: FLR 与 destroy→init 周期隔离 (per Risk #1)

- **WHEN** FLR 触发 (经 `trigger_flr()`)
- **THEN** **不重建** `DGpuBoard::eq_` (per Step 0 P0 fix 锁的语义)
- **AND** **不重建** `DGpuBoard::soc_` (不重新 `load_soc_config`)
- **AND** **不触动** Change C 保留的 4 internal routing_enabled_ flag (per ADR-DGPU-10 anti-hijack root cause 4)
- **AND** DGpuBoard 仿真继续运行, 仅 PCIe state 清零

---

### Requirement: flr-abi-entry (v1.1 删除)

`cpptlm_emulator_trigger_flr()` SHALL **不实现** (v1.1 删除). HostPcieTestStub 通过既有 `DGpuBoard::trigger_flr()` C++ 入口触发 FLR.

#### Scenario: HostPcieTestStub trigger_flr() 入口 (per AC G3 + G6, v1.1 改写)

- **GIVEN** `DGpuBoard* board` (per `cpptlm_emulator_create("configs/dgpu_*.json")`)
- **WHEN** `HostPcieTestStub(board, config).trigger_flr()` 调
- **THEN** 转发到 `board->trigger_flr()` → `PcieEndpointIP::flr_pf()` → `pool_.flr_pf()` state reset
- **AND** **0 新增 ABI 函数** (per CRITICAL C2: ADR-DGPU-08 §2.3 0 新增约束)
- **AND** `find include/abi/cpptlm_emulator_flr.h` = 0 匹配 (v1.1 删除确认)

---

### Requirement: flr-state-consistency (v1.1 修 spec 矛盾)

FLR 完成后, **MSI-X PBA 全清** (per PCIe Base Spec §6.6.2: FLR 复位所有 Function state, 含 MSI-X PBA bits), 4 BAR 寄存器状态 + config space Device Control Register FLR bit 状态 SHALL 保持一致.

**v1.1 修复 vs v1.0 spec 矛盾**:
- v1.0 `pcie-flr/spec.md` "23 ABI 入口状态清零": "不影响 in-flight MSI-X pending vector"（错）
- v1.0 `pcie-host-test-flr-rebar/spec.md` "FLR 端到端": "MSI-X PBA 表全部清零"（正确）
- v1.1 **统一为**: FLR 后 MSI-X PBA 全清 (per PCIe Base Spec §6.6.2)

#### Scenario: MSI-X PBA 全清 (per AC G3, v1.1 修复)

- **WHEN** FLR 完成后 (经 `trigger_flr()` → `flr_pf()`)
- **THEN** `MsiXTable::pba_bits_[N]` 全清 (per PCIe Base Spec §6.6.2)
- **AND** `DGpuBoard::msix_pending_count(0..15)` 全部归零
- **NOTE**: per PCIe Base Spec §6.6.2 FLR: "Function 回到初始状态（与系统复位相同）", 含 MSI-X PBA bits; MSI-X vector table (BAR 指向的 memory) 内容**保留** (FLR 不清 BAR 空间)

#### Scenario: 4 BAR 寄存器重置 (per AC G3)

- **WHEN** FLR 完成后, 读 BAR0/BAR1/BAR2/BAR3 寄存器
- **THEN** 寄存器值重置为 config space 报告的默认值
- **AND** `pcie_ep()->bar_size(N)` 返回值重置

#### Scenario: config space Device Control Register FLR bit 验证 (per AC G3, v1.1 修复 offset)

- **WHEN** FLR 进行中 (经既有 `pool_.flr_pf()` InProgress state)
- **THEN** config space **Device Control Register offset 0x08 bit 15** (Initiate FLR per PCIe Base Spec §7.5.1.2) 读取返回 1
- **AND** FLR 完成后 (Complete state) bit 15 读取返回 0 (host driver 写入 0 表示完成确认)
- **NOTE**: **v1.1 修 v1.0 offset 0x04 错误** (per PCIe Base Spec §7.5.1.2: Device Control Register 在 offset 0x08, offset 0x04 是 Command Register)

---

