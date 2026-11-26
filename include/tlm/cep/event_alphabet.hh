// include/tlm/cep/event_alphabet.hh
// 事件字母表契约 —— 14 个**核心语义锚**（协议中立 · 通用层 · 零协议名）
//
// ── 定位（per `ADR-CEP-24` 决策 3.1 + `.rddf/reviews/cep-CP-Q-1a-adjudication.md` §4.1）──
// 用户自定义 SSP 的**事件**必须引用本表的**核心锚**（或用**受控私有事件**：① 映射 flow
// ② 指明等价锚或须 architect 批准 ③ 逐字断言 ④ 原子性 ⑤「无等价」占比 > 半数 ⇒ 触发复审）。
// 本表 = **封闭集**：新增锚 = 新增协议语义 ⇒ 须 **architect** 裁决（`ADR-CEP-20:88` 单向门）。
//
// ── 铁律（per `include/tlm/cep/AGENTS.md` 铁律 2）────────────────────────────
// ⛔ 本头**不得**出现任何协议名 / 协议类名；⛔ **不得** `#include` `*_protocol_engine.hh`
//    / `*_adapter.hh` / `*_state_spec.hh`。⇒ 本头**不** include 引擎基类（避免重依赖）：
//    `flow` 用**字符串**声明，其与 `CanonicalFlow` 的一致**由测试交叉核对**
//    （`kCanonicalFlowNames` in `protocol_engine_base.hh`，per `ADR-CEP-12 §4` 抓取点 4 同型）。
//
// ── ⚠️ flow 映射是**部分**的（诚实边界，per `CP-Q-1a` 裁决 §5 的体裁观察）────────
// **权限转移类**的 4 个事件（`Cap` / `Grow` / `Prune` / `Report`）是**本地权限转移**语义，
// ⛔ **不对应任何 canonical 流**（它们不是消息事件）⇒ 其 `flow = nullptr`。
// ⇒ AC「事件 flow ∈ 10 流」的正确表述 = 「**非空** flow ∈ 10 流」。
//
// 作者：CppTLM Team / 日期：2026-11-26
#ifndef CPPTLM_TLM_CEP_EVENT_ALPHABET_HH
#define CPPTLM_TLM_CEP_EVENT_ALPHABET_HH

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tlm::cep::ssp {

    // ─────────────────────────────────────────────────────────────────────
    //  核心语义锚（封闭 · 14 个 · per `CP-Q-1a` 裁决 §4.1 的初版清单）
    // ─────────────────────────────────────────────────────────────────────
    enum class CoreAnchor : std::uint8_t {
        // 请求类（local 发起）
        ReadShared = 0,   ///< 取共享副本（→ S）
        ReadUnique,       ///< 取独占数据（失效他人；→ E 或 M）
        Write,            ///< 写（要求独占）
        WriteNoAlloc,     ///< 写但不分配（write-no-allocate）
        Upgrade,          ///< 从共享升级为独占（S → E）
        // 侦听类（remote 发起）
        SnoopShared,      ///< 请求共享副本（保留他人权限）
        SnoopUnique,      ///< 请求独占（失效全部）
        SnoopInvalidate,  ///< 失效（→ I，脏则回数据）
        // 驱逐类
        EvictDirty,       ///< 脏驱逐（携数据回写）
        EvictClean,       ///< 净驱逐（无数据）
        // 权限转移类（本地权限转移风格 · **无 canonical 流**）
        Cap,              ///< 权限变为**目标值**（源不定 · 由格载荷承载）
        Grow,             ///< 权限升级（记录原 / 新）
        Prune,            ///< 权限降级（记录原 / 新）
        Report,           ///< 空操作（权限不变，只报告）
        kCount
    };

    /// 单锚的静态信息。`flow == nullptr` ⇒ 该锚**不对应任何 canonical 流**（本地权限转移）。
    struct AnchorInfo {
        CoreAnchor  anchor;
        const char* name;  ///< 逐字名（⛔ 改名即触发判据复核 · 先例 `kEventNames`）
        const char* flow;  ///< canonical 流名（逐字同 `kCanonicalFlowNames`）或 `nullptr`
        bool        remote;///< 是否 remote（侦听侧）发起
    };

    inline constexpr std::array<AnchorInfo, 14> kCoreAnchors{{
        // ── 请求类（flow = request）──────────────────────────────────────
        {CoreAnchor::ReadShared,     "ReadShared",     "request", false},
        {CoreAnchor::ReadUnique,     "ReadUnique",     "request", false},
        {CoreAnchor::Write,          "Write",          "request", false},
        {CoreAnchor::WriteNoAlloc,   "WriteNoAlloc",   "request", false},
        {CoreAnchor::Upgrade,        "Upgrade",        "request", false},
        // ── 侦听类（flow = snoop）────────────────────────────────────────
        {CoreAnchor::SnoopShared,    "SnoopShared",    "snoop",   true},
        {CoreAnchor::SnoopUnique,    "SnoopUnique",    "snoop",   true},
        {CoreAnchor::SnoopInvalidate,"SnoopInvalidate","snoop",   true},
        // ── 驱逐类（flow = writeback）────────────────────────────────────
        {CoreAnchor::EvictDirty,     "EvictDirty",     "writeback", false},
        {CoreAnchor::EvictClean,     "EvictClean",     "writeback", false},
        // ── 权限转移类（**无** canonical 流 ⇒ flow = nullptr）─────────────
        {CoreAnchor::Cap,            "Cap",            nullptr,   false},
        {CoreAnchor::Grow,           "Grow",           nullptr,   false},
        {CoreAnchor::Prune,          "Prune",          nullptr,   false},
        {CoreAnchor::Report,         "Report",         nullptr,   false},
    }};

    /// 锚集规模（**封闭**：新增须改本常量 ⇒ 触发判据复核 · AC-7）
    inline constexpr std::size_t kCoreAnchorCount = 14;
    static_assert(kCoreAnchors.size() == kCoreAnchorCount,
                  "kCoreAnchors 条数须 == kCoreAnchorCount（14）");

    /// 内部一致性：`kCount` == 14
    static_assert(static_cast<std::size_t>(CoreAnchor::kCount) == kCoreAnchorCount,
                  "CoreAnchor::kCount 须 == kCoreAnchorCount");

    // ─────────────────────────────────────────────────────────────────────
    //  查询（编译期可用 · 零分配）
    // ─────────────────────────────────────────────────────────────────────
    inline constexpr bool is_core_anchor_name(std::string_view n) noexcept {
        for (const auto& a : kCoreAnchors)
            if (n == a.name) return true;
        return false;
    }

    /// 按名解析锚；未命中 ⇒ `nullptr`（**fail-closed**：调用方须判空）
    inline constexpr const AnchorInfo* find_core_anchor(std::string_view n) noexcept {
        for (const auto& a : kCoreAnchors)
            if (n == a.name) return &a;
        return nullptr;
    }

    /// 锚 → flow 名；无 flow 的锚（权限转移类）⇒ `nullptr`
    inline constexpr const char* flow_of(CoreAnchor a) noexcept {
        for (const auto& x : kCoreAnchors)
            if (x.anchor == a) return x.flow;
        return nullptr;
    }

    // ─────────────────────────────────────────────────────────────────────
    //  诚实边界（⛔ 必须随本头一起读）
    // ─────────────────────────────────────────────────────────────────────
    /// B1: `flow` 映射是**部分**的 —— `Cap` / `Grow` / `Prune` / `Report` **无** canonical 流
    ///     （本地权限转移语义）⇒ AC 的正确表述 = 「**非空** flow ∈ 10 流」。
    inline constexpr bool kFlowMappingIsPartial = true;
    /// B2: 本表是**封闭集**；新增锚 = 新增协议语义 ⇒ 须 architect 裁决（`ADR-CEP-20:88`）。
    inline constexpr bool kCoreAnchorSetIsClosed = true;
    /// B3: 本头**不**定义私有事件机制 —— 私有事件（无等价锚者须 architect 批准）的
    ///     校验在 loader / 闸侧；⛔ 本头不承载「已批准私有事件先例库」。
    inline constexpr bool kPrivateEventsNotModelledHere = true;

} // namespace tlm::cep::ssp

#endif // CPPTLM_TLM_CEP_EVENT_ALPHABET_HH
