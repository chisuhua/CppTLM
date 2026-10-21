// test/test_cep_tools_contract.cc
// CEP Stage 0 — tools 契约测试
// per openspec/changes/cpptlm-cep-umbrella/design.md D4(零项目内依赖)
//     + D10(依赖方向单向)
//     + ADR-CEP-06 §4(4 件套 / 13 metric)
// 作者: CppTLM Team / 日期: 2026-10-21
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    const std::vector<std::string>& tool_files() {
        static const std::vector<std::string> t = {"tools/render_report.py", "tools/run_sweep.py",
                                                   "tools/ab_compare.py"};
        return t;
    }

    // 真执行一条命令,返回退出码(输出丢弃)。-1 = 无法启动。
    //
    // ⚠️ 2026-10-21 代码复审 B5:此前 tools 契约测试**全是字符串 grep**,
    //    一行 Python 都没执行过 —— 往 render_report.py 里塞一个语法错误,
    //    8 个 cep_* ctest **依然全绿**。故本文件改为**行为断言**。
    int run_cmd(const std::string& cmd) {
        const std::string full = cmd + " > /dev/null 2>&1";
        FILE* p = popen(full.c_str(), "r");
        if (!p)
            return -1;
        int rc = pclose(p);
        if (rc == -1)
            return -1;
        // pclose 返回的是 wait 状态;统一折算成退出码
        if (rc & 0x7f)
            return 128 + (rc & 0x7f);
        return (rc >> 8) & 0xff;
    }

    std::string shquote(const std::string& s) {
        return "'" + s + "'";
    }

    // 造一个实验目录。metrics 覆盖传入的字段。
    std::filesystem::path make_exp_dir(const std::string& tag, const std::string& metrics_json,
                                       bool with_trace = true) {
        const auto base = std::filesystem::temp_directory_path() / ("cep_tools_" + tag);
        std::filesystem::remove_all(base);
        std::filesystem::create_directories(base);
        std::ofstream(base / "metrics.json") << metrics_json;
        std::ofstream(base / "simulation_config.json")
            << R"({"simulation_mode":"timing","verification_mode":"exhaustive","verification_scope":"unit"})";
        if (with_trace) {
            std::ofstream(base / "trace.h5") << "stub";
        }
        return base;
    }

    const char* kGoodMetrics = R"({
 "total_cycles":100,"total_transactions":10,"transactions_per_cycle":0.1,
 "avg_latency_cycles":1.0,"p50_latency_cycles":1.0,"p95_latency_cycles":1.0,
 "p99_latency_cycles":1.0,"max_latency_cycles":1.0,"coherence_stalls":0,
 "collective_stalls":0,"directory_misses":0,"deadlock_violations":0,"memory_violations":0})";

    const char* kBadMetrics = R"({
 "total_cycles":100,"total_transactions":10,"transactions_per_cycle":0.1,
 "avg_latency_cycles":1.0,"p50_latency_cycles":1.0,"p95_latency_cycles":1.0,
 "p99_latency_cycles":1.0,"max_latency_cycles":1.0,"coherence_stalls":0,
 "collective_stalls":0,"directory_misses":0,
 "deadlock_violations":42,"memory_violations":17})";

} // namespace

TEST_CASE("[cep][tools] 3 个 tools 存在、语法正确且可 --help", "[cep][tools]") {
    for (const auto& t : tool_files()) {
        const auto path = repo_root() / t;
        INFO("缺 tool: " << t);
        REQUIRE(std::filesystem::exists(path));
        REQUIRE_FALSE(read_file(path.string()).empty());
        // 语法检查:py_compile 非零 → 语法错误(此前**完全没查**)
        INFO("tool 语法错误: " << t);
        REQUIRE(run_cmd("python3 -m py_compile " + shquote(path.string())) == 0);
        // --help 必须成功退出(测试名此前撒谎 —— 从未真的调过 --help)
        INFO("tool --help 失败: " << t);
        REQUIRE(run_cmd("python3 " + shquote(path.string()) + " --help") == 0);
    }
}

TEST_CASE("[cep][tools] 一致性闸门真的生效(ADR-CEP-06 IC3)", "[cep][tools]") {
    // ⚠️ 此前三个工具**只查 metric 键是否存在**、从不查值:
    //    deadlock_violations=42 的实验目录 → render_report --check 退出码 0、
    //    run_sweep 计入成功、ab_compare 输出正常 A/B 结论(2026-10-21 复审 B2)。
    const auto tool = (repo_root() / "tools/render_report.py").string();

    const auto good = make_exp_dir("gate_good", kGoodMetrics);
    INFO("合规实验的 --check 应退出 0");
    REQUIRE(run_cmd("python3 " + shquote(tool) + " " + shquote(good.string()) + " --check") == 0);

    const auto bad = make_exp_dir("gate_bad", kBadMetrics);
    INFO("违例实验的 --check 必须非零退出");
    REQUIRE(run_cmd("python3 " + shquote(tool) + " " + shquote(bad.string()) + " --check") != 0);
    INFO("违例实验的 render 必须非零退出");
    REQUIRE(run_cmd("python3 " + shquote(tool) + " " + shquote(bad.string())) != 0);
    // 报告里必须有醒目的违例横幅
    const std::string html = read_file((bad / "report.html").string());
    REQUIRE(html.find("一致性违例") != std::string::npos);

    std::filesystem::remove_all(good);
    std::filesystem::remove_all(bad);
}

TEST_CASE("[cep][tools] ab_compare 可实际运行(回归:缺 import html → NameError)", "[cep][tools]") {
    const auto tool = (repo_root() / "tools/ab_compare.py").string();
    const auto dir = make_exp_dir("ab_run", kGoodMetrics);
    const auto out = std::filesystem::temp_directory_path() / "cep_tools_ab_out";
    std::filesystem::remove_all(out);
    INFO("ab_compare 运行失败(此前缺 import html → NameError 崩出)");
    REQUIRE(run_cmd("python3 " + shquote(tool) + " " + shquote(dir.string()) + " " +
                    shquote(dir.string()) + " -o " + shquote(out.string())) == 0);
    REQUIRE(std::filesystem::exists(out / "delta_metrics.json"));
    REQUIRE(std::filesystem::exists(out / "ab_compare.html"));
    std::filesystem::remove_all(dir);
    std::filesystem::remove_all(out);
}

TEST_CASE("[cep][tools] metric 类型非法必须被拒(回归:schema 从未用于校验真实文件)",
          "[cep][tools]") {
    const auto tool = (repo_root() / "tools/render_report.py").string();
    const std::string bad_type = R"({
 "total_cycles":"abc","total_transactions":10,"transactions_per_cycle":0.1,
 "avg_latency_cycles":1.0,"p50_latency_cycles":1.0,"p95_latency_cycles":1.0,
 "p99_latency_cycles":1.0,"max_latency_cycles":1.0,"coherence_stalls":0,
 "collective_stalls":0,"directory_misses":0,"deadlock_violations":0,"memory_violations":0})";
    const auto dir = make_exp_dir("bad_type", bad_type);
    INFO("total_cycles 为字符串时 --check 必须非零退出");
    REQUIRE(run_cmd("python3 " + shquote(tool) + " " + shquote(dir.string()) + " --check") != 0);
    std::filesystem::remove_all(dir);
}

TEST_CASE("[cep][tools] 缺件套时 --check 必须失败", "[cep][tools]") {
    const auto tool = (repo_root() / "tools/render_report.py").string();
    const auto dir = make_exp_dir("missing", kGoodMetrics, /*with_trace=*/false);
    INFO("缺 trace.h5 时 --check 必须非零退出");
    REQUIRE(run_cmd("python3 " + shquote(tool) + " " + shquote(dir.string()) + " --check") != 0);
    std::filesystem::remove_all(dir);
}

TEST_CASE("[cep][tools] 零项目内依赖(per design D4)", "[cep][tools]") {
    // 约束:tools 不得 import 项目内包 —— 否则工具会依赖仿真构建产物,
    // 破坏"实验产物可独立处理"的能力。
    //
    // ⚠️ 此前只 grep "import cpptlm" —— `import os, cpptlm` / `__import__("cpptlm")`
    //    / `importlib.import_module("cpptlm")` / `from  cpptlm import x` **全部绕过**。
    //    现改用 Python **ast** 真判定(执行式,非文本式)。
    for (const auto& t : tool_files()) {
        const auto path = repo_root() / t;
        const std::string text = read_file(path.string());
        INFO("tool 出现项目内依赖: " << t);
        REQUIRE(text.find("cpptlm.library") == std::string::npos);
        // ast 判定:任何 import 的模块名落在 cpptlm / cpptlm.* 下即违规。
        // 判定脚本落**临时文件**再执行(避免 shell 引号嵌套把 Python 源码打碎)。
        const std::string script =
            "import ast,sys\n"
            "t=ast.parse(open(sys.argv[1],encoding='utf-8').read())\n"
            "bad=[]\n"
            "for n in ast.walk(t):\n"
            "    if isinstance(n,ast.Import):\n"
            "        for a in n.names:\n"
            "            if a.name=='cpptlm' or a.name.startswith('cpptlm.'): bad.append(a.name)\n"
            "    elif isinstance(n,ast.ImportFrom):\n"
            "        if n.module and (n.module=='cpptlm' or n.module.startswith('cpptlm.')): "
            "bad.append(n.module)\n"
            "print(';'.join(bad))\n";
        const auto checker = std::filesystem::temp_directory_path() / "cep_ast_dep_check.py";
        {
            std::ofstream cf(checker);
            cf << script;
        }
        const std::string cmd =
            "python3 " + shquote(checker.string()) + " " + shquote(path.string());
        FILE* p = popen((cmd + " 2>/dev/null").c_str(), "r");
        REQUIRE(p != nullptr);
        char buf[1024] = {0};
        const size_t n = fread(buf, 1, sizeof(buf) - 1, p);
        const int rc = pclose(p);
        INFO("ast 判定脚本执行失败 rc=" << rc);
        REQUIRE(rc == 0);
        std::string out(buf, n);
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
            out.pop_back();
        INFO("tool import 了项目内包: " << out);
        REQUIRE(out.empty());
    }
}

TEST_CASE("[cep][tools] 依赖方向单向:tools 读 experimental,不反向", "[cep][tools]") {
    for (const auto& t : tool_files()) {
        const std::string text = read_file((repo_root() / t).string());
        INFO("tool 不得依赖仿真构建产物: " << t);
        REQUIRE(text.find("cpptlm_core") == std::string::npos);
        REQUIRE(text.find("ctest") == std::string::npos);
    }
}

TEST_CASE("[cep][tools] 各 tool 声明并校验其消费的 4 件套子集", "[cep][tools]") {
    // 逐 tool 声明**必须**校验的 4 件套成员:
    //   render_report 产出 report.html -> 消费全部 4 件套
    //   run_sweep   产出 4 件套       -> 校验全部 4 件套
    //   ab_compare  只做 metrics 差分 -> 只需 metrics.json + simulation_config.json
    // 4 件套按**产出方向**区分(per ADR-CEP-06 §2):
    //   输入 3 件套 = simulation_config.json / trace.h5 / metrics.json(runner 产出)
    //   输出 1 件套 = report.html(render_report 产出)
    // 因此 render_report / run_sweep 消费**输入 3 件套**,ab_compare 只需 metrics + config。
    const std::vector<std::string> inputs3 = {"simulation_config.json", "trace.h5", "metrics.json"};
    const std::vector<std::string> metrics_only = {"simulation_config.json", "metrics.json"};
    const std::map<std::string, std::vector<std::string>> required = {
        {"tools/render_report.py", inputs3},
        {"tools/run_sweep.py", inputs3},
        {"tools/ab_compare.py", metrics_only},
    };
    for (const auto& kv : required) {
        const std::string text = read_file((repo_root() / kv.first).string());
        for (const auto& a : kv.second) {
            INFO("tool " << kv.first << " 未校验件套成员 " << a);
            REQUIRE(text.find(a) != std::string::npos);
        }
        INFO("tool 缺 metric 校验: " << kv.first);
        REQUIRE(text.find("total_cycles") != std::string::npos);
        REQUIRE(text.find("memory_violations") != std::string::npos);
        REQUIRE(text.find("deadlock_violations") != std::string::npos);
    }
    // 方向性纪律:render_report / run_sweep **不得**把 report.html 当输入件套
    // (否则 check 会要求自己产出的文件 —— 自指矛盾)
    for (const char* t : {"tools/render_report.py", "tools/run_sweep.py"}) {
        const std::string text = read_file((repo_root() / t).string());
        INFO(t << " 把 report.html 当作输入件套校验(自指矛盾): ");
        REQUIRE(text.find("INPUT_ARTIFACTS") != std::string::npos);
        REQUIRE(text.find("OUTPUT_ARTIFACT") != std::string::npos);
    }
}

TEST_CASE("[cep][tools] 验证深度不得被越界表述(per ADR-CEP-06 §3)", "[cep][tools]") {
    for (const auto& t : tool_files()) {
        const std::string text = read_file((repo_root() / t).string());
        INFO("tool 未强制 verification 声明: " << t);
        REQUIRE(text.find("verification_mode") != std::string::npos);
        REQUIRE(text.find("verification_scope") != std::string::npos);
    }
    // 且必须带"不构成已证明"的免责措辞
    const std::string r = read_file((repo_root() / "tools/ab_compare.py").string());
    REQUIRE(r.find("不构成") != std::string::npos);
}

TEST_CASE("[cep][tools] runner 契约:输出目录经 env + 参数双通道告知", "[cep][tools]") {
    // 回归:早期版本 runner 不知道往哪写产物 → 每次运行都失败。
    // 契约:环境变量 CEP_EXP_DIR / CEP_EXP_ID + 命令行 --exp-dir=。
    const std::string text = read_file((repo_root() / "tools/run_sweep.py").string());
    REQUIRE(text.find("CEP_EXP_DIR") != std::string::npos);
    REQUIRE(text.find("CEP_EXP_ID") != std::string::npos);
    REQUIRE(text.find("--exp-dir=") != std::string::npos);
    // 且 sweep 可后置补齐第 4 件套
    REQUIRE(text.find("--render-report") != std::string::npos);
    REQUIRE(text.find("render_report.py") != std::string::npos);
    REQUIRE(text.find("--require-complete") != std::string::npos);
}

TEST_CASE("[cep][tools] runner 不可执行时优雅报错", "[cep][tools]") {
    // OSError(文件不存在)须被捕获,不得抛出未处理异常
    const std::string text = read_file((repo_root() / "tools/run_sweep.py").string());
    REQUIRE(text.find("OSError") != std::string::npos);
    REQUIRE(text.find("runner 无法执行") != std::string::npos);
}

TEST_CASE("[cep][tools] experimental/ 目录约定存在", "[cep][tools]") {
    const auto dir = repo_root() / "experimental";
    REQUIRE(std::filesystem::exists(dir));
    REQUIRE(std::filesystem::exists(dir / "metrics.schema.json"));
    REQUIRE(std::filesystem::exists(dir / ".gitkeep"));
}
