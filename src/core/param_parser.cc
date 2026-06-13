#include "core/param_parser.hh"
#include <cstdint>
#include <regex>

namespace cpptlm {

    ParamParseResult ParamParser::parse(const std::string& input, ParamType type,
                                        double clock_frequency_mhz) {
        ParamParseResult result;

        if (input.empty()) {
            result.success = false;
            result.error_message = "Empty input";
            return result;
        }

        try {
            switch (type) {
            case ParamType::INTEGER: {
                int64_t val = std::stoll(input);
                result.value = val;
                result.success = true;
                break;
            }
            case ParamType::FLOAT: {
                double val = std::stod(input);
                result.value = val;
                result.success = true;
                break;
            }
            case ParamType::STRING: {
                result.value = input;
                result.success = true;
                break;
            }
            case ParamType::BOOLEAN: {
                bool val = (input == "true" || input == "1" || input == "yes");
                result.value = val;
                result.success = true;
                break;
            }
            default:
                result.success = false;
                result.error_message = "Unsupported type";
            }
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = e.what();
        }

        return result;
    }

    bool ParamParser::validate(const ParamParseResult& result, const ParamRule& rule) {
        if (!result.success)
            return false;

        if (std::holds_alternative<int64_t>(result.value)) {
            int64_t val = std::get<int64_t>(result.value);
            if (rule.min_value && val < *rule.min_value)
                return false;
            if (rule.max_value && val > *rule.max_value)
                return false;
        }

        return true;
    }

    int64_t ParamParser::evaluate_derive_expr(const std::string& expr,
                                              const std::map<std::string, int64_t>& params) {
        size_t q_pos = expr.find('?');
        if (q_pos == std::string::npos)
            return 0;

        std::string cond = expr.substr(0, q_pos);
        std::string rest = expr.substr(q_pos + 1);
        size_t colon_pos = rest.find(':');
        if (colon_pos == std::string::npos)
            return 0;

        std::string true_val_str = rest.substr(0, colon_pos);
        std::string false_val_str = rest.substr(colon_pos + 1);

        std::regex cmp_regex(R"((\w+)\s*(>=|<=|> |< |==)\s*(\d+))");
        std::smatch match;
        if (std::regex_match(cond, match, cmp_regex)) {
            std::string param_name = match[1].str();
            std::string op = match[2].str();
            int64_t rhs = std::stoll(match[3].str());

            auto it = params.find(param_name);
            if (it == params.end())
                return 0;
            int64_t lhs = it->second;

            bool cond_result = false;
            if (op == ">=")
                cond_result = lhs >= rhs;
            else if (op == "<=")
                cond_result = lhs <= rhs;
            else if (op == ">")
                cond_result = lhs > rhs;
            else if (op == "<")
                cond_result = lhs < rhs;
            else if (op == "==")
                cond_result = lhs == rhs;

            return cond_result ? std::stoll(true_val_str) : std::stoll(false_val_str);
        }

        return 0;
    }

    uint64_t ParamParser::parse_latency(const std::string& s, double clock_mhz) {
        std::regex latency_regex(R"((\d+)(ns|us|ms)?)");
        std::smatch match;
        if (std::regex_match(s, match, latency_regex)) {
            uint64_t value = std::stoull(match[1].str());
            std::string unit = match[2].str();
            if (unit == "ns") {
                return static_cast<uint64_t>(value * clock_mhz / 1000.0);
            } else if (unit == "us") {
                return static_cast<uint64_t>(value * clock_mhz);
            } else if (unit == "ms") {
                return static_cast<uint64_t>(value * clock_mhz * 1000);
            }
            return value;
        }
        return std::stoull(s);
    }

} // namespace cpptlm