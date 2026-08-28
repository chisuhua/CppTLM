/**
 * @file test_pipeline_tlm.cc
 * @brief PipelineTLM 单测 — S4/S5 延迟表验证 (PTX 指令名)
 *
 * 测试基于 PTX 指令名格式 (e.g. "add.f32", "ld.global.u32", "fma.rn.f32")
 * 延迟来源: A100 whitepaper + GPGPU-Sim
 *
 * @author CppTLM / 日期 2026-07-18 (P1) + 2026-07-21 (S4/S5)
 */

#include "catch_amalgamated.hpp"
#include "tlm/gpu/pipeline_tlm.hh"

#include <string>

using namespace tlm;

// ===== P0_INT_FP32: FP32/整数管线 =====

TEST_CASE("PipelineTLM: P0 fma/mul.f32/add.f32 = 4.22", "[gpu][d1p1][s4]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("fma.rn.f32", PipelineId::P0_INT_FP32) == 4.22);
    REQUIRE(pipe.get_fractional_cycles("mul.f32", PipelineId::P0_INT_FP32) == 4.22);
    REQUIRE(pipe.get_fractional_cycles("add.f32", PipelineId::P0_INT_FP32) == 4.22);
}

TEST_CASE("PipelineTLM: P0 simples ops = 1.0", "[gpu][d1p1][s4]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("add.s32", PipelineId::P0_INT_FP32) == 1.0);
    REQUIRE(pipe.get_fractional_cycles("mov.u32", PipelineId::P0_INT_FP32) == 1.0);
    REQUIRE(pipe.get_fractional_cycles("setp.eq.s32", PipelineId::P0_INT_FP32) == 1.0);
    REQUIRE(pipe.get_fractional_cycles("sub.s32", PipelineId::P0_INT_FP32) == 1.0);
}

TEST_CASE("PipelineTLM: P0 mad/mul.lo = 2.0", "[gpu][d1p1][s4]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("mad.lo.s32", PipelineId::P0_INT_FP32) == 2.0);
    REQUIRE(pipe.get_fractional_cycles("mul.lo.s32", PipelineId::P0_INT_FP32) == 2.0);
}

// ===== P3_LSU: 加载/存储 =====

TEST_CASE("PipelineTLM: P3 ld.global = 200", "[gpu][d1p1][s4]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("ld.global.u32", PipelineId::P3_LSU) == 200.0);
    REQUIRE(pipe.get_fractional_cycles("ld.global.f32", PipelineId::P3_LSU) == 200.0);
}

TEST_CASE("PipelineTLM: P3 st.global = 20", "[gpu][d1p1][s4]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("st.global.u32", PipelineId::P3_LSU) == 20.0);
}

TEST_CASE("PipelineTLM: P3 ld.shared/st.shared = 1.0", "[gpu][d1p1][s4]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("ld.shared.u32", PipelineId::P3_LSU) == 1.0);
    REQUIRE(pipe.get_fractional_cycles("st.shared.u32", PipelineId::P3_LSU) == 1.0);
}

TEST_CASE("PipelineTLM: P3 ld.local/st.local = 5.0", "[gpu][d1p1][s4]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("ld.local.u32", PipelineId::P3_LSU) == 5.0);
    REQUIRE(pipe.get_fractional_cycles("st.local.u32", PipelineId::P3_LSU) == 5.0);
}

TEST_CASE("PipelineTLM: P3 atom = 200", "[gpu][d1p1][s4]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("atom.global.add.u32", PipelineId::P3_LSU) == 200.0);
}

// ===== P4_TC =====

TEST_CASE("PipelineTLM: P4_TC = 0 (delegated to TensorCoreTLM)", "[gpu][d1p1][s4]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("mma.sync.aligned.m16n8k16", PipelineId::P4_TC) == 0.0);
    REQUIRE(pipe.get_fractional_cycles_by_type(0, PipelineId::P4_TC) == 0.0);
}

// ===== type 平均值 =====

TEST_CASE("PipelineTLM: type-based averages", "[gpu][d1p1][s4]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles_by_type(0, PipelineId::P0_INT_FP32) == 2.0);
    REQUIRE(pipe.get_fractional_cycles_by_type(0, PipelineId::V_SIMD) == 1.0);
    REQUIRE(pipe.get_fractional_cycles_by_type(0, PipelineId::P1_FP64) == 8.0);
    REQUIRE(pipe.get_fractional_cycles_by_type(0, PipelineId::P2_SFU) == 8.0);
    REQUIRE(pipe.get_fractional_cycles_by_type(0, PipelineId::P3_LSU) == 200.0);
}

// ===== S5 Phase 2b: V_SIMD / P1_FP64 / P2_SFU =====

TEST_CASE("PipelineTLM: V_SIMD = 1.0", "[gpu][d1p1][s5]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("vadd2", PipelineId::V_SIMD) == 1.0);
    REQUIRE(pipe.get_fractional_cycles_by_type(0, PipelineId::V_SIMD) == 1.0);
}

TEST_CASE("PipelineTLM: P1_FP64 = 8.0", "[gpu][d1p1][s5]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("add.f64", PipelineId::P1_FP64) == 8.0);
    REQUIRE(pipe.get_fractional_cycles("fma.rn.f64", PipelineId::P1_FP64) == 8.0);
}

TEST_CASE("PipelineTLM: P2 sin/cos = 16.0", "[gpu][d1p1][s5]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("sin.approx.f32", PipelineId::P2_SFU) == 16.0);
    REQUIRE(pipe.get_fractional_cycles("cos.approx.f32", PipelineId::P2_SFU) == 16.0);
}

TEST_CASE("PipelineTLM: P2 rcp/rsqrt = 4.0", "[gpu][d1p1][s5]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("rcp.approx.f32", PipelineId::P2_SFU) == 4.0);
}

TEST_CASE("PipelineTLM: P2 sqrt = 8.0", "[gpu][d1p1][s5]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("sqrt.approx.f32", PipelineId::P2_SFU) == 8.0);
}

TEST_CASE("PipelineTLM: P2 lg2/ex2 = 8.0", "[gpu][d1p1][s5]") {
    PipelineTLM pipe;
    REQUIRE(pipe.get_fractional_cycles("lg2.approx.f32", PipelineId::P2_SFU) == 8.0);
    REQUIRE(pipe.get_fractional_cycles("ex2.approx.f32", PipelineId::P2_SFU) == 8.0);
}

// ===== 非 placeholder =====

TEST_CASE("PipelineTLM: is_placeholder = false", "[gpu][d1p1][s4]") {
    PipelineTLM pipe;
    REQUIRE_FALSE(pipe.is_placeholder());
}

// ===== 全部 6 管线均有有效值 =====

TEST_CASE("PipelineTLM: all 6 pipelines have valid non-negative values", "[gpu][d1p1][s4]") {
    PipelineTLM pipe;
    PipelineId ids[] = {PipelineId::P0_INT_FP32, PipelineId::V_SIMD, PipelineId::P1_FP64,
                        PipelineId::P2_SFU,      PipelineId::P3_LSU, PipelineId::P4_TC};
    for (auto pid : ids) {
        double v = pipe.get_fractional_cycles("add.f32", pid);
        double t = pipe.get_fractional_cycles_by_type(0, pid);
        REQUIRE(v >= 0.0);
        REQUIRE(t >= 0.0);
    }
}
