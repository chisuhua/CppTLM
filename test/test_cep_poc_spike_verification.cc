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
// 运行: ctest -R cep_poc_spike_verification
//   —— 工作目录 = 仓库根(CPPTLM_SOURCE_DIR),spike 二进制按相对/绝对路径调用。

#include "catch_amalgamated.hpp"

#include <array>
#include <cstdio>
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
    };

    RunResult run_spike(const std::string& exe) {
        RunResult r;
        const std::string cmd = std::string(CEP_SPIKE_BIN_DIR) + "/" + exe + " 2>&1";
        FILE* p = popen(cmd.c_str(), "r");
        if (!p)
            return r;
        char buf[4096];
        while (std::fgets(buf, sizeof(buf), p))
            r.out += buf;
        const int status = pclose(p);
        r.rc = (status == -1) ? -1 : (status / 256);
        return r;
    }

    // ── delta spec 路径:active change 优先,否则 main / 归档 ───────────────
    std::string find_delta_spec() {
        const std::array<const char*, 3> candidates{
            "openspec/changes/cpptlm-cep-poc-spike/specs/cep-poc-spike-verification/spec.md",
            "openspec/specs/cep-poc-spike-verification/spec.md",
            "openspec/changes/archive/2026-10-23-cpptlm-cep-poc-spike/specs/"
            "cep-poc-spike-verification/spec.md",
        };
        for (const char* c : candidates) {
            if (!read_repo_file(c).empty())
                return c;
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

    // ── 证据映射表:Scenario 标题(子串匹配)→ 证据检查器 ────────────────
    //   每条 delta Scenario **必须**在此表里有一行(否则本测试失败)。
    //   反之,表里的每行**必须**匹配到一条真实 Scenario(否则算陈旧行 → 失败)。
    struct EvidenceRow {
        const char* scenario_substr; // Scenario 标题的子串
        const char* kind;            // "s1" / "s2" / "s3" / "file:<relpath>"
        const char* needle;          // 该证据源里必须出现(或必须**不**出现)的串
        const char* mode;            // "present"(默认) / "absent"
    };

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
        // ④ S2
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
        // ⑥ 每场景的反向验证不得被删除
        {"每场景的反向验证不得被删除", "s1", "S1_SELFCHECK=OK", "present"},
        {"每场景的反向验证不得被删除", "s2", "S2_SELFCHECK=OK", "present"},
        {"每场景的反向验证不得被删除", "s3", "S3_SELFCHECK=OK", "present"},
        // ⑦ 放行禁令
        {"用作协议实例的放行依据", "s3", "S3_RELEASE_USE=forbidden", "present"},
        {"用作协议实例的放行依据",
         "file:docs/adr/ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md",
         "用本 ADR 的无环判据放行任何协议实例", "present"},
        // ⑧ status: planned(防虚假触发 S1-2)—— 证据是「**不**出现 implemented 状态」
        {"spike 内引用的协议实例声明", "file:examples/cep_poc_spike_s1.cc",
         "\"status\": \"implemented\"", "absent"},
        {"spike 内引用的协议实例声明", "file:examples/cep_poc_spike_s2.cc",
         "\"status\": \"implemented\"", "absent"},
        {"spike 内引用的协议实例声明", "file:examples/cep_poc_spike_s3.cc",
         "\"status\": \"implemented\"", "absent"},
        // ⑨ 硬边界(可丢弃 / 零公共影响)
        {"spike 的硬边界", "file:examples/cep_poc_spike_s1.cc", "不进 ModuleFactory", "present"},
        {"spike 的硬边界", "file:examples/cep_poc_spike_s2.cc", "不进 ModuleFactory", "present"},
        {"spike 的硬边界", "file:examples/cep_poc_spike_s3.cc", "不进 ModuleFactory", "present"},
    };

} // namespace

TEST_CASE("[cep][poc-spike][verification] delta spec 的每条 Scenario 都有可执行断言",
          "[cep][poc-spike][verification]") {
    // ① 定位 delta spec(active change 优先,否则 main/归档)
    const std::string spec_path = find_delta_spec();
    REQUIRE_FALSE(spec_path.empty());
    const std::string spec_text = read_repo_file(spec_path);
    REQUIRE_FALSE(spec_text.empty());

    // ② 抽出 Scenario 标题
    const auto titles = scenario_titles(spec_text);
    REQUIRE(titles.size() >= 10); // 3 场景 × (正向 + 反向) + 语义分离 + 边界等

    // ③ 跑 3 个 spike 一次
    const RunResult s1 = run_spike("cep_poc_spike_s1");
    const RunResult s2 = run_spike("cep_poc_spike_s2");
    const RunResult s3 = run_spike("cep_poc_spike_s3");
    INFO("S1 rc=" << s1.rc << " S2 rc=" << s2.rc << " S3 rc=" << s3.rc);
    // exit 0 = 判据工作正常(判定结论可能是红 —— per design.md D7-a)
    REQUIRE(s1.rc == 0);
    REQUIRE(s2.rc == 0);
    REQUIRE(s3.rc == 0);

    // ④ 每条 Scenario 都必须在映射表里有行
    for (const auto& t : titles) {
        bool covered = false;
        for (const auto& row : kEvidence) {
            if (t.find(row.scenario_substr) != std::string::npos) {
                covered = true;
                break;
            }
        }
        INFO("未覆盖的 Scenario: " << t);
        REQUIRE(covered);
    }

    // ⑤ 映射表里的每行都**必须**匹配到一条真实 Scenario(防陈旧行)
    for (const auto& row : kEvidence) {
        bool matched = false;
        for (const auto& t : titles) {
            if (t.find(row.scenario_substr) != std::string::npos) {
                matched = true;
                break;
            }
        }
        INFO("映射表行未匹配任何 Scenario: " << row.scenario_substr);
        REQUIRE(matched);
    }

    // ⑥ 每条映射行的证据**必须**真实存在
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
            REQUIRE_FALSE(haystack.empty());
        } else {
            FAIL("未知的证据类型: " << kind);
        }
        // ⚠️ `mode` 若为 nullptr(聚合初始化漏字段)→ 视为 "present";
        //    直接 std::string(nullptr) 会抛异常,掩盖真正的证据缺失。
        const std::string mode = row.mode ? std::string(row.mode) : std::string("present");
        const bool absent = mode == "absent";
        const bool present = mode == "present";
        REQUIRE((present || absent));
        if (absent) {
            INFO("证据**不应**出现: " << row.needle << " @ " << kind);
            REQUIRE(haystack.find(row.needle) == std::string::npos);
        } else {
            INFO("证据缺失: " << row.needle << " @ " << kind);
            REQUIRE(haystack.find(row.needle) != std::string::npos);
        }
    }

    // ⑦ spike 硬边界的**可执行**核查:源码里不得出现注册宏 / implemented 状态
    for (const char* src : {"examples/cep_poc_spike_s1.cc", "examples/cep_poc_spike_s2.cc",
                            "examples/cep_poc_spike_s3.cc"}) {
        const std::string text = read_repo_file(src);
        REQUIRE_FALSE(text.empty());
        INFO("违反硬边界的文件: " << src);
        REQUIRE(text.find("REGISTER_MODULE") == std::string::npos);
        REQUIRE(text.find("REGISTER_CHSTREAM") == std::string::npos);
        REQUIRE(text.find("registerModule") == std::string::npos);
        // status: implemented 不得出现(否则 S1-2 会被虚假触发)
        REQUIRE(text.find("\"status\": \"implemented\"") == std::string::npos);
    }
}
