// test/test_cep_instance_declaration_contract.cc
// 协议实例**声明侧** + 装配期闸的语义不变量:
//   · INV-CEP-12-IC1 —— 实例记录必须含 7 字段组(共 13 顶层属性)
//   · INV-CEP-04-IC7 —— token 类协议下 Sharer Tracker 不跟踪 sharer set
//   · INV-CEP-09-IC1 —— 每个实例组合必须通过**装配期**无环性检查
//   · INV-CEP-14-IC3 —— 组合接缝检查项缺失时,判定必须 fail(不得静默跳过)
//   · INV-CEP-13-IC2 —— 跨域写传播必须由「域可见写」判据 (a)+(b) 触发
//
// ⚠️ 全部**行为/结构**断言(⛔ 不是「ADR 里写着这句话」):
//   变红条件分别是「实例缺字段」「BroadcastNull 开始记 sharer set」「有环却 PASS」
//   「缺检查项却 PASS」「写传播简化为全传播/全不传播」。
//
// 作者: CppTLM Team / 日期: 2026-11-05
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/broadcast_null_tracker.hh"
#include "tlm/cep/channel_dependency_graph.hh"
#include "tlm/cep/moesi_gpu_adapter.hh"
#include "tlm/cep/ordering_point_checker.hh"
#include "tlm/cep/sharer_tracker.hh"

using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    using nlohmann::json;

    json load_json(const char* rel) {
        const auto text = read_file((repo_root() / rel).string());
        if (text.empty())
            return json::object();
        return json::parse(text);
    }

    // ── INV-CEP-12-IC1:7 字段组 = 13 顶层属性 ────────────────────────────
    const std::vector<std::vector<std::string>>& field_groups() {
        static const std::vector<std::vector<std::string>> g = {
            {"id", "family", "adapter", "status", "protocol_version"},
            {"flow_support"},
            {"perm", "atomic"},
            {"consistency_labels"},
            {"ordering_assumption", "vc_binding"},
            {"sharer_tracker"},
            {"combination_capabilities"},
        };
        return g;
    }

    std::vector<std::string> missing_top_level(const json& inst,
                                               const std::vector<std::string>& required) {
        std::vector<std::string> out;
        for (const auto& k : required) {
            if (!inst.contains(k))
                out.push_back(k);
        }
        return out;
    }

} // namespace

// INV-CARRIER: INV-CEP-12-IC1
// 变红理由(per ADR-CEP-06 SU#4 规则 B):实例记录缺 13 顶层属性之一(字段组塌缩)⇒ 红
TEST_CASE("INV-CEP-12-IC1:每实例含 7 字段组共 13 顶层属性", "[cep][instance-decl]") {
    const auto schema = load_json("include/tlm/cep/protocol_instance.schema.json");
    REQUIRE(schema.contains("required"));
    const auto& required = schema.at("required");
    // ① schema 的 required 恰 13 项,且与 7 字段组**逐名一致**
    REQUIRE(required.size() == 13u);
    REQUIRE(field_groups().size() == 7u);
    std::set<std::string> from_schema;
    for (const auto& v : required)
        from_schema.insert(v.get<std::string>());
    std::set<std::string> from_groups;
    for (const auto& g : field_groups())
        from_groups.insert(g.begin(), g.end());
    INFO("7 字段组的并集与 schema required 不一致");
    REQUIRE(from_schema == from_groups);
    REQUIRE(from_groups.size() == 13u);

    // ② 每个实例都齐备 13 个顶层属性
    const auto decl = load_json("include/tlm/cep/protocol_instances.json");
    REQUIRE(decl.contains("instances"));
    std::size_t n = 0;
    const std::vector<std::string> req(required.begin(), required.end());
    for (const auto& inst : decl.at("instances")) {
        const auto miss = missing_top_level(inst, req);
        std::string joined;
        for (const auto& m : miss) {
            if (!joined.empty())
                joined += ",";
            joined += m;
        }
        INFO("实例缺顶层属性: " << inst.value("id", std::string("?")) << " ⇒ " << joined);
        REQUIRE(miss.empty());
        ++n;
    }
    REQUIRE(n == 7u);

    // ③ 反向 fixture:缺 `atomic` 的实例 ⇒ 检查器必须判否(证明非恒真)
    {
        json bad = decl.at("instances").at(0);
        bad.erase("atomic");
        REQUIRE_FALSE(missing_top_level(bad, req).empty());
    }
    // ④ 反向 fixture:缺 `vc_binding` 的实例 ⇒ 必须判否
    {
        json bad = decl.at("instances").at(0);
        bad.erase("vc_binding");
        REQUIRE_FALSE(missing_top_level(bad, req).empty());
    }
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-04-IC7 —— token 类协议下 Sharer Tracker **不跟踪 sharer set**
//   (token 由数据副本携带;实现上等价 `BroadcastNull` 或 **不参与**)
// ═════════════════════════════════════════════════════════════════════
namespace {
    // 「不跟踪 sharer set」的合法形态:impl = BroadcastNull / 不参与,且 caps.scope = NONE
    bool is_non_tracking_form(const std::string& impl, const std::string& scope) {
        const bool impl_ok = (impl == "BroadcastNull" || impl == "不参与");
        return impl_ok && scope == "NONE";
    }
} // namespace

// 「token 形态却跟踪 sharer set」⇒ 红
TEST_CASE("INV-CEP-04-IC7:token 类协议不跟踪 sharer set(BroadcastNull 退化基线)",
          "[cep][instance-decl]") {
    // ① 行为:`BroadcastNullTracker` **恒不持有** sharer 状态
    tlm::cep::BroadcastNullTracker bt;
    for (std::uint64_t addr : {0x0ull, 0x1000ull, 0xDEAD0000ull}) {
        INFO("BroadcastNull 不得持有任何 sharer 状态: " << addr);
        // INV-CARRIER: INV-CEP-04-IC7
        // 变红理由(per ADR-CEP-06 SU#4 规则 B):BroadcastNull 开始维护 sharer set / 声明侧出现
        REQUIRE(bt.possible_sharer_count(addr) == 0u);
        // lookup 恒广播 + 无本地视图(**不**依赖出参默认值:显式断言三项)
        tlm::cep::LookupResult result = tlm::cep::LookupResult::ANSWER_HERE;
        tlm::cep::LocalView view;
        tlm::cep::LookupHazard hazard = tlm::cep::LookupHazard::STALL;
        bt.lookup(addr, tlm::cep::Intent::want_read, result, view, hazard);
        REQUIRE(result == tlm::cep::LookupResult::BROADCAST);
        REQUIRE_FALSE(view.valid);
        REQUIRE(view.sharers == 0u);
    }

    // ② 声明侧:凡是「不跟踪」形态的实例,其 caps.scope 必须是 NONE
    const auto decl = load_json("include/tlm/cep/protocol_instances.json");
    std::size_t non_tracking = 0;
    for (const auto& inst : decl.at("instances")) {
        const auto& st = inst.at("sharer_tracker");
        const std::string impl = st.value("impl", std::string("?"));
        const std::string scope = st.at("caps").value("scope", std::string("?"));
        if (impl == "BroadcastNull" || impl == "不参与") {
            ++non_tracking;
            INFO("不跟踪形态的实例必须 scope=NONE: " << inst.value("id", std::string("?")));
            REQUIRE(is_non_tracking_form(impl, scope));
        }
    }
    INFO("声明侧应至少有一个不跟踪形态(BroadcastNull / 不参与)");
    REQUIRE(non_tracking >= 1u);

    // ③ 反向 fixture(判别力):「跟踪 sharer set」的形态必须**不被**判为不跟踪
    REQUIRE_FALSE(is_non_tracking_form("FullMapDirectory", "SYSTEM"));
    REQUIRE_FALSE(is_non_tracking_form("BroadcastNull", "SYSTEM")); // impl 对但 scope 错
    REQUIRE_FALSE(is_non_tracking_form("TreeManager", "NONE"));     // scope 对但 impl 错
    // ④ 正向对照(证明 ③ 不是「恒 false」)
    REQUIRE(is_non_tracking_form("BroadcastNull", "NONE"));
    REQUIRE(is_non_tracking_form("不参与", "NONE"));
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-09-IC1 —— 每个协议实例组合**必须**通过装配期无环性检查
// ═════════════════════════════════════════════════════════════════════
namespace {

    // 以真实声明为底,改写 `channel_dependency_edges.per_network` 后落临时文件
    std::string write_edges_variant(const json& decl, const json& per_network_edges) {
        json d = decl;
        d["channel_dependency_edges"]["per_network"] = per_network_edges;
        const auto path = std::filesystem::temp_directory_path() / "cep_ic9_edges.json";
        std::ofstream out(path);
        out << d.dump(2);
        return path.string();
    }

} // namespace

// INV-CARRIER: INV-CEP-09-IC1
// 变红理由(per ADR-CEP-06 SU#4 规则 B):有环的声明组合被判 PASS(或装配期不检查)⇒ 红
TEST_CASE("INV-CEP-09-IC1:装配期通道依赖图无环性检查(有环 ⇒ 配置 fail)", "[cep][instance-decl]") {
    const auto decl = load_json("include/tlm/cep/protocol_instances.json");
    REQUIRE(decl.contains("channel_dependency_edges"));
    const auto real_edges = decl.at("channel_dependency_edges").at("per_network");

    auto gate_for = [](const std::string& path) {
        json cfg;
        cfg["cep"] = json{{"protocol_instances", path}};
        return tlm::cep::run_channel_gate(cfg);
    };

    // ① 正控:真实声明(无环)⇒ PASS(既非 FAIL 也非 SKIP)
    const auto real_path = write_edges_variant(decl, real_edges);
    const auto ok = gate_for(real_path);
    INFO("真实声明应通过无环检查: " << ok.to_log().substr(0, 160));
    REQUIRE(ok.verdict == tlm::cep::GateVerdict::PASS);

    // ② 判据主体:人造 **2-环**(0→1,1→0)⇒ 必须 FAIL
    json cyc = real_edges;
    cyc["compute_noc"] = json::array({json::array({0, 1}), json::array({1, 0})});
    const auto cyc_path = write_edges_variant(decl, cyc);
    const auto bad = gate_for(cyc_path);
    INFO("有环声明必须 FAIL,实际: " << bad.to_log().substr(0, 200));
    REQUIRE(bad.verdict == tlm::cep::GateVerdict::FAIL);
    // 且日志给出环(可归因,不是裸 FAIL)
    REQUIRE((bad.to_log().find("cycle") != std::string::npos ||
             bad.to_log().find("环") != std::string::npos));

    // ③ 对照:同一声明改成**无环**(0→1→2)⇒ 恢复 PASS(证明 FAIL 出自「环」而非其他)
    json acyc = real_edges;
    acyc["compute_noc"] = json::array({json::array({0, 1}), json::array({1, 2})});
    const auto acyc_path = write_edges_variant(decl, acyc);
    REQUIRE(gate_for(acyc_path).verdict == tlm::cep::GateVerdict::PASS);
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-14-IC3 —— 组合接缝检查项**缺失**时,判定必须 fail(不得静默跳过)
// ═════════════════════════════════════════════════════════════════════
// INV-CARRIER: INV-CEP-14-IC3
// 变红理由(per ADR-CEP-06 SU#4 规则 B):ic2_item_present == false 时判定仍 PASS
// (静默跳过组合接缝检查项)⇒ 红
TEST_CASE("INV-CEP-14-IC3:组合接缝检查项缺失 ⇒ 判定 fail(不静默跳过)", "[cep][instance-decl]") {
    using namespace tlm::cep;

    CriterionResult j1;
    j1.criterion = CriterionId::J1_SinglePoint;
    j1.instance = "demo";
    j1.verdict = GateVerdict::PASS;
    const std::vector<CriterionResult> with_ic2{j1};

    // ① 缺检查项(ic2_item_present = false)⇒ **FAIL**(判据主体)
    INFO("检查项缺失必须判 fail —— 不得静默跳过");
    REQUIRE(derive_ordering_verdict(with_ic2, /*ic2_item_present=*/false) == GateVerdict::FAIL);

    // ② 有检查项 + 全 PASS ⇒ PASS
    REQUIRE(derive_ordering_verdict(with_ic2, true) == GateVerdict::PASS);

    // ③ 有检查项但有 FAIL ⇒ FAIL
    {
        auto bad = with_ic2;
        bad[0].verdict = GateVerdict::FAIL;
        REQUIRE(derive_ordering_verdict(bad, true) == GateVerdict::FAIL);
    }
    // ④ 未评估(空结果)⇒ SKIP,与 FAIL 可区分
    REQUIRE(derive_ordering_verdict({}, false) == GateVerdict::SKIP);
    REQUIRE(derive_ordering_verdict({}, false) != GateVerdict::FAIL);

    // ⑤ 真实路径:实际闸的 ic2_item_present 必须为 true(否则真声明就在静默跳过)
    json cfg;
    cfg["cep"] = json::object();
    const auto rep = run_ordering_gate(cfg);
    REQUIRE(rep.verdict != GateVerdict::SKIP);
    INFO("真实声明的组合接缝检查项缺失 ⇒ 闸已在静默跳过");
    REQUIRE(rep.ic2_item_present);
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-13-IC2 —— 跨域写传播必须由「域可见写」判据 (a)+(b) 触发
//   不得以「所有写都传播」或「任何写都不传播」简化。
// ═════════════════════════════════════════════════════════════════════
namespace {
    using MA = tlm::cep::MoesiGpuAdapter;

    // 判据 (a):终态 = M 的静默升级(代码侧 `is_silent_upgrade`);
    // 判据 (b):目标态接受「导致数据值响应的转发请求」(代码侧 `snoop_response_carries_data`)。
    // ⇒ 域可见写 = (a) **且** (b)。
    bool is_domain_visible_write(MA::State s, MA::Event e) {
        if (!MA::is_silent_upgrade(s, e))
            return false;                                     // (a)
        return MA::snoop_response_carries_data(MA::State::M); // (b)
    }

    // 反向 fixture 用的「坏 adapter」:(b) 不成立(目标态不携带数据值)
    struct BadAdapterNoDataOnSnoop {
        static constexpr bool snoop_response_carries_data(MA::State) noexcept {
            return false;
        }
        static constexpr bool is_silent_upgrade(MA::State s, MA::Event e) noexcept {
            return MA::is_silent_upgrade(s, e);
        }
    };
    template <class A> bool is_domain_visible_write_with(const A&, MA::State s, MA::Event e) {
        return A::is_silent_upgrade(s, e) && A::snoop_response_carries_data(MA::State::M);
    }
} // namespace

// INV-CARRIER: INV-CEP-13-IC2
// 变红理由(per ADR-CEP-06 SU#4 规则 B):写传播被简化为「所有写都传播」(非 E 态的写也
// 传播)或「任何写都不传播」(E+Wr 不传播)/ 缺 (b) 条件 ⇒ 红
TEST_CASE("INV-CEP-13-IC2:跨域写传播由判据 (a)+(b) 触发(非全传播 / 非全不传播)",
          "[cep][instance-decl]") {
    // ① 不是「任何写都不传播」:至少有一个 (状态,写事件) 触发传播
    REQUIRE(is_domain_visible_write(MA::State::E, MA::Event::Wr));
    // ② 不是「所有写都传播」:其余状态的写**不**触发静默升级 ⇒ 逐态核对
    for (MA::State s : {MA::State::I, MA::State::S, MA::State::M, MA::State::O, MA::State::T}) {
        INFO("非 E 态的写不得判为静默升级,state=" << static_cast<int>(s));
        REQUIRE_FALSE(MA::is_silent_upgrade(s, MA::Event::Wr));
    }
    // ③ (b) 是**必要**条件:去掉「目标态携带数据值」⇒ 整体不成立
    //    (证明判据不是只有 (a) —— 缺 (b) 会把 RC 的读请求误判为写,per ADR-CEP-13 §4 抓取点 2)
    REQUIRE_FALSE(
        is_domain_visible_write_with(BadAdapterNoDataOnSnoop{}, MA::State::E, MA::Event::Wr));
    // ④ 反向 fixture:非写事件(Snoop 响应)不触发传播
    for (MA::Event e :
         {MA::Event::Rd, MA::Event::Inv, MA::Event::InvAck, MA::Event::Fl, MA::Event::FlAck}) {
        INFO("非写事件不得触发跨域写传播,event=" << static_cast<int>(e));
        REQUIRE_FALSE(MA::is_silent_upgrade(MA::State::E, e));
    }
}
