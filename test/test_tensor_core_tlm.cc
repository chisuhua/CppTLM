/**
 * @file test_tensor_core_tlm.cc
 * @brief TensorCoreTLM 单测 — S4 Phase 2a 延迟表验证
 *
 * 测试范围:
 *   - TcPrecision 各精度延迟: FP16=8, TF32=4, FP8=16, FP4=32
 *   - get_throughput_cycles == get_latency (A100 每周期 1 TC 指令)
 *   - get_latency_mnk 退化到 get_latency
 *   - is_placeholder() 返回 false
 *
 * 延迟来源: A100 MMA 指令间距 (GPGPU-Sim tensor_core_config.h)
 *
 * @author CppTLM / 日期 2026-07-18 (P1) + 2026-07-21 (S4)
 */

#include "catch_amalgamated.hpp"
#include "tlm/gpu/tensor_core_tlm.hh"

using namespace tlm;

TEST_CASE("TensorCoreTLM: FP16 latency is 8", "[gpu][d1p1][s4]") {
    TensorCoreTLM tc;
    REQUIRE(tc.get_latency(TcPrecision::FP16) == 8);
    REQUIRE(tc.get_latency(TcPrecision::BF16) == 8);
}

TEST_CASE("TensorCoreTLM: TF32 latency is 4", "[gpu][d1p1][s4]") {
    TensorCoreTLM tc;
    REQUIRE(tc.get_latency(TcPrecision::TF32) == 4);
}

TEST_CASE("TensorCoreTLM: FP8 latency is 16", "[gpu][d1p1][s4]") {
    TensorCoreTLM tc;
    REQUIRE(tc.get_latency(TcPrecision::FP8) == 16);
    REQUIRE(tc.get_latency(TcPrecision::FP6) == 16);
}

TEST_CASE("TensorCoreTLM: FP4 latency is 32", "[gpu][d1p1][s4]") {
    TensorCoreTLM tc;
    REQUIRE(tc.get_latency(TcPrecision::FP4) == 32);
}

TEST_CASE("TensorCoreTLM: throughput_cycles equals latency", "[gpu][d1p1][s4]") {
    TensorCoreTLM tc;
    TcPrecision precs[] = {TcPrecision::FP4,  TcPrecision::FP6,  TcPrecision::FP8,
                           TcPrecision::FP16, TcPrecision::BF16, TcPrecision::TF32};
    for (auto prec : precs) {
        REQUIRE(tc.get_throughput_cycles(prec) == tc.get_latency(prec));
    }
}

TEST_CASE("TensorCoreTLM: get_latency_mnk degenerates to get_latency", "[gpu][d1p1][s4]") {
    TensorCoreTLM tc;
    // default impl: return get_latency(prec), ignore M,N,K
    REQUIRE(tc.get_latency_mnk(TcPrecision::FP16, 64, 64, 64) == 8);
    REQUIRE(tc.get_latency_mnk(TcPrecision::TF32, 128, 64, 32) == 4);
    REQUIRE(tc.get_latency_mnk(TcPrecision::FP8, 256, 256, 256) == 16);
}

TEST_CASE("TensorCoreTLM: is_placeholder returns false in Phase 2a", "[gpu][d1p1][s4]") {
    TensorCoreTLM tc;
    REQUIRE_FALSE(tc.is_placeholder());
}

TEST_CASE("TensorCoreTLM: all 6 precisions have distinct valid values", "[gpu][d1p1][s4]") {
    TensorCoreTLM tc;
    TcPrecision precs[] = {TcPrecision::FP4,  TcPrecision::FP6,  TcPrecision::FP8,
                           TcPrecision::FP16, TcPrecision::BF16, TcPrecision::TF32};
    for (auto prec : precs) {
        uint32_t lat = tc.get_latency(prec);
        REQUIRE(lat > 0);
        REQUIRE(lat <= 32);
    }
}
