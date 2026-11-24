// include/tlm/cep/fence_effects.hh
// 「fence 一致性效果」的**派生声明面**（通用层 · 协议中立 · 零协议类名）
//
// 功能：把「某个协议实例的**协议语义**上，fence（barrier）带来什么一致性效果」从
//       **散文**变成**可机检的现状面**：由 **一致性标签（read/write）+ barrier 支持**
//       **推导**出每个实例的 fence 效果，登记为**声明面**（per change
//       `cep-fence-consistency-effect-derivation` · gap 分析 G4）。
// 作者：CppTLM Team / 日期：2026-11-24
//
// ── 定位（⛔ 本单元只做「让缺口可检出」，不做「补齐缺口」）────────────────
//   · 本头**只登记现状**：由既有声明（标签 + barrier）**推导**出的 fence 效果。
//   · ⛔ 不改任何协议引擎的**行为**；⛔ 不实现 fence 的**协议语义**
//     （那属协议 FSM 语义设计，per 宪章归 ArchForge）。
//
// ── 推导规则表（本单元的**决策面** · 随新建 main spec 的五步链 ratify）─────
//   R1  has_barrier == false          ⇒ None      （无 fence 通道 · **最高优先级**）
//   R2  read / write 标签**族不同**    ⇒ Invalid   （域外 · 必红）
//   R3  族 = SC                       ⇒ None      （SC 已全序 · **与 barrier 存在与否无关**）
//   R4  族 = TSO                      ⇒ FullBarrier（补回 store→load）
//   R5  族 = RC                       ⇒ ReleaseAcquire（**模型基线**：fence 在 RC 下可提供
//                                       release + acquire 两方向；⛔ 不由访问标签门控 ——
//                                       标签只决定**基线 ppo 的强弱**，不决定 fence 的边）
//   R6  族 = PLO                      ⇒ Partial   （只补 W→W 与 R→W）
//   R7  标签族 ∈ {SC,TSO,RC-*,PLO} 之外 ⇒ Invalid
//   ⚠️ R5 的口径已由 `ADR-CEP-13` 的 Status Update(2026-11-24) **追认**（G4-c：登记本
//      派生规则表 + 推翻原 provisional 读法「read=RC-data ⇒ fence 不提供 acquire」——
//      该读法把「模型基线」与「实例标签基线」混用 ⇒ 与 `mesif` 行自相矛盾）。
//   ⚠️ 全表取**模型基线**（= fence 在该模型下**可提供**的效果），⛔ 不是「相对该实例
//      基线 ppo 的增量」。这是 R3（SC ⇒ None）的取值方式，也是 R5 的取值方式。
//
// ── 协议中立（per `include/tlm/cep/AGENTS.md` 铁律 2）────────────────────
//   本头**不得**出现任何协议**类名**，**不得** include 协议引擎 / adapter 头；
//   ⛔ 不含任何实例 id 之外的协议标识 ⇒ 实例 id 只作为**声明面的一行主键**。
//
// ── 诚实边界（⛔ 必须随本头一起读）────────────────────────────────────
//   · 本头登记的是**声明效果**（由声明推导），⛔ **不是**引擎的行为语义，
//     ⛔ **不是**跨一致性域的 barrier 保证（per ADR-CEP-13 决策 1：
//     barrier_domain ≠ 一致性域边界 ⇒ 本视图粒度 = **实例**）。
//   · 本头内含标签 / barrier 的**副本** ⇒ 副本与 JSON 的**逐字段比对**由闸守
//     （红因 fence-input-copy-drift）⇒ 副本不是第二真源，是**受监督的镜像**。
//   · 论文口径的推导规则来自**调研文档**（附属证据层）⇒ 提升为规则**随五步链 ratify**。
//
// ── 判据骨架（判据本体在此，反向 fixture 在闸与测试）────────────────────
//   J-1  实例集双向零 orphan（运行期 + 闸）
//   J-2  派生正确性（闸级**独立 oracle**：闸内手写期望表 ⇄ 本头派生值 · ⛔ 不得自证）
//   J-3  输入副本漂移（运行期 + 闸）：本头副本 ⇄ protocol_instances.json 逐字段
//   J-4  域外必红：任一行派生为 Invalid ⇒ 必红
//   J-5  JSON 缺键必红（带签名 · ⛔ 不静默 SKIP）
//   J-6  basis 非空 + id 唯一
//   J-7  边界常量齐备（机器可读）
//   J-8  通用层零协议类名 / 零 include 协议头 + 与 G1 / G10 正交
#ifndef CPPTLM_TLM_CEP_FENCE_EFFECTS_HH
#define CPPTLM_TLM_CEP_FENCE_EFFECTS_HH

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tlm::cep {

    // ─────────────────────────────────────────────────────────────────────
    //  枚举（协议中立）
    // ─────────────────────────────────────────────────────────────────────

    /// fence（barrier）的一致性**声明效果**（由标签 + barrier 推导）。
    enum class FenceEffect : std::uint8_t {
        /// 无 fence 效果（无 barrier 通道 · 或 SC 已全序 ⇒ fence 是空操作）
        None = 0,
        /// 仅 release 方向有效果 —— ⚠️ **预留值**：当前 R1–R7 下**不可达**
        /// （RC 族恒 ReleaseAcquire；其余族为 None / FullBarrier / Partial / Invalid）
        /// ⇒ ⛔ 删除是**枚举值序变更 = 公共契约面破坏**（per ADR-CEP-13 SU 死枚举处置）
        ReleaseOnly = 1,
        /// 仅 acquire 方向有效果 —— ⚠️ **预留值**：当前 R1–R7 下**不可达**（同 ReleaseOnly）
        /// ⇒ 单方向 fence 建模（「按 read / write 分列多值」）时启用
        AcquireOnly = 2,
        /// release + acquire 两方向都有效果（论文 RC 行）
        ReleaseAcquire = 3,
        /// 全屏障（TSO：补回 store→load 序）
        FullBarrier = 4,
        /// 部分效果（PLO：只补 W→W 与 R→W）
        Partial = 5,
        /// **推导域外**（跨族组合 / 未知标签 ⇒ 必红）
        Invalid = 6,
    };

    inline const char* fence_effect_name(FenceEffect e) noexcept {
        switch (e) {
            case FenceEffect::None: return "none";
            case FenceEffect::ReleaseOnly: return "release_only";
            case FenceEffect::AcquireOnly: return "acquire_only";
            case FenceEffect::ReleaseAcquire: return "release_acquire";
            case FenceEffect::FullBarrier: return "full_barrier";
            case FenceEffect::Partial: return "partial";
            case FenceEffect::Invalid: return "invalid";
        }
        return "unknown";
    }

    /// 一致性标签的**族**（论文模型维：SC / TSO / RC / PLO）。
    enum class ConsistencyFamily : std::uint8_t {
        Sc = 0,
        Tso = 1,
        Rc = 2,
        Plo = 3,
        /// 不在 schema 枚举内 ⇒ 域外
        Unknown = 4,
    };

    /// 标签文本 → 族（**唯一** 映射点 · ⛔ 不得在行内手写族）。
    ///
    /// ⚠️ 取值域 = protocol_instance.schema.json 的 consistency_labels 枚举
    ///    （read ∈ {SC, TSO, RC-acquire, RC-data, PLO} · write ∈ {SC, TSO, RC-release, RC-data, PLO}）。
    constexpr ConsistencyFamily consistency_family_of(std::string_view label) noexcept {
        if (label == "SC") return ConsistencyFamily::Sc;
        if (label == "TSO") return ConsistencyFamily::Tso;
        if (label == "RC-acquire" || label == "RC-data" || label == "RC-release") {
            return ConsistencyFamily::Rc;
        }
        if (label == "PLO") return ConsistencyFamily::Plo;
        return ConsistencyFamily::Unknown;
    }

    /// 推导 fence 效果（规则 R1–R7 · 见文件头）。
    ///
    /// ⚠️ **这是本单元的唯一推导入口** ⇒ 表行由它生成 ⇒ 行值与规则**不可能**不一致
    ///    （一致性由 **编译期** 保证）；「规则实现是否正确」由闸级**独立 oracle** 守。
    constexpr FenceEffect derive_fence_effect(std::string_view read_label,
                                              std::string_view write_label,
                                              bool has_barrier) noexcept {
        // R1 · 无 barrier 通道 ⇒ 无 fence 效果（**最高优先级** · 遮蔽模型规则）
        if (!has_barrier) return FenceEffect::None;

        const ConsistencyFamily rf = consistency_family_of(read_label);
        const ConsistencyFamily wf = consistency_family_of(write_label);

        // R7 · 未知标签 ⇒ 域外
        if (rf == ConsistencyFamily::Unknown || wf == ConsistencyFamily::Unknown) {
            return FenceEffect::Invalid;
        }
        // R2 · 跨族组合 ⇒ 域外
        if (rf != wf) return FenceEffect::Invalid;

        switch (rf) {
            case ConsistencyFamily::Sc:
                // R3 · SC 已全序 ⇒ fence 无额外效果（与 barrier 存在与否无关）
                return FenceEffect::None;
            case ConsistencyFamily::Tso:
                // R4 · TSO：补回 store→load 序
                return FenceEffect::FullBarrier;
            case ConsistencyFamily::Plo:
                // R6 · PLO：只补 W→W 与 R→W
                return FenceEffect::Partial;
            case ConsistencyFamily::Rc:
                // R5 · RC：**模型基线** ⇒ fence 可提供 release + acquire（两方向）
                //   ⛔ 不由访问标签门控：标签（RC-acquire 读 / RC-release 写 / RC-data）
                //      只决定**基线 ppo 的强弱**，不决定 fence 的边
                //   （per ADR-CEP-13 SU(2026-11-24) ② —— 原「按标签逐项」读数已推翻）
                return FenceEffect::ReleaseAcquire;
            case ConsistencyFamily::Unknown:
            default:
                return FenceEffect::Invalid;
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  实例级声明（派生面 · 本仓权威 · 单一真源）
    // ─────────────────────────────────────────────────────────────────────
    //
    // ⚠️ id **逐字**等于 include/tlm/cep/protocol_instances.json 的 id 键值。
    // ⚠️ read_label / write_label / has_barrier 是 **JSON 的镜像副本**
    //    ⇒ 副本与 JSON 的逐字段比对由闸守（红因 fence-input-copy-drift）。
    // ⚠️ effect **不由手写**：由 derive_fence_effect() 生成（见 make_... ）。
    // ⛔ 本结构**不含** G1 的 egress / in_flight、**不含** G10 的 hop / timing 字段（**正交**）。

    // == AC-7 scan begin: FenceEffectCapability fields ==
    struct FenceEffectCapability {
        const char* id;             ///< 实例 id（逐字等于 protocol_instances.json 的 id）
        const char* read_label;     ///< 一致性标签 · 读（JSON 镜像副本）
        const char* write_label;    ///< 一致性标签 · 写（JSON 镜像副本）
        bool has_barrier;           ///< barrier 流支持（JSON 镜像副本）
        FenceEffect effect;         ///< **派生**效果（⛔ 不由手写）
        const char* basis;          ///< 判定依据（非空）
    };
    // == AC-7 scan end: FenceEffectCapability fields ==

    // == AC-7 scan begin: kFenceEffectCapabilities init ==
    /// 8 行派生表（**由推导函数生成** ⇒ 行值与规则不可能不一致）。
    constexpr std::array<FenceEffectCapability, 8> make_fence_effect_capabilities() {
        return {{
            {"chi_rnf", "SC", "SC", false,
             derive_fence_effect("SC", "SC", false),
             "R1 · 无 barrier 流 ⇒ fence 无通道 ⇒ None（实例粒度 · 不区分网络侧）"},
            {"cxl_2_0", "SC", "SC", false,
             derive_fence_effect("SC", "SC", false),
             "R1 · 无 barrier 流 ⇒ fence 无通道 ⇒ None（实例粒度 · 不区分网络侧）"},
            {"cxl_3_0_symmetric", "SC", "SC", false,
             derive_fence_effect("SC", "SC", false),
             "R1 · 无 barrier 流 ⇒ fence 无通道 ⇒ None（实例粒度 · 不区分网络侧）"},
            {"mesif", "RC-acquire", "RC-release", true,
             derive_fence_effect("RC-acquire", "RC-release", true),
             "R5 · RC 族 ⇒ fence 可提供 release + acquire（模型基线）（实例粒度 · 不区分网络侧）"},
            {"moesi_gpu", "RC-data", "RC-release", true,
             derive_fence_effect("RC-data", "RC-release", true),
             "R5 · RC 族 ⇒ fence 可提供 release + acquire（模型基线 · 与访问标签的基线强弱无关）（实例粒度 · 不区分网络侧）"},
            {"nvlink_4", "SC", "SC", true,
             derive_fence_effect("SC", "SC", true),
             "R3 · SC 已全序 ⇒ fence 无额外效果（有通道但空操作）（实例粒度 · 不区分网络侧）"},
            {"tilelink_c", "SC", "SC", false,
             derive_fence_effect("SC", "SC", false),
             "R1 · 无 barrier 流 ⇒ fence 无通道 ⇒ None（实例粒度 · 不区分网络侧）"},
            {"ualink_4", "PLO", "PLO", false,
             derive_fence_effect("PLO", "PLO", false),
             "R1 · 无 barrier 流 ⇒ 遮蔽 R6（未来开 barrier ⇒ 翻转为 Partial）（实例粒度 · 不区分网络侧）"},
        }};
    }

    inline constexpr std::array<FenceEffectCapability, 8> kFenceEffectCapabilities =
        make_fence_effect_capabilities();
    // == AC-7 scan end: kFenceEffectCapabilities init ==

    /// 声明行数（与实例数相等 —— 由 J-1 判据守）。
    inline constexpr std::size_t kFenceEffectCapabilityCount = kFenceEffectCapabilities.size();

    /// 声明自洽谓词（运行期；**J-6** + **J-4** 的行级自证面）。
    ///
    /// · id / basis / read_label / write_label 均非空
    /// · id 唯一
    /// · **无** `Invalid` 行（域外 ⇒ 必红）
    inline bool fence_effect_capabilities_are_consistent(const FenceEffectCapability* rows,
                                                          std::size_t count) {
        if (rows == nullptr || count == 0) return false;
        for (std::size_t i = 0; i < count; ++i) {
            const FenceEffectCapability& r = rows[i];
            if (r.id == nullptr || r.id[0] == '\0') return false;
            if (r.basis == nullptr || r.basis[0] == '\0') return false;
            if (r.read_label == nullptr || r.read_label[0] == '\0') return false;
            if (r.write_label == nullptr || r.write_label[0] == '\0') return false;
            if (r.effect == FenceEffect::Invalid) return false;   // J-4 · 域外必红
            for (std::size_t j = i + 1; j < count; ++j) {
                if (std::string_view(rows[i].id) == std::string_view(rows[j].id)) {
                    return false;   // id 唯一
                }
            }
        }
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────
    //  诚实边界（机器可读 —— J-7；⛔ 措辞变更须同步闸的判据）
    // ─────────────────────────────────────────────────────────────────────

    /// 派生值 = **声明效果**（由声明推导）⛔ **不是**引擎的行为语义。
    inline constexpr bool kFenceEffectIsDeclarationOnly = true;

    /// ⛔ 派生值**不是**跨一致性域的 barrier 保证（per ADR-CEP-13 决策 1）。
    inline constexpr bool kFenceEffectIsNotCrossDomainGuarantee = true;

    /// 无 barrier 通道 ⇒ fence 无效果（R1 · 遮蔽模型规则）。
    inline constexpr bool kBarrierAbsenceMeansNoFenceEffect = true;

    /// SC 已全序 ⇒ fence 无额外效果（R3 · 与 barrier 存在与否无关）。
    inline constexpr bool kScModelMeansNoExtraFenceEffect = true;

    /// RC 族：论文 RC 行是**并集上界**；本单元按方向取**子集**（更严）。
    inline constexpr bool kRcEffectIsPaperUpperBoundSubset = true;

    /// 派生粒度 = **实例**（⛔ 不区分 barrier 发生在哪个网络侧）。
    inline constexpr bool kFenceEffectGranularityIsPerInstance = true;

}  // namespace tlm::cep

#endif  // CPPTLM_TLM_CEP_FENCE_EFFECTS_HH
