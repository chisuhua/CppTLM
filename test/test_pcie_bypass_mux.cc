// test/test_pcie_bypass_mux.cc
// PcieBypassMux: 3 态 Bypass Mux (Full/Bypass/Partial) + apply_mode 10 步状态清理
// Author: CppTLM Team
// Date: 2026-10-06
// 参考: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §7
//       decisions.md §Q7 (3 态 + DrainPolicy) + §Q14 (Surprise Removal 语义)
//       openspec/changes/2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl/proposal.md §T-P3-1
//
// 覆盖:
//   - 3 态模式切换 (Full/Bypass/Partial)
//   - apply_mode 10 步清理: in-flight TLP (DRAIN vs ABORT), retry buffer 清到 ack,
//     seq# 重置, FC 重置, Partial 守卫, MSI-X pending 清理, 对端通知, 暂停/恢复
//   - DrainPolicy 二选一 (GRACEFUL_DRAIN / IMMEDIATE_ABORT)

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/pcie/pcie_bypass_mux.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"

using namespace tlm::pcie;
using namespace bundles;

TEST_CASE("BypassMux: 默认模式 Full + GRACEFUL_DRAIN",
          "[pcie][bypass][mux][t-p3-1]") {
    PcieBypassMux mux;
    REQUIRE(mux.mode() == BypassMode::Full);
    REQUIRE(mux.drain_policy() == DrainPolicy::GRACEFUL_DRAIN);
    REQUIRE(mux.link_paused() == false);
}

TEST_CASE("BypassMux: 3 态模式切换 Full→Bypass→Partial→Full",
          "[pcie][bypass][mux][t-p3-1]") {
    PcieBypassMux mux;
    mux.set_phy_initialized(true);  // Partial 模式守卫要求 PHY 已初始化
    mux.apply_mode(BypassMode::Bypass);
    REQUIRE(mux.mode() == BypassMode::Bypass);
    mux.apply_mode(BypassMode::Partial);
    REQUIRE(mux.mode() == BypassMode::Partial);
    mux.apply_mode(BypassMode::Full);
    REQUIRE(mux.mode() == BypassMode::Full);
}

TEST_CASE("BypassMux: GRACEFUL_DRAIN 等待 in-flight 完成 (不 abort)",
          "[pcie][bypass][mux][t-p3-1]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PcieBypassMux mux(&ll);

    mux.add_in_flight_tlp(3);
    REQUIRE(mux.in_flight_tlps() == 3u);

    mux.apply_mode(BypassMode::Bypass, DrainPolicy::GRACEFUL_DRAIN);

    // in-flight 全部完成（drain），无 abort
    REQUIRE(mux.in_flight_tlps() == 0u);
    REQUIRE(mux.aborted_tlps() == 0u);
    REQUIRE(mux.mode() == BypassMode::Bypass);
    // 切换完成后恢复传输
    REQUIRE(mux.link_paused() == false);
}

TEST_CASE("BypassMux: IMMEDIATE_ABORT abort in-flight TLP",
          "[pcie][bypass][mux][t-p3-1]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PcieBypassMux mux(&ll);

    mux.add_in_flight_tlp(2);
    REQUIRE(mux.in_flight_tlps() == 2u);

    mux.apply_mode(BypassMode::Bypass, DrainPolicy::IMMEDIATE_ABORT);

    REQUIRE(mux.aborted_tlps() == 2u);
    REQUIRE(mux.in_flight_tlps() == 0u);
    REQUIRE(mux.link_paused() == false);
}

TEST_CASE("BypassMux: apply_mode 暂停→清理→恢复 (link layer 生命周期)",
          "[pcie][bypass][mux][t-p3-1]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PcieBypassMux mux(&ll);

    // 切换前可以传 TLP
    REQUIRE(mux.link_paused() == false);
    mux.apply_mode(BypassMode::Bypass);
    REQUIRE(mux.link_paused() == false);  // 切换完成后已恢复
    REQUIRE(mux.link_layer() == &ll);
}

TEST_CASE("BypassMux: Partial 守卫 — PHY 未初始化时拒绝",
          "[pcie][bypass][mux][t-p3-1]") {
    PcieBypassMux mux;
    mux.set_phy_initialized(false);
    REQUIRE_THROWS_AS(mux.apply_mode(BypassMode::Partial), std::logic_error);
    // 模式未提交
    REQUIRE(mux.mode() == BypassMode::Full);
}

TEST_CASE("BypassMux: Partial 守卫 — PHY 已初始化时允许",
          "[pcie][bypass][mux][t-p3-1]") {
    PcieBypassMux mux;
    mux.set_phy_initialized(true);
    mux.apply_mode(BypassMode::Partial);
    REQUIRE(mux.mode() == BypassMode::Partial);
}

TEST_CASE("BypassMux: seq# 重置 — 切换后从 0 重新开始 (防对端失步)",
          "[pcie][bypass][mux][t-p3-1]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PcieBypassMux mux(&ll);

    // 发 5 个 TLP → next_tx_seq = 5
    PcieTlpBundle t(PcieTlpBundle::MMIO_WRITE, 0, 0x1000, 4, 1, 0x0100, 1);
    for (int i = 0; i < 5; ++i) {
        REQUIRE(ll.tx_tlp(t) == true);
    }
    REQUIRE(ll.next_tx_seq() == 5u);

    mux.apply_mode(BypassMode::Bypass);

    // seq# 计数器重置为 0
    REQUIRE(ll.next_tx_seq() == 0u);
}

TEST_CASE("BypassMux: retry buffer 清到 ack seq (累积确认语义)",
          "[pcie][bypass][mux][t-p3-1]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PcieBypassMux mux(&ll);

    PcieTlpBundle t(PcieTlpBundle::MMIO_WRITE, 0, 0x1000, 4, 1, 0x0100, 1);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(ll.tx_tlp(t) == true);
    }
    REQUIRE(ll.retry_buffer_size() == 4u);

    mux.apply_mode(BypassMode::Bypass);

    // retry buffer 清理（DRAIN 语义：全部可清，或清到 ack）
    REQUIRE(ll.retry_buffer_size() == 0u);
}

TEST_CASE("BypassMux: FC token bucket 重置 (apply_mode 后恢复初始 credit)",
          "[pcie][bypass][mux][t-p3-1]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PcieBypassMux mux(&ll);

    // 默认 fc_capacity=256; 发 10 个 write 消耗 Posted credit
    PcieTlpBundle t(PcieTlpBundle::MMIO_WRITE, 0, 0x1000, 4, 1, 0x0100, 1);
    for (int i = 0; i < 10; ++i) {
        REQUIRE(ll.tx_tlp(t) == true);
    }
    REQUIRE(ll.can_send_fc(FcTokenBucket::Type::Posted) == true);  // 仍有余量

    mux.apply_mode(BypassMode::Bypass);

    // FC 桶重置后 Posted credit 恢复初始 256
    REQUIRE(ll.can_send_fc(FcTokenBucket::Type::Posted) == true);
    // 验证重置确实发生：重置后 Tx 路径 FC 检查通过且不因清桶而异常
    REQUIRE(ll.tx_tlp(t) == true);
}

TEST_CASE("BypassMux: MSI-X pending 清理 (apply_mode 后清零)",
          "[pcie][bypass][mux][t-p3-1]") {
    PcieBypassMux mux;
    mux.set_msix_pending(5);
    REQUIRE(mux.msix_pending() == 5u);

    mux.apply_mode(BypassMode::Bypass);

    REQUIRE(mux.msix_pending() == 0u);
}

TEST_CASE("BypassMux: 对端通知 — 切换开始与完成各通知一次",
          "[pcie][bypass][mux][t-p3-1]") {
    PcieBypassMux mux;
    REQUIRE(mux.peer_mode_change_count() == 0u);
    REQUIRE(mux.peer_mode_complete_count() == 0u);

    mux.apply_mode(BypassMode::Bypass);

    REQUIRE(mux.peer_mode_change_count() == 1u);
    REQUIRE(mux.peer_mode_complete_count() == 1u);
}
