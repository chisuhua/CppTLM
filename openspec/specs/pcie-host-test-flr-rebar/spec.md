# pcie-host-test-flr-rebar Specification

## Purpose
TBD - created by archiving change cpptlm-pcie-host-test-flr-rebar. Update Purpose after archive.
## Requirements
### Requirement: pcie-host-test-infrastructure

`HostPcieTestStub` SHALL be a new class in `include/tlm/pcie/host_pcie_test_stub.hh` providing in-process host-side PCIe TLP traffic generation + error injection + state trigger, **不依赖 UsrLinuxEmu 跨仓 driver** (per 用户决策 2026-10-09).

**v1.1 统一构造签名**:
```cpp
HostPcieTestStub(DGpuBoard* board, const std::string& config_path);
```

#### Scenario: 统一构造 + DGpuBoard 句柄注入 (v1.1 修复 v1.0 三处不一致)

- **GIVEN** 已 init 的 `DGpuBoard* board` (per `cpptlm_emulator_create("configs/dgpu_*.json")`)
- **WHEN** `HostPcieTestStub stub(board, "configs/dgpu_soc_v2_realip_p2.json")` 实例化（v1.1 统一签名）
- **THEN** `stub.serializer_` 私有成员就绪
- **AND** `stub.board_` 私有指针 = injected board
- **AND** `stub.config_path_` = `"configs/dgpu_soc_v2_realip_p2.json"`

#### Scenario: TLP 序列化 round-trip (per PcieTlpSerializer)

- **GIVEN** `PcieTlpSerializer::encode_config_read(bdf=0x100, offset=0x10, width=4)` 调
- **THEN** 返回 `PcieTlpBundle` 含 `kind=CFG_READ, bar_index=0, offset=0x10, size=4, data=0`
- **AND** `PcieTlpSerializer::decode(encoded_tlp)` 返回 `DecodedTlp` 字段 round-trip 一致

#### Scenario: 错误注入点 (per PcieTlpSerializer ErrorType)

- **GIVEN** `HostPcieTestStub stub(board, config)`
- **WHEN** `stub.inject_error(ErrorType::POISONED_TLP, ErrorParams{offset=0x100})` 调
- **THEN** 内部错误队列 + 1
- **AND** 下一 `send_*` 触发带毒 TLP

---

### Requirement: pcie-flr (v1.1 复用既有 + 修 spec 错误)

`PcieEndpointIP` SHALL **复用既有** `flr_pf()/flr_vf()` API (per `pcie_endpoint_ip.hh:201-202`) + `pool_.flr_pf()` 全状态复位 (per `pcie_sriov_vf_pool_tlm.hh:110`), **不新建 PcieFlrHandler 类** (per 双审查: 既有实现完整).

#### Scenario: FLR 4-state 状态转换 (v1.1 复用既有 `pool_.flr_pf()`)

- **GIVEN** `PcieEndpointIP ep;` (初始 state = D0 active)
- **WHEN** host 触发 `HostPcieTestStub::trigger_flr()` → `board->trigger_flr()` → `ep.flr_pf()` → `pool_.flr_pf()` 调
- **THEN** state 转换: Active → Resetting → Complete → Active (1 cycle, 0 wall-clock, per PCIe spec 100ms timeout)
- **AND** 17 端口 config space 寄存器 + per-VF MSI-X + FC + seq# + CompletionTracker 全部 reset

#### Scenario: PcieEndpointIP FLR TLP 入口 + state reset (v1.1 复用既有)

- **GIVEN** `PcieEndpointIP ep;` with `bar_store_` non-empty + `msix_pending_` non-empty
- **WHEN** `ep.flr_pf()` 调 (经既有 `trigger_flr()`)
- **THEN** `bar_store_` 全清 (4 BAR 寄存器重置)
- **AND** `MsiXTable::pba_bits_[N]` **全清** (per PCIe Base Spec §6.6.2, **v1.1 修 v1.0 spec 矛盾**)
- **AND** config space **Device Control Register offset 0x08 bit 15** = 1 during reset (per PCIe Base Spec §7.5.1.2, **v1.1 修 v1.0 offset 0x04 错误**)
- **AND** `ep.current_flr_state() = FLR_COMPLETE`

#### Scenario: HostPcieTestStub trigger_flr() 入口 (v1.1 改写: 直接调既有 C++ 入口)

- **GIVEN** `DGpuBoard* board` init via `cpptlm_emulator_create("configs/dgpu_soc_v2_realip_p2.json")`
- **WHEN** `HostPcieTestStub(board, config).trigger_flr()` 调 (per X1 G3)
- **THEN** `board->trigger_flr()` → `PcieEndpointIP::flr_pf()` → `pool_.flr_pf()` 自动调
- **AND** **0 调用 23 ABI** (per CRITICAL C2: ADR-DGPU-08 §2.3 0 新增约束)
- **AND** **不重建 DGpuBoard::eq_** (per Step 0 P0 SIGSEGV fix, commit `4d35faee`)
- **AND** **0 新增 ABI 头文件** (`find include/abi/cpptlm_emulator_flr.h` = 0 匹配)

#### Scenario: FLR 端到端 `[pcie][flr]` (per X1 G3)

- **GIVEN** 完整 dGPU SoC 仿真运行 (`board` init, 23 ABI 写入若干 BAR/状态)
- **WHEN** `HostPcieTestStub(board, config).trigger_flr()` 调 + 1 cycle tick
- **THEN** host 读 BAR0/BAR1/BAR2 寄存器全部归零 (per state reset)
- **AND** host 读 config space **Device Control Register offset 0x08** bit 15 = 1 during reset
- **AND** host 读 MSI-X PBA 表全部清零 (**v1.1 修 v1.0 spec 矛盾**)
- **AND** 4 internal `routing_enabled_*_` flag 0 触动 (per ADR-DGPU-10 + C.4 REVISED)

---

### Requirement: pcie-resizable-bar-runtime (v1.1 复用既有 + uint64)

`ResizableBarRuntime` SHALL 提供 Runtime resize 协议 **薄 wrapper** (per PCIe Base Spec §7.8.6), **复用既有** `enable_resizable_bar() + on_bar_resize()` API (per `pcie_endpoint_ip.cc:670-694`).

**v1.1 关键修改**: `ResizableBar::new_size_bytes_` 字段类型 `uint32_t` → `uint64_t` (per CRITICAL C3: 4GB 溢出).

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

#### Scenario: HostPcieTestStub trigger_resize_bar() 入口 (v1.1 改写: 直接调既有 C++ 入口)

- **GIVEN** `DGpuBoard* board` init via `cpptlm_emulator_create("configs/dgpu_soc_v2_realip_p2.json")`
- **WHEN** `HostPcieTestStub(board, config).trigger_resize_bar(2, 0x100000000ULL)` (uint64) 调 (per X1 G4)
- **THEN** `board->resize_bar(2, 0x100000000)` → `PcieEndpointIP::handle_resize_bar(2, 4GB)` → `ResizableBarRuntime::commit()` 自动调
- **AND** **0 调用 23 ABI** (per CRITICAL C2: ADR-DGPU-08 §2.3 0 新增约束)
- **AND** config space 0x18 寄存器重写（64-bit encoding: low=0, high=1）
- **AND** **0 新增 ABI 头文件** (`find include/abi/cpptlm_emulator_resizable_bar.h` = 0 匹配)

#### Scenario: Resizable BAR 端到端 `[pcie][rebar]` (per X1 G4)

- **GIVEN** 完整 dGPU SoC 仿真运行 (BAR2 = 8GB default)
- **WHEN** `HostPcieTestStub(board, config).trigger_resize_bar(2, 0x100000000ULL)` 调
- **THEN** host 读 config space 0x18 看到 low dword=0 (4GB encoding low half)
- **AND** host 读 config space 0x1C 看到 high dword=1 (4GB encoding high half, 64-bit BAR pair)
- **AND** host 写 BAR2 offset 0x10000000 (256MB) 命中 VRAM (4GB 范围内)
- **AND** host 写 BAR2 offset 0x20000000 (512MB) 命中 VRAM (4GB 范围内)
- **AND** host 写 BAR2 offset 0x40000000 (1GB) 命中 VRAM (4GB 范围内)
- **AND** host 写 BAR2 offset 0x200000000 (8GB) 触发 out-of-range (4GB 范围外)
- **AND** DGpuBoard routing verdict 与 new_size 一致 (per Change C Strangler gate)
- **AND** 4 internal `routing_enabled_*_` flag 0 触动 (per ADR-DGPU-10 + C.4 REVISED)

---

