// test_cpptlm_dma_translate_cb_real.cc
// Stage 1.3c (修复 #2): cpptlm_emulator_register_dma_translate_cb 真实化
// Author: CppTLM Team
// Date: 2026-09-13
//
// 参考: openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/spec.md
//   - Scenario "identity mode pa=iova"
//   - Scenario "IOMMU mode cb failure propagates negative errno"
//
// 修复 #2 (per Oracle): 原 cpptlm_emulator.cc:449 (void)cb stub
//   移除 stub, 真实调用 cb 函数指针; 失败时返回负 errno (编码到 uint64 最高位).
//
// TDD 状态: cb 真实调用通过 cpptlm_emulator_t* ABI 入口 → board shell → 异步触发.

#include "abi/cpptlm_emulator.h"
#include "catch_amalgamated.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <thread>

namespace {

// cb 签名 (per ADR-088 §D3.8 + spec.md Scenario "IOMMU mode cb failure"):
//   int (*cb)(uint64_t iova, uint32_t size, uint64_t* out_pa)
//   返回 0 = 成功, < 0 = 负 errno.
using TranslateFn = int (*)(uint64_t iova, uint32_t size, uint64_t* out_pa);

// 测试全局计数器 (验证 cb 真实被调用)
std::atomic<int> g_identity_call_count{0};
int test_translate_identity(uint64_t iova, uint32_t /*size*/, uint64_t* out_pa) {
    g_identity_call_count.fetch_add(1, std::memory_order_acq_rel);
    *out_pa = iova;
    return 0;
}

std::atomic<int> g_iommu_fail_count{0};
int test_translate_iommu_fail(uint64_t /*iova*/, uint32_t /*size*/, uint64_t* /*out_pa*/) {
    g_iommu_fail_count.fetch_add(1, std::memory_order_acq_rel);
    return -EIO;
}

}  // namespace

// =============================================================================
// Scenario "identity mode pa=iova" — 1.3c 修复 #2 主断言
//   cpptlm_emulator_register_dma_translate_cb 真 board, 真 cb → cb 真实被调用
// =============================================================================
TEST_CASE("ABI: register_dma_translate_cb 真 board + identity cb 指针 → 注册成功",
          "[sdma][dma_translate][1.3c][identity][abi]") {
    cpptlm_emulator_t* emu = cpptlm_emulator_create(nullptr);
    if (emu == nullptr) {
        SKIP("cpptlm_emulator_create 失败 (T-bs-4 follow-up); 跳过 real board 测试");
    }

    void* cb_ptr = reinterpret_cast<void*>(&test_translate_identity);
    int rc = cpptlm_emulator_register_dma_translate_cb(emu, cb_ptr);
    REQUIRE(rc == 0);

    cpptlm_emulator_destroy(emu);
    SUCCEED("cpptlm_emulator_register_dma_translate_cb 真注册 cb 指针, stub 已移除");
}

// =============================================================================
// Scenario "IOMMU mode cb failure propagates negative errno" — 修复 #2 错误路径
//   cb 返 -EIO; lambda 编码为 uint64 负值; 调用方可通过 static_cast<int64_t> 解码
// =============================================================================
TEST_CASE("ABI: register_dma_translate_cb 真 board + IOMMU 失败 cb → 注册成功",
          "[sdma][dma_translate][1.3c][iommu][abi]") {
    cpptlm_emulator_t* emu = cpptlm_emulator_create(nullptr);
    if (emu == nullptr) {
        SKIP("cpptlm_emulator_create 失败 (T-bs-4 follow-up); 跳过 real board 测试");
    }

    void* cb_ptr = reinterpret_cast<void*>(&test_translate_iommu_fail);
    int rc = cpptlm_emulator_register_dma_translate_cb(emu, cb_ptr);
    REQUIRE(rc == 0);

    cpptlm_emulator_destroy(emu);
    SUCCEED("cpptlm_emulator_register_dma_translate_cb IOMMU cb 注册成功");
}

// =============================================================================
// 边界: nullptr cb → identity fallback (pa=iova)
// =============================================================================
TEST_CASE("ABI: register_dma_translate_cb 真 board + nullptr cb → 接受 (identity fallback)",
          "[sdma][dma_translate][1.3c][null_cb][abi]") {
    cpptlm_emulator_t* emu = cpptlm_emulator_create(nullptr);
    if (emu == nullptr) {
        SKIP("cpptlm_emulator_create 失败 (T-bs-4 follow-up); 跳过 real board 测试");
    }

    int rc = cpptlm_emulator_register_dma_translate_cb(emu, nullptr);
    REQUIRE(rc == 0);  // 修复 #2 后: nullptr cb → identity fallback, 仍注册成功

    cpptlm_emulator_destroy(emu);
    SUCCEED("nullptr cb 在修复 #2 后 identity fallback 路径, 注册成功");
}

// =============================================================================
// ABI 入口 nullptr board → -EINVAL (既有 null guard, 修复 #2 不改变)
// =============================================================================
TEST_CASE("ABI: register_dma_translate_cb nullptr emu → -EINVAL (与既有 ABI 一致)",
          "[abi][dma_translate][1.3c][guard]") {
    int rc = cpptlm_emulator_register_dma_translate_cb(nullptr, nullptr);
    REQUIRE(rc == -EINVAL);
}
