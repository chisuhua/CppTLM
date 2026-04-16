/**
 * @file test_percentile_histogram.cc
 * @brief Phase 8 Phase 2 — PercentileHistogram 单元测试
 * 
 * @author CppTLM Development Team
 * @date 2026-04-16
 */

#include "catch_amalgamated.hpp"
#include "metrics/histogram.hh"

#include <vector>
#include <sstream>
#include <algorithm>

namespace {

// ============================================================================
// Basic Tests
// ============================================================================

TEST_CASE("PercentileHistogram.zero_samples", "[phase8-histogram][basic]") {
    tlm_stats::PercentileHistogram hist("Test", "cycle");
    
    REQUIRE(hist.total_count() == 0);
    REQUIRE(hist.min_value() == 0);
    REQUIRE(hist.max_value() == 0);
    REQUIRE(hist.p50() == 0);
    REQUIRE(hist.p99() == 0);
}

TEST_CASE("PercentileHistogram.single_record", "[phase8-histogram][basic]") {
    tlm_stats::PercentileHistogram hist("Test", "cycle");
    
    hist.record(42);
    
    REQUIRE(hist.total_count() == 1);
    REQUIRE(hist.min_value() == 42);
    REQUIRE(hist.max_value() == 42);
    // 42 在 bucket 5 [32, 64)，p50 线性插值为 32+0.5*32=48
    REQUIRE(hist.p50() == 48);
    REQUIRE(hist.p99() == 63);  // 32+0.99*32=63.68 → 63
    REQUIRE(hist.p99() >= 60);
}

TEST_CASE("PercentileHistogram.neglects_zero_and_negative", "[phase8-histogram][basic]") {
    tlm_stats::PercentileHistogram hist("Test", "cycle");
    
    hist.record(0);
    hist.record(-1);
    hist.record(-100);
    hist.record(10);
    
    REQUIRE(hist.total_count() == 1);  // only the 10 should count
    REQUIRE(hist.min_value() == 10);
}

// ============================================================================
// Percentile Calculation Tests
// ============================================================================

TEST_CASE("PercentileHistogram.uniform_values", "[phase8-histogram][percentile]") {
    tlm_stats::PercentileHistogram hist("Uniform", "cycle");
    
    // 100 values from 1 to 100
    for (int i = 1; i <= 100; i++) {
        hist.record(i);
    }
    
    REQUIRE(hist.total_count() == 100);
    REQUIRE(hist.min_value() == 1);
    REQUIRE(hist.max_value() == 100);
    
    // p50 应该在 50 附近（由于指数桶量化，可能略有偏差）
    auto p50 = hist.p50();
    REQUIRE(p50 >= 40);
    REQUIRE(p50 <= 64);  // within one bucket of true p50
    
    auto p99 = hist.p99();
    REQUIRE(p99 >= 64);
    REQUIRE(p99 <= 128);  // within bucket of 64-127
}

TEST_CASE("PercentileHistogram.skewed_distribution", "[phase8-histogram][percentile]") {
    tlm_stats::PercentileHistogram hist("Skewed", "cycle");
    
    // 90 values of 5, 10 values of 100
    for (int i = 0; i < 90; i++) {
        hist.record(5);
    }
    for (int i = 0; i < 10; i++) {
        hist.record(100);
    }
    
    REQUIRE(hist.total_count() == 100);
    
    // p50 should be 5 (since 50th percentile falls in the 5-value region)
    REQUIRE(hist.p50() <= 8);   // bucket [4,7] or [4,8]
    
    // p99 should be around 100
    REQUIRE(hist.p99() >= 64);  // 100 falls in [64, 127)
    REQUIRE(hist.p99() <= 128);
}

TEST_CASE("PercentileHistogram.wide_range_values", "[phase8-histogram][percentile]") {
    tlm_stats::PercentileHistogram hist("Wide range", "cycle");
    
    // Values spanning multiple decades
    hist.record(1);
    hist.record(10);
    hist.record(100);
    hist.record(1000);
    hist.record(10000);
    
    REQUIRE(hist.total_count() == 5);
    REQUIRE(hist.min_value() == 1);
    REQUIRE(hist.max_value() == 10000);
    
    // p50 should be in the 100-255 bucket
    auto p50 = hist.p50();
    REQUIRE(p50 >= 64);
    REQUIRE(p50 <= 256);
}

// ============================================================================
// Reset Tests
// ============================================================================

TEST_CASE("PercentileHistogram.reset_clears_all", "[phase8-histogram][reset]") {
    tlm_stats::PercentileHistogram hist("Reset test", "cycle");
    
    for (int i = 1; i <= 100; i++) {
        hist.record(i);
    }
    
    REQUIRE(hist.total_count() == 100);
    
    hist.reset();
    
    REQUIRE(hist.total_count() == 0);
    REQUIRE(hist.min_value() == 0);
    REQUIRE(hist.max_value() == 0);
    REQUIRE(hist.p50() == 0);
}

// ============================================================================
// Memory and Dump Tests
// ============================================================================

TEST_CASE("PercentileHistogram.memory_usage_small", "[phase8-histogram][memory]") {
    tlm_stats::PercentileHistogram hist("Memory test", "cycle");
    
    REQUIRE(hist.memory_usage() == 32 * sizeof(uint64_t));  // 256 bytes
    REQUIRE(hist.memory_usage() < 512);
}

TEST_CASE("PercentileHistogram.dump_output", "[phase8-histogram][dump]") {
    tlm_stats::PercentileHistogram hist("Dump test", "cycle");
    
    for (int i = 1; i <= 100; i++) {
        hist.record(i);
    }
    
    std::ostringstream oss;
    hist.dump(oss, "test.hist", 50);
    
    std::string output = oss.str();
    
    REQUIRE(output.find("test.hist.count") != std::string::npos);
    REQUIRE(output.find("test.hist.p50") != std::string::npos);
    REQUIRE(output.find("test.hist.p95") != std::string::npos);
    REQUIRE(output.find("test.hist.p99") != std::string::npos);
    REQUIRE(output.find("test.hist.p99.9") != std::string::npos);
    REQUIRE(output.find("test.hist.max") != std::string::npos);
}

// ============================================================================
// StatGroup Integration Tests
// ============================================================================

TEST_CASE("StatGroup.addPercentileHistogram", "[phase8-histogram][integration]") {
    tlm_stats::StatGroup root("system");
    
    auto& cache = root.addSubgroup("cache");
    auto& hist = cache.addPercentileHistogram("latency_hist", "Latency percentiles", "cycle");
    
    // Verify the reference works
    hist.record(10);
    hist.record(20);
    hist.record(30);
    
    REQUIRE(hist.total_count() == 3);
    REQUIRE(hist.min_value() == 10);
    
    // Verify it's also accessible through the group
    auto* found = cache.findStat("latency_hist");
    REQUIRE(found != nullptr);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("PercentileHistogram.large_values", "[phase8-histogram][edge]") {
    tlm_stats::PercentileHistogram hist("Large values", "cycle");
    
    // Record values near 2^31
    hist.record(1000000000);  // 1 billion
    hist.record(2000000000);  // 2 billion
    
    REQUIRE(hist.total_count() == 2);
    REQUIRE(hist.min_value() == 1000000000);
    REQUIRE(hist.max_value() == 2000000000);
}

TEST_CASE("PercentileHistogram.identical_values", "[phase8-histogram][edge]") {
    tlm_stats::PercentileHistogram hist("Identical", "cycle");
    
    for (int i = 0; i < 1000; i++) {
        hist.record(42);
    }
    
    REQUIRE(hist.total_count() == 1000);
    REQUIRE(hist.min_value() == 42);
    REQUIRE(hist.max_value() == 42);
    // 42 在 bucket 5 [32, 64)，p50 线性插值为 48
    REQUIRE(hist.p50() == 48);
    // p99: 1000 条记录，target=990，position=0.9 → 32+0.9*32≈60
    REQUIRE(hist.p99() >= 60);
    REQUIRE(hist.p99() <= 64);
}

TEST_CASE("PercentileHistogram.percentile_bounds", "[phase8-histogram][edge]") {
    tlm_stats::PercentileHistogram hist("Bounds test", "cycle");
    
    for (int i = 1; i <= 100; i++) {
        hist.record(i);
    }
    
    // Out-of-range percentile values should be clamped
    REQUIRE(hist.percentile(-10.0) == hist.percentile(0.0));
    REQUIRE(hist.percentile(110.0) == hist.percentile(100.0));
}

TEST_CASE("PercentileHistogram.all_percentile_methods", "[phase8-histogram][edge]") {
    tlm_stats::PercentileHistogram hist("All methods", "cycle");
    
    for (int i = 1; i <= 100; i++) {
        hist.record(i);
    }
    
    // All percentile methods should return values within range
    auto v50 = hist.p50();
    auto v95 = hist.p95();
    auto v99 = hist.p99();
    auto v99_9 = hist.p99_9();
    
    REQUIRE(v50 >= 1);
    REQUIRE(v95 >= v50);  // p95 >= p50
    REQUIRE(v99 >= v95);  // p99 >= p95
    REQUIRE(v99_9 >= v99);  // p99.9 >= p99
    REQUIRE(v99_9 <= 128);  // max value is 100, p99.9 can't exceed next bucket
}

} // anonymous namespace
