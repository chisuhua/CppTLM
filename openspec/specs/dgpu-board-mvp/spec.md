# dgpu-board-mvp Specification

## Purpose
TBD - created by archiving change 2026-08-21-cpptlm-v05-mvp-s2-dgpu-board. Update Purpose after archive.
## Requirements
### Requirement: dgpu-board-8-component-wrapper

The system MUST provide a `DGpuBoardTLM` class in `include/tlm/gpu/dgpu_board_mvp.hh` + `src/tlm/gpu/dgpu_board_mvp.cc` inheriting from `ChStreamModuleBase` that wraps **8 components** as a single ChStream module (per Phase F-H.2 of design.md). The 8 components MUST be:
1. `DGpuBar` (BAR0 MMIO + BAR1 VRAM, vendored from `dgpu_bar.hh` v0.4)
2. `Doorbell` (SQ tail register + strong-order write)
3. `SubmitQueue` (WDU distribution network, single-SM MVP)
4. `CompletionRing` (push + host_notify hook)
5. `UsrLinuxEmuIoctlStub` (4 IOCTLs: 0x27 LOAD / 0x28 -ENOSYS / 0x29 UNLOAD / 0x01 PUSHBUFFER)
6. `CommandProcessor` (skeleton with 5-state FSM, `set_decoder` injection point for s3)
7. `TmuDispatchProcessor` (skeleton with `set_handler` injection point for s3)
8. `Pm4Decoder` (forward declaration via `Pm4DecoderInterface`, s3 fills)

The wrapper MUST expose `ChStreamPort` interfaces consistent with other CppTLM modules and MUST be registered via `REGISTER_CHSTREAM(DGpuBoardTLM)` in `include/chstream_register.hh`.

#### Scenario: Wrapper compiles and instantiates from JSON config
- **WHEN** `configs/dgpu_board_v1_mvp.json.in` is configured via CMake `configure_file` (injecting `${PTX_EMU_ROOT}`)
- **AND** `cmake --build build` is invoked
- **THEN** `DGpuBoardTLM` MUST instantiate all 8 sub-components without linker errors (s2-G1 acceptance)

#### Scenario: E2E from config 6 SECTION test (s2 W4 降级为 5 SECTION, item 4 标 ⏳)
- **WHEN** `ctest -R "test_dgpu_board_v1_mvp_from_config"` runs
- **THEN** 5 SECTION (item 4 deferred) MUST PASS (s2-G5 acceptance)

### Requirement: dgpu-board-chstream-registration

The system MUST register `DGpuBoardTLM` and `UsrLinuxEmuIoctlStub` via `REGISTER_CHSTREAM` macro in `include/chstream_register.hh` so that `ModuleFactory::instantiateAll` can construct them from JSON config.

#### Scenario: ModuleFactory creates DGpuBoardTLM
- **WHEN** JSON config contains `{"name": "dgpu_board_0", "type": "DGpuBoardTLM"}`
- **THEN** `ModuleFactory::instantiateAll` MUST succeed and return a non-null `SimObject*`

