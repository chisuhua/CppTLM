// =============================================================================
// include/tlm/cep/tilelink_c_adapter.hh
//   —— TileLinkCAdapter:`tilelink_c` 协议实例 adapter(TileLink-C · SiFive 1.7.1)
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: include/tlm/cep/tilelink_state_spec.hh(规约载体 · 第 0 个 change 已归档)
//       include/tlm/cep/tilelink_tree_manager.hh(TreeManager 行为实现)
//       openspec/changes/cpptlm-cep-tilelink-tree-manager/(change 4 件套)
//       台账 obj-cep-platform.md Entry #144(建档)· #141(二审落修 批 2)
//
// ── 定位(per 该 change 的 design `D-α`)─────────────────────────────────
// 本类**不是**缓存性能模型,而是**协议插件的语义载体**:
//   (1) 12 格转移查询(3 态 × 4 事件,**委托**规约载体,零自有表副本)
//   (2) 3→3 `perm` 投影(**委托**规约载体)
//   (3) 6→3 消息类映射 + **逐消息 → 6 类**归属表(R-T3)
//
// ── ⭐ 零副本原则(per 该 change 的 design `D-α`)────────────────────────
// 本 adapter **SHALL NOT** 携带自有的转换表 / 投影表副本:
//   · 规约层 `tilelink_state_spec.hh` 是 TileLink-C 语义的**唯一真源**
//     (12 格 + 逐格依据 + 钉版台账,per Entry #143 归档);
//   · 本类以 `constexpr` **委托**消费它 ⇒ 规约一改,本类**编译期即红**;
//   · **结构性保证**:本文件的**去注释后**文本 SHALL **不**含 `State::` / `Event::` /
//     `Perm::` **任何字面量**,SHALL **不**含任何**表名**
//     (`kTransitions` / `kPermProjection` / `kTable`)⇒ 本类在语法上
//     **不可能**携带自己的表,只能**调访问器**。
//     (判据:`TMA-01` ③ 的静态扫描 + 扫描器**鉴别力**自测)
//
// ── ⚠️ 两张**方向不同**的映射表(不得混为一谈)──────────────────────────
//   ① `kMessageClassMap` :**消息类 → 链路层 `CATEGORY_*` 3 类**(平台机制)
//      —— 本实例**只用 4 类**(TileLink 无 `atomic` / `query`)⇒ 相对平台方向默认表
//      **非恒等**(词表**子集**)⇒ `kMappingIsIdentity = false` + 理由必填。
//   ② `kProtocolMessageMap` :**逐消息 → 6 类消息类**(per R-T3,协议族特有)
//      —— 这才是 TileLink-C 的**真正协议差异**所在(2 处降维见下)。
// ⇒ ⚠️ 二者**不是**同一张表;`TMA-03` 分别断言。
//
// ── 边界 ────────────────────────────────────────────────────────────────
//   · **header-only**,**不进 `ModuleFactory`**(同 4 个既有 adapter)
//   · **不注册** 23 ABI · **不改** frozen header · **不扩** `CATEGORY_*` 词表
//   · **不扩** `perm` 4 值 · **不写** protocol engine(照 `moesi_gpu` 先例 = adapter-only)
//   · **新增 invariant 数 = 0**
//
// ── 依据 ────────────────────────────────────────────────────────────────
//   · 3 态 / 4 事件 / 12 格 / 3→3 投影 ⇒ `include/tlm/cep/tilelink_state_spec.hh`
//   · 逐消息 → 6 类归属表 + 2 处降维 ⇒ `ADR-CEP-12` 的
//     `## Status Update(2026-11-01, per 台账 Entry #138)`
//   · `perm` 4 值锁 ⇒ `ADR-CEP-02` CEN-Ib11 + `## Status Update(2026-11-02, per Entry #141)`
//   · 口径 ⇒ 台账 Entry #134 / #135 / #138 / #141 / #143
//   · ✅ 合规:TileLink 是 SiFive 明示的 free and open standard ⇒ 规范正文可读
//     ⇒ 语义直接来自规范;⚠️ 规范 © SiFive(非 CC-BY)⇒ **只记条款级定位 + 断言摘要**
//
// =============================================================================

#ifndef CPPTLM_CEP_TILELINK_C_ADAPTER_HH
#define CPPTLM_CEP_TILELINK_C_ADAPTER_HH

#include <cstddef>
#include <cstdint>

#include "bundles/noc_bundles_tlm.hh"
#include "tlm/cep/tilelink_state_spec.hh"

namespace tlm::cep {

    /// `tilelink_c` 协议实例 adapter —— TileLink-C 3 态 × 4 事件 + 3→3 `perm` 投影
    class TileLinkCAdapter {
    public:
        // ── 3 态 / 4 事件 / 4 值 perm(**别名**规约载体,不另立词表)────────
        using State = TileLinkStateSpec::State;
        using Event = TileLinkStateSpec::Event;
        using Perm = TileLinkStateSpec::Perm;
        /// 转换格的**语义种类**(委托规约载体)
        using CellKind = TileLinkStateSpec::CellKind;

        static constexpr std::size_t kStateCount = TileLinkStateSpec::kStateCount;
        static constexpr std::size_t kEventCount = TileLinkStateSpec::kEventCount;
        static constexpr std::size_t kPermCount = TileLinkStateSpec::kPermCount;

        /// 协议标识(逐字对齐 `protocol_instances.json` 的 `adapter` 字段)
        static constexpr const char* kAdapterName = "TileLinkCAdapter";
        /// 实例 id(逐字对齐 `protocol_instances.json` 的 `id` 字段)
        static constexpr const char* kInstanceId = "tilelink_c";

        // ══════════════════════════════════════════════════════════════════
        //  (1) 转移查询 —— **委托**规约载体(零副本)
        // ══════════════════════════════════════════════════════════════════

        /// 转换格的**语义种类**(`Known` / `Unknown`)。
        static constexpr CellKind transition_kind(State s, Event e) noexcept {
            return TileLinkStateSpec::transition(s, e).kind;
        }

        /// 转换的**目标掩码**(3 位)—— ⚠️ **不是单值**:`Cap` 的**源不定**语义使目标为**集合**;
        /// `Grow`(源=`Tip`)与 `Prune`(源=`Nothing`)的**规范明确空集** ⇒ 掩码 `0`。
        static constexpr std::uint8_t transition_target_mask(State s, Event e) noexcept {
            return TileLinkStateSpec::transition(s, e).target_mask;
        }

        /// 是否为**规范明确**的转换(便捷判定;`Unknown` ⇒ `false`)。
        static constexpr bool transition_is_known(State s, Event e) noexcept {
            return TileLinkStateSpec::transition(s, e).kind == CellKind::Known;
        }

        /// 是否为**规范未覆盖**的转换(⇒ 消费方 SHALL 走**保守降级**路径)。
        static constexpr bool transition_is_unknown(State s, Event e) noexcept {
            return TileLinkStateSpec::transition(s, e).kind == CellKind::Unknown;
        }

        /// 是否为**规范明确实现自由**的转换(⇒ 消费方 SHALL 走**合法**降级路径;
        /// ⛔ 不得归入 `Known` 或 `Unknown`)。
        static constexpr bool transition_is_free(State s, Event e) noexcept {
            return TileLinkStateSpec::transition(s, e).kind == CellKind::Free;
        }

        // ══════════════════════════════════════════════════════════════════
        //  (2) 3→3 `perm` 投影 —— **委托**规约载体
        // ══════════════════════════════════════════════════════════════════

        /// 稳定态 → `perm` 投影(3→3;**无 `UNIQUE` 行** —— 干净独占在 TileLink-C 不可确证)。
        static constexpr bool perm_of(State s, Perm& out) noexcept {
            return TileLinkStateSpec::perm_of(s, out);
        }

        /// 瞬态态**不建模**(`Trunk` 已被**折叠**为 `Branch`,不是独立瞬态)⇒ 计数恒 0。
        static constexpr std::uint8_t kTransientStates[] = {0xFF};
        static constexpr std::size_t kTransientStateCount = 0;

        // ══════════════════════════════════════════════════════════════════
        //  (3) 消息类映射
        // ══════════════════════════════════════════════════════════════════

        /// 一行映射(消息类名 → 链路层 flit 类别)
        struct MessageClassMapping {
            const char* message_class;
            std::uint8_t category;
        };

        /// 本实例**实际使用**的 6 类消息类 → `CATEGORY_*` 映射。
        ///
        /// ⚠️ TileLink-C **不使用** `atomic` / `query` 两类(per R-T3 的逐消息归属表)
        /// ⇒ 本表是平台方向默认表的**词表子集**(4 行)⇒ 相对默认表**非恒等**。
        static constexpr std::size_t kMessageClassCount = 4;
        static constexpr MessageClassMapping kMessageClassMap[kMessageClassCount] = {
            {"request", bundles::NoCFlitBundle::CATEGORY_REQUEST},
            {"snoop", bundles::NoCFlitBundle::CATEGORY_REQUEST},
            {"snoop_resp", bundles::NoCFlitBundle::CATEGORY_RESPONSE},
            {"response", bundles::NoCFlitBundle::CATEGORY_RESPONSE},
        };

        /// 本实例的映射是否恒等(相对平台方向默认表)—— **否**(词表子集)
        static constexpr bool kMappingIsIdentity = false;

        /// 非恒等理由(恒等 / 非恒等都**不得静默放过** —— 必须被登记)
        static constexpr const char* kIdentityReason =
            "TileLink-C 的 6 类消息类词表**只用 4 类**(`request` / `snoop` / `snoop_resp` / "
            "`response`;"
            "无 `atomic`、无 `query` —— per `ADR-CEP-12` 的 R-T3 逐消息归属表)"
            "⇒ 本表是平台方向默认表的**词表子集** ⇒ 相对默认表**非恒等**。"
            "⚠️ **非恒等的成因是「词表子集」而非「方向不同」** —— 这 4 行的方向与默认表**逐一相同**;"
            "⚠️ 本协议族的**真正协议差异**在**逐消息 → 6 类**层(见 `kProtocolMessageMap` 与"
            "`kMessageClassDowngradeNote`)⇒ 那 2 处降维**不在**本表内。";

        /// 查某个消息类的链路层类别;未知消息类返回 `false`
        static constexpr bool category_of(const char* message_class, std::uint8_t& out) noexcept {
            for (std::size_t i = 0; i < kMessageClassCount; ++i) {
                const char* a = kMessageClassMap[i].message_class;
                const char* b = message_class;
                while (*a && *a == *b) {
                    ++a;
                    ++b;
                }
                if (*a == '\0' && *b == '\0') {
                    out = kMessageClassMap[i].category;
                    return true;
                }
            }
            return false;
        }

        /// 本实例用到的消息类数(与 `kMessageClassMap` 同序,供完备性判据使用)
        static constexpr std::size_t used_message_class_count() noexcept {
            return kMessageClassCount;
        }

        // ── 逐消息 → 6 类消息类(**协议族特有**,per R-T3)──────────────────
        /// 一行逐消息归属(消息名 → 6 类消息类名)
        struct ProtocolMessageMapping {
            const char* message;
            const char* message_class;
        };

        /// TileLink-C 的**逐消息 → 6 类**归属表(10 条,per `ADR-CEP-12` 的 R-T3 裁决)
        ///
        /// ⚠️ **2 处降维**(per R-T3 ③)⇒ 登记于 `kMessageClassDowngradeNote`:
        ///   ① `Release` / `ReleaseData` 归 `snoop_resp`(语义是「**自愿放弃**」,词表无对应档)
        ///   ② `GrantAck` 归 `response`(词表无「对 completion 的 ack」档)
        static constexpr std::size_t kProtocolMessageCount = 10;
        static constexpr ProtocolMessageMapping kProtocolMessageMap[kProtocolMessageCount] = {
            {"Acquire", "request"},     {"Probe", "snoop"},
            {"ProbeAck", "snoop_resp"}, {"ProbeAckData", "snoop_resp"},
            {"Release", "snoop_resp"},  {"ReleaseData", "snoop_resp"},
            {"Grant", "response"},      {"GrantData", "response"},
            {"ReleaseAck", "response"}, {"GrantAck", "response"},
        };

        /// 查某个**具体消息**的 6 类归属;未知消息返回 `false`
        static constexpr bool message_class_of(const char* message, const char*& out) noexcept {
            for (std::size_t i = 0; i < kProtocolMessageCount; ++i) {
                const char* a = kProtocolMessageMap[i].message;
                const char* b = message;
                while (*a && *a == *b) {
                    ++a;
                    ++b;
                }
                if (*a == '\0' && *b == '\0') {
                    out = kProtocolMessageMap[i].message_class;
                    return true;
                }
            }
            return false;
        }

        /// 本实例用到的具体消息数(供完备性判据使用)
        static constexpr std::size_t used_protocol_message_count() noexcept {
            return kProtocolMessageCount;
        }

        /// **2 处降维**的登记(per `ADR-CEP-12` 的 R-T3 ③)—— **不得静默沿用**
        static constexpr const char* kMessageClassDowngradeNote =
            "R-T3 的 2 处降维(per `ADR-CEP-12` 的 `## Status Update(2026-11-01, per 台账 Entry "
            "#138)`):"
            "① `Release` / `ReleaseData` 归 `snoop_resp` —— 语义是「**自愿放弃**权限」,"
            "词表**无**对应档 ⇒ 借 `snoop_resp`(同方向同通道);"
            "② `GrantAck` 归 `response` —— 词表**无**「对 completion 的 ack」档 ⇒ 与 `Grant` 同类。"
            "⚠️ 另:「canonical 10 流名」与「消息类名」是**两套词表** ⇒ `writeback` **只是流名**,"
            "**不是**消息类名 ⇒ 填 `vc_binding` 时**不得**写 `writeback`。";

        /// 非对称形态的登记(TileLink 形态,per `ADR-CEP-17` 的 R-T6 裁决)
        static constexpr const char* kAsymmetryNote =
            "TileLink-C 的非对称 = 「**权限的授予与强制撤销只由 slave 发起**」"
            "(=`Grant` / `GrantData` 只在 D 通道 · `Probe` 只在 B 通道)"
            "⇒ ⚠️ **不得**套用队列 #10 的 CXL 形态(「request 类消息只在一个方向」)—— "
            "TileLink 的 A / B **都是** request 类 ⇒ 该形态在 TileLink 上**恒假**。";
    };

} // namespace tlm::cep

#endif // CPPTLM_CEP_TILELINK_C_ADAPTER_HH
