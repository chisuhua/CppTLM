// =============================================================================
// include/tlm/cep/ualink_adapter.hh
//   —— `UalinkAdapter`:UALink INC 协议族 adapter(**独立**载体 · **零自有表副本**)
//
// 作者: CppTLM Team / 日期: 2026-11-04
// 配套: include/tlm/cep/ualink_inc_state_spec.hh(**已归档**的规约载体,唯一真源)
//       openspec/changes/cpptlm-cep-ualink-inc/(4 件套 + 3 份 delta spec)
//       台账 obj-cep-platform.md Entry #204(建档 + 范围收窄)· #203(规约层归档)
//       docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md(basis 唯一人工审计面)
//
// ── 定位(per 本 change 的 `design.md` §2.1)─────────────────────────────────
// **不是缓存、不是网络组件**:它不接 `connections[]`、不进 `ModuleFactory`、不注册 23 ABI。
// 它是 **UALink INC 语义在实现层的门面**,**只消费**规约载体 `ualink_inc_state_spec.hh`。
//
// ── ⭐ 零自有表副本(per delta spec UIA-01)────────────────────────────────
// 本文件的**去注释后**文本 SHALL **不**含下列**成员字面量 / 表名**:
//   `Obligation::` · `BasisKind::` · `CertificateId::` · `UntestedId::` · `IncOp::` · `PendingId::`
//   · `BasisCell` · `CertificateSpec` · `UntestedItem` · `PendingItem` · `PrimitiveSet` · `IncOpBasis`
//   · `Ic1Discrimination` · `kBasisObligations` · `kBasisIncOps` · `kPendingSemanticItems`
//   · `kCertificateSpecs` · `kUntestedItems` · `kIncOps` · `kIncOpPrimitives` · `kUsedPrimitiveNames`
//   · `kObligationCount` 之外的表名一律禁止(⛔ 本类**不**转出任何载体表)
// ⇒ 本类在语法上**不可能**携带自己的状态机 / 投影表 / basis 表,只能**调访问器**。
// **类型名**的**别名**亦 SHALL NOT 出现(本类**不**转出任何载体类型)—— 与
// `NvlinkAdapter` 的「类型别名允许」口径**故意相反**:本仓当前只有**一个** M-2 使用方,
// 转出类型会把 UALink 专有类型**扩散**到判据面 ⇒ 判据面**直接消费**规约载体即可。
//
// ── ⛔ 本类 SHALL NOT 暴露 `transition_*`(per delta spec UIA-02)────────────
//   INC 四原语的**规范定义**是 **pending**(锚点摘要 `N-1`:公开来源**连原语名都没有**)
//   ⇒ 转移表会要求**推理填空** ⇒ 本类提供该 API 即等于**自造转移表**。
//
// ── ⛔ 本类 SHALL NOT 声明实例 id(per delta spec UIA-07)──────────────────
//   `ualink_4` 实例**尚未声明**(取值不可判定,per `ADR-CEP-04` SU 2026-11-03 §③ 残余
//   + 锚点摘要 `N-8` / `N-9` **pending**)⇒ 本类**无**可指的实例 ⇒ 不声明 `kInstanceId`。
// =============================================================================

#ifndef TLM_CEP_UALINK_ADAPTER_HH
#define TLM_CEP_UALINK_ADAPTER_HH

#include <cstddef>

#include "tlm/cep/ualink_inc_state_spec.hh"

namespace tlm {
    namespace cep {

        /// **UALink INC** 协议族 adapter —— 零自有表副本,只消费规约载体。
        ///
        /// ⚠️ 消费方纪律:本类**不**持任何 UALink 协议的**表副本**(态 / 投影 / basis 表全部
        /// **委托**规约载体)⇒ 规约改一格、本类**不**必红(本类**不**依赖表)。
        class UalinkAdapter {
        public:
            // ── 标量常量(**委托**规约载体;**不是**副本)────────────────────
            static constexpr std::size_t kObligationCount = ualink_inc::kObligationCount;
            static constexpr std::size_t kIncOpCount = ualink_inc::kIncOpCount;
            static constexpr std::size_t kCertificateCount = ualink_inc::kCertificateCount;
            static constexpr std::size_t kUntestedCount = ualink_inc::kUntestedCount;

            /// **adapter 名** —— per 07 号 §5.1.1 的 UALink 行(锚点 `C-6`)
            static constexpr const char* kAdapterName = "UalinkAdapter";

            /// **协议版本锁** —— `ualink-2.0` / UALink Common 2.0(per `ADR-CEP-11 §3`,锚点 `C-5`)
            static constexpr const char* kProtocolVersion = "ualink-2.0";

            // ── 判定面(**全部委托**规约载体)──────────────────────────────

            /// **partial failure 必须显式**(走既有 `CollectiveEvt::Fault` 通道)—— 委托
            static constexpr bool partial_failure_must_be_explicit(bool silent) noexcept {
                return ualink_inc::partial_failure_must_be_explicit(silent);
            }

            /// **槽位语义继承良定义**(该网全部有绑定的实例**逐类相等**)—— 委托
            static constexpr bool vc_inheritance_is_well_defined(bool all_instances_agree) noexcept {
                return ualink_inc::vc_inheritance_is_well_defined(all_instances_agree);
            }

            /// **该网是否可用 M-2**(映射不一致 ⇒ **不得**用)—— 委托
            static constexpr bool m2_usable_on_network(bool all_bound_instances_agree) noexcept {
                return ualink_inc::m2_usable_on_network(all_bound_instances_agree);
            }

            /// **四条证书的规约面齐备**(逐条 `detail` 子标记 / 义务 / 逐边依据 / 参照域)—— 委托
            static constexpr bool certificate_specs_wellformed() noexcept {
                return ualink_inc::all_certificate_specs_wellformed();
            }

            /// **未检验项未被冒充成「已验证 / 已满足」**(U-1 / U-2)—— 委托
            static constexpr bool untested_registry_clean() noexcept {
                return ualink_inc::all_untested_not_satisfied();
            }

            // ── ⛔ 三条**否定式**事实(委托;**不是**「保证为真」而是「事实为假」)──

            /// ⛔ **载体内不得**定义 reduce 算子族(唯一真源 = 既有 `ReduceOp` 恰 6 值)—— 委托
            static constexpr bool operator_family_defined_in_carrier() noexcept {
                return ualink_inc::kOperatorFamilyDefinedInCarrier;
            }

            /// ⛔ **「重跑通道依赖闸」SHALL NOT** 作为证书判据(实测恒真)—— 委托
            static constexpr bool dag_rerun_is_certificate() noexcept {
                return ualink_inc::kDagRerunIsCertificate;
            }

            /// ⛔ **载体不得**暴露区域表(区域表运行时状态归 collective 侧)—— 委托
            static constexpr bool region_table_exposed() noexcept {
                return ualink_inc::kRegionTableExposed;
            }
        };

    } // namespace cep
} // namespace tlm

#endif // TLM_CEP_UALINK_ADAPTER_HH
