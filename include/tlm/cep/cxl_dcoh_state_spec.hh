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
                M2S_Req = 0,   // memory → host:request
                M2S_RwD = 1,   // memory → host:read-with-ownership-data
                S2M_NDR = 2,   // host → memory:non-data-request
                S2M_DRS = 3,   // host → memory:data-response
                S2M_BISnp = 4, // host → memory:back-invalidate snoop  ⭐ CXL 3.0 新增
                M2S_BIRsp = 5  // memory → host:back-invalidate response(CXL 3.0 新增)
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

        } // namespace cxl_dcoh
    } // namespace cep
} // namespace tlm

#endif // TLM_CEP_CXL_DCOH_STATE_SPEC_HH
