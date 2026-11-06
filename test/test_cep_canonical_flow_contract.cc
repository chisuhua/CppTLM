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
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/moesi_gpu_adapter.hh"
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
    template <class E, class = void> struct HasSnoopPair : std::false_type {};
    template <class E>
    struct HasSnoopPair<E, std::void_t<decltype(&E::on_snoop), decltype(&E::on_snoop_resp)>>
        : std::true_type {};

    // ── CEN-Ib4:`on_snoop_resp` 的形状必须恰为 6 参且末位是 `txn_id` ────
    template <class E, class = void> struct SnoopRespCarriesTxnId : std::false_type {};
    template <class E>
    struct SnoopRespCarriesTxnId<E, std::void_t<decltype(&E::on_snoop_resp)>>
        : std::bool_constant<std::is_same_v<decltype(&E::on_snoop_resp),
                                            void (E::*)(std::uint64_t, bool, LineState, bool,
                                                        std::uint64_t, std::uint64_t)>> {};

    // ── CEN-Ib9:`on_barrier` 的形状必须恰为 2 参(domain + txn_id)────────
    //    ⇒ **不含** `group_id` / `participant_count` / `epoch_id`
    template <class E, class = void> struct BarrierCarriesNoGroupFields : std::false_type {};
    template <class E>
    struct BarrierCarriesNoGroupFields<E, std::void_t<decltype(&E::on_barrier)>>
        : std::bool_constant<
              std::is_same_v<decltype(&E::on_barrier), void (E::*)(std::uint64_t, std::uint64_t)>> {
    };

    // ── CEN-Ib10:`on_snoop_completion_ack` 是**独立入口**(含 `txn_id`) ────
    //    ⇒ ack 配对 `snoop_resp`,不与 `completion` 混用(故要求 ack 入口存在且带 txn_id)
    template <class E, class = void> struct HasIndependentSnoopAck : std::false_type {};
    template <class E>
    struct HasIndependentSnoopAck<E, std::void_t<decltype(&E::on_snoop_completion_ack)>>
        : std::bool_constant<
              std::is_same_v<decltype(&E::on_snoop_completion_ack),
                             void (E::*)(std::uint64_t, std::uint64_t, std::uint64_t)>> {};

    // ── 反向 fixture 的「故意坏类型」(仅用于 trait 判别力自证)────────────
    struct BadNoSnoopResp { // 缺 `on_snoop_resp` ⇒ 违反 CEN-Ib3
        virtual void on_snoop(std::uint64_t, SnoopType, std::uint64_t) = 0;
    };
    struct BadSnoopRespWithoutTxnId { // `on_snoop_resp` 少 `txn_id` ⇒ 违反 CEN-Ib4
        virtual void on_snoop_resp(std::uint64_t, bool, LineState, bool, std::uint64_t) = 0;
    };
    struct BadBarrierWithGroupFields { // `on_barrier` 带 group 字段 ⇒ 违反 CEN-Ib9
        virtual void on_barrier(std::uint64_t, std::uint64_t, std::uint64_t) = 0;
    };
    // ── CEN-Ib6:`forward_data` 对 stub **透明** ⇒ stub 的 completion 接口
    //    **不得**含任何「数据来源」判别字段(per ADR-CEP-05 §2 决策 3「契约测试 1 必须钉死」)
    template <class S, class = void> struct CompletionHasNoSourceDiscriminator : std::false_type {};
    template <class S>
    struct CompletionHasNoSourceDiscriminator<S, std::void_t<decltype(&S::completion)>>
        : std::bool_constant<
              std::is_same_v<decltype(&S::completion), bool (S::*)(std::uint64_t, std::uint64_t,
                                                                   std::uint64_t, Perm, bool)>> {};

    struct BadCompletionWithSourceField { // completion 带「来自 peer」判别位 ⇒ 破坏不可分辨性
        virtual bool completion(std::uint64_t, std::uint64_t, std::uint64_t, Perm, bool,
                                bool from_peer) = 0;
    };
    struct BadNoSnoopAck { // 无 ack 入口 ⇒ 违反 CEN-Ib10
        virtual void on_snoop(std::uint64_t, SnoopType, std::uint64_t) = 0;
        virtual void on_snoop_resp(std::uint64_t, bool, LineState, bool, std::uint64_t,
                                   std::uint64_t) = 0;
    };

} // namespace

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
    constexpr bool kBadNoGroupFields =
        BarrierCarriesNoGroupFields<BadBarrierWithGroupFields>::value;
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
    constexpr bool kNoSource = CompletionHasNoSourceDiscriminator<tlm::cep::StubCacheCore>::value;
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
    template <class T, class = void> struct HasOrderingAssumptionMember : std::false_type {};
    template <class T>
    struct HasOrderingAssumptionMember<T, std::void_t<decltype(&T::ordering_assumption)>>
        : std::true_type {};

    template <class T, class = void> struct HasVcBindingMember : std::false_type {};
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

} // namespace

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
        INFO("实例 " << inst.value("id", std::string("?")) << " 的组合接缝缺失: " << join(probs));
        REQUIRE(probs.empty());
        ++n_inst;
    }
    // 声明侧实例数(与 `07 号 §5.1.1` 的 8 实例口径不同层,见该节)
    // ⚠️ 2026-11-06:8(per 台账 Entry #239 —— 新增 `ualink_4`;此前 7 与 07 号的 8 口径**不一致**,本次修复)
    REQUIRE(n_inst == 8u);

    // ── ③ 接缝被组合逻辑**消费**(行为):ordering gate 对每个实例产出 J1 判定 ──
    //    J1 = 「响应向路径必须声明 p2p_ordered」—— 其判定输入正是 `vc_binding`(定位
    //    响应向消息类绑定的 VC)+ `ordering_assumption`(该 VC 的排序假设)。
    //    ⇒ 逐实例 J1 判定存在 = 接缝被真的消费(而非只存在于 JSON 里)。
    nlohmann::json cfg;
    cfg["cep"] = nlohmann::json::object(); // 未指定 ⇒ 落本仓默认声明
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

// ═════════════════════════════════════════════════════════════════════
// CEN-Ib2 —— `completion` **不携带 unsolicited 数据**(per `INV-SC4`)
//   判据(per `ADR-CEP-02 §3`):无配对的 completion 不得把数据落位。
//   ⇒ 机器化 = **行为**:未配对 completion 必须被拒 + **line 不出现**。
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEN-Ib2:`completion` 不携带 unsolicited 数据(未配对 ⇒ 拒绝且不落位)",
          "[cep][canonical-flow]") {
    tlm::cep::StubCacheCore stub;
    const std::uint64_t addr = 0x8000;

    // 正控:配对的 completion ⇒ 接受且落位(证明「拒绝」不是恒真)
    const auto req = stub.request(addr, Perm::UNIQUE);
    // INV-CARRIER: CEN-Ib2
    // 变红理由(per ADR-CEP-06 SU#4 规则 B):未配对 completion 被接受 / 数据被落位 ⇒ 红
    REQUIRE(req.accepted);
    REQUIRE(stub.completion(addr, req.txn_id, /*data=*/0xCAFE, Perm::UNIQUE, /*error=*/false));
    REQUIRE(stub.has_line(addr));
    REQUIRE(stub.state_of(addr) == tlm::cep::LineState::E);

    // 反控(判据主体):**未配对** completion ⇒ 拒绝 + **不得落位**
    const std::uint64_t fresh = 0x9000;
    REQUIRE_FALSE(stub.completion(fresh, /*txn_id=*/0xDEAD, 0xBEEF, Perm::UNIQUE, false));
    INFO("未配对 completion 不得把数据落位(unsolicited 数据 = 违例)");
    REQUIRE_FALSE(stub.has_line(fresh)); // ⛔ 数据必须**没有**进缓存
    REQUIRE(stub.counters().unsolicited_completions == 1u);

    // txn_id 不匹配(有 MSHR 但 txn 不同)同样必须拒绝
    const auto req2 = stub.request(0xA000, Perm::READ);
    REQUIRE(req2.accepted);
    REQUIRE_FALSE(stub.completion(0xA000, req2.txn_id + 1, 0x1, Perm::READ, false));
    INFO("txn 不匹配的 completion 不得落位");
    REQUIRE_FALSE(stub.has_line(0xA000));
    REQUIRE(stub.counters().unsolicited_completions == 2u);
}

// ═════════════════════════════════════════════════════════════════════
// CEN-Ib5 —— `writeback` 不与 `request` 竞态(per `INV-SC2` evict 互斥)
//   判据:在途(MSHR 有条目)的行**不得**被驱逐 ⇒ 驱逐与 in-flight request 互斥。
// ═════════════════════════════════════════════════════════════════════
// INV-CARRIER: CEN-Ib5
// 变红理由(per ADR-CEP-06 SU#4 规则 B):在途行被驱逐(evict 与 request 竞态)⇒ 红
TEST_CASE("CEN-Ib5:`writeback` 不与 `request` 竞态(在途行拒绝驱逐)", "[cep][canonical-flow]") {
    tlm::cep::StubCacheCore stub;
    const std::uint64_t addr = 0x8100;

    // 建一条 clean 行(先配对 completion)
    const auto rq = stub.request(addr, Perm::READ);
    REQUIRE(rq.accepted);
    REQUIRE(stub.completion(addr, rq.txn_id, 0x1, Perm::READ, false));

    // ① 正控:非在途行可驱逐(证明「拒绝」不是恒真)
    const auto ok_ev = stub.evict_line(addr);
    REQUIRE(ok_ev.valid);
    REQUIRE_FALSE(stub.has_line(addr));

    // ② 判据主体:发起 request(在途)后驱逐 ⇒ **拒绝**,且 line 不被移除
    const auto inflight = stub.request(addr, Perm::UNIQUE);
    REQUIRE(inflight.accepted);
    REQUIRE(stub.is_outstanding(addr));
    const auto rejected = stub.evict_line(addr);
    INFO("在途行必须拒驱逐(否则 evict 与 request 竞态)");
    REQUIRE_FALSE(rejected.valid);
    REQUIRE(stub.counters().evict_skipped == 1u);

    // ③ completion 后不再在途 ⇒ 驱逐恢复可用(证明拒绝原因 = outstanding)
    REQUIRE(stub.completion(addr, inflight.txn_id, 0x2, Perm::UNIQUE, false));
    REQUIRE_FALSE(stub.is_outstanding(addr));
    REQUIRE(stub.evict_line(addr).valid);
}

// ═════════════════════════════════════════════════════════════════════
// CEN-Ib7 —— `back_invalidate` 是 **requester-scoped**,不是 group rendezvous
//   判据(per `ADR-CEP-02 §3`):反向失效只针对**单个 requester**,不携带任何
//   group / participant / epoch 参数(与 `barrier` 的域级语义**可区分**)。
// ═════════════════════════════════════════════════════════════════════
namespace {
    template <class E, class = void> struct BackInvalidateIsRequesterScoped : std::false_type {};
    template <class E>
    struct BackInvalidateIsRequesterScoped<E, std::void_t<decltype(&E::on_back_invalidate)>>
        : std::bool_constant<std::is_same_v<decltype(&E::on_back_invalidate),
                                            void (E::*)(std::uint64_t, SnoopType, std::uint64_t)>> {
    };
    // 对照:trait 必须能区分 group 级签名(否则「无 group 参数」是恒真)
    struct GroupScopedBarrierLike {
        virtual void on_back_invalidate(std::uint64_t, SnoopType, std::uint64_t,
                                        std::uint64_t group_id) = 0;
    };
    template <class E, class = void> struct BackInvalidateHasGroupParam : std::false_type {};
    template <class E>
    struct BackInvalidateHasGroupParam<E, std::void_t<decltype(&E::on_back_invalidate)>>
        : std::bool_constant<
              std::is_same_v<decltype(&E::on_back_invalidate),
                             void (E::*)(std::uint64_t, SnoopType, std::uint64_t, std::uint64_t)>> {
    };
} // namespace

// INV-CARRIER: CEN-Ib7
// 变红理由(per ADR-CEP-06 SU#4 规则 B):back_invalidate 带 group/participant 参数
// (变成 group rendezvous)⇒ 红
TEST_CASE("CEN-Ib7:`back_invalidate` 是 requester-scoped(无 group 参数)", "[cep][canonical-flow]") {
    constexpr bool kScoped = BackInvalidateIsRequesterScoped<Engine2>::value;
    REQUIRE(kScoped);
    // 对照:group 级签名必须被判否(且能被 trait 认出来 ⇒ 该 trait 不是恒真)
    constexpr bool kBadHasGroup = BackInvalidateHasGroupParam<GroupScopedBarrierLike>::value;
    REQUIRE(kBadHasGroup);
    constexpr bool kGoodHasGroup = BackInvalidateHasGroupParam<Engine2>::value;
    REQUIRE_FALSE(kGoodHasGroup);
    // `barrier` 才是域级语义(带 domain 参数)⇒ 两种语义可区分
    constexpr bool kBarrierIsDomain =
        std::is_same_v<decltype(&Engine2::on_barrier),
                       void (Engine2::*)(std::uint64_t, std::uint64_t)>;
    REQUIRE(kBarrierIsDomain);
}

// ═════════════════════════════════════════════════════════════════════
// CEN-Ib11 —— atomic 意图作 `request` 流**独立属性**;`perm` 维持 **4 值锁**
//   判据(per `ADR-CEP-02 §2`):`perm` 取值域**恰 4 值**,**不新增 ATOMIC**;
//   atomic 由独立属性承载(不是 perm 的第 5 个值)。
// ═════════════════════════════════════════════════════════════════════
namespace {

    // perm 取值域白名单 = 恰 4 值(⛔ 不接受第 5 个值)
    bool perm_domain_is_four_values(const nlohmann::json& perm_prop, std::string* why) {
        if (!perm_prop.contains("items") || !perm_prop.at("items").contains("enum")) {
            *why = "schema 的 perm 缺少 items.enum";
            return false;
        }
        std::set<std::string> dom;
        for (const auto& v : perm_prop.at("items").at("enum"))
            dom.insert(v.get<std::string>());
        const std::set<std::string> expected{"NONE", "READ", "UNIQUE", "WRITEBACK"};
        if (dom != expected) {
            *why = "perm 取值域不等于 {NONE, READ, UNIQUE, WRITEBACK}(实际 " +
                   std::to_string(dom.size()) + " 值)";
            return false;
        }
        return true;
    }
} // namespace

// INV-CARRIER: CEN-Ib11
// 变红理由(per ADR-CEP-06 SU#4 规则 B):perm 取值域新增 ATOMIC(第 5 值)/ atomic 被塞进
// perm ⇒ 红
TEST_CASE("CEN-Ib11:atomic 走独立属性,perm 维持 4 值锁", "[cep][canonical-flow]") {
    // ① 编译期:两侧 `Perm` 枚举取值**恰 4 个编号**(0..3,无 ATOMIC 槽位)
    static_assert(static_cast<int>(Perm::NONE) == 0 && static_cast<int>(Perm::READ) == 1 &&
                      static_cast<int>(Perm::UNIQUE) == 2 && static_cast<int>(Perm::WRITEBACK) == 3,
                  "CEN-Ib11: perm 必须恰 4 值(NONE/READ/UNIQUE/WRITEBACK)");
    static_assert(static_cast<int>(tlm::cep::MoesiGpuAdapter::Perm::WRITEBACK) == 3,
                  "CEN-Ib11: 跨实现的 perm 值域须一致");

    // ② schema 的 perm 取值域恰 4 值(反向 fixture:加 ATOMIC ⇒ 判否)
    const auto schema =
        nlohmann::json::parse(read_repo_file_7("include/tlm/cep/protocol_instance.schema.json"));
    std::string why;
    INFO("perm 取值域: " << why);
    REQUIRE(perm_domain_is_four_values(schema.at("properties").at("perm"), &why));

    // 反向 fixture:把 ATOMIC 加进 perm ⇒ 必须判否(证明判别力)
    {
        auto bad = schema.at("properties").at("perm");
        bad["items"]["enum"].push_back("ATOMIC");
        std::string w2;
        REQUIRE_FALSE(perm_domain_is_four_values(bad, &w2));
    }

    // ③ atomic 是**独立顶层属性**(不在 perm 里),且每个实例都声明
    REQUIRE(schema.at("properties").contains("atomic"));
    const auto decl =
        nlohmann::json::parse(read_repo_file_7("include/tlm/cep/protocol_instances.json"));
    for (const auto& inst : decl.at("instances")) {
        INFO("实例未声明独立的 atomic 属性: " << inst.value("id", std::string("?")));
        REQUIRE(inst.contains("atomic"));
        REQUIRE(inst.at("atomic").contains("supported"));
        // perm 数组里不得出现 ATOMIC(atomic 不是 perm 值)
        for (const auto& v : inst.at("perm")) {
            INFO("实例 perm 出现 ATOMIC 值: " << inst.value("id", std::string("?")));
            REQUIRE(v.get<std::string>() != "ATOMIC");
        }
    }
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-05-IC1 —— Stub Cache Core **只暴露稳定状态**(瞬态/私有状态住插件内)
// ═════════════════════════════════════════════════════════════════════
namespace {
    // Stub 侧状态白名单 = 恰 {NONE, I, S, E, M}(⛔ 无瞬态 T / O / F)
    bool stub_state_is_stable(tlm::cep::LineState s) {
        switch (s) {
        case tlm::cep::LineState::NONE:
        case tlm::cep::LineState::I:
        case tlm::cep::LineState::S:
        case tlm::cep::LineState::E:
        case tlm::cep::LineState::M:
            return true;
        }
        return false;
    }
} // namespace

// INV-CARRIER: INV-CEP-05-IC1
// 变红理由(per ADR-CEP-06 SU#4 规则 B):Stub 侧出现瞬态(T/O/F)/ 瞬态有 perm 投影 ⇒ 红
TEST_CASE("INV-CEP-05-IC1:Stub 只暴露稳定状态,瞬态住插件内", "[cep][canonical-flow]") {
    // ① 行为:Stub 的任何状态取值都是**稳定态**(穷举 switch 白名单)
    //    实跑:read 建行(S 或 I)、write 建行(E/M)、completion(error) 建行(I)
    tlm::cep::StubCacheCore stub;
    const auto r1 = stub.request(0x1000, Perm::READ);
    REQUIRE(stub.completion(0x1000, r1.txn_id, 1, Perm::READ, false));
    REQUIRE(stub_state_is_stable(stub.state_of(0x1000)));
    const auto r2 = stub.request(0x2000, Perm::UNIQUE);
    REQUIRE(stub.completion(0x2000, r2.txn_id, 2, Perm::UNIQUE, false));
    REQUIRE(stub_state_is_stable(stub.state_of(0x2000)));
    REQUIRE(stub.state_of(0x2000) == tlm::cep::LineState::E); // UNIQUE ⇒ E(稳定)

    // ② 反向 fixture:非稳定态取值必须被判否(证明白名单不是恒真)
    REQUIRE_FALSE(stub_state_is_stable(static_cast<tlm::cep::LineState>(99)));

    // ③ 瞬态**住在插件内**:MoesiGpu 有 `T` 态,但 `perm_of(T)` **无投影**
    //    ⇒ 瞬态不跨域接口流出(per 本条 + `ADR-CEP-13` 的跨域接口约束)
    using MA = tlm::cep::MoesiGpuAdapter;
    constexpr bool kHasTransient =
        MA::kTransientStateCount == 1 &&
        MA::kTransientStates[0] == static_cast<std::uint8_t>(MA::State::T);
    REQUIRE(kHasTransient);
    MA::Perm proj = MA::Perm::NONE;
    const bool kTransientHasProjection = MA::perm_of(MA::State::T, proj);
    INFO("瞬态 T **不得**有 perm 投影(否则瞬态会流出到跨域接口)");
    REQUIRE_FALSE(kTransientHasProjection);
    // 对照:稳定态**有**投影(证明「无投影」不是恒真)
    REQUIRE(MA::perm_of(MA::State::M, proj));
    REQUIRE(proj == MA::Perm::WRITEBACK);
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-07A-IC2 —— 与协议无关的层**不得**出现协议标识符
//   例外:ProtocolEngine 插件内部 + capability schema 的数据值。
// ═════════════════════════════════════════════════════════════════════
namespace {

    // 去注释(行注释 + 块注释),避免「文档提及」被当作「代码泄漏」
    std::string strip_comments(const std::string& src) {
        std::string out;
        out.reserve(src.size());
        bool in_line = false, in_block = false;
        for (std::size_t i = 0; i < src.size(); ++i) {
            if (in_line) {
                if (src[i] == '\n') {
                    in_line = false;
                    out += '\n';
                }
                continue;
            }
            if (in_block) {
                if (i + 1 < src.size() && src[i] == '*' && src[i + 1] == '/') {
                    in_block = false;
                    ++i;
                } else if (src[i] == '\n') {
                    out += '\n';
                }
                continue;
            }
            if (i + 1 < src.size() && src[i] == '/' && src[i + 1] == '/') {
                in_line = true;
                ++i;
                continue;
            }
            if (i + 1 < src.size() && src[i] == '/' && src[i + 1] == '*') {
                in_block = true;
                ++i;
                continue;
            }
            out += src[i];
        }
        return out;
    }

    // 返回命中的协议标识符(**词边界**;⛔ 避免 `HIERARCHICAL` 被 `CHI` 误伤)
    std::vector<std::string> protocol_identifiers(const std::string& code) {
        // ⚠️ 前边界 `\b` + 后向断言 `(?![A-Za-z0-9])`:
        //    既命中 `CHI_mode`(协议前缀泄漏),又不误伤 `HIERARCHICAL` / `CHIps`。
        static const std::regex kRe(
            R"(\b(CHI|CXL|TileLink|NVLink|UALink|MESIF|MOESI)(?![A-Za-z0-9]))");
        std::vector<std::string> out;
        for (std::sregex_iterator it(code.begin(), code.end(), kRe), end; it != end; ++it)
            out.push_back(it->str());
        return out;
    }

} // namespace

// Engine 框架/依赖图/SharerTracker)代码中出现协议标识符 ⇒ 红
TEST_CASE("INV-CEP-07A-IC2:与协议无关的层代码中无协议标识符", "[cep][canonical-flow]") {
    // 协议无关层 = 框架/契约层(不含各协议 adapter/engine 插件)
    static const char* kNeutral[] = {
        "include/tlm/cep/stub_cache_core.hh",      "include/tlm/cep/ordering_point_checker.hh",
        "include/tlm/cep/collective_plane.hh",     "include/tlm/cep/address_resolver.hh",
        "include/tlm/cep/protocol_engine_base.hh", "include/tlm/cep/channel_dependency_graph.hh",
        "include/tlm/cep/sharer_tracker.hh",
    };
    for (const char* f : kNeutral) {
        const auto src = read_repo_file_7(f);
        // INV-CARRIER: INV-CEP-07A-IC2
        // 变红理由(per ADR-CEP-06 SU#4 规则 B):协议无关层(Stub/Ordering/Collective/AddressResolver/
        REQUIRE(!src.empty());
        const auto hits = protocol_identifiers(strip_comments(src));
        std::string joined;
        for (const auto& h : hits) {
            if (!joined.empty())
                joined += ",";
            joined += h;
        }
        INFO("协议无关层代码泄漏协议标识符: " << f << " ⇒ " << joined);
        REQUIRE(hits.empty());
    }

    // 反向 fixture(判别力证据)──────────────────────────────────────────
    // ① 代码中的 `CHI` ⇒ 必须命中
    {
        const auto hits = protocol_identifiers(strip_comments("int CHI_mode = 1;"));
        REQUIRE(hits.size() == 1u);
        REQUIRE(hits[0] == "CHI");
    }
    // ② `HIERARCHICAL` **不得**被 `CHI` 误伤(词边界)
    {
        const auto hits = protocol_identifiers(strip_comments("auto x = HIERARCHICAL;"));
        REQUIRE(hits.empty());
    }
    // ③ 注释里的协议名不算泄漏(去注释后为空)
    {
        const auto hits = protocol_identifiers(strip_comments("// per CXL 2.0 与 CHI\nint x = 1;"));
        REQUIRE(hits.empty());
    }
    // ④ 块注释同样被剥离
    {
        const auto hits =
            protocol_identifiers(strip_comments("/* MESIF / MOESI */ int y = 2; // NVLink\n"));
        REQUIRE(hits.empty());
    }
}
