# Spec: cpptlm-d1-p1-pipeline-scoreboard

> **Status**: Proposed（Day 3 / Metis Round 4 完成后启动）
> **Scope**: P1 D1-Full Compute 注入
> **Parent**: `cpptlm-f12b-ld-impl` (P0, 归档后本 change 进入实施)
> **Cross-project**: `PTX-EMU/openspec/changes/cpptlm-phase8b-injection-points/`（接口上游）
> **Created**: 2026-07-16
> **Last revised**: 2026-07-18（Metis + Oracle 双审后修复 7 项 P0；详见 design.md §0 Design Revision）

---

## ADDED Requirements

### Requirement: cpptlm-scoreboard
The system MUST provide a `ScoreboardTLM` class in `include/tlm/gpu/scoreboard_tlm.hh` + `src/tlm/gpu/scoreboard_tlm.cc` inheriting **directly** from `IScoreboard` (vendored from PTX-EMU `8acfd2d1` in `include/cudart/scoreboard_interface.h`) implementing a **global** hazard table (shared across all warps in an SM, indexed by `(reg_id, warp_id)` tuple) with **at least 512 entries** (covers 64 warps/SM × 8 in-flight instructions; see design.md §2.1 for容量依据). The class MUST provide:
- `bool has_free_entry() const`
- `bool allocate(uint32_t reg_id, uint32_t warp_id)` — rejects duplicate `(reg_id, warp_id)` allocation (returns `false`); see design.md §2.1 for决议依据
- `bool release(uint32_t reg_id, uint32_t warp_id)`
- `void tick()` — P1 no-op (see design.md §2.1 for死锁缓解说明)

#### Scenario: RAW hazard detection blocks new allocation
- **WHEN** a warp issues an instruction whose destination register is already in-flight (same `(reg_id, warp_id)` already active)
- **THEN** `allocate(reg_id, warp_id)` returns `false`
- **AND** the calling pipeline stage stalls until `release()` is called

---

### Requirement: cpptlm-pipeline
The system MUST provide a `PipelineTLM` class in `include/tlm/gpu/pipeline_tlm.hh` + `src/tlm/gpu/pipeline_tlm.cc` inheriting **directly** from `IPipelineLatencyProvider` (vendored from PTX-EMU `9e7361b9` in `include/cudart/pipeline_interface.h`) implementing fractional cycle latency queries. The class MUST provide:
- `double get_fractional_cycles(const std::string& instruction, PipelineId pipe) const`
- `double get_fractional_cycles_by_type(int statement_type, PipelineId pipe) const`

> **Phase 1 placeholder**: All queries return `1.0` (NOT canonical values; see design.md §2.2). Phase 4 will replace with gpgpu-sim aligned values (G-D5, ±15%).

#### Scenario: Phase 1 placeholder returns 1.0
- **WHEN** `get_fractional_cycles_by_type(S_FFMA, PipelineId::P0_INT_FP32)` is called
- **THEN** it returns `1.0` (**Phase 1 placeholder**, NOT canonical 4.0)
- **AND** `is_placeholder() const` returns `true` (Phase 4 替换为真实值后返回 `false`)

---

### Requirement: cpptlm-tensorcore
The system MUST provide a `TensorCoreTLM` class in `include/tlm/gpu/tensor_core_tlm.hh` + `src/tlm/gpu/tensor_core_tlm.cc` inheriting **directly** from `ITensorCoreTiming` (vendored from PTX-EMU `463038e0` in `include/cudart/tensor_core_interface.h`) supporting 6 precisions (FP4 / FP6 / FP8 / FP16 / BF16 / TF32). The class MUST provide:
- `uint32_t get_latency(TcPrecision prec) const`
- `uint32_t get_throughput_cycles(TcPrecision prec) const`
- `uint32_t get_latency_mnk(TcPrecision prec, uint32_t M, uint32_t N, uint32_t K) const` — 不 override，使用 PTX-EMU 头文件 default impl 退化到 `get_latency(prec)`

> **Phase 1 placeholder**: `get_latency`/`get_throughput_cycles` return `1` (NOT canonical values; see design.md §2.3). Phase 4 will replace with gpgpu-sim aligned values (G-D5, ±15%).

#### Scenario: Phase 1 placeholder returns 1
- **WHEN** `get_latency(TcPrecision::FP16)` is called
- **THEN** it returns `1` (**Phase 1 placeholder**, NOT canonical 4)
- **AND** `is_placeholder() const` returns `true` (Phase 4 替换为真实值后返回 `false`)

---

### Requirement: cpptlm-12-endpoint-static-assert

The system MUST provide `test/test_12_endpoint_static_assert.cc` that performs **编译期** `static_assert` on all 12 enum endpoints (6 `PipelineId` + 6 `TcPrecision`) using the vendored PTX-EMU enum definitions as truth source.

Additionally, to guard against silent ABI drift when PTX-EMU changes method signatures (not just enum values), the file SHOULD include **signature-level** `static_assert`s using `decltype` on each pure virtual method (see design.md §4 for示例).

#### Scenario: PipelineId and TcPrecision enum values match PTX-EMU
- **WHEN** CppTLM compiles `test/test_12_endpoint_static_assert.cc`
- **THEN** all 6 `PipelineId` values (P0_INT_FP32, V_SIMD, P1_FP64, P2_SFU, P3_LSU, P4_TC) match PTX-EMU enum values (0..5)
- **AND** all 6 `TcPrecision` values (FP4, FP6, FP8, FP16, BF16, TF32) match PTX-EMU enum values (0..5)
- **AND** any mismatch causes a compile-time failure

---

### Requirement: cpptlm-async-completion
The system MUST provide an `AsyncCompletionAdapter` class in `include/tlm/gpu/async_completion_adapter.hh` (header-only, no `.cc`) implementing `IAsyncCompletion`. The class MUST provide:
- `void register_completion_callback(uint64_t id, std::function<void()> cb)`
- `void fire_completion(uint64_t id)`

In Phase 8.B, `fire_completion()` MUST be a **placeholder** (no-op stub) that records the call but does not invoke the callback. Phase 9+ will replace with real invocation.

> **Status**: ✅ 已落地于 commit `e69cd1d`（97 行 header-only, 5 单测全 PASS）。

#### Scenario: Phase 8.B placeholder does not invoke callback
- **WHEN** `register_completion_callback(id, cb)` then `fire_completion(id)` is called
- **THEN** the callback is stored but NOT invoked
- **AND** `fire_completion_count_` increments for monitoring

> **Architecture note**: `IAsyncCompletion` 是 CppTLM 自定义接口（非 PTX-EMU vendor）。PTX-EMU 端 `sm_context.h` 是否需要 `set_async_completion()` setter 待 Phase 9+ 确认（见 design.md §5）。当前为投机性脚手架。

---

## Verification Gates (P1)

> **来源**: tasks.md §G-D1~G-D8
>
> **验收归属说明**（Metis + Oracle 双审后明确）:
> - **CppTLM 端可独立验证**: G-D1, G-D4
> - **PTX-EMU 端验收**（CppTLM 无法独立测试，因 nullptr 回退行为在 PTX-EMU `step_a/b/c` helper 中实现）: G-D6, G-D7
> - **双端联合验收**（需 PTX-7a/7b 完成）: G-D2, G-D3, G-D5, G-D8

- [ ] **G-D1** [CppTLM 端] 3 pure interfaces compile and link; vendor 头文件零依赖（仅 `<cstdint>` + `<string>`）
- [ ] **G-D2** [双端] `set_blocked_cycles_for_active()` correctly applies per-warp latency
- [ ] **G-D3** [双端] `blocked_cycles_remaining` differs from standalone by ≤ 1 cycle
- [ ] **G-D4** [CppTLM 端] 12-endpoint static_assert passes (enum 值 + 方法签名级 `decltype` 验证)
- [ ] **G-D5** [双端] 5 microbenchmarks stay within ±15% of gpgpu-sim baseline (Phase 4)
- [ ] **G-D6** [PTX-EMU 端] **3 setter** all nullptr -> PTX-EMU zero regression (验证在 PTX-EMU `test_nullptr_fallback`)
- [ ] **G-D7** [PTX-EMU 端] Scoreboard/Pipeline/TC any nullptr -> fallback to **PTX-EMU 内置 InstructionLatencyTable**（此回退逻辑在 PTX-EMU 端实现，CppTLM 端无需实现 InstructionLatencyTable）
- [ ] **G-D8** [双端] `exe_once` stall -> re-schedule -> release -> re-issue loop is consistent

---

## Cross-Project Counterparts

| Repo | Change | Role |
|------|--------|------|
| PTX-EMU | `openspec/changes/cpptlm-phase8b-injection-points/` | Upstream injection interfaces (IScoreboard / IPipelineLatencyProvider / ITensorCoreTiming) + `exe_once()` 3-step hooks |
| PTX-EMU | `openspec/changes/cpptlm-d1-full/` | P0 MemoryBridge + clock-of-truth handshake |
| CppTLM | `openspec/changes/cpptlm-f12b-ld-impl/` | P0 MemoryBridge + KernelLaunchTLM extension (parent, archived `b94eccc`) |
