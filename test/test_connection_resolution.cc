// test/test_connection_resolution.cc
#include "catch_amalgamated.hpp"
#include "core/module_factory.hh"
#include "mock_modules.hh"

// 注册 MockSim 类型
namespace {
auto _register = []() {
    ModuleFactory::registerObject<MockSim>("MockSim");
    return 0;
}();
}

TEST_CASE("ConnectionResolutionTest WildcardConnection", "[connection][resolution]") {
    EventQueue eq;

    json config = R"({
        "modules": [
            { "name": "cpu0", "type": "MockSim" },
            { "name": "cpu1", "type": "MockSim" },
            { "name": "l1", "type": "MockSim" }
        ],
        "connections": [
            { "src": "cpu*", "dst": "l1", "latency": 2 }
        ]
    })"_json;

    ModuleFactory factory(&eq);
    factory.instantiateAll(config);

    auto* l1 = factory.getInstance("l1");
    REQUIRE(l1 != nullptr);

    const auto& upstream_ports = l1->getPortManager().getUpstreamPorts();
    REQUIRE(upstream_ports.size() == 2);  // cpu0 + cpu1
}

TEST_CASE("ConnectionResolutionTest RegexConnection", "[connection][resolution]") {
    EventQueue eq;

    json config = R"({
        "modules": [
            { "name": "cpu0", "type": "MockSim" },
            { "name": "cpu1", "type": "MockSim" },
            { "name": "cpu2", "type": "MockSim" },
            { "name": "l1", "type": "MockSim" }
        ],
        "connections": [
            { "src": "regex:cpu[0-1]", "dst": "l1", "latency": 2 }
        ]
    })"_json;

    ModuleFactory factory(&eq);
    factory.instantiateAll(config);

    auto* l1 = factory.getInstance("l1");
    const auto& upstream_ports = l1->getPortManager().getUpstreamPorts();
    REQUIRE(upstream_ports.size() == 2);  // cpu0 + cpu1
}

TEST_CASE("ConnectionResolutionTest ModuleGroupConnection", "[connection][resolution]") {
    EventQueue eq;

    json config = R"({
        "modules": [
            { "name": "cpu0", "type": "MockSim" },
            { "name": "cpu1", "type": "MockSim" },
            { "name": "l1", "type": "MockSim" }
        ],
        "groups": {
            "cpus": ["cpu0", "cpu1"]
        },
        "connections": [
            { "src": "group:cpus", "dst": "l1", "latency": 2 }
        ]
    })"_json;

    ModuleFactory factory(&eq);
    factory.instantiateAll(config);

    auto* l1 = factory.getInstance("l1");
    const auto& upstream_ports = l1->getPortManager().getUpstreamPorts();
    REQUIRE(upstream_ports.size() == 2);
}

TEST_CASE("ConnectionResolutionTest ExcludeList", "[connection][resolution]") {
    EventQueue eq;

    json config = R"({
        "modules": [
            { "name": "cpu0", "type": "MockSim" },
            { "name": "cpu1", "type": "MockSim" },
            { "name": "l1", "type": "MockSim" }
        ],
        "connections": [
            {
                "src": "cpu*",
                "dst": "l1",
                "latency": 2,
                "exclude": ["cpu1"]
            }
        ]
    })"_json;

    ModuleFactory factory(&eq);
    factory.instantiateAll(config);

    auto* l1 = factory.getInstance("l1");
    const auto& upstream_ports = l1->getPortManager().getUpstreamPorts();
    REQUIRE(upstream_ports.size() == 1);  // 只有 cpu0
}

// DEF-02: Step 6/7b duplicate connection deduplication test
// When the same connection appears twice (or expands to same resolved pair),
// only ONE PortPair should be created (not two).
// This test verifies the connection deduplication behavior.
TEST_CASE("ConnectionResolutionTest DuplicateConnectionDeduplication", "[connection][resolution][defect02]") {
    EventQueue eq;

    // DEFECT REPRODUCTION: This config has "cpu0->l1" specified twice.
    // Without deduplication, Step 6 would create 2 PortPairs for the same
    // source port, leading to duplicate processing in the tick loop.
    json config = R"({
        "modules": [
            { "name": "cpu0", "type": "MockSim" },
            { "name": "cpu1", "type": "MockSim" },
            { "name": "l1", "type": "MockSim" }
        ],
        "connections": [
            // First explicit connection
            { "src": "cpu0", "dst": "l1", "latency": 2 },
            // DEFECT: Same connection specified again (should be deduplicated)
            { "src": "cpu0", "dst": "l1", "latency": 2 }
        ]
    })"_json;

    ModuleFactory factory(&eq);
    factory.instantiateAll(config);

    auto* l1 = factory.getInstance("l1");
    REQUIRE(l1 != nullptr);

    // With deduplication: only 1 upstream port from cpu0
    // Without deduplication: 2 upstream ports from cpu0 (duplicate)
    const auto& upstream_ports = l1->getPortManager().getUpstreamPorts();
    REQUIRE(upstream_ports.size() == 1);  // Must be 1, not 2
}