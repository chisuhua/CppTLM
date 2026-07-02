// include/modules_cluster.hh
// SimModule 派生类集中注册 (Phase 2: GPU 端 4 个核心类 + CpuCluster)
// REGISTER_MODULE 宏走 getModuleRegistry() (双注册表 SimModule 路径)
//
// 使用方式: 在 #include "modules.hh" (拿到 REGISTER_MODULE 宏) 之后
//           再 #include "modules_cluster.hh" (触发集中注册)
//
// 不在 modules.hh 末尾自动 include 是为了避免循环 include:
//   modules.hh 定义 REGISTER_MODULE 宏 + 末尾 include modules_cluster.hh
//   modules_cluster.hh 使用 REGISTER_MODULE 宏 (需 modules.hh 先处理)
//
// 参考: include/AGENTS.md 注册宏体系 + include/modules.hh::REGISTER_MODULE
//       docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §0.4
// 作者: Sisyphus / 日期: 2026-06-19
#ifndef MODULES_CLUSTER_HH
#define MODULES_CLUSTER_HH

#include "tlm/cluster/cpu_cluster.hh"
#include "tlm/cluster/compute_cluster.hh"
#include "tlm/cluster/tpc_cluster.hh"
#include "tlm/cluster/gpc_cluster.hh"
#include "tlm/cluster/gpu_cluster.hh"
#include "tlm/cluster/cache_cluster.hh"
#include "tlm/cluster/memory_cluster.hh"
#include "tlm/cluster/gpu_noc_cluster.hh"
#include "tlm/cluster/apu_soc.hh"
#include "tlm/gpu/gpu_soc_tlm.hh"
#include "core/module_factory.hh"

// ComputeCluster / TpcCluster / GpcCluster / GpuCluster / CacheCluster / MemoryCluster / GpuNoC / ApuSoC 在 cpptlm::tlm 命名空间
using namespace cpptlm::tlm;

// 注册表写入是 expression statement, C++ 全局作用域禁止 expression statement,
// 必须包成变量初始化: const auto _reg_X = REGISTER_MODULE(X);  触发 static init
const bool _reg_cpucluster = (REGISTER_MODULE(CpuCluster), true);
const bool _reg_computecluster = (REGISTER_MODULE(ComputeCluster), true);
const bool _reg_tpccluster = (REGISTER_MODULE(TpcCluster), true);
const bool _reg_gpccluster = (REGISTER_MODULE(GpcCluster), true);
const bool _reg_gpucluster = (REGISTER_MODULE(GpuCluster), true);
const bool _reg_cachecluster = (REGISTER_MODULE(CacheCluster), true);
const bool _reg_memorycluster = (REGISTER_MODULE(MemoryCluster), true);
const bool _reg_gpunoc = (REGISTER_MODULE(GpuNoC), true);
const bool _reg_apusoc = (REGISTER_MODULE(ApuSoC), true);
const bool _reg_gpusoc = (REGISTER_MODULE(GpuSocTLM), true);

#endif  // MODULES_CLUSTER_HH
