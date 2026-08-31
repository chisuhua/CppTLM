# Design: cpptlm-2026-08-31-fix-test-infra-drift — 测试基础设施漂移修复

> **Status**: Proposed
> **Parent**: [`proposal.md`](./proposal.md)
> **前置**: 无
> **关联**:
> - HSK-6 CppTLM ack: commit `369cf71` (2026-08-18)
> - HSK-8 Phase 2 Step 4: commit `738b412c`（`--f12b-ld` wiring 永久删除）
> - CppTLM `src/main.cpp:113-167`（f12b 决策落地）

## 1. 修复 1：PTX-EMU cute benchmark 隔离

### 1.1 当前 ctest 配置状态

```bash
$ grep "CPPTLM_WITH_PTX_EMU\|PTXEMU_BUILD_TESTING" build/CMakeCache.txt
BUILD_TESTS:BOOL=ON
CPPTLM_WITH_PTX_EMU:BOOL=ON             # ← 当前 build/ 是 PTX-EMU ON
PTXEMU_BUILD_TESTING:BOOL=OFF            # ← CppTLM 强制关闭
```

```bash
$ cat build/CTestTestfile.cmake | grep subdirs
subdirs("src")
subdirs("test")
subdirs("examples")
subdirs("samples")
subdirs("scripts")
subdirs("external/PTX-EMU")             # ← PTX-EMU 子模块被 include
```

### 1.2 CppTLM 端守卫已生效

`CMakeLists.txt:162-176`:
```cmake
if(CPPTLM_WITH_PTX_EMU)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/PTX-EMU/CMakeLists.txt")
        message(FATAL_ERROR ...)         # ← 守卫 1: submodule 必须存在
    endif()
    set(PTXEMU_BUILD_TESTING OFF CACHE BOOL "..." FORCE)   # ← 守卫 2: 强制关闭测试
    add_subdirectory(external/PTX-EMU)
    if(NOT TARGET ptxemu_core)
        message(FATAL_ERROR ...)
    endif()
endif()
```

### 1.3 PTX-EMU 端 tests/ 守卫已生效

`external/PTX-EMU/CMakeLists.txt:130-137`:
```cmake
if(PROJECT_IS_TOP_LEVEL OR PTXEMU_BUILD_TESTING)
    add_subdirectory(tests)              # ← PTXEMU_BUILD_TESTING=OFF 时跳过
endif()
```

### 1.4 PTX-EMU 端 bench/cute 守卫缺失（Bug #1 真实根因）

`external/PTX-EMU/CMakeLists.txt:138-139`:
```cmake
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/bench/cute/CMakeLists.txt)
    add_subdirectory(bench/cute)         # ← ❌ 无 PTXEMU_BUILD_TESTING 守卫
endif()
```

`external/PTX-EMU/bench/cute/CMakeLists.txt` 内部:
```cmake
file(GLOB CUTE_CUDA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cu")
foreach(CUDA_FILE ${CUTE_CUDA_FILES})
    get_filename_component(TEST_NAME ${CUDA_FILE} NAME_WE)
    add_executable(${FULL_TEST_NAME} "")
    # ...
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})         # ← 无条件注册 ctest
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "cute")
endforeach()
```

### 1.5 修复策略：实施中发现的双层防御

**Layer 1（上游 PTX-EMU，本 change **不开** PR — 作为 follow-up 追踪）**:
- `external/PTX-EMU/CMakeLists.txt:138-139` 当前无条件 `add_subdirectory(bench/cute)`，应改为：
```cmake
if((PTXEMU_BUILD_TESTING OR PROJECT_IS_TOP_LEVEL) AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/bench/cute/CMakeLists.txt)
```
- 与 PTX-EMU 端现有 `tests/` 守卫（line 130-137）**完全对称**
- 价值：根治 cute benchmark 不再污染 CppTLM ctest gate
- 决策：**延后到上游 PR**（本 change 范围内不做 submodule 文件改动 — submodule ref bump 需独立提交）

**Layer 2（本地 CppTLM，本 change 实施 — 实际采用 wrapper-level 排除）**:

**设计变更**：原 design.md 设想的 CMakeLists.txt `set_tests_properties(... DISABLED TRUE)` 方案在实施中**遇到 cmake 跨子目录 scope 限制**：
- CppTLM 顶层调用 `set_tests_properties` **无法** 访问 PTX-EMU submodule 子目录（bench/cute）中 `add_test` 注册的 test
- cmake 报错：`set_tests_properties Can not find test to add properties to: cute_hello_col_major`
- 这是 cmake nested project 的固有行为，不是 bug

**替代方案（实际采用）**：在 `scripts/test/test.sh` wrapper 中追加 `ctest -E cute` 排除：

```bash
# scripts/test/test.sh line 30
ctest --output-on-failure -E cute "$@"
```

**为什么 wrapper-level 排除能闭环**:
- ✅ 默认 CppTLM ctest gate 不跑 cute（防 cold-start PTX JIT 编译 hang）
- ✅ 开发者手动跑 cute：`cd build && ctest -R cute` 仍可
- ✅ PTX-EMU=OFF 路径自动 no-op（`-E cute` 不匹配任何 test，无副作用）
- ✅ 不依赖 cmake 跨子目录 hack，干净
- ⚠️ cute test 在 `ctest -N` 仍可见（不影响 gate 行为，但用户可能注意到）

## 2. 修复 2：HSK-8 `--f12b-ld` observability 日志恢复 + 测试同步

### 2.1 HSK-8 Phase 2 Step 4 行为契约（来自 main.cpp + HSK-6/HSK-8 spec）

| 场景 | 行为 | 输出位置 |
|------|------|---------|
| 不带 `--f12b-ld` | 默认 rc=0，仿真正常运行，stdout **应**打印 `[INFO] --f12b-ld: MemoryBridge disabled (zero regression)` | stdout |
| 带 `--f12b-ld` | rc=1，stderr 打印 `[ERROR] --f12b-ld disabled (HSK-8 Phase 2 Step 4 removed MemoryBridge + g_ptx_emu_driver; use PtxEmuSubmoduleMVP facade->attach_timing instead)` | stderr |
| 带 `--f12b-ld` 但缺 kernel_launch JSON entry | rc=1（继承 #if 0 块 else 分支语义，但当前 main.cpp 在该分支前已 return 1，所以走"disabled"路径） | stderr |

### 2.2 main.cpp diff（精确）

**Before** (`src/main.cpp:113-127`):
```cpp
    // HSK-8 Phase 2 Step 4: --f12b-ld wiring block disabled (MemoryBridge + g_ptx_emu_driver
    // removed).
    std::vector<std::unique_ptr<ScoreboardTLM>> per_sm_scoreboards;
    std::vector<std::unique_ptr<PipelineTLM>> per_sm_pipelines;
    std::vector<std::unique_ptr<TensorCoreTLM>> per_sm_tensorcores;

    if (f12b_ld) {
        std::cerr << "[ERROR] --f12b-ld disabled (HSK-8 Phase 2 Step 4 removed "
                     "MemoryBridge + g_ptx_emu_driver; use PtxEmuSubmoduleMVP "
                     "facade->attach_timing instead)\n";
        return 1;
    }
    (void)per_sm_scoreboards;
    (void)per_sm_pipelines;
    (void)per_sm_tensorcores;
```

**After** (恢复原 #if 0 块 else 分支的 observability 日志):
```cpp
    // HSK-8 Phase 2 Step 4: --f12b-ld wiring block disabled (MemoryBridge + g_ptx_emu_driver
    // removed).
    std::vector<std::unique_ptr<ScoreboardTLM>> per_sm_scoreboards;
    std::vector<std::unique_ptr<PipelineTLM>> per_sm_pipelines;
    std::vector<std::unique_ptr<TensorCoreTLM>> per_sm_tensorcores;

    if (f12b_ld) {
        std::cerr << "[ERROR] --f12b-ld disabled (HSK-8 Phase 2 Step 4 removed "
                     "MemoryBridge + g_ptx_emu_driver; use PtxEmuSubmoduleMVP "
                     "facade->attach_timing instead)\n";
        return 1;
    } else {
        // Observability 日志（恢复自 #if 0 块 else 分支）：
        // 保留 [INFO] MemoryBridge disabled 输出，让 test_f12b_smoke.py::test_f12b_default_off
        // 等烟雾测试可验证 "零退化" 契约。
        std::cout << "[INFO] --f12b-ld: MemoryBridge disabled (zero regression)\n";
    }
    (void)per_sm_scoreboards;
    (void)per_sm_pipelines;
    (void)per_sm_tensorcores;
```

**净增**: 4 行（含注释）。

### 2.3 test_f12b_smoke.py diff

**Before** (`test/python/test_f12b_smoke.py` 全文 72 行):
- `test_f12b_default_off`: 期望 stdout 含 "MemoryBridge disabled" + rc=0
- `test_f12b_enabled_no_crash`: 期望 stdout 含 "MemoryBridge enabled" + rc=0 ← 与 HSK-8 矛盾
- `test_f12b_requires_json_entries`: 期望 rc != 0

**After** (删除矛盾用例 + 文件 docstring 同步 HSK-8):
- `test_f12b_default_off`: 保留（依赖 main.cpp 恢复日志，自动 PASS）
- `test_f12b_enabled_no_crash`: **删除**（HSK-8 永久禁用，无合法 "enabled" 状态；保留即持续 fail）
- `test_f12b_requires_json_entries`: 保留（rc=1 仍 PASS）

**文件 docstring 更新**:
```python
"""test_f12b_smoke.py — F12b-LD 基础烟雾测试 (HSK-8 Phase 2 Step 4 后)

HSK-8 Phase 2 Step 4 (commit 738b412c) 已永久禁用 --f12b-ld wiring。
本文件验证:
  - 默认 (无 --f12b-ld) cpptlm_sim 正常运行 (零退化), stdout 含 observability 日志
  - --f12b-ld 触发时 rc != 0 (stderr 禁用错误) 而非崩溃

历史: 原 3 用例中 test_f12b_enabled_no_crash (期望 "MemoryBridge enabled" + rc=0)
      与 HSK-8 决策矛盾, 已删除 (HSK-8 永久禁用, 无 "enabled" 合法语义)。
"""
```

### 2.4 净影响

- main.cpp: +5 行（恢复 observability 日志）
- test_f12b_smoke.py: -18 行 + 7 行（删除矛盾用例 + docstring 同步）= -11 行
- CMakeLists.txt: +12 行（Layer 2 兜底守卫）
- 净 LOC: **+6 行**（极轻量）

## 3. 为何不开上游 PTX-EMU PR

**Layer 1（上游）**虽然根治但需权衡：
- **跨仓 PR 协调成本**: 需 fork PTX-EMU repo → 创建 branch → 提交 PR → 等待 maintainer review → rebase CppTLM submodule → 验证
- **依赖关系破坏**: PTX-EMU submodule 锁定 `fcdad151+`（`CMakeLists.txt:166` 注释明示），PR 合入前 CppTLM 端无法消费修复
- **Layer 2 已可独立闭环**: CppTLM 端 `if(TARGET ...)` 守卫天然兼容上游修复（PR 合入后，Layer 2 的 `if(TARGET ...)` 检测到 cute benchmark 不再被注册，foreach 自然无操作；PR 合入前的"中间态"也能维持 ctest gate）

**结论**: Layer 2 兜底足以闭环，Layer 1 作为后续 PTX-EMU upstream PR 单独追踪（建议在 PTX-EMU 仓 issue 中创建 `bench/cute test isolation` ticket）。

## 4. 验证矩阵

| 验证 | 命令 | 期望 |
|------|------|------|
| **AC-1**: ctest 全量 | `ctest --test-dir build --output-on-failure` | 26/26 PASS（无 hang） |
| **AC-2**: f12b smoke | `pytest test/python/test_f12b_smoke.py -v` | 2/2 PASS |
| **AC-3**: Catch2 全量基线 | `./build/bin/cpptlm_tests` | 998/998 PASS |
| **AC-4**: Python 全量基线 | `pytest cpptlm_config/tests/ test/python/ --ignore=test/python/test_f12b_smoke.py` | 261/261 PASS |
| **AC-5**: E2E 基线 | 12 configs 各 `--cycles 100` | 12/12 PASS |
| **AC-6**: OpenSpec validate | `openspec validate cpptlm-2026-08-31-fix-test-infra-drift --strict` | 0 错 |
| **AC-7**: Doc sync | `./scripts/test/docs_sync_check.sh --strict` | 0 错 |
| **AC-8**: main.cpp 编译 | `cmake --build build -j$(nproc)` | 0 错 |

## 5. 回退路径

- 单 commit revert 即可
- main.cpp 回退 → 失去 observability 日志（与 HSK-8 `#if 0` 块 else 分支一致 — HSK-8 当时就是删掉了，所以 revert 等于回到现状）
- test_f12b_smoke.py 回退 → 3 用例恢复，2 个 fail 复现（与现状一致）
- CMakeLists.txt 回退 → cute benchmark 重新启用（与现状一致）
- **无状态影响**，纯 revert 安全