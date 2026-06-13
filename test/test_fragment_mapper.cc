// test/test_fragment_mapper.cc
// FragmentMapper 单元测试：以 TransactionContextExt 为真值源
// 覆盖：单拍/多拍序列化、Extension 缺失回退、X.13 安全写入、辅助函数
#include "core/packet_pool.hh"
#include "rtl/fragment_mapper.hh"
#include "tlm/tlm_stub.hh"
#include <catch2/catch_all.hpp>

using namespace cpptlm;
using namespace cpptlm::rtl;

TEST_CASE("FragmentMapper: serialize single-beat (root) transaction", "[rtl][fragment]") {
    Packet* pkt = PacketPool::get().acquire();
    pkt->type = PKT_REQ;
    pkt->stream_id = 100;

    // 关键：acquire() 触发 reset() 会 delete[] data; data=nullptr; len=0
    // 必须先 set_data_length() 分配数据 buffer
    pkt->payload->set_data_length(sizeof(uint64_t));
    pkt->payload->set_address(0x1000);
    uint64_t data = 0xDEADBEEFCAFEBABEULL;
    std::memcpy(pkt->payload->get_data_ptr(), &data, sizeof(uint64_t));

    // 根事务：parent_id=0, fragment_id=0, fragment_total=1
    create_transaction_context(pkt->payload, 100, 0, 0, 1);

    auto beat = FragmentMapper::serialize_req(pkt);

    REQUIRE(beat.tid == 100);
    REQUIRE(beat.parent_id == 0);
    REQUIRE(beat.fragment_id == 0);
    REQUIRE(beat.fragment_total == 1);
    REQUIRE(beat.first == true);
    REQUIRE(beat.last == true);
    REQUIRE(beat.addr == 0x1000);
    REQUIRE(beat.data == 0xDEADBEEFCAFEBABEULL);
    REQUIRE(beat.strb == 0xFF);

    PacketPool::get().release(pkt);
}

TEST_CASE("FragmentMapper: serialize multi-beat transaction (middle beat)", "[rtl][fragment]") {
    Packet* pkt = PacketPool::get().acquire();
    pkt->type = PKT_REQ;
    pkt->stream_id = 201;

    pkt->payload->set_data_length(sizeof(uint64_t));
    pkt->payload->set_address(0x2000);
    uint64_t data = 0x1122334455667788ULL;
    std::memcpy(pkt->payload->get_data_ptr(), &data, sizeof(uint64_t));

    // 分片事务：parent=200, 当前是 fragment 1/4
    create_transaction_context(pkt->payload, 201, 200, 1, 4);

    auto beat = FragmentMapper::serialize_req(pkt);

    REQUIRE(beat.tid == 201);       // 每拍独立 tid
    REQUIRE(beat.parent_id == 200); // 共享 parent
    REQUIRE(beat.fragment_id == 1);
    REQUIRE(beat.fragment_total == 4);
    REQUIRE(beat.first == false); // 不是首拍
    REQUIRE(beat.last == false);  // 不是末拍
    REQUIRE(beat.addr == 0x2000);
    REQUIRE(beat.data == 0x1122334455667788ULL);

    PacketPool::get().release(pkt);
}

TEST_CASE("FragmentMapper: serialize without Extension (fallback to stream_id)",
          "[rtl][fragment]") {
    Packet* pkt = PacketPool::get().acquire();
    pkt->type = PKT_REQ;
    pkt->stream_id = 42;
    pkt->payload->set_data_length(sizeof(uint64_t));
    pkt->payload->set_address(0x3000);
    // 注意：未创建 Extension

    auto beat = FragmentMapper::serialize_req(pkt);

    REQUIRE(beat.tid == 42); // 回退到 stream_id
    REQUIRE(beat.parent_id == 0);
    REQUIRE(beat.fragment_id == 0);
    REQUIRE(beat.fragment_total == 1);
    REQUIRE(beat.first == true);
    REQUIRE(beat.last == true);
    REQUIRE(beat.addr == 0x3000);

    PacketPool::get().release(pkt);
}

TEST_CASE("FragmentMapper: serialize first/last fragments correctly", "[rtl][fragment]") {
    SECTION("First fragment") {
        Packet* pkt = PacketPool::get().acquire();
        pkt->stream_id = 300;
        create_transaction_context(pkt->payload, 300, 250, 0, 3);
        auto beat = FragmentMapper::serialize_req(pkt);
        REQUIRE(beat.first == true);
        REQUIRE(beat.last == false);
        PacketPool::get().release(pkt);
    }

    SECTION("Last fragment") {
        Packet* pkt = PacketPool::get().acquire();
        pkt->stream_id = 302;
        create_transaction_context(pkt->payload, 302, 250, 2, 3);
        auto beat = FragmentMapper::serialize_req(pkt);
        REQUIRE(beat.first == false);
        REQUIRE(beat.last == true);
        PacketPool::get().release(pkt);
    }
}

TEST_CASE("FragmentMapper: serialize_beat_at overrides fragment_id", "[rtl][fragment]") {
    Packet* pkt = PacketPool::get().acquire();
    pkt->stream_id = 400;
    pkt->payload->set_data_length(sizeof(uint64_t));
    pkt->payload->set_address(0x4000);
    create_transaction_context(pkt->payload, 400, 350, 2, 4); // 起始 fragment_id=2

    SECTION("Beat 0: override to first") {
        auto beat = FragmentMapper::serialize_beat_at(pkt, 0);
        REQUIRE(beat.fragment_id == 0);
        REQUIRE(beat.first == true);
        REQUIRE(beat.last == false);
        // tid/parent_id/total 保留自 Extension
        REQUIRE(beat.tid == 400);
        REQUIRE(beat.parent_id == 350);
        REQUIRE(beat.fragment_total == 4);
    }

    SECTION("Beat 3: override to last") {
        auto beat = FragmentMapper::serialize_beat_at(pkt, 3);
        REQUIRE(beat.fragment_id == 3);
        REQUIRE(beat.first == false);
        REQUIRE(beat.last == true);
    }

    PacketPool::get().release(pkt);
}

TEST_CASE("FragmentMapper: write_resp creates Extension with X.13 safety", "[rtl][fragment]") {
    Packet* resp = PacketPool::get().acquire();
    resp->type = PKT_RESP;
    resp->payload->set_data_length(sizeof(uint64_t)); // 分配 data buffer

    CacheRespBeatRTL beat;
    beat.tid = 500;
    beat.parent_id = 450;
    beat.fragment_id = 1;
    beat.fragment_total = 2;
    beat.data = 0xAABBCCDDEEFF0011ULL;
    beat.hit = true;
    beat.error_code = 0;
    beat.first = false;
    beat.last = false;

    FragmentMapper::write_resp(resp, beat);

    // Extension 已创建
    auto* ext = get_transaction_context(resp->payload);
    REQUIRE(ext != nullptr);
    REQUIRE(ext->transaction_id == 500);
    REQUIRE(ext->parent_id == 450);
    REQUIRE(ext->fragment_id == 1);
    REQUIRE(ext->fragment_total == 2);

    // Packet::stream_id 同步
    REQUIRE(resp->get_transaction_id() == 500);
    REQUIRE(resp->stream_id == 500);

    // payload 数据已写入
    uint64_t roundtrip = 0;
    std::memcpy(&roundtrip, resp->payload->get_data_ptr(), sizeof(uint64_t));
    REQUIRE(roundtrip == 0xAABBCCDDEEFF0011ULL);

    PacketPool::get().release(resp);
}

TEST_CASE("FragmentMapper: write_resp replaces old Extension (X.13 safe)", "[rtl][fragment]") {
    Packet* resp = PacketPool::get().acquire();
    resp->type = PKT_RESP;
    resp->payload->set_data_length(sizeof(uint64_t)); // 分配 data buffer

    // 预存旧 Extension
    create_transaction_context(resp->payload, 999, 0, 0, 1);

    // 新 beat 覆盖
    CacheRespBeatRTL new_beat;
    new_beat.tid = 600;
    new_beat.parent_id = 550;
    new_beat.fragment_id = 0;
    new_beat.fragment_total = 1;
    new_beat.data = 0x12345678ULL;
    new_beat.first = true;
    new_beat.last = true;

    FragmentMapper::write_resp(resp, new_beat);

    // 验证替换成功（不是累积）
    auto* ext = get_transaction_context(resp->payload);
    REQUIRE(ext != nullptr);
    REQUIRE(ext->transaction_id == 600); // 新值
    REQUIRE(ext->parent_id == 550);
    REQUIRE(resp->get_transaction_id() == 600);

    PacketPool::get().release(resp);
}

TEST_CASE("FragmentMapper: beats_remaining computation", "[rtl][fragment]") {
    CacheReqBeatRTL beat;

    beat.fragment_id = 0;
    beat.fragment_total = 4;
    REQUIRE(FragmentMapper::beats_remaining(beat) == 3);

    beat.fragment_id = 1;
    beat.fragment_total = 4;
    REQUIRE(FragmentMapper::beats_remaining(beat) == 2);

    beat.fragment_id = 2;
    beat.fragment_total = 4;
    REQUIRE(FragmentMapper::beats_remaining(beat) == 1);

    beat.fragment_id = 3;
    beat.fragment_total = 4; // last
    REQUIRE(FragmentMapper::beats_remaining(beat) == 0);

    // 边界：fragment_total=0
    beat.fragment_total = 0;
    REQUIRE(FragmentMapper::beats_remaining(beat) == 0);
}

TEST_CASE("FragmentMapper: group_key uses parent_id when set", "[rtl][fragment]") {
    CacheReqBeatRTL beat;

    beat.tid = 201;
    beat.parent_id = 200;
    REQUIRE(FragmentMapper::group_key(beat) == 200); // 用 parent_id

    beat.parent_id = 0;                              // 根事务
    REQUIRE(FragmentMapper::group_key(beat) == 201); // 用 tid
}

TEST_CASE("FragmentMapper: is_single_beat detection", "[rtl][fragment]") {
    CacheReqBeatRTL beat;

    beat.fragment_total = 1;
    REQUIRE(FragmentMapper::is_single_beat(beat) == true);

    beat.fragment_total = 4;
    REQUIRE(FragmentMapper::is_single_beat(beat) == false);

    beat.fragment_total = 0; // 边界
    REQUIRE(FragmentMapper::is_single_beat(beat) == true);
}

TEST_CASE("FragmentMapper: serialize_req on null packet is safe", "[rtl][fragment]") {
    // 不应崩溃
    auto beat_null = FragmentMapper::serialize_req(nullptr);
    REQUIRE(beat_null.tid == 0);
    REQUIRE(beat_null.first == true);
    REQUIRE(beat_null.last == true);
}

TEST_CASE("FragmentMapper: write_resp on null packet is safe", "[rtl][fragment]") {
    CacheRespBeatRTL beat;
    beat.tid = 100;
    // 不应崩溃
    FragmentMapper::write_resp(nullptr, beat);
    SUCCEED("null packet safely ignored");
}

TEST_CASE("FragmentMapper: serialize_req on packet with null payload is safe", "[rtl][fragment]") {
    Packet* pkt = PacketPool::get().acquire();
    pkt->type = PKT_REQ;
    pkt->stream_id = 555;
    auto* saved_payload = pkt->payload;
    pkt->payload = nullptr;
    auto beat = FragmentMapper::serialize_req(pkt);
    REQUIRE(beat.tid == 0);
    REQUIRE(beat.first == true);
    REQUIRE(beat.last == true);
    pkt->payload = saved_payload;
    PacketPool::get().release(pkt);
}

// =============================================================================
// Round-trip 测试(Oracle 评审 v4 缺失项)
// =============================================================================

TEST_CASE("FragmentMapper: round-trip single-beat preserves all fields",
          "[rtl][fragment][roundtrip]") {
    // Step 1: 创建请求 Packet
    Packet* req = PacketPool::get().acquire();
    req->type = PKT_REQ;
    req->stream_id = 700;
    req->payload->set_data_length(sizeof(uint64_t));
    req->payload->set_address(0xABCD);
    uint64_t original_data = 0x1122334455667788ULL;
    std::memcpy(req->payload->get_data_ptr(), &original_data, sizeof(uint64_t));
    create_transaction_context(req->payload, 700, 0, 0, 1);

    // Step 2: serialize → beat
    auto req_beat = FragmentMapper::serialize_req(req);
    REQUIRE(req_beat.tid == 700);
    REQUIRE(req_beat.addr == 0xABCD);
    REQUIRE(req_beat.data == 0x1122334455667788ULL);
    REQUIRE(req_beat.first == true);
    REQUIRE(req_beat.last == true);

    // Step 3: write_resp → response Packet(模拟 RTL 处理后回写)
    Packet* resp = PacketPool::get().acquire();
    resp->type = PKT_RESP;
    resp->payload->set_data_length(sizeof(uint64_t));
    CacheRespBeatRTL resp_beat;
    resp_beat.tid = 700; // 同一 tid
    resp_beat.parent_id = 0;
    resp_beat.fragment_id = 0;
    resp_beat.fragment_total = 1;
    resp_beat.data = 0xDEADBEEF00000000ULL;
    resp_beat.hit = true;
    resp_beat.error_code = 0;
    resp_beat.first = true;
    resp_beat.last = true;
    FragmentMapper::write_resp(resp, resp_beat);

    // Step 4: 验证响应 Packet 字段
    REQUIRE(resp->get_transaction_id() == 700); // 同步 stream_id
    REQUIRE(resp->stream_id == 700);
    auto* ext = get_transaction_context(resp->payload);
    REQUIRE(ext != nullptr);
    REQUIRE(ext->transaction_id == 700);
    REQUIRE(ext->is_first_fragment());
    REQUIRE(ext->is_last_fragment());
    uint64_t resp_data = 0;
    std::memcpy(&resp_data, resp->payload->get_data_ptr(), sizeof(uint64_t));
    REQUIRE(resp_data == 0xDEADBEEF00000000ULL);

    PacketPool::get().release(req);
    PacketPool::get().release(resp);
}

TEST_CASE("FragmentMapper: round-trip multi-beat preserves fragment metadata",
          "[rtl][fragment][roundtrip]") {
    // 多拍事务:tid 100, parent 50, 4 beats
    Packet* req = PacketPool::get().acquire();
    req->type = PKT_REQ;
    req->stream_id = 100;
    req->payload->set_data_length(sizeof(uint64_t));
    req->payload->set_address(0x100);
    uint64_t d = 0xCAFE;
    std::memcpy(req->payload->get_data_ptr(), &d, sizeof(uint64_t));
    create_transaction_context(req->payload, 100, 50, 0, 4);

    // 序列化 4 个 beat
    std::vector<CacheReqBeatRTL> beats;
    for (uint8_t i = 0; i < 4; i++) {
        beats.push_back(FragmentMapper::serialize_beat_at(req, i));
    }

    // 验证每拍元数据一致
    for (uint8_t i = 0; i < 4; i++) {
        REQUIRE(beats[i].tid == 100);
        REQUIRE(beats[i].parent_id == 50);
        REQUIRE(beats[i].fragment_total == 4);
        REQUIRE(beats[i].fragment_id == i);
        REQUIRE(beats[i].first == (i == 0));
        REQUIRE(beats[i].last == (i == 3));
    }

    // group_key 在所有拍上相同
    REQUIRE(FragmentMapper::group_key(beats[0]) == 50);
    REQUIRE(FragmentMapper::group_key(beats[3]) == 50);

    std::vector<Packet*> resp_pkts(4, nullptr);
    for (uint8_t i = 0; i < 4; i++) {
        resp_pkts[i] = PacketPool::get().acquire();
        resp_pkts[i]->type = PKT_RESP;
        resp_pkts[i]->payload->set_data_length(sizeof(uint64_t));

        CacheRespBeatRTL resp_beat;
        resp_beat.tid = beats[i].tid;
        resp_beat.parent_id = beats[i].parent_id;
        resp_beat.fragment_id = beats[i].fragment_id;
        resp_beat.fragment_total = beats[i].fragment_total;
        resp_beat.data = 0xBEEF0000ULL + i;
        resp_beat.hit = true;
        resp_beat.error_code = 0;
        resp_beat.first = beats[i].first;
        resp_beat.last = beats[i].last;

        FragmentMapper::write_resp(resp_pkts[i], resp_beat);

        auto* ext = get_transaction_context(resp_pkts[i]->payload);
        REQUIRE(ext != nullptr);
        REQUIRE(ext->transaction_id == 100);
        REQUIRE(ext->parent_id == 50);
        REQUIRE(ext->fragment_id == i);
        REQUIRE(ext->fragment_total == 4);
        REQUIRE(ext->is_first_fragment() == (i == 0));
        REQUIRE(ext->is_last_fragment() == (i == 3));
        REQUIRE(resp_pkts[i]->get_transaction_id() == 100);
        REQUIRE(ext->get_group_key() == 50);
    }

    for (auto* p : resp_pkts)
        PacketPool::get().release(p);
    PacketPool::get().release(req);
}

// =============================================================================
// 边界条件测试(Oracle 评审 v4 缺失项)
// =============================================================================

TEST_CASE("FragmentMapper: serialize_req with partial data length (< 8 bytes)",
          "[rtl][fragment][edge]") {
    Packet* pkt = PacketPool::get().acquire();
    pkt->type = PKT_REQ;
    pkt->stream_id = 800;
    pkt->payload->set_data_length(3); // 只 3 字节
    pkt->payload->set_address(0x2000);
    uint8_t partial[3] = {0xAA, 0xBB, 0xCC};
    std::memcpy(pkt->payload->get_data_ptr(), partial, 3);
    create_transaction_context(pkt->payload, 800, 0, 0, 1);

    auto beat = FragmentMapper::serialize_req(pkt);

    REQUIRE(beat.tid == 800);
    REQUIRE(beat.addr == 0x2000);
    // 部分数据应被截断到 fit(只读 3 字节,其余保留为 0)
    REQUIRE((beat.data & 0xFFFFFF) == 0x00CCBBAAULL); // little-endian
    REQUIRE(beat.strb == 0xFF);

    PacketPool::get().release(pkt);
}

TEST_CASE("FragmentMapper: serialize_beat_at marks last when beat_index >= fragment_total-1",
          "[rtl][fragment][edge]") {
    Packet* pkt = PacketPool::get().acquire();
    pkt->type = PKT_REQ;
    pkt->stream_id = 900;
    pkt->payload->set_data_length(sizeof(uint64_t));
    pkt->payload->set_address(0x3000);
    create_transaction_context(pkt->payload, 900, 800, 0, 4);

    auto beat = FragmentMapper::serialize_beat_at(pkt, 10);
    REQUIRE(beat.fragment_id == 10);
    REQUIRE(beat.fragment_total == 4);
    REQUIRE(beat.first == false);
    REQUIRE(beat.last == true);

    PacketPool::get().release(pkt);
}

TEST_CASE("FragmentMapper: write_resp with empty data length (no data buffer)",
          "[rtl][fragment][edge]") {
    Packet* resp = PacketPool::get().acquire();
    resp->type = PKT_RESP;
    // 不调用 set_data_length,保持 len=0
    CacheRespBeatRTL beat;
    beat.tid = 1000;
    beat.parent_id = 950;
    beat.fragment_id = 0;
    beat.fragment_total = 1;
    beat.data = 0;
    beat.hit = false;
    beat.error_code = 1;
    beat.first = true;
    beat.last = true;

    // 不应崩溃(FragmentMapper::write_resp 检查 length)
    FragmentMapper::write_resp(resp, beat);

    // Extension 仍应正确创建
    auto* ext = get_transaction_context(resp->payload);
    REQUIRE(ext != nullptr);
    REQUIRE(ext->transaction_id == 1000);
    REQUIRE(ext->parent_id == 950);
    REQUIRE(resp->get_transaction_id() == 1000);

    PacketPool::get().release(resp);
}
