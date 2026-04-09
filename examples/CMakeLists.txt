# examples/CMakeLists.txt — CppTLM 示例程序

# ==============================================================================
# Cache 系统示例
# ==============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cache_system/main.cpp")
    add_executable(example_cache cache_system/main.cpp)
    target_link_libraries(example_cache PRIVATE cpptlm_core)
endif()

# ==============================================================================
# NoC 系统示例
# ==============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/noc_system/main.cpp")
    add_executable(example_noc noc_system/main.cpp)
    target_link_libraries(example_noc PRIVATE cpptlm_core)
endif()

# ==============================================================================
# 输出目录
# ==============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples)
