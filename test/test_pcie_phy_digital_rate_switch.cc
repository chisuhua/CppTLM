// test/test_pcie_phy_digital_rate_switch.cc
// PciePhyDigitalCtrl: Rate Switch 集成 (修 Phase 2 评审 #1)
// Author: CppTLM Team
// Date: 2026-10-06
// 参考: openspec/changes/2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl/proposal.md §T-P3-5
//       pcie_link_layer_tlm.cc trigger_rate_switch API
//
// 覆盖:
//   - PHY 调用 link->trigger_rate_switch(GEN3, GEN5) → link 不可用
//   - 期间 tx_tlp/rx_tlp_from_host 都返回 false (拒绝)
//   - 延迟 ~µs 级 (rate_switch_delay_us(GEN3, GEN5))
//   - 切换完成后 link 恢复可用

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"
#include "tlm/pcie/pcie_phy_digital_ctrl_tlm.hh"

using namespace tlm::pcie;
using namespace bundles;

namespace {

    PcieLinkLayerConfig abundant_config() {
        PcieLinkLayerConfig c;
        c.fc_capacity = 4096;
        c.fc_init_p = 4096;
        c.fc_init_np = 4096;
        c.fc_init_cpl = 4096;
        return c;
    }

    PcieTlpBundle make_write(uint32_t tid) {
        return PcieTlpBundle(PcieTlpBundle::MMIO_WRITE, 0, 0x1000, 4, 1, 0x0100, tid);
    }

} // namespace

TEST_CASE("RateSwitch: PHY 触发 → link->is_rate_switching() == true",
          "[pcie][phy][rate-switch][t-p3-5]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq, abundant_config());
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll);
    phy.start_link_training();

    REQUIRE(ll.is_rate_switching() == false);
    phy.start_rate_switch(PcieEncodingLatencyModel::Rate::GEN5);
    REQUIRE(ll.is_rate_switching() == true);
}

TEST_CASE("RateSwitch: 触发期间 tx_tlp 返回 false (wire busy)",
          "[pcie][phy][rate-switch][t-p3-5]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq, abundant_config());
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll);
    phy.start_link_training();
    phy.set_rate(PcieEncodingLatencyModel::Rate::GEN3);

    phy.start_rate_switch(PcieEncodingLatencyModel::Rate::GEN5);
    // 切换期间: tx_tlp 拒绝 (FC 检查前先看 rate_switching_)
    REQUIRE(ll.tx_tlp(make_write(1)) == false);
}

TEST_CASE("RateSwitch: 触发期间 rx_tlp_from_host 返回 false (rx wire busy)",
          "[pcie][phy][rate-switch][t-p3-5]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq, abundant_config());
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll);
    phy.start_link_training();
    phy.set_rate(PcieEncodingLatencyModel::Rate::GEN3);

    phy.start_rate_switch(PcieEncodingLatencyModel::Rate::GEN5);
    // 切换期间: rx_tlp_from_host 拒绝
    REQUIRE(ll.rx_tlp_from_host(make_write(1)) == false);
}

TEST_CASE("RateSwitch: rate_switch_delay_us(GEN3,GEN5) >= 1µs (含均衡)",
          "[pcie][phy][rate-switch][t-p3-5][delay]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq, abundant_config());
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll);
    phy.start_link_training();
    phy.set_rate(PcieEncodingLatencyModel::Rate::GEN3);

    phy.start_rate_switch(PcieEncodingLatencyModel::Rate::GEN5);
    const uint64_t now = eq.getCurrentCycle();
    const uint64_t ready = ll.rate_switch_ready_ns();
    // ready > now (有延迟); 1µs = 1000ns; 延迟 >= 1µs
    REQUIRE(ready > now);
    REQUIRE(ready - now >= 1000u);
}

TEST_CASE("RateSwitch: tick 推进 — ready 后 link 恢复可用",
          "[pcie][phy][rate-switch][t-p3-5][recovery]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq, abundant_config());
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll);
    phy.start_link_training();
    phy.set_rate(PcieEncodingLatencyModel::Rate::GEN3);

    phy.start_rate_switch(PcieEncodingLatencyModel::Rate::GEN5);
    REQUIRE(ll.is_rate_switching() == true);

    // 推进 2000ns (GEN3→GEN5 切换 1µs=1000ns 已过), tick 链路层 + PHY
    eq.run(2000);
    ll.tick();  // 链路层 tick: ready 后清除 rate_switching_
    phy.tick(); // PHY tick: 完成 Recovery → L0
    REQUIRE(ll.is_rate_switching() == false);
    REQUIRE(phy.is_rate_switching() == false);
    REQUIRE(phy.state() == LtState::L0);
    REQUIRE(phy.is_link_up() == true);
    REQUIRE(phy.rate() == PcieEncodingLatencyModel::Rate::GEN5);
}

TEST_CASE("RateSwitch: PHY 同速率 → no-op", "[pcie][phy][rate-switch][t-p3-5]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq, abundant_config());
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll);
    phy.start_link_training();
    phy.set_rate(PcieEncodingLatencyModel::Rate::GEN5);

    REQUIRE(ll.is_rate_switching() == false);
    phy.start_rate_switch(PcieEncodingLatencyModel::Rate::GEN5); // same rate
    REQUIRE(ll.is_rate_switching() == false);                    // 无切换触发
}

TEST_CASE("RateSwitch: 反向切换 (GEN5→GEN1) ≥ 1µs (downshift 重训练)",
          "[pcie][phy][rate-switch][t-p3-5][downshift]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq, abundant_config());
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll);
    phy.start_link_training();
    phy.set_rate(PcieEncodingLatencyModel::Rate::GEN5);

    phy.start_rate_switch(PcieEncodingLatencyModel::Rate::GEN1);
    REQUIRE(ll.is_rate_switching() == true);
    const uint64_t now = eq.getCurrentCycle();
    REQUIRE(ll.rate_switch_ready_ns() >= now + 1000u);
}

// ========== Oracle C2: 速率切换完成 → 编码延迟模型同步新速率 ==========

TEST_CASE("RateSwitch: 切换完成后 encoding_rate 同步 GEN5 (C2)",
          "[pcie][phy][rate-switch][c2][encoding]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq, abundant_config());
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll);
    phy.start_link_training();
    phy.set_rate(PcieEncodingLatencyModel::Rate::GEN3);

    // 开启编码延迟 (GEN3, 16 lane) → 未启用时切换不会意外开启
    ll.set_encoding_latency(PcieEncodingLatencyModel::Rate::GEN3, 16);
    REQUIRE(ll.encoding_latency_enabled() == true);
    REQUIRE(ll.encoding_rate() == PcieEncodingLatencyModel::Rate::GEN3);

    // GEN3 → GEN5 切换
    phy.start_rate_switch(PcieEncodingLatencyModel::Rate::GEN5);
    REQUIRE(ll.is_rate_switching() == true);

    // 推进 2000ns (切换 1µs 已过) + tick → 完成时同步编码速率
    eq.run(2000);
    ll.tick();
    phy.tick();

    REQUIRE(phy.rate() == PcieEncodingLatencyModel::Rate::GEN5);
    // C2 修复: 编码延迟模型也切到 GEN5
    REQUIRE(ll.encoding_latency_enabled() == true);
    REQUIRE(ll.encoding_rate() == PcieEncodingLatencyModel::Rate::GEN5);
}

TEST_CASE("RateSwitch: 切换完成后 block_latency 按新速率计费 (C2)",
          "[pcie][phy][rate-switch][c2][latency]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq, abundant_config());
    PciePhyDigitalCtrl phy(&eq);
    phy.link_layer(&ll);
    phy.start_link_training();
    phy.set_rate(PcieEncodingLatencyModel::Rate::GEN3);

    // GEN3 x16: 128B 块 = 8ns; GEN5 x16: 128B 块 = 2ns
    ll.set_encoding_latency(PcieEncodingLatencyModel::Rate::GEN3, 16);
    REQUIRE(PcieEncodingLatencyModel::block_latency_ns(PcieEncodingLatencyModel::Rate::GEN3, 128,
                                                       16) == 8u);
    REQUIRE(PcieEncodingLatencyModel::block_latency_ns(PcieEncodingLatencyModel::Rate::GEN5, 128,
                                                       16) == 2u);

    phy.start_rate_switch(PcieEncodingLatencyModel::Rate::GEN5);
    eq.run(2000);
    ll.tick();
    phy.tick();

    // 切换完成后: 后续 TLP 延迟按 GEN5 (2ns) 而非 GEN3 (8ns) 计费
    REQUIRE(ll.encoding_rate() == PcieEncodingLatencyModel::Rate::GEN5);
    PcieTlpBundle t = make_write(99);
    REQUIRE(ll.rx_tlp_from_host(t) == true);
    // RX wire busy 推进: 2ns (GEN5) 而非 8ns (GEN3)
    REQUIRE(ll.rx_wire_busy_until_ns_debug() - eq.getCurrentCycle() == 2u);
}
