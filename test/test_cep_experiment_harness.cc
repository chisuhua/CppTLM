// test/test_cep_experiment_harness.cc
// CEP 实验装置观测层的判据(延迟采集 / 双违例检测 / 死锁 watchdog / metrics 组装)
//
// 作者: CppTLM Team / 日期: 2026-11-04
//
// 纪律:
//   · 每条负向断言配**正控**
//   · 反向 fixture 必须**因正确原因红**(漏打时间戳 / 检测器空检查 / 空闲误判死锁)
//   · 0 样本 ⇒ 0,不得静默产出非零值

#include "catch_amalgamated.hpp"

#include "cep_experiment_harness.hh"

using namespace cep_exp;

// ══════════════════════════════════════════════════════════════════
// ① LatencyCollector
// ══════════════════════════════════════════════════════════════════

TEST_CASE("观测层 · LatencyCollector 已知样本的分布可复现", "[cep][exp][harness]") {
    LatencyCollector c;
    // 样本 10 / 20 / 30 / 40 / 50 ⇒ avg 30
    for (std::uint64_t lat : {10u, 20u, 30u, 40u, 50u}) {
        c.note_issue();
        REQUIRE(c.note_complete(100, 100 + lat));
    }

    SECTION("计数与完整性") {
        REQUIRE(c.issued() == 5);
        REQUIRE(c.completed() == 5);
        REQUIRE(c.sample_count() == 5);
        REQUIRE(c.audit_balanced()); // 正控:配对完整
        REQUIRE(c.underflow() == 0);
    }

    SECTION("分布量") {
        REQUIRE(c.avg() == Catch::Approx(30.0));
        REQUIRE(c.p50() == 30); // nearest-rank:ceil(0.5×5)=3 → 第 3 个
        REQUIRE(c.p95() == 50); // ceil(0.95×5)=5
        REQUIRE(c.p99() == 50);
        REQUIRE(c.max() == 50);
        REQUIRE(c.stddev() == Catch::Approx(15.8113883).epsilon(0.01));
    }
}

TEST_CASE("观测层 · 反向 fixture:漏打 complete ⇒ audit 必须红", "[cep][exp][harness][reverse]") {
    LatencyCollector c;
    c.note_issue();
    c.note_issue(); // 两次 issue,只 complete 一次(模拟漏打时间戳)

    REQUIRE(c.note_complete(0, 10));
    REQUIRE(c.issued() == 2);
    REQUIRE(c.completed() == 1);

    // ⚠️ 这正是「不得静默产 0」的判据:漏样本必须让收尾自检失败
    REQUIRE_FALSE(c.audit_balanced());
    // 且分布只基于**真实收到的**样本(不得把漏掉的当 0 延迟混入)
    REQUIRE(c.sample_count() == 1);
    REQUIRE(c.avg() == Catch::Approx(10.0));
}

TEST_CASE("观测层 · 反向 fixture:时间戳倒挂 ⇒ 样本被拒", "[cep][exp][harness][reverse]") {
    LatencyCollector c;
    c.note_issue();
    REQUIRE_FALSE(c.note_complete(100, 90)); // complete < issue
    REQUIRE(c.underflow() == 1);
    REQUIRE(c.sample_count() == 0);
    REQUIRE_FALSE(c.audit_balanced()); // 有倒挂 ⇒ 收尾自检必须红
}

TEST_CASE("观测层 · 0 样本时分布量恒 0(不静默产非零)", "[cep][exp][harness]") {
    LatencyCollector c;
    REQUIRE(c.avg() == 0.0);
    REQUIRE(c.p50() == 0);
    REQUIRE(c.p95() == 0);
    REQUIRE(c.p99() == 0);
    REQUIRE(c.max() == 0);
    REQUIRE(c.stddev() == 0.0);
    REQUIRE(c.audit_balanced()); // 空但配对(0 == 0)
}

// ══════════════════════════════════════════════════════════════════
// ② SwmrDataValueDetector
// ══════════════════════════════════════════════════════════════════

TEST_CASE("观测层 · 合法写读 ⇒ 双不变量 0 违例(正控)", "[cep][exp][harness]") {
    SwmrDataValueDetector d;
    d.write(0x1000, /*version=*/1);
    d.read(0x1000, /*observed=*/1);
    d.write(0x1000, /*version=*/2); // 合法:调用方已履行 invalidate
    d.read(0x1000, /*observed=*/2);

    REQUIRE(d.counts().swmr == 0);
    REQUIRE(d.counts().data_value == 0);
    REQUIRE(d.counts().total() == 0);
}

TEST_CASE("观测层 · 未先 invalidate 的二次写 ⇒ SWMR 违例", "[cep][exp][harness]") {
    SwmrDataValueDetector d;
    d.write(0x2000, 1);
    d.write_without_invalidate(0x2000, 2); // 该地址已写过且未失效

    REQUIRE(d.counts().swmr == 1);
    REQUIRE(d.counts().data_value == 0);
}

TEST_CASE("观测层 · 读到过期版本 ⇒ data-value 违例", "[cep][exp][harness]") {
    SwmrDataValueDetector d;
    d.write(0x3000, /*version=*/7);
    d.read(0x3000, /*observed=*/6); // 读到旧版本

    REQUIRE(d.counts().data_value == 1);
    REQUIRE(d.counts().swmr == 0);
}

TEST_CASE("观测层 · 红绿分离:检测器被变异成空检查 ⇒ 已知违例检出 0",
          "[cep][exp][harness][reverse]") {
    // 同一违例 workload 跑两个变体。
    const auto run = [](bool enabled) {
        SwmrDataValueDetector d(enabled);
        d.write(0x4000, 1);
        d.write_without_invalidate(0x4000, 2); // SWMR 违例
        d.read(0x4000, 99);                    // data-value 违例
        return d.counts().total();
    };

    REQUIRE(run(true) == 2);  // 正控:两个违例都被检出
    REQUIRE(run(false) == 0); // 变异体:空检查 ⇒ 0 ⇒ 证明上面的 2 不是构造出来的
}

TEST_CASE("观测层 · 从未写过的地址被读 ⇒ 不判违例(防恒真)", "[cep][exp][harness]") {
    SwmrDataValueDetector d;
    d.read(0x5000, 123); // 无任何写 ⇒ 无从比较版本
    REQUIRE(d.counts().total() == 0);
}

// ══════════════════════════════════════════════════════════════════
// ③ ProgressWatchdog
// ══════════════════════════════════════════════════════════════════

TEST_CASE("观测层 · in-flight 非空且长期无 retire ⇒ 死锁", "[cep][exp][harness]") {
    ProgressWatchdog w(/*threshold=*/100);
    for (int i = 0; i < 150; ++i)
        w.tick(/*in_flight=*/3, /*retired=*/0);

    REQUIRE(w.stalled());
    REQUIRE(w.max_idle_run() >= 100);
}

TEST_CASE("观测层 · 反向 fixture:空闲(无 in-flight)不得判死锁", "[cep][exp][harness][reverse]") {
    ProgressWatchdog w(/*threshold=*/100);
    for (int i = 0; i < 10000; ++i)
        w.tick(/*in_flight=*/0, /*retired=*/0);

    REQUIRE_FALSE(w.stalled()); // 空闲不是死锁
    REQUIRE(w.max_idle_run() == 0);
}

TEST_CASE("观测层 · 有 retire 即清零(不得累计出假死锁)", "[cep][exp][harness]") {
    ProgressWatchdog w(/*threshold=*/100);
    for (int i = 0; i < 90; ++i)
        w.tick(2, 0);
    w.tick(2, 1); // 一次 retire ⇒ 活性证据
    for (int i = 0; i < 90; ++i)
        w.tick(2, 0);

    REQUIRE_FALSE(w.stalled()); // 两段 90 不连续
}

// ══════════════════════════════════════════════════════════════════
// ④ ExperimentMetrics
// ══════════════════════════════════════════════════════════════════

TEST_CASE("观测层 · metrics 组装:13 必纳字段齐备 + 自洽", "[cep][exp][harness]") {
    ExperimentMetrics m;
    m.total_cycles = 1000;
    m.total_transactions = 5;
    for (std::uint64_t lat : {10u, 20u, 30u, 40u, 50u}) {
        m.latency.note_issue();
        m.latency.note_complete(0, lat);
    }
    m.verification_mode = "bounded_random";
    m.verification_scope = "5 事务 / 1000 cycle / 单节点";

    REQUIRE(m.well_formed());

    const std::string js = m.to_json();
    for (const char* key :
         {"total_cycles", "total_transactions", "transactions_per_cycle", "avg_latency_cycles",
          "p50_latency_cycles", "p95_latency_cycles", "p99_latency_cycles", "max_latency_cycles",
          "coherence_stalls", "collective_stalls", "directory_misses", "deadlock_violations",
          "memory_violations"}) {
        INFO("缺字段: " << key);
        REQUIRE(js.find(std::string("\"") + key + "\"") != std::string::npos);
    }
    REQUIRE(js.find("\"verification_mode\": \"bounded_random\"") != std::string::npos);
    REQUIRE(js.find("\"verification_scope\": \"5 事务") != std::string::npos);
}

TEST_CASE("观测层 · 反向 fixture:metrics 三处自洽性缺失必须被拒", "[cep][exp][harness][reverse]") {
    SECTION("verification_scope 为空") {
        ExperimentMetrics m;
        m.total_cycles = 1;
        m.total_transactions = 1;
        m.latency.note_issue();
        m.latency.note_complete(0, 1);
        m.verification_mode = "sampled";
        m.verification_scope = "";
        REQUIRE_FALSE(m.well_formed());
    }
    SECTION("verification_mode 非法") {
        ExperimentMetrics m;
        m.total_cycles = 1;
        m.total_transactions = 1;
        m.latency.note_issue();
        m.latency.note_complete(0, 1);
        m.verification_mode = "proven"; // ⛔ ADR-CEP-06 §3 之外
        m.verification_scope = "1 事务";
        REQUIRE_FALSE(m.well_formed());
    }
    SECTION("total_transactions 与 issue 数不符") {
        ExperimentMetrics m;
        m.total_cycles = 1;
        m.total_transactions = 9; // 与 latency.issued()==1 不符
        m.latency.note_issue();
        m.latency.note_complete(0, 1);
        m.verification_mode = "sampled";
        m.verification_scope = "1 事务";
        REQUIRE_FALSE(m.well_formed());
    }
}
