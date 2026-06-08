// sample/plugins/noc_tile.cc
#include "../include/sim_module.hh"

class NOCTile : public SimModule {
public:
    using SimModule::SimModule;

    void onRouterEvent() {
        // 可添加 NoC 特定逻辑
    }
};

extern "C" void register_plugin_types() {
    ModuleFactory::registerType<NOCTile>("NOCTile");
}
