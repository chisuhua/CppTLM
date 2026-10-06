// include/tlm/pcie/pcie_axi_bridge_ip.hh
// PcieAxiBridgeIP facade (P2 PR-A Task 15)
//
// Per H1 (Oracle bg_fb84a648): MUST extend SimModule (not SimObject) for
// REGISTER_MODULE compatibility. Plan v1.1 had this wrong; corrected.
//
// Facade wrapping PcieAxiAdapterTLM + AXI4Mapper pair. OOO rid state stays
// in AXI4Mapper per Task 15 design.
//
// Per T2: dgpusoc_v2_realip JSON MUST NOT contain "axi_adapter" key in
// controller params — would double-attach via attach_composition auto-flow.

#ifndef CPPTLM_PCIE_AXI_BRIDGE_IP_H
#define CPPTLM_PCIE_AXI_BRIDGE_IP_H

#include "core/sim_module.hh"

#include <string>

namespace tlm::pcie {

class PcieAxiBridgeIP : public SimModule {
public:
    PcieAxiBridgeIP(const std::string& name, EventQueue* eq);
    ~PcieAxiBridgeIP() override = default;

    std::string get_module_type() const override { return "PcieAxiBridgeIP"; }

    // PR-B will add: wire_up(PcieEndpointIP*) + member PcieAxiAdapter + AXI4Mapper
};

} // namespace tlm::pcie

#endif // CPPTLM_PCIE_AXI_BRIDGE_IP_H