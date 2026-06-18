// include/tlm/cluster/cache_cluster.hh
// CacheCluster - L1×N (私有) + L2 (共享) cache 子系统
// 参考: gem5 PrivateL1PrivateL2CacheHierarchy 模式
//       docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.4.1
// 作者: Sisyphus / 日期: 2026-06-19
#ifndef TLM_CLUSTER_CACHE_CLUSTER_HH
#define TLM_CLUSTER_CACHE_CLUSTER_HH

#include "core/sim_module.hh"
#include <nlohmann/json.hpp>
#include <string>

namespace cpptlm::tlm {

class CacheCluster : public SimModule {
public:
    explicit CacheCluster(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}

    void set_config(const nlohmann::json& params);
    std::string get_module_type() const { return "CacheCluster"; }
    void simulate_instantiate(const nlohmann::json& cfg);

private:
    int l1_count_ = 4;
    std::string l1_size_ = "16KB";
    std::string l2_size_ = "256KB";
};

}  // namespace cpptlm::tlm

#endif  // TLM_CLUSTER_CACHE_CLUSTER_HH
