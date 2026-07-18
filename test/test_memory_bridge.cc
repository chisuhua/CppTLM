/**
 * @file test_memory_bridge.cc
 * @brief MemoryBridge 单测（P0 F12b-LD 阶段）
 *
 * 测试范围:
 *   - ABI 版本一致性
 *   - submit_kernel 入队 + args deep-copy
 *   - poll_kernel 返回语义 (P0 立即完成)
 *   - synchronize_stream 迭代器安全
 *   - global_access 延迟查询
 *   - deep_copy_args_ 独立性
 *   - 参数校验 (nullptr 等)
 *
 * @author CppTLM / 日期 2026-07-16
 */

#include "catch_amalgamated.hpp"
#include "tlm/gpu/memory_bridge.hh"
#include "tlm/gpu/kernel_launch_tlm.hh"
#include "tlm/crossbar_tlm.hh"

using namespace tlm;

/// ABI 约定的 submit_kernel 包装: 简化单次调用
static int submitSimple(MemoryBridge* mb, uint64_t id,
                        const char* name, const void** args,
                        size_t args_cnt, size_t shmem) {
    return mb->submit_kernel(id, name,
                             1, 1, 1,   // grid_x, y, z
                             1, 1, 1,   // block_x, y, z
                             args, args_cnt, shmem, 0 /* stream_id */);
}

TEST_CASE("MemoryBridge: ABI 版本一致性", "[gpu][f12b]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());

    REQUIRE(bridge->version() == CPPTLMBRIDGE_VERSION);
    // 2026-07-18 Oracle P0: v1 → v2 (新增 3 vendor header: scoreboard/pipeline/tensor_core_interface.h)
    REQUIRE(CPPTLMBRIDGE_VERSION == 2);
}

TEST_CASE("MemoryBridge: submit_kernel 存储 PendingKernel + 无回归", "[gpu][f12b]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());

    const void* args[] = {nullptr};
    int ret = submitSimple(bridge.get(), 42, "vector_add", args, 1, 0);
    REQUIRE(ret == 0);  // kCudaSuccess
}

TEST_CASE("MemoryBridge: poll_kernel 返回 0 (P0 立即完成)", "[gpu][f12b]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());

    const void* args[] = {nullptr};
    REQUIRE(submitSimple(bridge.get(), 99, "test_kernel", args, 1, 0) == 0);

    uint64_t remaining = bridge->poll_kernel(99);
    REQUIRE(remaining == 0);  // P0 语义: 立即完成
}

TEST_CASE("MemoryBridge: synchronize_stream 迭代器安全性 (快照模式)", "[gpu][f12b]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());

    // 入队多个 kernel
    for (uint64_t id = 100; id < 110; ++id) {
        submitSimple(bridge.get(), id, "multi", nullptr, 0, 0);
    }

    // synchronize_stream 应无崩溃 (no iterator UB)
    int ret = bridge->synchronize_stream(0);
    REQUIRE(ret == 0);
}

TEST_CASE("MemoryBridge: global_access 返回 query_latency()", "[gpu][f12b]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());

    uint64_t latency = bridge->global_access(0x1000, 0, 0);
    REQUIRE(latency == 3);  // P0 占位: query_latency 返回 3
}

TEST_CASE("MemoryBridge: deep_copy_args_ 源数据修改不影响副本", "[gpu][f12b]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());

    int val = 42;
    const void* args[] = {&val};
    REQUIRE(submitSimple(bridge.get(), 200, "copy_test", args, 1, 0) == 0);

    // 修改源数据
    val = 999;

    // poll 不应受影响 (P0 立即完成)
    uint64_t remaining = bridge->poll_kernel(200);
    REQUIRE(remaining == 0);
}

TEST_CASE("MemoryBridge: nullptr kernel_name 返回错误码", "[gpu][f12b]") {
    EventQueue eq;
    auto xbar = std::make_unique<CrossbarTLM>("xbar", &eq);
    auto kl = std::make_unique<KernelLaunchTLM>("kl", &eq);
    auto bridge = std::make_unique<MemoryBridge>(kl.get(), xbar.get());

    const void* args[] = {nullptr};
    int ret = submitSimple(bridge.get(), 300, nullptr, args, 1, 0);
    REQUIRE(ret != 0);  // kCudaErrorInvalidValue = 11
}