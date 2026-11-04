// test/test_cep_h6_vc_isolation.cc
// H6 实验：collective 独立 VC 是否避免协议级死锁
//
// 作者: CppTLM Team / 日期: 2026-11-04
//
// 判据来源(`obj-cep-platform.md §9.1 H6`):
//   VC 隔离(collective 独立 VC)确实避免协议级死锁。
//   验证手段 `vc_stall_per_vc` + `deadlock_violations`。
//   成功判据:**0 死锁 + collective stall 不占 coherence VC**。
//
// ⚠️ 口径限定:
//   · 本实验是**最小 VC 模型**(单级仲裁 / 无 credit 环流 / 无跨 VC 依赖图),
//     不代表完整 fabric 仲裁(属 Stage 4+,per `ADR-CEP-09`)。
//   · 既有登记义务必须随结论带上:`ADR-CEP-09` SU 2026-11-02 §④ 的
//     「D∪E 合并后 `GrantAck` 可能被 `Grant` 饿死**未建模**」本模型**不覆盖**。
//
// 纪律:
//   · 共享 VC(负控)必须**真的**饥饿,否则「隔离有效」无对照
//   · 反向 fixture:标志位写 `isolate_collective=true` 但两类流量仍落在同一 VC
//     ⇒ 必须同样饥饿 ⇒ 证明判据看的是**实际 VC 归属**而非布尔标志

#include "catch_amalgamated.hpp"

#include <cstdint>

#include "cep_experiment_harness.hh"
#include "cep_vc_fabric_model.hh"

using namespace cep_exp;

namespace {

    /// 跑固定注入速率的对抗 workload:coherence 2/cycle、collective 1/cycle
    void run_adversarial(VcFabricModel& m, int cycles) {
        for (int i = 0; i < cycles; ++i) {
            m.inject(TrafficClass::Coherence, 2);
            m.inject(TrafficClass::Collective, 1);
            m.tick();
        }
    }

    VcFabricConfig isolation_cfg() {
        VcFabricConfig c;
        c.isolate_collective = true;
        c.coherence_vc = 0;
        c.collective_vc = 3;
        return c;
    }

    VcFabricConfig shared_cfg() {
        VcFabricConfig c;
        c.isolate_collective = false;
        c.shared_vc = 0;
        return c;
    }

} // namespace

TEST_CASE("H6 · 独立 VC ⇒ 0 死锁且 collective stall 不占 coherence VC", "[cep][exp][h6]") {
    VcFabricModel m(isolation_cfg());
    run_adversarial(m, 2000);

    INFO("collective backlog = " << m.collective_backlog());
    INFO("stagnant cycles    = " << m.stagnant_cycles());
    INFO("wait on coh VC     = " << m.collective_wait_on_coherence_vc());

    REQUIRE_FALSE(m.collective_shares_coherence_vc());
    REQUIRE(m.deadlock_violations() == 0);             // 0 死锁
    REQUIRE(m.collective_wait_on_coherence_vc() == 0); // collective stall 不占 coherence VC
    REQUIRE(m.collective_backlog() == 0);              // 1/cycle 注入 ≤ 1/cycle 服务
    REQUIRE(m.stall_per_vc().size() == 4);             // vc_stall_per_vc 的键空间
}

TEST_CASE("H6 · 负控:共享 VC ⇒ collective 必须饥饿(证明隔离有对照)", "[cep][exp][h6][reverse]") {
    VcFabricModel m(shared_cfg());
    run_adversarial(m, 2000);

    INFO("collective backlog = " << m.collective_backlog());
    INFO("stagnant cycles    = " << m.stagnant_cycles());

    REQUIRE(m.collective_shares_coherence_vc());
    REQUIRE(m.collective_backlog() > 0);              // 确实积压
    REQUIRE(m.deadlock_violations() == 1);            // 饥饿 ⇒ 死锁
    REQUIRE(m.collective_wait_on_coherence_vc() > 0); // 等待落在 coherence VC 上
}

TEST_CASE("H6 · 反向 fixture:标志位为真但 VC 归属相同 ⇒ 仍必须饥饿", "[cep][exp][h6][reverse]") {
    // ⚠️ 这是「假隔离」:配置声称 isolate=true,但 collective_vc == coherence_vc == 0
    VcFabricConfig fake = isolation_cfg();
    fake.collective_vc = 0; // 与 coherence_vc 撞车

    VcFabricModel m(fake);
    run_adversarial(m, 2000);

    // 判据看的是**实际 VC 归属**,不是布尔标志
    REQUIRE(m.collective_shares_coherence_vc());
    REQUIRE(m.deadlock_violations() == 1);
}

TEST_CASE("H6 · 无 collective 流量时共享 VC 不得误报死锁", "[cep][exp][h6][reverse]") {
    VcFabricModel m(shared_cfg());
    for (int i = 0; i < 2000; ++i) {
        m.inject(TrafficClass::Coherence, 2);
        m.tick();
    }
    REQUIRE(m.collective_backlog() == 0);
    REQUIRE(m.deadlock_violations() == 0); // 无 collective ⇒ 无「collective 饥饿」
}

TEST_CASE("H6 · metrics 组装带 verification_mode / scope(不得留空)", "[cep][exp][h6]") {
    VcFabricModel m(isolation_cfg());
    run_adversarial(m, 1000);

    ExperimentMetrics metrics;
    metrics.total_cycles = m.cycle();
    metrics.total_transactions = m.served_per_vc()[0] + m.served_per_vc()[3];
    // 事务与延迟配平(本模型不采延迟 ⇒ 逐事务补 0 样本,保持 issued == completed)
    for (std::uint64_t i = 0; i < metrics.total_transactions; ++i) {
        metrics.latency.note_issue();
        metrics.latency.note_complete(0, 0);
    }
    metrics.deadlock_detected = m.deadlock_violations();
    for (std::size_t vc = 0; vc < m.stall_per_vc().size(); ++vc)
        metrics.vc_stall_per_vc[static_cast<int>(vc)] = m.stall_per_vc()[vc];
    metrics.verification_mode = "bounded_random";
    metrics.verification_scope = m.scope();

    REQUIRE(metrics.well_formed());
    const auto js = metrics.to_json();
    REQUIRE(js.find("\"vc_stall_per_vc\"") != std::string::npos);
    REQUIRE(js.find("\"vc3\"") != std::string::npos);
    REQUIRE(js.find("collective 独立 VC") != std::string::npos);
}
