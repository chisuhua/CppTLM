// include/tlm/cluster/memory_cluster.hh
// MemoryCluster - 多通道 HBM/DDR5 + Arbiter
// 参考: docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.4.2
// 作者: Sisyphus / 日期: 2026-06-19
#ifndef TLM_CLUSTER_MEMORY_CLUSTER_HH
#define TLM_CLUSTER_MEMORY_CLUSTER_HH

#include "core/sim_module.hh"
#include <nlohmann/json.hpp>
#include <string>

namespace cpptlm::tlm {

class MemoryCluster : public SimModule {
public:
    explicit MemoryCluster(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}

    void set_config(const nlohmann::json& params);
    std::string get_module_type() const { return "MemoryCluster"; }
    void simulate_instantiate(const nlohmann::json& cfg);

private:
    int channel_count_ = 2;
    std::string channel_size_ = "1GB";
    std::string memory_type_ = "HBM";
};

}  // namespace cpptlm::tlm

#endif  // TLM_CLUSTER_MEMORY_CLUSTER_HH
