// test/test_soc_internal_noc_send_req_resp.cc
// SocInternalNoc::send_request/response API 单元测试 (per E.1 v1.1 spec AC-E1-3.4)
//
// 测试标签: [realip][pin-contract][noc][send-api]
//
// 验证:
//   - send_request: 验证 flit_category 计数 + flits_dispatched_ (替代 route_flit 单独调用)
//   - send_response: 验证 flit_category 计数 + flits_dispatched_
//   - 与 route_flit 行为一致 (per e1 v1.1 design §3.4)
#include "catch_amalgamated.hpp"

#include "bundles/noc_bundles_tlm.hh"
#include "tlm/cluster/soc_internal_noc.hh"

#include <cstdint>

using tlm::cluster::SocInternalNoc;
using bundles::NoCFlitBundle;

TEST_CASE("SocInternalNoc::send_request: 计数 + 路由 (per AC-E1-3.4)",
          "[realip][pin-contract][noc][send-api][request]") {
    SocInternalNoc soc_noc("soc_noc", nullptr);
    soc_noc.set_vram_accept_fn([](const NoCFlitBundle&) {});  // 注册 callback

    NoCFlitBundle req{};
    req.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    req.src_node.write(SocInternalNoc::NODE_ID_SDMA);
    req.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    req.transaction_id.write(1);

    soc_noc.send_request(req);

    CHECK(soc_noc.flits_dispatched() == 1);
    CHECK(soc_noc.vc_req_count() == 1);
    CHECK(soc_noc.vc_rsp_count() == 0);
}

TEST_CASE("SocInternalNoc::send_response: 计数 + 路由 (per AC-E1-3.4)",
          "[realip][pin-contract][noc][send-api][response]") {
    SocInternalNoc soc_noc("soc_noc", nullptr);
    soc_noc.set_sdma_accept_fn([](const NoCFlitBundle&) {});

    NoCFlitBundle resp{};
    resp.dst_node.write(SocInternalNoc::NODE_ID_SDMA);
    resp.src_node.write(SocInternalNoc::NODE_ID_VRAM);
    resp.flit_category.write(NoCFlitBundle::CATEGORY_RESPONSE);
    resp.transaction_id.write(1);

    soc_noc.send_response(resp);

    CHECK(soc_noc.flits_dispatched() == 1);
    CHECK(soc_noc.vc_rsp_count() == 1);
    CHECK(soc_noc.vc_req_count() == 0);
}

TEST_CASE("SocInternalNoc::send_request + send_response: REQ/RSP 配对 (per AC-E1-3.4)",
          "[realip][pin-contract][noc][send-api][req-rsp-pair]") {
    SocInternalNoc soc_noc("soc_noc", nullptr);
    soc_noc.set_vram_accept_fn([](const NoCFlitBundle&) {});
    soc_noc.set_sdma_accept_fn([](const NoCFlitBundle&) {});

    NoCFlitBundle req{};
    req.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    req.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    req.transaction_id.write(42);

    soc_noc.send_request(req);
    CHECK(soc_noc.flits_dispatched() == 1);
    CHECK(soc_noc.vc_req_count() == 1);

    NoCFlitBundle resp{};
    resp.dst_node.write(SocInternalNoc::NODE_ID_SDMA);
    resp.flit_category.write(NoCFlitBundle::CATEGORY_RESPONSE);
    resp.transaction_id.write(42);

    soc_noc.send_response(resp);
    CHECK(soc_noc.flits_dispatched() == 2);
    CHECK(soc_noc.vc_req_count() == 1);
    CHECK(soc_noc.vc_rsp_count() == 1);
}

TEST_CASE("SocInternalNoc: send_request 与 route_flit 行为一致",
          "[realip][pin-contract][noc][send-api][route-flit-equivalent]") {
    SocInternalNoc soc1("soc1", nullptr);
    SocInternalNoc soc2("soc2", nullptr);
    soc1.set_vram_accept_fn([](const NoCFlitBundle&) {});
    soc2.set_vram_accept_fn([](const NoCFlitBundle&) {});

    NoCFlitBundle req{};
    req.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    req.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);

    // route_flit 直接调用
    soc1.route_flit(req);
    // send_request 间接调用
    soc2.send_request(req);

    CHECK(soc1.flits_dispatched() == soc2.flits_dispatched());
    CHECK(soc1.vc_req_count() == soc2.vc_req_count());
}
