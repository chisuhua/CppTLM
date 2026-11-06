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

#include "tlm/cep/source_registry.hh"

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
        RdAny,         ///< 取所有权(⚠️ 与 `RdOwn` 的**关系无公开来源** —— 摘要 §2 只列名不释义,
                       ///<   ⇒ **不得**断言「已被 RdOwn + RdShared 覆盖」(该断言原无出处,2026-11-01 删除))
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
    /// 事件总数 = 14 local + 3 remote ⇒ 转换矩阵 = `kEventCount × kStateCount` = **68 格**
    static constexpr std::size_t kEventCount = kLocalEventCount + kRemoteEventCount;
    static constexpr std::size_t kTransitionCellCount = kEventCount * kStateCount;

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
    /// 稳定态 → 4 值 `perm` 投影(**访问器**)。
    /// ⚠️ **本访问器是「实现层零副本」的前置条件**(2026-11-01 增,per 队列 #10 实现层):
    ///    adapter 若直接读 `kPermProjection[...]` 就必须**指名**该表名 ⇒ 违「零自有表副本」
    ///    的静态扫描(禁止记号含表名)⇒ 故提供**函数**形态的访问面,adapter 只调函数。
    /// ⚠️ **纯访问器,零语义变更**(与 `kPermProjection` 逐行相同)⇒ 已归档的
    ///    `cep-cxl-nonsymmetric-state-spec` 的 CSS-01 ~ CSS-06 **不受影响**。
    static constexpr bool perm_of(State s, Perm& out) noexcept {
        const auto v = kPermProjection[static_cast<std::uint8_t>(s)];
        out = v;
        return true;  // 4 态**全部**有投影(CXL 无瞬态 ⇒ 无「无投影」情形)
    }

    static constexpr std::uint8_t perm_rank(Perm p) noexcept {
        return static_cast<std::uint8_t>(p);
    }

    /// **写事件类**(= 摘要 §4 的 `local write` 类:`WrInv` / `WOWrInv` / `ItoMWr`)。
    /// ⚠️ **类级判定,不是单 opcode** —— 摘要 §4 锚的是**事件类**;无任何公开来源把写语义
    ///    钉到单一 opcode(残余 R-2)⇒ 单 opcode 版(`e == ItoMWr`)会把类级证据
    ///    **越证成**单 opcode 事实(2026-11-01 修正,per oracle 复核)。
    /// ⚠️ **`WrCur` 存疑**:其名指示写语义,但摘要 §4 的 `local write` 类**未收**它
    ///    ⇒ 本函数**不收**,该缺口登记于 R-2(「不保证与规范权威枚举等量」)⇒ 待重锚。
    static constexpr bool is_write_event(Event e) noexcept {
        return e == Event::WrInv || e == Event::WOWrInv || e == Event::ItoMWr;
    }

    /// 是否为**静默升级**(跨域可见写:`E + 写事件 → M`)。
    /// ⚠️ **basis = `public-analogical` + `pending = true`**:「`E` 蕴含无其他 sharer ⇒
    ///    就地改 `M` 不产生跨域可见性」是**MESI 通用性质**,公开来源**未逐字断言该格**
    ///    (摘要 §4 已相应降为 `analogical`)⇒ **不得当作已裁决**;Phase 2b SHALL 尝试在
    ///    D1(字节已在本地)重锚,重锚不出则维持 `pending`。
    /// ⚠️ 该升级 SHALL 落 `perm_upgrades.json`(per `ADR-CEP-12 §11`)—— 属第 1 个 change。
    static constexpr bool is_silent_upgrade(State s, Event e) noexcept {
        return s == State::E && is_write_event(e);
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

    // ══════════════════════════════════════════════════════════════════════
    //  转换矩阵(Phase 2b)—— **三态格语义**,4 态 × 17 事件 = 68 格
    // ══════════════════════════════════════════════════════════════════════
    //
    // ⚠️ **二态(合法/未知)不采用**(per `design.md D-γbis`):若用「未定义空洞值」表达「未知」,
    //    消费者会把**未知**读成**非法** ⇒ 这**本身是一处未实测断言**(「我断言此转换非法」≠「我不知道」)
    //    ⇒ ⇒ 每格 SHALL ∈ {`Known`, `Illegal`, `Unknown`},三者**语义不同**。
    //
    // ⚠️ **`Illegal` 当前恒 0 格** —— 不是「没查」,是**来源不给**:
    //    D1 p.7 证实规范**存在**这类约束(`certain requests are only legal when the device's cache
    //    is in a certain state`)但**不给具体是哪些**;逐状态合法性表在规范 §3.2.5.15(R-2,禁读)
    //    ⇒ **填任何 `Illegal` 都是杜撰** ⇒ 一律留 `Unknown` + `R-2` 注记。
    //
    // ⚠️ **`E` 列的 D1 证据量为零**(D1 把 E 并入 M:`StableState def = {M,S,I}` 闭枚举 + 瞬时态无 `E` 前缀)
    //    ⇒ `E` 格只接受**态无关**义务(如 `SnpInv ⇒ 降 I`)或**独立 E 证据**;
    //    **严禁**把 D1 的 `M` 规则复制成 `E` 规则。
    //
    // ⚠️ **D2 无许可声明**(`pin=strong` 但 license `absent`)⇒ 仅靠 D2 支撑的格维持 `pending = true`
    //    (现为 `CleanEvict` / `CleanEvictNoData` 的 `E` 格)。
    //
    // ⚠️ 初始化器一律用**聚合字面量**而非辅助函数调用 —— 类内 `constexpr` 成员函数在类
    //    **未完成**时不可作常量求值(GCC:`called in a constant expression before its
    //    definition is complete`)⇒ 这不是风格选择,是语言限制。

    /// 格的**四态语义**(⚠️ 不是二态;`Free` 为 change cpptlm-cep-basis-source-kind-and-cell-free 新增)。
    enum class CellKind : std::uint8_t {
        Known = 0,    ///< 已知**目标态**(强度由 `basis` 承载,弱证据可 `pending = true`)
        Illegal = 1,  ///< 已知**不合法**(须有证据 —— 当前恒 0 格)
        Unknown = 2,  ///< **不知道**(不是非法)⇒ 消费者 SHALL 走降级路径,SHALL NOT 当作 `Illegal`
        Free = 3,     ///< 规范**明确**「实现定义 / 自由选择」⇒ 消费者 SHALL 走**合法**降级
    };

    /// 单格。
    struct Transition {
        CellKind kind;
        State target;        ///< **仅 `kind == Known` 时有意义**
        BasisKind basis;
        bool pending;
        const char* ref;     ///< 锚点摘要条目号(如 `D1 p.5 Fig.4`)—— SHALL 不指向规范原文
        const char* note;    ///< 缺失项(`R-1`~`R-8` 编号)或口径说明
    };

    // ── 来源类型解析(change cpptlm-cep-basis-source-kind-and-cell-free)──────
    /// `D1 …` / `D2 …`(公开非规范论文)⇒ 对应登记 id;空 / `R-n`(缺失项编号)⇒ 本仓推导。
    /// ⚠️ `PublicDirect` 在本载体语义 = **公开来源明确这样写** ⇒ 映射 `public_nonnormative`,
    ///    ⛔ **不是** `spec_clause`(CXL 规范正文禁读,per `07 号` SU · 四 D-1)。
    static constexpr SourceResolution resolve_ref(const Transition& t) noexcept {
        if (t.ref == nullptr || t.ref[0] == '\0') {
            return resolve("repo");
        }
        if (starts_with(t.ref, "D1")) {
            return resolve("cxl-d1");
        }
        if (starts_with(t.ref, "D2")) {
            return resolve("cxl-d2");
        }
        return resolve("repo");
    }

    static constexpr bool basis_ref_kind_consistent(const Transition& t) noexcept {
        const SourceResolution r = resolve_ref(t);
        if (!r.registered) {
            return false;
        }
        switch (t.basis) {
            case BasisKind::PublicDirect:
                return r.kind == RefKind::PublicNonnormative;
            case BasisKind::PublicAnalogical:
                return r.kind != RefKind::SpecClause;
            case BasisKind::ArchforgeAnchor:
                return r.kind == RefKind::RepoDerived;
            case BasisKind::SelfDerived:
                // `SelfDerived` = 本仓推导;`ref` 指向**推导依据**的来源(可为公开非规范
                // 论文,如 CXL 的 D1 结构性推断)⇒ 只禁止冒充规范条款级。
                return r.kind != RefKind::SpecClause;
        }
        return false;
    }

    /// **行序 SHALL 与 `Event` 枚举值一致**(index = 枚举值)—— 下方 `static_assert` 锁。
    ///
    /// 强度口径(实测 2026-11-01):`Known` **26** 格 · `Illegal` **0** 格 · `Unknown` **42** 格;
    /// 其中 `pending = false` 的 **22** 格,**全部为 D1**(CC-BY 4.0);余 4 格 `pending = true`:
    /// `RdOwn` / `RdOwnNoData` 的 `S` 列(`self-derived` 结构性推断)·
    /// `CleanEvict` / `CleanEvictNoData` 的 `E` 列(仅 D2 分类式表述,且 **D2 许可 absent**)。
    static constexpr std::array<std::array<Transition, kStateCount>, kEventCount> kTransitions{{
        // 0 RdShared
        {{Transition{CellKind::Known, State::S, BasisKind::PublicDirect, false, "D1 p.5 Fig.4 InvalidLoad1", "I + Load ⇒ 发 RdShared;态 := ISAD ⇒ 完成后 S"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: S 态读命中应静默无消息(可能根本没有规则 ⇒ 不可读作 Illegal)"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上;D1 合并 E/M ⇒ E 无独立证据"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"}}},
        // 1 RdCurr
        {{Transition{CellKind::Known, State::I, BasisKind::PublicDirect, false, "D1 p.4", "逐字:does not affect ownership (nor coherence)"},
          Transition{CellKind::Known, State::S, BasisKind::PublicDirect, false, "D1 p.4", "不改态"},
          Transition{CellKind::Known, State::E, BasisKind::PublicDirect, false, "D1 p.4", "不改态"},
          Transition{CellKind::Known, State::M, BasisKind::PublicDirect, false, "D1 p.4", "不改态"}}},
        // 2 RdOwn
        {{Transition{CellKind::Known, State::M, BasisKind::PublicDirect, false, "D1 p.4 Fig.4 InvalidStore1", "I + Store ⇒ 发 RdOwn;IMAD 释义给 I→M 的 A/D 等待"},
          Transition{CellKind::Known, State::M, BasisKind::SelfDerived, true, "D1 p.4 瞬时态命名", "结构性推断:SMAD/SMD/SMA + IMAD 同构 ⇒ 目标 M;⚠️ 无逐字规则"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: E 已含写权限(D1 p.3 exclusive (write-access and clean))⇒ 语义冗余,但无来源断言合法性"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 无来源"}}},
        // 3 RdAny
        {{Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: D1 只说功能被 RdOwn+RdShared 覆盖,**不指明取哪个** ⇒ 不得臆断目标态"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"}}},
        // 4 RdOwnNoData
        {{Transition{CellKind::Known, State::M, BasisKind::PublicDirect, false, "D1 p.4", "定义式等价于 RdOwn ⇒ 继承其 I 格"},
          Transition{CellKind::Known, State::M, BasisKind::SelfDerived, true, "D1 p.4", "继承 RdOwn 的 S 格(结构性推断)"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 继承 RdOwn 的 E 格"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 继承 RdOwn 的 M 格"}}},
        // 5 CLFlush
        {{Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: D1 p.5/p.12 显式排除(**证明相关性判断**,非状态语义断言)"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"}}},
        // 6 CacheFlushed
        {{Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 无来源"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 无来源"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 无来源"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 无来源"}}},
        // 7 ItoMWr
        {{Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: D1 显式排除;⚠️ 该排除不得推出「不改态」"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上;⚠️ D1 合并 E/M ⇒ 严禁复制 M 规则"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"}}},
        // 8 WrCur
        {{Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: D1 显式排除;⚠️ 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"}}},
        // 9 WOWrInv
        {{Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: D1 显式排除;⚠️ 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"}}},
        // 10 WrInv
        {{Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: D1 显式排除;⚠️ 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同上"}}},
        // 11 DirtyEvict
        {{Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: I 态无脏数据可逐出"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: S 态无脏数据"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: E 无脏数据(D1 的 M+DirtyEvict 路径依赖 dirty payload ⇒ 不可移植)"},
          Transition{CellKind::Known, State::I, BasisKind::PublicDirect, false, "D1 p.9 Table 2 ModifiedEvict1 / MIAGO_WritePull1", "M + Evict ⇒ DirtyEvict;收 GO_WritePull 后回 Data 并转 I"}}},
        // 12 CleanEvict
        {{Transition{CellKind::Known, State::I, BasisKind::PublicDirect, false, "D1 p.8 §5.1", "逐字:Subsequent Evicts have no effect ... already in invalid"},
          Transition{CellKind::Known, State::I, BasisKind::PublicDirect, false, "D1 p.9 Table 1 SharedEvict1 / SIA_GO_WritePullDrop1", "S + Evict ⇒ CleanEvict ⇒ SIA ⇒ 收 GO_WritePullDrop 转 I"},
          Transition{CellKind::Known, State::I, BasisKind::PublicDirect, true, "D2 p.10", "D2 分类式表述 clean Data (E or S state) 给合法性;⚠️ 目标态为自明语义推断;⚠️ **D2 许可 absent** ⇒ pending"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: M 态走 dirty 变体;无来源禁止 CleanEvict"}}},
        // 13 CleanEvictNoData
        {{Transition{CellKind::Known, State::I, BasisKind::PublicDirect, false, "D1 p.4", "定义式:同 CleanEvict 但无数据 ⇒ 继承其 I 格"},
          Transition{CellKind::Known, State::I, BasisKind::PublicDirect, false, "D1 p.4", "继承 CleanEvict 的 S 格"},
          Transition{CellKind::Known, State::I, BasisKind::PublicDirect, true, "D2 p.10", "继承 CleanEvict 的 E 格(⚠️ D2 许可 absent ⇒ pending)"},
          Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 继承 CleanEvict 的 M 格"}}},
        // 14 SnpInv
        {{Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: D1 显式不建模 I 态收 snoop(RspIHitI is not used)⇒ **未知,非非法**"},
          Transition{CellKind::Known, State::I, BasisKind::PublicDirect, false, "D1 p.5 Fig.4 SharedSnpInv1", "回 RspIHitSE(S 干净 ⇒ 不带数据)"},
          Transition{CellKind::Known, State::I, BasisKind::PublicDirect, false, "D1 p.5 通用义务 + RspIHitSE 定义含 E", "E 干净 ⇒ 回 RspIHitSE 且不带数据"},
          Transition{CellKind::Known, State::I, BasisKind::PublicDirect, false, "D1 p.5", "逐字:from M to I and is also forwarding its dirty data(RspIFwdM)"}}},
        // 15 SnpData
        {{Transition{CellKind::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 同 SnpInv 的 I 格(D1 显式不建模)"},
          Transition{CellKind::Known, State::S, BasisKind::PublicDirect, false, "D1 p.5", "值域 {S,I};干净行无需失效 ⇒ 取 S(⚠️ S-vs-I 选择为推断,降级本身 direct)"},
          Transition{CellKind::Known, State::S, BasisKind::PublicDirect, false, "D1 p.5", "同上;E 干净 ⇒ 不回数据"},
          Transition{CellKind::Known, State::S, BasisKind::PublicDirect, false, "D1 p.5", "逐字:from M to S and is also forwarding its dirty data(RspSFwdM)"}}},
        // 16 SnpCurr
        {{Transition{CellKind::Known, State::I, BasisKind::PublicDirect, false, "D1 p.5", "不改态"},
          Transition{CellKind::Known, State::S, BasisKind::PublicDirect, false, "D1 p.5", "不改态"},
          Transition{CellKind::Known, State::E, BasisKind::PublicDirect, false, "D1 p.5", "不改态"},
          Transition{CellKind::Known, State::M, BasisKind::PublicDirect, false, "D1 p.5", "不改态;脏则回数据"}}},
    }};

    /// 行序自检:矩阵行序 SHALL == `Event` 枚举值。
    static_assert(static_cast<std::uint8_t>(Event::RdShared) == 0, "矩阵第 0 行 SHALL 是 RdShared");
    static_assert(static_cast<std::uint8_t>(Event::SnpInv) == 14, "矩阵第 14 行 SHALL 是 SnpInv");
    static_assert(static_cast<std::uint8_t>(Event::SnpCurr) + 1 == kEventCount,
                  "矩阵行数 SHALL == 事件数");

    /// 取格(⚠️ 调用方 SHALL 先判 `kind`;`Unknown` 的 `target` **无意义**)。
    static constexpr Transition transition(State s, Event e) noexcept {
        return kTransitions[static_cast<std::uint8_t>(e)][static_cast<std::uint8_t>(s)];
    }

    /// 逐格统计 —— 供测试**交叉核对**下方硬编码常量 ⇒ 防「虚报覆盖率」(判据 AC-9)。
    static constexpr std::size_t count_cells(CellKind k) noexcept {
        std::size_t n = 0;
        for (const auto& row : kTransitions) {
            for (const auto& cell : row) {
                if (cell.kind == k) {
                    ++n;
                }
            }
        }
        return n;
    }

    static constexpr std::size_t kKnownCellCount = 26;
    static constexpr std::size_t kIllegalCellCount = 0;
    static constexpr std::size_t kUnknownCellCount = 42;
    static constexpr std::size_t kFreeCellCount = 0;

    /// 统计自洽(四者相加 == 68)—— 防算术笔误;真值由测试用 `count_cells()` 交叉核对。
    static_assert(kKnownCellCount + kIllegalCellCount + kUnknownCellCount + kFreeCellCount ==
                      kTransitionCellCount,
                  "四态格数之和 SHALL == 总格数");

    /// **反秃头覆盖率**(判据 AC-9):已定立场的格数 / 总格数。
    /// ⚠️ 「已定立场」= `Known` / `Illegal` / `Free`(规范明确实现自由也是立场);
    ///   `Unknown` **不计入**(「不知道」不是立场)。
    static constexpr std::size_t kLegalTransitionCoverageNum =
        kKnownCellCount + kIllegalCellCount + kFreeCellCount;
    static constexpr std::size_t kLegalTransitionCoverageDen = kTransitionCellCount;

    // ── **单调下界**(AC-9 的「单调不减」条款)───────────────────────────────
    /// 已确证格(`pending = false`)的**下界** —— ⚠️ **不得下降**。
    /// ⇒ 「全 68 格皆 `pending`」这类整体降档 SHALL **必红**;而**上调下界**是一次
    ///    **可见且可审计**的编辑(须台账 entry 授权,per AC-9)。
    static constexpr std::size_t kPendingFalseFloor = 22;
    /// 覆盖率(已定立场格数)的**下界** —— 同上,不得下降。
    static constexpr std::size_t kCoverageFloor = 26;

    /// **覆盖率未回退**判据(运行时可判,判据铁律 ③)。
    static constexpr bool coverage_is_not_regressed() noexcept {
        std::size_t pf = 0;
        std::size_t decided = 0;
        for (const auto& row : kTransitions) {
            for (const auto& cell : row) {
                if (cell.kind == CellKind::Known || cell.kind == CellKind::Illegal) {
                    ++decided;
                }
                if (cell.kind == CellKind::Known && !cell.pending) {
                    ++pf;
                }
            }
        }
        return pf >= kPendingFalseFloor && decided >= kCoverageFloor;
    }

    /// **可传入矩阵**的校验(⚠️ 反向 fixture 的运行时可判入口,判据铁律 ③)。
    /// 校验 5 条:① 三态之和 == 总格数 ② `Unknown` 的 `note` 须带 `R-` 编号
    /// ③ `Illegal` 格须有 `ref` ④ 只有 `Known` 可 `pending = false`,且须有 `ref`
    /// ⑤ `basis_pending_consistent` 逐格成立。
    template <std::size_t NE, std::size_t NS>
    static constexpr bool transitions_are_valid_for(
        const std::array<std::array<Transition, NS>, NE>& m) noexcept {
        std::size_t n_known = 0;
        std::size_t n_illegal = 0;
        std::size_t n_unknown = 0;
        for (const auto& row : m) {
            for (const auto& cell : row) {
                switch (cell.kind) {
                    case CellKind::Known: ++n_known; break;
                    case CellKind::Illegal: ++n_illegal; break;
                    case CellKind::Unknown: ++n_unknown; break;
                }
                if (cell.kind == CellKind::Unknown) {
                    if (!contains(cell.note, "R-")) {
                        return false;
                    }
                    continue;
                }
                if (cell.kind == CellKind::Illegal && !contains(cell.ref, "D")) {
                    return false;
                }
                if (!cell.pending && (cell.kind != CellKind::Known || !contains(cell.ref, "D"))) {
                    return false;
                }
                if (!basis_pending_consistent(cell.basis, cell.pending)) {
                    return false;
                }
            }
        }
        return n_known + n_illegal + n_unknown == NE * NS;
    }

    /// 本矩阵自检(= 上式的绑定调用)。
    static constexpr bool transitions_are_valid() noexcept {
        return transitions_are_valid_for(kTransitions);
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

    /// `basis.kind` / `pending` 的一致性判据 —— ⚠️ **单向**(2026-11-01 修正,per 裁决 A 两轴)。
    ///
    /// 原版要求**双向**(`public-direct` ⇒ `pending = false`)。该要求与**裁决 A** 冲突:
    /// 裁决 A 明定 `pin=weak` 的格**可保留** `public-direct` 但 `pending = true`
    /// (证据强度轴 ≠ 钉版强度轴)⇒ 双向判据会把**合法组合判成非法**。
    ///
    /// 现规则(单向):
    /// · `public-analogical` / `self-derived` ⇒ **必须** `pending = true`(强度天花板,不得当已裁决);
    /// · `public-direct` / `archforge-anchor` ⇒ `pending` **不受 kind 约束**
    ///   (由 `pin` 等级 + 许可确认**独立**决定,per 锚点摘要 §8.2)。
    static constexpr bool basis_pending_consistent(BasisKind k, bool pending) noexcept {
        if (k == BasisKind::PublicAnalogical || k == BasisKind::SelfDerived) {
            return pending;
        }
        return true;  // PublicDirect / ArchforgeAnchor:两轴独立
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
