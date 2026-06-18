// src/tlm/cluster/apu_soc.cc
// ApuSoC 实现 - 顶层容器加载 cpu_topology / gpu_topology 模板 JSON 并 instantiate
// 通过 SimModule::incorporate_parent late-binding 钩子保留跨域 wiring 扩展点。
// 参考: docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.5.1
// 作者: Sisyphus / 日期: 2026-06-19
#include "tlm/cluster/apu_soc.hh"
#include "core/module_factory.hh"
#include "utils/json_includer.hh"
#include <string>

namespace cpptlm::tlm {

    void ApuSoC::set_config(const nlohmann::json& params) {
        SimModule::set_config(params);
        if (params.contains("cpu_topology")) {
            cpu_topology_ = params["cpu_topology"].get<std::string>();
        }
        if (params.contains("gpu_topology")) {
            gpu_topology_ = params["gpu_topology"].get<std::string>();
        }
    }

    static nlohmann::json wrap_template_as_module(const nlohmann::json& tmpl,
                                                  const std::string& name,
                                                  const std::string& type) {
        nlohmann::json entry;
        entry["name"] = name;
        entry["type"] = type;
        entry["params"] = nlohmann::json::object();
        if (tmpl.contains("modules"))
            entry["modules"] = tmpl["modules"];
        else
            entry["modules"] = nlohmann::json::array();
        if (tmpl.contains("connections"))
            entry["connections"] = tmpl["connections"];
        else
            entry["connections"] = nlohmann::json::array();
        return entry;
    }

    void ApuSoC::simulate_instantiate(const nlohmann::json& cfg) {
        if (internal_factory && !internal_factory->getAllInstances().empty()) {
            return;
        }

        nlohmann::json wrap;
        wrap["modules"] = nlohmann::json::array();
        wrap["connections"] = nlohmann::json::array();

        if (cfg.contains("modules")) {
            for (const auto& m : cfg["modules"]) {
                wrap["modules"].push_back(m);
            }
        }
        if (!cpu_topology_.empty()) {
            auto tmpl = JsonIncluder::loadAndInclude(cpu_topology_);
            wrap["modules"].push_back(wrap_template_as_module(tmpl, "cpu", "CpuCluster"));
        }
        if (!gpu_topology_.empty()) {
            auto tmpl = JsonIncluder::loadAndInclude(gpu_topology_);
            wrap["modules"].push_back(wrap_template_as_module(tmpl, "gpu", "GpuCluster"));
        }

        internal_factory->instantiateAll(wrap);

        if (cfg.contains("modules")) {
            for (auto& child_cfg : cfg["modules"]) {
                if (!child_cfg.contains("name"))
                    continue;
                auto* child = internal_factory->getInstance(child_cfg["name"]);
                if (auto* sub = dynamic_cast<SimModule*>(child)) {
                    sub->simulate_instantiate(child_cfg);
                }
            }
        }
    }

    void ApuSoC::incorporate_parent(SimModule* parent) {
        SimModule::incorporate_parent(parent);
    }

} // namespace cpptlm::tlm
