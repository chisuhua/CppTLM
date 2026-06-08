#include "mem_cluster.hh"

// 注册 MemCluster 类型
static bool mem_cluster_registered = []() {
    ModuleFactory::registerModule<MemCluster>("MemCluster");
    return true;
}();
