// src/tlm/cluster/apu_soc.cc
// ApuSoC 实现 - 顶层容器加载 cpu_topology / gpu_topology 模板 JSON 并 instantiate
// 通过 SimModule::incorporate_parent late-binding 钩子保留跨域 wiring 扩展点。
// 参考: docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.5.1
// 作者: Sisyphus / 日期: 2026-06-19
#include "tlm/cluster/apu_soc.hh"
#include <string>
#include "core/module_factory.hh"
#include "tlm/cache_tlm.hh"         // P1: collectAndRegisterPeerCaches needs CacheTLM
#include "tlm/coherent_xbar_tlm.hh" // P1: incorporate_parent wiring needs CoherentXBarTLM
#include "utils/json_includer.hh"

namespace cpptlm::tlm {

    void ApuSoC::set_config(const nlohmann::json& params) {
        // change `cpptlm-module-param-migrate-batch2` T2.x：**声明即读取**（`ADR-X.17` D-1）。
        // ⛔ 旧写法先调 `SimModule::set_config()`（含 enforce）**再**自读 ⇒ 准入后
        //    自读声明的键会被误判为未知键（design D-2.3：enforce 必须晚于模块自读）。
        if (!params.is_object()) {
            SimObject::set_config(params);
            return;
        }
        set_config_with(params, [this]() {
            // `cpu_topology` / `gpu_topology` 是**蓝图路径字符串**（design 记为「嵌套块」，
            // 实测配置一律为路径 —— 处置按实测：D-2 态① 取值 + 模块内加载）。
            load_param_opt("cpu_topology", cpu_topology_);
            load_param_opt("gpu_topology", gpu_topology_);
            // P1: 可选 coherent_xbar_name (默认 "xbar", 已初始化)
            load_param_opt("coherent_xbar_name", coherent_xbar_name_);
        });
    }

    static nlohmann::json wrap_template_as_module(const nlohmann::json& tmpl,
                                                  const std::string& name,
                                                  const std::string& type) {
        nlohmann::json entry;
        entry["name"] = name;
        entry["type"] = type;
        if (tmpl.contains("modules") && tmpl["modules"].is_array() && !tmpl["modules"].empty()) {
            entry["params"] = tmpl["modules"][0].value("params", nlohmann::json::object());
            entry["modules"] = tmpl["modules"][0].value("modules", nlohmann::json::array());
            entry["connections"] = tmpl["modules"][0].value("connections", nlohmann::json::array());
        } else {
            entry["params"] = nlohmann::json::object();
            entry["modules"] = nlohmann::json::array();
            entry["connections"] = nlohmann::json::array();
        }
        return entry;
    }

    void ApuSoC::simulate_instantiate(const nlohmann::json& cfg) {
        // ⚠️ 判定含「上次成功」——失败路径下 internal_factory 可能部分填充 (design D-4)。
        if (already_instantiated()) {
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

        bool ok = internal_factory->instantiateAll(wrap);

        if (cfg.contains("modules")) {
            for (auto& child_cfg : cfg["modules"]) {
                if (!child_cfg.contains("name"))
                    continue;
                auto* child = internal_factory->getInstance(child_cfg["name"]);
                if (auto* sub = dynamic_cast<SimModule*>(child)) {
                    sub->simulate_instantiate(child_cfg);
                    if (!sub->instantiate_succeeded()) {
                        ok = false;
                    }
                }
            }
        }
        instantiate_ok_ = ok;
    }

    void ApuSoC::incorporate_parent(SimModule* /*parent*/) {
        // P1 幂等性: 多次调用早退
        if (peer_caches_wired_)
            return;
        peer_caches_wired_ = true;

        // 1. 找 xbar (命名可配置, 默认 "xbar")
        auto* xbar_obj = getInternalInstance(coherent_xbar_name_);
        auto* xbar = dynamic_cast<CoherentXBarTLM*>(xbar_obj);
        if (!xbar) {
            DPRINTF(MODULE, "[ApuSoC] no CoherentXBarTLM '%s' found, skip peer wiring\n",
                    coherent_xbar_name_.c_str());
            return; // 软失败: 无 xbar 是合法拓扑 (单元测试场景)
        }

        // 2. 递归遍历整棵子树, 注册所有 CacheTLM peer
        collectAndRegisterPeerCaches(xbar, this, /*path_prefix=*/"");

        // 3. 递归通知子 SimModule (保留 hook 语义供未来扩展)
        SimModule::incorporate_parent(this);
    }
    void ApuSoC::collectAndRegisterPeerCaches(CoherentXBarTLM* xbar, SimModule* subtree_root,
                                              const std::string& path_prefix) {
        for (const auto& [name, obj_ptr] : subtree_root->getInternalFactory().getAllInstances()) {
            if (!obj_ptr)
                continue;
            std::string full_name = path_prefix.empty() ? name : path_prefix + "." + name;

            // 命中 CacheTLM: 取 D.1 修复后的 req_out 并注册
            if (auto* cache = dynamic_cast<CacheTLM*>(obj_ptr.get())) {
                if (!cache->hasPortManager())
                    continue;
                auto* req_out =
                    dynamic_cast<MasterPort*>(cache->getPortManager().getDownstreamPort("req_out"));
                if (req_out) {
                    xbar->registerPeerCache(full_name, req_out); // 内部按名去重
                } else {
                    DPRINTF(MODULE, "[ApuSoC] cache '%s' has no req_out port, skip\n",
                            full_name.c_str());
                }
            }

            // 命中 SimModule: 递归下钻 (CpuCluster/GpuCluster/GpcCluster/...)
            if (auto* sub = dynamic_cast<SimModule*>(obj_ptr.get())) {
                collectAndRegisterPeerCaches(xbar, sub, full_name);
            }
        }
    }

} // namespace cpptlm::tlm
