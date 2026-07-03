// src/tlm/gpu/shared_memory_tlm.cc
// SharedMemoryTLM 实现 (Phase 8.A Task 1)
// 作者 CppTLM Team / 日期 2026-06-24
#include "tlm/gpu/shared_memory_tlm.hh"

namespace tlm {

    uint32_t SharedMemoryTLM::bank_conflict_cycles(uint32_t num_threads,
                                                   uint32_t stride_bytes) const {
        // Phase 8.A 简化模型 (per design.md §3.1, ADR-NV-01 D2 决策):
        //   base 1 cycle + 每个 conflict way +1 cycle
        // 不模拟真实 SM 内部 cache hierarchy 或 warp scheduler 影响
        (void)stride_bytes; // 当前模型未使用 stride
        if (num_threads <= 1)
            return 1;
        return 1 + (num_threads - 1);
    }

    void SharedMemoryTLM::tick() {
        // Phase 8.A stub: bank conflict 在请求发生时计算 (非 tick 推进)
        // 真实 shared memory 访问模型在 Phase 9+ (per ADR-NV-01 §10)
    }

} // namespace tlm