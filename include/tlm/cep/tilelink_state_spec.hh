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
        Unknown = 1  ///< **规范未覆盖** ⇒ 消费者 SHALL 走降级路径,SHALL NOT 当作「非法」
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
        std::uint8_t target_mask;  ///< 仅 `kind == Known` 时有意义;`kind == Unknown` ⇒ MUST 为 0
        BasisKind basis;
        bool pending;
        const char* ref;   ///< 锚点摘要 §2.3bis 的**逐格条目号**(`TLC-01` ~ `TLC-12`)
        const char* note;  ///< 条款级定位 + 断言摘要(**不整段复制**规范原文)
    };

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
        std::size_t n_unknown = 0;
        for (const auto& row : m) {
            for (const auto& cell : row) {
                switch (cell.kind) {
                    case CellKind::Known: ++n_known; break;
                    case CellKind::Unknown: ++n_unknown; break;
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
        return n_known + n_unknown == NE * NS;
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

#endif  // CPPTLM_CEP_TILELINK_STATE_SPEC_HH
