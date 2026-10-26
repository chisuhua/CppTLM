// include/tlm/cep/collective_plane.hh
// Fabric Collective Plane —— CEP 的**旁路平面**(per `ADR-CEP-01 Decision 1`)
//
// 功能:把 `ADR-CEP-03` 的定义(7 方法接口 / 4 原语最小集 / 3 变体事件 / group 生命周期 /
//       region-epoch 两阶段 / BARRIER rendezvous)实现为**可执行**的契约。
// 作者: CppTLM Team / 日期: 2026-10-26
//
// ── 定位(per `ADR-CEP-01 §1.2`)────────────────────────────────────
// 集合通信原语**不是**传统缓存一致性事务:
//   · 终结点 = "reduce result" / "broadcast completion",不是 cache line 状态
//   · 完成语义 = **group-scoped**(全组 ack),不是 requester-scoped(单点 ack)
//   · 生命周期 = per-op(短),与目录状态(per-line,长)不匹配
// ⇒ 故**不**并入 Directory & Ordering Plane(会腐蚀 per-address 串行化不变量),
//   也**不**并入 Coherence Interface 事务集(会破坏 canonical 10 条流不变量)。
//
// ── 平面关系(硬约束,per `ADR-CEP-01 §3.3` 抓取点 1)────────────────
// 本平面与 Directory & Ordering Plane **横向并列、彼此零调用边**。
// 本文件**不得** include directory / ordering / sharer 侧的头 —— 由
// `test_cep_collective_plane_base.cc` 去注释后静态扫描 + 反向 fixture 守住。
// R3 的 drain 由**发起注册的 node 端**经**自身** Coherence Interface 端口发出;
// 本平面**只输出** invalidate 请求清单(per `ADR-CEP-03 §3.4`)。
//
// ── 集成面(per delta capability `cep-collective-plane-base`)────────
//   · **不**注册 23 ABI(per `ADR-DGPU-08`);**不**经 `ModuleFactory` 注册
//     (本类是 **testbench 定位**的组件,由测试夹具装配;per `07 号 §6.3` 最终形态口径);
//   · **不**改动任何 frozen header;**不**引入 `include/tlm/cep/` 之外的依赖。
//
// ── 与 canonical 10 流的关系(per `ADR-CEP-02 §2`)──────────────────
// 本平面**不**新增/不改 canonical 流。`group_id` / `member_set_t` / `CollectiveReq` /
// `CollectiveEvt` 等 group-scoped 字段**不得**出现在 Coherence Interface 侧
// (完成域判别规则的**反向**面,per `ADR-CEP-01 Decision 2`)。

#ifndef INCLUDE_TLM_CEP_COLLECTIVE_PLANE_HH_
#define INCLUDE_TLM_CEP_COLLECTIVE_PLANE_HH_

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace tlm::cep {

// ─────────────────────────────────────────────────────────────────────
// 基础类型
// ─────────────────────────────────────────────────────────────────────

/// group 标识(plane 的一等公民,per `ADR-CEP-03 §2 Decision 2`)
using group_id_t = std::uint32_t;

/// 成员集合(节点编号的集合)
using member_set_t = std::set<std::uint32_t>;

/// 相位 / 轮次标签(同相位内无序、跨相位有序,per `ADR-CEP-03 §2`)
using phase_t = std::uint32_t;

/// 操作标签(per-op 生命周期内的区分)
using tag_t = std::uint32_t;

/// 能力位图(per `ADR-CEP-03 §2` 的 `query_capability` 返回)
using cap_t = std::uint64_t;

/// 无效 group 哨兵(`group_create` 失败时返回)
inline constexpr group_id_t kInvalidGroupId = 0xFFFFFFFFu;

/// 无效成员
inline constexpr std::uint32_t kInvalidMember = 0xFFFFFFFFu;

/// 归约算子(per `ADR-CEP-03 §2` 的 `reduce_op` 字段)
enum class ReduceOp : std::uint8_t {
    Sum = 0,
    Min = 1,
    Max = 2,
    BitAnd = 3,
    BitOr = 4,
    BitXor = 5,
};

/// 数据类型(per `ADR-CEP-03 §2` 的 `dtype` 字段)
enum class DataType : std::uint8_t {
    U8 = 0,
    U32 = 1,
    U64 = 2,
    F32 = 3,
    F64 = 4,
};

/// **原语最小集 = 恰好 4 个**(per `ADR-CEP-03 §2`)。
/// AllReduce **不列为原语** —— 它是 REDUCE + BROADCAST 的**复合**。
enum class CollectiveOp : std::uint8_t {
    REDUCE = 0,
    BROADCAST = 1,
    MULTICAST = 2,
    BARRIER = 3,
};

inline constexpr std::size_t kCollectiveOpCount = 4;

inline const char* collective_op_name(CollectiveOp op) {
    switch (op) {
        case CollectiveOp::REDUCE:
            return "REDUCE";
        case CollectiveOp::BROADCAST:
            return "BROADCAST";
        case CollectiveOp::MULTICAST:
            return "MULTICAST";
        case CollectiveOp::BARRIER:
            return "BARRIER";
    }
    return "";
}

/// region 的访问模式(per `ADR-CEP-03 §3.2` 的 R3 降级选项)
enum class AccessMode : std::uint8_t {
    /// 普通 collective 访问(区域仍可被 coherence 缓存,但已注册 ⇒ 走 divert)
    Normal = 0,
    /// `NC_DROP_CACHEABLE` —— 注册时对区域做 drain 并在 coherence plane 降级为 non-cacheable
    NcDropCacheable = 1,
};

/// 地址区间(闭区间 [lo, hi])
struct AddrRange {
    std::uint64_t lo = 0;
    std::uint64_t hi = 0;

    bool valid() const { return lo <= hi; }
    bool contains(std::uint64_t addr) const { return addr >= lo && addr <= hi; }
    /// 与另一区间是否**重叠**(含端点)
    bool overlaps(const AddrRange& o) const { return lo <= o.hi && o.lo <= hi; }

    /// map key 所需的**全序**(按 (lo, hi) 字典序)
    bool operator<(const AddrRange& o) const {
        if (lo != o.lo)
            return lo < o.lo;
        return hi < o.hi;
    }
    bool operator==(const AddrRange& o) const { return lo == o.lo && hi == o.hi; }
};

/// group 属性(per `ADR-CEP-03 §2` 的 `group_attrs_t`)
struct GroupAttrs {
    std::string name;
    // 注:`collective_enabled` 曾在此处声明,但**全仓无一处读取** ⇒ 属「声称的契约未实施」,
    //   已删除(per critic NIT)。「未注册 region 的 group 不可操作」由
    //   `send` 的 NoRegion 分支**直接**保证,不靠属性开关。
};

/// 状态码 —— **「不支持」的返回通道**(per design D-ι 裁决 3)
enum class Status : std::uint8_t {
    Ok = 0,
    /// 原语在此实现上**不支持**(per `INV-CEP-07-IC3` 的 NullCollectivePlane)
    Unsupported = 1,
    /// 数据路径(REDUCE / BROADCAST / MULTICAST)在本实现上**未实现**
    DataPathNotImplemented = 2,
    /// 集合 plane 操作只允许作用于**已注册 region**(per `ADR-CEP-03 §3.4`)
    GroupRegisterError = 3,
    InvalidGroup = 4,
    InvalidRange = 5,
    /// 区域重叠 ⇒ 违反区域互斥(per CEP-13)
    RegionOverlap = 6,
    /// drain 失败 ⇒ **不建 epoch**(per CEP-13)
    DrainFailed = 7,
    UnknownOp = 8,
    EmptyMemberSet = 9,
    /// 当前成员**不属于**该 group(barrier 到达不得被计入阈值)
    NotMember = 10,
    /// `recv` 时**无事件可取**(与「取到事件」必须可区分)
    NoEvent = 11,
};

inline const char* status_name(Status s) {
    switch (s) {
        case Status::Ok:
            return "ok";
        case Status::Unsupported:
            return "unsupported";
        case Status::DataPathNotImplemented:
            return "data-path-not-implemented";
        case Status::GroupRegisterError:
            return "group-register-error";
        case Status::InvalidGroup:
            return "invalid-group";
        case Status::InvalidRange:
            return "invalid-range";
        case Status::RegionOverlap:
            return "region-overlap";
        case Status::DrainFailed:
            return "drain-failed";
        case Status::UnknownOp:
            return "unknown-op";
        case Status::EmptyMemberSet:
            return "empty-member-set";
        case Status::NotMember:
            return "not-member";
        case Status::NoEvent:
            return "no-event";
    }
    return "";
}

/// group 归属判定(per design D-ι 裁决 1 —— R2 谓词的**判读侧**)
///
/// ⚠️ 本判定是**判读**,不是**接线** —— 真正把判定接到 fabric 路由上的
/// `fabric_ingress_classify` 属 **P3**(`cpptlm-cep-nvlink-sharp-adapter`)。
enum class RegionState : std::uint8_t {
    /// 地址未命中任何 collective region ⇒ **不 divert**(走 coherence 路径)
    NoRegion = 0,
    /// 地址命中 region,但其 group 的 **epoch 尚未 commit** ⇒ **不 divert**
    /// (per `ADR-CEP-03 §3.1` R4「epoch 未 commit 前 = 正常服务」)
    RegisteredEpochInvalid = 1,
    /// 地址命中 region 且 **epoch 已生效** ⇒ **divert** 到 collective plane
    RegisteredActive = 2,
};

inline const char* region_state_name(RegionState s) {
    switch (s) {
        case RegionState::NoRegion:
            return "no-region";
        case RegionState::RegisteredEpochInvalid:
            return "registered-epoch-invalid";
        case RegionState::RegisteredActive:
            return "registered-active";
    }
    return "";
}

/// 请求载荷描述(per `ADR-CEP-03 §2`:addr+len | inline)
struct PayloadDesc {
    /// 载荷所在地址(非 inline 时有效)
    std::uint64_t addr = 0;
    /// 载荷长度(字节)
    std::uint32_t len = 0;
    /// 是否为 inline 载荷
    bool inline_payload = false;
    /// inline 载荷(仅 `inline_payload == true` 时有效)
    std::uint64_t inline_value = 0;
};

/// 集合请求(per `ADR-CEP-03 §2` 的「plane 的通用词汇」)
struct CollectiveReq {
    group_id_t group = kInvalidGroupId;
    CollectiveOp op = CollectiveOp::BROADCAST;
    tag_t tag = 0;
    std::uint32_t src_member = kInvalidMember;
    PayloadDesc payload;
    ReduceOp reduce_op = ReduceOp::Sum;
    DataType dtype = DataType::U64;
    /// 组播掩码(per `MULTICAST`)
    std::uint64_t multicast_mask = 0;
};

/// 集合事件 —— **恰好 3 变体**(per `ADR-CEP-03 §2`)
///
/// `Fault` 变体是 **partial failure 的显式建模**载体(per `ADR-CEP-01 §3.3` 抓取点 6):
/// 缺它 ⇒ 成员掉线会变成不可见死锁点。
class CollectiveEvt {
public:
    enum class Kind : std::uint8_t {
        Result = 0,
        Completion = 1,
        Fault = 2,
    };

    static CollectiveEvt make_result(group_id_t gid, tag_t tag, std::uint64_t payload) {
        CollectiveEvt e;
        e.kind_ = Kind::Result;
        e.group_ = gid;
        e.tag_ = tag;
        e.payload_ = payload;
        return e;
    }
    static CollectiveEvt make_completion(group_id_t gid, tag_t tag) {
        CollectiveEvt e;
        e.kind_ = Kind::Completion;
        e.group_ = gid;
        e.tag_ = tag;
        return e;
    }
    static CollectiveEvt make_fault(group_id_t gid, std::uint32_t culprit, Status cause) {
        CollectiveEvt e;
        e.kind_ = Kind::Fault;
        e.group_ = gid;
        e.culprit_ = culprit;
        e.cause_ = cause;
        return e;
    }

    Kind kind() const { return kind_; }
    group_id_t group() const { return group_; }
    tag_t tag() const { return tag_; }
    std::uint64_t payload() const { return payload_; }
    std::uint32_t culprit() const { return culprit_; }
    Status cause() const { return cause_; }

    bool is_fault() const { return kind_ == Kind::Fault; }

    const char* kind_name() const {
        switch (kind_) {
            case Kind::Result:
                return "Result";
            case Kind::Completion:
                return "Completion";
            case Kind::Fault:
                return "Fault";
        }
        return "";
    }

private:
    Kind kind_ = Kind::Completion;
    group_id_t group_ = kInvalidGroupId;
    tag_t tag_ = 0;
    std::uint64_t payload_ = 0;
    std::uint32_t culprit_ = kInvalidMember;
    Status cause_ = Status::Ok;
};

inline constexpr std::size_t kCollectiveEvtKindCount = 3;

// ─────────────────────────────────────────────────────────────────────
// 单接口(恰好 7 个虚方法,逐字对齐 `ADR-CEP-03 §2 Decision 2`)
// ─────────────────────────────────────────────────────────────────────
class CollectivePlaneIf {
public:
    virtual ~CollectivePlaneIf() = default;

    // ---- group 生命周期(慢路径,管理语义)----
    virtual group_id_t group_create(member_set_t members, const GroupAttrs& attrs) = 0;
    virtual void group_destroy(group_id_t gid) = 0;
    // SHARP 式注册:注册时触发 coherence drain(跨 plane 契约的入口)
    virtual Status group_register_region(group_id_t gid, const AddrRange& range,
                                         AccessMode mode) = 0;

    // ---- 数据路径原语 ----
    virtual void send(const CollectiveReq& req) = 0;
    virtual void recv(CollectiveEvt& evt) = 0;

    // ---- barrier = 相位转换原语 ----
    virtual void collective_barrier(group_id_t gid, phase_t phase) = 0;

    // ---- 能力协商 ----
    virtual cap_t query_capability(CollectiveOp op, std::size_t max_bytes) const = 0;
};

// ─────────────────────────────────────────────────────────────────────
// NullCollectivePlane —— 默认实现(**行为化 `INV-CEP-07-IC3`**)
// ─────────────────────────────────────────────────────────────────────
// `ADR-CEP-07` 的 `INV-CEP-07-IC3`:`NullCollectivePlane` 是 Collective Plane 默认实现,
// 所有原语返回"不支持"。CHI / CXL 配置下的默认绑定(per `07 号 §6.4`)。
//
// 「不支持」经**状态查询口** `last_status()` 表达(per design D-ι 裁决 3)——
// `ADR-CEP-03 §2` 的伪码里 `send` / `recv` / `collective_barrier` 是 `task`(**无返回值**),
// 故必须有**可观测**的返回通道,否则该 invariant 无法被机器检查。
class NullCollectivePlane final : public CollectivePlaneIf {
public:
    group_id_t group_create(member_set_t members, const GroupAttrs& attrs) override {
        (void)members;
        (void)attrs;
        last_status_ = Status::Unsupported;
        return kInvalidGroupId;  // 哨兵:创建失败
    }

    void group_destroy(group_id_t gid) override {
        (void)gid;
        last_status_ = Status::Unsupported;
    }

    Status group_register_region(group_id_t gid, const AddrRange& range,
                                  AccessMode mode) override {
        (void)gid;
        (void)range;
        (void)mode;
        last_status_ = Status::Unsupported;
        return Status::Unsupported;
    }

    void send(const CollectiveReq& req) override {
        (void)req;
        last_status_ = Status::Unsupported;
    }

    void recv(CollectiveEvt& evt) override {
        (void)evt;
        last_status_ = Status::Unsupported;
    }

    void collective_barrier(group_id_t gid, phase_t phase) override {
        (void)gid;
        (void)phase;
        last_status_ = Status::Unsupported;
    }

    cap_t query_capability(CollectiveOp op, std::size_t max_bytes) const override {
        (void)op;
        (void)max_bytes;
        return 0;  // 「不支持」的能力位图 = 0
    }

    /// 状态查询口(只读辅助口,`CollectivePlaneIf` 7 方法之外)
    Status last_status() const { return last_status_; }

    /// 副作用观测:Null **不得**建 group / 不得投递事件
    std::size_t group_count() const { return groups_; }
    std::size_t pending_event_count() const { return events_; }

private:
    mutable Status last_status_ = Status::Unsupported;
    std::size_t groups_ = 0;
    std::size_t events_ = 0;
};

// ─────────────────────────────────────────────────────────────────────
// LocalCollectivePlane —— 行为实现(group 生命周期 / region-epoch / BARRIER)
// ─────────────────────────────────────────────────────────────────────
// 范围(per design D-α):group 生命周期 + region 表 + **epoch 两阶段** + BARRIER rendezvous。
// REDUCE / BROADCAST / MULTICAST 的**数据路径不实现** —— 它需要 fabric ingress 分类
// (per `ADR-CEP-03 §3.1` R2),属 **P3**。本实现对此**显式**报「数据路径未实现」,
// 而非静默(per design D-ι 裁决 4)。
class LocalCollectivePlane final : public CollectivePlaneIf {
public:
    group_id_t group_create(member_set_t members, const GroupAttrs& attrs) override {
        if (members.empty()) {
            last_status_ = Status::EmptyMemberSet;
            return kInvalidGroupId;
        }
        // ⚠️ 绕回时跳过 `kInvalidGroupId`(0xFFFFFFFF)—— 它是「无效 group」哨兵
        if (next_gid_ == kInvalidGroupId)
            next_gid_ = 1;
        const group_id_t gid = next_gid_++;
        groups_[gid] = Group{};
        groups_[gid].members = std::move(members);
        groups_[gid].attrs = attrs;
        groups_[gid].epoch_valid = false;
        last_status_ = Status::Ok;
        return gid;
    }

    void group_destroy(group_id_t gid) override {
        auto it = groups_.find(gid);
        if (it == groups_.end()) {
            last_status_ = Status::InvalidGroup;
            return;
        }
        // R4:group_destroy 后区域**回归 coherence 域** —— 即该 region **从表里移除**
        //   (只把 epoch 置 false 会留下**残留**:重叠检查遍历全表不分 gid 死活 ⇒
        //    该地址段被**永久报废**,新 group 再也注册不进去)
        for (auto rit = regions_.begin(); rit != regions_.end();) {
            if (rit->second.gid == gid)
                rit = regions_.erase(rit);
            else
                ++rit;
        }
        // 对 in-flight 操作发 Fault 且**不阻塞**(design D-ζ)
        for (auto& [g, st] : barrier_state_) {
            if (g != gid)
                continue;
            for (const auto& [phase, members] : st.arrived) {
                (void)phase;
                for (std::uint32_t m : members)
                    events_.push_back(CollectiveEvt::make_fault(gid, m, Status::InvalidGroup));
            }
        }
        barrier_state_.erase(gid);
        groups_.erase(it);
        last_status_ = Status::Ok;
    }

    Status group_register_region(group_id_t gid, const AddrRange& range,
                                  AccessMode mode) override {
        auto it = groups_.find(gid);
        if (it == groups_.end()) {
            last_status_ = Status::InvalidGroup;
            return Status::InvalidGroup;
        }
        if (!range.valid()) {
            last_status_ = Status::InvalidRange;
            return Status::InvalidRange;
        }
        // 区域互斥(per CEP-13):重叠注册 ⇒ 报错,**不**建 epoch
        for (const auto& [existing, reg] : regions_) {
            (void)reg;
            if (existing.overlaps(range)) {
                last_status_ = Status::RegionOverlap;
                return Status::RegionOverlap;
            }
        }
        Region reg;
        reg.gid = gid;
        reg.mode = mode;
        reg.epoch_valid = false;  // epoch 两阶段:register 后仍 invalid
        regions_[range] = reg;
        last_status_ = Status::Ok;
        return Status::Ok;
    }

    void send(const CollectiveReq& req) override {
        const auto st = region_state_of_payload(req);
        if (st == RegionState::NoRegion) {
            // 未注册区域 ⇒ 立即报错,不静默(per `ADR-CEP-03 §3.4`)
            // ⚠️ **不得投递任何事件** —— spec 明文禁止;错误只经**状态**传达
            //   (事件通道留给「已注册但被中断」这类真正的 partial failure)
            last_status_ = Status::GroupRegisterError;
            return;
        }
        if (st == RegionState::RegisteredEpochInvalid) {
            // epoch 未 commit ⇒ 正常服务窗口(R4),但 collective 数据路径**不执行**
            last_status_ = Status::GroupRegisterError;
            return;
        }
        // 已注册 + epoch 生效 ⇒ 数据路径**未实现**(显式,不静默)
        last_status_ = Status::DataPathNotImplemented;
    }

    void recv(CollectiveEvt& evt) override {
        if (events_.empty()) {
            // ⚠️ **无事件**必须与「取到事件」**可区分**(轮询式消费者会误读):
            //   清空出参 + 置 `NoEvent` 状态。
            evt = CollectiveEvt::make_completion(kInvalidGroupId, 0);
            last_status_ = Status::NoEvent;
            return;
        }
        evt = events_.front();
        events_.erase(events_.begin());
        last_status_ = Status::Ok;
    }

    void collective_barrier(group_id_t gid, phase_t phase) override {
        auto it = groups_.find(gid);
        if (it == groups_.end()) {
            last_status_ = Status::InvalidGroup;
            return;
        }
        if (current_member_ == kInvalidMember) {
            // 无成员身份 ⇒ 无法计入 rendezvous(显式报错,不静默)
            last_status_ = Status::NotMember;
            return;
        }
        // ⚠️ **成员身份校验**:非成员到达**不得**计入阈值 ——
        //   否则「N-1 不释放 / N 释放」被绕过(非成员到达 + N-2 个真成员即释放),
        //   破掉本 plane 唯一的 ordering 承诺。
        if (it->second.members.count(current_member_) == 0) {
            last_status_ = Status::NotMember;
            return;
        }
        BarrierState& st = barrier_state_[gid];
        // 跨相位有序:相位 N+1 的到达**不得**使 N 提前释放 ⇒ 按相位分别计到达成员
        st.arrived[phase].insert(current_member_);
        // N-1 到达不释放;N 到达释放
        if (st.arrived[phase].size() >= it->second.members.size()) {
            for (std::uint32_t m : it->second.members)
                events_.push_back(CollectiveEvt::make_completion(gid, phase));
            st.arrived.erase(phase);
        }
        last_status_ = Status::Ok;
    }

    cap_t query_capability(CollectiveOp op, std::size_t max_bytes) const override {
        (void)max_bytes;
        switch (op) {
            case CollectiveOp::BARRIER:
                return kCapBarrier;
            case CollectiveOp::REDUCE:
            case CollectiveOp::BROADCAST:
            case CollectiveOp::MULTICAST:
                // 数据路径未实现 ⇒ 能力位为 0(「不支持」),**不**谎报支持
                return 0;
        }
        return 0;
    }

    // ── 装配 / 驱动用辅助口(不在 7 方法接口面内)────────────────────

    /// **当前成员身份**的装配缝 —— `collective_barrier(gid, phase)` 的签名里
    /// **没有** member 参数(逐字对齐 `ADR-CEP-03 §2`),而 rendezvous 的 N-1/N 判定
    /// 必须知道「谁到了」⇒ 由调用侧(每个 endpoint)先行声明自己的成员身份。
    /// 与 `complete_drain` 同属**装配/驱动用辅助口**,不在 7 方法接口面内。
    Status set_current_member(std::uint32_t member) {
        current_member_ = member;
        last_status_ = Status::Ok;
        return Status::Ok;
    }

    std::uint32_t current_member() const { return current_member_; }

    /// **drain 完成回告** —— epoch 由 invalid 转 valid(per design D-ι 裁决 2)。
    /// 先例:`FullMapDirectory::set_sharer` 同样是 4 方法之外的**目录安装面**;
    /// plane 的「drain 回告」与目录的「sharer 安装」是同一种**装配缝**。
    /// **drain 完成回告** —— 指定 region 的 epoch 由 invalid 转 valid。
    ///
    /// ⚠️ **粒度 = per-region**:一次把该 gid 的**所有** region 置 valid 会造成
    ///   「某 region 的 drain 只完成一部分」无法表达 ⇒ 该 region 提前进入 divert 窗口。
    ///   先例:`FullMapDirectory::set_sharer` 同样是 4 方法之外的**目录安装面**。
    Status complete_drain(group_id_t gid, const AddrRange& range) {
        auto it = groups_.find(gid);
        if (it == groups_.end()) {
            last_status_ = Status::InvalidGroup;
            return Status::InvalidGroup;
        }
        auto rit = regions_.find(range);
        if (rit == regions_.end() || rit->second.gid != gid) {
            last_status_ = Status::InvalidRange;
            return Status::InvalidRange;
        }
        rit->second.epoch_valid = true;
        it->second.epoch_valid = true;
        last_status_ = Status::Ok;
        return Status::Ok;
    }

    /// **归属判定**(R2 判读侧,per design D-ι 裁决 1)—— 只读观测口
    RegionState region_state_of(std::uint64_t addr) const {
        for (const auto& [range, reg] : regions_) {
            if (!range.contains(addr))
                continue;
            return reg.epoch_valid ? RegionState::RegisteredActive
                                   : RegionState::RegisteredEpochInvalid;
        }
        return RegionState::NoRegion;
    }

    /// 请求载荷地址的归属判定
    RegionState region_state_of_payload(const CollectiveReq& req) const {
        return region_state_of(req.payload.addr);
    }

    Status last_status() const { return last_status_; }

    std::size_t group_count() const { return groups_.size(); }
    std::size_t pending_event_count() const { return events_.size(); }
    bool group_alive(group_id_t gid) const { return groups_.count(gid) != 0; }
    bool epoch_valid(group_id_t gid) const {
        auto it = groups_.find(gid);
        return it != groups_.end() && it->second.epoch_valid;
    }

    static constexpr cap_t kCapBarrier = 1u << 0;

private:
    struct Group {
        member_set_t members;
        GroupAttrs attrs;
        bool epoch_valid = false;
    };
    struct Region {
        group_id_t gid = kInvalidGroupId;
        AccessMode mode = AccessMode::Normal;
        bool epoch_valid = false;
    };
    struct BarrierState {
        /// 相位 → 已到达该相位的成员集合(per-phase 计,保证跨相位有序)
        std::map<phase_t, std::set<std::uint32_t>> arrived;
    };

    std::map<group_id_t, Group> groups_;
    std::map<AddrRange, Region> regions_;
    std::map<group_id_t, BarrierState> barrier_state_;
    std::vector<CollectiveEvt> events_;
    group_id_t next_gid_ = 1;
    std::uint32_t current_member_ = kInvalidMember;
    Status last_status_ = Status::Ok;
};

}  // namespace tlm::cep

#endif  // INCLUDE_TLM_CEP_COLLECTIVE_PLANE_HH_
