// include/tlm/cluster/compute_cluster.hh
// ComputeCluster - 1 个 CU 组, 从 JSON 蓝图模板复制 N 份
// 功能: 接收 cu_template 路径 + cu_count, 加载蓝图并实例化 N 个 CU
// 参考: gem5 SimpleProcessor(cpu_type, num_cores, isa) 模式
//       docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.2.1
// 作者: Sisyphus / 日期: 2026-06-19
#ifndef TLM_CLUSTER_COMPUTE_CLUSTER_HH
#define TLM_CLUSTER_COMPUTE_CLUSTER_HH

#include "core/sim_module.hh"
#include <nlohmann/json.hpp>
#include <string>

namespace cpptlm::tlm {

class ComputeCluster : public SimModule {
public:
    explicit ComputeCluster(const std::string& n, EventQueue* eq)
        : SimModule(n, eq) {}

    void set_config(const nlohmann::json& params) override;
    std::string get_module_type() const override { return "ComputeCluster"; }
    void simulate_instantiate(const nlohmann::json& cfg);

private:
    std::string cu_template_path_;
    int cu_count_ = 1;
};

}  // namespace cpptlm::tlm

#endif  // TLM_CLUSTER_COMPUTE_CLUSTER_HH