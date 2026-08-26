// tmu_handler_mvp.hh
// TMU Handler 纯接口头 - s3 实现具体 handler (S3SubmitQueueHandler)
// Author: CppTLM Team
// Date: 2026-08-26
//
// s3 W6 (per Oracle M4):
//   - TmuHandlerInterface::on_dispatch() 扩展从 void 到 TmuHandlerResult
//   - handler 内部调 SubmitQueue.enqueue()
//   - 返回 SQ_REJECTED 让 TMU 上报 SUBMIT_QUEUE_REJECTED 给 CP
#ifndef CPPTLM_TMU_HANDLER_MVP_H
#define CPPTLM_TMU_HANDLER_MVP_H

#include "tlm/gpu/tmu_types_mvp.hh"

namespace tlm::gpu {

    // 纯虚接口 — s3 提供具体 handler
    class TmuHandlerInterface {
    public:
        virtual ~TmuHandlerInterface() = default;

        // 分发 record 到下游 (s3: SubmitQueue.enqueue)
        // 返回 HANDLED 成功, SQ_REJECTED 下游满, INVALID_RECORD 字段非法
        virtual TmuHandlerResult on_dispatch(const TmuDispatchRecord& record) = 0;
    };

} // namespace tlm::gpu

#endif // CPPTLM_TMU_HANDLER_MVP_H
