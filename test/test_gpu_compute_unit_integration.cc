// test/test_gpu_compute_unit_integration.cc
// GpuComputeUnitTLM 简化集成测试：验证多 wavefront dispatch + tick 后 requests_completed > 0
// 作者 CppTLM Team / 日期 2026-06-30
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "modules.hh"
#include "tlm/gpu/gpu_compute_unit_tlm.hh"
#include "tlm/gpu/wavefront_tlm.hh"
#include <catch2/catch_all.hpp>

using namespace tlm;

namespace {
    void registerModules() {
        static bool registered = false;
        if (!registered) {
            REGISTER_OBJECT;
            REGISTER_CHSTREAM;
            registered = true;
        }
    }
} // namespace

TEST_CASE("GpuComputeUnitTLM.Integration_RequestsCompleted",
          "[compute_unit][gpu][phase7b][integration]") {
    registerModules();
    EventQueue eq;

    GpuComputeUnitTLM cu("cu0", &eq);
    cu.set_execution_latency(1);

    for (uint32_t i = 0; i < 4; ++i) {
        WavefrontTLM wf("wf" + std::to_string(i), &eq);
        wf.set_kernel_id(1);
        wf.set_workgroup_id(0);
        wf.set_warp_id(i);
        cu.dispatch_wavefront(&wf);
    }

    // 跑 10 个 cycle 让所有 warp 完成
    for (int i = 0; i < 10; ++i) {
        cu.tick();
    }

    REQUIRE(cu.get_requests_completed() > 0);
    REQUIRE(cu.get_requests_completed() == 4);
}