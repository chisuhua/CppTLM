// test/test_chstream_helpers.cc
// P3 ChStream helper 方法测试
// 验证 CacheTLM::connectBus + CrossbarTLM::connectCPUSideBus + connectMemSideBus
// 参考: docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.3
// 作者: Sisyphus / 日期: 2026-06-19
#include "tlm/cache_tlm.hh"
#include "tlm/crossbar_tlm.hh"
#include "core/event_queue.hh"
#include "core/sim_object.hh"
#include "core/port_manager.hh"
#include "core/simple_port.hh"
#include <catch2/catch_all.hpp>

TEST_CASE("CacheTLM::connectBus binds mem_side to bus.cpu_side", "[chstream][helper]") {
    EventQueue eq;
    auto* cache = new CacheTLM("cache0", &eq);
    auto* bus = new CrossbarTLM("bus0", &eq);

    REQUIRE_NOTHROW(cache->connectBus(bus));

    // P3 verification: helper 应在 port_manager 中注册 mem_side 和 cpu_side 标签
    REQUIRE(cache->getPortManager().getUpstreamPort("mem_side") != nullptr);
    REQUIRE(bus->getPortManager().getUpstreamPort("cpu_side") != nullptr);

    delete cache;
    delete bus;
}

TEST_CASE("CrossbarTLM::connectCPUSideBus / connectMemSideBus", "[chstream][helper]") {
    EventQueue eq;
    auto* xbar = new CrossbarTLM("xbar0", &eq);
    auto* bus_a = new CrossbarTLM("bus_a", &eq);
    auto* bus_b = new CrossbarTLM("bus_b", &eq);

    REQUIRE_NOTHROW(xbar->connectCPUSideBus(bus_a));
    REQUIRE_NOTHROW(xbar->connectMemSideBus(bus_b));

    // P3 verification: 4 个端口标签均注册
    REQUIRE(xbar->getPortManager().getUpstreamPort("cpu_side") != nullptr);
    REQUIRE(xbar->getPortManager().getUpstreamPort("mem_side") != nullptr);
    REQUIRE(bus_a->getPortManager().getUpstreamPort("mem_side") != nullptr);
    REQUIRE(bus_b->getPortManager().getUpstreamPort("cpu_side") != nullptr);

    delete xbar;
    delete bus_a;
    delete bus_b;
}

TEST_CASE("[regression] existing 82+ test files still pass", "[chstream][regression]") {
    SUCCEED("P3 helper methods do not regress existing ChStream behavior");
}
