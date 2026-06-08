// sample/plugins/cpu_cluster.cc
#include "sim_module.hh"

class CPUCluster : public SimModule {
public:
    using SimModule::SimModule;

    void customBehavior() {
        // 可添加特定行为
    }
};

extern "C" void register_plugin_types() {
    ModuleFactory::registerType<CPUCluster>("CPUCluster");
}
