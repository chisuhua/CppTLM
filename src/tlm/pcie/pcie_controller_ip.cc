// src/tlm/pcie/pcie_controller_ip.cc
// PcieControllerIP facade implementation (P2 PR-A Task 14)
//
// Per H2 (Metis/Oracle review): consumes PcieEndpointIP::bar_router().
// No BAR match reimplementation — single source of truth.
#include "tlm/pcie/pcie_controller_ip.hh"

namespace tlm::pcie {

    PcieControllerIP::PcieControllerIP(const std::string& name, EventQueue* eq)
        : SimModule(name, eq) {
    }

    bool PcieControllerIP::match_bar(uint8_t bar, uint64_t offset, BarTarget& out_target,
                                     uint64_t& out_sub_offset) noexcept {
        out_sub_offset = offset;

        // BAR3 is upper dword of BAR2 64-bit pair (per H2) — never matches.
        if (bar == 3) {
            out_target = BarTarget::Unknown;
            return false;
        }

        if (bar == 0) {
            // Per Oracle bg_c60653e0 §2.1: BAR0 0x100 sub-dispatch REMOVED.
            // Controller facade returns Bar0Mmio for whole 4KB aperture;
            // GMMU/Display split belongs to target-side (DGpuBoard / P3 fabric).
            out_target = BarTarget::Bar0Mmio;
            return true;
        }

        if (bar == 1) {
            // BAR1 = framebuffer window. Per H3 + existing logic, sub-dispatch by
            // size relative to bar1 window (read live via PcieEndpointIP::bar_size).
            // B1 PR-B Phase Gate: composition — read via ep_ not inherited member.
            // If ep_ is null (attach_endpoint skipped), collapse to 0 → Bar1FbFastPath
            // impossible — explicit guard against dormant-gate regression.
            if (offset < (ep_ ? ep_->bar_size(1) : 0)) {
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