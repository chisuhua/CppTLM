// include/tlm/gpu/dgpu_soc.hh
// DGpuSoc - SimModule 容器, JSON 实例化 SOC 内部组件
// Per board-soc-split design §3 + ADR-SOC-07 D1/D4/D5/D6
// Owner: CppTLM Team (Sisyphus) · Date: 2026-08-31
#ifndef CPPTLM_DGPU_SOC_H
#define CPPTLM_DGPU_SOC_H

#include "core/sim_module.hh"
#include <nlohmann/json.hpp>
#include <string>

namespace cpptlm::tlm {

class DGpuSoc : public SimModule {
public:
    explicit DGpuSoc(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}
    ~DGpuSoc() override = default;

    std::string get_module_type() const override { return "DGpuSoc"; }

    // 嵌套 JSON 实例化 SOC 内部组件
    // per design §3: internal_factory->instantiateAll(config) 自动递归 StreamAdapter 注入
    void simulate_instantiate(const nlohmann::json& cfg) override;
};

} // namespace cpptlm::tlm

#endif // CPPTLM_DGPU_SOC_H