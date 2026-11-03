// =============================================================================
// include/tlm/cep/nvlink_sharp_state_spec.hh
//   —— NVLink SHARP 规约载体(规约层 · header-only · 自足)
//
// 作者: CppTLM Team / 日期: 2026-11-03
// 配套: docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md(basis 唯一人工审计面)
//       openspec/changes/cpptlm-cep-nvlink-sharp-state-spec/(change 4 件套)
//       台账 obj-cep-platform.md Entry #173(Task 0)· #176(①/⑤)· #177(③/④)· #178 / #179(critic)
//       docs/adr/ADR-CEP-03-... §Status Update(2026-11-03)(Decision D 之外的载体裁决 F.1 / F.2 /
//       K.2)
// ── 定位 ──────────────────────────────────────────────────────────────────
// **不是缓存、不是网络组件**:它不接 `connections[]`、不进 `ModuleFactory`、不注册 ABI。
// 它是 **NVLink SHARP 语义的规约真源**:分类判据(R1/R2)· 区域与 epoch 纪律(R3/R4)·
// 可观测性(R5)· 跨平面 ordering A/B/C/D · 集合 VC 预留单一状态 · 能力域
// ⇒ 供第 1 个 change 的 `NvlinkAdapter` **零自有表副本**地委托消费。
// ── ⛔ 自足红线(per ADR-CEP-03 SU 2026-11-03(#2) F.1)────────────────────────
// ① **只 include 标准库**;⛔ **SHALL NOT** include `collective_plane.hh`
//    ⇒ 故本载体**不引用**平台词表型语义(`ReduceOp` / `CollectiveOp` / `AccessMode` 等枚举)
//    ⇒ 算子族与原语族的**唯一真源恒为** `tlm::cep::ReduceOp` / `tlm::cep::CollectiveOp`
//    ⇒ 「恰 6 值」的运行期断言**归实现层**,本载体**不自证**。
// ② MAY 自带**形状型**辅助类型(`GroupId` / `AddressRange`)—— 形状**不承载**语义枚举。
// ③ ⛔ **SHALL NOT** 定义 / 暴露 `collective_region_table` 类型 ⇒ 区域表的**运行时状态归实现层**
//    ⇒ 载体只提供**纯函数形态**的判定(输入 = 「是否命中」/「epoch 是否有效」/「drain 是否成功」
//    这类**已判定的事实**,输出 = 判定结果),状态容器由调用方持有(per SU(#3) K.2)。
// ④ ⛔ **SHALL NOT** 定义任何 reduce 算子枚举 / 常量 ⇒ 逐算子支持一律 **pending**(锚点 `N-1`)。
// ── 口径(per 裁决链)───────────────────────────────────────────────────────
// ① **R1/R2**:opcode **永不作分类器**;**唯一分类点 = fabric ingress**
//    ⇒ 本载体的分类函数**签名里没有 opcode、也没有区域表** ⇒ 源端**无法**用它分类。
// ② **R3**:注册 = **登记 + drain + 建 epoch** 三步原子;drain 失败 ⇒ 注册失败且**不建 epoch**。
// ③ **R4**:epoch **未 commit 前 = 正常服务(放行)**;commit 后命中 ⇒ divert。
//    ⚠️ ⛔ **不得**写成「未 commit ⇒ 拒绝」—— 与 R4 矛盾(本仓已错过一次,per Entry #169 ②-⑨)。
// ④ **R5**:divert 计数器(形状)+ **fail-stop** 断言(命中 = error,非 warning)。
// ⑤ **ordering A/B/C/D**:四类**可判定**谓词(per ADR-CEP-03 §3.3);**code 相等**时顺序未定义。
// ⑥ **VC 预留**:载体**仅**表达 `Unavailable` **单一取值**(类型上**无** `vc_id`、**无**四字段)
//    ⇒ **四字段权威恒在** `collective_planes.json` + `CP-03`(Decision D)。
// ⑦ **能力域** = **节点内**(intra-node;A-1 / A-5)⇒ 载体只允许 `IntraNode`。
//    ⚠️ 本条约束**能力域**,**不**约束**流量落点**(落点见 ADR-CEP-01 SU(#2) G.1)。
// ── 阶段 ──────────────────────────────────────────────────────────────────
// **Phase 1(本文件当前内容)**:结构层 —— 类型 / 纯函数 / 常量表 / **编译期断言**。
// **Phase 2(待补)**:逐格 `basis`(kind / pending / ref)+ 锚点摘要逐格核对表 + 完整编译期锁。
// ── ⛔ 合规禁令 ───────────────────────────────────────────────────────────
// NVIDIA NVLink SHARP 正式文档受 NDA 约束(照 CXL 的 ECA §3 + AI AUP 同款)⇒
// **禁止**把规范正文投喂任何 AI ⇒ 本载体**零**规范正文引用,零 NDA 材料引用。
// =============================================================================
#ifndef TLM_CEP_NVLINK_SHARP_STATE_SPEC_HH
#define TLM_CEP_NVLINK_SHARP_STATE_SPEC_HH

#include <cstddef>
#include <cstdint>

namespace tlm {
    namespace cep {
        namespace nvlink_sharp {

            // ═════════════════════════════════════════════
            //  ⓪ 形状型辅助类型(**不承载**语义枚举)
            // ═════════════════════════════════════════════

            /// **形状型**:group 标识 —— ⚠️ 平台真源的 `tlm::cep::group_id_t` 在
            /// `collective_plane.hh`, 而本载体⛔ **不得** include 它 ⇒
            /// 此处是**形状副本**(不承载任何语义)⇒ 消费方负责转换。
            using GroupId = std::uint32_t;

            /// **形状型**:地址区间 —— 同上,**不含**任何语义枚举。
            struct AddressRange {
                std::uint64_t lo = 0;
                std::uint64_t hi = 0;
                constexpr bool valid() const noexcept {
                    return lo <= hi;
                }
            };

            // ═════════════════════════════════════════════
            //  ① R1/R2 分类判据(**唯一分类点 = fabric ingress**)
            // ═════════════════════════════════════════════

            /// **分类结果**:fabric ingress 处的分流决策。
            enum class IngressDecision : std::uint8_t {
                DiverT = 0, ///< 命中已注册区域 **且** epoch 已 commit ⇒ 转 collective 平面
                Pass = 1    ///< 其余 ⇒ 走 coherence `request` 流(**正常服务**)
            };

            inline constexpr std::size_t kIngressDecisionCount = 2;

            /// **R1/R2 的唯一分类点** —— 输入是**已判定的事实**(调用方持有状态),输出是判定结果。
            ///
            /// ⚠️ **签名里刻意没有**:opcode(R1:opcode **永不作分类器**)·
            /// 区域表(R1/S4:源端**不得**持有 区域表可见性)· 任何 group / 地址上下文(⇒
            /// 判定与「源端是谁」无关)。
            constexpr IngressDecision classify_at_fabric_ingress(bool region_hit,
                                                                 bool epoch_committed) noexcept {
                return (region_hit && epoch_committed) ? IngressDecision::DiverT
                                                       : IngressDecision::Pass;
            }

            // ═════════════════════════════════════════════
            //  ② R3/R4 区域与 epoch 纪律
            // ═════════════════════════════════════════════

            /// **注册结果**(R3 三步原子的可判定出口)。
            enum class RegisterResult : std::uint8_t {
                Ok = 0,         ///< 登记 + drain + 建 epoch **全部成功**
                DrainFailed = 1 ///< drain 失败 ⇒ 注册失败且**不建 epoch**(per R3)
            };

            inline constexpr std::size_t kRegisterResultCount = 2;

            /// **epoch 状态**(平台侧纪律,per R4)—— ⚠️ 仅**平台侧**两态;
            /// 协议侧 epoch 状态机细节为 **pending**(锚点 `N-3`)⇒ 本载体**不得**扩展此枚举。
            enum class EpochState : std::uint8_t {
                Uncommitted = 0, ///< epoch 未 commit ⇒ 该区域**正常服务(放行)**
                Committed = 1    ///< epoch 已 commit ⇒ 命中即 divert
            };

            inline constexpr std::size_t kEpochStateCount = 2;

            /// **R3 的三步原子** —— drain 是否成功由调用方给出(状态归实现层)。
            constexpr RegisterResult register_region(bool drain_succeeded) noexcept {
                return drain_succeeded ? RegisterResult::Ok : RegisterResult::DrainFailed;
            }

            /// ⛔ **R4 的唯一真源**:epoch **未 commit ⇒ 放行**(正常服务)· commit ⇒ divert。
            /// ⚠️ **不得**反向(「未 commit ⇒ 拒绝」与 R4 矛盾 —— 本仓已错过一次)。
            constexpr bool epoch_routes_to_collective(EpochState state) noexcept {
                return state == EpochState::Committed;
            }

            /// **R4 的注销**:区域**退出**表 ⇒ 后续 coherence 请求**正常服务** ⇒ 恒 `Pass`。
            /// (实现层负责把该区域从表里摘除 ⇒ `region_hit` 变 false ⇒ 同
            /// `classify_at_fabric_ingress(false, …)`)
            constexpr IngressDecision decision_after_destroy() noexcept {
                return IngressDecision::Pass;
            }

            // ═════════════════════════════════════════════
            //  ③ R5 可观测性(计数器**形状** + fail-stop 判据)
            // ═════════════════════════════════════════════

            /// **R5 的计数器形状** —— ⚠️ 载体**不持有**计数器状态(状态归实现层,按 group /
            /// 区域可寻址)。
            struct DivertCounter {
                std::uint64_t total = 0;
            };

            /// **R5 的 fail-stop 判据** —— epoch commit 后仍有 coherence atomic 命中已注册区域
            /// ⇒ **error**(⛔ 不是 warning、不是静默计数)。
            constexpr bool is_fail_stop(bool region_hit, bool epoch_committed,
                                        bool coherence_atomic_hit) noexcept {
                return region_hit && epoch_committed && coherence_atomic_hit;
            }

            // ═════════════════════════════════════════════
            //  ④ 跨平面 ordering A/B/C/D(**可判定**谓词)
            // ═════════════════════════════════════════════

            /// **跨平面 ordering 的 4 类**(per `ADR-CEP-03 §3.3` A/B/C/D)。
            enum class OrderingRule : std::uint8_t { A = 0, B = 1, C = 2, D = 3 };

            inline constexpr std::size_t kOrderingRuleCount = 4;

            /// **A**:同址 coherent atomic → SHARP reduce ⇒ atomic 必须先 **completion**。
            constexpr bool ordering_a_allows_reduce(bool atomic_completed,
                                                    bool reduce_requested) noexcept {
                return !reduce_requested || atomic_completed;
            }

            /// **B**:同址 SHARP reduce → coherent atomic ⇒ reduce 必须先 **commit**。
            constexpr bool ordering_b_allows_atomic(bool reduce_committed,
                                                    bool atomic_requested) noexcept {
                return !atomic_requested || reduce_committed;
            }

            /// **C**:同 group 内 reduce 顺序由 **code** 排序约束 ⇒ 「a 先于 b」⇔ `code_a <
            /// code_b`。 ⚠️ **code 相等时顺序未定义** ⇒ 见
            /// `kReduceCodeTieUnspecified`(排序约束无公开来源)。
            constexpr bool ordering_c_orders_reduce(std::uint32_t code_a,
                                                    std::uint32_t code_b) noexcept {
                return code_a < code_b;
            }

            /// ⚠️ **code 相等 ⇒ 顺序未定义** ⇒ 显式登记,禁止实现层**默认**某一种序。
            inline constexpr bool kReduceCodeTieUnspecified = true;

            /// **D**:SHARP barrier → SHARP reduce ⇒ group-scoped rendezvous 必须先完成。
            constexpr bool ordering_d_allows_reduce(bool barrier_completed,
                                                    bool reduce_requested) noexcept {
                return !reduce_requested || barrier_completed;
            }

            // ═════════════════════════════════════════════
            //  ⑤ 集合 VC 预留(**单一状态**;Decision D)
            // ═════════════════════════════════════════════

            /// **集合 VC 预留状态** —— 载体**仅**表达单一取值 `Unavailable`。
            /// ⚠️ 类型上**无** `vc_id`、**无** `reason` / `owner` / `revisit_trigger`
            /// ⇒ **四字段权威恒在** `collective_planes.json` + `CP-03`(Decision D)。
            enum class VcReservationState : std::uint8_t { Unavailable = 0 };

            inline constexpr std::size_t kVcReservationStateCount = 1;

            // ═════════════════════════════════════════════
            //  ⑥ 能力域(**节点内**;A-1 / A-5)
            // ═════════════════════════════════════════════

            /// **collective 能力域** —— NVLink SHARP(`NVLS`)用于**节点内**归约;跨节点归约属 CollNet
            /// 通路 ⇒ 载体**只允许** `IntraNode`(⛔ **不得**声明跨 fabric 的通用归约能力)。
            enum class CollectiveScope : std::uint8_t { IntraNode = 0 };

            inline constexpr std::size_t kCollectiveScopeCount = 1;

            // ═════════════════════════════════════════════
            //  ⑦ 编译期断言(结构层;完整锁在 Phase 2)
            // ═════════════════════════════════════════════

            static_assert(kIngressDecisionCount == 2, "分类结果必须恰 2 个");
            static_assert(kRegisterResultCount == 2, "注册结果必须恰 2 个");
            static_assert(kEpochStateCount == 2, "epoch 平台侧状态必须恰 2 个");
            static_assert(kOrderingRuleCount == 4, "ordering 必须恰 4 类(A/B/C/D)");
            static_assert(kVcReservationStateCount == 1,
                          "VC 预留状态必须只有 Unavailable(单一取值)");
            static_assert(kCollectiveScopeCount == 1, "能力域必须只有 IntraNode(节点内)");

            // ⭐ **R4 回归锁** —— 未 commit ⇒ **放行**(⛔ 不是「拒绝」)。本仓已错过一次。
            static_assert(!epoch_routes_to_collective(EpochState::Uncommitted),
                          "R4:epoch 未 commit ⇒ 必须放行(正常服务),不得拒绝");
            static_assert(epoch_routes_to_collective(EpochState::Committed),
                          "R4:epoch 已 commit ⇒ 必须 divert");

            // **分类语义锁** —— 命中 + commit ⇒ divert;其余 ⇒ pass。
            static_assert(classify_at_fabric_ingress(true, true) == IngressDecision::DiverT,
                          "分类:命中且 epoch 已 commit ⇒ divert");
            static_assert(classify_at_fabric_ingress(true, false) == IngressDecision::Pass,
                          "分类:命中但 epoch 未 commit ⇒ 放行(R4)");
            static_assert(classify_at_fabric_ingress(false, true) == IngressDecision::Pass,
                          "分类:未命中 ⇒ 放行");
            static_assert(decision_after_destroy() == IngressDecision::Pass,
                          "R4:注销后 ⇒ 正常服务(pass)");

            // **R3 / R5 语义锁**
            static_assert(register_region(true) == RegisterResult::Ok, "R3:drain 成功 ⇒ Ok");
            static_assert(register_region(false) == RegisterResult::DrainFailed,
                          "R3:drain 失败 ⇒ 注册失败");
            static_assert(is_fail_stop(true, true, true), "R5:命中 + commit + atomic ⇒ fail-stop");
            static_assert(!is_fail_stop(false, true, true), "R5:未命中 ⇒ 非 fail-stop");

            // **ordering 语义锁**
            static_assert(ordering_a_allows_reduce(true, true),
                          "A:atomic 已 completion ⇒ 允许 reduce");
            static_assert(!ordering_a_allows_reduce(false, true),
                          "A:atomic 未 completion ⇒ 禁止 reduce");
            static_assert(ordering_b_allows_atomic(true, true), "B:reduce 已 commit ⇒ 允许 atomic");
            static_assert(!ordering_b_allows_atomic(false, true),
                          "B:reduce 未 commit ⇒ 禁止 atomic");
            static_assert(ordering_c_orders_reduce(1, 2), "C:code 小者先");
            static_assert(!ordering_c_orders_reduce(2, 1), "C:code 大者后");
            static_assert(ordering_d_allows_reduce(true, true), "D:barrier 已完成 ⇒ 允许 reduce");
            static_assert(!ordering_d_allows_reduce(false, true), "D:barrier 未完成 ⇒ 禁止 reduce");

        } // namespace nvlink_sharp
    } // namespace cep
} // namespace tlm

#endif // TLM_CEP_NVLINK_SHARP_STATE_SPEC_HH
