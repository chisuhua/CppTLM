// test/test_pin_contract_pcie_controller_ip.cc
// Pin contract test for PcieControllerIP (B1 PR-B Phase Gate).
//
// Per docs/superpowers/plans/2026-10-06-dgpu-realip-refactor.md Task 13 (B1)
// + session handoff §6 (B1a, 锁行为).
//
// Purpose:
//   Pin the behavioral contract of PcieControllerIP that B1b will establish
//   (composition refactor: PcieControllerIP : public SimModule + non-owning
//   PcieEndpointIP* ep_ + attach_endpoint() setter). 5 stimuli cover:
//     1. bar=0                            -> Bar0Mmio (whole aperture)
//     2. bar=1, offset <  bar_size(1)     -> Bar1FbFastPath
//     3. bar=1, offset >= bar_size(1)     -> Unknown  (B2d regression guard)
//     4. bar=2 (any offset)               -> Bar2VramAperture
//     5. bar=3                            -> Unknown  (BAR3 = upper dword)
//
// MUST-DO in setup (per handoff §6 B1a 必做):
//   - ctrl.attach_endpoint(&ep)  [currently does not exist → TDD compile RED]
//   - board.set_pcie_controller_ip(&ctrl)  [activates dormant B2 gate in mmio paths]
//
// Dormant-gate trap: if attach_endpoint() is omitted, stimulus 2 will always
// return Unknown because ep_ stays nullptr and bar_size(1) collapses to 0
// (P1 regression per Metis hidden-req #4 / Oracle bg_9c11d0e8).
//
// TDD state:
//   B1a (Red):   attach_endpoint() missing -> compile failure (this file).
//   B1b (Green): method added + composition refactor -> 5 stimuli pass.
//
// Tag: `[controller_ip][p2]`
//
// Author: Architecture WG / Date: 2026-10-07
// Status: RED (compile failure expected)

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/pcie/pcie_controller_ip.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"

#include <nlohmann/json.hpp>

using ::tlm::pcie::PcieControllerIP;
using ::tlm::pcie::PcieEndpointIP;
using BT = ::tlm::pcie::PcieControllerIP::BarTarget;
using json = nlohmann::json;

namespace {

// 单实例共享配置: BAR1 = 64 KiB framebuffer window; BAR2 = 8 GiB VRAM aperture.
// BAR3..BAR5 不参与契约 (BAR3 是 BAR2 64-bit pair upper dword, BAR4-5 unused).
constexpr uint64_t kBar1Size = 0x10000ULL;            // 64 KiB
constexpr uint64_t kBar2Size = 0x200000000ULL;        // 8 GiB

// Pinned contract harness (per stimulus set in dgpu_board_shell.cc:316 / :434):
//   "expected mirrors controller semantics (pcie_controller_ip.cc)"
struct PinnedHarness {
    ::EventQueue eq;
    PcieEndpointIP ep;
    ::tlm::gpu::DGpuBoard board;
    PcieControllerIP ctrl;

    PinnedHarness()
        : eq{},
          ep{"pcie_ep_ip_pinned", &eq},
          board{"board_pinned", &eq},
          ctrl{"ctrl_pinned", &eq} {
        // Configure BAR sizes via JSON path (canonical source of truth per
        // PcieEndpointIP::attach_composition §bar_sizes handler).
        json cfg;
        cfg["bar_sizes"] = json::array({0ULL, kBar1Size, kBar2Size, 0ULL, 0ULL, 0ULL});
        ep.set_config(cfg);

        // 断言 BAR sizes 真的被写进了 EP (否则下游 match_bar 走错路径)
        REQUIRE(ep.bar_size(1) == kBar1Size);
        REQUIRE(ep.bar_size(2) == kBar2Size);

        // ===== B1a MUST-DO #1: attach EP into controller (RED: method 尚未存在) =====
        ctrl.attach_endpoint(&ep);

        // ===== B1a MUST-DO #2: inject controller into DGpuBoard (dormant B2 gate) =====
        board.set_pcie_controller_ip(&ctrl);
    }
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────
// Stimulus 1: bar=0 → Bar0Mmio (whole BAR0 aperture)
// Per Oracle bg_c60653e0 §2.1: BAR0 sub-dispatch REMOVED. Controller facade
// returns Bar0Mmio for whole 4KB aperture; GMMU/Display split belongs to
// target-side (DGpuBoard / P3 fabric).
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieControllerIP: bar=0 returns Bar0Mmio (whole aperture, no sub-dispatch)",
          "[controller_ip][p2]") {
    PinnedHarness s;

    BT target = BT::Unknown;
    uint64_t sub_offset = 0;
    const bool matched = s.ctrl.match_bar(0, 0x123ULL, target, sub_offset);

    REQUIRE(matched);
    REQUIRE(target == BT::Bar0Mmio);
    REQUIRE(sub_offset == 0x123ULL);
}

// ─────────────────────────────────────────────────────────────────────────
// Stimulus 2: bar=1, offset < bar_size(1) → Bar1FbFastPath
// 关键契约: 若 attach_endpoint 未被调 (ep_=null),bar_size(1) 退化为 0,
// 任何 offset >= 0 都触发 stimulus 3 的 Unknown 路径 (B2d dormancy trap).
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieControllerIP: bar=1 offset < bar_size(1) returns Bar1FbFastPath",
          "[controller_ip][p2]") {
    PinnedHarness s;

    BT target = BT::Unknown;
    uint64_t sub_offset = 0;
    // 0x5000 < kBar1Size (0x10000) → 在 framebuffer window 内
    const bool matched = s.ctrl.match_bar(1, 0x5000ULL, target, sub_offset);

    REQUIRE(matched);
    REQUIRE(target == BT::Bar1FbFastPath);
    REQUIRE(sub_offset == 0x5000ULL);
}

// ─────────────────────────────────────────────────────────────────────────
// Stimulus 3: bar=1, offset >= bar_size(1) → Unknown
// B2d regression guard (per handoff §4). dgpu_board_shell.cc:316 expected
// 也镜像 controller bounds;doorbell 0x10010000 / OOB 不应被误判为 fb.
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieControllerIP: bar=1 offset >= bar_size(1) returns Unknown (B2d guard)",
          "[controller_ip][p2]") {
    PinnedHarness s;

    BT target = BT::Bar0Mmio;  // sentinel to verify write happened
    uint64_t sub_offset = 0;

    // Exactly at boundary (offset == bar_size(1)) — 必须 NOT matched
    const bool at_boundary = s.ctrl.match_bar(1, kBar1Size, target, sub_offset);
    REQUIRE_FALSE(at_boundary);
    REQUIRE(target == BT::Unknown);

    // Far beyond (模拟 doorbell 0x10010000 / OOB 场景)
    const bool far_oob = s.ctrl.match_bar(1, 0x10000000ULL, target, sub_offset);
    REQUIRE_FALSE(far_oob);
    REQUIRE(target == BT::Unknown);

    // 高位 doorbell offset — handoff §9 提及的 0x10010000 合成偏移
    const bool doorbell = s.ctrl.match_bar(1, 0x10010000ULL, target, sub_offset);
    REQUIRE_FALSE(doorbell);
    REQUIRE(target == BT::Unknown);
}

// ─────────────────────────────────────────────────────────────────────────
// Stimulus 4: bar=2 → Bar2VramAperture (any offset)
// No size check in controller (ResizableBar may change dynamically);
// PcieMemoryDevice applies its own bound check per ADR-DGPU-05.
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieControllerIP: bar=2 returns Bar2VramAperture (any offset, no bound)",
          "[controller_ip][p2]") {
    PinnedHarness s;

    BT target = BT::Unknown;
    uint64_t sub_offset = 0;
    const bool matched = s.ctrl.match_bar(2, 0x1000ULL, target, sub_offset);

    REQUIRE(matched);
    REQUIRE(target == BT::Bar2VramAperture);
    REQUIRE(sub_offset == 0x1000ULL);
}

// ─────────────────────────────────────────────────────────────────────────
// Stimulus 5: bar=3 → Unknown (BAR3 is upper dword of BAR2 64-bit pair)
// Per H2 (Metis bg_899dfdcc + Oracle bg_fb84a648): BAR3 是 BAR2 64-bit pair
// 的高 dword, 永远不应该被独立访问.
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieControllerIP: bar=3 returns Unknown (BAR3 = upper dword of BAR2 pair)",
          "[controller_ip][p2]") {
    PinnedHarness s;

    BT target = BT::Bar0Mmio;  // sentinel
    uint64_t sub_offset = 0;
    const bool matched = s.ctrl.match_bar(3, 0x0ULL, target, sub_offset);

    REQUIRE_FALSE(matched);
    REQUIRE(target == BT::Unknown);
}

// ─────────────────────────────────────────────────────────────────────────
// 隐藏契约: DGpuBoard::set_pcie_controller_ip 激活 dormant B2 闸门
// dgpu_board_shell.cc:320 + :433 conditional `if (controller_ip_)` 是闸门.
// 没注入则 mmio_read/write 走 legacy flag-based dispatch,不 consult facade.
// 此测试用 controller_ip() accessor 验证 setter 生效.
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieControllerIP: DGpuBoard::controller_ip() returns injected pointer",
          "[controller_ip][p2][b2-gate]") {
    PinnedHarness s;

    REQUIRE(s.board.controller_ip() == &s.ctrl);
    REQUIRE(s.board.controller_ip() != nullptr);
}