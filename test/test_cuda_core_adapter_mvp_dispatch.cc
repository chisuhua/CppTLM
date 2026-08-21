// test/test_cuda_core_adapter_mvp_dispatch.cc
// =============================================================================
// CudaCoreAdapterMVP — on_cta_arrival 资源反压单元测试 (S1 / T-s1-4 §6.d)
// 功能: 验证 on_cta_arrival() 经 sm->reserve_resources() 的 SM 资源反压:
//       合理 shared_mem/warp 请求被接受, 超限请求被拒绝 (返回 false)。
// 约束: 仅用 facade API + adapter API — 不 include 任何 PTX-EMU 头。
// 注: shared_mem 阈值断言对 ResourceManager 初始化尺寸 (8KB/64KB) 均鲁棒 —
//     1KB 恒定可接受, 1MB 恒定拒绝 (默认 64KB/SM 上限)。
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-4, Sisyphus)
// 标签 [cuda-core][mvp][dispatch]
// =============================================================================

#include "catch_amalgamated.hpp"

#include "tlm/gpu/cuda_core_adapter_mvp.hh"
#include "tlm/gpu/ptx_emu_submodule_mvp.hh"

TEST_CASE("cuda_core_adapter_dispatch_backpressure",
          "[cuda-core][mvp][dispatch]") {
    tlm::PtxEmuSubmoduleMVP facade;
    tlm::CudaCoreAdapterMVP adapter;
    adapter.init(facade);

    // 合理请求: 1KB shared + 4 warps → 接受
    tlm::CudaCoreAdapterMVP::CtaDescriptor small;
    small.shared_mem_size = 1024;
    small.warp_count = 4;
    small.task_id = 1;
    CHECK(adapter.on_cta_arrival(small));

    // 零 shared_mem 请求 (纯计算 CTA) → 接受
    tlm::CudaCoreAdapterMVP::CtaDescriptor no_smem;
    no_smem.shared_mem_size = 0;
    no_smem.warp_count = 8;
    no_smem.task_id = 2;
    CHECK(adapter.on_cta_arrival(no_smem));

    // 超限 shared_mem: 1MB >> 64KB/SM 上限 → 拒绝
    tlm::CudaCoreAdapterMVP::CtaDescriptor huge_smem;
    huge_smem.shared_mem_size = 1u << 20;
    huge_smem.warp_count = 1;
    huge_smem.task_id = 3;
    CHECK_FALSE(adapter.on_cta_arrival(huge_smem));

    // 超限 warp 数: 100 > 64 warps/SM 上限 → 拒绝
    tlm::CudaCoreAdapterMVP::CtaDescriptor too_many_warps;
    too_many_warps.shared_mem_size = 0;
    too_many_warps.warp_count = 100;
    too_many_warps.task_id = 4;
    CHECK_FALSE(adapter.on_cta_arrival(too_many_warps));

    // 边界: 恰好 64 warps → 接受
    tlm::CudaCoreAdapterMVP::CtaDescriptor max_warps;
    max_warps.shared_mem_size = 0;
    max_warps.warp_count = 64;
    max_warps.task_id = 5;
    CHECK(adapter.on_cta_arrival(max_warps));

    facade.shutdown();
}

TEST_CASE("cuda_core_adapter_dispatch_uninitialized_rejects",
          "[cuda-core][mvp][dispatch]") {
    // 未 init 的 adapter: on_cta_arrival 必须安全拒绝
    tlm::CudaCoreAdapterMVP adapter;
    tlm::CudaCoreAdapterMVP::CtaDescriptor cta;
    cta.shared_mem_size = 0;
    cta.warp_count = 1;
    CHECK_FALSE(adapter.on_cta_arrival(cta));
}

TEST_CASE("cuda_core_adapter_on_warp_complete_records_status",
          "[cuda-core][mvp][dispatch]") {
    tlm::PtxEmuSubmoduleMVP facade;
    tlm::CudaCoreAdapterMVP adapter;
    adapter.init(facade);

    CHECK(adapter.completed_warp_count() == 0u);

    adapter.on_warp_complete(/*task_id=*/42, /*status=*/0);
    CHECK(adapter.completed_warp_count() == 1u);
    CHECK(adapter.last_completion_status() == 0);

    adapter.on_warp_complete(/*task_id=*/43, /*status=*/-1);
    CHECK(adapter.completed_warp_count() == 2u);
    CHECK(adapter.last_completion_status() == -1);

    facade.shutdown();
}
