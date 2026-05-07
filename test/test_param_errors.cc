// test/test_param_errors.cc

#include <catch2/catch_all.hpp>
#include "core/param_errors.hh"

TEST_CASE("ParamValidationError: exception carries info", "[param_errors][phase3.3]") {
    cpptlm::ParamValidationError err("router_0", "node_x", "out of range");
    REQUIRE(err.module_name == "router_0");
    REQUIRE(err.param_name == "node_x");
    REQUIRE(std::string(err.what()) == "out of range");
}

TEST_CASE("ParamValidationError: can be caught as std::exception", "[param_errors][phase3.3]") {
    try {
        throw cpptlm::ParamValidationError("mod", "param", "test error");
    } catch (const std::exception& e) {
        REQUIRE(std::string(e.what()) == "test error");
    }
}