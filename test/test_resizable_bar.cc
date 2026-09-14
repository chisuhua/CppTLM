// test/test_resizable_bar.cc
// Resizable BAR state machine + INV-C disable/reprogram/enable (Stage 2.1 §2.3)
// per openspec/changes/2026-09-10-cpptlm-stage-1-4-2-1/design.md §2.3
//
// 标签: [pcie] [bar] + 子标签 [resizable] [inv-c]
//
// 作者 CppTLM Team / 日期 2027-02-09
#include "catch_amalgamated.hpp"

#include "tlm/pcie/pcie_resizable_bar.hh"

using tlm::pcie::ResizableBar;

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: Initial state is Disabled
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("ResizableBar: initial state is Disabled",
          "[pcie][bar][resizable]") {
    ResizableBar bar;
    REQUIRE(bar.state() == ResizableBar::State::Disabled);
    REQUIRE_FALSE(bar.enabled());
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: INV-C — reprogram requires Disabled state
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("ResizableBar: reprogram rejected when Enabled (INV-C)",
          "[pcie][bar][inv-c]") {
    ResizableBar bar;
    REQUIRE(bar.reprogram_size(0x100000));
    REQUIRE(bar.enable());
    REQUIRE(bar.enabled());

    // INV-C: 已 Enabled 时 reprogram 必须拒绝, 必须先 disable
    REQUIRE_FALSE(bar.reprogram_size(0x200000));
    REQUIRE(bar.state() == ResizableBar::State::Enabled);

    // 必须 disable 然后 reprogram
    bar.disable();
    REQUIRE(bar.state() == ResizableBar::State::Disabled);
    REQUIRE(bar.reprogram_size(0x200000));
    REQUIRE(bar.state() == ResizableBar::State::Programming);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: enable requires Programming state (post-reprogram)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("ResizableBar: enable rejected without prior reprogram",
          "[pcie][bar][resizable]") {
    ResizableBar bar;

    // 直接 enable 失败 (未 reprogram)
    REQUIRE_FALSE(bar.enable());
    REQUIRE(bar.state() == ResizableBar::State::Disabled);

    // reprogram → enable
    REQUIRE(bar.reprogram_size(0x40000));
    REQUIRE(bar.state() == ResizableBar::State::Programming);
    REQUIRE(bar.enable());
    REQUIRE(bar.enabled());
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: full disable/reprogram/enable cycle
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("ResizableBar: full disable→reprogram→enable cycle",
          "[pcie][bar][resizable]") {
    ResizableBar bar;

    // 1. Initial disabled
    REQUIRE(bar.state() == ResizableBar::State::Disabled);

    // 2. Program to 1MB
    REQUIRE(bar.reprogram_size(0x100000));
    REQUIRE(bar.size_bytes() == 0x100000);
    REQUIRE(bar.state() == ResizableBar::State::Programming);

    // 3. Enable
    REQUIRE(bar.enable());
    REQUIRE(bar.enabled());

    // 4. Disable to resize again
    bar.disable();
    REQUIRE_FALSE(bar.enabled());

    // 5. Resize to 256MB (must succeed after disable)
    REQUIRE(bar.reprogram_size(0x10000000));
    REQUIRE(bar.size_bytes() == 0x10000000);

    // 6. Enable
    REQUIRE(bar.enable());
    REQUIRE(bar.enabled());
    REQUIRE(bar.size_bytes() == 0x10000000);  // size 保持
}
