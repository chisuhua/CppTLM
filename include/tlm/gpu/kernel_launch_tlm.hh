// include/tlm/gpu/kernel_launch_tlm.hh
// KernelLaunchTLM: AQL 简化 dispatcher
// 功能: 按 interval 周期向 ComputeCluster 发 KernelDesc (简化版 AQL packet)
// 作者 CppTLM Team / 日期 2026-06-24
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §3.4
// Phase 8.A Task 4 stub
#ifndef TLM_GPU_KERNEL_LAUNCH_TLM_HH
#define TLM_GPU_KERNEL_LAUNCH_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>

namespace tlm {

/**
 * @brief AQL 简化 dispatcher (按 ADR-SOC-04 黑盒决策)
 *
 * 简化模型: tick() 按 interval 周期向 ComputeCluster 发 KernelDesc
 * 不模拟真实 HSA Queue / AQL packet
 */
class KernelLaunchTLM : public ChStreamModuleBase {
public:
    KernelLaunchTLM(const std::string& name, EventQueue* eq);
    ~KernelLaunchTLM() override = default;

    std::string get_module_type() const override { return "KernelLaunchTLM"; }

    // ChStreamModuleBase required override
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    // === 程序化 setter (JSON 解析在 Phase 7.B+ 完整实现) ===
    void set_kernel_id(uint32_t id) { kernel_id_ = id; }
    void set_workgroup_size(uint32_t sz) { workgroup_size_ = sz; }
    void set_grid_size(uint32_t sz) { grid_size_ = sz; }
    void set_kernel_launch_interval(uint32_t cyc) { interval_ = cyc; }

    uint32_t get_kernel_id() const { return kernel_id_; }
    uint32_t get_workgroup_size() const { return workgroup_size_; }
    uint32_t get_grid_size() const { return grid_size_; }
    uint32_t get_kernel_launch_interval() const { return interval_; }

    /// 统计: 已 launch kernel 数 (测试用)
    uint64_t kernels_launched() const { return kernels_launched_; }

    void tick() override;

private:
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    uint32_t kernel_id_ = 0;
    uint32_t workgroup_size_ = 64;
    uint32_t grid_size_ = 1;
    uint32_t interval_ = 1000;
    uint64_t cycle_counter_ = 0;
    uint64_t kernels_launched_ = 0;
};

}  // namespace tlm

#endif  // TLM_GPU_KERNEL_LAUNCH_TLM_HH