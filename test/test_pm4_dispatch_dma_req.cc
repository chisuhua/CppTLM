// test_pm4_dispatch_dma_req.cc
// Stage 1.3c: PM4 opcode 0x4600-0x4900 DISPATCH dma_req 分支
// Author: CppTLM Team
// Date: 2026-09-13
//
// 参考: openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/spec.md
//   - Scenario "PM4 opcode 0x4600-0x4900 dispatch"
//   tasks.md §3: command_processor_mvp.cc DISPATCH dma_req
//
// 4 个新 SDMA PM4 opcode ranges:
//   0x4600-0x46FF → DISPATCH_INDIRECT      (SDMA copy 1D/2D/3D)
//   0x4700-0x47FF → DISPATCH_DIRECT_SDMA   (SDMA fill)
//   0x4800-0x48FF → DISPATCH_INDIRECT_SDMA (SDMA copy w/ SG chain)
//   0x4900-0x49FF → DISPATCH_FENCE_SDMA    (SDMA Fence)
//
// TDD 状态 (RED): 1.3c 实施前 LUT[0x46..0x49] 返回 UNKNOWN.

#include "catch_amalgamated.hpp"
#include "tlm/gpu/pm4_decoder_mvp.hh"
#include "tlm/gpu/pm4_types_mvp.hh"

#include <cstdint>

using namespace tlm::gpu;

// =============================================================================
// Scenario "PM4 opcode 0x4600-0x4900 dispatch"
//   Pm4Decoder LUT 0x46-0x49 命中新 4 个 SDMA types
// =============================================================================
TEST_CASE("Pm4Decoder: LUT[0x46-0x49] 命中 SDMA DISPATCH types",
          "[pm4][decoder][1.3c][sdma_lut]") {
    Pm4Decoder dec;
    // Header bit layout: [0]=inc, [1:15]=method_addr (15 bits), [16:19]=subchannel, [20:23]=data_count
    //   method_header = (method_addr << 1) | (subchannel << 16) | (data_count << 20) | inc
    SECTION("0x4600 → DISPATCH_INDIRECT (SDMA copy)") {
        const uint32_t h = (0x4600u << 1) | (1u << 16) | (2u << 20);  // sub=1, dwords=2
        auto d = dec.parse_method(h, nullptr, 0);
        REQUIRE(d.type == Pm4MethodType::DISPATCH_INDIRECT);
        REQUIRE(d.method_addr == 0x4600);
    }
    SECTION("0x4700 → DISPATCH_DIRECT_SDMA (SDMA fill)") {
        const uint32_t h = (0x4700u << 1);
        auto d = dec.parse_method(h, nullptr, 0);
        REQUIRE(d.type == Pm4MethodType::DISPATCH_DIRECT_SDMA);
    }
    SECTION("0x4800 → DISPATCH_INDIRECT_SDMA (SDMA SG chain)") {
        const uint32_t h = (0x4800u << 1);
        auto d = dec.parse_method(h, nullptr, 0);
        REQUIRE(d.type == Pm4MethodType::DISPATCH_INDIRECT_SDMA);
    }
    SECTION("0x4900 → DISPATCH_FENCE_SDMA (SDMA Fence)") {
        const uint32_t h = (0x4900u << 1);
        auto d = dec.parse_method(h, nullptr, 0);
        REQUIRE(d.type == Pm4MethodType::DISPATCH_FENCE_SDMA);
    }
}

// =============================================================================
// 既有 4 ranges 仍正确 (回归测试)
// =============================================================================
TEST_CASE("Pm4Decoder: 既有 4 ranges 仍正确 (回归兼容)",
          "[pm4][decoder][1.3c][regression]") {
    Pm4Decoder dec;
    SECTION("0x4000 → DISPATCH_DIRECT (CTA)") {
        auto d = dec.parse_method((0x4000u << 1), nullptr, 0);
        REQUIRE(d.type == Pm4MethodType::DISPATCH_DIRECT);
    }
    SECTION("0x4200 → EVENT_WRITE") {
        auto d = dec.parse_method((0x4200u << 1), nullptr, 0);
        REQUIRE(d.type == Pm4MethodType::EVENT_WRITE);
    }
    SECTION("0x4400 → RELEASE_MEM") {
        auto d = dec.parse_method((0x4400u << 1), nullptr, 0);
        REQUIRE(d.type == Pm4MethodType::RELEASE_MEM);
    }
    SECTION("0x4500 → ACQUIRE_MEM") {
        auto d = dec.parse_method((0x4500u << 1), nullptr, 0);
        REQUIRE(d.type == Pm4MethodType::ACQUIRE_MEM);
    }
}

// =============================================================================
// 既有 UNKNOWN 路径 (0x4A00+) 仍返回 UNKNOWN
// =============================================================================
TEST_CASE("Pm4Decoder: 0x4A00+ 仍 UNKNOWN",
          "[pm4][decoder][1.3c][unknown]") {
    Pm4Decoder dec;
    auto d = dec.parse_method((0x4A00u << 1), nullptr, 0);
    REQUIRE(d.type == Pm4MethodType::UNKNOWN);
}

// =============================================================================
// Pm4MethodType 枚举值断言 (强约束, 编译期)
// =============================================================================
TEST_CASE("Pm4MethodType: 9 枚举值 (4 既有 + 4 新 SDMA + UNKNOWN)",
          "[pm4][decoder][1.3c][enum]") {
    // 4 既有
    static_assert(static_cast<int>(Pm4MethodType::DISPATCH_DIRECT) == 0);
    static_assert(static_cast<int>(Pm4MethodType::EVENT_WRITE) == 1);
    static_assert(static_cast<int>(Pm4MethodType::RELEASE_MEM) == 2);
    static_assert(static_cast<int>(Pm4MethodType::ACQUIRE_MEM) == 3);
    // 4 新 (Stage 1.3c)
    static_assert(static_cast<int>(Pm4MethodType::DISPATCH_INDIRECT) == 4);
    static_assert(static_cast<int>(Pm4MethodType::DISPATCH_DIRECT_SDMA) == 5);
    static_assert(static_cast<int>(Pm4MethodType::DISPATCH_INDIRECT_SDMA) == 6);
    static_assert(static_cast<int>(Pm4MethodType::DISPATCH_FENCE_SDMA) == 7);
    static_assert(static_cast<int>(Pm4MethodType::UNKNOWN) == 8);
    SUCCEED("Pm4MethodType 9 枚举值断言通过");
}