#pragma once

#include <string>
#include <vector>
#include <memory>
#include "plugin_load_exception.hh"
#include "load_policy.hh"

/**
 * @brief Plugin loader class for dynamic library loading
 * 
 * Wraps dlopen/dlsym for loading and managing plugins with configurable
 * error handling policies and severity levels.
 */
class PluginLoader {
public:
    PluginLoader() = default;
    ~PluginLoader() = default;

    /**
     * @brief Load a single plugin from the given path
     * @param path Path to the plugin shared library
     * @param policy Loading policy (BEST_EFFORT, STRICT, CRITICAL_ONLY)
     * @param is_critical Whether this is a critical plugin
     * @return true if plugin loaded successfully
     * @throws PluginLoadException if loading fails and policy doesn't continue
     */
    bool loadPlugin(const std::string& path, LoadPolicy policy = LoadPolicy::BEST_EFFORT, bool is_critical = false);

    /**
     * @brief Get list of all loaded plugin paths
     * @return Vector of plugin file paths
     */
    std::vector<std::string> getLoadedPlugins() const;

    /**
     * @brief Check if a plugin with given name is loaded
     * @param path The plugin file path to check
     * @return true if plugin is loaded, false otherwise
     */
    bool hasPlugin(const std::string& path) const;

    /**
     * @brief Clear all loaded plugins
     */
    void clear();

    /**
     * @brief Load all plugins from a list with the same policy
     * @param paths List of plugin paths
     * @param policy Loading policy
     * @param critical_paths List of plugin paths considered critical
     * @return Number of plugins successfully loaded
     */
    int loadPlugins(const std::vector<std::string>& paths, LoadPolicy policy = LoadPolicy::BEST_EFFORT, 
                    const std::vector<std::string>& critical_paths = {});

private:
    std::vector<std::string> loadedPlugins_;
};
