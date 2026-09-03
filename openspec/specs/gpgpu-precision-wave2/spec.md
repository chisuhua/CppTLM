# gpgpu-precision-wave2 Specification

## Purpose
TBD - created by archiving change 2026-09-03-cpptlm-dgpu-gpgpu-precision-wave2. Update Purpose after archive.
## Requirements
### Requirement: calibrated-compute-latency

CppTLM SHALL 将 `PipelineTLM` 与 `TensorCoreTLM` 的 Phase 1 placeholder latency 替换为可追溯的 gpgpu-sim 参考值，并在校准完成后令 `is_placeholder()` 返回 `false`。

#### Scenario: Pipeline latency is calibrated

- **WHEN** 查询非 TC 指令的 Pipeline latency
- **THEN** 返回 gpgpu-sim 参考值，允许误差不超过 ±15%
- **AND** TC 指令由 TensorCoreTLM 负责时，Pipeline latency 返回 `0.0`

#### Scenario: Tensor core latency is calibrated

- **WHEN** 查询 FP4、FP6、FP8、FP16、BF16 或 TF32 的 TensorCore latency/throughput
- **THEN** 返回对应 gpgpu-sim 参考值，允许误差不超过 ±15%
- **AND** `is_placeholder()` 返回 `false`

### Requirement: dual-end-cycle-validation

系统 SHALL 提供条件链接 PTX-EMU 的双端集成测试，验证 CppTLM 与 PTX-EMU 的 cycle 契约及 warp stall 生命周期。

#### Scenario: Cycle contract is preserved

- **WHEN** CppTLM `KernelLaunchTLM::tick()` 推进 PTX-EMU
- **THEN** 每次 CppTLM tick 遵守 1 tick = 1 `GPUContext::exe_once()` 的契约
- **AND** `blocked_cycles_remaining` 与 CppTLM 独立模型差值不超过 1 cycle

#### Scenario: Warp stall lifecycle completes

- **WHEN** warp 因 scoreboard/pipeline hazard 被阻塞
- **THEN** `exe_once` 完成 stall → re-schedule → release → re-issue 完整循环
- **AND** `set_blocked_cycles_for_active()` 只作用于该 warp 的活跃线程

### Requirement: gpgpu-sim-comparison

系统 SHALL 提供五类 microbenchmark（FFMA、MUFU、LDG、STG、IMAD）并与 gpgpu-sim 参考结果比较。

#### Scenario: Benchmark variance is bounded

- **WHEN** 在 OFF 与 PTX-EMU 测试路径执行五类 microbenchmark
- **THEN** 每类 benchmark 的结果与 gpgpu-sim 参考值误差不超过 ±15%
- **AND** 失败输出包含 benchmark 名称、CppTLM 值、参考值和偏差百分比

