// include/core/param_errors.hh
// Phase 3.3: Parameter validation error exception

#pragma once

#include <stdexcept>
#include <string>

namespace cpptlm {

class ParamValidationError : public std::invalid_argument {
public:
    std::string module_name;
    std::string param_name;
    std::string rule_violated;

    ParamValidationError(const std::string& module,
                         const std::string& param,
                         const std::string& reason)
        : std::invalid_argument(reason),
          module_name(module),
          param_name(param),
          rule_violated(reason) {}
};

} // namespace cpptlm