# examples/CMakeLists.txt — CppTLM 示例程序
#
# 注意：原文件用 `if(EXISTS cache_system/main.cpp)` 守卫 `example_cache`/`example_noc`
# target，但 cache_system/ 和 noc_system/ 目录不存在，target 永不触发。
# 修复后（2026-06-08）：删除无效守卫，仅注册实际存在的源文件。

# 共享 include 路径（example_*.cc 引用 core/ ext/ framework/）
set(EXAMPLES_INCLUDE_DIRS
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/include/core
 ${CMAKE_SOURCE_DIR}/external/json
)

# ==============================================================================
# 基础交易追踪示例（TransactionTracker + TransactionContextExt）
# ==============================================================================
add_executable(example_basic_transaction example_basic_transaction.cc)
target_link_libraries(example_basic_transaction PRIVATE cpptlm_core)
target_include_directories(example_basic_transaction PRIVATE ${EXAMPLES_INCLUDE_DIRS})

# ==============================================================================
# 错误处理示例（ErrorContextExt + DebugTracker）
# ==============================================================================
add_executable(example_error_handling example_error_handling.cc)
target_link_libraries(example_error_handling PRIVATE cpptlm_core)
target_include_directories(example_error_handling PRIVATE ${EXAMPLES_INCLUDE_DIRS})

# ==============================================================================
# SimModule 多层嵌套示例 (v2.2, openspec/json-nested-simmodule Stage 4)
# ==============================================================================
add_executable(example_simmodule_nested example_simmodule_nested.cc)
target_link_libraries(example_simmodule_nested PRIVATE cpptlm_core)
target_include_directories(example_simmodule_nested PRIVATE ${EXAMPLES_INCLUDE_DIRS})

# ==============================================================================
# Phase 7: PCIe Full E2E Demo (HostBypassTLM + PcieRootComplexTLM <-> PcieEndpointIP)
# ==============================================================================
add_executable(demo_pcie_full_e2e demo_pcie_full_e2e.cc)
target_link_libraries(demo_pcie_full_e2e PRIVATE cpptlm_core)
target_include_directories(demo_pcie_full_e2e PRIVATE ${EXAMPLES_INCLUDE_DIRS})

# ==============================================================================
# T26 (Phase C cpptlm-dgpu-realip-refactor): 4-IP Topology Assembly Demo
# 装载 configs/dgpu_soc_v2_realip_p2.json (T23B 重建后的嵌套 DGpuSoc) +
# 实例化 DGpuSoc + PcieControllerIP + GpuInternalFabric + VramControllerTLM.
# 验证 4 IP accessible + T25A.1 crossbar auto-attach + T23C backing null
# + T25B facade 5 BAR decode stimuli.
# 链接 cpptlm_emulator 让 VramControllerTLM (REGISTER_CHSTREAM) 在 cpptlm_emulator.so
# 静态初始化时注册 (per src/abi/cpptlm_emulator.cc:31-40)。
# ==============================================================================
add_executable(demo_dgpu_soc_realip demo_dgpu_soc_realip.cpp)
target_link_libraries(demo_dgpu_soc_realip PRIVATE cpptlm_core cpptlm_emulator)
target_include_directories(demo_dgpu_soc_realip PRIVATE ${EXAMPLES_INCLUDE_DIRS})

# ==============================================================================
# T-W3-2 (T-ae-4): dlopen usage template for UsrLinuxEmu integration
# (per ADR-088 §D5 + ADR-SOC-07 D5). 演示 dlopen libcpptlm_emulator.so +
# dlsym 19 forward ABI + 调通关键路径 (get_version/create_by_id/mmio_/destroy).
# ==============================================================================
add_executable(test_cpptlm_emulator_dlopen
               test_cpptlm_emulator_dlopen/test_dlopen.cc)
target_link_libraries(test_cpptlm_emulator_dlopen PRIVATE cpptlm_core cpptlm_emulator)

# T-RG-5: 注册到 ctest 让 CI 自动验证 dlopen 集成 (per release-gate tasks.md)
# ENVIRONMENT 注入 LD_LIBRARY_PATH 让 dlopen 能找到 libcpptlm_emulator.so
# (UsrLinuxEmu linux_compat 端 dlopen 模板的镜像测试, AE-G5)
add_test(NAME test_cpptlm_emulator_dlopen COMMAND test_cpptlm_emulator_dlopen)
set_tests_properties(test_cpptlm_emulator_dlopen PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{LD_LIBRARY_PATH}")

# ==============================================================================
# T4 (Phase 9 P0.5-cpptlm-minimal-dgpu-soc-v1-landing): sibling dlopen unit test
# 验证 minimal SoC (configs/dgpu_soc_minimal_v1.json) ABI 闭环: cpptlm_emulator_create
# + BAR1 framebuffer 自动分配 + mmio_write/read round-trip. 选 Oracle 推荐 (b):
# 新建 sibling 文件而非修改既有模板.
# ==============================================================================
add_executable(test_cpptlm_emulator_dlopen_minimal_soc
               test_cpptlm_emulator_dlopen/test_dlopen_minimal_soc.cc)
target_link_libraries(test_cpptlm_emulator_dlopen_minimal_soc PRIVATE cpptlm_core cpptlm_emulator)
add_test(NAME test_cpptlm_emulator_dlopen_minimal_soc
         COMMAND test_cpptlm_emulator_dlopen_minimal_soc)
set_tests_properties(test_cpptlm_emulator_dlopen_minimal_soc PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{LD_LIBRARY_PATH}")

# ==============================================================================
# 输出目录
# ==============================================================================
# ==============================================================================
# 输出目录
# ==============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples)

# ==============================================================================
# CEP PoC Spike S2(可丢弃单文件 spike,per docs/implementation/2026-10-22-cep-poc-spike-spec.md §3.2)
# 硬边界(规格 §1.2):不进 ModuleFactory / 不注册 ABI / 无公共 API / 零冻结产物改动
# 注册 ctest 为「确认 + 回归门」落地(S2 定位 per 台账 Entry #13 ④ / Entry #16);
# WORKING_DIRECTORY = 仓库根 —— spike 按相对路径读声明 JSON。
# ==============================================================================
add_executable(cep_poc_spike_s2 cep_poc_spike_s2.cc)
target_link_libraries(cep_poc_spike_s2 PRIVATE cpptlm_core)
target_include_directories(cep_poc_spike_s2 PRIVATE ${EXAMPLES_INCLUDE_DIRS})
add_test(NAME cep_poc_spike_s2 COMMAND cep_poc_spike_s2)
set_tests_properties(cep_poc_spike_s2 PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ==============================================================================
# CEP PoC Spike S1(可丢弃单文件 spike,per docs/implementation/2026-10-22-cep-poc-spike-spec.md §3.1)
# 硬边界(规格 §1.2):不进 ModuleFactory / 不注册 ABI / 无公共 API / 零冻结产物改动
# 红绿分离(per design.md D7-a):exit 0 = 判据工作正常(**判定结论可能为红**);
#   exit 1 = 判据退化(故意注入的错期望未被检出)→ 回归门因此能区分两者。
# WORKING_DIRECTORY = 仓库根 —— spike 按相对路径读冻结 schema。
# ==============================================================================
add_executable(cep_poc_spike_s1 cep_poc_spike_s1.cc)
target_link_libraries(cep_poc_spike_s1 PRIVATE cpptlm_core)
target_include_directories(cep_poc_spike_s1 PRIVATE ${EXAMPLES_INCLUDE_DIRS})
add_test(NAME cep_poc_spike_s1 COMMAND cep_poc_spike_s1)
set_tests_properties(cep_poc_spike_s1 PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ==============================================================================
# CEP PoC Spike S3(可丢弃单文件 spike,per docs/implementation/2026-10-22-cep-poc-spike-spec.md §3.3)
# 硬边界(规格 §1.2):不进 ModuleFactory / 不注册 ABI / 无公共 API / 零冻结产物改动
# 红绿分离(per design.md D7-a):exit 0 = 判据工作正常;exit 1 = 判据退化。
# WORKING_DIRECTORY = 仓库根 —— spike 按相对路径读声明 JSON。
# ==============================================================================
add_executable(cep_poc_spike_s3 cep_poc_spike_s3.cc)
target_link_libraries(cep_poc_spike_s3 PRIVATE cpptlm_core)
target_include_directories(cep_poc_spike_s3 PRIVATE ${EXAMPLES_INCLUDE_DIRS})
add_test(NAME cep_poc_spike_s3 COMMAND cep_poc_spike_s3)
set_tests_properties(cep_poc_spike_s3 PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ==============================================================================
# CEP 装配期判据的**离线挂点**(队列 #4,per ADR-CEP-09 抓取点 1)
# ==============================================================================
# 与 `src/core/module_factory.cc::instantiateAll()` 的装配闸**共享同一组判据函数**
# (tlm/cep/channel_dependency_graph.hh + tlm/cep/ordering_point_checker.hh),禁双实现。
# `scripts/CMakeLists.txt` 的 `validate_topology` target 追加本 CLI 调用。
# WORKING_DIRECTORY = 仓库根 —— CLI 按相对路径读 include/tlm/cep/protocol_instances.json。
add_executable(cep_channel_dependency_gate cep_channel_dependency_gate.cc)
target_link_libraries(cep_channel_dependency_gate PRIVATE cpptlm_core)
target_include_directories(cep_channel_dependency_gate PRIVATE ${EXAMPLES_INCLUDE_DIRS})
add_test(NAME cep_channel_dependency_gate COMMAND cep_channel_dependency_gate)
set_tests_properties(cep_channel_dependency_gate PROPERTIES
                     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# 队列 #5:collective plane 契约判据的离线挂点 CLI(per delta capability
# `cep-collective-plane-contract`)。与 test 侧**同一份**判据实现
# (`include/tlm/cep/collective_plane_contract.hh`)⇒ 守卫与被守卫对象不漂移。
add_executable(cep_collective_plane_contract cep_collective_plane_contract.cc)
target_link_libraries(cep_collective_plane_contract PRIVATE cpptlm_core)
target_include_directories(cep_collective_plane_contract PRIVATE ${EXAMPLES_INCLUDE_DIRS})
add_test(NAME cep_collective_plane_contract COMMAND cep_collective_plane_contract)
set_tests_properties(cep_collective_plane_contract PROPERTIES
                     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ==============================================================================
# HSK-8 Phase 2 Step 6: examples 同样需要 PTX-EMU shared libs
# ==============================================================================
if(CPPTLM_WITH_PTX_EMU)
    set(PTXEMU_EXAMPLE_LIBS ptxemu_device ptxsim ptx_parser ptxir cudart)
    foreach(example
            example_basic_transaction
            example_error_handling
            example_simmodule_nested
            demo_pcie_full_e2e)
        if(TARGET ${example})
            target_link_libraries(${example} PRIVATE ${PTXEMU_EXAMPLE_LIBS})
        endif()
    endforeach()
endif()
