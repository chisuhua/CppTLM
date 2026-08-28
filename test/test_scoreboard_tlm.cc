/**
 * @file test_scoreboard_tlm.cc
 * @brief ScoreboardTLM 单测 (D1-Full P1 #C4, Phase 1 核心模块)
 *
 * 测试范围 (per openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/spec.md §cpptlm-scoreboard):
 *   - Global hazard table 语义: O(1) unordered_map, CAPACITY=2048
 *   - allocate/release/has_free_entry 基本流程
 *   - 2048 entries 满后 allocate 返回 false
 *   - duplicate allocate (重复 reg_id+warp_id) rejects
 *   - multi_warp: 同 reg_id 不同 warp_id 独立 allocate
 *   - tick() no-op 不改变 entries
 *   - reset() 清空所有 entries
 *
 * Design Revision 3 (2026-07-18 Oracle P0):
 *   - unordered_map O(1): 消除线性扫描 O(N) 时序失真
 *   - CAPACITY=2048: 64 warps × 8 in-flight × 3 dest × 1.33 margin
 *   - 新增 reset() 跨 kernel 测试
 *
 * @author CppTLM / 日期 2026-07-18
 */

#include "catch_amalgamated.hpp"
#include "tlm/gpu/scoreboard_tlm.hh"

using namespace tlm;

TEST_CASE("ScoreboardTLM: has_free_entry initially returns true", "[gpu][d1p1]") {
    ScoreboardTLM sb;
    REQUIRE(sb.has_free_entry());
}

TEST_CASE("ScoreboardTLM: allocate fills entries and returns false when full", "[gpu][d1p1]") {
    ScoreboardTLM sb;

    // Fill all 2048 entries (64 warps × 8 in-flight × 3 dest margin)
    for (size_t i = 0; i < ScoreboardTLM::CAPACITY; ++i) {
        // 用不同 (reg_id, warp_id) 确保无 duplicate
        uint32_t reg = static_cast<uint32_t>(i % 65536);
        uint32_t warp = static_cast<uint32_t>(i / 65536);
        bool ok = sb.allocate(reg, warp);
        REQUIRE(ok);
    }

    REQUIRE_FALSE(sb.has_free_entry());

    // 第 2049 个 allocate 应失败
    bool full = sb.allocate(65535, 31);
    REQUIRE_FALSE(full);
}

TEST_CASE("ScoreboardTLM: release frees entry for reuse", "[gpu][d1p1]") {
    ScoreboardTLM sb;

    REQUIRE(sb.allocate(10, 0));
    REQUIRE(sb.release(10, 0));

    // reuse 刚释放的 slot
    REQUIRE(sb.allocate(10, 0));
    REQUIRE(sb.release(10, 0));
}

TEST_CASE("ScoreboardTLM: release unknown reg returns false", "[gpu][d1p1]") {
    ScoreboardTLM sb;
    REQUIRE_FALSE(sb.release(42, 0));
}

TEST_CASE("ScoreboardTLM: duplicate allocate returns false (rejects)", "[gpu][d1p1]") {
    ScoreboardTLM sb;

    REQUIRE(sb.allocate(10, 0));
    REQUIRE_FALSE(sb.allocate(10, 0)); // duplicate rejects

    REQUIRE(sb.release(10, 0));
    REQUIRE(sb.allocate(10, 0)); // 释放后可重新 allocate
}

TEST_CASE("ScoreboardTLM: multi_warp allocate independent", "[gpu][d1p1]") {
    ScoreboardTLM sb;

    REQUIRE(sb.allocate(5, 0));
    REQUIRE(sb.allocate(5, 1)); // 同 reg_id, 不同 warp_id — 独立
    REQUIRE(sb.allocate(5, 2));

    REQUIRE(sb.release(5, 0));
    REQUIRE(sb.release(5, 1));
    REQUIRE(sb.release(5, 2));
}

TEST_CASE("ScoreboardTLM: tick is no-op in Phase 1", "[gpu][d1p1]") {
    ScoreboardTLM sb;

    REQUIRE(sb.allocate(1, 0));
    REQUIRE(sb.allocate(2, 0));
    REQUIRE(sb.has_free_entry()); // 2048 capacity, 2 used → yes

    sb.tick(); // no-op

    // tick 后状态不变
    REQUIRE_FALSE(sb.release(99, 0)); // 未分配 entry 应 release 失败
    REQUIRE(sb.release(1, 0));
    REQUIRE(sb.release(2, 0));
}

TEST_CASE("ScoreboardTLM: reset clears all entries", "[gpu][d1p1]") {
    ScoreboardTLM sb;

    // 分配若干 entries
    for (uint32_t i = 0; i < 100; ++i) {
        REQUIRE(sb.allocate(i, 0));
    }
    REQUIRE_FALSE(sb.allocate(0, 0)); // duplicate → alloc 100 entries

    sb.reset();

    // reset 后所有 entries 清空，可重新分配
    REQUIRE(sb.has_free_entry());
    REQUIRE(sb.allocate(0, 0)); // 之前是 duplicate 的 reg 现在可用
    REQUIRE(sb.allocate(1, 0));
    REQUIRE(sb.release(0, 0));
    REQUIRE(sb.release(1, 0));
}

TEST_CASE("ScoreboardTLM: O(1) lookup — many entries", "[gpu][d1p1]") {
    ScoreboardTLM sb;

    // 分配 1024 个 entries（不触发上限）
    for (uint32_t i = 0; i < 1024; ++i) {
        REQUIRE(sb.allocate(i, i % 64));
    }

    // 中间 entry 的 duplicate 检测应为 O(1)
    REQUIRE_FALSE(sb.allocate(512, 512 % 64)); // duplicate

    // 释放也应 O(1)
    REQUIRE(sb.release(512, 512 % 64));

    // 可重新分配
    REQUIRE(sb.allocate(512, 512 % 64));
}