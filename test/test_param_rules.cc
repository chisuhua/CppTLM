// test/test_param_rules.cc
// Phase 3.3: ParamRule JSON serialization tests
// 作者：CppTLM Team
// 日期：2026-05-07

#include "core/param_rules.hh"
#include <catch2/catch_all.hpp>

TEST_CASE("ParamRule: JSON serialization roundtrip", "[param][phase3.3]") {
    cpptlm::ParamRule rule;
    rule.name = "vc_count";
    rule.type = cpptlm::ParamType::INTEGER;
    rule.required = false;
    rule.default_int = 4;
    rule.min_value = 1;
    rule.max_value = 8;

    nlohmann::json j = rule;
    REQUIRE(j["type"] == "INTEGER");
    REQUIRE(j["default_int"] == 4);

    cpptlm::ParamRule rule2 = j.get<cpptlm::ParamRule>();
    REQUIRE(rule.type == rule2.type);
    REQUIRE(rule.default_int == rule2.default_int);
}

TEST_CASE("ParamRule: default values omitted in JSON", "[param][phase3.3]") {
    cpptlm::ParamRule rule;
    rule.name = "test_param";
    rule.type = cpptlm::ParamType::STRING;
    rule.required = true;

    nlohmann::json j = rule;
    REQUIRE(j["name"] == "test_param");
    REQUIRE(j["required"] == true);
    REQUIRE(j.contains("default_int") == false);
    REQUIRE(j.contains("min_value") == false);
}

TEST_CASE("ParamType: enum serialization", "[param][phase3.3]") {
    nlohmann::json j;
    j["type"] = cpptlm::ParamType::FLOAT;
    REQUIRE(j["type"] == "FLOAT");

    j["type"] = cpptlm::ParamType::BOOLEAN;
    REQUIRE(j["type"] == "BOOLEAN");
}

TEST_CASE("ParamRule: full roundtrip with all fields", "[param][phase3.3]") {
    cpptlm::ParamRule rule;
    rule.name = "complex_param";
    rule.type = cpptlm::ParamType::INTEGER;
    rule.required = true;
    rule.default_int = 16;
    rule.min_value = 1;
    rule.max_value = 32;

    nlohmann::json j = rule;
    cpptlm::ParamRule restored = j.get<cpptlm::ParamRule>();

    REQUIRE(restored.name == rule.name);
    REQUIRE(restored.type == rule.type);
    REQUIRE(restored.required == rule.required);
    REQUIRE(restored.default_int == rule.default_int);
    REQUIRE(restored.min_value == rule.min_value);
    REQUIRE(restored.max_value == rule.max_value);
}