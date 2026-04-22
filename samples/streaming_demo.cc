/**
 * @file streaming_demo.cc
 * @brief StreamingReporter 使用示例
 *
 * 演示如何在仿真过程中流式输出统计指标：
 * 1. 注册统计组到 StatsManager
 * 2. 配置 StreamingReporter
 * 3. 分批运行仿真并输出统计
 */

#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include "metrics/stats.hh"
#include "metrics/stats_manager.hh"
#include "metrics/streaming_reporter.hh"

// 模拟模块：带统计的 CPU
class StatsCPU {
public:
    tlm_stats::StatGroup stats_;
    tlm_stats::Scalar& cycles_;
    tlm_stats::Scalar& instructions_;
    tlm_stats::Distribution& latency_;
    tlm_stats::Average& ipc_;

    explicit StatsCPU(const std::string& name)
        : stats_("cpu")
        , cycles_(stats_.addScalar("cycles", "CPU cycles", "cycle"))
        , instructions_(stats_.addScalar("instructions", "Instructions", "count"))
        , latency_(stats_.addDistribution("latency", "Memory latency", "cycle"))
        , ipc_(stats_.addAverage("ipc", "Instructions per cycle", "ipc"))
    {
        tlm_stats::StatsManager::instance().register_group(&stats_, name);
    }

    void tick() {
        cycles_++;
        if (latency_.count() % 10 == 0) {
            instructions_++;
            ipc_.sample(1.0, 1);
        }
        latency_.sample(4 + (latency_.count() % 3));
    }
};

// 模拟模块：带统计的 Cache
class StatsCache {
public:
    tlm_stats::StatGroup stats_;
    tlm_stats::Scalar& accesses_;
    tlm_stats::Scalar& hits_;
    tlm_stats::Scalar& misses_;
    tlm_stats::PercentileHistogram& latency_hist_;

    explicit StatsCache(const std::string& name)
        : stats_("cache")
        , accesses_(stats_.addScalar("accesses", "Cache accesses", "count"))
        , hits_(stats_.addScalar("hits", "Cache hits", "count"))
        , misses_(stats_.addScalar("misses", "Cache misses", "count"))
        , latency_hist_(stats_.addPercentileHistogram("latency", "Access latency", "cycle"))
    {
        tlm_stats::StatsManager::instance().register_group(&stats_, name);
    }

    void access(bool hit, int latency) {
        accesses_++;
        if (hit) {
            hits_++;
        } else {
            misses_++;
        }
        latency_hist_.record(latency);
    }
};

int main(int argc, char* argv[]) {
    std::cout << "=== StreamingReporter Demo ===\n\n";

    // 解析命令行参数
    std::string output_path = "output/stats_stream.jsonl";
    uint64_t interval = 1000;
    uint64_t total_cycles = 5000;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else if (strcmp(argv[i], "--interval") == 0 && i + 1 < argc) {
            interval = std::strtoull(argv[++i], nullptr, 10);
        } else if (strcmp(argv[i], "--cycles") == 0 && i + 1 < argc) {
            total_cycles = std::strtoull(argv[++i], nullptr, 10);
        }
    }

    std::cout << "Configuration:\n";
    std::cout << "  Output: " << output_path << "\n";
    std::cout << "  Interval: " << interval << " cycles\n";
    std::cout << "  Total cycles: " << total_cycles << "\n\n";

    // 创建模拟模块
    StatsCPU cpu("system.cpu");
    StatsCache l1_cache("system.cache.l1");
    StatsCache l2_cache("system.cache.l2");

    // 创建流式报告器
    tlm_stats::StreamingReporter reporter(output_path);
    reporter.set_interval(interval);

    std::cout << "Starting simulation with streaming stats...\n\n";

    // 启动报告器
    reporter.start();

    // 分批运行仿真
    uint64_t current_cycle = 0;
    while (current_cycle < total_cycles) {
        uint64_t batch = (interval < total_cycles - current_cycle) ? interval : total_cycles - current_cycle;

        // 模拟本批的统计更新
        for (uint64_t i = 0; i < batch; ++i) {
            cpu.tick();

            bool l1_hit = (cpu.cycles_.value() % 5) != 0;
            l1_cache.access(l1_hit, l1_hit ? 4 : 12);

            bool l2_hit = (cpu.cycles_.value() % 7) != 0;
            l2_cache.access(l2_hit, l2_hit ? 12 : 40);

            current_cycle++;
        }

        // 输出本批统计
        reporter.set_current_cycle(current_cycle);
        reporter.enqueue_all();

        std::cout << "[Cycle " << current_cycle << "] Stats snapshot written\n";

        // 模拟一点延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 停止报告器
    reporter.stop();

    std::cout << "\n[INFO] Simulation complete. Stats written to " << output_path << "\n";
    std::cout << "\nYou can analyze the stats with:\n";
    std::cout << "  python3 scripts/stats_annotator.py --stats " << output_path << " --output report.html\n";

    return 0;
}