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
# 输出目录
# ==============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples)
