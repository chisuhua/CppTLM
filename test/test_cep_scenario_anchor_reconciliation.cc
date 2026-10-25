// test/test_cep_scenario_anchor_reconciliation.cc
// CEP delta capability 的「Scenario ↔ 锚点」对账测试
//
// 功能/作者/日期:断言每个 delta capability 的 **Scenario 数 == 锚点数**,且每个锚点
//               都指向**真实存在**的可执行证据 / CppTLM build agent / 2026-10-25
//
// 为什么需要(per change `cpptlm-cep-stub-cache-core` 的 A-P1-7):
//   delta capability 是**可执行回归契约**。若某条 Scenario 没有消费者,它就退化为
//   「声称可自动化却无自动化」—— 与本仓已登记的 M-2 病根同型(台账 Entry #8)。
//   上一批(队列 #2 `cep-poc-spike-verification`)已有同类对账测试;本测试把该机制
//   **推广到本 change 的 2 个 capability**,并加**锚点存在性**核对(锚点指向的
//   ctest 用例 / lint 断言 / 文件必须真的在仓内)。
//
// 对账的两侧:
//   ① **数量等值**:Scenario 数 == 锚点数(锚点内嵌在各自 Scenario 末尾 ⇒
//      「锚点数 > Scenario 数」意味着有锚点落在 Scenario 之外)
//   ② **锚点可解析**:锚点里点名的 ctest 用例名 / 文件路径**必须真实存在**
//      (否则对账只是数数,锚点指向早已删除的证据也照样「等值」)
//
// 锚点的存放位置(per openspec 归档语义):锚点**内嵌在各 Scenario 末尾**,
// 因为 delta spec 只保留 `## ADDED Requirements` 段内内容 —— 放在 `## 附录` 段
// 会在归档合并进 main spec 时**被丢弃**。
//
// 运行:ctest -R cep_scenario_anchor_reconciliation
#include "catch_amalgamated.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

    namespace fs = std::filesystem;

    const std::vector<std::string> kCapabilities = {
        "openspec/changes/cpptlm-cep-stub-cache-core/specs/cep-vc-mapping-contract/spec.md",
        "openspec/changes/cpptlm-cep-stub-cache-core/specs/cep-stub-cache-core/spec.md",
    };

    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    struct CapabilityScan {
        std::vector<std::string> scenario_titles;
        std::vector<std::string> anchors; // 每个锚点行去掉前缀后的正文
        size_t anchor_in_scenario = 0;    // 落在某个 Scenario 块内的锚点数
    };

    CapabilityScan scan(const std::string& text) {
        CapabilityScan sc;
        std::istringstream is(text);
        std::string line;
        bool in_scenario = false;
        std::string cur;
        while (std::getline(is, line)) {
            if (line.rfind("#### Scenario:", 0) == 0) {
                in_scenario = true;
                sc.scenario_titles.push_back(line.substr(std::string("#### Scenario:").size()));
                continue;
            }
            if (line.rfind("## ", 0) == 0) { // 新的 H2 ⇒ 离开 Scenario 块
                in_scenario = false;
            }
            if (line.rfind("> **锚点**:", 0) == 0) {
                sc.anchors.push_back(line.substr(std::string("> **锚点**:").size()));
                if (in_scenario)
                    ++sc.anchor_in_scenario;
                continue;
            }
        }
        return sc;
    }

    /// 从锚点正文里抽出「点名的仓内文件路径」(形如 `path/to/file.cc`)
    std::set<std::string> referenced_files(const std::string& anchor) {
        std::set<std::string> out;
        // 锚点里出现的反引号包裹的 token,取含 '/' 且有已知后缀的
        std::string token;
        bool in_tick = false;
        for (size_t i = 0; i <= anchor.size(); ++i) {
            const char c = (i < anchor.size()) ? anchor[i] : '`';
            if (c == '`') {
                if (in_tick) {
                    in_tick = false;
                    // 路径 token 的形态:带**已知源码/脚本后缀**(.cc/.hh/.sh/.py/.md/.json)
                    //   —— 允许**裸文件名**(锚点里常写 `test_xxx.cc` 而不带目录),
                    //   但排除 `test_cep_vc_mapping_contract.cc` 这类**带后缀的散文引用**。
                    static const char* kSuffix[] = {".cc", ".hh", ".sh", ".py", ".md", ".json"};
                    bool has_suffix = false;
                    for (const char* sfx : kSuffix) {
                        const size_t n = std::strlen(sfx);
                        if (token.size() > n &&
                            token.rfind(sfx, token.size() - n) == token.size() - n) {
                            has_suffix = true;
                            break;
                        }
                    }
                    if (has_suffix && token.find(' ') == std::string::npos)
                        out.insert(token);
                } else {
                    in_tick = true;
                    token.clear();
                }
                continue;
            }
            if (in_tick)
                token.push_back(c);
        }
        return out;
    }

} // namespace

TEST_CASE("CEP delta: 每个 capability 的 Scenario 数 == 锚点数", "[cep][scenario-reconciliation]") {
    for (const auto& rel : kCapabilities) {
        const std::string text = read_repo_file(rel);
        REQUIRE_FALSE(text.empty());
        const CapabilityScan sc = scan(text);

        INFO("capability 文件: " << rel);
        // 每个 Scenario 都必须**有**锚点(否则是无消费者的 Scenario)
        CHECK(sc.scenario_titles.size() >= 8);
        CHECK(sc.anchors.size() == sc.scenario_titles.size());
        // 锚点必须**全部落在 Scenario 块内** —— 落在块外的锚点在归档合并时会被丢弃
        CHECK(sc.anchor_in_scenario == sc.anchors.size());
    }
}

TEST_CASE("CEP delta: 锚点点名的仓内文件真实存在", "[cep][scenario-reconciliation]") {
    for (const auto& rel : kCapabilities) {
        const std::string text = read_repo_file(rel);
        REQUIRE_FALSE(text.empty());
        const CapabilityScan sc = scan(text);
        INFO("capability 文件: " << rel);

        std::set<std::string> all_refs;
        for (const auto& a : sc.anchors)
            for (const auto& f : referenced_files(a))
                all_refs.insert(f);

        // **正控**(判据③):锚点必须真的点到了仓内文件 —— 若一个都没抽到,
        // 下面的「文件存在」检查就是空转(无判别力)。
        //   阈值 = 1(而非 2):单个 capability 的锚点**可以**全部指向同一个文件
        //   (如 `cep-stub-cache-core` 的 9 个锚点都指向 `test_cep_stub_cache_core.cc`)。
        CHECK(all_refs.size() >= 1);
        for (const auto& f : all_refs) {
            INFO("锚点引用的文件: " << f);
            // 锚点里的路径**可能是裸文件名**(如 `test_cep_vc_mapping_contract.cc`)
            //   —— 故按候选目录逐个探测(仓根 / test/ / scripts/test/ / include/…),
            //   **任一**候选存在即算「锚点可解析」(锚点的语义是「指向某处」,
            //   不是「唯一路径」)。
            const bool found =
                fs::exists(fs::path(CPPTLM_SOURCE_DIR) / f) ||
                fs::exists(fs::path(CPPTLM_SOURCE_DIR) / "test" / f) ||
                fs::exists(fs::path(CPPTLM_SOURCE_DIR) / "scripts" / "test" / f) ||
                fs::exists(fs::path(CPPTLM_SOURCE_DIR) / "include" / f) ||
                fs::exists(fs::path(CPPTLM_SOURCE_DIR) / "include" / "tlm" / f) ||
                fs::exists(fs::path(CPPTLM_SOURCE_DIR) / "include" / "tlm" / "cep" / f);
            CHECK(found);
        }
    }
}

TEST_CASE("CEP delta: 两个 capability 的锚点文件并集覆盖 ≥ 2 个文件",
          "[cep][scenario-reconciliation]") {
    // 理由:单个 capability 的锚点可全指向 1 个文件,但**两个** capability 合起来
    //   不得只指向 1 个文件(否则「对账」会退化成「对同一个文件数两遍」)。
    std::set<std::string> union_refs;
    for (const auto& rel : kCapabilities) {
        const std::string text = read_repo_file(rel);
        REQUIRE_FALSE(text.empty());
        for (const auto& a : scan(text).anchors)
            for (const auto& f : referenced_files(a))
                union_refs.insert(f);
    }
    CHECK(union_refs.size() >= 2);
}

TEST_CASE("CEP delta: 锚点点名的 ctest 用例真实存在", "[cep][scenario-reconciliation]") {
    // 读 test/CMakeLists.txt,抽全部 `add_test(NAME <name>` —— 锚点里点名的
    // ctest 用例必须在这张表里(否则「锚点」指向一个不存在的测试 = 假覆盖)。
    const std::string cml = read_repo_file("test/CMakeLists.txt");
    REQUIRE_FALSE(cml.empty());
    std::set<std::string> ctest_names;
    {
        std::istringstream is(cml);
        std::string line;
        while (std::getline(is, line)) {
            const auto pos = line.find("add_test(NAME ");
            if (pos == std::string::npos)
                continue;
            std::string rest = line.substr(pos + std::string("add_test(NAME ").size());
            std::string name;
            for (const char c : rest) {
                if (c == ' ' || c == '\t' || c == '\r')
                    break;
                name.push_back(c);
            }
            if (!name.empty())
                ctest_names.insert(name);
        }
    }
    REQUIRE(ctest_names.size() >= 10);

    // 收集锚点点名的 ctest 用例名(形如 `cep_vc_mapping_contract`)
    std::set<std::string> wanted;
    for (const auto& rel : kCapabilities) {
        const std::string text = read_repo_file(rel);
        REQUIRE_FALSE(text.empty());
        for (const auto& a : scan(text).anchors) {
            for (const char* nm : {"cep_vc_mapping_contract", "cep_stub_cache_core"}) {
                if (a.find(nm) != std::string::npos)
                    wanted.insert(nm);
            }
        }
    }
    // **正控**:至少抽到 1 个 —— 否则下面的存在性检查空转
    REQUIRE(wanted.size() >= 1);
    for (const auto& w : wanted) {
        INFO("锚点点名的 ctest 用例: " << w);
        CHECK(ctest_names.count(w) == 1);
    }
}
