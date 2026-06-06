// test/test_cppHDL_smoke.cc
// CppHDL 集成 Smoke 测试（Catch2 v3）
// 功能描述：验证 CppHDL ch_device<HybridCacheComponent> + Simulator 工作
//           使用 ch_stream<CacheReqBundleRTL/RespBundleRTL> 接口
//           (canonical pattern 与 example_rtl_modules.md 一致)
// 标签: [cpphdl][smoke]
// 作者 CppTLM Team / 日期 2026-06-07
#include <catch2/catch_all.hpp>
#include "ch.hpp"
#include "chlib/stream.h"
#include "simulator.h"
#include "rtl/hybrid_cache_component.hh"
#include "bundles/cache_bundles_rtl.hh"

using cpptlm::rtl::HybridCacheComponent;

TEST_CASE("CppHDL ch_stream device instantiation", "[cpphdl][smoke]") {
    REQUIRE_NOTHROW([]() {
        ch::ch_device<HybridCacheComponent> device;
        (void)device.instance();
    }());
}

TEST_CASE("CppHDL ch_stream simulator tick (1 cycle)", "[cpphdl][smoke]") {
    ch::ch_device<HybridCacheComponent> device;
    ch::Simulator sim(device.context(), false);

    REQUIRE_NOTHROW(sim.reset());
    REQUIRE_NOTHROW(sim.tick());
}

TEST_CASE("CppHDL ch_stream port access (valid/ready/payload)", "[cpphdl][smoke]") {
    ch::ch_device<HybridCacheComponent> device;
    ch::Simulator sim(device.context(), false);

    auto& io = device.instance().io();

    REQUIRE_NOTHROW(sim.set_value(io.req_in.payload.transaction_id, 42ULL));
    REQUIRE_NOTHROW(sim.set_value(io.req_in.payload.address, 0xDEADBEEFULL));
    REQUIRE_NOTHROW(sim.set_value(io.req_in.valid, 1u));
    REQUIRE_NOTHROW(sim.set_value(io.resp_out.ready, 1u));

    REQUIRE_NOTHROW(sim.reset());
    REQUIRE_NOTHROW(sim.tick());

    REQUIRE_NOTHROW((void)sim.get_value(io.resp_out.payload.transaction_id));
    REQUIRE_NOTHROW((void)sim.get_value(io.resp_out.payload.data));
    REQUIRE_NOTHROW((void)sim.get_value(io.resp_out.payload.is_hit));
    REQUIRE_NOTHROW((void)sim.get_value(io.resp_out.valid));

    SUCCEED("ch_stream<BundleT> access + 1 cycle tick completed");
}
