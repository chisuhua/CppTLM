// include/tlm/gpu/gpu_soc_tlm.hh
// GpuSocTLM: gpu_soc 顶层容器 — 组合 GpuCluster + GpuMeshNoC + MemoryClusterTLM + IComputeDevice
// 功能: GPU 芯片顶层仿真容器，通过 GpuClusterSharedInterface 与 apu_soc 共享 GpuCluster
// 作者: CppTLM Team / 日期: 2026-07-02 (修订 2027-02-09 Task 11)
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §3.5
//       docs/soc_arch/architecture/15-sm-microarchitecture-design.md §15.5 (SM 重构)
#ifndef TLM_GPU_GPU_SOC_TLM_HH
#define TLM_GPU_GPU_SOC_TLM_HH

#include "core/sim_module.hh"
#include "tlm/gpu/i_compute_device.hh"
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
    // Task 11: KernelLaunchTLM 替换为 IComputeDevice (SM 重构后, 计算设备接口统一)
    void set_compute_device(cpptlm::gpu::IComputeDevice* dev) { compute_dev_ = dev; }

    GpuCluster*                 get_gpu_cluster()     { return gpu_cluster_; }
    ::tlm::GpuMeshNoC*          get_noc()             { return noc_; }
    ::tlm::MemoryClusterTLM*    get_memory_cluster()  { return memory_cluster_; }
    cpptlm::gpu::IComputeDevice* get_compute_device() { return compute_dev_; }

    void tick() override;

private:
    GpuCluster*                 gpu_cluster_ = nullptr;
    ::tlm::GpuMeshNoC*          noc_ = nullptr;
    ::tlm::MemoryClusterTLM*    memory_cluster_ = nullptr;
    cpptlm::gpu::IComputeDevice* compute_dev_ = nullptr;
};

}  // namespace cpptlm::tlm

#endif  // TLM_GPU_GPU_SOC_TLM_HH