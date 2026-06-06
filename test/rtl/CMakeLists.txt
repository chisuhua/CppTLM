# test/rtl/CMakeLists.txt — RTL smoke tests

add_executable(cpptlm_rtl_tests
    ../test_cppHDL_smoke.cc
    ../catch_amalgamated.cpp
)

target_include_directories(cpptlm_rtl_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/external/CppHDL/include
    ${CMAKE_SOURCE_DIR}/external/CppHDL/include/core
    ${CMAKE_SOURCE_DIR}/external/CppHDL/include/ast
    ${CMAKE_SOURCE_DIR}/external/CppHDL/include/utils
    ${CMAKE_SOURCE_DIR}/external/CppHDL/include/lnode
    ${CMAKE_SOURCE_DIR}/external/CppHDL/external/inipp
)

target_compile_features(cpptlm_rtl_tests PRIVATE cxx_std_20)

target_link_libraries(cpptlm_rtl_tests PRIVATE
    cpptlm_rtl
    cpptlm_core
    LLVM-22
)

# 添加CTest
add_test(NAME RTL_Smoke COMMAND cpptlm_rtl_tests "[cpphdl][smoke]")
