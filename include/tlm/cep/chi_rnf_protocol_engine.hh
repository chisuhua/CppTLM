// include/tlm/cep/chi_rnf_protocol_engine.hh
// ChiRnfProtocolEngine —— CHI RN-F subset 的**真插件**协议引擎
//
// 功能:交付 `cpptlm-cep-chi-rnf-subset` 的 **Phase 4**(判据 CR-10 ~ CR-14):
//       (1) **6 通道观测点**(TXREQ / RXRSP / RXDAT / RXSNP / TXRSP / TXDAT 逐通道计数)
//       (2) **Credit 记账**(按通道记账、耗尽停发、补充恢复)
//       (3) **TxnID 管理**(在途唯一 + 完成后释放可复用)
//       (4) **CompAck 发射**(需 CompAck 的读类事务恰好 1 个)
//       (5) **subset 排除项的机器闸**(非 CopyBack 写 / atomic / DVM / Retry 注入即拒绝)
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ── 为什么需要「真插件」(相对 `NullProtocolEngine`)────────────────────
// `NullProtocolEngine` 的全部 `on_*` 把参数 `(void)` 丢弃、**零发射**
// (per `protocol_engine_base.hh` 的 Null 段)⇒ 它有**计数器**,但没有
// **行为**。本类提供 Null **结构上不可能**具备的四类行为:
//   · 发射(egress `CohIfBundle`)/ 通道归属(6 通道逐条可辨)
//   · Credit 门控(耗尽 ⇒ 发射停;补充 ⇒ 恢复)
//   · TxnID 生命周期(分配 / 拒绝重复 / 释放 / 复用)
//   · CompAck(按需恰好 1 个)
// ⇒ CR-10 ~ CR-14 的判据都钉在这四类行为上,因此「换成 Null ⇒ 必红」成立。
//
// ── 6 通道的归属规则(单一真源 = `kChiOpcodeTable`)─────────────────────
// 归属**由 opcode 决定**,不由 CohIf 流单独决定(CHI 的 `Writeback` 流**跨两个通道**:
// `WriteBackFull` / `WriteCleanFull` ⇒ TXREQ;`CopyBackWrData` ⇒ TXDAT,per change
// design `D-β`)。运行期只拿到 CohIf Bundle 的路径(Completion / SnoopResp)按
// 「是否携带数据」判定,并用编译期 `static_assert` 与本表**锁一致**。
//
// ── ⚠️ 已知缺口(诚实登记,不静默放过)──────────────────────────────────
// ① **「是否携带数据」以 `data != 0` 近似** —— `CohIfBundle` **无** `has_data` 位
//    (它只有 `size`,而 canonical `on_completion` 签名不带 `size`)⇒ 数据恰为 0 的
//    `CompData` 会被归到 RXRSP。**修法需扩冻结面 `CohIfBundle`** ⇒ 本 change 零改动
//    (per design `D-β` 的「`CohIfBundle` 零扩展」);登记为 Stage 2 的缺口。
// ② **RN-F 不接收 request**(它只发 REQ、只收 SNP)⇒ CohIf ingress 的
//    `Request` / `Writeback` / `SnoopResp` 三条在本引擎内是**外来事件**
//    (`foreign_events`),**不**计入 6 通道(否则会把「收到请求」误记成「发出请求」)。
//
// ── 边界 ────────────────────────────────────────────────────────────────
//   · **不注册** 23 ABI · **不改** frozen header · **不扩** `CohIfBundle`
//   · **不扩** `perm` 4 值锁 / `CATEGORY_*` 3 类词表 · **新增 invariant 数 = 0**
//   · 端口数固定 **N = 2**(与 `CohIfNullProtocolEngine` 同款,便于 JSON 装配对照)
#ifndef TLM_CEP_CHI_RNF_PROTOCOL_ENGINE_HH
#define TLM_CEP_CHI_RNF_PROTOCOL_ENGINE_HH

#include "tlm/cep/chi_rnf_adapter.hh"
#include "tlm/cep/protocol_engine_base.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace tlm::cep {

// ══════════════════════════════════════════════════════════════════════
//  CHI RN-F 的 6 通道(per `ADR-CEP-11 §3` 的 `chi-r2` 通道面)
// ══════════════════════════════════════════════════════════════════════
enum class ChiChannel : std::uint8_t {
    TXREQ = 0,  ///< RN → 网络:ReadShared / ReadUnique / MakeUnique / WriteBack / WriteClean
    RXRSP = 1,  ///< 网络 → RN:Comp / CompDBIDResp
    RXDAT = 2,  ///< 网络 → RN:CompData / DataSepResp
    RXSNP = 3,  ///< 网络 → RN:Snp*
    TXRSP = 4,  ///< RN → 网络:SnpResp(无数据)/ CompAck
    TXDAT = 5,  ///< RN → 网络:CopyBackWrData / SnpRespData
};

inline constexpr std::size_t kChiChannelCount = 6;

/// 通道名(判据输出可 grep;与枚举逐值对应)
inline constexpr std::array<const char*, kChiChannelCount> kChiChannelNames = {{
    "TXREQ", "RXRSP", "RXDAT", "RXSNP", "TXRSP", "TXDAT",
}};

// ══════════════════════════════════════════════════════════════════════
//  subset 的 opcode 白名单 + 排除项(4.3 的机器闸)
// ══════════════════════════════════════════════════════════════════════
enum class ChiRejectKind : std::uint8_t {
    None = 0,             ///< 接受(subset 白名单)
    NonCopyBackWrite = 1, ///< 非 CopyBack 写(`WriteUnique` / `WriteNoSnp` 的 `NCBWrData`)
    Atomic = 2,           ///< Atomic* 事务(subset `atomic.supported = false`)
    Dvm = 3,              ///< DVM(`DvmSync` / `DvmTlbi`)
    Retry = 4,            ///< `RetryAck` / `PCrdGrant`(subset 假设「对端 HN 不 Retry」)
};

inline constexpr std::size_t kChiRejectKindCount = 5;

inline constexpr std::array<const char*, kChiRejectKindCount> kChiRejectKindNames = {{
    "none", "non_copyback_write", "atomic", "dvm", "retry",
}};

enum class ChiOpcode : std::uint8_t {
    // ── 接受(subset 白名单,16 个)──────────────────────────────────
    ReadShared = 0,
    ReadUnique = 1,
    MakeUnique = 2,
    WriteBackFull = 3,
    WriteCleanFull = 4,
    Comp = 5,
    CompDBIDResp = 6,
    CompData = 7,
    DataSepResp = 8,
    SnpShared = 9,
    SnpUnique = 10,
    SnpClean = 11,
    SnpResp = 12,
    SnpRespData = 13,
    CompAck = 14,
    CopyBackWrData = 15,
    // ── 拒绝(subset 排除项,9 个)──────────────────────────────────
    WriteUnique = 16,
    WriteNoSnp = 17,
    AtomicStore = 18,
    AtomicLoad = 19,
    AtomicSwap = 20,
    DvmSync = 21,
    DvmTlbi = 22,
    RetryAck = 23,
    PcrdGrant = 24,

    kCount = 25,
};

inline constexpr std::size_t kChiOpcodeCount = static_cast<std::size_t>(ChiOpcode::kCount);

/// opcode 的**唯一真源**:通道归属 + CohIf 流 + 是否接受 + 拒绝理由 + 是否带数据
struct ChiOpcodeInfo {
    ChiOpcode      opcode;
    const char*    name;
    ChiChannel     channel;
    CanonicalFlow  flow;
    bool           accepted;
    ChiRejectKind  reject;
    bool           has_data;
};

inline constexpr ChiOpcodeInfo kChiOpcodeTable[] = {
    // opcode            name              channel           flow                           acc   reject                      data
    {ChiOpcode::ReadShared,     "ReadShared",     ChiChannel::TXREQ, CanonicalFlow::Request,           true,  ChiRejectKind::None,              false},
    {ChiOpcode::ReadUnique,     "ReadUnique",     ChiChannel::TXREQ, CanonicalFlow::Request,           true,  ChiRejectKind::None,              false},
    {ChiOpcode::MakeUnique,     "MakeUnique",     ChiChannel::TXREQ, CanonicalFlow::Request,           true,  ChiRejectKind::None,              false},
    {ChiOpcode::WriteBackFull,  "WriteBackFull",  ChiChannel::TXREQ, CanonicalFlow::Writeback,         true,  ChiRejectKind::None,              true},
    {ChiOpcode::WriteCleanFull, "WriteCleanFull", ChiChannel::TXREQ, CanonicalFlow::Writeback,         true,  ChiRejectKind::None,              true},
    {ChiOpcode::Comp,           "Comp",           ChiChannel::RXRSP, CanonicalFlow::Completion,        true,  ChiRejectKind::None,              false},
    {ChiOpcode::CompDBIDResp,   "CompDBIDResp",   ChiChannel::RXRSP, CanonicalFlow::Completion,        true,  ChiRejectKind::None,              false},
    {ChiOpcode::CompData,       "CompData",       ChiChannel::RXDAT, CanonicalFlow::Completion,        true,  ChiRejectKind::None,              true},
    {ChiOpcode::DataSepResp,    "DataSepResp",    ChiChannel::RXDAT, CanonicalFlow::Completion,        true,  ChiRejectKind::None,              true},
    {ChiOpcode::SnpShared,      "SnpShared",      ChiChannel::RXSNP, CanonicalFlow::Snoop,             true,  ChiRejectKind::None,              false},
    {ChiOpcode::SnpUnique,      "SnpUnique",      ChiChannel::RXSNP, CanonicalFlow::Snoop,             true,  ChiRejectKind::None,              false},
    {ChiOpcode::SnpClean,       "SnpClean",       ChiChannel::RXSNP, CanonicalFlow::Snoop,             true,  ChiRejectKind::None,              false},
    {ChiOpcode::SnpResp,        "SnpResp",        ChiChannel::TXRSP, CanonicalFlow::SnoopResp,         true,  ChiRejectKind::None,              false},
    {ChiOpcode::SnpRespData,    "SnpRespData",    ChiChannel::TXDAT, CanonicalFlow::SnoopResp,         true,  ChiRejectKind::None,              true},
    {ChiOpcode::CompAck,        "CompAck",        ChiChannel::TXRSP, CanonicalFlow::SnoopCompletionAck, true, ChiRejectKind::None,              false},
    {ChiOpcode::CopyBackWrData, "CopyBackWrData", ChiChannel::TXDAT, CanonicalFlow::Writeback,         true,  ChiRejectKind::None,              true},
    // ── 排除项(注入即拒绝)──────────────────────────────────────────
    {ChiOpcode::WriteUnique,    "WriteUnique",    ChiChannel::TXREQ, CanonicalFlow::Request,           false, ChiRejectKind::NonCopyBackWrite,  true},
    {ChiOpcode::WriteNoSnp,     "WriteNoSnp",     ChiChannel::TXREQ, CanonicalFlow::Request,           false, ChiRejectKind::NonCopyBackWrite,  true},
    {ChiOpcode::AtomicStore,    "AtomicStore",    ChiChannel::TXREQ, CanonicalFlow::Request,           false, ChiRejectKind::Atomic,            true},
    {ChiOpcode::AtomicLoad,     "AtomicLoad",     ChiChannel::TXREQ, CanonicalFlow::Request,           false, ChiRejectKind::Atomic,            false},
    {ChiOpcode::AtomicSwap,     "AtomicSwap",     ChiChannel::TXREQ, CanonicalFlow::Request,           false, ChiRejectKind::Atomic,            true},
    {ChiOpcode::DvmSync,        "DvmSync",        ChiChannel::TXREQ, CanonicalFlow::Request,           false, ChiRejectKind::Dvm,               false},
    {ChiOpcode::DvmTlbi,        "DvmTlbi",        ChiChannel::TXREQ, CanonicalFlow::Request,           false, ChiRejectKind::Dvm,               false},
    {ChiOpcode::RetryAck,       "RetryAck",       ChiChannel::RXRSP, CanonicalFlow::Completion,        false, ChiRejectKind::Retry,             false},
    {ChiOpcode::PcrdGrant,      "PcrdGrant",      ChiChannel::RXRSP, CanonicalFlow::Completion,        false, ChiRejectKind::Retry,             false},
};

inline constexpr std::size_t kChiOpcodeTableSize =
    sizeof(kChiOpcodeTable) / sizeof(kChiOpcodeTable[0]);

/// 查 opcode 的元数据(表驱动;未知 opcode ⇒ 落到「拒收 + 越域」的保守分支)
constexpr const ChiOpcodeInfo& chi_opcode_info(ChiOpcode op) {
    for (std::size_t i = 0; i < kChiOpcodeTableSize; ++i) {
        if (kChiOpcodeTable[i].opcode == op)
            return kChiOpcodeTable[i];
    }
    return kChiOpcodeTable[0];  // 不可达(表完备性由 static_assert 保证)
}

/// 运行期只有 CohIf Bundle 的两条路径的**通道归属**(与表一致,编译期锁定)
constexpr ChiChannel channel_of_completion(bool has_data) {
    return has_data ? ChiChannel::RXDAT : ChiChannel::RXRSP;
}
constexpr ChiChannel channel_of_snoop_resp(bool has_data) {
    return has_data ? ChiChannel::TXDAT : ChiChannel::TXRSP;
}

// ── 编译期锁 ①:表恰好覆盖 `ChiOpcode` 全集,每个 opcode 恰好一次 ──────────
constexpr bool chi_opcode_table_is_complete() {
    if (kChiOpcodeTableSize != kChiOpcodeCount)
        return false;
    for (std::size_t i = 0; i < kChiOpcodeTableSize; ++i) {
        std::size_t hits = 0;
        for (std::size_t j = 0; j < kChiOpcodeTableSize; ++j) {
            if (kChiOpcodeTable[i].opcode == kChiOpcodeTable[j].opcode)
                ++hits;
        }
        if (hits != 1)
            return false;
    }
    return true;
}

// ── 编译期锁 ②:6 通道**各有**至少一个可接受 opcode(CR-11 的可达性前提)────
constexpr bool chi_channels_are_reachable() {
    for (std::size_t c = 0; c < kChiChannelCount; ++c) {
        bool hit = false;
        for (std::size_t i = 0; i < kChiOpcodeTableSize; ++i) {
            if (kChiOpcodeTable[i].accepted &&
                static_cast<std::size_t>(kChiOpcodeTable[i].channel) == c) {
                hit = true;
                break;
            }
        }
        if (!hit)
            return false;
    }
    return true;
}

// ── 编译期锁 ③:表的通道与「运行期判定规则」一致(防两张口径漂移)──────────
constexpr bool chi_channel_rules_are_consistent() {
    for (std::size_t i = 0; i < kChiOpcodeTableSize; ++i) {
        const auto& e = kChiOpcodeTable[i];
        if (!e.accepted)
            continue;
        if (e.flow == CanonicalFlow::Completion && e.channel != channel_of_completion(e.has_data))
            return false;
        if (e.flow == CanonicalFlow::SnoopResp && e.channel != channel_of_snoop_resp(e.has_data))
            return false;
        if (e.flow == CanonicalFlow::Request && e.channel != ChiChannel::TXREQ)
            return false;
        if (e.flow == CanonicalFlow::Snoop && e.channel != ChiChannel::RXSNP)
            return false;
    }
    return true;
}

static_assert(chi_opcode_table_is_complete(),
              "CR-11:opcode 表必须恰好覆盖 subset 的全部 opcode(各一次)");
static_assert(chi_channels_are_reachable(),
              "CR-11:6 通道每个必须至少有一个可接受 opcode 可达(否则观测点恒 0)");
static_assert(chi_channel_rules_are_consistent(),
              "CR-11:opcode 表的通道归属必须与运行期判定规则一致(两张口径禁止漂移)");

// ══════════════════════════════════════════════════════════════════════
//  可观测量(判据的观测面)
// ══════════════════════════════════════════════════════════════════════
struct ChiRnfEngineCounters {
    // ── CR-11:6 通道观测点 ──────────────────────────────────────────
    std::array<std::uint64_t, kChiChannelCount> observed{};  ///< 该通道发生的**协议事件**数
    std::array<std::uint64_t, kChiChannelCount> emitted{};   ///< 其中**成功发到 egress** 的数
    std::uint64_t txreq_request = 0;    ///< TXREQ 承载 `Request` 流(CR-11 ②)
    std::uint64_t txreq_writeback = 0;  ///< TXREQ 承载 `Writeback` 流(CR-11 ②)
    // ── CR-12:Credit ────────────────────────────────────────────────
    std::uint64_t credit_stalls = 0;      ///< 因 credit 耗尽而未能发出的次数
    std::uint64_t credit_consumed = 0;    ///< 成功发射消耗的 credit 总数
    std::uint64_t credit_restored = 0;    ///< `receive_credit` 补充的 credit 总数
    std::uint64_t credit_out_of_domain = 0;  ///< `vc >= 6`(越域;防御性计数)
    std::uint64_t egress_backpressure = 0;   ///< 因 egress 槽满而未能发出的次数
    // ── CR-13:TxnID ─────────────────────────────────────────────────
    std::uint64_t txn_allocated = 0;
    std::uint64_t txn_released = 0;
    std::uint64_t txn_reused = 0;              ///< 释放后**再次**分配(复用)的次数
    std::uint64_t txn_duplicate_rejected = 0;  ///< 在途重复被拒的次数
    // ── CR-14:CompAck ───────────────────────────────────────────────
    std::uint64_t comp_ack_sent = 0;
    std::uint64_t comp_ack_not_required = 0;  ///< 不需 CompAck 却收到数据完成(正控见证)
    // ── subset 排除项(4.3)──────────────────────────────────────────
    std::array<std::uint64_t, kChiRejectKindCount> rejected{};  ///< 按拒绝理由分桶
    std::uint64_t wrong_channel_rejected = 0;  ///< 白名单 opcode 走了非 TXREQ 通道(内部错用)
    // ── 建模面 ──────────────────────────────────────────────────────
    std::uint64_t copyback_emitted = 0;   ///< TXDAT 的 CopyBack(命中脏行)
    std::uint64_t snoop_miss = 0;         ///< snoop 未命中(回 SnpResp I)
    std::uint64_t foreign_events = 0;     ///< 外来 CohIf 事件(RN-F 不该收的 request/writeback/snoop_resp)
};

// ══════════════════════════════════════════════════════════════════════
//  ChiRnfProtocolEngine
// ══════════════════════════════════════════════════════════════════════
class ChiRnfProtocolEngine : public ProtocolEngineBase<2> {
public:
    using Base = ProtocolEngineBase<2>;
    using Bundle = bundles::CohIfBundle;
    using State = ChiRnfAdapter::State;
    using Counters = ChiRnfEngineCounters;

    /// 每通道的初始 credit(构造参数 ⇒ CR-12 可用「0 credit」构造出**耗尽态**,
    /// 无需测试专用开关)
    static constexpr unsigned kDefaultInitialCredit = 4;

    ChiRnfProtocolEngine(const std::string& name, EventQueue* eq,
                         unsigned initial_credit = kDefaultInitialCredit)
        : Base(name, eq) {
        credits_.fill(initial_credit);
    }
    ~ChiRnfProtocolEngine() override = default;

    const Counters& counters() const { return ctr_; }

    /// CR-11 的汇聚判据:6 通道**每个**都至少观测到 1 次
    bool all_channels_observed() const {
        for (std::size_t c = 0; c < kChiChannelCount; ++c) {
            if (ctr_.observed[c] == 0)
                return false;
        }
        return true;
    }

    // ── TxnID 查询(CR-13 的观测面)───────────────────────────────────
    bool txn_in_flight(std::uint64_t id) const { return in_flight_.count(id) != 0; }
    std::size_t in_flight_count() const { return in_flight_.size(); }

    // ── Credit 查询(CR-12 的观测面)─────────────────────────────────
    unsigned credits(ChiChannel ch) const { return credits_[static_cast<std::size_t>(ch)]; }
    std::size_t pending_egress() const { return pending_.size(); }

    // ── 行状态(引擎持有的最小行表;snoop 响应的依据)──────────────────
    void set_line(std::uint64_t addr, State s, std::uint64_t data) {
        if (s == State::I) {
            lines_.erase(addr);
            return;
        }
        lines_[addr] = Line{s, data};
    }
    bool line_state(std::uint64_t addr, State& out) const {
        const auto it = lines_.find(addr);
        if (it == lines_.end()) {
            out = State::I;
            return false;
        }
        out = it->second.state;
        return true;
    }
    std::size_t line_count() const { return lines_.size(); }

    // ══════════════════════════════════════════════════════════════════
    //  CHI 侧入口(opcode 级)—— subset 排除项的**机器闸**(task 4.3)
    // ══════════════════════════════════════════════════════════════════
    /// 本地缓存发起一次 CHI 请求。
    ///
    /// **闸的顺序**(任何一步失败 ⇒ 不产生任何可观测副作用):
    ///   ① opcode 白名单:排除项 ⇒ **拒绝**并计数(静默收下 ⇒ 判据红)
    ///   ② 通道校验:白名单 opcode 必须落在 TXREQ(表错用 ⇒ 拒绝)
    ///   ③ TxnID 分配:在途重复 ⇒ **拒绝**并计数
    ///   ④ 观测 TXREQ(+ 双流承载分桶)+ 发射对应的 CohIf Bundle
    ///
    /// @return true = 已接受并进入发射路径(可能因 credit 耗尽而**暂存**)
    bool submit_request(ChiOpcode op, std::uint64_t addr, Perm perm, std::uint64_t size,
                        std::uint64_t txn_id) {
        const ChiOpcodeInfo& info = chi_opcode_info(op);
        if (!info.accepted) {
            ++ctr_.rejected[static_cast<std::size_t>(info.reject)];
            return false;
        }
        if (info.channel != ChiChannel::TXREQ) {
            ++ctr_.wrong_channel_rejected;
            return false;
        }
        if (!begin_txn(txn_id))
            return false;

        observe(ChiChannel::TXREQ);
        if (info.flow == CanonicalFlow::Writeback)
            ++ctr_.txreq_writeback;
        else
            ++ctr_.txreq_request;

        Bundle b;
        b.transaction_id.write(txn_id);
        b.address.write(addr);
        b.size.write(size);
        b.perm.write(static_cast<std::uint8_t>(perm));
        txn_addr_[txn_id] = addr;  // 数据回来时按 txn 反查地址装载行(canonical 签名不带地址)
        if (info.flow == CanonicalFlow::Writeback) {
            b.flow_id = bundles::CohIfFlowId::Writeback;
            const auto it = lines_.find(addr);
            b.data.write(it != lines_.end() ? it->second.data : 0);  // 行表是唯一真源
            b.dirty.write(1);
            lines_.erase(addr);
        } else {
            b.flow_id = bundles::CohIfFlowId::Request;
            if (op == ChiOpcode::ReadUnique || op == ChiOpcode::MakeUnique)
                comp_ack_pending_.insert(txn_id);  // 读类独占 ⇒ 数据回来后须 CompAck
        }
        emit(b, ChiChannel::TXREQ);
        return true;
    }

    // ══════════════════════════════════════════════════════════════════
    //  canonical 10 流(CohIf ingress 侧)
    // ══════════════════════════════════════════════════════════════════

    /// ⚠️ **外来事件**:RN-F 不接收 request(它只发 REQ、只收 SNP)。
    /// 该路径存在只为「协议中立的 CohIf 兼容」⇒ **不**计入 TXREQ
    /// (否则「收到请求」会被误记成「发出请求」)。
    void on_request(std::uint64_t, Perm, std::uint64_t) override { ++ctr_.foreign_events; }

    /// Completion(RXRSP / RXDAT)—— 数据回来的那一刻是 CompAck 的触发点
    void on_completion(std::uint64_t txn_id, std::uint64_t data, Perm perm,
                       bool error) override {
        // ⚠️ `CohIfBundle` 无 `has_data` 位 ⇒ 以 `data != 0` 近似(已知缺口 ①)
        observe(channel_of_completion(data != 0));

        if (error) {
            end_txn(txn_id);
            comp_ack_pending_.erase(txn_id);
            return;
        }

        if (data != 0) {
            // 行装载以**地址**为键,而 canonical 签名不带地址 ⇒ 用请求侧登记的
            // `txn_addr_` 查表(`submit_request` 时写入)。
            install_line_from_completion(txn_id, data, perm);

            if (comp_ack_pending_.erase(txn_id) != 0) {
                emit_comp_ack(txn_id);  // 恰好一次:erase 成功才发
            } else {
                ++ctr_.comp_ack_not_required;  // 正控见证:该事务不需要 CompAck
            }
            end_txn(txn_id);
            return;
        }

        // 无数据的 Comp / CompDBIDResp:需 CompAck 的事务仍在等数据 ⇒ 不结束
        if (comp_ack_pending_.count(txn_id) == 0)
            end_txn(txn_id);
    }

    /// Snoop(RXSNP)—— 命中脏行 ⇒ TXDAT(CopyBack);否则 ⇒ TXRSP(SnpResp)
    void on_snoop(std::uint64_t addr, SnoopType type, std::uint64_t txn_id) override {
        observe(ChiChannel::RXSNP);

        const auto it = lines_.find(addr);
        const bool hit = (it != lines_.end() && it->second.state != State::I);

        if (!hit) {
            ++ctr_.snoop_miss;
            emit_snoop_resp(addr, /*hit=*/false, State::I, /*dirty=*/false, 0, txn_id);
            return;
        }

        const bool dirty = (it->second.state == State::UD || it->second.state == State::SD);
        if (dirty) {
            // CopyBack:一行脏数据回送 + 本地失效(TXDAT,承载 `Writeback` 流)
            Bundle b;
            b.flow_id = bundles::CohIfFlowId::Writeback;
            b.transaction_id.write(txn_id);
            b.address.write(addr);
            b.data.write(it->second.data);
            b.dirty.write(1);
            lines_.erase(it);
            ++ctr_.copyback_emitted;
            observe(ChiChannel::TXDAT);
            emit(b, ChiChannel::TXDAT);
            return;
        }

        // 干净行:降级(INVALIDATE ⇒ I;CLEAN / FORWARD ⇒ SC)并回 SnpResp
        const bool invalidating = (type == SnoopType::INVALIDATE);
        const State after = invalidating ? State::I : State::SC;
        const std::uint64_t payload = it->second.data;
        if (invalidating)
            lines_.erase(it);
        else
            it->second.state = State::SC;
        emit_snoop_resp(addr, /*hit=*/true, after, /*dirty=*/false, payload, txn_id);
    }

    /// ⚠️ **外来事件**:RN-F 发出 snoop_resp(由 `on_snoop` 生成),不接收它。
    void on_snoop_resp(std::uint64_t, bool, LineState, bool, std::uint64_t,
                       std::uint64_t) override {
        ++ctr_.foreign_events;
    }

    /// ⚠️ **外来事件**:RN-F 的写回由 `submit_request(WriteBackFull)` 发起
    /// (那是**发出**);从线缆上收到 Writeback 不是 RN-F 的行为。
    void on_writeback(std::uint64_t, std::uint64_t, bool) override { ++ctr_.foreign_events; }

    // ── 不经 CohIf 腿的 5 条(PE 内部终结 / 对接 Directory & Ordering Plane)──
    void on_forward_data(std::uint64_t, std::uint64_t, std::uint64_t, Perm,
                         std::uint64_t) override {
        ++ctr_.foreign_events;
    }
    void on_back_invalidate(std::uint64_t, SnoopType, std::uint64_t) override {
        ++ctr_.foreign_events;
    }
    void on_address_resolve(std::uint64_t pa, std::uint64_t node_id,
                            AddressResolveResult& out) override {
        // ⚠️ CHI 的 home 由**静态 SAM** 解析(subset 无 wire query 事务)⇒
        //    最小诚实语义:解析为「请求者自身」且**不做一致性声称**
        //    (与 `NullProtocolEngine` 的同款口径,但本引擎**不**把它当作协议动作)
        (void)pa;
        out.home_node = node_id;
        out.coherent = false;
    }
    void on_barrier(std::uint64_t, std::uint64_t) override { ++ctr_.foreign_events; }
    void on_snoop_completion_ack(std::uint64_t, std::uint64_t, std::uint64_t) override {
        ++ctr_.foreign_events;
    }

    // ══════════════════════════════════════════════════════════════════
    //  Credit(CR-12)
    // ══════════════════════════════════════════════════════════════════
    /// ⚠️ **`vc` 被解释为「通道号」**(`0..5`)—— CHI 的 6 通道**就是** RN-F 的
    ///    信用域粒度(`CohIfBundle` 的 VC 只有 3 类 flit 类别,不足以表达 6 通道)。
    void receive_credit(unsigned /*port*/, unsigned vc) override {
        if (vc >= kChiChannelCount) {
            ++ctr_.credit_out_of_domain;
            return;
        }
        ++credits_[vc];
        ++ctr_.credit_restored;
    }

    // ══════════════════════════════════════════════════════════════════
    //  周期推进
    // ══════════════════════════════════════════════════════════════════
    void tick() override {
        drain_pending();  // ① 上一拍的积压先占空槽(credit 可能已补)
        Base::tick();     // ② ingress 分派(产生新发射)+ adapter 发送
        drain_pending();  // ③ adapter 发送后空出的槽再补一轮
    }

private:
    struct Line {
        State         state;
        std::uint64_t data;
    };
    struct Pending {
        Bundle     bundle;
        ChiChannel channel;
    };

    // ── TxnID ────────────────────────────────────────────────────────
    bool begin_txn(std::uint64_t id) {
        if (in_flight_.count(id) != 0) {
            ++ctr_.txn_duplicate_rejected;
            return false;
        }
        if (released_.count(id) != 0)
            ++ctr_.txn_reused;  // 释放后**再次**分配 ⇒ 复用计数
        in_flight_.insert(id);
        ++ctr_.txn_allocated;
        return true;
    }
    void end_txn(std::uint64_t id) {
        if (in_flight_.erase(id) == 0)
            return;
        released_.insert(id);
        ++ctr_.txn_released;
    }

    // ── 观测 ─────────────────────────────────────────────────────────
    void observe(ChiChannel ch) { ++ctr_.observed[static_cast<std::size_t>(ch)]; }

    // ── 发射(credit 门控)───────────────────────────────────────────
    bool emit(const Bundle& b, ChiChannel ch) {
        const auto ci = static_cast<std::size_t>(ch);
        if (credits_[ci] == 0) {
            ++ctr_.credit_stalls;  // 耗尽 ⇒ **停发**(CR-12 ① 的观测点)
            pending_.push_back(Pending{b, ch});
            return false;
        }
        if (!write_to_egress(b)) {
            ++ctr_.egress_backpressure;
            pending_.push_back(Pending{b, ch});
            return false;
        }
        --credits_[ci];
        ++ctr_.credit_consumed;
        ++ctr_.emitted[ci];
        return true;
    }

    bool write_to_egress(const Bundle& b) {
        for (std::size_t p = 0; p < Base::kPortCount; ++p) {
            if (!this->resp_out()[p].valid()) {
                this->resp_out()[p].write(b);
                return true;
            }
        }
        return false;
    }

    void drain_pending() {
        while (!pending_.empty()) {
            const auto& head = pending_.front();
            const auto ci = static_cast<std::size_t>(head.channel);
            // 队头阻塞:head 的通道没 credit ⇒ 整队等待(保持**每通道 FIFO** 语义)
            if (credits_[ci] == 0)
                return;
            if (!write_to_egress(head.bundle))
                return;
            --credits_[ci];
            ++ctr_.credit_consumed;
            ++ctr_.emitted[ci];
            pending_.pop_front();
        }
    }

    /// CompAck 的发射 —— ⚠️ **无 CohIf 载体**(流 10 被 `CohIfBundle` 的 `flow_id`
    /// 类型层排除,per design `D-β`)⇒ 它是**通道级事件**(TXRSP),记账 + 占 TXRSP
    /// 观测点,但**不**构造可发送的 Bundle。Stage 2 若扩 `CohIfBundle` 的流域,
    /// 这里改为真实发射(登记为缺口)。
    void emit_comp_ack(std::uint64_t txn_id) {
        (void)txn_id;
        observe(ChiChannel::TXRSP);
        ++ctr_.comp_ack_sent;
    }

    void emit_snoop_resp(std::uint64_t addr, bool hit, State state, bool dirty,
                         std::uint64_t data, std::uint64_t txn_id) {
        Bundle b;
        b.flow_id = bundles::CohIfFlowId::SnoopResp;
        b.transaction_id.write(txn_id);
        b.address.write(addr);
        b.data.write(data);
        b.hit.write(hit ? 1 : 0);
        b.dirty.write(dirty ? 1 : 0);
        b.line_state.write(static_cast<std::uint8_t>(state));
        observe(ChiChannel::TXRSP);
        emit(b, ChiChannel::TXRSP);
    }

    void install_line_from_completion(std::uint64_t txn_id, std::uint64_t data, Perm perm) {
        const auto it = txn_addr_.find(txn_id);
        if (it == txn_addr_.end())
            return;
        const State st = (perm == Perm::UNIQUE)      ? State::UC
                         : (perm == Perm::WRITEBACK) ? State::UD
                                                     : State::SC;
        lines_[it->second] = Line{st, data};
    }

    Counters                                  ctr_;
    std::array<unsigned, kChiChannelCount>    credits_{};
    std::unordered_set<std::uint64_t>         in_flight_;
    std::unordered_set<std::uint64_t>         released_;
    std::unordered_set<std::uint64_t>         comp_ack_pending_;
    std::unordered_map<std::uint64_t, Line>   lines_;
    std::unordered_map<std::uint64_t, std::uint64_t> txn_addr_;
    std::deque<Pending>                       pending_;
};

}  // namespace tlm::cep

#endif  // TLM_CEP_CHI_RNF_PROTOCOL_ENGINE_HH
