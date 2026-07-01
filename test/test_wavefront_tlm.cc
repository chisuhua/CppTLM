// test/test_wavefront_tlm.cc
// WavefrontTLM 单元测试
// 作者 CppTLM Team / 日期 2026-06-30
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "modules.hh"
#include "tlm/gpu/wavefront_tlm.hh"
#include <catch2/catch_all.hpp>

using namespace tlm;

namespace {
    void registerChStreamModules() {
        static bool registered = false;
        if (!registered) {
            REGISTER_OBJECT;
            REGISTER_CHSTREAM;
            registered = true;
        }
    }
}

TEST_CASE("WavefrontTLM.Defaults", "[wavefront][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    WavefrontTLM wf("wf0", &eq);

    REQUIRE(wf.get_kernel_id() == 0);
    REQUIRE(wf.get_workgroup_id() == 0);
    REQUIRE(wf.get_warp_id() == 0);
    REQUIRE(wf.get_active_mask() == 0xFFFFFFFFu);
    REQUIRE(wf.get_coalescing_factor() == 1);
    REQUIRE(wf.get_module_type() == "WavefrontTLM");
}

TEST_CASE("WavefrontTLM.Setters", "[wavefront][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    WavefrontTLM wf("wf0", &eq);

    wf.set_kernel_id(7);
    wf.set_workgroup_id(3);
    wf.set_warp_id(5);
    wf.set_active_mask(0x0000FFFFu);
    wf.set_coalescing_factor(4);

    REQUIRE(wf.get_kernel_id() == 7);
    REQUIRE(wf.get_workgroup_id() == 3);
    REQUIRE(wf.get_warp_id() == 5);
    REQUIRE(wf.get_active_mask() == 0x0000FFFFu);
    REQUIRE(wf.get_coalescing_factor() == 4);
}

TEST_CASE("WavefrontTLM.TickNoCrash", "[wavefront][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    WavefrontTLM wf("wf0", &eq);
    wf.tick();
    REQUIRE(true);
}