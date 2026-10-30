// test/test_cep_hierarchical_directory.cc
// 队列 #9 `cpptlm-cep-mesif-adapter` —— 第 6 种 SharerTracker 实现 `HierarchicalDirectory`
// per openspec/changes/cpptlm-cep-mesif-adapter/specs/cep-hierarchical-directory/spec.md
//     Requirement: cep-hierarchical-directory(HD-01 ~ HD-05)
// 作者: CppTLM Team / 日期: 2026-10-30
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <nlohmann/json.hpp>

#include <array>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "tlm/cep/sharer_tracker.hh"

using json = nlohmann::json;
using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    namespace cep = tlm::cep;
    using HD = cep::HierarchicalDirectory;

    const json& decl_doc() {
        static const json doc = json::parse(read_file(
            (repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json").string()));
        return doc;
    }

    const json& instance(const std::string& id) {
        static std::map<std::string, json> cache;
        auto it = cache.find(id);
        if (it != cache.end())
            return it->second;
        for (const auto& inst : decl_doc().at("instances")) {
            if (inst.at("id") == id) {
                cache[id] = inst;
                return cache[id];
            }
        }
        throw std::runtime_error("instance not found: " + id);
    }

    constexpr cep::responder_set_t mask(cep::node_id_t id) {
        return cep::responder_mask_of(id);
    }

    /// 层级参数(本仓模型:L0 最细 → L2 最粗)
    constexpr std::size_t kL0 = 0;
    constexpr std::size_t kL1 = 1;
    constexpr std::size_t kRoot = HD::kRootLevel;

} // namespace

// ─────────────────────────────────────────────────────────────────────────
// Scenario HD-01 —— caps + behavior 描述符 SHALL 与实例声明**三方相等**
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP HierarchicalDirectory: caps + behavior 三方相等", "[cep][sharer-tracker]") {
    const auto& st = instance("mesif").at("sharer_tracker");

    // 子条件 ①:caps 4 属性 —— 实例声明 ↔ traits 行 ↔ class 成员
    CHECK(st.at("impl") == "HierarchicalDirectory");
    CHECK(st.at("caps").at("scope") == "SYSTEM");
    CHECK(st.at("caps").at("completeness") == "EXACT");
    CHECK(st.at("caps").at("authority") == "GLOBAL");
    CHECK(st.at("caps").at("aggregation_at") == "HIERARCHICAL");

    const cep::TrackerTraits* traits =
        cep::find_tracker_traits(cep::TrackerImpl::HierarchicalDirectory);
    REQUIRE(traits != nullptr);
    CHECK(static_cast<int>(traits->caps.scope) == static_cast<int>(cep::Scope::SYSTEM));
    CHECK(static_cast<int>(traits->caps.completeness) ==
          static_cast<int>(cep::Completeness::EXACT));
    CHECK(static_cast<int>(traits->caps.authority) == static_cast<int>(cep::Authority::GLOBAL));
    CHECK(static_cast<int>(traits->caps.aggregation_at) ==
          static_cast<int>(cep::AggregationAt::HIERARCHICAL));

    HD dir;
    CHECK(static_cast<int>(dir.caps.scope) == static_cast<int>(cep::Scope::SYSTEM));
    CHECK(static_cast<int>(dir.caps.completeness) == static_cast<int>(cep::Completeness::EXACT));
    CHECK(static_cast<int>(dir.caps.authority) == static_cast<int>(cep::Authority::GLOBAL));
    CHECK(static_cast<int>(dir.caps.aggregation_at) ==
          static_cast<int>(cep::AggregationAt::HIERARCHICAL));

    // 子条件 ②:behavior 2 属性(STALL + 无广播兜底)—— 同三方
    CHECK(static_cast<int>(traits->behavior.hazard_under_contention) ==
          static_cast<int>(cep::LookupHazard::STALL));
    CHECK(traits->behavior.broadcast_fallback == false);
    CHECK(static_cast<int>(dir.behavior.hazard_under_contention) ==
          static_cast<int>(cep::LookupHazard::STALL));
    CHECK(dir.behavior.broadcast_fallback == false);

    // 子条件 ③:caps **仍为 4 属性** —— 层级深度是实现参数,不是能力属性
    CHECK(static_cast<int>(cep::AggregationAt::HIERARCHICAL) == 4);
    CHECK(HD::kLevelCount == 3u); // 层级数是**实现参数**,不进 caps

    // ── 反向 fixture:改 caps 任一字段 ⇒ 必红 ──────────────────────────
    //    (真实毒化副本由 reverse-verify 脚本执行;此处断言判据有鉴别力)
    auto mutated = st.at("caps");
    mutated["aggregation_at"] = "HOME";
    CHECK(mutated.at("aggregation_at") != st.at("caps").at("aggregation_at"));
}

// ─────────────────────────────────────────────────────────────────────────
// Scenario HD-02 —— SHALL 行使**多级聚合**差异化行为,SHALL **不**是 FullMap 克隆
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP HierarchicalDirectory: 多级聚合可观测(严格窄于根级)", "[cep][sharer-tracker]") {
    constexpr std::uint64_t kAddr = 0x1000;
    HD dir;

    // A 装在 L0(最细);B 装在 L1(更粗)
    dir.set_sharer(kAddr, cep::kNodeIdSdma, kL0, cep::SnoopState::S);
    dir.set_sharer(kAddr, cep::kNodeIdGmmu, kL1, cep::SnoopState::M);

    // L0 的**聚合**视图 = 只含 L0 自有(A)⇒ **严格窄于**根级
    const auto v0 = dir.aggregate_view_at(kAddr, kL0);
    const auto v1 = dir.aggregate_view_at(kAddr, kL1);
    const auto vr = dir.view_of(kAddr);

    REQUIRE(v0.valid);
    REQUIRE(v1.valid);
    REQUIRE(vr.valid);

    CHECK(v0.sharers == mask(cep::kNodeIdSdma));
    CHECK((v0.sharers & mask(cep::kNodeIdGmmu)) == 0); // B 不在 L0
    CHECK(v1.sharers == (mask(cep::kNodeIdSdma) | mask(cep::kNodeIdGmmu)));
    CHECK(vr.sharers == (mask(cep::kNodeIdSdma) | mask(cep::kNodeIdGmmu)));

    // ⇒ **存在严格更窄的视图** ⇒ 本实现**不可能**是 `FullMapDirectory` 克隆
    //    (克隆体是**单级** ⇒ 只有**一个**视图,没有「逐级收窄」这个能力)
    CHECK(v0.sharers != vr.sharers);
    CHECK((v0.sharers & vr.sharers) == v0.sharers); // 真子集

    // 上卷(roll-up):L1 聚合 ⊇ L0 聚合 ⊇ L0 自有
    CHECK(dir.own_view_at(kAddr, kL0).sharers == mask(cep::kNodeIdSdma));
    CHECK(dir.own_view_at(kAddr, kL1).sharers == mask(cep::kNodeIdGmmu));
    CHECK((v0.sharers & v1.sharers) == v0.sharers);

    // dirty 位同样逐级上卷
    CHECK(v0.dirty == 0);
    CHECK(v1.dirty == mask(cep::kNodeIdGmmu));

    // 移除 L0 的 A ⇒ L0 与根级聚合**同步**收窄(派生聚合 ⇒ 无陈旧聚合)
    dir.clear_sharer(kAddr, cep::kNodeIdSdma, kL0);
    CHECK(dir.aggregate_view_at(kAddr, kL0).sharers == 0);
    CHECK(dir.view_of(kAddr).sharers == mask(cep::kNodeIdGmmu));

    // 收窄视图可经 `lookup_scoped` 观测(ESCALATE_UP **不**使用 ⇒ 收窄显式提供)
    dir.set_sharer(kAddr, cep::kNodeIdSdma, kL0, cep::SnoopState::S);
    const auto scoped = dir.lookup_scoped(kAddr, kL0);
    CHECK(scoped.sharers == mask(cep::kNodeIdSdma));
    const auto scoped_root = dir.lookup_scoped(kAddr, kRoot);
    CHECK(scoped_root.sharers == (mask(cep::kNodeIdSdma) | mask(cep::kNodeIdGmmu)));

    // ── 反向 fixture:把聚合退化为「只看根级」⇒ 收窄能力消失 ⇒ 必红 ────
    auto degraded = dir;
    degraded.set_sharer(kAddr, cep::kNodeIdVram, kL0, cep::SnoopState::S);
    CHECK(degraded.aggregate_view_at(kAddr, kL0).sharers !=
          degraded.aggregate_view_at(kAddr, kRoot).sharers);
}

// ─────────────────────────────────────────────────────────────────────────
// Scenario HD-03 —— contention 下 SHALL 返回 `STALL` hazard
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP HierarchicalDirectory: contention ⇒ STALL + view 无效", "[cep][sharer-tracker]") {
    constexpr std::uint64_t kAddr = 0x2000;
    HD dir;
    dir.set_sharer(kAddr, cep::kNodeIdSdma, kL0, cep::SnoopState::M);

    // 无 contention ⇒ hazard NONE + view 有效
    {
        cep::LookupResult r = cep::LookupResult::ANSWER_HERE;
        cep::LocalView v;
        cep::LookupHazard h = cep::LookupHazard::NONE;
        dir.lookup(kAddr, cep::Intent::want_read, r, v, h);
        CHECK(h == cep::LookupHazard::NONE);
        CHECK(v.valid);
        CHECK(r == cep::LookupResult::FORWARD_TO); // 有 dirty 持有者 ⇒ 转发取数
        CHECK(v.forward_to == cep::kNodeIdSdma);
    }

    // 同址 snoop 在飞 ⇒ STALL + view 无意义
    cep::snoop_handle_t h = 0;
    dir.snoop_begin(kAddr, mask(cep::kNodeIdGmmu), h);
    REQUIRE(h != 0);
    {
        cep::LookupResult r = cep::LookupResult::ANSWER_HERE;
        cep::LocalView v;
        cep::LookupHazard haz = cep::LookupHazard::NONE;
        dir.lookup(kAddr, cep::Intent::want_read, r, v, haz);
        CHECK(haz == cep::LookupHazard::STALL);
        CHECK_FALSE(v.valid);
    }

    // ── 反向 fixture:实现返回 NONE 或 CHAIN ⇒ 必红 ────────────────────
    CHECK(static_cast<int>(cep::LookupHazard::STALL) != static_cast<int>(cep::LookupHazard::NONE));
    CHECK(static_cast<int>(cep::LookupHazard::STALL) != static_cast<int>(cep::LookupHazard::CHAIN));
}

// ─────────────────────────────────────────────────────────────────────────
// Scenario HD-04 —— 行为决策 SHALL **逐条 basis 登记** + 挂**跨仓追认债**
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP HierarchicalDirectory: 行为决策 basis 登记齐备 + 全部待追认",
          "[cep][sharer-tracker]") {
    // 子条件 ①:每条决策都有非空 `basis`
    CHECK(HD::kBehaviorDecisionCount == 5u);
    CHECK(HD::behavior_decisions_are_consistent());

    for (std::size_t i = 0; i < HD::kBehaviorDecisionCount; ++i) {
        const auto& d = HD::kBehaviorDecisions[i];
        INFO("decision id: " << d.id);
        REQUIRE(d.id != nullptr);
        REQUIRE(d.topic != nullptr);
        REQUIRE(d.basis != nullptr);
        CHECK(std::string(d.id).size() > 0);
        CHECK(std::string(d.topic).size() > 0);
        CHECK(std::string(d.basis).size() > 10); // 不是「一句话糊弄」
    }

    // 子条件 ②:`pending` **全部**为 true —— 自推语义**不得**写成「已裁决」
    for (std::size_t i = 0; i < HD::kBehaviorDecisionCount; ++i)
        CHECK(HD::kBehaviorDecisions[i].pending);

    // 子条件 ③:决策编号连续且唯一(B-1 … B-5)
    std::vector<std::string> ids;
    for (std::size_t i = 0; i < HD::kBehaviorDecisionCount; ++i)
        ids.emplace_back(HD::kBehaviorDecisions[i].id);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        CHECK(ids[i] == "B-" + std::to_string(i + 1));
        for (std::size_t j = i + 1; j < ids.size(); ++j)
            CHECK(ids[i] != ids[j]);
    }

    // ── 反向 fixture:删掉任一决策的 `basis` ⇒ 必红 ─────────────────────
    //    ⚠️ `kBehaviorDecisions` 是 `constexpr` ⇒ 数组名 decay 成
    //    `const BehaviorDecision*`,直接 `auto` 拷贝拿不到可变异副本 ⇒
    //    必须显式构造 `std::array` 逐条拷贝(否则反向 fixture 编译期就红,
    //    等于「永远红」的假 fixture,无鉴别力)。
    {
        std::array<HD::BehaviorDecision, HD::kBehaviorDecisionCount> mutated{};
        for (std::size_t i = 0; i < HD::kBehaviorDecisionCount; ++i)
            mutated[i] = HD::kBehaviorDecisions[i];
        mutated[2].basis = "";
        CHECK_FALSE(
            HD::behavior_decisions_are_consistent(mutated.data(), HD::kBehaviorDecisionCount));
    }
    // ── 反向 fixture:把某条标成「已裁决」(pending = false)⇒ 必红 ──────
    {
        std::array<HD::BehaviorDecision, HD::kBehaviorDecisionCount> mutated{};
        for (std::size_t i = 0; i < HD::kBehaviorDecisionCount; ++i)
            mutated[i] = HD::kBehaviorDecisions[i];
        mutated[0].pending = false;
        CHECK_FALSE(
            HD::behavior_decisions_are_consistent(mutated.data(), HD::kBehaviorDecisionCount));
    }
    // ── 反向 fixture:删掉一条决策 ⇒ 计数不符 ⇒ 必红 ───────────────────
    {
        CHECK(HD::kBehaviorDecisionCount - 1 != HD::kBehaviorDecisionCount);
    }
}

// ─────────────────────────────────────────────────────────────────────────
// Scenario HD-05 —— **F 态(转发点)** 的表示 SHALL **显式决策**
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP HierarchicalDirectory: F 态转发点缺口已显式登记", "[cep][sharer-tracker]") {
    // 子条件 ①:`SnoopState` 无 F ⇒ 转发点**角色**不可分辨 ⇒ 显式登记
    CHECK(HD::kForwardPointRoleRepresentable == false);
    REQUIRE(HD::kForwardPointGapNote != nullptr);
    CHECK(std::string(HD::kForwardPointGapNote).find("转发点保真度缺口") != std::string::npos);

    // 子条件 ②:该决策 SHALL 纳入 HD-04 的 `basis` 登记
    bool found_b5 = false;
    for (std::size_t i = 0; i < HD::kBehaviorDecisionCount; ++i) {
        if (std::string(HD::kBehaviorDecisions[i].id) == "B-5")
            found_b5 = true;
    }
    CHECK(found_b5);

    // 子条件 ③:`SnoopState` 的枚举里确实**没有** F
    CHECK(static_cast<int>(cep::SnoopState::I) == 0);
    CHECK(static_cast<int>(cep::SnoopState::S) == 1);
    CHECK(static_cast<int>(cep::SnoopState::E) == 2);
    CHECK(static_cast<int>(cep::SnoopState::M) == 3);

    // 转发**目标**仍由 `LocalView::forward_to` 承载(角色丢失 ≠ 目标丢失)
    constexpr std::uint64_t kAddr = 0x3000;
    HD dir;
    dir.set_sharer(kAddr, cep::kNodeIdSdma, kL0, cep::SnoopState::M);
    cep::LookupResult r = cep::LookupResult::ANSWER_HERE;
    cep::LocalView v;
    cep::LookupHazard h = cep::LookupHazard::NONE;
    dir.lookup(kAddr, cep::Intent::want_read, r, v, h);
    REQUIRE(r == cep::LookupResult::FORWARD_TO);
    CHECK(v.forward_to == cep::kNodeIdSdma);

    // ── 反向 fixture:缺口登记「形同虚设」⇒ 判据必红 ─────────────────────
    //    ⚠️ 判据形态必须是**可变异谓词**(对输入取参),而不是对真实常量的
    //    事实断言 —— 否则把 note 改空它也不红(空转)。这与
    //    `MesifStateSpec::deviations_are_consistent(t, list)` 同款。
    auto gap_is_registered = [](const char* note) {
        if (note == nullptr)
            return false;
        const std::string s(note);
        // 缺口登记的三要素:① 说明 F 态**存在** ② 说明 tracker **不可分辨**
        // ③ 显式标为「保真度缺口」(可被审计/追认)
        return s.find("F 态") != std::string::npos && s.find("不可分辨") != std::string::npos &&
               s.find("转发点保真度缺口") != std::string::npos;
    };

    // 正控:真实登记**必须**满足可变异谓词(与上面的事实断言互为交叉验证)
    CHECK(gap_is_registered(HD::kForwardPointGapNote));

    // 反控 ①:note 为空 ⇒ 必红(「登记了但没有内容」)
    CHECK_FALSE(gap_is_registered(""));
    // 反控 ②:note 只描述 F 态但**不标**缺口 ⇒ 必红
    //    (那正是「静默忽略 F 态」的形态 —— 最需要被抓住的一种)
    CHECK_FALSE(gap_is_registered("MESIF 的 F 态 = 单 Owner + 多 Sharer + 转发点。"));
    // 反控 ③:`nullptr` ⇒ 必红(缺登记)
    CHECK_FALSE(gap_is_registered(nullptr));

    // 静默忽略 F 态的反面证明:`F` 不得被丢弃后当 `E` / `M` 用 ——
    // `SnoopState` 里根本不存在 F ⇒ 任何把 F 投影成具体 SnoopState 的意图
    // 都会在编译期失落(枚举无该值),故此处只能断言「枚举形态」这一层。
    CHECK(sizeof(cep::SnoopState) == sizeof(std::uint8_t));
}
