#pragma once

#include <string>
#include <vector>

/**
 * @brief Load policy for plugin loading
 * 
 * Controls how missing or failing plugins are handled:
 * - BEST_EFFORT: Continue on any failure
 * - STRICT: Stop on FATAL or ERROR severity
 * - CRITICAL_ONLY: Only stop on FATAL severity
 */
enum class LoadPolicy {
    BEST_EFFORT,      // Continue regardless of failures
    STRICT,           // Stop on FATAL or ERROR
    CRITICAL_ONLY     // Stop only on FATAL
};

/**
 * @brief Check if the policy allows continuation after a failure
 * 
 * @param policy The load policy
 * @param severity The severity of the failure
 * @return true if loading should continue, false if should stop
 */
inline bool shouldContinue(LoadPolicy policy, PluginLoadException::Severity severity) {
    switch (policy) {
        case LoadPolicy::BEST_EFFORT:
            return true;
        case LoadPolicy::STRICT:
            return severity != PluginLoadException::Severity::FATAL && 
                   severity != PluginLoadException::Severity::ERROR;
        case LoadPolicy::CRITICAL_ONLY:
            return severity != PluginLoadException::Severity::FATAL;
    }
    return true;
}

/**
 * @brief Get a human-readable string for the policy
 */
inline std::string to_string(LoadPolicy policy) {
    switch (policy) {
        case LoadPolicy::BEST_EFFORT:
            return "BEST_EFFORT";
        case LoadPolicy::STRICT:
            return "STRICT";
        case LoadPolicy::CRITICAL_ONLY:
            return "CRITICAL_ONLY";
    }
    return "UNKNOWN";
}
