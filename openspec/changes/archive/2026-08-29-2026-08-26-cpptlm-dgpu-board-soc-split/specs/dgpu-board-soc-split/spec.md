# Spec: DGpuBoard shell + DGpuSoc JSON 拓扑分离

> **Status**: Proposed — 2026-08-26
> **Scope**: cpptlm-dgpu-board-soc-split change
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D1/D4/D5/D6

---

## ADDED Requirements

### Requirement: dgpu-soc-json-topology

The system MUST provide a `DGpuSoc` class in `include/tlm/gpu/dgpu_soc.hh` + `src/tlm/gpu/dgpu_soc.cc` inheriting from `SimModule`, registered via `REGISTER_MODULE` (SimModule registry per the dual-registry rule), that instantiates ALL internal SOC components (PcieEndpointTLM, SdmaEngineTLM, CommandProcessorTLM, TmuDispatchProcessorTLM, SubmitQueueTLM, CompletionRingTLM, GPU compute clusters, memory) from nested JSON via its `internal_factory`, with ALL internal wiring declared in JSON `connections` and injected through StreamAdapters. No SOC-internal component may be constructed as a plain C++ value member driven by a hand-written `tick()` sequence.

#### Scenario: SOC instantiates from nested JSON with non-empty connections
- **WHEN** a board JSON declares a `DGpuSoc` module with inline `modules` and `connections`
- **THEN** `simulate_instantiate` MUST construct every declared component and resolve every declared connection (BS-G1)

#### Scenario: SOC boundary ports exposed
- **WHEN** the SOC JSON declares `outputs`/`inputs` (e.g., `irq`, `host_dma`, `host_tlp`)
- **THEN** the board shell MUST be able to resolve them via `findInternalPath` / internal instance lookup (BS-G1)

### Requirement: dgpu-board-cpp-shell

The system MUST provide a `DGpuBoard` C++ class in `include/tlm/gpu/dgpu_board_shell.hh` + `src/tlm/gpu/dgpu_board_shell.cc` that implements the ADR-088 board-facing contract as a pure shell. The shell MUST NOT inherit `ChStreamModuleBase`, MUST NOT hold any register state, and MUST limit itself to five responsibilities: (1) ABI translation of `mmio_read/write` and `pcie_config_read/write` into transactions injected into the SOC boundary ingress port; (2) device enumeration (dev_id → SOC JSON profile); (3) SOC assembly via `ModuleFactory`; (4) callback wiring (`irq_out` → interrupt callback, `host_out` → DMA-translate callback, error channel); (5) lifecycle forwarding.

#### Scenario: MMIO write reaches the SOC endpoint
- **WHEN** the shell receives `mmio_write(bar=0, offset=0x14, value)` 
- **THEN** a transaction MUST be constructed and injected into the SOC `host_tlp` ingress port, and the doorbell side effect MUST be observable through the SOC topology (not through shell-held state) (BS-G2)

#### Scenario: s2 monolith retired
- **WHEN** migration completes
- **THEN** `grep -rl "DGpuBoardTLM" include/ src/ test/ configs/` MUST return zero matches, the s2 registration MUST be removed, and the full test suite MUST pass with no regression (BS-G4)

### Requirement: dgpu-board-abi-contract

The shell C++ method signatures exposed for the 23 ABI C wrapper (per ADR-088 §D5, implemented in the subsequent `cpptlm-dgpu-abi-export` change) MUST be frozen at this revision: ABI function names, parameter types, and behavioral semantics MUST match the 23 ABI list in `include/abi/cpptlm_emulator.h` (delivered by T-bs-6 of this change). Any future change to the shell method signatures MUST update the header in lockstep.

#### Scenario: Shell method ↔ ABI mapping frozen
- **WHEN** the shell implements `mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len)` and `mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len)`
- **THEN** the subsequent `cpptlm-dgpu-abi-export` change MUST implement `cpptlm_emulator_mmio_read/write(void* emu, uint8_t bar, uint64_t offset, void* buf, uint32_t size)` calling these exact signatures, and the header freeze MUST match ADR-088 §D5 23-item list (per BS-G5 verification: name/signature/parameter-order diff = 0)

### Requirement: dgpu-pcie-perspective-parity

The PCIe device-perspective test suite MUST retain its six scenarios (BAR0 doorbell wake, BAR1 VRAM round-trip, IOCTL 0x27/0x28/0x29 lifecycle, PUSHBUFFER→doorbell→SQ progression, BAR1 bounds rejection, GPFIFO PUT non-doorbell no-op) while driving them through the shell→SOC port-injection path instead of direct monolith method calls.

#### Scenario: Test semantics preserved across migration
- **WHEN** `ctest -R "test_dgpu_pcie_device_perspective"` runs after migration
- **THEN** all 6 cases MUST pass with equivalent assertions (BS-G3)

### Requirement: dgpu-board-execution-model

The system MUST enforce the per-card execution model documented in `design.md §2.5`: each `DGpuBoard` MUST own a private `EventQueue*` (not thread-safe per `event_queue.hh:49`); host→sim transactions MUST be queued via a per-card `mutex + deque<PendingReq>` injection queue and drained by the sim thread at quantum boundaries (default `quantum_cycles = 1000` from JSON); `mmio_read` MUST block on a `std::promise/future` with a 1ms wall-clock timeout; sim→host callbacks (`irq_out`, `cpptlm_dma_translate_cb`, error channel) MUST be non-blocking and MUST NOT re-enter the 23 ABI surface; `destroy` MUST follow the order `stop_=true → poison pill → join sim_thread_ → destruct SOC → destruct EventQueue`; multi-card `StatsManager` paths MUST be prefixed with `device_id` to avoid singleton collisions; and `sim_loop` exceptions MUST be captured via `std::exception_ptr` and rethrown on the next ABI call (not silently swallowed).

#### Scenario: Multi-card per-thread isolation
- **WHEN** 2+ boards are created concurrently via `cpptlm_emulator_create_by_id`
- **THEN** each board MUST have its own independent `EventQueue` + `inject_q` + sim_thread, and per-card `StatsManager` paths MUST NOT collide (BS-G2)
- **AND** the SOC assembly MUST NOT redefine `bundles::CompletionBundle` — that type is owned by the `cpptlm-dgpu-sdma-engine` change and is reused by this change's components (CQ, SQ, TMU, all `done_in`/`done_out` ports).
