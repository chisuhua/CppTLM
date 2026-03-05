#include "core/plugin_loader.hh"
#include "core/plugin_load_exception.hh"
#include "core/load_policy.hh"

#include <dlfcn.h>
#include <iostream>

bool PluginLoader::loadPlugin(const std::string& path, LoadPolicy policy, bool is_critical) {
    void* handle = dlopen(path.c_str(), RTLD_NOW);
    if (!handle) {
        std::cerr << "plugin load failed: " << path << " : " << dlerror() << std::endl;
        
        PluginLoadException::Severity severity = 
            is_critical ? PluginLoadException::Severity::FATAL : 
            policy == LoadPolicy::BEST_EFFORT ? PluginLoadException::Severity::WARNING :
            PluginLoadException::Severity::ERROR;
        
        if (!shouldContinue(policy, severity)) {
            throw PluginLoadException(severity, path, dlerror());
        }
        return false;
    }

    // Check for registerType function
    typedef std::string (*RegisterTypeFunc)();
    RegisterTypeFunc registerType = (RegisterTypeFunc)dlsym(handle, "registerType");
    if (!registerType) {
        std::cerr << "plugin load failed: " << path << " : missing registerType function" << std::endl;
        
        PluginLoadException::Severity severity = 
            is_critical ? PluginLoadException::Severity::FATAL : 
            policy == LoadPolicy::BEST_EFFORT ? PluginLoadException::Severity::WARNING :
            PluginLoadException::Severity::ERROR;
        
        dlclose(handle);
        if (!shouldContinue(policy, severity)) {
            throw PluginLoadException(severity, path, "missing registerType function");
        }
        return false;
    }

    // Register the plugin
    loadedPlugins_.push_back(path);
    return true;
}

std::vector<std::string> PluginLoader::getLoadedPlugins() const {
    return loadedPlugins_;
}

bool PluginLoader::hasPlugin(const std::string& path) const {
    for (const auto& p : loadedPlugins_) {
        if (p == path) {
            return true;
        }
    }
    return false;
}

void PluginLoader::clear() {
    loadedPlugins_.clear();
}

int PluginLoader::loadPlugins(const std::vector<std::string>& paths, LoadPolicy policy, 
                              const std::vector<std::string>& critical_paths) {
    int loaded = 0;
    for (const auto& path : paths) {
        bool is_critical = false;
        for (const auto& crit : critical_paths) {
            if (crit == path) {
                is_critical = true;
                break;
            }
        }
        if (loadPlugin(path, policy, is_critical)) {
            loaded++;
        }
    }
    return loaded;
}
