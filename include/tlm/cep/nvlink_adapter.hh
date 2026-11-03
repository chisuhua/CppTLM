// =============================================================================
// include/tlm/cep/nvlink_adapter.hh
//   —— `NvlinkAdapter`:NVLink SHARP 协议实例 adapter(**独立**载体)
//
// 作者: CppTLM Team / 日期: 2026-11-03
// 配套: include/tlm/cep/nvlink_sharp_state_spec.hh(**已归档**的规约载体,唯一真源)
//       openspec/changes/cpptlm-cep-nvlink-sharp/(4 件套 + 2 份 delta spec)
//       台账 obj-cep-platform.md Entry #187(双审修订)· #186(规约层归档 + 移交)
//       docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md(basis 唯一人工审计面)
//
// ── 定位(per 本 change 的 `design.md` §2)──────────────────────────────────
// **不是缓存、不是网络组件**:它不接 `connections[]`、不进 `ModuleFactory`、不注册 23 ABI。
// 它是 **NVLink SHARP 语义��实现层载体**,**只消费**规约载体 `nvlink_sharp_state_spec.hh`。
//
// ── ⭐ 零自有表副本(per delta spec NVA-01)────────────────────────────────
// 本文件的**去注释后**文本 SHALL **不**含下列**成员字面量 / 表名**:
//   `IngressDecision::` · `RegisterResult::` · `EpochState::` · `OrderingRule::`
//   · `VcReservationState::` · `CollectiveScope::` · `EvidenceKind::` · `SourceId::`
//   · `Basis{` · `kSourceLicenseConfirmed` · `all_basis_valid` · `PendingSemanticItem`
//   · `kBasis*` · `kPendingSemanticItems`
// ⇒ 本类在语法上**不可能**携带自己的状态机 / 投影表 / basis 表,只能**调访问器**。
// **类型名**(`using IngressDecision = nvlink_sharp::IngressDecision;`)SHALL **允许**
// (它是**类型**别名,不含 `::` ⇒ 与 `CxlDcohAdapter` 的
// `using DevCacheState = cxl_dcoh::DevCacheState;` 同构)。
//
// ⚠️ **文本扫描的已知盲区**:**数值镜像表**(如 `static constexpr uint8_t kMap[12]`)
// **不在**文本扫描面内 ⇒ 由锚点摘要 §2bis 的 **12 格人工核对** + 载体侧 `static_assert` 兜底。
//
// ── ⛔ 本类 SHALL NOT 暴露 `transition_*`(per delta spec NVA-01)────────────
//   规约载体**没有**转移表(SHARP 转移语义在**公开非规范来源**里**没有**
//   ⇒ 转移表会要求**推理填空**)⇒ 本类提供该 API 即等于**自造转移表**。
//
// ── ⛔ 本类 SHALL NOT 暴露 `collective_region_table` ───────────────────────
//   区域表的**运行时状态**归实现层(collective 侧)⇒ 载体**不**拥有表、
//   **不**拥有状态、**不**暴露表类型 ⇒ **委托 ≠ 源端获得区域表可见性**。
//
// ── ⚠️ V3(barrier 分类 litmus)SHALL NOT 落在本类 ──────────────────────────
//   V3 是**协议中立**判读面 ⇒ 载体 = `include/tlm/cep/collective_plane.hh` 的
//   `barrier_key_class`(per `design.md` N-7 · oracle R1:⛔ **不**复用
//   `fabric_ingress_classify` —— 该名已被 `INV-CEP-04-IC5` 占用)。
// =============================================================================

#ifndef TLM_CEP_NVLINK_ADAPTER_HH
#define TLM_CEP_NVLINK_ADAPTER_HH

#include <cstddef>
#include <cstdint>

#include "tlm/cep/nvlink_sharp_state_spec.hh"

namespace tlm {
    namespace cep {

        /// **NVLink SHARP** 协议实例 adapter —— 零自有表副本,只消费规约载体。
        ///
        /// ⚠️ 消费方纪律:本类**不**持任何 NVLink 协议的**表副本**(态 / 投影 / basis 表全部
        /// **委托**规约载体)⇒ 规约改一格、本类**不**必红(本类**不**依赖表)。
        class NvlinkAdapter {
        public:
            // ── 类型别名(**类型名**允许;**成员**字面量禁止)─────────────────────
            using SourceId = nvlink_sharp::SourceId;
            using EvidenceKind = nvlink_sharp::EvidenceKind;
            using Basis = nvlink_sharp::Basis;
            using IngressDecision = nvlink_sharp::IngressDecision;
            using RegisterResult = nvlink_sharp::RegisterResult;
            using EpochState = nvlink_sharp::EpochState;
            using OrderingRule = nvlink_sharp::OrderingRule;
            using VcReservationState = nvlink_sharp::VcReservationState;
            using CollectiveScope = nvlink_sharp::CollectiveScope;

            /// **标量**常量(委托规约载体;**不是**副本)
            static constexpr std::size_t kIngressDecisionCount = nvlink_sharp::kIngressDecisionCount;
            static constexpr std::size_t kRegisterResultCount = nvlink_sharp::kRegisterResultCount;
            static constexpr std::size_t kEpochStateCount = nvlink_sharp::kEpochStateCount;
            static constexpr std::size_t kOrderingRuleCount = nvlink_sharp::kOrderingRuleCount;
            static constexpr std::size_t kVcReservationStateCount =
                nvlink_sharp::kVcReservationStateCount;
            static constexpr std::size_t kCollectiveScopeCount = nvlink_sharp::kCollectiveScopeCount;

            static constexpr const char* kAdapterName = "NvlinkAdapter";
            static constexpr const char* kInstanceId = "nvlink_4";
            static constexpr const char* kProtocolVersion = "nvlink-4.0";

            // ── R1/R2 分类判据(**委托**)──────────────────────────────────────
            //
            // ⚠️ 签名里**刻意没有** opcode(R1:opcode **永不作分类器**)· 区域表 · group 上下文
            //   ⇒ 载体侧有**编译期锁**(`CP-OPCODE-CLASSIFIER`),本类**不**重述。

            /// 唯一分类点(fabric ingress)—— 委托规约载体
            static constexpr IngressDecision classify_at_fabric_ingress(bool region_hit,
                                                                       bool epoch_committed) noexcept {
                return nvlink_sharp::classify_at_fabric_ingress(region_hit, epoch_committed);
            }

            // ── R3/R4 区域与 epoch 纪律(**委托**)────────────────────────────

            /// 注册三步原子的可判定出口 —— 委托
            static constexpr RegisterResult register_region(bool drain_succeeded) noexcept {
                return nvlink_sharp::register_region(drain_succeeded);
            }

            /// epoch 是否已 commit(⇒ 命中即 divert)—— 委托
            static constexpr bool epoch_routes_to_collective(EpochState state) noexcept {
                return nvlink_sharp::epoch_routes_to_collective(state);
            }

            /// 注销后的归属判定 —— 委托
            static constexpr IngressDecision
            classify_after_destroy(bool region_still_registered) noexcept {
                return nvlink_sharp::classify_after_destroy(region_still_registered);
            }

            // ── R5 可观测性(**委托**)────────────────────────────────────────

            /// fail-stop 判据 —— 委托
            static constexpr bool is_fail_stop(bool region_hit, bool epoch_committed,
                                               bool coherence_atomic_hit) noexcept {
                return nvlink_sharp::is_fail_stop(region_hit, epoch_committed, coherence_atomic_hit);
            }

            // ── 跨平面 ordering A/B/C/D(**委托**)────────────────────────────

            static constexpr bool ordering_a_allows_reduce(bool atomic_completed,
                                                            bool reduce_may_access) noexcept {
                return nvlink_sharp::ordering_a_allows_reduce(atomic_completed, reduce_may_access);
            }

            static constexpr bool ordering_b_allows_atomic(bool reduce_committed,
                                                            bool atomic_may_access) noexcept {
                return nvlink_sharp::ordering_b_allows_atomic(reduce_committed, atomic_may_access);
            }

            static constexpr bool ordering_c_orders_reduce(std::uint32_t code_a,
                                                             std::uint32_t code_b) noexcept {
                return nvlink_sharp::ordering_c_orders_reduce(code_a, code_b);
            }

            static constexpr bool ordering_d_allows_reduce(bool barrier_completed,
                                                            bool reduce_may_access) noexcept {
                return nvlink_sharp::ordering_d_allows_reduce(barrier_completed, reduce_may_access);
            }

            /// 「code 相等 ⇒ 顺序未定义」—— 显式登记(委托)
            static constexpr bool kReduceCodeTieUnspecified =
                nvlink_sharp::kReduceCodeTieUnspecified;
        };

    } // namespace cep
} // namespace tlm

#endif // TLM_CEP_NVLINK_ADAPTER_HH
