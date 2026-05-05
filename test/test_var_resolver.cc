// test/test_var_resolver.cc
// T3.1-07: Variable reference ${path} resolution tests

#include "catch_amalgamated.hpp"
#include "utils/var_resolver.hh"

using json = nlohmann::json;

TEST_CASE("T3.1-07a: Basic variable reference", "[var_ref][phase3]") {
    json config = R"({
        "settings": {"base_latency": 1},
        "modules": [{"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}],
        "connections": [{"src": "r0.0", "dst": "r0.1", "latency": "${settings.base_latency}"}]
    })"_json;

    cpptlm::VarResolver resolver(config);
    auto resolved = resolver.resolveAll(config);
    CHECK(resolved["connections"][0]["latency"] == 1);
}

TEST_CASE("T3.1-07b: Module name reference", "[var_ref][phase3]") {
    json config = R"({
        "modules": [
            {"name": "cpu0", "type": "CPUTLM"},
            {"name": "r0", "type": "RouterTLM", "params": {"cpu_type": "${modules[0].type}", "node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}
        ],
        "connections": []
    })"_json;

    cpptlm::VarResolver resolver(config);
    auto resolved = resolver.resolveAll(config);
    CHECK(resolved["modules"][1]["params"]["cpu_type"] == "CPUTLM");
}

TEST_CASE("T3.1-07c: Array element reference", "[var_ref][phase3]") {
    json config = R"({
        "routers": ["r0", "r1", "r2"],
        "modules": [
            {"name": "r0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 1, "mesh_y": 1}},
            {"name": "r1", "type": "RouterTLM", "params": {"node_x": 1, "node_y": 0, "mesh_x": 1, "mesh_y": 1}}
        ],
        "connections": [{"src": "${routers[0]}.0", "dst": "${routers[1]}.1", "latency": 1}]
    })"_json;

    cpptlm::VarResolver resolver(config);
    auto resolved = resolver.resolveAll(config);
    CHECK(resolved["connections"][0]["src"] == "r0.0");
    CHECK(resolved["connections"][0]["dst"] == "r1.1");
}

TEST_CASE("T3.1-07d: Unresolved variable warning", "[var_ref][phase3]") {
    json config = R"({
        "connections": [{"src": "cpu0", "dst": "r0", "latency": "${undefined.path}"}]
    })"_json;

    cpptlm::VarResolver resolver(config);
    auto resolved = resolver.resolveAll(config);
    CHECK(resolved["connections"][0]["latency"] == "${undefined.path}");
}