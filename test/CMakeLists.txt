# test/CMakeLists.txt
# 使用本地预编译的 Catch2（catch_amalgamated.cpp/.hpp）
# 避免 GitHub 网络下载问题
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB TEST_SOURCES "test_*.cc")
# test_cppHDL_smoke.cc 需要 CppHDL 头文件 (ch.hpp 等)，
# 仅当 BUILD_RTL=ON 时可编译。它已在 test/rtl/CMakeLists.txt 中
# 按 BUILD_RTL 条件注册（产生 cpptlm_rtl_tests），主测试 target
# 不链接 CppHDL include 路径，因此无条件包含会导致 build 失败。
if(NOT BUILD_RTL)
    list(REMOVE_ITEM TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/test_cppHDL_smoke.cc")
endif()

# S1 WU-3/T-s1-3: PtxEmuSubmoduleMVP facade tests 仅在 ON 路径编译
# (OFF 路径下 src/tlm/gpu/ptx_emu_submodule_mvp.cc 不存在)
if(NOT CPPTLM_WITH_PTX_EMU)
    list(REMOVE_ITEM TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/test_ptx_emu_facade_decode.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_ptx_emu_facade_arith.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_ptx_emu_facade_memory.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_ptx_emu_facade_branch.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_ptx_emu_facade_barrier.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_ptx_emu_facade_state.cc")
endif()

# S1 WU-4: CudaCoreAdapter tests gated by CPPTLM_WITH_PTX_EMU
if(NOT CPPTLM_WITH_PTX_EMU)
    list(REMOVE_ITEM TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/test_cuda_core_adapter_mvp_tick.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_cuda_core_adapter_mvp_scoreboard.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_cuda_core_adapter_mvp_pipeline.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_cuda_core_adapter_mvp_dispatch.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_cuda_core_adapter_mvp_warp_state.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_cuda_core_adapter_mvp_injection.cc")
endif()

# 将 Catch2 amalgamated 源直接加入编译（避免外部依赖）
add_executable(cpptlm_tests ${TEST_SOURCES} catch_amalgamated.cpp)

target_include_directories(cpptlm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(USE_SYSTEMC_STUB)
    target_compile_definitions(cpptlm_tests PRIVATE USE_SYSTEMC_STUB)
else()
    find_library(SYSTEMC_LIB systemc HINTS /usr/lib/x86_64-linux-gnu)
    target_link_libraries(cpptlm_tests PRIVATE ${SYSTEMC_LIB})
endif()

target_link_libraries(cpptlm_tests PRIVATE cpptlm_core)

include(CTest)
# 注意：不需要 include(Catch)，因为使用的是 catch_amalgamated.cpp

if(ENABLE_COVERAGE)
    target_compile_options(cpptlm_tests PRIVATE --coverage)
    target_link_options(cpptlm_tests PRIVATE --coverage)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 传入项目根目录路径，使测试可执行文件能找到 configs/
target_compile_definitions(cpptlm_tests PRIVATE CPPTLM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# P1 测试辅助: 启用 CPPTLM_TESTING 守卫的方法 (addInternalInstance / addOutputConfig / setName / addInstanceForTesting)
target_compile_definitions(cpptlm_tests PRIVATE CPPTLM_TESTING)
