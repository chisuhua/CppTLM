#include "modules/legacy/cpu_sim.hh"
#include "modules/legacy/cache_sim.hh"
#include "modules/legacy/memory_sim.hh"
#include "modules/legacy/crossbar.hh"
#include "modules/legacy/router.hh"
#include "modules/legacy/arbiter.hh"
#include "modules/legacy/traffic_gen.hh"
#include "modules/legacy/cpu_cluster.hh"

#define REGISTER_OBJECT \
    ModuleFactory::registerObject<CPUSim>("CPUSim"); \
    ModuleFactory::registerObject<CacheSim>("CacheSim"); \
    ModuleFactory::registerObject<MemorySim>("MemorySim"); \
    ModuleFactory::registerObject<Crossbar>("Crossbar"); \
    ModuleFactory::registerObject<Router>("Router"); \
    ModuleFactory::registerObject<Arbiter>("Arbiter"); \
    ModuleFactory::registerObject<TrafficGenerator>("TrafficGenerator");

// REGISTER_CHSTREAM moved to chstream_register.hh

#define REGISTER_MODULE \
    ModuleFactory::registerModule<CpuCluster>("CpuCluster");
