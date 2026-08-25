# Spec: TmuDispatchProcessor Fill Implementation (s3 T-s3-3)

> **Status**: Proposed — 2026-08-21
> **Scope**: s3 change — fill `tmu_dispatch_processor_mvp.cc` 实现 (header 由 s2 已创建)
> **依赖**: s1 + s2 已 archive (s2 已建立 TMU skeleton with `set_handler` injection)
> **关联**: Phase F-D.2 H5 (dep chain)

---

## ADDED Requirements

### Requirement: tmu-dispatch-fill-data-structure

The system MUST define `TmuDispatchRecord` struct with **9 fields** (per s2 design). Field layout MUST be specified in `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` (s3 fills the inline comments).

#### Scenario: TmuDispatchRecord 9-field layout
- **WHEN** a `TmuDispatchRecord` is constructed
- **THEN** it MUST contain exactly 9 fields per design.md §3 (s3 填字段语义注释)

### Requirement: tmu-dispatch-fill-logic

The system MUST fill `src/tlm/gpu/tmu_dispatch_processor_mvp.cc` (~250 LOC 填充) implementing:
- `submit(Pm4MethodDispatch)` — enqueue dispatch record
- **Back-pressure stop fetch**: signal SubmitQueue to pause when capacity full
- **Dependency chain tracking**: detect FIFO ordering violations across SubmitQueue entries
- **Cycle detection**: detect and break circular dep chains
- `set_handler(std::unique_ptr<TmuHandlerInterface>)` — inject actual handler (replaces s2's set_handler stub)

#### Scenario: submit enqueues to TMU pending
- **WHEN** CommandProcessor DISPATCH calls `tmu_.submit(Pm4MethodDispatch)`
- **THEN** a new `TmuDispatchRecord` MUST be added to TMU pending queue (if capacity available)

#### Scenario: Back-pressure stops SubmitQueue fetch
- **WHEN** TMU pending queue is full (32 entries)
- **THEN** TMU MUST signal SubmitQueue to pause enqueue
- **AND** SubmitQueue MUST stop fetching new WDUs until TMU capacity frees

#### Scenario: Dependency chain violation detected
- **WHEN** dispatch record B depends on record A (FIFO ordering violation across dispatch boundaries)
- **THEN** TMU MUST detect and report dep violation (via handler callback or error state)

#### Scenario: Circular dep chain detected and broken
- **WHEN** dep chain contains cycle (A → B → A)
- **THEN** TMU MUST detect cycle
- **AND** break cycle (drop oldest dep edge OR marked-as-broken in record)

### Requirement: tmu-dispatch-tests

The system MUST provide `test/test_tmu_dispatch_processor_mvp.cc` covering:
- submit (enqueue correctness)
- 反压停 fetch (back-pressure signal)
- dep chain (ordering violation detection)
- 环检测 (cycle detection + breaking)

All MUST pass `ctest -R "test_tmu_dispatch_processor_mvp"`.

#### Scenario: All 4 TMU behaviors verified
- **WHEN** `ctest -R "test_tmu_dispatch_processor_mvp"` runs after s3 W6
- **THEN** submit + back-pressure + dep chain + cycle detection MUST all PASS