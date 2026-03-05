#include "core/plugin_loader.hh"

#include <dlfcn.h>
#include <iostream>
#include <vector>

bool PluginLoader::loadPlugin(const std::string& path) {
    // Use RTLD_GLOBAL to match previous behavior and allow symbols to be
    // visible for plugins that rely on static registration.
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        std::cerr << "plugin load failed: " << path << " : " << dlerror() << std::endl;
        return false;
    }

    // Prefer the existing plugin entry point used in this repository:
    //   void register_plugin_types();
    typedef void (*RegisterPluginTypesFunc)();
    RegisterPluginTypesFunc register_plugin_types =
        (RegisterPluginTypesFunc)dlsym(handle, "register_plugin_types");

    if (register_plugin_types) {
        // Explicitly register plugin types via the standard entry point.
        register_plugin_types();
    } else {
        // Fall back to the older/alternate entry point:
        //   std::string registerType();
        typedef std::string (*RegisterTypeFunc)();
        RegisterTypeFunc registerType = (RegisterTypeFunc)dlsym(handle, "registerType");
        if (registerType) {
            // Call the function to allow any side effects/registration to occur.
            // Ignore the returned string here, as the loader only tracks the path.
            registerType();
        } else {
            // Neither registration symbol is present; assume the plugin uses
            // static registration. Do not treat this as a hard failure.
            std::cerr << "plugin load warning: " << path
                      << " : no registration function found (assuming static registration)"
                      << std::endl;
        }
    }

    // Register the plugin as successfully loaded.
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
