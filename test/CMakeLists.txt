# test/CMakeLists.txt — CppTLM 测试配置

# ==============================================================================
# Catch2 测试框架（FetchContent 自动下载）
# ==============================================================================
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# ==============================================================================
# 测试可执行文件
# ==============================================================================
file(GLOB TEST_SOURCES "test_*.cc")

add_executable(cpptlm_tests
    ${TEST_SOURCES}
    catch_amalgamated.cpp
)

target_link_libraries(cpptlm_tests PRIVATE
    cpptlm_core
    Catch2::Catch2WithMain
)

# 测试额外包含目录（mock 头文件等）
target_include_directories(cpptlm_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# SystemC 定义（与主库一致）
if(USE_SYSTEMC)
    target_compile_definitions(cpptlm_tests PRIVATE USE_SYSTEMC)
endif()

# ==============================================================================
# 测试发现
# ==============================================================================
include(CTest)
include(Catch)
catch_discover_tests(cpptlm_tests)

# ==============================================================================
# 覆盖率选项
# ==============================================================================
if(ENABLE_COVERAGE)
    target_compile_options(cpptlm_tests PRIVATE --coverage)
    target_link_options(cpptlm_tests PRIVATE --coverage)
endif()

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
