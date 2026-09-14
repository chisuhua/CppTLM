// test/test_aspm.cc
// PciePhyDigitalCtrl ASPM idle-timer L0↔L0s/L1 + INV-B exit latency countdown
// per openspec/changes/2026-09-10-cpptlm-stage-1-4-2-1/design.md §1.2+1.4
//
// 标签: [pcie] [aspm] + 子标签 [l0s] [l1] [exit-latency]
//
// 作者 CppTLM Team / 日期 2027-02-09
#include "catch_amalgamated.hpp"

#include "tlm/pcie/pcie_phy_digital_ctrl_tlm.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"

#include "chstream_register.hh"
#include "core/event_queue.hh"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

using tlm::pcie::PciePhyDigitalCtrl;
using tlm::pcie::LtState;

namespace {

constexpr uint64_t L0S_IDLE_THRESHOLD = 100;
constexpr uint64_t L0S_EXIT_LATENCY   = 4;
constexpr uint64_t L1_IDLE_THRESHOLD  = 4000;
constexpr uint64_t L1_EXIT_LATENCY    = 32;

struct AspmFixture {
    EventQueue eq;
    PciePhyDigitalCtrl* phy = nullptr;  // owned by static registry
    tlm::pcie::PcieLinkLayer* ll = nullptr;  // owned by static registry
    std::string ep_name;

    AspmFixture() : ep_name("pcie_ep_aspm_test_" + std::to_string(next_id())) {
        tlm::pcie::PcieLinkLayerConfig ll_cfg;
        ll_cfg.enabled = true;
        tlm::pcie::PcieLinkLayer::attach_to_endpoint(ep_name, &eq, ll_cfg);
        ll = tlm::pcie::PcieLinkLayer::for_endpoint(ep_name);

        phy = PciePhyDigitalCtrl::attach_to_endpoint(ep_name, &eq);
        REQUIRE(phy != nullptr);
        phy->link_layer(ll);
        // 走完整 LTSSM 训练 Detect→Polling→Configuration→L0
        phy->start_link_training();
        phy->advance_training();
        phy->advance_training();
        phy->advance_training();
        REQUIRE(phy->state() == LtState::L0);
    }

    ~AspmFixture() {
        // 关键: 在 unique_ptr 释放前先从 registry 解绑 (lifetime 由 registry owns)
        if (phy) {
            phy->link_layer(nullptr);
            PciePhyDigitalCtrl::detach_from_endpoint(ep_name);
        }
        tlm::pcie::PcieLinkLayer::detach_from_endpoint(ep_name);
    }

    // 推进 N cycles: 每次 run(1) 推 cycle +1, 然后 tick 一次
    void tick_n(uint64_t n) {
        for (uint64_t i = 0; i < n; ++i) {
            eq.run(1);
            phy->tick();
        }
    }

    static int next_id() {
        static std::atomic<int> s{0};
        return s.fetch_add(1);
    }
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: ASPM disabled by default (no idle-timer)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PciePhyDigitalCtrl: ASPM disabled by default; no auto-L0s",
          "[pcie][aspm]") {
    AspmFixture f;
    REQUIRE(f.phy->state() == LtState::L0);

    // 默认 ASPM 关闭, 即使 idle 大量 cycle 也不进入 L0s
    f.tick_n(L0S_IDLE_THRESHOLD * 10);
    REQUIRE(f.phy->state() == LtState::L0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: enable_aspm(L0s) auto-enters L0s after idle threshold (INV-B)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PciePhyDigitalCtrl: enable_aspm(L0s) auto-enters L0s after idle",
          "[pcie][aspm][l0s]") {
    AspmFixture f;

    f.phy->enable_aspm(PciePhyDigitalCtrl::AspmLevel::L0s);
    REQUIRE(f.phy->aspm_level() == PciePhyDigitalCtrl::AspmLevel::L0s);

    // 边界前: 仍 L0
    f.tick_n(L0S_IDLE_THRESHOLD - 1);
    REQUIRE(f.phy->state() == LtState::L0);

    // 越过 idle threshold: 应自动进入 L0s
    f.tick_n(1);
    REQUIRE(f.phy->state() == LtState::L0s);
    REQUIRE(f.phy->is_link_up() == false);  // 低功耗: link 不可用
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: INV-B — on_traffic triggers exit_pending_ countdown (4 cycles L0s)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PciePhyDigitalCtrl: INV-B L0s exit via on_traffic honors 4-cycle latency",
          "[pcie][aspm][exit-latency]") {
    AspmFixture f;
    f.phy->enable_aspm(PciePhyDigitalCtrl::AspmLevel::L0s);

    // 进入 L0s
    f.tick_n(L0S_IDLE_THRESHOLD);
    REQUIRE(f.phy->state() == LtState::L0s);

    // 触发 traffic → 设 exit_pending_ (INV-B 倒计时)
    f.phy->on_traffic();
    // 立即查询: 状态仍 L0s (倒计时未到)
    REQUIRE(f.phy->state() == LtState::L0s);

    // 推进 < L0S_EXIT_LATENCY: 仍 L0s
    f.tick_n(L0S_EXIT_LATENCY - 1);
    REQUIRE(f.phy->state() == LtState::L0s);

    // 再推 1 cycle: 倒计时完成, 回 L0
    f.tick_n(1);
    REQUIRE(f.phy->state() == LtState::L0);
    REQUIRE(f.phy->is_link_up() == true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: INV-B L1 exit honors 32-cycle latency
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PciePhyDigitalCtrl: INV-B L1 exit via on_traffic honors 32-cycle latency",
          "[pcie][aspm][l1]") {
    AspmFixture f;
    f.phy->enable_aspm(PciePhyDigitalCtrl::AspmLevel::L1);

    // 进入 L1 (L1 idle threshold 比 L0s 长)
    f.tick_n(L1_IDLE_THRESHOLD);
    REQUIRE(f.phy->state() == LtState::L1);

    // on_traffic → exit_pending_ (32 cycle 倒计时)
    f.phy->on_traffic();
    REQUIRE(f.phy->state() == LtState::L1);

    // 推进 < L1_EXIT_LATENCY: 仍 L1
    f.tick_n(L1_EXIT_LATENCY - 1);
    REQUIRE(f.phy->state() == LtState::L1);

    // 再 1 cycle: 倒计时完成, 回 L0
    f.tick_n(1);
    REQUIRE(f.phy->state() == LtState::L0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: enable_aspm(Off) disables idle-timer
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PciePhyDigitalCtrl: enable_aspm(Off) disables idle-timer",
          "[pcie][aspm][off]") {
    AspmFixture f;
    f.phy->enable_aspm(PciePhyDigitalCtrl::AspmLevel::L0s);
    REQUIRE(f.phy->aspm_level() == PciePhyDigitalCtrl::AspmLevel::L0s);

    f.phy->enable_aspm(PciePhyDigitalCtrl::AspmLevel::Off);
    REQUIRE(f.phy->aspm_level() == PciePhyDigitalCtrl::AspmLevel::Off);

    // 关闭后即使 idle 也不进入 L0s
    f.tick_n(L0S_IDLE_THRESHOLD * 10);
    REQUIRE(f.phy->state() == LtState::L0);
}
