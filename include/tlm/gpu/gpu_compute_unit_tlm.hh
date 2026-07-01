// include/tlm/gpu/gpu_compute_unit_tlm.hh
// GpuComputeUnitTLM: SM 抽象，含 4 个 SubCoreSlot + MinimalWarpSchedulerTLM
// 作者 CppTLM Team / 日期 2026-06-30
#ifndef TLM_GPU_GPU_COMPUTE_UNIT_TLM_HH
#define TLM_GPU_GPU_COMPUTE_UNIT_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include "tlm/gpu/sub_core_slot.hh"
#include "tlm/gpu/wavefront_tlm.hh"
#include "tlm/gpu/minimal_warp_scheduler_tlm.hh"
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace tlm {

class GpuComputeUnitTLM : public ChStreamModuleBase {
public:
    explicit GpuComputeUnitTLM(const std::string& name, EventQueue* eq);
    ~GpuComputeUnitTLM() override = default;

    std::string get_module_type() const override { return "GpuComputeUnitTLM"; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;

    void set_num_subcores(uint32_t n);
    void set_execution_latency(uint32_t cyc);

    uint32_t get_num_subcores() const { return static_cast<uint32_t>(subcores_.size()); }
    uint32_t get_execution_latency() const { return execution_latency_; }
    uint64_t get_requests_completed() const { return requests_completed_; }
    uint64_t get_warps_dispatched() const { return warps_dispatched_; }

    void dispatch_wavefront(WavefrontTLM* wf);

    void tick() override;

    MinimalWarpSchedulerTLM* scheduler() { return scheduler_.get(); }

private:
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    std::vector<SubCoreSlot> subcores_;
    std::unique_ptr<MinimalWarpSchedulerTLM> scheduler_;
    uint32_t execution_latency_ = 1;
    uint64_t requests_completed_ = 0;
    uint64_t warps_dispatched_ = 0;

    void try_issue();
};

}  // namespace tlm

#endif  // TLM_GPU_GPU_COMPUTE_UNIT_TLM_HH
