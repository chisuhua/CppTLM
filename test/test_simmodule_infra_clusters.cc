// test/test_simmodule_infra_clusters.cc
// P4 基础设施 3 类集成测试
// 验证 CacheCluster + MemoryCluster + GpuNoC
// 参考: docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §7.4
// 作者: Sisyphus / 日期: 2026-06-19
#include "chstream_register.hh"
#include "core/module_factory.hh"
#include "core/sim_module.hh"
#include "modules.hh"
#include "modules_cluster.hh"
#include "tlm/cluster/cache_cluster.hh"
#include "tlm/cluster/gpu_noc_cluster.hh"
#include "tlm/cluster/memory_cluster.hh"
#include "utils/json_includer.hh"
#include <catch2/catch_all.hpp>

using json = nlohmann::json;

namespace {
    auto _register_infra = []() {
        ModuleFactory::registerObject<MemoryTLM>("MemoryTLM");
        ModuleFactory::registerObject<tlm::RouterTLM>("RouterTLM");
        ModuleFactory::registerObject<ArbiterTLM<2>>("ArbiterTLM2");
        ModuleFactory::registerObject<ArbiterTLM<4>>("ArbiterTLM4");
        return 0;
    }();
} // namespace

TEST_CASE("CacheCluster auto-generates L1xN + L2", "[simmodule][infra]") {
    EventQueue eq;
    auto* cluster = new CacheCluster("cc0", &eq);
    json params = {{"l1_count", 2}, {"l1_size", "16KB"}, {"l2_size", "256KB"}};
    cluster->set_config(params);
    cluster->simulate_instantiate({});
    REQUIRE(cluster->getInternalFactory().getAllInstances().size() == 3);
    REQUIRE(cluster->getInternalInstance("l2") != nullptr);
    REQUIRE(cluster->getInternalInstance("l1_0") != nullptr);
    REQUIRE(cluster->getInternalInstance("l1_1") != nullptr);
    delete cluster;
}

TEST_CASE("MemoryCluster multi-channel", "[simmodule][infra]") {
    EventQueue eq;
    auto* mc = new MemoryCluster("mc0", &eq);
    json params = {{"channel_count", 4}, {"channel_size", "1GB"}, {"memory_type", "HBM"}};
    mc->set_config(params);
    mc->simulate_instantiate({});
    REQUIRE(mc->getInternalFactory().getAllInstances().size() == 5);
    REQUIRE(mc->getInternalInstance("arbiter") != nullptr);
    delete mc;
}

TEST_CASE("GpuNoC mesh 2x2 with 4 routers", "[simmodule][infra]") {
    EventQueue eq;
    auto* noc = new GpuNoC("noc0", &eq);
    json params = {{"mesh_size", 2}, {"routing", "XY"}};
    noc->set_config(params);
    noc->simulate_instantiate({});
    REQUIRE(noc->getInternalFactory().getAllInstances().size() == 4);
    REQUIRE(noc->getInternalInstance("router_0") != nullptr);
    REQUIRE(noc->getInternalInstance("router_3") != nullptr);
    delete noc;
}
