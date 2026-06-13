#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include <catch2/catch_all.hpp>

TEST_CASE("T3.1-11a: Missing modules field", "[validate][phase3]") {
    EventQueue eq;
    json config = R"({"connections": []})"_json;
    ModuleFactory factory(&eq);
    bool result = factory.instantiateAll(config);
    CHECK(result == false);
}

TEST_CASE("T3.1-11b: Missing module name", "[validate][phase3]") {
    EventQueue eq;
    json config = R"({
        "modules": [{"type": "RouterTLM"}],
        "connections": []
    })"_json;
    ModuleFactory factory(&eq);
    bool result = factory.instantiateAll(config);
    CHECK(result == false);
}

TEST_CASE("T3.1-11c: RouterTLM missing required params", "[validate][phase3]") {
    EventQueue eq;
    json config = R"({
        "modules": [{"name": "r0", "type": "RouterTLM", "params": {"node_x": 0}}],
        "connections": []
    })"_json;
    ModuleFactory factory(&eq);
    bool result = factory.instantiateAll(config);
    CHECK(result == false);
}

TEST_CASE("T3.1-11d: Valid config passes validation", "[validate][phase3]") {
    EventQueue eq;
    json config = R"({
        "version": "1.0",
        "modules": [
            {"name": "r0", "type": "RouterTLM", "params": {
                "node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1
            }}
        ],
        "connections": []
    })"_json;
    ModuleFactory factory(&eq);
    bool result = factory.instantiateAll(config);
    CHECK(result == true);
}