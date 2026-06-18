// include/tlm/cluster/cpu_cluster.cc
// CpuCluster 实现占位文件 (v2.2)
//
// 说明:
//   CpuCluster 的所有成员函数 (构造函数 / set_config / tick / get_module_type)
//   均在 cpu_cluster.hh 中以 inline 形式实现,确保:
//   1. 无需单独的 .cc 编译即可被任意 TU 通过 #include 使用 (与 legacy/cpu_cluster.hh 同模式)
//   2. 避免 SimModule 派生类在 include/modules.hh 注册宏处出现未定义符号
//
// 未来扩展:
//   - 当 CpuCluster 包含非平凡实现 (>20 行) 时,可将实现迁出至本 .cc
//   - 届时需同步更新 src/CMakeLists.txt 的 CORE_SOURCES 列表加入本文件
//   - 当前 keep-implementation-in-header 模式与 v2.1 legacy/cpu_cluster.hh 一致
//
// 作者: CppTLM Team
// 日期: 2026-06-18

#include "tlm/cluster/cpu_cluster.hh"

// 当前所有实现位于头文件 (inline)。本 .cc 文件作为占位存在,
// 以保持与 openspec/json-nested-simmodule Stage 1 任务文件路径约束一致。
// 包含 cpu_cluster.hh 确保任何将本 .cc 加入编译单元的项目仍能链接到符号。