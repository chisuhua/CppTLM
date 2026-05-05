// include/core/param_rules.hh
// SPDX-License-Identifier: Apache-2.0
// Phase 3.1: 参数规则定义

#pragma once

#include <string>
#include <optional>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

namespace cpptlm {

enum class ParamType {
    INTEGER,
    FLOAT,
    STRING,
    BOOLEAN,
    ENUM
};

struct ParamRule {
    std::string name;
    ParamType type;
    bool required;
    std::optional<int> default_int;
    std::optional<double> default_float;
    std::optional<std::string> default_str;
    std::optional<bool> default_bool;
    std::optional<int> min_value;
    std::optional<int> max_value;
    std::optional<std::vector<std::string>> enum_values;
};

using ParamRules = std::map<std::string, ParamRule>;

} // namespace cpptlm