// test/test_acs_extended_cap.cc
// ACS Extended Capability (id=0x000D) 安装 + Control dword RMW (followups §5)
// per openspec/changes/2027-02-09-cpptlm-stage-1-4-2-1-followups/design.md §5
// 勘误: header=0x0001000D, offset=0x100, Control Reg 走 offset+4 dword RMW
//
// 标签: [pcie] [acs] [extended-cap]
//
// 作者 CppTLM Team / 日期 2027-02-09
#include "catch_amalgamated.hpp"

#include "tlm/pcie/pcie_acs_extended_cap.hh"
#include "tlm/gpu/pcie_config_space_mvp.hh"

using tlm::gpu::PcieConfigSpace;
using tlm::pcie::install_acs_extended_cap;
using tlm::pcie::set_acs_bit_enabled;

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: install_acs_extended_cap writes standard header (0x0001000D)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("ACS Extended Cap: install writes standard header at 0x100",
          "[pcie][acs][extended-cap]") {
    PcieConfigSpace cfg;  // 默认 config_size=4096
    cfg.init();

    REQUIRE(install_acs_extended_cap(cfg, 0x100));
    // 勘误: header=0x0001000D (id=0x000D, version=1, next=0); 非 0x000D0001
    REQUIRE(cfg.read(0x100) == 0x0001000Du);
    // Cap+Control dword 全 0
    REQUIRE(cfg.read(0x104) == 0x00000000u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: ACS Control V bit enable via dword RMW
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("ACS Extended Cap: V bit enable via dword RMW",
          "[pcie][acs][extended-cap]") {
    PcieConfigSpace cfg;
    cfg.init();
    REQUIRE(install_acs_extended_cap(cfg, 0x100));

    // V bit = ACS Control Reg bit 0 = dword 高 16-bit bit0 = 绝对 bit 16
    REQUIRE(set_acs_bit_enabled(cfg, 0x100, 16, true));
    REQUIRE((cfg.read(0x104) & 0x00010000u) == 0x00010000u);

    // disable
    REQUIRE(set_acs_bit_enabled(cfg, 0x100, 16, false));
    REQUIRE((cfg.read(0x104) & 0x00010000u) == 0u);

    // bit 17 (Translation Blocking) 独立
    REQUIRE(set_acs_bit_enabled(cfg, 0x100, 17, true));
    REQUIRE((cfg.read(0x104) & 0x00020000u) == 0x00020000u);
    REQUIRE((cfg.read(0x104) & 0x00010000u) == 0u);  // V bit 仍 0
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: install fails when extended space insufficient
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("ACS Extended Cap: install rejected with 256B config space",
          "[pcie][acs][extended-cap]") {
    PcieConfigSpace cfg(256);  // PCI 1.x 兼容: 无扩展空间
    cfg.init();
    REQUIRE_FALSE(install_acs_extended_cap(cfg, 0x100));
}
