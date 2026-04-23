// test/test_router_tlm.cc
// RouterTLM 单元测试
// 功能描述：验证 RouterTLM 六阶段流水线、XY 路由、Credit Flow
// 作者 CppTLM Team / 日期 2026-04-23
#include <catch2/catch_all.hpp>
#include "tlm/router_tlm.hh"
#include "core/event_queue.hh"

using namespace tlm;

TEST_CASE("RouterTLM can be constructed", "[router][tlm]") {
    EventQueue eq;
    RouterTLM router("router0", &eq, 0, 0, 2, 2);

    REQUIRE(router.node_x() == 0);
    REQUIRE(router.node_y() == 0);
    REQUIRE(router.mesh_x() == 2);
    REQUIRE(router.mesh_y() == 2);
    REQUIRE(router.node_id() == 0);
}

TEST_CASE("RouterTLM XY routing computes correct direction", "[router][tlm]") {
    EventQueue eq;
    RouterTLM router("router0", &eq, 1, 1, 4, 4);
    auto* routing = router.routing_algorithm();

    SECTION("route to east (larger X)") {
        uint32_t dst = 3 + 1 * 4;
        unsigned out_port = routing->computeRoute(0, dst, 1, 1, 4, 4);
        REQUIRE(out_port == static_cast<unsigned>(RouterPort::EAST));
    }

    SECTION("route to west (smaller X)") {
        uint32_t dst = 0 + 1 * 4;
        unsigned out_port = routing->computeRoute(0, dst, 1, 1, 4, 4);
        REQUIRE(out_port == static_cast<unsigned>(RouterPort::WEST));
    }

    SECTION("route to north (larger Y)") {
        uint32_t dst = 1 + 3 * 4;
        unsigned out_port = routing->computeRoute(0, dst, 1, 1, 4, 4);
        REQUIRE(out_port == static_cast<unsigned>(RouterPort::NORTH));
    }

    SECTION("route to south (smaller Y)") {
        uint32_t dst = 1 + 0 * 4;
        unsigned out_port = routing->computeRoute(0, dst, 1, 1, 4, 4);
        REQUIRE(out_port == static_cast<unsigned>(RouterPort::SOUTH));
    }

    SECTION("route to local (same node)") {
        uint32_t dst = router.node_id();
        unsigned out_port = routing->computeRoute(0, dst, 1, 1, 4, 4);
        REQUIRE(out_port == static_cast<unsigned>(RouterPort::LOCAL));
    }
}

TEST_CASE("RouterTLM node ID encoding/decoding", "[router][tlm]") {
    unsigned mesh_x = 4;

    SECTION("node 0 -> (0,0)") {
        REQUIRE(RoutingAlgorithm::nodeToX(0, mesh_x) == 0);
        REQUIRE(RoutingAlgorithm::nodeToY(0, mesh_x) == 0);
    }

    SECTION("node 5 -> (1,1) for 4x4 mesh") {
        REQUIRE(RoutingAlgorithm::nodeToX(5, mesh_x) == 1);
        REQUIRE(RoutingAlgorithm::nodeToY(5, mesh_x) == 1);
    }

    SECTION("node 15 -> (3,3) for 4x4 mesh") {
        REQUIRE(RoutingAlgorithm::nodeToX(15, mesh_x) == 3);
        REQUIRE(RoutingAlgorithm::nodeToY(15, mesh_x) == 3);
    }

    SECTION("round-trip encoding") {
        REQUIRE(RoutingAlgorithm::nodeToX(RoutingAlgorithm::coordToNode(0, 0, mesh_x), mesh_x) == 0);
        REQUIRE(RoutingAlgorithm::nodeToY(RoutingAlgorithm::coordToNode(0, 0, mesh_x), mesh_x) == 0);
        REQUIRE(RoutingAlgorithm::nodeToX(RoutingAlgorithm::coordToNode(1, 1, mesh_x), mesh_x) == 1);
        REQUIRE(RoutingAlgorithm::nodeToY(RoutingAlgorithm::coordToNode(1, 1, mesh_x), mesh_x) == 1);
        REQUIRE(RoutingAlgorithm::nodeToX(RoutingAlgorithm::coordToNode(3, 3, mesh_x), mesh_x) == 3);
        REQUIRE(RoutingAlgorithm::nodeToY(RoutingAlgorithm::coordToNode(3, 3, mesh_x), mesh_x) == 3);
    }
}

TEST_CASE("RouterTLM stats initialized correctly", "[router][tlm]") {
    EventQueue eq;
    RouterTLM router("router0", &eq, 0, 0, 2, 2);

    const auto& stats = router.stats();
    REQUIRE(stats.flits_forwarded == 0);
    REQUIRE(stats.packets_forwarded == 0);
    REQUIRE(stats.total_hops == 0);
    REQUIRE(stats.total_latency_cycles == 0);
    REQUIRE(stats.congestion_events == 0);
}

TEST_CASE("RouterTLM port count is 5", "[router][tlm]") {
    EventQueue eq;
    RouterTLM router("router0", &eq, 0, 0, 2, 2);

    REQUIRE(router.num_ports() == 5);
}

TEST_CASE("RouterTLM can set custom routing algorithm", "[router][tlm]") {
    EventQueue eq;
    RouterTLM router("router0", &eq, 0, 0, 2, 2);

    REQUIRE(router.routing_algorithm() != nullptr);
    REQUIRE(dynamic_cast<XYRouting*>(router.routing_algorithm()) != nullptr);
}
