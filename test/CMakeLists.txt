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

# HSK-8 Phase 2 Step 5+: S1 的 12 个 test_ptx_emu_facade_* 和
# test_cuda_core_adapter_mvp_* 已物理删除 (依赖旧 PTX-EMU 内部头 c2038a93 接口,
# 与新 PTX-EMU fcdad151 IPtxEmuDevice 抽象不兼容). 后续 PR (Step 6+) 将基于
# IPtxEmuDevice 实际 API (init/sm_exe_once/attach_timing/get_warp_status 等 13 个方法)
# 重新设计测试, 重命名为 test_ptxemu_public_api_*.cc / test_cuda_core_adapter_*.cc.

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
if(CPPTLM_WITH_PTX_EMU)
    # HSK-8 Phase 2 Step 6: cpptlm_tests 同样需要 PTX-EMU shared libs
    # (libptxemu_device.so / libptxsim.so / libcudart.so 提供 PTX-EMU 内部符号).
    target_link_libraries(cpptlm_tests PRIVATE
        ptxemu_device ptxsim ptx_parser ptxir cudart)
    # HSK-8 Phase 2 P0-1 任务 1: 传递 CPPTLM_WITH_PTX_EMU 宏给测试源
    # (test_cuda_core_adapter_timing.cc 等 ON-only 测试用 #ifdef 保护).
    target_compile_definitions(cpptlm_tests PRIVATE CPPTLM_WITH_PTX_EMU=1)
endif()

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
