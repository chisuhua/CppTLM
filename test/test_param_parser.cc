// test/test_param_parser.cc
// SPDX-License-Identifier: Apache-2.0
// 作者 CppTLM Team / 日期 2026-05-09
#include "catch_amalgamated.hpp"
#include "core/param_parser.hh"

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

// ⚠️ `validate`（min/max）用例已于 change `cpptlm-module-param-migrate-batch2` T4 删除
//    —— `ParamParser::validate` 随其唯一调用方（死函数 `validate_module_params`）一并退役
//    （`ADR-X.17` D-3：范围/枚举校验改由 `load_param` 的 `lo`/`hi` 参数承担）。
//    `parse` 本身保留（本文件仍是其判据）。

TEST_CASE("evaluate_derive_expr: missing param returns 0", "[param][derive]") {
    std::map<std::string, int64_t> params = {{"other", 10}};
    int64_t result = ParamParser::evaluate_derive_expr("vc_count >= 4 ? 100 : 50", params);
    REQUIRE(result == 0);
}
