// =============================================================================
// test/test_cep_tilelink_instance_decl.cc
//   —— 队列 #11 `cpptlm-cep-tilelink-tree-manager` 的**实例声明面**判据
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: include/tlm/cep/protocol_instances.json · include/tlm/cep/perm_upgrades.json
//       include/tlm/cep/tilelink_c_adapter.hh · include/tlm/cep/tilelink_state_spec.hh
//       openspec/changes/cpptlm-cep-tilelink-tree-manager/(delta spec TMA-06 / TMA-08 / TMA-09 /
//       TMA-10) 台账 obj-cep-platform.md Entry #146(Phase 3 落修)
//
// ── 判据口径 ──────────────────────────────────────────────────────────────
// · **真实数据**侧的取值(13 键齐备 / `perm` 3 值 / `vc_binding` 逐类 / caps 逐字段)
// · **族无关化**的判定:AC-05 对本族 SHALL 走 **SKIP**(不是 FAIL)
// · **派生量**的判别力:`no_upgrade_instances` SHALL **恰含** `tilelink_c`(首次非空)
// · ⚠️ **不重复实现**规则本体(规则本体在 `adapter_schema_contracts.hh`)
// =============================================================================

#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "tlm/cep/adapter_contracts_loader.hh"
#include "tlm/cep/channel_dependency_graph.hh"
#include "tlm/cep/ordering_point_checker.hh"
#include "tlm/cep/sharer_tracker.hh"
#include "tlm/cep/tilelink_c_adapter.hh"

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

    const json& tilelink_c() {
        static const json t = [&]() {
            for (const auto& i : instances_doc().at("instances")) {
                if (i.value("id", "") == "tilelink_c")
                    return i;
            }
            return json::object();
        }();
        return t;
    }

} // namespace

// ══════════════════════════════════════════════════════════════════════════
// TMA-08 —— 实例 13 键齐备 + `perm` 3 值 + `vc_binding` 逐类
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TileLink 实例声明: 13 键齐备 + perm 3 值 + vc_binding 按 R-T2",
          "[cep][tilelink][instance-decl]") {
    const json& inst = tilelink_c();
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

    // ── ② `perm` 恰为 3 值(R-T4 裁决 = 值域锁)────────────────────────────
    const auto& perm = inst.at("perm");
    REQUIRE(perm.size() == 3u);
    CHECK(perm.at(0).get<std::string>() == "NONE");
    CHECK(perm.at(1).get<std::string>() == "READ");
    CHECK(perm.at(2).get<std::string>() == "WRITEBACK");
    // ⚠️ `UNIQUE` SHALL NOT 出现(填 4 值 ⇒ 语义为假的绿灯)
    for (const auto& p : perm) {
        CHECK(p.get<std::string>() != "UNIQUE");
    }
    // ⚠️ schema 零改动 ⇒ 3 值仍**合法**(items.enum 是值级枚举 + minItems:1)
    const auto& perm_schema = schema.at("properties").at("perm");
    CHECK(perm_schema.at("minItems").get<int>() == 1);
    CHECK(perm_schema.at("items").at("enum").size() == 4u); // 值域仍 4 值

    // ── ③ `vc_binding` 逐类等于 R-T2 ─────────────────────────────────────
    const auto& vc = inst.at("vc_binding").at("compute_noc");
    CHECK(vc.at("request").get<int>() == 0);
    CHECK(vc.at("snoop").get<int>() == 1);
    CHECK(vc.at("snoop_resp").get<int>() == 2);
    CHECK(vc.at("response").get<int>() == 3);
    // ⚠️ `writeback` 是 **canonical 10 流名**,不是消息类名 ⇒ SHALL NOT 出现
    CHECK_FALSE(vc.contains("writeback"));
    // 未使用的两类不出现
    CHECK_FALSE(vc.contains("atomic"));
    CHECK_FALSE(vc.contains("query"));

    // ── ④ `ordering_assumption`:响应向 vc2 / vc3 SHALL `p2p_ordered`(J1)──
    const auto& ord = inst.at("ordering_assumption").at("compute_noc");
    CHECK(ord.at("2").get<std::string>() == "p2p_ordered");
    CHECK(ord.at("3").get<std::string>() == "p2p_ordered");

    // ── ⑤ `sharer_tracker` 逐字段等于 `kTrackerTraits[TreeManager]` ────────
    CHECK(inst.at("sharer_tracker").at("impl").get<std::string>() == "TreeManager");
    const auto& caps = inst.at("sharer_tracker").at("caps");
    const auto& traits = tlm::cep::kTrackerTraits[3];
    REQUIRE(traits.impl == tlm::cep::TrackerImpl::TreeManager);
    CHECK(caps.at("scope").get<std::string>() == "SUBTREE");
    CHECK(caps.at("completeness").get<std::string>() == "EXACT");
    CHECK(caps.at("authority").get<std::string>() == "LOCAL");
    CHECK(caps.at("aggregation_at").get<std::string>() == "TREE_PARENT");

    // ── ⑥ `repo_owned_instance_ids` SHALL 含 `tilelink_c`(⑥ 镜像跳过 / ①~⑤ 覆盖)
    const auto& owned = instances_doc().at("repo_owned_instance_ids");
    bool found = false;
    for (const auto& id : owned) {
        if (id.get<std::string>() == "tilelink_c")
            found = true;
    }
    CHECK(found);

    // ── ⑦ `protocol_version` SHALL 等于锁表值 ─────────────────────────────
    CHECK(inst.at("protocol_version").get<std::string>() == "tilelink-c-1.7.1");
    CHECK(inst.at("adapter").get<std::string>() == "TileLinkCAdapter");
    CHECK(inst.at("family").get<std::string>() == "academic");
    CHECK(inst.at("status").get<std::string>() == "implemented");
}

// ══════════════════════════════════════════════════════════════════════════
// TMA-09 —— AC-05 **族无关化**:本族走 SKIP(不是 FAIL)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TileLink: AC-05 族无关化 ⇒ 本族 SKIP(不得 FAIL / 不得 PASS)",
          "[cep][tilelink][instance-decl]") {
    auto inputs = tlm::cep::AdapterSchemaContracts::Inputs{};
    inputs.perm_domain.values = {"NONE", "READ", "UNIQUE", "WRITEBACK"};
    tlm::cep::fill_adapter_inputs<tlm::cep::TileLinkCAdapter>(inputs);

    // 该族**无** unique-clean 态 ⇒ 装载层如实标记
    CHECK_FALSE(inputs.has_unique_clean_state);
    CHECK(inputs.e_write_target_state == -1);
    CHECK(inputs.e_state_perm_rank == -1);
    CHECK(inputs.m_state_perm_rank == -1);

    // AC-05 ⇒ **SKIP**(不是 FAIL)
    const auto ac05 = tlm::cep::AdapterSchemaContracts::check_adapter_self_consistency(
        inputs.stable_state_has_projection, inputs.e_write_target_state, inputs.e_state_perm_rank,
        inputs.m_state_perm_rank, inputs.unique_dirty_target_state, inputs.has_unique_clean_state);
    CHECK(ac05.rule_id == "AC-05");
    CHECK(ac05.verdict == tlm::cep::AdapterContractVerdict::SKIP);
    CHECK(ac05.verdict != tlm::cep::AdapterContractVerdict::FAIL);
    // ⚠️ 不得记为 PASS(「无可检查对象」≠「通过」)
    CHECK(ac05.verdict != tlm::cep::AdapterContractVerdict::PASS);

    // ⚠️ **回归锁**:既有族(MESI 语义)**判定逐条不变** ⇒ 走默认参数即**原行为**
    const auto ac05_mesi = tlm::cep::AdapterSchemaContracts::check_adapter_self_consistency(
        {{0, true}, {1, true}, {2, true}, {3, true}}, 3, 2, 3);
    CHECK(ac05_mesi.verdict == tlm::cep::AdapterContractVerdict::PASS);
    // 既有的 FAIL 路径**仍**红(错误目标态 ⇒ FAIL,不是 SKIP)
    const auto ac05_bad = tlm::cep::AdapterSchemaContracts::check_adapter_self_consistency(
        {{0, true}, {1, true}, {2, true}, {3, true}}, 2, 2, 3);
    CHECK(ac05_bad.verdict == tlm::cep::AdapterContractVerdict::FAIL);

    // ── 族无关化的**鉴别力**:目标态不再硬编码 ⇒ 传 2 作为期望值 ⇒ 必红 ──────
    const auto ac05_param = tlm::cep::AdapterSchemaContracts::check_adapter_self_consistency(
        {{0, true}, {1, true}, {2, true}, {3, true}}, 3, 2, 3, 2, true);
    CHECK(ac05_param.verdict == tlm::cep::AdapterContractVerdict::FAIL);
}

// ══════════════════════════════════════════════════════════════════════════
// TMA-10 —— 派生量 `no_upgrade_instances` **首次非空** + IC2 vacuous
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TileLink: 派生量首次非空 + IC-2 vacuous PASS(文字与实际一致)",
          "[cep][tilelink][instance-decl]") {
    // ── ① `perm_upgrades.json` SHALL **无** `tilelink_c` 行(无 UNIQUE ⇒ 无静默升级)
    const json& ups = upgrades_doc();
    for (const auto& u : ups.at("perm_upgrade").at("silent_upgrades")) {
        CHECK(u.value("instance_id", "") != "tilelink_c");
    }

    // ── ② 装载层派生出的 `no_upgrade_instances` SHALL **恰含** `tilelink_c` ──
    const auto loaded = tlm::cep::load_adapter_contract_inputs(cpptlm::cep::repo_root().string());
    REQUIRE(loaded.ok);
    REQUIRE(loaded.inputs.no_upgrade_instances.size() == 1u);
    CHECK(loaded.inputs.no_upgrade_instances.at(0) == "tilelink_c");

    // ── ③ 实例总数 5(新增实例 ⇒ 计数同步)────────────────────────────────
    CHECK(loaded.inputs.instances.instances.size() == 5u);

    // ── ④ 装配期 ordering 判据:本族 IC-2 SHALL 是 **vacuous PASS** ───────
    //    (perm 不含 UNIQUE ⇒ `exposes_unique_perm == false` ⇒ 无静默升级入口)
    //    ⇒ detail 文字与实际**一致**(不是「UNIQUE(E 可静默升 M)」的假绿灯)
    json cfg = json::object();
    cfg["cep"] = json::object();
    cfg["cep"]["protocol_instances"] =
        (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json").string();
    const auto report = tlm::cep::run_ordering_gate(cfg);
    bool saw_tilelink_ic2 = false;
    for (const auto& line : report.results) {
        if (line.instance == "tilelink_c" &&
            line.criterion == tlm::cep::CriterionId::IC2_ESilentUpgrade) {
            saw_tilelink_ic2 = true;
            CHECK(line.verdict == tlm::cep::GateVerdict::PASS);
            // ⚠️ 文字 SHALL 是「无静默升级入口(vacuous PASS)」⇒ **不含**「E 可静默升 M」
            CHECK(line.detail.find("vacuous PASS") != std::string::npos);
            CHECK(line.detail.find("E 可静默升 M") == std::string::npos);
        }
    }
    CHECK(saw_tilelink_ic2);

    // ── ⑤ 通道依赖闸:本族 SHALL 无环(PASS)────────────────────────────────
    const auto gate = tlm::cep::run_channel_gate(cfg);
    CHECK(gate.verdict != tlm::cep::GateVerdict::FAIL);
}
