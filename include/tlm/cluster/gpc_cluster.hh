// include/tlm/cluster/gpc_cluster.hh
// GpcCluster - Graphics Processing Cluster (M TpcClusters + 共享 Frontend)
// 参考: AMD GPU 架构 - 多个 TPC 共享 front-end
//       docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.2.3
// 作者: Sisyphus / 日期: 2026-06-19
#ifndef TLM_CLUSTER_GPC_CLUSTER_HH
#define TLM_CLUSTER_GPC_CLUSTER_HH

#include "core/sim_module.hh"
#include <nlohmann/json.hpp>

namespace cpptlm::tlm {

class GpcCluster : public SimModule {
public:
    explicit GpcCluster(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}

    void set_config(const nlohmann::json& params);
    std::string get_module_type() const { return "GpcCluster"; }
    void simulate_instantiate(const nlohmann::json& cfg);

private:
    int gpc_id_ = 0;
    int tpc_per_gpc_ = 2;
    int cu_per_tpc_ = 2;
    std::string cu_template_path_;
};

}  // namespace cpptlm::tlm

#endif  // TLM_CLUSTER_GPC_CLUSTER_HH