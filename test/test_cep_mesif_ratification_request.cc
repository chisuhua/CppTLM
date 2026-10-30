// test/test_cep_mesif_ratification_request.cc
// 队列 #9 `cpptlm-cep-mesif-adapter` —— 跨仓追认请求清单的机器判据
//
// per docs/implementation/2026-10-30-cep-mesif-adapter-ratification-request.md
//     (由 scripts/test/generate_mesif_ratification_request.sh 从**载体生成**)
//
// ── 判据纪律 ───────────────────────────────────────────────────────────
// ① 清单**必须**与机器可读载体**逐条一致**(载体是唯一真源)⇒ 清单手改 ⇒ 必红;
// ② 每条载体决策的 `basis` 非空 + `pending` 全 true(不得写成「已裁决」);
// ③ 反向 fixture 必须**可变异**(缺一条 / basis 置空 / pending 置 false ⇒ 必红);
// ④ 纯负向断言必带正控(同款输入下的反向取值可分辨)。
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "tlm/cep/mesif_protocol_engine.hh"
#include "tlm/cep/sharer_tracker.hh"

namespace {

    namespace cep = tlm::cep;

    constexpr const char* kRequestPath =
        "docs/implementation/2026-10-30-cep-mesif-adapter-ratification-request.md";

    std::string read_request_list() {
        const auto p = cpptlm::cep::repo_root() / kRequestPath;
        std::ifstream in(p, std::ios::binary);
        if (!in)
            return {};
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    /// 载体里**全部**决策 id(B-1 ~ B-5 + E-1 ~ E-6 + A-1)
    std::set<std::string> carrier_decision_ids() {
        std::set<std::string> ids;
        for (std::size_t i = 0; i < cep::HierarchicalDirectory::kBehaviorDecisionCount; ++i)
            ids.insert(cep::HierarchicalDirectory::kBehaviorDecisions[i].id);
        for (std::size_t i = 0; i < cep::MesifProtocolEngine::kEngineDecisionCount; ++i)
            ids.insert(cep::MesifProtocolEngine::kEngineDecisions[i].id);
        ids.insert("A-1"); // 债 #2 的纯请求项(本仓无载体)
        return ids;
    }

    /// 清单里**已列出**的请求项 id(解析 `### <id> ——` 标题行)
    std::set<std::string> listed_decision_ids(const std::string& text) {
        std::set<std::string> ids;
        std::size_t pos = 0;
        while ((pos = text.find("\n### ", pos)) != std::string::npos) {
            const std::size_t s = pos + 5;
            const std::size_t e = text.find(' ', s);
            if (e == std::string::npos)
                break;
            const std::string id = text.substr(s, e - s);
            if (!id.empty())
                ids.insert(id);
            pos = e;
        }
        return ids;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────
// ① 载体自洽:逐条 `basis` 非空 + `pending` 全 true(不得写成「已裁决」)
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP 追认请求:两类载体决策表自洽且全部 pending", "[cep][ratification-request]") {
    // tracker 侧(B-1 ~ B-5)
    REQUIRE(cep::HierarchicalDirectory::behavior_decisions_are_consistent());
    CHECK(cep::HierarchicalDirectory::kBehaviorDecisionCount == 5u);
    for (std::size_t i = 0; i < cep::HierarchicalDirectory::kBehaviorDecisionCount; ++i) {
        INFO("tracker 决策 " << cep::HierarchicalDirectory::kBehaviorDecisions[i].id);
        CHECK(cep::HierarchicalDirectory::kBehaviorDecisions[i].basis != nullptr);
        CHECK(cep::HierarchicalDirectory::kBehaviorDecisions[i].basis[0] != '\0');
        CHECK(cep::HierarchicalDirectory::kBehaviorDecisions[i].pending);
    }
    // 引擎侧(E-1 ~ E-6)
    REQUIRE(cep::MesifProtocolEngine::engine_decisions_are_consistent());
    CHECK(cep::MesifProtocolEngine::kEngineDecisionCount == 6u);
    for (std::size_t i = 0; i < cep::MesifProtocolEngine::kEngineDecisionCount; ++i) {
        INFO("引擎决策 " << cep::MesifProtocolEngine::kEngineDecisions[i].id);
        CHECK(cep::MesifProtocolEngine::kEngineDecisions[i].basis != nullptr);
        CHECK(cep::MesifProtocolEngine::kEngineDecisions[i].basis[0] != '\0');
        CHECK(cep::MesifProtocolEngine::kEngineDecisions[i].pending);
    }

    // ── 反向 fixture ①:把某条 `basis` 置空 ⇒ 谓词必红 ────────────────────
    {
        std::array<cep::HierarchicalDirectory::BehaviorDecision,
                   cep::HierarchicalDirectory::kBehaviorDecisionCount>
            mutated{};
        for (std::size_t i = 0; i < cep::HierarchicalDirectory::kBehaviorDecisionCount; ++i)
            mutated[i] = cep::HierarchicalDirectory::kBehaviorDecisions[i];
        mutated[2].basis = "";
        CHECK_FALSE(cep::HierarchicalDirectory::behavior_decisions_are_consistent(
            mutated.data(), cep::HierarchicalDirectory::kBehaviorDecisionCount));
    }
    // ── 反向 fixture ②:把某条标成「已裁决」(`pending = false`)⇒ 必红 ──────
    {
        std::array<cep::HierarchicalDirectory::BehaviorDecision,
                   cep::HierarchicalDirectory::kBehaviorDecisionCount>
            mutated{};
        for (std::size_t i = 0; i < cep::HierarchicalDirectory::kBehaviorDecisionCount; ++i)
            mutated[i] = cep::HierarchicalDirectory::kBehaviorDecisions[i];
        mutated[0].pending = false;
        CHECK_FALSE(cep::HierarchicalDirectory::behavior_decisions_are_consistent(
            mutated.data(), cep::HierarchicalDirectory::kBehaviorDecisionCount));
    }
    // ── 反向 fixture ③:引擎侧同理 ───────────────────────────────────────
    {
        std::array<cep::MesifProtocolEngine::EngineDecision,
                   cep::MesifProtocolEngine::kEngineDecisionCount>
            mutated{};
        for (std::size_t i = 0; i < cep::MesifProtocolEngine::kEngineDecisionCount; ++i)
            mutated[i] = cep::MesifProtocolEngine::kEngineDecisions[i];
        mutated[1].basis = "";
        CHECK_FALSE(cep::MesifProtocolEngine::engine_decisions_are_consistent(
            mutated.data(), cep::MesifProtocolEngine::kEngineDecisionCount));
    }
    // 正控:未变异时必绿(否则上面的反控可能因谓词恒假而恒绿)
    CHECK(cep::HierarchicalDirectory::behavior_decisions_are_consistent());
    CHECK(cep::MesifProtocolEngine::engine_decisions_are_consistent());
}

// ─────────────────────────────────────────────────────────────────────────
// ② 清单与载体**逐条一致**(清单是生成的 ⇒ 手改必红)
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP 追认请求:清单与机器载体逐条一致", "[cep][ratification-request]") {
    const std::string text = read_request_list();
    REQUIRE_FALSE(text.empty()); // 清单缺失 ⇒ 必红(不是「跳过」)

    const auto carrier = carrier_decision_ids();
    const auto listed = listed_decision_ids(text);

    // 载体有而清单缺 ⇒ 必红(漏登记)
    for (const auto& id : carrier) {
        INFO("清单缺载体决策项: " << id);
        CHECK(listed.count(id) == 1u);
    }
    // 清单有而载体无 ⇒ 必红(凭空登记)
    for (const auto& id : listed) {
        INFO("清单有载体没有的决策项: " << id);
        CHECK(carrier.count(id) == 1u);
    }
    CHECK(listed.size() == 12u); // 5 + 6 + 1

    // ── 反向 fixture:模拟「清单漏登记一条」⇒ 判定必红 ──────────────────
    //    ⚠️ 用**可变异**的集合运算表达(不真改文件)⇒ 判据有鉴别力。
    {
        std::set<std::string> mutated = listed;
        mutated.erase("B-5");
        bool missing_detected = false;
        for (const auto& id : carrier) {
            if (mutated.count(id) != 1u)
                missing_detected = true;
        }
        CHECK(missing_detected); // 漏一条 ⇒ 被抓到
        // 正控:未删时不被抓到
        bool not_detected = true;
        for (const auto& id : carrier) {
            if (listed.count(id) != 1u)
                not_detected = false;
        }
        CHECK(not_detected);
    }

    // ── 口径断言:清单必须写明「未回执 ⇒ pending 保持 true」─────────────
    CHECK(text.find("未回执") != std::string::npos);
    CHECK(text.find("pending") != std::string::npos);
    // 3 项债的编号都在
    for (const char* d : {"| #1 |", "| #2 |", "| #3 |"}) {
        INFO("清单缺债编号: " << d);
        CHECK(text.find(d) != std::string::npos);
    }
    // 生成器指针(保证「清单由载体生成」这一事实可寻址)
    CHECK(text.find("generate_mesif_ratification_request") != std::string::npos);
}
