// src/tlm/cluster/gpu_cluster.cc
#include "tlm/cluster/gpu_cluster.hh"
#include "core/module_factory.hh"

namespace cpptlm::tlm {

    void GpuCluster::set_config(const nlohmann::json& params) {
        SimModule::set_config(params);
        if (params.contains("gpc_count"))
            gpc_count_ = params["gpc_count"].get<int>();
        if (params.contains("tpc_per_gpc"))
            tpc_per_gpc_ = params["tpc_per_gpc"].get<int>();
        if (params.contains("cu_per_tpc"))
            cu_per_tpc_ = params["cu_per_tpc"].get<int>();
        if (params.contains("cu_template"))
            cu_template_path_ = params["cu_template"].get<std::string>();
    }

    void GpuCluster::simulate_instantiate(const nlohmann::json& cfg) {
        SimModule::simulate_instantiate(cfg);
        for (int i = 0; i < gpc_count_; ++i) {
            nlohmann::json gpc_cfg = {{"name", "gpc" + std::to_string(i)},
                                      {"type", "GpcCluster"},
                                      {"params",
                                       {{"gpc_id", i},
                                        {"tpc_per_gpc", tpc_per_gpc_},
                                        {"cu_per_tpc", cu_per_tpc_},
                                        {"cu_template", cu_template_path_}}}};
            internal_factory->instantiateAll(gpc_cfg);
        }
    }

} // namespace cpptlm::tlm