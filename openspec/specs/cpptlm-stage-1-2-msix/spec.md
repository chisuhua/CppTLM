# cpptlm-stage-1-2-msix Specification

## Purpose
TBD - created by archiving change 2026-09-10-cpptlm-stage-1-2-msix. Update Purpose after archive.
## Requirements
### Requirement: MSI-X trigger_irq_async 真实实现

The system MUST implement `PcieEndpointTLM::trigger_irq_async(uint32_t vector, uint64_t payload)` to push MSI-X TLP into inject_q_ and invoke the registered intr_cb after sim_loop drain completes. When vector is out of range or already pending, MUST return appropriate error (not segfault).

#### Scenario: intr_cb called within 200ms (Oracle O6 修订：payload 不可用)

- **WHEN** `msix_init(4)` + `cpptlm_emulator_register_callbacks(intr_cb, msix_cb, ...)` + `msix_update_pending(emu, 0)` are called
- **THEN** `msix_update_pending` returns 0
- **AND** `intr_cb(user_ctx, vector=0, trans_id=...)` is invoked within 200ms timeout
- **AND** `intr_cb_called` count ≥ 1
- **AND** captured_vector == 0
- **NOTE**: payload 不在 23 ABI 范围（头文件 L96 无 payload 参数；intr_cb typedef L56 为 `(user_ctx, vector, trans_id)`）

#### Scenario: Out-of-range vector returns -EINVAL

- **WHEN** `msix_update_pending(emu, 999)` is called with `msix_init(4)` (table_size=4)
- **THEN** the function returns -EINVAL
- **AND** no TLP pushed to inject_q_
- **AND** intr_cb not invoked

#### Scenario: Vector 0-7 valid range (Oracle O6: 命名"0-3" → "0-7")

- **WHEN** `msix_init(8)` then `msix_update_pending(emu, vector=0..7)` for all 8 vectors
- **THEN** all 8 calls return 0
- **AND** 8 intr_cb invocations after drain

#### Scenario: Already pending vector may coalesce

- **WHEN** `msix_update_pending(emu, 0)` is called twice rapidly without drain
- **THEN** first call returns 0
- **AND** second call returns 0 (or -EAGAIN，per DGpuBoard::trigger_irq_async 内部 mutex 裁决)
- **AND** intr_cb invocation count ≥ 1 (may coalesce)

### Requirement: Existing ABI contract preserved

The system MUST preserve the existing `cpptlm_emulator_msix_init/update/clear` ABI signatures and behaviors. No ABI changes.

#### Scenario: msix_init/update/clear signatures unchanged

- **WHEN** existing cpptlm_emulator C ABI callers upgrade to this change version
- **THEN** no caller code modification required
- **AND** ABI function signatures/return values unchanged

