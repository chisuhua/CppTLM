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
};

}  // namespace cpptlm::tlm

#endif  // TLM_CLUSTER_APU_SOC_HH
