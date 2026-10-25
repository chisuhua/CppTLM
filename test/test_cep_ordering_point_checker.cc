// test/test_cep_ordering_point_checker.cc
// capability `cep-ordering-point-checker` —— 6 Scenario / 6 锚点(
//   AddressResolverIf 单方法 / J1 静态路径覆盖 / J2 与 caps 耦合 / INV-CEP-08-IC2 /
//   S1-7 (β) 裁决交付物 / J3 vacuous 降级)
//
// 作者: CppTLM Team / 日期: 2026-10-25
//
// 纪律:
//   · J2 在**合法声明**上恒 PASS(schema enum 无 PARTIAL ⇒ vacuous),判别力由**合成声明**承担
//   · J3 的通用形式不可静态判定 ⇒ 显式 SKIP + `vacuous` 标记,不得静默当作已实现

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/address_resolver.hh"
#include "tlm/cep/ordering_point_checker.hh"

namespace {

    namespace cep = tlm::cep;

    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    std::string strip_comments(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        bool line_comment = false;
        bool block_comment = false;
        for (std::size_t i = 0; i < s.size(); ++i) {
            const char c = s[i];
            const char n = (i + 1 < s.size()) ? s[i + 1] : '\0';
            if (line_comment) {
                if (c == '\n') {
                    line_comment = false;
                    out.push_back(c);
                }
                continue;
            }
            if (block_comment) {
                if (c == '*' && n == '/') {
                    block_comment = false;
                    ++i;
                }
                continue;
            }
            if (c == '/' && n == '/') {
                line_comment = true;
                ++i;
                continue;
            }
            if (c == '/' && n == '*') {
                block_comment = true;
                ++i;
                continue;
            }
            out.push_back(c);
        }
        return out;
    }

    std::size_t count_member_virtuals(const std::string& code) {
        std::size_t n = 0;
        std::istringstream in(code);
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("    virtual ", 0) == 0)
                ++n;
        }
        return n;
    }

    /// 读 S1-7 裁决的登记文件:**双态**(change 目录内 → 归档件)。
    /// `kS1_7BetaVerdict.registered_in` 是**逻辑指针**,不随归档改名而变。
    std::string read_registered_doc(const std::string& logical) {
        namespace fs = std::filesystem;
        if (const std::string text = read_repo_file(logical); !text.empty())
            return text;

        const std::string marker = "openspec/changes/";
        const auto pos = logical.find(marker);
        if (pos == std::string::npos)
            return {};
        const std::string tail = logical.substr(pos + marker.size()); // "<name>/design.md"
        const std::string change = tail.substr(0, tail.find('/'));
        const std::string rest = tail.substr(tail.find('/') + 1);

        const fs::path arch = fs::path(CPPTLM_SOURCE_DIR) / "openspec/changes/archive";
        std::error_code ec;
        if (!fs::is_directory(arch, ec))
            return {};
        for (const auto& e : fs::directory_iterator(arch, ec)) {
            if (!e.is_directory())
                continue;
            if (e.path().filename().string().find(change) == std::string::npos)
                continue;
            const fs::path candidate = e.path() / rest;
            std::error_code ec2;
            if (!fs::exists(candidate, ec2))
                continue;
            std::ifstream in(candidate);
            std::ostringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }
        return {};
    }

    cep::ChannelDeclaration repo_declaration() {
        return cep::load_channel_declaration(std::string(CPPTLM_SOURCE_DIR) +
                                             "/include/tlm/cep/protocol_instances.json");
    }

    const cep::CriterionResult* find_result(const cep::OrderingReport& r, cep::CriterionId id) {
        for (const auto& c : r.results) {
            if (c.criterion == id)
                return &c;
        }
        return nullptr;
    }

    /// 单网络、响应向 p2p_ordered 的最小合法实例(J3 的「单点」样本)
    cep::DeclaredInstance single_point_instance() {
        cep::DeclaredInstance d;
        d.id = "synthetic_single_point";
        d.status = "planned";
        d.completeness = "EXACT";
        d.vc_binding["net_a"] = {{"request", 0}, {"response", 1}};
        d.ordering["net_a"] = {{0, "unordered"}, {1, "p2p_ordered"}};
        return d;
    }

} // namespace

// ═════════════════════════════════════════════════════════════════════
// Scenario 1:AddressResolverIf 恰含单方法 resolve(pa) → home
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP OrderingPoint: AddressResolverIf 单方法", "[cep][ordering-point]") {
    using ResolveSig = cep::node_id_t (cep::AddressResolverIf::*)(std::uint64_t) const;
    CHECK(std::is_same_v<decltype(&cep::AddressResolverIf::resolve), ResolveSig>);

    // 「仅 1 个方法」的机检:虚成员计数 = resolve + 虚析构
    const std::string code = strip_comments(read_repo_file("include/tlm/cep/address_resolver.hh"));
    REQUIRE(!code.empty());
    CHECK(count_member_virtuals(code) == 2);

    // 行为:窗口内归属 home,窗口外未映射(不得静默当作某节点)
    cep::WindowAddressResolver window(cep::kNodeIdVram, 0x1000, 0x100);
    CHECK(window.resolve(0x1000) == cep::kNodeIdVram);
    CHECK(window.resolve(0x10FF) == cep::kNodeIdVram);
    CHECK(window.resolve(0x1100) == cep::kInvalidNode);
    CHECK(window.resolve(0x0FFF) == cep::kInvalidNode);
    cep::WindowAddressResolver empty(cep::kNodeIdVram, 0x0, 0);
    CHECK(empty.resolve(0x0) == cep::kInvalidNode);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 2:J1 单点性 —— 响应向路径必须落在有序点
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP OrderingPoint: J1 静态路径覆盖", "[cep][ordering-point]") {
    const cep::ChannelDeclaration decl = repo_declaration();
    REQUIRE(!decl.empty());

    const cep::OrderingReport report = cep::evaluate_ordering_points(decl);
    const auto* j1 = find_result(report, cep::CriterionId::J1_SinglePoint);
    REQUIRE(j1 != nullptr);
    CHECK(j1->verdict == cep::GateVerdict::PASS);
    CHECK(j1->detail.find("p2p_ordered") != std::string::npos);

    // 负向:把某响应向 VC 改成 unordered ⇒ J1 必须翻红(输入改变 ⇒ 输出改变)
    cep::ChannelDeclaration broken = decl;
    REQUIRE(!broken.instances.empty());
    auto& ordering = broken.instances.front().ordering["compute_noc"];
    REQUIRE(ordering.count(1) == 1);
    ordering[1] = "unordered";

    const cep::OrderingReport broken_report = cep::evaluate_ordering_points(broken);
    const auto* broken_j1 = find_result(broken_report, cep::CriterionId::J1_SinglePoint);
    REQUIRE(broken_j1 != nullptr);
    CHECK(broken_j1->verdict == cep::GateVerdict::FAIL);
    CHECK(broken_j1->detail.find("unordered") != std::string::npos);
    CHECK(broken_report.verdict == cep::GateVerdict::FAIL);

    // 负向:删掉整段 ordering_assumption ⇒ 也是 FAIL(未声明 ≠ 有序)
    cep::ChannelDeclaration missing = decl;
    missing.instances.front().ordering.clear();
    const auto missing_report = cep::evaluate_ordering_points(missing);
    const auto* missing_j1 = find_result(missing_report, cep::CriterionId::J1_SinglePoint);
    REQUIRE(missing_j1 != nullptr);
    CHECK(missing_j1->verdict == cep::GateVerdict::FAIL);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 3:J2 全序可观测性与 caps 耦合
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP OrderingPoint: J2 与 caps 耦合", "[cep][ordering-point]") {
    // 合成声明:PARTIAL 无广播兜底 ⇒ FAIL(决策 2 明令禁止)
    cep::DeclaredInstance partial = single_point_instance();
    partial.id = "synthetic_partial_no_fallback";
    partial.completeness = "PARTIAL";
    partial.broadcast_fallback = false;

    cep::ChannelDeclaration decl;
    decl.instances.push_back(partial);
    auto report = cep::evaluate_ordering_points(decl);
    const auto* j2 = find_result(report, cep::CriterionId::J2_TotalOrderObservability);
    REQUIRE(j2 != nullptr);
    CHECK(j2->verdict == cep::GateVerdict::FAIL);
    CHECK(report.verdict == cep::GateVerdict::FAIL);

    // 同一份声明 + 声明兜底 ⇒ PASS(输入改变 ⇒ 输出改变)
    decl.instances.front().broadcast_fallback = true;
    report = cep::evaluate_ordering_points(decl);
    j2 = find_result(report, cep::CriterionId::J2_TotalOrderObservability);
    REQUIRE(j2 != nullptr);
    CHECK(j2->verdict == cep::GateVerdict::PASS);
    CHECK(j2->detail.find("broadcast fallback") != std::string::npos);

    // 合法声明(schema enum 无 PARTIAL)⇒ PASS,且 detail 显式登记 vacuous 事实
    const cep::ChannelDeclaration repo = repo_declaration();
    REQUIRE(!repo.empty());
    const auto repo_report = cep::evaluate_ordering_points(repo);
    const auto* repo_j2 = find_result(repo_report, cep::CriterionId::J2_TotalOrderObservability);
    REQUIRE(repo_j2 != nullptr);
    CHECK(repo_j2->verdict == cep::GateVerdict::PASS);
    CHECK(repo_j2->detail.find("vacuous") != std::string::npos);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 4:INV-CEP-08-IC2(E 态静默升级)组合必查项
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP OrderingPoint: IC2 E 态静默升级检查项", "[cep][ordering-point]") {
    const cep::ChannelDeclaration repo = repo_declaration();
    REQUIRE(!repo.empty());

    // 实跑 moesi_gpu:perm 含 UNIQUE + hierarchical 已声明 ⇒ PASS
    const auto report = cep::evaluate_ordering_points(repo);
    CHECK(report.ic2_item_present);
    const auto* ic2 = find_result(report, cep::CriterionId::IC2_ESilentUpgrade);
    REQUIRE(ic2 != nullptr);
    CHECK(ic2->verdict == cep::GateVerdict::PASS);
    CHECK(ic2->detail.find("hierarchical") != std::string::npos);

    // 判据总数恒定 = 4 × 实例数(删掉任何一个判据 ⇒ 用例红)
    CHECK(report.results.size() == 4 * repo.instances.size());
    CHECK(report.has(cep::CriterionId::J1_SinglePoint));
    CHECK(report.has(cep::CriterionId::J2_TotalOrderObservability));
    CHECK(report.has(cep::CriterionId::J3_UniquenessVacuous));
    CHECK(report.has(cep::CriterionId::IC2_ESilentUpgrade));

    // 负向:暴露 E 态但未声明层叠组合 ⇒ FAIL
    cep::DeclaredInstance broken = single_point_instance();
    broken.id = "synthetic_unique_no_hierarchy";
    broken.exposes_unique_perm = true;
    broken.hierarchical = false;
    cep::ChannelDeclaration decl;
    decl.instances.push_back(broken);
    const auto broken_report = cep::evaluate_ordering_points(decl);
    const auto* broken_ic2 = find_result(broken_report, cep::CriterionId::IC2_ESilentUpgrade);
    REQUIRE(broken_ic2 != nullptr);
    CHECK(broken_ic2->verdict == cep::GateVerdict::FAIL);
    CHECK(broken_report.verdict == cep::GateVerdict::FAIL);

    // 正控:同一声明只把 hierarchical 打开 ⇒ PASS
    decl.instances.front().hierarchical = true;
    const auto fixed_report = cep::evaluate_ordering_points(decl);
    const auto* fixed_ic2 = find_result(fixed_report, cep::CriterionId::IC2_ESilentUpgrade);
    REQUIRE(fixed_ic2 != nullptr);
    CHECK(fixed_ic2->verdict == cep::GateVerdict::PASS);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 5:S1-7 (β) 裁决 —— 作为交付物登记
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP OrderingPoint: S1-7 (β) 裁决登记", "[cep][ordering-point]") {
    // 裁决本体
    CHECK(cep::kS1_7BetaVerdict.needs_fifth_perm_bit == false);
    CHECK(std::string(cep::kS1_7BetaVerdict.decision).find("4 值 perm") != std::string::npos);

    // 裁决必须能在**登记文件**里查到(不是只活在头文件里)
    const std::string registered_in = cep::kS1_7BetaVerdict.registered_in;
    REQUIRE(!registered_in.empty());
    const std::string doc = read_registered_doc(registered_in);
    REQUIRE(!doc.empty());
    CHECK(doc.find("S1-7") != std::string::npos);
    CHECK(doc.find("第 5 判别位") != std::string::npos);

    // 4 值 perm 锁的权威来源:冻结 schema 的 perm enum 恰 4 个值
    const std::string schema_text = read_repo_file("include/tlm/cep/protocol_instance.schema.json");
    REQUIRE(!schema_text.empty());
    const auto schema = nlohmann::json::parse(schema_text);
    const auto& perm_enum = schema["properties"]["perm"]["items"]["enum"];
    CHECK(perm_enum.size() == 4);
    bool has_unique = false;
    for (const auto& v : perm_enum) {
        if (v.get<std::string>() == "UNIQUE")
            has_unique = true;
    }
    CHECK(has_unique); // E 态由 UNIQUE 承载 ⇒ 判别位无须新增
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 6:J3 唯一性保持 —— 只做单点 vacuous 机器判据
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP OrderingPoint: J3 vacuous 降级", "[cep][ordering-point]") {
    // 单点 ⇒ vacuous PASS,且必须显式写出 vacuous
    cep::ChannelDeclaration single;
    single.instances.push_back(single_point_instance());
    auto report = cep::evaluate_ordering_points(single);
    const auto* j3 = find_result(report, cep::CriterionId::J3_UniquenessVacuous);
    REQUIRE(j3 != nullptr);
    CHECK(j3->verdict == cep::GateVerdict::PASS);
    CHECK(j3->detail.find("vacuous") != std::string::npos);

    // 无有序点 ⇒ SKIP(「没有可判的对象」≠「通过」)
    cep::DeclaredInstance no_point = single_point_instance();
    no_point.id = "synthetic_no_point";
    no_point.ordering["net_a"][1] = "unordered";
    cep::ChannelDeclaration none;
    none.instances.push_back(no_point);
    report = cep::evaluate_ordering_points(none);
    j3 = find_result(report, cep::CriterionId::J3_UniquenessVacuous);
    REQUIRE(j3 != nullptr);
    CHECK(j3->verdict == cep::GateVerdict::SKIP);

    // 多点(仓库声明有 2 张网)⇒ SKIP + 降级理由(通用形式不可静态判定)
    const cep::ChannelDeclaration repo = repo_declaration();
    REQUIRE(!repo.empty());
    const auto repo_report = cep::evaluate_ordering_points(repo);
    const auto* repo_j3 = find_result(repo_report, cep::CriterionId::J3_UniquenessVacuous);
    REQUIRE(repo_j3 != nullptr);
    CHECK(repo_j3->verdict == cep::GateVerdict::SKIP);
    CHECK(repo_j3->detail.find("决策 7") != std::string::npos);
    CHECK(repo_j3->detail.find("队列 #6") != std::string::npos);
}
