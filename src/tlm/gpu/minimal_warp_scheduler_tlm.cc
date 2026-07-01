// src/tlm/gpu/minimal_warp_scheduler_tlm.cc
// MinimalWarpSchedulerTLM 实现
// 作者 CppTLM Team / 日期 2026-06-30
#include "tlm/gpu/minimal_warp_scheduler_tlm.hh"

#include <algorithm>
#include <iterator>

namespace tlm {

    void MinimalWarpSchedulerTLM::add_warp(uint32_t warp_id) {
        if (warps_.find(warp_id) == warps_.end()) {
            warps_[warp_id] = WarpState{};
            order_.push_back(warp_id);
        }
    }

    void MinimalWarpSchedulerTLM::remove_warp(uint32_t warp_id) {
        warps_.erase(warp_id);
        auto it = std::find(order_.begin(), order_.end(), warp_id);
        if (it != order_.end()) {
            size_t removed_idx = std::distance(order_.begin(), it);
            order_.erase(it);
            if (next_idx_ > removed_idx && next_idx_ > 0) {
                --next_idx_;
            }
            if (next_idx_ >= order_.size()) {
                next_idx_ = 0;
            }
        }
    }

    std::optional<uint32_t> MinimalWarpSchedulerTLM::schedule_next() {
        if (order_.empty()) {
            return std::nullopt;
        }

        size_t start_idx = next_idx_;
        do {
            uint32_t warp_id = order_[next_idx_];
            next_idx_ = (next_idx_ + 1) % order_.size();
            if (!warps_[warp_id].blocked) {
                return warp_id;
            }
        } while (next_idx_ != start_idx);

        return std::nullopt;
    }

    bool MinimalWarpSchedulerTLM::all_warps_finished() const {
        return order_.empty();
    }

    void MinimalWarpSchedulerTLM::update_state(uint32_t warp_id, bool blocked,
                                               uint32_t blocked_cycles) {
        auto it = warps_.find(warp_id);
        if (it != warps_.end()) {
            it->second.blocked = blocked;
            it->second.blocked_cycles_remaining = blocked_cycles;
        }
    }

    void MinimalWarpSchedulerTLM::tick() {
        for (auto& kv : warps_) {
            if (kv.second.blocked && kv.second.blocked_cycles_remaining > 0) {
                --kv.second.blocked_cycles_remaining;
                if (kv.second.blocked_cycles_remaining == 0) {
                    kv.second.blocked = false;
                }
            }
        }
    }

} // namespace tlm