// include/tlm/gpu/gpu_mesh_noc_tlm.hh
// GpuMeshNoC: GPC 之间 mesh interconnect (XY 路由)
// 功能: 简化的 NxN mesh 网络 + XY 维度序路由 + hops × latency 延迟模型
// 作者 CppTLM Team / 日期 2026-06-24
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §3.3
// Phase 8.A Task 3 stub
// 类名 GpuMeshNoC 避免与 include/tlm/cluster/gpu_noc_cluster.hh 中已有的 GpuNoC (SimModule) 类冲突
#ifndef TLM_GPU_GPU_MESH_NOC_TLM_HH
#define TLM_GPU_GPU_MESH_NOC_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>
#include <utility>

namespace tlm {

/**
 * @brief GPC 之间 mesh interconnect (XY 维度序路由)
 *
 * 简化模型 (per D2 决策): XY 路由 + hops × latency
 * 不模拟 VC 分配/拥塞控制
 */
class GpuMeshNoC : public ChStreamModuleBase {
public:
    explicit GpuMeshNoC(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}
    ~GpuMeshNoC() override = default;

    std::string get_module_type() const override { return "GpuMeshNoC"; }

    // ChStreamModuleBase required override
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    /**
     * @brief 计算 XY 路由延迟
     * @param src 源坐标 (x, y)
     * @param dst 目标坐标 (x, y)
     * @return 路由延迟 cycles = (|dx| + |dy|) × hops_latency
     */
    uint32_t route_latency(std::pair<uint32_t, uint32_t> src,
                           std::pair<uint32_t, uint32_t> dst) const;

    // === 程序化 setter (JSON 解析后注入) ===
    void set_dim(uint32_t dim) { dim_ = dim; }
    void set_hops_latency(uint32_t hops_latency) { hops_latency_ = hops_latency; }

    uint32_t get_dim() const { return dim_; }
    uint32_t get_hops_latency() const { return hops_latency_; }

    void tick() override;

private:
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    uint32_t dim_ = 2;
    uint32_t hops_latency_ = 2;
    uint64_t cycle_counter_ = 0;
};

}  // namespace tlm

#endif  // TLM_GPU_GPU_MESH_NOC_TLM_HH