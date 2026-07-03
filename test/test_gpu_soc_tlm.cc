// test/test_gpu_soc_tlm.cc
// GpuSocTLM 单元测试
// 验证 GpuSocTLM 构造 + 4 个 setter/getter + tick() 推进子模块
// 作者: CppTLM Team / 日期: 2026-07-02
// Phase 8.A Task 6
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "modules.hh"
#include "tlm/cluster/gpu_cluster.hh"
#include "tlm/gpu/gpu_mesh_noc_tlm.hh"
#include "tlm/gpu/gpu_soc_tlm.hh"
#include "tlm/gpu/kernel_launch_tlm.hh"
#include "tlm/gpu/memory_cluster_tlm.hh"
#include <catch2/catch_all.hpp>

using namespace tlm;
using namespace cpptlm::tlm;

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

TEST_CASE("GpuSocTLM.ConstructorDefaults", "[gpu][soc][phase8a]") {
    registerModules();
    EventQueue eq;
    GpuSocTLM soc("soc", &eq);

    REQUIRE(soc.get_module_type() == "GpuSocTLM");
    REQUIRE(soc.get_gpu_cluster() == nullptr);
    REQUIRE(soc.get_noc() == nullptr);
    REQUIRE(soc.get_memory_cluster() == nullptr);
    REQUIRE(soc.get_kernel_launch() == nullptr);
}

TEST_CASE("GpuSocTLM.SetterGetter", "[gpu][soc][phase8a]") {
    registerModules();
    EventQueue eq;

    GpuSocTLM soc("soc", &eq);
    GpuCluster cluster("cluster", &eq);
    GpuMeshNoC noc("noc", &eq);
    MemoryClusterTLM mem("mem", &eq);
    KernelLaunchTLM kl("kl", &eq);

    soc.set_gpu_cluster(&cluster);
    soc.set_noc(&noc);
    soc.set_memory_cluster(&mem);
    soc.set_kernel_launch(&kl);

    REQUIRE(soc.get_gpu_cluster() == &cluster);
    REQUIRE(soc.get_noc() == &noc);
    REQUIRE(soc.get_memory_cluster() == &mem);
    REQUIRE(soc.get_kernel_launch() == &kl);
}

TEST_CASE("GpuSocTLM.TickNoCrash", "[gpu][soc][phase8a]") {
    registerModules();
    EventQueue eq;

    GpuSocTLM soc("soc", &eq);
    GpuCluster cluster("cluster", &eq);
    GpuMeshNoC noc("noc", &eq);
    MemoryClusterTLM mem("mem", &eq);
    KernelLaunchTLM kl("kl", &eq);

    soc.set_gpu_cluster(&cluster);
    soc.set_noc(&noc);
    soc.set_memory_cluster(&mem);
    soc.set_kernel_launch(&kl);

    // tick 不崩溃
    for (int i = 0; i < 10; ++i) {
        soc.tick();
    }
    REQUIRE(true);
}