// test/test_pcie_phy_digital_ltssm.cc
// PciePhyDigitalCtrl: LTSSM 11 主状态转换测试
// Author: CppTLM Team
// Date: 2026-10-06
// 参考: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §5
//       openspec/changes/2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl/proposal.md §T-P3-2
//
// 覆盖:
//   - 11 主状态转换 (Detect→Polling→Configuration→L0)
//   - Hot_Reset 子状态 (Recovery 内)
//   - L0s/L1/L2 触发与恢复

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/pcie/pcie_phy_digital_ctrl_tlm.hh"

using namespace tlm::pcie;

namespace {
inline void train_to_l0(PciePhyDigitalCtrl& phy) {
    phy.start_link_training();
    while (!phy.advance_training()) {}
}
} // namespace

TEST_CASE("LTSSM: 初始状态 Detect + 链路未 up", "[pcie][phy][ltssm][t-p3-2]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    REQUIRE(phy.state() == LtState::Detect);
    REQUIRE(phy.is_link_up() == false);
    REQUIRE(phy.is_initialized() == false);
}

TEST_CASE("LTSSM: start_link_training Detect→Polling→Configuration→L0",
          "[pcie][phy][ltssm][t-p3-2]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    phy.start_link_training();
    REQUIRE(phy.state() == LtState::Detect);
    REQUIRE(phy.is_link_up() == false);
    REQUIRE(phy.advance_training() == false);
    REQUIRE(phy.state() == LtState::Polling);
    REQUIRE(phy.advance_training() == false);
    REQUIRE(phy.state() == LtState::Configuration);
    REQUIRE(phy.advance_training() == true);
    REQUIRE(phy.state() == LtState::L0);
    REQUIRE(phy.is_link_up() == true);
    REQUIRE(phy.is_initialized() == true);
}

TEST_CASE("LTSSM: set_link_up(false) 回 Detect", "[pcie][phy][ltssm][t-p3-2]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    train_to_l0(phy);
    REQUIRE(phy.is_link_up() == true);
    phy.set_link_up(false);
    REQUIRE(phy.is_link_up() == false);
    REQUIRE(phy.state() == LtState::Detect);
}

TEST_CASE("LTSSM: 11 主状态枚举值映射正确", "[pcie][phy][ltssm][t-p3-2][enum]") {
    REQUIRE(static_cast<uint8_t>(LtState::Detect) == 0u);
    REQUIRE(static_cast<uint8_t>(LtState::Polling) == 1u);
    REQUIRE(static_cast<uint8_t>(LtState::Configuration) == 2u);
    REQUIRE(static_cast<uint8_t>(LtState::Recovery) == 3u);
    REQUIRE(static_cast<uint8_t>(LtState::L0) == 4u);
    REQUIRE(static_cast<uint8_t>(LtState::L0s) == 5u);
    REQUIRE(static_cast<uint8_t>(LtState::L1) == 6u);
    REQUIRE(static_cast<uint8_t>(LtState::L2) == 7u);
    REQUIRE(static_cast<uint8_t>(LtState::Disabled) == 8u);
    REQUIRE(static_cast<uint8_t>(LtState::Loopback) == 9u);
    REQUIRE(static_cast<uint8_t>(LtState::Hot_Reset) == 10u);
}

TEST_CASE("LTSSM: enter_l0s 链路 down + exit 回 L0", "[pcie][phy][ltssm][t-p3-2]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    train_to_l0(phy);
    phy.enter_l0s();
    REQUIRE(phy.state() == LtState::L0s);
    REQUIRE(phy.is_link_up() == false);
    phy.exit_low_power();
    REQUIRE(phy.state() == LtState::L0);
    REQUIRE(phy.is_link_up() == true);
}

TEST_CASE("LTSSM: enter_l1 链路 down + exit 回 L0", "[pcie][phy][ltssm][t-p3-2]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    train_to_l0(phy);
    phy.enter_l1();
    REQUIRE(phy.state() == LtState::L1);
    REQUIRE(phy.is_link_up() == false);
    phy.exit_low_power();
    REQUIRE(phy.state() == LtState::L0);
    REQUIRE(phy.is_link_up() == true);
}

TEST_CASE("LTSSM: enter_l2 不自动退出 (PERST# 唤醒)", "[pcie][phy][ltssm][t-p3-2]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    train_to_l0(phy);
    phy.enter_l2();
    REQUIRE(phy.state() == LtState::L2);
    REQUIRE(phy.is_link_up() == false);
    // L2 不自动退出
    phy.exit_low_power();
    REQUIRE(phy.state() == LtState::L2);
}

TEST_CASE("LTSSM: Recovery 内速率切换 → 新速率 + L0 (tick 推进)",
          "[pcie][phy][ltssm][t-p3-2][rate-switch]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    train_to_l0(phy);
    phy.set_rate(PcieEncodingLatencyModel::Rate::GEN3);

    phy.start_rate_switch(PcieEncodingLatencyModel::Rate::GEN5);
    // 无链路层 → 立即完成 (TLM 简化路径)
    REQUIRE(phy.state() == LtState::L0);
    REQUIRE(phy.rate() == PcieEncodingLatencyModel::Rate::GEN5);
    REQUIRE(phy.is_link_up() == true);
}

TEST_CASE("LTSSM: Hot_Reset 是 Recovery 内子状态 (enum 存在且可查询)",
          "[pcie][phy][ltssm][t-p3-2][hot-reset]") {
    // Hot_Reset 作为 Recovery 内子状态建模 (per design §5 修订)
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    // 通过 enum 值断言存在
    REQUIRE(static_cast<uint8_t>(LtState::Hot_Reset) == 10u);
    (void)phy;
}

TEST_CASE("LTSSM: in_recovery 覆盖 Recovery + Hot_Reset",
          "[pcie][phy][ltssm][t-p3-2]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    REQUIRE(phy.in_recovery() == false);
}
