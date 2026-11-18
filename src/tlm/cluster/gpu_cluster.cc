// src/tlm/cluster/gpu_cluster.cc
#include "tlm/cluster/gpu_cluster.hh"
#include "core/module_factory.hh"

namespace cpptlm::tlm {

    void GpuCluster::set_config(const nlohmann::json& params) {
        // change `cpptlm-module-param-migrate-batch2` T2.x：**声明即读取**（`ADR-X.17` D-1）。
        // ⛔ 旧写法先调 `SimModule::set_config()`（含 enforce）**再**自读 ⇒ 准入后
        //    自读声明的键会被误判为未知键（design D-2.3：enforce 必须晚于模块自读）。
        if (!params.is_object()) {
            SimObject::set_config(params);
            return;
        }
        set_config_with(params, [this]() {
            load_param_opt("gpc_count", gpc_count_);
            load_param_opt("tpc_per_gpc", tpc_per_gpc_);
            load_param_opt("cu_per_tpc", cu_per_tpc_);
            load_param_opt("cu_template", cu_template_path_);
        });
    }

    void GpuCluster::simulate_instantiate(const nlohmann::json& cfg) {
        // 幂等守卫: 双激活 (instantiateAll Step 4.5 + SimModule 递归) 下第二次进入时
        // 直接返回, 避免二次生成覆盖 instances map 泄漏第一批子树 (fix-asan 根因 2)。
        // ⚠️ 判定含「上次成功」——失败路径下 internal_factory 可能部分填充 (design D-4)。
        if (already_instantiated()) {
            return;
        }
        SimModule::simulate_instantiate(cfg);
        if (!instantiate_succeeded()) {
            return;
        }
        nlohmann::json gpc_cfgs = nlohmann::json::array();
        for (int i = 0; i < gpc_count_; ++i) {
            nlohmann::json gpc_entry;
            gpc_entry["name"] = "gpc" + std::to_string(i);
            gpc_entry["type"] = "GpcCluster";
            gpc_entry["params"] = {{"gpc_id", i},
                                   {"tpc_per_gpc", tpc_per_gpc_},
                                   {"cu_per_tpc", cu_per_tpc_},
                                   {"cu_template", cu_template_path_}};
            gpc_entry["modules"] = nlohmann::json::array();
            gpc_entry["connections"] = nlohmann::json::array();
            gpc_cfgs.push_back(gpc_entry);
        }
        nlohmann::json gpu_cfg;
        gpu_cfg["modules"] = gpc_cfgs;
        gpu_cfg["connections"] = nlohmann::json::array();
        instantiate_ok_ = internal_factory->instantiateAll(gpu_cfg);
    }

} // namespace cpptlm::tlm