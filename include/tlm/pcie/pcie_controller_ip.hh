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

#include "tlm/pcie/pcie_endpoint_ip.hh"

#include <cstdint>

namespace tlm::pcie {

class PcieControllerIP : public PcieEndpointIP {
public:
    PcieControllerIP(const std::string& name, EventQueue* eq);

    // Per H3: explicit doorbell carve-out (synthetic 0x10010000 outside BAR1 16MB)
    // Per T3: BAR0Mmio sub-dispatch (offset<0x100 → GMMU, else Display)
    // BAR3 is upper dword of BAR2 64-bit pair — never matches
    enum class BarTarget : uint8_t {
        Unknown          = 0,
        Bar0GmmuReg      = 1,   // BAR0 offset < 0x100 (GMMU register space)
        Bar0DisplayMmio  = 2,   // BAR0 offset >= 0x100 (display device MMIO)
        Bar1FbFastPath   = 3,   // BAR1 offset < bar1_window_size_ (framebuffer window)
        Bar2VramAperture = 4,   // BAR2 (any offset) → PcieMemoryDevice
    };

    // Match (bar, offset) → BarTarget + sub_offset (within-BAR offset)
    // Reads sizes dynamically from PcieEndpointIP::bar_sizes_ (no static snapshot).
    // ResizableBar changes are reflected on next call (per Metis hidden req #2).
    bool match_bar(uint8_t bar, uint64_t offset,
                 BarTarget& out_target, uint64_t& out_sub_offset) const;

    std::string get_module_type() const override { return "PcieControllerIP"; }
};

} // namespace tlm::pcie

#endif // CPPTLM_PCIE_CONTROLLER_IP_H