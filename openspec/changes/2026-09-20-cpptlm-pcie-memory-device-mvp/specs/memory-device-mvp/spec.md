# Memory 设备 MVP Spec

> **配套**: [proposal.md](../proposal.md) · [design.md](../design.md) · [tasks.md](../tasks.md)
> **D2**: Memory 设备 MVP（区别 D1 Display IO 设备）

## ADDED Requirements

### Requirement: PcieMemoryDevice Class
**Where**: `include/tlm/gpu/pcie_memory_device.hh`

CppTLM dGPU SoC SHALL 提供 `PcieMemoryDevice` 类作为**内存设备**（Memory Device MVP）。

The class SHALL:
- 暴露 4KB MMIO 寄存器空间（`registers_[kRegSize]`）
- 暴露 8GB BAR 2 memory backing store（`memory_backing_`，lazy alloc）
- 提供 `mmio_read(offset, buf, len)` / `mmio_write(offset, buf, len)` API
- 提供 `memory_read(offset, buf, len)` / `memory_write(offset, buf, len)` API（BAR 2 backing）
- 提供 `tick()` 方法推进内部 cycle counter
- device_id = 0x0002（区别 D1 DisplayDevice 的 0x0001）
- **NO MSI-X**（D2 无中断）
- **NO VBLANK**（D2 是被动内存设备）

The class SHALL NOT:
- 触发任何中断（无 MSI-X）
- 集成 GPU 计算
- 实现 Power Management（D-states）

#### Scenario: Class instantiation
- **WHEN**: 调用 `PcieMemoryDevice dev;`
- **THEN**: 创建实例，分配 4KB `registers_` 数组（零初始化），`memory_backing_` 延迟分配

#### Scenario: BAR 0 mmio round-trip
- **WHEN**: 调用 `dev.mmio_write(kRegMemSizeLo, &size_lo, 4)` 后 `dev.mmio_read(kRegMemSizeLo, &read_val, 4)`
- **THEN**: `read_val == size_lo`（寄存器状态保存）

#### Scenario: BAR 0 unaligned access
- **WHEN**: 调用 `dev.mmio_write(0x15, &val, 4)`（起始 offset 非 4 字节对齐）
- **THEN**: 返回 0（实现支持 unaligned）

#### Scenario: BAR 0 out-of-range
- **WHEN**: 调用 `dev.mmio_read(0x1000, buf, 4)`（offset+len > 4096）
- **THEN**: 返回 -EINVAL

#### Scenario: BAR 2 memory backing round-trip
- **WHEN**: 调用 `dev.memory_write(0x1000, data, len)` 后 `dev.memory_read(0x1000, buf, len)`
- **THEN**: `memcmp(buf, data, len) == 0`（memory backing 数据持久）

#### Scenario: RO register write silently ignored
- **WHEN**: 调用 `dev.mmio_write(kRegDeviceIdentity, &val, 4)`（DEVICE_IDENTITY 是 RO）
- **THEN**: 返回 0，但寄存器不变

### Requirement: 寄存器布局
**Where**: `PcieMemoryDevice::registers_[4096]`

The memory device SHALL 暴露以下寄存器映射（精确偏移）：

| Offset | Access | Name | Purpose |
|--------|--------|------|---------|
| 0x00-0x0F | R | DEVICE_IDENTITY | vendor_id (0x1002) + device_id (0x0002) + revision |
| 0x10 | RW | MEM_SIZE_LO | 容量低 32-bit（默认 8GB 低 32-bit） |
| 0x14 | RW | MEM_SIZE_HI | 容量高 32-bit（默认 8GB 高 32-bit = 0x00000002） |
| 0x18 | RW | MEM_BASE_LO | 主机可见基地址低 32-bit（D2: identity = 0x0） |
| 0x1C | RW | MEM_BASE_HI | 主机可见基地址高 32-bit |
| 0x20 | RW | STATUS | bit 0=ready, bit 1=error |
| 0xF0-0xFF | RW | SCRATCH[0..3] | 4 × uint32_t 测试用 |

#### Scenario: DEVICE_IDENTITY read
- **WHEN**: 驱动发起 `mmio_read(0x00, &val, 4)`
- **THEN**: 返回 vendor_id=0x1002 + device_id=0x0002（device 自身 ID）

#### Scenario: MEM_SIZE write-then-read
- **WHEN**: 驱动写 MEM_SIZE_LO=0x00000000 + MEM_SIZE_HI=0x00000002 后再读
- **THEN**: 读回 0x00000000 / 0x00000002（8GB）

#### Scenario: STATUS ready bit
- **WHEN**: 驱动读 STATUS register
- **THEN**: bit 0 = ready（RW），bit 1 = error（W1C）

### Requirement: PcieEndpointIP 持有 memory_device
**Where**: `include/tlm/pcie/pcie_endpoint_ip.hh`

The PCIe endpoint SHALL 持有 `PcieMemoryDevice` 实例。

#### Scenario: EP has memory_device after construction
- **WHEN**: `PcieEndpointIP ep(name, eq);`
- **THEN**: `ep.has_memory_device() == true`，`ep.memory_device()` 返回非空引用

#### Scenario: EP accessor returns valid reference
- **WHEN**: 任意时刻调用 `ep.memory_device()`
- **THEN**: 返回对 `PcieMemoryDevice` 的有效引用（生命周期与 EP 绑定）

### Requirement: cycle_counter 推进
**Where**: `PcieEndpointIP::tick()` + `PcieMemoryDevice::tick()`

The PCIe endpoint SHALL 在 `tick()` 中推进 memory device 的 cycle counter。

#### Scenario: cycle_counter increments
- **WHEN**: `PcieEndpointIP::tick()` 被调用 1024 次
- **THEN**: `memory_device().cycle_counter() == 1024`

#### Scenario: cycle_counter continues from previous value
- **WHEN**: tick 1024 次后，再 tick 1024 次
- **THEN**: `memory_device().cycle_counter() == 2048`

### Requirement: DGpuBoard 路由 BAR 2 到 PcieMemoryDevice
**Where**: `src/tlm/gpu/dgpu_board_shell.cc` 的 `mmio_read/write` 和 `backdoor_read/write`

The DGpuBoard SHALL 在 memory_device 存在时将 BAR 2 路由到 `PcieMemoryDevice`。

#### Scenario: BAR 2 backdoor_write routes to memory device
- **WHEN**: 接 SOC + EP + memory_device 存在，驱动 `backdoor_write(offset=0x1000, data, 8)`
- **THEN**: 写入 `memory_device_->memory_backing_[0x1000..0x1007]`

#### Scenario: BAR 2 backdoor_read returns memory device state
- **WHEN**: 上面 write 后，驱动 `backdoor_read(offset=0x1000, buf, 8)`
- **THEN**: `memcmp(buf, data, 8) == 0`（来自 device，非 shell-local）

#### Scenario: BAR 2 fallback when no memory_device (向后兼容)
- **WHEN**: 无 memory_device（`has_memory_device() == false`），`backdoor_write(offset=0x1000, data, 8)`
- **THEN**: 写入原 `vram_segments_`（**不**调用 device，向后兼容现有测试）

### Requirement: BAR 0 routing priority (display > memory)
**Where**: `src/tlm/gpu/dgpu_board_shell.cc` 的 `mmio_read/write`

The DGpuBoard SHALL prioritize display_device over memory_device when both exist on BAR 0.

#### Scenario: BAR 0 routes to display_device when both exist
- **WHEN**: `has_display_device() == true` 且 `has_memory_device() == true`，BAR 0 mmio_write
- **THEN**: 路由到 `display_device_->mmio_write()`（**D1 优先**）

#### Scenario: BAR 0 routes to memory_device when display_device absent
- **WHEN**: `has_display_device() == false` 且 `has_memory_device() == true`，BAR 0 mmio_write
- **THEN**: 路由到 `memory_device_->mmio_write()`（D2 兜底）

### Requirement: 15 ABI Functions Unchanged
**Where**: `include/abi/cpptlm_emulator.h`

The 15 C ABI functions + 4 callback typedef in `include/abi/cpptlm_emulator.h` SHALL remain byte-for-byte identical (no signature changes, no additions, no removals).

#### Scenario: ABI header diff empty
- **WHEN**: 比较 D2 实施前后 `include/abi/cpptlm_emulator.h`（`git diff HEAD -- include/abi/cpptlm_emulator.h`）
- **THEN**: 输出为空

#### Scenario: Function count = 15
- **WHEN**: `grep -c "^uint32_t cpptlm_emulator_\|^int cpptlm_emulator_\|^void cpptlm_emulator_\|^cpptlm_emulator_t\* cpptlm_emulator_" include/abi/cpptlm_emulator.h`
- **THEN**: 输出 15

#### Scenario: Callback typedef count = 4
- **WHEN**: `grep -c "typedef.*cpptlm_.*_cb_t" include/abi/cpptlm_emulator.h`
- **THEN**: 输出 4

### Requirement: Freeze Surface Untouched
**Where**: `include/tlm/gpu/pcie_endpoint_tlm.h` + `include/abi/cpptlm_emulator.h`

D2 SHALL NOT modify either `pcie_endpoint_tlm.h` (PcieEndpointTLM 4 端口冻结) or `cpptlm_emulator.h` (ABI 冻结)。

#### Scenario: pcie_endpoint_tlm.h not modified
- **WHEN**: `git diff HEAD -- include/tlm/gpu/pcie_endpoint_tlm.h`
- **THEN**: 输出为空

#### Scenario: cpptlm_emulator.h not modified
- **WHEN**: `git diff HEAD -- include/abi/cpptlm_emulator.h`
- **THEN**: 输出为空

#### Scenario: PcieDisplayDevice unchanged
- **WHEN**: `git diff HEAD -- include/tlm/gpu/pcie_display_device.hh`
- **THEN**: 输出为空（D1 保持不变）

## Cross-Repository Independence

本 spec 完全在 CppTLM 仓内可实现，**零依赖 ArchForge 仓**。
- 设计参考可在 ArchForge 仓查阅（VIRTUAL_PATHS），但运行时不需要
- UsrLinuxEmu 端构建 + 测试无需 clone ArchForge
