// test/test_chstream_integration.cc
// P1.8 Phase 2: 集成测试 — ChStream 模块端到端验证

#include <catch2/catch_all.hpp>
#include "framework/stream_adapter.hh"
#include "core/packet.hh"
#include "core/packet_pool.hh"
#include "core/event_queue.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "bundles/bundle_serialization.hh"
#include "tlm/cache_tlm.hh"
#include "tlm/memory_tlm.hh"

#ifdef USE_SYSTEMC_STUB
#include "tlm/tlm_stub.hh"
#else
#include "tlm.h"
#endif

using namespace tlm;

// T5 依赖：测试 set_stream_adapter 是否真正存储（T1 已完成）
// 这些测试独立于 ModuleFactory，验证模块层面的 adapter 集成

class MockStreamAdapter : public cpptlm::StreamAdapterBase {
public:
    bool tick_called = false;
    bool process_req_called = false;
    Packet* last_processed_pkt = nullptr;

    void bind_ports(MasterPort*, SlavePort*, MasterPort*, SlavePort*) override {}
    void tick() override { tick_called = true; }
    void process_request_input(Packet* pkt) override {
        process_req_called = true;
        last_processed_pkt = pkt;
    }
    Packet* process_response_output() override { return nullptr; }
};

TEST_CASE("CacheTLM stores adapter pointer correctly", "[chstream][phase2]") {
    EventQueue eq;
    auto* cache = new CacheTLM("cache", &eq);
    REQUIRE(cache->get_adapter() == nullptr);

    MockStreamAdapter mock;
    cache->set_stream_adapter(&mock);
    REQUIRE(cache->get_adapter() == static_cast<cpptlm::StreamAdapterBase*>(&mock));

    delete cache;
}

TEST_CASE("CacheTLM tick() delegates to adapter tick()", "[chstream][phase2]") {
    EventQueue eq;
    auto* cache = new CacheTLM("cache", &eq);

    MockStreamAdapter mock;
    cache->set_stream_adapter(&mock);

    // Trigger module tick — adapter should be ticked at end
    cache->tick();

    REQUIRE(mock.tick_called == true);

    delete cache;
}

TEST_CASE("MemoryTLM stores adapter and delegates tick", "[chstream][phase2]") {
    EventQueue eq;
    auto* mem = new MemoryTLM("mem", &eq);
    REQUIRE(mem->get_adapter() == nullptr);

    MockStreamAdapter mock;
    mem->set_stream_adapter(&mock);
    REQUIRE(mem->get_adapter() == static_cast<cpptlm::StreamAdapterBase*>(&mock));

    // No req_in data, so tick should only call adapter
    REQUIRE(mock.tick_called == false);
    mem->tick();
    REQUIRE(mock.tick_called == true);

    // No input data means process_request_input should NOT be called by module
    // (it will be called by StreamTargetPort in Phase 2 runtime)
    REQUIRE(mock.process_req_called == false);

    delete mem;
}

TEST_CASE("Bundle serialization in StreamAdapter context", "[chstream][serialize]") {
    bundles::CacheReqBundle req;
    req.transaction_id.write(500);
    req.address.write(0xABC0);
    req.is_write.write(0);
    req.data.write(0);
    req.size.write(8);

    // Simulate what StreamAdapter does: packet ← bundle
    Packet* pkt = PacketPool::get().acquire();
    REQUIRE(pkt != nullptr);
    pkt->payload->set_data_length(sizeof(bundles::CacheReqBundle));

    bundles::serialize_bundle(req, pkt->payload->get_data_ptr(), pkt->payload->get_data_length());

    // Deserialize back
    bundles::CacheReqBundle recovered;
    bundles::deserialize_bundle(pkt->payload->get_data_ptr(), pkt->payload->get_data_length(), recovered);

    REQUIRE(recovered.transaction_id.read() == 500);
    REQUIRE(recovered.address.read() == 0xABC0);
    REQUIRE(recovered.is_write.read() == false);

    PacketPool::get().release(pkt);
}

TEST_CASE("OutputStreamAdapter write followed by serialize produces valid data", "[chstream][adapter]") {
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> out_adapter;
    REQUIRE_FALSE(out_adapter.valid());

    bundles::CacheRespBundle resp;
    resp.transaction_id.write(999);
    resp.data.write(0xDEADBEEF);
    resp.is_hit.write(1);
    resp.error_code.write(0);
    out_adapter.write(resp);

    REQUIRE(out_adapter.valid());

    // Serialize to packet
    Packet* pkt = PacketPool::get().acquire();
    pkt->payload->set_data_length(sizeof(bundles::CacheRespBundle));
    bool ok = bundles::serialize_bundle(out_adapter.data(), pkt->payload->get_data_ptr(), pkt->payload->get_data_length());
    REQUIRE(ok);

    // Deserialize and verify
    bundles::CacheRespBundle recovered;
    bundles::deserialize_bundle(pkt->payload->get_data_ptr(), pkt->payload->get_data_length(), recovered);

    REQUIRE(recovered.transaction_id.read() == 999);
    REQUIRE(recovered.data.read() == 0xDEADBEEF);
    REQUIRE(recovered.is_hit.read() == true);
    REQUIRE(recovered.error_code.read() == 0);

    PacketPool::get().release(pkt);
}
