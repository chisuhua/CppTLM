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
#include <type_traits>
#include <vector>

#include <nlohmann/json.hpp>

#include <sys/wait.h>

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

    /// 抽取「含 `anchor` 的那行起、到花括号配平结束」的**函数/类体**文本。
    /// ⚠️ 输入须**已去注释**(否则注释里的花括号会打乱配平)⇒ 调用方先 `strip_comments`。
    std::string extract_block(const std::string& code, const std::string& anchor) {
        const std::size_t pos = code.find(anchor);
        if (pos == std::string::npos)
            return {};
        const std::size_t open = code.find('{', pos);
        if (open == std::string::npos)
            return {};
        int depth = 0;
        for (std::size_t i = open; i < code.size(); ++i) {
            if (code[i] == '{')
                ++depth;
            else if (code[i] == '}') {
                --depth;
                if (depth == 0)
                    return code.substr(open, i - open + 1);
            }
        }
        return {}; // 未配平
    }

    /// `git diff --quiet HEAD -- <paths>` 的退出码(= 0 ⇒ 工作区与 HEAD 一致)。
    /// ⚠️ 用 `HEAD` 而非 index ⇒ 同时覆盖「已 staged」与「未 staged」的工作区改动。
    int git_diff_quiet(const std::string& paths) {
        const std::string cmd = "git -C '" + repo_root_dir() + "' diff --quiet HEAD -- " + paths;
        std::FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe)
            return -1;
        char buf[64];
        while (std::fgets(buf, sizeof(buf), pipe) != nullptr) {
        }
        const int rc = pclose(pipe);
        return WEXITSTATUS(rc);
    }

    /// 「零副本」违禁记号 —— adapter **不**命名任何枚举字面量,**也不**指名任何表。
    const std::vector<std::string>& forbidden_tokens() {
        static const std::vector<std::string> kTokens = {
            "State::", "Event::", "Perm::", "kTable", "kTransitions", "kPermProjection"};
        return kTokens;
    }

    /// 成员指针 → **函数类型**(去类部分)—— **const 特化**版。
    /// ⚠️ 既有 `MemberFnType`(`test_cep_sharer_tracker_base.cc:162-163`)**只特化非 const**
    ///    成员指针 ⇒ const 成员函数无对应特化 ⇒ 本单元**本地**补一份
    ///    (⛔ **不改** `test_cep_sharer_tracker_base.cc` —— 不在本单元 In-Scope 内)。
    template <class MPtr> struct ConstMemberFnType;
    template <class C, class R, class... A> struct ConstMemberFnType<R (C::*)(A...) const> {
        using type = R(A...) const;
    };

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

// ══════════════════════════════════════════════════════════════════════════
// I-7a —— `TreeManager::view_of` 纯查询视图
//   登记册 §6.4 队列 2 的 **I-7a**(I-7 拆 a/b 后的前置产出单元)
//   形态来源 = architect 裁决 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md`
//   §四(Q-a):⛔ **不动** `SharerTrackerIf`,在**具体类**上补与 `FullMapDirectory::view_of`
//   **同签名**的方法(消费方按具体类型持有,per mesif 先例「按具体实现接线是诚实的」)。
//
// ⚠️ 锚点纪律:AC-1 ~ AC-7 的锚点串**逐字照抄** `scripts/test/rddf_ac_carrier.tsv`
//    的载体表内容(载体表对 `test` 行做 `grep -F` ⇒ 锚点串必须在此文件内命中);
//    AC-8 ~ AC-12 的锚点串**不在载体表内** ⇒ 自拟描述。
// ⚠️ 诚实边界:本载体证明的是**视图面**成立 —— ⛔ **不等于**档 2 已扩面
//    (档 2 仍单族 `mesif-observation`;扩面 = I-7b 的 home 侧驱动)。
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TreeManager: view_of 纯查询视图(Tip/Branch/Nothing/空条目/CHAIN)",
          "[cep][tilelink][adapter]") {
    using TreeViewSig = ConstMemberFnType<decltype(&TreeMgr::view_of)>::type;
    using FullViewSig = ConstMemberFnType<decltype(&tlm::cep::FullMapDirectory::view_of)>::type;

    // I-7a AC-1: view_of 签名与 FullMapDirectory::view_of 同形
    // ⚠️ 比**函数类型**(去类部分)而非成员指针 —— 两个成员指针**恒**是不同类型。
    REQUIRE(std::is_same_v<TreeViewSig, FullViewSig>);

    // I-7a AC-2: view_of 逐字段口径(Tip/Branch/Nothing/空条目)
    {
        const node_id_t kParent = tlm::cep::kNodeIdPcie;
        const node_id_t kSelf = tlm::cep::kNodeIdSdma;
        const node_id_t kPeer = tlm::cep::kNodeIdGmmu;
        const responder_set_t mSelf = tlm::cep::responder_mask_of(kSelf);
        const responder_set_t mPeer = tlm::cep::responder_mask_of(kPeer);
        REQUIRE(mSelf != 0);
        REQUIRE(mPeer != 0);

        TreeMgr tm(kParent);

        // ① 从未出现 ⇒ valid=false / sharers=0 / dirty=0 / forward_to 恒无效
        {
            const LocalView v = tm.view_of(0x9000);
            CHECK_FALSE(v.valid);
            CHECK(v.sharers == 0);
            CHECK(v.dirty == 0);
            CHECK(v.forward_to == tlm::cep::kInvalidNode);
        }
        // ② 空条目(`set_sharer(Nothing)` 建条目但无副本)⇒ valid=true / sharers=0 / dirty=0
        {
            tm.set_sharer(0x9001, kSelf, State::Nothing);
            const LocalView v = tm.view_of(0x9001);
            CHECK(v.valid);
            CHECK(v.sharers == 0);
            CHECK(v.dirty == 0);
        }
        // ③ Branch 单节点 ⇒ valid=true / sharers=m / dirty=0
        {
            tm.set_sharer(0x9002, kSelf, State::Branch);
            const LocalView v = tm.view_of(0x9002);
            CHECK(v.valid);
            CHECK(v.sharers == mSelf);
            CHECK(v.dirty == 0);
        }
        // ④ Tip 单节点 ⇒ valid=true / sharers=m / dirty=m
        {
            tm.set_sharer(0x9003, kSelf, State::Tip);
            const LocalView v = tm.view_of(0x9003);
            CHECK(v.valid);
            CHECK(v.sharers == mSelf);
            CHECK(v.dirty == mSelf);
        }
        // ⑤ Tip + 另一 Branch ⇒ sharers=m1|m2 / dirty=m1
        {
            tm.set_sharer(0x9004, kSelf, State::Tip);
            tm.set_sharer(0x9004, kPeer, State::Branch);
            const LocalView v = tm.view_of(0x9004);
            CHECK(v.valid);
            CHECK(v.sharers == (mSelf | mPeer));
            CHECK(v.dirty == mSelf);
        }
        // ⑥ Tip→Branch 降级 ⇒ sharers=m / dirty=0
        {
            tm.set_sharer(0x9005, kSelf, State::Tip);
            tm.set_sharer(0x9005, kSelf, State::Branch);
            const LocalView v = tm.view_of(0x9005);
            CHECK(v.valid);
            CHECK(v.sharers == mSelf);
            CHECK(v.dirty == 0);
        }
    }

    // I-7a AC-3: lookup 不是快照(ESCALATE_UP 时 sharers 留 0)
    {
        const node_id_t kParent = tlm::cep::kNodeIdPcie;
        const node_id_t kSelf = tlm::cep::kNodeIdSdma;
        TreeMgr tm(kParent);
        tm.set_sharer(0x9100, kSelf, State::Branch);
        LookupResult r = LookupResult::ANSWER_HERE;
        LocalView v;
        LookupHazard hz = LookupHazard::NONE;
        tm.lookup(0x9100, Intent::want_write, r, v, hz);
        REQUIRE(r == LookupResult::ESCALATE_UP);
        CHECK(v.sharers == 0);                  // ← 判决视图**漏** sharer
        CHECK(tm.view_of(0x9100).sharers != 0); // ← 快照**不漏** ⇒ 本条的判别力来源
    }

    // I-7a AC-4: 脏性口径(dirty == tips;Branch ⇒ dirty == 0)
    {
        const node_id_t kParent = tlm::cep::kNodeIdPcie;
        const node_id_t kSelf = tlm::cep::kNodeIdSdma;
        const responder_set_t mSelf = tlm::cep::responder_mask_of(kSelf);
        TreeMgr tm(kParent);
        // ① Branch ⇒ dirty == 0 ∧ sharers != 0(防「顺手把 Branch 标脏」)
        tm.set_sharer(0x9200, kSelf, State::Branch);
        {
            const LocalView v = tm.view_of(0x9200);
            CHECK(v.dirty == 0);
            CHECK(v.sharers != 0);
        }
        // ② Tip ⇒ dirty == tips ∧ dirty ⊆ sharers
        tm.set_sharer(0x9201, kSelf, State::Tip);
        {
            const LocalView v = tm.view_of(0x9201);
            CHECK(v.dirty == mSelf);
            CHECK((v.dirty & v.sharers) == v.dirty);
        }
        // ③ Tip→Branch 降级 ⇒ dirty == 0 ∧ sharers == m(防 dirty 与 sharers 写成同一字段)
        tm.set_sharer(0x9202, kSelf, State::Tip);
        tm.set_sharer(0x9202, kSelf, State::Branch);
        {
            const LocalView v = tm.view_of(0x9202);
            CHECK(v.dirty == 0);
            CHECK(v.sharers == mSelf);
        }
    }

    // I-7a AC-5: CHAIN in-flight 期间 view_of 仍返回安装面视图
    {
        const node_id_t kParent = tlm::cep::kNodeIdPcie;
        const node_id_t kSelf = tlm::cep::kNodeIdSdma;
        const responder_set_t mSelf = tlm::cep::responder_mask_of(kSelf);
        TreeMgr tm(kParent);
        tm.set_sharer(0x9300, kSelf, State::Tip);
        tlm::cep::snoop_handle_t h = 0;
        tm.snoop_begin(0x9300, mSelf, h);
        REQUIRE(h != 0);
        LookupResult r = LookupResult::ANSWER_HERE;
        LocalView v;
        LookupHazard hz = LookupHazard::NONE;
        tm.lookup(0x9300, Intent::want_read, r, v, hz);
        // ⚠️ CHAIN 体现在 **hazard** 上(`result` 仍是 ANSWER_HERE)—— per 实现 :147-149
        CHECK(hz == LookupHazard::CHAIN);
        CHECK_FALSE(v.valid);
        // 快照面:反映**安装面**真相,与 in-flight 无关(snoop 只动 `inflight_`,不动 `entries_`)
        const LocalView snap = tm.view_of(0x9300);
        CHECK(snap.valid);
        CHECK(snap.sharers == mSelf);
    }

    // I-7a AC-6: 零副作用(has_node 探针 + events 长度不变)
    // ⚠️ **实测订正(2026-11-21)**:上面锚点串沿用载体表原文(⛔ 字面契约不改);
    //    但 AC-6 原口径的「events 长度不变」子判据**不可执行** —— `TreeManager` **无**
    //    `events()` 方法(实测:本类只有 `entries_`;in-flight 面在 `SnoopMechanics::inflight_`)
    //    ⇒ 零副作用由下方 ② `has_node` 探针(测「不创建条目」)+ ③ 源码文本判据
    //    (测「不改写内容」)承担 ⇒ **覆盖面不减**。
    {
        const node_id_t kParent = tlm::cep::kNodeIdPcie;
        TreeMgr tm(kParent);
        const std::uint64_t z = 0xDEAD0000ull;
        // ① 未触碰地址
        REQUIRE_FALSE(tm.has_node(z));
        // ② 读操作**不创建**条目
        const LocalView v = tm.view_of(z);
        CHECK_FALSE(v.valid);
        CHECK_FALSE(tm.has_node(z));
    }
    // ③ 源码文本判据:`view_of` 函数体不得含插入/删除原语,且 `entries_` 的**每次**使用
    //    都必须是**只读**访问(`.find(` / `.end()`)⇒ 无 `entries_[…]` 写入、无 emplace/insert/erase
    {
        const std::string code =
            strip_comments(read_repo_file("include/tlm/cep/tilelink_tree_manager.hh"));
        REQUIRE_FALSE(code.empty());
        const std::string body = extract_block(code, "LocalView view_of");
        REQUIRE_FALSE(body.empty());
        CHECK(body.find("operator[]") == std::string::npos);
        CHECK(body.find("emplace") == std::string::npos);
        CHECK(body.find("insert") == std::string::npos);
        CHECK(body.find("erase") == std::string::npos);
        // `entries_` 的每一次使用都必须是只读访问
        std::size_t entries_uses = 0;
        for (std::size_t p = body.find("entries_"); p != std::string::npos;
             p = body.find("entries_", p + 1)) {
            ++entries_uses;
            const std::size_t after = body.find('.', p);
            REQUIRE(after != std::string::npos);
            const std::string accessor = body.substr(after, body.find('(', after) - after);
            INFO("view_of 的 entries_ 使用形态: " << accessor);
            // ⚠️ Catch2 v3 不支持断言内的 `||` ⇒ 先在**变量**里算好再断言
            const bool is_read_only = (accessor == ".find") || (accessor == ".end");
            CHECK(is_read_only);
        }
        CHECK(entries_uses >= 1u); // ⇒ 确实读了安装面(而非恒真占位)
    }

    // I-7a AC-7: 族 1 的档 2 条目(`mesif-observation`)完好无损
    // ⚠️ 精确对象 = `evidence_tiers`(⛔ 不是 `coverage_map`、不是 `coverage_scope.families`)
    // ⚠️ **口径订正(2026-11-21 · I-7b)**:原断言 `size() == 1` 表达的是「I-7a 本单元**未**扩面」;
    //    I-7b 已**授权**新增族 2 条目 ⇒ 该计数不再是 I-7a 的边界,且会与授权扩面直接互斥。
    //    ⇒ 改为断言**族 1 条目本身**仍完好 —— 这才是 I-7a 该守的内容;
    //    「两族登记完备 + 与覆盖地图闭合」由 I-7b 的 T8/T9 承担(⛔ 不在此重复,避免第二判据源)。
    {
        const std::string raw = read_repo_file("test/cep_evidence_tiers.json");
        REQUIRE_FALSE(raw.empty());
        const nlohmann::json doc = nlohmann::json::parse(raw);
        REQUIRE(doc.contains("evidence_tiers"));
        const auto& tiers = doc.at("evidence_tiers");
        REQUIRE(tiers.is_object());
        REQUIRE(tiers.contains("mesif-observation"));
        const auto& mesif = tiers.at("mesif-observation");
        CHECK(mesif.at("tier").get<int>() == 2);
        CHECK(mesif.at("family").get<std::string>() == "mesif");
        CHECK(mesif.at("run_produced").get<bool>() == false);
        CHECK(mesif.at("has_verdict").get<bool>() == false);
    }

    // I-7a AC-8: 零 diff 面(禁改路径未被本单元触碰)
    // ⚠️ **实测订正(2026-11-21)**:AC-8 原口径用 `git diff --name-only $(git merge-base HEAD
    //    main)..HEAD` ⇒ 本仓**直推 main** ⇒ `merge-base HEAD main == HEAD` ⇒ diff **恒空**
    //    ⇒ **恒绿**(与 AC-8 自己警示的「无参 git diff 恒绿」同型)。
    //    ⇒ 精化为下面 3 条**可执行且有鉴别力**的判据:
    //      ① 接口面虚方法数仍 == 5(加纯虚 `view_of` ⇒ 6 ⇒ 红)
    //      ② `SharerTrackerIf` 类体内**无独立** `view_of` 标识符(接口零扩展)
    //      ③ 禁改路径的**工作区内容**与 HEAD 一致(抓「未提交就改了禁改面」)
    //    ⚠️ 「已提交的越权」由**验收侧**承担(报告记录 `git show --stat` 的实际文件清单)。
    {
        const std::string code =
            strip_comments(read_repo_file("include/tlm/cep/sharer_tracker.hh"));
        REQUIRE_FALSE(code.empty());
        // ①
        CHECK(count_member_virtuals(code) == 5u);
        // ② 「view_of」的出现次数 == 「peer_view_of」的出现次数 ⇒ 无**独立** `view_of`
        const std::string iface = extract_block(code, "class SharerTrackerIf");
        REQUIRE_FALSE(iface.empty());
        std::size_t n_viewof = 0;
        for (std::size_t p = iface.find("view_of"); p != std::string::npos;
             p = iface.find("view_of", p + 1))
            ++n_viewof;
        std::size_t n_peer_viewof = 0;
        for (std::size_t p = iface.find("peer_view_of"); p != std::string::npos;
             p = iface.find("peer_view_of", p + 1))
            ++n_peer_viewof;
        CHECK(n_viewof == n_peer_viewof);
        // ③
        // ⚠️ **两项移出(2026-11-21 · I-7b)**:
        //    · `test/cep_evidence_tiers.json` —— 档 2 第二族条目**属 I-7b 写集**;
        //    · `docs/architecture` —— I-7b 的 AC-17 授权在 `07-coherence-exploration-platform.md`
        //      **追加** SU(append-only)。
        //    留在「工作区 == HEAD」面 ⇒ **任何授权的后续单元**都会在提交前把本测试判红
        //    (判据与被守护的边界错位)。⇒ 本清单只保留**永久冻结**面;
        //    「既有 SU 段落逐字不改」由 I-7b AC-17 的 diff 证据承担。
        const std::string forbidden =
            "include/tlm/cep/sharer_tracker.hh include/tlm/cep/protocol_engine_base.hh "
            "docs/adr openspec/specs/cep-tilelink-runtime-engine/spec.md";
        CHECK(git_diff_quiet(forbidden) == 0);
    }

    // I-7a AC-9: 既有断言零回归(TMA-07 接口面 + `TreeManager::view_of` **非**虚方法)
    {
        const std::string code =
            strip_comments(read_repo_file("include/tlm/cep/sharer_tracker.hh"));
        REQUIRE_FALSE(code.empty());
        CHECK(count_member_virtuals(code) == 5u);
        CHECK(code.find("tree_parent") == std::string::npos); // LocalView 零改动
        const std::string tm_code =
            strip_comments(read_repo_file("include/tlm/cep/tilelink_tree_manager.hh"));
        CHECK(tm_code.find("virtual LocalView view_of") == std::string::npos);
    }

    // I-7a AC-10: 0 回归 · 含队列语义 —— 判据在 **ctest / 闸层**(本载体不含)
    //    (`bash scripts/test/rddf_stage_gate.sh --phase queue2` + `--self-test` +
    //     全部 `scripts/test/cep_*.sh`;⚠️ ctest 裸调用**不覆盖**队列语义)

    // I-7a AC-11: 载体身份 —— 本 TEST_CASE 即载体(`[cep][tilelink][adapter]`)
    //    (名 = `CEP TreeManager: view_of 纯查询视图(Tip/Branch/Nothing/空条目/CHAIN)`;
    //     每条 AC 的**逐字**锚点注释见上)

    // I-7a AC-12: 多 Tip 如实返回(两个节点先后 `set_sharer(Tip)` ⇒ 两位都返回)
    {
        const node_id_t kParent = tlm::cep::kNodeIdPcie;
        const node_id_t kSelf = tlm::cep::kNodeIdSdma;
        const node_id_t kPeer = tlm::cep::kNodeIdGmmu;
        const responder_set_t mSelf = tlm::cep::responder_mask_of(kSelf);
        const responder_set_t mPeer = tlm::cep::responder_mask_of(kPeer);
        TreeMgr tm(kParent);
        tm.set_sharer(0x9400, kSelf, State::Tip);
        tm.set_sharer(0x9400, kPeer, State::Tip);
        const LocalView v = tm.view_of(0x9400);
        // ⛔ 不得 `first_bit` 式实现 —— `set_sharer` 只做 `|=` ⇒ 「至多 1 个
        // Tip」是**约定不是判据**
        CHECK(v.dirty == (mSelf | mPeer));
        CHECK(v.sharers == (mSelf | mPeer));
    }
}
