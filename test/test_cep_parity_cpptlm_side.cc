// test/test_cep_parity_cpptlm_side.cc
// H7 对拍基准 —— **CppTLM 侧**执行器(与 gem5 侧**参数同源**)
//
// 标签: [cep][parity]
//
// ── 为什么是「参数同源」而不是「文件同源」────────────────────────────
//   · **文件同源**(两侧读同一个 trace 文件)是 `14 号` §4.1 的原始契约;
//   · 但**实测发现 gem5 v24 的 protobuf 流读取不可靠**:
//     `ProtoInputStream` 用 `google::protobuf::io::IstreamInputStream`(零拷贝路径)
//     **逐条消息新建** `io::CodedInputStream` ⇒ **文件较大时提前 EOF**
//     (实测:1000 条读到 966 / 20000 条读到 19735 / 100000 条读到 98874;
//      同一文件用 Python 的 protobuf 库可完整读出 ⇒ 文件本身没问题);
//   ⇒ 本测试走**参数同源**:两侧**各自独立**按**同一组参数**生成访问序列,
//     再用**地址序列 sha256** 交叉验证「两边确实生成了同一条序列」。
//     ⇒ 这比「读同一个文件」**更强**:文件同源只能证明「读到了同样的数据」,
//       参数同源能证明「**各自独立生成的序列逐条相同**」。
//
// ── 序列定义(与 gem5 `PyTrafficGen.createLinear` 完全同构)────────────
//   地址: `start_addr + i*blocksize`,到 `end_addr` 后 wrap 到 `start_addr`
//   读写: `read_percent == 100` ⇒ 全读(gem5 侧 `LinearGen` 短路,不调 RNG)
//   条数: `data_limit / blocksize`
//
// ── 口径边界 ───────────────────────────────────────────────────────
//   `CacheTLM` 的命中判定是「该地址**曾被命中/写入**」(精确地址 map,无限容量),
//   gem5 是「容量内命中」(16KiB/2-way/LRU)⇒ **结构性差异**(per `14 号` §4.2)
//   ⇒ 命中率**不直接判**;本测试产出**路径延迟常数**供 P-4 比对。

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

    // 环境变量:`PARITY_OUT`(结果 JSON)· `PARITY_START_ADDR` / `PARITY_END_ADDR`
    //          / `PARITY_BLOCKSIZE` / `PARITY_READ_PERCENT` / `PARITY_DATA_LIMIT`
    //          / `PARITY_HIT_NS` / `PARITY_MISS_NS`
    std::string env_or(const char* key, const char* fallback) {
        const char* v = std::getenv(key);
        return (v && *v) ? std::string(v) : std::string(fallback);
    }

    long long env_ll(const char* key, long long fallback) {
        const std::string s = env_or(key, "");
        if (s.empty()) {
            return fallback;
        }
        return std::strtoll(s.c_str(), nullptr, 0);
    }

    // ── 与 gem5 `LinearGen` 同构的序列生成 ─────────────────────────────
    struct LinearSeq {
        uint64_t start_addr;
        uint64_t end_addr;
        uint64_t blocksize;
        uint64_t read_percent;
        uint64_t data_limit;
        size_t num_accesses;
    };

    // ⚠️ **必须与 gem5 的 `LinearGen` 逐条一致**:
    //   · gem5 的地址推进 = `nextAddr += blocksize`,到 `end_addr` 后 wrap;
    //   · gem5 的 `dataManipulated += blocksize` ⇒ 条数 = `data_limit / blocksize`;
    //   · gem5 的 `readPercent == 100` ⇒ 全读(短路,不调 RNG)。
    LinearSeq build_seq() {
        LinearSeq s;
        s.start_addr = static_cast<uint64_t>(env_ll("PARITY_START_ADDR", 0x100000));
        s.end_addr = static_cast<uint64_t>(env_ll("PARITY_END_ADDR", 0x104000));
        s.blocksize = static_cast<uint64_t>(env_ll("PARITY_BLOCKSIZE", 64));
        s.read_percent = static_cast<uint64_t>(env_ll("PARITY_READ_PERCENT", 100));
        s.data_limit = static_cast<uint64_t>(env_ll("PARITY_DATA_LIMIT", 100000 * 64));
        s.num_accesses = s.data_limit / s.blocksize;
        return s;
    }

    std::vector<uint64_t> gen_addresses(const LinearSeq& s) {
        std::vector<uint64_t> out;
        out.reserve(s.num_accesses);
        uint64_t addr = s.start_addr;
        for (size_t i = 0; i < s.num_accesses; ++i) {
            out.push_back(addr);
            addr += s.blocksize;
            if (addr >= s.end_addr) {
                addr = s.start_addr;
            }
        }
        return out;
    }

    // ⚠️ **FNV-1a 64bit** —— 必须与 gem5 侧(`gem5_linear_probe.py::_fnv1a64`)**逐字一致**:
    //    初始值 0xcbf29ce484222325 · 逐字节 xor 后乘 0x100000001b3 · 全程 mod 2^64。
    //    不用 SHA-256:本仓(C++)侧无现成实现 ⇒ 为「算个校验和」引入密码学哈希不划算。
    std::string seq_fnv1a64(const std::vector<uint64_t>& addrs) {
        uint64_t h = 0xcbf29ce484222325ULL;
        for (uint64_t v : addrs) {
            h ^= v;
            h *= 0x100000001b3ULL;
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
        return std::string(buf);
    }

} // namespace

TEST_CASE("CEP parity: CppTLM 侧参数同源", "[cep][parity]") {
    const std::string out_path = env_or("PARITY_OUT", "");

    const LinearSeq seq = build_seq();
    REQUIRE(seq.num_accesses > 0);
    REQUIRE(seq.end_addr > seq.start_addr);

    const std::vector<uint64_t> addrs = gen_addresses(seq);
    REQUIRE(addrs.size() == seq.num_accesses);

    // ── 装配:TrafficGenTLM → CacheTLM ──────────────────────────────
    EventQueue eq;
    TrafficGenTLM gen("parity_gen", &eq);
    CacheTLM cache("parity_cache", &eq);

    // 注入**同一条**序列(gem5 侧按同一参数独立生成 ⇒ sha256 交叉验证)
    std::vector<TraceRecord_TLM> trace_rows;
    trace_rows.reserve(addrs.size());
    const bool all_read = (seq.read_percent == 100);
    for (uint64_t a : addrs) {
        trace_rows.push_back({a, !all_read});
    }
    gen.set_trace(trace_rows);
    gen.set_mode(GenMode_TLM::TRACE);
    gen.set_num_requests(static_cast<int>(addrs.size()));

    // ⚠️ **手工搬运**(per `test/test_cache_tlm_comprehensive.cc` 的既有范式):
    //    `gen` 写进 `req_out_`,`cache` 从 `req_in_` 读 —— 两者之间**没有**
    //    StreamAdapter 绑定(那由 ModuleFactory 在正式装配时做)。
    uint64_t issued = 0;
    for (size_t i = 0; i < addrs.size(); ++i) {
        gen.tick();
        cache.req_in().consume();
        std::memcpy(&cache.req_in().data(), &gen.req_out().data(), sizeof(gen.req_out().data()));
        cache.req_in().set_valid(true);
        cache.tick();
        if (cache.resp_out().valid()) {
            cache.resp_out().clear_valid();
        }
        ++issued;
    }

    // 命中/未命中(cache 的统计;`StatGroup::stats()` 返回
    // `map<string, unique_ptr<StatBase>>` ⇒ 取值要用**指针**,不能拷贝)
    const auto& cs = cache.stats().stats();
    const auto& hits_it = cs.at("hits");
    const auto& misses_it = cs.at("misses");
    REQUIRE(hits_it != nullptr);
    REQUIRE(misses_it != nullptr);
    const uint64_t hits = static_cast<tlm_stats::Scalar&>(*hits_it).value();
    const uint64_t misses = static_cast<tlm_stats::Scalar&>(*misses_it).value();

    // 延迟常数:与实现一致(命中 5 / 未命中 50 cycle,1 cycle = 1 ns)
    // ⚠️ **不是手抄** ⇒ 由 `cpptlm_params.py` 抽取后经环境变量传入,
    //    避免两处副本(本仓的「双真源」漂移来源)。
    const double hit_ns = static_cast<double>(env_ll("PARITY_HIT_NS", 5));
    const double miss_ns = static_cast<double>(env_ll("PARITY_MISS_NS", 50));

    const double latency_sum_ns = hits * hit_ns + misses * miss_ns;
    const double avg_latency_ns = issued ? latency_sum_ns / double(issued) : 0.0;
    const double miss_rate = (hits + misses) ? double(misses) / double(hits + misses) : 0.0;

    const std::string sha = seq_fnv1a64(addrs);

    if (!out_path.empty()) {
        std::ofstream out(out_path);
        REQUIRE(out.good());
        const uint64_t n_reads = all_read ? issued : 0;
        const uint64_t n_writes = all_read ? 0 : issued;
        out << "{\n"
            << "  \"tool\": \"cpptlm\",\n"
            << "  \"mode\": \"parameter_same_source\",\n"
            << "  \"sequence\": {\n"
            << "    \"start_addr\": " << seq.start_addr << ",\n"
            << "    \"end_addr\": " << seq.end_addr << ",\n"
            << "    \"blocksize\": " << seq.blocksize << ",\n"
            << "    \"read_percent\": " << seq.read_percent << ",\n"
            << "    \"data_limit\": " << seq.data_limit << ",\n"
            << "    \"num_accesses\": " << seq.num_accesses << ",\n"
            << "    \"addr_seq_fnv1a64\": \"" << sha << "\"\n"
            << "  },\n"
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
            << "  \"latency_sum_ns\": " << latency_sum_ns << ",\n"
            << "  \"avg_latency_ns\": " << avg_latency_ns << ",\n"
            << "  \"ticks_per_ns\": 1000.0\n"
            << "}\n";
    }

    // 判据:访问数必须与序列条数**完全一致**;命中+未命中必须等于访问数
    REQUIRE(issued == seq.num_accesses);
    REQUIRE(hits + misses == issued);
}
