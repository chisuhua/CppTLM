// test/test_soc_internal_noc.cc
// SocInternalNoc 测试覆盖 (per cpptlm-dgpu-soc-internal-noc-minimal Change, 2026-10-08)
// tag: [realip][pin-contract][noc] (防 vacuous pass per handoff §9.2 #10)
#include "bundles/noc_bundles_tlm.hh"
#include "core/event_queue.hh"
#include "tlm/cluster/soc_internal_noc.hh"
#include "utils/port_contract_asserts.hh"
#include <catch_amalgamated.hpp>
#include <cstdint>

using tlm::cluster::SocInternalNoc;
using bundles::NoCFlitBundle;

// 辅助: 把整数地址 reinterpret 成目标类型 (target 类不实际构造, 仅作指针值测试)
static tlm::pcie::PcieControllerIP* as_pcie(uintptr_t v) { return reinterpret_cast<tlm::pcie::PcieControllerIP*>(v); }
static tlm::gpu::SdmaEngineTLM* as_sdma(uintptr_t v) { return reinterpret_cast<tlm::gpu::SdmaEngineTLM*>(v); }
static tlm::gpu::GmmuTLM* as_gmmu(uintptr_t v) { return reinterpret_cast<tlm::gpu::GmmuTLM*>(v); }
static VramControllerTLM* as_vram(uintptr_t v) { return reinterpret_cast<VramControllerTLM*>(v); }

// ── 1) 4 NODE_ID_* 唯一性 + 顺序 (编译期) ──
TEST_CASE("SocInternalNoc: 4 NODE_ID constants are unique + monotone",
          "[realip][pin-contract][noc]") {
    STATIC_REQUIRE(SocInternalNoc::NODE_ID_PCIE == 0);
    STATIC_REQUIRE(SocInternalNoc::NODE_ID_SDMA == 1);
    STATIC_REQUIRE(SocInternalNoc::NODE_ID_GMMU == 2);
    STATIC_REQUIRE(SocInternalNoc::NODE_ID_VRAM == 3);
    STATIC_REQUIRE(SocInternalNoc::NODE_ID_PCIE != SocInternalNoc::NODE_ID_SDMA);
    STATIC_REQUIRE(SocInternalNoc::NODE_ID_PCIE != SocInternalNoc::NODE_ID_GMMU);
    STATIC_REQUIRE(SocInternalNoc::NODE_ID_PCIE != SocInternalNoc::NODE_ID_VRAM);
    STATIC_REQUIRE(SocInternalNoc::NODE_ID_SDMA != SocInternalNoc::NODE_ID_GMMU);
    STATIC_REQUIRE(SocInternalNoc::NODE_ID_SDMA != SocInternalNoc::NODE_ID_VRAM);
    STATIC_REQUIRE(SocInternalNoc::NODE_ID_GMMU != SocInternalNoc::NODE_ID_VRAM);
}

// ── 2) 4 setter + accessor 双向 ──
TEST_CASE("SocInternalNoc: 4 setters + accessors are independent",
          "[realip][pin-contract][noc]") {
    EventQueue eq;
    SocInternalNoc soc_noc("soc_noc", &eq);
    REQUIRE(soc_noc.pcie_target() == nullptr);
    REQUIRE(soc_noc.sdma_target() == nullptr);
    REQUIRE(soc_noc.gmmu_target() == nullptr);
    REQUIRE(soc_noc.vram_target() == nullptr);
    REQUIRE(soc_noc.flits_routed() == 0);
    REQUIRE(soc_noc.unknown_dst_count() == 0);

    soc_noc.set_pcie_target(as_pcie(0x1000));
    REQUIRE(soc_noc.pcie_target() == as_pcie(0x1000));
    REQUIRE(soc_noc.sdma_target() == nullptr);

    soc_noc.set_pcie_target(nullptr);
    REQUIRE(soc_noc.pcie_target() == nullptr);

    soc_noc.set_vram_target(as_vram(0x4000));
    soc_noc.set_sdma_target(as_sdma(0x2000));
    REQUIRE(soc_noc.vram_target() == as_vram(0x4000));
    REQUIRE(soc_noc.sdma_target() == as_sdma(0x2000));
}

// ── 3+4) dst 路由: 4 路径, flits_routed++ ──
TEST_CASE("SocInternalNoc: route_flit() by dst_node (4 valid paths)",
          "[realip][pin-contract][noc]") {
    EventQueue eq;
    SocInternalNoc soc_noc("soc_noc", &eq);
    soc_noc.set_pcie_target(as_pcie(0x1));
    soc_noc.set_sdma_target(as_sdma(0x2));
    soc_noc.set_gmmu_target(as_gmmu(0x3));
    soc_noc.set_vram_target(as_vram(0x4));

    auto make_flit_with_dst = [](uint32_t dst) {
        NoCFlitBundle flit{};
        flit.dst_node.write(dst);
        return flit;
    };

    soc_noc.route_flit(make_flit_with_dst(SocInternalNoc::NODE_ID_PCIE));
    REQUIRE(soc_noc.flits_routed() == 1);
    soc_noc.route_flit(make_flit_with_dst(SocInternalNoc::NODE_ID_SDMA));
    REQUIRE(soc_noc.flits_routed() == 2);
    soc_noc.route_flit(make_flit_with_dst(SocInternalNoc::NODE_ID_GMMU));
    REQUIRE(soc_noc.flits_routed() == 3);
    soc_noc.route_flit(make_flit_with_dst(SocInternalNoc::NODE_ID_VRAM));
    REQUIRE(soc_noc.flits_routed() == 4);
    REQUIRE(soc_noc.unknown_dst_count() == 0);
}

// ── 5) dst=99 (未知) → unknown_dst_count++, 0 crash ──
TEST_CASE("SocInternalNoc: route_flit() unknown dst_node graceful skip",
          "[realip][pin-contract][noc]") {
    EventQueue eq;
    SocInternalNoc soc_noc("soc_noc", &eq);

    NoCFlitBundle flit{};
    flit.dst_node.write(99);
    soc_noc.route_flit(flit);
    REQUIRE(soc_noc.flits_routed() == 0);
    REQUIRE(soc_noc.unknown_dst_count() == 1);
    soc_noc.route_flit(flit);
    soc_noc.route_flit(flit);
    REQUIRE(soc_noc.unknown_dst_count() == 3);
}

// ── 6) nullptr target + 路由触发 → 0 crash ──
TEST_CASE("SocInternalNoc: route_flit() nullptr target is no-op",
          "[realip][pin-contract][noc]") {
    EventQueue eq;
    SocInternalNoc soc_noc("soc_noc", &eq);
    NoCFlitBundle flit{};

    flit.dst_node.write(SocInternalNoc::NODE_ID_PCIE);
    soc_noc.route_flit(flit);
    flit.dst_node.write(SocInternalNoc::NODE_ID_SDMA);
    soc_noc.route_flit(flit);
    flit.dst_node.write(SocInternalNoc::NODE_ID_GMMU);
    soc_noc.route_flit(flit);
    flit.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    soc_noc.route_flit(flit);

    REQUIRE(soc_noc.flits_routed() == 4);
    REQUIRE(soc_noc.unknown_dst_count() == 0);
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, dst_node, 32);
}
