# src/rtl/CMakeLists.txt — RTL桥接库（C++20 + CppHDL）

# CppHDL路径
set(CppHDL_ROOT "${CMAKE_SOURCE_DIR}/external/CppHDL")
set(CppHDL_LIB "${CppHDL_ROOT}/build/libcpphdl.a")

# 检查CppHDL库是否存在
if(NOT EXISTS ${CppHDL_LIB})
    message(WARNING "CppHDL library not found at ${CppHDL_LIB}. "
        "RTL bridge will not be built. Run: cd external/CppHDL && cmake -S . -B build")
endif()

# RTL桥接静态库
add_library(cpptlm_rtl STATIC
    hybrid_cache_wrapper.cc
    hybrid_cache_component.cc
)

target_include_directories(cpptlm_rtl PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CppHDL_ROOT}/include           # ch.hpp, simulator.h
    ${CppHDL_ROOT}/include/core      # context.h, lnodeimpl.h
    ${CppHDL_ROOT}/include/ast       # ast_nodes.h
    ${CppHDL_ROOT}/include/utils     # logger.h
    ${CppHDL_ROOT}/include/lnode     # uint.tpp
    ${CppHDL_ROOT}/external/inipp    # inipp/inipp.h (needed by simulator.h)
)

target_include_directories(cpptlm_rtl PRIVATE
    ${CMAKE_SOURCE_DIR}/external/json
)

# C++20标准
target_compile_features(cpptlm_rtl PUBLIC cxx_std_20)

# 链接CppHDL和LLVM
target_link_libraries(cpptlm_rtl PUBLIC
    cpptlm_core
)

target_link_libraries(cpptlm_rtl PRIVATE
    ${CppHDL_LIB}
    LLVM-22
)

# CppHDL编译选项
target_compile_options(cpptlm_rtl PRIVATE
    -Wno-unused-parameter
    -Wno-deprecated-declarations
    -w  # 静默CppHDL内部警告
)
