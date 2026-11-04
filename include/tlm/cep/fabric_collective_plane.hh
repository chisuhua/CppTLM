// =============================================================================
// include/tlm/cep/fabric_collective_plane.hh
//   —— `FabricCollectivePlane`:承载**真实 fabric 集合流量**的 collective plane
//
// 作者: CppTLM Team / 日期: 2026-11-04
// 配套: include/tlm/cep/collective_plane.hh(接口 + 类型集 + plane 注册表)
//       include/tlm/cep/ualink_adapter.hh(UALink INC 侧门面)
//       openspec/changes/cpptlm-cep-ualink-inc/(4 件套 + 3 份 delta spec)
//       台账 obj-cep-platform.md Entry #204(建档 + 范围收窄)· #203(规约层归档)
//       docs/architecture/07-coherence-exploration-platform.md §5.3(plane 族)
//
// ── 定位(per 本 change 的 `design.md` §2.2)─────────────────────────────────
// 平台**第一个**数据路径**真可达**的 collective plane —— `LocalCollectivePlane` 的数据原语
// 是**显式声明未实现**(`Status::DataPathNotImplemented`),本类**真执行** REDUCE /
// BROADCAST / MULTICAST ⇒ 平台首次能回答「集合流量真的走 fabric 时会怎样」。
//
// ── ⭐ 与既有 plane 的**可区分性** ─────────────────────────────────────────
// `LocalCollectivePlane`:**本地模型**(不承载 fabric 流量)· 数据路径未实现。
// `FabricCollectivePlane`:**非本地模型**(承载真实 fabric 流量)· 数据路径真可达。
// ⇒ 二者在 `query_capability` 与 `send` 的返回上**可区分**(⛔ 不得「一样地未实现」)。
//
// ── ⭐ C-3 控制面与数据面互斥(per `ADR-CEP-01` SU#4)──────────────────────
// 某 group 有 op **in-flight** 时,`group_register_region`(R3 drain)与
// `group_destroy`(R4 回归)—— 二者是**控制面**操作 —— SHALL **被拒绝**,
// 且拒绝 SHALL 有**专属**状态取值(`Status::ControlPlaneConflict`)。
// ⇒ ⛔ **不得**复用「不支持」等既有取值(调用方须能区分「能力缺失」与「时序冲突」)。
//
// ── ⚠️ 本类**不**依赖 UALink 的 INC 语义 ───────────────────────────────────
// 这里的原语语义是**平台侧**语义(per `ADR-CEP-03 §2`:类型集 + 7 方法面),
// ⛔ **不是** UALink 的 INC 规范语义(后者 4 项仍 **pending**,per 锚点摘要 `N-1`)
// ⇒ 本类的行为**不因** UALink 语义转正而改变。
//
// ── ⚠️ 建模简化(显式登记,不得被误读为规范结论)────────────────────────────
// ① **归约算术**只在**整数** datatype(`U8` / `U32` / `U64`)上进行;`F32` / `F64` 的
//    REDUCE ⇒ `Status::Unsupported`(**显式「不支持」**,不是静默近似)。
// ② **重复贡献**(同一轮、同一成员再发一次)⇒ **覆盖**前值(re-send 语义),
//    不额外报错(该行为**不**代表任何 UALink 语义)。
// ③ **每轮**的参与集由**该轮首个** `send` 决定;后续 `send` 若来自**不在参与集**的成员
//    ⇒ `Status::NotMember`。
// ④ ⚠️ **成员失联(member dropout)本 plane 未建模** ⇒ 本类**不产出** `CollectiveEvt::Fault`。
//    (「partial failure ⇒ 显式 `Fault`」的**类型面**由既有 `CollectiveEvt` 三变体承担
//    —— per 冻结 spec `cep-collective-plane-base` 的「类型集齐备且 `CollectiveEvt` 含 Fault 变体」;
//     **行为面**的 Fault 产出在 `LocalCollectivePlane::group_destroy`(in-flight ⇒ Fault),
//     而本 plane 在同一场景下按 **C-3** **拒绝**销毁 ⇒ 两条路径**互斥**、无冲突。
//     ⚠️ 若将来要建模「成员失联 ⇒ 其余成员观察到什么」⇒ 需**新**的规约依据
//     (锚点摘要 `N-2` = `WriteMulticast` 的部分失败语义 **pending**)⇒ **不得**推理填空。
// =============================================================================

#ifndef TLM_CEP_FABRIC_COLLECTIVE_PLANE_HH
#define TLM_CEP_FABRIC_COLLECTIVE_PLANE_HH

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "tlm/cep/collective_plane.hh"

namespace tlm {
    namespace cep {

        /// **承载真实 fabric 集合流量**的 collective plane。
        class FabricCollectivePlane final : public CollectivePlaneIf {
        public:
            // ── group 生命周期(慢路径,管理语义)─────────────────────────
            group_id_t group_create(member_set_t members, const GroupAttrs& attrs) override {
                if (members.empty()) {
                    last_status_ = Status::EmptyMemberSet;
                    return kInvalidGroupId;
                }
                if (next_gid_ == kInvalidGroupId)
                    next_gid_ = 1;
                const group_id_t gid = next_gid_++;
                groups_[gid] = Group{};
                groups_[gid].members = std::move(members);
                groups_[gid].attrs = attrs;
                last_status_ = Status::Ok;
                return gid;
            }

            void group_destroy(group_id_t gid) override {
                auto it = groups_.find(gid);
                if (it == groups_.end()) {
                    last_status_ = Status::InvalidGroup;
                    return;
                }
                // ⚠️ **C-3**:有 op in-flight ⇒ **拒绝**控制面操作(⛔ **不**销毁 group)
                if (group_has_inflight(gid)) {
                    last_status_ = Status::ControlPlaneConflict;
                    return;
                }
                // R4:group_destroy 后区域**回归 coherence 域** ⇒ 该 region **从表里移除**
                for (auto rit = regions_.begin(); rit != regions_.end();) {
                    if (rit->second.gid == gid)
                        rit = regions_.erase(rit);
                    else
                        ++rit;
                }
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
                // ⚠️ **C-3**:有 op in-flight ⇒ **拒绝**控制面操作
                if (group_has_inflight(gid)) {
                    last_status_ = Status::ControlPlaneConflict;
                    return Status::ControlPlaneConflict;
                }
                if (!range.valid()) {
                    last_status_ = Status::InvalidRange;
                    return Status::InvalidRange;
                }
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
                reg.epoch_valid = false; // epoch 两阶段:register 后仍 invalid
                regions_[range] = reg;
                last_status_ = Status::Ok;
                return Status::Ok;
            }

            // ── 数据路径原语(真可达)────────────────────────────────────
            void send(const CollectiveReq& req) override {
                const auto st = region_state_of_payload(req);
                if (st == RegionState::NoRegion) {
                    // 未注册区域 ⇒ 立即报错,**不投递任何事件**(per `ADR-CEP-03 §3.4`)
                    last_status_ = Status::GroupRegisterError;
                    return;
                }
                if (st == RegionState::RegisteredEpochInvalid) {
                    // epoch 未 commit ⇒ 正常服务窗口(R4),但集合数据路径**不执行**
                    last_status_ = Status::GroupRegisterError;
                    return;
                }
                auto git = groups_.find(req.group);
                if (git == groups_.end()) {
                    last_status_ = Status::InvalidGroup;
                    return;
                }
                if (req.src_member == kInvalidMember ||
                    git->second.members.count(req.src_member) == 0) {
                    last_status_ = Status::NotMember;
                    return;
                }
                if (req.op == CollectiveOp::BARRIER) {
                    // BARRIER 走 `collective_barrier`(相位 rendezvous)⇒ 不由 `send` 承担
                    last_status_ = Status::UnknownOp;
                    return;
                }
                if (req.op == CollectiveOp::REDUCE) {
                    const std::uint64_t mask = datatype_mask(req.dtype);
                    if (mask == 0u) {
                        // 浮点归约 ⇒ **显式不支持**(⛔ 不是静默近似)
                        last_status_ = Status::Unsupported;
                        return;
                    }
                }

                // 轮次键 = (group, op, tag) —— `tag` 即 `ADR-CEP-03 §2` 的「相位 / 轮次」
                const RoundKey key{req.group, req.op, req.tag};
                auto rit = rounds_.find(key);
                if (rit == rounds_.end()) {
                    Round round;
                    round.op = req.op;
                    round.participants = participants_of(git->second.members, req);
                    if (round.participants.empty()) {
                        last_status_ = Status::EmptyMemberSet;
                        return;
                    }
                    rit = rounds_.emplace(key, std::move(round)).first;
                }
                Round& round = rit->second;
                if (round.participants.empty()) {
                    last_status_ = Status::EmptyMemberSet;
                    return;
                }
                // 参与集由**该轮首个** `send` 决定 ⇒ 非参与者的贡献被拒
                if (std::find(round.participants.begin(), round.participants.end(), req.src_member) ==
                    round.participants.end()) {
                    last_status_ = Status::NotMember;
                    return;
                }

                const std::uint64_t value = payload_value(req);
                if (!round.broadcast_set) {
                    // BROADCAST / MULTICAST 的广播值 = **首个**贡献者的载荷
                    round.broadcast_value = value;
                    round.broadcast_set = true;
                }
                round.contributions[req.src_member] = value; // 重复贡献 ⇒ 覆盖(re-send 语义)

                if (round.contributions.size() < round.participants.size()) {
                    // **未收齐** ⇒ 只累积,不产出事件(不得提前释放)
                    last_status_ = Status::Ok;
                    return;
                }

                // **收齐** ⇒ 产出结果
                const std::uint64_t result = compute_result(round, req);
                for (std::uint32_t m : round.participants) {
                    (void)m;
                    events_.push_back(CollectiveEvt::make_result(req.group, req.tag, result));
                }
                events_.push_back(CollectiveEvt::make_completion(req.group, req.tag));
                rounds_.erase(rit);   // 完成的轮次**移出** ⇒ in-flight 判定只看未完成轮次
                last_status_ = Status::Ok;
            }

            void recv(CollectiveEvt& evt) override {
                if (events_.empty()) {
                    // **无事件**必须与「取到事件」**可区分**
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
                    last_status_ = Status::NotMember;
                    return;
                }
                // 成员身份校验:非成员到达**不得**计入阈值
                if (it->second.members.count(current_member_) == 0) {
                    last_status_ = Status::NotMember;
                    return;
                }
                BarrierState& st = barrier_state_[gid];
                st.arrived[phase].insert(current_member_);
                if (st.arrived[phase].size() >= it->second.members.size()) {
                    for (std::uint32_t m : it->second.members) {
                        (void)m;
                        events_.push_back(CollectiveEvt::make_completion(gid, phase));
                    }
                    st.arrived.erase(phase);
                }
                last_status_ = Status::Ok;
            }

            cap_t query_capability(CollectiveOp op, std::size_t max_bytes) const override {
                // `max_bytes == 0` ⇒ 「不支持」语义(per base spec 的 capability 边界契约)
                if (max_bytes == 0)
                    return 0;
                switch (op) {
                    case CollectiveOp::BARRIER:
                        return kCapBarrier;
                    case CollectiveOp::REDUCE:
                        return kCapReduce;
                    case CollectiveOp::BROADCAST:
                        return kCapBroadcast;
                    case CollectiveOp::MULTICAST:
                        return kCapMulticast;
                }
                return 0;
            }

            // ── 装配 / 驱动用辅助口(不在 7 方法接口面内)────────────────────
            Status set_current_member(std::uint32_t member) {
                current_member_ = member;
                last_status_ = Status::Ok;
                return Status::Ok;
            }

            std::uint32_t current_member() const { return current_member_; }

            /// **drain 完成回告** —— 指定 region 的 epoch 由 invalid 转 valid(per-region 粒度)
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
                last_status_ = Status::Ok;
                return Status::Ok;
            }

            /// **归属判定**(R2 判读侧)—— 只读观测口
            RegionState region_state_of(std::uint64_t addr) const {
                for (const auto& [range, reg] : regions_) {
                    if (!range.contains(addr))
                        continue;
                    return reg.epoch_valid ? RegionState::RegisteredActive
                                           : RegionState::RegisteredEpochInvalid;
                }
                return RegionState::NoRegion;
            }

            RegionState region_state_of_payload(const CollectiveReq& req) const {
                return region_state_of(req.payload.addr);
            }

            Status last_status() const { return last_status_; }
            std::size_t group_count() const { return groups_.size(); }
            std::size_t pending_event_count() const { return events_.size(); }
            bool group_alive(group_id_t gid) const { return groups_.count(gid) != 0; }

            /// 该 group 的 epoch 是否**整体**生效
            bool epoch_valid(group_id_t gid) const {
                if (groups_.count(gid) == 0)
                    return false;
                for (const auto& [range, reg] : regions_) {
                    (void)range;
                    if (reg.gid == gid && !reg.epoch_valid)
                        return false;
                }
                return true;
            }

            /// ⭐ **C-3 的可观测辅助口** —— 该 group 是否有 op **in-flight**
            bool group_has_inflight(group_id_t gid) const {
                for (const auto& [key, round] : rounds_) {
                    (void)round;
                    if (std::get<0>(key) == gid)
                        return true;
                }
                return false;
            }

            static constexpr cap_t kCapBarrier = 1u << 0;
            static constexpr cap_t kCapReduce = 1u << 1;
            static constexpr cap_t kCapBroadcast = 1u << 2;
            static constexpr cap_t kCapMulticast = 1u << 3;

        private:
            /// 轮次键 = **(group_id, op, tag)** —— ⚠️ **必须含 `group_id`**:
            /// `ADR-CEP-03 §2` 明定 `group_id` 是 collective 的**一等公民**、完成语义是
            /// **group-scoped** ⇒ 两个 group 用**同一** `(op, tag)` 时**不得**共享同一轮次。
            using RoundKey = std::tuple<group_id_t, CollectiveOp, tag_t>;

            struct Group {
                member_set_t members;
                GroupAttrs attrs;
            };
            struct Region {
                group_id_t gid = kInvalidGroupId;
                AccessMode mode = AccessMode::Normal;
                bool epoch_valid = false;
            };
            struct Round {
                CollectiveOp op = CollectiveOp::REDUCE;
                std::vector<std::uint32_t> participants;
                std::map<std::uint32_t, std::uint64_t> contributions;
                std::uint64_t broadcast_value = 0;
                bool broadcast_set = false;
            };
            struct BarrierState {
                std::map<phase_t, std::set<std::uint32_t>> arrived;
            };

            /// datatype → 掩码;**0** ⇒ 浮点(本实现**不支持**浮点归约)
            static constexpr std::uint64_t datatype_mask(DataType dt) noexcept {
                switch (dt) {
                    case DataType::U8:
                        return 0xFFull;
                    case DataType::U32:
                        return 0xFFFFFFFFull;
                    case DataType::U64:
                        return ~0ull;
                    case DataType::F32:
                    case DataType::F64:
                        return 0u;
                }
                return 0u;
            }

            static constexpr std::uint64_t payload_value(const CollectiveReq& req) noexcept {
                return req.payload.inline_payload ? req.payload.inline_value : req.payload.addr;
            }

            static std::vector<std::uint32_t> participants_of(const member_set_t& members,
                                                              const CollectiveReq& req) {
                std::vector<std::uint32_t> out;
                if (req.op == CollectiveOp::MULTICAST) {
                    for (std::uint32_t m : members) {
                        if (m < 64u && ((req.multicast_mask >> m) & 1ull) != 0ull)
                            out.push_back(m);
                    }
                    return out;
                }
                for (std::uint32_t m : members)
                    out.push_back(m);
                return out;
            }

            static std::uint64_t apply_reduce(ReduceOp op, std::uint64_t acc, std::uint64_t v,
                                               std::uint64_t mask) noexcept {
                switch (op) {
                    case ReduceOp::Sum:
                        return (acc + v) & mask;
                    case ReduceOp::Min:
                        return acc < v ? acc : v;
                    case ReduceOp::Max:
                        return acc > v ? acc : v;
                    case ReduceOp::BitAnd:
                        return acc & v;
                    case ReduceOp::BitOr:
                        return acc | v;
                    case ReduceOp::BitXor:
                        return acc ^ v;
                }
                return acc;
            }

            std::uint64_t compute_result(const Round& round, const CollectiveReq& req) const {
                if (round.op == CollectiveOp::REDUCE) {
                    const std::uint64_t mask = datatype_mask(req.dtype);
                    std::uint64_t acc = 0;
                    bool first = true;
                    for (std::uint32_t m : round.participants) {
                        const auto it = round.contributions.find(m);
                        if (it == round.contributions.end())
                            continue;
                        const std::uint64_t v = it->second & mask;
                        acc = first ? v : apply_reduce(req.reduce_op, acc, v, mask);
                        first = false;
                    }
                    return acc;
                }
                // BROADCAST / MULTICAST ⇒ 广播值 = 首个贡献者的载荷
                return round.broadcast_value;
            }

            std::map<group_id_t, Group> groups_;
            std::map<AddrRange, Region> regions_;
            std::map<RoundKey, Round> rounds_;
            std::map<group_id_t, BarrierState> barrier_state_;
            std::vector<CollectiveEvt> events_;
            group_id_t next_gid_ = 1;
            std::uint32_t current_member_ = kInvalidMember;
            Status last_status_ = Status::Ok;
        };

    } // namespace cep
} // namespace tlm

#endif // TLM_CEP_FABRIC_COLLECTIVE_PLANE_HH
