// test/test_sdma_engine_timing.cc
// SdmaEngineTLM cycle accounting 单元测试 (Phase 4 cpptlm-dgpu-soc-timing-mvp T3)
// v0.2 简化 (per H2/H3 Oracle + design.md §4):
//   - 无新端口/无 outstanding/无 rid
//   - cycle accounting 仅在 translate_cb 内累加 GMMU TLB miss latency
//   - last_descriptor_complete_cycle_ 由 board 注入 (fence complete = board.current_cycle_)
// 验证 design.md §4 + spec.md "SDMA cycle accounting":
//   1. 4KB H2D TLB hit (lat=0) → last_descriptor_complete_cycle_ - submit_cycle ≈ 4 cyc
//   2. 4KB H2D TLB miss (lat=50) → 差 ≈ 54 cyc
//   3. Translate 失败 → fence err + lat 丢弃
//   4. 公共 API 零回归 (submit_descriptor 签名兼容)
//   5. set_cycle_accounting_enabled(false) → lat 丢弃 (functional 兼容)
#include <cstdint>
#include <cstring>
#include <vector>
#include "core/event_queue.hh"
#include "tlm/gpu/dma_descriptor_mvp.hh"
#include "tlm/gpu/gmmu_tlm.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"
#include <catch2/catch_all.hpp>

using namespace tlm::gpu;
using namespace bundles;

namespace {

constexpr uint64_t kPtBase = 0x10000;
constexpr uint64_t kPageSize = 4096;

// 构造 GMMU: 单页 PTE (page_num=1 → paddr 0x2000), 32-entry TLB
std::unique_ptr<GmmuTLM> make_gmmu(EventQueue* eq) {
    auto g = std::make_unique<GmmuTLM>("gmmu", eq);
    std::vector<uint8_t>* backing = new std::vector<uint8_t>(128 * 1024, 0);
    uint64_t pte = (0x2000ULL & ~0xFFFULL) | 1ULL;
    std::memcpy(backing->data() + kPtBase + 1 * 8, &pte, 8);
    g->set_mem_view(backing->data(), backing->size());
    g->set_pt_base_lo(static_cast<uint32_t>(kPtBase));
    g->set_pt_base_hi(0);
    g->set_enabled(true);
    g->set_timing_params(50);
    return g;
}

// 注入 H2D 描述符 + tick 处理, 返回描述符 (供后续断言)
DmaDescriptor submit_h2d(SdmaEngineTLM* sdma, uint64_t iova, uint32_t size, uint32_t tag) {
    DmaDescriptor desc(DmaDescriptor::Dir::H2D, iova, /*vram_offset=*/0, size, tag);
    PcieTlpBundle pkt = SdmaEngineTLM::to_pcie_tlp_descriptor(desc);
    sdma->req_in[SdmaEngineTLM::PORT_DESC_IN].data() = pkt;
    sdma->req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);
    sdma->tick();
    return desc;
}

} // namespace

TEST_CASE("sdma_timing: 4KB H2D TLB hit (lat=0) → diff ≈ 4 cyc", "[dgpu_soc_timing][sdma][timing]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();

    // host/vram backdoor (H2D 数据搬运路径)
    std::vector<uint8_t> host_buf(8192, 0xAA);
    std::vector<uint8_t> vram_buf(8192, 0);
    sdma.set_host_backdoor(host_buf.data(), host_buf.size());
    sdma.set_vram_backdoor(vram_buf.data(), vram_buf.size());

    // timing translate cb: gmmu->translate_timing 记录 latency
    auto gmmu = make_gmmu(&eq);
    sdma.set_cycle_accounting_enabled(true);
    sdma.set_translate_timing_cb([&](uint64_t iova, uint32_t size, uint64_t& phys, uint64_t& lat) {
        return gmmu->translate_timing(iova, size, phys, lat);
    });

    // 先预热 TLB (1 次 translate_timing 触发 miss + fill)
    uint64_t pa = 0, lat = 0;
    REQUIRE(gmmu->translate_timing(0x1000, kPageSize, pa, lat) == 0);
    REQUIRE(lat == 50);
    REQUIRE(gmmu->stats().misses == 1);

    // submit_cycle = 当前仿真 cycle (0)
    uint64_t submit_cycle = eq.getCurrentCycle();
    auto desc = submit_h2d(&sdma, 0x1000, kPageSize, 1);

    REQUIRE(sdma.completed_count() == 1);
    REQUIRE(sdma.error_count() == 0);
    // TLB hit → lat=0 → complete_cycle = submit + 0 + 4
    uint64_t diff = sdma.last_descriptor_complete_cycle() - submit_cycle;
    REQUIRE(diff == 4);
    REQUIRE(sdma.last_tlb_latency() == 0);
}

TEST_CASE("sdma_timing: 4KB H2D TLB miss (lat=50) → diff ≈ 54 cyc", "[dgpu_soc_timing][sdma][timing]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();

    std::vector<uint8_t> host_buf(8192, 0xAA);
    std::vector<uint8_t> vram_buf(8192, 0);
    sdma.set_host_backdoor(host_buf.data(), host_buf.size());
    sdma.set_vram_backdoor(vram_buf.data(), vram_buf.size());

    auto gmmu = make_gmmu(&eq);
    sdma.set_cycle_accounting_enabled(true);
    sdma.set_translate_timing_cb([&](uint64_t iova, uint32_t size, uint64_t& phys, uint64_t& lat) {
        return gmmu->translate_timing(iova, size, phys, lat);
    });

    // 未预热 TLB → 首次访问 miss (lat=50)
    uint64_t submit_cycle = eq.getCurrentCycle();
    auto desc = submit_h2d(&sdma, 0x1000, kPageSize, 1);

    REQUIRE(sdma.completed_count() == 1);
    REQUIRE(sdma.error_count() == 0);
    uint64_t diff = sdma.last_descriptor_complete_cycle() - submit_cycle;
    REQUIRE(diff == 54);  // 50 (TLB miss walk) + 4 (descriptor pipeline)
    REQUIRE(sdma.last_tlb_latency() == 0);  // 单次消费已重置
}

TEST_CASE("sdma_timing: translate 失败 → fence err + lat 丢弃", "[dgpu_soc_timing][sdma][timing]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();

    std::vector<uint8_t> host_buf(8192, 0xAA);
    std::vector<uint8_t> vram_buf(8192, 0);
    sdma.set_host_backdoor(host_buf.data(), host_buf.size());
    sdma.set_vram_backdoor(vram_buf.data(), vram_buf.size());

    auto gmmu = make_gmmu(&eq);
    sdma.set_cycle_accounting_enabled(true);
    sdma.set_translate_timing_cb([&](uint64_t iova, uint32_t size, uint64_t& phys, uint64_t& lat) {
        return gmmu->translate_timing(iova, size, phys, lat);
    });

    // 非法 iova (无 PTE) → translate 返回 -EIO
    uint64_t submit_cycle = eq.getCurrentCycle();
    auto desc = submit_h2d(&sdma, 0x9000, kPageSize, 7);  // page 9 无 PTE

    REQUIRE(sdma.completed_count() == 0);
    REQUIRE(sdma.error_count() == 1);
    REQUIRE(sdma.last_tlb_latency() == 0);  // lat 丢弃
    // fence err 不更新 last_descriptor_complete_cycle_ (保持 submit 值)
    REQUIRE(sdma.last_descriptor_complete_cycle() == submit_cycle);
}

TEST_CASE("sdma_timing: 公共 API 零回归 (submit_descriptor 签名兼容)", "[dgpu_soc_timing][sdma][timing]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();

    // 既有 functional translate_cb (签名不变) 仍可用
    sdma.set_translate_cb([&](uint64_t iova, uint32_t size, uint64_t& phys) {
        auto gmmu = make_gmmu(&eq);
        return gmmu->translate(iova, size, phys);
    });
    sdma.set_cycle_accounting_enabled(true);  // timing 开关不影响 functional cb 存在

    std::vector<uint8_t> host_buf(8192, 0xAA);
    std::vector<uint8_t> vram_buf(8192, 0);
    sdma.set_host_backdoor(host_buf.data(), host_buf.size());
    sdma.set_vram_backdoor(vram_buf.data(), vram_buf.size());

    // 既有 desc_in 提交路径 (PORT_DESC_IN) 兼容
    auto desc = submit_h2d(&sdma, 0x1000, kPageSize, 2);
    REQUIRE(sdma.completed_count() == 1);
    REQUIRE(sdma.error_count() == 0);
}

TEST_CASE("sdma_timing: set_cycle_accounting_enabled(false) → functional 兼容 (lat 丢弃)",
          "[dgpu_soc_timing][sdma][timing]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();

    std::vector<uint8_t> host_buf(8192, 0xAA);
    std::vector<uint8_t> vram_buf(8192, 0);
    sdma.set_host_backdoor(host_buf.data(), host_buf.size());
    sdma.set_vram_backdoor(vram_buf.data(), vram_buf.size());

    auto gmmu = make_gmmu(&eq);
    // 既有 functional translate_cb_ (DGpuBoard::bind_memory_backings 注入)
    sdma.set_translate_cb([&](uint64_t iova, uint32_t size, uint64_t& phys) {
        return gmmu->translate(iova, size, phys);
    });
    // timing cb 存在但不启用 (functional-mode 等价)
    sdma.set_translate_timing_cb([&](uint64_t iova, uint32_t size, uint64_t& phys, uint64_t& lat) {
        return gmmu->translate_timing(iova, size, phys, lat);
    });
    sdma.set_cycle_accounting_enabled(false);
    REQUIRE(sdma.cycle_accounting_enabled() == false);

    uint64_t submit_cycle = eq.getCurrentCycle();
    auto desc = submit_h2d(&sdma, 0x1000, kPageSize, 3);

    REQUIRE(sdma.completed_count() == 1);
    REQUIRE(sdma.error_count() == 0);
    // cycle accounting 禁用 → 无 diff 记录 (保持 submit 值)
    REQUIRE(sdma.last_descriptor_complete_cycle() == submit_cycle);
    REQUIRE(sdma.last_tlb_latency() == 0);  // lat 丢弃
}
