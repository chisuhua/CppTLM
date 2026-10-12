# Spec: MSI-X Generator IP (cpptlm-msix-generator-ip v0.5.0)

> **配套**: [`../../proposal.md`](../../proposal.md) · [`../../design.md`](../../design.md) · [`../../tasks.md`](../../tasks.md)
> **版本**: v0.5.0 (per tasks.md Phase 2 Change E, 2026-10-12 lock)

---

## ADDED Requirements

> Phase 2 / Change E: 5 requirements (Gap L 修复 + 测试要求)

### Requirement: msix-generator-ip-core (新)

`MsixGeneratorIP` 模块 SHALL 在 `include/tlm/pcie/msix_generator_ip.{hh,cc}` 实现 PBA + Vector table, per PCIe spec §6.8.

#### Scenario: MSI-X table lookup (per AC-MSIX-1.1)

- **GIVEN** `MsixGeneratorIP` 实例已 set_table_entry(vector=3, addr_lo=0xFFFF0000, addr_hi=0, data=0xABCD, masked=false)
- **WHEN** `trigger_msix(vector=3, addr=0, data=0, masked=false)` 调用
- **THEN** 内部查表得到 addr=0xFFFF0000, data=0xABCD
- **AND** 生成 PcieTlpBundle (MWr 64-bit, addr=0xFFFF0000, data=0xABCD)
- **AND** 调 submit_tlp(tlp) → PcieEndpointIP

#### Scenario: MSI-X PBA pending bit check (per AC-MSIX-1.2)

- **GIVEN** `set_pba_bit(vector=5, pending=true)` 已设置
- **WHEN** `trigger_msix(vector=5, ...)` 调用
- **THEN** 内部检查 PBA[5] == true, 继续查表 + 生成 TLP
- **NOTE**: masked bit 检查在 PBA 检查之后 (per PCIe spec §6.8)

#### Scenario: MSI-X vector masked (per AC-MSIX-1.3)

- **GIVEN** `set_table_entry(vector=7, ..., masked=true)` 已设置
- **WHEN** `trigger_msix(vector=7, ...)` 调用
- **THEN** **不发 TLP**; PBA[7] 置 pending bit
- **AND** 解除 mask 后, pending bit 触发 TLP
- **NOTE**: per PCIe spec §6.8: masked vector 不发 TLP, 但 pending bit 仍可设置

---

### Requirement: trigger-msix-api (新, 补 CRITICAL C3)

`DGpuBoard::trigger_msix(uint16_t vector, uint64_t addr, uint32_t data, bool masked)` C++ API SHALL 存在, **不暴露为 ABI 函数** (per CRITICAL C3 全仓零匹配修复 + ADR-DGPU-08 §2.3).

#### Scenario: trigger_msix C++ API 存在 (per AC-MSIX-2.1)

- **GIVEN** `DGpuBoard` 实例
- **WHEN** 编译期检查 `DGpuBoard::trigger_msix` 符号存在
- **THEN** `trigger_msix(vector, addr, data, masked)` 可调用
- **AND** 内部调用 `msix_generator_->trigger_msix()`
- **NOTE**: 0 ABI 函数新增 (per ADR-DGPU-08 §2.3)

#### Scenario: trigger_msix via device IP (SDMA completion interrupt, per AC-MSIX-2.2)

- **GIVEN** SDMA H2D 完成
- **WHEN** SDMA 调 `dgpu_board->trigger_msix(vector=1, addr=0xFFFF0000, data=0x1234, masked=false)`
- **THEN** MSI-X Generator 查表 + 生成 MWr TLP
- **AND** TLP 经 submit_tlp() → PcieEndpointIP → PCIe link

#### Scenario: trigger_msix 不进 cpptlm_emulator.h (per AC-MSIX-2.3)

- **GIVEN** grep 检查 `include/abi/cpptlm_emulator.h` 包含 `trigger_msix`
- **WHEN** 编译
- **THEN** **0 结果** (trigger_msix 不在 ABI 头文件)
- **AND** trigger_msix 仅在 DGpuBoard C++ 内部 API

---

### Requirement: lwc-integration (新)

`MsixGeneratorIP` SHALL 通过 LWC hub (Change B 设施) 提交 interrupt 请求.

#### Scenario: LWC hub wiring (per AC-MSIX-3.1)

- **GIVEN** `DGpuBoard::bind_memory_backings()` 实施
- **WHEN** LWC hub 初始化
- **THEN** `lwc_hub_->set_target(PortIndex::MSIX, msix_generator_.get())` 已调用
- **AND** `msix_generator_` 有 LWC SlavePort (MsixBundle)

#### Scenario: MsixBundle via LWC (per AC-MSIX-3.2)

- **GIVEN** device IP (SDMA) 发送 MsixBundle via LWC hub
- **WHEN** LWC hub route_to_target(MSIX, bundle) 调用
- **THEN** MsixGeneratorIP::msix_in() 收到 bundle
- **NOTE**: per Change B T8 MsixBundle 已定义 (`include/bundles/msix_bundles_tlm.hh`)

---

### Requirement: pcie-tlp-emission (新)

`PcieEndpointIP` 接收 MSI-X 请求 SHALL 发 MSI-X TLP (MWr 64-bit with addr + data, per PCIe spec §6.8.2).

#### Scenario: MSI-X TLP MWr 64-bit encoding (per AC-MSIX-4.1)

- **GIVEN** `submit_msix_tlp(vector=2, addr=0xFEDC0000, data=0xBEEF)` 调用
- **WHEN** PcieTlpBundle 生成
- **THEN** `fmt = 0b110` (MWr 64-bit)
- **AND** `type = 0b00000` (Memory Write)
- **AND** `addr_hi = 0x0000FEDC`, `addr_lo = 0x00000000` (64-bit addr)
- **AND** `data = 0xBEEF` (32-bit data)
- **AND** `length = 1` (1 DW)
- **NOTE**: per PCIe spec §6.8.2 MSI-X uses 64-bit MWr

#### Scenario: MSI-X TLP via submit_tlp (per AC-MSIX-4.2)

- **GIVEN** `PcieEndpointIP` 实例已 `submit_tlp(tlp)` API
- **WHEN** MsixGeneratorIP 调 `submit_tlp(tlp)`
- **THEN** TLP 进入 PcieEndpointIP 处理 pipeline
- **AND** TLP 发往 PCIe link
- **NOTE**: per Change D `submit_tlp()` 准 frozen additive API

---

### Requirement: test-coverage (新)

`[msix_generator]` Catch2 测试套 SHALL ≥ 5 cases (vector table lookup + boundary + error injection).

#### Scenario: vector table lookup (per AC-MSIX-5.1)

- **GIVEN** `MsixGeneratorIP` 已 set_table_entry
- **WHEN** `trigger_msix` 调用
- **THEN** TLP 字段正确 (addr + data from table)

#### Scenario: boundary vectors (per AC-MSIX-5.2)

- **GIVEN** vector = 0 (最小) 和 vector = 63 (最大, per 64-entry table)
- **WHEN** `trigger_msix` 调用
- **THEN** 两者均正确处理

#### Scenario: masked vector 不发 TLP (per AC-MSIX-5.3)

- **GIVEN** vector 已 masked=true
- **WHEN** `trigger_msix` 调用
- **THEN** 0 TLP 生成; PBA bit set

#### Scenario: unknown target graceful handling (per AC-MSIX-5.4)

- **GIVEN** `trigger_msix(vector=99)` (超出范围)
- **WHEN** 调用
- **THEN** graceful handling; error counter++

#### Scenario: PBA pending bit persistence (per AC-MSIX-5.5)

- **GIVEN** PBA bit 已 set + vector masked
- **WHEN** mask 解除 + trigger_msix 再次调用
- **THEN** PBA bit 仍 true; TLP 正确生成

---

## MODIFIED Requirements

> None (本 change 不修改既有 requirements)

## REMOVED Requirements

> None (本 change 不删除既有 requirements)

## CROSS-REFERENCES

| 相关 Change | 关系 | 复用内容 |
|-----------|------|---------|
| Change B (`lwc-bus-hub-mvp`) | 上游, archived | LWC hub 设施 + MsixBundle (4 fields) |
| Change D (`pcie-tlp-entry-layer`) | 上游, archived | `PcieTlpSerializer` + `submit_tlp()` API |
| Change A (`vram-controller-functional-integration`) | 上游, archived | 0 关联 (独立路径) |
| Change C (`gmmu-pte-bus-correction`) | 上游, archived | 0 关联 (独立路径) |
| `cpptlm-dgpu-soc-arch-truth-cleanup` | Cross-ref | CRITICAL C3: `trigger_msix` API 全仓零匹配修复 |
