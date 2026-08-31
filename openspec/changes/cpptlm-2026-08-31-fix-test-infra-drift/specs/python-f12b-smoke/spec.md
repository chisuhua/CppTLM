# Spec: cpptlm-2026-08-31-fix-test-infra-drift (delta for python-f12b-smoke)

> **Status**: Proposed
> **Delta Type**: ADDED (new capability) + REMOVED Requirement (test deletion)
> **Capability**: `python-f12b-smoke`

## ADDED Requirements

### Requirement: f12b-default-off-prints-observability

The `test_f12b_default_off` test in `test/python/test_f12b_smoke.py` SHALL verify that invoking `cpptlm_sim` without `--f12b-ld` produces the `cli-f12b-flag` observability log line on stdout and exits with returncode 0.

#### Scenario: test_f12b_default_off passes with restored observability log

- **WHEN** pytest executes `TestF12bSmoke.test_f12b_default_off`
- **THEN** the test SHALL spawn `cpptlm_sim configs/vector_add_n1024.json --cycles 100` (no `--f12b-ld`)
- **AND** SHALL assert that stdout contains the literal substring `MemoryBridge disabled`
- **AND** SHALL assert that returncode = 0
- **AND** the assertion SHALL pass (depends on `src/main.cpp` else-branch emitting `[INFO] --f12b-ld: MemoryBridge disabled (zero regression)`)

### Requirement: f12b-flag-with-bare-config-returns-nonzero

The `test_f12b_requires_json_entries` test in `test/python/test_f12b_smoke.py` SHALL verify that invoking `cpptlm_sim --f12b-ld` with a minimal config (no `kernel_launch` field) returns non-zero without crashing.

#### Scenario: test_f12b_requires_json_entries passes with HSK-8 disabled behavior

- **WHEN** pytest executes `TestF12bSmoke.test_f12b_requires_json_entries`
- **THEN** the test SHALL write a temporary config file `test_minimal_config.json` with content `{"modules":[{"name":"mem","type":"MemoryTLM"}],"connections":[]}`
- **AND** SHALL spawn `cpptlm_sim <min_config> --cycles 10 --f12b-ld`
- **AND** SHALL assert that returncode != 0 (HSK-8 permanent disable → immediate rc=1)
- **AND** SHALL remove the temporary config file in a `finally` block (cleanup)

### Requirement: f12b-smoke-test-count-is-two

The test file `test/python/test_f12b_smoke.py` SHALL expose exactly 2 test methods after the HSK-8 Phase 2 Step 4 alignment (no third test). The legacy `test_f12b_enabled_no_crash` test was removed because it asserted on an unreachable state.

#### Scenario: pytest collection reports 2 tests

- **WHEN** pytest collects tests from `test/python/test_f12b_smoke.py`
- **THEN** the test class `TestF12bSmoke` SHALL expose exactly 2 test methods: `test_f12b_default_off` + `test_f12b_requires_json_entries`
- **AND** `pytest test/python/test_f12b_smoke.py -v` SHALL report 2 tests collected

## REMOVED Requirements

### Requirement: f12b-enabled-no-crash-test

> **REMOVED reason**: HSK-8 Phase 2 Step 4 (commit `738b412c`) permanently disabled `--f12b-ld`. The legacy test asserted that `cpptlm_sim --f12b-ld` prints `MemoryBridge enabled` on stdout with returncode 0, but HSK-8 removed all wiring for this flag (the `MemoryBridge` class itself was physically deleted in commit `369cf71`). Retaining the test would cause permanent CI failure. Actual bridge functionality is now provided by `PtxEmuSubmoduleMVP facade->attach_timing` (per HSK-8 spec).

The legacy `test_f12b_enabled_no_crash` test (which asserted `MemoryBridge enabled` + `returncode=0`) MUST be removed from `test/python/test_f12b_smoke.py`.

#### Scenario: File no longer contains test_f12b_enabled_no_crash

- **WHEN** pytest collects tests from `test/python/test_f12b_smoke.py`
- **THEN** `TestF12bSmoke` SHALL NOT expose `test_f12b_enabled_no_crash`
- **AND** `pytest test/python/test_f12b_smoke.py -v` SHALL report 2 tests (not 3)