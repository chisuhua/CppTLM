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
option(BUILD_LEGACY_MODULES "Build legacy CPUSim/CpuCluster modules (DEPRECATED in v2.1)" OFF)

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
install(TARGETS cpptlm_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

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
