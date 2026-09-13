// test_sdma_ring_buffer.cc
// SdmaRingBuffer + SdmaPacket: SDMA Ring Buffer 单元测试 (Stage 1.3a, SD-RB-1)
// Author: CppTLM Team
// Date: 2026-09-13
//
// 参考: openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/spec.md
//   - Scenario "Ring Buffer 4 config sizes"
//   - Scenario "RPTR/WPTR atomic 32-bit"
//   - Scenario "SG descriptor chain ≥ 8"
//
// TDD 状态 (RED): 本文件覆盖 1.3a 实施前的 API 契约; 当前 SdmaRingBuffer /
// SdmaPacket 类不存在, 编译应失败, 错误原因为 "feature missing" (非 typo).
// 1.3a 实施 (Day 3-5) 后本测试变绿.

#include "bundles/dma_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dma_descriptor_mvp.hh"

// 前向依赖: SdmaRingBuffer / SdmaPacket 类尚未存在, 预期编译失败 (RED)
#include "tlm/gpu/sdma_packet.hh"
#include "tlm/gpu/sdma_ring_buffer.hh"

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

using namespace tlm::gpu;

namespace {

// 测试辅助: 构造确定性 DmaDescriptor (H2D, 4 字节)
DmaDescriptor make_test_desc(uint32_t tag) {
    return DmaDescriptor(DmaDescriptor::Dir::H2D,
                         /*host_iova=*/0x1000 + tag * 4,
                         /*vram_offset=*/0x2000 + tag * 4,
                         /*size=*/4,
                         /*tag=*/tag);
}

// 测试辅助: 序列化 DmaDescriptor → 64-byte ring entry (Round-trip 校验用)
std::array<uint8_t, 64> serialize_desc(const DmaDescriptor& d) {
    std::array<uint8_t, 64> buf{};
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(&d);
    for (size_t i = 0; i < sizeof(DmaDescriptor) && i < 64; ++i) {
        buf[i] = raw[i];
    }
    return buf;
}

}  // namespace

// =============================================================================
// Scenario 1: Ring Buffer 4 config sizes
//   cfg.ring_size ∈ {4KB, 8KB, 16KB, 64KB}
//   entry_size ∈ {32B, 64B}
//   entry_count = cfg_size / entry_size, capped at 1024 @64B
// =============================================================================
TEST_CASE("SdmaRingBuffer: 4 ring sizes × 2 entry sizes → capacity matches",
          "[sdma][ring][1.3a][capacity]") {
    SECTION("4KB / 32B = 128 entries") {
        SdmaRingBuffer ring(SdmaRingBuffer::RingSize::KB_4, SdmaRingBuffer::EntrySize::B_32);
        REQUIRE(ring.capacity_bytes() == 4096u);
        REQUIRE(ring.capacity_entries() == 128u);
    }
    SECTION("4KB / 64B = 64 entries") {
        SdmaRingBuffer ring(SdmaRingBuffer::RingSize::KB_4, SdmaRingBuffer::EntrySize::B_64);
        REQUIRE(ring.capacity_bytes() == 4096u);
        REQUIRE(ring.capacity_entries() == 64u);
    }
    SECTION("8KB / 64B = 128 entries") {
        SdmaRingBuffer ring(SdmaRingBuffer::RingSize::KB_8, SdmaRingBuffer::EntrySize::B_64);
        REQUIRE(ring.capacity_bytes() == 8192u);
        REQUIRE(ring.capacity_entries() == 128u);
    }
    SECTION("16KB / 64B = 256 entries") {
        SdmaRingBuffer ring(SdmaRingBuffer::RingSize::KB_16, SdmaRingBuffer::EntrySize::B_64);
        REQUIRE(ring.capacity_bytes() == 16384u);
        REQUIRE(ring.capacity_entries() == 256u);
    }
    SECTION("64KB / 64B = 1024 entries (max, no cap)") {
        SdmaRingBuffer ring(SdmaRingBuffer::RingSize::KB_64, SdmaRingBuffer::EntrySize::B_64);
        REQUIRE(ring.capacity_bytes() == 65536u);
        REQUIRE(ring.capacity_entries() == 1024u);
    }
    SECTION("64KB / 32B = 2048 entries → capped at 1024") {
        SdmaRingBuffer ring(SdmaRingBuffer::RingSize::KB_64, SdmaRingBuffer::EntrySize::B_32);
        REQUIRE(ring.capacity_bytes() == 65536u);
        // spec: "max 1024 entries @64B" — capacity 在 32B entry 下也被夹到 1024
        REQUIRE(ring.capacity_entries() == 1024u);
    }
}

// =============================================================================
// Scenario 2: RPTR/WPTR atomic 32-bit
//   std::atomic<uint32_t>; wptr ≥ rptr always
// =============================================================================
TEST_CASE("SdmaRingBuffer: RPTR/WPTR 32-bit atomic, wptr >= rptr invariant",
          "[sdma][ring][1.3a][atomic]") {
    SdmaRingBuffer ring(SdmaRingBuffer::RingSize::KB_4, SdmaRingBuffer::EntrySize::B_64);

    SECTION("初始 RPTR/WPTR 都为 0") {
        REQUIRE(ring.rptr().load() == 0u);
        REQUIRE(ring.wptr().load() == 0u);
    }
    SECTION("WPTR 写后 ≥ RPTR") {
        ring.advance_wptr(8);
        REQUIRE(ring.wptr().load() == 8u);
        REQUIRE(ring.wptr().load() >= ring.rptr().load());
    }
    SECTION("RPTR/WPTR 并发推进: std::atomic 数据竞争安全 + 不变量保持") {
        // 4 producer / 4 consumer × 16 iterations
        constexpr int kThreads = 4;
        constexpr int kIters = 16;
        std::vector<std::thread> writers;
        std::vector<std::thread> readers;
        writers.reserve(kThreads);
        readers.reserve(kThreads);

        for (int t = 0; t < kThreads; ++t) {
            writers.emplace_back([&ring, t]() {
                for (int i = 0; i < kIters; ++i) {
                    ring.advance_wptr(1);
                }
            });
            readers.emplace_back([&ring, t]() {
                for (int i = 0; i < kIters; ++i) {
                    ring.advance_rptr(1);
                }
            });
        }
        for (auto& w : writers) w.join();
        for (auto& r : readers) r.join();

        REQUIRE(ring.wptr().load() == static_cast<uint32_t>(kThreads * kIters));
        REQUIRE(ring.rptr().load() == static_cast<uint32_t>(kThreads * kIters));
        // 不变量 (per spec)
        REQUIRE(ring.wptr().load() >= ring.rptr().load());
    }
}

// =============================================================================
// Scenario 3: Doorbell write triggers ring processing
//   WPTR 写入 BAR1+0x10010000 → ring[RPTR..WPTR] 在下一 tick 被消费
// =============================================================================
TEST_CASE("SdmaRingBuffer: doorbell-style WPTR advance → ring entries marked ready",
          "[sdma][ring][1.3a][doorbell]") {
    SdmaRingBuffer ring(SdmaRingBuffer::RingSize::KB_4, SdmaRingBuffer::EntrySize::B_64);

    SECTION("WPTR=0 → 0 ready entries") {
        REQUIRE(ring.ready_count() == 0u);
    }
    SECTION("WPTR 推进 8 → 8 ready entries") {
        ring.advance_wptr(8);
        REQUIRE(ring.ready_count() == 8u);
    }
    SECTION("RPTR 推进到与 WPTR 相等 → ready_count 归零") {
        ring.advance_wptr(4);
        ring.advance_rptr(4);
        REQUIRE(ring.ready_count() == 0u);
    }
    SECTION("write_entry + advance_wptr 联合: ready_count 与 entry 内容正确") {
        DmaDescriptor d = make_test_desc(/*tag=*/42);
        auto buf = serialize_desc(d);
        ring.write_entry(/*index=*/0, buf.data());
        ring.advance_wptr(1);

        REQUIRE(ring.ready_count() == 1u);
        auto read_back = ring.read_entry(/*index=*/0);
        REQUIRE(read_back.size() == 64u);
        // Round-trip: 前 sizeof(DmaDescriptor) 字节应还原 DmaDescriptor
        DmaDescriptor recovered;
        std::memcpy(&recovered, read_back.data(), sizeof(DmaDescriptor));
        REQUIRE(recovered.host_iova == d.host_iova);
        REQUIRE(recovered.vram_offset == d.vram_offset);
        REQUIRE(recovered.size == d.size);
        REQUIRE(recovered.tag == d.tag);
    }
}

// =============================================================================
// Scenario 4: SG descriptor chain ≥ 8
//   SdmaPacket::add_sg() × 8 → sg_count() == 8, 链式访问全部有效
// =============================================================================
TEST_CASE("SdmaPacket: SG descriptor chain supports ≥ 8 entries",
          "[sdma][packet][sg][1.3a]") {
    SdmaPacket packet;

    SECTION("空 packet: sg_count() == 0") {
        REQUIRE(packet.sg_count() == 0u);
    }
    SECTION("add 8 SG descriptors → sg_count() == 8") {
        constexpr uint32_t kSgCount = 8;
        for (uint32_t i = 0; i < kSgCount; ++i) {
            SgDescriptor sg;
            sg.iova = 0x1000 + i * 0x1000;
            sg.size = 0x1000;
            sg.vram_offset = 0x10000 + i * 0x1000;
            sg.next_offset = (i + 1 < kSgCount) ? (i + 1) * 64 : 0;  // ring offset, 0 = end
            REQUIRE(packet.add_sg(sg) == true);
        }
        REQUIRE(packet.sg_count() == kSgCount);
        REQUIRE(packet.sg_count() >= 8u);  // spec: "SG ≥ 8"
    }
    SECTION("SG 链式访问: 8 个 sg 的字段全部正确读回") {
        constexpr uint32_t kSgCount = 8;
        for (uint32_t i = 0; i < kSgCount; ++i) {
            SgDescriptor sg;
            sg.iova = 0x2000 + i * 0x1000;
            sg.size = 0x800;
            sg.vram_offset = 0x20000 + i * 0x800;
            sg.next_offset = (i + 1 < kSgCount) ? (i + 1) * 64 : 0;
            packet.add_sg(sg);
        }
        for (uint32_t i = 0; i < kSgCount; ++i) {
            const SgDescriptor& sg = packet.sg_at(i);
            REQUIRE(sg.iova == 0x2000 + i * 0x1000);
            REQUIRE(sg.size == 0x800u);
            REQUIRE(sg.vram_offset == 0x20000 + i * 0x800u);
        }
    }
    SECTION("MAX_SG_ENTRIES = 8 硬上限: 第 9 个 add_sg 返回 false") {
        // per spec "SG ≥ 8" 但本实现硬上限 8 (chain 链式, >8 不实现)
        constexpr uint32_t kMaxSg = 8;
        for (uint32_t i = 0; i < kMaxSg; ++i) {
            SgDescriptor sg;
            sg.iova = i;
            sg.size = 4;
            sg.vram_offset = i * 4;
            sg.next_offset = 0;
            REQUIRE(packet.add_sg(sg) == true);
        }
        // 第 9 个应被拒绝
        SgDescriptor overflow;
        overflow.iova = 99;
        overflow.size = 4;
        overflow.vram_offset = 0;
        overflow.next_offset = 0;
        REQUIRE(packet.add_sg(overflow) == false);
        REQUIRE(packet.sg_count() == kMaxSg);
    }
}
