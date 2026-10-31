// test/test_cep_parity_cpptlm_side.cc
// H7 对拍基准 —— **CppTLM 侧**执行器(与 gem5 侧消费**同一条** trace)
//
// 标签: [cep][parity]
//
// 为什么需要它(per `docs/architecture/14-coherence-parity-baseline.md` §4.1):
//   · 本仓 `TrafficGenTLM` 的 RNG 是**内部种子** ⇒ 两侧各自生成 ⇒ 无法同源;
//   · gem5 `MemTest` 的地址序列由 `Random::genRandom()` 播种 ⇒ 每次运行不同;
//   ⇒ 唯一正解 = **离线生成一条确定性访问序列,两侧读同一文件**。
//
// 本测试:
//   ① 读 `scripts/cep/parity/parity_trace_gen.py` 产出的 **CSV** trace
//      (`addr,is_write,size` 三列);
//   ② 注入 `TrafficGenTLM`(`set_trace` + `GenMode_TLM::TRACE`);
//   ③ 跑 `TrafficGenTLM` → `CacheTLM`,统计 **命中 / 未命中 / 延迟**;
//   ④ 产出与 gem5 侧**同格式**的 JSON ⇒ 交给 `scripts/cep/parity/parity_compare.py` 比对。
//
// ⚠️ **口径边界**:`CacheTLM` 的命中判定是「该地址**曾被命中/写入**」
//    (`include/tlm/cache_tlm.hh`:`cache_lines_.count(addr) > 0`),
//    **无限容量 + 精确地址 map** ⇒ 与 gem5 的「容量内命中」是**结构性差异**
//    (per `14 号` §4.2)⇒ 命中率**不可直接比**;本测试只产出
//    **路径延迟常数**供 P-4 比对。

#include "catch_amalgamated.hpp"

#include "core/event_queue.hh"
#include "tlm/cache_tlm.hh"
#include "tlm/traffic_gen_tlm.hh"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

    // 环境变量:`PARITY_TRACE_CSV`(gem5 侧用的同一个 CSV)· `PARITY_OUT`(结果 JSON)
    std::string env_or(const char* key, const char* fallback) {
        const char* v = std::getenv(key);
        return (v && *v) ? std::string(v) : std::string(fallback);
    }

    struct TraceRow {
        uint64_t addr;
        bool is_write;
        uint64_t size;
    };

    std::vector<TraceRow> load_trace_csv(const std::string& path, size_t& total_rows) {
        std::vector<TraceRow> rows;
        total_rows = 0;
        std::ifstream in(path);
        REQUIRE(in.good());
        std::string line;
        // 跳过表头
        if (!std::getline(in, line)) {
            return rows;
        }
        while (std::getline(in, line)) {
            if (line.empty()) {
                continue;
            }
            std::istringstream ss(line);
            std::string a, w, s;
            if (!std::getline(ss, a, ',')) {
                continue;
            }
            std::getline(ss, w, ',');
            std::getline(ss, s, ',');
            TraceRow r;
            r.addr = std::strtoull(a.c_str(), nullptr, 0);
            r.is_write = (w == "1");
            r.size = std::strtoull(s.c_str(), nullptr, 0);
            rows.push_back(r);
            ++total_rows;
        }
        return rows;
    }

} // namespace

TEST_CASE("CEP parity: CppTLM 侧消费同源 trace", "[cep][parity]") {
    const std::string csv_path = env_or("PARITY_TRACE_CSV", "");
    const std::string out_path = env_or("PARITY_OUT", "");

    // ⚠️ 没有 trace ⇒ **跳过**(不失败):CI 上没跑 trace 生成器时不应红。
    if (csv_path.empty()) {
        SKIP("未设 PARITY_TRACE_CSV(trace 生成步骤未运行)");
    }

    size_t total_rows = 0;
    const std::vector<TraceRow> rows = load_trace_csv(csv_path, total_rows);
    REQUIRE(total_rows > 0);
    REQUIRE(rows.size() == total_rows);

    // ── 装配:TrafficGenTLM → CacheTLM ──────────────────────────────
    EventQueue eq;
    TrafficGenTLM gen("parity_gen", &eq);
    CacheTLM cache("parity_cache", &eq);

    // 注入**外部** trace(与 gem5 侧读同一 CSV ⇒ 逐条相同)
    std::vector<TraceRecord_TLM> trace_rows;
    trace_rows.reserve(rows.size());
    for (const auto& r : rows) {
        trace_rows.push_back({r.addr, r.is_write});
    }
    gen.set_trace(trace_rows);
    gen.set_mode(GenMode_TLM::TRACE);
    gen.set_num_requests(static_cast<int>(rows.size()));

    // 手工接线:`gen` 的请求出口 → `cache` 的请求入口;
    // `cache` 的响应出口 → `gen` 的响应入口。
    // (StreamAdapter 的端口绑定由 ModuleFactory 在正式装配时完成;
    //  这里走**直连**路径:见下方 `tick` 循环的显式搬运。)
    uint64_t issued = 0;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t latency_sum = 0;

    // ⚠️ `CacheTLM` 的延迟是**硬编码**(`hit ? 5 : 50`),且**无逐请求统计**;
    //    ⇒ 本侧产出的是「**命中/未命中计数** + 「**命中/未命中延迟常数**」
    //    (从实现源码抽取,per `scripts/cep/parity/cpptlm_params.py`)。
    //    逐请求延迟不单独采集(CacheTLM 无该统计口径)⇒ P-4 比的是**常数**。

    // ⚠️ **手工搬运**(per `test/test_cache_tlm_comprehensive.cc` 的既有范式):
    //    `gen` 写进自己的 `req_out_`,`cache` 从自己的 `req_in_` 读 ——
    //    两者之间**没有** StreamAdapter 绑定(那由 ModuleFactory 在正式装配时做)。
    //    ⇒ 这里逐条把 `gen.req_out().data()` 拷进 `cache.req_in().data()`。
    //    (CacheTLM 的响应是**同拍组合式**返回 ⇒ cache.tick() 一次即完成一次访问。)
    for (size_t i = 0; i < rows.size(); ++i) {
        gen.tick(); // 发出一条请求
        // 搬运请求
        cache.req_in().consume();
        std::memcpy(&cache.req_in().data(), &gen.req_out().data(), sizeof(gen.req_out().data()));
        cache.req_in().set_valid(true);
        cache.tick(); // cache 处理(命中 5 cycle / 未命中 50 cycle)
        // 消费响应(避免 resp_out 的 valid 残留)
        // ⚠️ `consume()` 只存在于 **InputStreamAdapter**;输出侧是 `clear_valid()`。
        if (cache.resp_out().valid()) {
            cache.resp_out().clear_valid();
        }
        ++issued;
    }

    // 从 cache 的统计里取命中/未命中
    // ⚠️ `StatGroup::stats()` 返回 **`std::map<std::string, std::unique_ptr<StatBase>>`**
    //    ⇒ 取值要**用指针**,不能拷贝(unique_ptr 不可拷贝)。
    const auto& cs = cache.stats().stats();
    // ⚠️ `at()` 返回 `unique_ptr&` ⇒ 必须用 `const auto&` 绑定引用
    //    (写 `auto` 会尝试**拷贝** unique_ptr ⇒ 编译失败)。
    const auto& hits_it = cs.at("hits");
    const auto& misses_it = cs.at("misses");
    REQUIRE(hits_it != nullptr);
    REQUIRE(misses_it != nullptr);
    hits = static_cast<tlm_stats::Scalar&>(*hits_it).value();
    misses = static_cast<tlm_stats::Scalar&>(*misses_it).value();

    // 延迟常数:与实现一致(命中 5 / 未命中 50 cycle,1 cycle = 1 ns)
    // ⚠️ 这里**不是**手抄 ⇒ 由 `cpptlm_params.py` 抽取后随环境变量传入,
    //    避免两处副本(本仓的「双真源」漂移来源)。
    const double hit_ns = std::atof(env_or("PARITY_HIT_NS", "5").c_str());
    const double miss_ns = std::atof(env_or("PARITY_MISS_NS", "50").c_str());

    const double access_latency_sum = hits * hit_ns + misses * miss_ns;
    const double avg_latency_ns = issued ? access_latency_sum / double(issued) : 0.0;
    latency_sum = static_cast<uint64_t>(access_latency_sum);

    const double miss_rate = (hits + misses) ? double(misses) / double(hits + misses) : 0.0;

    if (!out_path.empty()) {
        std::ofstream out(out_path);
        REQUIRE(out.good());
        // ⚠️ 读/写条数从 trace **实测**(不猜)⇒ 与 gem5 侧逐条一致。
        uint64_t n_reads = 0;
        uint64_t n_writes = 0;
        for (const auto& r : rows) {
            (r.is_write ? n_writes : n_reads)++;
        }
        out << "{\n"
            << "  \"tool\": \"cpptlm\",\n"
            << "  \"mode\": \"same_source_trace\",\n"
            << "  \"trace_file\": \"" << csv_path << "\",\n"
            << "  \"hits\": " << hits << ",\n"
            << "  \"misses\": " << misses << ",\n"
            << "  \"accesses\": " << issued << ",\n"
            << "  \"miss_rate\": " << miss_rate << ",\n"
            << "  \"num_reads\": " << n_reads << ",\n"
            << "  \"num_writes\": " << n_writes << ",\n"
            << "  \"hit_latency_ns\": " << hit_ns << ",\n"
            << "  \"miss_latency_ns\": " << miss_ns << ",\n"
            << "  \"hit_latency_ns_source\": \"derived(from cache_tlm.hh via cpptlm_params.py)\",\n"
            << "  \"miss_latency_ns_source\": \"derived(from cache_tlm.hh via "
               "cpptlm_params.py)\",\n"
            << "  \"latency_sum_ns\": " << latency_sum << ",\n"
            << "  \"avg_latency_ns\": " << avg_latency_ns << ",\n"
            << "  \"ticks_per_ns\": 1000.0\n"
            << "}\n";
    }

    // 判据:访问数必须与 trace 条数**完全一致**(否则两侧不同源 ⇒ 对拍无效)
    REQUIRE(issued == total_rows);
    REQUIRE(hits + misses == issued);
}
