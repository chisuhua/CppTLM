// =============================================================================
// include/tlm/cep/cxl_dcoh_tracker.hh
//   —— `DcohSymmetric`:CXL 3.0 对称(DCOH)的 **SharerTracker 行为实现**
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: include/tlm/cep/sharer_tracker.hh(接口面 + SnoopMechanics 公共机械)
//       include/tlm/cep/cxl_dcoh_state_spec.hh(**已归档**的规约载体,只读消费)
//       openspec/changes/cpptlm-cep-cxl-dcoh-symmetric/(4 件套 + delta spec CDX-01 ~ CDX-08)
//       台账 obj-cep-platform.md Entry #154(4 项 architect 裁决)· #159(规约层归档)· #160(本 change)
//       docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md(basis 唯一人工审计面)
//
// ── 定位 ────────────────────────────────────────────────────────────────────
// 本类**兑现** `ADR-CEP-04 §2` caps 表已登记、但**行为面一直空缺**的第 6 个 tracker
// traits 行(`DcohSymmetric`)⇒ 闭合 07 号「残留 A:行为语义无基源」。
//
// ⚠️ **为什么放在独立头(不并入 `sharer_tracker.hh`)**:
//   `sharer_tracker.hh` 是**协议无关**的通用抽象层 ⇒ 若在其中 `#include` 协议族头
//   (本例 = CXL DCOH 规约载体)⇒ **通用层反向依赖协议族** ⇒ 层级倒置。
//   ⇒ 本类**依赖**通用层(正确方向),且**复用**同一份 MESI 4 态词表(单源,无重复枚举)。
//   ⇒ 同队列 #11 的 `TreeManager`(独立头 `tilelink_tree_manager.hh`)**同款**。
//
// ── ⭐ 6 个 caps / behavior 字段的**逐字段**行为落地(per 本 change `design.md` §1.4)──
//
//   | 字段 | 值 | 本类的**可观测**行为 |
//   |------|----|---------------------|
//   | `scope` | `LOCAL_MEMORY_REGION` | lookup **只**在本区域(HDM)内有效 ⇒ 区域外地址**不**由本
//   tracker 回答 | | `completeness` | `CONSERVATIVE` | **宁多探不漏探** ⇒ 保守近似,**不得**声称精确
//   ⇒ 任何「可能多探」处一律 `BROADCAST` | | `authority` | `LOCAL_HINT` | 答案**只是 hint** ⇒
//   **不可当权威** ⇒ `LocalView` **零改动** ⇒ `forward_to` **语义重载** | | `aggregation_at` |
//   `HOME` | 聚合点**恒为 home**(per Entry #154 ①)⇒ 无 `ESCALATE_UP` 语义 | | `hazard` | `STALL` |
//   同址并发 lookup ⇒ 返 `STALL`(不是 NONE / 不是 CHAIN) | | `broadcast_fallback` | `true` | ⚠️
//   **公开来源未见依据** ⇒ 按 traits 实现 + **登记 pending**;**不改 traits** |
//
// ── ⚠️ 行为规约的**依据强度**(per 锚点摘要 §6 #4 + §7)──────────────────────
//   · `STALL` ✅(S6 p.39–40,直接印证)
//   · `LOCAL_MEMORY_REGION` ✅(HDM 区域)
//   · `CONSERVATIVE` 🟡(imprecise SF 会 over-snoop,S7 p.1)⇒ **保守方向与公开来源一致**
//   · `LOCAL_HINT` 🟡(DCOH 亦被称为「最终仲裁者」,S6 p.30)⇒ **存在张力** ⇒ 见下方「语义代价」
//   · `broadcast_fallback=true` ❌ **未见依据** ⇒ **登记 pending**
// ⇒ 本类**不预支**任何 pending 语义:不建模 transient / 错误 / peer set(per R-S6 / R-S7 / R-S8)。
//
// ── ⚠️ `LOCAL_HINT` 的**语义代价**(CDX-05)──────────────────────────────────
//   tracker 的答案**只是 hint**,**不可当权威** ⇒ `LocalView` **零改动**
//   ⇒ `forward_to` 字段**语义重载**:本实现用它承载「hint 的目标」,而**不是**「权威转发点」。
//   ⇒ **消费方纪律**:判读 `forward_to` 前**必须**先读 `result`。
//   ⇒ 该义务的**可寻址登记**见 `openspec/specs/cep-sharer-tracker-base/spec.md` 追加段 + 台账 §10。
//
// ── ⛔ 本类**不**提供的 API(per delta spec CDX-03)──────────────────────────
//   规约载体**无转移表**(对称模式的转移语义在**公开非规范来源**里**没有**)
//   ⇒ 本类**不**提供任何 `transition_*` / `is_silent_upgrade` 之类**转移判定**。
//   ⇒ 本类**不**消费 CXL.mem 通道词表(那属 adapter 面)。
//
// ── 边界 ────────────────────────────────────────────────────────────────────
//   · **header-only** · **不进 `ModuleFactory`**(同既有 adapter)· **不注册** 23 ABI
//   · **不改** `sharer_tracker.hh`(接口面零改动)· **不改** `cxl_dcoh_state_spec.hh`(只读消费)
//   · **不扩** `perm` 4 值 · **不新增** invariant 编号 · **不**产出性能结论
// =============================================================================

#ifndef CPPTLM_CEP_CXL_DCOH_TRACKER_HH
#define CPPTLM_CEP_CXL_DCOH_TRACKER_HH

#include <cstddef>
#include <cstdint>
#include <map>

#include "tlm/cep/sharer_tracker.hh"

namespace tlm::cep {

    /// `DcohSymmetric` —— CXL 3.0 对称(DCOH)的 **SharerTracker 行为实现**。
    ///
    /// ⚠️ 消费方纪律:本类**不**持任何 CXL 协议族的**表副本**(态 / 投影 / 转移表全部
    /// **委托**规约载体或**不建模**)⇒ 规约改一格、本类**不**必红(本类**不**依赖表)。
    ///
    /// ⚠️ **本类的实现是「通用层语义」而非「CXL 协议语义」** —— 它只兑现 `ADR-CEP-04 §2`
    /// caps 表承诺的**行为形状**(区域限定 + 保守 + hint + STALL + 广播兜底),
    /// **不**建模任何 CXL 特有概念(通道 / opcode / peer set / 错误语义)。
    class DcohSymmetric final : public SharerTrackerIf, public SnoopMechanics {
    public:
        static constexpr TrackerImpl kImpl = TrackerImpl::DcohSymmetric;

        /// handle 空间大小(与既有行为类同款;16-bit 是 `ADR-CEP-04` 的 RTL 契约)
        static constexpr std::size_t kHandleSpace = SnoopMechanics::kHandleSpace;

        /// traits 行锁定 —— 本类**从 `kTrackerTraits` 取值**(不另写 caps 字面量)
        static_assert(kTrackerTraits[2].impl == TrackerImpl::DcohSymmetric,
                      "traits 第 2 行 SHALL 是 DcohSymmetric");

        /// 本 tracker 覆盖的**本地内存区域**(HDM 区间)—— `scope=LOCAL_MEMORY_REGION` 的具体化。
        ///
        /// ⚠️ **语义**:`base` 起 `size` 字节为**本区域**;**区域外**的地址
        /// **不由本 tracker 回答**(`lookup` 返回 `BROADCAST` + `hazard=NONE`)。
        /// ⇒ 这正是 `LOCAL_MEMORY_REGION` 与 `SYSTEM` scope 的**可观测差异**。
        ///
        /// ⚠️ **零长度区域** = 「本 tracker 不覆盖任何地址」⇒ 所有 lookup 均 `BROADCAST`。
        struct LocalRegion {
            std::uint64_t base = 0;
            std::uint64_t size = 0;

            constexpr bool contains(std::uint64_t addr) const noexcept {
                // ⚠️ 溢出安全:用「减法判据」而非 `addr + size`(后者在 `size` 极大时会绕回)
                return size != 0 && addr >= base && (addr - base) < size;
            }
        };

        /// @param region 本 tracker 覆盖的**本地内存区域**(HDM 区间)
        explicit DcohSymmetric(const LocalRegion& region) : region_(region) {
            caps = kTrackerTraits[2].caps;
            behavior = kTrackerTraits[2].behavior;
        }

        // ── 安装面(记录「**可能**持有」)───────────────────────────────
        //
        // ⚠️ **CONSERVATIVE 纪律**:位集是 sharer 集的**上界** ⇒ **只增不减**是安全的
        //    (宁可多探不漏探)⇒ 与 `SparseSnoopFilter` 同款纪律。

        /// 记录「**可能**持有该行」。
        /// @param state 该节点的状态(M ⇒ 标脏;其余 ⇒ 不标脏)
        /// ⚠️ **区域外** ⇒ **显式 no-op**:本 tracker 的 scope 不覆盖该地址
        ///    ⇒ 不得留下可见条目(否则与「不在本区域」在 `view_of` 上无法区分)。
        void set_sharer(std::uint64_t addr, node_id_t node, SnoopState state) {
            if (!region_.contains(addr))
                return;
            const responder_set_t m = responder_mask_of(node);
            if (m == 0)
                return;
            Entry& e = entries_[addr];
            e.sharers |= m;
            if (state == SnoopState::M)
                e.dirty |= m;
            else
                e.dirty &= ~m;
        }

        /// 依据**观测**确认该节点已放弃该行 ⇒ **合法收缩**(CONSERVATIVE 的唯一收缩来源)。
        void note_sharer_dropped(std::uint64_t addr, node_id_t node) {
            auto it = entries_.find(addr);
            if (it == entries_.end())
                return;
            const responder_set_t m = responder_mask_of(node);
            if (m == 0)
                return;
            if ((it->second.sharers & m) == 0 && (it->second.dirty & m) == 0)
                return; // 本非 sharer ⇒ no-op(避免伪触发 Retire,同 `SparseSnoopFilter` 的同型纪律)
            it->second.sharers &= ~m;
            it->second.dirty &= ~m;
            if (it->second.sharers == 0 && it->second.dirty == 0) {
                entries_.erase(it);
                events_.push_back(LifecycleEvent::RetireOnZeroSharers);
            }
        }

        /// 调用方**确知**地清除(⚠️ 误用 ⇒ 漏探风险 ⇒ SHALLOW 前须有确证)。
        void clear_sharer(std::uint64_t addr, node_id_t node) {
            note_sharer_dropped(addr, node);
        }

        // ── 4 个接口方法 ─────────────────────────────────────────────────

        void lookup(std::uint64_t addr, Intent intent, LookupResult& result, LocalView& view,
                    LookupHazard& hazard) override {
            if (inflight_by_addr_.count(addr) != 0) {
                // 同地址 snoop 在飞:本实现的契约是 **STALL**(per `ADR-CEP-04 §4` 表)
                // ⇒ hazard != NONE ⇒ view 无意义(per `LocalView` 契约)
                result = LookupResult::ANSWER_HERE; // 无意义占位(视图在 STALL 下不成立)
                view = LocalView{};
                hazard = behavior.hazard_under_contention; // == STALL(取自 traits,非字面量)
                return;
            }

            hazard = LookupHazard::NONE;

            // ⚠️ **区域外** ⇒ 本 tracker 的 scope 不覆盖该地址 ⇒ **不**给任何本地结论
            //    ⇒ `BROADCAST`(保守兜底)—— 这是 `LOCAL_MEMORY_REGION` 的**可观测差异**。
            if (!region_.contains(addr)) {
                view = LocalView{};
                result = LookupResult::BROADCAST;
                return;
            }

            auto it = entries_.find(addr);
            if (it == entries_.end()) {
                // ★ **CONSERVATIVE 保守默认**:本区域无此地址 ⇒ **不能**证明无人持有 ⇒ 广播
                view = LocalView{};
                result = LookupResult::BROADCAST;
                return;
            }

            view = LocalView{};
            view.valid = true;
            view.sharers = it->second.sharers;
            view.dirty = it->second.dirty;

            // 位集是 sharer 集的**上界** ⇒ 掩码只含 1 位 ⇒ 不可能有别的 sharer ⇒ 单点转发安全。
            // ⚠️ **但这只是 hint**(`authority=LOCAL_HINT`)⇒ 消费方**不得**当权威
            //    ⇒ `forward_to` 承载的是「hint 的目标」(语义重载,per CDX-05)。
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

        // ── 观测(测试夹具 / 调用方诊断)──────────────────────────────────

        /// 本区域该地址的本地视图(区域外 ⇒ `valid == false`)
        LocalView view_of(std::uint64_t addr) const {
            LocalView v;
            if (!region_.contains(addr))
                return v;
            auto it = entries_.find(addr);
            if (it == entries_.end())
                return v;
            v.valid = true;
            v.sharers = it->second.sharers;
            v.dirty = it->second.dirty;
            return v;
        }

        /// 本区域该地址**可能**持有的节点数(诊断用;`0` = 无条目 或 已确知为空)
        std::size_t possible_sharer_count(std::uint64_t addr) const {
            auto it = entries_.find(addr);
            if (it == entries_.end())
                return 0;
            return bit_count(it->second.sharers);
        }

        /// 本 tracker 覆盖的区域(`scope=LOCAL_MEMORY_REGION` 的具体化)
        const LocalRegion& region() const {
            return region_;
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
        /// 该地址在本区域内的副本分布(**上界**,非精确)
        struct Entry {
            responder_set_t sharers = 0; ///< **可能**持有该行的节点集合(sharer 集的**上界**)
            responder_set_t dirty = 0;   ///< **可能**的脏持有者集合
        };

        std::map<std::uint64_t, Entry> entries_;
        LocalRegion region_;
    };

} // namespace tlm::cep

#endif // CPPTLM_CEP_CXL_DCOH_TRACKER_HH
