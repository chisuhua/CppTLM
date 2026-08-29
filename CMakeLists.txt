# CppTLM - C++ TLM Hybrid Simulation Framework
# Version: 2.1.0
#
# Build system: CMake 3.16+ with optional Ninja + ccache

cmake_minimum_required(VERSION 3.16)
project(CppTLM
    VERSION 2.1.0
    DESCRIPTION "C++ TLM Hybrid Simulation Framework"
    LANGUAGES CXX
)

# ==============================================================================
# ccache 支持（自动检测，加速编译）
# ==============================================================================
find_program(CCACHE_PROGRAM ccache)
option(USE_SYSTEMC_STUB "Use TLM stub implementation" ON)
if(USE_SYSTEMC_STUB)
    add_compile_definitions(USE_SYSTEMC_STUB)
endif()
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
else()
    message(STATUS "ccache not found — install for faster builds (apt install ccache)")
endif()

# ==============================================================================
# C++ 标准
# ==============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# 构建选项
# ==============================================================================
option(BUILD_TESTS    "Build unit tests"                     ON)
option(BUILD_EXAMPLES "Build example programs"               ON)
option(BUILD_DOCS     "Build documentation (Doxygen)"        OFF)
option(ENABLE_COVERAGE "Enable code coverage (gcov)"         OFF)
option(USE_ASAN       "Enable AddressSanitizer (Debug only)" OFF)
option(BUILD_RTL "Build CppHDL RTL bridge (Spike)" OFF)

# S1: PTX-EMU submodule 集成 (默认 OFF — submodule 未初始化时 ON 会全员
# configure 失败; GPU 集成开发者/CI 显式 -DCPPTLM_WITH_PTX_EMU=ON)
option(CPPTLM_WITH_PTX_EMU "Build PTX-EMU core as static lib (S1)" OFF)

# ==============================================================================
# 编译选项
# ==============================================================================
add_compile_options(-Wall -Wextra -Wpedantic)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -DDEBUG_PRINT)
else()
    add_compile_options(-O2 -g)
endif()

# AddressSanitizer (ASan) — 仅 Debug 构建生效
if(USE_ASAN)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "USE_ASAN is most effective with Debug build (current: ${CMAKE_BUILD_TYPE})")
    endif()
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
    message(STATUS "ASan enabled (Debug build)")
endif()

# ==============================================================================
# 依赖查找
# ==============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# ==============================================================================
# W2 (board-soc-split T-bs-5): dGPU board topology generation
# dgpu_board_v1_mvp.json.in (s2 W4) 已物理删除 (T-bs-5 commit 569917d)
# s2 单体退役 → DGpuBoard C++ shell + DGpuSoc SimModule 容器 (per ADR-SOC-07 D1/D7)
# ==============================================================================
set(PTX_EMU_ROOT "${CMAKE_SOURCE_DIR}/external/PTX-EMU" CACHE PATH "PTX-EMU source root")
# pcie-endpoint T-pe-5: SOC topology with PcieEndpointTLM (per ADR-SOC-07 D2)
configure_file(
    "${CMAKE_SOURCE_DIR}/configs/dgpu_soc_v1.json.in"
    "${CMAKE_BINARY_DIR}/configs/dgpu_soc_v1.json"
    @ONLY
)

# ==============================================================================
# 输出目录
# ==============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ==============================================================================
# 子目录
# ==============================================================================
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
    if(BUILD_RTL)
        add_subdirectory(test/rtl)
    endif()
endif()

if(BUILD_RTL)
    add_subdirectory(src/rtl)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
    add_subdirectory(samples)
endif()

# Python 拓扑验证集成
add_subdirectory(scripts)

# ==============================================================================
# 安装规则
# ==============================================================================
install(DIRECTORY include/ DESTINATION include/cpp-tlm)
install(DIRECTORY external/json/ DESTINATION include/cpp-tlm/external/json
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)
install(TARGETS cpptlm_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# T-W3-2 (T-ae-1): 23 ABI 暴露 SHARED 库 cpptlm_emulator install + find_package 支持
# (per design §5 + ADR-SOC-07 D5: UsrLinuxEmu 端通过 find_package(cpptlm) 或
# dlopen libcpptlm_emulator.so 消费 19 forward ABI + 4 callback typedefs)
install(TARGETS cpptlm_emulator
    EXPORT cpptlmTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include/cpp-tlm
)
install(EXPORT cpptlmTargets
    FILE cpptlmTargets.cmake
    NAMESPACE cpptlm::
    DESTINATION lib/cmake/cpptlm
)

# ==============================================================================
# HSK-8 Phase 2 (PTX-EMU owner ack 738b412c + 6 步指南 in #22 comment):
# Replace S1 cmake/PTXEmuCore.cmake shim with PTX-EMU native add_subdirectory.
# PTX-EMU src/CMakeLists.txt exports ptxemu_core STATIC target with PUBLIC
# ${PROJECT_SOURCE_DIR}/include/ptxemu (PUBLIC) + PRIVATE internal headers
# (PRIVATE = internal implementation hidden from CppTLM).
#
# Must be AFTER add_subdirectory(src) + install(TARGETS cpptlm_core) so the
# ptxemu_core target's PUBLIC include path propagates via target_link_libraries.
# ==============================================================================
if(CPPTLM_WITH_PTX_EMU)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/PTX-EMU/CMakeLists.txt")
        message(FATAL_ERROR
            "CPPTLM_WITH_PTX_EMU=ON 但 external/PTX-EMU 未初始化 (HSK-8 ack 738b412c 要求 PTX-EMU fcdad151+)。\n"
            "  修复: git submodule update --init external/PTX-EMU && cd external/PTX-EMU && git checkout fcdad151\n"
            "  或:   -DCPPTLM_WITH_PTX_EMU=OFF 关闭 GPU 集成功能")
    endif()
    # PTXEMU_BUILD_TESTING=OFF default — CppTLM ctest gate 与 PTX-EMU tests 隔离
    # (PROJECT_IS_TOP_LEVEL 在 add_subdirectory 调用时会自动变为 FALSE)
    set(PTXEMU_BUILD_TESTING OFF CACHE BOOL "PTX-EMU tests when not top-level" FORCE)
    add_subdirectory(external/PTX-EMU)
    if(NOT TARGET ptxemu_core)
        message(FATAL_ERROR "PTX-EMU 未导出 ptxemu_core target (submodule 过旧, 需 bump 到 >= HSK-8 Phase 2 commit fcdad151)")
    endif()
endif()

# ==============================================================================
# 配置摘要
# ==============================================================================
message(STATUS "")
message(STATUS "CppTLM ${PROJECT_VERSION} Configuration")
message(STATUS "  Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests:       ${BUILD_TESTS}")
message(STATUS "  Examples:    ${BUILD_EXAMPLES}")
message(STATUS "  Coverage:    ${ENABLE_COVERAGE}")
message(STATUS "  ccache:      ${CCACHE_PROGRAM}")
message(STATUS "")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
