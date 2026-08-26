# command-processor-skeleton-mvp Specification

## Purpose
TBD - created by archiving change 2026-08-21-cpptlm-v05-mvp-s2-dgpu-board. Update Purpose after archive.
## Requirements
### Requirement: command-processor-skeleton-interface

The system MUST provide a `CommandProcessor` class in `include/tlm/gpu/command_processor_mvp.hh` + `src/tlm/gpu/command_processor_mvp.cc` (~150 LOC 骨架, ~30 LOC no-op 实现) with the following interface:
- `enum class State { IDLE, FETCH, DECODE, DISPATCH, COMPLETE }`
- `State state() const { return state_; }`
- `void wake()` — DGpuBoardTLM 在 Doorbell ring 后调用
- `void tick()` — 5-state FSM 主入口（s2 no-op: 仅状态切换, 无 method packet 解析）
- `void set_decoder(std::unique_ptr<Pm4DecoderInterface> decoder)` — s3 注入实际实现

#### Scenario: CP skeleton 5-state transitions
- **WHEN** `wake()` is called
- **THEN** state MUST transition IDLE → FETCH
- **AND** subsequent `tick()` calls MUST advance through FETCH → DECODE → DISPATCH → COMPLETE → IDLE

#### Scenario: set_decoder injection point exists (s3 依赖)
- **WHEN** s3 calls `set_decoder(std::make_unique<Pm4Decoder>())`
- **THEN** CP MUST hold the decoder pointer (s3 填充实现后实际调用 parse_method)

### Requirement: command-processor-skeleton-test

The system MUST provide `test/test_command_processor_mvp_skeleton.cc` validating 5 state transitions and wake behavior (no-op for s2; s3 replaces with real NVIDIA method packet test).

#### Scenario: Skeleton test passes 5 transitions
- **WHEN** `ctest -R "test_command_processor_mvp_skeleton"` runs
- **THEN** it MUST PASS (s2 W3 acceptance for CP 骨架)

