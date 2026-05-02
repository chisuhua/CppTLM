// test/test_e2e_simulation.cc
// 端到端仿真测试：覆盖所有已注册模块类型的JSON配置加载、实例化、仿真运行和payload验证
// 标签体系：[e2e][module-type][topology][sim]

#include <catch2/catch_all.hpp>
#include "modules.hh"
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/chstream_adapter_factory.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
using namespace bundles;

// ============================================================================
// 共享基础设施
// ============================================================================

// 一次性注册所有模块类型（static guard 防止重复注册）
static void registerAllModules() {
    static bool registered = false;
    if (!registered) {
        REGISTER_OBJECT;
        REGISTER_CHSTREAM;
        REGISTER_MODULE;
        registered = true;
    }
}

// 从 configs/ 加载 JSON 配置文件
static json loadConfig(const std::string& relative_path) {
    std::string full_path = std::string(CPPTLM_SOURCE_DIR) + "/" + relative_path;
    std::ifstream f(full_path);
    REQUIRE(f.is_open());
    return json::parse(f);
}

// 标准仿真启动：EventQueue → 注册 → 工厂 → 实例化 → startAllTicks
static std::tuple<EventQueue, ModuleFactory>
setupSimulation(const json& config) {
    EventQueue eq;
    registerAllModules();
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(config));
    factory.startAllTicks();
    return {std::move(eq), std::move(factory)};
}

// ============================================================================
// 测试用例
// ============================================================================

TEST_CASE("E2E: 测试基础设施可用", "[e2e][infra]") {
    registerAllModules();
    EventQueue eq;
    REQUIRE(eq.getCurrentCycle() == 0);
    SUCCEED("Infrastructure initialized");
}