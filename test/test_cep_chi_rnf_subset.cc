// test/test_cep_chi_rnf_subset.cc
// 队列 #8 `cpptlm-cep-chi-rnf-subset` —— CHI RN-F subset 的判据收口
//
// per openspec/changes/cpptlm-cep-chi-rnf-subset/tasks.md
//      · Phase 4(CR-10 ~ CR-14 + subset 排除项)—— 本文件先行落地
//      · Phase 6(CR-01 ~ CR-09)—— 追加到本文件
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ── 判据纪律(本文件的写法约束)──────────────────────────────────────
// ① **每条判据都有可执行反向 fixture**:要么「换成 Null 引擎」(结构上不可能满足),
//    要么「不驱动产生该通道的场景」(观测点必 0),且 fixture 断言的是**具体通道**
//    而不是「整体不全」(否则 fixture 可能因别的原因红)。
// ② **纯负向断言必带正控**:`CHECK_FALSE(x)` 之后必有同款驱动下的 `CHECK(x)` 对照,
//    否则「机制没跑起来」与「行为正确」不可分辨。
// ③ **判别谓词必须能区分真插件与 Null** —— 不能只断言「都绿」。
#include "catch_amalgamated.hpp"

#include "tlm/cep/chi_rnf_protocol_engine.hh"
#include "tlm/cep/cohif_protocol_engine.hh"

#include <cstddef>
#include <cstdint>
#include <string>

namespace {

using tlm::cep::CanonicalFlow;
using tlm::cep::ChiChannel;
using tlm::cep::ChiOpcode;
using tlm::cep::ChiRejectKind;
using tlm::cep::ChiRnfProtocolEngine;
using tlm::cep::CohIfNullProtocolEngine;
using tlm::cep::LineState;
using tlm::cep::Perm;
using tlm::cep::SnoopType;
using State = tlm::cep::ChiRnfAdapter::State;

constexpr std::size_t ch(ChiChannel c) { return static_cast<std::size_t>(c); }

/// 清空 egress 槽(单测里没有 adapter 搬运;`tick()` 的 adapter 分支是空操作)
template <typename PE> void drain_egress(PE& pe) {
    for (std::size_t p = 0; p < PE::kPortCount; ++p) {
        if (pe.resp_out()[p].valid())
            pe.resp_out()[p].clear_valid();
    }
}

/// egress 上当前有几个待发 Bundle
template <typename PE> std::size_t pending_bundles(PE& pe) {
    std::size_t n = 0;
    for (std::size_t p = 0; p < PE::kPortCount; ++p) {
        if (pe.resp_out()[p].valid())
            ++n;
    }
    return n;
}

/// 把一次 CHI `ReadShared` 送达引擎,返回 egress 上产生的 Bundle 数。
///
/// ⚠️ **这是 CR-10 的判别谓词**:真插件走 `submit_request`(CHI 侧 opcode 入口)
///    并**发射**一个 CohIf Request;Null 只能走 canonical `on_request`,而 Null 的
///    全部 `on_*` 把参数 `(void)` 丢弃、**零发射**
///    (per `protocol_engine_base.hh` 的 Null 段)⇒ 谓词值 1 vs 0。
template <typename PE> std::size_t deliver_read_bundles(PE& pe, std::uint64_t txn_id) {
    if constexpr (requires(PE& e) {
                      e.submit_request(ChiOpcode::ReadShared, std::uint64_t{0}, Perm::READ,
                                       std::uint64_t{64}, std::uint64_t{0});
                  }) {
        pe.submit_request(ChiOpcode::ReadShared, 0x1000, Perm::READ, 64, txn_id);
    } else {
        pe.on_request(0x1000, Perm::READ, txn_id);
    }
    return pending_bundles(pe);
}

/// 驱动 6 通道的**场景开关**(CR-11 的「屏蔽任一通道」fixture 用)
struct DrivePlan {
    bool txreq       = true;  ///< S1:`submit_request(ReadShared)` ⇒ TXREQ
    bool rxrsp       = true;  ///< S2:`on_completion(无 data)` ⇒ RXRSP
    bool rxdat       = true;  ///< S3:`on_completion(带 data)` ⇒ RXDAT
    bool snoop_clean = true;  ///< S4:snoop 未命中/干净行 ⇒ RXSNP + TXRSP
    bool snoop_dirty = true;  ///< S5:snoop 命中脏行 ⇒ RXSNP + TXDAT
};

/// 按 `plan` 驱动引擎(每个场景独立、互不依赖 ⇒ 屏蔽某场景只影响其通道)
void drive_all_channels(ChiRnfProtocolEngine& e, const DrivePlan& plan = {}) {
    if (plan.txreq) {  // S1
        e.submit_request(ChiOpcode::ReadShared, 0x1000, Perm::READ, 64, 1);
        drain_egress(e);
    }
    if (plan.rxrsp) {  // S2:无数据的 Comp
        e.on_completion(9001, 0, Perm::READ, false);
    }
    if (plan.rxdat) {  // S3:带数据的 CompData(不占 TXREQ —— 不发起请求)
        e.on_completion(9002, 0xABCD, Perm::READ, false);
    }
    if (plan.snoop_clean) {  // S4:未命中 ⇒ SnpResp I(TXRSP)
        e.on_snoop(0x9999, SnoopType::CLEAN, 7001);
        drain_egress(e);
    }
    if (plan.snoop_dirty) {  // S5:命中脏行 ⇒ CopyBack(TXDAT)
        e.set_line(0x7000, State::UD, 0xCAFE);
        e.on_snoop(0x7000, SnoopType::INVALIDATE, 7002);
        drain_egress(e);
    }
}

}  // namespace

// ══════════════════════════════════════════════════════════════════════
//  CR-10 —— 真插件驱动:Null **结构上不可能**有的行为(发射)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-10 真插件 vs Null:CHI 请求必须由真插件**发射**出去", "[cep][chi-rnf][cr-10]") {
    EventQueue eq;

    // ── 正控:真插件 ⇒ egress 上恰好 1 个 Bundle ──────────────────────
    ChiRnfProtocolEngine real("chi_real", &eq);
    CHECK(deliver_read_bundles(real, 11) == 1u);
    // ⇒ TXREQ 观测点同时被点亮(真插件既有发射、也有通道观测)
    CHECK(real.counters().observed[ch(ChiChannel::TXREQ)] == 1u);
    CHECK(real.counters().txreq_request == 1u);

    // ── 反向 fixture:换成 Null ⇒ **零发射**(判据必红)───────────────
    // Null 的全部 `on_*` 把参数 `(void)` 丢弃、不写任何 egress
    // ⇒ 「egress 上有 Bundle」这个谓词对 Null **结构上不可能**成立。
    CohIfNullProtocolEngine null_pe("chi_null", &eq);
    CHECK(deliver_read_bundles(null_pe, 11) == 0u);
    // 正控补强:Null **确实**收到了这次调用(不是「没跑到」)—— 它的流计数器 +1
    CHECK(null_pe.counters().per_flow[static_cast<std::size_t>(CanonicalFlow::Request)] == 1u);

    // ── 判据结论:同一谓词在两个引擎上**取值不同** ⇒ 有判别力 ──────────
    ChiRnfProtocolEngine real2("chi_real2", &eq);
    CohIfNullProtocolEngine null2("chi_null2", &eq);
    CHECK(deliver_read_bundles(real2, 12) != deliver_read_bundles(null2, 12));
}

// ══════════════════════════════════════════════════════════════════════
//  CR-11 —— 6 通道各有观测点 + TXREQ 双流承载
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-11 6 通道各有观测点 + TXREQ 双流承载", "[cep][chi-rnf][cr-11]") {
    EventQueue eq;
    ChiRnfProtocolEngine e("chi", &eq);

    // ── 正控:全场景驱动 ⇒ 6 通道全部点亮 ─────────────────────────────
    drive_all_channels(e);
    const auto& c = e.counters();
    for (std::size_t ch_i = 0; ch_i < tlm::cep::kChiChannelCount; ++ch_i) {
        INFO("channel " << tlm::cep::kChiChannelNames[ch_i] << " observed=" << c.observed[ch_i]);
        CHECK(c.observed[ch_i] >= 1u);
    }
    CHECK(e.all_channels_observed());

    // ── 子条件 ②:TXREQ 的**双流承载**必须被分别观测 ──────────────────
    // `Request`(读类)与 `Writeback`(WriteBackFull)走**同一通道**,靠 `flow_id` 区分。
    ChiRnfProtocolEngine e2("chi2", &eq);
    e2.set_line(0x4000, State::UD, 0x1234);
    e2.submit_request(ChiOpcode::ReadShared, 0x3000, Perm::READ, 64, 21);      // Request
    e2.submit_request(ChiOpcode::WriteBackFull, 0x4000, Perm::WRITEBACK, 64, 22);  // Writeback
    drain_egress(e2);
    CHECK(e2.counters().txreq_request == 1u);
    CHECK(e2.counters().txreq_writeback == 1u);
    // ⇒ 二者合计 = TXREQ 观测数(该通道没有被别的流借用)
    CHECK(e2.counters().observed[ch(ChiChannel::TXREQ)] == 2u);

    // ── 反向 fixture:屏蔽任一通道 ⇒ 该通道观测点必 0 + 汇聚判据必红 ────
    // ⚠️ `RXSNP` 的屏蔽必然同时屏蔽 `TXRSP` / `TXDAT`(snoop 是二者的**唯一**来源,
    //    这是协议事实而非夹具缺陷)⇒ 断言仍**精确钉在被屏蔽的那个通道**上。
    auto plan_without = [](ChiChannel target) {
        DrivePlan p;
        switch (target) {
            case ChiChannel::TXREQ: p.txreq = false; break;
            case ChiChannel::RXRSP: p.rxrsp = false; break;
            case ChiChannel::RXDAT: p.rxdat = false; break;
            case ChiChannel::RXSNP: p.snoop_clean = false; p.snoop_dirty = false; break;
            case ChiChannel::TXRSP: p.snoop_clean = false; break;
            case ChiChannel::TXDAT: p.snoop_dirty = false; break;
        }
        return p;
    };
    for (std::size_t ch_i = 0; ch_i < tlm::cep::kChiChannelCount; ++ch_i) {
        const auto target = static_cast<ChiChannel>(ch_i);
        ChiRnfProtocolEngine z("chi_masked", &eq);
        drive_all_channels(z, plan_without(target));
        INFO("masked channel " << tlm::cep::kChiChannelNames[ch_i]);
        CHECK(z.counters().observed[ch_i] == 0u);   // 该通道**确实**没被观测到
        CHECK_FALSE(z.all_channels_observed());     // ⇒ 汇聚判据必红
    }
}

// ══════════════════════════════════════════════════════════════════════
//  CR-12 —— Credit 流控:耗尽停发 / 补充恢复
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-12 Credit 耗尽停发 + 补充恢复", "[cep][chi-rnf][cr-12]") {
    EventQueue eq;

    // ── 耗尽态:0 初始 credit(**构造参数**,非测试开关)───────────────
    ChiRnfProtocolEngine starved("chi_starved", &eq, /*initial_credit=*/0);
    for (std::uint64_t i = 0; i < 3; ++i) {
        CHECK(starved.submit_request(ChiOpcode::ReadShared, 0x1000 + i, Perm::READ, 64, 100 + i));
        drain_egress(starved);
    }
    // 子条件 ①:发出计数**不再增长** + 三次都因 credit 停发 + 全部暂存
    CHECK(starved.counters().emitted[ch(ChiChannel::TXREQ)] == 0u);
    CHECK(starved.counters().credit_stalls == 3u);
    CHECK(starved.pending_egress() == 3u);
    // ⚠️ 停发**不影响**观测(协议事件仍发生,只是没能上线缆)⇒ 两个面必须分开
    CHECK(starved.counters().observed[ch(ChiChannel::TXREQ)] == 3u);

    // ── 子条件 ②:补充 1 个 credit ⇒ 恢复 1 个(正控)─────────────────
    starved.receive_credit(0, static_cast<unsigned>(ChiChannel::TXREQ));
    CHECK(starved.counters().credit_restored == 1u);
    starved.tick();  // drain_pending ⇒ 占空槽;adapter 缺省空操作 ⇒ 不再搬运
    drain_egress(starved);
    starved.tick();
    drain_egress(starved);
    CHECK(starved.counters().emitted[ch(ChiChannel::TXRSP)] == 0u);  // 无关通道不受影响
    CHECK(starved.counters().emitted[ch(ChiChannel::TXREQ)] == 1u);
    CHECK(starved.pending_egress() == 2u);

    // ── 反向 fixture:旁路 credit 记账 ⇒ 「停发」断言必红 ──────────────
    // 与耗尽态**同款驱动**,但 credit 充足 ⇒ 3 个全部发出。
    // ⇒ 若记账被旁路(门控不参与),耗尽态会得到 emitted=3 ⇒ 上面的 `== 0` 必红。
    ChiRnfProtocolEngine rich("chi_rich", &eq, /*initial_credit=*/4);
    for (std::uint64_t i = 0; i < 3; ++i) {
        CHECK(rich.submit_request(ChiOpcode::ReadShared, 0x1000 + i, Perm::READ, 64, 200 + i));
        drain_egress(rich);
    }
    CHECK(rich.counters().emitted[ch(ChiChannel::TXREQ)] == 3u);
    CHECK(rich.counters().credit_stalls == 0u);
    CHECK(rich.pending_egress() == 0u);
    CHECK(rich.counters().credit_consumed == 3u);

    // ── 越域 credit(防御性):`vc >= 6` ⇒ 计数而不涨 credit ──────────
    rich.receive_credit(0, 9);
    CHECK(rich.counters().credit_out_of_domain == 1u);
    CHECK(rich.credits(ChiChannel::TXREQ) == 4u - 3u);
}

// ══════════════════════════════════════════════════════════════════════
//  CR-13 —— TxnID:在途唯一 + 完成后可复用
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-13 TxnID 在途唯一 + 完成后释放复用", "[cep][chi-rnf][cr-13]") {
    EventQueue eq;
    ChiRnfProtocolEngine e("chi", &eq);

    // ── 子条件 ①:在途事务的 id 唯一 ─────────────────────────────────
    CHECK(e.submit_request(ChiOpcode::ReadShared, 0x1000, Perm::READ, 64, 42));
    CHECK(e.submit_request(ChiOpcode::ReadShared, 0x2000, Perm::READ, 64, 43));
    drain_egress(e);
    CHECK(e.in_flight_count() == 2u);
    CHECK(e.txn_in_flight(42));
    CHECK(e.txn_in_flight(43));

    // ── 反向 fixture:强制注入在途重复 id ⇒ 必被拒(唯一性有牙)────────
    CHECK_FALSE(e.submit_request(ChiOpcode::ReadShared, 0x3000, Perm::READ, 64, 42));
    CHECK(e.counters().txn_duplicate_rejected == 1u);
    CHECK(e.in_flight_count() == 2u);  // 重复注入**不**增长在途集(不是「接受后再报错」)
    // 正控:同一引擎用**新** id 仍可正常接受
    CHECK(e.submit_request(ChiOpcode::ReadShared, 0x3001, Perm::READ, 64, 44));
    CHECK(e.in_flight_count() == 3u);
    drain_egress(e);

    // ── 子条件 ②:完成后释放 ⇒ 可复用 ────────────────────────────────
    e.on_completion(42, 0, Perm::READ, false);  // 无数据 Comp ⇒ 结束(ReadShared 不需 CompAck)
    CHECK_FALSE(e.txn_in_flight(42));
    CHECK(e.counters().txn_released == 1u);
    CHECK(e.submit_request(ChiOpcode::ReadShared, 0x4000, Perm::READ, 64, 42));  // 复用
    CHECK(e.counters().txn_reused == 1u);
    CHECK(e.txn_in_flight(42));
    drain_egress(e);
}

// ══════════════════════════════════════════════════════════════════════
//  CR-14 —— CompAck 恰好 1 个 / 不需者零发送
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-14 CompAck 恰好 1 个 + 不需者零发送", "[cep][chi-rnf][cr-14]") {
    EventQueue eq;

    // ── 子条件 ①:需 CompAck 的读类事务在收到数据后**恰好** 1 个 ──────
    ChiRnfProtocolEngine e("chi", &eq);
    CHECK(e.submit_request(ChiOpcode::ReadUnique, 0x1000, Perm::UNIQUE, 64, 51));
    drain_egress(e);
    CHECK(e.counters().comp_ack_sent == 0u);  // 数据未回来 ⇒ 尚未发(不是「发早了」)
    e.on_completion(51, 0xAA55, Perm::UNIQUE, false);
    drain_egress(e);
    CHECK(e.counters().comp_ack_sent == 1u);
    // 重复完成(**反向 fixture**:若缺「恰好一次」的守卫,这里会变成 2)──────
    e.on_completion(51, 0xAA55, Perm::UNIQUE, false);
    drain_egress(e);
    CHECK(e.counters().comp_ack_sent == 1u);

    // ── 子条件 ②:不需 CompAck 的事务**零发送**(正控)─────────────────
    ChiRnfProtocolEngine n("chi_noack", &eq);
    CHECK(n.submit_request(ChiOpcode::ReadShared, 0x2000, Perm::READ, 64, 52));
    drain_egress(n);
    n.on_completion(52, 0xBB66, Perm::READ, false);
    drain_egress(n);
    CHECK(n.counters().comp_ack_sent == 0u);
    CHECK(n.counters().comp_ack_not_required == 1u);  // 该事务确实**走完了**数据完成

    // 写回类事务同样不发 CompAck(WriteBackFull 的完成是 Comp,无数据)
    ChiRnfProtocolEngine w("chi_wb", &eq);
    w.set_line(0x3000, State::UD, 0x1111);
    CHECK(w.submit_request(ChiOpcode::WriteBackFull, 0x3000, Perm::WRITEBACK, 64, 53));
    drain_egress(w);
    w.on_completion(53, 0, Perm::WRITEBACK, false);
    drain_egress(w);
    CHECK(w.counters().comp_ack_sent == 0u);
}

// ══════════════════════════════════════════════════════════════════════
//  subset 排除项(task 4.3)—— 注入即拒绝(静默收下 ⇒ 红)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("subset 排除项:非 CopyBack 写 / atomic / DVM / Retry 注入即拒",
          "[cep][chi-rnf][subset-exclusions]") {
    EventQueue eq;
    ChiRnfProtocolEngine e("chi", &eq);

    // ── 四类排除项逐个注入 ⇒ 全部拒绝 ────────────────────────────────
    CHECK_FALSE(e.submit_request(ChiOpcode::WriteUnique, 0x1000, Perm::UNIQUE, 64, 61));
    CHECK_FALSE(e.submit_request(ChiOpcode::WriteNoSnp, 0x1001, Perm::UNIQUE, 64, 62));
    CHECK_FALSE(e.submit_request(ChiOpcode::AtomicStore, 0x1002, Perm::UNIQUE, 8, 63));
    CHECK_FALSE(e.submit_request(ChiOpcode::AtomicLoad, 0x1003, Perm::READ, 8, 64));
    CHECK_FALSE(e.submit_request(ChiOpcode::AtomicSwap, 0x1004, Perm::UNIQUE, 8, 65));
    CHECK_FALSE(e.submit_request(ChiOpcode::DvmSync, 0x1005, Perm::READ, 8, 66));
    CHECK_FALSE(e.submit_request(ChiOpcode::DvmTlbi, 0x1006, Perm::READ, 8, 67));
    CHECK_FALSE(e.submit_request(ChiOpcode::RetryAck, 0x1007, Perm::READ, 8, 68));
    CHECK_FALSE(e.submit_request(ChiOpcode::PcrdGrant, 0x1008, Perm::READ, 8, 69));

    const auto& c = e.counters();
    CHECK(c.rejected[static_cast<std::size_t>(ChiRejectKind::NonCopyBackWrite)] == 2u);
    CHECK(c.rejected[static_cast<std::size_t>(ChiRejectKind::Atomic)] == 3u);
    CHECK(c.rejected[static_cast<std::size_t>(ChiRejectKind::Dvm)] == 2u);
    CHECK(c.rejected[static_cast<std::size_t>(ChiRejectKind::Retry)] == 2u);

    // ── 拒绝的**三条**可观测后果(缺任一条 ⇒ 「静默收下」会漏过判据)────
    CHECK(c.observed[ch(ChiChannel::TXREQ)] == 0u);  // ① 未产生通道事件
    CHECK(e.in_flight_count() == 0u);                // ② 未占用 TxnID
    CHECK(e.pending_egress() == 0u);                 // ③ 未进入发射队列

    // ── 正控:同引擎下白名单 opcode 仍被接受 ⇒ 拒绝是**分类**而非「全拒」──
    CHECK(e.submit_request(ChiOpcode::ReadShared, 0x2000, Perm::READ, 64, 70));
    CHECK(c.observed[ch(ChiChannel::TXREQ)] == 1u);
    CHECK(e.in_flight_count() == 1u);
    drain_egress(e);

    // ── 编译期表的**运行期复核**:排除项在表中确实 `accepted == false` ──
    for (std::size_t i = 0; i < tlm::cep::kChiOpcodeTableSize; ++i) {
        const auto& row = tlm::cep::kChiOpcodeTable[i];
        INFO("opcode " << row.name);
        CHECK(row.accepted == (row.reject == ChiRejectKind::None));
    }
    // 白名单的 16 个 opcode 必须全在表中且接受(与 `protocol_instances.json` 的
    // `atomic.supported=false` 口径一致 —— 后者由 Phase 6 的 CR-01/CR-02 断言)
    std::size_t accepted = 0;
    for (std::size_t i = 0; i < tlm::cep::kChiOpcodeTableSize; ++i) {
        if (tlm::cep::kChiOpcodeTable[i].accepted)
            ++accepted;
    }
    CHECK(accepted == 16u);
}
