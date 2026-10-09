# pcie-resizable-bar-runtime Specification

## Purpose
TBD - created by archiving change cpptlm-pcie-host-test-flr-rebar. Update Purpose after archive.
## Requirements
### Requirement: resizable-bar-runtime-protocol (v1.1 复用既有 + uint64)

`ResizableBarRuntime` SHALL 提供 Runtime resize 协议 **薄 wrapper** (per PCIe Base Spec §7.8.6), **复用既有** `enable_resizable_bar() + on_bar_resize()` API (per `pcie_endpoint_ip.cc:670-694`).

**v1.1 关键修改**: `ResizableBar::new_size_bytes_` 字段类型 `uint32_t` → `uint64_t` (per CRITICAL C3: 4GB 溢出)

#### Scenario: Resizable BAR 协议 validate (per AC G4)

- **GIVEN** `ResizableBarRuntime runtime;` 初始
- **WHEN** `runtime.validate(bar_idx=2, new_size=0x100000000ULL)` (4GB, **uint64**) 调
- **THEN** 验证: power-of-2 ✓ (4GB = 2^32), >= 1MB ✓, <= max_size ✓, BAR0 ≠ 2 ✓
- **AND** 返回 `true` (commit 可行)
- **WHEN** `runtime.validate(bar_idx=2, new_size=3GB)` 调
- **THEN** 验证: power-of-2 ✗ (3GB 非 2^n)
- **AND** 返回 `false` (commit 拒绝)
- **WHEN** `runtime.validate(bar_idx=0, new_size=2GB)` 调
- **THEN** 验证: BAR0 fixed (per PCIe spec)
- **AND** 返回 `false` (commit 拒绝)

#### Scenario: Resizable BAR commit + config space 同步 (v1.1 修复 64-bit 编码)

- **GIVEN** `ResizableBarRuntime runtime;` + `PcieEndpointIP ep;` 初始 (BAR2 = 8GB)
- **WHEN** `runtime.commit(bar_idx=2, new_size=0x100000000ULL)` (4GB, **uint64**) 调
- **THEN** 复用既有 `ep.enable_resizable_bar(2)` (INV-C 序列)
- **AND** `ep.resizable_bar(2).reprogram_size(0x100000000ULL)` 调 (uint64, **修 v1.0 uint32 溢出**)
- **AND** `ep.on_bar_resize(2, 0x100000000ULL)` 调 (既有 callback, per Change B)
- **AND** config space BAR register 同步重写 (v1.1 **修 v1.0 编码错误**):
  - 0x18 (BAR2 register): low dword = **0** (64-bit BAR pair, low 32-bit of 4GB = 0)
  - 0x1C (BAR3 upper dword): high dword = **1** (high 32-bit of 4GB = 0x100000000 >> 32 = 1)
- **NOTE**: **v1.1 修 v1.0 编码错误** — v1.0 spec "low dword = 4GB encoding, high dword = 0 (32-bit addressing)" 自相矛盾（4GB 需要 33 bits, 32-bit 无法表达）

---

### Requirement: resizable-bar-abi-entry (v1.1 删除)

`cpptlm_emulator_resize_bar()` SHALL **不实现** (v1.1 删除). HostPcieTestStub 通过既有 `DGpuBoard::resize_bar()` C++ 入口触发 resize.

#### Scenario: HostPcieTestStub trigger_resize_bar() 入口 (per AC G4, v1.1 改写)

- **GIVEN** `DGpuBoard* board` (per `cpptlm_emulator_create("configs/dgpu_*.json")`)
- **WHEN** `HostPcieTestStub(board, config).trigger_resize_bar(2, 0x100000000ULL)` (uint64) 调
- **THEN** 转发到 `board->resize_bar(2, 0x100000000)` → `PcieEndpointIP::handle_resize_bar(2, 4GB)` → `ResizableBarRuntime::commit()`
- **AND** **0 新增 ABI 函数** (per CRITICAL C2: ADR-DGPU-08 §2.3 0 新增约束)
- **AND** `find include/abi/cpptlm_emulator_resizable_bar.h` = 0 匹配 (v1.1 删除确认)

---

### Requirement: resizable-bar-routing-consistency (v1.1 统一归属)

Resize 完成后, BAR routing SHALL 与新 size 一致 (per Change B `PcieControllerIP::match_bar` 4 枚举 + Change C Strangler gate verdict).

**v1.1 统一路由归属**（修 v1.0 三处不一致）:
- ✅ **PcieControllerIP::resize_bar()** 不存在（v1.0 提议，**删除**）
- ✅ 改为 **PcieEndpointIP::handle_resize_bar(bar_idx, new_size)**
- ✅ DGpuBoard::resize_bar() 路由到 PcieEndpointIP::handle_resize_bar()
- ✅ ResizableBarRuntime::commit() 复用既有 `enable_resizable_bar() + on_bar_resize()`

#### Scenario: BAR routing 仍可路由 (per AC G4)

- **GIVEN** BAR1 从 16MB resize 到 64MB
- **WHEN** host 读 BAR1 offset 0x1000000 (16MB 处)
- **THEN** `PcieControllerIP::match_bar(1, 0x1000000)` 返 `Bar1FbFastPath` (per 4 枚举 verdict)
- **AND** `pcie_ep()->bar_size(1) == 64MB` (64 * 1024 * 1024)
- **AND** framebuffer fast-path 仍可正确读写

#### Scenario: BAR 上限保护 (per Risk #2)

- **GIVEN** host 写 BAR1 offset 0x8000000 (128MB, 超出 resize 后的 64MB)
- **WHEN** `match_bar` 评估
- **THEN** 返 `Unknown` (per Change B 4 枚举 `Unknown` verdict)
- **AND** DGpuBoard mmio_read/write 走 fallback 路径 (per Change C 6 if 分支简化为 4 if 分支)

---

### Requirement: resizable-bar-config-sync (v1.1 修复 64-bit 编码)

Resize 完成后, config space BAR register (offset 0x10 + N*4) SHALL 同步更新, **4GB 64-bit BAR 编码正确** (v1.1 修 v1.0 编码错误).

#### Scenario: config space BAR register 同步 (per AC G4, v1.1 修复)

- **WHEN** resize Complete (`commit(2, 0x100000000ULL)` 后)
- **THEN** config space 0x18 (BAR2 register) **low dword = 0** (per PCIe Base Spec §7.8.6)
- **AND** config space 0x1C (BAR3 upper dword, 64-bit BAR pair) **high dword = 1** (0x100000000 >> 32)
- **AND** `host 读 config space BAR register` 与 `pcie_ep()->bar_size(N)` 返回值一致
- **AND** 4GB = 0x100000000 正确编码 (low dword=0, high dword=1, 64-bit addressing)
- **NOTE**: **v1.1 修 v1.0 编码错误** (per CRITICAL C3: v1.0 spec "low dword = 4GB encoding, high dword = 0" 自相矛盾，4GB 需要 33 bits, 32-bit addressing 无法表达)

#### Scenario: Resizable BAR Capability register (per PCIe spec §7.8)

- **WHEN** resize 完成后, host 读 Resizable BAR Extended Capability (PCIe capability ID = 0015)
- **THEN** 返回 `ResizableBar::state_` + `ResizableBar::size_bytes_` (uint64)
- **AND** 字段定义遵循 PCIe spec §7.8 (BAR Index + BAR Size + 1MB granularity)

---

