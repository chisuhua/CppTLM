/**
 * @file test_tensor_core_tlm.cc
 * @brief TensorCoreTLM 单测 (D1-Full P1 #C4, Phase 1 占位实现)
 *
 * 测试范围 (per openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md §1.3):
 *   - P1 占位: get_latency 返回 1
 *   - get_throughput_cycles 返回 1
 *   - 6 个 TcPrecision 值全部返回 1
 *   - get_latency_mnk 用 vendor default impl 退化到 get_latency (=1)
 *   - is_placeholder() 返回 true
 *
 * @author CppTLM / 日期 2026-07-18
 */

#include "catch_amalgamated.hpp"
#include "tlm/gpu/tensor_core_tlm.hh"

using namespace tlm;

TEST_CASE("TensorCoreTLM: get_latency returns 1 placeholder", "[gpu][d1p1]") {
    TensorCoreTLM tc;
    REQUIRE(tc.get_latency(TcPrecision::FP16) == 1);
}

TEST_CASE("TensorCoreTLM: get_throughput_cycles returns 1 placeholder", "[gpu][d1p1]") {
    TensorCoreTLM tc;
    REQUIRE(tc.get_throughput_cycles(TcPrecision::FP16) == 1);
}

TEST_CASE("TensorCoreTLM: all 6 TC precisions return 1", "[gpu][d1p1]") {
    TensorCoreTLM tc;
    TcPrecision precs[] = {TcPrecision::FP4, TcPrecision::FP6, TcPrecision::FP8,
                           TcPrecision::FP16, TcPrecision::BF16, TcPrecision::TF32};

    for (auto prec : precs) {
        REQUIRE(tc.get_latency(prec) == 1);
        REQUIRE(tc.get_throughput_cycles(prec) == 1);
    }
}

TEST_CASE("TensorCoreTLM: get_latency_mnk degenerates to get_latency (vendor default)",
          "[gpu][d1p1]") {
    TensorCoreTLM tc;
    REQUIRE(tc.get_latency_mnk(TcPrecision::FP16, 64, 64, 64) == 1);
}

TEST_CASE("TensorCoreTLM: is_placeholder returns true in Phase 1", "[gpu][d1p1]") {
    TensorCoreTLM tc;
    REQUIRE(tc.is_placeholder());
}
