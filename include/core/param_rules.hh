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

NLOHMANN_JSON_SERIALIZE_ENUM(ParamType, {
    {ParamType::INTEGER, "INTEGER"},
    {ParamType::FLOAT, "FLOAT"},
    {ParamType::STRING, "STRING"},
    {ParamType::BOOLEAN, "BOOLEAN"},
    {ParamType::ENUM, "ENUM"}
})

struct ParamRule {
    std::string name;
    ParamType type = ParamType::STRING;
    bool required = false;
    std::optional<int> default_int;
    std::optional<double> default_float;
    std::optional<std::string> default_str;
    std::optional<bool> default_bool;
    std::optional<int> min_value;
    std::optional<int> max_value;
    std::optional<std::vector<std::string>> enum_values;
};

inline void to_json(nlohmann::json& j, const ParamRule& p) {
    j = nlohmann::json{{"name", p.name}, {"type", p.type}, {"required", p.required}};
    if (p.default_int) j["default_int"] = *p.default_int;
    if (p.default_float) j["default_float"] = *p.default_float;
    if (p.default_str) j["default_str"] = *p.default_str;
    if (p.default_bool) j["default_bool"] = *p.default_bool;
    if (p.min_value) j["min_value"] = *p.min_value;
    if (p.max_value) j["max_value"] = *p.max_value;
    if (p.enum_values) j["enum_values"] = *p.enum_values;
}

inline void from_json(const nlohmann::json& j, ParamRule& p) {
    j.at("name").get_to(p.name);
    j.at("type").get_to(p.type);
    j.at("required").get_to(p.required);
    if (j.contains("default_int")) p.default_int = j["default_int"].get<int>();
    if (j.contains("default_float")) p.default_float = j["default_float"].get<double>();
    if (j.contains("default_str")) p.default_str = j["default_str"].get<std::string>();
    if (j.contains("default_bool")) p.default_bool = j["default_bool"].get<bool>();
    if (j.contains("min_value")) p.min_value = j["min_value"].get<int>();
    if (j.contains("max_value")) p.max_value = j["max_value"].get<int>();
    if (j.contains("enum_values")) p.enum_values = j["enum_values"].get<std::vector<std::string>>();
}

using ParamRules = std::map<std::string, ParamRule>;

} // namespace cpptlm