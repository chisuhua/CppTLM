// =============================================================================
// include/tlm/cep/nvlink_sharp_state_spec.hh
//   —— NVLink SHARP 规约载体(规约层 · header-only · 自足)
//
// 作者: CppTLM Team / 日期: 2026-11-03
// 配套: docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md(basis 唯一人工审计面;
//         `A-n` 公开断言 / `N-n` pending 白名单 / `C-n` 契约锚点)
//       openspec/changes/cpptlm-cep-nvlink-sharp-state-spec/(change 4 件套)
//       台账 obj-cep-platform.md Entry #173(Task 0)· #176(①/⑤)· #177(③/④)
//         · #178 / #179(critic 两轮)· #180 / #181(Phase 1)· #182(Phase 2 + D-1/D-2/D-3)
//       docs/adr/ADR-CEP-03-... §Status Update(2026-11-03)(F.1 / F.2 / K.2)
//
// ── 定位 ──────────────────────────────────────────────────────────────────
// **不是缓存、不是网络组件**:它不接 `connections[]`、不进 `ModuleFactory`、不注册 ABI。
// 它是 **NVLink SHARP 语义的规约真源**:分类判据(R1/R2)· 区域与 epoch 纪律(R3/R4)·
// 可观测性(R5)· 跨平面 ordering A/B/C/D · 集合 VC 预留单一状态 · 能力域 ·
// **逐格 `basis`(两轴)** · **9 项 pending 登记**
// ⇒ 供第 1 个 change 的 `NvlinkAdapter` **零自有表副本**地委托消费。
//
// ── ⛔ 自足红线(per ADR-CEP-03 SU 2026-11-03(#2) F.1)────────────────────────
// ① **只 include 标准库**;⛔ **SHALL NOT** include `collective_plane.hh`
//    ⇒ 故本载体**不引用**平台词表型语义(`ReduceOp` / `CollectiveOp` / `AccessMode` 等枚举)
//    ⇒ 算子族与原语族的**唯一真源恒为** `tlm::cep::ReduceOp` / `tlm::cep::CollectiveOp`
//    ⇒ 「恰 6 值」的运行期断言**归实现层**,本载体**不自证**。
// ② MAY 自带**形状型**辅助类型(`GroupId` / `AddressRange` / `DivertCounter`)——
// 形状**不承载**语义枚举。 ③ ⛔ **SHALL NOT** 定义 / 暴露 `collective_region_table` 类型 ⇒
// 区域表的**运行时状态归实现层**
//    ⇒ 载体只提供**纯函数形态**的判定(输入 = 「是否命中」/「epoch 是否有效」/「drain 是否成功」
//    这类**已判定的事实**,输出 = 判定结果),状态容器由调用方持有(per SU(#3) K.2)。
// ④ ⛔ **SHALL NOT** 定义任何 reduce 算子枚举 / 常量 ⇒ 逐算子支持一律 **pending**(锚点 `N-1`)。
// ⑤ 标准库 include 清单:`<array>` / `<cstddef>` / `<cstdint>` / `<type_traits>`
//    ⇒ 全部是标准库,**不破** F.1-a。
//
// ── 口径(per 裁决链)───────────────────────────────────────────────────────
// ① **R1/R2**:opcode **永不作分类器**;**唯一分类点 = fabric ingress**
//    ⇒ 本载体的分类函数**签名里没有 opcode、也没有区域表** ⇒ 源端**无法**用它分类
//    ⇒ 编译期另有**签名锁**(`NSS-01` / `CP-OPCODE-CLASSIFIER`)。
// ② **R3**:注册 = **登记 + drain + 建 epoch** 三步原子;drain 失败 ⇒ 注册失败且**不建 epoch**。
// ③ **R4**:epoch **未 commit 前 = 正常服务(放行)**;commit 后命中 ⇒ divert。
//    ⚠️ ⛔ **不得**写成「未 commit ⇒ 拒绝」—— 与 R4 矛盾(本仓已错过一次,per Entry #169 ②-⑨)。
// ④ **R5**:divert 计数器(**形状**,按 group / 区域可寻址)+ **fail-stop** 断言(命中 = error)。
// ⑤ **ordering A/B/C/D**:四类**可判定**谓词(per ADR-CEP-03 §3.3);**code 相等**时顺序未定义。
// ⑥ **VC 预留**:载体**仅**表达 `Unavailable` **单一取值**(类型上**无** `vc_id`、**无**四字段)
//    ⇒ **四字段权威恒在** `collective_planes.json` + `CP-03`(Decision D)。
// ⑦ **能力域** = **节点内**(intra-node;A-1 / A-5)⇒ 载体只允许 `IntraNode`。
//    ⚠️ 本条约束**能力域**,**不**约束**流量落点**(落点见 ADR-CEP-01 SU(#2) G.1)。
//
// ── 逐格 `basis` 的口径(Phase 2;per 台账 Entry #182 的 D-1 / D-2 / D-3)────────
// · **格 = 12 个枚举值**:`IngressDecision`2 + `RegisterResult`2 + `EpochState`2
//   + `OrderingRule`4 + `VcReservationState`1 + `CollectiveScope`1
//   (形状型 `GroupId` / `AddressRange` / `DivertCounter` **不占格**)。
// · **两轴**:`kind`(证据强度)× `pending`(钉版强度)—— 二者**必须拆开**。
// · **`pending == false` 的充分条件**:`source` 的**许可已人工确认**
//   (`kSourceLicenseConfirmed[source] == true`)。
//   ⇒ 本 change 的 12 格**全部** `source = AdrContract`(本仓已签发 ADR ⇒ 许可**自持**)
//     ⇒ 不依赖 S1 / S2 / S3 的再分发许可(三者均**未**确认 ⇒ 保守取 `false`)。
// · **`ref` 的解析空间** = `A-n`(公开锚点)/ `N-n`(pending)/ `C-n`(**契约锚点**)。
//   `ref` MAY 含**多个 token**(空格分隔):`C-n` = 断言依据,`A-n` = **佐证指针**。
//   ⚠️ 佐证指针**不**参与许可门控 —— 门控只看 `source`(见锚点摘要 §2bis.2 口径 2)。
// · ⛔ `kind` **禁止升格**;`ref` **SHALL NOT** 指向规范原文。
//
// ── 9 项 pending 的口径(Phase 2)───────────────────────────────────────────
// `N-1` ~ `N-9` = **语义本身尚未确定**(只有规范正文能定)⇒ 载体**不得**出现其具体取值的断言。
// ⚠️ 与「格的 `pending`(钉版强度)」是**两个层次**:格级 `pending` = 该格的**证据**可否直接用;
//    本清单 = **语义本身**尚未确定 ⇒ 二者**不得混谈**(照队列 #12 先例)。
//
// ── 阶段 ──────────────────────────────────────────────────────────────────
// **Phase 1**(已完成):结构层 —— 类型 / 纯函数 / 常量表 / 编译期断言。
// **Phase 2**(本文件当前内容):逐格 `basis`(12 格)+ 来源许可表 + 9 项 pending 登记
//   + **非 pending 格数下限** + 归因 id 落进断言文案 + 两处判据口径订正
//   (`classify_after_destroy` 参数化 / `DivertCounter` 2 字段可寻址键)。
//
// ── ⛔ 合规禁令 ───────────────────────────────────────────────────────────
// NVIDIA NVLink SHARP 正式文档受 NDA 约束(照 CXL 的 ECA §3 + AI AUP 同款)⇒
// **禁止**把规范正文投喂任何 AI ⇒ 本载体**零**规范正文引用,零 NDA 材料引用。
// =============================================================================
#ifndef TLM_CEP_NVLINK_SHARP_STATE_SPEC_HH
#define TLM_CEP_NVLINK_SHARP_STATE_SPEC_HH

#include <array>
#include <cstddef>
#include <cstdint>

#include "tlm/cep/source_registry.hh"
#include <type_traits>

namespace tlm {
    namespace cep {
        namespace nvlink_sharp {

            // ═════════════════════════════════════════════
            //  ⓪ `basis` 元数据(来源 / 证据强度 / 逐格 Basis)
            // ═════════════════════════════════════════════

            /// **来源** —— `source` 只表达「许可门控看谁」,不表达「断言依据在哪」(后者看 `ref`)。
            enum class SourceId : std::uint8_t {
                S1 = 0,          // arXiv 2507.04786v3(公开预印本)—— 再分发许可**未**确认
                S2 = 1,          // NVIDIA Developer Blog —— © NVIDIA,再分发**未**授权
                S3 = 2,          // NVIDIA SHARP UM Overview —— © NVIDIA,再分发**未**授权
                AdrContract = 3, // **本仓已签发 ADR(契约锚点)** —— 许可**自持**
                Derived = 4      // 派生 / 二手推论 —— 无许可基础
            };

            inline constexpr std::size_t kSourceCount = 5;

            /// **逐来源**:「引用 / 再分发许可已**人工确认**」
            /// ⚠️ **保守**取值 —— `S1` / `S2` / `S3` 的**再分发**许可均未确认 ⇒ `false`
            /// ⇒ 以它们为依据的格**必须** `pending = true`;本 change 的 12 格**均不以它们为依据**。
            inline constexpr std::array<bool, kSourceCount> kSourceLicenseConfirmed{{
                false, // S1 —— arXiv per-paper license 未逐一核验
                false, // S2 —— © NVIDIA(再分发未授权)
                false, // S3 —— © NVIDIA(再分发未授权)
                true,  // AdrContract —— 本仓自著 ⇒ 无第三方许可问题
                false  // Derived —— 派生 / 二手
            }};

            /// **证据强度**(禁止升格)
            enum class EvidenceKind : std::uint8_t {
                PublicDirect = 0,     // 公开来源**明确这样写**
                PublicAnalogical = 1, // 跨代次 / 跨协议**类比**
                SelfDerived = 2       // **平台自定**(ADR 契约)
            };

            /// **逐格 `basis`**
            struct Basis {
                EvidenceKind kind = EvidenceKind::SelfDerived;
                bool pending = true;
                SourceId source = SourceId::Derived;
                /// ⚠️ 可含**多个 token**(空格分隔):`C-n` / `A-n` / `N-n`
                const char* ref = nullptr;
            };

            // ── 来源**类型**标签(裁决:甲「类型标签统一 / 清单自治」,per 台账 Entry #266)──
            /// ⚠️ **清单自治**:本载体的来源清单 = **本地 `SourceId`**(自带逐来源许可表);
            ///    全局 `kSources[]` 只作**跨载体统一的类型标签**(`RefKind`)⇒ 两者**不是**同一真源。
            ///    映射:`S1/S2/S3` = **公开非规范来源**(预印本 / 厂商文档)⇒ `public_nonnormative`;
            ///    `AdrContract` = **本仓已签 ADR**(契约锚点)⇒ `repo_derived`;`Derived` ⇒ `repo_derived`。
            ///    ⛔ 本族**恒不出现** `spec_clause`(NVLink 无条款级公开规范,per `07 号` SU · 四 D-1)
            ///    ⇒ **不计入 B3b**。
            static constexpr RefKind ref_kind_of(const Basis& b) noexcept {
                switch (b.source) {
                    case SourceId::S1:
                    case SourceId::S2:
                    case SourceId::S3:
                        return RefKind::PublicNonnormative;
                    default:
                        return RefKind::RepoDerived;
                }
            }

            /// `basis` ↔ 来源类型一致性(有牙:`PublicDirect` 指向 `AdrContract`/`Derived` ⇒ 必红)。
            static constexpr bool basis_ref_kind_consistent(const Basis& b) noexcept {
                const RefKind k = ref_kind_of(b);
                switch (b.kind) {
                    case EvidenceKind::PublicDirect:
                        return k == RefKind::PublicNonnormative &&
                               (b.source == SourceId::S1 || b.source == SourceId::S2 ||
                                b.source == SourceId::S3);
                    case EvidenceKind::PublicAnalogical:
                    case EvidenceKind::SelfDerived:
                        return k != RefKind::SpecClause;
                }
                return false;
            }

            /// **`basis` 有效性**:① `ref` 非空 ② `source` 下标合法
            /// ③ `pending == false` ⇒ 该来源的**许可已人工确认**
            constexpr bool basis_is_valid(const Basis& b) noexcept {
                return b.ref != nullptr && b.ref[0] != '\0' &&
                       static_cast<std::size_t>(b.source) < kSourceCount &&
                       (b.pending || kSourceLicenseConfirmed[static_cast<std::size_t>(b.source)]);
            }

            /// 逐组 `basis` 表的**全格**有效性(编译期)
            template <std::size_t N>
            constexpr bool all_basis_valid(const std::array<Basis, N>& t) noexcept {
                for (const auto& b : t) {
                    if (!basis_is_valid(b))
                        return false;
                }
                return true;
            }

            // ═════════════════════════════════════════════
            //  ① 形状型辅助类型(**不承载**语义枚举)
            // ═════════════════════════════════════════════

            /// **形状型**:group 标识 —— ⚠️ 平台真源的 `tlm::cep::group_id_t` 在
            /// `collective_plane.hh`,而本载体⛔ **不得** include 它 ⇒ 此处是**形状副本**
            /// (不承载任何语义)⇒ 消费方负责转换。
            /// ⚠️ `A-4`(多 group 并存)在本载体**只**以本形状承载、**未断言**「多 group」
            /// ⇒ 见锚点摘要 §2bis.2 口径 3(⛔ 不得声称已覆盖)。
            using GroupId = std::uint32_t;

            /// **形状型**:地址区间 —— 同上,**不含**任何语义枚举。
            struct AddressRange {
                std::uint64_t lo = 0;
                std::uint64_t hi = 0;
                constexpr bool valid() const noexcept {
                    return lo <= hi;
                }
            };

            /// **形状型**:divert 计数器的**可寻址键**(`NSS-05` 的「按 group / 区域可寻址」)。
            struct DivertCounterKey {
                GroupId group = 0;
                AddressRange region{};
            };

            /// **R5 的计数器形状** —— ⚠️ 载体**不持有**计数器状态(状态归实现层)。
            /// **恰 2 字段**:`key`(可寻址键:group + 区域)+ `total`(计数)
            /// ⇒ 有编译期字段数锁(`NSS-05` / `CP-DIVERT-STOP`)。
            struct DivertCounter {
                DivertCounterKey key{};
                std::uint64_t total = 0;
            };

            // ═════════════════════════════════════════════
            //  ② R1/R2 分类判据(**唯一分类点 = fabric ingress**)
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
            /// 判定与「源端是谁」无关)。 ⇒ 该签名有**编译期锁**(见 §⑪ 的 `CP-OPCODE-CLASSIFIER`)。
            constexpr IngressDecision classify_at_fabric_ingress(bool region_hit,
                                                                 bool epoch_committed) noexcept {
                return (region_hit && epoch_committed) ? IngressDecision::DiverT
                                                       : IngressDecision::Pass;
            }

            // ═════════════════════════════════════════════
            //  ③ R3/R4 区域与 epoch 纪律
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

            /// **R4 的注销判定** —— 输入是「注销后该区域**是否仍留在表内**」这一**已判定的事实**。
            ///
            /// · 注销**成功**(`region_still_registered == false`)⇒ `Pass`(回归 coherence
            /// **正常服务**) · 注销**失败**(仍留表内)⇒ 仍 `DiverT` ⇒
            /// 该**泄露可被观测**(`NSS-04-DESTROY-LEAK` 的鉴别点)
            ///
            /// ⚠️ 本函数**刻意不是恒真** —— 无参返常量的形态无法被任何反向 fixture 毒化(违铁律①)。
            constexpr IngressDecision
            classify_after_destroy(bool region_still_registered) noexcept {
                return region_still_registered ? IngressDecision::DiverT : IngressDecision::Pass;
            }

            // ═════════════════════════════════════════════
            //  ④ R5 可观测性(fail-stop 判据;计数器形状见 §①)
            // ═════════════════════════════════════════════

            /// **R5 的 fail-stop 判据** —— epoch commit 后仍有 coherence atomic 命中已注册区域
            /// ⇒ **error**(⛔ 不是 warning、不是静默计数)。
            constexpr bool is_fail_stop(bool region_hit, bool epoch_committed,
                                        bool coherence_atomic_hit) noexcept {
                return region_hit && epoch_committed && coherence_atomic_hit;
            }

            // ⚠️ **R1/R2 分流 ↔ R5 fail-stop 的交叉锁**(2026-11-05,per architect 裁决 oracle 复核)
            // —— 把「R1/R2 的命中判定」与「R5 的 fail-stop 检出」钉成**同一输入语义**:
            //    fail-stop 命中 ⟺ (R1/R2 分流判 DiverT) ∧ (coherence atomic 命中)
            // ⇒ **语义漂移防护**:若将来 `classify_at_fabric_ingress` 与 `is_fail_stop` 的
            //    `region_hit` / `epoch_committed` 语义分叉(两者独立演化)⇒ 本锁**立即红**。
            // ⇒ 关闭 H5 假设「5 条规则**可靠区分**」的系统级含义的演化漂移面
            //   (per `objective §9.1` ① 表 H5 判据;证据边界 = **函数级**,非协议级/流量级)。
            /// **交叉锁的可执行形态**(见上方注释)—— 用 constexpr 函数 + static_assert
            /// (类作用域内 static_assert 不能引用函数参数名 ⇒ 须包一层)。
            static constexpr bool fail_stop_matches_ingress_split(bool region_hit,
                                                                 bool epoch_committed,
                                                                 bool coherence_atomic_hit) noexcept {
                return is_fail_stop(region_hit, epoch_committed, coherence_atomic_hit) ==
                       (classify_at_fabric_ingress(region_hit, epoch_committed) ==
                            IngressDecision::DiverT &&
                        coherence_atomic_hit);
            }
            static_assert(fail_stop_matches_ingress_split(true, true, true),
                          "R5 fail-stop 的输入语义必须与 R1/R2 分流一致(交叉锁)");
            static_assert(fail_stop_matches_ingress_split(false, true, true),
                          "R5 fail-stop 的输入语义必须与 R1/R2 分流一致(交叉锁)");
            static_assert(fail_stop_matches_ingress_split(true, false, true),
                          "R5 fail-stop 的输入语义必须与 R1/R2 分流一致(交叉锁)");
            static_assert(fail_stop_matches_ingress_split(true, true, false),
                          "R5 fail-stop 的输入语义必须与 R1/R2 分流一致(交叉锁)");
            static_assert(fail_stop_matches_ingress_split(false, false, false),
                          "R5 fail-stop 的输入语义必须与 R1/R2 分流一致(交叉锁)");

            // ═════════════════════════════════════════════
            //  ⑤ 跨平面 ordering A/B/C/D(**可判定**谓词)
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
            //  ⑥ 集合 VC 预留(**单一状态**;Decision D)
            // ═════════════════════════════════════════════

            /// **集合 VC 预留状态** —— 载体**仅**表达单一取值 `Unavailable`。
            /// ⚠️ 类型上**无** `vc_id`、**无** `reason` / `owner` / `revisit_trigger`
            /// ⇒ **四字段权威恒在** `collective_planes.json` + `CP-03`(Decision D)。
            enum class VcReservationState : std::uint8_t { Unavailable = 0 };

            inline constexpr std::size_t kVcReservationStateCount = 1;

            // ═════════════════════════════════════════════
            //  ⑦ 能力域(**节点内**;A-1 / A-5)
            // ═════════════════════════════════════════════

            /// **collective 能力域** —— NVLink SHARP(`NVLS`)用于**节点内**归约;跨节点归约属 CollNet
            /// 通路 ⇒ 载体**只允许** `IntraNode`(⛔ **不得**声明跨 fabric 的通用归约能力)。
            enum class CollectiveScope : std::uint8_t { IntraNode = 0 };

            inline constexpr std::size_t kCollectiveScopeCount = 1;

            // ═════════════════════════════════════════════
            //  ⑧ 逐格 `basis`(**12 格** = 2+2+2+4+1+1)
            // ═════════════════════════════════════════════
            //
            // ⚠️ **唯一真源** —— 锚点摘要**不复制**本表(只提供条目号 `A-n` / `N-n` / `C-n`);
            //    测试逐格核对。
            // ⚠️ 本 change 的 12 格**全部** `kind = SelfDerived` + `pending = false`
            //    + `source = AdrContract`(per 台账 Entry #182 的 D-2):
            //    能力域收窄与 R1~R5 / ordering 全是**已签发 ADR 的契约**,不是公开来源的转述。
            // ⚠️ `ref` 里点名的 `A-n` 是**佐证指针**(说明契约不凭空)⇒ **不**参与许可门控。

            /// ① `IngressDecision` 2 格 —— `C-1`(R1/R2)+ 佐证 `A-3`(归约在 fabric 内完成)
            inline constexpr std::array<Basis, kIngressDecisionCount> kBasisIngressDecision{
                {{EvidenceKind::SelfDerived, false, SourceId::AdrContract, "C-1 A-3"},
                 {EvidenceKind::SelfDerived, false, SourceId::AdrContract, "C-1 A-3"}}};

            /// ② `RegisterResult` 2 格 —— `C-2`(R3 三步原子)
            inline constexpr std::array<Basis, kRegisterResultCount> kBasisRegisterResult{
                {{EvidenceKind::SelfDerived, false, SourceId::AdrContract, "C-2"},
                 {EvidenceKind::SelfDerived, false, SourceId::AdrContract, "C-2"}}};

            /// ③ `EpochState` 2 格 —— `C-3`(R4 双向);
            ///    ⚠️ **协议侧** epoch 状态机 = `N-3` pending(本表**不**覆盖它)
            inline constexpr std::array<Basis, kEpochStateCount> kBasisEpochState{
                {{EvidenceKind::SelfDerived, false, SourceId::AdrContract, "C-3"},
                 {EvidenceKind::SelfDerived, false, SourceId::AdrContract, "C-3"}}};

            /// ④ `OrderingRule` 4 格 —— `C-4` / `C-5` / `C-6` / `C-7`
            inline constexpr std::array<Basis, kOrderingRuleCount> kBasisOrderingRule{
                {{EvidenceKind::SelfDerived, false, SourceId::AdrContract, "C-4"},
                 {EvidenceKind::SelfDerived, false, SourceId::AdrContract, "C-5"},
                 {EvidenceKind::SelfDerived, false, SourceId::AdrContract, "C-6"},
                 {EvidenceKind::SelfDerived, false, SourceId::AdrContract, "C-7"}}};

            /// ⑤ `VcReservationState` 1 格 —— `C-8`(Decision D);⚠️ `N-4`(VC 编号)**不**在此格
            inline constexpr std::array<Basis, kVcReservationStateCount> kBasisVcReservationState{
                {{EvidenceKind::SelfDerived, false, SourceId::AdrContract, "C-8"}}};

            /// ⑥ `CollectiveScope` 1 格 —— `C-9`(能力域收窄)+ 佐证 `A-1` / `A-5`
            inline constexpr std::array<Basis, kCollectiveScopeCount> kBasisCollectiveScope{
                {{EvidenceKind::SelfDerived, false, SourceId::AdrContract, "C-9 A-1 A-5"}}};

            // ═════════════════════════════════════════════
            //  ⑨ 9 项语义 `pending`(只能靠规范正文确定 ⇒ 禁止推理填空)
            // ═════════════════════════════════════════════

            struct PendingSemanticItem {
                const char* id = nullptr;   // `N-1` ~ `N-9`
                const char* what = nullptr; // 一句话摘要
                const char* ref = nullptr;  // 锚点摘要条目号
            };

            inline constexpr std::size_t kPendingSemanticItemCount = 9;

            /// **9 项**语义 pending(per 锚点摘要 §3)—— 逐条登记,供判据逐项寻址
            inline constexpr std::array<PendingSemanticItem, kPendingSemanticItemCount>
                kPendingSemanticItems{
                    {{"N-1", "reduce 函数族的穷举算子集", "§3 / N-1"},
                     {"N-2", "逐集合操作的 ✓/✗ 支持矩阵", "§3 / N-2"},
                     {"N-3", "group epoch 的状态机与提交时点", "§3 / N-3"},
                     {"N-4", "VC / 通道编号、每 VC 的用途分配", "§3 / N-4"},
                     {"N-5", "聚合树的构建算法(选根 / 树高 / 失败重建)", "§3 / N-5"},
                     {"N-6", "协议报文格式 / opcode 布局", "§3 / N-6"},
                     {"N-7", "数值 / 精度语义(浮点归约顺序 / 确定性)", "§3 / N-7"},
                     {"N-8", "NVLink 实例的 `perm` 具体子集", "§3 / N-8"},
                     {"N-9", "NVLink atomic 的协议侧 completion 配对", "§3 / N-9"}}};

            // ═════════════════════════════════════════════
            //  ⑩ 非 pending 格数下限(防「规约层自身成为纸面契约」)
            // ═════════════════════════════════════════════

            template <std::size_t N>
            constexpr std::size_t count_non_pending(const std::array<Basis, N>& t) noexcept {
                std::size_t n = 0;
                for (const auto& b : t) {
                    if (!b.pending)
                        ++n;
                }
                return n;
            }

            /// **12 格中 `pending == false` 的格数**(编译期实算,不是手写常量)
            inline constexpr std::size_t kNonPendingCellCount =
                count_non_pending(kBasisIngressDecision) + count_non_pending(kBasisRegisterResult) +
                count_non_pending(kBasisEpochState) + count_non_pending(kBasisOrderingRule) +
                count_non_pending(kBasisVcReservationState) +
                count_non_pending(kBasisCollectiveScope);

            /// **下限** —— 由 Phase 1 的实际产出确定(12 格全部出自已签发 ADR)
            /// ⇒ 任一格被改成 `pending = true` ⇒ 低于下限 ⇒ **编译红**。
            inline constexpr std::size_t kNonPendingFloor = 12;

            // ═════════════════════════════════════════════
            //  ⑪ 编译期断言(NSS-01 ~ NSS-09;⚠️ **每条带归因 id**)
            // ═════════════════════════════════════════════

            // ── 辅助:常量字符串比较(不引入 <string_view>)──────────────────────
            constexpr bool cstr_eq(const char* a, const char* b) noexcept {
                if (a == nullptr || b == nullptr)
                    return a == b;
                while (*a != '\0' && *b != '\0') {
                    if (*a != *b)
                        return false;
                    ++a;
                    ++b;
                }
                return *a == *b;
            }

            /// 逐项良构:三字段非空 + id 形态 `N-<数字>` + **id 唯一**
            template <std::size_t N>
            constexpr bool
            all_pending_items_wellformed(const std::array<PendingSemanticItem, N>& t) noexcept {
                for (std::size_t i = 0; i < N; ++i) {
                    const auto& it = t[i];
                    if (it.id == nullptr || it.id[0] == '\0' || it.what == nullptr ||
                        it.what[0] == '\0' || it.ref == nullptr || it.ref[0] == '\0')
                        return false;
                    if (it.id[0] != 'N' || it.id[1] != '-')
                        return false;
                    for (std::size_t j = i + 1; j < N; ++j) {
                        if (cstr_eq(it.id, t[j].id))
                            return false;
                    }
                }
                return true;
            }

            /// `DivertCounter` 的**字段数**(结构化绑定 ⇒ 增删字段即编译红)
            constexpr std::size_t divert_counter_field_count() noexcept {
                constexpr DivertCounter probe{};
                const auto& [key, total] = probe;
                (void)key;
                (void)total;
                return 2;
            }

            // ── NSS-01 / CP-OPCODE-CLASSIFIER:分类点唯一 + **签名无 opcode** ──────
            static_assert(kIngressDecisionCount == 2,
                          "CP-OPCODE-CLASSIFIER: 分类结果 SHALL 恰 2 个(DiverT / Pass)");
            static_assert(std::is_same_v<decltype(&classify_at_fabric_ingress),
                                         IngressDecision (*)(bool, bool) noexcept>,
                          "CP-OPCODE-CLASSIFIER: classify_at_fabric_ingress SHALL 恰 2 个 bool 参数"
                          "且 noexcept —— 签名里 SHALL NOT 出现 opcode / 区域表 / group 上下文");

            // ── NSS-02 / CP-EPOCH-SEMANTICS:R4 **双向**(未 commit ⇒ 放行)──────────
            static_assert(
                kEpochStateCount == 2,
                "CP-EPOCH-SEMANTICS: epoch 平台侧状态 SHALL 恰 2 个(Uncommitted / Committed)");
            static_assert(!epoch_routes_to_collective(EpochState::Uncommitted),
                          "CP-EPOCH-SEMANTICS: R4 —— epoch 未 commit ⇒ SHALL 放行(正常服务),"
                          "SHALL NOT 拒绝(本仓已错过一次的回归锁)");
            static_assert(epoch_routes_to_collective(EpochState::Committed),
                          "CP-EPOCH-SEMANTICS: R4 —— epoch 已 commit ⇒ SHALL divert");
            static_assert(classify_at_fabric_ingress(true, true) == IngressDecision::DiverT,
                          "CP-OPCODE-CLASSIFIER: 命中 + epoch 已 commit ⇒ SHALL divert");
            static_assert(classify_at_fabric_ingress(true, false) == IngressDecision::Pass,
                          "CP-EPOCH-SEMANTICS: 命中但 epoch 未 commit ⇒ SHALL 放行(R4)");
            static_assert(classify_at_fabric_ingress(false, true) == IngressDecision::Pass,
                          "CP-OPCODE-CLASSIFIER: 未命中 ⇒ SHALL 放行(与源端 opcode 无关)");

            // ── NSS-03 / CP-DRAIN-FAIL:注册三步原子 ────────────────────────────
            static_assert(kRegisterResultCount == 2,
                          "CP-DRAIN-FAIL: 注册结果 SHALL 恰 2 个(Ok / DrainFailed)");
            static_assert(register_region(true) == RegisterResult::Ok,
                          "CP-DRAIN-FAIL: R3 —— drain 成功 ⇒ SHALL 返回 Ok");
            static_assert(register_region(false) == RegisterResult::DrainFailed,
                          "CP-DRAIN-FAIL: R3 —— drain 失败 ⇒ SHALL 返回 DrainFailed(且不建 epoch)");

            // ── NSS-04 / NSS-04-DESTROY-LEAK:注销回归 + **泄露可观测** ───────────
            static_assert(classify_after_destroy(false) == IngressDecision::Pass,
                          "NSS-04-DESTROY-LEAK: R4 —— 注销成功(区域已退出表)⇒ SHALL 回归 coherence"
                          "正常服务(Pass)");
            static_assert(classify_after_destroy(true) == IngressDecision::DiverT,
                          "NSS-04-DESTROY-LEAK: 注销**失败**(区域仍留表内)⇒ SHALL 仍 divert"
                          "⇒ 泄露可被观测(⛔ 不得静默 Pass)");

            // ── NSS-05 / CP-DIVERT-STOP:计数器形状 + fail-stop ──────────────────
            static_assert(divert_counter_field_count() == 2,
                          "CP-DIVERT-STOP: R5 —— DivertCounter SHALL 恰 2 字段"
                          "(可寻址键 group+region / 计数)");
            static_assert(is_fail_stop(true, true, true),
                          "CP-DIVERT-STOP: R5 —— 命中 + epoch 已 commit + coherence atomic ⇒ SHALL "
                          "fail-stop");
            static_assert(!is_fail_stop(false, true, true),
                          "CP-DIVERT-STOP: R5 —— 未命中 ⇒ SHALL NOT fail-stop");

            // ── NSS-06 / NSS-06-ORDERING-MIS:ordering A/B/C/D ──────────────────
            static_assert(kOrderingRuleCount == 4,
                          "NSS-06-ORDERING-MIS: ordering SHALL 恰 4 类(A/B/C/D)");
            static_assert(ordering_a_allows_reduce(true, true),
                          "NSS-06-ORDERING-MIS: A —— atomic 已 completion ⇒ SHALL 允许 reduce");
            static_assert(!ordering_a_allows_reduce(false, true),
                          "NSS-06-ORDERING-MIS: A —— atomic 未 completion ⇒ SHALL 禁止 reduce");
            static_assert(ordering_b_allows_atomic(true, true),
                          "NSS-06-ORDERING-MIS: B —— reduce 已 commit ⇒ SHALL 允许 atomic");
            static_assert(!ordering_b_allows_atomic(false, true),
                          "NSS-06-ORDERING-MIS: B —— reduce 未 commit ⇒ SHALL 禁止 atomic");
            static_assert(ordering_c_orders_reduce(1, 2),
                          "NSS-06-ORDERING-MIS: C —— code 小者 SHALL 先");
            static_assert(!ordering_c_orders_reduce(2, 1),
                          "NSS-06-ORDERING-MIS: C —— code 大者 SHALL 后");
            static_assert(!ordering_c_orders_reduce(7, 7),
                          "NSS-06-ORDERING-MIS: C —— code 相等 ⇒ SHALL NOT 声称任何序"
                          "(顺序未定义,见 kReduceCodeTieUnspecified)");
            static_assert(ordering_d_allows_reduce(true, true),
                          "NSS-06-ORDERING-MIS: D —— barrier 已完成 ⇒ SHALL 允许 reduce");
            static_assert(!ordering_d_allows_reduce(false, true),
                          "NSS-06-ORDERING-MIS: D —— barrier 未完成 ⇒ SHALL 禁止 reduce");

            // ⚠️ **「登记锁」与「结构锁」的界限**(评测夹具 Phase 3 实测后如实登记,Entry #184)
            //   `kIngressDecisionCount` / `kRegisterResultCount` / `kEpochStateCount` /
            //   `kOrderingRuleCount` / `kVcReservationStateCount` / `kCollectiveScopeCount`
            //   都是**登记锁**:常量是**手写**的,与枚举成员**手工同步** ——
            //   C++17 **无法**反射枚举基数 ⇒ 给某个枚举**新增一个成员**时,这些计数断言
            //   **不会**自动触发(实测:`enum { Unavailable = 0, Reserved = 1 }` ⇒ 计数仍为 1)。
            //   ⇒ 真正的**结构性**保证在下面两处,**不**依赖计数常量:
            //     ① 「类型**无** `vc_id` / 四字段」—— SFINAE 可判定(探针 `CP-03`)·
            //        载体源扫描(代码标识符里不得出现 `vc_id` / `revisit_trigger`)
            //     ② 「跨 fabric 取值不可表达」—— 载体源扫描(`CrossFabric` / `InterNode` /
            //        `GlobalScope` 零命中,探针 `NSS-08-SCOPE-OVERREACH`)
            //   ⇒ ⛔ **不得**把登记锁当作结构锁来宣称(否则就是「纸面锁」)。

            // ── NSS-07 / CP-03:VC 预留单一状态 ─────────────────────────────────
            static_assert(kVcReservationStateCount == 1,
                          "CP-03: 集合 VC 预留 SHALL 只有 Unavailable 单一取值"
                          "(⛔ 不得表达 vc_id / reason / owner / revisit_trigger)");

            // ── NSS-08 / NSS-08-SCOPE-OVERREACH:能力域 = 节点内 ────────────────
            static_assert(kCollectiveScopeCount == 1,
                          "NSS-08-SCOPE-OVERREACH: 能力域 SHALL 只有 IntraNode(节点内)"
                          "—— ⛔ 不得声明跨 fabric 归约");

            // ── NSS-09 / NSS-09-BASIS-MISSING:逐格 basis + 9 项 pending + 下限 ──
            static_assert(all_basis_valid(kBasisIngressDecision),
                          "NSS-09-BASIS-MISSING: IngressDecision 逐格 basis SHALL 全格有效");
            static_assert(all_basis_valid(kBasisRegisterResult),
                          "NSS-09-BASIS-MISSING: RegisterResult 逐格 basis SHALL 全格有效");
            static_assert(all_basis_valid(kBasisEpochState),
                          "NSS-09-BASIS-MISSING: EpochState 逐格 basis SHALL 全格有效");
            static_assert(all_basis_valid(kBasisOrderingRule),
                          "NSS-09-BASIS-MISSING: OrderingRule 逐格 basis SHALL 全格有效");
            static_assert(all_basis_valid(kBasisVcReservationState),
                          "NSS-09-BASIS-MISSING: VcReservationState 逐格 basis SHALL 全格有效");
            static_assert(all_basis_valid(kBasisCollectiveScope),
                          "NSS-09-BASIS-MISSING: CollectiveScope 逐格 basis SHALL 全格有效");

            // 覆盖完整性:每组的 basis 数 SHALL == 该组的项数(逐格,不留空洞)
            static_assert(kBasisIngressDecision.size() == kIngressDecisionCount,
                          "NSS-09-BASIS-MISSING: IngressDecision 的 basis 数 SHALL == 格数");
            static_assert(kBasisRegisterResult.size() == kRegisterResultCount,
                          "NSS-09-BASIS-MISSING: RegisterResult 的 basis 数 SHALL == 格数");
            static_assert(kBasisEpochState.size() == kEpochStateCount,
                          "NSS-09-BASIS-MISSING: EpochState 的 basis 数 SHALL == 格数");
            static_assert(kBasisOrderingRule.size() == kOrderingRuleCount,
                          "NSS-09-BASIS-MISSING: OrderingRule 的 basis 数 SHALL == 格数");
            static_assert(kBasisVcReservationState.size() == kVcReservationStateCount,
                          "NSS-09-BASIS-MISSING: VcReservationState 的 basis 数 SHALL == 格数");
            static_assert(kBasisCollectiveScope.size() == kCollectiveScopeCount,
                          "NSS-09-BASIS-MISSING: CollectiveScope 的 basis 数 SHALL == 格数");

            // 9 项 pending:计数 + 逐项良构
            static_assert(
                kPendingSemanticItems.size() == kPendingSemanticItemCount,
                "NSS-09-BASIS-MISSING: 9 项 pending 表的大小 SHALL == kPendingSemanticItemCount");
            static_assert(all_pending_items_wellformed(kPendingSemanticItems),
                          "NSS-09-BASIS-MISSING: 9 项 pending SHALL 逐项良构(id 唯一 + 三字段非空 "
                          "+ 形态 N-<数字>)");

            // ⭐ **非 pending 格数下限**(防「规约层自身成为纸面契约」)
            static_assert(kNonPendingCellCount >= kNonPendingFloor,
                          "NSS-09-BASIS-MISSING: `pending == false` 的格数 SHALL >= 下限"
                          "—— 低于下限 = 规约层退化为纸面契约(design §5.3 风险 5)");

            // ⭐ 来源许可表的保守性锁:三个公开来源的再分发许可**未**确认
            static_assert(!kSourceLicenseConfirmed[static_cast<std::size_t>(SourceId::S1)] &&
                              !kSourceLicenseConfirmed[static_cast<std::size_t>(SourceId::S2)] &&
                              !kSourceLicenseConfirmed[static_cast<std::size_t>(SourceId::S3)],
                          "NSS-09-BASIS-MISSING: S1 / S2 / S3 的再分发许可 SHALL 保守取 false"
                          "(未人工确认 ⇒ 以其为依据的格必须 pending = true)");
            static_assert(kSourceLicenseConfirmed[static_cast<std::size_t>(SourceId::AdrContract)],
                          "NSS-09-BASIS-MISSING: AdrContract(本仓已签发 ADR)的许可 SHALL 为 true"
                          "—— 本 change 的 12 格依赖此条(D-2)");
            static_assert(!kSourceLicenseConfirmed[static_cast<std::size_t>(SourceId::Derived)],
                          "NSS-09-BASIS-MISSING: Derived(派生 / 二手)的许可 SHALL 为 false");

        } // namespace nvlink_sharp
    } // namespace cep
} // namespace tlm

#endif // TLM_CEP_NVLINK_SHARP_STATE_SPEC_HH
