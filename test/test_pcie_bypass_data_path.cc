// test/test_pcie_bypass_data_path.cc
// PcieEndpointTLM Bypass Mux 数据路径测试 (Oracle C1)
// Author: CppTLM Team
// Date: 2026-10-06
// 参考: openspec/changes/2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl/proposal.md §T-P3-7
//
// 覆盖 (C1 修复: Bypass Mux mode 对 TLP 数据路径的真实分派):
//   - Full   : TLP 经 PcieLinkLayer (FC 不足被反压 → 证明走了 LL)
//   - Bypass : 即使 FC 容量=0, TLP 仍被接受 (证明绕过 LL)
//   - Partial: LL 仍工作 (FC 反压生效)
//   - 模式切换后行为立即改变 (无需重建/重注入)

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/pcie_endpoint_tlm.h"
#include "tlm/pcie/pcie_bypass_mux.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"

#include <memory>
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using namespace tlm::pcie;
using namespace bundles;
using json = nlohmann::json;

namespace {

// 构造一个 bar0 MMIO_WRITE 寄存器（side_effect none）→ 通过 bar_router 观察落写
void add_bar0_reg(PcieEndpointTLM& ep, uint32_t off) {
    ep.bar_router().add_register(off, "REG",
                                 PcieBarRouter::Access::RW,
                                 PcieBarRouter::SideEffect::NONE, 0);
}

PcieTlpBundle make_mmio_write(uint32_t off, uint64_t data) {
    PcieTlpBundle t;
    t.kind.write(PcieTlpBundle::MMIO_WRITE);
    t.offset.write(off);
    t.size.write(4);
    t.data.write(data);
    t.requester_id.write(0x0100);
    t.trans_id.write(1);
    return t;
}

// 通用构造: named endpoint + link_layer 配置 (fc_credit 可配, bypass_mode 可配)
struct EndpointFixture {
    EventQueue eq;
    std::unique_ptr<PcieEndpointTLM> ep;
    PcieTlpBundle tlp;

    EndpointFixture(const std::string& name, uint32_t fc_credit,
                    const std::string& bypass_mode)
        : ep(std::make_unique<PcieEndpointTLM>(name, &eq)),
          tlp(make_mmio_write(0x1000, 0xAA)) {
        ep->init();
        json cfg;
        cfg["link_layer"]["enabled"] = true;
        cfg["link_layer"]["fc_token_bucket_capacity"] = fc_credit;
        cfg["link_layer"]["fc_initial_credit_p"] = fc_credit;
        cfg["link_layer"]["fc_initial_credit_np"] = fc_credit;
        cfg["link_layer"]["fc_initial_credit_cpl"] = fc_credit;
        if (!bypass_mode.empty()) {
            cfg["link_layer"]["bypass_mode"] = bypass_mode;
        }
        ep->set_config(cfg);
        add_bar0_reg(*ep, 0x1000);
    }

    void inject(uint64_t data) {
        tlp.data.write(data);
        ep->req_in[PcieEndpointTLM::PORT_SLAVE_IN].data() = tlp;
        ep->req_in[PcieEndpointTLM::PORT_SLAVE_IN].set_valid(true);
    }

    void tick() { ep->tick(); }

    bool pending() const {
        return ep->req_in[PcieEndpointTLM::PORT_SLAVE_IN].valid();
    }
};

} // namespace

TEST_CASE("BypassDataPath: Full mode → TLP 经 LL (FC 不足被反压)",
          "[pcie][bypass][datapath][c1][full]") {
    EndpointFixture f("ep_dp_full", /*fc_credit=*/1, /*bypass_mode=*/"Full");
    REQUIRE(PcieBypassMux::for_endpoint("ep_dp_full") != nullptr);
    REQUIRE(PcieBypassMux::for_endpoint("ep_dp_full")->mode() == BypassMode::Full);

    // 第 1 个 → FC P 1→0, 消费
    f.inject(1);
    f.tick();
    REQUIRE(f.pending() == false);
    REQUIRE(f.ep->bar_router().mmio_read(0x1000) == 1u);

    // 第 2 个 → FC P=0 → 反压, 不消费 (证明走了 LL)
    f.inject(2);
    f.tick();
    REQUIRE(f.pending() == true);
    REQUIRE(f.ep->bar_router().mmio_read(0x1000) == 1u);
}

TEST_CASE("BypassDataPath: Bypass mode → FC 容量=0 仍接受 (绕过 LL)",
          "[pcie][bypass][datapath][c1][bypass]") {
    // fc_credit=0: Full 模式下第一个 TLP 就会被反压; Bypass 模式必须接受
    EndpointFixture f("ep_dp_bypass", /*fc_credit=*/0, /*bypass_mode=*/"Bypass");
    REQUIRE(PcieBypassMux::for_endpoint("ep_dp_bypass")->mode() == BypassMode::Bypass);

    // 即使 FC=0, TLP 直接送事务层 → 消费 + 落写
    f.inject(0xBB);
    f.tick();
    REQUIRE(f.pending() == false);
    REQUIRE(f.ep->bar_router().mmio_read(0x1000) == 0xBBu);
}

TEST_CASE("BypassDataPath: Partial mode → LL 仍工作 (FC 反压生效)",
          "[pcie][bypass][datapath][c1][partial]") {
    EndpointFixture f("ep_dp_partial", /*fc_credit=*/1, /*bypass_mode=*/"Partial");
    REQUIRE(PcieBypassMux::for_endpoint("ep_dp_partial")->mode() == BypassMode::Partial);

    f.inject(1);
    f.tick();
    REQUIRE(f.pending() == false);
    REQUIRE(f.ep->bar_router().mmio_read(0x1000) == 1u);

    // FC 耗尽 → 反压 (Partial 保留 LL FC 检查)
    f.inject(2);
    f.tick();
    REQUIRE(f.pending() == true);
    REQUIRE(f.ep->bar_router().mmio_read(0x1000) == 1u);
}

TEST_CASE("BypassDataPath: 模式切换后行为立即改变 (Full → Bypass)",
          "[pcie][bypass][datapath][c1][switch]") {
    EndpointFixture f("ep_dp_switch", /*fc_credit=*/1, /*bypass_mode=*/"Full");
    auto* mux = PcieBypassMux::for_endpoint("ep_dp_switch");
    REQUIRE(mux != nullptr);

    // Full: 第 2 个 TLP FC 反压, 保持 pending
    f.inject(1);
    f.tick();
    REQUIRE(f.pending() == false);
    f.inject(2);
    f.tick();
    REQUIRE(f.pending() == true);
    REQUIRE(f.ep->bar_router().mmio_read(0x1000) == 1u);

    // 切换到 Bypass → 无需重建: 同一 pending TLP 立即被接受
    mux->apply_mode(BypassMode::Bypass);
    f.tick();
    REQUIRE(f.pending() == false);
    REQUIRE(f.ep->bar_router().mmio_read(0x1000) == 2u);
}