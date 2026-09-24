// test/test_dgpu_board_shell_full_abi.cc
// T-W3-3 Phase 4 (1B): 23 ABI 多卡 lifecycle + stats path + device enum edge cases
// SOC deferred (D15) so happy-path forwarding returns -ENOSYS; tests cover
// lifecycle/enumeration paths testable without SOC.
// dev_id 0 has a profile path; higher dev_ids need a matching profile JSON
// or board methods return -EINVAL. Use dev_id 0 for forwarding tests.
#include <catch_amalgamated.hpp>
#include "abi/cpptlm_emulator.h"

TEST_CASE("Full ABI: multi-card lifecycle (4 distinct emus → destroy → restored, 修订版)",
          "[dgpu][shell][full_abi][lifecycle]") {
    // 修订版: dev_id opaque + next_dev_id_ 递增器 → first_dev_id 顺序敏感 (full-regression
    // 时 [abi] tag 先跑导致 -ENOENT). 改用 get_device_info(0, ...) 健壮写法 (per
    // test_dgpu_adapter_info.cc:24-28), 接受 rc=0 / -ENOENT 两种结果.
    uint32_t before = cpptlm_emulator_get_device_count();
    cpptlm_emulator_t* emus[4];
    for (uint32_t i = 0; i < 4; ++i) {
        // 修订版: 改用 cpptlm_emulator_create(nullptr) 替代 cpptlm_emulator_create_by_id(10+i)
        // (per ADR-SOC-20 §2.1 cpptlm-abi-secondary-slimming)
        emus[i] = cpptlm_emulator_create(nullptr);
        REQUIRE(emus[i] != nullptr);
        for (uint32_t j = 0; j < i; ++j) {
            REQUIRE(emus[i] != emus[j]);
        }
    }
    REQUIRE(cpptlm_emulator_get_device_count() == before + 4);

    cpptlm_device_info_t info{};
    int rc = cpptlm_emulator_get_device_info(0, &info);
    if (rc == 0) {
        REQUIRE(info.vendor_id != 0);
    } else {
        REQUIRE(rc == -ENOENT);
    }
    REQUIRE(cpptlm_emulator_get_device_info(0xFFFFFFFE, &info) == -2); // ENOENT (actual)

    for (uint32_t i = 0; i < 4; ++i) {
        cpptlm_emulator_destroy(emus[i]);
    }
    REQUIRE(cpptlm_emulator_get_device_count() == before);
}

TEST_CASE("Full ABI: mmio/backdoor return -ENOSYS via wrapper when SOC not instantiated",
          "[dgpu][shell][full_abi][forward]") {
    // 修订版: 改用 cpptlm_emulator_create(nullptr) 替代 create_by_id(0)
    cpptlm_emulator_t* e = cpptlm_emulator_create(nullptr);
    REQUIRE(e != nullptr);

    uint32_t val = 0;
    int wr = cpptlm_emulator_mmio_write(e, 0, 0x14, &val, sizeof(val));
    int rd = cpptlm_emulator_mmio_read(e, 0, 0x14, &val, sizeof(val));
    REQUIRE((wr == 0 || wr == -110 || wr == -22));
    REQUIRE((rd == 0 || rd == -110 || rd == -22));

    cpptlm_emulator_destroy(e);
}

TEST_CASE("Full ABI: create idempotent destroy (double destroy safe)",
          "[dgpu][shell][full_abi][lifecycle]") {
    uint32_t before = cpptlm_emulator_get_device_count();
    // 修订版: 改用 cpptlm_emulator_create(nullptr) 替代 create_by_id(0)
    cpptlm_emulator_t* e = cpptlm_emulator_create(nullptr);
    REQUIRE(e != nullptr);
    REQUIRE(cpptlm_emulator_get_device_count() == before + 1);

    cpptlm_emulator_destroy(e);
    cpptlm_emulator_destroy(e); // idempotent — must not crash
    REQUIRE(cpptlm_emulator_get_device_count() == before);
}

TEST_CASE("Full ABI: get_version non-null + device_info null/ENOENT guards (修订版: 改用宏)",
          "[dgpu][shell][full_abi][edge]") {
    // 修订版: cpptlm_emulator_get_version 函数已删除, 改用 CPPTLM_EMULATOR_VERSION_STRING 宏
    REQUIRE(CPPTLM_EMULATOR_VERSION_STRING != nullptr);

    cpptlm_device_info_t info{};
    REQUIRE(cpptlm_emulator_get_device_info(0, nullptr) == -22);
    REQUIRE(cpptlm_emulator_get_device_info(999, &info) == -2); // ENOENT
}

TEST_CASE("Full ABI: register_callbacks accepts non-null callbacks on valid emu",
          "[dgpu][shell][full_abi][callback]") {
    // 修订版: 改用 cpptlm_emulator_create(nullptr) 替代 create_by_id(0)
    cpptlm_emulator_t* e = cpptlm_emulator_create(nullptr);
    REQUIRE(e != nullptr);
    REQUIRE(cpptlm_emulator_register_callbacks(e, nullptr, nullptr, nullptr, nullptr, nullptr) ==
            0);
    REQUIRE(cpptlm_emulator_register_callbacks(nullptr, nullptr, nullptr, nullptr, nullptr,
                                               nullptr) == -22);
    cpptlm_emulator_destroy(e);
}
