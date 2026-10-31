// =============================================================================
// include/tlm/cep/cxl_state_spec.hh
//   —— CXL 2.0 非对称(CXL.cache · host-centric)规约载体
//
// 作者: CppTLM Team / 日期: 2026-11-01
// 配套: docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md(basis 唯一人工审计面)
//       openspec/changes/cpptlm-cep-cxl-nonsymmetric-state-spec/(change 4 件套)
//       台账 obj-cep-platform.md Entry #122(口径裁决)· #123(change 转正)
//       docs/adr/ADR-CEP-11-protocol-version-locking.md §Status Update(2026-11-01)
//
// ── 定位 ───────────────────────────────────────────────────────────────────
// **不是缓存、不是网络组件**:它不接 `connections[]`、不进 `ModuleFactory`、不注册 ABI。
// 它是 **协议语义的规约真源**:稳定态集合 / 事件词表 / `perm` 投影 / 秩口径 /
// 逐格 `basis`(Phase 2b)⇒ 供第 1 个 change 的 `CxlCacheAdapter` **零自有表副本**地委托消费。
//
// ── 口径(per 台账 Entry #122 + ADR-CEP-11 Status Update,7 条)────────────────
// ① 「CXL 2.0 非对称」= **CXL.cache** 子协议(**host-centric / 单一一致性点**):
//    设备以 MESI 缓存 Host 内存;Host 侧 Home / Snoop Filter 为**唯一**一致性点;
//    **H2D 承载 snoop**;设备侧**无** snoop **发起**型事件。
//    **显式不含** CXL.mem 池化语义与 CXL 3.0 DCOH / BISnp / BIRsp。
// ② 设备侧稳定态 = **MESI 4 态**(M/E/S/I)⇒ `perm` 投影 = **4→4**。
//    ⚠️ **「5→4」是 MESIF 模板形状的未审视继承**(MESIF 有 `F` 态)⇒ CXL **无 `F` 态**
//    ⇒ 本载体**不含** `F` 态、**不含** `F→WRITEBACK` 投影行、**不含** `F→E` 秩回落例外。
// ③ 「非对称」的精确表述 = **host-centric / 单一一致性点**。
//    ⚠️ 「设备互不感知其他设备」在 CXL 2.0 **平凡成立**(每 root port 只 1 个 cache device)
//    ⇒ **不得**把它当作 CXL 2.0 的区分性语义。
// ④ basis 权威 = **本仓「公开来源锚点摘要」** —— **既非 CXL 规范正文、亦非 ArchForge**。
// ⑤ ⛔ **合规禁令**:CXL 规范受 Evaluation Copy Agreement §3 + AI Acceptable Use Policy
//    约束 ⇒ **禁止**把规范正文投喂任何 AI ⇒ 本载体 basis **只来自公开非规范来源**。
// ⑥ CXL 实例 = **CEP 探索实例** —— 本平台建模的 CXL 语义
//    **≠ dGPU SoC 的实现路径**(per ArchForge `ADR-SOC-21` D2:「GPU Die 上**严禁**
//    包含任何 CXL PHY 或 CXL Controller」;该仓的 CXL 转换由**外部** Scale-Up Switch 黑盒完成)。
//    ⇒ 本载体**不镜像**进 ArchForge `32 号 §6.1`(那是**产品侧**实例注册表)。
// ⑦ ⚠️「CXL 3.0 对称」指 **HDM-DB / CXL.mem 的多 host 共享内存**,
//    **不是** CXL.cache 变对称 ⇒ **CXL.cache 全代次都非对称**。
//
// ── 阶段 ───────────────────────────────────────────────────────────────────
// **Phase 2a(本文件当前内容)**:结构层 —— 态 / 事件词表 / `perm` 投影 / 秩口径 /
//   **非对称结构级编译期断言** / 命名函数 / 头注释消歧。
// **Phase 2b(待补)**:转换表 + **逐格 `basis`** —— ⚠️ **阻塞于 Phase 1 钉版补齐**
//   (sha256 / Wayback 快照 / D14 commit SHA)⇒ 逐格 `ref` **必须**指向锚点摘要条目号。
//   ⇒ 在 Phase 2b 落地前,本载体**不提供** `transition()` ⇒ 避免半成品 API 被误用。
//
// ── 上游契约(只消费,不重推)──────────────────────────────────────────────────
// · `perm` 4 值锁:`NONE` / `READ` / `UNIQUE` / `WRITEBACK`(per `ADR-CEP-02` CEN-Ib11)
// · `SparseSnoopFilter` 的 caps 与 hazard:`ADR-CEP-04 §2` / `§4`(与本载体**无关**,仅记录)
// =============================================================================

#ifndef CPPTLM_CEP_CXL_STATE_SPEC_HH
#define CPPTLM_CEP_CXL_STATE_SPEC_HH

#include <array>
#include <cstddef>
#include <cstdint>

namespace tlm::cep {

/// CXL 2.0 非对称(CXL.cache)规约载体 —— **Phase 2a 结构层**。
///
/// ⚠️ 消费方纪律:adapter SHALL **零自有表副本**(委托消费本载体)⇒ 规约改一格、adapter 必红。
class CxlStateSpec {
public:
    // ── 设备侧稳定态(**恰 4 态**,per 口径 ②)────────────────────────────────
    /// ⚠️ **无 `F` 态**(CXL.cache 公开口径只有 M/E/S/I)⇒ 与 MESIF 载体**形态不同构**。
    enum class State : std::uint8_t { I = 0, S = 1, E = 2, M = 3 };

    // ── 事件词表 ────────────────────────────────────────────────────────────
    /// ⚠️ **枚举名表达,SHALL NOT 写死 opcode 编码** —— 编码 / 字段宽度 / credit 规则
    /// **公开来源不可得**(残余风险 R-1)⇒ 载体不建模编码。
    ///
    /// 划分(per 口径 ① + 锚点摘要 §2):
    /// · **local** = D2H 侧**设备发起**的操作(设备请求 / evict / 写);
    /// · **remote** = **仅** H2D snoop 操作码(发起方**恒为** Home)。
    enum class Event : std::uint8_t {
        // ── local:D2H Req(设备请求)──────────────────────────────────────
        RdShared = 0,  ///< 读共享(可进 S)
        RdCurr,        ///< 只取当前值(不改态)
        RdOwn,         ///< 取所有权(grant 为 E **或** M)
        RdAny,         ///< 取所有权(功能已被 RdOwn + RdShared 覆盖)
        RdOwnNoData,   ///< 只要所有权不要数据(必须收 GO-E)
        CLFlush,       ///< cache line flush
        CacheFlushed,  ///< cache flushed 通知
        ItoMWr,        ///< 写(M 态)
        WrCur,         ///< 当前值写
        WOWrInv,       ///< 写 + 使其它 sharer 失效
        WrInv,         ///< 写 + 失效(带 F 语义变体)
        // ── local:D2H Req(驱逐)────────────────────────────────────────
        DirtyEvict,    ///< 脏驱逐(回写)
        CleanEvict,    ///< 净驱逐
        CleanEvictNoData, ///< 净驱逐(无数据)
        // ── remote:H2D Req(snoop,**仅** Home 发起)─────────────────────────
        SnpInv,        ///< 失效 snoop(→ I,脏则回数据)
        SnpData,       ///< 数据 snoop(→ S,脏则回数据)
        SnpCurr,       ///< 当前值 snoop(**不改态**,只回当前值 + 脏数据)
    };

    /// `perm` 4 值锁(per `ADR-CEP-02` CEN-Ib11)—— **只复用,不扩**。
    enum class Perm : std::uint8_t { NONE = 0, READ = 1, UNIQUE = 2, WRITEBACK = 3 };

    // ── `basis.kind` **封闭枚举**(4 档,per 口径 ④)──────────────────────────
    /// ⚠️ **强度不得升格**:`public-analogical` / `self-derived` ⇒ `pending = true`。
    enum class BasisKind : std::uint8_t {
        PublicDirect = 0,   ///< 公开来源**明确这样写**(强度 direct)
        PublicAnalogical,   ///< 跨协议 / 跨代次**类比**(强度 analogical)⇒ `pending = true`
        ArchforgeAnchor,    ///< **仅** caps 等**已逐字段对齐**的格(per `ADR-CEP-04 §2`)
        SelfDerived,        ///< **本仓推导** ⇒ `pending = true`
    };

    // ── 规模常量(编译期锁的锚)──────────────────────────────────────────────
    static constexpr std::size_t kStateCount = 4;   ///< **恰 4**(口径 ②)
    static constexpr std::size_t kPermCount = 4;    ///< 4 值锁(不扩)
    static constexpr std::size_t kRemoteEventCount = 3; ///< H2D snoop 操作码数
    static constexpr std::size_t kLocalEventCount = 14; ///< D2H 侧设备发起的事件数

    /// 全部 **remote**(H2D snoop)事件 —— **仅**这三个(口径 ①)。
    static constexpr std::array<Event, kRemoteEventCount> kRemoteEvents{
        Event::SnpInv, Event::SnpData, Event::SnpCurr};

    /// 全部 **local**(D2H 设备侧)事件。
    static constexpr std::array<Event, kLocalEventCount> kLocalEvents{
        Event::RdShared, Event::RdCurr, Event::RdOwn, Event::RdAny, Event::RdOwnNoData,
        Event::CLFlush, Event::CacheFlushed, Event::ItoMWr, Event::WrCur,
        Event::WOWrInv, Event::WrInv, Event::DirtyEvict, Event::CleanEvict,
        Event::CleanEvictNoData};

    // ── `perm` 投影:**4→4**(口径 ②)────────────────────────────────────────
    /// ⚠️ **与 MESIF 投影表的前 4 行逐格相同**;**`F→WRITEBACK` 行不存在**。
    static constexpr std::array<Perm, kStateCount> kPermProjection{
        Perm::NONE,      // I
        Perm::READ,      // S
        Perm::UNIQUE,    // E
        Perm::WRITEBACK, // M
    };

    // ── 秩口径:**只增不减**,**无例外**(CXL 无 `F` 态 ⇒ 无 `F→E` 秩回落)──────
    static constexpr std::uint8_t perm_rank(Perm p) noexcept {
        return static_cast<std::uint8_t>(p);
    }

    /// 是否为**静默升级**(跨域可见写):仅 `E + 写事件 → M`。
    /// ⚠️ 该升级 SHALL 落 `perm_upgrades.json`(per `ADR-CEP-12 §11`)—— 属第 1 个 change。
    static constexpr bool is_silent_upgrade(State s, Event e) noexcept {
        return s == State::E && e == Event::ItoMWr;
    }

    // ── local / remote 判定(口径 ① 的**结构级**落点)────────────────────────
    /// **remote** = 仅 H2D snoop(发起方恒为 Home)。
    static constexpr bool is_remote_event(Event e) noexcept {
        return e == Event::SnpInv || e == Event::SnpData || e == Event::SnpCurr;
    }

    /// **local** = D2H 侧设备发起。
    static constexpr bool is_local_event(Event e) noexcept {
        return !is_remote_event(e);
    }

    // ── 命名函数(供错误消息 / 核对表生成器用)────────────────────────────────
    static constexpr const char* state_name(State s) noexcept {
        switch (s) {
            case State::I: return "I";
            case State::S: return "S";
            case State::E: return "E";
            case State::M: return "M";
        }
        return "?";
    }

    static constexpr const char* event_name(Event e) noexcept {
        switch (e) {
            case Event::RdShared: return "RdShared";
            case Event::RdCurr: return "RdCurr";
            case Event::RdOwn: return "RdOwn";
            case Event::RdAny: return "RdAny";
            case Event::RdOwnNoData: return "RdOwnNoData";
            case Event::CLFlush: return "CLFlush";
            case Event::CacheFlushed: return "CacheFlushed";
            case Event::ItoMWr: return "ItoMWr";
            case Event::WrCur: return "WrCur";
            case Event::WOWrInv: return "WOWrInv";
            case Event::WrInv: return "WrInv";
            case Event::DirtyEvict: return "DirtyEvict";
            case Event::CleanEvict: return "CleanEvict";
            case Event::CleanEvictNoData: return "CleanEvictNoData";
            case Event::SnpInv: return "SnpInv";
            case Event::SnpData: return "SnpData";
            case Event::SnpCurr: return "SnpCurr";
        }
        return "?";
    }

    static constexpr const char* basis_kind_name(BasisKind k) noexcept {
        switch (k) {
            case BasisKind::PublicDirect: return "public-direct";
            case BasisKind::PublicAnalogical: return "public-analogical";
            case BasisKind::ArchforgeAnchor: return "archforge-anchor";
            case BasisKind::SelfDerived: return "self-derived";
        }
        return "?";
    }

    /// `basis.kind` / `pending` 的**双向一致**判据(口径 ④)。
    /// ⚠️ `public-analogical` 与 `self-derived` ⇒ **必须** `pending = true`
    ///    (强度天花板 ⇒ 不得当已裁决)。
    static constexpr bool basis_pending_consistent(BasisKind k, bool pending) noexcept {
        if (k == BasisKind::PublicAnalogical || k == BasisKind::SelfDerived) {
            return pending;
        }
        return !pending;  // PublicDirect / ArchforgeAnchor:复核通过即 false
    }

    // ── 结构自检(编译期锁)──────────────────────────────────────────────────
    /// ① 态集合**恰 4** ② 投影**恰 4 行** ③ `F` 态**不存在**
    /// ④ remote 事件集**恰为** 3 个 snoop ⑤ **设备侧无** snoop 发起型事件
    /// ⑥ local ∩ remote = ∅ ⑦ remote 集**无**重复
    /// **可传入集合**的结构校验 —— ⚠️ **反向 fixture 的载体**
    /// (判据铁律 ③:不得只以「编译红」充当 ctest 判据 ⇒ 须有**运行时**可判形态)。
    ///
    /// 校验 6 条:① 态数 **恰 4** ② 投影行数 == 态数 ③ 投影**逐格** =
    /// {NONE, READ, UNIQUE, WRITEBACK} ④ remote 集**恰 3** 且**无重复**
    /// ⑤ remote 集**全部**是 `is_remote_event` ⑥ local 集**不含**任何 remote 事件。
    template <std::size_t NS, std::size_t NE, std::size_t NL>
    static constexpr bool structure_is_valid_for(
        const std::array<State, NS>& states,
        const std::array<Event, NE>& remote_events,
        const std::array<Event, NL>& local_events,
        const std::array<Perm, NS>& projection) noexcept {
        // ① 态数恰 4
        if (states.size() != 4) {
            return false;
        }
        // ② 投影行数 == 态数
        if (projection.size() != states.size()) {
            return false;
        }
        // ③ 投影逐格
        if (projection[0] != Perm::NONE || projection[1] != Perm::READ ||
            projection[2] != Perm::UNIQUE || projection[3] != Perm::WRITEBACK) {
            return false;
        }
        // ④ remote 集恰 3 且无重复
        if (remote_events.size() != 3) {
            return false;
        }
        for (std::size_t i = 0; i < remote_events.size(); ++i) {
            for (std::size_t j = i + 1; j < remote_events.size(); ++j) {
                if (remote_events[i] == remote_events[j]) {
                    return false;
                }
            }
        }
        // ⑤ remote 集全部是 remote
        for (const auto e : remote_events) {
            if (!is_remote_event(e)) {
                return false;
            }
        }
        // ⑥ local ∩ remote = ∅
        for (const auto e : local_events) {
            if (is_remote_event(e)) {
                return false;
            }
        }
        return true;
    }

    /// 本载体自身的结构自检(编译期锁 + 运行期返回)。
    static constexpr bool structure_is_valid() noexcept {
        // ② 投影行数 == 态数
        static_assert(kPermProjection.size() == kStateCount, "投影行数 SHALL == 态数");
        // ①③ 态枚举**不得**含 `F`(值 4 不得出现)
        static_assert(static_cast<std::uint8_t>(State::M) == 3, "态 SHALL 恰为 I/S/E/M(无 F)");
        static_assert(kStateCount == 4, "CXL.cache 稳定态 SHALL 恰为 4");
        // ⑤ 设备侧无 snoop 发起型事件:三个 snoop **全部**是 remote
        static_assert(is_remote_event(Event::SnpInv) &&
                      is_remote_event(Event::SnpData) &&
                      is_remote_event(Event::SnpCurr),
                      "三个 snoop SHALL 全部是 remote(H2D,发起方恒为 Home)");
        // ④⑦ remote 集恰 3 且无重复
        static_assert(kRemoteEvents.size() == 3, "remote 事件 SHALL 恰为 3 个 snoop");
        static_assert(kRemoteEvents[0] != kRemoteEvents[1] &&
                      kRemoteEvents[1] != kRemoteEvents[2] &&
                      kRemoteEvents[0] != kRemoteEvents[2],
                      "remote 事件集 SHALL 无重复");
        // ⑥ local ∩ remote = ∅
        for (const auto e : kLocalEvents) {
            if (is_remote_event(e)) {
                return false;
            }
        }
        return true;
    }

    /// transient 态**不建模**(残余风险 R-3:公开来源明说其「not officially part of
    /// the CXL.cache specification」)⇒ 计数恒 0。
    static constexpr std::size_t kTransientStateCount = 0;

    // ── 残余风险登记(8 项,per 锚点摘要 §6 / delta spec AC-6)───────────────
    static constexpr std::size_t kResidualRiskCount = 8;
    static constexpr std::array<const char*, kResidualRiskCount> kResidualRisks{
        "R-1 opcode 编码 / 字段宽度 / credit 规则(载体不建模)",
        "R-2 request 权威枚举与逐状态合法性表(公开 deck 间有差异)",
        "R-3 transient 态命名(载体不建模)",
        "R-4 错误语义(GO-Err / poison / viral)",
        "R-5 SF completeness 规范性判定(仅 analogical)",
        "R-6 版本记账(CacheID / 16 device / 68B vs 256B)",
        "R-7 伪态 / 厂商扩展态的存在性(「搜不到」≠「不存在」)",
        "R-8 HDM-DB / BISnp / BIRsp 细节(队列 #12)",
    };

    /// 头注释消歧的**可执行**副本(判据 AC-5 的正向锚 + 反向 fixture 的毒化点)。
    static constexpr const char* kDisambiguation =
        "CXL 实例 = CEP 探索实例;本平台建模的 CXL 语义 != dGPU SoC 的实现路径";

    // ── 头注释消歧的机器检查(判据 AC-5)────────────────────────────────────
    /// 载体 SHALL 含「≠ dGPU SoC 实现路径」的消歧声明 ⇒ 机器可校。
    static constexpr bool has_disambiguation(const char* text) noexcept {
        return contains(text, "dGPU SoC") && contains(text, "!=");
    }

    /// 简易子串匹配(载体自包含,不引入 std::string 依赖)。
    static constexpr bool contains(const char* hay, const char* needle) noexcept {
        if (hay == nullptr || needle == nullptr) {
            return false;
        }
        for (const char* h = hay; *h != '\0'; ++h) {
            const char* a = h;
            const char* b = needle;
            while (*a != '\0' && *b != '\0' && *a == *b) {
                ++a;
                ++b;
            }
            if (*b == '\0') {
                return true;
            }
        }
        return false;
    }
};

}  // namespace tlm::cep

#endif  // CPPTLM_CEP_CXL_STATE_SPEC_HH
