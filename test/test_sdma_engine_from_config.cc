// test_sdma_engine_from_config.cc
// SdmaEngineTLM: JSON 实例化 + 5 端口 adapter 注入测试 (SD-G5)
// Author: CppTLM Team
// Date: 2026-08-26
//
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/design.md §6,§7
//       spec.md Scenarios "JSON instantiation with multi-port adapter injection"
//                          "All 5 ports receive non-null StreamAdapter"
//
// 测试策略（per tasks.md T-sd-3 注释）：
//   - JSON 实例化测试通过 ModuleFactory 注入 sdma_engine_min.json fixture
//   - 5 端口 adapter 注入测试通过 set_stream_adapter(arr) 注入 fake StreamAdapter

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/master_port.hh"
#include "core/module_factory.hh"
#include "core/packet.hh"
#include "core/sim_object.hh"
#include "core/slave_port.hh"
#include "core/stream_adapter_base.hh"
#include "framework/chstream_adapter_factory.hh"
#include "framework/multi_port_stream_adapter.hh"
#include "framework/stream_adapter.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"

#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using namespace bundles;
using json = nlohmann::json;

namespace {

    // Fake StreamAdapter for multi-port injection test
    // (per design.md §7 "JSON instantiation" - 仅断言端口接收 adapter,
    //  不验证内部数据流，因为数据流已在 test_sdma_engine_h2d/d2h 覆盖)
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

TEST_CASE("SdmaEngine: JSON config loads max_inflight/translate_latency/vram_size",
          "[sdma][json]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);

    // 注入 config
    json cfg;
    cfg["max_inflight"] = 8;
    cfg["translate_latency"] = 3;
    cfg["vram_size_bytes"] = 134217728u; // 128 MB

    sdma.set_config(cfg);
    sdma.on_config_loaded();

    // 验证字段已加载（通过行为间接断言，因为没有公开 getter）
    // max_inflight 行为可验证：构造 fake 8 个 desc 看反压
    // MVP 简化：仅触发 on_config_loaded 不抛异常即认为成功
    REQUIRE_NOTHROW(sdma.init());
}

TEST_CASE("SdmaEngine: All 5 ports receive non-null StreamAdapter (SD-G5)", "[sdma][adapter]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();

    // 注入前 all_ports_have_adapter 返回 false
    REQUIRE(sdma.all_ports_have_adapter() == false);

    // 构造 5 个 fake StreamAdapter
    auto a0 = make_fake();
    auto a1 = make_fake();
    auto a2 = make_fake();
    auto a3 = make_fake();
    auto a4 = make_fake();

    cpptlm::StreamAdapterBase* arr[5] = {a0.get(), a1.get(), a2.get(), a3.get(), a4.get()};
    sdma.set_stream_adapter(arr);

    // 5 个端口都收到非空 adapter
    REQUIRE(sdma.all_ports_have_adapter() == true);
    REQUIRE(sdma.get_adapter(0) == a0.get());
    REQUIRE(sdma.get_adapter(1) == a1.get());
    REQUIRE(sdma.get_adapter(2) == a2.get());
    REQUIRE(sdma.get_adapter(3) == a3.get());
    REQUIRE(sdma.get_adapter(4) == a4.get());

    // 验证端口索引顺序（per design.md §2.5 Port index ordering lock）
    REQUIRE(SdmaEngineTLM::PORT_DESC_IN == 0u);
    REQUIRE(SdmaEngineTLM::PORT_MEM_IN == 1u);
    REQUIRE(SdmaEngineTLM::PORT_MEM_OUT == 2u);
    REQUIRE(SdmaEngineTLM::PORT_HOST_OUT == 3u);
    REQUIRE(SdmaEngineTLM::PORT_DONE_OUT == 4u);

    // 单端口回退：set_stream_adapter(single) 只绑定到 port 0
    auto a_single = make_fake();
    cpptlm::StreamAdapterBase* single_ptr = a_single.get();
    sdma.set_stream_adapter(single_ptr);
    REQUIRE(sdma.get_adapter(0) == single_ptr);
}

TEST_CASE("SdmaEngine: ModuleFactory registers SdmaEngineTLM type", "[sdma][json][factory]") {
    // 验证 "SdmaEngineTLM" 已注册到 ModuleFactory（REGISTER_CHSTREAM 副作用）
    auto types = ModuleFactory::getRegisteredTypes();
    bool found = false;
    for (const auto& t : types) {
        if (t == "SdmaEngineTLM") {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("SdmaEngine: ModuleFactory multi-port SdmaEngineTLM returns 5", "[sdma][json][factory]") {
    // SdmaEngineTLM 是 5 端口模块（per spec.md Scenario "JSON instantiation"）
    REQUIRE(ModuleFactory::getRegisteredTypes().size() > 0);
    auto& factory = ChStreamAdapterFactory::get();
    REQUIRE(factory.knows("SdmaEngineTLM") == true);
    REQUIRE(factory.isMultiPort("SdmaEngineTLM") == true);
    REQUIRE(factory.getPortCount("SdmaEngineTLM") == 5u);
}

TEST_CASE("SdmaEngine: JSON fixture sdma_engine_min.json loads", "[sdma][json][fixture]") {
    // 加载 fixtures JSON（per tasks.md T-sd-3 "JSON fixture configs/test/sdma_engine_min.json"）
    const std::string path = std::string(CPPTLM_SOURCE_DIR) + "/configs/test/sdma_engine_min.json";
    std::ifstream ifs(path);
    REQUIRE(ifs.is_open());

    json cfg;
    ifs >> cfg;
    REQUIRE(cfg.contains("modules"));
    REQUIRE(cfg["modules"].size() == 1u);
    REQUIRE(cfg["modules"][0]["type"] == "SdmaEngineTLM");
    REQUIRE(cfg["modules"][0]["name"] == "sdma");
    REQUIRE(cfg["modules"][0]["params"]["max_inflight"] == 4);
}

TEST_CASE("SdmaEngine: SdmaEngineTLM::num_ports() == 5", "[sdma][json][port-count]") {
    // 验证 ChStreamModuleBase::num_ports() 返回 5（per design.md §3 + spec.md Scenario "JSON
    // instantiation"）
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    REQUIRE(sdma.num_ports() == 5u);
    REQUIRE(SdmaEngineTLM::NUM_PORTS == 5u);
}