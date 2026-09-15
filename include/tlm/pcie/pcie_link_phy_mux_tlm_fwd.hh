// include/tlm/pcie/pcie_link_phy_mux_tlm_fwd.hh
// 轻量级 forward-declaration shim (避免循环 include):
//   - pcie_link_phy_mux_tlm.{hh,cc} 含 PciePhyDigitalCtrl/PcieLinkLayer/PcieBypassMux 全定义
//   - pcie_phy_digital_ctrl_tlm.cc / pcie_bypass_mux.cc / pcie_link_layer_tlm.cc 调用
//     composite 成员时若 include 复合 header 会触发循环 (复合 header 含这 3 个 header).
//   - 此 shim 声明 3 个 free function 分别返回 PciePhyDigitalCtrl*/PcieBypassMux*/
//     PcieLinkLayer*, 调用者无需知道 composite 类型, 仅需对应子模块 header 即可。
//   - 实现见 pcie_link_phy_mux_tlm.cc (那里有 composite 完整定义).
#ifndef TLM_PCIE_PCIE_LINK_PHY_MUX_TLM_FWD_HH
#define TLM_PCIE_PCIE_LINK_PHY_MUX_TLM_FWD_HH

#include <string>

namespace tlm::pcie {

class PciePhyDigitalCtrl;
class PcieBypassMux;
class PcieLinkLayer;

PciePhyDigitalCtrl* lpm_phy_for_endpoint(const std::string& ep_name) noexcept;
PcieBypassMux* lpm_mux_for_endpoint(const std::string& ep_name) noexcept;
PcieLinkLayer* lpm_ll_for_endpoint(const std::string& ep_name) noexcept;

}  // namespace tlm::pcie

#endif  // TLM_PCIE_PCIE_LINK_PHY_MUX_TLM_FWD_HH
