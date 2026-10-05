// include/tlm/gpu/dgpu_soc.hh
// DGpuSoc - SimModule 容器, JSON 实例化 SOC 内部组件
// Per board-soc-split design §3 + ADR-SOC-07 D1/D4/D5/D6
// D3a-2: 持有 PcieDisplayDevice value 对象 (方案 B, design.md §3.3)
//   - PcieDisplayDevice 是 pure class (非 SimObject), 无法经 getInternalInstance 查找
//   - 由 DGpuSoc 直接持有 + display_device() accessor 暴露
//   - DGpuBoard::bind_display_backings 经此 accessor 注入 backing + EP 注册
// Owner: CppTLM Team (Sisyphus) · Date: 2026-08-31 (D3a-2: 2026-10-04)
#ifndef CPPTLM_DGPU_SOC_H
#define CPPTLM_DGPU_SOC_H

#include "core/sim_module.hh"
#include "tlm/gpu/pcie_display_device.hh"
#include <nlohmann/json.hpp>
#include <string>

namespace cpptlm::tlm {

class DGpuSoc : public SimModule {
public:
    explicit DGpuSoc(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}
    ~DGpuSoc() override = default;

    std::string get_module_type() const override { return "DGpuSoc"; }

    // D3a-2: PcieDisplayDevice value 对象 accessor (non-owning, 生命周期归 DGpuSoc)
    ::tlm::gpu::PcieDisplayDevice& display_device() noexcept { return display_device_; }
    const ::tlm::gpu::PcieDisplayDevice& display_device() const noexcept {
        return display_device_;
    }

    // 嵌套 JSON 实例化 SOC 内部组件
    // per design §3: internal_factory->instantiateAll(config) 自动递归 StreamAdapter 注入
    void simulate_instantiate(const nlohmann::json& cfg) override;

private:
    // D3a-2: PcieDisplayDevice 唯一实例 (value 对象, 无 backing; backing 由 DGpuBoard 注入)
    ::tlm::gpu::PcieDisplayDevice display_device_;
};

} // namespace cpptlm::tlm

#endif // CPPTLM_DGPU_SOC_H