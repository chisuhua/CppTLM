// test/test_cuda_core_adapter_mvp_injection.cc
// =============================================================================
// CudaCoreAdapterMVP — 4 个 timing 模块注入路径单元测试 (S1 / T-s1-4 §6.f)
// 功能: 验证 inject_timing_modules() 创建 MinimalWarpSchedulerTLM /
//       ScoreboardTLM / PipelineTLM / TensorCoreTLM 4 个 owned 实例,
//       幂等 (重复调用不重建), 且注入后 tick() 正常推进 cycle
//       (SM 侧 set_*() 注入不破坏 exe_once 路径)。
// 约束: 仅用 facade API + adapter API + CppTLM timing 模块头 —
//       不 include 任何 PTX-EMU 头。
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-4, Sisyphus)
// 标签 [cuda-core][mvp][injection]
// =============================================================================

#include "catch_amalgamated.hpp"

#include "tlm/gpu/cuda_core_adapter_mvp.hh"
#include "tlm/gpu/minimal_warp_scheduler_tlm.hh"
#include "tlm/gpu/pipeline_tlm.hh"
#include "tlm/gpu/ptx_emu_submodule_mvp.hh"
#include "tlm/gpu/scoreboard_tlm.hh"
#include "tlm/gpu/tensor_core_tlm.hh"

TEST_CASE("cuda_core_adapter_injection_populates_four_modules",
          "[cuda-core][mvp][injection]") {
    tlm::PtxEmuSubmoduleMVP facade;
    tlm::CudaCoreAdapterMVP adapter;

    // 注入前: 4 个实例均为 null
    CHECK(adapter.warp_scheduler() == nullptr);
    CHECK(adapter.scoreboard() == nullptr);
    CHECK(adapter.pipeline() == nullptr);
    CHECK(adapter.tensor_core() == nullptr);
    CHECK_FALSE(adapter.timing_injected());

    adapter.init(facade);
    adapter.inject_timing_modules();

    // 注入后: 4 个实例全部就绪
    CHECK(adapter.warp_scheduler() != nullptr);
    CHECK(adapter.scoreboard() != nullptr);
    CHECK(adapter.pipeline() != nullptr);
    CHECK(adapter.tensor_core() != nullptr);
    CHECK(adapter.timing_injected());

    facade.shutdown();
}

TEST_CASE("cuda_core_adapter_injection_idempotent",
          "[cuda-core][mvp][injection]") {
    tlm::PtxEmuSubmoduleMVP facade;
    tlm::CudaCoreAdapterMVP adapter;
    adapter.init(facade);
    adapter.inject_timing_modules();

    // 记录首次注入的实例地址
    auto* sched = adapter.warp_scheduler();
    auto* sb = adapter.scoreboard();
    auto* pipe = adapter.pipeline();
    auto* tc = adapter.tensor_core();

    // 重复注入: 幂等, 实例不被重建 (SM 侧 raw 指针不失效)
    adapter.inject_timing_modules();
    CHECK(adapter.warp_scheduler() == sched);
    CHECK(adapter.scoreboard() == sb);
    CHECK(adapter.pipeline() == pipe);
    CHECK(adapter.tensor_core() == tc);

    facade.shutdown();
}

TEST_CASE("cuda_core_adapter_tick_works_after_injection",
          "[cuda-core][mvp][injection]") {
    tlm::PtxEmuSubmoduleMVP facade;
    tlm::CudaCoreAdapterMVP adapter;
    adapter.init(facade);
    adapter.inject_timing_modules();

    // 注入 scoreboard/pipeline/tensor_core 后 exe_once 路径仍正常
    // (IDLE 态 SM: cycle 递增, 不触发 3-Step 注入, 不 crash)
    adapter.tick();
    adapter.tick();
    adapter.tick();
    CHECK(adapter.get_warp_state(nullptr).cycle_count == 3u);

    // warp_scheduler 调度接口可用 (round-robin, 独立于 SM)
    auto* sched = adapter.warp_scheduler();
    REQUIRE(sched != nullptr);
    sched->add_warp(0);
    sched->add_warp(1);
    auto first = sched->schedule_next();
    auto second = sched->schedule_next();
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(first.value() != second.value());  // round-robin 轮换

    facade.shutdown();
}
