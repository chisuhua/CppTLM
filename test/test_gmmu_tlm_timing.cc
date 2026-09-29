// test/test_gmmu_tlm_timing.cc
// GmmuTLM TLB + page walk 单元测试 (Phase 4 cpptlm-dgpu-soc-timing-mvp T2)
// 验证 design.md §3 + spec.md "GmmuTLM TLB miss + page walk":
//   1. TLB hit → translate_timing lat=0, hits++
//   2. TLB miss → lat=50, TLB fill
//   3. 128KB 工作集 (32 pages) 重复 → hit rate ≈ 96.875% (32 fills + 992 hits)
//   4. 4MB 顺序流 → hit rate 0% (direct-mapped, 无 LRU)
//   5. 32-entry TLB 全满 → 33rd access 仍 fill (无 LRU 驱逐)
//   6. 跨页 → -EIO
//   7. 既有 translate() API 兼容 (零回归)
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include "event_queue.hh"
#include "tlm/gpu/gmmu_tlm.hh"
#include <catch2/catch_all.hpp>

using tlm::gpu::GmmuTLM;
static constexpr uint64_t kPageSize = 4096;
static constexpr uint64_t kPtBase = 0x10000;

namespace {

// 构造 GMMU: 32-entry TLB, 128KB backing, PT_BASE=0x10000, enabled
std::unique_ptr<GmmuTLM> make_gmmu(EventQueue* eq, uint64_t backing_size = 128 * 1024,
                                   size_t tlb_size = 32) {
    auto g = std::make_unique<GmmuTLM>("gmmu", eq);
    g->set_mem_view(nullptr, 0);  // ensure clean
    return g;
}

// 写入合法 PTE: page_num → paddr_page (paddr = page_num * 0x2000 + 0x2000)
void fill_pte(std::vector<uint8_t>& backing, uint64_t page_num, uint64_t paddr_page) {
    uint64_t pte = (paddr_page & ~0xFFFULL) | 1ULL;
    std::memcpy(backing.data() + kPtBase + page_num * 8, &pte, 8);
}

} // namespace

TEST_CASE("gmmu_timing: TLB hit → lat=0, hits++", "[dgpu_soc_timing][gmmu][timing]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("gmmu", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    fill_pte(backing, 1, 0x2000);
    g->set_mem_view(backing.data(), backing.size());
    g->set_pt_base_lo(static_cast<uint32_t>(kPtBase));
    g->set_pt_base_hi(0);
    g->set_enabled(true);
    g->set_timing_params(50);
    REQUIRE(g->tlb_size() == 32);

    uint64_t pa = 0, lat = 999;
    int rc = g->translate_timing(0x1000, kPageSize, pa, lat);
    REQUIRE(rc == 0);
    REQUIRE(pa == 0x2000);
    REQUIRE(lat == 50);   // first access = TLB miss → page walk
    REQUIRE(g->stats().hits == 0);
    REQUIRE(g->stats().misses == 1);
    REQUIRE(g->stats().total_walk_cycles == 50);

    // second access same page = TLB hit
    lat = 999;
    rc = g->translate_timing(0x1000, kPageSize, pa, lat);
    REQUIRE(rc == 0);
    REQUIRE(pa == 0x2000);
    REQUIRE(lat == 0);
    REQUIRE(g->stats().hits == 1);
    REQUIRE(g->stats().misses == 1);
}

TEST_CASE("gmmu_timing: TLB miss → lat=50, TLB fill", "[dgpu_soc_timing][gmmu][timing]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("gmmu", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    fill_pte(backing, 2, 0x4000);
    g->set_mem_view(backing.data(), backing.size());
    g->set_pt_base_lo(static_cast<uint32_t>(kPtBase));
    g->set_pt_base_hi(0);
    g->set_enabled(true);
    g->set_timing_params(50);

    uint64_t pa = 0, lat = 0;
    REQUIRE(g->translate_timing(0x2000, kPageSize, pa, lat) == 0);
    REQUIRE(pa == 0x4000);
    REQUIRE(lat == 50);
    // TLB 已 fill → 后续访问 hit
    REQUIRE(g->translate_timing(0x2FF0, 16, pa, lat) == 0);
    REQUIRE(lat == 0);
    REQUIRE(pa == (0x4000 | 0xFF0));
}

TEST_CASE("gmmu_timing: 128KB 工作集 (32 pages) 重复 → hit rate ≈ 96.875%",
          "[dgpu_soc_timing][gmmu][timing]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("gmmu", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    // 32 pages: page 0..31 → paddr page 0x2000.. (每个 page 独立 paddr)
    for (uint64_t p = 0; p < 32; ++p) {
        fill_pte(backing, p, 0x2000 + p * kPageSize);
    }
    g->set_mem_view(backing.data(), backing.size());
    g->set_pt_base_lo(static_cast<uint32_t>(kPtBase));
    g->set_pt_base_hi(0);
    g->set_enabled(true);
    g->set_timing_params(50);

    // 1024 次访问: 32 pages 循环 × 32 次 → 初始 fill 32 miss + 992 hit
    for (int i = 0; i < 1024; ++i) {
        uint64_t page = static_cast<uint64_t>(i % 32);
        uint64_t pa = 0, lat = 0;
        REQUIRE(g->translate_timing(page * kPageSize, kPageSize, pa, lat) == 0);
        REQUIRE(pa == (0x2000 + page * kPageSize));
        REQUIRE((lat == 0 || lat == 50));
    }
    REQUIRE(g->stats().hits == 992);
    REQUIRE(g->stats().misses == 32);
    double hit_rate = static_cast<double>(g->stats().hits) /
                      (g->stats().hits + g->stats().misses);
    REQUIRE(hit_rate == Catch::Approx(0.96875).epsilon(0.0001));
}

TEST_CASE("gmmu_timing: 4MB 顺序流 → hit rate 0% (direct-mapped 无 LRU)",
          "[dgpu_soc_timing][gmmu][timing]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("gmmu", &eq);
    std::vector<uint8_t> backing(8 * 1024 * 1024, 0);  // 8MB backing for 4MB stream
    // 4MB / 4KB = 1024 pages
    for (uint64_t p = 0; p < 1024; ++p) {
        fill_pte(backing, p, 0x2000 + p * kPageSize);
    }
    g->set_mem_view(backing.data(), backing.size());
    g->set_pt_base_lo(static_cast<uint32_t>(kPtBase));
    g->set_pt_base_hi(0);
    g->set_enabled(true);
    g->set_timing_params(50);

    // 顺序访问 1024 pages (每 page 1 次) → 全部 miss (direct-mapped, 不缓存第二次)
    for (uint64_t p = 0; p < 1024; ++p) {
        uint64_t pa = 0, lat = 0;
        REQUIRE(g->translate_timing(p * kPageSize, kPageSize, pa, lat) == 0);
        REQUIRE(lat == 50);  // 全 miss
    }
    REQUIRE(g->stats().misses == 1024);
    REQUIRE(g->stats().hits == 0);
}

TEST_CASE("gmmu_timing: 32-entry TLB 满 → 33rd access 仍 fill (无 LRU 驱逐)",
          "[dgpu_soc_timing][gmmu][timing]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("gmmu", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    for (uint64_t p = 0; p < 64; ++p) {
        fill_pte(backing, p, 0x2000 + p * kPageSize);
    }
    g->set_mem_view(backing.data(), backing.size());
    g->set_pt_base_lo(static_cast<uint32_t>(kPtBase));
    g->set_pt_base_hi(0);
    g->set_enabled(true);
    g->set_timing_params(50);

    // 访问 33 个不同 pages: page 0..32
    for (uint64_t p = 0; p < 33; ++p) {
        uint64_t pa = 0, lat = 0;
        REQUIRE(g->translate_timing(p * kPageSize, kPageSize, pa, lat) == 0);
        REQUIRE(lat == 50);  // 每个不同 page 都是 miss
    }
    REQUIRE(g->stats().misses == 33);
    REQUIRE(g->stats().hits == 0);
}

TEST_CASE("gmmu_timing: 跨页 → -EIO", "[dgpu_soc_timing][gmmu][timing]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("gmmu", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    fill_pte(backing, 1, 0x2000);
    g->set_mem_view(backing.data(), backing.size());
    g->set_pt_base_lo(static_cast<uint32_t>(kPtBase));
    g->set_pt_base_hi(0);
    g->set_enabled(true);
    g->set_timing_params(50);

    // iova 0x1000 + size 4096+1 → 跨页
    uint64_t pa = 0, lat = 0;
    int rc = g->translate_timing(0x1000, kPageSize + 1, pa, lat);
    REQUIRE(rc == -EIO);
    REQUIRE(lat == 0);
    // 无统计更新
    REQUIRE(g->stats().hits == 0);
    REQUIRE(g->stats().misses == 0);
}

TEST_CASE("gmmu_timing: 既有 translate() API 兼容 (零回归)", "[dgpu_soc_timing][gmmu][timing]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("gmmu", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    fill_pte(backing, 3, 0x6000);
    g->set_mem_view(backing.data(), backing.size());
    g->set_pt_base_lo(static_cast<uint32_t>(kPtBase));
    g->set_pt_base_hi(0);
    g->set_enabled(true);
    g->set_timing_params(50);

    // translate() 返回成功 + 正确 paddr, 无 latency 输出
    uint64_t pa = 0;
    int rc = g->translate(0x3000, kPageSize, pa);
    REQUIRE(rc == 0);
    REQUIRE(pa == 0x6000);
    // 内部转调 translate_timing → stats 更新
    REQUIRE(g->stats().misses == 1);

    // 第二次 = TLB hit
    rc = g->translate(0x3000, kPageSize, pa);
    REQUIRE(rc == 0);
    REQUIRE(g->stats().hits == 1);

    // disabled → -EIO (既有行为)
    g->set_enabled(false);
    rc = g->translate(0x3000, kPageSize, pa);
    REQUIRE(rc == -EIO);
}

TEST_CASE("gmmu_timing: reset 清 TLB + stats", "[dgpu_soc_timing][gmmu][timing]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("gmmu", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    fill_pte(backing, 1, 0x2000);
    g->set_mem_view(backing.data(), backing.size());
    g->set_pt_base_lo(static_cast<uint32_t>(kPtBase));
    g->set_pt_base_hi(0);
    g->set_enabled(true);
    g->set_timing_params(50);

    uint64_t pa = 0, lat = 0;
    REQUIRE(g->translate_timing(0x1000, kPageSize, pa, lat) == 0);  // miss
    REQUIRE(g->stats().misses == 1);

    ResetConfig rc;
    g->do_reset(rc);
    REQUIRE(g->stats().hits == 0);
    REQUIRE(g->stats().misses == 0);
    REQUIRE(g->tlb_size() == 32);

    // reset 后再次访问 → 重新 miss (TLB 已清)
    lat = 0;
    REQUIRE(g->translate_timing(0x1000, kPageSize, pa, lat) == 0);
    REQUIRE(lat == 50);
    REQUIRE(g->stats().misses == 1);
}
