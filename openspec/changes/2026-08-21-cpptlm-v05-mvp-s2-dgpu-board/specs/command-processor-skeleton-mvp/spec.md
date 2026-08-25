# Spec: CommandProcessor Skeleton (s2 T-s2-3a)

> **Status**: Proposed — 2026-08-21
> **Scope**: s2 change — CP 骨架（5-state FSM no-op + set_decoder 注入接口，s3 填充数据面）
> **关键约束**: s2 W3-4 必须创建 CP 类骨架使 DGpuBoardTLM 可独立编译；s3 W5-6 填充 NVIDIA method packet 数据面
> **关联**: Oracle ses_fe0b6e44 修复 CRITICAL s2 逆依赖 s3

---

## ADDED Requirements

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