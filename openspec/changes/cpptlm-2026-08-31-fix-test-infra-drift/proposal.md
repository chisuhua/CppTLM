# Proposal: cpptlm-2026-08-31-fix-test-infra-drift — 修复全量回归测试基础设施漂移

> **Status**: Proposed（2026-08-31 全量回归审计发现 2 处非阻塞基础设施 bug）
> **Created**: 2026-08-31
> **Discover context**: 2026-08-31 全量回归测试（HEAD=`1d16e5f`）暴露
> **Branch**: 建议直接 main（轻量 follow-up，总工时 < 0.5d）
> **Worktree**: 不需要
> **关联 ADR**: 无（纯基础设施同步，非架构变更）
> **Design Revision 1** (2026-08-31 实施中发现): 原 Bug #1 修复方案（CMakeLists.txt Layer 2 `set_tests_properties`）因 cmake 跨子目录 scope 限制**不可行**，改为 `scripts/test/test.sh` wrapper-level `ctest -E cute` 排除。详见 design.md §1.5。

## Why

2026-08-31 全量回归测试暴露 **2 项测试/构建基础设施漂移失败**（核心 TLM/CPU/Cache/Crossbar/Memory/GPU 仿真代码 100% PASS）：

| # | 失败位置 | 根因 | 严重性 |
|---|---------|------|--------|
| 1 | `ctest #23 cute_hello_tensor` hang 180s+ | PTX-EMU `bench/cute/CMakeLists.txt` 缺 `if(PTXEMU_BUILD_TESTING)` 守卫，即使 CppTLM 端 `set(PTXEMU_BUILD_TESTING OFF FORCE)` 也无法阻止 cute benchmark 被构建 + 注册为 ctest | 中（破坏 ctest gate） |
| 2 | `test/python/test_f12b_smoke.py` 2 fail（`test_f12b_default_off` + `test_f12b_enabled_no_crash`） | HSK-8 Phase 2 Step 4 永久禁用 `--f12b-ld` 后，`src/main.cpp` 仅保留 `f12b_ld=true` 分支的错误日志到 stderr，**丢失**了原 `#if 0` 块 `else` 分支 `[INFO] MemoryBridge disabled` 日志；同时 `test_f12b_smoke.py` 未跟随 HSK-8 决策更新 | 中（破坏 Python gate） |

**回归状态（2026-08-31）**:
- ✅ Catch2 `cpptlm_tests`: **998 用例 / 32600 assertions / ALL PASSED**（比 AGENTS.md 记录的 817/18864 多 181 用例 + 13736 assertions）
- ✅ E2E 仿真: **12/12 PASSED**（hierarchical_2x2, mesh_2x2, mesh_4x4, ring_8, tlm_e2e_test, cpu_tlm_test, crossbar_test, cache_chstream_test, arbiter_tlm_test, traffic_gen_tlm_test, gpu_2gpc_2tpc_2cu, apu_soc_v1）
- ⚠️ ctest: **25/26 PASSED**（仅 #23 hang）
- ⚠️ Python pytest: **261 PASSED / 2 FAILED / 14 subtests passed**

**触发事件**:
- **2026-08-18**: CppTLM HSK-6 ack commit `369cf71`（消费关系废止 + 11 项物理删除 + HSK-5 `advance()` CANCELLED）
- **2026-08-XX**: HSK-8 Phase 2 Step 4 commit `738b412c`（`--f12b-ld` wiring 永久删除）
- **2026-08-31**: 全量回归审计发现测试与日志未同步

**前置**: 无（独立 follow-up，可立即启动）

## What Changes

### 修复 1：PTX-EMU `bench/cute` 测试隔离（CppTLM 端兜底）

**根因**（`external/PTX-EMU/CMakeLists.txt:138-139`）:
```cmake
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/bench/cute/CMakeLists.txt)
    add_subdirectory(bench/cute)        # ← 无 PTXEMU_BUILD_TESTING 守卫
endif()
```
无条件 include，且 `bench/cute/CMakeLists.txt` 内部 `foreach` + `add_executable` + `add_test` 全无守卫。

**双层防御方案**（设计修订：Layer 2 由 CMakeLists.txt 改为 wrapper script）:
- **Layer 1（上游，PR，follow-up）**: PTX-EMU 端 `external/PTX-EMU/CMakeLists.txt:138-139` 加 `if(PTXEMU_BUILD_TESTING OR PROJECT_IS_TOP_LEVEL)` 守卫（与现有 `tests/` 守卫 line 130-137 对称）。**本 change 不开上游 PR**，作为 follow-up 单独追踪（在 PTX-EMU 仓 issue + CppTLM 仓 submodule ref bump）。
- **Layer 2（本地，本 change 实际采用）**: `scripts/test/test.sh:30` 改为 `ctest --output-on-failure -E cute "$@"`，wrapper-level 排除 cute benchmarks。**理由**: cmake 跨子目录 scope 限制使原计划的 CMakeLists.txt `set_tests_properties` 方案失败（`set_tests_properties Can not find test` 错误）。wrapper-level 排除是更简单且无副作用的解决方案。

### 修复 2：HSK-8 `--f12b-ld` observability 日志恢复 + 测试同步

**根因**（`src/main.cpp:113-127`）:
HSK-8 Phase 2 Step 4 删除 wiring 时，**只保留** `if (f12b_ld)` 错误分支，**删除** `else` 分支 `[INFO] --f12b-ld: MemoryBridge disabled (zero regression)` 日志。导致 `test_f12b_default_off` 期望的字符串消失。

**修复**:
- **main.cpp 113-127 行**: 改为 `if (f12b_ld) { stderr + return 1 } else { stdout [INFO] MemoryBridge disabled }`，恢复原 `#if 0` 块 else 分支的 observability
- **test_f12b_smoke.py**:
  - `test_f12b_default_off`: 保留（依赖 main.cpp 恢复日志，自动 PASS）
  - `test_f12b_enabled_no_crash`: **删除**（与 HSK-8 "永久禁用" 决策矛盾 — HSK-8 已明确 "use PtxEmuSubmoduleMVP facade->attach_timing instead"，不存在"enabled 不崩溃"的合法语义）
  - `test_f12b_requires_json_entries`: 保留（仍 PASS）

## Capabilities

### Modified Capabilities

- **`cpptlm-cli-f12b-ld-flag`** (`src/main.cpp`): 启用 `--f12b-ld` 行为不变（仍 rc=1 + stderr `[ERROR] --f12b-ld disabled`），但**未启用时**恢复 `[INFO] --f12b-ld: MemoryBridge disabled (zero regression)` 日志到 stdout（observability 对称）
- **`cpptlm-test-f12b-smoke`** (`test/python/test_f12b_smoke.py`): 3 测试减为 2 测试（删除与 HSK-8 矛盾的 `test_f12b_enabled_no_crash`）
- **`cpptlm-test-ptxemu-bench-isolation`** (`scripts/test/test.sh`): wrapper-level 排除 PTX-EMU cute benchmarks（`ctest -E cute`），防御 cold-start PTX JIT 编译 hang
- **`cpptlm-test-ptxemu-bench-follow-up`** (本 change 跟踪): 上游 PTX-EMU 仓 PR `bench/cute` 守卫（Layer 1 根治），合并后 CppTLM 端 submodule ref bump + 删除 `-E cute`

## Impact

| 文件 | 类型 | 工时 | 验证 |
|------|------|:---:|------|
| `src/main.cpp` | 修改（+2 行） | 0.05d | `ctest` + `pytest test_f12b_smoke.py` PASS |
| `test/python/test_f12b_smoke.py` | 修改（-15 行 + 注释） | 0.05d | `pytest test_f12b_smoke.py` 2/2 PASS |
| `scripts/test/test.sh` | 修改（+1 `-E cute` 标志） | 0.05d | `./scripts/test/test.sh` 21/21 PASS（cute 5 个排除） |
| `openspec/specs/{cli-f12b-flag,ptxemu-bench-isolation,python-f12b-smoke}/spec.md` | 新增 3 delta specs | 0.05d | `openspec validate` PASS |
| `AGENTS.md` / `docs/superpowers/specs/2026-08-21-hsk-8-cpptlm-response.md` | 微调（可选） | 0.05d | 路径同步 |
| **合计** | | **~0.25d** | **全量回归 100% PASS** |

**影响类别**:
- **现有行为变更**: `--f12b-ld` 启用时仍 rc=1（不变），未启用时多一行 `[INFO] MemoryBridge disabled` 日志到 stdout（observability 恢复）
- **测试影响**: 删除 1 个与 HSK-8 决策矛盾的测试用例
- **依赖关系**: 无（独立 follow-up）
- **回退路径**: revert 单 commit 即可，无状态影响

## Cross-Project Coordination Points

| 同步点 | 时间 | 说明 |
|--------|------|------|
| PTX-EMU 端 cute 守卫 PR | 后续（**非本 change 范围**） | 建议 PTX-EMU maintainer 在上游加守卫；本 change CppTLM 端兜底已可独立闭环 |
| HSK-8 决策同步 | ✅ 已闭环 | HSK-6 ack `369cf71` + HSK-8 Phase 2 Step 4 `738b412c` 已确认永久禁用，无需重新 ack |

## Acceptance Criteria

实测结果（2026-08-31 15:53 → 17:28 验证 + 复验）：

- [x] `ctest --test-dir build --output-on-failure -E cute` **20/21 PASS + 1 Not Run**（`test_cpptlm_emulator_dlopen` 因 BUILD_RTL=OFF 未构建，与本 change 无关，pre-existing 状态；5 cute benchmarks 被 wrapper `-E cute` 排除无 hang）
- [x] `pytest test/python/test_f12b_smoke.py -v` **2/2 PASS**（`test_f12b_default_off` + `test_f12b_requires_json_entries`）
- [x] `pytest cpptlm_config/tests/ test/python/ --ignore=test/python/test_f12b_smoke.py` **253 passed + 7 skipped**（7 skipped 为 BUILD_RTL=OFF 基础设施不可用导致，与本 change 无关）
- [x] `./build/bin/cpptlm_tests` **998 test cases / 32600 assertions / ALL PASSED**（比 AGENTS.md 记录 817/18864 多 181 用例 + 13736 assertions）
- [x] **12 E2E config 仿真 12/12 PASS**（hierarchical_2x2, mesh_2x2, mesh_4x4, ring_8, tlm_e2e_test, cpu_tlm_test, crossbar_test, cache_chstream_test, arbiter_tlm_test, traffic_gen_tlm_test, gpu_2gpc_2tpc_2cu, apu_soc_v1）
- [x] `openspec validate cpptlm-2026-08-31-fix-test-infra-drift --strict` **PASSED**（7 deltas across 3 capabilities）
- [x] `scripts/test/docs_sync_check.sh --strict` **PASSED**（4 文档 / 355 路径引用 / 0 缺失）

**净影响**：
- C++ 测试：998/998 PASS（基线不退化）
- Python 测试：255/2 PASS（基线不退化，f12b_smoke 从 fail 改为 2/2 PASS）
- ctest gate：5 cute hang 风险已消除（wrapper `-E cute`）
- E2E：12/12 PASS（基线不退化）