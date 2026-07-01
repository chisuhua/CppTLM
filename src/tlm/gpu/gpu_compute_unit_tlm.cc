// src/tlm/gpu/gpu_compute_unit_tlm.cc
// GpuComputeUnitTLM 实现
// 作者 CppTLM Team / 日期 2026-06-30
#include "tlm/gpu/gpu_compute_unit_tlm.hh"

namespace tlm {

    GpuComputeUnitTLM::GpuComputeUnitTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq), subcores_(4),
          scheduler_(std::make_unique<MinimalWarpSchedulerTLM>(name + "_sched", eq)) {
    }

    void GpuComputeUnitTLM::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
        adapter_ = adapter;
        if (scheduler_) {
            scheduler_->set_stream_adapter(adapter);
        }
    }

    void GpuComputeUnitTLM::set_num_subcores(uint32_t n) {
        subcores_.resize(n);
    }

    void GpuComputeUnitTLM::set_execution_latency(uint32_t cyc) {
        execution_latency_ = std::max(1u, cyc);
    }

    void GpuComputeUnitTLM::dispatch_wavefront(WavefrontTLM* wf) {
        if (!wf)
            return;
        scheduler_->add_warp(wf->get_warp_id());
        ++warps_dispatched_;
    }

    void GpuComputeUnitTLM::try_issue() {
        for (auto& slot : subcores_) {
            if (slot.busy)
                continue;
            auto next_warp = scheduler_->schedule_next();
            if (!next_warp.has_value())
                return;

            uint32_t warp_id = next_warp.value();
            slot.occupy(warp_id, execution_latency_);
            scheduler_->update_state(warp_id, true, execution_latency_);
        }
    }

    void GpuComputeUnitTLM::tick() {
        // 1. 推进所有 sub-core 执行
        for (auto& slot : subcores_) {
            if (slot.busy) {
                slot.tick();
                if (!slot.busy) {
                    scheduler_->update_state(slot.warp_id, false, 0);
                    scheduler_->remove_warp(slot.warp_id);
                    ++requests_completed_;
                    slot.release();
                }
            }
        }

        // 2. 派发新 warp 到空闲 slot
        try_issue();

        // 3. 推进 scheduler 计数器
        scheduler_->tick();

        // 4. Adapter tick
        if (adapter_)
            adapter_->tick();
    }

} // namespace tlm
