// include/tlm/gpu/minimal_warp_scheduler_tlm.hh
// MinimalWarpSchedulerTLM: round-robin warp 调度器
// 接口名与 PTX-EMU WarpScheduler 对齐，便于 F12b-LD 替换
// 作者 CppTLM Team / 日期 2026-06-30
#ifndef TLM_GPU_MINIMAL_WARP_SCHEDULER_TLM_HH
#define TLM_GPU_MINIMAL_WARP_SCHEDULER_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tlm {

// Task 10 (per architecture/15 §15.7.2 + plan): MinimalWarpSchedulerTLM 内化到 sm::IssueUnitTLM.
// 保留旧类, 标记 deprecated, chstream_register 注销. Task 16 删除文件.
class [[deprecated("use tlm::sm::IssueUnitTLM")]] MinimalWarpSchedulerTLM : public ChStreamModuleBase {
public:
    explicit MinimalWarpSchedulerTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {
    }
    ~MinimalWarpSchedulerTLM() override = default;

    std::string get_module_type() const override {
        return "MinimalWarpSchedulerTLM";
    }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    // PTX-EMU WarpScheduler-compatible interface names
    void add_warp(uint32_t warp_id);
    void remove_warp(uint32_t warp_id);
    std::optional<uint32_t> schedule_next();
    bool all_warps_finished() const;
    void update_state(uint32_t warp_id, bool blocked, uint32_t blocked_cycles);

    void tick() override;

private:
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    struct WarpState {
        bool blocked = false;
        uint32_t blocked_cycles_remaining = 0;
    };

    std::unordered_map<uint32_t, WarpState> warps_;
    std::vector<uint32_t> order_;
    size_t next_idx_ = 0;
};

} // namespace tlm

#endif // TLM_GPU_MINIMAL_WARP_SCHEDULER_TLM_HH