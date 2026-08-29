# DGpuBoard Shell: Host→Sim Injection + Sync Wait Design

**Date**: 2026-08-29
**Task**: T-bs-3b (W6 continuation)
**Scope**: mmio_write real injection path + mmio_read real sync wait response
**Status**: Implementation ready

## Overview

This design completes the DGpuBoard shell's host→sim injection path and synchronous wait mechanism for mmio_read operations. The W6 skeleton (commit `0928e12`) established basic mmio_read/write with placeholder responses. This task implements the real injection queue mechanics while deferring actual PcieTlpBundle construction to T-bs-3c.

## Current State (W6)

- `DGpuBoard::mmio_read()`: Promise/future with 1ms timeout, returns -110 on timeout
- `DGpuBoard::mmio_write()`: Async injection into `inject_q_` with mutex protection
- `DGpuBoard::drain_injection_queue()`: Placeholder `set_value(0)` for all requests
- Thread safety: `inject_mu_` protects both `inject_q_` and `pending_resp_`

## Required Changes

### 1. mmio_write Real Path

**Current**: Basic injection with no exception check
**Target**: Add exception propagation check + proper data copying

```cpp
int DGpuBoard::mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len) {
    if (last_exception_) {
        std::rethrow_exception(last_exception_);  // #8 exception propagation
    }
    PendingReq req;
    req.bar = bar;
    req.offset = offset;
    req.data.assign(static_cast<const uint8_t*>(buf), 
                    static_cast<const uint8_t*>(buf) + len);
    req.trans_id = next_trans_id_++;
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        inject_q_.push_back(std::move(req));
    }
    return 0;  // async, no wait
}
```

### 2. mmio_read Real Path

**Current**: Promise/future with timeout, but no response data copy
**Target**: Add response data copy to user buffer

```cpp
int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
    if (last_exception_) {
        std::rethrow_exception(last_exception_);  // #8
    }
    PendingReq req;
    req.bar = bar;
    req.offset = offset;
    req.data.resize(len);  // pre-allocate for response
    req.trans_id = next_trans_id_++;
    auto fut = req.resp.get_future();
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        pending_resp_[req.trans_id] = std::move(fut);
        inject_q_.push_back(std::move(req));
    }
    // 1ms timeout (W6 established)
    auto status = pending_resp_[req.trans_id].wait_for(std::chrono::milliseconds(1));
    if (status != std::future_status::ready) {
        std::lock_guard<std::mutex> lock(inject_mu_);
        pending_resp_.erase(req.trans_id);
        return -110;  // ETIMEDOUT
    }
    int32_t rc = pending_resp_[req.trans_id].get();
    // TODO T-bs-3c: copy response data from req.data to buf
    std::lock_guard<std::mutex> lock(inject_mu_);
    pending_resp_.erase(req.trans_id);
    return rc;
}
```

### 3. drain_injection_queue Real Path

**Current**: Placeholder `set_value(0)` for all requests
**Target**: Keep placeholder (real PcieTlpBundle construction deferred to T-bs-3c)

```cpp
void DGpuBoard::drain_injection_queue() {
    std::deque<PendingReq> drained;
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        drained.swap(inject_q_);
    }
    for (auto& req : drained) {
        if (req.trans_id == UINT64_MAX) {
            // poison pill, skip
            continue;
        }
        // TODO T-bs-3c: construct PcieTlpBundle and inject to soc_->getInternalInputPort("pcie_ep.slave_in")
        // Placeholder: immediate set_value(0) - allows mmio_read to respond
        try {
            req.resp.set_value(0);
        } catch (const std::future_error&) {
            // already set, ignore
        }
        // Clean pending_resp_ (mmio_read caller holds lock for cleanup)
    }
}
```

## Testing Strategy (BS-G2 Start)

### Test Cases

1. **mmio_write returns 0 without exception** - basic functionality
2. **mmio_read with 1ms timeout returns -110 ETIMEDOUT or 0** - timeout behavior
3. **5 responsibilities present** - ABI translation, device enumeration, SOC assembly, callback wiring, lifecycle
4. **destroy order is strict** - stop→poison→join→destruct + idempotent
5. **2 boards concurrent mmio_write** - multi-card thread isolation

### Test File

New file: `test/test_dgpu_board_shell_abi.cc`

## Constraints

1. **1ms timeout must be preserved** - prevents sim thread deadlock
2. **Mutex order**: `inject_mu_` protects both `inject_q_` and `pending_resp_`
3. **Exception propagation**: `last_exception_` checked at entry of mmio_read/write
4. **Real PcieTlpBundle construction deferred** to T-bs-3c
5. **No modifications to**: dgpu_soc.{hh,cc}, dgpu_board_mvp.{hh,cc}, command_processor_mvp.{hh,cc}, etc.

## Success Criteria

- All existing tests pass (978/978 baseline)
- New test cases pass (5 TEST_CASEs)
- mmio_write correctly injects into queue with data
- mmio_read waits for response with 1ms timeout
- drain_injection_queue processes queue and responds to promises
- Multi-card isolation verified (2 boards concurrent mmio_write)
- Destroy sequence strict and idempotent

## References

- Design §2.5 constraints #2/#3
- W6 commit `0928e12` (shell skeleton)
- tasks.md T-bs-3b
- PcieTlpBundle definition in `include/bundles/pcie_bundles_tlm.hh`
- SimModule::getInternalInputPort in `include/core/sim_module.hh`