// include/tlm/pcie/pcie_controller_ip.hh
// PcieControllerIP facade over PcieEndpointIP (Phase P2 PR-A)
// Per docs/superpowers/plans/2026-10-06-dgpu-realip-refactor.md Task 13-14.
//
// Per H2 (Metis bg_899dfdcc + Oracle bg_fb84a648 review):
//   - Consumes existing PcieEndpointIP::bar_router() (NOT reimplement)
//   - PcieEndpointIP already owns PcieBarRouter + bar_sizes_ + resizable_bar
//     + bar_store_; PcieControllerIP is a thin facade for routing decisions.
//
// Author: Architecture WG / Date: 2026-10-06
#ifndef CPPTLM_PCIE_CONTROLLER_IP_H
#define CPPTLM_PCIE_CONTROLLER_IP_H

#include "core/sim_module.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"

#include <cstdint>

namespace tlm::pcie {

// B1 PR-B Phase Gate: composition refactor (per Oracle bg_9c11d0e8).
// Was: `class PcieControllerIP : public PcieEndpointIP` (inheritance — wrong: controller
//      "is-an" endpoint is a category error per controller IP wire spec; controllers
//      own endpoints, never derive).
// Now: `class PcieControllerIP : public SimModule` + non-owning PcieEndpointIP* ep_
//      + attach_endpoint() setter. Consumer (`DGpuBoard::set_pcie_controller_ip`)
//      owns lifecycle; controller IP is a facade over the EP.
//
// Non-owning rationale: PcieEndpointIP lifecycle is managed by DGpuBoard /
// ModuleFactory (composite ownership pattern). The controller IP is a stateless
// facade; resetting/replacing EP does not require recreating controller.
class PcieControllerIP : public ::SimModule {
public:
    PcieControllerIP(const std::string& name, EventQueue* eq);

    // B1 MUST-DO #1 (per session handoff §6 B1a): attach endpoint.
    // Without this, `ep_` stays nullptr and `bar_size(1)` collapses to 0,
    // making stimulus 2 (Bar1FbFastPath) always return Unknown — dormant-gate
    // regression trap (per Metis hidden req #4 / Oracle bg_9c11d0e8).
    void attach_endpoint(PcieEndpointIP* ep) noexcept { ep_ = ep; }

    // Accessor for diagnostics / unit tests.
    [[nodiscard]] PcieEndpointIP* endpoint() const noexcept { return ep_; }

    // Per Oracle bg_c60653e0 §2.1: BAR0 0x100 GMMU/Display sub-dispatch REMOVED.
    // Real PCIe EP controllers do NOT sub-split a BAR by offset — controller
    // forwards the whole BAR0 window to internal bus; decode happens on
    // target side (GMMU block, display block each claim their own range).
    // Sub-dispatch belongs in target-side / GpuInternalFabric (P3), not here.
    // Per Oracle on-card: BAR3 is upper dword of BAR2 64-bit pair — never matches.
    enum class BarTarget : uint8_t {
        Unknown          = 0,
        Bar0Mmio         = 1,   // BAR0 — whole 4KB aperture (target-side decodes)
        Bar1FbFastPath   = 2,   // BAR1 offset < bar_size(1) (framebuffer window)
        Bar2VramAperture = 3,   // BAR2 (any offset) → PcieMemoryDevice
    };

    // Match (bar, offset) → BarTarget + sub_offset (within-BAR offset)
    // Reads sizes dynamically from PcieEndpointIP::bar_sizes_ (no static snapshot).
    // ResizableBar changes are reflected on next call (per Metis hidden req #2).
    bool match_bar(uint8_t bar, uint64_t offset,
                 BarTarget& out_target, uint64_t& out_sub_offset) noexcept;

    std::string get_module_type() const override { return "PcieControllerIP"; }

private:
    // B1 PR-B Phase Gate: composition (non-owning).
    // Set via attach_endpoint(). nullptr-safe: match_bar() falls back to 0 if not set,
    // which makes Bar1FbFastPath impossible — explicit guard against silent misuse.
    PcieEndpointIP* ep_ = nullptr;
};

} // namespace tlm::pcie

#endif // CPPTLM_PCIE_CONTROLLER_IP_H