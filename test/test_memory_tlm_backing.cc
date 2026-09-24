// test/test_memory_tlm_backing.cc
// MemoryTLM backing-store 单元测试 (Phase A1 of openspec/changes/cpptlm-minimal-dgpu-soc-v1)
// 验证 spec/minimal-dgpu-soc: Requirement "memory-tlm-backing-store" 的 6 个 Scenario
#include <cstdint>
#include <cstring>
#include <vector>
#include "event_queue.hh"
#include "tlm/memory_tlm.hh"
#include <catch2/catch_all.hpp>

static void inject_req(MemoryTLM* mem, uint64_t tid, uint64_t addr, bool wr,
                       uint64_t data = 0, uint8_t size = 8) {
    bundles::CacheReqBundle req;
    req.transaction_id.write(tid);
    req.address.write(addr);
    req.is_write.write(wr ? 1 : 0);
    req.data.write(data);
    req.size.write(size);
    mem->req_in().consume();
    std::memcpy(&mem->req_in().data(), &req, sizeof(req));
    mem->req_in().set_valid(true);
}

TEST_CASE("memory_backing: write then read roundtrip + out-of-range", "[memory_backing][chstream]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(4096, 0);
    mem.set_backing_store(backing.data(), backing.size());

    inject_req(&mem, 1, 0x10, true, 0xDEADBEEFCAFE1234ULL);
    mem.tick();
    REQUIRE(mem.resp_out().valid());
    auto wresp = mem.resp_out().data();
    REQUIRE(wresp.transaction_id.read() == 1);
    REQUIRE(wresp.error_code.read() == 0);
    REQUIRE(wresp.is_hit.read() == 1);
    mem.resp_out().clear_valid();

    inject_req(&mem, 2, 0x10, false);
    mem.tick();
    REQUIRE(mem.resp_out().valid());
    auto rresp = mem.resp_out().data();
    REQUIRE(rresp.data.read() == 0xDEADBEEFCAFE1234ULL);
    REQUIRE(rresp.error_code.read() == 0);
    REQUIRE(rresp.is_hit.read() == 1);

    REQUIRE(backing[0x10] == 0x34);
    REQUIRE(backing[0x17] == 0xDE);
    mem.resp_out().clear_valid();

    inject_req(&mem, 3, 0x2000, false);
    mem.tick();
    REQUIRE(mem.resp_out().valid());
    auto oresp = mem.resp_out().data();
    REQUIRE(oresp.error_code.read() == 1);
    REQUIRE(oresp.is_hit.read() == 0);
}

TEST_CASE("memory_backing: write size > 8 truncated to 8 bytes", "[memory_backing][chstream]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(64, 0);
    mem.set_backing_store(backing.data(), backing.size());

    inject_req(&mem, 1, 0x10, true, 0xAABBCCDD11223344ULL, 16);
    mem.tick();
    REQUIRE(mem.resp_out().valid());
    REQUIRE(mem.resp_out().data().error_code.read() == 0);
    mem.resp_out().clear_valid();

    REQUIRE(backing[0x10] == 0x44);
    REQUIRE(backing[0x17] == 0xAA);
    REQUIRE(backing[0x18] == 0);
}

TEST_CASE("memory_backing: nullptr backing legacy 0xDEADBEEF", "[memory_backing][chstream]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    inject_req(&mem, 1, 0x1000, false);
    mem.tick();
    REQUIRE(mem.resp_out().valid());
    auto resp = mem.resp_out().data();
    REQUIRE(resp.data.read() == 0xDEADBEEF);
    REQUIRE(resp.error_code.read() == 0);
    REQUIRE(resp.is_hit.read() == 0);
}

TEST_CASE("memory_backing: stats_requests counters track reads/writes", "[memory_backing][chstream]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(4096, 0);
    mem.set_backing_store(backing.data(), backing.size());

    inject_req(&mem, 1, 0x10, true, 0x1111);
    mem.tick();
    mem.resp_out().clear_valid();
    inject_req(&mem, 2, 0x20, false);
    mem.tick();
    mem.resp_out().clear_valid();

    std::ostringstream oss;
    mem.dumpStats(oss);
    auto s = oss.str();
    REQUIRE(s.find("requests_read") != std::string::npos);
    REQUIRE(s.find("requests_write") != std::string::npos);
    REQUIRE(s.find("latency") != std::string::npos);
}

TEST_CASE("memory_backing: set_size_bytes caps backing visible range", "[memory_backing][chstream]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(8192, 0);
    mem.set_backing_store(backing.data(), backing.size());
    mem.set_size_bytes(2048);

    inject_req(&mem, 1, 0x100, false);
    mem.tick();
    REQUIRE(mem.resp_out().valid());
    REQUIRE(mem.resp_out().data().error_code.read() == 0);
    mem.resp_out().clear_valid();

    inject_req(&mem, 2, 0x800, false);
    mem.tick();
    REQUIRE(mem.resp_out().valid());
    REQUIRE(mem.resp_out().data().error_code.read() == 1);
}