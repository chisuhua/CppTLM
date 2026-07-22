/**
 * @file test_latency_tlm_perf.cc
 * @brief PipelineTLM / TensorCoreTLM 查询开销微基准 — S7 Phase 3 性能测量
 *
 * 测量维度:
 *   - string-based 查询: 各管线 best-path / fallback 延迟
 *   - type-based 查询: switch 管线平均值延迟
 *   - TensorCoreTLM: get_latency / get_throughput_cycles 延迟
 *
 * 预期: per-query < 100ns (string 含子串匹配, type 纯 switch)
 * 硬件基准: 全在 CPU 上运行, 无 GPU 加速
 *
 * @author CppTLM / 日期 2026-07-21
 */

#include "catch_amalgamated.hpp"
#include "tlm/gpu/pipeline_tlm.hh"
#include "tlm/gpu/tensor_core_tlm.hh"

#include <chrono>
#include <string>

using namespace tlm;
using namespace std::chrono;

namespace {

/// 微秒级计时辅助
struct Timer {
    high_resolution_clock::time_point t0 = high_resolution_clock::now();
    double us() const {
        return duration_cast<nanoseconds>(high_resolution_clock::now() - t0).count() / 1000.0;
    }
};

constexpr int kWarmup = 100;
constexpr int kIters  = 100000;

}  // namespace

// ===== PipelineTLM 字符串查询 =====

TEST_CASE("pipeline perf: P0 best-path (add.s32) string lookup", "[gpu][perf]") {
    PipelineTLM pipe;
    // warmup
    for (int i = 0; i < kWarmup; ++i)
        pipe.get_fractional_cycles("add.s32", PipelineId::P0_INT_FP32);

    Timer t;
    for (int i = 0; i < kIters; ++i)
        pipe.get_fractional_cycles("add.s32", PipelineId::P0_INT_FP32);

    double ns_per_call = t.us() * 1e3 / kIters;
    INFO("P0 best-path: " << ns_per_call << " ns/call");
    REQUIRE(ns_per_call < 1000.0);  // 含 string 子串匹配
}

TEST_CASE("pipeline perf: P0 fma.f32 string lookup (4.22 path)", "[gpu][perf]") {
    PipelineTLM pipe;
    for (int i = 0; i < kWarmup; ++i)
        pipe.get_fractional_cycles("fma.rn.f32", PipelineId::P0_INT_FP32);

    Timer t;
    for (int i = 0; i < kIters; ++i)
        pipe.get_fractional_cycles("fma.rn.f32", PipelineId::P0_INT_FP32);

    double ns_per_call = t.us() * 1e3 / kIters;
    INFO("P0 fma path: " << ns_per_call << " ns/call");
    REQUIRE(ns_per_call < 1000.0);
}

TEST_CASE("pipeline perf: P3 ld.global string lookup (200 path)", "[gpu][perf]") {
    PipelineTLM pipe;
    for (int i = 0; i < kWarmup; ++i)
        pipe.get_fractional_cycles("ld.global.u32", PipelineId::P3_LSU);

    Timer t;
    for (int i = 0; i < kIters; ++i)
        pipe.get_fractional_cycles("ld.global.u32", PipelineId::P3_LSU);

    double ns_per_call = t.us() * 1e3 / kIters;
    INFO("P3 ld.global: " << ns_per_call << " ns/call");
    REQUIRE(ns_per_call < 500.0);
}

TEST_CASE("pipeline perf: P0 fallback (unknown instr) string lookup", "[gpu][perf]") {
    PipelineTLM pipe;
    for (int i = 0; i < kWarmup; ++i)
        pipe.get_fractional_cycles("unknown.xyz", PipelineId::P0_INT_FP32);

    Timer t;
    for (int i = 0; i < kIters; ++i)
        pipe.get_fractional_cycles("unknown.xyz", PipelineId::P0_INT_FP32);

    double ns_per_call = t.us() * 1e3 / kIters;
    INFO("P0 fallback: " << ns_per_call << " ns/call");
    REQUIRE(ns_per_call < 1000.0);  // 全子串扫描最坏路径
}

// ===== PipelineTLM type-based (核心热路径) =====

TEST_CASE("pipeline perf: type-based P0 average (2.0)", "[gpu][perf]") {
    PipelineTLM pipe;
    for (int i = 0; i < kWarmup; ++i)
        pipe.get_fractional_cycles_by_type(0, PipelineId::P0_INT_FP32);

    Timer t;
    for (int i = 0; i < kIters; ++i)
        pipe.get_fractional_cycles_by_type(0, PipelineId::P0_INT_FP32);

    double ns_per_call = t.us() * 1e3 / kIters;
    INFO("type-based P0: " << ns_per_call << " ns/call");
    REQUIRE(ns_per_call < 100.0);  // 纯 switch, 应极快
}

TEST_CASE("pipeline perf: type-based all 6 pipelines", "[gpu][perf]") {
    PipelineTLM pipe;
    PipelineId ids[] = {PipelineId::P0_INT_FP32, PipelineId::V_SIMD, PipelineId::P1_FP64,
                        PipelineId::P2_SFU,      PipelineId::P3_LSU,  PipelineId::P4_TC};

    for (auto pid : ids)
        for (int i = 0; i < kWarmup; ++i)
            pipe.get_fractional_cycles_by_type(0, pid);

    Timer t;
    for (int i = 0; i < kIters; ++i)
        for (auto pid : ids)
            pipe.get_fractional_cycles_by_type(0, pid);

    double ns_per_call = t.us() * 1e3 / (kIters * 6);
    INFO("type-based 6-pipe avg: " << ns_per_call << " ns/call");
    REQUIRE(ns_per_call < 100.0);
}

// ===== TensorCoreTLM =====

TEST_CASE("tc perf: get_latency FP16", "[gpu][perf]") {
    TensorCoreTLM tc;
    for (int i = 0; i < kWarmup; ++i)
        tc.get_latency(TcPrecision::FP16);

    Timer t;
    for (int i = 0; i < kIters; ++i)
        tc.get_latency(TcPrecision::FP16);

    double ns_per_call = t.us() * 1e3 / kIters;
    INFO("TC FP16 latency: " << ns_per_call << " ns/call");
    REQUIRE(ns_per_call < 100.0);
}

TEST_CASE("tc perf: get_latency FP4 (worst-case switch position)", "[gpu][perf]") {
    TensorCoreTLM tc;
    for (int i = 0; i < kWarmup; ++i)
        tc.get_latency(TcPrecision::FP4);

    Timer t;
    for (int i = 0; i < kIters; ++i)
        tc.get_latency(TcPrecision::FP4);

    double ns_per_call = t.us() * 1e3 / kIters;
    INFO("TC FP4 latency: " << ns_per_call << " ns/call");
    REQUIRE(ns_per_call < 100.0);
}

TEST_CASE("tc perf: get_latency_mnk degenerates to get_latency", "[gpu][perf]") {
    TensorCoreTLM tc;
    for (int i = 0; i < kWarmup; ++i)
        tc.get_latency_mnk(TcPrecision::FP16, 64, 64, 64);

    Timer t;
    for (int i = 0; i < kIters; ++i)
        tc.get_latency_mnk(TcPrecision::FP16, 64, 64, 64);

    double ns_per_call = t.us() * 1e3 / kIters;
    INFO("TC mnk default: " << ns_per_call << " ns/call");
    REQUIRE(ns_per_call < 100.0);
}