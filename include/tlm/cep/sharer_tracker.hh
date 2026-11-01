// include/tlm/cep/sharer_tracker.hh
// Sharer Tracker —— 协议无关单接口 + caps descriptor + 2 个行为实现
//
// 功能:把 `ADR-CEP-04` 的 α′ 三件套((α) 单接口 + (γ′) caps descriptor + D-5 重构)
//       落成**可执行契约**。行为实现已有 **3** 种:
//       · `FullMapDirectory`(队列 #4 —— 单级目录,**EXACT**)
//       · `HierarchicalDirectory`(队列 #9 —— 分级目录,第 6 种实现,per `ADR-CEP-04 §13`)
//       · `SparseSnoopFilter`(队列 #10 实现层 —— **CONSERVATIVE** 保守型 snoop filter)
//       其余 3 种(DcohSymmetric / TreeManager / BroadcastNull)
//       **行为实现尚未落地** —— 此处只留**声明层** traits;落位已登记(per 台账 Entry #107):
//       `TreeManager` ↔ 队列 #11 · `DcohSymmetric` ↔ 队列 #12(均**队列名对应**的推导值);
//       `BroadcastNull` = **当前队列序列无消费者 ⇒ 不排期、保留登记**(per Entry #112)。
// ⚠️ `SparseSnoopFilter` 的行为实现**已被 `moesi_gpu`(队列 #6)声明占用**,
//    落地于**队列 #10 实现层**(per 台账 Entry #112 去重 · Entry #124 复核维持)⇒ §8.1 **R11**
//    的「meta 层」缺口闭合;其「语义层」缺口(补偿路径无判据)由**实现层 change** 承担。
//
// ⚠️ **`FullMapDirectory` 的落位裁决(2026-11-01,per 台账 Entry #139)**:它**有行为实现**但
//    **零实例声明**(4 个实例无一声明它)⇒ **反向缺口**(与 R11 的「声明超前于实现」**方向相反**)
//    ⇒ 裁决 = **当前队列序列无消费者 ⇒ 不排期、保留登记**(照 `BroadcastNull` 同款,per Entry #112)
//    ⇒ ⚠️ **实现不废弃** —— 未来 CHI-full 队列出现时**直接可用**。
// 作者: CppTLM Team / 日期: 2026-10-25(2026-10-30 增 `HierarchicalDirectory` + 公共机械抽取;
//       2026-11-01 增 `SparseSnoopFilter`,per 队列 #10 实现层)
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
enum class AggregationAt : std::uint8_t { HOME = 0, REQUESTER = 1, TREE_PARENT = 2, NONE = 3,
                                  /// **多级聚合**(per `ADR-CEP-04 §13` 的 R-1 裁决):聚合发生在**多级目录**
                                  /// (L1 per-GPC / L2 per-GPU / L3 per-Compute-Tray)—— `HOME` 是**单点**、
                                  /// `TREE_PARENT` 是**单父**,二者都表达不了「多级」
                                  HIERARCHICAL = 4 };

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
    /// 第 6 种实现(per `ADR-CEP-04 §13` 的 R-1 裁决):**分级目录**
    /// —— `scope=SYSTEM` + `completeness=EXACT` + `authority=GLOBAL`,但聚合在**多级**目录
    HierarchicalDirectory = 5,
};

struct TrackerTraits {
    TrackerImpl impl = TrackerImpl::FullMapDirectory;
    const char* name = "";
    SharerTrackerCaps caps{};
    SharerTrackerBehavior behavior{};
};

// **6** 实现 × (4 caps + 2 behavior)。数据源:`ADR-CEP-04 §2 决策 3 表` + `§4 表` + `§3.5`(二元)
// + `§13`(2026-10-29 的 R-1 裁决:新增第 6 种 `HierarchicalDirectory` + `AggregationAt::HIERARCHICAL`)。
// DcohSymmetric 的 `aggregation_at` 是**二元**(P2P ⇒ REQUESTER / HDM home ⇒ HOME,per §3.5);
// 表内取 P2P 侧作为代表值,二元本身由测试守(DcohSymmetric 行必须落在该二元内)。
inline constexpr std::array<TrackerTraits, 6> kTrackerTraits{{
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
    // 第 6 行(per `ADR-CEP-04 §13` 的 R-1 裁决):分级目录 —— EXACT + GLOBAL,
    // 但 `aggregation_at` 是**多级**(HOME 单点 / TREE_PARENT 单父 都表达不了)
    {TrackerImpl::HierarchicalDirectory, "HierarchicalDirectory",
     {Scope::SYSTEM, Completeness::EXACT, Authority::GLOBAL, AggregationAt::HIERARCHICAL},
     {LookupHazard::STALL, false}},
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

    // ── 能力描述符(**数据成员**,不是虚方法)────────────────────────────
    // ⚠️ **为什么提到接口上**(2026-11-01,per 台账 Entry #136 / R11 语义层缺口):
    //   `caps` / `behavior` 此前**只在各行为实现类上** ⇒ 持 `SharerTrackerIf*` 的
    //   **消费方读不到** `completeness` ⇒ `moesi_gpu` 的 `O→WRITEBACK` **补偿路径**
    //   无法判定「保守补偿 vs 等价恢复」⇒ 只能抄第二份 caps ⇒ **违反单源纪律**。
    //   (根因:补偿路径需要一个**运行期载体**才能持有并查询 tracker ——
    //    `MoesiGpuAdapter` 是**纯静态**的且 `moesi_gpu` **无 protocol engine**。)
    // ⇒ 现在 **存储唯一**(基类这一份),各实现**不再另存副本**。
    //
    // ⚠️ **「接口恰好 4 个方法」契约不受影响**(per `ADR-CEP-04 §3`):
    //   本处是**数据成员**、**不是**虚方法 ⇒ `count_member_virtuals` 仍 = 5
    //   (4 个接口方法 + 1 个虚析构)⇒ 已归档的 `cep-sharer-tracker-base`
    //   的 **Scenario 全部不受影响** ⇒ main spec **零改动**。
    //
    // ⚠️ **各实现的 caps 取值仍逐字来自 `kTrackerTraits[N]`**(冻结契约**被消费**,
    //    不重推)—— 只是**存放位置**从派生类移到基类。
    SharerTrackerCaps caps{};
    SharerTrackerBehavior behavior{};

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
// SnoopMechanics —— snoop 在飞句柄 + 被动记账的**公共机械**
// ─────────────────────────────────────────────────────────────────────
// ⚠️ **为什么抽出来**:snoop begin/fold/observe 的**协议语义**由 `ADR-CEP-04 §4`
//    定义,**不随实现而异** ⇒ 多个行为实现(本期:`FullMapDirectory` /
//    `HierarchicalDirectory`)**必须共用一份** ⇒ 禁双实现(同
//    `adapter_contracts_loader.hh` 的同源纪律)。
// ⚠️ **零虚函数** ⇒ `SharerTrackerIf` 的「恰好 4 个方法」契约不受影响
//    (判据 `count_member_virtuals == 5` 不变)。
struct SnoopMechanics {
    /// 在飞的 snoop 条目
    struct Inflight {
        std::uint64_t addr = 0;
        responder_set_t expected = 0;  // begin 时的原始期望集(幂等判定用,不随 fold 收缩)
        responder_set_t remaining = 0;
        MergedState merged = SnoopState::I;
    };

    /// handle 空间大小(handle = 1..65535;0 保留为「无效」)。
    /// 16-bit 是 `ADR-CEP-04` 的 RTL 契约(`typedef logic [15:0] snoop_handle_t`),不可 widen。
    static constexpr std::size_t kHandleSpace = 65535u;

    // ── snoop_begin 的公共实现 ──────────────────────────────────────────
    void snoop_begin_core(std::uint64_t addr, responder_set_t expected_responders,
                          snoop_handle_t& handle) {
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

    // ── snoop_join 的公共实现(**CHAIN** 语义:加入**同一** in-flight snoop)──
    // ⚠️ **与 `snoop_begin_core` 的拒绝语义不同**:后者在「同址已有 snoop 且期望集不同」时
    //    **拒绝**(返 0);而 `LookupHazard::CHAIN` 的语义是「**加入**它」(扩展期望集)
    //    ⇒ 二者是**不同**操作 ⇒ 分列两个入口(禁双实现 ⇒ 本机械是**唯一**实现)。
    //
    // 4 步语义(per `openspec/changes/cpptlm-cep-tilelink-tree-manager` 的 `design.md` D-γ):
    //   ① 该地址**无** in-flight ⇒ 返 0(「无可加入」⇒ 调用方应改走 `snoop_begin_core`)
    //   ② 加入集含**未映射位** ⇒ 返 0(与 `snoop_begin_core` 同一拒绝语义)
    //   ③ 加入集与既有期望集**无新增位** ⇒ 返**既有 handle**(幂等)
    //   ④ 否则 ⇒ 把新增位并入 `expected` **与** `remaining` ⇒ `snoop_fold` 的归零逻辑**不变**
    //
    // ⚠️ **不产生 IC3 生命周期事件** —— join **不是**一次新的 snoop begin,而是既有 snoop 的
    //    期望集扩展 ⇒ 事件流语义不变(`MergeOnSnoopBegin` 的计数不受影响)。
    void snoop_join_core(std::uint64_t addr, responder_set_t extra_responders,
                         snoop_handle_t& handle) {
        handle = snoop_handle_t{0};
        // ① 该地址**无** in-flight ⇒ 无可加入
        auto sit = inflight_by_addr_.find(addr);
        if (sit == inflight_by_addr_.end())
            return;
        auto fit = inflight_.find(sit->second);
        if (fit == inflight_.end())
            return;
        // ② 加入集含**未映射位** ⇒ 与 `snoop_begin_core` 同一拒绝语义
        if ((extra_responders & ~kMappedResponderMask) != 0)
            return;
        // ③ 无新增位 ⇒ 幂等(返既有 handle,不重复扩张)
        const responder_set_t added = extra_responders & ~fit->second.expected;
        if (added == 0) {
            handle = sit->second;
            return;
        }
        // ④ 并入 `expected`(幂等判定基准)**与** `remaining`(归零判定基准)
        //    ⇒ `snoop_fold_core` 的归零逻辑**完全不变** ⇒ 零重复实现
        fit->second.expected |= extra_responders;
        fit->second.remaining |= added;
        handle = sit->second;
    }

    // ── snoop_fold 的公共实现 ───────────────────────────────────────────
    void snoop_fold_core(snoop_handle_t handle, node_id_t node_id, SnoopState resp_state,
                         bool dirty, bool data_supplied, responder_set_t& remaining_mask,
                         MergedState& merged_state) {
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

    // ── observe_snoop 的公共实现 ────────────────────────────────────────
    void observe_snoop_core(std::uint64_t addr, node_id_t from_node,
                            SnoopState resulting_local_state) {
        // 被动记账:同 (addr, from_node, state) 重复调用 ⇒ 同一投影(IC2 幂等)
        observed_[addr][from_node] = resulting_local_state;
    }

    // ── 观测(测试夹具 / 调用方诊断)──
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

protected:
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

    /// 掩码中最低置位的 bit 序号(无则 `kNoBit`)
    static unsigned first_bit(responder_set_t mask) {
        for (unsigned b = 0; b < 64; ++b) {
            if ((mask & (responder_set_t{1} << b)) != 0)
                return b;
        }
        return kNoBit;
    }

    std::map<snoop_handle_t, Inflight> inflight_;
    std::map<std::uint64_t, snoop_handle_t> inflight_by_addr_;
    std::map<std::uint64_t, std::map<node_id_t, SnoopState>> observed_;
    std::vector<LifecycleEvent> events_;
    snoop_handle_t next_handle_ = 1;
};

// ─────────────────────────────────────────────────────────────────────
// FullMapDirectory —— 行为实现之一(completeness = EXACT,无 broadcast 分支)
// ─────────────────────────────────────────────────────────────────────
// 状态模型:per addr → {sharers, dirty} 两个 64-bit bitmask(**精确集**,可超集?不 ——
// FullMap 是 EXACT,不做保守超集)。`observe_snoop` 记的是「peer 对某节点的认知」,
// 与 `dir_` 正交存放,不参与判断(per ADR §3.4「被动记账」)。
//
// ⚠️ **单级**目录:全部 sharer 落在**同一张**表里 ⇒ 只有**一个**视图。
//    对比:`HierarchicalDirectory`(同头文件)按**层级**分解 ⇒ 视图可**逐级收窄**。
class FullMapDirectory final : public SharerTrackerIf, public SnoopMechanics {
public:
    static constexpr TrackerImpl kImpl = TrackerImpl::FullMapDirectory;

    /// handle 空间大小(handle = 1..65535;0 保留为「无效」)。
    /// 16-bit 是 `ADR-CEP-04` 的 RTL 契约(`typedef logic [15:0] snoop_handle_t`),不可 widen。
    static constexpr std::size_t kHandleSpace = SnoopMechanics::kHandleSpace;

    // ⚠️ **caps / behavior 现存于基类** `SharerTrackerIf`(2026-11-01 接口面扩展)
    //    ⇒ 本类**不再另存副本**(存储唯一)⇒ `impl.caps` 写法仍可用(继承而来)。
    // ⚠️ **取** `kTrackerTraits[0]`(traits 行已存在)⇒ 本类**不**另写一份 caps 字面量
    //    ⇒ `ADR-CEP-04 §2` / `§4` 的冻结契约**被消费**,不重推。
    FullMapDirectory() {
        caps = kTrackerTraits[0].caps;
        behavior = kTrackerTraits[0].behavior;
    }

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
        snoop_begin_core(addr, expected_responders, handle);
    }

    void snoop_fold(snoop_handle_t handle, node_id_t node_id, SnoopState resp_state, bool dirty,
                    bool data_supplied, responder_set_t& remaining_mask,
                    MergedState& merged_state) override {
        snoop_fold_core(handle, node_id, resp_state, dirty, data_supplied, remaining_mask,
                        merged_state);
    }

    void observe_snoop(std::uint64_t addr, node_id_t from_node,
                       SnoopState resulting_local_state) override {
        observe_snoop_core(addr, from_node, resulting_local_state);
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

private:
    struct Entry {
        responder_set_t sharers = 0;
        responder_set_t dirty = 0;
    };

    std::map<std::uint64_t, Entry> dir_;
};

// ─────────────────────────────────────────────────────────────────────
// SparseSnoopFilter —— 第 3 种行为实现(CHI SF / CXL 2.0 Host SF)
// ─────────────────────────────────────────────────────────────────────
// **保守型 snoop filter**:home 侧维护「**可能**持有某行的节点集合」(位集 = sharer 集的**上界**)。
//
// ⚠️ **CONSERVATIVE 的确切含义(不得含糊,否则实现会过度声称)**:
//    `completeness = CONSERVATIVE` ⇒ filter **不能**回答「**没有**其他 sharer」
//    ⇒ 本实现 **永不**返回 `LookupResult::ANSWER_HERE`。
//    它能给的只有:①「**可能**有别人」⇒ `BROADCAST` ②「**可能**有别人且只有一个脏持有者」
//    ⇒ `FORWARD_TO` ③ 同地址 snoop 在飞 ⇒ `STALL`。
//    ⇒ **漏探**(漏掉一个真实 sharer)是**正确性失败**;**多探**只是代价 ⇒ 恒偏保守。
//
// ⚠️ **与 `FullMapDirectory` 的可观测差异(本实现**不是**它的克隆)**:
//    EXACT 目录**能**回答「没有其他 sharer」;CONSERVATIVE filter **不能** ⇒ 行为可判别。
//
// ⚠️ **收缩的合法来源**:`observe_snoop` 观测到该节点转为 `I`,或调用方**确知**地
//    `clear_sharer` ⇒ 其它路径**不得**收缩(filter 更新滞后只会造成**假阳性**,
//    而假阳性在 CONSERVATIVE 下**可接受** —— 漏探不可接受)。
//
// ⚠️ **归属**:本实现**不服务** CXL 实例(CXL.cache 的 peer 集平凡为空 ⇒ 其
//    `sharer_tracker` 为「不参与」);它服务 **`moesi_gpu`** 的 `O→WRITEBACK` 投影损失补偿
//    (台账 §8.1 **R11**,per Entry #124)。
class SparseSnoopFilter final : public SharerTrackerIf, public SnoopMechanics {
public:
    static constexpr TrackerImpl kImpl = TrackerImpl::SparseSnoopFilter;

    /// handle 空间大小(与另两个实现同 —— `ADR-CEP-04` 的 RTL 契约,不可 widen)
    static constexpr std::size_t kHandleSpace = SnoopMechanics::kHandleSpace;

    // ⚠️ **caps / behavior 现存于基类** `SharerTrackerIf`(2026-11-01 接口面扩展)
    //    ⇒ 本类**不再另存副本**(存储唯一)⇒ `impl.caps` 写法仍可用(继承而来)。
    // ⚠️ **取** `kTrackerTraits[1]`(traits 行已存在)⇒ 本类**不**另写一份 caps 字面量
    //    ⇒ `ADR-CEP-04 §2` / `§4` 的冻结契约**被消费**,不重推。
    SparseSnoopFilter() {
        caps = kTrackerTraits[1].caps;
        behavior = kTrackerTraits[1].behavior;
    }

    // ── 安装面(记录「**可能**持有」)──

    /// 记录「**可能**持有该行」 —— CONSERVATIVE 下**只增不减**是安全的。
    /// 未映射节点 ⇒ **显式 no-op**(不留下可见空条目,同 `FullMapDirectory` 的同型纪律)。
    void set_sharer(std::uint64_t addr, node_id_t node, SnoopState state) {
        const responder_set_t m = responder_mask_of(node);
        if (m == 0)
            return;
        Entry& e = filter_[addr];
        e.sharers |= m;
        if (state == SnoopState::M)
            e.dirty |= m;
        else
            e.dirty &= ~m;
    }

    /// 依据**观测**确认该节点已放弃该行 ⇒ **合法收缩**。
    void note_sharer_dropped(std::uint64_t addr, node_id_t node) {
        auto it = filter_.find(addr);
        if (it == filter_.end())
            return;
        const responder_set_t m = responder_mask_of(node);
        if (m == 0)
            return;
        if ((it->second.sharers & m) == 0 && (it->second.dirty & m) == 0)
            return;  // 本非 sharer ⇒ no-op(避免伪触发 Retire,同 FullMap 的同型纪律)
        it->second.sharers &= ~m;
        it->second.dirty &= ~m;
        if (it->second.sharers == 0 && it->second.dirty == 0) {
            filter_.erase(it);
            events_.push_back(LifecycleEvent::RetireOnZeroSharers);
        }
    }

    /// 调用方**确知**地清除(⚠️ 误用 ⇒ 漏探风险 ⇒ SHALLOW 前须有确证)。
    void clear_sharer(std::uint64_t addr, node_id_t node) { note_sharer_dropped(addr, node); }

    // ── 4 个接口方法 ──

    void lookup(std::uint64_t addr, Intent intent, LookupResult& result, LocalView& view,
                LookupHazard& hazard) override {
        if (inflight_by_addr_.count(addr) != 0) {
            // 同地址 snoop 在飞:本实现的契约是 STALL(per `ADR-CEP-04 §4` 表首行)
            result = LookupResult::ANSWER_HERE;  // 无意义占位(视图在 STALL 下不成立)
            view = LocalView{};
            hazard = behavior.hazard_under_contention;
            return;
        }

        hazard = LookupHazard::NONE;
        auto it = filter_.find(addr);
        if (it == filter_.end()) {
            // ★ **CONSERVATIVE 保守默认**:filter 无此地址 ⇒ **不能**证明无人持有 ⇒ 广播
            view = LocalView{};
            result = LookupResult::BROADCAST;
            return;
        }

        view = LocalView{};
        view.valid = true;
        view.sharers = it->second.sharers;
        view.dirty = it->second.dirty;

        // 位集是 sharer 集的**上界** ⇒ 掩码只含 1 位 ⇒ 不可能有别的 sharer ⇒ 单点转发安全。
        if (intent == Intent::want_read && it->second.dirty != 0 &&
            bit_count(it->second.dirty) == 1 && bit_count(it->second.sharers) == 1) {
            view.forward_to = responder_node_of(first_bit(it->second.dirty));
            result = LookupResult::FORWARD_TO;
            return;
        }
        // 其余一切情形 ⇒ **广播**(含「可能多个 sharer」⇒ 单点转发会漏探)
        result = LookupResult::BROADCAST;
    }

    void snoop_begin(std::uint64_t addr, responder_set_t expected_responders,
                     snoop_handle_t& handle) override {
        snoop_begin_core(addr, expected_responders, handle);
    }

    void snoop_fold(snoop_handle_t handle, node_id_t node_id, SnoopState resp_state, bool dirty,
                    bool data_supplied, responder_set_t& remaining_mask,
                    MergedState& merged_state) override {
        snoop_fold_core(handle, node_id, resp_state, dirty, data_supplied, remaining_mask,
                        merged_state);
    }

    void observe_snoop(std::uint64_t addr, node_id_t from_node,
                       SnoopState resulting_local_state) override {
        observe_snoop_core(addr, from_node, resulting_local_state);
        // 观测到该节点转为 `I` ⇒ 它已放弃该行 ⇒ **合法收缩**(CONSERVATIVE 的唯一收缩来源)
        if (resulting_local_state == SnoopState::I) {
            note_sharer_dropped(addr, from_node);
        }
    }

    // ── 观测(测试夹具 / 调用方诊断)──

    LocalView view_of(std::uint64_t addr) const {
        LocalView v;
        auto it = filter_.find(addr);
        if (it == filter_.end())
            return v;
        v.valid = true;
        v.sharers = it->second.sharers;
        v.dirty = it->second.dirty;
        return v;
    }

    /// filter 当前**可能**持有的节点数(诊断用;`0` = 无条目 或 已确知为空)
    std::size_t possible_sharer_count(std::uint64_t addr) const {
        auto it = filter_.find(addr);
        if (it == filter_.end())
            return 0;
        return bit_count(it->second.sharers);
    }

    /// 位计数(CONSERVATIVE 判定用:唯一可能 sharer ⇒ 单点转发安全)
    static constexpr std::size_t bit_count(responder_set_t v) noexcept {
        std::size_t n = 0;
        while (v != 0) {
            v &= (v - 1);
            ++n;
        }
        return n;
    }

private:
    struct Entry {
        responder_set_t sharers = 0;  ///< **可能**持有该行的节点集合(sharer 集的**上界**)
        responder_set_t dirty = 0;    ///< **可能**的脏持有者集合
    };

    std::map<std::uint64_t, Entry> filter_;
};

// ─────────────────────────────────────────────────────────────────────
// HierarchicalDirectory —— 第 6 种实现(`ADR-CEP-04 §13` 的 R-1 裁决落地)
// ─────────────────────────────────────────────────────────────────────
// **分级目录型**:sharer 按**层级**归属(本仓模型:`L0` = 最细 / 离 sharer 最近,
// `L{kLevelCount-1}` = 最粗 / 全局),每一级维护**自有**视图;任一级视图是
// 「本级自有 + 所有更细级自有」的**并集** ⇒ **逐级上卷(roll-up)**。
//
// ⚠️ **与 `FullMapDirectory` 的可观测差异**(HD-02 的判据落点):
//   · `FullMapDirectory`:**单级** ⇒ 只有**一个**视图(`view_of`);
//   · `HierarchicalDirectory`:**多级** ⇒ `aggregate_view_at(addr, L)` 给出
//     **L 级的聚合视图**,它**可能严格窄于**根级视图(当 sharer 分居不同层时)
//     ⇒ 「`FullMapDirectory` 克隆 + 纸面 caps」在此**不可能**:克隆体**没有**
//     `aggregate_view_at` 这个**收窄**能力。
//
// ⚠️ **聚合是派生的,不是存储的** ⇒ 各级只存**自有**位掩码,聚合视图在查询时
//    按需 OR 出来 ⇒ **不可能出现「陈旧聚合」**(增量维护式聚合的经典失效模式)。
//
// ── 行为决策登记(per `ADR-CEP-04 §13` + 本 change 的 design `D-β`)────────
// ⚠️ **本实现的行为规约在 ArchForge 与本仓均不存在**(只有 caps 声明 + 一句
//    注释级描述)⇒ 以下决策**逐条**登记推导依据 + **待追认**标记 ⇒ 挂**跨仓追认债**
//    (类比规约层 D-α 的 35 格 `pending` + ArchForge `42 号` 逐格追认)。
//    ⇒ **不得**把自推语义写成「已裁决」。
//
//   | # | 决策点 | 依据 | 待追认 |
//   |---|--------|------|:-----:|
//   | B-1 | 层级数 = 3(`L0` 最细 → `L2` 最粗) | `sharer_tracker.hh:138-141` 注释级描述「L1 per-GPC / L2 per-GPU / L3 per-Compute-Tray」+ `ADR-CEP-04 §13` 的「层级深度是**实现参数**,不是能力属性」 | ✅ |
//   | B-2 | `LookupHazard::ESCALATE_UP` **不**使用 —— 本实现的 `lookup` 一律给**根级完整视图**;**收窄**视图由 `lookup_scoped` 显式提供 | `ADR-CEP-04 §2` 的 hazard 语义(4 值:`NONE`/`STALL`/`CHAIN`/`ESCALATE_UP`);本仓 `traits` 只声明 `STALL` ⇒ 引入 `ESCALATE_UP` 会让**声明与实现**脱节 | ✅ |
//   | B-3 | `snoop_fold` 的合并规则**复用** `SnoopMechanics::join_state`(与 `FullMapDirectory` **同源**) | `ADR-CEP-04 §4` 的 fold 语义**不随实现而异** | ✅ |
//   | B-4 | handle 空间满 ⇒ **拒绝**(返回 `handle = 0`),不覆盖存活 handle | 与 `FullMapDirectory` 同源(`SnoopMechanics::allocate_handle`) | ✅ |
//   | B-5 | **F 态(转发点)在 tracker 中不可分辨** —— `SnoopState` 只有 `I`/`S`/`E`/`M`,**无 F** ⇒ 「谁是转发点」对 tracker 不可见;本实现**只**用 `LocalView::forward_to` 承载**转发目标节点**,**转发点角色本身丢失** ⇒ 登记为「**转发点保真度缺口**」 | MESIF 规约的 `F` 态 = 单 Owner + 多 Sharer + 转发点(规约载体 `F` 态定义);`sharer_tracker.hh:128` 的 `SnoopState` 定义 | ✅ |
class HierarchicalDirectory final : public SharerTrackerIf, public SnoopMechanics {
public:
    static constexpr TrackerImpl kImpl = TrackerImpl::HierarchicalDirectory;

    /// handle 空间大小(与 `FullMapDirectory` 同契约 —— `ADR-CEP-04` 的 RTL 契约)
    static constexpr std::size_t kHandleSpace = SnoopMechanics::kHandleSpace;

    /// 层级数(`L0` 最细 → `L{kLevelCount-1}` 最粗)—— per 行为决策 **B-1**
    static constexpr std::size_t kLevelCount = 3;
    /// 根级(最粗 / 全局)索引
    static constexpr std::size_t kRootLevel = kLevelCount - 1;

    // ── 行为决策登记(机器可读的那一份;散文版见本类上方注释)──────────
    //
    // ⚠️ **为什么做成结构化字段**:本实现的行为规约**无真源** ⇒ 决策必须**可判据**,
    //    否则「登记了没有」与「登记了但 basis 是空的」都查不出来 ⇒ 跨仓追认债会
    //    退化成**不可审计的散文**。
    struct BehaviorDecision {
        const char* id;      ///< 决策编号(`B-1` …)
        const char* topic;   ///< 决策点
        const char* basis;   ///< 推导依据(**非空**;`basis` 缺失 ⇒ 判据必红)
        bool        pending; ///< 待追认标记(**本实现全部为 true**)
    };

    /// 逐条行为决策(per `ADR-CEP-04 §13` + 本 change 的 design `D-β`)
    static constexpr BehaviorDecision kBehaviorDecisions[] = {
        {"B-1", "层级数与层级语义(L0 最细 → L2 最粗)",
         "sharer_tracker.hh:138-141 的注释级描述(L1 per-GPC / L2 per-GPU / L3 per-Compute-Tray)"
         " + ADR-CEP-04 §13 的「层级深度是实现参数,不是能力属性」", true},
        {"B-2", "LookupHazard::ESCALATE_UP 不使用;收窄语义由 lookup_scoped 显式提供",
         "ADR-CEP-04 §2 的 hazard 语义(4 值)+ 本仓 traits 只声明 STALL"
         " ⇒ 引入 ESCALATE_UP 会让声明与实现脱节", true},
        {"B-3", "snoop_fold 的合并规则复用 SnoopMechanics::join_state(与 FullMapDirectory 同源)",
         "ADR-CEP-04 §4 的 fold 语义不随实现而异 ⇒ 禁双实现", true},
        {"B-4", "handle 空间满 ⇒ 拒绝(返回 handle = 0),不覆盖存活 handle",
         "与 FullMapDirectory 同源(SnoopMechanics::allocate_handle 的线性探测 + 满表拒绝)", true},
        {"B-5", "F 态(转发点)角色在 tracker 中不可分辨 —— 只承载转发目标节点",
         "sharer_tracker.hh:128 的 SnoopState 只有 I/S/E/M,无 F"
         " ⇒ 「谁是转发点」对 tracker 不可见 ⇒ 登记为转发点保真度缺口", true},
    };

    /// 行为决策条数
    static constexpr std::size_t kBehaviorDecisionCount =
        sizeof(kBehaviorDecisions) / sizeof(kBehaviorDecisions[0]);

    /// 行为决策表的**自洽谓词**(可变异形态 ⇒ 反向 fixture 可传「删了一条 / basis 置空」的副本)
    /// —— 四字段**全部必填**,且 `pending` **必须**为 true(自推语义**不得**写成「已裁决」)。
    static constexpr bool behavior_decisions_are_consistent(const BehaviorDecision* list,
                                                             std::size_t n) noexcept {
        for (std::size_t i = 0; i < n; ++i) {
            if (list[i].id == nullptr || list[i].id[0] == '\0')
                return false;
            if (list[i].topic == nullptr || list[i].topic[0] == '\0')
                return false;
            if (list[i].basis == nullptr || list[i].basis[0] == '\0')
                return false;
            if (!list[i].pending)
                return false;
        }
        return true;
    }

    /// 本实现的决策表 SHALL 自洽
    static constexpr bool behavior_decisions_are_consistent() noexcept {
        return behavior_decisions_are_consistent(kBehaviorDecisions, kBehaviorDecisionCount);
    }

    /// **F 态(转发点)角色可分辨?** —— **否**:`SnoopState` 无 F ⇒ 角色本身丢失
    /// ⇒ 只用 `LocalView::forward_to` 承载**转发目标节点**。
    /// ⇒ 登记为「**转发点保真度缺口**」(per 行为决策 **B-5**)。
    static constexpr bool kForwardPointRoleRepresentable = false;

    /// 转发点保真度缺口的登记措辞
    static constexpr const char* kForwardPointGapNote =
        "F 态(转发点)= 单 Owner + 多 Sharer + 一个转发点;但 SnoopState 只有 I/S/E/M,无 F"
        " ⇒ 「谁是转发点」对 tracker 不可分辨 ⇒ 本实现只承载转发目标节点"
        "(LocalView::forward_to),转发点角色本身丢失 ⇒ 转发点保真度缺口(待跨仓追认)。";

    // ⚠️ **caps / behavior 现存于基类** `SharerTrackerIf`(2026-11-01 接口面扩展)
    //    ⇒ 本类**不再另存副本**(存储唯一)⇒ `impl.caps` 写法仍可用(继承而来)。
    HierarchicalDirectory() {
        caps = kTrackerTraits[5].caps;
        behavior = kTrackerTraits[5].behavior;
    }

    /// traits 表第 6 行**必须**是本实现(索引写错 ⇒ 编译期红)
    static_assert(kTrackerTraits[5].impl == TrackerImpl::HierarchicalDirectory,
                  "kTrackerTraits[5] 必须是 HierarchicalDirectory 行");
    static_assert(kTrackerTraits[5].caps.aggregation_at == AggregationAt::HIERARCHICAL,
                  "HierarchicalDirectory 的 aggregation_at 必须是 HIERARCHICAL");



    // ── 目录安装(带层级)──
    /// 把 `node` 装到 `addr` 的**第 `level` 级**;`level` 非法 ⇒ **显式 no-op**
    /// (不得静默落到根级 —— 那会让「层级归属」不可辨)。
    void set_sharer(std::uint64_t addr, node_id_t node, std::size_t level, SnoopState state) {
        const responder_set_t m = responder_mask_of(node);
        if (m == 0)
            return;  // 未映射节点 ⇒ 显式 no-op(不造空条目)
        if (level >= kLevelCount)
            return;  // 非法层级 ⇒ 显式 no-op
        LevelEntry& e = own_[level][addr];
        e.sharers |= m;
        if (state == SnoopState::M)
            e.dirty |= m;
        else
            e.dirty &= ~m;
    }

    /// 从第 `level` 级移除 `node`
    void clear_sharer(std::uint64_t addr, node_id_t node, std::size_t level) {
        if (level >= kLevelCount)
            return;
        auto it = own_[level].find(addr);
        if (it == own_[level].end())
            return;
        const responder_set_t m = responder_mask_of(node);
        if (m == 0)
            return;
        if ((it->second.sharers & m) == 0 && (it->second.dirty & m) == 0)
            return;  // 本非持有者 ⇒ no-op(不伪触发 retire)
        it->second.sharers &= ~m;
        it->second.dirty &= ~m;
        if (it->second.sharers == 0 && it->second.dirty == 0) {
            own_[level].erase(it);
            events_.push_back(LifecycleEvent::RetireOnZeroSharers);
        }
    }

    // ── 4 个接口方法 ──

    void lookup(std::uint64_t addr, Intent intent, LookupResult& result, LocalView& view,
                LookupHazard& hazard) override {
        if (inflight_by_addr_.count(addr) != 0) {
            // 同地址 snoop 在飞:契约是 STALL(ADR §4 表首行),view 无意义
            result = LookupResult::ANSWER_HERE;
            view = LocalView{};
            hazard = behavior.hazard_under_contention;
            return;
        }

        const bool existed = has_entry(addr);
        if (!existed)
            events_.push_back(LifecycleEvent::AllocateOnFirstForward);

        hazard = LookupHazard::NONE;
        view = aggregate_view_at(addr, kRootLevel);
        view.valid = true;

        // 分级目录:要取数 ⇒ 找 dirty 持有者(根级聚合 ⇒ 完整视图,不会漏 snoop)
        if (intent == Intent::want_read && view.dirty != 0) {
            const unsigned b = first_bit(view.dirty);
            view.forward_to = responder_node_of(b);
            result = LookupResult::FORWARD_TO;
            return;
        }
        result = LookupResult::ANSWER_HERE;
    }

    void snoop_begin(std::uint64_t addr, responder_set_t expected_responders,
                     snoop_handle_t& handle) override {
        snoop_begin_core(addr, expected_responders, handle);
    }

    void snoop_fold(snoop_handle_t handle, node_id_t node_id, SnoopState resp_state, bool dirty,
                    bool data_supplied, responder_set_t& remaining_mask,
                    MergedState& merged_state) override {
        snoop_fold_core(handle, node_id, resp_state, dirty, data_supplied, remaining_mask,
                        merged_state);
    }

    void observe_snoop(std::uint64_t addr, node_id_t from_node,
                       SnoopState resulting_local_state) override {
        observe_snoop_core(addr, from_node, resulting_local_state);
    }

    // ── 层级视图(多级聚合的**可观测面**)──

    /// 根级(全局)聚合视图 —— 与 `FullMapDirectory::view_of` **同口径**(完整视图)
    LocalView view_of(std::uint64_t addr) const { return aggregate_view_at(addr, kRootLevel); }

    /// 第 `level` 级的**聚合**视图 = 本级自有 + 所有**更细**级自有(逐级上卷)
    /// —— `level` 非法 ⇒ 返回**无效**视图(不静默落到根级)。
    LocalView aggregate_view_at(std::uint64_t addr, std::size_t level) const {
        LocalView v;
        if (level >= kLevelCount)
            return v;
        v.valid = true;
        for (std::size_t l = 0; l <= level; ++l) {
            auto it = own_[l].find(addr);
            if (it == own_[l].end())
                continue;
            v.sharers |= it->second.sharers;
            v.dirty |= it->second.dirty;
        }
        return v;
    }

    /// 第 `level` 级的**自有**视图(只含**恰好**装在该级的 sharer)
    LocalView own_view_at(std::uint64_t addr, std::size_t level) const {
        LocalView v;
        if (level >= kLevelCount)
            return v;
        auto it = own_[level].find(addr);
        if (it == own_[level].end())
            return v;
        v.valid = true;
        v.sharers = it->second.sharers;
        v.dirty = it->second.dirty;
        return v;
    }

    /// **限定层级**的 lookup:从 `level` 起**逐级上卷**,直到找到**持有该地址**的层级
    /// ⇒ 返回**该级**的聚合视图(**可能严格窄于**根级视图)⇒ 收窄能力可观测。
    /// 找不到 ⇒ 落回根级完整视图。
    ///
    /// ⚠️ 行为决策 **B-2**:`lookup` 本身**不**使用 `ESCALATE_UP`;**收窄**语义
    ///    由本方法**显式**提供 ⇒ `traits` 声明的 hazard(`STALL`)与实现**一致**。
    LocalView lookup_scoped(std::uint64_t addr, std::size_t level) const {
        for (std::size_t l = level; l < kLevelCount; ++l) {
            if (has_entry_at(addr, l))
                return aggregate_view_at(addr, l);
        }
        return aggregate_view_at(addr, kRootLevel);
    }

    /// 该地址在**任一级**是否有条目
    bool has_entry(std::uint64_t addr) const {
        for (std::size_t l = 0; l < kLevelCount; ++l)
            if (own_[l].count(addr) != 0)
                return true;
        return false;
    }

    /// 该地址在**指定级**是否有条目
    bool has_entry_at(std::uint64_t addr, std::size_t level) const {
        if (level >= kLevelCount)
            return false;
        return own_[level].count(addr) != 0;
    }

private:
    struct LevelEntry {
        responder_set_t sharers = 0;
        responder_set_t dirty = 0;
    };

    /// 每级**自有**视图(聚合视图在查询时派生 ⇒ 无陈旧聚合)
    std::map<std::uint64_t, LevelEntry> own_[kLevelCount];
};


}  // namespace tlm::cep

#endif  // CPPTLM_CEP_SHARER_TRACKER_HH
