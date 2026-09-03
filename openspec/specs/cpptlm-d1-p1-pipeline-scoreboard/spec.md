# cpptlm-d1-p1-pipeline-scoreboard Specification

## Purpose
TBD - created by archiving change cpptlm-d1-p1-pipeline-scoreboard. Update Purpose after archive.
## Requirements
### Requirement: cpptlm-scoreboard
The system MUST provide a `ScoreboardTLM` class in `include/tlm/gpu/scoreboard_tlm.hh` + `src/tlm/gpu/scoreboard_tlm.cc` inheriting **directly** from `IScoreboard` (vendored from PTX-EMU `8acfd2d1` in `include/cudart/scoreboard_interface.h`) implementing a **global** hazard table (shared across all warps in an SM, indexed by `(reg_id, warp_id)` tuple) with **O(1) lookup via `std::unordered_map` and CAPACITY=2048** (covers 64 warps/SM × 8 in-flight instructions × 3 dest regs × 1.33 margin; see design.md §2.1 for容量依据). The class MUST provide:
- `bool has_free_entry() const` — O(1)
- `bool allocate(uint32_t reg_id, uint32_t warp_id)` — O(1) via `unordered_map::emplace`, rejects duplicate `(reg_id, warp_id)` allocation (returns `false`); see design.md §2.1 for决议依据
- `bool release(uint32_t reg_id, uint32_t warp_id)` — O(1) via `unordered_map::erase`
- `void tick()` — P1 no-op (see design.md §2.1 for死锁缓解说明)
- `void reset()` — 跨 kernel 清空所有 entries（非虚，CppTLM 特有 — IScoreboard vendored 接口不含此方法）

> **Design Revision 4** (2026-07-18 Oracle P0): std::array 线性扫描 → unordered_map O(1); MAX_ENTRIES=512 → CAPACITY=2048; 新增 reset() 跨 kernel 清空

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

The system MUST perform **编译期** `static_assert` on all 12 enum endpoints (6 `PipelineId` + 6 `TcPrecision`) using the vendored PTX-EMU enum definitions as truth source. Implementation lives in `include/cudart/cpptlm_bridge.h` within the `namespace abi_guards_g_d4` block (lines 196–262), which is compiled every time `cpptlm_bridge.h` is included via the `memory_bridge.hh → src/main.cpp` build chain.

Additionally, to guard against silent ABI drift when PTX-EMU changes method signatures (not just enum values), the implementation includes **signature-level** `static_assert`s using `decltype` on each pure virtual method (see design.md §4).

> **Implementation note (2026-07-18)**: No standalone `test/test_12_endpoint_static_assert.cc` file exists. The 16 static_asserts (12 enum values + 4 signature-level `decltype`) are co-located with the vendored headers in `cpptlm_bridge.h` — this ensures they are evaluated on every build, not only when a specific test file is compiled. A negative test (temporarily breaking `TcPrecision::FP4 = 1`) confirmed the assertions fire at compile time.

#### Scenario: PipelineId and TcPrecision enum values match PTX-EMU
- **WHEN** any translation unit that includes `cpptlm_bridge.h` is compiled (e.g., `src/main.cpp` via `memory_bridge.hh`)
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

### Requirement: cpptlm-ptxemu-driver

The system MUST provide a narrow driver abstraction (`IPtxEmuDriver`) in `include/tlm/gpu/ptx_emu_driver.hh` that decouples CppTLM (C++17) from PTX-EMU (C++20) types. The interface MUST be pure virtual with zero PTX-EMU header dependencies. The class MUST provide:

- `uint32_t advance(uint32_t max_cycles)` — 推进 PTX-EMU 执行并返回实际推进 cycle 数
- `bool is_kernel_complete(uint64_t kernel_id)` — 查询 kernel 完成状态
- `void inject_scoreboard(uint32_t sm_id, IScoreboard* sb)` — per-SM scoreboard 注入
- `void inject_pipeline(uint32_t sm_id, IPipelineLatencyProvider* p)` — per-SM pipeline 注入
- `void inject_tensor_core(uint32_t sm_id, ITensorCoreTiming* tc)` — per-SM tensor core 注入
- `uint32_t num_sms() const` — 返回 SM 数量

The PTX-EMU side MUST provide `PtxEmuDriverShim` implementing `IPtxEmuDriver` using `GPUContext*` as backing, bridging `inject_*` calls to `SMContext::set_scoreboard()` / `set_pipeline_latency_provider()` / `set_tensor_core_timing()` (already defined at `sm_context.h:67-75`).

The shim MUST be registered via `extern "C" PTXEMU_BRIDGE_API void cpptlm_set_driver(IPtxEmuDriver*)`, called during `initialize_environment()` after `GPUContext` is constructed. CppTLM MUST NOT include any PTX-EMU header.

> **Design**: See design.md §8 for complete interface definition, data flow diagram, and build integration fixes (PIC + commit pin + CMake export).

#### Scenario: CppTLM tick drives PTX-EMU execution via driver
- **WHEN** `KernelLaunchTLM::tick()` is called with a non-null `IPtxEmuDriver*`
- **THEN** `driver->advance(MAX_PTX_STEPS_PER_TICK)` calls `GPUContext::exe_once()` N times
- **AND** each `exe_once()` invokes `SMContext::exe_once()` for all SMs, which interacts with injected CppTLM ScoreboardTLM/PipelineTLM/TensorCoreTLM via the 3-step injection (Step A/B/C)
- **AND** completed kernels are detected via `driver->is_kernel_complete(kernel_id)` and their pending entries are popped

#### Scenario: Per-SM scoreboard injection
- **WHEN** `driver->num_sms()` returns N and `main.cpp` creates N `ScoreboardTLM` instances
- **THEN** `driver->inject_scoreboard(i, sb)` for each SM correctly sets `SMContext::scoreboard_` via the existing inline setter
- **AND** each SM independently tracks register hazards within its own warp set (64 warps × 8 in-flight × 3 dest, CAPACITY=2048)
- **AND** PipelineTLM/TensorCoreTLM instances may be shared across SMs (stateless) or created per SM (at identical cost)

---

