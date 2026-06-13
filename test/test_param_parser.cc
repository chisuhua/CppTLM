// test/test_param_parser.cc
// SPDX-License-Identifier: Apache-2.0
// 作者 CppTLM Team / 日期 2026-05-09
#include "catch_amalgamated.hpp"
#include "core/param_parser.hh"
#include "core/param_rules.hh"

using namespace cpptlm;

TEST_CASE("ParamParser: parse integer", "[param][parser]") {
    auto result = ParamParser::parse("42", ParamType::INTEGER);
    REQUIRE(result.success == true);
    REQUIRE(std::get<int64_t>(result.value) == 42);
}

TEST_CASE("ParamParser: parse float", "[param][parser]") {
    auto result = ParamParser::parse("3.14", ParamType::FLOAT);
    REQUIRE(result.success == true);
    REQUIRE(std::get<double>(result.value) == Catch::Approx(3.14));
}

TEST_CASE("ParamParser: parse string", "[param][parser]") {
    auto result = ParamParser::parse("hello", ParamType::STRING);
    REQUIRE(result.success == true);
    REQUIRE(std::get<std::string>(result.value) == "hello");
}

TEST_CASE("ParamParser: parse boolean true", "[param][parser]") {
    auto result = ParamParser::parse("true", ParamType::BOOLEAN);
    REQUIRE(result.success == true);
    REQUIRE(std::get<bool>(result.value) == true);
}

TEST_CASE("ParamParser: parse boolean false", "[param][parser]") {
    auto result = ParamParser::parse("false", ParamType::BOOLEAN);
    REQUIRE(result.success == true);
    REQUIRE(std::get<bool>(result.value) == false);
}

TEST_CASE("ParamParser: empty input fails", "[param][parser]") {
    auto result = ParamParser::parse("", ParamType::INTEGER);
    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Empty input");
}

TEST_CASE("ParamParser: invalid integer fails", "[param][parser]") {
    auto result = ParamParser::parse("not_a_number", ParamType::INTEGER);
    REQUIRE(result.success == false);
}

TEST_CASE("ParamParser: validate with min constraint", "[param][parser]") {
    ParamRule rule;
    rule.type = ParamType::INTEGER;
    rule.min_value = 10;

    auto result = ParamParser::parse("5", ParamType::INTEGER);
    REQUIRE(result.success == true);
    REQUIRE(ParamParser::validate(result, rule) == false);

    result = ParamParser::parse("15", ParamType::INTEGER);
    REQUIRE(result.success == true);
    REQUIRE(ParamParser::validate(result, rule) == true);
}

TEST_CASE("ParamParser: validate with max constraint", "[param][parser]") {
    ParamRule rule;
    rule.type = ParamType::INTEGER;
    rule.max_value = 100;

    auto result = ParamParser::parse("200", ParamType::INTEGER);
    REQUIRE(result.success == true);
    REQUIRE(ParamParser::validate(result, rule) == false);

    result = ParamParser::parse("50", ParamType::INTEGER);
    REQUIRE(result.success == true);
    REQUIRE(ParamParser::validate(result, rule) == true);
}

TEST_CASE("evaluate_derive_expr: missing param returns 0", "[param][derive]") {
    std::map<std::string, int64_t> params = {{"other", 10}};
    int64_t result = ParamParser::evaluate_derive_expr("vc_count >= 4 ? 100 : 50", params);
    REQUIRE(result == 0);
}
