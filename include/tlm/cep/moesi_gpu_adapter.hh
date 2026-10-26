// include/tlm/cep/moesi_gpu_adapter.hh
// MoesiGpuAdapter —— `moesi_gpu` 协议实例 adapter(6 态 × 6 事件 + 6→4 perm 投影 + 6→3 消息类映射)
//
// 功能:把 `moesi_gpu` 的协议语义实现为**可执行**的契约载体:
//       (1) 6 态 × 6 事件状态转换表(表驱动)
//       (2) 6→4 `perm` 投影(瞬态 `T` **无**投影 —— 跨域接口不得出现中间态)
//       (3) 6→3 消息类 → 链路层 `CATEGORY_*` 映射(显式表 + 恒等登记)
// 作者: CppTLM Team / 日期: 2026-10-27
//
// ── 定位(per `openspec/changes/cpptlm-cep-moesi-gpu-adapter` delta capability
//    `cep-moesi-gpu-adapter`)────────────────────────────────────────────
// 本类**不是**缓存性能模型,而是**协议插件的语义载体**:
//   · 状态机 = 协议自身的 6 态语义(与 Stub Cache 的 5 态投影是**两个层级**);
//   · `perm` 投影 = 跨域接口上的 4 值权限表达(per `ADR-CEP-05` 的 `perm` 4 值锁);
//   · 消息类映射 = 「一致性层 6 类词表 → 链路层 3 类 flit 类别」的**特化字典**
//     (per `ADR-CEP-12 §8.3` ② 分层纪律 —— 两个维度正交,关系由 adapter 处的
//      显式映射表承担,**非恒等**)。
//
// ── 边界 ────────────────────────────────────────────────────────────────
//   · **header-only**,**不进 `ModuleFactory`**(testbench 定位的组件,同 `StubCacheCore`)
//   · **不注册** 23 ABI · **不改** frozen header · **不扩** `CATEGORY_*` 词表
//   · **新增 invariant 数 = 0**(per change design `D-ι` —— `INV-CEP-05-IC1` /
//     `INV-CEP-12-IC1~IC3` 只做**行为化**,不新增编号)
//   · 6×6 全表与 ArchForge 规约的**逐格镜像不在本 change 范围**(per design `D-ε`)——
//     只锁住语义关键点(`E+Write→M`);该缺口在 change 的 design 中显式登记。
//
// ── 依据 ────────────────────────────────────────────────────────────────
//   · 6 态 / 6 事件 / `state_transition[6][6]` ⇒ ArchForge `09 号` §3.1~§3.3
//   · 6→4 投影表 ⇒ ArchForge `32 号` §7.2(逐字:`I→NONE` / `S→READ` / `E→UNIQUE` /
//     `M→WRITEBACK` / `O→WRITEBACK`;瞬态 `T` 无投影)
//   · 6 类消息类 + 方向 ⇒ ArchForge `32 号` §6.0.1
//   · 静默升级判据 (a)(b) ⇒ `ADR-CEP-13` 决策 3
//   · `perm` 4 值锁 ⇒ `ADR-CEP-02` CEN-Ib11

#ifndef CPPTLM_TLM_CEP_MOESI_GPU_ADAPTER_HH
#define CPPTLM_TLM_CEP_MOESI_GPU_ADAPTER_HH

#include <cstddef>
#include <cstdint>
#include <string>

#include "bundles/noc_bundles_tlm.hh"

namespace tlm::cep {

/// `moesi_gpu` 协议实例 adapter —— 6 态 × 6 事件 + 6→4 perm 投影 + 6→3 消息类映射
class MoesiGpuAdapter {
public:
    // ── 6 态(per ArchForge `09 号` §3.1)──────────────────────────────
    enum class State : std::uint8_t { I = 0, S = 1, E = 2, M = 3, O = 4, T = 5 };

    // ── 6 事件(per ArchForge `09 号` §3.2)────────────────────────────
    enum class Event : std::uint8_t { Rd = 0, Wr = 1, Inv = 2, InvAck = 3, Fl = 4, FlAck = 5 };

    // ── 4 值 perm(per `ADR-CEP-02` CEN-Ib11 的 4 值锁)─────────────────
    enum class Perm : std::uint8_t { NONE = 0, READ = 1, UNIQUE = 2, WRITEBACK = 3 };

    static constexpr std::size_t kStateCount        = 6;
    static constexpr std::size_t kEventCount        = 6;
    static constexpr std::size_t kMessageClassCount = 6;

    /// 协议标识(逐字对齐 `protocol_instances.json` 的 `adapter` 字段)
    static constexpr const char* kAdapterName = "MoesiGpuAdapter";
    /// 实例 id(逐字对齐 `protocol_instances.json` 的 `id` 字段)
    static constexpr const char* kInstanceId = "moesi_gpu";

    // ══════════════════════════════════════════════════════════════════
    //  (1) 6 态 × 6 事件状态转换表
    // ══════════════════════════════════════════════════════════════════

    /// 状态转换表(per ArchForge `09 号` §3.3)—— 行 = 当前态,列 = 事件。
    /// 值 = 目标态。**索引**与枚举一致(`State`/`Event` 的底层值即下标)。
    static constexpr std::uint8_t kStateTransition[kStateCount][kEventCount] = {
        {1, 3, 0, 0, 0, 0},  // I: Rd→S / Wr→M / 其余→I
        {1, 3, 0, 1, 4, 1},  // S: Rd→S / Wr→M / Inv→I / InvAck→S / Fl→O / FlAck→S
        {2, 3, 0, 2, 4, 2},  // E: Rd→E / Wr→M / Inv→I / InvAck→E / Fl→O / FlAck→E
        {3, 3, 3, 3, 3, 3},  // M: 任意事件保持 M
        {4, 3, 0, 4, 4, 4},  // O: Rd→O / Wr→M / Inv→I / 其余→O
        {1, 3, 0, 1, 4, 1},  // T: 过渡态,收敛回 S / M
    };

    /// 状态转换(表驱动)
    static constexpr std::uint8_t transition(State s, Event e) noexcept {
        return kStateTransition[static_cast<std::uint8_t>(s)][static_cast<std::uint8_t>(e)];
    }

    /// `E + Write → M` 的**静默升级**判定(per `ADR-CEP-13` 决策 3 判据 (a)):
    /// 终态 `s1` = `E` 允许 store **命中且无需外部通信**(转 `s2` = `M`),
    /// 且 `s2` 接受「导致数据值响应的转发请求」⇒ 判据 (b) 成立 ⇒ **是域可见写**。
    static constexpr bool is_silent_upgrade(State s, Event e) noexcept {
        return s == State::E && e == Event::Wr && transition(s, e) == static_cast<std::uint8_t>(State::M);
    }

    /// snoop 响应的数据承载语义(per `ADR-CEP-13` 决策 3 判据 (b) 的两支):
    /// `M` 态 = `s2` 支 ⇒ **携带数据值**;`E` 态 = `s1` 支 ⇒ **不**携带数据值。
    static constexpr bool snoop_response_carries_data(State s) noexcept {
        return s == State::M;
    }

    // ══════════════════════════════════════════════════════════════════
    //  (2) 6→4 `perm` 投影
    // ══════════════════════════════════════════════════════════════════

    /// 稳定态 → 4 值 `perm` 投影。**瞬态 `T` 无投影**(返回 `false`):
    /// 跨域接口 SHALL NOT 出现「等待 snoop 响应」的中间态
    /// (per `ADR-CEP-05` 的 `INV-CEP-05-IC1`:Stub 只暴露稳定状态)。
    static constexpr bool perm_of(State s, Perm& out) noexcept {
        switch (s) {
            case State::I: out = Perm::NONE;     return true;
            case State::S: out = Perm::READ;     return true;
            case State::E: out = Perm::UNIQUE;   return true;
            case State::M: out = Perm::WRITEBACK; return true;
            case State::O: out = Perm::WRITEBACK; return true;
            case State::T: return false;  // 瞬态:无投影
        }
        return false;
    }

    /// 稳定态计数(投影表完备性判据的期望值 = 5)
    static constexpr std::size_t kStableStateCount = 5;

    /// `O` 与 `M` 合并为 `WRITEBACK` 的**语义损失登记**
    /// (「是否还有其他 sharer」信息由 Sharer Tracker 补偿,**不**依赖 `perm` 表达)。
    static constexpr const char* kPermMergeNote =
        "O 与 M 合并投影为 WRITEBACK:「是否还有其他 sharer」的信息**不**由 perm 表达,"
        "而由 SharerTracker(`SparseSnoopFilter` / scope=SYSTEM / completeness=CONSERVATIVE)"
        "补偿 —— 故 perm 投影的**语义损失是已知且被登记的**。";

    // ══════════════════════════════════════════════════════════════════
    //  (3) 6→3 消息类 → 链路层 `CATEGORY_*` 映射
    // ══════════════════════════════════════════════════════════════════

    /// 一行映射(消息类名 → 链路层 flit 类别)
    struct MessageClassMapping {
        const char*    message_class;
        std::uint8_t   category;
    };

    /// 本实例**实际使用**的 6 类消息类 → `CATEGORY_*` 映射
    /// (per ArchForge `32 号` §6.0.1 的 6 类词表 + 方向列)。
    ///
    /// 映射轴 = **链路事务视角**:需要应答的走 `CATEGORY_REQUEST`,应答走 `CATEGORY_RESPONSE`
    /// (node→center 的 `request`/`atomic`/`query` 与 center→node 的 `snoop` 都是
    ///  「发起一次需要应答的链路事务」;`response`/`snoop_resp` 是应答)。
    /// ⇒ 本表 **== 平台方向默认表**(见 `adapter_schema_contracts.hh` 的
    /// `identity_reference_map()`),故本实例**恒等** —— 恒等是合法状态,
    /// 但必须**被登记**(per delta spec「若某实例的映射确实恒等,则 SHALL 显式声明恒等
    /// 并携带理由」)。**不**为了「看起来非恒等」而破坏链路层的 credit 记账语义。
    static constexpr MessageClassMapping kMessageClassMap[kMessageClassCount] = {
        {"request",    bundles::NoCFlitBundle::CATEGORY_REQUEST},
        {"response",   bundles::NoCFlitBundle::CATEGORY_RESPONSE},
        {"snoop",      bundles::NoCFlitBundle::CATEGORY_REQUEST},
        {"snoop_resp", bundles::NoCFlitBundle::CATEGORY_RESPONSE},
        {"atomic",     bundles::NoCFlitBundle::CATEGORY_REQUEST},
        {"query",      bundles::NoCFlitBundle::CATEGORY_REQUEST},
    };

    /// 本实例的映射是否恒等(相对平台方向默认表)
    static constexpr bool kMappingIsIdentity = true;

    /// 恒等理由(恒等是合法状态,但**不得静默放过** —— 必须被登记)
    static constexpr const char* kIdentityReason =
        "moesi_gpu 的 6 类消息类在**链路事务视角**下与平台方向默认表逐一相同"
        "(request/atomic/query/snoop = 发起一次需应答的事务 ⇒ REQUEST;"
        " response/snoop_resp = 应答 ⇒ RESPONSE)。本实例无协议特有偏离 ⇒ 显式登记恒等。";

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

    /// 本实例用到的 6 类消息类(与 `kMessageClassMap` 同序,供完备性判据使用)
    static constexpr std::size_t used_message_class_count() noexcept { return kMessageClassCount; }
};

}  // namespace tlm::cep

#endif  // CPPTLM_TLM_CEP_MOESI_GPU_ADAPTER_HH
