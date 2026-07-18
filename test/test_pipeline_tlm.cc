/**
 * @file test_pipeline_tlm.cc
 * @brief PipelineTLM 单测 (D1-Full P1 #C4, Phase 1 占位实现)
 *
 * 测试范围 (per openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md §1.2):
 *   - P1 占位: get_fractional_cycles 返回 1.0
 *   - get_fractional_cycles_by_type 返回 1.0
 *   - 6 个 PipelineId 值全部返回 1.0
 *   - is_placeholder() 返回 true
 *
 * @author CppTLM / 日期 2026-07-18
 */

#include "catch_amalgamated.hpp"
#include "tlm/gpu/pipeline_tlm.hh"

#include <string>

using namespace tlm;

TEST_CASE("PipelineTLM: get_fractional_cycles returns 1.0 placeholder", "[gpu][d1p1]") {
    PipelineTLM pipe;
    double cycles = pipe.get_fractional_cycles("FMUL", PipelineId::P0_INT_FP32);
    REQUIRE(cycles == 1.0);
}

TEST_CASE("PipelineTLM: get_fractional_cycles_by_type returns 1.0 placeholder", "[gpu][d1p1]") {
    PipelineTLM pipe;
    double cycles = pipe.get_fractional_cycles_by_type(0, PipelineId::V_SIMD);
    REQUIRE(cycles == 1.0);
}

TEST_CASE("PipelineTLM: all 6 pipeline IDs return 1.0", "[gpu][d1p1]") {
    PipelineTLM pipe;
    PipelineId ids[] = {PipelineId::P0_INT_FP32, PipelineId::V_SIMD, PipelineId::P1_FP64,
                        PipelineId::P2_SFU,      PipelineId::P3_LSU,  PipelineId::P4_TC};

    for (auto pid : ids) {
        REQUIRE(pipe.get_fractional_cycles("ADD", pid) == 1.0);
        REQUIRE(pipe.get_fractional_cycles_by_type(1, pid) == 1.0);
    }
}

TEST_CASE("PipelineTLM: is_placeholder returns true in Phase 1", "[gpu][d1p1]") {
    PipelineTLM pipe;
    REQUIRE(pipe.is_placeholder());
}
