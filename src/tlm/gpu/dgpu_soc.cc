// src/tlm/gpu/dgpu_soc.cc
// Per board-soc-split design §3: SimModule::simulate_instantiate 默认递归内部组件
// 通过 internal_factory->instantiateAll(cfg) 触发 Step 7 StreamAdapter 注入
// 不修改 sim_module.hh / module_factory.cc (per design §3 Q1 裁决)
#include "tlm/gpu/dgpu_soc.hh"
#include "core/module_factory.hh"

namespace cpptlm::tlm {

void DGpuSoc::simulate_instantiate(const nlohmann::json& cfg) {
    // 嵌套实例化: SimModule 默认实现已递归 (参见 compute_cluster.cc:28-53 先例)
    SimModule::simulate_instantiate(cfg);
}

} // namespace cpptlm::tlm