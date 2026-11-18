// include/core/param_parser.hh

#pragma once

#include <string>
#include <map>
#include <variant>
#include <cstdint>

namespace cpptlm {

// `ParamType` 原属 `core/param_rules.hh`（change `cpptlm-module-param-migrate-batch2`
// T4 退役 ParamRules 机制时迁入 —— `ParamParser::parse` 仍需要它）。
enum class ParamType { INTEGER, FLOAT, STRING, BOOLEAN, ENUM };

struct ParamParseResult {
    bool success;
    std::string error_message;
    std::variant<int64_t, uint64_t, double, std::string, bool> value;
};

class ParamParser {
public:
    static ParamParseResult parse(const std::string& input, ParamType type,
                                  double clock_frequency_mhz = 1000.0);

    static int64_t evaluate_derive_expr(const std::string& expr,
                                        const std::map<std::string, int64_t>& params);

private:
    static uint64_t parse_latency(const std::string& s, double clock_mhz);
};

} // namespace cpptlm