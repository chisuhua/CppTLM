// =============================================================================
// DEPRECATED in v2.1: See header for migration notes. This file is kept for
// backward compatibility with v2.0 projects and is not compiled by the default
// samples/CMakeLists.txt. Use CPUTLM (include/tlm/cpu_tlm.hh) for new work.
// =============================================================================
#include "cpu_cluster.hh"

// 注册 CpuCluster 类型
static bool cpu_cluster_registered = []() {
    ModuleFactory::registerModule<CpuCluster>("CPUCluster");
    return true;
}();
