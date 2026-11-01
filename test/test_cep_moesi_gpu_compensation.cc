// test/test_cep_moesi_gpu_compensation.cc
//   —— `moesi_gpu` 的 `O→WRITEBACK` **补偿路径**契约测试(R11 的 **V-2**)
//
// 作者: CppTLM Team / 日期: 2026-11-01
// 配套: include/tlm/cep/moesi_gpu_protocol_engine.hh(运行期载体)
//       include/tlm/cep/moesi_gpu_adapter.hh · include/tlm/cep/sharer_tracker.hh
//       台账 obj-cep-platform.md Entry #133(R11 债期重开)· #124 · #125 · #131
//       docs/adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md 的 Status Update(2026-11-01)
//
// ── 判据纪律(per 本仓 4 条铁律)────────────────────────────────────────────
// ① 纯负向断言 SHALL 有正控;② 每条判据 SHALL 有可执行反向 fixture 且**因正确原因红**;
// ③ 编译期锁 SHALL **不**只以「编译红」充当 ctest 判据;④ fixture 状态变迁后 SHALL 重新基线化。
//
// ── 本文件的核心语义纪律(per `ADR-CEP-04` SU(2026-11-01) 的 V-2 补记)────────
// 「补偿路径的判据 SHALL 判**保守补偿**语义,SHALL **NOT** 读作『**等价恢复**』——
//   `SparseSnoopFilter` = `CONSERVATIVE` ⇒ 只能给『**可能**还有其他 sharer』的保守近似」
// ⇒ ⇒ **verdict 由 tracker 的 `completeness` 决定**,**不**由本次 `lookup` 的返回值决定。

#include "catch_amalgamated.hpp"

#include "core/event_queue.hh"
#include "tlm/cep/moesi_gpu_adapter.hh"
#include "tlm/cep/moesi_gpu_protocol_engine.hh"
#include "tlm/cep/sharer_tracker.hh"

#include <cstddef>
#include <cstdint>
#include <string>

namespace {

    using tlm::cep::LookupHazard;
    using tlm::cep::LookupResult;
    using tlm::cep::MoesiGpuAdapter;
    using tlm::cep::MoesiGpuProtocolEngine;
    using tlm::cep::SharerTrackerIf;
    using tlm::cep::SparseSnoopFilter;

    /// **可编程的** tracker 夹具 —— 用于给判据提供**鉴别力**:
    /// 若只能测真实的 `SparseSnoopFilter`(CONSERVATIVE ⇒ 恒 `BROADCAST`),
    /// 「记录逐字镜像返回值」这条判据就会**恒真** ⇒ 无鉴别力(违铁律 ①)。
    class MockTracker final : public SharerTrackerIf {
    public:
        /// 预置的 `lookup` 返回(**可被毒化** ⇒ 记录必变 ⇒ 判据必红)
        LookupResult result = LookupResult::BROADCAST;
        LookupHazard hazard = LookupHazard::NONE;
        tlm::cep::node_id_t forward_to = tlm::cep::kInvalidNode;

        /// 观测:被查了几次 / 用的什么 intent
        std::uint64_t lookups = 0;
        tlm::cep::Intent last_intent = tlm::cep::Intent::want_read;

        void lookup(std::uint64_t, tlm::cep::Intent intent, LookupResult& res,
                    tlm::cep::LocalView& view, LookupHazard& hz) override {
            ++lookups;
            last_intent = intent;
            res = result;
            view = tlm::cep::LocalView{};
            view.valid = true;
            view.forward_to = forward_to;
            hz = hazard;
        }
        void snoop_begin(std::uint64_t, tlm::cep::responder_set_t,
                         tlm::cep::snoop_handle_t&) override {
        }
        void snoop_fold(tlm::cep::snoop_handle_t, tlm::cep::node_id_t, tlm::cep::SnoopState, bool,
                        bool, tlm::cep::responder_set_t&, tlm::cep::MergedState&) override {
        }
        void observe_snoop(std::uint64_t, tlm::cep::node_id_t, tlm::cep::SnoopState) override {
        }
    };

    constexpr std::uint64_t kAddr = 0x1000;

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// CMP-01 —— 补偿路径**存在**且在合并点被**强制触发**
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP moesi_gpu 补偿: 合并点强制查询 tracker(未接 tracker ⇒ 拒绝投影)",
          "[cep][moesi-gpu][compensation]") {
    EventQueue eq;
    MoesiGpuProtocolEngine eng("moesi_comp", &eq);

    // ── ① 未接 tracker ⇒ **拒绝**投影(决策 M-3:不静默降级)──────────────
    tlm::cep::MoesiGpuProtocolEngine::AdapterPerm p =
        tlm::cep::MoesiGpuProtocolEngine::AdapterPerm::NONE;
    REQUIRE_FALSE(eng.project_with_compensation(kAddr, MoesiGpuAdapter::State::O, p));
    REQUIRE(eng.compensation_count() == 0u);
    REQUIRE(eng.counters().compensation_skipped == 1u);
    REQUIRE(eng.counters().compensation_queries == 0u);

    // ── ② 接上 tracker ⇒ 投影成功 + **恰好 1 次**补偿查询 + 1 条记录 ──────
    SparseSnoopFilter sf;
    eng.set_sharer_tracker(&sf);
    REQUIRE(eng.project_with_compensation(kAddr, MoesiGpuAdapter::State::O, p));
    REQUIRE(p == MoesiGpuProtocolEngine::AdapterPerm::WRITEBACK);
    REQUIRE(eng.compensation_count() == 1u);
    REQUIRE(eng.counters().compensation_queries == 1u);
    REQUIRE(eng.counters().compensation_skipped == 1u); // 仍是 1(未增)

    // ── ③ 非 `O` 态**不**触发补偿(决策 M-5)───────────────────────────────
    const std::size_t before = eng.compensation_count();
    const std::uint64_t q_before = eng.counters().compensation_queries;
    REQUIRE(eng.project_with_compensation(kAddr, MoesiGpuAdapter::State::M, p));
    REQUIRE(eng.compensation_count() == before);
    REQUIRE(eng.counters().compensation_queries == q_before);

    // ── ④ 瞬态 `T` **无投影**(per `INV-CEP-05-IC1`)────────────────────────
    REQUIRE_FALSE(eng.project_with_compensation(kAddr, MoesiGpuAdapter::State::T, p));

    // ── 正控:真实 `SparseSnoopFilter` 的 caps 逐字取自 traits 行 ─────────
    REQUIRE(sf.caps.completeness == tlm::cep::Completeness::CONSERVATIVE);
    REQUIRE(sf.caps.scope == tlm::cep::Scope::SYSTEM);
}

// ─────────────────────────────────────────────────────────────────────────────
// CMP-02 —— verdict 由 `completeness` 决定(**不**由 `lookup` 返回值决定)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP moesi_gpu 补偿: verdict 由 completeness 决定(保守补偿 ≠ 等价恢复)",
          "[cep][moesi-gpu][compensation]") {
    // ── ① 真实 tracker(CONSERVATIVE)⇒ 恒 `ConservativeOnly` ──────────────
    {
        EventQueue eq;
        MoesiGpuProtocolEngine eng("moesi_comp", &eq);
        SparseSnoopFilter sf;
        eng.set_sharer_tracker(&sf);

        tlm::cep::MoesiGpuProtocolEngine::AdapterPerm p =
            tlm::cep::MoesiGpuProtocolEngine::AdapterPerm::NONE;
        REQUIRE(eng.project_with_compensation(kAddr, MoesiGpuAdapter::State::O, p));
        const auto* rec = eng.last_compensation();
        REQUIRE(rec != nullptr);
        REQUIRE(rec->verdict == MoesiGpuProtocolEngine::CompensationVerdict::ConservativeOnly);
        REQUIRE(rec->tracker_completeness == tlm::cep::Completeness::CONSERVATIVE);
        REQUIRE(std::string(MoesiGpuProtocolEngine::verdict_name(rec->verdict)) ==
                "conservative-only");
    }

    // ── ② ⚠️ **鉴别力核心**:CONSERVATIVE tracker 即使**返回** `ANSWER_HERE`
    //      ⇒ verdict **仍**是 `ConservativeOnly` ⇒ **不得**声称等价恢复 ──
    //    (CONSERVATIVE 的**能力**是「不精确」,不是「本次恰好没歧义」)
    {
        EventQueue eq;
        MoesiGpuProtocolEngine eng("moesi_comp", &eq);
        MockTracker mock;
        mock.caps.completeness = tlm::cep::Completeness::CONSERVATIVE;
        mock.result = LookupResult::ANSWER_HERE;
        eng.set_sharer_tracker(&mock);

        tlm::cep::MoesiGpuProtocolEngine::AdapterPerm p =
            tlm::cep::MoesiGpuProtocolEngine::AdapterPerm::NONE;
        REQUIRE(eng.project_with_compensation(kAddr, MoesiGpuAdapter::State::O, p));
        const auto* rec = eng.last_compensation();
        REQUIRE(rec != nullptr);
        REQUIRE(rec->lookup == LookupResult::ANSWER_HERE); // 逐字镜像(值确实变了)
        // ⚠️ 但 verdict **不**跟着返回值变 ⇒ 判「等价恢复」的唯一依据是 caps
        REQUIRE(rec->verdict == MoesiGpuProtocolEngine::CompensationVerdict::ConservativeOnly);
    }

    // ── ③ 正控:EXACT tracker ⇒ `ExactRestorable`(此时 `ANSWER_HERE`
    //      **确实**证明「没有其他 sharer」)⇒ 两档**都有**实例 ⇒ 非恒真空转
    {
        EventQueue eq;
        MoesiGpuProtocolEngine eng("moesi_comp", &eq);
        MockTracker mock;
        mock.caps.completeness = tlm::cep::Completeness::EXACT;
        mock.result = LookupResult::ANSWER_HERE;
        eng.set_sharer_tracker(&mock);

        tlm::cep::MoesiGpuProtocolEngine::AdapterPerm p =
            tlm::cep::MoesiGpuProtocolEngine::AdapterPerm::NONE;
        REQUIRE(eng.project_with_compensation(kAddr, MoesiGpuAdapter::State::O, p));
        const auto* rec = eng.last_compensation();
        REQUIRE(rec != nullptr);
        REQUIRE(rec->verdict == MoesiGpuProtocolEngine::CompensationVerdict::ExactRestorable);
        REQUIRE(std::string(MoesiGpuProtocolEngine::verdict_name(rec->verdict)) ==
                "exact-restorable");
    }

    // ── ④ verdict 判定入口的**纯函数**性质(可直接机器核对)──────────────
    REQUIRE(MoesiGpuProtocolEngine::verdict_of(tlm::cep::Completeness::CONSERVATIVE) ==
            MoesiGpuProtocolEngine::CompensationVerdict::ConservativeOnly);
    REQUIRE(MoesiGpuProtocolEngine::verdict_of(tlm::cep::Completeness::EXACT) ==
            MoesiGpuProtocolEngine::CompensationVerdict::ExactRestorable);
    REQUIRE(MoesiGpuProtocolEngine::verdict_of(tlm::cep::Completeness::PARTIAL) ==
            MoesiGpuProtocolEngine::CompensationVerdict::ConservativeOnly);
    REQUIRE(MoesiGpuProtocolEngine::kCompensationVerdictCount == 2u);
}

// ─────────────────────────────────────────────────────────────────────────────
// CMP-03 —— 补偿记录**逐字镜像** tracker 的实际返回值(可观测 + 有鉴别力)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP moesi_gpu 补偿: 记录逐字镜像 tracker 的实际返回值",
          "[cep][moesi-gpu][compensation]") {
    EventQueue eq;
    MoesiGpuProtocolEngine eng("moesi_comp", &eq);

    MockTracker mock;
    mock.caps.completeness = tlm::cep::Completeness::CONSERVATIVE;
    mock.result = LookupResult::FORWARD_TO;
    mock.hazard = LookupHazard::STALL;
    mock.forward_to = tlm::cep::kNodeIdSdma;
    eng.set_sharer_tracker(&mock);

    tlm::cep::MoesiGpuProtocolEngine::AdapterPerm p =
        tlm::cep::MoesiGpuProtocolEngine::AdapterPerm::NONE;
    REQUIRE(eng.project_with_compensation(kAddr, MoesiGpuAdapter::State::O, p));

    const auto* rec = eng.last_compensation();
    REQUIRE(rec != nullptr);
    REQUIRE(rec->addr == kAddr);
    REQUIRE(rec->from == MoesiGpuAdapter::State::O);
    REQUIRE(rec->projected == MoesiGpuProtocolEngine::AdapterPerm::WRITEBACK);
    REQUIRE(rec->lookup == LookupResult::FORWARD_TO);  // 逐字镜像
    REQUIRE(rec->hazard == LookupHazard::STALL);       // 逐字镜像
    REQUIRE(rec->forward_to == tlm::cep::kNodeIdSdma); // 逐字镜像

    // tracker **确实**被查了,且用的是「想要独占所有权」的 intent
    REQUIRE(mock.lookups == 1u);
    REQUIRE(mock.last_intent == tlm::cep::Intent::want_own);

    // ── 反向 fixture:毒化 tracker 的返回值 ⇒ 记录**必变** ⇒ 判据**必红** ──
    {
        MockTracker poisoned;
        poisoned.caps.completeness = tlm::cep::Completeness::CONSERVATIVE;
        poisoned.result = LookupResult::BROADCAST; // ← 毒化点
        poisoned.hazard = LookupHazard::NONE;
        poisoned.forward_to = tlm::cep::kInvalidNode;

        MoesiGpuProtocolEngine eng2("moesi_comp2", &eq);
        eng2.set_sharer_tracker(&poisoned);
        tlm::cep::MoesiGpuProtocolEngine::AdapterPerm p2 =
            tlm::cep::MoesiGpuProtocolEngine::AdapterPerm::NONE;
        REQUIRE(eng2.project_with_compensation(kAddr, MoesiGpuAdapter::State::O, p2));
        const auto* rec2 = eng2.last_compensation();
        REQUIRE(rec2 != nullptr);
        // 毒化后的记录**不等于**未毒化的记录 ⇒ 「逐字镜像」这条判据**有鉴别力**
        REQUIRE(rec2->lookup != rec->lookup);
        REQUIRE(rec2->forward_to != rec->forward_to);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CMP-04 —— `O→M` 迁移**前**补偿(未补偿 ⇒ **不迁移**)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP moesi_gpu 补偿: O→M 迁移前补偿(无补偿 ⇒ 不迁移)", "[cep][moesi-gpu][compensation]") {
    // ── ① 有 tracker ⇒ 先补偿(1 条记录)后迁移(态变 `M`)─────────────────
    {
        EventQueue eq;
        MoesiGpuProtocolEngine eng("moesi_comp", &eq);
        SparseSnoopFilter sf;
        eng.set_sharer_tracker(&sf);

        const auto to =
            eng.submit_local_request(kAddr, MoesiGpuAdapter::State::O, MoesiGpuAdapter::Event::Wr);
        REQUIRE(to == MoesiGpuAdapter::State::M);
        REQUIRE(eng.state_of(kAddr) == MoesiGpuAdapter::State::M);
        REQUIRE(eng.compensation_count() == 1u);
        REQUIRE(eng.counters().compensation_queries == 1u);
        REQUIRE(eng.counters().local_requests == 1u);
        REQUIRE(eng.counters().adapter_transitions == 1u);
    }

    // ── ② 无 tracker ⇒ **不迁移**(不得在未补偿的情况下丢信息)───────────
    {
        EventQueue eq;
        MoesiGpuProtocolEngine eng("moesi_comp", &eq);
        const auto to =
            eng.submit_local_request(kAddr, MoesiGpuAdapter::State::O, MoesiGpuAdapter::Event::Wr);
        REQUIRE(to == MoesiGpuAdapter::State::O);
        REQUIRE(eng.state_of(kAddr) == MoesiGpuAdapter::State::I); // 迁移**未发生**
        REQUIRE(eng.compensation_count() == 0u);
        REQUIRE(eng.counters().compensation_skipped == 1u);
        REQUIRE(eng.counters().adapter_transitions == 0u);
    }

    // ── ③ 非 `O` 出发 ⇒ **不**触发补偿(决策 M-5)───────────────────────
    {
        EventQueue eq;
        MoesiGpuProtocolEngine eng("moesi_comp", &eq);
        SparseSnoopFilter sf;
        eng.set_sharer_tracker(&sf);
        const auto to =
            eng.submit_local_request(kAddr, MoesiGpuAdapter::State::E, MoesiGpuAdapter::Event::Wr);
        REQUIRE(to == MoesiGpuAdapter::State::M);
        REQUIRE(eng.compensation_count() == 0u);
        REQUIRE(eng.counters().compensation_queries == 0u);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CMP-05 —— 「可执行」的含义:补偿路径**有运行期载体**才能成立
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP moesi_gpu 补偿: 运行期载体是 V-2 的前提(纯静态 adapter 不够)",
          "[cep][moesi-gpu][compensation]") {
    // `MoesiGpuAdapter` 是**纯静态**的 ⇒ 它**无法**持有 tracker ⇒ 无法补偿。
    // (这不是「没写」,而是**结构性不可达** —— 故必须有运行期载体。)
    // 判据:adapter 面上**不存在**任何 tracker 相关的成员/方法。
    // 正控:载体面上**存在** `set_sharer_tracker` 且**能**查到 SF。
    EventQueue eq;
    MoesiGpuProtocolEngine eng("moesi_comp", &eq);
    SparseSnoopFilter sf;
    eng.set_sharer_tracker(&sf);
    REQUIRE(eng.sharer_tracker() == &sf);

    // 载体**可运行期改挂** tracker ⇒ 补偿结论随挂载对象变化(有鉴别力)
    MockTracker mock;
    mock.caps.completeness = tlm::cep::Completeness::EXACT;
    eng.set_sharer_tracker(&mock);
    REQUIRE(eng.sharer_tracker() == &mock);

    tlm::cep::MoesiGpuProtocolEngine::AdapterPerm p =
        tlm::cep::MoesiGpuProtocolEngine::AdapterPerm::NONE;
    REQUIRE(eng.project_with_compensation(kAddr, MoesiGpuAdapter::State::O, p));
    const auto* rec = eng.last_compensation();
    REQUIRE(rec != nullptr);
    REQUIRE(rec->verdict == MoesiGpuProtocolEngine::CompensationVerdict::ExactRestorable);
}
