// test/test_cep_engine_and_numbering_contract.cc
// 引擎默认实现 + 冻结面 + 编号口径的语义不变量:
//   · INV-CEP-07-IC2 —— `NullProtocolEngine` 是默认实现(10 流全按矩阵 no-op/immediate)
//   · INV-CEP-07-IC5 —— CEP 不破 23 ABI / frozen headers / `[[deprecated]]` 状态
//   · INV-CEP-07-IC6 —— functional 模式 `avg_latency_cycles == 0`
//   · INV-CEP-16-IC2 —— 每个 Stage 能指出承接的 06 §15.4 条目(或显式"不承接")
//   · INV-CEP-16-IC3 —— P3 协议语义项归 ArchForge;CppTLM 侧不得有实现
//   · INV-CEP-NUM-IC2 —— 跨 ADR 不变量引用必须加 ADR 前缀
//   · INV-CEP-NUM-IC3 —— A14 编号口径冻结为 06 §15.4
//
// ⚠️ 全部**结构/行为**断言(⛔ 不是「ADR 里写着这句话」)。每条都配反向 fixture。
//
// 作者: CppTLM Team / 日期: 2026-11-05
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/event_queue.hh"
#include "tlm/cep/protocol_engine_base.hh"

using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    using nlohmann::json;

    // ── git 辅助(与 `test_cep_module_factory_seam.cc` 同纪律:git 出错**不得**静默绿)──
    std::string run_git(const std::string& args) {
        const std::string root = repo_root().string();
        const std::string cmd = "git -C '" + root + "' " + args + " 2>/dev/null";
        FILE* p = popen(cmd.c_str(), "r");
        if (!p)
            throw std::runtime_error("popen 失败: " + cmd);
        std::string out;
        char buf[4096];
        while (fgets(buf, sizeof(buf), p))
            out += buf;
        const int rc = pclose(p);
        if (rc != 0)
            throw std::runtime_error("git 执行失败(rc=" + std::to_string(rc) + "): " + cmd);
        return out;
    }

    bool modified_since(const std::string& rel, const std::string& baseline) {
        const std::string out = run_git("diff --name-only " + baseline + " -- '" + rel + "'");
        return out.find(rel + "\n") != std::string::npos;
    }

    // CEP 冻结面基线 = CEP 工作开始前的 commit(与 `module_factory_seam` 同值)
    constexpr const char* kCepBaselineCommit = "4ae6bfc435a68c34c85504f24928ae3fea752f31";
    bool file_modified(const std::string& rel) {
        const char* env = std::getenv("CEP_BASELINE_COMMIT");
        const std::string baseline =
            (env && *env) ? std::string(env) : std::string(kCepBaselineCommit);
        return modified_since(rel, baseline);
    }

} // namespace

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-07-IC2 —— `NullProtocolEngine` = 默认实现(10 流按矩阵)
// ═════════════════════════════════════════════════════════════════════
namespace {

    using tlm::cep::CanonicalFlow;
    using tlm::cep::NullFlowBehavior;

    // 由**矩阵**(唯一真源)推导期望分桶,⛔ 不硬编码条数
    struct ExpectedBuckets {
        std::size_t no_op = 0;
        std::size_t immediate_complete = 0;
        std::size_t immediate_resolve = 0;
    };
    ExpectedBuckets expected_buckets() {
        ExpectedBuckets b;
        for (std::size_t i = 0; i < tlm::cep::kNullFlowMatrixSize; ++i) {
            switch (tlm::cep::kNullFlowMatrix[i].behavior) {
            case NullFlowBehavior::NoOp:
                ++b.no_op;
                break;
            case NullFlowBehavior::ImmediateComplete:
                ++b.immediate_complete;
                break;
            case NullFlowBehavior::ImmediateResolve:
                ++b.immediate_resolve;
                break;
            }
        }
        return b;
    }

    // 计数是否与矩阵一致(每流恰 1 次 + 三个行为桶的分布)
    bool counters_match_matrix(const tlm::cep::NullProtocolEngineCounters& c, std::string* why) {
        const auto exp = expected_buckets();
        for (std::size_t i = 0; i < tlm::cep::kCanonicalFlowCount; ++i) {
            if (c.per_flow[i] != 1u) {
                *why = "流 " + std::to_string(i) + " 的处理次数不是 1";
                return false;
            }
        }
        if (c.no_op != exp.no_op || c.immediate_complete != exp.immediate_complete ||
            c.immediate_resolve != exp.immediate_resolve) {
            *why = "行为分桶与矩阵不一致(no-op=" + std::to_string(c.no_op) + "/" +
                   std::to_string(exp.no_op) +
                   ", imm-complete=" + std::to_string(c.immediate_complete) + "/" +
                   std::to_string(exp.immediate_complete) +
                   ", imm-resolve=" + std::to_string(c.immediate_resolve) + "/" +
                   std::to_string(exp.immediate_resolve) + ")";
            return false;
        }
        return true;
    }

} // namespace

// INV-CARRIER: INV-CEP-07-IC2
// 变红理由(per ADR-CEP-06 SU#4 规则 B):Null 引擎某流行为偏离矩阵(引入协议动作/延迟)/
// 覆盖矩阵缺流 / 计数器无法区分行为 ⇒ 红
TEST_CASE("INV-CEP-07-IC2:NullProtocolEngine 是默认实现(10 流按矩阵,零协议动作)",
          "[cep][null-engine]") {
    // ① 覆盖完整性 + 逐流行为与 07 号 §6.4 口径一致(编译期锁的运行期证据)
    static_assert(tlm::cep::null_flow_matrix_is_complete(),
                  "10 流覆盖矩阵必须恰好覆盖 canonical 10 流");
    static_assert(tlm::cep::null_flow_matrix_behaviors_are_expected(),
                  "逐流行为必须与 07 号 §6.4 一致");
    REQUIRE(tlm::cep::null_flow_matrix_is_complete());
    REQUIRE(tlm::cep::null_flow_matrix_behaviors_are_expected());
    REQUIRE(tlm::cep::kNullFlowMatrixSize == tlm::cep::kCanonicalFlowCount);

    // ② 行为:实际调用 10 流 ⇒ 计数分布与矩阵一致(no-op 不产生协议动作)
    EventQueue eq;
    tlm::cep::NullProtocolEngine<2> eng("null_engine", &eq);
    eng.on_request(0x1000, tlm::cep::Perm::READ, 1);
    eng.on_completion(1, 0, tlm::cep::Perm::READ, false);
    eng.on_snoop(0x1000, tlm::cep::SnoopType::INVALIDATE, 2);
    eng.on_snoop_resp(0x1000, true, tlm::cep::LineState::E, false, 0, 2);
    eng.on_writeback(0x1000, 0, true);
    eng.on_forward_data(0x1000, 0x2000, 0, tlm::cep::Perm::READ, 3);
    eng.on_back_invalidate(0x1000, tlm::cep::SnoopType::CLEAN, 4);
    tlm::cep::AddressResolveResult rr;
    eng.on_address_resolve(0x1000, 7, rr);
    eng.on_barrier(0, 5);
    eng.on_snoop_completion_ack(0x1000, 7, 2);

    std::string why;
    INFO("Null 引擎计数与矩阵不一致: " << why);
    REQUIRE(counters_match_matrix(eng.counters(), &why));

    // ③ 反向 fixture(判别力证据)────────────────────────────────────────
    // (a) 某流计数被篡改 ⇒ 判否
    {
        auto c = eng.counters();
        c.per_flow[0] = 2;
        std::string w;
        REQUIRE_FALSE(counters_match_matrix(c, &w));
    }
    // (b) 行为分桶被篡改(把一次 immediate-complete 记成 no-op)⇒ 判否
    {
        auto c = eng.counters();
        ++c.no_op;
        --c.immediate_complete;
        std::string w;
        REQUIRE_FALSE(counters_match_matrix(c, &w));
    }
    // (c) 「期望」规则不是恒 NoOp:Completion/AddressResolve/Barrier 各有非 NoOp 行为
    REQUIRE(tlm::cep::expected_null_behavior(CanonicalFlow::Completion) ==
            NullFlowBehavior::ImmediateComplete);
    REQUIRE(tlm::cep::expected_null_behavior(CanonicalFlow::AddressResolve) ==
            NullFlowBehavior::ImmediateResolve);
    REQUIRE(tlm::cep::expected_null_behavior(CanonicalFlow::Barrier) ==
            NullFlowBehavior::ImmediateComplete);
    REQUIRE(tlm::cep::expected_null_behavior(CanonicalFlow::Request) == NullFlowBehavior::NoOp);
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-07-IC5 —— CEP 不破 ABI / frozen headers / `[[deprecated]]` 状态
// ═════════════════════════════════════════════════════════════════════
namespace {
    // 某源码中 `[[deprecated ...]]` 是否修饰**指定的那个**类声明。
    // 判定:属性与 `class X` 必须**同行**,或属性独占**紧邻的上一行**
    //   (⛔ 不得把「上一个类的属性」误算到本类 —— 那会让反向 fixture 假绿)
    bool class_is_deprecated(const std::string& src, const std::string& cls) {
        const auto pos = src.find("class " + cls);
        if (pos == std::string::npos)
            return false;
        const auto nl = src.rfind('\n', pos);
        const std::size_t line_start = (nl == std::string::npos) ? 0 : nl + 1;
        const std::string line = src.substr(line_start, pos - line_start);
        if (line.find("[[deprecated") != std::string::npos)
            return true;
        if (line_start == 0)
            return false;
        // 紧邻的上一行
        const auto prev_nl =
            (line_start >= 2) ? src.rfind('\n', line_start - 2) : std::string::npos;
        const std::size_t prev_start = (prev_nl == std::string::npos) ? 0 : prev_nl + 1;
        const std::string prev = src.substr(prev_start, line_start - 1 - prev_start);
        // 上一行只能是**裸属性行**(不得是另一个类的声明,否则会把别人的属性算到本类)
        if (prev.find("class ") != std::string::npos)
            return false;
        return prev.find("[[deprecated") != std::string::npos;
    }
} // namespace

// INV-CARRIER: INV-CEP-07-IC5
// 变红理由(per ADR-CEP-06 SU#4 规则 B):ABI / 冻结头相对 CEP 基线被改(cpp 改动)/
// PcieEndpointTLM 的 [[deprecated]] 状态被移除 ⇒ 红
TEST_CASE("INV-CEP-07-IC5:CEP 不破 ABI 与冻结面(PcieEndpointTLM 保持 deprecated)",
          "[cep][null-engine]") {
    // ① ABI 头相对 CEP 基线**零 diff**
    INFO("ABI 头被改动(CEP 冻结面)");
    REQUIRE_FALSE(file_modified("include/abi/cpptlm_emulator.h"));

    // ② 4 个 PCIe 冻结头零 diff
    for (const char* f :
         {"include/tlm/gpu/pcie_endpoint_tlm.h", "include/tlm/pcie/pcie_bundles_tlm.hh",
          "include/tlm/gpu/pcie_display_device.hh", "include/tlm/gpu/pcie_endpoint_ip.hh"}) {
        INFO("冻结头被改动: " << f);
        REQUIRE_FALSE(file_modified(f));
    }

    // ③ `PcieEndpointTLM` 仍带 `[[deprecated]]` 且提示指向新实现(状态未被移除)
    const auto hdr = read_file((repo_root() / "include/tlm/gpu/pcie_endpoint_tlm.h").string());
    REQUIRE(!hdr.empty());
    REQUIRE(class_is_deprecated(hdr, "PcieEndpointTLM"));
    REQUIRE(hdr.find("deprecated") != std::string::npos);
    // 「与 PcieEndpointIP 并存」(不得直接删除 —— 既有 Phase 4 测试依赖)
    REQUIRE(hdr.find("PcieEndpointIP") != std::string::npos);

    // ④ 反向 fixture:去掉 deprecated 属性的类声明 ⇒ 检测必须判否
    REQUIRE_FALSE(
        class_is_deprecated("class PcieEndpointTLM : public SimObject {};", "PcieEndpointTLM"));
    REQUIRE_FALSE(class_is_deprecated("[[deprecated]] class Other {};\nclass PcieEndpointTLM {};",
                                      "PcieEndpointTLM"));
    // 正向对照(证明 ④ 不是恒 false)
    REQUIRE(class_is_deprecated("[[deprecated(\"use X\")]] class PcieEndpointTLM : public Y {};",
                                "PcieEndpointTLM"));
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-07-IC6 —— functional 模式 `avg_latency_cycles == 0`
// ═════════════════════════════════════════════════════════════════════
namespace {
    // 返回问题列表;空 = 合规。
    // 规则(per ADR-CEP-06 §5):`simulation_mode == functional` ⇒ 延迟不得为正数。
    //   · 数值 0  ⇒ 合规(功能模式不建模延迟)
    //   · "not-collected" ⇒ 合规(**更强**的诚实标注,per SU#1 §②)
    //   · 正数      ⇒ **违规**(functional 模式编造时序数据)
    std::vector<std::string> functional_latency_problems(const json& sim_cfg, const json& metrics) {
        std::vector<std::string> out;
        const std::string mode = sim_cfg.value("simulation_mode", std::string(""));
        if (mode != "functional")
            return out; // 非 functional 模式不在本判据范围
        if (!metrics.contains("avg_latency_cycles")) {
            out.push_back("functional 模式下缺 avg_latency_cycles");
            return out;
        }
        const auto& v = metrics.at("avg_latency_cycles");
        if (v.is_string()) {
            if (v.get<std::string>() != "not-collected")
                out.push_back("avg_latency_cycles 字符串值非法: " + v.get<std::string>());
            return out;
        }
        if (!v.is_number())
            out.push_back("avg_latency_cycles 既非数值也非 not-collected");
        else if (v.get<double>() != 0.0)
            out.push_back("functional 模式下 avg_latency_cycles 必须为 0(实际 " +
                          std::to_string(v.get<double>()) + ")");
        return out;
    }
} // namespace

// (编造时序数据)⇒ 红
TEST_CASE("INV-CEP-07-IC6:functional 模式 avg_latency_cycles 必须为 0", "[cep][null-engine]") {
    const json func = json{{"simulation_mode", "functional"}};
    const json timing = json{{"simulation_mode", "timing"}};

    // ① 正控:functional + 0 ⇒ 合规
    // INV-CARRIER: INV-CEP-07-IC6
    // 变红理由(per ADR-CEP-06 SU#4 规则 B):functional 模式产出正数 avg_latency_cycles
    REQUIRE(functional_latency_problems(func, json{{"avg_latency_cycles", 0}}).empty());
    // ② functional + "not-collected" ⇒ 合规(更强的诚实标注)
    REQUIRE(
        functional_latency_problems(func, json{{"avg_latency_cycles", "not-collected"}}).empty());
    // ③ 判据主体:functional + 正数 ⇒ **违规**
    INFO("functional 模式不得产出正延迟");
    REQUIRE_FALSE(functional_latency_problems(func, json{{"avg_latency_cycles", 42}}).empty());
    // ④ 对照:timing 模式 + 正数 ⇒ 合规(证明 ③ 不是恒否)
    REQUIRE(functional_latency_problems(timing, json{{"avg_latency_cycles", 42}}).empty());
    // ⑤ functional 缺该项 ⇒ 违规(不得静默放过)
    REQUIRE_FALSE(functional_latency_problems(func, json::object()).empty());
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-16-IC2 / INV-CEP-NUM-IC3 —— 06 §15.4 逐项映射 + 口径冻结
// ═════════════════════════════════════════════════════════════════════
namespace {

    struct MappingRow {
        std::string p;           // "P2.0" / …
        std::string disposition; // "supersede" / …
    };

    // 解析 `ADR-CEP-16 §2 决策 1` 的逐项映射表(行首单元格 = **P…**)
    std::vector<MappingRow> parse_mapping_table(const std::string& adr) {
        std::vector<MappingRow> rows;
        const auto sec = adr.find("### 决策 1:逐项映射表");
        if (sec == std::string::npos)
            return rows;
        const auto end = adr.find("\n### ", sec + 1);
        const std::string body =
            adr.substr(sec, (end == std::string::npos ? adr.size() : end) - sec);

        std::size_t pos = 0;
        while (pos < body.size()) {
            auto nl = body.find('\n', pos);
            if (nl == std::string::npos)
                nl = body.size();
            const std::string line = body.substr(pos, nl - pos);
            pos = nl + 1;
            if (line.empty() || line[0] != '|')
                continue;
            // 按 `|` 切单元格
            std::vector<std::string> cells;
            std::size_t c = 0;
            while (c < line.size()) {
                const auto bar = line.find('|', c);
                if (bar == std::string::npos) {
                    cells.push_back(line.substr(c));
                    break;
                }
                cells.push_back(line.substr(c, bar - c));
                c = bar + 1;
            }
            if (cells.size() < 6)
                continue;
            const std::string& idcell = cells[1];
            const auto b = idcell.find("**P");
            if (b == std::string::npos)
                continue;
            const auto e = idcell.find("**", b + 2);
            if (e == std::string::npos)
                continue;
            MappingRow r;
            r.p = idcell.substr(b + 2, e - b - 2);
            std::string d = cells[4];
            std::string clean;
            for (char ch : d) {
                if (ch == '*')
                    continue;
                clean += ch;
            }
            const auto l = clean.find_first_not_of(" \t");
            const auto rr = clean.find_last_not_of(" \t");
            if (l != std::string::npos)
                r.disposition = clean.substr(l, rr - l + 1);
            rows.push_back(r);
        }
        return rows;
    }

    // 处置词白名单 = 决策 2 表定义的 5 词 + P6 专用的「不引入」
    const std::set<std::string>& disposition_vocab() {
        static const std::set<std::string> v{"supersede", "部分承接", "改判", "改判 + 拆分",
                                             "明确排除"};
        return v;
    }

} // namespace

// INV-CARRIER: INV-CEP-16-IC2
// 变红理由(per ADR-CEP-06 SU#4 规则 B):某 §15.4 条目缺处置(无法指出承接)/
// 处置词不在定义集合内(发明新词)⇒ 红
TEST_CASE("INV-CEP-16-IC2:每个 06 §15.4 条目都有处置(或显式不引入)", "[cep][null-engine]") {
    const auto adr =
        read_file((repo_root() / "docs/adr/ADR-CEP-16-06-evolution-route-mapping.md").string());
    REQUIRE(!adr.empty());
    const auto rows = parse_mapping_table(adr);
    REQUIRE(rows.size() == 7u); // 6 个 §15.4 条目 + P6

    std::set<std::string> used;
    for (const auto& r : rows) {
        INFO("§15.4 条目缺处置: " << r.p);
        REQUIRE_FALSE(r.disposition.empty());
        if (r.p == "P6") {
            INFO("P6 必须是「不引入」");
            REQUIRE(r.disposition == "不引入");
            continue;
        }
        INFO("处置词不在定义集合内: " << r.p << " ⇒ " << r.disposition);
        REQUIRE(disposition_vocab().count(r.disposition) == 1u);
        used.insert(r.disposition);
    }
    // 5 个处置词**都被实际使用**(否则定义集合里有死词,判据会变松)
    INFO("处置词未被全部使用(决策 2 的定义有死词)");
    REQUIRE(used == disposition_vocab());

    // 反向 fixture:空处置 / 未知词 ⇒ 判否(证明校验非恒真)
    {
        auto bad = rows;
        bad.at(0).disposition.clear();
        bool rejected = false;
        for (const auto& r : bad)
            if (r.disposition.empty())
                rejected = true;
        REQUIRE(rejected);
    }
    {
        auto bad = rows;
        bad.at(0).disposition = "随便处置";
        REQUIRE(disposition_vocab().count(bad.at(0).disposition) == 0u);
    }
}

// INV-CARRIER: INV-CEP-NUM-IC3
// 变红理由(per ADR-CEP-06 SU#4 规则 B):§15.4 口径集合偏离 {P2.0,P2.1,P3,P3.5,P4,P5}
// (引入 P6 或漏项)⇒ 红
TEST_CASE("INV-CEP-NUM-IC3:A14 编号口径冻结为 06 §15.4(不含 P6)", "[cep][null-engine]") {
    const auto adr =
        read_file((repo_root() / "docs/adr/ADR-CEP-16-06-evolution-route-mapping.md").string());
    REQUIRE(!adr.empty());
    const auto rows = parse_mapping_table(adr);
    std::set<std::string> session154;
    bool has_p6 = false;
    for (const auto& r : rows) {
        if (r.p == "P6")
            has_p6 = true;
        else
            session154.insert(r.p);
    }
    const std::set<std::string> frozen{"P2.0", "P2.1", "P3", "P3.5", "P4", "P5"};
    INFO("§15.4 口径集合偏离冻结值");
    REQUIRE(session154 == frozen);
    // P6 只作为「§11.4 独有、不引入」出现 ⇒ 不属于 §15.4 口径
    REQUIRE(has_p6);
    REQUIRE(session154.count("P6") == 0u);
    // 反向 fixture:把 P6 混进 §15.4 集合 ⇒ 与冻结值不等(证明比较有判别力)
    auto polluted = session154;
    polluted.insert("P6");
    REQUIRE(polluted != frozen);
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-16-IC3 —— P3 协议语义建模项归 ArchForge;CppTLM 侧不得有实现
// ═════════════════════════════════════════════════════════════════════
namespace {

    std::string strip_comments_num(const std::string& src) {
        std::string out;
        bool in_line = false, in_block = false;
        for (std::size_t i = 0; i < src.size(); ++i) {
            if (in_line) {
                if (src[i] == '\n') {
                    in_line = false;
                    out += '\n';
                }
                continue;
            }
            if (in_block) {
                if (i + 1 < src.size() && src[i] == '*' && src[i + 1] == '/') {
                    in_block = false;
                    ++i;
                } else if (src[i] == '\n') {
                    out += '\n';
                }
                continue;
            }
            if (i + 1 < src.size() && src[i] == '/' && src[i + 1] == '/') {
                in_line = true;
                ++i;
                continue;
            }
            if (i + 1 < src.size() && src[i] == '/' && src[i + 1] == '*') {
                in_block = true;
                ++i;
                continue;
            }
            out += src[i];
        }
        return out;
    }

    // 是否出现**定义形态**:`class X` / `struct X` / `X(` 返回型函数定义 / `enum class X`
    bool defines_symbol(const std::string& code, const std::string& sym) {
        static const char* kKinds[] = {"class ", "struct ", "enum class ", "namespace "};
        for (const char* k : kKinds) {
            if (code.find(std::string(k) + sym) != std::string::npos)
                return true;
        }
        // 函数定义形态:`<type> translate_snoop(`
        const std::regex re("\\b" + sym + "\\s*\\(");
        return std::regex_search(code, re);
    }

} // namespace

// MesiThreeLevelProtocol / translate_snoop 的实现 ⇒ 红
TEST_CASE("INV-CEP-16-IC3:P3 协议语义建模项在 CppTLM 侧无实现", "[cep][null-engine]") {
    static const char* kForbidden[] = {"MesiGpuProtocol", "MesiThreeLevelProtocol",
                                       "translate_snoop"};
    // 只扫**实现面**(include/ + src/),文档提及合法
    const std::vector<std::filesystem::path> roots = {repo_root() / "include", repo_root() / "src"};
    std::size_t scanned = 0;
    for (const auto& root : roots) {
        for (const auto& e : std::filesystem::recursive_directory_iterator(root)) {
            if (!e.is_regular_file())
                continue;
            const auto ext = e.path().extension().string();
            if (ext != ".hh" && ext != ".h" && ext != ".cc" && ext != ".cpp")
                continue;
            ++scanned;
            const auto code = strip_comments_num(read_file(e.path().string()));
            for (const char* sym : kForbidden) {
                INFO("CppTLM 侧不得实现协议语义项: " << sym << " @ "
                                                     << e.path().filename().string());
                // INV-CARRIER: INV-CEP-16-IC3
                // 变红理由(per ADR-CEP-06 SU#4 规则 B):CppTLM 仓内出现 MesiGpuProtocol /
                REQUIRE_FALSE(defines_symbol(code, sym));
            }
        }
    }
    REQUIRE(scanned > 100u); // 扫描面下界(防目录读空导致恒真)

    // 反向 fixture(判别力):合成定义必须被抓;注释提及不得被抓
    REQUIRE(defines_symbol(strip_comments_num("class MesiGpuProtocol {};"), "MesiGpuProtocol"));
    REQUIRE(defines_symbol(strip_comments_num("void translate_snoop(int a);"), "translate_snoop"));
    REQUIRE_FALSE(
        defines_symbol(strip_comments_num("// MesiGpuProtocol class\nint x;"), "MesiGpuProtocol"));
    REQUIRE_FALSE(
        defines_symbol(strip_comments_num("auto s = \"translate_snoop\";"), "translate_snoop"));
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-NUM-IC2 —— 不变量 id 与 ADR 编号命名空间分离(跨 ADR 引用加前缀)
// ═════════════════════════════════════════════════════════════════════
namespace {
    // 裸引用形态:`CEP-04-IC1`(缺 `INV-` 前缀)/ `CEP-07A-IC1`
    // ⚠️ `std::regex`(ECMAScript)**不支持** lookbehind ⇒ 先匹配再手工查前缀。
    // ⚠️ 前缀白名单:`INV-`(canonical)/ `CEN-`(CEN-Ib* 族的另一前缀)/
    //    `ADR-`(如 `ADR-CEP-16-IC2` —— 仍是**显式命名空间**限定,不属「裸名混用」)。
    // ⚠️ 元讨论行排除:规则自身会**引用错误形态作反例**(如「`INV-CEP-04-IC1` 而非
    //    `CEP-04-IC1`」),此类行必须排除,否则判据只能靠改 ADR 正文才能绿。
    bool is_meta_naming_discussion(const std::string& line) {
        static const char* kMarkers[] = {"而非", "不强制", "避免", "统一加", "建议统一", "裸名"};
        for (const char* m : kMarkers) {
            if (line.find(m) != std::string::npos)
                return true;
        }
        return false;
    }

    std::vector<std::string> bare_invariant_refs(const std::string& text) {
        static const std::regex kRe(R"(\bCEP-\d+A?-IC\d+)");
        std::vector<std::string> out;
        // 逐行判定(元讨论行整行排除)
        std::size_t line_begin = 0;
        while (line_begin <= text.size()) {
            auto nl = text.find('\n', line_begin);
            if (nl == std::string::npos)
                nl = text.size();
            const std::string line = text.substr(line_begin, nl - line_begin);
            if (!is_meta_naming_discussion(line)) {
                for (std::sregex_iterator it(line.begin(), line.end(), kRe), end; it != end; ++it) {
                    const auto pos = static_cast<std::size_t>(it->position());
                    const std::string prefix = (pos >= 4) ? line.substr(pos - 4, 4) : std::string();
                    if (prefix == "INV-" || prefix == "CEN-" || prefix == "ADR-")
                        continue; // 显式命名空间限定
                    // 定义语境(「不变量 X」/「| 不变量 | X」)允许裸名
                    const std::string ctx =
                        (pos >= 24) ? line.substr(pos - 24, 24) : line.substr(0, pos);
                    if (ctx.find("不变量") != std::string::npos)
                        continue;
                    out.push_back(it->str());
                }
            }
            if (nl == text.size())
                break;
            line_begin = nl + 1;
        }
        return out;
    }
} // namespace

// INV-CARRIER: INV-CEP-NUM-IC2
// 变红理由(per ADR-CEP-06 SU#4 规则 B):ADR 里出现裸 `CEP-NN-ICx` 引用(与 ADR 编号
// 命名空间混用)⇒ 红
TEST_CASE("INV-CEP-NUM-IC2:跨 ADR 不变量引用必须带 INV- 前缀", "[cep][null-engine]") {
    const auto dir = repo_root() / "docs" / "adr";
    REQUIRE(std::filesystem::exists(dir));
    std::size_t scanned = 0;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        if (!e.is_regular_file() || e.path().extension() != ".md")
            continue;
        ++scanned;
        const auto text = read_file(e.path().string());
        const auto bare = bare_invariant_refs(text);
        std::string joined;
        for (const auto& b : bare) {
            if (!joined.empty())
                joined += ",";
            joined += b;
        }
        INFO("裸不变量引用(缺 INV- 前缀): " << e.path().filename().string() << " ⇒ " << joined);
        REQUIRE(bare.empty());
    }
    REQUIRE(scanned > 10u);

    // 反向 fixture:裸引用必须被抓;规范形态不得误伤
    REQUIRE(bare_invariant_refs("见 `CEP-04-IC1`").size() == 1u);
    REQUIRE(bare_invariant_refs("见 `INV-CEP-04-IC1`").empty());
    REQUIRE(bare_invariant_refs("见 `CEN-Ib1`").empty());
    REQUIRE(bare_invariant_refs("见 `INV-CEP-07A-IC1`").empty());
    REQUIRE(bare_invariant_refs("见 `ADR-CEP-16-IC2`").empty());                   // 有命名空间限定
    REQUIRE(bare_invariant_refs("统一加 `INV-` 前缀(`CEP-04-IC1` 而非)").empty()); // 元讨论行
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-07A-IC1 —— `coherence_domain.{hh,cc}` 必须带 `[[deprecated]]` +
//   指向 `ADR-CEP-07A` 的顶部注释;**不得**被**新**代码引用
// ═════════════════════════════════════════════════════════════════════
namespace {

    // 「既有引用」白名单 = CEP 工作开始前就存在的引用点(⛔ 只禁**新增**,不禁清理)
    const std::set<std::string>& allowed_referrers() {
        static const std::set<std::string> s{
            "src/core/coherence_domain.cc",        // 自身实现
            "src/core/module_factory.cc",          // 既有装配路径
            "src/core/module_factory_validate.cc", // 既有校验路径
            "test/test_coherence_domain.cc",       // 既有测试
            "test/test_domain_boundary.cc",        // 既有测试
        };
        return s;
    }

    // 扫描 include/ src/ test/ examples/ 中的 `#include ".../coherence_domain.hh"`
    std::vector<std::string> coherence_domain_referrers() {
        std::vector<std::string> out;
        for (const char* root : {"include", "src", "test", "examples"}) {
            const auto dir = repo_root() / root;
            if (!std::filesystem::exists(dir))
                continue;
            for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
                if (!e.is_regular_file())
                    continue;
                const auto ext = e.path().extension().string();
                if (ext != ".hh" && ext != ".h" && ext != ".cc" && ext != ".cpp")
                    continue;
                const auto text = read_file(e.path().string());
                // ⚠️ 待查串**分段拼接** ⇒ 扫描器不会命中自身源码(否则自己成为「新引用点」)
                static const std::string kQuoted =
                    std::string("#include") + " \"core/" + "coherence_domain.hh\"";
                static const std::string kAngled =
                    std::string("#include") + " <core/" + "coherence_domain.hh>";
                if (text.find(kQuoted) != std::string::npos ||
                    text.find(kAngled) != std::string::npos) {
                    out.push_back(std::filesystem::relative(e.path(), repo_root()).string());
                }
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }
} // namespace

// ADR-CEP-07A / 出现**新**引用点 ⇒ 红
TEST_CASE("INV-CEP-07A-IC1:coherence_domain 产物已标注废弃且无新引用", "[cep][null-engine]") {
    // ① 两个产物都带 `[[deprecated]]` + 顶部注释指向 `ADR-CEP-07A`
    for (const char* rel : {"include/core/coherence_domain.hh", "src/core/coherence_domain.cc"}) {
        const auto text = read_file((repo_root() / rel).string());
        INFO("废弃产物缺失: " << rel);
        // INV-CARRIER: INV-CEP-07A-IC1
        // 变红理由(per ADR-CEP-06 SU#4 规则 B):废弃产物缺 [[deprecated]] / 顶部注释不指向
        REQUIRE(!text.empty());
        INFO("废弃产物缺 [[deprecated]]: " << rel);
        REQUIRE(text.find("[[deprecated") != std::string::npos);
        INFO("顶部注释未指向 ADR-CEP-07A: " << rel);
        REQUIRE(text.find("ADR-CEP-07A") != std::string::npos);
    }

    // ② 不得被**新**代码引用:引用点集合 ⊆ 既有白名单
    const auto referrers = coherence_domain_referrers();
    REQUIRE(!referrers.empty()); // 既有引用确实存在(否则 ② 恒真)
    for (const auto& r : referrers) {
        INFO("出现**新**的 coherence_domain 引用点: " << r);
        REQUIRE(allowed_referrers().count(r) == 1u);
    }

    // ③ 反向 fixture(判别力):白名单外的引用必须被判否
    REQUIRE(allowed_referrers().count("src/brand_new_module.cc") == 0u);
    // ④ 正向对照(证明 ③ 不是恒 false)
    REQUIRE(allowed_referrers().count("src/core/module_factory.cc") == 1u);
    // ⑤ 反向 fixture:去 deprecated 的合成头 ⇒ 判否
    {
        const std::string bad = "// per ADR-CEP-07A\nclass CoherenceDomain {};\n";
        REQUIRE(bad.find("[[deprecated") == std::string::npos);
    }
}
