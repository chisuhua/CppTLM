// test/test_cep_b4_evidence_device.cc
// B4 达成证据载体 —— 「一致性 × 完成域」复合装置的单测
//
// 覆盖(per `ADR-CEP-06` SU#4 §② 六条非空洞性硬约束):
//   · 正控:两类违例类**同时**实测 0 + 观测键实例级交集成立 + watchdog 观测面非空
//   · 负控①:coherence 错标 group-scoped ⇒ 死锁被检出(检测力)
//   · 负控②:SWMR 注入 ⇒ memory 违例被检出
//   · 负控③:过期读注入 ⇒ data-value 违例被检出
//   · 退化形态(两段拼接 / 无在途窗口)⇒ 装置合格性**判红**
//
// 作者: CppTLM Team / 日期: 2026-11-05(per 台账 Entry #226)

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <set>
#include <vector>

#include "cep_consistency_completion_model.hh"

using namespace cep_cc;

namespace {

    /// 构造一个场景模型(默认 = 正控配置)。
    cep_cc::Config make_config() {
        cep_cc::Config c;
        c.txns_per_round = 3;
        c.service_per_cycle = 1;
        c.deadlock_threshold = 200;
        return c;
    }

    /// 跑一个场景 ⇒ 返回模型(供观测)。
    cep_cc::ConsistencyCompletionModel run_scenario(const cep_cc::Config& cfg,
                                                    std::uint64_t target_txns) {
        cep_cc::ConsistencyCompletionModel m(cfg);
        m.run_until(target_txns);
        return m;
    }

} // namespace

TEST_CASE("B4 证据载体 · 正控:两类违例类同时实测 0 + 观测窗非空", "[cep][b4]") {
    const auto cfg = make_config();
    auto m = run_scenario(cfg, 1200);

    SECTION("内存一致性侧:swmr / data-value / 漏探 全为 0") {
        REQUIRE(m.detector().counts().swmr == 0);
        REQUIRE(m.detector().counts().data_value == 0);
        REQUIRE(m.superset_violations() == 0);
    }
    SECTION("活性侧:未判死锁,且在途窗口真实存在") {
        REQUIRE_FALSE(m.deadlocked());
        REQUIRE_FALSE(m.watch_dog_stalled());
        REQUIRE(m.max_in_flight() > 1); // 真实在途窗口(⛔ 非恒假 watchdog)
        REQUIRE(m.max_idle_run() < m.deadlock_threshold());
        REQUIRE(m.coherence_retired() == 1200); // 事务全部完成(⇒ 内存检查真实执行)
    }
    SECTION("同一事务流:实例级观测键交集 = 1.0 + 地址级重叠") {
        REQUIRE(m.same_stream_coverage() == Catch::Approx(1.0));
        REQUIRE(m.blocked_address_overlap());
        REQUIRE(m.memory_checked_txn_ids().size() == 1200);
        REQUIRE(m.blocking_subject_txn_ids().size() >= 1200);
        // ⛔ 退化形态的判据基准:内存操作实例必须**就是**受阻塞支配的实例
        const auto checked = m.memory_checked_txn_ids();
        const auto subjects = m.blocking_subject_txn_ids();
        const std::set<std::uint64_t> subject_set(subjects.begin(), subjects.end());
        std::size_t inter = 0;
        for (std::uint64_t id : checked)
            if (subject_set.count(id) != 0)
                ++inter;
        REQUIRE(inter == checked.size());
    }
    SECTION("延迟面:在途窗口可测(非恒 0)") {
        REQUIRE(m.completed().size() == 1200);
        REQUIRE(m.completed().back().latency_cycles > 0);
    }
}

TEST_CASE("B4 证据载体 · 负控①:coherence 错标 ⇒ 死锁被检出", "[cep][b4]") {
    auto cfg = make_config();
    cfg.coherence_domain = CompletionDomain::GroupScoped; // 错标
    auto m = run_scenario(cfg, 1200);

    REQUIRE(m.watch_dog_stalled());           // watchdog 检出
    REQUIRE(m.completion_model_deadlocked()); // 完成域模型同步检出
    REQUIRE(m.deadlocked());
    // ⚠️ 错标 ⇒ coherence 永不退休 ⇒ 内存检查**未执行**(⇒ 负控不提供 memory 证据)
    REQUIRE(m.coherence_retired() == 0);
    REQUIRE(m.detector().counts().swmr == 0);
}

TEST_CASE("B4 证据载体 · 负控②③:注入各自被检出(检测器非空检查)", "[cep][b4]") {
    SECTION("SWMR 注入 ⇒ swmr 违例 > 0") {
        auto cfg = make_config();
        cfg.inject_swmr_violation = true;
        auto m = run_scenario(cfg, 600);
        REQUIRE(m.detector().counts().swmr > 0);
        REQUIRE(m.detector().counts().data_value == 0);
    }
    SECTION("过期读注入 ⇒ data-value 违例 > 0") {
        auto cfg = make_config();
        cfg.inject_stale_read = true;
        auto m = run_scenario(cfg, 600);
        REQUIRE(m.detector().counts().data_value > 0);
        REQUIRE(m.detector().counts().swmr == 0);
    }
}

TEST_CASE("B4 证据载体 · 耦合探针:阻塞开/关下内存测量不漂移且探针有灵敏度", "[cep][b4]") {
    // per ADR-CEP-06 SU#4 §② 约束 6:若阻塞把事务串行化 ⇒ SWMR 观测窗被收缩 ⇒
    // 「memory_violations = 0」可能只是阻塞的人造物 ⇒ 须证明:
    //   ① 无注入时,阻塞开/关两配置的内存计数**无漂移**(Δ = 0);
    //   ② **灵敏度**:SWMR 注入在**两配置下均被检出**(> 0)⇒ 探针不是空过。
    auto on_cfg = make_config();
    auto off_cfg = make_config();
    off_cfg.blocking_enabled = false;

    SECTION("无注入:两配置内存计数一致(Δ = 0)") {
        const auto on = run_scenario(on_cfg, 600);
        const auto off = run_scenario(off_cfg, 600);
        REQUIRE(on.detector().counts().swmr == 0);
        REQUIRE(off.detector().counts().swmr == 0);
        REQUIRE(on.detector().counts().data_value == 0);
        REQUIRE(off.detector().counts().data_value == 0);
    }
    SECTION("灵敏度:SWMR 注入在阻塞开/关两配置下均被检出") {
        auto on_inj = on_cfg;
        on_inj.inject_swmr_violation = true;
        auto off_inj = off_cfg;
        off_inj.inject_swmr_violation = true;
        const auto on = run_scenario(on_inj, 600);
        const auto off = run_scenario(off_inj, 600);
        REQUIRE(on.detector().counts().swmr > 0);
        REQUIRE(off.detector().counts().swmr > 0);
    }
    SECTION("阻塞关:不产生跨平面阻塞窗口(pending 中无 collective 地址)") {
        const auto on = run_scenario(on_cfg, 600);
        const auto off = run_scenario(off_cfg, 600);
        REQUIRE(on.blocked_address_overlap());
        // 阻塞关 ⇒ collective 立即到达 ⇒ 在途里不再堆积 collective
        REQUIRE(off.pending_addrs().size() < on.pending_addrs().size() + 1);
    }
}

TEST_CASE("B4 证据载体 · 反向 fixture:目录丢失真实 sharer ⇒ 漏探判定必须红", "[cep][b4]") {
    // per AC-9 评审 **S1**:`superset_violations` 默认路径下不可证伪
    // (`truth` 源 = `set_sharer` 参数,`view` 紧随 set 后读取 ⇒ `truth ⊆ sharers` 同义反复)。
    // 本条证明它**非恒过**:目录丢掉一个真实 sharer 而事务仍期望它 ⇒ 判定必须红。
    auto inj_cfg = make_config();
    inj_cfg.inject_missed_sharer = true;
    const auto injured = run_scenario(inj_cfg, 600);

    INFO("注入后 superset 违例必须 > 0(否则该判定是空检查): " << injured.superset_violations());
    REQUIRE(injured.superset_violations() > 0);
    // 注入只污染 sharer 视图,不得污染 SWMR / data-value(隔离性)
    REQUIRE(injured.detector().counts().swmr == 0);
    REQUIRE(injured.detector().counts().data_value == 0);

    INFO("正控(默认关闭)仍须为 0: " << run_scenario(make_config(), 600).superset_violations());
    REQUIRE(run_scenario(make_config(), 600).superset_violations() == 0);
}

TEST_CASE("B4 证据载体 · 装置合格性:四条件缺一即红", "[cep][b4]") {
    const auto pos = run_scenario(make_config(), 600);
    auto mis_cfg = make_config();
    mis_cfg.coherence_domain = CompletionDomain::GroupScoped;
    const auto mis = run_scenario(mis_cfg, 600);
    auto swmr_cfg = make_config();
    swmr_cfg.inject_swmr_violation = true;
    const auto swmr = run_scenario(swmr_cfg, 600);
    auto stale_cfg = make_config();
    stale_cfg.inject_stale_read = true;
    const auto stale = run_scenario(stale_cfg, 600);

    SECTION("正确装置 ⇒ 四条件全绿") {
        const auto q = qualify(pos, mis, swmr, stale);
        INFO(q.reason());
        REQUIRE(q.deadlock_detector_live);
        REQUIRE(q.memory_detector_live);
        REQUIRE(q.same_stream);
        REQUIRE(q.watchdog_not_vacuous);
        REQUIRE(q.qualified());
    }
    SECTION("退化形态①(内存操作挂独立实例流)⇒ same_stream 红 ⇒ 不合格") {
        auto deg_cfg = make_config();
        deg_cfg.detach_memory_ops = true;
        const auto deg = run_scenario(deg_cfg, 600);
        REQUIRE(deg.same_stream_coverage() == Catch::Approx(0.0));
        const auto q = qualify(deg, mis, swmr, stale);
        INFO(q.reason());
        REQUIRE_FALSE(q.same_stream);
        REQUIRE_FALSE(q.qualified());
    }
    SECTION("退化形态②(无在途窗口)⇒ watchdog 条件红 ⇒ 不合格") {
        auto deg_cfg = make_config();
        deg_cfg.txns_per_round = 1; // 每轮 1 条 ⇒ 在途窗口退化
        const auto deg = run_scenario(deg_cfg, 600);
        const auto q = qualify(deg, mis, swmr, stale);
        INFO(q.reason());
        REQUIRE_FALSE(q.watchdog_not_vacuous);
        REQUIRE_FALSE(q.qualified());
    }
    SECTION("负控不红 ⇒ 检测力条件红 ⇒ 不合格") {
        const auto q = qualify(pos, pos, pos, pos); // 用正控冒充负控 ⇒ 不该检出死锁/违例
        INFO(q.reason());
        REQUIRE_FALSE(q.deadlock_detector_live);
        REQUIRE_FALSE(q.memory_detector_live);
        REQUIRE_FALSE(q.qualified());
    }
}
