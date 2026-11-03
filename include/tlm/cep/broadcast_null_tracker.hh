// =============================================================================
// include/tlm/cep/broadcast_null_tracker.hh
//   —— `BroadcastNull`:**SharerTracker 的退化基线行为实现**(协议无关)
//
// 作者: CppTLM Team / 日期: 2026-11-03
// 配套: include/tlm/cep/sharer_tracker.hh(接口面 + `kTrackerTraits` + SnoopMechanics 公共机械)
//       include/tlm/cep/nvlink_sharp_state_spec.hh(**已归档**的规约载体;本类**不**消费它)
//       openspec/changes/cpptlm-cep-nvlink-sharp/(4 件套 + 2 份 delta spec)
//       台账 obj-cep-platform.md Entry #187(双审修订)· #186(规约层归档 + 移交)
//
// ── 定位(per 本 change 的 `design.md` §3)──────────────────────────────────
// 本类**兑现** `ADR-CEP-04 §2` caps 表已登记、但**行为面一直空缺**的第 5 个 tracker
// traits 行(`BroadcastNull`)⇒ 闭合 07 号「`BroadcastNull` 仅声明层 traits」缺口
// (per Entry #112 的「无消费者 ⇒ 不排期、保留登记」—— **本仓成为其首个消费者**)。
//
// ⚠️ **为什么是「协议无关」而不是 `nvlink_*`**:(per `design.md` N-1 · oracle R4)
//   `BroadcastNull` 是 `ADR-CEP-04 §2` 的**通用** tracker(非任何协议族专有)
//   ⇒ 挂在 `nvlink_*` 名下 ⇒ 下一个消费者(队列 #14 `ualink_inc`)被迫 include 一个
//   **NVLink 命名的头** ⇒ **协议族之间的横向依赖**(与 `cxl_dcoh_tracker.hh` 想避免的
//   「通用层反向依赖协议族」同型、只是换了个维度)⇒ 本文件**协议无关**。
//
// ⚠️ **依赖方向**:**依赖**通用层(`sharer_tracker.hh`)—— 正确方向;**不**被协议族头依赖。
//
// ── ⭐ caps / behavior **逐字段取自 traits**(不另写 caps 字面量)──────────────
// ⚠️ **鉴别力陷阱**(per `design.md` §3.3 · metis 审查):`SharerTrackerCaps{}` 的
//   **默认值** = `{Scope::NONE, Completeness::EXACT, Authority::NONE, AggregationAt::NONE}`
//   ⇒ 与 traits 的 `BroadcastNull` 行**逐字段相同** ⇒ 一个**从不赋值**的实现也
//   **100% 通过**「caps == traits」的等值断言 ⇒ 该断言**无鉴别力**。
//   ⇒ 本类的**真判据**是「**毒化 traits 行 ⇒ 可见 caps 必须跟随**」(反向 fixture F-3)。
//
// ── ⭐ 抓取点 1(调用边恒为零)的**行为侧兑现**──────────────────────────────
//   `ADR-CEP-01 §3.3` 抓取点 1 要求 Collective Plane → Directory 的调用边**必须为零**;
//   `scope=NONE` + `authority=NONE` ⇒ 本 tracker **从不**给出本地 sharer 视图
//   ⇒ **零目录边**。⚠️ 该结论是**由 traits 字段语义直接推出的定义性推论**,
//   **不**是可独立观测的事实 ⇒ 可观测判据只有:① 全部出参 ② 本类**不持有**任何
//   `std::map` / `std::vector` 状态容器(源扫描)⇒ 「零目录边」才**可证伪**。
//
// ── ⛔ 本类**不**提供的 API ────────────────────────────────────────────────
//   ⛔ **不**新增 `SharerTrackerIf` 的虚方法(接口面**恰 4 个**虚方法 + 虚析构,已硬锁)
//   ⛔ **不**定义 / 复制任何 caps 字面量(逐字段取自 traits)
//   ⛔ **不**持有目录 / sharer 状态副本(状态容器归协议引擎侧)
//   ⛔ **不**建模任何协议族特有概念(通道 / opcode / peer set / 错误语义)
// =============================================================================

#ifndef TLM_CEP_BROADCAST_NULL_TRACKER_HH
#define TLM_CEP_BROADCAST_NULL_TRACKER_HH

#include <cstddef>
#include <cstdint>

#include "tlm/cep/sharer_tracker.hh"

namespace tlm::cep {

    /// `BroadcastNull` —— **退化基线** tracker:无目录、无状态、无本地结论。
    ///
    /// ⚠️ 消费方纪律:本类**恒**返回 `BROADCAST` + `view.valid == false`
    /// ⇒ **不得**把本 tracker 的返回值当**本地答案**(它**没有**本地答案)。
    class BroadcastNullTracker final : public SharerTrackerIf, public SnoopMechanics {
    public:
        static constexpr TrackerImpl kImpl = TrackerImpl::BroadcastNull;

        /// traits 行 —— 用**查找函数**而非硬编码下标(⛔ 不用 `[4]`)
        /// ⇒ 行序变化不会静默错位(铁律 ④:fixture 状态变迁后须重新基线化)。
        static constexpr const TrackerTraits* kTraits = find_tracker_traits(TrackerImpl::BroadcastNull);

        /// traits 行 SHALL 存在(归因 `NVA-02`)
        static_assert(kTraits != nullptr, "NVA-02: BroadcastNull 的 traits 行 SHALL 存在");

        /// traits 行访问器(运行期用)
        static constexpr const TrackerTraits* traits() noexcept { return kTraits; }

        /// handle 空间大小(与既有行为类同款;16-bit 是 `ADR-CEP-04` 的 RTL 契约)
        static constexpr std::size_t kHandleSpace = SnoopMechanics::kHandleSpace;

        BroadcastNullTracker() {
            // ⚠️ **逐字段取自 traits**(零字面量)—— 见文件头的「鉴别力陷阱」说明
            caps = traits()->caps;
            behavior = traits()->behavior;
        }

        // ── 4 个接口方法 ─────────────────────────────────────────────────

        void lookup(std::uint64_t addr, Intent intent, LookupResult& result, LocalView& view,
                    LookupHazard& hazard) override {
            (void)addr;
            (void)intent;
            // ⚠️ **不**依赖出参的默认值:全部**显式**写死
            //    (`LocalView` 的默认值恰好符合期望 ⇒ 少写一项就恒真)
            hazard = behavior.hazard_under_contention; // == NONE(取自 traits,非字面量)
            view = LocalView{};                        // valid = false / sharers = 0 / dirty = 0
            result = LookupResult::BROADCAST;           // 退化基线 ⇒ 恒广播
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

        // ── 安装面(**显式 no-op**)────────────────────────────────────────
        //
        // ⚠️ `scope = NONE` ⇒ 本 tracker **不覆盖任何地址**
        //    ⇒ 任何安装动作都**不得**留下可见条目(否则与「不在范围内」不可区分)。

        /// 记录「可能持有该行」—— **显式 no-op**(`scope = NONE`)
        void set_sharer(std::uint64_t addr, node_id_t node, SnoopState state) {
            (void)addr;
            (void)node;
            (void)state;
        }

        /// 观测到放弃 —— **显式 no-op**
        void note_sharer_dropped(std::uint64_t addr, node_id_t node) {
            (void)addr;
            (void)node;
        }

        /// 调用方确知地清除 —— **显式 no-op**
        void clear_sharer(std::uint64_t addr, node_id_t node) {
            (void)addr;
            (void)node;
        }

        // ── 观测(测试夹具 / 调用方诊断)──────────────────────────────────

        /// 本地视图 —— **恒** `valid == false`(退化基线没有本地答案)
        LocalView view_of(std::uint64_t addr) const {
            (void)addr;
            return LocalView{};
        }

        /// 「可能持有」的节点数 —— **恒 0**(本类不持有任何 sharer 状态)
        std::size_t possible_sharer_count(std::uint64_t addr) const {
            (void)addr;
            return 0;
        }
    };

} // namespace tlm::cep

#endif // TLM_CEP_BROADCAST_NULL_TRACKER_HH
