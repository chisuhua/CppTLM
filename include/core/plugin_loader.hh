#pragma once

#include <string>
#include <vector>

/**
 * @brief Plugin loader class for dynamic library loading
 * 
 * Wraps dlopen/dlsym for loading and managing plugins.
 * Plugins must implement a registerType() function that returns
 * the plugin type name as a std::string.
 */
class PluginLoader {
public:
    PluginLoader() = default;
    ~PluginLoader() = default;

    /**
     * @brief Load a single plugin from the given path
     * @param path Path to the plugin shared library
     * @return true if plugin loaded successfully, false otherwise
     */
    bool loadPlugin(const std::string& path);

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

private:
    std::vector<std::string> loadedPlugins_;
};
