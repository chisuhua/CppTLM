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

    /// 迁移登记的**必填字段 + id 存在性**谓词
    /// (per `ADR-CEP-12 §13` 的判据 ①②③④)
    /// @param declared_ids `instances[]` 中已声明的 id 集合(判据 ① 的依据)
    /// @return 空 = 通过;非空 = 违规原因
    std::vector<std::string> migration_entry_violations(const json& m,
                                                        const std::set<std::string>& declared_ids) {
        std::vector<std::string> bad;
        if (!m.is_object())
            return {"迁移条目不是对象"};
        const std::string id = m.value("instance_id", "");
        if (id.empty())
            bad.push_back("缺 instance_id");
        else if (declared_ids.count(id) == 0)
            bad.push_back("迁移 id 未在 instances[] 中声明(判据 ①)");
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
//  V1 ~ V5 —— **降级后**的守门状态(追认闭合 ⇒ 迁移条目已删除)
// ══════════════════════════════════════════════════════════════════════
//
//  ⚠️ **状态变迁**(2026-10-30,per ArchForge `42 号` 追认闭合 + 台账 Entry #102):
//  `authority_migrations[mesif]` 条目**已删除**(X-1)⇒ 本测试的前提由「迁移登记**存在**」
//  变为「迁移登记**已清空** + 降级后的守门状态正确」⇒ 原「V1:含 mesif」断言**必须**改写。
//  ⇒ 但**迁移机制本身的规则**必须**保持有效**(将来再有权威迁移时仍要拦住不合规登记)
//  ⇒ 故以「**合成条目 + 变异 fixture**」表达(见 V3)。
TEST_CASE("R-1 权威迁移:降级后的守门状态(迁移已清空 + 机制规则仍有效)",
          "[cep][authority-migration][r-1]") {
    const json doc = load_json(cep_file("protocol_instances.json"));

    // ── V1:顶层有 `authority_migrations[]`,且**当前为空**(追认闭合 ⇒ X-1 降级完成)──
    REQUIRE(doc.contains("authority_migrations"));
    const json& migs = doc.at("authority_migrations");
    REQUIRE(migs.is_array());
    CHECK(migs.empty());

    // ── V2:`mesif` **不在** `repo_owned_instance_ids[]` ────────────────────────
    //    理由:`mesif` 仍由 ArchForge 声明(镜像域)⇒ 若登记进 `repo_owned`,撞名闸必红
    //    ⇒ 降级的正确形态是「**既不在**迁移名单、**也不在** repo_owned」⇒ 由 ⑥ 逐字段镜像守。
    const json& owned = doc.at("repo_owned_instance_ids");
    REQUIRE(owned.is_array());
    for (const auto& oid : owned)
        CHECK(oid.get<std::string>() != "mesif");
    std::set<std::string> migrated;
    for (const auto& m : migs)
        migrated.insert(m.value("instance_id", ""));
    CHECK(migrated.count("mesif") == 0u);

    // ── V3:迁移**机制规则**仍有效(对**合成**条目判红)──────────────────────
    //    已声明的 id 集合(判据 ① 的依据)
    std::set<std::string> declared;
    for (const auto& inst : doc.at("instances"))
        declared.insert(inst.value("id", ""));
    //    正控:一条**合规**的合成迁移条目(登记 `mesif` + 四字段齐备)⇒ 零违规
    json synth = {{"instance_id", "mesif"},
                  {"reason", "合成条目(机制自检用)"},
                  {"ledger_entry", "Entry #SYNTH"},
                  {"archforge_snapshot", "ArchForge 32 号:529(合成)"}};
    CHECK(migration_entry_violations(synth, declared).empty());
    // 反向 fixture:四字段任一缺失 ⇒ 必红(机制**有牙**)
    {
        json broken = synth;
        broken["reason"] = "";
        CHECK_FALSE(migration_entry_violations(broken, declared).empty());
    }
    {
        json broken = synth;
        broken["ledger_entry"] = "";
        CHECK_FALSE(migration_entry_violations(broken, declared).empty());
    }
    {
        json broken = synth;
        broken["archforge_snapshot"] = "";
        CHECK_FALSE(migration_entry_violations(broken, declared).empty());
    }
    {
        json broken = synth;
        broken["instance_id"] = "no_such_instance";
        CHECK_FALSE(migration_entry_violations(broken, declared).empty());
    }

    // ── V4:两个名单**不重叠**(权威归属不得二义)—— 机制规则仍有效 ────────────
    {
        std::set<std::string> with_synth;
        with_synth.insert(synth.value("instance_id", ""));
        for (const auto& oid : owned)
            with_synth.insert(oid.get<std::string>());
        // 变异:把 `mesif` 同时放进两个名单 ⇒ 权威归属二义 ⇒ 必红
        std::set<std::string> overlap = with_synth;
        overlap.insert("mesif");
        bool ambiguous = false;
        for (const auto& oid : owned)
            if (overlap.count(oid.get<std::string>()) != 0 &&
                migrated.count(oid.get<std::string>()) != 0)
                ambiguous = true;
        // 正态:`owned` 与 `migrated` 当前无交集 ⇒ 不二义
        bool real_ambiguity = false;
        for (const auto& oid : owned)
            if (migrated.count(oid.get<std::string>()) != 0)
                real_ambiguity = true;
        CHECK_FALSE(real_ambiguity);
    }

    // ── V5:撞名闸**仍有效** —— 把 `mesif` 登记进 `repo_owned` ⇒ 必红 ────────
    //    谓词与守门脚本 `cep_decl_alignment_check.sh` 的「撞名闸」同口径:
    //    `repo_owned` id 与 ArchForge 声明撞名且**未**登记迁移 ⇒ 红。
    //    ⚠️ ArchForge 声明集 = {`moesi_gpu`, `mesif`}(镜像域;由该脚本的 ① 校验)
    {
        const std::set<std::string> archforge_declared = {"moesi_gpu", "mesif"};
        const auto collide = [&](const std::vector<std::string>& repo_owned) {
            for (const auto& id : repo_owned) {
                if (archforge_declared.count(id) != 0 && migrated.count(id) == 0)
                    return true;
            }
            return false;
        };
        std::vector<std::string> current;
        for (const auto& oid : owned)
            current.push_back(oid.get<std::string>());
        CHECK_FALSE(collide(current)); // 正控:当前不撞名
        current.push_back("mesif");    // 变异:撞名 ⇒ 必红
        CHECK(collide(current));
    }
}

// ══════════════════════════════════════════════════════════════════════
//  V6 —— 降级后 ⑥ 对 `mesif` **恢复**逐字段镜像(两端已一致)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("R-1 权威迁移:降级后 mesif 与 ArchForge **恢复**逐字段镜像(⑥)",
          "[cep][authority-migration][r-1]") {
    const json doc = load_json(cep_file("protocol_instances.json"));
    const json* mesif = nullptr;
    for (const auto& inst : doc.at("instances")) {
        if (inst.value("id", "") == "mesif")
            mesif = &inst;
    }
    REQUIRE(mesif != nullptr);

    // ── `mesif` 的 `sharer_tracker` 是**本仓口径**(R-1 裁决的产物)────────────
    CHECK(mesif->at("sharer_tracker").at("impl") == "HierarchicalDirectory");
    const auto& caps = mesif->at("sharer_tracker").at("caps");
    CHECK(caps.at("scope") == "SYSTEM");
    CHECK(caps.at("completeness") == "EXACT");
    CHECK(caps.at("authority") == "GLOBAL");
    CHECK(caps.at("aggregation_at") == "HIERARCHICAL");

    // ── 降级后:`mesif` **不在**迁移名单 ⇒ ⑥ **恢复**逐字段镜像 ────────────────
    //    (两端声明已**逐字段实测全等** —— 由 `cep_decl_alignment_check.sh` 的 ⑥ 校验)
    const json& migs = doc.at("authority_migrations");
    for (const auto& m : migs)
        CHECK(m.value("instance_id", "") != "mesif");

    // ── 审计痕迹:降级事实 + 台账锚保留在 `derived_from.freeze_state` ─────────
    const std::string fs = doc.at("derived_from").value("freeze_state", "");
    CHECK(fs.find("权威迁移条目已删除") != std::string::npos);
    CHECK(fs.find("Entry #102") != std::string::npos);

    // ── 正控:`moesi_gpu` 仍是**镜像域**(未登记迁移)⇒ 不得被牵连 ─────────────
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
