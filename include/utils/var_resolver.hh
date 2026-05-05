// include/utils/var_resolver.hh
// T3.1-07: Variable reference ${path} resolution
// Created: 2026-05-06

#ifndef VAR_RESOLVER_HH
#define VAR_RESOLVER_HH

#include <string>
#include <regex>
#include <sstream>
#include <nlohmann/json.hpp>
#include "core/sim_core.hh"

namespace cpptlm {

class VarResolver {
public:
    VarResolver(const nlohmann::json& config) : config_(config) {}

    nlohmann::json resolveAll(nlohmann::json j) {
        if (j.is_object()) {
            nlohmann::json result;
            for (auto& [key, val] : j.items()) {
                result[key] = resolveAll(val);
            }
            return result;
        } else if (j.is_array()) {
            nlohmann::json result;
            for (auto& val : j) {
                result.push_back(resolveAll(val));
            }
            return result;
        } else if (j.is_string()) {
            return resolve_string(j.get<std::string>());
        }
        return j;
    }

private:
    nlohmann::json config_;

    nlohmann::json resolve_string(const std::string& value) {
        std::regex var_regex(R"(\$\{([^}]+)\})");
        std::smatch match;
        std::string result = value;

        while (std::regex_search(result, match, var_regex)) {
            std::string var_path = match[1].str();
            nlohmann::json resolved = resolve_path(var_path);

            if (resolved.is_null()) {
                DPRINTF(MODULE, "[WARN] Unresolved variable reference: ${%s}\n", var_path.c_str());
                break;
            }

            std::string replacement;
            if (resolved.is_string()) {
                replacement = resolved.get<std::string>();
            } else if (resolved.is_number()) {
                replacement = std::to_string(resolved.get<double>());
            } else if (resolved.is_boolean()) {
                replacement = resolved.get<bool>() ? "true" : "false";
            }

            result.replace(match.position(), match.length(), replacement);
        }

        if (!result.empty() && result.find('$') == std::string::npos) {
            char* end;
            double num = strtod(result.c_str(), &end);
            if (end != result.c_str() && *end == '\0') {
                return num;
            }
        }

        return result;
    }

    nlohmann::json resolve_path(const std::string& path) {
        std::istringstream iss(path);
        std::string token;
        nlohmann::json current = config_;

        while (std::getline(iss, token, '.')) {
            auto bracket_pos = token.find('[');
            if (bracket_pos != std::string::npos) {
                std::string key = token.substr(0, bracket_pos);
                int index = std::stoi(token.substr(bracket_pos + 1));
                if (current.contains(key) && current[key].is_array() &&
                    index < static_cast<int>(current[key].size())) {
                    current = current[key][index];
                } else {
                    return nlohmann::json(nullptr);
                }
            } else {
                if (current.is_object() && current.contains(token)) {
                    current = current[token];
                } else {
                    return nlohmann::json(nullptr);
                }
            }
        }
        return current;
    }
};

} // namespace cpptlm

#endif // VAR_RESOLVER_HH