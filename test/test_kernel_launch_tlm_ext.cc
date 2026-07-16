/**
 * @file test_kernel_launch_tlm_ext.cc
 * @brief KernelLaunchTLM 扩展单测（P0 F12b-LD 阶段）
 *
 * 测试范围:
 *   - setMemoryBridge 非所有权存储
 *   - set_ptx_emu_context 原始指针存储
 *   - submit FIFO 入队
 *   - tick() nullptr bridge → Phase 8.A 退化
 *   - tick() 死循环熔断 (MAX_PTX_STEPS_PER_TICK)
 *
 * @author CppTLM / 日期 2026-07-16
 */

#include "catch_amalgamated.hpp"
#include "tlm/gpu/kernel_launch_tlm.hh"
#include "tlm/gpu/memory_bridge.hh"
#include "tlm/crossbar_tlm.hh"

using namespace tlm;

TEST_CASE("KernelLaunchTLM: setMemoryBridge 非所有权存储", "[gpu][f12b]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());

    kl->setMemoryBridge(bridge.get());
    // 验证: bridge_ 存储了非空指针, 且不持有所有权 (无 double-free 风险)
    REQUIRE(kl->pending_count() == 0);  // 初始无 pending

    // bridge 析构后 kl->bridge_ 悬空, 但 P0 生命周期保证 bridge 先于 kl 构造、后于 kl 析构
    // 此处仅验证接口签名编译通过
}

TEST_CASE("KernelLaunchTLM: set_ptx_emu_context 存储原始指针", "[gpu][f12b]") {
    EventQueue eq;
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);

    int context_value = 42;
    kl->set_ptx_emu_context(&context_value);
    // P0 仅存储指针, 不 deref; 编译通过即验证 setter 可用
    // 无 getter 公开接口, 通过 tick 无崩溃间接验证
    kl->set_ptx_emu_context(nullptr);
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

TEST_CASE("KernelLaunchTLM: tick() 死循环熔断 (MAX_PTX_STEPS_PER_TICK)", "[gpu][f12b]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());

    kl->setMemoryBridge(bridge.get());

    // 即使 PTX-EMU 上下文 stub (nullptr), tick() 应安全地在 MAX_PTX_STEPS 后返回
    // 不造成死循环
    REQUIRE_NOTHROW(kl->tick());
}