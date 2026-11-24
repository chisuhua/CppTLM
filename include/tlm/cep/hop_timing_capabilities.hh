// include/tlm/cep/hop_timing_capabilities.hh
// 协议语义层的「hop / 时序模型」承载能力声明面（通用层 · 协议中立 · 零协议名）
//
// 功能：把「某个协议实例的**协议语义**有没有 hop 计数 / 消息级时序模型」从
//       **引擎头文件里的散文**变成**编译期 + 闸双重保证的声明面**（per change
//       `cep-hop-timing-capability-declaration` · gap 分析 G10）。
// 作者：CppTLM Team / 日期：2026-11-24
//
// ── 定位（⛔ 本单元只做「让缺口可检出」，不做「补齐缺口」）────────────────
//   · 本头**只登记现状**：协议语义层（8 个实例的引擎）的 hop / 时序建模**有无**。
//   · ⛔ 不改任何协议引擎的**行为**；⛔ 不补 hop 计数；⛔ 不补消息级时序模型
//     （那属协议 FSM 语义设计，per 宪章归 ArchForge）。
//
// ── 协议的哪一层（⛔ 本头最容易被误读的地方 —— 必须逐字守住）──────────────
//   · **协议语义层**（本头登记的对象）：8 个实例的引擎 ⇒ 现状**零** hop / 时序 API。
//   · **步进钩子**：引擎有 `tick()`（驱动入站分派 + egress 发送）⇒ ⛔ 那是
//     **全局步进**，⛔ **不是**消息级时序模型（消息何时到达、耗时多久 ⇒ 无建模）。
//   · **载体 / 器件层**：**已有** hop 与时延概念（flit 载体的 hop 计数字段 ·
//     responder 与桩缓存核的固定延迟参数）⇒ ⛔ 「无 hop / 时序」**仅对协议语义层
//     成立**，⛔ **不得**表述为「全平台无 hop / 时序」。
//
// ── 协议中立（per `include/tlm/cep/AGENTS.md` 铁律 2）──────────────────
//   本头**不得**出现任何具体引擎类名 / adapter 类名，**不得** `#include`
//   `*_protocol_engine.hh` / `*_adapter.hh`；载体 / 器件层的既有事实按**角色**
//   描述（⛔ 不按协议名描述）⇒ 协议差异只经由各协议头对 traits 的**特化**注入。
//
// ── 判据骨架（判据本体在此，反向 fixture 在闸与测试）────────────────────
//   J-1  实例集双向零 orphan（运行期 + 闸）：`kHopTimingCapabilities` 的 `id` 集
//        == `protocol_instances.json` 的 `id` 集
//   J-2  声明自洽（运行期）：每行 `basis` 非空 ∧ `id` 唯一
//   J-3  协议语义层 hop / 时序**现状锁**：任一行 ≠ `None` ⇒ 必红
//        （⇒ 有人实现了协议语义层 hop / 时序 ⇒ **必须**走决策 + 更新声明）
//   J-4  引擎层 hop / 时序 API **反向存在性**（闸）：`*_protocol_engine.hh`
//        去注释后出现 hop / 时序 API ⇒ 必红 ⇒ 强制更新声明 + 走决策
//   J-5  编译期**否定断言**（测试）：5 个实例引擎类型上
//        `has_hop_api_v<E> == false` ∧ `has_timing_api_v<E> == false`
//   J-6  载体 / 器件层 hop / 时序**存在性**（闸）：既有事实被删 ⇒ 必红
//        （⇒ 防本声明失真）
//   J-7  与 G1 正交：声明表区段内**不得**出现 `egress` / `in_flight`（不重复真源）
//   J-8  诚实边界**机器可读**（闸 + 头文件常量）：见下方三个 `k*` 常量
//
// ── 诚实边界（⛔ 必须随本头一起读）────────────────────────────────────
//   · 本头**不含**「载体 / 器件层」的逐条事实表 ⇒ 那些事实的真源是**载体 / 器件
//     头文件本身**；本头只登记「它们已有 hop / 时延」这**一件事实**（J-6 守其不失真）。
//   · `has_hop_api_v` / `has_timing_api_v` 是**探针名白名单** ⇒ 只对**白名单里的
//     名字**有牙；白名单之外的命名由 J-4 的**闸级去注释扫描**兜底（更宽）。
//   · 3 个实例**无独立引擎头**（与 G1 同型）⇒ 只由 J-1 / J-3 覆盖，⛔ 不假装有
//     编译期绑定（**编译期飞地**）。
#ifndef CPPTLM_TLM_CEP_HOP_TIMING_CAPABILITIES_HH
#define CPPTLM_TLM_CEP_HOP_TIMING_CAPABILITIES_HH

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

namespace tlm::cep {

    // ─────────────────────────────────────────────────────────────────────
    //  枚举（协议中立）
    // ─────────────────────────────────────────────────────────────────────

    /// hop（消息穿越网络的**跳数**）建模形态。
    enum class HopModel : std::uint8_t {
        /// 无 hop 建模（协议语义层的**现状** —— 8 个实例全为该值）。
        None = 0,
        /// **仅有** hop **计数**（无模型）—— 载体层形态（计数由转发方递增）。
        /// ⚠️ **协议语义层现状恒为 `None`**（J-3 现状锁）⇒ 该值当前**仅**供
        ///   「载体层计数」语义与反向 fixture 使用。
        CounterOnly = 1,
    };

    /// 时序（消息何时到达 / 耗时多久）建模形态。
    enum class TimingModel : std::uint8_t {
        /// 无时序建模（协议语义层的**现状** —— 8 个实例全为该值）。
        None = 0,
        /// **仅有**固定**延迟参数**（无消息级时序模型）—— 器件层形态。
        /// ⚠️ **协议语义层现状恒为 `None`**（J-3 现状锁）⇒ 该值当前**仅**供
        ///   「器件层固定延迟」语义与反向 fixture 使用。
        ParameterizedDelay = 1,
    };

    inline const char* hop_model_name(HopModel m) noexcept {
        switch (m) {
            case HopModel::None: return "none";
            case HopModel::CounterOnly: return "counter_only";
        }
        return "unknown";
    }

    inline const char* timing_model_name(TimingModel m) noexcept {
        switch (m) {
            case TimingModel::None: return "none";
            case TimingModel::ParameterizedDelay: return "parameterized_delay";
        }
        return "unknown";
    }

    // ─────────────────────────────────────────────────────────────────────
    //  实例级声明（本仓权威 · 单一真源）
    // ─────────────────────────────────────────────────────────────────────
    //
    // ⚠️ `id` **逐字**等于 `include/tlm/cep/protocol_instances.json` 的 `id` 键值。
    // ⚠️ `basis` **非空**（缺失 ⇒ 判据必红）。
    // ⛔ 本结构**不含** G1 的 `egress` / `in_flight` 字段（**正交**，避免第二真源）。

    // == AC-7 scan begin: HopTimingCapability fields ==
    struct HopTimingCapability {
        const char* id;             ///< 实例 id（逐字等于 protocol_instances.json 的 id）
        HopModel protocol_hop;      ///< 协议语义层的 hop 建模（**现状恒为 None**）
        TimingModel protocol_timing;///< 协议语义层的时序建模（**现状恒为 None**）
        const char* basis;          ///< 判定依据（非空）
    };
    // == AC-7 scan end: HopTimingCapability fields ==

    // == AC-7 scan begin: kHopTimingCapabilities init ==
    inline constexpr std::array<HopTimingCapability, 8> kHopTimingCapabilities{{
        {"chi_rnf", HopModel::None, TimingModel::None,
         "chi_rnf_protocol_engine.hh 公开面 = tick() 步进钩子 + emit* 三处发射；零 hop / 时序 API"},
        {"mesif", HopModel::None, TimingModel::None,
         "mesif_protocol_engine.hh 公开面 = tick() 步进钩子 + emit_snoop_resp；零 hop / 时序 API"},
        {"moesi_gpu", HopModel::None, TimingModel::None,
         "moesi_gpu_protocol_engine.hh:canonical 10 流 on_* 只记账；零 hop / 时序 API"},
        {"tilelink_c", HopModel::None, TimingModel::None,
         "tilelink_protocol_engine.hh:事件计数 / 上报器；零 hop / 时序 API"},
        {"ualink_4", HopModel::None, TimingModel::None,
         "ualink_protocol_engine.hh:零发射、零状态；零 hop / 时序 API"},
        {"cxl_2_0", HopModel::None, TimingModel::None,
         "无独立引擎头 ⇒ 协议语义层零 hop / 时序（编译期飞地）"},
        {"cxl_3_0_symmetric", HopModel::None, TimingModel::None,
         "无独立引擎头 ⇒ 协议语义层零 hop / 时序（编译期飞地）"},
        {"nvlink_4", HopModel::None, TimingModel::None,
         "无独立引擎头 ⇒ 协议语义层零 hop / 时序（编译期飞地）"},
    }};
    // == AC-7 scan end: kHopTimingCapabilities init ==

    /// 声明行数（与 `protocol_instances.json` 的实例数相等 —— 由 J-1 判据守）。
    inline constexpr std::size_t kHopTimingCapabilityCount = kHopTimingCapabilities.size();

    /// 声明自洽谓词（运行期；**J-2** + **J-3**）。
    ///
    /// · `basis` 非空 ∧ `id` 非空 ∧ `id` 唯一（J-2）
    /// · 协议语义层的 `protocol_hop` / `protocol_timing` **恒为 `None`**（J-3 现状锁）
    ///   ⇒ 有人实现了协议语义层 hop / 时序 ⇒ 本谓词为假 ⇒ **必须**走决策 + 更新声明
    inline bool hop_timing_capabilities_are_consistent(const HopTimingCapability* rows,
                                                        std::size_t count) {
        if (rows == nullptr || count == 0) return false;
        std::set<std::string> ids;
        for (std::size_t i = 0; i < count; ++i) {
            const HopTimingCapability& r = rows[i];
            if (r.id == nullptr || r.id[0] == '\0') return false;
            if (r.basis == nullptr || r.basis[0] == '\0') return false;
            if (!ids.insert(std::string(r.id)).second) return false;   // id 唯一
            if (r.protocol_hop != HopModel::None) return false;         // J-3 现状锁
            if (r.protocol_timing != TimingModel::None) return false;   // J-3 现状锁
        }
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────
    //  诚实边界（机器可读 —— J-8；⛔ 措辞变更须同步闸的判据）
    // ─────────────────────────────────────────────────────────────────────

    /// 引擎的 `tick()` 是**全局步进钩子**，⛔ **不是**消息级时序模型。
    /// ⇒ 闸判据：头文件须含本常量（AC-9 的机器面）。
    inline constexpr bool kTickIsSteppingHookNotTimingModel = true;

    /// 「无 hop / 时序」**仅对协议语义层**成立 —— 载体 / 器件层**已有** hop 与时延
    /// 概念（flit 载体的 hop 计数字段 · responder 与桩缓存核的固定延迟参数）。
    /// ⇒ 闸判据：头文件须含本常量（AC-10 的机器面）。
    inline constexpr bool kHopTimingAbsenceIsProtocolSemanticsOnly = true;

    /// 协议语义层现状**恒无** hop / 时序模型（J-3 现状锁的机器面）。
    /// ⇒ 闸判据：声明表内任一行 ≠ `None` ⇒ 必红。
    inline constexpr bool kProtocolSemanticsHasNoHopOrTimingModel = true;

    // ─────────────────────────────────────────────────────────────────────
    //  编译期探针（协议中立 · 模板形态；⛔ 不含任何协议类型）
    // ─────────────────────────────────────────────────────────────────────
    //
    // ⚠️ **探针名白名单** ⇒ 只对下列名字有牙；白名单之外的命名由 J-4 的**闸级
    //    去注释扫描**兜底（更宽）。改动本白名单 ⇒ 须复核 J-4 的模式词表。
    //
    // ⛔ **零实例特化是刻意的**：现状**没有任何**实例有 hop / 时序能力 ⇒ 特化集
    //    为**空** ⇒ ⛔ 不写「假的空特化」（那会让声明面假装有编译期绑定）⇒ 探针以
    //    **否定断言**形态出现在测试里（`static_assert(!has_hop_api_v<E>)`）。

    namespace detail {

        // SFINAE 探针（每个候选名一个 trait；⛔ 探测点在**成员表达式**上 ⇒
        // 真正的替换失败 ⇒ 主模板（false_type）生效）
        template <class T, class = void>
        struct HasLastHops : std::false_type {};
        template <class T>
        struct HasLastHops<T, std::void_t<decltype(std::declval<T&>().last_hops())>>
            : std::true_type {};

        template <class T, class = void>
        struct HasHopCount : std::false_type {};
        template <class T>
        struct HasHopCount<T, std::void_t<decltype(std::declval<T&>().hop_count())>>
            : std::true_type {};

        template <class T, class = void>
        struct HasHops : std::false_type {};
        template <class T>
        struct HasHops<T, std::void_t<decltype(std::declval<T&>().hops())>> : std::true_type {};

        template <class T, class = void>
        struct HasLastLatencyCycles : std::false_type {};
        template <class T>
        struct HasLastLatencyCycles<
            T, std::void_t<decltype(std::declval<T&>().last_latency_cycles())>> : std::true_type {};

        template <class T, class = void>
        struct HasLatencyCycles : std::false_type {};
        template <class T>
        struct HasLatencyCycles<T, std::void_t<decltype(std::declval<T&>().latency_cycles())>>
            : std::true_type {};

        template <class T, class = void>
        struct HasTravelTime : std::false_type {};
        template <class T>
        struct HasTravelTime<T, std::void_t<decltype(std::declval<T&>().travel_time())>>
            : std::true_type {};

        template <class T, class = void>
        struct HasArrivalCycle : std::false_type {};
        template <class T>
        struct HasArrivalCycle<T, std::void_t<decltype(std::declval<T&>().arrival_cycle())>>
            : std::true_type {};

    }  // namespace detail

    /// 该类型是否暴露 **hop 计数** API（探针名白名单，见上）。
    template <class T>
    inline constexpr bool has_hop_api_v = detail::HasLastHops<T>::value ||
                                          detail::HasHopCount<T>::value ||
                                          detail::HasHops<T>::value;

    /// 该类型是否暴露 **消息级时序** API（探针名白名单，见上）。
    template <class T>
    inline constexpr bool has_timing_api_v = detail::HasLastLatencyCycles<T>::value ||
                                             detail::HasLatencyCycles<T>::value ||
                                             detail::HasTravelTime<T>::value ||
                                             detail::HasArrivalCycle<T>::value;

}  // namespace tlm::cep

#endif  // CPPTLM_TLM_CEP_HOP_TIMING_CAPABILITIES_HH
