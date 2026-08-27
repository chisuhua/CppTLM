// test_pcie_endpoint_from_config.cc
// PcieEndpointTLM: JSON 实例化 + 4 端口 adapter 注入测试 (PE-G5)
// Author: CppTLM Team
// Date: 2026-08-26
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/design.md §6
//       spec.md Scenarios "JSON instantiation with multi-port adapter injection"
//                          "All 4 ports receive non-null StreamAdapter"

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/master_port.hh"
#include "core/module_factory.hh"
#include "core/packet.hh"
#include "core/slave_port.hh"
#include "core/stream_adapter_base.hh"
#include "framework/chstream_adapter_factory.hh"
#include "framework/multi_port_stream_adapter.hh"
#include "framework/stream_adapter.hh"
#include "tlm/gpu/pcie_endpoint_tlm.h"

#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using namespace bundles;
using json = nlohmann::json;

namespace {

    // Helper: 构造 4 个 StreamAdapterBase* 用于多端口注入
    // (test stub — 使用 MultiPortStreamAdapter 模板的最小子集)
    class FakeStreamAdapter : public cpptlm::StreamAdapterBase {
    public:
        void tick() override {
        }
        void bind_ports(MasterPort*, SlavePort*, MasterPort* = nullptr,
                        SlavePort* = nullptr) override {
        }
        void process_request_input(Packet*) override {
        }
        Packet* process_response_output() override {
            return nullptr;
        }
    };

    std::unique_ptr<FakeStreamAdapter> make_fake() {
        return std::make_unique<FakeStreamAdapter>();
    }

} // namespace

TEST_CASE("PcieEndpoint: JSON config loads config_size", "[pcie][endpoint][json]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);

    // 注入 config: config_size = 256
    json cfg;
    cfg["config_size"] = 256;
    cfg["msix_num_vectors"] = 8;

    ep.set_config(cfg);
    ep.on_config_loaded();

    REQUIRE(ep.config_space().config_size() == 256u);
    REQUIRE(ep.msix().num_vectors() == 8u);
}

TEST_CASE("PcieEndpoint: JSON bar0_registers data-driven loading", "[pcie][endpoint][json]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);

    json cfg;
    cfg["bar0_registers"] = json::array({
        {{"offset", 0x0014},
         {"name", "GPU_REG_DOORBELL"},
         {"access", "WO"},
         {"side_effect", "doorbell"},
         {"stream_id", 0}},
        {{"offset", 0x0020}, {"name", "GPU_REG_STATUS"}, {"access", "RO"}, {"side_effect", "none"}},
    });

    ep.set_config(cfg);
    ep.on_config_loaded();

    // 0x0014 doorbell: write 触发 pending
    REQUIRE(ep.bar_router().mmio_write(0x0014, 0x100) == true);
    REQUIRE(ep.bar_router().doorbell_is_pending(0) == true);

    // 0x0020 RO: write 拒绝
    REQUIRE(ep.bar_router().mmio_write(0x0020, 0x42) == false);
}

TEST_CASE("PcieEndpoint: JSON capabilities chain loading", "[pcie][endpoint][json]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);

    json cfg;
    cfg["capabilities"] = json::array({
        {{"id", 17}, {"offset", 64}, {"next", 80}, {"control", 0xAB00}},
        {{"id", 16}, {"offset", 80}, {"next", 0}, {"control", 0}},
    });

    ep.set_config(cfg);
    ep.on_config_loaded();

    REQUIRE(ep.config_space().capability_count() == 2u);
    REQUIRE((ep.config_space().read(0x34) & 0xFFu) == 64u); // capabilities pointer
}

TEST_CASE("PcieEndpoint: All 4 ports receive non-null StreamAdapter (PE-G5)",
          "[pcie][endpoint][adapter]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 注入前 all_ports_have_adapter 返回 false
    REQUIRE(ep.all_ports_have_adapter() == false);

    // 构造 4 个 fake StreamAdapter
    auto a0 = make_fake();
    auto a1 = make_fake();
    auto a2 = make_fake();
    auto a3 = make_fake();

    cpptlm::StreamAdapterBase* arr[4] = {a0.get(), a1.get(), a2.get(), a3.get()};
    ep.set_stream_adapter(arr);

    // 4 个端口都收到非空 adapter
    REQUIRE(ep.all_ports_have_adapter() == true);
    REQUIRE(ep.get_adapter(0) == a0.get());
    REQUIRE(ep.get_adapter(1) == a1.get());
    REQUIRE(ep.get_adapter(2) == a2.get());
    REQUIRE(ep.get_adapter(3) == a3.get());

    // 单端口回退：set_stream_adapter(single) 只绑定到 port 0
    auto a_single = make_fake();
    cpptlm::StreamAdapterBase* single_ptr = a_single.get();
    ep.set_stream_adapter(single_ptr);
    REQUIRE(ep.get_adapter(0) == single_ptr);
}

TEST_CASE("PcieEndpoint: ModuleFactory registers PcieEndpointTLM type",
          "[pcie][endpoint][json][factory]") {
    // 验证 "PcieEndpointTLM" 已注册到 ModuleFactory（REGISTER_CHSTREAM 副作用）
    auto types = ModuleFactory::getRegisteredTypes();
    bool found = false;
    for (const auto& t : types) {
        if (t == "PcieEndpointTLM") {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("PcieEndpoint: ModuleFactory.isMultiPort PcieEndpointTLM returns true",
          "[pcie][endpoint][json][factory]") {
    // PcieEndpointTLM 是 4 端口模块（per spec.md Scenario "JSON instantiation with multi-port
    // adapter injection"）
    REQUIRE(ModuleFactory::getRegisteredTypes().size() > 0);
    // ChStreamAdapterFactory 的 isMultiPort 通过 registerMultiPortAdapter 注册
    auto& factory = ChStreamAdapterFactory::get();
    REQUIRE(factory.knows("PcieEndpointTLM") == true);
    REQUIRE(factory.isMultiPort("PcieEndpointTLM") == true);
    REQUIRE(factory.getPortCount("PcieEndpointTLM") == 4u);
}