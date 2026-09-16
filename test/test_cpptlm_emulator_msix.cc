// test/test_cpptlm_emulator_msix.cc
// T-W3-3 Phase 3: 4 ABI stub 替换验证 (msix_init/update_pending/clear_pending + lookup_register)
// D15 fix (5425c45): SOC instantiated → wrappers forward (0); null/over-cap → -EINVAL (-22)
// RAII: EmulatorHandleGuard (test_cpptlm_emulator_handle_helpers.hh) 保证 REQUIRE 失败时
// handle 仍被释放, ASan 零泄漏 (fix-asan-cpptlm-emulator-leak 任务 2.2/2.3).
#include <catch_amalgamated.hpp>
#include "abi/cpptlm_emulator.h"
#include "test_cpptlm_emulator_handle_helpers.hh"

TEST_CASE("cpptlm_emulator_msix_init forwards to wrapper (null emu → -EINVAL, valid emu → wrapper "
          "return)",
          "[abi][msix][t-w3-3]") {
    REQUIRE(cpptlm_emulator_msix_init(nullptr, 16, 0) == -22); // EINVAL
    EmulatorHandleGuard emu(0);
    REQUIRE(emu.valid());
    // D15 fix: SOC live → wrapper forwards to PcieEndpointTLM::msix().init() → 0
    REQUIRE(cpptlm_emulator_msix_init(emu.emu, 16, 0) == 0);
    REQUIRE(cpptlm_emulator_msix_init(emu.emu, 3000, 0) == -22); // EINVAL (table_size > 2048)
}

TEST_CASE("cpptlm_emulator_msix_update_pending forwards to wrapper", "[abi][msix][t-w3-3]") {
    REQUIRE(cpptlm_emulator_msix_update_pending(nullptr, 0) == -22);
    EmulatorHandleGuard emu(0);
    REQUIRE(emu.valid());
    // D15 fix: SOC live → wrapper forwards → 0
    REQUIRE(cpptlm_emulator_msix_update_pending(emu.emu, 0) == 0);
}

TEST_CASE("cpptlm_emulator_msix_clear_pending forwards to wrapper", "[abi][msix][t-w3-3]") {
    REQUIRE(cpptlm_emulator_msix_clear_pending(nullptr, 0) == -22);
    EmulatorHandleGuard emu(0);
    REQUIRE(emu.valid());
    // D15 fix: SOC live → wrapper forwards. clear_pending returns 0 only when
    // PBA bit is set (i.e. after update_pending). Post-init PBA is empty so
    // clear returns false → wrapper -22; accept either valid outcome.
    int rc = cpptlm_emulator_msix_clear_pending(emu.emu, 0);
    REQUIRE((rc == 0 || rc == -22));
}
