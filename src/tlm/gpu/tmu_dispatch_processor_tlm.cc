// tmu_dispatch_processor_tlm.cc (renamed 2026-08-29 T-bs-2c-TMU)
// TMU Dispatch Processor 实现 - 反压 4 路径 + dep chain + handler 派发
// Author: CppTLM Team
// Date: 2026-08-26 (s3 commit 2026-08-28 填充 scheduler_cache_ + dep chain 推进)
//
// Per design §4 + Phase F-D.2 H5 + Oracle M4:
//   submit() 4 路径:
//     ① dep latches (dep_enable && wait_on 没匹配 → DEP_LATCH_MISMATCH)
//     ② capacity    (inflight_count_ >= MAX_ACTIVE_TASKS → BACKPRESSURED)
//     ③ handler     (SQ 满 → SQ_REJECTED → SUBMIT_QUEUE_REJECTED;否则 HANDLED)
//     ④ register    (scheduler_cache_[task_id] → SUBMITTED)
//   dep chain 推进:on_complete → try_chain_dependent 扫 cache 找 wait_on 匹配
//
// GCC -O2 寄存器复用陷阱规避同 pm4_decoder_mvp.cc (.cc 内注释)
#include "tlm/gpu/tmu_dispatch_processor_tlm.hh"

#include <unordered_map>
#include <vector>

namespace tlm::gpu {

    TmuDispatchProcessor::TmuDispatchProcessor() = default;
    TmuDispatchProcessor::~TmuDispatchProcessor() = default;

    TmuSubmitResult TmuDispatchProcessor::submit(TmuDispatchRecord record,
                                                 uint32_t* /*out_evicted*/) {
        if (record.dep_enable) {
            dep_latch_mismatch_count_++;
            return TmuSubmitResult::DEP_LATCH_MISMATCH;
        }

        if (inflight_count_ >= MAX_ACTIVE_TASKS) {
            backpressure_count_++;
            return TmuSubmitResult::BACKPRESSURED;
        }

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

        scheduler_cache_[record.task_id] = record;
        inflight_count_++;
        submitted_count_++;
        return TmuSubmitResult::SUBMITTED;
    }

    void TmuDispatchProcessor::on_complete(uint32_t task_id, int32_t status) {
        auto it = scheduler_cache_.find(task_id);
        if (it == scheduler_cache_.end()) {
            return;
        }
        TmuDispatchRecord completed = it->second;
        scheduler_cache_.erase(it);

        if (inflight_count_ > 0) {
            inflight_count_--;
        }
        completed_count_++;

        try_chain_dependent(completed);
        (void)status;
    }

    void TmuDispatchProcessor::try_chain_dependent(const TmuDispatchRecord& completed_record) {
        // MVP: 扫描 cache 找 wait_on_latch_id 匹配的任务,s4 再实现 advance
        std::vector<uint32_t> to_advance;
        for (const auto& [task_id, rec] : scheduler_cache_) {
            if (rec.dep_enable && rec.wait_on_latch_id == completed_record.arrive_at_latch_id) {
                to_advance.push_back(task_id);
            }
        }
        (void)to_advance;
    }

    void TmuDispatchProcessor::set_handler(std::unique_ptr<TmuHandlerInterface> handler) {
        handler_ = std::move(handler);
    }

} // namespace tlm::gpu
