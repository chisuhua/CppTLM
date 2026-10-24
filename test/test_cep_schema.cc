// test/test_cep_schema.cc
// CEP Stage 0 — 协议实例能力声明 schema 契约测试
// per openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md
//     Requirement: cep-capability-map-contract
//     + ADR-CEP-12 §2 决策 1/2/3/4/5/6/7/8/9
//     + ADR-CEP-14 §2 决策 3(C9 负向断言)
// 作者: CppTLM Team / 日期: 2026-10-21(2026-10-22 契约演进:ordering_assumption /
//       vc_binding 改 per-network 嵌套形态,per ADR-CEP-12 决策 5 修订 + ArchForge N-2 裁决)
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
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

    // fabric config 的**类型描述**(per ADR-CEP-12 决策 5 修订)
    json load_fabric_schema() {
        const auto path = repo_root() / "include" / "tlm" / "cep" / "fabric.schema.json";
        const std::string text = read_file(path.string());
        REQUIRE_FALSE(text.empty());
        return json::parse(text);
    }

    // fabric config 的**实例数据**(派生自 ArchForge 22-nsa §0.5.4.1)
    json load_fabric_instance() {
        const auto path = repo_root() / "include" / "tlm" / "cep" / "fabric.json";
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

    // fabric 配置的 **network → vc_count** 映射(Stage 1 装配期检查器的输入形态;
    // 本仓实例 = include/tlm/cep/fabric.json —— 派生自 ArchForge 22-nsa §0.5.4.1)。
    //
    // ⚠️ 2026-10-22 契约演进(per ADR-CEP-12 决策 5 修订 + ArchForge 32 号 N-2 裁决选项 A):
    //    `ordering_assumption` 由「key = <vc_id | named_network> 的**扁平** map」
    //    改为「key = **network**,值 = {vc_id → ordering} 的**嵌套** map」。
    //    → 「named_network 解析到**唯一** vc_id」这个谓词**整体消失**:
    //      network 本身就是 key,其下每个 key 直接就是该网络的槽位标识。
    //    → 槽位上界由 fabric 侧的 `vc_count` 给出(不再由 schema 的硬编码上界兜)。
    using NetworkMap = std::map<std::string, int>; // network → vc_count

    // 规范消息类词表(per ADR-CEP-12 §8.3 IC2 ③;ArchForge 32 号 §6.0.1 的 6 类)
    const std::set<std::string>& canonical_msg_classes() {
        static const std::set<std::string> cls = {"request",    "response", "snoop",
                                                  "snoop_resp", "atomic",   "query"};
        return cls;
    }

    ResolutionResult resolve_ordering_assumptions(const json& instance,
                                                  const std::set<int>& known_vc_ids,
                                                  const NetworkMap& fabric_networks) {
        ResolutionResult r;
        if (!instance.contains("ordering_assumption")) {
            r.reason = "缺少 ordering_assumption";
            return r;
        }
        if (!instance.contains("vc_binding")) {
            r.reason = "缺少 vc_binding";
            return r;
        }
        const auto& oa = instance.at("ordering_assumption");
        const auto& vb = instance.at("vc_binding");
        if (!oa.is_object()) {
            r.reason = "ordering_assumption 必须是对象";
            return r;
        }
        if (!vb.is_object()) {
            r.reason = "vc_binding 必须是对象";
            return r;
        }
        // IC2 ③:消息类 ∈ 6 类规范词表
        for (auto vit = vb.begin(); vit != vb.end(); ++vit) {
            if (!vit.value().is_object()) {
                r.reason = "vc_binding." + vit.key() + " 必须是对象(network→{消息类→vc_id})";
                return r;
            }
            for (auto mit = vit.value().begin(); mit != vit.value().end(); ++mit) {
                if (canonical_msg_classes().count(mit.key()) == 0) {
                    r.reason =
                        "vc_binding." + vit.key() + "." + mit.key() + ": 消息类不在 6 类规范词表";
                    return r;
                }
            }
        }
        for (auto nit = oa.begin(); nit != oa.end(); ++nit) {
            const std::string network = nit.key();
            const auto& per_net = nit.value();
            if (!per_net.is_object()) {
                r.reason =
                    "ordering_assumption." + network + " 必须是对象(network→{vc_id→ordering})";
                return r;
            }
            const auto fit = fabric_networks.find(network);
            if (fit == fabric_networks.end()) {
                r.reason = "ordering_assumption 的网络未在 fabric 命名空间登记: " + network;
                return r;
            }
            const int vc_count = fit->second;
            // IC2 ①:该网络的 vc_binding 值域(用于 ⊆ 判定)
            std::set<int> bound;
            if (vb.contains(network) && vb.at(network).is_object()) {
                for (auto vit = vb.at(network).begin(); vit != vb.at(network).end(); ++vit) {
                    if (vit.value().is_number_integer())
                        bound.insert(vit.value().get<int>());
                }
            }
            for (auto vit = per_net.begin(); vit != per_net.end(); ++vit) {
                const std::string key = vit.key();
                if (key.empty() || !std::all_of(key.begin(), key.end(),
                                                [](unsigned char c) { return std::isdigit(c); })) {
                    r.reason = "ordering_assumption." + network + " 的 key 非 vc_id: " + key;
                    return r;
                }
                const int vc = std::stoi(key);
                if (known_vc_ids.count(vc) == 0) {
                    r.reason = "ordering_assumption." + network + " 的 vc_id 不在已声明域: " + key;
                    return r;
                }
                if (vc >= vc_count) {
                    r.reason =
                        "ordering_assumption." + network + " 的 vc_id 超出该网络槽位上界: " + key;
                    return r;
                }
                // IC2 ①:ordering 的每个 vc_id 必须出现在**同网络**的 vc_binding 值域中
                if (bound.count(vc) == 0) {
                    r.reason = "ordering_assumption." + network + "." + key +
                               ": vc_id 不在同网络 vc_binding 的值域中(IC2 ①)";
                    return r;
                }
                if (!vit.value().is_string()) {
                    r.reason = "ordering_assumption." + network + "." + key + " 的取值必须是字符串";
                    return r;
                }
                const std::string ordering = vit.value().get<std::string>();
                if (ordering != "p2p_ordered" && ordering != "unordered") {
                    r.reason =
                        "ordering_assumption." + network + "." + key + " 的取值非法: " + ordering;
                    return r;
                }
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
        // ⚠️ 2026-10-22:改为 **per-network 嵌套形态**(per ADR-CEP-12 决策 5 修订)
        j["ordering_assumption"] = {{"net_fwd", {{"0", "p2p_ordered"}}}};
        j["vc_binding"] = {{"net_fwd", {{"request", 0}, {"response", 1}}}};
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

TEST_CASE("[cep][schema] schema 必含 13 个顶层属性(7 字段组的划分见下一用例)", "[cep][schema]") {
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

TEST_CASE("[cep][schema] ordering_assumption / vc_binding 为 per-network 嵌套形态(IC2 结构)",
          "[cep][schema]") {
    const json s = load_schema();

    // ordering_assumption: network → {vc_id → ordering}
    // ⚠️ 2026-10-22 契约演进(per ADR-CEP-12 决策 5 修订 + ArchForge N-2 裁决选项 A):
    //    形态由扁平(key = <vc_id | named_network>)改为嵌套(key = network)。
    const auto& oa = s.at("properties").at("ordering_assumption");
    REQUIRE(oa.at("minProperties") == 1);
    REQUIRE(oa.at("additionalProperties").at("type") == "object");
    const auto& oa_inner = oa.at("additionalProperties");
    REQUIRE(oa_inner.at("minProperties") == 1);
    const std::set<std::string> vals(oa_inner.at("additionalProperties").at("enum").begin(),
                                     oa_inner.at("additionalProperties").at("enum").end());
    REQUIRE(vals == std::set<std::string>({"p2p_ordered", "unordered"}));

    // vc_binding: network → {消息类 → vc_id}
    const auto& vb = s.at("properties").at("vc_binding");
    REQUIRE(vb.at("minProperties") == 1);
    REQUIRE(vb.at("additionalProperties").at("type") == "object");
    const auto& vb_inner = vb.at("additionalProperties");
    REQUIRE(vb_inner.at("minProperties") == 1);
    REQUIRE(vb_inner.at("additionalProperties").at("type") == "integer");
    REQUIRE(vb_inner.at("additionalProperties").at("minimum") == 0);
    REQUIRE(vb_inner.at("additionalProperties").at("maximum") == 3);
}

TEST_CASE("[cep][schema][fabric] fabric config schema 含 networks + vc_count,实例符合该 schema",
          "[cep][schema]") {
    const json fs = load_fabric_schema();
    REQUIRE(fs.contains("properties"));
    const auto& nets = fs.at("properties").at("networks");
    REQUIRE(nets.at("minProperties") == 1);
    REQUIRE(nets.at("additionalProperties").at("type") == "object");
    const auto& net_spec = nets.at("additionalProperties");
    REQUIRE(net_spec.at("required").size() == 1);
    REQUIRE(net_spec.at("required").at(0) == "vc_count");
    const auto& vc_count = net_spec.at("properties").at("vc_count");
    REQUIRE(vc_count.at("type") == "integer");
    REQUIRE(vc_count.at("minimum") == 1);
    // 与 protocol_instance.schema.json 的 vc_id 上界(0-3)对齐 → vc_count 上界取 4
    // (若将来某网络的 vc_count > 4,**两处须同步改**)
    REQUIRE(vc_count.at("maximum") == 4);

    // **实例的键必须全在 schema 声明的 properties 内** —— 否则「schema 声明的约束」
    // 与「实例实际形态」会分叉(2026-10-22 代码复审 B1:实例曾带 3 个 schema 未声明的注记字段)。
    // 注:完整的 JSON Schema 校验由 `scripts/test/cep_decl_alignment_check.sh`
    //     (jsonschema)承担;此处做**不依赖外部库**的键集包含断言。
    const json fi = load_fabric_instance();
    const auto& declared = fs.at("properties");
    for (auto it = fi.begin(); it != fi.end(); ++it) {
        INFO("fabric 实例含 schema 未声明的字段: " << it.key());
        REQUIRE(declared.contains(it.key()));
    }

    REQUIRE(fi.contains("networks"));
    const auto& inst_nets = fi.at("networks");
    REQUIRE(inst_nets.size() >= 1);
    for (auto it = inst_nets.begin(); it != inst_nets.end(); ++it) {
        INFO("fabric 实例缺 vc_count: " << it.key());
        REQUIRE(it.value().contains("vc_count"));
        REQUIRE(it.value().at("vc_count").get<int>() <= vc_count.at("maximum").get<int>());
    }
    // 本仓 fabric 实例是 ArchForge 单点权威的**派生副本**,非独立权威(per NS-R2)
    REQUIRE(fi.contains("derived_from"));
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

    // 锁定表的 8 个取值(per ADR-CEP-11 §3;2026-10-22 per 台账 Entry #12 增补
    // moesi-gpu-1.0 / mesif-1.0 —— 登记 moesi_gpu / mesif 进 07 号 §5.1.1 后,
    // INV-CEP-11-IC1 要求其 protocol_version 必须落在锁表内)
    const std::vector<std::string> locked = {"chi-r2",         "cxl-2.0",    "cxl-3.0",
                                             "tilelink-c-1.3", "nvlink-4.0", "ualink-2.0",
                                             "moesi-gpu-1.0",  "mesif-1.0"};
    const std::regex re(pattern);
    for (const auto& v : locked) {
        INFO("锁定表取值未出现在 ADR-CEP-11: " << v);
        REQUIRE(lock.find(v) != std::string::npos);
        INFO("schema pattern 拒绝锁定表合法取值: " << v);
        REQUIRE(std::regex_match(v, re));
    }

    // 锁表**恰好** N 项 —— 防「表里加了第 9 个值、但本测试的硬编码列表没跟上」这类
    // **静默失去覆盖**(2026-10-22 critic NIT:hardcode 6 → 8 只保证了「旧值仍在表里」,
    // 保证不了「表里没有新值」)。做法:解析 §3 表的第 2 列,断言其 backtick 取值集合
    // 与硬编码列表**完全相等**(个数 + 逐项双向)。
    const size_t sec3 = lock.find("## 3. 规范版本锁定表");
    REQUIRE(sec3 != std::string::npos);
    const size_t sec3_end = lock.find("\n## ", sec3 + 1);
    const std::string sec3_body =
        lock.substr(sec3, (sec3_end == std::string::npos ? lock.size() : sec3_end) - sec3);

    std::set<std::string> table_vals;
    {
        std::istringstream ss(sec3_body);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty() || line[0] != '|')
                continue; // 只看表格行
            if (line.find("---") != std::string::npos)
                continue; // 跳分隔行
            if (line.find("协议族") != std::string::npos)
                continue; // 跳表头
            const size_t c1 = line.find('|', 1);
            if (c1 == std::string::npos)
                continue;
            const size_t c2 = line.find('|', c1 + 1);
            if (c2 == std::string::npos)
                continue;
            const std::string col = line.substr(c1 + 1, c2 - c1 - 1);
            const size_t b = col.find('`');
            if (b == std::string::npos)
                continue;
            const size_t e = col.find('`', b + 1);
            if (e == std::string::npos)
                continue;
            table_vals.insert(col.substr(b + 1, e - b - 1));
        }
    }
    INFO("§3 锁表解析出的 protocol_version 取值数: " << table_vals.size() << " (硬编码列表 "
                                                     << locked.size() << ")");
    REQUIRE(table_vals.size() == locked.size());
    for (const auto& v : locked) {
        INFO("§3 锁表缺取值: " << v);
        REQUIRE(table_vals.count(v) == 1);
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

TEST_CASE("[cep][schema] per-network 形态下每个 vc_id 落在该网络槽位上界内(IC2 正向路径)",
          "[cep][schema]") {
    // ⚠️ 2026-10-22 契约演进:正向路径从「named_network 解析到唯一 vc_id」改为
    //    「network 已在 fabric 命名空间登记 + 其下每个 vc_id 落在该网络的槽位上界内」。
    //    「唯一性」不再是判据 —— 嵌套形态下 vc_id 是**显式**给出的,不存在多义。
    const std::set<int> known = {0, 1, 2, 3};
    const NetworkMap fabric = {{"net_fwd", 4}, {"net_bwd", 2}};

    json inst = minimal_valid_instance();
    inst["ordering_assumption"] = {{"net_fwd", {{"0", "p2p_ordered"}, {"1", "unordered"}}}};
    const auto ok = resolve_ordering_assumptions(inst, known, fabric);
    INFO("per-network 正向路径应通过: " << ok.reason);
    REQUIRE(ok.ok);

    // 同一协议实例可横跨多张网(per ArchForge 32 号 §6.1:moesi_gpu 横跨 compute_noc + ualink_fabric)
    json multi = minimal_valid_instance();
    multi["ordering_assumption"] = {{"net_fwd", {{"0", "p2p_ordered"}}},
                                    {"net_bwd", {{"1", "unordered"}}}};
    multi["vc_binding"] = {{"net_fwd", {{"request", 0}}}, {"net_bwd", {{"request", 1}}}};
    REQUIRE(resolve_ordering_assumptions(multi, known, fabric).ok);
}

TEST_CASE("[cep][schema] per-network 形态下的 5 类非法输入必须 fail(IC2 反向路径)",
          "[cep][schema]") {
    const std::set<int> known = {0, 1, 2, 3};
    const NetworkMap fabric = {{"net_fwd", 4}, {"net_bwd", 2}};

    json ok = minimal_valid_instance();
    REQUIRE(resolve_ordering_assumptions(ok, known, fabric).ok);

    // ① 网络未在 fabric 命名空间登记(NS-R1)
    json bad = minimal_valid_instance();
    bad["ordering_assumption"] = {{"net_unknown", {{"0", "p2p_ordered"}}}};
    const auto r = resolve_ordering_assumptions(bad, known, fabric);
    REQUIRE_FALSE(r.ok);
    REQUIRE_FALSE(r.reason.empty());

    // ② 内层 key 越界(不在已声明域)
    json bad2 = minimal_valid_instance();
    bad2["ordering_assumption"] = {{"net_fwd", {{"9", "p2p_ordered"}}}};
    REQUIRE_FALSE(resolve_ordering_assumptions(bad2, known, fabric).ok);

    // ③ 内层 key 非 vc_id(不可解析为整数)
    json bad3 = minimal_valid_instance();
    bad3["ordering_assumption"] = {{"net_fwd", {{"high_pri", "p2p_ordered"}}}};
    REQUIRE_FALSE(resolve_ordering_assumptions(bad3, known, fabric).ok);

    // ④ vc_id 超出该网络的槽位上界(net_bwd 只有 2 个槽)
    json bad4 = minimal_valid_instance();
    bad4["ordering_assumption"] = {{"net_bwd", {{"3", "p2p_ordered"}}}};
    const auto r4 = resolve_ordering_assumptions(bad4, known, fabric);
    REQUIRE_FALSE(r4.ok);
    REQUIRE(r4.reason.find("槽位上界") != std::string::npos);

    // ⑤ 未提供 fabric 映射时, 任何网络一律 fail(不得静默放过)
    json bad5 = minimal_valid_instance();
    bad5["ordering_assumption"] = {{"net_fwd", {{"0", "p2p_ordered"}}}};
    REQUIRE_FALSE(resolve_ordering_assumptions(bad5, known, {}).ok);

    // ⑥ IC2 ①:ordering 的 vc_id 必须出现在**同网络** vc_binding 的值域中
    json bad6 = minimal_valid_instance();
    bad6["ordering_assumption"] = {{"net_fwd", {{"3", "p2p_ordered"}}}};
    bad6["vc_binding"] = {{"net_fwd", {{"request", 0}, {"response", 1}}}};
    const auto r6 = resolve_ordering_assumptions(bad6, known, fabric);
    REQUIRE_FALSE(r6.ok);
    REQUIRE(r6.reason.find("值域") != std::string::npos);

    // ⑦ IC2 ③:消息类必须 ∈ 6 类规范词表(CATEGORY_CREDIT / forward_data / snoop_completion_ack
    // 均排除)
    json bad7 = minimal_valid_instance();
    bad7["vc_binding"] = {{"net_fwd", {{"request", 0}, {"forward_data", 1}}}};
    const auto r7 = resolve_ordering_assumptions(bad7, known, fabric);
    REQUIRE_FALSE(r7.ok);
    REQUIRE(r7.reason.find("6 类规范词表") != std::string::npos);
}

// 计数口径(per ADR-CEP-12 决策 9):**仅** `implemented` 计入。
// ⚠️ 2026-10-21 代码复审 S13:原断言是「枚举里 'implemented' 出现 1 次」
//    —— **恒真**,与 ADR 声称的计数口径毫无关系。现改为可测的规则实现。
std::size_t count_implemented(const std::vector<std::string>& statuses) {
    std::size_t n = 0;
    for (const auto& st : statuses) {
        if (st == "implemented")
            ++n;
    }
    return n;
}

TEST_CASE("[cep][schema] status 只有 implemented 计入 adapter 计数(IC3)", "[cep][schema]") {
    const json s = load_schema();
    const std::set<std::string> st(s.at("properties").at("status").at("enum").begin(),
                                   s.at("properties").at("status").at("enum").end());
    // 枚举取值锁定(3 值,缺一即口径漂移)
    REQUIRE(st.size() == 3);
    REQUIRE(st.count("planned") == 1);
    REQUIRE(st.count("implemented") == 1);
    REQUIRE(st.count("excluded") == 1);

    // 计数规则的**行为**:混合状态 → 只有 implemented 计入
    const std::vector<std::string> mixed = {"implemented", "planned",     "implemented",
                                            "excluded",    "implemented", "planned"};
    REQUIRE(count_implemented(mixed) == 3);
    REQUIRE(count_implemented({}) == 0);
    REQUIRE(count_implemented({"planned", "excluded"}) == 0);
    REQUIRE(count_implemented({"implemented"}) == 1);
    // 规则只能统计枚举内的取值(否则拼写错误会被静默漏计)
    for (const auto& stt : mixed) {
        INFO("非枚举 status 值: " << stt);
        REQUIRE(st.count(stt) == 1);
    }

    // ADR 侧的口径文字必须被钉住(决策 9 是计数口径的**唯一依据**)
    const auto adr12 = read_file(
        (repo_root() / "docs" / "adr" / "ADR-CEP-12-protocol-engine-capability-map.md").string());
    INFO("ADR-CEP-12 决策 9 的计数口径文字缺失");
    REQUIRE(adr12.find("implemented") != std::string::npos);
    REQUIRE(adr12.find("计数口径") != std::string::npos);
    REQUIRE(adr12.find("MESI_Three_Level_HTM") != std::string::npos);
}

TEST_CASE("[cep][schema] 07 号 §5.1 的 adapter 实例数与表格行数一致(IC3 口径锚点)",
          "[cep][schema]") {
    // ⚠️ S13 的第二半:「统计结果与 07 号 §5.1 的口径表述**一致**」(spec:257)
    //    —— 原测试**完全没查 07 号**。
    //    计数规则的**下界**来自 07 号 §5.1 的表格;若表格行数与声明数漂移,
    //    「已实现 adapter 实例数」的下界就失真 → 必须机器核对。
    const auto truth = read_file(
        (repo_root() / "docs" / "architecture" / "07-coherence-exploration-platform.md").string());

    // §5.1.1 工业硬件协议族: | **族** | 实例 | `Adapter` | 状态 | ... |
    const auto i511 = truth.find("#### 5.1.1");
    const auto i512 = truth.find("#### 5.1.2");
    const auto i513 = truth.find("#### 5.1.3");
    REQUIRE(i511 != std::string::npos);
    REQUIRE(i512 != std::string::npos);
    REQUIRE(i513 != std::string::npos);

    int rows511 = 0;
    {
        const std::string sec = truth.substr(i511, i512 - i511);
        std::size_t pos = 0;
        while ((pos = sec.find("| **", pos)) != std::string::npos) {
            const std::size_t eol = sec.find('\n', pos);
            const std::string line =
                sec.substr(pos, (eol == std::string::npos ? sec.size() : eol) - pos);
            // 只数**数据行**(表头以 `| 协议族 |` 开头,不带 `**`)
            if (line.find("`") != std::string::npos && line.find("Adapter") != std::string::npos) {
                ++rows511;
            }
            pos = (eol == std::string::npos) ? sec.size() : eol + 1;
        }
    }
    int rows512 = 0;
    {
        const std::string sec = truth.substr(i512, i513 - i512);
        std::size_t pos = 0;
        while ((pos = sec.find("| ", pos)) != std::string::npos) {
            const std::size_t eol = sec.find('\n', pos);
            const std::string line =
                sec.substr(pos, (eol == std::string::npos ? sec.size() : eol) - pos);
            // §5.1.2 数据行形如 | MESI_Two_Level | ✅ 直接支持 | ... |
            // 取**第 1 格**(协议实例名):第 1 个 "|" 与第 2 个 "|" 之间
            const std::size_t c2 = line.find('|', 1);
            if (c2 != std::string::npos && c2 > 2) {
                std::string cell = line.substr(2, c2 - 2);
                // 去掉首尾空白
                const auto b = cell.find_first_not_of(" \t");
                const auto e = cell.find_last_not_of(" \t");
                if (b != std::string::npos && e != std::string::npos) {
                    cell = cell.substr(b, e - b + 1);
                }
                if (!cell.empty() &&
                    (std::isalpha(static_cast<unsigned char>(cell[0])) || cell[0] == '*')) {
                    ++rows512;
                }
            }
            pos = (eol == std::string::npos) ? sec.size() : eol + 1;
        }
    }
    INFO("§5.1.1 行数=" << rows511 << " §5.1.2 行数=" << rows512);
    REQUIRE(rows511 == 8); // 工业 6 族 / 8 实例(2026-10-22 per Entry #12:moesi_gpu / mesif 登记)
    REQUIRE(rows512 == 8); // Ruby 1 族 / 8 实例
    REQUIRE(rows511 + rows512 == 16);

    // §5.1 的声明必须与表格一致,且必须声明 16 是**下界** + 语义索引不重复计 adapter
    INFO("07 号 §5.1 未声明 16 个 adapter 实例");
    REQUIRE(truth.find("16 个 adapter 实例") != std::string::npos);
    INFO("07 号 §5.1 未声明 §5.1.4 的语义索引不重复计 adapter");
    REQUIRE(truth.find("不重复计 adapter") != std::string::npos);
    INFO("07 号 §5.1 未声明 5 个一致性模型语义索引");
    REQUIRE(truth.find("5 个一致性模型语义索引") != std::string::npos);
    // §5.1.1 / §5.1.2 各自的族/实例声明
    REQUIRE(truth.find("6 族 / 8 实例") != std::string::npos);
    REQUIRE(truth.find("1 族 / 8 实例") != std::string::npos);

    // 交叉一致性:`excluded` 的用法(MESI_Three_Level_HTM)在 07 号侧标「明确排除」
    INFO("07 号 §5.1.2 未把 MESI_Three_Level_HTM 标为明确排除");
    REQUIRE(truth.find("MESI_Three_Level_HTM") != std::string::npos);
    REQUIRE(truth.find("明确排除") != std::string::npos);
}
