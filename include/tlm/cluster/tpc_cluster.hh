// include/tlm/cluster/tpc_cluster.hh
// TpcCluster - Texture Processing Cluster (1 ComputeCluster + 共享 TextureUnit)
// 参考: AMD GPU 架构 - 2 CU 共享 texture unit
//       docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.2.2
// 作者: Sisyphus / 日期: 2026-06-19
#ifndef TLM_CLUSTER_TPC_CLUSTER_HH
#define TLM_CLUSTER_TPC_CLUSTER_HH

#include "core/sim_module.hh"
#include <nlohmann/json.hpp>

namespace cpptlm::tlm {

class TpcCluster : public SimModule {
public:
    explicit TpcCluster(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}

    void set_config(const nlohmann::json& params);
    std::string get_module_type() const { return "TpcCluster"; }

    // ── 参数观测面（`ADR-X.17` D-6 行为判别要求：两个合法值 ⇒ 可观测读数不同）
    int tpc_id() const { return tpc_id_; }
    int cu_per_tpc() const { return cu_per_tpc_; }
    const std::string& cu_template() const { return cu_template_path_; }
    void simulate_instantiate(const nlohmann::json& cfg);

private:
    int tpc_id_ = 0;
    int cu_per_tpc_ = 2;
    std::string cu_template_path_;
};

}  // namespace cpptlm::tlm

#endif  // TLM_CLUSTER_TPC_CLUSTER_HH