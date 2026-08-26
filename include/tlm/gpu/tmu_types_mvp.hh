// tmu_types_mvp.hh
// TMU (Texture/Topology Management Unit) Dispatch 数据类型
// Author: CppTLM Team
// Date: 2026-08-26
//
// 依据 Oracle M4 + Phase F-D.2 H5:
// - TmuDispatchRecord: 9-field dispatch record (s3 will use full)
// - TmuSubmitResult: 5-value enum (s2 uses 3; s3 adds 2 for back-pressure path)
// - TmuHandlerResult: 3-value enum (s3 introduces for SQ rejection propagation)
#ifndef CPPTLM_TMU_TYPES_MVP_H
#define CPPTLM_TMU_TYPES_MVP_H

#include <cstdint>

namespace tlm::gpu {

    // TMU dispatch record (9-field, per s3 T-s3-3)
    struct TmuDispatchRecord {
        uint32_t task_id = 0;
        uint64_t vram_image_addr = 0;
        uint32_t grid_x = 0;
        uint32_t grid_y = 0;
        uint32_t grid_z = 0;
        uint32_t block_x = 0;
        uint32_t block_y = 0;
        uint32_t block_z = 0;
        uint32_t shared_mem_bytes = 0;
        uint64_t args_vram_addr = 0;
        // Dep latch fields (per Phase F-D.2 H5)
        bool dep_enable = false;
        uint32_t wait_on_latch_id = 0;
        uint32_t arrive_at_latch_id = 0;
    };

    // TMU submit 结果 (per Oracle M4)
    enum class TmuSubmitResult {
        SUBMITTED,             // 接受
        BACKPRESSURED,         // TMU 容量满 (32), CP 退避
        DEP_LATCH_MISMATCH,    // dep latch 不满足
        SUBMIT_QUEUE_REJECTED, // downstream SQ 满 (handler 上报)
        INTERNAL_ERROR         // 防御性
    };

    // Handler dispatch 结果 (per Oracle M4)
    enum class TmuHandlerResult {
        HANDLED,         // 下游 SQ.enqueue 成功
        SQ_REJECTED,     // SQ 满
        INVALID_RECORD   // record 字段非法 (防御性)
    };

} // namespace tlm::gpu

#endif // CPPTLM_TMU_TYPES_MVP_H
