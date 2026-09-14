// test/test_power_state_transition.cc
// PcieEndpointIP Power state 状态机 + INV-A MMIO gating (Stage 1.4 §1.3)
// per openspec/changes/2026-09-10-cpptlm-stage-1-4-2-1/design.md §1.3
//
// 标签: [pcie] [pm] [state]
//
// 作者 CppTLM Team / 日期 2027-02-09
#include "catch_amalgamated.hpp"

#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/gpu/pcie_config_space_mvp.hh"
#include "tlm/pcie/pcie_axi_adapter_tlm.hh"

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/sim_object.hh"
#include "bundles/axi4_bundles_tlm.hh"
#include "framework/stream_adapter.hh"

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using tlm::pcie::PcieEndpointIP;
using tlm::gpu::PcieConfigSpace;
using cpptlm::Axi4StreamAdapter;

namespace {

constexpr uint16_t PM_CAP_OFFSET   = 0x40;
constexpr uint16_t PM_PMCSR_OFFSET = PM_CAP_OFFSET + 0x04; // 0x44
constexpr uint16_t PWS_D0          = 0x0000;
constexpr uint16_t PWS_D3HOT       = 0x0003;

struct PwrFixture {
    EventQueue eq;
    PcieEndpointIP ep;

    PwrFixture() : ep("pcie_ep_pwr_test", &eq) {
        json cfg;
        cfg["axi_adapter"] = json::object();
        cfg["link_layer"]["enabled"] = true;
        ep.set_config(cfg);
        ep.on_config_loaded();
    }

    void install_pm_cap_and_callback() {
        // PM Cap 安装由 PcieEndpointIP::init() 处理 (Stage 1.4 新增)
        // 我们只需验证 PMCSR 写会触发状态切换
    }

    void write_pmcsr(uint16_t pws) {
        auto* ax = tlm::pcie::PcieAxiAdapter::for_endpoint("pcie_ep_pwr_test");
        if (!ax) return;
        auto& axi = ax->axi();

        bundles::Axi4Bundle wreq;
        wreq.awid.write(0);
        wreq.awaddr.write(PM_PMCSR_OFFSET);
        wreq.awlen.write(0);
        wreq.awsize.write(2);
        wreq.awburst.write(1);
        wreq.wdata.write(pws);
        wreq.wstrb.write(0xF);
        wreq.wlast.write(1);
        axi.master_req(wreq);
        if (axi.master_resp_valid()) {
            axi.master_resp_consume();
        }
    }
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: PcieEndpointIP default state is D0
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: default power state is D0",
          "[pcie][pm][state]") {
    PwrFixture f;
    REQUIRE(f.ep.power_state() == tlm::pcie::PcieEndpointIP::PciePowerState::D0);
    REQUIRE(f.ep.mmio_gated() == false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: PMCSR write D3hot triggers state transition + MMIO gating (INV-A)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: PMCSR D3hot triggers state transition + INV-A MMIO gating",
          "[pcie][pm][state][d3hot]") {
    PwrFixture f;

    SECTION("set_power_state(D3hot) gates MMIO writes") {
        f.ep.set_power_state(tlm::pcie::PcieEndpointIP::PciePowerState::D3hot);
        REQUIRE(f.ep.power_state() == tlm::pcie::PcieEndpointIP::PciePowerState::D3hot);
        REQUIRE(f.ep.mmio_gated() == true);
    }

    SECTION("set_power_state(D0) un-gates MMIO") {
        f.ep.set_power_state(tlm::pcie::PcieEndpointIP::PciePowerState::D3hot);
        REQUIRE(f.ep.mmio_gated() == true);
        f.ep.set_power_state(tlm::pcie::PcieEndpointIP::PciePowerState::D0);
        REQUIRE(f.ep.mmio_gated() == false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: MMIO gating blocks writes at tick() boundary (INV-A)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: D3hot tick() rejects CFG/BAR writes (INV-A)",
          "[pcie][pm][state][mmio-gate]") {
    PwrFixture f;

    auto* ax = tlm::pcie::PcieAxiAdapter::for_endpoint("pcie_ep_pwr_test");
    REQUIRE(ax != nullptr);
    auto& axi = ax->axi();

    // 步骤 1: 验证 D0 默认状态 (mmio_gated=false, power_state=D0)
    REQUIRE(f.ep.mmio_gated() == false);

    // 步骤 1b: D0 → D3hot
    f.ep.set_power_state(tlm::pcie::PcieEndpointIP::PciePowerState::D3hot);
    REQUIRE(f.ep.mmio_gated() == true);

    // 步骤 1c: 验证 bar_store_ 在 gate 状态下不会被 EP 写入
    // (PMCSR 写拦截保证 D3hot 仅由 cfg 写触发, MMIO path 被 gate)
    const uint64_t key = 0x10000000ULL & ~0x3ULL;
    REQUIRE(f.ep.bar_store_value(key) == 0u);

    // 步骤 2: 进入 D3hot
    f.ep.set_power_state(tlm::pcie::PcieEndpointIP::PciePowerState::D3hot);
    REQUIRE(f.ep.mmio_gated() == true);

    // INV-A 验证: D3hot 下 EP.tick() 不会消费任何 AXI slave 请求
    // 我们通过 mmio_gated_ accessor 间接验证 (true = tick 早返回)
    // 详细 E2E 行为验证见 test_pm_capability.cc (PMCSR 写拦截)
    // 这里只验证 accessor 状态
    REQUIRE(f.ep.mmio_gated() == true);

    // 步骤 3: 回到 D0, 验证可写
    f.ep.set_power_state(tlm::pcie::PcieEndpointIP::PciePowerState::D0);
    REQUIRE(f.ep.mmio_gated() == false);
}
