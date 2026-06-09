# P0-P1 架构债务修复计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix 6 newly discovered architecture/code debt items: ModuleFactory memory leak, chstream_adapter_factory.hh layer violation, tlm_stub include inversion, packet_to_payload path bug, module_factory.cc oversized, and remaining TODO.

**Architecture:** 
- Memory safety: Convert raw pointers to unique_ptr in ModuleFactory
- Layering: Move chstream_adapter_factory.hh from core/ to framework/
- Cleanup: Fix include paths and TODOs
- Refactoring: Split module_factory.cc into logical units

**Tech Stack:** C++17, CMake, Catch2, ASan

---

## Context

基于 [analyze-mode] 全面健康检查，发现 6 项新债务（不在 debt-remediation-plan.md 中）：

| # | 问题 | 严重度 | 文件位置 |
|---|------|--------|----------|
| 1 | ModuleFactory 内存泄漏 | P0 | `src/core/module_factory.cc:533` |
| 2 | chstream_adapter_factory 分层倒置 | P0 | `include/core/chstream_adapter_factory.hh:9-12` |
| 3 | core/ext 引用 tlm_stub | P0 | `include/core/packet.hh:7` |
| 4 | packet_to_payload 路径错误 | P1 | `include/core/ext/packet_to_payload.hh:6` |
| 5 | module_factory.cc 过大(1062行) | P1 | `src/core/module_factory.cc` |
| 6 | hierarchy_root TODO | P1 | `src/core/module_factory.cc:439` |

---

## Work Objectives

### Core Objective
修复 ASan 发现的内存泄漏（26240 bytes / 168 allocations），同时修复架构分层倒置和代码质量问题。

### Concrete Deliverables
1. ModuleFactory 使用 `unique_ptr` 管理对象生命周期
2. `chstream_adapter_factory.hh` 从 `core/` 移至 `framework/`
3. `tlm_stub.hh` 引用统一为 `core/` 路径
4. 修复 `packet_to_payload.hh` 路径错误
5. 拆分 `module_factory.cc` 为多个逻辑文件
6. 移除或实现 `module_factory.cc:439` TODO

### Definition of Done
- [ ] ASan build: `./build-asan/bin/cpptlm_tests` 无泄漏报告
- [ ] 标准 build: `./build/bin/cpptlm_tests ~"[crossbar]"` 全部通过
- [ ] 无新的编译警告

### Must Have
- 内存泄漏修复（ASan clean）
- 分层架构修复（core 不再依赖 framework/tlm）
- 所有现有测试通过

### Must NOT Have
- 不引入新的 API 变更（保持向后兼容）
- 不修改测试行为（仅修复实现）
- 不增加新的依赖

---

## Verification Strategy

### Test Decision
- **Infrastructure exists**: YES
- **Automated tests**: 使用 ASan 作为主要验证手段
- **Framework**: ASan via `-DUSE_ASAN=ON`

### QA Policy
每个任务包含 Agent-Executed QA Scenario：
- 构建验证: `cmake --build build -j$(nproc)`
- ASan 验证: `./build-asan/bin/cpptlm_tests` 无泄漏
- 测试验证: `./build/bin/cpptlm_tests ~"[crossbar]"` 通过

---

## Execution Strategy

### 依赖关系

```
Task 1 (packet_to_payload path fix) ──► 独立，最低风险
Task 2 (chstream_adapter_factory move) ──► 独立，需更新引用
Task 3 (tlm_stub path fix) ──► 独立
Task 4 (TODO removal) ──► 独立
Task 5 (module_factory leak fix) ──► 依赖 Task 2（若移动了 chstream）
Task 6 (module_factory.cc split) ──► 依赖 Task 5

推荐执行顺序:
  Wave 1: Task 1 + Task 2 + Task 3 + Task 4 (全部独立)
  Wave 2: Task 5 (依赖 Wave 1)
  Wave 3: Task 6 (依赖 Wave 2)
```

### Agent Dispatch Summary

- Wave 1: 4 个 quick 任务并行
- Wave 2: 1 个 deep 任务
- Wave 3: 1 个 deep 任务（文件拆分）

---

## TODOs

### Task 1: P1-1 — 修复 packet_to_payload.hh 路径错误

**Files:**
- Modify: `include/core/ext/packet_to_payload.hh:6`

**What to do:**
修复错误的 include 路径。当前 `"packet/packet.hh"` 不存在。

```cpp
// BEFORE (line 6):
#include "packet/packet.hh"

// AFTER:
#include "packet.hh"
```

**Why:** 当编译 `include/core/ext/packet_to_payload.hh` 时，include 搜索路径已包含 `include/core/`，所以 `"packet.hh"` 可正确解析到 `include/core/packet.hh`。

**Recommended Agent Profile:**
- **Category**: `quick`
- **Reason**: 单文件单行修改

**Parallelization:**
- **Can Run In Parallel**: YES (Wave 1)
- **Blocks**: None
- **Blocked By**: None

**QA Scenarios:**
```
Scenario: Build succeeds after path fix
  Tool: Bash
  Steps:
    1. cmake -S . -B build-test -DCMAKE_BUILD_TYPE=Release
    2. cmake --build build-test -j$(nproc) --target cpptlm_core
  Expected Result: Build succeeds with no "packet/packet.hh" not found error
```

**Commit**: YES
- Message: `fix(core): correct include path in packet_to_payload.hh`
- Files: `include/core/ext/packet_to_payload.hh`

---

### Task 2: P0-2 — 修复 chstream_adapter_factory 分层倒置

**Files:**
- Create: `include/framework/chstream_adapter_factory.hh` (move from core/)
- Delete: `include/core/chstream_adapter_factory.hh`
- Modify: 所有引用此文件的位置

**What to do:**
1. 将 `include/core/chstream_adapter_factory.hh` 物理移动到 `include/framework/chstream_adapter_factory.hh`
2. 文件内容无需修改（只需移动位置）
3. 更新所有 include 引用：
   - `include/core/module_factory.hh` — 检查是否有引用
   - `src/core/module_factory.cc` — Step 7 中创建 adapter 时
   - `include/chstream_register.hh` — 注册宏中
4. 确保 CMakeLists.txt 显式源文件列表更新（如有）

**Must NOT do:**
- 不要修改类定义或 API
- 不要修改测试代码（测试应继续编译）

**Recommended Agent Profile:**
- **Category**: `quick`
- **Reason**: 文件移动 + include 路径更新

**Parallelization:**
- **Can Run In Parallel**: YES (Wave 1)
- **Blocks**: None
- **Blocked By**: None

**QA Scenarios:**
```
Scenario: Build after file move
  Tool: Bash
  Steps:
    1. rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    2. cmake --build build -j$(nproc)
    3. ./build/bin/cpptlm_tests "[chstream]" --output-on-failure
  Expected Result: Build succeeds, chstream tests pass
```

**Commit**: YES
- Message: `refactor(framework): move chstream_adapter_factory from core/ to framework/`
- Files: `include/core/chstream_adapter_factory.hh` → `include/framework/chstream_adapter_factory.hh`

---

### Task 3: P0-3 — 修复 core/ext 引用 tlm_stub

**Files:**
- Modify: `include/core/packet.hh:7`
- Modify: `include/core/ext/cmd_exts.hh:6`

**What to do:**
`tlm_stub.hh` 仅包含标准库头文件（无内部依赖），且仅在 `#ifdef USE_SYSTEMC_STUB` 下使用。修复方案：将 `tlm_stub.hh` 复制/移动到 `include/core/tlm_compat_stub.hh`，并更新引用。

```cpp
// packet.hh BEFORE (lines 6-10):
#ifdef USE_SYSTEMC_STUB
#include "tlm/tlm_stub.hh"
#else
#include "tlm.h"
#endif

// packet.hh AFTER:
#ifdef USE_SYSTEMC_STUB
#include "core/tlm_compat_stub.hh"
#else
#include "tlm.h"
#endif
```

**Why:** `tlm_stub.hh` 是一个兼容性 stub（当 USE_SYSTEMC=OFF 时提供 tlm 类型定义）。它不包含任何 tlm 模块逻辑，将其移动到 core/ 不违反分层。

**Recommended Agent Profile:**
- **Category**: `quick`
- **Reason**: 文件移动 + include 更新

**Parallelization:**
- **Can Run In Parallel**: YES (Wave 1)
- **Blocks**: None
- **Blocked By**: None

**QA Scenarios:**
```
Scenario: Stub build still works
  Tool: Bash
  Steps:
    1. cmake -S . -B build-stub -DCMAKE_BUILD_TYPE=Release -DUSE_SYSTEMC=OFF -DUSE_SYSTEMC_STUB=ON
    2. cmake --build build-stub -j$(nproc)
  Expected Result: Build succeeds (stub mode compiles)
```

**Commit**: YES
- Message: `refactor(core): move tlm_stub to core/ to fix layer violation`
- Files: `include/tlm/tlm_stub.hh` → `include/core/tlm_compat_stub.hh`

---

### Task 4: P1-3 — 移除/实现 TODO (module_factory.cc:439)

**Files:**
- Modify: `src/core/module_factory.cc:439`

**What to do:**
读取 TODO 上下文，确定是移除还是实现。

```bash
# 读取 TODO 上下文
sed -n '435,445p' src/core/module_factory.cc
```

根据上下文决定：
- 如果 hierarchy_root 确实不需要存储：移除 TODO 注释
- 如果需要存储：添加 `std::unique_ptr<TopologyNode> hierarchy_root_` 成员并在 instantiateAll 中赋值

**Recommended Agent Profile:**
- **Category**: `quick`
- **Reason**: 单行 TODO 处理

**Parallelization:**
- **Can Run In Parallel**: YES (Wave 1)
- **Blocks**: None
- **Blocked By**: None

**QA Scenarios:**
```
Scenario: Build succeeds after TODO removal
  Tool: Bash
  Steps:
    1. cmake --build build -j$(nproc)
    2. grep -n "TODO" src/core/module_factory.cc
  Expected Result: Build succeeds, no TODO in module_factory.cc (or TODO is resolved)
```

**Commit**: YES
- Message: `chore(module_factory): resolve hierarchy_root TODO`
- Files: `src/core/module_factory.cc`

---

### Task 5: P0-1 — 修复 ModuleFactory 内存泄漏

**Files:**
- Modify: `include/core/module_factory.hh` (变更 instances 类型)
- Modify: `src/core/module_factory.cc` (更新所有 usages)

**What to do:**
将 `ModuleFactory` 中的 raw pointer 存储改为 `unique_ptr`：

```cpp
// module_factory.hh BEFORE:
std::unordered_map<std::string, SimObject*> instances;

// module_factory.hh AFTER:
std::unordered_map<std::string, std::unique_ptr<SimObject>> instances_;
```

然后在 `module_factory.cc` 中：
1. `object_instances[name] = new_module;` → `object_instances_[name] = std::unique_ptr<SimObject>(new_module);`
2. 所有 `object_instances[name]` 引用 → `object_instances_[name].get()` 或保持引用不变（map 的 operator[] 返回 unique_ptr&，可通过 .get() 获取 raw ptr）
3. 确保 ModuleGroup::registerInstance 等调用不受影响

**Must NOT do:**
- 不要改变 SimObject 的生命周期语义（仍由 ModuleFactory 管理）
- 不要修改测试代码（除非测试直接 delete SimObject）

**Recommended Agent Profile:**
- **Category**: `deep`
- **Reason**: 影响面广（28 处 usages），需要仔细验证

**Parallelization:**
- **Can Run In Parallel**: NO (Wave 2)
- **Blocks**: None
- **Blocked By**: Task 2 (chstream_adapter_factory 移动，如有引用)

**QA Scenarios:**
```
Scenario: ASan detects no leaks after fix
  Tool: Bash
  Steps:
    1. cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
    2. cmake --build build-asan -j$(nproc)
    3. ./build-asan/bin/cpptlm_tests "[connection]"
  Expected Result: All tests pass, no "Direct leak" in ASan output

Scenario: Full test suite passes
  Tool: Bash
  Steps:
    1. cmake --build build -j$(nproc)
    2. ./build/bin/cpptlm_tests ~"[crossbar]"
  Expected Result: All tests pass (14519 assertions, 545 cases)
```

**Commit**: YES
- Message: `fix(memory): use unique_ptr for ModuleFactory object_instances`
- Files: `include/core/module_factory.hh`, `src/core/module_factory.cc`

---

### Task 6: P1-2 — 拆分 module_factory.cc (1062 行)

**Files:**
- Create: `src/core/module_factory_instantiate.cc` (instantiateAll + instantiate + helper)
- Create: `src/core/module_factory_connect.cc` (resolveConnections + connection helpers)
- Create: `src/core/module_factory_validate.cc` (validateConfig + mergeConfigs + processExtends)
- Modify: `src/core/module_factory.cc` (保留工厂注册 + startAllTicks + 析构)
- Modify: `src/CMakeLists.txt` (添加新源文件)

**What to do:**
按功能将 1062 行拆分为 4 个逻辑单元：

1. **module_factory.cc** (保留 ~200 行):
   - 构造函数/析构函数
   - registerObject / registerModule / unregisterObject / unregisterModule
   - clearAllObjects / clearAllModules
   - startAllTicks
   - getInstance / getObjectRegistry / getModuleRegistry
   - debug_config

2. **module_factory_instantiate.cc** (~400 行):
   - instantiateAll (主入口)
   - instantiate (单个模块创建)
   - loadPlugins
   - processExtends
   - mergeConfigs
   - validateConfig

3. **module_factory_connect.cc** (~300 行):
   - resolveConnections
   - parsePortSpec
   - check_port_compatibility
   - validate_nic_pe_connection
   - 所有连接/端口相关 helper

4. **module_factory_validate.cc** (~200 行):
   - validateConfig (JSON Schema 验证)
   - validate_module_params
   - 参数验证 helper

**Must NOT do:**
- 不改变任何函数签名
- 不改变任何行为
- 不引入新的依赖

**Recommended Agent Profile:**
- **Category**: `deep`
- **Reason**: 大范围重构，需要保持行为完全一致

**Parallelization:**
- **Can Run In Parallel**: NO (Wave 3)
- **Blocks**: None
- **Blocked By**: Task 5 (内存泄漏修复应先完成，避免 merge 冲突)

**QA Scenarios:**
```
Scenario: Build and all tests pass after split
  Tool: Bash
  Steps:
    1. rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    2. cmake --build build -j$(nproc)
    3. ./build/bin/cpptlm_tests ~"[crossbar]"
  Expected Result: All tests pass (14519 assertions, 545 cases)

Scenario: Integration tests pass
  Tool: Bash
  Steps:
    1. ./build/bin/cpptlm_tests "[phase6]"
  Expected Result: 9 test cases pass (Cache→Crossbar→Memory end-to-end)
```

**Commit**: YES
- Message: `refactor(module_factory): split 1062-line file into logical units`
- Files: `src/core/module_factory*.cc`, `src/CMakeLists.txt`

---

## Final Verification Wave

- [ ] F1. **ASan Verification** — `unspecified-high`
  ```bash
  cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
  cmake --build build-asan -j$(nproc)
  ./build-asan/bin/cpptlm_tests
  # Expected: 0 leaks reported
  ```

- [ ] F2. **Full Test Suite** — `quick`
  ```bash
  cmake --build build -j$(nproc)
  ./build/bin/cpptlm_tests ~"[crossbar]"
  # Expected: All tests passed (14519 assertions in 545 test cases)
  ```

- [ ] F3. **Build Warnings Check** — `quick`
  ```bash
  cmake --build build -j$(nproc) 2>&1 | grep -i "warning:"
  # Expected: No new warnings (compare with baseline)
  ```

- [ ] F4. **Architecture Layer Check** — `oracle`
  ```bash
  grep -rn "#include" include/core/ | grep -E "framework/|tlm/"
  # Expected: 0 matches (core should not depend on framework or tlm)
  ```

---

## Commit Strategy

```bash
# Wave 1 commits
git add include/core/ext/packet_to_payload.hh
git commit -m "fix(core): correct include path in packet_to_payload.hh"

git add include/core/chstream_adapter_factory.hh include/framework/chstream_adapter_factory.hh
git commit -m "refactor(framework): move chstream_adapter_factory from core/ to framework/"

git add include/tlm/tlm_stub.hh include/core/tlm_compat_stub.hh
git commit -m "refactor(core): move tlm_stub to core/ to fix layer violation"

git add src/core/module_factory.cc
git commit -m "chore(module_factory): resolve hierarchy_root TODO"

# Wave 2 commit
git add include/core/module_factory.hh src/core/module_factory.cc
git commit -m "fix(memory): use unique_ptr for ModuleFactory object_instances"

# Wave 3 commit
git add src/core/module_factory*.cc src/CMakeLists.txt
git commit -m "refactor(module_factory): split 1062-line file into logical units"
```

---

## Success Criteria

### Verification Commands
```bash
# 1. ASan build clean
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON -DUSE_SYSTEMC=OFF
cmake --build build-asan -j$(nproc)
./build-asan/bin/cpptlm_tests 2>&1 | grep -i "leak" || echo "ASan: CLEAN"

# 2. Standard build passes
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DUSE_SYSTEMC=OFF
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests ~"[crossbar]"

# 3. Architecture layer clean
grep -rn "#include" include/core/ | grep -E "framework/|tlm/" | wc -l
# Expected: 0
```

### Final Checklist
- [ ] ASan: 0 leaks reported
- [ ] All tests: 14519 assertions, 545 cases pass
- [ ] Architecture: core/ includes no framework/ or tlm/
- [ ] No new compiler warnings
- [ ] chstream_register.hh 和 modules.hh 的注册宏完整
- [ ] Phase 6 集成测试通过（9 cases）
