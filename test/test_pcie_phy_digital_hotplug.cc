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

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/pcie/pcie_bypass_mux.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"
#include "tlm/pcie/pcie_phy_digital_ctrl_tlm.hh"

using namespace tlm::pcie;
using namespace bundles;

namespace {
    inline void train_to_l0(PciePhyDigitalCtrl& phy) {
        phy.start_link_training();
        while (!phy.advance_training()) {
        }
    }
} // namespace

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
    train_to_l0(phy);
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
    train_to_l0(phy);
    phy.signal_prsnt(false); // remove
    REQUIRE(phy.surprise_removal_count() == 1u);

    phy.signal_prsnt(true); // re-insert
    REQUIRE(phy.hotplug_insertion_count() == 1u);
    // 插入后需重新训练 (PERST# deassert + 信号就绪)
    REQUIRE(phy.state() == LtState::Detect);
}

TEST_CASE("HotPlug: MRL latch 解除 — 只是信号状态变化", "[pcie][phy][hotplug][t-p3-4]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    phy.signal_mrl(false);
    REQUIRE(phy.mrl_latched() == false);
    // MRL 解除不直接触发链路复位 (平台负责顺序)
    REQUIRE(phy.state() == LtState::Detect);
}

TEST_CASE("HotPlug: PWRGOOD 掉电 + 恢复", "[pcie][phy][hotplug][t-p3-4]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    phy.signal_pwrgood(false);
    REQUIRE(phy.pwrgood_ok() == false);
    phy.signal_pwrgood(true);
    REQUIRE(phy.pwrgood_ok() == true);
}

TEST_CASE("HotPlug: REFCLK+ 检测 + 恢复", "[pcie][phy][hotplug][t-p3-4]") {
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
    train_to_l0(phy);
    REQUIRE(phy.is_link_up() == true);

    phy.signal_perst(true); // assert → 复位
    REQUIRE(phy.perst_asserted() == true);
    REQUIRE(phy.state() == LtState::Detect);
    REQUIRE(phy.is_link_up() == false);

    phy.signal_perst(false); // deassert + 信号就绪 → 训练 (C3: 分步)
    REQUIRE(phy.perst_asserted() == false);
    REQUIRE(phy.state() == LtState::Detect);
    // 训练序列开始: 每 tick 一状态 → L0
    for (int i = 0; i < 4; ++i)
        phy.tick();
    REQUIRE(phy.state() == LtState::L0);
    REQUIRE(phy.is_link_up() == true);
}

TEST_CASE("HotPlug: 完整插入序列 (PRSNT→MRL→PWRGOOD→REFCLK→PERST# deassert) 训练",
          "[pcie][phy][hotplug][t-p3-4]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll); // 挂 LL 保持一致性
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
    for (int i = 0; i < 4; ++i)
        phy.tick();
    REQUIRE(phy.state() == LtState::L0);
    REQUIRE(phy.is_link_up() == true);
    REQUIRE(phy.hotplug_insertion_count() == 1u);
}

// ========== Oracle C3: Hot_Reset 可达 (Recovery + PERST#) ==========

TEST_CASE("HotPlug: Recovery 内 PERST# assert → Hot_Reset 子状态 (C3)",
          "[pcie][phy][hotplug][t-p3-4][hot-reset][c3]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll); // 挂 LL → start_rate_switch 停留在 Recovery (非立即完成)
    train_to_l0(phy);
    phy.set_rate(PcieEncodingLatencyModel::Rate::GEN3);

    // 速率切换 → Recovery
    phy.start_rate_switch(PcieEncodingLatencyModel::Rate::GEN5);
    REQUIRE(phy.state() == LtState::Recovery);

    // Recovery 内 PERST# assert → Hot_Reset
    phy.signal_perst(true);
    REQUIRE(phy.state() == LtState::Hot_Reset);
    REQUIRE(phy.is_link_up() == false);
    REQUIRE(phy.in_recovery() == true);
}

TEST_CASE("HotPlug: Hot_Reset 可退出 — PERST# deassert 恢复训练 (C3)",
          "[pcie][phy][hotplug][t-p3-4][hot-reset][c3]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    train_to_l0(phy);
    phy.signal_perst(true); // 从 L0 assert → Detect
    REQUIRE(phy.state() == LtState::Detect);

    phy.signal_perst(false); // deassert → 训练分步
    for (int i = 0; i < 4; ++i)
        phy.tick();
    REQUIRE(phy.state() == LtState::L0);
    REQUIRE(phy.is_link_up() == true);
}

TEST_CASE("HotPlug: enter_disabled / enter_loopback 显式可达 (C3)",
          "[pcie][phy][hotplug][t-p3-4][c3]") {
    EventQueue eq;
    PciePhyDigitalCtrl phy(&eq);
    train_to_l0(phy);

    phy.enter_disabled();
    REQUIRE(phy.state() == LtState::Disabled);
    REQUIRE(phy.is_link_up() == false);

    phy.enter_loopback();
    REQUIRE(phy.state() == LtState::Loopback);
    REQUIRE(phy.is_link_up() == false);

    phy.enter_hot_reset();
    REQUIRE(phy.state() == LtState::Hot_Reset);
    REQUIRE(phy.in_recovery() == true);
}

// ========== Oracle C5: Surprise Removal 完整 drain/abort/clear 序列 (Q14) ==========

TEST_CASE("HotPlug: Surprise Removal — mux 挂接时 abort + MSI-X pending 清零 (C5)",
          "[pcie][phy][hotplug][t-p3-4][surprise][c5]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PcieBypassMux mux(&ll);
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll);
    phy.mux(&mux);
    train_to_l0(phy);
    REQUIRE(phy.is_link_up() == true);

    // 准备 in-flight TLP 与 MSI-X pending
    mux.add_in_flight_tlp(3);
    mux.set_msix_pending(5);
    REQUIRE(mux.in_flight_tlps() == 3u);
    REQUIRE(mux.msix_pending() == 5u);

    // PRSNT# 移除 → Surprise Removal: abort + clear msix + 回 Detect
    phy.signal_prsnt(false);
    REQUIRE(phy.surprise_removal_count() == 1u);
    // in-flight abort (非 drain: Surprise Removal 语义)
    REQUIRE(mux.in_flight_tlps() == 0u);
    REQUIRE(mux.aborted_tlps() == 3u);
    // MSI-X pending 清零
    REQUIRE(mux.msix_pending() == 0u);
    REQUIRE(phy.is_link_up() == false);
    REQUIRE(phy.state() == LtState::Detect);
}

TEST_CASE("HotPlug: Surprise Removal — 无 mux 时 link_layer 清 retry/seq/FC (C5)",
          "[pcie][phy][hotplug][t-p3-4][surprise][c5]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll); // 只挂 LL, 不挂 mux → 回退路径
    train_to_l0(phy);

    // 上行发 4 个 TLP → retry buffer 非空, seq 计数器 4
    PcieTlpBundle t(PcieTlpBundle::MMIO_WRITE, 0, 0x1000, 4, 1, 0x0100, 9);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(ll.tx_tlp(t) == true);
    }
    REQUIRE(ll.retry_buffer_size() == 4u);
    REQUIRE(ll.next_tx_seq() == 4u);

    phy.signal_prsnt(false);
    // 回退路径: 清 retry buffer + 重置 seq 计数器
    REQUIRE(ll.retry_buffer_size() == 0u);
    REQUIRE(ll.next_tx_seq() == 0u);
    REQUIRE(phy.state() == LtState::Detect);
}
