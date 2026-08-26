# submit-queue-mvp Specification

## Purpose
TBD - created by archiving change 2026-08-21-cpptlm-v05-mvp-s2-dgpu-board. Update Purpose after archive.
## Requirements
### Requirement: submit-queue-wdu-distribution

The system MUST provide a `SubmitQueue` class in `include/tlm/gpu/submit_queue_mvp.hh` + `src/tlm/gpu/submit_queue_mvp.cc` (~150 LOC) implementing **WDU distribution network** with:
- `bool enqueue(cta_desc)` — per-cluster pending FIFO (32 slots)
- `tick()` — per-core active dispatch (4 slots per core)
- `on_warp_complete(task_id, status)` — reverse flow callback
- `select_target_core(cta_desc) -> uint8_t` — MVP固定返回 0

#### Scenario: Enqueue to pending FIFO succeeds within capacity
- **WHEN** `enqueue(cta_desc)` is called with pending FIFO < 32 entries
- **THEN** it MUST return `true` and append to pending FIFO

#### Scenario: Enqueue rejected when FIFO full
- **WHEN** pending FIFO already has 32 entries
- **THEN** `enqueue` MUST return `false` (back-pressure signal)

#### Scenario: Tick dispatches up to active capacity
- **WHEN** `tick()` is called with pending FIFO non-empty and active slots < 4
- **THEN** up to 4 entries MUST be dispatched from pending to active

#### Scenario: Reverse flow on warp complete
- **WHEN** `on_warp_complete(task_id, status)` is called for a dispatched entry
- **THEN** the active slot MUST be released (active count decremented)

#### Scenario: MVP target core selection is fixed
- **WHEN** `select_target_core(any_cta_desc)` is called
- **THEN** it MUST return `0` (single-SM MVP simplification)

### Requirement: submit-queue-tests

The system MUST provide 5 single-test files (per Phase F-H.5 §7) all PASS:
- `test/test_submit_queue_mvp_route.cc` — routing logic
- `test/test_submit_queue_mvp_enqueue.cc` — enqueue boundary conditions
- `test/test_submit_queue_mvp_dispatch.cc` — tick dispatch semantics
- `test/test_submit_queue_mvp_complete.cc` — reverse flow on complete
- `test/test_submit_queue_mvp_concurrent.cc` — concurrent enqueue/tick

All 5 MUST pass `ctest -R "test_submit_queue_mvp"` (s2-G3 acceptance).

#### Scenario: All 5 SQ tests pass
- **WHEN** `ctest -R "test_submit_queue_mvp"` runs after s2 W3
- **THEN** all 5 test files MUST PASS (s2-G3 acceptance)

