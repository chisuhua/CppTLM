# Spec: UsrLinuxEmuIoctlStub (s2 IOCTL bridge)

> **Status**: Proposed — 2026-08-21
> **Scope**: s2 change — 4 IOCTL stub for UsrLinuxEmu ↔ CppTLM bridge
> **关联**: Phase F-H.3

---

## ADDED Requirements

### Requirement: usrlxemu-ioctl-stub-4-codes

The system MUST provide a `UsrLinuxEmuIoctlStub` class in `include/tlm/gpu/usrlxemu_ioctl_stub_mvp.hh` + `src/tlm/gpu/usrlxemu_ioctl_stub_mvp.cc` implementing exactly **4 IOCTL codes**:
- `0x27` (`GPU_IOCTL_LOAD_KERNEL_MODULE`) — load kernel image
- `0x28` — return `-ENOSYS` (not implemented stub)
- `0x29` (`GPU_IOCTL_UNLOAD_KERNEL_MODULE`) — unload kernel
- `AND** `0x01` (`GPU_IOCTL_PUSHBUFFER`) — submit push buffer

#### Scenario: LOAD_KERNEL_MODULE (0x27) loads image bytes via H2D DMA
- **WHEN** `ioctl(fd, 0x27, image_ptr)` is called
- **THEN** it MUST write `image_bytes` to `DGpuBar.vram_base()` via H2D DMA path
- **AND** return 0 on success

#### Scenario: 0x28 returns ENOSYS
- **WHEN** `ioctl(fd, 0x28, ...)` is called
- **THEN** it MUST return `-ENOSYS` (indicating not-yet-implemented)

#### Scenario: UNLOAD_KERNEL_MODULE (0x29) frees kernel handle
- **WHEN** `ioctl(fd, 0x29, handle)` is called for a previously-loaded kernel
- **THEN** it MUST free the kernel handle and return 0

#### Scenario: PUSHBUFFER (0x01) submits WDUs
- **WHEN** `ioctl(fd, 0x01, push_buffer_ptr)` is called
- **THEN** it MUST enqueue WDUs to `SubmitQueue`

### Requirement: ioctl-stub-e2e-test

The system MUST provide `test/test_usrlxemu_ioctl_stub.cc` covering all 4 IOCTL codes with end-to-end verification (s2-G4 acceptance: `ctest -R "test_usrlxemu_ioctl_stub"` PASS).

#### Scenario: All 4 IOCTL codes covered
- **WHEN** `ctest -R "test_usrlxemu_ioctl_stub"` runs after s2 W4
- **THEN** it MUST PASS covering 0x27 LOAD / 0x28 -ENOSYS / 0x29 UNLOAD / 0x01 PUSHBUFFER (s2-G4 acceptance)