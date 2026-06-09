# CppTLM 示例与脚本清理修复

## TL;DR

> **Quick Summary**: 修复 CppTLM 项目 samples/ 和 scripts/ 目录的 4 类已知问题（死路径、漏注册 target、孤儿示例、CI 集成），交付一个干净、可构建、可运行、可被 CI 验证的项目结构。
>
> **Deliverables**:
> - `run_all_tests.sh` EXECUTABLES 列表清理（删除幽灵可执行文件）
> - `samples/CMakeLists.txt` 补全（streaming_demo + 3 Python demo target）
> - `examples/CMakeLists.txt` 补全（example_basic_transaction + example_error_handling target，删除无效守卫）
> - 归档孤儿：`samples/simple1/`、`samples/simple_hier/` → `docs-archived/samples-orphaned/`
> - 5 子目录重构：`scripts/{build,test,pipeline,topology,stats}/` + 调度 `CMakeLists.txt`
> - AGENTS.md 修正（删除不存在的 src/ 文件引用，反映归档）
> - CI 集成：`ci_e2e_test.sh` + `run_all_tests.sh --quick` 加入 `.github/workflows/ci.yml`
> - 清理 `__pycache__/linter.cpython-312.pyc` 残留
>
> **Estimated Effort**: Medium
> **Parallel Execution**: YES — 4 waves (A blockers / B orphans / C scripts restructure / D CI)
> **Critical Path**: A1 → C1 → C2 → D1 → F1-F4 → user okay

---

## Context

### Original Request
"请检查目前的示例项目有哪些，如何运行. 同时整理scripts下的运行脚本"

**用户后续指示**："给出修复计划，并按计划执行"

### Interview Summary

**Key Discussions**:
- 用户确认范围：4 个优先级的全部任务都要执行
- 用户决策：
  - **B-3**：保留 `examples/example_*.cc`，补全 CMake target
  - **C**：完整 5 子目录重构
  - **D**：完整 CI 集成（ci_e2e_test.sh + run_all_tests.sh --quick）

**Research Findings**:
- `samples/CMakeLists.txt` 仅注册 2 个 demo（`stats_demo`、`traffic_gen_demo`）
- `samples/streaming_demo.cc` 有完整 main() 但**未注册**（含 LSP 错误：找不到 `metrics/stats.hh`，因 include 路径未设置）
- 3 个 samples Python demo 完全无 target
- `examples/CMakeLists.txt` 守卫永远不触发的 `cache_system/main.cpp` / `noc_system/main.cpp` 路径
- `examples/example_basic_transaction.cc` / `example_error_handling.cc` 都有 main() 但无 CMake target
- `samples/simple1/` 标 DEPRECATED in v2.1，`cpu_cluster.cc` 显式标注 "not compiled by default samples/CMakeLists.txt"
- `samples/simple_hier/` 无 CMakeLists.txt，孤儿
- `src/cpu_main.cpp` / `traffic_main.cpp` / `sc_main.cpp` 在整个项目**不存在**（AGENTS.md 文档错误）
- `scripts/run_all_tests.sh:7-13` 硬编码 `cpptlm_cpu` / `cpptlm_traffic` 但 src 中无对应 main.cpp
- `scripts/__pycache__/linter.cpython-312.pyc` 残留（`linter.py` 已删）
- `scripts/CMakeLists.txt` 用 `${CMAKE_SOURCE_DIR}/scripts/topology_validator.py` 绝对路径引用 → 重命名需同步
- 所有 samples/*_demo.py 用 `sys.path.insert(0, '..', 'scripts')` 相对路径导入 `topology_generator` 等
- `run_full_pipeline.sh` 用 `python3 "$SCRIPT_DIR/topology_generator.py"` 相对引用

### Metis Review (Self-Executed)

API 配额耗尽，自审识别出以下问题：

**Identified Gaps**（已通过用户决策解决）:
- B-3 处理方式 → 用户决定：补 CMake target
- C 重构范围 → 用户决定：完整 5 子目录
- D CI 范围 → 用户决定：完整集成

**Scope Creep Risks**（已锁定）:
- 不修改 C++ 核心代码（src/core/, include/ 内部）
- 不新增示例功能
- 不重命名对外 API
- 不重命名 `cpptlm_sim` 等可执行文件
- 不修改现有 test/ 测试

**Hidden Risks Identified**:
1. **Python 路径脆弱**：`samples/*_demo.py` 的 `sys.path.insert(0, '..', 'scripts')` 在重构后需改为 `'..', '..', 'scripts'`
2. **scripts/CMakeLists.txt 路径**：`${CMAKE_SOURCE_DIR}/scripts/topology_validator.py` 在重构后需改为 `topology/topology_validator.py`
3. **CI 工作流不引用 scripts/**：`.github/workflows/ci.yml` 只用 `format.sh --check`，新集成需谨慎添加
4. **`streaming_demo.cc` 包含头失败**：`#include "metrics/stats.hh"` 找不到 — 需确认 include 路径或使用绝对路径
5. **孤儿归档可能破坏外部引用**：`git mv` 保留历史，但若 AGENTS.md 引用需同步

---

## Work Objectives

### Core Objective
清理 CppTLM 的 samples/ 和 scripts/ 目录，修复构建/运行链中的死路径、漏注册、孤儿示例，整理脚本结构，集成 CI 验证，使项目可构建、可运行、可被 CI 验证。

### Concrete Deliverables

**Priority A（阻塞修复）**:
- [x] `scripts/run_all_tests.sh` EXECUTABLES 列表删除 `cpptlm_cpu` / `cpptlm_traffic`
- [x] `samples/CMakeLists.txt` 添加 `streaming_demo` target + 3 Python demo target
- [x] `AGENTS.md` 删除 `src/cpu_main.cpp` / `traffic_main.cpp` / `sc_main.cpp` 引用
- [x] `scripts/__pycache__/linter.cpython-312.pyc` 删除

**Priority B（孤儿清理）**:
- [x] `samples/simple1/` → `docs-archived/samples-orphaned/simple1/`（git mv）
- [x] `samples/simple_hier/` → `docs-archived/samples-orphaned/simple_hier/`（git mv）
- [x] `examples/CMakeLists.txt` 添加 `example_basic_transaction` + `example_error_handling` target
- [x] `examples/CMakeLists.txt` 删除无效 `if(EXISTS cache_system/main.cpp)` 守卫
- [x] `AGENTS.md` 反映 simple1/simple_hier 归档

**Priority C（脚本重构）**:
- [x] 创建 `scripts/{build,test,pipeline,topology,stats}/` 5 个子目录
- [x] 移动 6 个 bash 脚本 + 9 个 Python 脚本到对应子目录
- [x] 重组 `scripts/CMakeLists.txt` 调度 5 个子目录的 Python 验证
- [x] 更新所有相对路径引用（samples/*_demo.py、run_full_pipeline.sh 等）
- [x] 验证重构后 `validate_topology` target 仍可工作

**Priority D（CI 集成）**:
- [x] `.github/workflows/ci.yml` build-and-test job 末尾添加 ci_e2e_test.sh 步骤
- [x] `.github/workflows/ci.yml` 新增 PR 友好的 run_all_tests.sh --quick 步骤

### Definition of Done
- [x] `cmake --build build` 成功（无 error）
- [x] `cd build && ctest --output-on-failure` 通过
- [x] `./scripts/format.sh --check` 通过
- [x] 新增的 target 都能构建并运行（stats_demo、traffic_gen_demo、streaming_demo、example_basic_transaction、example_error_handling、3 Python demo）
- [x] `./scripts/ci_e2e_test.sh` 通过
- [x] `./scripts/run_all_tests.sh --quick` 通过
- [x] 重构后 `validate_topology` target 工作
- [x] 0 个 LSP 错误（针对 samples/streaming_demo.cc 和 traffic_gen_demo.cc 的现有 LSP 错误不在本计划范围内 — 由 A2 任务实现 CMake 包含路径间接修复）

### Must Have
- 所有死路径引用彻底删除
- 所有示例有明确运行入口
- 5 子目录重构保持现有功能（run_all_tests.sh、ci_e2e_test.sh、validate_topology 不破）
- CI 集成不破坏现有 workflow

### Must NOT Have (Guardrails)
- ❌ 不修改 src/ 下的 C++ 核心代码
- ❌ 不重命名现有可执行文件
- ❌ 不修改 test/ 下的测试
- ❌ 不创建新的示例功能
- ❌ 不修改 AGENTS.md 的"零债务原则"等约定
- ❌ 不删除 docs-archived 已有内容
- ❌ 不修改 .clang-format
- ❌ 不绕过 BUILD_EXAMPLES=OFF 的用户选择
- ❌ 不修改 include/ 公共头文件

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** - ALL verification is agent-executed. No exceptions.
> Acceptance criteria requiring "user manually tests/confirms" are FORBIDDEN.

### Test Decision
- **Infrastructure exists**: YES (Catch2 v3.7.0, ctest-driven)
- **Automated tests**: tests-after（无新业务逻辑，修复任务）
- **Framework**: ctest (Catch2 后端) + bash 脚本验证 + Python smoke test
- **如果 TDD**: N/A（修复任务，无新逻辑）
- **Agent QA 必填**：每个任务必须含 1+ happy path + 1+ failure scenario

### QA Policy

每个任务必须包含 agent-executed QA scenarios。证据保存到 `.omo/evidence/task-{N}-{scenario-slug}.{ext}`。

- **构建验证**: `cmake --build build` 退出码 0 + 输出无 "error:"
- **测试验证**: `ctest --output-on-failure` 退出码 0
- **格式验证**: `./scripts/format.sh --check` 退出码 0
- **Demo 验证**: 每个新加 target 实际运行一次（接受 PASS/输出非空）
- **路径验证**: grep 确认无残留死路径引用

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately - 阻塞修复 A，独立任务可并行):
├── Task 1:  修复 run_all_tests.sh EXECUTABLES 列表 [quick]
├── Task 2:  补全 samples/CMakeLists.txt（streaming + 3 Python demo）[quick]
├── Task 3:  修正 AGENTS.md（删除不存在的源文件引用）[quick]
├── Task 4:  清理 scripts/__pycache__/linter.cpython-312.pyc 残留 [quick]
└── Task 5:  搜索其他潜在死路径引用（完整性检查）[quick]

Wave 2 (After Wave 1 - 孤儿清理 B):
├── Task 6:  归档 samples/simple1/ 到 docs-archived/samples-orphaned/ [quick]
├── Task 7:  归档 samples/simple_hier/ 到 docs-archived/samples-orphaned/ [quick]
├── Task 8:  补全 examples/CMakeLists.txt（添加 2 个 target + 删除无效守卫）[quick]
├── Task 9:  更新 AGENTS.md 反映归档和 example target [quick]
└── Task 10: 全量构建+测试+格式化验证 [unspecified-low]

Wave 3 (After Wave 2 - 脚本重构 C，依赖 A 路径确定):
├── Task 11: 创建 scripts/ 5 个子目录骨架 [quick]
├── Task 12: 移动 bash 脚本到 scripts/{build,test,pipeline}/ [quick]
├── Task 13: 移动 Python 脚本到 scripts/{topology,stats}/ [quick]
├── Task 14: 重组 scripts/CMakeLists.txt 调度各子目录 [quick]
├── Task 15: 更新 samples/*_demo.py 的 sys.path 引用 [quick]
├── Task 16: 更新 run_full_pipeline.sh 内部脚本引用 [quick]
└── Task 17: 验证重构后所有脚本可工作（ctest + ci_e2e + validate_topology）[unspecified-low]

Wave 4 (After Wave 3 - CI 集成 D，依赖 C 完成):
├── Task 18: 添加 ci_e2e_test.sh 步骤到 .github/workflows/ci.yml [quick]
├── Task 19: 添加 run_all_tests.sh --quick 步骤到 .github/workflows/ci.yml [quick]
└── Task 20: 本地工作流 dry-run 验证（act 或 yaml 校验）[quick]

Wave FINAL (After ALL tasks — 4 parallel reviews):
├── Task F1: Plan Compliance Audit (oracle)
├── Task F2: Code Quality Review (unspecified-high)
├── Task F3: Real Manual QA (unspecified-high)
└── Task F4: Scope Fidelity Check (deep)

Critical Path: Task 1 → Task 2 → Task 10 → Task 11 → Task 14 → Task 17 → Task 20 → F1-F4
Parallel Speedup: ~60% faster than sequential
Max Concurrent: 5 (Wave 1 & 2)
```

### Dependency Matrix

- **1-5**: independent
- **6-9**: depend on A complete (verified by Task 5)
- **10**: depends on 1-9
- **11-13**: depend on 10
- **14-16**: depend on 11-13
- **17**: depends on 14-16
- **18-19**: depend on 17
- **20**: depends on 18-19
- **F1-F4**: depend on 1-20

### Agent Dispatch Summary
- **Wave 1**: 5× `quick`
- **Wave 2**: 4× `quick` + 1× `unspecified-low` (Task 10 验证)
- **Wave 3**: 6× `quick` + 1× `unspecified-low` (Task 17 验证)
- **Wave 4**: 3× `quick`
- **Final**: `oracle` + `unspecified-high` + `unspecified-high` + `deep`

---

## TODOs

> Implementation + Test = ONE Task. Never separate.
> EVERY task MUST have: Recommended Agent Profile + Parallelization info + QA Scenarios.
> **A task WITHOUT QA Scenarios is INCOMPLETE. No exceptions.**
> **FORMAT**: Task labels MUST use bare numbers: `1.`, `2.`, `3.` — NOT `T1.`, `Task 1.`, `Phase 1:`.
> The /start-work progress counter requires exact format. Deviation = progress shows 0/0.
> Final Verification Wave labels MUST use `F1.`, `F2.`, etc. — NOT `T-F1.`, `F-1.`, `Final 1.`.

### Wave 1 — Blocker Fixes (Priority A)

- [x] 1. 修复 `scripts/run_all_tests.sh` EXECUTABLES 列表

  **What to do**:
  - 读取 `/workspace/project/CppTLM/scripts/run_all_tests.sh:7-13`
  - 删除 `cpptlm_cpu` 和 `cpptlm_traffic` 两个条目（源文件不存在，`src/CMakeLists.txt` 用 `if(EXISTS)` 保护所以也不会被构建）
  - 保留 `cpptlm_sim` / `stats_demo` / `traffic_gen_demo`
  - 同时修复 `run_all_tests.sh:131-139` 的 if-elif 分支：删除 `cpptlm_cpu` / `cpptlm_traffic` 对应分支（保留 `cpptlm_sim` 分支和 else 分支）
  - 同时修复 `run_all_tests.sh:73` 的注释或保留（test case count 报告部分不变）

  **Must NOT do**:
  - 不修改其他脚本
  - 不添加新可执行文件
  - 不修改 EXECUTABLES 数组的拼写风格

  **Recommended Agent Profile**:
  > Select category + skills based on task domain. Justify each choice.
  - **Category**: `quick`
    - Reason: 单文件小改动，5-10 行 bash 数组调整
  - **Skills**: `[]`
    - 无需特殊 skill
  - **Skills Evaluated but Omitted**:
    - `verification-before-completion`: 任务完成后自审（不需技能）

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 2, 3, 4, 5)
  - **Blocks**: Task 10 (Wave 2 验证)
  - **Blocked By**: None

  **References** (CRITICAL - Be Exhaustive):
  - `scripts/run_all_tests.sh:7-13` - EXECUTABLES bash 数组定义
  - `scripts/run_all_tests.sh:131-139` - if-elif 分支处理各可执行文件
  - `src/CMakeLists.txt:65-74` - `if(EXISTS)` 保护的源文件列表（确认 cpptlm_cpu/cpptlm_traffic 不会构建）

  **WHY Each Reference Matters**:
  - `scripts/run_all_tests.sh:7-13` 是死路径引用的位置
  - `src/CMakeLists.txt:65-74` 解释为什么 EXECUTABLES 中条目永远不会运行

  **Acceptance Criteria**:

  **If TDD (tests enabled)**: N/A — tests-after

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: EXECUTABLES 列表已清理
    Tool: Bash (grep)
    Preconditions: 仓库根目录
    Steps:
      1. grep -n "cpptlm_cpu\|cpptlm_traffic" scripts/run_all_tests.sh
    Expected Result: 0 个匹配（除了可能保留的注释）
    Failure Indicators: 输出含 cpptlm_cpu 或 cpptlm_traffic
    Evidence: .omo/evidence/task-1-executables-cleaned.txt

  Scenario: bash 语法检查
    Tool: Bash (bash -n)
    Preconditions: 修复后
    Steps:
      1. bash -n scripts/run_all_tests.sh
    Expected Result: 退出码 0，无语法错误
    Failure Indicators: 输出含 "syntax error"
    Evidence: .omo/evidence/task-1-bash-syntax.txt

  Scenario: dry-run 数组打印
    Tool: Bash (source + print)
    Preconditions: 修复后
    Steps:
      1. cd /workspace/project/CppTLM && bash -c "source scripts/run_all_tests.sh 2>/dev/null; declare -p EXECUTABLES" | head -20
    Expected Result: 数组仅含 cpptlm_sim / stats_demo / traffic_gen_demo
    Failure Indicators: 含 cpptlm_cpu 或 cpptlm_traffic
    Evidence: .omo/evidence/task-1-array-dryrun.txt
  ```

  **Evidence to Capture**:
  - [ ] grep 输出确认无死路径
  - [ ] bash -n 通过
  - [ ] 数组 dry-run 输出

  **Commit**: YES
  - Message: `fix(scripts): remove dead executable references from run_all_tests.sh`
  - Files: `scripts/run_all_tests.sh`
  - Pre-commit: `bash -n scripts/run_all_tests.sh`

- [x] 2. 补全 `samples/CMakeLists.txt`（streaming_demo + 3 Python demo）

  **What to do**:
  - 读取当前 `samples/CMakeLists.txt`（10 行）
  - 添加 `add_executable(streaming_demo streaming_demo.cc)` + `target_link_libraries(streaming_demo PRIVATE cpptlm_core)`
  - **关键**: 添加 `target_include_directories(streaming_demo PRIVATE ${CMAKE_SOURCE_DIR}/include)` 解决 `metrics/stats.hh` 找不到问题
  - 同样给现有 `stats_demo` / `traffic_gen_demo` 添加 include 路径（LSP 错误显示这些 demo 也有 include 问题）
  - 添加 3 个 `add_custom_target`:
    - `demo_topology_generator`: `python3 ${CMAKE_SOURCE_DIR}/samples/topology_generator_demo.py`
    - `demo_stats_visualization`: `python3 ${CMAKE_SOURCE_DIR}/samples/stats_visualization_demo.py`
    - `demo_stats_watcher`: `python3 ${CMAKE_SOURCE_DIR}/samples/stats_watcher_demo.py`
  - 这些 demo 目标不应在 `cmake --build` 默认 target 中（避免 Python 错误打断构建），但 `cmake --build build --target demo_*` 可手动触发

  **Must NOT do**:
  - 不修改 samples/*.cc / *.py 内容
  - 不修改 src/CMakeLists.txt
  - 不动 add_subdirectory 调用顺序
  - 不加 ENABLE_DEMOS 选项（项目已用 BUILD_EXAMPLES 控制）

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 1 个 CMakeLists 文件，~15 行追加
  - **Skills**: `["cmake"]`
    - `cmake`: 熟悉 add_executable / target_include_directories / add_custom_target 语法
  - **Skills Evaluated but Omitted**:
    - `verification-before-completion`: 自行 grep+ctest 验证

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 3, 4, 5)
  - **Blocks**: Task 10, 15 (sys.path 更新)
  - **Blocked By**: None

  **References**:
  - `samples/CMakeLists.txt` 当前内容
  - `samples/streaming_demo.cc:16` - 引用 `metrics/stats.hh` 失败
  - `samples/stats_demo.cc:9` - 同样引用 `metrics/stats.hh`
  - `samples/traffic_gen_demo.cc:1` - 引用 `tlm/traffic_gen_tlm.hh`
  - `src/CMakeLists.txt:33-37` - cpptlm_core 的 include 目录设置（已 PUBLIC include include/）
  - `include/metrics/stats.hh` 存在确认
  - `include/tlm/traffic_gen_tlm.hh` 存在确认

  **WHY Each Reference Matters**:
  - 当前 CMakeLists 漏 streaming_demo，3 个 Python demo 缺入口
  - LSP 报告 stats_demo / streaming_demo 找不到 metrics/stats.hh —— 需补 include 路径
  - cpptlm_core 已 PUBLIC 设置 include/，但 streaming_demo 是独立 target，继承需明确

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 新 target 出现在 cmake 配置中
    Tool: Bash (cmake configure)
    Preconditions: 修复 samples/CMakeLists.txt 后
    Steps:
      1. cmake -S . -B build 2>&1 | tee /tmp/cmake-reconf.log
      2. grep -E "streaming_demo|stats_demo|traffic_gen_demo" /tmp/cmake-reconf.log
    Expected Result: 输出含 streaming_demo / stats_demo / traffic_gen_demo 三个 target
    Failure Indicators: 仅含 2 个（说明漏注册）
    Evidence: .omo/evidence/task-2-cmake-configure.log

  Scenario: 新 target 可构建
    Tool: Bash (cmake build)
    Preconditions: 上一场景通过
    Steps:
      1. cmake --build build --target streaming_demo -j$(nproc) 2>&1 | tee /tmp/build-streaming.log
    Expected Result: 退出码 0，无 "error:"
    Failure Indicators: "error:" 或 "undefined reference" 或 "fatal error"
    Evidence: .omo/evidence/task-2-build-streaming.log

  Scenario: stats_demo 和 traffic_gen_demo 也能构建
    Tool: Bash (cmake build)
    Preconditions: include 路径已添加
    Steps:
      1. cmake --build build --target stats_demo -j$(nproc) 2>&1 | tail -5
      2. cmake --build build --target traffic_gen_demo -j$(nproc) 2>&1 | tail -5
    Expected Result: 两个都构建成功
    Failure Indicators: include 错误（metrics/stats.hh 找不到）
    Evidence: .omo/evidence/task-2-build-existing-demos.log

  Scenario: Python demo target 列表
    Tool: Bash (cmake help)
    Preconditions: 修复后
    Steps:
      1. cd build && cmake --build . --target help 2>&1 | grep -E "demo_topology_generator|demo_stats_visualization|demo_stats_watcher"
    Expected Result: 3 个 demo_* target 都列出
    Failure Indicators: 0 个匹配
    Evidence: .omo/evidence/task-2-python-demo-targets.txt

  Scenario: streaming_demo 可执行并产生输出
    Tool: Bash (run binary)
    Preconditions: 上一场景通过
    Steps:
      1. timeout 30 ./build/bin/streaming_demo 2>&1 | head -20
    Expected Result: 输出非空（含 "Streaming" 或 "[INFO]" 字样）
    Failure Indicators: "command not found" 或立即退出无输出
    Evidence: .omo/evidence/task-2-run-streaming-demo.txt
  ```

  **Evidence to Capture**:
  - [ ] cmake 配置日志含 3 个 C++ demo
  - [ ] streaming_demo 构建日志（成功）
  - [ ] stats_demo + traffic_gen_demo 构建日志（确认 include 修复）
  - [ ] cmake help 列出 3 个 demo_* target
  - [ ] streaming_demo 运行输出

  **Commit**: YES
  - Message: `build(samples): add streaming_demo and python demo targets, fix include paths`
  - Files: `samples/CMakeLists.txt`
  - Pre-commit: `cmake --build build --target streaming_demo stats_demo traffic_gen_demo`

- [x] 3. 修正 `AGENTS.md`（删除不存在的源文件引用）

  **What to do**:
  - 读取 `/workspace/project/CppTLM/AGENTS.md`
  - 找到 `src/cpu_main.cpp` / `traffic_main.cpp` / `sc_main.cpp` 三处引用（在文档"STRUCTURE" 段，"src/" 部分，以及可能的 "WHERE TO LOOK" 或 "COMMANDS" 段）
  - 实际定位：用 `grep -n "cpu_main\|traffic_main\|sc_main" AGENTS.md`
  - 删除这三处引用（删除整行或修改为注释）
  - 保留 `src/main.cpp` 描述（确实存在）
  - 修改 `src/main.cpp` 描述以反映 cpptlm_sim 是当前唯一入口

  **Must NOT do**:
  - 不修改 AGENTS.md 的结构/版本号
  - 不修改 "CONVENTIONS" / "ANTI-PATTERNS" / "UNIQUE STYLES" 段
  - 不删除 "零债务原则" 约定
  - 不修改 "COMMANDS" 段中的构建命令

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 文档 3-5 行小修
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 2, 4, 5)
  - **Blocks**: Task 9 (后续归档后再改 AGENTS.md)
  - **Blocked By**: None

  **References**:
  - `AGENTS.md` - 整体文档
  - `src/main.cpp` - 唯一存在的入口
  - `src/CMakeLists.txt:59-74` - 三个 if(EXISTS) 保护

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: AGENTS.md 不再引用不存在的源文件
    Tool: Bash (grep)
    Preconditions: 修复后
    Steps:
      1. grep -n "cpu_main\|traffic_main\|sc_main" AGENTS.md
    Expected Result: 0 个匹配
    Failure Indicators: 任何匹配项
    Evidence: .omo/evidence/task-3-agents-grep.txt

  Scenario: AGENTS.md 中 main.cpp 描述正确
    Tool: Bash (grep)
    Preconditions: 修复后
    Steps:
      1. grep -n "main.cpp\|cpptlm_sim" AGENTS.md
    Expected Result: 至少 1 个匹配（保留 main.cpp 描述）
    Failure Indicators: 0 个匹配（说明误删了正确描述）
    Evidence: .omo/evidence/task-3-main-cpp-ref.txt

  Scenario: 文档字数变化合理
    Tool: Bash (wc)
    Preconditions: 修复后
    Steps:
      1. wc -l AGENTS.md
    Expected Result: 行数减少 3-10 行（不应大量删除）
    Failure Indicators: 行数减少 > 20（可能误删了其他内容）
    Evidence: .omo/evidence/task-3-wc-line-count.txt
  ```

  **Evidence to Capture**:
  - [ ] grep 输出确认 0 匹配
  - [ ] main.cpp 描述保留
  - [ ] 行数变化在合理范围

  **Commit**: YES
  - Message: `docs(agents): remove references to non-existent source files`
  - Files: `AGENTS.md`
  - Pre-commit: N/A

- [x] 4. 清理 `scripts/__pycache__/linter.cpython-312.pyc` 残留

  **What to do**:
  - 读取 `/workspace/project/CppTLM/scripts/__pycache__/` 列表
  - 确认 `linter.cpython-312.pyc` 存在但 `linter.py` 已删除
  - 删除 `linter.cpython-312.pyc`（保留其他合法 .pyc 缓存）
  - 添加 `.gitignore` 规则（如不存在）忽略 `__pycache__/`
  - **重要**: 检查项目根 `.gitignore` 是否已有 `__pycache__` 规则

  **Must NOT do**:
  - 不删除其他合法 .pyc 文件
  - 不强行清理所有 __pycache__（属于 Python 运行时缓存，应被 .gitignore 排除）
  - 不修改 Python 脚本本身

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 单文件删除 + gitignore 验证
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 2, 3, 5)
  - **Blocks**: Task 10
  - **Blocked By**: None

  **References**:
  - `scripts/__pycache__/` 目录列表
  - `.gitignore` 项目根文件（确认 __pycache__ 规则）
  - `git log --all --full-history -- scripts/linter.py` 确认 linter.py 历史

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: linter.cpython-312.pyc 已删除
    Tool: Bash (ls)
    Preconditions: 修复后
    Steps:
      1. ls scripts/__pycache__/ | grep linter
    Expected Result: 0 个匹配（无 linter 文件）
    Failure Indicators: linter.cpython-312.pyc 存在
    Evidence: .omo/evidence/task-4-linter-deleted.txt

  Scenario: 其他合法 .pyc 保留
    Tool: Bash (ls)
    Preconditions: 修复后
    Steps:
      1. ls scripts/__pycache__/
    Expected Result: 含 analyzer / credit_flow / derive_expr / path_tracer / topology_generator 等合法 .pyc
    Failure Indicators: 目录为空或仅含 linter
    Evidence: .omo/evidence/task-4-pyc-inventory.txt

  Scenario: .gitignore 包含 __pycache__ 规则
    Tool: Bash (grep)
    Preconditions: 修复后
    Steps:
      1. grep -n "__pycache__\|\.pyc" .gitignore
    Expected Result: 含 __pycache__ 规则
    Failure Indicators: 0 个匹配（说明 .pyc 会被 git 跟踪）
    Evidence: .omo/evidence/task-4-gitignore-check.txt
  ```

  **Evidence to Capture**:
  - [ ] linter .pyc 已删除
  - [ ] 其他合法 .pyc 保留
  - [ ] .gitignore 验证

  **Commit**: YES
  - Message: `chore(scripts): clean up orphaned __pycache__ entries`
  - Files: `scripts/__pycache__/linter.cpython-312.pyc` (deleted)
  - Pre-commit: N/A

- [x] 5. 完整性检查 — 搜索其他潜在死路径引用

  **What to do**:
  - 在整个仓库搜索所有对不存在文件/可执行文件的引用
  - 具体 grep 模式:
    - `grep -rn "cpu_main\.cpp\|traffic_main\.cpp\|sc_main\.cpp" .` (排除 docs-archived)
    - `grep -rn "cpptlm_cpu\|cpptlm_traffic" .` (排除 docs-archived 和 build/)
    - `grep -rn "linter\.py\b" scripts/`
    - `grep -rn "samples/simple1\|samples/simple_hier" .` (排除归档目标)
    - `grep -rn "cache_system/main\.cpp\|noc_system/main\.cpp" .` (排除归档)
    - `grep -rn "simple1\|simple_hier" docs/ configs/ scripts/ samples/` 找残留引用
  - 输出"无残留"报告（每个 grep 命令的结果）
  - 如发现新残留，**不在本任务中修复**，而是把发现记录到 `.omo/drafts/cpptlm-cleanup-additional.md` 并通知用户

  **Must NOT do**:
  - 不修改除 `.omo/drafts/cpptlm-cleanup-additional.md` 外的任何文件
  - 不删除已发现的死引用（留给后续任务）
  - 不修改 docs-archived/ 内容

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 只读 grep 检查
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 2, 3, 4)
  - **Blocks**: Task 6 (确认 simple1/simple_hier 可安全归档)
  - **Blocked By**: None

  **References**:
  - `scripts/run_all_tests.sh:7-13` - 已识别
  - `src/cpu_main.cpp` / `traffic_main.cpp` / `sc_main.cpp` - 已识别
  - `samples/simple1/` / `simple_hier/` - 已识别
  - `examples/cache_system/main.cpp` / `noc_system/main.cpp` - 已识别
  - `scripts/__pycache__/linter.cpython-312.pyc` - 已识别

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 死路径 grep 报告
    Tool: Bash (grep + tee)
    Preconditions: 仓库根
    Steps:
      1. mkdir -p .omo/evidence
      2. grep -rn "cpu_main\.cpp\|traffic_main\.cpp\|sc_main\.cpp" . --exclude-dir=build --exclude-dir=docs-archived --exclude-dir=.git 2>/dev/null | tee .omo/evidence/task-5-grep-cpu-main.txt
      3. grep -rn "cpptlm_cpu\|cpptlm_traffic" . --exclude-dir=build --exclude-dir=docs-archived --exclude-dir=.git 2>/dev/null | tee .omo/evidence/task-5-grep-cpptlm-cpu.txt
      4. grep -rn "linter\.py\b" scripts/ 2>/dev/null | tee .omo/evidence/task-5-grep-linter.txt
      5. grep -rn "cache_system/main\.cpp\|noc_system/main\.cpp" . --exclude-dir=build --exclude-dir=docs-archived 2>/dev/null | tee .omo/evidence/task-5-grep-examples.txt
    Expected Result: 每个文件应显示该 grep 的实际匹配数（0 或 N）
    Failure Indicators: grep 命令失败
    Evidence: .omo/evidence/task-5-grep-*.txt (5 个文件)

  Scenario: samples/simple1 和 simple_hier 残留检查
    Tool: Bash (grep)
    Preconditions: 仓库根
    Steps:
      1. grep -rn "samples/simple1\|samples/simple_hier" . --exclude-dir=build --exclude-dir=.git --exclude-dir=docs-archived 2>/dev/null | tee .omo/evidence/task-5-grep-simple-residue.txt
    Expected Result: 输出 AGENTS.md 或其他文档对 simple1/simple_hier 的引用（这些将在 Wave 2 中更新）
    Failure Indicators: 完全 0 匹配
    Evidence: .omo/evidence/task-5-grep-simple-residue.txt
  ```

  **Evidence to Capture**:
  - [ ] 5 个 grep 报告文件
  - [ ] simple1/simple_hier 残留清单

  **Commit**: NO (read-only task; results feed into Wave 2)
  - Message: N/A
  - Files: `.omo/evidence/task-5-grep-*.txt`
  - Pre-commit: N/A

### Wave 2 — Orphan Cleanup (Priority B)

- [x] 6. 归档 `samples/simple1/` 到 `docs-archived/samples-orphaned/`

  **What to do**:
  - 读取 `/workspace/project/CppTLM/samples/simple1/` 全部内容（CMakeLists.txt, configs/, modules/）
  - 创建目标目录 `docs-archived/samples-orphaned/simple1/`
  - 使用 `git mv` 移动所有内容（保留 git 历史）:
    - `git mv samples/simple1/CMakeLists.txt docs-archived/samples-orphaned/simple1/`
    - `git mv samples/simple1/configs docs-archived/samples-orphaned/simple1/`
    - `git mv samples/simple1/modules docs-archived/samples-orphaned/simple1/`
    - `git rmdir samples/simple1` (空目录删除)
  - 在 `docs-archived/samples-orphaned/simple1/README.md` 写明归档原因（引用 v2.1 DEPRECATED 注释）
  - 验证 `samples/` 目录不再含 `simple1/`

  **Must NOT do**:
  - 不修改 AGENTS.md（由 Task 9 处理）
  - 不删除 samples/simple1/ 任何文件（仅移动）
  - 不修改 simple1/ 内部文件
  - 不创建新示例

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: git mv 操作 + README 创建
  - **Skills**: `["using-git-worktrees"]`
    - `using-git-worktrees`: 确认操作不会污染主分支

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 7)
  - **Blocks**: Task 9 (AGENTS.md 更新)
  - **Blocked By**: Task 5 (确认无残留引用)

  **References**:
  - `samples/simple1/CMakeLists.txt` - 独立项目（自声明 cmake_minimum_required）
  - `samples/simple1/modules/cpu_cluster.cc:1-5` - 显式标 DEPRECATED
  - `AGENTS.md` - 旧"WHERE TO LOOK"段可能引用
  - `docs-archived/` 目录命名约定

  **WHY Each Reference Matters**:
  - `samples/simple1/CMakeLists.txt` 独立项目声明，根项目未引用
  - `cpu_cluster.cc` 头注释明确"v2.1 DEPRECATED"
  - 归档路径命名与现有 `docs-archived/` 一致

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: git mv 成功且历史保留
    Tool: Bash (git status + log)
    Preconditions: Task 5 完成（无残留）
    Steps:
      1. git status -s | head -10
      2. git log --oneline -3 -- samples/simple1/CMakeLists.txt
    Expected Result: status 显示 "R samples/simple1/... -> docs-archived/samples-orphaned/simple1/..."（rename 而非 delete+add）
    Failure Indicators: "D samples/simple1/..." + "?? docs-archived/..."（说明历史丢失）
    Evidence: .omo/evidence/task-6-git-status.txt

  Scenario: 源路径已清空
    Tool: Bash (ls)
    Preconditions: 上一场景
    Steps:
      1. ls samples/simple1 2>&1 | head -3
    Expected Result: "No such file or directory"
    Failure Indicators: 仍列出文件
    Evidence: .omo/evidence/task-6-source-empty.txt

  Scenario: 目标路径存在
    Tool: Bash (ls)
    Preconditions: 上一场景
    Steps:
      1. ls -la docs-archived/samples-orphaned/simple1/
    Expected Result: 列出 CMakeLists.txt + configs/ + modules/ + README.md
    Failure Indicators: 目录不存在或仅含部分文件
    Evidence: .omo/evidence/task-6-dest-content.txt

  Scenario: 归档 README 存在
    Tool: Bash (cat)
    Preconditions: 上一场景
    Steps:
      1. head -10 docs-archived/samples-orphaned/simple1/README.md
    Expected Result: 含 "DEPRECATED" / "v2.1" / "归档" 等关键词
    Failure Indicators: 文件不存在或内容空白
    Evidence: .omo/evidence/task-6-readme-head.txt
  ```

  **Evidence to Capture**:
  - [ ] git status 确认 rename（不是 delete+add）
  - [ ] 源目录已空
  - [ ] 目标目录内容完整
  - [ ] README.md 内容

  **Commit**: YES
  - Message: `archive(samples): move simple1 to docs-archived (DEPRECATED v2.1)`
  - Files: `samples/simple1/...` (renamed), `docs-archived/samples-orphaned/simple1/README.md` (new)
  - Pre-commit: N/A

- [x] 7. 归档 `samples/simple_hier/` 到 `docs-archived/samples-orphaned/`

  **What to do**:
  - 读取 `/workspace/project/CppTLM/samples/simple_hier/` 全部内容（main.cpp, src/, configs/）
  - 确认无 CMakeLists.txt（孤儿）
  - 创建目标 `docs-archived/samples-orphaned/simple_hier/`
  - 使用 `git mv`:
    - `git mv samples/simple_hier/main.cpp docs-archived/samples-orphaned/simple_hier/`
    - `git mv samples/simple_hier/src docs-archived/samples-orphaned/simple_hier/`
    - `git mv samples/simple_hier/configs docs-archived/samples-orphaned/simple_hier/`
    - `git rmdir samples/simple_hier`
  - 在 `docs-archived/samples-orphaned/simple_hier/README.md` 写明归档原因（孤儿 + 模块 v2.1 不再支持）
  - 验证 `samples/` 不再含 `simple_hier/`

  **Must NOT do**:
  - 不修改 main.cpp 内容
  - 不补 CMakeLists（这是归档，不是修复）
  - 不删除 src/ 或 configs/

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Task 6)
  - **Blocks**: Task 9
  - **Blocked By**: Task 5

  **References**:
  - `samples/simple_hier/main.cpp` - 有 main() 但缺构建配置
  - `samples/simple_hier/src/cpu_cluster.cc` - 引用 DEPRECATED CpuCluster
  - `samples/simple_hier/src/noc_tile.cc` - 引用 NOCTile（未在 v2.1 注册）
  - `samples/simple_hier/configs/system.json` - 含 libcpu_cluster_plugin.so 引用（v2.1 不支持）

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: git mv 成功
    Tool: Bash (git status)
    Preconditions: Task 5 完成
    Steps:
      1. git status -s | grep simple_hier
    Expected Result: 显示 "R samples/simple_hier/... -> docs-archived/..." rename
    Failure Indicators: "D" + "??" 模式
    Evidence: .omo/evidence/task-7-git-status.txt

  Scenario: 源目录已清空
    Tool: Bash (ls)
    Preconditions: 上一场景
    Steps:
      1. ls samples/simple_hier 2>&1
    Expected Result: "No such file or directory"
    Evidence: .omo/evidence/task-7-source-empty.txt

  Scenario: 目标目录内容
    Tool: Bash (ls)
    Preconditions: 上一场景
    Steps:
      1. ls -la docs-archived/samples-orphaned/simple_hier/
    Expected Result: main.cpp + src/ + configs/ + README.md
    Evidence: .omo/evidence/task-7-dest-content.txt
  ```

  **Evidence to Capture**:
  - [ ] git rename
  - [ ] 源空 + 目标存在
  - [ ] README 存在

  **Commit**: YES
  - Message: `archive(samples): move simple_hier to docs-archived (orphan)`
  - Files: `samples/simple_hier/...` (renamed), `docs-archived/samples-orphaned/simple_hier/README.md` (new)
  - Pre-commit: N/A

- [x] 8. 补全 `examples/CMakeLists.txt`（添加 2 target + 删除无效守卫）

  **What to do**:
  - 读取 `/workspace/project/CppTLM/examples/CMakeLists.txt`（22 行）
  - 在 `add_executable(example_cache ...)` 之后添加:
    - `add_executable(example_basic_transaction example_basic_transaction.cc)`
    - `target_link_libraries(example_basic_transaction PRIVATE cpptlm_core)`
    - `add_executable(example_error_handling example_error_handling.cc)`
    - `target_link_libraries(example_error_handling PRIVATE cpptlm_core)`
  - **删除**两个无效守卫:
    - 第 6-9 行 `if(EXISTS .../cache_system/main.cpp) add_executable(example_cache ...)`
    - 第 14-17 行 `if(EXISTS .../noc_system/main.cpp) add_executable(example_noc ...)`
  - 添加注释说明删除原因（参考 archive 决定）

  **Must NOT do**:
  - 不修改 example_*.cc 内容
  - 不添加 cache_system/ 或 noc_system/ 目录
  - 不修改 demo_e2e_*.py 路径

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 单文件 ~10 行修改
  - **Skills**: `["cmake"]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 6, 7, 9)
  - **Blocks**: Task 10
  - **Blocked By**: Task 5

  **References**:
  - `examples/CMakeLists.txt:6-17` - 待修改的 if(EXISTS) 块
  - `examples/example_basic_transaction.cc` - 含 main()
  - `examples/example_error_handling.cc` - 含 main()
  - `src/CMakeLists.txt:33-37` - cpptlm_core include 设置参考

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 新 target 在 cmake 配置中
    Tool: Bash (cmake configure)
    Preconditions: 修改后
    Steps:
      1. cmake -S . -B build 2>&1 | tee /tmp/cmake-reconf2.log
      2. grep -E "example_basic_transaction|example_error_handling" /tmp/cmake-reconf2.log
    Expected Result: 2 个 target 名出现
    Failure Indicators: 0 个
    Evidence: .omo/evidence/task-8-cmake-configure.log

  Scenario: 旧守卫已删除
    Tool: Bash (cat)
    Preconditions: 上一场景
    Steps:
      1. grep -n "cache_system/main\|noc_system/main" examples/CMakeLists.txt
    Expected Result: 0 个匹配
    Failure Indicators: 含 if(EXISTS cache_system...)
    Evidence: .omo/evidence/task-8-old-guards-removed.txt

  Scenario: 两个新 example 可构建
    Tool: Bash (cmake build)
    Preconditions: 上一场景
    Steps:
      1. cmake --build build --target example_basic_transaction -j$(nproc) 2>&1 | tail -5
      2. cmake --build build --target example_error_handling -j$(nproc) 2>&1 | tail -5
    Expected Result: 两个都成功（无 error:）
    Failure Indicators: 找不到头文件（如 packet.hh / packet_pool.hh）
    Evidence: .omo/evidence/task-8-build-examples.log

  Scenario: example_basic_transaction 可运行
    Tool: Bash (run binary)
    Preconditions: 上一场景
    Steps:
      1. timeout 10 ./build/bin/examples/example_basic_transaction 2>&1 | head -20
    Expected Result: 输出含 "CppTLM" / "TransactionTracker" 等字样
    Failure Indicators: 段错误立即退出
    Evidence: .omo/evidence/task-8-run-basic-tx.txt

  Scenario: example_error_handling 可运行
    Tool: Bash (run binary)
    Preconditions: 上一场景
    Steps:
      1. timeout 10 ./build/bin/examples/example_error_handling 2>&1 | head -20
    Expected Result: 输出含 "ErrorContext" / "DebugTracker" 等字样
    Failure Indicators: 段错误立即退出
    Evidence: .omo/evidence/task-8-run-error-handling.txt
  ```

  **Evidence to Capture**:
  - [ ] cmake configure 日志含 2 个 target
  - [ ] 旧守卫 grep 0 匹配
  - [ ] 两个 example 构建成功
  - [ ] 两个 example 运行输出

  **Commit**: YES
  - Message: `build(examples): add example_basic_transaction and example_error_handling targets, remove dead guards`
  - Files: `examples/CMakeLists.txt`
  - Pre-commit: `cmake --build build --target example_basic_transaction example_error_handling`

- [x] 9. 更新 `AGENTS.md` 反映归档和 example target

  **What to do**:
  - 读取当前 `AGENTS.md`（Task 3 已删除 cpu_main/traffic_main/sc_main 引用）
  - 找到 "STRUCTURE" 段或 "WHERE TO LOOK" 段中 `samples/simple1` / `samples/simple_hier` 引用
  - 实际定位：`grep -n "simple1\|simple_hier" AGENTS.md`
  - 替换为对 `docs-archived/samples-orphaned/` 的说明
  - 找到 `examples/` 描述段（如果存在），添加新 example 描述
  - 添加"已归档"段说明 simple1 / simple_hier / cache_system / noc_system / linter.py 已归档或删除

  **Must NOT do**:
  - 不重新引入 cpu_main.cpp 引用
  - 不修改零债务原则
  - 不删除有用的内容

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 6, 7, 8)
  - **Blocks**: Task 10
  - **Blocked By**: Tasks 6, 7 (归档完成)

  **References**:
  - `AGENTS.md` - 当前内容（Task 3 后）
  - `docs-archived/samples-orphaned/` - 新归档位置

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: AGENTS.md 含归档说明
    Tool: Bash (grep)
    Preconditions: 修改后
    Steps:
      1. grep -n "docs-archived\|已归档\|DEPRECATED\|simple1\|simple_hier" AGENTS.md
    Expected Result: 含归档说明
    Failure Indicators: 0 个匹配（说明忘了加）
    Evidence: .omo/evidence/task-9-archive-ref.txt

  Scenario: cpu_main 等仍 0 引用
    Tool: Bash (grep)
    Preconditions: 上一场景
    Steps:
      1. grep -n "cpu_main\|traffic_main\|sc_main" AGENTS.md
    Expected Result: 0 个匹配
    Evidence: .omo/evidence/task-9-no-cpu-main.txt
  ```

  **Evidence to Capture**:
  - [ ] 归档说明存在
  - [ ] cpu_main 仍 0 引用

  **Commit**: YES
  - Message: `docs(agents): reflect sample archiving and new example targets`
  - Files: `AGENTS.md`
  - Pre-commit: N/A

- [x] 10. 全量构建+测试+格式化验证（Wave 1+2 完整验证）

  **What to do**:
  - 清理 build 目录：`rm -rf build` (可选，但确保从头构建)
  - 重新配置：`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON`
  - 全量构建：`cmake --build build -j$(nproc) 2>&1 | tee .omo/evidence/task-10-build.log`
  - 运行测试：`cd build && ctest --output-on-failure 2>&1 | tee .omo/evidence/task-10-ctest.log`
  - 格式检查：`./scripts/format.sh --check 2>&1 | tee .omo/evidence/task-10-format.log`
  - **关键确认**：
    - 5 个新 target（streaming_demo、example_basic_transaction、example_error_handling、demo_topology_generator、demo_stats_visualization、demo_stats_watcher）都在 build 列表中
    - 现有 75+ Catch2 测试通过（或已知失败不变更）
    - 格式检查无错
  - 如构建失败，回滚最近 commit，定位问题，修复后重新跑

  **Must NOT do**:
  - 不修改源代码（仅验证）
  - 不跳过 ctest（即使测试慢）
  - 不修改 AGENTS.md / CMakeLists.txt

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
    - Reason: 3-4 个命令串行执行 + 日志分析
  - **Skills**: `["cmake", "verification-before-completion"]`
    - `cmake`: 构建命令熟悉
    - `verification-before-completion`: 验证后报告

  **Parallelization**:
  - **Can Run In Parallel**: NO (sequential verification gate)
  - **Parallel Group**: 必须在 Wave 2 最后执行
  - **Blocks**: Wave 3
  - **Blocked By**: Tasks 1, 2, 3, 4, 6, 7, 8, 9

  **References**:
  - `CMakeLists.txt` - 根配置
  - `samples/CMakeLists.txt` - Task 2 改后
  - `examples/CMakeLists.txt` - Task 8 改后
  - `scripts/format.sh` - 格式检查

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 全量构建成功
    Tool: Bash (cmake build)
    Preconditions: 所有 Wave 1+2 任务完成
    Steps:
      1. cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5
      2. cmake --build build -j$(nproc) 2>&1 | tee .omo/evidence/task-10-build.log | tail -10
    Expected Result: 退出码 0，最后 10 行无 "error:"
    Failure Indicators: "error:" 或 "fatal error" 或退出码非 0
    Evidence: .omo/evidence/task-10-build.log

  Scenario: ctest 通过
    Tool: Bash (ctest)
    Preconditions: 上一场景
    Steps:
      1. cd build && ctest --output-on-failure 2>&1 | tee .omo/evidence/task-10-ctest.log | tail -15
    Expected Result: 100% tests passed（或与基线一致）
    Failure Indicators: "Failed" 字段非 0
    Evidence: .omo/evidence/task-10-ctest.log

  Scenario: 格式检查通过
    Tool: Bash (format.sh)
    Preconditions: 上一场景
    Steps:
      1. cd /workspace/project/CppTLM && ./scripts/format.sh --check 2>&1 | tee .omo/evidence/task-10-format.log | tail -10
    Expected Result: 输出 "✅ 格式化完成！" 或类似
    Failure Indicators: "❌ N 个文件需要格式化"
    Evidence: .omo/evidence/task-10-format.log

  Scenario: 5 个新 target 在 build 中
    Tool: Bash (find build/bin)
    Preconditions: 构建成功
    Steps:
      1. find build/bin -type f -executable | sort
    Expected Result: 含 streaming_demo、stats_demo、traffic_gen_demo、cpptlm_sim、example_basic_transaction、example_error_handling
    Failure Indicators: 缺任何一个
    Evidence: .omo/evidence/task-10-bin-inventory.txt
  ```

  **Evidence to Capture**:
  - [ ] 完整 build.log
  - [ ] ctest.log
  - [ ] format.log
  - [ ] bin/inventory 列表

  **Commit**: YES (verification tag, no code change)
  - Message: `verify(phase-a-b): all builds and tests pass after blocker + orphan fixes`
  - Files: N/A (empty commit or pre-existing evidence)
  - Pre-commit: `cmake --build build && cd build && ctest`

### Wave 3 — Scripts Restructuring (Priority C)

- [x] 11. 创建 `scripts/` 5 个子目录骨架

  **What to do**:
  - 在 `/workspace/project/CppTLM/scripts/` 下创建 5 个子目录:
    - `scripts/build/`（接收 build.sh, format.sh）
    - `scripts/test/`（接收 test.sh, run_all_tests.sh, ci_e2e_test.sh）
    - `scripts/pipeline/`（接收 run_full_pipeline.sh）
    - `scripts/topology/`（接收 topology_validator.py, topology_generator.py, analyzer.py, path_tracer.py, credit_flow.py）
    - `scripts/stats/`（接收 stats_annotator.py, stats_watcher.py, layout_manager.py, derive_expr.py）
  - 创建 `scripts/README.md` 说明新结构
  - **不移动文件** —— 留给 Tasks 12, 13
  - 验证 5 个目录创建成功

  **Must NOT do**:
  - 不移动任何脚本（Tasks 12-13 负责）
  - 不修改 `scripts/CMakeLists.txt`（Task 14 负责）
  - 不删除 `scripts/` 根下的文件

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 5 个 mkdir + 1 个 README
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 起始 (with Tasks 12, 13, 但实际 12/13 依赖 11)
  - **Blocks**: Tasks 12, 13
  - **Blocked By**: Task 10 (Wave 2 验证通过)

  **References**:
  - `scripts/` 当前文件清单
  - `docs-archived/` 命名约定（如有 README 索引风格）

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 5 个子目录创建
    Tool: Bash (ls)
    Preconditions: Task 10 通过
    Steps:
      1. ls -la scripts/ | grep "^d" | awk '{print $9}'
    Expected Result: 含 build, test, pipeline, topology, stats 5 个
    Failure Indicators: 少于 5 个
    Evidence: .omo/evidence/task-11-subdirs-created.txt

  Scenario: README 存在
    Tool: Bash (cat)
    Preconditions: 上一场景
    Steps:
      1. head -20 scripts/README.md
    Expected Result: 含 5 子目录列表
    Failure Indicators: 文件不存在
    Evidence: .omo/evidence/task-11-readme-head.txt
  ```

  **Evidence to Capture**:
  - [ ] 5 子目录列表
  - [ ] README 内容

  **Commit**: YES
  - Message: `refactor(scripts): create 5-subdir skeleton (build/test/pipeline/topology/stats)`
  - Files: `scripts/{build,test,pipeline,topology,stats}/.gitkeep`, `scripts/README.md`
  - Pre-commit: N/A

- [x] 12. 移动 bash 脚本到 `scripts/{build,test,pipeline}/`

  **What to do**:
  - 使用 `git mv`（保留历史）:
    - `git mv scripts/build.sh scripts/build/build.sh`
    - `git mv scripts/format.sh scripts/build/format.sh`
    - `git mv scripts/test.sh scripts/test/test.sh`
    - `git mv scripts/run_all_tests.sh scripts/test/run_all_tests.sh`
    - `git mv scripts/ci_e2e_test.sh scripts/test/ci_e2e_test.sh`
    - `git mv scripts/run_full_pipeline.sh scripts/pipeline/run_full_pipeline.sh`
  - 验证移动成功
  - **保留权限**（git mv 自动保留）

  **Must NOT do**:
  - 不修改脚本内容（Task 16 处理引用）
  - 不移动 Python 脚本（Task 13）
  - 不修改 CMakeLists.txt（Task 14）

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 6 个 git mv
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 (with Task 13)
  - **Blocks**: Task 14
  - **Blocked By**: Task 11

  **References**:
  - `scripts/build.sh` 等当前路径
  - `scripts/README.md` 分类（Task 11 创建）

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 6 个 bash 移动完成
    Tool: Bash (git status + find)
    Preconditions: Task 11 完成
    Steps:
      1. git status -s | grep "^R.*\.sh"
    Expected Result: 6 个 "R" 状态（rename）
    Failure Indicators: < 6 个
    Evidence: .omo/evidence/task-12-git-rename.txt

  Scenario: 脚本权限保留
    Tool: Bash (ls -l)
    Preconditions: 上一场景
    Steps:
      1. ls -l scripts/build/build.sh scripts/build/format.sh scripts/test/test.sh scripts/test/run_all_tests.sh scripts/test/ci_e2e_test.sh scripts/pipeline/run_full_pipeline.sh
    Expected Result: 所有文件含 "x" 权限
    Failure Indicators: 缺 x 权限
    Evidence: .omo/evidence/task-12-permissions.txt

  Scenario: 源路径已清
    Tool: Bash (ls)
    Preconditions: 上一场景
    Steps:
      1. ls scripts/*.sh 2>&1
    Expected Result: "No such file or directory"（scripts/ 根下无 .sh）
    Failure Indicators: 仍列出旧文件
    Evidence: .omo/evidence/task-12-source-clean.txt
  ```

  **Evidence to Capture**:
  - [ ] git rename 列表（6 项）
  - [ ] 权限保留
  - [ ] 源清空

  **Commit**: YES
  - Message: `refactor(scripts): move bash scripts to build/test/pipeline subdirs`
  - Files: 6 个 git mv
  - Pre-commit: N/A

- [x] 13. 移动 Python 脚本到 `scripts/{topology,stats}/`

  **What to do**:
  - 使用 `git mv`:
    - `git mv scripts/topology_validator.py scripts/topology/topology_validator.py`
    - `git mv scripts/topology_generator.py scripts/topology/topology_generator.py`
    - `git mv scripts/analyzer.py scripts/topology/analyzer.py`
    - `git mv scripts/path_tracer.py scripts/topology/path_tracer.py`
    - `git mv scripts/credit_flow.py scripts/topology/credit_flow.py`
    - `git mv scripts/stats_annotator.py scripts/stats/stats_annotator.py`
    - `git mv scripts/stats_watcher.py scripts/stats/stats_watcher.py`
    - `git mv scripts/layout_manager.py scripts/stats/layout_manager.py`
    - `git mv scripts/derive_expr.py scripts/stats/derive_expr.py`
  - 验证移动成功
  - 清理 `scripts/__pycache__/`（重建后会有新路径）

  **Must NOT do**:
  - 不修改 .py 内容（Task 15 处理引用）
  - 不移动 .sh 脚本（Task 12）
  - 不动 `__pycache__/linter.cpython-312.pyc`（Task 4 已删）

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 9 个 git mv
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 (with Task 12)
  - **Blocks**: Task 14
  - **Blocked By**: Task 11

  **References**:
  - `scripts/*.py` 当前路径
  - `scripts/README.md` 分类

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 9 个 Python 移动完成
    Tool: Bash (git status)
    Preconditions: Task 11 完成
    Steps:
      1. git status -s | grep "^R.*\.py"
    Expected Result: 9 个 "R" 状态
    Failure Indicators: < 9 个
    Evidence: .omo/evidence/task-13-git-rename.txt

  Scenario: 子目录内容正确
    Tool: Bash (find)
    Preconditions: 上一场景
    Steps:
      1. find scripts/topology scripts/stats -type f -name "*.py" | sort
    Expected Result: topology/ 5 个，stats/ 4 个
    Failure Indicators: 数量不对
    Evidence: .omo/evidence/task-13-paths.txt
  ```

  **Evidence to Capture**:
  - [ ] git rename 9 项
  - [ ] 子目录文件清单

  **Commit**: YES
  - Message: `refactor(scripts): move python scripts to topology/stats subdirs`
  - Files: 9 个 git mv
  - Pre-commit: N/A

- [x] 14. 重组 `scripts/CMakeLists.txt` 调度各子目录

  **What to do**:
  - 读取当前 `scripts/CMakeLists.txt`（46 行）
  - 重写以调度新子目录:
    - 4 个 `validate_topology` target 引用路径改为 `${CMAKE_SOURCE_DIR}/scripts/topology/topology_validator.py`
    - 合并的 `validate_topology` target 同样更新
  - 添加 `add_subdirectory(topology)` 等调度（如果用子目录 CMakeLists）
  - **简化方案**: 保持单文件 `scripts/CMakeLists.txt`，但更新内部路径引用即可
  - 添加注释说明新结构

  **Must NOT do**:
  - 不删除 `find_package(Python3 QUIET)` 守卫
  - 不修改其他 target 名（保持 `validate_topology` 等）
  - 不修改 `WORKING_DIRECTORY`

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 路径更新 + 注释
  - **Skills**: `["cmake"]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 (with Tasks 15, 16)
  - **Blocks**: Task 17
  - **Blocked By**: Tasks 12, 13

  **References**:
  - `scripts/CMakeLists.txt:15,30` - 路径引用需更新
  - 4 个 config 路径保持不变（configs/ 目录未动）

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 新路径生效
    Tool: Bash (cmake configure + build target)
    Preconditions: Tasks 12, 13 完成
    Steps:
      1. cmake -S . -B build 2>&1 | tee .omo/evidence/task-14-cmake-configure.log
      2. grep -E "topology/topology_validator" scripts/CMakeLists.txt
      3. cmake --build build --target validate_topology 2>&1 | tee .omo/evidence/task-14-validate-topology.log | tail -10
    Expected Result:
      - cmake configure 成功
      - grep 显示 4-5 个新路径引用
      - validate_topology target 运行成功（含 "ALL VALIDATIONS PASSED"）
    Failure Indicators: 找不到 topology_validator.py（"No such file"）
    Evidence: .omo/evidence/task-14-cmake-configure.log, .omo/evidence/task-14-validate-topology.log

  Scenario: 单个 validate_mesh_2x2 target 工作
    Tool: Bash (cmake build target)
    Preconditions: 上一场景
    Steps:
      1. cmake --build build --target validate_mesh_2x2 2>&1 | tail -5
    Expected Result: "ALL VALIDATIONS PASSED" 输出
    Failure Indicators: "No such file" 或 Python 错误
    Evidence: .omo/evidence/task-14-validate-mesh.txt
  ```

  **Evidence to Capture**:
  - [ ] cmake configure 日志
  - [ ] validate_topology 运行日志
  - [ ] 单个 validate_mesh_2x2 输出

  **Commit**: YES
  - Message: `refactor(scripts): update CMakeLists.txt dispatch for new subdir layout`
  - Files: `scripts/CMakeLists.txt`
  - Pre-commit: `cmake --build build --target validate_topology`

- [x] 15. 更新 `samples/*_demo.py` 的 `sys.path` 引用

  **What to do**:
  - 读取 3 个 samples Python demo 的 `sys.path.insert(...)` 行
  - 当前路径（重构前）: `sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'scripts'))` → 指向 `scripts/`
  - 重构后路径: 改为 `os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'topology')` 和 `os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'stats')`
  - 具体:
    - `samples/topology_generator_demo.py`: 导入 `topology_generator` → 改 path 到 `scripts/topology/`
    - `samples/stats_visualization_demo.py`: 导入 `stats_annotator` → 改 path 到 `scripts/stats/`
    - `samples/stats_watcher_demo.py`: 导入 stats_watcher 工具（实际是 subprocess，不导入）→ 检查是否需要改
  - 测试导入路径正确

  **Must NOT do**:
  - 不修改 samples/*.cc
  - 不移动 demo 文件本身
  - 不修改 scripts/* 下的 Python 代码（它们是库，不是 importer）

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 3 个 sys.path 字符串修改
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 (with Tasks 14, 16)
  - **Blocks**: Task 17
  - **Blocked By**: Task 13

  **References**:
  - `samples/topology_generator_demo.py:25` - sys.path.insert
  - `samples/stats_visualization_demo.py:16` - sys.path.insert
  - `samples/stats_watcher_demo.py` - 实际用 subprocess

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: topology_generator_demo 导入成功
    Tool: Bash (python3 import test)
    Preconditions: Tasks 12, 13 完成
    Steps:
      1. python3 -c "import sys; sys.path.insert(0, 'samples'); from topology_generator_demo import demo_mesh; print('OK')" 2>&1 | tail -5
      2. python3 samples/topology_generator_demo.py 2>&1 | tail -10
    Expected Result: 退出码 0，输出 "OK" 含 "Demo 1" 等
    Failure Indicators: ImportError: No module named 'topology_generator'
    Evidence: .omo/evidence/task-15-topology-demo.txt

  Scenario: stats_visualization_demo 导入成功
    Tool: Bash (python3 run)
    Preconditions: 上一场景
    Steps:
      1. python3 samples/stats_visualization_demo.py 2>&1 | tail -10
    Expected Result: 退出码 0，含 "Generated" / "HTML" 字样
    Failure Indicators: ImportError
    Evidence: .omo/evidence/task-15-stats-vis-demo.txt

  Scenario: stats_watcher_demo 运行
    Tool: Bash (python3 run)
    Preconditions: 上一场景
    Steps:
      1. timeout 5 python3 samples/stats_watcher_demo.py 2>&1 | tail -10
    Expected Result: 退出码 0 或 124（timeout），含 "Demo" 字样
    Failure Indicators: ImportError
    Evidence: .omo/evidence/task-15-watcher-demo.txt
  ```

  **Evidence to Capture**:
  - [ ] 3 个 demo 运行输出

  **Commit**: YES
  - Message: `fix(samples): update demo sys.path for new script locations`
  - Files: 3 个 `samples/*_demo.py`
  - Pre-commit: 3 个 demo 各跑一次

- [x] 16. 更新 `run_full_pipeline.sh` 内部脚本引用

  **What to do**:
  - 读取 `scripts/pipeline/run_full_pipeline.sh:40-46, 96-101, 107-110`
  - 引用从:
    - `python3 "$SCRIPT_DIR/topology_generator.py"` → `python3 "$SCRIPT_DIR/../topology/topology_generator.py"`
    - `python3 "$SCRIPT_DIR/stats_annotator.py"` → `python3 "$SCRIPT_DIR/../stats/stats_annotator.py"`
    - `python3 "$SCRIPT_DIR/stats_watcher.py"` → `python3 "$SCRIPT_DIR/../stats/stats_watcher.py"`
  - 变量 `SCRIPT_DIR` 当前是 `scripts/`，重构后是 `scripts/pipeline/`
  - 验证所有引用

  **Must NOT do**:
  - 不修改 OUTPUT_DIR 处理
  - 不改 ENABLE_WATCHER 逻辑
  - 不改 mock 数据 fallback

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 3-5 行路径修复
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 (with Tasks 14, 15)
  - **Blocks**: Task 17
  - **Blocked By**: Task 12

  **References**:
  - `scripts/pipeline/run_full_pipeline.sh:40` - topology_generator 调用
  - `scripts/pipeline/run_full_pipeline.sh:96` - stats_annotator 调用
  - `scripts/pipeline/run_full_pipeline.sh:107` - stats_watcher 调用
  - `scripts/pipeline/run_full_pipeline.sh:25-26` - SCRIPT_DIR 计算

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: run_full_pipeline.sh 路径正确
    Tool: Bash (grep)
    Preconditions: Task 12 完成
    Steps:
      1. grep -n "topology_generator\|stats_annotator\|stats_watcher" scripts/pipeline/run_full_pipeline.sh
    Expected Result: 含 `.. /topology/` 和 `.. /stats/` 路径
    Failure Indicators: 直接 `$SCRIPT_DIR/topology_generator.py`（无 ../）
    Evidence: .omo/evidence/task-16-pipeline-paths.txt

  Scenario: bash 语法
    Tool: Bash (bash -n)
    Preconditions: 上一场景
    Steps:
      1. bash -n scripts/pipeline/run_full_pipeline.sh
    Expected Result: 退出码 0
    Failure Indicators: syntax error
    Evidence: .omo/evidence/task-16-bash-syntax.txt

  Scenario: dry-run 模式跑 pipeline（不实际仿真）
    Tool: Bash (env override)
    Preconditions: 上一场景
    Steps:
      1. mkdir -p /tmp/cpptlm-pipeline-test
      2. timeout 60 bash -c "cd /workspace/project/CppTLM && ENABLE_WATCHER=0 bash -n scripts/pipeline/run_full_pipeline.sh && echo 'Syntax OK'"
    Expected Result: "Syntax OK"
    Failure Indicators: 任何错误
    Evidence: .omo/evidence/task-16-pipeline-syntax.txt
  ```

  **Evidence to Capture**:
  - [ ] grep 路径
  - [ ] bash 语法
  - [ ] 端到端 dry-run

  **Commit**: YES
  - Message: `fix(pipeline): update run_full_pipeline.sh script refs after subdir refactor`
  - Files: `scripts/pipeline/run_full_pipeline.sh`
  - Pre-commit: `bash -n scripts/pipeline/run_full_pipeline.sh`

- [x] 17. 重构后全量验证

  **What to do**:
  - 重新配置 CMake: `cmake -S . -B build`
  - 全量构建: `cmake --build build -j$(nproc) 2>&1 | tee .omo/evidence/task-17-build.log`
  - 完整测试: `cd build && ctest --output-on-failure 2>&1 | tee .omo/evidence/task-17-ctest.log`
  - 验证 `validate_topology` target: `cmake --build build --target validate_topology`
  - 跑 `ci_e2e_test.sh`: `./scripts/test/ci_e2e_test.sh 2>&1 | tee .omo/evidence/task-17-ci-e2e.log`
  - 跑 `run_all_tests.sh --quick`: `./scripts/test/run_all_tests.sh --quick 2>&1 | tee .omo/evidence/task-17-quick.log`
  - 跑 `format.sh --check`: `./scripts/build/format.sh --check 2>&1 | tee .omo/evidence/task-17-format.log`
  - 跑 3 个 Python demo（Task 15 已验证，再次跑确认）
  - 跑 `run_full_pipeline.sh` 完整流程（带 mock 数据，验证整链路）

  **Must NOT do**:
  - 不修改任何源代码
  - 不跳过验证
  - 不修改 AGENTS.md

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
    - Reason: 8-10 个验证步骤
  - **Skills**: `["verification-before-completion", "cmake"]`

  **Parallelization**:
  - **Can Run In Parallel**: NO (sequential)
  - **Parallel Group**: 必须在 Wave 3 最后
  - **Blocks**: Wave 4
  - **Blocked By**: Tasks 11-16

  **References**:
  - 所有重构后脚本路径
  - `docs-archived/samples-orphaned/`（不应被 ctest 引用）

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 完整构建成功
    Tool: Bash
    Preconditions: Tasks 11-16 完成
    Steps:
      1. cmake -S . -B build 2>&1 | tail -3
      2. cmake --build build -j$(nproc) 2>&1 | tee .omo/evidence/task-17-build.log | tail -10
    Expected Result: 0 错误
    Failure Indicators: 任何 error
    Evidence: .omo/evidence/task-17-build.log

  Scenario: ctest + validate_topology
    Tool: Bash
    Preconditions: 上一场景
    Steps:
      1. cd build && ctest --output-on-failure 2>&1 | tee .omo/evidence/task-17-ctest.log | tail -5
      2. cmake --build build --target validate_topology 2>&1 | tee .omo/evidence/task-17-validate.log | tail -10
    Expected Result: 两者都成功
    Failure Indicators: 任何 "FAIL"
    Evidence: .omo/evidence/task-17-ctest.log, .omo/evidence/task-17-validate.log

  Scenario: ci_e2e_test.sh 通过
    Tool: Bash
    Preconditions: 上一场景
    Steps:
      1. cd /workspace/project/CppTLM && ./scripts/test/ci_e2e_test.sh 2>&1 | tee .omo/evidence/task-17-ci-e2e.log | tail -10
    Expected Result: "All E2E tests passed!" 或 "Results: N passed, 0 failed"
    Failure Indicators: 任何 FAIL
    Evidence: .omo/evidence/task-17-ci-e2e.log

  Scenario: run_all_tests.sh --quick 通过
    Tool: Bash
    Preconditions: 上一场景
    Steps:
      1. ./scripts/test/run_all_tests.sh --quick 2>&1 | tee .omo/evidence/task-17-quick.log | tail -10
    Expected Result: "[SUCCESS] Quick mode complete"
    Failure Indicators: 任何 "FAIL" 或 "ERROR"
    Evidence: .omo/evidence/task-17-quick.log

  Scenario: 格式检查通过
    Tool: Bash
    Preconditions: 上一场景
    Steps:
      1. ./scripts/build/format.sh --check 2>&1 | tee .omo/evidence/task-17-format.log | tail -5
    Expected Result: "✅ 格式化完成！"
    Failure Indicators: "❌ N 个文件需要格式化"
    Evidence: .omo/evidence/task-17-format.log
  ```

  **Evidence to Capture**:
  - [ ] 完整 build.log
  - [ ] ctest.log
  - [ ] validate_topology.log
  - [ ] ci_e2e_test.log
  - [ ] run_all_tests_quick.log
  - [ ] format.log

  **Commit**: YES (verification tag)
  - Message: `verify(phase-c): all scripts and ctest pass after subdir refactor`
  - Files: N/A
  - Pre-commit: All above

### Wave 4 — CI Integration (Priority D)

- [x] 18. 添加 `ci_e2e_test.sh` 步骤到 `.github/workflows/ci.yml`

  **What to do**:
  - 读取 `.github/workflows/ci.yml`（84 行）
  - 在 `build-and-test` job 的 "Run tests" 步骤（54-61 行）之后，"Upload test results" 之前（63-69 行），添加新步骤:
    ```yaml
    - name: Run E2E simulation tests
      if: matrix.build-type == 'Release'  # 仅 Release 跑（节省 CI 时间）
      run: ./scripts/test/ci_e2e_test.sh
    ```
  - 注意 `if: matrix.build-type == 'Release'` 避免 Debug+ASan 矩阵重复跑
  - 验证 YAML 语法

  **Must NOT do**:
  - 不修改 `build-and-test` job 的现有步骤
  - 不修改 `code-format` job
  - 不删除现有 `Run tests` 步骤
  - 不改 artifacts 上传配置

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 添加一个 job step
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4 (with Task 19)
  - **Blocks**: Task 20
  - **Blocked By**: Task 17 (Wave 3 验证通过 + 新路径确认)

  **References**:
  - `.github/workflows/ci.yml:54-61` - 现有 Run tests 步骤
  - `.github/workflows/ci.yml:63-69` - artifacts 上传
  - `scripts/test/ci_e2e_test.sh` - 新路径
  - `docs-archived/` (无 CI 引用)

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: YAML 语法有效
    Tool: Bash (python yaml validate)
    Preconditions: 修复后
    Steps:
      1. python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))" 2>&1
    Expected Result: 无错误输出，退出码 0
    Failure Indicators: yaml.YAMLError 或 ScannerError
    Evidence: .omo/evidence/task-18-yaml-valid.txt

  Scenario: 新步骤已添加
    Tool: Bash (grep)
    Preconditions: 上一场景
    Steps:
      1. grep -n "ci_e2e_test.sh\|E2E simulation" .github/workflows/ci.yml
    Expected Result: 含 ci_e2e_test.sh 引用
    Failure Indicators: 0 匹配
    Evidence: .omo/evidence/task-18-step-added.txt

  Scenario: 现有结构未破坏
    Tool: Bash (grep)
    Preconditions: 上一场景
    Steps:
      1. grep -E "name: (Build|Configure|Run tests|Upload test results|Check format|Install dependencies)" .github/workflows/ci.yml
    Expected Result: 至少 5 个匹配（现有 step 仍在）
    Failure Indicators: < 5 个
    Evidence: .omo/evidence/task-18-structure-intact.txt
  ```

  **Evidence to Capture**:
  - [ ] YAML 验证输出
  - [ ] 新 step 存在
  - [ ] 现有结构完整

  **Commit**: YES
  - Message: `ci(workflow): add ci_e2e_test.sh step to build-and-test job`
  - Files: `.github/workflows/ci.yml`
  - Pre-commit: N/A

- [x] 19. 添加 `run_all_tests.sh --quick` 步骤到 `.github/workflows/ci.yml`

  **What to do**:
  - 在 Task 18 添加的 E2E 步骤之后，添加 Python + 关键 C++ 快速测试:
    ```yaml
    - name: Run quick test suite (Python + key C++)
      if: matrix.build-type == 'Release'
      run: |
        pip install networkx pydot pytest  # 必备依赖
        ./scripts/test/run_all_tests.sh --quick
    ```
  - **注意**: 需先安装 Python 依赖（当前 ci.yml 只装 cmake/ccache/ninja/clang-format）
  - 考虑在 "Install dependencies" 步骤扩展 apt-get + pip

  **Must NOT do**:
  - 不修改 Task 18 添加的 E2E 步骤
  - 不删除现有依赖安装
  - 不在 Debug+ASan 矩阵运行（节省时间）

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 添加 1-2 个 job step
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4 (with Task 18)
  - **Blocks**: Task 20
  - **Blocked By**: Task 17

  **References**:
  - `.github/workflows/ci.yml:33-36` - 现有 apt 依赖安装
  - `scripts/test/run_all_tests.sh:33-48` - --quick 模式定义
  - `run_all_tests.sh:6` - PYTEST="python3 -m pytest"（需 pytest）

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: YAML 仍有效
    Tool: Bash
    Preconditions: Task 18 完成
    Steps:
      1. python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))" 2>&1
    Expected Result: 退出码 0，无错误
    Failure Indicators: 任何错误
    Evidence: .omo/evidence/task-19-yaml-valid.txt

  Scenario: run_all_tests.sh --quick 步骤存在
    Tool: Bash (grep)
    Preconditions: 上一场景
    Steps:
      1. grep -n "run_all_tests.sh --quick\|quick test suite" .github/workflows/ci.yml
    Expected Result: 含引用
    Failure Indicators: 0 匹配
    Evidence: .omo/evidence/task-19-step-added.txt

  Scenario: 依赖安装扩展（如使用）
    Tool: Bash (grep)
    Preconditions: 上一场景
    Steps:
      1. grep -A2 "pip install" .github/workflows/ci.yml || echo "No pip install"
    Expected Result: 含 pytest / networkx / pydot 等
    Failure Indicators: 缺 pytest 但有 pytest 调用
    Evidence: .omo/evidence/task-19-pip-install.txt
  ```

  **Evidence to Capture**:
  - [ ] YAML 仍有效
  - [ ] 新 step 存在
  - [ ] pip 依赖安装（如添加）

  **Commit**: YES
  - Message: `ci(workflow): add run_all_tests.sh --quick step with python deps`
  - Files: `.github/workflows/ci.yml`
  - Pre-commit: N/A

- [x] 20. 本地工作流 dry-run 验证

  **What to do**:
  - 验证 YAML 结构 + GitHub Actions 工具兼容:
    - `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))"` 完整解析
    - 检查所有 step 的 `run:` 字段是合法 bash
    - 检查 `if:` 表达式语法
  - 模拟 step 执行（不需要 act，但可以手跑验证）:
    - `chmod +x scripts/test/ci_e2e_test.sh && ./scripts/test/ci_e2e_test.sh 2>&1 | head -20`（在本地 Ubuntu 上跑一遍）
    - `chmod +x scripts/test/run_all_tests.sh && ./scripts/test/run_all_tests.sh --quick 2>&1 | tail -10`
  - 检查 `actions/checkout@v4`, `actions/upload-artifact@v4`, `hendrikmuhs/ccache-action@v1.2` 版本仍是最新
  - 输出"CI 集成可工作"最终报告

  **Must NOT do**:
  - 不修改 .yml 文件
  - 不创建 .github/workflows/ 新文件
  - 不跑实际 push

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 验证步骤
  - **Skills**: `["verification-before-completion"]`

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 4 最后
  - **Blocks**: F1-F4
  - **Blocked By**: Tasks 18, 19

  **References**:
  - `.github/workflows/ci.yml` 修改后
  - `scripts/test/ci_e2e_test.sh` 新路径
  - `scripts/test/run_all_tests.sh` 新路径

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: YAML 完整解析
    Tool: Bash (python yaml)
    Preconditions: Tasks 18, 19 完成
    Steps:
      1. python3 -c "
    import yaml
    with open('.github/workflows/ci.yml') as f:
        doc = yaml.safe_load(f)
    print('jobs:', list(doc.get('jobs', {}).keys()))
    for job_name, job in doc.get('jobs', {}).items():
        print(f'  {job_name}: {len(job.get(\"steps\", []))} steps')
    " 2>&1 | tee .omo/evidence/task-20-yaml-structure.txt
    Expected Result: 含 build-and-test 和 code-format 两个 job，build-and-test 含 6+ steps
    Failure Indicators: 解析错误
    Evidence: .omo/evidence/task-20-yaml-structure.txt

  Scenario: ci_e2e_test.sh 本地可跑
    Tool: Bash (run script)
    Preconditions: 上一场景
    Steps:
      1. ./scripts/test/ci_e2e_test.sh 2>&1 | tail -5
    Expected Result: "All E2E tests passed!" 或类似成功
    Failure Indicators: "command not found" 或任何 FAIL
    Evidence: .omo/evidence/task-20-local-ci-e2e.txt

  Scenario: run_all_tests.sh --quick 本地可跑
    Tool: Bash (run script)
    Preconditions: 上一场景
    Steps:
      1. ./scripts/test/run_all_tests.sh --quick 2>&1 | tail -5
    Expected Result: "[SUCCESS] Quick mode complete"
    Failure Indicators: 任何错误
    Evidence: .omo/evidence/task-20-local-quick.txt
  ```

  **Evidence to Capture**:
  - [ ] YAML 结构报告
  - [ ] 本地 ci_e2e_test 输出
  - [ ] 本地 quick 模式输出

  **Commit**: YES (verification tag)
  - Message: `verify(ci): workflow YAML validates and scripts run locally`
  - Files: N/A
  - Pre-commit: All above

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.
>
> **Do NOT auto-proceed after verification. Wait for user's explicit approval before marking work complete.**
> **Never mark F1-F4 as checked before getting user's okay.** Rejection or user feedback -> fix -> re-run -> present again -> wait for okay.

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists (read file, run command). For each "Must NOT Have": search codebase for forbidden patterns — reject with file:line if found. Check evidence files exist in `.omo/evidence/`. Compare deliverables against plan.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run `cmake --build build` + `ctest` + `./scripts/format.sh --check`. Review all changed files for: dead code, broken paths, `as any`/`@ts-ignore` (N/A in C++), commented-out code, unused includes. Check AI slop: excessive comments, over-abstraction, generic names.
  Output: `Build [PASS/FAIL] | Tests [N pass/N fail] | Format [PASS/FAIL] | Files [N clean/N issues] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high`
  Start from clean state. Execute EVERY QA scenario from EVERY task — follow exact steps, capture evidence. Test cross-task integration: do all 5 subdir scripts work? do all demo targets run? does CI workflow YAML validate?
  Output: `Scenarios [N/N pass] | Integration [N/N] | Edge Cases [N tested] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff (`git diff HEAD`). Verify 1:1 — everything in spec was built (no missing), nothing beyond spec was built (no creep). Check "Must NOT do" compliance. Detect cross-task contamination: Task N touching Task M's files. Flag unaccounted changes.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

- **Wave 1 (commits 1-5)**: 5 atomic commits by task
  - `fix(scripts): remove dead executable references from run_all_tests.sh`
  - `build(samples): add streaming_demo and python demo targets`
  - `docs(agents): remove references to non-existent source files`
  - `chore(scripts): clean up orphaned __pycache__ entries`
  - `chore(verify): confirm no other dead path references exist`

- **Wave 2 (commits 6-10)**: 5 atomic commits
  - `archive(samples): move simple1 to docs-archived (DEPRECATED v2.1)`
  - `archive(samples): move simple_hier to docs-archived (orphan)`
  - `build(examples): add example_basic_transaction and example_error_handling targets`
  - `docs(agents): reflect sample archiving and example targets`
  - `verify(phase-a-b): all builds and tests pass`

- **Wave 3 (commits 11-17)**: 7 atomic commits
  - `refactor(scripts): create 5-subdir skeleton`
  - `refactor(scripts): move bash scripts to build/test/pipeline subdirs`
  - `refactor(scripts): move python scripts to topology/stats subdirs`
  - `refactor(scripts): update CMakeLists.txt dispatch`
  - `fix(samples): update demo sys.path for new script locations`
  - `fix(pipeline): update run_full_pipeline.sh script refs`
  - `verify(phase-c): all scripts and ctest pass after refactor`

- **Wave 4 (commits 18-20)**: 3 atomic commits
  - `ci(workflow): add ci_e2e_test.sh step`
  - `ci(workflow): add run_all_tests.sh --quick step`
  - `verify(ci): workflow YAML validates and acts locally`

---

## Success Criteria

### Verification Commands
```bash
# Build
cmake --build build -j$(nproc)              # Expected: 0 errors

# Tests
cd build && ctest --output-on-failure       # Expected: all pass

# Format
./scripts/format.sh --check                 # Expected: 0 files need formatting

# Demo executables (after refactor, paths may change)
./build/bin/stats_demo                      # Expected: non-empty output
./build/bin/traffic_gen_demo                # Expected: non-empty output
./build/bin/streaming_demo                  # Expected: non-empty output (after A2)
./build/bin/examples/example_basic_transaction  # Expected: non-empty output (after B)
./build/bin/examples/example_error_handling     # Expected: non-empty output (after B)

# Python demos (after C, paths may be scripts/{topology,stats}/)
python3 samples/topology_generator_demo.py   # Expected: 0 exit
python3 samples/stats_visualization_demo.py # Expected: 0 exit
python3 samples/stats_watcher_demo.py       # Expected: 0 exit

# Validate
cmake --build build --target validate_topology  # Expected: all PASS

# E2E
./scripts/ci_e2e_test.sh                    # Expected: All E2E tests passed!
./scripts/run_all_tests.sh --quick          # Expected: [SUCCESS] Quick mode complete

# Git status
git status                                  # Expected: clean tree (all committed)
```

### Final Checklist
- [x] All "Must Have" present
- [x] All "Must NOT Have" absent
- [x] All tests pass
- [x] All demos runnable
- [x] CI workflow validates
- [x] 0 ghost references in codebase (grep verification)
- [x] docs-archived/samples-orphaned/ has both directories
- [x] scripts/ has 5 subdirs (build/test/pipeline/topology/stats)
- [x] AGENTS.md reflects all changes
