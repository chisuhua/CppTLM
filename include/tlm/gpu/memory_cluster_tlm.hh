// include/tlm/gpu/memory_cluster_tlm.hh
// MemoryClusterTLM: HBM/GDDR 多通道内存控制器
// 功能: 多通道 round-robin channel 分配 + 容量管理
// 作者 CppTLM Team / 日期 2026-06-24
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §3.2
// Phase 8.A Task 2 stub
#ifndef TLM_GPU_MEMORY_CLUSTER_TLM_HH
#define TLM_GPU_MEMORY_CLUSTER_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>

namespace tlm {

/**
 * @brief HBM/GDDR 多通道内存控制器 (简化 round-robin 模型)
 *
 * 按 D2 决策: 不模拟真实 DRAM 调度,仅 round-robin channel 分配
 */
class MemoryClusterTLM : public ChStreamModuleBase {
public:
    explicit MemoryClusterTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}
    ~MemoryClusterTLM() override = default;

    std::string get_module_type() const override { return "MemoryClusterTLM"; }

    // ChStreamModuleBase required override
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    /**
     * @brief Round-robin 分配 channel
     * @param request_id 请求 ID (用于 trace/统计)
     * @return 分配的 channel index [0, channels)
     */
    uint32_t allocate_channel(uint64_t request_id);

    // === 程序化 setter (JSON 解析后注入) ===
    void set_channels(uint32_t channels) { channels_ = channels; }
    void set_capacity_gb(uint32_t capacity_gb) { capacity_gb_ = capacity_gb; }

    uint32_t get_channels() const { return channels_; }
    uint32_t get_capacity_gb() const { return capacity_gb_; }

    /// 统计: 已完成请求数 (测试用)
    uint64_t requests_completed() const { return requests_completed_; }

    void tick() override;

private:
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    uint32_t channels_ = 4;            // 默认 4 通道 (GB203 HBM 典型)
    uint32_t capacity_gb_ = 8;         // 默认 8 GB (GB203 显存典型)
    uint64_t rr_counter_ = 0;
    uint64_t requests_completed_ = 0;
};

}  // namespace tlm

#endif  // TLM_GPU_MEMORY_CLUSTER_TLM_HH