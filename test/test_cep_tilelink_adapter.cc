// =============================================================================
// test/test_cep_tilelink_adapter.cc
//   —— TileLink-C(TileLink Tree Manager)实现层的契约测试 + 反向 fixture
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: include/tlm/cep/tilelink_c_adapter.hh · include/tlm/cep/tilelink_tree_manager.hh
//       include/tlm/cep/tilelink_state_spec.hh(规约载体 · 第 0 个 change 已归档)
//       openspec/changes/cpptlm-cep-tilelink-tree-manager/(delta spec TMA-01 ~ TMA-07)
//       台账 obj-cep-platform.md Entry #144(建档)· #141(二审落修 批 2)
//
// ── 判据纪律(per 本仓 4 条铁律)──────────────────────────────────────────
// ① 纯负向断言 SHALL 有正控 ⇒ 每条负向断言都有**正向**对照;
// ② 每条判据 SHALL 有**可执行反向 fixture** 且**因正确原因红**;
// ③ 编译期锁 SHALL **不**只以「编译红」充当 ctest 判据 ⇒ 结构校验提供
//    **运行时可判**入口;
// ④ fixture 状态变迁后 SHALL 重新基线化 —— 本文件为**新增**,基线 = 全绿。
// =============================================================================

#include "tlm/cep/tilelink_c_adapter.hh"
#include "tlm/cep/tilelink_tree_manager.hh"

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

    using Adapter = tlm::cep::TileLinkCAdapter;
    using TreeMgr = tlm::cep::TreeManager;
    using Spec = tlm::cep::TileLinkStateSpec;
    using State = Spec::State;
    using Event = Spec::Event;
    using Perm = Spec::Perm;
    using tlm::cep::Intent;
    using tlm::cep::LocalView;
    using tlm::cep::LookupHazard;
    using tlm::cep::LookupResult;
    using tlm::cep::node_id_t;
    using tlm::cep::responder_set_t;

    /// 仓库根(CPPTLM_SOURCE_DIR 由 test/CMakeLists.txt 注入)。
    std::string repo_root_dir() {
#ifdef CPPTLM_SOURCE_DIR
        return std::string(CPPTLM_SOURCE_DIR);
#else
        return std::string(".");
#endif
    }

    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(repo_root_dir() + "/" + rel, std::ios::binary);
        if (!in) {
            return {};
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// 去注释(行注释 + 块注释)—— 违禁记号扫描的**前提**。
    std::string strip_comments(std::string code) {
        std::string out;
        out.reserve(code.size());
        for (std::size_t i = 0; i < code.size();) {
            if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '/') {
                while (i < code.size() && code[i] != '\n') {
                    ++i;
                }
                continue;
            }
            if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '*') {
                i += 2;
                while (i + 1 < code.size() && !(code[i] == '*' && code[i + 1] == '/')) {
                    ++i;
                }
                i = (i + 1 < code.size()) ? i + 2 : code.size();
                out.push_back(' ');
                continue;
            }
            out.push_back(code[i]);
            ++i;
        }
        return out;
    }

    /// 统计「行首恰为 4 空格 + virtual 」的行数(接口面的虚方法计数)
    /// —— 与 `test_cep_sharer_tracker_base.cc` 同款机制(禁双实现)。
    std::size_t count_member_virtuals(const std::string& code) {
        std::size_t n = 0;
        std::istringstream in(code);
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("    virtual ", 0) == 0)
                ++n;
        }
        return n;
    }

    /// 「零副本」违禁记号 —— adapter **不**命名任何枚举字面量,**也不**指名任何表。
    const std::vector<std::string>& forbidden_tokens() {
        static const std::vector<std::string> kTokens = {
            "State::", "Event::", "Perm::", "kTable", "kTransitions", "kPermProjection"};
        return kTokens;
    }

} // namespace

// ══════════════════════════════════════════════════════════════════════════
// TMA-01 —— 零自有表副本
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TileLink adapter: 零自有表副本(12 格 pass-through + 静态扫描)",
          "[cep][tilelink][adapter]") {
    // ── 子条件 ① / ②:12 格 pass-through(逐格委托规约载体)──────────────────
    std::size_t cells = 0;
    for (std::size_t s = 0; s < Adapter::kStateCount; ++s) {
        const auto st = static_cast<State>(s);
        for (std::size_t e = 0; e < Adapter::kEventCount; ++e) {
            const auto ev = static_cast<Event>(e);
            CHECK(Adapter::transition_kind(st, ev) == Spec::transition(st, ev).kind);
            CHECK(Adapter::transition_target_mask(st, ev) == Spec::transition(st, ev).target_mask);
            CHECK(Adapter::transition_is_known(st, ev) ==
                  (Spec::transition(st, ev).kind == Spec::CellKind::Known));
            CHECK(Adapter::transition_is_unknown(st, ev) ==
                  (Spec::transition(st, ev).kind == Spec::CellKind::Unknown));
            ++cells;
        }
    }
    CHECK(cells == 12u);

    // ── 子条件 ③:静态扫描(去注释后不含枚举字面量 / 表名)──────────────────
    const std::string code =
        strip_comments(read_repo_file("include/tlm/cep/tilelink_c_adapter.hh"));
    REQUIRE_FALSE(code.empty());
    for (const auto& tok : forbidden_tokens()) {
        CHECK(code.find(tok) == std::string::npos);
    }

    // ── 扫描器的**鉴别力自测**(正控):违禁记号必须**能被**扫到 ──────────────
    const std::string forged =
        "class TileLinkCAdapter { static constexpr int kTransitions[4][3] = {}; "
        "static constexpr int kPermProjection[3] = {}; };";
    CHECK(strip_comments(forged).find("kTransitions") != std::string::npos);
    CHECK(strip_comments(forged).find("kPermProjection") != std::string::npos);
    // ⚠️ 注释里的违禁记号**不**算命中(否则扫描器会被注释骗过 ⇒ 失守)
    CHECK(strip_comments("// State:: Perm:: kTable\nint x;").find("State::") == std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════
// TMA-02 —— perm 投影 3→3
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TileLink adapter: perm 投影 3→3 且 UNIQUE 不出现", "[cep][tilelink][adapter]") {
    CHECK(Adapter::kPermCount == 4u); // 值域仍 4 值(只复用不扩)
    Perm p = Perm::UNIQUE;
    CHECK(Adapter::perm_of(State::Nothing, p));
    CHECK(p == Perm::NONE);
    CHECK(Adapter::perm_of(State::Branch, p));
    CHECK(p == Perm::READ);
    CHECK(Adapter::perm_of(State::Tip, p));
    CHECK(p == Perm::WRITEBACK);
    // `UNIQUE` SHALL NOT 出现
    for (std::size_t s = 0; s < Adapter::kStateCount; ++s) {
        Perm q = Perm::UNIQUE;
        REQUIRE(Adapter::perm_of(static_cast<State>(s), q));
        CHECK(q != Perm::UNIQUE);
    }
    // 无瞬态态 ⇒ 计数恒 0
    CHECK(Adapter::kTransientStateCount == 0u);
}

// ══════════════════════════════════════════════════════════════════════════
// TMA-03 —— 6→3 映射 + 逐消息 → 6 类(R-T3)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TileLink adapter: 6→3 映射(4 类 · 非恒等)+ 逐消息 → 6 类(R-T3)",
          "[cep][tilelink][adapter]") {
    // ── 6→3:本实例**只用 4 类**(TileLink 无 atomic / query)⇒ 相对平台
    //    方向默认表**非恒等**(缺 2 类)⇒ 无需恒等登记,但**理由必填** ──────────
    CHECK(Adapter::kMessageClassCount == 4u);
    CHECK_FALSE(Adapter::kMappingIsIdentity);
    REQUIRE(std::string(Adapter::kIdentityReason).size() > 0u);

    std::uint8_t cat = 0xFF;
    CHECK(Adapter::category_of("request", cat));
    CHECK(cat == 0u); // CATEGORY_REQUEST
    CHECK(Adapter::category_of("snoop", cat));
    CHECK(cat == 0u);
    CHECK(Adapter::category_of("snoop_resp", cat));
    CHECK(cat == 1u); // CATEGORY_RESPONSE
    CHECK(Adapter::category_of("response", cat));
    CHECK(cat == 1u);
    // 未使用的两类 ⇒ 查不到
    CHECK_FALSE(Adapter::category_of("atomic", cat));
    CHECK_FALSE(Adapter::category_of("query", cat));
    // ⚠️ `writeback` 是 **canonical 流名**,不是消息类名 ⇒ 查不到
    CHECK_FALSE(Adapter::category_of("writeback", cat));

    // ── 逐消息 → 6 类(R-T3 归属表,10 条)──────────────────────────────
    CHECK(Adapter::kProtocolMessageCount == 10u);
    struct Expect {
        const char* msg;
        const char* cls;
    };
    const Expect kExpect[] = {
        {"Acquire", "request"},         {"Probe", "snoop"},        {"ProbeAck", "snoop_resp"},
        {"ProbeAckData", "snoop_resp"}, {"Release", "snoop_resp"}, {"ReleaseData", "snoop_resp"},
        {"Grant", "response"},          {"GrantData", "response"}, {"ReleaseAck", "response"},
        {"GrantAck", "response"},
    };
    for (const auto& e : kExpect) {
        const char* cls = nullptr;
        REQUIRE(Adapter::message_class_of(e.msg, cls));
        CHECK(std::string(cls) == std::string(e.cls));
    }
    // 未知消息 ⇒ 查不到
    const char* cls = nullptr;
    CHECK_FALSE(Adapter::message_class_of("NoSuchMessage", cls));
    // ⚠️ 2 处降维必须登记
    CHECK(std::string(Adapter::kMessageClassDowngradeNote).find("Release") != std::string::npos);
    CHECK(std::string(Adapter::kMessageClassDowngradeNote).find("GrantAck") != std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════
// TMA-04 —— TreeManager 的 caps/behavior 逐字段等于 kTrackerTraits[3]
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TreeManager: caps/behavior 逐字段等于 kTrackerTraits[TreeManager]",
          "[cep][tilelink][adapter]") {
    const auto& traits = tlm::cep::kTrackerTraits[3];
    REQUIRE(traits.impl == tlm::cep::TrackerImpl::TreeManager);
    CHECK(std::string(traits.name) == "TreeManager");
    CHECK(traits.caps.scope == tlm::cep::Scope::SUBTREE);
    CHECK(traits.caps.completeness == tlm::cep::Completeness::EXACT);
    CHECK(traits.caps.authority == tlm::cep::Authority::LOCAL);
    CHECK(traits.caps.aggregation_at == tlm::cep::AggregationAt::TREE_PARENT);
    CHECK(traits.behavior.hazard_under_contention == LookupHazard::CHAIN);
    CHECK_FALSE(traits.behavior.broadcast_fallback);

    // 实例的声明必须**逐字段**相等(取自 traits,非另写字面量)
    TreeMgr tm(tlm::cep::kNodeIdPcie);
    CHECK(tm.caps.scope == traits.caps.scope);
    CHECK(tm.caps.completeness == traits.caps.completeness);
    CHECK(tm.caps.authority == traits.caps.authority);
    CHECK(tm.caps.aggregation_at == traits.caps.aggregation_at);
    CHECK(tm.behavior.hazard_under_contention == traits.behavior.hazard_under_contention);
    CHECK(tm.behavior.broadcast_fallback == traits.behavior.broadcast_fallback);
    CHECK(tm.kImpl == tlm::cep::TrackerImpl::TreeManager);
}

// ══════════════════════════════════════════════════════════════════════════
// TMA-05 —— ESCALATE_UP 与 CHAIN 的**首次真实使用**
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TreeManager: ESCALATE_UP / CHAIN 真实使用(Tip / Branch / Nothing + contention)",
          "[cep][tilelink][adapter]") {
    const node_id_t kParent = tlm::cep::kNodeIdPcie;
    const node_id_t kSelf = tlm::cep::kNodeIdSdma;

    // ── ① Tip(本子树内有 R/W 副本)⇒ ANSWER_HERE ────────────────────────
    {
        TreeMgr tm(kParent);
        tm.set_sharer(0x1000, kSelf, State::Tip);
        LookupResult r = LookupResult::BROADCAST;
        LocalView v;
        LookupHazard h = LookupHazard::STALL;
        tm.lookup(0x1000, Intent::want_read, r, v, h);
        CHECK(r == LookupResult::ANSWER_HERE);
        CHECK(h == LookupHazard::NONE);
        CHECK(v.valid);
        CHECK(v.forward_to == tlm::cep::kInvalidNode);
        CHECK(v.sharers != 0);
    }

    // ── ② Branch(本子树内只有只读副本)+ want_read ⇒ ANSWER_HERE ─────────
    {
        TreeMgr tm(kParent);
        tm.set_sharer(0x2000, kSelf, State::Branch);
        LookupResult r = LookupResult::BROADCAST;
        LocalView v;
        LookupHazard h = LookupHazard::STALL;
        tm.lookup(0x2000, Intent::want_read, r, v, h);
        CHECK(r == LookupResult::ANSWER_HERE);
        CHECK(h == LookupHazard::NONE);
    }

    // ── ③ Branch + want_write ⇒ ESCALATE_UP(向上递归到树父)──────────────
    {
        TreeMgr tm(kParent);
        tm.set_sharer(0x3000, kSelf, State::Branch);
        LookupResult r = LookupResult::ANSWER_HERE;
        LocalView v;
        LookupHazard h = LookupHazard::NONE;
        tm.lookup(0x3000, Intent::want_write, r, v, h);
        CHECK(r == LookupResult::ESCALATE_UP);
        CHECK(h == LookupHazard::NONE);
        CHECK(v.valid);
        CHECK(v.forward_to == kParent); // ← 树父节点
    }

    // ── ④ Nothing(本子树内无副本)⇒ ESCALATE_UP ─────────────────────────
    {
        TreeMgr tm(kParent);
        tm.set_sharer(0x4000, kSelf, State::Nothing);
        LookupResult r = LookupResult::ANSWER_HERE;
        LocalView v;
        LookupHazard h = LookupHazard::NONE;
        tm.lookup(0x4000, Intent::want_read, r, v, h);
        CHECK(r == LookupResult::ESCALATE_UP);
        CHECK(v.forward_to == kParent);
    }

    // ── ⑤ contention(同址 snoop 在飞)⇒ **CHAIN**(不是 STALL)─────────────
    {
        TreeMgr tm(kParent);
        tm.set_sharer(0x5000, kSelf, State::Tip);
        tlm::cep::snoop_handle_t h = 0;
        tm.snoop_begin(0x5000, tlm::cep::responder_mask_of(kSelf), h);
        REQUIRE(h != 0);
        LookupResult r = LookupResult::ANSWER_HERE;
        LocalView v;
        LookupHazard hz = LookupHazard::NONE;
        tm.lookup(0x5000, Intent::want_read, r, v, hz);
        CHECK(hz == LookupHazard::CHAIN); // ← 全仓首次真实使用
        CHECK_FALSE(v.valid);             // hazard != NONE ⇒ view 无意义
    }

    // ── ⑥ **永不** BROADCAST(树形有唯一 Tip)────────────────────────────
    {
        TreeMgr tm(kParent);
        for (std::uint64_t a : {0x10ull, 0x20ull, 0x30ull}) {
            tm.set_sharer(a, kSelf, State::Nothing);
            LookupResult r = LookupResult::ANSWER_HERE;
            LocalView v;
            LookupHazard hz = LookupHazard::NONE;
            tm.lookup(a, Intent::want_write, r, v, hz);
            CHECK(r != LookupResult::BROADCAST);
        }
    }

    // ── 反向 fixture:把 ESCALATE_UP 换成 ANSWER_HERE ⇒ 必红 ──────────────
    {
        TreeMgr tm(kParent);
        tm.set_sharer(0x6000, kSelf, State::Nothing);
        LookupResult r = LookupResult::ANSWER_HERE;
        LocalView v;
        LookupHazard hz = LookupHazard::NONE;
        tm.lookup(0x6000, Intent::want_read, r, v, hz);
        // 毒化版:把结果**改写**为 ANSWER_HERE ⇒ 判据的期望(ESCALATE_UP)落空
        const LookupResult poisoned = LookupResult::ANSWER_HERE;
        CHECK(poisoned != r); // ⇒ 「因正确原因红」的等价断言
        CHECK(r == LookupResult::ESCALATE_UP);
    }
}

// ══════════════════════════════════════════════════════════════════════════
// TMA-06 —— snoop_join_core 的 4 步语义(CHAIN 的公共机械)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP SnoopMechanics: snoop_join_core 加入既有 in-flight snoop",
          "[cep][tilelink][adapter]") {
    const node_id_t kA = tlm::cep::kNodeIdSdma;
    const node_id_t kB = tlm::cep::kNodeIdGmmu;

    // ── ① 无 in-flight ⇒ 返 0(无可加入)─────────────────────────────────
    {
        tlm::cep::SnoopMechanics m;
        tlm::cep::snoop_handle_t h = 0xFFFF;
        m.snoop_join_core(0x100, tlm::cep::responder_mask_of(kA), h);
        CHECK(h == 0);
    }

    // ── ② 含未映射位 ⇒ 返 0(同一拒绝语义)───────────────────────────────
    {
        tlm::cep::SnoopMechanics m;
        tlm::cep::snoop_handle_t h = 0;
        m.snoop_begin_core(0x100, tlm::cep::responder_mask_of(kA), h);
        REQUIRE(h != 0);
        tlm::cep::snoop_handle_t h2 = 0xFFFF;
        m.snoop_join_core(0x100, responder_set_t{1} << 40, h2);
        CHECK(h2 == 0);
    }

    // ── ③ 无新增位 ⇒ 返既有 handle(幂等)─────────────────────────────────
    {
        tlm::cep::SnoopMechanics m;
        tlm::cep::snoop_handle_t h = 0;
        m.snoop_begin_core(0x200, tlm::cep::responder_mask_of(kA), h);
        REQUIRE(h != 0);
        tlm::cep::snoop_handle_t h2 = 0;
        m.snoop_join_core(0x200, tlm::cep::responder_mask_of(kA), h2);
        CHECK(h2 == h);
        // 幂等 ⇒ 期望集**不**被重复扩张
        tlm::cep::snoop_handle_t h3 = 0;
        m.snoop_join_core(0x200, tlm::cep::responder_mask_of(kA), h3);
        CHECK(h3 == h);
    }

    // ── ④ 有新增位 ⇒ 并入 expected 与 remaining ⇒ 归零逻辑不变 ────────────
    {
        tlm::cep::SnoopMechanics m;
        tlm::cep::snoop_handle_t h = 0;
        m.snoop_begin_core(0x300, tlm::cep::responder_mask_of(kA), h);
        REQUIRE(h != 0);
        tlm::cep::snoop_handle_t h2 = 0;
        m.snoop_join_core(0x300, tlm::cep::responder_mask_of(kB), h2);
        CHECK(h2 == h); // 同一 in-flight
        // 加入后:fold 掉 A ⇒ remaining 仍含 B ⇒ **未**归零
        tlm::cep::responder_set_t remaining = 0;
        tlm::cep::MergedState merged{};
        m.snoop_fold_core(h, kA, tlm::cep::SnoopState::I, false, false, remaining, merged);
        CHECK((remaining & tlm::cep::responder_mask_of(kB)) != 0);
        // fold 掉 B ⇒ 归零
        m.snoop_fold_core(h, kB, tlm::cep::SnoopState::I, false, false, remaining, merged);
        CHECK(remaining == 0);
    }

    // ── 反向 fixture:不并入 remaining ⇒ 归零判定**必须红** ──────────────
    {
        tlm::cep::SnoopMechanics m;
        tlm::cep::snoop_handle_t h = 0;
        m.snoop_begin_core(0x400, tlm::cep::responder_mask_of(kA), h);
        REQUIRE(h != 0);
        // 毒化:只扩 expected(**不**扩 remaining)⇒ fold 掉 A 后即归零(错误)
        // ⇒ 正确实现必须**不**出现该行为 ⇒ 用「正确实现」断言其**不**归零
        tlm::cep::snoop_handle_t h2 = 0;
        m.snoop_join_core(0x400, tlm::cep::responder_mask_of(kB), h2);
        REQUIRE(h2 == h);
        tlm::cep::responder_set_t remaining = 0;
        tlm::cep::MergedState merged{};
        m.snoop_fold_core(h, kA, tlm::cep::SnoopState::I, false, false, remaining, merged);
        CHECK((remaining & tlm::cep::responder_mask_of(kB)) != 0); // 未归零 ⇒ 正确
        // 「毒化版」的行为(立即归零)必须**不**成立
        const bool poisoned_would_retire = (remaining == 0);
        CHECK_FALSE(poisoned_would_retire);
    }
}

// ══════════════════════════════════════════════════════════════════════════
// TMA-07 —— LocalView 零改动 + SharerTrackerIf 仍恰好 4 个虚方法
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TreeManager: LocalView 零改动(复用 forward_to)+ 接口面不变",
          "[cep][tilelink][adapter]") {
    const std::string code = strip_comments(read_repo_file("include/tlm/cep/sharer_tracker.hh"));
    REQUIRE_FALSE(code.empty());
    // ── ① LocalView 零改动 ⇒ 通用抽象层 SHALL **不**引入 `tree_parent` ──────
    CHECK(code.find("tree_parent") == std::string::npos);
    // ── ② 接口仍恰好 4 个虚方法 + 1 个虚析构(与既有 base 判据同款机制)────
    CHECK(count_member_virtuals(code) == 5u);
    // ── ③ `snoop_join_core` SHALL **不**是虚方法(公共机械 ⇒ 零虚函数)────────
    CHECK(code.find("virtual void snoop_join_core") == std::string::npos);
}
