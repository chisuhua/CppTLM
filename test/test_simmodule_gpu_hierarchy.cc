// test/test_simmodule_gpu_hierarchy.cc
// GPU 4 类集成测试
// 验证 ComputeCluster / TpcCluster / GpcCluster / GpuCluster
// 参考: docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §7.2
// 作者: Sisyphus / 日期: 2026-06-19
#include "chstream_register.hh"
#include "core/module_factory.hh"
#include "core/sim_module.hh"
#include "modules.hh"         // P2-T2.4: 提供 REGISTER_MODULE 宏 (modules_cluster.hh 依赖)
#include "modules_cluster.hh" // 5 个 REGISTER_MODULE
#include "tlm/cluster/compute_cluster.hh"
#include "tlm/cluster/gpc_cluster.hh"
#include "tlm/cluster/gpu_cluster.hh"
#include "tlm/cluster/tpc_cluster.hh"
#include "utils/json_includer.hh"
#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

TEST_CASE("ComputeCluster loads cu_template and instantiates N copies", "[simmodule][gpu]") {
    EventQueue eq;
    auto* cluster = new ComputeCluster("compute_grp", &eq);
    json params = {{"cu_template", "configs/templates/compute_unit_v1.json"}, {"cu_count", 4}};
    cluster->set_config(params);
    cluster->simulate_instantiate({});

    REQUIRE(cluster->getInternalInstance("cu0") != nullptr);
    REQUIRE(cluster->getInternalInstance("cu3") != nullptr);
    auto* cu0 = dynamic_cast<SimModule*>(cluster->getInternalInstance("cu0"));
    REQUIRE(cu0 != nullptr);
    REQUIRE(cu0->getInternalFactory().getAllInstances().size() == 2);
    delete cluster;
}

TEST_CASE("TpcCluster contains 1 ComputeCluster with 2 CUs", "[simmodule][gpu]") {
    EventQueue eq;
    auto* tpc = new TpcCluster("tpc0", &eq);
    json params = {{"tpc_id", 0},
                   {"cu_per_tpc", 2},
                   {"cu_template", "configs/templates/compute_unit_v1.json"}};
    tpc->set_config(params);
    tpc->simulate_instantiate({});
    REQUIRE(tpc->getInternalInstance("compute_grp") != nullptr);
    delete tpc;
}

TEST_CASE("GpcCluster contains M TpcClusters", "[simmodule][gpu]") {
    EventQueue eq;
    auto* gpc = new GpcCluster("gpc0", &eq);
    json params = {{"gpc_id", 0},
                   {"tpc_per_gpc", 2},
                   {"cu_per_tpc", 2},
                   {"cu_template", "configs/templates/compute_unit_v1.json"}};
    gpc->set_config(params);
    gpc->simulate_instantiate({});
    REQUIRE(gpc->getInternalInstance("tpc0") != nullptr);
    REQUIRE(gpc->getInternalInstance("tpc1") != nullptr);
    delete gpc;
}

TEST_CASE("GpuCluster full APU-2GPC-2TPC-2CU runs E2E", "[simmodule][gpu][e2e]") {
    // 使用 JSON config 端到端测试
    const std::string config_path = "configs/gpu_2gpc_2tpc_2cu.json";
    auto config = JsonIncluder::loadAndInclude(config_path);
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_NOTHROW(factory.instantiateAll(config));
    auto* gpu = dynamic_cast<SimModule*>(factory.getInstance("gpu"));
    REQUIRE(gpu != nullptr);
    // 验证 2 GPC × 2 TPC × 2 CU × 2 子 = 16 个 leaf module
    int total = 0;
    std::function<void(SimModule*)> count = [&](SimModule* m) {
        for (auto& [n, obj] : m->getInternalFactory().getAllInstances()) {
            if (auto* sub = dynamic_cast<SimModule*>(obj))
                count(sub);
            else
                ++total;
        }
    };
    count(gpu);
    REQUIRE(total == 2 * 2 * 2 * 2); // 16
}
