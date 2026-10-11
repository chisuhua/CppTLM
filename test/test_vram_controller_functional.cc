// test/test_vram_controller_functional.cc
// Change A (cpptlm-vram-controller-functional-integration): VramControllerTLM functional-mode
// 集成测试 — set_mode() API + handle_functional_request() 同步入口 + PcieMemoryDevice 委托 +
// minimal_v1 board functional-mode E2E.
//
// 关联: openspec/changes/cpptlm-vram-controller-functional-integration/
//         specs/vram-controller-functional-mode/spec.md (Requirement vc-mode-configuration)
// 作者: CppTLM Team · 日期: 2026-10-09
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/vram_controller_tlm.hh"

#include <cstdint>
#include <vector>

using tlm::vram::VramControllerMode;

// ── T1 (RED): 默认 mode = TimingCycleApproximate (backward compat) ──
// per Requirement vc-mode-configuration Scenario "默认 mode = TimingCycleApproximate"
TEST_CASE("[vram_controller] functional: default mode = TimingCycleApproximate (backward compat)",
          "[vram_controller][functional][mode][minimal_dgpu_soc]") {
    EventQueue eq;
    VramControllerTLM vc("vram", &eq);
    REQUIRE(vc.mode() == VramControllerMode::TimingCycleApproximate);
}

// ── T1 (RED): set_mode(FunctionalZeroDelay) 切换生效 ──
// per Requirement vc-mode-configuration Scenario "minimal_v1 functional-mode 显式激活"
TEST_CASE("[vram_controller] functional: set_mode switches to FunctionalZeroDelay",
          "[vram_controller][functional][mode][minimal_dgpu_soc]") {
    EventQueue eq;
    VramControllerTLM vc("vram", &eq);
    vc.set_mode(VramControllerMode::FunctionalZeroDelay);
    REQUIRE(vc.mode() == VramControllerMode::FunctionalZeroDelay);
    vc.set_mode(VramControllerMode::TimingCycleApproximate);
    REQUIRE(vc.mode() == VramControllerMode::TimingCycleApproximate);
}
