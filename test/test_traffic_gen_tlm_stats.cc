// test/test_traffic_gen_tlm_stats.cc
// TrafficGenTLM 压力测试与性能指标收集
// 标签: [phase8][traffic-gen][stats]

#include "catch_amalgamated.hpp"
#include "tlm/traffic_gen_tlm.hh"
#include "core/event_queue.hh"

#include <sstream>

using namespace tlm_stats;

TEST_CASE("TrafficGenTLM: stats collection basic", "[phase8][traffic-gen][stats]") {
    EventQueue eq;
    TrafficGenTLM gen("gen", &eq);
    gen.set_num_requests(100);

    for (int i = 0; i < 1000; ++i) {
        gen.tick();
    }

    auto& s = gen.stats();
    auto& issued = static_cast<Scalar&>(*s.stats().at("requests_issued"));
    REQUIRE(issued.value() > 0);
}

TEST_CASE("TrafficGenTLM: sequential mode stats", "[phase8][traffic-gen][stats]") {
    EventQueue eq;
    TrafficGenTLM gen("gen", &eq);
    gen.set_mode(GenMode_TLM::SEQUENTIAL);
    gen.set_num_requests(50);

    for (int i = 0; i < 500; ++i) {
        gen.tick();
    }

    REQUIRE(gen.stats().stats().at("requests_issued") != nullptr);
    REQUIRE(gen.stats().stats().at("reads") != nullptr);
    REQUIRE(gen.stats().stats().at("writes") != nullptr);
}

TEST_CASE("TrafficGenTLM: random mode stats", "[phase8][traffic-gen][stats]") {
    EventQueue eq;
    TrafficGenTLM gen("gen", &eq);
    gen.set_mode(GenMode_TLM::RANDOM);
    gen.set_num_requests(50);

    for (int i = 0; i < 500; ++i) {
        gen.tick();
    }

    auto& s = gen.stats();
    REQUIRE(s.stats().at("requests_issued") != nullptr);
}

TEST_CASE("TrafficGenTLM: stats dump output", "[phase8][traffic-gen][stats]") {
    EventQueue eq;
    TrafficGenTLM gen("gen", &eq);
    gen.set_mode(GenMode_TLM::SEQUENTIAL);
    gen.set_num_requests(20);

    for (int i = 0; i < 200; ++i) {
        gen.tick();
    }

    std::ostringstream oss;
    gen.dumpStats(oss);
    std::string output = oss.str();

    REQUIRE(output.find("gen.requests_issued") != std::string::npos);
    REQUIRE(output.find("gen.latency.count") != std::string::npos);
}

TEST_CASE("TrafficGenTLM: stats reset", "[phase8][traffic-gen][stats]") {
    EventQueue eq;
    TrafficGenTLM gen("gen", &eq);
    gen.set_num_requests(100);

    for (int i = 0; i < 500; ++i) {
        gen.tick();
    }

    ResetConfig config;
    gen.reset(config);

    auto& s = gen.stats();
    auto& latency = static_cast<Distribution&>(*s.stats().at("latency"));
    REQUIRE(latency.count() == 0);
}

TEST_CASE("TrafficGenTLM: stress test - 10K requests", "[phase8][traffic-gen][stats][stress]") {
    EventQueue eq;
    TrafficGenTLM gen("gen", &eq);
    gen.set_mode(GenMode_TLM::RANDOM);
    gen.set_num_requests(10000);

    for (int i = 0; i < 50000; ++i) {
        gen.tick();
    }

    auto& s = gen.stats();
    auto& issued = static_cast<Scalar&>(*s.stats().at("requests_issued"));
    auto& reads = static_cast<Scalar&>(*s.stats().at("reads"));
    auto& writes = static_cast<Scalar&>(*s.stats().at("writes"));

    // issued < num_requests due to random issuance (10% chance per tick)
    REQUIRE(issued.value() > 1000);
    // reads + writes should equal total issued
    REQUIRE(reads.value() + writes.value() == issued.value());
}

TEST_CASE("TrafficGenTLM: read/write ratio", "[phase8][traffic-gen][stats]") {
    EventQueue eq;
    TrafficGenTLM gen("gen", &eq);
    gen.set_mode(GenMode_TLM::RANDOM);
    gen.set_num_requests(1000);

    for (int i = 0; i < 10000; ++i) {
        gen.tick();
    }

    auto& s = gen.stats();
    auto& reads = static_cast<Scalar&>(*s.stats().at("reads"));
    auto& writes = static_cast<Scalar&>(*s.stats().at("writes"));
    auto& issued = static_cast<Scalar&>(*s.stats().at("requests_issued"));

    uint64_t total = reads.value() + writes.value();
    REQUIRE(total == issued.value());
}

TEST_CASE("TrafficGenTLM: latency distribution", "[phase8][traffic-gen][stats]") {
    EventQueue eq;
    TrafficGenTLM gen("gen", &eq);
    gen.set_mode(GenMode_TLM::SEQUENTIAL);
    gen.set_num_requests(100);

    for (int i = 0; i < 2000; ++i) {
        gen.tick();
    }

    auto& s = gen.stats();
    auto& issued = static_cast<Scalar&>(*s.stats().at("requests_issued"));
    REQUIRE(issued.value() > 0);

    // Latency requires completed responses - verify structure exists
    REQUIRE(s.stats().at("latency") != nullptr);
}
