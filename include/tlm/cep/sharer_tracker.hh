// include/tlm/cep/sharer_tracker.hh
// Sharer Tracker —— 协议无关单接口 + caps descriptor + FullMapDirectory 最小行为实现
//
// 功能:把 `ADR-CEP-04` 的 α′ 三件套((α) 单接口 + (γ′) caps descriptor + D-5 重构)
//       落成**可执行契约**。队列 #4 只实现 5 种实现中的 **FullMapDirectory**;
//       其余 4 种(SparseSnoopFilter / DcohSymmetric / TreeManager / BroadcastNull)
//       跟随各自的消费队列(队列 #6~#9)落地 —— 此处只留**声明层** traits。
// 作者: CppTLM Team / 日期: 2026-10-25
//
// ── 定位 ───────────────────────────────────────────────────────────────
// **不是缓存、不是网络组件**:不与 JSON `connections[]` 相连、不进 `ModuleFactory`、
// 不注册 ABI。它是 home 侧(或 per-device 侧)的**目录视图**,由 ProtocolEngine 调用。
//
// ── 接口面(逐字对齐 `ADR-CEP-04 §3.1 / §3.3 / §3.4`)────────────────────
//   lookup(addr, intent, result, view, hazard)
//   snoop_begin(addr, expected_responders, handle)
//   snoop_fold(handle, node_id, resp_state, dirty, data_supplied, remaining_mask, merged_state)
//   observe_snoop(addr, from_node, resulting_local_state)
//   ⇒ 恰好 4 个虚方法 + 虚析构(测试用 `virtual` 计数机检)
//
// ── 两个「本文件首次定义」的类型(ADR 未给 wire 形态)─────────────────────
//   · `responder_set_t` = **64-bit bitmask**(`static_assert` 锁);bit → node_id 走
//     `kResponderBitToNode` **显式映射表**(bit 0 = `kNodeIdSdma`),**不**假设恒等映射
//   · `MergedState` = `SnoopState`(fold 后的状态并;**E 与 M 的「并」不会丢脏**)
//
// ── 禁止的旧接口(per `ADR-CEP-04 §3.6`,由测试的去注释静态扫描守住)────────
//   旧的单次完成事件、把「远程 snoop 订阅」塞进 tracker、以及任何
//   「集中/分布」布尔开关,都不在本文件出现 —— 它们把传输层概念漏进了本平面。
//
// ── 5 实现 × 4 caps + 2 behavior(数据级表,per ADR 决策 3 表 + §4 表)──────
//   caps 的 4 属性**必须**是 {scope, completeness, authority, aggregation_at}
//   (冻结 schema `protocol_instance.schema.json` 同形);`hazard` 与 `broadcast_fallback`
//   是**行为**属性(ADR §4 表 / 决策 2),不是 caps 字段。
#ifndef CPPTLM_CEP_SHARER_TRACKER_HH
#define CPPTLM_CEP_SHARER_TRACKER_HH

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace tlm::cep {

// ─────────────────────────────────────────────────────────────────────
// 基础类型
// ─────────────────────────────────────────────────────────────────────
using node_id_t = std::uint32_t;
using responder_set_t = std::uint64_t;
using snoop_handle_t = std::uint16_t;

static_assert(sizeof(responder_set_t) == 8,
              "ResponderSet SHALL be a 64-bit bitmask(本 spec 首次定义 · 队列 #4 A-1-2)");

inline constexpr node_id_t kInvalidNode = 0xFFFFFFFFu;

// node_id 取值镜像 `soc_internal_noc.hh` 的 NODE_ID 编码(该处是**传输层**路由节点编号;
// 本处复用其数值以保持 bit→node 的映射可对账,不代表同一张图的顶点)。
inline constexpr node_id_t kNodeIdPcie = 0;
inline constexpr node_id_t kNodeIdSdma = 1;
inline constexpr node_id_t kNodeIdGmmu = 2;
inline constexpr node_id_t kNodeIdVram = 3;

inline constexpr unsigned kNoBit = 64u;
inline constexpr std::size_t kResponderMappedBits = 4;

// bit → node_id **显式映射表**(bit 0 = SDMA;其余 3 位按「内存归属 → 页表 → 主机」序)
// bits 4..63 未映射(`kInvalidNode`),留给后续协议族的 more-sharer 场景。
inline constexpr std::array<node_id_t, 8> kResponderBitToNode{{
    kNodeIdSdma,   // bit 0
    kNodeIdVram,   // bit 1
    kNodeIdGmmu,   // bit 2
    kNodeIdPcie,   // bit 3
    kInvalidNode, kInvalidNode, kInvalidNode, kInvalidNode,
}};

constexpr unsigned responder_bit_of(node_id_t id) {
    for (std::size_t i = 0; i < kResponderMappedBits; ++i) {
        if (kResponderBitToNode[i] == id)
            return static_cast<unsigned>(i);
    }
    return kNoBit;
}

constexpr responder_set_t responder_mask_of(node_id_t id) {
    const unsigned b = responder_bit_of(id);
    return (b == kNoBit) ? responder_set_t{0} : (responder_set_t{1} << b);
}

constexpr node_id_t responder_node_of(unsigned bit) {
    return (bit < kResponderMappedBits) ? kResponderBitToNode[bit] : kInvalidNode;
}

/// 已映射 bit 的掩码(bits 0..kResponderMappedBits-1)。
/// 判据用:`expected_responders` 若含此掩码之外的位,那些位**永远无法被 `snoop_fold` 扣减**
/// (fold 只认已映射节点 ⇒ `responder_mask_of` 对未映射节点返回 0)⇒ 一经接受,
/// in-flight 条目永不 retire、该地址 `lookup` 永久 STALL。故 `snoop_begin` 必须显式拒绝。
inline constexpr responder_set_t kMappedResponderMask =
    (kResponderMappedBits >= 64) ? ~responder_set_t{0}
                                 : ((responder_set_t{1} << kResponderMappedBits) - 1);

// ─────────────────────────────────────────────────────────────────────
// D-5 重构的结果枚举 / 规范意图 / 状态(per ADR §3.1 §3.2)
// ─────────────────────────────────────────────────────────────────────
enum class LookupResult : std::uint8_t {
    ANSWER_HERE = 0,  // 本地视图充分回答
    FORWARD_TO = 1,   // 向指定目标转发(取数)
    ESCALATE_UP = 2,  // 树形向上递归(TileLink)
    BROADCAST = 3,    // 无目录,需广播
};

enum class LookupHazard : std::uint8_t {
    NONE = 0,
    STALL = 1,  // 等待 in-flight snoop 完成后重试
    CHAIN = 2,  // 加入同一 in-flight snoop 的 expected_responders
};

// 规范意图(替代协议 opcode;协议特有 op 放 opaque 扩展字段)
enum class Intent : std::uint8_t {
    want_read = 0,
    want_write = 1,
    want_own = 2,
    evict = 3,
};

// per-node 稳定状态(I/S/E/M)。与 `stub_cache_core.hh` 的 `LineState` 是**不同平面**
// 的状态:那是 RN 侧客户端的本行状态,这是 home 侧对某 sharer 的认知。
enum class SnoopState : std::uint8_t { I = 0, S = 1, E = 2, M = 3 };
using MergedState = SnoopState;

// ─────────────────────────────────────────────────────────────────────
// caps descriptor(per ADR §2 决策 3,4 属性,第一天就位)
// ─────────────────────────────────────────────────────────────────────
enum class Scope : std::uint8_t { SYSTEM = 0, SUBTREE = 1, LOCAL_MEMORY_REGION = 2, NONE = 3 };
enum class Completeness : std::uint8_t { EXACT = 0, CONSERVATIVE = 1, PARTIAL = 2 };
enum class Authority : std::uint8_t { GLOBAL = 0, LOCAL_HINT = 1, LOCAL = 2, NONE = 3 };
enum class AggregationAt : std::uint8_t { HOME = 0, REQUESTER = 1, TREE_PARENT = 2, NONE = 3 };

struct SharerTrackerCaps {
    Scope scope = Scope::NONE;
    Completeness completeness = Completeness::EXACT;
    Authority authority = Authority::NONE;
    AggregationAt aggregation_at = AggregationAt::NONE;
};

constexpr bool operator==(const SharerTrackerCaps& a, const SharerTrackerCaps& b) {
    return a.scope == b.scope && a.completeness == b.completeness &&
           a.authority == b.authority && a.aggregation_at == b.aggregation_at;
}
constexpr bool operator!=(const SharerTrackerCaps& a, const SharerTrackerCaps& b) { return !(a == b); }

// 行为属性(不是 caps 字段):
//   · hazard_under_contention —— 同地址并发时的返回(ADR §4 表,逐实现)
//   · broadcast_fallback      —— 是否声明广播兜底(ADR 决策 2:PARTIAL 无兜底 = 失效实例)
struct SharerTrackerBehavior {
    LookupHazard hazard_under_contention = LookupHazard::NONE;
    bool broadcast_fallback = false;
};

enum class TrackerImpl : std::uint8_t {
    FullMapDirectory = 0,
    SparseSnoopFilter = 1,
    DcohSymmetric = 2,
    TreeManager = 3,
    BroadcastNull = 4,
};

struct TrackerTraits {
    TrackerImpl impl = TrackerImpl::FullMapDirectory;
    const char* name = "";
    SharerTrackerCaps caps{};
    SharerTrackerBehavior behavior{};
};

// 5 实现 × (4 caps + 2 behavior)。数据源:`ADR-CEP-04 §2 决策 3 表` + `§4 表` + `§3.5`(二元)。
// DcohSymmetric 的 `aggregation_at` 是**二元**(P2P ⇒ REQUESTER / HDM home ⇒ HOME,per §3.5);
// 表内取 P2P 侧作为代表值,二元本身由测试守(DcohSymmetric 行必须落在该二元内)。
inline constexpr std::array<TrackerTraits, 5> kTrackerTraits{{
    {TrackerImpl::FullMapDirectory, "FullMapDirectory",
     {Scope::SYSTEM, Completeness::EXACT, Authority::GLOBAL, AggregationAt::HOME},
     {LookupHazard::STALL, false}},
    {TrackerImpl::SparseSnoopFilter, "SparseSnoopFilter",
     {Scope::SYSTEM, Completeness::CONSERVATIVE, Authority::GLOBAL, AggregationAt::HOME},
     {LookupHazard::STALL, true}},
    {TrackerImpl::DcohSymmetric, "DcohSymmetric",
     {Scope::LOCAL_MEMORY_REGION, Completeness::CONSERVATIVE, Authority::LOCAL_HINT,
      AggregationAt::REQUESTER},
     {LookupHazard::STALL, true}},
    {TrackerImpl::TreeManager, "TreeManager",
     {Scope::SUBTREE, Completeness::EXACT, Authority::LOCAL, AggregationAt::TREE_PARENT},
     {LookupHazard::CHAIN, false}},
    {TrackerImpl::BroadcastNull, "BroadcastNull",
     {Scope::NONE, Completeness::EXACT, Authority::NONE, AggregationAt::NONE},
     {LookupHazard::NONE, true}},
}};

constexpr const TrackerTraits* find_tracker_traits(TrackerImpl impl) {
    for (const auto& t : kTrackerTraits) {
        if (t.impl == impl)
            return &t;
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────
// IC3:条目生命周期 5 事件(名称逐字对齐 ADR §6 表)
// ─────────────────────────────────────────────────────────────────────
enum class LifecycleEvent : std::uint8_t {
    AllocateOnFirstForward = 0,
    MergeOnSnoopBegin = 1,
    DecrementOnSnoopFold = 2,
    RetireOnZeroSharers = 3,
    RetireOnZeroPending = 4,
};

inline constexpr std::array<const char*, 5> kLifecycleEventNames{{
    "allocate-on-first-forward",
    "merge-on-snoop-begin",
    "decrement-on-snoop-fold",
    "retire-on-zero-sharers",
    "retire-on-zero-pending",
}};

constexpr const char* lifecycle_event_name(LifecycleEvent e) {
    const std::size_t i = static_cast<std::size_t>(e);
    return (i < kLifecycleEventNames.size()) ? kLifecycleEventNames[i] : "";
}

// ─────────────────────────────────────────────────────────────────────
// LocalView —— lookup 的本地视图
// ─────────────────────────────────────────────────────────────────────
// 形态本 spec 首次定义:sharers / dirty 为 64-bit bitmask(与 responder_set_t 同型);
// `valid == false` ⇔ hazard != NONE(此时 view 无意义,per ADR §3.1 注释)。
struct LocalView {
    responder_set_t sharers = 0;
    responder_set_t dirty = 0;
    node_id_t forward_to = kInvalidNode;
    bool valid = false;
};

constexpr bool operator==(const LocalView& a, const LocalView& b) {
    return a.sharers == b.sharers && a.dirty == b.dirty && a.forward_to == b.forward_to &&
           a.valid == b.valid;
}

// ─────────────────────────────────────────────────────────────────────
// 单接口(恰好 4 个虚方法)
// ─────────────────────────────────────────────────────────────────────
class SharerTrackerIf {
public:
    virtual ~SharerTrackerIf() = default;

    virtual void lookup(std::uint64_t addr, Intent intent, LookupResult& result, LocalView& view,
                        LookupHazard& hazard) = 0;

    virtual void snoop_begin(std::uint64_t addr, responder_set_t expected_responders,
                             snoop_handle_t& handle) = 0;

    virtual void snoop_fold(snoop_handle_t handle, node_id_t node_id, SnoopState resp_state,
                            bool dirty, bool data_supplied, responder_set_t& remaining_mask,
                            MergedState& merged_state) = 0;

    virtual void observe_snoop(std::uint64_t addr, node_id_t from_node,
                               SnoopState resulting_local_state) = 0;
};

// ─────────────────────────────────────────────────────────────────────
// FullMapDirectory —— 唯一行为实现(completeness = EXACT,无 broadcast 分支)
// ─────────────────────────────────────────────────────────────────────
// 状态模型:per addr → {sharers, dirty} 两个 64-bit bitmask(**精确集**,可超集?不 ——
// FullMap 是 EXACT,不做保守超集)。`observe_snoop` 记的是「peer 对某节点的认知」,
// 与 `dir_` 正交存放,不参与判断(per ADR §3.4「被动记账」)。
class FullMapDirectory final : public SharerTrackerIf {
public:
    static constexpr TrackerImpl kImpl = TrackerImpl::FullMapDirectory;

    /// handle 空间大小(handle = 1..65535;0 保留为「无效」)。
    /// 16-bit 是 `ADR-CEP-04` 的 RTL 契约(`typedef logic [15:0] snoop_handle_t`),不可 widen。
    static constexpr std::size_t kHandleSpace = 65535u;

    // 公开**数据成员**(不是方法)⇒ 接口面仍恰好 4 个方法
    SharerTrackerCaps caps = kTrackerTraits[0].caps;
    SharerTrackerBehavior behavior = kTrackerTraits[0].behavior;

    FullMapDirectory() = default;

    // ── 目录安装(由 home 侧 / 测试夹具驱动)──
    void set_sharer(std::uint64_t addr, node_id_t node, SnoopState state) {
        const responder_set_t m = responder_mask_of(node);
        // 未映射节点 ⇒ **显式 no-op**:不得 `dir_[addr]` 先造条目,否则会留下
        // sharers == 0 的可见空条目(与「无此地址」在 `view_of` 上无法区分)。
        if (m == 0)
            return;
        Entry& e = dir_[addr];
        e.sharers |= m;
        if (state == SnoopState::M)
            e.dirty |= m;
        else
            e.dirty &= ~m;
    }

    void clear_sharer(std::uint64_t addr, node_id_t node) {
        auto it = dir_.find(addr);
        if (it == dir_.end())
            return;
        const responder_set_t m = responder_mask_of(node);
        if (m == 0)
            return;  // 未映射节点 ⇒ 本来就无位可清
        // 该节点**本非** sharer/dirty 持有者 ⇒ no-op。
        // 否则会伪触发 `RetireOnZeroSharers`:例如 `lookup` 刚 Allocate 的空条目
        // (sharers == dirty == 0)被清一次就满足 0/0 条件 ⇒ 事件流出现
        // `Allocate → Retire`,而全程从未有 sharer 存在过 —— 破坏 IC3 的事件语义。
        if ((it->second.sharers & m) == 0 && (it->second.dirty & m) == 0)
            return;
        it->second.sharers &= ~m;
        it->second.dirty &= ~m;
        if (it->second.sharers == 0 && it->second.dirty == 0) {
            dir_.erase(it);
            events_.push_back(LifecycleEvent::RetireOnZeroSharers);
        }
    }

    // ── 4 个接口方法 ──

    void lookup(std::uint64_t addr, Intent intent, LookupResult& result, LocalView& view,
                LookupHazard& hazard) override {
        if (inflight_by_addr_.count(addr) != 0) {
            // 同地址 snoop 在飞:本实现的契约是 STALL(ADR §4 表首行),view 无意义
            result = LookupResult::ANSWER_HERE;
            view = LocalView{};
            hazard = behavior.hazard_under_contention;
            return;
        }

        auto it = dir_.find(addr);
        if (it == dir_.end()) {
            it = dir_.emplace(addr, Entry{}).first;
            events_.push_back(LifecycleEvent::AllocateOnFirstForward);
        }

        hazard = LookupHazard::NONE;
        view = LocalView{};
        view.valid = true;
        view.sharers = it->second.sharers;
        view.dirty = it->second.dirty;

        // FullMap 是 home 侧目录 ⇒ 要么就地回答,要么让请求者去 dirty 持有者取数
        if (intent == Intent::want_read && it->second.dirty != 0) {
            const unsigned b = first_bit(it->second.dirty);
            view.forward_to = responder_node_of(b);
            result = LookupResult::FORWARD_TO;
            return;
        }
        result = LookupResult::ANSWER_HERE;
    }

    void snoop_begin(std::uint64_t addr, responder_set_t expected_responders,
                     snoop_handle_t& handle) override {
        // 期望集含**未映射位**(bits 4..63)⇒ 那些位永远扣不掉 ⇒ snoop 永不完成、
        // 条目永久泄漏、该地址 lookup 永久 STALL。与「同址矛盾」同一拒绝语义。
        if ((expected_responders & ~kMappedResponderMask) != 0) {
            handle = snoop_handle_t{0};
            return;
        }

        // 同地址已有 snoop 在飞:本实现按 ADR §4 返回 STALL,调用方本应先等 ——
        //   · 期望集**相同** ⇒ IC2 幂等,返回既有 handle(不新建 pending)
        //   · 期望集**不同** ⇒ 与在飞 snoop 矛盾,返回 **0**(无效 handle)并拒绝;
        //     不得新建第二条 pending(否则旧 handle 泄漏,且 addr→handle 索引会被覆盖)
        auto sit = inflight_by_addr_.find(addr);
        if (sit != inflight_by_addr_.end()) {
            const auto fit = inflight_.find(sit->second);
            handle = (fit != inflight_.end() && fit->second.expected == expected_responders)
                         ? sit->second
                         : snoop_handle_t{0};
            return;
        }

        handle = allocate_handle();
        if (handle == 0)
            return;  // 满表 ⇒ 拒绝,不得覆盖存活 handle
        Inflight f;
        f.addr = addr;
        f.expected = expected_responders;
        f.remaining = expected_responders;
        f.merged = SnoopState::I;
        inflight_[handle] = f;
        inflight_by_addr_[addr] = handle;
        events_.push_back(LifecycleEvent::MergeOnSnoopBegin);
    }

    void snoop_fold(snoop_handle_t handle, node_id_t node_id, SnoopState resp_state, bool dirty,
                    bool data_supplied, responder_set_t& remaining_mask,
                    MergedState& merged_state) override {
        auto it = inflight_.find(handle);
        if (it == inflight_.end()) {
            // 未知 handle:显式返回「无进展」的哨兵值(全 1 + I),调用方据此报错
            remaining_mask = ~responder_set_t{0};
            merged_state = SnoopState::I;
            return;
        }

        const responder_set_t bit = responder_mask_of(node_id);
        if ((it->second.remaining & bit) != 0) {
            it->second.remaining &= ~bit;
            it->second.merged = join_state(it->second.merged, resp_state, dirty, data_supplied);
            events_.push_back(LifecycleEvent::DecrementOnSnoopFold);
        }
        // bit 已清 ⇒ 重复 fold:返回当前值、不重复扣减(IC2 幂等)

        remaining_mask = it->second.remaining;
        merged_state = it->second.merged;

        if (it->second.remaining == 0) {
            inflight_by_addr_.erase(it->second.addr);
            inflight_.erase(it);
            events_.push_back(LifecycleEvent::RetireOnZeroPending);
        }
    }

    void observe_snoop(std::uint64_t addr, node_id_t from_node,
                       SnoopState resulting_local_state) override {
        // 被动记账:同 (addr, from_node, state) 重复调用 ⇒ 同一投影(IC2 幂等)
        observed_[addr][from_node] = resulting_local_state;
    }

    // ── 观测(测试夹具 / 调用方诊断)──

    LocalView view_of(std::uint64_t addr) const {
        LocalView v;
        auto it = dir_.find(addr);
        if (it == dir_.end())
            return v;
        v.valid = true;
        v.sharers = it->second.sharers;
        v.dirty = it->second.dirty;
        return v;
    }

    /// peer 视角投影(由 `observe_snoop` 累积) —— IC2 幂等断言的观测点
    LocalView peer_view_of(std::uint64_t addr) const {
        LocalView v;
        auto it = observed_.find(addr);
        if (it == observed_.end())
            return v;
        v.valid = true;
        for (const auto& [node, state] : it->second) {
            if (state == SnoopState::I)
                continue;
            v.sharers |= responder_mask_of(node);
            if (state == SnoopState::M)
                v.dirty |= responder_mask_of(node);
        }
        return v;
    }

    std::size_t inflight_count() const { return inflight_.size(); }
    bool is_inflight(std::uint64_t addr) const { return inflight_by_addr_.count(addr) != 0; }
    const std::vector<LifecycleEvent>& events() const { return events_; }

    std::size_t event_count(LifecycleEvent e) const {
        std::size_t n = 0;
        for (const auto& x : events_)
            if (x == e)
                ++n;
        return n;
    }

private:
    struct Entry {
        responder_set_t sharers = 0;
        responder_set_t dirty = 0;
    };
    struct Inflight {
        std::uint64_t addr = 0;
        responder_set_t expected = 0;  // begin 时的原始期望集(幂等判定用,不随 fold 收缩)
        responder_set_t remaining = 0;
        MergedState merged = SnoopState::I;
    };

    /// 16-bit handle 是 `ADR-CEP-04` 的 **RTL 契约**(`typedef logic [15:0] snoop_handle_t`),
    /// 不可 widen ⇒ 分配时**线性探测空闲槽**,表满显式拒绝。
    /// (旧实现 `next_handle_++` 无存活检查:65535 个在飞后第 65536 个新地址拿到 handle 1,
    ///  覆盖存活条目 —— `inflight_by_addr_` 悬挂指向已易主的 handle,旧地址永久 STALL。)
    snoop_handle_t allocate_handle() {
        for (std::size_t probe = 0; probe < kHandleSpace; ++probe) {
            const snoop_handle_t cand = next_handle_;
            next_handle_ = static_cast<snoop_handle_t>(
                (next_handle_ == 0xFFFFu) ? 1u : (static_cast<unsigned>(next_handle_) + 1u));
            if (cand != 0 && inflight_.count(cand) == 0)
                return cand;
        }
        return 0;  // 满表
    }

    static unsigned first_bit(responder_set_t mask) {
        for (unsigned b = 0; b < 64; ++b) {
            if ((mask & (responder_set_t{1} << b)) != 0)
                return b;
        }
        return kNoBit;
    }

    /// 状态并:E/M 的「并」不丢脏;但 dirty 必须**带数据**才升 M(per ArchForge VC-R2:
    /// 数据一律走 forward_data 槽,dirty 而无数据 ⇒ 不构成可写副本)。
    static MergedState join_state(MergedState cur, SnoopState resp, bool dirty,
                                  bool data_supplied) {
        MergedState r = cur;
        if (static_cast<std::uint8_t>(resp) > static_cast<std::uint8_t>(r))
            r = resp;
        if (dirty && data_supplied)
            r = SnoopState::M;
        return r;
    }

    std::map<std::uint64_t, Entry> dir_;
    std::map<snoop_handle_t, Inflight> inflight_;
    std::map<std::uint64_t, snoop_handle_t> inflight_by_addr_;
    std::map<std::uint64_t, std::map<node_id_t, SnoopState>> observed_;
    std::vector<LifecycleEvent> events_;
    snoop_handle_t next_handle_ = 1;
};

}  // namespace tlm::cep

#endif  // CPPTLM_CEP_SHARER_TRACKER_HH
