# Spec: cpptlm-2026-08-31-fix-test-infra-drift

> **Status**: Proposed
> **Change**: cpptlm-2026-08-31-fix-test-infra-drift
> **触发事件**: 2026-08-31 全量回归审计发现 HSK-8 Phase 2 Step 4 永久禁用 `--f12b-ld` 后 observability 漂移

## ADDED Requirements

### Requirement: cli-f12b-flag

The `cpptlm_sim` CLI binary's `--f12b-ld` command-line flag SHALL follow the HSK-8 Phase 2 Step 4 contract (commit `738b412c`): the flag is **permanently disabled** because the underlying `MemoryBridge` + `g_ptx_emu_driver` components have been physically removed (HSK-6 ack commit `369cf71`). The flag SHALL be retained for parser compatibility but MUST NOT activate any wiring.

The CLI MUST emit distinct, predictable output for both code paths so the `python-f12b-smoke` test suite (see `specs/python-f12b-smoke/spec.md`) can assert observability invariants.

#### Scenario: Default (flag absent) — zero regression observability

- **WHEN** the user invokes `cpptlm_sim <config.json> --cycles <N>` without the `--f12b-ld` flag
- **THEN** the simulation SHALL run normally with returncode = 0
- **AND** stdout SHALL contain the literal substring `[INFO] --f12b-ld: MemoryBridge disabled (zero regression)` (observability log emitted by `src/main.cpp` else-branch)
- **AND** stderr SHALL NOT contain `[ERROR] --f12b-ld disabled`

#### Scenario: Flag present — HSK-8 permanent disable

- **WHEN** the user invokes `cpptlm_sim <config.json> --cycles <N> --f12b-ld`
- **THEN** the binary SHALL return immediately with returncode = 1 (no simulation loop entered)
- **AND** stderr SHALL contain the literal substring `[ERROR] --f12b-ld disabled (HSK-8 Phase 2 Step 4 removed MemoryBridge + g_ptx_emu_driver; use PtxEmuSubmoduleMVP facade->attach_timing instead)`
- **AND** stdout SHALL NOT contain `[INFO] --f12b-ld: MemoryBridge enabled` (HSK-8 permanently disabled — no legal "enabled" state exists)
- **AND** the simulation MUST NOT execute any cycles (returncode=1 emitted before `eq.run(sim_cycles)`)

#### Scenario: Flag present with minimal config — single returncode=1 path

- **WHEN** the user invokes `cpptlm_sim <config_without_kernel_launch.json> --cycles <N> --f12b-ld`
- **THEN** the behavior SHALL be identical to "Flag present — HSK-8 permanent disable" (returncode=1, stderr disabled-error)
- **AND** the binary SHALL NOT emit the legacy "requires JSON config with 'kernel_launch'" diagnostic (the entire wiring block is unreachable post-HSK-8)

## ADDED Requirements

### Requirement: ptxemu-bench-isolation

The CppTLM ctest gate SHALL isolate PTX-EMU submodule's `bench/cute/` benchmarks from CppTLM's test runs when `PTXEMU_BUILD_TESTING=OFF`. This protects the ctest gate from hangs caused by PTX-EMU CUDA benchmarks (`cute_hello_tensor` 180s+ hang observed in 2026-08-31 regression).

The PTX-EMU submodule's `external/PTX-EMU/CMakeLists.txt:138-139` does NOT gate `bench/cute/` on `PTXEMU_BUILD_TESTING` (unlike `tests/` which is gated). CppTLM MUST add a Layer-2 fallback in its top-level `CMakeLists.txt:172` to `set_tests_properties(... DISABLED TRUE)` for these benchmarks.

#### Scenario: CPPTLM_WITH_PTX_EMU=ON + PTXEMU_BUILD_TESTING=OFF (CppTLM default)

- **WHEN** the user builds CppTLM with `cmake -DCPPTLM_WITH_PTX_EMU=ON -DBUILD_TESTS=ON`
- **THEN** CppTLM's `set(PTXEMU_BUILD_TESTING OFF ... FORCE)` SHALL propagate to PTX-EMU's `tests/` (which is gated)
- **AND** CppTLM's Layer-2 fallback `foreach(cute_test ... set_tests_properties(... DISABLED TRUE))` SHALL mark all 5 cute benchmarks as DISABLED in ctest
- **AND** `ctest --test-dir build --output-on-failure` SHALL report the 5 cute benchmarks as "Disabled" (not "Passed" and not "Failed")
- **AND** the 5 cute executables SHALL still exist in `build/bin/` for manual developer invocation under `BUILD_DIR=build-on`
- **AND** the Layer-2 fallback SHALL be a no-op when `CPPTLM_WITH_PTX_EMU=OFF` (`if(TARGET ...)` guards prevent side effects)

#### Scenario: Upstream PTX-EMU adds gate (future state)

- **WHEN** an upstream PTX-EMU PR adds `if(PTXEMU_BUILD_TESTING OR PROJECT_IS_TOP_LEVEL)` guard around `add_subdirectory(bench/cute)`
- **THEN** the CppTLM Layer-2 fallback's `if(TARGET ${cute_test})` guard SHALL naturally no-op (cute targets no longer registered when guarded)
- **AND** the ctest gate SHALL remain green with 5 cute benchmarks still DISABLED (defense-in-depth)
- **AND** removing CppTLM's Layer-2 fallback later SHALL be safe (no behavioral regression)

## ADDED Requirements

### Requirement: python-f12b-smoke

The Python test suite `test/python/test_f12b_smoke.py` SHALL validate the `cli-f12b-flag` contract through subprocess invocations of `cpptlm_sim`. The suite SHALL reflect the HSK-8 Phase 2 Step 4 decision: the `--f12b-ld` flag is permanently disabled, so there is no "enabled without crash" state to test.

#### Scenario: test_f12b_default_off passes

- **WHEN** pytest executes `TestF12bSmoke.test_f12b_default_off`
- **THEN** the test SHALL invoke `cpptlm_sim configs/vector_add_n1024.json --cycles 100` (no `--f12b-ld`)
- **AND** SHALL assert stdout contains `MemoryBridge disabled` (depends on `cli-f12b-flag` observability log)
- **AND** SHALL assert returncode = 0

#### Scenario: test_f12b_requires_json_entries passes

- **WHEN** pytest executes `TestF12bSmoke.test_f12b_requires_json_entries`
- **THEN** the test SHALL create a minimal config (`{"modules":[{"name":"mem","type":"MemoryTLM"}],"connections":[]}`) without `kernel_launch`
- **AND** SHALL invoke `cpptlm_sim <min_config> --cycles 10 --f12b-ld`
- **AND** SHALL assert returncode != 0 (HSK-8 permanent disable → immediate rc=1)
- **AND** SHALL clean up the temporary config file in a `finally` block

#### REMOVED Scenario: test_f12b_enabled_no_crash (HSK-8 conflict)

> **REMOVED reason**: HSK-8 Phase 2 Step 4 permanently disabled `--f12b-ld`. The legacy test asserted "stdout contains `MemoryBridge enabled` + returncode=0", which is an unreachable state post-HSK-8 (the `MemoryBridge` class itself was physically deleted in commit `369cf71`). Retaining the test would cause permanent CI failure. Use `PtxEmuSubmoduleMVP facade->attach_timing` for actual bridge functionality (per HSK-8 spec).