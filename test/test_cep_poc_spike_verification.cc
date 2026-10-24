// test/test_cep_poc_spike_verification.cc
//
// CEP PoC Spike —— delta capability「每条 Scenario 都有可执行断言」的对账测试
//
// 功能/作者/日期:把 `cep-poc-spike-verification` 的**每条 Scenario** 对上**可执行证据**
//               / CppTLM build agent / 2026-10-24
//
// 为什么需要(per openspec/changes/cpptlm-cep-poc-spike/tasks.md T5.9):
//   delta capability 是**可执行回归契约**,若某条 Scenario 没有消费者,它就退化为
//   「声称可自动化却无自动化」—— 与本仓已登记的 M-2 病根同型(台账 Entry #8)。
//   本测试把「Scenario → 证据」做成**显式映射表**并**逐条断言**证据存在;
//   同时断言映射表**没有**指向已不存在的 Scenario(防陈旧行)。
//
// 证据的两种形态:
//   ① spike 的**机器可读输出**(S1/S2/S3 的 [x][VERDICT]/[x][SELFCHECK] 行)
//   ② 仓内**文件内容**(如 examples/CMakeLists.txt 的 ctest 注册、ADR 的放行禁令原文)
//
// 反自伤纪律(critic 2026-10-24 复审后加固,per 台账 Entry #49):
//   · `mode` 字段缺失 SHALL **判失败**(此前静默按 present → 会让漏字段的 absent 行静默失效)
//   · 枚举循环用 CHECK(累积)而非 REQUIRE(首败即止会**掩盖**后续 Scenario 的覆盖缺口)
//   · `ctest -R cep_poc_spike` 的命中数 SHALL **恰好 3**(此前验证用例名撞前缀 → 实为 4)
//
// 运行: ctest -R cep_spike_delta_verification
//   —— 工作目录 = 仓库根(CPPTLM_SOURCE_DIR),spike 二进制按绝对路径调用
//      (`CEP_SPIKE_BIN_DIR` 由 test/CMakeLists.txt 注入)。

#include "catch_amalgamated.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

    // ── 读仓内文件(相对 CPPTLM_SOURCE_DIR)───────────────────────────────
    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    // ── 跑一个 spike,收集 stdout + 退出码 ───────────────────────────────
    struct RunResult {
        int rc = -1;
        std::string out;
        bool spawned = false;
    };

    RunResult run_spike(const std::string& exe) {
        RunResult r;
        const std::string cmd = std::string(CEP_SPIKE_BIN_DIR) + "/" + exe + " 2>&1";
        FILE* p = popen(cmd.c_str(), "r");
        if (!p)
            return r;
        r.spawned = true;
        char buf[4096];
        while (std::fgets(buf, sizeof(buf), p))
            r.out += buf;
        const int status = pclose(p);
        r.rc = (status == -1) ? -1 : (status / 256);
        return r;
    }

    // ── delta spec 路径:active change 优先 → main specs → 归档(动态发现)──────
    //   归档目录名带日期前缀(`<YYYY-MM-DD>-<change>`),日期不可硬编码
    //   (critic NIT:曾写死 `2026-10-23-...`,归档后必落空)。
    std::string find_delta_spec() {
        const std::vector<std::string> direct{
            "openspec/changes/cpptlm-cep-poc-spike/specs/cep-poc-spike-verification/spec.md",
            "openspec/specs/cep-poc-spike-verification/spec.md",
        };
        for (const auto& c : direct) {
            if (!read_repo_file(c).empty())
                return c;
        }
        // 归档兜底:扫 archive/ 下所有 `<date>-cpptlm-cep-poc-spike` 目录
        namespace fs = std::filesystem;
        const fs::path arch = fs::path(CPPTLM_SOURCE_DIR) / "openspec/changes/archive";
        std::error_code ec;
        if (fs::is_directory(arch, ec)) {
            for (const auto& e : fs::directory_iterator(arch, ec)) {
                if (!e.is_directory())
                    continue;
                const std::string name = e.path().filename().string();
                if (name.find("cpptlm-cep-poc-spike") == std::string::npos)
                    continue;
                const fs::path p = e.path() / "specs/cep-poc-spike-verification/spec.md";
                std::error_code ec2;
                if (fs::exists(p, ec2)) {
                    std::error_code ec3;
                    const fs::path rel = fs::relative(p, fs::path(CPPTLM_SOURCE_DIR), ec3);
                    if (!ec3)
                        return rel.string();
                }
            }
        }
        return {};
    }

    // ── 抽出 spec 里的 Scenario 标题 ─────────────────────────────────────
    std::vector<std::string> scenario_titles(const std::string& spec_text) {
        std::vector<std::string> out;
        std::istringstream in(spec_text);
        std::string line;
        while (std::getline(in, line)) {
            const std::string marker = "#### Scenario:";
            if (line.rfind(marker, 0) == 0) {
                std::string t = line.substr(marker.size());
                while (!t.empty() && (t.front() == ' '))
                    t.erase(t.begin());
                out.push_back(t);
            }
        }
        return out;
    }

    // ── 数**真实 ctest 注册面**里 name **含** `prefix` 的条数(critic BLOCKER-2/四审)──
    //   早前实现有二跳错误:
    //   ① 用源码正则代理 → 与 `ctest -R` **语义不等价**;
    //   ② 改读 `CTestTestfile.cmake` 后仍用**前缀锚定** `add_test(<prefix>`,
    //      而 `ctest -R <p>` 是**非锚定正则**(匹配 NAME 的任意子串)⇒ `test_cep_poc_spike_extra`
    //      这类「含前缀但不以之开头」的新用例会**静默逃逸**(实测:真实命中 4 / 守卫仍 3)。
    //   ⇒ 抽 `add_test(<name> ...)` 的 `<name>`(剥引号),再 `name.find(prefix) != npos`。
    std::size_t count_registered_tests(const std::string& prefix) {
        namespace fs = std::filesystem;
        fs::path build_dir = fs::path(CEP_SPIKE_BIN_DIR);
        build_dir = build_dir.parent_path().parent_path(); // bin/examples → <build>
        std::size_t n = 0;
        std::error_code ec;
        if (!fs::is_directory(build_dir, ec))
            return 0; // 调用方据 0 报红
        for (const auto& e : fs::recursive_directory_iterator(build_dir, ec)) {
            if (!e.is_regular_file())
                continue;
            if (e.path().filename() != "CTestTestfile.cmake")
                continue;
            std::ifstream in(e.path());
            if (!in)
                continue;
            std::ostringstream ss;
            ss << in.rdbuf();
            const std::string t = ss.str();
            // CTestTestfile 里的形态:`add_test(<name> "<cmd>")` —— 抽第一个 token
            const std::string marker = "add_test(";
            for (std::size_t pos = t.find(marker); pos != std::string::npos;
                 pos = t.find(marker, pos + marker.size())) {
                std::size_t i = pos + marker.size();
                while (i < t.size() && t[i] == ' ')
                    ++i;
                // name 可能被引号包裹
                if (i < t.size() && t[i] == '"')
                    ++i;
                std::string name;
                while (i < t.size() && t[i] != ' ' && t[i] != '"' && t[i] != ')')
                    name.push_back(t[i++]);
                // ⚠️ 非锚定:与 `ctest -R <prefix>` 的正则语义一致
                if (name.find(prefix) != std::string::npos)
                    ++n;
            }
        }
        return n;
    }

    // ── **结构化**子串检查(critic S-2 + 四审 SF-2 + 六审 B1)──
    //   目标:捕获 spike 源里的 `protocol_instance` 声明键 `status`(**含各种字面量形态**)。
    //   做法:归一化时剥掉**空白 + 引号 + 反斜杠**(六审 B1:C++ 里 JSON 声明必然写成
    //   `"{\"status\":\"implemented\"}"` —— 转义引号;不剥反斜杠则归一化得
    //   `{\status\:...}`,键位形态失配 → **两层同时漏判**)。
    //   剥反斜杠后,转义形态归一化为 `{status:`,与 raw string 形态一致。
    //   再要求出现 **JSON 键位形态** `{status:` 或 `,status:`(而非裸 `status` 子串
    //   —— 后者会把注释 / 标识符也判红,四审 SF-2 的误报面)。
    //   ⚠️ 已知边界(不防蓄意):`\x73tatus` 转义 / 运行时逐字符构造。
    bool contains_instance_status_key(const std::string& text) {
        std::string norm;
        norm.reserve(text.size());
        for (char c : text) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '"' || c == '\'' ||
                c == '\\')
                continue;
            norm.push_back(c);
        }
        return norm.find("{status:") != std::string::npos ||
               norm.find(",status:") != std::string::npos;
    }

    // ── (已移除)二进制级 `status` 扫描(critic 四审 SHOULD_FIX-2)──
    //   它扫**整个二进制**(含静态链接的 `cpptlm_core.a`):未来 core 任何一处出现
    //   该子串都会让三份 spike **无理由变红**,且现场难定位;而它依赖「字面量被实际
    //   使用」(-O2 会消除未使用的 static 变量),本就**非密封**。
    //   ⇒ 主判据保留在 ⑧ 的**结构化源码检查**(`{status:` / `,status:` 键位形态)。

    // ── 证据映射表:Scenario 标题(子串匹配)→ 证据检查器 ────────────────
    //   每条 delta Scenario **必须**在此表里有一行(否则本测试失败)。
    //   反之,表里的每行**必须**匹配到一条真实 Scenario(否则算陈旧行 → 失败)。
    struct EvidenceRow {
        const char* scenario_substr; // Scenario 标题的子串
        const char* kind;            // "s1" / "s2" / "s3" / "file:<relpath>"
        const char* needle;          // 该证据源里必须出现(或必须**不**出现)的串
        const char* mode;            // "present" / "absent"(**必填**,缺失即失败)
    };

    // delta spec 的 Scenario 条数(防「Scenario 被静默删除」;新增须显式上调 —— 刻意摩擦)
    constexpr std::size_t kExpectedScenarioCount = 15; // 14(初版)+ 1(C2 有效期登记)

    const std::vector<EvidenceRow> kEvidence{
        // ① 3 个场景各自有唯一判据且全部注册为 ctest
        {"3 个场景各自有唯一判据且全部注册为 ctest", "file:examples/CMakeLists.txt",
         "add_test(NAME cep_poc_spike_s1", "present"},
        {"3 个场景各自有唯一判据且全部注册为 ctest", "file:examples/CMakeLists.txt",
         "add_test(NAME cep_poc_spike_s2", "present"},
        {"3 个场景各自有唯一判据且全部注册为 ctest", "file:examples/CMakeLists.txt",
         "add_test(NAME cep_poc_spike_s3", "present"},
        // ② 红绿语义分离
        {"判定结论为红 与 判据实现退化 语义分离", "s1", "S1_VERDICT=FAIL", "present"},
        {"判定结论为红 与 判据实现退化 语义分离", "s1", "S1_SELFCHECK=OK", "present"},
        // ③ S1
        {"S1 —— 单域基线", "s1", "S1_VERDICT=FAIL", "present"},
        {"S1 —— 单域基线", "s1", "S1_R2=TRIGGERED(state_transition[2][1]=3)", "present"},
        {"S1 —— 单域基线", "s1", "S1_WRITEBACK_OBSERVED=1", "present"},
        {"S1 的反向验证", "s1", "S1_SELFCHECK=OK", "present"},
        // ④ S2 —— **含 NEG 实验真实性的锚**(critic B1:仅锁 `S2_SELFCHECK=OK` 时,
        //     整段 NEG 实验可被删除而不被发现 → 追加 `S2_NEG_REAL_FALLBACK=1`)
        {"S2 —— 跨域可见写", "s2", "S2_SELFCHECK=OK", "present"},
        {"S2 —— 跨域可见写", "s2", "判据 (b) 声明层裁定", "present"},
        // ⑤ S3
        {"S3 —— 死锁自由", "s3", "S3_VERDICT=ACYCLIC", "present"},
        {"S3 的顶点集与依赖边集", "s3", "S3_VERTICES=8", "present"},
        {"S3 的顶点集与依赖边集", "s3", "S3_EDGES=8", "present"},
        {"S3 的顶点集与依赖边集", "s3", "S3_CROSSNET_EDGES=0", "present"},
        {"S3 降级后报环的", "s3", "S3_CAUSE=alias-false-cycle", "present"},
        {"S3 降级后报环的", "s3", "S3_DOWNGRADED_CYCLE=vc2<->vc3", "present"},
        {"S3 反向断言的有效期", "s3", "S3_REVERSE_VALIDITY=until-3rd-network-appears", "present"},
        {"S3 的反向验证", "s3", "S3_SELFCHECK=OK", "present"},
        // ⑥ 每场景的反向验证不得被删除 —— **锚在「反向实验真实发生」的机器证据上**
        //    (critic B1:锚 `SELFCHECK=OK` 字符串时,删块后该串仍在 → 无判别力)
        {"每场景的反向验证不得被删除", "s1", "S1_SELFCHECK=OK", "present"},
        {"每场景的反向验证不得被删除", "s2", "S2_NEG_REAL_FALLBACK=1", "present"},
        {"每场景的反向验证不得被删除", "s2", "S2_NEG_FALLBACK_HITS=1", "present"},
        {"每场景的反向验证不得被删除", "s3", "S3_SELFCHECK=OK", "present"},
        // ⑥b **边敏感度**(critic 复审 BLOCKER-1):检测器须**看边** ——
        //     删掉降级图里构成 2↔3 环的一条边后必须改判无环,否则硬编码检测器也能过
        {"S3 的反向验证", "s3", "S3_EDGE_SENSITIVE=1", "present"},
        {"S3 的反向验证", "s3", "S3_CYCLE_CLOSED=1", "present"},
        // ⑥c **注入判别力**(critic 三轮 BLOCKER-1 + 四审 SHOULD_FIX-1 + 五审 SF-B):
        //     须同时检出 2 元环 / 3 元环 / 4 元环+隔离弱连通分量,且**注入集自身**的
        //     环长与分量数须自守(否则 fixture 被侵蚀时保护静默消失)
        {"S3 的反向验证", "s3", "注入检出(2元环=1 / 3元环=1 / 4元环+隔离分量=1)", "present"},
        {"S3 的反向验证", "s3", "注入集自守: 环长=(2,3,4) 分量数=2", "present"},
        // ⑦ 放行禁令
        {"用作协议实例的放行依据", "s3", "S3_RELEASE_USE=forbidden", "present"},
        {"用作协议实例的放行依据",
         "file:docs/adr/ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md",
         "用本 ADR 的无环判据放行任何协议实例", "present"},
        // ⑧ spike SHALL NOT 引入协议实例声明(防虚假触发 S1-2)——
        //    证据 = 三份 spike 源**不含 `status` 键位形态**(含字面量拼接变体;
        //    四审 SHOULD_FIX-1:改结构化检查后不再误报注释/标识符)
        // ⑧ spike SHALL NOT 引入协议实例声明(防虚假触发 S1-2)——
        //    证据 = 原始文本不含 JSON 键 `"status":`;拼接变体由 section ⑧ 的
        //    结构化归一化检查(`contains_instance_status_key`)捕获(两层互补)
        {"spike SHALL NOT 引入协议实例声明", "file:examples/cep_poc_spike_s1.cc",
         "\"status\":", "absent"},
        {"spike SHALL NOT 引入协议实例声明", "file:examples/cep_poc_spike_s2.cc",
         "\"status\":", "absent"},
        {"spike SHALL NOT 引入协议实例声明", "file:examples/cep_poc_spike_s3.cc",
         "\"status\":", "absent"},
        // ⑨ S1 当前判定结论 + 有效期登记(C2)
        {"S1 当前判定结论", "s1", "S1_VERDICT=FAIL", "present"},
        {"S1 当前判定结论", "file:examples/cep_poc_spike_s1.cc",
         "S1_VALIDITY_UNTIL=queue6-perm-schema-evolution", "present"},
        // ⑩ 硬边界(可丢弃 / 零公共影响)
        {"spike 的硬边界", "file:examples/cep_poc_spike_s1.cc", "不进 ModuleFactory", "present"},
        {"spike 的硬边界", "file:examples/cep_poc_spike_s2.cc", "不进 ModuleFactory", "present"},
        {"spike 的硬边界", "file:examples/cep_poc_spike_s3.cc", "不进 ModuleFactory", "present"},
    };

} // namespace

TEST_CASE("[cep][poc-spike][verification] delta spec 的每条 Scenario 都有可执行断言",
          "[cep][poc-spike][verification]") {
    // ① 定位 delta spec(active change → main specs → 归档动态发现)
    const std::string spec_path = find_delta_spec();
    REQUIRE_FALSE(spec_path.empty());
    const std::string spec_text = read_repo_file(spec_path);
    REQUIRE_FALSE(spec_text.empty());
    INFO("delta spec = " << spec_path);

    // ② 抽出 Scenario 标题 —— 条数须**恰好**等于登记值(防静默删除/新增未登记)
    const auto titles = scenario_titles(spec_text);
    CHECK(titles.size() == kExpectedScenarioCount);
    INFO("实测 Scenario 条数 = " << titles.size() << " / 登记值 = " << kExpectedScenarioCount);

    // ③ 跑 3 个 spike 一次
    const RunResult s1 = run_spike("cep_poc_spike_s1");
    const RunResult s2 = run_spike("cep_poc_spike_s2");
    const RunResult s3 = run_spike("cep_poc_spike_s3");
    INFO("S1 rc=" << s1.rc << " spawned=" << s1.spawned << " | S2 rc=" << s2.rc
                  << " | S3 rc=" << s3.rc);
    // exit 0 = 判据工作正常(判定结论可能是红 —— per design.md D7-a)
    REQUIRE(s1.spawned);
    REQUIRE(s2.spawned);
    REQUIRE(s3.spawned);
    REQUIRE(s1.rc == 0);
    REQUIRE(s2.rc == 0);
    REQUIRE(s3.rc == 0);

    // ④ `ctest -R cep_poc_spike` 的命中数须**恰好 3**(critic B3 + BLOCKER-2)——
    //    以 **CMake 生成的 CTestTestfile.cmake**(ctest 的真实输入)为据,
    //    不用源码正则(与 `ctest -R` 语义不等价)
    {
        const std::size_t n = count_registered_tests("cep_poc_spike");
        INFO("CTestTestfile 里 `add_test(cep_poc_spike*` 条数 = " << n);
        CHECK(n == 3);
        // 验证用例仍须注册(只是名字不带该前缀)
        CHECK(count_registered_tests("cep_spike_delta_verification") == 1);
    }

    // ⑤ 每条 Scenario 都必须在映射表里有行(累积报告,不首败即止)
    for (const auto& t : titles) {
        bool covered = false;
        for (const auto& row : kEvidence) {
            if (t.find(row.scenario_substr) != std::string::npos) {
                covered = true;
                break;
            }
        }
        INFO("未覆盖的 Scenario: " << t);
        CHECK(covered);
    }

    // ⑥ 映射表里的每行都**必须**匹配到一条真实 Scenario(防陈旧行)
    for (const auto& row : kEvidence) {
        bool matched = false;
        for (const auto& t : titles) {
            if (t.find(row.scenario_substr) != std::string::npos) {
                matched = true;
                break;
            }
        }
        INFO("映射表行未匹配任何 Scenario: " << row.scenario_substr);
        CHECK(matched);
    }

    // ⑦ 每条映射行的证据**必须**真实存在
    for (const auto& row : kEvidence) {
        const std::string kind(row.kind);
        std::string haystack;
        if (kind == "s1")
            haystack = s1.out;
        else if (kind == "s2")
            haystack = s2.out;
        else if (kind == "s3")
            haystack = s3.out;
        else if (kind.rfind("file:", 0) == 0) {
            haystack = read_repo_file(kind.substr(5));
            INFO("证据文件缺失: " << kind.substr(5));
            REQUIRE_FALSE(haystack.empty());
        } else {
            FAIL("未知的证据类型: " << kind);
            continue;
        }
        // ⚠️ `mode` **必填** —— 缺失即失败(critic SHOULD_FIX:此前静默按 present,
        //    会让漏字段的 absent 行**静默失效**,恰是它本应拦截的方向)
        if (row.mode == nullptr) {
            FAIL("EvidenceRow.mode 缺失(必填 present/absent): " << row.scenario_substr);
            continue;
        }
        const std::string mode(row.mode);
        if (mode == "absent") {
            INFO("证据**不应**出现: " << row.needle << " @ " << kind);
            CHECK(haystack.find(row.needle) == std::string::npos);
        } else if (mode == "present") {
            INFO("证据缺失: " << row.needle << " @ " << kind);
            CHECK(haystack.find(row.needle) != std::string::npos);
        } else {
            FAIL("未知 mode: " << mode << "(须 present/absent)");
        }
    }

    // ⑧ spike 硬边界的**可执行**核查:源码里不得出现注册宏 / 实例声明
    for (const char* src : {"examples/cep_poc_spike_s1.cc", "examples/cep_poc_spike_s2.cc",
                            "examples/cep_poc_spike_s3.cc"}) {
        const std::string text = read_repo_file(src);
        REQUIRE_FALSE(text.empty());
        INFO("违反硬边界的文件: " << src);
        CHECK(text.find("REGISTER_MODULE") == std::string::npos);
        CHECK(text.find("REGISTER_CHSTREAM") == std::string::npos);
        CHECK(text.find("registerModule") == std::string::npos);
        // 协议实例声明键不得出现(否则 S1-2 会被虚假触发)。
        //   ⚠️ 用**结构化 + 归一化**检查(四审 SF-2):裸 `status` 子串会把
        //      注释 / 标识符也判红(误报面);键位形态 `{status:` / `,status:`
        //      在剥掉空白/引号/**反斜杠**后判定(六审 B1:转义形态必需)。
        CHECK_FALSE(contains_instance_status_key(text));
    }

    // ⑨ **正控**(critic 六审 B1):上面的 `CHECK_FALSE(contains_instance_status_key(...))`
    //    是**纯负向**断言 —— 若 helper 退化为恒假(`return false;`),所有文件都「不含声明键」
    //    → 全绿,而保护已消失。故此处断言 helper 对**阳性输入**必须返回 true。
    //    形态覆盖(六审 B1 的核心):C++ 里真实 JSON 声明**必然**是**转义引号**形态,
    //    以及 raw string 形态、字面量拼接形态 —— 三者都须被捕获。
    {
        const std::string raw_decl = "static const char* s = R\"({\"status\":\"implemented\"})\";";
        const std::string esc_decl =
            "static const char* s = \"{\\\"status\\\":\\\"implemented\\\"}\";";
        const std::string cat_decl = "static const char* s = \"{\\\"sta\" \"tus\\\":1}\";";
        // 负控:注释 / 标识符里的裸 `status` 不得误报(四审 SF-2)
        const std::string comment_neg = "// no protocol_instance status field here\n";
        const std::string ident_neg = "const char* inst_status_tag = \"x\";";
        INFO("正控: raw=" << contains_instance_status_key(raw_decl)
                          << " escaped=" << contains_instance_status_key(esc_decl)
                          << " concat=" << contains_instance_status_key(cat_decl));
        CHECK(contains_instance_status_key(raw_decl));
        CHECK(contains_instance_status_key(esc_decl)); // ← 六审 B1:此形态曾漏判
        CHECK(contains_instance_status_key(cat_decl));
        CHECK_FALSE(contains_instance_status_key(comment_neg));
        CHECK_FALSE(contains_instance_status_key(ident_neg));
    }
}
