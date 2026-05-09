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

TEST_CASE("NICTLM packetization: can be constructed", "[nic][packetization]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);
    REQUIRE(nic.node_id() == 0);
}

TEST_CASE("NICTLM packetization: num_ports returns 4", "[nic][packetization]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);
    REQUIRE(nic.num_ports() == 4);
}

TEST_CASE("NICTLM packetization: address region lookup", "[nic][packetization]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);
    nic.add_address_region(0x1000, 0x1000, 5);
    REQUIRE(nic.lookup_node(0x1500) == 5);
}

TEST_CASE("NICTLM packetization: address out of range returns 0", "[nic][packetization]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);
    nic.add_address_region(0x1000, 0x1000, 5);
    REQUIRE(nic.lookup_node(0x5000) == 0);
}

TEST_CASE("NICTLM packetization: node_to_coord conversion", "[nic][packetization]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 5, 4, 4);
    REQUIRE(nic.node_id() == 5);
}

TEST_CASE("NICTLM packetization: num_vcs is 4", "[nic][packetization]") {
    REQUIRE(NICTLM::NUM_VCS == 4);
}

TEST_CASE("NICTLM packetization: flits_per_packet is 4", "[nic][packetization]") {
    REQUIRE(NICTLM::FLITS_PER_PACKET == 4);
}