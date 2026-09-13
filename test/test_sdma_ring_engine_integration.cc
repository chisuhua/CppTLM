// test_sdma_ring_engine_integration.cc
// Stage 1.3a 接入: SdmaEngineTLM 内部 Ring Buffer 路径 + desc_in 互斥规则
// Author: CppTLM Team
// Date: 2026-09-13
//
// 参考: openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/spec.md
//   - Scenario "Doorbell write triggers": WPTR 写入 BAR1+0x10010000 → ring 消费
//   - tasks.md §1: "sdma_engine_tlm.cc 改造: descriptor 直投 → Ring Buffer + Doorbell"
//
// Oracle 决策 (per 1.3a ship commit 344a7a5c notes):
//   - 互斥规则: ring 模式下 desc_in 收到包 → dropped 计数 + error_cb, 不立即处理
//   - 非 ring 模式 (legacy): desc_in 仍直投 (既有 [sdma] 回归测试零破坏)
//   - Doorbell 真实触发 ring consume (BAR1+0x10010000 → mmio_write → SdmaEngineTLM)
//
// TDD 状态 (RED): 1.3a 接入前 SdmaEngineTLM 无 ring 路径 + 无 mmio_write 公开方法.

#include "bundles/dma_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dma_descriptor_mvp.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"
#include "tlm/gpu/sdma_packet.hh"
#include "tlm/gpu/sdma_ring_buffer.hh"

#include <cstdint>
#include <cstring>

using namespace tlm::gpu;
using namespace bundles;

static int fake_translate(uint64_t iova, uint32_t size, uint64_t& phys) {
    phys = iova;
    return 0;
}

// =============================================================================
// Scenario "Doorbell write triggers" — 真实触发链路
//   SdmaEngineTLM::mmio_write(bar=1, offset=0x10010000, wptr) → ring consume
// =============================================================================
TEST_CASE("SDMA: BAR1+0x10010000 doorbell 触发 ring consume",
          "[sdma][ring_engine][1.3a_integration][doorbell]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();
    sdma.set_translate_cb(fake_translate);

    // 1.3a 接入后存在: mmio_write + ring_mode 启用 + ring_consume API
    sdma.enable_ring_mode(SdmaRingBuffer::RingSize::KB_4, SdmaRingBuffer::EntrySize::B_64);
    REQUIRE(sdma.is_ring_mode() == true);

    // doorbell write WPTR=2 → ring[RPTR..WPTR] 被消费 → 2 个 H2D descriptor 处理
    // 注入 fake VRAM (ring consume 路径用 backdoor)
    std::vector<uint8_t> vram(0x100000, 0);
    sdma.set_vram_backdoor(vram.data(), vram.size());

    // 先手动 write 2 个 entry 到 ring (BAR1+0x10010000 data=WPTR=2)
    DmaDescriptor d1(DmaDescriptor::Dir::H2D, /*iova=*/0x100, /*vram=*/0x1000, /*sz=*/4, /*tag=*/1);
    DmaDescriptor d2(DmaDescriptor::Dir::H2D, /*iova=*/0x200, /*vram=*/0x2000, /*sz=*/4, /*tag=*/2);
    auto buf1 = SdmaPacket::serialize_descriptor(d1);
    auto buf2 = SdmaPacket::serialize_descriptor(d2);
    sdma.ring_write_entry(0, buf1.data(), 64);
    sdma.ring_write_entry(1, buf2.data(), 64);

    // doorbell 触发
    bool rc = sdma.mmio_write(/*bar=*/1, /*offset=*/0x10010000, /*data=*/2);
    REQUIRE(rc);

    // tick 消费 ring
    sdma.tick();

    // 验证: 2 个 descriptor 已 done
    REQUIRE(sdma.ring_consumed_count() == 2u);
    REQUIRE(sdma.completed_count() == 2u);
    REQUIRE(sdma.dropped_desc_in_count() == 0u);
}

// =============================================================================
// 互斥规则: ring 模式下 desc_in 收到包 → dropped + error_cb
// =============================================================================
TEST_CASE("SDMA: ring 模式下 desc_in 收到包 → dropped_desc_in + error_cb",
          "[sdma][ring_engine][1.3a_integration][mutex]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma_mutex", &eq);
    sdma.init();
    sdma.set_translate_cb(fake_translate);

    sdma.enable_ring_mode(SdmaRingBuffer::RingSize::KB_4, SdmaRingBuffer::EntrySize::B_64);
    REQUIRE(sdma.is_ring_mode() == true);

    int error_cb_count = 0;
    std::string last_err_msg;
    sdma.set_error_cb([&](int /*code*/, const std::string& msg) {
        error_cb_count++;
        last_err_msg = msg;
    });

    // 推 desc_in (ring 模式下应被 dropped)
    DmaDescriptor d(DmaDescriptor::Dir::H2D, /*iova=*/0x100, /*vram=*/0x1000, /*sz=*/4, /*tag=*/42);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].data() = SdmaEngineTLM::to_pcie_tlp_descriptor(d);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);
    sdma.tick();

    // 验证: 1 dropped + 1 error_cb (互斥规则)
    REQUIRE(sdma.dropped_desc_in_count() == 1u);
    REQUIRE(error_cb_count >= 1);
    REQUIRE((sdma.last_err_msg().find("ring mode") != std::string::npos ||
             sdma.last_err_msg().find("desc_in disabled") != std::string::npos));

    // ring 模式下 done_out 不应被 desc_in 路径触发
    // (descriptor-only 直接 emit 应被压制)
}

// =============================================================================
// 非 ring 模式 (legacy): desc_in 仍直投 + done_out emit (既有回归兼容)
// =============================================================================
TEST_CASE("SDMA: 非 ring 模式 (legacy) → desc_in 仍直投处理 (回归兼容)",
          "[sdma][ring_engine][1.3a_integration][legacy]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma_legacy", &eq);
    sdma.init();
    sdma.set_translate_cb(fake_translate);

    // 默认非 ring 模式 (legacy)
    REQUIRE_FALSE(sdma.is_ring_mode());

    std::vector<uint8_t> vram(0x100000, 0);
    sdma.set_vram_backdoor(vram.data(), vram.size());

    // 推 desc_in (legacy 路径)
    DmaDescriptor d(DmaDescriptor::Dir::H2D, /*iova=*/0x100, /*vram=*/0x1000, /*sz=*/4, /*tag=*/1);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].data() = SdmaEngineTLM::to_pcie_tlp_descriptor(d);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);
    sdma.tick();

    // legacy: done_out 应有 valid
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].valid());
    REQUIRE(sdma.completed_count() == 1u);
    REQUIRE(sdma.dropped_desc_in_count() == 0u);
}

// =============================================================================
// mmio_write 非 doorbell offset → 落入 BAR space (legacy)
// =============================================================================
TEST_CASE("SDMA: mmio_write BAR1 非 doorbell offset → 不触发 ring",
          "[sdma][ring_engine][1.3a_integration][bar_nop]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma_nop", &eq);
    sdma.init();

    sdma.enable_ring_mode(SdmaRingBuffer::RingSize::KB_4, SdmaRingBuffer::EntrySize::B_64);

    // 写 BAR1 offset 0x1000 (非 doorbell) → 返回 true (write accepted), 但不增加 ring 计数
    bool rc = sdma.mmio_write(/*bar=*/1, /*offset=*/0x1000, /*data=*/0xCAFE);
    REQUIRE(rc);
    REQUIRE(sdma.ring_consumed_count() == 0u);

    // 写 BAR0 offset 0x10010000 (BAR 错) → 不触发
    rc = sdma.mmio_write(/*bar=*/0, /*offset=*/0x10010000, /*data=*/0xDEAD);
    REQUIRE(rc);
    REQUIRE(sdma.ring_consumed_count() == 0u);
}

// =============================================================================
// SdmaPacket serialize_descriptor helper (为 mmio_write / ring_write_entry 准备)
// =============================================================================
TEST_CASE("SdmaPacket: serialize_descriptor / deserialize_descriptor roundtrip",
          "[sdma][packet][1.3a_integration][roundtrip]") {
    DmaDescriptor d(DmaDescriptor::Dir::H2D, /*iova=*/0xCAFE, /*vram=*/0xBABE, /*sz=*/0x100, /*tag=*/0x42);
    auto buf = SdmaPacket::serialize_descriptor(d);
    REQUIRE(buf.size() == 64u);

    DmaDescriptor recovered = SdmaPacket::deserialize_descriptor(buf.data(), 64);
    REQUIRE(recovered.host_iova == 0xCAFE);
    REQUIRE(recovered.vram_offset == 0xBABE);
    REQUIRE(recovered.size == 0x100);
    REQUIRE(recovered.tag == 0x42);
    REQUIRE(recovered.dir == DmaDescriptor::Dir::H2D);
}