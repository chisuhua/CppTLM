# display-io-mvp Specification

## Purpose
TBD - created by archiving change 2026-09-20-cpptlm-pcie-display-io-mvp. Update Purpose after archive.
## Requirements
### Requirement: PcieDisplayDevice Class

This Requirement SHALL be implemented per design.md §12 v1.1.1.

This Requirement SHALL be satisfied by the D1 v1.1.1 implementation.

CppTLM dGPU SoC SHALL 提供 `PcieDisplayDevice` 类作为**显示 IO 设备**（Display Engine MVP）。

The class SHALL:
- 暴露 4KB MMIO 寄存器空间（`registers_[kRegSize]`）
- 暴露 32MB Framebuffer backing store（`framebuffer_`）
- 提供 `mmio_read(offset, buf, len)` / `mmio_write(offset, buf, len)` API
- 提供 `backdoor_read(offset, buf, len)` / `backdoor_write(offset, buf, len)` API
- 提供 `tick(MsiXTable& msix)` 方法推进内部 VBLANK timer
- 每 1024 cycle 触发一次 VBLANK 中断（写 STATUS.VBLANK_PENDING + 调 `msix.update_pending(0)`）

The class SHALL NOT:
- 真实渲染像素（无 scan-out）
- 集成 SDMA / SM
- 实现 Power Management（D-states）

#### Scenario: Class instantiation
- **WHEN**: 调用 `PcieDisplayDevice dev;`
- **THEN**: 创建实例，分配 4KB `registers_` 数组（零初始化），`framebuffer_` 延迟分配

#### Scenario: BAR 0 mmio round-trip
- **WHEN**: 调用 `dev.mmio_write(kRegDisplayControl, &mode_val, 4)` 后 `dev.mmio_read(kRegDisplayControl, &read_val, 4)`
- **THEN**: `read_val == mode_val`（寄存器状态保存）

#### Scenario: BAR 0 unaligned access
- **WHEN**: 调用 `dev.mmio_write(0x15, &val, 4)`（起始 offset 非 4 字节对齐）
- **THEN**: 返回 0（**实现**支持 unaligned，v1.0 未明确）

#### Scenario: BAR 0 out-of-range
- **WHEN**: 调用 `dev.mmio_read(0x1000, buf, 4)`（offset+len > 4096）
- **THEN**: 返回 -EINVAL

#### Scenario: BAR 1 backdoor round-trip
- **WHEN**: 调用 `dev.backdoor_write(0x1000, data, len)` 后 `dev.backdoor_read(0x1000, buf, len)`
- **THEN**: `memcmp(buf, data, len) == 0`（framebuffer 数据持久）

#### Scenario: RO register write silently ignored
- **WHEN**: 调用 `dev.mmio_write(kRegDeviceIdentity, &val, 4)`（DEVICE_IDENTITY 是 RO）
- **THEN**: 返回 0，但寄存器不变（驱动误写 RO 寄存器不报错）

### Requirement: 寄存器布局

This Requirement SHALL be implemented per design.md §12 v1.1.1.

This Requirement SHALL be satisfied by the D1 v1.1.1 implementation.

The display device SHALL 暴露以下寄存器映射（精确偏移）：

| Offset | Access | Name | Purpose |
|--------|--------|------|---------|
| 0x00-0x0F | R | DEVICE_IDENTITY | vendor_id (0x1002) + device_id (0x0001) + revision 镜像 |
| 0x10 | RW | DISPLAY_MODE | 0=off, 1=1920x1080, 2=2560x1440, 3=3840x2160 |
| 0x14 | RW | PIXEL_FORMAT | 0=RGB888, 1=RGBA8888, 2=NV12 |
| 0x20 | RW | FB_BASE_LO | Framebuffer 主机可见基地址（D1: identity） |
| 0x28 | RW | FB_SIZE | 默认 32MB |
| 0x2C | RW | FB_PITCH | Bytes per scan line |
| 0x30 | R/W1C | STATUS | bit 0 = VBLANK pending (write-1-to-clear) |
| 0x34 | W | STATUS_CLEAR | W1C for VBLANK pending |
| 0x40 | RW | INT_MASK | bit 0 = VBLANK mask (0=masked, 1=enabled) |
| 0x44 | R | INT_STATUS | 镜像 STATUS bit 0 |
| 0xF0-0xFF | RW | SCRATCH[0..3] | 4 × uint32_t 测试用 |

#### Scenario: DEVICE_IDENTITY read
- **WHEN**: 驱动发起 `mmio_read(0x00, &val, 4)`
- **THEN**: 返回 vendor_id=0x1002 + device_id=0x0001（device 自身 ID，不是 config space 镜像——v1.1 修正：device 自管身份）

#### Scenario: DISPLAY_CONTROL write-then-read
- **WHEN**: 驱动写 DISPLAY_MODE=2 后再读 DISPLAY_MODE
- **THEN**: 读回 2（2560x1440）

#### Scenario: STATUS W1C behavior
- **WHEN**: VBLANK pending=1 时，驱动写 STATUS_CLEAR=1（offset 0x34）
- **THEN**: pending 变为 0（write-1-to-clear 语义）

### Requirement: PcieEndpointIP 持有 display_device

This Requirement SHALL be implemented per design.md §12 v1.1.1.

This Requirement SHALL be satisfied by the D1 v1.1.1 implementation.

The PCIe endpoint SHALL 持有 `PcieDisplayDevice` 实例。

#### Scenario: EP has display_device after construction
- **WHEN**: `PcieEndpointIP ep(name, eq);`
- **THEN**: `ep.has_display_device() == true`，`ep.display_device()` 返回非空引用

#### Scenario: EP accessor returns valid reference
- **WHEN**: 任意时刻调用 `ep.display_device()`
- **THEN**: 返回对 `PcieDisplayDevice` 的有效引用（生命周期与 EP 绑定）

### Requirement: VBLANK timer 推进

This Requirement SHALL be implemented per design.md §12 v1.1.1.

This Requirement SHALL be satisfied by the D1 v1.1.1 implementation.

The PCIe endpoint SHALL 在 `tick()` 中推进显示设备的 VBLANK timer。

#### Scenario: VBLANK triggers every 1024 cycles
- **WHEN**: `PcieEndpointIP::tick()` 被调用 1024 次
- **THEN**: `STATUS.VBLANK_PENDING=1`（registers_[0x30/4] bit 0 = 1），且 `MsiXTable::update_pending(0)` 被调用

#### Scenario: VBLANK counter increments
- **WHEN**: `tick()` 被调用 10240 次
- **THEN**: `display_device().vblank_count() == 10`

#### Scenario: INT_MASK=0 suppresses MSI-X
- **WHEN**: INT_MASK=0（VBLANK masked）+ tick 1024 次
- **THEN**: STATUS.VBLANK_PENDING=1，但 `msix.update_pending(0)` **不**被调用

#### Scenario: Pending cleared via W1C
- **WHEN**: VBLANK pending=1，驱动 mmio_write(0x34, &one, 4)
- **THEN**: STATUS.VBLANK_PENDING 变为 0

### Requirement: DGpuBoard 路由 BAR 0 到 PcieDisplayDevice

This Requirement SHALL be implemented per design.md §12 v1.1.1.

This Requirement SHALL be satisfied by the D1 v1.1.1 implementation.

The DGpuBoard SHALL 在 device 存在时将 BAR 0 MMIO 路由到 `PcieDisplayDevice`。

#### Scenario: BAR 0 mmio_write routes to device
- **WHEN**: 接 SOC + EP + display_device 存在，驱动 `mmio_write(bar=0, off=0x10, &mode=1, 4)`
- **THEN**: 写入 `display_device_->registers_[kRegDisplayControl] = 1`

#### Scenario: BAR 0 mmio_read returns device state
- **WHEN**: 上面 write 后，驱动 `mmio_read(bar=0, off=0x10, &val, 4)`
- **THEN**: `val == 1`（来自 device，非 shell-local `mmio_regs_`）

#### Scenario: BAR 0 routes when device exists
- **WHEN**: `display_device()` 存在，`mmio_read(bar=0, off=0x00, buf, 4)`
- **THEN**: 路由到 `display_device_->mmio_read(0x00, buf, 4)`

#### Scenario: BAR 0 fallback when no device (向后兼容)
- **WHEN**: 无 display_device（`has_display_device() == false`），`mmio_write(bar=0, off=0x10, &val, 4)`
- **THEN**: 写入原 `mmio_regs_[map]`（**不**调用 device，向后兼容现有测试）

### Requirement: DGpuBoard 路由 backdoor 到 PcieDisplayDevice framebuffer

This Requirement SHALL be implemented per design.md §12 v1.1.1.

This Requirement SHALL be satisfied by the D1 v1.1.1 implementation.

The DGpuBoard SHALL 在 device 存在时将 `backdoor_read/write`（board 内部方法）路由到 `PcieDisplayDevice::backdoor_read/write`。

#### Scenario: backdoor_read routes to device
- **WHEN**: device 存在，调用 `board->backdoor_read(0x1000, buf, 4096)`
- **THEN**: 路由到 `display_device_->backdoor_read(0x1000, buf, 4096)`，从 device framebuffer 读

#### Scenario: backdoor fallback when no device
- **WHEN**: 无 device，调用 `board->backdoor_read(0x1000, buf, 4096)`
- **THEN**: 走原 `vram_segments_` 路径（**已有修复 #6**：miss 返 -ENOENT）

#### Scenario: cpptlm_emulator_backdoor_read NOT in ABI
- **WHEN**: 搜索 `include/abi/cpptlm_emulator.h` 找 `cpptlm_emulator_backdoor_read`
- **THEN**: 0 命中（**确认**: ABI 中**没有**此函数，v1.0 提议修复为假命题）

### Requirement: PcieConfigSpace 路由（已存在，**不修改**）

This Requirement SHALL be implemented per design.md §12 v1.1.1.

This Requirement SHALL be satisfied by the D1 v1.1.1 implementation.

`PcieConfigSpace` 路由 SHALL 保持当前已修复的行为；D1 不修改此路径。

#### Scenario: config_read 真实化（已修复）
- **WHEN**: 接 SOC + EP，调用 `cpptlm_emulator_pcie_config_read(emu, 0x00, 4, &val)`
- **THEN**: 返回 0，`val = ep->config_space().read(0)`（**v1.1 不修改**——已有实现）

#### Scenario: config_read 未接 EP 返 ENOSYS
- **WHEN**: 无 SOC，调用 `cpptlm_emulator_pcie_config_read(emu, 0x00, 4, &val)`
- **THEN**: 返回 -ENOSYS（**v1.1 不修改**——保留原行为）

### Requirement: MSI-X 触发链（**D1 范围明确**）

This Requirement SHALL be implemented per design.md §12 v1.1.1.

This Requirement SHALL be satisfied by the D1 v1.1.1 implementation.

The D1 SHALL 触发 `MsiXTable::update_pending(0)`（**仅此一步**）；`intr_cb` 派发 + `trigger_irq_async` 由**现有已修复代码**完成（per `dgpu_board_shell.cc:466-480`，v1.1 不修改）。

#### Scenario: D1 触发 MSI-X pending
- **WHEN**: tick 1024 次（device 内部）+ INT_MASK=1
- **THEN**: `msix.update_pending(0)` 被调用 1 次

#### Scenario: intr_cb 派发不在 D1 范围
- **WHEN**: 驱动调用 `cpptlm_emulator_register_callbacks(emu, intr_cb, err_cb, ...)` 后触发 VBLANK
- **THEN**: intr_cb 被现有代码派发（**不**是 D1 改动；`dgpu_board_shell.cc` 已修复）

### Requirement: 15 ABI Functions Unchanged（**v1.1 修正数字**）

This Requirement SHALL be implemented per design.md §12 v1.1.1.

This Requirement SHALL be satisfied by the D1 v1.1.1 implementation.

The 15 C ABI functions + 4 callback typedef in `include/abi/cpptlm_emulator.h` SHALL remain byte-for-byte identical (no signature changes, no additions, no removals).

#### Scenario: ABI header diff empty
- **WHEN**: 比较 D1 实施前后 `include/abi/cpptlm_emulator.h`（`git diff HEAD -- include/abi/cpptlm_emulator.h`）
- **THEN**: 输出为空（**v1.1 修正**：替代 v1.0 "22 函数 + 4 typedef" 不可验证表述）

#### Scenario: Function count = 15
- **WHEN**: `grep -c "^uint32_t cpptlm_emulator_\|^int cpptlm_emulator_\|^void cpptlm_emulator_\|^cpptlm_emulator_t\* cpptlm_emulator_" include/abi/cpptlm_emulator.h`
- **THEN**: 输出 15（必须包含 `uint32_t` 模式，因 `cpptlm_emulator_get_device_count` 是 `uint32_t` 返回类型，line 69）

#### Scenario: Callback typedef count = 4
- **WHEN**: `grep -c "typedef.*cpptlm_.*_cb_t" include/abi/cpptlm_emulator.h`
- **THEN**: 输出 4

#### Scenario: src/abi/cpptlm_emulator.cc wrapper unchanged
- **WHEN**: 比较 D1 实施前后 `src/abi/cpptlm_emulator.cc`
- **THEN**: 输出为空（**ABI wrapper 已正确，不修改**）

### Requirement: Freeze Surface Untouched

This Requirement SHALL be implemented per design.md §12 v1.1.1.

This Requirement SHALL be satisfied by the D1 v1.1.1 implementation.

D1 SHALL NOT modify either `pcie_endpoint_tlm.h` (PcieEndpointTLM 4 端口冻结) or `cpptlm_emulator.h` (ABI 冻结)。

#### Scenario: pcie_endpoint_tlm.h not modified
- **WHEN**: `git diff HEAD -- include/tlm/gpu/pcie_endpoint_tlm.h`
- **THEN**: 输出为空（**PcieEndpointTLM 4 端口冻结**）

#### Scenario: cpptlm_emulator.h not modified
- **WHEN**: `git diff HEAD -- include/abi/cpptlm_emulator.h`
- **THEN**: 输出为空

