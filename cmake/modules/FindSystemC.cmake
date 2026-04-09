# FindSystemC.cmake — 查找 SystemC 头文件
#
# 搜索结果:
#   SystemC_FOUND        — 是否找到
#   SystemC_INCLUDE_DIR  — 头文件目录
#
# 查找顺序:
#   1. 系统安装路径 (/usr/include, /usr/local/include)
#   2. 项目内路径 (external/systemc/include)
#   3. 环境变量 SYSTEMC_HOME

find_path(SystemC_INCLUDE_DIR
    NAMES systemc.h
    PATHS
        /usr/include
        /usr/local/include
        ${CMAKE_SOURCE_DIR}/external/systemc/include
        $ENV{SYSTEMC_HOME}/include
    DOC "SystemC include directory"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SystemC
    REQUIRED_VARS SystemC_INCLUDE_DIR
)

if(SystemC_FOUND AND NOT TARGET SystemC::SystemC)
    add_library(SystemC::SystemC INTERFACE IMPORTED)
    set_target_properties(SystemC::SystemC PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SystemC_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(SystemC_INCLUDE_DIR)
