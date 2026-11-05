// test/test_cep_canonical_flow_contract.cc
// canonical 10 流的**契约级**不变量 —— CEN-Ib3 / CEN-Ib4 / CEN-Ib9 / CEN-Ib10
//
// 依据:`ADR-CEP-02 §2` 的 canonical 10 流表 + 其「不变量集合」(CEN-Ib1 ~ CEN-Ib11)。
// 逐条来源(逐字):
//   · CEN-Ib3 : `snoop` 必有 `snoop_resp`
//   · CEN-Ib4 : `snoop_resp` 必须含 `txn_id` 用于 ack 配对
//   · CEN-Ib9 : `barrier` 流**不**携带 `group_id` / `participant_count` / `epoch_id`
//   · CEN-Ib10: `snoop_completion_ack` 配对 `snoop_resp`,**不**配对 `completion`
//
// ⚠️ **为什么用「签名 trait」而不是运行期行为**:这四条的不变量主体是
//   **协议引擎的接口形状**(哪些回调存在 / 带哪些字段)⇒ 契约层断言是**正确层级**
//   (与 `ADR-CEP-05 §2` 的折叠映射表同族)。⛔ 纯编译期 `static_assert` 不足:
//   「删掉锁 ⇒ 用例假绿」是本仓已知失败模式(per `test_cep_cohif_carrier.cc` 文件头 ③)
//   ⇒ 每条都配**反向 fixture**(故意坏类型 ⇒ trait 必须 false)证明 trait **有判别力**。
//
// ⚠️ **实现坑(踩过)**:Catch2 的 `REQUIRE` 宏展开为 `Decomposer() <= __VA_ARGS__`,
//   于是 `HasTrait<Engine>::value` 里的 `<` 被解析成**比较运算符** ⇒
//   「expected a type, got 'ProtocolEngineBase'」。⇒ 模板实参一律**先绑到 constexpr 变量**再断言。
//
// ⚠️ **口径**:本文件断言的是**接口契约**,⛔ **不是**协议实现的运行期行为证明。
//   「某协议引擎是否真的遵守」属该协议的 E2E 判据面(per 各协议 change)。
//
// 作者: CppTLM Team / 日期: 2026-11-05(per 台账 Entry #232 / B2 映射审计)

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <type_traits>

#include "tlm/cep/protocol_engine_base.hh"

namespace {

    using tlm::cep::LineState;
    using tlm::cep::Perm;
    /// `ProtocolEngineBase` 是**类模板**(N = 端口数)⇒ 取 N=2 形态(与各协议引擎一致)
    using Engine2 = tlm::cep::ProtocolEngineBase<2>;
    using tlm::cep::SnoopType;

    // ── CEN-Ib3:`snoop` 与 `snoop_resp` 必须**同时**存在 ────────────────
    template <class E, class = void>
    struct HasSnoopPair : std::false_type {};
    template <class E>
    struct HasSnoopPair<E, std::void_t<decltype(&E::on_snoop),
                                       decltype(&E::on_snoop_resp)>> : std::true_type {};

    // ── CEN-Ib4:`on_snoop_resp` 的形状必须恰为 6 参且末位是 `txn_id` ────
    template <class E, class = void>
    struct SnoopRespCarriesTxnId : std::false_type {};
    template <class E>
    struct SnoopRespCarriesTxnId<E, std::void_t<decltype(&E::on_snoop_resp)>>
        : std::bool_constant<std::is_same_v<
              decltype(&E::on_snoop_resp),
              void (E::*)(std::uint64_t, bool, LineState, bool, std::uint64_t,
                          std::uint64_t)>> {};

    // ── CEN-Ib9:`on_barrier` 的形状必须恰为 2 参(domain + txn_id)────────
    //    ⇒ **不含** `group_id` / `participant_count` / `epoch_id`
    template <class E, class = void>
    struct BarrierCarriesNoGroupFields : std::false_type {};
    template <class E>
    struct BarrierCarriesNoGroupFields<E, std::void_t<decltype(&E::on_barrier)>>
        : std::bool_constant<std::is_same_v<
              decltype(&E::on_barrier), void (E::*)(std::uint64_t, std::uint64_t)>> {};

    // ── CEN-Ib10:`on_snoop_completion_ack` 是**独立入口**(含 `txn_id`) ────
    //    ⇒ ack 配对 `snoop_resp`,不与 `completion` 混用(故要求 ack 入口存在且带 txn_id)
    template <class E, class = void>
    struct HasIndependentSnoopAck : std::false_type {};
    template <class E>
    struct HasIndependentSnoopAck<E, std::void_t<decltype(&E::on_snoop_completion_ack)>>
        : std::bool_constant<std::is_same_v<
              decltype(&E::on_snoop_completion_ack),
              void (E::*)(std::uint64_t, std::uint64_t, std::uint64_t)>> {};

    // ── 反向 fixture 的「故意坏类型」(仅用于 trait 判别力自证)────────────
    struct BadNoSnoopResp {  // 缺 `on_snoop_resp` ⇒ 违反 CEN-Ib3
        virtual void on_snoop(std::uint64_t, SnoopType, std::uint64_t) = 0;
    };
    struct BadSnoopRespWithoutTxnId {  // `on_snoop_resp` 少 `txn_id` ⇒ 违反 CEN-Ib4
        virtual void on_snoop_resp(std::uint64_t, bool, LineState, bool, std::uint64_t) = 0;
    };
    struct BadBarrierWithGroupFields {  // `on_barrier` 带 group 字段 ⇒ 违反 CEN-Ib9
        virtual void on_barrier(std::uint64_t, std::uint64_t, std::uint64_t) = 0;
    };
    struct BadNoSnoopAck {  // 无 ack 入口 ⇒ 违反 CEN-Ib10
        virtual void on_snoop(std::uint64_t, SnoopType, std::uint64_t) = 0;
        virtual void on_snoop_resp(std::uint64_t, bool, LineState, bool, std::uint64_t,
                                   std::uint64_t) = 0;
    };

}  // namespace

// INV-CARRIER: CEN-Ib3
TEST_CASE("CEN-Ib3:`snoop` 必有 `snoop_resp`(两个回调入口同时存在)", "[cep][canonical-flow]") {
    INFO("ProtocolEngineBase<2> 必须同时暴露 on_snoop 与 on_snoop_resp");
    constexpr bool kHasPair = HasSnoopPair<Engine2>::value;
    REQUIRE(kHasPair);

    INFO("反向 fixture:缺 on_snoop_resp 的引擎必须被判否(trait 有判别力)");
    constexpr bool kBadHasPair = HasSnoopPair<BadNoSnoopResp>::value;
    REQUIRE_FALSE(kBadHasPair);
}

// INV-CARRIER: CEN-Ib4
TEST_CASE("CEN-Ib4:`snoop_resp` 必须含 `txn_id` 用于 ack 配对", "[cep][canonical-flow]") {
    INFO("on_snoop_resp 的形状必须是 (addr, hit, state, dirty, data, txn_id)");
    constexpr bool kCarriesTxnId = SnoopRespCarriesTxnId<Engine2>::value;
    REQUIRE(kCarriesTxnId);

    INFO("反向 fixture:少 txn_id 的签名必须被判否");
    constexpr bool kBadCarriesTxnId = SnoopRespCarriesTxnId<BadSnoopRespWithoutTxnId>::value;
    REQUIRE_FALSE(kBadCarriesTxnId);
}

// INV-CARRIER: CEN-Ib9
TEST_CASE("CEN-Ib9:`barrier` 流不携带 group_id / participant_count / epoch_id",
          "[cep][canonical-flow]") {
    INFO("on_barrier 必须是 (domain, txn_id) 两参形态");
    constexpr bool kNoGroupFields = BarrierCarriesNoGroupFields<Engine2>::value;
    REQUIRE(kNoGroupFields);

    INFO("反向 fixture:带 group 字段的 barrier 必须被判否");
    constexpr bool kBadNoGroupFields = BarrierCarriesNoGroupFields<BadBarrierWithGroupFields>::value;
    REQUIRE_FALSE(kBadNoGroupFields);
}

// INV-CARRIER: CEN-Ib10
TEST_CASE("CEN-Ib10:`snoop_completion_ack` 配对 `snoop_resp` 不配对 `completion`",
          "[cep][canonical-flow]") {
    INFO("必须存在独立的 on_snoop_completion_ack 入口(addr, node_id, txn_id)");
    constexpr bool kHasAck = HasIndependentSnoopAck<Engine2>::value;
    REQUIRE(kHasAck);

    INFO("反向 fixture:无 ack 入口的引擎必须被判否");
    constexpr bool kBadHasAck = HasIndependentSnoopAck<BadNoSnoopAck>::value;
    REQUIRE_FALSE(kBadHasAck);
}
