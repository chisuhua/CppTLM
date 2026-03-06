#pragma once

#include <exception>
#include <string>

/**
 * @brief Exception thrown when a plugin fails to load
 * 
 * Includes severity level (FATAL, ERROR, WARNING) to allow
 * different handling strategies for different failure severities.
 */
class PluginLoadException : public std::exception {
public:
    enum class Severity {
        FATAL,      // Critical plugin - simulation should not continue
        ERROR,      // Important plugin - warn but may continue
        WARNING     // Optional plugin - log but continue
    };

    PluginLoadException(Severity severity, const std::string& path, const std::string& message)
        : severity_(severity), path_(path), message_(message) {}

    Severity severity() const noexcept { return severity_; }
    const std::string& path() const noexcept { return path_; }
    const std::string& message() const noexcept { return message_; }

    const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    Severity severity_;
    std::string path_;
    std::string message_;
};
