// test/test_pcie_endpoint_ip_from_config.cc
// PcieEndpointIP: JSON 实例化 + 17 端口 adapter 注入测试 (C4 Oracle fix)
// 作者 CppTLM Team / 日期 2026-10-13
// 参考: openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/proposal.md T-P4-7

#include "chstream_register.hh" // 必须: 提供 cpptlm/tlm::pcie 命名空间 + ModuleFactory 注册宏体系
#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/master_port.hh"
#include "core/module_factory.hh"
#include "core/packet.hh"
#include "core/sim_module.hh"
#include "core/slave_port.hh"
#include "core/stream_adapter_base.hh"
#include "framework/chstream_adapter_factory.hh"
#include "framework/multi_port_stream_adapter.hh"
#include "framework/stream_adapter.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/pcie/pcie_link_phy_mux_tlm.hh"

// cpptlm_tests 二进制不自动触发 REGISTER_CHSTREAM（该宏仅在主应用层使用）。
// Phase 2 (pcie-endpoint-ip-simmodule-refactor): PcieEndpointIP 从 REGISTER_CHSTREAM
// 迁移到 REGISTER_MODULE（modules_cluster.hh）; 本测试显式注册 composite
// PcieLinkPhyMuxTLM (object + 17 端口 multiport adapter)。幂等（重复注册安全）。
static const int s_pcie_endpoint_ip_test_registered = []() {
    using Lpm_t = tlm::pcie::PcieLinkPhyMuxTLM;
    using ReqB_t = bundles::PcieTlpBundle;
    using RespB_t = bundles::PcieTlpBundle;
    ModuleFactory::registerObject<Lpm_t>("PcieLinkPhyMuxTLM");
    ChStreamAdapterFactory::get().registerMultiPortAdapter<Lpm_t, ReqB_t, RespB_t, 17>(
        "PcieLinkPhyMuxTLM");
    return 0;
}();

#include <memory>
#include <nlohmann/json.hpp>

using namespace tlm::pcie;
using json = nlohmann::json;

namespace {

    json make_pcie_ip_config(const std::string& name) {
        json cfg;
        cfg["modules"] = json::array({
            {{"name", name}, {"type", "PcieEndpointIP"}},
        });
        // validateConfig (module_factory_validate.cc) 要求 connections 字段存在，
        // 缺失即返回 false 导致 instantiateAll 静默失败（测试曾误报 C4 未注册）。
        cfg["connections"] = json::array();
        return cfg;
    }

} // namespace

TEST_CASE("PcieEndpointIP: ModuleFactory registers PcieEndpointIP type (C4)",
          "[pcie][sriov][endpoint-ip][json][factory]") {
    // 强制触发 lambda 注册（否则编译器可能优化掉 static initialization）
    (void)s_pcie_endpoint_ip_test_registered;
    // 验证 "PcieEndpointIP" 已注册到 ModuleFactory
    // (Phase 2: REGISTER_MODULE → module registry; getRegisteredTypes 合并双注册表)
    auto types = ModuleFactory::getRegisteredTypes();
    bool found = false;
    for (const auto& t : types) {
        INFO("registered type: " << t);
        if (t == "PcieEndpointIP") {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("PcieLinkPhyMuxTLM: ModuleFactory.isMultiPort returns true for 17-port composite (C4)",
          "[pcie][sriov][endpoint-ip][json][factory]") {
    // Phase 2: 17 端口 multiport adapter 归 composite (PcieLinkPhyMuxTLM)
    auto& factory = ChStreamAdapterFactory::get();
    REQUIRE(factory.knows("PcieLinkPhyMuxTLM") == true);
    REQUIRE(factory.isMultiPort("PcieLinkPhyMuxTLM") == true);
    REQUIRE(factory.getPortCount("PcieLinkPhyMuxTLM") == 17u);
}

TEST_CASE("PcieEndpointIP: JSON instantiation via ModuleFactory (C4)",
          "[pcie][sriov][endpoint-ip][json][factory]") {
    EventQueue eq;
    ModuleFactory factory(&eq);

    json cfg = make_pcie_ip_config("pcie_ep_ip");
    REQUIRE_NOTHROW(factory.instantiateAll(cfg));

    auto* ep = factory.getInstance<PcieEndpointIP>("pcie_ep_ip");
    REQUIRE(ep != nullptr);
    REQUIRE(ep->get_module_type() == "PcieEndpointIP");
    // Phase 2: EP 是 SimModule; JSON 实例化经 Step 4.5 simulate_instantiate
    auto* as_sim = dynamic_cast<SimModule*>(ep);
    REQUIRE(as_sim != nullptr);
}

TEST_CASE("PcieEndpointIP: composite 17 ports wired by internal Step 7 (C4, Phase 2)",
          "[pcie][sriov][endpoint-ip][adapter]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_ip_adapter", &eq);
    ep.init();

    json cfg;
    cfg["link_layer"]["enabled"] = true;
    ep.set_config(cfg); // → on_config_loaded → ensure_composite (internal_factory) + Step 7 注入

    // composite 在 internal_factory 内构造; Step 7 为其注入 multiport adapter
    // (multiport 走单指针重载注入 adapter 对象, 17 端口组由 adapter 内部 bind_port_pair)
    auto* composite = PcieEndpointIP::find_composite("pcie_ep_ip_adapter");
    REQUIRE(composite != nullptr);
    REQUIRE(composite->get_adapter(0) != nullptr);
    REQUIRE(ChStreamAdapterFactory::get().getPortCount("PcieLinkPhyMuxTLM") == 17u);

    // 17 端口程序化可达 (R4 决策 C: getInternalOutputPort/InputPort 命中 internal_factory)
    for (unsigned i = 0; i < 17; ++i) {
        const std::string idx = std::to_string(i);
        REQUIRE(ep.getInternalOutputPort("pcie_ep_ip_adapter_lpm.resp_out[" + idx + "]") != nullptr);
        REQUIRE(ep.getInternalInputPort("pcie_ep_ip_adapter_lpm.req_in[" + idx + "]") != nullptr);
    }
}