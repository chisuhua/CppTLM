/**
 * @file test_gpu_soc_perf.cc
 * @brief GpuSocTLM tick 开销 + MemoryBridge poll_kernel 性能 — S7 Phase 3
 *
 * 测量维度:
 *   - Pool 分配/释放吞吐: PacketPool + TransactionInfo 对象池性能
 *   - ScoreboardTLM: allocate/release/has_free_entry 吞吐
 *   - MemoryBridge poll_kernel: 完成/未完成路径延迟
 *
 * 真实 tick overhead 需 PTX-EMU .so 链接; 此处测量 CppTLM 端独立可测组件.
 *
 * @author CppTLM / 日期 2026-07-21
 */

#include "catch_amalgamated.hpp"
#include "tlm/gpu/scoreboard_tlm.hh"
#include "tlm/gpu/memory_bridge.hh"
#include "tlm/gpu/kernel_launch_tlm.hh"
#include "tlm/gpu/ptx_emu_driver.hh"

#include <chrono>
#include <memory>

using namespace tlm;
using namespace std::chrono;

namespace {

struct Timer {
    high_resolution_clock::time_point t0 = high_resolution_clock::now();
    double us() const {
        return duration_cast<nanoseconds>(high_resolution_clock::now() - t0).count() / 1000.0;
    }
};

constexpr int kWarmup = 100;
constexpr int kIters  = 100000;

}  // namespace

// ===== ScoreboardTLM =====

TEST_CASE("sb perf: has_free_entry throughput", "[gpu][perf]") {
    ScoreboardTLM sb;
    for (int i = 0; i < kWarmup; ++i) sb.has_free_entry();

    Timer t;
    for (int i = 0; i < kIters; ++i) sb.has_free_entry();

    double ns = t.us() * 1e3 / kIters;
    INFO("ScoreboardTLM::has_free_entry: " << ns << " ns/call");
    REQUIRE(ns < 100.0);
}

TEST_CASE("sb perf: allocate + release cycle", "[gpu][perf]") {
    ScoreboardTLM sb;
    for (int i = 0; i < kWarmup; ++i) {
        sb.allocate(i % 2048, i % 64);
        sb.release(i % 2048, i % 64);
    }

    Timer t;
    int hits = 0;
    for (int i = 0; i < kIters; ++i) {
        if (sb.allocate(i % 2048, i % 64)) ++hits;
        sb.release(i % 2048, i % 64);
    }

    double ns = t.us() * 1e3 / kIters;
    INFO("ScoreboardTLM alloc+release: " << ns << " ns/cycle, hits=" << hits);
    REQUIRE(ns < 200.0);
}

TEST_CASE("sb perf: full table allocate rejects", "[gpu][perf]") {
    ScoreboardTLM sb;
    // fill to capacity
    for (uint32_t i = 0; i < 2048; ++i)
        sb.allocate(i, 0);

    for (int i = 0; i < kWarmup; ++i) sb.allocate(0, 1);

    Timer t;
    for (int i = 0; i < kIters; ++i) sb.allocate(0, 1);

    double ns = t.us() * 1e3 / kIters;
    INFO("full-table reject: " << ns << " ns/call");
    REQUIRE(ns < 100.0);
}

// ===== MemoryBridge poll_kernel =====

class MockCompleteDriver : public IPtxEmuDriver {
public:
    MockCompleteDriver(bool complete = true) : c_(complete) {}
    AdvanceResult advance(uint32_t, uint32_t&) override { return AdvanceResult::NoOp; }
    bool is_kernel_complete(uint64_t) override { return c_; }
    void inject_scoreboard(uint32_t, std::unique_ptr<IScoreboard>) override {}
    void inject_pipeline(uint32_t, std::unique_ptr<IPipelineLatencyProvider>) override {}
    void inject_tensor_core(uint32_t, std::unique_ptr<ITensorCoreTiming>) override {}
    uint32_t num_sms() const override { return 0; }
private:
    bool c_;
};

TEST_CASE("mb perf: poll_kernel complete path (driver says done)", "[gpu][perf]") {
    EventQueue eq;
    KernelLaunchTLM kl("perf_kl", &eq);
    MockCompleteDriver driver(true);
    kl.set_ptx_emu_driver(&driver);

    MemoryBridge bridge(&kl, nullptr);

    // submit then poll loop
    for (int i = 0; i < 100; ++i)
        bridge.submit_kernel(i, "k", 1, 1, 1, 32, 1, 1, nullptr, 0, 0, 0);

    for (int i = 0; i < kWarmup; ++i)
        bridge.poll_kernel(i % 100);

    Timer t;
    for (int i = 0; i < kIters; ++i)
        bridge.poll_kernel(i % 100);

    double ns = t.us() * 1e3 / kIters;
    INFO("poll_kernel complete: " << ns << " ns/call");
    REQUIRE(ns < 500.0);
}

TEST_CASE("mb perf: poll_kernel NOT complete path (driver says not done)", "[gpu][perf]") {
    EventQueue eq;
    KernelLaunchTLM kl("perf_kl", &eq);
    MockCompleteDriver driver(false);
    kl.set_ptx_emu_driver(&driver);

    MemoryBridge bridge(&kl, nullptr);

    for (int i = 0; i < 100; ++i)
        bridge.submit_kernel(i, "k", 1, 1, 1, 32, 1, 1, nullptr, 0, 0, 0);

    for (int i = 0; i < kWarmup; ++i)
        bridge.poll_kernel(i % 100);

    Timer t;
    for (int i = 0; i < kIters; ++i)
        bridge.poll_kernel(i % 100);

    double ns = t.us() * 1e3 / kIters;
    INFO("poll_kernel NOT complete: " << ns << " ns/call");
    REQUIRE(ns < 500.0);
}
