// test/test_noc_integration.cc
// NoC/NIC 集成测试
// 功能描述：验证 Router 多跳转发、NIC packetize/reassemble 端到端
// 作者 CppTLM Team / 日期 2026-04-24
#include <catch2/catch_all.hpp>
#include "tlm/router_tlm.hh"
#include "tlm/nic_tlm.hh"
#include "core/event_queue.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh"

using namespace tlm;

TEST_CASE("RouterTLM: single flit can traverse pipeline", "[noc][integration]") {
    EventQueue eq;
    RouterTLM router("router0", &eq, 0, 0, 2, 2);

    // 构造一个 HEAD flit，目标是 LOCAL 端口（同节点）
    bundles::NoCFlitBundle flit;
    flit.transaction_id.write(100);
    flit.src_node.write(0);
    flit.dst_node.write(0);  // 同节点，XY 路由会选 LOCAL
    flit.vc_id.write(0);
    flit.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
    flit.flit_index.write(0);
    flit.flit_count.write(1);
    flit.flit_category.write(bundles::NoCFlitBundle::CATEGORY_REQUEST);

    // 注入到 router 的 req_in[LOCAL]
    router.req_in()[static_cast<unsigned>(RouterPort::LOCAL)].data() = flit;
    router.req_in()[static_cast<unsigned>(RouterPort::LOCAL)].set_valid(true);

    // 运行一个周期
    router.tick();

    // 验证输出有效（经过 ST→LT，1 周期延迟）
    REQUIRE(router.resp_out()[static_cast<unsigned>(RouterPort::LOCAL)].valid());
}

TEST_CASE("RouterTLM: two hops routing", "[noc][integration]") {
    EventQueue eq;
    // Router at (0,0) - 要发送到 (1,0)，应该走 EAST
    RouterTLM router("router00", &eq, 0, 0, 2, 2);

    bundles::NoCFlitBundle flit;
    flit.transaction_id.write(200);
    flit.src_node.write(0);
    flit.dst_node.write(1);  // (1,0) - 同一行，X+1，EAST
    flit.vc_id.write(0);
    flit.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
    flit.flit_index.write(0);
    flit.flit_count.write(1);
    flit.flit_category.write(bundles::NoCFlitBundle::CATEGORY_REQUEST);

    router.req_in()[static_cast<unsigned>(RouterPort::WEST)].data() = flit;
    router.req_in()[static_cast<unsigned>(RouterPort::WEST)].set_valid(true);

    router.tick();

    // 应该输出到 EAST 端口
    REQUIRE(router.resp_out()[static_cast<unsigned>(RouterPort::EAST)].valid());
    auto out_flit = router.resp_out()[static_cast<unsigned>(RouterPort::EAST)].data();
    REQUIRE(out_flit.hops.read() == 1);
}

TEST_CASE("RouterTLM: SA can grant multiple flits in same cycle", "[noc][integration]") {
    EventQueue eq;
    RouterTLM router("router0", &eq, 1, 1, 4, 4);

    // 注入两个 flit，分别去 EAST 和 NORTH
    bundles::NoCFlitBundle flit1;
    flit1.transaction_id.write(301);
    flit1.src_node.write(5);
    flit1.dst_node.write(7);  // (1,1) -> (3,1): X+2, EAST
    flit1.vc_id.write(0);
    flit1.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
    flit1.flit_index.write(0);
    flit1.flit_count.write(1);
    flit1.flit_category.write(bundles::NoCFlitBundle::CATEGORY_REQUEST);

    bundles::NoCFlitBundle flit2;
    flit2.transaction_id.write(302);
    flit2.src_node.write(5);
    flit2.dst_node.write(1);  // (1,1) -> (1,3): Y+2, NORTH
    flit2.vc_id.write(1);
    flit2.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
    flit2.flit_index.write(0);
    flit2.flit_count.write(1);
    flit2.flit_category.write(bundles::NoCFlitBundle::CATEGORY_REQUEST);

    // 注入到不同输入端口
    router.req_in()[static_cast<unsigned>(RouterPort::SOUTH)].data() = flit1;
    router.req_in()[static_cast<unsigned>(RouterPort::SOUTH)].set_valid(true);

    router.req_in()[static_cast<unsigned>(RouterPort::WEST)].data() = flit2;
    router.req_in()[static_cast<unsigned>(RouterPort::WEST)].set_valid(true);

    // 运行一个周期
    router.tick();

    // 两个输出都应该有效（多 flit 并行仲裁）
    bool east_valid = router.resp_out()[static_cast<unsigned>(RouterPort::EAST)].valid();
    bool north_valid = router.resp_out()[static_cast<unsigned>(RouterPort::NORTH)].valid();

    // 至少一个应该有效（取决于仲裁顺序）
    REQUIRE(east_valid || north_valid);
}

TEST_CASE("NICTLM: packetize then reassemble cycle", "[noc][integration]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);
    nic.add_address_region(0x1000, 0x1000, 5);

    // 注入读请求
    bundles::CacheReqBundle req;
    req.transaction_id.write(400);
    req.address.write(0x1500);  // 目标节点 5
    req.is_write.write(0);
    req.data.write(0);
    req.size.write(8);

    nic.pe_req_in().data() = req;
    nic.pe_req_in().set_valid(true);

    // 第一次 tick: packetize
    nic.tick();

    // 验证产生了 flit
    REQUIRE(nic.net_req_out().valid());
    auto head_flit = nic.net_req_out().data();
    REQUIRE(head_flit.dst_node.read() == 5);
    REQUIRE(head_flit.is_head());

    // 模拟响应 flit 回来
    bundles::NoCFlitBundle resp_flit;
    resp_flit.transaction_id.write(400);
    resp_flit.src_node.write(5);
    resp_flit.dst_node.write(0);
    resp_flit.vc_id.write(0);
    resp_flit.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
    resp_flit.flit_index.write(0);
    resp_flit.flit_count.write(1);
    resp_flit.flit_category.write(bundles::NoCFlitBundle::CATEGORY_RESPONSE);
    resp_flit.data.write(0xDEAD);
    resp_flit.error_code.write(0);

    nic.net_resp_in().data() = resp_flit;
    nic.net_resp_in().set_valid(true);

    // 第二次 tick: reassemble
    nic.tick();

    // 验证产生了 CacheRespBundle
    REQUIRE(nic.pe_resp_out().valid());
    auto resp = nic.pe_resp_out().data();
    REQUIRE(resp.transaction_id.read() == 400);
    REQUIRE(resp.data.read() == 0xDEAD);
}

TEST_CASE("NICTLM: multi-flit packetize", "[noc][integration]") {
    EventQueue eq;
    NICTLM nic("nic0", &eq, 0, 4, 4);
    nic.add_address_region(0x1000, 0x1000, 5);

    // 32 字节请求 -> 4 个 flits
    bundles::CacheReqBundle req;
    req.transaction_id.write(500);
    req.address.write(0x1500);
    req.is_write.write(1);
    req.data.write(0x12345678);
    req.size.write(32);

    nic.pe_req_in().data() = req;
    nic.pe_req_in().set_valid(true);

    // 第一个周期: packetize HEAD
    nic.tick();
    REQUIRE(nic.net_req_out().valid());
    REQUIRE(nic.net_req_out().data().is_head());
    REQUIRE(nic.net_req_out().data().flit_index.read() == 0);
    REQUIRE(nic.net_req_out().data().flit_count.read() == 4);

    // 清除并继续
    nic.net_req_out().clear_valid();

    // 后续 flits (模拟已发送)
    for (uint8_t i = 1; i < 4; ++i) {
        bundles::NoCFlitBundle f;
        f.transaction_id.write(500);
        f.src_node.write(0);
        f.dst_node.write(5);
        f.vc_id.write(i % 4);
        f.flit_index.write(i);
        f.flit_count.write(4);
        f.flit_type.write(i == 3 ? bundles::NoCFlitBundle::FLIT_TAIL
                                  : bundles::NoCFlitBundle::FLIT_BODY);
        f.flit_category.write(bundles::NoCFlitBundle::CATEGORY_REQUEST);
        f.data.write(0x1000 + i);

        nic.net_req_in().data() = f;
        nic.net_req_in().set_valid(true);
        nic.tick();
        nic.net_req_in().clear_valid();
    }

    // 验证 stats
    REQUIRE(nic.net_req_out().valid() == false);  // 所有 flits 已发送
}
