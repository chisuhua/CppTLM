# Tasks: cpptlm-2026-08-31-fix-test-infra-drift — 修复全量回归测试基础设施漂移

> **Status**: Proposed
> **Parent**: [`proposal.md`](./proposal.md) + [`design.md`](./design.md)
> **前置**: 无（独立 follow-up）
> **总工时**: ~0.3d
> **Acceptance**: 见 `proposal.md` § Acceptance Criteria + `design.md` §4 验证矩阵

## Task 1: 修复 `src/main.cpp` 恢复 observability 日志（~0.05d）

- [x] 1.1 打开 `src/main.cpp`，定位 line 113-127（`if (f12b_ld)` 当前分支 + `(void)per_sm_xxx;` 末尾）
- [x] 1.2 在 `if (f12b_ld) { ... return 1; }` 后**追加** `else { std::cout << "[INFO] --f12b-ld: MemoryBridge disabled (zero regression)\n"; }` 分支
- [x] 1.3 验证编译：`cmake --build build -j$(nproc)` 0 错
- [x] 1.4 验证行为：`build/bin/cpptlm_sim configs/vector_add_n1024.json --cycles 100` stdout 含 `MemoryBridge disabled` + rc=0
- [x] 1.5 验证禁用：`build/bin/cpptlm_sim configs/vector_add_n1024.json --cycles 100 --f12b-ld` stderr 含 `[ERROR] --f12b-ld disabled` + rc=1

## Task 2: 修复 `test/python/test_f12b_smoke.py` 删除 HSK-8 矛盾用例（~0.05d）

- [x] 2.1 打开 `test/python/test_f12b_smoke.py`，定位 `test_f12b_enabled_no_crash` 方法（line 42-49）
- [x] 2.2 删除 `test_f12b_enabled_no_crash` 整段（包括 docstring + `def` + body + 空行）
- [x] 2.3 更新文件 docstring（line 1-7）说明 HSK-8 Phase 2 Step 4 同步背景 + 1 用例删除原因
- [x] 2.4 验证：`pytest test/python/test_f12b_smoke.py -v` 2/2 PASS

## Task 3: 修复 `scripts/test/test.sh` wrapper 排除 PTX-EMU cute benchmark（~0.05d）

> **设计变更**：实施中测试发现原计划 `CMakeLists.txt` Layer 2 `set_tests_properties` 方案因 cmake 跨子目录 scope 限制不可行（cmake 报错 `Can not find test to add properties to: cute_hello_col_major`）。改为 `scripts/test/test.sh:30` 加 `ctest -E cute` wrapper-level 排除。详见 design.md §1.5 设计修订。

- [x] 3.1 打开 `scripts/test/test.sh`，定位 line 30（`ctest --output-on-failure "$@"`）
- [x] 3.2 在 line 30 改为 `ctest --output-on-failure -E cute "$@"`（追加 `-E cute` 标志）
- [x] 3.3 验证：`./scripts/test/test.sh` 跑全量，cute 5 个被排除，21 个非 cute 测试 PASS（无 hang）
- [x] 3.4 验证 PTX-EMU=OFF 路径：`cmake -S . -B build-off -DCPPTLM_WITH_PTX_EMU=OFF && cmake --build build-off`（`add_subdirectory(external/PTX-EMU)` 跳过，`-E cute` 自动 no-op）
- [x] 3.5 验证 wrapper 行为：用户传 args 时 pass-through（`./scripts/test/test.sh -R sdma_engine` 等）

## Task 4: 全量回归验证（~0.05d）

- [x] 4.1 Catch2 全量：`./build/bin/cpptlm_tests` → **998 test cases / 32600 assertions / ALL PASSED**
- [x] 4.2 Python 全量（除 f12b_smoke）：`pytest cpptlm_config/tests/ test/python/ --ignore=test/python/test_f12b_smoke.py` → **253 passed + 7 skipped**（7 skipped 因 BUILD_RTL=OFF 基础设施不可用，pre-existing）
- [x] 4.3 f12b_smoke 单跑：`pytest test/python/test_f12b_smoke.py -v` → **2 passed**
- [x] 4.4 ctest 全量：`ctest --test-dir build --output-on-failure -E cute` → **20/21 PASS + 1 Not Run**（test_cpptlm_emulator_dlopen Not Run 因 BUILD_RTL=OFF pre-existing；5 cute 经 wrapper 排除无 hang）
- [x] 4.5 E2E 仿真：12 configs 各 `--cycles 100` → **12/12 PASS**

## Task 5: OpenSpec 校验 + 文档同步（~0.05d）

- [x] 5.1 OpenSpec validate：`openspec validate cpptlm-2026-08-31-fix-test-infra-drift --strict` → **PASSED**（7 deltas across 3 capabilities: cli-f12b-flag, ptxemu-bench-isolation, python-f12b-smoke）
- [x] 5.2 文档路径同步：`./scripts/test/docs_sync_check.sh --strict` → **PASSED**（4 文档 / 355 路径引用 / 0 缺失）
- [x] 5.3 AGENTS.md / HSK-8 spec 同步（可选）：**未执行**（用户未明确要求，避免范围蔓延）

## Task 6: 提交 + 归档准备（~0.05d）

- [x] 6.1 `git status` 检查 3 个修改文件 + 1 untracked openspec/change 目录
- [x] 6.2 `git diff --stat` 净 LOC 增量确认：**11 insertions / 14 deletions across 3 files**（主变更）+ untracked 6 files (proposal.md / design.md / tasks.md / 3 spec.md) for OpenSpec
- [x] 6.3 提交（**不主动 commit，per HARD BLOCK**）—— 准备 3+1 原子 commit 信息待用户授权

### 6.3 准备的 3+1 commit 信息

**Commit 1: src/main.cpp**（+2/-0）
```
fix(sim): restore --f12b-ld observability log on default invocation

HSK-8 Phase 2 Step 4 (commit 738b412c) permanently disabled --f12b-ld
wiring but inadvertently removed the else-branch observability log.
Restore `[INFO] --f12b-ld: MemoryBridge disabled (zero regression)` to
stdout for default invocation, enabling test_f12b_default_off to verify
the zero-regression contract.

Refs: openspec/changes/cpptlm-2026-08-31-fix-test-infra-drift/specs/cli-f12b-flag/spec.md
```

**Commit 2: test/python/test_f12b_smoke.py**（+5/-16）
```
test(f12b): remove HSK-8 conflicting test case + sync docstring

HSK-8 Phase 2 Step 4 permanently disabled --f12b-ld (MemoryBridge +
g_ptx_emu_driver physically deleted in HSK-6 ack 369cf71). The legacy
test_f12b_enabled_no_crash (asserting "MemoryBridge enabled" + rc=0)
asserts on an unreachable state post-HSK-8, causing permanent CI failure.

- Remove conflicting test_f12b_enabled_no_crash method
- Update module docstring to document HSK-8 sync context
- Retain test_f12b_default_off (now depends on restored observability log)
- Retain test_f12b_requires_json_entries (already passes)

Refs: openspec/changes/cpptlm-2026-08-31-fix-test-infra-drift/specs/python-f12b-smoke/spec.md
```

**Commit 3: scripts/test/test.sh**（+1/-1）
```
fix(test-wrapper): exclude PTX-EMU cute benchmarks from default ctest gate

PTX-EMU submodule's external/PTX-EMU/CMakeLists.txt:138-139 lacks an
if(PTXEMU_BUILD_TESTING OR PROJECT_IS_TOP_LEVEL) guard around
add_subdirectory(bench/cute), causing 5 CUDA PTX simulation benchmarks
(cute_hello_col_major, cute_hello_tensor, cute_hello_tiled_copy,
cute_rmsnorm, cute_rmsnorm_debug) to be unconditionally registered in
CppTLM's ctest gate. These benchmarks can cold-start hang 180s+ during
PTX JIT compilation, blocking CI.

Add `ctest -E cute` to scripts/test/test.sh:30 wrapper invocation to
exclude these benchmarks from the default ctest gate. The change is
contained to CppTLM's wrapper with no submodule modification (cmake
cross-subdirectory scope limitation prevents Layer-2 CMakeLists.txt
set_tests_properties approach). Developers can still invoke cute
benchmarks manually via `cd build && ctest -R cute` for PTX-EMU debug.

Root-cause fix (PTX-EMU upstream PR adding symmetric guard, mirroring
existing tests/ guard at line 130-137) is tracked as a follow-up issue
in this OpenSpec change.

Refs: openspec/changes/cpptlm-2026-08-31-fix-test-infra-drift/specs/ptxemu-bench-isolation/spec.md
```

**Commit 4 (optional, separate): openspec/changes/cpptlm-2026-08-31-fix-test-infra-drift/*** (untracked → new files)
```
docs(openspec): add cpptlm-2026-08-31-fix-test-infra-drift change

OpenSpec change artifacts (proposal.md + design.md + tasks.md + 3
capability specs) documenting the test infrastructure drift fix discovered
during 2026-08-31 full regression audit.

Capabilities:
- cli-f12b-flag: --f12b-ld observability log + permanent disable contract
- ptxemu-bench-isolation: scripts/test/test.sh wrapper-level ctest -E cute exclusion
- python-f12b-smoke: HSK-8-aligned test cases (3 → 2 tests)

Layer 1 follow-up (PTX-EMU upstream PR) tracked in proposal.md Cross-Project
section. Design revision noted in proposal.md: original CMakeLists.txt Layer 2
set_tests_properties approach failed due to cmake cross-subdirectory scope
limitation, pivot to wrapper-level exclusion.

Refs: HSK-6 ack 369cf71, HSK-8 Phase 2 Step 4 738b412c
```

## 后续建议（非本 change 范围）

- [ ] **PTX-EMU 上游 PR**: 提交 `external/PTX-EMU/bench/cute/CMakeLists.txt` 加 `if(PTXEMU_BUILD_TESTING OR PROJECT_IS_TOP_LEVEL)` 守卫（Layer 1 根治）。建议在 PTX-EMU 仓 issue 跟踪。
- [ ] **AGENTS.md § KEY INVARIANTS 段更新**: `cpptlm_tests 764/764 pass (2026-07-03)` 已过时（实际 998/998），需在主仓 README/AGENTS 同步最新数字