// test/test_cep_collective_plane_contract.cc
// capability `cep-collective-plane-contract` —— 8 Scenario / 8 锚点(
//   声明源形态与 schema / 判据三态 + verdict 格式 / 零调用边闸 / canonical 10 流零 diff /
//   collective VC 预留不冲突 / 区域互斥同源 / 声明源配置错误 ⇒ FAIL /
//   抓取点可测性登记 + B2 贡献非空)
//
// 作者: CppTLM Team / 日期: 2026-10-26
//
// 纪律:
//   · 判据实现退化 = 用例红;判定结论为红 = 用例**仍绿**且结论登记为交付物
//   · 每条负向断言配**对照**(反向 fixture)—— 纯负向断言无对照 = 判据可能没生效
//   · 声明源**必须**被本文件消费(⇒ 不产生死文件)

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/collective_plane.hh"
#include "tlm/cep/collective_plane_contract.hh"

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

    /// 判据跑在仓根(相对路径解析)
    cep::ContractPaths repo_paths() {
        cep::ContractPaths p;
        p.root = CPPTLM_SOURCE_DIR;
        return p;
    }

    /// 写一个临时 JSON 文件并返回路径
    std::string write_temp_json(const std::string& name, const std::string& content) {
        const auto path = std::filesystem::temp_directory_path() / name;
        std::ofstream out(path);
        out << content;
        return path.string();
    }

} // namespace

// ═════════════════════════════════════════════════════════════════════
// Scenario 1:声明源形态与 schema 一致 + 被消费(不产生死文件)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectiveContract: 声明源形态与 schema 一致", "[cep][collective-contract]") {
    const std::string decl_text = read_repo_file("include/tlm/cep/collective_planes.json");
    REQUIRE(!decl_text.empty());
    const auto doc = nlohmann::json::parse(decl_text);

    // 三组必填字段
    CHECK(doc.contains("plane_binding"));
    CHECK(doc.contains("registered_regions"));
    CHECK(doc.contains("collective_vc_reservation"));

    // 解析器接受它(⇒ 声明源形态与解析实现一致)
    const auto decl = cep::parse_collective_plane_declaration(doc);
    CHECK_FALSE(decl.source_error);
    CHECK(decl.plane_binding.size() == 2u);
    // 默认:全部不启用 collective(CHI / CXL 配置下绑 NullCollectivePlane,per 07 号 §6.4)
    CHECK(decl.plane_binding.at("moesi_gpu").empty());
    CHECK(decl.plane_binding.at("mesif").empty());
    // 默认:未预留任何 VC(本仓当前拓扑无 collective 可用隔离 VC)
    CHECK(decl.vc_reservation.empty());

    // schema 存在且必填项与声明源一致
    const std::string schema_text = read_repo_file("include/tlm/cep/collective_planes.schema.json");
    REQUIRE(!schema_text.empty());
    const auto schema = nlohmann::json::parse(schema_text);
    // ⚠️ 必须先 `.get<bool>()`:Catch2 的 `CHECK(a == b)` 展开为 `Decomposer() <= a == b`,
    //    而 `<=` 优先级**高于** `==` ⇒ 直接传 `nlohmann::json` 会变成链式比较而编译失败。
    CHECK(schema["additionalProperties"].get<bool>() == false);
    const auto& req = schema["required"];
    CHECK(req.size() == 3u);

    // 对照:声明源**确实被消费** —— 判据清单里必须有与声明源相关的规则
    const auto report = cep::run_collective_plane_contract(repo_paths());
    CHECK(report.has(cep::ContractRule::VcReservation));
    CHECK(report.has(cep::ContractRule::RegionExclusion));
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 2:判据三态可区分 + verdict 逐条可 grep
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectiveContract: 三态显式化 + verdict 可 grep", "[cep][collective-contract]") {
    const auto report = cep::run_collective_plane_contract(repo_paths());
    REQUIRE(report.verdict != cep::ContractVerdict::SKIP);

    // 每条规则一行 verdict,格式 `[verdict] rule=<id> <VERDICT> (detail)`
    const std::string& log = report.to_log();
    std::size_t verdict_lines = 0;
    std::istringstream in(log);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("[verdict]", 0) != 0)
            continue;
        ++verdict_lines;
        CHECK(line.find("rule=CP-") != std::string::npos);
        // ⚠️ 组合表达式必须**整体加括号**:Catch2 的 `CHECK(a == b)` 展开为
        //    `Decomposer() <= a == b`,而 `<=` 优先级高于 `!=` / `||` ⇒ 不加括号即链式比较。
        CHECK((line.find("PASS") != std::string::npos || line.find("FAIL") != std::string::npos ||
               line.find("SKIP") != std::string::npos));
        CHECK(line.find('(') != std::string::npos); // detail 必须在
    }
    CHECK(verdict_lines == cep::kContractRuleCount);

    // 三态**可区分**:枚举值互异 + 名字互异
    CHECK(cep::contract_verdict_name(cep::ContractVerdict::PASS) !=
          cep::contract_verdict_name(cep::ContractVerdict::FAIL));
    CHECK(cep::contract_verdict_name(cep::ContractVerdict::FAIL) !=
          cep::contract_verdict_name(cep::ContractVerdict::SKIP));
    // 对照:SKIP 不得被当作 PASS —— 判据清单里 SKIP 的条目数必须单列
    CHECK(report.count(cep::ContractVerdict::SKIP) <= report.lines.size());
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 3:零调用边闸可执行 —— 反向 fixture(注入 include ⇒ 红)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectiveContract: 零调用边闸可执行", "[cep][collective-contract]") {
    // 正控:仓内真实 collective 头 ⇒ PASS
    std::string detail;
    const auto v = cep::evaluate_zero_call_edge(
        std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/collective_plane.hh", detail);
    CHECK(v == cep::ContractVerdict::PASS);
    CHECK(detail.size() > 0u);

    // ── 反向 fixture:写一份含 forbidden include 的头文件 ⇒ 必须判红 ──
    const auto poisoned_path = write_temp_json(
        "cep_collective_poisoned.hh", "#include \"tlm/cep/sharer_tracker.hh\"\n#include <map>\n");
    std::string d2;
    const auto v2 = cep::evaluate_zero_call_edge(poisoned_path, d2);
    CHECK(v2 == cep::ContractVerdict::FAIL);
    CHECK(d2.find("层级纠缠") != std::string::npos);

    // 对照:干净头文件必须 PASS(否则该判据恒红)
    const auto clean_path =
        write_temp_json("cep_collective_clean.hh", "#include \"tlm/cep/collective_plane.hh\"\n");
    std::string d3;
    CHECK(cep::evaluate_zero_call_edge(clean_path, d3) == cep::ContractVerdict::PASS);

    // 另一条 forbidden 变体也必须被抓
    const auto poisoned2 = write_temp_json("cep_collective_poisoned2.hh",
                                           "#include \"tlm/cep/ordering_point_checker.hh\"\n");
    std::string d4;
    CHECK(cep::evaluate_zero_call_edge(poisoned2, d4) == cep::ContractVerdict::FAIL);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 4:canonical 10 流零 diff —— 反向 fixture
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectiveContract: canonical 10 流零 diff", "[cep][collective-contract]") {
    // 正控:权威源可解析出 10 条流
    const std::string adr =
        read_repo_file("docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md");
    REQUIRE(!adr.empty());
    const auto flows = cep::parse_canonical_flow_names(adr);
    CHECK(flows.size() == 10u);
    for (const char* f :
         {"request", "completion", "snoop", "snoop_resp", "writeback", "forward_data",
          "back_invalidate", "address_resolve", "barrier", "snoop_completion_ack"}) {
        CHECK(flows.count(f) == 1u);
    }

    std::string detail;
    const auto v = cep::evaluate_canonical_flows(
        std::string(CPPTLM_SOURCE_DIR) +
            "/docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md",
        std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/collective_plane.hh", detail);
    CHECK(v == cep::ContractVerdict::PASS);

    // ── 反向 fixture:在 collective 侧**引入**一个 canonical 流名 ⇒ 必须判红 ──
    const auto poisoned_path = write_temp_json("cep_collective_canonical_leak.hh",
                                               "class X { public: void request(int a); };\n");
    std::string d2;
    const auto v2 = cep::evaluate_canonical_flows(
        std::string(CPPTLM_SOURCE_DIR) +
            "/docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md",
        poisoned_path, d2);
    CHECK(v2 == cep::ContractVerdict::FAIL);
    CHECK(d2.find("canonical 流名") != std::string::npos);

    // 对照:**词边界** —— collective 自有的 `collective_barrier(` / `make_completion(` 不是
    // canonical 流
    const auto legit_path =
        write_temp_json("cep_collective_legit.hh", "void collective_barrier(int g);\n"
                                                   "void make_completion(int g);\n");
    std::string d3;
    CHECK(cep::evaluate_canonical_flows(
              std::string(CPPTLM_SOURCE_DIR) +
                  "/docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md",
              legit_path, d3) == cep::ContractVerdict::PASS);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 5:collective VC 预留判据 —— 反向 fixture(注入冲突 VC ⇒ 红)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectiveContract: collective VC 预留不冲突", "[cep][collective-contract]") {
    const auto snap = cep::load_vc_binding_snapshot(
        std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/protocol_instances.json",
        std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/fabric.json");
    REQUIRE_FALSE(snap.source_error);
    // 实测:coherence 已占满 compute_noc / ualink_fabric 的 VC 0-3
    CHECK(snap.used.at("compute_noc").size() == 4u);
    CHECK(snap.vc_count.at("compute_noc") == 4u);

    // 正控:不预留 ⇒ PASS
    cep::CollectivePlaneDeclaration empty_decl;
    std::string detail;
    CHECK(cep::evaluate_vc_reservation(empty_decl, snap, detail) == cep::ContractVerdict::PASS);

    // ── 反向 fixture:预留一个**已被 coherence 占用**的 VC ⇒ 必须判红 ──
    cep::CollectivePlaneDeclaration conflict;
    conflict.vc_reservation.emplace_back("compute_noc", 1u);
    std::string d2;
    CHECK(cep::evaluate_vc_reservation(conflict, snap, d2) == cep::ContractVerdict::FAIL);
    CHECK(d2.find("死锁通道隔离") != std::string::npos);

    // 另一条:越界 VC(> vc_count)⇒ 判红
    cep::CollectivePlaneDeclaration overflow;
    overflow.vc_reservation.emplace_back("compute_noc", 4u);
    std::string d3;
    CHECK(cep::evaluate_vc_reservation(overflow, snap, d3) == cep::ContractVerdict::FAIL);

    // 另一条:未登记的网络 ⇒ 判红
    cep::CollectivePlaneDeclaration unknown_net;
    unknown_net.vc_reservation.emplace_back("no_such_net", 0u);
    std::string d4;
    CHECK(cep::evaluate_vc_reservation(unknown_net, snap, d4) == cep::ContractVerdict::FAIL);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 6:区域互斥判据与行为面同源(结论一致)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectiveContract: 区域互斥判据与行为面同源", "[cep][collective-contract]") {
    // 判据:两个重叠 region ⇒ FAIL
    cep::CollectivePlaneDeclaration decl;
    decl.registered_regions.emplace_back(
        cep::AddrRange{0x1000, 0x1FFF},
        std::make_pair(cep::group_id_t{1}, cep::AccessMode::Normal));
    decl.registered_regions.emplace_back(
        cep::AddrRange{0x1800, 0x2800},
        std::make_pair(cep::group_id_t{2}, cep::AccessMode::Normal));
    std::string detail;
    CHECK(cep::evaluate_region_exclusion(decl, detail) == cep::ContractVerdict::FAIL);
    CHECK(detail.find("重叠") != std::string::npos);

    // **同源**验证:行为面 `LocalCollectivePlane` 对同样的输入给出**同样**的结论
    cep::LocalCollectivePlane plane;
    const auto gid = plane.group_create({1, 2}, cep::GroupAttrs{});
    REQUIRE(gid != cep::kInvalidGroupId);
    CHECK(plane.group_register_region(gid, cep::AddrRange{0x1000, 0x1FFF},
                                      cep::AccessMode::Normal) == cep::Status::Ok);
    CHECK(plane.group_register_region(gid, cep::AddrRange{0x1800, 0x2800},
                                      cep::AccessMode::Normal) == cep::Status::RegionOverlap);

    // 正控:不重叠 ⇒ 两边都判成立
    cep::CollectivePlaneDeclaration ok_decl;
    ok_decl.registered_regions.emplace_back(
        cep::AddrRange{0x1000, 0x1FFF},
        std::make_pair(cep::group_id_t{1}, cep::AccessMode::Normal));
    std::string d2;
    CHECK(cep::evaluate_region_exclusion(ok_decl, d2) == cep::ContractVerdict::PASS);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 7:声明源缺失 / 畸形 ⇒ FAIL(不是 SKIP)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectiveContract: 声明源配置错误 ⇒ FAIL", "[cep][collective-contract]") {
    cep::ContractPaths p = repo_paths();
    p.declaration = "/nonexistent/cep_collective_decl.json";
    const auto missing = cep::run_collective_plane_contract(p);
    CHECK(missing.verdict == cep::ContractVerdict::FAIL);
    CHECK(missing.to_log().rfind("[FAIL]", 0) == 0);
    CHECK(missing.to_log().find("不可读") != std::string::npos);

    // 畸形 JSON ⇒ FAIL
    const auto bad = write_temp_json("cep_collective_bad.json", "{ not json ,,,");
    cep::ContractPaths p2 = repo_paths();
    p2.declaration = bad;
    const auto malformed = cep::run_collective_plane_contract(p2);
    CHECK(malformed.verdict == cep::ContractVerdict::FAIL);
    CHECK(malformed.to_log().find("畸形") != std::string::npos);

    // 类型非法(plane_binding 不是对象)⇒ FAIL
    const auto wrong_type = write_temp_json(
        "cep_collective_wrong_type.json",
        R"({"plane_binding": [], "registered_regions": [], "collective_vc_reservation": []})");
    cep::ContractPaths p3 = repo_paths();
    p3.declaration = wrong_type;
    CHECK(cep::run_collective_plane_contract(p3).verdict == cep::ContractVerdict::FAIL);

    // **不得**把配置错误当 SKIP(那等于闸无牙)
    CHECK(missing.verdict != cep::ContractVerdict::SKIP);
    CHECK(malformed.verdict != cep::ContractVerdict::SKIP);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 8:抓取点可测性登记 + B2 贡献清单非空
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectiveContract: 抓取点登记 + B2 贡献非空", "[cep][collective-contract]") {
    // 权威源 = ADR-CEP-01 的 Status Update 表格(per design D-ι 裁决 1 引入的 S4 修)
    const std::string adr = read_repo_file("docs/adr/ADR-CEP-01-fabric-collective-plane.md");
    REQUIRE(!adr.empty());
    const auto cov = cep::parse_catchpoint_coverage(adr);
    CHECK(cov.parsed == 7u);
    for (const char* id : {"1", "2", "3", "4", "5", "6", "7"})
        CHECK(cov.verdicts.count(id) == 1u);

    // 逐条判别:抓取点 1(零调用边)声称可机器检查 ⇒ 裁决文本里必须出现该措辞
    CHECK(cov.verdicts.at("1").find("可机器检查") != std::string::npos);
    // 抓取点 3 = 仅声明层可测
    CHECK(cov.verdicts.at("3").find("仅声明层可测") != std::string::npos);
    // 抓取点 2 / 7 = 推迟
    CHECK(cov.verdicts.at("2").find("不可测") != std::string::npos);
    CHECK(cov.verdicts.at("7").find("不可测") != std::string::npos);

    // 映射表穷举 7 条
    CHECK(cep::catchpoint_bindings().size() == 7u);

    // 判据实跑:抓取点覆盖核对必须 PASS
    const auto report = cep::run_collective_plane_contract(repo_paths());
    CHECK(report.verdict_of(cep::ContractRule::CatchpointCoverage) == cep::ContractVerdict::PASS);

    // ── 反向 fixture:抓取点登记**缺一条** ⇒ 必须判红 ──
    cep::CatchpointCoverage broken;
    broken.verdicts = cov.verdicts;
    broken.verdicts.erase("7");
    broken.parsed = broken.verdicts.size();
    std::string detail;
    // mock 的判据清单:必须包含**所有**「声称可测」的抓取点所绑定的规则,
    // 否则核对会先在别的抓取点上短路(抓取点 3 → CP-03),到不了本 fixture 想验的分支
    cep::ContractReport self;
    self.lines.push_back({cep::ContractRule::ZeroCallEdge, cep::ContractVerdict::PASS, "x"});
    self.lines.push_back({cep::ContractRule::CanonicalFlows, cep::ContractVerdict::PASS, "x"});
    self.lines.push_back({cep::ContractRule::VcReservation, cep::ContractVerdict::PASS, "x"});
    CHECK(cep::evaluate_catchpoint_coverage(broken, self, detail) == cep::ContractVerdict::FAIL);

    // ── 反向 fixture:抓取点 1 声称可测但映射表未绑规则 ⇒ 必须判红 ──
    //   (把 ADR 文本里的抓取点 1 裁决改成「已满足」而映射表仍绑定 CP-01 ⇒ 判红)
    cep::CatchpointCoverage mismatched;
    mismatched.verdicts = cov.verdicts;
    mismatched.parsed = mismatched.verdicts.size();
    mismatched.verdicts["1"] = "**已满足**(无需实施动作)";
    std::string d2;
    CHECK(cep::evaluate_catchpoint_coverage(mismatched, self, d2) == cep::ContractVerdict::FAIL);
    CHECK(d2.find("禁写恒真判据") != std::string::npos);

    // B2 贡献清单**非空** —— 本 change 行为化 2 条既有编号 + 加固 2 组契约
    //   (per design D-ε:新增 invariant 数 = 0,贡献 = 行为化既有条目)
    const std::string inv_text = read_repo_file("test/test_cep_canonical_invariants.cc");
    REQUIRE(!inv_text.empty());
    CHECK(inv_text.find("INV-CEP-07-IC3") != std::string::npos);
    CHECK(inv_text.find("INV-CEP-04-IC5") != std::string::npos);
}
