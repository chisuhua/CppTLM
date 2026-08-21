// test/test_cuda_core_adapter_mvp_pipeline.cc
// =============================================================================
// CudaCoreAdapterMVP — PipelineTLM latency 注入单元测试 (S1 / T-s1-4 §6.c)
// 功能: 验证注入后的 pipeline 实例按 A100 延迟表返回 fractional cycles:
//       P0_INT_FP32: fma=4.22 / 通用整数=1.0; P3_LSU: ld.global=200 /
//       st.shared=1.0; P4_TC 委托 TensorCoreTLM。
// 约束: 仅用 facade API + adapter API + CppTLM timing 模块头 —
//       不 include 任何 PTX-EMU 头。
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-4, Sisyphus)
// 标签 [cuda-core][mvp][pipeline]
// =============================================================================

#include "catch_amalgamated.hpp"

#include "tlm/gpu/cuda_core_adapter_mvp.hh"
#include "tlm/gpu/ptx_emu_submodule_mvp.hh"
#include "tlm/gpu/pipeline_tlm.hh"
#include "tlm/gpu/tensor_core_tlm.hh"

TEST_CASE("cuda_core_adapter_pipeline_latency_table",
          "[cuda-core][mvp][pipeline]") {
    tlm::PtxEmuSubmoduleMVP facade;
    tlm::CudaCoreAdapterMVP adapter;
    adapter.init(facade);
    adapter.inject_timing_modules();

    tlm::PipelineTLM* pipe = adapter.pipeline();
    REQUIRE(pipe != nullptr);

    // P0_INT_FP32: FMA/FP32 算术 = 4.22 cycles (A100 §5.4.1)
    CHECK(pipe->get_fractional_cycles("fma.rn.f32", PipelineId::P0_INT_FP32)
          == Catch::Approx(4.22));

    // P0_INT_FP32: 通用整数/移动指令 = 1.0 cycle
    CHECK(pipe->get_fractional_cycles("add.u32", PipelineId::P0_INT_FP32)
          == Catch::Approx(1.0));
    CHECK(pipe->get_fractional_cycles("mov.u32", PipelineId::P0_INT_FP32)
          == Catch::Approx(1.0));

    // P0_INT_FP32: 整数乘加 = 2.0 cycles
    CHECK(pipe->get_fractional_cycles("mad.lo.s32", PipelineId::P0_INT_FP32)
          == Catch::Approx(2.0));

    // P3_LSU: 全局加载 = 200 cycles (DRAM + NoC + cache miss)
    CHECK(pipe->get_fractional_cycles("ld.global.u32", PipelineId::P3_LSU)
          == Catch::Approx(200.0));
    // P3_LSU: 共享内存 = 1.0 cycle (on-chip SRAM)
    CHECK(pipe->get_fractional_cycles("st.shared.u32", PipelineId::P3_LSU)
          == Catch::Approx(1.0));

    facade.shutdown();
}

TEST_CASE("cuda_core_adapter_pipeline_tc_delegates_to_tensor_core",
          "[cuda-core][mvp][pipeline]") {
    tlm::PtxEmuSubmoduleMVP facade;
    tlm::CudaCoreAdapterMVP adapter;
    adapter.init(facade);
    adapter.inject_timing_modules();

    tlm::PipelineTLM* pipe = adapter.pipeline();
    tlm::TensorCoreTLM* tc = adapter.tensor_core();
    REQUIRE(pipe != nullptr);
    REQUIRE(tc != nullptr);

    // P4_TC: PipelineTLM 返回 0 — 延迟由 TensorCoreTLM 承担
    CHECK(pipe->get_fractional_cycles("mma.sync.aligned.m16n8k16.f32.f16.f16.f32",
                                      PipelineId::P4_TC)
          == Catch::Approx(0.0));

    // TensorCoreTLM: A100 MMA 指令间距 (FP16=8, TF32=4, FP8=16)
    CHECK(tc->get_latency(TcPrecision::FP16) == 8u);
    CHECK(tc->get_latency(TcPrecision::TF32) == 4u);
    CHECK(tc->get_latency(TcPrecision::FP8) == 16u);

    facade.shutdown();
}
