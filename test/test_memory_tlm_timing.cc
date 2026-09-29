// test/test_memory_tlm_timing.cc
// MemoryTLM cycle-approximate 单元测试 (Phase 4 cpptlm-dgpu-soc-timing-mvp T1)
// 验证 design.md §2 + spec.md "MemoryTLM defer_response 路径":
//   1. read hit → 100 cyc latency + pending_resps_ queue order
//   2. read miss → 200 cyc latency
//   3. write → 120 cyc latency
//   4. out-of-range → 1 cyc immediate error
//   5. pending_resps_ order strict (priority_queue by ready_cycle)
//   6. use_zero_delay_for_test=true → functional-mode v2.2 路径零 diff
#include <cstdint>
#include <cstring>
#include <sstream>
#include <vector>
#include "event_queue.hh"
#include "tlm/memory_tlm.hh"
#include <catch2/catch_all.hpp>

namespace {

// 与 test_memory_tlm_backing.cc 相同的注入 helper
void inject_req(MemoryTLM* mem, uint64_t tid, uint64_t addr, bool wr, uint64_t data = 0,
                uint8_t size = 8) {
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

// 推进 N 个 cycle (advance + tick), 期间断言无 resp
void advance_and_tick_no_resp(MemoryTLM* mem, uint64_t n) {
    for (uint64_t c = 0; c < n; ++c) {
        mem->advance_cycle();
        mem->tick();
        REQUIRE(mem->resp_out().valid() == false);
    }
}

} // namespace

TEST_CASE("memory_timing: read hit → 100 cyc latency + pending queue",
          "[dgpu_soc_timing][memory_tlm][timing]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(8192, 0);
    mem.set_backing_view(backing.data(), backing.size());
    mem.set_timing_params(100, 200, 120, /*use_zero=*/false);

    // cycle 0: 读 addr 0x100 (row hit: addr & 0xF000 == 0)
    inject_req(&mem, 1, 0x100, false, 0, 8);
    mem.tick();
    REQUIRE(mem.inflight_resp_count() == 1);
    REQUIRE(mem.resp_out().valid() == false);

    // cycles 1..99: 未到 ready_cycle=100, 无 resp
    advance_and_tick_no_resp(&mem, 99);
    REQUIRE(mem.current_cycle() == 99);

    // cycle 100: ready_cycle=100 ≤ 100 → resp 发出
    mem.advance_cycle();
    mem.tick();
    REQUIRE(mem.current_cycle() == 100);
    REQUIRE(mem.resp_out().valid());
    auto resp = mem.resp_out().data();
    REQUIRE(resp.transaction_id.read() == 1);
    REQUIRE(resp.error_code.read() == 0);
    REQUIRE(resp.is_hit.read() == 1);
    REQUIRE(mem.inflight_resp_count() == 0);

    // stats_latency_read_ sample 100
    std::ostringstream oss;
    mem.dumpStats(oss);
    REQUIRE(oss.str().find("latency_read") != std::string::npos);
}

TEST_CASE("memory_timing: read miss → 200 cyc latency", "[dgpu_soc_timing][memory_tlm][timing]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(8192, 0);
    mem.set_backing_view(backing.data(), backing.size());
    mem.set_timing_params(100, 200, 120, /*use_zero=*/false);

    // addr 0x1100: row miss (addr & 0xF000 != 0), within backing range (8192)
    inject_req(&mem, 2, 0x1100, false, 0, 8);
    mem.tick();
    REQUIRE(mem.inflight_resp_count() == 1);
    REQUIRE(mem.resp_out().valid() == false);

    // cycles 1..199: 无 resp
    advance_and_tick_no_resp(&mem, 199);
    REQUIRE(mem.current_cycle() == 199);

    // cycle 200: resp 发出
    mem.advance_cycle();
    mem.tick();
    REQUIRE(mem.current_cycle() == 200);
    REQUIRE(mem.resp_out().valid());
    REQUIRE(mem.resp_out().data().transaction_id.read() == 2);
    REQUIRE(mem.resp_out().data().is_hit.read() == 1);
    REQUIRE(mem.inflight_resp_count() == 0);
}

TEST_CASE("memory_timing: write → 120 cyc latency", "[dgpu_soc_timing][memory_tlm][timing]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(8192, 0);
    mem.set_backing_view(backing.data(), backing.size());
    mem.set_timing_params(100, 200, 120, /*use_zero=*/false);

    // 写 0x100 (backing 写入立即, resp 延迟 120)
    inject_req(&mem, 3, 0x100, true, 0xDEADBEEFCAFE1234ULL, 8);
    mem.tick();
    REQUIRE(mem.inflight_resp_count() == 1);
    REQUIRE(backing[0x100] == 0x34); // 写立即可见 (TInv-2 强 ordering)

    // cycles 1..119: 无 resp
    advance_and_tick_no_resp(&mem, 119);
    REQUIRE(mem.current_cycle() == 119);

    mem.advance_cycle(); // cycle 120
    mem.tick();
    REQUIRE(mem.current_cycle() == 120);
    REQUIRE(mem.resp_out().valid());
    REQUIRE(mem.resp_out().data().transaction_id.read() == 3);
    REQUIRE(mem.resp_out().data().error_code.read() == 0);
    REQUIRE(mem.inflight_resp_count() == 0);
}

TEST_CASE("memory_timing: out-of-range → 1 cyc immediate error", "[dgpu_soc_timing][memory_tlm][timing]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(4096, 0);
    mem.set_backing_view(backing.data(), backing.size());
    mem.set_timing_params(100, 200, 120, /*use_zero=*/false);

    // addr 0x10000 超出 cap (4096) → error, latency=1
    inject_req(&mem, 4, 0x10000, false, 0, 8);
    mem.tick();
    REQUIRE(mem.inflight_resp_count() == 1);

    // cycle 1: error resp 发出 (latency=1, 优先于其他 in-flight)
    mem.advance_cycle();
    mem.tick();
    REQUIRE(mem.current_cycle() == 1);
    REQUIRE(mem.resp_out().valid());
    auto resp = mem.resp_out().data();
    REQUIRE(resp.transaction_id.read() == 4);
    REQUIRE(resp.error_code.read() == 1);
    REQUIRE(resp.is_hit.read() == 0);
    REQUIRE(mem.inflight_resp_count() == 0);
}

TEST_CASE("memory_timing: pending_resps_ order strict (priority by ready_cycle)",
          "[dgpu_soc_timing][memory_tlm][timing]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(8192, 0);
    mem.set_backing_view(backing.data(), backing.size());
    // read_hit=100 (req A), read_miss=50 (req B): B 后到但先 ready
    mem.set_timing_params(100, 50, 120, /*use_zero=*/false);

    // cycle 0: req A read hit (addr 0x100) → ready=0+100=100
    inject_req(&mem, 1, 0x100, false, 0, 8);
    mem.tick();

    // cycle 5: req B read miss (addr 0x1100, within backing) → ready=5+50=55
    for (uint64_t c = 0; c < 5; ++c) mem.advance_cycle();
    inject_req(&mem, 2, 0x1100, false, 0, 8);
    mem.tick();
    REQUIRE(mem.current_cycle() == 5);
    REQUIRE(mem.resp_out().valid() == false);  // 100 > 5

    // cycle 55: B 先发出 (ready=55 < A ready=100)
    for (uint64_t c = 0; c < 50; ++c) mem.advance_cycle(); // cycle 5→55
    mem.tick();
    REQUIRE(mem.current_cycle() == 55);
    REQUIRE(mem.resp_out().valid());
    REQUIRE(mem.resp_out().data().transaction_id.read() == 2);
    mem.resp_out().clear_valid();

    // cycle 100: A 发出
    for (uint64_t c = 0; c < 45; ++c) mem.advance_cycle(); // cycle 55→100
    mem.tick();
    REQUIRE(mem.current_cycle() == 100);
    REQUIRE(mem.resp_out().valid());
    REQUIRE(mem.resp_out().data().transaction_id.read() == 1);
    mem.resp_out().clear_valid();
    REQUIRE(mem.inflight_resp_count() == 0);
}

TEST_CASE("memory_timing: use_zero_delay_for_test=true → functional zero diff",
          "[dgpu_soc_timing][memory_tlm][timing]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(4096, 0);
    mem.set_backing_view(backing.data(), backing.size());
    // 默认 use_zero_delay_for_test_=true (M5): 立即 resp, 不触发 pending_resps_
    REQUIRE(mem.use_zero_delay_for_test() == true);

    inject_req(&mem, 1, 0x10, true, 0xABCDEFULL, 8);
    mem.tick();
    REQUIRE(mem.resp_out().valid());
    REQUIRE(mem.resp_out().data().error_code.read() == 0);
    REQUIRE(mem.inflight_resp_count() == 0); // 不走 pending_resps_
    mem.resp_out().clear_valid();

    inject_req(&mem, 2, 0x10, false, 0, 8);
    mem.tick();
    REQUIRE(mem.resp_out().valid());
    REQUIRE(mem.resp_out().data().data.read() == 0xABCDEFULL);
    REQUIRE(mem.resp_out().data().is_hit.read() == 1);
    REQUIRE(mem.inflight_resp_count() == 0);
}
