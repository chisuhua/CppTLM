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
# T-W3-2 (T-ae-4): dlopen usage template for UsrLinuxEmu integration
# (per ADR-088 §D5 + ADR-SOC-07 D5). 演示 dlopen libcpptlm_emulator.so +
# dlsym 19 forward ABI + 调通关键路径 (get_version/create_by_id/mmio_/destroy).
# ==============================================================================
add_executable(test_cpptlm_emulator_dlopen
               test_cpptlm_emulator_dlopen/test_dlopen.cc)
target_link_libraries(test_cpptlm_emulator_dlopen PRIVATE cpptlm_core cpptlm_emulator)

# ==============================================================================
# 输出目录
# ==============================================================================
# ==============================================================================
# 输出目录
# ==============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples)

# ==============================================================================
# HSK-8 Phase 2 Step 6: examples 同样需要 PTX-EMU shared libs
# ==============================================================================
if(CPPTLM_WITH_PTX_EMU)
    set(PTXEMU_EXAMPLE_LIBS ptxemu_device ptxsim ptx_parser ptxir cudart)
    foreach(example
            example_basic_transaction
            example_error_handling
            example_simmodule_nested)
        if(TARGET ${example})
            target_link_libraries(${example} PRIVATE ${PTXEMU_EXAMPLE_LIBS})
        endif()
    endforeach()
endif()
