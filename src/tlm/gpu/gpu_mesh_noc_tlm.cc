// src/tlm/gpu/gpu_mesh_noc_tlm.cc
// GpuMeshNoC 实现 (Phase 8.A Task 3)
// 作者 CppTLM Team / 日期 2026-06-24
#include "tlm/gpu/gpu_mesh_noc_tlm.hh"

namespace tlm {

uint32_t GpuMeshNoC::route_latency(std::pair<uint32_t, uint32_t> src,
                                   std::pair<uint32_t, uint32_t> dst) const {
    uint32_t dx = (src.first > dst.first) ? (src.first - dst.first)
                                          : (dst.first - src.first);
    uint32_t dy = (src.second > dst.second) ? (src.second - dst.second)
                                            : (dst.second - src.second);
    return (dx + dy) * hops_latency_;
}

void GpuMeshNoC::tick() {
    cycle_counter_++;
}

}  // namespace tlm