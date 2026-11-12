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
# 队列 #8 `cpptlm-cep-chi-rnf-subset`:CHI RN-F subset 的实例面 + 真插件引擎判据(CR-01 ~ CR-14)
add_test(NAME cep_chi_rnf_subset COMMAND cpptlm_tests "[cep][chi-rnf]" -r compact)

# 队列 #9 `cpptlm-cep-mesif-adapter` 的 R-1 裁决落地:权威迁移 + 第 6 种 SharerTracker 实现
add_test(NAME cep_authority_migration COMMAND cpptlm_tests "[cep][authority-migration]" -r compact)

# 队列 #9 `cpptlm-cep-mesif-adapter` 的**规约层**判据(MESIF 状态机规约 + 5→4 perm 投影表)
add_test(NAME cep_mesif_state_spec COMMAND cpptlm_tests "[cep][mesif-state-spec]" -r compact)

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
# 双向适配器**多端口入向分桶**(per change `cpptlm-cep-bidirectional-multiport-inbound`
# 的 AC-2/AC-3/AC-7/AC-8;台账 Entry #310 R-5/R-6 + Entry #317)
# ⚠️ 标签**不含** `[cep]` —— 本测试判的是**框架层 adapter** 行为,与 CEP 契约无关
#    ⇒ 必须独立注册,否则 `cep_all` 覆盖不到(即「只测不跑」)。
add_test(NAME framework_bidirectional_multiport_inbound
         COMMAND cpptlm_tests "[adapter][bidirectional][multiport]" -r compact)
# CEP 契约同步 lint(ADR 存在性 / invariant 计数单一真源 / P 编号口径 / 4 件套)
add_test(NAME cep_docs_sync_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_docs_sync_check.sh --strict)

# ADR **结构闸**(per oracle 2026-11-09 裁决 C-1 / 台账 Entry #307 §③ T1)
# —— 把「ADR 正文缺顶层 Verification 章」从暗变可见,并**锁住**例外登记:
#    ① 分区完整性(新 ADR 必须显式归类)② 未登记即红 ③ 豁免锚点存活(反后门)
#    ④ 登记表健康。附 **3 个决定性变异**(影子树 ⇒ 不动真实仓)证明闸有牙。
add_test(NAME cep_adr_structural_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_adr_structural_check.sh)
set_tests_properties(cep_adr_structural_check
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# CEP **治理判据闸**(per change `cpptlm-cep-invariant-gate-teeth` / 台账 Entry #363)
# —— 把 `ADR-CEP-00` SU 2026-11-11 §⑤ 自认的两项「有登记、无牙齿」残留落为机器判据:
#    [R-d] 22 份 ADR 的 §2 决策块计数**逐份** == 快照(D-1 四步 + D-3.2 块 ID 全匹配)
#          + 显式红①(非 ### 级)②(编号非法)+ 围栏危险标记
#    [R-c] 双向对账:正向(磁盘集合 / 无同号拆出件)+ 反向(新 SU 的 `### ⑥` 重述块)
# 附 **决定性变异**(影子树 ⇒ 不动真实仓)证明闸有牙;每条变异按**固定签名**变红。
# ⚠️ 影子树机制已抽取为共享 helper `scripts/test/cep_test_shadow.sh`
#    ⇒ 与 `cep_adr_structural_check.sh` 共用**唯一一份**实现(per Entry #361 ③ D-8)。
add_test(NAME cep_decision_block_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_decision_block_check.sh)
set_tests_properties(cep_decision_block_check
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

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

# ProtocolBridge 特化桥契约的**反向验证**(per change `cpptlm-cep-cohif-carrier`
# task 3.8 / 3.9)—— 2 类毒化副本(perm → flit_category / address ↔ data 对调)
#    分别必须让「字段映射探针」「内容层 round-trip」判据报红;
#    且毒化是**运行期**语义 ⇒ 毒化副本必须**编得过**(否则判据「因错误的原因红」)。
add_test(NAME cep_protocol_bridge_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_protocol_bridge_reverse_verify.sh)

# `REGISTER_CHSTREAM_BIDIRECTIONAL` 宏的**编译期锁**反向验证(per change
# `cpptlm-cep-cohif-carrier` task 4.3 / 4.4)—— N=1 必须让宏内的
# `static_assert(n > 1)` 报红(判据 CC-04 ③);N=2 必须编译成功(正控)。
# MESIF 规约判据的**反向验证** —— 8 类毒化副本(依据清空 / kind↔pending 错配 /
#    无锚点引用 / 共享态写突变 / 本地事件秩下降 / 转换表挖洞 / 偏离不真)
#    分别必须被抓到(编译期锁或运行期谓词报红)
add_test(NAME cep_mesif_state_spec_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_mesif_state_spec_reverse_verify.sh)
# 队列 #9 Phase 4:实现层判据(MA-01 ~ MA-05 / MA-08 / HD-01 ~ HD-05)的反向验证。
# ⚠️ 与上者的口径差异:每条 fixture 声明**期望失败模式**(compile-lock / runtime-predicate)
#    且必须命中**指定的判据 id 或 static_assert 文案** ⇒ 「因正确原因红」有机器判定。
add_test(NAME cep_mesif_adapter_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_mesif_adapter_reverse_verify.sh)

# 队列 #9 后续:跨仓追认请求清单的**生成器自检**(只比对不写)——
# 清单由机器可读载体生成 ⇒ 手改清单(或载体改了忘重生成)⇒ rc≠0。
add_test(NAME cep_mesif_ratification_request_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/generate_mesif_ratification_request.sh --check)

# 队列 #10 `cpptlm-cep-cxl-nonsymmetric-state-spec`(第 0 个 change = 规约层)的判据:
#   ① 载体契约(4 态 / 4→4 投影 / 非对称结构断言 / 三态 68 格矩阵 / 反向 fixture);
#   ② CSS-06 —— 载体 ↔ 核对表 ↔ 锚点摘要 三方逐格一致 + 「不整段复制原文」。
add_test(NAME cep_cxl_state_spec COMMAND cpptlm_tests "[cep][cxl][state-spec]" -r compact)

# 队列 #11 `cpptlm-cep-tilelink-state-spec`(第 0 个 change = 规约层)的判据:
#   ① 载体契约(3 态 / 4 事件 / 12 格(5/5/2/0 分桶)/ 3→3 投影 / TileLink 形态非对称断言
#      / 逐格 basis + 偏离清单)+ 8 条反向 fixture;
#   ② TSS-07 —— 载体逐格 `ref` ↔ 锚点摘要 §2.3bis 的 `TLC-01`~`TLC-12` **逐格**一一对应
#      (ID 文法容忍 Markdown 粗体 + 接受 `R-T<n>` 前缀 ⇒ **不得**照抄 CXL 版的「全数字」文法)。
add_test(NAME cep_tilelink_state_spec COMMAND cpptlm_tests "[cep][tilelink][state-spec]" -r compact)

# 队列 #12 `cpptlm-cep-cxl-dcoh-symmetric-state-spec`(第 0 个 change = 规约层)的判据:
#   ① 3 套状态空间(基数 + 跨域同名是预期的 + 强类型互斥)
#   ② 通道词表(CXL.mem 6 + CXL.cache 每方向 3 + 方向表)
#   ③ ⭐ 对称判别式(`device_initiated_snoop_set == {BISnp}`)
#   ④ `perm` 只从缓存代理域投影(4→4)
#   ⑤ 窄义「对称」5 谓词合取(正控 + 逐谓词反控)
add_test(NAME cep_cxl_dcoh_state_spec COMMAND cpptlm_tests "[cep][cxl-dcoh][state-spec]" -r compact)

# 队列 #12 `cpptlm-cep-cxl-dcoh-symmetric`(第 1 个 change = 实现层)的 **adapter 判据**:
#   ① CDX-01 零自有表副本(零成员字面量 + 零表名)· 含扫描器**正控 + 反向 fixture**
#   ② CDX-02 逐项委托规约载体
#   ③ CDX-03 不暴露 `transition_*`(载体无转移表 ⇒ 暴露即等于自造转移表)
add_test(NAME cep_cxl_dcoh_adapter COMMAND cpptlm_tests "[cep][cxl-dcoh][adapter]" -r compact)

# 队列 #12 `cpptlm-cep-cxl-dcoh-symmetric`(第 1 个 change = 实现层)的 **tracker 行为判据**:
#   ① CDX-04 caps/behavior 6 字段**逐字段**数据级断言(非仅「常量存在」)
#   ② 每个行为断言**配正控**(对照实现 `FullMapDirectory` / `SparseSnoopFilter` / `TreeManager`)
#      ⇒ 「断言恒绿」= 判据可能没生效 ⇒ 铁律 ①
#   ③ CDX-05 `LOCAL_HINT` 双重语义登记可寻址(main spec)
add_test(NAME cep_cxl_dcoh_tracker COMMAND cpptlm_tests "[cep][cxl-dcoh][tracker]" -r compact)

# 队列 #12 `cpptlm-cep-cxl-dcoh-symmetric`(第 1 个 change = 实现层)的 **实例声明判据**:
#   ① CDX-06 13 键齐备(**机器驱动**:schema `required` 实读)+ `perm` 4 值 + ★`back_invalidate=true`
#      (配正控:`cxl_2_0` 同项 = `false` ⇒ 二者可判别)
#   ② CDX-07 不声明静默升级 + 派生量 `no_upgrade_instances` 恰含 2 个
#   ③ CDX-08 `AC-05` 走 **SKIP**(不得 FAIL / 不得 PASS)+ 既有族判定**回归锁** + 鉴别力
add_test(NAME cep_cxl_dcoh_instance_decl COMMAND cpptlm_tests "[cep][cxl-dcoh][instance-decl]" -r compact)

# 队列 #12 `cpptlm-cep-cxl-dcoh-symmetric-state-spec`(第 0 个 change = 规约层)的**反向验证**:
#   4 组毒化 fixture + 基线正控 ⇒ 手改载体任一处必红且**归因命中**判据 id。
#   ⚠️ 本组形态 = 「**毒化 ⇒ 判据必红**」(鉴别力)⇒ 与队列 #11 修**误报**的那一组
#   (「毒化 ⇒ 判据仍绿」= 回归防护)**形态相反** ⇒ 两组都须登记,避免混用。
add_test(NAME cep_cxl_dcoh_state_spec_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_cxl_dcoh_state_spec_reverse_verify.sh)

# 队列 #13 `cpptlm-cep-nvlink-sharp-state-spec`(第 0 个 change = 规约层)的判据:
#   ① 载体**自足**(只 include 标准库 · 无词表型枚举 · 无区域表类型)
#   ② R1/R2 分类判据 + **R4 epoch 语义双向**(未 commit ⇒ 放行;本仓已错过一次的回归锁)
#   ③ R3 注册三步原子 · R4 注销回归 · R5 fail-stop
#   ④ ordering A/B/C/D 的**可判定**谓词 · VC 预留**单一状态** · 能力域**节点内**
#   ⑤ **逐格 `basis`**(12 格)+ 9 项 pending + **非 pending 格数下限**(Phase 2)
#   ⑥ **逐格 `ref` 可解析到锚点摘要**(含**防空转** —— `A-n` 在标题、`N-n`/`C-n` 在表格)
#   ⚠️ 本组 = 正控 + 编译期锁的运行期见证;**反向 fixture** 见下一个 add_test。
add_test(NAME cep_nvlink_sharp_state_spec
         COMMAND cpptlm_tests "[cep][nvlink-sharp][state-spec]" -r compact)

# 队列 #13 规约层判据的**反向验证**(异步鉴别力):
#   **9 组 / 10 条**毒化 fixture(每条必红 **且** 归因 id 命中)+ **基线正控**;
#   两类失败形态都支持:编译期红(`static_assert` 文案)/ 运行期红(`PROBE:FAIL <id>`)。
add_test(NAME cep_nvlink_sharp_state_spec_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_nvlink_sharp_state_spec_reverse_verify.sh)

# 队列 #13 `cpptlm-cep-nvlink-sharp`(第 1 个 change = **实现层**)的判据:
#   ① `NvlinkAdapter` **零自有表副本** · 无 `transition_*` · 不暴露区域表类型;
#   ② `BroadcastNull` **行为实现**(caps 取自 traits · 全部出参 · 零状态容器);
#   ③ `nvlink_4` **13 键**齐备(schema `required` 实读)+ caps ↔ traits + `repo_owned`;
#   ④ **V3** barrier 分类 litmus 双向(归因 `CP-BARRIER-CLASS`);
#   ⑤ 「算子族**恰 6** 值」(源文本枚举项计数);
#   ⑥ **partial failure** 显式建模(归因 `CP-PARTIAL-FAIL`);
#   ⑦ 集体面绑定 + region(non-cacheable 降级)· ⑧ **V6-c** 前提约束 · ⑨ **CP-05** 清单扩展;
#   ⑩ **归因 id 元测试**(fixture id ⊆ design §7 id,双向)。
#   ⚠️ 本组 = 正控 + 运行期兜底;**反向 fixture** 见下一个 add_test。
add_test(NAME cep_nvlink_sharp
         COMMAND cpptlm_tests "[cep][nvlink-sharp]" -r compact)

# 队列 #13 **实现层**判据的**反向验证**(异步鉴别力):
#   **9 组**毒化 fixture(每条必红 **且** 归因 id **精确相等**命中)+ **基线正控**;
#   两类失败形态都支持:编译期红(`static_assert` 文案)/ 运行期红(`PROBE:FAIL <id>`)。
add_test(NAME cep_nvlink_sharp_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_nvlink_sharp_reverse_verify.sh)

# 队列 #14 `cpptlm-cep-ualink-inc-state-spec`(第 0 个 change = **规约层**)的载体判据:
#   ① INC 四原语恰 4 + `AllReduce` = REDUCE+BROADCAST **复合**(⛔ 不是第 5 原语)+ 映射不越出;
#   ② **逐格 basis** 完整(每格有 `ref`)+ `self-derived` 不得指向公开来源;
#   ③ **9 项 pending**(id 与 ref 一一对应)+ 形状型谓词极性;
#   ④ **载体自足**:只 include 标准库 + **零**平台标识 + **零**区域表 + **零**转移判定
#      (⚠️ 含**鉴别力自测**:同一扫描器对毒化文本必须命中 ⇒ 证明扫描器有牙)。
#   ⚠️ 本组 = 编译期锁的**运行期见证**(铁律 ③);**反向 fixture** 见 Phase 2。
add_test(NAME cep_ualink_inc_state_spec
         COMMAND cpptlm_tests "[cep][ualink-inc]" -r compact)

# 队列 #14 `cpptlm-cep-ualink-inc-state-spec`(**规约层**)的**反向验证**:
#   ① **基线正控**(干净载体必编译 + 探针全绿);
#   ② **归因 id 双向覆盖自检**(fixture 期望集合 ⟺ 载体断言文案集合);
#   ③ **11 组**毒化 fixture(每条必红 + 归因 id **命中** `static assertion failed: <id>: `);
#   ④ **探针鉴别力自测**(运行期分支 ⇒ 探针必须红)。
#   ⚠️ 归因匹配**带尾冒号** —— 裸子串会让 `UIS-01` 被 `UIS-10` 误命中。
add_test(NAME cep_ualink_inc_state_spec_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_ualink_inc_state_spec_reverse_verify.sh)

# 队列 #14 `cpptlm-cep-ualink-inc`(第 1 个 change = 实现层)的判据:
#   ① 归因 id 双向覆盖自检(fixture 期望集合 ⟺ 登记集合)
#   ② 基线正控(干净副本编译通过 + 探针全绿)
#   ③ **11 组 / 11 条**反向 fixture(毒化 ⇒ 探针红且归因**精确命中**)
add_test(NAME cep_ualink_inc_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_ualink_inc_reverse_verify.sh)

# 队列 #11 `cpptlm-cep-tilelink-tree-manager`(第 1 个 change = 实现层)的判据:
#   ① 载体契约(adapter 零副本 + `TreeManager` 的 `ESCALATE_UP`/`CHAIN` 真实使用
#      + `snoop_join_core` 的 4 步语义);
#   ② 实例声明面(13 键 / `perm` 3 值 / `vc_binding` 逐类 / AC-05 SKIP / 派生量首次非空);
#   ③ **反向验证**(13 条毒化 fixture + 基线正控)⇒ 手改任一侧必红且归因命中。
add_test(NAME cep_tilelink_adapter COMMAND cpptlm_tests "[cep][tilelink][adapter]" -r compact)
add_test(NAME cep_tilelink_instance_decl COMMAND cpptlm_tests "[cep][tilelink][instance-decl]" -r compact)
add_test(NAME cep_tilelink_adapter_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_tilelink_adapter_reverse_verify.sh)

# 队列 #11 `cpptlm-cep-tilelink-state-spec` 的**锚点核对表生成器自检**(只比对不写)——
# 12 格核对表 + 锚点统计块均由机器可读载体生成 ⇒ 手改任一侧(或载体改了忘重生成)⇒ rc≠0;
# 且生成器内置**逐格交叉核对**(每格 `ref` SHALL 一一对应锚点 §2.3bis 的真实条目号
# + 每行目标掩码 / 分桶 SHALL 与载体实算一致)⇒ 手改锚点同样 rc≠0。
# ⚠️ ID 文法**容忍 Markdown 粗体**且接受 `R-T<n>` 前缀(复用 `test/cep_anchor_ids.hh`)
#    ⇒ 合法 ID 集合**非空** ⇒ 逐格校验**不是**空转(per 二审 finding B-7)。
add_test(NAME cep_tilelink_anchor_checklist_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/generate_tilelink_anchor_checklist.sh --check)

# R11 的 **V-2** 判据(队列 #10 之后的独立面):
#   `moesi_gpu` 的 `O→WRITEBACK` **补偿路径** —— 合并点强制查询 tracker +
#   verdict 由 `completeness` 决定(保守补偿 ≠ 等价恢复)+ 记录逐字镜像实际返回值。
add_test(NAME cep_moesi_gpu_compensation COMMAND cpptlm_tests "[cep][moesi-gpu][compensation]" -r compact)

# 队列 #10 `cpptlm-cep-cxl-nonsymmetric`(实现层)的**实例声明面**判据:
#   `cxl_2_0` 的 13 必填键 + `sharer_tracker` = 「不参与」+ 权威分区 + 静默升级登记。
add_test(NAME cep_cxl_instance_decl COMMAND cpptlm_tests "[cep][cxl][instance-decl]" -r compact)

# 队列 #10 `cpptlm-cep-cxl-nonsymmetric`(实现层)的 **adapter** 判据:
#   零自有表副本(68 格 pass-through + 静态扫描 + 扫描器鉴别力自测)+ 三态不被折叠。
add_test(NAME cep_cxl_cache_adapter COMMAND cpptlm_tests "[cep][cxl][adapter]" -r compact)

# 队列 #10 `cpptlm-cep-cxl-nonsymmetric`(实现层)的**行为实现**判据:
#   `SparseSnoopFilter` —— CONSERVATIVE 保守型 snoop filter(caps/hazard 逐字段对齐
#   `ADR-CEP-04 §2`/`§4`;「永不 ANSWER_HERE」+ 漏探必红 + contention STALL + 合法收缩)。
add_test(NAME cep_sparse_snoop_filter COMMAND cpptlm_tests "[cep][sparse-snoop-filter]" -r compact)

# 队列 #10 后续:68 格核对表 + 摘要统计块的**生成器自检**(只比对不写)——
# 两者均由机器可读载体生成 ⇒ 手改任一侧(或载体改了忘重生成)⇒ rc≠0;
# 且生成器内置**交叉核对**(每格 `ref` SHALL 解析到摘要 §8 真实条目号)⇒ 手改摘要同样 rc≠0。
add_test(NAME cep_cxl_anchor_checklist_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/generate_cxl_anchor_checklist.sh --check)

# H7 对拍基准(辅助档)的**比较器自测** —— 不需要 gem5 ⇒ 可进 ctest;
# 判据逻辑坏掉(边界放行 / 静默 PASS / 缺归因)⇒ 必红。
add_test(NAME cep_parity_compare_selftest
         COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/cep/parity/selftest_parity_compare.py)

# H7 对拍基准的 **CppTLM 侧**执行器 —— 读**同一条** trace(CSV)→ 跑 TrafficGenTLM+CacheTLM
#   → 产出与 gem5 侧同格式的 JSON。未设 `PARITY_TRACE_CSV` ⇒ **跳过**(SKIP)。
add_test(NAME cep_parity_cpptlm_side
         COMMAND ${CMAKE_SOURCE_DIR}/build/bin/cpptlm_tests "[cep][parity]")
# ⚠️ Catch2 的 `SKIP` 返回码 = **4** ⇒ 登记为 ctest 的「跳过」返回码,
#    否则「没跑 trace 生成」的常规 CI 会把它当**失败**(实测:ctest 报 Failed)。
set_tests_properties(cep_parity_cpptlm_side PROPERTIES SKIP_RETURN_CODE 4)

# ⚠️ **主对拍档(固定命中率档)刻意不进 ctest 门禁** ——
#   `scripts/cep/parity/run_parity_main.sh` 的**结论本身就是 FAIL**
#   (P-2 在「统一 `h_0`」口径下不成立,per 台账 Entry #117)
#   ⇒ 它是**一次性实验**而非**回归门禁** ⇒ 把「预期红」写进 CI 会污染门禁语义。
#   ⇒ 人工复跑:`bash scripts/cep/parity/run_parity_main.sh`(详见 docs/validation/)。

# H7 对拍基准的**参数同源档端到端**(两侧**各自独立**按同一组参数生成序列
#   → 地址序列指纹(FNV-1a 64bit)交叉验证 → 比较)。
#   —— 这是**当前唯一可靠**的同源档:gem5 v24 的 protobuf 文件读取会提前 EOF
#      (per 台账 Entry #115);参数同源反而是**更强**的证据。
#   ⚠️ 需要 gem5 二进制(第三方基准,不入库);缺席时 exit 77 ⇒ ctest 跳过。
add_test(NAME cep_parity_linear_e2e
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/cep/parity/run_parity_linear.sh)
set_tests_properties(cep_parity_linear_e2e PROPERTIES
                     SKIP_RETURN_CODE 77
                     TIMEOUT 1800)

# H7 对拍基准的**同源 trace 档端到端**(trace 生成 → gem5 回放 → CppTLM 回放 → 比较)
#   —— 两侧消费**同一条**访问流(per `14 号` §4.1)。
#   ⚠️ 需要 gem5 二进制(第三方基准,不入库);缺席时 exit 77 ⇒ ctest 跳过。
add_test(NAME cep_parity_trace_e2e
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/cep/parity/run_parity_trace.sh)
set_tests_properties(cep_parity_trace_e2e PROPERTIES
                     SKIP_RETURN_CODE 77
                     TIMEOUT 1800)

# H7 对拍基准的**端到端实测**(gem5 探针 → 比较器)——
# ⚠️ 需要 gem5 二进制(第三方基准,**不入库**;构建入口 = scripts/cep/parity/build_gem5.sh)。
#    gem5 缺席时**跳过**(SKIP_RETURN_CODE 77)⇒ 不阻塞常规 CI,
#    但在「装了 gem5」的机器上会真跑并对判据。
add_test(NAME cep_parity_e2e
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/cep/parity/run_parity.sh)
set_tests_properties(cep_parity_e2e PROPERTIES
                     SKIP_RETURN_CODE 77
                     TIMEOUT 1800)

add_test(NAME cep_chstream_bidirectional_macro_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_chstream_bidirectional_macro_reverse_verify.sh)

# 队列 #7 `cpptlm-cep-cohif-carrier` 的 3 个判据收口用例(per Phase 6 task 6.1)
# —— 显式登记(除 `cep_all` 的 `[cep]` 通配外)⇒ 反向 fixture 的**登记位置**可查。
# ⚠️ `cep_cohif_carrier` 内含**编译期**反向 fixture(在 ctest 内调编译器断言
#    「编译确实失败」且失败原因命中预期断言)⇒ 耗时高于普通 Catch2 用例。
add_test(NAME cep_cohif_carrier
         COMMAND cpptlm_tests "[cep][cohif-carrier]" -r compact)
add_test(NAME cep_protocol_bridge_contracts
         COMMAND cpptlm_tests "[cep][protocol-bridge]" -r compact)
add_test(NAME cep_json_assembly
         COMMAND cpptlm_tests "[cep][json-assembly]" -r compact)

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

# `validate_topology` target 的对账闸(per 台账 Entry #358)。
# ⚠️ 该 target 此前首条命令引用**已被物理删除**的 config ⇒ 整条 target 跑不完 ⇒
#    其后 4 条拓扑校验 + CEP 装配闸 CLI 挂点**全部从未执行**(静默 20 天)。
#    ⛔ ctest 侧的 `cep_channel_dependency_gate` 用例**独立注册** ⇒ 即使挂点消失
#    它仍绿 ⇒ 本闸是把该失效**变可见**的唯一手段。
add_test(NAME cep_validate_topology_reconcile_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_validate_topology_reconcile_check.sh)
set_tests_properties(cep_validate_topology_reconcile_check
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

# 声明对齐的**反向验证**(上者的「防自证」对手:40 条毒化副本)
# ⚠️ **2026-11-12 补注册**(per 台账 Entry #343 = verifier 报告 F-1):本脚本此前**在
#    `scripts/README.md` 守门表内且状态列写「✅ 生效」,却**从未** `add_test` ⇒
#    **CI 从不执行它** ⇒ 「登记了,但没有牙齿」的同缺陷类(与 C1 同类,只是不在 C1 的 5 子项内)。
#    2026-11-12 起**真进 CI**(实测 3 连跑 rc=0 · ~5.7s · 不写仓内文件 ⇒ 可安全并行)。
add_test(NAME cep_decl_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_decl_reverse_verify.sh)
set_tests_properties(cep_decl_reverse_verify
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# 预登记 out-of-scope 集合的**真源 ↔ 机器投影 drift 检测**
# (per ADR-CEP-06 SU#2 ② 门禁②:objective §9.1 ② 表 ↔ experimental/preregistration.json
#  逐假设集合相等 —— 两侧互为锁,单改一侧即红)
# ⚠️ 本闸**无 SKIP 路径**:两侧都在本仓内 ⇒ 缺失 = 本仓缺陷(不是外部条件)⇒ 退出 2 而非跳过。
add_test(NAME cep_prereg_drift_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_prereg_drift_check.sh)
set_tests_properties(cep_prereg_drift_check
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# IC3 报告层**三态**的机器 fixture(per ADR-CEP-06 SU#3 §②A / §⑤)
# —— 合成 4 个 run 覆盖 ✓通过 / ⚪不适用 / ❌违例 / **连带洞**(dv 未采集 ∧ mv 有实测违例)
# ⚠️ **为什么必须有**:当前 4 个真实 run(h2/h3/h4/h6)**无一**落在「✓通过」态
#   ⇒ 若无合成 fixture,「通过」分支是**未被验证的死代码**。
add_test(NAME cep_ic3_report_states_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_ic3_report_states_check.sh)
set_tests_properties(cep_ic3_report_states_check
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# H3 v2「一致性 × 完成域」复合装置的两道文件层闸(per ADR-CEP-06 SU#4 §② 约束 5/6)
#  · 观测键:实例级/地址级键集须落 metrics.json **且** trace.h5,覆盖率 = 1.0
#    (退化装置「两段拼接」⇒ 交集为空 ⇒ 判红)
#  · 耦合探针:阻塞开/关两份独立 metrics.json 的内存违例计数 Δ 须与**预测锚**一致
#    (锚须先于 run 产出;⛔ 无锚不产出)
add_test(NAME cep_b4_observation_keys_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_b4_observation_keys_check.sh)
set_tests_properties(cep_b4_observation_keys_check
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME cep_b4_coupling_probe
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_b4_coupling_probe.sh)
set_tests_properties(cep_b4_coupling_probe
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# `INV-CEP-06-IC1` 的 4 件套完整性闸自检(缺一件 = 实验 fail)
#  · 只跑 fixture,不产实验 ⇒ 可在 CI 常驻;正向(齐备⇒通过)/ 反向(缺任一⇒失败)双向
add_test(NAME cep_experiment_run_selftest
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_experiment_run.sh --selftest)
set_tests_properties(cep_experiment_run_selftest
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# B2「66 条 invariant 逐条有牙载体」的映射表闸(per 台账 Entry #232 / #233)
#  · 校验五项: ⏭ id ⊆ 66 / deferral 三字段非空 / 有牙载体 ∪ 授权⏭ == 66 且交为空 /
#    ledger_entry 解析到真实台账 Entry 且正文含该 id / 每个载体标记邻域有断言宏(反「恒真占位」)
#  · --for-achievement 模式:⏭-pending 必须为 0,否则拒绝 B2 达成宣称
add_test(NAME cep_invariant_map_check
         COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/test/cep_invariant_map_check.py --check)
set_tests_properties(cep_invariant_map_check
                     PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

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

# change `cpptlm-cep-basis-source-kind-and-cell-free`(2026-11-06,台账 Entry #252)的判据:
#   ① 4 载体逐格 `ref` 可解析 + `basis ↔ ref_kind` **按载体**映射一致;
#   ② `sources.json` ↔ `kSources[]` 逐条一致;
#   ③ `CellKind::Free` 四态互斥 + 当前 0 格 + adapter 显式分支;
#   ④ 决定性变异三态(clean / dropped / downgraded)签名钉死 + 真源 sha256 未变。
add_test(NAME cep_source_registry
         COMMAND cpptlm_tests "[cep][source-registry]" -r compact)
add_test(NAME cep_source_effect_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_source_effect_check.sh)

# change `cpptlm-cep-dcoh-nvlink-refkind-migration`(2026-11-07,台账 Entry #266)的判据:
#   ① `cxl_dcoh` / `nvlink_sharp` 接入 `RefKind` 后的逐格一致性(含「PublicDirect 指向 Derived/AdrContract ⇒ 红」负控);
#   ② **零追溯降级**(许可表 / 来源枚举 / basis 取值未被改动);
#   ③ B3b-2 的 **deferral 机器可查登记**校验(4 族齐全 + 字段齐备 + 台账凭据可解析)
#      ⚠️ 该闸**不**改 B3b-2 计数(仍 0/4)—— 登记 ≠ 达成(per `07 号` SU · 六 / Entry #264)。
add_test(NAME cep_refkind_coverage
         COMMAND cpptlm_tests "[cep][refkind][coverage]" -r compact)
add_test(NAME cep_b3b2_deferral_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_b3b2_deferral_check.sh)

# change `cpptlm-cep-ualink-coherence-engine`(2026-11-07,台账 Entry #269)的判据:
#   ① UALink **记账级专用承载**引擎的三分法观测面(记账 2 / 拒绝 7 / pending 1)+ 4 条机器可判不变量;
#   ② completion 记账**不依赖有序性**(中途探针 + 端态 + **决定性变异** ⇒ 判据有牙);
#   ③ 装配闭环(`ualink_4` **required** plane 绑定 + 负控)+ **零发射**;
#   ④ **B3b-2 仍 0/4** 的断言显式存在(本 change **不改**计数,per `07 号` SU · 四 D-1)。
add_test(NAME cep_ualink_engine
         COMMAND cpptlm_tests "[cep][ualink][engine]" -r compact)

# change `cpptlm-cep-refkind-carrier-whitelist-contract`(2026-11-07,台账 Entry #272)的判据:
#   载体自足性的**唯一白名单**已登记进契约面,且**唯一 + 三处一致**(契约面 + UIS-10 探针 + CP-05 探针)。
#   ⚠️ doc-only:0 源码 / 0 探针实现 / 0 声明 / 0 计数改动。
add_test(NAME cep_refkind_whitelist_contract
         COMMAND cpptlm_tests "[cep][refkind][whitelist]" -r compact)

# `B3b-2` 受限族的 **D-4 触发探测清单**闸(2026-11-07,per 台账 Entry #274 §④ E1 + Entry #277):
#   ① 探测清单的结构 / 覆盖谓词(4 族 × 2 路径 = 8 槽位)/ 合规边界 / 防漂移
#   ② **5 个决定性变异**按固定签名变红(证明判据有牙,非恒真)
#   ⚠️ 本闸只校验**协议清单**;⛔ 不保证执行者未在其他渠道接触规范正文(incident 登记 + 季度人工审计)
#   ⛔ **不改**任何 DoD 计数(B3b-2 仍 0/4)
add_test(NAME cep_b3b2_d4_probe_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_b3b2_d4_probe_check.sh)

# ── 2026-11-07(Entry #278):**注册债**清理 ──────────────────────────────
# ⚠️ 此前 3 个反向验证脚本**既未注册 ctest 也未登记进 `scripts/README.md`**
#    ⇒ 它们**从不被 CI 执行**(判据可能早已失效而无人知道)。
#    实测三者均跑绿(rc=0 / 12s·1s·4s)⇒ 是**注册债**而非判据漂移 ⇒ 本次注册。
add_test(NAME cep_cxl_dcoh_adapter_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_cxl_dcoh_adapter_reverse_verify.sh)
add_test(NAME cep_freeze_reverse_verify
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_freeze_reverse_verify.sh)
add_test(NAME cep_tilelink_engine_failclosed
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_tilelink_engine_failclosed.sh)

# ── 2026-11-09(per 台账 Entry #303):**CEP E2E 双向闸**(台账 ↔ 仓内 run 产物)──
# retention **丙**(Entry #295):首批 run 只入库 `metrics.json` + `simulation_config.json`
# ⇒ 台账结论首次**可对账**(`metrics_sha256` ↔ 仓内文件);零 orphan + sha256 相等。
add_test(NAME cep_e2e_reconcile_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_e2e_reconcile_check.sh)

# ── 2026-11-09(per 台账 Entry #305 / Entry #295 F-08 残余):**索引位置闸** ──
# 「索引永久置于文末」纪律(新增段落 SHALL 插在索引之前 ⇒ 既有 path:line 锚点零位移)
# 的**机器可判**载体:索引之后零个 `## ` + 索引计数自述与实际逐个相等。
add_test(NAME cep_index_position_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_index_position_check.sh)

# ── 2026-11-13(per 台账 Entry #373 / change cpptlm-cep-dod-reading-consistency):**DoD 读法闸** ──
# 「唯一权威声明处 + 其余载体只写指针」纪律的机器可判载体:4 条整行锚(整行全等 + 唯一承载
# 文件 + 行次数==1)+ 旧文零命中 + 作用域要素 + 绝对禁令不复活 + 5 条禁表述 + 指针目标存在,
# 并配 10 条决定性变异(M1~M10 · 只在影子树)。
add_test(NAME cep_dod_reading_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_dod_reading_check.sh)

# ── 2026-11-13(per 台账 Entry #379 / change cpptlm-cep-iag-gate-execution-arm):**IAG 执行臂对账闸** ──
# 「装配闸族(IAG)不再只测不拦」的对账载体:两臂(装配路径臂 + 离线 CLI 臂)存在且共用
# 同一份判据实现与加载器 + 判据本体零 diff + CLI 已注册 + validate_topology 的 IAG
# 第二段在命令面内 + CEP config 清单 == 磁盘实数;并配 2 条决定性变异。
add_test(NAME cep_iag_gate_reconcile_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_iag_gate_reconcile_check.sh)

# ── 2026-11-13(per 台账 Entry #384 / change cpptlm-cep-b3b2-archive-form):**归档形态对账闸** ──
# B3b-2 归档形态(MUST-FIX-3 六项)+ 回滚路径(MUST-FIX-4)的对账载体:形态定义齐备 +
# 闸的锚解析未降级 + 重定向表 + 07 号 维护态指针闭合 + 撤回标记成对;配 2 条决定性变异。
add_test(NAME cep_b3b2_archive_form_check
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/cep_b3b2_archive_form_check.sh)
