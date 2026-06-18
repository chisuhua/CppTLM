// include/modules_cluster.hh
// 集群模块集中注册宏 (Phase P2 占位文件)
// 当前阶段 (P1) 为占位,实际 REGISTER_MODULE 仍位于 include/modules.hh
// P2 将新增 4 个 GPU 端核心 SimModule 的 REGISTER_MODULE 集中入口:
//   ComputeCluster / TpcCluster / GpcCluster / GpuCluster
// P4 / P5 将追加 CacheCluster / MemoryCluster / GpuNoC / ApuSoC
//
// 作者: Sisyphus / 日期: 2026-06-19
// 参考: docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §6
#ifndef MODULES_CLUSTER_HH
#define MODULES_CLUSTER_HH

// P1 占位: 暂不引入任何注册宏,后续 P2 在此追加。
// 当前 SimModule 派生类 (CpuCluster) 通过 include/modules.hh::REGISTER_MODULE 注册。
#include "tlm/cluster/cpu_cluster.hh"

#endif // MODULES_CLUSTER_HH