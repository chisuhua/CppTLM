# Plan: F12 + PTX-EMU LD_PRELOAD Integration

> **Status**: Draft for review  
> **Date**: 2026-06-30  
> **Scope**: Phase 7.B (F12) GPU core modules + PTX-EMU integration while preserving LD_PRELOAD usage  
> **Decision basis**: Oracle recommendation (Option A progressive integration) + user requirement to keep `LD_PRELOAD libcudart.so`

---

## 1. Context

### 1.1 CppTLM State
- Phase 8.A Tasks 1-4 are frozen (737/737 tests pass, 364 docs paths valid).
- Phase 8.A Tasks 5-8 are blocked by F12 (Phase 7.B GPU core modules).
- F12 originally planned 4 new classes:
  - `GpuComputeUnitTLM` (~300-400 LOC)
  - `VectorRegFileTLM` (~150 LOC)
  - `WavefrontTLM` (~200 LOC)
  - `MinimalWarpSchedulerTLM` (~300-400 LOC)
- Total budget: 950-1200 LOC, 2-3 weeks.

### 1.2 PTX-EMU State
- PTX-EMU (`/workspace/project/PTX-EMU`) is a PTX instruction-level simulator.
- It has a working cycle-approximate timing model (`InstructionLatencyTable`, `blocked_cycles_remaining`, `SMContext::exe_once()`).
- It provides a fake `libcudart.so` that intercepts CUDA runtime APIs.
- Users can run real CUDA programs via:
  ```bash
  LD_PRELOAD=/path/to/PTX-EMU/lib/libcudart.so ./my_cuda_app
  ```

### 1.3 User Requirement
- Keep the `LD_PRELOAD libcudart.so` workflow.
- Kernel dispatch behavior correctness is sufficient for now; precise Command Processor / HyperQueue / Compute Grid dispatch modeling is deferred.
- Want a roadmap for how `KernelLaunchTLM` and `WavefrontTLM` can directly use PTX-EMU's kernel dispatch.

---

## 2. Goal

Design an integration where:
1. Users continue to run CUDA applications via `LD_PRELOAD`.
2. PTX-EMU executes PTX instructions functionally.
3. CppTLM drives timing and routes memory transactions through its NoC/cache/memory modules.
4. F12a first unblocks Phase 8.A with standalone implementations, then F12b-LD introduces PTX-EMU integration.

---

## 3. Integration Model: Scheme D (Single-Process Library Integration)

PTX-EMU's fake `libcudart.so` links against the CppTLM core library. The CUDA application, PTX-EMU, and CppTLM run in the same OS process. `cudaLaunchKernel` is intercepted and forwards the kernel request to CppTLM; CppTLM's `EventQueue` drives PTX-EMU's `GPUContext::exe_once()`; PTX-EMU GLOBAL memory accesses are routed through CppTLM's `MemoryTLM`/`CacheTLM`/`CrossbarTLM`.

### 3.1 High-Level Flow

```
User shell:
  LD_PRELOAD=/path/to/libcpptlm_cudart.so ./my_cuda_app

        |
        v
libcpptlm_cudart.so (PTX-EMU fake libcudart + CppTLM core linked)
  |-- __cudaRegisterFunction(func, name)
  |-- cudaMalloc(...)                     [optional: route to CppTLM]
  |-- cudaMemcpy(H2D/D2H)                 [optional: route to CppTLM]
  |-- cudaLaunchKernel(func, gridDim, blockDim, args, sharedMem, stream)
         |
         v
     [PTX-EMU side] parse PTX -> build KernelLaunchRequest
         |
         v
     [CppTLM side] KernelLaunchTLM::submit(request)
         |
         v
     CppTLM EventQueue loop:
       while (!done):
         KernelLaunchTLM.tick()          -> advance GPUContext::exe_once()
         MinimalWarpSchedulerTLM.tick()  -> schedule warp (D1 only)
         GpuComputeUnitTLM.tick()        -> dispatch sub-core
         CrossbarTLM/CacheTLM/MemoryTLM.tick()
         advance_event_queue()
         |
         v
     PTX-EMU LD/ST hits HardwareMemoryManager::access()
       -> route GLOBAL to CppTLM MemoryBridge
       -> return data + latency
         |
         v
     kernel completion -> on_complete callback -> cudaLaunchKernel returns
```

### 3.2 Sub-Options

| Sub-option | Description | LD_PRELOAD | Complexity | Precision |
|------------|-------------|:----------:|:----------:|:---------:|
| **D1** | Full-function co-process: CppTLM replaces PTX-EMU WarpScheduler and controls SM scheduling. | Yes | High | High |
| **D2** | Memory-timing bridge (recommended for F12b): PTX-EMU keeps its own SM/scheduler; only GLOBAL memory accesses route through CppTLM NoC. | Yes | Medium | Medium |
| **D3** | Trace-replay: PTX-EMU runs independently, emits memory trace, CppTLM replays/processes. | Yes | Low | Low |

**Recommendation**: Target **D2 for F12b**. It preserves the LD_PRELOAD workflow, introduces real workload-driven memory traffic with minimal PTX-EMU changes, and leaves the door open to D1 in Phase 8.B.

---

## 4. Roadmap

### Phase F12a (current, unblock Phase 8.A)
- `KernelLaunchTLM`: standalone AQL dispatch (existing Phase 8.A behavior).
- `WavefrontTLM`: standalone data carrier.
- `MinimalWarpSchedulerTLM`: standalone round-robin scheduler.
  - Interface names/signatures aligned with PTX-EMU `WarpScheduler` for future replacement.
- `GpuComputeUnitTLM`: standalone 4 `SubCoreSlot` implementation.
- Acceptance: Phase 8.A G1/G2 gates pass; `requests_completed > 0` in integration test.

### Phase F12b-LD (after 8.A unblocked)
- Build `libcpptlm_cudart.so` = PTX-EMU fake `libcudart` + CppTLM core (static or dynamic link).
- `KernelLaunchTLM` extension: accept PTX-EMU `KernelLaunchRequest`.
- Modify PTX-EMU `cudaLaunchKernel` to submit request to CppTLM instead of synchronous `wait_for_completion()`.
- Add `MemoryBridge`: intercept `HardwareMemoryManager::access()` for `MemorySpace::GLOBAL` and route to CppTLM `MemoryTLM`/`CacheTLM`.
- `GpuComputeUnitTLM` extension: wrap PTX-EMU `SMContext`; each CppTLM tick calls `GPUContext::exe_once()`.
- `MinimalWarpSchedulerTLM`: operate in observer/statistics mode (D2); do not replace PTX-EMU WarpScheduler yet.
- `WavefrontTLM` extension: optionally wrap PTX-EMU `WarpContext` for trace/observation.
- Acceptance: A simple CUDA program under `LD_PRELOAD` runs; its global memory accesses traverse CppTLM NoC; CppTLM reports latency/counters.

### Phase 8.B
- `MinimalWarpSchedulerTLM` replaces PTX-EMU `WarpScheduler` via `SMContext::set_warp_scheduler()` (D1).
- Add real compute-grid dispatch policy (GPC/TPC/SM distribution).
- Optional: model Command Processor and HyperQueue.

### Phase 9+ (Option C)
- CppTLM JSON topology directly specifies `.ptx` path.
- No `LD_PRELOAD` required; CppTLM main program loads and executes PTX.
- Full co-simulation.

---

## 5. PTX-EMU Side Changes

1. **`src/cudart/cudart_sim.cpp:343-348`**
   - Remove or optionalize the synchronous `wait_for_completion()` loop inside `cudaLaunchKernel`.
   - Replace with submission to CppTLM bridge and an async completion callback.

2. **New `include/cudart/cpptlm_bridge.h`**
   - Define a C++ callback interface:
     ```cpp
static constexpr uint64_t CppTLMBRIDGE_VERSION = 1;

class CppTLMBridge {
public:
    virtual ~CppTLMBridge() = default;

    /// 异步提交：返回 false 表示失败
    virtual bool submit_kernel(uint64_t kernel_id, const void* params,
                               size_t param_size) = 0;

    /// GLOBAL 内存访问：返回 latency cycles，UINT64_MAX = 地址未映射
    virtual uint64_t global_access(uint64_t device_addr, uint64_t val,
                                   uint8_t type) = 0;

    /// 轮询完成状态：0 = 已完成，>0 = 剩余 cycles，UINT64_MAX = 未知 kernel_id
    virtual uint64_t poll_kernel(uint64_t kernel_id) = 0;

    /// mmap 基地址
    virtual uint64_t get_mmap_base() const = 0;

    /// 当前 CppTLM 仿真 cycle（首次 submit_kernel 前返回 0）
    virtual uint64_t get_current_cycle() const = 0;
};
     ```

   **Error semantics**: `global_access` returns `UINT64_MAX` for unmapped addresses; `submit_kernel` returns `false` on init/param error; `poll_kernel` returns `UINT64_MAX` for unknown kernel IDs; `get_current_cycle()` returns 0 before first submission (valid pre-sim state). Bridge version mismatch is a compile-time `static_assert`.

3. **`src/memory/hardware_memory_manager.cpp`**
   - In `access()`, check whether a `CppTLMBridge` is registered.
   - If `space == MemorySpace::GLOBAL`, route the request through the bridge.
   - Otherwise, keep existing fast-path behavior.

4. **`src/cudart/ptx_interpreter.cpp:433`**
   - Replace `g_gpu_context->submit_kernel_request(std::move(request))` with a call to the registered bridge.

5. **CMake / build**
   - Add a new target `cpptlm_cudart` that builds a shared library combining PTX-EMU fake `libcudart` sources with CppTLM core library.

---

## 6. CppTLM Side Additions

1. **`libcpptlm_cudart.so` build target**
   - CMake target linking PTX-EMU cudart sources + CppTLM core.

2. **`KernelLaunchTLM` extension**
   - Accept a PTX-EMU `KernelLaunchRequest`.
   - Hold a PTX-EMU `GPUContext` instance.
   - Provide `tick()` that advances `GPUContext::exe_once()`.

3. **`MinimalWarpSchedulerTLM`**
   - F12a: standalone round-robin.
   - F12b-LD: observer mode (D2), collects scheduling traces.
   - 8.B: replacement mode (D1) via `SMContext::set_warp_scheduler()`.

4. **`GpuComputeUnitTLM` extension**
   - F12a: standalone `SubCoreSlot` model.
   - F12b-LD: wraps PTX-EMU `SMContext` and exposes CppTLM counters/stats.

5. **`WavefrontTLM` extension**
   - F12a: standalone data carrier.
   - F12b-LD: optionally wraps PTX-EMU `WarpContext`.

6. **`MemoryBridge`**
   - Implements PTX-EMU `CppTLMBridge::global_access()`.
   - Translates PTX-EMU memory requests into CppTLM `ComputeReqBundle` transactions.
   - Routes through `CrossbarTLM` -> `CacheTLM` -> `MemoryTLM`.

7. **`CudaRuntimeBridge`** (optional)
   - Routes `cudaMalloc`/`cudaMemcpy` to CppTLM memory space if desired.

---

## 7. Timing Model

- CppTLM `EventQueue` tick is the single clock source.
- One CppTLM tick drives one PTX-EMU `GPUContext::exe_once()` cycle.
- PTX-EMU's internal `gpu_clock` is aligned with CppTLM tick count.
- Memory latency returned by CppTLM `MemoryBridge` is fed back into PTX-EMU `blocked_cycles_remaining`.
- PTX-EMU's `InstructionLatencyTable` is still used for non-memory instruction latencies when consumed.

### 7.1 Timing Semantics (from Oracle review)

1. **Tick ordering**: CppTLM advances one tick, _then_ the bridge calls `exe_once()`. `get_current_cycle()` reflects the tick value _before_ the PTX-EMU step.
2. **Idle loop burn**: Multiple `exe_once()` calls between two `global_access` invocations do **not** advance the simulation cycle counter.
3. **Bounded execution**: At most `max_ptx_steps_per_tick` (default: 10,000) `exe_once()` calls per CppTLM tick, preventing infinite loops/deadlocks.

> **Cross-reference**: Detailed risk analysis in §10.1 (Dual timing models).

---

## 8. Resource Management Ownership

| Resource | Owner | Notes |
|----------|-------|-------|
| SM count / GPU config | CppTLM topology -> fills PTX-EMU `GPUConfig` | |
| CTA creation / admission | PTX-EMU `execute_kernel_internal` / `SMContext::add_block` | Reused as-is |
| Warp creation | PTX-EMU `CTAContext::init` | Reused as-is |
| Shared memory space | PTX-EMU allocates; capacity constrained by CppTLM `GpuTopology` | |
| Shared memory timing | Optional: route `.shared` accesses to `SharedMemoryTLM` | Deferred to 8.B |
| Local memory | PTX-EMU `CudaDriver` pool | Keep internal |
| Registers | PTX-EMU `RegisterBankManager` | `VectorRegFileTLM` observes in 8.B |
| **Global memory** | **CppTLM `MemoryTLM`** | Main integration value |

---

## 9. Acceptance Criteria (Verifiable Gates)

### F12a
- [ ] `./build/bin/cpptlm_tests "[gpu]"` still 737/737 pass.
- [ ] `./build/bin/cpptlm_tests "[phase8a]"` passes (34 cases / 82 assertions).
- [ ] Integration test: `GpuComputeUnitTLM` -> `CacheTLM` -> `MemoryTLM` yields `requests_completed > 0`.

### F12b-LD
- [ ] `libcpptlm_cudart.so` builds successfully.
- [ ] A simple CUDA program (e.g., vector add) launches under `LD_PRELOAD=libcpptlm_cudart.so` without crash.
- [ ] Global memory accesses from the CUDA program are observed in CppTLM `MemoryTLM` counters.
- [ ] **Correctness**: output of reference CUDA kernel matches CPU/reference implementation.
- [ ] **Performance sanity**: kernel execution time/cycle count is within 10% of standalone PTX-EMU run for the same kernel (or documented deviation).
- [ ] Synthetic memory traffic test passes before full CUDA application test.
- [ ] `docs_sync_check.sh --strict` reports 0 missing paths.
- [ ] CppTLM existing tests still 737/737 pass.

### 8.B
- [ ] `MinimalWarpSchedulerTLM` replaces PTX-EMU `WarpScheduler` and produces identical functional results for a reference kernel.
- [ ] Multi-SM dispatch policy implemented.

---

## 10. Top Risks

1. **Dual timing models**: PTX-EMU's `cycle_counter_` and CppTLM `EventQueue` must be carefully aligned to avoid double-counting or skipped cycles.
2. **PTX-EMU global singletons**: `g_gpu_context`, `g_ptx_interpreter`, `CudaDriver::instance()`, `HardwareMemoryManager::instance()` must be abstracted or made instance-per-CppTLM-simulation.
3. **ANTLR4 build dependency**: Adding PTX-EMU sources to CppTLM build introduces ANTLR4 runtime and generated parser compilation into CppTLM CI, increasing build time and complexity.
4. **Memory address space mapping**: PTX-EMU's flat `CudaDriver` global pool must be mapped to CppTLM's address space; pointer values visible to the CUDA app must remain valid.
5. **Synchronous CUDA API expectations**: `cudaLaunchKernel` is synchronous in current PTX-EMU; making it async while preserving CUDA semantics requires careful completion signaling.
6. **Interface stability**: No versioning/compatibility strategy for the `CppTLMBridge` interface as projects evolve.
7. **Silent data corruption**: Basic "no crash" validation is insufficient; numerical correctness must be verified.
8. **Performance validity**: Without a baseline comparison, coupled timing results may look plausible but be misleading.

---

## 11. Decision Checklist Before Starting F12b-LD

- [ ] F12a complete and Phase 8.A integration tests stable.
- [ ] OpenSpec change `2026-06-xx-ptxemu-integration-strategy` approved.
- [ ] PTX-EMU version/commit and build dependency handling specified (ANTLR4, generated parser).
- [ ] PTX-EMU team agrees to the bridge interface changes and commits to a stable interface version.
- [ ] CMake integration approach for `libcpptlm_cudart.so` validated with a proof-of-concept build.
- [ ] Reference CUDA program, expected output, and correctness validation method chosen for F12b-LD.
- [ ] Synthetic memory traffic test passes before full CUDA application test.
- [ ] Performance validation approach established (baseline comparison with standalone PTX-EMU).
- [ ] Strategy for handling PTX-EMU global singletons decided and prototyped.
- [ ] Risk mitigation plans for top 3 hidden risks documented and reviewed.

---

## 12. Open Questions

1. Should `cudaMalloc`/`cudaMemcpy` be routed through CppTLM in F12b-LD, or kept internal to PTX-EMU until Phase 8.B?
2. Should `libcpptlm_cudart.so` be a CppTLM build target or a PTX-EMU build target?
3. How is the PTX-EMU `GPUConfig` populated from CppTLM `GpuTopology`?
4. What is the fallback if PTX-EMU ANTLR parser fails on a user CUDA program?
5. What is the versioning policy for the `CppTLMBridge` interface?

---

*End of plan.*
