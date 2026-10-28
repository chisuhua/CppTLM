// include/tlm/cep/chi_rnf_adapter.hh
// ChiRnfAdapter —— `chi_rnf` 协议实例 adapter(CHI RN-F subset:5 稳定态 + 6→4 perm 投影 + 6→3 消息类映射)
//
// 功能:把 **CHI RN-F 子集**的协议语义实现为**可执行**的契约载体:
//       (1) 5 稳定态 × 事件状态转换表(表驱动)
//       (2) 6→4 `perm` 投影(瞬态**无**投影 —— 跨域接口不得出现中间态)
//       (3) 6→3 消息类 → 链路层 `CATEGORY_*` 映射(显式表 + 恒等登记)
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ── 定位(per `openspec/changes/cpptlm-cep-chi-rnf-subset` delta capability
//    `cep-chi-rnf-subset`)────────────────────────────────────────────────
// 本类**不是**缓存性能模型,而是**协议插件的语义载体**:
//   · 状态机 = CHI RN-F 子集自身的缓存态语义(与 Stub Cache 的 5 态投影是**两个层级**);
//   · `perm` 投影 = 跨域接口上的 4 值权限表达(per `ADR-CEP-02` 的 `perm` 4 值锁);
//   · 消息类映射 = 「一致性层 6 类词表 → 链路层 3 类 flit 类别」的**特化字典**
//     (per `ADR-CEP-12 §8.3` ② 分层纪律 —— 两个维度正交,关系由 adapter 处的
//      显式映射表承担,**非恒等**)。
//
// ── 边界 ────────────────────────────────────────────────────────────────
//   · **header-only**,**不进 `ModuleFactory`**(testbench 定位的组件,同 `StubCacheCore`)
//   · **不注册** 23 ABI · **不改** frozen header · **不扩** `CATEGORY_*` 词表(恒 3 类)
//   · **不扩** `perm` 4 值锁 · **新增 invariant 数 = 0**
//
// ── 一手依据(E1 锚,per change design `D-δ`)────────────────────────────
// **AMBA® CHI Architecture Specification, ARM IHI 0050, Issue G**(2025-09-08):
//   · §B4.1:  "A store to a local cache line can only happen when it is held in a
//              Unique state."                       ⇒ write 只在 Unique 态发生
//   · §B1.5.2:"When Unique, the cache line exists only in the Unique cache."
//                                                    ⇒ Unique 态 ⇒ 唯一副本
//   · §B1.5.2/Fig B1.3: 七态模型(`UCE`/`UC`/`SC`/`UDP`/`UD`/`SD`/`I`);
//              且 "UC, UD, SC, and SD are all Full cache line states."
// ⇒ **净结论**:`UC` 是 Unique 态 ⇒ 唯一副本 ⇒ store 命中**零总线事务**
//   ⇒ `UC→UD` 是**静默升级** ⇒ 4 值 `perm` 无法区分「静默变脏」
//   ⇒ 与 `moesi_gpu` 的 `E→M` **同型** ⇒ `perm_upgrades.json` 加行有依据。
//
// ── ⚠️ 与一手规范的**范围偏离(显式登记)**────────────────────────────
// IHI0050 定义 **7 态**;本 subset 实现 **5 态**(Full 态子集)。
// `UCE`(Unique Clean Empty)与 `UDP`(Unique Dirty Partial)被**排除**:
//   ① 二者是 Partial / Empty 变体(非 Full),Stage 1 无价值;
//   ② `UDP` 的**部分行脏**写回合并复杂度对 Stage 1 无收益。
// ⇒ 该偏离**只缩小**覆盖面(不产生与规范矛盾的行为)。

#ifndef CPPTLM_TLM_CEP_CHI_RNF_ADAPTER_HH
#define CPPTLM_TLM_CEP_CHI_RNF_ADAPTER_HH

#include <cstddef>
#include <cstdint>

#include "bundles/noc_bundles_tlm.hh"

namespace tlm::cep {

/// `chi_rnf` 协议实例 adapter —— CHI RN-F subset 5 稳定态 × 事件 + 6→4 perm 投影 + 6→3 消息类映射
class ChiRnfAdapter {
public:
    // ── 5 稳定态(per IHI0050 §B1.5.2 Figure B1.3 的 **Full 态子集**)────
    // ⚠️ 枚举**值序刻意**让 `UC = 2` / `UD = 3` —— 使 `UC+Store→UD` 的目标态
    //    恰为 `3`,从而**复用**队列 #6 的 AC-05 判据(其输入是 POD,判据本体零改动)。
    enum class State : std::uint8_t { I = 0, SC = 1, UC = 2, UD = 3, SD = 4 };

    // ── 事件(per IHI0050 §B1.5.2 + VLSI Mentor《CHI State Transitions》§8 的
    //    **local / remote** 二源转换表;**只取**本 subset 需要的事件)────────
    enum class Event : std::uint8_t {
        Rd = 0,        ///< local  :ReadShared(→ SC) / ReadUnique(→ UC)
        RdUnique = 1,  ///< local  :ReadUnique(→ UC)
        Wr = 2,        ///< local  :write(**只**在 Unique 态合法;UC→UD / UD→UD)
        Upgrade = 3,   ///< local  :MakeUnique / CleanUnique(SC → UC;失效其他 sharer)
        SnpShared = 4, ///< remote :降级(any valid → SC)
        SnpUnique = 5  ///< remote :失效(any valid → I)
    };

    // ── 4 值 perm(per `ADR-CEP-02` CEN-Ib11 的 4 值锁)─────────────────
    enum class Perm : std::uint8_t { NONE = 0, READ = 1, UNIQUE = 2, WRITEBACK = 3 };

    static constexpr std::size_t kStateCount        = 5;
    static constexpr std::size_t kEventCount        = 6;
    static constexpr std::size_t kMessageClassCount = 4;

    /// 协议标识(逐字对齐 `protocol_instances.json` 的 `adapter` 字段)
    static constexpr const char* kAdapterName = "ChiRnfAdapter";
    /// 实例 id(逐字对齐 `protocol_instances.json` 的 `id` 字段)
    static constexpr const char* kInstanceId = "chi_rnf";

    // ══════════════════════════════════════════════════════════════════
    //  (1) 状态转换表
    // ══════════════════════════════════════════════════════════════════

    /// 状态转换表 —— 行 = 当前态,列 = 事件。值 = 目标态。
    /// **索引**与枚举一致(`State`/`Event` 的底层值即下标)。
    ///
    /// 逐格依据(per IHI0050 + VLSI Mentor §8 的转换表;**二手**仅作辅助):
    /// | 事件 | 源 | From → To |
    /// |---|---|---|
    /// | Read | local | I → SC |
    /// | ReadUnique | local | I → UC |
    /// | Write | local | UC → UD,UD → UD(**写**只从 Unique 态合法) |
    /// | Upgrade(MakeUnique) | local | SC → UC(**共享行须先升级**,无 silent SC→UD) |
    /// | SnpShared | remote | any valid → SC |
    /// | SnpUnique | remote | any valid → I |
    ///
    /// ⚠️ **非法组合的编码约定**:写事件落在**非 Unique** 态(`I` / `SC` / `SD`)⇒
    ///    目标态 = **自身**(状态**不变**)—— 即「该事件在此态**不发生任何转换**」。
    ///    这与 `moesi_gpu` 的「非法事件 ⇒ 回落 I」**不同**:CHI 的语义是
    ///    「共享行必须先 Upgrade」⇒ 共享态上的 write 是**未定义行为的前置缺失**,
    ///    建模为「状态不变」比「静默失效」更贴近规范(且不掩盖 `Upgrade` 事件的必要性)。
    static constexpr std::uint8_t kStateTransition[kStateCount][kEventCount] = {
        // Rd  RdUnique  Wr  Upgrade  SnpShared  SnpUnique
        {1,     2,     0,     0,       1,         0},  // I : Read→SC / ReadUnique→UC
        {1,     1,     1,     2,       1,         0},  // SC: Read→SC / Upgrade→UC
        {2,     2,     3,     2,       1,         0},  // UC: Read→UC / Wr→UD / SnpShared→SC
        {3,     3,     3,     3,       1,         0},  // UD: 保持 UD(脏) / SnpShared→SC
        {4,     4,     4,     4,       1,         0},  // SD: Read→SD / SnpShared→SC
    };

    /// 状态转换(表驱动)
    static constexpr std::uint8_t transition(State s, Event e) noexcept {
        return kStateTransition[static_cast<std::uint8_t>(s)][static_cast<std::uint8_t>(e)];
    }

    /// `UC + Store → UD` 的**静默升级**判定(per IHI0050 §B4.1 + §B1.5.2):
    /// `UC` 是 **Unique** 态 ⇒ 该行**只存在于本 cache** ⇒ store 命中
    /// **无需任何总线事务**(无副本可失效)⇒ 4 值 `perm` 无法表达 ⇒ 域可见写。
    static constexpr bool is_silent_upgrade(State s, Event e) noexcept {
        return s == State::UC && e == Event::Wr && transition(s, e) == static_cast<std::uint8_t>(State::UD);
    }

    /// write 事件在**当前态**是否合法(per IHI0050 §B4.1「write 只在 Unique 态发生」)
    static constexpr bool write_is_legal(State s) noexcept {
        return s == State::UC || s == State::UD;
    }

    // ══════════════════════════════════════════════════════════════════
    //  (2) 6→4 `perm` 投影
    // ══════════════════════════════════════════════════════════════════

    /// 稳定态 → 4 值 `perm` 投影。**瞬态无投影**(返回 `false`):
    /// 跨域接口 SHALL NOT 出现「等待 snoop 响应 / 等数据」的中间态
    /// (per `ADR-CEP-05` 的 `INV-CEP-05-IC1`:Stub 只暴露稳定状态)。
    static constexpr bool perm_of(State s, Perm& out) noexcept {
        switch (s) {
            case State::I:  out = Perm::NONE;     return true;
            case State::SC: out = Perm::READ;     return true;
            case State::UC: out = Perm::UNIQUE;   return true;
            case State::UD: out = Perm::WRITEBACK; return true;
            case State::SD: out = Perm::WRITEBACK; return true;
        }
        return false;
    }

    /// 稳定态计数(投影表完备性判据的期望值 = 5)
    static constexpr std::size_t kStableStateCount = 5;

    /// `SD` 与 `UD` 合并为 `WRITEBACK` 的**语义损失登记**
    ///
    /// ⚠️ **与 `moesi_gpu` 的 `O/M` 合并损失同型、但补偿机制不同** ——
    ///    措辞必须写准,否则审查会拿 moesi 的补偿句来套:
    /// · `moesi_gpu`:`O→WRITEBACK` 的损失由 `SparseSnoopFilter`(`scope=SYSTEM`)**补偿**;
    /// · `chi_rnf`:`sharer_tracker = "不参与"` ⇒ 「是否还有其他 sharer」在 RN 侧
    ///   **物理上不可得**(该信息只存在于 HN-F 的目录)⇒ 本损失是
    ///   **协议信息拓扑的固有损失**,**非实现缺口**,RN 侧**不可补偿**。
    static constexpr const char* kPermMergeNote =
        "SD 与 UD 合并投影为 WRITEBACK:「是否还有其他 sharer」的信息**不由 perm 表达"
        ",而由 Sharer Tracker 补偿 —— 但本实例的 sharer_tracker 为「不参与」(RN-F 不维护 "
        "sharer set,该信息只存在于 HN-F 的目录)⇒ 本合并是**协议信息拓扑的固有损失**"
        "(非实现缺口),RN 侧**不可补偿**。";

    // ══════════════════════════════════════════════════════════════════
    //  (3) 6→3 消息类 → 链路层 `CATEGORY_*` 映射
    // ══════════════════════════════════════════════════════════════════

    /// 一行映射(消息类名 → 链路层 flit 类别)
    struct MessageClassMapping {
        const char*  message_class;
        std::uint8_t category;
    };

    /// 本实例**实际使用**的 4 类消息类 → `CATEGORY_*` 映射
    ///
    /// **「实际使用」的界定规则**(per change design `D-γ`):
    /// 某消息类「实际使用」⟺ subset 声明的 opcode 集能在该类上**发出或吸收**
    /// 一个 wire 事务。
    ///
    /// · `request`    ← TXREQ(ReadShared / ReadUnique / MakeUnique / WriteBack*)
    /// · `response`   ← RXRSP(Comp / CompDBIDResp)+ TXRSP(CompAck)
    /// · `snoop`      ← RXSNP(Snp*)—— ⚠️ **入向**(RN-F 只发 REQ、只收 SNP)
    /// · `snoop_resp` ← TXRSP(SnpResp)
    ///
    /// **不使用**:
    /// · `atomic` —— subset 不实现 Atomic* 事务 ⇒ `atomic.supported = false`,**零**映射行
    /// · `query`  —— CHI 的 home 解析走**静态 SAM**,subset 内无 wire query 事务
    ///
    /// **映射轴 = 链路事务视角**:需要应答的走 `CATEGORY_REQUEST`,应答走 `CATEGORY_RESPONSE`。
    /// ⇒ 本表 **== 平台方向默认表**(见 `adapter_schema_contracts.hh` 的
    /// `identity_reference_map()`),故本实例**恒等** —— 恒等是合法状态,
    /// 但必须**被登记**(per delta spec「若某实例的映射确实恒等,则 SHALL 显式声明恒等
    /// 并携带理由」)。**不**为了「看起来非恒等」而破坏链路层的 credit 记账语义。
    ///
    /// ⚠️ **⚠️ `snoop` 类的入向性不改变其类别**:RXSNP 虽**入向**,但按同一「链路事务轴」
    ///    (需要应答 ⇒ REQUEST)—— 与平台默认表一致。
    static constexpr MessageClassMapping kMessageClassMap[kMessageClassCount] = {
        {"request",    bundles::NoCFlitBundle::CATEGORY_REQUEST},
        {"response",   bundles::NoCFlitBundle::CATEGORY_RESPONSE},
        {"snoop",      bundles::NoCFlitBundle::CATEGORY_REQUEST},
        {"snoop_resp", bundles::NoCFlitBundle::CATEGORY_RESPONSE},
    };

    /// 本实例的映射是否恒等(相对平台方向默认表)
    static constexpr bool kMappingIsIdentity = true;

    /// 恒等理由(恒等是合法状态,但**不得静默放过** —— 必须被登记)
    static constexpr const char* kIdentityReason =
        "chi_rnf 的 4 类消息类在**链路事务视角**下与平台方向默认表逐一相同"
        "(request/response/snoop/snoop_resp:snoop 虽入向,仍需应答 ⇒ REQUEST;"
        " response/snoop_resp 是应答 ⇒ RESPONSE)。本实例无协议特有偏离 ⇒ 显式登记恒等。";

    /// 查某个消息类的链路层类别;未知消息类返回 `false`
    static constexpr bool category_of(const char* message_class, std::uint8_t& out) noexcept {
        for (std::size_t i = 0; i < kMessageClassCount; ++i) {
            const char* a = kMessageClassMap[i].message_class;
            const char* b = message_class;
            while (*a && *a == *b) { ++a; ++b; }
            if (*a == '\0' && *b == '\0') {
                out = kMessageClassMap[i].category;
                return true;
            }
        }
        return false;
    }

    /// 本实例用到的消息类数(与 `kMessageClassMap` 同序,供完备性判据使用)
    static constexpr std::size_t used_message_class_count() noexcept { return kMessageClassCount; }
};

}  // namespace tlm::cep

#endif  // CPPTLM_TLM_CEP_CHI_RNF_ADAPTER_HH
