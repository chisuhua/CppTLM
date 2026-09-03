# Spec: cdna-isa-abstraction

> **Capability**: cdna-isa-abstraction
> **Source change**: [`cpptlm-dgpu-d1-cdna-isa-phase-a`](../)
> **Parent ADR**: [`ADR-SOC-15-cdna-real-isa-roadmap.md`](../../../../docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md)
> **Parent design**: [`docs/soc_arch/architecture/11-cdna-real-isa-integration.md`](../../../../docs/soc_arch/architecture/11-cdna-real-isa-integration.md)
> **Created**: 2027-02-09
> **Status**: Proposed（阶段 A 启动）

---

## ADDED Requirements

### Requirement: cpptlm-instruction-descriptor

The system MUST provide a `cpptlm::gpu::InstrDescriptor` POD struct in `include/tlm/gpu/instruction_descriptor.hh` containing:
- `PipeClass pipe` (1 byte): enum value in {kScalarALU, kVectorALU, kMatrixCore, kBranch, kLSU_Global, kLSU_LDS, kSpecialSync}
- `LatencyClass latency_class` (1 byte): enum value in {kFixed1Cycle, kFixed4Cycles, kFixed8Cycles, kFixed16Cycles, kMatrixHeavy, kMemoryVariable}
- `CtrlBits ctrl` (6 bytes): containing `vmcnt_req`, `lgkmcnt_req`, `expcnt_req`, `wait_vmcnt`, `wait_lgkmcnt`, `wait_expcnt` fields
- `uint16_t dst_regs[4]` + `uint16_t src_regs[4]` (16 bytes total)
- `uint8_t num_dst` + `uint8_t num_src` + `bool is_memory` + `uint64_t target_vaddr` + `uint32_t mem_size` (memory op metadata)

The struct MUST be `std::is_trivially_copyable` (POD), `sizeof(InstrDescriptor) <= 64 bytes`, and support `std::hash<InstrDescriptor>` for hashing in unordered containers. The struct MUST NOT contain any platform-specific (PTX/SASS/CDNA) text fields in the **main path**; original PTX instruction string may be optionally carried in a debug-only sibling structure (NOT in this struct).

#### Scenario: cross-ISA pipe class mapping
- **WHEN** a PTX `fma.rn.f32` is decoded
- **THEN** `InstrDescriptor.pipe` is set to `kVectorALU`
- **AND** `InstrDescriptor.latency_class` is set to `kFixed16Cycles` (PTX 阶段 A 映射；CDNA 阶段 C 改用 `kMatrixHeavy`)
- **AND** no platform-specific text fields are present in the struct body

#### Scenario: CDNA MFMA descriptor readiness
- **WHEN** a CDNA `v_mfma_f32_16x16x16_fp16` is decoded (阶段 C 启用)
- **THEN** `InstrDescriptor.pipe` is set to `kMatrixCore`
- **AND** `InstrDescriptor.latency_class` is set to `kMatrixHeavy`
- **AND** `InstrDescriptor.ctrl.exp_cnt_req = 0` (no export for MFMA)

#### Scenario: memory operation metadata
- **WHEN** a PTX `ld.global.f32 [addr]` is decoded
- **THEN** `InstrDescriptor.is_memory` is `true`
- **AND** `InstrDescriptor.target_vaddr` equals `addr`
- **AND** `InstrDescriptor.mem_size` equals 4 (32-bit load)
- **AND** `InstrDescriptor.pipe` is `kLSU_Global`
- **AND** `InstrDescriptor.latency_class` is `kMemoryVariable` (placeholder; 阶段 B 实际延迟由 `IMemoryPort` 异步测量)

#### Scenario: POD serialization
- **WHEN** a caller serializes `InstrDescriptor` via `std::memcpy` or `std::bit_cast`
- **THEN** the bytes are stable across platforms (no padding holes, no pointer fields in main struct)

### Requirement: cpptlm-cdna-pipeline

The system MUST provide a `CdnaPipelineTLM` class in `include/tlm/gpu/cdna_pipeline_tlm.hh` + `src/tlm/gpu/cdna_pipeline_tlm.cc` implementing the same `IPipelineLatencyProvider` interface as the existing `PipelineTLM` class. The class MUST expose `double get_latency(LatencyClass lc) const` returning cycles based on a 6-entry lookup table:

| LatencyClass | Cycles |
|---|---|
| kFixed1Cycle | 1 |
| kFixed4Cycles | 4 |
| kFixed8Cycles | 8 |
| kFixed16Cycles | 16 |
| kMatrixHeavy | 32 |
| kMemoryVariable | -1 (deferred to `IMemoryPort`, 阶段 B) |

For backward compatibility, the class MUST also expose `double get_fractional_cycles(const std::string& instr, PipeClass pipe) const` that maps PTX string to `LatencyClass` then defers to `get_latency()`. This shim MUST produce **bit-identical** output to the existing `PipelineTLM::get_fractional_cycles()` for all 6 supported PTX patterns (`fma`→32, `mul`→16, `add`→4, `ld`→200, `st`→200, `bar`→1) when constructed in `Mode::kPtxCompat`.

#### Scenario: PTX 阶段 A bit-identical parity
- **WHEN** `CdnaPipelineTLM` (Mode::kPtxCompat) is constructed and `get_fractional_cycles("v_fma.f32", kVectorALU)` is called
- **THEN** it returns 32.0 (same as `PipelineTLM::get_fractional_cycles()`)
- **AND** `get_fractional_cycles("ld.global.f32", kLSU_Global)` returns 200.0
- **AND** all 6 PTX patterns return identical values to legacy `PipelineTLM`

#### Scenario: CDNA 阶段 C ready (kCdnaStrict mode)
- **WHEN** `CdnaPipelineTLM` is constructed in `Mode::kCdnaStrict` (阶段 C 启用)
- **THEN** `get_latency(kMemoryVariable)` returns -1 (signals: defer to IMemoryPort)
- **AND** `get_latency(kMatrixHeavy)` returns 32 (CDNA MFMA throughput baseline)
- **AND** `get_fractional_cycles()` shim throws `std::logic_error` (CDNA 路径不接受 PTX 字符串)

### Requirement: cpptlm-hazard-tracker-v2

The system MUST provide an `IHazardTracker` abstract interface in `include/tlm/gpu/hazard_tracker_interface.hh` declaring:
- `virtual void tick() = 0`
- `virtual bool is_stalled(uint32_t sm_id, uint32_t wave_id) const = 0`
- `virtual void notify_instruction_issued(const InstrDescriptor& instr, uint32_t sm_id, uint32_t wave_id) = 0`
- `virtual void notify_instruction_completed(const InstrDescriptor& instr, uint32_t sm_id, uint32_t wave_id) = 0`
- `virtual void reset() = 0`

The system MUST provide a `ScoreboardTLMv2` class in `include/tlm/gpu/scoreboard_tlm_v2.hh` + `src/tlm/gpu/scoreboard_tlm_v2.cc` implementing `IHazardTracker` with two modes selected at construction:
- `Mode::kVirtualReg`: delegates to existing `ScoreboardTLM` (CAPACITY=2048, `(reg_id, warp_id)` hash map)
- `Mode::kHardwareCounter`: provides `vmcnt_[]`/`lgkmcnt_[]`/`expcnt_[]` per-(sm_id, wave_id) arrays; `notify_instruction_issued` increments counters based on `InstrDescriptor.ctrl.*_req` fields; `notify_instruction_completed` decrements; `is_stalled` returns true if any `wait_*cnt` in `instr.ctrl` exceeds current counter value

The `Mode::kHardwareCounter` implementation MUST be marked `[[deprecated("stage C only")]]` in 阶段 A but compile and pass basic unit tests (counters track values correctly).

#### Scenario: kVirtualReg backward compat
- **WHEN** `ScoreboardTLMv2` is constructed with `Mode::kVirtualReg` and `notify_instruction_issued` is called with `InstrDescriptor` that has `dst_regs[0]=5`
- **THEN** `is_stalled(sm_id, wave_id)` returns true for warps issuing to reg 5 until `notify_instruction_completed` is called

#### Scenario: kHardwareCounter 阶段 C readiness
- **WHEN** `ScoreboardTLMv2` is constructed with `Mode::kHardwareCounter` and `notify_instruction_issued` is called with `InstrDescriptor` that has `vmcnt_req=1` and `lgkmcnt_req=0`
- **THEN** `vmcnt[sm_id, wave_id]` is incremented by 1
- **AND** `lgkmcnt[sm_id, wave_id]` is unchanged
- **AND** if a subsequent `notify_instruction_issued` is called with `wait_vmcnt=0`, the wave is marked stalled until counter reaches 0

### Requirement: cpptlm-kernel-launch-v2-setters

The `KernelLaunchTLM` class in `include/tlm/gpu/kernel_launch_tlm.hh` MUST provide **additional** setters (without removing existing ones):
- `void set_scoreboard_v2(IHazardTracker* tracker)` — sets the v2 hazard tracker (preferred)
- `void set_pipeline_v2(CdnaPipelineTLM* pipeline)` — sets the CDNA-aware pipeline (preferred)

The `tick()` method MUST support 3 paths:
1. If `set_scoreboard_v2` and `set_pipeline_v2` are NOT called: use legacy `IScoreboard` + `IPipelineLatencyProvider` (PTX mode unchanged)
2. If both are called: use v2 paths (CDNA mode / PTX-with-v2 mode)
3. Hybrid (only one called): use v2 for the called one, legacy for the other

This MUST NOT break any existing `[pcie]/[axi]/[e2e]/[wave2]` tests that use only legacy setters.

#### Scenario: PTX mode unchanged
- **WHEN** only `set_scoreboard(IScoreboard*)` and `set_pipeline(IPipelineLatencyProvider*)` are called (legacy path)
- **THEN** `tick()` proceeds with legacy implementation
- **AND** no v2 methods are invoked

#### Scenario: CDNA mode (阶段 C)
- **WHEN** `set_scoreboard_v2(ScoreboardTLMv2*)` is called with `Mode::kHardwareCounter` and `set_pipeline_v2(CdnaPipelineTLM*)` is called with `Mode::kCdnaStrict`
- **THEN** `tick()` uses v2 paths for both
- **AND** `IMemoryPort` integration is enabled (阶段 B 完成时 `kMemoryVariable` 解析)

---

## MODIFIED Requirements

### Requirement: cpptlm-pipeline-ptx-shim

The existing `PipelineTLM` class in `include/tlm/gpu/pipeline_tlm.hh` + `src/tlm/gpu/pipeline_tlm.cc` MUST remain **unchanged** (no source modifications). It serves as the PTX 阶段 A backward compatibility shim. New code MUST use `CdnaPipelineTLM` instead.

#### Scenario: PipelineTLM legacy path
- **WHEN** existing tests use `PipelineTLM::get_fractional_cycles(instr, pipe)` directly
- **THEN** the function returns the same value as before 阶段 A (no behavior change)
- **AND** the function signature remains `double get_fractional_cycles(const std::string& instr, PipeClass pipe) const`

### Requirement: cpptlm-scoreboard-v1-shim

The existing `ScoreboardTLM` class in `include/tlm/gpu/scoreboard_tlm.hh` MUST remain unchanged. `ScoreboardTLMv2::kVirtualReg` mode delegates to it.

#### Scenario: ScoreboardTLM legacy path
- **WHEN** existing tests use `ScoreboardTLM::allocate(reg_id, warp_id)` directly
- **THEN** the function returns the same value as before 阶段 A
- **AND** the function signature remains `bool allocate(uint32_t reg_id, uint32_t warp_id)`

---

## REMOVED Requirements

无（阶段 A 不删除任何现有功能；保留 `PipelineTLM` 与 `ScoreboardTLM` 作为 PTX 阶段 A 兼容 shim）