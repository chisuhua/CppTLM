// include/tlm/gpu/gpu_soc_tlm.hh
// GpuSocTLM: gpu_soc 顶层容器 — 组合 GpuCluster + GpuMeshNoC + MemoryClusterTLM + KernelLaunchTLM
// 功能: GPU 芯片顶层仿真容器，通过 GpuClusterSharedInterface 与 apu_soc 共享 GpuCluster
// 作者: CppTLM Team / 日期: 2026-07-02
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §3.5
// Phase 8.A Task 6
#ifndef TLM_GPU_GPU_SOC_TLM_HH
#define TLM_GPU_GPU_SOC_TLM_HH

#include "core/sim_module.hh"
#include <cstdint>
#include <string>

namespace cpptlm::tlm {

// forward decls (cluster 层)
class GpuCluster;

}  // namespace cpptlm::tlm

namespace tlm {

// forward decls (ChStream 模块层)
class GpuMeshNoC;
class MemoryClusterTLM;
class KernelLaunchTLM;

}  // namespace tlm

namespace cpptlm::tlm {

class GpuSocTLM : public SimModule {
public:
    GpuSocTLM(const std::string& name, EventQueue* eq);
    ~GpuSocTLM() override = default;

    std::string get_module_type() const override { return "GpuSocTLM"; }

    // 子模块 setter/getter（::tlm:: 前缀引用全局 tlm 命名空间中的 ChStream 模块）
    void set_gpu_cluster(GpuCluster* cluster)    { gpu_cluster_ = cluster; }
    void set_noc(::tlm::GpuMeshNoC* noc)           { noc_ = noc; }
    void set_memory_cluster(::tlm::MemoryClusterTLM* mc) { memory_cluster_ = mc; }
    void set_kernel_launch(::tlm::KernelLaunchTLM* kl)   { kernel_launch_ = kl; }

    GpuCluster*              get_gpu_cluster()     { return gpu_cluster_; }
    ::tlm::GpuMeshNoC*         get_noc()             { return noc_; }
    ::tlm::MemoryClusterTLM*   get_memory_cluster()  { return memory_cluster_; }
    ::tlm::KernelLaunchTLM*    get_kernel_launch()   { return kernel_launch_; }

    void tick() override;

private:
    GpuCluster*              gpu_cluster_ = nullptr;
    ::tlm::GpuMeshNoC*       noc_ = nullptr;
    ::tlm::MemoryClusterTLM* memory_cluster_ = nullptr;
    ::tlm::KernelLaunchTLM*  kernel_launch_ = nullptr;
};

}  // namespace cpptlm::tlm

#endif  // TLM_GPU_GPU_SOC_TLM_HH