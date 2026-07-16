# Spec: cpptlm-d1-p1-pipeline-scoreboard

> **Status**: Proposed（Day 3 / Metis Round 4 完成后启动）
> **Scope**: P1 D1-Full Compute 注入
> **Parent**: `cpptlm-f12b-ld-impl` (P0, 归档后本 change 进入实施)
> **Cross-project**: `PTX-EMU/openspec/changes/cpptlm-phase8b-injection-points/`（接口上游）
> **Created**: 2026-07-16

---

## ADDED Requirements

### Requirement: cpptlm-scoreboard
The system MUST provide a `ScoreboardTLM` class in `include/tlm/gpu/scoreboard_tlm.hh` + `src/tlm/gpu/scoreboard_tlm.cc` inheriting from `IScoreboardInternal` (defined in `include/tlm/gpu/scoreboard_internal.hh`) implementing a hazard table with at least 12 entries. The class MUST provide:
- `bool has_free_entry() const`
- `bool allocate(uint32_t reg_id, uint32_t warp_id)`
- `bool release(uint32_t reg_id, uint32_t warp_id)`
- `void tick()`

#### Scenario: RAW hazard detection blocks new allocation
- **WHEN** a warp issues an instruction whose destination register is already in-flight
- **THEN** `allocate(reg_id, warp_id)` returns `false`
- **AND** the calling pipeline stage stalls until `release()` is called

---

### Requirement: cpptlm-pipeline
The system MUST provide a `PipelineTLM` class in `include/tlm/gpu/pipeline_tlm.hh` + `src/tlm/gpu/pipeline_tlm.cc` inheriting from `IPipelineLatencyInternal` (defined in `include/tlm/gpu/pipeline_internal.hh`) implementing fractional cycle latency queries. The class MUST provide:
- `double get_fractional_cycles(const std::string& instruction, PipelineId pipe) const`
- `double get_fractional_cycles_by_type(StatementType type, PipelineId pipe) const`

#### Scenario: FFMA latency returns canonical 4.0 cycles
- **WHEN** `get_fractional_cycles_by_type(S_FFMA, PipelineId::P0_INT_FP32)` is called
- **THEN** it returns `4.0`
- **AND** matches PTX-EMU `IPipelineLatencyProvider` default

---

### Requirement: cpptlm-tensorcore
The system MUST provide a `TensorCoreTLM` class in `include/tlm/gpu/tensor_core_tlm.hh` + `src/tlm/gpu/tensor_core_tlm.cc` inheriting from `ITensorCoreTimingInternal` (defined in `include/tlm/gpu/tensor_core_internal.hh`) supporting 6 precisions (FP4 / FP6 / FP8 / FP16 / BF16 / TF32). The class MUST provide:
- `uint32_t get_latency(TcPrecision prec) const`
- `uint32_t get_throughput_cycles(TcPrecision prec) const`
- `uint32_t get_latency_mnk(TcPrecision prec, uint32_t M, uint32_t N, uint32_t K) const`

#### Scenario: FP16 latency defaults to 4 cycles
- **WHEN** `get_latency(TcPrecision::FP16)` is called
- **THEN** it returns `4`
- **AND** matches PTX-EMU `ITensorCoreTiming` reference table

---

### Requirement: cpptlm-4-adapters
The system MUST provide four Adapter classes in `include/tlm/gpu/adapter/` that bridge CppTLM internal interfaces to PTX-EMU interfaces:
- `CppTLMWarpSchedulerAdapter` (optional, for SMContext)
- `CppTLMScoreboardAdapter : public IScoreboard` → forwards to `ScoreboardTLM`
- `CppTLMPipelineAdapter : public IPipelineLatencyProvider` → forwards to `PipelineTLM`
- `CppTLMTensorCoreAdapter : public ITensorCoreTiming` → forwards to `TensorCoreTLM`

Each Adapter MUST be constructible from a raw pointer to its CppTLM-side module (non-owning).

#### Scenario: Scoreboard adapter forwards allocate calls
- **WHEN** PTX-EMU calls `IScoreboard::allocate(reg_id, warp_id)` via `CppTLMScoreboardAdapter`
- **THEN** the call is delegated to `ScoreboardTLM::allocate(reg_id, warp_id)`
- **AND** the boolean return value is preserved
- **AND** nullptr CppTLM module → return false (fallback)

---

### Requirement: cpptlm-async-completion
The system MUST provide an `AsyncCompletionAdapter` class in `include/tlm/gpu/async_completion_adapter.hh` + `src/tlm/gpu/async_completion_adapter.cc` implementing `IAsyncCompletion`. The class MUST provide:
- `void register_completion_callback(uint64_t id, std::function<void()> cb)`
- `void fire_completion(uint64_t id)`

In Phase 8.B, `fire_completion()` MUST be a **placeholder** (no-op stub) that records the call but does not invoke the callback. Phase 9+ will replace with real invocation.

#### Scenario: Phase 8.B placeholder does not invoke callback
- **WHEN** `register_completion_callback(id, cb)` then `fire_completion(id)` is called
- **THEN** the callback is stored but NOT invoked
- **AND** `fire_completion_count_` increments for monitoring

---

## 12-endpoint static_assert

#### Scenario: PipelineId and TcPrecision enum values match PTX-EMU
- **WHEN** CppTLM compiles `test/test_12_endpoint_static_assert.cc`
- **THEN** all 6 `PipelineId` values (P0_INT_FP32, V_SIMD, P1_FP64, P2_SFU, P3_LSU, P4_TC) match PTX-EMU enum values (0..5)
- **AND** all 6 `TcPrecision` values (FP4, FP6, FP8, FP16, BF16, TF32) match PTX-EMU enum values (0..5)
- **AND** any mismatch causes a compile-time failure

```cpp
// PipelineId verification (CppTLM-side, mirrors PTX-EMU cpptlm-phase8b-injection-points)
static_assert(static_cast<int>(PipelineId::P0_INT_FP32) == 0);
static_assert(static_cast<int>(PipelineId::V_SIMD)      == 1);
static_assert(static_cast<int>(PipelineId::P1_FP64)     == 2);
static_assert(static_cast<int>(PipelineId::P2_SFU)      == 3);
static_assert(static_cast<int>(PipelineId::P3_LSU)      == 4);
static_assert(static_cast<int>(PipelineId::P4_TC)       == 5);

// TcPrecision verification
static_assert(static_cast<int>(TcPrecision::FP4)  == 0);
static_assert(static_cast<int>(TcPrecision::FP6)  == 1);
static_assert(static_cast<int>(TcPrecision::FP8)  == 2);
static_assert(static_cast<int>(TcPrecision::FP16) == 3);
static_assert(static_cast<int>(TcPrecision::BF16) == 4);
static_assert(static_cast<int>(TcPrecision::TF32) == 5);
```

---

## Verification Gates (P1)

> **来源**: tasks.md §G-D1~G-D8

- [ ] **G-D1** 3 pure interfaces compile and link without CppTLM headers polluting PTX-EMU
- [ ] **G-D2** `set_blocked_cycles_for_active()` correctly applies per-warp latency
- [ ] **G-D3** `blocked_cycles_remaining` differs from standalone by ≤ 1 cycle
- [ ] **G-D4** 12-endpoint static_assert passes on both sides
- [ ] **G-D5** 5 microbenchmarks stay within ±15% of gpgpu-sim baseline
- [ ] **G-D6** 4 setter all nullptr → PTX-EMU zero regression
- [ ] **G-D7** Scoreboard/Pipeline/TC any nullptr → fallback to InstructionLatencyTable
- [ ] **G-D8** `exe_once` stall → re-schedule → release → re-issue loop is consistent

---

## Cross-Project Counterparts

| Repo | Change | Role |
|------|--------|------|
| PTX-EMU | `openspec/changes/cpptlm-phase8b-injection-points/` | Upstream injection interfaces (IScoreboard / IPipelineLatencyProvider / ITensorCoreTiming) + `exe_once()` hooks |
| PTX-EMU | `openspec/changes/cpptlm-d1-full/` | P0 MemoryBridge + clock-of-truth handshake |
| CppTLM | `openspec/changes/cpptlm-f12b-ld-impl/` | P0 MemoryBridge + KernelLaunchTLM extension (parent) |
