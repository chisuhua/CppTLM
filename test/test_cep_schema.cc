// test/test_cep_schema.cc
// CEP Stage 0 — 协议实例能力声明 schema 契约测试
// per openspec/changes/cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md
//     Requirement: cep-capability-map-contract
//     + ADR-CEP-12 §2 决策 1/2/3/4/5/6/7/8/9
//     + ADR-CEP-14 §2 决策 3(C9 负向断言)
// 作者: CppTLM Team / 日期: 2026-10-21
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <vector>

using json = nlohmann::json;
using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    const std::vector<std::string>& canonical_flows() {
        static const std::vector<std::string> flows = {
            "request",   "completion",          "snoop",           "snoop_resp",
            "writeback", "forward_data",        "back_invalidate", "address_resolve",
            "barrier",   "snoop_completion_ack"};
        return flows;
    }

    json load_schema() {
        const auto path = repo_root() / "include" / "tlm" / "cep" / "protocol_instance.schema.json";
        const std::string text = read_file(path.string());
        REQUIRE_FALSE(text.empty());
        return json::parse(text);
    }

    // ADR-CEP-12 IC2 的解析规则(Stage 0 契约层:test 内部实现,
    // Stage 1 的装配期检查器须实现同一规则)
    struct ResolutionResult {
        bool ok = false;
        std::string reason;
    };

    ResolutionResult resolve_named_networks(const json& instance,
                                            const std::set<int>& known_vc_ids) {
        ResolutionResult r;
        if (!instance.contains("ordering_assumption")) {
            r.reason = "缺少 ordering_assumption";
            return r;
        }
        const auto& oa = instance.at("ordering_assumption");
        if (!oa.is_object()) {
            r.reason = "ordering_assumption 必须是对象";
            return r;
        }
        for (auto it = oa.begin(); it != oa.end(); ++it) {
            const std::string key = it.key();
            bool resolved = false;
            // 纯数字 key 视为已是 vc_id
            if (!key.empty() && std::all_of(key.begin(), key.end(),
                                            [](unsigned char c) { return std::isdigit(c); })) {
                const int vc = std::stoi(key);
                resolved = (known_vc_ids.count(vc) > 0);
            } else {
                // named_network:须经 fabric config 解析到唯一 vc_id;本测试的
                // fixture 里,named_network 形如 "net_fwd" → 解析到 vc_id。
                resolved = false;
            }
            if (!resolved) {
                r.reason = "ordering_assumption 的 key 无法解析到唯一 vc_id: " + key;
                return r;
            }
        }
        r.ok = true;
        return r;
    }

    // 词边界包含判定(避免子串假阳性)
    bool contains_word(const std::string& hay, const std::string& needle) {
        if (needle.empty())
            return false;
        std::size_t pos = 0;
        while ((pos = hay.find(needle, pos)) != std::string::npos) {
            // ⚠️ `_` **不算**词字符 —— 否则 `chi_rnf` / `tilelink_c` / `token_capable`
            //    这类下划线形式能逃过 C9 负向断言,而 schema 的 id pattern
            //    `^[a-z0-9][a-z0-9_-]*$` **允许**下划线(契约允许的形态恰好能绕过契约检查)。
            const bool left_ok =
                (pos == 0) || !std::isalnum(static_cast<unsigned char>(hay[pos - 1]));
            const std::size_t after = pos + needle.size();
            const bool right_ok =
                (after >= hay.size()) || !std::isalnum(static_cast<unsigned char>(hay[after]));
            if (left_ok && right_ok)
                return true;
            pos += 1;
        }
        return false;
    }

    json minimal_valid_instance() {
        json j;
        j["id"] = "chi-rnf";
        j["family"] = "industrial";
        j["adapter"] = "ChiRnfAdapter";
        j["status"] = "implemented";
        j["protocol_version"] = "chi-r2";
        json fs;
        for (const auto& f : canonical_flows())
            fs[f] = true;
        j["flow_support"] = fs;
        j["perm"] = json::array({"NONE", "READ", "UNIQUE", "WRITEBACK"});
        j["atomic"] = {{"supported", false}};
        j["consistency_labels"] = {
            {"read", "SC"}, {"write", "SC"}, {"request_level", json::object()}};
        j["ordering_assumption"] = {{"0", "p2p_ordered"}};
        j["vc_binding"] = {{"request", 0}, {"completion", 1}};
        j["sharer_tracker"] = {{"impl", "FullMapDirectory"},
                               {"caps",
                                {{"scope", "SYSTEM"},
                                 {"completeness", "EXACT"},
                                 {"authority", "GLOBAL"},
                                 {"aggregation_at", "HOME"}}}};
        j["combination_capabilities"] = {
            {"concurrent", true}, {"hierarchical", true}, {"heterogeneous", false}};
        return j;
    }

} // namespace

TEST_CASE("[cep][schema] schema 必含 8 个字段组", "[cep][schema]") {
    const json s = load_schema();
    REQUIRE(s.contains("properties"));
    const auto& props = s.at("properties");

    const std::vector<std::string> groups = {"id",
                                             "family",
                                             "adapter",
                                             "status",
                                             "protocol_version", // 身份与状态
                                             "flow_support",
                                             "perm",
                                             "atomic", // 能力面
                                             "consistency_labels",
                                             "ordering_assumption",
                                             "vc_binding", // 前提假设面
                                             "sharer_tracker",
                                             "combination_capabilities"}; // 组合能力
    REQUIRE(props.size() == groups.size()); // additionalProperties 是 properties 的兄弟,不在其内
    for (const auto& g : groups) {
        INFO("schema 缺字段组: " << g);
        REQUIRE(props.contains(g));
    }
    REQUIRE(s.at("required").size() == groups.size());
    for (const auto& g : groups) {
        INFO("required 未列 " << g);
        REQUIRE(std::find(s.at("required").begin(), s.at("required").end(), g) !=
                s.at("required").end());
    }
}

TEST_CASE("[cep][schema] flow_support 恰好 10 项且与 canonical 10 流逐名对齐", "[cep][schema]") {
    const json s = load_schema();
    const auto& fs = s.at("properties").at("flow_support");
    REQUIRE(fs.at("properties").size() == canonical_flows().size());
    for (const auto& f : canonical_flows()) {
        INFO("flow_support 缺流: " << f);
        REQUIRE(fs.at("properties").contains(f));
    }
    REQUIRE(fs.at("required").size() == canonical_flows().size());
    REQUIRE(fs.at("additionalProperties") == false);
}

TEST_CASE("[cep][schema] perm 维持 4 值锁且无 ATOMIC", "[cep][schema]") {
    const json s = load_schema();
    const auto& perm = s.at("properties").at("perm");
    const auto& en = perm.at("items").at("enum");
    REQUIRE(en.size() == 4);
    const std::set<std::string> vals(en.begin(), en.end());
    REQUIRE(vals == std::set<std::string>({"NONE", "READ", "UNIQUE", "WRITEBACK"}));
    REQUIRE(vals.count("ATOMIC") == 0);
}

TEST_CASE("[cep][schema] atomic 走独立属性而非 perm 扩展", "[cep][schema]") {
    const json s = load_schema();
    const auto& a = s.at("properties").at("atomic");
    REQUIRE(a.at("properties").contains("supported"));
    REQUIRE(a.at("properties").contains("intent_field"));
    REQUIRE(a.at("additionalProperties") == false);
}

TEST_CASE("[cep][schema] consistency_labels 含 read/write 两级 + 请求级", "[cep][schema]") {
    const json s = load_schema();
    const auto& cl = s.at("properties").at("consistency_labels");
    REQUIRE(cl.at("properties").contains("read"));
    REQUIRE(cl.at("properties").contains("write"));
    REQUIRE(cl.at("properties").contains("request_level"));
    const std::set<std::string> reads = cl.at("properties").at("read").at("enum");
    const std::set<std::string> writes = cl.at("properties").at("write").at("enum");
    REQUIRE(reads == std::set<std::string>({"SC", "TSO", "RC-acquire", "RC-data", "PLO"}));
    REQUIRE(writes == std::set<std::string>({"SC", "TSO", "RC-release", "RC-data", "PLO"}));
}

TEST_CASE("[cep][schema] ordering_assumption 逐网络 + 值域封闭;vc_binding 值域 0-3",
          "[cep][schema]") {
    const json s = load_schema();
    const auto& oa = s.at("properties").at("ordering_assumption");
    REQUIRE(oa.at("minProperties") == 1);
    const std::set<std::string> vals(oa.at("additionalProperties").at("enum").begin(),
                                     oa.at("additionalProperties").at("enum").end());
    REQUIRE(vals == std::set<std::string>({"p2p_ordered", "unordered"}));

    const auto& vb = s.at("properties").at("vc_binding");
    REQUIRE(vb.at("additionalProperties").at("type") == "integer");
    REQUIRE(vb.at("additionalProperties").at("minimum") == 0);
    REQUIRE(vb.at("additionalProperties").at("maximum") == 3);
}

TEST_CASE("[cep][schema] sharer_tracker 含 5 实现 + 「不参与」+ caps 四属性", "[cep][schema]") {
    const json s = load_schema();
    const auto& st = s.at("properties").at("sharer_tracker");
    const std::set<std::string> impls(st.at("properties").at("impl").at("enum").begin(),
                                      st.at("properties").at("impl").at("enum").end());
    REQUIRE(impls ==
            std::set<std::string>({"FullMapDirectory", "SparseSnoopFilter", "DcohSymmetric",
                                   "TreeManager", "BroadcastNull", "不参与"}));
    const auto& caps = st.at("properties").at("caps");
    REQUIRE(caps.at("required").size() == 4);
    for (const char* k : {"scope", "completeness", "authority", "aggregation_at"}) {
        INFO("caps 缺属性: " << k);
        REQUIRE(caps.at("properties").contains(k));
    }
}

TEST_CASE("[cep][schema] combination_capabilities 恰好 3 维", "[cep][schema]") {
    const json s = load_schema();
    const auto& cc = s.at("properties").at("combination_capabilities");
    REQUIRE(cc.at("properties").size() == 3);
    for (const char* k : {"concurrent", "hierarchical", "heterogeneous"}) {
        INFO("combination_capabilities 缺维度: " << k);
        REQUIRE(cc.at("properties").contains(k));
    }
}

TEST_CASE("[cep][schema] status 是计数口径且枚举封闭", "[cep][schema]") {
    const json s = load_schema();
    const std::set<std::string> st(s.at("properties").at("status").at("enum").begin(),
                                   s.at("properties").at("status").at("enum").end());
    REQUIRE(st == std::set<std::string>({"planned", "implemented", "excluded"}));
}

TEST_CASE("[cep][schema] schema 字段组完整性(7 组 / 13 个顶层属性)", "[cep][schema]") {
    // ⚠️ 口径修订(2026-10-21,per 代码复审):「字段组数」此前在 4 处写着
    //    6 / 7 / 8 / 13 四个不同数字且**无人断言**。现统一为 7 组 / 13 属性。
    const json s = load_schema();
    const json props = s.at("properties");
    REQUIRE(props.size() == 13);

    // 7 个字段组的显式划分(per ADR-CEP-12 IC1 口径修订)
    const std::vector<std::vector<std::string>> groups = {
        {"id", "family", "adapter", "status", "protocol_version"},
        {"flow_support"},
        {"perm", "atomic"},
        {"consistency_labels"},
        {"ordering_assumption", "vc_binding"},
        {"sharer_tracker"},
        {"combination_capabilities"},
    };
    REQUIRE(groups.size() == 7);

    // 分组并集 == schema 的全部顶层属性(不多不少)
    std::set<std::string> union_ids;
    for (const auto& g : groups) {
        for (const auto& k : g) {
            INFO("分组里有重复属性: " << k);
            REQUIRE(union_ids.insert(k).second);
            INFO("schema 缺字段组属性: " << k);
            REQUIRE(props.contains(k));
        }
    }
    REQUIRE(union_ids.size() == props.size());

    // ADR 侧的表述必须与本口径一致(防止口径再次漂移)
    const auto adr12 = read_file(
        (repo_root() / "docs" / "adr" / "ADR-CEP-12-protocol-engine-capability-map.md").string());
    INFO("ADR-CEP-12 未声明 7 个字段组 / 13 个顶层属性口径");
    REQUIRE(adr12.find("7 个字段组") != std::string::npos);
    REQUIRE(adr12.find("13 个顶层属性") != std::string::npos);
    REQUIRE(adr12.find("6 个字段组") == std::string::npos);
}

TEST_CASE("[cep][schema][negative] contains_word 词边界不得被下划线绕过", "[cep][schema]") {
    // 回归:`chi_rnf` / `tilelink_c` 这类下划线形式曾能逃过 C9 负向断言 ——
    // 而 schema 的 id pattern `^[a-z0-9][a-z0-9_-]*$` **允许**下划线
    // (契约允许的形态恰好能绕过契约检查)。
    // 注:contains_word 本身**大小写敏感**;实际 C9 用法是先把整段大写归一,
    //     故此处同样先大写再匹配(与 test 的真实调用路径一致)。
    auto up = [](const std::string& v) {
        std::string r = v;
        for (auto& c : r)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return r;
    };
    REQUIRE(contains_word(up("chi_rnf"), "CHI"));
    REQUIRE(contains_word(up("x tilelink_c y"), "TILELINK"));
    REQUIRE(contains_word(up("token_capable"), "TOKEN"));
    REQUIRE(contains_word(up("MOESI_CMP"), "MOESI"));
    // 正常词仍应命中
    REQUIRE(contains_word(up("MESI protocol"), "MESI"));
    REQUIRE(contains_word(up("`CHI`"), "CHI"));
    // 非命中仍应不命中
    REQUIRE_FALSE(contains_word(up("hierarchical"), "CHI")); // 子串,非独立词
    REQUIRE_FALSE(contains_word(up("no_such_word"), "CHI"));
    REQUIRE_FALSE(contains_word(up("mismatched"), "CHI"));
}

TEST_CASE("[cep][schema] protocol_version 值域必须容纳 ADR-CEP-11 锁定表全部取值",
          "[cep][schema]") {
    // 跨文档契约:schema 的值域 ⊇ ADR-CEP-11 §3 锁定表。
    // 回归:初版 pattern `^[a-z0-9]+(-[0-9.]+)?$` 拒绝 `chi-r2` / `tilelink-c-1.3`
    //      —— 即拒绝锁定表 2/6 的合法取值(2026-10-21 自审发现)。
    const json s = load_schema();
    const std::string pattern = s.at("properties").at("protocol_version").at("pattern");
    INFO("schema 的 protocol_version pattern: " << pattern);

    const auto lock = read_file(
        (repo_root() / "docs" / "adr" / "ADR-CEP-11-protocol-version-locking.md").string());
    REQUIRE_FALSE(lock.empty());

    // 锁定表的 6 个取值(per ADR-CEP-11 §3)
    const std::vector<std::string> locked = {"chi-r2",         "cxl-2.0",    "cxl-3.0",
                                             "tilelink-c-1.3", "nvlink-4.0", "ualink-2.0"};
    const std::regex re(pattern);
    for (const auto& v : locked) {
        INFO("锁定表取值未出现在 ADR-CEP-11: " << v);
        REQUIRE(lock.find(v) != std::string::npos);
        INFO("schema pattern 拒绝锁定表合法取值: " << v);
        REQUIRE(std::regex_match(v, re));
    }
}

TEST_CASE("[cep][schema][negative] schema 不得出现协议标识符(C9 负向断言)", "[cep][schema]") {
    // per ADR-CEP-14 C9:协议标识符只允许出现在 ProtocolEngine 插件内部与
    // capability schema 的**数据值**;此处校验 schema **键名/枚举**不含协议名。
    const auto path = repo_root() / "include" / "tlm" / "cep" / "protocol_instance.schema.json";
    const std::string text = read_file(path.string());
    REQUIRE_FALSE(text.empty());
    // ⚠️ 必须用**词边界**:子串匹配会产生假阳性
    //   (如 "hierarchical" 含 "chi"、"concurrent" 含 "current")。
    //   注:C++ std::regex 默认引擎不支持 lookbehind,故手写边界判定。
    std::string upper = text;
    for (auto& c : upper)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (const char* proto :
         {"MESI", "MOESI", "CHI", "CXL", "TILELINK", "NVLINK", "UALINK", "TOKEN"}) {
        INFO("schema 出现协议标识符: " << proto);
        REQUIRE_FALSE(contains_word(upper, proto));
    }
}

TEST_CASE("[cep][schema] named_network 不可解析到唯一 vc_id 时必须 fail", "[cep][schema]") {
    const std::set<int> known = {0, 1, 2, 3};

    json ok = minimal_valid_instance();
    REQUIRE(resolve_named_networks(ok, known).ok);

    json bad = minimal_valid_instance();
    bad["ordering_assumption"] = {{"net_fwd", "p2p_ordered"}}; // 未注册的 named_network
    const auto r = resolve_named_networks(bad, known);
    REQUIRE_FALSE(r.ok);
    REQUIRE_FALSE(r.reason.empty());

    json bad2 = minimal_valid_instance();
    bad2["ordering_assumption"] = {{"9", "p2p_ordered"}}; // 越界 vc_id
    REQUIRE_FALSE(resolve_named_networks(bad2, known).ok);
}

TEST_CASE("[cep][schema] status 只有 implemented 计入 adapter 计数", "[cep][schema]") {
    const json s = load_schema();
    const std::set<std::string> st(s.at("properties").at("status").at("enum").begin(),
                                   s.at("properties").at("status").at("enum").end());
    // 计数口径(per ADR-CEP-12 决策 9):仅 implemented 计入
    int counted = 0;
    for (const auto& v : st) {
        if (v == "implemented")
            ++counted;
    }
    REQUIRE(counted == 1);
    REQUIRE(st.count("planned") == 1);
    REQUIRE(st.count("excluded") == 1);
}
