// test/test_cpptlm_emulator_msix.cc
// T-W3-3 Phase 3: 4 ABI stub 替换验证 (msix_init/update_pending/clear_pending + lookup_register)
// SOC 未实例化时 board wrapper 返 -ENOSYS (-38); ABI stub 转发 + null check 返 -EINVAL (-22)
#include "abi/cpptlm_emulator.h"
#include <catch_amalgamated.hpp>

TEST_CASE("cpptlm_emulator_msix_init forwards to wrapper (null emu → -EINVAL, valid emu → wrapper "
          "return)",
          "[abi][msix][t-w3-3]") {
    REQUIRE(cpptlm_emulator_msix_init(nullptr, 16, 0) == -22); // EINVAL
    cpptlm_emulator_t* emu = cpptlm_emulator_create_by_id(0);
    REQUIRE(emu != nullptr);
    REQUIRE(cpptlm_emulator_msix_init(emu, 16, 0) == -38);   // ENOSYS (SOC deferred)
    REQUIRE(cpptlm_emulator_msix_init(emu, 3000, 0) == -22); // EINVAL (table_size > 2048)
    cpptlm_emulator_destroy(emu);
}

TEST_CASE("cpptlm_emulator_msix_update_pending forwards to wrapper", "[abi][msix][t-w3-3]") {
    REQUIRE(cpptlm_emulator_msix_update_pending(nullptr, 0) == -22);
    cpptlm_emulator_t* emu = cpptlm_emulator_create_by_id(0);
    REQUIRE(emu != nullptr);
    REQUIRE(cpptlm_emulator_msix_update_pending(emu, 0) == -38);
    cpptlm_emulator_destroy(emu);
}

TEST_CASE("cpptlm_emulator_msix_clear_pending forwards to wrapper", "[abi][msix][t-w3-3]") {
    REQUIRE(cpptlm_emulator_msix_clear_pending(nullptr, 0) == -22);
    cpptlm_emulator_t* emu = cpptlm_emulator_create_by_id(0);
    REQUIRE(emu != nullptr);
    REQUIRE(cpptlm_emulator_msix_clear_pending(emu, 0) == -38);
    cpptlm_emulator_destroy(emu);
}

TEST_CASE("cpptlm_emulator_lookup_register fills cpptlm_register_info_t via wrapper",
          "[abi][lookup_register][t-w3-3]") {
    cpptlm_register_info_t info{};
    REQUIRE(cpptlm_emulator_lookup_register(nullptr, 0, &info) == -22);
    cpptlm_emulator_t* emu = cpptlm_emulator_create_by_id(0);
    REQUIRE(emu != nullptr);
    REQUIRE(cpptlm_emulator_lookup_register(emu, 0x14, nullptr) == -22);
    REQUIRE(cpptlm_emulator_lookup_register(emu, 0x15, &info) == -38);    // unaligned
    REQUIRE(cpptlm_emulator_lookup_register(emu, 0x10000, &info) == -38); // > BAR0
    REQUIRE(cpptlm_emulator_lookup_register(emu, 0x14, &info) == -38);    // SOC deferred
    cpptlm_emulator_destroy(emu);
}
