// test/test_cep_litmus_falsification.cc
// 复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` 的 litmus **证伪工装**判据
//
// per change `cpptlm-cep-litmus-falsification-harness`（登记册队列 2 的 **I-6a**）
//     + `ADR-CEP-13` 决策 5 ②（实验只能证伪 · 平台不提供无违例保证）
//
// ── 判据纪律 ────────────────────────────────────────────────────────
// ① **只尝试证伪**：无环 ⇒ 「本次语料范围内**未证伪**」，⛔ **不是**「已证明」；
// ② 正控给出**真结果**（当前不可证伪），反 fixture 证明机械**有牙**（跨域环 ⇒ falsified）；
// ③ ⛔ 任何输出字段**不得**出现「已证明 / proved」。
//
// ── 用例按「语料相关性」拆分（闸的两条臂分别跑）──────────────────────
//   [generic]     —— 与语料无关的诚实性判据 + 判定产物写出（env 驱动）
//   [repo-corpus] —— **仓内**语料当前为空 ⇒ 复合公理不可证伪（闸的臂①）
//   [fixture]     —— 合成跨域环 ⇒ 证伪成功（闸的臂②）
#include "catch_amalgamated.hpp"

#include "tlm/cep/channel_dependency_graph.hh"
#include "tlm/cep/litmus_falsifier.hh"

#include <cstdlib>
#include <fstream>
#include <string>

namespace {

    namespace cep = tlm::cep;

    const cep::ChannelDeclaration& decl() {
        static const cep::ChannelDeclaration d =
            cep::load_channel_declaration(cep::kDefaultDeclarationPath);
        return d;
    }

    /// 跨域环语料：`compute_noc/0 → ualink_fabric/0 → compute_noc/0`
    /// （两个端点都是 `mesif` 已绑定的 vc ⇒ 顶点存在 ⇒ 边有效）
    nlohmann::json cross_domain_cycle_corpus() {
        nlohmann::json j;
        j["compound_edges"] = nlohmann::json::array({
            {{"instance", "mesif"},
             {"from_net", "compute_noc"},
             {"from_vc", 0},
             {"to_net", "ualink_fabric"},
             {"to_vc", 0},
             {"class", "rfe"}},
            {{"instance", "mesif"},
             {"from_net", "ualink_fabric"},
             {"from_vc", 0},
             {"to_net", "compute_noc"},
             {"to_vc", 0},
             {"class", "fr"}},
        });
        return j;
    }

    std::string corpus_path_from_env(const char* fallback) {
        const char* p = std::getenv("CPPTLM_LITMUS_CORPUS");
        return (p != nullptr && *p != '\0') ? std::string(p) : std::string(fallback);
    }

} // namespace

// ══════════════════════════════════════════════
//  [generic] 诚实性 —— 判定形态与语料无关；并写出判定产物
// ══════════════════════════════════════════════
TEST_CASE("CEP litmus: 诚实性 —— 判定形态与语料无关 + 产出判定产物",
          "[cep][litmus][falsification][generic]") {
    const std::string path = corpus_path_from_env("test/cep_litmus_corpus.json");
    bool readable = false;
    const auto corpus = cep::load_litmus_corpus(path, &readable);
    REQUIRE(readable);

    const auto v = cep::evaluate_litmus_falsification(decl(), corpus);

    // ① 覆盖说明恒非空（必须随判定一起读）
    REQUIRE_FALSE(v.coverage_note.empty());
    CHECK(v.coverage_note.find("不提供无违例保证") != std::string::npos);

    // ② 判定形态自洽：可达 ⇒ 有语料边且原因为空；不可达 ⇒ 原因非空且无环
    if (v.reachable) {
        CHECK(v.unreachable_reason.empty());
        CHECK(v.compound_edges > 0u);
    } else {
        REQUIRE_FALSE(v.unreachable_reason.empty());
        CHECK(v.compound_edges == 0u);
        CHECK(v.cycles.empty());
        CHECK(v.falsified == false);
    }
    // ③ 找到环 ⇒ 必为证伪；证伪 ⇒ 必有环
    CHECK((v.cycles.empty()) != v.falsified);

    // ④ 判定产物（env 驱动 ⇒ 平时**零写盘**）—— 闸据此机读核对
    const char* report_path = std::getenv("CPPTLM_LITMUS_REPORT");
    if (report_path != nullptr && *report_path != '\0') {
        nlohmann::json rep;
        rep["corpus_path"] = path;
        rep["reachable"] = v.reachable;
        rep["unreachable_reason"] = v.unreachable_reason;
        rep["compound_edges"] = v.compound_edges;
        rep["classes_seen"] = v.classes_seen;
        rep["cycles"] = v.cycles;
        rep["falsified"] = v.falsified;
        rep["coverage_note"] = v.coverage_note;
        std::ofstream out(report_path);
        REQUIRE(out.good());
        out << rep.dump(2) << '\n';
    }
}

// ══════════════════════════════════════════════
//  [repo-corpus] 正控 —— **仓内**语料为空 ⇒ 复合公理当前**不可证伪**
// ══════════════════════════════════════════════
TEST_CASE("CEP litmus: 正控 —— 仓内语料为空 ⇒ 复合公理当前不可证伪",
          "[cep][litmus][falsification][repo-corpus]") {
    bool readable = false;
    const auto corpus = cep::load_litmus_corpus("test/cep_litmus_corpus.json", &readable);
    REQUIRE(readable);
    CHECK(corpus.value("compound_edges", nlohmann::json::array()).empty());

    const auto v = cep::evaluate_litmus_falsification(decl(), corpus);
    // ① 不可达 + 给出**具体原因**
    CHECK(v.reachable == false);
    REQUIRE_FALSE(v.unreachable_reason.empty());
    CHECK(v.unreachable_reason.find("不可证伪") != std::string::npos);
    // ② ⛔ 未证伪（⛔ 且**不得**被表述为「已证明」）
    CHECK(v.falsified == false);
    CHECK(v.cycles.empty());
}

// ══════════════════════════════════════════════
//  [fixture] 反 fixture —— 跨域环 ⇒ 证伪成功（证明工装有牙）
// ══════════════════════════════════════════════
TEST_CASE("CEP litmus: 反 fixture —— 跨域环 ⇒ 证伪成功", "[cep][litmus][falsification][fixture]") {
    const auto v = cep::evaluate_litmus_falsification(decl(), cross_domain_cycle_corpus());
    // ① 语料非空 ⇒ 可达
    CHECK(v.reachable == true);
    CHECK(v.unreachable_reason.empty());
    CHECK(v.compound_edges == 2u);
    // ② 边类被**记录**（⛔ 不解释语义）
    REQUIRE(v.classes_seen.size() == 2u);
    CHECK(v.classes_seen[0] == "fr");
    CHECK(v.classes_seen[1] == "rfe");
    // ③ 找到环 ⇒ 证伪成立
    CHECK(v.falsified == true);
    REQUIRE(v.cycles.size() == 1u);
    // 环文本必须含**两个网络**（⇒ 证明是**跨域**环，不是单网内的旧环）
    const std::string& cyc = v.cycles[0];
    CHECK(cyc.find("compute_noc") != std::string::npos);
    CHECK(cyc.find("ualink_fabric") != std::string::npos);
}

// ══════════════════════════════════════════════
//  [fixture] 诚实性 —— 端点未绑定的边**不参与**（不得造出假环）
// ══════════════════════════════════════════════
TEST_CASE("CEP litmus: 诚实性 —— 端点未绑定 ⇒ 该边不参与、不得造出假环",
          "[cep][litmus][falsification][fixture]") {
    nlohmann::json j;
    j["compound_edges"] = nlohmann::json::array({
        {{"instance", "mesif"},
         {"from_net", "compute_noc"},
         {"from_vc", 0},
         {"to_net", "compute_noc"},
         {"to_vc", 0},
         {"class", "rfe"}},
    });
    const auto v = cep::evaluate_litmus_falsification(decl(), j);
    CHECK(v.reachable == true);
    // 自环边端点存在 ⇒ 它**确实**构成环（语料自洽的体现）
    CHECK(v.falsified == true);

    // ⛔ 端点**未绑定**的边（vc 越界）⇒ 不参与 ⇒ 不得产生环
    nlohmann::json bad;
    bad["compound_edges"] = nlohmann::json::array({
        {{"instance", "mesif"},
         {"from_net", "compute_noc"},
         {"from_vc", 0},
         {"to_net", "compute_noc"},
         {"to_vc", 7},
         {"class", "rfe"}},
    });
    const auto v2 = cep::evaluate_litmus_falsification(decl(), bad);
    CHECK(v2.reachable == true);
    CHECK(v2.falsified == false);
    CHECK(v2.cycles.empty());
}
