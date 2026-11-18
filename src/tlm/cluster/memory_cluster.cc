// src/tlm/cluster/memory_cluster.cc
#include "tlm/cluster/memory_cluster.hh"
#include "core/module_factory.hh"

namespace cpptlm::tlm {

    void MemoryCluster::set_config(const nlohmann::json& params) {
        SimModule::set_config(params);
        if (params.contains("channel_count"))
            channel_count_ = params["channel_count"].get<int>();
        if (params.contains("channel_size"))
            channel_size_ = params["channel_size"].get<std::string>();
        // ⚠️ `memory_type` **不再读取**（`ADR-X.17` D-2 态②）—— 内存**类型**在本仓
        //    无任何模型（无 DRAM/HBM 行为差异）⇒ 该键**零效果** ⇒ 显式拒绝而非静默丢弃。
        //    台账 `docs/implementation/module-param-baseline.md` B-10 已登记该缺陷。
        if (params.contains("memory_type")) {
            throw cpptlm::ParamValidationError(
                name, "memory_type",
                "memory_type: MemoryTLM has no memory-type model (no DRAM/HBM behaviour "
                "difference) ⇒ this key has no effect (remove it from the configuration)");
        }
    }

    void MemoryCluster::simulate_instantiate(const nlohmann::json& cfg) {
        // 幂等守卫: 同根因 as 4 cluster fix (PR #27), 当前无活路径触发, 防御性补齐。
        if (!internal_factory->getAllInstances().empty()) {
            return;
        }
        SimModule::simulate_instantiate(cfg);
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
        internal_factory->instantiateAll(full_config);
    }

} // namespace cpptlm::tlm
