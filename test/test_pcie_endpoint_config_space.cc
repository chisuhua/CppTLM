// test_pcie_endpoint_config_space.cc
// PcieEndpointTLM: Config Space + capability chain 测试 (PE-G2)
// Author: CppTLM Team
// Date: 2026-08-26
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/design.md §6
//       spec.md Scenario "Capability chain walk"

#include "catch_amalgamated.hpp"
#include "tlm/gpu/pcie_endpoint_tlm.h"
#include "tlm/gpu/pcie_config_space_mvp.hh"
#include "bundles/pcie_bundles_tlm.hh"
#include "core/event_queue.hh"
#include "framework/stream_adapter.hh"

using namespace tlm::gpu;
using namespace bundles;

TEST_CASE("PcieEndpoint: Config Space defaults (vendor/device_id)", "[pcie][endpoint][config]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 默认 vendor=0x10DE (NVIDIA), device=0x1234 (per s2 DGpuBar)
    REQUIRE(ep.config_space().read(0x00) == 0x123410DEu);
    // Revision = 0x01, header type = 0x00 (Type 0)
    REQUIRE((ep.config_space().read(0x08) & 0xFFu) == 0x01u);
}

TEST_CASE("PcieEndpoint: Config Space OOB read returns 0xFFFFFFFF", "[pcie][endpoint][config]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 4096B config size: offset 0x1000 越界 → 0xFFFFFFFF
    REQUIRE(ep.config_space().read(0x1000) == 0xFFFFFFFFu);
    // 未对齐 offset → 0xFFFFFFFF
    REQUIRE(ep.config_space().read(0x01) == 0xFFFFFFFFu);
}

TEST_CASE("PcieEndpoint: Config Space capability chain walk", "[pcie][endpoint][config]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 添加 3 个 capability：MSI-X (id=17) → PCIe (id=16) → AER (id=1) → end
    REQUIRE(ep.config_space().add_capability(17, 0x40, 0x50, 0xAB00) == true);
    REQUIRE(ep.config_space().add_capability(16, 0x50, 0x60, 0x0002) == true);
    REQUIRE(ep.config_space().add_capability(1,  0x60, 0x00, 0x0000) == true);

    // capability_count = 3
    REQUIRE(ep.config_space().capability_count() == 3u);

    // capability pointer (offset 0x34) 指向第一个：offset 0x40
    REQUIRE((ep.config_space().read(0x34) & 0xFFu) == 0x40u);

    // walk chain：通过 next 字段连接
    const auto* c0 = ep.config_space().get_capability(0);
    REQUIRE(c0 != nullptr);
    REQUIRE(c0->id == 17);  // MSI-X
    REQUIRE(c0->next == 0x50);

    const auto* c1 = ep.config_space().get_capability(1);
    REQUIRE(c1->id == 16);  // PCIe
    REQUIRE(c1->next == 0x60);

    const auto* c2 = ep.config_space().get_capability(2);
    REQUIRE(c2->id == 1);   // AER
    REQUIRE(c2->next == 0x00);  // chain end
}

TEST_CASE("PcieEndpoint: Config Space RO field protection", "[pcie][endpoint][config]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    const uint32_t original = ep.config_space().read(0x00);
    ep.config_space().write(0x00, 0xDEADBEEFu);
    // vendor_id (offset 0x00) is RO
    REQUIRE(ep.config_space().read(0x00) == original);
}

TEST_CASE("PcieEndpoint: num_ports() returns 4", "[pcie][endpoint][config]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    REQUIRE(ep.num_ports() == 4u);
}