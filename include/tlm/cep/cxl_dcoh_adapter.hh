// =============================================================================
// include/tlm/cep/cxl_dcoh_adapter.hh
//   —— `CxlDcohAdapter`:CXL 3.0 对称(DCOH)协议实例 adapter(**独立**载体)
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: include/tlm/cep/cxl_dcoh_state_spec.hh(**已归档**的规约载体,唯一真源)
//       openspec/changes/cpptlm-cep-cxl-dcoh-symmetric/(4 件套 + delta spec CDX-01 ~ CDX-08)
//       台账 obj-cep-platform.md Entry #154(4 项 architect 裁决)· #159(规约层归档)· #160(本 change)
//       docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md(basis 唯一人工审计面)
//
// ── 定位(per 本 change 的 `design.md` §1)───────────────────────────────
// **不是缓存、不是网络组件**:它不接 `connections[]`、不进 `ModuleFactory`、不注册 23 ABI。
// 它是 **CXL 3.0 对称(DCOH)语义的实现层载体**,**只消费**规约载体 `cxl_dcoh_state_spec.hh`。
//
// ⚠️ **为什么是独立 adapter**(per architect 裁决 #4)──────────────────────
// `CxlCacheAdapter` 是 **`cxl_2_0` 专用**(零自有表副本 + **单一委托** `cxl_state_spec.hh`)
// ⇒ 支持双载体将迫使它引入**载体选择逻辑** ⇒ **破坏**「去注释后文本不含成员字面量」的
//    **结构性保证**;且对称模式的**状态空间完全不同**(3 套 vs 1 套)⇒ 一个 adapter 一套真相源。
//
// ── ⭐ 零自有表副本(per delta spec CDX-01)─────────────────────────────
// 本文件的**去注释后**文本 SHALL **不**含下列**成员字面量**:
//   `DevCacheState::` · `DcohViewState::` · `HomeDirState::` · `CxlMemChannel::`
//   · `CxlCacheChannel::` · `ChannelDirection::` · `Perm::`
// ⇒ 本类在语法上**不可能**携带自己的状态机 / 投影表,只能**调访问器**。
// **类型名**(`using DevCacheState = cxl_dcoh::DevCacheState;`)SHALL **允许**
// (它是**类型**别名,不是**成员**访问)⇒ 与 `CxlCacheAdapter` 的 `using State = CxlStateSpec::State;`
// 同构。
//
// ⚠️ **本类 SHALL NOT 暴露任何「表」**(通道词表 / 方向表 / 投影表 / basis 表)
//   ⇒ 那些**只**由规约载体持有;本类**只**暴露**标量**访问器
//   ⇒ 这样「零副本」既是**语义**要求也是**静态可判**的要求。
//
// ⛔ **本类 SHALL NOT 暴露 `transition_*`**(per delta spec CDX-03)
//   规约载体**没有转移表**(对称模式的转移语义在**公开非规范来源**里**没有**
//   ⇒ 转移表会要求**推理填空** ⇒ 禁止)⇒ 本类提供该 API 即等于**自造转移表**。
// =============================================================================

#ifndef TLM_CEP_CXL_DCOH_ADAPTER_HH
#define TLM_CEP_CXL_DCOH_ADAPTER_HH

#include <cstddef>

#include "tlm/cep/cxl_dcoh_state_spec.hh"

namespace tlm {
    namespace cep {

        /// **CXL 3.0 对称(DCOH)**协议实例 adapter —— 零自有表副本,只消费规约载体
        class CxlDcohAdapter {
        public:
            // ── 类型别名(**类型名**允许;**成员字面量**禁止)─────────────────────
            using DevCacheState = cxl_dcoh::DevCacheState;
            using DcohViewState = cxl_dcoh::DcohViewState;
            using HomeDirState = cxl_dcoh::HomeDirState;
            using Perm = cxl_dcoh::Perm;
            using SymmetricCapability = cxl_dcoh::SymmetricCapability;

            /// **标量**常量(委托规约载体;**不是**副本)
            static constexpr std::size_t kDevCacheStateCount = cxl_dcoh::kDevCacheStateCount;
            static constexpr std::size_t kDcohViewStateCount = cxl_dcoh::kDcohViewStateCount;
            static constexpr std::size_t kHomeDirStateCount = cxl_dcoh::kHomeDirStateCount;
            static constexpr std::size_t kCxlMemChannelCount = cxl_dcoh::kCxlMemChannelCount;
            static constexpr std::size_t kCxlCacheChannelCount = cxl_dcoh::kCxlCacheChannelCount;

            // ⚠️ **刻意不暴露**「设备发起的 snoop 集合 / 计数」——
            //    表达它需要**引用载体表名**(`kDeviceInitiatedSnoopSet`)⇒ 违「零副本」禁词表
            //    ⇒ 该判别式由 `is_cxl3_symmetric`(**委托**载体)承担 ⇒ 本类**零表名**。

            static constexpr const char* kAdapterName = "CxlDcohAdapter";
            static constexpr const char* kInstanceId = "cxl_3_0_symmetric";

            /// `perm` 投影(**委托**规约载体;4→4,仅来自缓存代理域)
            static constexpr Perm perm_of(DevCacheState s) noexcept {
                return cxl_dcoh::perm_of(s);
            }

            /// 窄义「对称」判别式(**委托**规约载体;5 谓词合取)
            static constexpr bool is_cxl3_symmetric(const SymmetricCapability& c) noexcept {
                return cxl_dcoh::is_cxl3_symmetric(c);
            }

            // ── ⛔ **无** `transition_*` API(per delta spec CDX-03)────────────────
            //    规约载体**无转移表** ⇒ 本类提供该 API 即等于**自造转移表** ⇒ 禁止。
        };

    } // namespace cep
} // namespace tlm

#endif // TLM_CEP_CXL_DCOH_ADAPTER_HH
