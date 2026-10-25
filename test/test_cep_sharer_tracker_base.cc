// test/test_cep_sharer_tracker_base.cc
// capability `cep-sharer-tracker-base` —— 9 Scenario / 9 锚点(
//   接口签名 / caps 形态 / 5×6 数据表 / FullMap 行为 / 生命周期 / 同址 hazard /
//   IC2 幂等 + IC3 五事件 / 禁 API 扫描 / 4 实现 deferred)
//
// 作者: CppTLM Team / 日期: 2026-10-25
//
// 纪律:
//   · 「判据实现退化」= 用例红;「判定结论为红」= 用例仍绿且结论登记(本文件无此类条款)
//   · 纯负向断言必须配正控(禁 API 扫描:必须能命中确实存在的标识符)
//   · 断言必须有判别力:不做「恒真的常量存在性」检查

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>

#include "tlm/cep/sharer_tracker.hh"

namespace {

    namespace cep = tlm::cep;

    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// 去掉 `//` 与 `/* */` 注释(禁 API 扫描的前提:注释里允许解释被禁的名字)
    std::string strip_comments(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        bool line_comment = false;
        bool block_comment = false;
        for (std::size_t i = 0; i < s.size(); ++i) {
            const char c = s[i];
            const char n = (i + 1 < s.size()) ? s[i + 1] : '\0';
            if (line_comment) {
                if (c == '\n') {
                    line_comment = false;
                    out.push_back(c);
                }
                continue;
            }
            if (block_comment) {
                if (c == '*' && n == '/') {
                    block_comment = false;
                    ++i;
                }
                continue;
            }
            if (c == '/' && n == '/') {
                line_comment = true;
                ++i;
                continue;
            }
            if (c == '/' && n == '*') {
                block_comment = true;
                ++i;
                continue;
            }
            out.push_back(c);
        }
        return out;
    }

    /// 统计「行首恰为 4 空格 + virtual 」的行数(接口面的虚方法计数)
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

    constexpr cep::responder_set_t mask(cep::node_id_t id) {
        return cep::responder_mask_of(id);
    }

    cep::SharerTrackerCaps caps_of(cep::TrackerImpl impl) {
        const auto* t = cep::find_tracker_traits(impl);
        return t ? t->caps : cep::SharerTrackerCaps{};
    }

    cep::SharerTrackerBehavior behavior_of(cep::TrackerImpl impl) {
        const auto* t = cep::find_tracker_traits(impl);
        return t ? t->behavior : cep::SharerTrackerBehavior{};
    }

    /// FullMap 行的期望值(ADR-CEP-04 §2 决策 3 表 + §4 表首行)
    cep::SharerTrackerCaps fullmap_caps_expected() {
        return cep::SharerTrackerCaps{.scope = cep::Scope::SYSTEM,
                                      .completeness = cep::Completeness::EXACT,
                                      .authority = cep::Authority::GLOBAL,
                                      .aggregation_at = cep::AggregationAt::HOME};
    }

} // namespace

// ═════════════════════════════════════════════════════════════════════
// Scenario 1:接口面恰好 4 个方法,命名与签名逐字对齐 ADR-CEP-04 §3
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP SharerTracker: 接口恰好 4 个方法且签名逐字对齐", "[cep][sharer-tracker]") {
    using LookupSig = void (cep::SharerTrackerIf::*)(std::uint64_t, cep::Intent, cep::LookupResult&,
                                                     cep::LocalView&, cep::LookupHazard&);
    using BeginSig =
        void (cep::SharerTrackerIf::*)(std::uint64_t, cep::responder_set_t, cep::snoop_handle_t&);
    using FoldSig =
        void (cep::SharerTrackerIf::*)(cep::snoop_handle_t, cep::node_id_t, cep::SnoopState, bool,
                                       bool, cep::responder_set_t&, cep::MergedState&);
    using ObserveSig =
        void (cep::SharerTrackerIf::*)(std::uint64_t, cep::node_id_t, cep::SnoopState);

    CHECK(std::is_same_v<decltype(&cep::SharerTrackerIf::lookup), LookupSig>);
    CHECK(std::is_same_v<decltype(&cep::SharerTrackerIf::snoop_begin), BeginSig>);
    CHECK(std::is_same_v<decltype(&cep::SharerTrackerIf::snoop_fold), FoldSig>);
    CHECK(std::is_same_v<decltype(&cep::SharerTrackerIf::observe_snoop), ObserveSig>);

    // 「恰好 4 个」的机检:虚成员计数 = 4 方法 + 1 虚析构
    const std::string code = strip_comments(read_repo_file("include/tlm/cep/sharer_tracker.hh"));
    REQUIRE(!code.empty());
    CHECK(count_member_virtuals(code) == 5);

    // intent 的 4 个规范值(替代协议 opcode)
    CHECK(static_cast<int>(cep::Intent::want_read) == 0);
    CHECK(static_cast<int>(cep::Intent::want_own) == 2);
    CHECK(static_cast<int>(cep::Intent::evict) == 3);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 2:caps descriptor 4 属性 + ResponderSet 64-bit + bit→node 显式映射
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP SharerTracker: caps 4 属性 / ResponderSet 64-bit / bit→node 映射",
          "[cep][sharer-tracker]") {
    // ① 4 个字段名由**指定初始化**机检(字段改名/删除 ⇒ 编译失败)
    const cep::SharerTrackerCaps caps{.scope = cep::Scope::SYSTEM,
                                      .completeness = cep::Completeness::EXACT,
                                      .authority = cep::Authority::GLOBAL,
                                      .aggregation_at = cep::AggregationAt::HOME};
    CHECK(caps == fullmap_caps_expected());
    // ② 4 × 1 字节(加第 5 个属性会改变尺寸)
    static_assert(sizeof(cep::SharerTrackerCaps) == 4, "caps 必须恰为 4 个 1-byte 枚举字段");

    // ③ ResponderSet 编译期锁 64-bit
    static_assert(sizeof(cep::responder_set_t) == 8, "ResponderSet 必须为 64-bit bitmask");

    // ④ bit→node 显式映射:bit 0 = SDMA;4 个已知节点构成双射
    CHECK(cep::responder_node_of(0) == cep::kNodeIdSdma);
    CHECK(cep::responder_bit_of(cep::kNodeIdSdma) == 0u);
    CHECK(mask(cep::kNodeIdSdma) == 1u);
    for (const cep::node_id_t id :
         {cep::kNodeIdSdma, cep::kNodeIdVram, cep::kNodeIdGmmu, cep::kNodeIdPcie}) {
        const unsigned b = cep::responder_bit_of(id);
        CHECK(b < cep::kResponderMappedBits);
        CHECK(cep::responder_node_of(b) == id); // 双向一致
    }
    // 未映射节点 ⇒ 位号为 kNoBit、掩码为 0(不得静默映射到某个位)
    CHECK(cep::responder_bit_of(cep::kInvalidNode) == cep::kNoBit);
    CHECK(mask(cep::kInvalidNode) == 0u);

    // 未映射节点写目录 = 显式 no-op(不得把它「就近」塞进某个已映射位)
    cep::FullMapDirectory dir;
    dir.set_sharer(0xA000, cep::kInvalidNode, cep::SnoopState::S);
    CHECK(dir.view_of(0xA000).sharers == 0u);
    CHECK(dir.view_of(0xA000).dirty == 0u);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 3:5 实现 × (4 caps + 2 behavior) 数据级自洽
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP SharerTracker: 5 实现 × caps/behavior 数据表自洽", "[cep][sharer-tracker]") {
    CHECK(cep::kTrackerTraits.size() == 5);

    // FullMap 行(ADR §2 决策 3 表 + §4 表首行)
    CHECK(caps_of(cep::TrackerImpl::FullMapDirectory) == fullmap_caps_expected());
    CHECK(behavior_of(cep::TrackerImpl::FullMapDirectory).hazard_under_contention ==
          cep::LookupHazard::STALL);
    CHECK(behavior_of(cep::TrackerImpl::FullMapDirectory).broadcast_fallback == false);

    // SparseSnoopFilter / DcohSymmetric 是 CONSERVATIVE(宁多探不漏探)
    CHECK(caps_of(cep::TrackerImpl::SparseSnoopFilter).completeness ==
          cep::Completeness::CONSERVATIVE);
    CHECK(caps_of(cep::TrackerImpl::DcohSymmetric).completeness == cep::Completeness::CONSERVATIVE);
    CHECK(caps_of(cep::TrackerImpl::DcohSymmetric).authority == cep::Authority::LOCAL_HINT);

    // DcohSymmetric 的 aggregation_at 是**二元**(P2P ⇒ REQUESTER / HDM home ⇒ HOME,per §3.5)
    const auto dcoh_agg = caps_of(cep::TrackerImpl::DcohSymmetric).aggregation_at;
    CHECK((dcoh_agg == cep::AggregationAt::REQUESTER || dcoh_agg == cep::AggregationAt::HOME));

    // TreeManager:向上递归 ⇒ CHAIN 而非 STALL(与 FullMap 的区别点)
    CHECK(behavior_of(cep::TrackerImpl::TreeManager).hazard_under_contention ==
          cep::LookupHazard::CHAIN);
    CHECK(caps_of(cep::TrackerImpl::TreeManager).aggregation_at == cep::AggregationAt::TREE_PARENT);

    // BroadcastNull:无状态 ⇒ 无 hazard
    CHECK(behavior_of(cep::TrackerImpl::BroadcastNull).hazard_under_contention ==
          cep::LookupHazard::NONE);

    // 数据级自洽:5 行 name 唯一;合法实现不声明 PARTIAL(决策 2:无兜底的 PARTIAL = 失效实例)
    std::set<std::string> names;
    for (const auto& t : cep::kTrackerTraits) {
        names.insert(t.name);
        CHECK(t.caps.completeness != cep::Completeness::PARTIAL);
        REQUIRE(cep::find_tracker_traits(t.impl) != nullptr);
        CHECK(cep::find_tracker_traits(t.impl)->caps == t.caps);
    }
    CHECK(names.size() == 5);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 4:FullMapDirectory 的 lookup 语义
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP SharerTracker: FullMapDirectory lookup 语义", "[cep][sharer-tracker]") {
    cep::FullMapDirectory dir;
    const std::uint64_t addr = 0x1000;
    dir.set_sharer(addr, cep::kNodeIdSdma, cep::SnoopState::S);
    dir.set_sharer(addr, cep::kNodeIdVram, cep::SnoopState::E);

    cep::LookupResult result = cep::LookupResult::BROADCAST;
    cep::LocalView view;
    cep::LookupHazard hazard = cep::LookupHazard::CHAIN;
    dir.lookup(addr, cep::Intent::want_write, result, view, hazard);

    CHECK(result == cep::LookupResult::ANSWER_HERE);
    CHECK(hazard == cep::LookupHazard::NONE);
    CHECK(view.valid);
    CHECK(view.sharers == (mask(cep::kNodeIdSdma) | mask(cep::kNodeIdVram)));
    CHECK(view.dirty == 0u);
    CHECK(view.forward_to == cep::kInvalidNode);

    // caps 与实现逐项对齐:EXACT / 无广播兜底 / 争用时 STALL
    CHECK(dir.caps.completeness == cep::Completeness::EXACT);
    CHECK(dir.caps == caps_of(cep::TrackerImpl::FullMapDirectory));
    CHECK(dir.behavior.broadcast_fallback == false);
    CHECK(dir.behavior.hazard_under_contention == cep::LookupHazard::STALL);

    // dirty 持有者 + want_read ⇒ FORWARD_TO(取数走 cache-to-cache)
    const std::uint64_t dirty_addr = 0x2000;
    dir.set_sharer(dirty_addr, cep::kNodeIdGmmu, cep::SnoopState::M);
    dir.lookup(dirty_addr, cep::Intent::want_read, result, view, hazard);
    CHECK(result == cep::LookupResult::FORWARD_TO);
    CHECK(view.forward_to == cep::kNodeIdGmmu);
    CHECK(view.dirty == mask(cep::kNodeIdGmmu));

    // 输入改变 ⇒ 输出改变(同地址,只换 intent)
    dir.lookup(dirty_addr, cep::Intent::want_write, result, view, hazard);
    CHECK(result == cep::LookupResult::ANSWER_HERE);
    CHECK(view.forward_to == cep::kInvalidNode);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 5:snoop begin → fold → RetireOnZeroPending(mask 单调递减)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP SharerTracker: snoop begin/fold 生命周期", "[cep][sharer-tracker]") {
    cep::FullMapDirectory dir;
    cep::responder_set_t remaining = 0;
    cep::MergedState merged = cep::SnoopState::I;

    // ── 序列 1:两个 responder ──
    cep::snoop_handle_t h = 0;
    dir.snoop_begin(0x3000, mask(cep::kNodeIdSdma) | mask(cep::kNodeIdVram), h);
    REQUIRE(h != 0);
    CHECK(dir.inflight_count() == 1);

    dir.snoop_fold(h, cep::kNodeIdSdma, cep::SnoopState::S, false, false, remaining, merged);
    CHECK(remaining == mask(cep::kNodeIdVram));
    CHECK(merged == cep::SnoopState::S);
    CHECK(dir.inflight_count() == 1);

    dir.snoop_fold(h, cep::kNodeIdVram, cep::SnoopState::M, true, true, remaining, merged);
    CHECK(remaining == 0u);
    CHECK(merged == cep::SnoopState::M);
    CHECK(dir.inflight_count() == 0); // remaining == 0 ⇔ RetireOnZeroPending
    CHECK(dir.event_count(cep::LifecycleEvent::RetireOnZeroPending) == 1);

    // ── 序列 2:单 responder(同一台机器的另一实例)──
    cep::FullMapDirectory dir2;
    cep::snoop_handle_t h2 = 0;
    dir2.snoop_begin(0x4000, mask(cep::kNodeIdGmmu), h2);
    dir2.snoop_fold(h2, cep::kNodeIdGmmu, cep::SnoopState::E, true, true, remaining, merged);
    CHECK(remaining == 0u);
    CHECK(merged == cep::SnoopState::M);

    // ── dirty 但未带数据:不升 M(数据须走 forward_data 槽,per ArchForge VC-R2)──
    cep::FullMapDirectory dir3;
    cep::snoop_handle_t h3 = 0;
    dir3.snoop_begin(0x5000, mask(cep::kNodeIdSdma), h3);
    dir3.snoop_fold(h3, cep::kNodeIdSdma, cep::SnoopState::S, true, false, remaining, merged);
    CHECK(merged == cep::SnoopState::S);

    // ── 未知 handle:显式哨兵(全 1 + I),不是静默成功 ──
    dir3.snoop_fold(0xBEEFu, cep::kNodeIdSdma, cep::SnoopState::M, true, true, remaining, merged);
    CHECK(remaining == ~cep::responder_set_t{0});
    CHECK(merged == cep::SnoopState::I);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 6:同地址并发 lookup ⇒ hazard 按 caps 表返回 STALL
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP SharerTracker: 同地址并发 lookup 返回 STALL", "[cep][sharer-tracker]") {
    cep::FullMapDirectory dir;
    const std::uint64_t addr = 0x6000;
    dir.set_sharer(addr, cep::kNodeIdSdma, cep::SnoopState::S);

    cep::LookupResult r1 = cep::LookupResult::BROADCAST;
    cep::LocalView v1;
    cep::LookupHazard hz1 = cep::LookupHazard::CHAIN;
    dir.lookup(addr, cep::Intent::want_read, r1, v1, hz1);
    CHECK(hz1 == cep::LookupHazard::NONE);
    CHECK(v1.valid);

    // snoop 在飞 ⇒ 第二次同地址 lookup 必须 STALL(不是 NONE,也不是 CHAIN)
    cep::snoop_handle_t h = 0;
    dir.snoop_begin(addr, mask(cep::kNodeIdSdma), h);
    cep::LookupResult r2 = cep::LookupResult::BROADCAST;
    cep::LocalView v2;
    cep::LookupHazard hz2 = cep::LookupHazard::NONE;
    dir.lookup(addr, cep::Intent::want_read, r2, v2, hz2);
    CHECK(hz2 == cep::LookupHazard::STALL);
    CHECK(hz2 != cep::LookupHazard::CHAIN);
    CHECK(v2.valid == false); // hazard != NONE ⇒ view 无意义

    // 与 caps 表一致
    CHECK(dir.behavior.hazard_under_contention == hz2);

    // snoop 完成后 hazard 回到 NONE(输入改变 ⇒ 输出改变)
    cep::responder_set_t remaining = 0;
    cep::MergedState merged = cep::SnoopState::I;
    dir.snoop_fold(h, cep::kNodeIdSdma, cep::SnoopState::I, false, false, remaining, merged);
    REQUIRE(remaining == 0u);
    dir.lookup(addr, cep::Intent::want_read, r2, v2, hz2);
    CHECK(hz2 == cep::LookupHazard::NONE);
    CHECK(v2.valid);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 7:INV-CEP-04-IC2 幂等 + IC3 五事件名与顺序
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP SharerTracker: IC2 幂等 + IC3 生命周期事件", "[cep][sharer-tracker]") {
    cep::FullMapDirectory dir;
    const std::uint64_t addr = 0x7000;
    dir.set_sharer(addr, cep::kNodeIdSdma, cep::SnoopState::S);

    // ── IC2:lookup 同参同返 ──
    cep::LookupResult ra = cep::LookupResult::BROADCAST, rb = cep::LookupResult::BROADCAST;
    cep::LocalView va, vb;
    cep::LookupHazard ha = cep::LookupHazard::CHAIN, hb = cep::LookupHazard::CHAIN;
    dir.lookup(addr, cep::Intent::want_read, ra, va, ha);
    dir.lookup(addr, cep::Intent::want_read, rb, vb, hb);
    CHECK(ra == rb);
    CHECK(ha == hb);
    CHECK(va == vb);

    // ── IC2:snoop_begin 重复调用返回同一 handle ──
    //    期望集取两个 responder ⇒ 首次 fold 不会 retire(否则第二次 fold 打的是失效 handle,
    //    那属于**协议错误**而非幂等场景)
    cep::snoop_handle_t h1 = 0, h2 = 0;
    const cep::responder_set_t exp = mask(cep::kNodeIdSdma) | mask(cep::kNodeIdVram);
    dir.snoop_begin(addr, exp, h1);
    dir.snoop_begin(addr, exp, h2);
    CHECK(h1 == h2);
    CHECK(dir.inflight_count() == 1);

    // 期望集**不同**的重复 begin ⇒ 拒绝(handle = 0),不得新建第二条 pending
    cep::snoop_handle_t h3 = 0xFFFF;
    dir.snoop_begin(addr, mask(cep::kNodeIdGmmu), h3);
    CHECK(h3 == 0);
    CHECK(dir.inflight_count() == 1); // 无泄漏

    // ── IC2:snoop_fold 重复 fold 不重复扣减 ──
    cep::responder_set_t rem_a = 0, rem_b = 0;
    cep::MergedState m_a = cep::SnoopState::I, m_b = cep::SnoopState::I;
    dir.snoop_fold(h1, cep::kNodeIdSdma, cep::SnoopState::S, false, false, rem_a, m_a);
    const auto decrements_before = dir.event_count(cep::LifecycleEvent::DecrementOnSnoopFold);
    dir.snoop_fold(h1, cep::kNodeIdSdma, cep::SnoopState::S, false, false, rem_b, m_b);
    CHECK(rem_a == rem_b);
    CHECK(m_a == m_b);
    CHECK(rem_a == mask(cep::kNodeIdVram)); // 只扣了一次
    CHECK(dir.event_count(cep::LifecycleEvent::DecrementOnSnoopFold) == decrements_before);
    CHECK(dir.inflight_count() == 1); // 仍在飞(VRAM 未应答)

    // ── IC2:observe_snoop 幂等(同 from_node 连续两次 ⇒ 同一投影)──
    cep::FullMapDirectory obs;
    obs.observe_snoop(0x8000, cep::kNodeIdVram, cep::SnoopState::M);
    const cep::LocalView before = obs.peer_view_of(0x8000);
    obs.observe_snoop(0x8000, cep::kNodeIdVram, cep::SnoopState::M);
    CHECK(before == obs.peer_view_of(0x8000));
    CHECK(before.sharers == mask(cep::kNodeIdVram));
    CHECK(before.dirty == mask(cep::kNodeIdVram));

    // ── IC3:5 个事件名逐字一致 + 时序(allocate 在 merge 之前,decrement 在 retire 之前)──
    REQUIRE(cep::kLifecycleEventNames.size() == 5);
    CHECK(std::string(cep::lifecycle_event_name(cep::LifecycleEvent::AllocateOnFirstForward)) ==
          "allocate-on-first-forward");
    CHECK(std::string(cep::lifecycle_event_name(cep::LifecycleEvent::MergeOnSnoopBegin)) ==
          "merge-on-snoop-begin");
    CHECK(std::string(cep::lifecycle_event_name(cep::LifecycleEvent::DecrementOnSnoopFold)) ==
          "decrement-on-snoop-fold");
    CHECK(std::string(cep::lifecycle_event_name(cep::LifecycleEvent::RetireOnZeroSharers)) ==
          "retire-on-zero-sharers");
    CHECK(std::string(cep::lifecycle_event_name(cep::LifecycleEvent::RetireOnZeroPending)) ==
          "retire-on-zero-pending");

    // 时序:在同一实例上按脚本跑一遍,事件序列必须按契约序出现
    cep::FullMapDirectory seq;
    cep::LookupResult r = cep::LookupResult::BROADCAST;
    cep::LocalView v;
    cep::LookupHazard hz = cep::LookupHazard::CHAIN;
    seq.lookup(0x9000, cep::Intent::want_read, r, v, hz); // allocate
    cep::snoop_handle_t hs = 0;
    seq.snoop_begin(0x9000, mask(cep::kNodeIdSdma), hs); // merge
    cep::responder_set_t rem = 0;
    cep::MergedState mm = cep::SnoopState::I;
    seq.snoop_fold(hs, cep::kNodeIdSdma, cep::SnoopState::I, false, false, rem,
                   mm); // decrement + retire
    REQUIRE(rem == 0u);

    const auto& ev = seq.events();
    REQUIRE(ev.size() >= 4);
    CHECK(ev[0] == cep::LifecycleEvent::AllocateOnFirstForward);
    CHECK(ev[1] == cep::LifecycleEvent::MergeOnSnoopBegin);
    CHECK(ev[2] == cep::LifecycleEvent::DecrementOnSnoopFold);
    CHECK(ev.back() == cep::LifecycleEvent::RetireOnZeroPending);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 8:禁用 API 去注释静态扫描(+ 正控)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP SharerTracker: 禁用 API 零出现(去注释 + 正控)", "[cep][sharer-tracker]") {
    const std::string code = strip_comments(read_repo_file("include/tlm/cep/sharer_tracker.hh"));
    REQUIRE(!code.empty());

    CHECK(code.find("snoop_complete") == std::string::npos);
    CHECK(code.find("subscribe_remote_snoop") == std::string::npos);
    CHECK(code.find("is_distributed") == std::string::npos);
    CHECK(code.find("sharer_bitmap") == std::string::npos);

    // 正控 1:同一机制必须能命中确实存在的标识符(否则「零命中」可能只是扫了个空串)
    CHECK(code.find("observe_snoop") != std::string::npos);
    CHECK(code.find("snoop_begin") != std::string::npos);

    // 正控 2:注释被剥掉,但**代码里**的同名 token 必须被抓到
    const std::string probe =
        strip_comments("// snoop_complete 在注释里\nvoid snoop_complete();\n");
    CHECK(probe.find("snoop_complete") != std::string::npos);
    CHECK(strip_comments("// snoop_complete only in comment\n").find("snoop_complete") ==
          std::string::npos);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 9:其余 4 实现只做声明层断言(deferred)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP SharerTracker: 非 FullMap 的 4 实现只做声明层断言", "[cep][sharer-tracker]") {
    const std::string code = strip_comments(read_repo_file("include/tlm/cep/sharer_tracker.hh"));
    REQUIRE(!code.empty());

    // 声明层:4 个名字都在表里
    for (const char* name :
         {"SparseSnoopFilter", "DcohSymmetric", "TreeManager", "BroadcastNull"}) {
        CHECK(code.find(std::string("\"") + name + "\"") != std::string::npos);
        // 行为层:这些实现的行为断言 deferred ⇒ 头文件里不得有它们的类定义
        CHECK(code.find(std::string("class ") + name) == std::string::npos);
        CHECK(code.find(std::string("struct ") + name) == std::string::npos);
    }

    // deferred 的登记必须可在 spec 里查到(不是只在本文件的注释里)
    const std::string spec = read_repo_file(
        "openspec/changes/cpptlm-cep-directory-ordering-plane/specs/cep-sharer-tracker-base/"
        "spec.md");
    REQUIRE(!spec.empty());
    CHECK(spec.find("只做声明层断言") != std::string::npos);
    CHECK(spec.find("deferred") != std::string::npos);
}
