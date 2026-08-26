# doorbell-mvp Specification

## Purpose
TBD - created by archiving change 2026-08-21-cpptlm-v05-mvp-s2-dgpu-board. Update Purpose after archive.
## Requirements
### Requirement: doorbell-strong-order-write

The system MUST provide a `Doorbell` class in `include/tlm/gpu/doorbell_mvp.hh` + `src/tlm/gpu/doorbell_mvp.cc` implementing SQ tail register write with **strong-order write** semantics. Latency MUST fall within the 250-700ns range per PCIe ordering research.

#### Scenario: Doorbell write latency in PCIe range
- **WHEN** `Doorbell::ring(stream_id, wdu_offset)` is called
- **THEN** the write MUST complete within the 250-700ns latency window (s2-G2 acceptance)
- **AND** the SQ tail pointer MUST be atomically visible to the SubmitQueue

#### Scenario: Same-stream sequential order preserved
- **WHEN** two `Doorbell::ring(stream_id, ...)` calls fire on the same stream_id within ordering window
- **THEN** the SubmitQueue MUST observe them in call order (strong-order)

### Requirement: completion-ring-push-and-host-notify

The system MUST provide a `CompletionRing` class in `include/tlm/gpu/completion_ring_mvp.hh` + `src/tlm/gpu/completion_ring_mvp.cc` that:
- Pushes a completion entry on `on_warp_complete(task_id, status)` callback
- Triggers `host_notify` hook for guest-side notification

#### Scenario: Completion entry pushed on warp completion
- **WHEN** SubmitQueue calls `CompletionRing::on_warp_complete(task_id=42, status=0)`
- **THEN** a completion entry with `task_id=42, status=0` MUST be pushed to the ring buffer
- **AND** the `host_notify` hook MUST fire exactly once per entry

#### Scenario: Host notify hook fires once per entry
- **WHEN** 10 `on_warp_complete` calls happen in sequence
- **THEN** `host_notify` MUST fire exactly 10 times (no drops, no duplicates)

