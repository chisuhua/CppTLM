// test/test_legacy_e2e_simulation.cc
// Legacy module E2E tests — ONLY CPUSim (PortPair model)
// 所有其他模块测试在 test_e2e_simulation.cc 中使用 TLM 模块

#include <catch2/catch_all.hpp>
#include "modules.hh"
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static void registerAllModules() {
    static bool registered = false;
    if (!registered) {
        REGISTER_OBJECT;
        REGISTER_CHSTREAM;
        REGISTER_MODULE;
        registered = true;
    }
}

TEST_CASE("Legacy: CPUSim 实例化与运行", "[legacy][cpu-sim]") {
    registerAllModules();
    json config = R"({
        "modules": [
            {"name": "proc0", "type": "CPUSim"},
            {"name": "proc1", "type": "CPUSim"}
        ],
        "connections": [
            {"src": "proc0", "dst": "proc1", "latency": 1}
        ]
    })"_json;
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(config));
    factory.startAllTicks();
    REQUIRE(factory.getInstance("proc0") != nullptr);
    REQUIRE(factory.getInstance("proc1") != nullptr);
    eq.run(50);
    REQUIRE(eq.getCurrentCycle() == 50);
    SUCCEED("CPUSim legacy module instantiated and ran");
}

TEST_CASE("Legacy: CPUSim 缺少 connections 被拒绝", "[legacy][cpu-sim][validation]") {
    registerAllModules();
    json config = R"({"modules": [{"name": "cpu", "type": "CPUSim"}]})"_json;
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(config));
    SUCCEED("CPUSim missing connections field correctly rejected");
}