/**
 * @file stats_demo.cc
 * @brief Phase 8 性能统计框架演示
 * 
 * 展示如何收集和输出性能指标数据
 */

#include <iostream>
#include "metrics/stats.hh"

int main() {
    std::cout << "=== Phase 8 Stats Framework Demo ===\n\n";

    // 1. 创建层次化统计组 (模拟 CacheTLM)
    tlm_stats::StatGroup system("system");
    
    auto& cache = system.addSubgroup("cache");
    auto& l1_icache = cache.addSubgroup("l1_icache");
    
    // 添加统计指标
    auto& accesses = l1_icache.addScalar("accesses", "L1 I-cache accesses", "count");
    auto& hits = l1_icache.addScalar("hits", "L1 I-cache hits", "count");
    auto& misses = l1_icache.addScalar("misses", "L1 I-cache misses", "count");
    auto& latency = l1_icache.addDistribution("latency", "L1 I-cache access latency", "cycle");
    
    // 添加 CPU 统计
    auto& cpu = system.addSubgroup("cpu");
    auto& cycles = cpu.addScalar("cycles", "CPU cycles", "cycle");
    auto& instructions = cpu.addScalar("instructions", "Instructions executed", "count");
    auto& ipc = cpu.addAverage("ipc", "Instructions per cycle", "ipc");
    
    // 2. 模拟流量
    std::cout << "--- Simulating traffic ---\n";
    for (int i = 0; i < 100000; ++i) {
        ++cycles;
        ++instructions;
        ipc.sample(1.0, 1);
        
        ++accesses;
        latency.sample(4 + (i % 3));  // 4-6 cycle latency
        
        if (i % 5 == 0) {  // 20% miss rate
            ++misses;
        } else {
            ++hits;
        }
    }
    
    std::cout << "  cycles: " << cycles.value() << "\n";
    std::cout << "  instructions: " << instructions.value() << "\n";
    std::cout << "  cache accesses: " << accesses.value() << "\n";
    std::cout << "  cache hits: " << hits.value() << "\n";
    std::cout << "  cache misses: " << misses.value() << "\n\n";

    // 3. 输出统计 (gem5 风格)
    std::cout << "--- Stats Output (gem5 style) ---\n\n";
    system.dump(std::cout);
    
    std::cout << "\n--- Derived Metrics ---\n";
    double miss_rate = static_cast<double>(misses.value()) / accesses.value();
    std::cout << "  system.cache.l1_icache.miss_rate: " 
              << miss_rate << " (" << (miss_rate * 100) << "%)\n";
    std::cout << "  system.cpu.ipc: " << ipc.result() << "\n";
    
    // 4. 演示 reset
    std::cout << "\n--- After Reset ---\n";
    system.reset();
    std::cout << "  cycles after reset: " << cycles.value() << "\n";
    std::cout << "  latency.count after reset: " << latency.count() << "\n";
    
    return 0;
}
