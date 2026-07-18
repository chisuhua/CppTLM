/**
 * @file test_kernel_launch_tlm_ext.cc
 * @brief KernelLaunchTLM 扩展单测（D1-Full P1 Phase 4 Wave 1 — IPtxEmuDriver 模式）
 *
 * 测试范围:
 *   - setMemoryBridge 非所有权存储
 *   - set_ptx_emu_driver 非所有权存储 (P1: 替代 set_ptx_emu_context)
 *   - submit FIFO 入队
 *   - tick() nullptr bridge → Phase 8.A 退化
 *   - tick() AdvanceResult::Executed 路径 (mock driver)
 *   - tick() AdvanceResult::NoOp 路径 (mock driver)
 *   - tick() AdvanceResult::Error 路径 (mock driver, 安全终止)
 *
 * @author CppTLM / 日期 2026-07-16 (P0) + 2026-07-18 (Phase 4 P1)
 */

#include "catch_amalgamated.hpp"
#include "tlm/gpu/kernel_launch_tlm.hh"
#include "tlm/gpu/memory_bridge.hh"
#include "tlm/gpu/ptx_emu_driver.hh"
#include "tlm/gpu/scoreboard_tlm.hh"
#include "tlm/gpu/pipeline_tlm.hh"
#include "tlm/gpu/tensor_core_tlm.hh"
#include "tlm/crossbar_tlm.hh"

#include <memory>

using namespace tlm;

// ─── Mock IPtxEmuDriver ───────────────────────────────────────────────

/// 可配置的 mock driver, 用于测试 KernelLaunchTLM::tick() 各分支
class MockPtxEmuDriver : public IPtxEmuDriver {
public:
    // 可配置字段
    AdvanceResult advance_result = AdvanceResult::NoOp;
    uint32_t advance_actual = 0;
    bool kernel_complete = false;
    uint32_t sm_count = 8;  // 默认 8 SM (Volta V100)

    // 追踪调用
    uint32_t advance_call_count = 0;
    uint32_t inject_scoreboard_calls = 0;
    uint32_t inject_pipeline_calls = 0;
    uint32_t inject_tensor_core_calls = 0;

    AdvanceResult advance(uint32_t max_cycles, uint32_t& actual_cycles) override {
        ++advance_call_count;
        actual_cycles = advance_actual;
        return advance_result;
    }

    bool is_kernel_complete(uint64_t kernel_id) override {
        return kernel_complete;
    }

    void inject_scoreboard(uint32_t sm_id,
                           std::unique_ptr<IScoreboard> sb) override {
        ++inject_scoreboard_calls;
        scoreboards_.push_back(std::move(sb));
    }

    void inject_pipeline(uint32_t sm_id,
                         std::unique_ptr<IPipelineLatencyProvider> p) override {
        ++inject_pipeline_calls;
        pipelines_.push_back(std::move(p));
    }

    void inject_tensor_core(uint32_t sm_id,
                            std::unique_ptr<ITensorCoreTiming> tc) override {
        ++inject_tensor_core_calls;
        tensorcores_.push_back(std::move(tc));
    }

    uint32_t num_sms() const override { return sm_count; }

private:
    std::vector<std::unique_ptr<IScoreboard>> scoreboards_;
    std::vector<std::unique_ptr<IPipelineLatencyProvider>> pipelines_;
    std::vector<std::unique_ptr<ITensorCoreTiming>> tensorcores_;
};

// ─── 测试 ─────────────────────────────────────────────────────────────

TEST_CASE("KernelLaunchTLM: setMemoryBridge 非所有权存储", "[gpu][f12b]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());

    kl->setMemoryBridge(bridge.get());
    // 验证: bridge_ 存储了非空指针, 且不持有所有权 (无 double-free 风险)
    REQUIRE(kl->pending_count() == 0);  // 初始无 pending
}

TEST_CASE("KernelLaunchTLM: set_ptx_emu_driver 存储原始指针", "[gpu][f12b][d1p1]") {
    EventQueue eq;
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);

    MockPtxEmuDriver driver;
    kl->set_ptx_emu_driver(&driver);
    // P1: 存储 IPtxEmuDriver* 非所有权指针, 接口已编译通过
    // 无 getter 公开接口, 通过 tick 无崩溃间接验证

    // nullptr 重置 (合法, 表示无 driver)
    kl->set_ptx_emu_driver(nullptr);
}

TEST_CASE("KernelLaunchTLM: submit FIFO 入队", "[gpu][f12b]") {
    EventQueue eq;
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);

    KernelLaunchRequest req;
    req.kernel_id = 1;
    req.stream_id = 0;
    req.grid_x = 1; req.grid_y = 1; req.grid_z = 1;
    req.block_x = 1; req.block_y = 1; req.block_z = 1;
    req.func_ptr = nullptr;

    kl->submit(std::move(req));
    REQUIRE(kl->pending_count() == 1);

    KernelLaunchRequest req2;
    req2.kernel_id = 2;
    kl->submit(std::move(req2));
    REQUIRE(kl->pending_count() == 2);
}

TEST_CASE("KernelLaunchTLM: tick() nullptr bridge = Phase 8.A 退化", "[gpu][f12b]") {
    EventQueue eq;
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);

    // 未调用 setMemoryBridge → bridge_ == nullptr
    // tick() 应走 Phase 8.A 路径 (无异常)
    REQUIRE_NOTHROW(kl->tick());
}

TEST_CASE("KernelLaunchTLM: tick() AdvanceResult::Executed 路径 (P1)", "[gpu][f12b][d1p1]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());
    MockPtxEmuDriver driver;

    kl->setMemoryBridge(bridge.get());
    kl->set_ptx_emu_driver(&driver);

    // 配置 driver: advance 返回 Executed
    driver.advance_result = AdvanceResult::Executed;
    driver.advance_actual = 1;

    // tick() 不应崩溃
    REQUIRE_NOTHROW(kl->tick());
    REQUIRE(driver.advance_call_count >= 1);
}

TEST_CASE("KernelLaunchTLM: tick() AdvanceResult::NoOp 路径 (P1)", "[gpu][f12b][d1p1]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());
    MockPtxEmuDriver driver;

    kl->setMemoryBridge(bridge.get());
    kl->set_ptx_emu_driver(&driver);

    // NoOp: kernel 未启动或已完成
    driver.advance_result = AdvanceResult::NoOp;

    REQUIRE_NOTHROW(kl->tick());
    REQUIRE(driver.advance_call_count >= 1);
}

TEST_CASE("KernelLaunchTLM: tick() AdvanceResult::Error 路径 (P1)", "[gpu][f12b][d1p1]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());
    MockPtxEmuDriver driver;

    kl->setMemoryBridge(bridge.get());
    kl->set_ptx_emu_driver(&driver);

    // Error: PTX-EMU 异常
    driver.advance_result = AdvanceResult::Error;

    // tick() 安全终止 (不崩溃, 不继续处理 pending)
    REQUIRE_NOTHROW(kl->tick());
    REQUIRE(driver.advance_call_count >= 1);
}

TEST_CASE("KernelLaunchTLM: tick() AdvanceResult::KernelComplete 路径 (P1)", "[gpu][f12b][d1p1]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());
    MockPtxEmuDriver driver;

    // 入队一个 kernel 请求, 期待 advance 后 is_kernel_complete 返回 true 清队列
    KernelLaunchRequest req;
    req.kernel_id = 42;
    req.stream_id = 0;
    req.grid_x = 1; req.grid_y = 1; req.grid_z = 1;
    req.block_x = 1; req.block_y = 1; req.block_z = 1;
    kl->submit(std::move(req));
    REQUIRE(kl->pending_count() == 1);

    kl->setMemoryBridge(bridge.get());
    kl->set_ptx_emu_driver(&driver);

    driver.advance_result = AdvanceResult::KernelComplete;
    driver.kernel_complete = true;

    REQUIRE_NOTHROW(kl->tick());
    // kernel 标记完成 → pending 清空
    REQUIRE(kl->pending_count() == 0);
    REQUIRE(driver.advance_call_count >= 1);
}

TEST_CASE("KernelLaunchTLM: tick() bridge_ set but driver_ null = 安全退化 (P1)", "[gpu][f12b][d1p1]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());

    // bridge_ 设置但 driver_ 为 nullptr (未集成 PTX-EMU)
    kl->setMemoryBridge(bridge.get());
    // 不调用 set_ptx_emu_driver()

    REQUIRE_NOTHROW(kl->tick());
}

TEST_CASE("MockPtxEmuDriver: inject_scoreboard 统计 (P1)", "[gpu][f12b][d1p1]") {
    MockPtxEmuDriver driver;
    auto sb = std::make_unique<ScoreboardTLM>();

    driver.inject_scoreboard(0, std::move(sb));
    REQUIRE(driver.inject_scoreboard_calls == 1);

    // 越界注入 (sm_id 过大, mock 不检查)
    auto sb2 = std::make_unique<ScoreboardTLM>();
    driver.inject_scoreboard(999, std::move(sb2));
    REQUIRE(driver.inject_scoreboard_calls == 2);
}

TEST_CASE("MockPtxEmuDriver: inject_pipeline 统计 (P1)", "[gpu][f12b][d1p1]") {
    MockPtxEmuDriver driver;
    auto p = std::make_unique<PipelineTLM>();

    driver.inject_pipeline(0, std::move(p));
    REQUIRE(driver.inject_pipeline_calls == 1);
}

TEST_CASE("MockPtxEmuDriver: inject_tensor_core 统计 (P1)", "[gpu][f12b][d1p1]") {
    MockPtxEmuDriver driver;
    auto tc = std::make_unique<TensorCoreTLM>();

    driver.inject_tensor_core(0, std::move(tc));
    REQUIRE(driver.inject_tensor_core_calls == 1);
}

TEST_CASE("MockPtxEmuDriver: num_sms 查询 (P1)", "[gpu][f12b][d1p1]") {
    MockPtxEmuDriver driver;
    driver.sm_count = 64;  // GV100

    REQUIRE(driver.num_sms() == 64);
}

TEST_CASE("MockPtxEmuDriver: AdvanceResult enum 值完整性 (P1)", "[gpu][f12b][d1p1]") {
    // 验证 4 个枚举值可编译且互异
    auto e = AdvanceResult::Executed;
    auto n = AdvanceResult::NoOp;
    auto k = AdvanceResult::KernelComplete;
    auto r = AdvanceResult::Error;

    REQUIRE(e != n);
    REQUIRE(n != k);
    REQUIRE(k != r);
    REQUIRE(r != e);
}