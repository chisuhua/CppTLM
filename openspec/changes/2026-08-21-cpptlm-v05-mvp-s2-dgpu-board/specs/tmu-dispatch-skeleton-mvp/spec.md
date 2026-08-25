# Spec: TmuDispatchProcessor Skeleton (s2 T-s2-3b)

> **Status**: Proposed — 2026-08-21
> **Scope**: s2 change — TMU 骨架（反压 + set_handler 注入接口，s3 填充 dep chain + 真实 handler）
> **关联**: Oracle ses_fe0b6e44 修复 CRITICAL s2 逆依赖 s3

---

## ADDED Requirements

### Requirement: tmu-dispatch-skeleton-interface

The system MUST provide a `TmuDispatchProcessor` class in `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `src/tlm/gpu/tmu_dispatch_processor_mvp.cc` (~150 LOC 骨架) with:
- `set_handler(std::unique_ptr<TmuHandlerInterface> handler)` — s3 注入实际 handler
- Internal capacity management (32 pending slots)
- Back-pressure signal to SubmitQueue when capacity full
- s2 no-op: tick() 仅管理容量状态，不实际调度

The system MUST also provide `include/tlm/gpu/tmu_handler_mvp.hh` declaring pure-virtual `TmuHandlerInterface` (s3 fills).

#### Scenario: TMU skeleton back-pressure when full
- **WHEN** pending slots = 32 (full)
- **THEN** SubmitQueue MUST observe back-pressure signal (s2 no-op: capacity tracking only)

#### Scenario: set_handler injection point exists
- **WHEN** s3 calls `set_handler(std::make_unique<TmuHandlerImpl>())`
- **THEN** TMU MUST hold the handler pointer (s3 填充后实际 dispatch 到 handler)

### Requirement: tmu-dispatch-skeleton-test

The system MUST provide `test/test_tmu_dispatch_processor_mvp_skeleton.cc` validating back-pressure + capacity management (s3 replaces with real submit/dep-chain/cycle-detection tests).

#### Scenario: Skeleton test passes back-pressure + capacity
- **WHEN** `ctest -R "test_tmu_dispatch_processor_mvp_skeleton"` runs
- **THEN** it MUST PASS (s2 W3 acceptance for TMU 骨架)