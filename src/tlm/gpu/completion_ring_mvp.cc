// completion_ring_mvp.cc
// CompletionRingTLM 实现 - ChStream 组件 (CQ ring + 4 端口)
// Per board-soc-split design §3.5 + Phase F-H.4
// Author: CppTLM Team
// Date: 2026-08-31

#include "tlm/gpu/completion_ring_mvp.hh"

namespace tlm::gpu {

    CompletionRingTLM::CompletionRingTLM(const std::string& n, EventQueue* eq)
        : ChStreamModuleBase(n, eq) {
    }

    void CompletionRingTLM::push(const bundles::CompletionEntry& entry) {
        if (ring_.size() >= kRingSize) {
            return; // 满 → 丢包 (防御性)
        }
        ring_.push_back(entry);
        // TODO: 触发 done_out 转发 + irq_out (MSI-X delivery)
    }

    bool CompletionRingTLM::pop(bundles::CompletionEntry* out) {
        if (ring_.empty())
            return false;
        *out = ring_.front();
        ring_.pop_front();
        return true;
    }

    size_t CompletionRingTLM::pending_count() const {
        return ring_.size();
    }

    // 兼容旧 API (s2 测试使用)
    bool CompletionRingTLM::on_warp_complete(uint32_t task_id, int32_t status) {
        if (ring_.size() >= capacity_) {
            return false; // 满则拒绝, 不覆盖旧 entry (避免丢失语义歧义)
        }
        bundles::CompletionEntry entry(task_id, status, 0);
        ring_.push_back(entry);
        return true;
    }

    void CompletionRingTLM::tick() {
        // 每周期消费一条: pop 与 notify 成对发生, 保证 exactly-once
        if (ring_.empty()) {
            return;
        }
        const auto e = ring_.front();
        ring_.pop_front();
        if (host_notify_) {
            host_notify_(e.task_id, static_cast<int32_t>(e.status.read()));
        }
    }

    void CompletionRingTLM::set_host_notify(HostNotifyFn fn) {
        host_notify_ = std::move(fn);
    }

    size_t CompletionRingTLM::inflight_count() const {
        return ring_.size();
    }

    size_t CompletionRingTLM::capacity() const {
        return capacity_;
    }

} // namespace tlm::gpu