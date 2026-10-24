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

// ── T3 (RED): handle_functional_request() 同步入口 ──
// per Requirement vc-functional-mode-data-path Scenario "SDMA H2D 经 VramController functional-mode
// 路径" + design.md A-B1 (模仿 gem5 AbstractMemory::functionalAccess() 同步 API):
//   FunctionalZeroDelay → latency=0, 字节级 memcpy 到 backing
TEST_CASE("[vram_controller] functional: handle_functional_request byte-level rw + latency 0",
          "[vram_controller][functional][request][minimal_dgpu_soc]") {
    EventQueue eq;
    VramControllerTLM vc("vram", &eq);
    std::vector<uint8_t> backing(8192, 0);
    vc.set_backing_view(backing.data(), backing.size());
    vc.set_mode(VramControllerMode::FunctionalZeroDelay);

    const uint64_t payload = 0x1122334455667788ULL;
    auto wr = vc.handle_functional_request(0x100, 8, /*is_write=*/true, &payload);
    REQUIRE(wr.rc == 0);
    REQUIRE(wr.latency_cycles == 0); // functional-mode zero-delay
    REQUIRE(backing[0x100] == 0x88); // byte-level memcpy 落 backing (LE)

    uint64_t back = 0;
    auto rd = vc.handle_functional_request(0x100, 8, /*is_write=*/false, &back);
    REQUIRE(rd.rc == 0);
    REQUIRE(back == payload);
}
