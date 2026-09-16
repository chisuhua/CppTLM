// test/test_pcie_power_state_cfg_access.cc
// INV-A gate 收窄 (BAR only, cfg 保留 D3hot) + DECERR 响应 + PWS mask (EP 级)
// per openspec/changes/2027-02-09-cpptlm-stage-1-4-2-1-followups/design.md §1+§6
//
// 标签: [pcie] [pm] [state] [gate] [d3hot]
//
// 作者 CppTLM Team / 日期 2027-02-09
#include "catch_amalgamated.hpp"

#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/pcie/pcie_axi_adapter_tlm.hh"
#include "tlm/gpu/pcie_config_space_mvp.hh"

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "bundles/axi4_bundles_tlm.hh"

#include <nlohmann/json.hpp>

#include <atomic>
#include <string>

using json = nlohmann::json;
using tlm::pcie::PcieEndpointIP;
using tlm::pcie::PcieAxiAdapter;
using tlm::pcie::PciePhyDigitalCtrl;

namespace {

constexpr uint16_t PM_PMCSR_OFFSET = 0x44;   // PM Cap (0x40) + 4

struct GateFixture {
    EventQueue eq;
    PcieEndpointIP ep;
    std::string ep_name;

    GateFixture() : ep("pcie_ep_gate_test_" + std::to_string(next_id()), &eq),
                    ep_name(ep.getName()) {
        json cfg;
        cfg["axi_adapter"] = json::object();
        cfg["link_layer"]["enabled"] = true;
        ep.set_config(cfg);
        ep.on_config_loaded();
    }

    static int next_id() {
        static std::atomic<int> s{0};
        return s.fetch_add(1);
    }

    PcieAxiAdapter* ax() { return PcieAxiAdapter::for_endpoint(ep_name); }

    // 注入 cfg 写（EP slave 通道）并驱动 tick
    bool cfg_write(uint16_t offset, uint32_t value) {
        auto* a = ax();
        if (!a) return false;
        auto& axi = a->axi();
        bundles::Axi4Bundle wreq;
        wreq.awid.write(0);
        wreq.awaddr.write(offset);
        wreq.awlen.write(0);
        wreq.awsize.write(2);
        wreq.awburst.write(1);
        wreq.wdata.write(value);
        wreq.wstrb.write(0xF);
        wreq.wlast.write(1);
        axi.slave_req(wreq);
        ep.tick();
        // 推进 adapter 通道转移 + 消费响应
        a->axi().tick();
        if (axi.slave_resp_valid()) axi.slave_resp_consume();
        return true;
    }

    // 注入 BAR 写（EP slave 通道）并驱动 tick, 返回 bresp (0xFF = 未消费)
    uint32_t bar_write(uint64_t addr, uint32_t value) {
        auto* a = ax();
        if (!a) return 0xFF;
        auto& axi = a->axi();
        bundles::Axi4Bundle wreq;
        wreq.awid.write(0xB1);
        wreq.awaddr.write(addr);
        wreq.awlen.write(0);
        wreq.awsize.write(2);
        wreq.awburst.write(1);
        wreq.wdata.write(value);
        wreq.wstrb.write(0xF);
        wreq.wlast.write(1);
        axi.slave_req(wreq);
        ep.tick();
        // 先读响应 (slave_ready_ 默认 true, axi.tick() 会清 valid), 再推进通道
        uint32_t bresp_out = 0xFE;
        if (axi.slave_resp_valid()) {
            bresp_out = axi.slave_resp_data().bresp.read();
            axi.slave_resp_consume();
        }
        a->axi().tick();
        return bresp_out;
    }
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: D3hot cfg write via PMCSR wakes to D0 (INV-E)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: D3hot cfg write PMCSR wakes to D0 (INV-E)",
          "[pcie][pm][state][gate][d3hot]") {
    GateFixture f;
    REQUIRE(f.ep.power_state() == PcieEndpointIP::PciePowerState::D0);

    // 进入 D3hot
    f.ep.set_power_state(PcieEndpointIP::PciePowerState::D3hot);
    REQUIRE(f.ep.mmio_gated() == true);

    // D3hot 下 cfg 写 PMCSR = D0 → 状态机应回到 D0 (INV-E)
    const bool consumed = f.cfg_write(PM_PMCSR_OFFSET, 0x0000);
    REQUIRE(consumed == true);
    REQUIRE(f.ep.power_state() == PcieEndpointIP::PciePowerState::D0);
    REQUIRE(f.ep.mmio_gated() == false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: D3hot cfg read always succeeds (PCI PM spec)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: D3hot cfg read Vendor ID still works",
          "[pcie][pm][state][gate][d3hot]") {
    GateFixture f;
    f.ep.set_power_state(PcieEndpointIP::PciePowerState::D3hot);

    // D3hot 下 cfg 读 Vendor ID (offset 0x00) → EP 内部 cfg 路径不被 gate
    // 通过直接读 config space 验证 (cfg 路径未被 mmio_gated 阻断)
    auto& cfg_pf = f.ep.vf_pool().config_of(0);
    REQUIRE((cfg_pf.read(0x00) & 0xFFFFu) == 0x10DEu);  // Nvidia vendor ID
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: D3hot BAR write returns DECERR (not silent)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: D3hot BAR write returns DECERR (AXI encoding 3)",
          "[pcie][pm][state][gate][d3hot][decerr]") {
    GateFixture f;

    // D0 下 BAR 写成功 (bresp=0 OKAY)
    const uint32_t bresp_d0 = f.bar_write(0x10000000ULL, 0xDEADBEEF);
    REQUIRE(bresp_d0 == 0u);

    // D3hot 下 BAR 写 → DECERR (bresp=3; 勘误: 2=SLVERR, 3=DECERR)
    f.ep.set_power_state(PcieEndpointIP::PciePowerState::D3hot);
    const uint32_t bresp_d3 = f.bar_write(0x10000000ULL, 0xCAFEBABE);
    REQUIRE(bresp_d3 == 3u);

    // bar_store_ 未被覆写
    const uint64_t key = 0x10000000ULL & ~0x3ULL;
    REQUIRE(f.ep.bar_store_value(0, 0, key) == 0xDEADBEEFu);

    // 回 D0 后 BAR 写恢复 OKAY
    f.ep.set_power_state(PcieEndpointIP::PciePowerState::D0);
    REQUIRE(f.bar_write(0x10000000ULL, 0x12345678) == 0u);
    REQUIRE(f.ep.bar_store_value(0, 0, key) == 0x12345678u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: PMCSR PWS=1/2 reserved values ignored (EP 级 mask)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: PMCSR PWS=1/2 reserved values ignored (EP mask)",
          "[pcie][pm][state][mask]") {
    GateFixture f;

    // PWS=1 (D1) → 写被忽略, 保持 D0
    f.cfg_write(PM_PMCSR_OFFSET, 0x0001);
    REQUIRE(f.ep.power_state() == PcieEndpointIP::PciePowerState::D0);
    REQUIRE(f.ep.mmio_gated() == false);

    // PWS=2 (D2) → 同上
    f.cfg_write(PM_PMCSR_OFFSET, 0x0002);
    REQUIRE(f.ep.power_state() == PcieEndpointIP::PciePowerState::D0);

    // PWS=3 (D3hot) → 正常切换
    f.cfg_write(PM_PMCSR_OFFSET, 0x0003);
    REQUIRE(f.ep.power_state() == PcieEndpointIP::PciePowerState::D3hot);
    REQUIRE(f.ep.mmio_gated() == true);

    // PWS=0 (D0) → 回 D0
    f.cfg_write(PM_PMCSR_OFFSET, 0x0000);
    REQUIRE(f.ep.power_state() == PcieEndpointIP::PciePowerState::D0);
}
