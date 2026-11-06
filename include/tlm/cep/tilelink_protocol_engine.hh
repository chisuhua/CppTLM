// =============================================================================
// include/tlm/cep/tilelink_protocol_engine.hh
//   —— TileLink-C(TileLink Tree Manager)**运行时引擎**(CohIf 意图面 → 事件面)
//
// 作者: CppTLM Team / 日期: 2026-11-06
// 配套: openspec/changes/cpptlm-cep-tilelink-runtime-engine/(proposal + design D-1~D-7b
//       + delta spec TLE-01~07 + tasks P0-0~P3-12)
//       台账 obj-cep-platform.md Entry #243(立项)· #245(F2 漏勾授权)
//       规约真源(只读消费):include/tlm/cep/tilelink_state_spec.hh 的
//         `kT3MappingCells`(11 行,TLC-M-01~11)· `t3_consumable_event_mask` · `t3_basis_of`
//       锚点审计面:docs/research/2026-11-06-cep-tilelink-t3-mapping-anchor-summary.md
//       引擎形态先例:include/tlm/cep/chi_rnf_protocol_engine.hh · mesif_protocol_engine.hh
//
// ── 定位 ────────────────────────────────────────────────────────────────────
// **不是**规约层(12 格转移矩阵 = `tilelink_state_spec.hh` 的真源,引擎**只读**);
// **不是**缓存/网络组件(不接 `connections[]`、不进 `ModuleFactory`、不注册 ABI)。
// 它是 **CohIf 意图面 → TileLink-C 权限转移事件面** 的**事件计数/上报器**:
// 每个 canonical 意图到达 ⇒ 经**唯一消费载具**取 T3 掩码 ⇒ 按事件类别计数。
//
// ── 纪律(per design D-2,metis BLOCKER M-3)──────────────────────────────────
// ① **零自有事件表副本**:引擎内 0 处「意图 → 事件」字面映射;
//    ⛔ 不得持有 `t3_consumable_event_mask` 结果的**成员数组**(构造期拷贝 = 变相副本,
//    且行为与真源逐位相同 ⇒ 毒化实测**不可区分** ⇒ 纪律被架空)。
// ② **唯一消费载具** `consumable_mask_of(PlatformIntent)`:所有 `on_*` 一律经它;
//    ⛔ 不得直接引用 `kT3MappingCells` / `event_mask` 字段。
// ③ **不携带源态**(TLE-05):引擎只声明**事件类别**;⛔ 不引用 `TileLinkStateSpec::State`
//    / 转移矩阵 / `transition_cell*` ⇒ 不在运行面重发明规约真源,
//    也不预置 TileLink `Cap`「源不定」的源态假设。
// ④ **fail-closed 自动传导**(TLE-06):某行 basis = `Pending` ⇒ `t3_consumable_event_mask` = 0
//    ⇒ 引擎**零事件**;⛔ 消费者 SHALL NOT 把「未锚」解释为「无副作用」——
//    零事件断言必须同时校验 `t3_basis_of(...) == Direct`。
//
// ── 边界 ────────────────────────────────────────────────────────────────────
// · **header-only**;`ProtocolEngineBase<2>` 派生(与 `CohIfProtocolEngine` 同端口数 N=2
//   ⇒ JSON 侧可逐实例选择引擎类型,per `chstream_register.hh` 的
//   `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED("TileLinkProtocolEngine", ...)`)。
// · **不解析 JSON**(声明是唯一真源,`vc_binding` 由装配方 `set_route(...)` 注入)。
// · **不注册 ABI** · **不扩** `CATEGORY_*` / `perm` 4 值 / canonical 10 流词表。
// · **不建** PE 本地树状态机(Nothing/Branch/Tip)—— 那是规约层真源 + 另一原型范围。
// · **已知基类限制**:`on_request(addr, perm, txn_id)` **不透传** bundle 的 `atomic` 位
//   ⇒ atomic 意图在 CohIf 腿上**无 `on_*` 落点** ⇒ 本引擎提供**显式落点**
//   `note_atomic_intent()`(供 11-intent 参数化断言覆盖;⛔ 不伪造基类钩子)。
//
// =============================================================================

#ifndef CPPTLM_TLM_CEP_TILELINK_PROTOCOL_ENGINE_HH
#define CPPTLM_TLM_CEP_TILELINK_PROTOCOL_ENGINE_HH

#include "tlm/cep/protocol_engine_base.hh"
#include "tlm/cep/tilelink_state_spec.hh"

#include <array>
#include <cstdint>
#include <string>

namespace tlm::cep {

// ══════════════════════════════════════════════════════════════════════════
//  TileLinkEngineCounters —— 事件面计数(⛔ **不是**映射副本)
// ══════════════════════════════════════════════════════════════════════════
// ⚠️ 本结构**只存计数**:元素是「到达次数 / 事件次数」,⛔ **不存任何掩码或映射关系**
//   (判别式:若某成员能回答「某意图 ⇒ 何事件」,它就是映射副本 ⇒ 违规)。
// ⚠️ `intent_observed` 是**按意图的到达计数**(11 项),用于「零事件意图确实到过」的断言,
//   ⛔ **不**存 `t3_consumable_event_mask` 的结果。
struct TileLinkEngineCounters {
    /// 4 类权限转移事件(TileLink-C 事件词表 = `TileLinkStateSpec::Event`)的计数
    std::uint64_t cap_events = 0;
    std::uint64_t grow_events = 0;
    std::uint64_t prune_events = 0;
    std::uint64_t report_events = 0;
    /// 事件总数(4 类之和)
    std::uint64_t total_events = 0;
    /// 「观察到意图但零事件」的次数(零事件意图 + fail-closed 行都会落在这里)
    std::uint64_t zero_event_intents = 0;
    /// 每个意图的**到达次数**(非掩码、非映射)
    std::array<std::uint64_t, TileLinkStateSpec::kPlatformIntentCount> intent_observed{};
};

// ══════════════════════════════════════════════════════════════════════════
//  TileLinkProtocolEngine
// ══════════════════════════════════════════════════════════════════════════
class TileLinkProtocolEngine : public ProtocolEngineBase<2> {
public:
    using Base = ProtocolEngineBase<2>;
    using Bundle = bundles::CohIfBundle;
    using Intent = TileLinkStateSpec::PlatformIntent;
    using Event = TileLinkStateSpec::Event;
    using Counters = TileLinkEngineCounters;

    /// 意图总数(= canonical 10 流 + atomic 意图)
    static constexpr std::size_t kIntentCount = TileLinkStateSpec::kPlatformIntentCount;

    TileLinkProtocolEngine(const std::string& name, EventQueue* eq) : Base(name, eq) {}
    ~TileLinkProtocolEngine() override = default;

    const Counters& counters() const { return ctr_; }

    // ── 观测面(事件计数)───────────────────────────────────────────────
    std::uint64_t event_count(Event e) const {
        switch (e) {
            case Event::Cap: return ctr_.cap_events;
            case Event::Grow: return ctr_.grow_events;
            case Event::Prune: return ctr_.prune_events;
            case Event::Report: return ctr_.report_events;
        }
        return 0;
    }
    std::uint64_t total_events() const { return ctr_.total_events; }
    std::uint64_t intent_observed(Intent i) const {
        return ctr_.intent_observed[static_cast<std::size_t>(i)];
    }
    /// 「零事件意图」的到达次数(TLE-02 的 7 条零事件断言用)
    std::uint64_t zero_event_intents() const { return ctr_.zero_event_intents; }

    // ── atomic 意图的**显式落点**(基类 `on_request` 不透传 atomic 位)────
    /// TileLink-C 的 10 条消息表**无** atomic 类消息 ⇒ 该意图**零事件**
    /// (锚点摘要 §2 `TLC-M-11`)。本方法提供**可断言的落点**,⛔ 不伪造 CohIf 钩子。
    void note_atomic_intent() { consume(Intent::AtomicIntent); }

    // ══════════════════════════════════════════════════════════════════
    //  10 个 canonical 流的 `on_*`(类型系统强制全覆盖)
    //  —— 每个都**只**做一件事:把意图交给**唯一消费载具**。
    // ══════════════════════════════════════════════════════════════════
    void on_request(std::uint64_t /*addr*/, Perm /*perm*/, std::uint64_t /*txn_id*/) override {
        consume(Intent::Request);
    }
    void on_completion(std::uint64_t /*txn_id*/, std::uint64_t /*data*/, Perm /*perm*/,
                       bool /*error*/) override {
        consume(Intent::Completion);
    }
    void on_snoop(std::uint64_t /*addr*/, SnoopType /*type*/, std::uint64_t /*txn_id*/) override {
        consume(Intent::Snoop);
    }
    void on_snoop_resp(std::uint64_t /*addr*/, bool /*hit*/, LineState /*state*/, bool /*dirty*/,
                       std::uint64_t /*data*/, std::uint64_t /*txn_id*/) override {
        consume(Intent::SnoopResp);
    }
    void on_writeback(std::uint64_t /*addr*/, std::uint64_t /*data*/, bool /*dirty*/) override {
        consume(Intent::Writeback);
    }
    void on_forward_data(std::uint64_t /*src_addr*/, std::uint64_t /*dst_addr*/,
                         std::uint64_t /*data*/, Perm /*perm*/, std::uint64_t /*txn_id*/) override {
        consume(Intent::ForwardData);
    }
    void on_back_invalidate(std::uint64_t /*addr*/, SnoopType /*type*/,
                            std::uint64_t /*txn_id*/) override {
        consume(Intent::BackInvalidate);
    }
    void on_address_resolve(std::uint64_t /*pa*/, std::uint64_t /*node_id*/,
                            AddressResolveResult& /*out*/) override {
        consume(Intent::AddressResolve);
    }
    void on_barrier(std::uint64_t /*domain*/, std::uint64_t /*txn_id*/) override {
        consume(Intent::Barrier);
    }
    void on_snoop_completion_ack(std::uint64_t /*addr*/, std::uint64_t /*node_id*/,
                                 std::uint64_t /*txn_id*/) override {
        consume(Intent::SnoopCompletionAck);
    }

protected:
    /// **唯一消费载具**(choke point)—— 所有 `on_*` 的事件语义都从这里取。
    /// ⛔ 不得改写为「先缓存到成员再读」(= 变相副本,见纪律 ①)。
    std::uint8_t consumable_mask_of(Intent i) const noexcept {
        return TileLinkStateSpec::t3_consumable_event_mask(i);
    }

private:
    /// 消费一个意图:取掩码 ⇒ 按位计数(**唯一的「事件发射出口」**)。
    /// ⚠️ fail-closed:掩码 0 ⇒ **零事件**;判定「未锚 vs 合法无事件」须另查
    ///    `t3_basis_of(i) == Direct`(⛔ 消费者 SHALL NOT 把未锚当无副作用)。
    void consume(Intent i) {
        const std::uint8_t mask = consumable_mask_of(i);
        ctr_.intent_observed[static_cast<std::size_t>(i)] += 1;
        if (mask == 0) {
            ctr_.zero_event_intents += 1;
            return;
        }
        if (mask & TileLinkStateSpec::kT3EventCap) ++ctr_.cap_events;
        if (mask & TileLinkStateSpec::kT3EventGrow) ++ctr_.grow_events;
        if (mask & TileLinkStateSpec::kT3EventPrune) ++ctr_.prune_events;
        if (mask & TileLinkStateSpec::kT3EventReport) ++ctr_.report_events;
        ctr_.total_events += 1;
    }

    Counters ctr_{};
};

} // namespace tlm::cep

// ══════════════════════════════════════════════════════════════════════════
//  编译期锁(per tasks P1-4)
//  ⚠️ 判据铁律 ③:编译期锁 SHALL **同时**有运行时可判入口
//     (见 `test/test_cep_tilelink_engine.cc` 的 TLE-01/TLE-02/TLE-05 段)。
// ══════════════════════════════════════════════════════════════════════════
static_assert(tlm::cep::TileLinkProtocolEngine::kIntentCount == 11u,
              "TLE: 引擎消费面 SHALL 恰为 11 意图(canonical 10 流 + atomic)");
static_assert(static_cast<std::size_t>(tlm::cep::TileLinkStateSpec::Event::Report) + 1 == 4u,
              "TLE: 事件词表 SHALL 恰为 Cap/Grow/Prune/Report 4 类(引擎不自有词表)");
// ⚠️ 用四个**类别常量**组合表达「掩码全集 = 4 位」,⛔ 不用 `kT3EventAll`
//   (引擎侧的禁止性 lint 判据:`kT3EventAll` 不得出现在引擎里 —— 它是「掩码全集」
//    的速记常量,引用它容易滑向「持有一张掩码表」的形态)。
static_assert((tlm::cep::TileLinkStateSpec::kT3EventCap |
               tlm::cep::TileLinkStateSpec::kT3EventGrow |
               tlm::cep::TileLinkStateSpec::kT3EventPrune |
               tlm::cep::TileLinkStateSpec::kT3EventReport) == 0b1111u,
              "TLE: T3 事件掩码全集 SHALL 为 4 位");

#endif // CPPTLM_TLM_CEP_TILELINK_PROTOCOL_ENGINE_HH
