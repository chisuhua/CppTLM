// test/test_gpu_soc_phase8a.cc
// Phase 8.A 端到端集成测试
// 验证 GpuComputeUnitTLM 请求完成后端到端闭环
// 作者: CppTLM Team / 日期: 2026-07-02
// Phase 8.A Task 7
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "modules.hh"
#include "tlm/gpu/gpu_compute_unit_tlm.hh"
#include "tlm/gpu/gpu_mesh_noc_tlm.hh"
#include "tlm/gpu/memory_cluster_tlm.hh"
#include "tlm/gpu/shared_memory_tlm.hh"
#include "tlm/gpu/wavefront_tlm.hh"
#include <catch2/catch_all.hpp>

using namespace tlm;

namespace {
    void registerModules() {
        static bool done = false;
        if (!done) {
            REGISTER_OBJECT;
            REGISTER_CHSTREAM;
            done = true;
        }
    }
} // namespace

TEST_CASE("phase8a: GpuComputeUnit dispatches and completes requests", "[gpu][soc][phase8a]") {
    registerModules();
    EventQueue eq;
    GpuComputeUnitTLM cu("cu0", &eq);
    cu.set_execution_latency(1);

    // 派发 1 个 wavefront
    WavefrontTLM wf("wf0", &eq);
    wf.set_kernel_id(1);
    wf.set_workgroup_id(0);
    wf.set_warp_id(0);
    cu.dispatch_wavefront(&wf);

    REQUIRE(cu.get_warps_dispatched() == 1);
    REQUIRE(cu.get_requests_completed() == 0);

    // 跑 cycle: dispatch → execute → complete
    cu.tick();
    cu.tick();
    REQUIRE(cu.get_requests_completed() == 1);
}

TEST_CASE("phase8a: 4-SM parallel execution completes all warps", "[gpu][soc][phase8a][multism]") {
    registerModules();
    EventQueue eq;
    GpuComputeUnitTLM cu("cu0", &eq);
    cu.set_execution_latency(1);

    // 派发 4 个 wavefront 到 4 sub-core
    for (uint32_t i = 0; i < 4; ++i) {
        WavefrontTLM wf("wf" + std::to_string(i), &eq);
        wf.set_warp_id(i);
        cu.dispatch_wavefront(&wf);
    }

    REQUIRE(cu.get_warps_dispatched() == 4);

    cu.tick(); // dispatch 4 warps to 4 sub-cores (1+N model: dispatch = 1 cycle)
    REQUIRE(cu.get_requests_completed() == 0);
    cu.tick(); // 4 warps execute in parallel (1 cycle latency) and complete
    REQUIRE(cu.get_requests_completed() == 4);
}