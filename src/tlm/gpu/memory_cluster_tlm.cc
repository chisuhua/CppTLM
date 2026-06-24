// src/tlm/gpu/memory_cluster_tlm.cc
// MemoryClusterTLM 实现 (Phase 8.A Task 2)
// 作者 CppTLM Team / 日期 2026-06-24
#include "tlm/gpu/memory_cluster_tlm.hh"

namespace tlm {

uint32_t MemoryClusterTLM::allocate_channel(uint64_t request_id) {
    (void)request_id;
    uint32_t ch = static_cast<uint32_t>(rr_counter_ % channels_);
    rr_counter_++;
    return ch;
}

void MemoryClusterTLM::tick() {
    requests_completed_ += rr_counter_;
}

}  // namespace tlm