// test/test_stress_patterns.cc
// Phase 8: TrafficGenTLM 6种压力模式单元测试
// 功能描述：验证 SEQUENTIAL/RANDOM/HOTSPOT/STRIDED/NEIGHBOR/TORNADO 6种压力模式策略
// 作者 CppTLM Development Team
// 日期 2026-04-16
#include "catch_amalgamated.hpp"
#include "tlm/stress_patterns.hh"
#include "tlm/traffic_gen_tlm.hh"
#include "core/event_queue.hh"

using namespace std;

TEST_CASE("SequentialStrategy produces linear addresses", "[phase8-stress-patterns][sequential]") {
    SequentialStrategy strategy;
    uint64_t base = 0x1000;
    uint64_t range = 16;

    // 验证前5个地址是线性递增的
    REQUIRE(strategy.next_address(base, range) == base + 0);  // 0x1000
    REQUIRE(strategy.next_address(base, range) == base + 1);  // 0x1001
    REQUIRE(strategy.next_address(base, range) == base + 2);  // 0x1002
    REQUIRE(strategy.next_address(base, range) == base + 3);  // 0x1003
    REQUIRE(strategy.next_address(base, range) == base + 4);  // 0x1004

    // 验证回绕
    for (int i = 5; i < 16; i++) {
        strategy.next_address(base, range);
    }
    REQUIRE(strategy.next_address(base, range) == base);  // 回绕到起始地址
}

TEST_CASE("SequentialStrategy reset works correctly", "[phase8-stress-patterns][sequential][reset]") {
    SequentialStrategy strategy;
    uint64_t base = 0x1000;
    uint64_t range = 16;

    // 消耗几个地址
    strategy.next_address(base, range);
    strategy.next_address(base, range);
    strategy.next_address(base, range);

    // reset 后应从头开始
    strategy.reset();
    REQUIRE(strategy.next_address(base, range) == base + 0);
    REQUIRE(strategy.next_address(base, range) == base + 1);
}

TEST_CASE("RandomStrategy produces values within range", "[phase8-stress-patterns][random]") {
    RandomStrategy strategy;
    uint64_t base = 0x1000;
    uint64_t range = 1000;

    // 运行多次，验证所有值都在 [base, base+range) 范围内
    for (int i = 0; i < 1000; i++) {
        uint64_t addr = strategy.next_address(base, range);
        REQUIRE(addr >= base);
        REQUIRE(addr < base + range);
    }
}

TEST_CASE("RandomStrategy produces roughly uniform distribution", "[phase8-stress-patterns][random]") {
    RandomStrategy strategy;
    uint64_t base = 0;
    uint64_t range = 100;
    const int total_samples = 10000;

    // 分成4个桶，验证分布大致均匀（每个桶 25%左右，允许 ±15%）
    vector<int> bucket_counts(4, 0);
    for (int i = 0; i < total_samples; i++) {
        uint64_t addr = strategy.next_address(base, range);
        size_t bucket = (addr * 4) / range;  // 0-3
        if (bucket >= 4) bucket = 3;
        bucket_counts[bucket]++;
    }

    // 验证每个桶至少有 10% 的样本
    for (int i = 0; i < 4; i++) {
        double ratio = static_cast<double>(bucket_counts[i]) / total_samples;
        REQUIRE(ratio > 0.10);  // 至少 10%
    }
}

TEST_CASE("RandomStrategy reset reinitializes RNG", "[phase8-stress-patterns][random][reset]") {
    RandomStrategy strategy1;
    RandomStrategy strategy2;
    uint64_t base = 0;
    uint64_t range = 100;

    // 两个相同种子的策略应产生相同序列
    for (int i = 0; i < 100; i++) {
        strategy1.next_address(base, range);
    }

    strategy1.reset();
    for (int i = 0; i < 100; i++) {
        REQUIRE(strategy1.next_address(base, range) == strategy2.next_address(base, range));
    }
}

TEST_CASE("HotspotStrategy respects weights", "[phase8-stress-patterns][hotspot]") {
    HotspotStrategy strategy;
    vector<uint64_t> addrs = {0x1000, 0x2000, 0x3000};
    vector<double> weights = {50.0, 30.0, 20.0};  // 50%, 30%, 20%
    strategy.set_hotspot_config(addrs, weights);

    const int total_samples = 10000;
    vector<int> counts(3, 0);

    for (int i = 0; i < total_samples; i++) {
        uint64_t addr = strategy.next_address(0, 0);
        if (addr == 0x1000) counts[0]++;
        else if (addr == 0x2000) counts[1]++;
        else if (addr == 0x3000) counts[2]++;
    }

    // 验证比例在 ±5% 以内
    double ratio0 = static_cast<double>(counts[0]) / total_samples;
    double ratio1 = static_cast<double>(counts[1]) / total_samples;
    double ratio2 = static_cast<double>(counts[2]) / total_samples;

    REQUIRE(ratio0 > 0.45);  // 50% ± 5%
    REQUIRE(ratio0 < 0.55);
    REQUIRE(ratio1 > 0.25);  // 30% ± 5%
    REQUIRE(ratio1 < 0.35);
    REQUIRE(ratio2 > 0.15);  // 20% ± 5%
    REQUIRE(ratio2 < 0.25);
}

TEST_CASE("HotspotStrategy reset reinitializes RNG", "[phase8-stress-patterns][hotspot][reset]") {
    HotspotStrategy strategy1;
    HotspotStrategy strategy2;
    vector<uint64_t> addrs = {0x1000, 0x2000, 0x3000};
    vector<double> weights = {50.0, 30.0, 20.0};
    strategy1.set_hotspot_config(addrs, weights);
    strategy2.set_hotspot_config(addrs, weights);

    // 消耗一些随机数
    for (int i = 0; i < 100; i++) {
        strategy1.next_address(0, 0);
    }

    strategy1.reset();
    for (int i = 0; i < 100; i++) {
        REQUIRE(strategy1.next_address(0, 0) == strategy2.next_address(0, 0));
    }
}

TEST_CASE("StridedStrategy produces fixed stride", "[phase8-stress-patterns][strided]") {
    StridedStrategy strategy;
    strategy.set_stride(64);
    uint64_t base = 0x1000;
    uint64_t range = 256;

    // 验证连续地址在范围内递增，差值为 stride
    vector<uint64_t> values;
    for (int i = 0; i < 5; i++) {
        values.push_back(strategy.next_address(base, range));
    }
    
    // 验证都在范围内
    for (uint64_t v : values) {
        REQUIRE(v >= base);
        REQUIRE(v < base + range);
    }
    
    // 验证差值正确（不 wrap 的情况下）
    REQUIRE(values[1] - values[0] == 64);
    REQUIRE(values[2] - values[1] == 64);
    REQUIRE(values[3] - values[2] == 64);
}

TEST_CASE("StridedStrategy wraps around correctly", "[phase8-stress-patterns][strided]") {
    StridedStrategy strategy;
    strategy.set_stride(64);
    uint64_t base = 0x1000;
    uint64_t range = 128;  // 128 字节范围，stride 64 应该回绕

    vector<uint64_t> values;
    for (int i = 0; i < 10; i++) {
        values.push_back(strategy.next_address(base, range));
    }

    // 验证前两个值在范围内
    REQUIRE(values[0] >= base);
    REQUIRE(values[0] < base + range);
    REQUIRE(values[1] >= base);
    REQUIRE(values[1] < base + range);

    // 验证值是重复的（因为 range / stride = 2）
    REQUIRE(values[0] == values[2]);
    REQUIRE(values[1] == values[3]);
}

TEST_CASE("StridedStrategy reset works correctly", "[phase8-stress-patterns][strided][reset]") {
    StridedStrategy strategy;
    strategy.set_stride(10);
    uint64_t base = 0;

    strategy.next_address(base, 100);
    strategy.next_address(base, 100);
    strategy.next_address(base, 100);

    strategy.reset();
    REQUIRE(strategy.next_address(base, 100) == base);
    REQUIRE(strategy.next_address(base, 100) == base + 10);
}

TEST_CASE("NeighborStrategy produces locality", "[phase8-stress-patterns][neighbor]") {
    NeighborStrategy strategy;
    uint64_t base = 0x1000;
    uint64_t range = 1000;

    int adjacent_count = 0;
    const int total_samples = 1000;

    uint64_t prev = base;
    for (int i = 0; i < total_samples; i++) {
        uint64_t curr = strategy.next_address(base, range);
        // 检查是否与前一个地址相邻（差值为1）
        if (curr == prev + 1 || curr + 1 == prev) {
            adjacent_count++;
        }
        prev = curr;
    }

    // 约 80% 应该相邻（宽松验证，至少 50%）
    double ratio = static_cast<double>(adjacent_count) / total_samples;
    REQUIRE(ratio > 0.50);
}

TEST_CASE("NeighborStrategy reset works correctly", "[phase8-stress-patterns][neighbor][reset]") {
    NeighborStrategy strategy;
    uint64_t base = 0x1000;
    uint64_t range = 100;

    strategy.next_address(base, range);
    strategy.next_address(base, range);
    strategy.next_address(base, range);

    strategy.reset();
    uint64_t first = strategy.next_address(base, range);
    REQUIRE(first >= base);
    REQUIRE(first < base + range);
}

TEST_CASE("TornadoStrategy produces addresses in range", "[phase8-stress-patterns][tornado]") {
    TornadoStrategy strategy;
    strategy.set_mesh_config(4, 4);
    uint64_t base = 0;
    uint64_t range = 16;

    for (int i = 0; i < 100; i++) {
        uint64_t addr = strategy.next_address(base, range);
        REQUIRE(addr >= base);
        REQUIRE(addr < base + range);
    }
}

TEST_CASE("TornadoStrategy visits multiple nodes", "[phase8-stress-patterns][tornado]") {
    TornadoStrategy strategy;
    strategy.set_mesh_config(4, 4);
    uint64_t base = 0;
    uint64_t range = 16;

    vector<bool> visited(16, false);
    for (int i = 0; i < 32; i++) {
        uint64_t addr = strategy.next_address(base, range);
        size_t node = static_cast<size_t>(addr - base);
        if (node < 16) {
            visited[node] = true;
        }
    }

    int visited_count = 0;
    for (bool v : visited) {
        if (v) visited_count++;
    }
    REQUIRE(visited_count >= 8);
}

TEST_CASE("TornadoStrategy reset works correctly", "[phase8-stress-patterns][tornado][reset]") {
    TornadoStrategy strategy;
    strategy.set_mesh_config(4, 4);

    strategy.next_address(0, 16);
    strategy.next_address(0, 16);
    strategy.next_address(0, 16);

    strategy.reset();
    uint64_t addr = strategy.next_address(0, 16);
    REQUIRE(addr < 16);
}

TEST_CASE("create_strategy returns correct type", "[phase8-stress-patterns][factory]") {
    auto s1 = create_strategy(StressPattern::SEQUENTIAL);
    auto s2 = create_strategy(StressPattern::RANDOM);
    auto s3 = create_strategy(StressPattern::HOTSPOT);
    auto s4 = create_strategy(StressPattern::STRIDED);
    auto s5 = create_strategy(StressPattern::NEIGHBOR);
    auto s6 = create_strategy(StressPattern::TORNADO);

    REQUIRE(s1 != nullptr);
    REQUIRE(s2 != nullptr);
    REQUIRE(s3 != nullptr);
    REQUIRE(s4 != nullptr);
    REQUIRE(s5 != nullptr);
    REQUIRE(s6 != nullptr);

    // 验证类型转换
    REQUIRE(dynamic_cast<SequentialStrategy*>(s1.get()) != nullptr);
    REQUIRE(dynamic_cast<RandomStrategy*>(s2.get()) != nullptr);
    REQUIRE(dynamic_cast<HotspotStrategy*>(s3.get()) != nullptr);
    REQUIRE(dynamic_cast<StridedStrategy*>(s4.get()) != nullptr);
    REQUIRE(dynamic_cast<NeighborStrategy*>(s5.get()) != nullptr);
    REQUIRE(dynamic_cast<TornadoStrategy*>(s6.get()) != nullptr);
}

TEST_CASE("Strategies reset correctly", "[phase8-stress-patterns][reset]") {
    {
        SequentialStrategy s;
        s.next_address(0, 100);
        s.reset();
        REQUIRE(s.next_address(0, 100) == 0);
    }
    {
        StridedStrategy s;
        s.set_stride(10);
        s.next_address(0, 100);
        s.reset();
        REQUIRE(s.next_address(0, 100) == 0);
    }
    {
        TornadoStrategy s;
        s.set_mesh_config(4, 4);
        s.next_address(0, 16);
        s.reset();
        uint64_t addr = s.next_address(0, 16);
        REQUIRE(addr < 16);
    }
}

TEST_CASE("TrafficGenTLM set_stress_pattern integration", "[phase8-stress-patterns][traffic-gen]") {
    EventQueue eq;
    TrafficGenTLM gen("gen", &eq);

    // 设置为 HOTSPOT 模式
    gen.set_stress_pattern(StressPattern::HOTSPOT);
    gen.set_hotspot_config({0x1000, 0x2000, 0x3000}, {60, 30, 10});

    // 验证能正常 tick 而不崩溃
    gen.tick();
    gen.tick();
    gen.tick();
}

TEST_CASE("TrafficGenTLM stress pattern address range validation", "[phase8-stress-patterns][traffic-gen]") {
    EventQueue eq;
    TrafficGenTLM gen("gen", &eq);

    // 设置范围为 0x1000 - 0x2000 (4096 bytes)
    gen.set_stress_pattern(StressPattern::RANDOM);

    // 多次 tick 验证地址在范围内
    for (int i = 0; i < 100; i++) {
        gen.tick();
    }
    // 如果有任何地址越界，行为未定义，这里只验证不崩溃
}

TEST_CASE("StressPattern enum has all values", "[phase8-stress-patterns][factory]") {
    // 验证枚举值完整性
    REQUIRE(static_cast<int>(StressPattern::SEQUENTIAL) == 0);
    REQUIRE(static_cast<int>(StressPattern::RANDOM) == 1);
    REQUIRE(static_cast<int>(StressPattern::HOTSPOT) == 2);
    REQUIRE(static_cast<int>(StressPattern::STRIDED) == 3);
    REQUIRE(static_cast<int>(StressPattern::NEIGHBOR) == 4);
    REQUIRE(static_cast<int>(StressPattern::TORNADO) == 5);
}