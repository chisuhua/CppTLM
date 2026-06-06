// test/test_cppHDL_smoke.cc
// CppHDL 集成 Smoke 测试（Catch2 v3）
// 功能描述：验证 CppHDL ch_device + Simulator + 端口读写在 Spike 范围内的
//           可编译性 + 1 周期 tick 安全性。覆盖三项基础能力：
//             1. 设备实例化（create_ports + describe 不抛异常）
//             2. 仿真器 1 周期 tick（无崩溃、无未初始化寄存器）
//             3. 端口读写（set_value 写 ch_in，get_value 读 ch_out）
// 作者 CppTLM Team
// 日期 2026-06-06

#include <catch2/catch_all.hpp>
#include "ch.hpp"
#include "simulator.h"
#include "rtl/hybrid_cache_component.hh"

using cpptlm::rtl::HybridCacheComponent;
using namespace ch::core;

TEST_CASE("CppHDL device instantiation", "[cpphdl][smoke]") {
    REQUIRE_NOTHROW([]() {
        ch::ch_device<HybridCacheComponent> device;
        (void)device.instance();
    }());
}

TEST_CASE("CppHDL simulator tick (1 cycle)", "[cpphdl][smoke]") {
    ch::ch_device<HybridCacheComponent> device;
    ch::Simulator                       sim(device.context(), false);

    REQUIRE_NOTHROW(sim.reset());
    REQUIRE_NOTHROW(sim.tick());
}

TEST_CASE("CppHDL port access", "[cpphdl][smoke]") {
    ch::ch_device<HybridCacheComponent> device;
    ch::Simulator                       sim(device.context(), false);

    auto& comp = device.instance();

    REQUIRE_NOTHROW(sim.set_value(comp.req_addr_, 0xDEADBEEFu));
    REQUIRE_NOTHROW(sim.set_value(comp.req_tid_,  0x1u));
    REQUIRE_NOTHROW(sim.set_value(comp.req_valid_, 0u));
    REQUIRE_NOTHROW(sim.set_value(comp.resp_ready_, 1u));

    REQUIRE_NOTHROW(sim.reset());
    REQUIRE_NOTHROW(sim.tick());

    (void)sim.get_value(comp.resp_tid_);
    (void)sim.get_value(comp.resp_data_);
    (void)sim.get_value(comp.resp_hit_);
    (void)sim.get_value(comp.resp_valid_);
    SUCCEED("port read/write + 1 cycle tick completed without exception");
}
