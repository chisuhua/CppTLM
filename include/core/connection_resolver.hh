#pragma once

#include <string>
#include <vector>
#include <utility>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>

class SimModule;

struct PortCreationInfo {
    std::string owner_name;
    std::string port_name;
    std::vector<size_t> buffer_sizes;
    std::vector<size_t> priorities;
    bool is_upstream;
    int latency;  // P1.2: latency 注入 bug 修复 - 需要传递到端口创建
    
    PortCreationInfo() = default;
    PortCreationInfo(const std::string& o, const std::string& p, 
                     const std::vector<size_t>& bs, const std::vector<size_t>& pri, bool upstream, int lat = 0)
        : owner_name(o), port_name(p), buffer_sizes(bs), priorities(pri), is_upstream(upstream), latency(lat) {}
};
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
        std::function<bool(const std::string&, const std::string&, size_t, bool)>
    ) const;

    // D.2 (Change D): 暴露警告收集器 (per Metis bg_03094fec)
    // mutable 因为 resolveConnections 是 const 方法; 收集 WARN 级诊断,
    // 不影响 return 路径 (向后兼容)。测试断言用 `warnings()` 访问。
    // 注: warnings_ 不属于 stateful behaviour, mutable 仅作技术性需求。
    const std::vector<std::string>& warnings() const { return warnings_; }
    void clear_warnings() { warnings_.clear(); }

private:
    /**
     * @brief Find internal path for exposed port
     * @param module SimModule instance
     * @param port_name Port name to find
     * @return Internal path string or empty if not found
     */
    std::string findInternalPath(SimModule* module, const std::string& port_name) const;

    // D.2 (Change D): WARN 收集 (mutable, see warnings() above)
    mutable std::vector<std::string> warnings_;
};
