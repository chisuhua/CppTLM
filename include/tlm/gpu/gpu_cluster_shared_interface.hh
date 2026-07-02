// include/tlm/gpu/gpu_cluster_shared_interface.hh
// GpuClusterSharedInterface: apu_soc 与 gpu_soc 共享 GpuCluster 的抽象层
// 功能: 定义 GpuTopology 结构体 + GpuClusterSharedInterface 纯虚接口
// 作者: CppTLM Team / 日期: 2026-07-02
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §2
// Phase 8.A Task 5 — Oracle 修复: tick()/get_module_type() 由 SimModule 提供，避免多重继承二义性
#ifndef TLM_GPU_GPU_CLUSTER_SHARED_INTERFACE_HH
#define TLM_GPU_GPU_CLUSTER_SHARED_INTERFACE_HH

#include <cstdint>

namespace cpptlm::tlm {

/// @brief GPU 拓扑参数（apu_soc 和 gpu_soc 共享）
struct GpuTopology {
    uint32_t num_gpc = 1;               // GPC 数量
    uint32_t num_tpc_per_gpc = 1;       // 每个 GPC 的 TPC 数
    uint32_t num_sm_per_tpc = 1;        // 每个 TPC 的 CU/SM 数 (1=GB202, 2=H100/B200/GB203)
    uint32_t num_subcore_per_sm = 4;    // 每个 SM 的 sub-core 数
    uint32_t warp_size = 32;            // warp 大小 (NVIDIA: 32)
};

/// @brief GpuCluster 共享接口（apu_soc + gpu_soc 双路径）
///
/// GpuCluster 同时继承 SimModule + 本接口。
/// tick() / get_module_type() 由 SimModule 基类统一提供。
class GpuClusterSharedInterface {
public:
    virtual ~GpuClusterSharedInterface() = default;

    virtual void set_gpu_topology(const GpuTopology& topo) = 0;
    virtual GpuTopology get_gpu_topology() const = 0;
};

}  // namespace cpptlm::tlm

#endif  // TLM_GPU_GPU_CLUSTER_SHARED_INTERFACE_HH