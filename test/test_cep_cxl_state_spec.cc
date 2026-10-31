// =============================================================================
// test/test_cep_cxl_state_spec.cc
//   —— CXL 2.0 非对称规约载体(Phase 2a 结构层)的契约测试 + 反向 fixture
//
// 作者: CppTLM Team / 日期: 2026-11-01
// 配套: include/tlm/cep/cxl_state_spec.hh · docs/architecture/14-coherence-parity-baseline.md(无关)
//       openspec/changes/cpptlm-cep-cxl-nonsymmetric-state-spec/(delta spec AC-1 / AC-2 / AC-4 /
//       AC-5) 台账 obj-cep-platform.md Entry #122(口径裁决)· #123(change 转正)
//
// ── 判据纪律(per 本仓 4 条铁律)────────────────────────────────────────────
// ① 纯负向断言 SHALL 有正控 ⇒ 每条负向断言都有**正向**对照;
// ② 每条判据 SHALL 有**可执行反向 fixture** 且**因正确原因红**;
// ③ 编译期锁 SHALL **不**只以「编译红」充当 ctest 判据 ⇒ 结构校验提供
//    **运行时可判**入口(`structure_is_valid_for` 接受任意集合);
// ④ fixture 状态变迁后 SHALL 重新基线化 —— 本文件为**新增**,基线 = 全绿。
// =============================================================================

#include "tlm/cep/cxl_state_spec.hh"

#include "catch_amalgamated.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace {

    using cpptlm::cep::CxlStateSpec;

    using State = CxlStateSpec::State;
    using Event = CxlStateSpec::Event;
    using Perm = CxlStateSpec::Perm;
    using BasisKind = CxlStateSpec::BasisKind;

    constexpr std::array<State, 4> kStates{State::I, State::S, State::E, State::M};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// 正控:基线必须全绿
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP CXL state spec — 态集合恰 4 态(无 F)", "[cep][cxl][state-spec]") {
    STATIC_REQUIRE(CxlStateSpec::kStateCount == 4);
    STATIC_REQUIRE(CxlStateSpec::kPermCount == 4);

    // ⚠️ 「无 `F` 态」的运行时判据:枚举值域恰为 0..3 ⇒ 取不到第 5 个态。
    //    (MESIF 载体的 `State::F` == 4 ⇒ 本载体**形态不同构**)
    STATIC_REQUIRE(static_cast<std::uint8_t>(State::M) == 3);
    STATIC_REQUIRE(std::string(CxlStateSpec::state_name(State::I)) == "I");
    STATIC_REQUIRE(std::string(CxlStateSpec::state_name(State::S)) == "S");
    STATIC_REQUIRE(std::string(CxlStateSpec::state_name(State::E)) == "E");
    STATIC_REQUIRE(std::string(CxlStateSpec::state_name(State::M)) == "M");
}

TEST_CASE("CEP CXL state spec — perm 投影恰 4 行且逐格正确", "[cep][cxl][state-spec]") {
    STATIC_REQUIRE(CxlStateSpec::kPermProjection.size() == 4);
    STATIC_REQUIRE(CxlStateSpec::kPermProjection[0] == Perm::NONE);
    STATIC_REQUIRE(CxlStateSpec::kPermProjection[1] == Perm::READ);
    STATIC_REQUIRE(CxlStateSpec::kPermProjection[2] == Perm::UNIQUE);
    STATIC_REQUIRE(CxlStateSpec::kPermProjection[3] == Perm::WRITEBACK);
    // ⚠️ **无 `F→WRITEBACK` 行**(MESIF 的第 5 行在 CXL 无对应物)
    STATIC_REQUIRE(CxlStateSpec::kPermProjection.size() != 5);
}

TEST_CASE("CEP CXL state spec — 秩只增不减,无例外", "[cep][cxl][state-spec]") {
    STATIC_REQUIRE(CxlStateSpec::perm_rank(Perm::NONE) == 0);
    STATIC_REQUIRE(CxlStateSpec::perm_rank(Perm::READ) == 1);
    STATIC_REQUIRE(CxlStateSpec::perm_rank(Perm::UNIQUE) == 2);
    STATIC_REQUIRE(CxlStateSpec::perm_rank(Perm::WRITEBACK) == 3);
    // ⚠️ 唯一静默升级 = `E + 写事件 → M`(MESIF 的 `F→E` 秩回落例外**不可移植**)
    STATIC_REQUIRE(CxlStateSpec::is_silent_upgrade(State::E, Event::ItoMWr));
    STATIC_REQUIRE_FALSE(CxlStateSpec::is_silent_upgrade(State::M, Event::ItoMWr));
    STATIC_REQUIRE_FALSE(CxlStateSpec::is_silent_upgrade(State::E, Event::RdShared));
}

TEST_CASE("CEP CXL state spec — 非对称结构:local / remote 划分", "[cep][cxl][state-spec]") {
    // remote = **仅** H2D snoop(发起方恒为 Home)
    STATIC_REQUIRE(CxlStateSpec::kRemoteEvents.size() == 3);
    STATIC_REQUIRE(CxlStateSpec::is_remote_event(Event::SnpInv));
    STATIC_REQUIRE(CxlStateSpec::is_remote_event(Event::SnpData));
    STATIC_REQUIRE(CxlStateSpec::is_remote_event(Event::SnpCurr));
    // ⚠️ **设备侧无 snoop 发起型事件** ⇒ 其余全部是 local
    STATIC_REQUIRE(CxlStateSpec::is_local_event(Event::RdOwn));
    STATIC_REQUIRE(CxlStateSpec::is_local_event(Event::WrInv));
    STATIC_REQUIRE(CxlStateSpec::is_local_event(Event::DirtyEvict));
    STATIC_REQUIRE_FALSE(CxlStateSpec::is_local_event(Event::SnpInv));
}

TEST_CASE("CEP CXL state spec — basis kind / pending 双向一致", "[cep][cxl][state-spec]") {
    // 强度不得升格:analogical / self-derived ⇒ 必须 pending
    STATIC_REQUIRE(CxlStateSpec::basis_pending_consistent(BasisKind::PublicAnalogical, true));
    STATIC_REQUIRE_FALSE(
        CxlStateSpec::basis_pending_consistent(BasisKind::PublicAnalogical, false));
    STATIC_REQUIRE(CxlStateSpec::basis_pending_consistent(BasisKind::SelfDerived, true));
    STATIC_REQUIRE_FALSE(CxlStateSpec::basis_pending_consistent(BasisKind::SelfDerived, false));
    // direct / archforge-anchor:复核通过即 false
    STATIC_REQUIRE(CxlStateSpec::basis_pending_consistent(BasisKind::PublicDirect, false));
    STATIC_REQUIRE_FALSE(CxlStateSpec::basis_pending_consistent(BasisKind::PublicDirect, true));
    STATIC_REQUIRE(CxlStateSpec::basis_pending_consistent(BasisKind::ArchforgeAnchor, false));
}

TEST_CASE("CEP CXL state spec — 头注释消歧声明可机器校", "[cep][cxl][state-spec]") {
    STATIC_REQUIRE(CxlStateSpec::has_disambiguation(CxlStateSpec::kDisambiguation));
    // ⚠️ 反向:不含「dGPU SoC」⇒ 红
    STATIC_REQUIRE_FALSE(CxlStateSpec::has_disambiguation("CXL 实例 = CEP 探索实例"));
    // ⚠️ 反向:不含「!=」⇒ 红
    STATIC_REQUIRE_FALSE(CxlStateSpec::has_disambiguation("dGPU SoC 实现路径"));
}

TEST_CASE("CEP CXL state spec — 残余风险登记 8 项", "[cep][cxl][state-spec]") {
    STATIC_REQUIRE(CxlStateSpec::kResidualRiskCount == 8);
    STATIC_REQUIRE(CxlStateSpec::kResidualRisks.size() == 8);
    STATIC_REQUIRE(CxlStateSpec::kTransientStateCount == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// 反向 fixture:每条 SHALL **因正确原因**红(判据铁律 ②)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP CXL state spec — 反向 fixture:结构校验对错误集合必红",
          "[cep][cxl][state-spec][reverse]") {
    // ① 态数 **≠ 4** ⇒ 红(正控:恰 4 ⇒ 绿)
    {
        constexpr std::array<State, 3> three{State::I, State::S, State::E};
        constexpr std::array<Perm, 3> proj3{Perm::NONE, Perm::READ, Perm::UNIQUE};
        STATIC_REQUIRE_FALSE(CxlStateSpec::structure_is_valid_for(
            three, CxlStateSpec::kRemoteEvents, CxlStateSpec::kLocalEvents, proj3));
    }
    // ② 投影逐格错(第 4 行给了 `UNIQUE` 而非 `WRITEBACK`)⇒ 红
    {
        constexpr std::array<Perm, 4> bad_proj{Perm::NONE, Perm::READ, Perm::UNIQUE, Perm::UNIQUE};
        STATIC_REQUIRE_FALSE(CxlStateSpec::structure_is_valid_for(
            kStates, CxlStateSpec::kRemoteEvents, CxlStateSpec::kLocalEvents, bad_proj));
    }
    // ③ remote 集里混入一个**非** snoop 事件 ⇒ 红(remote SHALL 全是 snoop)
    {
        constexpr std::array<Event, 3> bad_remote{Event::SnpInv, Event::SnpData, Event::RdOwn};
        STATIC_REQUIRE_FALSE(CxlStateSpec::structure_is_valid_for(
            kStates, bad_remote, CxlStateSpec::kLocalEvents, CxlStateSpec::kPermProjection));
    }
    // ④ local 集里混入一个 snoop ⇒ 红(local ∩ remote = ∅ 被破)
    {
        constexpr std::array<Event, 4> bad_local{Event::RdOwn, Event::WrInv, Event::SnpInv,
                                                 Event::DirtyEvict};
        STATIC_REQUIRE_FALSE(CxlStateSpec::structure_is_valid_for(
            kStates, CxlStateSpec::kRemoteEvents, bad_local, CxlStateSpec::kPermProjection));
    }
    // ⑤ remote 集有重复 ⇒ 红
    {
        constexpr std::array<Event, 3> dup{Event::SnpInv, Event::SnpInv, Event::SnpCurr};
        STATIC_REQUIRE_FALSE(CxlStateSpec::structure_is_valid_for(
            kStates, dup, CxlStateSpec::kLocalEvents, CxlStateSpec::kPermProjection));
    }
    // 正控:全部正确 ⇒ 绿(否则上面的负向断言就没有鉴别力)
    STATIC_REQUIRE(CxlStateSpec::structure_is_valid_for(kStates, CxlStateSpec::kRemoteEvents,
                                                        CxlStateSpec::kLocalEvents,
                                                        CxlStateSpec::kPermProjection));
    STATIC_REQUIRE(CxlStateSpec::structure_is_valid());
}
