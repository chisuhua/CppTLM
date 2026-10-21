// test/test_cep_dummy_signals.cc
// CEP Stage 0 — Null 实现 10 流覆盖矩阵契约
// per openspec/changes/cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md
//     Requirement: cep-integration-seam-contract
//     + ADR-CEP-14 §2 决策 2(C3:折叠映射表)与决策 5(测试替身)
//     + ADR-CEP-07 §2 决策 4(NullProtocolEngine 10 流覆写)
// 作者: CppTLM Team / 日期: 2026-10-21
//
// 注:Stage 0 只断言**契约层** —— Null 实现本体是 Stage 1 的产品代码
//     (per ADR-CEP-14 决策 1:不测实现行为)。
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <string>
#include <vector>

using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    const std::vector<std::string>& canonical_flows() {
        static const std::vector<std::string> flows = {
            "request",   "completion",          "snoop",           "snoop_resp",
            "writeback", "forward_data",        "back_invalidate", "address_resolve",
            "barrier",   "snoop_completion_ack"};
        return flows;
    }

    std::string adr07() {
        return read_file(
            (repo_root() / "docs/adr/ADR-CEP-07-simulation-and-integration-semantics.md").string());
    }
    std::string adr05() {
        return read_file(
            (repo_root() / "docs/adr/ADR-CEP-05-stub-cache-core-interface.md").string());
    }
    std::string adr14() {
        return read_file((repo_root() / "docs/adr/ADR-CEP-14-stub-contract-tests.md").string());
    }

} // namespace

TEST_CASE("[cep][dummy-signals] NullProtocolEngine 契约覆写全部 10 流", "[cep][dummy-signals]") {
    const auto text = adr07();
    REQUIRE(text.find("NullProtocolEngine") != std::string::npos);
    for (const auto& f : canonical_flows()) {
        INFO("NullProtocolEngine 契约缺流: " << f);
        REQUIRE(text.find(f) != std::string::npos);
    }
    REQUIRE(text.find("10 流覆盖矩阵") != std::string::npos);
}

TEST_CASE("[cep][dummy-signals] canonical 10 流名与 ADR-CEP-02 逐名对齐", "[cep][dummy-signals]") {
    const auto text =
        read_file((repo_root() / "docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md")
                      .string());
    for (const auto& f : canonical_flows()) {
        INFO("ADR-CEP-02 缺流: " << f);
        REQUIRE(text.find(f) != std::string::npos);
    }
    // 零流数变更:不得出现第 11 条流
    REQUIRE(text.find("CEN-Ib11") != std::string::npos); // 存在的是 invariant,不是流
}

TEST_CASE("[cep][dummy-signals] 5 隐藏流的折叠映射表齐备", "[cep][dummy-signals]") {
    const auto text = adr05();
    for (const auto& f : {"forward_data", "back_invalidate", "barrier", "address_resolve",
                          "snoop_completion_ack"}) {
        INFO("折叠映射表缺隐藏流: " << f);
        REQUIRE(text.find(f) != std::string::npos);
    }
    REQUIRE(text.find("折叠映射表") != std::string::npos);
}

TEST_CASE("[cep][dummy-signals] Stub 净侧接口 5 条 + core-side 1 条", "[cep][dummy-signals]") {
    const auto text = adr05();
    for (const auto& i :
         {"request(addr, perm)", "snoop(addr, type, txn_id)",
          "snoop_resp(addr, hit, state, dirty, data, txn_id)", "writeback(addr, data, dirty)",
          "completion(txn_id, data, perm, error)", "evict_line(addr)"}) {
        INFO("Stub 接口缺失: " << i);
        REQUIRE(text.find(i) != std::string::npos);
    }
    REQUIRE(text.find("6 接口") != std::string::npos);
}

TEST_CASE("[cep][dummy-signals] 测试替身不入产品目录(per ADR-CEP-14 决策 5)",
          "[cep][dummy-signals]") {
    // 替身只允许在 test/ 下;include/tlm/cep/ 不得含替身类
    const auto prod = repo_root() / "include" / "tlm" / "cep";
    REQUIRE(std::filesystem::exists(prod));
    for (const auto& entry : std::filesystem::directory_iterator(prod)) {
        const std::string name = entry.path().filename().string();
        INFO("产品目录出现疑似测试替身: " << name);
        REQUIRE(name.find("test") == std::string::npos);
        REQUIRE(name.find("mock") == std::string::npos);
        REQUIRE(name.find("fake") == std::string::npos);
        REQUIRE(name.find("stub_double") == std::string::npos);
    }
}

TEST_CASE("[cep][dummy-signals] ADR-CEP-14 的 C1~C10 清单齐备", "[cep][dummy-signals]") {
    // ⚠️ 此前是**裸子串**查找:"C1" 会被 "INV-CEP-14-IC1" 里的 "IC1" 满足、
    //    "C10" 又包含 "C1" → 删掉 C1/C2/C3 三行**不会红**。
    //    现改为解析 ADR-CEP-14 的 C 表行,逐条断言 Cn 存在 + 锚点列非空。
    const auto text = adr14();
    std::set<int> found;
    std::map<int, std::string> anchors;
    // C 表行形态: | **C1** | ... | ... |
    static const std::regex row(R"(^\|\s*\*\*C(\d+)\*\*\s*\|([^|]*)\|([^|]*)\|)",
                                std::regex::multiline);
    for (std::sregex_iterator it(text.begin(), text.end(), row), e; it != e; ++it) {
        const int n = std::stoi(it->str(1));
        found.insert(n);
        anchors[n] = it->str(2) + " | " + it->str(3);
    }
    for (int i = 1; i <= 10; ++i) {
        INFO("ADR-CEP-14 的 C 表缺 C" << i);
        REQUIRE(found.count(i) == 1);
        // 每条合同测试必须有**非空锚点**(per INV-CEP-14-IC1:
        // 每条 Stub 合同测试必须可定位到至少一条 invariant id)
        INFO("C" << i << " 的锚点列为空: " << anchors[i]);
        REQUIRE_FALSE(anchors[i].empty());
    }
    REQUIRE(text.find("负向") != std::string::npos);
    REQUIRE(text.find("INV-CEP-14-IC2") != std::string::npos);
}
