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
    # HSK-8 Phase 2 Step 4: 桥接残留簇删除 (PTX-EMU 已废弃 cpptlm_bridge.h)
    # - tlm/gpu/memory_bridge.cc       (deprecated by HSK-6, removed in v3.0 P4 — early delete OK)
    # - tlm/gpu/ptx_emu_driver_shim.cc (cpptlm_set_driver removed by HSK-6 — early delete OK)
    tlm/gpu/dgpu_bar.cc
    tlm/gpu/doorbell_mvp.cc        # s2 W3: SQ tail register + strong-order write
    tlm/gpu/completion_ring_mvp.cc # s2 W3: push + host_notify hook
    tlm/gpu/scoreboard_tlm.cc  # D1-Full P1: IScoreboard 实现
    tlm/gpu/pipeline_tlm.cc    # D1-Full P1: IPipelineLatencyProvider 实现
    tlm/gpu/tensor_core_tlm.cc # D1-Full P1: ITensorCoreTiming 实现
)

# ==============================================================================
# HSK-8 Phase 2 (PTX-EMU owner ack 738b412c):
# PTX-EMU 集成代码仅在 CPPTLM_WITH_PTX_EMU=ON 时编译 (默认 OFF)
# - ptx_emu_submodule_mvp.cc (facade, 仅 include ptxemu/device_api.h, C++17)
# - cuda_core_adapter_mvp.cc (adapter, 仅 include ptxemu/device_api.h, C++17)
#
# 旧 S1 (c2038a93 pin) 的 WU-2 stub.cc + WU-3/WU-4 OBJECT 库块 已废弃:
# - 新 PTX-EMU fcdad151+ 不再消费 g_cpptlm_bridge, stub.cc 不需要
# - 新 PTX-EMU 头 (ptxemu/device_api.h) 是 C++17 兼容, 不需要 OBJECT 库隔离
# ==============================================================================
if(CPPTLM_WITH_PTX_EMU)
    list(APPEND CORE_SOURCES
        tlm/gpu/ptx_emu_submodule_mvp.cc # HSK-8: PTX functional facade (仅 include ptxemu/device_api.h)
        tlm/gpu/cuda_core_adapter_mvp.cc  # HSK-8: CudaCoreAdapter timing (仅 include ptxemu/device_api.h)
    )
endif()

add_library(cpptlm_core STATIC ${CORE_SOURCES})
if(CPPTLM_WITH_PTX_EMU)
    # HSK-8 Phase 2 (per #22 comment step 2): link ptxemu_core (PUBLIC
    # propagates include path + lib deps to cpptlm_core consumers)
    target_link_libraries(cpptlm_core PUBLIC ptxemu_core)
endif()

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
    if(CPPTLM_WITH_PTX_EMU)
        # HSK-8 Phase 2 Step 6: cpptlm_sim 需要 PTX-EMU shared libs
        # (libptxemu_device.so / libptxsim.so / libptx_parser.so / libcudart.so
        #  提供 CudaDriver::instance() / SimpleMemoryAllocator::init() /
        #  PtxInterpreter::launchPtxInterpreter() / AtomicLockGuard 等符号)。
        target_link_libraries(cpptlm_sim PRIVATE
            ptxemu_device ptxsim ptx_parser ptxir cudart)
    endif()
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
