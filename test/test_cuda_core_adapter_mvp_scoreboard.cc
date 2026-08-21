// test/test_cuda_core_adapter_mvp_scoreboard.cc
// =============================================================================
// CudaCoreAdapterMVP — ScoreboardTLM RAW hazard 检测单元测试 (S1 / T-s1-4 §6.b)
// 功能: 验证注入后的 scoreboard 实例的 allocate/release 语义 —
//       同 (reg, warp) 重复 allocate 返回 false (= RAW/WAW hazard 检出,
//       匹配 PTX-EMU sm_context.cpp rollback 逻辑), release 后可再分配,
//       跨 warp 同 reg 不冲突。
// 约束: 仅用 facade API + adapter API + CppTLM timing 模块头 —
//       不 include 任何 PTX-EMU 头。
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-4, Sisyphus)
// 标签 [cuda-core][mvp][scoreboard]
// =============================================================================

#include "catch_amalgamated.hpp"

#include "tlm/gpu/cuda_core_adapter_mvp.hh"
#include "tlm/gpu/ptx_emu_submodule_mvp.hh"
#include "tlm/gpu/scoreboard_tlm.hh"

TEST_CASE("cuda_core_adapter_scoreboard_raw_hazard_detection",
          "[cuda-core][mvp][scoreboard]") {
    tlm::PtxEmuSubmoduleMVP facade;
    tlm::CudaCoreAdapterMVP adapter;
    adapter.init(facade);
    adapter.inject_timing_modules();

    tlm::ScoreboardTLM* sb = adapter.scoreboard();
    REQUIRE(sb != nullptr);
    REQUIRE(sb->has_free_entry());

    // 首次分配 r1@warp0 成功
    CHECK(sb->allocate(/*reg_id=*/1, /*warp_id=*/0));

    // RAW/WAW hazard: 同 reg 同 warp 的 in-flight 重复分配被拒绝
    CHECK_FALSE(sb->allocate(/*reg_id=*/1, /*warp_id=*/0));

    // 跨 warp 同 reg 不冲突 (全局表按 (warp, reg) 二元组索引)
    CHECK(sb->allocate(/*reg_id=*/1, /*warp_id=*/1));

    // 同 warp 不同 reg 不冲突
    CHECK(sb->allocate(/*reg_id=*/2, /*warp_id=*/0));

    // release r1@warp0 后可再分配 (hazard 解除)
    CHECK(sb->release(/*reg_id=*/1, /*warp_id=*/0));
    CHECK(sb->allocate(/*reg_id=*/1, /*warp_id=*/0));

    // release 不存在的 entry 返回 false (防御性语义)
    CHECK_FALSE(sb->release(/*reg_id=*/99, /*warp_id=*/7));

    facade.shutdown();
}

TEST_CASE("cuda_core_adapter_scoreboard_reset_clears_entries",
          "[cuda-core][mvp][scoreboard]") {
    tlm::PtxEmuSubmoduleMVP facade;
    tlm::CudaCoreAdapterMVP adapter;
    adapter.init(facade);
    adapter.inject_timing_modules();

    tlm::ScoreboardTLM* sb = adapter.scoreboard();
    REQUIRE(sb != nullptr);

    CHECK(sb->allocate(/*reg_id=*/5, /*warp_id=*/3));
    CHECK_FALSE(sb->allocate(/*reg_id=*/5, /*warp_id=*/3));

    // 跨 kernel reset → 所有 hazard entry 清空
    sb->reset();
    CHECK(sb->allocate(/*reg_id=*/5, /*warp_id=*/3));

    facade.shutdown();
}
