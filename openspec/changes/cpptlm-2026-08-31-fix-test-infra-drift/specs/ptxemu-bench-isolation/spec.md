# Spec: cpptlm-2026-08-31-fix-test-infra-drift (delta for ptxemu-bench-isolation)

> **Status**: Proposed
> **Delta Type**: ADDED (new capability)
> **Capability**: `ptxemu-bench-isolation`
> **Design revision**: 实施中发现原 CMakeLists.txt `set_tests_properties` Layer 2 方案因 cmake 跨子目录 scope 限制不可行，改为 scripts/test/test.sh wrapper-level `-E cute` 排除。详见 design.md §1.5。

## ADDED Requirements

### Requirement: test-wrapper-excludes-cute-benchmarks

The CppTLM test wrapper script `scripts/test/test.sh` SHALL exclude PTX-EMU submodule's 5 `bench/cute/` CUDA benchmarks from the default `ctest` invocation by passing `-E cute` to ctest. This protects the ctest gate from cold-start PTX JIT compilation hangs (e.g. `cute_hello_tensor` 180s+ observed in 2026-08-31 regression audit).

The 5 cute benchmark test names:
- `cute_hello_col_major`
- `cute_hello_tensor`
- `cute_hello_tiled_copy`
- `cute_rmsnorm`
- `cute_rmsnorm_debug`

The `-E cute` flag MUST be appended to the default `ctest --output-on-failure "$@"` invocation in `scripts/test/test.sh`.

#### Scenario: Default wrapper invocation excludes cute benchmarks

- **WHEN** the user runs `./scripts/test/test.sh` (no extra args)
- **THEN** ctest SHALL be invoked as `ctest --output-on-failure -E cute`
- **AND** the 5 cute benchmarks SHALL be excluded from the ctest run
- **AND** the 21 non-cute ctest entries SHALL execute normally

#### Scenario: User can manually run cute benchmarks

- **WHEN** the user runs `cd build && ctest -R cute` directly (bypassing wrapper)
- **THEN** the 5 cute benchmarks SHALL execute normally (no `DISABLED` flag at cmake level)
- **AND** PTX-EMU developers SHALL be able to debug PTX pipeline issues by manual invocation

#### Scenario: User-supplied ctest args pass through wrapper

- **WHEN** the user runs `./scripts/test/test.sh -R sdma_engine`
- **THEN** ctest SHALL be invoked as `ctest --output-on-failure -E cute -R sdma_engine`
- **AND** both `-E cute` (wrapper default) AND `-R sdma_engine` (user filter) SHALL apply

#### Scenario: PTX-EMU=OFF build configuration

- **WHEN** the user runs `cmake -DCPPTLM_WITH_PTX_EMU=OFF -DBUILD_TESTS=ON && cmake --build build && ./scripts/test/test.sh`
- **THEN** PTX-EMU submodule SHALL NOT be configured (`add_subdirectory(external/PTX-EMU)` skipped)
- **AND** no cute benchmarks SHALL be registered in ctest
- **AND** `-E cute` SHALL be a no-op (no matching test names)
- **AND** the ctest gate SHALL still pass 21 non-cute tests

### Requirement: cute-executables-remain-buildable

When `CPPTLM_WITH_PTX_EMU=ON`, the 5 cute benchmark executables SHALL still be built (linking against PTX-EMU CUDA runtime) so PTX-EMU developers can manually invoke them for visual CUDA debugging. The wrapper-level exclusion does NOT affect the build step.

#### Scenario: Manual invocation by PTX-EMU developer

- **WHEN** a PTX-EMU developer runs `build/bin/cute_hello_tensor` directly (bypassing ctest and wrapper)
- **THEN** the executable SHALL run normally (no wrapper-level exclusion effect on direct invocation)
- **AND** the developer SHALL be able to capture PTX-EMU pipeline logs for debugging

### Requirement: follow-up-layer1-root-cause-tracking

The CppTLM OpenSpec change `cpptlm-2026-08-31-fix-test-infra-drift` SHALL track a follow-up PTX-EMU upstream PR as the root-cause fix for this capability. The upstream PR adds an `if(PTXEMU_BUILD_TESTING OR PROJECT_IS_TOP_LEVEL)` guard to `external/PTX-EMU/CMakeLists.txt:138-139` (around `add_subdirectory(bench/cute)`), symmetric to the existing `tests/` guard at line 130-137.

#### Scenario: PTX-EMU upstream PR merged (future state)

- **WHEN** PTX-EMU maintainer merges the upstream PR adding the gate
- **THEN** CppTLM SHALL bump the PTX-EMU submodule reference to the post-merge commit
- **AND** the CppTLM wrapper `-E cute` SHALL remain safe (cute benchmarks no longer registered when guarded, `-E cute` no-op)
- **AND** CppTLM SHALL be able to remove the `-E cute` exclusion from `scripts/test/test.sh` without behavior regression