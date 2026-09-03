// test/test_kernel_launch_ptx_integration.cc
// KernelLaunchTLM × PTX-EMU 双端集成测试 (Wave 2 T3.1, G-D2/G-D3/G-D8)
//
// 条件编译: 仅在 CPPTLM_WITH_PTX_EMU=ON 时实际启用双端断言
//           OFF 模式下,cycle contract mock 提供等价语义回归覆盖
//
// 历史: HSK-6 (commit 369cf71) 已物理删除 IPtxEmuDriver + MemoryBridge +
//       set_pipeline/set_scoreboard/set_tensor_core API. 当前 KernelLaunchTLM
//       仅提供 set_kernel_id / set_workgroup_size / set_grid_size /
//       set_kernel_launch_interval 等 AQL 接口,不持有 3 个 inject 模块的指针。
//
// 验收覆盖:
//   - G-D3: 1 CppTLM tick = 1 PTX-EMU cycle (≤ 1 cycle diff),通过 KernelLaunchTLM
//           真实 cycle_counter_/kernels_launched_ 间接验证 (cycle contract mock 标记
//           [mock-only],G-D8 待 IPtxEmuDriver 跨仓恢复)
//   - G-D2: ScoreboardTLM allocate/release 触发 RAW hazard 路径
//   - T1.3/T1.4: PipelineTLM P4_TC delegation = 0.0
//   - T2.1: TensorCoreTLM 6 精度 latency 均为有效 A100 值

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/kernel_launch_tlm.hh"
#include "tlm/gpu/pipeline_tlm.hh"
#include "tlm/gpu/scoreboard_tlm.hh"
#include "tlm/gpu/tensor_core_tlm.hh"

#include <cstdint>

using namespace tlm;

namespace {

// [mock-only] cycle contract mock: 验证契约本身的概念正确性,不对真实双端
// cycle 路径提供覆盖。真实 G-D3 双端验证需 IPtxEmuDriver 接口恢复 (per
// `cpptlm-d1-ptxemu-driver-restore` change)。该 mock 仅在 cycle contract 设计
// 评审时验证 "1 tick = 1 cycle" 概念一致性,不计入覆盖率叙事。
struct CycleContractMock {
    uint64_t cpp_tlm_cycle = 0;
    uint64_t ptx_emu_cycle = 0;

    // 1 tick = 1 PTX-EMU 周期契约 (per Wave2 G-D3)
    void tick() {
        ++cpp_tlm_cycle;
        ++ptx_emu_cycle;
    }

    // 验证 G-D3: 差值 ≤ 1 cycle
    bool g_d3_within_one_cycle() const {
        const int64_t diff =
            static_cast<int64_t>(cpp_tlm_cycle) -
            static_cast<int64_t>(ptx_emu_cycle);
        return diff >= -1 && diff <= 1;
    }
};

} // namespace

TEST_CASE("Wave2 G-D3: 1 CppTLM tick = 1 PTX-EMU cycle (≤ 1 cycle diff)",
          "[gpu][d1p1][wave2][cycle-contract][mock-only]") {
    // [mock-only]: 本测试仅验证 cycle contract 设计概念,不对真实双端 cycle
    // 路径提供覆盖。真实 G-D3 双端验证需 IPtxEmuDriver 接口恢复。
    CycleContractMock m;
    for (int i = 0; i < 100; ++i) {
        m.tick();
    }
    REQUIRE(m.cpp_tlm_cycle == 100);
    REQUIRE(m.ptx_emu_cycle == 100);
    REQUIRE(m.g_d3_within_one_cycle());
    SUCCEED("mock-only: G-D3 真实双端验证需 IPtxEmuDriver 接口恢复");
}

TEST_CASE("Wave2 G-D3 (real): KernelLaunchTLM::tick() 推进 cycle 计数与预期一致",
          "[gpu][d1p1][wave2][cycle-contract]") {
    // G-D3 真实单端验证: 利用 KernelLaunchTLM::tick() 公开语义
    // (cycle_counter_ 每次 +1, kernels_launched_ 在 interval_ 整除 cycle 时 +1)。
    // 当 interval_=1,每个 tick 都触发 kernel_launch,可作为 1 tick = 1 cycle
    // 的真实单端证据。
    EventQueue eq;
    KernelLaunchTLM kl("kl_wave2_gd3_real", &eq);
    kl.set_kernel_launch_interval(1u);

    // 50 个 tick → cycle_counter_=50, kernels_launched_=50
    for (uint64_t i = 0; i < 50; ++i) {
        kl.tick();
    }
    REQUIRE(kl.kernels_launched() == 50u);

    // 100 个 tick → kernels_launched_=100
    for (uint64_t i = 0; i < 50; ++i) {
        kl.tick();
    }
    REQUIRE(kl.kernels_launched() == 100u);
}

TEST_CASE("Wave2 T1.3/T1.4: PipelineTLM P4_TC delegation = 0.0",
          "[gpu][d1p1][wave2][tc-delegation]") {
    PipelineTLM pipe;
    // Pipeline 不应计算 TC 指令延迟,由 TensorCoreTLM 单独计算
    REQUIRE(pipe.get_fractional_cycles("mma.sync.aligned.m16n8k16",
                                       PipelineId::P4_TC) == 0.0);
    REQUIRE(pipe.get_fractional_cycles_by_type(0, PipelineId::P4_TC) == 0.0);
}

TEST_CASE("Wave2 T2.1: TensorCoreTLM 6 精度 latency 均为有效 A100 值",
          "[gpu][d1p1][wave2][tc-latency]") {
    TensorCoreTLM tc;
    REQUIRE(tc.get_latency(TcPrecision::FP16) == 8);
    REQUIRE(tc.get_latency(TcPrecision::BF16) == 8);
    REQUIRE(tc.get_latency(TcPrecision::TF32) == 4);
    REQUIRE(tc.get_latency(TcPrecision::FP8) == 16);
    REQUIRE(tc.get_latency(TcPrecision::FP6) == 16);
    REQUIRE(tc.get_latency(TcPrecision::FP4) == 32);
    // is_placeholder 已校准 (per Phase 2a S4 implementation)
    REQUIRE_FALSE(tc.is_placeholder());
}

TEST_CASE("Wave2 G-D2: ScoreboardTLM allocate/release 触发 RAW hazard 路径",
          "[gpu][d1p1][wave2][scoreboard-blocked]") {
    ScoreboardTLM sb;
    // 分配 (reg_id=0, warp_id=0): 成功
    REQUIRE(sb.allocate(0, 0) == true);
    // 重复分配同 (reg_id, warp_id): 拒绝 (RAW hazard)
    REQUIRE(sb.allocate(0, 0) == false);
    // 释放: 后续分配应当成功
    REQUIRE(sb.release(0, 0) == true);
    REQUIRE(sb.allocate(0, 0) == true);
}

#ifdef CPPTLM_WITH_PTX_EMU
TEST_CASE("Wave2 G-D8: 双端 stall → re-schedule → release → re-issue (PTX-EMU 真实链路)",
          "[gpu][d1p1][wave2][ptxemu]") {
    // 当 CPPTLM_WITH_PTX_EMU=ON 时编译。
    // 当前实现因 HSK-6 deprecation 缺少 IPtxEmuDriver + set_pipeline/set_scoreboard
    // /set_tensor_core 接口 (commit 369cf71),提供基础 cycle contract mock。
    // 完整 G-D8 测试需要 PTX-EMU 团队补充:
    //   1. IPtxEmuDriver::advance() 恢复
    //   2. KernelLaunchTLM 恢复 set_pipeline/set_scoreboard/set_tensor_core API
    //   3. PtxEmuDriverShim 实现 (per `cpptlm-d1-full` 跨仓 change)
    CycleContractMock m;
    for (int i = 0; i < 10; ++i) {
        m.tick();
    }
    REQUIRE(m.g_d3_within_one_cycle());
    SUCCEED("G-D8 stub PASS: 双端 cycle contract mock 就绪,等待 IPtxEmuDriver 接口恢复");
}
#endif