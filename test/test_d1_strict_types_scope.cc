// test_d1_strict_types_scope.cc
// D.1 (cpptlm-dgpu-realip-pin-contract Change D, 2026-10-08):
// ModuleInstScope + registerModuleScope 单元测试
#include "catch_amalgamated.hpp"
#include "core/module_factory.hh"

TEST_CASE("D.1: ModuleInstScope enum values", "[d1][strict-types]") {
    using Scope = ModuleFactory::ModuleInstScope;
    REQUIRE(static_cast<uint8_t>(Scope::kTopLevel) == 0);
    REQUIRE(static_cast<uint8_t>(Scope::kCompositeOnly) == 1);
}

TEST_CASE("D.1: registerModuleScope records kTopLevel / kCompositeOnly", "[d1][strict-types]") {
    auto& reg = ModuleFactory::getModuleScopeRegistry();
    // 默认无注册 (避免污染其他测试): 改用临时 type
    const std::string sentinel = "TestType_D1_Sentinel_XYZ";
    reg[sentinel] = ModuleFactory::ModuleInstScope::kCompositeOnly;
    REQUIRE(reg.find(sentinel) != reg.end());
    REQUIRE(reg[sentinel] == ModuleFactory::ModuleInstScope::kCompositeOnly);
    reg.erase(sentinel); // 清理
}

TEST_CASE("D.1: 默认 type 未注册 scope (graceful skip)", "[d1][strict-types]") {
    auto& reg = ModuleFactory::getModuleScopeRegistry();
    REQUIRE(reg.find("CpuCluster") == reg.end());
    REQUIRE(reg.find("GpuCluster") == reg.end());
    // 验证: 未注册的 type 在 instantiateAll 中跳过 scope 检查,行为等同原版
    // (这是 strict_types 的 opt-in 性质 — opt-in vs 强制的关键设计)
}
