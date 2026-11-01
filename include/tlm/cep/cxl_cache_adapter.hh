// include/tlm/cep/cxl_cache_adapter.hh
// CxlCacheAdapter —— `cxl_2_0` 协议实例 adapter(CXL 2.0 非对称 · CXL.cache · host-centric)
//
// 功能:把 CXL.cache 的协议语义实现为**可执行**的契约载体:
//       (1) 4 态 × 17 事件**三态**转换查询(**委托**规约载体,零自有表副本)
//       (2) 4→4 `perm` 投影(**委托**规约载体)
//       (3) 静默升级判定 + 残余风险登记的**委托**
// 作者: CppTLM Team / 日期: 2026-11-01
//
// ── 定位(per `openspec/changes/cpptlm-cep-cxl-nonsymmetric` 的
//    `cep-cxl-nonsymmetric` capability)─────────────────────────────────────
// 本类**不是**缓存性能模型,而是**协议插件的语义载体**:
//   · 状态机 = CXL.cache 自身的 **MESI 4 态**语义(不是 MESIF 的 5 态);
//   · `perm` 投影 = 跨域接口上的 4 值权限表达(per `ADR-CEP-02` 的 `perm` 4 值锁);
//   · ⚠️ **本类不含消息类 → `CATEGORY_*` 映射**:CXL 的通道划分(D2H/H2D × Req/Rsp/Data)
//     与既有 3 类 flit 词表**不是同一层** ⇒ 映射属**后续**工作(本 change 非目标),
//     此刻**刻意不做**(宁可缺,不可编)。
//
// ── ⭐ 零副本原则(per 本 change 的 design `D-α`)────────────────────────
// 本 adapter **SHALL NOT** 携带自有的转换表 / 投影表副本:
//   · 规约层 `cxl_state_spec.hh` 是 CXL.cache 语义的**唯一真源**
//     (68 格三态矩阵 + 逐格依据 + 钉版台账,per Entry #127 归档);
//   · 本类以 `constexpr` **委托**消费它 ⇒ 规约一改,本类**编译期即红**;
//   · **结构性保证**:本文件的**去注释后**文本 SHALL **不**含 `State::` / `Event::` /
//     `Perm::` **任何字面量**,SHALL **不**含任何**表名**
//     (`kTransitions` / `kPermProjection` / `kTable`)⇒ 本类在语法上
//     **不可能**携带自己的表,只能**调访问器**。
//     (判据:`CNA-01` ③ 的静态扫描 + 扫描器**鉴别力**自测)
//
// ── ⚠️ **三态消费纪律**(per design `D-α.1` / `CNA-02`)─────────────────────
// 规约层的 68 格是**三态**:`Known` 26 / `Illegal` 0 / `Unknown` 42。
// ⇒ 本类 SHALL **区分**三态,**SHALL NOT** 把 `Unknown` 折叠成哨兵值
//   (MESIF 的 `0xFF = 未定义(空洞)` 做法会让消费者把「未知」读成「非法」
//    ⇒ 那是一处**新的未实测断言**,per 规约层 `design.md D-γbis` 已否决)。
// ⇒ 本类**刻意不提供**返回 `std::uint8_t` 目标态的折叠接口。
//
// ── 边界 ────────────────────────────────────────────────────────────────
//   · **header-only**,**不进 `ModuleFactory`**(testbench 定位的组件,同 `MesifAdapter`)
//   · **不注册** 23 ABI · **不改** frozen header · **不扩** `CATEGORY_*` 词表
//   · **不扩** `perm` 4 值 · **不引入**新的 SharerTracker 实现
//   · **不写** protocol engine(照 `moesi_gpu` 先例 = adapter-only)
//   · **新增 invariant 数 = 0**
//
// ── 依据 ────────────────────────────────────────────────────────────────
//   · 4 态 / 17 事件 / 三态转换表 / 4→4 投影 ⇒ `include/tlm/cep/cxl_state_spec.hh`
//     (规约载体;逐格依据见锚点摘要 + 核对表)
//   · `perm` 4 值锁 ⇒ `ADR-CEP-02` CEN-Ib11 · 静默升级 ⇒ `ADR-CEP-12 §10`
//   · 口径 7 条 ⇒ `ADR-CEP-11` 的 `## Status Update(2026-11-01)` + 台账 Entry #122
//   · ⛔ **合规**:CXL 规范正文**不得**投喂任何 AI(ECA §3 + AI AUP)
//     ⇒ 本类的全部语义**只来自**公开非规范来源与**已裁决**口径

#ifndef CPPTLM_TLM_CEP_CXL_CACHE_ADAPTER_HH
#define CPPTLM_TLM_CEP_CXL_CACHE_ADAPTER_HH

#include <cstddef>
#include <cstdint>

#include "tlm/cep/cxl_state_spec.hh"

namespace tlm::cep {

    /// `cxl_2_0` 协议实例 adapter —— CXL.cache 4 态 × 17 事件(三态)+ 4→4 `perm` 投影
    class CxlCacheAdapter {
    public:
        // ── 4 态 / 17 事件 / 4 值 perm(**别名**规约载体,不另立词表)────────
        using State = CxlStateSpec::State;
        using Event = CxlStateSpec::Event;
        using Perm = CxlStateSpec::Perm;
        /// 转换格的**三态语义**(委托规约载体)
        using CellKind = CxlStateSpec::CellKind;

        static constexpr std::size_t kStateCount = CxlStateSpec::kStateCount;
        static constexpr std::size_t kEventCount = CxlStateSpec::kEventCount;
        static constexpr std::size_t kPermCount = CxlStateSpec::kPermCount;

        /// 协议标识(逐字对齐 `protocol_instances.json` 的 `adapter` 字段)
        static constexpr const char* kAdapterName = "CxlCacheAdapter";
        /// 实例 id(逐字对齐 `protocol_instances.json` 的 `id` 字段)
        static constexpr const char* kInstanceId = "cxl_2_0";

        // ══════════════════════════════════════════════════════════════════
        //  (1) 三态转换查询 —— **委托**规约载体(零副本)
        // ══════════════════════════════════════════════════════════════════

        /// 转换格的**语义种类**(`Known` / `Illegal` / `Unknown`)。
        /// ⚠️ **三态 SHALL NOT 被折叠** —— 「不知道」≠「非法」(per `CNA-02`)。
        static constexpr CellKind transition_kind(State s, Event e) noexcept {
            return CxlStateSpec::transition(s, e).kind;
        }

        /// 转换的**目标态** —— ⚠️ **仅 `Known` 时有意义**;
        /// 对 `Illegal` / `Unknown` 读它即**违约**(消费方 SHALL 先判 `transition_kind`)。
        static constexpr State transition_target(State s, Event e) noexcept {
            return CxlStateSpec::transition(s, e).target;
        }

        /// 是否为**已知目标态**的转换(便捷判定;`Unknown` ⇒ `false`)。
        static constexpr bool transition_is_known(State s, Event e) noexcept {
            return CxlStateSpec::transition(s, e).kind == CellKind::Known;
        }

        /// 是否为**已知不合法**的转换(⚠️ 当前恒 `false` —— 公开来源**不给**
        /// 「哪些转换非法」;逐状态合法性表在规范 §3.2.5.15,**禁读** ⇒ 登记为 R-2)。
        static constexpr bool transition_is_illegal(State s, Event e) noexcept {
            return CxlStateSpec::transition(s, e).kind == CellKind::Illegal;
        }

        /// 是否为**未知**的转换(⇒ 消费方 SHALL 走**保守降级**路径)。
        static constexpr bool transition_is_unknown(State s, Event e) noexcept {
            return CxlStateSpec::transition(s, e).kind == CellKind::Unknown;
        }

        // ══════════════════════════════════════════════════════════════════
        //  (2) 4→4 `perm` 投影 —— **委托**规约载体
        // ══════════════════════════════════════════════════════════════════

        /// 稳定态 → 4 值 `perm` 投影(CXL 无瞬态 ⇒ 4 态**全部**有投影)。
        static constexpr bool perm_of(State s, Perm& out) noexcept {
            return CxlStateSpec::perm_of(s, out);
        }

        // ══════════════════════════════════════════════════════════════════
        //  (3) 静默升级 + 残余风险 —— **委托**规约载体
        // ══════════════════════════════════════════════════════════════════

        /// `E + 写事件 → M` 的**静默升级**判定。
        /// ⚠️ **该判据的 basis = `public-analogical` + `pending`**(MESI 通用性质;
        ///    公开来源**未逐字断言**该格)⇒ **SHALL NOT** 当作已裁决事实陈述。
        static constexpr bool is_silent_upgrade(State s, Event e) noexcept {
            return CxlStateSpec::is_silent_upgrade(s, e);
        }

        /// 残余风险清单(**委托** ⇒ 散文无双真源)。
        static constexpr const auto& kResidualRisks = CxlStateSpec::kResidualRisks;
        static constexpr std::size_t kResidualRiskCount = CxlStateSpec::kResidualRiskCount;

        /// transient 态**不建模**(公开来源明说其「not officially part of
        /// the CXL.cache specification」)⇒ 计数恒 0(**委托**)。
        static constexpr std::size_t kTransientStateCount = CxlStateSpec::kTransientStateCount;
    };

}  // namespace tlm::cep

#endif  // CPPTLM_TLM_CEP_CXL_CACHE_ADAPTER_HH
