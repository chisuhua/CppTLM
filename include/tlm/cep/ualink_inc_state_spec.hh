// =============================================================================
// include/tlm/cep/ualink_inc_state_spec.hh
//   —— UALink INC 规约载体(规约层 · header-only · 自足)
//
// 作者: CppTLM Team / 日期: 2026-11-04
// 配套: docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md(basis 唯一人工审计面;
//         `A-n` 公开断言 / `N-n` pending 白名单 / `C-n` 契约锚点)
//       openspec/changes/cpptlm-cep-ualink-inc-state-spec/(change 4 件套)
//       台账 obj-cep-platform.md Entry #194(improvement)· #195(本 change 建档 + Task 0)
//         · #193(三则 ADR SU:ADR-CEP-01 / 04 / 09)
//       docs/adr/ADR-CEP-01-... §Status Update 2026-11-03(#4)(路径 D + 四条证书)
//       docs/adr/ADR-CEP-03-... §3.1(原语集 / 跨平面契约)
//
// ── 定位 ─────────────────────────────────────────────────────────────────────
// **不是缓存、不是网络组件**:它不接 `connections[]`、不进 `ModuleFactory`、不注册 ABI。
// 它是 **UALink INC 语义的规约真源**:INC 四原语的自持 id 与**到既有原语的名字映射**
// · **逐格 `basis`** · **7 项 pending 登记**(2026-11-06: `N-8` / `N-9` 转正 ⇒ 9 → 7)· 形状型判定谓词
// ⇒ 供第 1 个 change 的 `UalinkAdapter` / `FabricCollectivePlane` **零自有表副本**地委托消费。
//
// ── ⛔ 自足红线(per ADR-CEP-03 SU 2026-11-03(#2) F.1 · 本 change design §2.1)──
// ① **只 include 标准库**;⛔ **SHALL NOT** include 任何项目内头
//    ⇒ 故本载体**不引用**平台词表型语义(原语族 / 算子族 / 事件族 / tracker 族等枚举)
//    ⇒ 那些族类的**唯一真源恒在**平台头 ⇒ 本载体**不自证**其取值域。
// ② MAY 自带**形状型**辅助类型(原语 id / 名字对 / basis 格 / pending 条目)——
//    形状**不承载**语义枚举。 ③ ⛔ **SHALL NOT** 定义 / 暴露区域表类型
//    ⇒ 区域表的**运行时状态归实现层**。
// ④ ⛔ **SHALL NOT** 定义任何 reduce 算子枚举 / 常量 ⇒ 逐算子支持一律 **pending**(锚点 `N-6`)。
// ⑤ 标准库 include 清单:`<array>` / `<cstddef>` / `<cstdint>` / `<string_view>`
//    ⇒ 全部是标准库,**不破**自足红线。
//
// ── 口径(per 裁决链)──────────────────────────────────────────────────────────
// ① ⛔ **两个「恰 4」是不同的事实,勿混读**:
//    (a) **平台原语族** `CollectiveOp` 恰 4 值(REDUCE / BROADCAST / MULTICAST / BARRIER)
//        —— per `ADR-CEP-01 §7 V7` / `ADR-CEP-03 §9 V2`;**不在**本载体断言(平台词表在平台头);
//    (b) **INC 的四个操作**(`ReadReduce` / `WriteMulticast` / `Broadcast` / `AllReduce`)
//        —— 名字来源 = 仓内锚点 `C-6`;**本载体断言的是 (b)**。
//    ⇒ UIS-01 的义务 = (b) **不新增**平台原语:`AllReduce` = REDUCE + BROADCAST 的
//      **复合**,⛔ **不是**第 5 个原语 ⇒ (b) 的覆盖集 ⊆ (a)。
// ② **reduce 算子 = 只引用既有算子族**(恰 6 值)⇒ 载体**不**定义算子族 ⇒ 逐算子支持 **pending**(`N-6`)。
// ③ **partial failure** ⇒ 既有 `Fault` 通道(不得静默)⇒ 载体只给**形状型谓词**,
//    ⛔ **不**新增集合面方法(7 方法面已硬锁)。
// ④ **集合 VC = 继承**该 network 既有实例的映射(**不声明**自己的绑定)
//    ⇒ 继承**良定义**的充要条件 = 该 network 上全部有绑定的既有实例逐类相等
//    ⇒ **不一致 ⇒ 该 network 不得用 M-2**。⇒ 载体只给形状型谓词,实例比对归实现层。
// ⑤ **`kind` ↔ `ref` 类别 SHALL 自洽**(per 本 change design §2.3 的修订)
//    ⇒ `ref` 的 token 按**前缀**分类:`C-` = 仓内已签锚点 · `N-` = pending 白名单
//      · `A-` = 公开来源(其余 = 未知类)。
//    ⇒ `SelfDerived` ⇒ **全部** token 必须是 `C-` 类;
//      `Pending` ⇒ **至少一个** `N-` 类 token · **不得**含 `A-` 类 token ·
//                   其余 token **只允许** `C-` 类(名字来源的**附加**依据)⇒ **未知类判否**。
//    ⇒ 三种冒充**全部**被堵:`SelfDerived` 却引 `N-` / `Pending` 却**纯**引 `C-`
//      (无 `N-` token ⇒ 拿已定稿冒充待定)/ `Pending` 却引 `A-`。
//    ⇒ ⚠️ `C-` token 在 `Pending` 分支**允许**(INC 原语格 = 名字 `C-6` + 语义 `N-1`)
//      ⇒ ⛔ **不得**把结论写成「`Pending` 却引 `C-` ⇒ 判否」(那**强于**实现 ⇒ Phase 2 会假绿)。
//    ⇒ ⚠️ **相对队列 #13 两轴模型的简化**:本载体用「单 `kind` + token 级类别」表达
//      INC 原语格的**混合**证据(名字自持 `C-6` + 语义 pending `N-1`)⇒
//      **鉴别力不低于**两轴(三种冒充均红);若 Phase 2 需表达第三类证据来源 ⇒ 升级为两轴。
// ⑥ ⛔ **不得**把「把集合平面喂进通道依赖闸重跑」当作证书判据 —— 实测**恒真**
//    (边按 network 声明 ⇒ 任一实例的图 = 声明边表的**诱导子图** ⇒ DAG 的子图仍无环)
//    ⇒ 语义由「槽位语义继承」与「控制面互斥」承担(per ADR-CEP-01 SU#4 的 E.4)。
//
// ── 逐格 `basis` 的口径(per 本 change design §2.3)──────────────────────────
// · **格 = 11 个义务格**(`UIS-01` ~ `UIS-11`)+ **4 个 INC 原语格**。
// · **`kind`** = 证据强度:`SelfDerived`(引**仓内已签**锚点 `C-n`)/ `Pending`(引 `N-n`)。
//   ⛔ `kind == SelfDerived` 的格 SHALL **不**以公开来源(`A-n`)为依据 ——
//   公开来源**撑不住**细粒度语义(锚点摘要 §2 的边界声明)⇒ 冒充即为纸面契约。
// · **`ref`** = 锚点摘要的**条目 id**(空格分隔的多个 token)⇒ **每格必有 ref**。
// · **`pending` 白名单** = 载体可逐项报出的 **9** 条(`N-1` ~ `N-9`)
//   ⇒ ⚠️ **「恰 9 项 + 与锚点摘要 §3 逐项相等」由 X-2 元测试 + 人工审计面承担**
//     (Phase 2 · **可执行**:读锚点摘要解析 `N-n` 集合)
//   ⇒ **载体只保证自洽型**(表大小 == `kPendingCount` · id 唯一且落在 `N1`~`N7` · id ↔ ref 一一对应)
//   ⇒ **载体不硬编数量** ⇒ 语义转正时**审计键 id 保持稳定**(可逆性)。
//
// ── ⚠️ 载体自建的**教训**(per 队列 #14 Phase 1 自查,已固化)────────────────
// 禁项清单里的**短 token** 会被**载体自身的标识符**命中 —— 本轮首跑即命中
// `kReduceOperatorFamilyDefinedInCarrier` 含子串 `ReduceOp` ⇒ 已改名
// `kOperatorFamilyDefinedInCarrier`(**保留扫描的严格性**,不做词边界放宽)。
// ⇒ **下一个载体**新增标识符时,须把**自己的**标识符也纳入禁项清单复核。
//
// ── ⚠️ 合规注记(per 锚点摘要 §0)──────────────────────────────────────────
// UALink Common 2.0 的**规范正文**受 **Evaluation Copy Agreement** 约束(实测)
// ⇒ 与本仓 **CXL ECA §3** 禁令**同型** ⇒ **禁止**把规范正文投喂任何 AI
// ⇒ 本载体**零**公开来源支撑的语义格 ⇒ 全部 `SelfDerived` 或 `Pending`。
// =============================================================================

#ifndef TLM_CEP_UALINK_INC_STATE_SPEC_HH
#define TLM_CEP_UALINK_INC_STATE_SPEC_HH

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tlm {
    namespace cep {
        namespace ualink_inc {

            // ═══════════════════════════════════════════════════════════════
            // ① INC 四原语(**自持 id** —— 载体**不**引用平台的原语族类型)
            // ═══════════════════════════════════════════════════════════════
            enum class IncOp : std::uint8_t {
                ReadReduce = 0,    ///< 读 + 网内归约
                WriteMulticast = 1,///< 写 + 网内组播
                Broadcast = 2,      ///< 网内广播
                AllReduce = 3,      ///< 归约 + 广播的**复合**(⛔ **不是**第 5 个原语)
            };

            inline constexpr std::size_t kIncOpCount = 4;

            inline constexpr std::array<IncOp, kIncOpCount> kIncOps{{IncOp::ReadReduce,
                                                                     IncOp::WriteMulticast,
                                                                     IncOp::Broadcast,
                                                                     IncOp::AllReduce}};

            // ═══════════════════════════════════════════════════════════════
            // ② 原语映射(**按名字**)—— 载体**只用** 3 个既有原语名
            // ═══════════════════════════════════════════════════════════════
            // ⚠️ **为什么只列 3 个而不是 4 个**:载体的映射**只用**这三个 ⇒ 它**不是**
            //    平台原语族的**副本**(平台还有第 4 个,本载体未用)⇒ 避免「平台词表副本」。
            //    「映射是否落在平台的原语取值域内」由**跨载体元测试**判(禁双实现)。

            /// 一个原语操作**用到**的既有原语名(最多 2 个 ⇒ `AllReduce` 是复合)
            struct PrimitiveSet {
                std::string_view first;
                std::string_view second; ///< 空串 = 无第二个
            };

            inline constexpr std::array<PrimitiveSet, kIncOpCount> kIncOpPrimitives{{
                {"REDUCE", std::string_view{}},         ///< ReadReduce
                {"MULTICAST", std::string_view{}},      ///< WriteMulticast
                {"BROADCAST", std::string_view{}},      ///< Broadcast
                {"REDUCE", "BROADCAST"},///< AllReduce(复合)
            }};

            /// 载体**用到的**既有原语名(3 个)
            /// ⚠️ **本常量不证明「不是平台词表的副本」** —— 它只陈述「本载体用到的名字
            ///    恰 3 个」这一**事实**;「非副本」需要「载体集 ⊊ 平台集」⇒ **载体不可自证**
            ///    ⇒ 由**跨载体**的 X-1 判(per 本 change design §2.4)。
            inline constexpr std::array<std::string_view, 3> kUsedPrimitiveNames{{"REDUCE", "MULTICAST",
                                                                                  "BROADCAST"}};

            /// 该操作**用到**几个既有原语(复合 ⇒ 2;其余 ⇒ 1)
            inline constexpr std::size_t inc_op_primitive_count(IncOp op) noexcept {
                return (op == IncOp::AllReduce) ? 2u : 1u;
            }

            /// 该操作的第 `index` 个既有原语名(越界 ⇒ 空串)
            inline constexpr std::string_view inc_op_primitive_name(IncOp op,
                                                                     std::size_t index) noexcept {
                const PrimitiveSet& ps = kIncOpPrimitives[static_cast<std::size_t>(op)];
                if (index == 0u)
                    return ps.first;
                if (index == 1u)
                    return ps.second;
                return std::string_view{};
            }

            // ═══════════════════════════════════════════════════════════════
            // ③ 义务格 + 逐格 `basis`
            // ═══════════════════════════════════════════════════════════════
            enum class Obligation : std::uint8_t {
                Uis01 = 0, ///< 原语集恰 4 / 不新增原语
                Uis02 = 1, ///< reduce 算子只引用既有算子族
                Uis03 = 2, ///< partial failure ⇒ 显式 Fault
                Uis04 = 3, ///< ReadReduce 的 coherence 可见性边界(**pending**)
                Uis05 = 4, ///< INC 与 coherence 流的排序 / 原子性(**pending**)
                Uis06 = 5, ///< INC 的控制面归属(**pending**)
                Uis07 = 6, ///< 集合 VC 继承 + 不一致即禁用 M-2
                Uis08 = 7, ///< `ualink_4` 的 `flow_support`(**pending**)
                Uis09 = 8, ///< `ualink_4` 的 `perm` 子集(**pending**)
                Uis10 = 9, ///< basis 完整 + pending 自洽(「逐项相等」归 X-2 元测试)
                Uis11 = 10, ///< 四条证书的规约面(⛔ 不得以「重跑 DAG」为判据)
            };

            inline constexpr std::size_t kObligationCount = 11;

            /// 证据强度 —— ⛔ `SelfDerived` 的格**不得**以公开来源(`A-n`)为依据
            enum class BasisKind : std::uint8_t {
                SelfDerived = 0, ///< 引**仓内已签**锚点(`C-n`)
                Pending = 1,      ///< 引 pending 白名单(`N-n`)
            };

            struct BasisCell {
                Obligation obligation;
                std::string_view ref; ///< 锚点摘要的条目 id(空格分隔)
                BasisKind kind;
            };

            /// 11 个义务格的 basis
            inline constexpr std::array<BasisCell, kObligationCount> kBasisObligations{{
                {Obligation::Uis01, "C-2 C-3", BasisKind::SelfDerived},
                {Obligation::Uis02, "C-1 C-3", BasisKind::SelfDerived},
                {Obligation::Uis03, "C-1", BasisKind::SelfDerived},
                {Obligation::Uis04, "N-3", BasisKind::Pending},
                {Obligation::Uis05, "N-4", BasisKind::Pending},
                {Obligation::Uis06, "N-5", BasisKind::Pending},
                {Obligation::Uis07, "C-1 C-8 C-9", BasisKind::SelfDerived},
                // ⚠️ **2026-11-06 翻转**(per 台账 Entry #238 + `ADR-CEP-04` SU 2026-11-06):
                //   `Uis08` / `Uis09` 由 `Pending` 转 **`SelfDerived`** —— 但 ⛔ **不是**「有规范依据」,
                //   而是「**非规范公开来源**支撑的**建模声明**」⇒ 其 ref **必须**指向**仓内已签锚点** `C-10`
                //   (= `ADR-CEP-04` SU 2026-11-06 的 `ualink_4` 取值裁决,该 SU 内记录 4 个公开来源
                //     + 「一步推断」的限定)⇒ 载体侧**不发明第三种 basis 值**(`BasisKind` 仍只 2 档)。
                {Obligation::Uis08, "C-10", BasisKind::SelfDerived},
                {Obligation::Uis09, "C-10", BasisKind::SelfDerived},
                {Obligation::Uis10, "C-1", BasisKind::SelfDerived},
                {Obligation::Uis11, "C-1", BasisKind::SelfDerived},
            }};

            /// 4 个 INC 原语格的 basis(**名字**来自仓内 `C-6`;**语义**全部 pending `N-1`)
            struct IncOpBasis {
                IncOp op;
                std::string_view ref;
                BasisKind kind;
            };

            inline constexpr std::size_t kIncOpBasisCount = 4;

            inline constexpr std::array<IncOpBasis, kIncOpBasisCount> kBasisIncOps{{
                {IncOp::ReadReduce, "C-6 N-1", BasisKind::Pending},
                {IncOp::WriteMulticast, "C-6 N-1", BasisKind::Pending},
                {IncOp::Broadcast, "C-6 N-1", BasisKind::Pending},
                {IncOp::AllReduce, "C-6 N-1", BasisKind::Pending},
            }};

            // ═══════════════════════════════════════════════════════════════
            // ④ pending 白名单(7 项 —— 与锚点摘要 §3 **逐项相等**;⚠️ 2026-11-06: `N-8` / `N-9` 已转正 ⇒ 9 → 7)
            // ═══════════════════════════════════════════════════════════════
            enum class PendingId : std::uint8_t {
                N1 = 0, ///< INC 四原语的规范定义与语义
                N2 = 1, ///< `WriteMulticast` 的部分失败语义
                N3 = 2, ///< `ReadReduce` 与 coherence 可见性的关系
                N4 = 3, ///< INC 原语与 coherence 流的排序 / 原子性要求
                N5 = 4, ///< INC 的控制面(group / epoch / 成员集合)归属
                N6 = 5, ///< 既有算子族与 UALink 归约算子的对应关系
                N7 = 6, ///< 集合流量在既有 VC 上的占用语义
                // ⚠️ **2026-11-06**:`N8` / `N9`(`ualink_4` 的 `flow_support` / `perm` 子集)**已转正**
                //   ⇒ 由 pending 白名单**移除**(取值见 `ADR-CEP-04` SU 2026-11-06 + 锚点摘要 `C-10`)
                //   ⇒ 白名单 **9 项 → 7 项**(`N-1` ~ `N-7`)。
            };

            inline constexpr std::size_t kPendingCount = 7;

            struct PendingItem {
                PendingId id;
                std::string_view ref;        ///< 锚点摘要的条目 id(与 `id` 一一对应)
                std::string_view statement;  ///< 断言摘要(≤1 句)
            };

            inline constexpr std::array<PendingItem, kPendingCount> kPendingSemanticItems{{
                {PendingId::N1, "N-1", "INC 四原语的规范定义与语义"},
                {PendingId::N2, "N-2", "`WriteMulticast` 的部分失败语义"},
                {PendingId::N3, "N-3", "`ReadReduce` 与 coherence 可见性的关系"},
                {PendingId::N4, "N-4", "INC 原语与 coherence 流的排序 / 原子性要求"},
                {PendingId::N5, "N-5", "INC 的控制面(group / epoch / 成员集合)归属"},
                {PendingId::N6, "N-6", "既有算子族与 UALink 归约算子的对应关系"},
                {PendingId::N7, "N-7", "集合流量在既有 VC 上的占用语义"},
            }};

            // ═══════════════════════════════════════════════════════════════
            // ⑤ 形状型判定谓词(**不承载**语义枚举)
            // ═══════════════════════════════════════════════════════════════
            /// **partial failure 必须显式** —— 输入 = 「是否静默」,输出 = 判定
            /// ⚠️ 本谓词**只**表达规则形状;事件族的**唯一真源恒在**平台头。
            inline constexpr bool partial_failure_must_be_explicit(bool silent) noexcept {
                return !silent;
            }

            /// **VC 继承良定义** —— 输入 = 「该 network 上全部有绑定的既有实例是否逐类相等」
            /// ⚠️ 实例比对归实现层(载体**不**引用平台实例声明)。
            inline constexpr bool vc_inheritance_is_well_defined(bool all_instances_agree) noexcept {
                return all_instances_agree;
            }

            // ═══════════════════════════════════════════════════════════════
            // ⑥ 声明锁与形状型谓词(**可被毒化的「翻转点」** —— 毒化 ⇒ 编译期红)
            //    ⚠️ 翻转点**不止**这三个声明锁 —— **形状型谓词的函数体**同样是翻转点
            //    (per 本 change design §4 的 F-3 / F-7:翻转 `partial_failure_must_be_explicit`
            //    与 `vc_inheritance_is_well_defined` 的**函数体** ⇒ 编译期红)
            // ═══════════════════════════════════════════════════════════════
            /// ⛔ 载体**不**定义 reduce 算子族 ⇒ 恒 `false`
            inline constexpr bool kOperatorFamilyDefinedInCarrier = false;
            /// ⛔ 「重跑通道依赖闸」**不**是证书判据(实测恒真)⇒ 恒 `false`
            inline constexpr bool kDagRerunIsCertificate = false;
            /// ⛔ 载体**不**定义 / 暴露区域表类型 ⇒ 恒 `false`
            inline constexpr bool kRegionTableExposed = false;

            // ═══════════════════════════════════════════════════════════════
            // ⑦ 载体自检(编译期可判 —— 供 `static_assert` 使用)
            // ═══════════════════════════════════════════════════════════════
            constexpr bool sv_contains(std::string_view hay, std::string_view needle) noexcept {
                if (needle.empty())
                    return true;
                if (hay.size() < needle.size())
                    return false;
                for (std::size_t i = 0; i + needle.size() <= hay.size(); ++i) {
                    bool eq = true;
                    for (std::size_t j = 0; j < needle.size(); ++j) {
                        if (hay[i + j] != needle[j]) {
                            eq = false;
                            break;
                        }
                    }
                    if (eq)
                        return true;
                }
                return false;
            }

            /// 该原语名是否在载体用到的名字集合内
            constexpr bool primitive_name_used(std::string_view name) noexcept {
                for (const auto& n : kUsedPrimitiveNames) {
                    if (n == name)
                        return true;
                }
                return false;
            }

            /// 该操作映射到的**全部**原语名是否都在载体的名字集合内(⇒ 不越出)
            constexpr bool inc_op_mapping_within_used_names(IncOp op) noexcept {
                const std::size_t n = inc_op_primitive_count(op);
                for (std::size_t i = 0; i < n; ++i) {
                    if (!primitive_name_used(inc_op_primitive_name(op, i)))
                        return false;
                }
                return true;
            }

            /// 该 basis 格是否有非空 `ref`
            constexpr bool basis_cell_has_ref(std::string_view ref) noexcept { return !ref.empty(); }

            // ── `ref` token 的**来源类别**(per 本 change design §2.3 的修订)─────
            /// token 按**前缀**分类 —— ⛔ **不用子串**(短 token 会误伤,如 `A-` 会命中 `X-A-1`)
            enum class RefClass : std::uint8_t {
                Contract = 0,    ///< `C-` = **仓内已签**锚点(ADR / 仓内实测)
                PendingRef = 1,  ///< `N-` = **pending** 白名单(锚点摘要 §3)
                PublicSource = 2,///< `A-` = **公开来源**(锚点摘要 §2)
                Unknown = 3,     ///< 其余(不可归类 ⇒ 一律判否)
            };

            constexpr RefClass classify_ref_token(std::string_view token) noexcept {
                // ⚠️ 前缀**后须有编号** —— 否则无编号 token(如 `C-`)会被误认作已签锚点
                if (token.size() >= 3 && token[0] == 'C' && token[1] == '-' &&
                    token[2] >= '0' && token[2] <= '9')
                    return RefClass::Contract;
                if (token.size() >= 3 && token[0] == 'N' && token[1] == '-' &&
                    token[2] >= '0' && token[2] <= '9')
                    return RefClass::PendingRef;
                if (token.size() >= 3 && token[0] == 'A' && token[1] == '-' &&
                    token[2] >= '0' && token[2] <= '9')
                    return RefClass::PublicSource;
                return RefClass::Unknown;
            }

            /// `ref` 中**是否至少有一个**属于类别 `c` 的 token
            constexpr bool ref_contains_class(std::string_view ref, RefClass c) noexcept {
                std::size_t i = 0;
                while (i < ref.size()) {
                    while (i < ref.size() && ref[i] == ' ')
                        ++i;
                    std::size_t j = i;
                    while (j < ref.size() && ref[j] != ' ')
                        ++j;
                    if (j > i && classify_ref_token(ref.substr(i, j - i)) == c)
                        return true;
                    i = j + 1;
                }
                return false;
            }

            /// `ref` 的**全部** token 是否都属于类别 `c`(且至少一个 token)
            constexpr bool ref_only_class(std::string_view ref, RefClass c) noexcept {
                std::size_t i = 0;
                bool any = false;
                while (i < ref.size()) {
                    while (i < ref.size() && ref[i] == ' ')
                        ++i;
                    std::size_t j = i;
                    while (j < ref.size() && ref[j] != ' ')
                        ++j;
                    if (j > i) {
                        any = true;
                        if (classify_ref_token(ref.substr(i, j - i)) != c)
                            return false;
                    }
                    i = j + 1;
                }
                return any;
            }

            /// `kind` 与 `ref` 类别**是否自洽**(per design §2.3)
            /// · `SelfDerived` ⇒ **全部** token 必须是 `C-` 类(仓内已签锚点)
            /// · `Pending` ⇒ **至少一个** `N-` 类 token(语义待定的依据)· **不得**含 `A-` 类
            ///   (公开来源不得冒充)· 其余 token **只允许** `C-` 类(名字来源的**附加**依据)
            ///   ⇒ **未知类 token 一律判否**
            /// ⇒ 冒充(`SelfDerived` 却引 `N-` / `Pending` 却**纯**引 `C-`(无 `N-` token)
            ///   / `Pending` 却引 `A-`)**全部**判否。
            /// ⚠️ ⛔ **不得**把结论写成「`Pending` 却引 `C-` ⇒ 判否」—— 那**强于**实现
            ///   (INC 原语格 = `Pending` + `"C-6 N-1"`)⇒ Phase 2 的 fixture 会**假绿**。
            constexpr bool basis_kind_matches_ref_class(BasisKind kind,
                                                        std::string_view ref) noexcept {
                if (kind == BasisKind::SelfDerived)
                    return ref_only_class(ref, RefClass::Contract);
                // `Pending`:至少一个 `N-`(语义待定的依据)· 不得含 `A-`(公开来源不得冒充)
                //           · 其余 token **只允许** `C-`(名字来源的附加依据)⇒ 未知类判否
                return ref_contains_class(ref, RefClass::PendingRef) &&
                       !ref_contains_class(ref, RefClass::PublicSource) &&
                       !ref_contains_class(ref, RefClass::Unknown);
            }

            /// `SelfDerived` 的格**不得**以公开来源(`A-` 类 token)为依据
            constexpr bool self_derived_avoids_public_source(std::string_view ref) noexcept {
                return !ref_contains_class(ref, RefClass::PublicSource);
            }

            /// 该 obligation 的格是否 `pending`
            constexpr bool obligation_is_pending(Obligation ob) noexcept {
                return kBasisObligations[static_cast<std::size_t>(ob)].kind == BasisKind::Pending;
            }

            /// 该 obligation 的格是否 `self-derived`
            constexpr bool obligation_is_self_derived(Obligation ob) noexcept {
                return kBasisObligations[static_cast<std::size_t>(ob)].kind == BasisKind::SelfDerived;
            }

            /// pending 条目的 id 是否**唯一**且**落在** `N1` ~ `N7` 内(2026-11-06:`N-8`/`N-9` 转正 ⇒ 上界 `N7`)
            /// ⚠️ ⛔ **不要求**「id == 数组下标」—— 那会把「一次语义转正」强制变成
            ///    「一次审计键重写」(转正一项 ⇒ 其余全部重编号 ⇒ 破坏与锚点摘要 §3
            ///    / delta spec 的对应)⇒ **可逆性**(per 本 change design §2.3 的修订)。
            /// ⇒ 「恰 7 项 + 与锚点摘要逐项相等」= **X-2 元测试**的义务(Phase 2 · 可执行)
            ///    + 人工审计面 ⇒ **载体不硬编数量**。
            template <std::size_t N>
            constexpr bool pending_ids_unique_and_in_range(const std::array<PendingItem, N>& items) noexcept {
                constexpr std::size_t kPendingIdSpace =
                    static_cast<std::size_t>(PendingId::N7) + 1u;
                for (std::size_t i = 0; i < N; ++i) {
                    const std::size_t id = static_cast<std::size_t>(items[i].id);
                    if (id >= kPendingIdSpace)
                        return false;
                    for (std::size_t j = i + 1; j < N; ++j) {
                        if (items[i].id == items[j].id)
                            return false;
                    }
                }
                return true;
            }

            /// pending 条目的 `ref` 是否与其 id 一一对应(`N1` ↔ `"N-1"` …)
            template <std::size_t N>
            constexpr bool pending_refs_match_ids(const std::array<PendingItem, N>& items) noexcept {
                for (std::size_t i = 0; i < N; ++i) {
                    const PendingItem& it = items[i];
                    const std::size_t n = static_cast<std::size_t>(it.id) + 1u;
                    // 期望 "N-<n>"
                    if (it.ref.size() != 3u)
                        return false;
                    if (it.ref[0] != 'N' || it.ref[1] != '-')
                        return false;
                    if (it.ref[2] != static_cast<char>('0' + static_cast<char>(n)))
                        return false;
                }
                return true;
            }

            /// **全部义务格**的 `kind` ↔ `ref` 类别是否自洽(遍历**真实表格**)
            /// ⚠️ 必须**遍历表格** —— 只断言字面量组合 ⇒ 毒化表格里的 `ref` **不触发**任何断言
            constexpr bool all_obligations_kind_match_ref_class() noexcept {
                for (const auto& c : kBasisObligations) {
                    if (!basis_kind_matches_ref_class(c.kind, c.ref))
                        return false;
                }
                return true;
            }

            /// **全部 INC 原语格**的 `kind` ↔ `ref` 类别是否自洽(遍历**真实表格**)
            constexpr bool all_inc_op_basis_kind_match_ref_class() noexcept {
                for (const auto& c : kBasisIncOps) {
                    if (!basis_kind_matches_ref_class(c.kind, c.ref))
                        return false;
                }
                return true;
            }

            /// 所有 basis 格是否都有非空 `ref`
            constexpr bool all_basis_cells_have_ref() noexcept {
                for (const auto& c : kBasisObligations) {
                    if (!basis_cell_has_ref(c.ref))
                        return false;
                }
                for (const auto& c : kBasisIncOps) {
                    if (!basis_cell_has_ref(c.ref))
                        return false;
                }
                return true;
            }

            /// 所有 `SelfDerived` 的格是否都**不**指向公开来源
            constexpr bool all_self_derived_avoid_public_source() noexcept {
                for (const auto& c : kBasisObligations) {
                    if (c.kind == BasisKind::SelfDerived && !self_derived_avoids_public_source(c.ref))
                        return false;
                }
                for (const auto& c : kBasisIncOps) {
                    if (c.kind == BasisKind::SelfDerived && !self_derived_avoids_public_source(c.ref))
                        return false;
                }
                return true;
            }

            // ═══════════════════════════════════════════════════════════════
            // ⑨ 证书规约面(per ADR-CEP-01 SU#4 的 **E.3** / **E.4** · UIS-11)
            // ═══════════════════════════════════════════════════════════════
            // ⚠️ 本段只给**规约面**(逐边依据 + 参照域 + `IC1` 鉴别力边界 + U-1/U-2 登记)
            //    ⛔ **不**实现判据 —— 判据面属 `cep-collective-plane-contract`(由**实现层** delta 承载)。

            /// 四条证书(per E.3.1) —— ⛔ 四条**共用**系统级归因 `CP-03-VC-CERTIFICATE`
            /// (以 `detail` 子标记区分;per E.3.1 的归因 id 纪律)
            enum class CertificateId : std::uint8_t {
                C1Prime = 0, ///< C-1′ 槽位语义继承
                C2 = 1,      ///< C-2 VC 覆盖完整(系统级归因 = `CP-09-IC1`,per E.3.1)
                C3 = 2,      ///< C-3 控制面与数据面互斥
                C4 = 3,      ///< C-4 未检验项登记(U-1 / U-2)
            };

            inline constexpr std::size_t kCertificateCount = 4;

            /// 参照域的**位掩码**(per E.3.3:C-1′ 的参照集 = **两域并集**)
            inline constexpr std::uint8_t kDomainArchForgeDerived = 1u;
            inline constexpr std::uint8_t kDomainRepoOwned = 2u;

            struct CertificateSpec {
                CertificateId id;
                std::string_view detail_tag; ///< `detail` 子标记(per E.3.1 的 `[CP-03-VC-CERTIFICATE/<tag>]`)
                std::string_view obligation; ///< 义务(≤1 句)
                std::string_view edge_basis; ///< **逐边依据**(引 `C-n`)
                std::uint8_t domains;        ///< **参照域**位掩码
            };

            /// 四条证书的**逐边依据 + 参照域**(顺序**稳定** ⇒ 断言可逐条核对)
            inline constexpr std::array<CertificateSpec, kCertificateCount> kCertificateSpecs{{
                {CertificateId::C1Prime, "/C-1′", "槽位语义继承:集合平面不声明自己的 VC 绑定",
                 "C-1 C-8 C-9", kDomainArchForgeDerived | kDomainRepoOwned},
                {CertificateId::C2, "/C-2", "VC 覆盖完整:覆盖集 ⊇ 该网 vc_count 全部取值", "C-1",
                 kDomainRepoOwned},
                {CertificateId::C3, "/C-3", "控制面与数据面互斥:op in-flight 时拒绝 register / destroy",
                 "C-1", kDomainRepoOwned},
                {CertificateId::C4, "/C-4", "未检验项登记:U-1 / U-2", "C-1", kDomainRepoOwned},
            }};

            /// 每行证书是否**齐备**(detail 子标记 / 义务 / 逐边依据 均非空)
            constexpr bool all_certificate_specs_wellformed() noexcept {
                for (const auto& c : kCertificateSpecs) {
                    if (c.detail_tag.empty() || c.obligation.empty() || c.edge_basis.empty())
                        return false;
                    if (c.domains == 0u)
                        return false;
                }
                return true;
            }

            // ── C-1′ 的**可判定形式**:映射不一致 ⇒ 该网**不得**用 M-2(per E.3.3)───
            /// 输入 = 「该 network 上**全部有绑定的**既有实例的映射是否**逐类相等**」
            /// ⇒ `false` ⇒ 该网**不得**使用 M-2(须回 M-1 或先消除不一致)
            /// ⚠️ 实例比对归**实现层**(载体**不**引用平台实例声明;per E.3.2 的「零新声明组」)
            inline constexpr bool m2_usable_on_network(bool all_bound_instances_agree) noexcept {
                return vc_inheritance_is_well_defined(all_bound_instances_agree);
            }

            // ── `IC1` 的**鉴别力边界**(per ADR-CEP-09 SU 2026-11-03 ③ + E.4)──────
            struct Ic1Discrimination {
                bool edge_endpoints_bound;    ///< ① 边端点良构(声明边表引用的每个 `vc_id` 都被绑定)
                bool vc_id_in_range;          ///< ② `vc_id` 越界(per `kMaxVcId = 3`)
                bool declared_edge_table_dag; ///< ③ 声明边表**自身**无环(前提 P1)
                bool vc_binding_semantics;    ///< ⛔ `vc_binding` 的**语义分配** —— **零鉴别力**
            };

            /// ⛔ `vc_binding_semantics` 恒 `false`(实测:语义旋转仍 PASS)
            inline constexpr Ic1Discrimination kIc1Discrimination{true, true, true, false};

            // ── C-4 的两项**未检验项**(per E.3.4)────────────────────────────────
            // ⚠️ 与 `N-n` 的**区别**:`N-n` = 语义**待定**(等依据到位);`U-n` = 语义已定但**不可检验**
            //    ⇒ 二者**不得**混用:`U-n` **不得**写成 `N-n`,反之亦然。
            enum class UntestedId : std::uint8_t {
                U1 = 0, ///< 「VC 隔离确实避免协议级死锁」(H6) —— M-2 下**永远无法**检验
                U2 = 1, ///< 同 VC 内 coherence ↔ collective 的**相互阻塞** —— VC 级依赖图**不表达**
            };

            inline constexpr std::size_t kUntestedCount = 2;

            struct UntestedItem {
                UntestedId id;
                std::string_view ref;
                std::string_view statement;
            };

            inline constexpr std::array<UntestedItem, kUntestedCount> kUntestedItems{{
                {UntestedId::U1, "U-1", "「VC 隔离确实避免协议级死锁」(H6)"},
                {UntestedId::U2, "U-2", "同 VC 内 coherence 与 collective 的相互阻塞不被 VC 级依赖图表达"},
            }};

            /// ⛔ 未检验项 SHALL **不**含「已验证 / 已满足」字样(per E.3.1 的 C-4 反向 fixture)
            constexpr bool untested_is_not_satisfied(std::string_view text) noexcept {
                return !sv_contains(text, "已验证") && !sv_contains(text, "已满足");
            }

            /// 全部未检验项是否都**未被**冒充成「已验证 / 已满足」
            constexpr bool all_untested_not_satisfied() noexcept {
                for (const auto& u : kUntestedItems) {
                    if (!untested_is_not_satisfied(u.statement))
                        return false;
                }
                return true;
            }

            // ═══════════════════════════════════════════════════════════════
            // ⑧ 编译期断言(**每条带归因 id** —— 归因 id 的唯一登记处 = 本 change 的 `design §5`)
            // ═══════════════════════════════════════════════════════════════
            static_assert(kIncOpCount == 4u, "UIS-01: INC 原语 SHALL 恰为 4 个");
            static_assert(inc_op_primitive_count(IncOp::AllReduce) == 2u,
                          "UIS-01: AllReduce SHALL 映射到 2 个既有原语(REDUCE + BROADCAST)");
            static_assert(inc_op_primitive_count(IncOp::ReadReduce) == 1u &&
                              inc_op_primitive_count(IncOp::WriteMulticast) == 1u &&
                              inc_op_primitive_count(IncOp::Broadcast) == 1u,
                          "UIS-01: 其余 3 个原语 SHALL 各映射到 1 个既有原语");
            static_assert(inc_op_primitive_name(IncOp::AllReduce, 0u) == "REDUCE" &&
                              inc_op_primitive_name(IncOp::AllReduce, 1u) == "BROADCAST",
                          "UIS-01: AllReduce = REDUCE + BROADCAST 的复合");
            static_assert(kUsedPrimitiveNames.size() == 3u,
                          "UIS-01: 载体 SHALL 只用到 3 个既有原语名 ⇒ 不是平台词表的副本");
            static_assert(inc_op_mapping_within_used_names(IncOp::ReadReduce) &&
                              inc_op_mapping_within_used_names(IncOp::WriteMulticast) &&
                              inc_op_mapping_within_used_names(IncOp::Broadcast) &&
                              inc_op_mapping_within_used_names(IncOp::AllReduce),
                          "UIS-01: 映射 SHALL 不越出载体用到的原语名集合");

            static_assert(kOperatorFamilyDefinedInCarrier == false,
                          "UIS-02: 载体 SHALL NOT 定义 reduce 算子族(唯一真源在平台头)");

            static_assert(partial_failure_must_be_explicit(false) &&
                              !partial_failure_must_be_explicit(true),
                          "UIS-03: partial failure SHALL 显式(静默 ⇒ 判否)");

            // ⚠️ **逐格断言**(⛔ **不得**合并成一条分组断言)——
            //    理由:分组文案(如 `UIS-04/05/06/08/09:`)**不含** `UIS-05` 等子串
            //    ⇒ 毒化某格时编译日志**无法精确命中**该格的归因 id
            //    ⇒ Phase 2 的反向 fixture「归因 id 精确相等」对该格**不可能通过**。
            //    ⇒ 每条断言 = **一个**归因 id,文案以 `<id>: ` 开头(照队列 #13 载体形态)。
            static_assert(obligation_is_pending(Obligation::Uis04),
                          "UIS-04: ReadReduce 的 coherence 可见性边界 SHALL 保持 pending(公开来源撑不住)");
            static_assert(obligation_is_pending(Obligation::Uis05),
                          "UIS-05: INC 与 coherence 流的排序 / 原子性 SHALL 保持 pending(公开来源撑不住)");
            static_assert(obligation_is_pending(Obligation::Uis06),
                          "UIS-06: INC 的控制面归属 SHALL 保持 pending(公开来源撑不住)");
            // ⚠️ **2026-11-06 翻转**(per `ADR-CEP-04` SU 2026-11-06):`Uis08` / `Uis09` **不再** pending
            //   —— 取值由「非规范公开来源(4 个独立来源)+ 显式一步推断」支撑 ⇒ 作为**建模声明**
            //   锚到仓内已签锚点 `C-10` ⇒ `SelfDerived`。
            //   ⛔ **不得**据此推断「有规范依据」⇒ `C-10` 的正文必须自带该限定(per 锚点摘要 `C-10`)。
            static_assert(obligation_is_self_derived(Obligation::Uis08),
                          "UIS-08: ualink_4 的 flow_support SHALL 引仓内已签锚点 C-10(非规范来源支撑的建模声明)");
            static_assert(obligation_is_self_derived(Obligation::Uis09),
                          "UIS-09: ualink_4 的 perm 支持子集 SHALL 引仓内已签锚点 C-10(建模声明)");
            static_assert(obligation_is_self_derived(Obligation::Uis01),
                          "UIS-01: 原语集义务 SHALL 引仓内已签锚点");
            static_assert(obligation_is_self_derived(Obligation::Uis02),
                          "UIS-02: reduce 算子义务 SHALL 引仓内已签锚点");
            static_assert(obligation_is_self_derived(Obligation::Uis03),
                          "UIS-03: partial failure 义务 SHALL 引仓内已签锚点");
            static_assert(obligation_is_self_derived(Obligation::Uis07),
                          "UIS-07: 集合 VC 继承义务 SHALL 引仓内已签锚点");
            static_assert(obligation_is_self_derived(Obligation::Uis10),
                          "UIS-10: basis 完备性义务 SHALL 引仓内已签锚点");
            static_assert(obligation_is_self_derived(Obligation::Uis11),
                          "UIS-11: 证书规约面义务 SHALL 引仓内已签锚点");

            static_assert(vc_inheritance_is_well_defined(true) &&
                              !vc_inheritance_is_well_defined(false),
                          "UIS-07: 继承良定义 SHALL 要求该 network 上既有实例逐类相等");

            // ⚠️ 「pending 白名单**恰 9** 项 + 与锚点摘要**逐项相等**」**不在**此处断言
            //    ⇒ 它是 **X-2 元测试**的义务(Phase 2 · 可执行:读锚点摘要解析 `N-n` 集合)
            //    + 人工审计面 ⇒ 载体**不硬编数量**(保可逆性,per design §2.3 的修订)。
            static_assert(kPendingSemanticItems.size() == kPendingCount,
                          "UIS-10: pending 表的大小 SHALL == kPendingCount");
            static_assert(pending_ids_unique_and_in_range(kPendingSemanticItems),
                          "UIS-10: pending 的 id SHALL 唯一且落在 N1 ~ N7 内");
            static_assert(pending_refs_match_ids(kPendingSemanticItems),
                          "UIS-10: pending 的 ref SHALL 与其 id 一一对应");
            static_assert(all_basis_cells_have_ref(), "UIS-10: 每个 basis 格 SHALL 有非空 ref");
            static_assert(all_self_derived_avoid_public_source(),
                          "UIS-10: self-derived 的格 SHALL NOT 以公开来源(A-)为依据");

            // ── `kind` ↔ `ref` 类别**遍历式**自洽(S1)────────────────────────
            //    ⚠️ 这两条**遍历真实表格** ⇒ 毒化任一格的 `kind` / `ref` **必红**
            static_assert(all_obligations_kind_match_ref_class(),
                          "UIS-10: 义务格的 kind SHALL 与其 ref 类别自洽(遍历表)");
            static_assert(all_inc_op_basis_kind_match_ref_class(),
                          "UIS-10: INC 原语格的 kind SHALL 与其 ref 类别自洽(遍历表)");
            // ── 判定函数本身的**样例**(证明函数有牙:三种冒充全被判否)──────────
            static_assert(basis_kind_matches_ref_class(BasisKind::SelfDerived, "C-2 C-3") &&
                              basis_kind_matches_ref_class(BasisKind::SelfDerived, "C-1 C-3") &&
                              basis_kind_matches_ref_class(BasisKind::SelfDerived, "C-1") &&
                              basis_kind_matches_ref_class(BasisKind::SelfDerived, "C-1 C-8 C-9") &&
                              basis_kind_matches_ref_class(BasisKind::Pending, "N-3") &&
                              basis_kind_matches_ref_class(BasisKind::Pending, "N-4") &&
                              basis_kind_matches_ref_class(BasisKind::Pending, "N-5"),
                          "UIS-10: 义务格的 kind SHALL 与其 ref 类别自洽");
            static_assert(basis_kind_matches_ref_class(BasisKind::Pending, "C-6 N-1"),
                          "UIS-10: INC 原语格 SHALL 同时引名字锚点(C-)与语义 pending(N-)");
            // ① `SelfDerived` 却引 pending 锚点 ⇒ 判否
            static_assert(!basis_kind_matches_ref_class(BasisKind::SelfDerived, "N-3"),
                          "UIS-10: self-derived 的格 SHALL NOT 引 pending 锚点(N-)");
            // ② `Pending` 却引公开来源 ⇒ 判否
            static_assert(!basis_kind_matches_ref_class(BasisKind::Pending, "A-1"),
                          "UIS-10: pending 的格 SHALL NOT 以公开来源(A-)为依据");
            // ③ `Pending` 却引仓内锚点(冒充 pending 的理由)⇒ 判否
            static_assert(!basis_kind_matches_ref_class(BasisKind::Pending, "C-9"),
                          "UIS-10: pending 的格 SHALL 至少引一个 pending 白名单(N-)");
            static_assert(kBasisIncOps.size() == kIncOpBasisCount,
                          "UIS-10: INC 原语格数 SHALL 与原语数相等");

            static_assert(kDagRerunIsCertificate == false,
                          "UIS-11: 「重跑通道依赖闸」SHALL NOT 作为证书判据(实测恒真)");
            // ── UIS-11:证书规约面(per E.3 / E.4)──────────────────────────────
            static_assert(kCertificateCount == 4u,
                          "UIS-11: 证书 SHALL 恰 4 条(C-1′ / C-2 / C-3 / C-4)");
            static_assert(kCertificateSpecs[0].id == CertificateId::C1Prime &&
                              kCertificateSpecs[1].id == CertificateId::C2 &&
                              kCertificateSpecs[2].id == CertificateId::C3 &&
                              kCertificateSpecs[3].id == CertificateId::C4,
                          "UIS-11: 证书表 SHALL 逐条齐备且顺序稳定");
            static_assert(all_certificate_specs_wellformed(),
                          "UIS-11: 每行证书 SHALL 有 detail 子标记 / 义务 / 逐边依据 / 非空参照域");
            static_assert(kCertificateSpecs[0].domains == (kDomainArchForgeDerived | kDomainRepoOwned),
                          "UIS-11: C-1′ 的参照域 SHALL 为**两域并集**(per E.3.3)");
            static_assert(m2_usable_on_network(true) && !m2_usable_on_network(false),
                          "UIS-11: 映射不一致 ⇒ 该网 SHALL 不得使用 M-2(per E.3.3)");
            static_assert(kIc1Discrimination.edge_endpoints_bound &&
                              kIc1Discrimination.vc_id_in_range &&
                              kIc1Discrimination.declared_edge_table_dag,
                          "UIS-11: IC1 的鉴别力 SHALL 恰为三项(边端点良构 / vc_id 越界 / 边表无环)");
            static_assert(!kIc1Discrimination.vc_binding_semantics,
                          "UIS-11: IC1 对 vc_binding 的语义分配 SHALL 零鉴别力(实测恒真)");
            static_assert(kUntestedCount == 2u,
                          "UIS-11: 未检验项 SHALL 恰 2 项(U-1 / U-2,per E.3.4)");
            static_assert(all_untested_not_satisfied(),
                          "UIS-11: 未检验项 SHALL NOT 含「已验证 / 已满足」字样");

            static_assert(kBasisObligations[static_cast<std::size_t>(Obligation::Uis11)].ref ==
                              "C-1",
                          "UIS-11: 证书规约面义务格 SHALL 有 basis(ref = C-1)");
            static_assert(kRegionTableExposed == false,
                          "UIS-10: 载体 SHALL NOT 暴露区域表类型");

        } // namespace ualink_inc
    }     // namespace cep
} // namespace tlm

#endif // TLM_CEP_UALINK_INC_STATE_SPEC_HH
