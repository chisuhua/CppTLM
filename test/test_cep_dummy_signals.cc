// test/test_cep_dummy_signals.cc
// CEP Stage 0 — Null 实现 10 流覆盖矩阵契约
// per openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md
//     Requirement: cep-integration-seam-contract
//     + ADR-CEP-14 §2 决策 2(C3:折叠映射表)与决策 5(测试替身)
//     + ADR-CEP-07 §2 决策 4(NullProtocolEngine 10 流覆写)
// 作者: CppTLM Team / 日期: 2026-10-21
//
// 注:Stage 0 只断言**契约层** —— Null 实现本体是 Stage 1 的产品代码
//     (per ADR-CEP-14 决策 1:不测实现行为)。
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <filesystem>
#include <regex>
#include <string>
#include <utility>
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

    // ── 「合同测试集**含**该负向断言」的机器判据 ────────────────────────
    // 负向断言宏(负控形式)
    bool has_negative_assertion(const std::string& content) {
        return content.find("REQUIRE_FALSE(") != std::string::npos ||
               content.find("CHECK_FALSE(") != std::string::npos;
    }

    /// 合同测试集内是否存在「**同时**含锚点 id 与负向断言」的文件。
    /// ⚠️ 按**锚点 id** 定位(⛔ 不依赖 C 表已陈旧的「归属测试文件」列 ——
    ///    C9 该列写的是 `test_cep_canonical_invariants.cc`,而 ADR-CEP-14 自记
    ///    「实测不成立」;实际负向断言在别处 ⇒ 按 id 定位可自动避开该陈旧列)。
    std::vector<std::string>
    files_with_anchor_and_negative(const std::string& anchor,
                                   const std::vector<std::pair<std::string, std::string>>& files) {
        std::vector<std::string> out;
        for (const auto& [path, content] : files) {
            if (content.find(anchor) != std::string::npos && has_negative_assertion(content))
                out.push_back(path);
        }
        return out;
    }

    /// 抽取 C 表「断言的不变量」列里的 invariant id(可多个)
    std::vector<std::string> extract_anchor_ids(const std::string& cell) {
        static const std::regex id_re(R"((INV-CEP-\d+A?-IC\d+|INV-SC\d+|CEN-Ib\d+))");
        std::vector<std::string> out;
        for (std::sregex_iterator it(cell.begin(), cell.end(), id_re), e; it != e; ++it)
            out.push_back(it->str());
        return out;
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

// 注:该 case 属 `ADR-CEP-14` **决策 5**(测试替身的产物处置),**不承载**任何 invariant
//     (architect 验收 2026-11-05:此处曾误挂 `INV-CEP-14-IC1` ⇒ 借壳,已移除标记)
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

// INV-CARRIER: INV-CEP-14-IC1
// 变红理由(per ADR-CEP-06 SU#4 规则 B):某条合同测试的**锚点列**为空(无锚点的合同测试)⇒ 红
// ⛔ 短式 id 已在本标题消除 ⇒ 目标唯一
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

// INV-CARRIER: INV-CEP-14-IC2
// 变红理由(per ADR-CEP-06 SU#4 规则 B):合同测试集**缺负向断言**(C9)/ 负向断言的判定
// 机制无鉴别力(恒不命中 ⇒ 负向断言形同虚设)⇒ 红
TEST_CASE("[cep][dummy-signals] 合同测试集含负向断言(C9)且该断言有鉴别力", "[cep][dummy-signals]") {
    // ① 清单元数据:ADR-CEP-14 的 C 表**恰有一行**标注「负向」,且其锚点列非空
    const auto text = adr14();
    static const std::regex row(R"(^\|\s*\*\*C(\d+)\*\*\s*\|([^|]*)\|([^|]*)\|([^|]*)\|)",
                                std::regex::multiline);
    int total_rows = 0;
    int negative_rows = 0;
    std::string negative_anchor;
    for (std::sregex_iterator it(text.begin(), text.end(), row), e; it != e; ++it) {
        ++total_rows;
        if (it->str(2).find("负向") != std::string::npos) {
            ++negative_rows;
            negative_anchor = it->str(3);
        }
    }
    INFO("C 表行数: " << total_rows);
    REQUIRE(total_rows == 10);
    INFO("C 表必须有**恰一条**负向断言行(C9)");
    REQUIRE(negative_rows == 1);
    INFO("负向行的「断言的不变量」列不得为空");
    REQUIRE(negative_anchor.find_first_not_of(" \t") != std::string::npos);

    // ② 反向 fixture:负向断言的判定机制**有鉴别力**(不是恒不命中)
    REQUIRE_FALSE(cpptlm::cep::protocol_identifier_hits("struct CHI_frame {};").empty());
    REQUIRE(cpptlm::cep::protocol_identifier_hits("struct frame {};").empty());

    // ③ 该负向断言**真的施加在产品层**(C9「Stub 不泄漏协议信息」的可执行形态):
    //    去注释后扫 Stub 源码 ⇒ 协议名命中集为空
    const auto stub =
        read_file((repo_root() / "include" / "tlm" / "cep" / "stub_cache_core.hh").string());
    REQUIRE_FALSE(stub.empty());
    const auto hits = cpptlm::cep::protocol_identifier_hits(cpptlm::cep::strip_comments(stub));
    std::string joined;
    for (const auto& h : hits)
        joined += h + " ";
    INFO("Stub 源码泄漏协议标识符: " << joined);
    REQUIRE(hits.empty());

    // ④ 判据主体(architect 复验 §7.2 追加):「合同测试集**含**该负向断言」——
    //    取 C 表负向行的**锚点 id**,要求仓内存在某个 `test/test_*.cc`
    //    **同时**含该锚点 id 与至少一条负向断言宏。
    //    ⇒ 删掉那条负向断言 ⇒ 红;删掉锚点 ⇒ 红。
    const auto anchors = extract_anchor_ids(negative_anchor);
    INFO("负向行的锚点列未解析出 invariant id: " << negative_anchor);
    REQUIRE(!anchors.empty());

    // 扫描仓内合同测试文件
    std::vector<std::pair<std::string, std::string>> test_files;
    {
        const auto dir = repo_root() / "test";
        REQUIRE(std::filesystem::exists(dir));
        for (const auto& e : std::filesystem::directory_iterator(dir)) {
            if (!e.is_regular_file() || e.path().extension() != ".cc")
                continue;
            const auto name = e.path().filename().string();
            if (name.rfind("test_", 0) != 0)
                continue;
            test_files.emplace_back(name, read_file(e.path().string()));
        }
    }
    INFO("扫描的合同测试文件数: " << test_files.size());
    REQUIRE(test_files.size() > 10u); // 防目录读空导致恒真

    for (const auto& anchor : anchors) {
        const auto carriers = files_with_anchor_and_negative(anchor, test_files);
        std::string joined2;
        for (const auto& c : carriers)
            joined2 += c + " ";
        INFO("合同测试集里没有「含锚点 " << anchor << " 且含负向断言」的文件");
        REQUIRE(!carriers.empty());
    }

    // ⑤ 反向 fixture(判别力):三条都必须判否 / 命中
    {
        // (a) 有锚点、无负向断言 ⇒ 判否
        const std::vector<std::pair<std::string, std::string>> only_anchor = {
            {"a.cc", "INV-CEP-05-IC1"}};
        REQUIRE(files_with_anchor_and_negative("INV-CEP-05-IC1", only_anchor).empty());
        // (b) 有负向断言、无锚点 ⇒ 判否
        const std::vector<std::pair<std::string, std::string>> only_negative = {
            {"b.cc", "REQUIRE_FALSE(x);"}};
        REQUIRE(files_with_anchor_and_negative("INV-CEP-05-IC1", only_negative).empty());
        // (c) 二者兼有 ⇒ 命中(证明判别力,不是恒否)
        const std::vector<std::pair<std::string, std::string>> both = {
            {"c.cc", "INV-CEP-05-IC1\nREQUIRE_FALSE(x);"}};
        REQUIRE(files_with_anchor_and_negative("INV-CEP-05-IC1", both).size() == 1u);
    }
}
