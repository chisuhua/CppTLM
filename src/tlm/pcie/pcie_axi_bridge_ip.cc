// src/tlm/pcie/pcie_axi_bridge_ip.cc
// PcieAxiBridgeIP facade implementation (P2 PR-A Task 15)
//
// Per H1 (Oracle bg_fb84a648): extends SimModule.
//
// PR-A SIMPLIFIED: only facade ctor (composition deferred to PR-B to avoid
// PcieAxiAdapter ctor dependency on parent PcieEndpointIP*).
#include "tlm/pcie/pcie_axi_bridge_ip.hh"

namespace tlm::pcie {

PcieAxiBridgeIP::PcieAxiBridgeIP(const std::string& name, EventQueue* eq)
    : SimModule(name, eq) {}

} // namespace tlm::pcie
