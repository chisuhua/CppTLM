// test/test_port_types.cc
// Phase 3.2: Port Types and Compatibility Tests

#include "catch_amalgamated.hpp"
#include "core/port_types.hh"
#include "core/port_compatibility.hh"

using json = nlohmann::json;

TEST_CASE("PortRole: JSON serialization roundtrip", "[port_types]") {
    cpptlm::PortRole roles[] = {
        cpptlm::PortRole::INITIATOR,
        cpptlm::PortRole::TARGET,
        cpptlm::PortRole::BI_DIRECTIONAL,
        cpptlm::PortRole::NETWORK,
        cpptlm::PortRole::PE
    };

    for (auto role : roles) {
        json j = role;
        auto restored = j.get<cpptlm::PortRole>();
        REQUIRE(restored == role);
    }
}

TEST_CASE("BundleType: JSON serialization roundtrip", "[port_types]") {
    cpptlm::BundleType bundles[] = {
        cpptlm::BundleType::CACHE_REQ,
        cpptlm::BundleType::CACHE_RESP,
        cpptlm::BundleType::NOC_FLIT,
        cpptlm::BundleType::GENERIC
    };

    for (auto bundle : bundles) {
        json j = bundle;
        auto restored = j.get<cpptlm::BundleType>();
        REQUIRE(restored == bundle);
    }
}

TEST_CASE("PortSpec: JSON serialization roundtrip", "[port_types]") {
    cpptlm::PortSpec spec;
    spec.name = "NORTH";
    spec.role = cpptlm::PortRole::BI_DIRECTIONAL;
    spec.bundle = cpptlm::BundleType::NOC_FLIT;
    spec.width = 64;
    spec.is_multi = true;
    spec.port_count = 5;
    spec.layout_hint = "top";

    json j = spec;
    auto restored = j.get<cpptlm::PortSpec>();

    REQUIRE(restored.name == spec.name);
    REQUIRE(restored.role == spec.role);
    REQUIRE(restored.bundle == spec.bundle);
    REQUIRE(restored.width == spec.width);
    REQUIRE(restored.is_multi == spec.is_multi);
    REQUIRE(restored.port_count == spec.port_count);
    REQUIRE(restored.layout_hint == spec.layout_hint);
}

TEST_CASE("PortSpec: deprecated names resolution", "[port_types]") {
    auto deprecated = cpptlm::PortSpec::deprecated_names();

    REQUIRE(deprecated.at("NORTH") == 0);
    REQUIRE(deprecated.at("EAST") == 1);
    REQUIRE(deprecated.at("SOUTH") == 2);
    REQUIRE(deprecated.at("WEST") == 3);
    REQUIRE(deprecated.at("LOCAL") == 4);

    REQUIRE(deprecated.at("N_in") == 0);
    REQUIRE(deprecated.at("E_out") == 1);
}

TEST_CASE("PortGroupMember: JSON serialization roundtrip", "[port_types]") {
    cpptlm::PortGroupMember member;
    member.index = 3;
    member.role = cpptlm::PortRole::NETWORK;
    member.bundle = cpptlm::BundleType::NOC_FLIT;

    json j = member;
    auto restored = j.get<cpptlm::PortGroupMember>();

    REQUIRE(restored.index == member.index);
    REQUIRE(restored.role == member.role);
    REQUIRE(restored.bundle == member.bundle);
}

TEST_CASE("PortGroupSpec: JSON serialization roundtrip", "[port_types]") {
    cpptlm::PortGroupSpec group;
    group.name = "PE_PORTS";
    group.bundle_type = cpptlm::PortGroupBundleType::BUNDLE_MASTER;

    cpptlm::PortGroupMember m1;
    m1.index = 0;
    m1.role = cpptlm::PortRole::PE;
    m1.bundle = cpptlm::BundleType::CACHE_REQ;
    group.ports.push_back(m1);

    cpptlm::PortGroupMember m2;
    m2.index = 1;
    m2.role = cpptlm::PortRole::NETWORK;
    m2.bundle = cpptlm::BundleType::NOC_FLIT;
    group.ports.push_back(m2);

    json j = group;
    auto restored = j.get<cpptlm::PortGroupSpec>();

    REQUIRE(restored.name == group.name);
    REQUIRE(restored.bundle_type == group.bundle_type);
    REQUIRE(restored.ports.size() == 2);
    REQUIRE(restored.ports[0].index == 0);
    REQUIRE(restored.ports[1].index == 1);
}

TEST_CASE("ModulePortSpec: JSON serialization roundtrip", "[port_types]") {
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

    REQUIRE(restored.module_name == mod_spec.module_name);
    REQUIRE(restored.ports.size() == 2);
    REQUIRE(restored.aliases.at("N") == "0");
    REQUIRE(restored.aliases.at("E") == "1");
}

TEST_CASE("PortCompatibility: L1 role matrix INITIATOR->TARGET", "[port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::INITIATOR, cpptlm::PortRole::TARGET) == true);
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::TARGET, cpptlm::PortRole::INITIATOR) == false);
}

TEST_CASE("PortCompatibility: L1 role matrix BI_DIRECTIONAL", "[port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::PortRole::BI_DIRECTIONAL) == true);
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::PortRole::INITIATOR) == true);
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::PortRole::PE) == false);
}

TEST_CASE("PortCompatibility: L1 role matrix NETWORK", "[port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::NETWORK, cpptlm::PortRole::NETWORK) == true);
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::NETWORK, cpptlm::PortRole::BI_DIRECTIONAL) == true);
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::NETWORK, cpptlm::PortRole::PE) == true);
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::NETWORK, cpptlm::PortRole::INITIATOR) == false);
}

TEST_CASE("PortCompatibility: L1 role matrix PE", "[port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::PE, cpptlm::PortRole::NETWORK) == true);
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::PE, cpptlm::PortRole::PE) == false);
    REQUIRE(cpptlm::PortCompatibility::is_role_compatible(
        cpptlm::PortRole::PE, cpptlm::PortRole::BI_DIRECTIONAL) == false);
}

TEST_CASE("PortCompatibility: L2 bundle matrix NOC_FLIT", "[port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_bundle_compatible(
        cpptlm::BundleType::NOC_FLIT, cpptlm::BundleType::NOC_FLIT) == true);
    REQUIRE(cpptlm::PortCompatibility::is_bundle_compatible(
        cpptlm::BundleType::NOC_FLIT, cpptlm::BundleType::CACHE_REQ) == false);
}

TEST_CASE("PortCompatibility: L2 bundle matrix GENERIC", "[port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_bundle_compatible(
        cpptlm::BundleType::GENERIC, cpptlm::BundleType::GENERIC) == true);
    REQUIRE(cpptlm::PortCompatibility::is_bundle_compatible(
        cpptlm::BundleType::GENERIC, cpptlm::BundleType::NOC_FLIT) == true);
    REQUIRE(cpptlm::PortCompatibility::is_bundle_compatible(
        cpptlm::BundleType::CACHE_REQ, cpptlm::BundleType::GENERIC) == true);
}

TEST_CASE("PortCompatibility: L3 width check", "[port_compat]") {
    REQUIRE(cpptlm::PortCompatibility::is_width_compatible(64, 64) == true);
    REQUIRE(cpptlm::PortCompatibility::is_width_compatible(64, 128) == false);
}

TEST_CASE("PortCompatibility: is_compatible combines L1+L2", "[port_compat]") {
    cpptlm::PortSpec src;
    src.role = cpptlm::PortRole::INITIATOR;
    src.bundle = cpptlm::BundleType::CACHE_REQ;
    src.width = 64;

    cpptlm::PortSpec dst;
    dst.role = cpptlm::PortRole::TARGET;
    dst.bundle = cpptlm::BundleType::CACHE_REQ;
    dst.width = 64;

    REQUIRE(cpptlm::PortCompatibility::is_compatible(src, dst) == true);

    dst.role = cpptlm::PortRole::INITIATOR;
    REQUIRE(cpptlm::PortCompatibility::is_compatible(src, dst) == false);
}

TEST_CASE("PortCompatibility: role_name returns correct string", "[port_compat]") {
    REQUIRE(strcmp(cpptlm::PortCompatibility::role_name(cpptlm::PortRole::INITIATOR), "initiator") == 0);
    REQUIRE(strcmp(cpptlm::PortCompatibility::role_name(cpptlm::PortRole::TARGET), "target") == 0);
    REQUIRE(strcmp(cpptlm::PortCompatibility::role_name(cpptlm::PortRole::BI_DIRECTIONAL), "bi_directional") == 0);
    REQUIRE(strcmp(cpptlm::PortCompatibility::role_name(cpptlm::PortRole::NETWORK), "network") == 0);
    REQUIRE(strcmp(cpptlm::PortCompatibility::role_name(cpptlm::PortRole::PE), "pe") == 0);
}

TEST_CASE("PortCompatibility: bundle_name returns correct string", "[port_compat]") {
    REQUIRE(strcmp(cpptlm::PortCompatibility::bundle_name(cpptlm::BundleType::CACHE_REQ), "cache_req") == 0);
    REQUIRE(strcmp(cpptlm::PortCompatibility::bundle_name(cpptlm::BundleType::CACHE_RESP), "cache_resp") == 0);
    REQUIRE(strcmp(cpptlm::PortCompatibility::bundle_name(cpptlm::BundleType::NOC_FLIT), "noc_flit") == 0);
    REQUIRE(strcmp(cpptlm::PortCompatibility::bundle_name(cpptlm::BundleType::GENERIC), "generic") == 0);
}