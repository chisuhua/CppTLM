// include/tlm/cluster/apu_soc.hh
// ApuSoC - 顶层 APU SoC 容器 (CPU + GPU + Memory + 跨域 CoherentXBar)
// 引用 cpu_topology + gpu_topology JSON 模板, 配合 SimModule::incorporate_parent
// late-binding 钩子实现跨域 wiring。
// 参考: docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.5.1
// 作者: Sisyphus / 日期: 2026-06-19
#ifndef TLM_CLUSTER_APU_SOC_HH
#define TLM_CLUSTER_APU_SOC_HH

#include "core/sim_module.hh"
#include <nlohmann/json.hpp>
#include <string>

namespace cpptlm::tlm {

class ApuSoC : public SimModule {
public:
    explicit ApuSoC(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}

    void set_config(const nlohmann::json& params);
    std::string get_module_type() const { return "ApuSoC"; }
    void simulate_instantiate(const nlohmann::json& cfg);
    void incorporate_parent(SimModule* parent);

private:
    std::string cpu_topology_;
    std::string gpu_topology_;
    // P1: coherent_xbar_name 配置 (params 可覆盖, 默认 "xbar")
    std::string coherent_xbar_name_ = "xbar";
    // P1: 幂等守卫 - 防止多次 incorporate_parent 重复注册
    bool peer_caches_wired_ = false;

    // P1: 全树递归收集 CacheTLM peer cache 并注册到 xbar
    // path_prefix 形如 "cpu" 或 "gpu.gpc0.tpc0" (递归累积)
    void collectAndRegisterPeerCaches(class CoherentXBarTLM* xbar,
                                      SimModule* subtree_root,
                                      const std::string& path_prefix);
};

}  // namespace cpptlm::tlm

#endif  // TLM_CLUSTER_APU_SOC_HH
