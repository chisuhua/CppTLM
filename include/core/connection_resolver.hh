#pragma once

#include <string>
#include <vector>
#include <utility>
#include <functional>
#include "core/module_factory.hh"

/**
 * @brief Connection resolver for parsing and creating connections between modules
 * 
 * Handles the parsing of connection specifications (src/dst port specs),
 * buffer sizes, and creates port creation data structures.
 */
class ConnectionResolver {
public:
    ConnectionResolver() = default;
    ~ConnectionResolver() = default;

    /**
     * @brief Parse a port specification string
     * @param spec Port spec in format "module.port" or just "module" or "module.*" for wildcards
     * @return Pair of (module_name, port_name)
     */
    std::pair<std::string, std::string> parsePortSpec(const std::string& spec) const;

    /**
     * @brief Resolve connections and create port creation data
     * @param connections Json array of connection objects
     * @param module_instances Map of module instances
     * @param createPortFunc Function to create a port (returns bool success)
     * @return Vector of port creation info structures
     */
    std::vector<PortCreationInfo> resolveConnections(
        const nlohmann::json& connections,
        const std::unordered_map<std::string, SimModule*>& module_instances,
        std::function<bool(const std::string&, const std::string&, size_t, bool)> createPortFunc
    ) const;

private:
    /**
     * @brief Find internal path for exposed port
     * @param module SimModule instance
     * @param port_name Port name to find
     * @return Internal path string or empty if not found
     */
    std::string findInternalPath(SimModule* module, const std::string& port_name) const;
};
