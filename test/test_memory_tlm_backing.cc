// test/test_memory_tlm_backing.cc
// MemoryTLM backing-store 单元测试 (Phase A1 of openspec/changes/cpptlm-minimal-dgpu-soc-v1)
// 验证 spec/minimal-dgpu-soc: Requirement "memory-tlm-backing-store" 的 6 个 Scenario
#include <cstdint>
#include <cstring>
#include <vector>
#include "event_queue.hh"
#include "core/param_admission.hh"
#include "core/param_errors.hh"
#include "tlm/memory_tlm.hh"
#include <catch2/catch_all.hpp>

static void inject_req(MemoryTLM* mem, uint64_t tid, uint64_t addr, bool wr, uint64_t data = 0,
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

TEST_CASE("memory_backing: write then read roundtrip + out-of-range",
          "[memory_backing][chstream]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(4096, 0);
    mem.set_backing_view(backing.data(), backing.size());

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
    mem.set_backing_view(backing.data(), backing.size());

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

TEST_CASE("memory_backing: stats_requests counters track reads/writes",
          "[memory_backing][chstream]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(4096, 0);
    mem.set_backing_view(backing.data(), backing.size());

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

TEST_CASE("memory_backing: set_size_bytes caps backing visible range",
          "[memory_backing][chstream]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(8192, 0);
    mem.set_backing_view(backing.data(), backing.size());
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

// D-AXI v1.5 Phase 0.3 (B18): MemoryTLM::on_config_loaded wires capacity_gb
// 消除 MemoryTLM "1GB cap 不存在" 谎言 (Oracle/Metis 三轮敌对审查发现).
// 当前 on_config_loaded() 是 no-op (memory_tlm.hh:95-99); size_cap_ 不被 JSON 接线.
TEST_CASE("memory_backing: on_config_loaded wires capacity_gb (D-AXI B18)",
          "[memory_backing][chstream][d-axi][B18]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(2 * 1024 * 1024, 0);
    mem.set_backing_view(backing.data(), backing.size());

    nlohmann::json cfg = {{"capacity_gb", 1}};
    mem.set_config(cfg);

    // backing 应可访问; OUT_OF_RANGE 路径由 B12 实施 (当前 v2.2 路径无 bound check)
    inject_req(&mem, 1, 0x1000, false);
    mem.tick();
    REQUIRE(mem.resp_out().valid());
    mem.resp_out().clear_valid();

    REQUIRE(mem.backing_size() == backing.size());
}

TEST_CASE("memory_backing: on_config_loaded absent capacity_gb leaves size_cap untouched",
          "[memory_backing][chstream][d-axi][B18]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(4096, 0);
    mem.set_backing_view(backing.data(), backing.size());

    // ⚠️ 语义已变更（change `cpptlm-module-param-migrate-batch2` T1.2）：
    //    `MemoryTLM` 现已**准入** ⇒ 未知键不再被静默忽略。
    //    本用例原先用 `{"other_param": 42}` 验证「缺 capacity_gb 时 size_cap 不动」，
    //    但那个**未知键**现在会正确地让装配失败（这正是准入生效的证据）。
    //    ⇒ 改用**空 params**（合法的「什么都不配」形态）验证同一断言。
    nlohmann::json cfg = nlohmann::json::object();
    mem.set_config(cfg);

    inject_req(&mem, 1, 0x100, false);
    mem.tick();
    REQUIRE(mem.resp_out().valid());
    REQUIRE(mem.resp_out().data().error_code.read() == 0);
}

TEST_CASE("B18+: 准入后的 MemoryTLM 拒绝未知键（准入生效的正向证据）",
          "[memory_backing][chstream][d-axi][B18][param][migrate]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    std::vector<uint8_t> backing(4096, 0);
    mem.set_backing_view(backing.data(), backing.size());

    // 同一个键：准入前静默忽略、准入后**装配期失败**（且错误含键名）
    CHECK(cpptlm::is_param_admitted("MemoryTLM"));
    REQUIRE_THROWS_AS(mem.set_config(nlohmann::json{{"other_param", 42}}),
                      cpptlm::ParamValidationError);
}