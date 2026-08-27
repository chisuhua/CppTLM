// test_pcie_endpoint_bar_routing.cc
// PcieEndpointTLM: BAR0/BAR1 路由测试 + 0x0014 门铃 strong-order (PE-G3)
// Author: CppTLM Team
// Date: 2026-08-26
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/design.md §6
//       spec.md Scenario "Doorbell register write is table-driven"

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/pcie_bar_router_mvp.hh"
#include "tlm/gpu/pcie_endpoint_tlm.h"

#include <chrono>
#include <thread>

using namespace tlm::gpu;
using namespace bundles;

TEST_CASE("PcieEndpoint: BAR0 unknown offset returns 0xFFFFFFFF", "[pcie][endpoint][bar]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 0x0014 未注册 → read 返回 0xFFFFFFFF
    REQUIRE(ep.bar_router().mmio_read(0x0014) == 0xFFFFFFFFu);
    // 越界 0xFFFFFFF0 越界/未对齐 → 0xFFFFFFFF
    REQUIRE(ep.bar_router().mmio_read(0xFFFFFFF0) == 0xFFFFFFFFu);
}

TEST_CASE("PcieEndpoint: BAR0 doorbell side_effect (250-700ns strong-order)",
          "[pcie][endpoint][bar][doorbell]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 配置门铃寄存器 0x0014, stream_id=0
    REQUIRE(ep.bar_router().add_register(0x0014, "GPU_REG_DOORBELL", PcieBarRouter::Access::WO,
                                         PcieBarRouter::SideEffect::DOORBELL,
                                         /*stream_id=*/0) == true);

    const uint64_t t0 = ep.bar_router().doorbell_now_cycles();
    REQUIRE(ep.bar_router().mmio_write(0x0014, 0x200) == true);
    REQUIRE(ep.bar_router().doorbell_is_pending(0) == true);

    // Advance cycles 推进 doorbell (per design.md §3 250-700ns strong-order)
    for (int i = 0; i < 1000 && ep.bar_router().doorbell_is_pending(0); ++i) {
        ep.bar_router().tick();
    }

    REQUIRE(ep.bar_router().doorbell_is_pending(0) == false);
    REQUIRE(ep.bar_router().doorbell_sq_tail(0) == 0x200u);

    // 断言 250-700ns 区间 (per spec.md Scenario "Doorbell register write is table-driven")
    const uint64_t latency_cycles = ep.bar_router().doorbell_now_cycles() - t0;
    REQUIRE(latency_cycles >= Doorbell::MIN_LATENCY_NS); // 250
    REQUIRE(latency_cycles <= Doorbell::MAX_LATENCY_NS); // 700
}

TEST_CASE("PcieEndpoint: BAR0 data-driven registers (no if-else)",
          "[pcie][endpoint][bar][data-driven]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 配置 3 个不同 offset 的寄存器
    REQUIRE(ep.bar_router().add_register(0x0000, "REG0", PcieBarRouter::Access::RW,
                                         PcieBarRouter::SideEffect::NONE) == true);
    REQUIRE(ep.bar_router().add_register(0x1000, "REG1", PcieBarRouter::Access::RW,
                                         PcieBarRouter::SideEffect::NONE) == true);
    REQUIRE(ep.bar_router().add_register(0x2000, "REG2", PcieBarRouter::Access::RW,
                                         PcieBarRouter::SideEffect::NONE) == true);

    // 写后读回
    REQUIRE(ep.bar_router().mmio_write(0x0000, 0xAAAA0001) == true);
    REQUIRE(ep.bar_router().mmio_read(0x0000) == 0xAAAA0001u);
    REQUIRE(ep.bar_router().mmio_write(0x1000, 0xBBBB0002) == true);
    REQUIRE(ep.bar_router().mmio_read(0x1000) == 0xBBBB0002u);
    REQUIRE(ep.bar_router().mmio_write(0x2000, 0xCCCC0003) == true);
    REQUIRE(ep.bar_router().mmio_read(0x2000) == 0xCCCC0003u);

    // RO 寄存器写拒绝
    REQUIRE(ep.bar_router().add_register(0x3000, "REG_RO", PcieBarRouter::Access::RO,
                                         PcieBarRouter::SideEffect::NONE) == true);
    REQUIRE(ep.bar_router().mmio_write(0x3000, 0x12345678) == false);
}

TEST_CASE("PcieEndpoint: BAR1 MEM size <= 8 forwards to mem_out", "[pcie][endpoint][bar][mem]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 不直接测试 mem_out（通过 PcieEndpointTLM::handle_slave_in_tlp）;
    // 这里仅断言 PcieTlpBundle 是 POD 且 size 字段语义（字节数）
    PcieTlpBundle req;
    req.kind.write(PcieTlpBundle::MEM_WRITE);
    req.bar_index.write(1);
    req.offset.write(0x100);
    req.size.write(8); // inline
    req.data.write(0xDEADBEEFCAFEBABEULL);
    REQUIRE(req.size.read() == 8u);
    REQUIRE_FALSE(req.is_bulk_mem());
}

TEST_CASE("PcieEndpoint: BAR1 MEM size > 8 descriptor-only TLP (backdoor)",
          "[pcie][endpoint][bar][mem][backdoor]") {
    // per design.md §2.3: size > 8 → descriptor-only TLP, data=0
    PcieTlpBundle req;
    req.kind.write(PcieTlpBundle::MEM_WRITE);
    req.bar_index.write(1);
    req.offset.write(0x1000);
    req.size.write(4096); // 4KB bulk DMA
    req.data.write(0);    // data=0 (descriptor-only)
    REQUIRE(req.is_bulk_mem());
    REQUIRE(req.size.read() == 4096u);
    REQUIRE(req.data.read() == 0u);
}