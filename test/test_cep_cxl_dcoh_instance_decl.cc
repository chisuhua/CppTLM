// =============================================================================
// test/test_cep_cxl_dcoh_instance_decl.cc
//   —— 队列 #12 `cpptlm-cep-cxl-dcoh-symmetric` 的**实例声明面**判据
//     (CDX-06 13 键齐备 + `perm` 4 值 · CDX-07 不声明静默升级 · CDX-08 `AC-05` 走 SKIP)
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: include/tlm/cep/protocol_instances.json · include/tlm/cep/perm_upgrades.json
//       include/tlm/cep/protocol_instance.schema.json · include/tlm/cep/adapter_schema_contracts.hh
//       include/tlm/cep/cxl_dcoh_adapter.hh · include/tlm/cep/cxl_dcoh_tracker.hh
//       openspec/changes/cpptlm-cep-cxl-dcoh-symmetric/(delta spec CDX-06 ~ CDX-08)
//       台账 obj-cep-platform.md Entry #154(architect 裁决)· #160(建档)· #162(Phase 2.3)
//
// ── 判据口径 ────────────────────────────────────────────────────────────────
// · **机器驱动**:13 键齐备性由 `protocol_instance.schema.json` 的 `required` **实读**得出
//   ⇒ 不用硬编码键名清单(schema 改 ⇒ 判据自动跟随)。
// · ⚠️ **CDX-08 的 SKIP ≠ PASS**:AC-05 对本族**无 unique-clean 态** ⇒ 语义**不适用**
//   ⇒ 判据必须区分「SKIP」与「PASS」⇒ 断言 `verdict == SKIP` 且 `!= PASS`。
// · ⚠️ **回归锁**:既有 MESI 族的 AC-05 判定 SHALL **逐条不变**(本族接入不得放宽既有族)。
// · ⚠️ **不重复实现**规则本体(规则本体在 `adapter_schema_contracts.hh`)。
// =============================================================================

#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "tlm/cep/adapter_contracts_loader.hh"
#include "tlm/cep/adapter_schema_contracts.hh"
#include "tlm/cep/cxl_dcoh_adapter.hh"
#include "tlm/cep/cxl_dcoh_tracker.hh"

namespace {

    using json = nlohmann::json;

    json load_cep_json(const char* name) {
        return json::parse(cpptlm::cep::read_file(
            (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / name).string()));
    }

    const json& instances_doc() {
        static const json d = load_cep_json("protocol_instances.json");
        return d;
    }

    const json& upgrades_doc() {
        static const json d = load_cep_json("perm_upgrades.json");
        return d;
    }

    const json& dcoh_inst() {
        static const json t = [&]() {
            for (const auto& i : instances_doc().at("instances")) {
                if (i.value("id", "") == "cxl_3_0_symmetric")
                    return i;
            }
            return json::object();
        }();
        return t;
    }

    const json& cxl2_inst() {
        static const json t = [&]() {
            for (const auto& i : instances_doc().at("instances")) {
                if (i.value("id", "") == "cxl_2_0")
                    return i;
            }
            return json::object();
        }();
        return t;
    }

} // namespace

// ═══════════════════════════════════════════════════════════════════════════
// CDX-06 —— 实例声明 SHALL 齐 13 键且 `perm` 为 4 值(机器驱动)
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-06:cxl_3_0_symmetric 实例声明 13 键齐备 + perm 4 值 + back_invalidate=true",
          "[cep][cxl-dcoh][instance-decl]") {
    const json& inst = dcoh_inst();
    REQUIRE(!inst.is_null());
    REQUIRE(inst.is_object());

    // ── ① schema 的 13 个 required 键 SHALL 齐备(机器驱动)────────────────
    const json schema = load_cep_json("protocol_instance.schema.json");
    const auto& required = schema.at("required");
    REQUIRE(required.size() == 13u);
    for (const auto& k : required) {
        INFO("required key " << k.get<std::string>());
        CHECK(inst.contains(k.get<std::string>()));
    }

    // ── ② `perm` 恰为 **4** 值(含 UNIQUE —— 设备缓存含 E 态 ⇒ E→UNIQUE 需可表达)──
    const auto& perm = inst.at("perm");
    REQUIRE(perm.size() == 4u);
    CHECK(perm.at(0).get<std::string>() == "NONE");
    CHECK(perm.at(1).get<std::string>() == "READ");
    CHECK(perm.at(2).get<std::string>() == "UNIQUE");
    CHECK(perm.at(3).get<std::string>() == "WRITEBACK");
    // ⚠️ schema **零改动** ⇒ 4 值仍**合法**(值级枚举 + minItems:1)
    const auto& perm_schema = schema.at("properties").at("perm");
    CHECK(perm_schema.at("minItems").get<int>() == 1);
    CHECK(perm_schema.at("items").at("enum").size() == 4u); // 值域仍 4 值

    // ── ③ ★ `flow_support.back_invalidate` = **true** —— 本族**与 CXL 2.0 的关键差异** ──
    //    (BISnp 走 canonical `back_invalidate` 流;`cxl_2_0` 是 `false`)
    CHECK(inst.at("flow_support").at("back_invalidate").get<bool>() == true);
    //    ⇒ **正控**:`cxl_2_0` 的同项 = `false` ⇒ 二者**可判别** ⇒ 本断言不是恒绿
    const json& cxl2 = cxl2_inst();
    REQUIRE(!cxl2.is_null());
    CHECK(cxl2.at("flow_support").at("back_invalidate").get<bool>() == false);

    // ── ④ 其余 `flow_support` 键(逐项)─────────────────────────────────────
    const auto& fs = inst.at("flow_support");
    CHECK(fs.at("request").get<bool>() == true);
    CHECK(fs.at("completion").get<bool>() == true);
    CHECK(fs.at("snoop").get<bool>() == true);
    CHECK(fs.at("snoop_resp").get<bool>() == true);
    CHECK(fs.at("writeback").get<bool>() == true);
    CHECK(fs.at("forward_data").get<bool>() == true);
    CHECK(fs.at("address_resolve").get<bool>() == false);
    CHECK(fs.at("barrier").get<bool>() == false);
    CHECK(fs.at("snoop_completion_ack").get<bool>() == false);

    // ── ⑤ `vc_binding` 逐类(BISnp 是 **snoop** 家族 ⇒ `back_invalidate` 归 snoop 的 VC)──
    const auto& vc = inst.at("vc_binding").at("compute_noc");
    CHECK(vc.at("request").get<int>() == 0);
    CHECK(vc.at("response").get<int>() == 1);
    CHECK(vc.at("snoop").get<int>() == 2);
    CHECK(vc.at("snoop_resp").get<int>() == 3);
    // ⚠️ `writeback` 是 **canonical 10 流名**,不是消息类名 ⇒ SHALL NOT 出现
    CHECK_FALSE(vc.contains("writeback"));
    CHECK_FALSE(vc.contains("atomic"));
    CHECK_FALSE(vc.contains("query"));

    // ── ⑥ `ordering_assumption`:响应向 vc1 / vc3 SHALL `p2p_ordered`(J1)──
    const auto& ord = inst.at("ordering_assumption").at("compute_noc");
    CHECK(ord.at("0").get<std::string>() == "unordered");
    CHECK(ord.at("1").get<std::string>() == "p2p_ordered");
    CHECK(ord.at("2").get<std::string>() == "unordered");
    CHECK(ord.at("3").get<std::string>() == "p2p_ordered");

    // ── ⑦ `sharer_tracker` 逐字段等于 `kTrackerTraits[DcohSymmetric]` ─────
    CHECK(inst.at("sharer_tracker").at("impl").get<std::string>() == "DcohSymmetric");
    const auto& caps = inst.at("sharer_tracker").at("caps");
    const auto& traits = tlm::cep::kTrackerTraits[2];
    REQUIRE(traits.impl == tlm::cep::TrackerImpl::DcohSymmetric);
    CHECK(caps.at("scope").get<std::string>() == "LOCAL_MEMORY_REGION");
    CHECK(caps.at("completeness").get<std::string>() == "CONSERVATIVE");
    CHECK(caps.at("authority").get<std::string>() == "LOCAL_HINT");
    CHECK(caps.at("aggregation_at").get<std::string>() == "HOME");
    //    ⇒ 声明面与**实现面**同源(实现类取同一 traits 行 ⇒ 不另写一份字面量)
    const tlm::cep::DcohSymmetric tracker({0x1000, 0x100000});
    CHECK(tracker.caps.scope == traits.caps.scope);
    CHECK(tracker.caps.completeness == traits.caps.completeness);
    CHECK(tracker.caps.authority == traits.caps.authority);
    CHECK(tracker.caps.aggregation_at == traits.caps.aggregation_at);

    // ── ⑧ `repo_owned_instance_ids` SHALL 含本族(⑥ 镜像跳过 / ①~⑤ 覆盖)──
    const auto& owned = instances_doc().at("repo_owned_instance_ids");
    bool found = false;
    for (const auto& id : owned) {
        if (id.get<std::string>() == "cxl_3_0_symmetric")
            found = true;
    }
    CHECK(found);

    // ── ⑨ `protocol_version` SHALL 等于锁表值(`ADR-CEP-11 §3`: `cxl-3.0`)──
    CHECK(inst.at("protocol_version").get<std::string>() == "cxl-3.0");
    CHECK(inst.at("adapter").get<std::string>() == "CxlDcohAdapter");
    CHECK(inst.at("family").get<std::string>() == "industrial");
    CHECK(inst.at("status").get<std::string>() == "implemented");
    // ⚠️ **正控**:本族 adapter 名 ≠ `cxl_2_0` 的 ⇒ 独立 adapter 裁决生效
    CHECK(cxl2.at("adapter").get<std::string>() == "CxlCacheAdapter");

    // ── ⑩ `atomic` / `consistency_labels` ────────────────────────────────
    CHECK(inst.at("atomic").at("supported").get<bool>() == false);
    CHECK(inst.at("consistency_labels").at("read").get<std::string>() == "SC");
    CHECK(inst.at("consistency_labels").at("write").get<std::string>() == "SC");
}

// ═══════════════════════════════════════════════════════════════════════════
// CDX-07 —— 实例 SHALL **不**声明静默升级
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-07:cxl_3_0_symmetric 不声明静默升级(E→M 未确证 ⇒ 声明即假声明)",
          "[cep][cxl-dcoh][instance-decl]") {
    // ① `perm_upgrades.json` **无**本族行
    const auto& rows = upgrades_doc().at("perm_upgrade").at("silent_upgrades");
    for (const auto& u : rows) {
        INFO("silent upgrade: " << u.at("instance_id").get<std::string>());
        CHECK(u.at("instance_id").get<std::string>() != "cxl_3_0_symmetric");
    }
    // ⚠️ **理由**:E→M 静默升级在本族**未确证**(公开非规范来源里**没有**)
    //    ⇒ 声明即**假声明** ⇒ 走「不声明」路(照队列 #10 / #11 先例)

    // ② 派生量 `no_upgrade_instances` **恰含 2 个**(本族 + `tilelink_c`)
    const auto loaded = tlm::cep::load_adapter_contract_inputs(cpptlm::cep::repo_root().string());
    REQUIRE(loaded.ok);
    const auto& no_up = loaded.inputs.no_upgrade_instances;
    // ⚠️ 2026-11-03(队列 #13 **实现层**):新增 `nvlink_4`(本仓自持 · BroadcastNull · 无静默升级行)⇒
    // 计数 **+1**(per 台账 Entry #187;设计 §9.3 计数同步清单)
    // ⚠️ 2026-11-06(per 台账 Entry #239 §⑤-1):4 —— 新增 `ualink_4`(无静默升级 ⇒ 入派生集)
    REQUIRE(no_up.size() == 4u);
    //    ⇒ 顺序 = `instances[]` 数组序(派生函数按数组序遍历)
    CHECK(no_up.at(0) == "cxl_3_0_symmetric");
    CHECK(no_up.at(3) == "ualink_4"); // ⚠️ 2026-11-06:追加在末尾 ⇒ `at(3)`
    CHECK(no_up.at(1) == "tilelink_c");
    CHECK(no_up.at(2) == "nvlink_4");

    // ③ IC-2 判据**只**查 `combination_capabilities.hierarchical` ⇒ 本族填 `true` 即 PASS
    //    ⚠️ IC-2 的 detail 文案「UNIQUE(E 可静默升 M)」属**沿用**通用措辞
    //    ⇒ 本族 E→M **未确证** ⇒ 该措辞是**已知失实** ⇒ 登记 pending(非本 change 修)
    CHECK(dcoh_inst().at("combination_capabilities").at("hierarchical").get<bool>() == true);
}

// ═══════════════════════════════════════════════════════════════════════════
// CDX-08 —— `AC-05` 对本族 SHALL 走 **SKIP**(不是 FAIL,也不得记 PASS)
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-08:AC-05 对本族走 SKIP(不得 FAIL / 不得 PASS)+ 既有族判定逐条不变",
          "[cep][cxl-dcoh][instance-decl]") {
    // ① 本族:**无** unique-clean 态 ⇒ AC-05 **SKIP**
    //    (⚠️ 本族的 `perm` **含** `UNIQUE` —— 但 AC-05 的「unique-clean 态」指**状态机**的
    //     E 态,而本族载体**无转移表** ⇒ 无 `kUniqueCleanState` 可声明
    //     ⇒ `AdapterHasUniqueClean` 判 false ⇒ SKIP)
    {
        tlm::cep::AdapterSchemaContracts::Inputs in;
        in.has_unique_clean_state = false;
        in.e_write_target_state = -1;
        in.e_state_perm_rank = -1;
        in.m_state_perm_rank = -1;
        const auto v = tlm::cep::AdapterSchemaContracts::check_adapter_self_consistency(
            in.stable_state_has_projection, in.e_write_target_state, in.e_state_perm_rank,
            in.m_state_perm_rank, in.unique_dirty_target_state, in.has_unique_clean_state);
        CHECK(v.rule_id == "AC-05");
        CHECK(v.verdict == tlm::cep::AdapterContractVerdict::SKIP);
        //    ⇒ 「SKIP ≠ PASS」:不得把「无可检查对象」记为通过
        CHECK(v.verdict != tlm::cep::AdapterContractVerdict::PASS);
        CHECK(v.verdict != tlm::cep::AdapterContractVerdict::FAIL);
    }

    // ② **回归锁**:既有 MESI 族(MOESI / MESIF)的 AC-05 判定 SHALL **仍为 PASS**
    //    ⇒ 本族接入**不得**放宽既有族的严格性
    {
        tlm::cep::AdapterSchemaContracts::Inputs in;
        in.stable_state_has_projection = {{0, true}, {1, true}, {2, true}, {3, true}};
        in.e_write_target_state = 3;
        in.e_state_perm_rank = 2; // UNIQUE
        in.m_state_perm_rank = 3; // WRITEBACK
        in.has_unique_clean_state = true;
        const auto v = tlm::cep::AdapterSchemaContracts::check_adapter_self_consistency(
            in.stable_state_has_projection, in.e_write_target_state, in.e_state_perm_rank,
            in.m_state_perm_rank, in.unique_dirty_target_state, in.has_unique_clean_state);
        CHECK(v.verdict == tlm::cep::AdapterContractVerdict::PASS);
    }

    // ③ **鉴别力**:同一族但「目标态 ≠ 期望值」⇒ **仍 FAIL**(⇒ SKIP 不是放宽)
    {
        tlm::cep::AdapterSchemaContracts::Inputs in;
        in.stable_state_has_projection = {{0, true}, {1, true}, {2, true}, {3, true}};
        in.e_write_target_state = 2; // ≠ 3
        in.e_state_perm_rank = 2;
        in.m_state_perm_rank = 3;
        in.has_unique_clean_state = true;
        const auto v = tlm::cep::AdapterSchemaContracts::check_adapter_self_consistency(
            in.stable_state_has_projection, in.e_write_target_state, in.e_state_perm_rank,
            in.m_state_perm_rank, in.unique_dirty_target_state, in.has_unique_clean_state);
        CHECK(v.verdict == tlm::cep::AdapterContractVerdict::FAIL);
    }
}
