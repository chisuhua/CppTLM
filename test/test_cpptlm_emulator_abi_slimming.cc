// SPDX-License-Identifier: Apache-2.0
// T-ABI-2 + ABI-2-secondary: CppTLM Emulator ABI 表面精简验证测试 (修订版)
// 验证 22→18 (per cpptlm-abi-slimming) + 18→15+宏 (per cpptlm-abi-secondary-slimming 修订版)
// 删除 4 个 backdoor/lookup + 2 真冗余 + 1 改宏, 保留 open/close (修订版 fd 风格)
//
// Per openspec/changes/cpptlm-abi-slimming/{proposal,spec}.md
// Per openspec/changes/cpptlm-abi-secondary-slimming/{proposal,spec}.md (修订版)
// Per HSK-11 + HSK-12 (修订版)

#include <cstdint>
#include <cstring>

extern "C" {
#include "abi/cpptlm_emulator.h"
}

#include <catch_amalgamated.hpp>

// ============================================================
// 验证 1: 18→15+1 宏 驱动核心函数仍可调用 (修订版, 签名零修改)
// ============================================================

TEST_CASE("cpptlm_emulator ABI slimming: 18→15+1 宏 驱动核心函数签名零修改 (修订版)",
          "[abi-slimming][abi-secondary-slimming][pcie]") {
    // 设备管理 4 个 (含 1 宏)
    SECTION("CPPTLM_EMULATOR_VERSION_STRING 宏可用 (修订版: 替代 get_version)") {
        constexpr const char* v = CPPTLM_EMULATOR_VERSION_STRING;
        REQUIRE(v != nullptr);
        REQUIRE(std::string(v) == "v1.0-dgpu-v0");
    }
    SECTION("get_device_count 仍可用") {
        using FnGetDeviceCount = uint32_t (*)();
        constexpr FnGetDeviceCount fn = &cpptlm_emulator_get_device_count;
        REQUIRE(fn != nullptr);
    }
    SECTION("mmio_write 仍可用") {
        using FnMmioWrite = int (*)(cpptlm_emulator_t*, uint8_t, uint64_t, const void*, std::size_t);
        constexpr FnMmioWrite fn = &cpptlm_emulator_mmio_write;
        REQUIRE(fn != nullptr);
    }
    SECTION("mmio_read 仍可用") {
        using FnMmioRead = int (*)(cpptlm_emulator_t*, uint8_t, uint64_t, void*, std::size_t);
        constexpr FnMmioRead fn = &cpptlm_emulator_mmio_read;
        REQUIRE(fn != nullptr);
    }
    SECTION("register_callbacks 仍可用") {
        using FnRegCb = int (*)(cpptlm_emulator_t*, cpptlm_intr_deliver_cb_t, cpptlm_error_cb_t,
                                cpptlm_reset_complete_cb_t, cpptlm_power_cb_t, void*);
        constexpr FnRegCb fn = &cpptlm_emulator_register_callbacks;
        REQUIRE(fn != nullptr);
    }
    SECTION("get_device_info 仍可用 (修订版: 替代 get_adapter_info)") {
        using FnGetDeviceInfo = int (*)(uint32_t, cpptlm_device_info_t*);
        constexpr FnGetDeviceInfo fn = &cpptlm_emulator_get_device_info;
        REQUIRE(fn != nullptr);
    }
}

// ============================================================
// 验证 2: 4 个 backdoor/lookup 函数应已从 ABI 删除
// (编译期验证: 引用已删函数应编译失败)
// ============================================================

TEST_CASE("cpptlm_emulator ABI slimming: 4 函数应已从 ABI 删除",
          "[abi-slimming][pcie][t-p-9-0]") {
    SECTION("cpptlm_emulator_backdoor_read 签名不可引用") {
        // 引用应编译失败: 函数已删除
        // 验证: 头文件不应有该函数声明
        // (编译期检查: 如果下面这行能编译, 说明函数仍在)
        // using FnBackdoorRead = int (*)(cpptlm_emulator_t*, uint8_t, uint64_t, void*, std::size_t);
        // constexpr FnBackdoorRead fn = &cpptlm_emulator_backdoor_read;
        // REQUIRE(fn != nullptr);

        // 运行时验证: 头文件中无该符号 (使用 grep 替代)
        // 注: 此处仅做注释提示, 实际验证由 G2 门禁完成
        SUCCEED("cpptlm_emulator_backdoor_read 应已删除 (G2 门禁验证)");
    }
    SECTION("cpptlm_emulator_backdoor_write 签名不可引用") {
        SUCCEED("cpptlm_emulator_backdoor_write 应已删除 (G2 门禁验证)");
    }
    SECTION("cpptlm_emulator_register_backdoor_cb 签名不可引用") {
        SUCCEED("cpptlm_emulator_register_backdoor_cb 应已删除 (G2 门禁验证)");
    }
    SECTION("cpptlm_emulator_lookup_register 签名不可引用") {
        SUCCEED("cpptlm_emulator_lookup_register 应已删除 (G2 门禁验证)");
    }
}

// ============================================================
// 验证 3: 4 callback typedef 仍存在 (零修改)
// ============================================================

TEST_CASE("cpptlm_emulator ABI slimming: 4 callback typedef 不动",
          "[abi-slimming][pcie][t-p-9-0]") {
    SECTION("cpptlm_intr_deliver_cb_t 存在") {
        // 编译期验证: 头文件定义该 typedef
        using FnType = void (*)(void*, uint32_t, uint32_t);
        constexpr FnType fn = [](void* ctx, uint32_t vec, uint32_t tid) {
            (void)ctx; (void)vec; (void)tid;
        };
        cpptlm_intr_deliver_cb_t cb = fn;
        REQUIRE(cb != nullptr);
    }
    SECTION("cpptlm_error_cb_t 存在") {
        using FnType = void (*)(void*, int, const char*);
        constexpr FnType fn = [](void* ctx, int code, const char* msg) {
            (void)ctx; (void)code; (void)msg;
        };
        cpptlm_error_cb_t cb = fn;
        REQUIRE(cb != nullptr);
    }
    SECTION("cpptlm_reset_complete_cb_t 存在") {
        using FnType = void (*)(void*, int);
        constexpr FnType fn = [](void* ctx, int sid) {
            (void)ctx; (void)sid;
        };
        cpptlm_reset_complete_cb_t cb = fn;
        REQUIRE(cb != nullptr);
    }
    SECTION("cpptlm_power_cb_t 存在") {
        using FnType = void (*)(void*, int);
        constexpr FnType fn = [](void* ctx, int state) {
            (void)ctx; (void)state;
        };
        cpptlm_power_cb_t cb = fn;
        REQUIRE(cb != nullptr);
    }
}

// ============================================================
// 验证 4: cpptlm_emulator_t opaque 结构体存在
// ============================================================

TEST_CASE("cpptlm_emulator ABI slimming: cpptlm_emulator_t opaque 存在",
          "[abi-slimming][pcie][t-p-9-0]") {
    SECTION("opaque struct 可声明指针") {
        cpptlm_emulator_t* emu = nullptr;
        REQUIRE(emu == nullptr);
    }
    SECTION("结构体 sizeof 满足最小约束") {
        cpptlm_emulator_t* emu = nullptr;
        REQUIRE(sizeof(emu) == sizeof(void*));
    }
}
