// test/test_cep_simulation_mode_lock.cc
// CEP Stage 0 — simulation_mode 单一维度 + verification_mode 契约 + 4 件套契约
// per openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md
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
    //
    // ⚠️ 2026-10-21 代码复审 S3:原判定是「±200/+120 **字符窗口**内出现任一否定词」
    //    —— 两个缺陷:
    //      · **反向语义可骗过**:「network.mode 与 simulation_mode 并列,均可使用
    //        (不得混淆二者)」含「不得」→ 判为合规,但契约已破;
    //      · **正向误报**:距否定词 >200 字的合法否决提及 → 判违规。
    //    现改为**行级**判定:否定词必须与 `network.mode` 在**同一行**
    //    (表格行算同一单元)—— 无关联的邻近句不再能充当否定证据。
    const char* kNegations[] = {"不得", "禁止", "拒绝", "不引入", "不存在", "不共存",
                                "无 ",  "不再", "不会", "非",     "剔除"};
    auto line_of = [&text](std::size_t pos) {
        const std::size_t b = text.rfind('\n', pos);
        const std::size_t e = text.find('\n', pos);
        return text.substr(b == std::string::npos ? 0 : b + 1,
                           (e == std::string::npos ? text.size() : e) -
                               (b == std::string::npos ? 0 : b + 1));
    };
    std::size_t pos = 0;
    int occurrences = 0;
    while ((pos = text.find("network.mode", pos)) != std::string::npos) {
        ++occurrences;
        const std::string line = line_of(pos);
        INFO("network.mode 第 " << occurrences << " 处出现但同行无否定词: " << line);
        bool negated = false;
        for (const char* neg : kNegations) {
            if (line.find(neg) != std::string::npos) {
                negated = true;
                break;
            }
        }
        REQUIRE(negated);
        pos += 11;
    }
    REQUIRE(occurrences > 0);
    REQUIRE(text.find("INV-CEP-07-IC1") != std::string::npos);

    // ⚠️ spec:143「协议插件**不引入** `network.mode`(机器 grep 可查)」——
    //    断言面必须覆盖**代码与配置**,不只是 ADR 文字。
    int code_hits = 0;
    for (const char* root : {"include", "src", "configs", "tools"}) {
        const auto dir = repo_root() / root;
        if (!std::filesystem::exists(dir))
            continue;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file())
                continue;
            const auto ext = entry.path().extension().string();
            if (ext != ".cc" && ext != ".hh" && ext != ".h" && ext != ".json" && ext != ".py") {
                continue;
            }
            const auto body = read_file(entry.path().string());
            if (body.find("network.mode") == std::string::npos)
                continue;
            ++code_hits;
            INFO("代码/配置里出现 network.mode: " << entry.path().string());
            // 该行必须含否定词(与 ADR 同口径)
            std::size_t p2 = 0;
            while ((p2 = body.find("network.mode", p2)) != std::string::npos) {
                const auto b = body.rfind('\n', p2);
                const auto e = body.find('\n', p2);
                const std::string ln = body.substr(b == std::string::npos ? 0 : b + 1,
                                                   (e == std::string::npos ? body.size() : e) -
                                                       (b == std::string::npos ? 0 : b + 1));
                bool neg = false;
                for (const char* n : kNegations) {
                    if (ln.find(n) != std::string::npos) {
                        neg = true;
                        break;
                    }
                }
                INFO("代码里 network.mode 同行无否定词: " << ln);
                REQUIRE(neg);
                p2 += 11;
            }
        }
    }
    INFO("代码/配置中 network.mode 命中数: " << code_hits);
    REQUIRE(code_hits == 0);
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
