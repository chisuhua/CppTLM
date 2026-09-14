// test/test_resizable_bar_integration.cc
// ResizableBar 集成 PcieEndpointIP + INV-G 越界校验 (followups §3)
// per openspec/changes/2027-02-09-cpptlm-stage-1-4-2-1-followups/design.md §3
// 勘误: erase-it 惯用法 + enable_resizable_bar wrapper + INV-C disable 序列
//
// 标签: [pcie] [bar] [resizable] [integration]
//
// 作者 CppTLM Team / 日期 2027-02-09
#include "catch_amalgamated.hpp"

#include "tlm/pcie/pcie_endpoint_ip.hh"

#include "chstream_register.hh"
#include "core/event_queue.hh"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <string>

using json = nlohmann::json;
using tlm::pcie::PcieEndpointIP;

namespace {

struct RbFixture {
    EventQueue eq;
    std::unique_ptr<PcieEndpointIP> ep;
    std::string ep_name;

    RbFixture() : ep_name("pcie_ep_rb_test_" + std::to_string(next_id())) {
        ep = std::make_unique<PcieEndpointIP>(ep_name, &eq);
        ep->init();
    }

    static int next_id() {
        static std::atomic<int> s{0};
        return s.fetch_add(1);
    }

    bool warns_about(const std::string& substr) const {
        const auto& w = ep->config_warnings();
        return std::any_of(w.begin(), w.end(),
            [&](const std::string& s) { return s.find(substr) != std::string::npos; });
    }
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: INV-G — bar resize down clears out-of-bounds keys
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("ResizableBar integration: resize down clears out-of-bounds keys (INV-G)",
          "[pcie][bar][resizable][integration]") {
    RbFixture f;

    // 先写一个 BAR entry (模拟 D0 下的 MMIO 写; bar_store_ 只能经 EP 内部写)
    // bar_store_value 是 const getter — 写入经 enable_resizable_bar 后 on_bar_resize
    // 需已有 key 才触发; MVP: 直接 resize down, 空 bar_store_ 无 key 被清除
    // 所以改测: 先注入 key 再 resize down (经 mmio_write ABI 路径)
    f.ep->mmio_write(0, 0x80000, 0xDEADBEEF);

    // resize 到 1MB 并 enable (INV-C: Disabled → Programming → Enabled)
    REQUIRE(f.ep->resizable_bar(0).reprogram_size(0x100000));
    REQUIRE(f.ep->enable_resizable_bar(0));
    REQUIRE(f.ep->resizable_bar(0).enabled());

    // resize down 到 0x100: 必须 disable → reprogram → enable (INV-C)
    f.ep->resizable_bar(0).disable();
    REQUIRE(f.ep->resizable_bar(0).reprogram_size(0x100));
    REQUIRE(f.ep->enable_resizable_bar(0));

    // INV-G: 越界 key 被警告清除
    REQUIRE(f.warns_about("out of bounds"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: 6 independent BAR slots
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("ResizableBar integration: 6 independent BAR slots",
          "[pcie][bar][resizable][integration]") {
    RbFixture f;

    // slot 0 = 4KB, slot 2 = 64KB
    REQUIRE(f.ep->resizable_bar(0).reprogram_size(0x1000));
    REQUIRE(f.ep->enable_resizable_bar(0));
    REQUIRE(f.ep->resizable_bar(2).reprogram_size(0x10000));
    REQUIRE(f.ep->enable_resizable_bar(2));

    // slot 1 未触碰
    REQUIRE(f.ep->resizable_bar(1).size_bytes() == 0);
    REQUIRE_FALSE(f.ep->resizable_bar(1).enabled());

    // slot 0 / 2 状态独立
    REQUIRE(f.ep->resizable_bar(0).size_bytes() == 0x1000);
    REQUIRE(f.ep->resizable_bar(2).size_bytes() == 0x10000);
    REQUIRE(f.ep->resizable_bar(0).enabled());
    REQUIRE(f.ep->resizable_bar(2).enabled());
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: INV-C — reprogram rejected when Enabled (integration level)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("ResizableBar integration: reprogram rejected when Enabled (INV-C)",
          "[pcie][bar][resizable][integration]") {
    RbFixture f;

    REQUIRE(f.ep->resizable_bar(1).reprogram_size(0x200000));
    REQUIRE(f.ep->enable_resizable_bar(1));
    REQUIRE(f.ep->resizable_bar(1).enabled());

    // 已 Enabled 时 reprogram 必须拒绝
    REQUIRE_FALSE(f.ep->resizable_bar(1).reprogram_size(0x400000));
    REQUIRE(f.ep->resizable_bar(1).size_bytes() == 0x200000);  // size 不变
}
