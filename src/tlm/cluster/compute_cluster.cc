// src/tlm/cluster/compute_cluster.cc
// ComputeCluster 实现 - JSON 蓝图模板 + N 份实例化
#include "tlm/cluster/compute_cluster.hh"
#include <stdexcept>
#include "core/module_factory.hh"
#include "utils/json_includer.hh"

namespace cpptlm::tlm {

    void ComputeCluster::set_config(const nlohmann::json& params) {
        // change `cpptlm-module-param-migrate-batch2` T2.4：**声明即读取**（`ADR-X.17` D-1）。
        // ⛔ 旧写法先调 `SimModule::set_config()`（含 enforce）**再**自读 ⇒ 准入后
        //    自读声明的键会被误判为未知键（design D-2.3）。
        if (!params.is_object()) {
            SimObject::set_config(params);
            return;
        }
        set_config_with(params, [this]() {
            std::string tmpl = cu_template_path_;
            if (load_param_opt("cu_template", tmpl) && !tmpl.empty()) {
                cu_template_path_ = tmpl;
            }
            int n = cu_count_;
            if (load_param_opt("cu_count", n)) {
                // 钳位语义保留（合法性裁决属模块自身，非 `load_param` 的职责）
                if (n < 1) {
                    n = 1;
                    DPRINTF(MODULE, "[WARN] ComputeCluster: cu_count < 1, clamped to 1\n");
                }
                if (n > 64) {
                    n = 64;
                    DPRINTF(MODULE, "[WARN] ComputeCluster: cu_count > 64, clamped to 64\n");
                }
                cu_count_ = n;
            }
        });
    }

    void ComputeCluster::simulate_instantiate(const nlohmann::json& cfg) {
        // 幂等守卫: 防双激活二次生成覆盖 instances map 泄漏第一批子树 (fix-asan 根因 2)。
        // ⚠️ 判定含「上次成功」——失败路径下 internal_factory 可能部分填充 (design D-4)。
        if (already_instantiated()) {
            return;
        }
        SimModule::simulate_instantiate(cfg);
        if (!instantiate_succeeded()) {
            return;
        }
        // P1 fix: 子 ComputeCluster 场景下 cu_template_path_ 为空 (默认),
        // 但父 ComputeCluster 已在 cu_entry["modules"] 提供蓝图实例, 通过基类
        // SimModule::simulate_instantiate(cfg) 已实例化 cfg.modules.
        // 此处不再构造额外 cu_cfg (避免 cu_count_ 重复实例化).
        if (cu_template_path_.empty())
            return;
        auto tmpl = JsonIncluder::loadAndInclude(cu_template_path_);
        if (!tmpl.contains("modules")) {
            throw std::runtime_error("ComputeCluster: cu_template must contain 'modules' array: " +
                                     cu_template_path_);
        }
        nlohmann::json cu_cfg;
        cu_cfg["modules"] = nlohmann::json::array();
        cu_cfg["connections"] = nlohmann::json::array();
        for (int i = 0; i < cu_count_; ++i) {
            nlohmann::json cu_entry;
            cu_entry["name"] = "cu" + std::to_string(i);
            cu_entry["type"] = "ComputeCluster";
            cu_entry["params"] = {{"cu_template", ""}, {"cu_count", 0}};
            cu_entry["modules"] = tmpl["modules"];
            cu_entry["connections"] = nlohmann::json::array();
            cu_cfg["modules"].push_back(cu_entry);
        }
        instantiate_ok_ = internal_factory->instantiateAll(cu_cfg);
    }

} // namespace cpptlm::tlm