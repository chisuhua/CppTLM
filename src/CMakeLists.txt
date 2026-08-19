# src/CMakeLists.txt — CppTLM 核心库 + 可执行文件

# ==============================================================================
# 核心静态库（显式列出源文件，不用 GLOB）
# ==============================================================================
# 注意：packet/packet.cc 和 noc/*.cc 为历史遗留文件，与当前头文件不兼容
#       暂不编译，待后续重构
set(CORE_SOURCES
    # core
    core/connection_resolver.cc
    core/coherence_domain.cc
    core/module_factory.cc
    core/module_factory_validate.cc
    core/plugin_loader.cc
    core/port_compatibility.cc
    core/param_parser.cc
    core/topology_node.cc
    core/topology_parser.cc
    # utils
    utils/dynamic_loader.cc
    # tlm (Phase 7: RouterTLM + NICTLM + LinkTLM + Phase 2 SimModule 容器 + Phase 3 ChStream helpers)
    tlm/router_tlm.cc
    tlm/nic_tlm.cc
    tlm/link_tlm.cc
    tlm/cache_tlm.cc
    tlm/crossbar_tlm.cc
    tlm/coherent_xbar_tlm.cc
    tlm/cluster/compute_cluster.cc
    tlm/cluster/tpc_cluster.cc
    tlm/cluster/gpc_cluster.cc
    tlm/cluster/gpu_cluster.cc
    tlm/cluster/cache_cluster.cc
    tlm/cluster/memory_cluster.cc
    tlm/cluster/gpu_noc_cluster.cc
    tlm/cluster/apu_soc.cc
    # gpu_soc Phase 8.A: 基础设施层 (Task 1-4 独立模块, 不依赖 F12)
    tlm/gpu/shared_memory_tlm.cc
    tlm/gpu/memory_cluster_tlm.cc
    tlm/gpu/gpu_mesh_noc_tlm.cc
    tlm/gpu/kernel_launch_tlm.cc
    tlm/gpu/wavefront_tlm.cc
    tlm/gpu/vector_regfile_tlm.cc
    tlm/gpu/minimal_warp_scheduler_tlm.cc
    tlm/gpu/gpu_compute_unit_tlm.cc
    tlm/gpu/gpu_soc_tlm.cc
    tlm/gpu/memory_bridge.cc   # D1-Full P0: CppTLMBridge 接口实现
    tlm/gpu/dgpu_bar.cc
tlm/gpu/scoreboard_tlm.cc  # D1-Full P1: IScoreboard 实现
tlm/gpu/pipeline_tlm.cc    # D1-Full P1: IPipelineLatencyProvider 实现
tlm/gpu/tensor_core_tlm.cc # D1-Full P1: ITensorCoreTiming 实现
tlm/gpu/ptx_emu_driver_shim.cc # D1-Full P1: cpptlm_set_driver ABI + DriverWrapper
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

# 覆盖率选项
if(ENABLE_COVERAGE)
    target_compile_options(cpptlm_core PRIVATE --coverage)
    target_link_options(cpptlm_core PRIVATE --coverage)
endif()

# ==============================================================================
# 可执行文件
# ==============================================================================

# 主仿真程序（不依赖 SystemC 库，仅需 cpptlm_core 静态库）
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

# 注：输出目录已在根 CMakeLists.txt 中设置
