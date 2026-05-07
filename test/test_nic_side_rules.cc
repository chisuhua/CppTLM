// test/test_nic_side_rules.cc
#include <catch2/catch_all.hpp>
#include "modules.hh"
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"

static void registerAllModules() {
    static bool registered = false;
    if (!registered) {
        REGISTER_OBJECT;
        REGISTER_CHSTREAM;
        registered = true;
    }
}

TEST_CASE("T3.1-09: NICTLM PE-side to Router rejected", "[nic][phase3]") {
    registerAllModules();
    EventQueue eq;
    json config = R"({
        "modules": [
            {"name": "ni0", "type": "NICTLM", "params": {"node_id": 0}},
            {"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}
        ],
        "connections": [
            {"src": "ni0.0", "dst": "r0.4", "latency": 1}
        ]
    })"_json;

    ModuleFactory factory(&eq);
    bool result = factory.instantiateAll(config);
    CHECK(result == false);
}

TEST_CASE("T3.1-09b: NICTLM NETWORK-side to Router allowed", "[nic][phase3]") {
    registerAllModules();
    EventQueue eq;
    json config = R"({
        "modules": [
            {"name": "ni0", "type": "NICTLM", "params": {"node_id": 0}},
            {"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}
        ],
        "connections": [
            {"src": "ni0.1", "dst": "r0.4", "latency": 1}
        ]
    })"_json;

    ModuleFactory factory(&eq);
    bool result = factory.instantiateAll(config);
    CHECK(result == true);
}