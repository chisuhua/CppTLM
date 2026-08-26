// tmu_dispatch_processor_mvp.cc
// TMU Dispatch Processor 骨架实现 - 反压停 fetch
// Author: CppTLM Team
// Date: 2026-08-26

#include "tlm/gpu/tmu_dispatch_processor_mvp.hh"

namespace tlm::gpu {

    TmuDispatchProcessor::TmuDispatchProcessor() = default;
    TmuDispatchProcessor::~TmuDispatchProcessor() = default;

    TmuSubmitResult TmuDispatchProcessor::submit(TmuDispatchRecord record, uint32_t* /*out_evicted*/) {
        // ① dep latch 检查 (s2 skeleton: 简化 — 若 dep_enable=true 直接拒收)
        // s3 will implement check_dep_latches(record) via wait_on_latch_id ↔ arrive_at_latch_id matching
        if (record.dep_enable) {
            dep_latch_mismatch_count_++;
            return TmuSubmitResult::DEP_LATCH_MISMATCH;
        }

        // ② 容量检查 (反压 — per Phase F-D.2 H5: 不驱逐,通知 CP 退避)
        if (inflight_count_ >= MAX_ACTIVE_TASKS) {
            backpressure_count_++;
            return TmuSubmitResult::BACKPRESSURED;
        }

        // ③ handler 派发 (s2 skeleton: 仅在 handler 注入时调用)
        // s3 S3SubmitQueueHandler will call sq_.enqueue() and return HANDLED/SQ_REJECTED
        if (handler_) {
            TmuHandlerResult hresult = handler_->on_dispatch(record);
            if (hresult == TmuHandlerResult::SQ_REJECTED) {
                sq_rejected_count_++;
                return TmuSubmitResult::SUBMIT_QUEUE_REJECTED;
            }
            if (hresult != TmuHandlerResult::HANDLED) {
                return TmuSubmitResult::INTERNAL_ERROR;
            }
        }

        // ④ 注册到 scheduler cache (skeleton: 仅增加计数)
        // s3 will store record in scheduler_cache_[record.task_id] for dep chain tracking
        inflight_count_++;
        submitted_count_++;
        return TmuSubmitResult::SUBMITTED;
    }

    void TmuDispatchProcessor::on_complete(uint32_t /*task_id*/, int32_t /*status*/) {
        // s2 skeleton: 仅减计数
        if (inflight_count_ > 0) {
            inflight_count_--;
            completed_count_++;
        }
        // s3 will remove from scheduler_cache_ + trigger try_chain_dependent
    }

    void TmuDispatchProcessor::try_chain_dependent(const TmuDispatchRecord& /*completed_record*/) {
        // s2 skeleton: no-op
        // s3 will scan scheduler_cache_ for tasks with wait_on_latch_id == completed_record.arrive_at_latch_id
    }

    void TmuDispatchProcessor::set_handler(std::unique_ptr<TmuHandlerInterface> handler) {
        handler_ = std::move(handler);
    }

} // namespace tlm::gpu
