// src/tlm/cluster/gpu_noc_cluster.cc
#include "tlm/cluster/gpu_noc_cluster.hh"
#include "core/module_factory.hh"

namespace cpptlm::tlm {

    void GpuNoC::set_config(const nlohmann::json& params) {
        // change `cpptlm-module-param-migrate-batch2` T2.x：**声明即读取**（`ADR-X.17` D-1）。
        // ⛔ 旧写法先调 `SimModule::set_config()`（含 enforce）**再**自读 ⇒ 准入后
        //    自读声明的键会被误判为未知键（design D-2.3：enforce 必须晚于模块自读）。
        if (!params.is_object()) {
            SimObject::set_config(params);
            return;
        }
        set_config_with(params, [this]() {
            load_param_opt("mesh_size", mesh_size_);
            // ⚠️ `routing` **暂不裁决**（T2.10）：本类只是把它**透传**给内部 `RouterTLM`，
            //    而 `RouterTLM` 侧已于 C1 落**态③ 显式拒绝（伪参数）** —— 只接受 "XY"。
            //    ⇒ 本类仍按透传语义读取（⛔ 本批**不准入** `GpuNoC`：`routing` 的
            //    效果裁决归 C1 之后的独立 change，准入会让两个裁决点互相牵制）。
            load_param_opt("routing", routing_);
        });
    }

    void GpuNoC::simulate_instantiate(const nlohmann::json& cfg) {
        // 幂等守卫: 同根因 as 4 cluster fix (PR #27), 当前无活路径触发, 防御性补齐。
        // ⚠️ 判定含「上次成功」——失败路径下 internal_factory 可能部分填充 (design D-4)。
        if (already_instantiated()) {
            return;
        }
        SimModule::simulate_instantiate(cfg);
        if (!instantiate_succeeded()) {
            return;
        }
        nlohmann::json full_config = {{"modules", nlohmann::json::array()},
                                      {"connections", nlohmann::json::array()}};
        int n = mesh_size_ * mesh_size_;
        for (int i = 0; i < n; ++i) {
            full_config["modules"].push_back({{"name", "router_" + std::to_string(i)},
                                              {"type", "RouterTLM"},
                                              {"params",
                                               {{"node_x", i % mesh_size_},
                                                {"node_y", i / mesh_size_},
                                                {"mesh_x", mesh_size_},
                                                {"mesh_y", mesh_size_},
                                                {"routing", routing_}}}});
        }
        instantiate_ok_ = internal_factory->instantiateAll(full_config);
    }

} // namespace cpptlm::tlm
