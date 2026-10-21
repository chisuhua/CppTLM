// test/test_cep_simulation_mode_lock.cc
// CEP Stage 0 — simulation_mode 单一维度 + verification_mode 契约 + 4 件套契约
// per openspec/changes/cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md
//     Requirement: cep-experiment-output-contract
//     + cep-integration-seam-contract
// 作者: CppTLM Team / 日期: 2026-10-21
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using json = nlohmann::json;
using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    const std::vector<std::string>& required_metrics() {
        static const std::vector<std::string> m = {
            "total_cycles",       "total_transactions", "transactions_per_cycle",
            "avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
            "p99_latency_cycles", "max_latency_cycles", "coherence_stalls",
            "collective_stalls",  "directory_misses",   "deadlock_violations",
            "memory_violations"};
        return m;
    }

    const std::vector<std::string>& optional_metrics() {
        static const std::vector<std::string> m = {"link_utilization_per_link", "vc_stall_per_vc",
                                                   "snoop_fanout_per_request",
                                                   "directory_size_distribution"};
        return m;
    }

    const std::vector<std::string>& four_artifacts() {
        static const std::vector<std::string> a = {"simulation_config.json", "trace.h5",
                                                   "metrics.json", "report.html"};
        return a;
    }

    json load_metrics_schema() {
        const auto path = repo_root() / "experimental" / "metrics.schema.json";
        const std::string text = read_file(path.string());
        REQUIRE_FALSE(text.empty());
        return json::parse(text);
    }

    std::string adr_text(const std::string& file) {
        return read_file((repo_root() / "docs" / "adr" / file).string());
    }

} // namespace

TEST_CASE("[cep][simulation-mode] 4 件套恰好 4 个", "[cep][simulation-mode]") {
    const auto text = adr_text("ADR-CEP-06-experiment-protocol.md");
    for (const auto& a : four_artifacts()) {
        INFO("4 件套缺: " << a);
        REQUIRE(text.find(a) != std::string::npos);
    }
}

TEST_CASE("[cep][simulation-mode] 13 必纳 metric 齐备", "[cep][simulation-mode]") {
    const json s = load_metrics_schema();
    const auto& props = s.at("properties");
    for (const auto& m : required_metrics()) {
        INFO("metrics.schema 缺必纳 metric: " << m);
        REQUIRE(props.contains(m));
    }
    // ⚠️ 此前只校验**数量** → 换成 13 个任意名字也绿。改为集合相等。
    const auto& req = s.at("required");
    REQUIRE(req.size() == required_metrics().size());
    for (const auto& m : required_metrics()) {
        INFO("metrics.schema 的 required 缺: " << m);
        // ⚠️ nlohmann::json::contains() 只对 **object** 有效;对 array 恒返回 false
        //    → 必须显式遍历。
        bool found = false;
        for (const auto& r : req) {
            if (r.is_string() && r.get<std::string>() == m) {
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }
    for (const auto& m : optional_metrics()) {
        INFO("metrics.schema 缺可选 metric: " << m);
        REQUIRE(props.contains(m));
    }
}

TEST_CASE("[cep][simulation-mode] 不可观测 metric 的 not-collected 契约存在",
          "[cep][simulation-mode]") {
    const json s = load_metrics_schema();
    REQUIRE(s.at("properties").contains("_not_collected_reason"));
    const auto& r = s.at("properties").at("_not_collected_reason");
    REQUIRE(r.at("type") == "string");
    REQUIRE(r.at("minLength") == 1);
}

TEST_CASE("[cep][simulation-mode] verification_mode 四档且契约文本齐备", "[cep][simulation-mode]") {
    const json s = load_metrics_schema();
    const auto& props = s.at("properties");
    REQUIRE(props.contains("verification_mode"));
    REQUIRE(props.contains("verification_scope"));

    const std::set<std::string> modes(props.at("verification_mode").at("enum").begin(),
                                      props.at("verification_mode").at("enum").end());
    const std::set<std::string> expected_modes = {"exhaustive", "bounded_random", "sampled"};
    REQUIRE(modes == expected_modes);

    // ADR-CEP-06 §3:必须明文禁止把 bounded/sampled 表述为「已证明」
    const auto text = adr_text("ADR-CEP-06-experiment-protocol.md");
    REQUIRE(text.find("verification_mode") != std::string::npos);
    REQUIRE(text.find("verification_scope") != std::string::npos);
    REQUIRE(text.find("bounded_random") != std::string::npos);
    REQUIRE(text.find("sampled") != std::string::npos);
}

TEST_CASE("[cep][simulation-mode] memory_violations 须覆盖 SWMR + data-value 双不变量(IC5)",
          "[cep][simulation-mode]") {
    const json s = load_metrics_schema();
    const auto& props = s.at("properties");
    REQUIRE(props.contains("memory_violation_kinds"));
    const auto& kinds = props.at("memory_violation_kinds").at("properties");
    REQUIRE(kinds.contains("swmr"));
    REQUIRE(kinds.contains("data_value"));

    const auto text = adr_text("ADR-CEP-06-experiment-protocol.md");
    REQUIRE(text.find("SWMR") != std::string::npos);
    REQUIRE(text.find("data-value") != std::string::npos);
}

TEST_CASE("[cep][simulation-mode] simulation_mode 是唯一模式维度,无 network.mode",
          "[cep][simulation-mode]") {
    const auto text = adr_text("ADR-CEP-07-simulation-and-integration-semantics.md");
    REQUIRE(text.find("simulation_mode") != std::string::npos);

    // `network.mode` 的**每一处**出现都必须在否定语境。
    // 判定窗口同时覆盖出现点**前后**(否定词常在之前,如「不引入 network.mode 4 组合」)。
    const char* kNegations[] = {"不得", "禁止", "拒绝", "不引入", "不存在", "不共存", "无 "};
    std::size_t pos = 0;
    int occurrences = 0;
    while ((pos = text.find("network.mode", pos)) != std::string::npos) {
        ++occurrences;
        const std::size_t from = (pos > 200) ? pos - 200 : 0;
        const std::size_t to = (pos + 120 < text.size()) ? pos + 120 : text.size();
        const std::string ctx = text.substr(from, to - from);
        INFO("network.mode 第 " << occurrences << " 处出现但语境可疑: " << ctx);
        bool negated = false;
        for (const char* neg : kNegations) {
            if (ctx.find(neg) != std::string::npos) {
                negated = true;
                break;
            }
        }
        REQUIRE(negated);
        pos += 11;
    }
    REQUIRE(occurrences > 0);
    REQUIRE(text.find("INV-CEP-07-IC1") != std::string::npos);
}

TEST_CASE("[cep][simulation-mode] functional 模式 avg_latency_cycles == 0 为硬约束",
          "[cep][simulation-mode]") {
    const auto text = adr_text("ADR-CEP-07-simulation-and-integration-semantics.md");
    REQUIRE(text.find("avg_latency_cycles == 0") != std::string::npos);
    REQUIRE(text.find("INV-CEP-07-IC6") != std::string::npos);
}

TEST_CASE("[cep][simulation-mode] experimental/ 目录约定已建立", "[cep][simulation-mode]") {
    const auto dir = repo_root() / "experimental";
    REQUIRE(std::filesystem::exists(dir));
    REQUIRE(std::filesystem::exists(dir / "metrics.schema.json"));
}
