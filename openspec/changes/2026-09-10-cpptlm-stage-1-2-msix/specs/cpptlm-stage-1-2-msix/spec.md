# Spec: cpptlm-stage-1-2-msix

> **Capability**: cpptlm-stage-1-2-msix
> **Owner**: CppTLM Architecture Team
> **状态**: 🔄 Proposed（2026-09-10）
> **Created**: 2026-09-10
> **关联**: [proposal.md](../proposal.md) + [design.md](../design.md) + [tasks.md](../tasks.md)

## Purpose

修复 #4 中断链断裂：`cpptlm_emulator_msix_init/update/clear` intr_cb 真实接线，但 `trigger_irq_async` 全仓无调用方，导致测试验证 0 触发。本 spec 实施 `trigger_irq_async` 真实实现，闭合中断链。

## ADDED Requirements

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

## MODIFIED Requirements

(N/A — this change does not modify existing specs)

## REMOVED Requirements

(N/A — this change does not remove existing specs)

## Cross-References

- Upstream: `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes`（阶段 1.1 4 bug 修复，前置）
- Downstream: `2026-09-10-ue-stage-1-2-msix-integration`（UE 集成测试）
- Parent: `2026-09-09-cpptlm-pcie-ep-foundation`（spec SSOT）
- ADR-092 HAL adapter + bypass binding
- [design.md §9.1](../design.md) — `trigger_msix(vector)` 透传
