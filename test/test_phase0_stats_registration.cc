// test/test_phase0_stats_registration.cc
// Phase 0: 统计注册验证 — 确保 StatGroup 正确注册到 StatsManager
// 功能描述：验证 ModuleFactory::instantiateAll() 自动调用 register_group()
// 作者 CppTLM Team / 日期 2026-05-11
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "metrics/stats_manager.hh"
#include "modules.hh"
#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static void registerChStreamModules() {
    static bool registered = false;
    if (!registered) {
        REGISTER_OBJECT;
        REGISTER_CHSTREAM;
        registered = true;
    }
}

TEST_CASE("Phase 0: StatGroup registration via ModuleFactory", "[phase0][stats]") {
    EventQueue eq;
    registerChStreamModules();

    // 清空 StatsManager（单例，可能有残留）
    tlm_stats::StatsManager::instance().reset_all();

    ModuleFactory factory(&eq);

    json config = R"({
        "modules": [
            {"name": "cache", "type": "CacheTLM"},
            {"name": "mem", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "cache", "dst": "mem", "latency": 1}
        ]
    })"_json;

    REQUIRE(factory.instantiateAll(config));

    // 验证 StatsManager 中存在注册的 group
    auto* cache_group = tlm_stats::StatsManager::instance().find_group("system.cache");
    auto* mem_group = tlm_stats::StatsManager::instance().find_group("system.memory");

    INFO("system.cache should be registered: " << (cache_group ? "yes" : "no"));
    INFO("system.memory should be registered: " << (mem_group ? "yes" : "no"));

    REQUIRE(cache_group != nullptr);
    REQUIRE(mem_group != nullptr);

    // 验证 StatGroup 名称正确
    INFO("Cache group name: " << cache_group->name());
    INFO("Memory group name: " << mem_group->name());
    REQUIRE(cache_group->name() == "cache");
    REQUIRE(mem_group->name() == "memory");

    // 注意：不要调用 clearAllTypes() —— 它会清空全局类型注册表，
    // 破坏后续测试的 registerChStreamModules()（static flag 防重入）
}

TEST_CASE("Phase 0: RouterTLM stats registration", "[phase0][stats][router]") {
    EventQueue eq;
    registerChStreamModules();

    // 清空 StatsManager
    tlm_stats::StatsManager::instance().reset_all();

    ModuleFactory factory(&eq);

    // RouterTLM 使用 BidirectionalPortAdapter，不是 SimObject 端口
    // 所以 connections 为空（不需要 SimObject 风格的端口连接）
    json config = R"({
        "modules": [
            {"name": "router0", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}},
            {"name": "router1", "type": "RouterTLM", "params": {"node_x": 1, "node_y": 0, "mesh_x": 2, "mesh_y": 2}}
        ],
        "connections": []
    })"_json;

    REQUIRE(factory.instantiateAll(config));

    // 验证 RouterTLM 统计组已注册
    // 路径格式：system.router_{node_id}，node_id = node_x + node_y * mesh_x
    auto* router0_group = tlm_stats::StatsManager::instance().find_group("system.router_0");
    auto* router1_group = tlm_stats::StatsManager::instance().find_group("system.router_1");

    INFO("system.router_0 registered: " << (router0_group ? "yes" : "no"));
    INFO("system.router_1 registered: " << (router1_group ? "yes" : "no"));

    REQUIRE(router0_group != nullptr);
    REQUIRE(router1_group != nullptr);

    // 验证 StatGroup 名称正确
    INFO("Router0 group name: " << router0_group->name());
    REQUIRE(router0_group->name() == "router");

    // 注意：不要调用 clearAllTypes() —— 它会清空全局类型注册表，
    // 破坏后续测试的 registerChStreamModules()（static flag 防重入）
}

TEST_CASE("Phase 0: NICTLM stats registration", "[phase0][stats][noc]") {
    EventQueue eq;
    registerChStreamModules();

    // 清空 StatsManager
    tlm_stats::StatsManager::instance().reset_all();

    ModuleFactory factory(&eq);

    json config = R"({
        "modules": [
            {"name": "nic0", "type": "NICTLM", "params": {"node_id": 0, "mesh_x": 4, "mesh_y": 4}},
            {"name": "nic1", "type": "NICTLM", "params": {"node_id": 1, "mesh_x": 4, "mesh_y": 4}}
        ],
        "connections": []
    })"_json;

    REQUIRE(factory.instantiateAll(config));

    // 验证 NICTLM 统计组已注册
    auto* nic0_group = tlm_stats::StatsManager::instance().find_group("system.nic_0");
    auto* nic1_group = tlm_stats::StatsManager::instance().find_group("system.nic_1");

    INFO("system.nic_0 registered: " << (nic0_group ? "yes" : "no"));
    INFO("system.nic_1 registered: " << (nic1_group ? "yes" : "no"));

    REQUIRE(nic0_group != nullptr);
    REQUIRE(nic1_group != nullptr);

    // 验证 StatGroup 名称正确
    INFO("NIC0 group name: " << nic0_group->name());
    REQUIRE(nic0_group->name() == "nic");

    // 注意：不要调用 clearAllTypes() —— 它会清空全局类型注册表，
    // 破坏后续测试的 registerChStreamModules()（static flag 防重入）
}
