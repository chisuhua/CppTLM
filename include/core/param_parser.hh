// include/core/param_parser.hh

#pragma once

#include "param_rules.hh"
#include <string>
#include <map>
#include <variant>

namespace cpptlm {

struct ParamParseResult {
    bool success;
    std::string error_message;
    std::variant<int64_t, uint64_t, double, std::string, bool> value;
};

class ParamParser {
public:
    static ParamParseResult parse(const std::string& input, ParamType type,
                                  double clock_frequency_mhz = 1000.0);

    static bool validate(const ParamParseResult& result, const ParamRule& rule);

    static int64_t evaluate_derive_expr(const std::string& expr,
                                        const std::map<std::string, int64_t>& params);

private:
    static uint64_t parse_latency(const std::string& s, double clock_mhz);
};

} // namespace cpptlm