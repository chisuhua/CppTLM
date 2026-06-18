// include/tlm/cluster/gpu_noc_cluster.hh
// GpuNoC - Garnet 风格 N×N mesh NoC (RouterTLM + LinkTLM + NICTLM)
// 参考: docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.4.3
// 作者: Sisyphus / 日期: 2026-06-19
#ifndef TLM_CLUSTER_GPU_NOC_CLUSTER_HH
#define TLM_CLUSTER_GPU_NOC_CLUSTER_HH

#include "core/sim_module.hh"
#include <nlohmann/json.hpp>

namespace cpptlm::tlm {

class GpuNoC : public SimModule {
public:
    explicit GpuNoC(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}

    void set_config(const nlohmann::json& params);
    std::string get_module_type() const { return "GpuNoC"; }
    void simulate_instantiate(const nlohmann::json& cfg);

private:
    int mesh_size_ = 2;  // NxN (2 = 4 routers, 2x2 = 4)
    std::string routing_ = "XY";
};

}  // namespace cpptlm::tlm

#endif  // TLM_CLUSTER_GPU_NOC_CLUSTER_HH
