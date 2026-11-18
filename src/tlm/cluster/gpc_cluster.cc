// src/tlm/cluster/gpc_cluster.cc
#include "tlm/cluster/gpc_cluster.hh"
#include "core/module_factory.hh"

namespace cpptlm::tlm {

    void GpcCluster::set_config(const nlohmann::json& params) {
        // change `cpptlm-module-param-migrate-batch2` T2.x：**声明即读取**（`ADR-X.17` D-1）。
        // ⛔ 旧写法先调 `SimModule::set_config()`（含 enforce）**再**自读 ⇒ 准入后
        //    自读声明的键会被误判为未知键（design D-2.3：enforce 必须晚于模块自读）。
        if (!params.is_object()) {
            SimObject::set_config(params);
            return;
        }
        set_config_with(params, [this]() {
            load_param_opt("gpc_id", gpc_id_);
            load_param_opt("tpc_per_gpc", tpc_per_gpc_);
            load_param_opt("cu_per_tpc", cu_per_tpc_);
            load_param_opt("cu_template", cu_template_path_);
        });
    }

    void GpcCluster::simulate_instantiate(const nlohmann::json& cfg) {
        // 幂等守卫: 防双激活二次生成覆盖 instances map 泄漏第一批子树 (fix-asan 根因 2)。
        // ⚠️ 判定含「上次成功」——失败路径下 internal_factory 可能部分填充 (design D-4)。
        if (already_instantiated()) {
            return;
        }
        SimModule::simulate_instantiate(cfg);
        if (!instantiate_succeeded()) {
            return;
        }
        nlohmann::json tpc_cfgs = nlohmann::json::array();
        for (int i = 0; i < tpc_per_gpc_; ++i) {
            nlohmann::json tpc_entry;
            tpc_entry["name"] = "tpc" + std::to_string(i);
            tpc_entry["type"] = "TpcCluster";
            tpc_entry["params"] = {
                {"tpc_id", i}, {"cu_per_tpc", cu_per_tpc_}, {"cu_template", cu_template_path_}};
            tpc_entry["modules"] = nlohmann::json::array();
            tpc_entry["connections"] = nlohmann::json::array();
            tpc_cfgs.push_back(tpc_entry);
        }
        nlohmann::json gpc_cfg;
        gpc_cfg["modules"] = tpc_cfgs;
        gpc_cfg["connections"] = nlohmann::json::array();
        instantiate_ok_ = internal_factory->instantiateAll(gpc_cfg);
    }

} // namespace cpptlm::tlm