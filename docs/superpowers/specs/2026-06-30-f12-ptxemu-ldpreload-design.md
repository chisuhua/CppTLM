# Design: F12 + PTX-EMU LD_PRELOAD Integration

> **Status**: Draft  
> **Date**: 2026-06-30  
> **Decision**: A-B-A (see §4)  
> **Scope**: Phase 7.B (F12) GPU core modules + PTX-EMU integration, preserving `LD_PRELOAD libcudart.so` workflow

---

## 1. Context and Goals

### 1.1 CppTLM State
- Phase 8.A Tasks 1-4 are frozen (737/737 tests pass).
- Phase 8.A Tasks 5-8 are blocked by F12 (Phase 7.B GPU core modules).
- F12 originally planned four new classes:
  - `GpuComputeUnitTLM`
  - `VectorRegFileTLM`
  - `WavefrontTLM`
  - `MinimalWarpSchedulerTLM`

### 1.2 PTX-EMU State
- PTX-EMU is a PTX instruction-level simulator with a fake `libcudart.so`.
- It has a working cycle-approximate timing model (`InstructionLatencyTable`, `blocked_cycles_remaining`, `SMContext::exe_once()`).
- Users run CUDA programs via `LD_PRELOAD`.

### 1.3 Goal
Enable users to run real CUDA programs while CppTLM drives timing and routes memory transactions through its NoC/cache/memory modules.

```bash
LD_PRELOAD=/path/to/libcpptlm_cudart.so ./my_cuda_app
```

---

## 2. Terminology

| Term | Meaning |
|------|---------|
| **PTX-EMU** | PTX instruction-level simulator |
| **CppTLM** | Cycle-approximate TLM 2.0 NoC framework |
| **libcpptlm_cudart.so** | PTX-EMU fake `libcudart.so` enhanced with CppTLM bridge |
| **CppTLMBridge** | C++ interface between PTX-EMU and CppTLM |
| **MemoryBridge** | CppTLM implementation of `CppTLMBridge` for memory routing |
| **KernelLaunchTLM** | CppTLM module accepting PTX-EMU kernel launch requests |

---

## 3. High-Level Architecture

```
User shell:
  LD_PRELOAD=/path/to/libcpptlm_cudart.so ./my_cuda_app

        |
        v
libcpptlm_cudart.so (PTX-EMU fake libcudart + CppTLM core linked)
  |-- __cudaRegisterFunction(func, name)
  |-- cudaMalloc(...)                     [PTX-EMU internal]
  |-- cudaMemcpy(H2D/D2H)                 [PTX-EMU internal]
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

---

## 4. Decision Summary (A-B-A)

| # | Decision | Chosen Option | Rationale |
|---|----------|:-------------:|-----------|
| 1 | `cudaMalloc`/`cudaMemcpy` routing | **A** | Keep internal to PTX-EMU for F12b-LD; H2D/D2H timing deferred |
| 2 | `libcpptlm_cudart.so` build target | **B** | PTX-EMU build target linking CppTLM core; avoids ANTLR4 in CppTLM CI |
| 3 | `GPUConfig` population | **A** | CppTLM emits JSON from `GpuTopology`; PTX-EMU loads via existing `load_json_config()` |

---

## 5. PTX-EMU Side Changes

### 5.1 `cudaLaunchKernel` Async Submission

**File**: `src/cudart/cudart_sim.cpp`

Replace synchronous `wait_for_completion()` with async bridge submission. Completion signaled via `poll_kernel()`.

```cpp
// BEFORE
g_ptx_interpreter->launchPtxInterpreter(...);
g_gpu_context->wait_for_completion();

// AFTER
if (cpptlm_bridge_) {
    cpptlm_bridge_->submit_kernel(kernel_id, launch_params, param_size);
    return cudaSuccess;  // 调用方通过 poll_kernel() 轮询完成
} else {
    // Fallback for standalone PTX-EMU tests
    g_ptx_interpreter->launchPtxInterpreter(...);
    g_gpu_context->wait_for_completion();
}
```

### 5.2 `CppTLMBridge` Interface

**File**: `include/cudart/cpptlm_bridge.h` (new)

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

    /// mmap 基地址，用于 device_addr 转换
    virtual uint64_t get_mmap_base() const = 0;

    /// 当前 CppTLM 仿真 cycle（首次 submit_kernel 前返回 0）
    virtual uint64_t get_current_cycle() const = 0;
};
```

### 5.2.1 Error Semantics

| Condition | Return value | Logging |
|-----------|-------------|---------|
| `global_access` addr not in mmap range | `UINT64_MAX` | WARN "address not in mmap range" |
| `submit_kernel` before bridge init | `false` | ERROR "bridge not initialized" |
| `submit_kernel` with invalid params | `false` | ERROR "invalid kernel params" |
| `poll_kernel` unknown kernel_id | `UINT64_MAX` | WARN "unknown kernel_id" |
| `get_current_cycle()` before first submit | `0` (valid pre-sim state) | No log needed |
| Bridge version mismatch | `static_assert` at compile time | FATAL abort |

### 5.3 `HardwareMemoryManager` Intercept

**File**: `src/memory/hardware_memory_manager.cpp`

For `MemorySpace::GLOBAL`, route through `CppTLMBridge` for timing while keeping data in `SimpleMemory`.

```cpp
if (req.space == MemorySpace::GLOBAL && cpptlm_bridge_) {
    uint64_t device_addr = req.address - cpptlm_bridge_->get_mmap_base();
    uint64_t latency = cpptlm_bridge_->global_access(
        device_addr, req.data, req.type);
    if (latency == UINT64_MAX) {
        // 地址未映射 → 回退到本地 fast path
        return simple_memory_->direct_access(req.address, req.data, req.size, req.is_write);
    }
    // Actual data still in SimpleMemory
    simple_memory_->direct_access(req.address, req.data, req.size, req.is_write);
    return latency;
}
```

### 5.4 Build Target

**File**: `PTX-EMU/CMakeLists.txt`

```cmake
find_package(CppTLM REQUIRED)

target_link_libraries(cudart ${ANTLR4_LIBRARY} pthread cpptlm::cpptlm_core)
set_target_properties(cudart PROPERTIES LIBRARY_OUTPUT_NAME "cpptlm_cudart")
```

---

## 6. CppTLM Side Additions

### 6.1 `MemoryBridge`

Implements `CppTLMBridge` and routes PTX-EMU GLOBAL memory requests through CppTLM NoC.

```cpp
class MemoryBridge : public CppTLMBridge {
public:
    uint64_t global_access(uint64_t device_addr, uint64_t val,
                           uint8_t type) override {
        // Convert to ComputeReqBundle
        // Route through CrossbarTLM -> CacheTLM -> MemoryTLM
        // Return latency cycles (UINT64_MAX if unmapped)
    }

    bool submit_kernel(uint64_t kernel_id, const void* params,
                       size_t param_size) override { /* ... */ }
    uint64_t poll_kernel(uint64_t kernel_id) override { /* ... */ }
    uint64_t get_mmap_base() const override { /* ... */ }
    uint64_t get_current_cycle() const override { /* ... */ }
};
```

### 6.2 `KernelLaunchTLM` Extension

Accept PTX-EMU `KernelLaunchRequest` and hold `GPUContext` instance.

```cpp
class KernelLaunchTLM : public ChStreamModuleBase {
public:
    void submit(ptxsim::KernelLaunchRequest&& req);
    void tick() override;
private:
    ptxsim::GPUContext* gpu_context_ = nullptr;
};
```

### 6.3 Python GPU Config Emitter

**File**: `cpptlm_config/builder/gpu_config_emitter.py`

```python
GPU_CONFIG_TO_PTXEMU = {
    "num_sms": lambda t: t["num_gpc"] * t["num_tpc_per_gpc"] * t["num_sm_per_tpc"],
    "warp_size": lambda t: t["warp_size"],
    "shared_mem_size_per_sm": lambda t: t["smem_kb_per_sm"] * 1024,
    "registers_per_sm": lambda t: t["regfile_kb_per_sm"] * 1024 // 4,
    "max_blocks_per_sm": lambda t: 32,
}
```

---

## 7. Timing Model

- CppTLM `EventQueue` tick is the single clock source.
- One CppTLM tick drives one PTX-EMU `GPUContext::exe_once()` cycle.
- Memory latency from CppTLM `MemoryBridge` is fed back into PTX-EMU `blocked_cycles_remaining`.
- PTX-EMU `InstructionLatencyTable` continues to provide non-memory instruction latencies when consumed.

### 7.1 Timing Semantics

1. **Tick ordering**: CppTLM advances one tick, _then_ the bridge calls `exe_once()`. `get_current_cycle()` reflects the tick value _before_ the PTX-EMU step.
2. **Idle loop burn**: Multiple `exe_once()` calls between two `global_access` invocations do **not** advance the simulation cycle counter.
3. **Bounded execution**: At most `max_ptx_steps_per_tick` (default: 10,000) `exe_once()` calls per CppTLM tick, preventing infinite loops/deadlocks.

> For detailed risk analysis of dual-model alignment, see Integration Plan §7 (Timing Alignment Risks) and §10.1.

---

## 8. Address Mapping

PTX-EMU `cudaMalloc` returns Linux virtual addresses (mmap region). CppTLM NoC uses CUDA device addresses.

```
device_addr = virtual_addr - mmap_base - device_base_offset
```

`device_base_offset` is typically `0x0`.

---

## 9. Resource Ownership

| Resource | Owner | Notes |
|----------|-------|-------|
| SM count / GPU config | CppTLM topology -> PTX-EMU `GPUConfig` | Via JSON |
| CTA creation / admission | PTX-EMU | Reused as-is |
| Warp creation | PTX-EMU | Reused as-is |
| Shared memory space | PTX-EMU | Capacity constrained by CppTLM topology |
| Local memory | PTX-EMU | `CudaDriver` pool |
| Registers | PTX-EMU | `RegisterBankManager` |
| **Global memory timing** | **CppTLM** | Main integration value |
| Global memory data | PTX-EMU `SimpleMemory` | Data stays in PTX-EMU |

---

## 10. Roadmap

### F12a (current)
Implement standalone classes to unblock Phase 8.A:
- `GpuComputeUnitTLM`
- `VectorRegFileTLM`
- `WavefrontTLM`
- `MinimalWarpSchedulerTLM`

### F12b-LD
- Build `libcpptlm_cudart.so`.
- Add `CppTLMBridge` interface in PTX-EMU.
- Async `cudaLaunchKernel` submission.
- `HardwareMemoryManager` GLOBAL intercept.
- `MemoryBridge` implementation in CppTLM.
- `KernelLaunchTLM` PTX-EMU request extension.
- Python GPU config emitter.

### Phase 8.B
- Replace PTX-EMU `WarpScheduler` with `MinimalWarpSchedulerTLM`.
- Add compute-grid dispatch policy.
- Optional Command Processor / HyperQueue modeling.

### Phase 9+
- CppTLM JSON directly loads `.ptx`.
- Full co-simulation without LD_PRELOAD.

---

## 11. Acceptance Criteria

### F12a
- [ ] 737/737 CppTLM tests pass.
- [ ] Phase 8.A integration test yields `requests_completed > 0`.

### F12b-LD
- [ ] `libcpptlm_cudart.so` builds.
- [ ] Reference CUDA program runs under `LD_PRELOAD` without crash.
- [ ] Global memory accesses observed in CppTLM counters.
- [ ] Kernel output matches CPU/reference implementation.
- [ ] Performance within 10% of standalone PTX-EMU or documented deviation.
- [ ] docs_sync 0 missing paths.

---

## 12. Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Dual timing models | CppTLM EventQueue as single clock; one tick = one `exe_once()` |
| Global singletons | Refactor PTX-EMU to context objects; encapsulate per-simulation state |
| ANTLR4 build dependency | Keep ANTLR4 in PTX-EMU build only; CppTLM links pre-built core static lib |
| Memory address mapping | Explicit `device_addr = virtual_addr - mmap_base` conversion in bridge |
| Silent data corruption | Numerical correctness validation against CPU/reference |
| Interface stability | `CppTLMBridge::VERSION` + versioning policy |

---

## 13. Open Questions

1. Should `cudaMalloc`/`cudaMemcpy` be routed through CppTLM in Phase 8.B?
2. Exact CMake integration: `find_package(CppTLM)` vs FetchContent vs subdirectory?
3. How to expose PTX-EMU `SimpleMemory` mmap base to `MemoryBridge`?
4. Fallback when PTX-EMU ANTLR parser fails on user code?

---

*End of design document.*
