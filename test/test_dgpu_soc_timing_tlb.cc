// test/test_dgpu_soc_timing_tlb.cc
// Timing-mode SoC E2E: TLB hit rate (Phase 4 cpptlm-dgpu-soc-timing-mvp T7.3)
// 验证 design.md §3 + spec.md "TLB hit rate":
//   - 128KB 工作集 (32 pages) 重复访问 → hit rate ≈ 96.875%
//   - 4MB 顺序流 → hit rate 0% (32-entry 直接映射, 无 LRU)
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
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using json = nlohmann::json;

namespace {

constexpr uint64_t kPtBase = 0x10000;
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

TEST_CASE("timing-soc E2E: TLB 128KB 工作集 hit rate ≈ 96.875%", "[dgpu_soc_timing][timing_soc][e2e][tlb]") {
    EventQueue eq;
    DGpuBoard board("timing_tlb", &eq);
    REQUIRE(board.load_soc_config(load_timing_config()));
    // 8MB backing: 32 pages (128KB 工作集) + PTE 区
    std::vector<uint8_t> framebuffer(8ULL * 1024 * 1024, 0);
    for (uint64_t p = 0; p < 32; ++p) {
        uint64_t pte = ((0x2000ULL + p * kPageSize) & ~0xFFFULL) | 1ULL;
        std::memcpy(framebuffer.data() + kPtBase + p * 8, &pte, 8);
    }
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    // GMMU 寄存器注入 (BAR0): PT_BASE + enable (gmmu_routing_enabled JSON=true)
    uint32_t pt_lo = static_cast<uint32_t>(kPtBase);
    REQUIRE(board.mmio_write(0, 0x00, &pt_lo, 4) == 0);
    uint32_t pt_hi = 0;
    REQUIRE(board.mmio_write(0, 0x04, &pt_hi, 4) == 0);
    uint32_t enable = 1;
    REQUIRE(board.mmio_write(0, 0x08, &enable, 4) == 0);
    board.tick();  // drain inject_q → gmmu set_pt_base/enabled

    GmmuTLM* gmmu = board.gmmu_module();
    REQUIRE(gmmu != nullptr);
    REQUIRE(gmmu->tlb_size() == 32);  // JSON 注入
    REQUIRE(gmmu->tlb_miss_latency() == 50);

    // 1024 次访问: 32 pages 循环 × 32 → 32 miss + 992 hit
    for (int i = 0; i < 1024; ++i) {
        uint64_t page = static_cast<uint64_t>(i % 32);
        uint64_t pa = 0, lat = 0;
        REQUIRE(gmmu->translate_timing(page * kPageSize, kPageSize, pa, lat) == 0);
        REQUIRE((lat == 0 || lat == 50));
    }
    REQUIRE(gmmu->stats().hits == 992);
    REQUIRE(gmmu->stats().misses == 32);
    double hit_rate = static_cast<double>(gmmu->stats().hits) /
                      (gmmu->stats().hits + gmmu->stats().misses);
    REQUIRE(hit_rate == Catch::Approx(0.96875).epsilon(0.0001));
    board.shutdown();
}

TEST_CASE("timing-soc E2E: TLB 4MB 顺序流 hit rate 0%", "[dgpu_soc_timing][timing_soc][e2e][tlb]") {
    EventQueue eq;
    DGpuBoard board("timing_tlb_seq", &eq);
    REQUIRE(board.load_soc_config(load_timing_config()));
    std::vector<uint8_t> framebuffer(8ULL * 1024 * 1024, 0);
    // 4MB = 1024 pages
    for (uint64_t p = 0; p < 1024; ++p) {
        uint64_t pte = ((0x2000ULL + p * kPageSize) & ~0xFFFULL) | 1ULL;
        std::memcpy(framebuffer.data() + kPtBase + p * 8, &pte, 8);
    }
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    // GMMU 寄存器注入 (BAR0)
    uint32_t pt_lo = static_cast<uint32_t>(kPtBase);
    REQUIRE(board.mmio_write(0, 0x00, &pt_lo, 4) == 0);
    uint32_t pt_hi = 0;
    REQUIRE(board.mmio_write(0, 0x04, &pt_hi, 4) == 0);
    uint32_t enable = 1;
    REQUIRE(board.mmio_write(0, 0x08, &enable, 4) == 0);
    board.tick();  // drain inject_q

    GmmuTLM* gmmu = board.gmmu_module();
    REQUIRE(gmmu != nullptr);

    for (uint64_t p = 0; p < 1024; ++p) {
        uint64_t pa = 0, lat = 0;
        REQUIRE(gmmu->translate_timing(p * kPageSize, kPageSize, pa, lat) == 0);
        REQUIRE(lat == 50);  // 全 miss (direct-mapped, 无 LRU)
    }
    REQUIRE(gmmu->stats().misses == 1024);
    REQUIRE(gmmu->stats().hits == 0);
    board.shutdown();
}
