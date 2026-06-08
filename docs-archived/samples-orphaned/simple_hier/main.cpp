// sample/main.cpp
#include "event_queue.hh"
#include "module_factory.hh"
#include "utils/dynamic_loader.hh"

int main() {
    EventQueue eq;
    ModuleFactory factory(&eq);

    // 加载顶层配置
    json config = JsonIncluder::loadAndInclude("configs/system.json");
    factory.instantiateAll(config);
    factory.startAllTicks();

    // 运行仿真
    eq.run(1000);

    // 清理
    DynamicLoader::unloadAll();
    return 0;
}
