// =============================================================================
// scripts/test/cep_tilelink_engine_probe.cc
//   —— TileLink-C 运行时引擎的**构建期探针**(fail-closed 消费面取证)
//
// 作者: CppTLM Team / 日期: 2026-11-06
// 配套: scripts/test/cep_tilelink_engine_failclosed.sh(影子树驱动)
//       openspec/changes/cpptlm-cep-tilelink-runtime-engine/(AC-4 / AC-5 / TLE-06)
//
// ── 探针语义(自适配影子树)──────────────────────────────────────────────────
// 探针**不**知道自己是编在「干净树」还是「毒化树」上 ⇒ 它**读取真源的 basis**
//   并据此断言**期望值**:
//   · 该意图行 basis == `Direct` ⇒ 引擎**应当**发出 1 个事件;
//   · 该意图行 basis == `Pending` ⇒ 引擎**必须零发射**(fail-closed)。
// ⇒ 同一份探针在两棵树上给出**相反**的期望 ⇒ 判据有鉴别力
//   (⛔ 若探针写死期望值,则在毒化树上会「因错误的原因红」)。
//
// ⚠️ 探针**只**消费引擎的**事件发射出口**(`total_events()`)——
//   ⛔ 不断言掩码函数返回值(那是 state_spec 层,重复 T3 普查的反向 fixture)。
//
// =============================================================================

#include "tlm/cep/tilelink_protocol_engine.hh"

#include <cstdio>

int main() {
    using Engine = tlm::cep::TileLinkProtocolEngine;
    using Spec = tlm::cep::TileLinkStateSpec;
    using Intent = Spec::PlatformIntent;

    EventQueue eq;
    Engine e("probe", &eq);

    const auto basis = Spec::t3_basis_of(Intent::Completion);

    // ① 驱动一个**有事件**意图(completion ⇒ T3-02)
    e.on_completion(1, 0x42, tlm::cep::Perm::READ, false);

    const std::uint64_t observed = e.intent_observed(Intent::Completion);
    const std::uint64_t events = e.total_events();

    // ② 正控:意图**确实到达过**(⇒ 不是「没跑到」)
    if (observed != 1u) {
        std::fprintf(stderr, "FAIL: completion 意图到达次数 = %llu(应为 1)\n",
                     static_cast<unsigned long long>(observed));
        return 1;
    }

    if (basis == Spec::T3BasisKind::Pending) {
        // ③ 反控(fail-closed):未锚 ⇒ 引擎**零发射**
        if (events != 0u) {
            std::fprintf(stderr,
                         "FAIL: Pending 行却发射了 %llu 个事件(fail-closed 被破坏)\n",
                         static_cast<unsigned long long>(events));
            return 1;
        }
        std::printf("PASS pending: observed=1 events=0 (fail-closed 成立)\n");
        return 0;
    }

    // ④ 正控:Direct 行 ⇒ 恰 1 个事件(Grow)
    if (events != 1u) {
        std::fprintf(stderr, "FAIL: Direct 行应发 1 个事件,实得 %llu\n",
                     static_cast<unsigned long long>(events));
        return 1;
    }
    if (e.counters().grow_events != 1u) {
        std::fprintf(stderr, "FAIL: 事件类别应为 Grow,实得 grow=%llu\n",
                     static_cast<unsigned long long>(e.counters().grow_events));
        return 1;
    }
    std::printf("PASS direct: observed=1 events=1 (grow)\n");
    return 0;
}
