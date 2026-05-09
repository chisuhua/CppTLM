// test/test_nic_packetization.cc
// SPDX-License-Identifier: Apache-2.0
// 作者 CppTLM Team / 日期 2026-05-09
#include "catch_amalgamated.hpp"
#include "tlm/nic_tlm.hh"
#include "core/event_queue.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh"

using namespace tlm;
using namespace bundles;

TEST_CASE("NICTLM construction and node identity", "[nic][packetization]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);
    REQUIRE(nic.node_id() == 0);
    REQUIRE(nic.mesh_x() == 4);
    REQUIRE(nic.mesh_y() == 4);
    REQUIRE(nic.get_module_type() == "NICTLM");
}

TEST_CASE("NICTLM port count", "[nic][packetization]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);
    REQUIRE(nic.num_ports() == 4);
}

TEST_CASE("NICTLM address region lookup", "[nic][packetization]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);
    nic.add_address_region(0x1000, 0x1000, 5);
    nic.add_address_region(0x5000, 0x1000, 7);
    REQUIRE(nic.lookup_node(0x1500) == 5);
    REQUIRE(nic.lookup_node(0x1000) == 5);
    REQUIRE(nic.lookup_node(0x1FFF) == 5);
    REQUIRE(nic.lookup_node(0x5000) == 7);
    REQUIRE(nic.lookup_node(0x5FFF) == 7);
    REQUIRE(nic.lookup_node(0x6000) == 0);
    REQUIRE(nic.lookup_node(0x0000) == 0);
}

TEST_CASE("NICTLM node ID from coordinates", "[nic][packetization]") {
    EventQueue eq;
    NICTLM nic0("nic0", &eq, 0, 4, 4);
    NICTLM nic5("nic5", &eq, 5, 4, 4);
    NICTLM nic15("nic15", &eq, 15, 4, 4);
    REQUIRE(nic0.node_id() == 0);
    REQUIRE(nic5.node_id() == 5);
    REQUIRE(nic15.node_id() == 15);
}

TEST_CASE("NICTLM static constants", "[nic][packetization]") {
    REQUIRE(NICTLM::NUM_VCS == 4);
    REQUIRE(NICTLM::FLITS_PER_PACKET == 4);
    REQUIRE(NICTLM::MAX_PENDING_PACKETS == 16);
}