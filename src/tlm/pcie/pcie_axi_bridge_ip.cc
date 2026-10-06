// src/tlm/pcie/pcie_axi_bridge_ip.cc
// PcieAxiBridgeIP EP-internal composite implementation (P2 PR-B Stage B (c))
#include "tlm/pcie/pcie_axi_bridge_ip.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"

namespace tlm::pcie {

PcieAxiBridgeIP::PcieAxiBridgeIP(const std::string& name, EventQueue* eq)
    : SimModule(name, eq) {}

void PcieAxiBridgeIP::lazy_init(PcieEndpointIP* parent) noexcept {
    parent_ep_ = parent;
    // Lazily construct on first use: defer PcieAxiAdapter(this, eq) until
    // mmio path actually needs it (PcieEndpointIP body fully constructed).
    // Per Oracle §3.2: this pattern kills the ctor-ordering half-construct
    // by construction-order guarantee.
    if (!adapter_) {
        adapter_ = std::make_unique<PcieAxiAdapter>(parent, event_queue);
    }
    if (!mapper_) {
        mapper_ = std::make_unique<cpptlm::Axi4Mapper>(16);
    }
    wired_ = (parent_ep_ != nullptr);
}

} // namespace tlm::pcie