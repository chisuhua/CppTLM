# Memory 设备 MVP Spec（v1.1 修订版）

> **配套**: [proposal.md](../proposal.md) · [design.md](../design.md) · [tasks.md](../tasks.md)
> **D2**: Memory 设备 MVP（区别 D1 Display IO 设备）
> **v1.0 → v1.1 修正**（2026-09-23 Oracle 复审触发）：D1 v1.1.1 实施后 Oracle 复审（ses_f342ea637ffeDWYBBEQDUUE63U）发现 D2 设计含 3 个与 D1 root cause 4 同型盲点。本 spec 在原 ADDED Requirements 基础上，**新增 MODIFIED Requirements 段**修订 D2 v1.1 实施细节。
> **方法**: TDD 6-step（T0 characterization → T0.5 路由 flag → T1 device → T2 EP → T3 board → T3.5 flag → T4 测试 → T5 docs）

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

---

## MODIFIED Requirements（v1.1 修订，2026-09-23）

> **MODIFIED 语义**：以下 5 个 Requirement 对上方 ADDED Requirements 中的对应条款进行修订。修订前请阅读"原 ADDED Requirement 段落 + 修订理由"。

### MODIFIED Requirement: 路由条件显式开关 `memory_routing_enabled_`（防劫持）

**原 ADDED Requirement 关联**: §"Requirement: DGpuBoard 路由 BAR 2 到 PcieMemoryDevice" + §"Requirement: BAR 0 routing priority (display > memory)"（spec.md:105-133）

**修改来源**: Oracle 复审根因 4 类比（D1 root cause 4 同型盲点，session ses_f342ea637ffeDWYBBEQDUUE63U）

**WHERE**: `include/tlm/gpu/dgpu_board_shell.hh` + `src/tlm/gpu/dgpu_board_shell.cc`

The DGpuBoard SHALL require `memory_routing_enabled_ == true` in addition to `soc_ && has_memory_device()` before routing BAR 0/2/backdoor to PcieMemoryDevice.

The flag SHALL default to `false`. SOC configuration `dgpu_soc_with_memory_device.json` SHALL explicitly set `"memory_routing_enabled": true` to enable D2 routing. Other dGPU configurations SHALL keep the default `false` to preserve dGPU BAR0 registers (doorbell/GPFIFO_PUT/DISPLAY_MODE).

(替代原 ADDED §"DGpuBoard 路由 BAR 2" 中隐含的"soc_ && ep && has_memory_device()"——dGPU 自身 BAR0 寄存器可能被 memory device 误劫持)

#### Scenario: 未启用时 BAR 0 不被劫持
- **WHEN**: `memory_routing_enabled_=false && has_memory_device()=true`
- **THEN**: 驱动 `board.mmio_write(0, 0x10, val, 4)`（MEM_SIZE_LO）走 dGPU 原 BAR0 寄存器路径，**不**写入 PcieMemoryDevice

#### Scenario: 启用时 memory device 接管 BAR 0/2
- **WHEN**: `memory_routing_enabled_=true && has_memory_device()=true && has_display_device()=false`
- **THEN**: 驱动 `board.mmio_write(0, 0x10, val, 4)` 写入 PcieMemoryDevice

#### Scenario: 双 flag 同时启用时 BAR 0 D1 优先
- **WHEN**: `display_routing_enabled_=true && memory_routing_enabled_=true && has_display_device()=true && has_memory_device()=true`，BAR 0 mmio_write
- **THEN**: 走 D1 PcieDisplayDevice 路径（D1 优先于 D2 memory）

#### Scenario: JSON 配置显式启用
- **WHEN**: `examples/dgpu_soc_with_memory_device.json` 顶层含 `"memory_routing_enabled": true`
- **THEN**: `DGpuBoard::init()` 末尾读取 JSON 字段并调 `set_memory_routing_enabled(true)`

### MODIFIED Requirement: 返回值契约统一为"0 成功 / -errno 失败"

**原 ADDED Requirement 关联**: §"Requirement: PcieMemoryDevice Class"（spec.md:8-51）

**修改来源**: D1 v1.1.1 修订统一契约（Oracle R7 冻结裁决）

**WHERE**: `PcieMemoryDevice::mmio_read/write` + `memory_read/write` + `backdoor_read/write`

The four API methods SHALL return `0` on success and negative errno (`-EINVAL`, etc.) on failure. The return value SHALL NOT indicate byte count transferred.

#### Scenario: mmio_read 4 字节成功- **WHEN**: 驱动发起 `dev.mmio_read(0x10, buf, 4)`
- **THEN**: 返回 `0`（不是 4），buf 填入 4 字节设备状态

#### Scenario: memory_read 256 字节成功
- **WHEN**: 驱动发起 `dev.memory_read(0x10000, buf, 256)`
- **THEN**: 返回 `0`（不是 256），buf 填入 256 字节 memory backing 数据

### MODIFIED Requirement: DEVICE_IDENTITY 默认初始化

**原 ADDED Requirement 关联**: §"Requirement: PcieMemoryDevice Class" + §"Scenario: DEVICE_IDENTITY read"（spec.md:8-51）

**修改来源**: D1 v1.1.1 修订根因 2（D1 同型问题）

**WHERE**: `PcieMemoryDevice::PcieMemoryDevice()` 构造函数

The memory device identity SHALL be initialized at construction time:
- `registers_[0..1]` SHALL contain `kVendorId` (0x1002) in little-endian encoding
- `registers_[2..3]` SHALL contain `kDeviceId` (0x0002, 区别 D1 的 0x0001) in little-endian encoding

#### Scenario: 构造后立即读 identity
- **WHEN**: `PcieMemoryDevice dev; dev.mmio_read(0x00, &vid, 2);`
- **THEN**: 返回 `0`，`vid == 0x1002`

### MODIFIED Requirement: fast-path 路由顺序——power gate 在前

**原 ADDED Requirement 关联**: §"Requirement: DGpuBoard 路由 BAR 2 到 PcieMemoryDevice"

**修改来源**: D1 v1.1.1 修订根因 3（D1 同型问题）

**WHERE**: `src/tlm/gpu/dgpu_board_shell.cc::mmio_read` + `mmio_write` + `backdoor_read/write`

The DGpuBoard fast-path routing to PcieMemoryDevice SHALL occur **after** the `is_mmio_gated()` power-state gate check. This preserves the INV-A invariant: D3 power state → BAR 0/2 mmio SHALL return `-EIO` regardless of memory device presence.

#### Scenario: D3 power state 下 BAR 0/2 mmio_read 返 -EIO
- **WHEN**: `memory_routing_enabled_=true && ep->power_state()==D3hot`
- **THEN**: `board.mmio_read(0/2, 0x10, buf, 4)` 返回 `-EIO`（不路由 device）

### MODIFIED Requirement: BAR 0 routing priority 明确化

**原 ADDED Requirement 关联**: §"Requirement: BAR 0 routing priority (display > memory)"（spec.md:122-133）

**修改来源**: v1.1 修订决策（与 D1 v1.1.1 对称）

**WHERE**: `src/tlm/gpu/dgpu_board_shell.cc::mmio_read/write`

The DGpuBoard BAR 0 routing priority SHALL be:
1. **`display_routing_enabled_=true && has_display_device()=true`** → PcieDisplayDevice（**D1 优先**）
2. **`memory_routing_enabled_=true && has_memory_device()=true`**（display 不可用或 routing 关闭）→ PcieMemoryDevice（D2 兜底）
3. 其他 → shell-local `mmio_regs_` map fallback

#### Scenario: 双 device 同时启用时 D1 优先（D1 root cause 4 防御）
- **WHEN**: `display_routing_enabled_=true && memory_routing_enabled_=true && has_display_device()=true && has_memory_device()=true`，BAR 0 offset 0x10 mmio_write
- **THEN**: 写入 PcieDisplayDevice::kRegDisplayMode（**不**写入 PcieMemoryDevice::kRegMemSizeLo）

#### Scenario: 仅 D2 启用时 D2 接管 BAR 0
- **WHEN**: `display_routing_enabled_=false && memory_routing_enabled_=true && has_display_device()=true && has_memory_device()=true`
- **THEN**: 驱动 BAR 0 mmio_write 写入 PcieMemoryDevice（DISPLAY_DEVICE 即使存在也不路由）

#### Scenario: 仅 D1 启用时 D1 接管 BAR 0
- **WHEN**: `display_routing_enabled_=true && memory_routing_enabled_=false`，BAR 0 mmio_write
- **THEN**: 走 D1 PcieDisplayDevice（与 v1.1.1 一致）

---

## v1.0 → v1.1 主要修正（spec 层面，2026-09-23 Oracle 复审触发）

| v1.0 措辞 | v1.1 修正 | 修订来源 |
|----------|------------|----------|
| `mmio_read/write` 路由 BAR 0/2（条件：`soc_ && ep && has_memory_device()`） | + `memory_routing_enabled_` flag（默认 false） | D1 root cause 4 同型 |
| 返回值隐含"返字节数" | 返 0 成功 / -errno 失败（与 D1 v1.1.1 统一） | D1 v1.1.1 修订 |
| DEVICE_IDENTITY "设备自管身份"（运行时填充假设） | ctor 写 0x1002/0x0002 到 registers_[0..3] | D1 v1.1.1 同型 |
| fast-path 路由（无条件优先） | 在 `is_mmio_gated()` 检查之后 | D1 v1.1.1 同型 |
| "display 优先，memory 兜底"（模糊） | 显式三段 priority：display → memory → fallback | v1.1 决策 |
| 5 个 ADDED Requirement | + 5 个 MODIFIED Requirement | 修订 |
