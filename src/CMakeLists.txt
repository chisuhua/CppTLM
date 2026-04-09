# src/CMakeLists.txt — CppTLM 核心库 + 可执行文件

# ==============================================================================
# 核心静态库（显式列出源文件，不用 GLOB）
# ==============================================================================
set(CORE_SOURCES
    # core
    core/connection_resolver.cc
    core/module_factory.cc
    core/plugin_loader.cc
    # packet
    packet/packet.cc
    # utils
    utils/dynamic_loader.cc
)

add_library(cpptlm_core STATIC ${CORE_SOURCES})

# 包含目录：
#   - include/       → 公共头文件（core/*.hh, utils/*.hh 等）
#   - src/core/      → 源文件同级头文件（module_factory.hh, sim_module.hh）
#   - external/json  → nlohmann/json
target_include_directories(cpptlm_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/json
)

# SystemC 头文件路径 & 编译定义（可选，仅头文件无需链接库）
if(USE_SYSTEMC)
    target_include_directories(cpptlm_core PUBLIC ${SystemC_INCLUDE_DIR})
    target_compile_definitions(cpptlm_core PUBLIC USE_SYSTEMC)
endif()

# 覆盖率选项
if(ENABLE_COVERAGE)
    target_compile_options(cpptlm_core PRIVATE --coverage)
    target_link_options(cpptlm_core PRIVATE --coverage)
endif()

# ==============================================================================
# 可执行文件
# ==============================================================================

# 主仿真程序
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    add_executable(cpptlm_sim main.cpp)
    target_link_libraries(cpptlm_sim PRIVATE cpptlm_core)
endif()

# CPU 模式主程序
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cpu_main.cpp")
    add_executable(cpptlm_cpu cpu_main.cpp)
    target_link_libraries(cpptlm_cpu PRIVATE cpptlm_core)
endif()

# 流量仿真主程序
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/traffic_main.cpp")
    add_executable(cpptlm_traffic traffic_main.cpp)
    target_link_libraries(cpptlm_traffic PRIVATE cpptlm_core)
endif()

# SystemC sc_main 入口
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sc_main.cpp")
    add_executable(cpptlm_sc_main sc_main.cpp)
    target_link_libraries(cpptlm_sc_main PRIVATE cpptlm_core)
endif()

# 布局示例
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main_layout.cpp")
    add_executable(cpptlm_layout main_layout.cpp)
    target_link_libraries(cpptlm_layout PRIVATE cpptlm_core)
endif()

# 层次示例
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main_hierarchy.cpp")
    add_executable(cpptlm_hierarchy main_hierarchy.cpp)
    target_link_libraries(cpptlm_hierarchy PRIVATE cpptlm_core)
endif()

# ==============================================================================
# 输出目录
# ==============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
