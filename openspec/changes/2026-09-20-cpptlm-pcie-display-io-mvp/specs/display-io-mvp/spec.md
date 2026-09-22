# Display IO 设备 MVP Spec

> **配套**: [proposal.md](../proposal.md) · [design.md](../design.md) · [tasks.md](../tasks.md)

## ADDED Requirements

### Requirement: PcieDisplayDevice Class
**Where**: `include/tlm/pcie/pcie_display_device.hh`

CppTLM dGPU SoC SHALL 提供 `PcieDisplayDevice` 类作为**显示 IO 设备**（Display Engine MVP）。

#### Scenario: Class instantiation
- **WHEN**: 调用 `PcieDisplayDevice dev;`
- **THEN**: 创建实例，分配 4KB 寄存器数组 + 32MB framebuffer

#### Scenario: BAR 0 mmio round-trip
- **WHEN**: 调用 `dev.mmio_write(0x10, &mode_val, 4)` 后 `dev.mmio_read(0x10, &read_val, 4)`
- **THEN**: `read_val == mode_val`（寄存器状态保存）

#### Scenario: BAR 1 backdoor round-trip
- **WHEN**: 调用 `dev.backdoor_write(0x1000, data, len)` 后 `dev.backdoor_read(0x1000, buf, len)`
- **THEN**: `memcmp(buf, data, len) == 0`（framebuffer 数据持久）

### Requirement: 寄存器布局
**Where**: `PcieDisplayDevice::registers_[4096]`

The display device SHALL 暴露以下寄存器映射：

| Offset | Access | Name | Purpose |
|--------|--------|------|---------|
| 0x00-0x0F | R | DEVICE_IDENTITY | vendor_id + device_id + revision 镜像 |
| 0x10-0x1F | RW | DISPLAY_CONTROL | mode (1920x1080/2560x1440/3840x2160), pixel_format |
| 0x20-0x2F | RW | FRAMEBUFFER_INFO | base_addr (identity mapping), size, pitch |
| 0x30-0x3F | R/W1C | STATUS | bit 0 = VBLANK pending (write-1-to-clear) |
| 0x40-0x4F | RW | INTERRUPT | mask + status 寄存器 |
| 0xF0-0xFF | RW | SCRATCH | 8 x uint32_t 测试用寄存器 |

#### Scenario: DEVICE_IDENTITY read
- **WHEN**: 驱动发起 `mmio_read(0x00, &val, 4)`
- **THEN**: 返回 vendor_id=0x1002 + device_id=0x0001（display device ID）

#### Scenario: DISPLAY_CONTROL write-then-read
- **WHEN**: 驱动写 DISPLAY_MODE=2 后再读 DISPLAY_MODE
- **THEN**: 读回 2（2560x1440）

#### Scenario: STATUS W1C behavior
- **WHEN**: VBLANK pending=1 时，驱动写 STATUS_CLEAR=1
- **THEN**: pending 变为 0（write-1-to-clear 语义）

### Requirement: BAR 路由扩展
**Where**: `pcie_bar_router_mvp`

The PCIe BAR router SHALL 扩展支持 PcieDisplayDevice。

#### Scenario: BAR 0 routed to display device
- **WHEN**: 驱动发起 `cpptlm_emulator_mmio_read(emu, 0, 0x10, buf, 4)`
- **THEN**: 路由到 `display_device_->mmio_read(0x10, buf, 4)` 并返 4

#### Scenario: BAR 1 routed to framebuffer
- **WHEN**: 驱动发起 `cpptlm_emulator_backdoor_read(emu, 1, offset, buf, len)`
- **THEN**: 路由到 `display_device_->backdoor_read(offset, buf, len)`

#### Scenario: Unmapped BAR returns EINVAL
- **WHEN**: 驱动发起 `cpptlm_emulator_mmio_read(emu, 2, ...)` (BAR 2 未映射)
- **THEN**: 返回 -EINVAL

### Requirement: VBLANK 中断链路
**Where**: `PcieEndpointIP::tick()`

The PCIe endpoint SHALL 推进显示设备的 VBLANK timer。

#### Scenario: VBLANK triggers every 1024 cycles
- **WHEN**: `tick()` 被调用 1024 次
- **THEN**: STATUS.VBLANK_PENDING=1 且 MSI-X vector 0 pending=1

#### Scenario: VBLANK counter increments
- **WHEN**: `tick()` 被调用 10240 次
- **THEN**: 内部 vblank_count_ == 10

#### Scenario: Pending cleared via W1C
- **WHEN**: VBLANK pending=1，驱动 mmio_write(0x34, &one, 4) (W1C)
- **THEN**: pending 变为 0

### Requirement: ABI 修复（事务层真实化）

CppTLM SHALL 修复以下 6 个 NO-OP ABI bug。

#### Scenario: cpptlm_emulator_pcie_config_read returns DEVICE_ID
- **WHEN**: 驱动调用 `cpptlm_emulator_pcie_config_read(emu, 0x00, 4, &val)`
- **THEN**: 返回 0 且 val != 0（vendor_id/device_id 已填充，修复前返 -ENOSYS）

#### Scenario: cpptlm_emulator_pcie_config_write persists
- **WHEN**: 驱动调用 `cpptlm_emulator_pcie_config_write(emu, 0x04, 1, 0xAB)`
- **THEN**: 返回 0 且后续读 offset 0x04 返 0xAB

#### Scenario: cpptlm_emulator_mmio_read fills buffer
- **WHEN**: 驱动 mmio_write DISPLAY_MODE=1 后 mmio_read DISPLAY_MODE
- **THEN**: 返回 4 且 buf 内容 == 1（修复前 buf 全 0）

#### Scenario: cpptlm_emulator_mmio_write persists
- **WHEN**: 驱动 mmio_write DISPLAY_MODE=1 后 mmio_read DISPLAY_MODE
- **THEN**: 返回 4 且 buf 内容 == 1（修复前立即返 0 丢弃数据）

#### Scenario: cpptlm_emulator_backdoor_read returns framebuffer data
- **WHEN**: 驱动 backdoor_write BAR 1 data 后 backdoor_read 同一地址
- **THEN**: 返回 len 且 buf 内容 == data（修复前伪装返 len 但 buf 未填）

#### Scenario: cpptlm_emulator_backdoor_write persists
- **WHEN**: 驱动 backdoor_write BAR 1 framebuffer
- **THEN**: 返回 len 且后续 backdoor_read 同一地址返相同数据

### Requirement: 22 ABI Signatures Unchanged
The 22 C ABI functions in `include/abi/cpptlm_emulator.h` SHALL remain byte-for-byte identical (no signature changes, no additions, no removals).

#### Scenario: ABI header file unchanged
- **WHEN**: 比较 D1 实施前后 `include/abi/cpptlm_emulator.h`
- **THEN**: 函数签名 + 数量一致（22 函数 + 4 callback typedef）

## MODIFIED Requirements

无（这是新功能，不修改既有 spec）

## REMOVED Requirements

无

## Cross-Repository Independence

本 spec 完全在 CppTLM 仓内可实现，**零依赖 ArchForge 仓**。
- 设计参考可在 ArchForge 仓查阅（VIRTUAL_PATHS），但运行时不需要
- UsrLinuxEmu 端构建 + 测试无需 clone ArchForge