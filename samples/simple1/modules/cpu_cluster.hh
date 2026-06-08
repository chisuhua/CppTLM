// =============================================================================
// DEPRECATED in v2.1: This sample uses legacy CPUSim/CacheSim modules.
// New projects should use CPUTLM (include/tlm/cpu_tlm.hh) from the Phase 6
// TLM module family. This file is kept for backward compatibility with v2.0
// projects and is NOT compiled by the default samples/CMakeLists.txt.
//
// To use this sample with the legacy modules enabled, build with:
//     cmake -DBUILD_LEGACY_MODULES=ON
// See docs-archived/ for migration guidance.
// =============================================================================
#ifndef CPU_CLUSTER_HH
#define CPU_CLUSTER_HH

#include "sim_module.hh"
#include "modules/cpu_sim.hh"
#include "modules/cache_sim.hh"

// CpuCluster 是一个具体的 SimModule 实现
class CpuCluster : public SimModule {
public:
    explicit CpuCluster(const std::string& name, EventQueue* eq) : SimModule(name, eq) {}
    
    // 可以在这里添加一些特定于 cpu cluster 的方法
    // 例如：
    CPUSim* getCpu(int index) {
        return dynamic_cast<CPUSim*>(getInternalInstance("cpu" + std::to_string(index)));
    }

    CacheSim* getCache() {
        return dynamic_cast<CacheSim*>(getInternalInstance("cache"));
    }

    void tick() {}

    // ... 其他业务逻辑 ...
};

#endif // CPU_CLUSTER_HH
