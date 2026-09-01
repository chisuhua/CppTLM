// test/test_pcie_phy_digital_hotplug.cc
// PciePhyDigitalCtrl: 热插拔 + Surprise Removal (Q14)
// Author: CppTLM Team
// Date: 2026-10-06
// 参考: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/decisions.md §Q14
//       design.md §5 (Hot-Plug State Machine)
//       openspec/changes/2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl/proposal.md §T-P3-4
//
// 覆盖:
//   - PRSNT# 变化检测
//   - MRL/PWRGOOD/REFCLK+/PERST# 信号
//   - Surprise Removal: drain(1µs) + abort + 回 Detect

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/pcie/pcie_phy_digital_ctrl_tlm.hh"

using namespace tlm::pcie;

TEST_CASE("HotPlug: 默认信号状态 (PRSNT present, MRL latched, PWRGOOD ok, REFCLK ok)",
          "[pcie][phy][hotplug][t-p3-4]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    REQUIRE(phy.prsnt_present() == true);
    REQUIRE(phy.mrl_latched() == true);
    REQUIRE(phy.pwrgood_ok() == true);
    REQUIRE(phy.refclk_ok() == true);
    REQUIRE(phy.perst_asserted() == false);
}

TEST_CASE("HotPlug: PRSNT# 变化检测 — 移除触发 Surprise Removal 回 Detect",
          "[pcie][phy][hotplug][t-p3-4][surprise]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    phy.start_link_training();
    REQUIRE(phy.is_link_up() == true);

    // PRSNT# 移除 (present=false) → Surprise Removal
    phy.signal_prsnt(false);
    REQUIRE(phy.surprise_removal_count() == 1u);
    REQUIRE(phy.is_link_up() == false);
    REQUIRE(phy.state() == LtState::Detect);
}

TEST_CASE("HotPlug: 重新插入 (present=true) 计数 + 链路保持 Detect",
          "[pcie][phy][hotplug][t-p3-4]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    phy.start_link_training();
    phy.signal_prsnt(false);  // remove
    REQUIRE(phy.surprise_removal_count() == 1u);

    phy.signal_prsnt(true);   // re-insert
    REQUIRE(phy.hotplug_insertion_count() == 1u);
    // 插入后需重新训练 (PERST# deassert + 信号就绪)
    REQUIRE(phy.state() == LtState::Detect);
}

TEST_CASE("HotPlug: MRL latch 解除 — 只是信号状态变化",
          "[pcie][phy][hotplug][t-p3-4]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    phy.signal_mrl(false);
    REQUIRE(phy.mrl_latched() == false);
    // MRL 解除不直接触发链路复位 (平台负责顺序)
    REQUIRE(phy.state() == LtState::Detect);
}

TEST_CASE("HotPlug: PWRGOOD 掉电 + 恢复",
          "[pcie][phy][hotplug][t-p3-4]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    phy.signal_pwrgood(false);
    REQUIRE(phy.pwrgood_ok() == false);
    phy.signal_pwrgood(true);
    REQUIRE(phy.pwrgood_ok() == true);
}

TEST_CASE("HotPlug: REFCLK+ 检测 + 恢复",
          "[pcie][phy][hotplug][t-p3-4]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    phy.signal_refclk(false);
    REQUIRE(phy.refclk_ok() == false);
    phy.signal_refclk(true);
    REQUIRE(phy.refclk_ok() == true);
}

TEST_CASE("HotPlug: PERST# assert → Detect (复位) + deassert → 训练 L0",
          "[pcie][phy][hotplug][t-p3-4]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    phy.start_link_training();
    REQUIRE(phy.is_link_up() == true);

    phy.signal_perst(true);   // assert → 复位
    REQUIRE(phy.perst_asserted() == true);
    REQUIRE(phy.state() == LtState::Detect);
    REQUIRE(phy.is_link_up() == false);

    phy.signal_perst(false);  // deassert + 信号就绪 → 训练
    REQUIRE(phy.perst_asserted() == false);
    REQUIRE(phy.state() == LtState::L0);
    REQUIRE(phy.is_link_up() == true);
}

TEST_CASE("HotPlug: 完整插入序列 (PRSNT→MRL→PWRGOOD→REFCLK→PERST# deassert) 训练",
          "[pcie][phy][hotplug][t-p3-4]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    // 模拟空槽: PRSNT 不在
    phy.signal_prsnt(false);
    phy.signal_mrl(false);
    phy.signal_pwrgood(false);
    phy.signal_refclk(false);

    // 插入流程
    phy.signal_prsnt(true);
    phy.signal_mrl(true);
    phy.signal_pwrgood(true);
    phy.signal_refclk(true);
    // PERST# deassert 触发训练 (所有信号就绪)
    phy.signal_perst(true);
    phy.signal_perst(false);
    REQUIRE(phy.state() == LtState::L0);
    REQUIRE(phy.is_link_up() == true);
    REQUIRE(phy.hotplug_insertion_count() == 1u);
}
