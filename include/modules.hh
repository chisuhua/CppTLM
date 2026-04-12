#include "modules/cpu_sim.hh"
#include "modules/cache_sim.hh"
#include "modules/memory_sim.hh"
#include "modules/crossbar.hh"
#include "modules/router.hh"
#include "modules/arbiter.hh"
#include "modules/traffic_gen.hh"
#include "modules/cpu_cluster.hh"

#define REGISTER_OBJECT \
    ModuleFactory::registerObject<CPUSim>("CPUSim"); \
    ModuleFactory::registerObject<CacheSim>("CacheSim"); \
    ModuleFactory::registerObject<MemorySim>("MemorySim"); \
    ModuleFactory::registerObject<Crossbar>("Crossbar"); \
    ModuleFactory::registerObject<Router>("Router"); \
    ModuleFactory::registerObject<Arbiter>("Arbiter"); \
    ModuleFactory::registerObject<TrafficGenerator>("TrafficGenerator");

#define REGISTER_CHSTREAM \
    ModuleFactory::registerObject<CacheTLM>("CacheTLM"); \
    ModuleFactory::registerObject<MemoryTLM>("MemoryTLM");

#define REGISTER_MODULE \
    ModuleFactory::registerModule<CpuCluster>("CpuCluster");
