// test/test_nic_tlm.cc
// NICTLM 单元测试
// 功能描述：验证 NICTLM packetize/reassemble 功能
// 作者 CppTLM Team / 日期 2026-04-24
#include <catch2/catch_all.hpp>
#include "tlm/nic_tlm.hh"
#include "core/event_queue.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh"

using namespace tlm;

static void inject_req(NICTLM* nic, uint64_t tid, uint64_t addr, bool wr, uint64_t data = 0) {
    bundles::CacheReqBundle req;
    req.transaction_id.write(tid);
    req.address.write(addr);
    req.is_write.write(wr ? 1 : 0);
    req.data.write(data);
    req.size.write(8);
    nic->pe_req_in().data() = req;
    nic->pe_req_in().set_valid(true);
}

TEST_CASE("NICTLM can be constructed", "[nic][tlm]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);

    REQUIRE(nic.node_id() == 0);
    REQUIRE(nic.mesh_x() == 4);
    REQUIRE(nic.mesh_y() == 4);
    REQUIRE(nic.get_module_type() == "NICTLM");
}

TEST_CASE("NICTLM num_ports returns 4", "[nic][tlm]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);

    REQUIRE(nic.num_ports() == 4);
}

TEST_CASE("NICTLM address map can be configured", "[nic][tlm]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);

    nic.add_address_region(0x1000, 0x1000, 5);
    REQUIRE(nic.lookup_node(0x1000) == 5);
    REQUIRE(nic.lookup_node(0x1500) == 5);
    REQUIRE(nic.lookup_node(0x2000) == 0);
}

TEST_CASE("NICTLM packetize produces flits", "[nic][tlm]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);
    nic.add_address_region(0x1000, 0x1000, 5);

    inject_req(&nic, 100, 0x1500, false, 0xDEAD);
    nic.tick();

    REQUIRE(nic.net_req_out().valid());
}

TEST_CASE("NICTLM flit has correct metadata", "[nic][tlm]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 3, 4, 4);
    nic.add_address_region(0x2000, 0x1000, 7);

    inject_req(&nic, 200, 0x2500, false, 0xBEEF);
    nic.tick();

    auto flit = nic.net_req_out().data();
    REQUIRE(flit.transaction_id.read() == 200);
    REQUIRE(flit.src_node.read() == 3);
    REQUIRE(flit.dst_node.read() == 7);
    REQUIRE(flit.flit_category.read() == bundles::NoCFlitBundle::CATEGORY_REQUEST);
}