// src/tlm/cluster/gpc_cluster.cc
#include "tlm/cluster/gpc_cluster.hh"
#include "core/module_factory.hh"

namespace cpptlm::tlm {

    void GpcCluster::set_config(const nlohmann::json& params) {
        SimModule::set_config(params);
        if (params.contains("gpc_id"))
            gpc_id_ = params["gpc_id"].get<int>();
        if (params.contains("tpc_per_gpc"))
            tpc_per_gpc_ = params["tpc_per_gpc"].get<int>();
        if (params.contains("cu_per_tpc"))
            cu_per_tpc_ = params["cu_per_tpc"].get<int>();
        if (params.contains("cu_template"))
            cu_template_path_ = params["cu_template"].get<std::string>();
    }

    void GpcCluster::simulate_instantiate(const nlohmann::json& cfg) {
        SimModule::simulate_instantiate(cfg);
        for (int i = 0; i < tpc_per_gpc_; ++i) {
            nlohmann::json tpc_cfg = {
                {"name", "tpc" + std::to_string(i)},
                {"type", "TpcCluster"},
                {"params",
                 {{"tpc_id", i}, {"cu_per_tpc", cu_per_tpc_}, {"cu_template", cu_template_path_}}}};
            internal_factory->instantiateAll(tpc_cfg);
        }
    }

} // namespace cpptlm::tlm