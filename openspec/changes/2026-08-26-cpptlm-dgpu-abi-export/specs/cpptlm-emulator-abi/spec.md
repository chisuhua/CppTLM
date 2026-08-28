# Spec: cpptlm_emulator ABI（C 包装层暴露 23 ABI + 多设备注册表）

> **Status**: Proposed — 2026-08-26
> **Scope**: cpptlm-dgpu-abi-export change
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D5 + Status Update Q2/Q6 · UsrLinuxEmu ADR-088 §D5

---

## ADDED Requirements

### Requirement: cpptlm-emulator-shared-library

The system MUST build a new SHARED library target `cpptlm_emulator` in `src/abi/cpptlm_emulator.cc` that exposes the 23 ABI declared in `include/abi/cpptlm_emulator.h` (per ADR-088 §D5; header frozen by change `cpptlm-dgpu-board-soc-split` T-bs-6). The library MUST compile with `-fvisibility=hidden` and MUST explicitly mark each of the 23 ABI symbols with `CPPTLM_EMULATOR_EXPORT` (defined as `__attribute__((visibility("default")))` on GCC/Clang). The static library `cpptlm_core` MUST be compiled with `POSITION_INDEPENDENT_CODE ON` so it can be linked into `libcpptlm_emulator.so` without breaking PTX-EMU's `ExternalProject_Add` static-consumption path.

#### Scenario: ABI symbols exported from SHARED library
- **WHEN** `cmake --build build` finishes
- **THEN** `nm -D build/lib/libcpptlm_emulator.so | grep cpptlm_emulator_` MUST list all 16 forward function symbols (`get_version`, `create`, `mmio_read`, `mmio_write`, `lookup_register`, `destroy`, `get_device_count`, `get_device_info`, `create_by_id`, `pcie_config_read`, `pcie_config_write`, `backdoor_read`, `backdoor_write`, `msix_init`, `msix_update_pending`, `msix_clear_pending`) plus 3 callback-register symbols (`register_callbacks`, `register_backdoor_cb`, `register_dma_translate_cb`) (AE-G2)

#### Scenario: PTX-EMU static-consumption path unaffected
- **WHEN** `cpptlm_core` PIC is enabled
- **THEN** `external/PTX-EMU` ExternalProject_Add consumption of `libcpptlm_core.a` MUST continue to work without requiring `cpptlm_emulator` (PTX-EMU never links the SHARED layer) (R1 mitigation verified)

### Requirement: cpptlm-emulator-device-registry

The library MUST maintain a global `std::unordered_map<uint32_t, DGpuBoard*>` protected by `std::mutex registry_mu_` for all device-handle-to-board-pointer lookups. `cpptlm_emulator_create_by_id` and `cpptlm_emulator_create` MUST insert into this map; `cpptlm_emulator_destroy` MUST erase from the map under the lock but MUST release the lock before invoking `~DGpuBoard` (which joins the simulation thread and may take time). Multi-device concurrent access MUST be safe (mutex-protected).

#### Scenario: Multi-device registry under concurrent create/destroy
- **WHEN** 2+ threads concurrently call `cpptlm_emulator_create_by_id` and `cpptlm_emulator_destroy` on overlapping and disjoint dev_id values
- **THEN** no race condition occurs (TSan clean), destroy-during-API-call returns `-1`, and each board's simulation thread terminates cleanly (AE-G4)

### Requirement: cpptlm-emulator-end-to-end

Each of the 23 ABI functions MUST behave identically to the corresponding `DGpuBoard` shell C++ method called with the same arguments (per the frozen `dgpu-board-abi-contract` scenario in change `cpptlm-dgpu-board-soc-split` spec). Each `extern "C"` function body MUST wrap its call site in `try { ... } catch (const std::exception&) { return -EINVAL; } catch (...) { return -EFAULT; }` to prevent C++ exceptions from crossing the C ABI boundary (UB).

#### Scenario: ABI function mirrors shell behavior
- **WHEN** `cpptlm_emulator_mmio_write(dev_id, bar=0, offset=0x14, &val, 4)` is called
- **THEN** it MUST produce a doorbell side effect observable through the SOC topology (per `pcie-endpoint-component` scenario "Doorbell register write is table-driven"), and the response MUST arrive within 1ms wall-clock (per change C `dgpu-board-cpp-shell` thread model OR this change's own timing budget — see note below). NOTE: the 1ms upper bound is the timing budget adopted by this change for synchronous mmio_write response; it corresponds to the quantum boundary service interval of DGpuBoard::sim_loop (per board-soc-split design §2.5) (AE-G3)

#### Scenario: Exception safety at C boundary
- **WHEN** a shell method throws a C++ exception
- **THEN** the ABI function MUST return a negative errno (-EINVAL for `std::invalid_argument`/`std::out_of_range`, -EFAULT for any other exception) and MUST NOT propagate the exception across the C ABI boundary
