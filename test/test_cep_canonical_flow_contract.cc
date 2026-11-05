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
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/ordering_point_checker.hh"
#include "tlm/cep/protocol_engine_base.hh"
#include "tlm/cep/stub_cache_core.hh"

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
    // ── CEN-Ib6:`forward_data` 对 stub **透明** ⇒ stub 的 completion 接口
    //    **不得**含任何「数据来源」判别字段(per ADR-CEP-05 §2 决策 3「契约测试 1 必须钉死」)
    template <class S, class = void>
    struct CompletionHasNoSourceDiscriminator : std::false_type {};
    template <class S>
    struct CompletionHasNoSourceDiscriminator<S, std::void_t<decltype(&S::completion)>>
        : std::bool_constant<std::is_same_v<
              decltype(&S::completion),
              bool (S::*)(std::uint64_t, std::uint64_t, std::uint64_t, Perm, bool)>> {};

    struct BadCompletionWithSourceField {  // completion 带「来自 peer」判别位 ⇒ 破坏不可分辨性
        virtual bool completion(std::uint64_t, std::uint64_t, std::uint64_t, Perm, bool,
                               bool from_peer) = 0;
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
// INV-CARRIER: CEN-Ib1
TEST_CASE("CEN-Ib10:`snoop_completion_ack` 配对 `snoop_resp` 不配对 `completion`",
          "[cep][canonical-flow]") {
    INFO("必须存在独立的 on_snoop_completion_ack 入口(addr, node_id, txn_id)");
    constexpr bool kHasAck = HasIndependentSnoopAck<Engine2>::value;
    REQUIRE(kHasAck);

    INFO("反向 fixture:无 ack 入口的引擎必须被判否");
    constexpr bool kBadHasAck = HasIndependentSnoopAck<BadNoSnoopAck>::value;
    REQUIRE_FALSE(kBadHasAck);
}

// INV-CARRIER: CEN-Ib6
TEST_CASE("CEN-Ib6:`forward_data` 对 stub 透明 —— completion 接口无来源判别字段",
          "[cep][canonical-flow]") {
    INFO("StubCacheCore::completion 的形状必须是 (addr, txn_id, data, perm, error) —— 无来源位");
    constexpr bool kNoSource =
        CompletionHasNoSourceDiscriminator<tlm::cep::StubCacheCore>::value;
    REQUIRE(kNoSource);

    INFO("反向 fixture:带来源判别位的 completion 必须被判否(否则 stub 可分辨来源)");
    constexpr bool kBadHasSource =
        CompletionHasNoSourceDiscriminator<BadCompletionWithSourceField>::value;
    REQUIRE_FALSE(kBadHasSource);
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-07-IC7 —— 插件接口**必须**为组合维度预留接缝
//   判据(per `ADR-CEP-07 §3` IC7 + §2 决策 9):插件**声明**排序假设与消息→VC 绑定,
//   **不实现**组合逻辑;组合接缝点须被合同测试覆盖。
//   ⇒ 三段机器化:
//     ① 插件基类内**不得**持有组合逻辑的载体字段(编译期 trait)
//     ② 接缝在**声明侧**持久化:每个协议实例都声明 per-network 的
//        `ordering_assumption` + `vc_binding`(非空对象)
//     ③ 接缝被**组合逻辑消费**:ordering gate 对每个实例产出 J1 判定
//   ⚠️ 变红条件:引擎基类出现这两字段 / 某实例缺声明 / 闸不再消费接缝 ⇒ 红
// ═════════════════════════════════════════════════════════════════════

namespace {

    namespace cep7 = tlm::cep;

    // ① 编译期:trait 检测「某类型是否持有该数据成员」
    template <class T, class = void>
    struct HasOrderingAssumptionMember : std::false_type {};
    template <class T>
    struct HasOrderingAssumptionMember<T, std::void_t<decltype(&T::ordering_assumption)>>
        : std::true_type {};

    template <class T, class = void>
    struct HasVcBindingMember : std::false_type {};
    template <class T>
    struct HasVcBindingMember<T, std::void_t<decltype(&T::vc_binding)>> : std::true_type {};

    // 反向 fixture 用的「坏插件」:把组合声明塞进插件内 ⇒ 检测器必须命中
    struct BadEngineWithCombinationState {
        int ordering_assumption = 0;
        int vc_binding = 0;
    };

    // ② 声明侧:每个实例的组合接缝是否齐备(返回缺失项,空 = 齐备)
    std::vector<std::string> seam_problems(const nlohmann::json& inst) {
        std::vector<std::string> out;
        for (const char* field : {"ordering_assumption", "vc_binding"}) {
            if (!inst.contains(field) || !inst.at(field).is_object() || inst.at(field).empty()) {
                out.push_back(std::string("缺 ") + field + "(或为空对象)");
                continue;
            }
            // per-network 嵌套:每个网络的值必须是**非空对象**
            for (auto it = inst.at(field).begin(); it != inst.at(field).end(); ++it) {
                if (!it.value().is_object() || it.value().empty())
                    out.push_back(std::string(field) + " 的网络 `" + it.key() + "` 为空");
            }
        }
        return out;
    }

    std::string read_repo_file_7(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    std::string join(const std::vector<std::string>& v) {
        std::string s;
        for (const auto& x : v) {
            if (!s.empty())
                s += "; ";
            s += x;
        }
        return s;
    }

}  // namespace

// INV-CARRIER: INV-CEP-07-IC7
// 变红理由(per ADR-CEP-06 SU#4 规则 B):插件基类持有组合逻辑字段 / 某实例缺
// ordering_assumption 或 vc_binding 声明 / 闸不再消费接缝 ⇒ 红
TEST_CASE("CEP 组合接缝: IC7 —— 插件只声明排序假设与 VC 绑定,组合逻辑不在插件内",
          "[cep][canonical-flow]") {
    // ── ① 插件基类内**不得**持有组合逻辑的载体字段(编译期 trait)────────────
    constexpr bool kEngineHoldsOrdering = HasOrderingAssumptionMember<Engine2>::value;
    constexpr bool kEngineHoldsVcBinding = HasVcBindingMember<Engine2>::value;
    REQUIRE_FALSE(kEngineHoldsOrdering);
    REQUIRE_FALSE(kEngineHoldsVcBinding);

    // ── ② 接缝在声明侧持久化:每个协议实例都声明两者(per-network 非空)──────
    const auto decl_text = read_repo_file_7("include/tlm/cep/protocol_instances.json");
    REQUIRE(!decl_text.empty());
    const auto decl = nlohmann::json::parse(decl_text);
    REQUIRE(decl.contains("instances"));
    std::size_t n_inst = 0;
    for (const auto& inst : decl.at("instances")) {
        const auto probs = seam_problems(inst);
        INFO("实例 " << inst.value("id", std::string("?")) << " 的组合接缝缺失: "
                     << join(probs));
        REQUIRE(probs.empty());
        ++n_inst;
    }
    REQUIRE(n_inst == 7u);  // 声明侧实例数(与 `07 号 §5.1.1` 的 8 实例口径不同层,见该节)

    // ── ③ 接缝被组合逻辑**消费**(行为):ordering gate 对每个实例产出 J1 判定 ──
    //    J1 = 「响应向路径必须声明 p2p_ordered」—— 其判定输入正是 `vc_binding`(定位
    //    响应向消息类绑定的 VC)+ `ordering_assumption`(该 VC 的排序假设)。
    //    ⇒ 逐实例 J1 判定存在 = 接缝被真的消费(而非只存在于 JSON 里)。
    nlohmann::json cfg;
    cfg["cep"] = nlohmann::json::object();  // 未指定 ⇒ 落本仓默认声明
    const auto rep = cep7::run_ordering_gate(cfg);
    INFO("闸未生效(SKIP)⇒ 接缝未被消费: " << rep.to_log());
    REQUIRE(rep.verdict != cep7::GateVerdict::SKIP);
    std::size_t j1 = 0;
    for (const auto& r : rep.results) {
        if (r.criterion == cep7::CriterionId::J1_SinglePoint)
            ++j1;
    }
    INFO("J1 判定数(" << j1 << ")应与实例数(" << n_inst << ")一致");
    REQUIRE(j1 == n_inst);

    // ── ④ 反向 fixture(判别力证据)────────────────────────────────────────
    // (a) 「坏插件」持有组合字段 ⇒ 检测器**必须**命中(否则 ① 恒假)
    REQUIRE(HasOrderingAssumptionMember<BadEngineWithCombinationState>::value);
    REQUIRE(HasVcBindingMember<BadEngineWithCombinationState>::value);

    // (b) 缺 `vc_binding` 的实例 ⇒ 声明侧检查必须判否
    {
        nlohmann::json bad = nlohmann::json::parse(R"({
            "id": "fake", "ordering_assumption": {"compute_noc": {"0": "unordered"}}
        })");
        const auto probs = seam_problems(bad);
        INFO("反向 fixture 应判否,实际缺失项: " << join(probs));
        REQUIRE_FALSE(probs.empty());
    }
    // (c) 某网络的 `vc_binding` 为空对象 ⇒ 必须判否
    {
        nlohmann::json bad = nlohmann::json::parse(R"({
            "id": "fake",
            "ordering_assumption": {"compute_noc": {"0": "unordered"}},
            "vc_binding": {"compute_noc": {}}
        })");
        const auto probs = seam_problems(bad);
        INFO("反向 fixture 应判否,实际缺失项: " << join(probs));
        REQUIRE_FALSE(probs.empty());
    }
    // (d) 齐备的实例 ⇒ 必须判否为「通过」(证明正向路径非恒假)
    {
        nlohmann::json good = nlohmann::json::parse(R"({
            "id": "fake",
            "ordering_assumption": {"compute_noc": {"0": "unordered", "1": "p2p_ordered"}},
            "vc_binding": {"compute_noc": {"request": 0, "response": 1}}
        })");
        const auto probs = seam_problems(good);
        REQUIRE(probs.empty());
    }
}
