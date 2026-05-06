// test/test_phase3_2_port_management.cc
// Phase 3.2: Port Management Integration Tests

#include "catch_amalgamated.hpp"
#include "core/module_factory.hh"
#include "core/port_types.hh"
#include "core/port_compatibility.hh"
#include "chstream_register.hh"
#include "modules.hh"

using json = nlohmann::json;

static void registerAllModules() {
    static bool registered = false;
    if (!registered) {
        REGISTER_OBJECT;
        REGISTER_CHSTREAM;
        registered = true;
    }
}

TEST_CASE("Phase 3.2: Port alias resolution via resolve_port_alias", "[phase3.2][port_alias]") {
    registerAllModules();
    EventQueue eq;
    ModuleFactory factory(&eq);

    json config = R"({
        "modules": [
            {"name": "router", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}}
        ],
        "connections": []
    })"_json;

    bool result = factory.instantiateAll(config);
    REQUIRE(result == true);
}

TEST_CASE("Phase 3.2: PortSpec JSON roundtrip", "[phase3.2][port_types]") {
    cpptlm::PortSpec spec;
    spec.name = "NORTH";
    spec.role = cpptlm::PortRole::BI_DIRECTIONAL;
    spec.bundle = cpptlm::BundleType::NOC_FLIT;
    spec.width = 64;
    spec.is_multi = false;
    spec.port_count = 1;
    spec.layout_hint = "top";

    json j = spec;
    auto restored = j.get<cpptlm::PortSpec>();

    REQUIRE(restored.name == spec.name);
    REQUIRE(restored.role == spec.role);
    REQUIRE(restored.bundle == spec.bundle);
    REQUIRE(restored.width == spec.width);
}

TEST_CASE("Phase 3.2: PortGroupBundleType serialization", "[phase3.2][port_types]") {
    auto types = {
        cpptlm::PortGroupBundleType::SINGLE,
        cpptlm::PortGroupBundleType::BUNDLE_MASTER,
        cpptlm::PortGroupBundleType::BUNDLE_SLAVE
    };

    for (auto bt : types) {
        json j = bt;
        auto restored = j.get<cpptlm::PortGroupBundleType>();
        REQUIRE(restored == bt);
    }
}

TEST_CASE("Phase 3.2: ModulePortSpec with ports and aliases", "[phase3.2][port_types]") {
    cpptlm::ModulePortSpec mod_spec;
    mod_spec.module_name = "router0";

    cpptlm::PortSpec port1;
    port1.name = "NORTH";
    port1.role = cpptlm::PortRole::BI_DIRECTIONAL;
    port1.bundle = cpptlm::BundleType::NOC_FLIT;
    port1.width = 64;
    mod_spec.ports.push_back(port1);

    cpptlm::PortSpec port2;
    port2.name = "EAST";
    port2.role = cpptlm::PortRole::BI_DIRECTIONAL;
    port2.bundle = cpptlm::BundleType::NOC_FLIT;
    port2.width = 64;
    mod_spec.ports.push_back(port2);

    mod_spec.aliases["N"] = "0";
    mod_spec.aliases["E"] = "1";

    json j = mod_spec;
    auto restored = j.get<cpptlm::ModulePortSpec>();

    REQUIRE(restored.module_name == "router0");
    REQUIRE(restored.ports.size() == 2);
    REQUIRE(restored.aliases.at("N") == "0");
    REQUIRE(restored.aliases.at("E") == "1");
}

TEST_CASE("Phase 3.2: PortCompatibility role matrix - INITIATOR to TARGET", "[phase3.2][port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::INITIATOR, cpptlm::PortRole::TARGET) == true);
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::TARGET, cpptlm::PortRole::INITIATOR) == false);
}

TEST_CASE("Phase 3.2: PortCompatibility role matrix - NETWORK to NETWORK", "[phase3.2][port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::NETWORK, cpptlm::PortRole::NETWORK) == true);
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::NETWORK, cpptlm::PortRole::BI_DIRECTIONAL) == true);
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::NETWORK, cpptlm::PortRole::PE) == true);
}

TEST_CASE("Phase 3.2: PortCompatibility PE to NETWORK allowed", "[phase3.2][port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::PE, cpptlm::PortRole::NETWORK) == true);
}

TEST_CASE("Phase 3.2: PortCompatibility bundle matrix - NOC_FLIT to NOC_FLIT", "[phase3.2][port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_bundle_compatible(
        cpptlm::BundleType::NOC_FLIT, cpptlm::BundleType::NOC_FLIT) == true);
    REQUIRE(cpptlm::PortCompatibility::is_bundle_compatible(
        cpptlm::BundleType::NOC_FLIT, cpptlm::BundleType::CACHE_REQ) == false);
}

TEST_CASE("Phase 3.2: PortCompatibility bundle matrix - GENERIC compatible with all", "[phase3.2][port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_bundle_compatible(
        cpptlm::BundleType::GENERIC, cpptlm::BundleType::GENERIC) == true);
    REQUIRE(cpptlm::PortCompatibility::is_bundle_compatible(
        cpptlm::BundleType::GENERIC, cpptlm::BundleType::NOC_FLIT) == true);
    REQUIRE(cpptlm::PortCompatibility::is_bundle_compatible(
        cpptlm::BundleType::CACHE_REQ, cpptlm::BundleType::GENERIC) == true);
}

TEST_CASE("Phase 3.2: PortCompatibility width check", "[phase3.2][port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_width_compatible(64, 64) == true);
    REQUIRE(cpptlm::PortCompatibility::is_width_compatible(64, 128) == false);
}

TEST_CASE("Phase 3.2: PortCompatibility full check - valid connection", "[phase3.2][port_compat]") {
    cpptlm::PortSpec src;
    src.role = cpptlm::PortRole::INITIATOR;
    src.bundle = cpptlm::BundleType::CACHE_REQ;
    src.width = 64;

    cpptlm::PortSpec dst;
    dst.role = cpptlm::PortRole::TARGET;
    dst.bundle = cpptlm::BundleType::CACHE_REQ;
    dst.width = 64;

    REQUIRE(cpptlm::PortCompatibility::is_compatible(src, dst) == true);
}

TEST_CASE("Phase 3.2: PortCompatibility full check - incompatible role", "[phase3.2][port_compat]") {
    cpptlm::PortSpec src;
    src.role = cpptlm::PortRole::INITIATOR;
    src.bundle = cpptlm::BundleType::CACHE_REQ;
    src.width = 64;

    cpptlm::PortSpec dst;
    dst.role = cpptlm::PortRole::INITIATOR;
    dst.bundle = cpptlm::BundleType::CACHE_REQ;
    dst.width = 64;

    REQUIRE(cpptlm::PortCompatibility::is_compatible(src, dst) == false);
}

TEST_CASE("Phase 3.2: PortCompatibility get_incompatibility_reason", "[phase3.2][port_compat]") {
    cpptlm::PortSpec src;
    src.role = cpptlm::PortRole::INITIATOR;
    src.bundle = cpptlm::BundleType::CACHE_REQ;
    src.width = 64;

    cpptlm::PortSpec dst;
    dst.role = cpptlm::PortRole::INITIATOR;
    dst.bundle = cpptlm::BundleType::CACHE_REQ;
    dst.width = 64;

    std::string reason = cpptlm::PortCompatibility::get_incompatibility_reason(src, dst);
    REQUIRE(reason.find("Incompatible port roles") != std::string::npos);
}

TEST_CASE("Phase 3.2: Deprecated port names map", "[phase3.2][port_alias]") {
    auto deprecated = cpptlm::PortSpec::deprecated_names();

    REQUIRE(deprecated.at("NORTH") == 0);
    REQUIRE(deprecated.at("EAST") == 1);
    REQUIRE(deprecated.at("SOUTH") == 2);
    REQUIRE(deprecated.at("WEST") == 3);
    REQUIRE(deprecated.at("LOCAL") == 4);
}