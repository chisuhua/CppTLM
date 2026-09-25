# minimal-dgpu-soc-abi-landing Specification

## Purpose
TBD - created by archiving change 2027-09-17-cpptlm-minimal-dgpu-soc-v1-landing. Update Purpose after archive.
## Requirements
### Requirement: minimal-dgpu-soc-abi-driver-closed-loop

UE 端 driver 通过 23 ABI SHALL 闭环驱动 minimal dGPU SoC 全能力 (P0.5-landing 新增).

#### Scenario: UE driver 8 步 ABI 闭环

- **WHEN** UE driver (ctypes/dlopen) 加载 `libcpptlm_emulator.so`
- **AND** `cpptlm_emulator_create("configs/dgpu_soc_minimal_v1.json")` (P0.5-landing auto-allocate)
- **AND** `cpptlm_emulator_get_device_info(dev_id, &info)` 验证 `info.bar_sizes[1]==16777216`
- **AND** `cpptlm_emulator_mmio_write(emu, 1, off, &magic, 4)` (BAR1 framebuffer)
- **AND** `cpptlm_emulator_mmio_read(emu, 1, off, &back, 4)` 返 0 + `back==magic`
- **AND** `cpptlm_emulator_mmio_write(emu, 0, 0, &pt_lo, 4)` (BAR0 GMMU PT_BASE)
- **AND** `cpptlm_emulator_mmio_write(emu, 1, 0x10010000, &wptr, 4)` (doorbell)
- **AND** `cpptlm_emulator_msix_init + register_callbacks` 接通 (接受 -ENOSYS deferred)
- **AND** `cpptlm_emulator_register_dma_translate_cb(identity)` 接通
- **AND** `cpptlm_emulator_destroy(emu)` 接通
- **THEN** 8/15 forward ABI 函数验证完整闭环, 0 依赖测试 harness 兜底

#### Scenario: dlopen-based minimal SoC unit test (sibling)

- **WHEN** `examples/test_cpptlm_emulator_dlopen/test_dlopen_minimal_soc.cc` 运行
- **AND** `dlopen("libcpptlm_emulator.so", RTLD_LAZY)`
- **AND** `dlsym` 4 个 ABI (`create/mmio_write/mmio_read/destroy`)
- **AND** `create("configs/dgpu_soc_minimal_v1.json")` 显式传路径
- **AND** BAR1 写 0xCAFEBABE + 读回 0xCAFEBABE
- **THEN** ctest PASS, 输出 `v1.0-dgpu-v0` + `minimal_soc BAR1 round-trip OK (0xCAFEBABE)`

### Requirement: minimal-dgpu-soc-framebuffer-size-cap

`DGpuBoard::load_soc_config` SHALL 拒绝 `framebuffer_size_bytes > 64GB` 配置以防 `std::vector::resize` 触发 `bad_alloc`.

#### Scenario: 64GB cap 校验 (G9 acceptance)

- **WHEN** JSON 顶层 `framebuffer_size_bytes=1099511627776` (= 1TB, > 64GB)
- **THEN** `load_soc_config` 返 false + `runtime_error` exception 设置到 `last_exception_`
- **AND** `cpptlm_emulator_create` 返 nullptr (而非异常逃逸)

#### Scenario: 64GB cap 边界 (G9 boundary)

- **WHEN** `framebuffer_size_bytes=68719476736` (= 64GB exactly)
- **THEN** `load_soc_config` 成功 (边界 inclusive)
- **AND** `framebuffer_size_=68719476736`

#### Scenario: 64GB cap 超出 (G9 over-boundary)

- **WHEN** `framebuffer_size_bytes=68719476737` (= 64GB + 1 byte)
- **THEN** `load_soc_config` 返 false + `runtime_error` exception

### Requirement: minimal-dgpu-soc-override-consistency-warning

`DGpuBoard::load_soc_config` SHALL 在 `framebuffer_size_bytes` 与 `bar_sizes[1]` 不一致时发出 DPRINTF warning.

#### Scenario: override > derived (G10 acceptance)

- **WHEN** JSON `framebuffer_size_bytes=32MB` 但 `bar_sizes[1]=16MB`
- **AND** 编译时启用 `-DDEBUG_PRINT`
- **THEN** DPRINTF 输出: `[DGpuBoard] WARN: framebuffer_size_bytes=33554432 != bar_sizes[1]=16777216; BAR1 window and backdoor bounds may diverge.`
- **AND** `framebuffer_size_=32MB` (override 胜出, 但 warning 已记录)

#### Scenario: override < derived (G10 acceptance)

- **WHEN** JSON `framebuffer_size_bytes=8MB` 但 `bar_sizes[1]=16MB`
- **AND** `-DDEBUG_PRINT` 启用
- **THEN** 同上 warning 输出
- **AND** `framebuffer_size_=8MB` (override 胜出, 但 BAR1 cfg space window 仍 16MB, backdoor 仅 8MB → BAR cfg 与 backdoor 语义分裂)

#### Scenario: override == derived (silent accept)

- **WHEN** JSON `framebuffer_size_bytes=16MB` == `bar_sizes[1]=16MB`
- **THEN** 无 warning 输出 (语义一致)
- **AND** `framebuffer_size_=16MB`

