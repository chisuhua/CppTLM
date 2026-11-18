// src/tlm/cluster/tpc_cluster.cc
#include "tlm/cluster/tpc_cluster.hh"
#include <stdexcept>
#include "core/module_factory.hh"
#include "utils/json_includer.hh"

namespace cpptlm::tlm {

    void TpcCluster::set_config(const nlohmann::json& params) {
        // change `cpptlm-module-param-migrate-batch2` T2.x：**声明即读取**（`ADR-X.17` D-1）。
        // ⛔ 旧写法先调 `SimModule::set_config()`（含 enforce）**再**自读 ⇒ 准入后
        //    自读声明的键会被误判为未知键（design D-2.3：enforce 必须晚于模块自读）。
        if (!params.is_object()) {
            SimObject::set_config(params);
            return;
        }
        set_config_with(params, [this]() {
            load_param_opt("tpc_id", tpc_id_);
            load_param_opt("cu_per_tpc", cu_per_tpc_);
            // `cu_template` 是**路径字符串**（实测配置一律为 `"configs/templates/..."`）
            // ⇒ D-2 态①：`load_param` 取值，文件加载归模块内（`simulate_instantiate`）。
            load_param_opt("cu_template", cu_template_path_);
        });
    }

    void TpcCluster::simulate_instantiate(const nlohmann::json& cfg) {
        // 幂等守卫: 防双激活二次生成覆盖 instances map 泄漏第一批子树 (fix-asan 根因 2)。
        // ⚠️ 判定含「上次成功」——失败路径下 internal_factory 可能部分填充 (design D-4)。
        if (already_instantiated()) {
            return;
        }
        SimModule::simulate_instantiate(cfg);
        if (!instantiate_succeeded()) {
            return;
        }
        if (cu_template_path_.empty()) {
            throw std::runtime_error("TpcCluster: cu_template must be set");
        }
        nlohmann::json compute_grp_entry;
        compute_grp_entry["name"] = "compute_grp";
        compute_grp_entry["type"] = "ComputeCluster";
        compute_grp_entry["params"] = {{"cu_template", cu_template_path_},
                                       {"cu_count", cu_per_tpc_}};
        compute_grp_entry["modules"] = nlohmann::json::array();
        compute_grp_entry["connections"] = nlohmann::json::array();
        nlohmann::json compute_grp_cfg;
        compute_grp_cfg["modules"] = nlohmann::json::array({compute_grp_entry});
        compute_grp_cfg["connections"] = nlohmann::json::array();
        instantiate_ok_ = internal_factory->instantiateAll(compute_grp_cfg);
    }

} // namespace cpptlm::tlm