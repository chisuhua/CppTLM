// test/test_cep_authority_migration.cc
// 队列 #9 `cpptlm-cep-mesif-adapter` 的 **R-1 裁决落地** —— 权威迁移判据(V1 ~ V7)
//
// per openspec/changes/... (R-1 落地) + `ADR-CEP-04 §13` + `ADR-CEP-12 §13`
// 作者: CppTLM Team / 日期: 2026-10-29
//
// ── 判据纪律 ──────────────────────────────────────────────────────────
// ① 每条判据都有**可执行反向 fixture**(本文件内以「同一谓词 + 变异输入」实现,
//    破坏性 fixture 另在 `scripts/test/cep_decl_reverse_verify.sh` 的破坏 21 ~ 26);
// ② **纯负向断言必带正控**:「迁移名单有 mesif」之后必跟「未登记迁移的镜像域实例
//    仍逐字段镜像」的对照,否则「守门整体失守」与「机制正确」不可分辨;
// ③ 判据读**真实文件**(声明源 / schema / 实现表),不读手搓常量。
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include "tlm/cep/sharer_tracker.hh"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace {

    using json = nlohmann::json;

    json load_json(const std::filesystem::path& p) {
        std::ifstream in(p);
        if (!in.good())
            throw std::runtime_error("无法打开 " + p.string());
        return json::parse(in);
    }

    std::filesystem::path cep_file(const char* name) {
        return cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / name;
    }

    /// 迁移登记的**必填字段**谓词(per `ADR-CEP-12 §13` 的判据 ②③④)
    /// @return 空 = 通过;非空 = 违规原因
    std::vector<std::string> migration_entry_violations(const json& m) {
        std::vector<std::string> bad;
        if (!m.is_object())
            return {"迁移条目不是对象"};
        const std::string id = m.value("instance_id", "");
        if (id.empty())
            bad.push_back("缺 instance_id");
        if (m.value("reason", "").empty())
            bad.push_back("缺 reason(迁移必须写明理由)");
        if (m.value("ledger_entry", "").empty())
            bad.push_back("缺 ledger_entry(台账锚)");
        if (m.value("archforge_snapshot", "").empty())
            bad.push_back("缺 archforge_snapshot(原声明锚)");
        return bad;
    }

} // namespace

// ══════════════════════════════════════════════════════════════════════
//  V1 ~ V3 —— 迁移登记的**存在性 + 必填字段 + 名单互斥**
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("R-1 权威迁移:登记存在且字段齐备(per ADR-CEP-12 §13 的 V1~V3)",
          "[cep][authority-migration][r-1]") {
    const json doc = load_json(cep_file("protocol_instances.json"));

    // ── V1:顶层有 `authority_migrations[]`,且含 `mesif` ──────────────
    REQUIRE(doc.contains("authority_migrations"));
    const json& migs = doc.at("authority_migrations");
    REQUIRE(migs.is_array());
    std::set<std::string> migrated;
    for (const auto& m : migs)
        migrated.insert(m.value("instance_id", ""));
    CHECK(migrated.count("mesif") == 1u);

    // ── V2:每条记录的必填字段齐备 ───────────────────────────────────
    for (const auto& m : migs) {
        INFO("migration entry: " << m.dump());
        CHECK(migration_entry_violations(m).empty());
    }

    // ── V3a:迁移 id 存在于 `instances[]` ─────────────────────────────
    std::set<std::string> declared;
    for (const auto& inst : doc.at("instances"))
        declared.insert(inst.value("id", ""));
    for (const auto& m : migs) {
        const std::string id = m.value("instance_id", "");
        INFO("migration id " << id);
        CHECK(declared.count(id) == 1u);
    }
    // ── V3b:两个名单**不重叠**(权威归属不得二义)─────────────────────
    const json& owned = doc.at("repo_owned_instance_ids");
    REQUIRE(owned.is_array());
    for (const auto& oid : owned) {
        INFO("repo_owned id " << oid.get<std::string>());
        CHECK(migrated.count(oid.get<std::string>()) == 0u);
    }

    // ── 反向 fixture(同谓词 + 变异输入)──────────────────────────────
    {
        json broken = migs[0];
        broken["reason"] = "";
        CHECK_FALSE(migration_entry_violations(broken).empty());
    }
    {
        json broken = migs[0];
        broken["ledger_entry"] = "";
        CHECK_FALSE(migration_entry_violations(broken).empty());
    }
    {
        json broken = migs[0];
        broken["archforge_snapshot"] = "";
        CHECK_FALSE(migration_entry_violations(broken).empty());
    }
    {
        json broken = migs[0];
        broken["instance_id"] = "no_such_instance";
        CHECK(declared.count(broken["instance_id"].get<std::string>()) == 0u);
    }
}

// ══════════════════════════════════════════════════════════════════════
//  V4 —— ⑥ 对迁移名单**不**要求逐字段镜像;①~⑤ 仍覆盖
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("R-1 权威迁移:mesif 的声明**不再**与 ArchForge 逐字段镜像(权威 = 本仓)",
          "[cep][authority-migration][r-1]") {
    const json doc = load_json(cep_file("protocol_instances.json"));
    const json* mesif = nullptr;
    for (const auto& inst : doc.at("instances")) {
        if (inst.value("id", "") == "mesif")
            mesif = &inst;
    }
    REQUIRE(mesif != nullptr);

    // ── 迁移后 `mesif` 的 `sharer_tracker` 已改成本仓口径 ─────────────
    CHECK(mesif->at("sharer_tracker").at("impl") == "HierarchicalDirectory");
    const auto& caps = mesif->at("sharer_tracker").at("caps");
    CHECK(caps.at("scope") == "SYSTEM");
    CHECK(caps.at("completeness") == "EXACT");
    CHECK(caps.at("authority") == "GLOBAL");
    CHECK(caps.at("aggregation_at") == "HIERARCHICAL");

    // ── 与 ArchForge 的分叉**是已登记的代价**(登记在迁移条目里)──────
    const json& migs = doc.at("authority_migrations");
    REQUIRE(migs.size() >= 1u);
    const auto& mig = migs[0];
    CHECK(mig.value("instance_id", "") == "mesif");
    CHECK(mig.value("reason", "").find("SparseSnoopFilter") != std::string::npos);
    CHECK(mig.value("archforge_snapshot", "").find("32 号") != std::string::npos);

    // ── 正控:`moesi_gpu` 仍是**镜像域**(未登记迁移)⇒ 不得被牵连 ─────
    std::set<std::string> migrated;
    for (const auto& m : migs)
        migrated.insert(m.value("instance_id", ""));
    CHECK(migrated.count("moesi_gpu") == 0u);
    const json* moesi = nullptr;
    for (const auto& inst : doc.at("instances")) {
        if (inst.value("id", "") == "moesi_gpu")
            moesi = &inst;
    }
    REQUIRE(moesi != nullptr);
    // `moesi_gpu` 的 `sharer_tracker` **零改动**(仍是 SparseSnoopFilter)
    CHECK(moesi->at("sharer_tracker").at("impl") == "SparseSnoopFilter");
}

// ══════════════════════════════════════════════════════════════════════
//  第 6 种实现 + `HIERARCHICAL`(per `ADR-CEP-04 §13` 的 V1 ~ V3)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("R-1 第 6 种 SharerTracker 实现 `HierarchicalDirectory` + `HIERARCHICAL`",
          "[cep][authority-migration][r-1][sharer-tracker]") {
    using namespace tlm::cep;

    // ── V1:实现表 6 行 + 第 6 行 caps ───────────────────────────────
    REQUIRE(kTrackerTraits.size() == 6u);
    const TrackerTraits* h = find_tracker_traits(TrackerImpl::HierarchicalDirectory);
    REQUIRE(h != nullptr);
    CHECK(h->caps.scope == Scope::SYSTEM);
    CHECK(h->caps.completeness == Completeness::EXACT);
    CHECK(h->caps.authority == Authority::GLOBAL);
    CHECK(h->caps.aggregation_at == AggregationAt::HIERARCHICAL);

    // ── V2:schema 枚举含新实现与新取值 ──────────────────────────────
    const json schema = load_json(cep_file("protocol_instance.schema.json"));
    const auto& st = schema.at("properties").at("sharer_tracker").at("properties");
    {
        const std::set<std::string> impls(st.at("impl").at("enum").begin(),
                                          st.at("impl").at("enum").end());
        CHECK(impls.count("HierarchicalDirectory") == 1u);
        // 「不参与」**不是**一种实现
        CHECK(impls.count("不参与") == 1u); // 是 schema 的**合法字面值**(选型可写)
    }
    {
        const auto& agg = st.at("caps").at("properties").at("aggregation_at");
        const std::set<std::string> aggs(agg.at("enum").begin(), agg.at("enum").end());
        CHECK(aggs.count("HIERARCHICAL") == 1u);
    }
    // ── V3:`caps` 维度**未扩**(仍 4 属性)─────────────────────────────
    CHECK(schema.at("properties")
              .at("sharer_tracker")
              .at("properties")
              .at("caps")
              .at("required")
              .size() == 4u);

    // ── 反向 fixture:新实现**不得**声明 PARTIAL(决策 2)─────────────
    for (const auto& t : kTrackerTraits) {
        INFO("impl " << t.name);
        CHECK(t.caps.completeness != Completeness::PARTIAL);
    }
    // ── 反向 fixture:`HIERARCHICAL` **只**被第 6 种实现使用 ──────────
    std::size_t hierarchical_users = 0;
    for (const auto& t : kTrackerTraits) {
        if (t.caps.aggregation_at == AggregationAt::HIERARCHICAL)
            ++hierarchical_users;
    }
    CHECK(hierarchical_users == 1u);
}
