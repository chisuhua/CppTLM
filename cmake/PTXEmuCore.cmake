# cmake/PTXEmuCore.cmake — PTX-EMU 核心静态库 shim (S1 / T-s1-2)
#
# 目的: 不经过 external/PTX-EMU/CMakeLists.txt (其 144-167 行反向
#       add_subdirectory(CppTLM)，直接 include 会循环) ，手工重建最小
#       ptxsim+ptx_ir 静态库。
#
# SOURCE OF TRUTH: external/PTX-EMU/src/CMakeLists.txt:64-68 (ptx_ir, 5 files)
#                  external/PTX-EMU/src/CMakeLists.txt:74-164 (ptxsim, 65 files)
#                  @ submodule pin 87820951
#
# 约定: 显式列表是唯一构建输入 (CppTLM "禁 GLOB") ; file(GLOB) 仅用于
#       configure 期 drift 校验，不参与构建。

if(NOT CPPTLM_WITH_PTX_EMU)
    return()
endif()

set(PTXEMU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/PTX-EMU")
set(PTXEMU_SRC  "${PTXEMU_ROOT}/src")

# ---- 门禁 1: submodule 存在性 (Amendment 2) ----
if(NOT EXISTS "${PTXEMU_SRC}/CMakeLists.txt")
    message(FATAL_ERROR
        "CPPTLM_WITH_PTX_EMU=ON 但 external/PTX-EMU 未初始化。\n"
        "  修复: git submodule update --init external/PTX-EMU\n"
        "  或:   -DCPPTLM_WITH_PTX_EMU=OFF 关闭 GPU 集成功能")
endif()

# ---- 显式源列表 (74 files: 65 ptxsim + 7 ptx_ir + 2 cudart 白名单) ----
# 全部使用绝对路径 (相对路径会导致 add_library 找不到)
set(PTXEMU_CORE_SOURCES
    # --- ptx_ir (7, src/CMakeLists.txt:64-68 列 5; GLOB drift 校验发现
    #             ptxir_reader.cpp (Oracle 二轮要求, decode_ptxir 实现)
    #             和 ptxir_writer.cpp 同目录同依赖, 一并加入) ---
    "${PTXEMU_SRC}/ptx_ir/ptx_types.cpp"
    "${PTXEMU_SRC}/ptx_ir/operand_context.cpp"
    "${PTXEMU_SRC}/ptx_ir/statement_context.cpp"
    "${PTXEMU_SRC}/ptx_ir/ptx_syntax_utils.cpp"
    "${PTXEMU_SRC}/ptx_ir/instruction_latency_table.cpp"
    "${PTXEMU_SRC}/ptx_ir/ptxir_reader.cpp"
    "${PTXEMU_SRC}/ptx_ir/ptxir_writer.cpp"
    # --- ptxsim top-level (4) ---
    "${PTXEMU_SRC}/ptxsim/instruction_factory.cpp"
    "${PTXEMU_SRC}/ptxsim/instruction_base.cpp"
    "${PTXEMU_SRC}/ptxsim/instruction_handlers.cpp"
    "${PTXEMU_SRC}/ptxsim/register_analyzer.cpp"
    # --- ptxsim/core (17) ---
    "${PTXEMU_SRC}/ptxsim/core/cta_context.cpp"
    "${PTXEMU_SRC}/ptxsim/core/thread_context.cpp"
    "${PTXEMU_SRC}/ptxsim/core/simt_pc_manager.cpp"
    "${PTXEMU_SRC}/ptxsim/core/register_access_layer.cpp"
    "${PTXEMU_SRC}/ptxsim/core/warp_context.cpp"
    "${PTXEMU_SRC}/ptxsim/core/warp_context_active_mask.cpp"
    "${PTXEMU_SRC}/ptxsim/core/warp_context_simt.cpp"
    "${PTXEMU_SRC}/ptxsim/core/warp_context_dispatch.cpp"
    "${PTXEMU_SRC}/ptxsim/core/execution_tracer.cpp"
    "${PTXEMU_SRC}/ptxsim/core/warp_scheduler.cpp"
    "${PTXEMU_SRC}/ptxsim/core/sm_context.cpp"
    "${PTXEMU_SRC}/ptxsim/core/sm_context_reconvergence.cpp"
    "${PTXEMU_SRC}/ptxsim/core/sm_context_cpptlm_inject.cpp"
    "${PTXEMU_SRC}/ptxsim/core/sm_block_dispatch.cpp"
    "${PTXEMU_SRC}/ptxsim/core/sm_warp_lifecycle.cpp"
    "${PTXEMU_SRC}/ptxsim/core/gpu_context.cpp"
    "${PTXEMU_SRC}/ptxsim/core/simt_stack.cpp"
    # --- register (2) ---
    "${PTXEMU_SRC}/register/condition_code_register.cpp"
    "${PTXEMU_SRC}/register/register_bank_manager.cpp"
    # --- memory legacy (4) ---
    "${PTXEMU_SRC}/memory/hardware_memory_manager.cpp"
    "${PTXEMU_SRC}/memory/simple_memory.cpp"
    "${PTXEMU_SRC}/memory/shared_memory_manager.cpp"
    "${PTXEMU_SRC}/memory/resource_manager.cpp"
    # --- ptxsim/memory (3) ---
    "${PTXEMU_SRC}/ptxsim/memory/tma_descriptor.cpp"
    "${PTXEMU_SRC}/ptxsim/memory/tmem.cpp"
    "${PTXEMU_SRC}/ptxsim/memory/tmem_allocator.cpp"
    # --- ptxsim/cluster + async + debug (4) ---
    "${PTXEMU_SRC}/ptxsim/cluster/cluster_context.cpp"
    "${PTXEMU_SRC}/ptxsim/async/tc_queue.cpp"
    "${PTXEMU_SRC}/ptxsim/debug/ptx_config.cpp"
    "${PTXEMU_SRC}/ptxsim/debug/warp_trace_formatter.cpp"
    # --- ptxsim/utils + utils (5) ---
    "${PTXEMU_SRC}/ptxsim/utils/qualifier_utils.cpp"
    "${PTXEMU_SRC}/ptxsim/utils/type_utils.cpp"
    "${PTXEMU_SRC}/ptxsim/utils/half_utils.cpp"
    "${PTXEMU_SRC}/utils/logger.cpp"
    "${PTXEMU_SRC}/utils/ptx_lane_verification.cpp"
    # --- ptxsim/instructions (17) ---
    "${PTXEMU_SRC}/ptxsim/instructions/arithmetic.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/arithmetic_muldiv.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/bitwise.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/math.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/memory.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/control.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/comparison.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/atomic.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/tcgen05.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/tcgen05_alloc.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/tcgen05_cp.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/tcgen05_helpers.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/tcgen05_fence.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/arithmetic_ext.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/data_transfer.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/call.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/barrier.cpp"
    # --- ptxsim/instructions/cvt (6) ---
    "${PTXEMU_SRC}/ptxsim/instructions/cvt/cvt_helpers.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/cvt/cvt_strategy.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/cvt/cvt_float_to_float.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/cvt/cvt_int_to_float.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/cvt/cvt_float_to_int.cpp"
    "${PTXEMU_SRC}/ptxsim/instructions/cvt/cvt_int_to_int.cpp"
    # --- ptxsim/barrier (3) ---
    "${PTXEMU_SRC}/ptxsim/barrier/warp_barrier.cpp"
    "${PTXEMU_SRC}/ptxsim/barrier/cta_barrier.cpp"
    "${PTXEMU_SRC}/ptxsim/barrier/barrier_module.cpp"
    # --- cudart 白名单 (2): gpu_context.cpp → CudaDriver 传递闭包 ---
    "${PTXEMU_SRC}/cudart/cuda_driver.cpp"
    "${PTXEMU_SRC}/cudart/simple_memory_allocator.cpp"
)

# ---- 门禁 2: 每文件存在性 (Amendment 3 加固, 防部分检出/抄写错误) ----
foreach(abs IN LISTS PTXEMU_CORE_SOURCES)
    if(NOT EXISTS "${abs}")
        message(FATAL_ERROR "PTX-EMU 源文件缺失: ${abs} "
            "(submodule 损坏或列表抄写错误, pin=87820951)")
    endif()
endforeach()

# ---- 门禁 3: GLOB drift 校验 (Amendment 1 修正版: GLOB 仅检测, 不构建) ----
set(_glob_dirs
    ptx_ir ptxsim ptxsim/core ptxsim/memory ptxsim/cluster ptxsim/async
    ptxsim/debug ptxsim/utils ptxsim/instructions ptxsim/instructions/cvt
    ptxsim/barrier register memory)
set(_globbed "")
foreach(d IN LISTS _glob_dirs)
    file(GLOB _tmp "${PTXEMU_SRC}/${d}/*.cpp")
    list(APPEND _globbed ${_tmp})
endforeach()
# utils/ 单独校验: ptxsim 库只消费其中 2 个, cubin_utils.cpp 为已知排除
file(GLOB _utils_glob "${PTXEMU_SRC}/utils/*.cpp")
list(REMOVE_ITEM _utils_glob "${PTXEMU_SRC}/utils/cubin_utils.cpp")
list(APPEND _globbed ${_utils_glob})
# 白名单 2 文件单独存在性即可, cudart/ 目录不参与 GLOB (其余文件均不编译)

# _expected 拷贝自 PTXEMU_CORE_SOURCES (已是绝对路径), 然后剔除 cudart
# 白名单 2 个 (它们不参与 GLOB 校验, 但进入构建)
set(_expected ${PTXEMU_CORE_SOURCES})
list(REMOVE_ITEM _expected
    "${PTXEMU_SRC}/cudart/cuda_driver.cpp"
    "${PTXEMU_SRC}/cudart/simple_memory_allocator.cpp")
# stub 不在文件系统内 (它是 CppTLM 自己写的)
list(REMOVE_ITEM _expected
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tlm/gpu/ptx_emu_bridge_stub.cc")

set(_drift_new ${_globbed})
list(REMOVE_ITEM _drift_new ${_expected})
set(_drift_missing ${_expected})
list(REMOVE_ITEM _drift_missing ${_globbed})
if(_drift_new OR _drift_missing)
    message(FATAL_ERROR
        "PTX-EMU submodule drift 检测失败 (pin=87820951):\n"
        "  新增未收录: ${_drift_new}\n"
        "  列表中已不存在: ${_drift_missing}\n"
        "  处理: bump PR 中同步更新 cmake/PTXEmuCore.cmake 显式列表")
endif()

# ---- stub 符号 (g_cpptlm_bridge + get_gpu_clock_from_context) ----
list(APPEND PTXEMU_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tlm/gpu/ptx_emu_bridge_stub.cc")

add_library(ptxemu_core STATIC ${PTXEMU_CORE_SOURCES})

# PTX-EMU 要求 C++20 (根 CMakeLists CMAKE_CXX_STANDARD 20); CppTLM 全局为 17,
# 仅对本 target 提升, 不影响其余代码。
set_target_properties(ptxemu_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)

# 第三方代码: 压制 root 全局 -Wall -Wextra -Wpedantic 造成的警告噪音
target_compile_options(ptxemu_core PRIVATE -w -fvisibility=hidden)

# include 顺序 (R-New1, 硬性): CppTLM include/ 必须最前 —
#   memory.cpp 的 "cudart/cpptlm_bridge.h" 必须解析到 CppTLM vendored
#   HSK-6 版本 (CPPTLMBRIDGE_VERSION=2 + abi_guards.h 17 断言随之生效),
#   而非 PTX-EMU 自有的旧版。
target_include_directories(ptxemu_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"      # 1. CppTLM vendored cudart/*.h
    "${PTXEMU_ROOT}/include"                   # 2. ptxsim/ ptx_ir/ cudart/ ptxir/ ptx_parser/
    "${PTXEMU_ROOT}/external/inipp"            # 3. inipp/inipp.h (logger.cpp)
    "${PTXEMU_SRC}"                            # 4. cudart/ptx_interpreter.h 等
    "${CMAKE_CURRENT_SOURCE_DIR}/external/json"  # 5. <nlohmann/json.hpp> (gpu_context.cpp)
)

find_package(Threads REQUIRED)
target_link_libraries(ptxemu_core PRIVATE Threads::Threads)

# ---- 门禁 4: 链接期 smoke (空库 → 链接错误, Amendment 3 加固) ----
add_executable(ptxemu_link_smoke test/ptxemu_link_smoke.cc)
target_link_libraries(ptxemu_link_smoke PRIVATE ptxemu_core)

# cpptlm_core 依赖 (静态库间仅传播 include/链接顺序, 对象不合并)
target_link_libraries(cpptlm_core PUBLIC ptxemu_core)

# ON 路径: 把 PTX-EMU include 路径作为 INTERFACE 传播给 cpptlm_core 消费者
# (主要是 cpptlm_tests — 让 S1 测试文件能 include PTX-EMU 公开头构造真实
# PTX 语句). 这是"编译防火墙 2.0": OFF 路径严格不暴露 PTX-EMU include,
# ON 路径放宽 (依赖 PTX-EMU 的代码可见). 生产代码仍由 facade 包装.
if(CPPTLM_WITH_PTX_EMU)
    # PRIVATE: 让 cpptlm_core 自身 (含 facade.cc 唯一 PTX-EMU 入口) 能找到头
    target_include_directories(cpptlm_core PRIVATE
        "${PTXEMU_ROOT}/include"
        "${PTXEMU_SRC}"
        "${PTXEMU_ROOT}/external/inipp"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/json")
    # INTERFACE: 传播给 cpptlm_core 消费者 (cpptlm_tests, cpptlm_sim 等)
    target_include_directories(cpptlm_core INTERFACE
        "${PTXEMU_ROOT}/include"
        "${PTXEMU_SRC}"
        "${PTXEMU_ROOT}/external/inipp"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/json")
    # OBJECT 库不继承 INTERFACE, 需显式设置 (S1 facade.cc)
    if(TARGET ptx_emu_submodule_mvp_obj)
        target_include_directories(ptx_emu_submodule_mvp_obj PRIVATE
            "${PTXEMU_ROOT}/include"
            "${PTXEMU_SRC}"
            "${PTXEMU_ROOT}/external/inipp"
            "${CMAKE_CURRENT_SOURCE_DIR}/external/json")
    endif()
    # S1 WU-4: cuda_core_adapter_mvp_obj OBJECT 库 (类似 WU-3 facade)
    if(TARGET cuda_core_adapter_mvp_obj)
        target_include_directories(cuda_core_adapter_mvp_obj PRIVATE
            "${PTXEMU_ROOT}/include"
            "${PTXEMU_SRC}"
            "${PTXEMU_ROOT}/external/inipp"
            "${CMAKE_CURRENT_SOURCE_DIR}/external/json")
    endif()
    # 注: facade.cc 的 C++20 标准已在 src/CMakeLists.txt 通过
    # set_source_files_properties 设置 (必须早于 add_library).
    # cpptlm_tests 也需要 C++20 (ON 路径下包含 PTX-EMU 头)
    if(TARGET cpptlm_tests)
        set_target_properties(cpptlm_tests PROPERTIES
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    endif()
endif()