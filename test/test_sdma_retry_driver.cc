// test/test_sdma_retry_driver.cc
// SDMA inflight retry driver (N3) + slot-2 resp (N7) 测试
// Phase 3 (driver-visible-minimal-soc v1.8)
#include <cerrno>
#include <cstdint>
#include <cstring>
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"
#include "bundles/pcie_bundles_tlm.hh"

using namespace tlm::gpu;
using namespace bundles;

// fake translate: 首次 -EAGAIN (异步 pending), 后续同步 0
static int g_retry_count = 0;
static int fake_async_translate(uint64_t iova, uint32_t size, uint64_t& phys) {
    if (g_retry_count++ == 0) return -EAGAIN;
    phys = iova;
    return 0;
}

TEST_CASE("SDMA: retry driver 保留 -EAGAIN inflight (N3)", "[sdma][retry][driver]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();
    sdma.set_translate_cb(fake_async_translate);
    g_retry_count = 0;

    DmaDescriptor desc(DmaDescriptor::Dir::H2D, /*iova=*/100, /*vram=*/0x1000, /*size=*/4, /*tag=*/1);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].data() = SdmaEngineTLM::to_pcie_tlp_descriptor(desc);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);

    // tick 1: translate 返 -EAGAIN → desc 保留在 inflight_, 不 emit done
    sdma.tick();
    REQUIRE(sdma.inflight_count() == 1);
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].valid() == false);

    // tick 2: retry_inflight() 先重试 → translate 同步 0 → emit done
    sdma.tick();
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].valid() == true);
    REQUIRE(sdma.completed_count() == 1);
    REQUIRE(sdma.inflight_count() == 0);
}

TEST_CASE("SDMA: slot-2 resp 消费 (N7)", "[sdma][slot2][response]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();

    // 注入 CPLD resp 到 req_in[PORT_MEM_OUT] (slot 2)
    PcieTlpBundle resp;
    resp.kind.write(PcieTlpBundle::CPLD);
    resp.trans_id.write(42);
    sdma.req_in[SdmaEngineTLM::PORT_MEM_OUT].data() = resp;
    sdma.req_in[SdmaEngineTLM::PORT_MEM_OUT].set_valid(true);

    REQUIRE(sdma.consume_mem_resp() == true);
    REQUIRE(sdma.req_in[SdmaEngineTLM::PORT_MEM_OUT].valid() == false);
}

TEST_CASE("SDMA: ring consume D2D 三向分派 (N1/H4)", "[sdma][d2d][dispatch]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();

    // 无 vram_backdoor: D2D 走 d2d_forward → memcpy 无效果但 emit done status=0
    // (诚实语义: minimal_v1 D2D 经 backdoor 或 -ENOSYS)
    DmaDescriptor desc(DmaDescriptor::Dir::D2D, /*iova=*/0, /*vram=*/0x1000, /*size=*/4, /*tag=*/7);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].data() = SdmaEngineTLM::to_pcie_tlp_descriptor(desc);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);
    sdma.tick();
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].valid() == true);
    // D2D 不增 host_out 计数
    REQUIRE(sdma.host_out_tx_count() == 0);
}
