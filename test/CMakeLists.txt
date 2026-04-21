# test/CMakeLists.txt
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(Catch2)

file(GLOB TEST_SOURCES "test_*.cc")

# 只使用 Catch2 库，不使用 amalgamated（避免重复定义）
add_executable(cpptlm_tests ${TEST_SOURCES})

target_include_directories(cpptlm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(USE_SYSTEMC_STUB)
    target_compile_definitions(cpptlm_tests PRIVATE USE_SYSTEMC_STUB)
else()
    find_library(SYSTEMC_LIB systemc HINTS /usr/lib/x86_64-linux-gnu)
    target_link_libraries(cpptlm_tests PRIVATE ${SYSTEMC_LIB})
endif()

target_link_libraries(cpptlm_tests PRIVATE cpptlm_core Catch2::Catch2WithMain)

include(CTest)
include(Catch)

if(ENABLE_COVERAGE)
    target_compile_options(cpptlm_tests PRIVATE --coverage)
    target_link_options(cpptlm_tests PRIVATE --coverage)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 传入项目根目录路径，使测试可执行文件能找到 configs/
target_compile_definitions(cpptlm_tests PRIVATE CPPTLM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
