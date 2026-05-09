// test/test_router_six_stage.cc
// SPDX-License-Identifier: Apache-2.0
// 作者 CppTLM Team / 日期 2026-05-09
#include "catch_amalgamated.hpp"
#include "tlm/router_tlm.hh"
#include "core/event_queue.hh"
#include "bundles/noc_bundles_tlm.hh"

using namespace tlm;
using namespace bundles;

static void inject_flit(RouterTLM* router, unsigned port, uint64_t tid,
                        uint32_t dst, uint8_t vc, uint8_t flit_type) {
    NoCFlitBundle flit;
    flit.transaction_id.write(tid);
    flit.src_node.write(0);
    flit.dst_node.write(dst);
    flit.vc_id.write(vc);
    flit.flit_type.write(flit_type);
    flit.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    router->req_in(port).data() = flit;
    router->req_in(port).set_valid(true);
}

TEST_CASE("RouterTLM default construction", "[router][tlm]") {
    EventQueue eq;
    RouterTLM router("r0", &eq, 0, 0, 2, 2);
    REQUIRE(router.node_x() == 0);
    REQUIRE(router.node_y() == 0);
    REQUIRE(router.mesh_x() == 2);
    REQUIRE(router.mesh_y() == 2);
    REQUIRE(router.num_ports() == 5);
}

TEST_CASE("RouterTLM credit timeout configurable", "[router][tlm]") {
    EventQueue eq;
    RouterTLM router("r0", &eq, 0, 0, 2, 2);
    REQUIRE(router.credit_timeout() == 0);
    router.set_credit_timeout(100);
    REQUIRE(router.credit_timeout() == 100);
}

TEST_CASE("RouterTLM reverse port mapping", "[router][tlm]") {
    REQUIRE(RouterTLM::reverse_port(0) == 2);
    REQUIRE(RouterTLM::reverse_port(1) == 3);
    REQUIRE(RouterTLM::reverse_port(2) == 0);
    REQUIRE(RouterTLM::reverse_port(3) == 1);
    REQUIRE(RouterTLM::reverse_port(4) == 4);
}

TEST_CASE("RouterTLM node ID computation", "[router][tlm]") {
    EventQueue eq;
    RouterTLM r00("r00", &eq, 0, 0, 3, 3);
    REQUIRE(r00.node_id() == 0);
    RouterTLM r10("r10", &eq, 1, 0, 3, 3);
    REQUIRE(r10.node_id() == 1);
    RouterTLM r01("r01", &eq, 0, 1, 3, 3);
    REQUIRE(r01.node_id() == 3);
    RouterTLM r12("r12", &eq, 1, 2, 3, 3);
    REQUIRE(r12.node_id() == 7);
}

TEST_CASE("RouterTLM coord to node", "[router][tlm]") {
    REQUIRE(RoutingAlgorithm::coordToNode(0, 0, 3) == 0);
    REQUIRE(RoutingAlgorithm::coordToNode(1, 0, 3) == 1);
    REQUIRE(RoutingAlgorithm::coordToNode(0, 1, 3) == 3);
    REQUIRE(RoutingAlgorithm::coordToNode(2, 2, 3) == 8);
}

TEST_CASE("RouterTLM XY routing to east node", "[router][tlm]") {
    EventQueue eq;
    RouterTLM router("r0", &eq, 0, 0, 2, 2);
    uint32_t dst_node = RoutingAlgorithm::coordToNode(1, 0, 2);
    unsigned out_port = router.compute_xy_route(dst_node);
    REQUIRE(out_port == 1);
}

TEST_CASE("RouterTLM XY routing to north node", "[router][tlm]") {
    EventQueue eq;
    RouterTLM router("r0", &eq, 0, 0, 2, 2);
    uint32_t dst_node = RoutingAlgorithm::coordToNode(0, 1, 2);
    unsigned out_port = router.compute_xy_route(dst_node);
    REQUIRE(out_port == 0);
}

TEST_CASE("RouterTLM XY routing to west node", "[router][tlm]") {
    EventQueue eq;
    RouterTLM router("r1", &eq, 1, 0, 2, 2);
    uint32_t dst_node = RoutingAlgorithm::coordToNode(0, 0, 2);
    unsigned out_port = router.compute_xy_route(dst_node);
    REQUIRE(out_port == 3);
}

TEST_CASE("RouterTLM local routing", "[router][tlm]") {
    EventQueue eq;
    RouterTLM router("r0", &eq, 0, 0, 2, 2);
    uint32_t dst_node = router.node_id();
    unsigned out_port = router.compute_xy_route(dst_node);
    REQUIRE(out_port == 4);
}