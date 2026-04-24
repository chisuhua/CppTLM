#include "modules/legacy/cpu_sim.hh"
#include "modules/legacy/cpu_cluster.hh"

// REGISTER_CHSTREAM moved to chstream_register.hh

// Legacy modules (CPUSim only remaining) - used by CpuCluster internal configs
#define REGISTER_OBJECT \
    ModuleFactory::registerObject<CPUSim>("CPUSim");

#define REGISTER_MODULE \
    ModuleFactory::registerModule<CpuCluster>("CpuCluster");