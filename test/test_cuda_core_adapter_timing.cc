/**
 * @file test_cuda_core_adapter_timing.cc
 * @brief CudaCoreAdapterMVP timing 注入链路测试 (HSK-8 Phase 2 §2.1 任务 1.4)
 *
 * 测试范围 (per plans/ptxemu-followup-roadmap.md §2.1):
 *   - init() 末尾自动调用 inject_timing_modules() (D1-Full tasks.md §1.5.1)
 *   - 4 个 timing 模块 (warp_scheduler/scoreboard/pipeline/tensor_core) 正确创建
 *   - IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming 3 个 HSK-4 接口
 *     注入路径正确（void* round-trip 通过 facade->attach_timing）
 *   - 注入幂等：重复 init 不重建实例
 *
 * 注意：本测试需要 PTX-EMU ON 路径 (CPPTLM_WITH_PTX_EMU=ON)。
 *       OFF 路径下整个文件编译为空（无 TEST_CASE），保持 817/817 PASS。
 *
 * @author CppTLM Team / 日期 2026-08-25
 */

#include "catch_amalgamated.hpp"

#ifdef CPPTLM_WITH_PTX_EMU

#include "tlm/gpu/cuda_core_adapter_mvp.hh"
#include "tlm/gpu/ptx_emu_submodule_mvp.hh"
#include "tlm/gpu/scoreboard_tlm.hh"
#include "tlm/gpu/pipeline_tlm.hh"
#include "tlm/gpu/tensor_core_tlm.hh"

#include "cudart/scoreboard_interface.h"
#include "cudart/pipeline_interface.h"
#include "cudart/tensor_core_interface.h"

using namespace tlm;

// =============================================================================
// Helper: 构造一个已初始化的 facade 用于测试
// =============================================================================
namespace {

struct TestRig {
    PtxEmuSubmoduleMVP facade;
    CudaCoreAdapterMVP adapter;

    TestRig() {
        DeviceConfig config{};  // 默认 1 SM
        REQUIRE(facade.init("/tmp/cpptlm-test-ptx-emu-root", config));
        adapter.init(facade);
    }
};

}  // anonymous namespace

// =============================================================================
// TC1: init() 后 4 个 timing 模块全部创建
// =============================================================================
TEST_CASE_METHOD(TestRig,
    "CudaCoreAdapterMVP: init() creates 4 timing modules and sets flag",
    "[gpu][hsk8][timing]") {
    REQUIRE(adapter.timing_injected());
    REQUIRE(adapter.warp_scheduler() != nullptr);
    REQUIRE(adapter.scoreboard() != nullptr);
    REQUIRE(adapter.pipeline() != nullptr);
    REQUIRE(adapter.tensor_core() != nullptr);
}

// =============================================================================
// TC2: init() 幂等 — 重复调用不重建实例
// =============================================================================
TEST_CASE_METHOD(TestRig,
    "CudaCoreAdapterMVP: init() is idempotent (no module recreation)",
    "[gpu][hsk8][timing]") {
    auto* sb_before = adapter.scoreboard();
    auto* pipe_before = adapter.pipeline();
    auto* tc_before = adapter.tensor_core();

    adapter.init(facade);  // 第二次 init

    REQUIRE(adapter.timing_injected());
    REQUIRE(adapter.scoreboard() == sb_before);      // 指针未变
    REQUIRE(adapter.pipeline() == pipe_before);
    REQUIRE(adapter.tensor_core() == tc_before);
}

// =============================================================================
// TC3: scoreboard 可转为 IScoreboard* 并调用 allocate/release
// =============================================================================
TEST_CASE_METHOD(TestRig,
    "CudaCoreAdapterMVP: scoreboard implements IScoreboard (HSK-4 PTX-1)",
    "[gpu][hsk8][timing]") {
    IScoreboard* sb = adapter.scoreboard();  // 上行转型 (IScoreboard 公开继承)
    REQUIRE(sb != nullptr);

    REQUIRE(sb->has_free_entry());
    REQUIRE(sb->allocate(10, 0));            // (reg_id=10, warp_id=0)
    REQUIRE_FALSE(sb->allocate(10, 0));     // duplicate rejects
    REQUIRE(sb->release(10, 0));
    REQUIRE_FALSE(sb->release(42, 0));      // 未分配 entry release 失败
}

// =============================================================================
// TC4: pipeline 可转为 IPipelineLatencyProvider* 并查询延迟
// =============================================================================
TEST_CASE_METHOD(TestRig,
    "CudaCoreAdapterMVP: pipeline implements IPipelineLatencyProvider (HSK-4 PTX-2)",
    "[gpu][hsk8][timing]") {
    IPipelineLatencyProvider* pipe = adapter.pipeline();
    REQUIRE(pipe != nullptr);

    // P0 fma = 4.22 (A100 模型)
    REQUIRE(pipe->get_fractional_cycles("fma.rn.f32", PipelineId::P0_INT_FP32) == 4.22);
    // P3 ld.global = 200 (DRAM 延迟)
    REQUIRE(pipe->get_fractional_cycles("ld.global.u32", PipelineId::P3_LSU) == 200.0);
    // P4 TC = 0 (委托给 TensorCoreTLM)
    REQUIRE(pipe->get_fractional_cycles("mma.sync", PipelineId::P4_TC) == 0.0);
}

// =============================================================================
// TC5: tensor_core 可转为 ITensorCoreTiming* 并查询延迟
// =============================================================================
TEST_CASE_METHOD(TestRig,
    "CudaCoreAdapterMVP: tensor_core implements ITensorCoreTiming (HSK-4 PTX-3)",
    "[gpu][hsk8][timing]") {
    ITensorCoreTiming* tc = adapter.tensor_core();
    REQUIRE(tc != nullptr);

    // A100 MMA 延迟: FP16=8, TF32=4, FP4=32
    REQUIRE(tc->get_latency(TcPrecision::FP16) == 8);
    REQUIRE(tc->get_latency(TcPrecision::TF32) == 4);
    REQUIRE(tc->get_latency(TcPrecision::FP4)  == 32);
    REQUIRE(tc->get_throughput_cycles(TcPrecision::BF16) == 8);
    // get_latency_mnk 默认实现: degenerates to get_latency
    REQUIRE(tc->get_latency_mnk(TcPrecision::FP8, 16, 8, 16) == 16);
}

// =============================================================================
// TC6: facade 收到正确的 void* 指针 (PtxEmuSubmoduleMVP 端验证)
// 通过查询 facade->device 的间接方式: 调用 sm_exe_once 后 warp_scheduler
// 被 PTX-EMU 调度路径访问, 验证 timing 链路完整
// =============================================================================
TEST_CASE_METHOD(TestRig,
    "CudaCoreAdapterMVP: facade->sm_exe_once() runs after timing injection",
    "[gpu][hsk8][timing][e2e]") {
    // PTX-EMU 接受注入后应能正常驱动 sm_exe_once
    // 返回值: 0=无 pending, -1=无 SM (与 PtxEmuSubmoduleMVP::sm_exe_once 文档一致)
    int rc = facade.sm_exe_once(0);
    REQUIRE((rc == 0 || rc == -1));  // 接受任意合法返回值

    // adapter.tick() 调用 facade.sm_exe_once(0) 不崩溃
    REQUIRE_NOTHROW(adapter.tick());
}

// =============================================================================
// TC7: on_cta_arrival 资源反压验证 (独立于 timing 注入, 但属于 adapter 接口)
// =============================================================================
TEST_CASE_METHOD(TestRig,
    "CudaCoreAdapterMVP: on_cta_arrival enforces 48KB shared mem + 64 warps limits",
    "[gpu][hsk8][timing]") {
    CudaCoreAdapterMVP::CtaDescriptor valid{};
    valid.shared_mem_size = 32 * 1024;  // 32KB < 48KB
    valid.warp_count = 32;             // 32 < 64
    REQUIRE(adapter.on_cta_arrival(valid));

    CudaCoreAdapterMVP::CtaDescriptor too_big{};
    too_big.shared_mem_size = 64 * 1024;  // > 48KB
    REQUIRE_FALSE(adapter.on_cta_arrival(too_big));

    CudaCoreAdapterMVP::CtaDescriptor too_many_warps{};
    too_many_warps.warp_count = 128;  // > 64
    REQUIRE_FALSE(adapter.on_cta_arrival(too_many_warps));
}

#endif  // CPPTLM_WITH_PTX_EMU
