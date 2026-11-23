// include/tlm/cep/engine_capabilities.hh
// 协议引擎**承载能力声明面**（通用层 · 协议中立 · 零协议名）
//
// 功能：把「某协议实例的协议语义由谁承载、承载到哪一层」从**引擎头文件里的散文**
//       变成**编译期 + 闸双重保证的声明面**（per change
//       `cep-protocol-engine-capability-declaration`）。
// 作者：CppTLM Team / 日期：2026-11-23
//
// ── 定位（⛔ 本单元只做「让缺口可检出」，不做「补齐缺口」）────────────
//   · 本头**只登记现状**：引擎存在性 / egress 形态 / 在飞事务建模与否。
//   · ⛔ 不改任何协议引擎的**行为**；⛔ 不补瞬态态；⛔ 不补在飞 snoop 集合。
//
// ── 协议中立（per `include/tlm/cep/AGENTS.md` 铁律 2）──────────────────
//   本头**不得**出现任何具体引擎类名 / adapter 类名，且**不得** `#include`
//   `*_protocol_engine.hh` / `*_adapter.hh` ⇒ 协议差异只经由各协议头对
//   `EngineTypeTraits<T>` 的**特化**注入（单向依赖：协议头 → 本头）。
//
// ── 判据骨架（判据本体在此，反向 fixture 在闸与测试）────────────────────
//   J-1  在飞建模一致（**编译期**）：`EngineTypeTraits<T>::in_flight` ⇔ `HasInFlightApi<T>`
//   J-2  实例集双向零 orphan（运行期 + 闸）：`kEngineCapabilities` 的 `id` 集
//        == `protocol_instances.json` 的 `id` 集
//   J-2b 声明自洽（运行期）：每行 `basis` 非空 ∧ `id` 唯一
//   J-3  瞬态态策略（**编译期**）：持 `kTransientStateCount` 的 adapter 满足
//        `kTransientStateCount <= kMaxTransientStatesPerInstance`
//
// ── 诚实边界（⛔ 必须随本头一起读）────────────────────────────────────
//   · `egress` **无机器判据**：5 个引擎的发射面没有统一的公开可观察 API
//     （`chi_rnf` 是 `emit(b, ch)`、`mesif` 是 `emit_snoop_resp(...)`，其余三个
//     **零发射**）⇒ 强行 grep 会命中注释 ⇒ 假绿。
//     ⇒ 以文件级常量 `kEgressHasMachineCheck = false` **显式登记**「此处无牙」。
//   · J-3 的阈值 1 是**现状锁**，⛔ 不是设计容量；提升阈值属 architect 裁决事项。
//   · 3 个**无引擎**实例（`cxl_2_0` / `cxl_3_0_symmetric` / `nvlink_4`）是本声明面
//     的**编译期飞地**（它们没有 `EngineTypeTraits` 特化）⇒ 只由 J-2 与闸的
//     「引擎头文件集反向存在性」覆盖，⛔ 不假装有编译期绑定。
#ifndef CPPTLM_TLM_CEP_ENGINE_CAPABILITIES_HH
#define CPPTLM_TLM_CEP_ENGINE_CAPABILITIES_HH

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace tlm::cep {

    // ─────────────────────────────────────────────────────────────────────
    //  枚举（协议中立）
    // ─────────────────────────────────────────────────────────────────────

    /// 引擎是否向网络发射**协议消息**。
    enum class EgressMode : std::uint8_t {
        ProtocolMessages = 0, ///< 有 egress（发协议消息）
        None = 1,             ///< 零发射（只记账 / 无引擎）
    };

    /// 引擎是否建模**在飞事务**（per-block / per-txn 的在途事务集合）。
    enum class InFlightModeling : std::uint8_t {
        Modeled = 0,    ///< 暴露在飞事务 API（当前仅 `chi_rnf`）
        NotModeled = 1, ///< 不建模（含「显式推迟到 Stage 2」与「零发射」两种情形）
    };

    inline const char* egress_mode_name(EgressMode m) noexcept {
        switch (m) {
            case EgressMode::ProtocolMessages: return "protocol_messages";
            case EgressMode::None:             return "none";
        }
        return "unknown";
    }

    inline const char* in_flight_modeling_name(InFlightModeling m) noexcept {
        switch (m) {
            case InFlightModeling::Modeled:    return "modeled";
            case InFlightModeling::NotModeled: return "not_modeled";
        }
        return "unknown";
    }

    // ─────────────────────────────────────────────────────────────────────
    //  实例级声明（本仓权威 · 单一真源）
    // ─────────────────────────────────────────────────────────────────────
    //
    // ⚠️ `id` **逐字**等于 `include/tlm/cep/protocol_instances.json` 的 `id` 键值
    //    （⚠️ 是 `id`，⛔ 不是 `instance_id`；TileLink 实例 id 是 `tilelink_c`）。
    // ⚠️ `basis` **非空**（缺失 ⇒ 判据必红）。
    // ⛔ 本结构**不含**「瞬态态计数」与「在飞建模」字段 —— 那两项的唯一真源分别是
    //    各 adapter 头的 `kTransientStateCount` 与各协议头对 `EngineTypeTraits<T>` 的
    //    特化（避免第二真源，per change AC-9）。

    // == AC-9 scan begin: EngineCapability fields ==
    struct EngineCapability {
        const char* id;    ///< 实例 id（逐字等于 protocol_instances.json 的 id）
        EgressMode egress; ///< 发射形态
        const char* basis; ///< 判定依据（非空）
    };
    // == AC-9 scan end: EngineCapability fields ==


    // == AC-9 scan begin: kEngineCapabilities init ==
    inline constexpr std::array<EngineCapability, 8> kEngineCapabilities{{
        {"chi_rnf", EgressMode::ProtocolMessages,
         "emit(b, ChiChannel) / emit_comp_ack / emit_snoop_resp 三处发射点"},
        {"mesif", EgressMode::ProtocolMessages,
         "emit_snoop_resp(...) + 请求装填(request_emitted 递增)"},
        {"moesi_gpu", EgressMode::None,
         "决策 M-6: canonical 10 流 on_* 只记账；emit 零命中"},
        {"tilelink_c", EgressMode::None,
         "事件计数/上报器(note_atomic_intent / consume)，不建 PE 本地树状态机；emit 零命中"},
        {"ualink_4", EgressMode::None,
         "自陈零发射、零状态(计数器自增)"},
        {"cxl_2_0", EgressMode::None,
         "无对应独立引擎头"},
        {"cxl_3_0_symmetric", EgressMode::None,
         "无对应独立引擎头"},
        {"nvlink_4", EgressMode::None,
         "无对应独立引擎头"},
    }};
    // == AC-9 scan end: kEngineCapabilities init ==

    /// 声明行数（与 `protocol_instances.json` 的实例数相等 —— 由 J-2 判据守）
    inline constexpr std::size_t kEngineCapabilityCount = kEngineCapabilities.size();

    /// ⛔ **诚实登记**：`egress` 字段当前**无机器判据**。
    /// 若将来引入 egress 机检 ⇒ 必须翻为 `true` 并同时补该判据
    /// （否则 J-04 判据红因码 `engine-cap-egress-claim-without-check`）。
    inline constexpr bool kEgressHasMachineCheck = false;

    /// J-3 阈值 = **现状锁**（本轮 8 实例合计瞬态态 = 1）。
    /// ⛔ 提升该阈值属 **architect 裁决**事项，不是实现自由度。
    inline constexpr std::size_t kMaxTransientStatesPerInstance = 1;

    // ─────────────────────────────────────────────────────────────────────
    //  自洽谓词（**纯函数** · 可传变异副本做负控）
    // ─────────────────────────────────────────────────────────────────────
    //
    // ⚠️ 本谓词**只管**：① 每行 `id` / `basis` 非空 ② `id` 唯一。
    // ⛔ 它**不管**「行数正确性」—— 行数正确性由 J-2（双向零 orphan）承担：
    //    谓词没有「行数期望源」（且⛔ 不得读取全局状态 ⇒ 无法知道该有几行）。
    inline bool engine_capabilities_are_consistent(const EngineCapability* list,
                                                   std::size_t n) noexcept {
        for (std::size_t i = 0; i < n; ++i) {
            if (list[i].id == nullptr || list[i].id[0] == '\0')
                return false;
            if (list[i].basis == nullptr || list[i].basis[0] == '\0')
                return false;
            for (std::size_t j = 0; j < i; ++j) {
                const char* a = list[j].id;
                const char* b = list[i].id;
                std::size_t k = 0;
                while (a[k] != '\0' && b[k] != '\0' && a[k] == b[k])
                    ++k;
                if (a[k] == b[k]) // 两者同址终止 ⇒ 完全相等 ⇒ 重复
                    return false;
            }
        }
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────
    //  类型级能力声明（协议层特化入口）
    // ─────────────────────────────────────────────────────────────────────
    //
    // ⚠️ **主模板故意用 dependent-false 报错**：既保留「未特化即编译错」，
    //    又给出可读归因（否则撞到 `incomplete type used in ...`，不含任何 CEP 语境）。
    template <class T>
    struct EngineTypeTraits {
        static_assert(!sizeof(T*),
                      "EngineTypeTraits: <T> 无能力特化；"
                      "若无引擎实例请走 kEngineCapabilities 行级声明");
    };

    // ─────────────────────────────────────────────────────────────────────
    //  SFINAE 反射：探测类型是否暴露公开 API `txn_in_flight`
    // ─────────────────────────────────────────────────────────────────────
    //
    // ⚠️ 反射优于 grep：grep 会命中**注释**与**历史行** ⇒ 假绿；
    //    本探测盯的是**类型真实暴露的公开 API** ⇒ 引擎删掉该方法 ⇒ 编译直接失败。
    // ⚠️ 探测名 `txn_in_flight` 若改名 ⇒ 判据必红（这是**特性**不是缺陷：
    //    「补了必红 / 删了也必红」正是本声明面的目的）。
    template <class T, class = void>
    struct HasInFlightApi : std::false_type {};

    template <class T>
    struct HasInFlightApi<
        T, std::void_t<decltype(std::declval<const T&>().txn_in_flight(std::uint64_t{}))>>
        : std::true_type {};

    // ══════════════════════════════════════════════════════════════════════
    //  J-1 的逐实例编译期判据（供测试与各协议头 include 后立即生效）
    // ══════════════════════════════════════════════════════════════════════
    //
    // 用法（在**测试 TU** 或协议头内）：
    //     // 在**协议头或测试 TU**（可含协议名的那一侧）对每个有引擎的实例各写一条：
    //     static_assert(in_flight_declaration_matches<SomeEngineT>(),
    //                   "<instance-id>: 声明的 in_flight 与 HasInFlightApi 不一致");
    //
    // ⚠️ 该函数**只**比较「声明 vs 真实 API」；⛔ 不做任何字符串比较
    //    （实例 id 的字面量由调用方写进 static_assert 的消息串里）。
    template <class T>
    constexpr bool in_flight_declaration_matches() noexcept {
        return (EngineTypeTraits<T>::in_flight == InFlightModeling::Modeled) ==
               HasInFlightApi<T>::value;
    }

} // namespace tlm::cep

#endif // CPPTLM_TLM_CEP_ENGINE_CAPABILITIES_HH
