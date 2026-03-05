#include "core/plugin_loader.hh"

#include <dlfcn.h>
#include <iostream>
#include <vector>

bool PluginLoader::loadPlugin(const std::string& path) {
    void* handle = dlopen(path.c_str(), RTLD_NOW);
    if (!handle) {
        std::cerr << "plugin load failed: " << path << " : " << dlerror() << std::endl;
        return false;
    }

    // Check for registerType function
    typedef std::string (*RegisterTypeFunc)();
    RegisterTypeFunc registerType = (RegisterTypeFunc)dlsym(handle, "registerType");
    if (!registerType) {
        std::cerr << "plugin load failed: " << path << " : missing registerType function" << std::endl;
        dlclose(handle);
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
