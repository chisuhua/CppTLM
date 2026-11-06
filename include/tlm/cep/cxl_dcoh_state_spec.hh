// =============================================================================
// include/tlm/cep/cxl_dcoh_state_spec.hh
//   —— CXL 3.0 对称(DCOH · HDM-DB / CXL.mem)规约载体
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md(basis 唯一人工审计面)
//       openspec/changes/cpptlm-cep-cxl-dcoh-symmetric-state-spec/(change 4 件套)
//       台账 obj-cep-platform.md Entry #153(Task 0 普查)· #154(4 项 architect 裁决)
//       docs/adr/ADR-CEP-04-... §Status Update(2026-11-02)(聚合点二元裁决)
//
// ── 定位 ───────────────────────────────────────────────────────────────
// **不是缓存、不是网络组件**:它不接 `connections[]`、不进 `ModuleFactory`、不注册 ABI。
// 它是 **协议语义的规约真源**:3 套状态空间 / 通道词表 / `perm` 投影(按域分离)/
// 窄义「对称」判别式 ⇒ 供第 1 个 change 的 `CxlDcohAdapter` **零自有表副本**地委托消费。
//
// ── 口径(per 台账 Entry #154 的 4 项 architect 裁决 + Entry #153 的普查)────
// ① **3 套状态空间,不得合并**:
//    `DevCacheState`(4:M/E/S/I)—— CXL.cache **缓存代理**侧;
//    `DcohViewState`(3:I/S/A)—— DCOH **bias / SF 视图**侧(`A` = host **可能**处于 M/E/S/I
//    的**集合**); `HomeDirState`(3:I/S/E)—— 多 host **home 目录**侧(+ sharing list)。 ⚠️
//    三套**各自**都含名为 `I` / `S` 的元素 ⇒ **互斥性由类型系统保证,不是按名字**。
// ② `perm` **只**从 `DevCacheState` 投影(4→4)⇒ `DcohViewState` / `HomeDirState`
//    **不得**折进 `perm`(它们是 **tracker view / coherence metadata**)。
//    ⚠️ `DcohViewState` 的 `A` 是**集合** ⇒ **不可单值投影**(见 `kDcohViewIsSetLike`)。
// ③ CXL.mem 通道 = **6**(`M2S Req` / `M2S RwD` / `S2M NDR` / `S2M DRS` / `S2M BISnp` / `M2S
// BIRsp`);
//    CXL.cache 通道 = **每方向 3**(`Req` / `Rsp` / `Data`)。
//    ⇒ 一律以**枚举名**表达 ⇒ **不写死 opcode / 字段宽度 / credit / 排序**(per `pending` R-S1 /
//    R-S2)。
// ④ ⭐ **对称判别式** = `device_initiated_snoop_set == { BISnp }`(非空 ∧ size == 1)
//    —— 正是队列 #10(CXL 2.0 非对称)的「设备侧**无** snoop 发起」的**否定**。
// ⑤ 窄义「对称」= **5 谓词**合取(P1~P5)⇒ ⛔ **不得**写成 peer-to-peer 全对称
//    (公开来源明写 CXL 3.0 **不满足**经典 symmetric)。
// ⑥ 聚合点 = **恒 `HOME`**(per `ADR-CEP-04` 的 `## Status Update(2026-11-02)`:
//    原二元「P2P ⇒ `REQUESTER` / home ⇒ `HOME`」的 `REQUESTER` 分支被**公开来源反证**)
//    ⇒ `P2P` / `HDM home` **降级为「场景标签」**(承载不同的 fold 触发源,**不**决定聚合点)。
// ⑦ ⛔ **合规禁令**:CXL 规范受 Evaluation Copy Agreement §3 + AI Acceptable Use Policy
//    约束 ⇒ **禁止**把规范正文投喂任何 AI ⇒ 本载体**零**规范正文引用。
//
// ── 阶段 ───────────────────────────────────────────────────────────────
// **Phase 1(本文件当前内容)**:结构层 —— 3 套状态空间 / 通道词表 / `perm` 投影 /
//   5 谓词判别式 / **编译期断言**。
// **Phase 2(待补)**:逐格 `basis`(kind / pending / ref)+ 锚点摘要逐格核对表。
//
// ── ⚠️ 本载体**不做转移表**(设计结论,须知悉)────────────────────────────────
// 对称模式的**状态转移语义**在**公开非规范来源**里**没有** ⇒ 逐格转移表会要求
// **推理填空** ⇒ ⛔ **禁止** ⇒ 本载体**只**表达**状态空间 / 通道 / 投影 / 判别式**;
// 转移语义作为 **pending** 登记(per 锚点摘要 §7 R-S1 ~ R-S9)。
// ⇒ 消费方(第 1 个 change 的 `CxlDcohAdapter`)**不得**自造转移表。
// =============================================================================

#ifndef TLM_CEP_CXL_DCOH_STATE_SPEC_HH
#define TLM_CEP_CXL_DCOH_STATE_SPEC_HH

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "tlm/cep/source_registry.hh"

namespace tlm {
    namespace cep {
        namespace cxl_dcoh {

            // ═══════════════════════════════════════════════════════════════════════════
            //  ① 3 套状态空间(**不得合并**)
            // ═══════════════════════════════════════════════════════════════════════════

            /// **CXL.cache 缓存代理**侧稳定态(CXL 3.0 **未改** CXL 2.0 的 MESI 4 态)
            enum class DevCacheState : std::uint8_t { M = 0, E = 1, S = 2, I = 3 };

            /// **DCOH bias / SF 视图**态 —— ⚠️ `A` = host **可能**处于 `M`/`E`/`S`/`I` 的**集合**
            /// (per 锚点摘要 §1 / S6)⇒ **不是**单态 ⇒ **不可单值投影**到 `perm`
            enum class DcohViewState : std::uint8_t { I = 0, S = 1, A = 2 };

            /// **多 host home 目录**态(+ sharing list;⚠️ `E` 是否区分 dirty **pending**,per R-S5)
            enum class HomeDirState : std::uint8_t { I = 0, S = 1, E = 2 };

            inline constexpr std::size_t kDevCacheStateCount = 4;
            inline constexpr std::size_t kDcohViewStateCount = 3;
            inline constexpr std::size_t kHomeDirStateCount = 3;

            /// ⚠️ `DcohViewState::A` 是**集合** ⇒ 显式登记「**不可单值投影**」
            inline constexpr bool kDcohViewIsSetLike = true;

            /// ⚠️ `HomeDirState::E` 是否含 dirty(M)—— **pending**(per R-S5)⇒ 不得推断
            inline constexpr bool kHomeDirExclusiveDirtyUnspecified = true;

            // ═══════════════════════════════════════════════════════════════════════════
            //  ② 通道词表(**枚举名**;不写死 opcode)
            // ═══════════════════════════════════════════════════════════════════════════

            /// **CXL.mem** 通道(CXL 3.0 = 4 基础 + 2 新增)
            enum class CxlMemChannel : std::uint8_t {
                // ⚠️ 方向口径(2026-11-04 订正):`M` = **Master = host**,`S` = **Slave = device**
                //    ⇒ M2S = host → device;S2M = device → host。
                //    此前 6 行注释整体写反(M2S 标成「memory → host」、S2M 标成「host → memory」),
                //    与同文件 `kCxlMemChannelDirection`(M2S=Remote / S2M=Local)及
                //    `CDS-03`(BISnp 是**设备发起**)自相矛盾。
                M2S_Req = 0,   // host → device:request
                M2S_RwD = 1,   // host → device:read-with-ownership data
                S2M_NDR = 2,   // device → host:non-data response
                S2M_DRS = 3,   // device → host:data response
                S2M_BISnp = 4, // device → host:back-invalidate snoop  ⭐ 设备发起 / CXL 3.0 新增
                M2S_BIRsp = 5  // host → device:back-invalidate response(CXL 3.0 新增)
            };

            /// **CXL.cache** 通道(**每方向 3**;方向由 `CxlMemDirection` 之外的调用侧给出)
            enum class CxlCacheChannel : std::uint8_t { Req = 0, Rsp = 1, Data = 2 };

            inline constexpr std::size_t kCxlMemChannelCount = 6;
            inline constexpr std::size_t kCxlCacheChannelCount = 3;

            /// **通道方向**(local = 设备发起;remote = host 发起)
            enum class ChannelDirection : std::uint8_t { Local = 0, Remote = 1 };

            /// CXL.mem 通道的**方向归属** —— ⚠️ **通道级**不相交;**事件种类级**不相交**不成立**
            /// (两侧都有 snoop 种类:`H2D Req` 是 host 的 snoop,`S2M BISnp` 是设备的 snoop)
            inline constexpr std::array<ChannelDirection, kCxlMemChannelCount>
                kCxlMemChannelDirection{{
                    ChannelDirection::Remote, // M2S_Req
                    ChannelDirection::Remote, // M2S_RwD
                    ChannelDirection::Local,  // S2M_NDR
                    ChannelDirection::Local,  // S2M_DRS
                    ChannelDirection::Local,  // S2M_BISnp  ⭐ 设备发起
                    ChannelDirection::Remote  // M2S_BIRsp
                }};

            /// ⭐ **设备发起的 snoop 集合** —— 对称判别式(非空 ∧ 恰 1 类)
            inline constexpr std::array<CxlMemChannel, 1> kDeviceInitiatedSnoopSet{
                {CxlMemChannel::S2M_BISnp}};

            // ═══════════════════════════════════════════════════════════════════════════
            //  ③ `perm` 投影(**只**从 `DevCacheState`;4 值锁)
            // ═══════════════════════════════════════════════════════════════════════════

            /// `perm` 4 值锁(per `ADR-CEP-02` CEN-Ib11)⇒ **不新增**第 5 值
            enum class Perm : std::uint8_t { NONE = 0, READ = 1, UNIQUE = 2, WRITEBACK = 3 };

            inline constexpr std::size_t kPermValueCount = 4;

            /// `perm` 投影(**参数类型即约束** —— 只接受 `DevCacheState`)
            constexpr Perm perm_of(DevCacheState s) noexcept {
                switch (s) {
                case DevCacheState::M:
                    return Perm::WRITEBACK;
                case DevCacheState::E:
                    return Perm::UNIQUE;
                case DevCacheState::S:
                    return Perm::READ;
                case DevCacheState::I:
                    return Perm::NONE;
                }
                return Perm::NONE;
            }

            /// 投影表(供表级判据使用;行序 == `DevCacheState` 枚举序)
            inline constexpr std::array<Perm, kDevCacheStateCount> kPermProjection{{
                Perm::WRITEBACK, // M
                Perm::UNIQUE,    // E
                Perm::READ,      // S
                Perm::NONE       // I
            }};

            /// SFINAE 检测:某类型**是否**可投影到 `perm`(用于「域分离」的**可执行**断言)
            template <typename T, typename = void> struct HasPermOf : std::false_type {};

            template <typename T>
            struct HasPermOf<T, std::void_t<decltype(perm_of(std::declval<T>()))>>
                : std::true_type {};

            // ═══════════════════════════════════════════════════════════════════════════
            //  ④ 窄义「对称」判别式(5 谓词合取)
            // ═══════════════════════════════════════════════════════════════════════════

            /// **对称能力**的 5 个谓词(per 锚点摘要 §4)
            struct SymmetricCapability {
                /// P1 ∃ 设备发起的 back-invalidate snoop 通道(target = host)
                bool device_originated_bi_snoop = false;
                /// P2 hosting device 实现覆盖其 HDM 区间的 snoop filter / 目录
                bool device_side_sf_or_directory = false;
                /// P3 硬件一致性跨 ≥ 2 个独立 host(同一物理行可被多 host 同时缓存)
                bool spans_multiple_independent_hosts = false;
                /// P4 P2P 访问**不经** host CPU(由内存设备解析一致性)
                bool p2p_without_host_cpu = false;
                /// P5 被 snoop 的是 **caching host**,**不是** peer device cache
                bool snooped_agent_is_caching_host = false;
            };

            /// ⛔ **不得**写成 peer-to-peer 全对称 —— 公开来源明写 CXL 3.0 **不满足**经典 symmetric
            /// (「所有节点实现同一套一致性算法」)⇒ 本判别式**只**表达**窄义**特性。
            constexpr bool is_cxl3_symmetric(const SymmetricCapability& c) noexcept {
                return c.device_originated_bi_snoop && c.device_side_sf_or_directory &&
                       c.spans_multiple_independent_hosts && c.p2p_without_host_cpu &&
                       c.snooped_agent_is_caching_host;
            }

            /// 判别式所需的谓词**个数**(5)
            inline constexpr std::size_t kSymmetricPredicateCount = 5;

            // ═══════════════════════════════════════════════════════════════════════════
            // ═══════════════════════════════════════════════════════════════════════════
            //  ④-b 逐格 `basis`(两轴:证据强度 × 钉版强度)
            // ═══════════════════════════════════════════════════════════════════════════
            //
            // ── 两轴规则(照队列 #10 锚点摘要 §8.2)────────────────────────────────────────
            //   **证据强度轴**(`kind`):`public-direct` / `public-analogical` / `self-derived`
            //   **钉版强度轴**(`pending`):`true` / `false` —— 二者**必须拆开**
            //   ⇒ `pending == false` SHALL **同时**满足 ① 来源 `pin=strong`
            //     **且** ② **引用 / 再分发许可已人工确认**(`kSourceLicenseConfirmed`)
            //   ⇒ `kind` **禁止升格**;`ref` **只**指向**锚点摘要条目号**(**不**指向规范原文)
            //
            // ── 来源表(逐来源的「许可已人工确认」)──────────────────────────────────────────
            enum class SourceId : std::uint8_t {
                S2 = 0,     // FMS 2023 Tutorial(公开)—— © CXL Consortium(引用需节制)
                S4 = 1,     // ACM CSUR 2024(arXiv 2306.11227v3)—— PDF 内**查无许可声明**
                S5 = 2,     // ASPLOS'25(arXiv 2410.15908v2)—— **CC-BY 4.0**
                S6 = 3,     // Hot Chips 2022(公开 tutorial)—— © CXL Consortium(引用需节制)
                S7 = 4,     // HCDS 2024(arXiv 2404.03245v1, AMD)—— **CC-BY 4.0**
                Derived = 5 // 派生 / 二手(锚点摘要内的推论)
            };

            inline constexpr std::size_t kSourceCount = 6;

            /// **逐来源**:「引用 / 再分发许可已**人工确认**」—— ⚠️ `S6`(© Consortium)与 `S4`(许可
            /// absent)
            /// **保守取 `false`** ⇒ 它们的格**必须** `pending = true`(待人工确认)。
            inline constexpr std::array<bool, kSourceCount> kSourceLicenseConfirmed{{
                false, // S2 —— © CXL Consortium(引用需节制 ⇒ 待人工确认)
                false, // S4 —— 无许可声明
                true,  // S5 —— CC-BY 4.0
                false, // S6 —— © CXL Consortium(引用需节制 ⇒ 待人工确认)
                true,  // S7 —— CC-BY 4.0
                false  // Derived —— 派生 / 二手
            }};

            /// **证据强度**(禁止升格)
            enum class EvidenceKind : std::uint8_t {
                PublicDirect = 0,     // 公开来源**明确这样写**
                PublicAnalogical = 1, // 跨代次 / 跨协议**类比**
                SelfDerived = 2       // 推理
            };

            /// **逐格 basis** —— `ref` **只**指向锚点摘要条目号
            struct Basis {
                EvidenceKind kind = EvidenceKind::SelfDerived;
                bool pending = true;
                SourceId source = SourceId::Derived;
                const char* ref = nullptr; // 例:"§1 / S5"
            };

            // ── 来源**类型**标签(裁决:甲「类型标签统一 / 清单自治」,per 台账 Entry #266)──
            /// ⚠️ **清单自治**:本载体的来源清单 = **本地 `SourceId`**(自带逐来源许可表);
            ///    全局 `kSources[]` 只作**跨载体统一的类型标签**(`RefKind`)⇒ 两者**不是**同一真源。
            ///    映射:`S2/S4/S5/S6/S7` = **公开非规范来源**(论文 / tutorial)⇒ `public_nonnormative`;
            ///    `Derived` = 本仓推导 ⇒ `repo_derived`。
            ///    ⛔ 本族**恒不出现** `spec_clause`(CXL 规范正文禁读,per `07 号` SU · 四 D-1)
            ///    ⇒ **不计入 B3b**。
            static constexpr RefKind ref_kind_of(const Basis& b) noexcept {
                switch (b.source) {
                    case SourceId::Derived:
                        return RefKind::RepoDerived;
                    default:
                        return RefKind::PublicNonnormative;
                }
            }

            /// `basis` ↔ 来源类型一致性(有牙:`PublicDirect` 指向 `Derived` ⇒ 必红)。
            static constexpr bool basis_ref_kind_consistent(const Basis& b) noexcept {
                const RefKind k = ref_kind_of(b);
                switch (b.kind) {
                    case EvidenceKind::PublicDirect:
                        return k == RefKind::PublicNonnormative && b.source != SourceId::Derived;
                    case EvidenceKind::PublicAnalogical:
                    case EvidenceKind::SelfDerived:
                        return k != RefKind::SpecClause;
                }
                return false;
            }

            /// **basis 有效性**:① `ref` 非空 ② `pending == false` ⇒ 来源许可已人工确认
            constexpr bool basis_is_valid(const Basis& b) noexcept {
                return b.ref != nullptr && b.ref[0] != '\0' &&
                       (b.pending || kSourceLicenseConfirmed[static_cast<std::size_t>(b.source)]);
            }

            /// 逐组 basis 表的**全格**有效性(编译期)
            template <std::size_t N>
            constexpr bool all_basis_valid(const std::array<Basis, N>& t) noexcept {
                for (const auto& b : t) {
                    if (!basis_is_valid(b))
                        return false;
                }
                return true;
            }


            // ── 10 项语义 pending(只能靠规范正文确定 ⇒ 禁止推理填空)───────────────────
            // ⚠️ 与「格的 `pending`(钉版强度)」是**两个层次**:格级 pending =
            // 该格的**证据**可否直接用;
            //    本清单 = **语义本身**尚未确定(只有规范正文能定)⇒ 二者**不得**混谈。
            struct PendingSemanticItem {
                const char* id = nullptr;   // R-S1 ~ R-S10
                const char* what = nullptr; // 一句话摘要
                const char* ref = nullptr;  // 锚点摘要条目号
            };

            inline constexpr std::size_t kPendingSemanticItemCount = 10;

            /// **10 项**语义 pending(per 锚点摘要 §7)—— 逐条登记,供判据逐项寻址
            inline constexpr std::array<PendingSemanticItem, kPendingSemanticItemCount>
                kPendingSemanticItems{
                    {{"R-S1", "opcode 编码 / 字段宽度 / credit 规则 / channel 级排序", "§7 / R-S1"},
                     {"R-S2", "BISnp 消息变体 / BIRsp payload / block 粒度(128B vs 256B)",
                      "§7 / R-S2"},
                     {"R-S3", "DCOH bias 表的精确编码(1 bit vs 2 bit / 第 4 code 是否保留)",
                      "§7 / R-S3"},
                     {"R-S4", "HDM-DB 是替换 bias-based 还是复用 bias table 状态", "§7 / R-S4"},
                     {"R-S5", "home 目录 I/S/E 是否区分 dirty / sharing list 编码 / SF 精度",
                      "§7 / R-S5"},
                     {"R-S6", "transient state 的官方命名与编码 / BI 与 M2S Req 的排序细节",
                      "§7 / R-S6"},
                     {"R-S7", "错误语义(BI timeout / poison / viral / isolation)", "§7 / R-S7"},
                     {"R-S8", "peer 集语义(home 是否 snoop peer device cache)/ GFD 规模",
                      "§7 / R-S8"},
                     {"R-S9", "是否存在 P / pseudo / 厂商扩展态;后续代次是否新增状态", "§7 / R-S9"},
                     {"R-S10", "aggregation_at 二元的规范性依据(已由公开来源反证 ⇒ 待复核)",
                      "§7 / R-S10"}}};

            // ── 28 格逐格 basis(4 + 3 + 3 + 6 + 3 + 4 + 5)──────────────────────────────
            // ⚠️ **唯一真源** —— 锚点摘要**不复制**本表(只提供条目号);测试逐格核对。

            /// ① `DevCacheState` 4 格(M/E/S/I)—— S5(CC-BY)+ S4 ⇒ **许可已确认** ⇒ `pending=false`
            inline constexpr std::array<Basis, kDevCacheStateCount> kBasisDevCacheState{
                {{EvidenceKind::PublicDirect, false, SourceId::S5, "§1 / S5"},
                 {EvidenceKind::PublicDirect, false, SourceId::S5, "§1 / S5"},
                 {EvidenceKind::PublicDirect, false, SourceId::S5, "§1 / S5"},
                 {EvidenceKind::PublicDirect, false, SourceId::S5, "§1 / S5"}}};

            /// ② `DcohViewState` 3 格(I/S/A)—— S6(© Consortium)⇒ `pending=true`
            inline constexpr std::array<Basis, kDcohViewStateCount> kBasisDcohViewState{
                {{EvidenceKind::PublicDirect, true, SourceId::S6, "§1 / S6"},
                 {EvidenceKind::PublicDirect, true, SourceId::S6, "§1 / S6"},
                 {EvidenceKind::PublicDirect, true, SourceId::S6, "§1 / S6"}}};

            /// ③ `HomeDirState` 3 格(I/S/E)—— S4(许可 absent)⇒ `pending=true`
            inline constexpr std::array<Basis, kHomeDirStateCount> kBasisHomeDirState{
                {{EvidenceKind::PublicDirect, true, SourceId::S4, "§1 / S4"},
                 {EvidenceKind::PublicDirect, true, SourceId::S4, "§1 / S4"},
                 {EvidenceKind::PublicDirect, true, SourceId::S4, "§1 / S4"}}};

            /// ④ CXL.mem 6 格通道 —— S4 / S6 ⇒ `pending=true`
            inline constexpr std::array<Basis, kCxlMemChannelCount> kBasisCxlMemChannel{
                {{EvidenceKind::PublicDirect, true, SourceId::S4, "§2 / S4"},
                 {EvidenceKind::PublicDirect, true, SourceId::S4, "§2 / S4"},
                 {EvidenceKind::PublicDirect, true, SourceId::S4, "§2 / S4"},
                 {EvidenceKind::PublicDirect, true, SourceId::S4, "§2 / S4"},
                 {EvidenceKind::PublicDirect, true, SourceId::S6, "§2 / S6"},
                 {EvidenceKind::PublicDirect, true, SourceId::S6, "§2 / S6"}}};

            /// ⑤ CXL.cache 3 格通道 —— S5(CC-BY)⇒ `pending=false`
            inline constexpr std::array<Basis, kCxlCacheChannelCount> kBasisCxlCacheChannel{
                {{EvidenceKind::PublicDirect, false, SourceId::S5, "§2 / S5"},
                 {EvidenceKind::PublicDirect, false, SourceId::S5, "§2 / S5"},
                 {EvidenceKind::PublicDirect, false, SourceId::S5, "§2 / S5"}}};

            /// ⑥ `perm` 投影 4 格 —— **类比**(#10 §3 的投影表)⇒ `public-analogical` +
            /// `pending=true`
            inline constexpr std::array<Basis, kDevCacheStateCount> kBasisPermProjection{
                {{EvidenceKind::PublicAnalogical, true, SourceId::Derived, "§3 / #10 §3"},
                 {EvidenceKind::PublicAnalogical, true, SourceId::Derived, "§3 / #10 §3"},
                 {EvidenceKind::PublicAnalogical, true, SourceId::Derived, "§3 / #10 §3"},
                 {EvidenceKind::PublicAnalogical, true, SourceId::Derived, "§3 / #10 §3"}}};

            /// ⑦ 窄义「对称」5 谓词 —— S2 / S3 / S6 / S4 / S7 ⇒ `pending=true`
            inline constexpr std::array<Basis, kSymmetricPredicateCount> kBasisSymmetricPredicate{
                {{EvidenceKind::PublicDirect, true, SourceId::S2, "§4 / S2"},
                 {EvidenceKind::PublicDirect, true, SourceId::S7, "§4 / S7"},
                 {EvidenceKind::PublicDirect, true, SourceId::S7, "§4 / S7"},
                 {EvidenceKind::PublicDirect, true, SourceId::S4, "§4 / S4"},
                 {EvidenceKind::PublicDirect, true, SourceId::S6, "§4 / S6"}}};

            //  ⑤ 编译期断言(V-1 ~ V-6)
            // ═══════════════════════════════════════════════════════════════════════════

            // ── V-1:3 套状态空间的基数;⚠️ 互斥性由**类型系统**保证(不是按名字)──────────
            static_assert(kDevCacheStateCount == 4,
                          "CDS-01: CXL.cache 缓存代理稳定态 SHALL 恰为 4(M/E/S/I)");
            static_assert(kDcohViewStateCount == 3,
                          "CDS-01: DCOH bias/SF 视图态 SHALL 恰为 3(I/S/A)");
            static_assert(kHomeDirStateCount == 3, "CDS-01: home 目录态 SHALL 恰为 3(I/S/E)");
            // ⚠️ 三套**各自**都含名为 `I` / `S` 的元素 ⇒ 「互斥」**不是**按名字,而是靠 enum class
            // 强类型。
            //    ⚠️ **枚举值不承载跨域语义** —— 同名元素的枚举值**可能相同**(如 `DcohViewState::S`
            //    与 `HomeDirState::S` 皆为 1)⇒ **不得**把「同名元素值必不同」当判据(实施期已实测)。
            static_assert(!std::is_convertible_v<DevCacheState, DcohViewState> &&
                              !std::is_convertible_v<DcohViewState, DevCacheState>,
                          "CDS-01: 缓存代理域与 DCOH 视图域 SHALL "
                          "是**互不转换**的强类型(同名元素跨域存在是预期的)");
            static_assert(!std::is_convertible_v<DevCacheState, HomeDirState> &&
                              !std::is_convertible_v<HomeDirState, DevCacheState>,
                          "CDS-01: 缓存代理域与 home 目录域 SHALL 是**互不转换**的强类型");

            // ── V-2:通道基数(枚举名;不写死 opcode)──────────────────────────────────
            static_assert(kCxlMemChannelCount == 6,
                          "CDS-02: CXL.mem 通道 SHALL 恰为 6(4 基础 + BISnp + BIRsp)");
            static_assert(kCxlCacheChannelCount == 3,
                          "CDS-02: CXL.cache 通道 SHALL 恰为 3(每方向 Req/Rsp/Data)");
            static_assert(kCxlMemChannelDirection.size() == kCxlMemChannelCount,
                          "CDS-02: 通道方向表 SHALL 与通道表等长");

            // ── V-3:⭐ 对称判别式 ────────────────────────────────────────────────────
            static_assert(kDeviceInitiatedSnoopSet.size() == 1,
                          "CDS-03: 设备发起的 snoop 集合 SHALL 恰含 1 类(⛔ 禁止沿用队列 #10 "
                          "的「空集」形态)");
            static_assert(kDeviceInitiatedSnoopSet[0] == CxlMemChannel::S2M_BISnp,
                          "CDS-03: 设备发起的 snoop SHALL 是 BISnp");
            static_assert(
                kCxlMemChannelDirection[static_cast<std::size_t>(CxlMemChannel::S2M_BISnp)] ==
                    ChannelDirection::Local,
                "CDS-03: BISnp SHALL 是**设备发起**(local)");

            // ── V-4:`perm` 只从缓存代理域投影(4→4)─────────────────────────────────
            static_assert(kPermValueCount == 4, "CDS-04: `perm` SHALL 维持 4 值锁(不新增第 5 值)");
            static_assert(kPermProjection.size() == kDevCacheStateCount,
                          "CDS-04: `perm` 投影行数 SHALL == 缓存代理态数");
            static_assert(HasPermOf<DevCacheState>::value,
                          "CDS-04: 缓存代理域 SHALL 可投影到 `perm`");
            static_assert(!HasPermOf<DcohViewState>::value,
                          "CDS-04: DCOH 视图域 SHALL **不得**能投影到 `perm`(它是 tracker view / "
                          "coherence metadata)");
            static_assert(!HasPermOf<HomeDirState>::value,
                          "CDS-04: home 目录域 SHALL **不得**能投影到 `perm`");
            static_assert(kDcohViewIsSetLike,
                          "CDS-04: DCOH 视图的 `A` SHALL 登记为**集合**(不可单值投影)");

            // ── V-5:窄义「对称」5 谓词 ──────────────────────────────────────────────
            static_assert(kSymmetricPredicateCount == 5,
                          "CDS-05: 窄义「对称」SHALL 恰为 5 谓词合取");


            // ── V-6:逐格 basis(CDS-06)—— 全格有效 + 覆盖完整 ─────────────────────────
            static_assert(
                all_basis_valid(kBasisDevCacheState),
                "CDS-06: DevCacheState 逐格 basis SHALL 全格有效(ref 非空 + pending 规则)");
            static_assert(all_basis_valid(kBasisDcohViewState),
                          "CDS-06: DcohViewState 逐格 basis SHALL 全格有效");
            static_assert(all_basis_valid(kBasisHomeDirState),
                          "CDS-06: HomeDirState 逐格 basis SHALL 全格有效");
            static_assert(all_basis_valid(kBasisCxlMemChannel),
                          "CDS-06: CXL.mem 通道逐格 basis SHALL 全格有效");
            static_assert(all_basis_valid(kBasisCxlCacheChannel),
                          "CDS-06: CXL.cache 通道逐格 basis SHALL 全格有效");
            static_assert(all_basis_valid(kBasisPermProjection),
                          "CDS-06: `perm` 投影逐格 basis SHALL 全格有效");
            static_assert(all_basis_valid(kBasisSymmetricPredicate),
                          "CDS-06: 「对称」谓词逐格 basis SHALL 全格有效");

            // 覆盖完整性:每组的 basis 数 SHALL == 该组的项数(逐格,不留空洞)
            static_assert(kBasisDevCacheState.size() == kDevCacheStateCount,
                          "CDS-06: DevCacheState 的 basis 数 SHALL == 态数");
            static_assert(kBasisDcohViewState.size() == kDcohViewStateCount,
                          "CDS-06: DcohViewState 的 basis 数 SHALL == 态数");
            static_assert(kBasisHomeDirState.size() == kHomeDirStateCount,
                          "CDS-06: HomeDirState 的 basis 数 SHALL == 态数");
            static_assert(kBasisCxlMemChannel.size() == kCxlMemChannelCount,
                          "CDS-06: CXL.mem 通道的 basis 数 SHALL == 通道数");
            static_assert(kBasisCxlCacheChannel.size() == kCxlCacheChannelCount,
                          "CDS-06: CXL.cache 通道的 basis 数 SHALL == 通道数");
            static_assert(kBasisPermProjection.size() == kDevCacheStateCount,
                          "CDS-06: `perm` 投影的 basis 数 SHALL == 投影行数");
            static_assert(kBasisSymmetricPredicate.size() == kSymmetricPredicateCount,
                          "CDS-06: 「对称」谓词的 basis 数 SHALL == 谓词数");

            // ⚠️ `pending == false` ⇒ 其来源的许可**必须**已人工确认(两轴规则的编译期形态)
            static_assert(kSourceLicenseConfirmed[static_cast<std::size_t>(SourceId::S5)],
                          "CDS-06: S5(ASPLOS'25) SHALL 是 CC-BY 4.0 ⇒ 许可已确认");
            static_assert(kSourceLicenseConfirmed[static_cast<std::size_t>(SourceId::S7)],
                          "CDS-06: S7(HCDS 2024) SHALL 是 CC-BY 4.0 ⇒ 许可已确认");
            static_assert(
                !kSourceLicenseConfirmed[static_cast<std::size_t>(SourceId::S4)],
                "CDS-06: S4(许可 absent) SHALL **不**标许可已确认 ⇒ 其格 SHALL pending=true");
            ;
            static_assert(!kSourceLicenseConfirmed[static_cast<std::size_t>(SourceId::S6)],
                          "CDS-06: S6 SHALL 不标许可已确认 ⇒ 其格 SHALL pending=true");


            // ── V-7:10 项语义 pending 的登记完整性(CDS-07)────────────────────────────
            static_assert(kPendingSemanticItemCount == 10,
                          "CDS-07: 语义 pending 项 SHALL 恰为 10(R-S1 ~ R-S10)");
            static_assert(kPendingSemanticItems.size() == kPendingSemanticItemCount,
                          "CDS-07: 语义 pending 清单长度 SHALL == 声明数");

            constexpr bool all_pending_items_wellformed(
                const std::array<PendingSemanticItem, kPendingSemanticItemCount>& items) noexcept {
                for (const auto& it : items) {
                    if (it.id == nullptr || it.id[0] == '\0')
                        return false;
                    if (it.what == nullptr || it.what[0] == '\0')
                        return false;
                    if (it.ref == nullptr || it.ref[0] == '\0')
                        return false;
                }
                return true;
            }

            static_assert(all_pending_items_wellformed(kPendingSemanticItems),
                          "CDS-07: 10 项语义 pending SHALL 逐项带 id / what / ref");

        } // namespace cxl_dcoh
    } // namespace cep
} // namespace tlm

#endif // TLM_CEP_CXL_DCOH_STATE_SPEC_HH
