// include/tlm/gpu/wavefront_tlm.hh
// WavefrontTLM: wavefront 数据载体
// 作者 CppTLM Team / 日期 2026-06-30
#ifndef TLM_GPU_WAVEFRONT_TLM_HH
#define TLM_GPU_WAVEFRONT_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>
#include <string>

namespace tlm {

// Task 10 (per architecture/15 §15.7.2 + plan): WavefrontTLM 内化到 sm::SIMTLane.
// 保留旧类, 标记 deprecated, chstream_register 注销. Task 16 删除文件.
class [[deprecated("use tlm::sm::SIMTLane")]] WavefrontTLM : public ChStreamModuleBase {
public:
    explicit WavefrontTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}
    ~WavefrontTLM() override = default;

    std::string get_module_type() const override { return "WavefrontTLM"; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    void set_kernel_id(uint32_t k) { kernel_id_ = k; }
    void set_workgroup_id(uint32_t w) { workgroup_id_ = w; }
    void set_warp_id(uint32_t w) { warp_id_ = w; }
    void set_active_mask(uint32_t m) { active_mask_ = m; }
    void set_coalescing_factor(uint32_t cf) { coalescing_factor_ = cf; }

    uint32_t get_kernel_id() const { return kernel_id_; }
    uint32_t get_workgroup_id() const { return workgroup_id_; }
    uint32_t get_warp_id() const { return warp_id_; }
    uint32_t get_active_mask() const { return active_mask_; }
    uint32_t get_coalescing_factor() const { return coalescing_factor_; }

    void tick() override {}

private:
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    uint32_t kernel_id_ = 0;
    uint32_t workgroup_id_ = 0;
    uint32_t warp_id_ = 0;
    uint32_t active_mask_ = 0xFFFFFFFFu;
    uint32_t coalescing_factor_ = 1;
};

}  // namespace tlm

#endif  // TLM_GPU_WAVEFRONT_TLM_HH