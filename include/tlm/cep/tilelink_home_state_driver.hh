// include/tlm/cep/tilelink_home_state_driver.hh
// TileLink-C home 侧**消息簿记层** + tracker 侧观测面 —— 档 2 第二族(`tilelink_c`)的载体
//
// 定位(per 登记册 §6.4 队列 2 的 I-7b · architect 裁决
//   `.rddf/reviews/cep-i7b-adjudication-2026-11-21.md`):
//   · **消息簿记层**(被动:把输入流翻译为树表更新;⛔ 不自主发 `Probe`/`Grant`,
//     不做超时与 transient 处理)
//   · **观测点 = 驱动边界**(调用前后的快照)⇒ ⛔ **不得**放进 `TreeManager` 内部
//     (那会观测它自己维持的不变量 ⇒ 同义反复 ⇒ 必绿)
//
// ⚠️ **不进引擎**:受 `openspec/specs/cep-tilelink-runtime-engine/spec.md` 的 TLE-05
//    约束(引擎事件面 SHALL 只声明事件类别,SHALL NOT 预设 from-state)⇒ 本文件
//    **不在**引擎源文件内 ⇒ 不被 TLE-05 的单文件 lint 命中(该 lint 顺带充当防搬移守卫)。
//
// ⚠️ **零自造表**:安装表**只**声明「消息 ⇒ canonical 意图 ⇒ 事件锚点 + 载荷域」,
//    事件与目标掩码**一律经规约层访问器只读消费**
//    (`TileLinkStateSpec::t3_consumable_event_mask` / `transition` / `mask_of`)
//    ⇒ ⛔ 禁把 T3 表 / 12 格转移表复制为自有字面量(判据 C-5 的静态扫描)。
//
// ⚠️ **诚实边界(声明 ≠ 判定)**:本组件是 home 侧**仿真模型**,⛔ **不得**宣称
//    TileLink-C 规范一致性;档 2 第二族的读法 = 「两族 / 两种载体形态 / 单子树 /
//    单 tracker 实例 / 无通过态」。
//
// 作者: CppTLM Team / 日期: 2026-11-21
#ifndef CPPTLM_CEP_TILELINK_HOME_STATE_DRIVER_HH
#define CPPTLM_CEP_TILELINK_HOME_STATE_DRIVER_HH

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

#include "tlm/cep/source_registry.hh"
#include "tlm/cep/tilelink_state_spec.hh"
#include "tlm/cep/tilelink_tree_manager.hh"

namespace tlm::cep {

    using TlHomeState = TileLinkStateSpec::State;

    /// TileLink-C 的 **10 条** home 侧消息(行序 == 安装表行序 ⇒ 判据 C-1 的行序锁)。
    enum class TlHomeMessage : std::uint8_t {
        Acquire = 0,
        Probe = 1,
        ProbeAck = 2,
        ProbeAckData = 3,
        Release = 4,
        ReleaseData = 5,
        Grant = 6,
        GrantData = 7,
        ReleaseAck = 8,
        GrantAck = 9,
    };

    inline constexpr std::size_t kTlHomeMessageCount = 10;

    /// 安装动作类别(⛔ `NoOp` = 该消息**不产生**事件 ⇒ 树表不变)。
    enum class TlHomeAction : std::uint8_t { NoOp = 0, SetState = 1 };

    /// 安装结果 —— ⚠️ `Undecidable` **≠**「零违例」(fail-closed 出口)。
    enum class TlHomeOutcome : std::uint8_t {
        Installed = 0,  ///< 已装表(目标态见 `TlHomeApplyResult::installed_state`)
        NoOp = 1,       ///< 该消息无事件 ⇒ 树表不变
        Undecidable = 2 ///< **不可判定**(载荷超域 / 源态下该目标不存在 / 前提不成立)
    };

    /// 安装语义表的**一行**(形态见 change 的 `design.md` §D-1)。
    struct TlHomeInstallRow {
        TlHomeMessage message;
        TileLinkStateSpec::PlatformIntent intent; ///< 本消息对应的 canonical 意图
        std::uint8_t payload_domain;               ///< State 位掩码;**0 = 无载荷**
        TlHomeAction action;
        TileLinkStateSpec::T3BasisKind basis; ///< ⚠️ 复用 T3 的三档(⛔ 不新造词表)
        const char* event_ref;                  ///< 事件锚点(T3 行 id · 规范条款级)
        const char* payload_ref;                ///< 载荷域锚点
        const char* note;                        ///< 逐行依据
    };

    /// **逐消息安装语义表** —— 恰 10 行(判据 C-1 的行宇宙锁)。
    ///
    /// ⚠️ **载荷域的排除项是本仓推导**(⇒ 7 行的 `payload_ref = "repo"` + 显式登记):
    ///    规约只给了 `toT`/`toB`/`toN` 记号(`TLC-01`)⇒ 「载荷 = 目标状态」有锚点,
    ///    但**每类消息允许哪几个目标态**在仓内**无逐值锚点** ⇒ ⛔ 不得表述为规范锚定。
    inline constexpr std::array<TlHomeInstallRow, kTlHomeMessageCount> kTlHomeInstallRows{{
        {TlHomeMessage::Acquire, TileLinkStateSpec::PlatformIntent::Request, 0,
         TlHomeAction::NoOp, TileLinkStateSpec::T3BasisKind::Direct, "TLC-M-01", "TLC-01",
         "D1 §8.1.1 Acquire + §8.1.2 通道D(权限随 Grant 回来)⇒ PE 侧此刻未变 ⇒ 无事件"},
        // ① Grant ⇒ Completion ⇒ `Grow`(NtoB/NtoT/BtoT)⇒ 目标 = 源态 + 载荷
        {TlHomeMessage::Probe, TileLinkStateSpec::PlatformIntent::Snoop,
         TileLinkStateSpec::kMaskNothing | TileLinkStateSpec::kMaskBranch, TlHomeAction::SetState,
         TileLinkStateSpec::T3BasisKind::SelfDerived, "TLC-M-03", "repo",
         "D1 §8.1.1 Probe + §8.1.4 Table 8.3(Prune);⚠️ 载荷域排除 Tip = 本仓推导"},
        // ② ProbeAck ⇒ SnoopResp ⇒ `Prune`(**potentially** releasing permissions)
        {TlHomeMessage::ProbeAck, TileLinkStateSpec::PlatformIntent::SnoopResp,
         TileLinkStateSpec::kMaskNothing | TileLinkStateSpec::kMaskBranch, TlHomeAction::SetState,
         TileLinkStateSpec::T3BasisKind::SelfDerived, "TLC-M-04", "repo",
         "D1 §8.1.2 通道C(potentially releasing permissions) + Table 8.3(Prune);"
         "⚠️ 「保留 vs 放弃」由 Prune 的 12 格行 + 载荷推导(⛔ 不引用 improvement 自身 ⇒ 非循环自证)"},
        // ⑥ ProbeAckData ⇒ 同 ③(方向由载荷区分,⛔ 不由消息名硬编码)
        {TlHomeMessage::ProbeAckData, TileLinkStateSpec::PlatformIntent::SnoopResp,
         TileLinkStateSpec::kMaskNothing | TileLinkStateSpec::kMaskBranch, TlHomeAction::SetState,
         TileLinkStateSpec::T3BasisKind::SelfDerived, "TLC-M-04", "repo",
         "同 ProbeAck;⚠️ 载荷域排除 Tip = 本仓推导"},
        // ④ Release ⇒ Writeback ⇒ `Prune`(自愿放弃)
        {TlHomeMessage::Release, TileLinkStateSpec::PlatformIntent::Writeback,
         TileLinkStateSpec::kMaskNothing | TileLinkStateSpec::kMaskBranch, TlHomeAction::SetState,
         TileLinkStateSpec::T3BasisKind::SelfDerived, "TLC-M-05", "repo",
         "D1 §8.1.1 Release + Table 8.3(Prune);⚠️ 载荷域排除 Tip = 本仓推导"},
        // ⑧ ReleaseData ⇒ 同 ⑤
        {TlHomeMessage::ReleaseData, TileLinkStateSpec::PlatformIntent::Writeback,
         TileLinkStateSpec::kMaskNothing | TileLinkStateSpec::kMaskBranch, TlHomeAction::SetState,
         TileLinkStateSpec::T3BasisKind::SelfDerived, "TLC-M-05", "repo",
         "同 Release(数据随放弃附着)⇒ 树表动作一致;⚠️ 载荷域排除 Tip = 本仓推导"},
        // ⑥ GrantAck ⇒ SnoopCompletionAck ⇒ **无事件**(ack 不改副本分布)
        {TlHomeMessage::Grant, TileLinkStateSpec::PlatformIntent::Completion,
         TileLinkStateSpec::kMaskBranch | TileLinkStateSpec::kMaskTip, TlHomeAction::SetState,
         TileLinkStateSpec::T3BasisKind::SelfDerived, "TLC-M-02", "repo",
         "D1 §8.1.4 Table 8.3(Grow) + §8.1.2 通道D;⚠️ 载荷域排除 Nothing = 本仓推导"},
        // ③ GrantData ⇒ 同 ⑦
        {TlHomeMessage::GrantData, TileLinkStateSpec::PlatformIntent::Completion,
         TileLinkStateSpec::kMaskBranch | TileLinkStateSpec::kMaskTip, TlHomeAction::SetState,
         TileLinkStateSpec::T3BasisKind::SelfDerived, "TLC-M-02", "repo",
         "同 Grant(数据随权限授予附着)⇒ 树表动作一致;⚠️ 载荷域排除 Nothing = 本仓推导"},
        // ⑧ Probe ⇒ Snoop ⇒ `Prune`(强制撤销发起方)
        {TlHomeMessage::ReleaseAck, TileLinkStateSpec::PlatformIntent::SnoopCompletionAck, 0,
         TlHomeAction::NoOp, TileLinkStateSpec::T3BasisKind::Direct, "TLC-M-10", "TLC-01",
         "锚点摘要 §4(ProbeAck 本身即 ack);ack 本身不改副本分布 ⇒ 无事件"},
        {TlHomeMessage::GrantAck, TileLinkStateSpec::PlatformIntent::SnoopCompletionAck, 0,
         TlHomeAction::NoOp, TileLinkStateSpec::T3BasisKind::Direct, "TLC-M-10", "TLC-01",
         "锚点摘要 §4(GrantAck 配对 completion);ack 本身不改副本分布 ⇒ 无事件"},
        // ⑩ ReleaseAck ⇒ 同 ⑩
    }};

    /// 行级 ref 解析 —— ⚠️ `TLC-*` 走**前缀**匹配(与 `resolve_t3_ref` 同款机制:
    ///    规范条款级 ⇒ `tilelink-c` = SpecClause);其余按登记面精确匹配。
    inline SourceResolution tl_home_resolve_ref(std::string_view ref) noexcept {
        if (starts_with(ref, "TLC-"))
            return resolve("tilelink-c");
        return resolve(ref);
    }

    /// **行级 basis ↔ 锚点一致性**谓词(判据 C-2)。
    /// ⚠️ `Direct` ⇒ `event_ref` **且** `payload_ref` **均**为 SpecClause;
    ///    `SelfDerived` ⇒ `event_ref` 为 SpecClause **且** `payload_ref` 为 RepoDerived
    ///    (⇒ 排除项是本仓推导 · 显式登记);`Pending` ⇒ 动作恒 NoOp。
    inline bool tl_home_basis_ref_kind_consistent(const TlHomeInstallRow& row) noexcept {
        const SourceResolution ev = tl_home_resolve_ref(row.event_ref);
        const SourceResolution pl = tl_home_resolve_ref(row.payload_ref);
        if (!ev.registered || !pl.registered)
            return false;
        switch (row.basis) {
            case TileLinkStateSpec::T3BasisKind::Direct:
                return ev.kind == RefKind::SpecClause && pl.kind == RefKind::SpecClause;
            case TileLinkStateSpec::T3BasisKind::SelfDerived:
                return ev.kind == RefKind::SpecClause && pl.kind == RefKind::RepoDerived;
            case TileLinkStateSpec::T3BasisKind::Pending:
                return row.action == TlHomeAction::NoOp;
        }
        return false;
    }

    /// 事件位掩码 → `Event` 枚举(⚠️ 位掩码 = `1 << Event 枚举值`:
    ///    `kT3EventCap=0b0001`↔`Cap=0` · `kT3EventGrow=0b0010`↔`1` ·
    ///    `kT3EventPrune=0b0100`↔`2` · `kT3EventReport=0b1000`↔`3`)。
    inline bool tl_home_event_of_mask(std::uint8_t mask, TileLinkStateSpec::Event& out) noexcept {
        if (mask == 0)
            return false;
        for (std::uint8_t e = 0; e < TileLinkStateSpec::kEventCount; ++e) {
            if (mask == static_cast<std::uint8_t>(1u << e)) {
                out = static_cast<TileLinkStateSpec::Event>(e);
                return true;
            }
        }
        return false; // ⚠️ 多位 ⇒ 非单事件
    }

    /// State 位掩码 → 唯一状态(⛔ 多位/零位 ⇒ false)。
    inline bool tl_home_state_of_mask(std::uint8_t mask, TlHomeState& out) noexcept {
        if (mask == TileLinkStateSpec::kMaskNothing) {
            out = TlHomeState::Nothing;
            return true;
        }
        if (mask == TileLinkStateSpec::kMaskBranch) {
            out = TlHomeState::Branch;
            return true;
        }
        if (mask == TileLinkStateSpec::kMaskTip) {
            out = TlHomeState::Tip;
            return true;
        }
        return false;
    }

    /// 位计数(≤ 4 位)。
    inline std::uint8_t tl_home_popcount(std::uint8_t v) noexcept {
        std::uint8_t n = 0;
        while (v != 0) {
            n = static_cast<std::uint8_t>(n + (v & 1u));
            v = static_cast<std::uint8_t>(v >> 1);
        }
        return n;
    }

    /// **安装解析**(纯函数 · 无副作用)—— 见 change `design.md` §D-1 的算法。
    ///
    /// ⚠️ **fail-closed**:载荷超域 / 目标非单值 / 源态下该目标不存在 ⇒ `Undecidable`。
    /// ⚠️ **`Cap` 行(源不定)不可直接安装** ⇒ 任何需要 `Cap` 的输入恒 `Undecidable`。
    inline TlHomeOutcome tl_home_resolve_install(TlHomeMessage msg, std::uint8_t payload_mask,
                                                TlHomeState src, TlHomeState& out_target) noexcept {
        const auto& row = kTlHomeInstallRows[static_cast<std::size_t>(msg)];
        if (row.action == TlHomeAction::NoOp) {
            if (payload_mask != 0)
                return TlHomeOutcome::Undecidable; // ⚠️ 超域(无事件的消息不该带载荷)
            out_target = src;
            return TlHomeOutcome::NoOp;
        }
        if (payload_mask == 0)
            return TlHomeOutcome::Undecidable; // ⚠️ 该行要求载荷
        if ((payload_mask & static_cast<std::uint8_t>(~row.payload_domain)) != 0)
            return TlHomeOutcome::Undecidable; // ⚠️ 超域
        if (tl_home_popcount(payload_mask) != 1)
            return TlHomeOutcome::Undecidable; // ⚠️ 目标须**单值**

        const std::uint8_t ev_mask = TileLinkStateSpec::t3_consumable_event_mask(row.intent);
        TileLinkStateSpec::Event ev = TileLinkStateSpec::Event::Report;
        if (!tl_home_event_of_mask(ev_mask, ev))
            return TlHomeOutcome::Undecidable; // ⚠️ 无事件 / 多事件 ⇒ 不可判定

        const TileLinkStateSpec::Transition& cell = TileLinkStateSpec::transition(src, ev);
        if (cell.kind != TileLinkStateSpec::CellKind::Known)
            return TlHomeOutcome::Undecidable; // ⚠️ 规范未覆盖 ⇒ 不可判定
        if ((cell.target_mask & payload_mask) == 0)
            return TlHomeOutcome::Undecidable; // ⚠️ 源态下该目标**不存在**(规范明确空集)

        TlHomeState target = TlHomeState::Nothing;
        if (!tl_home_state_of_mask(payload_mask, target))
            return TlHomeOutcome::Undecidable;
        out_target = target;
        return TlHomeOutcome::Installed;
    }

    /// **tracker 侧观测记录**(⛔ 不复用族 1 的 `EventObservation` —— 那是族 1 基底面,
    /// 且其字段表是**引擎态语义**(`event`/`from`/`to`/`data_*`)⇒ tracker 侧无引擎态迁移)。
    struct TlHomeObservation {
        std::uint64_t addr = 0;
        TlHomeMessage message = TlHomeMessage::Acquire;
        std::uint32_t node = 0;
        TlHomeAction action = TlHomeAction::NoOp;
        TlHomeState target = TlHomeState::Nothing;
        std::uint64_t sharers_before = 0; ///< **安装前**副本分布快照(`view_of`)
        std::uint64_t self_mask = 0;       ///< 自身位掩码(归因来源)
        bool self_mask_valid = false;      ///< 节点未映射 ⇒ false ⇒ 判不可判
        std::uint64_t episode_id = 0;       ///< 写 episode 归并键
        bool installed = false;             ///< 本次是否实际装表
    };

    /// `apply` 的结果。
    struct TlHomeApplyResult {
        TlHomeOutcome outcome = TlHomeOutcome::Undecidable;
        TlHomeState installed_state = TlHomeState::Nothing;
        bool observation_invalid = false; ///< ⚠️ 「不可判」⇒ ⛔ **不得**记 0 违例
        const char* invalid_reason = "";
    };

    /// **home 侧消息簿记层** —— 消息流 → 树表更新 + 驱动边界观测。
    ///
    /// ⚠️ **默认关闭**且**有界**(判据 AC-13);⚠️ **不自主发**任何消息。
    class TileLinkHomeStateDriver {
    public:
        explicit TileLinkHomeStateDriver(TreeManager& tracker) : tracker_(&tracker) {}

        // ── 观测面开关 / 容量 / 计数 ──────────────────────────────────────
        void set_observations_enabled(bool on) noexcept { observations_enabled_ = on; }
        bool observations_enabled() const noexcept { return observations_enabled_; }
        void set_observation_capacity(std::size_t cap) noexcept { capacity_ = cap; }
        std::size_t observation_capacity() const noexcept { return capacity_; }
        const std::vector<TlHomeObservation>& observations() const noexcept { return observations_; }
        std::uint64_t dropped_observations() const noexcept { return dropped_; }
        std::uint64_t invalid_observations() const noexcept { return invalid_; }

        void reset() noexcept {
            observations_.clear();
            dropped_ = 0;
            invalid_ = 0;
            episode_counter_ = 0;
            installed_by_.clear();
        }

        /// **安装台账**(溯源面)—— `addr` ⇒ 已安装的位掩码。
        /// ⚠️ **绕过驱动**直接 `set_sharer` 的位**不在**此表 ⇒ 判 `provenance_broken`。
        std::uint64_t installed_mask_of(std::uint64_t addr) const {
            auto it = installed_by_.find(addr);
            return it == installed_by_.end() ? 0u : it->second;
        }

        /// **主入口** —— 把一条消息翻译为树表动作 + 记 1 条观测。
        ///
        /// 步骤序(per `design.md` §D-3):① 装配前提检查 ② 取**安装前**快照
        /// ③ 解析安装 ④ 装表 ⑤ 记观测。
        TlHomeApplyResult apply(TlHomeMessage msg, std::uint64_t addr, node_id_t node,
                                std::uint8_t payload_mask) {
            TlHomeApplyResult res;

            // ① 装配前提(node 未映射 / 多 Tip)
            const std::uint64_t self_mask = responder_mask_of(node);
            const LocalView before = tracker_->view_of(addr); // ⚠️ const 纯查询(⛔ 不用 lookup)
            const bool node_unmapped = (self_mask == 0);
            const bool multi_tip = (before.dirty & ~self_mask) != 0;
            if (node_unmapped) {
                res.observation_invalid = true;
                res.invalid_reason = "节点未映射到 sharer 位(responder_mask_of == 0)";
            } else if (multi_tip) {
                res.observation_invalid = true;
                res.invalid_reason = "该地址 tips 已含他人(多 Tip)⇒ 装表会破坏「至多 1 个 Tip」";
            }

            // ②③ 解析安装(源态 = 树表当前树位置 ⇒ home 侧源态**可知**)
            const TlHomeState src = tracker_->position_of(addr);
            TlHomeState target = src;
            const TlHomeOutcome outcome = tl_home_resolve_install(msg, payload_mask, src, target);
            res.outcome = outcome;
            if (outcome == TlHomeOutcome::Undecidable) {
                res.observation_invalid = true;
                if (res.invalid_reason == nullptr || res.invalid_reason[0] == '\0')
                    res.invalid_reason = "载荷超域 / 源态下该目标不存在 ⇒ fail-closed 不可判定";
            }

            // ④ 装表(⛔ 多 Tip 时**不装** ⇒ 不破坏不变量;⛔ Undecidable 时**不装**)
            bool installed = false;
            if (outcome == TlHomeOutcome::Installed && !node_unmapped && !multi_tip) {
                tracker_->set_sharer(addr, node, target);
                installed = true;
                installed_by_[addr] = installed_by_[addr] | self_mask;
                res.installed_state = target;
            } else if (outcome == TlHomeOutcome::Installed) {
                // 前提不成立 ⇒ ⛔ 不装表(但仍记观测 ⇒ 该次不可判)
                res.outcome = TlHomeOutcome::Undecidable;
                res.observation_invalid = true;
            }

            // ⑤ 记观测(驱动边界 · 恰好 1 条 · 快照 = 安装前)
            if (observations_enabled_) {
                if (observations_.size() >= capacity_) {
                    ++dropped_; // ⚠️ ⛔ 不静默丢弃
                    res.observation_invalid = true;
                    if (res.invalid_reason == nullptr || res.invalid_reason[0] == '\0')
                        res.invalid_reason = "观测缓冲溢出 ⇒ 丢弃计数非零 ⇒ 不可判";
                } else {
                    TlHomeObservation o;
                    o.addr = addr;
                    o.message = msg;
                    o.node = static_cast<std::uint32_t>(node);
                    o.action = (outcome == TlHomeOutcome::Installed) ? TlHomeAction::SetState
                                                                     : TlHomeAction::NoOp;
                    o.target = target;
                    o.sharers_before = before.sharers;
                    o.self_mask = self_mask;
                    o.self_mask_valid = !node_unmapped;
                    o.episode_id = episode_counter_;
                    o.installed = installed;
                    observations_.push_back(o);
                    if (installed)
                        ++episode_counter_;
                }
            } else {
                // ⚠️ 观测面**关闭** ⇒ 无记录 ⇒ 「没观测到」⭔ ≠「无违例」⇒ 判不可判
                res.observation_invalid = true;
                if (res.invalid_reason == nullptr || res.invalid_reason[0] == '\0')
                    res.invalid_reason = "观测面未开启 ⇒ 无观测记录 ⇒ 不可判(⛔ 不是零违例)";
            }

            if (res.observation_invalid)
                ++invalid_;
            return res;
        }

    private:
        TreeManager* tracker_ = nullptr;
        bool observations_enabled_ = false;              // ⚠️ 默认关闭
        std::size_t capacity_ = 64;                     // ⚠️ 有界
        std::uint64_t dropped_ = 0;
        std::uint64_t invalid_ = 0;
        std::uint64_t episode_counter_ = 0;
        std::vector<TlHomeObservation> observations_;
        std::map<std::uint64_t, std::uint64_t> installed_by_;
    };

} // namespace tlm::cep

#endif // CPPTLM_CEP_TILELINK_HOME_STATE_DRIVER_HH
