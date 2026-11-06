// =============================================================================
// include/tlm/cep/tilelink_state_spec.hh
//   —— TileLink-C(TileLink Tree Manager)规约载体
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md(basis 唯一人工审计面)
//       openspec/changes/cpptlm-cep-tilelink-state-spec/(change 4 件套)
//       台账 obj-cep-platform.md Entry #134(Task 0 普查)· #138(三项 architect 裁决)
//                                      · #140(change 建档)· #141(二审落修 批 2)· #142(CP-02 回归修复)
//       docs/adr/ADR-CEP-11-protocol-version-locking.md §Status Update(2026-11-01)
//       docs/adr/ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md §Status Update(2026-11-02)
//
// ── 定位 ────────────────────────────────────────────────────────────────────
// **不是缓存、不是网络组件**:它不接 `connections[]`、不进 `ModuleFactory`、不注册 ABI。
// 它是 **协议语义的规约真源**:稳定态集合 / 事件词表 / 转移矩阵 / `perm` 投影 /
// 逐格 `basis` / 偏离清单 ⇒ 供第 1 个 change 的 `TileLinkCAdapter` **零自有表副本**地委托消费。
//
// ── 口径(per 台账 Entry #138 / #140 / #141)──────────────────────────────────
// ① 稳定态 = **3 值**(`Nothing` / `Branch` / `Tip`)。`Trunk` **折叠**为 `Branch`
//    —— **决策口径**(`Table 8.1` 权限等价 + `Table 8.3` 单调性),**非**规范断言;
//    `Trunk` **确实存在**于 D1 §8.1 的 4 类持久节点中。折叠代价见 `kDeviations` 的 `DEV-4`。
// ② 事件词表 = **4 类**(`Cap` / `Grow` / `Prune` / `Report`),D1 §8.1.4 `Table 8.3`。
// ③ 格形态 = **`kind ∈ {Known, Unknown}` + 3 位目标掩码** ⇒ **零非法组合**
//    (4 值枚举允许「`Known-single` 却携带 2 个目标位」这类矛盾组合)。
// ④ `perm` 投影 = **3→3**;`UNIQUE` **不可达**(规范只说副本 *may contain dirty data*
//    ⇒ 不承诺干净)⇒ 保守投影 `WRITEBACK`。
// ⑤ 非对称 = **TileLink 形态**(「权限的授予与强制撤销只由 slave 发起」)
//    ⇒ ⚠️ **不得**套用队列 #10 的 CXL 形态(「request 类消息只在一个方向」在 TileLink 上**恒假**)。
// ⑥ basis 权威 = **本仓「公开来源锚点摘要」**;`ref` 只指向锚点摘要 §2.3bis 的
//    **逐格条目号**(`TLC-01` ~ `TLC-12`)⇒ 载体与摘要**交叉核对**。
//
// ── 合规边界 ────────────────────────────────────────────────────────────────
// ✅ TileLink 是 SiFive 明示的 **free and open standard** ⇒ **规范正文可读**
//    (与队列 #10 的 CXL 情形**完全相反**:CXL 全程禁读规范正文)⇒ basis 直接来自规范。
// ⚠️ 规范 © SiFive(**非** CC-BY)⇒ 本载体**只记条款级定位 + 断言摘要**,**不整段复制**原文段落。
// ✅ 钉版 = `tilelink-c-1.7.1`(PDF sha256 `30c09b5f…a2dd`)⇒ 逐格 `basis` **全部 `direct`**。
//
// ── 上游契约(只消费,不重推)───────────────────────────────────────────────
// · `perm` 4 值锁:`NONE` / `READ` / `UNIQUE` / `WRITEBACK`(per `ADR-CEP-02` CEN-Ib11
//   + `## Status Update(2026-11-02, per 台账 Entry #141)` —— 4 值锁 = **值域锁**)
// · 通道 → VC 映射:`ADR-CEP-09 ## Status Update(2026-11-02, per 台账 Entry #141)`
//   (合并 D∪E 到 vc3;依赖集 4 条)
// · 6 类消息类词表:`ADR-CEP-12`(本载体**不**消费 —— 属实现层)
//
// =============================================================================

#ifndef CPPTLM_CEP_TILELINK_STATE_SPEC_HH
#define CPPTLM_CEP_TILELINK_STATE_SPEC_HH

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "tlm/cep/source_registry.hh"

namespace tlm::cep {

/// TileLink-C(TileLink Tree Manager)规约载体。
///
/// ⚠️ 消费方纪律:adapter SHALL **零自有表副本**(委托消费本载体)⇒ 规约改一格、adapter 必红。
class TileLinkStateSpec {
public:
    // ── 稳定态(**恰 3 态**,per 口径 ①)──────────────────────────────────────
    /// ⚠️ **无 `Trunk` 态** —— `Trunk` 被**折叠**为 `Branch`(决策口径,非规范断言)。
    enum class State : std::uint8_t { Nothing = 0, Branch = 1, Tip = 2 };

    // ── 事件词表(**恰 4 类**,per 口径 ②)───────────────────────────────────
    /// ⚠️ **枚举名表达,SHALL NOT 写死 opcode 编码** —— 编码 / 字段宽度 / credit 规则
    /// **不在本能力范围**(per delta spec 的「口径边界」)。
    enum class Event : std::uint8_t {
        Cap = 0,    ///< 权限变化**不指定原权限**,只指定**应变成什么**(`toT`/`toB`/`toN`)⇒ **源不定**
        Grow = 1,   ///< 权限**升级**(长树),记录原权限与新权限
        Prune = 2,  ///< 权限**降级**(缩树),记录原权限与新权限
        Report = 3  ///< **空操作**,权限不变,但**报告当前权限**
    };

    /// `perm` 4 值锁(per `ADR-CEP-02` CEN-Ib11)—— **只复用,不扩**。
    enum class Perm : std::uint8_t { NONE = 0, READ = 1, UNIQUE = 2, WRITEBACK = 3 };

    // ── `basis.kind` **封闭枚举**(3 档,per 口径 ⑥)──────────────────────────
    /// ⚠️ **强度不得升格**:`Analogical` / `Inferred` ⇒ `pending = true`。
    /// 本载体当前**全部**格为 `Direct`。
    enum class BasisKind : std::uint8_t {
        Direct = 0,      ///< 规范正文**明确这样写**(钉版 `pin=strong`)⇒ `pending = false`
        Analogical = 1,  ///< 跨协议 / 跨代次**类比** ⇒ `pending = true`
        Inferred = 2     ///< **本仓推导** ⇒ `pending = true`
    };

    // ── 格的**语义**(**2 值**,per 口径 ③)───────────────────────────────────
    /// ⚠️ **不是 4 值枚举**:4 值枚举允许「`Known-single` 却携带 2 个目标位」这类**非法组合**。
    /// ⇒ 「规范**明确**的空集」与「规范**未覆盖**」由 **`kind`** 区分,由**掩码内容**细分。
    enum class CellKind : std::uint8_t {
        Known = 0,    ///< 规范**明确**该转移(掩码可为空集 / 单值 / 多值)
        Free = 1,     ///< 规范**明确**「实现定义 / 自由选择」⇒ 消费者 SHALL 走**合法**降级
        Unknown = 2  ///< **规范未覆盖** ⇒ 消费者 SHALL 走降级路径,SHALL NOT 当作「非法」
    };

    // ── 目标掩码位(3 态 ⇒ 3 位)─────────────────────────────────────────────
    static constexpr std::uint8_t kMaskNothing = 0b001;  ///< bit 0 = `Nothing`
    static constexpr std::uint8_t kMaskBranch = 0b010;  ///< bit 1 = `Branch`
    static constexpr std::uint8_t kMaskTip = 0b100;     ///< bit 2 = `Tip`
    /// 合法目标位全集(3 位)—— 掩码**不得**含此外的位。
    static constexpr std::uint8_t kMaskAllStates = kMaskNothing | kMaskBranch | kMaskTip;

    // ── 规模常量(编译期锁的锚)───────────────────────────────────────────────
    static constexpr std::size_t kStateCount = 3;   ///< **恰 3**(口径 ①)
    static constexpr std::size_t kEventCount = 4;   ///< **恰 4**(口径 ②)
    static constexpr std::size_t kPermCount = 4;    ///< 4 值锁(不扩)
    /// 转移矩阵 = `kEventCount × kStateCount` = **12 格**
    static constexpr std::size_t kTransitionCellCount = kEventCount * kStateCount;

    // ── 稳定态 / 事件的**逐字**名字表(供「逐字断言」判据使用)─────────────
    /// ⚠️ **必须逐字断言**这三个名字 —— 只断言「恰为 3 个」**无鉴别力**
    /// (「把 `Tip` 换成 `Trunk`(仍 3 个)」的漂移将**不可分辨**)。
    static constexpr std::array<const char*, kStateCount> kStableStateNames{
        "Nothing", "Branch", "Tip"};

    static constexpr std::array<const char*, kEventCount> kEventNames{
        "Cap", "Grow", "Prune", "Report"};

    // ── `perm` 投影:**3→3**(口径 ④)──────────────────────────────────────────
    /// ⚠️ **无 `UNIQUE` 行** —— `UNIQUE`(干净独占)在 TileLink-C 投影中**不可确证**。
    static constexpr std::array<Perm, kStateCount> kPermProjection{
        Perm::NONE,      // Nothing —— 无副本
        Perm::READ,      // Branch  —— 只读副本
        Perm::WRITEBACK, // Tip     —— 独占写者,但**可能脏**
    };

    // ── 单格 ────────────────────────────────────────────────────────────────
    /// ⚠️ **目标用 3 位掩码**,不是单值 —— `Cap` 的**源不定**语义使目标成为**集合**。
    struct Transition {
        CellKind kind;
        std::uint8_t target_mask;  ///< 仅 `kind == Known` 时有意义;`kind == Unknown` / `Free` ⇒ MUST 为 0
        BasisKind basis;
        bool pending;
        const char* ref;   ///< 锚点摘要 §2.3bis 的**逐格条目号**(`TLC-01` ~ `TLC-12`)
        const char* note;  ///< 条款级定位 + 断言摘要(**不整段复制**规范原文)
    };

    // ── 来源类型解析(change cpptlm-cep-basis-source-kind-and-cell-free)──────
    /// 主矩阵 `ref` 形态 = `TLC-01` ~ `TLC-12`(锚点摘要条目号)⇒ 规范条款级;
    /// 空 `ref`(当前 0 格)⇒ 本仓推导。
    static constexpr SourceResolution resolve_ref(const Transition& t) noexcept {
        if (t.ref == nullptr || t.ref[0] == '\0') {
            return resolve("repo");
        }
        return resolve(starts_with(t.ref, "TLC-") ? "tilelink-c" : "repo");
    }

    /// `basis` ↔ 来源类型一致性(⛔ 按**本载体**规则,不跨载体套用 token 名)。
    static constexpr bool basis_ref_kind_consistent(const Transition& t) noexcept {
        const SourceResolution r = resolve_ref(t);
        if (!r.registered) {
            return false;
        }
        switch (t.basis) {
            case BasisKind::Direct:
                return r.kind == RefKind::SpecClause;
            case BasisKind::Analogical:
            case BasisKind::Inferred:
                return r.kind != RefKind::SpecClause;
        }
        return false;
    }

    /// **行序 SHALL 与 `Event` 枚举值一致**(index = 枚举值)—— 下方 `static_assert` 锁。
    ///
    /// ⚠️ **分桶**(唯一计数真源,per `design.md` D-γ 的逐格分桶表):
    /// `single` **5** · `multi` **5** · `empty` **2** · `unknown` **0** ⇒ 合计 **12**。
    static constexpr std::array<std::array<Transition, kStateCount>, kEventCount> kTransitions{{
        // 0 Cap —— 源不定 ⇒ 目标 = {Tip, Branch, Nothing}(3 位)
        {{
          Transition{CellKind::Known, kMaskTip | kMaskBranch | kMaskNothing, BasisKind::Direct, false,
                     "TLC-01", "D1 §8.1.4 Table 8.3:`Cap` 记号 toT/toB/toN ⇒ 目标 3 元集合(源不定)"},
          Transition{CellKind::Known, kMaskTip | kMaskBranch | kMaskNothing, BasisKind::Direct, false,
                     "TLC-02", "D1 §8.1.4 Table 8.3:`Cap` 记号 toT/toB/toN ⇒ 目标 3 元集合(源不定)"},
          Transition{CellKind::Known, kMaskTip | kMaskBranch | kMaskNothing, BasisKind::Direct, false,
                     "TLC-03", "D1 §8.1.4 Table 8.3:`Cap` 记号 toT/toB/toN ⇒ 目标 3 元集合(源不定)"}}},

        // 1 Grow —— 升级(长树)
        {{
          Transition{CellKind::Known, kMaskBranch | kMaskTip, BasisKind::Direct, false,
                     "TLC-04", "D1 §8.1.4 Table 8.3:`Grow` 记号 NtoB/NtoT ⇒ 目标 {Branch,Tip}(2 位)"},
          Transition{CellKind::Known, kMaskTip, BasisKind::Direct, false,
                     "TLC-05", "D1 §8.1.4 Table 8.3:`Grow` 记号 BtoT ⇒ 目标 {Tip}(单值)"},
          Transition{CellKind::Known, 0, BasisKind::Direct, false,
                     "TLC-06", "D1 §8.1.4 Table 8.3:`Grow` 记号仅 NtoB/NtoT/BtoT ⇒ 源=Tip **无** Grow 转移 ⇒ 规范明确空集"}}},

        // 2 Prune —— 降级(缩树)
        {{
          Transition{CellKind::Known, 0, BasisKind::Direct, false,
                     "TLC-07", "D1 §8.1.4 Table 8.3:`Prune` 记号仅 TtoB/TtoN/BtoN ⇒ 源=Nothing **无** Prune 转移 ⇒ 规范明确空集"},
          Transition{CellKind::Known, kMaskNothing, BasisKind::Direct, false,
                     "TLC-08", "D1 §8.1.4 Table 8.3:`Prune` 记号 BtoN ⇒ 目标 {Nothing}(单值)"},
          Transition{CellKind::Known, kMaskBranch | kMaskNothing, BasisKind::Direct, false,
                     "TLC-09", "D1 §8.1.4 Table 8.3:`Prune` 记号 TtoB/TtoN ⇒ 目标 {Branch,Nothing}(2 位)"}}},

        // 3 Report —— 空操作(报告当前权限)
        {{
          Transition{CellKind::Known, kMaskNothing, BasisKind::Direct, false,
                     "TLC-10", "D1 §8.1.4 Table 8.3:`Report` 记号 NtoN ⇒ 目标 {Nothing}(单值)"},
          Transition{CellKind::Known, kMaskBranch, BasisKind::Direct, false,
                     "TLC-11", "D1 §8.1.4 Table 8.3:`Report` 记号 BtoB ⇒ 目标 {Branch}(单值)"},
          Transition{CellKind::Known, kMaskTip, BasisKind::Direct, false,
                     "TLC-12", "D1 §8.1.4 Table 8.3:`Report` 记号 TtoT ⇒ 目标 {Tip}(单值)"}}},

    }};

    /// 行序自检:矩阵行序 SHALL == `Event` 枚举值。
    static_assert(static_cast<std::uint8_t>(Event::Cap) == 0, "矩阵第 0 行 SHALL 是 Cap");
    static_assert(static_cast<std::uint8_t>(Event::Grow) == 1, "矩阵第 1 行 SHALL 是 Grow");
    static_assert(static_cast<std::uint8_t>(Event::Prune) == 2, "矩阵第 2 行 SHALL 是 Prune");
    static_assert(static_cast<std::uint8_t>(Event::Report) == 3, "矩阵第 3 行 SHALL 是 Report");
    static_assert(static_cast<std::uint8_t>(Event::Report) + 1 == kEventCount,
                  "矩阵行数 SHALL == 事件数");
    static_assert(static_cast<std::uint8_t>(State::Nothing) == 0, "态 SHALL 恰为 Nothing/Branch/Tip");
    static_assert(static_cast<std::uint8_t>(State::Branch) == 1, "态 SHALL 恰为 Nothing/Branch/Tip");
    static_assert(static_cast<std::uint8_t>(State::Tip) == 2, "态 SHALL 恰为 Nothing/Branch/Tip");
    static_assert(static_cast<std::uint8_t>(State::Tip) + 1 == kStateCount, "态数 SHALL == 3");

    // ── 派生计数(硬编码常量 + 逐格统计**交叉核对** ⇒ 防「虚报覆盖率」)────────
    static constexpr std::size_t kKnownCellCount = 12;   ///< 规范**明确**的格数
    static constexpr std::size_t kUnknownCellCount = 0;  ///< 规范**未覆盖**的格数
    static constexpr std::size_t kEmptyCellCount = 2;    ///< `Known` ∧ 掩码为空集(规范明确该转移不存在)
    static constexpr std::size_t kSingleCellCount = 5;   ///< `Known` ∧ 掩码 1 位
    static constexpr std::size_t kMultiCellCount = 5;    ///< `Known` ∧ 掩码 ≥ 2 位

    /// 统计自洽 —— 防算术笔误;真值由测试用 `count_*()` 交叉核对。
    static_assert(kKnownCellCount + kUnknownCellCount == kTransitionCellCount,
                  "两态格数之和 SHALL == 总格数");
    static_assert(kEmptyCellCount + kSingleCellCount + kMultiCellCount == kKnownCellCount,
                  "Known 格 SHALL == 空集 + 单值 + 多值");
    static_assert(kEmptyCellCount == 2 && kSingleCellCount == 5 && kMultiCellCount == 5,
                  "分桶 SHALL == 2 / 5 / 5(逐格分桶表是唯一真源)");

    // ── 通道位(5 通道,per D1 §8.1.2)─────────────────────────────────────────
    static constexpr std::uint8_t kChA = 0b00001;
    static constexpr std::uint8_t kChB = 0b00010;
    static constexpr std::uint8_t kChC = 0b00100;
    static constexpr std::uint8_t kChD = 0b01000;
    static constexpr std::uint8_t kChE = 0b10000;

    /// 非对称判据涉及的**消息**(per 锚点摘要 §6 的可判定义)。
    enum class AsymMsg : std::uint8_t {
        Acquire = 0,  ///< A 通道 —— master → slave
        Probe = 1,    ///< B 通道 —— slave → master(**强制撤销**发起方)
        Grant = 2,    ///< D 通道 —— slave → master(**权限授予**)
        GrantData = 3,///< D 通道 —— slave → master(**权限授予** + 数据)
        GrantAck = 4  ///< E 通道 —— master → slave
    };

    struct AsymEntry {
        AsymMsg msg;
        std::uint8_t channel_mask;
    };

    /// 非对称判据的消息 → 通道映射(**逐消息查通道**,per 锚点摘要 §6)。
    static constexpr std::array<AsymEntry, 5> kAsymmetryMessages{{
        AsymEntry{AsymMsg::Acquire, kChA},
        AsymEntry{AsymMsg::Probe, kChB},
        AsymEntry{AsymMsg::Grant, kChD},
        AsymEntry{AsymMsg::GrantData, kChD},
        AsymEntry{AsymMsg::GrantAck, kChE},
    }};

    /// **request 类消息**的通道掩码 = A(Acquire)+ B(Probe)。
    /// ⇒ TileLink 的 request 类**双向**⇒ CXL 式「request 只在一个方向」**恒假**。
    static constexpr std::uint8_t kRequestClassChannelMask = kChA | kChB;

    // ── 偏离清单(**5 条**,per `design.md` D-γ / D-η)────────────────────────
    struct Deviation {
        const char* id;
        const char* text;
    };

    /// ⚠️ 清单 SHALL **不**把规范内部矛盾**消解**掉 ⇒ 矛盾**显式保留**,供规范修订时复核。
    static constexpr std::array<Deviation, 5> kDeviations{{
        Deviation{"DEV-1",
                  "D1 §8.1.4 正文说转移分「six subsets」而表列 4 类(下一段又说 four)⇒ 规范内部笔误,以 4 类为准"},
        Deviation{"DEV-2",
                  "D1 Table 8.3 图例行写 `Permissions: None, Branch, Trunk` 而转移记号用 T = Tip ⇒ 图例行笔误,以转移记号为准"},
        Deviation{"DEV-3",
                  "`UNIQUE` 不可达(规范只说副本 may contain dirty data ⇒ 不承诺干净)⇒ 保守投影 `WRITEBACK`,`UNIQUE` 行不存在"},
        Deviation{"DEV-4",
                  "`Trunk` 折叠为 `Branch` 的三条语义损失(副本存在性 / 脏数据 / 树位置)—— 见锚点摘要 §1.2bis"},
        Deviation{"DEV-5",
                  "`AC-05` 判据族对 TileLink 恒红(硬编码 e_write_target_state != 3 且无 SKIP 分支)⇒ 族无关化落第 1 个 change"},
    }};

    // ══════════════════════════════════════════════════════════════════════
    //  访问器
    // ══════════════════════════════════════════════════════════════════════

    /// 取格(⚠️ 调用方 SHALL 先判 `kind`;`Unknown` 的 `target_mask` **无意义**)。
    static constexpr Transition transition(State s, Event e) noexcept {
        return kTransitions[static_cast<std::uint8_t>(e)][static_cast<std::uint8_t>(s)];
    }

    /// 状态 → 目标掩码位。
    static constexpr std::uint8_t mask_of(State s) noexcept {
        switch (s) {
            case State::Nothing: return kMaskNothing;
            case State::Branch: return kMaskBranch;
            case State::Tip: return kMaskTip;
        }
        return 0;
    }

    /// 稳定态 → `perm` 投影(**访问器**)。
    static constexpr Perm perm_of(State s) noexcept {
        return kPermProjection[static_cast<std::uint8_t>(s)];
    }

    /// 稳定态 → `perm` 投影(**出参**形态)—— **平台 adapter 契约面**的调用约定
    /// (per `adapter_contracts_loader.hh` 的 `fill_adapter_inputs`:`perm_of(state, out)` → `bool`)。
    /// ⚠️ **与上面的返回值形态同源**(同一张 `kPermProjection` 表)⇒ **零双真源**;
    /// ⚠️ 本 overload 于 2026-11-02 补入 —— 第 0 个 change 的载体**只**提供了返回值形态,
    ///    与平台 adapter 装载契约**不兼容** ⇒ 实现层落地时才发现(per 台账 Entry #145)。
    static constexpr bool perm_of(State s, Perm& out) noexcept {
        out = kPermProjection[static_cast<std::uint8_t>(s)];
        return true;
    }

    /// 掩码中 1 的个数。
    static constexpr std::size_t popcount(std::uint8_t v) noexcept {
        std::size_t n = 0;
        while (v != 0) {
            n += static_cast<std::size_t>(v & 1u);
            v = static_cast<std::uint8_t>(v >> 1);
        }
        return n;
    }

    /// 该格是否为「规范**明确**的空集」(掩码 0 且 `kind == Known`)。
    static constexpr bool is_empty_cell(const Transition& c) noexcept {
        return c.kind == CellKind::Known && c.target_mask == 0;
    }

    /// 该格是否为「单值」。
    static constexpr bool is_single_cell(const Transition& c) noexcept {
        return c.kind == CellKind::Known && popcount(c.target_mask) == 1;
    }

    /// 该格是否为「多值」。
    static constexpr bool is_multi_cell(const Transition& c) noexcept {
        return c.kind == CellKind::Known && popcount(c.target_mask) >= 2;
    }

    /// 单值格的唯一目标态(**非**单值 ⇒ 返回 `false`)。
    static constexpr bool single_target_of(const Transition& c, State& out) noexcept {
        if (!is_single_cell(c)) {
            return false;
        }
        if ((c.target_mask & kMaskNothing) != 0) { out = State::Nothing; return true; }
        if ((c.target_mask & kMaskBranch) != 0) { out = State::Branch; return true; }
        if ((c.target_mask & kMaskTip) != 0) { out = State::Tip; return true; }
        return false;
    }

    // ══════════════════════════════════════════════════════════════════════
    //  逐格统计(供测试**交叉核对**硬编码常量 ⇒ 防「虚报覆盖率」)
    // ══════════════════════════════════════════════════════════════════════

    template <std::size_t NE, std::size_t NS>
    static constexpr std::size_t count_kind_for(
        const std::array<std::array<Transition, NS>, NE>& m, CellKind k) noexcept {
        std::size_t n = 0;
        for (const auto& row : m) {
            for (const auto& cell : row) {
                if (cell.kind == k) {
                    ++n;
                }
            }
        }
        return n;
    }

    template <std::size_t NE, std::size_t NS>
    static constexpr std::size_t count_popcount_for(
        const std::array<std::array<Transition, NS>, NE>& m, std::size_t pc) noexcept {
        std::size_t n = 0;
        for (const auto& row : m) {
            for (const auto& cell : row) {
                if (cell.kind == CellKind::Known && popcount(cell.target_mask) == pc) {
                    ++n;
                }
            }
        }
        return n;
    }

    /// 本矩阵的逐格统计(绑定调用)。
    static constexpr std::size_t count_kind(CellKind k) noexcept {
        return count_kind_for(kTransitions, k);
    }
    static constexpr std::size_t count_popcount(std::size_t pc) noexcept {
        return count_popcount_for(kTransitions, pc);
    }

    // ══════════════════════════════════════════════════════════════════════
    //  结构性断言(**可传入矩阵 / 名字表** ⇒ 反向 fixture 的运行时可判入口)
    // ══════════════════════════════════════════════════════════════════════

    /// 字符串逐字比较(`constexpr` 友好)。
    static constexpr bool streq(const char* a, const char* b) noexcept {
        if (a == nullptr || b == nullptr) {
            return a == b;
        }
        while (*a != '\0' && *b != '\0') {
            if (*a != *b) {
                return false;
            }
            ++a;
            ++b;
        }
        return *a == *b;
    }

    /// 稳定态集合 SHALL **恰为 3 个**且**逐字**为 `Nothing` / `Branch` / `Tip`。
    template <std::size_t N>
    static constexpr bool stable_states_are_exactly_nothing_branch_tip_for(
        const std::array<const char*, N>& names) noexcept {
        return N == 3u &&
               streq(names[0], "Nothing") &&
               streq(names[1], "Branch") &&
               streq(names[2], "Tip");
    }

    /// 本表的绑定调用。
    static constexpr bool stable_states_are_exactly_nothing_branch_tip() noexcept {
        return stable_states_are_exactly_nothing_branch_tip_for(kStableStateNames);
    }

    /// 事件词表 SHALL **恰为 4 类**且**逐字**为 `Cap` / `Grow` / `Prune` / `Report`。
    template <std::size_t N>
    static constexpr bool events_are_exactly_cap_grow_prune_report_for(
        const std::array<const char*, N>& names) noexcept {
        return N == 4u &&
               streq(names[0], "Cap") &&
               streq(names[1], "Grow") &&
               streq(names[2], "Prune") &&
               streq(names[3], "Report");
    }

    /// 本表的绑定调用。
    static constexpr bool events_are_exactly_cap_grow_prune_report() noexcept {
        return events_are_exactly_cap_grow_prune_report_for(kEventNames);
    }

    /// `Trunk` SHALL **NOT** 出现在稳定态集合中。
    /// ⚠️ 这只证明「名字表里没有 `Trunk`」;**不**证明 `Trunk` 是过渡态
    ///    (它是 D1 §8.1 的 4 类持久节点之一,本载体**折叠**它 —— 决策口径)。
    template <std::size_t N>
    static constexpr bool trunk_absent_from_stable_states_for(
        const std::array<const char*, N>& names) noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            if (streq(names[i], "Trunk")) {
                return false;
            }
        }
        return true;
    }

    /// 本表的绑定调用。
    static constexpr bool trunk_absent_from_stable_states() noexcept {
        return trunk_absent_from_stable_states_for(kStableStateNames);
    }

    /// **CXL 形态**的判据:「request 类消息只在一个方向」⇒ 恰好占 1 个通道。
    /// ⚠️ 对 TileLink **恒假**(A 与 B **都是** request 类)⇒ 载体**不得**用它。
    template <std::size_t N>
    static constexpr bool cxl_style_request_direction_would_hold_for(
        const std::array<AsymEntry, N>& t) noexcept {
        std::uint8_t req = 0;
        for (const auto& e : t) {
            if (e.msg == AsymMsg::Acquire || e.msg == AsymMsg::Probe) {
                req = static_cast<std::uint8_t>(req | e.channel_mask);
            }
        }
        const std::uint8_t cls = static_cast<std::uint8_t>(req & kRequestClassChannelMask);
        return cls != 0 && (cls & static_cast<std::uint8_t>(cls - 1)) == 0;
    }

    /// **TileLink 形态**的非对称判据(3 条):
    /// ① 权限**授予**:`Grant` / `GrantData` **只**出现在 D 通道
    /// ② 权限**强制撤销**:`Probe` **只**出现在 B 通道
    /// ③ request 类**双向**(A 与 B)⇒ **不得**表征为「request 只在一个方向」
    template <std::size_t N>
    static constexpr bool asymmetry_is_tilelink_form_for(
        const std::array<AsymEntry, N>& t) noexcept {
        std::uint8_t grant_mask = 0;
        std::uint8_t probe_mask = 0;
        for (const auto& e : t) {
            switch (e.msg) {
                case AsymMsg::Grant:
                case AsymMsg::GrantData:
                    grant_mask = static_cast<std::uint8_t>(grant_mask | e.channel_mask);
                    break;
                case AsymMsg::Probe:
                    probe_mask = static_cast<std::uint8_t>(probe_mask | e.channel_mask);
                    break;
                case AsymMsg::Acquire:
                case AsymMsg::GrantAck:
                    break;
            }
        }
        const bool grant_only_d = grant_mask != 0 && (grant_mask & static_cast<std::uint8_t>(~kChD)) == 0;
        const bool probe_only_b = probe_mask != 0 && (probe_mask & static_cast<std::uint8_t>(~kChB)) == 0;
        const bool request_bidirectional = !cxl_style_request_direction_would_hold_for(t);
        return grant_only_d && probe_only_b && request_bidirectional;
    }

    /// 本表的绑定调用。
    static constexpr bool asymmetry_is_tilelink_form() noexcept {
        return asymmetry_is_tilelink_form_for(kAsymmetryMessages);
    }

    /// `basis.kind` / `pending` 的一致性(⚠️ **单向**):
    /// `Direct` ⇒ `pending == false`;`Analogical` / `Inferred` ⇒ `pending == true`。
    static constexpr bool basis_pending_consistent(BasisKind b, bool pending) noexcept {
        switch (b) {
            case BasisKind::Direct: return !pending;
            case BasisKind::Analogical:
            case BasisKind::Inferred: return pending;
        }
        return false;
    }

    /// **可传入矩阵**的校验(⚠️ 反向 fixture 的运行时可判入口,判据铁律 ③)。
    /// 校验 5 条:
    /// ① 两态格数之和 == 总格数
    /// ② `Unknown` 格:`target_mask` MUST 为 0、`ref` MUST 为空、`note` MUST 非空
    /// ③ `Known` 格:`target_mask` MUST 不含 3 态以外的位
    /// ④ `Known` 格 MUST 有非空 `ref`
    /// ⑤ `basis_pending_consistent` 逐格成立
    template <std::size_t NE, std::size_t NS>
    static constexpr bool transitions_are_valid_for(
        const std::array<std::array<Transition, NS>, NE>& m) noexcept {
        std::size_t n_known = 0;
        std::size_t n_free = 0;
        std::size_t n_unknown = 0;
        for (const auto& row : m) {
            for (const auto& cell : row) {
                switch (cell.kind) {
                    case CellKind::Known: ++n_known; break;
                    case CellKind::Free: ++n_free; break;
                    case CellKind::Unknown: ++n_unknown; break;
                }
                // ⚠️ `Free`(规范**明确**「实现定义 / 自由选择」):目标掩码**无意义** ⇒ MUST 为 0;
                //    但**须有条款级定位**(规范明确 ⇒ 有 ref)+ 非空 note;
                //    且 basis 必须是**规范明确**档(Direct)⇒ 不得是本仓推导。
                if (cell.kind == CellKind::Free) {
                    if (cell.target_mask != 0) {
                        return false;
                    }
                    if (cell.ref == nullptr || cell.ref[0] == '\0') {
                        return false;
                    }
                    if (cell.note == nullptr || cell.note[0] == '\0') {
                        return false;
                    }
                    if (cell.basis != BasisKind::Direct) {
                        return false;
                    }
                    continue;
                }
                if (cell.kind == CellKind::Unknown) {
                    if (cell.target_mask != 0) {
                        return false;
                    }
                    if (cell.ref != nullptr && cell.ref[0] != '\0') {
                        return false;
                    }
                    if (cell.note == nullptr || cell.note[0] == '\0') {
                        return false;
                    }
                    continue;
                }
                if ((cell.target_mask & static_cast<std::uint8_t>(~kMaskAllStates)) != 0) {
                    return false;
                }
                if (cell.ref == nullptr || cell.ref[0] == '\0') {
                    return false;
                }
                if (!basis_pending_consistent(cell.basis, cell.pending)) {
                    return false;
                }
            }
        }
        return n_known + n_free + n_unknown == NE * NS;
    }

    /// 本矩阵自检(= 上式的绑定调用)。
    static constexpr bool transitions_are_valid() noexcept {
        return transitions_are_valid_for(kTransitions);
    }

    /// **计数一致性** —— 硬编码派生量 vs 逐格统计的**交叉核对**(防「虚报覆盖率」)。
    /// ⚠️ **5 个量分别比对** —— 合并比对则「空集 / 单值 / 多值」的混淆**不可发现**。
    template <std::size_t NE, std::size_t NS>
    static constexpr bool counts_are_consistent_for(
        const std::array<std::array<Transition, NS>, NE>& m) noexcept {
        return count_kind_for(m, CellKind::Known) == kKnownCellCount &&
               count_kind_for(m, CellKind::Unknown) == kUnknownCellCount &&
               count_popcount_for(m, 0) == kEmptyCellCount &&
               count_popcount_for(m, 1) == kSingleCellCount &&
               (count_popcount_for(m, 2) + count_popcount_for(m, 3)) == kMultiCellCount;
    }

    /// 本矩阵的绑定调用。
    static constexpr bool counts_are_consistent() noexcept {
        return counts_are_consistent_for(kTransitions);
    }

    // ── 命名函数(供错误消息 / 核对表生成器用)──────────────────────────────
    static constexpr const char* state_name(State s) noexcept {
        switch (s) {
            case State::Nothing: return "Nothing";
            case State::Branch: return "Branch";
            case State::Tip: return "Tip";
        }
        return "?";
    }

    static constexpr const char* event_name(Event e) noexcept {
        switch (e) {
            case Event::Cap: return "Cap";
            case Event::Grow: return "Grow";
            case Event::Prune: return "Prune";
            case Event::Report: return "Report";
        }
        return "?";
    }

    static constexpr const char* basis_kind_name(BasisKind k) noexcept {
        switch (k) {
            case BasisKind::Direct: return "direct";
            case BasisKind::Analogical: return "analogical";
            case BasisKind::Inferred: return "inferred";
        }
        return "?";
    }

    static constexpr const char* cell_kind_name(CellKind k) noexcept {
        switch (k) {
            case CellKind::Known: return "Known";
            case CellKind::Unknown: return "Unknown";
        }
        return "?";
    }


    // ═══════════════════════════════════════════════════════════════════
    // T3 映射(CohIf 平台消息/意图面 → 事件轴)
    //   per 台账 Entry #241(change `cpptlm-cep-tilelink-t3-mapping-survey`)
    //   判据源:docs/architecture/07-coherence-exploration-platform.md
    //          Status Update(2026-11-06) §⑤ 的 T1~T4 判定式(T3 = 可消费)
    //   审计面:docs/research/2026-11-06-cep-tilelink-t3-mapping-anchor-summary.md
    // ═══════════════════════════════════════════════════════════════════
    //
    // ── 为什么需要它(与既有 12 格矩阵的分工)──────────────────────────────
    // · 既有 12 格矩阵 = **状态 × 事件**(态 = 某 agent 副本的权限/树位置)
    // · 本表 = **平台消息/意图 → 事件**(即「哪个平台面消息触发哪类权限转移」)
    // ⇒ 二者是**不同 artifact**:`ADR-CEP-12` Status Update(2026-11-01)的
    //   「10 行逐消息表」映射的是 **canonical 10 流 → 6 类消息类**(vc_binding 轴),
    //   ⛔ **不满足 T3**(目标轴不同)—— 本表才是 T3 那一层。
    //
    // ── PE 角色(推导,非选择)────────────────────────────────────────────
    // 本表的事件轴 = **PE 自己副本**的权限转移。PE = **master**:
    //   锚点摘要 §4 的 canonical 10 流 ↔ TL-C 消息方向映射 **5/5 一致**
    //   (`request`→A master→slave · `completion`→D slave→master · `snoop`→B slave→master
    //    · `snoop_resp`→C master→slave · `writeback`→C master→slave)
    //   ⇒ PE 发 `request`/`snoop_resp`/`writeback`,收 `completion`/`snoop` ⇒ PE = master。
    //   ⚠️ 12 格矩阵描述的是「**任一 agent 的副本**」(D1 §8.1:「a particular agent's copy」)
    //      ⇒ **不限定** slave ⇒ master 侧同样适用。
    enum class PlatformIntent : std::uint8_t {
        Request = 0,
        Completion = 1,
        Snoop = 2,
        SnoopResp = 3,
        Writeback = 4,
        ForwardData = 5,
        BackInvalidate = 6,
        AddressResolve = 7,
        Barrier = 8,
        SnoopCompletionAck = 9,
        AtomicIntent = 10
    };
    static constexpr std::size_t kPlatformIntentCount = 11;

    /// T3 行的 basis —— ⚠️ **与既有 `BasisKind` 是两套独立枚举**
    /// (既有 `BasisKind` 描述「转移格」的证据强度;本枚举描述「映射行」的证据强度)
    enum class T3BasisKind : std::uint8_t {
        Direct = 0,       ///< **规范条款级**锚(D1 §8.1.x / Table 8.3)
        SelfDerived = 1,  ///< 仓内构造(须**显式登记**;⛔ 不得当规范锚)
        Pending = 2       ///< 未锚 ⇒ **fail-closed**(消费者 SHALL NOT 消费)
    };

    /// 事件掩码位(4 事件 ⇒ 4 位)
    static constexpr std::uint8_t kT3EventCap = 0b0001;
    static constexpr std::uint8_t kT3EventGrow = 0b0010;
    static constexpr std::uint8_t kT3EventPrune = 0b0100;
    static constexpr std::uint8_t kT3EventReport = 0b1000;
    static constexpr std::uint8_t kT3EventAll = 0b1111;

    struct T3MappingCell {
        PlatformIntent intent;
        const char* row_ref;  ///< 锚点摘要的**行 id**(`TLC-M-01` ~ `TLC-M-11`)
        std::uint8_t event_mask;  ///< 0 = 该意图**不产生**事件
        T3BasisKind basis;
        const char* ref;  ///< **条款级** ref(锚点摘要 §2 的 ref 列)
    };

    /// T3 来源解析:`D1 §…`(TileLink 规范条款)⇒ `tilelink-c`;空 ⇒ 本仓推导。
    static constexpr SourceResolution resolve_t3_ref(const T3MappingCell& c) noexcept {
        if (c.ref == nullptr || c.ref[0] == '\0') {
            return resolve("repo");
        }
        if (starts_with(c.ref, "D1")) {
            return resolve("tilelink-c");
        }
        // 无独立流 / 路由 / barrier 等行:ref 指向**本仓锚点摘要**(规范条款结论的转述)
        if (starts_with(c.ref, "锚点摘要")) {
            return resolve("tilelink-t3-anchor");
        }
        return resolve("repo");
    }

    static constexpr bool t3_basis_ref_kind_consistent(const T3MappingCell& c) noexcept {
        const SourceResolution r = resolve_t3_ref(c);
        if (!r.registered) {
            return false;
        }
        switch (c.basis) {
            case T3BasisKind::Direct:
                return r.kind == RefKind::SpecClause;
            case T3BasisKind::SelfDerived:
                return r.kind == RefKind::RepoDerived;
            case T3BasisKind::Pending:
                return r.kind != RefKind::SpecClause;
        }
        return false;
    }

    /// 11 行映射表(**行序 = `PlatformIntent` 枚举值**)—— 逐行依据见锚点摘要 §2。
    static constexpr std::array<T3MappingCell, kPlatformIntentCount> kT3MappingCells{{
        // ① request = A:Acquire(master→slave)—— PE 侧此刻**未变**(权限随 D 通道回来)⇒ 无事件
        {PlatformIntent::Request, "TLC-M-01", 0, T3BasisKind::Direct,
         "D1 §8.1.1 Acquire + §8.1.2 通道D(权限随 Grant 回来)"},
        // ② completion = D:Grant/GrantData(slave→master)—— PE 获得副本 ⇒ Grow
        {PlatformIntent::Completion, "TLC-M-02", kT3EventGrow, T3BasisKind::Direct,
         "D1 §8.1.4 Table 8.3(Grow) + §8.1.2 通道D"},
        // ③ snoop = B:Probe(slave→master)—— 强制移除副本/权限 ⇒ Prune
        {PlatformIntent::Snoop, "TLC-M-03", kT3EventPrune, T3BasisKind::Direct,
         "D1 §8.1.1 Probe + §8.1.4 Table 8.3(Prune)"},
        // ④ snoop_resp = C:ProbeAck/ProbeAckData(master→slave)—— "potentially releasing permissions" ⇒ Prune(可能)
        {PlatformIntent::SnoopResp, "TLC-M-04", kT3EventPrune, T3BasisKind::Direct,
         "D1 §8.1.2 通道C(potentially releasing permissions) + §8.1.4 Table 8.3"},
        // ⑤ writeback = C:Release/ReleaseData(master→slave)—— 自愿放弃副本/权限 ⇒ Prune
        {PlatformIntent::Writeback, "TLC-M-05", kT3EventPrune, T3BasisKind::Direct,
         "D1 §8.1.1 Release + §8.1.4 Table 8.3(Prune)"},
        // ⑥~⑩ 其余 5 条 canonical 流:TileLink-C **无独立流** ⇒ 无事件
        {PlatformIntent::ForwardData, "TLC-M-06", 0, T3BasisKind::Direct,
         "锚点摘要 §4(无独立流:数据恒附着于 GrantData/ProbeAckData/ReleaseData)"},
        {PlatformIntent::BackInvalidate, "TLC-M-07", 0, T3BasisKind::Direct,
         "锚点摘要 §4(无独立流:强制撤销由 B:Probe 承载)"},
        {PlatformIntent::AddressResolve, "TLC-M-08", 0, T3BasisKind::Direct,
         "锚点摘要 §4(路由静态)"},
        {PlatformIntent::Barrier, "TLC-M-09", 0, T3BasisKind::Direct,
         "锚点摘要 §4(规范无 barrier 消息)"},
        {PlatformIntent::SnoopCompletionAck, "TLC-M-10", 0, T3BasisKind::Direct,
         "锚点摘要 §4(ProbeAck 本身即 ack;GrantAck 配对 completion)"},
        // ⑪ atomic 意图:TileLink-C 的 10 条消息表**无** atomic 类消息 ⇒ 无事件
        {PlatformIntent::AtomicIntent, "TLC-M-11", 0, T3BasisKind::Direct,
         "D1 §8.1.3(10 条消息表无 atomic 类) + TileLinkCAdapter::kMessageClassMap(只用 4 类)"},
    }};

    /// 消费者可用的**事件掩码** —— ⚠️ **fail-closed**:`Pending` 行 ⇒ 返回 **0**
    /// (⛔ 消费者 SHALL NOT 把未锚的行当作「无事件」以外的任何解释;
    ///  ⛔ 更**不得**据此推断「该意图无副作用」—— 那是 `Direct` + 空掩码才有的含义)。
    static constexpr std::uint8_t t3_consumable_event_mask(PlatformIntent i) noexcept {
        const auto& c = kT3MappingCells[static_cast<std::size_t>(i)];
        if (c.basis == T3BasisKind::Pending)
            return 0;
        return c.event_mask;
    }

    /// 该意图行的 `basis`
    static constexpr T3BasisKind t3_basis_of(PlatformIntent i) noexcept {
        return kT3MappingCells[static_cast<std::size_t>(i)].basis;
    }

    /// 该意图行的 `row_ref`(锚点摘要行 id)
    static constexpr const char* t3_row_id_of(PlatformIntent i) noexcept {
        return kT3MappingCells[static_cast<std::size_t>(i)].row_ref;
    }

    /// 该意图行的条款级 `ref`
    static constexpr const char* t3_ref_of(PlatformIntent i) noexcept {
        return kT3MappingCells[static_cast<std::size_t>(i)].ref;
    }


    // ── T3 表的**带参**判定入口(判据铁律 ③:编译期锁 SHALL 同时有运行时可判入口)──
    /// 给定一张 T3 行表,判其是否自洽(行数固定 ⇒ 只查唯一性 / 覆盖性 / basis↔事件 / ref 非空 / 掩码范围)。
    static constexpr bool t3_rows_are_valid_for(
        const std::array<T3MappingCell, kPlatformIntentCount>& t) noexcept {
        for (std::size_t i = 0; i < t.size(); ++i) {
            if (static_cast<std::size_t>(t[i].intent) != i)
                return false;
            for (std::size_t j = i + 1; j < t.size(); ++j)
                if (t[i].intent == t[j].intent)
                    return false;
            if (t[i].row_ref == nullptr || t[i].row_ref[0] == '\0')
                return false;
            for (std::size_t j = i + 1; j < t.size(); ++j)
                if (std::string_view(t[i].row_ref) == std::string_view(t[j].row_ref))
                    return false;
            if (t[i].basis == T3BasisKind::Pending && t[i].event_mask != 0)
                return false;
            if (t[i].basis != T3BasisKind::Pending &&
                (t[i].ref == nullptr || t[i].ref[0] == '\0'))
                return false;
            if ((t[i].event_mask & ~kT3EventAll) != 0)
                return false;
        }
        return true;
    }

    /// 带参版的 **fail-closed** 访问器:`Pending` 行 ⇒ 掩码归零(⛔ 消费者 SHALL NOT 消费未锚行)。
    static constexpr std::uint8_t t3_consumable_event_mask_for(
        const std::array<T3MappingCell, kPlatformIntentCount>& t,
        PlatformIntent i) noexcept {
        const auto& c = t[static_cast<std::size_t>(i)];
        if (c.basis == T3BasisKind::Pending)
            return 0;
        return c.event_mask;
    }

    // ── T3 表的自洽判据(**运行时可判**;编译期锁见文件外)──────────────────
    /// 行数 == `kPlatformIntentCount`
    static constexpr bool t3_row_count_is_exact() noexcept {
        return kT3MappingCells.size() == kPlatformIntentCount;
    }
    /// `intent` 唯一且**覆盖** 0 ~ `kPlatformIntentCount-1`
    static constexpr bool t3_intents_are_unique_and_complete() noexcept {
        for (std::size_t i = 0; i < kT3MappingCells.size(); ++i) {
            if (static_cast<std::size_t>(kT3MappingCells[i].intent) != i)
                return false;
            for (std::size_t j = i + 1; j < kT3MappingCells.size(); ++j)
                if (kT3MappingCells[i].intent == kT3MappingCells[j].intent)
                    return false;
        }
        return true;
    }
    /// `row_ref` 唯一(每行一个锚点摘要行 id)
    static constexpr bool t3_row_refs_are_unique() noexcept {
        for (std::size_t i = 0; i < kT3MappingCells.size(); ++i) {
            if (kT3MappingCells[i].row_ref == nullptr || kT3MappingCells[i].row_ref[0] == '\0')
                return false;
            for (std::size_t j = i + 1; j < kT3MappingCells.size(); ++j)
                if (std::string_view(kT3MappingCells[i].row_ref) ==
                    std::string_view(kT3MappingCells[j].row_ref))
                    return false;
        }
        return true;
    }
    /// `Pending` 行 ⇒ **不得**声明事件(fail-closed 的前提)
    static constexpr bool t3_pending_rows_declare_no_event() noexcept {
        for (const auto& c : kT3MappingCells)
            if (c.basis == T3BasisKind::Pending && c.event_mask != 0)
                return false;
        return true;
    }
    /// 非 `Pending` 行 ⇒ `ref` 非空
    static constexpr bool t3_non_pending_rows_have_ref() noexcept {
        for (const auto& c : kT3MappingCells)
            if (c.basis != T3BasisKind::Pending &&
                (c.ref == nullptr || c.ref[0] == '\0'))
                return false;
        return true;
    }
    /// 事件掩码**不得**含 4 事件之外的位
    static constexpr bool t3_event_masks_are_in_range() noexcept {
        for (const auto& c : kT3MappingCells)
            if ((c.event_mask & ~kT3EventAll) != 0)
                return false;
        return true;
    }

    static constexpr const char* perm_name(Perm p) noexcept {
        switch (p) {
            case Perm::NONE: return "NONE";
            case Perm::READ: return "READ";
            case Perm::UNIQUE: return "UNIQUE";
            case Perm::WRITEBACK: return "WRITEBACK";
        }
        return "?";
    }
};

}  // namespace tlm::cep

// ── 编译期锁(置于类外 —— 类内 `constexpr` 成员函数在类**未完成**时不可作常量求值)──
// 判据铁律 ③:编译期锁 SHALL **同时**有运行时断言(见 `test/test_cep_tilelink_state_spec.cc`)。

static_assert(tlm::cep::TileLinkStateSpec::stable_states_are_exactly_nothing_branch_tip(),
              "稳定态集合 SHALL 恰为 3 个且逐字为 Nothing/Branch/Tip");
static_assert(tlm::cep::TileLinkStateSpec::events_are_exactly_cap_grow_prune_report(),
              "事件词表 SHALL 恰为 4 类且逐字为 Cap/Grow/Prune/Report");
static_assert(tlm::cep::TileLinkStateSpec::trunk_absent_from_stable_states(),
              "Trunk SHALL NOT 出现在稳定态集合中");
static_assert(tlm::cep::TileLinkStateSpec::asymmetry_is_tilelink_form(),
              "非对称判据 SHALL 为 TileLink 形态(Grant/GrantData 只在 D · Probe 只在 B)");
static_assert(tlm::cep::TileLinkStateSpec::transitions_are_valid(),
              "转移矩阵 SHALL 满足逐格校验规则");
static_assert(tlm::cep::TileLinkStateSpec::counts_are_consistent(),
              "派生计数 SHALL 与逐格统计一致(5 个量分别比对)");
static_assert(tlm::cep::TileLinkStateSpec::count_kind(tlm::cep::TileLinkStateSpec::CellKind::Known) == 12,
              "Known 格数 SHALL == 12");
static_assert(tlm::cep::TileLinkStateSpec::count_kind(tlm::cep::TileLinkStateSpec::CellKind::Unknown) == 0,
              "Unknown 格数 SHALL == 0");
static_assert(tlm::cep::TileLinkStateSpec::count_popcount(0) == 2, "空集格数 SHALL == 2");
static_assert(tlm::cep::TileLinkStateSpec::count_popcount(1) == 5, "单值格数 SHALL == 5");
static_assert(tlm::cep::TileLinkStateSpec::count_popcount(2) == 2, "2 位目标格数 SHALL == 2");
static_assert(tlm::cep::TileLinkStateSpec::count_popcount(3) == 3, "3 位目标格数 SHALL == 3");

// ── T3 映射表的编译期锁(per 台账 Entry #241)────────────────────────────
// ⚠️ 判据铁律 ③:编译期锁 SHALL **同时**有运行时断言
//    (见 `test/test_cep_tilelink_state_spec.cc` 的 T3 段)。
static_assert(tlm::cep::TileLinkStateSpec::t3_row_count_is_exact(),
              "T3: 映射行数 SHALL == kPlatformIntentCount(11)");
static_assert(tlm::cep::TileLinkStateSpec::t3_intents_are_unique_and_complete(),
              "T3: intent SHALL 唯一且覆盖 0 ~ kPlatformIntentCount-1");
static_assert(tlm::cep::TileLinkStateSpec::t3_row_refs_are_unique(),
              "T3: row_ref SHALL 唯一且非空");
static_assert(tlm::cep::TileLinkStateSpec::t3_pending_rows_declare_no_event(),
              "T3: Pending 行 SHALL NOT 声明事件(fail-closed)");
static_assert(tlm::cep::TileLinkStateSpec::t3_non_pending_rows_have_ref(),
              "T3: 非 Pending 行 SHALL 有非空 ref");
static_assert(tlm::cep::TileLinkStateSpec::t3_event_masks_are_in_range(),
              "T3: 事件掩码 SHALL 只含 4 个事件位");

#endif  // CPPTLM_CEP_TILELINK_STATE_SPEC_HH
