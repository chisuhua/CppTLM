// include/tlm/cluster/gpu_internal_fabric.hh
// P3 GpuInternalFabric: SimModule wrapper around crossbar_tlm/router_tlm
//
// Per Oracle bg_c60653e0 §3.4 + Metis bg_4051aad9:
//   - extends SimModule (composite, NOT ChStreamModuleBase — inner is ChStream)
//   - inner crossbar must be instantiated via ModuleFactory's object path
//     (JSON child modules), NEVER raw `new` — otherwise Step 7 adapter
//     injection silently skips (commit-time hooks dead-port scenario)
//   - routing table configurable via JSON params (no hardcoded routing)
//
// PR-A scope: class definition + facade getters. Wire-up of inner
// crossbar deferred to PR-B (or follow-up) when JSON child-module path
// is exercised end-to-end.
#ifndef CPPTLM_GPU_INTERNAL_FABRIC_H
#define CPPTLM_GPU_INTERNAL_FABRIC_H

#include "core/sim_module.hh"

#include <string>

namespace tlm::cluster {

class CrossbarTLM;

class GpuInternalFabric : public SimModule {
public:
    GpuInternalFabric(const std::string& name, EventQueue* eq);
    ~GpuInternalFabric() override = default;

    std::string get_module_type() const override {
        return "GpuInternalFabric";
    }

    // Inner crossbar reference (non-owning; ModuleFactory owns lifetime
    // when inner is instantiated via JSON child-modules path per Oracle
    // bg_c60653e0 §3.4).
    CrossbarTLM* crossbar() const noexcept { return crossbar_; }

    // Wire-up helper: attach inner crossbar. PT-PR-C will route this via
    // attach_composition() reading JSON children modules.
    void attach_crossbar(CrossbarTLM* inner) noexcept { crossbar_ = inner; }

    // Routing port count (default 4 matches CrossbarTLM::NUM_PORTS).
    void set_num_ports(unsigned n) noexcept { num_ports_ = n; }
    unsigned num_ports() const noexcept { return num_ports_; }

private:
    CrossbarTLM* crossbar_ = nullptr;  // non-owning (ModuleFactory owns)
    unsigned num_ports_ = 4;
};

} // namespace tlm::cluster

#endif // CPPTLM_GPU_INTERNAL_FABRIC_H