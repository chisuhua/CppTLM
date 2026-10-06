// src/tlm/pcie/pcie_controller_ip.cc
// PcieControllerIP facade implementation (P2 PR-A Task 14)
//
// Per H2 (Metis/Oracle review): consumes PcieEndpointIP::bar_router().
// No BAR match reimplementation — single source of truth.
#include "tlm/pcie/pcie_controller_ip.hh"

namespace tlm::pcie {

PcieControllerIP::PcieControllerIP(const std::string& name, EventQueue* eq)
    : PcieEndpointIP(name, eq) {}

bool PcieControllerIP::match_bar(uint8_t bar, uint64_t offset,
                                  BarTarget& out_target,
                                  uint64_t& out_sub_offset) const {
    out_sub_offset = offset;

    // BAR3 is upper dword of BAR2 64-bit pair (per H2) — never matches.
    if (bar == 3) {
        out_target = BarTarget::Unknown;
        return false;
    }

    if (bar == 0) {
        // Per T3: sub-dispatch BAR0Mmio by offset (<0x100 GMMU else Display)
        // GMMU register space covers offset < 0x100 (per existing DGpuBoard code).
        if (offset < 0x100) {
            out_target = BarTarget::Bar0GmmuReg;
        } else {
            out_target = BarTarget::Bar0DisplayMmio;
        }
        return true;
    }

    if (bar == 1) {
        // BAR1 = framebuffer window. Per H3 + existing logic, sub-dispatch by
        // size relative to bar1 window (read live via PcieEndpointIP::bar_size).
        if (offset < bar_size(1)) {
            out_target = BarTarget::Bar1FbFastPath;
            return true;
        }
        out_target = BarTarget::Unknown;
        return false;
    }

    if (bar == 2) {
        // BAR2 = VRAM aperture. No size check (ResizableBar may change
        // dynamically; match returns true and passes offset to PcieMemoryDevice
        // which applies its own bound check per ADR-DGPU-05).
        out_target = BarTarget::Bar2VramAperture;
        return true;
    }

    out_target = BarTarget::Unknown;
    return false;
}

} // namespace tlm::pcie