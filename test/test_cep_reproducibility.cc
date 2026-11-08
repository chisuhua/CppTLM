// test/test_cep_reproducibility.cc
// 判据:**复现门**(change `cpptlm-cep-stub-cache-chstream-surface` · A4)
//   RPR-01 同种子 **新建实例** 两次 run ⇒ 比较集合**逐字段相等**
//         (13 键之外的运行侧计数:{cpu_requests, cpu_responses, net_requests_out,
//          net_completions_in, in_flight} + TrafficGen 发射/完成计数 + 地址序列)
//   RPR-02 不同种子 ⇒ 地址序列**不相等**(否则「种子可控」是假的)
//   RPR-03 `set_addr_range` ⇒ 地址**全部落在新区间**(分布对象须重建)
//   RPR-04 provenance 三项齐备(种子 / git rev / 工具链)且 git rev 非空
//   RPR-05 **反例**:`reset()` 后(不重播种)重跑 ⇒ **不相等**;显式 `set_seed(同值)` 后 ⇒ 相等
//
// ⚠️ 复现口径(per 修订 ④ ②「A4 写死」):同一 binary + 同一 stdlib + **新建实例** +
//    固定 tick 预算 ⇒ 只承诺「**可复跑**」,⛔ 不承诺跨 stdlib 位精确。
//
// 作者: builder  日期: 2026-11-08
#include "catch_amalgamated.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "core/event_queue.hh"
#include "tlm/cep/stub_cache_endpoint.hh"
#include "tlm/traffic_gen_tlm.hh"

#include "cep_experiment_harness.hh"

namespace {

    constexpr int kTicks = 600;
    constexpr std::uint64_t kSeed = 20261108;

    /// 向上找仓根(含 `.git` 的目录)⇒ provenance 的 git rev 读取用
    std::string find_repo_root() {
        const char* candidates[] = {".", "..", "../..", "../../..", "../../../.."};
        for (const char* c : candidates) {
            std::string path = std::string(c) + "/.git";
            std::ifstream probe(path);
            if (probe)
                return c;
        }
        return ".";
    }

    /// 一次 run 的可比较快照(A4.3 的比较集合)
    struct RunSnapshot {
        std::vector<std::uint64_t> addrs;
        std::uint64_t tg_issued = 0;
        std::uint64_t tg_completed = 0;
        std::uint64_t cpu_requests = 0;
        std::uint64_t cpu_responses = 0;
        std::uint64_t net_requests_out = 0;
        std::uint64_t net_completions_in = 0;
        std::uint64_t in_flight = 0;

        bool operator==(const RunSnapshot& o) const {
            return addrs == o.addrs && tg_issued == o.tg_issued && tg_completed == o.tg_completed &&
                   cpu_requests == o.cpu_requests && cpu_responses == o.cpu_responses &&
                   net_requests_out == o.net_requests_out &&
                   net_completions_in == o.net_completions_in && in_flight == o.in_flight;
        }
    };

    /// 固定 tick 预算驱动 `TrafficGenTLM` + `StubCacheEndpointTLM`(业务请求直接喂 wrapper)
    RunSnapshot drive(std::uint64_t seed, bool apply_range) {
        EventQueue eq;
        TrafficGenTLM tg("rpr_tg", &eq);
        tlm::cep::StubCacheEndpointTLM w("rpr_w", &eq);

        tg.set_mode(GenMode_TLM::RANDOM);
        tg.set_seed(seed);
        tg.set_num_requests(50);
        if (apply_range)
            tg.set_addr_range(0x100000, 0x200000);

        RunSnapshot s;
        for (int i = 0; i < kTicks; ++i) {
            const std::uint64_t before = tg.issued();
            tg.tick();
            if (tg.issued() > before) {
                const auto req = tg.req_out().data();
                s.addrs.push_back(req.address.read());
                w.pe_req_in().data() = req;
                w.pe_req_in().set_valid(true);
                w.tick();
            }
        }
        s.tg_issued = tg.issued();
        s.tg_completed = tg.completed();
        s.cpu_requests = w.counters().cpu_requests;
        s.cpu_responses = w.counters().cpu_responses;
        s.net_requests_out = w.counters().net_requests_out;
        s.net_completions_in = w.counters().net_completions_in;
        s.in_flight = w.in_flight();
        return s;
    }

} // namespace

TEST_CASE("RPR-01: 同种子 **新建实例** 两次 run ⇒ 比较集合逐字段相等", "[cep][repro][rpr-01]") {
    const RunSnapshot a = drive(kSeed, false);
    const RunSnapshot b = drive(kSeed, false);

    REQUIRE(a.addrs.size() > 10u); // 真的有流量(否则「相等」是空真)
    REQUIRE(a.tg_issued > 0u);
    REQUIRE(a == b);
    // 比较集合逐项显式(⛔ 不靠 operator== 一句带过)
    REQUIRE(a.cpu_requests == b.cpu_requests);
    REQUIRE(a.cpu_responses == b.cpu_responses);
    REQUIRE(a.net_requests_out == b.net_requests_out);
    REQUIRE(a.net_completions_in == b.net_completions_in);
    REQUIRE(a.in_flight == b.in_flight);
}

TEST_CASE("RPR-02: 不同种子 ⇒ 地址序列不相等", "[cep][repro][rpr-02]") {
    const RunSnapshot a = drive(kSeed, false);
    const RunSnapshot c = drive(kSeed + 1, false);
    REQUIRE(a.addrs.size() > 10u);
    REQUIRE(c.addrs.size() > 10u);
    REQUIRE(a.addrs != c.addrs);
}

TEST_CASE("RPR-03: `set_addr_range` ⇒ 地址全部落在新区间", "[cep][repro][rpr-03]") {
    const RunSnapshot s = drive(kSeed, true);
    REQUIRE(s.addrs.size() > 10u);
    for (const std::uint64_t a : s.addrs) {
        REQUIRE(a >= 0x100000u);
        REQUIRE(a < 0x200000u);
    }
}

TEST_CASE("RPR-04: provenance 三项齐备(种子 / git rev / 工具链)", "[cep][repro][rpr-04]") {
    cep_exp::ExperimentProvenance prov;
    prov.seed = kSeed;
    prov.git_rev = cep_exp::ExperimentProvenance::current_git_rev(find_repo_root());
    prov.toolchain = cep_exp::ExperimentProvenance::current_toolchain();

    REQUIRE_FALSE(prov.git_rev.empty());
    REQUIRE_FALSE(prov.toolchain.empty());
    REQUIRE(prov.seed == kSeed);

    const std::string js = prov.to_json();
    REQUIRE(js.find("\"seed\": 20261108") != std::string::npos);
    REQUIRE(js.find("\"git_rev\"") != std::string::npos);
    REQUIRE(js.find("\"toolchain\"") != std::string::npos);

    // ⚠️ 工具链字段**不含**路径 / 时间戳 ⇒ 同环境两次取值逐字节相等
    REQUIRE(prov.toolchain == cep_exp::ExperimentProvenance::current_toolchain());
}

TEST_CASE("RPR-05: **反例** —— reset 后不重播种 ⇒ 不相等;显式重播种 ⇒ 相等",
          "[cep][repro][rpr-05]") {
    EventQueue eq;
    TrafficGenTLM tg("rpr_reset", &eq);
    tg.set_mode(GenMode_TLM::RANDOM);
    tg.set_seed(kSeed);
    tg.set_num_requests(50);

    std::vector<std::uint64_t> first;
    for (int i = 0; i < kTicks; ++i) {
        const std::uint64_t before = tg.issued();
        tg.tick();
        if (tg.issued() > before)
            first.push_back(tg.req_out().data().address.read());
    }
    REQUIRE(first.size() > 10u);

    // ① `reset()` **不重播种** ⇒ 重跑得到**不同**序列(反例:登记为「reset ≠ 可复现」)
    tg.do_reset(ResetConfig{});
    std::vector<std::uint64_t> after_reset;
    for (int i = 0; i < kTicks; ++i) {
        const std::uint64_t before = tg.issued();
        tg.tick();
        if (tg.issued() > before)
            after_reset.push_back(tg.req_out().data().address.read());
    }
    REQUIRE(after_reset.size() > 10u);
    REQUIRE(after_reset != first);

    // ② 显式重播种 ⇒ 复现(证明「种子是控制变量」)
    tg.do_reset(ResetConfig{});
    tg.set_seed(kSeed);
    std::vector<std::uint64_t> after_reseed;
    for (int i = 0; i < kTicks; ++i) {
        const std::uint64_t before = tg.issued();
        tg.tick();
        if (tg.issued() > before)
            after_reseed.push_back(tg.req_out().data().address.read());
    }
    REQUIRE(after_reseed == first);
}
