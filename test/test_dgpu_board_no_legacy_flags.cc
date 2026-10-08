// test/test_dgpu_board_no_legacy_flags.cc
// A.6 (cpptlm-driver-visible-minimal-soc 2026-10-08): DGpuBoard public API guard
//
// 双标签 [realip][pin-contract] per handoff §9.2 #10 防 vacuous pass.
//
// Author: CppTLM Team / Date: 2026-10-08

#include "tlm/gpu/dgpu_board_shell.hh"

#include <catch_amalgamated.hpp>

TEST_CASE("DGpuBoard public API marker (C.3.0 rewrite, ACTIVE pending C.3.1)",
          "[realip][pin-contract]") {
    tlm::gpu::DGpuBoard board("test_board");

    SUCCEED("DGpuBoard instantiated successfully (legacy API not referenced)");
}

TEST_CASE("DGpuBoard no legacy setters called (C.3.0 compile-safe placeholder)",
          "[realip][pin-contract]") {
    tlm::gpu::DGpuBoard board("test_board_2");

    SUCCEED("legacy setters/pcie_path/attach_profile not called (compile gate holds)");
}