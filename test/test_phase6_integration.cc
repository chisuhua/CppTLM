// test/test_phase6_integration.cc
// Phase 6: End-to-end integration — Cache→Crossbar→Memory
// 功能描述：验证 ChStream 模块端到端数据通路 + ModuleFactory 完整集成
// 作者 CppTLM Team / 日期 2026-04-13
#include <catch2/catch_all.hpp>
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/chstream_adapter_factory.hh"
#include "bundles/cache_bundles_tlm.hh"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static void registerChStreamModules() {
    static bool registered = false;
    if (!registered) {
        REGISTER_OBJECT;
        REGISTER_CHSTREAM;
        registered = true;
    }
}

TEST_CASE("Phase 6: Full integration — Cache→Crossbar→Memory", "[phase6][integration]") {
    EventQueue eq;
    registerChStreamModules();
    ModuleFactory factory(&eq);

    json config = R"({
        "modules": [
            {"name": "cache", "type": "CacheTLM"},
            {"name": "xbar", "type": "CrossbarTLM"},
            {"name": "mem", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "cache", "dst": "xbar.0", "latency": 1},
            {"src": "xbar.0", "dst": "mem", "latency": 2}
        ]
    })"_json;

    factory.instantiateAll(config);
    factory.startAllTicks();

    // Verify all instances exist
    auto* cache = factory.getInstance("cache");
    auto* xbar = factory.getInstance("xbar");
    auto* mem = factory.getInstance("mem");
    REQUIRE(cache != nullptr);
    REQUIRE(xbar != nullptr);
    REQUIRE(mem != nullptr);

    // Verify types
    REQUIRE(cache->get_module_type() == "CacheTLM");
    REQUIRE(xbar->get_module_type() == "CrossbarTLM");
    REQUIRE(mem->get_module_type() == "MemoryTLM");

    // Verify XbarTLM has correct port count
    auto* xbar_tlm = dynamic_cast<CrossbarTLM*>(xbar);
    REQUIRE(xbar_tlm != nullptr);
    REQUIRE(xbar_tlm->num_ports() == 4);

    // Verify routing logic
    REQUIRE(xbar_tlm->route_address(0x1000) == 1);

    // Run simulation for 50 cycles
    eq.run(50);
}

TEST_CASE("Phase 6: Multi-port Crossbar with 4 Memory modules", "[phase6][integration]") {
    EventQueue eq;
    registerChStreamModules();
    ModuleFactory factory(&eq);

    json config = R"({
        "modules": [
            {"name": "cache", "type": "CacheTLM"},
            {"name": "xbar", "type": "CrossbarTLM"},
            {"name": "mem0", "type": "MemoryTLM"},
            {"name": "mem1", "type": "MemoryTLM"},
            {"name": "mem2", "type": "MemoryTLM"},
            {"name": "mem3", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "cache", "dst": "xbar.0", "latency": 1},
            {"src": "xbar.0", "dst": "mem0", "latency": 2},
            {"src": "xbar.1", "dst": "mem1", "latency": 2},
            {"src": "xbar.2", "dst": "mem2", "latency": 2},
            {"src": "xbar.3", "dst": "mem3", "latency": 2}
        ]
    })"_json;

    factory.instantiateAll(config);

    // All 6 modules should exist
    REQUIRE(factory.getInstance("cache") != nullptr);
    REQUIRE(factory.getInstance("xbar") != nullptr);
    REQUIRE(factory.getInstance("mem0") != nullptr);
    REQUIRE(factory.getInstance("mem1") != nullptr);
    REQUIRE(factory.getInstance("mem2") != nullptr);
    REQUIRE(factory.getInstance("mem3") != nullptr);

    auto* xbar = dynamic_cast<CrossbarTLM*>(factory.getInstance("xbar"));
    REQUIRE(xbar != nullptr);
    REQUIRE(xbar->num_ports() == 4);

    eq.run(20);
}

TEST_CASE("Phase 6: ChStreamAdapterFactory multi-port detection", "[phase6][factory]") {
    auto& factory = ChStreamAdapterFactory::get();
    registerChStreamModules();
    REQUIRE(factory.knows("CacheTLM"));
    REQUIRE(factory.knows("MemoryTLM"));
    REQUIRE(factory.knows("CrossbarTLM"));

    REQUIRE_FALSE(factory.isMultiPort("CacheTLM"));
    REQUIRE_FALSE(factory.isMultiPort("MemoryTLM"));
    REQUIRE(factory.isMultiPort("CrossbarTLM"));

    REQUIRE(factory.getPortCount("CacheTLM") == 1);
    REQUIRE(factory.getPortCount("MemoryTLM") == 1);
    REQUIRE(factory.getPortCount("CrossbarTLM") == 4);
}

TEST_CASE("Phase 6: CrossbarTLM routing verification", "[phase6][crossbar]") {
    EventQueue eq;
    CrossbarTLM xbar("xbar", &eq);

    REQUIRE(xbar.route_address(0x0000) == 0);
    REQUIRE(xbar.route_address(0x0FFF) == 0);
    REQUIRE(xbar.route_address(0x1000) == 1);
    REQUIRE(xbar.route_address(0x1FFF) == 1);
    REQUIRE(xbar.route_address(0x2000) == 2);
    REQUIRE(xbar.route_address(0x2FFF) == 2);
    REQUIRE(xbar.route_address(0x3000) == 3);
    REQUIRE(xbar.route_address(0x3FFF) == 3);
}

TEST_CASE("Phase 6: CrossbarTLM tick routes request", "[phase6][crossbar]") {
    EventQueue eq;
    CrossbarTLM xbar("xbar", &eq);

    // Send request to port 0 with address routing to port 1
    bundles::CacheReqBundle req;
    req.transaction_id.write(42);
    req.address.write(0x1234);  // Routes to port 1
    req.is_write.write(0);
    req.data.write(0);
    req.size.write(8);
    xbar.req_in[0].consume();
    std::memcpy(&xbar.req_in[0].data(), &req, sizeof(req));
    xbar.req_in[0].set_valid(true);

    xbar.tick();

    // Verify response appears on port 1 (routed destination)
    REQUIRE(xbar.resp_out[1].valid());
    auto resp = xbar.resp_out[1].data();
    REQUIRE(resp.transaction_id.read() == 42);
    REQUIRE(resp.is_hit.read() == true);
}

TEST_CASE("Phase 6: JSON config with port-indexed connections", "[phase6][json]") {
    json config = R"({
        "modules": [
            {"name": "cache", "type": "CacheTLM"},
            {"name": "xbar", "type": "CrossbarTLM"},
            {"name": "mem", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "cache", "dst": "xbar.0", "latency": 1},
            {"src": "xbar.0", "dst": "mem", "latency": 2}
        ]
    })"_json;

    auto conns = config["connections"];
    REQUIRE(conns.size() == 2);
    REQUIRE(conns[0]["src"] == "cache");
    REQUIRE(conns[0]["dst"] == "xbar.0");
    REQUIRE(conns[0]["latency"] == 1);
    REQUIRE(conns[1]["src"] == "xbar.0");
    REQUIRE(conns[1]["dst"] == "mem");
}

TEST_CASE("ChStream ModuleFactory: CacheTLM single-port instantiation", "[phase6][factory]") {
    REGISTER_OBJECT
    REGISTER_CHSTREAM

    EventQueue eq;
    ModuleFactory factory(&eq);

    json config = R"({
        "modules": [
            {"name": "cache", "type": "CacheTLM"},
            {"name": "mem", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "cache", "dst": "mem", "latency": 1}
        ]
    })"_json;

    factory.instantiateAll(config);
    REQUIRE(factory.getInstance("cache") != nullptr);
    REQUIRE(factory.getInstance("mem") != nullptr);
    REQUIRE(factory.getInstance("cache")->get_module_type() == "CacheTLM");
    REQUIRE(factory.getInstance("mem")->get_module_type() == "MemoryTLM");
}

TEST_CASE("ChStream ModuleFactory: CrossbarTLM multi-port instantiation", "[phase6][factory]") {
    REGISTER_OBJECT
    REGISTER_CHSTREAM

    EventQueue eq;
    ModuleFactory factory(&eq);

    json config = R"({
        "modules": [
            {"name": "cache", "type": "CacheTLM"},
            {"name": "xbar", "type": "CrossbarTLM"},
            {"name": "mem", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "cache", "dst": "xbar.0", "latency": 1},
            {"src": "xbar.0", "dst": "mem", "latency": 2}
        ]
    })"_json;

    factory.instantiateAll(config);

    auto* xbar = dynamic_cast<CrossbarTLM*>(factory.getInstance("xbar"));
    REQUIRE(xbar != nullptr);
    REQUIRE(xbar->num_ports() == 4);
    REQUIRE(xbar->route_address(0x0000) == 0);
    REQUIRE(xbar->route_address(0x1000) == 1);
    REQUIRE(xbar->route_address(0x2000) == 2);
    REQUIRE(xbar->route_address(0x3000) == 3);
}

TEST_CASE("ChStream: CrossbarTLM routing verification", "[phase6][crossbar]") {
}
