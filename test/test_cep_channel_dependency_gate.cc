// test/test_cep_channel_dependency_gate.cc
// capability `cep-channel-dependency-gate` —— 7 Scenario / 7 锚点(
//   顶点三元组 / 边由声明导出 / 8-8-0 + 与 S3 对账 / 注入环 ⇒ 配置 fail /
//   verdict 逐 (network, vc) / SKIP vs PASS / 双挂共享同一判据)
//
// 作者: CppTLM Team / 日期: 2026-10-25
//
// 纪律:
//   · 边**只能**来自声明数据 ⇒ 头文件里不得出现任何网络名/边字面量(带正控)
//   · 「注入环 ⇒ fail」必须用**真** instantiateAll 驱动,且配**同 config 无环 ⇒ true** 的正控
//     (否则「返回 false」可能只是配置本来就非法 —— 那是假阳性)

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include <nlohmann/json.hpp>

#include "chstream_register.hh"
#include "core/module_factory.hh"
#include "tlm/cep/channel_dependency_graph.hh"
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

    std::string declaration_path() {
        return std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/protocol_instances.json";
    }

    cep::ChannelDeclaration repo_declaration() {
        return cep::load_channel_declaration(declaration_path());
    }

    nlohmann::json repo_declaration_json() {
        std::ifstream in(declaration_path());
        nlohmann::json doc;
        in >> doc;
        return doc;
    }

    std::size_t count_lines_with(const std::string& text, const std::string& needle) {
        std::size_t n = 0;
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line)) {
            if (line.find(needle) != std::string::npos)
                ++n;
        }
        return n;
    }

    struct RunResult {
        int rc = -1;
        std::string out;
        bool spawned = false;
    };

    RunResult run_binary(const std::string& exe, const std::string& args = "") {
        RunResult r;
        const std::string cmd =
            std::string(CEP_SPIKE_BIN_DIR) + "/" + exe + (args.empty() ? "" : " " + args) + " 2>&1";
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

    /// 最小可装配拓扑 + 可替换的 `cep` 段(用于「真 instantiateAll」双向验证)
    nlohmann::json cep_config(const nlohmann::json& declaration) {
        nlohmann::json cfg;
        cfg["name"] = "cep_gate_fixture";
        cfg["modules"] =
            nlohmann::json::array({nlohmann::json{{"name", "mem0"}, {"type", "MemoryTLM"}}});
        cfg["connections"] = nlohmann::json::array();
        cfg["cep"] = nlohmann::json{{"protocol_instances", declaration}};
        return cfg;
    }

    /// 把声明里的边表改成成环(compute_noc: 加一条 3→1,与既有 1→3 互等)
    nlohmann::json declaration_with_cycle() {
        nlohmann::json doc = repo_declaration_json();
        auto& edges = doc["channel_dependency_edges"]["per_network"]["compute_noc"];
        edges.push_back(nlohmann::json::array({3, 1}));
        return doc;
    }

} // namespace

// ═════════════════════════════════════════════════════════════════════
// Scenario 1:顶点 = (协议实例, network, vc_id) 三元组
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP ChannelGate: 顶点是三元组", "[cep][channel-gate]") {
    // 三个字段名由**指定初始化**机检(改名 ⇒ 编译失败)
    const cep::Vertex v{.instance = "inst", .network = "net", .vc = 2};
    CHECK(v.instance == "inst");
    CHECK(v.network == "net");
    CHECK(v.vc == 2);

    CHECK(cep::vertex_key(v) == "inst|net|2");
    CHECK(cep::vertex_text(v) == "(inst, net, vc=2)");

    // vc 值域与 fabric 槽位上界对齐(0..3)
    CHECK(cep::kMaxVcId == 3u);

    // 顶点按 (instance, network, vc) 排序 ⇒ 同 vc 不同网是两个顶点(升级的实质)
    const cep::Vertex a{.instance = "inst", .network = "compute_noc", .vc = 0};
    const cep::Vertex b{.instance = "inst", .network = "ualink_fabric", .vc = 0};
    CHECK(cep::vertex_key(a) != cep::vertex_key(b));
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 2:边由声明导出(不是硬编码镜像)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP ChannelGate: 边由声明导出", "[cep][channel-gate]") {
    const cep::ChannelDeclaration decl = repo_declaration();
    REQUIRE(!decl.empty());
    REQUIRE(!decl.per_network_edges.empty());

    const auto base = cep::evaluate_channel_dependency_gate(decl);
    CHECK(base.verdict == cep::GateVerdict::PASS);
    REQUIRE(base.per_instance.count("moesi_gpu") == 1);
    CHECK(base.per_instance.at("moesi_gpu").edges == 8u);

    // ① 改声明 ⇒ 统计改变(去掉一条边)
    cep::ChannelDeclaration fewer = decl;
    REQUIRE(!fewer.per_network_edges["compute_noc"].empty());
    fewer.per_network_edges["compute_noc"].pop_back();
    const auto fewer_report = cep::evaluate_channel_dependency_gate(fewer);
    CHECK(fewer_report.per_instance.at("moesi_gpu").edges == 7u);
    // 边表是**逐网络共享**的 ⇒ 去掉一条边,每个绑定该网络的实例各少一条
    CHECK(fewer_report.stats.edges == base.stats.edges - decl.instances.size());
    CHECK(fewer_report.stats.vertices == base.stats.vertices);
    CHECK(fewer_report.stats.cross_net_edges == 0u);

    // ② 端点未绑定的边 ⇒ 声明自相矛盾 ⇒ FAIL(不是静默忽略)
    cep::ChannelDeclaration dangling = decl;
    dangling.per_network_edges["compute_noc"].emplace_back(0u, 3u);
    dangling.instances.front().vc_binding["compute_noc"].erase("snoop_resp"); // vc3 失去绑定
    dangling.instances.front().vc_binding["compute_noc"].erase("snoop");      // 保持 vc2 有绑定
    const auto dangling_report = cep::evaluate_channel_dependency_gate(dangling);
    CHECK(dangling_report.verdict == cep::GateVerdict::FAIL);
    bool saw_unbound = false;
    for (const auto& e : dangling_report.errors) {
        if (e.find("未绑定") != std::string::npos)
            saw_unbound = true;
    }
    CHECK(saw_unbound);

    // ③ 抗硬编码:判据实现里不得出现网络名(否则「镜像」被冻进了代码)
    const std::string code =
        strip_comments(read_repo_file("include/tlm/cep/channel_dependency_graph.hh"));
    REQUIRE(!code.empty());
    CHECK(code.find("compute_noc") == std::string::npos);
    CHECK(code.find("ualink_fabric") == std::string::npos);
    // 正控:同一机制必须能在**声明**里找到这两个名字(否则上面的零命中可能是扫错文件)
    const std::string decl_text = read_repo_file("include/tlm/cep/protocol_instances.json");
    REQUIRE(!decl_text.empty());
    CHECK(decl_text.find("compute_noc") != std::string::npos);
    CHECK(decl_text.find("ualink_fabric") != std::string::npos);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 3:仓库声明 = 8 顶点 / 8 边 / 0 跨网边,并与 S3 spike 对账
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP ChannelGate: moesi_gpu 声明 8-8-0 且与 S3 对账", "[cep][channel-gate]") {
    const cep::ChannelDeclaration decl = repo_declaration();
    REQUIRE(!decl.empty());

    const auto report = cep::evaluate_channel_dependency_gate(decl);
    REQUIRE(report.per_instance.count("moesi_gpu") == 1);
    CHECK(report.per_instance.at("moesi_gpu") == (cep::GraphStats{8, 8, 0}));
    // 全域 = **逐实例之和**(⚠️ 不可硬编码 `8 × 实例数` —— 各实例绑定的网络数
    // 不同:`moesi_gpu` / `mesif` 绑 2 个网络(8 顶点 / 8 边),`chi_rnf` 只绑
    // `compute_noc`(4 顶点 / 5 边)⇒ 硬编码会把「实例绑定形态差异」误判为缺陷)
    // ⇒ 改为按 `per_instance` 实际值求和(保持「全域 = 逐实例之和」的原意图)
    {
        unsigned sum_v = 0, sum_e = 0, sum_x = 0;
        for (const auto& [inst, st] : report.per_instance) {
            INFO("instance=" << inst);
            sum_v += st.vertices;
            sum_e += st.edges;
            sum_x += st.cross_net_edges;
        }
        CHECK(report.stats.vertices == sum_v);
        CHECK(report.stats.edges == sum_e);
        CHECK(report.stats.cross_net_edges == sum_x);
        CHECK(report.per_instance.size() == decl.instances.size());
    }
    CHECK(report.stats.cross_net_edges == 0u);
    CHECK(report.verdict == cep::GateVerdict::PASS);
    CHECK(report.cycle.empty());

    // 同轴对账:跑 S3 spike 的二进制度量,与本判据的统计逐项相等
    const auto spike = run_binary("cep_poc_spike_s3");
    if (!spike.spawned) {
        WARN("S3 spike 二进制不可用(未构建?)—— 本场景的对账部分 SKIP");
        return;
    }
    REQUIRE(spike.rc == 0);

    const std::string marker = "[S3][DECL]";
    const auto pos = spike.out.find(marker);
    REQUIRE(pos != std::string::npos);
    std::size_t v = 0, e = 0, c = 0;
    REQUIRE(std::sscanf(spike.out.c_str() + pos + marker.size(), " 顶点=%zu 边=%zu 跨网边=%zu", &v,
                        &e, &c) == 3);
    // spike 只覆盖 moesi_gpu(32 号 §6.1)⇒ 与**该实例**的统计对账
    CHECK(v == report.per_instance.at("moesi_gpu").vertices);
    CHECK(e == report.per_instance.at("moesi_gpu").edges);
    CHECK(c == report.per_instance.at("moesi_gpu").cross_net_edges);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 4:注入环 ⇒ 真 instantiateAll 返回 false(带无环正控)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP ChannelGate: 注入环 ⇒ 配置 fail", "[cep][channel-gate]") {
    REGISTER_CHSTREAM;

    // 正控:同一 config 形状 + 无环声明 ⇒ 装配成功
    {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(cep_config(repo_declaration_json())));
        CHECK(factory.getInstance("mem0") != nullptr);
    }

    // 负向:同一 config + 成环边表 ⇒ 显式 false(INV-CEP-09-IC1:配置 fail)
    {
        EventQueue eq;
        ModuleFactory factory(&eq);
        CHECK(factory.instantiateAll(cep_config(declaration_with_cycle())) == false);
        CHECK(factory.getInstance("mem0") == nullptr); // 未创建任何实例
    }

    // 判据层同样翻红,且 verdict 逐 (network, vc) 标注(不是一句全局结论)
    const cep::ChannelDeclaration cycle_decl =
        cep::parse_channel_declaration(declaration_with_cycle());
    const auto report = cep::evaluate_channel_dependency_gate(cycle_decl);
    CHECK(report.verdict == cep::GateVerdict::FAIL);
    CHECK(!report.cycle.empty());
    CHECK(report.verdicts.size() == report.stats.vertices);
    std::size_t failed = 0;
    for (const auto& line : report.verdicts) {
        if (line.verdict == cep::GateVerdict::FAIL) {
            ++failed;
            CHECK(line.detail == "in cycle");
        }
    }
    CHECK(failed >= 2); // 环上至少两个顶点
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 5:verdict 输出通道机器可 grep
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP ChannelGate: verdict 逐 (network, vc) 可 grep", "[cep][channel-gate]") {
    const auto report = cep::evaluate_channel_dependency_gate(repo_declaration());
    REQUIRE(report.verdict == cep::GateVerdict::PASS);

    const std::string log = report.to_log();
    CHECK(log.rfind("[PASS] cep channel dependency gate:", 0) == 0);
    CHECK(count_lines_with(log, "[verdict] net=") == report.stats.vertices);
    CHECK(log.find("[verdict] net=compute_noc vc=0 PASS (") != std::string::npos);
    CHECK(log.find("[verdict] net=ualink_fabric vc=3 PASS (") != std::string::npos);
    // 逐实例统计行(否则「合计 16」会掩盖单实例退化)
    CHECK(count_lines_with(log, "[per-instance] instance=") == report.per_instance.size());
    CHECK(log.find("[per-instance] instance=moesi_gpu vertices=8 edges=8") != std::string::npos);

    // 每行都带 instance 与判定理由(不是只有 PASS 字样)
    CHECK(count_lines_with(log, "[verdict] net=compute_noc vc=0 PASS (instance=moesi_gpu") == 1);
    CHECK(count_lines_with(log, "instance=moesi_gpu,") ==
          report.per_instance.at("moesi_gpu").vertices);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 6:缺失 / 为空 ⇒ SKIP,且与 PASS 可区分
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP ChannelGate: 空声明 ⇒ SKIP(≠ PASS)", "[cep][channel-gate]") {
    const cep::ChannelDeclaration empty;
    const auto report = cep::evaluate_channel_dependency_gate(empty);
    CHECK(report.verdict == cep::GateVerdict::SKIP);
    CHECK(report.verdict != cep::GateVerdict::PASS);
    CHECK(report.to_log().rfind("[SKIP]", 0) == 0);
    CHECK(report.stats.vertices == 0u);

    // config 里没有 `cep` 段 ⇒ 同样是 SKIP(而不是静默 PASS)
    nlohmann::json plain;
    plain["modules"] = nlohmann::json::array();
    plain["connections"] = nlohmann::json::array();
    CHECK(cep::run_channel_gate(plain).verdict == cep::GateVerdict::SKIP);
    CHECK(cep::run_ordering_gate(plain).verdict == cep::GateVerdict::SKIP);

    // 有 `cep` 段但**未指定**声明源 ⇒ 落到本仓默认声明(有牙,不静默 SKIP)
    nlohmann::json cep_only;
    cep_only["cep"] = nlohmann::json::object();
    const auto fallback = cep::run_channel_gate(cep_only);
    CHECK(fallback.verdict == cep::GateVerdict::PASS);
    // 缺省声明源含 3 个实例(moesi_gpu / mesif / chi_rnf)⇒ 顶点 = 逐实例之和
    // (各实例绑定网络数不同 ⇒ 不可硬编码;按 per_instance 实际值求和)
    {
        const auto fb = cep::run_channel_gate(cep_only);
        REQUIRE(fb.verdict == cep::GateVerdict::PASS);
        unsigned sum_v = 0;
        for (const auto& [inst, st] : fb.per_instance) {
            INFO("instance=" << inst);
            sum_v += st.vertices;
        }
        CHECK(fallback.stats.vertices == sum_v);
        CHECK(sum_v > 0u);
    }

    // 文件不存在且**未显式声明** ⇒ 空声明 ⇒ SKIP(外部条件)
    CHECK(cep::load_channel_declaration("/nonexistent/cep_decl.json", false).empty());
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 8:声明源**配置错误** ⇒ FAIL(不是 SKIP)
// ═════════════════════════════════════════════════════════════════════
// 「无可检查对象」(默认源缺失 ⇒ SKIP)与「作者把源写错了」(⇒ FAIL)必须分开:
// 若共用 SKIP 出口,声明源写错一个字符就等于闸不存在,且日志只说
// 「no declared protocol instance」,定位不到是路径错还是 JSON 坏。
TEST_CASE("CEP ChannelGate: 声明源配置错误 ⇒ FAIL(≠ SKIP)", "[cep][channel-gate]") {
    // ① 显式路径不存在
    nlohmann::json missing_src;
    missing_src["cep"] = nlohmann::json{{"protocol_instances", "/nonexistent/decl.json"}};
    const auto r1 = cep::run_channel_gate(missing_src);
    CHECK(r1.verdict == cep::GateVerdict::FAIL);
    CHECK(r1.to_log().rfind("[FAIL]", 0) == 0);
    CHECK(r1.to_log().find("不可读") != std::string::npos);

    // ② 显式路径存在但 JSON 畸形
    const std::string malformed =
        (std::filesystem::temp_directory_path() / "cep_decl_malformed_test.json").string();
    {
        std::ofstream out(malformed);
        out << "{ not json ,,,";
    }
    nlohmann::json bad_json;
    bad_json["cep"] = nlohmann::json{{"protocol_instances", malformed}};
    const auto r2 = cep::run_channel_gate(bad_json);
    CHECK(r2.verdict == cep::GateVerdict::FAIL);
    CHECK(r2.to_log().find("畸形") != std::string::npos);

    // ③ `cep` 段不是对象
    nlohmann::json cep_not_object;
    cep_not_object["cep"] = true;
    CHECK(cep::run_channel_gate(cep_not_object).verdict == cep::GateVerdict::FAIL);

    // ④ protocol_instances 类型非法(非 string/object/array)
    nlohmann::json bad_type;
    bad_type["cep"] = nlohmann::json{{"protocol_instances", 42}};
    CHECK(cep::run_channel_gate(bad_type).verdict == cep::GateVerdict::FAIL);

    // ⑤ 显式给了内联声明但解析出 0 实例
    nlohmann::json empty_inline;
    empty_inline["cep"] = nlohmann::json{
        {"protocol_instances", nlohmann::json{{"instances", nlohmann::json::array()}}}};
    CHECK(cep::run_channel_gate(empty_inline).verdict == cep::GateVerdict::FAIL);

    // ⑥ 空实例数组(数组形态)
    nlohmann::json empty_array;
    empty_array["cep"] = nlohmann::json{{"protocol_instances", nlohmann::json::array()}};
    CHECK(cep::run_channel_gate(empty_array).verdict == cep::GateVerdict::FAIL);

    // ⑦ `instances` 数组里混入非对象项 ⇒ FAIL(不得 PASS 且 0 顶点 0 边 —— 那是绿灯无牙)
    nlohmann::json junk_items;
    junk_items["cep"] = nlohmann::json{
        {"protocol_instances",
         nlohmann::json{{"instances", nlohmann::json::array({42, "junk", nullptr})}}}};
    const auto r7 = cep::run_channel_gate(junk_items);
    CHECK(r7.verdict == cep::GateVerdict::FAIL);
    CHECK(r7.to_log().find("不是对象") != std::string::npos);
    // ordering gate 必须**对称**判 FAIL(否则日志出现「配置错误 / 没有实例」自相矛盾)
    CHECK(cep::run_ordering_gate(junk_items).verdict == cep::GateVerdict::FAIL);

    // ⑧ 实例对象缺非空 `id` ⇒ FAIL(同样不得当作可检查实例)
    nlohmann::json no_id;
    no_id["cep"] =
        nlohmann::json{{"protocol_instances",
                        nlohmann::json{{"instances", nlohmann::json::array({{{"family", "x"}}})}}}};
    CHECK(cep::run_channel_gate(no_id).verdict == cep::GateVerdict::FAIL);
    CHECK(cep::run_channel_gate(no_id).to_log().find("缺少非空") != std::string::npos);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 7:两个挂点共享同一判据(禁双实现)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP ChannelGate: 双挂共享同一判据实现", "[cep][channel-gate]") {
    const nlohmann::json cfg = cep_config(repo_declaration_json());

    // ① module_factory 挂点入口与「显式解析 + 判据」两条路径产出同一报告
    const auto via_config = cep::run_channel_gate(cfg);
    const auto via_explicit =
        cep::evaluate_channel_dependency_gate(cep::parse_channel_declaration_from_config(cfg));
    CHECK(via_config.verdict == via_explicit.verdict);
    CHECK(via_config.stats == via_explicit.stats);
    CHECK(via_config.to_log() == via_explicit.to_log());

    const auto ord_via_config = cep::run_ordering_gate(cfg);
    const auto ord_explicit =
        cep::evaluate_ordering_points(cep::parse_channel_declaration_from_config(cfg));
    CHECK(ord_via_config.to_log() == ord_explicit.to_log());

    // ② 离线挂点(validate_topology 追加的 CLI)与上述路径结论一致
    const auto cli = run_binary("cep_channel_dependency_gate");
    if (!cli.spawned) {
        WARN("cep_channel_dependency_gate CLI 不可用(未构建?)—— CLI 一致性部分 SKIP");
        return;
    }
    CHECK(cli.rc == 0);
    // ⚠️ CLI 汇总行的顶点/边数随实例集变化 ⇒ **不断言字面量**;
    //    改为断言「汇总行存在 + 各实例行都在」(逐实例行数 = 实例数)
    CHECK(cli.out.find("[PASS] cep channel dependency gate:") != std::string::npos);
    CHECK(cli.out.find("[per-instance] instance=chi_rnf ") != std::string::npos);
    CHECK(cli.out.find("[per-instance] instance=mesif ") != std::string::npos);
    CHECK(cli.out.find("[per-instance] instance=moesi_gpu ") != std::string::npos);
    CHECK(cli.out.find("[verdict] net=compute_noc vc=0 PASS (") != std::string::npos);
    CHECK(cli.out.find("[PASS] cep ordering point checker:") != std::string::npos);
}
