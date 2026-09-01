# test/CMakeLists.txt
# 使用本地预编译的 Catch2（catch_amalgamated.cpp/.hpp）
# 避免 GitHub 网络下载问题
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB TEST_SOURCES "test_*.cc")
# s2 W3 T-s2-1: test_doorbell_strong_order_mvp.cc 由上述 GLOB 自动收录
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
# T-W3-2 (T-ae-3): link cpptlm_emulator SHARED 库让 19 forward ABI 符号在
# 测试二进制中可解析 (test_cpptlm_emulator_abi.cc + test_cpptlm_emulator_registry.cc).
target_link_libraries(cpptlm_tests PRIVATE cpptlm_emulator)
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

# sdma-engine T-sd-3: 注册 sdma_engine 系列测试到 ctest
# (per openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/tasks.md §T-sd-3)
# 通过 ctest 标签 + Catch2 -t 标志过滤到对应测试
add_test(NAME sdma_engine COMMAND cpptlm_tests "[sdma]" -r compact)

# board-soc-split T-bs-1: DGpuSoc 容器测试
# (per openspec/changes/2026-08-26-cpptlm-dgpu-board-soc-split/tasks.md §T-bs-1)
add_test(NAME test_dgpu_soc_from_config COMMAND cpptlm_tests "[dgpu][json]" -r compact)
add_test(NAME test_dgpu_board_shell_abi COMMAND cpptlm_tests "[dgpu][shell]" -r compact)

# T-W3-3 D15 (5425c45) 端到端回归: SOC 真正实例化后 wrapper 转发到 PcieEndpointTLM
add_test(NAME test_dgpu_board_d15_regression
         COMMAND cpptlm_tests "[dgpu][shell][d15][regression]" -r compact)

# T-W3-3 ABI: msix_*/lookup_register 转发测试 (D15 修复后 SOC 真正实例化 → 返 0)
add_test(NAME test_cpptlm_emulator_msix COMMAND cpptlm_tests "[abi][msix][t-w3-3]" -r compact)
add_test(NAME test_cpptlm_emulator_lookup_register
         COMMAND cpptlm_tests "[abi][lookup_register][t-w3-3]" -r compact)

# cpptlm_emulator ABI tests: create_by_id(0) 读 cwd 相对 configs/dgpu_board_v1.json,
# ctest 默认 cwd=build/ 加载失败 → SOC 未实例化 → wrapper 返 -38 而非 0/-22.
# WORKING_DIRECTORY 设回仓库根,让 config 路径解析正确。
set_tests_properties(test_cpptlm_emulator_msix
                 test_cpptlm_emulator_lookup_register
                 PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# s3 W5-6: Pm4Decoder + CommandProcessor + TmuDispatchProcessor 测试
# (per openspec/changes/2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/tasks.md §T-s3-1/2/3)
# 每个 add_test 用对应 Catch2 标签过滤;测试文件由 file(GLOB TEST_SOURCES ...) 自动收录
add_test(NAME test_pm4_decoder_mvp COMMAND cpptlm_tests "[pm4-decoder][mvp]" -r compact)
add_test(NAME test_command_processor_mvp COMMAND cpptlm_tests "[command-processor][mvp]" -r compact)
add_test(NAME test_tmu_dispatch_processor_tlm COMMAND cpptlm_tests "[tmu][mvp][glue]" -r compact)
add_test(NAME test_pm4_decoder_mvp_integration COMMAND cpptlm_tests "[pm4-decoder-integration]" -r compact)

# 2026-08-28 cpptlm-dgpu-pcie-slice-prerequisites (T-prereq-1..5 前置测试)
# (per openspec/changes/2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/tasks.md §T-prereq-6)
# NAMING CONVENTION: add_test NAME 严格匹配文件名前缀去 'test_' (per Metis naming review)
add_test(NAME pcie_slice_wire_format_snapshot COMMAND cpptlm_tests "[pcie][slice][wire-format]" -r compact)
add_test(NAME sdma_engine_translate_boundary COMMAND cpptlm_tests "[sdma][translate-boundary]" -r compact)
add_test(NAME pcie_endpoint_msix_state COMMAND cpptlm_tests "[pcie][endpoint][msix][state]" -r compact)
add_test(NAME pcie_endpoint_doorbell_queue COMMAND cpptlm_tests "[pcie][endpoint][doorbell][queue]" -r compact)
add_test(NAME sdma_engine_backdoor_isolation COMMAND cpptlm_tests "[sdma][backdoor][isolation]" -r compact)

# Edge case 防御性测试 (T-prereq-7 补充 2026-08-28)
# 涵盖 3 个 invariants: MsiXTable(0) 抛错, MsiXTable::init() 清零 PBA, Doorbell cycle_ns=0 fallback
add_test(NAME pcie_endpoint_msix_state_edge COMMAND cpptlm_tests "[pcie][endpoint][msix][state][edge]" -r compact)
add_test(NAME pcie_endpoint_doorbell_edge COMMAND cpptlm_tests "[pcie][endpoint][doorbell][edge]" -r compact)

# 2026-09-08 cpptlm-dgpu-pcie-link-layer-and-fc Phase 1 (T-P1-1..T-P1-7 测试注册)
# 7 个新测试: PcieDllpBundle / FcTokenBucket / LinkLayer DLLP / ErrorInjector / DownstreamRx / AckNak / Endpoint+LL
add_test(NAME pcie_dllp_bundle COMMAND cpptlm_tests "[pcie][dllp][bundle]" -r compact)
add_test(NAME pcie_link_layer_fc_token_bucket COMMAND cpptlm_tests "[pcie][fc][token]" -r compact)
add_test(NAME pcie_link_layer_dllp COMMAND cpptlm_tests "[pcie][ll][dllp]" -r compact)
add_test(NAME pcie_link_layer_error_injector COMMAND cpptlm_tests "[pcie][ll][error-inject]" -r compact)
add_test(NAME pcie_link_layer_downstream_rx COMMAND cpptlm_tests "[pcie][ll][downstream]" -r compact)
add_test(NAME pcie_link_layer_ack_nak COMMAND cpptlm_tests "[pcie][ll][ack-nak]" -r compact)
add_test(NAME pcie_endpoint_with_ll COMMAND cpptlm_tests "[pcie][endpoint][ll][compose]" -r compact)

# 2026-09-01 Oracle Phase 1 Critical Issue 修复回归测试
#   Issue #1: NAK 重传顺序(升序 wire order) / #2: 下行 rx buffer 泄漏 / #3: 上下行独立 FC
#   #4: seq 窗口卫生(outstanding<2048 + stale ACK guard)
add_test(NAME pcie_link_layer_nak_retransmit_order COMMAND cpptlm_tests "[pcie][ll][nak-order]" -r compact)
add_test(NAME pcie_link_layer_no_rx_buffer_leak COMMAND cpptlm_tests "[pcie][ll][no-rx-leak]" -r compact)
add_test(NAME pcie_link_layer_per_direction_fc COMMAND cpptlm_tests "[pcie][ll][per-dir-fc]" -r compact)
add_test(NAME pcie_link_layer_seq_window COMMAND cpptlm_tests "[pcie][ll][seq-window]" -r compact)

# T-W3-3: msix_* + lookup_register wrapper + ABI coverage + full ABI lifecycle
add_test(NAME dgpu_board_msix_wrappers COMMAND cpptlm_tests "[dgpu][shell][msix][wrapper]" -r compact)
add_test(NAME dgpu_board_lookup_register_wrapper COMMAND cpptlm_tests "[dgpu][shell][lookup_register][wrapper]" -r compact)
add_test(NAME dgpu_board_shell_full_abi COMMAND cpptlm_tests "[dgpu][shell][full_abi]" -r compact)

if(ENABLE_COVERAGE)
    target_compile_options(cpptlm_tests PRIVATE --coverage)
    target_link_options(cpptlm_tests PRIVATE --coverage)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 传入项目根目录路径，使测试可执行文件能找到 configs/
target_compile_definitions(cpptlm_tests PRIVATE CPPTLM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# P1 测试辅助: 启用 CPPTLM_TESTING 守卫的方法 (addInternalInstance / addOutputConfig / setName / addInstanceForTesting)
target_compile_definitions(cpptlm_tests PRIVATE CPPTLM_TESTING)
