// test/test_gpu_compute_unit_tlm.cc
// GpuComputeUnitTLM 单元测试
// 作者 CppTLM Team / 日期 2026-06-30
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "modules.hh"
#include "tlm/gpu/gpu_compute_unit_tlm.hh"
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
} // namespace

TEST_CASE("GpuComputeUnitTLM.Defaults", "[compute_unit][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    GpuComputeUnitTLM cu("cu0", &eq);

    REQUIRE(cu.get_module_type() == "GpuComputeUnitTLM");
    REQUIRE(cu.get_num_subcores() == 4);
    REQUIRE(cu.get_requests_completed() == 0);
    REQUIRE(cu.get_warps_dispatched() == 0);
}

TEST_CASE("GpuComputeUnitTLM.Setters", "[compute_unit][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    GpuComputeUnitTLM cu("cu0", &eq);

    cu.set_num_subcores(8);
    cu.set_execution_latency(5);

    REQUIRE(cu.get_num_subcores() == 8);
    REQUIRE(cu.get_execution_latency() == 5);
}

TEST_CASE("GpuComputeUnitTLM.DispatchWavefront_IncrementsCounters",
          "[compute_unit][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    GpuComputeUnitTLM cu("cu0", &eq);

    WavefrontTLM wf("wf0", &eq);
    wf.set_kernel_id(1);
    wf.set_workgroup_id(2);
    wf.set_warp_id(3);

    cu.dispatch_wavefront(&wf);

    REQUIRE(cu.get_warps_dispatched() == 1);
}

TEST_CASE("GpuComputeUnitTLM.Tick_CompletesRequest", "[compute_unit][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    GpuComputeUnitTLM cu("cu0", &eq);
    cu.set_execution_latency(2);

    WavefrontTLM wf("wf0", &eq);
    wf.set_kernel_id(1);
    wf.set_workgroup_id(2);
    wf.set_warp_id(3);

    cu.dispatch_wavefront(&wf);

    cu.tick(); // dispatch to subcore
    cu.tick(); // remaining 2
    REQUIRE(cu.get_requests_completed() == 0);
    cu.tick(); // complete
    REQUIRE(cu.get_requests_completed() == 1);
}

TEST_CASE("GpuComputeUnitTLM.Tick_FourSubCoresParallel", "[compute_unit][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    GpuComputeUnitTLM cu("cu0", &eq);
    cu.set_execution_latency(1);

    for (uint32_t i = 0; i < 4; ++i) {
        WavefrontTLM wf("wf" + std::to_string(i), &eq);
        wf.set_warp_id(i);
        cu.dispatch_wavefront(&wf);
    }

    cu.tick(); // 4 warps dispatched in parallel to 4 subcores
    REQUIRE(cu.get_requests_completed() == 0);
    cu.tick(); // 4 warps complete in parallel
    REQUIRE(cu.get_requests_completed() == 4);
}
