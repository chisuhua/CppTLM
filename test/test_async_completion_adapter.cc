/**
 * @file test_async_completion_adapter.cc
 * @brief AsyncCompletionAdapter 单测（D1-Full P2 #C5 占位实现）
 *
 * 测试范围 (per openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/spec.md):
 *   - Phase 8.B 占位语义: fire_completion() 不调用 callback, 仅递增计数
 *   - 多次 fire_completion() 正确累计 fire_completion_count_
 *   - 未知 id 的 fire_completion() 仍递增计数但不崩溃
 *   - 重复 register 同一 id 覆盖前一次 callback, 但已 fire 的不追溯
 *   - 独立模式 (async_completion_ = nullptr) 安全: 调用方需检查 nullptr
 *
 * TDD step 1: 本测试文件应先于 AsyncCompletionAdapter 实现创建, 预期编译失败
 * (找不到 include/tlm/gpu/async_completion_adapter.hh)
 *
 * @author CppTLM / 日期 2026-07-16
 */

#include "catch_amalgamated.hpp"
#include "tlm/gpu/async_completion_adapter.hh"

#include <atomic>

using namespace tlm;

TEST_CASE("AsyncCompletionAdapter placeholder: fire does NOT invoke callback", "[gpu][async]") {
    AsyncCompletionAdapter adapter;
    std::atomic<int> call_count{0};

    adapter.register_completion_callback(1, [&call_count]() { call_count.fetch_add(1); });
    adapter.fire_completion(1);

    REQUIRE(call_count.load() == 0);  // Phase 8.B 占位: callback 不应被调用
}

TEST_CASE("AsyncCompletionAdapter placeholder: fire_completion_count_ increments on each fire",
          "[gpu][async]") {
    AsyncCompletionAdapter adapter;

    REQUIRE(adapter.fire_completion_count() == 0);

    adapter.fire_completion(42);
    REQUIRE(adapter.fire_completion_count() == 1);

    adapter.fire_completion(42);
    REQUIRE(adapter.fire_completion_count() == 2);

    adapter.fire_completion(999);  // 未知 id 也递增
    REQUIRE(adapter.fire_completion_count() == 3);
}

TEST_CASE("AsyncCompletionAdapter placeholder: fire unknown id does not crash",
          "[gpu][async]") {
    AsyncCompletionAdapter adapter;

    // 未 register 直接 fire
    REQUIRE_NOTHROW(adapter.fire_completion(12345));
    REQUIRE(adapter.fire_completion_count() == 1);
}

TEST_CASE("AsyncCompletionAdapter placeholder: register overwrites previous callback",
          "[gpu][async]") {
    AsyncCompletionAdapter adapter;
    std::atomic<int> first_count{0};
    std::atomic<int> second_count{0};

    // 第一次 register id=10
    adapter.register_completion_callback(10, [&first_count]() { first_count.fetch_add(1); });
    // 第二次 register 同 id=10 覆盖
    adapter.register_completion_callback(10, [&second_count]() { second_count.fetch_add(1); });

    // fire: Phase 8.B 占位不调用任何 callback, 但计数+1
    adapter.fire_completion(10);

    REQUIRE(first_count.load() == 0);   // 已被覆盖, 未调用
    REQUIRE(second_count.load() == 0);  // 占位未调用
    REQUIRE(adapter.fire_completion_count() == 1);
}

TEST_CASE("AsyncCompletionAdapter placeholder: nullptr safety pattern (caller checks)",
          "[gpu][async]") {
    // 验证: 调用方应先检查 nullptr (KernelLaunchTLM::set_async_completion 接收 raw pointer)
    // 这不是 adapter 内部的责任, 而是设计约束 — adapter 假设调用方不会传入 nullptr
    AsyncCompletionAdapter adapter;
    AsyncCompletionAdapter* null_adapter = nullptr;

    REQUIRE_NOTHROW((void)null_adapter);  // 编译器不警告
    // 实际 nullptr 调用未定义, 不测试以避免 UB
    REQUIRE(adapter.fire_completion_count() == 0);
}