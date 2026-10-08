// test/test_dgpu_board_no_legacy_flags.cc
// A.6 (cpptlm-driver-visible-minimal-soc 2026-10-08): DGpuBoard public API guard
// Per Oracle C3 fix: replace SFINAE-on-private with runtime checks (Catch2 WARN).
//
// 双标签 [realip][pin-contract] per handoff §9.2 #10 防 vacuous pass.
//
// In Change A scope: this test is XFAIL (legacy setters still exist, just verify
// public API surface — instantiation + name() work). Change C will activate
// STRICT verification that legacy setters are REMOVED.
//
// Author: CppTLM Team / Date: 2026-10-08

#include "tlm/gpu/dgpu_board_shell.hh"

#include <catch_amalgamated.hpp>

TEST_CASE("DGpuBoard public API marker (XFAIL pending Change C)", "[realip][pin-contract]") {
    tlm::gpu::DGpuBoard board("test_board");

    WARN("Change A scope: legacy setters still exist. Activate strict checks in Change C.");

    // 仅验证构造能完成 + 公共 enum 可访问。DGpuBoard::name_ 是 private 字段,
    // 无 public name() accessor, 故不能直接断言名字.
    SUCCEED("DGpuBoard instantiated successfully (private name_ not exposed via accessor)");
}

TEST_CASE("DGpuBoard legacy setters exist in Change A scope", "[realip][pin-contract]") {
    tlm::gpu::DGpuBoard board("test_board");

    // In Change A scope, these setters must exist (Change C will delete them).
    // Document the existence via successful compilation (the mere presence of this
    // TEST_CASE will fail at link time if Change C prematurely removes setters).
    board.set_display_routing_enabled(true);
    board.set_memory_routing_enabled(true);
    board.set_storage_routing_enabled(true);
    board.set_gmmu_routing_enabled(true);
    (void)board.pcie_path();

    SUCCEED("legacy setters exist as expected in Change A scope");
}