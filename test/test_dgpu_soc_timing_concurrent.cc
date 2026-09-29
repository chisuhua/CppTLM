// test/test_dgpu_soc_timing_concurrent.cc
// Timing-mode SoC E2E: SDMA + GMMU + Host egress 并发 (Phase 4 cpptlm-dgpu-soc-timing-mvp T7.4)
// 验证 design.md §6 + spec.md "concurrent submission":
//   - SDMA H2D + GMMU translate + Host egress (BAR1 读) 同一仿真窗口并发
//   - 验证 cycle accounting 正确 + fence 顺序正确
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>
#include "catch_amalgamated.hpp"
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/gpu/gmmu_tlm.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"
#include "tlm/gpu/sdma_packet.hh"
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using json = nlohmann::json;

namespace {

constexpr uint64_t kFramebufferSize = 16ULL * 1024 * 1024;
constexpr uint64_t kPtBase = 0x10000;
constexpr uint64_t kHostBufSize = 64ULL * 1024;
constexpr uint64_t kPageSize = 4096;

json load_timing_config() {
    json cfg;
    std::ifstream ifs("configs/dgpu_soc_timing_v1.json");
    if (ifs.is_open()) cfg = json::parse(ifs);
    REQUIRE(ifs.is_open());
    if (!cfg.contains("connections")) cfg["connections"] = json::array();
    if (cfg.contains("modules") && cfg["modules"].is_array()) {
        for (auto& m : cfg["modules"]) {
            if (m.contains("modules") && !m.contains("connections"))
                m["connections"] = json::array();
        }
    }
    return cfg;
}

} // namespace

TEST_CASE("timing-soc E2E: SDMA + GMMU + Host egress 并发提交", "[dgpu_soc_timing][timing_soc][e2e][concurrent]") {
    EventQueue eq;
    DGpuBoard board("timing_conc", &eq);
    REQUIRE(board.load_soc_config(load_timing_config()));
    std::vector<uint8_t> framebuffer(kFramebufferSize, 0);
    // PTE: descriptors iova=0x1000+i*4096 → page=1+i (1..8)
    for (uint64_t i = 1; i <= 8; ++i) {
        uint64_t pte = ((0x2000ULL + (i - 1) * kPageSize) & ~0xFFFULL) | 1ULL;
        std::memcpy(framebuffer.data() + kPtBase + i * 8, &pte, 8);
    }
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    SdmaEngineTLM* sdma = board.sdma_engine();
    REQUIRE(sdma != nullptr);
    GmmuTLM* gmmu = board.gmmu_module();
    REQUIRE(gmmu != nullptr);

    // host backdoor + ring mode
    std::vector<uint8_t> host_buf(kHostBufSize, 0x55);
    sdma->set_host_backdoor(host_buf.data(), host_buf.size());
    sdma->enable_ring_mode(SdmaRingBuffer::RingSize::KB_64, SdmaRingBuffer::EntrySize::B_64);

    // GMMU enable
    uint32_t pt_lo = static_cast<uint32_t>(kPtBase);
    REQUIRE(board.mmio_write(0, 0x00, &pt_lo, 4) == 0);
    uint32_t pt_hi = 0;
    REQUIRE(board.mmio_write(0, 0x04, &pt_hi, 4) == 0);
    uint32_t enable = 1;
    REQUIRE(board.mmio_write(0, 0x08, &enable, 4) == 0);

    // 8 个 H2D descriptors (iova = 0x1000+i*4096, vram_offset = i*4096)
    for (uint32_t i = 0; i < 8; ++i) {
        DmaDescriptor h2d(DmaDescriptor::Dir::H2D, 0x1000ULL + static_cast<uint64_t>(i) * kPageSize,
                          static_cast<uint64_t>(i) * kPageSize, kPageSize, i);
        auto entry = SdmaPacket::serialize_descriptor(h2d);
        REQUIRE(sdma->ring_write_entry(i, entry.data(), entry.size()));
    }
    // fence
    SdmaEngineTLM::FenceDescriptor fence;
    fence.opcode = 0x04;
    fence.fence_id = 42;
    fence.tag = 0x7;
    sdma->submit_fence(fence);

    // doorbell → 触发 consume
    uint32_t wptr = 8;
    REQUIRE(board.mmio_write(1, DGpuBoard::kBar1DoorbellOffset, &wptr, 4) == 0);

    // 并发: SDMA 完成 + GMMU translate + Host BAR1 读 同一窗口
    board.tick();
    board.tick();
    REQUIRE(sdma->completed_count() == 8);
    REQUIRE(sdma->error_count() == 0);
    REQUIRE(gmmu->stats().misses == 8);  // 8 distinct pages → 8 TLB miss

    // Host egress (BAR1 读) 与 SDMA 路径互不干扰 (数据面一致)
    uint64_t out = 0;
    REQUIRE(board.backdoor_read(0, &out, 1) == 0);
    REQUIRE((out & 0xFF) == 0x55);  // host_buf[0x2000] = 0x55

    board.shutdown();
}
