// test/test_cep_compound_graph_falsification.cc
// 复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` 的**操作级**证伪工装判据
//
// per change `cep-memory-model-operational-trace`（backlog `cep-memory-model-operational-trace`）
//     + `ADR-CEP-22`（✅ 已签发 v1.0 · 每域模型来源 + 域间边构造规则）
//     + `ADR-CEP-13` 决策 2（公理）· 决策 5 ②（实验只能证伪 · 平台不提供无违例保证）
//     + `ADR-CEP-12` 决策 3（`consistency_labels` = 域模型来源）
//
// ── 判据纪律 ─────────────────────────────────────────────────────────────
// ① **只尝试证伪**：无环 ⇒ 「本次语料范围内**未证伪**」，⛔ **不是**「已证明」；
// ② 正控给出**真结果**（金标准违例迹 ⇒ falsified + 见证），反 fixture 证明机械**有牙**
//    （通道边混入 ⇒ 拒收；派生边作为输入 ⇒ 拒收；未绑定域 ⇒ 拒收；截断 ⇒ 不可判）；
// ③ ⛔ 任何输出字段**不得**出现「已证明 / proved」。
//
// ── 用例按「语料相关性」拆分（闸的多条臂分别跑）──────────────────────────
//   [generic]     —— 与语料无关的诚实性判据 + 判定产物写出（env 驱动）
//   [repo-corpus] —— **仓内**操作迹语料 ⇒ 金标准违例迹 + companion 迹（双迹判别）
//   [fixture]     —— 反 fixture（拒收族 + 截断族 + 畸形族）
//
// ── 环境变量 ─────────────────────────────────────────────────────────────
//   CPPTLM_COMPOUND_GRAPH_TRACES  —— 操作迹语料路径（默认 test/cep_memory_model_traces.json）
//   CPPTLM_COMPOUND_GRAPH_CORPUS  —— **通道**语料路径（默认 test/cep_compound_graph_corpus.json）
//                                     ⛔ 仅用于「通道边必须被拒收」的反 fixture（AC-11 的**取代**臂）
//   CPPTLM_COMPOUND_GRAPH_REPORT  —— 判定产物写出路径

#include "catch_amalgamated.hpp"

#include "tlm/cep/compound_graph_falsifier.hh"
#include "tlm/cep/memory_model_trace.hh"

#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

namespace {

    namespace cep = tlm::cep;
    namespace mm = tlm::cep::mm;

    std::string env_path(const char* name, const char* fallback) {
        const char* p = std::getenv(name);
        return (p != nullptr && *p != '\0') ? std::string(p) : std::string(fallback);
    }

    nlohmann::json read_json(const std::string& path) {
        std::ifstream in(path);
        if (!in) return nlohmann::json();
        nlohmann::json j;
        in >> j;
        return j;
    }

    /// 声明面（实例 `id` → `consistency_labels`）—— per `ADR-CEP-22` 决策 2。
    mm::InstanceLabelMap instances() {
        static const mm::InstanceLabelMap m =
            mm::load_instance_labels(read_json("include/tlm/cep/protocol_instances.json"));
        return m;
    }

    /// 仓内操作迹语料的全部迹（按 `id` 索引）。
    std::map<std::string, nlohmann::json> repo_traces() {
        std::map<std::string, nlohmann::json> out;
        const auto j = read_json(env_path("CPPTLM_COMPOUND_GRAPH_TRACES",
                                          "test/cep_memory_model_traces.json"));
        if (!j.is_object()) return out;
        const auto it = j.find("traces");
        if (it == j.end() || !it->is_array()) return out;
        for (const auto& t : *it) {
            if (!t.is_object()) continue;
            out[t.value("id", std::string())] = t;
        }
        return out;
    }

    /// 判定 → JSON（产物面）。
    nlohmann::json verdict_to_json(const std::string& trace_id, const cep::CompoundGraphVerdict& v) {
        nlohmann::json j;
        j["trace_id"] = trace_id;
        switch (v.verdict) {
            case cep::FalsifyVerdict::Falsified: j["verdict"] = "falsified"; break;
            case cep::FalsifyVerdict::NotFalsified: j["verdict"] = "not_falsified"; break;
            case cep::FalsifyVerdict::Undecided: j["verdict"] = "undecided"; break;
            default: j["verdict"] = "invalid"; break;
        }
        j["reason"] = v.reason;
        j["detail"] = v.detail;
        j["search_truncated"] = v.search_truncated;
        j["candidates_total"] = v.candidates_total;
        j["candidates_examined"] = v.candidates_examined;
        nlohmann::json cands = nlohmann::json::array();
        for (const auto& c : v.candidate_outcomes) {
            nlohmann::json cj;
            cj["id"] = c.id;
            cj["cyclic"] = c.cyclic;
            cands.push_back(cj);
        }
        j["candidate_outcomes"] = cands;
        j["edge_classes_observed"] = v.edge_classes_observed;
        j["cross_domain_edge_classes"] = v.cross_domain_edge_classes;
        j["rf_edge_classes"] = v.rf_edge_classes;
        j["axiom_edges"] = v.axiom_edges;
        j["cycle"] = v.cycle;
        j["co_order"] = v.co_order;
        j["coverage_note"] = v.coverage_note;
        return j;
    }

    /// 跑一条迹（解析 + 判定）⇒ 产物（⛔ 解析失败 ⇒ `invalid` + 原因码）。
    cep::CompoundGraphVerdict run_trace(const nlohmann::json& tj, std::size_t cap = 4096) {
        mm::Trace t;
        mm::ParseError err;
        if (!mm::parse_trace(tj, t, err)) {
            cep::CompoundGraphVerdict v;
            v.verdict = cep::FalsifyVerdict::Invalid;
            v.reason = err.code;
            v.detail = err.detail;
            // ⚠️ 措辞纪律：⛔ 本字符串**不得**含「已证明 / proved」字样
            //    （闸的产物扫描是**朴素子串匹配** ⇒ 否定语境里的同字面也会命中）
            v.coverage_note = "⛔ 迹**被拒收**（解析期）⇒ 无判定 ⇒ ⛔ 不得表述为公理**成立**"
                              "（per `ADR-CEP-13` 决策 5 ②「平台不提供无违例保证」）";
            return v;
        }
        return cep::evaluate_compound_graph_falsification(t, instances(), cap);
    }

    bool has_class(const std::vector<std::string>& v, const std::string& c) {
        return std::find(v.begin(), v.end(), c) != v.end();
    }

} // namespace

// ══════════════════════════════════════════════════════════════════════════════
//  [generic] 诚实性 —— 判定形态与语料无关；并写出判定产物
// ══════════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP composite-graph: 诚实性 —— 判定形态与语料无关 + 产出判定产物",
          "[cep][compound-graph][falsification][generic]") {
    // ① 消歧常量（本工装**不是**内存模型 litmus）
    static_assert(!cep::kFalsifierIsMemoryModelLitmus,
                  "litmus-scope: 复合图证伪工装不是内存模型 litmus");
    static_assert(cep::kFalsifierScope[0] != '\0', "litmus-scope: kFalsifierScope 必须非空");
    CHECK(std::string(cep::kFalsifierScope) == "operation-level-composite-graph");

    // ② 判定四态的**原因码闭集**（决策 7）
    const auto& codes = cep::falsify_reason_codes();
    CHECK(codes.size() == 11);
    CHECK(std::find(codes.begin(), codes.end(), "search-truncated") != codes.end());
    CHECK(std::find(codes.begin(), codes.end(), "channel-edge-not-op-edge") != codes.end());

    // ③ 跑仓内全部操作迹 ⇒ 写判定产物
    const auto traces = repo_traces();
    REQUIRE(!traces.empty());

    nlohmann::json report = nlohmann::json::array();
    for (const auto& [id, tj] : traces) {
        const auto v = run_trace(tj);
        // ④ 覆盖边界必须随判定一起读（⚠️ 恒不为空）
        CHECK(!v.coverage_note.empty());
        // ⑤ ⛔ 任何产物字段**不得**出现「已证明 / proved」
        const std::string blob = v.detail + "|" + v.coverage_note;
        CHECK(blob.find("已证明") == std::string::npos);
        CHECK(blob.find("proved") == std::string::npos);
        // ⑥ 原因码（若有）必须 ∈ 闭集
        if (!v.reason.empty()) {
            CHECK(std::find(codes.begin(), codes.end(), v.reason) != codes.end());
        }
        report.push_back(verdict_to_json(id, v));
    }

    // ⑦ **通道**语料必须被拒收（AC-11 的**取代**臂 —— 旧语义「通道环 ⇒ falsified」
    //    ⛔ **不得**保留：通道图 ≠ 操作图 ⇒ 混图 = 假绿）
    {
        const auto chan = read_json(env_path("CPPTLM_COMPOUND_GRAPH_CORPUS",
                                             "test/cep_compound_graph_corpus.json"));
        const auto v = run_trace(chan);
        CHECK(v.verdict == cep::FalsifyVerdict::Invalid);
        CHECK(v.reason == "channel-edge-not-op-edge");
        report.push_back(verdict_to_json("synthetic:channel-corpus", v));
    }

    // ⑧ **合成的反例**也写入产物（供闸逐臂核对 · 臂②~⑤）
    {
        const auto src = repo_traces();
        const auto base = src.at("xdomain-sb");
        // 派生边作为输入
        auto tj = base;
        tj["fr"] = nlohmann::json::array();
        report.push_back(verdict_to_json("synthetic:derived-edge-as-input", run_trace(tj)));
        // 未绑定域
        tj = base;
        tj["threads"][0].erase("domain");
        report.push_back(verdict_to_json("synthetic:unbound-domain", run_trace(tj)));
        // 截断 ⇒ 不可判（cap = 1 ⇒ 只考察第一个（成环的）候选）
        report.push_back(verdict_to_json("synthetic:search-truncated",
                                         run_trace(src.at("co-two-candidates"), 1)));
    }

    const char* out = std::getenv("CPPTLM_COMPOUND_GRAPH_REPORT");
    if (out != nullptr && *out != '\0') {
        std::ofstream os(out);
        REQUIRE(os.good());
        os << report.dump(2) << "\n";
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  [repo-corpus] 正控 —— 仓内操作迹语料 ⇒ 金标准违例迹 + companion 迹
// ══════════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP composite-graph: 正控 —— 金标准违例迹 ⇒ falsified + 见证；companion ⇒ 未证伪",
          "[cep][compound-graph][falsification][repo-corpus]") {
    const auto traces = repo_traces();
    REQUIRE(traces.count("xdomain-sb") == 1);
    REQUIRE(traces.count("xdomain-sb-companion") == 1);

    // ① 金标准违例迹：跨域 `rfe` 拉环 ⇒ 环在 `ppo_com ∪ rfe` 内 ⇒ **与写序候选无关**
    const auto bad = run_trace(traces.at("xdomain-sb"));
    CHECK(bad.verdict == cep::FalsifyVerdict::Falsified);
    CHECK(bad.reason.empty());
    // 见证：环上的操作 id 序列（⛔ 非空 ⇒ 判定不自洽）
    REQUIRE(!bad.cycle.empty());
    const std::set<std::string> cyc(bad.cycle.begin(), bad.cycle.end());
    CHECK(cyc.count("w1") == 1);
    CHECK(cyc.count("r1") == 1);
    CHECK(cyc.count("w2") == 1);
    CHECK(cyc.count("r2") == 1);
    // 公理面边类：⛔ **不含**域内 `rf`（决策 6 的双图折衷）
    CHECK(has_class(bad.edge_classes_observed, "rfe"));
    CHECK(!has_class(bad.edge_classes_observed, "rf"));
    CHECK(has_class(bad.edge_classes_observed, "ppo_com"));
    // 跨域边类 ⊆ schema `edge_classes` 枚举（AC-8① · 运行时从 schema 读）
    const auto schema = read_json("include/tlm/cep/protocol_instance.schema.json");
    const auto enum_it = schema["properties"]["combination_contract"]["properties"]
                                ["compound_consistency"]["properties"]["edge_classes"]["items"]["enum"];
    REQUIRE(enum_it.is_array());
    for (const auto& cls : bad.cross_domain_edge_classes) {
        CHECK(std::find(enum_it.begin(), enum_it.end(), cls) != enum_it.end());
    }

    // ② companion 迹（**唯一差异** = 域绑定 ⇒ 读自为域内）⇒ **未证伪**
    const auto good = run_trace(traces.at("xdomain-sb-companion"));
    CHECK(good.verdict == cep::FalsifyVerdict::NotFalsified);
    CHECK(good.reason.empty());
    CHECK(good.cycle.empty());
    // ⛔ 「未证伪」⛔ **≠**「已证明」—— 产物文本里不得出现「已证明」
    CHECK(good.detail.find("已证明") == std::string::npos);
    CHECK(good.coverage_note.find("已证明") == std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════════
//  [fixture] 反 fixture —— 拒收族（派生边 / 未绑定域 / 未知实例 / 畸形输入）
// ══════════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP composite-graph: 反 fixture —— 派生边 / 未绑定域 / 未知实例 ⇒ 必拒收",
          "[cep][compound-graph][falsification][fixture]") {
    const auto traces = repo_traces();
    REQUIRE(traces.count("xdomain-sb") == 1);

    // ① 派生边作为输入 ⇒ `derived-edge-as-input`（AC-3①）
    {
        auto tj = traces.at("xdomain-sb");
        tj["fr"] = nlohmann::json::array();
        const auto v = run_trace(tj);
        CHECK(v.verdict == cep::FalsifyVerdict::Invalid);
        CHECK(v.reason == "derived-edge-as-input");
    }
    // ② 线程未声明域 ⇒ `unbound-domain`（AC-6）
    {
        auto tj = traces.at("xdomain-sb");
        tj["threads"][0].erase("domain");
        const auto v = run_trace(tj);
        CHECK(v.verdict == cep::FalsifyVerdict::Invalid);
        CHECK(v.reason == "unbound-domain");
    }
    // ③ 域绑定的实例不在声明面 ⇒ `no-domain-instance`（AC-6）
    {
        auto tj = traces.at("xdomain-sb");
        tj["domains"][0]["instance"] = "no_such_instance";
        const auto v = run_trace(tj);
        CHECK(v.verdict == cep::FalsifyVerdict::Invalid);
        CHECK(v.reason == "no-domain-instance");
    }
    // ④ 畸形输入族（AC-10）：每个原因码各一条
    {
        auto dup = traces.at("xdomain-sb");
        dup["ops"].push_back({{"id", "r1"}, {"kind", "read"}, {"addr", 100}, {"value", 0}});
        CHECK(run_trace(dup).reason == "duplicate-op-id");

        auto missing = traces.at("xdomain-sb");
        missing["rf"][0]["write"] = "no_such_op";
        CHECK(run_trace(missing).reason == "rf-target-missing");

        auto selfloop = traces.at("xdomain-sb");
        selfloop["rf"][0]["read"] = "w1";
        selfloop["rf"][0]["write"] = "w1";
        CHECK(run_trace(selfloop).reason == "rf-self-loop");

        auto selfppo = traces.at("xdomain-sb");
        selfppo["threads"][0]["ops"] = {"r1", "r1"};
        CHECK(run_trace(selfppo).reason == "self-ppo-edge");

        auto malformed = traces.at("xdomain-sb");
        malformed.erase("family");
        CHECK(run_trace(malformed).reason == "malformed-trace");

        auto bad_anchor = traces.at("xdomain-sb");
        bad_anchor["anchor_id"] = "";
        CHECK(run_trace(bad_anchor).reason == "malformed-trace");

        auto unknown_anchor = traces.at("xdomain-sb");
        unknown_anchor["anchor_id"] = "not-in-allowlist";
        CHECK(run_trace(unknown_anchor).reason == "malformed-trace");
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  [fixture] ∃/∀ 聚合 —— 逐候选判定 + 聚合语义（AC-7）
// ══════════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP composite-graph: ∃/∀ 聚合 —— 某候选成环 ⛔ 不等于 证伪",
          "[cep][compound-graph][falsification][fixture]") {
    const auto traces = repo_traces();
    REQUIRE(traces.count("co-two-candidates") == 1);

    const auto v = run_trace(traces.at("co-two-candidates"));
    // 逐候选判定：**成环**的候选在前（∃/∀ 短路 ⇒ 先考察它）· 可行的候选在后
    REQUIRE(v.candidate_outcomes.size() == 2);
    CHECK(v.candidate_outcomes[0].id == "co1");
    CHECK(v.candidate_outcomes[0].cyclic == true);
    CHECK(v.candidate_outcomes[1].id == "co2");
    CHECK(v.candidate_outcomes[1].cyclic == false);
    // 聚合语义（决策 5 ③）：∃ 可行候选 ⇒ **未证伪**（⛔ 「某候选成环 ⇒ 证伪」是假阳性）
    CHECK(v.verdict == cep::FalsifyVerdict::NotFalsified);
    CHECK(v.reason.empty());
}

// ══════════════════════════════════════════════════════════════════════════════
//  [fixture] 截断 ⇒ 不可判（AC-9）
// ══════════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP composite-graph: 截断 ⇒ undecided + search-truncated（⛔ 不得判未证伪）",
          "[cep][compound-graph][falsification][fixture]") {
    const auto traces = repo_traces();
    REQUIRE(traces.count("co-two-candidates") == 1);

    // cap = 2 ⇒ 考察 co1（成环）后 co2（可行）⇒ **短路命中** ⇒ 未证伪
    const auto ok = run_trace(traces.at("co-two-candidates"), 2);
    CHECK(ok.verdict == cep::FalsifyVerdict::NotFalsified);
    CHECK(ok.candidates_examined == 2);
    CHECK(ok.search_truncated == false);

    // cap = 1 ⇒ 只考察**第一个**候选（成环的那个）⇒ 触上限且未找到可行候选 ⇒ **不可判**
    const auto cut = run_trace(traces.at("co-two-candidates"), 1);
    CHECK(cut.verdict == cep::FalsifyVerdict::Undecided);
    CHECK(cut.reason == "search-truncated");
    CHECK(cut.search_truncated == true);
}

// ══════════════════════════════════════════════════════════════════════════════
//  [fixture] **物理分离**（AC-3③）—— 两个语料文件的记录**互不出现对方的字段**
// ══════════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP composite-graph: 物理分离 —— 通道语料与操作迹互不出现对方字段",
          "[cep][compound-graph][falsification][fixture]") {
    // ① 通道语料**只**含通道字段（⛔ 无操作级字段）
    const auto chan = read_json(env_path("CPPTLM_COMPOUND_GRAPH_CORPUS",
                                         "test/cep_compound_graph_corpus.json"));
    REQUIRE(chan.is_object());
    const auto edges_it = chan.find("compound_edges");
    REQUIRE(edges_it != chan.end());
    for (const auto& e : *edges_it) {
        REQUIRE(e.is_object());
        for (const char* op_field : {"id", "kind", "addr", "domain", "thread", "read", "write"}) {
            CHECK(e.find(op_field) == e.end());
        }
    }
    // ② 操作迹**只**含操作级字段（⛔ 无通道字段）
    const auto traces = repo_traces();
    REQUIRE(!traces.empty());
    for (const auto& [id, tj] : traces) {
        for (const char* chan_field : {"compound_edges", "from_net", "to_net", "from_vc", "to_vc"}) {
            CHECK(tj.find(chan_field) == tj.end());
        }
        const auto ops_it = tj.find("ops");
        REQUIRE(ops_it != tj.end());
        for (const auto& o : *ops_it) {
            for (const char* chan_field : {"from_net", "to_net", "from_vc", "to_vc"}) {
                CHECK(o.find(chan_field) == o.end());
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  [fixture] `rf` vs `rfe` 分类边界（AC-8）+ 派生边集合（AC-3②）
// ══════════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP composite-graph: `rf` vs `rfe` 分类边界 + 派生边集合逐条核对",
          "[cep][compound-graph][falsification][fixture]") {
    const auto traces = repo_traces();
    REQUIRE(traces.count("rf-intra-domain") == 1);
    REQUIRE(traces.count("rf-cross-domain") == 1);

    // ① 域内读自 ⇒ 观测面 `rf`（⛔ 不进公理面）
    const auto intra = run_trace(traces.at("rf-intra-domain"));
    CHECK(intra.rf_edge_classes.size() == 1);
    CHECK(intra.rf_edge_classes[0] == "rf");
    CHECK(!has_class(intra.edge_classes_observed, "rfe"));
    CHECK(!has_class(intra.edge_classes_observed, "rf") == false);  // 观测面**报告**域内 `rf`

    // ② 跨域读自 ⇒ 观测面 `rfe`（**唯一差异** = writer 的域）
    const auto cross = run_trace(traces.at("rf-cross-domain"));
    CHECK(cross.rf_edge_classes.size() == 1);
    CHECK(cross.rf_edge_classes[0] == "rfe");
    CHECK(has_class(cross.edge_classes_observed, "rfe"));

    // ③ 派生边集合（AC-3②）：给定 `rf` + 写序 ⇒ `fr` / `ws` 逐条等于期望集
    const auto traces2 = repo_traces();
    REQUIRE(traces2.count("co-single") == 1);
    const auto v = run_trace(traces2.at("co-single"));
    std::set<std::string> edges(v.axiom_edges.begin(), v.axiom_edges.end());
    // `ws`（同址写序 co1 = w1 → w2）
    CHECK(edges.count("w1→w2:ws") == 1);
    // `fr(r, w2) ⟺ ∃w1: rf(w1,r) ∧ ws(w1,w2)` ⇒ r → w2
    CHECK(edges.count("r→w2:fr") == 1);
    // `ppo_com`（域 A 程序序 r → w2；域 B 单操作 ⇒ 无边）
    CHECK(edges.count("r→w2:ppo_com") == 1);
    // `rfe`（跨域读自 w1 → r）
    CHECK(edges.count("w1→r:rfe") == 1);
    // ⛔ 域内 `rf` **不得**进公理面（⛔ 注意 `:rf` 是 `:rfe` 的前缀 ⇒ 须比对**结尾**）
    for (const auto& e : v.axiom_edges) {
        const bool ends_with_rf =
            e.size() >= 3 && e.compare(e.size() - 3, 3, ":rf") == 0;
        CHECK(!ends_with_rf);
    }
}
