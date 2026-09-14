// test/test_p2p_dma.cc
// P2P DMA 路由 + ACS 拒绝 (Stage 2.1 §2.1+2.2) - INV-D no silent fallback
// per openspec/changes/2026-09-10-cpptlm-stage-1-4-2-1/design.md §2.1+2.2
//
// 标签: [pcie] [p2p] + 子标签 [acs] [epperm] [noroute]
//
// 作者 CppTLM Team / 日期 2027-02-09
#include "catch_amalgamated.hpp"

#include "tlm/pcie/pcie_bypass_mux_p2p.hh"

#include <cstdint>

using tlm::pcie::p2p_dma_route;
using tlm::pcie::P2PResult;

namespace {

constexpr uint32_t BDF_PF0 = 0x0000;  // PF0
constexpr uint32_t BDF_VF1 = 0x0001;  // VF1 (function 1)
constexpr uint32_t BDF_PF1 = 0x0100;  // Bus 1
} // namespace

TEST_CASE("P2P DMA route: cross-BDF route blocked by ACS (strict default)",
          "[pcie][p2p][ok]") {
    // 用非零 BDF 避免触发 NO_ROUTE 分支; 当前 MVP: 所有跨 BDF 拒绝
    const auto r = p2p_dma_route(BDF_PF1, 0x0200, 0x100000, 4096);
    REQUIRE(r.ok() == false);
    REQUIRE(r.code == P2PResult::Code::BLOCKED_BY_ACS);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: INV-D — invalid BDF returns NOROUTE, never silent
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("P2P DMA route: invalid BDF returns NOROUTE explicitly (INV-D)",
          "[pcie][p2p][noroute]") {
    SECTION("src_bdf=0 returns NOROUTE") {
        const auto r = p2p_dma_route(0, BDF_VF1, 0x1000, 256);
        REQUIRE(r.ok() == false);
        REQUIRE(r.code == P2PResult::Code::NO_ROUTE);
    }

    SECTION("dst_bdf=0 returns NOROUTE") {
        const auto r = p2p_dma_route(BDF_PF0, 0, 0x1000, 256);
        REQUIRE(r.ok() == false);
        REQUIRE(r.code == P2PResult::Code::NO_ROUTE);
    }

    SECTION("same src/dst returns NOROUTE (self-route meaningless)") {
        const auto r = p2p_dma_route(BDF_PF0, BDF_PF0, 0x1000, 256);
        REQUIRE(r.ok() == false);
        REQUIRE(r.code == P2PResult::Code::NO_ROUTE);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: INV-D — every failure path returns explicit code, never silent
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("P2P DMA route: result code is always explicitly set (INV-D)",
          "[pcie][p2p][epperm]") {
    // 1000 次随机参数: 每次都返回 OK / EPERM / NOROUTE 之一 (无默认/无异常)
    for (int i = 0; i < 1000; ++i) {
        const uint32_t src = static_cast<uint32_t>(i * 31 + 1);
        const uint32_t dst = static_cast<uint32_t>(i * 17 + 100);
        const auto r = p2p_dma_route(src, dst, i * 4096, 4096);
        const bool valid = (r.code == P2PResult::Code::SUCCESS) ||
                            (r.code == P2PResult::Code::BLOCKED_BY_ACS) ||
                            (r.code == P2PResult::Code::NO_ROUTE);
        REQUIRE(valid);
        // INV-D: result 必须是显式 code, 不可 invalid/silent
        REQUIRE(r.code != static_cast<P2PResult::Code>(99));
    }
}
