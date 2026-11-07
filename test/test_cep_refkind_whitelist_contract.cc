// test/test_cep_refkind_whitelist_contract.cc
// 判据:载体自足性的**唯一白名单**已登记进契约面,且**唯一 + 三处一致**
//      (change `cpptlm-cep-refkind-carrier-whitelist-contract`)。
//
// 覆盖:
//   WLC-01 契约面 `cep-refkind-coverage` 登记「唯一白名单」+ 准入条件 + 路径
//   WLC-02 白名单**唯一**:两处探针的白名单条目集合 == {source_registry.hh}
//   WLC-03 **三处一致**:契约面 + UIS-10 探针 + CP-05 探针指向**同一**路径
//   WLC-04 负控:白名单**多一个**成员 ⇒ 必红(判据有牙)
//
// 作者: builder  日期: 2026-11-07
#include "catch_amalgamated.hpp"

#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

#include "cep_adr_parser.hh" // `cpptlm::cep::repo_root()`

namespace {

    /// 契约面候选路径(main spec 优先;归档前 main spec 尚不存在 ⇒ 用 change 的 delta;
    /// 归档后 delta 随 change 迁入 archive/ ⇒ 三者取第一个存在的)。
    const char* kContractCandidates[] = {
        "openspec/specs/cep-refkind-coverage/spec.md",
        "openspec/changes/cpptlm-cep-refkind-carrier-whitelist-contract/specs/cep-refkind-coverage/"
        "spec.md",
        "openspec/changes/archive/2026-11-07-cpptlm-cep-refkind-carrier-whitelist-contract/specs/"
        "cep-refkind-coverage/spec.md",
    };
    const char* kUialinkProbe = "scripts/test/cep_ualink_inc_state_spec_probe.cc";
    const char* kNvlinkProbe = "scripts/test/cep_nvlink_sharp_state_spec_probe.cc";

    /// 唯一白名单成员(契约面登记的唯一豁免面)
    /// ⚠️ **两种表示**须区分:
    ///   · 探针侧 = C++ 字符串字面量里的**转义**形式  \"tlm/cep/source_registry.hh\"
    ///   · 契约面侧 = markdown 文本里的**普通**形式    "tlm/cep/source_registry.hh"
    const char* kWhitelistPlain = R"("tlm/cep/source_registry.hh")";
    const char* kWhitelistEscaped = R"(\"tlm/cep/source_registry.hh\")";

    std::string read_repo_file(const std::string& rel) {
        const auto path = cpptlm::cep::repo_root() / rel;
        std::ifstream in(path);
        REQUIRE(in.good());
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    bool repo_file_exists(const std::string& rel) {
        std::ifstream in(cpptlm::cep::repo_root() / rel);
        return in.good();
    }

    /// 契约面文本(main spec 或 delta;归档前后皆可解析)
    /// ⚠️ 取「**第一个含白名单登记**」的候选,⛔ **不是**第一个存在的 ——
    ///    归档前 main spec 仍存在但**尚未合并**该登记 ⇒ 若按「存在即取」会读到旧内容。
    std::string read_contract_text() {
        for (const char* cand : kContractCandidates) {
            if (!repo_file_exists(cand))
                continue;
            const auto text = read_repo_file(cand);
            if (text.find(kWhitelistPlain) != std::string::npos)
                return text;
        }
        FAIL("契约面候选路径中**无一**含白名单登记(main spec / delta / 归档后 delta)");
        return std::string();
    }

    /// 扫出**探针**里的**白名单条目**(C++ 字面量中的**转义**引号路径)
    /// ⚠️ **只扫转义形式** —— 探针自身的 `#include "tlm/cep/xxx.hh"` 与待检载体路径
    ///    **不是**白名单条目(前者是探针的依赖,后者是被检对象)⇒ 扫普通形式会**误报**。
    std::set<std::string> probe_whitelist_paths(const std::string& src) {
        static const std::regex re(R"(\\"tlm/cep/[A-Za-z0-9_./-]+\.hh\\")");
        std::set<std::string> out;
        for (std::sregex_iterator it(src.begin(), src.end(), re), end; it != end; ++it) {
            out.insert(it->str());
        }
        return out;
    }

    /// 扫出**契约面文本**里的普通引号路径
    std::set<std::string> contract_cep_paths(const std::string& src) {
        static const std::regex re(R"("tlm/cep/[A-Za-z0-9_./-]+\.hh")");
        std::set<std::string> out;
        for (std::sregex_iterator it(src.begin(), src.end(), re), end; it != end; ++it) {
            out.insert(it->str());
        }
        return out;
    }

} // namespace

TEST_CASE("WLC-01: 契约面登记「唯一白名单」+ 准入条件 + 路径", "[cep][refkind][whitelist]") {
    const auto contract = read_contract_text();
    // 契约面须登记**唯一成员**的完整路径
    REQUIRE(contract.find(kWhitelistPlain) != std::string::npos);
    // 准入条件三条须齐(否则白名单可被随意扩张)
    REQUIRE(contract.find("纯 `constexpr`") != std::string::npos);
    REQUIRE(contract.find("只依赖标准库") != std::string::npos);
    REQUIRE(contract.find("不引入") != std::string::npos);
    // 「单一真源」纪律
    REQUIRE(contract.find("单一真源") != std::string::npos);
    // 探针与主测试的同步义务
    REQUIRE(contract.find("同步") != std::string::npos);
    // RKC-04 scenario 存在
    REQUIRE(contract.find("RKC-04") != std::string::npos);
}

TEST_CASE("WLC-02: 白名单唯一 —— 两处探针的白名单条目集合 == {source_registry.hh}",
          "[cep][refkind][whitelist]") {
    for (const char* probe : {kUialinkProbe, kNvlinkProbe}) {
        INFO("probe=" << probe);
        const auto paths = probe_whitelist_paths(read_repo_file(probe));
        REQUIRE(paths.size() == 1u);
        REQUIRE(paths.count(kWhitelistEscaped) == 1u);
    }
}

TEST_CASE("WLC-03: 三处一致 —— 契约面 + UIS-10 探针 + CP-05 探针指向同一路径",
          "[cep][refkind][whitelist]") {
    const auto contract = read_contract_text();
    const auto ualink = probe_whitelist_paths(read_repo_file(kUialinkProbe));
    const auto nvlink = probe_whitelist_paths(read_repo_file(kNvlinkProbe));

    // 契约面登记的路径 ⟹ 两处探针的白名单成员
    REQUIRE(ualink.count(kWhitelistEscaped) == 1u);
    REQUIRE(nvlink.count(kWhitelistEscaped) == 1u);
    // 契约面文本含该路径(⇒ 三处一致)
    REQUIRE(contract.find(kWhitelistPlain) != std::string::npos);
    // ⛔ 契约面**不得**登记第二个白名单成员(否则「唯一」失守)
    const auto contract_paths = contract_cep_paths(contract);
    REQUIRE(contract_paths.size() == 1u);
    REQUIRE(contract_paths.count(kWhitelistPlain) == 1u);
}

TEST_CASE("WLC-04: 负控 —— 白名单多一个成员 ⇒ 必红(判据有牙)", "[cep][refkind][whitelist]") {
    // 构造一个「多了一个白名单成员」的探针文本 ⇒ 唯一性判据 SHALL 变红
    const std::string polluted =
        R"(\"tlm/cep/source_registry.hh\"
\"tlm/cep/some_other_helper.hh\")";
    const auto paths = probe_whitelist_paths(polluted);
    // ⚠️ 断言**反向**:本测试的判据是「集合 == 1 且含白名单」⇒ 多一个 ⇒ 不满足
    REQUIRE(paths.size() == 2u);                                       // 多出来的那个
    REQUIRE(paths.count(kWhitelistEscaped) == 1u);                     // 白名单仍在
    REQUIRE(paths.count(R"(\"tlm/cep/some_other_helper.hh\")") == 1u); // ⇒ 唯一性判据必红
}
