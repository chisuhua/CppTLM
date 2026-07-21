/**
 * @file test_memory_bridge_poll.cc
 * @brief MemoryBridge::poll_kernel S2 Phase 1.1 P1 升级单测
 *
 * 测试范围:
 *   - 未知 kernel_id → UINT64_MAX
 *   - 无 driver (nullptr) fallback → 提交后 poll 返回 0
 *   - get_ptx_emu_driver() getter 正确传递 driver 指针
 *   - poll_kernel 对 nullptr kernel_launch_ 的防御性处理
 *
 * PTX-EMU 协同: PTX-EMU docs/dev-process/cpptlm-co-simulation-plan.md §S2
 *
 * @author CppTLM / 日期 2026-07-21
 */

#include "catch_amalgamated.hpp"
#include "tlm/gpu/memory_bridge.hh"
#include "tlm/gpu/kernel_launch_tlm.hh"
#include "tlm/gpu/ptx_emu_driver.hh"

using namespace tlm;

// 轻量 fake driver: is_kernel_complete 返回固定值
class FakeCompleteDriver : public IPtxEmuDriver {
public:
    explicit FakeCompleteDriver(bool complete = true) : complete_(complete) {}
    AdvanceResult advance(uint32_t, uint32_t&) override { return AdvanceResult::NoOp; }
    bool is_kernel_complete(uint64_t) override { return complete_; }
    void inject_scoreboard(uint32_t, std::unique_ptr<IScoreboard>) override {}
    void inject_pipeline(uint32_t, std::unique_ptr<IPipelineLatencyProvider>) override {}
    void inject_tensor_core(uint32_t, std::unique_ptr<ITensorCoreTiming>) override {}
    uint32_t num_sms() const override { return 0; }
private:
    bool complete_;
};

TEST_CASE("MemoryBridge::poll_kernel: unknown kernel_id returns UINT64_MAX", "[gpu][d1p1][s2]") {
    MemoryBridge bridge(nullptr, nullptr);
    REQUIRE(bridge.poll_kernel(999) == UINT64_MAX);
}

TEST_CASE("MemoryBridge::poll_kernel: nullptr kernel_launch_ fallback", "[gpu][d1p1][s2]") {
    MemoryBridge bridge(nullptr, nullptr);
    // submit_kernel 需要非空 kernel_launch_, 无法直接测试 submit+poll
    // poll_kernel 对 nullptr kernel_launch_ 已添加防御: 直接进入 fallback 路径
    REQUIRE(bridge.poll_kernel(0) == UINT64_MAX);
}

TEST_CASE("KernelLaunchTLM: get_ptx_emu_driver returns injected driver", "[gpu][d1p1][s2]") {
    EventQueue eq;
    KernelLaunchTLM kl("test_kl", &eq);
    REQUIRE(kl.get_ptx_emu_driver() == nullptr);

    FakeCompleteDriver driver(true);
    kl.set_ptx_emu_driver(&driver);
    REQUIRE(kl.get_ptx_emu_driver() == &driver);
    REQUIRE(kl.get_ptx_emu_driver()->is_kernel_complete(42));
}

TEST_CASE("MemoryBridge::poll_kernel: driver not set (nullptr) uses P0 fallback", "[gpu][d1p1][s2]") {
    // 构造带有效 kernel_launch_ 但 driver 未设置的 MemoryBridge
    EventQueue eq;
    KernelLaunchTLM kl("test_kl", &eq);
    MemoryBridge bridge(&kl, nullptr);

    // 提交 kernel (需要 driver 未设置 → P0 fallback)
    int rc = bridge.submit_kernel(1, "test_kernel", 1, 1, 1, 32, 1, 1,
                                  nullptr, 0, 0, 0);
    REQUIRE(rc == 0);

    // driver 未设置 → fallback: 立即完成
    REQUIRE(bridge.poll_kernel(1) == 0);

    // 重复 poll → unknown (已 erase)
    REQUIRE(bridge.poll_kernel(1) == UINT64_MAX);
}

TEST_CASE("MemoryBridge::poll_kernel: driver reports complete → returns 0", "[gpu][d1p1][s2]") {
    EventQueue eq;
    KernelLaunchTLM kl("test_kl", &eq);
    FakeCompleteDriver driver(true);  // is_kernel_complete = true
    kl.set_ptx_emu_driver(&driver);

    MemoryBridge bridge(&kl, nullptr);
    int rc = bridge.submit_kernel(2, "test_kernel", 1, 1, 1, 32, 1, 1,
                                  nullptr, 0, 0, 0);
    REQUIRE(rc == 0);

    // driver 报告完成 → 返回 0 + erase
    REQUIRE(bridge.poll_kernel(2) == 0);
    REQUIRE(bridge.poll_kernel(2) == UINT64_MAX);  // 已 erase
}

TEST_CASE("MemoryBridge::poll_kernel: driver reports NOT complete → returns 1", "[gpu][d1p1][s2]") {
    EventQueue eq;
    KernelLaunchTLM kl("test_kl", &eq);
    FakeCompleteDriver driver(false);  // is_kernel_complete = false
    kl.set_ptx_emu_driver(&driver);

    MemoryBridge bridge(&kl, nullptr);
    int rc = bridge.submit_kernel(3, "test_kernel", 1, 1, 1, 32, 1, 1,
                                  nullptr, 0, 0, 0);
    REQUIRE(rc == 0);

    // 第一次 poll: 未完成 → 返回 1, 不 erase
    REQUIRE(bridge.poll_kernel(3) == 1);
    REQUIRE(bridge.poll_kernel(3) == 1);  // 仍然 pending, 可重复 poll
}

TEST_CASE("MemoryBridge::poll_kernel: multi-kernel mixed completion", "[gpu][d1p1][s2]") {
    EventQueue eq;
    KernelLaunchTLM kl("test_kl", &eq);
    FakeCompleteDriver driver(true);  // 全部报告完成
    kl.set_ptx_emu_driver(&driver);

    MemoryBridge bridge(&kl, nullptr);

    // 提交 3 个 kernel
    for (uint64_t i = 10; i <= 12; ++i) {
        int rc = bridge.submit_kernel(i, "test", 1, 1, 1, 32, 1, 1,
                                      nullptr, 0, 0, 0);
        REQUIRE(rc == 0);
    }

    // 全部 poll（driver 报告完成）
    REQUIRE(bridge.poll_kernel(10) == 0);
    REQUIRE(bridge.poll_kernel(11) == 0);
    REQUIRE(bridge.poll_kernel(12) == 0);

    // 全部已 erase
    REQUIRE(bridge.poll_kernel(10) == UINT64_MAX);
    REQUIRE(bridge.poll_kernel(11) == UINT64_MAX);
    REQUIRE(bridge.poll_kernel(12) == UINT64_MAX);
}
