# Spec: CommandProcessor Fill Implementation (s3 T-s3-2)

> **Status**: Proposed — 2026-08-21
> **Scope**: s3 change — fill `command_processor_mvp.cc` 实现 (header 由 s2 已创建)
> **依赖**: s1 + s2 已 archive (s2 已建立 CP skeleton with `set_decoder` injection)

---

## ADDED Requirements

### Requirement: command-processor-fsm-fill

The system MUST fill `src/tlm/gpu/command_processor_mvp.cc` (~150 LOC 填充 + 既有 ~150 LOC 骨架) implementing the 5-state FSM:
- **IDLE** → **FETCH**: `mem_read_vram(GPU_VA, sizeof(gpu_gpfifo_entry))` (per Phase F-C.3 H1)
- **FETCH** → **DECODE**: call `pm4_decoder_->parse_method(method_header, payload, max_dwords)` (via s2's `set_decoder` injection, NOT direct construction)
- **DECODE** → **DISPATCH**: call `tmu_.submit(Pm4MethodDispatch)` (replaces s2's `record` stub)
- **DISPATCH** → `dispatch_target_(TmuDispatchRecord)` → TMU → S3SubmitQueueHandler → SubmitQueue.enqueue (per Oracle M4 handler 模式); CQ 通知 deferred to cpptlm-dgpu-abi-export change
- **COMPLETE** → **IDLE**: cycle restart

#### Scenario: Full IDLE→DISPATCH→TMU→SQ cycle with NVIDIA method packet
- **WHEN** CP wakes (Doorbell ring) and processes a real `gpu_gpfifo_entry` containing DISPATCH_DIRECT (0x4001)
- **THEN** CP MUST complete IDLE→FETCH→DECODE→DISPATCH
- **AND** DISPATCH MUST call `dispatch_target_(record)` to submit TmuDispatchRecord to TMU
- **AND** TMU MUST forward to S3SubmitQueueHandler → SubmitQueue.enqueue
- **AND** CQ notification is deferred to cpptlm-dgpu-abi-export change

#### Scenario: FETCH via VRAM read
- **WHEN** CP FETCH state runs `tick()`
- **THEN** it MUST call `mem_read_vram(GPU_VA, sizeof(gpu_gpfifo_entry))` to obtain the next packet
- **AND** transition to DECODE on successful read

#### Scenario: DECODE uses injected decoder (not direct construction)
- **WHEN** CP DECODE state runs `tick()`
- **THEN** it MUST call `pm4_decoder_->parse_method(...)` where `pm4_decoder_` was injected via s2's `set_decoder`
- **AND** MUST NOT directly construct or depend on `Pm4Decoder`

#### Scenario: DISPATCH submits to TMU
- **WHEN** DECODE returns a `Pm4MethodDispatch`
- **THEN** DISPATCH MUST call `tmu_.submit(dispatch)` (替代 s2's `record` stub)

### Requirement: command-processor-tests

The system MUST provide `test/test_command_processor_mvp.cc` covering:
- 5 state transitions (real NVIDIA method packet decode)
- (Replaces s2 skeleton `test_command_processor_mvp_skeleton.cc` no-op test)

Both MUST pass `ctest -R "test_command_processor_mvp"`.

#### Scenario: CP fill test passes 5 transitions with real NVIDIA packet
- **WHEN** `ctest -R "test_command_processor_mvp"` runs after s3 W5
- **THEN** all state transitions with real NVIDIA method packet decode MUST PASS