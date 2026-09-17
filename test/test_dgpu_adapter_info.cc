// test_dgpu_adapter_info.cc
// T-W3-3 修订版 (per ADR-SOC-20): get_adapter_info 已删除, 改测 get_device_info
// Author: CppTLM Team
// Date: 2027-09-17 (修订自原 2026-08-29, 删除 get_adapter_info 测试)
//
// 原文件测试 cpptlm_emulator_get_adapter_info (句柄依赖), 修订版:
// - 改测 cpptlm_emulator_get_device_info (按 dev_id 查询, 无句柄依赖)
// - open/close 修订版保留, 但测试不再依赖句柄 API
// - get_adapter_info 完全删除, 测试移除
//
// Per HSK-12 §3.2 迁移指南: get_adapter_info(handle, ...) → get_device_info(dev_id, ...)

#include "abi/cpptlm_emulator.h"
#include "catch_amalgamated.hpp"

TEST_CASE("ABI: get_device_info by dev_id (修订版, 替代 get_adapter_info)",
          "[abi][device_info]") {
    // dev_id 0 默认映射到 configs/dgpu_board_v1.json
    cpptlm_device_info_t info{};
    int rc = cpptlm_emulator_get_device_info(0, &info);

    // 期望: 返回 0 (成功) 或 -2 (ENOENT) — 取决于 registry 中是否存在 dev_id 0
    // 修订版测试只验证函数签名 + 基本错误处理, 不验证业务数据
    if (rc == 0) {
        REQUIRE(info.vendor_id != 0);  // NVIDIA = 0x10DE 或其他非零值
    } else {
        REQUIRE(rc == -ENOENT);  // 期望: dev_id 0 不存在
    }
}

TEST_CASE("ABI: get_device_info with invalid dev_id", "[abi][device_info]") {
    cpptlm_device_info_t info{};
    // 不存在的 dev_id 应返回 -ENOENT
    REQUIRE(cpptlm_emulator_get_device_info(0xFFFFFFFE, &info) == -ENOENT);
    REQUIRE(cpptlm_emulator_get_device_info(0xFFFFFFFD, &info) == -ENOENT);
}

TEST_CASE("ABI: get_device_info with null pointer", "[abi][device_info]") {
    REQUIRE(cpptlm_emulator_get_device_info(0, nullptr) == -EINVAL);
    REQUIRE(cpptlm_emulator_get_device_info(999, nullptr) == -EINVAL);
}
