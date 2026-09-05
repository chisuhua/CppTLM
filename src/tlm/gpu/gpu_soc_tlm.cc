// src/tlm/gpu/gpu_soc_tlm.cc
// GpuSocTLM 实现: 递归推进所有子模块
// 作者: CppTLM Team / 日期: 2026-07-02 (修订 2027-02-09 Task 11)
// Phase 8.A Task 6
#include "tlm/gpu/gpu_soc_tlm.hh"
#include "tlm/cluster/gpu_cluster.hh"
#include "tlm/gpu/gpu_mesh_noc_tlm.hh"
#include "tlm/gpu/memory_cluster_tlm.hh"

namespace cpptlm::tlm {

    GpuSocTLM::GpuSocTLM(const std::string& name, EventQueue* eq) : SimModule(name, eq) {
    }

    void GpuSocTLM::tick() {
        // 递归推进所有子模块
        if (gpu_cluster_)
            gpu_cluster_->tick();
        if (noc_)
            noc_->tick();
        if (memory_cluster_)
            memory_cluster_->tick();
        // Task 11: IComputeDevice (代替 KernelLaunchTLM). 驱动 SM 端 exe_once()
        // (one-step PTX-EMU 步进语义, 每 tick 推进一帧指令)
        if (compute_dev_)
            compute_dev_->exe_once();
    }

} // namespace cpptlm::tlm