// =============================================================================
// include/tlm/cep/tilelink_tree_manager.hh
//   —— TreeManager:TileLink-C 的**树形** SharerTracker 行为实现(第 4 种行为类)
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: include/tlm/cep/tilelink_state_spec.hh(规约载体 · 第 0 个 change 已归档)
//       include/tlm/cep/sharer_tracker.hh(接口面 + SnoopMechanics 公共机械)
//       openspec/changes/cpptlm-cep-tilelink-tree-manager/(change 4 件套)
//       台账 obj-cep-platform.md Entry #144(建档)· #141(二审落修 批 2)
//
// ── 定位(per 该 change 的 design `D-β` / `D-γ` / `D-δ`)──────────────────
// 本类**不是**缓存性能模型,而是 **TileLink 树形一致性语义的可执行载体**:
//   · 作用域 = **子树**(`Scope::SUBTREE`)· 权威 = **本地**(`Authority::LOCAL`)
//   · 聚合点 = **树父**(`AggregationAt::TREE_PARENT`)· contention = **CHAIN**
//
// ⚠️ **为什么放在独立头(不并入 `sharer_tracker.hh`)**:
//   `sharer_tracker.hh` 是**协议无关**的通用抽象层 ⇒ 若在其中 `#include` 协议族头
//   (本例 = TileLink 规约载体)⇒ **通用层反向依赖协议族** ⇒ 层级倒置。
//   ⇒ 本类**依赖**通用层(正确方向),且**复用**同一份 3 稳定态词表(单源,无重复枚举)。
//
// ── ⭐ 全仓**首次真实使用**的两个枚举(per `ADR-CEP-04` 为 TileLink 专设)────
//   · `LookupResult::ESCALATE_UP` —— 「树形向上递归」
//   · `LookupHazard::CHAIN`      —— 「加入同一 in-flight snoop」
//   ⇒ 此前 3 个行为实现**一个都没用** ⇒ 本类**闭合「枚举有、语义无」的缺口**。
//
// ── 边界 ────────────────────────────────────────────────────────────────
//   · **header-only**,**不进 `ModuleFactory`**(同 4 个既有 adapter)
//   · **不注册** 23 ABI · **不改** frozen header · **不扩** `CATEGORY_*` 词表
//   · **不扩** `perm` 4 值 · **不新增** invariant 编号
//   · ⚠️ **`LocalView` 零改动** —— 「向上递归的目标」复用 `forward_to` 承载
//     (双重语义已登记:调用方 SHALL 先读 `result` 再解释该字段)
//
// ── 依据 ────────────────────────────────────────────────────────────────
//   · 3 稳定态(`Nothing`/`Branch`/`Tip`)⇒ `include/tlm/cep/tilelink_state_spec.hh`
//   · caps / hazard ⇒ `ADR-CEP-04 §2` / `§4` 的冻结契约(**取自** `kTrackerTraits`,不重推)
//   · 「lookup 假设单次同步回答全局问题,但树形语义下做不到」⇒ `ADR-CEP-04 §5`
//     ⇒ `ESCALATE_UP` 正是「不撒谎」的显式出口
//   · `Trunk` 折叠为 `Branch` ⇒ 规约载体 `design.md` D-α(决策口径,非规范断言)
//
// ── 合规 ────────────────────────────────────────────────────────────────
//   TileLink 是 SiFive 明示的 free and open standard ⇒ 规范正文可读;
//   ⚠️ 规范 © SiFive(非 CC-BY)⇒ 本类**只记条款级定位 + 断言摘要**,不整段复制原文。
// =============================================================================

#ifndef CPPTLM_CEP_TILELINK_TREE_MANAGER_HH
#define CPPTLM_CEP_TILELINK_TREE_MANAGER_HH

#include <cstdint>
#include <map>

#include "tlm/cep/sharer_tracker.hh"
#include "tlm/cep/tilelink_state_spec.hh"

namespace tlm::cep {

    /// `TreeManager` —— TileLink-C 的**树形** SharerTracker 行为实现。
    ///
    /// ⚠️ 消费方纪律:本类**不**持任何协议族的**表副本**(态 / 投影 / 转移表全部**委托**规约载体)
    /// ⇒ 规约改一格、本类必红。
    class TreeManager final : public SharerTrackerIf, public SnoopMechanics {
    public:
        /// 3 稳定态(**别名**规约载体 ⇒ 单源,不另立词表)
        using State = TileLinkStateSpec::State;

        static constexpr TrackerImpl kImpl = TrackerImpl::TreeManager;

        /// handle 空间大小(与既有行为类同款;16-bit 是 `ADR-CEP-04` 的 RTL 契约)
        static constexpr std::size_t kHandleSpace = SnoopMechanics::kHandleSpace;

        /// traits 行锁定 —— 本类**从 `kTrackerTraits` 取值**(不另写 caps 字面量)
        static_assert(kTrackerTraits[3].impl == TrackerImpl::TreeManager,
                      "traits 第 3 行 SHALL 是 TreeManager");

        /// @param tree_parent 本 tracker 在树中的**父节点** —— 「向上递归」的终点
        ///                     (caps 的 `AggregationAt::TREE_PARENT` 的具体化)
        explicit TreeManager(node_id_t tree_parent) : tree_parent_(tree_parent) {
            caps = kTrackerTraits[3].caps;
            behavior = kTrackerTraits[3].behavior;
        }

        // ── 树结构安装(由 home 侧 / 测试夹具驱动)────────────────────────
        /// 设置某地址在**本子树**内的副本状态。
        /// @param position 该节点在树中的位置:`Tip`(持 R/W 副本)· `Branch`(只读副本)·
        ///                 `Nothing`(无副本)
        /// ⚠️ `Trunk`(路径节点)在本载体里**折叠**为 `Branch`(per 规约载体 D-α 决策口径)
        void set_sharer(std::uint64_t addr, node_id_t node, State position) {
            const responder_set_t m = responder_mask_of(node);
            // 未映射节点 ⇒ **显式 no-op**:不得 `entries_[addr]` 先造条目,否则会留下
            // sharers == 0 的可见空条目(与「无此地址」在 `position_of` 上无法区分)。
            if (m == 0)
                return;
            Entry& e = entries_[addr];
            if (position == State::Tip) {
                e.sharers |= m;
                e.tips |= m;
            } else if (position == State::Branch) {
                e.sharers |= m;
                e.tips &= ~m; // 只读副本 ⇒ 不是写者
            } else {          // State::Nothing —— 无副本
                e.sharers &= ~m;
                e.tips &= ~m;
            }
        }

        /// 清除某节点在某地址上的副本。
        void clear_sharer(std::uint64_t addr, node_id_t node) {
            auto it = entries_.find(addr);
            if (it == entries_.end())
                return;
            const responder_set_t m = responder_mask_of(node);
            if (m == 0)
                return; // 未映射节点 ⇒ 本来就无位可清
            it->second.sharers &= ~m;
            it->second.tips &= ~m;
        }

        /// 该地址在**本子树**内的树位置(**派生**,不另存 ⇒ 无双真源)
        State position_of(std::uint64_t addr) const {
            auto it = entries_.find(addr);
            if (it == entries_.end())
                return State::Nothing;
            if (it->second.tips != 0)
                return State::Tip;
            if (it->second.sharers != 0)
                return State::Branch;
            return State::Nothing;
        }

        /// 该地址在本子树内**是否有**条目
        bool has_node(std::uint64_t addr) const {
            return entries_.count(addr) != 0;
        }

        /// 本 tracker 的**树父节点**(向上递归的终点)
        node_id_t tree_parent() const {
            return tree_parent_;
        }

        // ── 4 个接口方法 ─────────────────────────────────────────────────

        void lookup(std::uint64_t addr, Intent intent, LookupResult& result, LocalView& view,
                    LookupHazard& hazard) override {
            if (is_inflight(addr)) {
                // 同址 snoop 在飞 ⇒ 本实现的契约是 **CHAIN**(「加入它」而非「等它」)
                // ⇒ hazard != NONE ⇒ view 无意义(per `LocalView` 契约:valid == false ⇔ hazard !=
                // NONE)
                result = LookupResult::ANSWER_HERE;
                view = LocalView{};
                hazard = behavior.hazard_under_contention; // == CHAIN(取自 traits,非字面量)
                return;
            }

            hazard = LookupHazard::NONE;
            view = LocalView{};
            view.valid = true;

            auto it = entries_.find(addr);
            const bool have = it != entries_.end();
            const State pos =
                !have ? State::Nothing
                      : (it->second.tips != 0
                             ? State::Tip
                             : (it->second.sharers != 0 ? State::Branch : State::Nothing));

            if (pos == State::Tip) {
                // 本子树内存在 **Tip**(R/W 副本)⇒ 本地权威 ⇒ 就地回答
                view.sharers = it->second.sharers;
                // ⚠️ **语义损失**:只有 Tip 被标脏 ⇒ `Trunk` 的「may contain dirty data」
                //    在 `dirty` 掩码上**不可见**(折叠为 `Branch` ⇒ 脏性丢失)—— 已登记
                view.dirty = it->second.tips;
                result = LookupResult::ANSWER_HERE;
                return;
            }
            if (pos == State::Branch && intent == Intent::want_read) {
                // 本子树内只有只读副本 ⇒ 读可由**本地副本**服务(无需向上递归)
                view.sharers = it->second.sharers;
                result = LookupResult::ANSWER_HERE;
                return;
            }
            // 本子树内无 Tip(且读不可本地服务)⇒ **向上递归**到树父
            // ⇒ `ESCALATE_UP` = 「不撒谎」的显式出口(per `ADR-CEP-04 §5`)
            view.forward_to = tree_parent_;
            result = LookupResult::ESCALATE_UP;
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

    private:
        /// 该地址在本子树内的副本分布
        struct Entry {
            responder_set_t sharers = 0; ///< 持副本的节点集
            responder_set_t tips = 0;    ///< 持 R/W 副本(Tip)的节点集(至多 1 个)
        };

        std::map<std::uint64_t, Entry> entries_;
        node_id_t tree_parent_ = kInvalidNode;
    };

} // namespace tlm::cep

#endif // CPPTLM_CEP_TILELINK_TREE_MANAGER_HH
