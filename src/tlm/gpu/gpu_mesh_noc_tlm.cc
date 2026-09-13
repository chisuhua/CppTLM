// src/tlm/gpu/gpu_mesh_noc_tlm.cc
// GpuMeshNoC 实现 (Phase 8.A Task 3 + Stage 1.3b D2D payload 扩展)
// 作者 CppTLM Team / 日期 2026-06-24 (扩展 2026-09-13)
#include "tlm/gpu/gpu_mesh_noc_tlm.hh"

#include <algorithm>
#include <cstring>

namespace tlm {

    uint32_t GpuMeshNoC::route_latency(std::pair<uint32_t, uint32_t> src,
                                       std::pair<uint32_t, uint32_t> dst) const {
        uint32_t dx = (src.first > dst.first) ? (src.first - dst.first) : (dst.first - src.first);
        uint32_t dy =
            (src.second > dst.second) ? (src.second - dst.second) : (dst.second - src.second);
        return (dx + dy) * hops_latency_;
    }

    // Stage 1.3b D2D payload forwarding (per openspec/.../2026-09-10-cpptlm-stage-1-3-sdma §1.3b):
    //   - VRAM→VRAM memcpy (无 PCIe TLP, bypass host_out)
    //   - 延迟 = route_latency({0,0}, {dim-1, dim-1}) = 2*(dim-1)*hops_latency
    //   - 累加 payload_bytes_forwarded_ + last_transfer_latency_cycles_
    //
    // 注: 不实际模拟 mesh flit 调度 (deterministic analytical model, per Oracle O9).
    uint32_t GpuMeshNoC::d2d_forward_payload(void* dst_vram, const void* src_vram, uint64_t len) {
        if (dst_vram == nullptr || src_vram == nullptr || len == 0) {
            return 0;  // noop
        }
        // 简化 memcpy (实际 GPU 硬件有 flit 切分, 但 MVP deterministic 即可)
        std::memcpy(dst_vram, src_vram, static_cast<size_t>(len));

        // 延迟计算: 最坏路径 (从 {0,0} 到 {dim-1, dim-1})
        const uint32_t latency =
            route_latency({0, 0}, {dim_ > 0 ? dim_ - 1 : 0, dim_ > 0 ? dim_ - 1 : 0});

        payload_bytes_forwarded_ += len;
        last_transfer_latency_cycles_ = latency;
        return latency;
    }

    // Oracle O9 测量定义:
    //   throughput = payload_bytes / (last_latency_cycles × cycle_period_s)
    //   单位: bytes/s; 转换为 GB/s (×1e-9)
    double GpuMeshNoC::simulated_throughput_GBps(double cycle_period_s) const {
        if (last_transfer_latency_cycles_ == 0 || cycle_period_s <= 0.0) {
            return 0.0;
        }
        const double seconds = static_cast<double>(last_transfer_latency_cycles_) * cycle_period_s;
        const double bytes_per_sec = static_cast<double>(payload_bytes_forwarded_) / seconds;
        return bytes_per_sec / 1e9;
    }

    void GpuMeshNoC::tick() {
        cycle_counter_++;
    }

} // namespace tlm