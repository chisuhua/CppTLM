// test/test_cuda_core_adapter_mvp_tick.cc
// =============================================================================
// CudaCoreAdapterMVP — tick() cycle 推进单元测试 (S1 / T-s1-4 §6.a)
// 功能: 验证 tick() 经 facade 驱动 sm->exe_once() 推进 SM cycle_count,
//       且 get_warp_state(nullptr) 镜像出当前 cycle。
// 约束: 仅用 facade API + adapter API — 不 include 任何 PTX-EMU 头,
//       不直接构造 WarpContext/ThreadContext/GPUContext。
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-4, Sisyphus)
// 标签 [cuda-core][mvp][tick]
// =============================================================================

#include "catch_amalgamated.hpp"

#include "tlm/gpu/cuda_core_adapter_mvp.hh"
#include "tlm/gpu/ptx_emu_submodule_mvp.hh"

TEST_CASE("cuda_core_adapter_tick_advances_cycle_count",
          "[cuda-core][mvp][tick]") {
    tlm::PtxEmuSubmoduleMVP facade;
    tlm::CudaCoreAdapterMVP adapter;
    adapter.init(facade);

    // 初始 cycle = 0
    CHECK(adapter.get_warp_state(nullptr).cycle_count == 0u);

    // 单次 tick → cycle +1 (exe_once 在 IDLE 态也递增 cycle_counter_)
    adapter.tick();
    CHECK(adapter.get_warp_state(nullptr).cycle_count == 1u);

    // 连续 5 次 tick → 累计 6
    for (int i = 0; i < 5; ++i) {
        adapter.tick();
    }
    CHECK(adapter.get_warp_state(nullptr).cycle_count == 6u);

    facade.shutdown();
}

TEST_CASE("cuda_core_adapter_tick_uninitialized_is_noop",
          "[cuda-core][mvp][tick]") {
    // 未 init 的 adapter: tick() 必须安全 no-op, 不 crash
    tlm::CudaCoreAdapterMVP adapter;
    adapter.tick();
    adapter.tick();
    CHECK(adapter.get_warp_state(nullptr).cycle_count == 0u);
}
