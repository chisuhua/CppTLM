// scripts/test/try_compile_legacy_flags_removed.cc
// CMake try_compile() negative-compile probe source.
// Per Oracle C3 fix: private-member deletion verification CANNOT be done via SFINAE
// (C++17 access-checking not part of SFINAE substitution).
//
// This probe attempts to call legacy setters that we plan to delete in Change C.
// Expected: EXPECT-FAIL when Change C deletes these setters.
//
// NOTE: This file is NEVER compiled into cpptlm_core. It lives under scripts/test/
// and is consumed only by CMake's try_compile() during test_realip_legacy_flags_removed test.

#include "tlm/gpu/dgpu_board_shell.hh"

void probe() {
    tlm::gpu::DGpuBoard board("probe");
    board.set_display_routing_enabled(true);   // Should fail to compile after Change C
    board.set_memory_routing_enabled(true);     // Should fail to compile after Change C
    board.set_storage_routing_enabled(true);    // Should fail to compile after Change C
    board.set_gmmu_routing_enabled(true);       // Should fail to compile after Change C
    (void)board.pcie_path();                   // Should fail to compile after Change C
}

void probe_attach_profile() {
    tlm::gpu::DGpuBoard board2("probe2");
    nlohmann::json p;
    p["pcie_path"] = "legacy";
    board2.attach_profile(p);  // Should fail to compile after Change C
}
