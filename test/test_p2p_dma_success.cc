// test/test_p2p_dma_success.cc
// P2P DMA SUCCESS 路径 + AcsPolicy (followups §2)
// per openspec/changes/2027-02-09-cpptlm-stage-1-4-2-1-followups/design.md §2
// 勘误: nullptr → strict (BLOCKED), 向后兼容既有 test_p2p_dma.cc
//
// 标签: [pcie] [p2p] [acs-policy]
//
// 作者 CppTLM Team / 日期 2027-02-09
#include "catch_amalgamated.hpp"

#include "tlm/pcie/pcie_bypass_mux_p2p.hh"

#include <cstdint>

using tlm::pcie::AcsPolicy;
using tlm::pcie::p2p_dma_route;
using tlm::pcie::P2PResult;

namespace {
constexpr uint32_t BDF_PF1 = 0x0100;
constexpr uint32_t BDF_DEV2 = 0x0200;
constexpr uint32_t BDF_DEV3 = 0x0300;
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: default AcsPolicy (empty grants) → BLOCKED (backward compat)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("P2P DMA: default AcsPolicy keeps strict-deny (backward compat)",
          "[pcie][p2p][acs-policy]") {
    AcsPolicy policy;
    const auto r = p2p_dma_route(BDF_PF1, BDF_DEV2, 0x100000, 4096, &policy);
    REQUIRE(r.ok() == false);
    REQUIRE(r.code == P2PResult::Code::BLOCKED_BY_ACS);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: explicit grant → SUCCESS
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("P2P DMA: explicit AcsPolicy grant enables SUCCESS path",
          "[pcie][p2p][acs-policy]") {
    AcsPolicy policy;
    policy.grant(BDF_PF1, BDF_DEV2);

    const auto r = p2p_dma_route(BDF_PF1, BDF_DEV2, 0x100000, 4096, &policy);
    REQUIRE(r.ok() == true);
    REQUIRE(r.code == P2PResult::Code::SUCCESS);

    // 其他 BDF pair 不受影响
    const auto r2 = p2p_dma_route(BDF_PF1, BDF_DEV3, 0x100000, 4096, &policy);
    REQUIRE(r2.code == P2PResult::Code::BLOCKED_BY_ACS);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: nullptr → strict (backward compat with existing callers)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("P2P DMA: nullptr policy keeps BLOCKED (INV-D no silent fallback)",
          "[pcie][p2p][acs-policy]") {
    const auto r = p2p_dma_route(BDF_PF1, BDF_DEV2, 0x100000, 4096);
    REQUIRE(r.ok() == false);
    REQUIRE(r.code == P2PResult::Code::BLOCKED_BY_ACS);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: fuzz with policy — result always explicit
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("P2P DMA: fuzz with AcsPolicy — result always explicit code",
          "[pcie][p2p][acs-policy]") {
    AcsPolicy policy;
    policy.grant(0x0100, 0x0200);

    for (int i = 0; i < 1000; ++i) {
        const uint32_t src = static_cast<uint32_t>(i * 31 + 1);
        const uint32_t dst = static_cast<uint32_t>(i * 17 + 100);
        const auto r = p2p_dma_route(src, dst, i * 4096, 4096, &policy);
        const bool valid = (r.code == P2PResult::Code::SUCCESS) ||
                            (r.code == P2PResult::Code::BLOCKED_BY_ACS) ||
                            (r.code == P2PResult::Code::NO_ROUTE);
        REQUIRE(valid);
    }
}
