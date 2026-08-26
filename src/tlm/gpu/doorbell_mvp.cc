// doorbell_mvp.cc
// Doorbell MVP 实现: SQ tail register 强序写 + 250-700ns 延迟模拟
// Author: CppTLM Team
// Date: 2026-08-26

#include "tlm/gpu/doorbell_mvp.hh"

namespace tlm::gpu {

    Doorbell::Doorbell() = default;

    Doorbell::~Doorbell() = default;

    void Doorbell::init(uint64_t cycle_ns) {
        if (cycle_ns == 0) {
            cycle_ns = 1; // 防止除零, 退化为 1ns/cycle
        }
        cycle_ns_ = cycle_ns;
        now_ = 0;
        streams_.clear();
    }

    uint64_t Doorbell::latency_ns(uint32_t stream_id, uint64_t wdu_offset) const {
        // Knuth 乘法散列混合 stream_id 与 offset, 取模映射到 [MIN, MAX]
        const uint64_t range = MAX_LATENCY_NS - MIN_LATENCY_NS + 1; // 451
        uint64_t mix = wdu_offset ^ (static_cast<uint64_t>(stream_id) * 2654435761ULL);
        mix *= 2654435761ULL;
        return MIN_LATENCY_NS + (mix % range);
    }

    bool Doorbell::ring(uint32_t stream_id, uint64_t wdu_offset) {
        StreamState& st = streams_[stream_id];
        if (st.pending.size() >= MAX_PENDING_PER_STREAM) {
            return false;
        }

        const uint64_t lat_cycles =
            (latency_ns(stream_id, wdu_offset) + cycle_ns_ - 1) / cycle_ns_;
        // 强序: 同 stream 后到的写不得先于先到的写完成。
        // 完成周期取 max(issue+latency, 前一笔完成周期), 保证 FIFO 完成顺序。
        uint64_t complete_cycle = now_ + lat_cycles;
        if (!st.pending.empty() && st.pending.back().complete_cycle > complete_cycle) {
            complete_cycle = st.pending.back().complete_cycle;
        }
        st.pending.push_back(PendingWrite{wdu_offset, complete_cycle});
        return true;
    }

    void Doorbell::tick() {
        now_ += 1;
        for (auto& kv : streams_) {
            StreamState& st = kv.second;
            // FIFO 弹出所有到期写, 尾指针按完成顺序推进(强序可见)
            while (!st.pending.empty() && st.pending.front().complete_cycle <= now_) {
                st.visible_tail = st.pending.front().wdu_offset;
                st.pending.pop_front();
            }
        }
    }

    bool Doorbell::is_pending(uint32_t stream_id) const {
        const auto it = streams_.find(stream_id);
        return it != streams_.end() && !it->second.pending.empty();
    }

    uint64_t Doorbell::sq_tail(uint32_t stream_id) const {
        const auto it = streams_.find(stream_id);
        return it == streams_.end() ? 0 : it->second.visible_tail;
    }

    uint64_t Doorbell::now_cycles() const {
        return now_;
    }

} // namespace tlm::gpu
