// include/tlm/cep/mesif_state_spec.hh
// MesifStateSpec —— `mesif` 协议实例的**规约层载体**(MESIF 5 态状态机 + 5→4 `perm` 投影表)
//
// 功能:把 **MESIF** 的协议语义规约实现为**可执行**的契约载体:
//       (1) 5 稳定态 × 6 事件状态转换表(表驱动,每格带**推导依据** `basis`)
//       (2) 5→4 `perm` 投影(每格带 `basis`;瞬态**无**投影)
//       (3) 与 CHI / MOESI **协议模板**的偏离清单(逐条登记)
//       (4) 2 项**语义损失**的显式登记 + `perm` 秩的精确口径
// 作者: CppTLM Team / 日期: 2026-10-29
//
// ── 定位(per `openspec/changes/cpptlm-cep-mesif-state-spec` delta capability
//    `cep-mesif-state-spec`)──────────────────────────────────────────────
// 本类**不是**协议插件(adapter / engine),而是**规约层**的可执行载体:
//   · 队列 #9 `cpptlm-cep-mesif-adapter` 的**实现层**(adapter + 引擎 + 装配)**尚未启动**;
//   · ArchForge 侧**没有** MESIF 的状态转换规约与 5→4 投影表(per 立项证据 G1 / G2)
//     ⇒ 本载体按「**本地推导 + 逐格 `basis` + 待追认标记**」产出(per change 的 design `D-α`);
//   · 每格的 `basis.pending` 标记该格**尚未被 ArchForge 追认** ⇒ 跨仓债**机器可查**。
//
// ── 边界 ────────────────────────────────────────────────────────────────
//   · **header-only**,**不进 `ModuleFactory`**(规约层,非运行时组件)
//   · **不**实现 `MesifAdapter` / `MesifProtocolEngine`(队列 #9 的实现层)
//   · **不**改 `protocol_instance.schema.json`(M1 冻结面)· **不**改 `CohIfBundle`
//   · **不**扩 `perm` 4 值锁(per `ADR-CEP-02` CEN-Ib11)
//   · **不**引入新的 SharerTracker 实现(第 6 种 `HierarchicalDirectory` 已由
//     `ADR-CEP-04 §13` 裁决落地,本载体只**消费**它)
//
// ── 单一真源纪律 ────────────────────────────────────────────────────────
// **逐格 `basis` 只在本文件存在** —— change 的 `design.md` 只承载**推导口径 + 统计**。
// 理由(**三条**,缺一不可):
//   ① **可变性**:跨仓追认会**改**逐格 `basis`(pending ⇒ 已追认)⇒ 可变部分必须住在**载体**;
//   ② **不可变性**:归档件**不可变**(per ADR 不可变先例)⇒ 归��后回写**归档件**即篡改;
//      而 归档目录被 `.gitignore` 排除 ⇒ **新**归档件默认**不入库**(历史上 3 次 force-add
//      例外,当前已跟踪 **114** 件)⇒ 引用**尚未 force-add** 的归档路径在干净 clone 中**会断链** ⇒
//      归档件承载逐格表会「冻结」或「断链」;
//   ③ **可核对**:design 只留**口径 + 统计** ⇒ 统计由 MS-03 / MS-06 **机器核对**载体 ⇒ 无双真源。
//
// ── 判据口径边界(per change 的 design `D-ζ`)────────────────────────────
// 本载体承载的**值**在 ArchForge 逐格追认闭合前**不得**被当作「既定语义」;
// 判据锁的是**完备性 / 可审计性 / 待追认标记 / 内部自洽**,**不**锁推导值的语义正确性。

#ifndef CPPTLM_TLM_CEP_MESIF_STATE_SPEC_HH
#define CPPTLM_TLM_CEP_MESIF_STATE_SPEC_HH

#include <cstddef>
#include <cstdint>

namespace tlm::cep {

    /// `mesif` 协议实例的**规约层**载体 —— MESIF 5 态 × 6 事件 + 5→4 `perm` 投影 + 逐格依据
    class MesifStateSpec {
    public:
        // ── 5 稳定态(per ArchForge `25 号:598` 的 MESIF 状态词表)────────────
        // ⚠️ 枚举**值序刻意**让 `E = 2` / `M = 3` —— 使 `E + Wr → M` 的目标态恰为 `3`,
        //    从而**复用**队列 #6 的 AC-05 判据形态(其输入是 POD,判据本体零改动)。
        enum class State : std::uint8_t { I = 0, S = 1, E = 2, M = 3, F = 4 };

        // ── 6 事件(与 CHI 同构;MESIF 是**目录 + 转发点**模型)────────────────
        enum class Event : std::uint8_t {
            Rd = 0,        ///< local  :读(不改变本节点的所有权角色)
            RdUnique = 1,  ///< local  :取独占数据(失效他人)
            Wr = 2,        ///< local  :写(**只**在独占态 `{E, M}` 合法)
            Upgrade = 3,   ///< local  :从共享升级为独占
            SnpShared = 4, ///< remote :请求共享副本
            SnpUnique = 5  ///< remote :请求独占数据(失效全部)
        };

        // ── 4 值 perm(per `ADR-CEP-02` CEN-Ib11 的 4 值锁)─────────────────
        enum class Perm : std::uint8_t { NONE = 0, READ = 1, UNIQUE = 2, WRITEBACK = 3 };

        /// 依据强度分级(per change 的 design `D-ε.2`)
        enum class BasisKind : std::uint8_t {
            Primary = 0,    ///< **一手原文**:结论可由 ArchForge 原文定义直接推出
            Structural = 1, ///< **结构模板推导**:以 MOESI 6×6 为模板裁剪
            Derived = 2,    ///< **本仓推导**:依 MESIF 的目录 / 事件语义推出
            Analogical = 3  ///< **类比推导**:ArchForge 约束 + CHI 同型先例
        };

        static constexpr std::size_t kStateCount = 5;
        static constexpr std::size_t kEventCount = 6;
        static constexpr std::size_t kPermCount = 4; ///< 4 值锁(per `ADR-CEP-02` CEN-Ib11)
        static constexpr std::size_t kBasisCount =
            kStateCount * kEventCount + kStateCount; ///< 35 格

        /// 单格的**推导依据**。三字段**必填**(per delta spec 的 `MS-03` ①)
        struct Basis {
            BasisKind kind;   ///< 依据强度
            bool pending;     ///< **待追认**标记(与 `kind` 双向一致:`Primary` ⇒ false,其余 ⇒ true)
            const char* ref;  ///< 依据锚点(`Primary` 必含 ArchForge 定位,如 `32 号:529`)
            const char* note; ///< 该格的推导说明
        };

        /// 一条偏离登记
        struct Deviation {
            State state;                 ///< 当前态
            Event event;                 ///< 事件
            const char* template_target; ///< 模板(CHI / MOESI)在该格的取值;`—` = **无对应态**
            const char* reason;          ///< 偏离理由
        };

        /// 规约的**完整数据**(转换表 + 逐格依据 + 投影表 + 逐格依据)
        struct Table {
            std::uint8_t transition[kStateCount][kEventCount]; ///< 目标态;`0xFF` = **未定义**(空洞)
            Basis transition_basis[kStateCount][kEventCount];  ///< 逐格依据
            Perm projection[kStateCount];                      ///< 5 态 → 4 值投影
            Basis projection_basis[kStateCount];               ///< 逐格依据
        };

        // ══════════════════════════════════════════════════════════════════
        //  (1) 规约的**规范表**(唯一真源)
        // ══════════════════════════════════════════════════════════════════
        //
        // ⚠️ **数据只存在这一处** —— 转换表、逐格依据、投影表、投影依据**同处** `kTable`;
        //    派生视图(`transition()` / `perm_of()` 等)一律从它读 ⇒ **无双份数据**。
        //    反向 fixture 也只需「拷贝 `kTable` → 改一格 → 跑同一批谓词」。
        //
        /// 状态转换表 —— 行 = 当前态,列 = 事件;值 = 目标态。**索引**与枚举一致。
        ///
        /// 逐格语义(推导依据见同位置的 `transition_basis`):
        /// | 事件 | 源 | From → To |
        /// |---|---|---|
        /// | `Rd` | local | `I→S` · `S→S` · `E→E` · `M→M` · `F→F` |
        /// | `RdUnique` | local | `I→E` · `S→E` · `E→E` · `M→M` · `F→E` |
        /// | `Wr` | local | `E→M` · `M→M`;**`I` / `S` / `F` 上写非法 ⇒ 状态不变** |
        /// | `Upgrade` | local | `S→E` · `F→E`;其余保持 |
        /// | `SnpShared` | remote | `E→F`(保留 Owner 身份)· `M→S`(先写回)· `S→S` · `F→F` |
        /// | `SnpUnique` | remote | 任何有副本的态 → `I` |
        static constexpr Table kTable{
            // ── 转换表(目标态)────────────────────────────────────────────
            {
                {1, 2, 0, 0, 0, 0}, // I : Rd→S / RdUnique→E / SnpShared→I(不变)
                {1, 2, 1, 2, 1, 0}, // S : RdUnique→E / Upgrade→E / Wr→S(不变)
                {2, 2, 3, 2, 4, 0}, // E : Wr→M / SnpShared→F
                {3, 3, 3, 3, 1, 0}, // M : SnpShared→S(先写回)
                {4, 2, 4, 2, 4, 0}, // F : RdUnique→E / Upgrade→E / SnpShared→F
            },
            // ── 转换表的逐格依据(kind / pending / ref / note)────────────
            {
                {
                    {BasisKind::Structural, true, "ArchForge 09 号 §3.3:150",
                     "读缺失 ⇒ 取共享干净副本(MOESI I+Rd→S 同构)"},
                    {BasisKind::Structural, true,
                     "ArchForge 09 号 §3.3:150; include/tlm/cep/chi_rnf_adapter.hh:108",
                     "取独占干净副本(CHI I+RdUnique→UC 同构)"},
                    {BasisKind::Derived, true, "include/tlm/cep/chi_rnf_adapter.hh:101-105",
                     "无副本可写 ⇒ 写非法 ⇒ 状态不变(CHI 非法事件 ⇒ 不变约定)"},
                    {BasisKind::Derived, true, "include/tlm/cep/chi_rnf_adapter.hh:109",
                     "无共享副本可升级 ⇒ 状态不变"},
                    {BasisKind::Derived, true, "include/tlm/cep/chi_rnf_adapter.hh:108",
                     "无副本可降级 ⇒ 状态不变(偏离 CHI 的 I+SnpShared→SC,见偏离清单 D1)"},
                    {BasisKind::Derived, true, "include/tlm/cep/chi_rnf_adapter.hh:108",
                     "无副本可失效 ⇒ 状态不变"},
                },
                {
                    {BasisKind::Structural, true, "ArchForge 09 号 §3.3:151",
                     "共享命中 ⇒ 保持共享"},
                    {BasisKind::Derived, true, "ArchForge 25-nsa:553-554",
                     "取独占 ⇒ 失效他 sharer ⇒ 本节点成干净独占(偏离 CHI 的 "
                     "SC+RdUnique→SC,见偏离清单 D3)"},
                    {BasisKind::Analogical, true, "ArchForge 25-nsa:553-554; IHI0050 §B4.1",
                     "写只在独占态合法 ⇒ 共享态写状态不变(须先 Upgrade)"},
                    {BasisKind::Derived, true, "ArchForge 25-nsa:553-554",
                     "共享 → 独占升级(失效 sharer)"},
                    {BasisKind::Derived, true, "ArchForge 25-nsa:599", "已是 sharer ⇒ 保持共享"},
                    {BasisKind::Derived, true, "ArchForge 25-nsa:553", "独占 snoop ⇒ 失效本副本"},
                },
                {
                    {BasisKind::Structural, true, "ArchForge 09 号 §3.3:152",
                     "独占命中 ⇒ 保持独占干净"},
                    {BasisKind::Derived, true, "ArchForge 25-nsa:553-554", "已是唯一持有者 ⇒ 保持"},
                    {BasisKind::Analogical, true, "ArchForge 09 号 §3.3:152; IHI0050 §B4.1",
                     "干净独占 → 脏独占(静默升级,无总线事务)"},
                    {BasisKind::Derived, true, "ArchForge 25-nsa:553-554", "已是独占 ⇒ 保持"},
                    {BasisKind::Primary, false, "ArchForge 25-nsa:599",
                     "唯一 Owner 收到远端共享请求 ⇒ Owner 仍单 ⇒ 直接落入 F 的定义(单 Owner + 多 "
                     "Sharer)"},
                    {BasisKind::Derived, true, "ArchForge 25-nsa:553",
                     "独占 snoop ⇒ 失效 ⇒ Owner 转移"},
                },
                {
                    {BasisKind::Structural, true, "ArchForge 09 号 §3.3:153", "脏独占命中 ⇒ 保持"},
                    {BasisKind::Derived, true, "ArchForge 25-nsa:553-554",
                     "已是唯一脏持有者 ⇒ 保持"},
                    {BasisKind::Structural, true, "ArchForge 09 号 §3.3:153", "已脏 ⇒ 保持脏独占"},
                    {BasisKind::Derived, true, "ArchForge 25-nsa:553-554", "已是独占 ⇒ 保持"},
                    {BasisKind::Derived, true, "include/tlm/cep/chi_rnf_adapter.hh:111",
                     "脏 Owner 须先写回再共享 ⇒ 降为 S(CHI UD+SnpShared→SC 同构)"},
                    {BasisKind::Derived, true, "ArchForge 25-nsa:553",
                     "独占 snoop ⇒ 失效(隐含写回)"},
                },
                {
                    {BasisKind::Structural, true, "ArchForge 09 号 §3.3:154",
                     "Forward 节点命中 ⇒ 保持转发者身份(MOESI O+Rd→O 同构)"},
                    {BasisKind::Analogical, true, "ArchForge 25-nsa:553",
                     "取独占 ⇒ sharer 全失效 ⇒ F 清空 ⇒ 干净独占"},
                    {BasisKind::Analogical, true, "ArchForge 25-nsa:554; IHI0050 §B4.1",
                     "F 非独占(有 sharer)⇒ 写非法 ⇒ 状态不变(须先 Upgrade 到 E)"},
                    {BasisKind::Analogical, true, "ArchForge 25-nsa:553",
                     "升级 ⇒ sharer 全失效 ⇒ F 清空 ⇒ 干净独占"},
                    {BasisKind::Primary, false, "ArchForge 25-nsa:599",
                     "已是单 Owner + 有 sharer ⇒ 再来 sharer ⇒ 仍满足 F 的定义"},
                    {BasisKind::Primary, false, "ArchForge 25-nsa:553; ArchForge 32 号:529",
                     "独占 snoop ⇒ Owner 转移 ⇒ Forward 清空 ⇒ 无副本"},
                },
            },
            // ── 5→4 投影表(瞬态无投影 —— 本规约不声明瞬态)──────────────
            {
                Perm::NONE,      // I
                Perm::READ,      // S
                Perm::UNIQUE,    // E
                Perm::WRITEBACK, // M
                Perm::WRITEBACK, // F  ← 与 M 同投影(语义损失 ①,已登记)
            },
            // ── 投影表的逐格依据────────────────────────────────────────
            {
                {BasisKind::Structural, true, "ArchForge 32 号 §7.2:586",
                 "MOESI 6→4 表的 I→NONE 同义迁移(状态同义)"},
                {BasisKind::Structural, true, "ArchForge 32 号 §7.2:587", "同上(S→READ)"},
                {BasisKind::Structural, true, "ArchForge 32 号 §7.2:588", "同上(E→UNIQUE)"},
                {BasisKind::Structural, true, "ArchForge 32 号 §7.2:589", "同上(M→WRITEBACK)"},
                {BasisKind::Primary, false, "ArchForge 32 号:529",
                 "一手原文:F(Forward)态 → perm 投影为 WRITEBACK"},
            },
        };

        // ══════════════════════════════════════════════════════════════════
        //  (2) 与协议模板(CHI / MOESI)的**偏离清单**
        // ══════════════════════════════════════════════════════════════════

        static constexpr std::size_t kDeviationCount = 8;

        /// 偏离清单(per change 的 design `D-ε.8`;追认优先级最高)
        static constexpr Deviation kDeviations[kDeviationCount] = {
            {State::I, Event::SnpShared, "SC",
             "D1:CHI 该格把请求者拿到共享副本写进了响应者的行;本表按语义正确性取无副本不因被 snoop "
             "而获得副本"},
            {State::E, Event::SnpShared, "SC",
             "D2:MESIF 的 Forward 语义 —— 干净 Owner 保留 Owner 身份成为转发点(CHI 的 RN "
             "侧无转发点概念)"},
            {State::S, Event::RdUnique, "SC",
             "D3:MESIF 的目录语义 —— 取独占 ⇒ sharer 全失效 ⇒ 本节点成唯一持有者"},
            {State::F, Event::RdUnique, "—",
             "D4a:MESIF 特有(F = 转发点)—— 取独占 ⇒ sharer 全失效 ⇒ F 清空 ⇒ 干净独占"},
            {State::F, Event::Wr, "—",
             "D4b:MESIF 特有 —— F 有 sharer ⇒ 非独占 ⇒ 写非法 ⇒ 状态不变(须先 Upgrade 到 E)"},
            {State::F, Event::Upgrade, "—",
             "D4c:MESIF 特有 —— 升级 ⇒ sharer 全失效 ⇒ F 清空 ⇒ 干净独占"},
            {State::F, Event::SnpShared, "—",
             "D4d:MESIF 特有 —— 已是单 Owner + 有 sharer ⇒ 再来 sharer ⇒ 仍满足 F 的定义"},
            {State::F, Event::SnpUnique, "—",
             "D4e:MESIF 特有 —— 独占 snoop ⇒ Owner 转移 ⇒ Forward 清空 ⇒ 无副本"},
        };

        // ══════════════════════════════════════════════════════════════════
        //  (3) 语义损失登记 + 秩口径
        // ══════════════════════════════════════════════════════════════════

        /// `F` 与 `M` 合并为 `WRITEBACK` 的**语义损失登记**(损失 ①)
        ///
        /// ⚠️ **与 `chi_rnf` 的同类损失措辞必须不同** ——
        /// · `chi_rnf`:`sharer_tracker` = 「不参与」(RN-F 不维护 sharer set)⇒
        /// **协议信息拓扑的固有损失**,
        ///   RN 侧**不可补偿**;
        /// · `mesif`:`sharer_tracker` = `HierarchicalDirectory`(caps
        /// `SYSTEM`/`EXACT`/`GLOBAL`/`HIERARCHICAL`,
        ///   per `ADR-CEP-04 §13`)⇒ EXACT + GLOBAL 目录**能**回答「谁持有」⇒ **可由 Sharer Tracker
        ///   补偿**。
        static constexpr const char* kPermMergeNote =
            "F 与 M 合并投影为 WRITEBACK:「是否还有其他 sharer」在 4 值接口上不可分辨 —— 该损失"
            "可由 Sharer Tracker 补偿(mesif 的 sharer_tracker = HierarchicalDirectory,caps "
            "SYSTEM/EXACT/GLOBAL/HIERARCHICAL,per ADR-CEP-04 §13 ⇒ EXACT + GLOBAL "
            "目录能回答谁持有)。";

        /// 损失 ① 的**对照说明**(与 `chi_rnf` 的措辞差异)
        ///
        /// ⚠️ **为什么拆成两个常量**:判据须能**机器**区分「本实例可补偿」与「`chi_rnf` 侧不可补偿」
        /// ⇒ 主体登记里**不得**出现「不可补偿」字样,对照说明里**必须**出现 ⇒ 措辞纪律可机器检查。
        static constexpr const char* kPermMergeNoteChiRnfContrast =
            "对照 chi_rnf:其 sharer_tracker 为「不参与」(RN-F 不维护 sharer set)⇒ "
            "那是协议信息拓扑的"
            "固有损失,RN 侧不可补偿;而 mesif 的 sharer_tracker = HierarchicalDirectory(EXACT + "
            "GLOBAL)"
            "⇒ 本损失是投影的表达力损失,可由 Sharer Tracker 补偿。两者措辞不得混用。";

        /// `F → E` 的**秩回落**登记(损失 ② —— 损失 ① 的次生效应)
        static constexpr const char* kRankRegressionNote =
            "F → E 的秩回落:因 F 没有独立的 perm 值(与 M 同投影 WRITEBACK),当 F 退出转发点角色落到 "
            "E "
            "(UNIQUE)时,观测到的秩从 3 降到 2 —— 这是损失①的次生效应,已登记为唯一允许的秩回落例外;"
            "不得以给 F 新增第 5 个 perm 值来消除(per ADR-CEP-02 CEN-Ib11 的 4 值锁)。";

        /// `perm` 秩(per change 的 design `D-ε.10`)
        static constexpr std::uint8_t perm_rank(Perm p) noexcept {
            return static_cast<std::uint8_t>(p);
        }

        /// 秩**单调不降**判据的**唯一允许例外**:`F → E`(本地取权路径上)
        static constexpr bool is_rank_regression_exception(State s, Event e) noexcept {
            return s == State::F && (e == Event::RdUnique || e == Event::Upgrade);
        }

        /// 本地事件集合(秩单调判据的作用域;远端 snoop 必然降秩,不可纳入)
        static constexpr bool is_local_event(Event e) noexcept {
            return e == Event::Rd || e == Event::RdUnique || e == Event::Wr || e == Event::Upgrade;
        }

        // ══════════════════════════════════════════════════════════════════
        //  (4) 瞬态(本规约**不声明**瞬态)
        // ══════════════════════════════════════════════════════════════════

        /// 瞬态态列表 —— 本规约的 5 个枚举值**全部**是稳定态
        /// (per `ADR-CEP-05` 的 `INV-CEP-05-IC1`:瞬态无投影 ⇒ 平凡成立)
        /// `0xFF` = **无**瞬态(哨兵);保留该列表以支持未来追认后的扩展。
        static constexpr std::uint8_t kTransientStates[] = {0xFF};
        static constexpr std::size_t kTransientStateCount = 0;

        // ══════════════════════════════════════════════════════════════════
        //  (5) 查表 API
        // ══════════════════════════════════════════════════════════════════

        /// 状态转换(表驱动)
        static constexpr std::uint8_t transition(State s, Event e) noexcept {
            return kTable.transition[static_cast<std::uint8_t>(s)][static_cast<std::uint8_t>(e)];
        }

        /// 稳定态 → 4 值 `perm` 投影(**瞬态无投影** ⇒ 返回 `false`)
        static constexpr bool perm_of(State s, Perm& out) noexcept {
            switch (s) {
            case State::I:
                out = kTable.projection[0];
                return true;
            case State::S:
                out = kTable.projection[1];
                return true;
            case State::E:
                out = kTable.projection[2];
                return true;
            case State::M:
                out = kTable.projection[3];
                return true;
            case State::F:
                out = kTable.projection[4];
                return true;
            }
            return false;
        }

        /// write 事件在**当前态**是否合法(per change 的 design `D-ε.1` 约定 C3:
        /// 写**只**在独占态 `{E, M}` 合法)
        static constexpr bool write_is_legal(State s) noexcept {
            return s == State::E || s == State::M;
        }

        /// `E + Wr → M` 的**静默升级**判定(唯一干净独占态 + 写 ⇒ 脏独占)
        static constexpr bool is_silent_upgrade(State s, Event e) noexcept {
            return s == State::E && e == Event::Wr &&
                   transition(s, e) == static_cast<std::uint8_t>(State::M);
        }

        /// 逐 adapter 装载契约三元组(与 CHI / MOESI 同款形态;供未来实现层消费)
        static constexpr State kUniqueCleanState = State::E;
        static constexpr Event kWriteEvent = Event::Wr;
        static constexpr State kUniqueDirtyState = State::M;

        /// 状态名 / 事件名 / 依据强度名(供判据与偏离清单比对)
        static constexpr const char* state_name(State s) noexcept {
            switch (s) {
            case State::I:
                return "I";
            case State::S:
                return "S";
            case State::E:
                return "E";
            case State::M:
                return "M";
            case State::F:
                return "F";
            }
            return "?";
        }

        static constexpr const char* event_name(Event e) noexcept {
            switch (e) {
            case Event::Rd:
                return "Rd";
            case Event::RdUnique:
                return "RdUnique";
            case Event::Wr:
                return "Wr";
            case Event::Upgrade:
                return "Upgrade";
            case Event::SnpShared:
                return "SnpShared";
            case Event::SnpUnique:
                return "SnpUnique";
            }
            return "?";
        }

        static constexpr const char* basis_kind_name(BasisKind k) noexcept {
            switch (k) {
            case BasisKind::Primary:
                return "Primary";
            case BasisKind::Structural:
                return "Structural";
            case BasisKind::Derived:
                return "Derived";
            case BasisKind::Analogical:
                return "Analogical";
            }
            return "?";
        }

        // ══════════════════════════════════════════════════════════════════
        //  (6) 谓词(判据的可执行形态)—— **以表为参数**,便于反向 fixture 跑「变异副本」
        // ══════════════════════════════════════════════════════════════════

        /// constexpr 字符串相等
        static constexpr bool streq(const char* a, const char* b) noexcept {
            if (a == nullptr || b == nullptr)
                return a == b;
            while (*a != '\0' && *b != '\0') {
                if (*a != *b)
                    return false;
                ++a;
                ++b;
            }
            return *a == '\0' && *b == '\0';
        }

        /// constexpr 子串查找
        static constexpr bool contains(const char* hay, const char* needle) noexcept {
            if (hay == nullptr || needle == nullptr || *needle == '\0')
                return false;
            for (const char* h = hay; *h != '\0'; ++h) {
                const char* n = needle;
                const char* p = h;
                while (*p != '\0' && *n != '\0' && *p == *n) {
                    ++p;
                    ++n;
                }
                if (*n == '\0')
                    return true;
            }
            return false;
        }

        /// `Primary` 依据**必须**指向 ArchForge 的具体位置(形如 `32 号:529` / `25-nsa:599`)
        static constexpr bool has_archforge_anchor(const char* ref) noexcept {
            return contains(ref, "号:") || contains(ref, "nsa:");
        }

        /// 转换表**完备**:无空洞(无 `0xFF`)且目标态落在合法范围
        static constexpr bool table_is_complete(const Table& t) noexcept {
            for (std::size_t s = 0; s < kStateCount; ++s) {
                for (std::size_t e = 0; e < kEventCount; ++e) {
                    const std::uint8_t v = t.transition[s][e];
                    if (v == 0xFF || v >= kStateCount)
                        return false;
                }
            }
            return true;
        }

        /// 投影表**完备**:5 态**各有**投影(瞬态不参与 —— 本规约不声明瞬态)
        static constexpr bool projection_is_complete(const Table& t) noexcept {
            for (std::size_t s = 0; s < kStateCount; ++s) {
                if (static_cast<std::uint8_t>(t.projection[s]) >= kPermCount)
                    return false;
            }
            return true;
        }

        /// 单格依据**自洽**:三字段非空 + `kind` 与 `pending` 双向一致 + `Primary` 带 ArchForge 锚点
        static constexpr bool basis_entry_is_consistent(const Basis& b) noexcept {
            if (b.ref == nullptr || b.note == nullptr)
                return false;
            if (b.ref[0] == '\0' || b.note[0] == '\0')
                return false;
            if (b.kind == BasisKind::Primary) {
                if (b.pending)
                    return false; // 一手原文 ⇒ 不得标待追认
                if (!has_archforge_anchor(b.ref))
                    return false; // 一手原文 ⇒ 必须有 ArchForge 定位
            } else {
                if (!b.pending)
                    return false; // 推导 / 类比 ⇒ 必须标待追认
            }
            return true;
        }

        /// 全部 35 格依据**自洽**
        static constexpr bool basis_is_consistent(const Table& t) noexcept {
            for (std::size_t s = 0; s < kStateCount; ++s) {
                for (std::size_t e = 0; e < kEventCount; ++e) {
                    if (!basis_entry_is_consistent(t.transition_basis[s][e]))
                        return false;
                }
            }
            for (std::size_t s = 0; s < kStateCount; ++s) {
                if (!basis_entry_is_consistent(t.projection_basis[s]))
                    return false;
            }
            return true;
        }

        /// 写规则:写**只**在独占态 `{E, M}` 合法
        ///   · 独占态(`E` / `M`)的写 ⇒ 必须**留在独占态**(脏化或保持脏);
        ///   · 非独占态(`I` / `S` / `F`)的写 ⇒ 必须**状态不变**(须先 `Upgrade` 到 `E`)。
        static constexpr bool write_rule_holds(const Table& t) noexcept {
            const std::size_t wr = static_cast<std::size_t>(Event::Wr);
            for (std::size_t s = 0; s < kStateCount; ++s) {
                const std::uint8_t tgt = t.transition[s][wr];
                if (write_is_legal(static_cast<State>(s))) {
                    if (tgt != static_cast<std::uint8_t>(State::E) &&
                        tgt != static_cast<std::uint8_t>(State::M))
                        return false; // 独占态的写必须留在独占态
                } else {
                    if (tgt != static_cast<std::uint8_t>(s))
                        return false; // 非独占态的写必须状态不变
                }
            }
            return true;
        }

        /// 秩口径(本地事件单调不降;唯一例外 `F → E`;`E + Wr → M` 严格升高)
        static constexpr bool rank_rule_holds(const Table& t) noexcept {
            for (std::size_t s = 0; s < kStateCount; ++s) {
                for (std::size_t e = 0; e < kEventCount; ++e) {
                    if (!is_local_event(static_cast<Event>(e)))
                        continue;
                    const std::uint8_t from = perm_rank(t.projection[s]);
                    const std::uint8_t to = perm_rank(t.projection[t.transition[s][e]]);
                    if (to < from) {
                        if (!is_rank_regression_exception(static_cast<State>(s),
                                                          static_cast<Event>(e)))
                            return false;
                    }
                }
            }
            // `E + Wr → M` 必须存在且秩严格升高
            const std::size_t e_i = static_cast<std::size_t>(State::E);
            const std::size_t wr = static_cast<std::size_t>(Event::Wr);
            if (t.transition[e_i][wr] != static_cast<std::uint8_t>(State::M))
                return false;
            if (perm_rank(t.projection[t.transition[e_i][wr]]) <= perm_rank(t.projection[e_i]))
                return false;
            return true;
        }

        /// 偏离清单**自洽**:每条都真偏离 + 无重复 + `F` 行的无对应格**全部**登记
        static constexpr bool deviations_are_consistent(const Table& t) noexcept {
            return deviations_are_consistent(t, kDeviations);
        }

        /// 偏离清单谓词的**可变异形态**(以清单为参数 ⇒ 反向 fixture 可传「删了一条」的副本)
        template <std::size_t N>
        static constexpr bool deviations_are_consistent(const Table& t,
                                                        const Deviation (&devs)[N]) noexcept {
            for (std::size_t i = 0; i < N; ++i) {
                const Deviation& d = devs[i];
                const std::size_t si = static_cast<std::size_t>(d.state);
                const std::size_t ei = static_cast<std::size_t>(d.event);
                if (si >= kStateCount || ei >= kEventCount)
                    return false;
                if (t.transition[si][ei] == 0xFF)
                    return false; // 登记的格必须存在
                if (streq(state_name(static_cast<State>(t.transition[si][ei])), d.template_target))
                    return false; // 必须**真**偏离
                for (std::size_t j = i + 1; j < N; ++j) {
                    if (devs[j].state == d.state && devs[j].event == d.event)
                        return false; // 不得重复登记
                }
            }
            // `F` 行中「模板无对应」的格必须**全部**登记(除 `F + Rd`,它与 MOESI 的 `O + Rd → O`
            // 同构)
            const std::size_t fi = static_cast<std::size_t>(State::F);
            for (std::size_t e = 0; e < kEventCount; ++e) {
                if (e == static_cast<std::size_t>(Event::Rd))
                    continue;
                bool listed = false;
                for (std::size_t i = 0; i < N; ++i) {
                    if (static_cast<std::size_t>(devs[i].state) == fi &&
                        static_cast<std::size_t>(devs[i].event) == e) {
                        listed = true;
                        break;
                    }
                }
                if (!listed)
                    return false;
            }
            return true;
        }

        /// 「无投影」哨兵(`perm` 只有 4 个合法值 ⇒ 用 `0xFF` 表示「该态无投影」)
        static constexpr Perm kNoProjection = static_cast<Perm>(0xFF);

        /// 瞬态**无投影**(per `ADR-CEP-05` 的 `INV-CEP-05-IC1`)
        ///
        /// 本规约当前**不声明瞬态**(列表为空)⇒ 平凡成立;载体保留该谓词以支持未来追认后的扩展。
        static constexpr bool transients_have_no_projection(const Table& t,
                                                            const std::uint8_t* transients,
                                                            std::size_t n) noexcept {
            for (std::size_t i = 0; i < n; ++i) {
                const std::uint8_t s = transients[i];
                if (s >= kStateCount)
                    return false; // 瞬态下标越界
                if (t.projection[s] != kNoProjection)
                    return false; // 瞬态**不得**有投影
            }
            return true;
        }

        /// 待追认格数(跨仓债的量化刻度)
        static constexpr std::size_t pending_count(const Table& t) noexcept {
            std::size_t n = 0;
            for (std::size_t s = 0; s < kStateCount; ++s) {
                for (std::size_t e = 0; e < kEventCount; ++e) {
                    if (t.transition_basis[s][e].pending)
                        ++n;
                }
            }
            for (std::size_t s = 0; s < kStateCount; ++s) {
                if (t.projection_basis[s].pending)
                    ++n;
            }
            return n;
        }

        /// `Primary`(一手原文)格数
        static constexpr std::size_t primary_count(const Table& t) noexcept {
            std::size_t n = 0;
            for (std::size_t s = 0; s < kStateCount; ++s) {
                for (std::size_t e = 0; e < kEventCount; ++e) {
                    if (t.transition_basis[s][e].kind == BasisKind::Primary)
                        ++n;
                }
            }
            for (std::size_t s = 0; s < kStateCount; ++s) {
                if (t.projection_basis[s].kind == BasisKind::Primary)
                    ++n;
            }
            return n;
        }

        /// 指定 `kind` 的格数
        static constexpr std::size_t kind_count(const Table& t, BasisKind k) noexcept {
            std::size_t n = 0;
            for (std::size_t s = 0; s < kStateCount; ++s) {
                for (std::size_t e = 0; e < kEventCount; ++e) {
                    if (t.transition_basis[s][e].kind == k)
                        ++n;
                }
            }
            for (std::size_t s = 0; s < kStateCount; ++s) {
                if (t.projection_basis[s].kind == k)
                    ++n;
            }
            return n;
        }

        /// 转换表中的 `Primary` 格数
        static constexpr std::size_t transition_primary_count(const Table& t) noexcept {
            std::size_t n = 0;
            for (std::size_t s = 0; s < kStateCount; ++s) {
                for (std::size_t e = 0; e < kEventCount; ++e) {
                    if (t.transition_basis[s][e].kind == BasisKind::Primary)
                        ++n;
                }
            }
            return n;
        }

        /// 投影表中的 `Primary` 格数
        static constexpr std::size_t projection_primary_count(const Table& t) noexcept {
            std::size_t n = 0;
            for (std::size_t s = 0; s < kStateCount; ++s) {
                if (t.projection_basis[s].kind == BasisKind::Primary)
                    ++n;
            }
            return n;
        }

        // ── 统计的**期望值**(与 change 的 design `D-ε.11` 逐数一致)────────
        static constexpr std::size_t kExpectedPrimaryCount = 4;     ///< 一手原文格(3 转换 + 1 投影)
        static constexpr std::size_t kExpectedPendingCount = 31;    ///< 待追认格
        static constexpr std::size_t kExpectedStructuralCount = 11; ///< 7 转换 + 4 投影
        static constexpr std::size_t kExpectedDerivedCount = 15;    ///< 15 转换
        static constexpr std::size_t kExpectedAnalogicalCount = 5;  ///< 5 转换
        static constexpr std::size_t kExpectedTransitionPrimary = 3;
        static constexpr std::size_t kExpectedProjectionPrimary = 1;
    };

} // namespace tlm::cep

// ══════════════════════════════════════════════════════════════════════
//  编译期锁(命名空间作用域)
// ──────────────────────────────────────────────────────────────────────
//  ⚠️ **为何置于类外**:类内成员函数体在**类闭合前**尚未解析完整 ⇒ 在类内用
//     constexpr 成员函数做常量表达式求值会被拒(「before its definition is
//     complete」)⇒ 编译期锁必须置于类闭合之后。
//
//  ⚠️ **编译期锁不替代 ctest 判据**(per 判据纪律)—— Phase 3 的判据对**同一批性质**
//     提供**运行时**检查(正控 = 真实表,反向 fixture = 变异副本)⇒ 编译期锁与运行时
//     判据**并存**,互为冗余。
// ══════════════════════════════════════════════════════════════════════
namespace tlm::cep {

    using MesifSpec = MesifStateSpec;

    /// ① 值序锁:`E = 2` / `M = 3`(⇒ `E + Wr → M` 的目标态恰为 3,可复用队列 #6 的 AC-05 形态)
    static_assert(static_cast<std::uint8_t>(MesifSpec::State::I) == 0, "MESIF 态值序: I = 0");
    static_assert(static_cast<std::uint8_t>(MesifSpec::State::S) == 1, "MESIF 态值序: S = 1");
    static_assert(static_cast<std::uint8_t>(MesifSpec::State::E) == 2, "MESIF 态值序: E = 2");
    static_assert(static_cast<std::uint8_t>(MesifSpec::State::M) == 3, "MESIF 态值序: M = 3");
    static_assert(static_cast<std::uint8_t>(MesifSpec::State::F) == 4, "MESIF 态值序: F = 4");

    /// ② 事件值序锁(与 CHI 同构)
    static_assert(static_cast<std::uint8_t>(MesifSpec::Event::Rd) == 0, "MESIF 事件值序: Rd = 0");
    static_assert(static_cast<std::uint8_t>(MesifSpec::Event::RdUnique) == 1,
                  "MESIF 事件值序: RdUnique = 1");
    static_assert(static_cast<std::uint8_t>(MesifSpec::Event::Wr) == 2, "MESIF 事件值序: Wr = 2");
    static_assert(static_cast<std::uint8_t>(MesifSpec::Event::Upgrade) == 3,
                  "MESIF 事件值序: Upgrade = 3");
    static_assert(static_cast<std::uint8_t>(MesifSpec::Event::SnpShared) == 4,
                  "MESIF 事件值序: SnpShared = 4");
    static_assert(static_cast<std::uint8_t>(MesifSpec::Event::SnpUnique) == 5,
                  "MESIF 事件值序: SnpUnique = 5");

    /// ③ `perm` 4 值锁(per `ADR-CEP-02` CEN-Ib11)
    static_assert(MesifSpec::kPermCount == 4, "perm 必须是 4 值(NONE/READ/UNIQUE/WRITEBACK)");
    static_assert(static_cast<std::uint8_t>(MesifSpec::Perm::NONE) == 0, "perm 值序: NONE = 0");
    static_assert(static_cast<std::uint8_t>(MesifSpec::Perm::READ) == 1, "perm 值序: READ = 1");
    static_assert(static_cast<std::uint8_t>(MesifSpec::Perm::UNIQUE) == 2, "perm 值序: UNIQUE = 2");
    static_assert(static_cast<std::uint8_t>(MesifSpec::Perm::WRITEBACK) == 3,
                  "perm 值序: WRITEBACK = 3");

    /// ④ 转换表**完备**(无空洞)且目标态合法
    static_assert(MesifSpec::table_is_complete(MesifSpec::kTable),
                  "MESIF 转换表有空洞或目标态越界");

    /// ⑤ 投影表**完备**(5 态各有 4 值投影)
    static_assert(MesifSpec::projection_is_complete(MesifSpec::kTable), "MESIF 投影表不完备");

    /// ⑥ 全部 35 格依据自洽(三字段非空 + kind/pending 双向一致 + Primary 带 ArchForge 锚点)
    static_assert(MesifSpec::basis_is_consistent(MesifSpec::kTable), "MESIF 规约的逐格依据不自洽");

    /// ⑦ 写规则:独占态的写留在独占态;非独占态的写状态不变
    static_assert(MesifSpec::write_rule_holds(MesifSpec::kTable), "MESIF 写规则被破坏");

    /// ⑧ 秩口径(本地事件单调不降 + `E + Wr → M` 严格升高)
    static_assert(MesifSpec::rank_rule_holds(MesifSpec::kTable), "MESIF 秩口径被破坏");

    /// ⑨ 偏离清单自洽
    static_assert(MesifSpec::deviations_are_consistent(MesifSpec::kTable), "MESIF 偏离清单不自洽");

    /// ⑩ 统计与 design `D-ε.11` 逐数一致
    static_assert(MesifSpec::primary_count(MesifSpec::kTable) == MesifSpec::kExpectedPrimaryCount,
                  "MESIF 一手原文格数与 design 不一致");
    static_assert(MesifSpec::pending_count(MesifSpec::kTable) == MesifSpec::kExpectedPendingCount,
                  "MESIF 待追认格数与 design 不一致");
    static_assert(MesifSpec::kind_count(MesifSpec::kTable, MesifSpec::BasisKind::Structural) ==
                      MesifSpec::kExpectedStructuralCount,
                  "MESIF Structural 格数与 design 不一致");
    static_assert(MesifSpec::kind_count(MesifSpec::kTable, MesifSpec::BasisKind::Derived) ==
                      MesifSpec::kExpectedDerivedCount,
                  "MESIF Derived 格数与 design 不一致");
    static_assert(MesifSpec::kind_count(MesifSpec::kTable, MesifSpec::BasisKind::Analogical) ==
                      MesifSpec::kExpectedAnalogicalCount,
                  "MESIF Analogical 格数与 design 不一致");
    static_assert(MesifSpec::transition_primary_count(MesifSpec::kTable) ==
                      MesifSpec::kExpectedTransitionPrimary,
                  "MESIF 转换表一手原文格数与 design 不一致");
    static_assert(MesifSpec::projection_primary_count(MesifSpec::kTable) ==
                      MesifSpec::kExpectedProjectionPrimary,
                  "MESIF 投影表一手原文格数与 design 不一致");

    /// ⑪ 瞬态:本规约**不声明**瞬态(per `INV-CEP-05-IC1` ⇒ 平凡成立)
    static_assert(MesifSpec::kTransientStateCount == 0, "MESIF 当前不声明瞬态");

} // namespace tlm::cep

#endif // CPPTLM_TLM_CEP_MESIF_STATE_SPEC_HH
