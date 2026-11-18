// src/tlm/cluster/cache_cluster.cc
#include "tlm/cluster/cache_cluster.hh"
#include "core/module_factory.hh"

namespace cpptlm::tlm {

    void CacheCluster::set_config(const nlohmann::json& params) {
        // change `cpptlm-module-param-migrate-batch2` T2.2：**声明即读取**（`ADR-X.17` D-1）。
        // ⛔ 旧写法先调 `SimModule::set_config()`（其中已含 enforce）**再**自读 ⇒
        //    自读声明的键在 enforce 时刻尚未登记 ⇒ 准入后会被误判为未知键。
        //    这正是 design D-2.3 裁决的「enforce 必须晚于模块自读」。
        if (!params.is_object()) {
            SimObject::set_config(params);
            return;
        }
        set_config_with(params, [this]() {
            load_param_opt("l1_count", l1_count_);
            // `l1_size` / `l2_size` 是**带单位的字符串**（`"16KB"`）⇒ 解析归模块内
            // （`ADR-X.17` D-2 态①：`load_param` 只负责取值与类型校验）。
            load_param_opt("l1_size", l1_size_);
            load_param_opt("l2_size", l2_size_);
        });
    }

    void CacheCluster::simulate_instantiate(const nlohmann::json& cfg) {
        // 幂等守卫: 同根因 as 4 cluster fix (PR #27), 当前无活路径触发, 防御性补齐。
        // ⚠️ 判定含「上次成功」——失败路径下 internal_factory 可能部分填充,
        //    仅看「已 populate」会把失败误判为已完成 (design D-4)。
        if (already_instantiated()) {
            return;
        }
        SimModule::simulate_instantiate(cfg);
        if (!instantiate_succeeded()) {
            return;
        }
        nlohmann::json full_config = {{"modules", nlohmann::json::array()}};
        full_config["modules"].push_back(
            {{"name", "l2"}, {"type", "CacheTLM"}, {"params", {{"size", l2_size_}, {"level", 2}}}});
        for (int i = 0; i < l1_count_; ++i) {
            full_config["modules"].push_back({{"name", "l1_" + std::to_string(i)},
                                              {"type", "CacheTLM"},
                                              {"params", {{"size", l1_size_}, {"level", 1}}}});
        }
        full_config["connections"] = nlohmann::json::array();
        for (int i = 0; i < l1_count_; ++i) {
            full_config["connections"].push_back(
                {{"src", "l1_" + std::to_string(i)}, {"dst", "l2"}, {"latency", 5}});
        }
        instantiate_ok_ = internal_factory->instantiateAll(full_config);
    }

} // namespace cpptlm::tlm
