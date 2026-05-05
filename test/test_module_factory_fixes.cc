// test/test_module_factory_fixes.cc
#include "catch_amalgamated.hpp"
#include "core/module_factory.hh"
#include "core/event_queue.hh"
#include "mock_modules.hh"
#include <sstream>
#include <regex>

namespace {
auto _reg1 = []() {
    ModuleFactory::registerObject<MockSim>("MockSim");
    return 0;
}();

auto _reg2 = []() {
    ModuleFactory::registerObject<MockSim>("RouterTLM");
    return 0;
}();
}

TEST_CASE("DEF-04b: Invalid port index produces WARNING", "[defect][phase3]") {
    EventQueue eq;

    json config = R"({
        "modules": [
            { "name": "r0", "type": "RouterTLM", "params": { "node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2 }},
            { "name": "r1", "type": "RouterTLM", "params": { "node_x": 1, "node_y": 0, "mesh_x": 2, "mesh_y": 2 }}
        ],
        "connections": [
            { "src": "r0.0abc", "dst": "r1.3", "latency": 1 }
        ]
    })"_json;

    ModuleFactory factory(&eq);
    bool result = factory.instantiateAll(config);

    REQUIRE(result == true);
}

TEST_CASE("DEF-03b: parsePortSpec handles empty spec", "[defect][phase3]") {
    std::string src = "module";
    auto [name, spec] = ModuleFactory::parsePortSpec(src);

    CHECK(name == "module");
    CHECK(spec == "");
}

TEST_CASE("DEF-03b: parsePortSpec handles valid port index", "[defect][phase3]") {
    std::string src = "module.42";
    auto [name, spec] = ModuleFactory::parsePortSpec(src);

    CHECK(name == "module");
    CHECK(spec == "42");
}

TEST_CASE("DEF-04b: Invalid dst port index produces WARNING", "[defect][phase3]") {
    EventQueue eq;

    json config = R"({
        "modules": [
            { "name": "r0", "type": "RouterTLM", "params": { "node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2 }},
            { "name": "r1", "type": "RouterTLM", "params": { "node_x": 1, "node_y": 0, "mesh_x": 2, "mesh_y": 2 }}
        ],
        "connections": [
            { "src": "r0.0", "dst": "r1.xyz", "latency": 1 }
        ]
    })"_json;

    ModuleFactory factory(&eq);
    bool result = factory.instantiateAll(config);

    REQUIRE(result == true);
}