# src/CMakeLists.txt — CppTLM 核心库 + 可执行文件

# ==============================================================================
# 核心静态库（显式列出源文件，不用 GLOB）
# ==============================================================================
# 注意：packet/packet.cc 和 noc/*.cc 为历史遗留文件，与当前头文件不兼容
#       暂不编译，待后续重构
set(CORE_SOURCES
    # core
    core/connection_resolver.cc
    core/module_factory.cc
    core/plugin_loader.cc
    # utils
    utils/dynamic_loader.cc
)

add_library(cpptlm_core STATIC ${CORE_SOURCES})

# 包含目录：
#   - include/       → 公共头文件（core/*.hh, utils/*.hh 等）
#   - include/core/  → 兼容旧式 #include "xxx.hh"（无 core/ 前缀）
#   - external/json  → nlohmann/json
target_include_directories(cpptlm_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/core
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

# 注意：以下可执行文件依赖 SystemC/TLM 库链接
#       仅在 USE_SYSTEMC=ON 时构建

if(USE_SYSTEMC)
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
endif()

# 注：输出目录已在根 CMakeLists.txt 中设置
