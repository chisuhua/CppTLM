// tmu_dispatch_processor_mvp.hh
// TMU Dispatch Processor 骨架 - 反压停 fetch (per Phase F-D.2 H5)
// Author: CppTLM Team
// Date: 2026-08-26
//
// s2 阶段: 骨架编译通过即可
// s3 阶段: 填充 dep chain + 反压退避 + 派发路径经 handler (S3SubmitQueueHandler)
#ifndef CPPTLM_TMU_DISPATCH_PROCESSOR_MVP_H
#define CPPTLM_TMU_DISPATCH_PROCESSOR_MVP_H

#include "tlm/gpu/tmu_types_mvp.hh"
#include "tlm/gpu/tmu_handler_mvp.hh"
#include <memory>

namespace tlm::gpu {

    class TmuDispatchProcessor {
    public:
        static constexpr size_t MAX_ACTIVE_TASKS = 32;  // per Phase F-D.2 H5

        TmuDispatchProcessor();
        ~TmuDispatchProcessor();

        TmuDispatchProcessor(const TmuDispatchProcessor&) = delete;
        TmuDispatchProcessor& operator=(const TmuDispatchProcessor&) = delete;

        // 提交 dispatch record
        // @param out_evicted [out] (s3 will fill: evicted task_id if LIFO eviction triggered)
        // @return SUBMITTED / BACKPRESSURED / DEP_LATCH_MISMATCH / SUBMIT_QUEUE_REJECTED / INTERNAL_ERROR
        TmuSubmitResult submit(TmuDispatchRecord record, uint32_t* out_evicted = nullptr);

        // 反向流: warp 完成时调
        void on_complete(uint32_t task_id, int32_t status);

        // 链式推进: 扫描注册任务, 找到等待当前 latch 的任务
        // s2 skeleton: no-op
        void try_chain_dependent(const TmuDispatchRecord& completed_record);

        // 状态查询
        size_t inflight_count() const { return inflight_count_; }

        // 计数器 (诊断/测试)
        uint64_t backpressure_count() const { return backpressure_count_; }
        uint64_t submitted_count() const { return submitted_count_; }
        uint64_t completed_count() const { return completed_count_; }
        uint64_t dep_latch_mismatch_count() const { return dep_latch_mismatch_count_; }
        uint64_t sq_rejected_count() const { return sq_rejected_count_; }

        // s3 注入 handler
        void set_handler(std::unique_ptr<TmuHandlerInterface> handler);

    private:
        std::unique_ptr<TmuHandlerInterface> handler_;
        size_t inflight_count_ = 0;

        uint64_t backpressure_count_ = 0;
        uint64_t submitted_count_ = 0;
        uint64_t completed_count_ = 0;
        uint64_t dep_latch_mismatch_count_ = 0;
        uint64_t sq_rejected_count_ = 0;
    };

} // namespace tlm::gpu

#endif // CPPTLM_TMU_DISPATCH_PROCESSOR_MVP_H
