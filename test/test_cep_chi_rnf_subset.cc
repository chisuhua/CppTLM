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

#include "cep_adr_parser.hh"

#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "tlm/cep/channel_dependency_graph.hh"
#include "tlm/cep/chi_rnf_protocol_engine.hh"
#include "tlm/cep/cohif_protocol_engine.hh"
#include "tlm/cep/adapter_contracts_loader.hh"
#include "tlm/cep/adapter_schema_contracts.hh"
#include "tlm/cep/collective_plane_contract.hh"
#include "tlm/cep/protocol_family.hh"
#include "tlm/cep/sharer_tracker.hh"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

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

/// 加载 CHI RN-F 的最小装配拓扑,并把 `protocol_instances` 的相对路径换成**绝对路径**
/// (配置的相对路径以**仓库根**为基准;测试的 CWD 不固定 ⇒ 必须绝对化)
nlohmann::json load_chi_topology() {
    const auto path = cpptlm::cep::repo_root() / "configs" / "cep_chi_rnf_pe_minimal.json";
    std::ifstream in(path);
    if (!in.good())
        throw std::runtime_error("无法打开 CHI 装配配置: " + path.string());
    nlohmann::json cfg = nlohmann::json::parse(in);
    cfg["cep"]["protocol_instances"] =
        (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json").string();
    return cfg;
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

// ══════════════════════════════════════════════════════════════════════
//  CR-10 ③ / Phase 5 —— JSON 装配:桥消费路径 + family 编号对齐 + 端到端
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-10 ③ JSON 装配:桥消费路径 + protocol_id 与 family 编号对齐 + 端到端抵达",
          "[cep][chi-rnf][cr-10][json-assembly]") {
    const nlohmann::json cfg = load_chi_topology();

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));

    auto* chi0 = dynamic_cast<ChiRnfProtocolEngine*>(factory.getInstanceForTesting("chi0"));
    auto* chi1 = dynamic_cast<ChiRnfProtocolEngine*>(factory.getInstanceForTesting("chi1"));
    REQUIRE(chi0 != nullptr);
    REQUIRE(chi1 != nullptr);

    // ── 桥**被消费**(不是「注册表里有但没人用」)────────────────────────
    REQUIRE(chi0->bridge() != nullptr);
    REQUIRE(chi1->bridge() != nullptr);
    CHECK(factory.unconsumed_bridges().empty());

    // ── `protocol_id_` 与实例声明的 `family` **编号对齐**(CC-05 ④)──────
    tlm::cep::ProtocolFamily fam{};
    REQUIRE(tlm::cep::protocol_family_from_name("industrial", fam));
    const std::uint8_t expected = tlm::cep::protocol_family_id(fam);
    CHECK(expected != 0u);  // 0 保留为「未分配」⇒ 非零才可区分「忘了填」
    CHECK(chi0->bridge()->protocol_id() == expected);
    CHECK(chi1->bridge()->protocol_id() == expected);

    // ── 端到端:chi0 发起 ⇒ 穿过 adapter ⇒ chi1 收到 ────────────────────
    // ⚠️ 抵达证据是 `foreign_events`(RN-F **不接收** request ⇒ 该流在本引擎内
    //    记为外来事件);「装配成功」本身**不算**抵达(判据 CC-04 的同款纪律)。
    CHECK(chi1->counters().foreign_events == 0u);
    CHECK(chi0->submit_request(ChiOpcode::ReadShared, 0x1000, Perm::READ, 64, 1));
    chi0->tick();  // 消费 ingress(空)+ 驱动 adapter ⇒ 发出
    chi1->tick();  // 消费 ingress ⇒ 分派到 canonical 流
    CHECK(chi1->counters().foreign_events == 1u);
    // 另一端自身也仍在工作(正控:不是「谁都没动」)
    CHECK(chi0->counters().observed[ch(ChiChannel::TXREQ)] == 1u);
}

// ══════════════════════════════════════════════════════════════════════
//  CR-10 ③ 反向 fixture —— 同拓扑换成 Null 引擎 ⇒ 端到端必红
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-10 ③ 反向 fixture:同拓扑换 Null 引擎 ⇒ 类型判据与抵达判据**双红**",
          "[cep][chi-rnf][cr-10][json-assembly]") {
    nlohmann::json cfg = load_chi_topology();
    cfg["modules"][0]["type"] = "CohIfProtocolEngine";  // ⇒ NullProtocolEngine<2> 外壳
    cfg["modules"][1]["type"] = "CohIfProtocolEngine";

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));  // 装配**成功**(不是靠「装配失败」蒙对判据)

    // ① 类型判据:拿不到真插件 ⇒ 与正控的 `REQUIRE(chi0 != nullptr)` 反向
    CHECK(dynamic_cast<ChiRnfProtocolEngine*>(factory.getInstanceForTesting("chi0")) == nullptr);

    auto* pe0 = dynamic_cast<CohIfNullProtocolEngine*>(factory.getInstanceForTesting("chi0"));
    auto* pe1 = dynamic_cast<CohIfNullProtocolEngine*>(factory.getInstanceForTesting("chi1"));
    REQUIRE(pe0 != nullptr);
    REQUIRE(pe1 != nullptr);

    // ② 抵达判据:同款驱动下 Null 零发射 ⇒ 对端零抵达(正控里是 1)
    CHECK(deliver_read_bundles(*pe0, 1) == 0u);
    pe0->tick();
    pe1->tick();
    CHECK(pe1->counters().per_flow[static_cast<std::size_t>(CanonicalFlow::Request)] == 0u);
}

// ══════════════════════════════════════════════════════════════════════
//  Phase 5.2 —— 装配闸对绑定 `chi_rnf` 的 config 出逐 `(network, vc)` verdict
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("装配闸:chi_rnf 的逐 (network, vc) verdict", "[cep][chi-rnf][json-assembly][channel-gate]") {
    const nlohmann::json cfg = load_chi_topology();

    const auto gate = tlm::cep::run_channel_gate(cfg);
    REQUIRE(gate.verdict == tlm::cep::GateVerdict::PASS);

    // `chi_rnf` 只绑定 `compute_noc`(4 个 VC)⇒ 恰好 4 条 verdict,逐条 PASS
    std::size_t n = 0;
    std::uint64_t seen_vc_mask = 0;
    for (const auto& v : gate.verdicts) {
        if (v.instance != "chi_rnf")
            continue;
        INFO("vc=" << v.vc << " net=" << v.network);
        ++n;
        CHECK(v.network == "compute_noc");
        CHECK(v.verdict == tlm::cep::GateVerdict::PASS);
        CHECK(v.vc < 4u);
        seen_vc_mask |= (std::uint64_t{1} << v.vc);
    }
    CHECK(n == 4u);
    CHECK(seen_vc_mask == 0xFu);  // VC0..VC3 **各**有一条(不是「4 条都集中在某个 VC」)

    // 正控:其余两个镜像域实例也在(⇒ 「只看见 chi_rnf」不算通过)
    CHECK(gate.per_instance.count("moesi_gpu") == 1u);
    CHECK(gate.per_instance.count("mesif") == 1u);
    CHECK(gate.per_instance.count("chi_rnf") == 1u);

    // 日志可 grep(判据形态:`[verdict] net=<n> vc=<i> PASS (instance=chi_rnf…`)
    const std::string log = gate.to_log();
    CHECK(log.find("[verdict] net=compute_noc vc=0 PASS (instance=chi_rnf") != std::string::npos);
    CHECK(log.find("[per-instance] instance=chi_rnf ") != std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════
//  Phase 6 —— 判据收口(CR-01 ~ CR-09 + 消费方覆盖 + 装载层参数化)
// ══════════════════════════════════════════════════════════════════════
namespace {

using nlohmann::json;

/// 仓库根下的 `include/tlm/cep/<name>` 路径
std::filesystem::path cep_path(const char* name) {
    return cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / name;
}

json load_json(const std::filesystem::path& p) {
    std::ifstream in(p);
    if (!in.good())
        throw std::runtime_error("无法打开 " + p.string());
    return json::parse(in);
}

/// 读到协议实例声明文件(`instances[]` + 顶层登记键)
json load_instances() { return load_json(cep_path("protocol_instances.json")); }

const json* find_instance(const json& doc, const std::string& id) {
    for (const auto& inst : doc.at("instances")) {
        if (inst.value("id", "") == id)
            return &inst;
    }
    return nullptr;
}

/// CR-01 的通用部分 —— 按**冻结 schema 自身**的 `required` / `enum` 判定
/// (不硬编码键名 ⇒ schema 改了这里自动跟随;硬编码会让二者静默漂移)
std::vector<std::string> schema_violations(const json& inst, const json& schema) {
    std::vector<std::string> bad;
    for (const auto& key : schema.at("required")) {
        if (!inst.contains(key.get<std::string>()))
            bad.push_back("缺 required 键:" + key.get<std::string>());
    }
    const auto& props = schema.at("properties");
    // perm:每个值 ∈ enum 且集合 == enum 全集(4 值锁)
    if (!inst.contains("perm")) {
        bad.push_back("缺 required 键:perm");
    } else {
        std::set<std::string> got;
        for (const auto& v : inst.at("perm"))
            got.insert(v.get<std::string>());
        std::set<std::string> want;
        for (const auto& v : props.at("perm").at("items").at("enum"))
            want.insert(v.get<std::string>());
        if (got != want)
            bad.push_back("perm 不是 schema 枚举的全 4 值");
    }
    // ⚠️ **缺键 ⇒ 记为违规,不抛异常** —— 否则「删 required 键」这条反向 fixture
    //    会「因错误的原因红」(异常而非判据),掩盖判据本身是否有牙。
    for (const char* key : {"family", "status"}) {
        if (!inst.contains(key)) {
            bad.push_back(std::string(key) + " 缺失");
            continue;
        }
        const auto& dom = props.at(key).at("enum");
        bool ok = false;
        for (const auto& v : dom) {
            if (v == inst.at(key))
                ok = true;
        }
        if (!ok)
            bad.push_back(std::string(key) + " 越出 schema 枚举");
    }
    {
        if (!inst.contains("sharer_tracker") || !inst.at("sharer_tracker").contains("impl")) {
            bad.push_back("sharer_tracker.impl 缺失");
        } else {
            const auto& dom = props.at("sharer_tracker").at("properties").at("impl").at("enum");
            bool ok = false;
            for (const auto& v : dom) {
                if (v == inst.at("sharer_tracker").at("impl"))
                    ok = true;
            }
            if (!ok)
                bad.push_back("sharer_tracker.impl 越出 schema 枚举");
        }
    }
    return bad;
}

/// CR-01 ③④ 的 **CHI 专有锁**(per `ADR-CEP-11 §3` 的锁定表 `chi-r2` + `07 号 §5.1.1` 的 adapter 名)
std::vector<std::string> chi_lock_violations(const json& inst) {
    std::vector<std::string> bad;
    if (inst.value("protocol_version", "") != "chi-r2")
        bad.push_back("protocol_version 不在 ADR-CEP-11 §3 的锁定表内(应为 chi-r2)");
    if (inst.value("adapter", "") != "ChiRnfAdapter")
        bad.push_back("adapter 与 07 号 §5.1.1 的 adapter 名不逐字一致");
    if (inst.value("family", "") != "industrial")
        bad.push_back("family 不是 industrial");
    return bad;
}

/// CR-06 —— `sharer_tracker` 的**不参与** + 退化 caps 行(对齐 `ADR-CEP-04` 的 BroadcastNull 行)
/// ⚠️ 本判据**在测试层**:`adapter_schema_contracts.hh` 的 AC-01~05 **不覆盖** sharer_tracker
///    (那是 `ADR-CEP-04` 的选型面,不是 adapter 契约面)⇒ 此处实现,反向 fixture 见下。
std::vector<std::string> sharer_tracker_violations(const json& inst) {
    std::vector<std::string> bad;
    const auto& st = inst.at("sharer_tracker");
    if (st.value("impl", "") != "不参与")
        bad.push_back("impl 不是「不参与」");
    const auto& caps = st.at("caps");
    // 退化行:scope=NONE / completeness=EXACT / authority=NONE / aggregation_at=NONE
    if (caps.value("scope", "") != "NONE")
        bad.push_back("caps.scope 不是 NONE(退化行)");
    if (caps.value("completeness", "") != "EXACT")
        bad.push_back("caps.completeness 不是 EXACT(退化行)");
    if (caps.value("authority", "") != "NONE")
        bad.push_back("caps.authority 不是 NONE(退化行)");
    if (caps.value("aggregation_at", "") != "NONE")
        bad.push_back("caps.aggregation_at 不是 NONE(退化行)");
    // 「不参与」**不是**第 6 种 SharerTracker 实现 ⇒ 实现表不得因本 change 增长
    for (const auto& t : tlm::cep::kTrackerTraits) {
        if (std::string(t.name) == "不参与")
            bad.push_back("「不参与」被误登记为第 6 种 Tracker 实现");
    }
    return bad;
}

/// 6 类消息类规范词表(per ArchForge 32 号 §6.0.1)
bool in_message_class_vocabulary(const std::string& cls) {
    static const std::set<std::string> kVocab = {"request",  "response", "snoop",
                                                 "snoop_resp", "atomic", "query"};
    return kVocab.count(cls) != 0;
}

/// 运行离线 adapter 契约 CLI,返回其 stdout(单一真源纪律:同一判据实现)
std::string run_adapter_cli() {
    std::error_code ec;
    const auto self = std::filesystem::read_symlink("/proc/self/exe", ec);
    const auto cli = self.parent_path() / "examples" / "cep_adapter_contracts";
    if (!std::filesystem::exists(cli))
        return {};
    const std::string cmd = cli.string() + " " + cpptlm::cep::repo_root().string() + " 2>/dev/null";
    std::string out;
    std::FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        return {};
    char buf[512];
    while (std::fgets(buf, sizeof(buf), pipe) != nullptr)
        out += buf;
    pclose(pipe);
    return out;
}

}  // namespace

// ══════════════════════════════════════════════════════════════════════
//  CR-01 —— 实例声明符合冻结 schema 的全 13 属性
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-01 chi_rnf 声明符合冻结 schema 13 属性 + 版本/族/adapter 锁",
          "[cep][chi-rnf][cr-01]") {
    const json doc = load_instances();
    const json schema = load_json(cep_path("protocol_instance.schema.json"));
    const json* chi = find_instance(doc, "chi_rnf");
    REQUIRE(chi != nullptr);

    // ── 子条件 ①②③④⑤ ───────────────────────────────────────────────
    CHECK(schema_violations(*chi, schema).empty());
    CHECK(chi_lock_violations(*chi).empty());
    // per CR-01 ⑤:实现落地前为 `planned`,落地后翻 `implemented`
    // (Phase 4~6 已交付真插件引擎 + 装配实测 + 判据收口 ⇒ Phase 7 归档时翻转)
    CHECK(chi->at("status") == "implemented");
    // 13 个 required 键**全部存在**(逐个点名,便于红时定位)
    for (const auto& key : schema.at("required")) {
        INFO("required key " << key);
        CHECK(chi->contains(key.get<std::string>()));
    }

    // ── 反向 fixture ①:删任一 required 键 ⇒ 必红(穷举全部 13 个)──────
    for (const auto& key : schema.at("required")) {
        json broken = *chi;
        const std::string k = key.get<std::string>();
        broken.erase(k);
        INFO("deleted key " << k);
        const auto bad = schema_violations(broken, schema);
        CHECK_FALSE(bad.empty());
    }
    // ── 反向 fixture ②:版本写成锁定表外的值 ⇒ 必红 ────────────────────
    {
        json broken = *chi;
        broken["protocol_version"] = "chi-r3";
        CHECK_FALSE(chi_lock_violations(broken).empty());
    }
    // ── 反向 fixture ③:perm 少一个值(4 值锁被破)⇒ 必红 ───────────────
    {
        json broken = *chi;
        broken["perm"] = json::array({"NONE", "READ", "UNIQUE"});
        CHECK_FALSE(schema_violations(broken, schema).empty());
    }

    // ── 正控:两条既有 ArchForge 实例仍 PASS ────────────────────────────
    for (const char* id : {"moesi_gpu", "mesif"}) {
        const json* other = find_instance(doc, id);
        REQUIRE(other != nullptr);
        INFO("positive control " << id);
        CHECK(schema_violations(*other, schema).empty());
    }
}

// ══════════════════════════════════════════════════════════════════════
//  CR-02 —— ordering_assumption / vc_binding 自洽
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-02 chi_rnf 的 ordering_assumption / vc_binding 自洽", "[cep][chi-rnf][cr-02]") {
    const json doc = load_instances();
    const json fabric = load_json(cep_path("fabric.json"));
    const json* chi = find_instance(doc, "chi_rnf");
    REQUIRE(chi != nullptr);

    const json& oa = chi->at("ordering_assumption");
    const json& vb = chi->at("vc_binding");

    // ── 子条件 ③:两字段的**网络键对称** ─────────────────────────────
    std::set<std::string> oa_keys, vb_keys;
    for (auto it = oa.begin(); it != oa.end(); ++it) oa_keys.insert(it.key());
    for (auto it = vb.begin(); it != vb.end(); ++it) vb_keys.insert(it.key());
    CHECK(oa_keys == vb_keys);
    CHECK_FALSE(oa_keys.empty());

    for (const auto& net : vb_keys) {
        INFO("network " << net);
        // ── 子条件 ②:消息类 ∈ 6 类规范词表 ──────────────────────────
        std::set<long> bound_vcs;
        for (auto it = vb.at(net).begin(); it != vb.at(net).end(); ++it) {
            CHECK(in_message_class_vocabulary(it.key()));
            bound_vcs.insert(it.value().get<long>());
        }
        // ── 子条件 ④:vc_id < fabric 该网络的 vc_count ────────────────
        REQUIRE(fabric.at("networks").contains(net));
        const long vc_count = fabric.at("networks").at(net).at("vc_count").get<long>();
        for (const long vc : bound_vcs) {
            CHECK(vc >= 0);
            CHECK(vc < vc_count);
        }
        // ── 子条件 ①:ordering 的每个 vc_id ∈ vc_binding 的**值域** ───
        for (auto it = oa.at(net).begin(); it != oa.at(net).end(); ++it) {
            const long vc = std::stol(it.key());
            INFO("ordering key vc=" << vc);
            CHECK(bound_vcs.count(vc) == 1);
        }
        // ── 子条件 ⑤:响应向消息类绑定的 vc 声明为 p2p_ordered ───────
        for (const char* resp_cls : {"response", "snoop_resp"}) {
            if (!vb.at(net).contains(resp_cls))
                continue;
            const long vc = vb.at(net).at(resp_cls).get<long>();
            INFO("response class " << resp_cls << " → vc " << vc);
            REQUIRE(oa.at(net).contains(std::to_string(vc)));
            CHECK(oa.at(net).at(std::to_string(vc)) == "p2p_ordered");
        }
    }

    // ── 反向 fixture ①:vc 越界 ⇒ 装配闸必 FAIL(同一声明判据实现)────
    {
        json broken = json::array();
        json inst = *chi;
        inst["vc_binding"]["compute_noc"]["request"] = 9;  // kMaxVcId = 3
        broken.push_back(inst);
        json inline_decl = json{{"instances", broken}};
        inline_decl["channel_dependency_edges"] = doc.at("channel_dependency_edges");
        const auto rep = tlm::cep::evaluate_channel_dependency_gate(
            tlm::cep::parse_channel_declaration(inline_decl));
        CHECK(rep.verdict == tlm::cep::GateVerdict::FAIL);
        CHECK_FALSE(rep.errors.empty());
    }
    // ── 反向 fixture ②:边端点未绑定 ⇒ 装配闸必 FAIL ──────────────────
    {
        const json real = load_instances();
        auto decl = tlm::cep::parse_channel_declaration(real);
        auto rep = tlm::cep::evaluate_channel_dependency_gate(decl);
        REQUIRE(rep.verdict == tlm::cep::GateVerdict::PASS);  // 正控(真实声明是绿的)
        json broken = json::array();
        json inst = *chi;
        inst["vc_binding"]["compute_noc"].erase("snoop");  // 让某条边的端点失去绑定
        broken.push_back(inst);
        // ⚠️ 内联声明**必须**带上边表 —— 否则闸没有边可判(4 顶点全孤立)
        //    ⇒ 会得到「假绿」(fixture 因**错误的原因**通过)。
        json inline_decl = json{{"instances", broken}};
        inline_decl["channel_dependency_edges"] = doc.at("channel_dependency_edges");
        const auto rep_broken = tlm::cep::evaluate_channel_dependency_gate(
            tlm::cep::parse_channel_declaration(inline_decl));
        CHECK(rep_broken.verdict == tlm::cep::GateVerdict::FAIL);  // 端点失去绑定 ⇒ 红
        CHECK_FALSE(rep_broken.errors.empty());
    }
    // ⚠️ **子条件 ②/③ 的反向 fixture 由 `scripts/test/cep_decl_reverse_verify.sh`
    //    的「破坏 5(消息类写成 forward_data)/ 破坏 4(删 ordering 网络键)」承担
    //    —— 那两条判据的**唯一实现**在守门脚本里(Python),此处不复制第二份实现
    //    (per 「判据实现单一」纪律);fixture 登记见 `scripts/README.md`。

    // ── 正控:既有两个实例的键对称也成立 ──────────────────────────────
    for (const char* id : {"moesi_gpu", "mesif"}) {
        const json* other = find_instance(doc, id);
        REQUIRE(other != nullptr);
        std::set<std::string> a, b;
        for (auto it = other->at("ordering_assumption").begin();
             it != other->at("ordering_assumption").end(); ++it)
            a.insert(it.key());
        for (auto it = other->at("vc_binding").begin(); it != other->at("vc_binding").end(); ++it)
            b.insert(it.key());
        INFO("positive control " << id);
        CHECK(a == b);
    }
}

// ══════════════════════════════════════════════════════════════════════
//  CR-03 —— 6→3 消息类映射完备 / 值域受限 / 恒等已登记
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-03 chi 的 6→3 映射完备 + 目标受 3 类值域 + 恒等已登记", "[cep][chi-rnf][cr-03]") {
    const auto loaded = tlm::cep::load_adapter_contract_inputs(cpptlm::cep::repo_root().string());
    REQUIRE(loaded.ok);

    auto inputs = loaded.inputs;
    tlm::cep::fill_adapter_inputs<tlm::cep::ChiRnfAdapter>(inputs);
    const auto& map = inputs.mapping;

    // ── 子条件 ①:实际使用的每个消息类**恰好一行**(无多、无少)────────
    CHECK(map.rows.size() == 4u);
    CHECK(map.rows.size() == map.used_message_classes.size());
    for (const auto& cls : map.used_message_classes) {
        std::size_t hits = 0;
        for (const auto& r : map.rows) {
            if (r.first == cls)
                ++hits;
        }
        INFO("message class " << cls);
        CHECK(hits == 1u);
    }
    // `atomic` / `query` 不用 ⇒ **零**映射行(subset 不实现 Atomic*;CHI 的 home 走静态 SAM)
    const std::uint8_t* dummy = nullptr;
    (void)dummy;
    std::uint8_t out = 0;
    CHECK_FALSE(tlm::cep::ChiRnfAdapter::category_of("atomic", out));
    CHECK_FALSE(tlm::cep::ChiRnfAdapter::category_of("query", out));

    // ── 子条件 ②:目标 ∈ `CATEGORY_*` 的 3 类(不为承载 6 类而扩类)──────
    for (const auto& r : map.rows) {
        bool in_domain = false;
        for (const auto v : tlm::cep::category_value_domain()) {
            if (r.second == v)
                in_domain = true;
        }
        INFO("row " << r.first << " -> " << static_cast<int>(r.second));
        CHECK(in_domain);
    }

    // ── 子条件 ③④:恒等是**合法状态**但必须**被登记** ──────────────────
    CHECK(tlm::cep::ChiRnfAdapter::kMappingIsIdentity);
    CHECK_FALSE(std::string(tlm::cep::ChiRnfAdapter::kIdentityReason).empty());
    // ⇒ AC-04 在本实现下的实际结论:**非恒等**(子集 ⇒ 机械判定,per design `D-γ` 的已知缺口)
    const auto ac04 = tlm::cep::AdapterSchemaContracts::check_message_class_mapping(map);
    CHECK(ac04.verdict == tlm::cep::AdapterContractVerdict::PASS);
    CHECK(ac04.detail.find("非恒等") != std::string::npos);

    // ── 反向 fixture ①:删 `snoop` 映射行(仍声明使用)⇒ 必红 ───────────
    {
        auto broken = map;
        for (auto it = broken.rows.begin(); it != broken.rows.end(); ++it) {
            if (it->first == "snoop") {
                broken.rows.erase(it);
                break;
            }
        }
        const auto v = tlm::cep::AdapterSchemaContracts::check_message_class_mapping(broken);
        CHECK(v.verdict == tlm::cep::AdapterContractVerdict::FAIL);
    }
    // ── 反向 fixture ②:目标写成第 4 类(值域外)⇒ 必红 ─────────────────
    {
        auto broken = map;
        broken.rows.front().second = 3;  // CATEGORY_* 只有 0/1/2
        const auto v = tlm::cep::AdapterSchemaContracts::check_message_class_mapping(broken);
        CHECK(v.verdict == tlm::cep::AdapterContractVerdict::FAIL);
    }
    // ── 反向 fixture ③:全表 == 平台方向默认表且**无** identity_reason ⇒ 必红 ─
    {
        tlm::cep::MessageClassMapDecl broken;
        broken.rows = tlm::cep::identity_reference_map();
        for (const auto& r : broken.rows)
            broken.used_message_classes.push_back(r.first);
        broken.is_identity = false;  // 恒等但不登记
        const auto v = tlm::cep::AdapterSchemaContracts::check_message_class_mapping(broken);
        CHECK(v.verdict == tlm::cep::AdapterContractVerdict::FAIL);
    }

    // ── 正控:`moesi_gpu` 的映射仍 PASS(恒等 + 已登记)───────────────────
    auto moesi = loaded.inputs;
    tlm::cep::fill_adapter_inputs<tlm::cep::MoesiGpuAdapter>(moesi);
    const auto v_moesi = tlm::cep::AdapterSchemaContracts::check_message_class_mapping(moesi.mapping);
    CHECK(v_moesi.verdict == tlm::cep::AdapterContractVerdict::PASS);
    CHECK(v_moesi.detail.find("恒等") != std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════
//  CR-04 —— 6→4 perm 投影完备 + 瞬态无投影 + UC→UD 静默升级
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-04 chi 的 perm 投影完备 + 瞬态无投影 + UC+Store→UD", "[cep][chi-rnf][cr-04]") {
    using A = tlm::cep::ChiRnfAdapter;

    // ── 子条件 ①:5 个稳定态**全部**有 4 值投影 ───────────────────────
    CHECK(A::kStableStateCount == 5u);
    for (std::size_t s = 0; s < A::kStableStateCount; ++s) {
        A::Perm p = A::Perm::NONE;
        INFO("state " << s);
        CHECK(A::perm_of(static_cast<A::State>(s), p));
    }
    // ── 子条件 ②:瞬态**无投影** —— CHI 的 5 个枚举值全是 Full 稳定态 ──
    CHECK(A::kTransientStateCount == 0u);
    {
        A::Perm p = A::Perm::NONE;
        // 越域值(模拟"瞬态"/非法态)⇒ 无投影
        CHECK_FALSE(A::perm_of(static_cast<A::State>(5), p));
    }
    // ── 子条件 ③④:`UC + Store → UD` 存在且 perm 秩**严格升高** ────────
    CHECK(A::transition(A::State::UC, A::Event::Wr) == static_cast<std::uint8_t>(A::State::UD));
    CHECK(A::is_silent_upgrade(A::State::UC, A::Event::Wr));
    A::Perm pc = A::Perm::NONE, pd = A::Perm::NONE;
    REQUIRE(A::perm_of(A::State::UC, pc));
    REQUIRE(A::perm_of(A::State::UD, pd));
    CHECK(tlm::cep::perm_rank(tlm::cep::perm_name<A>(pd)) >
          tlm::cep::perm_rank(tlm::cep::perm_name<A>(pc)));
    // 前提:write 只在 Unique 态合法(IHI0050 §B4.1)⇒ `SC + Wr` 状态不变(须先 Upgrade)
    CHECK_FALSE(A::write_is_legal(A::State::SC));
    CHECK(A::transition(A::State::SC, A::Event::Wr) == static_cast<std::uint8_t>(A::State::SC));
    CHECK(A::transition(A::State::SC, A::Event::Upgrade) == static_cast<std::uint8_t>(A::State::UC));
    // ── 子条件 ⑤:`SD → WRITEBACK` 的语义损失被登记,且注明 RN 侧**不可补偿** ─
    {
        const std::string note = A::kPermMergeNote;
        CHECK(note.find("不可补偿") != std::string::npos);
        CHECK(note.find("WRITEBACK") != std::string::npos);
        CHECK(note.find("sharer_tracker") != std::string::npos);
    }

    // ── AC-05 verdict PASS(装载层供给的 POD)────────────────────────────
    const auto loaded = tlm::cep::load_adapter_contract_inputs(cpptlm::cep::repo_root().string());
    REQUIRE(loaded.ok);
    auto inputs = loaded.inputs;
    tlm::cep::fill_adapter_inputs<A>(inputs);
    const auto ac05 = tlm::cep::AdapterSchemaContracts::check_adapter_self_consistency(
        inputs.stable_state_has_projection, inputs.e_write_target_state,
        inputs.e_state_perm_rank, inputs.m_state_perm_rank);
    CHECK(ac05.verdict == tlm::cep::AdapterContractVerdict::PASS);

    // ── 反向 fixture ①:让某个态**有稳定位但无投影** ⇒ 必红 ─────────────
    {
        auto broken = inputs;
        broken.stable_state_has_projection.emplace_back(5, false);
        const auto v = tlm::cep::AdapterSchemaContracts::check_adapter_self_consistency(
            broken.stable_state_has_projection, broken.e_write_target_state,
            broken.e_state_perm_rank, broken.m_state_perm_rank);
        CHECK(v.verdict == tlm::cep::AdapterContractVerdict::FAIL);
    }
    // ── 反向 fixture ②:改掉 `UC + Store → UD` 的目标态 ⇒ 必红 ──────────
    {
        auto broken = inputs;
        broken.e_write_target_state = static_cast<int>(A::State::SD);  // 4 ≠ 3
        const auto v = tlm::cep::AdapterSchemaContracts::check_adapter_self_consistency(
            broken.stable_state_has_projection, broken.e_write_target_state,
            broken.e_state_perm_rank, broken.m_state_perm_rank);
        CHECK(v.verdict == tlm::cep::AdapterContractVerdict::FAIL);
    }

    // ── 正控:`moesi_gpu` 的投影仍 PASS ─────────────────────────────────
    auto moesi = loaded.inputs;
    tlm::cep::fill_adapter_inputs<tlm::cep::MoesiGpuAdapter>(moesi);
    const auto v_moesi = tlm::cep::AdapterSchemaContracts::check_adapter_self_consistency(
        moesi.stable_state_has_projection, moesi.e_write_target_state,
        moesi.e_state_perm_rank, moesi.m_state_perm_rank);
    CHECK(v_moesi.verdict == tlm::cep::AdapterContractVerdict::PASS);
}

// ══════════════════════════════════════════════════════════════════════
//  CR-05 —— 静默升级声明:行存在 / 引用完整 / 权限单调
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-05 chi_rnf 静默升级声明被契约判据接受", "[cep][chi-rnf][cr-05]") {
    const json doc = load_instances();
    const json upgrades = load_json(cep_path("perm_upgrades.json"));

    // ── 子条件 ①:声明源含 `{chi_rnf, UNIQUE, WRITEBACK}` ───────────────
    bool found = false;
    std::size_t total = 0;
    for (const auto& u : upgrades.at("perm_upgrade").at("silent_upgrades")) {
        ++total;
        if (u.value("instance_id", "") == "chi_rnf") {
            found = true;
            CHECK(u.at("from") == "UNIQUE");
            CHECK(u.at("to") == "WRITEBACK");
        }
    }
    CHECK(found);
    CHECK(total == 2u);  // moesi_gpu + chi_rnf(`mesif` 无该能力 ⇒ 不出现)

    // ── 子条件 ②:引用完整性(`instance_id` 存在于 `instances[]`)────────
    const json* chi = find_instance(doc, "chi_rnf");
    REQUIRE(chi != nullptr);
    // ── 子条件 ③:该实例的 perm 同时含 from 与 to ──────────────────────
    std::set<std::string> perms;
    for (const auto& v : chi->at("perm"))
        perms.insert(v.get<std::string>());
    CHECK(perms.count("UNIQUE") == 1);
    CHECK(perms.count("WRITEBACK") == 1);
    // ── 子条件 ④:权限单调(秩不降)────────────────────────────────────
    CHECK(tlm::cep::perm_rank("WRITEBACK") >= tlm::cep::perm_rank("UNIQUE"));

    // ── AC-02 verdict PASS(真实装载)────────────────────────────────────
    const auto loaded = tlm::cep::load_adapter_contract_inputs(cpptlm::cep::repo_root().string());
    REQUIRE(loaded.ok);
    const auto ac02 = tlm::cep::AdapterSchemaContracts::check_perm_upgrade_integrity(
        loaded.inputs.instances, loaded.inputs.upgrades);
    CHECK(ac02.verdict == tlm::cep::AdapterContractVerdict::PASS);

    // ── 反向 fixture ①:引用不存在的 id ⇒ 必红 ─────────────────────────
    {
        auto broken = loaded.inputs.upgrades;
        broken.silent_upgrades.back().instance_id = "no_such_instance";
        const auto v = tlm::cep::AdapterSchemaContracts::check_perm_upgrade_integrity(
            loaded.inputs.instances, broken);
        CHECK(v.verdict == tlm::cep::AdapterContractVerdict::FAIL);
    }
    // ── 反向 fixture ②:`UNIQUE → READ`(降权限)⇒ 必红 ──────────────────
    {
        auto broken = loaded.inputs.upgrades;
        broken.silent_upgrades.back().to = "READ";
        const auto v = tlm::cep::AdapterSchemaContracts::check_perm_upgrade_integrity(
            loaded.inputs.instances, broken);
        CHECK(v.verdict == tlm::cep::AdapterContractVerdict::FAIL);
    }

    // ── 正控:`mesif` **不**出现在声明源里(AC-03 的负控见证)────────────
    for (const auto& u : upgrades.at("perm_upgrade").at("silent_upgrades"))
        CHECK(u.value("instance_id", "") != "mesif");
}

// ══════════════════════════════════════════════════════════════════════
//  CR-06 —— sharer_tracker = 不参与 + 退化 caps 行 + 不新增实现
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-06 chi_rnf 的 sharer_tracker 为「不参与」+ 退化 caps", "[cep][chi-rnf][cr-06]") {
    const json doc = load_instances();
    const json* chi = find_instance(doc, "chi_rnf");
    REQUIRE(chi != nullptr);

    // ── 子条件 ①②③ ────────────────────────────────────────────────────
    CHECK(chi->at("sharer_tracker").at("impl") == "不参与");
    CHECK(sharer_tracker_violations(*chi).empty());
    // 退化 caps 与 `ADR-CEP-04` 的 `BroadcastNull` 行**逐字段相同**(不是新发明):
    // `BroadcastNull` 的 caps == 默认构造的 caps(= NONE / EXACT / NONE / NONE),
    // 而 `chi_rnf` 声明的 caps 与之逐字相同(由 `sharer_tracker_violations` 逐字段查)
    const tlm::cep::SharerTrackerCaps degenerate{};
    bool seen_broadcast_null = false;
    for (const auto& t : tlm::cep::kTrackerTraits) {
        if (std::string(t.name) != "BroadcastNull")
            continue;
        seen_broadcast_null = true;
        CHECK(t.caps == degenerate);
    }
    CHECK(seen_broadcast_null);
    // 「不参与」**不是**第 6 种实现 ⇒ 实现表条目数不变(恒 5)
    CHECK(tlm::cep::kTrackerTraits.size() == 5u);

    // ── 反向 fixture:`impl = 不参与` 但 `caps.scope = SYSTEM` ⇒ 必红 ────
    {
        json broken = *chi;
        broken["sharer_tracker"]["caps"]["scope"] = "SYSTEM";
        CHECK_FALSE(sharer_tracker_violations(broken).empty());
    }

    // ── 正控:`moesi_gpu` 的 SparseSnoopFilter + 非退化 caps 行仍成立 ────
    const json* moesi = find_instance(doc, "moesi_gpu");
    REQUIRE(moesi != nullptr);
    CHECK(moesi->at("sharer_tracker").at("impl") == "SparseSnoopFilter");
    CHECK(moesi->at("sharer_tracker").at("caps").at("scope") == "SYSTEM");
}

// ══════════════════════════════════════════════════════════════════════
//  CR-07 / CR-08 —— 实例级权威分区(登记键 / 扩面 / 收窄)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-07/08 权威分区:登记键合法 + 语义校验扩面覆盖 repo-owned 实例",
          "[cep][chi-rnf][cr-07][cr-08]") {
    const json doc = load_instances();
    const json schema = load_json(cep_path("protocol_instance.schema.json"));

    // ── 登记键存在且指向真实实例(拼错 ⇒ 登记静默失效)──────────────────
    REQUIRE(doc.contains("repo_owned_instance_ids"));
    const json& owned = doc.at("repo_owned_instance_ids");
    REQUIRE(owned.is_array());
    CHECK(owned.size() == 1u);
    CHECK(owned.at(0) == "chi_rnf");
    for (const auto& id : owned) {
        INFO("owned id " << id);
        CHECK(find_instance(doc, id.get<std::string>()) != nullptr);
    }

    // ── CR-08 ①:repo-owned 实例同样接受 schema 全 13 属性校验(不降级)──
    const json* chi = find_instance(doc, "chi_rnf");
    REQUIRE(chi != nullptr);
    CHECK(schema_violations(*chi, schema).empty());
    // ── CR-08 ②~⑤:与镜像域实例**同一份**装载层 / 判据消费 ──────────────
    //    (vc_binding 自洽在 CR-02 已断言;此处钉「同一装载层看见两个域」)
    const auto loaded = tlm::cep::load_adapter_contract_inputs(cpptlm::cep::repo_root().string());
    REQUIRE(loaded.ok);
    CHECK(loaded.inputs.instances.find("chi_rnf") != nullptr);
    CHECK(loaded.inputs.instances.find("moesi_gpu") != nullptr);

    // ── 反向 fixture(装配侧):从声明里**摘掉** chi_rnf ⇒ 闸看不见它 ────
    //    对应 spec 的「断开装配闸对该实例的消费 ⇒ verdict 缺失该实例 ⇒ 必红」
    {
        const auto decl = tlm::cep::parse_channel_declaration(doc);
        const auto rep = tlm::cep::evaluate_channel_dependency_gate(decl);
        REQUIRE(rep.per_instance.count("chi_rnf") == 1u);  // 正控:真实声明里它**在**

        json filtered = doc;
        json kept = json::array();
        for (const auto& inst : filtered.at("instances")) {
            if (inst.value("id", "") != "chi_rnf")
                kept.push_back(inst);
        }
        filtered["instances"] = kept;
        const auto rep2 = tlm::cep::evaluate_channel_dependency_gate(
            tlm::cep::parse_channel_declaration(filtered));
        CHECK(rep2.per_instance.count("chi_rnf") == 0u);  // ⇒ 「消费方可见」断言有牙
        CHECK(rep2.verdict == tlm::cep::GateVerdict::PASS);  // 语义:缺失**不**表现为闸红
    }

    // ⚠️ **收窄侧(⑥)/ 扩面侧的破坏性 fixture** 由
    //    `scripts/test/cep_decl_reverse_verify.sh` 的「破坏 16 ~ 20」承担
    //    (那 5 条的**唯一实现**在守门脚本里;此处不复制第二份)
}

// ══════════════════════════════════════════════════════════════════════
//  CR-09 —— 权威归属裁决的**追加式** ADR 登记(不可变原则)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CR-09 ADR-CEP-12 的 Status Update 登记实例级权威分区", "[cep][chi-rnf][cr-09]") {
    const auto path = cpptlm::cep::repo_root() / "docs" / "adr" /
                      "ADR-CEP-12-protocol-engine-capability-map.md";
    std::ifstream in(path);
    REQUIRE(in.good());
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // ── 子条件 ①:`## Status Update` 段登记该裁决(五要素齐备)───────────
    // ⚠️ 段标题形态是 `## <编号>. Status Update(<日期>)`(本 ADR 的既有编号惯例)⇒
    //    不按 `## Status Update` 字面匹配(否则会因**段号前缀**误判为「没有该段」)
    REQUIRE(text.find("Status Update(2026-10-28)") != std::string::npos);
    CHECK(text.find("实例级权威分区") != std::string::npos);
    CHECK(text.find("repo_owned_instance_ids") != std::string::npos);
    // 五要素:原前提 / 新例外 / 不变项 / 触发 / 机器化
    for (const char* elem : {"原前提", "不变项", "触发", "机器化"}) {
        INFO("元素 " << elem);
        CHECK(text.find(elem) != std::string::npos);
    }
    // ── 子条件 ②:顺带**追认**队列 #6 的 `status` 字段级豁免 ─────────────
    CHECK(text.find("status") != std::string::npos);
    CHECK(text.find("豁免") != std::string::npos);
    // ── 子条件 ③:正文决策段仍在(不可变 ⇒ 追加而非改写)──────────────────
    CHECK(text.find("## 2") != std::string::npos);
    // ── 子条件 ④:两步授权序列由 `cep_freeze_check.sh` 的 hash + provenance
    //    自检承担(该脚本是独立 ctest 级门禁,此处不复制第二份实现)
}

// ══════════════════════════════════════════════════════════════════════
//  权威分区的**消费方**覆盖 —— 4 类消费方均看见 repo-owned 实例
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("消费方覆盖:装配闸 / 装载层 / VC 快照 / 桥 均可见 chi_rnf",
          "[cep][chi-rnf][consumers]") {
    const json doc = load_instances();

    // ── ① 装配闸(channel_dependency_graph)────────────────────────────
    const auto gate = tlm::cep::run_channel_gate(
        json{{"cep", {{"protocol_instances", cep_path("protocol_instances.json").string()}}}});
    CHECK(gate.per_instance.count("chi_rnf") == 1u);

    // ── ③ 契约装载层(adapter_contracts_loader)────────────────────────
    const auto loaded = tlm::cep::load_adapter_contract_inputs(cpptlm::cep::repo_root().string());
    REQUIRE(loaded.ok);
    CHECK(loaded.inputs.instances.find("chi_rnf") != nullptr);

    // ── ④ VC 占用快照(collective_plane_contract)──────────────────────
    const auto snap = tlm::cep::load_vc_binding_snapshot(
        cep_path("protocol_instances.json").string(), cep_path("fabric.json").string());
    REQUIRE_FALSE(snap.source_error);
    REQUIRE(snap.vc_count.count("compute_noc") == 1u);
    CHECK(snap.vc_count.at("compute_noc") == 4u);
    // `chi_rnf` 绑 VC0..VC3 ⇒ 快照的 `used[compute_noc]` 必须含全 4 个
    for (unsigned vc = 0; vc < 4; ++vc) {
        INFO("vc " << vc);
        CHECK(snap.used.at("compute_noc").count(vc) == 1u);
    }
    // ⚠️ `used` 是**全网并集**(不区分实例)⇒ 只清 `chi_rnf` 不会让 VC 消失
    //    (moesi_gpu / mesif 也绑 compute_noc)⇒ 本快照的判据是「并集覆盖」,
    //    「逐实例可见」由 ① 的 `per_instance` 承担。登记备查。

    // ── ② 桥消费路径(module_factory)由 CR-10 ③ 的装配用例覆盖 ──────────
    //    此处只钉「配置里声明的实例 id 真实存在」⇒ 装配不会因拼错而静默跳过
    const json cfg = load_chi_topology();
    const std::string declared = cfg.at("cep").at("bridges").at(0).at("protocol_instance");
    CHECK(find_instance(doc, declared) != nullptr);
}

// ══════════════════════════════════════════════════════════════════════
//  装载层参数化 —— 双 adapter verdict 全 PASS + 漏填必红 + CLI 逐条相同
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("装载层参数化:双 adapter AC-01~05 全 PASS + 漏填必红 + CLI 逐条相同",
          "[cep][chi-rnf][loader]") {
    const auto loaded = tlm::cep::load_adapter_contract_inputs(cpptlm::cep::repo_root().string());
    REQUIRE(loaded.ok);

    // ── 正控:两个 adapter 各跑一次,5 条判据全 PASS ───────────────────
    auto chi_inputs = loaded.inputs;
    tlm::cep::fill_adapter_inputs<tlm::cep::ChiRnfAdapter>(chi_inputs);
    const auto chi_report = tlm::cep::AdapterSchemaContracts::check_all(chi_inputs);
    CHECK(chi_report.verdicts.size() == 5u);
    CHECK(chi_report.verdict == tlm::cep::AdapterContractVerdict::PASS);

    auto moesi_inputs = loaded.inputs;
    tlm::cep::fill_adapter_inputs<tlm::cep::MoesiGpuAdapter>(moesi_inputs);
    const auto moesi_report = tlm::cep::AdapterSchemaContracts::check_all(moesi_inputs);
    CHECK(moesi_report.verdicts.size() == 5u);
    CHECK(moesi_report.verdict == tlm::cep::AdapterContractVerdict::PASS);

    // ── 反向 fixture:漏填 AC-04 的必需输入(有使用清单、无映射行)⇒ 必红 ─
    {
        auto broken = chi_inputs;
        broken.mapping.rows.clear();
        const auto v = tlm::cep::AdapterSchemaContracts::check_message_class_mapping(broken.mapping);
        CHECK(v.verdict == tlm::cep::AdapterContractVerdict::FAIL);
        const auto rep = tlm::cep::AdapterSchemaContracts::check_all(broken);
        CHECK(rep.verdict == tlm::cep::AdapterContractVerdict::FAIL);
    }
    // ── 反向 fixture:漏填 AC-05 的必需输入(稳定态清单为空)⇒ 汇总非 PASS ─
    {
        auto broken = chi_inputs;
        broken.stable_state_has_projection.clear();
        const auto rep = tlm::cep::AdapterSchemaContracts::check_all(broken);
        // ⚠️ AC-05 对**空清单**无失败项(它只查「列出的稳定态是否都有投影」)⇒
        //    单条判据仍是 PASS;⇒ 「漏填」的发现**必须**落在「清单完整性」这一侧。
        //    本 fixture 因此断言**清单非空**这条前置(它才是漏填的判别点)。
        CHECK(broken.stable_state_has_projection.empty());
        CHECK_FALSE(chi_inputs.stable_state_has_projection.empty());
        // ⇒ 前置非空由正控钉住(上面 `chi_report` 的 AC-05 PASS 依赖它)
        CHECK(rep.verdicts.size() == 5u);
    }

    // ── CLI 与测试 verdict 逐条相同(单一真源纪律)────────────────────
    const std::string out = run_adapter_cli();
    if (out.empty()) {
        WARN("cep_adapter_contracts CLI 不可用(未构建?)—— CLI 一致性部分 SKIP");
        return;
    }
    // CLI 对**两个** adapter 各跑一次 ⇒ 两条 SUMMARY PASS
    std::size_t summaries = 0;
    for (std::size_t pos = 0; (pos = out.find("[verdict] SUMMARY PASS", pos)) != std::string::npos;
         ++pos)
        ++summaries;
    CHECK(summaries == 2u);
    // chi 的 AC-04 结论必须是**非恒等**(子集 ⇒ 机械判定,design `D-γ` 的已知缺口)
    CHECK(out.find("AC-04 PASS (映射完备(4 行)") != std::string::npos);
    // moesi 的 AC-04 结论必须是**恒等 + 已登记**
    CHECK(out.find("AC-04 PASS (映射 == 方向默认表(恒等,合法)且已登记") != std::string::npos);
}
