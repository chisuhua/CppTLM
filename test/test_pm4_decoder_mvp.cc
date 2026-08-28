// test_pm4_decoder_mvp.cc
// Pm4Decoder 单元测试 - 4 method_addr ranges 解析 + UNKNOWN 错误通道 + 比特字段分解
// Author: CppTLM Team
// Date: 2026-08-28
//
// Per design.md §2 + specs/pm4-decoder-mvp/spec.md + Oracle P1-1/P1-2 修复
// TDD: Step 1 (FAIL) - 预期编译/链接失败因为 Pm4Decoder 未实现
#include "catch_amalgamated.hpp"
#include "tlm/gpu/pm4_decoder_mvp.hh"
#include "tlm/gpu/pm4_types_mvp.hh"

using tlm::gpu::Pm4Decoder;
using tlm::gpu::Pm4MethodDispatch;
using tlm::gpu::Pm4MethodHeader;
using tlm::gpu::Pm4MethodType;

namespace {

    // Helper: pack Pm4MethodHeader fields into uint32_t word for test inputs
    // Per Pm4MethodHeader bit layout: inc:1 | method_addr:15 | subchannel:4 | data_count:4 |
    // reserved:8
    uint32_t pack_header(uint32_t inc, uint32_t method_addr, uint32_t subchannel,
                         uint32_t data_count, uint32_t reserved) {
        return (inc & 0x1u) | ((method_addr & 0x7FFFu) << 1) | ((subchannel & 0xFu) << 16) |
               ((data_count & 0xFu) << 20) | ((reserved & 0xFFu) << 24);
    }

} // namespace

// ── spec.md Scenario: Pm4MethodHeader bit layout matches NVIDIA spec ──
// 验证 0x12345678 bitfield 数学分解正确。
// 0x2B3C 不在 4 ranges → type=UNKNOWN → subchannel/data_count 归零 (per design §2)
TEST_CASE("Pm4Decoder: bitfield bit layout math (0x12345678)", "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    const uint32_t payload[16] = {0};
    Pm4MethodDispatch result = decoder.parse_method(0x12345678u, payload, 16);

    REQUIRE(result.method_addr == 0x2B3C);
    REQUIRE(result.type == Pm4MethodType::UNKNOWN);
    REQUIRE(result.subchannel_id == 0);
    REQUIRE(result.data_count == 0);
}

TEST_CASE("Pm4Decoder: DISPATCH_DIRECT preserves subchannel/data_count", "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    uint32_t packed = pack_header(0, 0x4001, 0x4, 0x3, 0);
    const uint32_t payload[16] = {0};
    Pm4MethodDispatch result = decoder.parse_method(packed, payload, 16);

    REQUIRE(result.method_addr == 0x4001);
    REQUIRE(result.type == Pm4MethodType::DISPATCH_DIRECT);
    REQUIRE(result.subchannel_id == 0x4);
    REQUIRE(result.data_count == 0x3);
}

// ── DISPATCH_DIRECT (0x4000-0x40FF) ──
TEST_CASE("Pm4Decoder: DISPATCH_DIRECT (0x4000-0x40FF) parsed", "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    uint32_t packed = pack_header(/*inc*/ 1, /*addr*/ 0x4001, /*subch*/ 0, /*dc*/ 3, /*res*/ 0);
    const uint32_t payload[16] = {0xDEADBEEFu, 0xCAFEBABEu, 0xFEEDFACEu};
    Pm4MethodDispatch result = decoder.parse_method(packed, payload, 16);

    REQUIRE(result.type == Pm4MethodType::DISPATCH_DIRECT);
    REQUIRE(result.method_addr == 0x4001);
    REQUIRE(result.subchannel_id == 0);
    REQUIRE(result.data_count == 3);
}

// ── EVENT_WRITE (0x4200-0x42FF) ──
TEST_CASE("Pm4Decoder: EVENT_WRITE (0x4200-0x42FF) parsed", "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    uint32_t packed = pack_header(0, 0x4201, 1, 1, 0);
    const uint32_t payload[16] = {0x12345678u};
    Pm4MethodDispatch result = decoder.parse_method(packed, payload, 16);

    REQUIRE(result.type == Pm4MethodType::EVENT_WRITE);
    REQUIRE(result.method_addr == 0x4201);
    REQUIRE(result.subchannel_id == 1);
    REQUIRE(result.data_count == 1);
}

// ── RELEASE_MEM (0x4400-0x44FF) ──
TEST_CASE("Pm4Decoder: RELEASE_MEM (0x4400-0x44FF) parsed", "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    uint32_t packed = pack_header(0, 0x4402, 2, 2, 0);
    const uint32_t payload[16] = {0xAAAAAAAAu, 0xBBBBBBBBu};
    Pm4MethodDispatch result = decoder.parse_method(packed, payload, 16);

    REQUIRE(result.type == Pm4MethodType::RELEASE_MEM);
    REQUIRE(result.method_addr == 0x4402);
    REQUIRE(result.subchannel_id == 2);
    REQUIRE(result.data_count == 2);
}

// ── ACQUIRE_MEM (0x4500-0x45FF) ──
TEST_CASE("Pm4Decoder: ACQUIRE_MEM (0x4500-0x45FF) parsed", "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    uint32_t packed = pack_header(0, 0x4503, 3, 4, 0);
    const uint32_t payload[16] = {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
    Pm4MethodDispatch result = decoder.parse_method(packed, payload, 16);

    REQUIRE(result.type == Pm4MethodType::ACQUIRE_MEM);
    REQUIRE(result.method_addr == 0x4503);
    REQUIRE(result.subchannel_id == 3);
    REQUIRE(result.data_count == 4);
}

// ── Unknown method_addr rejected (no throw, returns UNKNOWN) ──
TEST_CASE("Pm4Decoder: unknown method_addr returns UNKNOWN type (no throw)", "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    // 0x5FFF 不在 4 ranges (0x40xx/0x42xx/0x44xx/0x45xx),但仍在 15-bit 合法范围
    uint32_t packed = pack_header(0, 0x5FFF, 0, 5, 0);
    const uint32_t payload[16] = {0};

    REQUIRE_NOTHROW(decoder.parse_method(packed, payload, 16));
    Pm4MethodDispatch result = decoder.parse_method(packed, payload, 16);
    REQUIRE(result.type == Pm4MethodType::UNKNOWN);
    REQUIRE(result.method_addr == 0x5FFF);
    REQUIRE(result.subchannel_id == 0);
    REQUIRE(result.data_count == 0);
}

// ── Boundary: DISPATCH_DIRECT lower bound 0x4000 ──
TEST_CASE("Pm4Decoder: boundary 0x4000 (DISPATCH_DIRECT lower bound)", "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    uint32_t packed = pack_header(0, 0x4000, 0, 0, 0);
    const uint32_t payload[16] = {0};
    Pm4MethodDispatch result = decoder.parse_method(packed, payload, 16);

    REQUIRE(result.type == Pm4MethodType::DISPATCH_DIRECT);
    REQUIRE(result.method_addr == 0x4000);
}

// ── Boundary: DISPATCH_DIRECT upper bound 0x40FF ──
TEST_CASE("Pm4Decoder: boundary 0x40FF (DISPATCH_DIRECT upper bound)", "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    uint32_t packed = pack_header(0, 0x40FF, 0, 0, 0);
    const uint32_t payload[16] = {0};
    Pm4MethodDispatch result = decoder.parse_method(packed, payload, 16);

    REQUIRE(result.type == Pm4MethodType::DISPATCH_DIRECT);
}

// ── Boundary: 0x4100 just above DISPATCH_DIRECT → UNKNOWN ──
TEST_CASE("Pm4Decoder: boundary 0x4100 (just above DISPATCH_DIRECT, UNKNOWN)",
          "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    uint32_t packed = pack_header(0, 0x4100, 0, 0, 0);
    const uint32_t payload[16] = {0};
    Pm4MethodDispatch result = decoder.parse_method(packed, payload, 16);

    REQUIRE(result.type == Pm4MethodType::UNKNOWN);
}

// ── Boundary: ACQUIRE_MEM upper bound 0x45FF ──
TEST_CASE("Pm4Decoder: boundary 0x45FF (ACQUIRE_MEM upper bound)", "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    uint32_t packed = pack_header(0, 0x45FF, 0, 0, 0);
    const uint32_t payload[16] = {0};
    Pm4MethodDispatch result = decoder.parse_method(packed, payload, 16);

    REQUIRE(result.type == Pm4MethodType::ACQUIRE_MEM);
}

// ── Boundary: 0x4600 just above ACQUIRE_MEM → UNKNOWN ──
TEST_CASE("Pm4Decoder: boundary 0x4600 (just above ACQUIRE_MEM, UNKNOWN)", "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    uint32_t packed = pack_header(0, 0x4600, 0, 0, 0);
    const uint32_t payload[16] = {0};
    Pm4MethodDispatch result = decoder.parse_method(packed, payload, 16);

    REQUIRE(result.type == Pm4MethodType::UNKNOWN);
}

// ── reserved bits: 0xFF should not affect classification ──
TEST_CASE("Pm4Decoder: reserved bits (0xFF) do not affect classification", "[pm4-decoder][mvp]") {
    Pm4Decoder decoder;
    uint32_t packed = pack_header(0, 0x4001, 0, 3, 0xFF); // reserved=0xFF
    const uint32_t payload[16] = {0};
    Pm4MethodDispatch result = decoder.parse_method(packed, payload, 16);

    REQUIRE(result.type == Pm4MethodType::DISPATCH_DIRECT);
    REQUIRE(result.method_addr == 0x4001);
}
