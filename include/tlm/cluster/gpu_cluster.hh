// include/tlm/cluster/gpu_cluster.hh
// GpuCluster - GPU 顶层 (K GpcClusters + TCC + KernelLaunch)
// 参考: gem5 GpuCluster + APU SoC 设计 §2.2
//       docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.2.4
// 作者: Sisyphus / 日期: 2026-06-19
#ifndef TLM_CLUSTER_GPU_CLUSTER_HH
#define TLM_CLUSTER_GPU_CLUSTER_HH

#include "core/sim_module.hh"
#include <nlohmann/json.hpp>

namespace cpptlm::tlm {

class GpuCluster : public SimModule {
public:
    explicit GpuCluster(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}

    void set_config(const nlohmann::json& params);
    std::string get_module_type() const { return "GpuCluster"; }
    void simulate_instantiate(const nlohmann::json& cfg);

private:
    int gpc_count_ = 1;
    int tpc_per_gpc_ = 2;
    int cu_per_tpc_ = 2;
    std::string cu_template_path_;
};

}  // namespace cpptlm::tlm

#endif  // TLM_CLUSTER_GPU_CLUSTER_HH