// include/modules/cpu_cluster.hh
// 分层模块容器：作为层次化拓扑的连接点
// 内部子模块通过 JSON config 实例化，外部通过 outputs/inputs 暴露端口
#ifndef CPU_CLUSTER_HH
#define CPU_CLUSTER_HH

#include "../core/sim_module.hh"

class CpuCluster : public SimModule {
public:
    explicit CpuCluster(const std::string& name, EventQueue* eq) : SimModule(name, eq) {}

    void tick() {}

private:
};

#endif // CPU_CLUSTER_HH