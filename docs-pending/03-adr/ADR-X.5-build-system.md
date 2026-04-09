# ADR-X.5: 构建系统

> **版本**: 2.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待确认  
> **影响**: v2.0 - 项目构建与开发体验

---

## 1. 核心问题

在混合仿真系统中，构建系统需要解决以下问题：

1. **构建工具选择**: CMake + Ninja / CMake + Make / Bazel / 其他？
2. **SystemC 支持**: 可选启用 / 强制依赖？
3. **测试框架**: Catch2 / GTest / 其他？
4. **依赖管理**: 手动 / FetchContent / 包管理器？
5. **示例组织**: 独立子目录 / 集成到主构建？
6. **跨平台**: Linux / macOS / Windows？
7. **CI/CD**: GitHub Actions / GitLab CI / 其他？

---

## 2. 行业调研

### 2.1 Gem5 构建系统

**Gem5 方式**: SCons + 自定义配置

```python
# Gem5 SConstruct
import m5.build

env = m5.build.Environment()
env.Append(CPPPATH=['#include'])
env.Program('gem5', Glob('src/main.cc') + env.Object('src/*'))
```

**特点**:
- ✅ 高度定制化
- ❌ SCons 学习曲线陡峭
- ❌ 构建速度较慢
- ❌ 非标准工具

**构建时间**:
- 完整构建：~30 分钟
- 增量构建：~2-5 分钟

---

### 2.2 SystemC 标准构建

**SystemC 方式**: CMake + Make

```cmake
# SystemC CMakeLists.txt
cmake_minimum_required(VERSION 3.5)
project(systemc)

add_library(systemc STATIC ${SYSTEMC_SOURCES})
target_include_directories(systemc PUBLIC include)
```

**特点**:
- ✅ 标准 CMake
- ✅ 跨平台
- ⚠️ 默认 Make 构建较慢

---

### 2.3 LLVM 构建系统

**LLVM 方式**: CMake + Ninja

```cmake
# LLVM CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(LLVM)

set(CMAKE_GENERATOR Ninja)
add_llvm_component_library(Core ...)
```

**特点**:
- ✅ CMake + Ninja（快速）
- ✅ 组件化设计
- ✅ 并行构建
- ✅ 标准工具链

**构建时间**:
- 完整构建：~15 分钟（Ninja）
- 增量构建：~10-30 秒

---

### 2.4 现代 C++ 项目趋势

| 项目 | 构建系统 | 测试框架 | 依赖管理 |
|------|---------|---------|---------|
| **fmt** | CMake + Ninja | GoogleTest | FetchContent |
| **spdlog** | CMake + Ninja | Catch2 | FetchContent |
| **nlohmann/json** | CMake | Catch2 | 无依赖 |
| **eosio** | CMake + Ninja | Catch2 | ExternalProject |

**趋势总结**:
- CMake 是事实标准（90%+ 项目）
- Ninja 是首选生成器（构建速度快 3-5 倍）
- Catch2 是轻量级测试首选
- FetchContent 是依赖管理趋势

---

## 3. 需求场景分析

### 场景 1: 日常开发构建（高频）

```
需求：每天数十次编译，需要快速反馈

当前代码规模:
- 头文件：~50 个
- 源文件：~30 个
- 总代码量：~10K LOC

构建时间目标:
- 完整构建：<5 分钟
- 增量构建：<30 秒

技术需求:
- 并行编译
- 增量编译
- 依赖追踪准确
```

**评估**: CMake + Ninja 完全满足需求

---

### 场景 2: SystemC 可选启用（中频）

```
需求：部分模块需要 SystemC 支持，部分不需要

使用场景:
- 纯 TLM 仿真：不需要 SystemC
- 混合仿真：需要 SystemC
- 单元测试：不需要 SystemC

技术需求:
- CMake 选项控制
- 条件编译
- 链接时可选
```

**评估**: CMake option + 条件编译可完美支持

---

### 场景 3: 单元测试（高频）

```
需求：每个模块有单元测试，快速执行

测试规模:
- 单元测试：~50 个
- 集成测试：~10 个
- 执行时间：<1 分钟

技术需求:
- 测试发现自动
- 测试隔离
- 覆盖率报告
```

**评估**: Catch2 轻量级，满足需求

---

### 场景 4: 跨平台构建（低频）

```
需求：支持 Linux/macOS 开发

目标平台:
- Linux (Ubuntu 20.04+): 主要开发平台
- macOS (11+): 部分用户使用
- Windows: 暂不支持

技术需求:
- 跨平台 CMake
- 依赖库跨平台
```

**评估**: CMake 原生支持跨平台

---

### 场景 5: CI/CD 集成（中频）

```
需求：自动构建、测试、发布

CI 流程:
- 每次 PR 触发构建
- 运行单元测试
- 代码风格检查
- 生成文档

技术需求:
- GitHub Actions 支持
- 缓存依赖
- 并行任务
```

**评估**: CMake + GitHub Actions 成熟方案

---

## 4. 方案对比

### 4.1 构建工具对比

| 工具 | 优点 | 缺点 | 适用场景 |
|------|------|------|---------|
| **CMake + Ninja** ✅ | 快速、标准、跨平台 | Ninja 需单独安装 | ✅ 推荐 |
| CMake + Make | 无需额外工具 | 构建速度慢 | 备选 |
| Bazel | 增量构建快、可重现 | 学习曲线陡、非标准 | ❌ 不推荐 |
| SCons (Gem5) | 高度定制 | 慢、非标准 | ❌ 不推荐 |
| Meson | 快速、现代 | 生态较小 | ❌ 不推荐 |

---

### 4.2 测试框架对比

| 框架 | 优点 | 缺点 | 适用场景 |
|------|------|------|---------|
| **Catch2** ✅ | 轻量级、header-only、现代 C++ | 功能较简单 | ✅ 推荐（单元测试） |
| GoogleTest | 功能完整、生态好 | 较重、编译慢 | 备选（集成测试） |
| Boost.Test | 功能完整 | 依赖 Boost | ❌ 不推荐 |
| doctest | 最轻量 | 生态小 | ❌ 不推荐 |

---

### 4.3 依赖管理对比

| 方式 | 优点 | 缺点 | 适用场景 |
|------|------|------|---------|
| **FetchContent** ✅ | CMake 内置、简单 | 首次下载慢 | ✅ 推荐 |
| ExternalProject | CMake 内置、灵活 | 配置复杂 | 备选 |
| 包管理器 (vcpkg) | 集中管理 | 需额外工具 | ⏳ 未来 |
| 手动下载 | 简单直接 | 易出错 | ❌ 不推荐 |

---

## 5. 推荐方案

### 5.1 核心设计

```
┌─────────────────────────────────────────────────────────────┐
│  构建系统架构                                                │
├─────────────────────────────────────────────────────────────┤
│  构建工具：CMake 3.16+ + Ninja                              │
│  - 并行编译                                                  │
│  - 增量构建 <30 秒                                           │
│  - 跨平台（Linux/macOS）                                    │
├─────────────────────────────────────────────────────────────┤
│  测试框架：Catch2（单元测试） + 自定义（集成测试）           │
│  - header-only，无需编译                                     │
│  - 现代 C++ 语法                                             │
│  - 覆盖率报告                                                │
├─────────────────────────────────────────────────────────────┤
│  依赖管理：FetchContent + 系统包                            │
│  - SystemC: 系统包（apt/dnf）                               │
│  - Catch2: FetchContent                                     │
│  - nlohmann/json: FetchContent                              │
├─────────────────────────────────────────────────────────────┤
│  CI/CD: GitHub Actions                                      │
│  - 自动构建/测试                                             │
│  - 缓存依赖                                                  │
│  - 多平台矩阵                                                │
└─────────────────────────────────────────────────────────────┘
```

---

### 5.2 目录结构

```
CppTLM/
├── CMakeLists.txt                 # 根 CMake 配置
├── cmake/
│   ├── modules/
│   │   └── FindSystemC.cmake      # SystemC find 模块
│   └── deps/
│       ├── Catch2.cmake           # Catch2 依赖
│       └── json.cmake             # nlohmann/json 依赖
├── include/
│   ├── core/                      # 核心头文件
│   ├── modules/                   # 模块头文件
│   ├── framework/                 # 框架头文件
│   └── ext/                       # Extension 头文件
├── src/
│   ├── core/                      # 核心源文件
│   ├── modules/                   # 模块源文件
│   └── main.cpp                   # 仿真入口
├── tests/
│   ├── CMakeLists.txt             # 测试配置
│   ├── unit/                      # 单元测试
│   │   ├── test_cache.cpp
│   │   ├── test_crossbar.cpp
│   │   └── ...
│   └── integration/               # 集成测试
│       ├── test_coherence.cpp
│       └── ...
├── examples/
│   ├── CMakeLists.txt             # 示例配置
│   ├── cache_system/              # Cache 系统示例
│   ├── noc_system/                # NoC 系统示例
│   └── coherence_system/          # 一致性系统示例
├── docs/
│   ├── CMakeLists.txt             # 文档配置（可选）
│   └── ...
├── scripts/
│   ├── build.sh                   # 构建脚本
│   ├── test.sh                    # 测试脚本
│   └── format.sh                  # 代码格式化
└── .github/
    └── workflows/
        └── ci.yml                 # GitHub Actions 配置
```

---

### 5.3 根 CMakeLists.txt

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(CppTLM 
    VERSION 2.0.0
    DESCRIPTION "C++ TLM Hybrid Simulation Framework"
    LANGUAGES CXX
)

# ========== ccache 支持（加速编译） ==========
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache found: ${CCACHE_PROGRAM}")
    message(STATUS "ccache stats:")
    execute_process(COMMAND ${CCACHE_PROGRAM} -s)
else()
    message(STATUS "ccache not found - install for faster builds")
endif()

# ========== 构建选项 ==========
option(USE_SYSTEMC "Enable SystemC support" OFF)
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_DOCS "Build documentation" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# ========== C++ 标准 ==========
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ========== 编译选项 ==========
add_compile_options(-Wall -Wextra -Wpedantic)
add_compile_options(-O2 -g)  # 优化 + 调试信息

# ========== 依赖查找 ==========
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# SystemC（可选，使用本地头文件）
if(USE_SYSTEMC)
    # 使用项目内 SystemC 头文件（用户已提供）
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/systemc")
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/systemc)
        add_compile_definitions(USE_SYSTEMC)
        message(STATUS "SystemC enabled (local headers)")
    else()
        message(FATAL_ERROR "SystemC headers not found in external/systemc")
    endif()
endif()

# ========== 包含目录 ==========
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ========== 子目录 ==========
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_DOCS)
    add_subdirectory(docs)
endif()

# ========== 安装规则 ==========
install(DIRECTORY include/ DESTINATION include/cpp-tlm)
install(TARGETS cpptlm_core DESTINATION lib)

# ========== 打印配置 ==========
message(STATUS "")
message(STATUS "CppTLM Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  SystemC: ${USE_SYSTEMC}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "  Examples: ${BUILD_EXAMPLES}")
message(STATUS "  Coverage: ${ENABLE_COVERAGE}")
message(STATUS "")
```

---

### 5.4 src/CMakeLists.txt

```cmake
# src/CMakeLists.txt
# 核心库
add_library(cpptlm_core STATIC
    core/sim_object.cc
    core/event_queue.cc
    core/packet.cc
    core/simple_port.cc
    core/master_port.cc
    core/slave_port.cc
    # ...
)

target_include_directories(cpptlm_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# SystemC 头文件（可选，仅头文件无需链接）
if(USE_SYSTEMC)
    target_compile_definitions(cpptlm_core PUBLIC USE_SYSTEMC)
endif()

# 主程序
add_executable(cpptlm_sim main.cpp)
target_link_libraries(cpptlm_sim PRIVATE cpptlm_core)

# 安装
install(TARGETS cpptlm_core cpptlm_sim
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
```

---

### 5.5 tests/CMakeLists.txt

```cmake
# tests/CMakeLists.txt
# FetchContent 获取 Catch2
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# 单元测试可执行文件
add_executable(cpptlm_tests
    unit/test_cache.cpp
    unit/test_crossbar.cpp
    unit/test_memory.cpp
    unit/test_packet.cpp
    # ...
)

target_link_libraries(cpptlm_tests PRIVATE
    cpptlm_core
    Catch2::Catch2WithMain
)

# 启用测试发现
include(CTest)
include(Catch)
catch_discover_tests(cpptlm_tests)

# 覆盖率（可选）
if(ENABLE_COVERAGE)
    target_compile_options(cpptlm_tests PRIVATE --coverage)
    target_link_options(cpptlm_tests PRIVATE --coverage)
endif()

# 集成测试（单独可执行文件）
add_executable(cpptlm_integration_tests
    integration/test_coherence.cpp
    integration/test_deadlock.cpp
)

target_link_libraries(cpptlm_integration_tests PRIVATE
    cpptlm_core
    Catch2::Catch2WithMain
)

catch_discover_tests(cpptlm_integration_tests)
```

---

### 5.6 examples/CMakeLists.txt

```cmake
# examples/CMakeLists.txt
# Cache 系统示例
add_executable(example_cache
    cache_system/main.cpp
)
target_link_libraries(example_cache PRIVATE cpptlm_core)

# NoC 系统示例
add_executable(example_noc
    noc_system/main.cpp
)
target_link_libraries(example_noc PRIVATE cpptlm_core)

# 一致性系统示例
add_executable(example_coherence
    coherence_system/main.cpp
)
target_link_libraries(example_coherence PRIVATE cpptlm_core)

# 安装示例（可选）
install(TARGETS example_cache example_noc example_coherence
    RUNTIME DESTINATION bin/examples
)
```

---

### 5.7 构建脚本

```bash
#!/bin/bash
# scripts/build.sh

set -e

# 默认配置
BUILD_TYPE=${BUILD_TYPE:-Release}
USE_SYSTEMC=${USE_SYSTEMC:-OFF}
BUILD_TESTS=${BUILD_TESTS:-ON}

# 创建构建目录
mkdir -p build
cd build

# 配置
cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DUSE_SYSTEMC=$USE_SYSTEMC \
    -DBUILD_TESTS=$BUILD_TESTS \
    -DBUILD_EXAMPLES=ON \
    "$@"

# 构建
ninja

echo ""
echo "Build completed successfully!"
echo "  Build type: $BUILD_TYPE"
echo "  SystemC: $USE_SYSTEMC"
echo ""
```

```bash
#!/bin/bash
# scripts/test.sh

set -e

cd build

# 运行测试
ctest --output-on-failure "$@"

echo ""
echo "Tests completed!"
echo ""
```

```bash
#!/bin/bash
# scripts/format.sh

# 代码格式化（clang-format）
find include src tests -name "*.hpp" -o -name "*.h" -o -name "*.cpp" -o -name "*.cc" | \
    xargs clang-format -i

echo "Code formatted!"
```

---

### 5.8 GitHub Actions CI 配置

```yaml
# .github/workflows/ci.yml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    
    strategy:
      matrix:
        build-type: [Release, Debug]
        use-systemc: [ON, OFF]
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y ninja-build ccache
        # SystemC 不需要安装（使用项目内头文件）
    
    - name: Configure
      run: |
        cmake -S . -B build \
          -G Ninja \
          -DCMAKE_BUILD_TYPE=${{ matrix.build-type }} \
          -DUSE_SYSTEMC=${{ matrix.use-systemc }} \
          -DBUILD_TESTS=ON
    
    - name: Build
      run: cmake --build build
    
    - name: Test
      run: ctest --test-dir build --output-on-failure
    
    - name: Upload test results
      if: always()
      uses: actions/upload-artifact@v3
      with:
        name: test-results-${{ matrix.build-type }}-${{ matrix.use-systemc }}
        path: build/Testing/

  code-format:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install clang-format
      run: sudo apt-get install -y clang-format
    
    - name: Check format
      run: |
        ./scripts/format.sh
        git diff --exit-code
```

---

## 6. 构建性能目标

| 指标 | 目标 | 当前估计 |
|------|------|---------|
| **完整构建** | <5 分钟 | ~3 分钟 |
| **增量构建** | <30 秒 | ~10-20 秒 |
| **单元测试** | <1 分钟 | ~30 秒 |
| **集成测试** | <5 分钟 | ~2 分钟 |
| **CI 总时间** | <10 分钟 | ~8 分钟 |

---

## 7. 需要确认的问题

| 问题 | 选项 | 推荐 | 决策 |
|------|------|------|------|
| **Q1**: 构建工具？ | A) CMake+Ninja / B) CMake+Make / C) Bazel | **A) CMake+Ninja** | ✅ 已确认 |
| **Q2**: SystemC 支持？ | A) 可选 / B) 强制 | **A) 可选** | ✅ 已确认 |
| **Q3**: SystemC 来源？ | A) 系统包 / B) 项目内头文件 | **B) 项目内头文件** | ✅ 已确认 |
| **Q4**: ccache 支持？ | A) 需要 / B) 不需要 | **A) 需要** | ✅ 已确认 |
| **Q5**: 测试框架？ | A) Catch2 / B) GTest / C) 混合 | **A) Catch2** | ✅ 已确认 |
| **Q6**: 依赖管理？ | A) FetchContent / B) 手动 / C) vcpkg | **A) FetchContent** | ✅ 已确认 |
| **Q7**: 示例组织？ | A) 独立子目录 / B) 集成 | **A) 独立子目录** | ✅ 已确认 |
| **Q8**: CI/CD? | A) GitHub Actions / B) GitLab CI | **A) GitHub Actions** | ✅ 已确认 |

---

## 8. 与现有架构整合

| 架构 | 整合方式 |
|------|---------|
| **模块系统** | 编译时链接到 cpptlm_core 库 |
| **插件系统** | v2.1 支持动态库（CMake 共享库） |
| **测试系统** | Catch2 单元测试 + 集成测试 |

---

## 9. 实施计划

### Phase 1: 基础构建（v2.0）

**内容**:
- [ ] 根 CMakeLists.txt
- [ ] src/CMakeLists.txt
- [ ] 构建脚本（build.sh, test.sh）
- [ ] GitHub Actions CI

**工期**: 2 天

---

### Phase 2: 测试框架（v2.0）

**内容**:
- [ ] tests/CMakeLists.txt
- [ ] Catch2 集成
- [ ] 单元测试模板
- [ ] 覆盖率支持（可选）

**工期**: 2 天

---

### Phase 3: 示例与文档（v2.0）

**内容**:
- [ ] examples/CMakeLists.txt
- [ ] 示例程序（3 个）
- [ ] 文档构建（可选）

**工期**: 2 天

---

## 10. 相关文档

| 文档 | 位置 |
|------|------|
| CMake 配置 | `CMakeLists.txt` |
| 构建脚本 | `scripts/build.sh` |
| CI 配置 | `.github/workflows/ci.yml` |
| ADR-X 汇总 | `ADR-X-SUMMARY.md` |

---

## 11. 决策汇总

**v2.0 决策**:
- ✅ 构建工具：CMake 3.16+ + Ninja
- ✅ ccache：自动检测并启用（加速编译）
- ✅ SystemC：可选启用（USE_SYSTEMC 选项）
- ✅ SystemC 来源：项目内头文件（`external/systemc/`）
- ✅ 测试框架：Catch2（单元测试）
- ✅ 依赖管理：FetchContent（Catch2, nlohmann/json）
- ✅ 示例组织：独立子目录
- ✅ CI/CD: GitHub Actions

**构建命令**:
```bash
# 标准构建（自动使用 ccache）
./scripts/build.sh

# 启用 SystemC
./scripts/build.sh -DUSE_SYSTEMC=ON

# 运行测试
./scripts/test.sh

# 覆盖率
./scripts/build.sh -DENABLE_COVERAGE=ON

# 查看 ccache 统计
ccache -s
```

---

**下一步**: 请老板确认构建系统方案
