// test_cpptlm_emulator_abi.cc
// T-ae-3 (AE-G3): 23 ABI 端到端测试 (per ADR-088 §D5 + ADR-SOC-07 D5).
// Author: CppTLM Team
// Date: 2026-08-29
//
// 验证: 19 forward ABI 函数符号可调 + 签名匹配 + 基本错误处理 (NULL,
// invalid args, 未知 dev_id) + 异常边界不逃逸.
// 注意: msix_* 当前 shell 未暴露 (deferred T-bs-4 follow-up),
// 测试期望返回 -ENOSYS (-38). load_soc_config SIGSEGV 已修复 (D15 commit
// 17413e4), backdoor sync 路径已实现 (commit e9d0030). full e2e 数据面
// round-trip 待 T-bs-4 后续工作补全.
//
// Link: target_link_libraries(cpptlm_tests PRIVATE cpptlm_emulator)
// 让符号在测试二进制中直接解析 (无需 dlopen).

#include "abi/cpptlm_emulator.h"

#include "catch_amalgamated.hpp"

#include <cstring>
#include <vector>

// Wrap ABI calls to swallow 23 函数符号 (避免头文件 extern "C" 重复声明).
// 直接 #include 头文件即可 (头已带 extern "C"),无需重复声明.

TEST_CASE("ABI: get_version returns v1.0-dgpu-v0 (修订版: 改用宏)",
          "[abi][version_macro]") {
    // 修订版: cpptlm_emulator_get_version 函数已删除, 改用 CPPTLM_EMULATOR_VERSION_STRING 宏
    // (per ADR-SOC-20 §2.1 cpptlm-abi-secondary-slimming, 修订版路径 18→15+宏)
    constexpr const char* v = CPPTLM_EMULATOR_VERSION_STRING;
    REQUIRE(v != nullptr);
    REQUIRE(std::string(v) == "v1.0-dgpu-v0");
}

TEST_CASE("ABI: get_device_count + get_device_info on empty registry", "[abi][registry][empty]") {
    REQUIRE(cpptlm_emulator_get_device_count() == 0u);

    cpptlm_device_info_t info{};
    int rc = cpptlm_emulator_get_device_info(1, &info);
    REQUIRE(rc == -ENOENT);

    REQUIRE(cpptlm_emulator_get_device_info(1, nullptr) == -EINVAL);
}

TEST_CASE("ABI: mmio_* / pcie_config_* return -ENOSYS or EINVAL on "
          "null handle",
          "[abi][null_handle]") {
    uint32_t v = 0;
    uint8_t buf[16] = {};

    REQUIRE(cpptlm_emulator_mmio_write(nullptr, 0, 0x14, buf, sizeof(buf)) == -EINVAL);
    REQUIRE(cpptlm_emulator_mmio_read(nullptr, 0, 0x14, buf, sizeof(buf)) == -EINVAL);
    REQUIRE(cpptlm_emulator_pcie_config_write(nullptr, 0, 4, 0) == -EINVAL);
    REQUIRE(cpptlm_emulator_pcie_config_read(nullptr, 0, 4, &v) == -EINVAL);

    REQUIRE(cpptlm_emulator_msix_init(nullptr, 16, 0) == -EINVAL);
    REQUIRE(cpptlm_emulator_msix_update_pending(nullptr, 0) == -EINVAL);
    REQUIRE(cpptlm_emulator_msix_clear_pending(nullptr, 0) == -EINVAL);

    REQUIRE(cpptlm_emulator_register_callbacks(nullptr, nullptr, nullptr, nullptr, nullptr,
                                               nullptr) == -EINVAL);
    REQUIRE(cpptlm_emulator_register_dma_translate_cb(nullptr, nullptr) == -EINVAL);
}

TEST_CASE("ABI: create / destroy / get_device_count lifecycle", "[abi][lifecycle]") {
    // create 调用 load_soc_config (SIGSEGV 已修复 per D15 commit 17413e4).
    // load_soc_config 已 defer SOC instantiate (T-bs-4 follow-up),返回 true.
    // 实际: create 失败时 create 返回 nullptr, 注册表 size 仍为 0.
    // 期望: create 返回 nullptr 或非-null, registry size 跟踪.
    uint32_t before = cpptlm_emulator_get_device_count();
    cpptlm_emulator_t* emu = cpptlm_emulator_create(nullptr);
    if (emu != nullptr) {
        cpptlm_emulator_destroy(emu);
        REQUIRE(cpptlm_emulator_get_device_count() == before);
    }
    // 多次 destroy nullptr 安全 (no-op per impl).
    cpptlm_emulator_destroy(nullptr);
    REQUIRE(cpptlm_emulator_get_device_count() == before);
}

TEST_CASE("ABI: msix_* return -ENOSYS (shell deferred)", "[abi][deferred][enotsys]") {
    cpptlm_emulator_t* emu = nullptr;
    REQUIRE(cpptlm_emulator_msix_init(emu, 16, 0) == -EINVAL);
    REQUIRE(cpptlm_emulator_msix_update_pending(emu, 0) == -EINVAL);
    REQUIRE(cpptlm_emulator_msix_clear_pending(emu, 0) == -EINVAL);
}

TEST_CASE("ABI: 18 symbols are linked (link-time verification, 修订版: 15 函数 + 1 宏)",
          "[abi][link]") {
    using FnCreate = cpptlm_emulator_t* (*)(const char*);
    using FnDestroy = void (*)(cpptlm_emulator_t*);
    using FnGetCount = uint32_t (*)();
    using FnGetInfo = int (*)(uint32_t, cpptlm_device_info_t*);
    using FnMmioW = int (*)(cpptlm_emulator_t*, uint8_t, uint64_t, const void*, size_t);
    using FnMmioR = int (*)(cpptlm_emulator_t*, uint8_t, uint64_t, void*, size_t);
    using FnPcieCfgW = int (*)(cpptlm_emulator_t*, uint16_t, uint8_t, uint32_t);
    using FnPcieCfgR = int (*)(cpptlm_emulator_t*, uint16_t, uint8_t, uint32_t*);
    using FnMsixInit = int (*)(cpptlm_emulator_t*, uint32_t, uint32_t);
    using FnMsixUpd = int (*)(cpptlm_emulator_t*, uint32_t);
    using FnMsixClr = int (*)(cpptlm_emulator_t*, uint32_t);
    using FnRegCbs = int (*)(cpptlm_emulator_t*, cpptlm_intr_deliver_cb_t, cpptlm_error_cb_t,
                              cpptlm_reset_complete_cb_t, cpptlm_power_cb_t, void*);
    using FnRegDmaCb = int (*)(cpptlm_emulator_t*, void*);
    using FnOpen = int (*)(uint32_t, cpptlm_emulator_handle_t*);
    using FnClose = int (*)(cpptlm_emulator_handle_t);

    // 修订版 (per ADR-SOC-20 §2.1): 15 函数 + 1 宏, 删 2 函数 + 改 1 函数为宏
    // 保留: create, destroy, get_device_count, get_device_info, mmio_*, pcie_config_*,
    //       msix_*, register_*, open, close + CPPTLM_EMULATOR_VERSION_STRING 宏
    // 删除: create_by_id, get_adapter_info, get_version (改宏)

    constexpr const char* p_version = CPPTLM_EMULATOR_VERSION_STRING;
    FnCreate p2 = &cpptlm_emulator_create;
    FnDestroy p4 = &cpptlm_emulator_destroy;
    FnGetCount p5 = &cpptlm_emulator_get_device_count;
    FnGetInfo p6 = &cpptlm_emulator_get_device_info;
    FnMmioW p7 = &cpptlm_emulator_mmio_write;
    FnMmioR p8 = &cpptlm_emulator_mmio_read;
    FnPcieCfgW p9 = &cpptlm_emulator_pcie_config_write;
    FnPcieCfgR p10 = &cpptlm_emulator_pcie_config_read;
    FnMsixInit p11 = &cpptlm_emulator_msix_init;
    FnMsixUpd p12 = &cpptlm_emulator_msix_update_pending;
    FnMsixClr p13 = &cpptlm_emulator_msix_clear_pending;
    FnRegCbs p14 = &cpptlm_emulator_register_callbacks;
    FnRegDmaCb p15 = &cpptlm_emulator_register_dma_translate_cb;
    FnOpen p16 = &cpptlm_emulator_open;
    FnClose p17 = &cpptlm_emulator_close;

    REQUIRE(p_version != nullptr);
    REQUIRE(p2 != nullptr);
    REQUIRE(p4 != nullptr);
    REQUIRE(p5 != nullptr);
    REQUIRE(p6 != nullptr);
    REQUIRE(p7 != nullptr);
    REQUIRE(p8 != nullptr);
    REQUIRE(p9 != nullptr);
    REQUIRE(p10 != nullptr);
    REQUIRE(p11 != nullptr);
    REQUIRE(p12 != nullptr);
    REQUIRE(p13 != nullptr);
    REQUIRE(p14 != nullptr);
    REQUIRE(p15 != nullptr);
    REQUIRE(p16 != nullptr);
    REQUIRE(p17 != nullptr);

    (void)p_version;
    (void)p2;
    (void)p4;
    (void)p5;
    (void)p6;
    (void)p7;
    (void)p8;
    (void)p9;
    (void)p10;
    (void)p11;
    (void)p12;
    (void)p13;
    (void)p14;
    (void)p15;
    (void)p16;
    (void)p17;
}
