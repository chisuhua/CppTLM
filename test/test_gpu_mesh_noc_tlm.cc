// test/test_gpu_mesh_noc_tlm.cc
// GpuMeshNoC mesh XY routing 单元测试 (Catch2 v3.7.0)
// 功能描述: 验证 GpuMeshNoC stub 的 XY 维度序路由 + hops × latency 延迟模型。
// 作者 CppTLM Team / 日期 2026-06-24
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/specs/gpu-soc-phase8a.md
//        §REQ-GPU-8A-3
// Phase 8.A Task 3

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "framework/chstream_adapter_factory.hh"
#include "modules.hh"
#include "tlm/gpu/gpu_mesh_noc_tlm.hh"
#include <catch2/catch_all.hpp>

using namespace tlm;

namespace {
    // 一次性注册所有 ChStream 模块 (避免多 TEST_CASE 重复注册)
    void registerChStreamModules() {
        static bool registered = false;
        if (!registered) {
            REGISTER_OBJECT;
            REGISTER_CHSTREAM;
            registered = true;
        }
    }
}  // namespace

TEST_CASE("GpuMeshNoC_MeshDiagonal_HopsTimesLatency", "[noc][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    GpuMeshNoC noc("noc0", &eq);
    noc.set_hops_latency(2);

    // 2x2 mesh: (0,0) → (1,1) 是 XY 路由 2 hops
    // latency = (|dx|+|dy|) × hops_latency = (1+1) × 2 = 4
    REQUIRE(noc.route_latency({0, 0}, {1, 1}) == 4);
}

TEST_CASE("GpuMeshNoC_SamePoint_ZeroLatency", "[noc][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    GpuMeshNoC noc("noc0", &eq);
    noc.set_hops_latency(2);

    // 同点 → dx=0, dy=0 → 0 cycles
    REQUIRE(noc.route_latency({1, 1}, {1, 1}) == 0);
}

TEST_CASE("GpuMeshNoC_AdjacentX_OneHop", "[noc][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    GpuMeshNoC noc("noc0", &eq);
    noc.set_hops_latency(1);

    // X 方向相邻 → 1 hop × 1 cyc = 1
    REQUIRE(noc.route_latency({0, 0}, {1, 0}) == 1);
}

TEST_CASE("GpuMeshNoC_AdjacentY_OneHop", "[noc][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    GpuMeshNoC noc("noc0", &eq);
    noc.set_hops_latency(1);

    // Y 方向相邻 → 1 hop × 1 cyc = 1
    REQUIRE(noc.route_latency({0, 0}, {0, 1}) == 1);
}

TEST_CASE("GpuMeshNoC_ReverseDirection_SameHopCount", "[noc][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    GpuMeshNoC noc("noc0", &eq);
    noc.set_hops_latency(1);

    // XY 路由 hops 数与方向无关: (1,1)→(0,0) 也是 2 hops
    REQUIRE(noc.route_latency({1, 1}, {0, 0}) == 2);
}

TEST_CASE("GpuMeshNoC_DefaultDimAndHopsLatency", "[noc][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    GpuMeshNoC noc2("noc1", &eq);

    // 默认值: dim=2, hops_latency=2 (per stub header)
    REQUIRE(noc2.get_dim() == 2);
    REQUIRE(noc2.get_hops_latency() == 2);
}

TEST_CASE("GpuMeshNoC_SetterInjection", "[noc][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    GpuMeshNoC noc("noc0", &eq);

    // JSON 解析后注入: dim=4, hops_latency=3
    noc.set_dim(4);
    noc.set_hops_latency(3);

    REQUIRE(noc.get_dim() == 4);
    REQUIRE(noc.get_hops_latency() == 3);

    // setter 后重新验证 route_latency 行为:
    // 4x4 mesh 中 (0,0) → (3,3) = 6 hops × 3 cyc = 18
    REQUIRE(noc.route_latency({0, 0}, {3, 3}) == 18);
}

TEST_CASE("GpuMeshNoC_GetModuleType_RenamedFromGpuNoC", "[noc][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    GpuMeshNoC noc("noc0", &eq);

    // 类已重命名为 GpuMeshNoC, 避免与 cluster/gpu_noc_cluster.hh 中
    // 已有 GpuNoC (SimModule) 类冲突
    REQUIRE(noc.get_module_type() == "GpuMeshNoC");
}

TEST_CASE("GpuMeshNoC_Tick_IncrementsCycleCounter", "[noc][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    GpuMeshNoC noc("noc0", &eq);

    // tick() 增加 cycle_counter_ (Smoke test)
    for (int i = 0; i < 5; ++i) {
        noc.tick();
    }
    // cycle_counter_ 是 private, 但 tick 不应崩溃 — 间接验证
    SUCCEED("GpuMeshNoC::tick() 连续执行 5 次未崩溃");
}

TEST_CASE("GpuMeshNoC_NullEventQueue_Constructible", "[noc][gpu][phase8a]") {
    registerChStreamModules();
    // EventQueue* 允许传 nullptr (stub 模式, 不实际调度)
    GpuMeshNoC noc("noc0", nullptr);
    noc.set_hops_latency(2);

    REQUIRE(noc.route_latency({0, 0}, {1, 1}) == 4);
    REQUIRE(noc.get_module_type() == "GpuMeshNoC");
}