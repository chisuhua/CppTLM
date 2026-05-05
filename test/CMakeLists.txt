# test/CMakeLists.txt
# 使用本地预编译的 Catch2（catch_amalgamated.cpp/.hpp）
# 避免 GitHub 网络下载问题
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB TEST_SOURCES "test_*.cc")

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
