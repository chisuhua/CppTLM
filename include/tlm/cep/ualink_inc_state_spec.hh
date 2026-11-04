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
// · **逐格 `basis`** · **9 项 pending 登记** · 形状型判定谓词
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
// ① **原语最小集 = 恰 4**(per ADR-CEP-01:94 / ADR-CEP-03:75)⇒ INC 的四个操作
//    (`ReadReduce` / `WriteMulticast` / `Broadcast` / `AllReduce`)**不新增**原语:
//    `AllReduce` = REDUCE + BROADCAST 的**复合**,⛔ **不是**第 5 个原语。
// ② **reduce 算子 = 只引用既有算子族**(恰 6 值)⇒ 载体**不**定义算子族 ⇒ 逐算子支持 **pending**(`N-6`)。
// ③ **partial failure** ⇒ 既有 `Fault` 通道(不得静默)⇒ 载体只给**形状型谓词**,
//    ⛔ **不**新增集合面方法(7 方法面已硬锁)。
// ④ **集合 VC = 继承**该 network 既有实例的映射(**不声明**自己的绑定)
//    ⇒ 继承**良定义**的充要条件 = 该 network 上全部有绑定的既有实例逐类相等
//    ⇒ **不一致 ⇒ 该 network 不得用 M-2**。⇒ 载体只给形状型谓词,实例比对归实现层。
// ⑤ ⛔ **不得**把「把集合平面喂进通道依赖闸重跑」当作证书判据 —— 实测**恒真**
//    (边按 network 声明 ⇒ 任一实例的图 = 声明边表的**诱导子图** ⇒ DAG 的子图仍无环)
//    ⇒ 语义由「槽位语义继承」与「控制面互斥」承担(per ADR-CEP-01 SU#4 的 E.4)。
//
// ── 逐格 `basis` 的口径(per 本 change design §2.3)──────────────────────────
// · **格 = 10 个义务格**(`UIS-01` ~ `UIS-10`)+ **4 个 INC 原语格**。
// · **`kind`** = 证据强度:`SelfDerived`(引**仓内已签**锚点 `C-n`)/ `Pending`(引 `N-n`)。
//   ⛔ `kind == SelfDerived` 的格 SHALL **不**以公开来源(`A-n`)为依据 ——
//   公开来源**撑不住**细粒度语义(锚点摘要 §2 的边界声明)⇒ 冒充即为纸面契约。
// · **`ref`** = 锚点摘要的**条目 id**(空格分隔的多个 token)⇒ **每格必有 ref**。
// · **`pending` 白名单** = 载体可逐项报出的 **9** 条(`N-1` ~ `N-9`)
//   ⇒ **不增不减**(与锚点摘要 §3 **逐项相等**)。
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

            /// 载体**用到的**既有原语名(3 个;⚠️ **不是**平台原语族的副本)
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
                Uis10 = 9, ///< basis 完整 + pending 白名单逐项相等(元判据)
            };

            inline constexpr std::size_t kObligationCount = 10;

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

            /// 10 个义务格的 basis
            inline constexpr std::array<BasisCell, kObligationCount> kBasisObligations{{
                {Obligation::Uis01, "C-2 C-3", BasisKind::SelfDerived},
                {Obligation::Uis02, "C-1 C-3", BasisKind::SelfDerived},
                {Obligation::Uis03, "C-1", BasisKind::SelfDerived},
                {Obligation::Uis04, "N-3", BasisKind::Pending},
                {Obligation::Uis05, "N-4", BasisKind::Pending},
                {Obligation::Uis06, "N-5", BasisKind::Pending},
                {Obligation::Uis07, "C-1 C-8 C-9", BasisKind::SelfDerived},
                {Obligation::Uis08, "N-8", BasisKind::Pending},
                {Obligation::Uis09, "N-9", BasisKind::Pending},
                {Obligation::Uis10, "C-1", BasisKind::SelfDerived},
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
            // ④ pending 白名单(9 项 —— 与锚点摘要 §3 **逐项相等**)
            // ═══════════════════════════════════════════════════════════════
            enum class PendingId : std::uint8_t {
                N1 = 0, ///< INC 四原语的规范定义与语义
                N2 = 1, ///< `WriteMulticast` 的部分失败语义
                N3 = 2, ///< `ReadReduce` 与 coherence 可见性的关系
                N4 = 3, ///< INC 原语与 coherence 流的排序 / 原子性要求
                N5 = 4, ///< INC 的控制面(group / epoch / 成员集合)归属
                N6 = 5, ///< 既有算子族与 UALink 归约算子的对应关系
                N7 = 6, ///< 集合流量在既有 VC 上的占用语义
                N8 = 7, ///< `ualink_4` 的 `flow_support`(snoop 类流量有无)
                N9 = 8, ///< `ualink_4` 的 `perm` 支持子集
            };

            inline constexpr std::size_t kPendingCount = 9;

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
                {PendingId::N8, "N-8", "`ualink_4` 的 `flow_support`(snoop 类流量有无)"},
                {PendingId::N9, "N-9", "`ualink_4` 的 `perm` 支持子集"},
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
            // ⑥ 声明锁(可被毒化的「翻转点」—— 毒化 ⇒ 编译期红)
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

            /// `SelfDerived` 的格**不得**以公开来源(`A-`)为依据
            constexpr bool self_derived_avoids_public_source(std::string_view ref) noexcept {
                return !sv_contains(ref, "A-");
            }

            /// 该 obligation 的格是否 `pending`
            constexpr bool obligation_is_pending(Obligation ob) noexcept {
                return kBasisObligations[static_cast<std::size_t>(ob)].kind == BasisKind::Pending;
            }

            /// 该 obligation 的格是否 `self-derived`
            constexpr bool obligation_is_self_derived(Obligation ob) noexcept {
                return kBasisObligations[static_cast<std::size_t>(ob)].kind == BasisKind::SelfDerived;
            }

            /// pending 条目的 id 是否**恰好**为 `N1` ~ `N9`(不多不少)
            constexpr bool pending_ids_exact() noexcept {
                for (std::size_t i = 0; i < kPendingCount; ++i) {
                    if (static_cast<std::size_t>(kPendingSemanticItems[i].id) != i)
                        return false;
                }
                return true;
            }

            /// pending 条目的 `ref` 是否与其 id 一一对应(`N1` ↔ `"N-1"` …)
            constexpr bool pending_refs_match_ids() noexcept {
                for (std::size_t i = 0; i < kPendingCount; ++i) {
                    const PendingItem& it = kPendingSemanticItems[i];
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

            static_assert(obligation_is_pending(Obligation::Uis04) &&
                              obligation_is_pending(Obligation::Uis05) &&
                              obligation_is_pending(Obligation::Uis06) &&
                              obligation_is_pending(Obligation::Uis08) &&
                              obligation_is_pending(Obligation::Uis09),
                          "UIS-04/05/06/08/09: 这 5 格 SHALL 为 pending(公开来源撑不住)");
            static_assert(obligation_is_self_derived(Obligation::Uis01) &&
                              obligation_is_self_derived(Obligation::Uis02) &&
                              obligation_is_self_derived(Obligation::Uis03) &&
                              obligation_is_self_derived(Obligation::Uis07) &&
                              obligation_is_self_derived(Obligation::Uis10),
                          "UIS-01/02/03/07/10: 这 5 格 SHALL 为 self-derived(引仓内已签锚点)");

            static_assert(vc_inheritance_is_well_defined(true) &&
                              !vc_inheritance_is_well_defined(false),
                          "UIS-07: 继承良定义 SHALL 要求该 network 上既有实例逐类相等");

            static_assert(kPendingCount == 9u, "UIS-10: pending 白名单 SHALL 恰为 9 项");
            static_assert(pending_ids_exact(), "UIS-10: pending 的 id SHALL 恰为 N1 ~ N9");
            static_assert(pending_refs_match_ids(),
                          "UIS-10: pending 的 ref SHALL 与其 id 一一对应");
            static_assert(all_basis_cells_have_ref(), "UIS-10: 每个 basis 格 SHALL 有非空 ref");
            static_assert(all_self_derived_avoid_public_source(),
                          "UIS-10: self-derived 的格 SHALL NOT 以公开来源(A-)为依据");
            static_assert(kBasisIncOps.size() == kIncOpBasisCount,
                          "UIS-10: INC 原语格数 SHALL 与原语数相等");

            static_assert(kDagRerunIsCertificate == false,
                          "UIS-11: 「重跑通道依赖闸」SHALL NOT 作为证书判据(实测恒真)");
            static_assert(kRegionTableExposed == false,
                          "UIS-10: 载体 SHALL NOT 暴露区域表类型");

        } // namespace ualink_inc
    }     // namespace cep
} // namespace tlm

#endif // TLM_CEP_UALINK_INC_STATE_SPEC_HH
