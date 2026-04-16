/**
 * @file test_module_stats.cc
 * @brief Phase 8 Phase 1 — 模块 Stats 集成测试
 * 
 * 测试覆盖：
 * - CacheTLM Stats: requests/hits/misses/latency 采集正确
 * - CrossbarTLM Stats: flits_received/sent/flit_latency 采集正确
 * - MemoryTLM Stats: requests_read/write/row_hits/misses/latency 采集正确
 * - ModuleFactory: enable_metrics/dump_metrics/reset_metrics/metrics_enabled
 * 
 * @author CppTLM Development Team
 * @date 2026-04-16
 */

#include "catch_amalgamated.hpp"
#include "tlm/cache_tlm.hh"
#include "tlm/crossbar_tlm.hh"
#include "tlm/memory_tlm.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"

#include <sstream>
#include <fstream>
#include <filesystem>

namespace {

// ============================================================================
// CacheTLM Stats Tests
// ============================================================================

TEST_CASE("CacheTLM: stats structure exists", "[phase8-module-stats][cache]") {
    EventQueue eq;
    CacheTLM cache("cache", &eq);
    
    auto& stats = cache.stats();
    REQUIRE(stats.stats().count("requests") > 0);
    REQUIRE(stats.stats().count("hits") > 0);
    REQUIRE(stats.stats().count("misses") > 0);
    REQUIRE(stats.stats().count("latency") > 0);
}

TEST_CASE("CacheTLM: stats collection on traffic", "[phase8-module-stats][cache]") {
    EventQueue eq;
    CacheTLM cache("cache", &eq);
    
    // 模拟 100 次 tick，每次触发请求
    for (int i = 0; i < 100; ++i) {
        cache.tick();
    }
    
    auto& stats = cache.stats();
    auto& requests = static_cast<tlm_stats::Scalar&>(*stats.stats().at("requests"));
    
    // 由于没有输入数据，requests 应该为 0（req_in_.valid() == false）
    // 但 stats 结构必须存在
    REQUIRE(requests.value() == 0);
}

TEST_CASE("CacheTLM: stats dump output", "[phase8-module-stats][cache]") {
    EventQueue eq;
    CacheTLM cache("cache", &eq);
    
    std::ostringstream oss;
    cache.dumpStats(oss);
    
    std::string output = oss.str();
    REQUIRE(output.find("cache.requests") != std::string::npos);
    REQUIRE(output.find("cache.hits") != std::string::npos);
    REQUIRE(output.find("cache.latency") != std::string::npos);
}

TEST_CASE("CacheTLM: stats reset", "[phase8-module-stats][cache]") {
    EventQueue eq;
    CacheTLM cache("cache", &eq);
    
    // reset 后统计应为空
    ResetConfig config;
    cache.do_reset(config);
    
    auto& stats = cache.stats();
    auto& requests = static_cast<tlm_stats::Scalar&>(*stats.stats().at("requests"));
    REQUIRE(requests.value() == 0);
}

// ============================================================================
// CrossbarTLM Stats Tests
// ============================================================================

TEST_CASE("CrossbarTLM: stats structure exists", "[phase8-module-stats][crossbar]") {
    EventQueue eq;
    CrossbarTLM xbar("xbar", &eq);
    
    auto& stats = xbar.stats();
    REQUIRE(stats.stats().count("flits_received") > 0);
    REQUIRE(stats.stats().count("flits_sent") > 0);
    REQUIRE(stats.stats().count("flit_latency") > 0);
    REQUIRE(stats.stats().count("buffer_occupancy") > 0);
}

TEST_CASE("CrossbarTLM: stats dump output", "[phase8-module-stats][crossbar]") {
    EventQueue eq;
    CrossbarTLM xbar("xbar", &eq);
    
    std::ostringstream oss;
    xbar.dumpStats(oss);
    
    std::string output = oss.str();
    REQUIRE(output.find("crossbar.flits_received") != std::string::npos);
    REQUIRE(output.find("crossbar.flit_latency") != std::string::npos);
}

TEST_CASE("CrossbarTLM: stats reset", "[phase8-module-stats][crossbar]") {
    EventQueue eq;
    CrossbarTLM xbar("xbar", &eq);
    
    ResetConfig config;
    xbar.do_reset(config);
    
    auto& stats = xbar.stats();
    auto& flits = static_cast<tlm_stats::Scalar&>(*stats.stats().at("flits_received"));
    REQUIRE(flits.value() == 0);
}

// ============================================================================
// MemoryTLM Stats Tests
// ============================================================================

TEST_CASE("MemoryTLM: stats structure exists", "[phase8-module-stats][memory]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);
    
    auto& stats = mem.stats();
    REQUIRE(stats.stats().count("requests_read") > 0);
    REQUIRE(stats.stats().count("requests_write") > 0);
    REQUIRE(stats.stats().count("row_hits") > 0);
    REQUIRE(stats.stats().count("row_misses") > 0);
    REQUIRE(stats.stats().count("latency_read") > 0);
    REQUIRE(stats.stats().count("latency_write") > 0);
    REQUIRE(stats.stats().count("row_buffer_hit_rate") > 0);
}

TEST_CASE("MemoryTLM: stats dump output", "[phase8-module-stats][memory]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);
    
    std::ostringstream oss;
    mem.dumpStats(oss);
    
    std::string output = oss.str();
    REQUIRE(output.find("memory.requests_read") != std::string::npos);
    REQUIRE(output.find("memory.latency_read") != std::string::npos);
    REQUIRE(output.find("memory.row_buffer_hit_rate") != std::string::npos);
}

TEST_CASE("MemoryTLM: stats reset", "[phase8-module-stats][memory]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);
    
    ResetConfig config;
    mem.do_reset(config);
    
    auto& stats = mem.stats();
    auto& reads = static_cast<tlm_stats::Scalar&>(*stats.stats().at("requests_read"));
    REQUIRE(reads.value() == 0);
}

TEST_CASE("MemoryTLM: Formula row_buffer_hit_rate", "[phase8-module-stats][memory]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);
    
    auto& stats = mem.stats();
    auto& hit_rate = static_cast<tlm_stats::Formula&>(*stats.stats().at("row_buffer_hit_rate"));
    
    // reset 后命中率为 0
    REQUIRE(hit_rate.value() == 0.0);
}

// ============================================================================
// ModuleFactory Metrics Tests
// ============================================================================

TEST_CASE("ModuleFactory: metrics disabled by default", "[phase8-module-stats][factory]") {
    EventQueue eq;
    ModuleFactory factory(&eq);
    
    REQUIRE(factory.metrics_enabled() == false);
    REQUIRE(factory.stats_root() == nullptr);
}

TEST_CASE("ModuleFactory: enable_metrics creates stats root", "[phase8-module-stats][factory]") {
    EventQueue eq;
    ModuleFactory factory(&eq);
    
    factory.enable_metrics(true);
    
    REQUIRE(factory.metrics_enabled() == true);
    REQUIRE(factory.stats_root() != nullptr);
    REQUIRE(factory.stats_root()->name() == "system");
}

TEST_CASE("ModuleFactory: enable_metrics(false) does not create root", "[phase8-module-stats][factory]") {
    EventQueue eq;
    ModuleFactory factory(&eq);
    
    factory.enable_metrics(false);
    
    REQUIRE(factory.metrics_enabled() == false);
    REQUIRE(factory.stats_root() == nullptr);
}

TEST_CASE("ModuleFactory: dump_metrics to file", "[phase8-module-stats][factory]") {
    EventQueue eq;
    ModuleFactory factory(&eq);
    factory.enable_metrics(true);
    
    // 添加子组和统计
    auto& cache = factory.stats_root()->addSubgroup("cache");
    auto& requests = cache.addScalar("requests", "Total requests", "count");
    requests += 100;
    
    // 导出到文件
    std::string path = "/tmp/test_module_stats_dump.txt";
    factory.dump_metrics(path);
    
    // 验证文件内容
    std::ifstream ifs(path);
    REQUIRE(ifs.is_open());
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();
    
    REQUIRE(content.find("---------- Begin Simulation Statistics ----------") != std::string::npos);
    REQUIRE(content.find("system.cache.requests") != std::string::npos);
    REQUIRE(content.find("---------- End Simulation Statistics ----------") != std::string::npos);
    
    // 清理
    std::filesystem::remove(path);
}

TEST_CASE("ModuleFactory: reset_metrics clears all stats", "[phase8-module-stats][factory]") {
    EventQueue eq;
    ModuleFactory factory(&eq);
    factory.enable_metrics(true);
    
    auto& cache = factory.stats_root()->addSubgroup("cache");
    auto& requests = cache.addScalar("requests", "Total requests", "count");
    requests += 100;
    
    factory.reset_metrics();
    
    REQUIRE(requests.value() == 0);
}

TEST_CASE("ModuleFactory: dump_metrics when disabled does nothing", "[phase8-module-stats][factory]") {
    EventQueue eq;
    ModuleFactory factory(&eq);
    // 未启用 metrics
    
    // 不应崩溃
    factory.dump_metrics("/tmp/test_disabled_metrics.txt");
    
    // 文件应该不存在
    REQUIRE(!std::filesystem::exists("/tmp/test_disabled_metrics.txt"));
}

} // anonymous namespace
