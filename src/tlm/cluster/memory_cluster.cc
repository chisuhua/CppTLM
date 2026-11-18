// src/tlm/cluster/memory_cluster.cc
#include "tlm/cluster/memory_cluster.hh"
#include "core/module_factory.hh"

namespace cpptlm::tlm {

    void MemoryCluster::set_config(const nlohmann::json& params) {
        // change `cpptlm-module-param-migrate-batch2` T2.3：**声明即读取**（`ADR-X.17` D-1）。
        // ⛔ 旧写法先调 `SimModule::set_config()`（含 enforce）**再**自读 ⇒ 准入后
        //    自读声明的键会被误判为未知键（design D-2.3：enforce 必须晚于模块自读）。
        if (!params.is_object()) {
            SimObject::set_config(params);
            return;
        }
        set_config_with(params, [this]() {
            load_param_opt("channel_count", channel_count_);
            // 带单位字符串（`"1GB"`）⇒ 解析归模块内（D-2 态①）
            load_param_opt("channel_size", channel_size_);
            // ⚠️ `memory_type` **态② 显式拒绝**（`ADR-X.17` D-2）—— 内存**类型**在本仓
            //    无任何模型（无 DRAM/HBM 行为差异）⇒ 该键**零效果** ⇒ 拒绝而非静默丢弃。
            //    台账 `docs/implementation/module-param-baseline.md` B-10 登记了该缺陷。
            reject_unsupported_param(
                "memory_type",
                "this cluster has no memory-type model (no DRAM/HBM behaviour difference)");
        });
    }

    void MemoryCluster::simulate_instantiate(const nlohmann::json& cfg) {
        // 幂等守卫: 同根因 as 4 cluster fix (PR #27), 当前无活路径触发, 防御性补齐。
        // ⚠️ 判定含「上次成功」——失败路径下 internal_factory 可能部分填充 (design D-4)。
        if (already_instantiated()) {
            return;
        }
        SimModule::simulate_instantiate(cfg);
        if (!instantiate_succeeded()) {
            return;
        }
        nlohmann::json full_config = {{"modules", nlohmann::json::array()}};
        for (int i = 0; i < channel_count_; ++i) {
            full_config["modules"].push_back(
                {{"name", "channel_" + std::to_string(i)},
                 // 不再生成 type —— MemoryTLM 无内存类型模型
                 // (change cpptlm-module-param-effect-landing 改为态② 显式拒绝);
                 // size 保留: MemoryTLM 现已按带单位字符串解析 ("1GB" => 字节数)。
                 {"type", "MemoryTLM"},
                 {"params", {{"size", channel_size_}}}});
        }
        const std::string arb_type = (channel_count_ == 2) ? "ArbiterTLM2" : "ArbiterTLM4";
        full_config["modules"].push_back(
            // ⚠️ 不再生成 `ports` —— 端口数由注册名（`ArbiterTLM2` / `ArbiterTLM4`）决定,
            //    该键已于 2026-11-18 落**态②**（`ADR-X.17` D-2：端口数是编译期常量 ⇒
            //    配置写了即装配失败）⇒ 继续生成会让内部装配失败。
            {{"name", "arbiter"}, {"type", arb_type}, {"params", {}}});
        full_config["connections"] = nlohmann::json::array();
        for (int i = 0; i < channel_count_; ++i) {
            full_config["connections"].push_back({{"src", "channel_" + std::to_string(i)},
                                                  {"dst", "arbiter." + std::to_string(i)},
                                                  {"latency", 10}});
        }
        instantiate_ok_ = internal_factory->instantiateAll(full_config);
    }

} // namespace cpptlm::tlm
