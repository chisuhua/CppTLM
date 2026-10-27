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

# T-W3-3 ABI: msix_* 转发测试 (D15 修复后 SOC 真正实例化 → 返 0)
# 注: lookup_register 已于 T-ABI-2 阶段从 ABI 表面删除
# (per openspec/changes/cpptlm-abi-slimming)
add_test(NAME test_cpptlm_emulator_msix COMMAND cpptlm_tests "[abi][msix][t-w3-3]" -r compact)

# cpptlm_emulator ABI tests: create_by_id(0) 读 cwd 相对 configs/dgpu_board_v1.json,
# ctest 默认 cwd=build/ 加载失败 → SOC 未实例化 → wrapper 返 -38 而非 0/-22.
# WORKING_DIRECTORY 设回仓库根,让 config 路径解析正确。
set_tests_properties(test_cpptlm_emulator_msix
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

# T-W3-3: msix_* wrapper + ABI coverage + full ABI lifecycle
# 注: lookup_register wrapper 已于 T-ABI-2 阶段从 ABI 表面删除
# (per openspec/changes/cpptlm-abi-slimming)
add_test(NAME dgpu_board_msix_wrappers COMMAND cpptlm_tests "[dgpu][shell][msix][wrapper]" -r compact)
add_test(NAME dgpu_board_shell_full_abi COMMAND cpptlm_tests "[dgpu][shell][full_abi]" -r compact)

# 2026-09-29 cpptlm-dgpu-pcie-130b-encoding Phase 2 (T-P2-1/T-P2-2 测试注册)
# 2 个新测试: PcieEncodingLatencyModel 延迟单元 + Gen5 吞吐回归
add_test(NAME pcie_encoding_latency COMMAND cpptlm_tests "[pcie][encoding][latency]" -r compact)
add_test(NAME pcie_endpoint_throughput COMMAND cpptlm_tests "[pcie][ll][throughput]" -r compact)

# 2026-10-06 cpptlm-dgpu-pcie-phy-digital-ctrl Phase 3 (T-P3-1..T-P3-6 测试注册)
# 6 个新测试: LTSSM / Equalization / Hotplug / RateSwitch / BypassMux / RxNoDoubleCharge
add_test(NAME pcie_phy_digital_ltssm COMMAND cpptlm_tests "[pcie][phy][ltssm]" -r compact)
add_test(NAME pcie_phy_digital_equalization COMMAND cpptlm_tests "[pcie][phy][eq]" -r compact)
add_test(NAME pcie_phy_digital_hotplug COMMAND cpptlm_tests "[pcie][phy][hotplug]" -r compact)
add_test(NAME pcie_phy_digital_rate_switch COMMAND cpptlm_tests "[pcie][phy][rate-switch]" -r compact)
add_test(NAME pcie_bypass_mux COMMAND cpptlm_tests "[pcie][bypass][mux]" -r compact)
add_test(NAME pcie_link_layer_rx_no_double_charge COMMAND cpptlm_tests "[pcie][ll][rx-no-double]" -r compact)

# 2026-10-13 cpptlm-dgpu-pcie-sriov-vf-pool Phase 4 (T-P4-1..T-P4-7 测试注册)
# 8 个新测试: per-VF Config / per-VF MSI-X / ARI / VF Pool 路由 / FLR / Completion / EndpointIP
add_test(NAME pcie_sriov_per_vf_config COMMAND cpptlm_tests "[pcie][sriov][per-vf-config]" -r compact)
add_test(NAME pcie_sriov_per_vf_msix COMMAND cpptlm_tests "[pcie][sriov][per-vf-msix]" -r compact)
add_test(NAME pcie_sriov_ari_capability COMMAND cpptlm_tests "[pcie][sriov][ari]" -r compact)
add_test(NAME pcie_sriov_vf_pool_routing COMMAND cpptlm_tests "[pcie][sriov][vf-pool][routing]" -r compact)
add_test(NAME pcie_sriov_flr COMMAND cpptlm_tests "[pcie][sriov][vf-pool][flr]" -r compact)
add_test(NAME pcie_sriov_completion_tracking COMMAND cpptlm_tests "[pcie][sriov][completion][tracking]" -r compact)
add_test(NAME pcie_sriov_endpoint_ip_basic COMMAND cpptlm_tests "[pcie][sriov][endpoint-ip][basic]" -r compact)
add_test(NAME pcie_sriov_all COMMAND cpptlm_tests "[pcie][sriov]" -r compact)

# 2026-11-03 cpptlm-dgpu-axi-stream-adapter Phase 5 (T-P5-1..T-P5-6 测试注册)
# 5 个新测试: axi4_bundle / axi_adapter_basic / 64byte_burst / backpressure / ids
add_test(NAME axi4_bundle COMMAND cpptlm_tests "[axi][bundle]" -r compact)
add_test(NAME pcie_axi_adapter_basic COMMAND cpptlm_tests "[axi][adapter][basic]" -r compact)
add_test(NAME pcie_axi_adapter_64byte_burst COMMAND cpptlm_tests "[axi][pcie][adapter][burst]" -r compact)
add_test(NAME pcie_axi_adapter_backpressure COMMAND cpptlm_tests "[axi][adapter][backpressure]" -r compact)
add_test(NAME pcie_axi_adapter_ids COMMAND cpptlm_tests "[axi][adapter][ids]" -r compact)

# 2026-12-22 cpptlm-dgpu-axi4-mapper Phase 6 (T-P6-1..T-P6-4 测试注册)
# 4 个新测试: basic / outstanding / out_of_order / integration_with_pcie
add_test(NAME axi4_mapper_basic COMMAND cpptlm_tests "[axi][mapper][basic]" -r compact)
add_test(NAME axi4_mapper_outstanding COMMAND cpptlm_tests "[axi][mapper][outstanding]" -r compact)
add_test(NAME axi4_mapper_out_of_order COMMAND cpptlm_tests "[axi][mapper][ooo]" -r compact)
add_test(NAME axi4_mapper_integration_with_pcie COMMAND cpptlm_tests "[axi][mapper][integration]" -r compact)

# 2027-01-19 cpptlm-dgpu-pcie-host-bypass-and-rc Phase 7 (T-P7-1..T-P7-3 测试注册)
# 3 个新测试: host_bypass_basic / host_bypass_software_bringup / root_complex_enumeration
add_test(NAME test_host_bypass_basic COMMAND cpptlm_tests "[pcie][host-bypass][basic]" -r compact)
add_test(NAME test_host_bypass_software_bringup COMMAND cpptlm_tests "[pcie][host-bypass][software]" -r compact)
add_test(NAME test_pcie_root_complex_enumeration COMMAND cpptlm_tests "[pcie][root-complex][enum]" -r compact)

# 2027-02-09 cpptlm-dgpu-pcie-ip-integration Phase 8 (T-P8-3 全链路 E2E 测试)
# 解决 Phase 7 M1 (EP 真实消费 AXI 请求，HostBypass/RC ↔ PcieEndpointIP 真实闭环)
add_test(NAME test_pcie_endpoint_ip_full_e2e_config COMMAND cpptlm_tests "[pcie][axi][e2e][phase8][config]" -r compact)
add_test(NAME test_pcie_endpoint_ip_full_e2e_bar COMMAND cpptlm_tests "[pcie][axi][e2e][phase8][bar]" -r compact)
add_test(NAME test_pcie_endpoint_ip_full_e2e_rc COMMAND cpptlm_tests "[pcie][axi][e2e][phase8][rc]" -r compact)

# 2026-09-10 cpptlm-stage-1-3-sdma (Stage 1.3a: SDMA Ring Buffer + BAR1 Doorbell)
# (per openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/tasks.md §1)
# TDD RED 状态: 1.3a 实施前编译失败 (feature missing), 实施后变绿
add_test(NAME test_sdma_ring_buffer_capacity COMMAND cpptlm_tests "[sdma][ring][1.3a][capacity]" -r compact)
add_test(NAME test_sdma_ring_buffer_atomic COMMAND cpptlm_tests "[sdma][ring][1.3a][atomic]" -r compact)
add_test(NAME test_sdma_ring_buffer_doorbell COMMAND cpptlm_tests "[sdma][ring][1.3a][doorbell]" -r compact)
add_test(NAME test_sdma_packet_sg_chain COMMAND cpptlm_tests "[sdma][packet][sg][1.3a]" -r compact)
add_test(NAME test_pcie_endpoint_ip_bar1_doorbell COMMAND cpptlm_tests "[pcie][endpoint][bar][doorbell][1.3a]" -r compact)

# 2026-09-10 cpptlm-stage-1-3-sdma (Stage 1.3b: D2D NoC payload forwarding + host_out zero)
# (per openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/tasks.md §2)
# D2D NoC ≥ 100 GB/s payload forward + memctl bypass
add_test(NAME test_d2d_noc_path COMMAND cpptlm_tests "[sdma][d2d][1.3b]" -r compact)

# 2026-09-10 cpptlm-stage-1-3-sdma (Stage 1.3d: SDMA Fence + MSI-X vector 0 + CompletionRing)
# (per openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/tasks.md §4)
# Fence descriptor (opcode=0x04) → completion_ring → MSI-X vector 0 → intr_cb 200ms
add_test(NAME test_sdma_completion COMMAND cpptlm_tests "[sdma][fence][1.3d]" -r compact)

# 2026-09-10 cpptlm-stage-1-3-sdma (Stage 1.3a 接入: SdmaEngineTLM ring 路径 + 互斥规则 + doorbell 触发)
# (per openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/tasks.md §1, tasks.md §1.3a 改造)
# SdmaEngineTLM 内部 ring consume + desc_in 互斥 + BAR1 doorbell 触发 ring
add_test(NAME test_sdma_ring_engine_integration COMMAND cpptlm_tests "[sdma][ring_engine][1.3a_integration]" -r compact)

# 2026-09-10 cpptlm-stage-1-3-sdma (Stage 1.3c: PcieEndpointIP GART/IOMMU 模式开关)
# (per openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/tasks.md §3, pcie_endpoint_ip.cc GART/IOMMU 4 级翻译链)
# C++ 端暴露 mode 开关 + cb 转发; 真实 4 级 walk 在 UE 进程
add_test(NAME test_pcie_iommu_translation COMMAND cpptlm_tests "[pcie][iommu][1.3c]" -r compact)

# 2026-09-10 cpptlm-stage-1-3-sdma (Stage 1.3c: PM4 DISPATCH dma_req)
# (per openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/tasks.md §3, command_processor_mvp.cc DISPATCH dma_req PM4 0x4600-0x4900)
# Pm4Decoder LUT 0x46-0x49 → 4 新 SDMA types
add_test(NAME test_pm4_dispatch_dma_req COMMAND cpptlm_tests "[pm4][decoder][1.3c]" -r compact)

# 2026-09-10 cpptlm-stage-1-3-sdma (Stage 1.3d: 真实 MSI-X 链路)
# (per openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/tasks.md §4, cpptlm_emulator.cc MSI-X vector 路由)
# D 项 API 已在 test_sdma_completion + test_cpptlm_dma_translate_cb_real 验证
# 完整真实链路 (SdmaEngineTLM.fence → board → intr_cb) 留 UE 集成测试覆盖

# 2026-09-10 cpptlm-stage-1-3-sdma (Stage 1.3c: dma_translate_cb 真实化, 修复 #2)
# (per openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/tasks.md §3)
# 修复 #2: 移除 cpptlm_emulator.cc:449 (void)cb stub, 真实调用 cb 函数指针
add_test(NAME test_cpptlm_dma_translate_cb_real COMMAND cpptlm_tests "[abi][dma_translate][1.3c]" -r compact)

# 2026-09-13 ue-sdma-p0-unblock (Task 2: DGpuBoard BAR1 doorbell routing)
# (per docs/superpowers/specs/2026-09-13-ue-sdma-p0-unblock-design.md §3.2 目标 1)
# 验证: doorbell write → mmio_regs_ stored (修复 #5) + SOC pcie_ep doorbell_count_++
add_test(NAME test_dgpu_board_doorbell_routing_p0 COMMAND cpptlm_tests "[dgpu-board][doorbell][1.3a][integration]" -r compact)
# Task 4 (2026-09-13 ue-sdma-p0-unblock): DGpuBoard BAR1 doorbell → SdmaEngineTLM ring consume
add_test(NAME test_sdma_ring_engine_integration_p0 COMMAND cpptlm_tests "[sdma][ring][integration][1.3a]" -r compact)
# Task 7 (P0 unblock): SDMA Fence → DGpuBoard → trigger_irq_async(vector=0) 真实链路
add_test(NAME test_sdma_fence_irq_integration_p0 COMMAND cpptlm_tests "[sdma][fence][1.3a][integration]" -r compact)

# 2026-10-21 cpptlm-cep-umbrella (Stage 0) — CEP 契约层测试注册
# (per openspec/changes/cpptlm-cep-umbrella/tasks.md Phase 1/3/6/7)
# 5 个 test 文件由 file(GLOB TEST_SOURCES ...) 自动收录,此处只注册 ctest 标签
add_test(NAME cep_canonical_invariant COMMAND cpptlm_tests "[cep][canonical-invariant]" -r compact)
add_test(NAME cep_schema COMMAND cpptlm_tests "[cep][schema]" -r compact)
add_test(NAME cep_simulation_mode COMMAND cpptlm_tests "[cep][simulation-mode]" -r compact)
add_test(NAME cep_tools_contract COMMAND cpptlm_tests "[cep][tools]" -r compact)
add_test(NAME cep_dummy_signals COMMAND cpptlm_tests "[cep][dummy-signals]" -r compact)
add_test(NAME cep_module_factory_seam COMMAND cpptlm_tests "[cep][module-factory-seam]" -r compact)
add_test(NAME cep_contracts_crosscheck COMMAND cpptlm_tests "[cep][contract]" -r compact)
# 传输层 CATEGORY_* → VC 桶映射契约(Phase 0,per change `cpptlm-cep-stub-cache-core`
# 的 delta capability `cep-vc-mapping-contract`;M-4 行为侧决策的可执行回归)
# ⚠️ ctest NAME **不含** `cep_vc_mapping` 之外的额外前缀 —— `[cep]` 已被 `cep_all` 覆盖,
#    本用例是**独立**标签,便于单独核验 M-4 契约。
add_test(NAME cep_vc_mapping_contract
         COMMAND cpptlm_tests "[cep][vc-mapping-contract]" -r compact)
# Stub Cache Core 契约(Phase 1,per change `cpptlm-cep-stub-cache-core` 的
# delta capability `cep-stub-cache-core`;ADR-CEP-05 的 6 接口 / 5 态 / INV-SC1~6)
# WORKING_DIRECTORY 设仓根 —— 用例会读仓内文件(头文件 / spec / design)做静态核对
add_test(NAME cep_stub_cache_core
         COMMAND cpptlm_tests "[cep][stub-cache-core]" -r compact)
set_tests_properties(cep_stub_cache_core PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# delta capability 的「Scenario ↔ 锚点」对账(A-P1-7)
# —— 断言 Scenario 数 == 锚点数,且锚点点名的文件 / ctest 用例真实存在
add_test(NAME cep_scenario_anchor_reconciliation
         COMMAND cpptlm_tests "[cep][scenario-reconciliation]" -r compact)
set_tests_properties(cep_scenario_anchor_reconciliation
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cep_all COMMAND cpptlm_tests "[cep]" -r compact)
# CEP 契约同步 lint(ADR 存在性 / invariant 计数单一真源 / P 编号口径 / 4 件套)
add_test(NAME cep_docs_sync_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_docs_sync_check.sh --strict)

# CohIf 载体契约的**反向验证**(per change `cpptlm-cep-cohif-carrier` task 1.5~1.7)
# —— 4 类毒化副本(第 6 个 flow_id / 混入网络层字段 / 字段宽度 +1 / 加虚函数)
#    分别必须让「静态扫描」报红 / 让「编译期锁」编译失败。
# ⚠️ 与 `cep_decl_reverse_verify.sh` 不同:本脚本**只写自己的临时目录**、
#    **不改**仓内任何文件 ⇒ 可安全并行进 ctest。
add_test(NAME cep_cohif_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_cohif_reverse_verify.sh)

# ProtocolEngine 端口面契约的**反向验证**(per change `cpptlm-cep-cohif-carrier`
# task 2.6 / 2.7)—— 2 类毒化副本(10 流覆盖矩阵缺 1 流 / 某流语义退化为 no-op)
#    分别必须让「覆盖完整性」「逐流行为」两条编译期锁报红。
add_test(NAME cep_protocol_engine_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_protocol_engine_reverse_verify.sh)

# CEP ↔ ArchForge 接口对齐清单的漂移检测(跨仓引用完整性)
# —— ArchForge 仓不在本环境时该检查**优雅跳过**(不视为失败)
# 状态一致性闸的**回归 fixture 集**(10+ 场景,变异 → 期望 rc)
add_test(NAME cep_gate_selftest
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_gate_selftest.sh)
# `test.sh` 参数解析回归锁(2026-10-24:`--build` 恒报「与 --test-only 互斥」→ PTX-EMU CI 挂)
add_test(NAME test_sh_arg_parsing
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/test_sh_arg_parsing.sh)
# PoC Spike 的 delta spec ↔ 实现对账(每条 Scenario 都有可执行断言;per tasks.md T5.9)
# ⚠️ ctest NAME 刻意**不含** `cep_poc_spike` 前缀 —— 否则 `ctest -R cep_poc_spike`
#    会命中 4 个,违反 spec 明文「恰好命中 3 个用例」(critic B3)。Catch2 tag 仍含
#    `[cep][poc-spike][verification]`(标签不参与 `ctest -R` 匹配)。
add_test(NAME cep_spike_delta_verification
         COMMAND cpptlm_tests "[cep][poc-spike][verification]" -r compact)
set_tests_properties(cep_spike_delta_verification PROPERTIES
                     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                     DEPENDS "cep_poc_spike_s1;cep_poc_spike_s2;cep_poc_spike_s3")
add_test(NAME cep_archforge_alignment_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_archforge_alignment_check.sh)
set_tests_properties(cep_archforge_alignment_check
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ArchForge 协议实例声明 ↔ 本仓 schema/fabric 的**对齐检查**(放行判据 C-4 的机器化)
# —— ArchForge 仓不在本环境时该检查**优雅跳过**(不视为失败;但输出标 [SKIP],
#    与「跑过」可区分 —— 判据 C-4 的机器化不应被一个跳过态当作已验证)
# ⚠️ SKIP_REGULAR_EXPRESSION:让 ctest 把「跳过」显示为 **Skipped** 而不是 **Passed** ——
#    否则 CI 上一个恒跳过的闸门看起来像「验过了」(2026-10-22 代码复审 S2)。
#    ⚠️ **不可**设 SKIP_RETURN_CODE 0 —— 那等于「退出码 0 即跳过」,会把**正常通过**也标成 Skipped。
#    脚本的 SKIP 路径打印 `[SKIP]` 并 exit 0;RUN 路径打印 `[RUN]` 并按结果 exit 0/1。
add_test(NAME cep_decl_alignment_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_decl_alignment_check.sh)
set_tests_properties(cep_decl_alignment_check
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                                SKIP_REGULAR_EXPRESSION "\\[SKIP\\]")

# M1 产物冻结校验(per obj-cep-platform §9.2 M1「产物冻结先于解读」)
# —— 独立于 lint,便于单独定位"冻结产物被改动"
add_test(NAME cep_freeze_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_freeze_check.sh)
set_tests_properties(cep_freeze_check PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# 对外文档「节号引用」校验(2026-10-22)
# —— 补 `docs_sync_check.sh`(管路径) 与 `cep_archforge_alignment_check.sh`(管锚点行号) 的缺口:
#    这两者都**不校验节号**。起因 = `ADR-CEP-05 §10` 笔误(判据 2 实际在 §7)扩散 24 处、
#    跨 3 个 commit 无人发现,且出现在**发给 ArchForge 的对外文档**里。
# —— 注册表外置:scripts/test/cep_doc_ref_registry.tsv（新增节号引用须在此登记）
add_test(NAME cep_doc_ref_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_doc_ref_check.sh --strict)
set_tests_properties(cep_doc_ref_check PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ⚠️ WORKING_DIRECTORY:ctest 默认在 build/test 下运行 —— git pathspec 与相对
#    路径会解析到 build/test/... → 冻结面守门空转(2026-10-21 代码复审 B1)。
#    与 test_cpptlm_emulator_msix(:73-75)的既有做法一致。
set_tests_properties(cep_canonical_invariant cep_schema cep_simulation_mode
                     cep_tools_contract cep_dummy_signals cep_module_factory_seam
                     cep_contracts_crosscheck cep_all
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# tools 语法检查 —— 独立于 Catch2,CI 可单独跑
add_test(NAME cep_tools_syntax
         COMMAND python3 -m py_compile
                 ${CMAKE_SOURCE_DIR}/tools/render_report.py
                 ${CMAKE_SOURCE_DIR}/tools/run_sweep.py
                 ${CMAKE_SOURCE_DIR}/tools/ab_compare.py)
set_tests_properties(cep_tools_syntax PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# tools --help 冒烟(每个 tool 独立一条,便于定位)
foreach(_cep_tool render_report run_sweep ab_compare)
    add_test(NAME cep_tool_help_${_cep_tool}
             COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/${_cep_tool}.py --help)
    set_tests_properties(cep_tool_help_${_cep_tool}
                         PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()

if(ENABLE_COVERAGE)
    target_compile_options(cpptlm_tests PRIVATE --coverage)
    target_link_options(cpptlm_tests PRIVATE --coverage)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 传入项目根目录路径，使测试可执行文件能找到 configs/
target_compile_definitions(cpptlm_tests PRIVATE CPPTLM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# PoC Spike 二进制目录(供 delta spec ↔ 实现的对账测试调用 spike,per tasks.md T5.9)
target_compile_definitions(cpptlm_tests PRIVATE CEP_SPIKE_BIN_DIR="${CMAKE_BINARY_DIR}/bin/examples")

# P1 测试辅助: 启用 CPPTLM_TESTING 守卫的方法 (addInternalInstance / addOutputConfig / setName / addInstanceForTesting)
target_compile_definitions(cpptlm_tests PRIVATE CPPTLM_TESTING)

# C.5 (Change C): try_compile negative-compile probe — 验证 legacy public API 已删
# probe 引用 set_display_routing_enabled 等 (C.3.1 已删) → 编译失败 = WILL_FAIL PASS
add_executable(try_compile_legacy_flags_removed EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/test/try_compile_legacy_flags_removed.cc)
target_include_directories(try_compile_legacy_flags_removed PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME try_compile_legacy_flags_removed
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target try_compile_legacy_flags_removed)
set_tests_properties(try_compile_legacy_flags_removed PROPERTIES WILL_FAIL TRUE)

# C.6 (Change C): grep gate — 验证 legacy PUBLIC API 已删 (setter/accessor/attach_profile/PciePath)
# 内部 routing_enabled_*_ 成员保留 (anti-hijack barrier, per C.3.1 修订), 不在此 gate 范围.
add_test(NAME grep_no_legacy_public_api_in_board_header
         COMMAND sh -c "! grep -E 'set_display_routing_enabled|set_memory_routing_enabled|set_storage_routing_enabled|set_gmmu_routing_enabled|display_routing_enabled\\\\(\\\\)|memory_routing_enabled\\\\(\\\\)|storage_routing_enabled\\\\(\\\\)|gmmu_routing_enabled\\\\(\\\\)|attach_profile|pcie_path\\\\(\\\\)|PciePath' ${CMAKE_SOURCE_DIR}/include/tlm/gpu/dgpu_board_shell.hh")
