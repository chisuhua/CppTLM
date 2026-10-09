// test_d3_param_resolver_registry.cc
// D.3 (cpptlm-dgpu-realip-pin-contract Change D, 2026-10-08):
// ParamRules 注册表 单元测试 — 验证 ModuleFactory getParamRulesRegistry /
// registerParamRules 工作正常, RouterTLM 自注册后可通过 type 查询。
#include "catch_amalgamated.hpp"
#include "core/module_factory.hh"
#include "core/param_rules.hh"

#include <string>
#include <vector>

using cpptlm::ParamRule;
using cpptlm::ParamRules;

TEST_CASE("D.3: ModuleFactory::getParamRulesRegistry is accessible", "[d3][param-resolver]") {
    auto& registry = ModuleFactory::getParamRulesRegistry();
    REQUIRE(registry.find("RouterTLM") != registry.end());
}

TEST_CASE("D.3: RouterTLM rules contain required node_x + node_y + defaults",
          "[d3][param-resolver]") {
    auto& registry = ModuleFactory::getParamRulesRegistry();
    auto it = registry.find("RouterTLM");
    REQUIRE(it != registry.end());

    ParamRules rules = it->second();
    REQUIRE(!rules.empty());

    // node_x 与 node_y 为 required,无 default
    bool found_node_x = false, found_node_y = false;
    int default_count = 0;
    for (const auto& [name, rule] : rules) {
        if (name == "node_x") {
            found_node_x = true;
            REQUIRE(rule.required);
            REQUIRE_FALSE(rule.default_int.has_value());
        } else if (name == "node_y") {
            found_node_y = true;
            REQUIRE(rule.required);
            REQUIRE_FALSE(rule.default_int.has_value());
        } else if (rule.default_int.has_value()) {
            ++default_count;
        }
    }
    REQUIRE(found_node_x);
    REQUIRE(found_node_y);
    REQUIRE(default_count >= 1);  // vc_count / buffer_size 至少有 1 个 default
}

TEST_CASE("D.3: registerParamRules is idempotent (ODR-safe)", "[d3][param-resolver]") {
    // 重复注册相同 type 应被 early-return 忽略,不会覆盖原 provider
    size_t before = ModuleFactory::getParamRulesRegistry().size();
    ModuleFactory::registerParamRules(
        "RouterTLM", []() -> ParamRules { return {{"sentinel", ParamRule{"sentinel", cpptlm::ParamType::INTEGER, true, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt}}}; });
    size_t after = ModuleFactory::getParamRulesRegistry().size();
    REQUIRE(before == after);  // 数量未变 (early-return)

    // 原 provider 仍是 RouterTLM::get_param_rules,不含 sentinel
    ParamRules rules = ModuleFactory::getParamRulesRegistry()["RouterTLM"]();
    for (const auto& [name, _] : rules) {
        REQUIRE(name != "sentinel");
    }
}

TEST_CASE("D.3: unknown type lookup returns end (graceful skip)", "[d3][param-resolver]") {
    auto& registry = ModuleFactory::getParamRulesRegistry();
    auto it = registry.find("NonExistentType_XYZ");
    REQUIRE(it == registry.end());
}
