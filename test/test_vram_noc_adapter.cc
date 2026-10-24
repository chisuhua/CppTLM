// test/test_vram_noc_adapter.cc
// VramNocAdapter 单元测试 (per E.1 v1.1 spec AC-E1-2.7 T1/T2/T3)
//
// 测试标签: [realip][pin-contract][noc][adapter][vram]
#include "catch_amalgamated.hpp"

#include "bundles/dgpu_fabric_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh"
#include "tlm/cluster/soc_internal_noc.hh"
#include "tlm/vram_noc_adapter.hh"

#include <cstdint>

using bundles::Fabric2VramCtrlBundle;
using bundles::NoCFlitBundle;
using tlm::cluster::SocInternalNoc;
using tlm::cluster::VramNocAdapter;

TEST_CASE("VramNocAdapter: emit_noc_request single flit field mapping",
          "[realip][pin-contract][noc][adapter][vram]") {
    VramNocAdapter adapter;

    // 构造 NoC REQ flit (SDMA → VRAM, op=WRITE, addr=0x1000, size=64, tag=42)
    NoCFlitBundle req{};
    req.src_node.write(SocInternalNoc::NODE_ID_SDMA);
    req.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    req.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    req.transaction_id.write(42);
    req.is_write.write(true);
    req.address.write(0x1000);
    req.size.write(64);
    req.vc_id.write(SocInternalNoc::VC_REQ);
    req.data.write(0xDEADBEEF);

    // 转换为 Fabric2VramCtrlBundle
    Fabric2VramCtrlBundle fabric = adapter.emit_noc_request(req);

    // 验证字段映射 (per design §3.3 表)
    CHECK(fabric.address.read() == 0x1000);
    CHECK(fabric.size.read() == 64);
    CHECK(fabric.is_write.read() == true);
    CHECK(fabric.data.read() == 0xDEADBEEFULL);
    CHECK(fabric.transaction_id.read() == 42); // 64→32 截断后
    CHECK(fabric.error_code.read() == 0);      // REQ 阶段未设
    // chip_select: (vc_id=0 << 4) | (src_node=1 SDMA) = 0x01
    CHECK(fabric.chip_select.read() == 0x01);

    CHECK(adapter.requests_emitted() == 1);
}

TEST_CASE("VramNocAdapter: wrap_response field mapping",
          "[realip][pin-contract][noc][adapter][vram]") {
    VramNocAdapter adapter;

    // 构造 Fabric2VramCtrlBundle response (VRAM → SDMA, success)
    Fabric2VramCtrlBundle resp{};
    resp.transaction_id.write(42);
    resp.resp_data.write(0xCAFEBABE);
    resp.error_code.write(0); // OK

    // 包装为 NoC RESP flit
    NoCFlitBundle flit = adapter.wrap_response(resp);

    // 验证响应字段
    CHECK(flit.dst_node.read() == SocInternalNoc::NODE_ID_SDMA);
    CHECK(flit.src_node.read() == SocInternalNoc::NODE_ID_VRAM);
    CHECK(flit.flit_category.read() == NoCFlitBundle::CATEGORY_RESPONSE);
    CHECK(flit.data.read() == 0xCAFEBABEULL);
    CHECK(flit.transaction_id.read() == 42);
    CHECK(flit.is_ok.read() == true);
    CHECK(flit.error_code.read() == 0);
    CHECK(flit.vc_id.read() == SocInternalNoc::VC_RSP);

    CHECK(adapter.responses_wrapped() == 1);
}

TEST_CASE("VramNocAdapter: multi-beat split flit_count > 1",
          "[realip][pin-contract][noc][adapter][vram][multibeat]") {
    VramNocAdapter adapter;

    // 构造 multi-beat NoC REQ flit (flit_count=4, AXI4 burst split 简化)
    NoCFlitBundle req{};
    req.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    req.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    req.transaction_id.write(100);
    req.is_write.write(true);
    req.address.write(0x10000);
    req.size.write(128);
    req.flit_count.write(4); // 4-beat burst
    req.flit_index.write(0);

    // multi-beat split
    auto beats = adapter.emit_noc_multibeat(req);

    // 验证: 4 beat, 每 beat 32B 步进地址
    REQUIRE(beats.size() == 4);
    CHECK(beats[0].address.read() == 0x10000); // beat 0
    CHECK(beats[1].address.read() == 0x10020); // beat 1
    CHECK(beats[2].address.read() == 0x10040); // beat 2
    CHECK(beats[3].address.read() == 0x10060); // beat 3
    for (const auto& beat : beats) {
        CHECK(beat.size.read() == 32);
        CHECK(beat.is_write.read() == true);
        CHECK(beat.transaction_id.read() == 100);
    }
    CHECK(adapter.requests_emitted() == 4);
}

TEST_CASE("VramNocAdapter: multi-beat with flit_count=1 returns single beat",
          "[realip][pin-contract][noc][adapter][vram][multibeat]") {
    VramNocAdapter adapter;

    NoCFlitBundle req{};
    req.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    req.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    req.address.write(0x2000);
    req.size.write(64);
    req.flit_count.write(1);

    auto beats = adapter.emit_noc_multibeat(req);
    CHECK(beats.size() == 1);
    CHECK(beats[0].address.read() == 0x2000);
}

TEST_CASE("VramNocAdapter: chip_select encoding captures vc + src",
          "[realip][pin-contract][noc][adapter][vram]") {
    VramNocAdapter adapter;

    // SDMA (src=1) + VC_REQ (0) → chip_select = (0 << 4) | 1 = 0x01
    {
        NoCFlitBundle req{};
        req.src_node.write(SocInternalNoc::NODE_ID_SDMA);
        req.vc_id.write(SocInternalNoc::VC_REQ);
        req.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
        req.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
        auto f = adapter.emit_noc_request(req);
        CHECK(f.chip_select.read() == 0x01);
    }

    // SDMA (src=1) + VC_DAT (2) → chip_select = (2 << 4) | 1 = 0x21
    {
        adapter.reset();
        NoCFlitBundle req{};
        req.src_node.write(SocInternalNoc::NODE_ID_SDMA);
        req.vc_id.write(SocInternalNoc::VC_DAT);
        req.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
        req.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
        auto f = adapter.emit_noc_request(req);
        CHECK(f.chip_select.read() == 0x21);
    }
}
