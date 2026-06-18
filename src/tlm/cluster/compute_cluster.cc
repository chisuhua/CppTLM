// src/tlm/cluster/compute_cluster.cc
// ComputeCluster 实现 - JSON 蓝图模板 + N 份实例化
#include "tlm/cluster/compute_cluster.hh"
#include "core/module_factory.hh"
#include "utils/json_includer.hh"
#include <stdexcept>

namespace cpptlm::tlm {

    void ComputeCluster::set_config(const nlohmann::json& params) {
        SimModule::set_config(params);
        if (params.contains("cu_template")) {
            cu_template_path_ = params["cu_template"].get<std::string>();
        }
        if (params.contains("cu_count")) {
            cu_count_ = params["cu_count"].get<int>();
            if (cu_count_ < 1) {
                cu_count_ = 1;
                DPRINTF(MODULE, "[WARN] ComputeCluster: cu_count < 1, clamped to 1\n");
            }
            if (cu_count_ > 64) {
                cu_count_ = 64;
                DPRINTF(MODULE, "[WARN] ComputeCluster: cu_count > 64, clamped to 64\n");
            }
        }
    }

    void ComputeCluster::simulate_instantiate(const nlohmann::json& cfg) {
        SimModule::simulate_instantiate(cfg);
        if (cu_template_path_.empty())
            return;
        auto tmpl = JsonIncluder::loadAndInclude(cu_template_path_);
        if (!tmpl.contains("modules")) {
            throw std::runtime_error("ComputeCluster: cu_template must contain 'modules' array: " +
                                     cu_template_path_);
        }
        for (int i = 0; i < cu_count_; ++i) {
            auto cu = tmpl;
            cu["name"] = "cu" + std::to_string(i);
            internal_factory->instantiateAll(cu);
        }
    }

} // namespace cpptlm::tlm