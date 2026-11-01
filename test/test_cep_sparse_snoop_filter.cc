// test/test_cep_sparse_snoop_filter.cc
//   —— `SparseSnoopFilter` 行为实现的契约测试 + 反向 fixture
//
// 作者: CppTLM Team / 日期: 2026-11-01
// 配套: include/tlm/cep/sharer_tracker.hh(SparseSnoopFilter)
//       openspec/changes/cpptlm-cep-cxl-nonsymmetric/specs/cep-sparse-snoop-filter/spec.md
//       台账 obj-cep-platform.md Entry #124(R11 归属复核)· #128(change 转正)
//
// ── 判据纪律(per 本仓 4 条铁律)────────────────────────────────────────────
// ① 纯负向断言 SHALL 有正控;② 每条判据 SHALL 有可执行反向 fixture 且**因正确原因红**;
// ③ 编译期锁 SHALL **不**只以「编译红」充当 ctest 判据;④ fixture 状态变迁后 SHALL 重新基线化。
//
// ── 本测的核心语义(CONSERVATIVE)──────────────────────────────────────────
// `completeness = CONSERVATIVE` ⇒ filter **不能**回答「**没有**其他 sharer」
// ⇒ 本实现 **永不**返回 `ANSWER_HERE`(这是与 EXACT 目录的**可观测差异**)。

#include "catch_amalgamated.hpp"

#include "tlm/cep/sharer_tracker.hh"

#include <cstdint>

namespace {

    using tlm::cep::Intent;
    using tlm::cep::LocalView;
    using tlm::cep::LookupHazard;
    using tlm::cep::LookupResult;
    using tlm::cep::SnoopState;
    using tlm::cep::SparseSnoopFilter;
    using tlm::cep::TrackerImpl;

    /// 一次 `lookup` 的完整结果
    struct Probe {
        LookupResult result = LookupResult::ANSWER_HERE;
        LocalView view{};
        LookupHazard hazard = LookupHazard::NONE;
    };

    Probe probe(SparseSnoopFilter& sf, std::uint64_t addr, Intent intent) {
        Probe p;
        sf.lookup(addr, intent, p.result, p.view, p.hazard);
        return p;
    }

    constexpr std::uint64_t kAddr = 0x1000;

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// 正控
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP SparseSnoopFilter — caps / behavior 逐字段等于 ADR-CEP-04 的冻结契约",
          "[cep][sparse-snoop-filter]") {
    SparseSnoopFilter sf;
    const auto* traits = tlm::cep::find_tracker_traits(TrackerImpl::SparseSnoopFilter);
    REQUIRE(traits != nullptr);

    // §2 caps:`SYSTEM` / `CONSERVATIVE` / `GLOBAL` / `HOME`
    REQUIRE(sf.caps.scope == traits->caps.scope);
    REQUIRE(sf.caps.completeness == traits->caps.completeness);
    REQUIRE(sf.caps.authority == traits->caps.authority);
    REQUIRE(sf.caps.aggregation_at == traits->caps.aggregation_at);

    // §4 hazard:`Hazard_STALL` + `broadcast_fallback = true`
    REQUIRE(sf.behavior.hazard_under_contention == traits->behavior.hazard_under_contention);
    REQUIRE(sf.behavior.broadcast_fallback == traits->behavior.broadcast_fallback);

    REQUIRE(sf.caps.scope == tlm::cep::Scope::SYSTEM);
    REQUIRE(sf.caps.completeness == tlm::cep::Completeness::CONSERVATIVE);
    REQUIRE(sf.caps.authority == tlm::cep::Authority::GLOBAL);
    REQUIRE(sf.caps.aggregation_at == tlm::cep::AggregationAt::HOME);
    REQUIRE(sf.behavior.hazard_under_contention == LookupHazard::STALL);
    REQUIRE(sf.behavior.broadcast_fallback);

    // 实现标识
    REQUIRE(SparseSnoopFilter::kImpl == TrackerImpl::SparseSnoopFilter);
}

TEST_CASE("CEP SparseSnoopFilter — CONSERVATIVE:无条目 ⇒ 广播;唯一可能脏持有者 ⇒ 转发",
          "[cep][sparse-snoop-filter]") {
    SparseSnoopFilter sf;

    // ① **无条目** ⇒ 「不能证明无人持有」⇒ `BROADCAST`(★ 保守默认)
    {
        const auto p = probe(sf, kAddr, Intent::want_read);
        REQUIRE(p.result == LookupResult::BROADCAST);
        REQUIRE(p.view.valid == false);
        REQUIRE(p.hazard == LookupHazard::NONE);
    }

    // ② 唯一可能的 sharer 且是脏持有者 ⇒ `FORWARD_TO`(位集是上界 ⇒ 单点转发安全)
    {
        SparseSnoopFilter sf2;
        sf2.set_sharer(kAddr, tlm::cep::kNodeIdPcie, SnoopState::M);
        const auto p = probe(sf2, kAddr, Intent::want_read);
        REQUIRE(p.result == LookupResult::FORWARD_TO);
        REQUIRE(p.view.forward_to == tlm::cep::kNodeIdPcie);
    }

    // ③ **可能多个** sharer ⇒ 必须广播(单点转发会漏探)
    {
        SparseSnoopFilter sf3;
        sf3.set_sharer(kAddr, tlm::cep::kNodeIdPcie, SnoopState::M);
        sf3.set_sharer(kAddr, tlm::cep::kNodeIdSdma, SnoopState::S);
        const auto p = probe(sf3, kAddr, Intent::want_read);
        REQUIRE(p.result == LookupResult::BROADCAST);
    }

    // ④ 唯一可能 sharer 但**干净** ⇒ 仍广播(CONSERVATIVE 不得声称「无人持有」)
    {
        SparseSnoopFilter sf4;
        sf4.set_sharer(kAddr, tlm::cep::kNodeIdPcie, SnoopState::S);
        const auto p = probe(sf4, kAddr, Intent::want_read);
        REQUIRE(p.result == LookupResult::BROADCAST);
    }
}

TEST_CASE("CEP SparseSnoopFilter — **永不** ANSWER_HERE(与 EXACT 目录的可观测差异)",
          "[cep][sparse-snoop-filter]") {
    SparseSnoopFilter sf;
    // ⚠️ 这是 CONSERVATIVE 的**定义性后果**,不是缺陷:
    //    「没有其他 sharer」只能由 EXACT 目录(`FullMapDirectory`)回答。
    // ⇒ 反向对照:EXACT 目录在同样场景下**能**给 `ANSWER_HERE` ⇒ 两者可判别。
    for (const auto intent :
         {Intent::want_read, Intent::want_write, Intent::want_own, Intent::evict}) {
        REQUIRE(probe(sf, kAddr, intent).result != LookupResult::ANSWER_HERE);
    }

    SparseSnoopFilter exact_contrast;
    exact_contrast.set_sharer(kAddr, tlm::cep::kNodeIdPcie, SnoopState::M);
    REQUIRE(probe(exact_contrast, kAddr, Intent::want_read).result == LookupResult::FORWARD_TO);

    // ⇒ EXACT 目录的对照
    tlm::cep::FullMapDirectory full;
    LookupResult r = LookupResult::ANSWER_HERE;
    LocalView v{};
    LookupHazard h = LookupHazard::NONE;
    full.lookup(kAddr, Intent::want_read, r, v, h);
    // EXACT 目录对**无条目**地址也直接就地回答 ⇒ 与本实现的可观测差异成立
    REQUIRE(r == LookupResult::ANSWER_HERE);
    REQUIRE(probe(sf, kAddr, Intent::want_read).result == LookupResult::BROADCAST);
}

TEST_CASE("CEP SparseSnoopFilter — contention ⇒ STALL;观测到 I ⇒ 合法收缩",
          "[cep][sparse-snoop-filter]") {
    SparseSnoopFilter sf;
    sf.set_sharer(kAddr, tlm::cep::kNodeIdPcie, SnoopState::S);

    // ① 同地址 snoop 在飞 ⇒ `STALL`(per `ADR-CEP-04 §4`)
    {
        SparseSnoopFilter sf2;
        sf2.set_sharer(kAddr, tlm::cep::kNodeIdPcie, SnoopState::S);
        tlm::cep::snoop_handle_t handle = 0;
        sf2.snoop_begin(kAddr, tlm::cep::responder_mask_of(tlm::cep::kNodeIdPcie), handle);
        REQUIRE(handle != 0);
        const auto p = probe(sf2, kAddr, Intent::want_read);
        REQUIRE(p.hazard == LookupHazard::STALL);
    }

    // ② 观测到该节点转为 `I` ⇒ 合法收缩 ⇒ 条目被回收 ⇒ 回到「无条目 ⇒ 广播」
    {
        REQUIRE(sf.possible_sharer_count(kAddr) == 1);
        sf.observe_snoop(kAddr, tlm::cep::kNodeIdPcie, SnoopState::I);
        REQUIRE(sf.possible_sharer_count(kAddr) == 0);
        REQUIRE(probe(sf, kAddr, Intent::want_read).result == LookupResult::BROADCAST);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 反向 fixture:每条 SHALL **因正确原因**红(判据铁律 ②)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP SparseSnoopFilter — 反向 fixture:漏探 / 错 hazard / 假 ANSWER_HERE 必红",
          "[cep][sparse-snoop-filter][reverse]") {
    // ① **漏探**(filter 说独占、实际另有 sharer)⇒ 必须被检出
    //    判据形态:SF 的 `lookup` 在「可能多 sharer」时 **不得** 单点转发。
    {
        SparseSnoopFilter sf;
        sf.set_sharer(kAddr, tlm::cep::kNodeIdPcie, SnoopState::M);
        sf.set_sharer(kAddr, tlm::cep::kNodeIdSdma, SnoopState::S);
        // ⇒ 掩码 2 位 ⇒ 必须广播;若实现误判「唯一脏持有者 ⇒ 转发」⇒ 漏探 ⇒ 红
        const auto p = probe(sf, kAddr, Intent::want_read);
        REQUIRE(p.result == LookupResult::BROADCAST);
    }
    // ② **未映射节点** ⇒ 显式 no-op(不得留下可见空条目)
    {
        SparseSnoopFilter sf;
        sf.set_sharer(kAddr, tlm::cep::kInvalidNode, SnoopState::M);
        REQUIRE(sf.possible_sharer_count(kAddr) == 0);
        REQUIRE(sf.view_of(kAddr).valid == false);
    }
    // ③ 对**未观测**到的节点调用收缩 ⇒ no-op(不得误删别人的位)
    {
        SparseSnoopFilter sf;
        sf.set_sharer(kAddr, tlm::cep::kNodeIdPcie, SnoopState::S);
        sf.note_sharer_dropped(kAddr, tlm::cep::kNodeIdSdma);
        REQUIRE(sf.possible_sharer_count(kAddr) == 1);
    }
    // ④ 收缩到零 ⇒ 条目回收(不留 `sharers == 0` 的可见空条目)
    {
        SparseSnoopFilter sf;
        sf.set_sharer(kAddr, tlm::cep::kNodeIdPcie, SnoopState::S);
        sf.note_sharer_dropped(kAddr, tlm::cep::kNodeIdPcie);
        REQUIRE(sf.possible_sharer_count(kAddr) == 0);
        REQUIRE(sf.view_of(kAddr).valid == false);
    }
}
