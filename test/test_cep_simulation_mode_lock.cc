// test/test_cep_simulation_mode_lock.cc
// CEP Stage 0 — simulation_mode 单一维度 + verification_mode 契约 + 4 件套契约
// per openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md
//     Requirement: cep-experiment-output-contract
//     + cep-integration-seam-contract
// 作者: CppTLM Team / 日期: 2026-10-21
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"
#include "cep_experiment_harness.hh"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using json = nlohmann::json;
using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    const std::vector<std::string>& required_metrics() {
        static const std::vector<std::string> m = {
            "total_cycles",       "total_transactions", "transactions_per_cycle",
            "avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
            "p99_latency_cycles", "max_latency_cycles", "coherence_stalls",
            "collective_stalls",  "directory_misses",   "deadlock_violations",
            "memory_violations"};
        return m;
    }

    const std::vector<std::string>& optional_metrics() {
        static const std::vector<std::string> m = {"link_utilization_per_link", "vc_stall_per_vc",
                                                   "snoop_fanout_per_request",
                                                   "directory_size_distribution"};
        return m;
    }

    const std::vector<std::string>& four_artifacts() {
        static const std::vector<std::string> a = {"simulation_config.json", "trace.h5",
                                                   "metrics.json", "report.html"};
        return a;
    }

    json load_metrics_schema() {
        const auto path = repo_root() / "experimental" / "metrics.schema.json";
        const std::string text = read_file(path.string());
        REQUIRE_FALSE(text.empty());
        return json::parse(text);
    }

    std::string adr_text(const std::string& file) {
        return read_file((repo_root() / "docs" / "adr" / file).string());
    }

} // namespace

TEST_CASE("[cep][simulation-mode] 4 件套恰好 4 个", "[cep][simulation-mode]") {
    const auto text = adr_text("ADR-CEP-06-experiment-protocol.md");
    for (const auto& a : four_artifacts()) {
        INFO("4 件套缺: " << a);
        REQUIRE(text.find(a) != std::string::npos);
    }
}

// INV-CARRIER: INV-CEP-06-IC2
// 变红理由(per ADR-CEP-06 SU#4 规则 B):13 必纳字段缺一 ⇒ 红
// ⛔ 短式 id 已在本标题消除 ⇒ 目标唯一
TEST_CASE("[cep][simulation-mode] 13 必纳 metric 齐备", "[cep][simulation-mode]") {
    const json s = load_metrics_schema();
    const auto& props = s.at("properties");
    for (const auto& m : required_metrics()) {
        INFO("metrics.schema 缺必纳 metric: " << m);
        REQUIRE(props.contains(m));
    }
    // ⚠️ 此前只校验**数量** → 换成 13 个任意名字也绿。改为集合相等。
    const auto& req = s.at("required");
    REQUIRE(req.size() == required_metrics().size());
    for (const auto& m : required_metrics()) {
        INFO("metrics.schema 的 required 缺: " << m);
        // ⚠️ nlohmann::json::contains() 只对 **object** 有效;对 array 恒返回 false
        //    → 必须显式遍历。
        bool found = false;
        for (const auto& r : req) {
            if (r.is_string() && r.get<std::string>() == m) {
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }
    // ⚠️ 2026-11-05(architect 验收 S-1):此前只断言 **schema** 有 13 项 ——
    //    invariant 的主体是「**实验输出** 13 必纳 metric」⇒ 必须有**产物侧**断言。
    //    用装置的真实产出者 `ExperimentMetrics::to_json()` 取产物,取其键集合比对。
    {
        cep_exp::ExperimentMetrics m;
        m.verification_scope = "architect-acceptance";
        const auto emitted = json::parse(m.to_json());
        std::set<std::string> keys;
        for (auto it = emitted.begin(); it != emitted.end(); ++it)
            keys.insert(it.key());
        for (const auto& name : required_metrics()) {
            INFO("产出的 metrics 缺 13 必纳字段: " << name);
            REQUIRE(keys.count(name) == 1u);
        }
    }

    for (const auto& m : optional_metrics()) {
        INFO("metrics.schema 缺可选 metric: " << m);
        REQUIRE(props.contains(m));
    }
}

// INV-CARRIER: INV-CEP-06-IC4
// 变红理由(per ADR-CEP-06 SU#4 规则 B):不可观测 metric 被写成 0 而非 not-collected ⇒ 红
// ⛔ 短式 id 已在本标题消除 ⇒ 目标唯一
TEST_CASE("[cep][simulation-mode] 不可观测 metric 的 not-collected 契约存在",
          "[cep][simulation-mode]") {
    const json s = load_metrics_schema();
    // ⚠️ 2026-11-05(Entry #218 §⑤-2;per ADR-CEP-06 SU#1 §② 载体 A):
    //   此前本用例只断言**单字符串** `_not_collected_reason` 的形状,
    //   **不断言 13 字段可承载 "not-collected"** ⇒ 测的是「reason 字段在」,
    //   而**值契约零覆盖** ⇒ 给缺口盖章(SU#1 §① 的实测结论)。
    //   ⇒ 本则改为断言**值契约**:13 必纳字段的 oneOf 含 "not-collected" 分支。
    REQUIRE(s.at("properties").contains("_not_collected"));
    const auto& nc = s.at("properties").at("_not_collected");
    REQUIRE(nc.at("type") == "object");
    REQUIRE(nc.at("additionalProperties").at("type") == "string");
    REQUIRE(nc.at("additionalProperties").at("minLength") == 1);

    // 逐个必纳 metric:必须能承载 "not-collected"(值契约的**正例**)
    for (const auto& m : required_metrics()) {
        INFO("metrics.schema 的 " + m + " 缺 oneOf");
        const auto& prop = s.at("properties").at(m);
        REQUIRE(prop.contains("oneOf"));
        bool has_sentinel = false;
        bool has_number = false;
        for (const auto& alt : prop.at("oneOf")) {
            if (alt.contains("const") && alt.at("const") == "not-collected")
                has_sentinel = true;
            if (alt.contains("type") && alt.at("type") == "number")
                has_number = true;
            if (alt.contains("type") && alt.at("type") == "integer")
                has_number = true;
        }
        INFO("metrics.schema 的 " + m + " 缺 not-collected 分支");
        REQUIRE(has_sentinel);
        INFO("metrics.schema 的 " + m + " 缺数值分支");
        REQUIRE(has_number);
    }
}

TEST_CASE("[cep][simulation-mode] not-collected 值必须配 per-metric 原因(反向 fixture)",
          "[cep][simulation-mode]") {
    // ⚠️ schema 层**无法**表达「标了 not-collected 但缺 reason」⇒ 该闸由 tools 承担
    //   (per SU#1 §② 配套)。本用例锁 tools 的判定函数行为(tools 是纯标准库 python,
    //   仓内无 python 单测框架 ⇒ 本用例锁「契约形状」而非行为)。
    const json s = load_metrics_schema();
    const auto& nc = s.at("properties").at("_not_collected");
    // ⛔ 不得退回单字符串形状(并存 = 两种形状双真源,per SU#1 §②)
    REQUIRE_FALSE(s.at("properties").contains("_not_collected_reason"));
    // per-metric 键必须落在 13 必纳 metric 内(由 tools 校验,schema 层用 additionalProperties 放开)
    REQUIRE(nc.at("additionalProperties").at("minLength") == 1);
}

TEST_CASE("[cep][simulation-mode] ADR-CEP-06 决策 5 三分支在 ADR 正文可寻址",
          "[cep][simulation-mode]") {
    // 决策 5 的三分支(SU#1 §③):functional ⇒ 延迟 = 0;timing 且未预声明 ⇒ 延迟 > 0;
    // timing 且预声明不采 ⇒ 延迟必须 "not-collected" ⇒ **禁写 0**。
    // ⚠️ 此前该规则在测试层**零断言**(实测 grep 全文无此检查)⇒ 纯靠 emit 自觉
    //   ⇒ 实测 h6 出现「timing + 自陈不采延迟 + 却写 0」。本用例锁 ADR 文本可寻址性。
    const std::string adr = adr_text("ADR-CEP-06-experiment-protocol.md");
    REQUIRE(adr.find("simulation_mode=functional") != std::string::npos);
    REQUIRE(adr.find("avg_latency_cycles") != std::string::npos);
    REQUIRE(adr.find("not-collected") != std::string::npos);
    // SU#1 §③ 的三分支表述须可寻址(追加在 SU 里 ⇒ 正文 §9 之后)
    REQUIRE(adr.find("决策 5 裁决") != std::string::npos);
}

TEST_CASE("[cep][simulation-mode] verification_mode 恰三档(exhaustive/bounded_random/sampled)且契约文本齐备", "[cep][simulation-mode]") {
    const json s = load_metrics_schema();
    const auto& props = s.at("properties");
    REQUIRE(props.contains("verification_mode"));
    REQUIRE(props.contains("verification_scope"));

    const std::set<std::string> modes(props.at("verification_mode").at("enum").begin(),
                                      props.at("verification_mode").at("enum").end());
    const std::set<std::string> expected_modes = {"exhaustive", "bounded_random", "sampled"};
    REQUIRE(modes == expected_modes);

    // ADR-CEP-06 §3:必须明文禁止把 bounded/sampled 表述为「已证明」
    const auto text = adr_text("ADR-CEP-06-experiment-protocol.md");
    REQUIRE(text.find("verification_mode") != std::string::npos);
    REQUIRE(text.find("verification_scope") != std::string::npos);
    REQUIRE(text.find("bounded_random") != std::string::npos);
    REQUIRE(text.find("sampled") != std::string::npos);
}

// INV-CARRIER: INV-CEP-06-IC5
// 变红理由(per ADR-CEP-06 SU#4 规则 B):memory_violations 只覆盖单分桶 ⇒ 红
// ⛔ 短式 id 已在本标题消除 ⇒ 目标唯一
TEST_CASE("[cep][simulation-mode] memory_violations 须覆盖 SWMR + data-value 双不变量(IC5)",
          "[cep][simulation-mode]") {
    const json s = load_metrics_schema();
    const auto& props = s.at("properties");
    REQUIRE(props.contains("memory_violation_kinds"));
    const auto& kinds = props.at("memory_violation_kinds").at("properties");
    REQUIRE(kinds.contains("swmr"));
    REQUIRE(kinds.contains("data_value"));

    const auto text = adr_text("ADR-CEP-06-experiment-protocol.md");
    REQUIRE(text.find("SWMR") != std::string::npos);
    REQUIRE(text.find("data-value") != std::string::npos);

    // ⚠️ 2026-11-05(architect 验收 S-3):schema 有双 kinds ≠ **统计**覆盖双分桶。
    //    用装置的真实检测器断言「两类违例**分别**计数且可区分」。
    {
        cep_exp::SwmrDataValueDetector det;
        det.write(0x100, /*version=*/1);          // 合法写
        det.write_without_invalidate(0x100, 2);   // 未先失效既有 sharer ⇒ SWMR
        det.read(0x100, /*observed_version=*/99); // 观察版本不符 ⇒ data-value
        INFO("SWMR 违例未单独计数");
        REQUIRE(det.counts().swmr == 1u);
        INFO("data-value 违例未单独计数");
        REQUIRE(det.counts().data_value == 1u);
        REQUIRE(det.counts().total() == 2u);
        // 反向:关掉检查 ⇒ 双桶均为 0(证明计数不是恒增)
        cep_exp::SwmrDataValueDetector off(/*checks_enabled=*/false);
        off.write(0x200, 1);
        off.write_without_invalidate(0x200, 2);
        off.read(0x200, 99);
        INFO("关闭检查后仍计数 ⇒ 统计面恒增(判据失效)");
        REQUIRE(off.counts().total() == 0u);
    }
}

// INV-CARRIER: INV-CEP-07-IC1
// 变红理由(per ADR-CEP-06 SU#4 规则 B):出现 network.mode ⇒ 红
// ⛔ 短式 id 已在本标题消除 ⇒ 目标唯一
TEST_CASE("[cep][simulation-mode] simulation_mode 是唯一模式维度,无 network.mode",
          "[cep][simulation-mode]") {
    const auto text = adr_text("ADR-CEP-07-simulation-and-integration-semantics.md");
    REQUIRE(text.find("simulation_mode") != std::string::npos);

    // `network.mode` 的**每一处**出现都必须在否定语境。
    //
    // ⚠️ 2026-10-21 代码复审 S3:原判定是「±200/+120 **字符窗口**内出现任一否定词」
    //    —— 两个缺陷:
    //      · **反向语义可骗过**:「network.mode 与 simulation_mode 并列,均可使用
    //        (不得混淆二者)」含「不得」→ 判为合规,但契约已破;
    //      · **正向误报**:距否定词 >200 字的合法否决提及 → 判违规。
    //    现改为**行级**判定:否定词必须与 `network.mode` 在**同一行**
    //    (表格行算同一单元)—— 无关联的邻近句不再能充当否定证据。
    const char* kNegations[] = {"不得", "禁止", "拒绝", "不引入", "不存在", "不共存",
                                "无 ",  "不再", "不会", "非",     "剔除"};
    auto line_of = [&text](std::size_t pos) {
        const std::size_t b = text.rfind('\n', pos);
        const std::size_t e = text.find('\n', pos);
        return text.substr(b == std::string::npos ? 0 : b + 1,
                           (e == std::string::npos ? text.size() : e) -
                               (b == std::string::npos ? 0 : b + 1));
    };
    std::size_t pos = 0;
    int occurrences = 0;
    while ((pos = text.find("network.mode", pos)) != std::string::npos) {
        ++occurrences;
        const std::string line = line_of(pos);
        INFO("network.mode 第 " << occurrences << " 处出现但同行无否定词: " << line);
        bool negated = false;
        for (const char* neg : kNegations) {
            if (line.find(neg) != std::string::npos) {
                negated = true;
                break;
            }
        }
        REQUIRE(negated);
        pos += 11;
    }
    REQUIRE(occurrences > 0);
    REQUIRE(text.find("INV-CEP-07-IC1") != std::string::npos);

    // ⚠️ spec:143「协议插件**不引入** `network.mode`(机器 grep 可查)」——
    //    断言面必须覆盖**代码与配置**,不只是 ADR 文字。
    int code_hits = 0;
    for (const char* root : {"include", "src", "configs", "tools"}) {
        const auto dir = repo_root() / root;
        if (!std::filesystem::exists(dir))
            continue;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file())
                continue;
            const auto ext = entry.path().extension().string();
            if (ext != ".cc" && ext != ".hh" && ext != ".h" && ext != ".json" && ext != ".py") {
                continue;
            }
            const auto body = read_file(entry.path().string());
            if (body.find("network.mode") == std::string::npos)
                continue;
            ++code_hits;
            INFO("代码/配置里出现 network.mode: " << entry.path().string());
            // 该行必须含否定词(与 ADR 同口径)
            std::size_t p2 = 0;
            while ((p2 = body.find("network.mode", p2)) != std::string::npos) {
                const auto b = body.rfind('\n', p2);
                const auto e = body.find('\n', p2);
                const std::string ln = body.substr(b == std::string::npos ? 0 : b + 1,
                                                   (e == std::string::npos ? body.size() : e) -
                                                       (b == std::string::npos ? 0 : b + 1));
                bool neg = false;
                for (const char* n : kNegations) {
                    if (ln.find(n) != std::string::npos) {
                        neg = true;
                        break;
                    }
                }
                INFO("代码里 network.mode 同行无否定词: " << ln);
                REQUIRE(neg);
                p2 += 11;
            }
        }
    }
    INFO("代码/配置中 network.mode 命中数: " << code_hits);
    REQUIRE(code_hits == 0);
}

TEST_CASE("[cep][simulation-mode] functional 模式 avg_latency_cycles == 0 为硬约束",
          "[cep][simulation-mode]") {
    const auto text = adr_text("ADR-CEP-07-simulation-and-integration-semantics.md");
    REQUIRE(text.find("avg_latency_cycles == 0") != std::string::npos);
    REQUIRE(text.find("INV-CEP-07-IC6") != std::string::npos);
}

TEST_CASE("[cep][simulation-mode] experimental/ 目录约定已建立", "[cep][simulation-mode]") {
    const auto dir = repo_root() / "experimental";
    REQUIRE(std::filesystem::exists(dir));
    REQUIRE(std::filesystem::exists(dir / "metrics.schema.json"));
}
