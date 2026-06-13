// test/test_topology_parser.cc
// 功能描述：TGMS v4.0 Phase 4.1 Hierarchy Tree Parser 单元测试
// 日期：2026-05-27

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/topology_node.hh"
#include "core/topology_parser.hh"

using json = nlohmann::json;

TEST_CASE("TopologyNode: Default constructor", "[topology][parser]") {
    cpptlm::TopologyNode node;
    REQUIRE(node.get_name().empty());
    REQUIRE(node.get_children().empty());
    REQUIRE(node.has_coherence() == false);
}

TEST_CASE("TopologyNode: Named constructor", "[topology][parser]") {
    cpptlm::TopologyNode node("cpu0");
    REQUIRE(node.get_name() == "cpu0");
}

TEST_CASE("TopologyNode: Add child and get children", "[topology][parser]") {
    auto parent = std::make_shared<cpptlm::TopologyNode>("parent");
    auto child = std::make_shared<cpptlm::TopologyNode>("child");
    parent->add_child(child);

    auto children = parent->get_children();
    REQUIRE(children.size() == 1);
    REQUIRE(children[0]->get_name() == "child");
}

TEST_CASE("TopologyNode: Parent relationship", "[topology][parser]") {
    auto parent = std::make_shared<cpptlm::TopologyNode>("parent");
    auto child = std::make_shared<cpptlm::TopologyNode>("child");
    parent->add_child(child);

    REQUIRE(child->get_parent() == "parent");
}

TEST_CASE("parse_hierarchy_tree: Simple tree", "[topology][parser]") {
    json hierarchy = json::parse(R"({
        "name": "system",
        "children": [
            {"name": "cpu0"},
            {"name": "cpu1"}
        ]
    })");

    auto root = cpptlm::parse_hierarchy_tree(hierarchy);
    REQUIRE(root != nullptr);
    REQUIRE(root->get_name() == "system");
    REQUIRE(root->get_children().size() == 2);
    REQUIRE(root->get_children()[0]->get_name() == "cpu0");
    REQUIRE(root->get_children()[1]->get_name() == "cpu1");
}

TEST_CASE("parse_hierarchy_tree: Nested tree", "[topology][parser]") {
    json hierarchy = json::parse(R"({
        "name": "system",
        "children": [
            {
                "name": "cluster0",
                "children": [
                    {"name": "cpu0"},
                    {"name": "cpu1"}
                ]
            }
        ]
    })");

    auto root = cpptlm::parse_hierarchy_tree(hierarchy);
    REQUIRE(root->get_name() == "system");
    REQUIRE(root->get_children().size() == 1);

    auto cluster = root->get_children()[0];
    REQUIRE(cluster->get_name() == "cluster0");
    REQUIRE(cluster->get_children().size() == 2);
    REQUIRE(cluster->get_children()[0]->get_name() == "cpu0");
    REQUIRE(cluster->get_children()[1]->get_name() == "cpu1");
}

TEST_CASE("parse_hierarchy_tree: Circular reference detection", "[topology][parser]") {
    json hierarchy = json::parse(R"({
        "name": "node1",
        "children": [
            {"name": "node2", "children": [
                {"name": "node1"}
            ]}
        ]
    })");

    REQUIRE_THROWS_AS(cpptlm::parse_hierarchy_tree(hierarchy), cpptlm::TopologyParseException);
}

TEST_CASE("parse_coherence_domains_array: Simple array", "[topology][parser]") {
    json coherence = json::parse(R"(["domain0", "domain1", "domain2"])");
    auto domains = cpptlm::parse_coherence_domains_array(coherence);

    REQUIRE(domains.size() == 3);
    REQUIRE(domains[0] == "domain0");
    REQUIRE(domains[1] == "domain1");
    REQUIRE(domains[2] == "domain2");
}

TEST_CASE("parse_coherence_domains_array: Object array", "[topology][parser]") {
    json coherence = json::parse(R"([
        {"name": "domain0"},
        {"name": "domain1"}
    ])");
    auto domains = cpptlm::parse_coherence_domains_array(coherence);

    REQUIRE(domains.size() == 2);
    REQUIRE(domains[0] == "domain0");
    REQUIRE(domains[1] == "domain1");
}

TEST_CASE("parse_hierarchy_tree_with_validation: With coherence domains", "[topology][parser]") {
    json hierarchy = json::parse(R"({
        "name": "system",
        "children": [
            {"name": "cpu0", "coherence_domain": "L2_cache"}
        ]
    })");

    json coherence = json::parse(R"(["L2_cache", "L3_cache"])");

    auto root = cpptlm::parse_hierarchy_tree_with_validation(hierarchy, coherence);
    REQUIRE(root != nullptr);
    REQUIRE(root->get_name() == "system");
}

TEST_CASE("parse_hierarchy_tree: Missing 'name' field throws", "[topology][parser]") {
    json hierarchy = json::parse(R"({"children": []})");
    REQUIRE_THROWS_AS(cpptlm::parse_hierarchy_tree(hierarchy), cpptlm::TopologyParseException);
}

TEST_CASE("parse_hierarchy_tree: Non-object throws", "[topology][parser]") {
    json hierarchy = json::parse(R"("not an object")");
    REQUIRE_THROWS_AS(cpptlm::parse_hierarchy_tree(hierarchy), cpptlm::TopologyParseException);
}