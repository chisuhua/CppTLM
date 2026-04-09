# CppTLM - C++ TLM Hybrid Simulation Framework
# Version: 2.0.0
#
# Build system: CMake 3.16+ with optional Ninja + ccache
# SystemC: optional via -DUSE_SYSTEMC=ON (local headers in external/systemc/include)

cmake_minimum_required(VERSION 3.16)
project(CppTLM
    VERSION 2.0.0
    DESCRIPTION "C++ TLM Hybrid Simulation Framework"
    LANGUAGES CXX
)

# ==============================================================================
# ccache 支持（自动检测，加速编译）
# ==============================================================================
find_program(CCACHE_PROGRAM ccache)
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
option(USE_SYSTEMC    "Enable SystemC support (local headers)" OFF)
option(BUILD_TESTS    "Build unit tests"                     ON)
option(BUILD_EXAMPLES "Build example programs"               ON)
option(BUILD_DOCS     "Build documentation (Doxygen)"        OFF)
option(ENABLE_COVERAGE "Enable code coverage (gcov)"         OFF)

# ==============================================================================
# 编译选项
# ==============================================================================
add_compile_options(-Wall -Wextra -Wpedantic)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -DDEBUG_PRINT)
else()
    add_compile_options(-O2 -g)
endif()

# ==============================================================================
# 依赖查找
# ==============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# SystemC — 可选，使用项目内头文件
if(USE_SYSTEMC)
    find_package(SystemC QUIET)
    if(SystemC_FOUND)
        message(STATUS "SystemC found via find_package: ${SystemC_INCLUDE_DIR}")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/external/systemc/include")
        set(SystemC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/systemc/include")
        message(STATUS "SystemC enabled (local headers: ${SystemC_INCLUDE_DIR})")
    else()
        message(FATAL_ERROR "SystemC headers not found — place them in external/systemc/include/ or install system-wide")
    endif()
else()
    message(STATUS "SystemC disabled (use -DUSE_SYSTEMC=ON to enable)")
endif()

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
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

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
message(STATUS "  SystemC:     ${USE_SYSTEMC}")
message(STATUS "  Tests:       ${BUILD_TESTS}")
message(STATUS "  Examples:    ${BUILD_EXAMPLES}")
message(STATUS "  Coverage:    ${ENABLE_COVERAGE}")
message(STATUS "  ccache:      ${CCACHE_PROGRAM}")
message(STATUS "")
