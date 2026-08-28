// tmu_types_mvp.hh
// TMU (Texture/Topology Management Unit) Dispatch 数据类型
// Author: CppTLM Team
// Date: 2026-08-26 (s3 commit 2026-08-28 补 PreExitPolicy + 字段数描述修正)
//
// 依据 Oracle M4 + Phase F-D.2 H5:
// - TmuDispatchRecord: 13-field dispatch record (10 base + 3 dep latch)
// - TmuSubmitResult: 5-value enum (s2 uses 3; s3 adds 2 for back-pressure path)
// - TmuHandlerResult: 3-value enum (s3 introduces for SQ rejection propagation)
// - PreExitPolicy: s3 补漏定义 (per Oracle P2-1)
#ifndef CPPTLM_TMU_TYPES_MVP_H
#define CPPTLM_TMU_TYPES_MVP_H

#include <cstdint>

namespace tlm::gpu {

    // TMU dispatch record (13-field: 10 base + 3 dep latch, per s3 T-s3-3)
    struct TmuDispatchRecord {
        // 10 基础字段
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
        // 3 Dep latch 字段 (per Phase F-D.2 H5)
        bool dep_enable = false;
        uint32_t wait_on_latch_id = 0;
        uint32_t arrive_at_latch_id = 0;
    };
    static_assert(sizeof(TmuDispatchRecord) > 0, "TmuDispatchRecord sanity check");

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

    // Pre-exit policy (per design.md §4 + Oracle P2-1 修复 2026-08-28)
    // s2 应有但漏了,s3 commit T-s3-3 补漏定义
    // MVP 仅 NONE 档(无 pre-exit 优化);未来可扩展 PRIORITY/EVICT_OLDEST
    enum class PreExitPolicy {
        NONE,   // MVP 唯一档:不退,不驱逐
        // FUTURE: PRIORITY, EVICT_OLDEST, LRU, ...
    };

} // namespace tlm::gpu

#endif // CPPTLM_TMU_TYPES_MVP_H
