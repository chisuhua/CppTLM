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
#include "tlm/gpu/dgpu_soc.hh"
#include "tlm/gpu/gmmu_tlm.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"
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
const bool _reg_dgpusoc = (REGISTER_MODULE(DGpuSoc), true);
// Phase 2 (pcie-endpoint-ip-simmodule-refactor): EP 从 REGISTER_CHSTREAM 迁 REGISTER_MODULE
// 双注册 (FQ + 短名): 配置 JSON "type": "PcieEndpointIP" 必须可解析 (per dgpu_soc
// v1.0 配置文件 + examples/dgpu_soc_with_pcie_ip.json); FQ 路径仍兼容既有代码。
// 短名原本只在 test/test_pcie_endpoint_ip_simmodule_refactor.cc 测试 TU 注册,
// 跨 TU 掩蔽测试通过但生产环境 (libcpptlm_emulator.so / 主可执行) 静默失败
// (pcie_ep nullptr → msix_*/pcie_config_* 返 -ENOSYS)。现统一在生产注册表登记。
const bool _reg_pcieendpointip = (REGISTER_MODULE(tlm::pcie::PcieEndpointIP), true);
const bool _reg_pcieendpointip_short =
    (ModuleFactory::registerModule<tlm::pcie::PcieEndpointIP>("PcieEndpointIP"), true);
// Phase 2 (minimal-dgpu-soc-v1 A3): GmmuTLM 一级页表翻译
// 双注册 (short + 全限定): 配置 JSON "type": "GmmuTLM" 必须可解析 (per minimal-dgpu-soc
// v1.0 配置文件 + docs/designs/2027-02-09-minimal-dgpu-soc.md 示例)。
const bool _reg_gmmutlm = (REGISTER_MODULE(tlm::gpu::GmmuTLM), true);
const bool _reg_gmmutlm_short =
    (ModuleFactory::registerModule<tlm::gpu::GmmuTLM>("GmmuTLM"), true);

#endif  // MODULES_CLUSTER_HH
