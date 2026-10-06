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

#include "framework/axi4_mapper.hh"
#include "tlm/pcie/pcie_axi_adapter_tlm.hh"

#include <memory>
#include <string>

namespace tlm::pcie {

class PcieEndpointIP;

// P2 PR-B Stage B (c) per Oracle bg_c60653e0 §4(c): EP-internal composite
// (internal_factory pattern like PcieLinkPhyMux). Kills PcieAxiAdapter
// (PcieEndpointIP*) ordering problem: bridge constructed AFTER EP body
// init via lazy_init; first mmio use triggers deferred PcieAxiAdapter ctor.
class PcieAxiBridgeIP : public SimModule {
public:
    PcieAxiBridgeIP(const std::string& name, EventQueue* eq);
    ~PcieAxiBridgeIP() override = default;

    std::string get_module_type() const override { return "PcieAxiBridgeIP"; }

    void lazy_init(PcieEndpointIP* parent) noexcept;

    PcieAxiAdapter* adapter() noexcept { return adapter_.get(); }
    cpptlm::Axi4Mapper* mapper() noexcept { return mapper_.get(); }

private:
    std::unique_ptr<PcieAxiAdapter> adapter_;
    std::unique_ptr<cpptlm::Axi4Mapper> mapper_;
    PcieEndpointIP* parent_ep_ = nullptr;
    bool wired_ = false;
};

} // namespace tlm::pcie

#endif // CPPTLM_PCIE_AXI_BRIDGE_IP_H