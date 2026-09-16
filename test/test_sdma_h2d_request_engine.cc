// test/test_sdma_h2d_request_engine.cc
// SdmaEngineTLM H2D → PcieRequesterEngine MRd 集成测试 (T-P11-2)
// 验证: SDMA H2D 描述符触发 RequesterEngine::mrd_read() → tx_tlp → LL → host
//       收到 CplD 后写 VRAM + 触发 completion ring
// 作者 CppTLM Team / 日期 2027-02-09
// 参考: openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/tasks.md T-P11-2

#include "bundles/dma_bundles_tlm.hh"
#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"
#include "tlm/pcie/pcie_requester_engine.hh"

#include <cstdint>

using namespace bundles;
using namespace cpptlm::pcie;

// ==================== TEST_CASE 1: H2D 描述符触发 MRd 经 RequesterEngine ====================

TEST_CASE("SDMA H2D 描述符触发 MRd 经 RequesterEngine", "[sdma][h2d][requester]") {
    EventQueue eq;

    // Setup PcieLinkLayer with FC credits
    tlm::pcie::PcieLinkLayerConfig cfg;
    cfg.fc_capacity = 256;
    cfg.fc_init_np = 256;
    tlm::pcie::PcieLinkLayer ll(&eq, cfg);

    // Setup RequesterEngine
    PcieRequesterEngine req_eng(&ll);
    REQUIRE(req_eng.outstanding_count() == 0);
    REQUIRE(ll.tx_tlp_out_count() == 0);

    // Setup SdmaEngineTLM
    tlm::gpu::SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();
    sdma.set_request_engine(&req_eng);
    sdma.register_dma_translate_callback([](uint64_t iova) -> uint64_t {
        return iova; // identity mapping
    });

    // Submit H2D descriptor: host_iova=0x1000 → vram_offset=0x2000, size=64, tag=42
    tlm::gpu::DmaDescriptor desc(tlm::gpu::DmaDescriptor::Dir::H2D,
                                 /*host_iova=*/0x1000,
                                 /*vram_offset=*/0x2000,
                                 /*size=*/64,
                                 /*tag=*/42);
    auto desc_pkt = tlm::gpu::SdmaEngineTLM::to_pcie_tlp_descriptor(desc);
    sdma.req_in[tlm::gpu::SdmaEngineTLM::PORT_DESC_IN].data() = desc_pkt;
    sdma.req_in[tlm::gpu::SdmaEngineTLM::PORT_DESC_IN].set_valid(true);

    // Process
    sdma.tick();

    // Verify RequesterEngine outstanding_count == 1 (MRd 已发出)
    REQUIRE(req_eng.outstanding_count() == 1);
    REQUIRE(req_eng.last_allocated_tag() != 0);

    // Verify TLP was sent via LinkLayer
    REQUIRE(ll.tx_tlp_out_count() == 1);
}

// ==================== TEST_CASE 2: CplD 接收后写 VRAM + 触发 completion ====================

TEST_CASE("SDMA CplD 接收后写 VRAM + 触发 completion ring", "[sdma][h2d][requester][cpld]") {
    EventQueue eq;

    tlm::pcie::PcieLinkLayerConfig cfg;
    cfg.fc_capacity = 256;
    cfg.fc_init_np = 256;
    tlm::pcie::PcieLinkLayer ll(&eq, cfg);

    PcieRequesterEngine req_eng(&ll);
    tlm::gpu::SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();
    sdma.set_request_engine(&req_eng);
    sdma.register_dma_translate_callback([](uint64_t iova) -> uint64_t {
        return iova;
    });

    // Submit H2D descriptor: host_iova=0x2000 → vram_offset=0x3000, size=8, tag=1
    tlm::gpu::DmaDescriptor desc(tlm::gpu::DmaDescriptor::Dir::H2D,
                                 /*host_iova=*/0x2000,
                                 /*vram_offset=*/0x3000,
                                 /*size=*/8,
                                 /*tag=*/1);
    auto desc_pkt = tlm::gpu::SdmaEngineTLM::to_pcie_tlp_descriptor(desc);
    sdma.req_in[tlm::gpu::SdmaEngineTLM::PORT_DESC_IN].data() = desc_pkt;
    sdma.req_in[tlm::gpu::SdmaEngineTLM::PORT_DESC_IN].set_valid(true);
    sdma.tick();

    // Verify MRd was issued
    REQUIRE(req_eng.outstanding_count() == 1);
    uint16_t tag = req_eng.last_allocated_tag();
    REQUIRE(tag != 0);

    // Before CplD: VRAM should be 0, completion count 0
    REQUIRE(sdma.read_vram(0x3000) == 0u);
    REQUIRE(sdma.h2d_completion_count() == 0u);

    // Inject CplD with matching tag
    PcieTlpBundle cpld;
    cpld.kind.write(PcieTlpBundle::CPLD);
    cpld.trans_id.write(tag);
    cpld.data.write(0xDEADBEEFull);
    cpld.size.write(8);

    req_eng.on_cpld_received(cpld);

    // Verify VRAM written + completion triggered
    REQUIRE(sdma.read_vram(0x3000) == 0xDEADBEEFull);
    REQUIRE(sdma.h2d_completion_count() == 1u);

    // Outstanding should be cleared
    REQUIRE(req_eng.outstanding_count() == 0);
}