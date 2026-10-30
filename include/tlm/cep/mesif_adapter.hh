// include/tlm/cep/mesif_adapter.hh
// MesifAdapter —— `mesif` 协议实例 adapter(5 态 × 6 事件 + 5→4 `perm` 投影 + 6→3 消息类映射)
//
// 功能:把 `mesif` 的协议语义实现为**可执行**的契约载体:
//       (1) 5 态 × 6 事件状态转换(**委托**规约载体,零自有表副本)
//       (2) 5→4 `perm` 投影(**委托**规约载体)
//       (3) 6→3 消息类 → 链路层 `CATEGORY_*` 映射(显式表 + 恒等登记)
// 作者: CppTLM Team / 日期: 2026-10-30
//
// ── 定位(per `openspec/changes/cpptlm-cep-mesif-adapter` delta capability
//    `cep-mesif-adapter` 的 `MA-01` ~ `MA-03`)────────────────────────────
// 本类**不是**缓存性能模型,而是**协议插件的语义载体**:
//   · 状态机 = 协议自身的 5 态语义(与 Stub Cache 的 5 态投影是**两个层级**);
//   · `perm` 投影 = 跨域接口上的 4 值权限表达(per `ADR-CEP-05` 的 `perm` 4 值锁);
//   · 消息类映射 = 「一致性层 6 类词表 → 链路层 3 类 flit 类别」的**特化字典**
//     (per `ADR-CEP-12 §8.3` ② 分层纪律 —— 两个维度正交,关系由 adapter 处的
//      显式映射表承担,**非恒等**)。
//
// ── ⭐ 零副本原则(per 本 change 的 design `D-α`)────────────────────────
// 本 adapter **SHALL NOT** 携带自有的 5×6 转换表 / 5→4 投影表副本:
//   · 规约层 `mesif_state_spec.hh` 是 MESIF 语义的**唯一真源**(35 格逐格 + 逐格依据
//     + 跨仓逐格追认已闭合,per Entry #101 / #102 / #103);
//   · 本类以 `constexpr` **委托**消费它 ⇒ 规约一改,本类**编译期即红**
//     (规约层的 14 条 `static_assert` 自动覆盖本类);
//   · **结构性保证**:本文件的**去注释后**文本 SHALL **不**含 `State::` / `Event::` /
//     `Perm::` **任何字面量** ⇒ 本类在语法上**不可能**携带自己的表。
//     (判据:`MA-01` 子条件 ③ 的静态扫描 + 35 格 pass-through 抽查)
//
// ── 边界 ────────────────────────────────────────────────────────────────
//   · **header-only**,**不进 `ModuleFactory`**(testbench 定位的组件,同 `StubCacheCore`)
//   · **不注册** 23 ABI · **不改** frozen header · **不扩** `CATEGORY_*` 词表
//   · **不扩** `perm` 4 值 · **不**引入新的 SharerTracker 实现
//     (第 6 种 `HierarchicalDirectory` 由 `cep-hierarchical-directory` 承担)
//   · **新增 invariant 数 = 0**
//
// ── 依据 ────────────────────────────────────────────────────────────────
//   · 5 态 / 6 事件 / 转换表 / 投影表 ⇒ `include/tlm/cep/mesif_state_spec.hh`
//     (规约载体,规范性来源;逐格依据见该文件的 `Basis` 表)
//   · 6 类消息类 + 方向 ⇒ ArchForge `32 号` §6.0.1 · 词表分层 ⇒ `ADR-CEP-12 §8.3`
//   · `perm` 4 值锁 ⇒ `ADR-CEP-02` CEN-Ib11 · 静默升级 ⇒ `ADR-CEP-13` 决策 3
//   · 语义损失 ①(`F`/`M` 合并)与对照措辞 ⇒ 规约载体的 `kPermMergeNote` /
//     `kPermMergeNoteChiRnfContrast`(**委托**,不复制散文 ⇒ 无双真源)

#ifndef CPPTLM_TLM_CEP_MESIF_ADAPTER_HH
#define CPPTLM_TLM_CEP_MESIF_ADAPTER_HH

#include <cstddef>
#include <cstdint>

#include "bundles/noc_bundles_tlm.hh"
#include "tlm/cep/mesif_state_spec.hh"

namespace tlm::cep {

    /// `mesif` 协议实例 adapter —— 5 态 × 6 事件 + 5→4 `perm` 投影 + 6→3 消息类映射
    class MesifAdapter {
    public:
        // ── 5 态 / 6 事件 / 4 值 perm(**别名**规约载体,不另立词表)────────
        using State = MesifStateSpec::State;
        using Event = MesifStateSpec::Event;
        using Perm = MesifStateSpec::Perm;

        static constexpr std::size_t kStateCount = MesifStateSpec::kStateCount;
        static constexpr std::size_t kEventCount = MesifStateSpec::kEventCount;
        static constexpr std::size_t kMessageClassCount = 6;

        /// 协议标识(逐字对齐 `protocol_instances.json` 的 `adapter` 字段)
        static constexpr const char* kAdapterName = "MesifAdapter";
        /// 实例 id(逐字对齐 `protocol_instances.json` 的 `id` 字段)
        static constexpr const char* kInstanceId = "mesif";

        // ══════════════════════════════════════════════════════════════════
        //  (1) 5 态 × 6 事件状态转换 —— **委托**规约载体(零副本)
        // ══════════════════════════════════════════════════════════════════

        /// 状态转换(表驱动;**唯一真源** = `MesifStateSpec::kTable`)
        /// **索引**与枚举一致(`State`/`Event` 的底层值即下标)。
        ///
        /// 逐格语义(推导依据见规约载体的 `transition_basis`):
        /// | 事件 | 源 | From → To |
        /// |---|---|---|
        /// | `Rd` | local | `I→S` · `S→S` · `E→E` · `M→M` · `F→F` |
        /// | `RdUnique` | local | `I→E` · `S→E` · `E→E` · `M→M` · `F→E` |
        /// | `Wr` | local | `E→M` · `M→M`;**`I` / `S` / `F` 上写非法 ⇒ 状态不变** |
        /// | `Upgrade` | local | `S→E` · `F→E`;其余保持 |
        /// | `SnpShared` | remote | `E→F`(保留 Owner 身份)· `M→F`(先写回,保留转发点)· `S→S` · `F→F`
        /// | | `SnpUnique` | remote | 任何有副本的态 → `I` |
        static constexpr std::uint8_t transition(State s, Event e) noexcept {
            return MesifStateSpec::transition(s, e);
        }

        /// 稳定态 → 4 值 `perm` 投影(**瞬态无投影** ⇒ 返回 `false`)
        /// —— **委托**规约载体;4 值锁不破(per `ADR-CEP-02` CEN-Ib11)
        static constexpr bool perm_of(State s, Perm& out) noexcept {
            return MesifStateSpec::perm_of(s, out);
        }

        /// write 事件在**当前态**是否合法(写**只**在独占态 `{E, M}` 合法)
        static constexpr bool write_is_legal(State s) noexcept {
            return MesifStateSpec::write_is_legal(s);
        }

        /// `E + Wr → M` 的**静默升级**判定(唯一干净独占态 + 写 ⇒ 脏独占)
        static constexpr bool is_silent_upgrade(State s, Event e) noexcept {
            return MesifStateSpec::is_silent_upgrade(s, e);
        }

        /// 瞬态态列表 —— 本规约的 5 个态**全部**是稳定态
        /// (`0xFF` = **无**瞬态(哨兵));保留该列表以支持未来扩展。
        static constexpr std::uint8_t kTransientStates[] = {0xFF};
        static constexpr std::size_t kTransientStateCount = 0;

        // ── 逐 adapter 装载契约三元组(**委托**规约载体)──────────────────
        static constexpr State kUniqueCleanState = MesifStateSpec::kUniqueCleanState;
        static constexpr Event kWriteEvent = MesifStateSpec::kWriteEvent;
        static constexpr State kUniqueDirtyState = MesifStateSpec::kUniqueDirtyState;

        // ══════════════════════════════════════════════════════════════════
        //  (2) 语义损失登记(**委托**规约载体 ⇒ 散文无双真源)
        // ══════════════════════════════════════════════════════════════════

        /// `F` 与 `M` 合并为 `WRITEBACK` 的**语义损失 ①** 登记
        /// —— 措辞与 `chi_rnf` 的同类损失**可机器区分**(本实例**可补偿** /
        ///    `chi_rnf` **不可补偿**),per 规约载体的 `D-ε.8` 措辞纪律。
        static constexpr const char* kPermMergeNote = MesifStateSpec::kPermMergeNote;

        /// 损失 ① 的**对照说明**(与 `chi_rnf` 的措辞差异)
        static constexpr const char* kPermMergeNoteChiRnfContrast =
            MesifStateSpec::kPermMergeNoteChiRnfContrast;

        /// 语义损失 ③ —— 纠正 `M + SnpShared`(`S`→`F`)之后,「已写回(干净)」与「仍脏」
        /// 在 4 值接口上**同值不可分辨** ⇒ **写回事实在 perm 接口上不可见**
        /// ⇒ 补偿维度 = **Sharer Tracker 的 dirty 掩码 + 写回接线**(非「谁持有」——
        /// 后者是语义损失 ① 的维度,二者**不同**)。
        static constexpr const char* kPermMergeWritebackPathNote =
            MesifStateSpec::kPermMergeWritebackPathNote;

        // ══════════════════════════════════════════════════════════════════
        //  (3) 6→3 消息类 → 链路层 `CATEGORY_*` 映射
        // ══════════════════════════════════════════════════════════════════

        /// 一行映射(消息类名 → 链路层 flit 类别)
        struct MessageClassMapping {
            const char* message_class;
            std::uint8_t category;
        };

        /// 本实例**实际使用**的 6 类消息类 → `CATEGORY_*` 映射
        /// (per ArchForge `32 号` §6.0.1 的 6 类词表 + 方向列)。
        ///
        /// 映射轴 = **链路事务视角**:需要应答的走 `CATEGORY_REQUEST`,应答走 `CATEGORY_RESPONSE`
        /// (node→center 的 `request`/`atomic`/`query` 与 center→node 的 `snoop` 都是
        ///  「发起一次需要应答的链路事务」;`response`/`snoop_resp` 是应答)。
        /// ⇒ 本表 **== 平台方向默认表**(见 `adapter_schema_contracts.hh` 的
        /// `identity_reference_map()`),故本实例**恒等** —— 恒等是合法状态,
        /// 但必须**被登记**(per delta spec「若某实例的映射确实恒等,则 SHALL 显式声明恒等
        /// 并携带理由」)。**不**为了「看起来非恒等」而破坏链路层的 credit 记账语义。
        ///
        /// ⚠️ **`forward_data` SHALL NOT 出现在本词表** —— 它是 canonical 流 6,
        /// 是 **`snoop` 槽的承载实例**(per `ADR-CEP-12 §8.3` 补充约定)。
        static constexpr MessageClassMapping kMessageClassMap[kMessageClassCount] = {
            {"request", bundles::NoCFlitBundle::CATEGORY_REQUEST},
            {"response", bundles::NoCFlitBundle::CATEGORY_RESPONSE},
            {"snoop", bundles::NoCFlitBundle::CATEGORY_REQUEST},
            {"snoop_resp", bundles::NoCFlitBundle::CATEGORY_RESPONSE},
            {"atomic", bundles::NoCFlitBundle::CATEGORY_REQUEST},
            {"query", bundles::NoCFlitBundle::CATEGORY_REQUEST},
        };

        /// 本实例的映射是否恒等(相对平台方向默认表)
        static constexpr bool kMappingIsIdentity = true;

        /// 恒等理由(恒等是合法状态,但**不得静默放过** —— 必须被登记)
        static constexpr const char* kIdentityReason =
            "mesif 的 6 类消息类在**链路事务视角**下与平台方向默认表逐一相同"
            "(request/atomic/query/snoop = 发起一次需应答的事务 ⇒ REQUEST;"
            " response/snoop_resp = 应答 ⇒ RESPONSE)。本实例无协议特有偏离 ⇒ 显式登记恒等。"
            "⚠️ mesif 的 forward_data 是 canonical 流 6、是 **snoop 槽的承载实例**,"
            "按 `ADR-CEP-12 §8.3` 补充约定**不进入** 6 类词表 ⇒ 不产生映射行。";

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

        /// 本实例用到的 6 类消息类(与 `kMessageClassMap` 同序,供完备性判据使用)
        static constexpr std::size_t used_message_class_count() noexcept {
            return kMessageClassCount;
        }
    };

} // namespace tlm::cep

#endif // CPPTLM_TLM_CEP_MESIF_ADAPTER_HH
