// include/tlm/cluster/cpu_cluster.hh
// CpuCluster - CPU 集群容器 (SimModule 派生,v2.2 新增,替代 legacy 模块)
//
// 功能描述:
//   - 持有 num_cpus_ 与 cluster_id_ 参数,支持 JSON params 透传
//   - tick() 显式转发到 internal_factory 子模块的 tick()
//   - 通过 REGISTER_MODULE 在 include/modules.hh 中无条件注册
//   - 与 CPUTLM/CacheTLM/MemoryTLM 端到端集成 (Phase 7.E 验证)
//
// 迁移说明:
//   - v2.1 旧版位于 include/modules/legacy/cpu_cluster.hh (DEPRECATED,空心类)
//   - v2.2 重写并移至 include/tlm/cluster/,成为活跃 SimModule 派生类
//
// 作者: CppTLM Team
// 日期: 2026-06-18
#ifndef TLM_CLUSTER_CPU_CLUSTER_HH
#define TLM_CLUSTER_CPU_CLUSTER_HH

#include "core/sim_module.hh"
#include <nlohmann/json.hpp>
#include <string>

class CpuCluster : public SimModule {
private:
    int num_cpus_ = 4;           // 默认 4 个 CPU
    std::string cluster_id_;     // 可选集群 ID (用于 hierarchical debug)

public:
    explicit CpuCluster(const std::string& name, EventQueue* eq)
        : SimModule(name, eq) {}

    std::string get_module_type() const override { return "CpuCluster"; }

    // 解析 JSON params 字段:num_cpus (默认 4), cluster_id (默认 "")
    void set_config(const nlohmann::json& params) override {
        if (params.is_object()) {
            if (params.contains("num_cpus")) {
                num_cpus_ = params["num_cpus"].get<int>();
            }
            if (params.contains("cluster_id")) {
                cluster_id_ = params["cluster_id"].get<std::string>();
            }
        }
        // 调用基类实现以保留 params 存储与 on_config_loaded() 触发
        SimModule::set_config(params);
    }

    // 显式转发到 internal_factory 所有 SimObject 子模块
    void tick() override {
        for (const auto& kv : internal_factory->getAllInstances()) {
            if (kv.second) {
                kv.second->tick();
            }
        }
    }

    int num_cpus() const { return num_cpus_; }
    const std::string& cluster_id() const { return cluster_id_; }
};

#endif // TLM_CLUSTER_CPU_CLUSTER_HH