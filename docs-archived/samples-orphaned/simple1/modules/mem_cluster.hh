// include/modules/mem_cluster.hh
#ifndef MEM_CLUSTER_HH
#define MEM_CLUSTER_HH

#include "sim_module.hh"
#include "modules/memory_sim.hh"

// MemCluster 是一个具体的 SimModule 实现
class MemCluster : public SimModule {
public:
    explicit MemCluster(const std::string& name, EventQueue* eq) : SimModule(name, eq) {}
    
    // 可以在这里添加一些特定于 mem cluster 的方法
    MemorySim* getMemory(int index) {
        return dynamic_cast<MemorySim*>(getInternalInstance("mem_" + std::string(index == 0 ? "a" : "b")));
    }

    void tick() override {
        // MemCluster本身不需要tick
    }
};

#endif // MEM_CLUSTER_HH
