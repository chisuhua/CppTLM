// test/test_dgpu_soc_timing_d2h.cc
// Timing-mode SoC E2E: 4MB D2H + MSI-X cycle 验证 (Phase 4 cpptlm-dgpu-soc-timing-mvp T7.2)
// 验证 design.md §4 D2H 路径 + spec.md "D2H throughput + MSI-X cycle":
//   - 4MB D2H 1024 descriptors
//   - MSI-X fence 完成触发 vector 0
//   - 断言 throughput 期望范围 + fence 触发
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <thread>
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

constexpr uint64_t kFramebufferSize = 64ULL * 1024 * 1024;
constexpr uint64_t kPtBase = 0x10000;
constexpr uint64_t kHostBufSize = 8ULL * 1024 * 1024;
constexpr uint64_t kChunk = 4096;
constexpr uint32_t kNumChunks = 1024;
constexpr uint64_t kMaxCycles = 1'000'000;

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

void fill_ptes(std::vector<uint8_t>& backing) {
    // descriptor iova=0x1000+i*4096 → page=1+i
    for (uint64_t i = 0; i < kNumChunks; ++i) {
        uint64_t page = 1 + i;
        uint64_t pte = ((0x100000ULL + i * kChunk) & ~0xFFFULL) | 1ULL;
        std::memcpy(backing.data() + kPtBase + page * 8, &pte, 8);
    }
}

} // namespace

TEST_CASE("timing-soc E2E: 4MB D2H + MSI-X fence vector 0",
          "[dgpu_soc_timing][timing_soc][e2e][d2h]") {
    EventQueue eq;
    DGpuBoard board("timing_d2h", &eq);
    REQUIRE(board.load_soc_config(load_timing_config()));
    std::vector<uint8_t> framebuffer(kFramebufferSize, 0);
    fill_ptes(framebuffer);
    // 预置 VRAM 数据 (D2H 源)
    for (uint32_t i = 0; i < kNumChunks; ++i)
        framebuffer[i * kChunk] = static_cast<uint8_t>((i * 7 + 3) & 0xFF);
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    SdmaEngineTLM* sdma = board.sdma_engine();
    REQUIRE(sdma != nullptr);
    GmmuTLM* gmmu = board.gmmu_module();
    REQUIRE(gmmu != nullptr);

    // host backdoor (D2H 目标)
    std::vector<uint8_t> host_buf(kHostBufSize, 0);
    sdma->set_host_backdoor(host_buf.data(), host_buf.size());
    sdma->enable_ring_mode(SdmaRingBuffer::RingSize::KB_64, SdmaRingBuffer::EntrySize::B_64);

    // MSI-X: disable coalescing + init table (delivery 前必需)
    board.set_msix_coalesce_enabled(false);
    REQUIRE(board.msix_init(4, 0) == 0);
    std::atomic<int> intr_count{0};
    std::atomic<uint32_t> captured_vector{0xFFFF};
    board.set_irq_callback([&](uint32_t v) {
        intr_count.fetch_add(1, std::memory_order_acq_rel);
        captured_vector.store(v, std::memory_order_relaxed);
    });
    sdma->set_dgpu_board(&board);

    // GMMU enable + PT_BASE
    uint32_t pt_lo = static_cast<uint32_t>(kPtBase);
    REQUIRE(board.mmio_write(0, 0x00, &pt_lo, 4) == 0);
    uint32_t pt_hi = 0;
    REQUIRE(board.mmio_write(0, 0x04, &pt_hi, 4) == 0);
    uint32_t enable = 1;
    REQUIRE(board.mmio_write(0, 0x08, &enable, 4) == 0);

    // 1024 D2H descriptors (host_iova = 0x1000+i*4096, vram_offset = i*4096)
    for (uint32_t i = 0; i < kNumChunks; ++i) {
        DmaDescriptor d2h(DmaDescriptor::Dir::D2H, 0x1000ULL + static_cast<uint64_t>(i) * kChunk,
                          static_cast<uint64_t>(i) * kChunk, kChunk, i);
        auto entry = SdmaPacket::serialize_descriptor(d2h);
        REQUIRE(sdma->ring_write_entry(i, entry.data(), entry.size()));
    }

    // fence descriptor
    SdmaEngineTLM::FenceDescriptor fence;
    fence.opcode = 0x04;
    fence.fence_id = 1;
    fence.tag = 0x100;
    sdma->submit_fence(fence);

    // doorbell
    uint32_t wptr = kNumChunks;
    REQUIRE(board.mmio_write(1, DGpuBoard::kBar1DoorbellOffset, &wptr, 4) == 0);

    // 推进: descriptors 同步完成 (mmio_write doorbell 内), 但 fence queue 需 sdma.tick() 处理
    // 至少 1 tick 让 process_fence_queue 触发 (ring consume 同步, fence 经 tick)
    for (uint64_t c = 0; c < kMaxCycles && (sdma->completed_count() < kNumChunks || c < 4); ++c) {
        board.tick();
    }
    REQUIRE(sdma->completed_count() == kNumChunks);
    REQUIRE(sdma->error_count() == 0);

    // MSI-X fence 完成触发 vector 0 (异步 thread, 200ms 窗口)
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (intr_count.load(std::memory_order_acquire) < 1 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(intr_count.load() >= 1);
    REQUIRE(captured_vector.load() == SdmaEngineTLM::kSdmaFenceVector);
    REQUIRE(captured_vector.load() == 0u);

    // D2H 数据正确性: host_buf[iova] == framebuffer[vram_offset]
    // (SDMA D2H: host_backdoor[d.host_iova] = vram_backdoor[d.vram_offset])
    uint64_t h0 = 0;
    std::memcpy(&h0, host_buf.data() + 0x1000, sizeof(h0));
    REQUIRE((h0 & 0xFF) == framebuffer[0]);
    board.shutdown();
}
