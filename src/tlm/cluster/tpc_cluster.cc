// src/tlm/cluster/tpc_cluster.cc
#include "tlm/cluster/tpc_cluster.hh"
#include "core/module_factory.hh"

namespace cpptlm::tlm {

    void TpcCluster::set_config(const nlohmann::json& params) {
        SimModule::set_config(params);
        if (params.contains("tpc_id"))
            tpc_id_ = params["tpc_id"].get<int>();
        if (params.contains("cu_per_tpc"))
            cu_per_tpc_ = params["cu_per_tpc"].get<int>();
        if (params.contains("cu_template"))
            cu_template_path_ = params["cu_template"].get<std::string>();
    }

    void TpcCluster::simulate_instantiate(const nlohmann::json& cfg) {
        SimModule::simulate_instantiate(cfg);
        nlohmann::json compute_grp_cfg = {
            {"name", "compute_grp"},
            {"type", "ComputeCluster"},
            {"params", {{"cu_template", cu_template_path_}, {"cu_count", cu_per_tpc_}}}};
        internal_factory->instantiateAll(compute_grp_cfg);
    }

} // namespace cpptlm::tlm