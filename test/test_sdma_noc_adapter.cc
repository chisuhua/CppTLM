// test/test_sdma_noc_adapter.cc
// SDMANocAdapter 单元测试 (per E.1 v1.1 spec AC-E1-2.8 T1/T2)
//
// 测试标签: [realip][pin-contract][noc][adapter][sdma]
#include "catch_amalgamated.hpp"

#include "bundles/noc_bundles_tlm.hh"
#include "bundles/pcie_bundles_tlm.hh"
#include "tlm/cluster/soc_internal_noc.hh"
#include "tlm/gpu/sdma_noc_adapter.hh"

#include <cstdint>

using bundles::NoCFlitBundle;
using tlm::cluster::SocInternalNoc;
using tlm::gpu::SDMANocAdapter;

TEST_CASE("SDMANocAdapter: emit_noc_request generates valid REQ flit",
          "[realip][pin-contract][noc][adapter][sdma]") {
    SDMANocAdapter adapter;

    // 发出 SDMA bulk DMA read request (op=0, addr=0x1000, size=64, tag=42)
    NoCFlitBundle flit = adapter.emit_noc_request(0, 0x1000, 64, 42);

    // 验证字段 (per design §3.2 字段映射表)
    CHECK(flit.dst_node.read() == SocInternalNoc::NODE_ID_VRAM);
    CHECK(flit.src_node.read() == SocInternalNoc::NODE_ID_SDMA);
    CHECK(flit.flit_category.read() == NoCFlitBundle::CATEGORY_REQUEST);
    CHECK(flit.transaction_id.read() == 42);
    CHECK(flit.is_write.read() == false); // op=0 → READ
    CHECK(flit.address.read() == 0x1000);
    CHECK(flit.size.read() == 64);
    CHECK(flit.vc_id.read() == SocInternalNoc::VC_REQ);

    CHECK(flit.flit_type.read() == NoCFlitBundle::FLIT_HEAD_TAIL);
    CHECK(flit.flit_count.read() == 1);
}

TEST_CASE("SDMANocAdapter: emit_noc_request WRITE flag correct",
          "[realip][pin-contract][noc][adapter][sdma]") {
    SDMANocAdapter adapter;

    // op=1 → WRITE
    NoCFlitBundle flit_w = adapter.emit_noc_request(1, 0x2000, 128, 100);
    CHECK(flit_w.is_write.read() == true);
    CHECK(flit_w.transaction_id.read() == 100);

    // op=0 → READ
    NoCFlitBundle flit_r = adapter.emit_noc_request(0, 0x2000, 128, 101);
    CHECK(flit_r.is_write.read() == false);
    CHECK(flit_r.transaction_id.read() == 101);
}

TEST_CASE("SDMANocAdapter: accept_noc_response records transaction_id + status",
          "[realip][pin-contract][noc][adapter][sdma]") {
    SDMANocAdapter adapter;

    // 构造 NoC RESP flit (VRAM → SDMA, transaction_id=42, success)
    NoCFlitBundle resp{};
    resp.flit_category.write(NoCFlitBundle::CATEGORY_RESPONSE);
    resp.transaction_id.write(42);
    resp.is_ok.write(true);
    resp.error_code.write(0);
    resp.data.write(0xDEADBEEFCAFEBABEULL);

    // 接受响应
    adapter.accept_noc_response(resp);

    // 验证记录
    CHECK(adapter.received_responses() == 1);
    CHECK(adapter.unexpected_responses() == 0);
    CHECK(adapter.last_response_tag() == 42);
    CHECK(adapter.last_response_ok() == true);
    CHECK(adapter.last_response_error_code() == 0);
    CHECK(adapter.last_response().data.read() == 0xDEADBEEFCAFEBABEULL);
}

TEST_CASE("SDMANocAdapter: accept_noc_response counts unexpected non-RESP",
          "[realip][pin-contract][noc][adapter][sdma]") {
    SDMANocAdapter adapter;

    // 构造 NoC REQ flit (非 RESP, 不应计入 received_responses)
    NoCFlitBundle req{};
    req.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    req.transaction_id.write(99);

    adapter.accept_noc_response(req);

    CHECK(adapter.received_responses() == 0);
    CHECK(adapter.unexpected_responses() == 1);
}

TEST_CASE("SDMANocAdapter: make_accept_fn connects to SocInternalNoc",
          "[realip][pin-contract][noc][adapter][sdma][integration]") {
    SDMANocAdapter adapter;
    SocInternalNoc soc_noc("soc_noc", nullptr);
    soc_noc.set_sdma_accept_fn(adapter.make_accept_fn());

    // 构造 RESP flit 经 soc_noc.route_flit → 应触发 accept_noc_response
    NoCFlitBundle resp{};
    resp.dst_node.write(SocInternalNoc::NODE_ID_SDMA);
    resp.src_node.write(SocInternalNoc::NODE_ID_VRAM);
    resp.flit_category.write(NoCFlitBundle::CATEGORY_RESPONSE);
    resp.transaction_id.write(7);
    resp.is_ok.write(true);

    soc_noc.route_flit(resp);

    CHECK(adapter.received_responses() == 1);
    CHECK(adapter.last_response_tag() == 7);
    CHECK(adapter.last_response_ok() == true);
    CHECK(soc_noc.flits_dispatched() == 1);
    CHECK(soc_noc.vc_rsp_count() == 1);
    CHECK(soc_noc.vc_req_count() == 0);
}

TEST_CASE("SDMANocAdapter: route via SocInternalNoc updates VC counters correctly",
          "[realip][pin-contract][noc][adapter][sdma][integration]") {
    SDMANocAdapter sdma_adapter;
    SocInternalNoc soc_noc("soc_noc", nullptr);
    soc_noc.set_sdma_accept_fn(sdma_adapter.make_accept_fn());

    // REQ flit (SDMA → VRAM)
    NoCFlitBundle req = sdma_adapter.emit_noc_request(0, 0x100, 32, 1);
    soc_noc.route_flit(req);
    CHECK(soc_noc.flits_dispatched() == 1);
    CHECK(soc_noc.vc_req_count() == 1);
    CHECK(soc_noc.vc_rsp_count() == 0);

    // RESP flit (VRAM → SDMA)
    NoCFlitBundle resp{};
    resp.dst_node.write(SocInternalNoc::NODE_ID_SDMA);
    resp.src_node.write(SocInternalNoc::NODE_ID_VRAM);
    resp.flit_category.write(NoCFlitBundle::CATEGORY_RESPONSE);
    resp.transaction_id.write(1);
    resp.is_ok.write(true);
    soc_noc.route_flit(resp);
    CHECK(soc_noc.flits_dispatched() == 2);
    CHECK(soc_noc.vc_req_count() == 1);
    CHECK(soc_noc.vc_rsp_count() == 1);
    CHECK(sdma_adapter.last_response_tag() == 1);
}
