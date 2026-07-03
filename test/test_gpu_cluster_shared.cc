// test/test_gpu_cluster_shared.cc
// GpuClusterSharedInterface 单元测试
// 验证 GpuCluster 正确实现抽象接口 + dynamic_cast 转换 + apu_soc 兼容性
// 作者: CppTLM Team / 日期: 2026-07-02
// Phase 8.A Task 5
#include "core/event_queue.hh"
#include "tlm/cluster/gpu_cluster.hh"
#include "tlm/gpu/gpu_cluster_shared_interface.hh"
#include <catch2/catch_all.hpp>

using namespace cpptlm::tlm;

TEST_CASE("GpuCluster implements GpuClusterSharedInterface", "[cluster_shared][gpu][phase8a]") {
    EventQueue eq;
    GpuCluster cluster("test_cluster", &eq);

    // dynamic_cast 应成功
    GpuClusterSharedInterface* iface = dynamic_cast<GpuClusterSharedInterface*>(&cluster);
    REQUIRE(iface != nullptr);
}

TEST_CASE("GpuCluster: topology setter/getter round-trip", "[cluster_shared][gpu][phase8a]") {
    EventQueue eq;
    GpuCluster cluster("test_cluster", &eq);

    // 默认值
    GpuTopology default_topo = cluster.get_gpu_topology();
    REQUIRE(default_topo.num_gpc == 1);
    REQUIRE(default_topo.num_tpc_per_gpc == 1);
    REQUIRE(default_topo.num_sm_per_tpc == 1);
    REQUIRE(default_topo.num_subcore_per_sm == 4);
    REQUIRE(default_topo.warp_size == 32);

    // 设置新值
    GpuTopology custom_topo;
    custom_topo.num_gpc = 9;
    custom_topo.num_tpc_per_gpc = 6;
    custom_topo.num_sm_per_tpc = 2;
    custom_topo.num_subcore_per_sm = 4;
    custom_topo.warp_size = 32;
    cluster.set_gpu_topology(custom_topo);

    // 读取验证
    GpuTopology read_back = cluster.get_gpu_topology();
    REQUIRE(read_back.num_gpc == 9);
    REQUIRE(read_back.num_tpc_per_gpc == 6);
    REQUIRE(read_back.num_sm_per_tpc == 2);
    REQUIRE(read_back.num_subcore_per_sm == 4);
    REQUIRE(read_back.warp_size == 32);
}

TEST_CASE("GpuCluster: get_module_type unchanged", "[cluster_shared][gpu][phase8a]") {
    EventQueue eq;
    GpuCluster cluster("test_cluster", &eq);

    REQUIRE(cluster.get_module_type() == "GpuCluster");
}

TEST_CASE("GpuCluster: multiple inheritance does not break SimModule identity",
          "[cluster_shared][gpu][phase8a]") {
    EventQueue eq;
    GpuCluster cluster("test_cluster", &eq);

    // 仍然可以作为 SimModule 使用（现有 apu_soc 代码依赖此转换）
    SimModule* sim_mod = dynamic_cast<SimModule*>(&cluster);
    REQUIRE(sim_mod != nullptr);
    REQUIRE(sim_mod->get_module_type() == "GpuCluster");
}