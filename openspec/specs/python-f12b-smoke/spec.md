# python-f12b-smoke Specification

## Purpose
Validate the `cpptlm_cli --f12b-ld` flag contract via subprocess tests in `test/python/test_f12b_smoke.py`. Reflects HSK-8 Phase 2 Step 4 (commit `738b412c`) decision that `--f12b-ld` is permanently disabled: 2 retained tests assert observability + nonzero-exit contracts, and the legacy conflicting test (`test_f12b_enabled_no_crash`) is removed.

## Requirements
### Requirement: python-f12b-smoke-tests

The Python test file `test/python/test_f12b_smoke.py` SHALL expose exactly 2 test methods validating the `cli-f12b-flag` contract (see `cli-f12b-flag/spec.md`): `test_f12b_default_off` + `test_f12b_requires_json_entries`. The legacy `test_f12b_enabled_no_crash` test MUST NOT exist because it asserts on an unreachable post-HSK-8 state.

#### Scenario: test_f12b_default_off passes with restored observability log

- **WHEN** pytest executes `TestF12bSmoke.test_f12b_default_off`
- **THEN** the test SHALL spawn `cpptlm_sim configs/vector_add_n1024.json --cycles 100` (no `--f12b-ld`)
- **AND** SHALL assert that stdout contains the literal substring `MemoryBridge disabled`
- **AND** SHALL assert that returncode = 0
- **AND** the assertion SHALL pass (depends on `src/main.cpp` else-branch emitting `[INFO] --f12b-ld: MemoryBridge disabled (zero regression)`)

#### Scenario: test_f12b_requires_json_entries passes with HSK-8 disabled behavior

- **WHEN** pytest executes `TestF12bSmoke.test_f12b_requires_json_entries`
- **THEN** the test SHALL write a temporary config file `test_minimal_config.json` with content `{"modules":[{"name":"mem","type":"MemoryTLM"}],"connections":[]}`
- **AND** SHALL spawn `cpptlm_sim <min_config> --cycles 10 --f12b-ld`
- **AND** SHALL assert that returncode != 0 (HSK-8 permanent disable → immediate rc=1)
- **AND** SHALL remove the temporary config file in a `finally` block (cleanup)

#### Scenario: pytest collection reports exactly 2 tests

- **WHEN** pytest collects tests from `test/python/test_f12b_smoke.py`
- **THEN** `TestF12bSmoke` SHALL expose exactly 2 test methods: `test_f12b_default_off` + `test_f12b_requires_json_entries`
- **AND** SHALL NOT expose `test_f12b_enabled_no_crash` (LEGACY REMOVED: it asserted `MemoryBridge enabled` + rc=0 on an unreachable state; HSK-8 permanently disabled all wiring)
- **AND** `pytest test/python/test_f12b_smoke.py -v` SHALL report 2 tests collected / 2 passed