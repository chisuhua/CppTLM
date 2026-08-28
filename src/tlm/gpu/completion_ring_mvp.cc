// completion_ring_mvp.cc
// CompletionRing MVP 实现: push + host_notify 钩子 (exactly-once)
// Author: CppTLM Team
// Date: 2026-08-26

#include "tlm/gpu/completion_ring_mvp.hh"

namespace tlm::gpu {

    CompletionRing::CompletionRing() = default;

    CompletionRing::CompletionRing(size_t capacity)
        : capacity_(capacity == 0 ? DEFAULT_CAPACITY : capacity) {
    }

    CompletionRing::~CompletionRing() = default;

    bool CompletionRing::on_warp_complete(uint32_t task_id, int32_t status) {
        if (ring_.size() >= capacity_) {
            return false; // 满则拒绝, 不覆盖旧 entry (避免丢失语义歧义)
        }
        ring_.push_back(Entry{task_id, status});
        return true;
    }

    void CompletionRing::tick() {
        // 每周期消费一条: pop 与 notify 成对发生, 保证 exactly-once
        if (ring_.empty()) {
            return;
        }
        const Entry e = ring_.front();
        ring_.pop_front();
        if (host_notify_) {
            host_notify_(e.task_id, e.status);
        }
    }

    void CompletionRing::set_host_notify(HostNotifyFn fn) {
        host_notify_ = std::move(fn);
    }

    size_t CompletionRing::inflight_count() const {
        return ring_.size();
    }

    size_t CompletionRing::capacity() const {
        return capacity_;
    }

} // namespace tlm::gpu
