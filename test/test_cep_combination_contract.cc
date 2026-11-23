// test/test_cep_combination_contract.cc
// `combination_contract` 声明项的**判定级**载体
//   · INV-CEP-13-IC3 —— adapter 提前 ack 行为成为**实例声明项**
//   · INV-CEP-13-IC1 —— 「采用该公理为判据」成为**结构化声明项**
//
// per 登记册 §6.4 队列 2 的 I-6 · 单元 `.rddf/improvements/cep-invariant-modeling-batch2.md`
//   AC-9（逐实例声明 + `unknown` 计数机器化）· AC-10（条件判据 + 合成实例三例）· AC-11（咬合）
// 形态来源 = architect 裁决 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §二
//   （用户 ratify R-2a~R-2f）—— ⛔ 本文件不新增任何语义,只把裁决给定的形态**变成判据**。
//
// ⚠️ 全部断言都是**结构/形态**断言(⛔ 不是「ADR 里写着这句话」):
//   变红条件 = `early_ack` 枚举被扩/缩 · 第 14 项被塞进 `required` · 某实例漏声明
//   · 某实例声明值与期望表不一致 · 条件规则(`adopted == true ⇒ …`)被架空
//   · 期望表的域与声明源的域不再是**双向**相等(新增实例无覆盖 / 删实例静默漏检)
//
// ⚠️ **诚实边界(声明 ≠ 判定)**:本载体证明的是**声明面**成立 —— ⛔ **不等于**复合一致性公理
//   已被检验(其判定载体归 backlog `cep-memory-model-operational-trace`;公理的可判定性仍
//   受限于「本仓无操作级模型」)⇒ ⛔ 不得把本载体绿表述为「公理已成立 / 已被检验」。
//
// 作者: CppTLM Team / 日期: 2026-11-21
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    using nlohmann::json;

    json load_json(const char* rel) {
        const std::string text = read_file((repo_root() / rel).string());
        REQUIRE_FALSE(text.empty());
        return json::parse(text);
    }

    json load_schema() {
        const std::string text = read_file(
            (repo_root() / "include" / "tlm" / "cep" / "protocol_instance.schema.json").string());
        REQUIRE_FALSE(text.empty());
        return json::parse(text);
    }

    /// 本仓权威域 = `repo_owned_instance_ids` ∪ `authority_migrations[].instance_id`
    /// (口径同 `scripts/test/cep_decl_alignment_check.sh` ⇒ 未来权威迁移不漏检)
    std::set<std::string> repo_domain_ids(const json& decl) {
        std::set<std::string> ids;
        for (const auto& v : decl.value("repo_owned_instance_ids", std::vector<std::string>{}))
            ids.insert(v);
        for (const auto& m : decl.value("authority_migrations", std::vector<json>{})) {
            if (m.contains("instance_id"))
                ids.insert(m.at("instance_id").get<std::string>());
        }
        return ids;
    }

    /// 最小 schema 求值器 —— 只覆盖 `combination_contract` 用到的约束子集
    /// (type / required / properties / additionalProperties / minProperties / enum /
    ///  minLength / minItems / items)。
    /// ⚠️ 约束值**从 schema 读**,⛔ 不在本文件硬编码 ⇒ 削弱 schema 会被本载体抓住。
    bool eval_schema(const json& sch, const json& inst, std::string& why) {
        if (sch.contains("type")) {
            const std::string t = sch.at("type").get<std::string>();
            bool ok = true;
            if (t == "object")
                ok = inst.is_object();
            else if (t == "array")
                ok = inst.is_array();
            else if (t == "string")
                ok = inst.is_string();
            else if (t == "boolean")
                ok = inst.is_boolean();
            else if (t == "integer")
                ok = inst.is_number_integer();
            else if (t == "number")
                ok = inst.is_number();
            if (!ok) {
                why = "type mismatch (期望 " + t + ")";
                return false;
            }
        }
        if (sch.contains("enum")) {
            bool found = false;
            for (const auto& e : sch.at("enum"))
                if (e == inst) {
                    found = true;
                    break;
                }
            if (!found) {
                why = "enum mismatch";
                return false;
            }
        }
        if (inst.is_string() && sch.contains("minLength")) {
            if (inst.get<std::string>().size() < sch.at("minLength").get<std::size_t>()) {
                why = "minLength violated";
                return false;
            }
        }
        if (inst.is_array()) {
            if (sch.contains("minItems") && inst.size() < sch.at("minItems").get<std::size_t>()) {
                why = "minItems violated";
                return false;
            }
            if (sch.contains("items")) {
                for (const auto& it : inst)
                    if (!eval_schema(sch.at("items"), it, why))
                        return false;
            }
        }
        if (inst.is_object()) {
            if (sch.contains("minProperties") &&
                inst.size() < sch.at("minProperties").get<std::size_t>()) {
                why = "minProperties violated";
                return false;
            }
            if (sch.contains("required")) {
                for (const auto& r : sch.at("required")) {
                    const std::string key = r.get<std::string>();
                    if (!inst.contains(key)) {
                        why = "missing required: " + key;
                        return false;
                    }
                }
            }
            const bool allow_additional = sch.value("additionalProperties", true);
            for (auto it = inst.begin(); it != inst.end(); ++it) {
                if (sch.contains("properties") && sch.at("properties").contains(it.key())) {
                    if (!eval_schema(sch.at("properties").at(it.key()), it.value(), why))
                        return false;
                } else if (!allow_additional) {
                    why = "additional property not allowed: " + it.key();
                    return false;
                }
            }
        }
        return true;
    }

    /// 条件规则(per 裁决 §2.2「必填策略」第 3 层):`adopted == true` ⇒
    /// `edge_classes` 非空 ∧ `verification_scope` 非空;`adopted == false` ⇒ 允许两者为空。
    /// ⚠️ schema **不写** `if/then` ⇒ 该规则落**判据**(本函数)。
    bool conditional_ok(const json& csc, std::string& why) {
        if (!csc.contains("adopted") || !csc.at("adopted").is_boolean())
            return true; // `adopted` 缺失 ⇒ 由 schema 的 `required` 负责判红
        if (!csc.at("adopted").get<bool>())
            return true; // `false` ⇒ 真空
        const bool ec_ok = csc.contains("edge_classes") && csc.at("edge_classes").is_array() &&
                           !csc.at("edge_classes").empty();
        if (!ec_ok) {
            why = "conditional violated: adopted==true ⇒ edge_classes 必须非空";
            return false;
        }
        const bool vs_ok = csc.contains("verification_scope") &&
                           csc.at("verification_scope").is_string() &&
                           !csc.at("verification_scope").get<std::string>().empty();
        if (!vs_ok) {
            why = "conditional violated: adopted==true ⇒ verification_scope 必须非空";
            return false;
        }
        return true;
    }

    /// `compound_consistency` 对象的完整有效性 = schema 声明的约束 ∧ 条件规则
    bool valid_compound(const json& sch_csc, const json& obj, std::string& why) {
        if (!eval_schema(sch_csc, obj, why))
            return false;
        return conditional_ok(obj, why);
    }

} // namespace

// ═══════════════════════════════════════════════════════════════════════════
// INV-CEP-13-IC3 —— adapter 提前 ack 行为成为**实例声明项**
// ═══════════════════════════════════════════════════════════════════════════
// INV-CARRIER: INV-CEP-13-IC3
TEST_CASE("[cep][combination-contract] IC3 —— early_ack 枚举封闭 + 必填 + 本仓权威域逐实例声明",
          "[cep][combination-contract][invariant]") {
    const json schema = load_schema();
    const json decl = load_json("include/tlm/cep/protocol_instances.json");
    const json table = load_json("test/cep_combination_contract_expected.json");

    REQUIRE(table.value("schema", std::string()) == "cep-combination-contract-expected/v1");

    const json& cc_schema = schema.at("properties").at("combination_contract");
    const json& bridging = cc_schema.at("properties").at("bridging");

    // ① 枚举**恰 3** 值且⛔ 不含 `not-applicable`(per 裁决 P-9:无适用 predicate ⇒ 不可判的声明项)
    const std::vector<std::string> want_enum = {"allow-early-write-ack", "deny-early-completion",
                                                "unknown"};
    const std::set<std::string> got_enum(
        bridging.at("properties").at("early_ack").at("enum").begin(),
        bridging.at("properties").at("early_ack").at("enum").end());
    REQUIRE(got_enum == std::set<std::string>(want_enum.begin(), want_enum.end()));
    REQUIRE(got_enum.size() == 3u);
    REQUIRE(got_enum.count("not-applicable") == 0u);

    // ② `bridging.required` **恰** `["early_ack"]`(声明了 bridging ⇒ early_ack 必填)
    const std::vector<std::string> bridging_required =
        bridging.at("required").get<std::vector<std::string>>();
    REQUIRE(bridging_required == std::vector<std::string>{"early_ack"});

    // ③ 本仓权威域的**每个**实例都声明 `combination_contract.bridging.early_ack`,
    //    且取值与期望表**逐实例一致**(域双向集合相等 ⇒ 新增实例不「无覆盖且绿」)
    const std::set<std::string> domain = repo_domain_ids(decl);
    std::set<std::string> fixture_ids;
    for (const auto& item : table.at("instances").items())
        fixture_ids.insert(item.key());
    REQUIRE(domain == fixture_ids);
    REQUIRE(domain.size() == 6u);

    const json& instances = decl.at("instances");
    // ⚠️ `protocol_instances.json` 的 `instances` 是**数组**(元素带 `id`)⇒ 先建 index
    std::map<std::string, const json*> by_id;
    for (const auto& inst : instances) {
        REQUIRE(inst.contains("id"));
        by_id[inst.at("id").get<std::string>()] = &inst;
    }
    REQUIRE(by_id.size() == instances.size()); // id 唯一(数组形态下不可能静默覆盖)
    std::size_t unknown_count = 0;
    for (const auto& id : domain) {
        INFO("权威域实例缺 declaration: " << id);
        REQUIRE(by_id.count(id) == 1u);
        const json& inst = *by_id.at(id);
        INFO("实例缺 combination_contract: " << id);
        REQUIRE(inst.contains("combination_contract"));
        const json& cc = inst.at("combination_contract");
        INFO("实例缺 combination_contract.bridging: " << id);
        REQUIRE(cc.contains("bridging"));
        const json& br = cc.at("bridging");
        INFO("实例缺 combination_contract.bridging.early_ack: " << id);
        REQUIRE(br.contains("early_ack"));
        const std::string got = br.at("early_ack").get<std::string>();
        INFO("实例的 early_ack 与期望表不一致: " << id);
        REQUIRE(got ==
                table.at("instances").at(id).at("bridging").at("early_ack").get<std::string>());
        REQUIRE(got_enum.count(got) == 1u);
        if (got == "unknown")
            ++unknown_count;
    }
    // `unknown` **精确计数**(⛔ 该计数是变更检测器,⛔ 不是「声明面已有信息量」的证据)
    REQUIRE(unknown_count == table.at("unknown_count").get<std::size_t>());

    // ④ `combination_contract` **可选** ⇒ ⛔ 不在 `required` 里(进 required ⇒ IC1 语义变更)
    const std::vector<std::string> required = schema.at("required").get<std::vector<std::string>>();
    REQUIRE(std::find(required.begin(), required.end(), "combination_contract") == required.end());
    REQUIRE(required.size() == 13u);
    REQUIRE(schema.at("properties").size() == 14u);
}

// ═══════════════════════════════════════════════════════════════════════════
// INV-CEP-13-IC1 —— 「采用该公理为判据」成为**结构化声明项**
// ═══════════════════════════════════════════════════════════════════════════
// INV-CARRIER: INV-CEP-13-IC1
TEST_CASE("[cep][combination-contract] IC1 —— 复合一致性声明项的取值域 + 条件规则(合成实例三例)",
          "[cep][combination-contract][invariant]") {
    const json schema = load_schema();
    const json& cc_schema = schema.at("properties").at("combination_contract");
    const json& csc = cc_schema.at("properties").at("compound_consistency");

    // ① `compound_consistency.required` **恰** `["adopted"]`
    const std::vector<std::string> csc_required =
        csc.at("required").get<std::vector<std::string>>();
    REQUIRE(csc_required == std::vector<std::string>{"adopted"});

    // ② `edge_classes` 的取值域**恰** 3 值(⛔ 本仓**只登记取值域**、⛔ 不定义语义 —— 语义锚点 =
    // ADR-CEP-13 决策 2)
    const std::vector<std::string> want_classes = {"rfe", "fr", "ws"};
    const std::set<std::string> got_classes(
        csc.at("properties").at("edge_classes").at("items").at("enum").begin(),
        csc.at("properties").at("edge_classes").at("items").at("enum").end());
    REQUIRE(got_classes == std::set<std::string>(want_classes.begin(), want_classes.end()));
    REQUIRE(got_classes.size() == 3u);

    // ③ 条件规则(`adopted == true` ⇒ `edge_classes` 非空 ∧ `verification_scope` 非空)
    //    —— ⛔ 用**合成实例**(不需入库):现状无实例声明 `compound_consistency` ⇒
    //    用仓内实例做反 fixture 会**永远空跑**。
    std::string why;
    // ③a 反例:`adopted == true` 但 `edge_classes` 为空 ⇒ schema 层**放行**(无 minItems)
    //     ⇒ **只有**条件规则能抓住它 ⇒ 本例即「条件规则有牙」的证据
    json c1 = json::object();
    c1["adopted"] = true;
    c1["edge_classes"] = json::array();
    c1["verification_scope"] = "domain-local";
    REQUIRE(eval_schema(csc, c1, why));          // schema 层不报错
    REQUIRE_FALSE(valid_compound(csc, c1, why)); // 条件规则判红

    // ③b 反例:`adopted == true` 但 `verification_scope` 为空串 ⇒ schema 的 minLength 即判红
    json c2 = json::object();
    c2["adopted"] = true;
    c2["edge_classes"] = json::array({"rfe"});
    c2["verification_scope"] = "";
    REQUIRE_FALSE(valid_compound(csc, c2, why));

    // ③c 正例:`adopted == false` ⇒ 允许 `edge_classes` / `verification_scope` 皆空
    json c3 = json::object();
    c3["adopted"] = false;
    REQUIRE(valid_compound(csc, c3, why));

    // ④ 禁止派生填充 litmus 语料(范畴错误 ⇒ 假绿):语料**必须**仍为空
    const json corpus = load_json("test/cep_compound_graph_corpus.json");
    REQUIRE(corpus.at("compound_edges").is_array());
    REQUIRE(corpus.at("compound_edges").empty());
}
